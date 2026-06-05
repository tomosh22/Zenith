#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "CityBuilder/Source/CB_CityGrid.h"
#include "CityBuilder/Source/CB_BuildingDefs.h"
#include "CityBuilder/Source/CB_BuildingManager.h"
#include "CityBuilder/Components/CB_CityManager_Behaviour.h"

// ============================================================================
// CB_Buildings — Phase-5 gate. The building def table, spawn/remove with cell
// occupancy, and the auto-growth eligibility gates (zone AND road-access AND
// demand). Logic-only (headless).
// ============================================================================

static bool Verify_CB_Buildings_DefTable()
{
	bool bOk = true;
	if (CB_BuildingDefs::TypeForZone(CB_ZONE_RESIDENTIAL, 0) != CB_BUILDING_RES_LOW)  { Zenith_Log(LOG_CATEGORY_UNITTEST, "DefTable: RES0"); bOk = false; }
	if (CB_BuildingDefs::TypeForZone(CB_ZONE_RESIDENTIAL, 2) != CB_BUILDING_RES_HIGH) { Zenith_Log(LOG_CATEGORY_UNITTEST, "DefTable: RES2"); bOk = false; }
	if (CB_BuildingDefs::TypeForZone(CB_ZONE_COMMERCIAL, 1) != CB_BUILDING_COM_MED)   { Zenith_Log(LOG_CATEGORY_UNITTEST, "DefTable: COM1"); bOk = false; }
	if (CB_BuildingDefs::TypeForZone(CB_ZONE_INDUSTRIAL, 0) != CB_BUILDING_IND_LOW)   { Zenith_Log(LOG_CATEGORY_UNITTEST, "DefTable: IND0"); bOk = false; }
	if (CB_BuildingDefs::TypeForZone(CB_ZONE_PARK, 0)       != CB_BUILDING_NONE)      { Zenith_Log(LOG_CATEGORY_UNITTEST, "DefTable: PARK"); bOk = false; }
	if (CB_BuildingDefs::Get(CB_BUILDING_RES_LOW).m_uMaxOccupants != 8)               { Zenith_Log(LOG_CATEGORY_UNITTEST, "DefTable: occ"); bOk = false; }
	if (!CB_BuildingDefs::IsResidential(CB_BUILDING_RES_HIGH))                        { Zenith_Log(LOG_CATEGORY_UNITTEST, "DefTable: isRes"); bOk = false; }
	if (!CB_BuildingDefs::IsService(CB_BUILDING_POLICE))                              { Zenith_Log(LOG_CATEGORY_UNITTEST, "DefTable: isSvc"); bOk = false; }
	if (CB_BuildingDefs::Get(CB_BUILDING_POWER_PLANT).m_fPowerUse >= 0.0f)            { Zenith_Log(LOG_CATEGORY_UNITTEST, "DefTable: powerplant produces"); bOk = false; }
	if (CB_BuildingDefs::Get(CB_BUILDING_POLICE).m_eService != CB_SERVICE_POLICE)     { Zenith_Log(LOG_CATEGORY_UNITTEST, "DefTable: police service"); bOk = false; }
	return bOk;
}

static bool Verify_CB_Buildings_SpawnRemove()
{
	CB_CityGrid xGrid;     xGrid.Initialize(64, 64, 4.0f, 0.0f, 0.0f);
	CB_BuildingManager xMgr; xMgr.Initialize(&xGrid);

	const uint32_t uRec = xMgr.SpawnBuilding(CB_BUILDING_RES_LOW, 10, 10);
	bool bOk = true;
	if (uRec == CB_BuildingManager::INVALID)        { Zenith_Log(LOG_CATEGORY_UNITTEST, "SpawnRemove: spawn failed"); return false; }
	if (!xGrid.GetCell(10, 10).HasBuilding())       { Zenith_Log(LOG_CATEGORY_UNITTEST, "SpawnRemove: cell no building"); bOk = false; }
	if (xGrid.GetCell(10, 10).GetBuildingRecord() != uRec) { Zenith_Log(LOG_CATEGORY_UNITTEST, "SpawnRemove: record mismatch"); bOk = false; }
	if (xMgr.GetActiveCount() != 1)                 { Zenith_Log(LOG_CATEGORY_UNITTEST, "SpawnRemove: active count"); bOk = false; }
	if (xMgr.GetTotalPopulation() != 8)             { Zenith_Log(LOG_CATEGORY_UNITTEST, "SpawnRemove: pop %u", xMgr.GetTotalPopulation()); bOk = false; }

	// Block: can't spawn on an occupied cell or a road.
	if (xMgr.SpawnBuilding(CB_BUILDING_RES_LOW, 10, 10) != CB_BuildingManager::INVALID) { Zenith_Log(LOG_CATEGORY_UNITTEST, "SpawnRemove: double spawn"); bOk = false; }
	xGrid.GetCell(11, 11).SetRoadType(CB_ROAD_SMALL);
	if (xMgr.SpawnBuilding(CB_BUILDING_RES_LOW, 11, 11) != CB_BuildingManager::INVALID) { Zenith_Log(LOG_CATEGORY_UNITTEST, "SpawnRemove: spawn on road"); bOk = false; }

	// Remove.
	if (!xMgr.RemoveBuilding(uRec))                 { Zenith_Log(LOG_CATEGORY_UNITTEST, "SpawnRemove: remove failed"); bOk = false; }
	if (xGrid.GetCell(10, 10).HasBuilding())        { Zenith_Log(LOG_CATEGORY_UNITTEST, "SpawnRemove: cell still building"); bOk = false; }
	if (xMgr.GetActiveCount() != 0)                 { Zenith_Log(LOG_CATEGORY_UNITTEST, "SpawnRemove: active after remove"); bOk = false; }
	if (xMgr.GetTotalPopulation() != 0)             { Zenith_Log(LOG_CATEGORY_UNITTEST, "SpawnRemove: pop after remove"); bOk = false; }
	return bOk;
}

static bool Verify_CB_Buildings_GrowthGates()
{
	CB_CityGrid xGrid;     xGrid.Initialize(64, 64, 4.0f, 0.0f, 0.0f);
	CB_BuildingManager xMgr; xMgr.Initialize(&xGrid);

	// Eligible: zoned RES + road access.
	xGrid.GetCell(5, 5).SetZone(CB_ZONE_RESIDENTIAL);
	xGrid.GetCell(5, 5).SetDensity(0);
	xGrid.GetCell(5, 5).SetRoadAccess(true);
	// Zoned but NO road access.
	xGrid.GetCell(20, 20).SetZone(CB_ZONE_RESIDENTIAL);
	// Road access but NOT zoned.
	xGrid.GetCell(30, 30).SetRoadAccess(true);

	const uint32_t uSpawned = xMgr.ProcessGrowth(1.0f, 0.0f, 0.0f, 100, 64 * 64);

	bool bOk = true;
	if (uSpawned != 1)                          { Zenith_Log(LOG_CATEGORY_UNITTEST, "GrowthGates: spawned %u (expected 1)", uSpawned); bOk = false; }
	if (!xGrid.GetCell(5, 5).HasBuilding())     { Zenith_Log(LOG_CATEGORY_UNITTEST, "GrowthGates: eligible cell empty"); bOk = false; }
	if (xGrid.GetCell(20, 20).HasBuilding())    { Zenith_Log(LOG_CATEGORY_UNITTEST, "GrowthGates: no-access cell built"); bOk = false; }
	if (xGrid.GetCell(30, 30).HasBuilding())    { Zenith_Log(LOG_CATEGORY_UNITTEST, "GrowthGates: unzoned cell built"); bOk = false; }

	// Demand = 0 => nothing grows even on an eligible cell.
	CB_CityGrid xGrid2;     xGrid2.Initialize(64, 64, 4.0f, 0.0f, 0.0f);
	CB_BuildingManager xMgr2; xMgr2.Initialize(&xGrid2);
	xGrid2.GetCell(8, 8).SetZone(CB_ZONE_COMMERCIAL);
	xGrid2.GetCell(8, 8).SetRoadAccess(true);
	const uint32_t uNoDemand = xMgr2.ProcessGrowth(0.0f, 0.0f, 0.0f, 100, 64 * 64);
	if (uNoDemand != 0)                         { Zenith_Log(LOG_CATEGORY_UNITTEST, "GrowthGates: grew with zero demand (%u)", uNoDemand); bOk = false; }
	return bOk;
}

static bool Verify_CB_Buildings_Active()
{
	CB_CityManager_Behaviour* pxMgr = CB_CityManager_Behaviour::GetActive();
	if (pxMgr == nullptr) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Buildings_Active: no active CityManager"); return false; }
	if (!pxMgr->GetBuildings().IsInitialized()) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Buildings_Active: buildings not init"); return false; }
	// Spawn a service building through the live grid + manager.
	const uint32_t uRec = pxMgr->GetBuildings().SpawnBuilding(CB_BUILDING_POLICE, 600, 600);
	if (uRec == CB_BuildingManager::INVALID) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Buildings_Active: service spawn failed"); return false; }
	if (!pxMgr->GetGrid().GetCell(600, 600).HasBuilding()) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Buildings_Active: cell not built"); return false; }
	return true;
}

static bool Step_Once(int iFrame) { return iFrame < 1; }

static const Zenith_AutomatedTest g_xBldDef    = { "CB_Buildings_DefTable",     nullptr, &Step_Once, &Verify_CB_Buildings_DefTable,     30, false };
static const Zenith_AutomatedTest g_xBldSpawn  = { "CB_Buildings_SpawnRemove",  nullptr, &Step_Once, &Verify_CB_Buildings_SpawnRemove,  30, false };
static const Zenith_AutomatedTest g_xBldGrowth = { "CB_Buildings_GrowthGates",  nullptr, &Step_Once, &Verify_CB_Buildings_GrowthGates,  30, false };
static const Zenith_AutomatedTest g_xBldActive = { "CB_Buildings_Active",       nullptr, &Step_Once, &Verify_CB_Buildings_Active,       30, false };

ZENITH_AUTOMATED_TEST_REGISTER(g_xBldDef);
ZENITH_AUTOMATED_TEST_REGISTER(g_xBldSpawn);
ZENITH_AUTOMATED_TEST_REGISTER(g_xBldGrowth);
ZENITH_AUTOMATED_TEST_REGISTER(g_xBldActive);

#endif // ZENITH_INPUT_SIMULATOR
