#include "Zenith.h"
#include "Zenithmon/Source/Data/ZM_BuildingData.h"

// ============================================================================
// ZM_BuildingData -- the 30-model building roster table (structural roster). Rows
// are in ZM_BUILDING_ID order; s_axBuildings[i].m_eId == i is asserted by the
// tests. Dimensions are metres of the parametric shell (houses modest, gyms
// larger, civic in-between); the fields drive the SC2 shell + the SC1/SC3 facade.
//
// The 8 gyms' m_eThemeType is pinned to the SHIPPED gym leaders' element types
// (ZM_HumanData.h GDD 3.4, badge order): Fenna GRASS, Bram FIRE, Maris WATER,
// Tessa ELECTRIC, Aquilo SKY, Morwenna PHANTOM, Halvard ICE, Vardis DRAKE. Every
// non-gym row is ZM_TYPE_NONE.
// ============================================================================

namespace
{
	const ZM_BuildingData s_axBuildings[ZM_BUILDING_COUNT] =
	{
		// --- Houses: 4 styles x 3 palettes (12) ---
		{ ZM_BUILDING_HOUSE_COTTAGE_WARM,    "CottageWarm",    ZM_BUILDING_STYLE_COTTAGE,   ZM_BUILDING_PALETTE_WARM,  ZM_ROOF_GABLE, 6.0f,  5.0f,  3.0f, 0.70f,  1u, 2u, 1u, ZM_TYPE_NONE, false,  1.30f, 2.20f },
		{ ZM_BUILDING_HOUSE_COTTAGE_COOL,    "CottageCool",    ZM_BUILDING_STYLE_COTTAGE,   ZM_BUILDING_PALETTE_COOL,  ZM_ROOF_GABLE, 6.0f,  5.0f,  3.0f, 0.70f,  1u, 2u, 1u, ZM_TYPE_NONE, false,  1.30f, 2.20f },
		{ ZM_BUILDING_HOUSE_COTTAGE_EARTH,   "CottageEarth",   ZM_BUILDING_STYLE_COTTAGE,   ZM_BUILDING_PALETTE_EARTH, ZM_ROOF_GABLE, 6.0f,  5.0f,  3.0f, 0.70f,  1u, 2u, 1u, ZM_TYPE_NONE, false,  1.30f, 2.20f },
		{ ZM_BUILDING_HOUSE_TOWNHOUSE_WARM,  "TownhouseWarm",  ZM_BUILDING_STYLE_TOWNHOUSE, ZM_BUILDING_PALETTE_WARM,  ZM_ROOF_HIP,   6.0f,  5.0f,  3.0f, 0.70f,  2u, 2u, 2u, ZM_TYPE_NONE, false,  1.30f, 2.20f },
		{ ZM_BUILDING_HOUSE_TOWNHOUSE_COOL,  "TownhouseCool",  ZM_BUILDING_STYLE_TOWNHOUSE, ZM_BUILDING_PALETTE_COOL,  ZM_ROOF_HIP,   6.0f,  5.0f,  3.0f, 0.70f,  2u, 2u, 2u, ZM_TYPE_NONE, false,  1.30f, 2.20f },
		{ ZM_BUILDING_HOUSE_TOWNHOUSE_EARTH, "TownhouseEarth", ZM_BUILDING_STYLE_TOWNHOUSE, ZM_BUILDING_PALETTE_EARTH, ZM_ROOF_HIP,   6.0f,  5.0f,  3.0f, 0.70f,  2u, 2u, 2u, ZM_TYPE_NONE, false,  1.30f, 2.20f },
		{ ZM_BUILDING_HOUSE_SHOP_WARM,       "ShopWarm",       ZM_BUILDING_STYLE_SHOP,      ZM_BUILDING_PALETTE_WARM,  ZM_ROOF_FLAT,  7.0f,  6.0f,  3.0f, 0.70f,  1u, 3u, 1u, ZM_TYPE_NONE, false,  1.30f, 2.20f },
		{ ZM_BUILDING_HOUSE_SHOP_COOL,       "ShopCool",       ZM_BUILDING_STYLE_SHOP,      ZM_BUILDING_PALETTE_COOL,  ZM_ROOF_FLAT,  7.0f,  6.0f,  3.0f, 0.70f,  1u, 3u, 1u, ZM_TYPE_NONE, false,  1.30f, 2.20f },
		{ ZM_BUILDING_HOUSE_SHOP_EARTH,      "ShopEarth",      ZM_BUILDING_STYLE_SHOP,      ZM_BUILDING_PALETTE_EARTH, ZM_ROOF_FLAT,  7.0f,  6.0f,  3.0f, 0.70f,  1u, 3u, 1u, ZM_TYPE_NONE, false,  1.30f, 2.20f },
		{ ZM_BUILDING_HOUSE_CHALET_WARM,     "ChaletWarm",     ZM_BUILDING_STYLE_CHALET,    ZM_BUILDING_PALETTE_WARM,  ZM_ROOF_GABLE, 6.0f,  6.0f,  3.0f, 0.70f,  2u, 2u, 2u, ZM_TYPE_NONE, false,  1.30f, 2.20f },
		{ ZM_BUILDING_HOUSE_CHALET_COOL,     "ChaletCool",     ZM_BUILDING_STYLE_CHALET,    ZM_BUILDING_PALETTE_COOL,  ZM_ROOF_GABLE, 6.0f,  6.0f,  3.0f, 0.70f,  2u, 2u, 2u, ZM_TYPE_NONE, false,  1.30f, 2.20f },
		{ ZM_BUILDING_HOUSE_CHALET_EARTH,    "ChaletEarth",    ZM_BUILDING_STYLE_CHALET,    ZM_BUILDING_PALETTE_EARTH, ZM_ROOF_GABLE, 6.0f,  6.0f,  3.0f, 0.70f,  2u, 2u, 2u, ZM_TYPE_NONE, false,  1.30f, 2.20f },

		// --- Named story buildings ---
		{ ZM_BUILDING_PLAYER_HOME,           "PlayerHome",     ZM_BUILDING_STYLE_COTTAGE,   ZM_BUILDING_PALETTE_WARM,  ZM_ROOF_GABLE,16.50f, 12.50f,  3.00f, 0.42f,  1u, 4u, 1u, ZM_TYPE_NONE, true,   4.00f, 2.50f },
		{ ZM_BUILDING_LAB,                   "Lab",            ZM_BUILDING_STYLE_CIVIC,     ZM_BUILDING_PALETTE_COOL,  ZM_ROOF_FLAT, 20.50f, 16.50f,  3.50f, 0.70f,  1u, 5u, 1u, ZM_TYPE_NONE, true,   6.00f, 3.00f },

		// --- 8 gyms (m_eThemeType == the gym leader's element type) ---
		{ ZM_BUILDING_GYM_1,                 "GymGrass",       ZM_BUILDING_STYLE_GYMHALL,   ZM_BUILDING_PALETTE_EARTH, ZM_ROOF_HIP,  12.0f, 14.0f,  3.0f, 0.70f,  2u, 4u, 2u, ZM_TYPE_GRASS, false,  1.30f, 2.20f },
		{ ZM_BUILDING_GYM_2,                 "GymFire",        ZM_BUILDING_STYLE_GYMHALL,   ZM_BUILDING_PALETTE_WARM,  ZM_ROOF_HIP,  12.0f, 14.0f,  3.0f, 0.70f,  2u, 4u, 2u, ZM_TYPE_FIRE, false,  1.30f, 2.20f },
		{ ZM_BUILDING_GYM_3,                 "GymWater",       ZM_BUILDING_STYLE_GYMHALL,   ZM_BUILDING_PALETTE_COOL,  ZM_ROOF_HIP,  12.0f, 14.0f,  3.0f, 0.70f,  2u, 4u, 2u, ZM_TYPE_WATER, false,  1.30f, 2.20f },
		{ ZM_BUILDING_GYM_4,                 "GymElectric",    ZM_BUILDING_STYLE_GYMHALL,   ZM_BUILDING_PALETTE_WARM,  ZM_ROOF_HIP,  12.0f, 14.0f,  3.0f, 0.70f,  2u, 4u, 2u, ZM_TYPE_ELECTRIC, false,  1.30f, 2.20f },
		{ ZM_BUILDING_GYM_5,                 "GymSky",         ZM_BUILDING_STYLE_GYMHALL,   ZM_BUILDING_PALETTE_COOL,  ZM_ROOF_HIP,  12.0f, 14.0f,  3.0f, 0.70f,  2u, 4u, 2u, ZM_TYPE_SKY, false,  1.30f, 2.20f },
		{ ZM_BUILDING_GYM_6,                 "GymPhantom",     ZM_BUILDING_STYLE_GYMHALL,   ZM_BUILDING_PALETTE_COOL,  ZM_ROOF_HIP,  12.0f, 14.0f,  3.0f, 0.70f,  2u, 4u, 2u, ZM_TYPE_PHANTOM, false,  1.30f, 2.20f },
		{ ZM_BUILDING_GYM_7,                 "GymIce",         ZM_BUILDING_STYLE_GYMHALL,   ZM_BUILDING_PALETTE_COOL,  ZM_ROOF_HIP,  12.0f, 14.0f,  3.0f, 0.70f,  2u, 4u, 2u, ZM_TYPE_ICE, false,  1.30f, 2.20f },
		{ ZM_BUILDING_GYM_8,                 "GymDrake",       ZM_BUILDING_STYLE_GYMHALL,   ZM_BUILDING_PALETTE_EARTH, ZM_ROOF_HIP,  12.0f, 14.0f,  3.0f, 0.70f,  2u, 4u, 2u, ZM_TYPE_DRAKE, false,  1.30f, 2.20f },

		// --- Civic service buildings ---
		{ ZM_BUILDING_CARE_CENTER,           "CareCenter",     ZM_BUILDING_STYLE_CIVIC,     ZM_BUILDING_PALETTE_WARM,  ZM_ROOF_HIP,  10.0f,  8.0f,  3.0f, 0.70f,  1u, 3u, 1u, ZM_TYPE_NONE, false,  1.30f, 2.20f },
		{ ZM_BUILDING_TRADE_POST,            "TradePost",      ZM_BUILDING_STYLE_SHOP,      ZM_BUILDING_PALETTE_COOL,  ZM_ROOF_FLAT, 10.0f,  8.0f,  3.0f, 0.70f,  1u, 3u, 1u, ZM_TYPE_NONE, false,  1.30f, 2.20f },
		{ ZM_BUILDING_LEAGUE,                "League",         ZM_BUILDING_STYLE_CIVIC,     ZM_BUILDING_PALETTE_COOL,  ZM_ROOF_HIP,  14.0f, 12.0f,  3.0f, 0.70f,  2u, 4u, 2u, ZM_TYPE_NONE, false,  1.30f, 2.20f },
		{ ZM_BUILDING_BATTLE_TOWER,          "BattleTower",    ZM_BUILDING_STYLE_CIVIC,     ZM_BUILDING_PALETTE_COOL,  ZM_ROOF_FLAT,  8.0f,  8.0f,  3.0f, 0.70f,  3u, 2u, 3u, ZM_TYPE_NONE, false,  1.30f, 2.20f },

		// --- Civic fillers ---
		{ ZM_BUILDING_TOWNHALL,              "TownHall",       ZM_BUILDING_STYLE_CIVIC,     ZM_BUILDING_PALETTE_WARM,  ZM_ROOF_HIP,  10.0f,  8.0f,  3.0f, 0.70f,  2u, 3u, 2u, ZM_TYPE_NONE, false,  1.30f, 2.20f },
		{ ZM_BUILDING_WAREHOUSE,             "Warehouse",      ZM_BUILDING_STYLE_CIVIC,     ZM_BUILDING_PALETTE_EARTH, ZM_ROOF_FLAT, 12.0f, 10.0f,  3.0f, 0.70f,  1u, 2u, 1u, ZM_TYPE_NONE, false,  1.30f, 2.20f },
		{ ZM_BUILDING_GATEHOUSE,             "Gatehouse",      ZM_BUILDING_STYLE_CIVIC,     ZM_BUILDING_PALETTE_EARTH, ZM_ROOF_GABLE, 6.0f,  6.0f,  3.0f, 0.70f,  1u, 1u, 1u, ZM_TYPE_NONE, false,  1.30f, 2.20f },
		{ ZM_BUILDING_DOCKSHED,              "DockShed",       ZM_BUILDING_STYLE_CIVIC,     ZM_BUILDING_PALETTE_COOL,  ZM_ROOF_FLAT, 10.0f,  6.0f,  3.0f, 0.70f,  1u, 2u, 1u, ZM_TYPE_NONE, false,  1.30f, 2.20f },
	};
}

const ZM_BuildingData& ZM_GetBuildingData(ZM_BUILDING_ID eId)
{
	Zenith_Assert(eId < ZM_BUILDING_COUNT, "ZM_GetBuildingData: building id out of range (%u)", (u_int)eId);
	return s_axBuildings[eId];
}

u_int ZM_GetBuildingCount()
{
	return ZM_BUILDING_COUNT;
}

const char* ZM_GetBuildingName(ZM_BUILDING_ID eId)
{
	if (eId >= ZM_BUILDING_COUNT)
	{
		return "NONE";
	}
	return s_axBuildings[eId].m_szName;
}
