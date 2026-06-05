#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "Input/Zenith_InputSimulator.h"
#include "Input/Zenith_KeyCodes.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Zenith_CameraResolve.h"
#include "Maths/Zenith_Maths.h"
#include "Windows/Zenith_Windows_Window.h"
#include "CityBuilder/Components/CB_CityManager_Behaviour.h"
#include "CityBuilder/Source/CB_RoadController.h"
#include "CityBuilder/Source/CB_Zoning.h"
#include "CityBuilder/Source/CB_BuildingPlacement.h"
#include "CityBuilder/Source/CB_TerrainHeightfield.h"
#include "CityBuilder/Source/CB_ToolSystem.h"
#include "CityBuilder/Source/CB_SaveLoadFreeform.h"
#include <cstdlib>
#include <string>

// ============================================================================
// CB_HumanSession — the headline windowed playthrough: a human builds a sizeable
// Cities: Skylines-style city with the free-form systems over a sustained ~20s
// session, and the city grows in REAL TIME (real sim ticks, no shortcuts):
//
//   * select the road tool and draw a curved road through the camera-unproject
//     picker (validates the interactive tool path end-to-end)
//   * select the power tool and click to place a plant (validates G6a placement)
//   * lay out a full road grid, zone it R/C/I, power+water it, and blanket it in
//     police/fire/health/education services (direct build, for scale)
//   * WATCH demand-driven growth + level-up fill the district, orbiting to survey
//   * zone more land for a second growth wave
//   * pause + resume the simulation (P)
//   * bulldoze a road + rebuild it
//   * save the city to disk and load it back (free-form serialization)
//   * take a camera tour
//
// Verify asserts a sizeable, fully-served, solvent city plus that each mechanic
// fired. m_bRequiresGraphics = true: the picker needs the live camera + window.
//
// REENTRANCY: a Step runs inside Zenith_MainLoop, so it uses ONLY the simulator
// state-setters (SimulateMousePosition / SetKeyHeld / SimulateKeyPress); the
// frame-advancing verbs (SimulateMouseClick / StepFrame) would deadlock the GPU.
// ============================================================================

namespace
{
	enum CB_HS_Phase
	{
		HS_WaitManager = 0,
		HS_MouseRoad,        // draw a road with the actual tool + picker
		HS_MouseService,     // place a power plant with the actual tool + picker
		HS_BuildRoads,       // direct: lay the full grid
		HS_Zone,             // direct: zone R/C/I
		HS_Utilities,        // direct: power + water
		HS_Services,         // direct: police/fire/health/education
		HS_Grow1,            // real-time growth wave 1 (orbit-survey)
		HS_MoreZone,         // zone more land
		HS_Grow2,            // growth wave 2
		HS_Pause, HS_Resume, // pause + resume
		HS_Bulldoze,         // bulldoze a road + rebuild
		HS_Grow3,            // settle
		HS_SaveLoad,         // save to disk + load back
		HS_CamTour,          // survey
		HS_Done
	};

	int g_iPhase = HS_WaitManager;
	int g_iWait  = 0;

	// City centre (matches the default camera framing — CB_CityGrow renders here).
	const float CX = 2100.0f;
	const float CZ = 2050.0f;

	// Mechanic-fired flags + final stats, asserted in Verify.
	bool     g_bMouseRoadWorks    = false;
	bool     g_bMouseServiceWorks = false;
	uint32_t g_uSvcBefore         = 0;   // services before the mouse-placed plant
	bool     g_bPauseWorks        = false;
	bool     g_bBulldozeWorks     = false;
	bool     g_bSaveLoadWorks     = false;
	uint32_t g_uSegsBeforeBulldoze = 0;
	uint32_t g_uFinalBuildings    = 0;
	uint32_t g_uFinalPopulation   = 0;
	uint32_t g_uFinalServices     = 0;

	const float fHS_FIXED_DT = 1.0f / 60.0f;

	void SetMouseFrac(double fXFrac, double fYFrac)
	{
		int32_t iW = 0, iH = 0;
		Zenith_Window::GetInstance()->GetSize(iW, iH);
		if (iW <= 0) { iW = 1280; }
		if (iH <= 0) { iH = 720; }
		Zenith_InputSimulator::SimulateMousePosition(fXFrac * static_cast<double>(iW),
		                                             fYFrac * static_cast<double>(iH));
	}

	// A straight road segment laid directly into the graph.
	void DirectRoad(CB_RoadController& xCtrl, float ax, float az, float bx, float bz)
	{
		xCtrl.HandleClick(ax, az);
		xCtrl.HandleClick(bx, bz);
		xCtrl.EndRoad();
	}

	// "Watch the city grow" — orbit out (E) then back (Q), net-zero yaw.
	bool GrowWatch(int iTotal)
	{
		const int iArc = iTotal / 6;
		if (g_iWait == 0)           { Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_E, true); }
		else if (g_iWait == iArc)   { Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_E, false); Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_Q, true); }
		else if (g_iWait == 2 * iArc) { Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_Q, false); }
		if (g_iWait >= iTotal)
		{
			Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_E, false);
			Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_Q, false);
			return false;
		}
		return true;
	}

	const char* SavePath() { return "cb_humansession_freeform.dat"; }   // relative to the exe cwd
}

static void Setup_CB_HumanSession()
{
	g_iPhase = HS_WaitManager;
	g_iWait  = 0;
	g_bMouseRoadWorks = g_bMouseServiceWorks = false;
	g_bPauseWorks = g_bBulldozeWorks = g_bSaveLoadWorks = false;
	g_uSegsBeforeBulldoze = 0;
	g_uFinalBuildings = g_uFinalPopulation = g_uFinalServices = 0;
	Zenith_InputSimulator::SetFixedDt(fHS_FIXED_DT);
}

static bool Step_CB_HumanSession(int /*iFrame*/)
{
	CB_CityManager_Behaviour* pxMgr = CB_CityManager_Behaviour::GetActive();
	if (pxMgr == nullptr)
	{
		return g_iPhase == HS_WaitManager && g_iWait++ < 600;
	}

	CB_ToolSystem&         xTools = pxMgr->GetTools();
	CB_RoadController*     pxCtrl = CB_CityManager_Behaviour::GetActiveRoadController();
	CB_Zoning*             pxZone = CB_CityManager_Behaviour::GetActiveZoning();
	CB_BuildingPlacement*  pxBld  = CB_CityManager_Behaviour::GetActiveBuild();
	CB_TerrainHeightfield* pxFld  = CB_CityManager_Behaviour::GetActiveHeightfield();
	CB_Districts*          pxDist = CB_CityManager_Behaviour::GetActiveDistricts();
	CB_TransitLines*       pxTran = CB_CityManager_Behaviour::GetActiveTransit();
	CB_Conduits*           pxCon  = CB_CityManager_Behaviour::GetActiveConduits();
	if (pxCtrl == nullptr || pxZone == nullptr || pxBld == nullptr || pxFld == nullptr || pxDist == nullptr || pxTran == nullptr || pxCon == nullptr)
	{
		return true;
	}

	switch (g_iPhase)
	{
	case HS_WaitManager:
	{
		// Start from a clean slate (tests share the scene).
		pxCtrl->Reset();
		pxZone->Reset();
		pxBld->Reset();
		pxBld->TakeLoan(60000.0f);   // borrow to fund the early infrastructure
		pxBld->SetTaxRate(1.1f);     // a modest tax rate
		g_iPhase = HS_MouseRoad;
		g_iWait  = 0;
		return true;
	}

	case HS_MouseRoad:
	{
		// Select the road tool and click two ground points through the real picker.
		if (g_iWait == 0)      { xTools.SetTool(CB_TOOL_ROAD); SetMouseFrac(0.44, 0.52); }
		else if (g_iWait == 2) { Zenith_InputSimulator::SimulateMouseButtonDown(ZENITH_MOUSE_BUTTON_LEFT); }
		else if (g_iWait == 3) { Zenith_InputSimulator::SimulateMouseButtonUp(ZENITH_MOUSE_BUTTON_LEFT); }
		else if (g_iWait == 5) { SetMouseFrac(0.56, 0.48); }
		else if (g_iWait == 7) { Zenith_InputSimulator::SimulateMouseButtonDown(ZENITH_MOUSE_BUTTON_LEFT); }
		else if (g_iWait == 8) { Zenith_InputSimulator::SimulateMouseButtonUp(ZENITH_MOUSE_BUTTON_LEFT); }
		else if (g_iWait >= 11)
		{
			g_bMouseRoadWorks = (pxCtrl->GetGraph().GetActiveNodeCount() > 0);  // picker hit + click landed
			pxCtrl->EndRoad();
			g_iPhase = HS_MouseService;
			g_iWait  = 0;
			return true;
		}
		++g_iWait;
		return true;
	}

	case HS_MouseService:
	{
		// Select the power tool and click — the controller places a plant (G6a).
		if (g_iWait == 0)      { g_uSvcBefore = pxBld->GetActiveServices(); xTools.SetTool(CB_TOOL_POWER); SetMouseFrac(0.5, 0.5); }
		else if (g_iWait == 2) { Zenith_InputSimulator::SimulateMouseButtonDown(ZENITH_MOUSE_BUTTON_LEFT); }
		else if (g_iWait == 3) { Zenith_InputSimulator::SimulateMouseButtonUp(ZENITH_MOUSE_BUTTON_LEFT); }
		else if (g_iWait >= 6)
		{
			g_bMouseServiceWorks = (pxBld->GetActiveServices() > g_uSvcBefore);
			g_iPhase = HS_BuildRoads;
			g_iWait  = 0;
			return true;
		}
		++g_iWait;
		return true;
	}

	case HS_BuildRoads:
	{
		// Direct: a 4x5 road grid around the city centre, then reconcile lots.
		xTools.SetTool(CB_TOOL_ROAD);
		for (int i = 0; i < 4; ++i) { const float z = CZ - 150.0f + i * 100.0f; DirectRoad(*pxCtrl, CX - 240.0f, z, CX + 240.0f, z); }
		for (int i = 0; i < 5; ++i) { const float x = CX - 200.0f + i * 100.0f; DirectRoad(*pxCtrl, x, CZ - 190.0f, x, CZ + 190.0f); }
		pxZone->SyncToGraph(pxCtrl->GetGraph(), *pxFld);
		pxCtrl->RebuildMesh(*pxFld);
		g_iPhase = HS_Zone;
		g_iWait  = 0;
		return true;
	}

	case HS_Zone:
	{
		// Direct: residential bulk + commercial / industrial clusters.
		xTools.SetTool(CB_TOOL_ZONE_RES);
		pxZone->PaintZone(CX, CZ, 320.0f, CB_ZONE_RESIDENTIAL, 2);
		pxZone->PaintZone(CX + 200.0f, CZ, 90.0f, CB_ZONE_COMMERCIAL, 2);
		pxZone->PaintZone(CX - 200.0f, CZ, 90.0f, CB_ZONE_INDUSTRIAL, 2);
		pxZone->PaintZone(CX, CZ + 150.0f, 70.0f, CB_ZONE_COMMERCIAL, 2);
		g_iPhase = HS_Utilities;
		g_iWait  = 0;
		return true;
	}

	case HS_Utilities:
	{
		// Direct: enough power + water for the whole grid (with headroom).
		xTools.SetTool(CB_TOOL_POWER);
		pxBld->PlaceService(CB_BUILDING_POWER_PLANT, CX - 260.0f, CZ - 200.0f, 0.0f);
		pxBld->PlaceService(CB_BUILDING_POWER_PLANT, CX + 260.0f, CZ - 200.0f, 0.0f);
		pxBld->PlaceService(CB_BUILDING_POWER_PLANT, CX,          CZ + 230.0f, 0.0f);
		xTools.SetTool(CB_TOOL_WATER);
		pxBld->PlaceService(CB_BUILDING_WATER_TOWER, CX - 260.0f, CZ + 200.0f, 0.0f);
		pxBld->PlaceService(CB_BUILDING_WATER_TOWER, CX + 260.0f, CZ + 200.0f, 0.0f);
		pxBld->PlaceService(CB_BUILDING_WATER_TOWER, CX,          CZ - 230.0f, 0.0f);
		pxBld->PlaceService(CB_BUILDING_POWER_PLANT, CX + 110.0f, CZ + 230.0f, 0.0f);   // headroom so level-up doesn't brown out
		pxBld->PlaceService(CB_BUILDING_WATER_TOWER, CX - 260.0f, CZ,          0.0f);
		pxBld->PlaceService(CB_BUILDING_WATER_TOWER, CX + 260.0f, CZ,          0.0f);
		g_iPhase = HS_Services;
		g_iWait  = 0;
		return true;
	}

	case HS_Services:
	{
		// Direct: blanket coverage (police/fire/health/education).
		xTools.SetTool(CB_TOOL_POLICE);
		pxBld->PlaceService(CB_BUILDING_POLICE,   CX - 120.0f, CZ - 60.0f, 0.0f);
		pxBld->PlaceService(CB_BUILDING_POLICE,   CX + 120.0f, CZ + 60.0f, 0.0f);
		pxBld->PlaceService(CB_BUILDING_FIRE,     CX + 120.0f, CZ - 60.0f, 0.0f);
		pxBld->PlaceService(CB_BUILDING_FIRE,     CX - 120.0f, CZ + 60.0f, 0.0f);
		pxBld->PlaceService(CB_BUILDING_HOSPITAL, CX,          CZ,         0.0f);
		pxBld->PlaceService(CB_BUILDING_HOSPITAL, CX + 60.0f,  CZ + 120.0f, 0.0f);
		pxBld->PlaceService(CB_BUILDING_SCHOOL,   CX - 60.0f,  CZ - 120.0f, 0.0f);
		pxBld->PlaceService(CB_BUILDING_SCHOOL,   CX + 60.0f,  CZ - 120.0f, 0.0f);
		pxBld->PlaceService(CB_BUILDING_PARK,     CX - 40.0f,  CZ + 40.0f,  0.0f);   // green amenities (raise land value + clean air)
		pxBld->PlaceService(CB_BUILDING_PARK,     CX + 40.0f,  CZ - 40.0f,  0.0f);
		pxBld->PlaceService(CB_BUILDING_PARK,     CX - 180.0f, CZ,          0.0f);   // by the industry → offsets its pollution
		pxBld->PlaceService(CB_BUILDING_PARK,     CX + 180.0f, CZ,          0.0f);
		pxBld->PlaceService(CB_BUILDING_PARK,     CX,          CZ - 100.0f, 0.0f);
		pxBld->PlaceService(CB_BUILDING_PARK,     CX,          CZ + 100.0f, 0.0f);
		// Municipal services: garbage collection, sewage treatment, public transport.
		pxBld->PlaceService(CB_BUILDING_LANDFILL,     CX - 320.0f, CZ - 280.0f, 0.0f);
		pxBld->PlaceService(CB_BUILDING_LANDFILL,     CX + 320.0f, CZ - 280.0f, 0.0f);
		pxBld->PlaceService(CB_BUILDING_SEWAGE_PLANT, CX - 320.0f, CZ + 280.0f, 0.0f);
		pxBld->PlaceService(CB_BUILDING_SEWAGE_PLANT, CX + 320.0f, CZ + 280.0f, 0.0f);
		pxBld->PlaceService(CB_BUILDING_BUS_DEPOT,    CX - 150.0f, CZ,          0.0f);
		pxBld->PlaceService(CB_BUILDING_BUS_DEPOT,    CX + 150.0f, CZ,          0.0f);
		pxBld->PlaceService(CB_BUILDING_POST_OFFICE,  CX - 80.0f,  CZ + 70.0f,  0.0f);
		pxBld->PlaceService(CB_BUILDING_POST_OFFICE,  CX + 80.0f,  CZ - 70.0f,  0.0f);
		// Enact city ordinances + paint a district with its own policy (Cities: Skylines).
		xTools.SetTool(CB_TOOL_DISTRICT);
		pxDist->SetCityPolicy(CB_POLICY_RECYCLING,    true);
		pxDist->SetCityPolicy(CB_POLICY_FREE_TRANSIT, true);
		pxDist->SetCityPolicy(CB_POLICY_PARKS_MANDATE, true);
		{
			const uint32_t uDistIdx = pxDist->PaintDistrict(CX - 200.0f, CZ);   // the industrial quarter
			pxDist->SetDistrictPolicy(uDistIdx, CB_POLICY_POLLUTION_CONTROL, true);
		}
		// A bus LINE routed across the populated centre (stops give transit reach).
		xTools.SetTool(CB_TOOL_TRANSIT);
		pxTran->StartLine();
		pxTran->AddStop(CX - 200.0f, CZ);
		pxTran->AddStop(CX,          CZ);
		pxTran->AddStop(CX + 200.0f, CZ);
		// A utility conduit run (carries power/water along the chain).
		xTools.SetTool(CB_TOOL_CONDUIT);
		pxCon->AddConduit(CX - 100.0f, CZ + 110.0f);
		pxCon->AddConduit(CX,          CZ + 150.0f);
		pxCon->AddConduit(CX + 100.0f, CZ + 110.0f);
		g_iPhase = HS_Grow1;
		g_iWait  = 0;
		return true;
	}

	case HS_Grow1:
	{
		// Disaster drill: once the district has grown, start a fire at its centre. The
		// fire stations placed earlier cover it, so the crews put it out (no buildings lost).
		if (g_iWait == 120) { pxBld->TriggerFireAt(CX, CZ); }
		if (!GrowWatch(300)) { g_iPhase = HS_MoreZone; g_iWait = 0; return true; }
		++g_iWait;
		return true;
	}

	case HS_MoreZone:
	{
		// A second residential wave on the outer rows.
		pxZone->PaintZone(CX, CZ - 150.0f, 260.0f, CB_ZONE_RESIDENTIAL, 2);
		pxZone->PaintZone(CX, CZ + 150.0f, 260.0f, CB_ZONE_RESIDENTIAL, 2);
		g_iPhase = HS_Grow2;
		g_iWait  = 0;
		return true;
	}

	case HS_Grow2:
	{
		if (!GrowWatch(300)) { g_iPhase = HS_Pause; g_iWait = 0; return true; }
		++g_iWait;
		return true;
	}

	case HS_Pause:
	{
		if (g_iWait == 0) { Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_P); }
		else if (g_iWait >= 6)
		{
			g_bPauseWorks = (pxMgr->GetSim().GetSpeed() == CB_SIM_PAUSED);
			g_iPhase = HS_Resume;
			g_iWait  = 0;
			return true;
		}
		++g_iWait;
		return true;
	}

	case HS_Resume:
	{
		if (g_iWait == 0) { Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_P); }
		else if (g_iWait >= 6) { g_iPhase = HS_Bulldoze; g_iWait = 0; return true; }
		++g_iWait;
		return true;
	}

	case HS_Bulldoze:
	{
		if (g_iWait == 0)
		{
			g_uSegsBeforeBulldoze = pxCtrl->GetGraph().GetActiveSegmentCount();
			pxCtrl->BulldozeAt(CX + 200.0f, CZ - 190.0f);   // remove a vertical road
			pxCtrl->RebuildMesh(*pxFld);
		}
		else if (g_iWait == 4)
		{
			const uint32_t uAfter = pxCtrl->GetGraph().GetActiveSegmentCount();
			g_bBulldozeWorks = (uAfter < g_uSegsBeforeBulldoze);
			DirectRoad(*pxCtrl, CX + 200.0f, CZ - 190.0f, CX + 200.0f, CZ + 190.0f);  // rebuild it
			pxZone->SyncToGraph(pxCtrl->GetGraph(), *pxFld);
			pxCtrl->RebuildMesh(*pxFld);
		}
		else if (g_iWait >= 8) { g_iPhase = HS_Grow3; g_iWait = 0; return true; }
		++g_iWait;
		return true;
	}

	case HS_Grow3:
	{
		if (g_iWait++ >= 150) { g_iPhase = HS_SaveLoad; g_iWait = 0; }
		return true;
	}

	case HS_SaveLoad:
	{
		const std::string strPath = SavePath();
		const uint32_t uBld = pxBld->GetActiveBuildings();
		const uint32_t uPop = pxBld->GetPopulation();
		const uint32_t uSvc = pxBld->GetActiveServices();
		CB_SaveLoadFreeform::SaveToFile(pxCtrl->GetGraph(), *pxZone, *pxBld, *pxDist, *pxTran, *pxCon, strPath.c_str());
		const bool bLoaded = CB_SaveLoadFreeform::LoadFromFile(pxCtrl->GetGraph(), *pxZone, *pxBld, *pxDist, *pxTran, *pxCon, strPath.c_str());
		pxCtrl->RebuildMesh(*pxFld);
		g_bSaveLoadWorks = bLoaded
			&& pxBld->GetActiveBuildings() == uBld
			&& pxBld->GetPopulation() == uPop
			&& pxBld->GetActiveServices() == uSvc;
		g_iPhase = HS_CamTour;
		g_iWait  = 0;
		return true;
	}

	case HS_CamTour:
	{
		// A short survey: zoom in a touch then orbit.
		if (g_iWait == 0)       { Zenith_InputSimulator::SimulateMouseWheel(2.0f); }
		else if (g_iWait < 60)  { Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_E, true); }
		else                    { Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_E, false); }
		if (g_iWait >= 90)
		{
			g_uFinalBuildings  = pxBld->GetActiveBuildings();
			g_uFinalPopulation = pxBld->GetPopulation();
			g_uFinalServices   = pxBld->GetActiveServices();
			g_iPhase = HS_Done;
			return false;
		}
		++g_iWait;
		return true;
	}

	case HS_Done:
	default:
		return false;
	}
}

static bool Verify_CB_HumanSession()
{
	CB_BuildingPlacement* pxBld  = CB_CityManager_Behaviour::GetActiveBuild();
	CB_RoadController*     pxCtrl = CB_CityManager_Behaviour::GetActiveRoadController();
	if (pxBld == nullptr || pxCtrl == nullptr)
	{
		Zenith_Log(LOG_CATEGORY_GAMEPLAY, "CB_HumanSession: systems missing");
		return false;
	}

	const uint32_t uSegs = pxCtrl->GetGraph().GetActiveSegmentCount();
	Zenith_Log(LOG_CATEGORY_GAMEPLAY,
		"CB_HumanSession: %u roads, %u buildings, pop %u, %u services, $%.0f, happy %.2f, served %.2f | mouseRoad=%d mouseSvc=%d pause=%d bulldoze=%d saveLoad=%d",
		uSegs, g_uFinalBuildings, g_uFinalPopulation, g_uFinalServices, pxBld->GetTreasury(),
		pxBld->GetHappiness(), pxBld->GetServedFraction(),
		g_bMouseRoadWorks, g_bMouseServiceWorks, g_bPauseWorks, g_bBulldozeWorks, g_bSaveLoadWorks);

	bool bOk = true;
	// A sizeable, working city built with every system.
	if (uSegs < 8)                     { Zenith_Log(LOG_CATEGORY_GAMEPLAY, "CB_HumanSession: too few roads (%u)", uSegs); bOk = false; }
	if (g_uFinalBuildings < 40)        { Zenith_Log(LOG_CATEGORY_GAMEPLAY, "CB_HumanSession: city not sizeable (%u buildings)", g_uFinalBuildings); bOk = false; }
	if (g_uFinalPopulation < 800)      { Zenith_Log(LOG_CATEGORY_GAMEPLAY, "CB_HumanSession: low population (%u)", g_uFinalPopulation); bOk = false; }
	if (g_uFinalServices < 14)         { Zenith_Log(LOG_CATEGORY_GAMEPLAY, "CB_HumanSession: services missing (%u)", g_uFinalServices); bOk = false; }
	if (pxBld->GetServedFraction() < 0.3f) { Zenith_Log(LOG_CATEGORY_GAMEPLAY, "CB_HumanSession: poor service coverage"); bOk = false; }
	// Each interactive mechanic fired.
	if (!g_bMouseRoadWorks)            { Zenith_Log(LOG_CATEGORY_GAMEPLAY, "CB_HumanSession: road tool/picker didn't fire"); bOk = false; }
	if (!g_bMouseServiceWorks)         { Zenith_Log(LOG_CATEGORY_GAMEPLAY, "CB_HumanSession: service tool didn't place"); bOk = false; }
	if (!g_bPauseWorks)                { Zenith_Log(LOG_CATEGORY_GAMEPLAY, "CB_HumanSession: pause didn't work"); bOk = false; }
	if (!g_bBulldozeWorks)             { Zenith_Log(LOG_CATEGORY_GAMEPLAY, "CB_HumanSession: bulldoze didn't work"); bOk = false; }
	if (!g_bSaveLoadWorks)             { Zenith_Log(LOG_CATEGORY_GAMEPLAY, "CB_HumanSession: save/load didn't round-trip"); bOk = false; }
	return bOk;
}

static const Zenith_AutomatedTest g_xHumanSession = { "CB_HumanSession", &Setup_CB_HumanSession, &Step_CB_HumanSession, &Verify_CB_HumanSession, 1400, true };
ZENITH_AUTOMATED_TEST_REGISTER(g_xHumanSession);

#endif // ZENITH_INPUT_SIMULATOR
