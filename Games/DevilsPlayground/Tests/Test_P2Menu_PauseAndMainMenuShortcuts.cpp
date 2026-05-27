#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "EntityComponent/Zenith_SceneManager.h"

#include "Components/DPMainMenuController_Behaviour.h"
#include "Components/DPPauseMenuController_Behaviour.h"

#include <cstdio>

// ============================================================================
// Test_P2Menu_PauseAndMainMenuShortcuts (MVP-2.5.5 + MVP-2.5.6)
//
// Pins the wiring contract for two menu polish items:
//
//   MVP-2.5.5 -- pause menu R / Q shortcuts. While paused, R reloads
//                the gameplay scene (Restart) and Q loads FrontEnd
//                (Quit-to-menu). Esc keeps doing pause-toggle.
//
//   MVP-2.5.6 -- main-menu Quit button. Clicking Quit on the front-
//                end menu raises a quit request (test build: flips
//                a static flag; production polish wires
//                Zenith_Application::RequestQuit).
//
// Procedure (no scene needed; flags only):
//   1. DPMainMenuController_Behaviour::ResetQuitForTest(); confirm
//      WasQuitRequestedForTest() == false.
//   2. FireQuitClickForTest() -- direct invocation of the Quit
//      button's OnClick handler. Assert WasQuitRequestedForTest()
//      == true.
//   3. DPPauseMenuController_Behaviour::ResetPauseShortcutsForTest().
//      Confirm both shortcut flags == false.
//   4. Load GameLevel so a persistent pause-menu singleton exists.
//   5. FireRestartShortcutForTest. Assert
//      WasRestartRequestedForTest() == true.
//   6. ResetPauseShortcutsForTest, FireQuitToMenuShortcutForTest.
//      Assert WasQuitToMenuRequestedForTest() == true.
//
// What this catches:
//   * MainMenu Quit click wiring missing.
//   * Pause-menu HandleRestart never sets the flag (regression: the
//     restart-scene call moved BEFORE the flag write).
//   * Pause-menu HandleQuit ditto.
//   * Static between-tests-reset hook leaks state from a prior test
//     (the ResetForTest hook in DevilsPlayground.cpp now clears
//     these flags too).
// ============================================================================

namespace
{
	bool g_bPassed = false;
	const char* g_szFailureReason = "";
	int g_iFailureStep = 0;

	enum Phase { kPM_Start, kPM_WaitForPersistentInstance, kPM_RunAssertions, kPM_Done };
	int g_iPhase = kPM_Start;
	int g_iFrameWait = 0;
}

static void Setup_P2MenuShortcuts()
{
	g_bPassed = false;
	g_szFailureReason = "";
	g_iFailureStep = 0;
	g_iPhase = kPM_Start;
	g_iFrameWait = 0;
}

static bool Step_P2MenuShortcuts(int /*iFrame*/)
{
	auto FailAt = [](int iStep, const char* sz)
	{
		g_iFailureStep = iStep;
		g_szFailureReason = sz;
	};

	switch (g_iPhase)
	{
	case kPM_Start:
		// Step 1+2: MainMenu Quit click. No scene needed -- the
		// static handler test only touches a static flag.
		DPMainMenuController_Behaviour::ResetQuitForTest();
		if (DPMainMenuController_Behaviour::WasQuitRequestedForTest())
		{
			FailAt(1, "WasQuitRequestedForTest true after ResetQuitForTest");
			g_iPhase = kPM_Done;
			return false;
		}
		DPMainMenuController_Behaviour::FireQuitClickForTest();
		if (!DPMainMenuController_Behaviour::WasQuitRequestedForTest())
		{
			FailAt(2, "Quit click handler didn't set flag");
			g_iPhase = kPM_Done;
			return false;
		}
		// Cleanup the static flag so its mere existence doesn't leak.
		DPMainMenuController_Behaviour::ResetQuitForTest();

		// Step 3-6 require the persistent pause-menu instance. Load
		// GameLevel and wait a few frames for OnStart to migrate the
		// singleton.
		g_xEngine.SceneOperations().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kPM_WaitForPersistentInstance;
		return true;

	case kPM_WaitForPersistentInstance:
		++g_iFrameWait;
		if (DPPauseMenuController_Behaviour::GetPersistentInstanceForTest() != nullptr)
		{
			g_iPhase = kPM_RunAssertions;
		}
		else if (g_iFrameWait > 60)
		{
			FailAt(3, "persistent pause-menu instance never appeared");
			g_iPhase = kPM_Done;
			return false;
		}
		return true;

	case kPM_RunAssertions:
	{
		DPPauseMenuController_Behaviour::ResetPauseShortcutsForTest();
		if (DPPauseMenuController_Behaviour::WasRestartRequestedForTest()
			|| DPPauseMenuController_Behaviour::WasQuitToMenuRequestedForTest())
		{
			FailAt(4, "ResetPauseShortcutsForTest left flags asserted");
			g_iPhase = kPM_Done;
			return false;
		}

		DPPauseMenuController_Behaviour::FireRestartShortcutForTest();
		if (!DPPauseMenuController_Behaviour::WasRestartRequestedForTest())
		{
			FailAt(5, "Restart shortcut didn't set flag");
			g_iPhase = kPM_Done;
			return false;
		}

		DPPauseMenuController_Behaviour::ResetPauseShortcutsForTest();
		DPPauseMenuController_Behaviour::FireQuitToMenuShortcutForTest();
		if (!DPPauseMenuController_Behaviour::WasQuitToMenuRequestedForTest())
		{
			FailAt(6, "Quit-to-menu shortcut didn't set flag");
			g_iPhase = kPM_Done;
			return false;
		}

		// Cleanup so the next test doesn't see stale state.
		DPPauseMenuController_Behaviour::ResetPauseShortcutsForTest();

		g_bPassed = true;
		std::printf("[P2MenuShortcuts] all 3 wiring cases passed\n");
		std::fflush(stdout);
		g_iPhase = kPM_Done;
		return false;
	}

	case kPM_Done:
	default:
		return false;
	}
}

static bool Verify_P2MenuShortcuts()
{
	if (!g_bPassed)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2MenuShortcuts: step %d failed -- %s",
			g_iFailureStep, g_szFailureReason);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP2MenuShortcutsTest = {
	"Test_P2Menu_PauseAndMainMenuShortcuts",
	&Setup_P2MenuShortcuts,
	&Step_P2MenuShortcuts,
	&Verify_P2MenuShortcuts,
	240
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP2MenuShortcutsTest);

#endif // ZENITH_INPUT_SIMULATOR
