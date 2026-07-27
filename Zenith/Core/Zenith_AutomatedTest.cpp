#include "Zenith.h"
#include "Core/Zenith_Engine.h"
// Needed for the clean-shutdown path on early-exit branches (list,
// not-found, no-tests-registered). Zenith_Window.h is platform-routed by
// Zenith.h so the include is portable between win64 and Android.
#include "Core/Zenith_Core.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "Input/Zenith_InputSimulator.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_EventSystem.h"
#include "FileAccess/Zenith_FileAccess.h"
// Between-tests engine hygiene: the process-level test-instrumentation logs and
// counters that no scene owns.
#include "Core/Zenith_AudioBus.h"
#include "Flux/Zenith_RenderBus.h"
#include "AI/Navigation/Zenith_NavMesh.h"
#include "SaveData/Zenith_SaveData.h"

#ifdef ZENITH_TOOLS
#include "Editor/Zenith_Editor.h"
#include "Editor/Zenith_EditorAutomation.h"
#include "Editor/Zenith_SelectionSystem.h"   // entity-keyed bounds cache
#include "Editor/Zenith_UndoSystem.h"        // EntityID-keyed undo/redo stacks
#endif

#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <chrono>

// ============================================================================
// Linked-list registry — populated by static initializers
// ============================================================================
namespace
{
	Zenith_AutomatedTestNode* s_pxTestListHead = nullptr;

	// Between-tests cleanup hooks. Capped to a small fixed array — games
	// register one or two and we don't want a heap allocation here because
	// the harness sits in the engine module.
	constexpr int kMaxBetweenTestsHooks = 8;
	Zenith_AutomatedTestRunner::BetweenTestsHook s_apfnBetweenTestsHooks[kMaxBetweenTestsHooks] = {};
	int s_iBetweenTestsHookCount = 0;

	// ------------------------------------------------------------------
	// Ordered execution list.
	//
	// Every selection mode — one named test (--automated-test), an explicit
	// ordered name list (--automated-tests), or the whole suite with an
	// optional order transform (--all-automated-tests [--batch-order]) —
	// resolves to this array during ParseCommandLine. Tick's advance is then
	// a single index bump rather than a linked-list walk, and the ordering
	// question lives in exactly one place.
	//
	// Fixed capacity for the same reason the hook array is fixed: the
	// harness sits in the engine module and runs on both sides of subsystem
	// lifetimes, so it stays heap-free. Overflow is a loud error, never a
	// silent truncation.
	constexpr int kMaxOrderedTests = 1024;
	const Zenith_AutomatedTestNode* s_apxOrderedNodes[kMaxOrderedTests] = {};
	int s_iOrderedCount = 0;

	// Backing storage for --automated-tests: argv hands us one comma-joined
	// token and the split needs writable memory that outlives the parse
	// (the resolved name pointers are only used during ParseCommandLine, but
	// keeping them alive costs nothing and avoids a lifetime trap).
	constexpr int kNameListBufferSize = 8192;
	char        s_acNameListBuffer[kNameListBufferSize] = {};
	const char* s_apszNameTokens[kMaxOrderedTests]      = {};

	// --batch-order spec. Registration order (the flag absent) is the
	// identity transform and is exactly what --all-automated-tests has
	// always run.
	enum class BatchOrderKind : uint32_t
	{
		Registration,
		Reverse,
		Rotate
	};

	struct BatchOrderSpec
	{
		BatchOrderKind m_eKind   = BatchOrderKind::Registration;
		int            m_iRotate = 0;
		bool           m_bValid  = true;
	};
}

// ============================================================================
// Order-control helpers (pure — unit-tested by Zenith_AutomatedTest.Tests.inl)
// ============================================================================

// Parses a --batch-order spec. nullptr / "" is the identity transform.
// Anything not exactly "reverse" or "rotate:<non-negative integer>" is
// rejected — including "rotate:-1", which the contract refuses outright
// rather than silently wrapping, and an over-long digit run, which would
// overflow the int conversion.
static BatchOrderSpec ParseBatchOrderSpec(const char* szSpec)
{
	BatchOrderSpec xSpec;
	if (szSpec == nullptr || szSpec[0] == '\0') return xSpec;

	if (std::strcmp(szSpec, "reverse") == 0)
	{
		xSpec.m_eKind = BatchOrderKind::Reverse;
		return xSpec;
	}

	static const char szRotatePrefix[] = "rotate:";
	constexpr size_t uPrefixLen = sizeof(szRotatePrefix) - 1;
	if (std::strncmp(szSpec, szRotatePrefix, uPrefixLen) == 0)
	{
		const char* szValue = szSpec + uPrefixLen;
		int iDigits = 0;
		for (const char* p = szValue; *p != '\0'; ++p, ++iDigits)
		{
			// Digits only. A leading '-' (negative rotation), a '+', and any
			// trailing garbage all land here and fail the spec.
			if (*p < '0' || *p > '9') { xSpec.m_bValid = false; return xSpec; }
		}
		// 9 digits is the widest run that cannot overflow a 32-bit int.
		if (iDigits == 0 || iDigits > 9) { xSpec.m_bValid = false; return xSpec; }
		xSpec.m_eKind   = BatchOrderKind::Rotate;
		xSpec.m_iRotate = std::atoi(szValue);
		return xSpec;
	}

	xSpec.m_bValid = false;
	return xSpec;
}

// Rotation is normalised modulo the suite size, so rotate:<N> is always a
// valid start offset; rotate:0 — and any exact multiple of the size — is the
// untransformed registration order.
static int NormalizeRotation(int iRotate, int iCount)
{
	if (iCount <= 0 || iRotate <= 0) return 0;
	return iRotate % iCount;
}

// Splits a NUL-terminated, WRITABLE comma-separated list into tokens in
// place. Surrounding spaces/tabs are trimmed and empty tokens are dropped
// ("A,,B" == "A,B"), so a trailing comma is harmless. Returns the token
// count, or -1 when the cap would be exceeded (never a silent truncation).
static int SplitCommaList(char* acBuffer, const char** apszOut, int iMaxOut)
{
	if (acBuffer == nullptr || apszOut == nullptr || iMaxOut <= 0) return 0;

	int   iCount   = 0;
	char* pcCursor = acBuffer;
	while (true)
	{
		char* pcComma = std::strchr(pcCursor, ',');
		if (pcComma != nullptr) *pcComma = '\0';

		char* pcStart = pcCursor;
		while (*pcStart == ' ' || *pcStart == '\t') ++pcStart;
		char* pcEnd = pcStart + std::strlen(pcStart);
		while (pcEnd > pcStart && (pcEnd[-1] == ' ' || pcEnd[-1] == '\t')) --pcEnd;
		*pcEnd = '\0';

		if (pcStart[0] != '\0')
		{
			if (iCount >= iMaxOut) return -1;
			apszOut[iCount++] = pcStart;
		}

		if (pcComma == nullptr) break;
		pcCursor = pcComma + 1;
	}
	return iCount;
}

// How many of the three mutually-exclusive selection flags were supplied.
// >1 is an ambiguous request, not a merge.
static int CountSelectedModes(bool bSingleName, bool bNameList, bool bAllTests)
{
	return (bSingleName ? 1 : 0) + (bNameList ? 1 : 0) + (bAllTests ? 1 : 0);
}

static void ReverseNodeRange(const Zenith_AutomatedTestNode** apxNodes, int iFirst, int iLast)
{
	while (iFirst < iLast)
	{
		const Zenith_AutomatedTestNode* pxTmp = apxNodes[iFirst];
		apxNodes[iFirst++] = apxNodes[iLast];
		apxNodes[iLast--]  = pxTmp;
	}
}

// Applies a --batch-order transform in place. Rotation is "rotate left by
// N": the N-th test becomes the first and the prefix wraps to the end.
// Implemented as the three-reversal rotation so no scratch array is needed.
static void ApplyBatchOrder(const Zenith_AutomatedTestNode** apxNodes, int iCount, const BatchOrderSpec& xSpec)
{
	if (apxNodes == nullptr || iCount <= 1) return;

	if (xSpec.m_eKind == BatchOrderKind::Reverse)
	{
		ReverseNodeRange(apxNodes, 0, iCount - 1);
	}
	else if (xSpec.m_eKind == BatchOrderKind::Rotate)
	{
		const int iShift = NormalizeRotation(xSpec.m_iRotate, iCount);
		if (iShift == 0) return;
		ReverseNodeRange(apxNodes, 0, iShift - 1);
		ReverseNodeRange(apxNodes, iShift, iCount - 1);
		ReverseNodeRange(apxNodes, 0, iCount - 1);
	}
}

void Zenith_AutomatedTestRunner::RegisterNode(Zenith_AutomatedTestNode* pxNode)
{
	if (pxNode == nullptr) return;
	pxNode->m_pxNext = s_pxTestListHead;
	s_pxTestListHead = pxNode;
}

void Zenith_AutomatedTestRunner::RegisterBetweenTestsHook(BetweenTestsHook pfn)
{
	if (pfn == nullptr) return;
	if (s_iBetweenTestsHookCount >= kMaxBetweenTestsHooks)
	{
		Zenith_Warning(LOG_CATEGORY_CORE,
			"AutomatedTest: too many between-tests hooks (max %d)",
			kMaxBetweenTestsHooks);
		return;
	}
	s_apfnBetweenTestsHooks[s_iBetweenTestsHookCount++] = pfn;
}

static void FireBetweenTestsHooks()
{
	// The hooks are now a harmless overlay: BetweenTests destroys the world and
	// rebuilds it before this runs, so every hook body acts on already-fresh
	// state. They stay registered for one more change purely to keep this flip
	// separable from their removal. Measured redundant with them suppressed --
	// DevilsPlayground 158/158 and Zenithmon 44/44, in registration order,
	// reversed, and rotated.
	for (int i = 0; i < s_iBetweenTestsHookCount; ++i)
	{
		if (s_apfnBetweenTestsHooks[i] != nullptr)
		{
			s_apfnBetweenTestsHooks[i]();
		}
	}
}

// ============================================================================
// Between-tests engine hygiene
//
// The process-level state that belongs to no scene and no entity: the
// test-instrumentation logs, the query counter, the deferred-event queue, and
// the owned save sandbox on disk. Everything world-shaped is handled by
// destroying the world; this is the short, explicit list of what is left.
//
// Each entry is here because the state OUTLIVES a scene by design:
//   * audio / render-bus logs + navmesh query counter -- process-global
//     recording buffers a test asserts against. Each was previously cleared by
//     exactly one test, at its own Setup, which only works while that test is
//     the only reader.
//   * deferred events -- owned payloads queued by one test but never drained
//     would be delivered into the NEXT test's subscribers, mid-scenario.
//     Subscriptions are deliberately NOT touched (wiping them breaks games).
//   * the save sandbox -- files on disk, which no in-memory reset can reach.
// The frame stamps on the audio / render buses stay monotonic by design: tests
// assert on deltas, not absolute frame numbers.
// ============================================================================
static void RunBetweenTestsEngineHygiene()
{
	Zenith_EventDispatcher::Get().DiscardDeferredEvents();
	Zenith_AudioBus::ClearEmittedSoundsForTest();
	Zenith_RenderBus::ClearDrawCallsForTest();
	Zenith_NavMesh::ResetQueryCountForTest();
	Zenith_SaveData::WipeTestSandbox();
#ifdef ZENITH_TOOLS
	// Entity-keyed editor-adjacent caches. They sit on sibling engine
	// subsystems rather than inside Zenith_Editor, and every entry is keyed by
	// an EntityID the reset just invalidated: the selection system's bounds
	// cache, and the undo/redo stacks (raw Zenith_UndoCommand* holding EntityIDs
	// of entities that no longer exist -- Clear() deletes the owned commands).
	g_xEngine.Selection().m_xEntityBoundingBoxes.Clear();
	g_xEngine.UndoSystem().Clear();
#endif
}

// ============================================================================
// Runner state
// ============================================================================
namespace
{
	enum class HarnessPhase : uint32_t
	{
		Disabled,
		WaitForAutomationComplete,
		WaitForSceneLoaded,
		EnterPlayingMode,
		FlushFirstFrameOnStart,
		ResetSimulatorAndCallSetup,
		Stepping,
		VerifyAndExit,
		BetweenTests,             // batch mode: settle one frame between tests
		Done
	};

	struct RunnerState
	{
		HarnessPhase                    m_ePhase             = HarnessPhase::Disabled;
		// Index into s_apxOrderedNodes. -1 = no test selected yet.
		int                             m_iCurrentIndex      = -1;
		const char*                     m_szRequestedName    = nullptr;
		const char*                     m_szResultsPath      = nullptr;
		const char*                     m_szResultsDir       = nullptr;
		int                             m_iStepFrame         = 0;
		int                             m_iMaxFramesOverride = -1;          // set via --exit-after-frames
		// Defaulted (not -1 "unset") so the harness PINS dt across the world
		// reset + settle + the Setup/Step-0 tick. UpdateTimers applies the
		// override at frame TOP, and ResetSimulatorAndCallSetup falls through to
		// Stepping in the SAME tick, so a test's own SetFixedDt in Setup lands
		// too late for that frame -- leaving Step 0 (which loads the scene) to
		// run game logic on wall-clock dt. Anything integrating dt from the frame
		// it was created on then carries a real frame time forever. --fixed-dt
		// overrides the value; it no longer decides whether dt is fixed at all.
		float                           m_fFixedDt           = 1.0f / 60.0f;   // overridden by --fixed-dt
		int                             m_iPendingExitCode   = 0;
		bool                            m_bListThenExit      = false;
		bool                            m_bRunAllTests       = false;   // --all-automated-tests
		bool                            m_bRunNamedList      = false;   // --automated-tests a,b,c
		bool                            m_bAnyFailures       = false;
		// For VerifyAndExit serialization — populated lazily.
		bool                            m_bVerifyReported    = false;
		bool                            m_bVerifyPassed      = false;
		// Batch-mode tally
		int                             m_iTotalTests        = 0;
		int                             m_iPassedTests       = 0;
		int                             m_iFailedTests       = 0;
		// Counted separately from passed: an infrastructure-skipped test never
		// ran, and reporting it as green is exactly the failure mode the
		// structured infrastructure error exists to prevent.
		int                             m_iSkippedTests      = 0;
		// BetweenTests sub-state: counts frames since the boot-scene reload
		// was triggered. -1 = scene reload not yet triggered for this gap.
		int                             m_iBetweenTestsFrame = -1;
		// Set when ResetSimulatorAndCallSetup detects that the current test
		// requires graphics but this is a Null (GPU-less) build. Causes
		// VerifyAndExit to skip the Verify() call, emit a SKIPPED log line,
		// and write results JSON with skipped=true. Reused via the same
		// advance/finalise code path so we don't duplicate state-machine
		// bookkeeping.
		bool                            m_bSkipCurrentTest   = false;
		// True once Setup has been entered for the current test. Gates the
		// Teardown callback: the pre-Setup graphics / manual-only skips never
		// ran the test's code, so they have nothing to undo, and calling
		// Teardown for them would break the exactly-one-per-Setup contract.
		bool                            m_bSetupRan          = false;
		// Wall-clock test timing. Captured immediately before Setup runs
		// (or immediately before the skip-decision for graphics-required
		// tests in a Null build) and consumed in VerifyAndExit to
		// populate m_fLastDurationMs on the node + the JSON + stdout.
		// Excludes the BetweenTests scene-reload settle phase so the
		// reported duration reflects the test's own work, not harness
		// bookkeeping.
		std::chrono::high_resolution_clock::time_point m_xTestStartTime;
	};
	RunnerState s_xRunner;

	const Zenith_AutomatedTestNode* CurrentNode()
	{
		if (s_xRunner.m_iCurrentIndex < 0 || s_xRunner.m_iCurrentIndex >= s_iOrderedCount) return nullptr;
		return s_apxOrderedNodes[s_xRunner.m_iCurrentIndex];
	}

	const Zenith_AutomatedTest* CurrentTest()
	{
		const Zenith_AutomatedTestNode* pxNode = CurrentNode();
		return pxNode ? pxNode->m_pxTest : nullptr;
	}

	// True for any run that executes more than one test out of one process:
	// the whole suite OR an explicit ordered name list. Drives per-test JSON
	// emission (results-DIR rather than results-PATH) and the suite summary.
	// Deliberately NOT the same predicate as m_bRunAllTests, which alone
	// governs the manual-only exclusion: naming a manual-only test explicitly
	// still runs it, exactly as single-test mode always has.
	bool IsMultiTestRun()
	{
		return s_xRunner.m_bRunAllTests || s_xRunner.m_bRunNamedList;
	}

	// Called at every transition INTO BetweenTests, i.e. BEFORE the world reset
	// and settle rather than after them (which is where the simulator reset used
	// to happen, inside ResetSimulatorAndCallSetup).
	//
	// Two things leak across a test boundary otherwise:
	//   * HELD keys / mouse buttons. The previous test's held input drove the
	//     freshly-loaded boot scene for the whole settle window -- which is why
	//     RT_PlayerActions has to hand-release its keys in Verify.
	//   * the fixed-dt override. ResetAllInputState does NOT clear it, and the
	//     harness never did either, so a test that pinned a dt (the tennis
	//     digest, the DP personality runs) silently changed the timebase of
	//     every test after it.
	// Re-applying the CLI value, or explicitly clearing when there is none,
	// makes the boundary deterministic either way.
	void NormalizeInputAndFixedDtForNextTest()
	{
		Zenith_InputSimulator::ResetAllInputState();
		if (s_xRunner.m_fFixedDt > 0.0f)
		{
			Zenith_InputSimulator::SetFixedDt(s_xRunner.m_fFixedDt);
		}
		else
		{
			Zenith_InputSimulator::ClearFixedDt();
		}
	}

	// Shared early-exit for the parse-time error paths. ParseCommandLine runs
	// AFTER Zenith_Init, so there is a fully-initialised engine to tear down;
	// going through the canonical full-shutdown wrapper means adding a new
	// init-only singleton doesn't silently rot these branches.
	[[noreturn]] void ExitHarness(int iCode)
	{
		std::fflush(stdout);
		s_xRunner.m_iPendingExitCode = iCode;
		Zenith_Core::Zenith_FullShutdown();
		std::exit(iCode);
	}
}

bool Zenith_AutomatedTestRunner::IsActive()
{
	return s_xRunner.m_ePhase != HarnessPhase::Disabled
	    && s_xRunner.m_ePhase != HarnessPhase::Done;
}

void Zenith_AutomatedTestRunner::SetPendingExitCode(int iCode)
{
	s_xRunner.m_iPendingExitCode = iCode;
}

int Zenith_AutomatedTestRunner::GetPendingExitCode()
{
	return s_xRunner.m_iPendingExitCode;
}

void Zenith_AutomatedTestRunner::RequestSkip(const char* szReason)
{
	// Called from within a test's Setup. Sets the same skip flag the graphics /
	// manual-only pre-Setup skips use; ResetSimulatorAndCallSetup honours it right
	// after Setup returns (routing straight to VerifyAndExit), so it finalises as
	// SKIPPED with the identical tally / JSON path.
	const Zenith_AutomatedTest* pxTest = CurrentTest();
	Zenith_Log(LOG_CATEGORY_UNITTEST,
		"[AutomatedTest] %s: SKIPPED (%s)",
		(pxTest != nullptr && pxTest->m_szName != nullptr) ? pxTest->m_szName : "(unknown)",
		szReason != nullptr ? szReason : "runtime skip");
	s_xRunner.m_bSkipCurrentTest = true;
}

void Zenith_AutomatedTestRunner::PrintRegisteredTests()
{
	std::printf("Registered automated tests:\n");
	for (const Zenith_AutomatedTestNode* p = s_pxTestListHead; p != nullptr; p = p->m_pxNext)
	{
		const char* szName = (p->m_pxTest && p->m_pxTest->m_szName) ? p->m_pxTest->m_szName : "(unnamed)";
		std::printf("  %s\n", szName);
	}
	std::fflush(stdout);
}

void Zenith_AutomatedTestRunner::ResetRegistry_TEST_ONLY()
{
	s_pxTestListHead = nullptr;
	s_iOrderedCount  = 0;
	s_xRunner = RunnerState();
}

// ============================================================================
// CLI parsing
// ============================================================================
static const Zenith_AutomatedTestNode* FindNodeByName(const char* szName)
{
	if (szName == nullptr) return nullptr;
	for (const Zenith_AutomatedTestNode* p = s_pxTestListHead; p != nullptr; p = p->m_pxNext)
	{
		if (p->m_pxTest == nullptr) continue;
		if (p->m_pxTest->m_szName != nullptr
			&& std::strcmp(p->m_pxTest->m_szName, szName) == 0)
		{
			return p;
		}
	}
	return nullptr;
}

// Collects every registered node into the ordered execution list, in
// registry-walk order — the same set and order --all-automated-tests has
// always run (RegisterNode prepends, so within a TU this is reverse
// declaration order; that is the historical baseline, not a claim about
// declaration order). Returns false when the fixed capacity is exceeded.
static bool CollectAllNodesInRegistrationOrder()
{
	s_iOrderedCount = 0;
	for (const Zenith_AutomatedTestNode* p = s_pxTestListHead; p != nullptr; p = p->m_pxNext)
	{
		if (p->m_pxTest == nullptr) continue;
		if (s_iOrderedCount >= kMaxOrderedTests) return false;
		s_apxOrderedNodes[s_iOrderedCount++] = p;
	}
	return true;
}

// Resolves an --automated-tests spec into the ordered execution list.
// Duplicates are deliberate and each occurrence runs (`A,A` is a
// self-contamination probe) — note both occurrences write the same
// <name>.json, last-wins, which is acceptable for probe use.
// Every unknown name is reported before bailing so a typo'd list needs one
// run to diagnose, not one per name. Returns false after printing the
// diagnosis; the caller exits 2 without running anything.
static bool BuildOrderedListFromNames(const char* szSpec)
{
	if (szSpec == nullptr) return false;

	const size_t uLen = std::strlen(szSpec);
	if (uLen + 1 > static_cast<size_t>(kNameListBufferSize))
	{
		std::printf("ERROR: --automated-tests list is too long (max %d characters).\n",
			kNameListBufferSize - 1);
		return false;
	}
	std::memcpy(s_acNameListBuffer, szSpec, uLen + 1);

	const int iNameCount = SplitCommaList(s_acNameListBuffer, s_apszNameTokens, kMaxOrderedTests);
	if (iNameCount < 0)
	{
		std::printf("ERROR: --automated-tests names exceed the harness capacity of %d entries.\n",
			kMaxOrderedTests);
		return false;
	}
	if (iNameCount == 0)
	{
		std::printf("ERROR: --automated-tests requires a comma-separated list of test names.\n");
		return false;
	}

	int iUnknown = 0;
	for (int i = 0; i < iNameCount; ++i)
	{
		if (FindNodeByName(s_apszNameTokens[i]) == nullptr)
		{
			std::printf("ERROR: --automated-tests: '%s' not found in registry.\n", s_apszNameTokens[i]);
			++iUnknown;
		}
	}
	if (iUnknown > 0)
	{
		std::printf("       %d unknown test name(s) -- nothing was run. "
			"Run with --list-automated-tests for the full list.\n", iUnknown);
		return false;
	}

	s_iOrderedCount = 0;
	for (int i = 0; i < iNameCount; ++i)
	{
		s_apxOrderedNodes[s_iOrderedCount++] = FindNodeByName(s_apszNameTokens[i]);
	}
	return true;
}

void Zenith_AutomatedTestRunner::ParseCommandLine(int argc, char** argv)
{
	const char* szNamedList  = nullptr;
	const char* szBatchOrder = nullptr;

	for (int i = 1; i < argc; ++i)
	{
		const char* szArg = argv[i];

		if (std::strcmp(szArg, "--list-automated-tests") == 0)
		{
			s_xRunner.m_bListThenExit = true;
		}
		else if (std::strcmp(szArg, "--automated-test") == 0 && i + 1 < argc)
		{
			s_xRunner.m_szRequestedName = argv[++i];
		}
		else if (std::strcmp(szArg, "--automated-tests") == 0 && i + 1 < argc)
		{
			szNamedList = argv[++i];
		}
		else if (std::strcmp(szArg, "--all-automated-tests") == 0)
		{
			s_xRunner.m_bRunAllTests = true;
		}
		else if (std::strcmp(szArg, "--batch-order") == 0 && i + 1 < argc)
		{
			szBatchOrder = argv[++i];
		}
		else if (std::strcmp(szArg, "--test-results") == 0 && i + 1 < argc)
		{
			s_xRunner.m_szResultsPath = argv[++i];
		}
		else if (std::strcmp(szArg, "--test-results-dir") == 0 && i + 1 < argc)
		{
			s_xRunner.m_szResultsDir = argv[++i];
		}
		else if (std::strcmp(szArg, "--exit-after-frames") == 0 && i + 1 < argc)
		{
			s_xRunner.m_iMaxFramesOverride = std::atoi(argv[++i]);
		}
		else if (std::strcmp(szArg, "--fixed-dt") == 0 && i + 1 < argc)
		{
			s_xRunner.m_fFixedDt = static_cast<float>(std::atof(argv[++i]));
		}
	}

	if (s_xRunner.m_bListThenExit)
	{
		PrintRegisteredTests();
		// Clean shutdown instead of a bare std::exit so GPU/Jolt/audio
		// resources release in the normal order. Without it,
		// --list-automated-tests would leak VRAM allocations, Jolt's
		// body-interface lock, and the GLFW window — usually observable only
		// as a noisy exit code from the Vulkan validation layer, but bad
		// hygiene in any case.
		ExitHarness(0);
	}

	// The three selection flags each BUILD the ordered execution list, so
	// supplying more than one is an ambiguous request rather than a merge.
	const int iModes = CountSelectedModes(
		s_xRunner.m_szRequestedName != nullptr,
		szNamedList != nullptr,
		s_xRunner.m_bRunAllTests);
	if (iModes > 1)
	{
		std::printf("ERROR: --automated-test, --automated-tests and --all-automated-tests "
			"are mutually exclusive; pass exactly one.\n");
		ExitHarness(2);
	}

	// --batch-order reorders the FULL suite; it is meaningless against an
	// explicit name list (which already states its order) or a single test.
	// Rejecting it loudly beats silently ignoring it in a diagnosis run.
	if (szBatchOrder != nullptr && !s_xRunner.m_bRunAllTests)
	{
		std::printf("ERROR: --batch-order requires --all-automated-tests.\n");
		ExitHarness(2);
	}
	const BatchOrderSpec xOrder = ParseBatchOrderSpec(szBatchOrder);
	if (!xOrder.m_bValid)
	{
		std::printf("ERROR: --batch-order '%s' is not understood. "
			"Use 'reverse' or 'rotate:<N>' with N >= 0.\n", szBatchOrder);
		ExitHarness(2);
	}

	if (iModes == 0)
	{
		// No automated-test mode requested — leave the harness disabled and
		// let the normal game boot proceed.
		return;
	}

	if (s_xRunner.m_bRunAllTests)
	{
		if (!CollectAllNodesInRegistrationOrder())
		{
			std::printf("ERROR: --all-automated-tests: the registry exceeds the harness "
				"capacity of %d tests.\n", kMaxOrderedTests);
			ExitHarness(2);
		}
		if (s_iOrderedCount == 0)
		{
			std::printf("ERROR: --all-automated-tests requested but no tests are registered.\n");
			ExitHarness(2);
		}
		ApplyBatchOrder(s_apxOrderedNodes, s_iOrderedCount, xOrder);
	}
	else if (szNamedList != nullptr)
	{
		s_xRunner.m_bRunNamedList = true;
		// BuildOrderedListFromNames prints its own diagnosis (unknown names,
		// empty list, capacity) before returning false.
		if (!BuildOrderedListFromNames(szNamedList))
		{
			ExitHarness(2);
		}
	}
	else
	{
		const Zenith_AutomatedTestNode* pxNode = FindNodeByName(s_xRunner.m_szRequestedName);
		if (pxNode == nullptr)
		{
			std::printf("ERROR: --automated-test '%s' not found in registry. "
				"Run with --list-automated-tests for the full list.\n",
				s_xRunner.m_szRequestedName);
			ExitHarness(2);
		}
		s_apxOrderedNodes[0] = pxNode;
		s_iOrderedCount      = 1;
	}

	// Echo the plan whenever the order is NOT the historical default, so a
	// diagnosis run records exactly what it executed. A plain
	// --all-automated-tests run stays quiet (158 extra lines is noise).
	if (s_xRunner.m_bRunNamedList || szBatchOrder != nullptr)
	{
		std::printf("[AutomatedTest] Execution order (%d test(s)):\n", s_iOrderedCount);
		for (int i = 0; i < s_iOrderedCount; ++i)
		{
			const Zenith_AutomatedTest* pxTest = s_apxOrderedNodes[i]->m_pxTest;
			std::printf("  %3d. %s\n", i + 1,
				(pxTest && pxTest->m_szName) ? pxTest->m_szName : "(unnamed)");
		}
		std::fflush(stdout);
	}

	Zenith_InputSimulator::Enable();
	s_xRunner.m_iCurrentIndex = 0;
	s_xRunner.m_ePhase        = HarnessPhase::WaitForAutomationComplete;
}

// ============================================================================
// JSON results writer (hand-rolled to avoid a library dependency)
// ============================================================================
static void WriteResultsJson(const RunnerState& xRunner,
                             const Zenith_AutomatedTest* pxTest,
                             bool bPassed,
                             float fDurationMs,
                             bool bSkipped = false,
                             const char* szSkipReason = nullptr)
{
	// Resolve the output path. Any multi-test run (the whole suite OR an
	// explicit ordered name list) prefers m_szResultsDir/<name>.json; single
	// mode uses m_szResultsPath verbatim. If neither is set, skip.
	char axPath[512];
	const char* szPath = nullptr;
	const bool bMultiTest = xRunner.m_bRunAllTests || xRunner.m_bRunNamedList;

	if (bMultiTest && xRunner.m_szResultsDir != nullptr
	    && pxTest != nullptr && pxTest->m_szName != nullptr)
	{
		std::snprintf(axPath, sizeof(axPath), "%s/%s.json",
			xRunner.m_szResultsDir, pxTest->m_szName);
		szPath = axPath;
	}
	else if (!bMultiTest && xRunner.m_szResultsPath != nullptr)
	{
		szPath = xRunner.m_szResultsPath;
	}

	if (szPath == nullptr) return;

	std::FILE* pxFile = nullptr;
#ifdef _MSC_VER
	const errno_t iErr = ::fopen_s(&pxFile, szPath, "wb");
	if (iErr != 0 || pxFile == nullptr)
#else
	pxFile = std::fopen(szPath, "wb");
	if (pxFile == nullptr)
#endif
	{
		Zenith_Warning(LOG_CATEGORY_CORE,
			"AutomatedTest: failed to open results path %s", szPath);
		return;
	}
	const char* szName = pxTest && pxTest->m_szName ? pxTest->m_szName : "unknown";
	std::fprintf(pxFile,
		"{\n"
		"  \"name\": \"%s\",\n"
		"  \"passed\": %s,\n"
		"  \"frames\": %d,\n"
		"  \"durationMs\": %.3f,\n"
		"  \"failures\": [],\n"
		"  \"skipped\": %s,\n"
		"  \"skipReason\": \"%s\"\n"
		"}\n",
		szName,
		bPassed ? "true" : "false",
		xRunner.m_iStepFrame,
		static_cast<double>(fDurationMs),
		bSkipped ? "true" : "false",
		szSkipReason != nullptr ? szSkipReason : "");
	std::fclose(pxFile);
}

// ============================================================================
// Infrastructure failure
//
// A BetweenTests failure is NOT a test result — the harness could not build a
// world to run the next test in, so every remaining test is unrun rather than
// failed. Reported three ways so nothing has to guess:
//   * exit code 3, which the harness header has always reserved for "harness
//     setup error" (the SetPendingExitCode seam existed with zero callers);
//   * a top-level _infrastructure.json naming the phase, the reason, and the
//     test we were about to run;
//   * a skipped record per not-yet-run test, so the runner reports one clear
//     infrastructure failure instead of a wall of MISSING lines.
// ============================================================================
static void WriteInfrastructureFailureJson(const char* szReason, const char* szBeforeTest)
{
	if (s_xRunner.m_szResultsDir == nullptr) return;

	char axPath[512];
	std::snprintf(axPath, sizeof(axPath), "%s/_infrastructure.json", s_xRunner.m_szResultsDir);

	std::FILE* pxFile = nullptr;
#ifdef _MSC_VER
	if (::fopen_s(&pxFile, axPath, "wb") != 0 || pxFile == nullptr) return;
#else
	pxFile = std::fopen(axPath, "wb");
	if (pxFile == nullptr) return;
#endif
	std::fprintf(pxFile,
		"{\n"
		"  \"infrastructureFailure\": true,\n"
		"  \"phase\": \"BetweenTests\",\n"
		"  \"reason\": \"%s\",\n"
		"  \"beforeTest\": \"%s\"\n"
		"}\n",
		szReason != nullptr ? szReason : "unknown",
		szBeforeTest != nullptr ? szBeforeTest : "");
	std::fclose(pxFile);
}

static void FailWithInfrastructureError(const char* szReason)
{
	const Zenith_AutomatedTest* pxTest = CurrentTest();
	const char* szBeforeTest = (pxTest != nullptr && pxTest->m_szName != nullptr) ? pxTest->m_szName : "(unknown)";

	std::printf("[AutomatedTest] INFRASTRUCTURE FAILURE in BetweenTests (%s) before '%s' -- "
		"the harness could not build a clean world; %d test(s) were not run.\n",
		szReason != nullptr ? szReason : "unknown",
		szBeforeTest,
		s_iOrderedCount - s_xRunner.m_iCurrentIndex);
	std::fflush(stdout);

	WriteInfrastructureFailureJson(szReason, szBeforeTest);

	// One skipped record per test from here to the end of the ordered list.
	// Counted as SKIPPED, never as passed — an unrun test must not read as a
	// green one.
	for (int i = s_xRunner.m_iCurrentIndex; i >= 0 && i < s_iOrderedCount; ++i)
	{
		const Zenith_AutomatedTestNode* pxNode = s_apxOrderedNodes[i];
		const Zenith_AutomatedTest* pxRemaining = pxNode ? pxNode->m_pxTest : nullptr;
		if (pxRemaining == nullptr) continue;
		WriteResultsJson(s_xRunner, pxRemaining, /*bPassed*/ false, /*fDurationMs*/ 0.0f,
			/*bSkipped*/ true, /*szSkipReason*/ "infrastructure");
		++s_xRunner.m_iTotalTests;
		++s_xRunner.m_iSkippedTests;
	}

	s_xRunner.m_bAnyFailures      = true;
	s_xRunner.m_iPendingExitCode  = 3;
	Zenith_Window::GetInstance()->RequestClose();
	s_xRunner.m_ePhase = HarnessPhase::Done;
}

// ============================================================================
// Slowest-tests summary (batch mode). Walks the registered-test list,
// collects (name, durationMs), insertion-sorts into a fixed-size top-N
// buffer, prints to stdout. Insertion sort is fine because we expect
// well under 1000 tests in practice.
// ============================================================================
static void PrintSlowestTestsSummary(int iTopN)
{
	if (iTopN <= 0) return;

	constexpr int kMaxN = 32;
	if (iTopN > kMaxN) iTopN = kMaxN;

	struct Entry { const char* szName; float fMs; };
	Entry axTop[kMaxN] = {};
	int   iCount = 0;

	for (const Zenith_AutomatedTestNode* p = s_pxTestListHead; p != nullptr; p = p->m_pxNext)
	{
		if (p->m_fLastDurationMs < 0.0f) continue;  // never measured (e.g. skipped before harness reset)
		const char* sz = (p->m_pxTest && p->m_pxTest->m_szName) ? p->m_pxTest->m_szName : "(unnamed)";
		const float fMs = p->m_fLastDurationMs;

		// Find insertion point in the descending-by-ms top-N.
		int iIns = iCount;
		while (iIns > 0 && axTop[iIns - 1].fMs < fMs) --iIns;

		if (iIns >= iTopN) continue;  // slower than every current entry, but the array is already full

		// Shift entries to make room.
		const int iShiftEnd = (iCount < iTopN) ? iCount : (iTopN - 1);
		for (int j = iShiftEnd; j > iIns; --j) axTop[j] = axTop[j - 1];
		axTop[iIns].szName = sz;
		axTop[iIns].fMs    = fMs;
		if (iCount < iTopN) ++iCount;
	}

	if (iCount == 0) return;

	std::printf("[AutomatedTest] Slowest %d tests:\n", iCount);
	for (int i = 0; i < iCount; ++i)
	{
		std::printf("  %7.1f ms  %s\n", static_cast<double>(axTop[i].fMs), axTop[i].szName);
	}
	std::fflush(stdout);
}

// ============================================================================
// Per-frame tick — boot-ordering state machine
// ============================================================================
bool Zenith_AutomatedTestRunner::Tick()
{
	if (!IsActive()) return false;

	switch (s_xRunner.m_ePhase)
	{
	case HarnessPhase::WaitForAutomationComplete:
	{
#ifdef ZENITH_TOOLS
		if (!g_xEngine.EditorAutomation().IsComplete()) return true;
#endif
		s_xRunner.m_ePhase = HarnessPhase::WaitForSceneLoaded;
		return true;
	}
	case HarnessPhase::WaitForSceneLoaded:
	{
		// Active scene must have a loaded SceneData with at least one OnAwake-
		// dispatched entity. The scheduler dispatches Awake at scene-load
		// completion, so checking active scene validity is a reasonable proxy
		// for "Awake done". For robustness we also wait one extra frame so
		// late-bound asset callbacks settle.
		if (!g_xEngine.Scenes().GetActiveScene().IsValid()) return true;
		s_xRunner.m_ePhase = HarnessPhase::EnterPlayingMode;
		return true;
	}
	case HarnessPhase::EnterPlayingMode:
	{
#ifdef ZENITH_TOOLS
		g_xEngine.Editor().SetEditorMode(EditorMode::Playing);
#endif
		s_xRunner.m_ePhase = HarnessPhase::FlushFirstFrameOnStart;
		return true;
	}
	case HarnessPhase::FlushFirstFrameOnStart:
	{
		// The current frame is the first one in Playing mode — OnStart fires
		// during this frame's scene update.
		//
		// Route the FIRST test through BetweenTests too, rather than straight to
		// Setup. Before this, test #1 (and every single-test run) inherited
		// whatever the boot-time unit suite and editor automation left behind,
		// while tests 2..N got a reset world — so the first test ran under
		// different conditions from all the others, and a first-position-only
		// failure was undiagnosable. The reset erases exactly that
		// contamination.
		NormalizeInputAndFixedDtForNextTest();
		s_xRunner.m_iBetweenTestsFrame = -1;
		s_xRunner.m_ePhase = HarnessPhase::BetweenTests;
		return true;
	}
	case HarnessPhase::ResetSimulatorAndCallSetup:
	{
		const Zenith_AutomatedTest* pxTest = CurrentTest();

		// Graphics skip: tests that READ PIXELS (screenshots, bitmap asserts,
		// A/B captures) opt in via m_bRequiresGraphics. A Null build rasterises
		// nothing, so there is no image for them to assert on -- skip them
		// BEFORE running Setup. Transition straight to VerifyAndExit with the
		// skip flag set; VerifyAndExit reuses its existing advance / finalise
		// path so suite tally + JSON emission stay consistent. Compile-time:
		// headless IS the Null config.
#ifdef ZENITH_NULL_RENDERER
		if (pxTest != nullptr && pxTest->m_bRequiresGraphics)
		{
			// Skipped tests still get a (near-zero) duration so the
			// JSON schema stays uniform across all rows.
			s_xRunner.m_xTestStartTime = std::chrono::high_resolution_clock::now();
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[AutomatedTest] %s: SKIPPED (requires graphics; running headless)",
				pxTest->m_szName ? pxTest->m_szName : "(unknown)");
			s_xRunner.m_bSkipCurrentTest = true;
			s_xRunner.m_iStepFrame       = 0;
			s_xRunner.m_ePhase           = HarnessPhase::VerifyAndExit;
			return true;
		}
#endif // ZENITH_NULL_RENDERER

		// Manual-only skip: tests flagged m_bManualOnly are excluded from the
		// --all-automated-tests batch (long-running balance harnesses with no
		// per-commit value -- see the field doc on Zenith_AutomatedTest). Marked
		// SKIPPED so the suite tally stays transparent; a direct
		// --automated-test <name> (m_bRunAllTests == false) still runs them in
		// full. Unlike the graphics skip this is NOT backend-gated -- the
		// exclusion applies to every batch run in every config.
		if (pxTest != nullptr
		    && pxTest->m_bManualOnly
		    && s_xRunner.m_bRunAllTests)
		{
			s_xRunner.m_xTestStartTime = std::chrono::high_resolution_clock::now();
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[AutomatedTest] %s: SKIPPED (manual-only; excluded from batch)",
				pxTest->m_szName ? pxTest->m_szName : "(unknown)");
			s_xRunner.m_bSkipCurrentTest = true;
			s_xRunner.m_iStepFrame       = 0;
			s_xRunner.m_ePhase           = HarnessPhase::VerifyAndExit;
			return true;
		}

		Zenith_InputSimulator::ResetAllInputState();
		if (s_xRunner.m_fFixedDt > 0.0f)
		{
			Zenith_InputSimulator::SetFixedDt(s_xRunner.m_fFixedDt);
		}
		// Capture start AFTER input reset / fixed-dt setup so the reported
		// duration is the test's own Setup + Step loop + Verify work, not
		// harness bookkeeping. Scene-load / BetweenTests settle frames are
		// likewise excluded (they happen in earlier phases).
		s_xRunner.m_xTestStartTime = std::chrono::high_resolution_clock::now();
		// Marked BEFORE Setup runs, not after: a Setup that calls RequestSkip
		// (or that half-installed something before deciding to skip) still has
		// state to undo, so its Teardown must fire.
		s_xRunner.m_bSetupRan = true;
		if (pxTest != nullptr && pxTest->m_pfnSetup != nullptr)
		{
			pxTest->m_pfnSetup();
		}
		// Setup may call Zenith_AutomatedTestRunner::RequestSkip() when a runtime
		// prerequisite is missing (e.g. generated game assets absent on a fresh CI
		// checkout). Finalise as SKIPPED without stepping/verifying -- same path as
		// the pre-Setup graphics / manual-only skips.
		if (s_xRunner.m_bSkipCurrentTest)
		{
			s_xRunner.m_iStepFrame = 0;
			s_xRunner.m_ePhase = HarnessPhase::VerifyAndExit;
			return true;
		}
		s_xRunner.m_iStepFrame = 0;
		s_xRunner.m_ePhase = HarnessPhase::Stepping;
		// Fall through to Stepping in the SAME tick so the first Step runs
		// before Zenith_Core::Zenith_MainLoop reaches EndOfFrameTickComplete.
		// Without this fall-through, anything Setup queues into the input
		// simulator (mouse wheel, mouse position) is wiped by the end-of-
		// frame tick before the first Step ever sees it. Tests that don't
		// rely on simulator queues are unaffected — Setup still runs once
		// and Step still runs once on this tick, just consecutively.
		[[fallthrough]];
	}
	case HarnessPhase::Stepping:
	{
		const Zenith_AutomatedTest* pxTest = CurrentTest();
		const int iMaxFrames = s_xRunner.m_iMaxFramesOverride > 0
			? s_xRunner.m_iMaxFramesOverride
			: (pxTest ? pxTest->m_iMaxFrames : 600);

		bool bKeepGoing = true;
		if (pxTest != nullptr && pxTest->m_pfnStep != nullptr)
		{
			bKeepGoing = pxTest->m_pfnStep(s_xRunner.m_iStepFrame);
		}
		++s_xRunner.m_iStepFrame;

		if (!bKeepGoing || s_xRunner.m_iStepFrame >= iMaxFrames)
		{
			s_xRunner.m_ePhase = HarnessPhase::VerifyAndExit;
		}
		return true;
	}
	case HarnessPhase::VerifyAndExit:
	{
		const Zenith_AutomatedTest* pxTest = CurrentTest();
		const bool bSkipped = s_xRunner.m_bSkipCurrentTest;
		bool bPassed = true;
		if (!bSkipped && pxTest != nullptr && pxTest->m_pfnVerify != nullptr)
		{
			bPassed = pxTest->m_pfnVerify();
		}
		// Teardown runs after the outcome is decided and before ANYTHING is
		// reported or reset: exactly once per test that reached this phase,
		// whether it passed, failed, timed out or skipped. Ordering matters --
		// running it after the world reset would hand the test a world that no
		// longer contains what it is trying to undo.
		if (s_xRunner.m_bSetupRan && pxTest != nullptr && pxTest->m_pfnTeardown != nullptr)
		{
			pxTest->m_pfnTeardown();
		}
		s_xRunner.m_bSetupRan = false;

		// Stop the wall-clock immediately after Verify so harness JSON-
		// write + stdout-print latency don't pollute the per-test number.
		const auto xEndTime = std::chrono::high_resolution_clock::now();
		const auto xDuration = xEndTime - s_xRunner.m_xTestStartTime;
		const float fDurationMs = static_cast<float>(
			std::chrono::duration_cast<std::chrono::nanoseconds>(xDuration).count() / 1.0e6);
		// Stash on the node so the batch-mode summary + external tooling
		// can surface it after every test has run.
		if (const Zenith_AutomatedTestNode* pxNode = CurrentNode())
		{
			pxNode->m_fLastDurationMs = fDurationMs;
		}
		s_xRunner.m_bVerifyPassed = bPassed;
		s_xRunner.m_bVerifyReported = true;

		WriteResultsJson(s_xRunner, pxTest, bPassed, fDurationMs, bSkipped);

		std::printf("[AutomatedTest] %s: %s (%d frames, %.1f ms)\n",
			pxTest && pxTest->m_szName ? pxTest->m_szName : "(unknown)",
			bSkipped ? "SKIPPED" : (bPassed ? "PASSED" : "FAILED"),
			s_xRunner.m_iStepFrame,
			static_cast<double>(fDurationMs));
		std::fflush(stdout);

		++s_xRunner.m_iTotalTests;
		if (bPassed)
		{
			// Skipped tests count as passed for tally purposes (non-failure);
			// the JSON's skipped:true field is what tooling reads to
			// disambiguate skipped from actually-passed.
			++s_xRunner.m_iPassedTests;
		}
		else
		{
			++s_xRunner.m_iFailedTests;
			s_xRunner.m_bAnyFailures = true;
		}

		// Advance to the next entry in the ordered execution list, if any.
		if (s_xRunner.m_iCurrentIndex >= 0
		    && s_xRunner.m_iCurrentIndex + 1 < s_iOrderedCount)
		{
			++s_xRunner.m_iCurrentIndex;
			// Normalise input + fixed-dt HERE, before the reset/settle window,
			// so the test that just finished cannot drive the next test's world.
			NormalizeInputAndFixedDtForNextTest();
			s_xRunner.m_iStepFrame         = 0;
			s_xRunner.m_iBetweenTestsFrame = -1;  // signals "run the world reset on the next BetweenTests tick"
			s_xRunner.m_bVerifyReported    = false;
			s_xRunner.m_bVerifyPassed      = false;
			s_xRunner.m_bSkipCurrentTest   = false;
			s_xRunner.m_bSetupRan          = false;
			s_xRunner.m_ePhase             = HarnessPhase::BetweenTests;
			return true;
		}

		// No more tests. Terminal hygiene BEFORE the summary: the last test's
		// residue (a save sandbox full of .zsave, a queued deferred event) has
		// no next test to leak into, but it does leak into the next PROCESS via
		// disk, and leaving it behind makes a triage dir misleading.
		RunBetweenTestsEngineHygiene();

		// Finalise exit code and request window close.
		if (IsMultiTestRun())
		{
			std::printf("[AutomatedTest] Suite summary: %d passed, %d failed (of %d)\n",
				s_xRunner.m_iPassedTests,
				s_xRunner.m_iFailedTests,
				s_xRunner.m_iTotalTests);
			std::fflush(stdout);
			// Slowest-N report: helps identify outlier tests dragging the
			// suite runtime down. Top 10 by wall-clock; emitted only after
			// every test has populated its node's m_fLastDurationMs.
			PrintSlowestTestsSummary(/*iTopN=*/10);
			s_xRunner.m_iPendingExitCode = s_xRunner.m_bAnyFailures ? 1 : 0;
		}
		else
		{
			s_xRunner.m_iPendingExitCode = bPassed ? 0 : 1;
		}

		Zenith_Window::GetInstance()->RequestClose();
		s_xRunner.m_ePhase = HarnessPhase::Done;
		return false;
	}
	case HarnessPhase::BetweenTests:
	{
		// Goal: hand the next test a world the ENGINE built, not one the
		// previous test left behind. Every test — batch member, batch-first,
		// single — routes through here.
		//
		// The order is load-bearing:
		//   1. input + fixed-dt were already normalised at the transition INTO
		//      this phase (see NormalizeInputAndFixedDtForNextTest) so the
		//      previous test's held keys cannot drive the reset/settle window.
		//   2. ResetWorldForNextTest destroys EVERY scene, INCLUDING the
		//      persistent one. That is the step that makes this a clean slate
		//      rather than a partial one: the persistent scene is where games
		//      park their singleton managers, and preserving it across tests is
		//      what used to force every game to hand-write a reset hook.
		//   3. engine hygiene for the handful of things no scene owns (the
		//      instrumentation logs, the deferred-event queue, the save sandbox
		//      on disk) + editor session hygiene under TOOLS.
		//   4. reload the boot scene, and SETTLE — the new scene's own asset
		//      loads still need frames to land, and a next-test Setup that
		//      captured half-constructed entities would fail mysteriously.
		//
		// A failure in any of that is INFRASTRUCTURE, not a test result: it is
		// reported as such and the run stops, instead of silently handing the
		// next test a broken world (which is what the old fall-through did).
		constexpr int kSettleFrames    = 8;
		constexpr int kMaxSettleFrames = 600;   // safety cap (~10 s @60Hz)

		if (s_xRunner.m_iBetweenTestsFrame < 0)
		{
			// Destroying the whole world and rebuilding it is synchronous:
			// PumpAutomatedTest runs BEFORE UpdateGameLogic, so m_bIsUpdating is
			// false here and LoadSceneByIndex completes inline. (The pre-flip
			// comments claiming this path was async were stale.)
			Zenith_SceneSystem::ResetWorldForNextTest();
			RunBetweenTestsEngineHygiene();
#ifdef ZENITH_TOOLS
			g_xEngine.Editor().ResetSessionForNextTest();
#endif

			// The return value is CHECKED. It used to be discarded, so a
			// mis-registered build index 0 produced an empty world and a
			// cascade of unexplained test failures instead of one clear error.
			const Zenith_Scene xBootScene = g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
			if (!xBootScene.IsValid())
			{
				FailWithInfrastructureError("invalid-boot-scene");
				return false;
			}
			s_xRunner.m_iBetweenTestsFrame = 0;
			return true;
		}

		++s_xRunner.m_iBetweenTestsFrame;
		if (s_xRunner.m_iBetweenTestsFrame < kSettleFrames) return true;

		// The scene load itself is synchronous, but the assets it references are
		// not necessarily resident yet, and destruction of the outgoing world
		// can leave queued work. Requiring BOTH a live active scene and a
		// drained destruction queue means a slow load waits rather than letting
		// the next test's Setup observe stale entity slots.
		Zenith_Scene xActive = g_xEngine.Scenes().GetActiveScene();
		const bool bSceneReady =
			xActive.IsValid()
			&& g_xEngine.Scenes().GetSceneData(xActive) != nullptr;
		const bool bDestructionDrained =
			!g_xEngine.Scenes().HasPendingDestructions();
		if (!bSceneReady || !bDestructionDrained)
		{
			if (s_xRunner.m_iBetweenTestsFrame < kMaxSettleFrames)
			{
				return true;
			}
			// Previously this fell through and ran the next test against a
			// half-built world.
			FailWithInfrastructureError("settle-timeout");
			return false;
		}

		// Per-game hooks still fire, at the same point they always did. They are
		// a harmless overlap now that the engine does the work — every hook body
		// is a statics clear, a generation-checked entity resolve, or a
		// null-safe forwarder, all of which are no-ops against a freshly built
		// world. Keeping them here isolates this flip's own regressions from
		// their removal, which is a separate change.
		FireBetweenTestsHooks();
		s_xRunner.m_ePhase = HarnessPhase::ResetSimulatorAndCallSetup;
		return true;
	}
	case HarnessPhase::Done:
	case HarnessPhase::Disabled:
		return false;
	}
	return false;
}

#include "Core/Zenith_AutomatedTest.Tests.inl"

#endif // ZENITH_INPUT_SIMULATOR
