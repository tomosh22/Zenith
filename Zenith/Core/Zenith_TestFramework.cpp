#include "Zenith.h"
#include "Zenith_TestFramework.h"

#ifdef ZENITH_TESTING

#include "Collections/Zenith_Vector.h"
#include "Core/Zenith_CommandLine.h"
#include "Core/Zenith_PlatformStdio.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <cmath>

// The runner is Core L0 and deliberately names no subsystem — not even the
// profiler — so it carries its own two-line timebase rather than reaching for
// Zenith_Profiling_Detail. Same clock underneath.
static u_int64 Zenith_TestClockTicks()
{
	return static_cast<u_int64>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
}
static double Zenith_TestTicksToMs(const u_int64 uBegin, const u_int64 uEnd)
{
	using Period = std::chrono::high_resolution_clock::period;
	const double fTicksToNs = static_cast<double>(Period::num) * 1.0e9 / static_cast<double>(Period::den);
	return static_cast<double>(uEnd - uBegin) * fTicksToNs / 1.0e6;
}

#ifdef ZENITH_WINDOWS
// __try / __except / EXCEPTION_EXECUTE_HANDLER. Per the W5.2 note in Zenith.h,
// <Windows.h> is no longer in the PCH; this TU used to get it transitively via
// vulkan.hpp, but the Null / D3D12 backends pull no Vulkan, so include it
// explicitly. Zenith_Win32.h carries the APIENTRY + LEAN_AND_MEAN guards.
#include "Core/Zenith_Win32.h"
#endif

Zenith_TestRunner& Zenith_TestRunner::Instance()
{
	static Zenith_TestRunner s_xInstance;
	return s_xInstance;
}

void Zenith_TestRunner::RegisterTest(Zenith_TestCase* pxCase)
{
	pxCase->m_pxNext = m_pxFirstTest;
	m_pxFirstTest = pxCase;
	m_uTestCount++;
}

static void Zenith_TestResetGlobalState()
{
	// The concrete reset (ECS scene-system) is installed from a higher layer via
	// Zenith_TestRunner::SetResetHook; the runner itself names no ECS type. Null
	// hook => nothing to reset (safe no-op).
	void (*pfnReset)() = Zenith_TestRunner::GetResetHook();
	if (pfnReset == nullptr)
	{
		return;
	}

#ifdef ZENITH_WINDOWS
	__try { pfnReset(); }
	__except (EXCEPTION_EXECUTE_HANDLER) {}
#else
	pfnReset();
#endif
}

void Zenith_TestRunner::RunAllTests()
{
	// Prime the global state so the first test starts from the same baseline
	// every subsequent test sees.
	Zenith_TestResetGlobalState();

	for (Zenith_TestCase* pxCase = m_pxFirstTest; pxCase != nullptr; pxCase = pxCase->m_pxNext)
	{
		m_pxCurrentCase       = pxCase;
		m_bCurrentTestFailed  = false;
		m_bCurrentTestSkipped = false;

		const u_int64 uBodyBegin = Zenith_TestClockTicks();
		pxCase->m_pfnTest();
		const u_int64 uBodyEnd = Zenith_TestClockTicks();

		// A later environment skip must never downgrade an assertion already
		// recorded by the same test. Several round-trip and bake tests perform
		// useful checks before discovering that an optional resource is absent.
		if (m_bCurrentTestFailed)       m_uFailedCount++;
		else if (m_bCurrentTestSkipped) m_uSkippedCount++;
		else                            m_uPassedCount++;

		m_pxCurrentCase = nullptr;

		// Clear any capture state a crashed test may have left on.
		g_bAssertCaptureActive.store(false, std::memory_order_release);
		g_uAssertCaptureHitCount.store(0, std::memory_order_release);

		// Reset engine-wide state so the next test starts clean. Wrap in SEH
		// because a catastrophically broken test could corrupt the state we're
		// about to touch.
		Zenith_TestResetGlobalState();
		const u_int64 uResetEnd = Zenith_TestClockTicks();

		// Charged to the test that CAUSED it, but reported in its own column: the
		// reset is work the runner does on the test's behalf, not work the test did.
		pxCase->m_fBodyMs  = Zenith_TestTicksToMs(uBodyBegin, uBodyEnd);
		pxCase->m_fResetMs = Zenith_TestTicksToMs(uBodyEnd, uResetEnd);
		m_fTotalBodyMs  += pxCase->m_fBodyMs;
		m_fTotalResetMs += pxCase->m_fResetMs;
	}

	// Final summary — the only unit-test logging the suite emits.
	Zenith_Log(LOG_CATEGORY_UNITTEST,
		"Unit tests complete: %u ran, %u passed, %u failed, %u skipped",
		m_uTestCount,
		m_uPassedCount,
		m_uFailedCount,
		m_uSkippedCount
		);

	// One machine-readable line ALWAYS (it is one line, and the boot report's
	// `UnitTests` phase is meaningless without knowing how it splits), then the
	// full per-test table only when it was asked for.
	Zenith_Log(LOG_CATEGORY_UNITTEST,
		"[UnitTest] TimingSummary totalMs=%.1f bodyMs=%.1f resetMs=%.1f tests=%u",
		m_fTotalBodyMs + m_fTotalResetMs, m_fTotalBodyMs, m_fTotalResetMs, m_uTestCount);

	const char* szTimingPath = Zenith_CommandLine::GetUnitTestTimingsPath();
	if (szTimingPath != nullptr)
	{
		WriteTimingReport(stdout);
		fflush(stdout);

		FILE* pxFile = Zenith_PlatformStdio::OpenFile(szTimingPath, "w");
		if (pxFile != nullptr)
		{
			WriteTimingReport(pxFile);
			fclose(pxFile);
			Zenith_Log(LOG_CATEGORY_UNITTEST, "Unit-test timings -> %s", szTimingPath);
		}
	}
	if (m_uSkippedCount > 0)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "  skipped: %u", m_uSkippedCount);
	}

	// Emit one block per captured failure.
	for (const Zenith_TestFailure* pxFail = m_pxFirstFailure; pxFail != nullptr; pxFail = pxFail->m_pxNext)
	{
		Zenith_Error(LOG_CATEGORY_UNITTEST,
			"FAILED  %s::%s\n    at %s:%d\n    %s",
			pxFail->m_strCategory,
			pxFail->m_strName,
			pxFail->m_strFile,
			pxFail->m_iLine,
			pxFail->m_acMsg);
	}

	if(m_uFailedCount)
		Zenith_DebugBreak();
}

void Zenith_TestRunner::WriteTimingReport(FILE* pFile) const
{
	if (pFile == nullptr) return;

	// Collect first, sort second: the registry is a singly-linked list in REVERSE
	// registration order, and the interesting order is by cost.
	Zenith_Vector<const Zenith_TestCase*> xCases;
	for (const Zenith_TestCase* pxCase = m_pxFirstTest; pxCase != nullptr; pxCase = pxCase->m_pxNext)
	{
		xCases.PushBack(pxCase);
	}
	std::sort(xCases.GetDataPointer(), xCases.GetDataPointer() + xCases.GetSize(),
		[](const Zenith_TestCase* pxA, const Zenith_TestCase* pxB)
		{ return (pxA->m_fBodyMs + pxA->m_fResetMs) > (pxB->m_fBodyMs + pxB->m_fResetMs); });

	const double fTotalMs = m_fTotalBodyMs + m_fTotalResetMs;

	fprintf(pFile, "\n=== Unit-test timings (every registered test, slowest first) ===\n");
	fprintf(pFile, "Tests: %u | Total: %.1f ms | Body: %.1f ms (%.1f%%) | Per-test reset: %.1f ms (%.1f%%)\n",
		m_uTestCount, fTotalMs,
		m_fTotalBodyMs,  fTotalMs > 0.0 ? (m_fTotalBodyMs  / fTotalMs) * 100.0 : 0.0,
		m_fTotalResetMs, fTotalMs > 0.0 ? (m_fTotalResetMs / fTotalMs) * 100.0 : 0.0);
	fprintf(pFile, "'reset' is the global-state reset the RUNNER performs after each test, not work the test did.\n\n");

	fprintf(pFile, "%6s  %-58s %11s %11s %11s %8s %8s\n",
		"rank", "Category::Name", "total (ms)", "body (ms)", "reset (ms)", "%", "cum %");
	fprintf(pFile, "------  ---------------------------------------------------------- ----------- ----------- ----------- -------- --------\n");

	double fCumulativeMs = 0.0;
	for (u_int u = 0; u < xCases.GetSize(); ++u)
	{
		const Zenith_TestCase* pxCase = xCases.Get(u);
		const double fCaseMs = pxCase->m_fBodyMs + pxCase->m_fResetMs;
		fCumulativeMs += fCaseMs;

		char acLabel[128];
		snprintf(acLabel, sizeof(acLabel), "%s::%s", pxCase->m_strCategory, pxCase->m_strName);

		fprintf(pFile, "%6u  %-58s %11.3f %11.3f %11.3f %7.2f%% %7.2f%%\n",
			u + 1, acLabel, fCaseMs, pxCase->m_fBodyMs, pxCase->m_fResetMs,
			fTotalMs > 0.0 ? (fCaseMs / fTotalMs) * 100.0 : 0.0,
			fTotalMs > 0.0 ? (fCumulativeMs / fTotalMs) * 100.0 : 0.0);
	}
	fprintf(pFile, "\n");
}

void Zenith_TestRunner::HandleFailure(const char* strType, const char* strMsg, const char* strFile, int iLine)
{
	// When a Zenith_AssertCaptureScope is active, the test is deliberately
	// triggering an assertion to verify the capture mechanism. Count the
	// hit and return silently so AssertCapture tests work with both
	// Zenith_Assert (engine) and ZENITH_ASSERT_* (framework).
	if (g_bAssertCaptureActive.load(std::memory_order_acquire))
	{
		g_uAssertCaptureHitCount.fetch_add(1, std::memory_order_acq_rel);
		return;
	}

	// Append to the in-memory failure list for printout at end-of-suite.
	// No live logging: the only unit-test output is the final summary.
	Zenith_TestFailure* pxFail = new Zenith_TestFailure;
	pxFail->m_strCategory = m_pxCurrentCase ? m_pxCurrentCase->m_strCategory : "<none>";
	pxFail->m_strName     = m_pxCurrentCase ? m_pxCurrentCase->m_strName     : "<none>";
	pxFail->m_strFile     = strFile;
	pxFail->m_iLine       = iLine;
	snprintf(pxFail->m_acMsg, sizeof(pxFail->m_acMsg), "%s: %s", strType, strMsg);
	pxFail->m_pxNext      = nullptr;

	// Insert at tail so the summary prints failures in the order they happened.
	if (m_pxFirstFailure == nullptr)
	{
		m_pxFirstFailure = pxFail;
	}
	else
	{
		Zenith_TestFailure* pxTail = m_pxFirstFailure;
		while (pxTail->m_pxNext != nullptr) pxTail = pxTail->m_pxNext;
		pxTail->m_pxNext = pxFail;
	}

	// Do NOT call Zenith_DebugBreak() here. EXPECT-style semantics: the test
	// body continues past the failing assertion so we can collect every
	// failure. Engine-side Zenith_Assert calls still break normally and get
	// caught by the SEH handler in RunAllTests for crash isolation.
	m_bCurrentTestFailed = true;
}

static void FormatUserMsg(char* acOut, size_t uOutSize, const char* strFormat, va_list args)
{
	if (strFormat == nullptr || strFormat[0] == '\0')
	{
		acOut[0] = '\0';
	}
	else
	{
		vsnprintf(acOut, uOutSize, strFormat, args);
	}
}

void Zenith_TestRunner::AssertTrue(bool bExpr, const char* strExpr, const char* strFile, int iLine, const char* strFormat, ...)
{
	if (!bExpr)
	{
		char acUser[512]; va_list args; va_start(args, strFormat);
		FormatUserMsg(acUser, sizeof(acUser), strFormat, args); va_end(args);
		char acFull[768];
		snprintf(acFull, sizeof(acFull), "(%s)%s%s", strExpr, acUser[0] ? " - " : "", acUser);
		HandleFailure("ZENITH_ASSERT_TRUE", acFull, strFile, iLine);
	}
}

void Zenith_TestRunner::AssertFalse(bool bExpr, const char* strExpr, const char* strFile, int iLine, const char* strFormat, ...)
{
	if (bExpr)
	{
		char acUser[512]; va_list args; va_start(args, strFormat);
		FormatUserMsg(acUser, sizeof(acUser), strFormat, args); va_end(args);
		char acFull[768];
		snprintf(acFull, sizeof(acFull), "!(%s)%s%s", strExpr, acUser[0] ? " - " : "", acUser);
		HandleFailure("ZENITH_ASSERT_FALSE", acFull, strFile, iLine);
	}
}

void Zenith_TestRunner::AssertNull(const void* pPtr, const char* strExpr, const char* strFile, int iLine, const char* strFormat, ...)
{
	if (pPtr != nullptr)
	{
		char acUser[512]; va_list args; va_start(args, strFormat);
		FormatUserMsg(acUser, sizeof(acUser), strFormat, args); va_end(args);
		char acFull[768];
		snprintf(acFull, sizeof(acFull), "%s (%p) is not null%s%s", strExpr, pPtr, acUser[0] ? " - " : "", acUser);
		HandleFailure("ZENITH_ASSERT_NULL", acFull, strFile, iLine);
	}
}

void Zenith_TestRunner::AssertNotNull(const void* pPtr, const char* strExpr, const char* strFile, int iLine, const char* strFormat, ...)
{
	if (pPtr == nullptr)
	{
		char acUser[512]; va_list args; va_start(args, strFormat);
		FormatUserMsg(acUser, sizeof(acUser), strFormat, args); va_end(args);
		char acFull[768];
		snprintf(acFull, sizeof(acFull), "%s is null%s%s", strExpr, acUser[0] ? " - " : "", acUser);
		HandleFailure("ZENITH_ASSERT_NOT_NULL", acFull, strFile, iLine);
	}
}

void Zenith_TestRunner::AssertStrEq(const char* strA, const char* strB, const char* strFile, int iLine, const char* strFormat, ...)
{
	if (strA == nullptr || strB == nullptr || strcmp(strA, strB) != 0)
	{
		char acUser[512]; va_list args; va_start(args, strFormat);
		FormatUserMsg(acUser, sizeof(acUser), strFormat, args); va_end(args);
		char acFull[1280];
		snprintf(acFull, sizeof(acFull), "\"%s\" != \"%s\"%s%s",
			strA ? strA : "(null)", strB ? strB : "(null)",
			acUser[0] ? " - " : "", acUser);
		HandleFailure("ZENITH_ASSERT_STREQ", acFull, strFile, iLine);
	}
}

void Zenith_TestRunner::AssertEqFloat(float fA, float fB, float fEps,
	const char* strExprA, const char* strExprB, const char* strFile, int iLine, const char* strFormat, ...)
{
	if (std::fabs(fA - fB) > fEps)
	{
		char acUser[512]; va_list args; va_start(args, strFormat);
		FormatUserMsg(acUser, sizeof(acUser), strFormat, args); va_end(args);
		char acFull[1024];
		snprintf(acFull, sizeof(acFull), "%s (%f) != %s (%f) within %f%s%s",
			strExprA, fA, strExprB, fB, fEps, acUser[0] ? " - " : "", acUser);
		HandleFailure("ZENITH_ASSERT_EQ_FLOAT", acFull, strFile, iLine);
	}
}

void Zenith_TestRunner::AssertNearVec3(const Zenith_Maths::Vector3& xA, const Zenith_Maths::Vector3& xB, float fEps,
	const char* strExprA, const char* strExprB, const char* strFile, int iLine, const char* strFormat, ...)
{
	if (glm::abs(xA.x - xB.x) > fEps ||
		glm::abs(xA.y - xB.y) > fEps ||
		glm::abs(xA.z - xB.z) > fEps)
	{
		char acUser[512]; va_list args; va_start(args, strFormat);
		FormatUserMsg(acUser, sizeof(acUser), strFormat, args); va_end(args);
		char acFull[1024];
		snprintf(acFull, sizeof(acFull),
			"%s (%.3f,%.3f,%.3f) != %s (%.3f,%.3f,%.3f) within %f%s%s",
			strExprA, xA.x, xA.y, xA.z, strExprB, xB.x, xB.y, xB.z, fEps,
			acUser[0] ? " - " : "", acUser);
		HandleFailure("ZENITH_ASSERT_NEAR_VEC3", acFull, strFile, iLine);
	}
}

void Zenith_TestRunner::Fail(const char* strMsg, const char* strFile, int iLine)
{
	HandleFailure("ZENITH_FAIL", strMsg, strFile, iLine);
}

void Zenith_TestRunner::Skip(const char* /*strReason*/)
{
	// No live logging — the final summary reports the total skipped count.
	m_bCurrentTestSkipped = true;
}

#endif // ZENITH_TESTING
