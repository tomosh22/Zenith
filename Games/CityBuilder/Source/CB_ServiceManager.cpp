#include "Zenith.h"

#include "CityBuilder/Source/CB_ServiceManager.h"

void CB_ServiceManager::Initialize(CB_CityGrid* pxGrid)
{
	m_pxGrid = pxGrid;
	for (uint32_t u = 0; u < CB_SERVICE_COUNT; ++u)
	{
		m_afCoverage[u] = 0.0f;
	}
}

void CB_ServiceManager::Shutdown()
{
	m_pxGrid = nullptr;
}

float CB_ServiceManager::GetServiceCoverage(CB_EServiceType eService) const
{
	return (eService < CB_SERVICE_COUNT) ? m_afCoverage[eService] : 0.0f;
}

void CB_ServiceManager::ComputeCoverage(CB_BuildingManager& xBuildings, CB_CityStats& xStats)
{
	float fPowerCapacity = 0.0f, fPowerDemand = 0.0f;
	float fWaterCapacity = 0.0f, fWaterDemand = 0.0f;
	uint32_t auServiceCount[CB_SERVICE_COUNT] = {};

	const uint32_t uRecords = xBuildings.GetRecordCount();
	for (uint32_t u = 0; u < uRecords; ++u)
	{
		const CB_BuildingRecord& xRec = xBuildings.GetRecord(u);
		if (!xRec.m_bActive)
		{
			continue;
		}
		const CB_BuildingDef& xDef = CB_BuildingDefs::Get(xRec.m_eType);
		if (xDef.m_fPowerUse < 0.0f) { fPowerCapacity += -xDef.m_fPowerUse; } else { fPowerDemand += xDef.m_fPowerUse; }
		if (xDef.m_fWaterUse < 0.0f) { fWaterCapacity += -xDef.m_fWaterUse; } else { fWaterDemand += xDef.m_fWaterUse; }
		if (xDef.m_eService != CB_SERVICE_NONE && xDef.m_eService < CB_SERVICE_COUNT)
		{
			++auServiceCount[xDef.m_eService];
		}
	}

	const bool bPowerOk = fPowerCapacity >= fPowerDemand;
	const bool bWaterOk = fWaterCapacity >= fWaterDemand;

	// Publish powered/watered onto each active record (v1 city-wide flag).
	for (uint32_t u = 0; u < uRecords; ++u)
	{
		xBuildings.SetServiceFlags(u, bPowerOk, bWaterOk);
	}

	xStats.m_fPowerCapacity = fPowerCapacity;
	xStats.m_fPowerDemand   = fPowerDemand;
	xStats.m_fWaterCapacity = fWaterCapacity;
	xStats.m_fWaterDemand   = fWaterDemand;
	xStats.m_bPowerOk       = bPowerOk;
	xStats.m_bWaterOk       = bWaterOk;

	// Radius services -> city coverage fraction (people served / population).
	const float fPop = static_cast<float>(xStats.m_uPopulation);
	const float fPeoplePerService = 1500.0f;
	auto Frac = [&](CB_EServiceType e) -> float
	{
		if (fPop <= 0.0f) { return (auServiceCount[e] > 0) ? 1.0f : 0.0f; }
		const float f = (static_cast<float>(auServiceCount[e]) * fPeoplePerService) / fPop;
		return (f > 1.0f) ? 1.0f : f;
	};
	m_afCoverage[CB_SERVICE_POWER]     = bPowerOk ? 1.0f : 0.0f;
	m_afCoverage[CB_SERVICE_WATER]     = bWaterOk ? 1.0f : 0.0f;
	m_afCoverage[CB_SERVICE_POLICE]    = Frac(CB_SERVICE_POLICE);
	m_afCoverage[CB_SERVICE_FIRE]      = Frac(CB_SERVICE_FIRE);
	m_afCoverage[CB_SERVICE_HEALTH]    = Frac(CB_SERVICE_HEALTH);
	m_afCoverage[CB_SERVICE_EDUCATION] = Frac(CB_SERVICE_EDUCATION);
}
