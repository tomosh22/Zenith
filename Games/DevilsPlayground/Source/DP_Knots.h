#pragma once

#include <cstdint>

// ============================================================================
// DP_Knots (2026-07-01, metagame v1) — per-run Knot-currency earning.
//
// GDD §5.4: Knots are the run currency, earned 1 per reagent inscribed
// (plus a hand-off-chain bonus for stylish voluntary possession relays),
// spent between Nights at the Liminal's hermit shrines. This namespace
// owns the per-run tally + the hand-off chain counter; the persistent
// balance lives in DP_MetaSave.
//
// Earning rules (all values via Tuning.json "metagame" keys):
//   * Reagent inscribed: +knots_per_reagent, plus +handoff_chain_bonus
//     when the live hand-off chain length >= handoff_chain_min.
//   * Hand-off chain: +1 per voluntary possession switch away from a
//     still-living villager (CommitVoluntaryPossession); reset to 0 when
//     any villager dies (the chain is only "alive" while every prior
//     vessel survived).
//   * Hounds neutralised: stubbed at 0 — hound gameplay does not exist
//     yet (GDD escalation modifiers are post-MVP); the hook exists so the
//     earning schema won't need to change when hounds land.
//
// Banking: on the run-end events (DP_OnVictory OR DP_OnRunLost — reagents
// already inscribed count even on a loss) the tally is added to the
// persisted DP_MetaSave balance exactly once per run and saved to disk.
//
// State lives in DP_Knots.cpp's anonymous namespace; reset per run by
// DPProcLevelBootstrap::OnAwake and between batched tests by the
// RegisterBetweenTestsHook lambda.
// ============================================================================
namespace DP_Knots
{
	// Subscribes the run-end banking + chain-reset event handlers
	// (captureless lambdas; process-lifetime). Called once from
	// Project_RegisterGameComponents; idempotent.
	void Initialise();

	// +1 Knot (+chain bonus). Called by DP_Win::NotifyObjectiveCollected
	// for each NEWLY-inscribed reagent.
	void NotifyReagentInscribed();

	// Voluntary possession switch away from a living villager.
	void NotifyVoluntaryHandoff();

	// Future-proofing stub: no hound gameplay exists — always contributes 0.
	void NotifyHoundNeutralised();

	uint32_t GetRunKnotTally();
	uint32_t GetHandoffChainLength();

	// Adds the tally to DP_MetaSave's balance + stamps
	// m_uEarnedUnspentKnotsLastRun, persists to disk, and latches so a run
	// banks at most once. Returns the number of Knots banked. Normally
	// driven by the run-end event subscriptions in Initialise().
	uint32_t BankRunTallyIntoMetaSave();
	bool HasBankedThisRun();

	// New run / between-tests reset: tally, chain, banked latch.
	void ResetForNewRun();
}
