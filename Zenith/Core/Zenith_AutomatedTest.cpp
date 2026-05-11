#include "Zenith.h"
// Needed for the clean-shutdown path on early-exit branches (list,
// not-found, no-tests-registered). Zenith_Window.h is platform-routed by
// Zenith.h so the include is portable between win64 and Android.
#include "Core/Zenith_Core.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "Input/Zenith_InputSimulator.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "FileAccess/Zenith_FileAccess.h"

#ifdef ZENITH_TOOLS
#include "Editor/Zenith_Editor.h"
#include "Editor/Zenith_EditorAutomation.h"
#endif

#include <cstring>
#include <cstdlib>
#include <cstdio>

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
	for (int i = 0; i < s_iBetweenTestsHookCount; ++i)
	{
		if (s_apfnBetweenTestsHooks[i] != nullptr)
		{
			s_apfnBetweenTestsHooks[i]();
		}
	}
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
		const Zenith_AutomatedTestNode* m_pxCurrentNode      = nullptr;
		const char*                     m_szRequestedName    = nullptr;
		const char*                     m_szResultsPath      = nullptr;
		const char*                     m_szResultsDir       = nullptr;
		int                             m_iStepFrame         = 0;
		int                             m_iMaxFramesOverride = -1;          // set via --exit-after-frames
		float                           m_fFixedDt           = -1.0f;       // set via --fixed-dt
		int                             m_iPendingExitCode   = 0;
		bool                            m_bListThenExit      = false;
		bool                            m_bRunAllTests       = false;
		bool                            m_bAnyFailures       = false;
		// For VerifyAndExit serialization — populated lazily.
		bool                            m_bVerifyReported    = false;
		bool                            m_bVerifyPassed      = false;
		// Batch-mode tally
		int                             m_iTotalTests        = 0;
		int                             m_iPassedTests       = 0;
		int                             m_iFailedTests       = 0;
		// BetweenTests sub-state: counts frames since the boot-scene reload
		// was triggered. -1 = scene reload not yet triggered for this gap.
		int                             m_iBetweenTestsFrame = -1;
	};
	RunnerState s_xRunner;

	const Zenith_AutomatedTest* CurrentTest()
	{
		return s_xRunner.m_pxCurrentNode ? s_xRunner.m_pxCurrentNode->m_pxTest : nullptr;
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

void Zenith_AutomatedTestRunner::ParseCommandLine(int argc, char** argv)
{
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
		else if (std::strcmp(szArg, "--all-automated-tests") == 0)
		{
			s_xRunner.m_bRunAllTests = true;
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
		// Clean shutdown instead of std::exit so GPU/Jolt/audio resources
		// release in the normal order. ParseCommandLine runs AFTER
		// Zenith_Init, so we have a fully-initialised engine to tear down.
		// Without this, --list-automated-tests would leak VRAM allocations,
		// Jolt's body-interface lock, and the GLFW window — usually
		// observable only as a noisy exit code from the Vulkan validation
		// layer, but bad hygiene in any case.
		Zenith_Core::Zenith_FullShutdown();
		std::exit(0);
	}

	// --all-automated-tests takes precedence: run every registered test in
	// sequence inside the same process (avoids per-test boot of ~20s).
	if (s_xRunner.m_bRunAllTests)
	{
		s_xRunner.m_pxCurrentNode = s_pxTestListHead;
		if (s_xRunner.m_pxCurrentNode == nullptr)
		{
			std::printf("ERROR: --all-automated-tests requested but no tests are registered.\n");
			std::fflush(stdout);
			s_xRunner.m_iPendingExitCode = 2;
			// Use the canonical full-shutdown wrapper so adding a new
			// init-only singleton doesn't silently rot these branches.
			Zenith_Core::Zenith_FullShutdown();
			std::exit(2);
		}
		Zenith_InputSimulator::Enable();
		s_xRunner.m_ePhase = HarnessPhase::WaitForAutomationComplete;
		return;
	}

	if (s_xRunner.m_szRequestedName != nullptr)
	{
		s_xRunner.m_pxCurrentNode = FindNodeByName(s_xRunner.m_szRequestedName);
		if (s_xRunner.m_pxCurrentNode == nullptr)
		{
			std::printf("ERROR: --automated-test '%s' not found in registry. "
				"Run with --list-automated-tests for the full list.\n",
				s_xRunner.m_szRequestedName);
			std::fflush(stdout);
			s_xRunner.m_iPendingExitCode = 2;
			// Use the canonical full-shutdown wrapper so adding a new
			// init-only singleton doesn't silently rot these branches.
			Zenith_Core::Zenith_FullShutdown();
			std::exit(2);
		}
		Zenith_InputSimulator::Enable();
		s_xRunner.m_ePhase = HarnessPhase::WaitForAutomationComplete;
	}
}

// ============================================================================
// JSON results writer (hand-rolled to avoid a library dependency)
// ============================================================================
static void WriteResultsJson(const RunnerState& xRunner,
                             const Zenith_AutomatedTest* pxTest,
                             bool bPassed)
{
	// Resolve the output path. In batch mode prefer m_szResultsDir/<name>.json;
	// in single mode use m_szResultsPath verbatim. If neither is set, skip.
	char axPath[512];
	const char* szPath = nullptr;

	if (xRunner.m_bRunAllTests && xRunner.m_szResultsDir != nullptr
	    && pxTest != nullptr && pxTest->m_szName != nullptr)
	{
		std::snprintf(axPath, sizeof(axPath), "%s/%s.json",
			xRunner.m_szResultsDir, pxTest->m_szName);
		szPath = axPath;
	}
	else if (!xRunner.m_bRunAllTests && xRunner.m_szResultsPath != nullptr)
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
		"  \"failures\": []\n"
		"}\n",
		szName,
		bPassed ? "true" : "false",
		xRunner.m_iStepFrame);
	std::fclose(pxFile);
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
		if (!Zenith_EditorAutomation::IsComplete()) return true;
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
		if (!Zenith_SceneManager::GetActiveScene().IsValid()) return true;
		s_xRunner.m_ePhase = HarnessPhase::EnterPlayingMode;
		return true;
	}
	case HarnessPhase::EnterPlayingMode:
	{
#ifdef ZENITH_TOOLS
		Zenith_Editor::SetEditorMode(EditorMode::Playing);
#endif
		s_xRunner.m_ePhase = HarnessPhase::FlushFirstFrameOnStart;
		return true;
	}
	case HarnessPhase::FlushFirstFrameOnStart:
	{
		// The current frame is the first one in Playing mode — OnStart fires
		// during this frame's scene update. Move on next frame so Setup runs
		// against fully-started entities.
		s_xRunner.m_ePhase = HarnessPhase::ResetSimulatorAndCallSetup;
		return true;
	}
	case HarnessPhase::ResetSimulatorAndCallSetup:
	{
		Zenith_InputSimulator::ResetAllInputState();
		if (s_xRunner.m_fFixedDt > 0.0f)
		{
			Zenith_InputSimulator::SetFixedDt(s_xRunner.m_fFixedDt);
		}
		const Zenith_AutomatedTest* pxTest = CurrentTest();
		if (pxTest != nullptr && pxTest->m_pfnSetup != nullptr)
		{
			pxTest->m_pfnSetup();
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
		bool bPassed = true;
		if (pxTest != nullptr && pxTest->m_pfnVerify != nullptr)
		{
			bPassed = pxTest->m_pfnVerify();
		}
		s_xRunner.m_bVerifyPassed = bPassed;
		s_xRunner.m_bVerifyReported = true;

		WriteResultsJson(s_xRunner, pxTest, bPassed);

		std::printf("[AutomatedTest] %s: %s (%d frames)\n",
			pxTest && pxTest->m_szName ? pxTest->m_szName : "(unknown)",
			bPassed ? "PASSED" : "FAILED",
			s_xRunner.m_iStepFrame);
		std::fflush(stdout);

		++s_xRunner.m_iTotalTests;
		if (bPassed)
		{
			++s_xRunner.m_iPassedTests;
		}
		else
		{
			++s_xRunner.m_iFailedTests;
			s_xRunner.m_bAnyFailures = true;
		}

		// Batch mode: advance to the next test if there is one.
		if (s_xRunner.m_bRunAllTests
		    && s_xRunner.m_pxCurrentNode != nullptr
		    && s_xRunner.m_pxCurrentNode->m_pxNext != nullptr)
		{
			s_xRunner.m_pxCurrentNode = s_xRunner.m_pxCurrentNode->m_pxNext;
			s_xRunner.m_iStepFrame         = 0;
			s_xRunner.m_iBetweenTestsFrame = -1;  // signals "trigger reload on next BetweenTests tick"
			s_xRunner.m_bVerifyReported    = false;
			s_xRunner.m_bVerifyPassed      = false;
			s_xRunner.m_ePhase             = HarnessPhase::BetweenTests;
			return true;
		}

		// No more tests — finalise exit code and request window close.
		if (s_xRunner.m_bRunAllTests)
		{
			std::printf("[AutomatedTest] Suite summary: %d passed, %d failed (of %d)\n",
				s_xRunner.m_iPassedTests,
				s_xRunner.m_iFailedTests,
				s_xRunner.m_iTotalTests);
			std::fflush(stdout);
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
		// Goal: get the engine back to a known clean state before the next
		// test's Setup runs. Two distinct sources of leakage to address:
		//   1. Scene-managed side-tables (DP_Items::g_xItemTagTable,
		//      DP_Fog::g_xFogHoles, …) which clear themselves when the
		//      owning entity's OnDestroy fires. Solved by force-loading
		//      the boot scene (build-index 0) in SCENE_LOAD_SINGLE mode —
		//      every prior-test entity gets destroyed and unregistered.
		//   2. Per-game persistent globals (DP_Player::g_xPossessedVillager,
		//      DP_Win::g_uCollectedObjectivesMask, …) which are NOT tied to
		//      entity lifetime. Solved by firing each registered
		//      BetweenTestsHook AFTER the scene reload settles.
		// The scene reload is async (queue-and-defer), so we wait at least
		// kSettleFrames frames AND require the new active scene to be
		// fully loaded before advancing. The frame budget on its own can
		// fall through with the scene only partially populated when the
		// async loader is slowed by asset I/O — the next test's Setup
		// would then capture half-constructed entity references.
		constexpr int kSettleFrames    = 8;
		constexpr int kMaxSettleFrames = 600;   // safety cap (~10 s @60Hz)

		if (s_xRunner.m_iBetweenTestsFrame < 0)
		{
			Zenith_SceneManager::LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
			s_xRunner.m_iBetweenTestsFrame = 0;
			return true;
		}

		++s_xRunner.m_iBetweenTestsFrame;
		if (s_xRunner.m_iBetweenTestsFrame < kSettleFrames) return true;

		// IsValid() check on the active scene's handle: the boot-scene
		// reload above is deferred via the scene-load queue, so the
		// "active scene" only flips when the loader has actually
		// finished. Without this check, a slow load (cold-disk asset
		// fetch, big mesh re-import) could leave the next test's
		// Setup querying a half-populated scene.
		Zenith_Scene xActive = Zenith_SceneManager::GetActiveScene();
		const bool bSceneReady =
			xActive.IsValid()
			&& Zenith_SceneManager::GetSceneData(xActive) != nullptr;
		if (!bSceneReady && s_xRunner.m_iBetweenTestsFrame < kMaxSettleFrames)
		{
			return true;
		}

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

#endif // ZENITH_INPUT_SIMULATOR
