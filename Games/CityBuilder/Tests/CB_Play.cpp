#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "CityBuilder/Source/CB_Config.h"
#if CB_USE_LEGACY_GRID   // legacy grid playthrough — superseded by CB_CityGrow / CB_HumanSession

#include "Core/Zenith_AutomatedTest.h"
#include "Input/Zenith_InputSimulator.h"
#include "CityBuilder/Components/CB_CityManager_Behaviour.h"

// ============================================================================
// CB_Play — Phase-9 windowed gate (m_bRequiresGraphics=true; skipped headless).
// Drives the live city through the tool system, runs the sim, and renders many
// frames — proving the presentation + interaction pipeline functions end to end
// (build tools, sim growth, debug-primitive rendering, and a simulated click
// through the camera-unproject picker all run without crashing).
// ============================================================================

static void Setup_CB_Play() {}

static bool Step_CB_Play(int iFrame)
{
	CB_CityManager_Behaviour* pxMgr = CB_CityManager_Behaviour::GetActive();
	if (pxMgr == nullptr)
	{
		return iFrame < 10;  // wait for the manager to come live
	}

	if (iFrame == 2)
	{
		CB_ToolSystem& xTools = pxMgr->GetTools();
		CB_CityGrid& xGrid = pxMgr->GetGrid();
		CB_RoadNetwork& xRoads = pxMgr->GetRoads();
		CB_BuildingManager& xBld = pxMgr->GetBuildings();
		CB_TerrainHeightfield& xTer = pxMgr->GetHeightfield();

		// Lay a road and zone residential beside it via the tool path.
		xTools.SetTool(CB_TOOL_ROAD);
		for (uint32_t x = 400; x <= 440; ++x) { xTools.ApplyToolAt(x, 400, xGrid, xRoads, xBld, xTer); }
		xTools.SetTool(CB_TOOL_ZONE_RES);
		for (uint32_t x = 400; x <= 440; ++x) { xTools.ApplyToolAt(x, 398, xGrid, xRoads, xBld, xTer); xTools.ApplyToolAt(x, 402, xGrid, xRoads, xBld, xTer); }
		xTools.SetTool(CB_TOOL_ZONE_COM);
		for (uint32_t x = 400; x <= 440; ++x) { xTools.ApplyToolAt(x, 397, xGrid, xRoads, xBld, xTer); }

		// A power plant so buildings power up.
		xTools.SetTool(CB_TOOL_POWER);
		xTools.ApplyToolAt(398, 400, xGrid, xRoads, xBld, xTer);

		// Grow the city.
		pxMgr->GetSim().RunTicks(20);

		// Picking smoke: exercise the camera-unproject ground picker through the
		// real simulated-input path. We deliberately DON'T use SimulateMouseClick
		// here — it calls StepFrame() -> a *nested* Zenith_MainLoop, whose
		// BeginFrame deadlocks on vkWaitForFences (the outer frame is still in
		// flight, so its swapchain fence never signals). That deadlock only bites
		// windowed (headless has no swapchain fence), which is exactly when this
		// requiresGraphics test runs. Setting the mouse position and calling the
		// picker directly is non-reentrant and tests the same unproject path.
		xTools.SetTool(CB_TOOL_ROAD);
		Zenith_InputSimulator::SimulateMousePosition(640.0, 360.0);
		uint32_t uPickX = 0, uPickZ = 0;
		const bool bPicked = xTools.PickGroundCell(xGrid, uPickX, uPickZ);
		Zenith_Log(LOG_CATEGORY_GAMEPLAY, "[CB_Play] pick @ screen(640,360) -> %s cell (%u,%u)",
			bPicked ? "hit" : "miss", uPickX, uPickZ);
	}

	// Keep rendering frames so CB_PresentationView draws the city repeatedly.
	return iFrame < 12;
}

static bool Verify_CB_Play()
{
	CB_CityManager_Behaviour* pxMgr = CB_CityManager_Behaviour::GetActive();
	if (pxMgr == nullptr)            { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Play: no CityManager"); return false; }
	if (pxMgr->GetRoads().GetRoadCellCount() == 0)    { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Play: no roads"); return false; }
	if (pxMgr->GetBuildings().GetActiveCount() == 0)  { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Play: no buildings grew"); return false; }
	if (pxMgr->GetStats().m_uPopulation == 0)         { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Play: no population"); return false; }
	Zenith_Log(LOG_CATEGORY_GAMEPLAY, "[CB_Play] city: %u buildings, pop %u, $%d",
		pxMgr->GetBuildings().GetActiveCount(), pxMgr->GetStats().m_uPopulation, static_cast<int>(pxMgr->GetStats().m_fTreasury));
	return true;
}

static const Zenith_AutomatedTest g_xPlay = {
	"CB_Play",
	&Setup_CB_Play,
	&Step_CB_Play,
	&Verify_CB_Play,
	120,
	true   // requires graphics — windowed pass only
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xPlay);

#endif // CB_USE_LEGACY_GRID
#endif // ZENITH_INPUT_SIMULATOR
