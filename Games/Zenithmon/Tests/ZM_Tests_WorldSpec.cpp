#include "Zenith.h"

// ============================================================================
// ZM_Tests_WorldSpec -- referential integrity of the world table skeleton
// (category ZM_Data). This is the schema enforcer that keeps the world honest as
// scenes are appended (S9/S10): every warp target + spawn tag + encounter species
// resolves, build indices are unique + anchored (FrontEnd 0, Battle 1), and every
// non-Battle scene is reachable from FrontEnd. See DecisionLog ZM-D-029.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"

#include <cstring>

namespace
{
	const ZM_WorldSpec& Sc(u_int i) { return ZM_GetWorldSpec((ZM_SCENE_ID)i); }

	bool IsOutdoor(ZM_SCENE_KIND e) { return e == ZM_SCENE_KIND_TOWN || e == ZM_SCENE_KIND_ROUTE; }

	bool SceneOffersTag(const ZM_WorldSpec& x, const char* szTag)
	{
		for (u_int i = 0; i < x.m_uSpawnTagCount; ++i)
		{
			if (strcmp(x.m_pszSpawnTags[i], szTag) == 0) { return true; }
		}
		return false;
	}
}

ZENITH_TEST(ZM_Data, WorldSpec_CountAndSelfConsistent)
{
	ZENITH_ASSERT_EQ((u_int)ZM_SCENE_NONE, (u_int)ZM_SCENE_COUNT);
	for (u_int i = 0; i < ZM_SCENE_COUNT; ++i)
	{
		ZENITH_ASSERT_EQ((u_int)Sc(i).m_eId, i, "scene row %u has mismatched m_eId", i);
	}
}

ZENITH_TEST(ZM_Data, WorldSpec_NamesUniqueNonEmpty)
{
	for (u_int i = 0; i < ZM_SCENE_COUNT; ++i)
	{
		const char* szA = Sc(i).m_szName;
		ZENITH_ASSERT_NOT_NULL(szA);
		ZENITH_ASSERT_TRUE(szA[0] != '\0', "scene %u has an empty name", i);
		for (u_int j = i + 1; j < ZM_SCENE_COUNT; ++j)
		{
			ZENITH_ASSERT_FALSE(strcmp(szA, Sc(j).m_szName) == 0, "duplicate scene name '%s'", szA);
		}
	}
}

// Build indices are unique; FrontEnd is 0 and Battle is 1 (ZM-D-012).
ZENITH_TEST(ZM_Data, WorldSpec_BuildIndicesUniqueAndAnchored)
{
	for (u_int i = 0; i < ZM_SCENE_COUNT; ++i)
	{
		for (u_int j = i + 1; j < ZM_SCENE_COUNT; ++j)
		{
			ZENITH_ASSERT_NE(Sc(i).m_uBuildIndex, Sc(j).m_uBuildIndex,
				"scenes %s and %s share build index %u", Sc(i).m_szName, Sc(j).m_szName, Sc(i).m_uBuildIndex);
		}
	}
	ZENITH_ASSERT_EQ(ZM_GetWorldSpec(ZM_SCENE_FRONTEND).m_uBuildIndex, 0u, "FrontEnd is not build index 0");
	ZENITH_ASSERT_EQ(ZM_GetWorldSpec(ZM_SCENE_BATTLE).m_uBuildIndex, 1u, "Battle is not build index 1");
	ZENITH_ASSERT_EQ((u_int)ZM_GetWorldSpec(ZM_SCENE_FRONTEND).m_eKind, (u_int)ZM_SCENE_KIND_FRONTEND);
	ZENITH_ASSERT_EQ((u_int)ZM_GetWorldSpec(ZM_SCENE_BATTLE).m_eKind, (u_int)ZM_SCENE_KIND_BATTLE);
}

ZENITH_TEST(ZM_Data, WorldSpec_KindsValid)
{
	for (u_int i = 0; i < ZM_SCENE_COUNT; ++i)
	{
		ZENITH_ASSERT_TRUE(Sc(i).m_eKind < ZM_SCENE_KIND_COUNT, "%s bad kind", Sc(i).m_szName);
	}
}

// Outdoor scenes (town/route) name a terrain set; indoor/battle/frontend do not.
ZENITH_TEST(ZM_Data, WorldSpec_TerrainByKind)
{
	for (u_int i = 0; i < ZM_SCENE_COUNT; ++i)
	{
		const ZM_WorldSpec& x = Sc(i);
		ZENITH_ASSERT_NOT_NULL(x.m_szTerrainSet);
		const bool bHasTerrain = (x.m_szTerrainSet[0] != '\0');
		if (IsOutdoor(x.m_eKind))
		{
			ZENITH_ASSERT_TRUE(bHasTerrain, "%s (outdoor) has no terrain set", x.m_szName);
		}
		else
		{
			ZENITH_ASSERT_FALSE(bHasTerrain, "%s (indoor) has a terrain set", x.m_szName);
		}
	}
}

// Spawn tags are non-empty and unique within a scene.
ZENITH_TEST(ZM_Data, WorldSpec_SpawnTagsNonEmptyUnique)
{
	for (u_int i = 0; i < ZM_SCENE_COUNT; ++i)
	{
		const ZM_WorldSpec& x = Sc(i);
		for (u_int a = 0; a < x.m_uSpawnTagCount; ++a)
		{
			ZENITH_ASSERT_NOT_NULL(x.m_pszSpawnTags[a]);
			ZENITH_ASSERT_TRUE(x.m_pszSpawnTags[a][0] != '\0', "%s has an empty spawn tag", x.m_szName);
			for (u_int b = a + 1; b < x.m_uSpawnTagCount; ++b)
			{
				ZENITH_ASSERT_FALSE(strcmp(x.m_pszSpawnTags[a], x.m_pszSpawnTags[b]) == 0,
					"%s repeats spawn tag '%s'", x.m_szName, x.m_pszSpawnTags[a]);
			}
		}
	}
}

// Every connection targets a real scene and a spawn tag that scene actually offers.
ZENITH_TEST(ZM_Data, WorldSpec_ConnectionsResolve)
{
	for (u_int i = 0; i < ZM_SCENE_COUNT; ++i)
	{
		const ZM_WorldSpec& x = Sc(i);
		for (u_int c = 0; c < x.m_uConnectionCount; ++c)
		{
			const ZM_SceneConnection& xConn = x.m_pxConnections[c];
			ZENITH_ASSERT_TRUE(xConn.m_eTarget < ZM_SCENE_COUNT, "%s connects to a non-scene", x.m_szName);
			ZENITH_ASSERT_NOT_NULL(xConn.m_szSpawnTag);
			ZENITH_ASSERT_TRUE(SceneOffersTag(ZM_GetWorldSpec(xConn.m_eTarget), xConn.m_szSpawnTag),
				"%s -> %s targets undeclared spawn tag '%s'", x.m_szName,
				ZM_GetSceneName(xConn.m_eTarget), xConn.m_szSpawnTag);
		}
	}
}

// Encounters live only on routes; each slot references a real species, a valid
// level band, and a positive weight.
ZENITH_TEST(ZM_Data, WorldSpec_EncountersValid)
{
	for (u_int i = 0; i < ZM_SCENE_COUNT; ++i)
	{
		const ZM_WorldSpec& x = Sc(i);
		if (x.m_uEncounterCount > 0)
		{
			ZENITH_ASSERT_EQ((u_int)x.m_eKind, (u_int)ZM_SCENE_KIND_ROUTE, "%s has encounters but is not a route", x.m_szName);
		}
		for (u_int e = 0; e < x.m_uEncounterCount; ++e)
		{
			const ZM_EncounterSlot& xEnc = x.m_pxEncounters[e];
			ZENITH_ASSERT_TRUE(xEnc.m_eSpecies < ZM_SPECIES_COUNT, "%s encounter is a non-species", x.m_szName);
			ZENITH_ASSERT_GE(xEnc.m_uMinLevel, 1u, "%s encounter min level < 1", x.m_szName);
			ZENITH_ASSERT_LE(xEnc.m_uMaxLevel, 100u, "%s encounter max level > 100", x.m_szName);
			ZENITH_ASSERT_LE(xEnc.m_uMinLevel, xEnc.m_uMaxLevel, "%s encounter min > max", x.m_szName);
			ZENITH_ASSERT_GT(xEnc.m_uWeight, 0u, "%s encounter has 0 weight", x.m_szName);
		}
	}
}

// Every non-Battle scene is reachable from FrontEnd by walking connections
// (Battle is loaded additively, never warped to).
ZENITH_TEST(ZM_Data, WorldSpec_AllReachableFromFrontEnd)
{
	bool abVisited[ZM_SCENE_COUNT] = { false };
	ZM_SCENE_ID aeStack[ZM_SCENE_COUNT * 8];
	u_int uSp = 0;
	aeStack[uSp++] = ZM_SCENE_FRONTEND;
	while (uSp > 0)
	{
		const ZM_SCENE_ID eCur = aeStack[--uSp];
		if (abVisited[eCur]) { continue; }
		abVisited[eCur] = true;
		const ZM_WorldSpec& x = ZM_GetWorldSpec(eCur);
		for (u_int c = 0; c < x.m_uConnectionCount; ++c)
		{
			const ZM_SCENE_ID eTgt = x.m_pxConnections[c].m_eTarget;
			if (!abVisited[eTgt]) { aeStack[uSp++] = eTgt; }
		}
	}
	for (u_int i = 0; i < ZM_SCENE_COUNT; ++i)
	{
		if ((ZM_SCENE_ID)i == ZM_SCENE_BATTLE) { continue; }
		ZENITH_ASSERT_TRUE(abVisited[i], "%s is unreachable from FrontEnd", Sc(i).m_szName);
	}
}

// ZM_FindSceneByBuildIndex round-trips every scene and rejects unknown indices.
ZENITH_TEST(ZM_Data, WorldSpec_FindByBuildIndex)
{
	for (u_int i = 0; i < ZM_SCENE_COUNT; ++i)
	{
		ZENITH_ASSERT_EQ((u_int)ZM_FindSceneByBuildIndex(Sc(i).m_uBuildIndex), i,
			"build index %u did not resolve back to %s", Sc(i).m_uBuildIndex, Sc(i).m_szName);
	}
	ZENITH_ASSERT_EQ((u_int)ZM_FindSceneByBuildIndex(9999u), (u_int)ZM_SCENE_NONE, "unknown index resolved");
}

// The new per-route encounter rate column (S5): a ROUTE that carries encounter
// slots must have a positive rate; every other row (non-route, or a route with no
// slots) must be 0; and no rate exceeds the /256 gate ceiling.
ZENITH_TEST(ZM_Data, WorldSpec_EncounterRateColumn)
{
	for (u_int i = 0; i < ZM_SCENE_COUNT; ++i)
	{
		const ZM_WorldSpec& x = Sc(i);
		const bool bRouteWithSlots = (x.m_eKind == ZM_SCENE_KIND_ROUTE) && (x.m_uEncounterCount > 0);
		if (bRouteWithSlots)
		{
			ZENITH_ASSERT_GT(x.m_uEncounterRatePer256, 0u,
				"%s is a route with slots but has a 0 encounter rate", x.m_szName);
		}
		else
		{
			ZENITH_ASSERT_EQ(x.m_uEncounterRatePer256, 0u,
				"%s must have a 0 encounter rate (not a route with slots)", x.m_szName);
		}
		ZENITH_ASSERT_LE(x.m_uEncounterRatePer256, 256u,
			"%s encounter rate exceeds the /256 gate ceiling", x.m_szName);
	}
}

ZENITH_TEST(ZM_Data, WorldSpec_AccessorAndToString)
{
	for (u_int i = 0; i < ZM_SCENE_COUNT; ++i)
	{
		ZENITH_ASSERT_STREQ(ZM_GetSceneName((ZM_SCENE_ID)i), Sc(i).m_szName);
	}
	ZENITH_ASSERT_STREQ(ZM_GetSceneName(ZM_SCENE_NONE), "NONE");

	for (u_int k = 0; k < ZM_SCENE_KIND_COUNT; ++k)
	{
		const char* sz = ZM_SceneKindToString((ZM_SCENE_KIND)k);
		ZENITH_ASSERT_NOT_NULL(sz);
		ZENITH_ASSERT_FALSE(strcmp(sz, "INVALID") == 0, "scene kind %u stringifies as INVALID", k);
	}
	ZENITH_ASSERT_STREQ(ZM_SceneKindToString(ZM_SCENE_KIND_COUNT), "INVALID");
}
