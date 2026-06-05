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
	m_uActive = 0;
	m_uRng    = 0x1234567u;
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

bool CB_Traffic::SpawnVehicle(const CB_RoadGraph& xGraph, CB_Vehicle& xV)
{
	const uint32_t uSegs = xGraph.GetSegmentSlotCount();
	if (uSegs == 0) { return false; }
	for (uint32_t uTry = 0; uTry < 32; ++uTry)
	{
		const uint32_t s = NextRand() % uSegs;
		const CB_RoadSegment& xSeg = xGraph.GetSegment(s);
		if (!xSeg.m_bActive) { continue; }
		xV.m_uSeg      = s;
		xV.m_uFromNode = xSeg.m_uNodeA;
		xV.m_fT        = static_cast<float>(NextRand() % 100u) / 100.0f;
		xV.m_fSpeed    = 12.0f + static_cast<float>(NextRand() % 10u);
		xV.m_xColor    = CarColor(NextRand());
		xV.m_bActive   = true;
		return true;
	}
	return false;
}

void CB_Traffic::AdvanceVehicle(const CB_RoadGraph& xGraph, const CB_TerrainHeightfield& xField, CB_Vehicle& xV, float fDt)
{
	const CB_RoadSegment* pxSeg = &xGraph.GetSegment(xV.m_uSeg);
	if (!pxSeg->m_bActive) { xV.m_bActive = false; return; }   // road bulldozed under it
	float fLen = pxSeg->m_xSpline.Length();
	if (fLen < 0.01f) { xV.m_bActive = false; return; }

	xV.m_fT += (xV.m_fSpeed * fDt) / fLen;

	if (xV.m_fT >= 1.0f)
	{
		// Arrived at the far node; turn onto a random connected road (avoid U-turns
		// where possible, else turn around at a dead end).
		const uint32_t uArrive = (xV.m_uFromNode == pxSeg->m_uNodeA) ? pxSeg->m_uNodeB : pxSeg->m_uNodeA;
		uint32_t auCand[16];
		uint32_t uNum = 0;
		const uint32_t uSegs = xGraph.GetSegmentSlotCount();
		for (uint32_t s = 0; s < uSegs && uNum < 16u; ++s)
		{
			const CB_RoadSegment& xS = xGraph.GetSegment(s);
			if (!xS.m_bActive || s == xV.m_uSeg) { continue; }
			if (xS.m_uNodeA == uArrive || xS.m_uNodeB == uArrive) { auCand[uNum++] = s; }
		}
		const uint32_t uNext = (uNum > 0) ? auCand[NextRand() % uNum] : xV.m_uSeg;  // dead end → U-turn
		xV.m_uSeg      = uNext;
		xV.m_uFromNode = uArrive;
		xV.m_fT        = 0.0f;
		pxSeg = &xGraph.GetSegment(xV.m_uSeg);
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

void CB_Traffic::Update(const CB_RoadGraph& xGraph, const CB_TerrainHeightfield& xField, float fDt)
{
	if (xGraph.GetActiveSegmentCount() == 0)
	{
		for (uint32_t i = 0; i < m_axVehicles.GetSize(); ++i) { m_axVehicles.Get(i).m_bActive = false; }
		m_uActive = 0;
		return;
	}

	for (uint32_t i = 0; i < m_axVehicles.GetSize(); ++i)
	{
		CB_Vehicle& xV = m_axVehicles.Get(i);
		if (xV.m_bActive) { AdvanceVehicle(xGraph, xField, xV, fDt); }
	}

	m_uActive = 0;
	for (uint32_t i = 0; i < m_axVehicles.GetSize(); ++i) { if (m_axVehicles.Get(i).m_bActive) { ++m_uActive; } }

	uint32_t uGuard = 0;
	while (m_uActive < TARGET_VEHICLES && uGuard++ < TARGET_VEHICLES + 4u)
	{
		uint32_t uSlot = CB_RoadGraph::INVALID;
		for (uint32_t i = 0; i < m_axVehicles.GetSize(); ++i) { if (!m_axVehicles.Get(i).m_bActive) { uSlot = i; break; } }
		if (uSlot == CB_RoadGraph::INVALID)
		{
			CB_Vehicle xNew;
			m_axVehicles.PushBack(xNew);
			uSlot = m_axVehicles.GetSize() - 1u;
		}
		if (SpawnVehicle(xGraph, m_axVehicles.Get(uSlot)))
		{
			AdvanceVehicle(xGraph, xField, m_axVehicles.Get(uSlot), 0.0f);
			++m_uActive;
		}
		else { break; }
	}
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
