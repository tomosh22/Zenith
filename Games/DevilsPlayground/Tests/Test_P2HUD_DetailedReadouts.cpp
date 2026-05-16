#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"

#include "Components/DPHUDController_Behaviour.h"
#include "Source/DevilsPlayground_Tags.h"

#include <cstdio>
#include <cstring>
#include <cmath>

// ============================================================================
// Test_P2HUD_DetailedReadouts
//
// Pins the format strings + state dispatch for the eight detailed-HUD
// readouts added on 2026-05-15. Each formatter is a pure static helper on
// DPHUDController_Behaviour (extracted from OnUpdate for testability);
// production code now calls these same helpers so the test path matches
// the runtime path exactly.
//
//   BuildMovementModeText       (sprint / quiet / jog precedence)
//   BuildVillagersAliveText     ("Vessels: N / Total")
//   BuildPriestDistanceText     ("Priest: X m" with clamp)
//   PriestDangerForDistance     (Danger <5m, Caution <15m, Calm otherwise)
//   PriestDistanceColor         (per-danger Vector4)
//   BuildRunTimerText           (mm:ss; clamps negative input)
//   BuildArchetypeText          ("Archetype: <id>"; empty hides)
//   BuildLifeNumericText        ("Life: X.X / Y.Y s"; clamps negatives)
//   BuildInteractHintText       ("F: interact with <type>"; nullptr hides)
//   ReagentHelpText             (BellSoul / BogWater / SkeletonKey copy)
//
// The colour thresholds (5m / 15m) are particularly bug-prone because
// they live as bare floats inside the function; the boundary cases below
// pin them so a regression flips the colour visibly in CI.
// ============================================================================

namespace
{
	bool g_bPassed = false;
	const char* g_szFailureReason = "";

	using PriestDanger = DPHUDController_Behaviour::PriestDanger;

	bool ContainsSubstring(const char* sz, const char* szNeedle)
	{
		return sz != nullptr && std::strstr(sz, szNeedle) != nullptr;
	}

	bool Fail(const char* sz)
	{
		g_szFailureReason = sz;
		return false;
	}

	bool ApproxEq(float a, float b, float fTol = 0.001f)
	{
		return std::fabs(a - b) < fTol;
	}

	bool ColorsMatch(const Zenith_Maths::Vector4& xA, const Zenith_Maths::Vector4& xB)
	{
		return ApproxEq(xA.x, xB.x) && ApproxEq(xA.y, xB.y)
		    && ApproxEq(xA.z, xB.z) && ApproxEq(xA.w, xB.w);
	}
}

static bool TestMovementMode()
{
	char buf[64];

	// Sprint wins ties.
	DPHUDController_Behaviour::BuildMovementModeText(buf, sizeof(buf),
		/*bSprintingNow=*/true, /*bWalkQuietNow=*/false);
	if (!ContainsSubstring(buf, "SPRINT")) return Fail("movement: sprint case missing 'SPRINT'");

	DPHUDController_Behaviour::BuildMovementModeText(buf, sizeof(buf),
		/*bSprintingNow=*/false, /*bWalkQuietNow=*/true);
	if (!ContainsSubstring(buf, "WALK QUIET")) return Fail("movement: quiet case missing 'WALK QUIET'");

	DPHUDController_Behaviour::BuildMovementModeText(buf, sizeof(buf),
		/*bSprintingNow=*/false, /*bWalkQuietNow=*/false);
	if (!ContainsSubstring(buf, "Move")) return Fail("movement: default case missing 'Move'");

	// Sprint + quiet held simultaneously -> sprint wins (matches villager
	// OnUpdate's tie-break in DPVillager_Behaviour.h ~line 235).
	DPHUDController_Behaviour::BuildMovementModeText(buf, sizeof(buf),
		/*bSprintingNow=*/true, /*bWalkQuietNow=*/true);
	if (!ContainsSubstring(buf, "SPRINT")) return Fail("movement: sprint+quiet tie should resolve to SPRINT");

	return true;
}

static bool TestVillagersAlive()
{
	char buf[64];

	DPHUDController_Behaviour::BuildVillagersAliveText(buf, sizeof(buf), 14, 17);
	if (!ContainsSubstring(buf, "14")) return Fail("vessels: missing alive count");
	if (!ContainsSubstring(buf, "17")) return Fail("vessels: missing total count");
	if (!ContainsSubstring(buf, "Vessels: ")) return Fail("vessels: missing 'Vessels: ' prefix");

	// Negative inputs clamp to 0 (defensive -- the count loop shouldn't
	// emit negatives but the formatter is the last line of defence).
	DPHUDController_Behaviour::BuildVillagersAliveText(buf, sizeof(buf), -3, -5);
	if (ContainsSubstring(buf, "-")) return Fail("vessels: negative input should clamp -- got minus sign");

	return true;
}

static bool TestPriestDistance()
{
	char buf[64];

	DPHUDController_Behaviour::BuildPriestDistanceText(buf, sizeof(buf), 12.4f);
	if (!ContainsSubstring(buf, "Priest: ")) return Fail("priest: missing prefix");
	if (!ContainsSubstring(buf, "12 m")) return Fail("priest: 12.4 should round to 12");

	DPHUDController_Behaviour::BuildPriestDistanceText(buf, sizeof(buf), -5.0f);
	if (ContainsSubstring(buf, "-")) return Fail("priest: negative input should clamp");
	if (!ContainsSubstring(buf, "0 m")) return Fail("priest: clamped negative should show 0 m");

	return true;
}

static bool TestPriestDanger()
{
	// Threshold boundaries: <5m = Danger, <15m = Caution, else Calm.
	if (DPHUDController_Behaviour::PriestDangerForDistance(0.0f)  != PriestDanger::Danger)  return Fail("danger: 0m should be Danger");
	if (DPHUDController_Behaviour::PriestDangerForDistance(4.99f) != PriestDanger::Danger)  return Fail("danger: 4.99m should be Danger");
	if (DPHUDController_Behaviour::PriestDangerForDistance(5.0f)  != PriestDanger::Caution) return Fail("danger: 5.0m should flip to Caution");
	if (DPHUDController_Behaviour::PriestDangerForDistance(10.0f) != PriestDanger::Caution) return Fail("danger: 10m should be Caution");
	if (DPHUDController_Behaviour::PriestDangerForDistance(14.99f)!= PriestDanger::Caution) return Fail("danger: 14.99m should be Caution");
	if (DPHUDController_Behaviour::PriestDangerForDistance(15.0f) != PriestDanger::Calm)    return Fail("danger: 15.0m should flip to Calm");
	if (DPHUDController_Behaviour::PriestDangerForDistance(50.0f) != PriestDanger::Calm)    return Fail("danger: 50m should be Calm");

	// Colour mapping. Hard-coded expected RGBA values mirror what the
	// HUD authoring uses for visual continuity; changing these breaks
	// the test deliberately to force a UX review.
	const Zenith_Maths::Vector4 xRed    (1.0f, 0.3f,  0.3f,  1.0f);
	const Zenith_Maths::Vector4 xAmber  (1.0f, 0.8f,  0.4f,  1.0f);
	const Zenith_Maths::Vector4 xGrey   (0.85f, 0.85f, 0.85f, 1.0f);

	if (!ColorsMatch(DPHUDController_Behaviour::PriestDistanceColor(PriestDanger::Danger),  xRed))   return Fail("colour: Danger != red");
	if (!ColorsMatch(DPHUDController_Behaviour::PriestDistanceColor(PriestDanger::Caution), xAmber)) return Fail("colour: Caution != amber");
	if (!ColorsMatch(DPHUDController_Behaviour::PriestDistanceColor(PriestDanger::Calm),    xGrey))  return Fail("colour: Calm != grey");

	return true;
}

static bool TestRunTimer()
{
	char buf[64];

	DPHUDController_Behaviour::BuildRunTimerText(buf, sizeof(buf), 0.0f);
	if (!ContainsSubstring(buf, "Time: 0:00")) return Fail("timer: zero should show 0:00");

	DPHUDController_Behaviour::BuildRunTimerText(buf, sizeof(buf), 65.7f);
	if (!ContainsSubstring(buf, "Time: 1:05")) return Fail("timer: 65.7s should show 1:05");

	DPHUDController_Behaviour::BuildRunTimerText(buf, sizeof(buf), 9.0f);
	if (!ContainsSubstring(buf, "Time: 0:09")) return Fail("timer: 9s should zero-pad to 0:09");

	DPHUDController_Behaviour::BuildRunTimerText(buf, sizeof(buf), 600.0f);
	if (!ContainsSubstring(buf, "Time: 10:00")) return Fail("timer: 10 minutes should show 10:00");

	DPHUDController_Behaviour::BuildRunTimerText(buf, sizeof(buf), -7.0f);
	if (ContainsSubstring(buf, "-")) return Fail("timer: negative input should clamp -- got minus sign");

	return true;
}

static bool TestArchetype()
{
	char buf[64];

	DPHUDController_Behaviour::BuildArchetypeText(buf, sizeof(buf), "Devout");
	if (!ContainsSubstring(buf, "Archetype: Devout")) return Fail("archetype: standard case wrong");

	// Empty id -> empty output (HUD hides the element).
	DPHUDController_Behaviour::BuildArchetypeText(buf, sizeof(buf), "");
	if (buf[0] != '\0') return Fail("archetype: empty id should produce empty string");

	// nullptr id -> empty output (defensive).
	DPHUDController_Behaviour::BuildArchetypeText(buf, sizeof(buf), nullptr);
	if (buf[0] != '\0') return Fail("archetype: nullptr id should produce empty string");

	return true;
}

static bool TestLifeNumeric()
{
	char buf[64];

	DPHUDController_Behaviour::BuildLifeNumericText(buf, sizeof(buf), 23.4f, 30.0f);
	if (!ContainsSubstring(buf, "Life: ")) return Fail("life-numeric: missing prefix");
	if (!ContainsSubstring(buf, "23.4"))  return Fail("life-numeric: missing remaining 23.4");
	if (!ContainsSubstring(buf, "30"))    return Fail("life-numeric: missing max 30");
	if (!ContainsSubstring(buf, " s"))    return Fail("life-numeric: missing seconds suffix");

	// Negative remaining clamps to 0.
	DPHUDController_Behaviour::BuildLifeNumericText(buf, sizeof(buf), -5.0f, 30.0f);
	if (ContainsSubstring(buf, "-5"))     return Fail("life-numeric: negative remaining should clamp");

	return true;
}

static bool TestInteractHint()
{
	char buf[64];

	DPHUDController_Behaviour::BuildInteractHintText(buf, sizeof(buf), "door");
	if (!ContainsSubstring(buf, "F: interact with door")) return Fail("interact: door case wrong");

	DPHUDController_Behaviour::BuildInteractHintText(buf, sizeof(buf), "noise machine");
	if (!ContainsSubstring(buf, "F: interact with noise machine")) return Fail("interact: noise machine case wrong");

	// Empty + nullptr -> empty string (HUD hides element).
	DPHUDController_Behaviour::BuildInteractHintText(buf, sizeof(buf), "");
	if (buf[0] != '\0') return Fail("interact: empty type should produce empty string");

	DPHUDController_Behaviour::BuildInteractHintText(buf, sizeof(buf), nullptr);
	if (buf[0] != '\0') return Fail("interact: nullptr type should produce empty string");

	return true;
}

static bool TestReagentHelp()
{
	// The three known reagents have specific descriptive text.
	const char* szBell = DPHUDController_Behaviour::ReagentHelpText(DP_ItemTag::BellSoul);
	if (!ContainsSubstring(szBell, "BellSoul")) return Fail("reagent: BellSoul missing label");
	if (!ContainsSubstring(szBell, "alert"))    return Fail("reagent: BellSoul missing 'alert' description");

	const char* szBog = DPHUDController_Behaviour::ReagentHelpText(DP_ItemTag::BogWater);
	if (!ContainsSubstring(szBog, "BogWater")) return Fail("reagent: BogWater missing label");
	if (!ContainsSubstring(szBog, "Evaporat") && !ContainsSubstring(szBog, "evaporat"))
		return Fail("reagent: BogWater missing evaporation description");

	const char* szKey = DPHUDController_Behaviour::ReagentHelpText(DP_ItemTag::SkeletonKey);
	if (!ContainsSubstring(szKey, "Skeleton Key")) return Fail("reagent: SkeletonKey missing label");
	if (!ContainsSubstring(szKey, "lock"))         return Fail("reagent: SkeletonKey missing 'lock' hint");

	// Non-reagent tags (Iron, Wood, Spike, Objective*, None) -> nullptr.
	if (DPHUDController_Behaviour::ReagentHelpText(DP_ItemTag::Iron)      != nullptr) return Fail("reagent: Iron should return nullptr");
	if (DPHUDController_Behaviour::ReagentHelpText(DP_ItemTag::Spike)     != nullptr) return Fail("reagent: Spike should return nullptr");
	if (DPHUDController_Behaviour::ReagentHelpText(DP_ItemTag::Wood)      != nullptr) return Fail("reagent: Wood should return nullptr");
	if (DPHUDController_Behaviour::ReagentHelpText(DP_ItemTag::None)      != nullptr) return Fail("reagent: None should return nullptr");
	if (DPHUDController_Behaviour::ReagentHelpText(DP_ItemTag::Objective1)!= nullptr) return Fail("reagent: Objective1 should return nullptr");

	return true;
}

static void Setup_DetailedReadouts()
{
	g_bPassed = false;
	g_szFailureReason = "";

	if (!TestMovementMode())   return;
	if (!TestVillagersAlive()) return;
	if (!TestPriestDistance()) return;
	if (!TestPriestDanger())   return;
	if (!TestRunTimer())       return;
	if (!TestArchetype())      return;
	if (!TestLifeNumeric())    return;
	if (!TestInteractHint())   return;
	if (!TestReagentHelp())    return;

	g_bPassed = true;
	std::printf("[P2HUD_DetailedReadouts] all 9 formatter clusters passed\n");
	std::fflush(stdout);
}

static bool Step_DetailedReadouts(int /*iFrame*/)
{
	return false;
}

static bool Verify_DetailedReadouts()
{
	if (!g_bPassed)
	{
		Zenith_Log(LOG_CATEGORY_AI, "P2HUD_DetailedReadouts: %s", g_szFailureReason);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xDetailedReadoutsTest = {
	"Test_P2HUD_DetailedReadouts",
	&Setup_DetailedReadouts,
	&Step_DetailedReadouts,
	&Verify_DetailedReadouts,
	30
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xDetailedReadoutsTest);

#endif // ZENITH_INPUT_SIMULATOR
