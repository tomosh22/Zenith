#include "Zenith.h"

#include "CityBuilder/Source/CB_SimulationTick.h"
#include "CityBuilder/Source/CB_CityGrid.h"
#include "CityBuilder/Source/CB_RoadNetwork.h"
#include "CityBuilder/Source/CB_BuildingManager.h"
#include "CityBuilder/Source/CB_ServiceManager.h"
#include "CityBuilder/Source/CB_EconomyManager.h"
#include "CityBuilder/Source/CB_CitizenManager.h"

void CB_SimulationTick::Initialize(CB_CityGrid* pxGrid, CB_RoadNetwork* pxRoads, CB_BuildingManager* pxBuildings,
                                   CB_ServiceManager* pxServices, CB_EconomyManager* pxEconomy, CB_CitizenManager* pxCitizens)
{
	m_pxGrid      = pxGrid;
	m_pxRoads     = pxRoads;
	m_pxBuildings = pxBuildings;
	m_pxServices  = pxServices;
	m_pxEconomy   = pxEconomy;
	m_pxCitizens  = pxCitizens;
	m_fAccumulator = 0.0f;
	m_ulTickCount  = 0;
}

void CB_SimulationTick::Shutdown()
{
	m_pxGrid = nullptr;
	m_pxRoads = nullptr;
	m_pxBuildings = nullptr;
	m_pxServices = nullptr;
	m_pxEconomy = nullptr;
}

float CB_SimulationTick::SpeedMultiplier(CB_ESimSpeed eSpeed)
{
	switch (eSpeed)
	{
	case CB_SIM_PAUSED: return 0.0f;
	case CB_SIM_SLOW:   return 0.5f;
	case CB_SIM_NORMAL: return 1.0f;
	case CB_SIM_FAST:   return 2.0f;
	case CB_SIM_ULTRA:  return 4.0f;
	default:            return 1.0f;
	}
}

void CB_SimulationTick::RefreshPopulationStats()
{
	uint32_t uPop = 0, uCom = 0, uInd = 0;
	const uint32_t uRecords = m_pxBuildings->GetRecordCount();
	for (uint32_t u = 0; u < uRecords; ++u)
	{
		const CB_BuildingRecord& xRec = m_pxBuildings->GetRecord(u);
		if (!xRec.m_bActive)
		{
			continue;
		}
		if (CB_BuildingDefs::IsResidential(xRec.m_eType))     { uPop += xRec.m_uOccupants; }
		else if (CB_BuildingDefs::IsCommercial(xRec.m_eType)) { uCom += xRec.m_uOccupants; }
		else if (CB_BuildingDefs::IsIndustrial(xRec.m_eType)) { uInd += xRec.m_uOccupants; }
	}
	m_xStats.m_uPopulation     = uPop;
	m_xStats.m_uCommercialJobs = uCom;
	m_xStats.m_uIndustrialJobs = uInd;
	m_xStats.m_uJobs           = uCom + uInd;
	m_xStats.m_uEmployed       = (uPop < m_xStats.m_uJobs) ? uPop : m_xStats.m_uJobs;
}

void CB_SimulationTick::RunTick()
{
	if (!IsInitialized())
	{
		return;
	}

	// Phase 1: population / jobs.
	RefreshPopulationStats();

	// Phase 1b: citizen lifecycle — size the pool to housing, match jobs.
	if (m_pxCitizens)
	{
		m_pxCitizens->Sync(m_xStats.m_uPopulation, m_xStats.m_uJobs);
		m_pxCitizens->UpdateCommutes(0.25f);
		m_xStats.m_uEmployed = m_pxCitizens->GetEmployedCount();
	}

	// Phase 2: service coverage + power/water balance.
	m_pxServices->ComputeCoverage(*m_pxBuildings, m_xStats);

	// Phase 3: RCI demand.
	m_pxEconomy->ComputeDemand(m_xStats);

	// Phase 4: building growth. Scan the whole grid each tick so growth reaches
	// any zoned area promptly (a full 1M-cell sweep is ~a few ms at 1 tick/sec);
	// the bounded spawn count keeps the growth *rate* gameplay-reasonable.
	const uint32_t uCells = m_pxGrid->GetWidth() * m_pxGrid->GetHeight();
	m_pxBuildings->ProcessGrowth(m_xStats.m_fResDemand, m_xStats.m_fComDemand, m_xStats.m_fIndDemand, 64u, uCells);

	// Phase 5: derived data — happiness from utilities + service coverage.
	const float fSvc = 0.25f * (m_pxServices->GetServiceCoverage(CB_SERVICE_POLICE)
	                          + m_pxServices->GetServiceCoverage(CB_SERVICE_FIRE)
	                          + m_pxServices->GetServiceCoverage(CB_SERVICE_HEALTH)
	                          + m_pxServices->GetServiceCoverage(CB_SERVICE_EDUCATION));
	const float fUtil = 0.5f * (m_xStats.m_bPowerOk ? 1.0f : 0.0f) + 0.5f * (m_xStats.m_bWaterOk ? 1.0f : 0.0f);
	const float fEmploy = m_pxCitizens ? m_pxCitizens->GetEmploymentRate() : 0.0f;
	float fHappy = 0.20f + 0.25f * fUtil + 0.25f * fSvc + 0.30f * fEmploy;
	fHappy = (fHappy < 0.0f) ? 0.0f : ((fHappy > 1.0f) ? 1.0f : fHappy);
	m_xStats.m_fHappiness = fHappy;

	// Phase 6: taxes.
	m_pxEconomy->CollectTaxes(*m_pxBuildings, *m_pxRoads, m_xStats);

	++m_ulTickCount;
}

void CB_SimulationTick::Update(float fRealDt)
{
	const float fMult = SpeedMultiplier(m_eSpeed);
	if (fMult <= 0.0f)
	{
		return;
	}
	m_fAccumulator += fRealDt * fMult;

	// Cap catch-up ticks per frame so a long stall can't spiral.
	int iGuard = 0;
	while (m_fAccumulator >= fTICK_INTERVAL && iGuard < 8)
	{
		RunTick();
		m_fAccumulator -= fTICK_INTERVAL;
		++iGuard;
	}
}
