#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "ZenithECS/Zenith_EventSystem.h"
#include "ZenithECS/Zenith_SceneSystem.h"

#include "Source/PublicInterfaces.h"

#include <cstdio>

// ============================================================================
// Test_P1Dawn_NoFireWhenNotStarted (MVP-1.3.5 Dawn -- negative control)
//
// Sibling to Test_P1Dawn_DispatchesRunLost. Verifies that when the
// player loads the scene but does NOT trigger StartNight (e.g., a
// gym scene that just exercises mechanics without the full night
// loop), the night timer stays inactive and DP_OnRunLost{Dawn}
// never fires.
//
// Procedure:
//   1. Subscribe to DP_OnRunLost in Setup; count Dawn-cause events.
//   2. Load GameLevel.
//   3. Do NOT call DP_Night::StartNight.
//   4. Tick ~120 frames (~2 s of game time) so TickNight is
//      definitely called many times.
//   5. Assert:
//        DawnCount == 0
//        IsNightActive() == false
//        HasDawnReached() == false
//
// What this catches:
//   * A regression where TickNight dispatches even when m_bNightActive
//     is false (i.e., the active-guard was removed).
//   * A regression where Reset() leaves m_bNightActive true (a
//     between-tests reset that left state stale).
//   * Implicit auto-start regression -- some commit that wired
//     StartNight into a scene-load hook without the menu being ready,
//     which would surprise tests that don't expect it.
// ============================================================================

namespace
{
	enum Phase : int { kDN_Start, kDN_WaitScene, kDN_Tick, kDN_Verify, kDN_Done };

	int                     g_iPhase = kDN_Start;
	int                     g_iDawnCount = 0;
	int                     g_iTickFrames = 0;
	bool                    g_bHasDawnFinal = true;   // sentinel
	bool                    g_bActiveFinal = true;    // sentinel
	Zenith_EventHandle      g_uHandle = 0;

	constexpr int kTICK_WINDOW_FRAMES = 120;

	void OnRunLost(const DP_OnRunLost& xEvt)
	{
		if (xEvt.m_eCause == DP_RunLostCause::Dawn)
		{
			++g_iDawnCount;
		}
	}
}

static void Setup_P1DawnNoFire()
{
	g_iPhase = kDN_Start;
	g_iDawnCount = 0;
	g_iTickFrames = 0;
	g_bHasDawnFinal = true;
	g_bActiveFinal = true;
	g_uHandle = Zenith_EventDispatcher::Get().Subscribe<DP_OnRunLost>(&OnRunLost);
}

static bool Step_P1DawnNoFire(int iFrame)
{
	switch (g_iPhase)
	{
	case kDN_Start:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kDN_WaitScene;
		return true;

	case kDN_WaitScene:
		if (iFrame > 5)
		{
			g_iTickFrames = 0;
			g_iPhase = kDN_Tick;
		}
		return true;

	case kDN_Tick:
		++g_iTickFrames;
		if (g_iTickFrames >= kTICK_WINDOW_FRAMES)
		{
			g_iPhase = kDN_Verify;
		}
		return true;

	case kDN_Verify:
		g_bHasDawnFinal = DP_Night::HasDawnReached();
		g_bActiveFinal = DP_Night::IsNightActive();
		std::printf("[P1DawnNoFire] dawnCount=%d hasDawn=%d active=%d (after %d ticks, NO StartNight called)\n",
			g_iDawnCount, (int)g_bHasDawnFinal, (int)g_bActiveFinal,
			g_iTickFrames);
		std::fflush(stdout);
		if (g_uHandle != 0)
		{
			Zenith_EventDispatcher::Get().Unsubscribe(g_uHandle);
			g_uHandle = 0;
		}
		g_iPhase = kDN_Done;
		return false;

	case kDN_Done:
	default:
		if (g_uHandle != 0)
		{
			Zenith_EventDispatcher::Get().Unsubscribe(g_uHandle);
			g_uHandle = 0;
		}
		return false;
	}
}

static bool Verify_P1DawnNoFire()
{
	if (g_iDawnCount != 0)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1DawnNoFire: DP_OnRunLost{Dawn} fired %d times without StartNight() being called -- TickNight is missing its active-guard or the between-tests reset is broken",
			g_iDawnCount);
		return false;
	}
	if (g_bHasDawnFinal)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1DawnNoFire: HasDawnReached() returned true without StartNight + tick-to-zero -- state corrupted between tests");
		return false;
	}
	if (g_bActiveFinal)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1DawnNoFire: IsNightActive() returned true without StartNight -- reset hook isn't clearing the flag, or implicit auto-start regression");
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP1DawnNoFireTest = {
	"Test_P1Dawn_NoFireWhenNotStarted",
	&Setup_P1DawnNoFire,
	&Step_P1DawnNoFire,
	&Verify_P1DawnNoFire,
	240
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP1DawnNoFireTest);

#endif // ZENITH_INPUT_SIMULATOR
