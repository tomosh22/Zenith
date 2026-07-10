#include "Zenith.h"
#include "Zenithmon/Source/Data/ZM_DataRegistry.h"

#include <cstring>

// ============================================================================
// ZM_DataRegistry -- linear-scan name->ID lookups (DecisionLog ZM-D-030). Each
// scans its table's canonical names via the ZM_GetXName accessors. Cross-table
// integrity is asserted by Tests/ZM_Tests_DataRegistry.cpp.
// ============================================================================

namespace
{
	bool NameMatches(const char* szCandidate, const char* szQuery)
	{
		return szCandidate != nullptr && strcmp(szCandidate, szQuery) == 0;
	}
}

ZM_SPECIES_ID ZM_FindSpeciesByName(const char* szName)
{
	if (szName == nullptr || szName[0] == '\0') { return ZM_SPECIES_NONE; }
	for (u_int i = 0; i < ZM_GetSpeciesCount(); ++i)
	{
		if (NameMatches(ZM_GetSpeciesName((ZM_SPECIES_ID)i), szName)) { return (ZM_SPECIES_ID)i; }
	}
	return ZM_SPECIES_NONE;
}

ZM_MOVE_ID ZM_FindMoveByName(const char* szName)
{
	if (szName == nullptr || szName[0] == '\0') { return ZM_MOVE_NONE; }
	for (u_int i = 0; i < ZM_GetMoveCount(); ++i)
	{
		if (NameMatches(ZM_GetMoveName((ZM_MOVE_ID)i), szName)) { return (ZM_MOVE_ID)i; }
	}
	return ZM_MOVE_NONE;
}

ZM_ITEM_ID ZM_FindItemByName(const char* szName)
{
	if (szName == nullptr || szName[0] == '\0') { return ZM_ITEM_NONE; }
	for (u_int i = 0; i < ZM_GetItemCount(); ++i)
	{
		if (NameMatches(ZM_GetItemName((ZM_ITEM_ID)i), szName)) { return (ZM_ITEM_ID)i; }
	}
	return ZM_ITEM_NONE;
}

ZM_ABILITY_ID ZM_FindAbilityByName(const char* szName)
{
	if (szName == nullptr || szName[0] == '\0') { return ZM_ABILITY_NONE; }
	for (u_int i = 0; i < ZM_GetAbilityCount(); ++i)
	{
		if (NameMatches(ZM_GetAbilityName((ZM_ABILITY_ID)i), szName)) { return (ZM_ABILITY_ID)i; }
	}
	return ZM_ABILITY_NONE;
}

ZM_NATURE ZM_FindNatureByName(const char* szName)
{
	if (szName == nullptr || szName[0] == '\0') { return ZM_NATURE_COUNT; }
	for (u_int i = 0; i < ZM_GetNatureCount(); ++i)
	{
		if (NameMatches(ZM_GetNatureName((ZM_NATURE)i), szName)) { return (ZM_NATURE)i; }
	}
	return ZM_NATURE_COUNT;
}

ZM_SCENE_ID ZM_FindSceneByName(const char* szName)
{
	if (szName == nullptr || szName[0] == '\0') { return ZM_SCENE_NONE; }
	for (u_int i = 0; i < ZM_GetSceneCount(); ++i)
	{
		if (NameMatches(ZM_GetSceneName((ZM_SCENE_ID)i), szName)) { return (ZM_SCENE_ID)i; }
	}
	return ZM_SCENE_NONE;
}
