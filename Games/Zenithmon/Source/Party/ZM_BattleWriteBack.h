#pragma once

#include "Zenithmon/Source/Party/ZM_GameState.h"        // ZM_GameState, ZM_Party (write-back target)
#include "Zenithmon/Source/Battle/ZM_BattleMonster.h"    // ZM_BattleMonster (the post-battle instance)
#include "Zenithmon/Source/Battle/ZM_BattleTypes.h"      // ZM_SIDE (winner side)

class ZM_BattleDirectorCore;   // read the resolved battle via GetWinner() + GetEngine() (full type in the .cpp)

// ============================================================================
// ZM_BattleWriteBack -- the S5 item 5 SC3 battle-result persistence. After a WON
// wild battle the engine has already mutated the player's ZM_BattleMonster IN PLACE
// (exp/level/EVs/moves+PP + damaged HP); these pure helpers copy that result back
// into the persistent party lead so gained levels + the lead's damaged HP carry out
// of battle. WIN-ONLY (a loss is the SC5 whiteout; a catch is SC4); SINGLE-LEAD only
// (Q-2026-07-18-001). Pure: no ECS, no graphics, no I/O; compiled in ALL configs.
// ============================================================================

// Write the resolved battle lead back into the party. Strict no-op unless the PLAYER
// won (loss/flee/draw persist nothing) and uLeadSlot is a live party index. On a win
// it copies the mutable post-battle state (level, exp, EVs, moves+PP, curHP, status)
// into slot uLeadSlot via ZM_ApplyBattleMonsterToRecord.
void ZM_ApplyBattleResultToParty(ZM_Party& xPartyInOut, u_int uLeadSlot, ZM_SIDE eWinner,
                                 const ZM_BattleMonster& xFinalLead);

// Convenience overload: resolves the winner + the player's final active monster from
// the core and writes back to the game state's party lead. Same win-only semantics.
void ZM_ApplyBattleResultToParty(ZM_GameState& xGameStateInOut, const ZM_BattleDirectorCore& xCore);
