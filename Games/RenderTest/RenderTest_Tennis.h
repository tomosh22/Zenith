#pragma once
#include "Maths/Zenith_Maths.h"

// Tennis-court testbed for RenderTest.
//
// A third "platform" (alongside the IK step cubes and the material showcase): a
// floating tennis court with real textured meshes (grass + painted lines, an
// alpha-tested two-sided net), two StickFigure NPCs that play a rule-correct
// match driven by a physics ball, a world-anchored score display, and an
// auto-restart on match completion. Doubles as the arm-IK / racket-attachment
// testbed.
//
// The geometry/materials/textures are now baked OFFLINE (tools) into CPU
// Zenith_MeshAssets + bundling .zmodels + .zmtrl materials + .ztxtr textures on
// disk; the authored scene loads those models and creates the court/net/ball/NPC
// entities itself. (Previously every mesh was a runtime GPU Flux_MeshGeometry
// spawned post scene-load — that path is gone.)

namespace RenderTest_Tennis
{
	// --- Court layout (world space) ---------------------------------------
	// The court sits flush on the flattened campus plateau, co-planar with the
	// player spawn / IK / gun deck (all at deck-top Y = 48.75) so the three
	// platforms read as one connected campus ringed by hills. +Z is the long
	// axis (baseline to baseline), +X is the court width. The net sits at the
	// centre (Z = fCZ). Surface top is at fSURFACE_Y (= the player deck top).
	// Centred on the 4096 m terrain with the rest of the campus (the court is 56 m
	// south of the campus centre 2048 — was Z=200 vs the old 256 corner anchor).
	constexpr float fCOURT_CX   = 2048.0f;
	constexpr float fCOURT_CZ   = 1992.0f;
	constexpr float fSURFACE_Y  = 48.75f;

	// Regulation doubles court: 23.77 m long x 10.97 m wide.
	constexpr float fCOURT_LENGTH = 23.77f;   // along Z
	constexpr float fCOURT_WIDTH  = 10.97f;   // along X
	constexpr float fHALF_LENGTH  = fCOURT_LENGTH * 0.5f;   // 11.885
	constexpr float fHALF_WIDTH   = fCOURT_WIDTH * 0.5f;    // 5.485

	// A grass apron around the painted court so the lines aren't flush to the edge.
	constexpr float fAPRON       = 2.5f;
	constexpr float fSLAB_HALF_LENGTH = fHALF_LENGTH + fAPRON;
	constexpr float fSLAB_HALF_WIDTH  = fHALF_WIDTH + fAPRON;
	constexpr float fSLAB_THICKNESS   = 0.4f;   // collider/visual slab depth

	// Net: spans the court width at Z = fCZ, 0.91 m high at the centre.
	constexpr float fNET_HEIGHT  = 0.91f;
	constexpr float fNET_HALF_WIDTH = fHALF_WIDTH + 0.5f;   // posts just outside the doubles lines

	// Baselines (where the servers/returners stand) and singles sideline.
	constexpr float fBASELINE_NEAR_Z = fCOURT_CZ - fHALF_LENGTH;   // 1980.115
	constexpr float fBASELINE_FAR_Z  = fCOURT_CZ + fHALF_LENGTH;   // 2003.885
	constexpr float fSERVICE_LINE_OFFSET = 6.40f;   // service line is 6.40 m from the net

	// --- Ball -------------------------------------------------------------
	// A real tennis ball is ~6.7 cm across; scaled up for visibility in the
	// testbed at court scale.
	constexpr float fBALL_RADIUS = 0.12f;

	// StickFigure feet bind 1 m below the entity origin, so an entity placed at
	// Y = fSURFACE_Y + 1 stands with its feet on the surface.
	constexpr float fPLAYER_FEET_OFFSET = 1.0f;
}

// Tools-only: bake the tennis geometry as CPU Zenith_MeshAssets and export them
// (each as a .zasset + a bundling .zmodel), plus the court/net textures (.ztxtr)
// and the court/net/tape/racket/ball materials (.zmtrl), to disk so the authored
// scene can LoadModel them. The court + net models bundle their textured
// materials; the tape + racket models bundle the shared vertex-colour material
// passed in (szVtxColorMaterialPath); the ball model bundles a dedicated yellow
// material. Overwrites every tools run. Texture export is CPU-only (no GPU
// upload) — headless/--skip-tool-exports safe (the caller gates this).
#ifdef ZENITH_TOOLS
void RenderTest_ExportTennisAssets(const char* szVtxColorMaterialPath);
#endif

// Deterministic on-disk paths of the exported tennis .zmodels (stable static
// storage — safe to pass straight to LoadModel). Used by both the export (write
// target) and the authoring (load reference).
const char* RenderTest_TennisCourtModelPath();
const char* RenderTest_TennisNetModelPath();
const char* RenderTest_TennisTapeModelPath();
const char* RenderTest_TennisRacketModelPath();
const char* RenderTest_TennisBallModelPath();

// Parse the tennis spectator / follow / camera / IK-showcase CLI flags into
// RenderTest_GameplayState (spectator vantage, --tenniscam-*, follow side,
// IK-showcase stroke). Called by the RenderTest bootstrap component (previously
// inlined in the now-deleted runtime spawn).
void RenderTest_ParseTennisCLI();

// Release the file-scope material/model handles BEFORE Zenith_AssetRegistry
// shutdown, so their static destructors don't Release into a freed registry
// ("Release called on asset with 0 ref count"). Call from Project_Shutdown.
void RenderTest_TennisShutdown();
