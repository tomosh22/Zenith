#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "CityBuilder/Source/CB_SaveLoadFreeform.h"
#include "CityBuilder/Source/CB_RoadGraph.h"
#include "CityBuilder/Source/CB_Zoning.h"
#include "CityBuilder/Source/CB_BuildingPlacement.h"
#include "CityBuilder/Source/CB_TerrainHeightfield.h"
#include "CityBuilder/Source/CB_Districts.h"
#include "CityBuilder/Source/CB_TransitLines.h"
#include "CityBuilder/Source/CB_Conduits.h"
#include "DataStream/Zenith_DataStream.h"

// ============================================================================
// CB_SaveLoad_Roundtrip — G6b gate (headless): build a free-form city (roads,
// zoning, buildings, services, economy), serialize it, deserialize into fresh
// instances, and assert every active count + population + treasury survives.
// Re-serializing the loaded city must produce the same byte length (stable).
// ============================================================================

namespace { using V2 = Zenith_Maths::Vector2; }

static bool Verify_CB_SaveLoad_Roundtrip()
{
	// --- build a city: an L of two roads, mixed zoning, utilities + a service ---
	CB_RoadGraph xG; CB_TerrainHeightfield xF; CB_Zoning xZ; CB_BuildingPlacement xB;
	xF.Init(257, 257, 16.0f, 0.0f, 0.0f, 200.0f);
	const uint32_t uA = xG.AddNode(V2(1000.0f, 1000.0f));
	const uint32_t uB = xG.AddNode(V2(1700.0f, 1000.0f));
	xG.AddSegment(uA, uB, CB_Spline::Straight(V2(1000.0f, 1000.0f), V2(1700.0f, 1000.0f)), CB_ROADCLASS_MEDIUM);
	const uint32_t uC = xG.AddNode(V2(1350.0f, 1000.0f));
	const uint32_t uD = xG.AddNode(V2(1350.0f, 1300.0f));
	xG.AddSegment(uC, uD, CB_Spline::Straight(V2(1350.0f, 1000.0f), V2(1350.0f, 1300.0f)), CB_ROADCLASS_SMALL);

	xZ.SyncToGraph(xG, xF);
	xZ.PaintZone(1350.0f, 1000.0f, 450.0f, CB_ZONE_RESIDENTIAL, 2);
	xZ.PaintZone(1500.0f, 1000.0f,  90.0f, CB_ZONE_COMMERCIAL,  2);

	xB.Reset();
	xB.PlaceService(CB_BUILDING_POWER_PLANT, 1350.0f, 1120.0f, 0.0f);
	xB.PlaceService(CB_BUILDING_WATER_TOWER, 1350.0f,  880.0f, 0.0f);
	xB.PlaceService(CB_BUILDING_POLICE,      1350.0f, 1000.0f, 0.0f);
	for (int i = 0; i < 600; ++i) { xB.Tick(xZ); }

	const uint32_t uSegs  = xG.GetActiveSegmentCount();
	const uint32_t uNodes = xG.GetActiveNodeCount();
	const uint32_t uLots  = xZ.GetActiveLotCount();
	const uint32_t uBldgs = xB.GetActiveBuildings();
	const uint32_t uSvcs  = xB.GetActiveServices();
	const uint32_t uPop   = xB.GetPopulation();
	const uint32_t uJobs  = xB.GetJobs();
	const float    fTreas = xB.GetTreasury();

	// Districts + policies: a city-wide ordinance + a district with its own.
	CB_Districts xDist;
	xDist.SetCityPolicy(CB_POLICY_RECYCLING, true);
	const uint32_t uDistIdx = xDist.PaintDistrict(1200.0f, 1000.0f);
	xDist.SetDistrictPolicy(uDistIdx, CB_POLICY_POLLUTION_CONTROL, true);
	const uint32_t uCityMask  = xDist.GetCityPolicyMask();
	const uint32_t uDistCount = xDist.GetActiveCount();

	// Transit line: a couple of stops.
	CB_TransitLines xTransit;
	xTransit.StartLine();
	xTransit.AddStop(1100.0f, 1000.0f); xTransit.AddStop(1600.0f, 1000.0f);
	const uint32_t uStops = xTransit.GetStopCount();

	// Utility conduits: a short chain.
	CB_Conduits xConduits;
	xConduits.AddConduit(1300.0f, 1000.0f); xConduits.AddConduit(1400.0f, 1000.0f); xConduits.AddConduit(1500.0f, 1000.0f);
	const uint32_t uPipes = xConduits.GetCount();

	// --- save ---
	Zenith_DataStream xStream;
	CB_SaveLoadFreeform::Save(xG, xZ, xB, xDist, xTransit, xConduits, xStream);
	const uint64_t ulSavedBytes = xStream.GetCursor();

	// --- load into FRESH instances ---
	xStream.SetCursor(0);
	CB_RoadGraph xG2; CB_Zoning xZ2; CB_BuildingPlacement xB2; CB_Districts xDist2; CB_TransitLines xTransit2; CB_Conduits xConduits2;
	const bool bLoaded = CB_SaveLoadFreeform::Load(xG2, xZ2, xB2, xDist2, xTransit2, xConduits2, xStream);

	bool bOk = true;
	if (!bLoaded)                                    { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_SaveLoad: load returned false"); return false; }
	if (uBldgs == 0 || uPop == 0)                    { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_SaveLoad: precondition — city didn't grow"); bOk = false; }
	if (xG2.GetActiveSegmentCount() != uSegs)        { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_SaveLoad: segments %u != %u", xG2.GetActiveSegmentCount(), uSegs); bOk = false; }
	if (xG2.GetActiveNodeCount()    != uNodes)       { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_SaveLoad: nodes %u != %u", xG2.GetActiveNodeCount(), uNodes); bOk = false; }
	if (xZ2.GetActiveLotCount()     != uLots)        { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_SaveLoad: lots %u != %u", xZ2.GetActiveLotCount(), uLots); bOk = false; }
	if (xB2.GetActiveBuildings()    != uBldgs)       { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_SaveLoad: buildings %u != %u", xB2.GetActiveBuildings(), uBldgs); bOk = false; }
	if (xB2.GetActiveServices()     != uSvcs)        { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_SaveLoad: services %u != %u", xB2.GetActiveServices(), uSvcs); bOk = false; }
	if (xB2.GetPopulation()         != uPop)         { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_SaveLoad: population %u != %u", xB2.GetPopulation(), uPop); bOk = false; }
	if (xB2.GetJobs()               != uJobs)        { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_SaveLoad: jobs %u != %u", xB2.GetJobs(), uJobs); bOk = false; }
	const float fDelta = (xB2.GetTreasury() > fTreas) ? (xB2.GetTreasury() - fTreas) : (fTreas - xB2.GetTreasury());
	if (fDelta > 0.5f)                               { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_SaveLoad: treasury %.1f != %.1f", xB2.GetTreasury(), fTreas); bOk = false; }
	if (xDist2.GetCityPolicyMask() != uCityMask)     { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_SaveLoad: city policy mask %u != %u", xDist2.GetCityPolicyMask(), uCityMask); bOk = false; }
	if (xDist2.GetActiveCount()    != uDistCount)    { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_SaveLoad: district count %u != %u", xDist2.GetActiveCount(), uDistCount); bOk = false; }
	if (uDistCount > 0 && xDist2.Get(uDistIdx).m_uPolicyMask != xDist.Get(uDistIdx).m_uPolicyMask) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_SaveLoad: district policy mask mismatch"); bOk = false; }
	if (xTransit2.GetStopCount() != uStops)          { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_SaveLoad: transit stops %u != %u", xTransit2.GetStopCount(), uStops); bOk = false; }
	if (xConduits2.GetCount() != uPipes)             { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_SaveLoad: conduits %u != %u", xConduits2.GetCount(), uPipes); bOk = false; }

	// --- re-serialize the loaded city → identical byte length ---
	Zenith_DataStream xStream2;
	CB_SaveLoadFreeform::Save(xG2, xZ2, xB2, xDist2, xTransit2, xConduits2, xStream2);
	if (xStream2.GetCursor() != ulSavedBytes)        { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_SaveLoad: re-save %llu != %llu bytes", xStream2.GetCursor(), ulSavedBytes); bOk = false; }

	Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_SaveLoad: %u segs, %u lots, %u bldgs, %u svcs, pop %u, $%.0f, %llu bytes",
		uSegs, uLots, uBldgs, uSvcs, uPop, fTreas, ulSavedBytes);
	return bOk;
}

static bool Step_Once(int iFrame) { return iFrame < 1; }

static const Zenith_AutomatedTest g_xSaveLoadRoundtrip = { "CB_SaveLoad_Roundtrip", nullptr, &Step_Once, &Verify_CB_SaveLoad_Roundtrip, 30, false };
ZENITH_AUTOMATED_TEST_REGISTER(g_xSaveLoadRoundtrip);

#endif // ZENITH_INPUT_SIMULATOR
