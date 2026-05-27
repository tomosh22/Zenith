#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_Scene.h"
#include "Input/Zenith_Input.h"
#include "Input/Zenith_InputSimulator.h"
#include "Input/Zenith_KeyCodes.h"
#include "Source/PublicInterfaces.h"
#include "Components/DPPauseMenuController_Behaviour.h"
#include "Components/DPVillager_Behaviour.h"

// ============================================================================
// Test_P1Pause_InputSimDuringPause (MVP-1.1.4)
//
// Verifies that the input simulator + pause controller continue to function
// while the gameplay scene is paused. Specifically:
//   1. Load GameLevel
//   2. Wait for DPPauseMenuController OnStart to migrate the controller to
//      the persistent scene
//   3. Simulate Esc -- gameplay scene becomes paused, IsPausedForTest()==true
//   4. While paused, simulate Esc again -- the controller's OnUpdate must
//      still fire (because it lives in the persistent scene which is not
//      paused) so it should toggle pause back off and IsPausedForTest()==false
//
// If the controller were left on the gameplay scene, its OnUpdate would be
// gated by the pause itself and the player couldn't unpause -- this test
// guards against that regression.
// ============================================================================

namespace
{
	enum Phase : int {
		kIS_Start, kIS_WaitController, kIS_PressEscToPause,
		kIS_VerifyPaused, kIS_PressEscToUnpause,
		kIS_VerifyUnpaused, kIS_Done
	};

	int  g_iPhase = kIS_Start;
	int  g_iFrameInPhase = 0;
	bool g_bSawPause   = false;
	bool g_bSawUnpause = false;
	bool g_bPassed     = false;

	DPPauseMenuController_Behaviour* FindController()
	{
		// The controller's OnStart establishes itself as the persistent
		// singleton (see DPPauseMenuController_Behaviour). Subsequent scene
		// loads update the singleton's gameplay-scene handle rather than
		// migrating new entities. So the singleton accessor is the contract:
		// whatever this returns is "the" pause controller for the current
		// gameplay scene.
		return DPPauseMenuController_Behaviour::GetPersistentInstanceForTest();
	}

	// We need at least one villager in the scene so the test waits for full
	// GameLevel load, not just for the singleton to exist (it may persist
	// across tests). Otherwise we could press Esc before the new scene's
	// own PauseManager OnStart has updated the singleton's gameplay-scene
	// handle, and the SetScenePaused call would silently target nothing.
	bool GameLevelLoadedEnough()
	{
		bool bHasVillager = false;
		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[&bHasVillager](Zenith_EntityID, DPVillager_Behaviour&)
			{
				bHasVillager = true;
			});
		return bHasVillager;
	}
}

static void Setup_P1PauseInputSim()
{
	g_iPhase = kIS_Start;
	g_iFrameInPhase = 0;
	g_bSawPause   = false;
	g_bSawUnpause = false;
	g_bPassed     = false;
}

static bool Step_P1PauseInputSim(int iFrame)
{
	switch (g_iPhase)
	{
	case kIS_Start:
		g_xEngine.SceneOperations().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kIS_WaitController;
		return true;

	case kIS_WaitController:
	{
		// Need BOTH a singleton AND the new GameLevel fully loaded -- the
		// singleton may persist from a prior test, but its gameplay-scene
		// handle gets updated only when the new GameLevel's PauseManager
		// OnStart runs.
		auto* pxCtrl = FindController();
		if (pxCtrl != nullptr && GameLevelLoadedEnough())
		{
			g_iPhase = kIS_PressEscToPause;
			g_iFrameInPhase = 0;
		}
		else if (iFrame > 120)
		{
			g_iPhase = kIS_Done;
		}
		return true;
	}

	case kIS_PressEscToPause:
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ESCAPE);
		g_iPhase = kIS_VerifyPaused;
		g_iFrameInPhase = 0;
		return true;

	case kIS_VerifyPaused:
	{
		// Give the simulated key one full frame to be consumed by the
		// controller's OnUpdate.
		++g_iFrameInPhase;
		if (g_iFrameInPhase < 2) return true;

		auto* pxCtrl = FindController();
		if (pxCtrl != nullptr)
		{
			g_bSawPause = pxCtrl->IsPausedForTest();
		}
		g_iPhase = kIS_PressEscToUnpause;
		g_iFrameInPhase = 0;
		return true;
	}

	case kIS_PressEscToUnpause:
		// Critical step: send Esc WHILE paused. If the controller were on
		// the paused gameplay scene, its OnUpdate wouldn't fire and this
		// would do nothing. With the persistent-scene migration, the
		// controller is still ticking and consumes the key.
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ESCAPE);
		g_iPhase = kIS_VerifyUnpaused;
		g_iFrameInPhase = 0;
		return true;

	case kIS_VerifyUnpaused:
	{
		++g_iFrameInPhase;
		if (g_iFrameInPhase < 2) return true;

		auto* pxCtrl = FindController();
		if (pxCtrl != nullptr)
		{
			g_bSawUnpause = !pxCtrl->IsPausedForTest();
		}
		g_bPassed = g_bSawPause && g_bSawUnpause;
		std::printf("[P1PauseInputSim] sawPause=%d sawUnpause=%d passed=%d\n",
			(int)g_bSawPause, (int)g_bSawUnpause, (int)g_bPassed);
		std::fflush(stdout);
		g_iPhase = kIS_Done;
		return false;
	}

	case kIS_Done:
	default:
		return false;
	}
}

static bool Verify_P1PauseInputSim()
{
	return g_bPassed;
}

static const Zenith_AutomatedTest g_xP1PauseInputSimTest = {
	"Test_P1Pause_InputSimDuringPause",
	&Setup_P1PauseInputSim,
	&Step_P1PauseInputSim,
	&Verify_P1PauseInputSim,
	240,
	false // m_bRequiresGraphics: input simulator + script-state read only
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP1PauseInputSimTest);

#endif // ZENITH_INPUT_SIMULATOR
