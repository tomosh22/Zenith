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
