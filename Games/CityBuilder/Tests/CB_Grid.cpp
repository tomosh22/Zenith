#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "CityBuilder/Source/CB_CityGrid.h"
#include "CityBuilder/Components/CB_CityManager_Behaviour.h"

// ============================================================================
// CB_Grid — Phase-3 gate. Exercises the CB_CityGrid spatial model: world<->grid
// round-trips, the 16-byte packed cell accessors, zone painting/clearing, road
// access flood-fill, and the CityManager wiring. Logic-only (headless). Local
// grids use 256x256 to stay snappy; the full 1024x1024 is checked via the
// CityManager's active grid.
// ============================================================================

// ---- world<->grid round-trips + bounds ----
static bool Verify_CB_Grid_RoundTrip()
{
	CB_CityGrid xGrid;
	xGrid.Initialize(256, 256, 4.0f, 0.0f, 0.0f);

	bool bOk = true;
	const uint32_t auCoords[][2] = { {0,0}, {1,1}, {128,200}, {255,255}, {37,99} };
	for (const auto& xc : auCoords)
	{
		float fwx, fwz;
		xGrid.GridToWorld(xc[0], xc[1], fwx, fwz);
		uint32_t rx = 0, rz = 0;
		if (!xGrid.WorldToGrid(fwx, fwz, rx, rz) || rx != xc[0] || rz != xc[1])
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Grid_RoundTrip: (%u,%u) -> world -> (%u,%u)", xc[0], xc[1], rx, rz);
			bOk = false;
		}
	}
	// Out-of-bounds rejects.
	uint32_t ux, uz;
	if (xGrid.WorldToGrid(-1.0f, 10.0f, ux, uz))      { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Grid_RoundTrip: negative not rejected"); bOk = false; }
	if (xGrid.WorldToGrid(99999.0f, 10.0f, ux, uz))   { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Grid_RoundTrip: far not rejected"); bOk = false; }
	return bOk;
}

// ---- 16-byte cell + bit-packed accessors ----
static bool Verify_CB_Grid_Cell()
{
	bool bOk = true;

	// sizeof(CB_Cell)==16 is enforced at compile time by static_assert in CB_CityGrid.h.
	CB_Cell xCell;
	xCell.SetZone(CB_ZONE_COMMERCIAL);
	xCell.SetDensity(2);
	xCell.SetPower(true);
	xCell.SetWater(false);
	xCell.SetRoadType(CB_ROAD_MEDIUM);
	xCell.SetRoadAccess(true);
	xCell.SetRoadMask(CB_ROAD_DIR_NORTH | CB_ROAD_DIR_SOUTH);

	if (xCell.GetZone() != CB_ZONE_COMMERCIAL) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Grid_Cell: zone"); bOk = false; }
	if (xCell.GetDensity() != 2)               { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Grid_Cell: density"); bOk = false; }
	if (!xCell.HasPower())                      { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Grid_Cell: power"); bOk = false; }
	if (xCell.HasWater())                       { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Grid_Cell: water"); bOk = false; }
	if (xCell.GetRoadType() != CB_ROAD_MEDIUM) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Grid_Cell: roadtype"); bOk = false; }
	if (!xCell.HasRoad())                       { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Grid_Cell: hasroad"); bOk = false; }
	if (!xCell.HasRoadAccess())                 { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Grid_Cell: roadaccess"); bOk = false; }
	if (xCell.GetRoadMask() != (CB_ROAD_DIR_NORTH | CB_ROAD_DIR_SOUTH)) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Grid_Cell: roadmask"); bOk = false; }
	return bOk;
}

// ---- zone painting + clearing + road-skip ----
static bool Verify_CB_Grid_PaintZone()
{
	CB_CityGrid xGrid;
	xGrid.Initialize(256, 256, 4.0f, 0.0f, 0.0f);

	xGrid.PaintZone(100, 100, 3, CB_ZONE_RESIDENTIAL, 1);

	bool bOk = true;
	// Inside the 7x7 brush.
	if (xGrid.GetCell(100, 100).GetZone() != CB_ZONE_RESIDENTIAL) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Grid_PaintZone: centre not zoned"); bOk = false; }
	if (xGrid.GetCell(103, 97).GetZone()  != CB_ZONE_RESIDENTIAL) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Grid_PaintZone: corner not zoned"); bOk = false; }
	if (xGrid.GetCell(100, 100).GetDensity() != 1)               { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Grid_PaintZone: density"); bOk = false; }
	// Outside the brush.
	if (xGrid.GetCell(150, 150).GetZone() != CB_ZONE_NONE)       { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Grid_PaintZone: far cell zoned"); bOk = false; }

	// Road cells are skipped by zone painting.
	xGrid.GetCell(100, 100).SetRoadType(CB_ROAD_SMALL);
	xGrid.PaintZone(100, 100, 0, CB_ZONE_INDUSTRIAL, 0);
	if (xGrid.GetCell(100, 100).GetZone() == CB_ZONE_INDUSTRIAL) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Grid_PaintZone: road got zoned"); bOk = false; }

	// Clear.
	xGrid.ClearZone(100, 100, 3);
	if (xGrid.GetCell(101, 101).GetZone() != CB_ZONE_NONE)       { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Grid_PaintZone: clear failed"); bOk = false; }
	return bOk;
}

// ---- road access flood-fill ----
static bool Verify_CB_Grid_RoadAccess()
{
	CB_CityGrid xGrid;
	xGrid.Initialize(64, 64, 4.0f, 0.0f, 0.0f);

	xGrid.GetCell(10, 10).SetRoadType(CB_ROAD_SMALL);
	xGrid.RecalculateRoadAccess();

	bool bOk = true;
	if (!xGrid.GetCell(10, 10).HasRoadAccess()) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Grid_RoadAccess: road cell no access"); bOk = false; }
	if (!xGrid.GetCell(11, 10).HasRoadAccess()) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Grid_RoadAccess: east neighbour no access"); bOk = false; }
	if (!xGrid.GetCell(10, 11).HasRoadAccess()) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Grid_RoadAccess: south neighbour no access"); bOk = false; }
	if (xGrid.GetCell(20, 20).HasRoadAccess())  { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Grid_RoadAccess: far cell has access"); bOk = false; }
	return bOk;
}

// ---- CityManager publishes a live 1024x1024 grid ----
static bool Verify_CB_Grid_Active()
{
	CB_CityGrid* pxGrid = CB_CityManager_Behaviour::GetActiveGrid();
	if (pxGrid == nullptr)        { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Grid_Active: no active grid"); return false; }
	if (!pxGrid->IsInitialized()) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Grid_Active: grid not initialised"); return false; }
	if (pxGrid->GetWidth() != 1024 || pxGrid->GetHeight() != 1024) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Grid_Active: wrong size %ux%u", pxGrid->GetWidth(), pxGrid->GetHeight()); return false; }
	return true;
}

static bool Step_Once(int iFrame) { return iFrame < 1; }

static const Zenith_AutomatedTest g_xGridRoundTrip  = { "CB_Grid_RoundTrip",  nullptr, &Step_Once, &Verify_CB_Grid_RoundTrip,  30, false };
static const Zenith_AutomatedTest g_xGridCell       = { "CB_Grid_Cell",       nullptr, &Step_Once, &Verify_CB_Grid_Cell,       30, false };
static const Zenith_AutomatedTest g_xGridPaintZone  = { "CB_Grid_PaintZone",  nullptr, &Step_Once, &Verify_CB_Grid_PaintZone,  30, false };
static const Zenith_AutomatedTest g_xGridRoadAccess = { "CB_Grid_RoadAccess", nullptr, &Step_Once, &Verify_CB_Grid_RoadAccess, 30, false };
static const Zenith_AutomatedTest g_xGridActive     = { "CB_Grid_Active",     nullptr, &Step_Once, &Verify_CB_Grid_Active,     30, false };

ZENITH_AUTOMATED_TEST_REGISTER(g_xGridRoundTrip);
ZENITH_AUTOMATED_TEST_REGISTER(g_xGridCell);
ZENITH_AUTOMATED_TEST_REGISTER(g_xGridPaintZone);
ZENITH_AUTOMATED_TEST_REGISTER(g_xGridRoadAccess);
ZENITH_AUTOMATED_TEST_REGISTER(g_xGridActive);

#endif // ZENITH_INPUT_SIMULATOR
