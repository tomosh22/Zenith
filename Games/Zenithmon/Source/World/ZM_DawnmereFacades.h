#pragma once

#include "Zenithmon/Source/Data/ZM_BuildingData.h"   // ZM_BUILDING_ID

#include <cstring>   // strcmp -- the mapping is an EXACT name match

// ============================================================================
// ZM_DawnmereFacades -- which generated building each Dawnmere facade entity
// wears, as ONE pure total function both the runtime component and the tests
// read.
//
// PURE. No ECS, no scene, no physics, no g_xEngine, no allocation, no I/O and no
// ZENITH_TOOLS guard -- the mapping has to be visible to boot units in a headless
// CI build where the authoring is compiled out entirely, AND to
// ZM_BuildingFacade, which compiles in EVERY configuration. HEADER-ONLY.
//
// ★★ WHY THE BUILDING IS RESOLVED FROM A NAME RATHER THAN AUTHORED INTO THE
// SCENE. The obvious design -- author a Zenith_ModelComponent and let
// AddStep_LoadModel put the .zmodel in it -- was tried first and is WRONG for a
// COMMITTED scene, for a reason worth writing down:
//
//   Zenith_ModelComponent::WriteToDataStream serializes the model as a GUID and
//   then writes EVERY MATERIAL INLINE, one full Zenith_MaterialAsset per slot.
//   How many slots exist at save time depends on whether the model and its
//   materials have finished resolving. The first authoring boot wrote Dawnmere at
//   79,058 bytes; the second, with the same 40 entities and the assets now warm,
//   wrote 82,152. A tracked scene whose size depends on asset load state fails
//   the boot-shape-independence invariant this repo has already lost twice
//   (ZM-D-179, ZM-D-183), and it freezes a copy of every material into the scene
//   so a later material edit would not reach the shipped bytes.
//
// So the scene carries a COMPONENT NAME and a version u_int, and the model is
// loaded at runtime from the id this file resolves -- which is exactly what every
// other model-bearing entity in this game already does (humans and props resolve
// through ZM_GreyboxVisual and author no model either). That is why a Zenithmon
// .zscen contains no `game:Props/...` or `game:Humans/...` reference at all.
//
// ★ THE NAMES ARE CONTRACT. They are what the authoring creates, what the
// component matches on, and what a test looks up; a rename is a scene re-author.
// ============================================================================

inline constexpr const char* szZM_DAWNMERE_HOME_FACADE_ENTITY_NAME =
	"DawnmereHomeFacade";
inline constexpr const char* szZM_DAWNMERE_LAB_FACADE_ENTITY_NAME =
	"DawnmereLabFacade";

// The building an entity of this name wears.
//
// TOTAL: any other name -- an NPC, a blockout, a prop, a null -- answers
// ZM_BUILDING_COUNT, which every caller reads as "this entity is not a facade".
// It does NOT answer with a plausible-looking building, because a facade that
// silently wore the wrong model would render a complete, correct-looking house in
// the wrong place and nothing headless could see it.
inline ZM_BUILDING_ID ZM_BuildingForFacadeEntity(const char* szEntityName)
{
	if (szEntityName == nullptr)
	{
		return ZM_BUILDING_COUNT;
	}
	if (strcmp(szEntityName, szZM_DAWNMERE_HOME_FACADE_ENTITY_NAME) == 0)
	{
		return ZM_BUILDING_PLAYER_HOME;
	}
	if (strcmp(szEntityName, szZM_DAWNMERE_LAB_FACADE_ENTITY_NAME) == 0)
	{
		return ZM_BUILDING_LAB;
	}
	return ZM_BUILDING_COUNT;
}

// Is this entity a Dawnmere building facade at all?
inline bool ZM_IsDawnmereFacadeEntity(const char* szEntityName)
{
	return ZM_BuildingForFacadeEntity(szEntityName) < ZM_BUILDING_COUNT;
}
