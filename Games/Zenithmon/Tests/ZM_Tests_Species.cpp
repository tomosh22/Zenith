#include "Zenith.h"

// ============================================================================
// ZM_Tests_Species -- structural integrity of the species dex (category
// ZM_Data). This is the schema enforcer for the 152-species roster: it pins the
// counts from Docs/GameDesignDocument.md section 5, the evolution graph shape,
// and per-family consistency. Base-stat / learnset tests join as those fields
// land on the same Roadmap box.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Source/Data/ZM_Types.h"
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"

#include <cstring>   // strcmp

namespace
{
	constexpr u_int uEXPECTED_SPECIES = 152;
	constexpr u_int uEXPECTED_FAMILIES = 59;   // 56 F-families + 3 legendaries
	constexpr u_int uMAX_FAMILY_ID = 60;       // sizing headroom (ids run 1..59)

	const ZM_SpeciesData& Sp(u_int uIndex)
	{
		return ZM_GetSpeciesData((ZM_SPECIES_ID)uIndex);
	}
}

// The roster is exactly 152 species and the table is index-consistent.
ZENITH_TEST(ZM_Data, Species_CountAndSelfConsistent)
{
	ZENITH_ASSERT_EQ(ZM_GetSpeciesCount(), uEXPECTED_SPECIES);
	ZENITH_ASSERT_EQ((u_int)ZM_SPECIES_COUNT, uEXPECTED_SPECIES);

	for (u_int i = 0; i < ZM_SPECIES_COUNT; ++i)
	{
		ZENITH_ASSERT_EQ((u_int)Sp(i).m_eId, i, "species row %u has mismatched m_eId", i);
	}
}

// Every name is present and unique.
ZENITH_TEST(ZM_Data, Species_NamesUniqueNonEmpty)
{
	for (u_int i = 0; i < ZM_SPECIES_COUNT; ++i)
	{
		const char* szA = Sp(i).m_szName;
		ZENITH_ASSERT_NOT_NULL(szA);
		ZENITH_ASSERT_TRUE(szA[0] != '\0', "species %u has an empty name", i);
		for (u_int j = i + 1; j < ZM_SPECIES_COUNT; ++j)
		{
			ZENITH_ASSERT_FALSE(strcmp(szA, Sp(j).m_szName) == 0,
				"duplicate species name '%s' at %u and %u", szA, i, j);
		}
	}
}

// Types: primary is a real type; secondary is real or NONE; never doubled.
ZENITH_TEST(ZM_Data, Species_TypesValid)
{
	for (u_int i = 0; i < ZM_SPECIES_COUNT; ++i)
	{
		const ZM_SpeciesData& x = Sp(i);
		ZENITH_ASSERT_TRUE(x.m_aeTypes[0] < ZM_TYPE_COUNT,
			"%s primary type is not a real type", x.m_szName);
		ZENITH_ASSERT_TRUE(x.m_aeTypes[1] < ZM_TYPE_COUNT || x.m_aeTypes[1] == ZM_TYPE_NONE,
			"%s secondary type out of range", x.m_szName);
		if (x.m_aeTypes[1] != ZM_TYPE_NONE)
		{
			ZENITH_ASSERT_NE(x.m_aeTypes[0], x.m_aeTypes[1],
				"%s lists the same type twice", x.m_szName);
		}
	}
}

// Evolution stage is 1..3; the graph is well-formed: a non-final species points
// to a real next-stage species in the SAME family (stage+1, same archetype and
// rarity); a final species points to NONE.
ZENITH_TEST(ZM_Data, Species_EvolutionGraphValid)
{
	for (u_int i = 0; i < ZM_SPECIES_COUNT; ++i)
	{
		const ZM_SpeciesData& x = Sp(i);
		ZENITH_ASSERT_TRUE(x.m_uEvoStage >= 1 && x.m_uEvoStage <= 3,
			"%s has out-of-range evo stage %u", x.m_szName, x.m_uEvoStage);

		if (x.m_eEvolvesTo == ZM_SPECIES_NONE)
		{
			continue;
		}
		ZENITH_ASSERT_TRUE(x.m_eEvolvesTo < ZM_SPECIES_COUNT,
			"%s evolves to an invalid id", x.m_szName);
		ZENITH_ASSERT_NE((u_int)x.m_eEvolvesTo, i, "%s evolves into itself", x.m_szName);

		const ZM_SpeciesData& xNext = ZM_GetSpeciesData(x.m_eEvolvesTo);
		ZENITH_ASSERT_EQ(xNext.m_uEvoStage, x.m_uEvoStage + 1,
			"%s -> %s must be exactly one stage up", x.m_szName, xNext.m_szName);
		ZENITH_ASSERT_EQ(xNext.m_uFamilyId, x.m_uFamilyId,
			"%s evolves outside its family", x.m_szName);
		ZENITH_ASSERT_EQ((u_int)xNext.m_eArchetype, (u_int)x.m_eArchetype,
			"%s changes archetype on evolution", x.m_szName);
		ZENITH_ASSERT_EQ((u_int)xNext.m_eRarity, (u_int)x.m_eRarity,
			"%s changes rarity on evolution", x.m_szName);
	}
}

// Family ids are in range; each family is a linear chain (one species per stage,
// stages 1..maxStage with no gaps) sharing one archetype and rarity.
ZENITH_TEST(ZM_Data, Species_FamiliesWellFormed)
{
	u_int auCount[uMAX_FAMILY_ID] = {};
	u_int auMaxStage[uMAX_FAMILY_ID] = {};
	u_int auStageBits[uMAX_FAMILY_ID] = {};   // bit s set => a stage-s member exists
	u_int aeArch[uMAX_FAMILY_ID] = {};
	u_int aeRar[uMAX_FAMILY_ID] = {};

	for (u_int i = 0; i < ZM_SPECIES_COUNT; ++i)
	{
		const ZM_SpeciesData& x = Sp(i);
		ZENITH_ASSERT_TRUE(x.m_uFamilyId >= 1 && x.m_uFamilyId < uMAX_FAMILY_ID,
			"%s has out-of-range family id %u", x.m_szName, x.m_uFamilyId);
		const u_int f = x.m_uFamilyId;

		if (auCount[f] == 0)
		{
			aeArch[f] = (u_int)x.m_eArchetype;
			aeRar[f] = (u_int)x.m_eRarity;
		}
		else
		{
			ZENITH_ASSERT_EQ((u_int)x.m_eArchetype, aeArch[f],
				"family %u mixes archetypes (at %s)", f, x.m_szName);
			ZENITH_ASSERT_EQ((u_int)x.m_eRarity, aeRar[f],
				"family %u mixes rarities (at %s)", f, x.m_szName);
		}
		auCount[f]++;
		if (x.m_uEvoStage > auMaxStage[f]) { auMaxStage[f] = x.m_uEvoStage; }
		const u_int uBit = 1u << x.m_uEvoStage;
		ZENITH_ASSERT_TRUE((auStageBits[f] & uBit) == 0,
			"family %u has two species at stage %u (at %s)", f, x.m_uEvoStage, x.m_szName);
		auStageBits[f] |= uBit;
	}

	u_int uFamilies = 0;
	for (u_int f = 1; f < uMAX_FAMILY_ID; ++f)
	{
		if (auCount[f] == 0) { continue; }
		uFamilies++;
		// Species count equals the max stage (linear 1..maxStage chain).
		ZENITH_ASSERT_EQ(auCount[f], auMaxStage[f],
			"family %u has %u members but max stage %u", f, auCount[f], auMaxStage[f]);
		// Stages 1..maxStage all present (no gaps).
		u_int uExpectBits = 0;
		for (u_int s = 1; s <= auMaxStage[f]; ++s) { uExpectBits |= (1u << s); }
		ZENITH_ASSERT_EQ(auStageBits[f], uExpectBits, "family %u has a stage gap", f);
	}
	ZENITH_ASSERT_EQ(uFamilies, uEXPECTED_FAMILIES);
}

// The family-size distribution matches the GDD count: 40 three-stage,
// 13 two-stage, 6 single-stage (3 rares + 3 legendaries).
ZENITH_TEST(ZM_Data, Species_FamilySizeDistribution)
{
	u_int auMaxStage[uMAX_FAMILY_ID] = {};
	for (u_int i = 0; i < ZM_SPECIES_COUNT; ++i)
	{
		const ZM_SpeciesData& x = Sp(i);
		if (x.m_uEvoStage > auMaxStage[x.m_uFamilyId]) { auMaxStage[x.m_uFamilyId] = x.m_uEvoStage; }
	}
	u_int uThree = 0, uTwo = 0, uOne = 0;
	for (u_int f = 1; f < uMAX_FAMILY_ID; ++f)
	{
		switch (auMaxStage[f])
		{
		case 3: uThree++; break;
		case 2: uTwo++; break;
		case 1: uOne++; break;
		default: break;
		}
	}
	ZENITH_ASSERT_EQ(uThree, 40u, "expected 40 three-stage families");
	ZENITH_ASSERT_EQ(uTwo, 13u, "expected 13 two-stage families");
	ZENITH_ASSERT_EQ(uOne, 6u, "expected 6 single-stage families (3 rares + 3 legendaries)");
}

// One base (stage-1) species and one final (evolves-to-NONE) per family, and
// exactly three legendaries, each a single-stage species.
ZENITH_TEST(ZM_Data, Species_BasesFinalsAndLegendaries)
{
	u_int uBases = 0, uFinals = 0, uLegendary = 0;
	for (u_int i = 0; i < ZM_SPECIES_COUNT; ++i)
	{
		const ZM_SpeciesData& x = Sp(i);
		if (x.m_uEvoStage == 1) { uBases++; }
		if (x.m_eEvolvesTo == ZM_SPECIES_NONE) { uFinals++; }
		if (x.m_eRarity == ZM_RARITY_LEGENDARY)
		{
			uLegendary++;
			ZENITH_ASSERT_EQ(x.m_uEvoStage, 1u, "%s (legendary) must be single-stage", x.m_szName);
			ZENITH_ASSERT_EQ((u_int)x.m_eEvolvesTo, (u_int)ZM_SPECIES_NONE,
				"%s (legendary) must not evolve", x.m_szName);
		}
	}
	ZENITH_ASSERT_EQ(uBases, uEXPECTED_FAMILIES, "one stage-1 base per family");
	ZENITH_ASSERT_EQ(uFinals, uEXPECTED_FAMILIES, "one final stage per family");
	ZENITH_ASSERT_EQ(uLegendary, 3u);
}

// Archetype spread by FAMILY matches the GDD (section 5 tally, incl. legendaries).
ZENITH_TEST(ZM_Data, Species_ArchetypeSpreadMatchesGdd)
{
	// bit f set in seen[arch] => family f has that archetype (families are
	// single-archetype, verified by Species_FamiliesWellFormed).
	u_int auFamCountByArch[ZM_ARCHETYPE_COUNT] = {};
	bool aabSeen[ZM_ARCHETYPE_COUNT][uMAX_FAMILY_ID] = {};
	for (u_int i = 0; i < ZM_SPECIES_COUNT; ++i)
	{
		const ZM_SpeciesData& x = Sp(i);
		if (!aabSeen[x.m_eArchetype][x.m_uFamilyId])
		{
			aabSeen[x.m_eArchetype][x.m_uFamilyId] = true;
			auFamCountByArch[x.m_eArchetype]++;
		}
	}
	ZENITH_ASSERT_EQ(auFamCountByArch[ZM_ARCHETYPE_QUADRUPED],        18u);
	ZENITH_ASSERT_EQ(auFamCountByArch[ZM_ARCHETYPE_BIPED],            6u);
	ZENITH_ASSERT_EQ(auFamCountByArch[ZM_ARCHETYPE_AVIAN],           7u);
	ZENITH_ASSERT_EQ(auFamCountByArch[ZM_ARCHETYPE_SERPENT],         4u);
	ZENITH_ASSERT_EQ(auFamCountByArch[ZM_ARCHETYPE_AQUATIC],         6u);
	ZENITH_ASSERT_EQ(auFamCountByArch[ZM_ARCHETYPE_INSECTOID],       7u);
	ZENITH_ASSERT_EQ(auFamCountByArch[ZM_ARCHETYPE_BLOB],            5u);
	ZENITH_ASSERT_EQ(auFamCountByArch[ZM_ARCHETYPE_FLOATER_PLANTOID], 6u);
}

// All 18 types appear on at least two families (GDD 5 invariant).
ZENITH_TEST(ZM_Data, Species_EveryTypeOnTwoFamilies)
{
	bool aabTypeInFamily[ZM_TYPE_COUNT][uMAX_FAMILY_ID] = {};
	for (u_int i = 0; i < ZM_SPECIES_COUNT; ++i)
	{
		const ZM_SpeciesData& x = Sp(i);
		aabTypeInFamily[x.m_aeTypes[0]][x.m_uFamilyId] = true;
		if (x.m_aeTypes[1] != ZM_TYPE_NONE)
		{
			aabTypeInFamily[x.m_aeTypes[1]][x.m_uFamilyId] = true;
		}
	}
	for (u_int t = 0; t < ZM_TYPE_COUNT; ++t)
	{
		u_int uFams = 0;
		for (u_int f = 1; f < uMAX_FAMILY_ID; ++f) { if (aabTypeInFamily[t][f]) { uFams++; } }
		ZENITH_ASSERT_GE(uFams, 2u, "type %s appears on fewer than two families", ZM_TypeToString((ZM_TYPE)t));
	}
}

// Derived family seed is shared within a family and unique across families.
ZENITH_TEST(ZM_Data, Species_FamilySeedConsistentAndUnique)
{
	u_int auSeed[uMAX_FAMILY_ID] = {};
	bool  abHaveSeed[uMAX_FAMILY_ID] = {};
	for (u_int i = 0; i < ZM_SPECIES_COUNT; ++i)
	{
		const ZM_SpeciesData& x = Sp(i);
		const u_int uSeed = ZM_GetSpeciesFamilySeed(x.m_eId);
		if (!abHaveSeed[x.m_uFamilyId])
		{
			abHaveSeed[x.m_uFamilyId] = true;
			auSeed[x.m_uFamilyId] = uSeed;
		}
		else
		{
			ZENITH_ASSERT_EQ(uSeed, auSeed[x.m_uFamilyId],
				"%s has a different family seed from its family", x.m_szName);
		}
	}
	for (u_int a = 1; a < uMAX_FAMILY_ID; ++a)
	{
		if (!abHaveSeed[a]) { continue; }
		for (u_int b = a + 1; b < uMAX_FAMILY_ID; ++b)
		{
			if (!abHaveSeed[b]) { continue; }
			ZENITH_ASSERT_NE(auSeed[a], auSeed[b], "families %u and %u share a seed", a, b);
		}
	}
}

// Derived size class never shrinks as a species evolves.
ZENITH_TEST(ZM_Data, Species_SizeClassNonDecreasing)
{
	for (u_int i = 0; i < ZM_SPECIES_COUNT; ++i)
	{
		const ZM_SpeciesData& x = Sp(i);
		if (x.m_eEvolvesTo == ZM_SPECIES_NONE) { continue; }
		const ZM_SIZE_CLASS eHere = ZM_GetSpeciesSizeClass(x.m_eId);
		const ZM_SIZE_CLASS eNext = ZM_GetSpeciesSizeClass(x.m_eEvolvesTo);
		ZENITH_ASSERT_LE((u_int)eHere, (u_int)eNext,
			"%s shrinks on evolution", x.m_szName);
	}
}
