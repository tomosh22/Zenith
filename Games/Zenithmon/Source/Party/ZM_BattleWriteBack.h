#pragma once

#include "Zenithmon/Source/Party/ZM_GameState.h"        // ZM_GameState, ZM_Party (write-back target)
#include "Zenithmon/Source/Battle/ZM_BattleMonster.h"    // ZM_BattleMonster (the post-battle instance)
#include "Zenithmon/Source/Battle/ZM_BattleTypes.h"      // ZM_SIDE (winner side)

class ZM_BattleDirectorCore;   // read the resolved battle via GetWinner() + GetEngine() (full type in the .cpp)

// ============================================================================
// ZM_BattleWriteBack -- the S5 item 5 battle-result persistence (SC3 win write-back,
// SC4 catch add, SC5 loss->whiteout + flee vitals). The engine has already mutated the
// player's ZM_BattleMonster IN PLACE; these pure helpers route the outcome into the
// persistent GameState by winner: PLAYER wins -> full lead write-back (+ party/box catch); a
// party wipe (ENEMY, or a COUNT draw whose lead fainted) -> latch the whiteout; a real
// flee (COUNT, lead alive) -> persist the lead's vitals only. SINGLE-LEAD (Q-2026-07-18-001).
// Pure: no ECS, no graphics, no I/O; compiled in ALL configs.
// ============================================================================

// Write the resolved battle lead back into the party. Strict no-op unless the PLAYER
// won (loss/flee/draw persist nothing) and uLeadSlot is a live party index. On a win
// it copies the mutable post-battle state (level, exp, EVs, moves+PP, curHP, status)
// into slot uLeadSlot via ZM_ApplyBattleMonsterToRecord.
void ZM_ApplyBattleResultToParty(ZM_Party& xPartyInOut, u_int uLeadSlot, ZM_SIDE eWinner,
                                 const ZM_BattleMonster& xFinalLead);

// The persistence action for a resolved battle, from the winner + whether the player's
// (single) lead fainted. CRITICAL: the engine returns ZM_SIDE_COUNT for BOTH a successful
// flee AND a DRAW/double-KO. A real flee always leaves the lead ALIVE (the flee resolves in
// the pre-move phase, before an end-of-turn/recoil chip can faint it), so a COUNT outcome
// whose lead FAINTED is a party wipe -> WHITEOUT, exactly like an ENEMY win. Only a COUNT
// with a live lead is a true flee. Pure (winner + a bool); unit-tested directly.
enum ZM_BATTLE_RESULT_ACTION : u_int
{
	ZM_BRA_WRITE_BACK_WIN,   // PLAYER won: full lead write-back (+ SC4 catch scan)
	ZM_BRA_WHITEOUT,         // party wipe: ENEMY won, OR a COUNT draw whose lead fainted
	ZM_BRA_PERSIST_VITALS,   // real flee (COUNT, lead alive): persist HP/PP/status only
};
ZM_BATTLE_RESULT_ACTION ZM_ClassifyBattleResult(ZM_SIDE eWinner, bool bLeadFainted);

// Convenience overload: resolves the winner + the player's final active monster from the
// core and routes the outcome to the game state via ZM_ClassifyBattleResult:
//   * WRITE_BACK_WIN -> full lead write-back (levels/exp/EVs/moves+PP/HP) + the SC4 catch
//                       scan (adds a caught wild monster to the party + dex).
//   * WHITEOUT       -> latch m_bPendingWhiteout (SC5); the manager heals + warps. Covers
//                       an ENEMY loss AND a COUNT draw/double-KO that wiped the lead.
//   * PERSIST_VITALS -> persist ONLY the lead's per-battle vitals (curHP/PP/status) via
//                       ZM_PersistBattleVitalsToRecord -- a flee awards no progression.
void ZM_ApplyBattleResultToParty(ZM_GameState& xGameStateInOut, const ZM_BattleDirectorCore& xCore);

// Add a caught wild monster to the persistent state. A failed catch is a strict
// no-op. A successful catch always marks the species seen+caught, then stores the
// new durable record party-first and falls back to the first free box slot. A full
// party plus full box grid still keeps the dex marks while rejecting storage. Pure.
void ZM_ApplyCatchToGameState(ZM_GameState& xGameStateInOut, bool bCaught, const ZM_BattleMonster& xCaught);
