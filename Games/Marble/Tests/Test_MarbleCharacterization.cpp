#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

/**
 * Test_MarbleCharacterization - characterization tests for the wave-2 graph
 * conversion of Marble's timer / win-loss flow.
 *
 * Written against the C++ version FIRST (the timer countdown + LOST-at-expiry
 * and the fall -> LOST decision inside Marble_GameComponent::OnUpdate); the
 * graph version must keep these green unchanged. Probes go through read-only
 * accessors that survive the conversion.
 *
 *   Marble_TimerFlow_Test - the countdown runs at exactly 1 second per second
 *                           (rate pinned over 600 fixed-dt frames), clamps to
 *                           0, and flips PLAYING -> LOST at expiry.
 *   Marble_FallLoss_Test  - rolling the ball off the platforms (held movement
 *                           keys, real input path) flips PLAYING -> LOST long
 *                           before the timer expires.
 *
 * W1 additions (written against the C++ key/menu/collection decisions FIRST,
 * before those decisions moved into Marble_GameFlow / the decomposed
 * Marble_LevelFlow):
 *
 *   Marble_PauseResume_Test   - P flips PLAYING -> PAUSED (timer frozen) and
 *                               back (timer running again).
 *   Marble_ResetFlow_Test     - R during PLAYING resets timer/score/collected
 *                               and stays PLAYING.
 *   Marble_EscapeMenu_Test    - Esc during PLAYING pauses (the WasPausePressed
 *                               P-or-Esc quirk); Esc again returns to the main
 *                               menu (MAIN_MENU on the menu-scene component).
 *   Marble_MenuPlay_Test      - initial focus on Play; W/S toggle focus
 *                               between the two buttons; activating Quit does
 *                               nothing (unwired quirk); activating Play loads
 *                               the gameplay scene -> PLAYING.
 *   Marble_CollectionWin_Test - injected collection results accumulate score/
 *                               collected; bAllCollected flips PLAYING -> WON.
 */

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "Input/Zenith_Input.h"
#include "Input/Zenith_InputSimulator.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "UI/Zenith_UIButton.h"
#include "Components/Marble_GameComponent.h"

#include <cmath>

namespace
{
	Marble_GameComponent* FindGameComponent()
	{
		Marble_GameComponent* pxFound = nullptr;
		g_xEngine.Scenes().QueryAllScenes<Marble_GameComponent>().ForEach(
			[&pxFound](Zenith_EntityID, Marble_GameComponent& xGame)
			{
				if (pxFound == nullptr) pxFound = &xGame;
			});
		return pxFound;
	}
}

// ============================================================================
// Marble_TimerFlow_Test
// ============================================================================

namespace
{
	enum class TimerPhase { Boot, WaitPlaying, RateCheck, AwaitLoss, Done };

	TimerPhase g_eTimerPhase = TimerPhase::Boot;
	int        g_iTimerFrame = 0;
	bool       g_bRateOk = false;
	bool       g_bSawLoss = false;
	bool       g_bTimeClamped = false;
}

static void Setup_TimerFlow()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_eTimerPhase = TimerPhase::Boot;
	g_iTimerFrame = 0;
	g_bRateOk = false;
	g_bSawLoss = false;
	g_bTimeClamped = false;
}

static bool Step_TimerFlow(int iFrame)
{
	switch (g_eTimerPhase)
	{
	case TimerPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_eTimerPhase = TimerPhase::WaitPlaying;
		return true;

	case TimerPhase::WaitPlaying:
	{
		Marble_GameComponent* pxGame = FindGameComponent();
		if (pxGame != nullptr && pxGame->GetGameState() == MarbleGameState::PLAYING)
		{
			g_eTimerPhase = TimerPhase::RateCheck;
			g_iTimerFrame = 0;
			return true;
		}
		return iFrame < 600;
	}

	case TimerPhase::RateCheck:
	{
		// 600 fixed-dt frames = exactly 10 seconds of game time off the 60 s
		// initial timer.
		if (++g_iTimerFrame < 600)
		{
			return true;
		}
		Marble_GameComponent* pxGame = FindGameComponent();
		if (pxGame == nullptr) return false;
		const float fTime = pxGame->GetTimeRemaining();
		g_bRateOk = (fTime > 49.0f && fTime < 51.0f);
		if (!g_bRateOk)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[MarbleTimer] expected ~50s after 600 frames, saw %.2f", fTime);
			return false;
		}
		g_eTimerPhase = TimerPhase::AwaitLoss;
		g_iTimerFrame = 0;
		return true;
	}

	case TimerPhase::AwaitLoss:
	{
		Marble_GameComponent* pxGame = FindGameComponent();
		if (pxGame == nullptr) return false;
		if (pxGame->GetGameState() == MarbleGameState::LOST)
		{
			g_bSawLoss = true;
			g_bTimeClamped = pxGame->GetTimeRemaining() <= 0.0f;
			g_eTimerPhase = TimerPhase::Done;
			return false;
		}
		// ~50 s of game time left; allow slack.
		return ++g_iTimerFrame < 3300;
	}

	case TimerPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_TimerFlow()
{
	Zenith_InputSimulator::ClearFixedDt();
	if (!g_bRateOk)      { Zenith_Log(LOG_CATEGORY_UNITTEST, "[MarbleTimer] countdown rate wrong"); }
	if (!g_bSawLoss)     { Zenith_Log(LOG_CATEGORY_UNITTEST, "[MarbleTimer] never flipped to LOST"); }
	if (!g_bTimeClamped) { Zenith_Log(LOG_CATEGORY_UNITTEST, "[MarbleTimer] time not clamped to 0 at expiry"); }
	return g_bRateOk && g_bSawLoss && g_bTimeClamped;
}

static const Zenith_AutomatedTest g_xMarbleTimerFlowTest = {
	"Marble_TimerFlow_Test",
	&Setup_TimerFlow,
	&Step_TimerFlow,
	&Verify_TimerFlow,
	/*maxFrames*/ 5400,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xMarbleTimerFlowTest);

// ============================================================================
// Marble_FallLoss_Test
// ============================================================================

namespace
{
	enum class FallPhase { Boot, WaitPlaying, Roll, Done };

	FallPhase g_eFallPhase = FallPhase::Boot;
	int       g_iFallFrame = 0;
	bool      g_bFellToLoss = false;
	float     g_fTimeAtLoss = 0.0f;
}

static void Setup_FallLoss()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_eFallPhase = FallPhase::Boot;
	g_iFallFrame = 0;
	g_bFellToLoss = false;
	g_fTimeAtLoss = 0.0f;
}

static bool Step_FallLoss(int iFrame)
{
	switch (g_eFallPhase)
	{
	case FallPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_eFallPhase = FallPhase::WaitPlaying;
		return true;

	case FallPhase::WaitPlaying:
	{
		Marble_GameComponent* pxGame = FindGameComponent();
		if (pxGame != nullptr && pxGame->GetGameState() == MarbleGameState::PLAYING)
		{
			g_eFallPhase = FallPhase::Roll;
			g_iFallFrame = 0;
			return true;
		}
		return iFrame < 600;
	}

	case FallPhase::Roll:
	{
		Marble_GameComponent* pxGame = FindGameComponent();
		if (pxGame == nullptr) return false;
		if (pxGame->GetGameState() == MarbleGameState::LOST)
		{
			Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_W, false);
			Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_S, false);
			g_bFellToLoss = true;
			g_fTimeAtLoss = pxGame->GetTimeRemaining();
			g_eFallPhase = FallPhase::Done;
			return false;
		}
		// Roll forward off the platforms (real input path; camera-relative).
		// If 25 s of forward rolling somehow kept us on the platforms, roll
		// backward off the start platform instead.
		++g_iFallFrame;
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_W, g_iFallFrame < 1500);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_S, g_iFallFrame >= 1500);
		return g_iFallFrame < 3000;
	}

	case FallPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_FallLoss()
{
	Zenith_InputSimulator::ClearFixedDt();
	Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_W, false);
	Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_S, false);
	if (!g_bFellToLoss)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[MarbleFall] never flipped to LOST from falling");
		return false;
	}
	// The loss must have come from the fall, not from timer expiry.
	if (g_fTimeAtLoss <= 5.0f)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[MarbleFall] LOST arrived with %.1fs left - looks like timer expiry", g_fTimeAtLoss);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xMarbleFallLossTest = {
	"Marble_FallLoss_Test",
	&Setup_FallLoss,
	&Step_FallLoss,
	&Verify_FallLoss,
	/*maxFrames*/ 4200,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xMarbleFallLossTest);

// ============================================================================
// Shared helpers for the W1 menu/key-flow tests
// ============================================================================

namespace
{
	// Buttons are canvas-owned and may be recreated - re-find every frame,
	// never cache across frames.
	Zenith_UI::Zenith_UIButton* FindMenuButton(const char* szName)
	{
		Zenith_UI::Zenith_UIButton* pxFound = nullptr;
		g_xEngine.Scenes().QueryAllScenes<Zenith_UIComponent>().ForEach(
			[&pxFound, szName](Zenith_EntityID, Zenith_UIComponent& xUI)
			{
				if (pxFound == nullptr)
				{
					pxFound = xUI.FindElement<Zenith_UI::Zenith_UIButton>(szName);
				}
			});
		return pxFound;
	}
}

// ============================================================================
// Marble_PauseResume_Test
// ============================================================================

namespace
{
	enum class PausePhase { Boot, WaitPlaying, PressPause, AwaitPaused, HoldPaused, PressResume, AwaitResumed, RunResumed, Done };

	PausePhase g_ePausePhase = PausePhase::Boot;
	int        g_iPauseFrame = 0;
	float      g_fTimeAtPause = 0.0f;
	bool       g_bSawPaused = false;
	bool       g_bTimeFrozeWhilePaused = false;
	bool       g_bSawResumed = false;
	bool       g_bTimeRanAfterResume = false;
}

static void Setup_PauseResume()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_ePausePhase = PausePhase::Boot;
	g_iPauseFrame = 0;
	g_fTimeAtPause = 0.0f;
	g_bSawPaused = false;
	g_bTimeFrozeWhilePaused = false;
	g_bSawResumed = false;
	g_bTimeRanAfterResume = false;
}

static bool Step_PauseResume(int iFrame)
{
	Marble_GameComponent* pxGame = FindGameComponent();
	switch (g_ePausePhase)
	{
	case PausePhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_ePausePhase = PausePhase::WaitPlaying;
		return true;

	case PausePhase::WaitPlaying:
		if (pxGame != nullptr && pxGame->GetGameState() == MarbleGameState::PLAYING)
		{
			g_ePausePhase = PausePhase::PressPause;
			g_iPauseFrame = 0;
		}
		return iFrame < 600;

	case PausePhase::PressPause:
		// Let the game run a moment so the timer is visibly below 60 first.
		if (++g_iPauseFrame < 60)
		{
			return true;
		}
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_P);
		g_ePausePhase = PausePhase::AwaitPaused;
		g_iPauseFrame = 0;
		return true;

	case PausePhase::AwaitPaused:
		if (pxGame == nullptr) return false;
		if (pxGame->GetGameState() == MarbleGameState::PAUSED)
		{
			g_bSawPaused = true;
			g_fTimeAtPause = pxGame->GetTimeRemaining();
			g_ePausePhase = PausePhase::HoldPaused;
			g_iPauseFrame = 0;
			return true;
		}
		return ++g_iPauseFrame < 10;

	case PausePhase::HoldPaused:
		if (pxGame == nullptr) return false;
		if (++g_iPauseFrame < 120)
		{
			return true;
		}
		// 2 s of paused frames: the countdown must not have moved.
		g_bTimeFrozeWhilePaused = std::fabs(pxGame->GetTimeRemaining() - g_fTimeAtPause) < 0.05f;
		if (!g_bTimeFrozeWhilePaused)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[MarblePause] timer moved while paused: %.2f -> %.2f",
				g_fTimeAtPause, pxGame->GetTimeRemaining());
			return false;
		}
		g_ePausePhase = PausePhase::PressResume;
		return true;

	case PausePhase::PressResume:
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_P);
		g_ePausePhase = PausePhase::AwaitResumed;
		g_iPauseFrame = 0;
		return true;

	case PausePhase::AwaitResumed:
		if (pxGame == nullptr) return false;
		if (pxGame->GetGameState() == MarbleGameState::PLAYING)
		{
			g_bSawResumed = true;
			g_fTimeAtPause = pxGame->GetTimeRemaining();	// re-baseline for the run check
			g_ePausePhase = PausePhase::RunResumed;
			g_iPauseFrame = 0;
			return true;
		}
		return ++g_iPauseFrame < 10;

	case PausePhase::RunResumed:
		if (pxGame == nullptr) return false;
		if (++g_iPauseFrame < 120)
		{
			return true;
		}
		// 2 s of resumed frames: the countdown runs at ~1 s/s again.
		{
			const float fElapsed = g_fTimeAtPause - pxGame->GetTimeRemaining();
			g_bTimeRanAfterResume = (fElapsed > 1.5f && fElapsed < 2.5f);
			if (!g_bTimeRanAfterResume)
			{
				Zenith_Log(LOG_CATEGORY_UNITTEST, "[MarblePause] expected ~2s elapsed after resume, saw %.2f", fElapsed);
			}
		}
		g_ePausePhase = PausePhase::Done;
		return false;

	case PausePhase::Done:
		return false;
	}
	return false;
}

static bool Verify_PauseResume()
{
	Zenith_InputSimulator::ClearFixedDt();
	if (!g_bSawPaused)            { Zenith_Log(LOG_CATEGORY_UNITTEST, "[MarblePause] P never flipped PLAYING -> PAUSED"); }
	if (!g_bTimeFrozeWhilePaused) { Zenith_Log(LOG_CATEGORY_UNITTEST, "[MarblePause] timer not frozen while paused"); }
	if (!g_bSawResumed)           { Zenith_Log(LOG_CATEGORY_UNITTEST, "[MarblePause] P never flipped PAUSED -> PLAYING"); }
	if (!g_bTimeRanAfterResume)   { Zenith_Log(LOG_CATEGORY_UNITTEST, "[MarblePause] timer not running after resume"); }
	return g_bSawPaused && g_bTimeFrozeWhilePaused && g_bSawResumed && g_bTimeRanAfterResume;
}

static const Zenith_AutomatedTest g_xMarblePauseResumeTest = {
	"Marble_PauseResume_Test",
	&Setup_PauseResume,
	&Step_PauseResume,
	&Verify_PauseResume,
	/*maxFrames*/ 1500,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xMarblePauseResumeTest);

// ============================================================================
// Marble_ResetFlow_Test
// ============================================================================

namespace
{
	enum class ResetPhase { Boot, WaitPlaying, RunDown, PressReset, AwaitReset, Done };

	ResetPhase g_eResetPhase = ResetPhase::Boot;
	int        g_iResetFrame = 0;
	float      g_fTimeBeforeReset = 0.0f;
	bool       g_bResetOk = false;
}

static void Setup_ResetFlow()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_eResetPhase = ResetPhase::Boot;
	g_iResetFrame = 0;
	g_fTimeBeforeReset = 0.0f;
	g_bResetOk = false;
}

static bool Step_ResetFlow(int iFrame)
{
	Marble_GameComponent* pxGame = FindGameComponent();
	switch (g_eResetPhase)
	{
	case ResetPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_eResetPhase = ResetPhase::WaitPlaying;
		return true;

	case ResetPhase::WaitPlaying:
		if (pxGame != nullptr && pxGame->GetGameState() == MarbleGameState::PLAYING)
		{
			g_eResetPhase = ResetPhase::RunDown;
			g_iResetFrame = 0;
		}
		return iFrame < 600;

	case ResetPhase::RunDown:
		// 5 s of play so the timer is clearly below the initial 60. Inflate
		// the score via the injection seam so the reset-to-zero is
		// distinguishable from the deterministic start-platform auto-pickup
		// (collectible 0 spawns inside the ball's pickup radius = +100 at
		// every level start).
		if (++g_iResetFrame == 100 && pxGame != nullptr)
		{
			pxGame->Test_InjectCollection(3, 300, false);
		}
		if (g_iResetFrame < 300)
		{
			return true;
		}
		g_eResetPhase = ResetPhase::PressReset;
		return true;

	case ResetPhase::PressReset:
		if (pxGame == nullptr) return false;
		g_fTimeBeforeReset = pxGame->GetTimeRemaining();
		if (g_fTimeBeforeReset > 56.0f)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[MarbleReset] timer did not run down before reset (%.2f)", g_fTimeBeforeReset);
			return false;
		}
		if (pxGame->GetScore() < 300)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[MarbleReset] score inflation never landed (%u)", pxGame->GetScore());
			return false;
		}
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_R);
		g_eResetPhase = ResetPhase::AwaitReset;
		g_iResetFrame = 0;
		return true;

	case ResetPhase::AwaitReset:
		if (pxGame == nullptr) return false;
		if (pxGame->GetTimeRemaining() > 58.0f)
		{
			// Fresh level: timer back at ~60, score/collected zeroed (allow
			// the start-platform auto-pickup to have already re-landed).
			g_bResetOk = pxGame->GetGameState() == MarbleGameState::PLAYING
				&& pxGame->GetScore() <= 100
				&& pxGame->GetCollectedCount() <= 1;
			if (!g_bResetOk)
			{
				Zenith_Log(LOG_CATEGORY_UNITTEST, "[MarbleReset] post-reset state wrong: score %u collected %u state %d",
					pxGame->GetScore(), pxGame->GetCollectedCount(), static_cast<int>(pxGame->GetGameState()));
			}
			g_eResetPhase = ResetPhase::Done;
			return false;
		}
		return ++g_iResetFrame < 10;

	case ResetPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_ResetFlow()
{
	Zenith_InputSimulator::ClearFixedDt();
	if (!g_bResetOk)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[MarbleReset] R did not reset timer/score/collected back to a fresh PLAYING level");
	}
	return g_bResetOk;
}

static const Zenith_AutomatedTest g_xMarbleResetFlowTest = {
	"Marble_ResetFlow_Test",
	&Setup_ResetFlow,
	&Step_ResetFlow,
	&Verify_ResetFlow,
	/*maxFrames*/ 1500,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xMarbleResetFlowTest);

// ============================================================================
// Marble_EscapeMenu_Test
// ============================================================================

namespace
{
	enum class EscPhase { Boot, WaitPlaying, PressEsc, AwaitPaused, PressEscAgain, AwaitMenu, Done };

	EscPhase g_eEscPhase = EscPhase::Boot;
	int      g_iEscFrame = 0;
	bool     g_bEscPaused = false;
	bool     g_bEscReachedMenu = false;
}

static void Setup_EscapeMenu()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_eEscPhase = EscPhase::Boot;
	g_iEscFrame = 0;
	g_bEscPaused = false;
	g_bEscReachedMenu = false;
}

static bool Step_EscapeMenu(int iFrame)
{
	Marble_GameComponent* pxGame = FindGameComponent();
	switch (g_eEscPhase)
	{
	case EscPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_eEscPhase = EscPhase::WaitPlaying;
		return true;

	case EscPhase::WaitPlaying:
		if (pxGame != nullptr && pxGame->GetGameState() == MarbleGameState::PLAYING)
		{
			g_eEscPhase = EscPhase::PressEsc;
			g_iEscFrame = 0;
		}
		return iFrame < 600;

	case EscPhase::PressEsc:
		if (++g_iEscFrame < 30)
		{
			return true;
		}
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ESCAPE);
		g_eEscPhase = EscPhase::AwaitPaused;
		g_iEscFrame = 0;
		return true;

	case EscPhase::AwaitPaused:
		if (pxGame == nullptr) return false;
		// The WasPausePressed quirk: Esc during PLAYING PAUSES (it never
		// reaches the return-to-menu branch).
		if (pxGame->GetGameState() == MarbleGameState::PAUSED)
		{
			g_bEscPaused = true;
			g_eEscPhase = EscPhase::PressEscAgain;
			g_iEscFrame = 0;
			return true;
		}
		return ++g_iEscFrame < 10;

	case EscPhase::PressEscAgain:
		if (++g_iEscFrame < 30)
		{
			return true;
		}
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ESCAPE);
		g_eEscPhase = EscPhase::AwaitMenu;
		g_iEscFrame = 0;
		return true;

	case EscPhase::AwaitMenu:
		// The gameplay component dies with its scene; the menu scene's fresh
		// component reports MAIN_MENU once the SINGLE load drains.
		if (pxGame != nullptr && pxGame->GetGameState() == MarbleGameState::MAIN_MENU)
		{
			g_bEscReachedMenu = true;
			g_eEscPhase = EscPhase::Done;
			return false;
		}
		return ++g_iEscFrame < 600;

	case EscPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_EscapeMenu()
{
	Zenith_InputSimulator::ClearFixedDt();
	if (!g_bEscPaused)      { Zenith_Log(LOG_CATEGORY_UNITTEST, "[MarbleEsc] Esc during PLAYING did not pause"); }
	if (!g_bEscReachedMenu) { Zenith_Log(LOG_CATEGORY_UNITTEST, "[MarbleEsc] Esc during PAUSED did not return to the main menu"); }
	return g_bEscPaused && g_bEscReachedMenu;
}

static const Zenith_AutomatedTest g_xMarbleEscapeMenuTest = {
	"Marble_EscapeMenu_Test",
	&Setup_EscapeMenu,
	&Step_EscapeMenu,
	&Verify_EscapeMenu,
	/*maxFrames*/ 2000,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xMarbleEscapeMenuTest);

// ============================================================================
// Marble_MenuPlay_Test
// ============================================================================

namespace
{
	enum class MenuPhase { Boot, WaitMenu, CheckInitialFocus, ToggleDown, AwaitQuitFocused, ToggleUp, AwaitPlayFocused, ActivateQuit, HoldAfterQuit, ActivatePlay, AwaitPlaying, Done };

	MenuPhase g_eMenuPhase = MenuPhase::Boot;
	int       g_iMenuFrame = 0;
	bool      g_bInitialFocusOnPlay = false;
	bool      g_bToggleToQuitOk = false;
	bool      g_bToggleToPlayOk = false;
	bool      g_bQuitInert = false;
	bool      g_bPlayStartedGame = false;
}

static void Setup_MenuPlay()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_eMenuPhase = MenuPhase::Boot;
	g_iMenuFrame = 0;
	g_bInitialFocusOnPlay = false;
	g_bToggleToQuitOk = false;
	g_bToggleToPlayOk = false;
	g_bQuitInert = false;
	g_bPlayStartedGame = false;
}

static bool Step_MenuPlay(int iFrame)
{
	Marble_GameComponent* pxGame = FindGameComponent();
	Zenith_UI::Zenith_UIButton* pxPlay = FindMenuButton("MenuPlay");
	Zenith_UI::Zenith_UIButton* pxQuit = FindMenuButton("MenuQuit");

	switch (g_eMenuPhase)
	{
	case MenuPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
		g_eMenuPhase = MenuPhase::WaitMenu;
		return true;

	case MenuPhase::WaitMenu:
		if (pxGame != nullptr && pxGame->GetGameState() == MarbleGameState::MAIN_MENU
			&& pxPlay != nullptr && pxQuit != nullptr)
		{
			g_eMenuPhase = MenuPhase::CheckInitialFocus;
			g_iMenuFrame = 0;
		}
		return iFrame < 600;

	case MenuPhase::CheckInitialFocus:
		// Give the per-frame focus refresh a few frames to settle.
		if (++g_iMenuFrame < 10)
		{
			return true;
		}
		if (pxPlay == nullptr || pxQuit == nullptr) return false;
		g_bInitialFocusOnPlay = pxPlay->IsFocused() && !pxQuit->IsFocused();
		if (!g_bInitialFocusOnPlay)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[MarbleMenu] initial focus not on Play");
			return false;
		}
		g_eMenuPhase = MenuPhase::ToggleDown;
		return true;

	case MenuPhase::ToggleDown:
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_S);
		g_eMenuPhase = MenuPhase::AwaitQuitFocused;
		g_iMenuFrame = 0;
		return true;

	case MenuPhase::AwaitQuitFocused:
		if (pxPlay == nullptr || pxQuit == nullptr) return false;
		if (pxQuit->IsFocused() && !pxPlay->IsFocused())
		{
			g_bToggleToQuitOk = true;
			g_eMenuPhase = MenuPhase::ToggleUp;
			return true;
		}
		return ++g_iMenuFrame < 10;

	case MenuPhase::ToggleUp:
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_W);
		g_eMenuPhase = MenuPhase::AwaitPlayFocused;
		g_iMenuFrame = 0;
		return true;

	case MenuPhase::AwaitPlayFocused:
		if (pxPlay == nullptr || pxQuit == nullptr) return false;
		if (pxPlay->IsFocused() && !pxQuit->IsFocused())
		{
			g_bToggleToPlayOk = true;
			g_eMenuPhase = MenuPhase::ActivateQuit;
			g_iMenuFrame = 0;
			return true;
		}
		return ++g_iMenuFrame < 10;

	case MenuPhase::ActivateQuit:
		// The Quit button is deliberately unwired - activating it must do
		// nothing (the quirk this test pins structurally).
		if (pxQuit == nullptr) return false;
		pxQuit->Activate();
		g_eMenuPhase = MenuPhase::HoldAfterQuit;
		g_iMenuFrame = 0;
		return true;

	case MenuPhase::HoldAfterQuit:
		if (++g_iMenuFrame < 60)
		{
			return true;
		}
		if (pxGame == nullptr) return false;
		g_bQuitInert = pxGame->GetGameState() == MarbleGameState::MAIN_MENU;
		if (!g_bQuitInert)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[MarbleMenu] Quit activation was not inert");
			return false;
		}
		g_eMenuPhase = MenuPhase::ActivatePlay;
		return true;

	case MenuPhase::ActivatePlay:
		if (pxPlay == nullptr) return false;
		pxPlay->Activate();
		g_eMenuPhase = MenuPhase::AwaitPlaying;
		g_iMenuFrame = 0;
		return true;

	case MenuPhase::AwaitPlaying:
		if (pxGame != nullptr && pxGame->GetGameState() == MarbleGameState::PLAYING)
		{
			g_bPlayStartedGame = true;
			g_eMenuPhase = MenuPhase::Done;
			return false;
		}
		return ++g_iMenuFrame < 600;

	case MenuPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_MenuPlay()
{
	Zenith_InputSimulator::ClearFixedDt();
	if (!g_bInitialFocusOnPlay) { Zenith_Log(LOG_CATEGORY_UNITTEST, "[MarbleMenu] initial focus wrong"); }
	if (!g_bToggleToQuitOk)     { Zenith_Log(LOG_CATEGORY_UNITTEST, "[MarbleMenu] S did not move focus to Quit"); }
	if (!g_bToggleToPlayOk)     { Zenith_Log(LOG_CATEGORY_UNITTEST, "[MarbleMenu] W did not move focus back to Play"); }
	if (!g_bQuitInert)          { Zenith_Log(LOG_CATEGORY_UNITTEST, "[MarbleMenu] Quit not inert"); }
	if (!g_bPlayStartedGame)    { Zenith_Log(LOG_CATEGORY_UNITTEST, "[MarbleMenu] Play activation never reached PLAYING"); }
	return g_bInitialFocusOnPlay && g_bToggleToQuitOk && g_bToggleToPlayOk && g_bQuitInert && g_bPlayStartedGame;
}

static const Zenith_AutomatedTest g_xMarbleMenuPlayTest = {
	"Marble_MenuPlay_Test",
	&Setup_MenuPlay,
	&Step_MenuPlay,
	&Verify_MenuPlay,
	/*maxFrames*/ 2500,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xMarbleMenuPlayTest);

// ============================================================================
// Marble_CollectionWin_Test
// ============================================================================

namespace
{
	enum class WinPhase { Boot, WaitPlaying, Settle, InjectPartial, AwaitPartial, InjectFinal, AwaitWon, Done };

	WinPhase g_eWinPhase = WinPhase::Boot;
	int      g_iWinFrame = 0;
	uint32_t g_uWinScore0 = 0;
	uint32_t g_uWinCollected0 = 0;
	bool     g_bPartialApplied = false;
	bool     g_bWonApplied = false;
}

static void Setup_CollectionWin()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_eWinPhase = WinPhase::Boot;
	g_iWinFrame = 0;
	g_uWinScore0 = 0;
	g_uWinCollected0 = 0;
	g_bPartialApplied = false;
	g_bWonApplied = false;
}

static bool Step_CollectionWin(int iFrame)
{
	Marble_GameComponent* pxGame = FindGameComponent();
	switch (g_eWinPhase)
	{
	case WinPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_eWinPhase = WinPhase::WaitPlaying;
		return true;

	case WinPhase::WaitPlaying:
		if (pxGame != nullptr && pxGame->GetGameState() == MarbleGameState::PLAYING)
		{
			g_eWinPhase = WinPhase::Settle;
			g_iWinFrame = 0;
		}
		return iFrame < 600;

	case WinPhase::Settle:
		// Collectible 0 spawns ON the start platform inside the ball's pickup
		// radius, so every level start auto-collects it within a couple of
		// frames (pre-existing C++ behaviour). Let that land, then baseline.
		if (++g_iWinFrame < 90)
		{
			return true;
		}
		if (pxGame == nullptr) return false;
		g_uWinScore0 = pxGame->GetScore();
		g_uWinCollected0 = pxGame->GetCollectedCount();
		g_eWinPhase = WinPhase::InjectPartial;
		return true;

	case WinPhase::InjectPartial:
		if (pxGame == nullptr) return false;
		pxGame->Test_InjectCollection(1, 100, false);
		g_eWinPhase = WinPhase::AwaitPartial;
		g_iWinFrame = 0;
		return true;

	case WinPhase::AwaitPartial:
		if (pxGame == nullptr) return false;
		if (pxGame->GetScore() == g_uWinScore0 + 100 && pxGame->GetCollectedCount() == g_uWinCollected0 + 1)
		{
			g_bPartialApplied = pxGame->GetGameState() == MarbleGameState::PLAYING;
			if (!g_bPartialApplied)
			{
				Zenith_Log(LOG_CATEGORY_UNITTEST, "[MarbleWin] partial collection changed the game state");
				return false;
			}
			g_eWinPhase = WinPhase::InjectFinal;
			return true;
		}
		if (++g_iWinFrame >= 60)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[MarbleWin] partial injection never applied: score %u collected %u state %d (baseline %u/%u)",
				pxGame->GetScore(), pxGame->GetCollectedCount(), static_cast<int>(pxGame->GetGameState()),
				g_uWinScore0, g_uWinCollected0);
			return false;
		}
		return true;

	case WinPhase::InjectFinal:
		if (pxGame == nullptr) return false;
		pxGame->Test_InjectCollection(2, 200, true);
		g_eWinPhase = WinPhase::AwaitWon;
		g_iWinFrame = 0;
		return true;

	case WinPhase::AwaitWon:
		if (pxGame == nullptr) return false;
		if (pxGame->GetGameState() == MarbleGameState::WON)
		{
			g_bWonApplied = pxGame->GetScore() == g_uWinScore0 + 300 && pxGame->GetCollectedCount() == g_uWinCollected0 + 3;
			if (!g_bWonApplied)
			{
				Zenith_Log(LOG_CATEGORY_UNITTEST, "[MarbleWin] WON with wrong totals: score %u collected %u (baseline %u/%u)",
					pxGame->GetScore(), pxGame->GetCollectedCount(), g_uWinScore0, g_uWinCollected0);
			}
			g_eWinPhase = WinPhase::Done;
			return false;
		}
		if (++g_iWinFrame >= 60)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[MarbleWin] WON never arrived: score %u collected %u state %d",
				pxGame->GetScore(), pxGame->GetCollectedCount(), static_cast<int>(pxGame->GetGameState()));
			return false;
		}
		return true;

	case WinPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_CollectionWin()
{
	Zenith_InputSimulator::ClearFixedDt();
	if (!g_bPartialApplied) { Zenith_Log(LOG_CATEGORY_UNITTEST, "[MarbleWin] partial collection not accumulated"); }
	if (!g_bWonApplied)     { Zenith_Log(LOG_CATEGORY_UNITTEST, "[MarbleWin] all-collected did not flip to WON with correct totals"); }
	return g_bPartialApplied && g_bWonApplied;
}

static const Zenith_AutomatedTest g_xMarbleCollectionWinTest = {
	"Marble_CollectionWin_Test",
	&Setup_CollectionWin,
	&Step_CollectionWin,
	&Verify_CollectionWin,
	/*maxFrames*/ 1500,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xMarbleCollectionWinTest);

#endif // ZENITH_INPUT_SIMULATOR
