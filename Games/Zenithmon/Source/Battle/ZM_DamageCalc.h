#pragma once

#include "Zenithmon/Source/Battle/ZM_BattleTypes.h"
#include "Zenithmon/Source/Data/ZM_Types.h"   // ZM_TYPE

// ============================================================================
// ZM_DamageCalc -- pure Gen-V integer damage arithmetic (S2 box 1). No RNG, no
// battle state: the engine draws crit/roll and passes them in, so the whole
// pipeline is a deterministic, unit-testable pure function. Floor after every
// multiply; canonical Gen-V modifier order (base -> weather -> crit -> random ->
// STAB -> type -> burn -> screen). The ~60-effect executor stays in box 2.
// ============================================================================

// Effective stat after a stat-stage multiplier (box 1: stage 0 -> identity).
//   stage >= 0: floor(stat * (2+stage) / 2);  stage < 0: floor(stat * 2 / (2-stage)).
u_int ZM_ApplyStatStage(u_int uStat, int iStage);

// Integer type-effectiveness as a PERCENT, never a float in the damage path.
// The chart returns only exact floats {0,0.5,1,2}; the dual product is one of
// {0,0.25,0.5,1,2,4}, mapping to percent {0,25,50,100,200,400}. Pass
// ZM_TYPE_NONE for eDefType2 on a mono-type defender.
u_int ZM_EffectivenessPercent(ZM_TYPE eMoveType, ZM_TYPE eDefType1, ZM_TYPE eDefType2);

struct ZM_DamageInput
{
	u_int uLevel = 0u;
	u_int uPower = 0u;
	u_int uAttack = 0u;              // effective (post-stage) attacking stat: ATK or SPATTACK
	u_int uDefense = 1u;             // effective (post-stage) defending stat: DEF or SPDEFENSE (never 0: divisor)
	bool  bStab = false;             // move type in attacker's types
	u_int uEffectivenessPercent = 100u; // {0,25,50,100,200,400} from ZM_EffectivenessPercent
	bool  bCrit = false;             // drawn by the engine (see draw order)
	u_int uRandomPercent = 100u;     // 85..100 inclusive, drawn by the engine
	// ---- box-2/3 seams: box-1 identity inputs, so turning them on never moves box-1 goldens ----
	u_int uWeatherNum = 1u, uWeatherDen = 1u;   // box 3: 3/2 or 1/2
	bool  bBurnedPhysical = false;              // box 2: physical + attacker BURN
	bool  bScreen = false;                      // box 3: defender behind a screen
};

// LOCKED pipeline (floor after EVERY multiply; canonical Gen-V modifier order).
// Returns 0 iff uEffectivenessPercent==0 (immune); else max(1, d).
u_int ZM_CalcDamage(const ZM_DamageInput& xIn);
