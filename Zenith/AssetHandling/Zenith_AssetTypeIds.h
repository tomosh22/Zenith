#pragma once

// u_int et al. come from the precompiled header (Core/Zenith.h), included first in
// every TU — matching Zenith_StreamEnvelope.h, which uses u_int with no types include.

// ============================================================================
// Zenith_AssetTypeIds - the single source of truth for the per-typed-asset
// envelope identity (Zenith_StreamEnvelope's uAssetTypeId) and the current
// on-disk payload schema version of each typed binary asset.
//
// Every typed binary asset (.ztxtr / .zmtrl / .zmesh / .zskel / .zmodel / .zanim)
// prefixes its DataStream payload with a Zenith_StreamHeader (magic + envelope
// version + THIS asset-type-id + THIS schema version). The reader validates the id
// (a mismatch is a wrong-type file) and takes the schema from the header.
//
// THE ENVELOPE IS MANDATORY AND THERE IS NO LEGACY BRANCH (ruling 2026-08-31, see
// Zenith_StreamEnvelope.cpp): a stream carrying no envelope is REFUSED, and so is a
// payload whose schema is not this file's *_SCHEMA_CURRENT. Every asset file is
// regenerable bake output, so an older layout is a stale bake — delete it and let
// the tools boot rewrite it.
//
// Bumping a *_SCHEMA_CURRENT means the payload layout changed; never repurpose an
// existing id.
// The generic .zdata path (serializable data assets) is SEPARATE: it has its own
// ZDATA magic + string type name and does NOT use these ids.
// ============================================================================

// Asset-type ids — stable, unique per typed asset. Do NOT renumber existing ids.
inline constexpr u_int uZENITH_TEXTURE_ASSET_TYPE_ID  = 1;
inline constexpr u_int uZENITH_MATERIAL_ASSET_TYPE_ID = 2;
inline constexpr u_int uZENITH_MESH_ASSET_TYPE_ID     = 3;
inline constexpr u_int uZENITH_SKELETON_ASSET_TYPE_ID = 4;
inline constexpr u_int uZENITH_MODEL_ASSET_TYPE_ID    = 5;
inline constexpr u_int uZENITH_ANIMATION_ASSET_TYPE_ID = 6;  // .zanim (Flux_AnimationClip)
inline constexpr u_int uZENITH_ANIMCTRL_ASSET_TYPE_ID  = 7;  // .zanimctrl (Flux_AnimatorControllerDef)
inline constexpr u_int uZENITH_ANIMMASK_ASSET_TYPE_ID  = 8;  // .zanimmask (Zenith_BoneMaskAsset)

// Current on-disk payload schema versions (carried verbatim from each asset's
// historical version constant, so no schema bump / no byte-layout change).
inline constexpr u_int uZENITH_TEXTURE_SCHEMA_V2       = 2;  // texture: schema>=2 == packed mip chain
inline constexpr u_int uZENITH_MATERIAL_SCHEMA_CURRENT = 5;
inline constexpr u_int uZENITH_MESH_SCHEMA_CURRENT     = 1;
inline constexpr u_int uZENITH_SKELETON_SCHEMA_CURRENT = 2;
inline constexpr u_int uZENITH_MODEL_SCHEMA_CURRENT    = 2;
// .zanim had NO version word at all before it adopted the envelope, so its schema
// starts at 1 — the first layout that is self-describing on the wire.
//
// ★ SCHEMA 3 IS THE PER-KEY TANGENT MODES, AND IT IS ON THE WIRE (B2). A tangent
// record is 26 bytes — six floats then the two Flux_TangentMode bytes, in-mode
// first — where schemas 1-2 wrote 24 and the modes had to be DERIVED from the
// vectors on the way in. That derivation now has exactly two callers left: the
// schema-<=2 branch of Flux_ReadKeyTangents, and the migrator's 2->3 step. Every
// production write path stores the mode it was GIVEN, which is what makes
// Flux_TangentMode::FLAT and ::AUTO authorable at all.
//
// The 17 committed clips under Assets/Authored were carried across by
// Zenith_Tools_MigrateAuthoredClipsAtBoot in the same change that bumped this
// constant; every generated clip is bake output and was simply rewritten.
inline constexpr u_int uZENITH_ANIMATION_SCHEMA_CURRENT = 3;  // 3: per-key tangent MODES on the wire (2: key times became SECONDS)

// WU-6.2. Both formats are self-describing from their FIRST byte — neither ever
// existed without an envelope — so both schemas start at 1.
//
// .zanimctrl is the whole controller: an optional EMBEDDED top-level state-machine
// def plus N layers, each owning its own EMBEDDED def. The SMs embed (D46) because
// their states name THIS controller's clips and parameters; a bone MASK does not,
// because a mask is SKELETON-scoped and shared by every controller on that rig, so
// a layer carries a mask ASSET PATH and .zanimmask is its own type.
inline constexpr u_int uZENITH_ANIMCTRL_SCHEMA_CURRENT = 1;
inline constexpr u_int uZENITH_ANIMMASK_SCHEMA_CURRENT = 1;
