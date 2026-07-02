#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "SaveData/Zenith_SaveData.h"
#include "ZenithECS/Zenith_EventSystem.h"

#include "Source/PublicInterfaces.h"

#include <cstdio>

// ============================================================================
// Test_T0Knots_* — two tests pinning the per-run Knot earning contract
// (metagame v1, GDD §5.4). Both drive DP_Knots directly (no scene needed);
// the reagent hook's DP_Win call site is covered by the analyzer/playthrough
// suites which run real deliveries.
//
//   EarningAndChain — reagent tally, hand-off-chain bonus threshold, and
//                     chain reset on villager death (via the real
//                     DP_OnVillagerDied dispatch the subscription listens
//                     to).
//   RunEndBanking   — DP_OnVictory banks the tally into the persisted
//                     meta save exactly once (idempotent within a run) and
//                     records a disk write via the Zenith_SaveData test log.
// ============================================================================

namespace
{
	bool g_bKnotsPassed = false;
	const char* g_szKnotsWhy = "";
}

// ---------------------------------------------------------------------------
// 1) Earning + chain.
// ---------------------------------------------------------------------------
static void Setup_KnotsEarning()
{
	g_bKnotsPassed = false;
	g_szKnotsWhy = "";

	DP_Knots::Initialise(); // idempotent; wires the death-event chain reset
	DP_Knots::ResetForNewRun();

	// Tuning contract this test rides on: 1 Knot per reagent, +1 bonus at
	// chain length >= 2.
	if (DP_Knots::GetRunKnotTally() != 0 || DP_Knots::GetHandoffChainLength() != 0)
	{
		g_szKnotsWhy = "reset did not zero tally/chain";
		return;
	}

	// No chain: 1 Knot per reagent.
	DP_Knots::NotifyReagentInscribed();
	if (DP_Knots::GetRunKnotTally() != 1)
	{
		g_szKnotsWhy = "single reagent must earn exactly knots_per_reagent";
		return;
	}

	// Chain of 1 (< min): still no bonus.
	DP_Knots::NotifyVoluntaryHandoff();
	DP_Knots::NotifyReagentInscribed();
	if (DP_Knots::GetRunKnotTally() != 2)
	{
		g_szKnotsWhy = "chain below handoff_chain_min must not pay a bonus";
		return;
	}

	// Chain of 2 (>= min): +1 bonus per reagent.
	DP_Knots::NotifyVoluntaryHandoff();
	DP_Knots::NotifyReagentInscribed();
	if (DP_Knots::GetRunKnotTally() != 4)
	{
		g_szKnotsWhy = "chain at handoff_chain_min must pay handoff_chain_bonus";
		return;
	}

	// A villager death breaks the chain (real event dispatch — the same
	// path DPVillager's life-timer expiry takes).
	Zenith_EventDispatcher::Get().Dispatch(DP_OnVillagerDied{ Zenith_EntityID{} });
	if (DP_Knots::GetHandoffChainLength() != 0)
	{
		g_szKnotsWhy = "villager death must reset the hand-off chain";
		return;
	}
	DP_Knots::NotifyReagentInscribed();
	if (DP_Knots::GetRunKnotTally() != 5)
	{
		g_szKnotsWhy = "post-death reagent must earn base rate only";
		return;
	}

	// Hound stub: contributes nothing (hounds are post-MVP).
	DP_Knots::NotifyHoundNeutralised();
	if (DP_Knots::GetRunKnotTally() != 5)
	{
		g_szKnotsWhy = "hound stub must contribute 0 Knots";
		return;
	}

	DP_Knots::ResetForNewRun();
	g_bKnotsPassed = true;
}

// ---------------------------------------------------------------------------
// 2) Run-end banking.
// ---------------------------------------------------------------------------
static void Setup_KnotsBanking()
{
	g_bKnotsPassed = false;
	g_szKnotsWhy = "";

	DP_Knots::Initialise();
	DP_Knots::ResetForNewRun();
	Zenith_SaveData::ClearForTest();

	// Seed a known meta balance without disk.
	DP_MetaState xSeed;
	xSeed.m_uKnotBalance = 10u;
	DP_MetaSave::SetCachedForTest(xSeed);

	DP_Knots::NotifyReagentInscribed();
	DP_Knots::NotifyReagentInscribed();
	DP_Knots::NotifyReagentInscribed();

	// Victory event banks the tally...
	Zenith_EventDispatcher::Get().Dispatch(DP_OnVictory{ Zenith_EntityID{}, Zenith_EntityID{} });
	if (!DP_Knots::HasBankedThisRun())
	{
		g_szKnotsWhy = "DP_OnVictory must bank the run tally";
		return;
	}
	const DP_MetaState& xAfter = DP_MetaSave::Cached();
	if (xAfter.m_uKnotBalance != 13u || xAfter.m_uEarnedUnspentKnotsLastRun != 3u)
	{
		g_szKnotsWhy = "banked balance/last-run stamp mismatch (expected 13 / 3)";
		return;
	}
	// ... and persists it.
	const auto& axWritten = Zenith_SaveData::GetWrittenSlotsForTest();
	bool bMetaSlotWritten = false;
	for (uint32_t u = 0; u < axWritten.GetSize(); ++u)
	{
		if (axWritten.Get(u).m_strSlotName == DP_MetaSave::SlotName()) bMetaSlotWritten = true;
	}
	if (!bMetaSlotWritten)
	{
		g_szKnotsWhy = "banking must persist the meta slot to Zenith_SaveData";
		return;
	}

	// Idempotent within a run: a second terminal event banks nothing more.
	Zenith_EventDispatcher::Get().Dispatch(DP_OnVictory{ Zenith_EntityID{}, Zenith_EntityID{} });
	if (DP_MetaSave::Cached().m_uKnotBalance != 13u)
	{
		g_szKnotsWhy = "a run must bank at most once";
		return;
	}

	// Test hygiene: banking wrote a REAL slot file — remove it so later
	// tests / fresh processes don't inherit this test's balance.
	Zenith_SaveData::DeleteSlot(DP_MetaSave::SlotName());
	Zenith_SaveData::ClearForTest();
	DP_MetaSave::InvalidateCacheForTest();
	DP_Knots::ResetForNewRun();
	g_bKnotsPassed = true;
}

static bool Step_Knots(int /*iFrame*/) { return false; }

static bool Verify_Knots()
{
	if (!g_bKnotsPassed)
	{
		Zenith_Log(LOG_CATEGORY_AI, "Knots test failed: %s", g_szKnotsWhy);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xKnotsEarningTest = {
	"Test_T0Knots_EarningAndChain",
	&Setup_KnotsEarning, &Step_Knots, &Verify_Knots, 60
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xKnotsEarningTest);

static const Zenith_AutomatedTest g_xKnotsBankingTest = {
	"Test_T0Knots_RunEndBanking",
	&Setup_KnotsBanking, &Step_Knots, &Verify_Knots, 60
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xKnotsBankingTest);

#endif // ZENITH_INPUT_SIMULATOR
