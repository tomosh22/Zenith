#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "CityBuilder/Source/CB_Zoning.h"
#include "CityBuilder/Source/CB_RoadGraph.h"
#include "CityBuilder/Source/CB_TerrainHeightfield.h"

// ============================================================================
// CB_ZoningLots — G3 gate (headless): road-relative frontage lots. SyncToGraph
// generates lots along a segment's frontages; PaintZone zones them; removing the
// segment + re-syncing frees its lots. Pure logic, built locally.
// ============================================================================

namespace { using V2 = Zenith_Maths::Vector2; }

static bool Verify_CB_Zoning_Lots()
{
	CB_RoadGraph xG;
	CB_TerrainHeightfield xF;
	xF.Init(257, 257, 16.0f, 0.0f, 0.0f, 200.0f);

	const uint32_t uA = xG.AddNode(V2(1000.0f, 1000.0f));
	const uint32_t uB = xG.AddNode(V2(1200.0f, 1000.0f));
	xG.AddSegment(uA, uB, CB_Spline::Straight(V2(1000.0f, 1000.0f), V2(1200.0f, 1000.0f)), CB_ROADCLASS_MEDIUM);

	CB_Zoning xZ;
	xZ.SyncToGraph(xG, xF);

	bool bOk = true;
	if (xZ.GetActiveLotCount() < 8)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Zoning_Lots: too few lots (%u)", xZ.GetActiveLotCount());
		bOk = false;
	}
	// Paint residential over the middle of the road (reaches both frontages).
	const uint32_t uPainted = xZ.PaintZone(1100.0f, 1000.0f, 25.0f, CB_ZONE_RESIDENTIAL, 2);
	if (uPainted == 0)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Zoning_Lots: paint hit no lots");
		bOk = false;
	}
	if (xZ.CountZonedLots(CB_ZONE_RESIDENTIAL) == 0)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Zoning_Lots: no residential lots after paint");
		bOk = false;
	}
	// A point far from any road paints nothing.
	if (xZ.PaintZone(3000.0f, 3000.0f, 25.0f, CB_ZONE_COMMERCIAL, 2) != 0)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Zoning_Lots: painted far from roads");
		bOk = false;
	}
	return bOk;
}

static bool Verify_CB_Zoning_SegRemove()
{
	CB_RoadGraph xG;
	CB_TerrainHeightfield xF;
	xF.Init(257, 257, 16.0f, 0.0f, 0.0f, 200.0f);

	const uint32_t uA = xG.AddNode(V2(1000.0f, 1000.0f));
	const uint32_t uB = xG.AddNode(V2(1200.0f, 1000.0f));
	const uint32_t uSeg = xG.AddSegment(uA, uB, CB_Spline::Straight(V2(1000.0f, 1000.0f), V2(1200.0f, 1000.0f)), CB_ROADCLASS_SMALL);

	CB_Zoning xZ;
	xZ.SyncToGraph(xG, xF);
	if (xZ.GetActiveLotCount() == 0)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Zoning_SegRemove: precondition (no lots)");
		return false;
	}

	xG.RemoveSegment(uSeg);
	xZ.SyncToGraph(xG, xF);

	bool bOk = true;
	if (xZ.GetActiveLotCount() != 0)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Zoning_SegRemove: lots not freed (%u)", xZ.GetActiveLotCount());
		bOk = false;
	}
	return bOk;
}

static bool Step_Once(int iFrame) { return iFrame < 1; }

static const Zenith_AutomatedTest g_xZoningLots      = { "CB_Zoning_Lots",      nullptr, &Step_Once, &Verify_CB_Zoning_Lots,      30, false };
static const Zenith_AutomatedTest g_xZoningSegRemove = { "CB_Zoning_SegRemove", nullptr, &Step_Once, &Verify_CB_Zoning_SegRemove, 30, false };

ZENITH_AUTOMATED_TEST_REGISTER(g_xZoningLots);
ZENITH_AUTOMATED_TEST_REGISTER(g_xZoningSegRemove);

#endif // ZENITH_INPUT_SIMULATOR
