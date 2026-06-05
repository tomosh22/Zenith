#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "CityBuilder/Source/CB_CityGrid.h"
#include "CityBuilder/Source/CB_RoadNetwork.h"
#include "CityBuilder/Source/CB_BuildingManager.h"
#include "CityBuilder/Source/CB_ServiceManager.h"
#include "CityBuilder/Source/CB_EconomyManager.h"
#include "CityBuilder/Source/CB_CitizenManager.h"
#include "CityBuilder/Source/CB_SimulationTick.h"
#include "CityBuilder/Components/CB_CityManager_Behaviour.h"

// ============================================================================
// CB_Sim — Phase-6 gate. RCI demand math, service coverage + power/water
// balance, tax/treasury, and an end-to-end city that grows over ticks
// (deterministically). Logic-only (headless).
// ============================================================================

static bool Verify_CB_Sim_DemandMath()
{
	CB_EconomyManager xEcon;
	bool bOk = true;

	// Empty city: residential demand positive (seeded growth).
	CB_CityStats xEmpty;
	xEcon.ComputeDemand(xEmpty);
	if (!(xEmpty.m_fResDemand > 0.0f)) { Zenith_Log(LOG_CATEGORY_UNITTEST, "DemandMath: empty resDemand %f", xEmpty.m_fResDemand); bOk = false; }

	// Lots of residents, no jobs: residential demand should go negative.
	CB_CityStats xOver;
	xOver.m_uPopulation = 2000;
	xEcon.ComputeDemand(xOver);
	if (!(xOver.m_fResDemand < 0.0f)) { Zenith_Log(LOG_CATEGORY_UNITTEST, "DemandMath: oversupply resDemand %f", xOver.m_fResDemand); bOk = false; }
	// Population with no commerce -> commercial demand positive.
	if (!(xOver.m_fComDemand > 0.0f)) { Zenith_Log(LOG_CATEGORY_UNITTEST, "DemandMath: comDemand %f", xOver.m_fComDemand); bOk = false; }

	return bOk;
}

static bool Verify_CB_Sim_ServiceCoverage()
{
	CB_CityGrid xGrid;       xGrid.Initialize(64, 64, 4.0f, 0.0f, 0.0f);
	CB_BuildingManager xBld; xBld.Initialize(&xGrid);
	CB_ServiceManager  xSvc; xSvc.Initialize(&xGrid);

	// Power plant (produces 200) + a couple of residential consumers (~4 power).
	xBld.SpawnBuilding(CB_BUILDING_POWER_PLANT, 1, 1);
	xBld.SpawnBuilding(CB_BUILDING_RES_LOW, 3, 3);
	xBld.SpawnBuilding(CB_BUILDING_RES_LOW, 4, 3);

	CB_CityStats xStats;
	xSvc.ComputeCoverage(xBld, xStats);

	bool bOk = true;
	if (!xStats.m_bPowerOk)                    { Zenith_Log(LOG_CATEGORY_UNITTEST, "ServiceCoverage: not powered (cap %f dem %f)", xStats.m_fPowerCapacity, xStats.m_fPowerDemand); bOk = false; }
	if (!(xStats.m_fPowerCapacity >= 200.0f))  { Zenith_Log(LOG_CATEGORY_UNITTEST, "ServiceCoverage: capacity %f", xStats.m_fPowerCapacity); bOk = false; }

	// A police station yields non-zero police coverage.
	xBld.SpawnBuilding(CB_BUILDING_POLICE, 6, 6);
	xStats.m_uPopulation = 100;
	xSvc.ComputeCoverage(xBld, xStats);
	if (!(xSvc.GetServiceCoverage(CB_SERVICE_POLICE) > 0.0f)) { Zenith_Log(LOG_CATEGORY_UNITTEST, "ServiceCoverage: no police coverage"); bOk = false; }
	return bOk;
}

static bool Verify_CB_Sim_TaxTreasury()
{
	CB_CityGrid xGrid;       xGrid.Initialize(64, 64, 4.0f, 0.0f, 0.0f);
	CB_RoadNetwork xRoads;   xRoads.Initialize(&xGrid);
	CB_BuildingManager xBld; xBld.Initialize(&xGrid);
	CB_EconomyManager xEcon;

	// A few residential buildings, marked powered (full tax).
	for (uint32_t i = 0; i < 5; ++i)
	{
		xBld.SpawnBuilding(CB_BUILDING_RES_LOW, 10 + i, 10);
	}
	xBld.SetServiceFlags(0, true, true);
	for (uint32_t u = 0; u < xBld.GetRecordCount(); ++u) { xBld.SetServiceFlags(u, true, true); }

	CB_CityStats xStats;
	xStats.m_fTreasury = 1000.0f;
	xEcon.CollectTaxes(xBld, xRoads, xStats);

	bool bOk = true;
	if (!(xStats.m_fIncomePerTick > 0.0f)) { Zenith_Log(LOG_CATEGORY_UNITTEST, "TaxTreasury: income %f", xStats.m_fIncomePerTick); bOk = false; }
	if (xStats.m_fTreasury == 1000.0f)     { Zenith_Log(LOG_CATEGORY_UNITTEST, "TaxTreasury: treasury unchanged"); bOk = false; }
	return bOk;
}

// Build a small residential strip beside a road and return how many people it
// grows to after uTicks. Used by the determinism check.
static uint32_t GrowStripPopulation(uint32_t uTicks)
{
	CB_CityGrid xGrid;       xGrid.Initialize(64, 64, 4.0f, 0.0f, 0.0f);
	CB_RoadNetwork xRoads;   xRoads.Initialize(&xGrid);
	CB_BuildingManager xBld; xBld.Initialize(&xGrid);
	CB_ServiceManager  xSvc; xSvc.Initialize(&xGrid);
	CB_EconomyManager  xEcon;
	CB_CitizenManager  xCit; xCit.Initialize(&xGrid, &xRoads);
	CB_SimulationTick  xSim; xSim.Initialize(&xGrid, &xRoads, &xBld, &xSvc, &xEcon, &xCit);

	// Road along z=10, x=5..30 -> cells at z=9 and z=11 gain road access.
	for (uint32_t x = 5; x <= 30; ++x) { xRoads.PlaceRoad(x, 10, CB_ROAD_SMALL); }
	for (uint32_t x = 5; x <= 30; ++x)
	{
		xGrid.GetCell(x, 9).SetZone(CB_ZONE_RESIDENTIAL);
		xGrid.GetCell(x, 11).SetZone(CB_ZONE_RESIDENTIAL);
	}
	xSim.RunTicks(uTicks);
	return xSim.GetStats().m_uPopulation;
}

static bool Verify_CB_Sim_Grows()
{
	CB_CityGrid xGrid;       xGrid.Initialize(64, 64, 4.0f, 0.0f, 0.0f);
	CB_RoadNetwork xRoads;   xRoads.Initialize(&xGrid);
	CB_BuildingManager xBld; xBld.Initialize(&xGrid);
	CB_ServiceManager  xSvc; xSvc.Initialize(&xGrid);
	CB_EconomyManager  xEcon;
	CB_CitizenManager  xCit; xCit.Initialize(&xGrid, &xRoads);
	CB_SimulationTick  xSim; xSim.Initialize(&xGrid, &xRoads, &xBld, &xSvc, &xEcon, &xCit);
	xSim.SetStartingTreasury(100000.0f);

	for (uint32_t x = 5; x <= 30; ++x) { xRoads.PlaceRoad(x, 10, CB_ROAD_SMALL); }
	for (uint32_t x = 5; x <= 30; ++x)
	{
		xGrid.GetCell(x, 9).SetZone(CB_ZONE_RESIDENTIAL);
		xGrid.GetCell(x, 11).SetZone(CB_ZONE_RESIDENTIAL);
	}

	xSim.RunTicks(10);
	const CB_CityStats& xStats = xSim.GetStats();

	bool bOk = true;
	if (xSim.GetTickCount() != 10)        { Zenith_Log(LOG_CATEGORY_UNITTEST, "Grows: tickcount %llu", static_cast<unsigned long long>(xSim.GetTickCount())); bOk = false; }
	if (!(xStats.m_uPopulation > 0))      { Zenith_Log(LOG_CATEGORY_UNITTEST, "Grows: population %u", xStats.m_uPopulation); bOk = false; }
	if (!(xBld.GetActiveCount() > 0))     { Zenith_Log(LOG_CATEGORY_UNITTEST, "Grows: no buildings"); bOk = false; }
	if (xStats.m_fTreasury == 100000.0f)  { Zenith_Log(LOG_CATEGORY_UNITTEST, "Grows: treasury unchanged"); bOk = false; }

	// Determinism: a second identical run reaches the same population.
	const uint32_t uA = GrowStripPopulation(10);
	const uint32_t uB = GrowStripPopulation(10);
	if (uA != uB)                         { Zenith_Log(LOG_CATEGORY_UNITTEST, "Grows: non-deterministic %u vs %u", uA, uB); bOk = false; }
	return bOk;
}

static bool Verify_CB_Sim_Active()
{
	CB_CityManager_Behaviour* pxMgr = CB_CityManager_Behaviour::GetActive();
	if (pxMgr == nullptr) { Zenith_Log(LOG_CATEGORY_UNITTEST, "Sim_Active: no CityManager"); return false; }
	if (!pxMgr->GetSim().IsInitialized()) { Zenith_Log(LOG_CATEGORY_UNITTEST, "Sim_Active: sim not init"); return false; }

	// Build a strip on the live grid + run ticks through the live sim.
	for (uint32_t x = 100; x <= 120; ++x) { pxMgr->GetRoads().PlaceRoad(x, 100, CB_ROAD_SMALL); }
	for (uint32_t x = 100; x <= 120; ++x)
	{
		pxMgr->GetGrid().GetCell(x, 99).SetZone(CB_ZONE_RESIDENTIAL);
		pxMgr->GetGrid().GetCell(x, 101).SetZone(CB_ZONE_RESIDENTIAL);
	}
	pxMgr->GetSim().RunTicks(10);
	if (!(pxMgr->GetSim().GetStats().m_uPopulation > 0)) { Zenith_Log(LOG_CATEGORY_UNITTEST, "Sim_Active: population did not grow"); return false; }
	return true;
}

static bool Step_Once(int iFrame) { return iFrame < 1; }

static const Zenith_AutomatedTest g_xSimDemand   = { "CB_Sim_DemandMath",      nullptr, &Step_Once, &Verify_CB_Sim_DemandMath,      30, false };
static const Zenith_AutomatedTest g_xSimService  = { "CB_Sim_ServiceCoverage", nullptr, &Step_Once, &Verify_CB_Sim_ServiceCoverage, 30, false };
static const Zenith_AutomatedTest g_xSimTax      = { "CB_Sim_TaxTreasury",     nullptr, &Step_Once, &Verify_CB_Sim_TaxTreasury,     30, false };
static const Zenith_AutomatedTest g_xSimGrows    = { "CB_Sim_Grows",           nullptr, &Step_Once, &Verify_CB_Sim_Grows,           30, false };
static const Zenith_AutomatedTest g_xSimActive   = { "CB_Sim_Active",          nullptr, &Step_Once, &Verify_CB_Sim_Active,          30, false };

ZENITH_AUTOMATED_TEST_REGISTER(g_xSimDemand);
ZENITH_AUTOMATED_TEST_REGISTER(g_xSimService);
ZENITH_AUTOMATED_TEST_REGISTER(g_xSimTax);
ZENITH_AUTOMATED_TEST_REGISTER(g_xSimGrows);
ZENITH_AUTOMATED_TEST_REGISTER(g_xSimActive);

#endif // ZENITH_INPUT_SIMULATOR
