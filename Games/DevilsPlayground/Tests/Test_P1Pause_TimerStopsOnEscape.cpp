#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Core.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "Input/Zenith_InputSimulator.h"
#include "Input/Zenith_KeyCodes.h"
#include "Source/PublicInterfaces.h"
#include "Components/DPVillager_Behaviour.h"

// ============================================================================
// Test_P1Pause_TimerStopsOnEscape (MVP-1.1.1)
//
// Possess a villager, drive a few frames of OnUpdate to confirm the life
// timer ticks down, then press Esc and assert the timer FREEZES across the
// next N frames. Press Esc again and assert the timer resumes.
//
// This exercises the contract added in MVP-1.1.2: DPPauseMenuController
// calls g_xEngine.SceneRegistry().SetScenePaused on the gameplay scene, which
// gates entity OnUpdates -- including DPVillager::TickLife.
// ============================================================================

namespace
{
	enum Phase : int {
		kP_Start, kP_WaitScene, kP_PossessAndBaseline,
		kP_RunUnpaused, kP_PressEsc, kP_PauseSettleFrame, kP_RunPaused,
		kP_PressEscAgain, kP_UnpauseSettleFrame, kP_RunUnpausedAgain,
		kP_Verify, kP_Done
	};

	int             g_iPhase = kP_Start;
	int             g_iFrameInPhase = 0;
	Zenith_EntityID g_xVillager;
	float           g_fLifeAfterUnpausedRun = 0.0f;
	float           g_fLifeBeforePause      = 0.0f;
	float           g_fLifeAfterPauseRun    = 0.0f;
	float           g_fLifeAfterResume      = 0.0f;
	bool            g_bPassed               = false;

	constexpr int kUNPAUSED_FRAMES = 30;
	constexpr int kPAUSED_FRAMES   = 30;
	constexpr int kRESUME_FRAMES   = 30;

	float ReadLife()
	{
		float fLife = -1.0f;
		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[&fLife](Zenith_EntityID, DPVillager_Behaviour& xV)
			{
				if (fLife < 0.0f) fLife = xV.GetRemainingLife();
			});
		return fLife;
	}
}

static void Setup_P1PauseTimer()
{
	g_iPhase = kP_Start;
	g_iFrameInPhase = 0;
	g_xVillager = INVALID_ENTITY_ID;
	g_fLifeAfterUnpausedRun = 0.0f;
	g_fLifeBeforePause      = 0.0f;
	g_fLifeAfterPauseRun    = 0.0f;
	g_fLifeAfterResume      = 0.0f;
	g_bPassed = false;
}

static bool Step_P1PauseTimer(int iFrame)
{
	switch (g_iPhase)
	{
	case kP_Start:
		g_xEngine.SceneOperations().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kP_WaitScene;
		g_iFrameInPhase = 0;
		return true;

	case kP_WaitScene:
	{
		Zenith_EntityID xFound;
		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[&xFound](Zenith_EntityID xId, DPVillager_Behaviour&)
			{
				if (!xFound.IsValid()) xFound = xId;
			});
		if (xFound.IsValid())
		{
			g_xVillager = xFound;
			DP_Player::SetPossessedVillager(g_xVillager);
			g_iPhase = kP_PossessAndBaseline;
			g_iFrameInPhase = 0;
		}
		else if (iFrame > 120)
		{
			g_iPhase = kP_Done;
		}
		return true;
	}

	case kP_PossessAndBaseline:
		// Possession bumps remaining life back to max on the next OnUpdate.
		// Settle one frame, then enter the unpaused tick window.
		g_iPhase = kP_RunUnpaused;
		g_iFrameInPhase = 0;
		return true;

	case kP_RunUnpaused:
		++g_iFrameInPhase;
		if (g_iFrameInPhase >= kUNPAUSED_FRAMES)
		{
			g_fLifeAfterUnpausedRun = ReadLife();
			g_iPhase = kP_PressEsc;
			g_iFrameInPhase = 0;
		}
		return true;

	case kP_PressEsc:
		// Simulate Esc to toggle pause. Note: the pause controller migrated
		// itself to the persistent scene during OnStart, so its OnUpdate
		// still fires (otherwise pause couldn't be toggled by the player).
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ESCAPE);
		g_iPhase = kP_PauseSettleFrame;
		return true;

	case kP_PauseSettleFrame:
		// One settle frame: the controller's OnUpdate runs THIS frame and
		// flips m_bIsPaused on the gameplay scene, but per-scene update
		// dispatch already collected the villager for ticking THIS frame
		// (so one stray TickLife may still fire). Capture the baseline
		// AFTER that stray tick so the paused window starts from a stable
		// "no further ticks will fire" point.
		g_fLifeBeforePause = ReadLife();
		g_iPhase = kP_RunPaused;
		g_iFrameInPhase = 0;
		return true;

	case kP_RunPaused:
		++g_iFrameInPhase;
		if (g_iFrameInPhase >= kPAUSED_FRAMES)
		{
			g_fLifeAfterPauseRun = ReadLife();
			g_iPhase = kP_PressEscAgain;
			g_iFrameInPhase = 0;
		}
		return true;

	case kP_PressEscAgain:
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ESCAPE);
		g_iPhase = kP_UnpauseSettleFrame;
		return true;

	case kP_UnpauseSettleFrame:
		// Symmetric settle frame on unpause: the first frame after Esc may
		// or may not tick the villager depending on dispatch order, so let
		// it pass before timing the resumed window.
		g_iPhase = kP_RunUnpausedAgain;
		g_iFrameInPhase = 0;
		return true;

	case kP_RunUnpausedAgain:
		++g_iFrameInPhase;
		if (g_iFrameInPhase >= kRESUME_FRAMES)
		{
			g_fLifeAfterResume = ReadLife();
			g_iPhase = kP_Verify;
		}
		return true;

	case kP_Verify:
	{
		// What we expect:
		//  - During the unpaused window: life ticks down.
		//  - During the paused window:   life is flat (no OnUpdate ran).
		//  - During the resume window:   life ticks down again.
		const float fPausedDelta = g_fLifeBeforePause - g_fLifeAfterPauseRun;
		const float fResumeDelta = g_fLifeAfterPauseRun - g_fLifeAfterResume;

		// Tolerance on the paused window: 0.01s slack around zero handles
		// the boundary frame where Esc is consumed -- the OnUpdate that
		// observes the Esc could still tick TickLife depending on dispatch
		// order between the pause controller (persistent scene) and the
		// villager (gameplay scene). Both observed orderings are valid.
		const bool bPausedFlat   = (fPausedDelta < 0.01f) && (fPausedDelta > -0.01f);
		const bool bResumedTicks = fResumeDelta > 0.0f;
		const bool bUnpausedSane = g_fLifeAfterUnpausedRun > 0.0f;

		g_bPassed = bPausedFlat && bResumedTicks && bUnpausedSane;

		std::printf("[P1PauseTimer] unpausedEnd=%.3f beforePause=%.3f afterPauseRun=%.3f afterResume=%.3f "
		            "pausedDelta=%.4f resumeDelta=%.4f passed=%d\n",
			g_fLifeAfterUnpausedRun, g_fLifeBeforePause, g_fLifeAfterPauseRun, g_fLifeAfterResume,
			fPausedDelta, fResumeDelta, (int)g_bPassed);
		std::fflush(stdout);
		g_iPhase = kP_Done;
		return false;
	}

	case kP_Done:
	default:
		return false;
	}
}

static bool Verify_P1PauseTimer()
{
	return g_bPassed && g_xVillager.IsValid();
}

static const Zenith_AutomatedTest g_xP1PauseTimerTest = {
	"Test_P1Pause_TimerStopsOnEscape",
	&Setup_P1PauseTimer,
	&Step_P1PauseTimer,
	&Verify_P1PauseTimer,
	300,
	false // m_bRequiresGraphics: pure scene-update test
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP1PauseTimerTest);

#endif // ZENITH_INPUT_SIMULATOR
