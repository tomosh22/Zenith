#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "CityBuilder/Source/CB_CityGrid.h"
#include "CityBuilder/Source/CB_RoadNetwork.h"
#include "CityBuilder/Components/CB_CityManager_Behaviour.h"

// ============================================================================
// CB_Roads — Phase-4 gate. The road piece truth table, placement/removal with
// auto neighbour-piece updates, road-access propagation, and building-blocks-
// road. Logic-only (headless).
// ============================================================================

static bool Verify_CB_Roads_PieceTruthTable()
{
	const uint8_t N = CB_ROAD_DIR_NORTH, E = CB_ROAD_DIR_EAST, S = CB_ROAD_DIR_SOUTH, W = CB_ROAD_DIR_WEST;
	struct Case { uint8_t uMask; CB_ERoadPiece ePiece; const char* sz; };
	const Case axCases[] = {
		{ static_cast<uint8_t>(N | S),         CB_ROAD_PIECE_STRAIGHT_NS,  "NS" },
		{ static_cast<uint8_t>(E | W),         CB_ROAD_PIECE_STRAIGHT_EW,  "EW" },
		{ static_cast<uint8_t>(N | E),         CB_ROAD_PIECE_CORNER_NE,    "NE" },
		{ static_cast<uint8_t>(S | E),         CB_ROAD_PIECE_CORNER_SE,    "SE" },
		{ static_cast<uint8_t>(S | W),         CB_ROAD_PIECE_CORNER_SW,    "SW" },
		{ static_cast<uint8_t>(N | W),         CB_ROAD_PIECE_CORNER_NW,    "NW" },
		{ static_cast<uint8_t>(N | E | S),     CB_ROAD_PIECE_T_E,          "T_E" },
		{ static_cast<uint8_t>(E | S | W),     CB_ROAD_PIECE_T_S,          "T_S" },
		{ static_cast<uint8_t>(N | S | W),     CB_ROAD_PIECE_T_W,          "T_W" },
		{ static_cast<uint8_t>(N | E | W),     CB_ROAD_PIECE_T_N,          "T_N" },
		{ static_cast<uint8_t>(N | E | S | W), CB_ROAD_PIECE_INTERSECTION, "X" },
	};
	bool bOk = true;
	for (const Case& xc : axCases)
	{
		const CB_ERoadPiece eGot = CB_RoadNetwork::DeterminePieceFromMask(xc.uMask);
		if (eGot != xc.ePiece)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Roads_PieceTruthTable: mask %s -> %d expected %d", xc.sz, static_cast<int>(eGot), static_cast<int>(xc.ePiece));
			bOk = false;
		}
	}
	return bOk;
}

static bool Verify_CB_Roads_Place()
{
	CB_CityGrid xGrid;
	xGrid.Initialize(64, 64, 4.0f, 0.0f, 0.0f);
	CB_RoadNetwork xRoads;
	xRoads.Initialize(&xGrid);

	// Horizontal run of 3 (varying X at fixed Z).
	xRoads.PlaceRoad(10, 10, CB_ROAD_SMALL);
	xRoads.PlaceRoad(11, 10, CB_ROAD_SMALL);
	xRoads.PlaceRoad(12, 10, CB_ROAD_SMALL);

	bool bOk = true;
	if (xRoads.GetRoadCellCount() != 3) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Roads_Place: count %u", xRoads.GetRoadCellCount()); bOk = false; }
	if (xRoads.GetPieceAt(11, 10) != CB_ROAD_PIECE_STRAIGHT_EW) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Roads_Place: middle not EW (%d)", static_cast<int>(xRoads.GetPieceAt(11, 10))); bOk = false; }
	if (!xGrid.GetCell(11, 10).HasRoad())       { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Roads_Place: middle not road"); bOk = false; }
	if (!xGrid.GetCell(11, 11).HasRoadAccess()) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Roads_Place: adjacent cell no access"); bOk = false; }
	if (xGrid.GetCell(30, 30).HasRoadAccess())  { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Roads_Place: far cell has access"); bOk = false; }
	return bOk;
}

static bool Verify_CB_Roads_Corner()
{
	CB_CityGrid xGrid;
	xGrid.Initialize(64, 64, 4.0f, 0.0f, 0.0f);
	CB_RoadNetwork xRoads;
	xRoads.Initialize(&xGrid);

	// L-shape: (10,10)-(10,11) vertical, (10,11)-(11,11) horizontal.
	xRoads.PlaceRoad(10, 10, CB_ROAD_SMALL);
	xRoads.PlaceRoad(10, 11, CB_ROAD_SMALL);
	xRoads.PlaceRoad(11, 11, CB_ROAD_SMALL);

	// (10,11) connects North (10,10) and East (11,11) -> corner NE.
	bool bOk = true;
	if (xRoads.GetPieceAt(10, 11) != CB_ROAD_PIECE_CORNER_NE) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Roads_Corner: not NE corner (%d)", static_cast<int>(xRoads.GetPieceAt(10, 11))); bOk = false; }
	return bOk;
}

static bool Verify_CB_Roads_Remove()
{
	CB_CityGrid xGrid;
	xGrid.Initialize(64, 64, 4.0f, 0.0f, 0.0f);
	CB_RoadNetwork xRoads;
	xRoads.Initialize(&xGrid);

	xRoads.PlaceRoad(5, 5, CB_ROAD_SMALL);
	xRoads.PlaceRoad(6, 5, CB_ROAD_SMALL);
	const bool bRemoved = xRoads.RemoveRoad(6, 5);

	bool bOk = true;
	if (!bRemoved)                          { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Roads_Remove: remove returned false"); bOk = false; }
	if (xRoads.GetRoadCellCount() != 1)     { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Roads_Remove: count %u", xRoads.GetRoadCellCount()); bOk = false; }
	if (xGrid.GetCell(6, 5).HasRoad())      { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Roads_Remove: still road"); bOk = false; }
	if (xRoads.RemoveRoad(40, 40))          { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Roads_Remove: removed empty cell"); bOk = false; }
	return bOk;
}

static bool Verify_CB_Roads_BuildingBlocks()
{
	CB_CityGrid xGrid;
	xGrid.Initialize(64, 64, 4.0f, 0.0f, 0.0f);
	CB_RoadNetwork xRoads;
	xRoads.Initialize(&xGrid);

	xGrid.GetCell(20, 20).SetBuilding(0, 0);  // occupied
	const bool bPlaced = xRoads.PlaceRoad(20, 20, CB_ROAD_SMALL);

	bool bOk = true;
	if (bPlaced)                        { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Roads_BuildingBlocks: road placed on building"); bOk = false; }
	if (xGrid.GetCell(20, 20).HasRoad()){ Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Roads_BuildingBlocks: cell became road"); bOk = false; }
	return bOk;
}

static bool Verify_CB_Roads_Active()
{
	CB_RoadNetwork* pxRoads = CB_CityManager_Behaviour::GetActiveRoads();
	CB_CityGrid* pxGrid = CB_CityManager_Behaviour::GetActiveGrid();
	if (pxRoads == nullptr || pxGrid == nullptr) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Roads_Active: no active road network"); return false; }
	if (!pxRoads->IsInitialized())               { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Roads_Active: not initialised"); return false; }
	if (!pxRoads->PlaceRoad(500, 500, CB_ROAD_MEDIUM)) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Roads_Active: place failed"); return false; }
	if (!pxGrid->GetCell(500, 500).HasRoad())    { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Roads_Active: cell not road after place"); return false; }
	return true;
}

static bool Step_Once(int iFrame) { return iFrame < 1; }

static const Zenith_AutomatedTest g_xRoadsTruth   = { "CB_Roads_PieceTruthTable", nullptr, &Step_Once, &Verify_CB_Roads_PieceTruthTable, 30, false };
static const Zenith_AutomatedTest g_xRoadsPlace   = { "CB_Roads_Place",           nullptr, &Step_Once, &Verify_CB_Roads_Place,           30, false };
static const Zenith_AutomatedTest g_xRoadsCorner  = { "CB_Roads_Corner",          nullptr, &Step_Once, &Verify_CB_Roads_Corner,          30, false };
static const Zenith_AutomatedTest g_xRoadsRemove  = { "CB_Roads_Remove",          nullptr, &Step_Once, &Verify_CB_Roads_Remove,          30, false };
static const Zenith_AutomatedTest g_xRoadsBlocks  = { "CB_Roads_BuildingBlocks",  nullptr, &Step_Once, &Verify_CB_Roads_BuildingBlocks,  30, false };
static const Zenith_AutomatedTest g_xRoadsActive  = { "CB_Roads_Active",          nullptr, &Step_Once, &Verify_CB_Roads_Active,          30, false };

ZENITH_AUTOMATED_TEST_REGISTER(g_xRoadsTruth);
ZENITH_AUTOMATED_TEST_REGISTER(g_xRoadsPlace);
ZENITH_AUTOMATED_TEST_REGISTER(g_xRoadsCorner);
ZENITH_AUTOMATED_TEST_REGISTER(g_xRoadsRemove);
ZENITH_AUTOMATED_TEST_REGISTER(g_xRoadsBlocks);
ZENITH_AUTOMATED_TEST_REGISTER(g_xRoadsActive);

#endif // ZENITH_INPUT_SIMULATOR
