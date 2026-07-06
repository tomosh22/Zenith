#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

/**
 * Test_RunnerCharacterization - characterization test for the wave-2 graph
 * conversion of Runner's scoring / game-over flow.
 *
 * Written against the C++ version FIRST (the score accumulation on collectible
 * pickup and the obstacle-hit / character-dead -> GAME_OVER decisions inside
 * Runner_GameComponent::OnUpdate's PLAYING branch); the graph version must
 * keep it green unchanged. Probes go through read-only accessors that survive
 * the conversion.
 *
 *   Runner_GameOverFlow_Test - the character auto-runs with no evasive input;
 *                              spawned obstacles eventually end the run, the
 *                              state flips to GAME_OVER, and the high score
 *                              equals the final score (first run, score <=
 *                              high). Score stays monotonic along the way.
 *
 * W1 additions (written against the C++ key/menu/character decisions FIRST,
 * before those decisions moved into the graphs):
 *
 *   Runner_PauseResume_Test      - P freezes the run (distance frozen), P
 *                                  again resumes it.
 *   Runner_ResetFlow_Test        - R rebuilds the run (distance drops, speed
 *                                  back to base, PLAYING again).
 *   Runner_EscapeMenu_Test       - Esc during the run returns straight to the
 *                                  main menu (no pause quirk in Runner).
 *   Runner_MenuPlay_Test         - the single Play button holds focus;
 *                                  activating it starts the run.
 *   Runner_ScoringHighScore_Test - injected pickups accumulate; R syncs the
 *                                  high score before resetting the score.
 *   Runner_CharacterActions_Test - lane switching (with clamping at the outer
 *                                  lanes), jump (+JUMP anim), slide (+SLIDE
 *                                  anim, ~0.8 s duration - the slide-timer
 *                                  countdown), and the no-jump-while-sliding
 *                                  gate; death-tolerant (R + retry the block).
 */

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "Input/Zenith_InputSimulator.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "UI/Zenith_UIButton.h"
#include "Components/Runner_GameComponent.h"

#include <cmath>

namespace
{
	Runner_GameComponent* FindRunnerGame()
	{
		Runner_GameComponent* pxFound = nullptr;
		g_xEngine.Scenes().QueryAllScenes<Runner_GameComponent>().ForEach(
			[&pxFound](Zenith_EntityID, Runner_GameComponent& xGame)
			{
				if (pxFound == nullptr) pxFound = &xGame;
			});
		return pxFound;
	}

	enum class RunPhase { Boot, WaitPlaying, Run, Done };

	RunPhase g_eRunPhase = RunPhase::Boot;
	int      g_iRunFrame = 0;
	bool     g_bSawGameOver = false;
	bool     g_bScoreMonotonic = true;
	uint32_t g_uLastScore = 0;
	uint32_t g_uFinalScore = 0;
	uint32_t g_uFinalHighScore = 0;
}

static void Setup_GameOverFlow()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_eRunPhase = RunPhase::Boot;
	g_iRunFrame = 0;
	g_bSawGameOver = false;
	g_bScoreMonotonic = true;
	g_uLastScore = 0;
	g_uFinalScore = 0;
	g_uFinalHighScore = 0;
}

static bool Step_GameOverFlow(int iFrame)
{
	switch (g_eRunPhase)
	{
	case RunPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_eRunPhase = RunPhase::WaitPlaying;
		return true;

	case RunPhase::WaitPlaying:
	{
		Runner_GameComponent* pxGame = FindRunnerGame();
		if (pxGame != nullptr && pxGame->GetGameState() == RunnerGameState::PLAYING)
		{
			g_eRunPhase = RunPhase::Run;
			g_iRunFrame = 0;
			return true;
		}
		return iFrame < 600;
	}

	case RunPhase::Run:
	{
		Runner_GameComponent* pxGame = FindRunnerGame();
		if (pxGame == nullptr) return false;

		const uint32_t uScore = pxGame->GetScore();
		if (uScore < g_uLastScore)
		{
			g_bScoreMonotonic = false;
		}
		g_uLastScore = uScore;

		if (pxGame->GetGameState() == RunnerGameState::GAME_OVER)
		{
			g_bSawGameOver = true;
			g_uFinalScore = pxGame->GetScore();
			g_uFinalHighScore = pxGame->GetHighScore();
			g_eRunPhase = RunPhase::Done;
			return false;
		}
		// No evasive input - the auto-runner hits an obstacle (or dies) on its
		// own. Two minutes of game time is far beyond the expected survival.
		return ++g_iRunFrame < 7200;
	}

	case RunPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_GameOverFlow()
{
	Zenith_InputSimulator::ClearFixedDt();
	if (!g_bSawGameOver)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[RunnerGameOver] never reached GAME_OVER");
		return false;
	}
	if (!g_bScoreMonotonic)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[RunnerGameOver] score decreased mid-run");
		return false;
	}
	// First run of the process: the high score must have absorbed the final
	// score (the obstacle-hit path syncs it).
	if (g_uFinalHighScore < g_uFinalScore)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[RunnerGameOver] high score %u < final score %u",
			g_uFinalHighScore, g_uFinalScore);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xRunnerGameOverFlowTest = {
	"Runner_GameOverFlow_Test",
	&Setup_GameOverFlow,
	&Step_GameOverFlow,
	&Verify_GameOverFlow,
	/*maxFrames*/ 9000,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xRunnerGameOverFlowTest);

// ============================================================================
// Runner_PauseResume_Test
// ============================================================================

namespace
{
	enum class RPausePhase { Boot, WaitPlaying, Settle, PressPause, AwaitPaused, HoldPaused, PressResume, AwaitResumed, RunResumed, Done };

	RPausePhase g_eRPausePhase = RPausePhase::Boot;
	int         g_iRPauseFrame = 0;
	float       g_fRDistAtPause = 0.0f;
	bool        g_bRSawPaused = false;
	bool        g_bRDistFroze = false;
	bool        g_bRSawResumed = false;
	bool        g_bRDistRan = false;
}

static void Setup_RunnerPauseResume()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_eRPausePhase = RPausePhase::Boot;
	g_iRPauseFrame = 0;
	g_fRDistAtPause = 0.0f;
	g_bRSawPaused = false;
	g_bRDistFroze = false;
	g_bRSawResumed = false;
	g_bRDistRan = false;
}

static bool Step_RunnerPauseResume(int iFrame)
{
	Runner_GameComponent* pxGame = FindRunnerGame();
	switch (g_eRPausePhase)
	{
	case RPausePhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_eRPausePhase = RPausePhase::WaitPlaying;
		return true;

	case RPausePhase::WaitPlaying:
		if (pxGame != nullptr && pxGame->GetGameState() == RunnerGameState::PLAYING)
		{
			g_eRPausePhase = RPausePhase::Settle;
			g_iRPauseFrame = 0;
		}
		return iFrame < 600;

	case RPausePhase::Settle:
		if (++g_iRPauseFrame < 30)
		{
			return true;
		}
		g_eRPausePhase = RPausePhase::PressPause;
		return true;

	case RPausePhase::PressPause:
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_P);
		g_eRPausePhase = RPausePhase::AwaitPaused;
		g_iRPauseFrame = 0;
		return true;

	case RPausePhase::AwaitPaused:
		if (pxGame == nullptr) return false;
		if (pxGame->GetGameState() == RunnerGameState::PAUSED)
		{
			g_bRSawPaused = true;
			g_fRDistAtPause = Runner_CharacterController::GetDistanceTraveled();
			g_eRPausePhase = RPausePhase::HoldPaused;
			g_iRPauseFrame = 0;
			return true;
		}
		return ++g_iRPauseFrame < 10;

	case RPausePhase::HoldPaused:
		if (++g_iRPauseFrame < 120)
		{
			return true;
		}
		g_bRDistFroze = std::fabs(Runner_CharacterController::GetDistanceTraveled() - g_fRDistAtPause) < 0.1f;
		if (!g_bRDistFroze)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[RunnerPause] distance moved while paused: %.2f -> %.2f",
				g_fRDistAtPause, Runner_CharacterController::GetDistanceTraveled());
			return false;
		}
		g_eRPausePhase = RPausePhase::PressResume;
		return true;

	case RPausePhase::PressResume:
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_P);
		g_eRPausePhase = RPausePhase::AwaitResumed;
		g_iRPauseFrame = 0;
		return true;

	case RPausePhase::AwaitResumed:
		if (pxGame == nullptr) return false;
		if (pxGame->GetGameState() == RunnerGameState::PLAYING)
		{
			g_bRSawResumed = true;
			g_fRDistAtPause = Runner_CharacterController::GetDistanceTraveled();
			g_eRPausePhase = RPausePhase::RunResumed;
			g_iRPauseFrame = 0;
			return true;
		}
		return ++g_iRPauseFrame < 10;

	case RPausePhase::RunResumed:
		if (++g_iRPauseFrame < 60)
		{
			return true;
		}
		// 1 s at >= base speed 15: several metres of progress.
		g_bRDistRan = Runner_CharacterController::GetDistanceTraveled() > g_fRDistAtPause + 1.0f;
		g_eRPausePhase = RPausePhase::Done;
		return false;

	case RPausePhase::Done:
		return false;
	}
	return false;
}

static bool Verify_RunnerPauseResume()
{
	Zenith_InputSimulator::ClearFixedDt();
	if (!g_bRSawPaused)   { Zenith_Log(LOG_CATEGORY_UNITTEST, "[RunnerPause] P never paused"); }
	if (!g_bRDistFroze)   { Zenith_Log(LOG_CATEGORY_UNITTEST, "[RunnerPause] run not frozen while paused"); }
	if (!g_bRSawResumed)  { Zenith_Log(LOG_CATEGORY_UNITTEST, "[RunnerPause] P never resumed"); }
	if (!g_bRDistRan)     { Zenith_Log(LOG_CATEGORY_UNITTEST, "[RunnerPause] run not moving after resume"); }
	return g_bRSawPaused && g_bRDistFroze && g_bRSawResumed && g_bRDistRan;
}

static const Zenith_AutomatedTest g_xRunnerPauseResumeTest = {
	"Runner_PauseResume_Test",
	&Setup_RunnerPauseResume,
	&Step_RunnerPauseResume,
	&Verify_RunnerPauseResume,
	/*maxFrames*/ 1500,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xRunnerPauseResumeTest);

// ============================================================================
// Runner_ResetFlow_Test
// ============================================================================

namespace
{
	enum class RResetPhase { Boot, WaitPlaying, RunDown, PressReset, AwaitReset, Done };

	RResetPhase g_eRResetPhase = RResetPhase::Boot;
	int         g_iRResetFrame = 0;
	float       g_fRDistBeforeReset = 0.0f;
	bool        g_bRResetOk = false;
}

static void Setup_RunnerResetFlow()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_eRResetPhase = RResetPhase::Boot;
	g_iRResetFrame = 0;
	g_fRDistBeforeReset = 0.0f;
	g_bRResetOk = false;
}

static bool Step_RunnerResetFlow(int iFrame)
{
	Runner_GameComponent* pxGame = FindRunnerGame();
	switch (g_eRResetPhase)
	{
	case RResetPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_eRResetPhase = RResetPhase::WaitPlaying;
		return true;

	case RResetPhase::WaitPlaying:
		if (pxGame != nullptr && pxGame->GetGameState() == RunnerGameState::PLAYING)
		{
			g_eRResetPhase = RResetPhase::RunDown;
			g_iRResetFrame = 0;
		}
		return iFrame < 600;

	case RResetPhase::RunDown:
		// 5 s of running (a natural death along the way is fine - R works
		// from GAME_OVER with the same reset semantics).
		if (++g_iRResetFrame < 300)
		{
			return true;
		}
		g_eRResetPhase = RResetPhase::PressReset;
		return true;

	case RResetPhase::PressReset:
		g_fRDistBeforeReset = Runner_CharacterController::GetDistanceTraveled();
		if (g_fRDistBeforeReset < 20.0f)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[RunnerReset] run never progressed (%.2f m)", g_fRDistBeforeReset);
			return false;
		}
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_R);
		g_eRResetPhase = RResetPhase::AwaitReset;
		g_iRResetFrame = 0;
		return true;

	case RResetPhase::AwaitReset:
		if (pxGame == nullptr) return false;
		if (Runner_CharacterController::GetDistanceTraveled() < g_fRDistBeforeReset - 10.0f)
		{
			// Fresh run: distance restarted, speed back at base, PLAYING.
			g_bRResetOk = pxGame->GetGameState() == RunnerGameState::PLAYING
				&& Runner_CharacterController::GetCurrentSpeed() < 16.0f;
			if (!g_bRResetOk)
			{
				Zenith_Log(LOG_CATEGORY_UNITTEST, "[RunnerReset] post-reset wrong: state %d speed %.2f",
					static_cast<int>(pxGame->GetGameState()), Runner_CharacterController::GetCurrentSpeed());
			}
			g_eRResetPhase = RResetPhase::Done;
			return false;
		}
		return ++g_iRResetFrame < 30;

	case RResetPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_RunnerResetFlow()
{
	Zenith_InputSimulator::ClearFixedDt();
	if (!g_bRResetOk)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[RunnerReset] R did not rebuild a fresh PLAYING run");
	}
	return g_bRResetOk;
}

static const Zenith_AutomatedTest g_xRunnerResetFlowTest = {
	"Runner_ResetFlow_Test",
	&Setup_RunnerResetFlow,
	&Step_RunnerResetFlow,
	&Verify_RunnerResetFlow,
	/*maxFrames*/ 1500,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xRunnerResetFlowTest);

// ============================================================================
// Runner_EscapeMenu_Test
// ============================================================================

namespace
{
	enum class REscPhase { Boot, WaitPlaying, PressEsc, AwaitMenu, Done };

	REscPhase g_eREscPhase = REscPhase::Boot;
	int       g_iREscFrame = 0;
	bool      g_bREscReachedMenu = false;
}

static void Setup_RunnerEscapeMenu()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_eREscPhase = REscPhase::Boot;
	g_iREscFrame = 0;
	g_bREscReachedMenu = false;
}

static bool Step_RunnerEscapeMenu(int iFrame)
{
	Runner_GameComponent* pxGame = FindRunnerGame();
	switch (g_eREscPhase)
	{
	case REscPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_eREscPhase = REscPhase::WaitPlaying;
		return true;

	case REscPhase::WaitPlaying:
		if (pxGame != nullptr && pxGame->GetGameState() == RunnerGameState::PLAYING)
		{
			g_eREscPhase = REscPhase::PressEsc;
			g_iREscFrame = 0;
		}
		return iFrame < 600;

	case REscPhase::PressEsc:
		if (++g_iREscFrame < 30)
		{
			return true;
		}
		// Unlike Marble there is NO pause-on-Esc quirk: Esc during the run
		// goes straight back to the menu.
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ESCAPE);
		g_eREscPhase = REscPhase::AwaitMenu;
		g_iREscFrame = 0;
		return true;

	case REscPhase::AwaitMenu:
		if (pxGame != nullptr && pxGame->GetGameState() == RunnerGameState::MAIN_MENU)
		{
			g_bREscReachedMenu = true;
			g_eREscPhase = REscPhase::Done;
			return false;
		}
		return ++g_iREscFrame < 600;

	case REscPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_RunnerEscapeMenu()
{
	Zenith_InputSimulator::ClearFixedDt();
	if (!g_bREscReachedMenu)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[RunnerEsc] Esc during the run did not return to the main menu");
	}
	return g_bREscReachedMenu;
}

static const Zenith_AutomatedTest g_xRunnerEscapeMenuTest = {
	"Runner_EscapeMenu_Test",
	&Setup_RunnerEscapeMenu,
	&Step_RunnerEscapeMenu,
	&Verify_RunnerEscapeMenu,
	/*maxFrames*/ 1500,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xRunnerEscapeMenuTest);

// ============================================================================
// Runner_MenuPlay_Test
// ============================================================================

namespace
{
	Zenith_UI::Zenith_UIButton* FindRunnerPlayButton()
	{
		Zenith_UI::Zenith_UIButton* pxFound = nullptr;
		g_xEngine.Scenes().QueryAllScenes<Zenith_UIComponent>().ForEach(
			[&pxFound](Zenith_EntityID, Zenith_UIComponent& xUI)
			{
				if (pxFound == nullptr)
				{
					pxFound = xUI.FindElement<Zenith_UI::Zenith_UIButton>("MenuPlay");
				}
			});
		return pxFound;
	}

	enum class RMenuPhase { Boot, WaitMenu, CheckFocus, ActivatePlay, AwaitPlaying, Done };

	RMenuPhase g_eRMenuPhase = RMenuPhase::Boot;
	int        g_iRMenuFrame = 0;
	bool       g_bRPlayFocused = false;
	bool       g_bRPlayStarted = false;
}

static void Setup_RunnerMenuPlay()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_eRMenuPhase = RMenuPhase::Boot;
	g_iRMenuFrame = 0;
	g_bRPlayFocused = false;
	g_bRPlayStarted = false;
}

static bool Step_RunnerMenuPlay(int iFrame)
{
	Runner_GameComponent* pxGame = FindRunnerGame();
	Zenith_UI::Zenith_UIButton* pxPlay = FindRunnerPlayButton();

	switch (g_eRMenuPhase)
	{
	case RMenuPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
		g_eRMenuPhase = RMenuPhase::WaitMenu;
		return true;

	case RMenuPhase::WaitMenu:
		if (pxGame != nullptr && pxGame->GetGameState() == RunnerGameState::MAIN_MENU && pxPlay != nullptr)
		{
			g_eRMenuPhase = RMenuPhase::CheckFocus;
			g_iRMenuFrame = 0;
		}
		return iFrame < 600;

	case RMenuPhase::CheckFocus:
		if (++g_iRMenuFrame < 10)
		{
			return true;
		}
		if (pxPlay == nullptr) return false;
		g_bRPlayFocused = pxPlay->IsFocused();
		if (!g_bRPlayFocused)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[RunnerMenu] Play button not focused");
			return false;
		}
		g_eRMenuPhase = RMenuPhase::ActivatePlay;
		return true;

	case RMenuPhase::ActivatePlay:
		if (pxPlay == nullptr) return false;
		pxPlay->Activate();
		g_eRMenuPhase = RMenuPhase::AwaitPlaying;
		g_iRMenuFrame = 0;
		return true;

	case RMenuPhase::AwaitPlaying:
		if (pxGame != nullptr && pxGame->GetGameState() == RunnerGameState::PLAYING)
		{
			g_bRPlayStarted = true;
			g_eRMenuPhase = RMenuPhase::Done;
			return false;
		}
		return ++g_iRMenuFrame < 600;

	case RMenuPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_RunnerMenuPlay()
{
	Zenith_InputSimulator::ClearFixedDt();
	if (!g_bRPlayFocused) { Zenith_Log(LOG_CATEGORY_UNITTEST, "[RunnerMenu] focus wrong"); }
	if (!g_bRPlayStarted) { Zenith_Log(LOG_CATEGORY_UNITTEST, "[RunnerMenu] Play activation never reached PLAYING"); }
	return g_bRPlayFocused && g_bRPlayStarted;
}

static const Zenith_AutomatedTest g_xRunnerMenuPlayTest = {
	"Runner_MenuPlay_Test",
	&Setup_RunnerMenuPlay,
	&Step_RunnerMenuPlay,
	&Verify_RunnerMenuPlay,
	/*maxFrames*/ 2000,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xRunnerMenuPlayTest);

// ============================================================================
// Runner_ScoringHighScore_Test
// ============================================================================

namespace
{
	enum class RScorePhase { Boot, WaitPlaying, Settle, Inject, AwaitScore, PressReset, AwaitReset, Done };

	RScorePhase g_eRScorePhase = RScorePhase::Boot;
	int         g_iRScoreFrame = 0;
	uint32_t    g_uRScoreBase = 0;
	uint32_t    g_uRScoreAtSync = 0;
	float       g_fRDistAtReset = 0.0f;
	bool        g_bRScoreApplied = false;
	bool        g_bRHighScoreSynced = false;
}

static void Setup_RunnerScoring()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_eRScorePhase = RScorePhase::Boot;
	g_iRScoreFrame = 0;
	g_uRScoreBase = 0;
	g_uRScoreAtSync = 0;
	g_fRDistAtReset = 0.0f;
	g_bRScoreApplied = false;
	g_bRHighScoreSynced = false;
}

static bool Step_RunnerScoring(int iFrame)
{
	Runner_GameComponent* pxGame = FindRunnerGame();
	switch (g_eRScorePhase)
	{
	case RScorePhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_eRScorePhase = RScorePhase::WaitPlaying;
		return true;

	case RScorePhase::WaitPlaying:
		if (pxGame != nullptr && pxGame->GetGameState() == RunnerGameState::PLAYING)
		{
			g_eRScorePhase = RScorePhase::Settle;
			g_iRScoreFrame = 0;
		}
		return iFrame < 600;

	case RScorePhase::Settle:
		if (++g_iRScoreFrame < 30)
		{
			return true;
		}
		g_eRScorePhase = RScorePhase::Inject;
		return true;

	case RScorePhase::Inject:
		if (pxGame == nullptr) return false;
		g_uRScoreBase = pxGame->GetScore();
		pxGame->Test_InjectCollection(50, 1);
		g_eRScorePhase = RScorePhase::AwaitScore;
		g_iRScoreFrame = 0;
		return true;

	case RScorePhase::AwaitScore:
		if (pxGame == nullptr) return false;
		// Real pickups may add on top - require at least the injected 50.
		if (pxGame->GetScore() >= g_uRScoreBase + 50)
		{
			g_bRScoreApplied = true;
			g_uRScoreAtSync = pxGame->GetScore();
			g_eRScorePhase = RScorePhase::PressReset;
			return true;
		}
		if (++g_iRScoreFrame >= 60)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[RunnerScore] injected pickup never scored (score %u, base %u)",
				pxGame->GetScore(), g_uRScoreBase);
			return false;
		}
		return true;

	case RScorePhase::PressReset:
		g_fRDistAtReset = Runner_CharacterController::GetDistanceTraveled();
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_R);
		g_eRScorePhase = RScorePhase::AwaitReset;
		g_iRScoreFrame = 0;
		return true;

	case RScorePhase::AwaitReset:
		if (pxGame == nullptr) return false;
		if (Runner_CharacterController::GetDistanceTraveled() < g_fRDistAtReset - 5.0f
			&& pxGame->GetGameState() == RunnerGameState::PLAYING)
		{
			// The reset synced the high score from the pre-reset score.
			g_bRHighScoreSynced = pxGame->GetHighScore() >= g_uRScoreAtSync;
			if (!g_bRHighScoreSynced)
			{
				Zenith_Log(LOG_CATEGORY_UNITTEST, "[RunnerScore] high score %u < pre-reset score %u",
					pxGame->GetHighScore(), g_uRScoreAtSync);
			}
			g_eRScorePhase = RScorePhase::Done;
			return false;
		}
		return ++g_iRScoreFrame < 30;

	case RScorePhase::Done:
		return false;
	}
	return false;
}

static bool Verify_RunnerScoring()
{
	Zenith_InputSimulator::ClearFixedDt();
	if (!g_bRScoreApplied)    { Zenith_Log(LOG_CATEGORY_UNITTEST, "[RunnerScore] scoring never applied"); }
	if (!g_bRHighScoreSynced) { Zenith_Log(LOG_CATEGORY_UNITTEST, "[RunnerScore] R did not sync the high score"); }
	return g_bRScoreApplied && g_bRHighScoreSynced;
}

static const Zenith_AutomatedTest g_xRunnerScoringTest = {
	"Runner_ScoringHighScore_Test",
	&Setup_RunnerScoring,
	&Step_RunnerScoring,
	&Verify_RunnerScoring,
	/*maxFrames*/ 1500,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xRunnerScoringTest);

// ============================================================================
// Runner_CharacterActions_Test
// ============================================================================
// Lane switching + clamping, jump (+JUMP anim), slide (+SLIDE anim, ~0.8 s -
// the slide-timer countdown), and the no-jump-while-sliding gate. The
// auto-runner can die to an obstacle at any point: the guard presses R and
// retries the current block from its start.

namespace
{
	enum class RActPhase
	{
		Boot, WaitPlaying,
		LaneStart, LaneRightWait, LaneRightClampHold, LaneLeft1Wait, LaneLeft2Wait, LaneLeftClampHold,
		JumpStart, JumpAirWait, JumpLandWait,
		SlideStart, SlideEnterWait, SlideGateJump, SlideGateHold, SlideEndWait,
		Done
	};

	RActPhase g_eRActPhase = RActPhase::Boot;
	int       g_iRActFrame = 0;
	int       g_iRSlideFrames = 0;
	bool      g_bRLanesOk = false;
	bool      g_bRJumpOk = false;
	bool      g_bRJumpAnimOk = false;
	bool      g_bRSlideOk = false;
	bool      g_bRSlideAnimOk = false;
	bool      g_bRSlideGateOk = false;
	bool      g_bRSlideDurationOk = false;

	// On death: press R and go back to the current block's entry phase.
	RActPhase BlockEntry(RActPhase ePhase)
	{
		if (ePhase >= RActPhase::LaneStart && ePhase <= RActPhase::LaneLeftClampHold) return RActPhase::LaneStart;
		if (ePhase >= RActPhase::JumpStart && ePhase <= RActPhase::JumpLandWait) return RActPhase::JumpStart;
		return RActPhase::SlideStart;
	}
}

static void Setup_RunnerActions()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_eRActPhase = RActPhase::Boot;
	g_iRActFrame = 0;
	g_iRSlideFrames = 0;
	g_bRLanesOk = false;
	g_bRJumpOk = false;
	g_bRJumpAnimOk = false;
	g_bRSlideOk = false;
	g_bRSlideAnimOk = false;
	g_bRSlideGateOk = false;
	g_bRSlideDurationOk = false;
}

static bool Step_RunnerActions(int iFrame)
{
	Runner_GameComponent* pxGame = FindRunnerGame();

	// Death guard: any obstacle hit -> reset and retry the current block.
	if (pxGame != nullptr && g_eRActPhase > RActPhase::WaitPlaying && g_eRActPhase != RActPhase::Done
		&& pxGame->GetGameState() == RunnerGameState::GAME_OVER)
	{
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_R);
		g_eRActPhase = BlockEntry(g_eRActPhase);
		g_iRActFrame = -60;	// give the reset a second to land back in PLAYING
		return true;
	}

	switch (g_eRActPhase)
	{
	case RActPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_eRActPhase = RActPhase::WaitPlaying;
		return true;

	case RActPhase::WaitPlaying:
		if (pxGame != nullptr && pxGame->GetGameState() == RunnerGameState::PLAYING)
		{
			g_eRActPhase = RActPhase::LaneStart;
			g_iRActFrame = 0;
		}
		return iFrame < 600;

	// ---- LANE block ----
	case RActPhase::LaneStart:
		if (++g_iRActFrame < 30)
		{
			return true;
		}
		if (pxGame == nullptr || pxGame->GetGameState() != RunnerGameState::PLAYING) return true;
		if (Runner_CharacterController::GetCurrentLane() != 1)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[RunnerActions] expected middle lane at start, got %d",
				Runner_CharacterController::GetCurrentLane());
			return false;
		}
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_D);
		g_eRActPhase = RActPhase::LaneRightWait;
		g_iRActFrame = 0;
		return true;

	case RActPhase::LaneRightWait:
		if (Runner_CharacterController::GetCurrentLane() == 2)
		{
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_D);	// clamp attempt
			g_eRActPhase = RActPhase::LaneRightClampHold;
			g_iRActFrame = 0;
			return true;
		}
		return ++g_iRActFrame < 40;

	case RActPhase::LaneRightClampHold:
		if (++g_iRActFrame < 30)
		{
			return true;
		}
		if (Runner_CharacterController::GetCurrentLane() != 2)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[RunnerActions] right-lane clamp failed (lane %d)",
				Runner_CharacterController::GetCurrentLane());
			return false;
		}
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_A);
		g_eRActPhase = RActPhase::LaneLeft1Wait;
		g_iRActFrame = 0;
		return true;

	case RActPhase::LaneLeft1Wait:
		if (Runner_CharacterController::GetCurrentLane() == 1)
		{
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_A);
			g_eRActPhase = RActPhase::LaneLeft2Wait;
			g_iRActFrame = 0;
			return true;
		}
		return ++g_iRActFrame < 40;

	case RActPhase::LaneLeft2Wait:
		if (Runner_CharacterController::GetCurrentLane() == 0)
		{
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_A);	// clamp attempt
			g_eRActPhase = RActPhase::LaneLeftClampHold;
			g_iRActFrame = 0;
			return true;
		}
		return ++g_iRActFrame < 40;

	case RActPhase::LaneLeftClampHold:
		if (++g_iRActFrame < 30)
		{
			return true;
		}
		if (Runner_CharacterController::GetCurrentLane() != 0)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[RunnerActions] left-lane clamp failed (lane %d)",
				Runner_CharacterController::GetCurrentLane());
			return false;
		}
		g_bRLanesOk = true;
		g_eRActPhase = RActPhase::JumpStart;
		g_iRActFrame = 0;
		return true;

	// ---- JUMP block ----
	case RActPhase::JumpStart:
		if (++g_iRActFrame < 10)
		{
			return true;
		}
		if (pxGame == nullptr || pxGame->GetGameState() != RunnerGameState::PLAYING) return true;
		if (Runner_CharacterController::GetState() != RunnerCharacterState::RUNNING
			|| !Runner_CharacterController::IsGrounded())
		{
			return g_iRActFrame < 120;	// wait for a clean grounded run state
		}
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_SPACE);
		g_eRActPhase = RActPhase::JumpAirWait;
		g_iRActFrame = 0;
		return true;

	case RActPhase::JumpAirWait:
		if (Runner_CharacterController::GetState() == RunnerCharacterState::JUMPING)
		{
			g_bRJumpOk = true;
			if (Runner_AnimationDriver::GetCurrentState() == RunnerAnimState::JUMP)
			{
				g_bRJumpAnimOk = true;
			}
			if (g_bRJumpAnimOk || g_iRActFrame > 5)
			{
				g_eRActPhase = RActPhase::JumpLandWait;
				g_iRActFrame = 0;
				return true;
			}
		}
		return ++g_iRActFrame < 20;

	case RActPhase::JumpLandWait:
		if (Runner_CharacterController::GetState() == RunnerCharacterState::RUNNING
			&& Runner_CharacterController::IsGrounded())
		{
			g_eRActPhase = RActPhase::SlideStart;
			g_iRActFrame = 0;
			return true;
		}
		return ++g_iRActFrame < 180;

	// ---- SLIDE block (slide duration = the timer countdown under test) ----
	case RActPhase::SlideStart:
		if (++g_iRActFrame < 10)
		{
			return true;
		}
		if (pxGame == nullptr || pxGame->GetGameState() != RunnerGameState::PLAYING) return true;
		if (Runner_CharacterController::GetState() != RunnerCharacterState::RUNNING
			|| !Runner_CharacterController::IsGrounded())
		{
			return g_iRActFrame < 120;
		}
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_S);
		g_eRActPhase = RActPhase::SlideEnterWait;
		g_iRActFrame = 0;
		return true;

	case RActPhase::SlideEnterWait:
		if (Runner_CharacterController::GetState() == RunnerCharacterState::SLIDING)
		{
			g_bRSlideOk = Runner_CharacterController::GetCurrentCharacterHeight() < 1.0f;
			if (Runner_AnimationDriver::GetCurrentState() == RunnerAnimState::SLIDE)
			{
				g_bRSlideAnimOk = true;
			}
			if (g_bRSlideAnimOk || g_iRActFrame > 5)
			{
				g_iRSlideFrames = g_iRActFrame;
				g_eRActPhase = RActPhase::SlideGateJump;
				return true;
			}
		}
		++g_iRActFrame;
		++g_iRSlideFrames;
		return g_iRActFrame < 20;

	case RActPhase::SlideGateJump:
		// Jump while sliding must be refused (the Try*-gate under test).
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_SPACE);
		g_eRActPhase = RActPhase::SlideGateHold;
		g_iRActFrame = 0;
		return true;

	case RActPhase::SlideGateHold:
		++g_iRSlideFrames;
		if (++g_iRActFrame < 8)
		{
			if (Runner_CharacterController::GetState() == RunnerCharacterState::JUMPING)
			{
				Zenith_Log(LOG_CATEGORY_UNITTEST, "[RunnerActions] jump was NOT refused while sliding");
				return false;
			}
			return true;
		}
		g_bRSlideGateOk = Runner_CharacterController::GetState() == RunnerCharacterState::SLIDING;
		g_eRActPhase = RActPhase::SlideEndWait;
		g_iRActFrame = 0;
		return true;

	case RActPhase::SlideEndWait:
		++g_iRSlideFrames;
		if (Runner_CharacterController::GetState() == RunnerCharacterState::RUNNING)
		{
			// Slide duration 0.8 s at 60 fps = ~48 frames; allow wide margin.
			g_bRSlideDurationOk = g_iRSlideFrames >= 30 && g_iRSlideFrames <= 80;
			if (!g_bRSlideDurationOk)
			{
				Zenith_Log(LOG_CATEGORY_UNITTEST, "[RunnerActions] slide lasted %d frames (expected ~48)", g_iRSlideFrames);
			}
			g_eRActPhase = RActPhase::Done;
			return false;
		}
		return ++g_iRActFrame < 120;

	case RActPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_RunnerActions()
{
	Zenith_InputSimulator::ClearFixedDt();
	if (!g_bRLanesOk)         { Zenith_Log(LOG_CATEGORY_UNITTEST, "[RunnerActions] lane switching/clamping wrong"); }
	if (!g_bRJumpOk)          { Zenith_Log(LOG_CATEGORY_UNITTEST, "[RunnerActions] jump never entered JUMPING"); }
	if (!g_bRJumpAnimOk)      { Zenith_Log(LOG_CATEGORY_UNITTEST, "[RunnerActions] JUMP anim state never seen"); }
	if (!g_bRSlideOk)         { Zenith_Log(LOG_CATEGORY_UNITTEST, "[RunnerActions] slide never lowered the character"); }
	if (!g_bRSlideAnimOk)     { Zenith_Log(LOG_CATEGORY_UNITTEST, "[RunnerActions] SLIDE anim state never seen"); }
	if (!g_bRSlideGateOk)     { Zenith_Log(LOG_CATEGORY_UNITTEST, "[RunnerActions] slide-jump gate wrong"); }
	if (!g_bRSlideDurationOk) { Zenith_Log(LOG_CATEGORY_UNITTEST, "[RunnerActions] slide duration off"); }
	return g_bRLanesOk && g_bRJumpOk && g_bRJumpAnimOk && g_bRSlideOk && g_bRSlideAnimOk && g_bRSlideGateOk && g_bRSlideDurationOk;
}

static const Zenith_AutomatedTest g_xRunnerActionsTest = {
	"Runner_CharacterActions_Test",
	&Setup_RunnerActions,
	&Step_RunnerActions,
	&Verify_RunnerActions,
	/*maxFrames*/ 6000,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xRunnerActionsTest);

#endif // ZENITH_INPUT_SIMULATOR
