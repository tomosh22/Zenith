#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"

#include "Components/DPHUDController_Behaviour.h"
#include "Source/DevilsPlayground_Tags.h"

#include <cstdio>
#include <cstring>

// ============================================================================
// Test_P2HUD_TutorialHint
//
// Pins the 9-case dispatch table for the instructional-HUD context line.
// BuildTutorialHintForState is a pure static helper that takes the
// currently-held item tag + possession state + Aelfric awareness +
// run-over flag and returns the single most useful instruction string
// (or nullptr to hide the HUD line).
//
// The wrapper BuildTutorialHint resolves the tag from DP_Player; the pure
// form keeps the dispatch table free of scene + entity dependencies so a
// unit-style test like this can exercise every branch deterministically
// with no setup cost.
//
// Priority order (asserted by the test order below):
//   1. bRunOver        -> nullptr (RestartPrompt covers this)
//   2. !bPossessed     -> "Click a villager to possess them..."
//   3. Pursuing        -> "Aelfric sees you..."
//   4. Holding objective -> "Carry to the pentagram..."
//   5. Holding SkeletonKey -> "Skeleton Key in hand..."
//   6. Holding Iron      -> "Carry the iron to a forge..."
//   7. Holding reagent (BellSoul / BogWater) -> "Reagent in hand..."
//   8. Suspicious      -> "Aelfric is suspicious..."
//   9. Default         -> "Find an objective..."
// ============================================================================

namespace
{
	bool g_bPassed = false;
	const char* g_szFailureReason = "";

	using AelfricState = DPHUDController_Behaviour::AelfricState;

	bool ContainsSubstring(const char* sz, const char* szNeedle)
	{
		return sz != nullptr && std::strstr(sz, szNeedle) != nullptr;
	}

	bool ExpectSubstring(const char* szWhich,
	                     const char* szActual, const char* szNeedle)
	{
		if (ContainsSubstring(szActual, szNeedle)) return true;
		static char s_axBuf[256];
		std::snprintf(s_axBuf, sizeof(s_axBuf),
			"%s: expected substring '%s' but got '%s'",
			szWhich, szNeedle, (szActual ? szActual : "(null)"));
		g_szFailureReason = s_axBuf;
		return false;
	}

	bool ExpectNull(const char* szWhich, const char* szActual)
	{
		if (szActual == nullptr) return true;
		static char s_axBuf[256];
		std::snprintf(s_axBuf, sizeof(s_axBuf),
			"%s: expected nullptr but got '%s'", szWhich, szActual);
		g_szFailureReason = s_axBuf;
		return false;
	}
}

static void Setup_TutorialHint()
{
	g_bPassed = false;
	g_szFailureReason = "";

	// Case 1: run-over wins over everything else.
	if (!ExpectNull("run-over",
		DPHUDController_Behaviour::BuildTutorialHintForState(
			/*bPossessed=*/true, DP_ItemTag::Iron, AelfricState::Pursuing,
			/*bRunOver=*/true)))
		return;

	// Case 2: no possession.
	if (!ExpectSubstring("not-possessed",
		DPHUDController_Behaviour::BuildTutorialHintForState(
			/*bPossessed=*/false, DP_ItemTag::None, AelfricState::Calm,
			/*bRunOver=*/false),
		"possess"))
		return;

	// Case 3: priest pursuing -- takes precedence over the held-item branches.
	if (!ExpectSubstring("pursuing",
		DPHUDController_Behaviour::BuildTutorialHintForState(
			/*bPossessed=*/true, DP_ItemTag::Iron, AelfricState::Pursuing,
			/*bRunOver=*/false),
		"Aelfric sees you"))
		return;

	// Case 4: holding any of the 5 objectives.
	for (int iObj = 0; iObj < 5; ++iObj)
	{
		const DP_ItemTag eTag = static_cast<DP_ItemTag>(
			static_cast<int>(DP_ItemTag::Objective1) + iObj);
		if (!ExpectSubstring("holding-objective",
			DPHUDController_Behaviour::BuildTutorialHintForState(
				/*bPossessed=*/true, eTag, AelfricState::Calm,
				/*bRunOver=*/false),
			"pentagram"))
			return;
	}

	// Case 5: holding SkeletonKey.
	if (!ExpectSubstring("holding-skeleton-key",
		DPHUDController_Behaviour::BuildTutorialHintForState(
			/*bPossessed=*/true, DP_ItemTag::SkeletonKey, AelfricState::Calm,
			/*bRunOver=*/false),
		"Skeleton Key"))
		return;

	// Case 6: holding Iron.
	if (!ExpectSubstring("holding-iron",
		DPHUDController_Behaviour::BuildTutorialHintForState(
			/*bPossessed=*/true, DP_ItemTag::Iron, AelfricState::Calm,
			/*bRunOver=*/false),
		"forge"))
		return;

	// Case 7: holding reagents (BellSoul + BogWater both route to the
	// generic 'Reagent in hand' hint; the per-reagent detail is on the
	// ReagentHelp line instead).
	if (!ExpectSubstring("holding-bellsoul",
		DPHUDController_Behaviour::BuildTutorialHintForState(
			/*bPossessed=*/true, DP_ItemTag::BellSoul, AelfricState::Calm,
			/*bRunOver=*/false),
		"Reagent"))
		return;
	if (!ExpectSubstring("holding-bogwater",
		DPHUDController_Behaviour::BuildTutorialHintForState(
			/*bPossessed=*/true, DP_ItemTag::BogWater, AelfricState::Calm,
			/*bRunOver=*/false),
		"Reagent"))
		return;

	// Case 8: priest suspicious (no held item).
	if (!ExpectSubstring("suspicious",
		DPHUDController_Behaviour::BuildTutorialHintForState(
			/*bPossessed=*/true, DP_ItemTag::None, AelfricState::Suspicious,
			/*bRunOver=*/false),
		"suspicious"))
		return;

	// Case 9: default fall-through.
	if (!ExpectSubstring("default",
		DPHUDController_Behaviour::BuildTutorialHintForState(
			/*bPossessed=*/true, DP_ItemTag::None, AelfricState::Calm,
			/*bRunOver=*/false),
		"Find an objective"))
		return;

	// Sanity: holding a non-reagent / non-special tag (Spike, Wood)
	// falls through to the default hint, NOT the reagent line.
	if (!ExpectSubstring("default-with-non-reagent-tag",
		DPHUDController_Behaviour::BuildTutorialHintForState(
			/*bPossessed=*/true, DP_ItemTag::Spike, AelfricState::Calm,
			/*bRunOver=*/false),
		"Find an objective"))
		return;

	g_bPassed = true;
	std::printf("[P2HUD_TutorialHint] all 9 dispatch cases passed\n");
	std::fflush(stdout);
}

static bool Step_TutorialHint(int /*iFrame*/)
{
	return false;
}

static bool Verify_TutorialHint()
{
	if (!g_bPassed)
	{
		Zenith_Log(LOG_CATEGORY_AI, "P2HUD_TutorialHint: %s", g_szFailureReason);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xTutorialHintTest = {
	"Test_P2HUD_TutorialHint",
	&Setup_TutorialHint,
	&Step_TutorialHint,
	&Verify_TutorialHint,
	30
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xTutorialHintTest);

#endif // ZENITH_INPUT_SIMULATOR
