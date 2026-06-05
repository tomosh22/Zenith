#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "CityBuilder/Source/CB_BuildingPlacement.h"
#include "CityBuilder/Source/CB_Zoning.h"
#include "CityBuilder/Source/CB_RoadGraph.h"
#include "CityBuilder/Source/CB_TerrainHeightfield.h"
#include "CityBuilder/Source/CB_Districts.h"
#include "CityBuilder/Source/CB_TransitLines.h"
#include "CityBuilder/Source/CB_Conduits.h"

// ============================================================================
// CB_CityServices — G5 gate (headless): the free-form city sim. Utilities gate
// growth (power/water caps), service coverage raises land value (happiness), and
// the economy moves the treasury (build costs + tax income − upkeep). Pure logic.
// ============================================================================

namespace
{
	using V2 = Zenith_Maths::Vector2;

	// A long street zoned mixed R/C/I. The commercial + industrial clusters give
	// the residential demand the jobs it needs to keep growing (without them, the
	// (30 - residents) residential formula self-limits at ~4 buildings), so the
	// city grows large enough for utility caps / coverage / economy to bite.
	void BuildRoadZone(CB_RoadGraph& xG, CB_TerrainHeightfield& xF, CB_Zoning& xZ)
	{
		xF.Init(257, 257, 16.0f, 0.0f, 0.0f, 200.0f);
		const uint32_t uA = xG.AddNode(V2(1000.0f, 1000.0f));
		const uint32_t uB = xG.AddNode(V2(1700.0f, 1000.0f));
		xG.AddSegment(uA, uB, CB_Spline::Straight(V2(1000.0f, 1000.0f), V2(1700.0f, 1000.0f)), CB_ROADCLASS_MEDIUM);
		xZ.SyncToGraph(xG, xF);
		xZ.PaintZone(1350.0f, 1000.0f, 600.0f, CB_ZONE_RESIDENTIAL, 2);  // residential along the street
		xZ.PaintZone(1530.0f, 1000.0f, 110.0f, CB_ZONE_COMMERCIAL,  2);  // commercial cluster
		xZ.PaintZone(1170.0f, 1000.0f, 110.0f, CB_ZONE_INDUSTRIAL,  2);  // industrial cluster
	}
}

// Utilities raise the power/water cap → a powered city outgrows the baseline-capped one.
static bool Verify_CB_Service_PowerGating()
{
	CB_RoadGraph xG1; CB_TerrainHeightfield xF1; CB_Zoning xZ1;
	BuildRoadZone(xG1, xF1, xZ1);
	CB_BuildingPlacement xNoUtil; xNoUtil.Reset();
	for (int i = 0; i < 800; ++i) { xNoUtil.Tick(xZ1); }
	const uint32_t uCapped = xNoUtil.GetActiveBuildings();

	CB_RoadGraph xG2; CB_TerrainHeightfield xF2; CB_Zoning xZ2;
	BuildRoadZone(xG2, xF2, xZ2);
	CB_BuildingPlacement xUtil; xUtil.Reset();
	xUtil.PlaceService(CB_BUILDING_POWER_PLANT, 1350.0f, 1100.0f, 0.0f);
	xUtil.PlaceService(CB_BUILDING_WATER_TOWER, 1350.0f,  900.0f, 0.0f);
	for (int i = 0; i < 800; ++i) { xUtil.Tick(xZ2); }
	const uint32_t uPowered = xUtil.GetActiveBuildings();

	bool bOk = true;
	if (uCapped == 0)        { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Service_PowerGating: baseline allowed no growth"); bOk = false; }
	if (uPowered <= uCapped) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Service_PowerGating: utilities didn't raise the cap (powered %u <= capped %u)", uPowered, uCapped); bOk = false; }
	Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Service_PowerGating: baseline-capped=%u, powered=%u", uCapped, uPowered);
	return bOk;
}

// Placing coverage services (police/fire/health/edu) raises served fraction + happiness.
static bool Verify_CB_Service_Coverage()
{
	CB_RoadGraph xG; CB_TerrainHeightfield xF; CB_Zoning xZ;
	BuildRoadZone(xG, xF, xZ);
	CB_BuildingPlacement xB; xB.Reset();
	xB.PlaceService(CB_BUILDING_POWER_PLANT, 1350.0f, 1100.0f, 0.0f);
	xB.PlaceService(CB_BUILDING_WATER_TOWER, 1350.0f,  900.0f, 0.0f);
	for (int i = 0; i < 400; ++i) { xB.Tick(xZ); }
	const float fServedBefore = xB.GetServedFraction();
	const float fHappyBefore  = xB.GetHappiness();

	xB.PlaceService(CB_BUILDING_POLICE,   1350.0f, 1000.0f, 0.0f);
	xB.PlaceService(CB_BUILDING_FIRE,     1350.0f, 1000.0f, 0.0f);
	xB.PlaceService(CB_BUILDING_HOSPITAL, 1350.0f, 1000.0f, 0.0f);
	xB.PlaceService(CB_BUILDING_SCHOOL,   1350.0f, 1000.0f, 0.0f);
	xB.PlaceService(CB_BUILDING_PARK,     1350.0f, 1000.0f, 0.0f);   // 5th coverage type (amenity)
	for (int i = 0; i < 60; ++i) { xB.Tick(xZ); }
	const float fServedAfter = xB.GetServedFraction();
	const float fHappyAfter  = xB.GetHappiness();

	bool bOk = true;
	if (!(fServedAfter > fServedBefore + 0.10f)) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Service_Coverage: served didn't rise (%.2f -> %.2f)", fServedBefore, fServedAfter); bOk = false; }
	if (!(fHappyAfter  > fHappyBefore))          { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Service_Coverage: happiness didn't rise (%.2f -> %.2f)", fHappyBefore, fHappyAfter); bOk = false; }
	Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Service_Coverage: served %.2f->%.2f, happiness %.2f->%.2f", fServedBefore, fServedAfter, fHappyBefore, fHappyAfter);
	return bOk;
}

// Build costs deduct funds; a profitable city's treasury then grows from tax.
static bool Verify_CB_Service_Economy()
{
	CB_RoadGraph xG; CB_TerrainHeightfield xF; CB_Zoning xZ;
	BuildRoadZone(xG, xF, xZ);
	CB_BuildingPlacement xB; xB.Reset();
	const float fStart = xB.GetTreasury();
	xB.PlaceService(CB_BUILDING_POWER_PLANT, 1350.0f, 1100.0f, 0.0f);
	xB.PlaceService(CB_BUILDING_WATER_TOWER, 1350.0f,  900.0f, 0.0f);
	const float fAfterBuy = xB.GetTreasury();
	for (int i = 0; i < 600; ++i) { xB.Tick(xZ); }
	const float fEnd = xB.GetTreasury();

	bool bOk = true;
	if (!(fAfterBuy < fStart))   { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Service_Economy: placement didn't cost (%.0f -> %.0f)", fStart, fAfterBuy); bOk = false; }
	if (!(fEnd > fAfterBuy))     { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Service_Economy: profitable city's treasury didn't grow (%.0f -> %.0f)", fAfterBuy, fEnd); bOk = false; }
	Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Service_Economy: start=%.0f afterBuy=%.0f end=%.0f", fStart, fAfterBuy, fEnd);
	return bOk;
}

// Traffic congestion (population vs road capacity) erodes happiness; ample road relieves it.
static bool Verify_CB_Service_Congestion()
{
	CB_RoadGraph xG; CB_TerrainHeightfield xF; CB_Zoning xZ;
	BuildRoadZone(xG, xF, xZ);
	CB_BuildingPlacement xB; xB.Reset();
	xB.PlaceService(CB_BUILDING_POWER_PLANT, 1350.0f, 1100.0f, 0.0f);
	xB.PlaceService(CB_BUILDING_WATER_TOWER, 1350.0f,  900.0f, 0.0f);
	xB.PlaceService(CB_BUILDING_POLICE,      1350.0f, 1000.0f, 0.0f);
	xB.PlaceService(CB_BUILDING_PARK,        1350.0f, 1000.0f, 0.0f);

	xB.SetRoadCapacity(100000.0f);   // ample road → uncongested
	for (int i = 0; i < 400; ++i) { xB.Tick(xZ); }
	const float fCongFree  = xB.GetCongestion();
	const float fHappyFree = xB.GetHappiness();

	xB.SetRoadCapacity(40.0f);        // choke the network → gridlock
	for (int i = 0; i < 40; ++i) { xB.Tick(xZ); }
	const float fCongJam  = xB.GetCongestion();
	const float fHappyJam = xB.GetHappiness();

	bool bOk = true;
	if (!(fCongFree < 0.05f))      { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Service_Congestion: ample roads still congested (%.2f)", fCongFree); bOk = false; }
	if (!(fCongJam > 0.5f))        { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Service_Congestion: choked network not congested (%.2f)", fCongJam); bOk = false; }
	if (!(fHappyJam < fHappyFree)) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Service_Congestion: congestion didn't lower happiness (%.2f -> %.2f)", fHappyFree, fHappyJam); bOk = false; }
	Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Service_Congestion: cong %.2f->%.2f, happy %.2f->%.2f", fCongFree, fCongJam, fHappyFree, fHappyJam);
	return bOk;
}

// Industry emits pollution; parks clean it. Pollution erodes happiness.
static bool Verify_CB_Service_Pollution()
{
	CB_RoadGraph xG; CB_TerrainHeightfield xF; CB_Zoning xZ;
	BuildRoadZone(xG, xF, xZ);   // mixed R/C/I (the industrial cluster pollutes)
	CB_BuildingPlacement xB; xB.Reset();
	xB.PlaceService(CB_BUILDING_POWER_PLANT, 1350.0f, 1100.0f, 0.0f);
	xB.PlaceService(CB_BUILDING_WATER_TOWER, 1350.0f,  900.0f, 0.0f);
	for (int i = 0; i < 400; ++i) { xB.Tick(xZ); }
	const float fPollDirty = xB.GetPollution();

	// Plant a belt of parks → pollution falls.
	for (int i = 0; i < 16; ++i) { xB.PlaceService(CB_BUILDING_PARK, 1050.0f + i * 40.0f, 1000.0f, 0.0f); }
	for (int i = 0; i < 60; ++i) { xB.Tick(xZ); }
	const float fPollClean = xB.GetPollution();

	bool bOk = true;
	if (!(fPollDirty > 0.10f))         { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Service_Pollution: industry didn't pollute (%.2f)", fPollDirty); bOk = false; }
	if (!(fPollClean < fPollDirty))    { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Service_Pollution: parks didn't reduce pollution (%.2f -> %.2f)", fPollDirty, fPollClean); bOk = false; }
	Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Service_Pollution: pollution %.2f -> %.2f (parks)", fPollDirty, fPollClean);
	return bOk;
}

// Loans credit the treasury against interest that amortizes; a higher tax rate
// out-earns a lower one.
static bool Verify_CB_Service_Budget()
{
	CB_RoadGraph xG; CB_TerrainHeightfield xF; CB_Zoning xZ;
	BuildRoadZone(xG, xF, xZ);
	CB_BuildingPlacement xB; xB.Reset();
	xB.PlaceService(CB_BUILDING_POWER_PLANT, 1350.0f, 1100.0f, 0.0f);
	xB.PlaceService(CB_BUILDING_WATER_TOWER, 1350.0f,  900.0f, 0.0f);
	const float fBeforeLoan = xB.GetTreasury();
	xB.TakeLoan(40000.0f);
	bool bOk = true;
	if (!(xB.GetTreasury() > fBeforeLoan + 39000.0f)) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Service_Budget: loan not credited"); bOk = false; }
	if (!(xB.GetDebt() > 40000.0f))                   { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Service_Budget: debt missing interest (%.0f)", xB.GetDebt()); bOk = false; }
	const float fDebt0 = xB.GetDebt();
	for (int i = 0; i < 300; ++i) { xB.Tick(xZ); }
	if (!(xB.GetDebt() < fDebt0))                     { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Service_Budget: debt didn't amortize (%.0f -> %.0f)", fDebt0, xB.GetDebt()); bOk = false; }

	// Two identical cities, different tax rate → the higher-taxed one earns more.
	CB_RoadGraph xGh; CB_TerrainHeightfield xFh; CB_Zoning xZh; CB_BuildingPlacement xHigh;
	BuildRoadZone(xGh, xFh, xZh); xHigh.Reset();
	xHigh.PlaceService(CB_BUILDING_POWER_PLANT, 1350.0f, 1100.0f, 0.0f); xHigh.PlaceService(CB_BUILDING_WATER_TOWER, 1350.0f, 900.0f, 0.0f);
	xHigh.SetTaxRate(1.4f);
	for (int i = 0; i < 400; ++i) { xHigh.Tick(xZh); }

	CB_RoadGraph xGl; CB_TerrainHeightfield xFl; CB_Zoning xZl; CB_BuildingPlacement xLow;
	BuildRoadZone(xGl, xFl, xZl); xLow.Reset();
	xLow.PlaceService(CB_BUILDING_POWER_PLANT, 1350.0f, 1100.0f, 0.0f); xLow.PlaceService(CB_BUILDING_WATER_TOWER, 1350.0f, 900.0f, 0.0f);
	xLow.SetTaxRate(0.6f);
	for (int i = 0; i < 400; ++i) { xLow.Tick(xZl); }

	if (!(xHigh.GetTreasury() > xLow.GetTreasury())) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Service_Budget: high tax ($%.0f) didn't out-earn low tax ($%.0f)", xHigh.GetTreasury(), xLow.GetTreasury()); bOk = false; }
	Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Service_Budget: debt %.0f->%.0f; highTax $%.0f vs lowTax $%.0f", fDebt0, xB.GetDebt(), xHigh.GetTreasury(), xLow.GetTreasury());
	return bOk;
}

// Utility-network reach: sources ON the district connect the buildings (high reach +
// happiness); sources placed far away leave them off-network (low reach), even though
// the city-wide power/water balance is satisfied in both.
static bool Verify_CB_Service_UtilityReach()
{
	CB_RoadGraph xGn; CB_TerrainHeightfield xFn; CB_Zoning xZn; CB_BuildingPlacement xNear;
	BuildRoadZone(xGn, xFn, xZn); xNear.Reset();
	xNear.PlaceService(CB_BUILDING_POWER_PLANT, 1350.0f, 1000.0f, 0.0f);   // on the street (road x 1000..1700)
	xNear.PlaceService(CB_BUILDING_WATER_TOWER, 1350.0f, 1000.0f, 0.0f);
	xNear.PlaceService(CB_BUILDING_POLICE,      1350.0f, 1000.0f, 0.0f);
	for (int i = 0; i < 400; ++i) { xNear.Tick(xZn); }

	CB_RoadGraph xGf; CB_TerrainHeightfield xFf; CB_Zoning xZf; CB_BuildingPlacement xFar;
	BuildRoadZone(xGf, xFf, xZf); xFar.Reset();
	xFar.PlaceService(CB_BUILDING_POWER_PLANT, 3900.0f, 3900.0f, 0.0f);    // far corner — supplies capacity but no reach
	xFar.PlaceService(CB_BUILDING_WATER_TOWER, 3900.0f, 3900.0f, 0.0f);
	xFar.PlaceService(CB_BUILDING_POLICE,      1350.0f, 1000.0f, 0.0f);
	for (int i = 0; i < 400; ++i) { xFar.Tick(xZf); }

	bool bOk = true;
	if (!(xNear.GetUtilityReach() > 0.5f))            { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Service_UtilityReach: on-site sources not connected (%.2f)", xNear.GetUtilityReach()); bOk = false; }
	if (!(xFar.GetUtilityReach() < 0.2f))             { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Service_UtilityReach: far sources still reached (%.2f)", xFar.GetUtilityReach()); bOk = false; }
	if (!(xNear.GetHappiness() > xFar.GetHappiness())) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Service_UtilityReach: connected city not happier (%.2f vs %.2f)", xNear.GetHappiness(), xFar.GetHappiness()); bOk = false; }
	Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Service_UtilityReach: near reach %.2f happy %.2f | far reach %.2f happy %.2f", xNear.GetUtilityReach(), xNear.GetHappiness(), xFar.GetUtilityReach(), xFar.GetHappiness());
	return bOk;
}

// People generate garbage + sewage; once past the free baseline an overwhelmed city
// piles up (happiness falls). Landfills collect, sewage plants treat → both fall, happiness rises.
static bool Verify_CB_Service_GarbageSewage()
{
	CB_RoadGraph xG; CB_TerrainHeightfield xF; CB_Zoning xZ;
	BuildRoadZone(xG, xF, xZ);
	CB_BuildingPlacement xB; xB.Reset();
	xB.PlaceService(CB_BUILDING_POWER_PLANT, 1350.0f, 1100.0f, 0.0f);
	xB.PlaceService(CB_BUILDING_WATER_TOWER, 1350.0f,  900.0f, 0.0f);
	for (int i = 0; i < 800; ++i) { xB.Tick(xZ); }
	const float fGarbDirty  = xB.GetGarbage();
	const float fSewDirty   = xB.GetSewage();
	const float fHappyDirty = xB.GetHappiness();

	// Build collection + treatment capacity (2 landfills + 2 sewage plants).
	xB.PlaceService(CB_BUILDING_LANDFILL,      1200.0f, 1120.0f, 0.0f);
	xB.PlaceService(CB_BUILDING_LANDFILL,      1500.0f, 1120.0f, 0.0f);
	xB.PlaceService(CB_BUILDING_SEWAGE_PLANT,  1200.0f,  880.0f, 0.0f);
	xB.PlaceService(CB_BUILDING_SEWAGE_PLANT,  1500.0f,  880.0f, 0.0f);
	for (int i = 0; i < 80; ++i) { xB.Tick(xZ); }
	const float fGarbClean  = xB.GetGarbage();
	const float fSewClean   = xB.GetSewage();

	// (We don't assert net happiness here: the facilities themselves draw power +
	// emit some pollution, which confounds a clean before/after — the four mechanism
	// checks below are the real proof. The happiness factors are in UpdateSimState.)
	bool bOk = true;
	if (!(fGarbDirty > 0.05f))         { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Service_GarbageSewage: city didn't accumulate garbage (%.2f)", fGarbDirty); bOk = false; }
	if (!(fSewDirty  > 0.05f))         { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Service_GarbageSewage: city didn't accumulate sewage (%.2f)", fSewDirty); bOk = false; }
	if (!(fGarbClean < fGarbDirty))    { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Service_GarbageSewage: landfills didn't cut garbage (%.2f -> %.2f)", fGarbDirty, fGarbClean); bOk = false; }
	if (!(fSewClean  < fSewDirty))     { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Service_GarbageSewage: sewage plants didn't treat sewage (%.2f -> %.2f)", fSewDirty, fSewClean); bOk = false; }
	Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Service_GarbageSewage: garbage %.2f->%.2f, sewage %.2f->%.2f (dirty happy %.2f)", fGarbDirty, fGarbClean, fSewDirty, fSewClean, fHappyDirty);
	return bOk;
}

// Public transport: bus depots carry commuters, taking their cars off the road →
// congestion eases + happiness rises. With the city sized to the road, this is a clear win.
static bool Verify_CB_Service_Transit()
{
	CB_RoadGraph xG; CB_TerrainHeightfield xF; CB_Zoning xZ;
	BuildRoadZone(xG, xF, xZ);
	CB_BuildingPlacement xB; xB.Reset();
	// Ample power up front (2 plants) so adding bus depots later can't brown the city
	// out — that would confound the happiness check; here the only delta is the transit.
	xB.PlaceService(CB_BUILDING_POWER_PLANT, 1350.0f, 1100.0f, 0.0f);
	xB.PlaceService(CB_BUILDING_POWER_PLANT, 1250.0f, 1100.0f, 0.0f);
	xB.PlaceService(CB_BUILDING_WATER_TOWER, 1350.0f,  900.0f, 0.0f);
	for (int i = 0; i < 400; ++i) { xB.Tick(xZ); }

	// Size the road network so the city runs at ~85% utilisation (mid-range congestion,
	// not saturated) — that's where taking cars off the road visibly helps.
	const float fPpl = static_cast<float>(xB.GetResidents() + xB.GetJobs());
	xB.SetRoadCapacity(fPpl / 0.85f / 2.0f);   // SetRoadCapacity doubles internally
	for (int i = 0; i < 40; ++i) { xB.Tick(xZ); }
	const float fCongNoTransit = xB.GetCongestion();
	const float fShareBefore   = xB.GetTransitShare();
	const float fHappyBefore   = xB.GetHappiness();

	xB.PlaceService(CB_BUILDING_BUS_DEPOT, 1300.0f, 1000.0f, 0.0f);
	xB.PlaceService(CB_BUILDING_BUS_DEPOT, 1400.0f, 1000.0f, 0.0f);
	for (int i = 0; i < 40; ++i) { xB.Tick(xZ); }
	const float fCongTransit = xB.GetCongestion();
	const float fShareAfter  = xB.GetTransitShare();
	const float fHappyAfter  = xB.GetHappiness();

	bool bOk = true;
	if (!(fShareBefore < 0.01f))          { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Service_Transit: riders without any depot (%.2f)", fShareBefore); bOk = false; }
	if (!(fShareAfter  > 0.10f))          { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Service_Transit: depots carried no commuters (%.2f)", fShareAfter); bOk = false; }
	if (!(fCongTransit < fCongNoTransit)) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Service_Transit: transit didn't ease congestion (%.2f -> %.2f)", fCongNoTransit, fCongTransit); bOk = false; }
	if (!(fHappyAfter  > fHappyBefore))   { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Service_Transit: transit didn't raise happiness (%.2f -> %.2f)", fHappyBefore, fHappyAfter); bOk = false; }
	Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Service_Transit: share %.2f->%.2f, cong %.2f->%.2f, happy %.2f->%.2f", fShareBefore, fShareAfter, fCongNoTransit, fCongTransit, fHappyBefore, fHappyAfter);
	return bOk;
}

// City-wide policies (recycling / pollution control / parks mandate / free transit) all
// reach every building (coverage 1.0) and each moves its sim metric the right way.
static bool Verify_CB_Policies_CityWide()
{
	CB_RoadGraph xG; CB_TerrainHeightfield xF; CB_Zoning xZ; CB_Districts xDist;
	CB_BuildingPlacement xB; xB.Reset(); xB.SetDistricts(&xDist);
	xB.PlaceService(CB_BUILDING_POWER_PLANT, 1350.0f, 1100.0f, 0.0f);
	xB.PlaceService(CB_BUILDING_WATER_TOWER, 1350.0f,  900.0f, 0.0f);
	xB.PlaceService(CB_BUILDING_BUS_DEPOT,   1300.0f, 1000.0f, 0.0f);   // one depot so free-transit has something to lift
	BuildRoadZone(xG, xF, xZ);
	for (int i = 0; i < 800; ++i) { xB.Tick(xZ); }
	const float fGarbBefore  = xB.GetGarbage();
	const float fPollBefore  = xB.GetPollution();
	const float fShareBefore = xB.GetTransitShare();
	const float fHappyBefore = xB.GetHappiness();

	xDist.SetCityPolicy(CB_POLICY_RECYCLING,         true);
	xDist.SetCityPolicy(CB_POLICY_POLLUTION_CONTROL, true);
	xDist.SetCityPolicy(CB_POLICY_PARKS_MANDATE,     true);
	xDist.SetCityPolicy(CB_POLICY_FREE_TRANSIT,      true);
	for (int i = 0; i < 40; ++i) { xB.Tick(xZ); }
	const float fGarbAfter  = xB.GetGarbage();
	const float fPollAfter  = xB.GetPollution();
	const float fShareAfter = xB.GetTransitShare();
	const float fHappyAfter = xB.GetHappiness();

	bool bOk = true;
	if (!(xB.GetPolicyCoverage(CB_POLICY_RECYCLING) > 0.95f)) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Policies_CityWide: city-wide policy not full coverage (%.2f)", xB.GetPolicyCoverage(CB_POLICY_RECYCLING)); bOk = false; }
	if (!(fGarbAfter  < fGarbBefore  - 0.02f)) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Policies_CityWide: recycling didn't cut garbage (%.2f -> %.2f)", fGarbBefore, fGarbAfter); bOk = false; }
	if (!(fPollAfter  < fPollBefore  - 0.01f)) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Policies_CityWide: pollution control didn't cut pollution (%.2f -> %.2f)", fPollBefore, fPollAfter); bOk = false; }
	if (!(fShareAfter > fShareBefore + 0.02f)) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Policies_CityWide: free transit didn't lift ridership (%.2f -> %.2f)", fShareBefore, fShareAfter); bOk = false; }
	if (!(fHappyAfter > fHappyBefore))         { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Policies_CityWide: policies didn't raise happiness (%.2f -> %.2f)", fHappyBefore, fHappyAfter); bOk = false; }
	Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Policies_CityWide: garbage %.2f->%.2f poll %.2f->%.2f transit %.2f->%.2f happy %.2f->%.2f", fGarbBefore, fGarbAfter, fPollBefore, fPollAfter, fShareBefore, fShareAfter, fHappyBefore, fHappyAfter);
	return bOk;
}

// A district-scoped policy only covers the buildings in its area, so it cuts garbage by
// less than the same policy enacted city-wide (per-district scoping is real, not global).
static bool Verify_CB_Policies_District()
{
	// City A — recycling inside a district covering part of the street.
	CB_RoadGraph xGa; CB_TerrainHeightfield xFa; CB_Zoning xZa; CB_Districts xDa;
	CB_BuildingPlacement xA; xA.Reset(); xA.SetDistricts(&xDa);
	BuildRoadZone(xGa, xFa, xZa);
	xA.PlaceService(CB_BUILDING_POWER_PLANT, 1350.0f, 1100.0f, 0.0f);
	xA.PlaceService(CB_BUILDING_WATER_TOWER, 1350.0f,  900.0f, 0.0f);
	for (int i = 0; i < 800; ++i) { xA.Tick(xZa); }
	const uint32_t uD = xDa.PaintDistrict(1150.0f, 1000.0f);   // covers the left part of the road
	xDa.SetDistrictPolicy(uD, CB_POLICY_RECYCLING, true);
	for (int i = 0; i < 40; ++i) { xA.Tick(xZa); }
	const float fCovDistrict  = xA.GetPolicyCoverage(CB_POLICY_RECYCLING);
	const float fGarbDistrict = xA.GetGarbage();

	// City B — the same recycling, city-wide.
	CB_RoadGraph xGb; CB_TerrainHeightfield xFb; CB_Zoning xZb; CB_Districts xDb;
	CB_BuildingPlacement xBcity; xBcity.Reset(); xBcity.SetDistricts(&xDb);
	BuildRoadZone(xGb, xFb, xZb);
	xBcity.PlaceService(CB_BUILDING_POWER_PLANT, 1350.0f, 1100.0f, 0.0f);
	xBcity.PlaceService(CB_BUILDING_WATER_TOWER, 1350.0f,  900.0f, 0.0f);
	for (int i = 0; i < 800; ++i) { xBcity.Tick(xZb); }
	xDb.SetCityPolicy(CB_POLICY_RECYCLING, true);
	for (int i = 0; i < 40; ++i) { xBcity.Tick(xZb); }
	const float fCovCity  = xBcity.GetPolicyCoverage(CB_POLICY_RECYCLING);
	const float fGarbCity = xBcity.GetGarbage();

	bool bOk = true;
	if (!(fCovDistrict > 0.02f && fCovDistrict < 0.95f)) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Policies_District: district coverage not partial (%.2f)", fCovDistrict); bOk = false; }
	if (!(fCovCity > 0.95f))                              { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Policies_District: city-wide coverage not full (%.2f)", fCovCity); bOk = false; }
	if (!(fGarbDistrict > fGarbCity + 0.05f))            { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Policies_District: district didn't reduce less than city-wide (district %.2f vs city %.2f)", fGarbDistrict, fGarbCity); bOk = false; }
	Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Policies_District: district cov %.2f garbage %.2f | city cov %.2f garbage %.2f", fCovDistrict, fGarbDistrict, fCovCity, fGarbCity);
	return bOk;
}

// Disasters: an uncovered building fire razes buildings; a fire station in range puts
// fires out (no losses). Uses the cumulative destroyed-by-fire counter (growth-proof).
static bool Verify_CB_Disaster_Fire()
{
	// --- uncovered: fires burn buildings down ---
	CB_RoadGraph xGa; CB_TerrainHeightfield xFa; CB_Zoning xZa; CB_BuildingPlacement xA;
	BuildRoadZone(xGa, xFa, xZa); xA.Reset();
	xA.PlaceService(CB_BUILDING_POWER_PLANT, 1350.0f, 1100.0f, 0.0f);
	xA.PlaceService(CB_BUILDING_WATER_TOWER, 1350.0f,  900.0f, 0.0f);
	for (int i = 0; i < 400; ++i) { xA.Tick(xZa); }
	int iIgnitedA = 0;
	for (int k = 0; k < 6; ++k) { if (xA.TriggerFireAt(1260.0f + k * 45.0f, 1000.0f)) { ++iIgnitedA; } }
	for (int i = 0; i < 300; ++i) { xA.Tick(xZa); }   // no fire cover → they burn down
	const uint32_t uDestroyedA = xA.GetFiresDestroyed();

	// --- covered: a fire station saves them ---
	CB_RoadGraph xGb; CB_TerrainHeightfield xFb; CB_Zoning xZb; CB_BuildingPlacement xB;
	BuildRoadZone(xGb, xFb, xZb); xB.Reset();
	xB.PlaceService(CB_BUILDING_POWER_PLANT, 1350.0f, 1100.0f, 0.0f);
	xB.PlaceService(CB_BUILDING_WATER_TOWER, 1350.0f,  900.0f, 0.0f);
	xB.PlaceService(CB_BUILDING_FIRE,        1375.0f, 1000.0f, 0.0f);   // covers ~1215..1535
	for (int i = 0; i < 400; ++i) { xB.Tick(xZb); }
	int iIgnitedB = 0;
	for (int k = 0; k < 6; ++k) { if (xB.TriggerFireAt(1260.0f + k * 45.0f, 1000.0f)) { ++iIgnitedB; } }
	for (int i = 0; i < 140; ++i) { xB.Tick(xZb); }   // fire crews extinguish them
	const uint32_t uDestroyedB = xB.GetFiresDestroyed();
	const uint32_t uActiveB    = xB.GetActiveFires();

	bool bOk = true;
	if (iIgnitedA < 3 || iIgnitedB < 3) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Disaster_Fire: couldn't ignite (A=%d B=%d)", iIgnitedA, iIgnitedB); bOk = false; }
	if (!(uDestroyedA > 0))             { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Disaster_Fire: uncovered fires razed nothing"); bOk = false; }
	if (uDestroyedB != 0)               { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Disaster_Fire: fire station failed (%u razed)", uDestroyedB); bOk = false; }
	if (uActiveB != 0)                  { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Disaster_Fire: covered fires not extinguished (%u still burning)", uActiveB); bOk = false; }
	Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Disaster_Fire: uncovered razed %u (of %d lit) | covered razed %u, active %u", uDestroyedA, iIgnitedA, uDestroyedB, uActiveB);
	return bOk;
}

// Freight: industry supplies the goods commerce sells. A city with no industry leaves
// commerce undersupplied (low freight ratio); adding industry lifts it.
static bool Verify_CB_Freight()
{
	using V2b = Zenith_Maths::Vector2;
	// City A — residential + commercial, NO industry.
	CB_RoadGraph xGa; CB_TerrainHeightfield xFa; CB_Zoning xZa; CB_BuildingPlacement xA;
	xFa.Init(257, 257, 16.0f, 0.0f, 0.0f, 200.0f);
	{ const uint32_t a = xGa.AddNode(V2b(1000,1000)), b = xGa.AddNode(V2b(1700,1000));
	  xGa.AddSegment(a, b, CB_Spline::Straight(V2b(1000,1000), V2b(1700,1000)), CB_ROADCLASS_MEDIUM); }
	xZa.SyncToGraph(xGa, xFa);
	xZa.PaintZone(1350.0f, 1000.0f, 600.0f, CB_ZONE_RESIDENTIAL, 2);
	xZa.PaintZone(1500.0f, 1000.0f, 160.0f, CB_ZONE_COMMERCIAL,  2);
	xA.Reset();
	xA.PlaceService(CB_BUILDING_POWER_PLANT, 1350.0f, 1100.0f, 0.0f);
	xA.PlaceService(CB_BUILDING_WATER_TOWER, 1350.0f,  900.0f, 0.0f);
	for (int i = 0; i < 800; ++i) { xA.Tick(xZa); }
	const float fFreightNoInd = xA.GetFreightRatio();

	// City B — the same, PLUS an industrial cluster.
	CB_RoadGraph xGb; CB_TerrainHeightfield xFb; CB_Zoning xZb; CB_BuildingPlacement xB;
	BuildRoadZone(xGb, xFb, xZb);   // mixed R/C/I (has industry)
	xB.Reset();
	xB.PlaceService(CB_BUILDING_POWER_PLANT, 1350.0f, 1100.0f, 0.0f);
	xB.PlaceService(CB_BUILDING_WATER_TOWER, 1350.0f,  900.0f, 0.0f);
	for (int i = 0; i < 800; ++i) { xB.Tick(xZb); }
	const float fFreightInd = xB.GetFreightRatio();

	bool bOk = true;
	if (!(fFreightNoInd < 0.35f))            { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Freight: no-industry city oversupplied (%.2f)", fFreightNoInd); bOk = false; }
	if (!(fFreightInd > 0.6f))               { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Freight: industry didn't supply commerce (%.2f)", fFreightInd); bOk = false; }
	if (!(fFreightInd > fFreightNoInd))      { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Freight: industry didn't raise freight (%.2f vs %.2f)", fFreightInd, fFreightNoInd); bOk = false; }
	Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Freight: no-industry %.2f vs with-industry %.2f", fFreightNoInd, fFreightInd);
	return bOk;
}

// Mail: buildings generate post; post offices collect it (like garbage).
static bool Verify_CB_Mail()
{
	CB_RoadGraph xG; CB_TerrainHeightfield xF; CB_Zoning xZ;
	BuildRoadZone(xG, xF, xZ);
	CB_BuildingPlacement xB; xB.Reset();
	xB.PlaceService(CB_BUILDING_POWER_PLANT, 1350.0f, 1100.0f, 0.0f);
	xB.PlaceService(CB_BUILDING_WATER_TOWER, 1350.0f,  900.0f, 0.0f);
	for (int i = 0; i < 800; ++i) { xB.Tick(xZ); }
	const float fMailDirty = xB.GetMail();

	xB.PlaceService(CB_BUILDING_POST_OFFICE, 1250.0f, 1100.0f, 0.0f);
	xB.PlaceService(CB_BUILDING_POST_OFFICE, 1450.0f, 1100.0f, 0.0f);
	for (int i = 0; i < 80; ++i) { xB.Tick(xZ); }
	const float fMailClean = xB.GetMail();

	bool bOk = true;
	if (!(fMailDirty > 0.05f))         { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Mail: city didn't accumulate mail (%.2f)", fMailDirty); bOk = false; }
	if (!(fMailClean < fMailDirty))    { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Mail: post offices didn't collect mail (%.2f -> %.2f)", fMailDirty, fMailClean); bOk = false; }
	Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Mail: mail %.2f -> %.2f (post offices)", fMailDirty, fMailClean);
	return bOk;
}

// Transit lines: with lines present, only people near a STOP ride. A line routed along
// the populated street carries riders; the same depots with stops off in an empty corner
// carry almost nobody — so routing (placement) matters, not just depot capacity.
static bool Verify_CB_TransitLines()
{
	// City A — a line whose stops sit on the populated street.
	CB_RoadGraph xGa; CB_TerrainHeightfield xFa; CB_Zoning xZa; CB_TransitLines xLa;
	BuildRoadZone(xGa, xFa, xZa);
	CB_BuildingPlacement xA; xA.Reset(); xA.SetTransitLines(&xLa);
	xA.PlaceService(CB_BUILDING_POWER_PLANT, 1350.0f, 1100.0f, 0.0f);
	xA.PlaceService(CB_BUILDING_WATER_TOWER, 1350.0f,  900.0f, 0.0f);
	xA.PlaceService(CB_BUILDING_BUS_DEPOT,   1350.0f, 1100.0f, 0.0f);
	xLa.StartLine();
	xLa.AddStop(1100.0f, 1000.0f); xLa.AddStop(1350.0f, 1000.0f); xLa.AddStop(1600.0f, 1000.0f);
	for (int i = 0; i < 500; ++i) { xA.Tick(xZa); }
	const float fShareOnRoute = xA.GetTransitShare();

	// City B — identical depots, but the line's stops are off in an empty corner.
	CB_RoadGraph xGb; CB_TerrainHeightfield xFb; CB_Zoning xZb; CB_TransitLines xLb;
	BuildRoadZone(xGb, xFb, xZb);
	CB_BuildingPlacement xB; xB.Reset(); xB.SetTransitLines(&xLb);
	xB.PlaceService(CB_BUILDING_POWER_PLANT, 1350.0f, 1100.0f, 0.0f);
	xB.PlaceService(CB_BUILDING_WATER_TOWER, 1350.0f,  900.0f, 0.0f);
	xB.PlaceService(CB_BUILDING_BUS_DEPOT,   1350.0f, 1100.0f, 0.0f);
	xLb.StartLine();
	xLb.AddStop(3500.0f, 3500.0f); xLb.AddStop(3600.0f, 3600.0f);
	for (int i = 0; i < 500; ++i) { xB.Tick(xZb); }
	const float fShareOffRoute = xB.GetTransitShare();

	bool bOk = true;
	if (!(fShareOnRoute > 0.2f))            { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_TransitLines: on-route line carried few riders (%.2f)", fShareOnRoute); bOk = false; }
	if (!(fShareOffRoute < 0.05f))          { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_TransitLines: off-route line still carried riders (%.2f)", fShareOffRoute); bOk = false; }
	if (!(fShareOnRoute > fShareOffRoute))  { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_TransitLines: routing didn't matter (%.2f vs %.2f)", fShareOnRoute, fShareOffRoute); bOk = false; }
	Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_TransitLines: on-route share %.2f vs off-route %.2f", fShareOnRoute, fShareOffRoute);
	return bOk;
}

// Utility conduits: sources placed far from the city leave it off-network (low reach);
// laying a conduit chain from the sources to the buildings carries power/water to them.
static bool Verify_CB_Conduits()
{
	// Sources in a far corner, no conduits → the city is off-network.
	CB_RoadGraph xG1; CB_TerrainHeightfield xF1; CB_Zoning xZ1; CB_BuildingPlacement xNoPipe;
	BuildRoadZone(xG1, xF1, xZ1); xNoPipe.Reset();
	xNoPipe.PlaceService(CB_BUILDING_POWER_PLANT, 2600.0f, 1000.0f, 0.0f);   // far east of the street (1000..1700)
	xNoPipe.PlaceService(CB_BUILDING_WATER_TOWER, 2600.0f, 1000.0f, 0.0f);
	for (int i = 0; i < 400; ++i) { xNoPipe.Tick(xZ1); }
	const float fReachNoPipe = xNoPipe.GetUtilityReach();

	// Same, but a conduit chain bridges the gap from the sources back across the street.
	CB_RoadGraph xG2; CB_TerrainHeightfield xF2; CB_Zoning xZ2; CB_Conduits xPipe; CB_BuildingPlacement xPiped;
	BuildRoadZone(xG2, xF2, xZ2); xPiped.Reset(); xPiped.SetConduits(&xPipe);
	xPiped.PlaceService(CB_BUILDING_POWER_PLANT, 2600.0f, 1000.0f, 0.0f);
	xPiped.PlaceService(CB_BUILDING_WATER_TOWER, 2600.0f, 1000.0f, 0.0f);
	for (float x = 2500.0f; x >= 1000.0f; x -= 110.0f) { xPipe.AddConduit(x, 1000.0f); }   // chain (spacing < LINK_DIST)
	for (int i = 0; i < 400; ++i) { xPiped.Tick(xZ2); }
	const float fReachPiped = xPiped.GetUtilityReach();

	bool bOk = true;
	if (!(fReachNoPipe < 0.30f))                 { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Conduits: far sources still reached without pipes (%.2f)", fReachNoPipe); bOk = false; }
	if (!(fReachPiped > fReachNoPipe + 0.30f))   { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Conduits: conduit chain didn't extend reach (%.2f -> %.2f)", fReachNoPipe, fReachPiped); bOk = false; }
	Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Conduits: utility reach %.2f (no pipes) -> %.2f (conduit chain)", fReachNoPipe, fReachPiped);
	return bOk;
}

static bool Step_Once(int iFrame) { return iFrame < 1; }

static const Zenith_AutomatedTest g_xServicePowerGating = { "CB_Service_PowerGating", nullptr, &Step_Once, &Verify_CB_Service_PowerGating, 30, false };
static const Zenith_AutomatedTest g_xServiceCoverage    = { "CB_Service_Coverage",    nullptr, &Step_Once, &Verify_CB_Service_Coverage,    30, false };
static const Zenith_AutomatedTest g_xServiceEconomy     = { "CB_Service_Economy",     nullptr, &Step_Once, &Verify_CB_Service_Economy,     30, false };
static const Zenith_AutomatedTest g_xServiceCongestion  = { "CB_Service_Congestion",  nullptr, &Step_Once, &Verify_CB_Service_Congestion,  30, false };
static const Zenith_AutomatedTest g_xServicePollution   = { "CB_Service_Pollution",   nullptr, &Step_Once, &Verify_CB_Service_Pollution,   30, false };
static const Zenith_AutomatedTest g_xServiceBudget      = { "CB_Service_Budget",      nullptr, &Step_Once, &Verify_CB_Service_Budget,      30, false };
static const Zenith_AutomatedTest g_xServiceUtilReach   = { "CB_Service_UtilityReach",nullptr, &Step_Once, &Verify_CB_Service_UtilityReach, 30, false };
static const Zenith_AutomatedTest g_xServiceGarbageSewage = { "CB_Service_GarbageSewage", nullptr, &Step_Once, &Verify_CB_Service_GarbageSewage, 30, false };
static const Zenith_AutomatedTest g_xServiceTransit     = { "CB_Service_Transit",     nullptr, &Step_Once, &Verify_CB_Service_Transit,     30, false };
static const Zenith_AutomatedTest g_xPoliciesCityWide   = { "CB_Policies_CityWide",   nullptr, &Step_Once, &Verify_CB_Policies_CityWide,   30, false };
static const Zenith_AutomatedTest g_xPoliciesDistrict   = { "CB_Policies_District",   nullptr, &Step_Once, &Verify_CB_Policies_District,   30, false };
static const Zenith_AutomatedTest g_xDisasterFire       = { "CB_Disaster_Fire",       nullptr, &Step_Once, &Verify_CB_Disaster_Fire,       30, false };
static const Zenith_AutomatedTest g_xFreight            = { "CB_Freight",             nullptr, &Step_Once, &Verify_CB_Freight,             30, false };
static const Zenith_AutomatedTest g_xMail               = { "CB_Mail",                nullptr, &Step_Once, &Verify_CB_Mail,                30, false };
static const Zenith_AutomatedTest g_xTransitLines       = { "CB_TransitLines",        nullptr, &Step_Once, &Verify_CB_TransitLines,        30, false };
static const Zenith_AutomatedTest g_xConduits           = { "CB_Conduits",            nullptr, &Step_Once, &Verify_CB_Conduits,            30, false };

ZENITH_AUTOMATED_TEST_REGISTER(g_xServicePowerGating);
ZENITH_AUTOMATED_TEST_REGISTER(g_xServiceCoverage);
ZENITH_AUTOMATED_TEST_REGISTER(g_xServiceEconomy);
ZENITH_AUTOMATED_TEST_REGISTER(g_xServiceCongestion);
ZENITH_AUTOMATED_TEST_REGISTER(g_xServicePollution);
ZENITH_AUTOMATED_TEST_REGISTER(g_xServiceBudget);
ZENITH_AUTOMATED_TEST_REGISTER(g_xServiceUtilReach);
ZENITH_AUTOMATED_TEST_REGISTER(g_xServiceGarbageSewage);
ZENITH_AUTOMATED_TEST_REGISTER(g_xServiceTransit);
ZENITH_AUTOMATED_TEST_REGISTER(g_xPoliciesCityWide);
ZENITH_AUTOMATED_TEST_REGISTER(g_xPoliciesDistrict);
ZENITH_AUTOMATED_TEST_REGISTER(g_xDisasterFire);
ZENITH_AUTOMATED_TEST_REGISTER(g_xFreight);
ZENITH_AUTOMATED_TEST_REGISTER(g_xMail);
ZENITH_AUTOMATED_TEST_REGISTER(g_xTransitLines);
ZENITH_AUTOMATED_TEST_REGISTER(g_xConduits);

#endif // ZENITH_INPUT_SIMULATOR
