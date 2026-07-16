#include "Zenith.h"
#include "Zenithmon/Source/Data/ZM_PropData.h"

// ============================================================================
// ZM_PropData -- the 25-model prop roster table (structural roster). Rows are in
// ZM_PROP_ID order; s_axProps[i].m_eId == i is asserted by the tests. Dimensions
// are metres of the box composition (small fixtures modest, bridges + dressing
// sets larger); the fields drive the SC4 static box mesh + the placeholder albedo.
//
// The 6 DRESSING rows each carry a real ZM_PROP_BIOME (one per battle-dome biome);
// every non-DRESSING row is ZM_PROP_BIOME_NONE.
// ============================================================================

namespace
{
	const ZM_PropData s_axProps[ZM_PROP_COUNT] =
	{
		{ ZM_PROP_FENCE_WOOD,   "FenceWood",   ZM_PROP_KIND_FENCE,     ZM_PROP_BIOME_NONE,     ZM_PROP_PALETTE_WOOD,    2.0f, 0.12f, 1.0f },
		{ ZM_PROP_FENCE_STONE,  "FenceStone",  ZM_PROP_KIND_FENCE,     ZM_PROP_BIOME_NONE,     ZM_PROP_PALETTE_STONE,   2.0f, 0.22f, 0.9f },
		{ ZM_PROP_SIGN_POST,    "SignPost",    ZM_PROP_KIND_SIGN,      ZM_PROP_BIOME_NONE,     ZM_PROP_PALETTE_WOOD,    0.9f, 0.12f, 2.0f },
		{ ZM_PROP_TOWN_BOARD,   "TownBoard",   ZM_PROP_KIND_SIGN,      ZM_PROP_BIOME_NONE,     ZM_PROP_PALETTE_PAINTED, 1.6f, 0.15f, 2.2f },
		{ ZM_PROP_LAMP_POST,    "LampPost",    ZM_PROP_KIND_LAMP,      ZM_PROP_BIOME_NONE,     ZM_PROP_PALETTE_METAL,   0.4f, 0.4f,  3.0f },
		{ ZM_PROP_LANTERN_POST, "LanternPost", ZM_PROP_KIND_LAMP,      ZM_PROP_BIOME_NONE,     ZM_PROP_PALETTE_METAL,   0.4f, 0.4f,  2.4f },
		{ ZM_PROP_BRIDGE_PLANK, "BridgePlank", ZM_PROP_KIND_BRIDGE,    ZM_PROP_BIOME_NONE,     ZM_PROP_PALETTE_WOOD,    3.0f, 4.0f,  1.0f },
		{ ZM_PROP_BRIDGE_STONE, "BridgeStone", ZM_PROP_KIND_BRIDGE,    ZM_PROP_BIOME_NONE,     ZM_PROP_PALETTE_STONE,   3.4f, 5.0f,  1.2f },
		{ ZM_PROP_LEDGE_LOW,    "LedgeLow",    ZM_PROP_KIND_LEDGE,     ZM_PROP_BIOME_NONE,     ZM_PROP_PALETTE_STONE,   2.0f, 1.0f,  0.6f },
		{ ZM_PROP_LEDGE_HIGH,   "LedgeHigh",   ZM_PROP_KIND_LEDGE,     ZM_PROP_BIOME_NONE,     ZM_PROP_PALETTE_STONE,   2.4f, 1.2f,  1.0f },
		{ ZM_PROP_ROCK_SMALL,   "RockSmall",   ZM_PROP_KIND_ROCK,      ZM_PROP_BIOME_NONE,     ZM_PROP_PALETTE_STONE,   0.8f, 0.8f,  0.7f },
		{ ZM_PROP_ROCK_LARGE,   "RockLarge",   ZM_PROP_KIND_ROCK,      ZM_PROP_BIOME_NONE,     ZM_PROP_PALETTE_STONE,   1.6f, 1.5f,  1.4f },
		{ ZM_PROP_BOULDER,      "Boulder",     ZM_PROP_KIND_ROCK,      ZM_PROP_BIOME_NONE,     ZM_PROP_PALETTE_STONE,   2.4f, 2.2f,  2.0f },
		{ ZM_PROP_TABLE,        "Table",       ZM_PROP_KIND_FURNITURE, ZM_PROP_BIOME_NONE,     ZM_PROP_PALETTE_WOOD,    1.4f, 0.9f,  0.9f },
		{ ZM_PROP_CHAIR,        "Chair",       ZM_PROP_KIND_FURNITURE, ZM_PROP_BIOME_NONE,     ZM_PROP_PALETTE_WOOD,    0.6f, 0.6f,  1.0f },
		{ ZM_PROP_BED,          "Bed",         ZM_PROP_KIND_FURNITURE, ZM_PROP_BIOME_NONE,     ZM_PROP_PALETTE_PAINTED, 2.0f, 1.2f,  0.7f },
		{ ZM_PROP_SHELF,        "Shelf",       ZM_PROP_KIND_FURNITURE, ZM_PROP_BIOME_NONE,     ZM_PROP_PALETTE_WOOD,    1.2f, 0.4f,  2.0f },
		{ ZM_PROP_COUNTER,      "Counter",     ZM_PROP_KIND_FURNITURE, ZM_PROP_BIOME_NONE,     ZM_PROP_PALETTE_WOOD,    2.2f, 0.7f,  1.0f },
		{ ZM_PROP_BARREL,       "Barrel",      ZM_PROP_KIND_FURNITURE, ZM_PROP_BIOME_NONE,     ZM_PROP_PALETTE_WOOD,    0.7f, 0.7f,  1.0f },

		// --- 6 battle-dome biome dressing sets ---
		{ ZM_PROP_DRESSING_MEADOW,   "DressingMeadow",   ZM_PROP_KIND_DRESSING, ZM_PROP_BIOME_MEADOW,   ZM_PROP_PALETTE_FOLIAGE, 3.0f, 3.0f, 1.6f },
		{ ZM_PROP_DRESSING_VOLCANIC, "DressingVolcanic", ZM_PROP_KIND_DRESSING, ZM_PROP_BIOME_VOLCANIC, ZM_PROP_PALETTE_STONE,   3.0f, 3.0f, 1.8f },
		{ ZM_PROP_DRESSING_COAST,    "DressingCoast",    ZM_PROP_KIND_DRESSING, ZM_PROP_BIOME_COAST,    ZM_PROP_PALETTE_STONE,   3.0f, 3.0f, 1.4f },
		{ ZM_PROP_DRESSING_WETLAND,  "DressingWetland",  ZM_PROP_KIND_DRESSING, ZM_PROP_BIOME_WETLAND,  ZM_PROP_PALETTE_FOLIAGE, 3.0f, 3.0f, 1.5f },
		{ ZM_PROP_DRESSING_SNOW,     "DressingSnow",     ZM_PROP_KIND_DRESSING, ZM_PROP_BIOME_SNOW,     ZM_PROP_PALETTE_STONE,   3.0f, 3.0f, 1.6f },
		{ ZM_PROP_DRESSING_CANYON,   "DressingCanyon",   ZM_PROP_KIND_DRESSING, ZM_PROP_BIOME_CANYON,   ZM_PROP_PALETTE_STONE,   3.0f, 3.0f, 2.0f },
	};
}

const ZM_PropData& ZM_GetPropData(ZM_PROP_ID eId)
{
	Zenith_Assert(eId < ZM_PROP_COUNT, "ZM_GetPropData: prop id out of range (%u)", (u_int)eId);
	return s_axProps[eId];
}

u_int ZM_GetPropCount()
{
	return ZM_PROP_COUNT;
}

const char* ZM_GetPropName(ZM_PROP_ID eId)
{
	if (eId >= ZM_PROP_COUNT)
	{
		return "NONE";
	}
	return s_axProps[eId].m_szName;
}
