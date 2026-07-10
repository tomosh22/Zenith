#pragma once

#include "Zenithmon/Source/Data/ZM_SpeciesData.h"
#include "Zenithmon/Source/Data/ZM_MoveData.h"
#include "Zenithmon/Source/Data/ZM_ItemData.h"
#include "Zenithmon/Source/Data/ZM_AbilityData.h"
#include "Zenithmon/Source/Data/ZM_NatureData.h"
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"

// ============================================================================
// ZM_DataRegistry -- name->ID lookup across every S1 data table (S1 data core,
// the box that CLOSES S1). Spec: Docs/TestPlan.md 5.1 (registry integrity);
// DecisionLog ZM-D-030.
//
// Each ZM_FindXByName returns the row's ID, or that table's NONE sentinel for an
// unknown / null / empty name (exact, case-sensitive match on the canonical
// name). These are the reverse of the ZM_GetXName accessors -- needed by
// save/load, WorldSpec authoring, and debug/console tooling that speak names.
// Lookups are a linear scan (the tables are small: <= ~220 rows); a hash index
// is a trivial later optimisation behind the same signatures.
//
// The mutual consistency of the tables (every evolution target / TM move /
// encounter species / learnset move resolves; every name round-trips) is the
// schema-enforcer contract, locked by Tests/ZM_Tests_DataRegistry.cpp.
// ============================================================================

ZM_SPECIES_ID	ZM_FindSpeciesByName(const char* szName);   // ZM_SPECIES_NONE if not found
ZM_MOVE_ID		ZM_FindMoveByName(const char* szName);      // ZM_MOVE_NONE
ZM_ITEM_ID		ZM_FindItemByName(const char* szName);      // ZM_ITEM_NONE
ZM_ABILITY_ID	ZM_FindAbilityByName(const char* szName);   // ZM_ABILITY_NONE
ZM_NATURE		ZM_FindNatureByName(const char* szName);    // ZM_NATURE_COUNT
ZM_SCENE_ID		ZM_FindSceneByName(const char* szName);     // ZM_SCENE_NONE
