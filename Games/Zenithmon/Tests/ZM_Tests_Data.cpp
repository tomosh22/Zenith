#include "Zenith.h"

// ============================================================================
// ZM_Tests_Data -- S1 data-core unit tests (category ZM_Data). This first slice
// covers ZM_Types + ZM_TypeChart; species/moves/registry/worldspec suites join
// this category as their tasks land (Docs/TestPlan.md 5.1).
//
// The chart test carries an INDEPENDENT golden copy of the 18x18 matrix: a
// chart edit must touch ZM_TypeChart.cpp AND this golden, or the test fails.
// That two-place discipline is the point. The design-intent tests below are the
// semantic guard -- they assert the GDD's stated relationships directly, so a
// value mistranscribed into BOTH tables is still caught where it matters.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Source/Data/ZM_Types.h"
#include "Zenithmon/Source/Data/ZM_TypeChart.h"

#include <cstring>   // strcmp (type-name distinctness check)

namespace
{
	// Independent golden matrix (rows = attacker, cols = defender, ZM_TYPE order;
	// 0 / 0.5 / 1 / 2). Kept deliberately separate from ZM_TypeChart.cpp.
	//
	// Columns:      Nor  Fir  Wat  Gra  Ele  Ice  Brw  Ven  Ear  Sky  Min  Swa  Sto  Pha  Dra  Umb  Iro  Fey
	constexpr float s_afGolden[ZM_TYPE_COUNT][ZM_TYPE_COUNT] =
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

	// A legal effectiveness multiplier is exactly one of {0, 0.5, 1, 2}. These
	// are exact in binary so an exact float compare is intentional here.
	bool IsLegalMultiplier(float f)
	{
		return f == 0.f || f == .5f || f == 1.f || f == 2.f;
	}
}

// The runtime chart equals the independent golden, cell for cell.
ZENITH_TEST(ZM_Data, TypeChart_MatchesGolden)
{
	for (u_int uAtk = 0; uAtk < ZM_TYPE_COUNT; ++uAtk)
	{
		for (u_int uDef = 0; uDef < ZM_TYPE_COUNT; ++uDef)
		{
			const float fActual = ZM_TypeChart::GetEffectiveness((ZM_TYPE)uAtk, (ZM_TYPE)uDef);
			ZENITH_ASSERT_EQ_FLOAT(fActual, s_afGolden[uAtk][uDef], 0.0001f,
				"type chart mismatch at [%s attacking %s]",
				ZM_TypeToString((ZM_TYPE)uAtk), ZM_TypeToString((ZM_TYPE)uDef));
		}
	}
}

// Every cell is one of the four legal multipliers -- no stray values.
ZENITH_TEST(ZM_Data, TypeChart_AllCellsLegal)
{
	for (u_int uAtk = 0; uAtk < ZM_TYPE_COUNT; ++uAtk)
	{
		for (u_int uDef = 0; uDef < ZM_TYPE_COUNT; ++uDef)
		{
			const float f = ZM_TypeChart::GetEffectiveness((ZM_TYPE)uAtk, (ZM_TYPE)uDef);
			ZENITH_ASSERT_TRUE(IsLegalMultiplier(f),
				"illegal multiplier %.3f at [%s attacking %s]", f,
				ZM_TypeToString((ZM_TYPE)uAtk), ZM_TypeToString((ZM_TYPE)uDef));
		}
	}
}

// Exactly eight immunities (0x cells) -- the well-known 18-type total. An
// independent count that catches a stray 0 or a dropped immunity.
ZENITH_TEST(ZM_Data, TypeChart_ImmunityCountIsEight)
{
	u_int uImmunities = 0;
	for (u_int uAtk = 0; uAtk < ZM_TYPE_COUNT; ++uAtk)
	{
		for (u_int uDef = 0; uDef < ZM_TYPE_COUNT; ++uDef)
		{
			if (ZM_TypeChart::GetEffectiveness((ZM_TYPE)uAtk, (ZM_TYPE)uDef) == 0.f)
			{
				++uImmunities;
			}
		}
	}
	ZENITH_ASSERT_EQ(uImmunities, 8u, "expected 8 type immunities in the chart");
}

// GDD 6: the starter triangle GRASS > WATER > FIRE > GRASS.
ZENITH_TEST(ZM_Data, TypeChart_StarterTriangle)
{
	ZENITH_ASSERT_EQ_FLOAT(ZM_TypeChart::GetEffectiveness(ZM_TYPE_GRASS, ZM_TYPE_WATER), 2.f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(ZM_TypeChart::GetEffectiveness(ZM_TYPE_WATER, ZM_TYPE_FIRE),  2.f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(ZM_TypeChart::GetEffectiveness(ZM_TYPE_FIRE,  ZM_TYPE_GRASS), 2.f, 0.0001f);
	// ...and each is resisted the other way round.
	ZENITH_ASSERT_EQ_FLOAT(ZM_TypeChart::GetEffectiveness(ZM_TYPE_WATER, ZM_TYPE_GRASS), .5f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(ZM_TypeChart::GetEffectiveness(ZM_TYPE_FIRE,  ZM_TYPE_WATER), .5f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(ZM_TypeChart::GetEffectiveness(ZM_TYPE_GRASS, ZM_TYPE_FIRE),  .5f, 0.0001f);
}

// GDD 6: MIND > BRAWL > UMBRAL > MIND, and PHANTOM<->NORMAL mutual immunity.
ZENITH_TEST(ZM_Data, TypeChart_SecondTriangleAndGhostNormal)
{
	ZENITH_ASSERT_EQ_FLOAT(ZM_TypeChart::GetEffectiveness(ZM_TYPE_MIND,   ZM_TYPE_BRAWL),  2.f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(ZM_TypeChart::GetEffectiveness(ZM_TYPE_BRAWL,  ZM_TYPE_UMBRAL), 2.f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(ZM_TypeChart::GetEffectiveness(ZM_TYPE_UMBRAL, ZM_TYPE_MIND),   2.f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(ZM_TypeChart::GetEffectiveness(ZM_TYPE_PHANTOM, ZM_TYPE_NORMAL),  0.f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(ZM_TypeChart::GetEffectiveness(ZM_TYPE_NORMAL,  ZM_TYPE_PHANTOM), 0.f, 0.0001f);
	// BRAWL is also walled by PHANTOM (classic 0x).
	ZENITH_ASSERT_EQ_FLOAT(ZM_TypeChart::GetEffectiveness(ZM_TYPE_BRAWL, ZM_TYPE_PHANTOM), 0.f, 0.0001f);
}

// GDD 6: DRAKE resists the elemental trio (+ELECTRIC) and is answered by ICE and
// FEY; DRAKE cannot touch FEY at all.
ZENITH_TEST(ZM_Data, TypeChart_DrakeChecks)
{
	ZENITH_ASSERT_EQ_FLOAT(ZM_TypeChart::GetEffectiveness(ZM_TYPE_FIRE,     ZM_TYPE_DRAKE), .5f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(ZM_TypeChart::GetEffectiveness(ZM_TYPE_WATER,    ZM_TYPE_DRAKE), .5f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(ZM_TypeChart::GetEffectiveness(ZM_TYPE_GRASS,    ZM_TYPE_DRAKE), .5f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(ZM_TypeChart::GetEffectiveness(ZM_TYPE_ELECTRIC, ZM_TYPE_DRAKE), .5f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(ZM_TypeChart::GetEffectiveness(ZM_TYPE_ICE, ZM_TYPE_DRAKE), 2.f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(ZM_TypeChart::GetEffectiveness(ZM_TYPE_FEY, ZM_TYPE_DRAKE), 2.f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(ZM_TypeChart::GetEffectiveness(ZM_TYPE_DRAKE, ZM_TYPE_FEY), 0.f, 0.0001f);
}

// GDD 6: the named immunities + IRON as the broad defensive wall.
ZENITH_TEST(ZM_Data, TypeChart_ImmunitiesAndIronWall)
{
	ZENITH_ASSERT_EQ_FLOAT(ZM_TypeChart::GetEffectiveness(ZM_TYPE_ELECTRIC, ZM_TYPE_EARTH),  0.f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(ZM_TypeChart::GetEffectiveness(ZM_TYPE_EARTH,    ZM_TYPE_SKY),    0.f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(ZM_TypeChart::GetEffectiveness(ZM_TYPE_VENOM,    ZM_TYPE_IRON),   0.f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(ZM_TypeChart::GetEffectiveness(ZM_TYPE_MIND,     ZM_TYPE_UMBRAL), 0.f, 0.0001f);

	// IRON weak only to FIRE / BRAWL / EARTH...
	ZENITH_ASSERT_EQ_FLOAT(ZM_TypeChart::GetEffectiveness(ZM_TYPE_FIRE,  ZM_TYPE_IRON), 2.f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(ZM_TypeChart::GetEffectiveness(ZM_TYPE_BRAWL, ZM_TYPE_IRON), 2.f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(ZM_TypeChart::GetEffectiveness(ZM_TYPE_EARTH, ZM_TYPE_IRON), 2.f, 0.0001f);

	// ...and resists a broad set (count the <1x columns for defender IRON).
	u_int uIronResists = 0;
	for (u_int uAtk = 0; uAtk < ZM_TYPE_COUNT; ++uAtk)
	{
		if (ZM_TypeChart::GetEffectiveness((ZM_TYPE)uAtk, ZM_TYPE_IRON) < 1.f)
		{
			++uIronResists;
		}
	}
	ZENITH_ASSERT_GE(uIronResists, 8u, "IRON is meant to be the broad defensive wall");
}

// Dual-type effectiveness = product of the attacking type vs each defending
// type; NONE and duplicate slots collapse to the single lookup.
ZENITH_TEST(ZM_Data, TypeChart_DualTypeProducts)
{
	// 4x: STONE vs a SKY/SWARM defender (Rock hits Flying and Bug 2x each).
	ZENITH_ASSERT_EQ_FLOAT(
		ZM_TypeChart::GetDualTypeEffectiveness(ZM_TYPE_STONE, ZM_TYPE_SKY, ZM_TYPE_SWARM), 4.f, 0.0001f);
	// 0.25x: GRASS vs a FIRE/DRAKE defender (0.5 * 0.5).
	ZENITH_ASSERT_EQ_FLOAT(
		ZM_TypeChart::GetDualTypeEffectiveness(ZM_TYPE_GRASS, ZM_TYPE_FIRE, ZM_TYPE_DRAKE), 0.25f, 0.0001f);
	// 0x: any component immunity zeroes the product (EARTH cannot hit SKY).
	ZENITH_ASSERT_EQ_FLOAT(
		ZM_TypeChart::GetDualTypeEffectiveness(ZM_TYPE_EARTH, ZM_TYPE_SKY, ZM_TYPE_IRON), 0.f, 0.0001f);
	// Single-typed defender via NONE == the single lookup.
	ZENITH_ASSERT_EQ_FLOAT(
		ZM_TypeChart::GetDualTypeEffectiveness(ZM_TYPE_FIRE, ZM_TYPE_GRASS, ZM_TYPE_NONE), 2.f, 0.0001f);
	// Duplicated slot is treated as single-typed, never squared.
	ZENITH_ASSERT_EQ_FLOAT(
		ZM_TypeChart::GetDualTypeEffectiveness(ZM_TYPE_FIRE, ZM_TYPE_GRASS, ZM_TYPE_GRASS), 2.f, 0.0001f);
}

// Type names are present, distinct, and the sentinels resolve as documented.
ZENITH_TEST(ZM_Data, Types_ToStringContract)
{
	ZENITH_ASSERT_STREQ(ZM_TypeToString(ZM_TYPE_NORMAL), "NORMAL");
	ZENITH_ASSERT_STREQ(ZM_TypeToString(ZM_TYPE_FIRE),   "FIRE");
	ZENITH_ASSERT_STREQ(ZM_TypeToString(ZM_TYPE_FEY),    "FEY");
	ZENITH_ASSERT_STREQ(ZM_TypeToString(ZM_TYPE_NONE),   "NONE");

	for (u_int uA = 0; uA < ZM_TYPE_COUNT; ++uA)
	{
		const char* szA = ZM_TypeToString((ZM_TYPE)uA);
		ZENITH_ASSERT_NOT_NULL(szA);
		ZENITH_ASSERT_TRUE(szA[0] != '\0', "type name %u is empty", uA);
		for (u_int uB = uA + 1; uB < ZM_TYPE_COUNT; ++uB)
		{
			ZENITH_ASSERT_FALSE(strcmp(szA, ZM_TypeToString((ZM_TYPE)uB)) == 0,
				"duplicate type name '%s' at %u and %u", szA, uA, uB);
		}
	}
}
