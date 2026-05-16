#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "Source/DPHeuristicBot.h"
#include "Source/DevilsPlayground_Tags.h"

#include <cstdio>

// ============================================================================
// Test_DPHeuristicBot_GoalDispatch
//
// Pins the 8-case PickGoalForState dispatch table on
// DPHeuristicBot::TestSurface. PickGoal in production builds an Observation
// tuple from DP_Player + scene queries; PickGoalForState takes the same
// logical state as primitives so we exercise every branch without setting
// up scene state.
//
// Priority order (the test asserts each branch by isolating it):
//   1. !bPossessed                        -> PossessClosest
//   2. priest close (< kFleeDistance ~7m) -> FleeFromPriest
//   3. life < swap threshold (~4s)        -> BodySwap
//   4. holding objective + pentagram in scene -> WalkToPentagram
//   5. holding Iron + forge in scene      -> WalkToForge
//   6. default with objective available   -> WalkToObjective
//   7. default with no objective          -> Idle
//   8. priest close STILL wins over holding-objective (priority check)
// ============================================================================

namespace
{
	bool g_bPassed = false;
	const char* g_szFailureReason = "";
	bool Fail(const char* sz) { g_szFailureReason = sz; return false; }

	using G = DPHeuristicBot::Goal;
	using DPHeuristicBot::TestSurface::PickGoalForState;

	bool Expect(G eExpected, G eActual, const char* szWhich)
	{
		if (eExpected == eActual) return true;
		static char sBuf[128];
		std::snprintf(sBuf, sizeof(sBuf),
			"%s: expected goal %u, got %u",
			szWhich, static_cast<unsigned>(eExpected), static_cast<unsigned>(eActual));
		g_szFailureReason = sBuf;
		return false;
	}
}

static void Setup_GoalDispatch()
{
	g_bPassed = false;
	g_szFailureReason = "";

	// 1. Not possessed -> PossessClosest. Other inputs don't matter.
	if (!Expect(G::PossessClosest,
		PickGoalForState(/*bPossessed=*/false,
			/*fPriestDist=*/-1.0f, /*fLife=*/0.0f, DP_ItemTag::None,
			/*bPentagramPresent=*/true, /*bForgePresent=*/true,
			/*bObjectiveItemAvailable=*/true),
		"not-possessed"))
		return;

	// 2. Possessed, priest within kFleeDistance -> FleeFromPriest.
	if (!Expect(G::FleeFromPriest,
		PickGoalForState(true,
			/*fPriestDist=*/5.0f, /*fLife=*/20.0f, DP_ItemTag::None,
			true, true, true),
		"priest-close"))
		return;

	// 3. Possessed, life < kSwapLifeThreshold -> BodySwap.
	if (!Expect(G::BodySwap,
		PickGoalForState(true,
			/*fPriestDist=*/30.0f, /*fLife=*/2.0f, DP_ItemTag::None,
			true, true, true),
		"low-life"))
		return;

	// 4. Holding an objective + pentagram in scene -> WalkToPentagram.
	if (!Expect(G::WalkToPentagram,
		PickGoalForState(true,
			/*fPriestDist=*/30.0f, /*fLife=*/20.0f, DP_ItemTag::Objective1,
			/*bPentagramPresent=*/true, true, true),
		"obj+pentagram"))
		return;
	// Test that every objective tag (1..5) routes the same way.
	for (int iObj = 0; iObj < 5; ++iObj)
	{
		const DP_ItemTag eTag = static_cast<DP_ItemTag>(
			static_cast<int>(DP_ItemTag::Objective1) + iObj);
		if (!Expect(G::WalkToPentagram,
			PickGoalForState(true, 30.0f, 20.0f, eTag, true, true, true),
			"obj-tag-variant"))
			return;
	}

	// 4b. Holding an objective but NO pentagram in scene -> fall through.
	// (Defensive: prevents NPE in production code via the .IsValid() check.)
	if (!Expect(G::WalkToObjective,
		PickGoalForState(true, 30.0f, 20.0f, DP_ItemTag::Objective1,
			/*bPentagramPresent=*/false, true, true),
		"obj-no-pentagram"))
		return;

	// 5. Holding Iron + forge in scene -> WalkToForge.
	if (!Expect(G::WalkToForge,
		PickGoalForState(true, 30.0f, 20.0f, DP_ItemTag::Iron,
			true, /*bForgePresent=*/true, true),
		"iron+forge"))
		return;

	// 5b. Holding Iron but NO forge -> fall through.
	if (!Expect(G::WalkToObjective,
		PickGoalForState(true, 30.0f, 20.0f, DP_ItemTag::Iron,
			true, /*bForgePresent=*/false, true),
		"iron-no-forge"))
		return;

	// 6. Default with objective available -> WalkToObjective.
	if (!Expect(G::WalkToObjective,
		PickGoalForState(true, 30.0f, 20.0f, DP_ItemTag::None,
			true, true, /*bObjectiveItemAvailable=*/true),
		"default-with-obj"))
		return;

	// 7. Default with NO objective available -> Idle.
	if (!Expect(G::Idle,
		PickGoalForState(true, 30.0f, 20.0f, DP_ItemTag::None,
			true, true, /*bObjectiveItemAvailable=*/false),
		"idle-no-obj"))
		return;

	// 8. Priest still wins priority over holding-objective +
	//    pentagram-present (flee takes precedence over deliver).
	if (!Expect(G::FleeFromPriest,
		PickGoalForState(true,
			/*fPriestDist=*/3.0f, 20.0f, DP_ItemTag::Objective2,
			true, true, true),
		"priest-vs-objective-priority"))
		return;

	// 9. Priest distance == kFleeDistance edge: 7.0f is NOT closer than
	//    7.0f, so it should NOT trigger flee. (Strict <.)
	//    7.001f should also not trigger; only < 7.
	if (!Expect(G::WalkToObjective,
		PickGoalForState(true,
			/*fPriestDist=*/7.0f, 20.0f, DP_ItemTag::None,
			true, true, true),
		"priest-at-threshold-not-flee"))
		return;

	// 10. fPriestDist = -1 (no priest known) treated as no flee.
	if (!Expect(G::WalkToObjective,
		PickGoalForState(true,
			/*fPriestDist=*/-1.0f, 20.0f, DP_ItemTag::None,
			true, true, true),
		"no-priest-no-flee"))
		return;

	// 11. Life == 0 with bPossessed=true is treated as "no life-swap
	// gate" (the > 0 guard in PickGoalForState skips the BodySwap branch
	// when life is exactly 0). Default with no objective -> Idle.
	if (!Expect(G::Idle,
		PickGoalForState(true,
			/*fPriestDist=*/30.0f, /*fLife=*/0.0f, DP_ItemTag::None,
			true, true, false),
		"life-zero-no-swap"))
		return;

	g_bPassed = true;
	std::printf("[DPHeuristicBot_GoalDispatch] all 11 priority/edge cases passed\n");
	std::fflush(stdout);
}

static bool Step_GoalDispatch(int /*iFrame*/) { return false; }

static bool Verify_GoalDispatch()
{
	if (!g_bPassed)
	{
		Zenith_Log(LOG_CATEGORY_AI, "DPHeuristicBot_GoalDispatch: %s", g_szFailureReason);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xGoalDispatchTest = {
	"Test_DPHeuristicBot_GoalDispatch",
	&Setup_GoalDispatch,
	&Step_GoalDispatch,
	&Verify_GoalDispatch,
	30
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xGoalDispatchTest);

#endif // ZENITH_INPUT_SIMULATOR
