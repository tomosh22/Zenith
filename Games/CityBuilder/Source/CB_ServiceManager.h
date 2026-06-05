#pragma once

#include "CityBuilder/Source/CB_CityGrid.h"
#include "CityBuilder/Source/CB_BuildingManager.h"
#include "CityBuilder/Source/CB_CityStats.h"

// ============================================================================
// CB_ServiceManager — power/water balance and service (police/fire/health/edu)
// coverage. v1 uses a city-wide balance model: power/water are powered when
// total capacity meets total demand; the radius services produce a 0..1 city
// coverage fraction (people-served / population) that feeds happiness. Per-cell
// flood-fill propagation is a documented post-v1 refinement.
// ============================================================================

class CB_ServiceManager
{
public:
	void Initialize(CB_CityGrid* pxGrid);
	void Shutdown();
	bool IsInitialized() const { return m_pxGrid != nullptr; }

	// Recompute balances + coverage from the current buildings, writing power/
	// water capacity+demand and the OK flags into xStats, and the powered/watered
	// flags onto each active building record.
	void ComputeCoverage(CB_BuildingManager& xBuildings, CB_CityStats& xStats);

	// City-wide coverage fraction (0..1) for a service type.
	float GetServiceCoverage(CB_EServiceType eService) const;

private:
	CB_CityGrid* m_pxGrid = nullptr;
	float        m_afCoverage[CB_SERVICE_COUNT] = {};
};
