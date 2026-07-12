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

	// Return a copy of xSpec with an explicit gender (box-6 SC-B). The gender-aware
	// ZM_AreCompatible / ZM_GenerateEgg now REQUIRE a valid pair -- one MALE + one
	// FEMALE, or the universal breeder (GLOOPET) with any non-universal partner. A
	// default spec is GENDERLESS, so every breeding fixture must set genders. Applied
	// consistently so the intended dam stays the FEMALE / non-universal parent, which
	// keeps each mother-sourced golden (offspring species, ability, inherited IVs)
	// byte-identical to the pre-SC-B value.
	ZM_BattleMonsterSpec WithGender(ZM_BattleMonsterSpec xSpec, ZM_GENDER eGender)
	{
		xSpec.m_eGender = eGender;
		return xSpec;
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
		ZENITH_ASSERT_EQ((u_int)xA.m_eGender, (u_int)xB.m_eGender, "%s gender", szLabel);   // box-6 SC-A
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
	// SC-B: the spec overload now needs a valid gender pairing (one MALE + one FEMALE).
	const ZM_BattleMonsterSpec xA = WithGender(MakeSpecUniform(ZM_SPECIES_FERNFAWN, 31u), ZM_GENDER_MALE);
	const ZM_BattleMonsterSpec xB = WithGender(MakeSpecUniform(ZM_SPECIES_FERNFAWN, 15u), ZM_GENDER_FEMALE);
	ZENITH_ASSERT_TRUE(ZM_AreCompatible(xA, xB), "spec overload same species, opposite gender");
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

// FERNFAWN and PIPWIT have DISJOINT egg-group lists ({FIELD,PLANT} vs {FLYING}), so they
// cannot breed. (SC-B: different archetypes CAN share a type-derived secondary group -- e.g.
// a GRASS quadruped and a GRASS plantoid both reach PLANT; this pair simply doesn't overlap.)
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

// Egg species is the MOTHER's base evolution, i.e. the FEMALE parent's (SC-B), not the
// father's. The dam is set FEMALE so the mother role -- and thus FERNFAWN -- is pinned.
ZENITH_TEST(ZM_Data, Breeding_Egg_SpeciesIsMotherBaseEvo)
{
	const ZM_BattleMonsterSpec xMother = WithGender(MakeSpecUniform(ZM_SPECIES_SYLVASTAG, 20u), ZM_GENDER_FEMALE);   // QUADRUPED stage 3
	const ZM_BattleMonsterSpec xFather = WithGender(MakeSpecUniform(ZM_SPECIES_NIBBIN, 20u), ZM_GENDER_MALE);         // QUADRUPED
	ZENITH_ASSERT_TRUE(ZM_AreCompatible(xMother, xFather), "fixture pair must be compatible");
	ZM_BattleRNG xRng(0x1111ull);
	const ZM_BattleMonsterSpec xEgg = ZM_GenerateEgg(xMother, xFather, xRng);
	ZENITH_ASSERT_EQ((u_int)xEgg.m_eSpecies, (u_int)ZM_SPECIES_FERNFAWN);
}

// Swapping the (MALE) father among compatible species leaves the egg species unchanged:
// it follows the FEMALE mother (SYLVASTAG -> FERNFAWN).
ZENITH_TEST(ZM_Data, Breeding_Egg_SpeciesIgnoresFather)
{
	const ZM_BattleMonsterSpec xMother = WithGender(MakeSpecUniform(ZM_SPECIES_SYLVASTAG, 20u), ZM_GENDER_FEMALE);
	const ZM_SPECIES_ID aeFathers[] = { ZM_SPECIES_NIBBIN, ZM_SPECIES_FERNFAWN, ZM_SPECIES_HOARDEL };
	for (u_int f = 0; f < (u_int)(sizeof(aeFathers) / sizeof(aeFathers[0])); ++f)
	{
		const ZM_BattleMonsterSpec xFather = WithGender(MakeSpecUniform(aeFathers[f], 20u), ZM_GENDER_MALE);
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
	const ZM_BattleMonsterSpec xMother = WithGender(MakeSpecUniform(ZM_SPECIES_THICKETBUCK, 20u), ZM_GENDER_FEMALE);
	const ZM_BattleMonsterSpec xFather = WithGender(MakeSpecUniform(ZM_SPECIES_NIBBIN, 20u), ZM_GENDER_MALE);
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
	const ZM_BattleMonsterSpec xMother = WithGender(MakeSpecUniform(ZM_SPECIES_SYLVASTAG, 20u), ZM_GENDER_FEMALE);
	const ZM_BattleMonsterSpec xFather = WithGender(MakeSpecUniform(ZM_SPECIES_NIBBIN, 20u), ZM_GENDER_MALE);
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
	const ZM_BattleMonsterSpec xMother = WithGender(MakeSpec(ZM_SPECIES_SYLVASTAG, aMotherIV, 40u,
		ZM_NATURE_BRUTISH, ZM_ABILITY_STREAMLINE), ZM_GENDER_FEMALE);
	const ZM_BattleMonsterSpec xFather = WithGender(MakeSpec(ZM_SPECIES_NIBBIN, aFatherIV, 40u), ZM_GENDER_MALE);

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
	const ZM_BattleMonsterSpec xMother = WithGender(MakeSpec(ZM_SPECIES_SYLVASTAG, aMotherIV), ZM_GENDER_FEMALE);
	const ZM_BattleMonsterSpec xFather = WithGender(MakeSpec(ZM_SPECIES_NIBBIN, aFatherIV), ZM_GENDER_MALE);

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
	const ZM_BattleMonsterSpec xMother = WithGender(MakeSpec(ZM_SPECIES_SYLVASTAG, aMotherIV), ZM_GENDER_FEMALE);
	const ZM_BattleMonsterSpec xFather = WithGender(MakeSpec(ZM_SPECIES_NIBBIN, aFatherIV), ZM_GENDER_MALE);

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
	const ZM_BattleMonsterSpec xMother = WithGender(MakeSpec(ZM_SPECIES_SYLVASTAG, aMotherIV), ZM_GENDER_FEMALE);
	const ZM_BattleMonsterSpec xFather = WithGender(MakeSpec(ZM_SPECIES_NIBBIN, aFatherIV), ZM_GENDER_MALE);

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
	const ZM_BattleMonsterSpec xMother = WithGender(MakeSpec(ZM_SPECIES_SYLVASTAG, aMotherIV), ZM_GENDER_FEMALE);
	const ZM_BattleMonsterSpec xFather = WithGender(MakeSpec(ZM_SPECIES_NIBBIN, aFatherIV), ZM_GENDER_MALE);

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
	const ZM_BattleMonsterSpec xMother = WithGender(MakeSpec(ZM_SPECIES_SYLVASTAG, aMotherIV), ZM_GENDER_FEMALE);
	const ZM_BattleMonsterSpec xFather = WithGender(MakeSpec(ZM_SPECIES_NIBBIN, aFatherIV), ZM_GENDER_MALE);

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
	const ZM_BattleMonsterSpec xMother = WithGender(MakeSpec(ZM_SPECIES_SYLVASTAG, aMotherIV), ZM_GENDER_FEMALE);
	const ZM_BattleMonsterSpec xFather = WithGender(MakeSpec(ZM_SPECIES_NIBBIN, aFatherIV), ZM_GENDER_MALE);

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
	const ZM_BattleMonsterSpec xMother = WithGender(MakeSpecUniform(ZM_SPECIES_SYLVASTAG, 15u), ZM_GENDER_FEMALE);
	const ZM_BattleMonsterSpec xFather = WithGender(MakeSpecUniform(ZM_SPECIES_NIBBIN, 15u), ZM_GENDER_MALE);

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
	const ZM_BattleMonsterSpec xMother = WithGender(MakeSpec(ZM_SPECIES_SYLVASTAG, aMotherIV), ZM_GENDER_FEMALE);
	const ZM_BattleMonsterSpec xFather = WithGender(MakeSpec(ZM_SPECIES_NIBBIN, aFatherIV), ZM_GENDER_MALE);

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

// Ability is copied from the mother (the FEMALE parent, SC-B); the father's is ignored.
ZENITH_TEST(ZM_Data, Breeding_Egg_InheritsMotherAbility)
{
	const ZM_BattleMonsterSpec xMother = WithGender(MakeSpecUniform(ZM_SPECIES_SYLVASTAG, 20u, 20u,
		ZM_NATURE_FERAL, ZM_ABILITY_STREAMLINE), ZM_GENDER_FEMALE);
	const ZM_BattleMonsterSpec xFather = WithGender(MakeSpecUniform(ZM_SPECIES_NIBBIN, 20u, 20u,
		ZM_NATURE_FERAL, ZM_ABILITY_BEDROCK), ZM_GENDER_MALE);
	ZM_BattleRNG xRng(0x6060ull);
	const ZM_BattleMonsterSpec xEgg = ZM_GenerateEgg(xMother, xFather, xRng);
	ZENITH_ASSERT_EQ((u_int)xEgg.m_eAbility, (u_int)ZM_ABILITY_STREAMLINE, "egg copies mother's ability");
}

// A compatible pair generates a well-formed egg (species = mother base evo,
// level 1). Documents the ZM_GenerateEgg contract on valid input.
ZENITH_TEST(ZM_Data, Breeding_Egg_CompatiblePairGeneratesValidEgg)
{
	const ZM_BattleMonsterSpec xMother = WithGender(MakeSpecUniform(ZM_SPECIES_WARDHUND, 22u), ZM_GENDER_FEMALE);   // QUADRUPED 2-stage final
	const ZM_BattleMonsterSpec xFather = WithGender(MakeSpecUniform(ZM_SPECIES_STRAYLING, 22u), ZM_GENDER_MALE);   // QUADRUPED base
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
	ZM_DaycareDeposit(xState, WithGender(MakeSpecUniform(ZM_SPECIES_FERNFAWN, 31u, 10u), ZM_GENDER_FEMALE));   // slot 0 mother
	ZM_DaycareDeposit(xState, WithGender(MakeSpecUniform(ZM_SPECIES_NIBBIN, 31u, 10u), ZM_GENDER_MALE));         // slot 1 father
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
	// Incompatible pair by SPECIES (QUADRUPED FIELD/PLANT + AVIAN FLYING -> disjoint egg
	// groups). Opposite genders isolate the species mismatch as the sole reason (SC-B).
	ZM_DaycareState xIncompat;
	ZM_DaycareDeposit(xIncompat, WithGender(MakeSpecUniform(ZM_SPECIES_FERNFAWN, 31u, 10u), ZM_GENDER_FEMALE));
	ZM_DaycareDeposit(xIncompat, WithGender(MakeSpecUniform(ZM_SPECIES_PIPWIT, 31u, 10u), ZM_GENDER_MALE));
	ZENITH_ASSERT_FALSE(ZM_DaycarePairCompatible(xIncompat), "premise: incompatible pair (disjoint egg groups)");
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
	ZM_DaycareDeposit(xState, WithGender(MakeSpecUniform(ZM_SPECIES_SYLVASTAG, 20u, 30u), ZM_GENDER_FEMALE));   // mother, base = FERNFAWN
	ZM_DaycareDeposit(xState, WithGender(MakeSpecUniform(ZM_SPECIES_NIBBIN, 25u, 30u), ZM_GENDER_MALE));         // father
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
	ZM_DaycareDeposit(xState, WithGender(MakeSpecUniform(ZM_SPECIES_FERNFAWN, 31u, 10u), ZM_GENDER_FEMALE));
	ZM_DaycareDeposit(xState, WithGender(MakeSpecUniform(ZM_SPECIES_NIBBIN, 31u, 10u), ZM_GENDER_MALE));
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

// ############################################################################
// G. Gender foundation (box-6 SC-A) (ZM_Data)
//
// Covers the derived per-species sex distribution (ZM_GetSpeciesGenderRatio),
// the /8 female-threshold table, the seeded ZM_RollGender (fixed = no draw,
// graded = one RandBelow(8)), and the egg's APPENDED gender draw (IV -> nature
// -> gender). Ratio/threshold values are pinned as literals so a gutted impl
// fails; every derivation premise (rarity / archetype / familyId) is asserted
// via ZM_GetSpeciesData so a data re-tag can't silently invalidate a case.
// ############################################################################

// Legendary and BLOB-archetype species derive GENDERLESS; the body-plan test
// precedes the rarity test, so an UNCOMMON blob is still GENDERLESS.
ZENITH_TEST(ZM_Data, Breeding_Gender_RatioGenderlessForLegendaryAndBlob)
{
	// Legendary -> GENDERLESS.
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(ZM_SPECIES_ZENARIS).m_eRarity, (u_int)ZM_RARITY_LEGENDARY,
		"premise: ZENARIS legendary");
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesGenderRatio(ZM_SPECIES_ZENARIS), (u_int)ZM_GENDER_RATIO_GENDERLESS);

	// COMMON BLOB -> GENDERLESS.
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(ZM_SPECIES_RUBBLET).m_eArchetype, (u_int)ZM_ARCHETYPE_BLOB,
		"premise: RUBBLET blob");
	ZENITH_ASSERT_NE((u_int)ZM_GetSpeciesData(ZM_SPECIES_RUBBLET).m_eRarity, (u_int)ZM_RARITY_LEGENDARY);
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesGenderRatio(ZM_SPECIES_RUBBLET), (u_int)ZM_GENDER_RATIO_GENDERLESS);

	// UNCOMMON BLOB -> still GENDERLESS (archetype dominates rarity).
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(ZM_SPECIES_SLAGLET).m_eArchetype, (u_int)ZM_ARCHETYPE_BLOB);
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(ZM_SPECIES_SLAGLET).m_eRarity, (u_int)ZM_RARITY_UNCOMMON);
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesGenderRatio(ZM_SPECIES_SLAGLET), (u_int)ZM_GENDER_RATIO_GENDERLESS);
}

// RARE (non-blob) lines -- starters + pseudo-legendaries -- follow the 7:1-male
// starter convention.
ZENITH_TEST(ZM_Data, Breeding_Gender_RatioMale71ForRareLines)
{
	const ZM_SPECIES_ID aeRare[] = {
		ZM_SPECIES_FERNFAWN,   // F01 starter
		ZM_SPECIES_KINDLET,    // F02 starter
		ZM_SPECIES_FINLET,     // F03 starter
		ZM_SPECIES_WYRMLING,   // F17 pseudo-legendary
	};
	for (u_int i = 0; i < (u_int)(sizeof(aeRare) / sizeof(aeRare[0])); ++i)
	{
		ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(aeRare[i]).m_eRarity, (u_int)ZM_RARITY_RARE,
			"premise: species %u is RARE", i);
		ZENITH_ASSERT_NE((u_int)ZM_GetSpeciesData(aeRare[i]).m_eArchetype, (u_int)ZM_ARCHETYPE_BLOB,
			"premise: species %u is not a blob", i);
		ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesGenderRatio(aeRare[i]), (u_int)ZM_GENDER_RATIO_MALE_7_1,
			"RARE line %u should be MALE_7_1", i);
	}
}

// Ordinary (COMMON/UNCOMMON, non-blob) species take the deterministic family-seed
// spread over MALE_3_1 / FEMALE_3_1 / EVEN. The exact ratios were computed offline
// from familyId * 2654435761: F04 bucket 0 -> EVEN, F05 bucket 2 -> MALE_3_1,
// F09 bucket 3 -> FEMALE_3_1. familyId is pinned so a re-tag is caught loudly.
ZENITH_TEST(ZM_Data, Breeding_Gender_RatioOrdinaryFamilySeedSpread)
{
	// NIBBIN F05 -> MALE_3_1.
	ZENITH_ASSERT_EQ(ZM_GetSpeciesData(ZM_SPECIES_NIBBIN).m_uFamilyId, 5u, "premise: NIBBIN family 5");
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(ZM_SPECIES_NIBBIN).m_eRarity, (u_int)ZM_RARITY_COMMON);
	ZENITH_ASSERT_NE((u_int)ZM_GetSpeciesData(ZM_SPECIES_NIBBIN).m_eArchetype, (u_int)ZM_ARCHETYPE_BLOB);
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesGenderRatio(ZM_SPECIES_NIBBIN), (u_int)ZM_GENDER_RATIO_MALE_3_1);

	// PIPWIT F04 -> EVEN.
	ZENITH_ASSERT_EQ(ZM_GetSpeciesData(ZM_SPECIES_PIPWIT).m_uFamilyId, 4u, "premise: PIPWIT family 4");
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(ZM_SPECIES_PIPWIT).m_eRarity, (u_int)ZM_RARITY_COMMON);
	ZENITH_ASSERT_NE((u_int)ZM_GetSpeciesData(ZM_SPECIES_PIPWIT).m_eArchetype, (u_int)ZM_ARCHETYPE_BLOB);
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesGenderRatio(ZM_SPECIES_PIPWIT), (u_int)ZM_GENDER_RATIO_EVEN);

	// SPARKIT F09 -> FEMALE_3_1.
	ZENITH_ASSERT_EQ(ZM_GetSpeciesData(ZM_SPECIES_SPARKIT).m_uFamilyId, 9u, "premise: SPARKIT family 9");
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(ZM_SPECIES_SPARKIT).m_eRarity, (u_int)ZM_RARITY_UNCOMMON);
	ZENITH_ASSERT_NE((u_int)ZM_GetSpeciesData(ZM_SPECIES_SPARKIT).m_eArchetype, (u_int)ZM_ARCHETYPE_BLOB);
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesGenderRatio(ZM_SPECIES_SPARKIT), (u_int)ZM_GENDER_RATIO_FEMALE_3_1);
}

// The /8 female-threshold table: graded ratios return 1/2/4/6/7; the three fixed
// ratios return the NO_ROLL sentinel (they resolve with no draw).
ZENITH_TEST(ZM_Data, Breeding_Gender_ThresholdTableOutOf8)
{
	ZENITH_ASSERT_EQ(ZM_GenderRatioFemaleThresholdOutOf8(ZM_GENDER_RATIO_MALE_7_1),   1u);
	ZENITH_ASSERT_EQ(ZM_GenderRatioFemaleThresholdOutOf8(ZM_GENDER_RATIO_MALE_3_1),   2u);
	ZENITH_ASSERT_EQ(ZM_GenderRatioFemaleThresholdOutOf8(ZM_GENDER_RATIO_EVEN),       4u);
	ZENITH_ASSERT_EQ(ZM_GenderRatioFemaleThresholdOutOf8(ZM_GENDER_RATIO_FEMALE_3_1), 6u);
	ZENITH_ASSERT_EQ(ZM_GenderRatioFemaleThresholdOutOf8(ZM_GENDER_RATIO_FEMALE_7_1), 7u);
	// Fixed ratios: no /8 threshold.
	ZENITH_ASSERT_EQ(ZM_GenderRatioFemaleThresholdOutOf8(ZM_GENDER_RATIO_GENDERLESS),  uZM_GENDER_RATIO_NO_ROLL);
	ZENITH_ASSERT_EQ(ZM_GenderRatioFemaleThresholdOutOf8(ZM_GENDER_RATIO_MALE_ONLY),   uZM_GENDER_RATIO_NO_ROLL);
	ZENITH_ASSERT_EQ(ZM_GenderRatioFemaleThresholdOutOf8(ZM_GENDER_RATIO_FEMALE_ONLY), uZM_GENDER_RATIO_NO_ROLL);
}

// A GENDERLESS-ratio species ALWAYS returns GENDERLESS and consumes ZERO draws (an
// untouched RNG at the same seed stays lock-step); a graded species consumes EXACTLY
// one RandBelow(8) draw.
ZENITH_TEST(ZM_Data, Breeding_Gender_RollGenderlessNeverDrawsNorVaries)
{
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesGenderRatio(ZM_SPECIES_ZENARIS), (u_int)ZM_GENDER_RATIO_GENDERLESS,
		"premise: legendary is GENDERLESS-ratio");
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesGenderRatio(ZM_SPECIES_RUBBLET), (u_int)ZM_GENDER_RATIO_GENDERLESS,
		"premise: blob is GENDERLESS-ratio");

	const u_int64 aulSeeds[] = { 0x1ull, 0x2ull, 0xDEADull, 0xC0FFEEull, 0x9E3779B1ull };
	for (u_int s = 0; s < (u_int)(sizeof(aulSeeds) / sizeof(aulSeeds[0])); ++s)
	{
		// Always GENDERLESS (legendary AND blob), any seed.
		ZM_BattleRNG xAny(aulSeeds[s]);
		ZENITH_ASSERT_EQ((u_int)ZM_RollGender(ZM_SPECIES_ZENARIS, xAny), (u_int)ZM_GENDER_GENDERLESS,
			"legendary always genderless (seed %u)", s);
		ZENITH_ASSERT_EQ((u_int)ZM_RollGender(ZM_SPECIES_RUBBLET, xAny), (u_int)ZM_GENDER_GENDERLESS,
			"blob always genderless (seed %u)", s);

		// Zero draws: the rolled RNG stays in lock-step with an untouched one.
		ZM_BattleRNG xRolled(aulSeeds[s]);
		ZM_BattleRNG xRef(aulSeeds[s]);
		ZM_RollGender(ZM_SPECIES_ZENARIS, xRolled);   // must not advance the stream
		for (u_int k = 0; k < 4u; ++k)
		{
			ZENITH_ASSERT_EQ(xRolled.RandBelow(64u), xRef.RandBelow(64u),
				"genderless roll perturbed the stream (seed %u, k %u)", s, k);
		}
	}

	// Contrast: a graded roll consumes EXACTLY one RandBelow(8) draw.
	ZM_BattleRNG xGraded(0x5150ull);
	ZM_BattleRNG xManual(0x5150ull);
	ZM_RollGender(ZM_SPECIES_FERNFAWN, xGraded);   // MALE_7_1 -> one draw
	xManual.RandBelow(8u);                          // manually consume one
	for (u_int k = 0; k < 4u; ++k)
	{
		ZENITH_ASSERT_EQ(xGraded.RandBelow(32u), xManual.RandBelow(32u),
			"graded roll must consume exactly one draw (k %u)", k);
	}
}

// Same seed -> same gender, across several graded species (which never yield GENDERLESS).
ZENITH_TEST(ZM_Data, Breeding_Gender_RollDeterministicSameSeed)
{
	const ZM_SPECIES_ID aeSpecies[] = {
		ZM_SPECIES_FERNFAWN,   // MALE_7_1
		ZM_SPECIES_PIPWIT,     // EVEN
		ZM_SPECIES_SPARKIT,    // FEMALE_3_1
		ZM_SPECIES_NIBBIN,     // MALE_3_1
	};
	const u_int64 aulSeeds[] = { 0x11ull, 0x2222ull, 0x333333ull, 0xABCDEF01ull };
	for (u_int sp = 0; sp < (u_int)(sizeof(aeSpecies) / sizeof(aeSpecies[0])); ++sp)
	{
		for (u_int s = 0; s < (u_int)(sizeof(aulSeeds) / sizeof(aulSeeds[0])); ++s)
		{
			ZM_BattleRNG xA(aulSeeds[s]);
			ZM_BattleRNG xB(aulSeeds[s]);
			const ZM_GENDER eA = ZM_RollGender(aeSpecies[sp], xA);
			const ZM_GENDER eB = ZM_RollGender(aeSpecies[sp], xB);
			ZENITH_ASSERT_EQ((u_int)eA, (u_int)eB, "same seed same gender (sp %u seed %u)", sp, s);
			ZENITH_ASSERT_NE((u_int)eA, (u_int)ZM_GENDER_GENDERLESS,
				"graded species never genderless (sp %u seed %u)", sp, s);
		}
	}
}

// MALE_7_1 (female threshold 1) rolls FEMALE in ~1/8 of a large single-stream sample;
// assert the count within a wide +-40% band [600,1400] of N/8 == 1000 across N == 8000.
ZENITH_TEST(ZM_Data, Breeding_Gender_RollMale71DistributionAboutOneEighth)
{
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesGenderRatio(ZM_SPECIES_FERNFAWN), (u_int)ZM_GENDER_RATIO_MALE_7_1,
		"premise: FERNFAWN is MALE_7_1");

	const u_int uN = 8000u;
	ZM_BattleRNG xRng(0x7E571357ull);   // one long stream -> uniform RandBelow(8)
	u_int uFemale = 0u;
	u_int uMale   = 0u;
	for (u_int i = 0; i < uN; ++i)
	{
		const ZM_GENDER e = ZM_RollGender(ZM_SPECIES_FERNFAWN, xRng);
		if (e == ZM_GENDER_FEMALE)    { ++uFemale; }
		else if (e == ZM_GENDER_MALE) { ++uMale; }
	}
	ZENITH_ASSERT_EQ(uFemale + uMale, uN, "every graded roll is male or female");
	ZENITH_ASSERT_GE(uFemale, 600u,  "MALE_7_1 female count too low (got %u)", uFemale);
	ZENITH_ASSERT_LE(uFemale, 1400u, "MALE_7_1 female count too high (got %u)", uFemale);
	ZENITH_ASSERT_GT(uMale, uFemale, "MALE_7_1 must be male-majority");
}

// EVEN (female threshold 4) rolls FEMALE ~half the time; assert within [3200,4800] of
// N/2 == 4000 across N == 8000.
ZENITH_TEST(ZM_Data, Breeding_Gender_RollEvenDistributionAboutHalf)
{
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesGenderRatio(ZM_SPECIES_PIPWIT), (u_int)ZM_GENDER_RATIO_EVEN,
		"premise: PIPWIT is EVEN");

	const u_int uN = 8000u;
	ZM_BattleRNG xRng(0xE5E5E5E5ull);
	u_int uFemale = 0u;
	u_int uMale   = 0u;
	for (u_int i = 0; i < uN; ++i)
	{
		const ZM_GENDER e = ZM_RollGender(ZM_SPECIES_PIPWIT, xRng);
		if (e == ZM_GENDER_FEMALE)    { ++uFemale; }
		else if (e == ZM_GENDER_MALE) { ++uMale; }
	}
	ZENITH_ASSERT_EQ(uFemale + uMale, uN, "every EVEN roll is male or female");
	ZENITH_ASSERT_GE(uFemale, 3200u, "EVEN female count too low (got %u)", uFemale);
	ZENITH_ASSERT_LE(uFemale, 4800u, "EVEN female count too high (got %u)", uFemale);
}

// The egg's gender equals ZM_RollGender(offspring) taken at the SAME RNG position
// (AFTER the IV + nature draws). The expected is derived by replaying the local oracle
// (IV -> nature, advancing the RNG) then rolling gender -- never by calling ZM_GenerateEgg.
ZENITH_TEST(ZM_Data, Breeding_Gender_EggGenderMatchesRollGenderAtPosition)
{
	const u_int aMotherIV[ZM_STAT_COUNT] = { 2u, 8u, 14u, 20u, 26u, 30u };
	const u_int aFatherIV[ZM_STAT_COUNT] = { 30u, 26u, 20u, 14u, 8u, 2u };
	const ZM_BattleMonsterSpec xMother = WithGender(MakeSpec(ZM_SPECIES_SYLVASTAG, aMotherIV), ZM_GENDER_FEMALE);   // base = FERNFAWN
	const ZM_BattleMonsterSpec xFather = WithGender(MakeSpec(ZM_SPECIES_NIBBIN, aFatherIV), ZM_GENDER_MALE);
	const ZM_SPECIES_ID eOffspring = ZM_GetBaseEvolution(ZM_SPECIES_SYLVASTAG);
	ZENITH_ASSERT_EQ((u_int)eOffspring, (u_int)ZM_SPECIES_FERNFAWN, "premise: offspring is FERNFAWN (MALE_7_1)");

	u_int uFemale = 0u;
	u_int uMale   = 0u;
	const u_int uN = 256u;
	for (u_int i = 0; i < uN; ++i)
	{
		const u_int64 ulSeed = (u_int64)(i + 1u) * 0x9E3779B97F4A7C15ull;

		ZM_BattleRNG xRng(ulSeed);
		const ZM_BattleMonsterSpec xEgg = ZM_GenerateEgg(xMother, xFather, xRng);

		// Oracle: replay IV + nature (advances the RNG), then roll gender at that position.
		ZM_BattleRNG xRngO(ulSeed);
		OracleGenerate(aMotherIV, aFatherIV, false, ZM_NATURE_COUNT, xRngO);
		const ZM_GENDER eExpected = ZM_RollGender(eOffspring, xRngO);

		ZENITH_ASSERT_EQ((u_int)xEgg.m_eGender, (u_int)eExpected, "egg gender vs positioned roll (i %u)", i);
		ZENITH_ASSERT_NE((u_int)xEgg.m_eGender, (u_int)ZM_GENDER_GENDERLESS,
			"graded offspring egg is gendered (i %u)", i);
		if (xEgg.m_eGender == ZM_GENDER_FEMALE) { ++uFemale; } else { ++uMale; }
	}
	// Over 256 seeds both sexes must appear (~1/8 female), proving the draw is live.
	ZENITH_ASSERT_GT(uFemale, 0u, "expected at least one female across seeds");
	ZENITH_ASSERT_GT(uMale,   0u, "expected at least one male across seeds");
}

// A genderless-offspring egg (BLOB base evo) skips the gender draw and is always
// GENDERLESS, regardless of seed. SC-B: two genderless non-universal blobs are no longer
// a legal pair, so the fixture is GLOOPET (universal breeder) x a BLOB -- the non-universal
// blob (RUBBLET) seeds the line, giving a genderless offspring.
ZENITH_TEST(ZM_Data, Breeding_Gender_EggGenderlessOffspringAlwaysGenderless)
{
	const ZM_BattleMonsterSpec xMother = MakeSpecUniform(ZM_SPECIES_GLOOPET, 20u);   // universal breeder (genderless)
	const ZM_BattleMonsterSpec xFather = MakeSpecUniform(ZM_SPECIES_RUBBLET, 20u);   // BLOB, base = RUBBLET
	ZENITH_ASSERT_TRUE(ZM_AreCompatible(xMother, xFather), "premise: GLOOPET x blob is compatible");
	const ZM_SPECIES_ID eOffspring = ZM_GetBaseEvolution(ZM_SPECIES_RUBBLET);
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesGenderRatio(eOffspring), (u_int)ZM_GENDER_RATIO_GENDERLESS,
		"premise: offspring ratio is GENDERLESS");

	const u_int64 aulSeeds[] = { 0x1ull, 0x99ull, 0xBEEFull, 0x12345678ull, 0xFEEDFACEull };
	for (u_int s = 0; s < (u_int)(sizeof(aulSeeds) / sizeof(aulSeeds[0])); ++s)
	{
		ZM_BattleRNG xRng(aulSeeds[s]);
		const ZM_BattleMonsterSpec xEgg = ZM_GenerateEgg(xMother, xFather, xRng);
		ZENITH_ASSERT_EQ((u_int)xEgg.m_eSpecies, (u_int)ZM_SPECIES_RUBBLET, "offspring species (seed %u)", s);
		ZENITH_ASSERT_EQ((u_int)xEgg.m_eGender, (u_int)ZM_GENDER_GENDERLESS, "genderless egg (seed %u)", s);
	}
}

// REGRESSION GUARD: appending the gender draw does NOT perturb the IV/nature goldens.
// Same seed + IVs as Breeding_Egg_Golden_Seed1; IV + nature still match the pre-gender
// oracle, and gender is the roll taken at the post-nature position.
ZENITH_TEST(ZM_Data, Breeding_Gender_AppendedDrawDoesNotPerturbIVNature)
{
	const u_int aMotherIV[ZM_STAT_COUNT] = { 0u, 6u, 12u, 18u, 24u, 31u };
	const u_int aFatherIV[ZM_STAT_COUNT] = { 31u, 25u, 19u, 13u, 7u, 1u };
	const ZM_BattleMonsterSpec xMother = WithGender(MakeSpec(ZM_SPECIES_SYLVASTAG, aMotherIV), ZM_GENDER_FEMALE);
	const ZM_BattleMonsterSpec xFather = WithGender(MakeSpec(ZM_SPECIES_NIBBIN, aFatherIV), ZM_GENDER_MALE);

	ZM_BattleRNG xRng(0xA1ull);   // matches the seed 1 golden
	const ZM_BattleMonsterSpec xEgg = ZM_GenerateEgg(xMother, xFather, xRng);

	ZM_BattleRNG xRngO(0xA1ull);
	const OracleEgg xOracle = OracleGenerate(aMotherIV, aFatherIV, false, ZM_NATURE_COUNT, xRngO);
	// IV + nature byte-identical to the pre-gender oracle: the appended draw perturbed nothing.
	AssertEggMatchesOracle(xEgg, xOracle, "regression");
	// Gender = ZM_RollGender at the SAME post-nature RNG position.
	const ZM_GENDER eGender = ZM_RollGender(ZM_SPECIES_FERNFAWN, xRngO);
	ZENITH_ASSERT_EQ((u_int)xEgg.m_eGender, (u_int)eGender, "egg gender = positioned roll");
	ZENITH_ASSERT_NE((u_int)xEgg.m_eGender, (u_int)ZM_GENDER_GENDERLESS, "FERNFAWN egg is gendered");
}

// A spec that never sets gender defaults GENDERLESS (keeps every pre-SC-A golden
// byte-safe), and ZM_BuildBattleMonster copies gender verbatim onto the battle monster.
ZENITH_TEST(ZM_Data, Breeding_Gender_SpecDefaultsGenderlessAndBuildCopies)
{
	const ZM_BattleMonsterSpec xDefault;
	ZENITH_ASSERT_EQ((u_int)xDefault.m_eGender, (u_int)ZM_GENDER_GENDERLESS, "spec default gender GENDERLESS");

	// An unset-gender spec builds a GENDERLESS monster.
	const ZM_BattleMonster xBuiltDefault = ZM_BuildBattleMonster(MakeSpecUniform(ZM_SPECIES_NIBBIN, 31u, 50u));
	ZENITH_ASSERT_EQ((u_int)xBuiltDefault.m_eGender, (u_int)ZM_GENDER_GENDERLESS, "unset spec builds GENDERLESS mon");

	// Build copies an explicit gender verbatim.
	ZM_BattleMonsterSpec xSpec = MakeSpecUniform(ZM_SPECIES_NIBBIN, 31u, 50u);
	xSpec.m_eGender = ZM_GENDER_FEMALE;
	ZENITH_ASSERT_EQ((u_int)ZM_BuildBattleMonster(xSpec).m_eGender, (u_int)ZM_GENDER_FEMALE, "build copies FEMALE");
	xSpec.m_eGender = ZM_GENDER_MALE;
	ZENITH_ASSERT_EQ((u_int)ZM_BuildBattleMonster(xSpec).m_eGender, (u_int)ZM_GENDER_MALE, "build copies MALE");
}

// End-to-end: a gendered egg (offspring FERNFAWN) built into a battle monster carries
// the egg's rolled gender.
ZENITH_TEST(ZM_Data, Breeding_Gender_EggBuiltMonsterCarriesEggGender)
{
	const ZM_BattleMonsterSpec xMother = WithGender(MakeSpecUniform(ZM_SPECIES_SYLVASTAG, 20u), ZM_GENDER_FEMALE);
	const ZM_BattleMonsterSpec xFather = WithGender(MakeSpecUniform(ZM_SPECIES_NIBBIN, 20u), ZM_GENDER_MALE);
	const u_int64 aulSeeds[] = { 0x1ull, 0x2ull, 0x2222ull, 0x9E3779B1ull, 0xF00Dull };
	for (u_int s = 0; s < (u_int)(sizeof(aulSeeds) / sizeof(aulSeeds[0])); ++s)
	{
		ZM_BattleRNG xRng(aulSeeds[s]);
		const ZM_BattleMonsterSpec xEgg = ZM_GenerateEgg(xMother, xFather, xRng);
		ZENITH_ASSERT_EQ((u_int)xEgg.m_eSpecies, (u_int)ZM_SPECIES_FERNFAWN, "offspring FERNFAWN (seed %u)", s);
		ZENITH_ASSERT_NE((u_int)xEgg.m_eGender, (u_int)ZM_GENDER_GENDERLESS, "gendered egg (seed %u)", s);

		const ZM_BattleMonster xBuilt = ZM_BuildBattleMonster(xEgg);
		ZENITH_ASSERT_EQ((u_int)xBuilt.m_eGender, (u_int)xEgg.m_eGender, "built mon carries egg gender (seed %u)", s);
	}
}

// ############################################################################
// H. Egg groups + gendered / universal compatibility + offspring roles
//    (box-6 SC-B) (ZM_Data)
//
// Real egg-group taxonomy (ZM_GetSpeciesEggGroups: archetype primary + one type-
// derived secondary), the GLOOPET Ditto-analog (ZM_IsUniversalBreeder), the
// gender-aware breeding gate (ZM_AreCompatible: opposite binary genders OR the
// universal breeder), and parent-role-driven offspring (mother = FEMALE / non-
// universal parent). Values are pinned as literals so a gutted impl fails; every
// derivation premise (archetype / type / rarity / family) is asserted via
// ZM_GetSpeciesData so a data re-tag is caught loudly. Deterministic + hermetic.
// ############################################################################

// --- H.1 Egg-group derivation --------------------------------------------------

// A NORMAL-type quadruped derives a SINGLE FIELD group (its type contributes no
// distinct secondary).
ZENITH_TEST(ZM_Data, Breeding_EggGroup_SingleGroupField)
{
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(ZM_SPECIES_NIBBIN).m_eArchetype, (u_int)ZM_ARCHETYPE_QUADRUPED,
		"premise: NIBBIN is QUADRUPED");
	const ZM_EggGroups xGroups = ZM_GetSpeciesEggGroups(ZM_SPECIES_NIBBIN);
	ZENITH_ASSERT_EQ(xGroups.m_uCount, 1u, "NIBBIN single group");
	ZENITH_ASSERT_EQ((u_int)xGroups.m_aeGroups[0], (u_int)ZM_EGG_GROUP_FIELD, "NIBBIN primary FIELD");
}

// A GRASS quadruped derives TWO groups: archetype FIELD + type-secondary PLANT.
ZENITH_TEST(ZM_Data, Breeding_EggGroup_TwoGroupArchetypePlusTypeSecondary)
{
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(ZM_SPECIES_FERNFAWN).m_eArchetype, (u_int)ZM_ARCHETYPE_QUADRUPED,
		"premise: FERNFAWN is QUADRUPED");
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(ZM_SPECIES_FERNFAWN).m_aeTypes[0], (u_int)ZM_TYPE_GRASS,
		"premise: FERNFAWN primary type GRASS");
	const ZM_EggGroups xGroups = ZM_GetSpeciesEggGroups(ZM_SPECIES_FERNFAWN);
	ZENITH_ASSERT_EQ(xGroups.m_uCount, 2u, "FERNFAWN two groups");
	ZENITH_ASSERT_EQ((u_int)xGroups.m_aeGroups[0], (u_int)ZM_EGG_GROUP_FIELD,  "primary FIELD");
	ZENITH_ASSERT_EQ((u_int)xGroups.m_aeGroups[1], (u_int)ZM_EGG_GROUP_PLANT,  "secondary PLANT");
}

// A STONE blob derives TWO groups: archetype AMORPHOUS + type-secondary MINERAL.
ZENITH_TEST(ZM_Data, Breeding_EggGroup_TwoGroupBlobMineral)
{
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(ZM_SPECIES_RUBBLET).m_eArchetype, (u_int)ZM_ARCHETYPE_BLOB,
		"premise: RUBBLET is BLOB");
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(ZM_SPECIES_RUBBLET).m_aeTypes[0], (u_int)ZM_TYPE_STONE,
		"premise: RUBBLET primary type STONE");
	const ZM_EggGroups xGroups = ZM_GetSpeciesEggGroups(ZM_SPECIES_RUBBLET);
	ZENITH_ASSERT_EQ(xGroups.m_uCount, 2u, "RUBBLET two groups");
	ZENITH_ASSERT_EQ((u_int)xGroups.m_aeGroups[0], (u_int)ZM_EGG_GROUP_AMORPHOUS, "primary AMORPHOUS");
	ZENITH_ASSERT_EQ((u_int)xGroups.m_aeGroups[1], (u_int)ZM_EGG_GROUP_MINERAL,   "secondary MINERAL");
}

// A type whose secondary maps ONTO the archetype primary adds no distinct group:
// PIPWIT is AVIAN (primary FLYING) + SKY (also FLYING) -> single FLYING group.
ZENITH_TEST(ZM_Data, Breeding_EggGroup_TypeSecondaryMapsToPrimarySingle)
{
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(ZM_SPECIES_PIPWIT).m_eArchetype, (u_int)ZM_ARCHETYPE_AVIAN,
		"premise: PIPWIT is AVIAN");
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(ZM_SPECIES_PIPWIT).m_aeTypes[1], (u_int)ZM_TYPE_SKY,
		"premise: PIPWIT carries SKY");
	const ZM_EggGroups xGroups = ZM_GetSpeciesEggGroups(ZM_SPECIES_PIPWIT);
	ZENITH_ASSERT_EQ(xGroups.m_uCount, 1u, "SKY collapses onto FLYING -> single group");
	ZENITH_ASSERT_EQ((u_int)xGroups.m_aeGroups[0], (u_int)ZM_EGG_GROUP_FLYING, "PIPWIT FLYING");
}

// The break-after-first-matching-type-slot branch: when slot-0's type maps ONTO the
// archetype primary, the scan STOPS and slot-1 is never consulted -- so a would-be second
// group is blocked. PUFFSEED is FLOATER_PLANTOID (primary PLANT) with types {GRASS, SKY}:
// GRASS (slot 0) maps to PLANT == primary, so SKY (slot 1, which maps to the DIFFERENT
// group FLYING) is never read -> a SINGLE {PLANT} group. Distinct from the PIPWIT case
// above (there slot 0 doesn't match at all): guards against a break->continue regression
// that would wrongly append FLYING and make the count 2.
ZENITH_TEST(ZM_Data, Breeding_EggGroup_PrimaryTypeSlotBlocksSecondary)
{
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(ZM_SPECIES_PUFFSEED).m_eArchetype, (u_int)ZM_ARCHETYPE_FLOATER_PLANTOID,
		"premise: PUFFSEED is FLOATER_PLANTOID (primary PLANT)");
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(ZM_SPECIES_PUFFSEED).m_aeTypes[0], (u_int)ZM_TYPE_GRASS,
		"premise: PUFFSEED slot-0 type GRASS (maps to PLANT == primary)");
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(ZM_SPECIES_PUFFSEED).m_aeTypes[1], (u_int)ZM_TYPE_SKY,
		"premise: PUFFSEED slot-1 type SKY (would map to FLYING if consulted)");
	const ZM_EggGroups xGroups = ZM_GetSpeciesEggGroups(ZM_SPECIES_PUFFSEED);
	ZENITH_ASSERT_EQ(xGroups.m_uCount, 1u, "slot-0 primary match blocks slot-1 -> single group");
	ZENITH_ASSERT_EQ((u_int)xGroups.m_aeGroups[0], (u_int)ZM_EGG_GROUP_PLANT, "PUFFSEED single PLANT group");
}

// The universal breeder still reads a concrete egg group (single AMORPHOUS); its
// universality lives in ZM_IsUniversalBreeder, NOT in a UNIVERSAL group value.
ZENITH_TEST(ZM_Data, Breeding_EggGroup_UniversalBreederAmorphousSingle)
{
	const ZM_EggGroups xGroups = ZM_GetSpeciesEggGroups(ZM_SPECIES_GLOOPET);
	ZENITH_ASSERT_EQ(xGroups.m_uCount, 1u, "GLOOPET single group");
	ZENITH_ASSERT_EQ((u_int)xGroups.m_aeGroups[0], (u_int)ZM_EGG_GROUP_AMORPHOUS, "GLOOPET AMORPHOUS");
	ZENITH_ASSERT_NE((u_int)xGroups.m_aeGroups[0], (u_int)ZM_EGG_GROUP_UNIVERSAL,
		"UNIVERSAL is never returned by ZM_GetSpeciesEggGroups");
}

// Every legendary derives the single sentinel NO_EGGS group (they never breed).
ZENITH_TEST(ZM_Data, Breeding_EggGroup_LegendaryNoEggs)
{
	const ZM_SPECIES_ID aeLegendary[] = { ZM_SPECIES_ZENARIS, ZM_SPECIES_NADIRATH, ZM_SPECIES_EQUINARA };
	for (u_int i = 0; i < (u_int)(sizeof(aeLegendary) / sizeof(aeLegendary[0])); ++i)
	{
		ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(aeLegendary[i]).m_eRarity, (u_int)ZM_RARITY_LEGENDARY,
			"premise: legendary %u", i);
		const ZM_EggGroups xGroups = ZM_GetSpeciesEggGroups(aeLegendary[i]);
		ZENITH_ASSERT_EQ(xGroups.m_uCount, 1u, "legendary single group (%u)", i);
		ZENITH_ASSERT_EQ((u_int)xGroups.m_aeGroups[0], (u_int)ZM_EGG_GROUP_NO_EGGS, "legendary NO_EGGS (%u)", i);
	}
}

// --- H.2 Universal breeder + species-level compatibility -----------------------

// Exactly one species is the universal breeder: GLOOPET. Its evolution and other
// species (blobs, legendaries) are not.
ZENITH_TEST(ZM_Data, Breeding_Universal_OnlyGloopetIsUniversal)
{
	ZENITH_ASSERT_TRUE(ZM_IsUniversalBreeder(ZM_SPECIES_GLOOPET), "GLOOPET is the universal breeder");
	ZENITH_ASSERT_EQ(ZM_GetSpeciesData(ZM_SPECIES_GLUTTONUB).m_uFamilyId,
		ZM_GetSpeciesData(ZM_SPECIES_GLOOPET).m_uFamilyId, "premise: GLUTTONUB is GLOOPET's line");
	ZENITH_ASSERT_FALSE(ZM_IsUniversalBreeder(ZM_SPECIES_GLUTTONUB), "GLOOPET's evolution is not universal");
	ZENITH_ASSERT_FALSE(ZM_IsUniversalBreeder(ZM_SPECIES_NIBBIN),    "an ordinary species is not universal");
	ZENITH_ASSERT_FALSE(ZM_IsUniversalBreeder(ZM_SPECIES_RUBBLET),   "another blob is not universal");
	ZENITH_ASSERT_FALSE(ZM_IsUniversalBreeder(ZM_SPECIES_ZENARIS),   "a legendary is not universal");
}

// Two distinct families that share an egg group are species-compatible.
ZENITH_TEST(ZM_Data, Breeding_SpeciesCompat_SameGroupDifferentFamilyTrue)
{
	ZENITH_ASSERT_NE(ZM_GetSpeciesData(ZM_SPECIES_NIBBIN).m_uFamilyId,
		ZM_GetSpeciesData(ZM_SPECIES_STRAYLING).m_uFamilyId, "premise: different families");
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesEggGroups(ZM_SPECIES_NIBBIN).m_aeGroups[0],   (u_int)ZM_EGG_GROUP_FIELD,
		"premise: NIBBIN FIELD");
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesEggGroups(ZM_SPECIES_STRAYLING).m_aeGroups[0], (u_int)ZM_EGG_GROUP_FIELD,
		"premise: STRAYLING FIELD");
	ZENITH_ASSERT_TRUE(ZM_AreSpeciesCompatible(ZM_SPECIES_NIBBIN, ZM_SPECIES_STRAYLING),
		"shared FIELD across families -> compatible");
}

// Species whose egg-group lists are disjoint are incompatible.
ZENITH_TEST(ZM_Data, Breeding_SpeciesCompat_DisjointGroupsFalse)
{
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesEggGroups(ZM_SPECIES_NIBBIN).m_aeGroups[0], (u_int)ZM_EGG_GROUP_FIELD,
		"premise: NIBBIN FIELD");
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesEggGroups(ZM_SPECIES_PIPWIT).m_aeGroups[0], (u_int)ZM_EGG_GROUP_FLYING,
		"premise: PIPWIT FLYING");
	ZENITH_ASSERT_FALSE(ZM_AreSpeciesCompatible(ZM_SPECIES_NIBBIN, ZM_SPECIES_PIPWIT),
		"disjoint egg groups -> incompatible");
}

// A shared type-derived SECONDARY bridges DIFFERENT archetypes: a FIELD/PLANT
// quadruped and a PLANT plantoid breed (impossible under the old archetype proxy).
ZENITH_TEST(ZM_Data, Breeding_SpeciesCompat_CrossArchetypeSharedSecondaryTrue)
{
	ZENITH_ASSERT_NE((u_int)ZM_GetSpeciesData(ZM_SPECIES_FERNFAWN).m_eArchetype,
		(u_int)ZM_GetSpeciesData(ZM_SPECIES_PUFFSEED).m_eArchetype, "premise: different archetypes");
	const ZM_EggGroups xF = ZM_GetSpeciesEggGroups(ZM_SPECIES_FERNFAWN);
	const ZM_EggGroups xP = ZM_GetSpeciesEggGroups(ZM_SPECIES_PUFFSEED);
	ZENITH_ASSERT_EQ(xF.m_uCount, 2u, "premise: FERNFAWN two groups");
	ZENITH_ASSERT_EQ((u_int)xF.m_aeGroups[1], (u_int)ZM_EGG_GROUP_PLANT, "premise: FERNFAWN secondary PLANT");
	ZENITH_ASSERT_EQ((u_int)xP.m_aeGroups[0], (u_int)ZM_EGG_GROUP_PLANT, "premise: PUFFSEED primary PLANT");
	ZENITH_ASSERT_TRUE(ZM_AreSpeciesCompatible(ZM_SPECIES_FERNFAWN, ZM_SPECIES_PUFFSEED),
		"cross-archetype shared PLANT -> compatible");
}

// The universal breeder's species-level paths: one-universal true, two-universal
// false, universal + legendary false.
ZENITH_TEST(ZM_Data, Breeding_SpeciesCompat_UniversalAndLegendaryPaths)
{
	ZENITH_ASSERT_TRUE(ZM_AreSpeciesCompatible(ZM_SPECIES_GLOOPET, ZM_SPECIES_NIBBIN),
		"one universal -> compatible");
	ZENITH_ASSERT_TRUE(ZM_AreSpeciesCompatible(ZM_SPECIES_PIPWIT, ZM_SPECIES_GLOOPET),
		"one universal -> compatible (even across disjoint groups, symmetric)");
	ZENITH_ASSERT_FALSE(ZM_AreSpeciesCompatible(ZM_SPECIES_GLOOPET, ZM_SPECIES_GLOOPET),
		"two universal breeders -> incompatible");
	ZENITH_ASSERT_FALSE(ZM_AreSpeciesCompatible(ZM_SPECIES_GLOOPET, ZM_SPECIES_ZENARIS),
		"universal + legendary -> incompatible");
}

// --- H.3 Gender-aware compatibility (ZM_AreCompatible) --------------------------

// One MALE + one FEMALE that share an egg group is a valid pair; symmetric.
ZENITH_TEST(ZM_Data, Breeding_GenderedCompat_OppositeGenderSameGroupTrue)
{
	const ZM_BattleMonsterSpec xMale   = WithGender(MakeSpecUniform(ZM_SPECIES_NIBBIN, 31u), ZM_GENDER_MALE);
	const ZM_BattleMonsterSpec xFemale = WithGender(MakeSpecUniform(ZM_SPECIES_STRAYLING, 31u), ZM_GENDER_FEMALE);
	ZENITH_ASSERT_TRUE(ZM_AreCompatible(xMale, xFemale), "M + F sharing FIELD -> compatible");
	ZENITH_ASSERT_TRUE(ZM_AreCompatible(xFemale, xMale), "compatibility is symmetric");
}

// Same-gender parents that share an egg group are NOT a valid pair.
ZENITH_TEST(ZM_Data, Breeding_GenderedCompat_SameGenderSameGroupFalse)
{
	ZENITH_ASSERT_TRUE(ZM_AreSpeciesCompatible(ZM_SPECIES_NIBBIN, ZM_SPECIES_STRAYLING),
		"premise: species share FIELD");
	const ZM_BattleMonsterSpec xMaleA = WithGender(MakeSpecUniform(ZM_SPECIES_NIBBIN, 31u), ZM_GENDER_MALE);
	const ZM_BattleMonsterSpec xMaleB = WithGender(MakeSpecUniform(ZM_SPECIES_STRAYLING, 31u), ZM_GENDER_MALE);
	ZENITH_ASSERT_FALSE(ZM_AreCompatible(xMaleA, xMaleB), "two males -> incompatible");
	const ZM_BattleMonsterSpec xFemaleA = WithGender(MakeSpecUniform(ZM_SPECIES_NIBBIN, 31u), ZM_GENDER_FEMALE);
	const ZM_BattleMonsterSpec xFemaleB = WithGender(MakeSpecUniform(ZM_SPECIES_STRAYLING, 31u), ZM_GENDER_FEMALE);
	ZENITH_ASSERT_FALSE(ZM_AreCompatible(xFemaleA, xFemaleB), "two females -> incompatible");
}

// A GENDERLESS non-universal pair never breeds -- even when species-compatible.
ZENITH_TEST(ZM_Data, Breeding_GenderedCompat_GenderlessNonUniversalFalse)
{
	ZENITH_ASSERT_TRUE(ZM_AreSpeciesCompatible(ZM_SPECIES_RUBBLET, ZM_SPECIES_SLAGLET),
		"premise: blobs share AMORPHOUS at the species level");
	const ZM_BattleMonsterSpec xA = WithGender(MakeSpecUniform(ZM_SPECIES_RUBBLET, 31u), ZM_GENDER_GENDERLESS);
	const ZM_BattleMonsterSpec xB = WithGender(MakeSpecUniform(ZM_SPECIES_SLAGLET, 31u), ZM_GENDER_GENDERLESS);
	ZENITH_ASSERT_FALSE(ZM_AreCompatible(xA, xB), "genderless non-universal pair -> incompatible");
}

// The universal breeder ignores gender: compatible with a non-legendary partner of
// ANY gender (both slot orders), but never with a legendary.
ZENITH_TEST(ZM_Data, Breeding_GenderedCompat_UniversalIgnoresGender)
{
	const ZM_BattleMonsterSpec xGloopet = MakeSpecUniform(ZM_SPECIES_GLOOPET, 31u);   // genderless
	const ZM_GENDER aeGenders[] = { ZM_GENDER_MALE, ZM_GENDER_FEMALE, ZM_GENDER_GENDERLESS };
	for (u_int g = 0; g < (u_int)(sizeof(aeGenders) / sizeof(aeGenders[0])); ++g)
	{
		const ZM_BattleMonsterSpec xPartner = WithGender(MakeSpecUniform(ZM_SPECIES_NIBBIN, 31u), aeGenders[g]);
		ZENITH_ASSERT_TRUE(ZM_AreCompatible(xGloopet, xPartner), "GLOOPET + any-gender partner (%u)", g);
		ZENITH_ASSERT_TRUE(ZM_AreCompatible(xPartner, xGloopet), "symmetric (%u)", g);
	}
	const ZM_BattleMonsterSpec xLegend = MakeSpecUniform(ZM_SPECIES_ZENARIS, 31u);
	ZENITH_ASSERT_FALSE(ZM_AreCompatible(xGloopet, xLegend), "GLOOPET + legendary -> incompatible");
}

// --- H.4 Parent-role-driven offspring ------------------------------------------

// Offspring species = the FEMALE parent's base evolution; swapping which parent is
// female flips the offspring. Expectations come from ZM_GetBaseEvolution, not from
// re-running ZM_GenerateEgg.
ZENITH_TEST(ZM_Data, Breeding_Offspring_SpeciesIsFemaleBaseEvo)
{
	ZENITH_ASSERT_EQ((u_int)ZM_GetBaseEvolution(ZM_SPECIES_SYLVASTAG), (u_int)ZM_SPECIES_FERNFAWN, "premise SYLVASTAG->FERNFAWN");
	ZENITH_ASSERT_EQ((u_int)ZM_GetBaseEvolution(ZM_SPECIES_GRAINMAW),  (u_int)ZM_SPECIES_NIBBIN,   "premise GRAINMAW->NIBBIN");

	// SYLVASTAG female -> FERNFAWN.
	{
		const ZM_BattleMonsterSpec xSyl = WithGender(MakeSpecUniform(ZM_SPECIES_SYLVASTAG, 20u), ZM_GENDER_FEMALE);
		const ZM_BattleMonsterSpec xGrn = WithGender(MakeSpecUniform(ZM_SPECIES_GRAINMAW, 20u), ZM_GENDER_MALE);
		ZENITH_ASSERT_TRUE(ZM_AreCompatible(xSyl, xGrn), "pair compatible");
		ZM_BattleRNG xRng(0x5B10ull);
		const ZM_BattleMonsterSpec xEgg = ZM_GenerateEgg(xSyl, xGrn, xRng);
		ZENITH_ASSERT_EQ((u_int)xEgg.m_eSpecies, (u_int)ZM_SPECIES_FERNFAWN, "female SYLVASTAG -> FERNFAWN");
	}
	// Swap the genders -> GRAINMAW is now the mother -> NIBBIN.
	{
		const ZM_BattleMonsterSpec xSyl = WithGender(MakeSpecUniform(ZM_SPECIES_SYLVASTAG, 20u), ZM_GENDER_MALE);
		const ZM_BattleMonsterSpec xGrn = WithGender(MakeSpecUniform(ZM_SPECIES_GRAINMAW, 20u), ZM_GENDER_FEMALE);
		ZENITH_ASSERT_TRUE(ZM_AreCompatible(xSyl, xGrn), "swapped pair compatible");
		ZM_BattleRNG xRng(0x5B10ull);
		const ZM_BattleMonsterSpec xEgg = ZM_GenerateEgg(xSyl, xGrn, xRng);
		ZENITH_ASSERT_EQ((u_int)xEgg.m_eSpecies, (u_int)ZM_SPECIES_NIBBIN, "female GRAINMAW -> NIBBIN");
	}
}

// With the universal breeder involved, the NON-universal parent seeds the line,
// regardless of slot order.
ZENITH_TEST(ZM_Data, Breeding_Offspring_UniversalPartnerSeedsLine)
{
	ZENITH_ASSERT_EQ((u_int)ZM_GetBaseEvolution(ZM_SPECIES_THICKETBUCK), (u_int)ZM_SPECIES_FERNFAWN, "premise");
	const ZM_BattleMonsterSpec xGloopet = MakeSpecUniform(ZM_SPECIES_GLOOPET, 20u);
	const ZM_BattleMonsterSpec xBuck    = WithGender(MakeSpecUniform(ZM_SPECIES_THICKETBUCK, 20u), ZM_GENDER_FEMALE);

	ZM_BattleRNG xRngA(0x60D5ull);
	const ZM_BattleMonsterSpec xEggA = ZM_GenerateEgg(xGloopet, xBuck, xRngA);   // universal in slot 0
	ZENITH_ASSERT_EQ((u_int)xEggA.m_eSpecies, (u_int)ZM_SPECIES_FERNFAWN, "GLOOPET first -> FERNFAWN");

	ZM_BattleRNG xRngB(0x60D5ull);
	const ZM_BattleMonsterSpec xEggB = ZM_GenerateEgg(xBuck, xGloopet, xRngB);   // universal in slot 1
	ZENITH_ASSERT_EQ((u_int)xEggB.m_eSpecies, (u_int)ZM_SPECIES_FERNFAWN, "GLOOPET second -> FERNFAWN");
}

// The egg's ability comes from the NON-universal mother, not from GLOOPET.
ZENITH_TEST(ZM_Data, Breeding_Offspring_AbilityFromNonUniversalMother)
{
	const ZM_BattleMonsterSpec xGloopet = MakeSpecUniform(ZM_SPECIES_GLOOPET, 20u, 20u,
		ZM_NATURE_FERAL, ZM_ABILITY_BEDROCK);
	const ZM_BattleMonsterSpec xSyl = WithGender(MakeSpecUniform(ZM_SPECIES_SYLVASTAG, 20u, 20u,
		ZM_NATURE_FERAL, ZM_ABILITY_STREAMLINE), ZM_GENDER_FEMALE);
	ZENITH_ASSERT_TRUE(ZM_AreCompatible(xGloopet, xSyl), "GLOOPET x SYLVASTAG compatible");
	ZM_BattleRNG xRng(0xAB111ull);
	const ZM_BattleMonsterSpec xEgg = ZM_GenerateEgg(xGloopet, xSyl, xRng);
	ZENITH_ASSERT_EQ((u_int)xEgg.m_eSpecies, (u_int)ZM_SPECIES_FERNFAWN,     "offspring = base of SYLVASTAG");
	ZENITH_ASSERT_EQ((u_int)xEgg.m_eAbility, (u_int)ZM_ABILITY_STREAMLINE,   "ability from non-universal mother");
	ZENITH_ASSERT_NE((u_int)xEgg.m_eAbility, (u_int)ZM_ABILITY_BEDROCK,      "not GLOOPET's ability");
}

// --- H.5 Daycare compatibility (box-6 SC-B) ------------------------------------

// A compatible opposite-gender pair accrues to the threshold and yields an egg of
// the FEMALE parent's base evolution.
ZENITH_TEST(ZM_Data, Daycare_SCB_OppositeGenderPairReachesEgg)
{
	ZM_DaycareState xState;
	ZM_DaycareDeposit(xState, WithGender(MakeSpecUniform(ZM_SPECIES_NIBBIN, 31u, 20u), ZM_GENDER_MALE));
	ZM_DaycareDeposit(xState, WithGender(MakeSpecUniform(ZM_SPECIES_STRAYLING, 31u, 20u), ZM_GENDER_FEMALE));
	ZENITH_ASSERT_TRUE(ZM_DaycarePairCompatible(xState), "opposite-gender shared-group pair is compatible");

	ZM_DaycareStep(xState, uZM_DAYCARE_EGG_STEP_THRESHOLD);
	ZENITH_ASSERT_TRUE(xState.m_bEggAvailable, "egg available at threshold");

	ZM_BattleRNG xRng(0xDACEull);
	ZM_BattleMonsterSpec xEgg;
	ZENITH_ASSERT_TRUE(ZM_DaycareCollectEgg(xState, xRng, ZM_BreedingParams{}, xEgg), "collect succeeds");
	ZENITH_ASSERT_EQ((u_int)xEgg.m_eSpecies, (u_int)ZM_SPECIES_STRAYLING, "offspring = female STRAYLING base evo");
	ZENITH_ASSERT_EQ(xState.m_uEggStepCounter, 0u, "counter reset after collect");
}

// Same-gender and genderless-non-universal daycare pairs never accrue an egg.
ZENITH_TEST(ZM_Data, Daycare_SCB_SameGenderOrGenderlessNeverEgg)
{
	// Two males sharing FIELD: incompatible, nothing accrues.
	ZM_DaycareState xSame;
	ZM_DaycareDeposit(xSame, WithGender(MakeSpecUniform(ZM_SPECIES_NIBBIN, 31u, 20u), ZM_GENDER_MALE));
	ZM_DaycareDeposit(xSame, WithGender(MakeSpecUniform(ZM_SPECIES_STRAYLING, 31u, 20u), ZM_GENDER_MALE));
	ZENITH_ASSERT_FALSE(ZM_DaycarePairCompatible(xSame), "two males -> incompatible");
	ZM_DaycareStep(xSame, 500u);
	ZENITH_ASSERT_EQ(xSame.m_uEggStepCounter, 0u, "same-gender pair accrues nothing");
	ZENITH_ASSERT_FALSE(xSame.m_bEggAvailable);

	// Two genderless non-universal blobs: also incompatible.
	ZM_DaycareState xNeuter;
	ZM_DaycareDeposit(xNeuter, MakeSpecUniform(ZM_SPECIES_RUBBLET, 31u, 20u));   // BLOB, genderless
	ZM_DaycareDeposit(xNeuter, MakeSpecUniform(ZM_SPECIES_SLAGLET, 31u, 20u));   // BLOB, genderless
	ZENITH_ASSERT_FALSE(ZM_DaycarePairCompatible(xNeuter), "genderless non-universal pair -> incompatible");
	ZM_DaycareStep(xNeuter, 500u);
	ZENITH_ASSERT_EQ(xNeuter.m_uEggStepCounter, 0u, "genderless pair accrues nothing");
	ZENITH_ASSERT_FALSE(xNeuter.m_bEggAvailable);
}

// A GLOOPET pairing (universal + genderless blob's cousin) breeds through the
// daycare; the offspring follows the non-universal partner.
ZENITH_TEST(ZM_Data, Daycare_SCB_UniversalPairingWorks)
{
	ZM_DaycareState xState;
	ZM_DaycareDeposit(xState, MakeSpecUniform(ZM_SPECIES_GLOOPET, 31u, 20u));                              // slot 0 universal
	ZM_DaycareDeposit(xState, WithGender(MakeSpecUniform(ZM_SPECIES_NIBBIN, 31u, 20u), ZM_GENDER_MALE));   // slot 1
	ZENITH_ASSERT_TRUE(ZM_DaycarePairCompatible(xState), "universal pairing is compatible");

	ZM_DaycareStep(xState, uZM_DAYCARE_EGG_STEP_THRESHOLD);
	ZENITH_ASSERT_TRUE(xState.m_bEggAvailable, "egg available at threshold");

	ZM_BattleRNG xRng(0x9111ull);
	ZM_BattleMonsterSpec xEgg;
	ZENITH_ASSERT_TRUE(ZM_DaycareCollectEgg(xState, xRng, ZM_BreedingParams{}, xEgg), "collect succeeds");
	ZENITH_ASSERT_EQ((u_int)xEgg.m_eSpecies, (u_int)ZM_SPECIES_NIBBIN, "offspring = non-universal partner base evo");
}

// ############################################################################
// I. Egg moves + ability / hidden-ability inheritance + hatch cycles
//    (box-6 SC-C) (ZM_Data)
//
// Derived { regular, hidden } ability pair (ZM_GetSpeciesAbilities: primary-type
// pool for regular, secondary-type OR archetype-fallback pool for hidden, forced
// distinct), the CONDITIONAL hidden-ability inheritance draw in ZM_GenerateEgg
// (fires only when the mother carries her species' hidden ability; Chance(60,100),
// appended AFTER the gender roll), the derived egg-move list (ZM_GetSpeciesEggMoves:
// own-type moves absent from the level-up learnset, table order, cap 6), the RNG-free
// egg-move inheritance (first empty slot after the L1 learnset, 4-cap, no eviction),
// and the derived hatch-cycle accessor (rarity base + size nudge).
//
// Exact values are pinned two ways so a gutted impl fails: (a) exact ability ids
// computed from the shipped derivation (family seed via ZM_GetSpeciesFamilySeed +
// the transcribed candidate pools, index math done by the compiler); (b) exact egg
// lists via an INDEPENDENT oracle (OracleEggMoves) that never calls the function
// under test. Every derivation premise (types / archetype / rarity / family / size)
// is asserted via ZM_GetSpeciesData so a data re-tag is caught loudly. The ability-
// inheritance goldens are derived by replaying the pinned draw order (IV -> nature ->
// gender -> conditional hidden) on an independent RNG, NEVER by calling ZM_GenerateEgg.
// Deterministic + hermetic. Purely ADDITIVE: existing fixtures have empty movesets
// (egg-move inheritance never fires) and regular mother abilities (hidden draw never
// fires), so every pre-SC-C golden stays byte-identical.
// ############################################################################

namespace
{
	// True iff eAbility appears in the first uCount entries of aePool.
	bool AbilityInPool(ZM_ABILITY_ID eAbility, const ZM_ABILITY_ID* aePool, u_int uCount)
	{
		for (u_int i = 0; i < uCount; ++i)
		{
			if (aePool[i] == eAbility) { return true; }
		}
		return false;
	}

	// Independent re-implementation of ZM_GetSpeciesEggMoves (box-6 SC-C): the species'
	// own-type moves NOT in its level-up learnset, in move-table order, capped at 6.
	// Uses only the production SUB-accessors (learnset / move table) -- never the function
	// under test -- so production == this pins the exact derived list against a gutted impl.
	ZM_EggMoves OracleEggMoves(ZM_SPECIES_ID eId)
	{
		ZM_EggMoves xOut = {};
		const ZM_SpeciesData& xData = ZM_GetSpeciesData(eId);
		if (xData.m_eRarity == ZM_RARITY_LEGENDARY) { return xOut; }

		const ZM_Learnset xLs = ZM_GetSpeciesLearnset(eId);
		const ZM_TYPE eT1 = xData.m_aeTypes[0];
		const ZM_TYPE eT2 = xData.m_aeTypes[1];
		const u_int uMoveCount = ZM_GetMoveCount();
		for (u_int i = 0; i < uMoveCount && xOut.m_uCount < uZM_MAX_EGG_MOVES; ++i)
		{
			const ZM_MOVE_ID   eMove = (ZM_MOVE_ID)i;
			const ZM_MoveData& xMove = ZM_GetMoveData(eMove);
			const bool bTypeMatch = (xMove.m_eType == eT1) ||
				(eT2 != ZM_TYPE_NONE && xMove.m_eType == eT2);
			if (!bTypeMatch) { continue; }

			bool bInLearnset = false;
			for (u_int k = 0; k < xLs.m_uCount; ++k)
			{
				if (xLs.m_axMoves[k].m_eMove == eMove) { bInLearnset = true; break; }
			}
			if (bInLearnset) { continue; }

			xOut.m_aeMoves[xOut.m_uCount++] = eMove;
		}
		return xOut;
	}

	// The offspring ability a HIDDEN-carrying mother yields, derived by replaying the pinned
	// draw order (IV -> nature -> gender -> hidden Chance) on an independent RNG -- mirrors
	// ZM_GenerateEgg step E for the mother-carries-hidden case. Never calls ZM_GenerateEgg.
	ZM_ABILITY_ID OracleDrawnAbility(const u_int (&aMotherIV)[ZM_STAT_COUNT],
		const u_int (&aFatherIV)[ZM_STAT_COUNT], ZM_SPECIES_ID eOffspring, u_int64 ulSeed)
	{
		ZM_BattleRNG xRngO(ulSeed);
		OracleGenerate(aMotherIV, aFatherIV, false, ZM_NATURE_COUNT, xRngO);   // IV -> nature
		ZM_RollGender(eOffspring, xRngO);                                       // gender
		const ZM_SpeciesAbilities xAb = ZM_GetSpeciesAbilities(eOffspring);
		return xRngO.Chance(uZM_BREED_HIDDEN_INHERIT_PCT, 100u) ? xAb.m_eHidden : xAb.m_eRegular;
	}

	// The base-evo level-1 learnset fill (ZM_GenerateEgg step F1) for a species: the first
	// uZM_MAX_MOVES learnset moves at level <= the egg hatch level. Returns the count filled.
	u_int ComputeLevelOneFill(ZM_SPECIES_ID eSpecies, ZM_MOVE_ID (&aeOut)[uZM_MAX_MOVES])
	{
		for (u_int i = 0; i < uZM_MAX_MOVES; ++i) { aeOut[i] = ZM_MOVE_NONE; }
		const ZM_Learnset xLs = ZM_GetSpeciesLearnset(eSpecies);
		u_int uFilled = 0u;
		for (u_int k = 0; k < xLs.m_uCount && uFilled < uZM_MAX_MOVES; ++k)
		{
			if (xLs.m_axMoves[k].m_uLevel <= uZM_EGG_HATCH_LEVEL)
			{
				aeOut[uFilled++] = xLs.m_axMoves[k].m_eMove;
			}
		}
		return uFilled;
	}
}

// --- I.1 ZM_GetSpeciesAbilities derivation --------------------------------------

// Every species derives a valid, DISTINCT { regular, hidden } pair (the forced-distinct
// invariant across the whole roster). Catches a gutted impl returning equal / out-of-range.
ZENITH_TEST(ZM_Data, Breeding_Abilities_AllSpeciesRegularDistinctInRange)
{
	const u_int uCount = ZM_GetSpeciesCount();
	for (u_int s = 0; s < uCount; ++s)
	{
		const ZM_SPECIES_ID eId = (ZM_SPECIES_ID)s;
		const ZM_SpeciesAbilities xAb = ZM_GetSpeciesAbilities(eId);
		ZENITH_ASSERT_LT((u_int)xAb.m_eRegular, (u_int)ZM_ABILITY_COUNT, "%s regular in range", ZM_GetSpeciesName(eId));
		ZENITH_ASSERT_LT((u_int)xAb.m_eHidden,  (u_int)ZM_ABILITY_COUNT, "%s hidden in range",  ZM_GetSpeciesName(eId));
		ZENITH_ASSERT_NE((u_int)xAb.m_eRegular, (u_int)xAb.m_eHidden, "%s regular/hidden must differ", ZM_GetSpeciesName(eId));
	}
}

// SINGLE-type species: regular from the primary TYPE pool, hidden from the ARCHETYPE
// fallback pool. NIBBIN (NORMAL, QUADRUPED, family 5). The two pools are disjoint, so no
// distinctness fixup applies and the raw picks are the final ids. Expected ids are computed
// from the shipped derivation (family seed + transcribed pools), so a gutted impl fails.
ZENITH_TEST(ZM_Data, Breeding_Abilities_SingleTypeArchetypeHidden_Exact)
{
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(ZM_SPECIES_NIBBIN).m_aeTypes[0], (u_int)ZM_TYPE_NORMAL, "premise types[0] NORMAL");
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(ZM_SPECIES_NIBBIN).m_aeTypes[1], (u_int)ZM_TYPE_NONE, "premise single-type");
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(ZM_SPECIES_NIBBIN).m_eArchetype, (u_int)ZM_ARCHETYPE_QUADRUPED, "premise QUADRUPED");
	ZENITH_ASSERT_EQ(ZM_GetSpeciesData(ZM_SPECIES_NIBBIN).m_uFamilyId, 5u, "premise family 5");

	const u_int uSeed = ZM_GetSpeciesFamilySeed(ZM_SPECIES_NIBBIN);
	const ZM_ABILITY_ID aeNormal[3] = { ZM_ABILITY_QUICKDRAW, ZM_ABILITY_GUARDIAN, ZM_ABILITY_WAKEFUL };
	const ZM_ABILITY_ID aeQuad[2]   = { ZM_ABILITY_DAUNTINGROAR, ZM_ABILITY_GRAZER };
	const ZM_ABILITY_ID eExpReg = aeNormal[uSeed % 3u];
	const ZM_ABILITY_ID eExpHid = aeQuad[(uSeed >> 5u) % 2u];

	const ZM_SpeciesAbilities xAb = ZM_GetSpeciesAbilities(ZM_SPECIES_NIBBIN);
	ZENITH_ASSERT_EQ((u_int)xAb.m_eRegular, (u_int)eExpReg, "NIBBIN regular = NORMAL pool pick");
	ZENITH_ASSERT_EQ((u_int)xAb.m_eHidden,  (u_int)eExpHid, "NIBBIN hidden = QUADRUPED archetype pick");
	ZENITH_ASSERT_NE((u_int)xAb.m_eRegular, (u_int)xAb.m_eHidden, "NIBBIN slots distinct");
}

// DUAL-type species: hidden from the SECONDARY type pool (not the archetype fallback).
// SYLVASTAG (GRASS/EARTH, family 1); GRASS and EARTH pools are disjoint -> no fixup.
ZENITH_TEST(ZM_Data, Breeding_Abilities_DualTypeSecondaryHidden_Exact)
{
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(ZM_SPECIES_SYLVASTAG).m_aeTypes[0], (u_int)ZM_TYPE_GRASS, "premise types[0] GRASS");
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(ZM_SPECIES_SYLVASTAG).m_aeTypes[1], (u_int)ZM_TYPE_EARTH, "premise types[1] EARTH");
	ZENITH_ASSERT_EQ(ZM_GetSpeciesData(ZM_SPECIES_SYLVASTAG).m_uFamilyId, 1u, "premise family 1");

	const u_int uSeed = ZM_GetSpeciesFamilySeed(ZM_SPECIES_SYLVASTAG);
	const ZM_ABILITY_ID aeGrass[4] = { ZM_ABILITY_VERDANTSURGE, ZM_ABILITY_GRAZER, ZM_ABILITY_ROOTFEED, ZM_ABILITY_THORNMAIL };
	const ZM_ABILITY_ID aeEarth[4] = { ZM_ABILITY_GRITSTRIDE, ZM_ABILITY_SANDCALLER, ZM_ABILITY_BEDROCK, ZM_ABILITY_DOWNDRAFT };
	const ZM_ABILITY_ID eExpReg = aeGrass[uSeed % 4u];
	const ZM_ABILITY_ID eExpHid = aeEarth[(uSeed >> 5u) % 4u];

	const ZM_SpeciesAbilities xAb = ZM_GetSpeciesAbilities(ZM_SPECIES_SYLVASTAG);
	ZENITH_ASSERT_EQ((u_int)xAb.m_eRegular, (u_int)eExpReg, "SYLVASTAG regular = GRASS pool pick");
	ZENITH_ASSERT_EQ((u_int)xAb.m_eHidden,  (u_int)eExpHid, "SYLVASTAG hidden = EARTH pool pick");
	ZENITH_ASSERT_NE((u_int)xAb.m_eRegular, (u_int)xAb.m_eHidden, "SYLVASTAG slots distinct");
}

// A second dual-type pin from a different family: PYROCLAST (FIRE/STONE, family 2); FIRE and
// STONE pools are disjoint -> no fixup.
ZENITH_TEST(ZM_Data, Breeding_Abilities_DualTypeSecondaryHidden_ExactPyroclast)
{
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(ZM_SPECIES_PYROCLAST).m_aeTypes[0], (u_int)ZM_TYPE_FIRE, "premise types[0] FIRE");
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(ZM_SPECIES_PYROCLAST).m_aeTypes[1], (u_int)ZM_TYPE_STONE, "premise types[1] STONE");
	ZENITH_ASSERT_EQ(ZM_GetSpeciesData(ZM_SPECIES_PYROCLAST).m_uFamilyId, 2u, "premise family 2");

	const u_int uSeed = ZM_GetSpeciesFamilySeed(ZM_SPECIES_PYROCLAST);
	const ZM_ABILITY_ID aeFire[4]  = { ZM_ABILITY_EMBERSURGE, ZM_ABILITY_CINDERSKIN, ZM_ABILITY_SUNCHASER, ZM_ABILITY_CINDERDRINK };
	const ZM_ABILITY_ID aeStone[4] = { ZM_ABILITY_BEDROCK, ZM_ABILITY_AFTERSHOCK, ZM_ABILITY_SOLIDCORE, ZM_ABILITY_HEAVYPLATE };
	const ZM_ABILITY_ID eExpReg = aeFire[uSeed % 4u];
	const ZM_ABILITY_ID eExpHid = aeStone[(uSeed >> 5u) % 4u];

	const ZM_SpeciesAbilities xAb = ZM_GetSpeciesAbilities(ZM_SPECIES_PYROCLAST);
	ZENITH_ASSERT_EQ((u_int)xAb.m_eRegular, (u_int)eExpReg, "PYROCLAST regular = FIRE pool pick");
	ZENITH_ASSERT_EQ((u_int)xAb.m_eHidden,  (u_int)eExpHid, "PYROCLAST hidden = STONE pool pick");
	ZENITH_ASSERT_NE((u_int)xAb.m_eRegular, (u_int)xAb.m_eHidden, "PYROCLAST slots distinct");
}

// Pool SOURCE guard: the regular comes from the PRIMARY-type pool for both single- and
// dual-typed species; the hidden comes from the ARCHETYPE fallback pool for a single-type
// species and from the SECONDARY-type pool for a dual-type one.
ZENITH_TEST(ZM_Data, Breeding_Abilities_RegularPrimaryHiddenPoolSource)
{
	// Single-type NIBBIN: regular in the NORMAL pool, hidden in the QUADRUPED archetype pool.
	const ZM_ABILITY_ID aeNormal[3] = { ZM_ABILITY_QUICKDRAW, ZM_ABILITY_GUARDIAN, ZM_ABILITY_WAKEFUL };
	const ZM_ABILITY_ID aeQuad[2]   = { ZM_ABILITY_DAUNTINGROAR, ZM_ABILITY_GRAZER };
	const ZM_SpeciesAbilities xNib = ZM_GetSpeciesAbilities(ZM_SPECIES_NIBBIN);
	ZENITH_ASSERT_TRUE(AbilityInPool(xNib.m_eRegular, aeNormal, 3u), "NIBBIN regular from NORMAL pool");
	ZENITH_ASSERT_TRUE(AbilityInPool(xNib.m_eHidden,  aeQuad, 2u),   "NIBBIN hidden from QUADRUPED archetype pool");

	// Dual-type SYLVASTAG: regular in the GRASS (primary) pool, hidden in the EARTH (secondary
	// type) pool -- NOT the archetype fallback.
	const ZM_ABILITY_ID aeGrass[4] = { ZM_ABILITY_VERDANTSURGE, ZM_ABILITY_GRAZER, ZM_ABILITY_ROOTFEED, ZM_ABILITY_THORNMAIL };
	const ZM_ABILITY_ID aeEarth[4] = { ZM_ABILITY_GRITSTRIDE, ZM_ABILITY_SANDCALLER, ZM_ABILITY_BEDROCK, ZM_ABILITY_DOWNDRAFT };
	const ZM_SpeciesAbilities xSyl = ZM_GetSpeciesAbilities(ZM_SPECIES_SYLVASTAG);
	ZENITH_ASSERT_TRUE(AbilityInPool(xSyl.m_eRegular, aeGrass, 4u), "SYLVASTAG regular from GRASS pool");
	ZENITH_ASSERT_TRUE(AbilityInPool(xSyl.m_eHidden,  aeEarth, 4u), "SYLVASTAG hidden from EARTH pool");
}

// --- I.2 Ability inheritance (ZM_GenerateEgg step E) ----------------------------

// Mother carrying her REGULAR ability -> offspring COPIES it verbatim with ZERO extra draws:
// an oracle stopping after the gender roll sits at the same RNG position (lock-step).
ZENITH_TEST(ZM_Data, Breeding_AbilityInherit_MotherRegularCopiedNoExtraDraw)
{
	const u_int aMotherIV[ZM_STAT_COUNT] = { 0u, 6u, 12u, 18u, 24u, 31u };
	const u_int aFatherIV[ZM_STAT_COUNT] = { 31u, 25u, 19u, 13u, 7u, 1u };
	const ZM_SpeciesAbilities xMotherAb = ZM_GetSpeciesAbilities(ZM_SPECIES_SYLVASTAG);
	ZENITH_ASSERT_NE((u_int)xMotherAb.m_eRegular, (u_int)xMotherAb.m_eHidden, "premise: mother abilities distinct");

	const ZM_BattleMonsterSpec xMother = WithGender(MakeSpec(ZM_SPECIES_SYLVASTAG, aMotherIV, 40u,
		ZM_NATURE_BRUTISH, xMotherAb.m_eRegular), ZM_GENDER_FEMALE);
	const ZM_BattleMonsterSpec xFather = WithGender(MakeSpec(ZM_SPECIES_NIBBIN, aFatherIV), ZM_GENDER_MALE);
	ZENITH_ASSERT_TRUE(ZM_AreCompatible(xMother, xFather), "premise: compatible pair");

	ZM_BattleRNG xRng(0xA11CE0ull);
	const ZM_BattleMonsterSpec xEgg = ZM_GenerateEgg(xMother, xFather, xRng);
	ZENITH_ASSERT_EQ((u_int)xEgg.m_eSpecies, (u_int)ZM_SPECIES_FERNFAWN, "offspring FERNFAWN");
	ZENITH_ASSERT_EQ((u_int)xEgg.m_eAbility, (u_int)xMotherAb.m_eRegular, "offspring copies mother's regular ability");

	// Lock-step: replay IV -> nature -> gender on an independent RNG; the two streams must
	// stay aligned, proving ZM_GenerateEgg consumed NO appended hidden-ability draw.
	ZM_BattleRNG xRngO(0xA11CE0ull);
	OracleGenerate(aMotherIV, aFatherIV, false, ZM_NATURE_COUNT, xRngO);   // IV -> nature
	ZM_RollGender(ZM_SPECIES_FERNFAWN, xRngO);                             // gender
	for (u_int k = 0; k < 6u; ++k)
	{
		ZENITH_ASSERT_EQ(xRng.RandBelow(997u), xRngO.RandBelow(997u),
			"regular-ability breed took an extra draw (k %u)", k);
	}
}

// Mother carrying her HIDDEN ability -> the offspring's ability is DRAWN: over a fixed seed
// list it is the offspring's hidden ~60% and regular ~40% (wide deterministic band), each
// seed matches the independent Chance oracle, and it is always one of the OFFSPRING species'
// two abilities.
ZENITH_TEST(ZM_Data, Breeding_AbilityInherit_MotherHiddenDrawsAboutSixtyPct)
{
	const u_int aMotherIV[ZM_STAT_COUNT] = { 4u, 4u, 4u, 4u, 4u, 4u };
	const u_int aFatherIV[ZM_STAT_COUNT] = { 24u, 24u, 24u, 24u, 24u, 24u };
	const ZM_SPECIES_ID eOffspring = ZM_GetBaseEvolution(ZM_SPECIES_SYLVASTAG);   // FERNFAWN
	ZENITH_ASSERT_EQ((u_int)eOffspring, (u_int)ZM_SPECIES_FERNFAWN, "premise: offspring FERNFAWN");

	const ZM_SpeciesAbilities xMotherAb = ZM_GetSpeciesAbilities(ZM_SPECIES_SYLVASTAG);
	const ZM_SpeciesAbilities xEggAb    = ZM_GetSpeciesAbilities(eOffspring);
	ZENITH_ASSERT_NE((u_int)xEggAb.m_eRegular, (u_int)xEggAb.m_eHidden, "premise: offspring abilities distinct");

	const ZM_BattleMonsterSpec xMother = WithGender(MakeSpec(ZM_SPECIES_SYLVASTAG, aMotherIV, 40u,
		ZM_NATURE_FERAL, xMotherAb.m_eHidden), ZM_GENDER_FEMALE);
	const ZM_BattleMonsterSpec xFather = WithGender(MakeSpec(ZM_SPECIES_NIBBIN, aFatherIV), ZM_GENDER_MALE);

	const u_int uN = 256u;
	u_int uHidden = 0u;
	u_int uRegular = 0u;
	for (u_int i = 0; i < uN; ++i)
	{
		const u_int64 ulSeed = (u_int64)(i + 1u) * 0x9E3779B97F4A7C15ull;
		ZM_BattleRNG xRng(ulSeed);
		const ZM_BattleMonsterSpec xEgg = ZM_GenerateEgg(xMother, xFather, xRng);

		const ZM_ABILITY_ID eExpected = OracleDrawnAbility(aMotherIV, aFatherIV, eOffspring, ulSeed);
		ZENITH_ASSERT_EQ((u_int)xEgg.m_eAbility, (u_int)eExpected, "drawn ability vs oracle (i %u)", i);
		ZENITH_ASSERT_TRUE(xEgg.m_eAbility == xEggAb.m_eHidden || xEgg.m_eAbility == xEggAb.m_eRegular,
			"offspring ability must be its own regular/hidden (i %u)", i);
		if (xEgg.m_eAbility == xEggAb.m_eHidden) { ++uHidden; } else { ++uRegular; }
	}
	ZENITH_ASSERT_EQ(uHidden + uRegular, uN, "every draw resolves to a concrete ability");
	// ~60% hidden across 256 draws -> a wide deterministic band around 153.6.
	ZENITH_ASSERT_GE(uHidden, 110u, "hidden share too low (got %u)", uHidden);
	ZENITH_ASSERT_LE(uHidden, 200u, "hidden share too high (got %u)", uHidden);
	ZENITH_ASSERT_GT(uHidden, uRegular, "hidden must be the majority at 60 percent");
}

// The hidden-ability draw is reproducible: same seed -> same drawn ability.
ZENITH_TEST(ZM_Data, Breeding_AbilityInherit_MotherHiddenReproducibleSameSeed)
{
	const ZM_SpeciesAbilities xMotherAb = ZM_GetSpeciesAbilities(ZM_SPECIES_SYLVASTAG);
	const ZM_BattleMonsterSpec xMother = WithGender(MakeSpecUniform(ZM_SPECIES_SYLVASTAG, 18u, 40u,
		ZM_NATURE_FERAL, xMotherAb.m_eHidden), ZM_GENDER_FEMALE);
	const ZM_BattleMonsterSpec xFather = WithGender(MakeSpecUniform(ZM_SPECIES_NIBBIN, 18u), ZM_GENDER_MALE);

	const u_int64 aulSeeds[] = { 0x1ull, 0x2222ull, 0xC0FFEEull, 0x9E3779B1ull };
	for (u_int s = 0; s < (u_int)(sizeof(aulSeeds) / sizeof(aulSeeds[0])); ++s)
	{
		ZM_BattleRNG xRngA(aulSeeds[s]);
		ZM_BattleRNG xRngB(aulSeeds[s]);
		const ZM_BattleMonsterSpec xEggA = ZM_GenerateEgg(xMother, xFather, xRngA);
		const ZM_BattleMonsterSpec xEggB = ZM_GenerateEgg(xMother, xFather, xRngB);
		ZENITH_ASSERT_EQ((u_int)xEggA.m_eAbility, (u_int)xEggB.m_eAbility, "same seed same drawn ability (s %u)", s);
	}
}

// PAIRED positive/control: at a shared golden seed, a regular-ability mother (no draw) and a
// hidden-ability mother (draw fires) produce byte-identical IV / nature / gender -- so the
// hidden Chance draw is APPENDED after the gender roll and perturbs nothing earlier.
ZENITH_TEST(ZM_Data, Breeding_AbilityInherit_HiddenDrawAppendedAfterGenderPaired)
{
	const u_int aMotherIV[ZM_STAT_COUNT] = { 0u, 6u, 12u, 18u, 24u, 31u };
	const u_int aFatherIV[ZM_STAT_COUNT] = { 31u, 25u, 19u, 13u, 7u, 1u };
	const ZM_SpeciesAbilities xMotherAb = ZM_GetSpeciesAbilities(ZM_SPECIES_SYLVASTAG);
	const ZM_SPECIES_ID eOffspring = ZM_SPECIES_FERNFAWN;

	const ZM_BattleMonsterSpec xFather = WithGender(MakeSpec(ZM_SPECIES_NIBBIN, aFatherIV), ZM_GENDER_MALE);
	const ZM_BattleMonsterSpec xMotherReg = WithGender(MakeSpec(ZM_SPECIES_SYLVASTAG, aMotherIV, 40u,
		ZM_NATURE_BRUTISH, xMotherAb.m_eRegular), ZM_GENDER_FEMALE);
	const ZM_BattleMonsterSpec xMotherHid = WithGender(MakeSpec(ZM_SPECIES_SYLVASTAG, aMotherIV, 40u,
		ZM_NATURE_BRUTISH, xMotherAb.m_eHidden), ZM_GENDER_FEMALE);

	// Same seed for both: the ONLY difference is whether step E's Chance draw fires.
	ZM_BattleRNG xRngReg(0xA1ull);
	ZM_BattleRNG xRngHid(0xA1ull);
	const ZM_BattleMonsterSpec xEggReg = ZM_GenerateEgg(xMotherReg, xFather, xRngReg);
	const ZM_BattleMonsterSpec xEggHid = ZM_GenerateEgg(xMotherHid, xFather, xRngHid);

	for (u_int i = 0; i < ZM_STAT_COUNT; ++i)
	{
		ZENITH_ASSERT_EQ(xEggReg.m_auIV[i], xEggHid.m_auIV[i], "hidden draw perturbed IV %u", i);
	}
	ZENITH_ASSERT_EQ((u_int)xEggReg.m_eNature, (u_int)xEggHid.m_eNature, "hidden draw perturbed nature");
	ZENITH_ASSERT_EQ((u_int)xEggReg.m_eGender, (u_int)xEggHid.m_eGender, "hidden draw perturbed gender");

	// IV + nature are still byte-identical to the pre-ability oracle (same seed as Golden_Seed1).
	ZM_BattleRNG xRngO(0xA1ull);
	const OracleEgg xOracle = OracleGenerate(aMotherIV, aFatherIV, false, ZM_NATURE_COUNT, xRngO);
	AssertEggMatchesOracle(xEggReg, xOracle, "paired-reg");

	// Regular mother -> verbatim copy; hidden mother -> the positioned Chance result.
	ZENITH_ASSERT_EQ((u_int)xEggReg.m_eAbility, (u_int)xMotherAb.m_eRegular, "regular copied verbatim");
	ZM_RollGender(eOffspring, xRngO);   // advance the oracle RNG past gender to the hidden-draw position
	const ZM_SpeciesAbilities xEggAb = ZM_GetSpeciesAbilities(eOffspring);
	const ZM_ABILITY_ID eExpectedHid = xRngO.Chance(uZM_BREED_HIDDEN_INHERIT_PCT, 100u)
		? xEggAb.m_eHidden : xEggAb.m_eRegular;
	ZENITH_ASSERT_EQ((u_int)xEggHid.m_eAbility, (u_int)eExpectedHid, "hidden-mother egg = positioned Chance result");
}

// SC-B role interaction: with the universal breeder (GLOOPET) as the FATHER role, the non-
// universal SYLVASTAG mother's hidden ability still drives the draw; the offspring gets ITS
// OWN species' regular/hidden -- never the universal parent's ability.
ZENITH_TEST(ZM_Data, Breeding_AbilityInherit_UniversalNonUniversalMotherHiddenDraws)
{
	const u_int aIV[ZM_STAT_COUNT] = { 12u, 12u, 12u, 12u, 12u, 12u };
	const ZM_SpeciesAbilities xMotherAb = ZM_GetSpeciesAbilities(ZM_SPECIES_SYLVASTAG);
	const ZM_SPECIES_ID eOffspring = ZM_GetBaseEvolution(ZM_SPECIES_SYLVASTAG);   // FERNFAWN
	const ZM_SpeciesAbilities xEggAb = ZM_GetSpeciesAbilities(eOffspring);

	const ZM_BattleMonsterSpec xGloopet = MakeSpecUniform(ZM_SPECIES_GLOOPET, 12u, 20u,
		ZM_NATURE_FERAL, ZM_ABILITY_BEDROCK);
	const ZM_BattleMonsterSpec xMother = WithGender(MakeSpec(ZM_SPECIES_SYLVASTAG, aIV, 30u,
		ZM_NATURE_FERAL, xMotherAb.m_eHidden), ZM_GENDER_FEMALE);
	ZENITH_ASSERT_TRUE(ZM_AreCompatible(xGloopet, xMother), "premise: GLOOPET x SYLVASTAG compatible");

	u_int uHidden = 0u, uRegular = 0u;
	const u_int uN = 64u;
	for (u_int i = 0; i < uN; ++i)
	{
		const u_int64 ulSeed = (u_int64)(i + 1u) * 0xD1B54A32D192ED03ull;
		ZM_BattleRNG xRng(ulSeed);
		const ZM_BattleMonsterSpec xEgg = ZM_GenerateEgg(xGloopet, xMother, xRng);   // GLOOPET in slot 0
		ZENITH_ASSERT_EQ((u_int)xEgg.m_eSpecies, (u_int)ZM_SPECIES_FERNFAWN, "offspring = SYLVASTAG base evo");

		const ZM_ABILITY_ID eExpected = OracleDrawnAbility(aIV, aIV, eOffspring, ulSeed);
		ZENITH_ASSERT_EQ((u_int)xEgg.m_eAbility, (u_int)eExpected, "universal-pairing drawn ability vs oracle (i %u)", i);
		ZENITH_ASSERT_NE((u_int)xEgg.m_eAbility, (u_int)ZM_ABILITY_BEDROCK, "ability never from the universal breeder");
		if (xEgg.m_eAbility == xEggAb.m_eHidden) { ++uHidden; } else { ++uRegular; }
	}
	ZENITH_ASSERT_GT(uHidden, 0u, "expected some hidden draws through the universal-role path");
	ZENITH_ASSERT_GT(uRegular, 0u, "expected some regular draws through the universal-role path");
}

// --- I.3 ZM_GetSpeciesEggMoves derivation ---------------------------------------

// Every returned egg move is DISJOINT from the species' own level-up learnset.
ZENITH_TEST(ZM_Data, Breeding_EggMoves_DisjointFromLearnset)
{
	const ZM_SPECIES_ID aeSample[] = {
		ZM_SPECIES_NIBBIN, ZM_SPECIES_FERNFAWN, ZM_SPECIES_SYLVASTAG,
		ZM_SPECIES_KINDLET, ZM_SPECIES_PIPWIT, ZM_SPECIES_WYRMLING,
	};
	for (u_int s = 0; s < (u_int)(sizeof(aeSample) / sizeof(aeSample[0])); ++s)
	{
		const ZM_SPECIES_ID eId = aeSample[s];
		const ZM_EggMoves xEgg = ZM_GetSpeciesEggMoves(eId);
		const ZM_Learnset  xLs  = ZM_GetSpeciesLearnset(eId);
		for (u_int e = 0; e < xEgg.m_uCount; ++e)
		{
			for (u_int k = 0; k < xLs.m_uCount; ++k)
			{
				ZENITH_ASSERT_NE((u_int)xEgg.m_aeMoves[e], (u_int)xLs.m_axMoves[k].m_eMove,
					"egg move must not be in the level-up learnset (%s move %u)", ZM_GetSpeciesName(eId), e);
			}
		}
	}
}

// Every returned egg move is of one of the species' OWN type slots.
ZENITH_TEST(ZM_Data, Breeding_EggMoves_AllMovesAreOwnType)
{
	const ZM_SPECIES_ID aeSample[] = {
		ZM_SPECIES_NIBBIN, ZM_SPECIES_FERNFAWN, ZM_SPECIES_SYLVASTAG,
		ZM_SPECIES_KINDLET, ZM_SPECIES_FINLET, ZM_SPECIES_WYRMLING,
	};
	for (u_int s = 0; s < (u_int)(sizeof(aeSample) / sizeof(aeSample[0])); ++s)
	{
		const ZM_SPECIES_ID eId = aeSample[s];
		const ZM_SpeciesData& xData = ZM_GetSpeciesData(eId);
		const ZM_EggMoves xEgg = ZM_GetSpeciesEggMoves(eId);
		for (u_int e = 0; e < xEgg.m_uCount; ++e)
		{
			const ZM_TYPE eMoveType = ZM_GetMoveData(xEgg.m_aeMoves[e]).m_eType;
			const bool bOwnType = (eMoveType == xData.m_aeTypes[0]) ||
				(xData.m_aeTypes[1] != ZM_TYPE_NONE && eMoveType == xData.m_aeTypes[1]);
			ZENITH_ASSERT_TRUE(bOwnType, "egg move must be of the species' own type(s) (%s move %u)",
				ZM_GetSpeciesName(eId), e);
		}
	}
}

// Legendaries return an empty egg-move list; no species exceeds the cap of 6.
ZENITH_TEST(ZM_Data, Breeding_EggMoves_LegendaryEmptyAndCappedAtSix)
{
	const ZM_SPECIES_ID aeLegendary[] = { ZM_SPECIES_ZENARIS, ZM_SPECIES_NADIRATH, ZM_SPECIES_EQUINARA };
	for (u_int i = 0; i < (u_int)(sizeof(aeLegendary) / sizeof(aeLegendary[0])); ++i)
	{
		ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(aeLegendary[i]).m_eRarity, (u_int)ZM_RARITY_LEGENDARY,
			"premise: legendary %u", i);
		ZENITH_ASSERT_EQ(ZM_GetSpeciesEggMoves(aeLegendary[i]).m_uCount, 0u, "legendary has no egg moves (%u)", i);
	}
	const u_int uCount = ZM_GetSpeciesCount();
	for (u_int s = 0; s < uCount; ++s)
	{
		ZENITH_ASSERT_LE(ZM_GetSpeciesEggMoves((ZM_SPECIES_ID)s).m_uCount, uZM_MAX_EGG_MOVES,
			"%s egg-move count exceeds the cap", ZM_GetSpeciesName((ZM_SPECIES_ID)s));
	}
}

// Exact derived egg-move list matches an INDEPENDENT oracle (own-type minus learnset, table
// order, cap 6) that never calls the function under test -- pins the list against a gutted impl.
ZENITH_TEST(ZM_Data, Breeding_EggMoves_ExactListMatchesOracle)
{
	const ZM_SPECIES_ID aeSample[] = { ZM_SPECIES_NIBBIN, ZM_SPECIES_FERNFAWN };
	for (u_int s = 0; s < (u_int)(sizeof(aeSample) / sizeof(aeSample[0])); ++s)
	{
		const ZM_SPECIES_ID eId = aeSample[s];
		const ZM_EggMoves xProd   = ZM_GetSpeciesEggMoves(eId);
		const ZM_EggMoves xOracle = OracleEggMoves(eId);
		ZENITH_ASSERT_GT(xProd.m_uCount, 0u, "premise: %s has derived egg moves", ZM_GetSpeciesName(eId));
		ZENITH_ASSERT_EQ(xProd.m_uCount, xOracle.m_uCount, "%s egg-move count vs oracle", ZM_GetSpeciesName(eId));
		for (u_int e = 0; e < xOracle.m_uCount; ++e)
		{
			ZENITH_ASSERT_EQ((u_int)xProd.m_aeMoves[e], (u_int)xOracle.m_aeMoves[e],
				"%s egg move %u vs oracle", ZM_GetSpeciesName(eId), e);
		}
	}
}

// --- I.4 Egg-move inheritance (ZM_GenerateEgg step F2) --------------------------

// A parent that knows an offspring egg move passes it into the FIRST empty slot after the
// base-evo level-1 learnset. offspring = NIBBIN (mother GRAINMAW), a NORMAL line with a full
// egg-move pool.
ZENITH_TEST(ZM_Data, Breeding_EggMoveInherit_ParentEggMovePlacedAfterLevelOne)
{
	const ZM_SPECIES_ID eOffspring = ZM_GetBaseEvolution(ZM_SPECIES_GRAINMAW);   // NIBBIN
	ZENITH_ASSERT_EQ((u_int)eOffspring, (u_int)ZM_SPECIES_NIBBIN, "premise: offspring NIBBIN");

	ZM_MOVE_ID aeL1[uZM_MAX_MOVES];
	const u_int uL1 = ComputeLevelOneFill(eOffspring, aeL1);
	ZENITH_ASSERT_GE(uL1, 1u, "premise: at least one L1 move");
	ZENITH_ASSERT_LT(uL1, uZM_MAX_MOVES, "premise: room for an inherited egg move");

	const ZM_EggMoves xEggMoves = ZM_GetSpeciesEggMoves(eOffspring);
	ZENITH_ASSERT_GE(xEggMoves.m_uCount, 1u, "premise: offspring has egg moves");
	const ZM_MOVE_ID eInherit = xEggMoves.m_aeMoves[0];

	const ZM_BattleMonsterSpec xMother = WithGender(MakeSpecUniform(ZM_SPECIES_GRAINMAW, 20u), ZM_GENDER_FEMALE);
	ZM_BattleMonsterSpec xFather = WithGender(MakeSpecUniform(ZM_SPECIES_STRAYLING, 20u), ZM_GENDER_MALE);
	xFather.m_aeMoves[0] = eInherit;   // the FATHER knows an offspring egg move
	ZENITH_ASSERT_TRUE(ZM_AreCompatible(xMother, xFather), "premise: compatible pair");

	ZM_BattleRNG xRng(0xE66A0ull);
	const ZM_BattleMonsterSpec xEgg = ZM_GenerateEgg(xMother, xFather, xRng);

	for (u_int i = 0; i < uL1; ++i)
	{
		ZENITH_ASSERT_EQ((u_int)xEgg.m_aeMoves[i], (u_int)aeL1[i], "L1 move slot %u", i);
	}
	ZENITH_ASSERT_EQ((u_int)xEgg.m_aeMoves[uL1], (u_int)eInherit, "inherited egg move in first empty slot");
	for (u_int i = uL1 + 1u; i < uZM_MAX_MOVES; ++i)
	{
		ZENITH_ASSERT_EQ((u_int)xEgg.m_aeMoves[i], (u_int)ZM_MOVE_NONE, "trailing slot %u empty", i);
	}
}

// CONTROL: a parent move that is NOT one of the offspring's derived egg moves (a FIRE move vs
// NIBBIN's NORMAL egg pool) is never inherited -- only the L1 learnset fills.
ZENITH_TEST(ZM_Data, Breeding_EggMoveInherit_NonEggMoveNotInherited)
{
	const ZM_SPECIES_ID eOffspring = ZM_GetBaseEvolution(ZM_SPECIES_GRAINMAW);   // NIBBIN
	ZM_MOVE_ID aeL1[uZM_MAX_MOVES];
	const u_int uL1 = ComputeLevelOneFill(eOffspring, aeL1);

	const ZM_MOVE_ID eNonEgg = ZM_MOVE_CINDERSPIT;   // FIRE, never a NIBBIN egg move
	const ZM_EggMoves xEggMoves = ZM_GetSpeciesEggMoves(eOffspring);
	for (u_int e = 0; e < xEggMoves.m_uCount; ++e)
	{
		ZENITH_ASSERT_NE((u_int)xEggMoves.m_aeMoves[e], (u_int)eNonEgg, "premise: control move is not an egg move");
	}

	const ZM_BattleMonsterSpec xMother = WithGender(MakeSpecUniform(ZM_SPECIES_GRAINMAW, 20u), ZM_GENDER_FEMALE);
	ZM_BattleMonsterSpec xFather = WithGender(MakeSpecUniform(ZM_SPECIES_STRAYLING, 20u), ZM_GENDER_MALE);
	xFather.m_aeMoves[0] = eNonEgg;

	ZM_BattleRNG xRng(0xC077011ull);
	const ZM_BattleMonsterSpec xEgg = ZM_GenerateEgg(xMother, xFather, xRng);

	for (u_int i = 0; i < uL1; ++i)
	{
		ZENITH_ASSERT_EQ((u_int)xEgg.m_aeMoves[i], (u_int)aeL1[i], "L1 move slot %u", i);
	}
	for (u_int i = uL1; i < uZM_MAX_MOVES; ++i)
	{
		ZENITH_ASSERT_EQ((u_int)xEgg.m_aeMoves[i], (u_int)ZM_MOVE_NONE, "no inheritance -> slot %u empty", i);
	}
}

// The 4-move cap is HARD and eviction-free: give the father one more inheritable egg move than
// fits; the overflow move is DROPPED, never displacing an L1 or earlier egg move.
ZENITH_TEST(ZM_Data, Breeding_EggMoveInherit_FourMoveCapNoEviction)
{
	const ZM_SPECIES_ID eOffspring = ZM_GetBaseEvolution(ZM_SPECIES_GRAINMAW);   // NIBBIN
	ZM_MOVE_ID aeL1[uZM_MAX_MOVES];
	const u_int uL1 = ComputeLevelOneFill(eOffspring, aeL1);
	ZENITH_ASSERT_GE(uL1, 1u, "premise: at least one L1 move");
	const u_int uFree = uZM_MAX_MOVES - uL1;   // empty slots after the L1 fill
	ZENITH_ASSERT_GE(uFree, 1u, "premise: some room after L1");

	const ZM_EggMoves xEggMoves = ZM_GetSpeciesEggMoves(eOffspring);
	ZENITH_ASSERT_GT(xEggMoves.m_uCount, uFree, "premise: more inheritable egg moves than empty slots");

	const ZM_BattleMonsterSpec xMother = WithGender(MakeSpecUniform(ZM_SPECIES_GRAINMAW, 20u), ZM_GENDER_FEMALE);
	ZM_BattleMonsterSpec xFather = WithGender(MakeSpecUniform(ZM_SPECIES_STRAYLING, 20u), ZM_GENDER_MALE);
	for (u_int j = 0; j <= uFree; ++j)   // uFree+1 distinct egg moves -> one more than fits
	{
		xFather.m_aeMoves[j] = xEggMoves.m_aeMoves[j];
	}

	ZM_BattleRNG xRng(0xCA9111ull);
	const ZM_BattleMonsterSpec xEgg = ZM_GenerateEgg(xMother, xFather, xRng);

	for (u_int i = 0; i < uL1; ++i)
	{
		ZENITH_ASSERT_EQ((u_int)xEgg.m_aeMoves[i], (u_int)aeL1[i], "L1 slot %u preserved", i);
	}
	for (u_int j = 0; j < uFree; ++j)
	{
		ZENITH_ASSERT_EQ((u_int)xEgg.m_aeMoves[uL1 + j], (u_int)xEggMoves.m_aeMoves[j], "inherited egg move slot %u", uL1 + j);
	}
	for (u_int i = 0; i < uZM_MAX_MOVES; ++i)
	{
		ZENITH_ASSERT_NE((u_int)xEgg.m_aeMoves[i], (u_int)ZM_MOVE_NONE, "all four slots filled (slot %u)", i);
	}
	bool bDropped = true;
	for (u_int i = 0; i < uZM_MAX_MOVES; ++i)
	{
		if (xEgg.m_aeMoves[i] == xEggMoves.m_aeMoves[uFree]) { bDropped = false; }
	}
	ZENITH_ASSERT_TRUE(bDropped, "the overflow egg move must be dropped, not evict an existing move");
}

// Egg-move inheritance is RNG-FREE: at a fixed seed, an empty-moveset father and a father who
// knows an egg move yield byte-identical IV / nature / gender -- only the move slots differ.
ZENITH_TEST(ZM_Data, Breeding_EggMoveInherit_RngFreeDoesNotPerturbStream)
{
	const ZM_SPECIES_ID eOffspring = ZM_GetBaseEvolution(ZM_SPECIES_GRAINMAW);   // NIBBIN
	ZM_MOVE_ID aeL1[uZM_MAX_MOVES];
	const u_int uL1 = ComputeLevelOneFill(eOffspring, aeL1);
	ZENITH_ASSERT_LT(uL1, uZM_MAX_MOVES, "premise: room for an inherited move");
	const ZM_EggMoves xEggMoves = ZM_GetSpeciesEggMoves(eOffspring);
	ZENITH_ASSERT_GE(xEggMoves.m_uCount, 1u, "premise: offspring has egg moves");
	const ZM_MOVE_ID eInherit = xEggMoves.m_aeMoves[0];

	const ZM_BattleMonsterSpec xMother = WithGender(MakeSpecUniform(ZM_SPECIES_GRAINMAW, 20u), ZM_GENDER_FEMALE);
	const ZM_BattleMonsterSpec xFatherEmpty = WithGender(MakeSpecUniform(ZM_SPECIES_STRAYLING, 20u), ZM_GENDER_MALE);
	ZM_BattleMonsterSpec xFatherMoves = xFatherEmpty;
	xFatherMoves.m_aeMoves[0] = eInherit;

	ZM_BattleRNG xRngA(0x5AFE01ull);
	ZM_BattleRNG xRngB(0x5AFE01ull);
	const ZM_BattleMonsterSpec xEggNone = ZM_GenerateEgg(xMother, xFatherEmpty, xRngA);
	const ZM_BattleMonsterSpec xEggMove = ZM_GenerateEgg(xMother, xFatherMoves, xRngB);

	for (u_int i = 0; i < ZM_STAT_COUNT; ++i)
	{
		ZENITH_ASSERT_EQ(xEggNone.m_auIV[i], xEggMove.m_auIV[i], "egg-move inheritance perturbed IV %u", i);
	}
	ZENITH_ASSERT_EQ((u_int)xEggNone.m_eNature, (u_int)xEggMove.m_eNature, "perturbed nature");
	ZENITH_ASSERT_EQ((u_int)xEggNone.m_eGender, (u_int)xEggMove.m_eGender, "perturbed gender");
	ZENITH_ASSERT_EQ((u_int)xEggNone.m_aeMoves[uL1], (u_int)ZM_MOVE_NONE, "empty-parent egg has no inherited move");
	ZENITH_ASSERT_EQ((u_int)xEggMove.m_aeMoves[uL1], (u_int)eInherit, "move-parent egg gains the inherited move");
}

// --- I.5 ZM_GetSpeciesHatchCycles derivation ------------------------------------

// Exact hatch cycles per rarity base (COMMON 10 / UNCOMMON 15 / RARE 25 / LEGENDARY 40) plus
// the size-class nudge; every size premise is pinned via ZM_GetSpeciesSizeClass.
ZENITH_TEST(ZM_Data, Breeding_HatchCycles_PerRarityExactWithSizeNudge)
{
	// COMMON: NIBBIN (QUADRUPED stage 1) is SMALL(1) -> 10 + 1 == 11.
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(ZM_SPECIES_NIBBIN).m_eRarity, (u_int)ZM_RARITY_COMMON, "premise NIBBIN COMMON");
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesSizeClass(ZM_SPECIES_NIBBIN), (u_int)ZM_SIZE_SMALL, "premise NIBBIN SMALL");
	ZENITH_ASSERT_EQ(ZM_GetSpeciesHatchCycles(ZM_SPECIES_NIBBIN), 10u + (u_int)ZM_SIZE_SMALL, "NIBBIN hatch = 10 + size");
	ZENITH_ASSERT_EQ(ZM_GetSpeciesHatchCycles(ZM_SPECIES_NIBBIN), 11u, "NIBBIN hatch == 11");

	// UNCOMMON: SPARKIT (QUADRUPED stage 1) SMALL(1) -> 15 + 1 == 16.
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(ZM_SPECIES_SPARKIT).m_eRarity, (u_int)ZM_RARITY_UNCOMMON, "premise SPARKIT UNCOMMON");
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesSizeClass(ZM_SPECIES_SPARKIT), (u_int)ZM_SIZE_SMALL, "premise SPARKIT SMALL");
	ZENITH_ASSERT_EQ(ZM_GetSpeciesHatchCycles(ZM_SPECIES_SPARKIT), 15u + (u_int)ZM_SIZE_SMALL, "SPARKIT hatch = 15 + size");

	// RARE: WYRMLING (SERPENT stage 1, +1 bulk nudge) is MEDIUM(2) -> 25 + 2 == 27.
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(ZM_SPECIES_WYRMLING).m_eRarity, (u_int)ZM_RARITY_RARE, "premise WYRMLING RARE");
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesSizeClass(ZM_SPECIES_WYRMLING), (u_int)ZM_SIZE_MEDIUM, "premise WYRMLING MEDIUM");
	ZENITH_ASSERT_EQ(ZM_GetSpeciesHatchCycles(ZM_SPECIES_WYRMLING), 25u + (u_int)ZM_SIZE_MEDIUM, "WYRMLING hatch = 25 + size");

	// LEGENDARY: ZENARIS always looms HUGE(4) -> 40 + 4 == 44.
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(ZM_SPECIES_ZENARIS).m_eRarity, (u_int)ZM_RARITY_LEGENDARY, "premise ZENARIS LEGENDARY");
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesSizeClass(ZM_SPECIES_ZENARIS), (u_int)ZM_SIZE_HUGE, "premise ZENARIS HUGE");
	ZENITH_ASSERT_EQ(ZM_GetSpeciesHatchCycles(ZM_SPECIES_ZENARIS), 40u + (u_int)ZM_SIZE_HUGE, "ZENARIS hatch = 40 + size");
	ZENITH_ASSERT_EQ(ZM_GetSpeciesHatchCycles(ZM_SPECIES_ZENARIS), 44u, "ZENARIS hatch == 44");
}

// Legendaries incubate longest: each is 44, and every non-legendary hatches strictly faster
// (max non-legendary is RARE 25 + HUGE 4 == 29 < 44).
ZENITH_TEST(ZM_Data, Breeding_HatchCycles_LegendariesHighest)
{
	const ZM_SPECIES_ID aeLegendary[] = { ZM_SPECIES_ZENARIS, ZM_SPECIES_NADIRATH, ZM_SPECIES_EQUINARA };
	for (u_int i = 0; i < (u_int)(sizeof(aeLegendary) / sizeof(aeLegendary[0])); ++i)
	{
		ZENITH_ASSERT_EQ(ZM_GetSpeciesHatchCycles(aeLegendary[i]), 44u, "legendary hatch == 40 + HUGE (%u)", i);
	}
	const u_int uLegendaryCycles = ZM_GetSpeciesHatchCycles(ZM_SPECIES_ZENARIS);
	const u_int uCount = ZM_GetSpeciesCount();
	for (u_int s = 0; s < uCount; ++s)
	{
		const ZM_SPECIES_ID eId = (ZM_SPECIES_ID)s;
		if (ZM_GetSpeciesData(eId).m_eRarity == ZM_RARITY_LEGENDARY) { continue; }
		ZENITH_ASSERT_LT(ZM_GetSpeciesHatchCycles(eId), uLegendaryCycles,
			"%s must hatch faster than a legendary", ZM_GetSpeciesName(eId));
	}
}
