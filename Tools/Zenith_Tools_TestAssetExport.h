#pragma once

// Generate all test assets (StickFigure and ProceduralTree)
// Called from main() before unit tests to ensure assets exist
extern void GenerateTestAssets();

// Generate StickFigure human test assets
// Creates: skeleton (16-bone rig, unchanged layout), smooth lofted body mesh,
// mesh geometry, static mesh, painted texture atlas (albedo/normal/RM),
// body material, model bundle, and 13 animations
// Output: ENGINE_ASSETS_DIR/Meshes/StickFigure/
extern void GenerateStickFigureAssets();

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
