#pragma once

#include "Zenithmon/Source/Data/ZM_SpeciesData.h"   // ZM_STAT
#include "Zenithmon/Source/Data/ZM_NatureData.h"     // ZM_NATURE, ZM_GetNatureStatPercent

// ============================================================================
// ZM_StatCalc -- the Gen-III+ stat formulas (S1 data core), pure integer math.
// Spec: Docs/GameDesignDocument.md section 7.1 (stats); DecisionLog ZM-D-027.
//
//   HP    = ((2*base + IV + EV/4) * level) / 100 + level + 10
//   other = (((2*base + IV + EV/4) * level) / 100 + 5) * naturePercent / 100
//
// All divisions truncate (floor); the nature multiplier is the integer percent
// from ZM_GetNatureStatPercent (110 / 90 / 100, ZM-D-025), applied last so the
// x11/10 and x9/10 are exact. No floating point anywhere -- the battle engine
// (S2) needs bit-exact reproducibility. HP ignores nature.
// ============================================================================

// Value ranges the formulas assume (the battle/data layers clamp to these).
static const u_int uZM_MAX_IV          = 31u;   // per stat, 0..31
static const u_int uZM_MAX_EV_PER_STAT = 252u;  // per stat cap
static const u_int uZM_MAX_EV_TOTAL    = 510u;  // sum across the six stats
static const u_int uZM_MIN_LEVEL       = 1u;
static const u_int uZM_MAX_LEVEL       = 100u;

// HP stat from base HP, IV, EV, level.
u_int ZM_CalcHPStat(u_int uBase, u_int uIV, u_int uEV, u_int uLevel);

// One of the five non-HP stats; uNaturePercent is 110 / 100 / 90.
u_int ZM_CalcOtherStat(u_int uBase, u_int uIV, u_int uEV, u_int uLevel, u_int uNaturePercent);

// Convenience: compute any stat for a species+nature, dispatching HP vs the rest
// and pulling the nature multiplier automatically (HP is nature-independent).
u_int ZM_CalcStat(ZM_STAT eStat, u_int uBase, u_int uIV, u_int uEV, u_int uLevel, ZM_NATURE eNature);
