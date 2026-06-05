#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "CityBuilder/Source/CB_Spline.h"
#include "CityBuilder/Source/CB_RoadGraph.h"
#include <cmath>

// ============================================================================
// CB_SplineRoads — G1 foundation gate (headless): the cubic-bezier spline math
// (CB_Spline) and the road graph (CB_RoadGraph nodes/segments/snap/junction/
// remove). Pure logic; built locally, no scene. (One file so the test .obj
// doesn't collide with Source/CB_RoadGraph.cpp's .obj.)
// ============================================================================

namespace
{
	using V2 = Zenith_Maths::Vector2;
	bool Near(float a, float b, float eps = 0.05f) { return std::fabs(a - b) <= eps; }
}

// ---- CB_Spline_Straight: a degenerate straight bezier behaves as a line ----
static bool Verify_CB_Spline_Straight()
{
	const CB_Spline xS = CB_Spline::Straight(V2(0.0f, 0.0f), V2(10.0f, 0.0f));
	bool bOk = true;
	const V2 p0 = xS.Evaluate(0.0f);
	const V2 pm = xS.Evaluate(0.5f);
	const V2 p1 = xS.Evaluate(1.0f);
	if (!Near(p0.x, 0.0f) || !Near(p0.y, 0.0f)) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Spline_Straight: P(0) wrong"); bOk = false; }
	if (!Near(pm.x, 5.0f) || !Near(pm.y, 0.0f)) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Spline_Straight: P(0.5) wrong (%f,%f)", pm.x, pm.y); bOk = false; }
	if (!Near(p1.x, 10.0f) || !Near(p1.y, 0.0f)) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Spline_Straight: P(1) wrong"); bOk = false; }
	if (!Near(xS.Length(), 10.0f, 0.1f)) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Spline_Straight: length %f != 10", xS.Length()); bOk = false; }
	const V2 t = xS.UnitTangent(0.5f);
	if (!Near(t.x, 1.0f) || !Near(t.y, 0.0f)) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Spline_Straight: tangent (%f,%f)", t.x, t.y); bOk = false; }
	return bOk;
}

// ---- CB_Spline_Curved: a curved bezier is longer than its chord ----
static bool Verify_CB_Spline_Curved()
{
	// Control net that bows out: chord is (0,0)->(10,10) ~= 14.14.
	const CB_Spline xS(V2(0.0f, 0.0f), V2(10.0f, 0.0f), V2(0.0f, 10.0f), V2(10.0f, 10.0f));
	bool bOk = true;
	const float fChord = std::sqrt(200.0f);
	if (!(xS.Length() > fChord)) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Spline_Curved: length %f not > chord %f", xS.Length(), fChord); bOk = false; }
	const V2 p0 = xS.Evaluate(0.0f);
	const V2 p1 = xS.Evaluate(1.0f);
	if (!Near(p0.x, 0.0f) || !Near(p0.y, 0.0f)) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Spline_Curved: start wrong"); bOk = false; }
	if (!Near(p1.x, 10.0f) || !Near(p1.y, 10.0f)) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Spline_Curved: end wrong"); bOk = false; }
	return bOk;
}

// ---- CB_Spline_Distance: DistanceToPoint matches geometry ----
static bool Verify_CB_Spline_Distance()
{
	const CB_Spline xS = CB_Spline::Straight(V2(0.0f, 0.0f), V2(100.0f, 0.0f));
	bool bOk = true;
	if (!Near(xS.DistanceToPoint(V2(50.0f, 5.0f)), 5.0f, 0.25f)) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Spline_Distance: offset %f != 5", xS.DistanceToPoint(V2(50.0f, 5.0f))); bOk = false; }
	if (!(xS.DistanceToPoint(V2(50.0f, 0.0f)) < 0.25f)) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Spline_Distance: on-line not ~0"); bOk = false; }
	if (!(xS.DistanceToPoint(V2(50.0f, 1000.0f)) > 900.0f)) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Spline_Distance: far not far"); bOk = false; }
	// Beyond the endpoint, distance is to the endpoint (clamped projection).
	if (!Near(xS.DistanceToPoint(V2(110.0f, 0.0f)), 10.0f, 0.25f)) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Spline_Distance: past-end %f != 10", xS.DistanceToPoint(V2(110.0f, 0.0f))); bOk = false; }
	return bOk;
}

// ---- CB_RoadGraph_AddSnap: FindOrAddNode snaps within radius, else creates ----
static bool Verify_CB_RoadGraph_AddSnap()
{
	CB_RoadGraph xG;
	const uint32_t uA = xG.AddNode(V2(0.0f, 0.0f));
	bool bOk = true;
	if (xG.FindOrAddNode(V2(3.0f, 0.0f), 5.0f) != uA) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_RoadGraph_AddSnap: near point did not snap"); bOk = false; }
	const uint32_t uB = xG.FindOrAddNode(V2(100.0f, 0.0f), 5.0f);
	if (uB == uA) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_RoadGraph_AddSnap: far point wrongly snapped"); bOk = false; }
	const uint32_t uSeg = xG.AddSegment(uA, uB, CB_Spline::Straight(V2(0.0f, 0.0f), V2(100.0f, 0.0f)), CB_ROADCLASS_MEDIUM);
	if (xG.GetActiveSegmentCount() != 1) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_RoadGraph_AddSnap: segment count != 1"); bOk = false; }
	if (!Near(xG.GetSegment(uSeg).m_fWidth, CB_RoadGraph::ClassWidth(CB_ROADCLASS_MEDIUM))) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_RoadGraph_AddSnap: width not by class"); bOk = false; }
	return bOk;
}

// ---- CB_RoadGraph_Junction: shared node = junction; nearest-segment query ----
static bool Verify_CB_RoadGraph_Junction()
{
	CB_RoadGraph xG;
	const uint32_t uA = xG.AddNode(V2(0.0f, 0.0f));
	const uint32_t uB = xG.AddNode(V2(100.0f, 0.0f));
	const uint32_t uC = xG.AddNode(V2(0.0f, 100.0f));
	xG.AddSegment(uA, uB, CB_Spline::Straight(V2(0.0f, 0.0f), V2(100.0f, 0.0f)), CB_ROADCLASS_SMALL);
	xG.AddSegment(uA, uC, CB_Spline::Straight(V2(0.0f, 0.0f), V2(0.0f, 100.0f)), CB_ROADCLASS_SMALL);
	bool bOk = true;
	if (xG.CountSegmentsAtNode(uA) != 2) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_RoadGraph_Junction: shared node count != 2"); bOk = false; }
	if (xG.CountSegmentsAtNode(uB) != 1) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_RoadGraph_Junction: endpoint count != 1"); bOk = false; }
	if (xG.FindNearestSegment(50.0f, 2.0f, 10.0f) == CB_RoadGraph::INVALID) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_RoadGraph_Junction: nearest-segment miss"); bOk = false; }
	if (xG.FindNearestSegment(50.0f, 500.0f, 10.0f) != CB_RoadGraph::INVALID) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_RoadGraph_Junction: nearest-segment false hit"); bOk = false; }
	if (!(xG.MinDistanceToAnyRoad(50.0f, 2.0f) < 3.0f)) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_RoadGraph_Junction: MinDistance wrong"); bOk = false; }
	return bOk;
}

// ---- CB_RoadGraph_Remove: bulldoze frees the segment + orphan nodes ----
static bool Verify_CB_RoadGraph_Remove()
{
	CB_RoadGraph xG;
	const uint32_t uA = xG.AddNode(V2(0.0f, 0.0f));
	const uint32_t uB = xG.AddNode(V2(100.0f, 0.0f));
	const uint32_t uSeg = xG.AddSegment(uA, uB, CB_Spline::Straight(V2(0.0f, 0.0f), V2(100.0f, 0.0f)), CB_ROADCLASS_SMALL);
	if (xG.GetActiveSegmentCount() != 1 || xG.GetActiveNodeCount() != 2) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_RoadGraph_Remove: precondition"); return false; }
	xG.RemoveSegment(uSeg);
	bool bOk = true;
	if (xG.GetActiveSegmentCount() != 0) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_RoadGraph_Remove: segment not removed"); bOk = false; }
	if (xG.GetActiveNodeCount() != 0) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_RoadGraph_Remove: orphan nodes not freed (%u)", xG.GetActiveNodeCount()); bOk = false; }
	xG.RemoveSegment(uSeg);  // double-remove is a safe no-op
	if (xG.GetActiveSegmentCount() != 0) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_RoadGraph_Remove: double-remove changed count"); bOk = false; }
	return bOk;
}

static bool Step_Once(int iFrame) { return iFrame < 1; }

static const Zenith_AutomatedTest g_xSplineStraight   = { "CB_Spline_Straight",    nullptr, &Step_Once, &Verify_CB_Spline_Straight,   30, false };
static const Zenith_AutomatedTest g_xSplineCurved     = { "CB_Spline_Curved",      nullptr, &Step_Once, &Verify_CB_Spline_Curved,     30, false };
static const Zenith_AutomatedTest g_xSplineDistance   = { "CB_Spline_Distance",    nullptr, &Step_Once, &Verify_CB_Spline_Distance,   30, false };
static const Zenith_AutomatedTest g_xRoadGraphAddSnap = { "CB_RoadGraph_AddSnap",  nullptr, &Step_Once, &Verify_CB_RoadGraph_AddSnap, 30, false };
static const Zenith_AutomatedTest g_xRoadGraphJunc    = { "CB_RoadGraph_Junction", nullptr, &Step_Once, &Verify_CB_RoadGraph_Junction,30, false };
static const Zenith_AutomatedTest g_xRoadGraphRemove  = { "CB_RoadGraph_Remove",   nullptr, &Step_Once, &Verify_CB_RoadGraph_Remove,  30, false };

ZENITH_AUTOMATED_TEST_REGISTER(g_xSplineStraight);
ZENITH_AUTOMATED_TEST_REGISTER(g_xSplineCurved);
ZENITH_AUTOMATED_TEST_REGISTER(g_xSplineDistance);
ZENITH_AUTOMATED_TEST_REGISTER(g_xRoadGraphAddSnap);
ZENITH_AUTOMATED_TEST_REGISTER(g_xRoadGraphJunc);
ZENITH_AUTOMATED_TEST_REGISTER(g_xRoadGraphRemove);

#endif // ZENITH_INPUT_SIMULATOR
