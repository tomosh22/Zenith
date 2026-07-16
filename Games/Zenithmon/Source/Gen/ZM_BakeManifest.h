#pragma once

// ============================================================================
// ZM_BakeManifest -- the S4 per-family bake GUARD. Each asset family
// (creatures/humans/buildings/props) has a tools-only ZM_BakeAll* orchestrator;
// this box stamps each family with a deterministic 12-byte manifest (generator
// version + expected-file count) written atomically to game:<Family>/.manifest.
// A re-bake is SKIPPED when the stamp is current AND every expected file is
// present non-empty, and re-RUN when the generator version bumped or any file is
// missing. The check is FAIL-OPEN: any doubt (missing/short stamp, absent file,
// filesystem error) returns "not warm" so the family is re-baked.
//
// Marker shape mirrors the terrain manifest (ZM_TerrainAuthoring's ZMTR marker):
// 4-byte magic + u32-LE version + u32-LE count, temp+rename finalize, is-regular-
// -file/size>0 checks with std::error_code fail-closed. The stamp is a pure
// function of (version, count) -- NO timestamps or paths -- so it never perturbs
// the byte-identical re-bake invariant the tests pin.
//
// GUARD MODEL (mirrors ZM_GenCommon / the four gen headers): the read/enumerate
// API compiles in ALL configs so the in-memory ZM_Gen unit gate exercises it
// headless. Only the disk write + the ZM_BakeAllAssets orchestrator are #ifdef
// ZENITH_TOOLS, with non-tools no-ops so _False builds link. This header is
// deliberately LEAN -- it does NOT include the four gen headers (that would cycle
// through their includes); the .cpp includes them for the enumerate/version bodies.
// ============================================================================

#include <string>
#include <filesystem>
#include "Collections/Zenith_Vector.h"

// The four procedural asset families, each with its own ZM_BakeAll* orchestrator.
enum ZM_ASSET_FAMILY : u_int
{
	ZM_ASSET_FAMILY_CREATURES,
	ZM_ASSET_FAMILY_HUMANS,
	ZM_ASSET_FAMILY_BUILDINGS,
	ZM_ASSET_FAMILY_PROPS,
	ZM_ASSET_FAMILY_COUNT
};

// ---- all-config read / enumerate ------------------------------------------

// The family's generator version (uZM_CREATUREGEN_VERSION / uZM_HUMANGEN_VERSION
// / uZM_BUILDINGGEN_VERSION / uZM_PROPGEN_VERSION). This module only READS it.
u_int ZM_BakeManifestFamilyVersion(ZM_ASSET_FAMILY eFamily);

// Push every expected "game:" asset ref for the family into xOut (cleared first),
// in a FIXED order via the per-family AssetPath functions. The COUNT is the load-
// bearing manifest quantity; the individual refs drive the file-presence check.
void ZM_EnumerateFamilyFiles(ZM_ASSET_FAMILY eFamily, Zenith_Vector<std::string>& xOut);

// The family's root asset ref (e.g. "game:Buildings"); the stamp lives at
// <root>/.manifest. Matches the folder segment the AssetPath refs live under.
const char* ZM_BakeManifestFamilyRootRef(ZM_ASSET_FAMILY eFamily);

// WARM? -- true iff the stamp under xGameAssetsRoot is current (magic + version +
// count all match) AND every enumerated file resolves to a non-empty file under
// xGameAssetsRoot. FAIL-OPEN: false on ANY doubt (so the family re-bakes). The
// xGameAssetsRoot param resolves stamp + files by stripping each ref's "game:"
// prefix and joining under the root, so the SAME path serves the real bake
// (xRoot == GAME_ASSETS_DIR) and a unit test pointing at a temp dir.
bool ZM_BakeManifestCheck(ZM_ASSET_FAMILY eFamily, const std::filesystem::path& xGameAssetsRoot);

// ---- tools-only write / orchestrate ---------------------------------------
#ifdef ZENITH_TOOLS
// Atomically (temp + rename) write the family's 12-byte stamp under
// xGameAssetsRoot: magic + current version + enumerated-file count. Returns true
// iff the stamp file exists after the rename.
bool ZM_WriteBakeManifest(ZM_ASSET_FAMILY eFamily, const std::filesystem::path& xGameAssetsRoot);

// Bake every family via the four (now guard+stamp) ZM_BakeAll* orchestrators,
// ANDing their results. NO shipped caller yet -- the S4 gallery gate wires it
// (deferred, exactly like the ZM_BakeAll* it wraps); wiring a synchronous cold
// full-family bake into the unconditional tools boot would slow every gate/CI boot.
bool ZM_BakeAllAssets();
#else
inline bool ZM_WriteBakeManifest(ZM_ASSET_FAMILY, const std::filesystem::path&) { return false; }
inline bool ZM_BakeAllAssets() { return false; }
#endif
