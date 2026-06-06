#include "Zenith.h"

#include "CityBuilder/Source/CB_Traffic.h"
#include "Flux/Primitives/Flux_PrimitivesImpl.h"

namespace
{
	constexpr float fBIG = 1.0e30f;

	Zenith_Maths::Vector3 CarColor(uint32_t uIdx)
	{
		switch (uIdx % 5u)
		{
		case 0:  return Zenith_Maths::Vector3(0.88f, 0.88f, 0.90f);  // white
		case 1:  return Zenith_Maths::Vector3(0.72f, 0.18f, 0.16f);  // red
		case 2:  return Zenith_Maths::Vector3(0.16f, 0.30f, 0.62f);  // blue
		case 3:  return Zenith_Maths::Vector3(0.18f, 0.18f, 0.20f);  // dark
		default: return Zenith_Maths::Vector3(0.55f, 0.57f, 0.60f);  // silver
		}
	}

	// A small car-sized oriented box at xCentre, long axis along xFwd (unit).
	void EmitCarBox(const Zenith_Maths::Vector3& xCentre, const Zenith_Maths::Vector2& xFwd, const Zenith_Maths::Vector3& xCol)
	{
		const Zenith_Maths::Vector2 xR(-xFwd.y, xFwd.x);
		const float fHL = 2.4f, fHW = 1.1f, fH = 1.5f;
		auto P = [&](float fs, float rs, float y) -> Zenith_Maths::Vector3
		{
			return Zenith_Maths::Vector3(
				xCentre.x + xFwd.x * fs * fHL + xR.x * rs * fHW,
				xCentre.y + y,
				xCentre.z + xFwd.y * fs * fHL + xR.y * rs * fHW);
		};
		const Zenith_Maths::Vector3 b00 = P(-1, -1, 0.0f), b10 = P(1, -1, 0.0f), b11 = P(1, 1, 0.0f), b01 = P(-1, 1, 0.0f);
		const Zenith_Maths::Vector3 t00 = P(-1, -1, fH), t10 = P(1, -1, fH), t11 = P(1, 1, fH), t01 = P(-1, 1, fH);
		Flux_PrimitivesImpl& xP = g_xEngine.Primitives();
		xP.AddTriangle(t00, t11, t10, xCol); xP.AddTriangle(t00, t01, t11, xCol);
		xP.AddTriangle(b00, b10, t10, xCol); xP.AddTriangle(b00, t10, t00, xCol);
		xP.AddTriangle(b10, b11, t11, xCol); xP.AddTriangle(b10, t11, t10, xCol);
		xP.AddTriangle(b11, b01, t01, xCol); xP.AddTriangle(b11, t01, t11, xCol);
		xP.AddTriangle(b01, b00, t00, xCol); xP.AddTriangle(b01, t00, t01, xCol);
	}
}

void CB_Traffic::Reset()
{
	m_axVehicles.Clear();
	m_auSegLoad.Clear();
	m_xPathScratch.Clear();
	m_xStats = CB_TrafficStats{};
	m_uRng   = 0x1234567u;
}

uint32_t CB_Traffic::NextRand()
{
	m_uRng = m_uRng * 1664525u + 1013904223u;
	return m_uRng;
}

bool CB_Traffic::FindPath(const CB_RoadGraph& xGraph, uint32_t uStart, uint32_t uGoal, Zenith_Vector<uint32_t>& outSegs)
{
	outSegs.Clear();
	const uint32_t uNodes = xGraph.GetNodeSlotCount();
	if (uStart >= uNodes || uGoal >= uNodes) { return false; }
	if (uStart == uGoal)                      { return true; }   // already there, empty path

	Zenith_Vector<float>    afG, afF;
	Zenith_Vector<uint32_t> auCameSeg, auCameNode;
	Zenith_Vector<bool>     abOpen, abClosed;
	for (uint32_t i = 0; i < uNodes; ++i)
	{
		afG.PushBack(fBIG); afF.PushBack(fBIG);
		auCameSeg.PushBack(CB_RoadGraph::INVALID); auCameNode.PushBack(CB_RoadGraph::INVALID);
		abOpen.PushBack(false); abClosed.PushBack(false);
	}

	const Zenith_Maths::Vector2 xGoalPos = xGraph.GetNode(uGoal).m_xPos;
	afG.Get(uStart) = 0.0f;
	afF.Get(uStart) = CB_Spline::Distance(xGraph.GetNode(uStart).m_xPos, xGoalPos);
	abOpen.Get(uStart) = true;

	const uint32_t uSegs = xGraph.GetSegmentSlotCount();
	bool bFound = false;
	while (true)
	{
		uint32_t uCur = CB_RoadGraph::INVALID;
		float fBest = fBIG;
		for (uint32_t i = 0; i < uNodes; ++i) { if (abOpen.Get(i) && afF.Get(i) < fBest) { fBest = afF.Get(i); uCur = i; } }
		if (uCur == CB_RoadGraph::INVALID) { break; }      // open set empty → unreachable
		if (uCur == uGoal)                 { bFound = true; break; }
		abOpen.Get(uCur) = false; abClosed.Get(uCur) = true;
		for (uint32_t s = 0; s < uSegs; ++s)
		{
			const CB_RoadSegment& xSeg = xGraph.GetSegment(s);
			if (!xSeg.m_bActive) { continue; }
			uint32_t uNb = CB_RoadGraph::INVALID;
			if      (xSeg.m_uNodeA == uCur) { uNb = xSeg.m_uNodeB; }
			else if (xSeg.m_uNodeB == uCur) { uNb = xSeg.m_uNodeA; }
			else { continue; }
			if (uNb >= uNodes || abClosed.Get(uNb)) { continue; }
			const float fTent = afG.Get(uCur) + xSeg.m_xSpline.Length();
			if (fTent < afG.Get(uNb))
			{
				afG.Get(uNb) = fTent;
				afF.Get(uNb) = fTent + CB_Spline::Distance(xGraph.GetNode(uNb).m_xPos, xGoalPos);
				auCameNode.Get(uNb) = uCur;
				auCameSeg.Get(uNb)  = s;
				abOpen.Get(uNb)     = true;
			}
		}
	}
	if (!bFound) { return false; }

	// Reconstruct goal→start, then emit start→goal.
	Zenith_Vector<uint32_t> auRev;
	uint32_t uN = uGoal;
	while (uN != uStart)
	{
		const uint32_t uSeg = auCameSeg.Get(uN);
		if (uSeg == CB_RoadGraph::INVALID) { return false; }
		auRev.PushBack(uSeg);
		uN = auCameNode.Get(uN);
		if (uN == CB_RoadGraph::INVALID) { return false; }
	}
	for (uint32_t i = auRev.GetSize(); i > 0; --i) { outSegs.PushBack(auRev.Get(i - 1)); }
	return true;
}

uint32_t CB_Traffic::SegCapacity(const CB_RoadSegment& xSeg)
{
	// Vehicles a segment carries freely before it congests — wider class = more lanes.
	switch (xSeg.m_eClass)
	{
	case CB_ROADCLASS_SMALL:  return 3u;
	case CB_ROADCLASS_MEDIUM: return 6u;
	case CB_ROADCLASS_LARGE:  return 10u;
	default:                  return 4u;
	}
}

// Dispatch one trip: a random home (origin node) → a random job/shop (destination node),
// routed via A* over the road graph. Fails (no trip) if there are no homes or no jobs, or
// the two are not road-connected — so a bare or disconnected network carries no traffic.
bool CB_Traffic::SpawnTrip(const CB_RoadGraph& xGraph, const Zenith_Vector<uint32_t>& xOrigins,
                           const Zenith_Vector<uint32_t>& xDests, CB_Vehicle& xV)
{
	if (xOrigins.GetSize() == 0u || xDests.GetSize() == 0u) { return false; }
	const uint32_t uOrigin = xOrigins.Get(NextRand() % xOrigins.GetSize());
	const uint32_t uDest   = xDests.Get(NextRand() % xDests.GetSize());
	if (uOrigin == uDest) { return false; }

	if (!FindPath(xGraph, uOrigin, uDest, m_xPathScratch)) { return false; }   // unreachable
	const uint32_t uLen = m_xPathScratch.GetSize();
	if (uLen == 0u) { return false; }                                          // adjacent / no segments

	const uint32_t uClamp = (uLen < CB_Vehicle::MAX_TRIP_SEGS) ? uLen : CB_Vehicle::MAX_TRIP_SEGS;
	for (uint32_t i = 0; i < uClamp; ++i) { xV.m_auPath[i] = m_xPathScratch.Get(i); }
	xV.m_uPathLen  = uClamp;
	xV.m_uPathIdx  = 0u;
	xV.m_uSeg      = xV.m_auPath[0];
	xV.m_uFromNode = uOrigin;                                // path[0] begins at the origin node
	xV.m_uGoalNode = uDest;
	xV.m_fT        = 0.0f;
	xV.m_fTripTime = 0.0f;
	xV.m_fSpeed    = 12.0f + static_cast<float>(NextRand() % 8u);
	xV.m_xColor    = CarColor(NextRand());
	xV.m_bActive   = true;
	return true;
}

void CB_Traffic::AdvanceVehicle(const CB_RoadGraph& xGraph, const CB_TerrainHeightfield& xField, CB_Vehicle& xV, float fDt)
{
	const CB_RoadSegment* pxSeg = &xGraph.GetSegment(xV.m_uSeg);
	if (!pxSeg->m_bActive) { xV.m_bActive = false; return; }   // road bulldozed under it
	float fLen = pxSeg->m_xSpline.Length();
	if (fLen < 0.01f) { xV.m_bActive = false; return; }

	xV.m_fTripTime += fDt;

	// Congestion: cars on an over-capacity segment crawl (capacity ÷ load, floored so they
	// never fully gridlock). This is what makes busy corridors back up.
	float fSpeedScale = 1.0f;
	if (xV.m_uSeg < m_auSegLoad.GetSize())
	{
		const float fLoad = static_cast<float>(m_auSegLoad.Get(xV.m_uSeg));
		const float fCap  = static_cast<float>(SegCapacity(*pxSeg));
		if (fLoad > fCap && fCap > 0.0f)
		{
			fSpeedScale = fCap / fLoad;
			if (fSpeedScale < 0.2f) { fSpeedScale = 0.2f; }
		}
	}

	xV.m_fT += (xV.m_fSpeed * fSpeedScale * fDt) / fLen;

	if (xV.m_fT >= 1.0f)
	{
		// Reached the far node of the current segment; step to the next segment on the ROUTE
		// (not a random turn — vehicles follow their A* path from home to job/shop).
		const uint32_t uArrive = (xV.m_uFromNode == pxSeg->m_uNodeA) ? pxSeg->m_uNodeB : pxSeg->m_uNodeA;
		++xV.m_uPathIdx;
		if (xV.m_uPathIdx >= xV.m_uPathLen)
		{
			// Arrived at the destination → trip complete; the slot frees for a new trip.
			xV.m_bActive = false;
			++m_xStats.m_uTripsCompleted;
			m_xStats.m_fAvgTripTime = (m_xStats.m_fAvgTripTime <= 0.0f)
				? xV.m_fTripTime : (m_xStats.m_fAvgTripTime * 0.9f + xV.m_fTripTime * 0.1f);
			return;
		}
		xV.m_uSeg      = xV.m_auPath[xV.m_uPathIdx];
		pxSeg          = &xGraph.GetSegment(xV.m_uSeg);
		if (!pxSeg->m_bActive) { xV.m_bActive = false; return; }   // route broken (bulldozed) → abort
		xV.m_uFromNode = uArrive;                                   // consecutive path segments share this node
		xV.m_fT        = 0.0f;
	}

	// World position + facing along the travel direction.
	const bool  bFwd   = (xV.m_uFromNode == pxSeg->m_uNodeA);
	const float fParam = bFwd ? xV.m_fT : (1.0f - xV.m_fT);
	const Zenith_Maths::Vector2 xP   = pxSeg->m_xSpline.Evaluate(fParam);
	Zenith_Maths::Vector2       xTan = pxSeg->m_xSpline.UnitTangent(fParam);
	if (!bFwd) { xTan = Zenith_Maths::Vector2(-xTan.x, -xTan.y); }
	xV.m_xFwd = xTan;
	xV.m_xPos = Zenith_Maths::Vector3(xP.x, xField.GetHeightAt(xP.x, xP.y) + 0.4f, xP.y);
}

void CB_Traffic::Update(const CB_RoadGraph& xGraph, const CB_TerrainHeightfield& xField, float fDt,
                        const Zenith_Vector<uint32_t>& xOriginNodes, const Zenith_Vector<uint32_t>& xDestNodes,
                        uint32_t uTargetVehicles)
{
	// No roads → no traffic at all.
	if (xGraph.GetActiveSegmentCount() == 0u)
	{
		for (uint32_t i = 0; i < m_axVehicles.GetSize(); ++i) { m_axVehicles.Get(i).m_bActive = false; }
		m_xStats.m_uActive = 0u; m_xStats.m_uMaxSegmentLoad = 0u; m_xStats.m_uCongestedSegs = 0u;
		return;
	}

	// 1) Tally per-segment vehicle load from current positions — drives congestion + telemetry.
	const uint32_t uSegSlots = xGraph.GetSegmentSlotCount();
	m_auSegLoad.Clear();
	for (uint32_t s = 0; s < uSegSlots; ++s) { m_auSegLoad.PushBack(0u); }
	for (uint32_t i = 0; i < m_axVehicles.GetSize(); ++i)
	{
		const CB_Vehicle& xV = m_axVehicles.Get(i);
		if (xV.m_bActive && xV.m_uSeg < uSegSlots) { ++m_auSegLoad.Get(xV.m_uSeg); }
	}
	uint32_t uMaxLoad = 0u, uCongested = 0u;
	for (uint32_t s = 0; s < uSegSlots; ++s)
	{
		const uint32_t uLoad = m_auSegLoad.Get(s);
		if (uLoad > uMaxLoad) { uMaxLoad = uLoad; }
		const CB_RoadSegment& xS = xGraph.GetSegment(s);
		if (xS.m_bActive && uLoad > SegCapacity(xS)) { ++uCongested; }
	}
	m_xStats.m_uMaxSegmentLoad = uMaxLoad;
	m_xStats.m_uCongestedSegs  = uCongested;

	// 2) Advance every live trip (despawns arrivals + counts completions inside AdvanceVehicle).
	for (uint32_t i = 0; i < m_axVehicles.GetSize(); ++i)
	{
		CB_Vehicle& xV = m_axVehicles.Get(i);
		if (xV.m_bActive) { AdvanceVehicle(xGraph, xField, xV, fDt); }
	}

	// 3) Dispatch new trips up to the population-derived target. DEMAND-DRIVEN: a trip needs a
	//    home (origin) AND a job/shop (destination), so with no built buildings there is no traffic.
	uint32_t uActive = 0u;
	for (uint32_t i = 0; i < m_axVehicles.GetSize(); ++i) { if (m_axVehicles.Get(i).m_bActive) { ++uActive; } }

	const uint32_t uTarget = (uTargetVehicles < MAX_VEHICLES) ? uTargetVehicles : MAX_VEHICLES;
	uint32_t uAttempts = 0u;
	while (uActive < uTarget && uAttempts < uTarget * 2u + 8u
	       && xOriginNodes.GetSize() > 0u && xDestNodes.GetSize() > 0u)
	{
		++uAttempts;
		uint32_t uSlot = CB_RoadGraph::INVALID;
		for (uint32_t i = 0; i < m_axVehicles.GetSize(); ++i) { if (!m_axVehicles.Get(i).m_bActive) { uSlot = i; break; } }
		if (uSlot == CB_RoadGraph::INVALID)
		{
			if (m_axVehicles.GetSize() >= MAX_VEHICLES) { break; }
			CB_Vehicle xNew;
			m_axVehicles.PushBack(xNew);
			uSlot = m_axVehicles.GetSize() - 1u;
		}
		if (SpawnTrip(xGraph, xOriginNodes, xDestNodes, m_axVehicles.Get(uSlot)))
		{
			AdvanceVehicle(xGraph, xField, m_axVehicles.Get(uSlot), 0.0f);   // place it on the road this frame
			++m_xStats.m_uTripsStarted;
			++uActive;
		}
		// failed pair (unreachable) → slot stays free, loop retries another random O/D
	}

	m_xStats.m_uActive = uActive;
}

void CB_Traffic::Render() const
{
	for (uint32_t i = 0; i < m_axVehicles.GetSize(); ++i)
	{
		const CB_Vehicle& xV = m_axVehicles.Get(i);
		if (!xV.m_bActive) { continue; }
		EmitCarBox(xV.m_xPos, xV.m_xFwd, xV.m_xColor);
	}
}
