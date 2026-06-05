#pragma once

#include "CityBuilder/Source/CB_CityGrid.h"
#include "CityBuilder/Source/CB_RoadNetwork.h"
#include "CityBuilder/Source/CB_BuildingManager.h"
#include "CityBuilder/Source/CB_TerrainHeightfield.h"

// ============================================================================
// CB_PresentationView — draws the city each frame using the engine debug-
// primitive subsystem (g_xEngine.Primitives()): a colored box per building, a
// flat box per road cell, and a ground plane. Immediate-mode, no mesh assets.
// Stateless; call Render() once per frame (skipped when headless).
// ============================================================================
namespace CB_PresentationView
{
	void Render(const CB_CityGrid& xGrid, const CB_RoadNetwork& xRoads,
	            const CB_BuildingManager& xBuildings, const CB_TerrainHeightfield& xTerrain);
}
