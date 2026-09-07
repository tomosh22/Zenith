#pragma once

#include <string>

// Generate all test assets (StickFigure and ProceduralTree)
// Called from main() before unit tests to ensure assets exist
extern void GenerateTestAssets();

// Generate StickFigure human test assets
// Creates: skeleton (16-bone rig, unchanged layout), smooth lofted body mesh,
// mesh geometry, static mesh, painted texture atlas (albedo/normal/RM),
// body material, model bundle, and 17 animations
// Output: ENGINE_ASSETS_DIR/Meshes/StickFigure/
extern void GenerateStickFigureAssets();

//=============================================================================
// SEEDING THE AUTHORED TWINS OF THE 17 StickFigure CLIPS (WU-9.1 stage 1).
//
// ★ THIS IS THE ONE SANCTIONED WRITE INTO `Assets/Authored/`, AND IT IS
// ONE-SHOT. Decision D21 says the bake never overwrites authored data: those
// files are committed, hand-edited in the Animation Editor and written by no
// generator, so a bake that rewrote one would destroy an edit with no way to
// get it back (see Tools/CLAUDE.md and Zenith/AssetHandling/CLAUDE.md). The
// exception granted here is SEEDING A FILE THAT DOES NOT EXIST: the twin is
// written only when nothing is on that path, which makes the phase idempotent
// -- the second boot, and every boot after it, writes nothing at all.
//
// The twin is the generated clip with `m_bGenerated` CLEARED and NOTHING ELSE
// changed: same name, same rig and preview-model refs, same authored frame
// rate, same channels, same keys, same events. It is the same animation, said
// to be authored rather than regenerated.
//=============================================================================

//-----------------------------------------------------------------------------
// What one seeding pass did. Every field is a COUNT OF CLIPS and they always
// satisfy m_uConsidered == m_uWritten + m_uSkippedExisting + m_uFailed
// (CountsAddUp) -- a clip that was neither written, nor already there, nor
// refused would be one the pass silently dropped, which is the failure mode
// this struct exists to make impossible to miss. The same shape, and the same
// reasoning, as Zenith_Tools_AnimMigrateReport.
//-----------------------------------------------------------------------------
struct Zenith_Tools_StickFigureAuthoredSeedReport
{
	u_int m_uConsidered = 0u;
	u_int m_uWritten = 0u;
	u_int m_uSkippedExisting = 0u;
	u_int m_uFailed = 0u;

	bool CountsAddUp() const
	{
		return m_uConsidered == (m_uWritten + m_uSkippedExisting + m_uFailed);
	}
};

// PURE. The bake's own file name for one clip: "Idle" -> "StickFigure_Idle.zanim".
// This is the naming GenerateStickFigureAssets' export loop uses, so the authored
// twin and the generated original share a leaf name by construction.
extern std::string Zenith_Tools_StickFigureClipFileName(const char* szClipName);

// PURE. The authored twin's ASSET PATH for one clip FILE NAME:
//   "StickFigure_Idle.zanim" -> "engine:Authored/Meshes/StickFigure/StickFigure_Idle.zanim"
//
// ★ THE RULE IS Zenith_AnimationDocument::BuildAuthoredAssetPath's, and it is
// matched rather than called -- Tools may not include Editor. The source's ROOT
// PREFIX is kept and "Authored/" is inserted directly under it, preserving the
// SUBDIRECTORY: two generated sets routinely both hold a clip called "Walk", and
// a flattened leaf name would have one silently overwrite the other.
extern std::string Zenith_Tools_StickFigureAuthoredPath(const char* szClipFileName);

// The on-disk directory the boot seeds into -- ENGINE_ASSETS_DIR + the same
// relative path Zenith_Tools_StickFigureAuthoredPath spells after "engine:", so
// the two describe one location. ENGINE_ASSETS_DIR is a define on the engine
// library this file compiles into; the game root (GAME_ASSETS_DIR) does not
// exist here, and no StickFigure clip lives under one.
extern std::string Zenith_Tools_StickFigureAuthoredDir();

// Write the authored twin of each of the 17 StickFigure clips into
// strAuthoredDir, SKIPPING every one that is already there. The directory is
// created if it is missing. strAuthoredDir is a parameter rather than a
// constant so a unit can seed a temp directory instead of the tracked asset
// tree; the boot phase passes Zenith_Tools_StickFigureAuthoredDir().
extern Zenith_Tools_StickFigureAuthoredSeedReport Zenith_Tools_ExportStickFigureAuthoredClips(const std::string& strAuthoredDir);

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
