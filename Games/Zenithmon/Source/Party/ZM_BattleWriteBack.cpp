#include "Zenith.h"

#include "Zenithmon/Source/Party/ZM_BattleWriteBack.h"

#include "Zenithmon/Source/Party/ZM_Monster.h"                // ZM_ApplyBattleMonsterToRecord (SC1 leaf) + ZM_MonsterFromBattleMonster (SC4)
#include "Zenithmon/Source/Battle/ZM_BattleDirectorCore.h"    // GetWinner / GetEngine
#include "Zenithmon/Source/Battle/ZM_BattleEngine.h"          // GetState() / GetEventCount() / GetEvent()
#include "Zenithmon/Source/Battle/ZM_BattleEvent.h"           // ZM_BATTLE_EVENT_CATCH_RESULT (SC4 catch scan)
#include "Zenithmon/Source/Battle/ZM_BattleState.h"           // Side(...).Active()
#include "Zenithmon/Source/Data/ZM_StoryFlags.h"              // ZM_SetStoryFlag / ZM_IsStoryFlagSet (SC5 trainer payout)

// ============================================================================
// ZM_BattleWriteBack -- pure battle-result persistence (S5 item 5). Routes a resolved
// battle to the GameState by winner (SC3 win write-back, SC4 catch add, SC5 loss->whiteout
// + flee vitals). The engine already mutated the player battle monster in place; these
// helpers copy the relevant slice into the persistent party lead and route catches
// party-first into box overflow. Single-lead. No ECS / graphics / I/O.
// ============================================================================

ZM_BATTLE_RESULT_ACTION ZM_ClassifyBattleResult(ZM_SIDE eWinner, bool bLeadFainted)
{
	if (eWinner == ZM_SIDE_PLAYER) { return ZM_BRA_WRITE_BACK_WIN; }
	if (eWinner == ZM_SIDE_ENEMY)  { return ZM_BRA_WHITEOUT; }
	// ZM_SIDE_COUNT is a successful flee OR a draw/double-KO. A real flee leaves the lead
	// alive; a COUNT whose lead fainted is a party wipe -> whiteout (same as a loss).
	return bLeadFainted ? ZM_BRA_WHITEOUT : ZM_BRA_PERSIST_VITALS;
}

void ZM_ApplyBattleResultToParty(ZM_Party& xPartyInOut, u_int uLeadSlot, ZM_SIDE eWinner,
                                 const ZM_BattleMonster& xFinalLead)
{
	if (eWinner != ZM_SIDE_PLAYER)                                  // win-only persistence (loss/flee/draw no-op)
	{
		return;
	}
	if (xPartyInOut.IsEmpty() || uLeadSlot >= xPartyInOut.Count())  // guards: never index an empty/stale slot
	{
		return;
	}
	ZM_ApplyBattleMonsterToRecord(xFinalLead, xPartyInOut.Get(uLeadSlot));   // SC1 leaf copies the mutable state
}

void ZM_ApplyCatchToGameState(ZM_GameState& xGameStateInOut, bool bCaught, const ZM_BattleMonster& xCaught)
{
	if (!bCaught)                                            // a failed catch persists nothing
	{
		return;
	}
	xGameStateInOut.MarkCaught(xCaught.m_eSpecies);          // dex: ALWAYS, even if every storage slot is full
	const ZM_Monster xRecord = ZM_MonsterFromBattleMonster(xCaught);
	if (!xGameStateInOut.m_xParty.Add(xRecord))              // party-first; Add is transactional at the cap
	{
		xGameStateInOut.m_xBoxes.StoreFirstFree(xRecord);      // first free box slot; full storage is a strict no-op
	}
}

void ZM_ApplyBattleResultToParty(ZM_GameState& xGameStateInOut, const ZM_BattleDirectorCore& xCore)
{
	const ZM_SIDE eWinner = xCore.GetWinner();
	const ZM_BattleMonster& xLead = xCore.GetEngine().GetState().Side(ZM_SIDE_PLAYER).Active();
	// Single-lead: the active IS the party lead, so its faint == a full-party wipe. (S6's
	// multi-member battles will need ZM_Party::AllFainted() here instead.)
	const bool bLeadFainted = (xLead.m_uCurHP == 0u);

	switch (ZM_ClassifyBattleResult(eWinner, bLeadFainted))
	{
	case ZM_BRA_WRITE_BACK_WIN:
	{
		// WIN: carry the lead's mutable post-battle state (level/exp/EVs/moves+PP + damaged
		// HP) back into the party lead. The per-slot leaf re-checks win-only, so passing the
		// (always PLAYER here) eWinner keeps it unchanged.
		ZM_ApplyBattleResultToParty(xGameStateInOut.m_xParty, xGameStateInOut.m_xParty.LeadIndex(), eWinner, xLead);

		// SC4 catch add: a successful capture ends the wild battle with the PLAYER as winner,
		// so the lead write-back above already fired (carrying the lead's damaged HP). Scan the
		// engine event stream for a CATCH_RESULT that reports caught (m_iAmount == 1) and, when
		// one fired, add the caught wild monster -- the ENEMY active at resolve -- party-first,
		// then into box overflow, and mark the dex. A non-catch battle is a strict no-op there.
		const ZM_BattleEngine& xEngine = xCore.GetEngine();
		bool bCaught = false;
		for (u_int i = 0u; i < xEngine.GetEventCount(); ++i)
		{
			const ZM_BattleEvent& xEv = xEngine.GetEvent(i);
			if (xEv.m_eKind == ZM_BATTLE_EVENT_CATCH_RESULT && xEv.m_iAmount == 1) { bCaught = true; break; }
		}
		ZM_ApplyCatchToGameState(xGameStateInOut, bCaught, xEngine.GetState().Side(ZM_SIDE_ENEMY).Active());
		break;
	}
	case ZM_BRA_WHITEOUT:
		// PARTY WIPE (SC5): an ENEMY loss OR a COUNT draw/double-KO that fainted the lead.
		// Latch the whiteout; the heal + warp to Dawnmere are the manager's job
		// (ZM_GameStateManager::OnUpdate consumes this flag) -- healing here would double-heal
		// and race the battle-round-trip resume. The heal happens BEFORE the player is
		// unfrozen, so a 0-HP lead can never re-enter grass; hence no fainted-lead guard.
		xGameStateInOut.m_bPendingWhiteout = true;
		break;

	case ZM_BRA_PERSIST_VITALS:
		// A real FLEE (COUNT, lead alive): persist ONLY the lead's per-battle vitals
		// (curHP / spent PP / major status); a flee awards no level/exp/EV progression.
		// Guard the empty party (Lead() would otherwise index slot 0 of an empty array).
		if (!xGameStateInOut.m_xParty.IsEmpty())
		{
			ZM_PersistBattleVitalsToRecord(xLead, xGameStateInOut.m_xParty.Lead());
		}
		break;
	}
}

// ============================================================================
// S7 item 3 SC5 -- the TRAINER payout, extended by S8 G1-3 (ZM-70) to the badge +
// take-home item. APPENDED beside the shipped write-back; nothing above this line
// is edited, so the whiteout latch, the catch scan and the lead write-back behave
// identically for the (byte-frozen) wild path.
// ============================================================================

ZM_TrainerRewardResult ZM_ApplyTrainerResultToGameState(ZM_GameState& xGameStateInOut,
	const ZM_TrainerData& xRow, ZM_SIDE eWinner, bool bLeadFainted)
{
	ZM_TrainerRewardResult xResult;

	// FAIL CLOSED on everything that is not an outright player win. Reusing the
	// shipped classifier is load-bearing: a ZM_SIDE_COUNT draw whose lead fainted is
	// a party WIPE, not a stalemate, and must never pay a prize. This primitive is
	// called both directly (a hand-built row) and via the by-id overload below (which
	// already classified once) -- re-classifying here is what keeps this function
	// TOTAL and safe on its own, per the header's contract, rather than trusting a
	// caller to have checked first.
	if (ZM_ClassifyBattleResult(eWinner, bLeadFainted) != ZM_BRA_WRITE_BACK_WIN)
	{
		return xResult;
	}

	// Through AddMoney, never a raw m_uMoney write: it is the sole enforcer of
	// uZM_MONEY_CAP and it is headroom-first, so a prize can never wrap the purse.
	// The credit is REPORTED as the observed delta, so a saturated credit reports
	// what actually landed rather than what was owed.
	const u_int uMoneyBefore = xGameStateInOut.m_uMoney;
	xGameStateInOut.AddMoney(xRow.m_uPrizeMoney);
	xResult.m_uMoneyCredited = xGameStateInOut.m_uMoney - uMoneyBefore;

	const bool bAlreadySet = ZM_IsStoryFlagSet(xGameStateInOut, xRow.m_eDefeatFlag);
	const bool bWritten    = ZM_SetStoryFlag(xGameStateInOut, xRow.m_eDefeatFlag, true);
	xResult.m_bFlagNewlySet = bWritten && !bAlreadySet;

	// S8 G1-3 (ZM-70): the badge + take-home item, read straight off the row. Both
	// ZM_GameState::AwardBadge and ZM_Bag::Add are already TOTAL over their own
	// sentinel (ZM_BADGE_NONE aliases ZM_BADGE_ID_COUNT past AwardBadge's valid
	// range; ZM_ITEM_NONE aliases ZM_ITEM_COUNT past Add's valid range), so a row
	// that names neither -- every row but Fenna's, today -- takes both calls as
	// total, silent no-ops with no branch needed here.
	const bool bBadgeAlreadyHeld = xGameStateInOut.HasBadge((u_int)xRow.m_eBadgeReward);
	const bool bBadgeAccepted    = xGameStateInOut.AwardBadge((u_int)xRow.m_eBadgeReward);
	xResult.m_bBadgeNewlyAwarded = bBadgeAccepted && !bBadgeAlreadyHeld;

	// The item DEDUPLICATES on a repeat win -- unlike the prize money, and for a
	// DIFFERENT reason than the badge (ZM-70 review correction: BLOCKING). m_xBag IS
	// its own record of "does the player already own this" (ZM_Bag::Has), so the
	// grant is gated on that read with ZERO new GameState state; ZM-D-135 never
	// governs this column because there is nothing here for it to forbid. Has() is
	// TOTAL over ZM_ITEM_NONE (GetCount range-checks before touching the item
	// table), so a NONE row's bAlreadyHasItem is always false and Add() still
	// rejects the sentinel itself -- m_bItemGranted stays false exactly as it did
	// before this gate existed.
	const bool bAlreadyHasItem = xGameStateInOut.m_xBag.Has(xRow.m_eItemReward);
	xResult.m_bItemGranted = !bAlreadyHasItem && xGameStateInOut.m_xBag.Add(xRow.m_eItemReward, 1u);

	xResult.m_bApplied = true;
	return xResult;
}

ZM_TrainerRewardResult ZM_ApplyTrainerResultToGameState(ZM_GameState& xGameStateInOut,
	ZM_TRAINER_ID eTrainer, ZM_SIDE eWinner, bool bLeadFainted)
{
	// Same fail-closed short-circuit the primitive re-checks below, kept here too so
	// an unregistered id NEVER reaches ZM_GetTrainerData on a non-win -- that
	// accessor logs a non-fatal Zenith_Error for an unregistered id, and this path
	// must stay silent per the header's totality table.
	if (ZM_ClassifyBattleResult(eWinner, bLeadFainted) != ZM_BRA_WRITE_BACK_WIN)
	{
		return ZM_TrainerRewardResult();
	}
	if (!ZM_IsRegisteredTrainer(eTrainer))
	{
		return ZM_TrainerRewardResult();   // silent: see the header's totality table
	}
	return ZM_ApplyTrainerResultToGameState(xGameStateInOut, ZM_GetTrainerData(eTrainer), eWinner, bLeadFainted);
}

ZM_TrainerRewardResult ZM_ApplyTrainerResultToGameState(ZM_GameState& xGameStateInOut,
	ZM_TRAINER_ID eTrainer, const ZM_BattleDirectorCore& xCore)
{
	const ZM_BattleMonster& xLead = xCore.GetEngine().GetState().Side(ZM_SIDE_PLAYER).Active();
	// Single-lead (Q-2026-07-18-001): the active IS the party lead, so its faint is a
	// full-party wipe -- the same derivation the shipped overload uses.
	return ZM_ApplyTrainerResultToGameState(xGameStateInOut, eTrainer,
		xCore.GetWinner(), xLead.m_uCurHP == 0u);
}
