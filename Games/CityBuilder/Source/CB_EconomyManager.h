#pragma once

#include "CityBuilder/Source/CB_BuildingManager.h"
#include "CityBuilder/Source/CB_RoadNetwork.h"
#include "CityBuilder/Source/CB_CityStats.h"

// ============================================================================
// CB_EconomyManager — RCI demand + treasury. Demand is derived from the
// population/jobs balance (a self-correcting loop: jobs pull residents,
// residents create retail/industrial demand). Taxes are collected from active
// buildings (full rate when powered, half otherwise) net of upkeep + road
// maintenance.
// ============================================================================

class CB_EconomyManager
{
public:
	void Initialize() {}

	// Compute residential/commercial/industrial demand (-1..1) from the
	// population + job counts already refreshed into xStats.
	void ComputeDemand(CB_CityStats& xStats) const;

	// Tally income/expenses for this tick and apply to the treasury.
	void CollectTaxes(const CB_BuildingManager& xBuildings, const CB_RoadNetwork& xRoads, CB_CityStats& xStats) const;
};
