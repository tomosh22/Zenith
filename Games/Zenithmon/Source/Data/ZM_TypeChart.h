#pragma once

#include "Zenithmon/Source/Data/ZM_Types.h"

// ============================================================================
// ZM_TypeChart -- the 18x18 type-effectiveness matrix (S1 data core).
//
// Pure, stateless lookup over the compiled chart table (ZM_TypeChart.cpp). The
// multipliers are the standard 18-type relationships under Zenithmon's original
// names; Docs/GameDesignDocument.md section 6 fixes the design intent (starter
// triangle, DRAKE/FEY checks, MIND/UMBRAL/BRAWL triangle, PHANTOM<->NORMAL
// immunity, IRON wall, ELECTRIC/EARTH and EARTH/SKY immunities).
//
// The chart is locked by a golden-matrix unit test (Tests/ZM_Tests_Data.cpp):
// any edit to the table is a deliberate two-place change (Docs/TestPlan.md 5.1).
// ============================================================================
namespace ZM_TypeChart
{
	// Damage multiplier of a single attacking type vs a single defending type:
	// 0 (immune), 0.5 (resisted), 1 (neutral), or 2 (super-effective).
	// eAttack and eDefend must be real types (< ZM_TYPE_COUNT).
	float GetEffectiveness(ZM_TYPE eAttack, ZM_TYPE eDefend);

	// Multiplier of an attacking type vs a (possibly dual-typed) defender. Pass
	// ZM_TYPE_NONE for eDefend2 on single-type defenders; the result is then the
	// single lookup. Otherwise it is the product of both lookups (a matching
	// eDefend2 == eDefend1 is treated as single-typed, never squared). This is
	// where 4x / 0.25x / 0x matchups come from.
	float GetDualTypeEffectiveness(ZM_TYPE eAttack, ZM_TYPE eDefend1, ZM_TYPE eDefend2);
}
