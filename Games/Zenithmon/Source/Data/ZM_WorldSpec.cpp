#include "Zenith.h"
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"

// ============================================================================
// ZM_WorldSpec -- the world table skeleton (DecisionLog ZM-D-029). Rows are in
// ZM_SCENE_ID order; s_axScenes[i].m_eId == i is asserted by the tests. Per-scene
// connection / spawn-tag / encounter arrays are static and referenced by
// pointer + count. Referential integrity (every warp target + spawn tag +
// encounter species resolves, all scenes reachable from FrontEnd) is locked by
// Tests/ZM_Tests_WorldSpec.cpp.
// ============================================================================

namespace
{
#define ZM_ARRLEN(a) ((u_int)(sizeof(a) / sizeof((a)[0])))

	// -- warp connections (target scene + the spawn tag reached there) --
	const ZM_SceneConnection s_axConnFrontEnd[]  = { { ZM_SCENE_DAWNMERE, "TownCenter" } };
	const ZM_SceneConnection s_axConnDawnmere[]  = { { ZM_SCENE_ROUTE1, "FromDawnmere" }, { ZM_SCENE_PLAYERHOME, "Door" }, { ZM_SCENE_PROFLAB, "Door" } };
	const ZM_SceneConnection s_axConnThornacre[] = { { ZM_SCENE_ROUTE1, "FromThornacre" }, { ZM_SCENE_GYM1, "Door" } };
	const ZM_SceneConnection s_axConnRoute1[]    = { { ZM_SCENE_DAWNMERE, "FromRoute1" }, { ZM_SCENE_THORNACRE, "FromRoute1" } };
	const ZM_SceneConnection s_axConnPlayerHome[]= { { ZM_SCENE_DAWNMERE, "FromHome" } };
	const ZM_SceneConnection s_axConnProfLab[]   = { { ZM_SCENE_DAWNMERE, "FromLab" } };
	const ZM_SceneConnection s_axConnGym1[]      = { { ZM_SCENE_THORNACRE, "FromGym" } };

	// -- spawn tags each scene offers (inbound connections target these) --
	const char* const s_aszTagsFrontEnd[]  = { "Start" };
	const char* const s_aszTagsDawnmere[]  = { "TownCenter", "FromRoute1", "FromHome", "FromLab" };
	const char* const s_aszTagsThornacre[] = { "FromRoute1", "FromGym" };
	const char* const s_aszTagsRoute1[]    = { "FromDawnmere", "FromThornacre" };
	const char* const s_aszTagsPlayerHome[]= { "Door" };
	const char* const s_aszTagsProfLab[]   = { "Door" };
	const char* const s_aszTagsGym1[]      = { "Door" };

	// -- wild encounters (routes only) --
	const ZM_EncounterSlot s_axEncRoute1[] = {
		{ ZM_SPECIES_PIPWIT,  2, 4, 40 },
		{ ZM_SPECIES_NIBBIN,  2, 4, 40 },
		{ ZM_SPECIES_SPARKIT, 3, 5, 20 },
	};

	const ZM_WorldSpec s_axScenes[ZM_SCENE_COUNT] =
	{
		{ ZM_SCENE_FRONTEND,   "Title",            0,  ZM_SCENE_KIND_FRONTEND, "",          s_axConnFrontEnd,  ZM_ARRLEN(s_axConnFrontEnd),  s_aszTagsFrontEnd,  ZM_ARRLEN(s_aszTagsFrontEnd),  nullptr,        0 },
		{ ZM_SCENE_BATTLE,     "Battlefield",      1,  ZM_SCENE_KIND_BATTLE,   "",          nullptr,           0,                            nullptr,            0,                             nullptr,        0 },
		{ ZM_SCENE_DAWNMERE,   "Dawnmere Village", 2,  ZM_SCENE_KIND_TOWN,     "Dawnmere",  s_axConnDawnmere,  ZM_ARRLEN(s_axConnDawnmere),  s_aszTagsDawnmere,  ZM_ARRLEN(s_aszTagsDawnmere),  nullptr,        0 },
		{ ZM_SCENE_THORNACRE,  "Thornacre Town",   3,  ZM_SCENE_KIND_TOWN,     "Thornacre", s_axConnThornacre, ZM_ARRLEN(s_axConnThornacre), s_aszTagsThornacre, ZM_ARRLEN(s_aszTagsThornacre), nullptr,        0 },
		{ ZM_SCENE_ROUTE1,     "Route 1",          20, ZM_SCENE_KIND_ROUTE,    "Route1",    s_axConnRoute1,    ZM_ARRLEN(s_axConnRoute1),    s_aszTagsRoute1,    ZM_ARRLEN(s_aszTagsRoute1),    s_axEncRoute1,  ZM_ARRLEN(s_axEncRoute1) },
		{ ZM_SCENE_PLAYERHOME, "Player's Home",    40, ZM_SCENE_KIND_INTERIOR, "",          s_axConnPlayerHome,ZM_ARRLEN(s_axConnPlayerHome),s_aszTagsPlayerHome,ZM_ARRLEN(s_aszTagsPlayerHome),nullptr,        0 },
		{ ZM_SCENE_PROFLAB,    "Aster's Lab",      41, ZM_SCENE_KIND_INTERIOR, "",          s_axConnProfLab,   ZM_ARRLEN(s_axConnProfLab),   s_aszTagsProfLab,   ZM_ARRLEN(s_aszTagsProfLab),   nullptr,        0 },
		{ ZM_SCENE_GYM1,       "Thornacre Gym",    42, ZM_SCENE_KIND_GYM,      "",          s_axConnGym1,      ZM_ARRLEN(s_axConnGym1),      s_aszTagsGym1,      ZM_ARRLEN(s_aszTagsGym1),      nullptr,        0 },
	};

#undef ZM_ARRLEN
}

const ZM_WorldSpec& ZM_GetWorldSpec(ZM_SCENE_ID eId)
{
	Zenith_Assert(eId < ZM_SCENE_COUNT, "ZM_GetWorldSpec: scene id out of range (%u)", (u_int)eId);
	return s_axScenes[eId];
}

u_int ZM_GetSceneCount()
{
	return ZM_SCENE_COUNT;
}

const char* ZM_GetSceneName(ZM_SCENE_ID eId)
{
	if (eId >= ZM_SCENE_COUNT)
	{
		return "NONE";
	}
	return s_axScenes[eId].m_szName;
}

ZM_SCENE_ID ZM_FindSceneByBuildIndex(u_int uBuildIndex)
{
	for (u_int i = 0; i < ZM_SCENE_COUNT; ++i)
	{
		if (s_axScenes[i].m_uBuildIndex == uBuildIndex)
		{
			return (ZM_SCENE_ID)i;
		}
	}
	return ZM_SCENE_NONE;
}

const char* ZM_SceneKindToString(ZM_SCENE_KIND eKind)
{
	switch (eKind)
	{
	case ZM_SCENE_KIND_FRONTEND:     return "FRONTEND";
	case ZM_SCENE_KIND_TOWN:         return "TOWN";
	case ZM_SCENE_KIND_ROUTE:        return "ROUTE";
	case ZM_SCENE_KIND_INTERIOR:     return "INTERIOR";
	case ZM_SCENE_KIND_GYM:          return "GYM";
	case ZM_SCENE_KIND_BATTLE:       return "BATTLE";
	case ZM_SCENE_KIND_TOWER:        return "TOWER";
	case ZM_SCENE_KIND_LEAGUE:       return "LEAGUE";
	case ZM_SCENE_KIND_VICTORY_ROAD: return "VICTORY_ROAD";
	default:                         return "INVALID";
	}
}
