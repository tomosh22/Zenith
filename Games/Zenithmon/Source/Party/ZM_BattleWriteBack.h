#pragma once

#include "Zenithmon/Source/Party/ZM_GameState.h"        // ZM_GameState, ZM_Party (write-back target)
#include "Zenithmon/Source/Battle/ZM_BattleMonster.h"    // ZM_BattleMonster (the post-battle instance)
#include "Zenithmon/Source/Battle/ZM_BattleTypes.h"      // ZM_SIDE (winner side)
#include "Zenithmon/Source/Data/ZM_TrainerData.h"        // ZM_TRAINER_ID (S7 item 3 SC5 trainer payout)

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

// ============================================================================
// S7 item 3 SC5 -- the TRAINER half of resolved-battle persistence.
// ============================================================================

// What a trainer payout ACTUALLY did, so a caller never has to re-derive it.
// m_uMoneyCredited is the OBSERVED purse delta, not the row's prize: at
// uZM_MONEY_CAP the credit saturates and this reports the smaller figure (0 for a
// purse already at or above the cap). m_bFlagNewlySet is true ONLY on a 0 -> 1
// transition of a REGISTERED flag -- it is false for a ZM_STORY_FLAG_NONE row and
// false on a repeat win, which is the lever SC6's "never re-spot a beaten trainer"
// guard is built on.
//
// S8 G1-3 (ZM-70) ADDS m_bBadgeNewlyAwarded and m_bItemGranted, read straight off
// the row's own m_eBadgeReward / m_eItemReward (ZM_TrainerData.h) -- most rows
// carry ZM_BADGE_NONE / ZM_ITEM_NONE, and both are total no-ops through
// ZM_GameState::AwardBadge / ZM_Bag::Add, so no per-row branch exists here either.
// m_bBadgeNewlyAwarded mirrors m_bFlagNewlySet's shape: true ONLY on a genuine
// 0 -> 1 transition of the mask bit, so it is idempotent on a repeat win the same
// way AwardBadge's OR is. m_bItemGranted IS DEDUPLICATED on a repeat win (ZM-70
// review correction: BLOCKING), but NOT by the badge's mechanism: it is gated on
// ZM_Bag::Has(xRow.m_eItemReward) before the Add, because the bag is ALREADY the
// record of "does the player own this" -- zero new GameState state, so ZM-D-135
// (which forbids adding one) never governs this column. The MONEY, unlike the
// item, still pays again on a repeat win, for the OPPOSITE reason: ZM-D-135 DOES
// apply there, because nothing already records "already paid" for it (the
// TOTALITY table below spells all three columns side by side).
struct ZM_TrainerRewardResult
{
	u_int	m_uMoneyCredited      = 0u;
	bool	m_bFlagNewlySet       = false;
	bool	m_bApplied            = false;   // true iff this call was a WIN payout (by-id: on a REGISTERED trainer)
	bool	m_bBadgeNewlyAwarded  = false;   // true iff the row's badge went 0 -> 1 this call (ZM_BADGE_NONE rows: always false)
	bool	m_bItemGranted        = false;   // true iff the row's item was successfully added to the bag (ZM_ITEM_NONE rows: always false)
};

// The trainer half of resolved-battle persistence (S7 item 3 SC5, extended by S8
// G1-3 / ZM-70 to the badge + item reward). PURE: it names no ECS type, no scene,
// no physics and no battle machinery -- just a ZM_GameState reference, a row, and
// the primitives that decide the outcome.
//
// WIN-ONLY, and the win test is not re-implemented here: it routes through the
// SHIPPED ZM_ClassifyBattleResult, so the trainer arm and the wild arm can never
// drift about what "the player won" means. Only ZM_BRA_WRITE_BACK_WIN pays.
//   * ZM_SIDE_PLAYER               -> credit m_uPrizeMoney via ZM_GameState::AddMoney
//                                     (never a raw m_uMoney write -- AddMoney is the
//                                     sole enforcer of the cap), set m_eDefeatFlag via
//                                     ZM_SetStoryFlag, award m_eBadgeReward via
//                                     ZM_GameState::AwardBadge and grant m_eItemReward
//                                     via ZM_Bag::Add.
//   * ZM_SIDE_ENEMY                -> classified WHITEOUT: pays NOTHING, and
//                                     deliberately does NOT touch m_bPendingWhiteout.
//                                     The shipped ZM_ApplyBattleResultToParty remains
//                                     the SINGLE owner of that latch.
//   * ZM_SIDE_COUNT (draw/double-KO/flee) -> pays NOTHING either way. Fail closed.
//
// TOTALITY, the complete table. It NEVER asserts and NEVER logs:
//   * unregistered id (incl. ZM_TRAINER_NONE) -> default result, ZERO mutation. The
//     registered check runs BEFORE ZM_GetTrainerData, whose UNKNOWN-row path logs a
//     non-fatal Zenith_Error. (by-id overload only -- the by-row primitive below
//     never resolves an id at all, so this arm does not apply to it.)
//   * m_eDefeatFlag == ZM_STORY_FLAG_NONE (the Rambler row) -> money is still
//     credited; ZM_SetStoryFlag returns false with no mutation and no log, so
//     m_bFlagNewlySet is false. There is no NONE branch here by design.
//   * m_eBadgeReward == ZM_BADGE_NONE / m_eItemReward == ZM_ITEM_NONE (every row but
//     Fenna's, today) -> AwardBadge and ZM_Bag::Add are already TOTAL and reject
//     their own sentinels, so there is no NONE branch for these either.
//   * purse at/over uZM_MONEY_CAP -> m_uMoneyCredited == 0, m_bApplied still true,
//     and the defeat flag is STILL set (the flag is never coupled to the money).
//   * a SECOND win over the same trainer -> THREE columns, three DIFFERENT
//     reasons, and they do NOT all behave the same (ZM-70 review correction):
//       - the FLAG is idempotent (m_bFlagNewlySet false): ZM_SetStoryFlag
//         reports no write for a bit already set. SC6's sight FSM ANDs in
//         "not yet defeated" to stop a re-battle happening at all.
//       - the BADGE is idempotent for a DIFFERENT, structural reason
//         (m_bBadgeNewlyAwarded false): AwardBadge ORs a single mask bit, so
//         OR-ing a bit already set changes nothing -- there is no dedup LOGIC,
//         because none is needed.
//       - the MONEY is credited AGAIN. SC5's deliberate answer: "pay once" is
//         a CALLER-side gate, because ZM-D-135 forbids a new ZM_GameState
//         member that would record "already paid" -- there is no existing
//         state anywhere that already answers it.
//       - the ITEM, unlike the money, DEDUPLICATES (m_bItemGranted false):
//         m_xBag IS its own record of "does the player own this"
//         (ZM_Bag::Has), so nothing new needs recording and ZM-D-135 never
//         governs this column at all. This matters in a way it does not for
//         the flag or the badge: a TM in this game is m_bConsumable == false,
//         so a second copy has exactly one use -- ZM_ShopSell converting it
//         to its m_uSellPrice -- a farmable prize the defeat flag exists to
//         forbid.
//
// ★ BY-ROW, extracted per ZM-D-208 Ruling 2's reasoning applied to the reward half
// (ZM-D-209): works PURELY from xRow's fields once classification has already
// decided this is a win, so a hand-built fixture can drive it directly and its
// coverage cannot be retired by a roster change.
ZM_TrainerRewardResult	ZM_ApplyTrainerResultToGameState(ZM_GameState& xGameStateInOut,
							const ZM_TrainerData& xRow, ZM_SIDE eWinner, bool bLeadFainted);

// by-ID convenience: classifies, checks ZM_IsRegisteredTrainer, resolves the row via
// ZM_GetTrainerData, then forwards to the by-ROW primitive above. An EXTRACTION, not
// new logic -- this function's entire body used to be what the primitive now is.
ZM_TrainerRewardResult	ZM_ApplyTrainerResultToGameState(ZM_GameState& xGameStateInOut,
							ZM_TRAINER_ID eTrainer, ZM_SIDE eWinner, bool bLeadFainted);

// Convenience overload for the ECS caller: resolves the winner and the
// single-lead faint from the core exactly as the shipped
// ZM_ApplyBattleResultToParty(ZM_GameState&, const ZM_BattleDirectorCore&) does,
// then forwards to the by-ID overload above.
ZM_TrainerRewardResult	ZM_ApplyTrainerResultToGameState(ZM_GameState& xGameStateInOut,
							ZM_TRAINER_ID eTrainer, const ZM_BattleDirectorCore& xCore);
