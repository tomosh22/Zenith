#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR
#ifdef ZENITH_TOOLS

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"

#include "Editor/TerrainEditor/Zenith_TerrainEditor.h"
#include "Editor/Zenith_Editor.h"
#include "Editor/Zenith_UndoSystem.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "ZenithECS/Zenith_SceneSystem.h"

#include "RenderTest/Components/RenderTest_GameplayState.h"

// Manual-only VISUAL showcase: drives every terrain-editor scenario through
// the scriptable API (the exact code path the panel uses) with multi-second
// pauses between scenarios, so an external capture harness can screenshot
// each state ("[RenderTest] SHOWCASE_MARK <name>" marks each transition).
// Runs in Playing mode under the test harness — the follow camera's photo-mode
// override (set in Setup, AFTER the script's OnAwake Reset) gives a stable
// elevated overview, and the terrain editor's ServiceUpdate keeps the live
// preview (evictions / paint uploads / grass rebuilds) pumping. All sculpt
// sites sit ahead (+Z) of the spawn camera; grass is painted on UNSCULPTED
// ground (blades ride the baked physics mesh).
//
// m_bManualOnly: a multi-minute screenshot demo has no per-commit signal —
// run it by name: --automated-test TerrainEditorShowcase.

namespace
{
	constexpr int iFRAMES_PER_SCENARIO = 420;

	void Showcase_Mark(const char* szName)
	{
		Zenith_Log(LOG_CATEGORY_TERRAIN, "[RenderTest] SHOWCASE_MARK %s", szName);
	}

	void Setup_TerrainEditorShowcase()
	{
		Zenith_EntityID uTerrain = INVALID_ENTITY_ID;
		g_xEngine.Scenes().QueryAllScenes<Zenith_TerrainComponent>().ForEach(
			[&uTerrain](Zenith_EntityID uEntity, Zenith_TerrainComponent&)
			{
				if (uTerrain == INVALID_ENTITY_ID)
				{
					uTerrain = uEntity;
				}
			});
		Zenith_Assert(uTerrain != INVALID_ENTITY_ID, "TerrainEditorShowcase: no terrain component in the scene");

		g_xEngine.UndoSystem().Clear();
		g_xEngine.Editor().OpenTerrainEditor(uTerrain);
		g_xEngine.TerrainEditor().m_xBrush.m_eTool = Zenith_TerrainBrushTool::Raise;
		g_xEngine.TerrainEditor().m_xBrush.m_fRadius = 45.0f;
		g_xEngine.TerrainEditor().m_xBrush.m_fStrength = 0.9f;

		// Elevated photo-mode overview via the follow camera's override (the
		// proven Play-mode view path): ~55m above and ~65m behind the player,
		// pitched down ~30 degrees — plateau at the bottom edge, grass/splat
		// sites mid-ground, hill/pit/stamp sites beyond.
		RenderTest_GameplayState::s_bPhotoModeActive = true;
		RenderTest_GameplayState::s_fPhotoOffsetX = 0.0f;
		RenderTest_GameplayState::s_fPhotoOffsetY = 55.0f;
		RenderTest_GameplayState::s_fPhotoOffsetZ = -65.0f;
		RenderTest_GameplayState::s_fPhotoPitch = -0.5f;
	}

	bool Step_TerrainEditorShowcase(int iFrame)
	{
		Zenith_TerrainEditor& xTE = g_xEngine.TerrainEditor();

		switch (iFrame)
		{
		case 30:
			Showcase_Mark("session_open");
			break;

		case 1 * iFRAMES_PER_SCENARIO:
		{
			xTE.BeginStroke();
			for (u_int u = 0; u < 30; u++)
			{
				xTE.ApplyBrushDab(Zenith_TerrainBrushTool::Raise, 256.0f, 340.0f, 45.0f, 1.0f, 0.0f);
			}
			xTE.EndStroke();
			Showcase_Mark("raise");
			break;
		}

		case 2 * iFRAMES_PER_SCENARIO:
		{
			xTE.BeginStroke();
			for (u_int u = 0; u < 24; u++)
			{
				xTE.ApplyBrushDab(Zenith_TerrainBrushTool::Lower, 330.0f, 320.0f, 32.0f, 1.0f, 0.0f);
			}
			xTE.EndStroke();
			Showcase_Mark("lower");
			break;
		}

		case 3 * iFRAMES_PER_SCENARIO:
		{
			xTE.BeginStroke();
			for (u_int u = 0; u < 6; u++)
			{
				xTE.ApplyBrushDab(Zenith_TerrainBrushTool::Terrace, 256.0f, 340.0f, 60.0f, 1.0f, 5.0f);
			}
			xTE.EndStroke();
			Showcase_Mark("terrace");
			break;
		}

		case 4 * iFRAMES_PER_SCENARIO:
		{
			xTE.SampleStamp(256.0f, 340.0f, 55.0f);
			xTE.BeginStroke();
			xTE.ApplyBrushDab(Zenith_TerrainBrushTool::Stamp, 180.0f, 330.0f, 55.0f, 1.0f, 0.0f);
			xTE.EndStroke();
			Showcase_Mark("stamp");
			break;
		}

		case 5 * iFRAMES_PER_SCENARIO:
		{
			Zenith_TerrainErosionParams xParams;
			xParams.m_uSeed = 1337;
			xParams.m_uHydraulicDroplets = 40000;
			xParams.m_uThermalIterations = 2;
			xParams.m_bRegionOnly = true;
			xParams.m_fRegionCentreX = 256.0f;
			xParams.m_fRegionCentreZ = 340.0f;
			xParams.m_fRegionRadius = 90.0f;
			xTE.RunErosion(xParams, true);
			Showcase_Mark("erode");
			break;
		}

		case 6 * iFRAMES_PER_SCENARIO:
		{
			xTE.BeginStroke();
			xTE.ApplyBrushDab(Zenith_TerrainBrushTool::GrassDensity, 230.0f, 290.0f, 40.0f, 1.0f, 0.9f);
			xTE.ApplyBrushDab(Zenith_TerrainBrushTool::GrassDensity, 285.0f, 290.0f, 35.0f, 1.0f, 0.8f);
			xTE.EndStroke();
			Showcase_Mark("grass");
			break;
		}

		case 7 * iFRAMES_PER_SCENARIO:
		{
			xTE.BeginStroke();
			for (u_int u = 0; u < 6; u++)
			{
				xTE.ApplyBrushDab(Zenith_TerrainBrushTool::SplatPaint, 256.0f, 295.0f, 45.0f, 1.0f, 1.0f /* layer 1 = rock */);
			}
			xTE.EndStroke();
			Showcase_Mark("splat");
			break;
		}

		case 8 * iFRAMES_PER_SCENARIO:
			g_xEngine.UndoSystem().Undo();   // reverts the splat stroke
			Showcase_Mark("undo");
			break;

		case 9 * iFRAMES_PER_SCENARIO:
			Showcase_Mark("complete");
			break;

		default:
			break;
		}

		return iFrame < 9 * iFRAMES_PER_SCENARIO + 240;
	}

	bool Verify_TerrainEditorShowcase()
	{
		// Visual demo — pass as long as the sculpting actually landed.
		const bool bSculpted = g_xEngine.TerrainEditor().SampleHeightWorld(256.0f, 340.0f) > 50.0f;
		g_xEngine.UndoSystem().Clear();
		g_xEngine.TerrainEditor().Close();
		return bSculpted;
	}

	const Zenith_AutomatedTest g_xTerrainEditorShowcase = {
		"TerrainEditorShowcase",
		&Setup_TerrainEditorShowcase,
		&Step_TerrainEditorShowcase,
		&Verify_TerrainEditorShowcase,
		9 * iFRAMES_PER_SCENARIO + 400,
		true /* m_bRequiresGraphics */,
		true /* m_bManualOnly */
	};
	ZENITH_AUTOMATED_TEST_REGISTER(g_xTerrainEditorShowcase);
}

#endif // ZENITH_TOOLS
#endif // ZENITH_INPUT_SIMULATOR
