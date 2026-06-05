#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "CityBuilder/Components/CB_CityManager_Behaviour.h"
#include "CityBuilder/Source/CB_RoadController.h"
#include "CityBuilder/Source/CB_Zoning.h"
#include "CityBuilder/Source/CB_BuildingPlacement.h"
#include "CityBuilder/Source/CB_TerrainHeightfield.h"

// ============================================================================
// CB_CityGrow — G4 windowed gate. Lays a small free-form road grid near the
// city centre through the live controller, zones the frontage R/C/I, and lets
// the manager's demand-driven growth fill it in over a few seconds — then
// asserts buildings + population. Renders the curved roads, zone overlay, and
// road-facing building boxes for a screenshot. Windowed (live terrain + GPU).
// ============================================================================

namespace { bool s_bBuilt = false; }

static void Setup_CityGrow() { s_bBuilt = false; }

static bool Step_CityGrow(int iFrame)
{
	if (!s_bBuilt)
	{
		CB_RoadController*     pxCtrl  = CB_CityManager_Behaviour::GetActiveRoadController();
		CB_Zoning*             pxZone  = CB_CityManager_Behaviour::GetActiveZoning();
		CB_TerrainHeightfield* pxField = CB_CityManager_Behaviour::GetActiveHeightfield();
		CB_BuildingPlacement*  pxBuild = CB_CityManager_Behaviour::GetActiveBuild();
		if (pxCtrl != nullptr && pxZone != nullptr && pxField != nullptr && pxBuild != nullptr)
		{
			pxCtrl->SetRoadClass(CB_ROADCLASS_MEDIUM);
			// Two horizontal + two vertical roads → a small block grid (free-form).
			pxCtrl->HandleClick(1880.0f, 1980.0f); pxCtrl->HandleClick(2220.0f, 1980.0f); pxCtrl->EndRoad();
			pxCtrl->HandleClick(1880.0f, 2120.0f); pxCtrl->HandleClick(2220.0f, 2120.0f); pxCtrl->EndRoad();
			pxCtrl->HandleClick(1940.0f, 1940.0f); pxCtrl->HandleClick(1940.0f, 2160.0f); pxCtrl->EndRoad();
			pxCtrl->HandleClick(2160.0f, 1940.0f); pxCtrl->HandleClick(2160.0f, 2160.0f); pxCtrl->EndRoad();

			pxZone->SyncToGraph(pxCtrl->GetGraph(), *pxField);
			pxZone->PaintZone(2000.0f, 2000.0f, 220.0f, CB_ZONE_RESIDENTIAL, 2);  // bulk residential
			pxZone->PaintZone(2160.0f, 2050.0f, 70.0f, CB_ZONE_COMMERCIAL,  2);
			pxZone->PaintZone(1940.0f, 2050.0f, 70.0f, CB_ZONE_INDUSTRIAL,  2);

			// Utilities (lift the power/water cap, with headroom) + services (raise land value → buildings level up).
			pxBuild->PlaceService(CB_BUILDING_POWER_PLANT, 1850.0f, 1880.0f, 0.0f);
			pxBuild->PlaceService(CB_BUILDING_POWER_PLANT, 2240.0f, 1880.0f, 0.0f);
			pxBuild->PlaceService(CB_BUILDING_WATER_TOWER, 1850.0f, 2220.0f, 0.0f);
			pxBuild->PlaceService(CB_BUILDING_WATER_TOWER, 2240.0f, 2220.0f, 0.0f);
			pxBuild->PlaceService(CB_BUILDING_POLICE,      2000.0f, 2000.0f, 0.0f);
			pxBuild->PlaceService(CB_BUILDING_FIRE,        2050.0f, 2050.0f, 0.0f);
			pxBuild->PlaceService(CB_BUILDING_HOSPITAL,    1950.0f, 2050.0f, 0.0f);
			pxBuild->PlaceService(CB_BUILDING_SCHOOL,      2100.0f, 1950.0f, 0.0f);

			pxCtrl->RebuildMesh(*pxField);
			s_bBuilt = true;
		}
	}
	return iFrame < 420;   // ~7s: let demand grow the city + allow a screenshot
}

static bool Verify_CityGrow()
{
	CB_BuildingPlacement* pxBuild = CB_CityManager_Behaviour::GetActiveBuild();
	CB_RoadController*     pxCtrl  = CB_CityManager_Behaviour::GetActiveRoadController();
	if (pxBuild == nullptr || pxCtrl == nullptr)
	{
		Zenith_Log(LOG_CATEGORY_GAMEPLAY, "CB_CityGrow: missing controller/build");
		return false;
	}
	bool bOk = true;
	if (pxCtrl->GetGraph().GetActiveSegmentCount() < 4) { Zenith_Log(LOG_CATEGORY_GAMEPLAY, "CB_CityGrow: roads not drawn"); bOk = false; }
	if (pxBuild->GetActiveBuildings() < 5) { Zenith_Log(LOG_CATEGORY_GAMEPLAY, "CB_CityGrow: too few buildings (%u)", pxBuild->GetActiveBuildings()); bOk = false; }
	if (pxBuild->GetPopulation() == 0)     { Zenith_Log(LOG_CATEGORY_GAMEPLAY, "CB_CityGrow: no population"); bOk = false; }
	if (pxBuild->GetActiveServices() < 6)  { Zenith_Log(LOG_CATEGORY_GAMEPLAY, "CB_CityGrow: services not placed (%u)", pxBuild->GetActiveServices()); bOk = false; }
	Zenith_Log(LOG_CATEGORY_GAMEPLAY, "CB_CityGrow: %u buildings, pop %u, jobs %u, services %u, treasury %.0f, happiness %.2f, served %.2f",
		pxBuild->GetActiveBuildings(), pxBuild->GetPopulation(), pxBuild->GetJobs(),
		pxBuild->GetActiveServices(), pxBuild->GetTreasury(), pxBuild->GetHappiness(), pxBuild->GetServedFraction());
	return bOk;
}

static const Zenith_AutomatedTest g_xCityGrowTest = { "CB_CityGrow", &Setup_CityGrow, &Step_CityGrow, &Verify_CityGrow, 520, true };
ZENITH_AUTOMATED_TEST_REGISTER(g_xCityGrowTest);

#endif // ZENITH_INPUT_SIMULATOR
