#include "Zenith.h"

#include "Zenithmon/Source/Party/ZM_BattleWriteBack.h"

#include "Zenithmon/Source/Party/ZM_Monster.h"                // ZM_ApplyBattleMonsterToRecord (SC1 leaf) + ZM_MonsterFromBattleMonster (SC4)
#include "Zenithmon/Source/Battle/ZM_BattleDirectorCore.h"    // GetWinner / GetEngine
#include "Zenithmon/Source/Battle/ZM_BattleEngine.h"          // GetState() / GetEventCount() / GetEvent()
#include "Zenithmon/Source/Battle/ZM_BattleEvent.h"           // ZM_BATTLE_EVENT_CATCH_RESULT (SC4 catch scan)
#include "Zenithmon/Source/Battle/ZM_BattleState.h"           // Side(...).Active()

// ============================================================================
// ZM_BattleWriteBack -- pure battle-result persistence (S5 item 5 SC3). Win-only,
// single-lead. The engine already mutated the player battle monster in place when
// ZM_BattleConfig::m_bAwardExp was set; these helpers copy that result into the
// persistent party lead. No ECS / graphics / I/O.
// ============================================================================

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
	xGameStateInOut.MarkCaught(xCaught.m_eSpecies);          // dex: ALWAYS, even if the party is full
	if (!xGameStateInOut.m_xParty.IsFull())                  // box storage is S7 -> a full party marks caught but does NOT add
	{
		xGameStateInOut.m_xParty.Add(ZM_MonsterFromBattleMonster(xCaught));
	}
}

void ZM_ApplyBattleResultToParty(ZM_GameState& xGameStateInOut, const ZM_BattleDirectorCore& xCore)
{
	const ZM_SIDE eWinner = xCore.GetWinner();
	const ZM_BattleMonster& xLead = xCore.GetEngine().GetState().Side(ZM_SIDE_PLAYER).Active();
	ZM_ApplyBattleResultToParty(xGameStateInOut.m_xParty, xGameStateInOut.m_xParty.LeadIndex(), eWinner, xLead);

	// SC4 catch add: a successful capture ends the wild battle with the PLAYER as winner,
	// so the lead write-back above already fired (carrying the lead's damaged HP). Scan the
	// engine event stream for a CATCH_RESULT that reports caught (m_iAmount == 1) and, when
	// one fired, add the caught wild monster -- the ENEMY active at resolve -- to the party
	// + dex. A non-catch battle finds no such event, so this is a strict no-op there.
	const ZM_BattleEngine& xEngine = xCore.GetEngine();
	bool bCaught = false;
	for (u_int i = 0u; i < xEngine.GetEventCount(); ++i)
	{
		const ZM_BattleEvent& xEv = xEngine.GetEvent(i);
		if (xEv.m_eKind == ZM_BATTLE_EVENT_CATCH_RESULT && xEv.m_iAmount == 1) { bCaught = true; break; }
	}
	ZM_ApplyCatchToGameState(xGameStateInOut, bCaught, xEngine.GetState().Side(ZM_SIDE_ENEMY).Active());
}
