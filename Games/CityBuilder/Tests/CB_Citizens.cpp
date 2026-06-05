#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "CityBuilder/Source/CB_CityGrid.h"
#include "CityBuilder/Source/CB_RoadNetwork.h"
#include "CityBuilder/Source/CB_CitizenManager.h"
#include "CityBuilder/Components/CB_CityManager_Behaviour.h"
#include "Collections/Zenith_Vector.h"

// ============================================================================
// CB_Citizens — Phase-7 gate. Citizen pool sizing + employment, and the BFS
// road shortest-path (straight, L-shape, no-path, non-road). Logic-only.
// ============================================================================

static bool Verify_CB_Citizens_Sync()
{
	CB_CitizenManager xMgr;
	xMgr.Initialize(nullptr, nullptr);

	bool bOk = true;
	xMgr.Sync(100, 60);
	if (xMgr.GetActiveCitizens() != 100) { Zenith_Log(LOG_CATEGORY_UNITTEST, "Citizens_Sync: active %u", xMgr.GetActiveCitizens()); bOk = false; }
	if (xMgr.GetEmployedCount() != 60)   { Zenith_Log(LOG_CATEGORY_UNITTEST, "Citizens_Sync: employed %u", xMgr.GetEmployedCount()); bOk = false; }
	if (!(xMgr.GetEmploymentRate() > 0.59f && xMgr.GetEmploymentRate() < 0.61f)) { Zenith_Log(LOG_CATEGORY_UNITTEST, "Citizens_Sync: rate %f", xMgr.GetEmploymentRate()); bOk = false; }

	// Shrink + more jobs than people => everyone employed.
	xMgr.Sync(50, 60);
	if (xMgr.GetActiveCitizens() != 50) { Zenith_Log(LOG_CATEGORY_UNITTEST, "Citizens_Sync: shrink active %u", xMgr.GetActiveCitizens()); bOk = false; }
	if (xMgr.GetEmployedCount() != 50)  { Zenith_Log(LOG_CATEGORY_UNITTEST, "Citizens_Sync: shrink employed %u", xMgr.GetEmployedCount()); bOk = false; }

	xMgr.Sync(0, 0);
	if (xMgr.GetActiveCitizens() != 0)  { Zenith_Log(LOG_CATEGORY_UNITTEST, "Citizens_Sync: empty active %u", xMgr.GetActiveCitizens()); bOk = false; }
	return bOk;
}

static bool AllRoad(const CB_CityGrid& xGrid, const Zenith_Vector<uint32_t>& xPath)
{
	for (uint32_t u = 0; u < xPath.GetSize(); ++u)
	{
		const uint32_t uIdx = xPath.Get(u);
		const uint32_t uX = uIdx % xGrid.GetWidth();
		const uint32_t uZ = uIdx / xGrid.GetWidth();
		if (!xGrid.GetCell(uX, uZ).HasRoad())
		{
			return false;
		}
	}
	return true;
}

static bool Verify_CB_Citizens_PathStraight()
{
	CB_CityGrid xGrid;   xGrid.Initialize(64, 64, 4.0f, 0.0f, 0.0f);
	CB_RoadNetwork xRoads; xRoads.Initialize(&xGrid);
	for (uint32_t x = 5; x <= 15; ++x) { xRoads.PlaceRoad(x, 10, CB_ROAD_SMALL); }

	Zenith_Vector<uint32_t> xPath;
	const bool bOk0 = xRoads.FindPath(5, 10, 15, 10, xPath);

	bool bOk = true;
	if (!bOk0)                            { Zenith_Log(LOG_CATEGORY_UNITTEST, "PathStraight: no path"); return false; }
	if (xPath.GetSize() != 11)            { Zenith_Log(LOG_CATEGORY_UNITTEST, "PathStraight: length %u", xPath.GetSize()); bOk = false; }
	if (xPath.Get(0) != 10u * 64u + 5u)   { Zenith_Log(LOG_CATEGORY_UNITTEST, "PathStraight: start"); bOk = false; }
	if (xPath.GetBack() != 10u * 64u + 15u){ Zenith_Log(LOG_CATEGORY_UNITTEST, "PathStraight: end"); bOk = false; }
	if (!AllRoad(xGrid, xPath))           { Zenith_Log(LOG_CATEGORY_UNITTEST, "PathStraight: non-road in path"); bOk = false; }
	return bOk;
}

static bool Verify_CB_Citizens_PathLShape()
{
	CB_CityGrid xGrid;   xGrid.Initialize(64, 64, 4.0f, 0.0f, 0.0f);
	CB_RoadNetwork xRoads; xRoads.Initialize(&xGrid);
	for (uint32_t x = 5; x <= 10; ++x) { xRoads.PlaceRoad(x, 10, CB_ROAD_SMALL); }
	for (uint32_t z = 10; z <= 15; ++z) { xRoads.PlaceRoad(10, z, CB_ROAD_SMALL); }

	Zenith_Vector<uint32_t> xPath;
	const bool bOk0 = xRoads.FindPath(5, 10, 10, 15, xPath);

	bool bOk = true;
	if (!bOk0)                  { Zenith_Log(LOG_CATEGORY_UNITTEST, "PathLShape: no path"); return false; }
	if (xPath.GetSize() != 11)  { Zenith_Log(LOG_CATEGORY_UNITTEST, "PathLShape: length %u (expected 11)", xPath.GetSize()); bOk = false; }
	if (xPath.Get(0) != 10u * 64u + 5u)    { Zenith_Log(LOG_CATEGORY_UNITTEST, "PathLShape: start"); bOk = false; }
	if (xPath.GetBack() != 15u * 64u + 10u){ Zenith_Log(LOG_CATEGORY_UNITTEST, "PathLShape: end"); bOk = false; }
	if (!AllRoad(xGrid, xPath)) { Zenith_Log(LOG_CATEGORY_UNITTEST, "PathLShape: non-road"); bOk = false; }
	return bOk;
}

static bool Verify_CB_Citizens_PathNoPath()
{
	CB_CityGrid xGrid;   xGrid.Initialize(64, 64, 4.0f, 0.0f, 0.0f);
	CB_RoadNetwork xRoads; xRoads.Initialize(&xGrid);
	// Two disconnected segments.
	for (uint32_t x = 5; x <= 7; ++x)   { xRoads.PlaceRoad(x, 10, CB_ROAD_SMALL); }
	for (uint32_t x = 20; x <= 22; ++x) { xRoads.PlaceRoad(x, 10, CB_ROAD_SMALL); }

	Zenith_Vector<uint32_t> xPath;
	bool bOk = true;
	if (xRoads.FindPath(5, 10, 22, 10, xPath))  { Zenith_Log(LOG_CATEGORY_UNITTEST, "PathNoPath: found across gap"); bOk = false; }
	if (xRoads.FindPath(5, 10, 30, 30, xPath))  { Zenith_Log(LOG_CATEGORY_UNITTEST, "PathNoPath: end not road"); bOk = false; }
	if (xRoads.FindPath(0, 0, 7, 10, xPath))    { Zenith_Log(LOG_CATEGORY_UNITTEST, "PathNoPath: start not road"); bOk = false; }
	return bOk;
}

static bool Verify_CB_Citizens_Active()
{
	CB_CityManager_Behaviour* pxMgr = CB_CityManager_Behaviour::GetActive();
	if (pxMgr == nullptr) { Zenith_Log(LOG_CATEGORY_UNITTEST, "Citizens_Active: no CityManager"); return false; }
	if (!pxMgr->GetCitizens().IsInitialized()) { Zenith_Log(LOG_CATEGORY_UNITTEST, "Citizens_Active: citizens not init"); return false; }

	for (uint32_t x = 200; x <= 220; ++x) { pxMgr->GetRoads().PlaceRoad(x, 200, CB_ROAD_SMALL); }
	for (uint32_t x = 200; x <= 220; ++x)
	{
		pxMgr->GetGrid().GetCell(x, 199).SetZone(CB_ZONE_RESIDENTIAL);
		pxMgr->GetGrid().GetCell(x, 201).SetZone(CB_ZONE_RESIDENTIAL);
	}
	pxMgr->GetSim().RunTicks(10);

	if (!(pxMgr->GetCitizens().GetActiveCitizens() > 0)) { Zenith_Log(LOG_CATEGORY_UNITTEST, "Citizens_Active: pool empty after growth"); return false; }
	return true;
}

static bool Step_Once(int iFrame) { return iFrame < 1; }

static const Zenith_AutomatedTest g_xCitSync    = { "CB_Citizens_Sync",        nullptr, &Step_Once, &Verify_CB_Citizens_Sync,        30, false };
static const Zenith_AutomatedTest g_xCitStraight = { "CB_Citizens_PathStraight", nullptr, &Step_Once, &Verify_CB_Citizens_PathStraight, 30, false };
static const Zenith_AutomatedTest g_xCitLShape  = { "CB_Citizens_PathLShape",   nullptr, &Step_Once, &Verify_CB_Citizens_PathLShape,  30, false };
static const Zenith_AutomatedTest g_xCitNoPath  = { "CB_Citizens_PathNoPath",   nullptr, &Step_Once, &Verify_CB_Citizens_PathNoPath,  30, false };
static const Zenith_AutomatedTest g_xCitActive  = { "CB_Citizens_Active",       nullptr, &Step_Once, &Verify_CB_Citizens_Active,      30, false };

ZENITH_AUTOMATED_TEST_REGISTER(g_xCitSync);
ZENITH_AUTOMATED_TEST_REGISTER(g_xCitStraight);
ZENITH_AUTOMATED_TEST_REGISTER(g_xCitLShape);
ZENITH_AUTOMATED_TEST_REGISTER(g_xCitNoPath);
ZENITH_AUTOMATED_TEST_REGISTER(g_xCitActive);

#endif // ZENITH_INPUT_SIMULATOR
