#pragma once

// Generate all test assets (StickFigure and ProceduralTree)
// Called from main() before unit tests to ensure assets exist
extern void GenerateTestAssets();

// Generate StickFigure humanoid test assets
// Creates: skeleton, mesh, mesh geometry, static mesh, 9 animations
// Output: ENGINE_ASSETS_DIR/Meshes/StickFigure/
extern void GenerateStickFigureAssets();

// Generate ProceduralTree test assets
// Creates: skeleton, mesh, mesh geometry, static mesh, VAT, sway animation
// Output: ENGINE_ASSETS_DIR/Meshes/ProceduralTree/
extern void GenerateProceduralTreeAssets();
