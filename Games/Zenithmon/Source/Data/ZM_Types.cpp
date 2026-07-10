#include "Zenith.h"
#include "Zenithmon/Source/Data/ZM_Types.h"

// ============================================================================
// ZM_Types -- type-name lookup. The name table is index-parallel to the ZM_TYPE
// enum; a static_assert pins the two together so adding a type without a name
// (or vice versa) fails to compile.
// ============================================================================

namespace
{
	constexpr const char* s_aszTypeNames[ZM_TYPE_COUNT] =
	{
		"NORMAL",
		"FIRE",
		"WATER",
		"GRASS",
		"ELECTRIC",
		"ICE",
		"BRAWL",
		"VENOM",
		"EARTH",
		"SKY",
		"MIND",
		"SWARM",
		"STONE",
		"PHANTOM",
		"DRAKE",
		"UMBRAL",
		"IRON",
		"FEY",
	};

	static_assert(sizeof(s_aszTypeNames) / sizeof(s_aszTypeNames[0]) == ZM_TYPE_COUNT,
		"ZM_TypeToString name table must have exactly one entry per ZM_TYPE");
}

const char* ZM_TypeToString(ZM_TYPE eType)
{
	if (eType == ZM_TYPE_NONE)
	{
		return "NONE";
	}
	if (eType >= ZM_TYPE_COUNT)
	{
		return "INVALID";
	}
	return s_aszTypeNames[eType];
}
