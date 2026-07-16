#pragma once

#include "Zenithmon/Source/Data/ZM_Types.h"   // ZM_TYPE (+ u_int etc.; mirrors ZM_HumanData's data-core include)

// ============================================================================
// ZM_BuildingData -- the building/prop roster table (S4 ZM_BuildingGen data core).
//
// Buildings are STATIC models (NO skeleton, NO animation), unlike the skinned +
// animated creatures/humans. This is the STRUCTURAL roster of the 30 building
// models the town needs: the ~12 generic houses (4 styles x 3 palettes), the two
// named story buildings (player home + lab), the 8 type-themed gyms, the four
// civic service buildings, and four civic fillers. Per-model variation lives in
// the parametric shell + facade texture driven by each row's fields.
//
// This file MIRRORS ZM_HumanData exactly: a save-stable APPEND-ONLY enum
// (ZM_BUILDING_ID), a compiled `const ZM_BuildingData` C array (zero file I/O in
// headless tests, DecisionLog ZM-D-009 precedent), and the
// ZM_GetBuildingData/ZM_GetBuildingName accessor idiom. The ZM_BUILDING_ID order
// is save-stable once content ships -- APPEND before ZM_BUILDING_COUNT, never
// reorder.
// ============================================================================

// Building silhouette family -- drives the (SC2) parametric shell. APPEND-only +
// save-stable; ZM_BUILDING_STYLE_COUNT is the sentinel, never a stored value.
enum ZM_BUILDING_STYLE : u_int
{
	ZM_BUILDING_STYLE_COTTAGE,
	ZM_BUILDING_STYLE_TOWNHOUSE,
	ZM_BUILDING_STYLE_SHOP,
	ZM_BUILDING_STYLE_CHALET,
	ZM_BUILDING_STYLE_CIVIC,
	ZM_BUILDING_STYLE_GYMHALL,

	ZM_BUILDING_STYLE_COUNT
};

// Facade colour family -- drives the (SC1 solid / SC3 decal) facade base colour.
// APPEND-only + save-stable.
enum ZM_BUILDING_PALETTE : u_int
{
	ZM_BUILDING_PALETTE_WARM,
	ZM_BUILDING_PALETTE_COOL,
	ZM_BUILDING_PALETTE_EARTH,

	ZM_BUILDING_PALETTE_COUNT
};

// Roof shape -- authored by the (SC2) shell. APPEND-only + save-stable.
enum ZM_ROOF_KIND : u_int
{
	ZM_ROOF_GABLE,
	ZM_ROOF_HIP,
	ZM_ROOF_FLAT,

	ZM_ROOF_KIND_COUNT
};

// Every building model, in roster order. IDs are contiguous 0..ZM_BUILDING_COUNT-1
// and save-stable -- APPEND before ZM_BUILDING_COUNT, never reorder.
enum ZM_BUILDING_ID : u_int
{
	// 4 house styles x 3 palettes = 12
	ZM_BUILDING_HOUSE_COTTAGE_WARM, ZM_BUILDING_HOUSE_COTTAGE_COOL, ZM_BUILDING_HOUSE_COTTAGE_EARTH,
	ZM_BUILDING_HOUSE_TOWNHOUSE_WARM, ZM_BUILDING_HOUSE_TOWNHOUSE_COOL, ZM_BUILDING_HOUSE_TOWNHOUSE_EARTH,
	ZM_BUILDING_HOUSE_SHOP_WARM, ZM_BUILDING_HOUSE_SHOP_COOL, ZM_BUILDING_HOUSE_SHOP_EARTH,
	ZM_BUILDING_HOUSE_CHALET_WARM, ZM_BUILDING_HOUSE_CHALET_COOL, ZM_BUILDING_HOUSE_CHALET_EARTH,

	ZM_BUILDING_PLAYER_HOME, ZM_BUILDING_LAB,

	// 8 gyms -- their m_eThemeType MUST equal the SHIPPED 8 gym leaders' types
	// (Fenna=GRASS, Bram=FIRE, Maris=WATER, Tessa=ELECTRIC, Aquilo=SKY,
	// Morwenna=PHANTOM, Halvard=ICE, Vardis=DRAKE -- ZM_HumanData.h GDD 3.4).
	ZM_BUILDING_GYM_1, ZM_BUILDING_GYM_2, ZM_BUILDING_GYM_3, ZM_BUILDING_GYM_4,
	ZM_BUILDING_GYM_5, ZM_BUILDING_GYM_6, ZM_BUILDING_GYM_7, ZM_BUILDING_GYM_8,

	ZM_BUILDING_CARE_CENTER, ZM_BUILDING_TRADE_POST, ZM_BUILDING_LEAGUE, ZM_BUILDING_BATTLE_TOWER,

	// 4 civic fillers -> 30 total
	ZM_BUILDING_TOWNHALL, ZM_BUILDING_WAREHOUSE, ZM_BUILDING_GATEHOUSE, ZM_BUILDING_DOCKSHED,

	ZM_BUILDING_COUNT,
	ZM_BUILDING_NONE = ZM_BUILDING_COUNT   // "no building" sentinel
};

// One roster row. m_szName is the asset STEM (the folder + file basename under
// game:Buildings/<Name>/...). The style/palette/roof/dims/window fields drive the
// parametric shell + facade; m_eThemeType is ZM_TYPE_NONE except for gyms, whose
// theme MUST equal the corresponding gym leader's type.
struct ZM_BuildingData
{
	ZM_BUILDING_ID      m_eId;
	const char*         m_szName;        // asset stem, PascalCase, e.g. "GymFire" / "CareCenter"
	ZM_BUILDING_STYLE   m_eStyle;
	ZM_BUILDING_PALETTE m_ePalette;
	ZM_ROOF_KIND        m_eRoof;
	float               m_fWidth;
	float               m_fDepth;
	u_int               m_uStoreys;
	u_int               m_uWindowCols;
	u_int               m_uWindowRows;
	ZM_TYPE             m_eThemeType;    // ZM_TYPE_NONE except gyms
};

// Table accessors (bounds-asserted). ZM_GetBuildingData indexes by ZM_BUILDING_ID.
const ZM_BuildingData&	ZM_GetBuildingData(ZM_BUILDING_ID eId);
u_int					ZM_GetBuildingCount();				// == ZM_BUILDING_COUNT
const char*				ZM_GetBuildingName(ZM_BUILDING_ID eId);
