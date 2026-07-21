#include "Zenith.h"

#include "Zenithmon/Source/Party/ZM_BattleWriteBack.h"

#include "Zenithmon/Source/Party/ZM_Monster.h"                // ZM_ApplyBattleMonsterToRecord (SC1 leaf) + ZM_MonsterFromBattleMonster (SC4)
#include "Zenithmon/Source/Battle/ZM_BattleDirectorCore.h"    // GetWinner / GetEngine
#include "Zenithmon/Source/Battle/ZM_BattleEngine.h"          // GetState() / GetEventCount() / GetEvent()
#include "Zenithmon/Source/Battle/ZM_BattleEvent.h"           // ZM_BATTLE_EVENT_CATCH_RESULT (SC4 catch scan)
#include "Zenithmon/Source/Battle/ZM_BattleState.h"           // Side(...).Active()

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
