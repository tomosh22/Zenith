#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "ZenithECS/Zenith_EventSystem.h"

#include "Source/PublicInterfaces.h"
#include "Source/DPTutorial.h"
#include "Source/DevilsPlayground_Tags.h"
#include "Maths/Zenith_Maths.h"

#include <cstdio>
#include <cstring>

// ============================================================================
// Test_P5Tutorial_FirstEncounters (2026-05-21)
//
// Pins the DP_Tutorial first-encounter dispatch chain. Each gameplay
// event that should produce a first-time tip:
//   1. Triggers the matching Kind on the rising-edge (and ONLY the
//      rising-edge -- subsequent events of the same kind no-op).
//   2. Surfaces the right tip text via GetActiveTipText.
//   3. Counts down via Tick + clears when the duration expires.
//
// What this catches:
//   * A DP_On<Foo> event missing its tutorial subscription (regression:
//     new event added, tutorial table not extended).
//   * Show() not being idempotent for already-shown kinds (would cause
//     tips to repeat).
//   * Tick not clearing the active tip on timer expiry.
//   * ResetForNewRun not clearing shown-flags (would prevent the next
//     test in a batch from firing tips).
//
// Tests run in pure-namespace mode (no scene load) -- the dispatcher is
// process-global and all the events used are pure structs.
// ============================================================================

namespace
{
	bool g_bPassed = false;
	const char* g_szFailureReason = nullptr;

	bool ContainsAny(const char* szText, const char* szNeedle)
	{
		return szText != nullptr && std::strstr(szText, szNeedle) != nullptr;
	}
}

static void Setup_P5TutorialFirstEncounters()
{
	g_bPassed = false;
	g_szFailureReason = nullptr;
	DP_Tutorial::ResetForNewRun();
}

static bool Step_P5TutorialFirstEncounters(int /*iFrame*/)
{
	auto& xDispatcher = Zenith_EventDispatcher::Get();
	Zenith_EntityID xDummy;

	// ----- FirstPossession via DP_OnPossessionChanged -----
	xDispatcher.Dispatch(DP_OnPossessionChanged{ INVALID_ENTITY_ID, INVALID_ENTITY_ID });
	if (DP_Tutorial::IsTipShown(DP_Tutorial::Kind::FirstPossession))
	{
		g_szFailureReason = "FirstPossession fired on un-possess (invalid new villager)";
		return false;
	}
	xDispatcher.Dispatch(DP_OnPossessionChanged{ INVALID_ENTITY_ID, xDummy });
	// Wait -- both fields are INVALID. We need a valid villager for the
	// tip to fire. Construct one manually -- entity IDs are public,
	// just inject a non-default value into the index/gen pair.
	Zenith_EntityID xMockValid;
	// Hack: bit-pattern a "valid" ID by setting both index and gen to 1.
	// This works because IsValid checks gen != 0 + index < UINT32_MAX.
	xMockValid.m_uIndex = 1;
	xMockValid.m_uGeneration = 1;
	xDispatcher.Dispatch(DP_OnPossessionChanged{ INVALID_ENTITY_ID, xMockValid });
	if (!DP_Tutorial::IsTipShown(DP_Tutorial::Kind::FirstPossession))
	{
		g_szFailureReason = "FirstPossession didn't fire on valid new villager";
		return false;
	}

	// ----- FirstIronPickup -----
	xDispatcher.Dispatch(DP_OnItemPickedUp{ xMockValid, xMockValid, DP_ItemTag::Iron });
	if (!DP_Tutorial::IsTipShown(DP_Tutorial::Kind::FirstIronPickup))
	{
		g_szFailureReason = "FirstIronPickup didn't fire on Iron pickup";
		return false;
	}
	// Re-dispatch -- should NOT increment shown-count (already shown).
	const uint32_t uCountBefore = DP_Tutorial::GetShownCount();
	xDispatcher.Dispatch(DP_OnItemPickedUp{ xMockValid, xMockValid, DP_ItemTag::Iron });
	if (DP_Tutorial::GetShownCount() != uCountBefore)
	{
		g_szFailureReason = "FirstIronPickup re-fired on second Iron pickup -- Show should be idempotent";
		return false;
	}

	// ----- FirstObjectivePickup -----
	xDispatcher.Dispatch(DP_OnItemPickedUp{ xMockValid, xMockValid, DP_ItemTag::Objective1 });
	if (!DP_Tutorial::IsTipShown(DP_Tutorial::Kind::FirstObjectivePickup))
	{
		g_szFailureReason = "FirstObjectivePickup didn't fire on Objective1 pickup";
		return false;
	}

	// ----- FirstKeyCrafted -----
	xDispatcher.Dispatch(DP_OnForgeCrafted{ xMockValid, xMockValid, xMockValid });
	if (!DP_Tutorial::IsTipShown(DP_Tutorial::Kind::FirstKeyCrafted))
	{
		g_szFailureReason = "FirstKeyCrafted didn't fire on ForgeCrafted";
		return false;
	}

	// ----- FirstLockedDoor -----
	xDispatcher.Dispatch(DP_OnDoorLockRejected{ xMockValid, xMockValid, DP_ItemTag::Key });
	if (!DP_Tutorial::IsTipShown(DP_Tutorial::Kind::FirstLockedDoor))
	{
		g_szFailureReason = "FirstLockedDoor didn't fire on DoorLockRejected";
		return false;
	}

	// ----- FirstDoorUnlocked -----
	xDispatcher.Dispatch(DP_OnDoorOpened{ xMockValid, xMockValid });
	if (!DP_Tutorial::IsTipShown(DP_Tutorial::Kind::FirstDoorUnlocked))
	{
		g_szFailureReason = "FirstDoorUnlocked didn't fire on DoorOpened";
		return false;
	}

	// ----- FirstObjectiveDelivered -----
	xDispatcher.Dispatch(DP_OnObjectivePlaced{ xMockValid, xMockValid, 0 });
	if (!DP_Tutorial::IsTipShown(DP_Tutorial::Kind::FirstObjectiveDelivered))
	{
		g_szFailureReason = "FirstObjectiveDelivered didn't fire on ObjectivePlaced";
		return false;
	}

	// ----- FirstPriestSpotted vs FirstPriestInvestigate -----
	xDispatcher.Dispatch(DP_OnPriestAlerted{
		xMockValid, DP_PriestAlertKind::HeardNoise, Zenith_Maths::Vector3(0.0f) });
	if (!DP_Tutorial::IsTipShown(DP_Tutorial::Kind::FirstPriestInvestigate))
	{
		g_szFailureReason = "FirstPriestInvestigate didn't fire on HeardNoise alert";
		return false;
	}
	xDispatcher.Dispatch(DP_OnPriestAlerted{
		xMockValid, DP_PriestAlertKind::SawTarget, Zenith_Maths::Vector3(0.0f) });
	if (!DP_Tutorial::IsTipShown(DP_Tutorial::Kind::FirstPriestSpotted))
	{
		g_szFailureReason = "FirstPriestSpotted didn't fire on SawTarget alert";
		return false;
	}

	// ----- FirstBellSoulRing via DP_OnBellRing -----
	xDispatcher.Dispatch(DP_OnBellRing{
		xMockValid, xMockValid, Zenith_Maths::Vector3(0.0f) });
	if (!DP_Tutorial::IsTipShown(DP_Tutorial::Kind::FirstBellSoulRing))
	{
		g_szFailureReason = "FirstBellSoulRing didn't fire on BellRing";
		return false;
	}

	// ----- FirstBogWaterEvaporate -----
	xDispatcher.Dispatch(DP_OnItemEvaporated{
		xMockValid, DP_ItemTag::BogWater, Zenith_Maths::Vector3(0.0f) });
	if (!DP_Tutorial::IsTipShown(DP_Tutorial::Kind::FirstBogWaterEvaporate))
	{
		g_szFailureReason = "FirstBogWaterEvaporate didn't fire on ItemEvaporated{BogWater}";
		return false;
	}

	// ----- Programmatic triggers -----
	DP_Tutorial::TriggerIfFirstTime(DP_Tutorial::Kind::FirstSprintUse);
	if (!DP_Tutorial::IsTipShown(DP_Tutorial::Kind::FirstSprintUse))
	{
		g_szFailureReason = "FirstSprintUse programmatic trigger didn't fire";
		return false;
	}
	DP_Tutorial::TriggerIfFirstTime(DP_Tutorial::Kind::FirstWalkQuietUse);
	if (!DP_Tutorial::IsTipShown(DP_Tutorial::Kind::FirstWalkQuietUse))
	{
		g_szFailureReason = "FirstWalkQuietUse programmatic trigger didn't fire";
		return false;
	}

	// ----- Active tip text + Tick decay -----
	const char* szActive = DP_Tutorial::GetActiveTipText();
	if (szActive == nullptr)
	{
		g_szFailureReason = "GetActiveTipText returned null after firing multiple tips";
		return false;
	}
	const float fStart = DP_Tutorial::GetActiveTipRemaining();
	if (fStart <= 0.0f)
	{
		g_szFailureReason = "GetActiveTipRemaining was zero post-fire";
		return false;
	}
	// Tick by half the tip duration; remaining should fall accordingly.
	DP_Tutorial::Tick(DP_Tutorial::kDefaultTipDurationSeconds * 0.5f);
	const float fMid = DP_Tutorial::GetActiveTipRemaining();
	if (!(fMid > 0.0f && fMid < fStart))
	{
		g_szFailureReason = "Tick(0.5x) didn't reduce the remaining time";
		return false;
	}
	// Tick past the end -- text should clear.
	DP_Tutorial::Tick(DP_Tutorial::kDefaultTipDurationSeconds);
	if (DP_Tutorial::GetActiveTipText() != nullptr)
	{
		g_szFailureReason = "Active tip text didn't clear after timer expiry";
		return false;
	}

	// ----- ResetForNewRun -----
	DP_Tutorial::ResetForNewRun();
	if (DP_Tutorial::GetShownCount() != 0)
	{
		g_szFailureReason = "ResetForNewRun didn't zero shown-count";
		return false;
	}
	if (DP_Tutorial::IsTipShown(DP_Tutorial::Kind::FirstIronPickup))
	{
		g_szFailureReason = "ResetForNewRun didn't clear FirstIronPickup flag";
		return false;
	}

	// ----- Per-kind tip text resolves -----
	if (!ContainsAny(DP_Tutorial::GetTipTextForKind(DP_Tutorial::Kind::FirstLockedDoor),
		"Key"))
	{
		g_szFailureReason = "FirstLockedDoor tip text doesn't mention 'Key'";
		return false;
	}
	if (!ContainsAny(DP_Tutorial::GetTipTextForKind(DP_Tutorial::Kind::FirstSprintUse),
		"life"))
	{
		g_szFailureReason = "FirstSprintUse tip text doesn't mention 'life' cost";
		return false;
	}

	g_bPassed = true;
	std::printf("[P5Tutorial] all 14 first-encounter cases passed; total shown = %u\n",
		DP_Tutorial::GetShownCount());
	std::fflush(stdout);
	return false;
}

static bool Verify_P5TutorialFirstEncounters()
{
	if (!g_bPassed)
	{
		Zenith_Log(LOG_CATEGORY_AI, "P5Tutorial: %s",
			g_szFailureReason != nullptr ? g_szFailureReason : "(no failure recorded)");
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP5TutorialTest = {
	"Test_P5Tutorial_FirstEncounters",
	&Setup_P5TutorialFirstEncounters,
	&Step_P5TutorialFirstEncounters,
	&Verify_P5TutorialFirstEncounters,
	10
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP5TutorialTest);

#endif // ZENITH_INPUT_SIMULATOR
