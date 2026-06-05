#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "CityBuilder/Source/CB_BuildingPlacement.h"
#include "CityBuilder/Source/CB_Zoning.h"
#include "CityBuilder/Source/CB_RoadGraph.h"
#include "CityBuilder/Source/CB_TerrainHeightfield.h"

// ============================================================================
// CB_BuildingGrow — G4 gate (headless): demand-driven growth in zoned frontage
// lots. A mixed-zone road grows residents + jobs via the RCI feedback; bulldozing
// the road despawns its buildings. Pure logic, built locally.
// ============================================================================

namespace { using V2 = Zenith_Maths::Vector2; }

static bool Verify_CB_Building_Grow()
{
	CB_RoadGraph xG;
	CB_TerrainHeightfield xF;
	xF.Init(257, 257, 16.0f, 0.0f, 0.0f, 200.0f);
	const uint32_t uA = xG.AddNode(V2(1000.0f, 1000.0f));
	const uint32_t uB = xG.AddNode(V2(1600.0f, 1000.0f));
	xG.AddSegment(uA, uB, CB_Spline::Straight(V2(1000.0f, 1000.0f), V2(1600.0f, 1000.0f)), CB_ROADCLASS_MEDIUM);

	CB_Zoning xZ;
	xZ.SyncToGraph(xG, xF);
	// Mostly residential, with a small commercial patch (the RCI feedback grows
	// the commercial once residents arrive). Paint order matters: the broad
	// residential first, then the narrow commercial overwrites a few lots.
	xZ.PaintZone(1300.0f, 1000.0f, 500.0f, CB_ZONE_RESIDENTIAL, 2);
	xZ.PaintZone(1480.0f, 1000.0f,  35.0f, CB_ZONE_COMMERCIAL,  2);

	CB_BuildingPlacement xBP;
	xBP.Reset();
	// Utilities so growth isn't capped at the free baseline (Cities: Skylines gating).
	xBP.PlaceService(CB_BUILDING_POWER_PLANT, 1300.0f, 1080.0f, 0.0f);
	xBP.PlaceService(CB_BUILDING_WATER_TOWER, 1300.0f,  920.0f, 0.0f);
	for (int i = 0; i < 800; ++i) { xBP.Tick(xZ); }

	bool bOk = true;
	if (xBP.GetActiveBuildings() < 2) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Building_Grow: too few buildings (%u)", xBP.GetActiveBuildings()); bOk = false; }
	if (xBP.GetPopulation() == 0)     { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Building_Grow: no residents"); bOk = false; }
	if (xBP.GetJobs() == 0)           { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Building_Grow: no jobs (commercial didn't grow)"); bOk = false; }
	return bOk;
}

static bool Verify_CB_Building_Despawn()
{
	CB_RoadGraph xG;
	CB_TerrainHeightfield xF;
	xF.Init(257, 257, 16.0f, 0.0f, 0.0f, 200.0f);
	const uint32_t uA = xG.AddNode(V2(1000.0f, 1000.0f));
	const uint32_t uB = xG.AddNode(V2(1300.0f, 1000.0f));
	const uint32_t uSeg = xG.AddSegment(uA, uB, CB_Spline::Straight(V2(1000.0f, 1000.0f), V2(1300.0f, 1000.0f)), CB_ROADCLASS_SMALL);

	CB_Zoning xZ;
	xZ.SyncToGraph(xG, xF);
	xZ.PaintZone(1150.0f, 1000.0f, 500.0f, CB_ZONE_RESIDENTIAL, 2);

	CB_BuildingPlacement xBP;
	xBP.Reset();
	for (int i = 0; i < 400; ++i) { xBP.Tick(xZ); }
	if (xBP.GetActiveBuildings() == 0) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Building_Despawn: precondition (no buildings grew)"); return false; }

	xG.RemoveSegment(uSeg);
	xZ.SyncToGraph(xG, xF);
	for (int i = 0; i < 30; ++i) { xBP.Tick(xZ); }

	bool bOk = true;
	if (xBP.GetActiveBuildings() != 0) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Building_Despawn: buildings not despawned (%u)", xBP.GetActiveBuildings()); bOk = false; }
	if (xBP.GetPopulation() != 0)      { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Building_Despawn: population not zero"); bOk = false; }
	return bOk;
}

static bool Step_Once(int iFrame) { return iFrame < 1; }

static const Zenith_AutomatedTest g_xBuildingGrow    = { "CB_Building_Grow",    nullptr, &Step_Once, &Verify_CB_Building_Grow,    30, false };
static const Zenith_AutomatedTest g_xBuildingDespawn = { "CB_Building_Despawn", nullptr, &Step_Once, &Verify_CB_Building_Despawn, 30, false };

ZENITH_AUTOMATED_TEST_REGISTER(g_xBuildingGrow);
ZENITH_AUTOMATED_TEST_REGISTER(g_xBuildingDespawn);

#endif // ZENITH_INPUT_SIMULATOR
