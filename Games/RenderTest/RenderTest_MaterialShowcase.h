#pragma once

// Material-showcase testbed for RenderTest.
//
// A wide platform north of the player carrying a 5-row grid of every procedural
// shape (sphere / cube / cylinder / cone / capsule) painted with a wide matrix of
// materials: roughness/metallic ladders, coloured metals, specular F0, clear coat,
// HDR emissive, unlit, normal maps, detail/texture, alpha cutout, translucency,
// additive, two-sided, and parent/instance overrides.
//
// This used to be spawned procedurally post scene-load (GPU meshes/textures built
// at runtime, never serialized). It is now baked OFFLINE (tools) exactly like the
// tennis / guns / jetpack testbeds:
//
//   .zmesh   (Zenith_MeshAsset)     - one per shape, CPU geometry, no GPU upload
//   .ztxtr   (Zenith_TextureAsset)  - the procedural normal / checker / cutout maps
//   .zmtrl   (Zenith_MaterialAsset) - one per cell (+ the shared instance parent)
//   .zmodel  (Zenith_ModelAsset)    - one per cell, bundling its shape mesh + material
//
// The authored scene (RenderTest.cpp) LoadModels these and creates the platform +
// per-shape cell entities itself, each with a STATIC per-primitive physics collider
// (sphere -> SPHERE, capsule -> explicit CAPSULE, everything else -> OBB) so the
// player can stand on the platform and bump into (not through) the shapes.

class Zenith_EditorAutomation;

#ifdef ZENITH_TOOLS
// Bake every showcase texture / mesh / material / model to disk. CPU-only (no GPU
// upload); overwrites every tools run so edits propagate. Call BEFORE automation
// runs (from GenerateRenderTestTestbedAssets), so AddStep_LoadModel resolves the
// exported models when the scene is (re)built and saved.
void RenderTest_ExportMaterialShowcaseAssets();

// Author the platform + every shape cell as saved (non-transient) entities via
// editor automation, each with a model + a static per-primitive collider. Call in
// the testbed-authoring block BEFORE AddStep_SaveScene.
void RenderTest_AuthorMaterialShowcase(Zenith_EditorAutomation& xAuto);
#endif

// Release the export-time asset handles before Zenith_AssetRegistry shutdown
// (mirrors RenderTest_TennisShutdown). Call from Project_Shutdown. No-op in
// non-tools builds (the handles are only populated during the tools export).
void RenderTest_MaterialShowcaseShutdown();

// Grid framing, filled by the export/authoring and read by the MaterialBattleTest
// capture sweep to aim the editor camera (columns + world-X span of the grid).
namespace RenderTest_Showcase
{
	extern int   g_iColumns;
	extern float g_fGridMinX;
	extern float g_fGridMaxX;
}
