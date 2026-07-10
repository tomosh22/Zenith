#include "Zenith.h"
#include "Zenithmon/Source/Data/ZM_TypeChart.h"

// ============================================================================
// ZM_TypeChart -- 18x18 effectiveness table + lookups. See the header and
// Docs/GameDesignDocument.md section 6. If you edit a cell here, mirror it in
// the golden copy in Tests/ZM_Tests_Data.cpp (the test fails otherwise -- that
// two-place discipline is deliberate).
// ============================================================================

namespace
{
	// Rows = ATTACKING type, columns = DEFENDING type, both in ZM_TYPE order.
	// Cell = damage multiplier (0 immune / 0.5 resisted / 1 neutral / 2 weak).
	//
	// Columns:      Nor  Fir  Wat  Gra  Ele  Ice  Brw  Ven  Ear  Sky  Min  Swa  Sto  Pha  Dra  Umb  Iro  Fey
	constexpr float s_afChart[ZM_TYPE_COUNT][ZM_TYPE_COUNT] =
	{
		/* NORMAL   */ { 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, .5f, 0.f, 1.f, 1.f, .5f, 1.f },
		/* FIRE     */ { 1.f, .5f, .5f, 2.f, 1.f, 2.f, 1.f, 1.f, 1.f, 1.f, 1.f, 2.f, .5f, 1.f, .5f, 1.f, 2.f, 1.f },
		/* WATER    */ { 1.f, 2.f, .5f, .5f, 1.f, 1.f, 1.f, 1.f, 2.f, 1.f, 1.f, 1.f, 2.f, 1.f, .5f, 1.f, 1.f, 1.f },
		/* GRASS    */ { 1.f, .5f, 2.f, .5f, 1.f, 1.f, 1.f, .5f, 2.f, .5f, 1.f, .5f, 2.f, 1.f, .5f, 1.f, .5f, 1.f },
		/* ELECTRIC */ { 1.f, 1.f, 2.f, .5f, .5f, 1.f, 1.f, 1.f, 0.f, 2.f, 1.f, 1.f, 1.f, 1.f, .5f, 1.f, 1.f, 1.f },
		/* ICE      */ { 1.f, .5f, .5f, 2.f, 1.f, .5f, 1.f, 1.f, 2.f, 2.f, 1.f, 1.f, 1.f, 1.f, 2.f, 1.f, .5f, 1.f },
		/* BRAWL    */ { 2.f, 1.f, 1.f, 1.f, 1.f, 2.f, 1.f, .5f, 1.f, .5f, .5f, .5f, 2.f, 0.f, 1.f, 2.f, 2.f, .5f },
		/* VENOM    */ { 1.f, 1.f, 1.f, 2.f, 1.f, 1.f, 1.f, .5f, .5f, 1.f, 1.f, 1.f, .5f, .5f, 1.f, 1.f, 0.f, 2.f },
		/* EARTH    */ { 1.f, 2.f, 1.f, .5f, 2.f, 1.f, 1.f, 2.f, 1.f, 0.f, 1.f, .5f, 2.f, 1.f, 1.f, 1.f, 2.f, 1.f },
		/* SKY      */ { 1.f, 1.f, 1.f, 2.f, .5f, 1.f, 2.f, 1.f, 1.f, 1.f, 1.f, 2.f, .5f, 1.f, 1.f, 1.f, .5f, 1.f },
		/* MIND     */ { 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 2.f, 2.f, 1.f, 1.f, .5f, 1.f, 1.f, 1.f, 1.f, 0.f, .5f, 1.f },
		/* SWARM    */ { 1.f, .5f, 1.f, 2.f, 1.f, 1.f, .5f, .5f, 1.f, .5f, 2.f, 1.f, 1.f, .5f, 1.f, 2.f, .5f, .5f },
		/* STONE    */ { 1.f, 2.f, 1.f, 1.f, 1.f, 2.f, .5f, 1.f, .5f, 2.f, 1.f, 2.f, 1.f, 1.f, 1.f, 1.f, .5f, 1.f },
		/* PHANTOM  */ { 0.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 2.f, 1.f, 1.f, 2.f, 1.f, .5f, 1.f, 1.f },
		/* DRAKE    */ { 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 2.f, 1.f, .5f, 0.f },
		/* UMBRAL   */ { 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, .5f, 1.f, 1.f, 1.f, 2.f, 1.f, 1.f, 2.f, 1.f, .5f, 1.f, .5f },
		/* IRON     */ { 1.f, .5f, .5f, 1.f, .5f, 2.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 2.f, 1.f, 1.f, 1.f, .5f, 2.f },
		/* FEY      */ { 1.f, .5f, 1.f, 1.f, 1.f, 1.f, 2.f, .5f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 2.f, 2.f, .5f, 1.f },
	};
}

float ZM_TypeChart::GetEffectiveness(ZM_TYPE eAttack, ZM_TYPE eDefend)
{
	Zenith_Assert(eAttack < ZM_TYPE_COUNT && eDefend < ZM_TYPE_COUNT,
		"ZM_TypeChart::GetEffectiveness: type index out of range (atk=%u def=%u)",
		(u_int)eAttack, (u_int)eDefend);
	return s_afChart[eAttack][eDefend];
}

float ZM_TypeChart::GetDualTypeEffectiveness(ZM_TYPE eAttack, ZM_TYPE eDefend1, ZM_TYPE eDefend2)
{
	float fResult = GetEffectiveness(eAttack, eDefend1);

	// Single-typed defender (NONE), or a duplicated slot -- take the one lookup.
	if (eDefend2 != ZM_TYPE_NONE && eDefend2 != eDefend1)
	{
		fResult *= GetEffectiveness(eAttack, eDefend2);
	}
	return fResult;
}
