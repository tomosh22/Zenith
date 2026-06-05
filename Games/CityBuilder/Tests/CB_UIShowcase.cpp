#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "CityBuilder/Components/CB_CityManager_Behaviour.h"
#include "CityBuilder/Source/CB_RoadController.h"
#include "CityBuilder/Source/CB_Zoning.h"
#include "CityBuilder/Source/CB_BuildingPlacement.h"
#include "CityBuilder/Source/CB_TerrainHeightfield.h"

// ============================================================================
// CB_UIShowcase — windowed UI showcase gate. Builds a small free-form city, then
// parks the simulated cursor over a rotating sequence of toolbar buttons so each
// tool's hover tooltip appears — for screenshots proving the icon toolbar + the
// hover tooltips render. Windowed (needs the live UI canvas + camera).
// ============================================================================

namespace { bool s_bBuilt = false; }

static void Setup_UIShowcase() { s_bBuilt = false; }

static bool Step_UIShowcase(int iFrame)
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
			pxCtrl->HandleClick(1880.0f, 1980.0f); pxCtrl->HandleClick(2220.0f, 1980.0f); pxCtrl->EndRoad();
			pxCtrl->HandleClick(1880.0f, 2120.0f); pxCtrl->HandleClick(2220.0f, 2120.0f); pxCtrl->EndRoad();
			pxCtrl->HandleClick(1940.0f, 1940.0f); pxCtrl->HandleClick(1940.0f, 2160.0f); pxCtrl->EndRoad();
			pxCtrl->HandleClick(2160.0f, 1940.0f); pxCtrl->HandleClick(2160.0f, 2160.0f); pxCtrl->EndRoad();

			pxZone->SyncToGraph(pxCtrl->GetGraph(), *pxField);
			pxZone->PaintZone(2000.0f, 2000.0f, 220.0f, CB_ZONE_RESIDENTIAL, 2);
			pxZone->PaintZone(2160.0f, 2050.0f, 70.0f, CB_ZONE_COMMERCIAL,  2);
			pxZone->PaintZone(1940.0f, 2050.0f, 70.0f, CB_ZONE_INDUSTRIAL,  2);

			pxBuild->PlaceService(CB_BUILDING_POWER_PLANT, 1850.0f, 1880.0f, 0.0f);
			pxBuild->PlaceService(CB_BUILDING_WATER_TOWER, 2240.0f, 2220.0f, 0.0f);
			pxBuild->PlaceService(CB_BUILDING_POLICE,      2000.0f, 2000.0f, 0.0f);
			pxBuild->PlaceService(CB_BUILDING_HOSPITAL,    1950.0f, 2050.0f, 0.0f);

			pxCtrl->RebuildMesh(*pxField);
			s_bBuilt = true;
		}
	}

	// Rotate the hovered tool every ~110 frames so successive screenshots catch
	// different tooltips (res, power, police, school, transit, district, terrain).
	static const int s_aiTools[] = { 2, 6, 8, 11, 14, 16, 19 };
	const int iCount = static_cast<int>(sizeof(s_aiTools) / sizeof(s_aiTools[0]));
	CB_CityManager_Behaviour::ShowcaseHoverTool(s_aiTools[(iFrame / 110) % iCount]);

	return iFrame < 900;   // ~15s windowed, plenty of time to screenshot
}

static bool Verify_UIShowcase()
{
	return CB_CityManager_Behaviour::GetActiveBuild() != nullptr;
}

static const Zenith_AutomatedTest g_xUIShowcaseTest = { "CB_UIShowcase", &Setup_UIShowcase, &Step_UIShowcase, &Verify_UIShowcase, 1000, true };
ZENITH_AUTOMATED_TEST_REGISTER(g_xUIShowcaseTest);

#endif // ZENITH_INPUT_SIMULATOR
