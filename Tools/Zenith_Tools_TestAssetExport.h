#pragma once

#include <string>

// Generate all test assets (StickFigure and ProceduralTree)
// Called from main() before unit tests to ensure assets exist
extern void GenerateTestAssets();

// Generate StickFigure human test assets
// Creates: skeleton (51-bone T-posed rig), smooth lofted body mesh, mesh
// geometry, static mesh, painted texture atlas (albedo/normal/RM/AO/height),
// body + eye materials, the model bundle, and the Blender round-trip .gltf
// (MESH + RIG ONLY -- it must carry no animations, see the .cpp).
// Output: ENGINE_ASSETS_DIR/Meshes/StickFigure/
//
// ★ IT WRITES NO CLIPS, AND DELETES ANY IT FINDS IN THAT DIRECTORY. The
// seventeen StickFigure animations are AUTHORED data (WU-9.1): committed under
// Assets/Authored/Meshes/StickFigure/, hand-edited in the Animation Editor, and
// produced by nothing here. A .zanim under Meshes/StickFigure/ is a generated
// SHADOW of a tracked file, so the exporter sweeps the directory clear of them
// on every boot. See the clip-name table below.
extern void GenerateStickFigureAssets();

//=============================================================================
// THE SEVENTEEN AUTHORED StickFigure CLIPS (WU-9.1).
//
// ★ THESE FILES ARE AUTHORED, NOT BAKED, AND THAT IS THE WHOLE POINT. They live
// under `Zenith/Assets/Authored/Meshes/StickFigure/`, which `.gitignore`
// re-includes wholesale and `.gitattributes` marks `binary -filter`, so they are
// COMMITTED as real bytes. No generator writes one: deleting one does not
// regenerate it, it destroys it (D21). A schema bump carries them forward
// through `Zenith_Tools_MigrateAuthoredClipsAtBoot()` instead of a re-bake.
//
// What lives here is the NAME LIST -- which files the set is -- published so that
// "the StickFigure set is these seventeen" is stated ONCE. The unit suite walks
// it to check every tracked file; the exporter reads no clip at all.
//=============================================================================

constexpr u_int uZENITH_STICKFIGURE_CLIP_COUNT = 17u;

// Clip names WITHOUT the "StickFigure_" prefix or the extension -- "Idle",
// "Walk", ... -- in the order the set has always been listed. Each name is both
// the clip's own m_strName and the middle of its file name.
extern const char* const azZENITH_STICKFIGURE_CLIP_NAMES[uZENITH_STICKFIGURE_CLIP_COUNT];

// PURE. The file name for one clip: "Idle" -> "StickFigure_Idle.zanim".
extern std::string Zenith_Tools_StickFigureClipFileName(const char* szClipName);

// PURE. The authored ASSET PATH for one clip FILE NAME:
//   "StickFigure_Idle.zanim" -> "engine:Authored/Meshes/StickFigure/StickFigure_Idle.zanim"
//
// ★ THE RULE IS Zenith_AnimationDocument::BuildAuthoredAssetPath's, and it is
// matched rather than called -- Tools may not include Editor. The source's ROOT
// PREFIX is kept and "Authored/" is inserted directly under it, preserving the
// SUBDIRECTORY: two asset sets routinely both hold a clip called "Walk", and a
// flattened leaf name would have one silently overwrite the other.
extern std::string Zenith_Tools_StickFigureAuthoredPath(const char* szClipFileName);

// The on-disk directory those files live in -- ENGINE_ASSETS_DIR + the same
// relative path Zenith_Tools_StickFigureAuthoredPath spells after "engine:", so
// the two describe one location. ENGINE_ASSETS_DIR is a define on the engine
// library this file compiles into; the game root (GAME_ASSETS_DIR) does not
// exist here, and no StickFigure clip lives under one.
extern std::string Zenith_Tools_StickFigureAuthoredDir();

// Generate ProceduralTree test assets
// Creates: skeleton, mesh, mesh geometry, static mesh, VAT, sway animation
// Output: ENGINE_ASSETS_DIR/Meshes/ProceduralTree/
extern void GenerateProceduralTreeAssets();

// Generate the SHARED procedural rock set
// Creates: 4 stone meshes (boulder / slab / shard / pebble cluster) as
// .zasset + .zmesh + .zmodel, plus granite and sandstone PBR texture sets
// (albedo / normal / RM / AO) and their materials
// Output: ENGINE_ASSETS_DIR/Meshes/Rocks/
extern void GenerateProceduralRockAssets();

// Generate the SHARED deadwood set
// Creates: 4 pieces (fallen log / mossy log / broken stump / branch tangle) as
// .zasset + .zmesh + .zmodel, plus bark and mossy-bark PBR texture sets
// (albedo / normal / RM / AO) and their materials
// Output: ENGINE_ASSETS_DIR/Meshes/FallenTrees/
extern void GenerateFallenTreeAssets();

// Generate the SHARED wind-animated bush set
// Creates: 3 foliage bushes (broad shrub / low mound / spindly upright), each
// as .zasset + .zmesh + .zskel + a sway VAT (.zanmt), plus the masked foliage
// albedo texture and material. One instance group per bush (all alpha-tested
// foliage; no opaque stem half) -- see the .cpp header for why.
// Output: ENGINE_ASSETS_DIR/Meshes/Bushes/
extern void GenerateBushAssets();

// Generate the SHARED grass texture set + the authored grass type table
// Creates: engine Vegetation/Grass_Blade_{Vein,Gloss} + Grass_Clump_Ramp
// .ztxtr, plus game:Vegetation/GrassTypes.zdata (4 types binding them) guarded
// by a GrassTypes.gen version marker so a hand-authored table is never
// clobbered -- see the .cpp header.
// Output: ENGINE_ASSETS_DIR/Vegetation/ + GAME_ASSETS_DIR/Vegetation/
extern void GenerateGrassAssets();

// Generate RenderTest game-specific assets (bullet sphere mesh + model)
// Output: GAME_ASSETS_DIR/Meshes/Bullet_Sphere.{zasset,zmodel}
extern void GenerateRenderTestAssets();

#ifdef ZENITH_TOOLS
class Flux_MeshGeometry;
class Zenith_MeshAsset;
class Zenith_SkeletonAsset;

// Mesh-asset -> Flux_MeshGeometry converters shared by the StickFigure and
// ProceduralTree generators (defined in Zenith_Tools_TestAssetExport.cpp).
// Caller owns the returned geometry.
extern Flux_MeshGeometry* Zenith_Tools_CreateFluxMeshGeometry(const Zenith_MeshAsset* pxMeshAsset, const Zenith_SkeletonAsset* pxSkeleton);
extern Flux_MeshGeometry* Zenith_Tools_CreateStaticFluxMeshGeometry(const Zenith_MeshAsset* pxMeshAsset);
#endif
