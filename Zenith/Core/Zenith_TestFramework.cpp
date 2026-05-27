#include "Zenith.h"
#include "Zenith_TestFramework.h"

#ifdef ZENITH_TESTING

#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneSystemBootstrap.h"

#include <cstring>
#include <cmath>

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
#ifdef ZENITH_WINDOWS
	__try { Zenith_ResetSceneSystemForNextTest(); }
	__except (EXCEPTION_EXECUTE_HANDLER) {}
#else
	Zenith_ResetSceneSystemForNextTest();
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

		pxCase->m_pfnTest();

		if (m_bCurrentTestSkipped)    m_uSkippedCount++;
		else if (m_bCurrentTestFailed) m_uFailedCount++;
		else                           m_uPassedCount++;

		m_pxCurrentCase = nullptr;

		// Clear any capture state a crashed test may have left on.
		g_bAssertCaptureActive.store(false, std::memory_order_release);
		g_uAssertCaptureHitCount.store(0, std::memory_order_release);

		// Reset engine-wide state so the next test starts clean. Wrap in SEH
		// because a catastrophically broken test could corrupt the state we're
		// about to touch.
		Zenith_TestResetGlobalState();
	}

	// Final summary — the only unit-test logging the suite emits.
	Zenith_Log(LOG_CATEGORY_UNITTEST,
		"Unit tests complete: %u ran, %u passed, %u failed, %u skipped",
		m_uTestCount,
		m_uPassedCount,
		m_uFailedCount,
		m_uSkippedCount
		);
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
