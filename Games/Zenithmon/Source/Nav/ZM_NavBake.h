#pragma once

#include "FileAccess/Zenith_FileAccess.h"

// ============================================================================
// ZM_NavBake -- S7 item 3 SC1b: Zenithmon as the FIRST CONSUMER of the engine's
// reusable baked-navmesh persistence feature (ZM-D-145 / ZM-D-147).
//
// SC1 (ZM_NavEval, beside this file) answered "can the generator ingest a
// Dawnmere-scale ground surface?" -- yes, in memory, writing nothing. THIS file
// is the persistence half: a tools-time step that feeds SC1's coverage grid to
// Zenith_NavMeshBaker and writes a COMMITTED `Dawnmere.znavmesh`, which
// Zenith_NavMeshComponent loads at runtime on every boot -- windowed, headless
// or packaged.
//
// Why that matters: the baked file is loadable with NO terrain component, NO
// GPU and NO gitignored asset, so navigation becomes CI-verifiable for the first
// time. It is also why the bake never runs on a Null (headless/CI) boot: CI
// loads the COMMITTED bytes, and a null backend authoring render content would
// only produce garbage and churn the tracked asset (D8).
//
// The whole orchestration -- generate, serialize, write, verify -- lives in the
// ENGINE (Zenith_NavMeshBaker); this file supplies only Dawnmere's geometry and
// its two constants.
// ============================================================================

// The bake cell size. 16 m is SC1's evaluated resolution: it lands the polygon
// count in the asserted 1365..1677 band and stays far clear of the generator's
// iMaxDim=1024 voxel-grid clamp. A finer re-bake is a DATA-only change later
// (Shortfalls E7 / option B); nothing in the pipeline hard-codes 16 elsewhere.
constexpr float fZM_DAWNMERE_NAVMESH_CELL_SIZE = 16.0f;

// The runtime asset ref authored onto Dawnmere's Zenith_NavMeshComponent. The
// `game:` prefix is resolved by Zenith_AssetRegistry, so a packaged build with
// --assets-root works with no extra wiring.
constexpr const char* szZM_DAWNMERE_NAVMESH_ASSET_REF = "game:Navmesh/Dawnmere" ZENITH_NAVMESH_EXT;

// The absolute destination the tools-time bake writes. The parent directory is
// created by the file layer, so the first bake into a fresh Assets/Navmesh/
// needs no mkdir.
const char* ZM_GetDawnmereNavmeshBakePath();

#ifdef ZENITH_TOOLS
// Zenith_EditorAutomation step (a captureless void(*)()): harvest Dawnmere's
// flat coverage grid at fZM_DAWNMERE_NAVMESH_CELL_SIZE and bake it to
// ZM_GetDawnmereNavmeshBakePath().
//
// Runs on EVERY windowed tools boot -- it needs no terrain, no scene and no
// disk asset, only the const recipe table -- and is skipped entirely on a Null
// build (see the header comment). Fails LOUDLY: a bake that does not produce
// the expected walkable mesh asserts, because the committed asset is what CI
// then tests against.
void ZM_BakeDawnmereNavmeshStep();
#endif
