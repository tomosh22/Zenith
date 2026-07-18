#include "Zenith.h"

#include "Zenithmon/Source/Party/ZM_BattleWriteBack.h"

#include "Zenithmon/Source/Party/ZM_Monster.h"                // ZM_ApplyBattleMonsterToRecord (SC1 leaf)
#include "Zenithmon/Source/Battle/ZM_BattleDirectorCore.h"    // GetWinner / GetEngine
#include "Zenithmon/Source/Battle/ZM_BattleEngine.h"          // GetState()
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

void ZM_ApplyBattleResultToParty(ZM_GameState& xGameStateInOut, const ZM_BattleDirectorCore& xCore)
{
	const ZM_SIDE eWinner = xCore.GetWinner();
	const ZM_BattleMonster& xLead = xCore.GetEngine().GetState().Side(ZM_SIDE_PLAYER).Active();
	ZM_ApplyBattleResultToParty(xGameStateInOut.m_xParty, xGameStateInOut.m_xParty.LeadIndex(), eWinner, xLead);
}
