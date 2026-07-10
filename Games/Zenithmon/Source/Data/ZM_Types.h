#pragma once

// ============================================================================
// ZM_Types -- the 18 elemental types (S1 data core).
//
// Original names over mainline mechanics (Docs/Scope.md: zero Nintendo IP).
// The effectiveness relationships between these types live in ZM_TypeChart;
// see Docs/GameDesignDocument.md section 6 for the type list and design intent.
//
// Data tables are compiled const C arrays, not disk assets (DecisionLog
// ZM-D-009), so this enum is the single source of truth for the type space:
// its order is the dex/UI order AND the row/column order of the type chart.
// ============================================================================

// A monster's / move's element. The 0..ZM_TYPE_COUNT range is save-stable once
// content ships -- APPEND new types before ZM_TYPE_COUNT, never reorder, or old
// saves and baked data reinterpret their types.
enum ZM_TYPE : u_int
{
	ZM_TYPE_NORMAL,
	ZM_TYPE_FIRE,
	ZM_TYPE_WATER,
	ZM_TYPE_GRASS,
	ZM_TYPE_ELECTRIC,
	ZM_TYPE_ICE,
	ZM_TYPE_BRAWL,
	ZM_TYPE_VENOM,
	ZM_TYPE_EARTH,
	ZM_TYPE_SKY,
	ZM_TYPE_MIND,
	ZM_TYPE_SWARM,
	ZM_TYPE_STONE,
	ZM_TYPE_PHANTOM,
	ZM_TYPE_DRAKE,
	ZM_TYPE_UMBRAL,
	ZM_TYPE_IRON,
	ZM_TYPE_FEY,

	ZM_TYPE_COUNT,                 // 18 real types; use as the array-size sentinel

	ZM_TYPE_NONE = ZM_TYPE_COUNT   // empty second-type slot; never a chart index
};

// Human-readable name (uppercase, matching the enum). Returns "NONE" for the
// empty slot and "INVALID" out of range -- for logs, tooltips, and dex text.
const char* ZM_TypeToString(ZM_TYPE eType);
