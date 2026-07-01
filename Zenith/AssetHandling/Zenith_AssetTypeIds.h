#pragma once

// u_int et al. come from the precompiled header (Core/Zenith.h), included first in
// every TU — matching Zenith_StreamEnvelope.h, which uses u_int with no types include.

// ============================================================================
// Zenith_AssetTypeIds - the single source of truth for the per-typed-asset
// envelope identity (Zenith_StreamEnvelope's uAssetTypeId) and the current
// on-disk payload schema version of each typed binary asset.
//
// Every typed binary asset (.ztxtr / .zmtrl / .zmesh / .zskel / .zmodel) prefixes
// its DataStream payload with a Zenith_StreamHeader (magic + envelope version +
// THIS asset-type-id + THIS schema version). The reader validates the id (a
// mismatch is a wrong-type file) and takes the schema from the header. Legacy
// pre-envelope (headerless) files still load: Zenith_ReadStreamHeader returns
// BAD_MAGIC and restores the cursor so the old bare-version-word layout reads.
//
// Bumping a *_SCHEMA_CURRENT means the payload layout changed — add a legacy read
// branch keyed on the older schema value; never repurpose an existing id.
// The generic .zdata path (serializable data assets) is SEPARATE: it has its own
// ZDATA magic + string type name and does NOT use these ids.
// ============================================================================

// Asset-type ids — stable, unique per typed asset. Do NOT renumber existing ids.
inline constexpr u_int uZENITH_TEXTURE_ASSET_TYPE_ID  = 1;
inline constexpr u_int uZENITH_MATERIAL_ASSET_TYPE_ID = 2;
inline constexpr u_int uZENITH_MESH_ASSET_TYPE_ID     = 3;
inline constexpr u_int uZENITH_SKELETON_ASSET_TYPE_ID = 4;
inline constexpr u_int uZENITH_MODEL_ASSET_TYPE_ID    = 5;

// Current on-disk payload schema versions (carried verbatim from each asset's
// historical version constant, so no schema bump / no byte-layout change).
inline constexpr u_int uZENITH_TEXTURE_SCHEMA_V2       = 2;  // texture: schema>=2 == packed mip chain
inline constexpr u_int uZENITH_MATERIAL_SCHEMA_CURRENT = 5;
inline constexpr u_int uZENITH_MESH_SCHEMA_CURRENT     = 1;
inline constexpr u_int uZENITH_SKELETON_SCHEMA_CURRENT = 2;
inline constexpr u_int uZENITH_MODEL_SCHEMA_CURRENT    = 2;
