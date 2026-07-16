#pragma once

#include "Zenithmon/Source/Data/ZM_Types.h"   // ZM_TYPE (+ u_int etc.; mirrors ZM_BuildingData's data-core include)

// ============================================================================
// ZM_PropData -- the prop roster table (S4 ZM_PropGen data core).
//
// Props are STATIC models (NO skeleton, NO animation), like buildings and unlike
// the skinned + animated creatures/humans. This is the STRUCTURAL roster of the
// ~25 dressing/set-piece models the town + battle-dome need: fences, signs,
// lamps, bridges, ledges, rocks, furniture, and the 6 per-biome battle-dome
// dressing sets. Per-model variation lives in the box composition (SC-local) +
// the placeholder albedo driven by each row's fields.
//
// This file MIRRORS ZM_BuildingData exactly: a save-stable APPEND-ONLY enum
// (ZM_PROP_ID), a compiled `const ZM_PropData` C array (zero file I/O in headless
// tests, DecisionLog ZM-D-009 precedent), and the ZM_GetPropData/ZM_GetPropCount/
// ZM_GetPropName accessor idiom. The ZM_PROP_ID order is save-stable once content
// ships -- APPEND before ZM_PROP_COUNT, never reorder.
// ============================================================================

// Prop silhouette family -- drives the (SC4) box composition. APPEND-only +
// save-stable; ZM_PROP_KIND_COUNT is the sentinel, never a stored value.
enum ZM_PROP_KIND : u_int
{
	ZM_PROP_KIND_FENCE,
	ZM_PROP_KIND_SIGN,
	ZM_PROP_KIND_LAMP,
	ZM_PROP_KIND_BRIDGE,
	ZM_PROP_KIND_LEDGE,
	ZM_PROP_KIND_ROCK,
	ZM_PROP_KIND_FURNITURE,
	ZM_PROP_KIND_DRESSING,

	ZM_PROP_KIND_COUNT
};

// Material/colour family -- drives the (SC4 solid) albedo base colour. APPEND-only
// + save-stable.
enum ZM_PROP_PALETTE : u_int
{
	ZM_PROP_PALETTE_WOOD,
	ZM_PROP_PALETTE_STONE,
	ZM_PROP_PALETTE_METAL,
	ZM_PROP_PALETTE_PAINTED,
	ZM_PROP_PALETTE_FOLIAGE,

	ZM_PROP_PALETTE_COUNT
};

// Battle-dome biome tag -- ZM_PROP_BIOME_NONE for the generic town props; the 6
// dressing sets each carry a real biome. APPEND-only + save-stable.
enum ZM_PROP_BIOME : u_int
{
	ZM_PROP_BIOME_NONE,
	ZM_PROP_BIOME_MEADOW,
	ZM_PROP_BIOME_VOLCANIC,
	ZM_PROP_BIOME_COAST,
	ZM_PROP_BIOME_WETLAND,
	ZM_PROP_BIOME_SNOW,
	ZM_PROP_BIOME_CANYON,

	ZM_PROP_BIOME_COUNT
};

// The first REAL (non-NONE) biome -- the biome-coverage gate iterates
// [uZM_PROP_BIOME_FIRST_REAL, ZM_PROP_BIOME_COUNT).
constexpr u_int uZM_PROP_BIOME_FIRST_REAL = ZM_PROP_BIOME_MEADOW;

// Every prop model, in roster order. IDs are contiguous 0..ZM_PROP_COUNT-1 and
// save-stable -- APPEND before ZM_PROP_COUNT, never reorder.
enum ZM_PROP_ID : u_int
{
	ZM_PROP_FENCE_WOOD, ZM_PROP_FENCE_STONE,
	ZM_PROP_SIGN_POST, ZM_PROP_TOWN_BOARD,
	ZM_PROP_LAMP_POST, ZM_PROP_LANTERN_POST,
	ZM_PROP_BRIDGE_PLANK, ZM_PROP_BRIDGE_STONE,
	ZM_PROP_LEDGE_LOW, ZM_PROP_LEDGE_HIGH,
	ZM_PROP_ROCK_SMALL, ZM_PROP_ROCK_LARGE, ZM_PROP_BOULDER,
	ZM_PROP_TABLE, ZM_PROP_CHAIR, ZM_PROP_BED, ZM_PROP_SHELF, ZM_PROP_COUNTER, ZM_PROP_BARREL,

	// 6 battle-dome biome dressing sets -> 25 total
	ZM_PROP_DRESSING_MEADOW, ZM_PROP_DRESSING_VOLCANIC, ZM_PROP_DRESSING_COAST,
	ZM_PROP_DRESSING_WETLAND, ZM_PROP_DRESSING_SNOW, ZM_PROP_DRESSING_CANYON,

	ZM_PROP_COUNT,
	ZM_PROP_NONE = ZM_PROP_COUNT   // "no prop" sentinel
};

// One roster row. m_szName is the asset STEM (the folder + file basename under
// game:Props/<Name>/...). The kind/biome/palette/dims fields drive the SC4 box
// composition + the placeholder albedo. m_eBiome is ZM_PROP_BIOME_NONE except for
// the DRESSING rows, whose biome tags the battle-dome set they belong to.
struct ZM_PropData
{
	ZM_PROP_ID      m_eId;
	const char*     m_szName;    // asset stem, PascalCase, e.g. "LampPost" / "DressingMeadow"
	ZM_PROP_KIND    m_eKind;
	ZM_PROP_BIOME   m_eBiome;
	ZM_PROP_PALETTE m_ePalette;
	float           m_fWidth;
	float           m_fDepth;
	float           m_fHeight;
};

// Table accessors (bounds-asserted). ZM_GetPropData indexes by ZM_PROP_ID.
const ZM_PropData&	ZM_GetPropData(ZM_PROP_ID eId);
u_int				ZM_GetPropCount();				// == ZM_PROP_COUNT
const char*			ZM_GetPropName(ZM_PROP_ID eId);
