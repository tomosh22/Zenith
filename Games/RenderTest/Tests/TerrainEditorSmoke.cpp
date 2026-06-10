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
	bool g_bRampFlattened = false;

	float g_fHeightBeforeStroke = 0.0f;

	// The sculpt site: chunk (6,6) centre — within HIGH-LOD streaming range of
	// the spawn camera (~256,52,252) and clear of the gameplay plateau.
	constexpr float fSCULPT_X = 400.0f;
	constexpr float fSCULPT_Z = 400.0f;
	constexpr u_int uSCULPT_CHUNK = (400 / 64) * 64 + (400 / 64);

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
			const u_int uTexel = ((400u / 2) * Zenith_TerrainEditor::uSPLATMAP_SIZE + (400u / 2)) * 4;
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
			Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneDataForEntity(g_uTerrainEntity);
			if (pxSceneData != nullptr)
			{
				Zenith_Entity xEntity = pxSceneData->GetEntity(g_uTerrainEntity);
				Zenith_TerrainComponent* pxTerrain = xEntity.TryGetComponent<Zenith_TerrainComponent>();
				if (pxTerrain != nullptr && pxTerrain->m_pxStreamingState != nullptr)
				{
					g_bChunkRestreamed = pxTerrain->m_pxStreamingState->m_axChunkResidency[uSCULPT_CHUNK]
						.m_aeStates[Flux_TerrainConfig::LOD_HIGH] == Flux_TerrainLODResidencyState::RESIDENT;
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
		if (!g_bChunkRestreamed) { Zenith_Error(LOG_CATEGORY_TERRAIN, "TerrainEditorSmoke: edited chunk did not re-stream HIGH after eviction"); bPass = false; }
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
