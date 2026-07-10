#include "Zenith.h"

// ============================================================================
// ZM_Tests_DataRegistry -- the cross-table schema enforcer (category ZM_Data),
// the suite that CLOSES S1. It locks (a) that every name round-trips through the
// ZM_FindXByName lookups and unknown/null names yield NONE, and (b) that the
// tables are MUTUALLY consistent: every evolution target, TM move, wild-encounter
// species, and derived learnset move resolves to a real row. See ZM-D-030.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"
#include "Zenithmon/Source/Data/ZM_MoveData.h"
#include "Zenithmon/Source/Data/ZM_ItemData.h"
#include "Zenithmon/Source/Data/ZM_AbilityData.h"
#include "Zenithmon/Source/Data/ZM_NatureData.h"
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"
#include "Zenithmon/Source/Data/ZM_Learnsets.h"
#include "Zenithmon/Source/Data/ZM_DataRegistry.h"

// Every species name resolves back to its own id.
ZENITH_TEST(ZM_Data, Registry_SpeciesNameRoundTrip)
{
	for (u_int i = 0; i < ZM_GetSpeciesCount(); ++i)
	{
		ZENITH_ASSERT_EQ((u_int)ZM_FindSpeciesByName(ZM_GetSpeciesName((ZM_SPECIES_ID)i)), i,
			"species name '%s' did not round-trip", ZM_GetSpeciesName((ZM_SPECIES_ID)i));
	}
}

ZENITH_TEST(ZM_Data, Registry_MoveNameRoundTrip)
{
	for (u_int i = 0; i < ZM_GetMoveCount(); ++i)
	{
		ZENITH_ASSERT_EQ((u_int)ZM_FindMoveByName(ZM_GetMoveName((ZM_MOVE_ID)i)), i,
			"move name '%s' did not round-trip", ZM_GetMoveName((ZM_MOVE_ID)i));
	}
}

ZENITH_TEST(ZM_Data, Registry_ItemNameRoundTrip)
{
	for (u_int i = 0; i < ZM_GetItemCount(); ++i)
	{
		ZENITH_ASSERT_EQ((u_int)ZM_FindItemByName(ZM_GetItemName((ZM_ITEM_ID)i)), i,
			"item name '%s' did not round-trip", ZM_GetItemName((ZM_ITEM_ID)i));
	}
}

ZENITH_TEST(ZM_Data, Registry_AbilityNameRoundTrip)
{
	for (u_int i = 0; i < ZM_GetAbilityCount(); ++i)
	{
		ZENITH_ASSERT_EQ((u_int)ZM_FindAbilityByName(ZM_GetAbilityName((ZM_ABILITY_ID)i)), i,
			"ability name '%s' did not round-trip", ZM_GetAbilityName((ZM_ABILITY_ID)i));
	}
}

ZENITH_TEST(ZM_Data, Registry_NatureNameRoundTrip)
{
	for (u_int i = 0; i < ZM_GetNatureCount(); ++i)
	{
		ZENITH_ASSERT_EQ((u_int)ZM_FindNatureByName(ZM_GetNatureName((ZM_NATURE)i)), i,
			"nature name '%s' did not round-trip", ZM_GetNatureName((ZM_NATURE)i));
	}
}

ZENITH_TEST(ZM_Data, Registry_SceneNameRoundTrip)
{
	for (u_int i = 0; i < ZM_GetSceneCount(); ++i)
	{
		ZENITH_ASSERT_EQ((u_int)ZM_FindSceneByName(ZM_GetSceneName((ZM_SCENE_ID)i)), i,
			"scene name '%s' did not round-trip", ZM_GetSceneName((ZM_SCENE_ID)i));
	}
}

// Unknown, null, and empty names resolve to each table's NONE sentinel.
ZENITH_TEST(ZM_Data, Registry_UnknownAndNullReturnNone)
{
	const char* szBogus = "ThisIsDefinitelyNotARealZenithmonName";
	ZENITH_ASSERT_EQ((u_int)ZM_FindSpeciesByName(szBogus), (u_int)ZM_SPECIES_NONE);
	ZENITH_ASSERT_EQ((u_int)ZM_FindMoveByName(szBogus), (u_int)ZM_MOVE_NONE);
	ZENITH_ASSERT_EQ((u_int)ZM_FindItemByName(szBogus), (u_int)ZM_ITEM_NONE);
	ZENITH_ASSERT_EQ((u_int)ZM_FindAbilityByName(szBogus), (u_int)ZM_ABILITY_NONE);
	ZENITH_ASSERT_EQ((u_int)ZM_FindNatureByName(szBogus), (u_int)ZM_NATURE_COUNT);
	ZENITH_ASSERT_EQ((u_int)ZM_FindSceneByName(szBogus), (u_int)ZM_SCENE_NONE);

	ZENITH_ASSERT_EQ((u_int)ZM_FindSpeciesByName(nullptr), (u_int)ZM_SPECIES_NONE);
	ZENITH_ASSERT_EQ((u_int)ZM_FindMoveByName(""), (u_int)ZM_MOVE_NONE);
	ZENITH_ASSERT_EQ((u_int)ZM_FindItemByName(nullptr), (u_int)ZM_ITEM_NONE);
	ZENITH_ASSERT_EQ((u_int)ZM_FindAbilityByName(""), (u_int)ZM_ABILITY_NONE);
	ZENITH_ASSERT_EQ((u_int)ZM_FindNatureByName(nullptr), (u_int)ZM_NATURE_COUNT);
	ZENITH_ASSERT_EQ((u_int)ZM_FindSceneByName(""), (u_int)ZM_SCENE_NONE);
}

// Cross-table: every evolution target and every TM's taught move resolves.
ZENITH_TEST(ZM_Data, Registry_EvolutionAndTmRefsResolve)
{
	for (u_int i = 0; i < ZM_GetSpeciesCount(); ++i)
	{
		const ZM_SpeciesData& x = ZM_GetSpeciesData((ZM_SPECIES_ID)i);
		if (x.m_eEvolvesTo != ZM_SPECIES_NONE)
		{
			ZENITH_ASSERT_EQ((u_int)ZM_FindSpeciesByName(ZM_GetSpeciesName(x.m_eEvolvesTo)), (u_int)x.m_eEvolvesTo,
				"%s evolves to an unresolvable species", x.m_szName);
		}
	}
	for (u_int i = 0; i < ZM_GetItemCount(); ++i)
	{
		const ZM_ItemData& x = ZM_GetItemData((ZM_ITEM_ID)i);
		if (x.m_eCategory == ZM_ITEM_CATEGORY_TM)
		{
			ZENITH_ASSERT_TRUE(x.m_eTaughtMove < ZM_MOVE_COUNT, "%s teaches a non-move", x.m_szName);
			ZENITH_ASSERT_EQ((u_int)ZM_FindMoveByName(ZM_GetMoveName(x.m_eTaughtMove)), (u_int)x.m_eTaughtMove,
				"%s teaches an unresolvable move", x.m_szName);
		}
	}
}

// Cross-table: every wild-encounter species and every derived learnset move resolves.
ZENITH_TEST(ZM_Data, Registry_EncounterAndLearnsetRefsResolve)
{
	for (u_int s = 0; s < ZM_GetSceneCount(); ++s)
	{
		const ZM_WorldSpec& x = ZM_GetWorldSpec((ZM_SCENE_ID)s);
		for (u_int e = 0; e < x.m_uEncounterCount; ++e)
		{
			const ZM_SPECIES_ID eSp = x.m_pxEncounters[e].m_eSpecies;
			ZENITH_ASSERT_EQ((u_int)ZM_FindSpeciesByName(ZM_GetSpeciesName(eSp)), (u_int)eSp,
				"%s has an unresolvable encounter species", x.m_szName);
		}
	}
	for (u_int i = 0; i < ZM_GetSpeciesCount(); ++i)
	{
		const ZM_Learnset xLs = ZM_GetSpeciesLearnset((ZM_SPECIES_ID)i);
		for (u_int k = 0; k < xLs.m_uCount; ++k)
		{
			ZENITH_ASSERT_TRUE(xLs.m_axMoves[k].m_eMove < ZM_MOVE_COUNT,
				"%s learnset references a non-move", ZM_GetSpeciesName((ZM_SPECIES_ID)i));
		}
	}
}
