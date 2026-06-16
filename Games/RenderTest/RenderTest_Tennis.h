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
// Like the material showcase, all of this is spawned PROCEDURALLY post scene
// load each boot (procedural Flux_MeshGeometry does not survive SaveScene), so
// it is windowed-only (Zenith_CommandLine::IsHeadless() early-out).

namespace RenderTest_Tennis
{
	// --- Court layout (world space) ---------------------------------------
	// The court floats clear of the terrain/play-area. +Z is the long axis
	// (baseline to baseline), +X is the court width. The net sits at the
	// centre (Z = fCZ). Surface top is at fSURFACE_Y.
	constexpr float fCOURT_CX   = 256.0f;
	constexpr float fCOURT_CZ   = 200.0f;
	constexpr float fSURFACE_Y  = 70.0f;

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
	constexpr float fBASELINE_NEAR_Z = fCOURT_CZ - fHALF_LENGTH;   // 188.115
	constexpr float fBASELINE_FAR_Z  = fCOURT_CZ + fHALF_LENGTH;   // 211.885
	constexpr float fSERVICE_LINE_OFFSET = 6.40f;   // service line is 6.40 m from the net

	// --- Ball -------------------------------------------------------------
	// A real tennis ball is ~6.7 cm across; scaled up for visibility in the
	// testbed at court scale.
	constexpr float fBALL_RADIUS = 0.12f;

	// StickFigure feet bind 1 m below the entity origin, so an entity placed at
	// Y = fSURFACE_Y + 1 stands with its feet on the surface.
	constexpr float fPLAYER_FEET_OFFSET = 1.0f;
}

// Procedural spawn entry point (queued via AddStep_Custom after the material
// showcase). Builds the court/net/lines, the ball, the two NPC players + their
// rackets, and the match manager. Windowed-only.
void RenderTest_SpawnTennisCourt();

// Release the file-scope material/texture handles BEFORE Zenith_AssetRegistry
// shutdown, so their static destructors don't Release into a freed registry
// ("Release called on asset with 0 ref count"). Call from Project_Shutdown.
void RenderTest_TennisShutdown();
