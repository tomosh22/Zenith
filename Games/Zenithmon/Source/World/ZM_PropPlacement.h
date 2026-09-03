#pragma once

#include "Zenithmon/Source/Data/ZM_PropData.h"
#include "Zenithmon/Source/World/ZM_InteriorDressing.h"
#include "Zenithmon/Source/World/ZM_DawnmereDressing.h"

// ============================================================================
// ZM_PropPlacement -- WHERE each roster prop is placed, and therefore whether it
// is placed at all.
//
// ★★★ THE RULING THIS EXISTS TO ENFORCE (2026-09-01): **if the game generates an
// asset then it is placed in game, and where a `.glb` exists it is the `.glb`
// that gets used.** Both halves are mechanical now -- this header answers the
// first, and `ZM_Dressing/EveryGeneratedPropIsPlacedInGame` refuses a roster row
// that answers NONE.
//
// ★★ IT EXISTS BECAUSE THE ANSWER WAS UNCOUNTABLE AND WAS THEREFORE GUESSED
// WRONG. Twelve of the twenty-eight rows were being generated, baked and
// rendered nowhere, and a comment in ZM_DawnmereDressing.h confidently said
// "22 of the 28" while listing the six battle-dome dressing sets among the
// unused -- ZM_BattleArena.cpp places all six -- and missing the three ground
// items entirely. The count was wrong because it came from reading code rather
// than from a function anything could call. This is that function.
//
// ★ THE COST OF GETTING IT WRONG IS NOT COSMETIC. A `.glb` dropped onto a row
// nothing places replaces a model that still does not appear, so the import
// looks finished and renders nowhere. AB-PROP-07 arrived exactly that way: the
// lamp post had never been placed in any scene, so importing art for it was
// half a job and the placement row was the other half.
//
// PURE: no ECS, no scene, no assets, no g_xEngine, no ZENITH_TOOLS guard. It
// reads compiled tables only, so the boot units can answer the ruling headless.
// ============================================================================

enum ZM_PROP_PLACEMENT_SITE : u_int
{
	// Placed nowhere. Under the ruling above this is a DEFECT, not a state.
	ZM_PROP_PLACEMENT_NONE,

	// An authored row in one of the two interior rooms (ZM_InteriorDressing.h).
	ZM_PROP_PLACEMENT_INTERIOR,

	// An authored row in the outdoor town table (ZM_DawnmereDressing.h).
	ZM_PROP_PLACEMENT_DAWNMERE,

	// ★ THE LAST TWO ARE PLACED BY A SYSTEM RATHER THAN A TABLE, and are resolved
	// by KIND because that is exactly how those systems choose them -- neither
	// reads a placement row, so there is no table here to consult:
	//
	//   ZM_BattleArena.cpp keys s_aeDressingProp by ZM_BATTLE_BIOME, one dressing
	//   set per biome, and spawns the matching child entity with the arena.
	//
	//   ZM_GroundItem.cpp chooses between the pickup and spent presentations per
	//   frame from the item a prop yields and whether this save has taken it.
	//
	// Answering by kind is therefore a statement about those systems, not a
	// shortcut: adding a DRESSING or ITEM row to the roster makes it placed by
	// construction, and that is the property being reported.
	ZM_PROP_PLACEMENT_BATTLE_ARENA,
	ZM_PROP_PLACEMENT_GROUND_ITEM,
};

// TOTAL for every id, in range or not: an out-of-range id answers NONE.
inline ZM_PROP_PLACEMENT_SITE ZM_WherePropIsPlaced(ZM_PROP_ID eProp)
{
	if (static_cast<u_int>(eProp) >= static_cast<u_int>(ZM_PROP_COUNT))
	{
		return ZM_PROP_PLACEMENT_NONE;
	}

	// The two AUTHORED tables first, since they name an entity and are the more
	// specific answer.
	for (u_int r = 0u; r < static_cast<u_int>(ZM_INTERIOR_ROOM_COUNT); ++r)
	{
		const ZM_INTERIOR_ROOM eRoom = static_cast<ZM_INTERIOR_ROOM>(r);
		const u_int uCount = ZM_GetInteriorPropCount(eRoom);
		for (u_int i = 0u; i < uCount; ++i)
		{
			if (ZM_GetInteriorProp(eRoom, i).m_eProp == eProp)
			{
				return ZM_PROP_PLACEMENT_INTERIOR;
			}
		}
		// ★ THE LIGHT FIXTURES ARE AN INTERIOR PLACEMENT TOO. They are authored
		// from their own table (axZM_*_FIXTURES) rather than the furniture one --
		// they carry no collider and their Y is a model base, not a floor -- but
		// "is this roster row placed in a scene" is the same question and has the
		// same answer. Without this arm the four ZM_PROP_KIND_LIGHT_FIXTURE rows
		// would answer NONE and ZM_Dressing/EveryGeneratedPropIsPlacedInGame would
		// red on props that are in fact standing in both rooms.
		const u_int uFixtures = ZM_GetInteriorFixtureCount(eRoom);
		for (u_int i = 0u; i < uFixtures; ++i)
		{
			if (ZM_GetInteriorFixture(eRoom, i).m_eProp == eProp)
			{
				return ZM_PROP_PLACEMENT_INTERIOR;
			}
		}
	}
	for (u_int i = 0u; i < ZM_GetDawnmerePropCount(); ++i)
	{
		if (ZM_GetDawnmereProp(i).m_eProp == eProp)
		{
			return ZM_PROP_PLACEMENT_DAWNMERE;
		}
	}

	// Then the two system-placed families, by kind. See the enum above.
	switch (ZM_GetPropData(eProp).m_eKind)
	{
	case ZM_PROP_KIND_DRESSING:
		return ZM_PROP_PLACEMENT_BATTLE_ARENA;
	case ZM_PROP_KIND_ITEM_PICKUP:
	case ZM_PROP_KIND_ITEM_SPENT:
		return ZM_PROP_PLACEMENT_GROUND_ITEM;
	default:
		return ZM_PROP_PLACEMENT_NONE;
	}
}

// For diagnostics. TOTAL: an unknown site answers "nowhere".
inline const char* ZM_PropPlacementSiteName(ZM_PROP_PLACEMENT_SITE eSite)
{
	switch (eSite)
	{
	case ZM_PROP_PLACEMENT_INTERIOR:      return "an interior room";
	case ZM_PROP_PLACEMENT_DAWNMERE:      return "the Dawnmere outdoor table";
	case ZM_PROP_PLACEMENT_BATTLE_ARENA:  return "the battle arena (by biome)";
	case ZM_PROP_PLACEMENT_GROUND_ITEM:   return "a ground-item prop (by kind)";
	default:                              return "nowhere";
	}
}
