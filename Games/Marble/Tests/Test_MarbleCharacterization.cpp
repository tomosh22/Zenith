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
 */

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "Input/Zenith_Input.h"
#include "Input/Zenith_InputSimulator.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
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

#endif // ZENITH_INPUT_SIMULATOR
