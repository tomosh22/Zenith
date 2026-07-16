#pragma once

#include "Zenithmon/Source/Data/ZM_SpeciesData.h"   // ZM_SPECIES_ID

// ============================================================================
// ZM_WorldSpec -- the keystone declarative world table (S1 data core, SKELETON).
// One row per scene: name, build index, kind, terrain set, warp connections
// (with spawn tags), and (for routes) an encounter table. Tools walk it to
// author terrains/scenes; runtime walks it for warps/encounters/gating. Spec:
// Docs/GameDesignDocument.md + DecisionLog ZM-D-005 (keystone) / ZM-D-029.
//
// S1 ships the SCHEMA + a small proving set (the scenes that exist conceptually
// now: FrontEnd, Battle, Dawnmere, Route 1, Thornacre, the two Dawnmere
// interiors, Gym 1) -- enough to exercise every column and lock the referential-
// integrity tests. The full ~40-scene world is authored at S9/S10 by APPENDING
// rows (ZM_SCENE_ID is save-stable: append before ZM_SCENE_COUNT, never reorder).
//
// Compiled const table (ZM-D-009). Build indices follow ZM-D-012 (0 FrontEnd,
// 1 Battle, towns/routes/interiors sparse). Names/tags are original (zero
// Nintendo IP). Connections reference target scenes by ZM_SCENE_ID; the runtime
// warp resolves the target's build index + the named spawn point (ZM-D-006).
// ============================================================================

// Scene category. Drives authoring (terrain vs interior), streaming, and the
// runtime (encounter rolls only on ROUTE, additive load for BATTLE, ...).
enum ZM_SCENE_KIND : u_int
{
	ZM_SCENE_KIND_FRONTEND,       // title screen (build index 0)
	ZM_SCENE_KIND_TOWN,           // outdoor hub with buildings
	ZM_SCENE_KIND_ROUTE,          // outdoor path with wild encounters
	ZM_SCENE_KIND_INTERIOR,       // building interior
	ZM_SCENE_KIND_GYM,            // gym interior
	ZM_SCENE_KIND_BATTLE,         // additive battle scene (build index 1)
	ZM_SCENE_KIND_TOWER,          // Battle Tower (post-game)
	ZM_SCENE_KIND_LEAGUE,         // Elite Four / Champion rooms
	ZM_SCENE_KIND_VICTORY_ROAD,   // pre-League gauntlet

	ZM_SCENE_KIND_COUNT
};

// Every scene, in table order. IDs are contiguous 0..ZM_SCENE_COUNT-1 and the
// row index equals the id (asserted by ZM_Tests_WorldSpec).
enum ZM_SCENE_ID : u_int
{
	ZM_SCENE_FRONTEND,
	ZM_SCENE_BATTLE,
	ZM_SCENE_DAWNMERE,
	ZM_SCENE_THORNACRE,
	ZM_SCENE_ROUTE1,
	ZM_SCENE_PLAYERHOME,
	ZM_SCENE_PROFLAB,
	ZM_SCENE_GYM1,

	ZM_SCENE_COUNT,
	ZM_SCENE_NONE = ZM_SCENE_COUNT   // "no scene" sentinel (unresolved build index)
};

// A warp edge: reaching m_szSpawnTag's spawn point in scene m_eTarget.
struct ZM_SceneConnection
{
	ZM_SCENE_ID	m_eTarget;
	const char*	m_szSpawnTag;   // must be a spawn tag the target scene offers
};

// One wild-encounter slot on a route: a species over a level band, weighted.
struct ZM_EncounterSlot
{
	ZM_SPECIES_ID	m_eSpecies;
	u_int			m_uMinLevel;
	u_int			m_uMaxLevel;
	u_int			m_uWeight;      // relative encounter weight (> 0)
};

// One scene row. The connection / spawn-tag / encounter arrays are per-scene
// static tables referenced by pointer + count (empty => count 0, pointer may be
// null). m_szTerrainSet is "" for scenes with no terrain (interiors, battle).
struct ZM_WorldSpec
{
	ZM_SCENE_ID					m_eId;
	const char*					m_szName;
	u_int						m_uBuildIndex;
	ZM_SCENE_KIND				m_eKind;
	const char*					m_szTerrainSet;
	const ZM_SceneConnection*	m_pxConnections;
	u_int						m_uConnectionCount;
	const char* const*			m_pszSpawnTags;
	u_int						m_uSpawnTagCount;
	const ZM_EncounterSlot*		m_pxEncounters;
	u_int						m_uEncounterCount;
	u_int						m_uEncounterRatePer256;   // per-tile-transition wild-encounter chance /256; 0 = none (non-route)
};

// Table accessors (bounds-asserted). GetWorldSpec indexes by ZM_SCENE_ID.
const ZM_WorldSpec&	ZM_GetWorldSpec(ZM_SCENE_ID eId);
u_int				ZM_GetSceneCount();                 // == ZM_SCENE_COUNT
const char*			ZM_GetSceneName(ZM_SCENE_ID eId);   // "NONE" out of range

// Resolve a runtime build index to its scene, or ZM_SCENE_NONE if none matches.
ZM_SCENE_ID			ZM_FindSceneByBuildIndex(u_int uBuildIndex);

const char*			ZM_SceneKindToString(ZM_SCENE_KIND eKind);
