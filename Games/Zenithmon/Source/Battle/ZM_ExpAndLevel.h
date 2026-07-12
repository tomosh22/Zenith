#pragma once

#include "Zenithmon/Source/Data/ZM_SpeciesData.h"     // ZM_SPECIES_ID, ZM_BaseStats, ZM_STAT, ZM_STAT_COUNT
#include "Zenithmon/Source/Data/ZM_StatCalc.h"        // authoritative uZM_MIN_LEVEL / uZM_MAX_LEVEL bounds
#include "Zenithmon/Source/Battle/ZM_BattleMonster.h"  // ZM_BattleMonster (+ ZM_SIDE via ZM_BattleTypes.h)
#include "Zenithmon/Source/Battle/ZM_BattleEvent.h"    // ZM_BattleEvent, ZM_MakeEvent, the reserved event kinds
#include "Collections/Zenith_Vector.h"

// ============================================================================
// ZM_ExpAndLevel -- the S2 exp/level/EV/evolution box (DecisionLog ZM-D-043).
// Pure integer math (NO RNG anywhere) so the exp path cannot shift a soak seed.
//
// Everything per-species is systematically DERIVED (the ZM-D-020/021/023
// placeholder philosophy) -- growth rate from rarity, base-exp yield from BST,
// EV yield from the highest base stat, evolve level from evo stage -- so no
// ZM_SpeciesData column is added and the species/data goldens never re-golden.
// The accessor signatures below are the stable seam an S11 balance pass tunes.
//
// The battle engine awards exp on a faint through ZM_ApplyExpGain, GATED by the
// new default-false ZM_BattleConfig::m_bAwardExp so every existing golden stays
// byte-identical. The reserved ZM_BATTLE_EVENT kinds EXP_GAINED / LEVEL_UP /
// MOVE_LEARNED / EVOLUTION_QUEUED (already declared before ZM_BATTLE_EVENT_COUNT)
// are used without changing the POD schema.
// ============================================================================

// The four classic cumulative-exp curve families (one per rarity tier).
enum ZM_GROWTH_RATE : u_int
{
	ZM_GROWTH_FAST,
	ZM_GROWTH_MEDIUM_FAST,
	ZM_GROWTH_MEDIUM_SLOW,
	ZM_GROWTH_SLOW,
	ZM_GROWTH_COUNT
};

// --- pure curve math ---
// Cumulative exp required to REACH uLevel (in [1,100]); L1 == 0 for every curve.
u_int          ZM_ExpForLevel(ZM_GROWTH_RATE eRate, u_int uLevel);
// Largest L in [1,100] whose ExpForLevel <= uExp (clamps: L1 for 0, L100 above the total).
u_int          ZM_LevelForExp(ZM_GROWTH_RATE eRate, u_int uExp);

// --- per-species derived accessors (placeholders; S11-tunable) ---
ZM_GROWTH_RATE ZM_GetSpeciesGrowthRate(ZM_SPECIES_ID eId);      // from rarity
u_int          ZM_GetSpeciesBaseExpYield(ZM_SPECIES_ID eId);    // max(1, BST*2/5)
ZM_BaseStats   ZM_GetSpeciesEVYield(ZM_SPECIES_ID eId);         // one stat set to the yield amount
u_int          ZM_GetSpeciesEvolveLevel(ZM_SPECIES_ID eId);     // 0 == no level evolution

// --- award / accumulation ---
u_int          ZM_CalcExpGain(ZM_SPECIES_ID eDefeated, u_int uDefLevel, bool bTrainer);
void           ZM_NormalizeEVs(u_int (&auEV)[ZM_STAT_COUNT]);
void           ZM_AddEVYield(u_int (&auEV)[ZM_STAT_COUNT], const ZM_BaseStats& xYield);

// --- evolution (pure; LEVEL trigger only this box) ---
ZM_SPECIES_ID  ZM_CheckEvolveEligible(const ZM_BattleMonster& xMon);   // target species, or NONE
bool           ZM_Evolve(ZM_BattleMonster& xMon);                      // exactly one edge + restat + guarded HP carry

// --- monster mutation (emits into an append-only event vector; NO RNG) ---
// Adds exp; loops level-ups (recompute the six max stats from xMon.m_xBaseStats,
// carry the HP delta); learns level-up moves (4-move overflow SKIPs, ZM-D-043).
// Emits EXP_GAINED with the amount actually credited, then per gained level
// LEVEL_UP and any MOVE_LEARNED. Evolution is queued only by terminal battle
// settlement. Returns levels gained. Zero/no room/already-at-cap is a strict no-op.
u_int ZM_ApplyExpGain(ZM_BattleMonster& xMon, u_int uExpGain,
                      Zenith_Vector<ZM_BattleEvent>& xEvents,
                      ZM_SIDE eSide, u_int uSlot, u_int uMaxLevel = uZM_MAX_LEVEL);
