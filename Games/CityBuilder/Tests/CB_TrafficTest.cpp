#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "CityBuilder/Source/CB_Traffic.h"
#include "CityBuilder/Source/CB_RoadGraph.h"
#include "CityBuilder/Source/CB_TerrainHeightfield.h"
#include "Maths/Zenith_Maths.h"
#include <cmath>

// ============================================================================
// CB_TrafficTest — G6c gate (headless): A* over the road graph returns the
// shortest segment path (and reports unreachable nodes), and the vehicle pool
// spawns on the network, advances smoothly (no teleporting), and despawns when
// the network is bulldozed away. Pure logic.
// ============================================================================

namespace { using V2 = Zenith_Maths::Vector2; }

static bool Verify_CB_Traffic_AStar()
{
	CB_RoadGraph xG;
	const uint32_t uA = xG.AddNode(V2(0.0f,   0.0f));
	const uint32_t uB = xG.AddNode(V2(100.0f, 0.0f));
	const uint32_t uC = xG.AddNode(V2(200.0f, 0.0f));
	const uint32_t uD = xG.AddNode(V2(100.0f, 200.0f));   // detour vertex
	xG.AddSegment(uA, uB, CB_Spline::Straight(V2(0.0f, 0.0f),     V2(100.0f, 0.0f)),   CB_ROADCLASS_SMALL);
	xG.AddSegment(uB, uC, CB_Spline::Straight(V2(100.0f, 0.0f),   V2(200.0f, 0.0f)),   CB_ROADCLASS_SMALL);
	xG.AddSegment(uA, uD, CB_Spline::Straight(V2(0.0f, 0.0f),     V2(100.0f, 200.0f)), CB_ROADCLASS_SMALL);  // long way
	xG.AddSegment(uD, uC, CB_Spline::Straight(V2(100.0f, 200.0f), V2(200.0f, 0.0f)),   CB_ROADCLASS_SMALL);  // long way
	const uint32_t uE = xG.AddNode(V2(9000.0f, 9000.0f)); // isolated, unreachable

	bool bOk = true;
	Zenith_Vector<uint32_t> auPath;

	// Shortest A->C is straight via B (200m), not the detour via D (~447m).
	if (!CB_Traffic::FindPath(xG, uA, uC, auPath)) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Traffic_AStar: no A->C path"); bOk = false; }
	else
	{
		float fLen = 0.0f;
		for (uint32_t i = 0; i < auPath.GetSize(); ++i) { fLen += xG.GetSegment(auPath.Get(i)).m_xSpline.Length(); }
		if (auPath.GetSize() != 2) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Traffic_AStar: expected 2 segments, got %u", auPath.GetSize()); bOk = false; }
		if (fLen > 260.0f)         { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Traffic_AStar: not shortest (%.0fm)", fLen); bOk = false; }
	}

	if (!CB_Traffic::FindPath(xG, uA, uB, auPath) || auPath.GetSize() != 1) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Traffic_AStar: A->B not a single segment"); bOk = false; }
	if (CB_Traffic::FindPath(xG, uA, uE, auPath))                           { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Traffic_AStar: isolated node reported reachable"); bOk = false; }

	return bOk;
}

static bool Verify_CB_Traffic_Drive()
{
	CB_RoadGraph xG; CB_TerrainHeightfield xF;
	xF.Init(257, 257, 16.0f, 0.0f, 0.0f, 200.0f);
	const uint32_t uA = xG.AddNode(V2(1000.0f, 1000.0f));
	const uint32_t uB = xG.AddNode(V2(1400.0f, 1000.0f));
	const uint32_t uC = xG.AddNode(V2(1400.0f, 1400.0f));
	xG.AddSegment(uA, uB, CB_Spline::Straight(V2(1000.0f, 1000.0f), V2(1400.0f, 1000.0f)), CB_ROADCLASS_MEDIUM);
	xG.AddSegment(uB, uC, CB_Spline::Straight(V2(1400.0f, 1000.0f), V2(1400.0f, 1400.0f)), CB_ROADCLASS_MEDIUM);

	CB_Traffic xT;
	xT.Reset();
	for (int i = 0; i < 12; ++i) { xT.Update(xG, xF, 1.0f / 60.0f); }   // spawn + drive

	bool bOk = true;
	if (xT.GetActiveVehicleCount() == 0) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Traffic_Drive: no vehicles spawned"); return false; }

	// One step: every still-active vehicle moves a bounded amount (no teleport),
	// and the fleet as a whole moves (it isn't frozen).
	Zenith_Vector<Zenith_Maths::Vector3> axBefore;
	for (uint32_t i = 0; i < xT.GetVehicleSlotCount(); ++i) { axBefore.PushBack(xT.GetVehicle(i).m_xPos); }
	xT.Update(xG, xF, 1.0f / 60.0f);
	float fMax = 0.0f, fSum = 0.0f;
	for (uint32_t i = 0; i < xT.GetVehicleSlotCount() && i < axBefore.GetSize(); ++i)
	{
		if (!xT.GetVehicle(i).m_bActive) { continue; }
		const Zenith_Maths::Vector3 xP = xT.GetVehicle(i).m_xPos;
		const float fDx = xP.x - axBefore.Get(i).x;
		const float fDz = xP.z - axBefore.Get(i).z;
		const float fMove = sqrtf(fDx * fDx + fDz * fDz);
		if (fMove > fMax) { fMax = fMove; }
		fSum += fMove;
	}
	if (fMax > 2.0f)  { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Traffic_Drive: vehicle teleported (%.2fm/step)", fMax); bOk = false; }
	if (fSum < 0.5f)  { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Traffic_Drive: fleet is static (%.2fm)", fSum); bOk = false; }

	// Bulldoze the whole network → vehicles despawn.
	xG.RemoveSegment(0);
	xG.RemoveSegment(1);
	xT.Update(xG, xF, 1.0f / 60.0f);
	if (xT.GetActiveVehicleCount() != 0) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Traffic_Drive: %u vehicles survived an empty network", xT.GetActiveVehicleCount()); bOk = false; }

	Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Traffic_Drive: fleet moved %.1fm/step, max %.2fm", fSum, fMax);
	return bOk;
}

static bool Step_Once(int iFrame) { return iFrame < 1; }

static const Zenith_AutomatedTest g_xTrafficAStar = { "CB_Traffic_AStar", nullptr, &Step_Once, &Verify_CB_Traffic_AStar, 30, false };
static const Zenith_AutomatedTest g_xTrafficDrive = { "CB_Traffic_Drive", nullptr, &Step_Once, &Verify_CB_Traffic_Drive, 30, false };

ZENITH_AUTOMATED_TEST_REGISTER(g_xTrafficAStar);
ZENITH_AUTOMATED_TEST_REGISTER(g_xTrafficDrive);

#endif // ZENITH_INPUT_SIMULATOR
