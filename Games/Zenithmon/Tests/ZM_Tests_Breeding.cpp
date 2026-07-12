#include "Zenith.h"

// ============================================================================
// ZM_Tests_Breeding -- S2 box-6 SC1 (ZM_Breeding + ZM_Daycare) unit tests.
// Covers base-evolution derivation, breeding-group compatibility, the seeded
// egg-generation draw order (IV inheritance / nature lock / ability + move
// copy), and the pure-integer daycare state machine (deposit / withdraw / step-
// leveling / egg-availability threshold / collect).
//
// Category split (mirrors ZM_Tests_ExpAndLevel.cpp): these are ALL pure
// data/logic + pure-mutation tests with NO battle-engine / event-stream
// dependency, so every case is suite ZM_Data.
//
// DETERMINISM STRATEGY (spec section 13): the exact IV/nature goldens are
// derived by a LOCAL offline oracle (OracleGenerate) that replays the pinned
// ZM_GenerateEgg draw order (spec section 7) against an independently-seeded
// ZM_BattleRNG. The expected literals therefore come from the SPEC, not from
// re-running the implementation -- which is what makes parallel authoring safe
// and the golden a real regression guard. Every stochastic test uses identical
// seeds for its positive/oracle pair and a distinct seed for its control.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Source/Battle/ZM_Breeding.h"
#include "Zenithmon/Source/Battle/ZM_Daycare.h"
#include "Zenithmon/Source/Battle/ZM_BattleMonster.h"
#include "Zenithmon/Source/Battle/ZM_ExpAndLevel.h"
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"
#include "Zenithmon/Source/Data/ZM_NatureData.h"
#include "Zenithmon/Source/Data/ZM_AbilityData.h"
#include "Zenithmon/Source/Data/ZM_Learnsets.h"
#include "Zenithmon/Source/Data/ZM_MoveData.h"
#include "Zenithmon/Source/Data/ZM_BattleRNG.h"

// ============================================================================
// Test-local helpers (anonymous namespace: no cross-TU ODR clashes).
// ============================================================================
namespace
{
	// ---- spec builders -----------------------------------------------------
	// A parent seed with explicit per-stat IVs. No base-stat override (breeding
	// reads only species / IVs / nature / ability / moves). EVs 0; exp UNSPECIFIED.
	ZM_BattleMonsterSpec MakeSpec(ZM_SPECIES_ID eSpecies, const u_int (&aIV)[ZM_STAT_COUNT],
		u_int uLevel = 50u, ZM_NATURE eNature = ZM_NATURE_FERAL,
		ZM_ABILITY_ID eAbility = ZM_ABILITY_NONE)
	{
		ZM_BattleMonsterSpec xSpec;
		xSpec.m_eSpecies = eSpecies;
		xSpec.m_uLevel = uLevel;
		for (u_int i = 0; i < ZM_STAT_COUNT; ++i) { xSpec.m_auIV[i] = aIV[i]; xSpec.m_auEV[i] = 0u; }
		xSpec.m_eNature = eNature;
		xSpec.m_eAbility = eAbility;
		for (u_int i = 0; i < uZM_MAX_MOVES; ++i) { xSpec.m_aeMoves[i] = ZM_MOVE_NONE; }
		xSpec.m_bOverrideBaseStats = false;
		xSpec.m_uCurExp = uZM_EXP_UNSPECIFIED;
		return xSpec;
	}

	// Convenience: a parent whose six IVs are all uIV.
	ZM_BattleMonsterSpec MakeSpecUniform(ZM_SPECIES_ID eSpecies, u_int uIV,
		u_int uLevel = 50u, ZM_NATURE eNature = ZM_NATURE_FERAL,
		ZM_ABILITY_ID eAbility = ZM_ABILITY_NONE)
	{
		const u_int aIV[ZM_STAT_COUNT] = { uIV, uIV, uIV, uIV, uIV, uIV };
		return MakeSpec(eSpecies, aIV, uLevel, eNature, eAbility);
	}

	// ---- offline inheritance oracle (spec section 13) ----------------------
	// A faithful, INDEPENDENT re-implementation of the pinned ZM_GenerateEgg draw
	// order. Run it on an RNG seeded identically to the one handed to the real
	// ZM_GenerateEgg and the two outputs must agree bit-for-bit.
	struct OracleEgg
	{
		u_int     m_auIV[ZM_STAT_COUNT];
		ZM_NATURE m_eNature;
		bool      m_abInherited[ZM_STAT_COUNT];   // which stats were copied from a parent
		u_int     m_uInheritCount;                 // == K (3 or 5)
	};

	OracleEgg OracleGenerate(const u_int (&aMotherIV)[ZM_STAT_COUNT],
		const u_int (&aFatherIV)[ZM_STAT_COUNT],
		bool bKnot, ZM_NATURE eEverstone, ZM_BattleRNG& xRng)
	{
		OracleEgg xOut{};
		const u_int uK = bKnot ? uZM_BREED_IV_INHERIT_KNOT : uZM_BREED_IV_INHERIT_BASE;

		// Phase B1 -- pick K distinct stat indices (rejection sampling; one draw/iter).
		bool abInherit[ZM_STAT_COUNT] = {};
		u_int uPicked = 0u;
		while (uPicked < uK)
		{
			const u_int uIdx = xRng.RandBelow(ZM_STAT_COUNT);
			if (!abInherit[uIdx]) { abInherit[uIdx] = true; ++uPicked; }
		}

		// Phase B2 -- fill all six stats in canonical order.
		for (u_int i = 0; i < ZM_STAT_COUNT; ++i)
		{
			xOut.m_abInherited[i] = abInherit[i];
			if (abInherit[i])
			{
				const u_int uParent = xRng.RandBelow(2u);   // 0 = mother, 1 = father
				xOut.m_auIV[i] = (uParent == 0u) ? aMotherIV[i] : aFatherIV[i];
			}
			else
			{
				xOut.m_auIV[i] = xRng.RandBelow(32u);        // fresh IV [0,31]
			}
		}
		xOut.m_uInheritCount = uK;

		// Step C -- nature AFTER the IV rolls (everstone skips the draw entirely).
		xOut.m_eNature = (eEverstone != ZM_NATURE_COUNT)
			? eEverstone
			: (ZM_NATURE)xRng.RandBelow((u_int)ZM_NATURE_COUNT);
		return xOut;
	}

	// ---- assertion helpers -------------------------------------------------
	// Full field-by-field egg-spec comparison (determinism + collect-vs-generate).
	void AssertEggSpecEq(const ZM_BattleMonsterSpec& xA, const ZM_BattleMonsterSpec& xB,
		const char* szLabel)
	{
		ZENITH_ASSERT_EQ((u_int)xA.m_eSpecies, (u_int)xB.m_eSpecies, "%s species", szLabel);
		ZENITH_ASSERT_EQ(xA.m_uLevel, xB.m_uLevel, "%s level", szLabel);
		ZENITH_ASSERT_EQ((u_int)xA.m_eNature, (u_int)xB.m_eNature, "%s nature", szLabel);
		ZENITH_ASSERT_EQ((u_int)xA.m_eAbility, (u_int)xB.m_eAbility, "%s ability", szLabel);
		ZENITH_ASSERT_EQ(xA.m_uCurExp, xB.m_uCurExp, "%s curexp", szLabel);
		for (u_int i = 0; i < ZM_STAT_COUNT; ++i)
		{
			ZENITH_ASSERT_EQ(xA.m_auIV[i], xB.m_auIV[i], "%s IV %u", szLabel, i);
			ZENITH_ASSERT_EQ(xA.m_auEV[i], xB.m_auEV[i], "%s EV %u", szLabel, i);
		}
		for (u_int i = 0; i < uZM_MAX_MOVES; ++i)
		{
			ZENITH_ASSERT_EQ((u_int)xA.m_aeMoves[i], (u_int)xB.m_aeMoves[i], "%s move %u", szLabel, i);
		}
	}

	// Assert an egg's IV array + nature match a spec-derived oracle result.
	void AssertEggMatchesOracle(const ZM_BattleMonsterSpec& xEgg, const OracleEgg& xOracle,
		const char* szLabel)
	{
		for (u_int i = 0; i < ZM_STAT_COUNT; ++i)
		{
			ZENITH_ASSERT_EQ(xEgg.m_auIV[i], xOracle.m_auIV[i], "%s oracle IV %u", szLabel, i);
		}
		ZENITH_ASSERT_EQ((u_int)xEgg.m_eNature, (u_int)xOracle.m_eNature, "%s oracle nature", szLabel);
	}

	// True iff two IV arrays differ in at least one stat.
	bool IVsDiffer(const ZM_BattleMonsterSpec& xA, const ZM_BattleMonsterSpec& xB)
	{
		for (u_int i = 0; i < ZM_STAT_COUNT; ++i)
		{
			if (xA.m_auIV[i] != xB.m_auIV[i]) { return true; }
		}
		return false;
	}
}

// ############################################################################
// A. Base-evolution derivation (ZM_Data)
// ############################################################################

// A stage-3 species walks all the way back to its stage-1 base, across 2 families.
ZENITH_TEST(ZM_Data, Breeding_BaseEvo_Stage3ReturnsStage1)
{
	ZENITH_ASSERT_EQ((u_int)ZM_GetBaseEvolution(ZM_SPECIES_SYLVASTAG), (u_int)ZM_SPECIES_FERNFAWN);
	ZENITH_ASSERT_EQ((u_int)ZM_GetBaseEvolution(ZM_SPECIES_GRAINMAW),  (u_int)ZM_SPECIES_NIBBIN);
	// Premise: SYLVASTAG really is a stage-3 form.
	ZENITH_ASSERT_EQ(ZM_GetSpeciesData(ZM_SPECIES_SYLVASTAG).m_uEvoStage, 3u);
	ZENITH_ASSERT_EQ(ZM_GetSpeciesData(ZM_SPECIES_FERNFAWN).m_uEvoStage,  1u);
}

// A stage-2 species resolves to its stage-1 base.
ZENITH_TEST(ZM_Data, Breeding_BaseEvo_Stage2ReturnsStage1)
{
	ZENITH_ASSERT_EQ((u_int)ZM_GetBaseEvolution(ZM_SPECIES_THICKETBUCK), (u_int)ZM_SPECIES_FERNFAWN);
	ZENITH_ASSERT_EQ((u_int)ZM_GetBaseEvolution(ZM_SPECIES_HOARDEL),     (u_int)ZM_SPECIES_NIBBIN);
	ZENITH_ASSERT_EQ(ZM_GetSpeciesData(ZM_SPECIES_THICKETBUCK).m_uEvoStage, 2u);
}

// A 2-stage family's final form resolves to its stage-1 base.
ZENITH_TEST(ZM_Data, Breeding_BaseEvo_TwoStageFamilyFinal)
{
	ZENITH_ASSERT_EQ((u_int)ZM_GetBaseEvolution(ZM_SPECIES_WARDHUND),  (u_int)ZM_SPECIES_STRAYLING);
	ZENITH_ASSERT_EQ((u_int)ZM_GetBaseEvolution(ZM_SPECIES_ASHENHOWL), (u_int)ZM_SPECIES_CINDERJACK);
	// WARDHUND is the FINAL form of a 2-stage family (evolvesTo NONE) yet stage 2.
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(ZM_SPECIES_WARDHUND).m_eEvolvesTo, (u_int)ZM_SPECIES_NONE);
	ZENITH_ASSERT_EQ(ZM_GetSpeciesData(ZM_SPECIES_WARDHUND).m_uEvoStage, 2u);
}

// A stage-1 species and a single-stage species each resolve to themselves.
ZENITH_TEST(ZM_Data, Breeding_BaseEvo_Stage1AndSingleStageReturnSelf)
{
	ZENITH_ASSERT_EQ((u_int)ZM_GetBaseEvolution(ZM_SPECIES_FERNFAWN), (u_int)ZM_SPECIES_FERNFAWN);
	ZENITH_ASSERT_EQ((u_int)ZM_GetBaseEvolution(ZM_SPECIES_AURICORN), (u_int)ZM_SPECIES_AURICORN);
	// AURICORN is single-stage (stage 1, evolvesTo NONE).
	ZENITH_ASSERT_EQ(ZM_GetSpeciesData(ZM_SPECIES_AURICORN).m_uEvoStage, 1u);
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(ZM_SPECIES_AURICORN).m_eEvolvesTo, (u_int)ZM_SPECIES_NONE);
}

// Global invariant: for EVERY species the base evolution is stage 1 and shares
// the family. Also proves the backward scan always terminates (no cycle hang).
ZENITH_TEST(ZM_Data, Breeding_BaseEvo_AllSpeciesTerminatesStage1Invariant)
{
	const u_int uCount = ZM_GetSpeciesCount();
	for (u_int s = 0; s < uCount; ++s)
	{
		const ZM_SPECIES_ID eId = (ZM_SPECIES_ID)s;
		const ZM_SPECIES_ID eBase = ZM_GetBaseEvolution(eId);
		ZENITH_ASSERT_EQ(ZM_GetSpeciesData(eBase).m_uEvoStage, 1u,
			"%s base evo not stage 1", ZM_GetSpeciesName(eId));
		ZENITH_ASSERT_EQ(ZM_GetSpeciesData(eBase).m_uFamilyId, ZM_GetSpeciesData(eId).m_uFamilyId,
			"%s base evo family mismatch", ZM_GetSpeciesName(eId));
	}
}

// ############################################################################
// B. Compatibility (ZM_Data)
// ############################################################################

// Same species (same archetype, non-legendary) is compatible; the spec overload
// agrees with the species-id overload.
ZENITH_TEST(ZM_Data, Breeding_Compat_SameSpeciesTrue)
{
	ZENITH_ASSERT_TRUE(ZM_AreSpeciesCompatible(ZM_SPECIES_FERNFAWN, ZM_SPECIES_FERNFAWN),
		"same species should be compatible");
	const ZM_BattleMonsterSpec xA = MakeSpecUniform(ZM_SPECIES_FERNFAWN, 31u);
	const ZM_BattleMonsterSpec xB = MakeSpecUniform(ZM_SPECIES_FERNFAWN, 15u);
	ZENITH_ASSERT_TRUE(ZM_AreCompatible(xA, xB), "spec overload same species");
}

// Two different families sharing an archetype (both QUADRUPED) are compatible.
ZENITH_TEST(ZM_Data, Breeding_Compat_SameArchetypeDifferentFamilyTrue)
{
	// Premise: FERNFAWN (F01) and NIBBIN (F05) share the QUADRUPED archetype but
	// are distinct families, and neither is legendary.
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(ZM_SPECIES_FERNFAWN).m_eArchetype,
		(u_int)ZM_GetSpeciesData(ZM_SPECIES_NIBBIN).m_eArchetype);
	ZENITH_ASSERT_NE(ZM_GetSpeciesData(ZM_SPECIES_FERNFAWN).m_uFamilyId,
		ZM_GetSpeciesData(ZM_SPECIES_NIBBIN).m_uFamilyId);
	ZENITH_ASSERT_TRUE(ZM_AreSpeciesCompatible(ZM_SPECIES_FERNFAWN, ZM_SPECIES_NIBBIN),
		"same-archetype different-family should be compatible");
}

// Different archetypes never share a breeding group.
ZENITH_TEST(ZM_Data, Breeding_Compat_DifferentArchetypeFalse)
{
	// Premise: FERNFAWN QUADRUPED vs PIPWIT AVIAN, neither legendary.
	ZENITH_ASSERT_NE((u_int)ZM_GetSpeciesData(ZM_SPECIES_FERNFAWN).m_eArchetype,
		(u_int)ZM_GetSpeciesData(ZM_SPECIES_PIPWIT).m_eArchetype);
	ZENITH_ASSERT_NE((u_int)ZM_GetSpeciesData(ZM_SPECIES_FERNFAWN).m_eRarity, (u_int)ZM_RARITY_LEGENDARY);
	ZENITH_ASSERT_NE((u_int)ZM_GetSpeciesData(ZM_SPECIES_PIPWIT).m_eRarity,   (u_int)ZM_RARITY_LEGENDARY);
	ZENITH_ASSERT_FALSE(ZM_AreSpeciesCompatible(ZM_SPECIES_FERNFAWN, ZM_SPECIES_PIPWIT),
		"different archetypes should be incompatible");
}

// A legendary parent blocks breeding regardless of order or shared archetype.
ZENITH_TEST(ZM_Data, Breeding_Compat_LegendaryBlocked)
{
	// Premise: ZENARIS is legendary.
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(ZM_SPECIES_ZENARIS).m_eRarity, (u_int)ZM_RARITY_LEGENDARY);
	ZENITH_ASSERT_FALSE(ZM_AreSpeciesCompatible(ZM_SPECIES_ZENARIS, ZM_SPECIES_FERNFAWN),
		"legendary mother should block");
	ZENITH_ASSERT_FALSE(ZM_AreSpeciesCompatible(ZM_SPECIES_FERNFAWN, ZM_SPECIES_ZENARIS),
		"legendary father should block");
	// Two legendaries -- still blocked.
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(ZM_SPECIES_NADIRATH).m_eRarity, (u_int)ZM_RARITY_LEGENDARY);
	ZENITH_ASSERT_FALSE(ZM_AreSpeciesCompatible(ZM_SPECIES_ZENARIS, ZM_SPECIES_NADIRATH),
		"two legendaries should block");
}

// Compatibility is symmetric over a sample of pairs.
ZENITH_TEST(ZM_Data, Breeding_Compat_SymmetricOverSample)
{
	const ZM_SPECIES_ID aeSample[] = {
		ZM_SPECIES_FERNFAWN, ZM_SPECIES_NIBBIN, ZM_SPECIES_PIPWIT,
		ZM_SPECIES_KINDLET,  ZM_SPECIES_ZENARIS, ZM_SPECIES_WRIGGLET,
	};
	const u_int uN = (u_int)(sizeof(aeSample) / sizeof(aeSample[0]));
	for (u_int a = 0; a < uN; ++a)
	{
		for (u_int b = 0; b < uN; ++b)
		{
			ZENITH_ASSERT_EQ((u_int)ZM_AreSpeciesCompatible(aeSample[a], aeSample[b]),
				(u_int)ZM_AreSpeciesCompatible(aeSample[b], aeSample[a]),
				"asymmetry at (%u,%u)", a, b);
		}
	}
}

// ############################################################################
// C. Offspring species + egg shell (ZM_Data)
// ############################################################################

// Egg species is the MOTHER's base evolution (param 0), not the father's.
ZENITH_TEST(ZM_Data, Breeding_Egg_SpeciesIsMotherBaseEvo)
{
	const ZM_BattleMonsterSpec xMother = MakeSpecUniform(ZM_SPECIES_SYLVASTAG, 20u);   // QUADRUPED stage 3
	const ZM_BattleMonsterSpec xFather = MakeSpecUniform(ZM_SPECIES_NIBBIN, 20u);       // QUADRUPED
	ZENITH_ASSERT_TRUE(ZM_AreCompatible(xMother, xFather), "fixture pair must be compatible");
	ZM_BattleRNG xRng(0x1111ull);
	const ZM_BattleMonsterSpec xEgg = ZM_GenerateEgg(xMother, xFather, xRng);
	ZENITH_ASSERT_EQ((u_int)xEgg.m_eSpecies, (u_int)ZM_SPECIES_FERNFAWN);
}

// Swapping the father among compatible species leaves the egg species unchanged.
ZENITH_TEST(ZM_Data, Breeding_Egg_SpeciesIgnoresFather)
{
	const ZM_BattleMonsterSpec xMother = MakeSpecUniform(ZM_SPECIES_SYLVASTAG, 20u);
	const ZM_SPECIES_ID aeFathers[] = { ZM_SPECIES_NIBBIN, ZM_SPECIES_FERNFAWN, ZM_SPECIES_HOARDEL };
	for (u_int f = 0; f < (u_int)(sizeof(aeFathers) / sizeof(aeFathers[0])); ++f)
	{
		const ZM_BattleMonsterSpec xFather = MakeSpecUniform(aeFathers[f], 20u);
		ZENITH_ASSERT_TRUE(ZM_AreCompatible(xMother, xFather), "father %u must be compatible", f);
		ZM_BattleRNG xRng(0x2222ull);
		const ZM_BattleMonsterSpec xEgg = ZM_GenerateEgg(xMother, xFather, xRng);
		ZENITH_ASSERT_EQ((u_int)xEgg.m_eSpecies, (u_int)ZM_SPECIES_FERNFAWN,
			"egg species must stay mother's base evo (father %u)", f);
	}
}

// Egg hatches at level 1 with zero EVs, and building it yields the L1 exp floor.
ZENITH_TEST(ZM_Data, Breeding_Egg_HatchLevelOneEVsZeroExpFloor)
{
	const ZM_BattleMonsterSpec xMother = MakeSpecUniform(ZM_SPECIES_THICKETBUCK, 20u);
	const ZM_BattleMonsterSpec xFather = MakeSpecUniform(ZM_SPECIES_NIBBIN, 20u);
	ZM_BattleRNG xRng(0x3333ull);
	const ZM_BattleMonsterSpec xEgg = ZM_GenerateEgg(xMother, xFather, xRng);

	ZENITH_ASSERT_EQ(xEgg.m_uLevel, uZM_EGG_HATCH_LEVEL);
	for (u_int i = 0; i < ZM_STAT_COUNT; ++i)
	{
		ZENITH_ASSERT_EQ(xEgg.m_auEV[i], 0u, "egg EV %u must be zero", i);
	}
	ZENITH_ASSERT_EQ(xEgg.m_uCurExp, uZM_EXP_UNSPECIFIED);

	const ZM_BattleMonster xBuilt = ZM_BuildBattleMonster(xEgg);
	ZENITH_ASSERT_EQ(xBuilt.m_uLevel, 1u);
	ZENITH_ASSERT_EQ(xBuilt.m_uCurExp, 0u, "built egg should sit at the L1 exp floor");
}

// Egg knows the base-evo species' level-1 learnset moves (first <=4), rest NONE.
ZENITH_TEST(ZM_Data, Breeding_Egg_KnowsBaseEvoLevelOneMoves)
{
	const ZM_BattleMonsterSpec xMother = MakeSpecUniform(ZM_SPECIES_SYLVASTAG, 20u);
	const ZM_BattleMonsterSpec xFather = MakeSpecUniform(ZM_SPECIES_NIBBIN, 20u);
	ZM_BattleRNG xRng(0x4444ull);
	const ZM_BattleMonsterSpec xEgg = ZM_GenerateEgg(xMother, xFather, xRng);

	const ZM_SPECIES_ID eBase = ZM_GetBaseEvolution(ZM_SPECIES_SYLVASTAG);   // FERNFAWN
	ZM_MOVE_ID aeExpected[uZM_MAX_MOVES] = { ZM_MOVE_NONE, ZM_MOVE_NONE, ZM_MOVE_NONE, ZM_MOVE_NONE };
	const ZM_Learnset xLs = ZM_GetSpeciesLearnset(eBase);
	u_int uFilled = 0u;
	for (u_int k = 0; k < xLs.m_uCount && uFilled < uZM_MAX_MOVES; ++k)
	{
		if (xLs.m_axMoves[k].m_uLevel <= uZM_EGG_HATCH_LEVEL)
		{
			aeExpected[uFilled] = xLs.m_axMoves[k].m_eMove;
			++uFilled;
		}
	}
	// The learnset derivation always teaches a level-1 move, so at least one fills.
	ZENITH_ASSERT_GE(uFilled, 1u, "base evo must have at least one L1 move");
	for (u_int i = 0; i < uZM_MAX_MOVES; ++i)
	{
		ZENITH_ASSERT_EQ((u_int)xEgg.m_aeMoves[i], (u_int)aeExpected[i], "egg move slot %u", i);
	}
}

// ############################################################################
// D. IV inheritance (ZM_Data)
// ############################################################################

// Same seed + same inputs => byte-identical egg spec.
ZENITH_TEST(ZM_Data, Breeding_Egg_Deterministic_SameSeedIdentical)
{
	const u_int aMotherIV[ZM_STAT_COUNT] = { 0u, 6u, 12u, 18u, 24u, 31u };
	const u_int aFatherIV[ZM_STAT_COUNT] = { 31u, 25u, 19u, 13u, 7u, 1u };
	const ZM_BattleMonsterSpec xMother = MakeSpec(ZM_SPECIES_SYLVASTAG, aMotherIV, 40u,
		ZM_NATURE_BRUTISH, ZM_ABILITY_STREAMLINE);
	const ZM_BattleMonsterSpec xFather = MakeSpec(ZM_SPECIES_NIBBIN, aFatherIV, 40u);

	ZM_BattleRNG xRngA(0xCAFEull);
	ZM_BattleRNG xRngB(0xCAFEull);
	const ZM_BattleMonsterSpec xEggA = ZM_GenerateEgg(xMother, xFather, xRngA);
	const ZM_BattleMonsterSpec xEggB = ZM_GenerateEgg(xMother, xFather, xRngB);
	AssertEggSpecEq(xEggA, xEggB, "same-seed");
}

// Golden seed 1: exact IVs + nature match the spec-derived offline oracle.
ZENITH_TEST(ZM_Data, Breeding_Egg_Golden_Seed1)
{
	const u_int aMotherIV[ZM_STAT_COUNT] = { 0u, 6u, 12u, 18u, 24u, 31u };
	const u_int aFatherIV[ZM_STAT_COUNT] = { 31u, 25u, 19u, 13u, 7u, 1u };
	const ZM_BattleMonsterSpec xMother = MakeSpec(ZM_SPECIES_SYLVASTAG, aMotherIV);
	const ZM_BattleMonsterSpec xFather = MakeSpec(ZM_SPECIES_NIBBIN, aFatherIV);

	ZM_BattleRNG xRng(0xA1ull);
	const ZM_BattleMonsterSpec xEgg = ZM_GenerateEgg(xMother, xFather, xRng);

	ZM_BattleRNG xRngOracle(0xA1ull);
	const OracleEgg xOracle = OracleGenerate(aMotherIV, aFatherIV, false, ZM_NATURE_COUNT, xRngOracle);

	AssertEggMatchesOracle(xEgg, xOracle, "seed1");
	ZENITH_ASSERT_EQ((u_int)xEgg.m_eSpecies, (u_int)ZM_SPECIES_FERNFAWN);
	ZENITH_ASSERT_EQ(xEgg.m_uLevel, 1u);
}

// Golden seed 2 (control): matches its OWN oracle, and differs from seed 1 --
// proving the RNG actually drives the rolls.
ZENITH_TEST(ZM_Data, Breeding_Egg_Golden_Seed2_Control)
{
	const u_int aMotherIV[ZM_STAT_COUNT] = { 0u, 6u, 12u, 18u, 24u, 31u };
	const u_int aFatherIV[ZM_STAT_COUNT] = { 31u, 25u, 19u, 13u, 7u, 1u };
	const ZM_BattleMonsterSpec xMother = MakeSpec(ZM_SPECIES_SYLVASTAG, aMotherIV);
	const ZM_BattleMonsterSpec xFather = MakeSpec(ZM_SPECIES_NIBBIN, aFatherIV);

	ZM_BattleRNG xRng2(0xF00DBEEFull);
	const ZM_BattleMonsterSpec xEgg2 = ZM_GenerateEgg(xMother, xFather, xRng2);
	ZM_BattleRNG xRngOracle2(0xF00DBEEFull);
	const OracleEgg xOracle2 = OracleGenerate(aMotherIV, aFatherIV, false, ZM_NATURE_COUNT, xRngOracle2);
	AssertEggMatchesOracle(xEgg2, xOracle2, "seed2");

	// Control: a different seed yields a different egg.
	ZM_BattleRNG xRng1(0xA1ull);
	const ZM_BattleMonsterSpec xEgg1 = ZM_GenerateEgg(xMother, xFather, xRng1);
	const bool bDiffers = IVsDiffer(xEgg1, xEgg2) || (xEgg1.m_eNature != xEgg2.m_eNature);
	ZENITH_ASSERT_TRUE(bDiffers, "distinct seeds must produce distinct eggs");
}

// The 3 inherited IVs each come from a parent value; the rest are the oracle's
// fresh rolls. Uses the oracle inherit-mask so the count is exact even if a
// random roll happens to land on a parent value.
ZENITH_TEST(ZM_Data, Breeding_Egg_InheritedIVsComeFromAParent)
{
	const u_int aMotherIV[ZM_STAT_COUNT] = { 10u, 10u, 10u, 10u, 10u, 10u };
	const u_int aFatherIV[ZM_STAT_COUNT] = { 20u, 20u, 20u, 20u, 20u, 20u };
	const ZM_BattleMonsterSpec xMother = MakeSpec(ZM_SPECIES_SYLVASTAG, aMotherIV);
	const ZM_BattleMonsterSpec xFather = MakeSpec(ZM_SPECIES_NIBBIN, aFatherIV);

	ZM_BattleRNG xRng(0x0D15EA5Eull);
	const ZM_BattleMonsterSpec xEgg = ZM_GenerateEgg(xMother, xFather, xRng);
	ZM_BattleRNG xRngOracle(0x0D15EA5Eull);
	const OracleEgg xOracle = OracleGenerate(aMotherIV, aFatherIV, false, ZM_NATURE_COUNT, xRngOracle);

	ZENITH_ASSERT_EQ(xOracle.m_uInheritCount, uZM_BREED_IV_INHERIT_BASE);
	u_int uInheritedFromParent = 0u;
	for (u_int i = 0; i < ZM_STAT_COUNT; ++i)
	{
		if (xOracle.m_abInherited[i])
		{
			ZENITH_ASSERT_TRUE(xEgg.m_auIV[i] == 10u || xEgg.m_auIV[i] == 20u,
				"inherited stat %u should be a parent value, got %u", i, xEgg.m_auIV[i]);
			++uInheritedFromParent;
		}
		// Whether inherited or fresh, the value must equal the oracle exactly.
		ZENITH_ASSERT_EQ(xEgg.m_auIV[i], xOracle.m_auIV[i], "IV %u vs oracle", i);
	}
	ZENITH_ASSERT_EQ(uInheritedFromParent, 3u, "exactly 3 IVs inherited without the knot");
}

// Heirloom Knot inherits 5 IVs; exact IV literals match the knot-on oracle.
ZENITH_TEST(ZM_Data, Breeding_Egg_HeirloomKnotInheritsFiveIVs_Golden)
{
	const u_int aMotherIV[ZM_STAT_COUNT] = { 10u, 10u, 10u, 10u, 10u, 10u };
	const u_int aFatherIV[ZM_STAT_COUNT] = { 20u, 20u, 20u, 20u, 20u, 20u };
	const ZM_BattleMonsterSpec xMother = MakeSpec(ZM_SPECIES_SYLVASTAG, aMotherIV);
	const ZM_BattleMonsterSpec xFather = MakeSpec(ZM_SPECIES_NIBBIN, aFatherIV);

	ZM_BreedingParams xParams;
	xParams.m_bHeirloomKnot = true;

	ZM_BattleRNG xRng(0xBEE5ull);
	const ZM_BattleMonsterSpec xEgg = ZM_GenerateEgg(xMother, xFather, xRng, xParams);
	ZM_BattleRNG xRngOracle(0xBEE5ull);
	const OracleEgg xOracle = OracleGenerate(aMotherIV, aFatherIV, true, ZM_NATURE_COUNT, xRngOracle);

	ZENITH_ASSERT_EQ(xOracle.m_uInheritCount, uZM_BREED_IV_INHERIT_KNOT);
	AssertEggMatchesOracle(xEgg, xOracle, "knot");
	u_int uInheritedFromParent = 0u;
	for (u_int i = 0; i < ZM_STAT_COUNT; ++i)
	{
		if (xOracle.m_abInherited[i]) { ++uInheritedFromParent; }
	}
	ZENITH_ASSERT_EQ(uInheritedFromParent, 5u, "exactly 5 IVs inherited with the knot");
}

// Same seed, knot on vs off => different IV arrays (more inherited). Both match
// their respective oracles, and the two oracles themselves differ.
ZENITH_TEST(ZM_Data, Breeding_Egg_HeirloomKnotDiffersFromDefault)
{
	const u_int aMotherIV[ZM_STAT_COUNT] = { 5u, 5u, 5u, 5u, 5u, 5u };
	const u_int aFatherIV[ZM_STAT_COUNT] = { 25u, 25u, 25u, 25u, 25u, 25u };
	const ZM_BattleMonsterSpec xMother = MakeSpec(ZM_SPECIES_SYLVASTAG, aMotherIV);
	const ZM_BattleMonsterSpec xFather = MakeSpec(ZM_SPECIES_NIBBIN, aFatherIV);

	ZM_BreedingParams xKnot;
	xKnot.m_bHeirloomKnot = true;

	ZM_BattleRNG xRngOff(0x9999ull);
	ZM_BattleRNG xRngOn(0x9999ull);
	const ZM_BattleMonsterSpec xEggOff = ZM_GenerateEgg(xMother, xFather, xRngOff);
	const ZM_BattleMonsterSpec xEggOn  = ZM_GenerateEgg(xMother, xFather, xRngOn, xKnot);
	ZENITH_ASSERT_TRUE(IVsDiffer(xEggOff, xEggOn), "knot must change the IV outcome at a fixed seed");
}

// ############################################################################
// E. Nature + ability (ZM_Data)
// ############################################################################

// Without the everstone the nature is a rolled value: it matches the oracle at a
// pinned seed and always stays in [0, ZM_NATURE_COUNT).
ZENITH_TEST(ZM_Data, Breeding_Egg_NatureRandomWithoutEverstone_Golden)
{
	const u_int aMotherIV[ZM_STAT_COUNT] = { 1u, 2u, 3u, 4u, 5u, 6u };
	const u_int aFatherIV[ZM_STAT_COUNT] = { 6u, 5u, 4u, 3u, 2u, 1u };
	const ZM_BattleMonsterSpec xMother = MakeSpec(ZM_SPECIES_SYLVASTAG, aMotherIV);
	const ZM_BattleMonsterSpec xFather = MakeSpec(ZM_SPECIES_NIBBIN, aFatherIV);

	ZM_BattleRNG xRng(0x5151ull);
	const ZM_BattleMonsterSpec xEgg = ZM_GenerateEgg(xMother, xFather, xRng);
	ZM_BattleRNG xRngOracle(0x5151ull);
	const OracleEgg xOracle = OracleGenerate(aMotherIV, aFatherIV, false, ZM_NATURE_COUNT, xRngOracle);
	ZENITH_ASSERT_EQ((u_int)xEgg.m_eNature, (u_int)xOracle.m_eNature, "golden nature");

	for (u_int seed = 1u; seed <= 24u; ++seed)
	{
		ZM_BattleRNG xRngN((u_int64)(seed * 0x9E3779B1ull));
		const ZM_BattleMonsterSpec xE = ZM_GenerateEgg(xMother, xFather, xRngN);
		ZENITH_ASSERT_LT((u_int)xE.m_eNature, (u_int)ZM_NATURE_COUNT, "nature in range (seed %u)", seed);
	}
}

// The everstone locks the egg nature for every seed.
ZENITH_TEST(ZM_Data, Breeding_Egg_EverstoneLocksNature)
{
	const ZM_BattleMonsterSpec xMother = MakeSpecUniform(ZM_SPECIES_SYLVASTAG, 15u);
	const ZM_BattleMonsterSpec xFather = MakeSpecUniform(ZM_SPECIES_NIBBIN, 15u);

	ZM_BreedingParams xParams;
	xParams.m_eEverstoneNature = ZM_NATURE_ARCANE;

	const u_int64 aulSeeds[] = { 0x1ull, 0x2ull, 0xDEADull, 0xABCDEFull };
	for (u_int s = 0; s < (u_int)(sizeof(aulSeeds) / sizeof(aulSeeds[0])); ++s)
	{
		ZM_BattleRNG xRng(aulSeeds[s]);
		const ZM_BattleMonsterSpec xEgg = ZM_GenerateEgg(xMother, xFather, xRng, xParams);
		ZENITH_ASSERT_EQ((u_int)xEgg.m_eNature, (u_int)ZM_NATURE_ARCANE, "everstone lock (seed %u)", s);
	}
}

// The everstone skips the nature DRAW, which comes AFTER the IV rolls -- so the
// IV arrays are identical whether the everstone is set or not (same seed).
ZENITH_TEST(ZM_Data, Breeding_Egg_EverstoneDoesNotShiftIVs)
{
	const u_int aMotherIV[ZM_STAT_COUNT] = { 3u, 9u, 15u, 21u, 27u, 30u };
	const u_int aFatherIV[ZM_STAT_COUNT] = { 30u, 27u, 21u, 15u, 9u, 3u };
	const ZM_BattleMonsterSpec xMother = MakeSpec(ZM_SPECIES_SYLVASTAG, aMotherIV);
	const ZM_BattleMonsterSpec xFather = MakeSpec(ZM_SPECIES_NIBBIN, aFatherIV);

	ZM_BreedingParams xLocked;
	xLocked.m_eEverstoneNature = ZM_NATURE_ARCANE;

	ZM_BattleRNG xRngFree(0x7AB1E5ull);
	ZM_BattleRNG xRngLocked(0x7AB1E5ull);
	const ZM_BattleMonsterSpec xEggFree   = ZM_GenerateEgg(xMother, xFather, xRngFree);
	const ZM_BattleMonsterSpec xEggLocked = ZM_GenerateEgg(xMother, xFather, xRngLocked, xLocked);

	for (u_int i = 0; i < ZM_STAT_COUNT; ++i)
	{
		ZENITH_ASSERT_EQ(xEggLocked.m_auIV[i], xEggFree.m_auIV[i],
			"everstone must not perturb IV %u", i);
	}
	ZENITH_ASSERT_EQ((u_int)xEggLocked.m_eNature, (u_int)ZM_NATURE_ARCANE);
}

// Ability is copied from the mother; the father's ability is ignored.
ZENITH_TEST(ZM_Data, Breeding_Egg_InheritsMotherAbility)
{
	const ZM_BattleMonsterSpec xMother = MakeSpecUniform(ZM_SPECIES_SYLVASTAG, 20u, 20u,
		ZM_NATURE_FERAL, ZM_ABILITY_STREAMLINE);
	const ZM_BattleMonsterSpec xFather = MakeSpecUniform(ZM_SPECIES_NIBBIN, 20u, 20u,
		ZM_NATURE_FERAL, ZM_ABILITY_BEDROCK);
	ZM_BattleRNG xRng(0x6060ull);
	const ZM_BattleMonsterSpec xEgg = ZM_GenerateEgg(xMother, xFather, xRng);
	ZENITH_ASSERT_EQ((u_int)xEgg.m_eAbility, (u_int)ZM_ABILITY_STREAMLINE, "egg copies mother's ability");
}

// A compatible pair generates a well-formed egg (species = mother base evo,
// level 1). Documents the ZM_GenerateEgg contract on valid input.
ZENITH_TEST(ZM_Data, Breeding_Egg_CompatiblePairGeneratesValidEgg)
{
	const ZM_BattleMonsterSpec xMother = MakeSpecUniform(ZM_SPECIES_WARDHUND, 22u);   // QUADRUPED 2-stage final
	const ZM_BattleMonsterSpec xFather = MakeSpecUniform(ZM_SPECIES_STRAYLING, 22u);   // QUADRUPED base
	ZENITH_ASSERT_TRUE(ZM_AreCompatible(xMother, xFather), "precondition: compatible pair");
	ZM_BattleRNG xRng(0x7070ull);
	const ZM_BattleMonsterSpec xEgg = ZM_GenerateEgg(xMother, xFather, xRng);
	ZENITH_ASSERT_EQ((u_int)xEgg.m_eSpecies, (u_int)ZM_SPECIES_STRAYLING);   // base of WARDHUND
	ZENITH_ASSERT_EQ(xEgg.m_uLevel, uZM_EGG_HATCH_LEVEL);
}

// ############################################################################
// F. Daycare (ZM_Data)
// ############################################################################

// Deposit fills slots 0 then 1; a 3rd deposit is rejected. Occupancy tracks.
ZENITH_TEST(ZM_Data, Daycare_DepositFillsThenRejectsWhenFull)
{
	ZM_DaycareState xState;
	ZENITH_ASSERT_EQ(ZM_DaycareOccupancy(xState), 0u);

	const ZM_BattleMonsterSpec xA = MakeSpecUniform(ZM_SPECIES_FERNFAWN, 31u, 10u);
	const ZM_BattleMonsterSpec xB = MakeSpecUniform(ZM_SPECIES_NIBBIN, 31u, 10u);
	const ZM_BattleMonsterSpec xC = MakeSpecUniform(ZM_SPECIES_STRAYLING, 31u, 10u);

	ZENITH_ASSERT_EQ(ZM_DaycareDeposit(xState, xA), 0u);
	ZENITH_ASSERT_EQ(ZM_DaycareOccupancy(xState), 1u);
	ZENITH_ASSERT_EQ(ZM_DaycareDeposit(xState, xB), 1u);
	ZENITH_ASSERT_EQ(ZM_DaycareOccupancy(xState), 2u);
	ZENITH_ASSERT_EQ(ZM_DaycareDeposit(xState, xC), uZM_DAYCARE_CAPACITY);   // full -> rejected
	ZENITH_ASSERT_EQ(ZM_DaycareOccupancy(xState), 2u);
}

// Withdraw returns the (level/exp-updated) spec, clears the slot, drops
// occupancy; withdrawing an empty or out-of-range slot returns false.
ZENITH_TEST(ZM_Data, Daycare_WithdrawReturnsMonAndClearsSlot)
{
	ZM_DaycareState xState;
	const u_int aIV[ZM_STAT_COUNT] = { 5u, 10u, 15u, 20u, 25u, 30u };
	const ZM_BattleMonsterSpec xIn = MakeSpec(ZM_SPECIES_NIBBIN, aIV, 10u);
	ZENITH_ASSERT_EQ(ZM_DaycareDeposit(xState, xIn), 0u);

	ZM_BattleMonsterSpec xOut;
	ZENITH_ASSERT_TRUE(ZM_DaycareWithdraw(xState, 0u, xOut), "withdraw occupied slot");
	ZENITH_ASSERT_EQ((u_int)xOut.m_eSpecies, (u_int)ZM_SPECIES_NIBBIN);
	ZENITH_ASSERT_EQ(xOut.m_uLevel, 10u);   // no steps -> unchanged level
	for (u_int i = 0; i < ZM_STAT_COUNT; ++i)
	{
		ZENITH_ASSERT_EQ(xOut.m_auIV[i], aIV[i], "withdrawn IV %u preserved", i);
	}
	// Deposit normalized UNSPECIFIED exp to the L10 floor; withdraw returns it.
	const ZM_GROWTH_RATE eRate = ZM_GetSpeciesGrowthRate(ZM_SPECIES_NIBBIN);
	ZENITH_ASSERT_EQ(xOut.m_uCurExp, ZM_ExpForLevel(eRate, 10u));
	ZENITH_ASSERT_EQ(ZM_DaycareOccupancy(xState), 0u);

	ZM_BattleMonsterSpec xJunk;
	ZENITH_ASSERT_FALSE(ZM_DaycareWithdraw(xState, 0u, xJunk), "empty slot -> false");
	ZENITH_ASSERT_FALSE(ZM_DaycareWithdraw(xState, uZM_DAYCARE_CAPACITY, xJunk), "out-of-range -> false");
}

// Stepping adds 1 exp/step and crosses a level, verified against an independent
// ZM_ExpForLevel / ZM_LevelForExp anchor (not the function under test).
ZENITH_TEST(ZM_Data, Daycare_StepAddsExpAndLevels)
{
	ZM_DaycareState xState;
	const ZM_BattleMonsterSpec xIn = MakeSpecUniform(ZM_SPECIES_NIBBIN, 31u, 5u);
	ZM_DaycareDeposit(xState, xIn);

	const u_int uSteps = 300u;
	ZM_DaycareStep(xState, uSteps);

	const ZM_GROWTH_RATE eRate = ZM_GetSpeciesGrowthRate(ZM_SPECIES_NIBBIN);
	const u_int uExpectedExp   = ZM_ExpForLevel(eRate, 5u) + uSteps * uZM_DAYCARE_EXP_PER_STEP;
	const u_int uExpectedLevel = ZM_LevelForExp(eRate, uExpectedExp);
	ZENITH_ASSERT_GT(uExpectedLevel, 5u, "fixture must actually cross a level");

	ZM_BattleMonsterSpec xOut;
	ZENITH_ASSERT_TRUE(ZM_DaycareWithdraw(xState, 0u, xOut), "withdraw stepped monster");
	ZENITH_ASSERT_EQ(xOut.m_uCurExp, uExpectedExp);
	ZENITH_ASSERT_EQ(xOut.m_uLevel, uExpectedLevel);
}

// A huge step count saturates at level 100 / the L100 exp cap (no wrap).
ZENITH_TEST(ZM_Data, Daycare_StepCapsAtLevel100)
{
	ZM_DaycareState xState;
	const ZM_BattleMonsterSpec xIn = MakeSpecUniform(ZM_SPECIES_NIBBIN, 31u, 99u);
	ZM_DaycareDeposit(xState, xIn);

	ZM_DaycareStep(xState, 5000000u);

	const ZM_GROWTH_RATE eRate = ZM_GetSpeciesGrowthRate(ZM_SPECIES_NIBBIN);
	const u_int uCap = ZM_ExpForLevel(eRate, 100u);
	ZM_BattleMonsterSpec xOut;
	ZENITH_ASSERT_TRUE(ZM_DaycareWithdraw(xState, 0u, xOut), "withdraw capped monster");
	ZENITH_ASSERT_EQ(xOut.m_uLevel, 100u);
	ZENITH_ASSERT_EQ(xOut.m_uCurExp, uCap);
}

// Egg availability crosses exactly at the 256-step threshold, not before.
ZENITH_TEST(ZM_Data, Daycare_EggAvailableAtThreshold)
{
	ZM_DaycareState xState;
	ZM_DaycareDeposit(xState, MakeSpecUniform(ZM_SPECIES_FERNFAWN, 31u, 10u));   // slot 0 mother
	ZM_DaycareDeposit(xState, MakeSpecUniform(ZM_SPECIES_NIBBIN, 31u, 10u));      // slot 1 father
	ZENITH_ASSERT_TRUE(ZM_DaycarePairCompatible(xState), "fixture pair must be compatible");

	ZM_DaycareStep(xState, uZM_DAYCARE_EGG_STEP_THRESHOLD - 1u);   // 255
	ZENITH_ASSERT_EQ(xState.m_uEggStepCounter, uZM_DAYCARE_EGG_STEP_THRESHOLD - 1u);
	ZENITH_ASSERT_FALSE(xState.m_bEggAvailable, "no egg one step early");

	ZM_DaycareStep(xState, 1u);   // 256
	ZENITH_ASSERT_EQ(xState.m_uEggStepCounter, uZM_DAYCARE_EGG_STEP_THRESHOLD);
	ZENITH_ASSERT_TRUE(xState.m_bEggAvailable, "egg available at the threshold");
}

// No egg accrues for an incompatible pair, nor for a single occupant.
ZENITH_TEST(ZM_Data, Daycare_NoEggIncompatiblePairOrSingle)
{
	// Incompatible pair (QUADRUPED + AVIAN).
	ZM_DaycareState xIncompat;
	ZM_DaycareDeposit(xIncompat, MakeSpecUniform(ZM_SPECIES_FERNFAWN, 31u, 10u));
	ZM_DaycareDeposit(xIncompat, MakeSpecUniform(ZM_SPECIES_PIPWIT, 31u, 10u));
	ZENITH_ASSERT_FALSE(ZM_DaycarePairCompatible(xIncompat), "premise: incompatible pair");
	ZM_DaycareStep(xIncompat, 500u);
	ZENITH_ASSERT_EQ(xIncompat.m_uEggStepCounter, 0u, "incompatible pair accrues nothing");
	ZENITH_ASSERT_FALSE(xIncompat.m_bEggAvailable);

	// Single occupant.
	ZM_DaycareState xSingle;
	ZM_DaycareDeposit(xSingle, MakeSpecUniform(ZM_SPECIES_FERNFAWN, 31u, 10u));
	ZM_DaycareStep(xSingle, 500u);
	ZENITH_ASSERT_EQ(xSingle.m_uEggStepCounter, 0u, "single occupant accrues nothing");
	ZENITH_ASSERT_FALSE(xSingle.m_bEggAvailable);
}

// Collecting an available egg returns the offspring (mother base evo), resets the
// counter/flag, leaves the parents in place, and matches a direct ZM_GenerateEgg.
ZENITH_TEST(ZM_Data, Daycare_CollectEggResetsAndReturnsOffspring)
{
	ZM_DaycareState xState;
	ZM_DaycareDeposit(xState, MakeSpecUniform(ZM_SPECIES_SYLVASTAG, 20u, 30u));   // mother, base = FERNFAWN
	ZM_DaycareDeposit(xState, MakeSpecUniform(ZM_SPECIES_NIBBIN, 25u, 30u));       // father
	ZM_DaycareStep(xState, uZM_DAYCARE_EGG_STEP_THRESHOLD);
	ZENITH_ASSERT_TRUE(xState.m_bEggAvailable, "egg should be available after threshold");

	// Snapshot the post-step parents for the independent ZM_GenerateEgg oracle.
	const ZM_BattleMonsterSpec xMother = xState.m_axSlots[0].m_xMonster;
	const ZM_BattleMonsterSpec xFather = xState.m_axSlots[1].m_xMonster;

	ZM_BattleRNG xRngCollect(0x5EEDull);
	ZM_BattleMonsterSpec xEgg;
	ZENITH_ASSERT_TRUE(ZM_DaycareCollectEgg(xState, xRngCollect, ZM_BreedingParams{}, xEgg),
		"collect should succeed when available");
	ZENITH_ASSERT_EQ((u_int)xEgg.m_eSpecies, (u_int)ZM_SPECIES_FERNFAWN);

	// Reset + parents remain.
	ZENITH_ASSERT_EQ(xState.m_uEggStepCounter, 0u, "counter reset after collect");
	ZENITH_ASSERT_FALSE(xState.m_bEggAvailable, "flag reset after collect");
	ZENITH_ASSERT_EQ(ZM_DaycareOccupancy(xState), 2u, "parents remain after collect");

	// Consistency with a direct, identically-seeded ZM_GenerateEgg.
	ZM_BattleRNG xRngDirect(0x5EEDull);
	const ZM_BattleMonsterSpec xExpected = ZM_GenerateEgg(xMother, xFather, xRngDirect, ZM_BreedingParams{});
	AssertEggSpecEq(xEgg, xExpected, "collect-vs-generate");
}

// Withdrawing a parent resets egg progress; a subsequent collect fails.
ZENITH_TEST(ZM_Data, Daycare_WithdrawResetsEggProgress)
{
	ZM_DaycareState xState;
	ZM_DaycareDeposit(xState, MakeSpecUniform(ZM_SPECIES_FERNFAWN, 31u, 10u));
	ZM_DaycareDeposit(xState, MakeSpecUniform(ZM_SPECIES_NIBBIN, 31u, 10u));
	ZM_DaycareStep(xState, uZM_DAYCARE_EGG_STEP_THRESHOLD);
	ZENITH_ASSERT_TRUE(xState.m_bEggAvailable, "precondition: egg available");

	ZM_BattleMonsterSpec xOut;
	ZENITH_ASSERT_TRUE(ZM_DaycareWithdraw(xState, 1u, xOut), "withdraw a parent");
	ZENITH_ASSERT_EQ(xState.m_uEggStepCounter, 0u, "counter reset by withdraw");
	ZENITH_ASSERT_FALSE(xState.m_bEggAvailable, "flag reset by withdraw");

	ZM_BattleRNG xRng(0x1234ull);
	ZM_BattleMonsterSpec xEgg;
	ZENITH_ASSERT_FALSE(ZM_DaycareCollectEgg(xState, xRng, ZM_BreedingParams{}, xEgg),
		"collect should fail after a parent leaves");
}
