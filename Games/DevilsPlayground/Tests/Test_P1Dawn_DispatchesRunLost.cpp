#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "ZenithECS/Zenith_EventSystem.h"
#include "ZenithECS/Zenith_SceneSystem.h"

#include "Source/PublicInterfaces.h"

#include <cstdio>

// ============================================================================
// Test_P1Dawn_DispatchesRunLost (MVP-1.3.5 Dawn half)
//
// Pins the Dawn run-loss path: when the night timer reaches 0,
// `DP_OnRunLost{Dawn}` dispatches EXACTLY ONCE. Subsequent ticks
// past 0 do NOT re-dispatch -- the GDD framing is "one dawn per
// run".
//
// Procedure (all in Setup / Step -- no scene-bound entities needed):
//   1. Subscribe to DP_OnRunLost in Setup; count Dawn-cause events.
//   2. Load GameLevel so DPPlayerController exists to drive
//      TickNight from its OnUpdate.
//   3. Call DP_Night::StartNight(0.5) -- timer counts down for ~0.5 s
//      of game time. At 60 Hz that's ~30 frames. We tick the harness
//      ~90 frames to overshoot the dispatch point by a factor of 3
//      so the "dispatches exactly once" assertion has teeth.
//   4. After the tick window, assert:
//        DawnCount == 1                 (fired exactly once)
//        HasDawnReached() == true       (state flag set)
//        GetNightTimeRemaining() == 0   (timer pinned at 0)
//        IsNightActive() == true        (run isn't reset -- only
//                                        Reset()/StartNight() flips
//                                        active back)
//
// What this catches:
//   * DP_Night::TickNight wasn't wired into DPPlayerController OnUpdate.
//   * The cross-zero detection used `< 0` instead of `<= 0` and
//     missed the exact-frame case.
//   * No dispatch guard -> Dawn fires every frame after timer hits 0.
//   * StartNight didn't reset the dispatch flag (second run after a
//     between-tests reset would not fire).
// ============================================================================

namespace
{
	enum Phase : int { kDW_Start, kDW_WaitScene, kDW_ArmTimer,
	                   kDW_TickWindow, kDW_Verify, kDW_Done };

	int                     g_iPhase = kDW_Start;
	int                     g_iDawnCount = 0;
	int                     g_iTickFrames = 0;
	bool                    g_bHasDawnFinal = false;
	float                   g_fRemainingFinal = -1.0f;
	bool                    g_bActiveFinal = false;
	Zenith_EventHandle      g_uHandle = 0;

	// 90 frames at fixed dt 0.01666 = ~1.5 s. Night duration 0.5 s
	// expires around frame 30. 90 frames overshoots by 60 frames so
	// "dispatched exactly once" is a meaningful assertion (a missing
	// dispatch-guard would make it ~60).
	constexpr int kTICK_WINDOW_FRAMES = 90;
	constexpr float kNIGHT_DURATION_S = 0.5f;

	void OnRunLost(const DP_OnRunLost& xEvt)
	{
		if (xEvt.m_eCause == DP_RunLostCause::Dawn)
		{
			++g_iDawnCount;
		}
	}
}

static void Setup_P1DawnDispatch()
{
	g_iPhase = kDW_Start;
	g_iDawnCount = 0;
	g_iTickFrames = 0;
	g_bHasDawnFinal = false;
	g_fRemainingFinal = -1.0f;
	g_bActiveFinal = false;
	g_uHandle = Zenith_EventDispatcher::Get().Subscribe<DP_OnRunLost>(&OnRunLost);
}

static bool Step_P1DawnDispatch(int iFrame)
{
	switch (g_iPhase)
	{
	case kDW_Start:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kDW_WaitScene;
		return true;

	case kDW_WaitScene:
		// Wait a few frames for DPPlayerController to have OnUpdate
		// firing (it needs the scene loaded + script attached).
		if (iFrame > 5)
		{
			g_iPhase = kDW_ArmTimer;
		}
		return true;

	case kDW_ArmTimer:
		DP_Night::StartNight(kNIGHT_DURATION_S);
		g_iTickFrames = 0;
		g_iPhase = kDW_TickWindow;
		return true;

	case kDW_TickWindow:
		++g_iTickFrames;
		if (g_iTickFrames >= kTICK_WINDOW_FRAMES)
		{
			g_iPhase = kDW_Verify;
		}
		return true;

	case kDW_Verify:
		g_bHasDawnFinal = DP_Night::HasDawnReached();
		g_fRemainingFinal = DP_Night::GetNightTimeRemaining();
		g_bActiveFinal = DP_Night::IsNightActive();
		std::printf("[P1DawnDispatch] dawnCount=%d hasDawn=%d remaining=%.3f active=%d (after %d ticks at duration %.3fs)\n",
			g_iDawnCount, (int)g_bHasDawnFinal, g_fRemainingFinal,
			(int)g_bActiveFinal, g_iTickFrames, kNIGHT_DURATION_S);
		std::fflush(stdout);
		if (g_uHandle != 0)
		{
			Zenith_EventDispatcher::Get().Unsubscribe(g_uHandle);
			g_uHandle = 0;
		}
		g_iPhase = kDW_Done;
		return false;

	case kDW_Done:
	default:
		if (g_uHandle != 0)
		{
			Zenith_EventDispatcher::Get().Unsubscribe(g_uHandle);
			g_uHandle = 0;
		}
		return false;
	}
}

static bool Verify_P1DawnDispatch()
{
	if (g_iDawnCount != 1)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1DawnDispatch: DP_OnRunLost{Dawn} fired %d times, expected exactly 1. >1 means missing dispatch-guard (TickNight refires every frame after 0); 0 means TickNight isn't wired into DPPlayerController OnUpdate or the cross-zero detection failed",
			g_iDawnCount);
		return false;
	}
	if (!g_bHasDawnFinal)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1DawnDispatch: HasDawnReached() returned false after dispatch -- m_bDawnDispatched flag isn't being set");
		return false;
	}
	if (g_fRemainingFinal != 0.0f)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1DawnDispatch: GetNightTimeRemaining() = %.3f, expected 0.0 (pinned at 0 after dispatch)",
			g_fRemainingFinal);
		return false;
	}
	if (!g_bActiveFinal)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1DawnDispatch: IsNightActive() returned false after dispatch -- only Reset/StartNight should clear active, not dispatch");
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP1DawnDispatchTest = {
	"Test_P1Dawn_DispatchesRunLost",
	&Setup_P1DawnDispatch,
	&Step_P1DawnDispatch,
	&Verify_P1DawnDispatch,
	240
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP1DawnDispatchTest);

#endif // ZENITH_INPUT_SIMULATOR
