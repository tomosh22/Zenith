#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR
#ifdef ZENITH_TOOLS

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"

#include "Editor/TerrainEditor/Zenith_TerrainEditor.h"
#include "Editor/Zenith_UndoSystem.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "Flux/Terrain/Flux_TerrainStreamingManagerImpl.h"
#include "ZenithECS/Zenith_SceneSystem.h"

// Windowed terrain-editor regression test. Drives the editor's scriptable API
// (the exact entry points the panel + automation use) against the live
// RenderTest terrain and verifies the full CPU -> GPU edit pipeline:
//
//   1. A sculpt stroke changes the CPU heightfield and marks its chunks
//      session-dirty (the stream-in hook's gate).
//   2. Close() + Open() force-evicts the edited chunks through the engine's
//      race-free streaming path; within ~100 frames they re-stream HIGH
//      (re-shaped by the hook on load — Debug builds run with VK validation,
//      so a sync mistake here trips the harness's validation-error gate).
//   3. The stroke's undo command restores the pre-stroke heights exactly.
//   4. A splat paint keeps the 4 weights normalized and its live GPU
//      re-upload (UpdateTextureVRAM staging path) drains via ServiceUpdate.
//   5. A ramp stroke (multi-dab corridor) flattens toward the anchor line.
//
// m_bRequiresGraphics: terrain render resources don't exist headless.

namespace
{
	Zenith_EntityID g_uTerrainEntity = INVALID_ENTITY_ID;

	bool g_bSculptChanged = false;
	bool g_bChunksMarkedDirty = false;
	bool g_bChunkRestreamed = false;
	bool g_bUndoRestored = false;
	bool g_bSplatNormalized = false;
	bool g_bSplatUploadDrained = false;

	// Captured alongside g_bChunkRestreamed so a failure NAMES what the streamer
	// was doing rather than only that it had not finished. A bare "did not
	// re-stream" cost an investigation once: the answer was that every chunk had
	// been latched SOURCE_UNAVAILABLE during authoring, which these three numbers
	// would have said outright.
	u_int g_uRestreamState = 0u;
	u_int g_uRestreamActiveSet = 0u;
	u_int g_uRestreamHighResident = 0u;
	bool g_bRampFlattened = false;

	float g_fHeightBeforeStroke = 0.0f;

	// The campus sits at the terrain centre, not the (256,256) corner it was
	// historically authored around, and fSHIFT (= centre - 256) carries the legacy
	// 256-anchored layout onto it. The sibling terrain tests
	// (TerrainEditorShowcase, MaterialBattleTest) apply the same shift; this test
	// was once missed in that migration, and unshifted its sculpt site fell
	// ~2300m from the then-centred editor camera on the old 4096m terrain --
	// permanently outside the 1000m HIGH-LOD range, so the edited chunk never
	// streamed HIGH and the re-stream assertion below could never pass.
	//
	// ★ THE SHIFT TRACKS THE TERRAIN SIZE. It was 1792 on a 4096m terrain and is
	// 256 on the 1024m one, which keeps the site's offset FROM THE CAMPUS CENTRE
	// identical (~204m) across the shrink -- the same distance from the gameplay
	// plateau and the same relation to the hill rings as before. It is the
	// relative layout that this test depends on, not the absolute coordinate, and
	// deriving the shift from the centre is what preserves it.
	//
	// The unshifted distance is no longer the dramatic failure it was on a 4096m
	// terrain (400,400 would now be ~150m from the camera, still in HIGH range),
	// so the shift is kept for LAYOUT CONSISTENCY with its siblings rather than to
	// stay inside a streaming radius.
	constexpr float fSHIFT    = 256.0f;
	constexpr float fSCULPT_X = 400.0f + fSHIFT;	// 656 -> chunk (10,10)
	constexpr float fSCULPT_Z = 400.0f + fSHIFT;	// 656
	// ★ THE FLAT INDEX STRIDE IS THE CHUNK-SLOT CAPACITY (64), NOT THE ACTIVE
	// GRID (16). RenderTest's terrain is a 16x16 grid now, but a chunk's GPU slot
	// is a property of its coordinates alone -- the residency table, the indirect
	// argument buffer and the culling shader all address the fixed 64x64 table,
	// so chunk (10,10) is slot 650 whatever the grid measures. Dividing by the
	// grid here instead would silently read a different chunk's residency.
	constexpr u_int uSCULPT_CHUNK = (u_int(fSCULPT_X) / 64) * 64 + (u_int(fSCULPT_Z) / 64);
	static_assert(uSCULPT_CHUNK == 10u * 64u + 10u,
		"the sculpt site must resolve to chunk (10,10) at the capacity stride");

	void Setup_TerrainEditorSmoke()
	{
		g_uTerrainEntity = INVALID_ENTITY_ID;
		g_xEngine.Scenes().QueryAllScenes<Zenith_TerrainComponent>().ForEach(
			[](Zenith_EntityID uEntity, Zenith_TerrainComponent&)
			{
				if (g_uTerrainEntity == INVALID_ENTITY_ID)
				{
					g_uTerrainEntity = uEntity;
				}
			});
		Zenith_Assert(g_uTerrainEntity != INVALID_ENTITY_ID, "TerrainEditorSmoke: no terrain component in the scene");

		g_xEngine.UndoSystem().Clear();
		g_xEngine.TerrainEditor().Open(g_uTerrainEntity);
	}

	bool Step_TerrainEditorSmoke(int iFrame)
	{
		Zenith_TerrainEditor& xEditor = g_xEngine.TerrainEditor();

		if (iFrame == 5)
		{
			// 1. Sculpt stroke (bracketed: exercises the tile-capture undo).
			g_fHeightBeforeStroke = xEditor.SampleHeightWorld(fSCULPT_X, fSCULPT_Z);
			xEditor.BeginStroke();
			for (u_int u = 0; u < 12; u++)
			{
				xEditor.ApplyBrushDab(Zenith_TerrainBrushTool::Raise, fSCULPT_X, fSCULPT_Z, 24.0f, 1.0f, 0.0f);
			}
			xEditor.EndStroke();

			g_bSculptChanged = xEditor.SampleHeightWorld(fSCULPT_X, fSCULPT_Z) > g_fHeightBeforeStroke + 1.0f;
			g_bChunksMarkedDirty = xEditor.IsChunkSessionDirty(uSCULPT_CHUNK);

			// 2. Splat paint on the same site (layer 1 = rock).
			xEditor.ApplyBrushDab(Zenith_TerrainBrushTool::SplatPaint, fSCULPT_X, fSCULPT_Z, 60.0f, 1.0f, 1.0f);
			const Zenith_Vector<u_int8>& xSplat = xEditor.GetSplatmap();
			// ★ WORLD -> SPLAT TEXEL GOES THROUGH THE SESSION, NOT THROUGH "/2".
			// That divisor was 2048 splat texels over a 4096m world, i.e. the
			// 1m-per-heightfield-pixel weld in disguise. On this 1024m terrain the
			// scale is 2 texels per METRE, so the old spelling read a texel four
			// times too close to the origin -- one the dab never touched, whose
			// untouched weights still sum to 255 and whose layer-1 weight is 0.
			// The session owns the conversion; the test asks it.
			const u_int uSplatX = static_cast<u_int>(xEditor.WorldToSplatPx(fSCULPT_X));
			const u_int uSplatZ = static_cast<u_int>(xEditor.WorldToSplatPx(fSCULPT_Z));
			const u_int uTexel = (uSplatX * Zenith_TerrainEditor::uSPLATMAP_SIZE + uSplatZ) * 4;
			const u_int uSum = xSplat.Get(uTexel) + xSplat.Get(uTexel + 1) + xSplat.Get(uTexel + 2) + xSplat.Get(uTexel + 3);
			g_bSplatNormalized = (uSum == 255u) && (xSplat.Get(uTexel + 1) > 0);
		}

		if (iFrame == 10)
		{
			// 3. Session cycle: Close() reverts visuals (hook cleared +
			// session-dirty chunks evicted), Open() re-registers + re-evicts so
			// the unbaked edits re-apply through the streaming path.
			xEditor.Close();
			g_xEngine.TerrainEditor().Open(g_uTerrainEntity);
		}

		if (iFrame == 40)
		{
			// 5. Ramp corridor: a real multi-dab stroke down the sculpted hill.
			// Probe BETWEEN dab centres — at a dab's own centre the corridor
			// target equals the pre-edit height by construction (t = 1).
			xEditor.BeginStroke();
			const float fBefore = xEditor.SampleHeightWorld(fSCULPT_X + 10.0f, fSCULPT_Z);
			xEditor.ApplyBrushDab(Zenith_TerrainBrushTool::Ramp, fSCULPT_X, fSCULPT_Z, 16.0f, 0.9f, 0.0f);
			xEditor.ApplyBrushDab(Zenith_TerrainBrushTool::Ramp, fSCULPT_X + 20.0f, fSCULPT_Z, 16.0f, 0.9f, 0.0f);
			xEditor.ApplyBrushDab(Zenith_TerrainBrushTool::Ramp, fSCULPT_X + 40.0f, fSCULPT_Z, 16.0f, 0.9f, 0.0f);
			xEditor.EndStroke();
			// The flank point must have moved toward the anchor-to-end chord —
			// any change proves the corridor math ran.
			g_bRampFlattened = fabsf(xEditor.SampleHeightWorld(fSCULPT_X + 10.0f, fSCULPT_Z) - fBefore) > 0.01f;
		}

		if (iFrame == 150)
		{
			// By now the evicted chunks re-streamed (budgeted at 16 evictions /
			// 8 uploads per frame) and the splat upload drained.
			Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(g_uTerrainEntity);
			if (xEntity.IsValid())
			{
				Zenith_TerrainComponent* pxTerrain = xEntity.TryGetComponent<Zenith_TerrainComponent>();
				if (pxTerrain != nullptr && pxTerrain->m_pxStreamingState != nullptr)
				{
					const Flux_TerrainStreamingState& xState = *pxTerrain->m_pxStreamingState;
					g_uRestreamState = static_cast<u_int>(
						xState.m_axChunkResidency[uSCULPT_CHUNK].m_aeStates[Flux_TerrainConfig::LOD_HIGH]);
					g_uRestreamActiveSet = static_cast<u_int>(xState.m_xActiveChunkIndices.GetSize());
					g_uRestreamHighResident = 0u;
					for (u_int uX = 0; uX < xState.m_xDims.m_uGridChunksX; uX++)
					{
						for (u_int uZ = 0; uZ < xState.m_xDims.m_uGridChunksZ; uZ++)
						{
							if (xState.m_axChunkResidency[Flux_TerrainConfig::ChunkCoordsToIndex(uX, uZ)]
								.m_aeStates[Flux_TerrainConfig::LOD_HIGH] == Flux_TerrainLODResidencyState::RESIDENT)
							{
								g_uRestreamHighResident++;
							}
						}
					}
					g_bChunkRestreamed = xState.m_axChunkResidency[uSCULPT_CHUNK]
						.m_aeStates[Flux_TerrainConfig::LOD_HIGH] == Flux_TerrainLODResidencyState::RESIDENT;
					// One line per non-resident chunk, with whether it is session-dirty.
					// "the non-resident set is EXACTLY the session-dirty set" is the
					// signature of an editor-side per-chunk defect rather than a
					// streaming budget, and reading it is what identified the
					// world-coordinates-from-pixels AABB bug this test caught. Worth
					// the handful of lines a real failure prints.
					if (!g_bChunkRestreamed)
					{
						for (u_int uX = 0; uX < xState.m_xDims.m_uGridChunksX; uX++)
						{
							for (u_int uZ = 0; uZ < xState.m_xDims.m_uGridChunksZ; uZ++)
							{
								const u_int uIdx = Flux_TerrainConfig::ChunkCoordsToIndex(uX, uZ);
								if (xState.m_axChunkResidency[uIdx].m_aeStates[Flux_TerrainConfig::LOD_HIGH]
									== Flux_TerrainLODResidencyState::RESIDENT)
								{
									continue;
								}
								Zenith_Log(LOG_CATEGORY_TERRAIN,
									"TerrainEditorSmoke: non-resident chunk (%u,%u) idx=%u state=%u sessionDirty=%d",
									uX, uZ, uIdx,
									static_cast<u_int>(xState.m_axChunkResidency[uIdx].m_aeStates[Flux_TerrainConfig::LOD_HIGH]),
									xEditor.IsChunkSessionDirty(uIdx) ? 1 : 0);
							}
						}
					}
				}
			}
			g_bSplatUploadDrained = !xEditor.HasPendingSplatUpload();

			// 4. Undo back to the pre-sculpt heights: ramp stroke first, then
			// the raise stroke.
			g_xEngine.UndoSystem().Undo();
			g_xEngine.UndoSystem().Undo();
			g_bUndoRestored = fabsf(xEditor.SampleHeightWorld(fSCULPT_X, fSCULPT_Z) - g_fHeightBeforeStroke) < 0.05f;
			return false;
		}

		return true;
	}

	bool Verify_TerrainEditorSmoke()
	{
		// The editor session must not outlive the test (the harness reloads
		// scenes between batch tests).
		g_xEngine.UndoSystem().Clear();
		g_xEngine.TerrainEditor().Close();

		bool bPass = true;
		if (!g_bSculptChanged) { Zenith_Error(LOG_CATEGORY_TERRAIN, "TerrainEditorSmoke: sculpt stroke did not raise the heightfield"); bPass = false; }
		if (!g_bChunksMarkedDirty) { Zenith_Error(LOG_CATEGORY_TERRAIN, "TerrainEditorSmoke: sculpt did not mark its chunk session-dirty"); bPass = false; }
		if (!g_bChunkRestreamed)
		{
			// NOT_LOADED with a healthy active set means the streamer simply had not
			// reached this chunk yet; SOURCE_UNAVAILABLE means it gave up on the
			// file; an empty active set means the camera never resolved onto the grid.
			Zenith_Error(LOG_CATEGORY_TERRAIN,
				"TerrainEditorSmoke: edited chunk %u (%u,%u) did not re-stream HIGH after eviction "
				"(state=%u, activeSet=%u, HIGHResident=%u of %u)",
				uSCULPT_CHUNK, u_int(fSCULPT_X) / 64u, u_int(fSCULPT_Z) / 64u,
				g_uRestreamState, g_uRestreamActiveSet, g_uRestreamHighResident,
				g_xEngine.TerrainEditor().GetDimensions().ChunkCount());
			bPass = false;
		}
		if (!g_bUndoRestored) { Zenith_Error(LOG_CATEGORY_TERRAIN, "TerrainEditorSmoke: undo did not restore pre-stroke heights"); bPass = false; }
		if (!g_bSplatNormalized) { Zenith_Error(LOG_CATEGORY_TERRAIN, "TerrainEditorSmoke: splat paint broke weight normalization"); bPass = false; }
		if (!g_bSplatUploadDrained) { Zenith_Error(LOG_CATEGORY_TERRAIN, "TerrainEditorSmoke: splat GPU upload never drained"); bPass = false; }
		if (!g_bRampFlattened) { Zenith_Error(LOG_CATEGORY_TERRAIN, "TerrainEditorSmoke: ramp corridor had no effect"); bPass = false; }
		return bPass;
	}

	const Zenith_AutomatedTest g_xTerrainEditorSmoke = {
		"TerrainEditorSmoke",
		&Setup_TerrainEditorSmoke,
		&Step_TerrainEditorSmoke,
		&Verify_TerrainEditorSmoke,
		400,
		true /* m_bRequiresGraphics */
	};
	ZENITH_AUTOMATED_TEST_REGISTER(g_xTerrainEditorSmoke);
}

#endif // ZENITH_TOOLS
#endif // ZENITH_INPUT_SIMULATOR
