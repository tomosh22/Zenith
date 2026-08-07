#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR
#ifdef ZENITH_TOOLS

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"

#include "Editor/Zenith_Editor.h"
#include "Editor/Zenith_EditorState.h"
#include "Flux/Flux_Screenshot.h"
#include "Flux/Vegetation/Flux_GrassImpl.h"
#include "Maths/Zenith_Maths.h"

#include "RenderTest/Components/RenderTest_BootstrapComponent.h"

#include <cmath>
#include <cstdio>

// ============================================================================
// GrassShowcase -- windowed proof that the GPU-driven grass pipeline actually
// reaches the screen over the campus meadow ring, plus a capture sweep of every
// debug view, the shadow-caster A/B, and the displacement trail.
//
// The three observable stages are asserted separately so a break localises:
//   * GetVisibleTileCount()      -- the CPU scheduler picked tiles this frame,
//   * GetSubmittedDrawCount()    -- the record stage issued the indirect draws,
//   * ReadbackVisibleBladeCount()-- the placement CS wrote survivors the draws
//                                   consumed. This one is a REAL GPU download,
//                                   which is why the test is windowed-only
//                                   (m_bRequiresGraphics; headless returns 0 by
//                                   construction and would assert a lie).
//
// Runs in Stopped mode so the editor free camera frames the meadow instead of
// the player camera -- the same choice MaterialBattleTest makes, and the reason
// this test drives the grass apply itself: the bootstrap component's retry only
// ticks while Playing.
// ============================================================================

namespace
{
	// The campus centre + the spawn meadow ring the terrain recipe paints
	// (RenderTest.cpp: four GrassDensity dabs at +/- 74 m, then GrassType dabs
	// stamping types 1/2/3 into three of those four lobes).
	constexpr float fCAMPUS_CX = 2048.0f;
	constexpr float fCAMPUS_CZ = 2048.0f;
	constexpr float fGROUND_Y  = 48.25f;   // the flattened gameplay plateau

	// Setup's Play -> Stopped switch queues a DEFERRED scene restore, and that
	// reload fires the grass scene-reset hook. Nothing is applied (or latched)
	// until it has landed, or the build would be cleared out from under the test.
	constexpr int iRESTORE_SETTLE_FRAMES = 15;
	// Terrain streams + the ~4 MB density set decodes before the first build.
	constexpr int iREADY_DEADLINE  = 240;
	// Park -> gather -> placement -> draw -> readback: the parked camera's tiles
	// have to survive a full frame before the counts mean anything.
	constexpr int iSETTLE_FRAMES   = 12;
	// One dump per view, and the swapchain consumes ONE request per frame.
	constexpr int iFRAMES_PER_MODE = 10;

	// The modes Flux_GrassDebugColour actually implements (Flux_GrassCommon.slang):
	// 0 None / 1 TypeIndex / 2 Clump / 3 HeightT / 4 LODMesh / 5 LatticeClass /
	// 6 Normals; 7 is in the variable's range but falls through to normals.
	const char* const aszDEBUG_MODE_NAMES[] = {
		"0_none", "1_typeindex", "2_clump", "3_heightt",
		"4_lodmesh", "5_latticeclass", "6_normals", "7_normals_alias",
	};
	constexpr int iDEBUG_MODE_COUNT = static_cast<int>(sizeof(aszDEBUG_MODE_NAMES) / sizeof(aszDEBUG_MODE_NAMES[0]));
	// Two extra sweep slots after the debug views: the lit scene with grass
	// shadows ON then OFF (the cascade A/B the DisableShadowCasting override
	// exists for). Distinct captures prove the caster wiring changes the image.
	constexpr int iSHADOW_AB_COUNT = 2;
	// ...then the displacement trail. It needs TIME, not a different view: the
	// orbiting mover has to walk far enough that the decaying field behind it
	// reads as a trail rather than a dot, so this capture occupies three
	// ordinary slots and dumps only in the last one. Three slots rather than one
	// long one keeps the sweep arithmetic a plain divide.
	constexpr int iTRAIL_SLOT_COUNT = 3;
	constexpr int iSWEEP_COUNT      = iDEBUG_MODE_COUNT + iSHADOW_AB_COUNT + iTRAIL_SLOT_COUNT;
	constexpr int iTRAIL_FIRST_SLOT = iDEBUG_MODE_COUNT + iSHADOW_AB_COUNT;
	// One dump per debug view, one per shadow A/B slot, and ONE for the trail —
	// so this is no longer the same number as iSWEEP_COUNT.
	constexpr int iEXPECTED_DUMPS   = iDEBUG_MODE_COUNT + iSHADOW_AB_COUNT + 1;

	int   g_iReadyFrame     = -1;
	bool  g_bFilesMissing   = false;
	u_int g_uVisibleTiles   = 0u;
	u_int g_uSubmittedDraws = 0u;
	u_int g_uVisibleBlades  = 0u;
	int   g_iDumpsRequested = 0;

	void SetCam(float fX, float fY, float fZ, double fPitch, double fYaw, float fFOV)
	{
		Zenith_EditorCameraState& xCam = g_xEngine.Editor().m_xEditorState.m_xCamera;
		xCam.m_xPosition = Zenith_Maths::Vector3(fX, fY, fZ);
		xCam.m_fPitch = fPitch;
		xCam.m_fYaw = fYaw;
		xCam.m_fFOV = fFOV;
		xCam.m_fNear = 0.1f;
		xCam.m_fFar = 6000.0f;
	}

	// Place the editor camera at (cx,cy,cz) aimed at (tx,ty,tz). Derives the
	// pitch/yaw the editor's BuildViewMatrix expects: forward =
	// (-cos p sin y, sin p, cos p cos y), so pitch=atan2(dy,horiz),
	// yaw=atan2(-dx,dz).
	void AimAt(float cx, float cy, float cz, float tx, float ty, float tz, float fFOV)
	{
		const float dx = tx - cx, dy = ty - cy, dz = tz - cz;
		const float fHoriz = sqrtf(dx * dx + dz * dz);
		const double fPitch = atan2((double)dy, (double)fHoriz);
		const double fYaw = atan2((double)(-dx), (double)dz);
		SetCam(cx, cy, cz, fPitch, fYaw, fFOV);
	}

	// Straight down over the south meadow lobe. The orbiting mover circles the
	// CAMERA's ground point, and the ground under a forward-looking camera is
	// never in its own frustum — so the trail capture is the one slot that has
	// to look at its own feet. 18 m up over the lobe centre puts a 3 m orbit
	// comfortably inside the frame and inside fully-painted coverage.
	void ParkOverTrail()
	{
		AimAt(fCAMPUS_CX, fGROUND_Y + 18.0f, fCAMPUS_CZ - 74.0f,
			fCAMPUS_CX, fGROUND_Y, fCAMPUS_CZ - 74.0f + 4.0f, 70.0f);
	}

	void ParkOverMeadowRing()
	{
		// Elevated overview from just south of the ring: all four lobes are in
		// frame (N = type 1, E = type 2, W = type 3, S left at type 0), and the
		// near lobe sits inside fHI_RADIUS (64 m) so the foreground is HI-lattice
		// blades rather than the LO ring alone.
		AimAt(fCAMPUS_CX, fGROUND_Y + 42.0f, fCAMPUS_CZ - 120.0f,
			fCAMPUS_CX, fGROUND_Y + 1.0f, fCAMPUS_CZ + 10.0f, 65.0f);
	}

	void Setup_GrassShowcase()
	{
		g_iReadyFrame     = -1;
		g_bFilesMissing   = false;
		g_uVisibleTiles   = 0u;
		g_uSubmittedDraws = 0u;
		g_uVisibleBlades  = 0u;
		g_iDumpsRequested = 0;

		// Stopped -> the editor free camera renders the world scene.
		g_xEngine.Editor().SetEditorMode(EditorMode::Stopped);
	}

	bool Step_GrassShowcase(int iFrame)
	{
		Flux_GrassImpl& xGrass = g_xEngine.Grass();

		if (g_iReadyFrame < 0)
		{
			if (iFrame < iRESTORE_SETTLE_FRAMES)
			{
				return true;
			}
			// The SAME shared helper the bootstrap component and the tools
			// post-load step call; it is idempotent and reports whether to retry
			// (terrain not streamed yet) or give up (no baked texture set).
			if (!xGrass.IsBuilt()
				&& RenderTest_TryApplyGrassFromDisk() == RenderTest_GrassApplyResult::FileMissing)
			{
				g_bFilesMissing = true;
				return false;
			}
			if (!xGrass.IsBuilt())
			{
				return iFrame < iREADY_DEADLINE;   // Verify reports the timeout
			}

			ParkOverMeadowRing();
			g_iReadyFrame = iFrame;
			return true;
		}

		const int iSinceReady = iFrame - g_iReadyFrame;
		if (iSinceReady < iSETTLE_FRAMES)
		{
			return true;
		}
		if (iSinceReady == iSETTLE_FRAMES)
		{
			// Sampled ONCE: the readback drains staged writes and idles the
			// device, so it must not sit in a per-frame poll.
			g_uVisibleTiles   = xGrass.GetVisibleTileCount();
			g_uSubmittedDraws = xGrass.GetSubmittedDrawCount();
			g_uVisibleBlades  = xGrass.ReadbackVisibleBladeCount();
		}

		const int iSweep = iSinceReady - iSETTLE_FRAMES;
		const int iMode  = iSweep / iFRAMES_PER_MODE;
		const int iPhase = iSweep % iFRAMES_PER_MODE;
		if (iMode >= iSWEEP_COUNT)
		{
			return false;
		}
		if (iPhase == 0)
		{
			// Set a few frames BEFORE the dump: the mode rides the draw constants
			// the NEXT gather stages, so a same-frame request would capture the
			// previous view.
			if (iMode < iDEBUG_MODE_COUNT)
			{
				xGrass.SetDebugMode(static_cast<u_int>(iMode));
			}
			else if (iMode < iTRAIL_FIRST_SLOT)
			{
				// Shadow A/B rides the lit view; the second slot disables casting.
				xGrass.SetDebugMode(0u);
				xGrass.SetDisableShadowCasting(iMode == iDEBUG_MODE_COUNT + 1);
			}
			else if (iMode == iTRAIL_FIRST_SLOT)
			{
				// The trail run opens by restoring the lit view and re-parking
				// overhead. The camera move is what re-anchors the displacement
				// field, so the orbiter has to start AFTER it — a mover submitted
				// at the old camera would splat into a map the move then scrolls
				// entirely out of range, and the field would blank.
				xGrass.SetDebugMode(0u);
				xGrass.SetDisableShadowCasting(false);
				ParkOverTrail();
				xGrass.SetDebugOrbitDisplacer(true);
			}
		}

		// The trail dumps once, in the middle of its LAST slot, so the orbiter has
		// had two full slots to lay a tail down first.
		const bool bTrailDump = iMode == iSWEEP_COUNT - 1 && iPhase == iFRAMES_PER_MODE / 2;
		const bool bModeDump = iMode < iTRAIL_FIRST_SLOT && iPhase == iFRAMES_PER_MODE / 2;
		if (bModeDump || bTrailDump)
		{
			char szPath[256];
			if (iMode < iDEBUG_MODE_COUNT)
			{
				snprintf(szPath, sizeof(szPath), "C:/tmp/grass_showcase_%s.tga", aszDEBUG_MODE_NAMES[iMode]);
			}
			else if (iMode < iTRAIL_FIRST_SLOT)
			{
				snprintf(szPath, sizeof(szPath), "C:/tmp/grass_showcase_shadows_%s.tga",
					iMode == iDEBUG_MODE_COUNT ? "on" : "off");
			}
			else
			{
				snprintf(szPath, sizeof(szPath), "C:/tmp/grass_showcase_displacement_trail.tga");
			}
			Zenith_Log(LOG_CATEGORY_TERRAIN, "[GrassShowcase] %s", szPath);
			Flux_Screenshot::RequestDump(szPath);
			++g_iDumpsRequested;
		}
		return true;
	}

	bool Verify_GrassShowcase()
	{
		bool bPass = true;
		if (g_bFilesMissing)
		{
			Zenith_Error(LOG_CATEGORY_TERRAIN,
				"[GrassShowcase] the baked terrain grass texture set is missing — one full tools boot must author it first");
			return false;
		}
		if (g_iReadyFrame < 0)
		{
			Zenith_Error(LOG_CATEGORY_TERRAIN,
				"[GrassShowcase] grass was never built within %d frames", iREADY_DEADLINE);
			return false;
		}

		Zenith_Log(LOG_CATEGORY_TERRAIN,
			"[GrassShowcase] captured: visibleTiles=%u (want > 0) submittedDraws=%u (want > 0) "
			"visibleBlades=%u (want > 0) dumps=%d (want %d)",
			g_uVisibleTiles, g_uSubmittedDraws, g_uVisibleBlades, g_iDumpsRequested, iEXPECTED_DUMPS);

		if (g_uVisibleTiles == 0u)
		{
			Zenith_Error(LOG_CATEGORY_TERRAIN,
				"[GrassShowcase] the tile scheduler picked no tiles from the parked camera");
			bPass = false;
		}
		if (g_uSubmittedDraws == 0u)
		{
			Zenith_Error(LOG_CATEGORY_TERRAIN,
				"[GrassShowcase] tiles were scheduled but no indirect draw was submitted");
			bPass = false;
		}
		if (g_uVisibleBlades == 0u)
		{
			Zenith_Error(LOG_CATEGORY_TERRAIN,
				"[GrassShowcase] draws were submitted but the placement CS left zero surviving blades");
			bPass = false;
		}
		if (g_iDumpsRequested != iEXPECTED_DUMPS)
		{
			Zenith_Error(LOG_CATEGORY_TERRAIN,
				"[GrassShowcase] requested %d captures, expected one per debug view + the shadow A/B pair "
				"+ the displacement trail (%d)",
				g_iDumpsRequested, iEXPECTED_DUMPS);
			bPass = false;
		}
		return bPass;
	}

	// The debug view, the caster override and the orbiting displacer are all global
	// render state no entity or scene owns, so they come back here rather than in
	// Step (which a timeout or a failure can leave early). A leaked orbiter would
	// keep pushing grass in every later test in the batch.
	//
	// ...and so is the EDITOR MODE Setup flipped to Stopped, which is the one piece
	// of leaked state that breaks the NEXT test rather than this one. The harness
	// enters Playing exactly ONCE (HarnessPhase::EnterPlayingMode, at boot) and
	// Zenith_Editor::ResetSessionForNextTest deliberately leaves the mode alone --
	// mode and play-backup are one state machine and normalising either half in
	// isolation desynchronises it -- so a test that stops the editor stops it for
	// every test after it. Stopped means Zenith_MainLoop clears
	// bShouldUpdateGameLogic, so the world BetweenTests rebuilds is awoken but its
	// pending-OnStart queue never drains: measured cost was HumanShowcase's player
	// animator (whose layers are built in RenderTest_PlayerComponent::OnStart),
	// leaving it FAILING only when batched after this test and PASSING alone.
	// Re-entering Playing here restores exactly the state boot established.
	void Teardown_GrassShowcase()
	{
		Flux_GrassImpl& xGrass = g_xEngine.Grass();
		xGrass.SetDebugMode(0u);
		xGrass.SetDisableShadowCasting(false);
		xGrass.SetDebugOrbitDisplacer(false);
		g_xEngine.Editor().SetEditorMode(EditorMode::Playing);
	}

	const Zenith_AutomatedTest g_xGrassShowcase = {
		"GrassShowcase",
		&Setup_GrassShowcase,
		&Step_GrassShowcase,
		&Verify_GrassShowcase,
		/* maxFrames */ 460,
		true /* m_bRequiresGraphics */,
		false /* m_bManualOnly */,
		&Teardown_GrassShowcase,
	};
	ZENITH_AUTOMATED_TEST_REGISTER(g_xGrassShowcase);
}

#endif // ZENITH_TOOLS
#endif // ZENITH_INPUT_SIMULATOR
