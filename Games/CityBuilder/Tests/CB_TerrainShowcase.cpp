#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "CityBuilder/Components/CB_CityManager_Behaviour.h"
#include "CityBuilder/Components/CB_CityCamera_Behaviour.h"
#include "CityBuilder/Source/CB_RoadController.h"
#include "CityBuilder/Source/CB_Zoning.h"
#include "CityBuilder/Source/CB_BuildingPlacement.h"
#include "CityBuilder/Source/CB_TerrainHeightfield.h"

// ============================================================================
// CB_TerrainShowcase — windowed gate that shows the rolling terrain: builds a
// small city near the centre, then drives the RTS camera to a low oblique angle
// (zoomed out) so the hill relief reads (the default near-top-down view masks
// gentle slopes). Screenshot target for "the terrain has slight hills".
// ============================================================================

namespace { bool s_bBuilt = false; }

static void Setup_TerrainShowcase() { s_bBuilt = false; }

static bool Step_TerrainShowcase(int iFrame)
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
			// A grid of avenues over the rolling terrain (roads cross hills + dips at many
			// angles — the worst case for road/terrain z-fighting).
			const float aH[3] = { 1850.0f, 2050.0f, 2250.0f };   // east-west avenues
			const float aV[3] = { 1750.0f, 2050.0f, 2350.0f };   // north-south avenues
			for (int i = 0; i < 3; ++i)
			{
				pxCtrl->HandleClick(1750.0f, aH[i]); pxCtrl->HandleClick(2350.0f, aH[i]); pxCtrl->EndRoad();
			}
			for (int i = 0; i < 3; ++i)
			{
				pxCtrl->HandleClick(aV[i], 1850.0f); pxCtrl->HandleClick(aV[i], 2250.0f); pxCtrl->EndRoad();
			}
			pxZone->SyncToGraph(pxCtrl->GetGraph(), *pxField);
			pxZone->PaintZone(2050.0f, 2050.0f, 400.0f, CB_ZONE_RESIDENTIAL, 2);
			pxZone->PaintZone(2050.0f, 1850.0f, 140.0f, CB_ZONE_COMMERCIAL, 2);
			// Services at the real ground height (GetRenderSurfaceY) so they sit on the terrain.
			pxBuild->PlaceService(CB_BUILDING_POWER_PLANT, 1700.0f, 1700.0f, pxField->GetRenderSurfaceY(1700.0f, 1700.0f));
			pxBuild->PlaceService(CB_BUILDING_WATER_TOWER, 2400.0f, 2400.0f, pxField->GetRenderSurfaceY(2400.0f, 2400.0f));
			pxCtrl->RebuildMesh(*pxField);
			s_bBuilt = true;
		}
	}

	// Drive the camera to a low oblique angle, zoomed out, so the hill relief reads.
	if (CB_CityCamera_Behaviour* pxCam = CB_CityCamera_Behaviour::GetActive())
	{
		CB_CameraController& xC = pxCam->GetController();
		xC.m_fPitch    = 0.55f;    // ~31 deg downward — a clear 3/4 view of the streets + hill relief
		xC.m_fDistance = 820.0f;   // close enough to read the road surface (z-fighting shows as shimmer)
		xC.m_fYaw      = 0.70f;    // a 3/4 angle
		xC.m_xTarget   = Zenith_Maths::Vector3(2050.0f, 55.0f, 2050.0f);
	}

	return iFrame < 3000;   // hold the oblique view ~30s so the capture window is comfortable
}

static bool Verify_TerrainShowcase()
{
	return CB_CityManager_Behaviour::GetActiveBuild() != nullptr;
}

static const Zenith_AutomatedTest g_xTerrainShowcaseTest = { "CB_TerrainShowcase", &Setup_TerrainShowcase, &Step_TerrainShowcase, &Verify_TerrainShowcase, 3200, true };
ZENITH_AUTOMATED_TEST_REGISTER(g_xTerrainShowcaseTest);

#endif // ZENITH_INPUT_SIMULATOR
