#include "Zenith.h"

#include "CityBuilder/Source/CB_RoadGraph.h"
#include "CityBuilder/Source/CB_Serialize.h"

namespace
{
	// First crossing of two spline centrelines (each sampled to a 16-segment polyline).
	// Returns the parameters on each spline at the crossing. Exact for straight roads.
	bool FindSplineCrossing(const CB_Spline& xA, const CB_Spline& xB, float& fOutTA, float& fOutTB)
	{
		constexpr uint32_t N = 16u;
		Zenith_Maths::Vector2 axA[N + 1];
		Zenith_Maths::Vector2 axB[N + 1];
		for (uint32_t i = 0; i <= N; ++i)
		{
			const float ft = static_cast<float>(i) / static_cast<float>(N);
			axA[i] = xA.Evaluate(ft);
			axB[i] = xB.Evaluate(ft);
		}
		for (uint32_t i = 0; i < N; ++i)
		{
			for (uint32_t j = 0; j < N; ++j)
			{
				float fsa = 0.0f, fsb = 0.0f;
				if (CB_Spline::SegSegIntersect(axA[i], axA[i + 1], axB[j], axB[j + 1], fsa, fsb))
				{
					fOutTA = (static_cast<float>(i) + fsa) / static_cast<float>(N);
					fOutTB = (static_cast<float>(j) + fsb) / static_cast<float>(N);
					return true;
				}
			}
		}
		return false;
	}
}

uint32_t CB_RoadGraph::AddNode(const Zenith_Maths::Vector2& xPos)
{
	CB_RoadNode xNode;
	xNode.m_xPos    = xPos;
	xNode.m_uRefs   = 0;
	xNode.m_bActive = true;
	const uint32_t uIdx = m_axNodes.GetSize();
	m_axNodes.PushBack(xNode);
	++m_uActiveNodes;
	return uIdx;
}

uint32_t CB_RoadGraph::FindNodeNear(const Zenith_Maths::Vector2& xPos, float fRadius) const
{
	uint32_t uBest = INVALID;
	float fBestSq = fRadius * fRadius;
	for (uint32_t i = 0; i < m_axNodes.GetSize(); ++i)
	{
		const CB_RoadNode& xN = m_axNodes.Get(i);
		if (!xN.m_bActive) continue;
		const float dx = xN.m_xPos.x - xPos.x;
		const float dy = xN.m_xPos.y - xPos.y;
		const float fSq = dx * dx + dy * dy;
		if (fSq <= fBestSq)
		{
			fBestSq = fSq;
			uBest = i;
		}
	}
	return uBest;
}

uint32_t CB_RoadGraph::FindOrAddNode(const Zenith_Maths::Vector2& xPos, float fSnapRadius)
{
	const uint32_t uExisting = FindNodeNear(xPos, fSnapRadius);
	if (uExisting != INVALID)
	{
		return uExisting;
	}
	return AddNode(xPos);
}

uint32_t CB_RoadGraph::FindOrSplitNodeAt(const Zenith_Maths::Vector2& xPos, float fSnapRadius)
{
	const uint32_t uNode = FindNodeNear(xPos, fSnapRadius);
	if (uNode != INVALID) { return uNode; }                       // snap to an existing junction/endpoint
	const uint32_t uSeg = FindNearestSegment(xPos.x, xPos.y, fSnapRadius);
	if (uSeg != INVALID)
	{
		const float    fT = m_axSegments.Get(uSeg).m_xSpline.ClosestParam(xPos);
		const uint32_t uJ = SplitSegmentAt(uSeg, fT);             // land ON a road → T-junction
		if (uJ != INVALID) { return uJ; }
	}
	return AddNode(xPos);                                          // open ground → a fresh node
}

uint32_t CB_RoadGraph::AddSegment(uint32_t uNodeA, uint32_t uNodeB, const CB_Spline& xSpline, CB_ERoadClass eClass)
{
	Zenith_Assert(uNodeA < m_axNodes.GetSize() && uNodeB < m_axNodes.GetSize(), "CB_RoadGraph::AddSegment bad node index");

	CB_RoadSegment xSeg;
	xSeg.m_uNodeA  = uNodeA;
	xSeg.m_uNodeB  = uNodeB;
	xSeg.m_xSpline = xSpline;
	xSeg.m_eClass  = eClass;
	xSeg.m_fWidth  = ClassWidth(eClass);
	xSeg.m_bActive = true;

	const uint32_t uIdx = m_axSegments.GetSize();
	m_axSegments.PushBack(xSeg);
	++m_uActiveSegments;

	if (uNodeA < m_axNodes.GetSize()) { ++m_axNodes.Get(uNodeA).m_uRefs; }
	if (uNodeB < m_axNodes.GetSize()) { ++m_axNodes.Get(uNodeB).m_uRefs; }
	return uIdx;
}

void CB_RoadGraph::RemoveSegment(uint32_t uSegment)
{
	if (uSegment >= m_axSegments.GetSize()) return;
	CB_RoadSegment& xSeg = m_axSegments.Get(uSegment);
	if (!xSeg.m_bActive) return;

	xSeg.m_bActive = false;
	if (m_uActiveSegments > 0) --m_uActiveSegments;

	const uint32_t auNodes[2] = { xSeg.m_uNodeA, xSeg.m_uNodeB };
	for (uint32_t n = 0; n < 2; ++n)
	{
		const uint32_t uNode = auNodes[n];
		if (uNode >= m_axNodes.GetSize()) continue;
		CB_RoadNode& xNode = m_axNodes.Get(uNode);
		if (xNode.m_uRefs > 0) --xNode.m_uRefs;
		if (xNode.m_uRefs == 0 && xNode.m_bActive)
		{
			xNode.m_bActive = false;
			if (m_uActiveNodes > 0) --m_uActiveNodes;
		}
	}
}

uint32_t CB_RoadGraph::SplitSegmentAt(uint32_t uSegment, float fT)
{
	if (uSegment >= m_axSegments.GetSize() || !m_axSegments.Get(uSegment).m_bActive) { return INVALID; }
	if (fT <= 0.01f || fT >= 0.99f) { return INVALID; }          // too close to an endpoint to be a junction
	const CB_RoadSegment xOld = m_axSegments.Get(uSegment);      // COPY: AddSegment below may reallocate the vector
	CB_Spline xLeft, xRight;
	xOld.m_xSpline.SplitAt(fT, xLeft, xRight);
	const uint32_t uMid = AddNode(xLeft.m_axControl[3]);          // junction at the split point
	AddSegment(xOld.m_uNodeA, uMid,          xLeft,  xOld.m_eClass);
	AddSegment(uMid,          xOld.m_uNodeB, xRight, xOld.m_eClass);
	RemoveSegment(uSegment);                                      // the two new segments preserve nodeA/B refs
	return uMid;
}

uint32_t CB_RoadGraph::AddSegmentWithJunctions(uint32_t uNodeA, uint32_t uNodeB, const CB_Spline& xSpline, CB_ERoadClass eClass)
{
	// 1) Collect crossings of the new centreline with existing active segments (snapshot the segment
	//    count first — we append new segments as we split, and must not re-cross a just-split one).
	struct Crossing { float fTNew; uint32_t uRef; float fTSeg; };
	Crossing axCross[64];
	uint32_t uNumCross = 0u;
	const uint32_t uSegCountBefore = m_axSegments.GetSize();
	for (uint32_t s = 0; s < uSegCountBefore && uNumCross < 64u; ++s)
	{
		const CB_RoadSegment& xE = m_axSegments.Get(s);
		if (!xE.m_bActive) { continue; }
		// already joined at a shared endpoint node → not a crossing
		if (xE.m_uNodeA == uNodeA || xE.m_uNodeA == uNodeB || xE.m_uNodeB == uNodeA || xE.m_uNodeB == uNodeB) { continue; }
		float fTNew = 0.0f, fTSeg = 0.0f;
		if (!FindSplineCrossing(xSpline, xE.m_xSpline, fTNew, fTSeg)) { continue; }
		if (fTNew <= 0.02f || fTNew >= 0.98f || fTSeg <= 0.02f || fTSeg >= 0.98f) { continue; }  // ≈ endpoint
		axCross[uNumCross].fTNew = fTNew;
		axCross[uNumCross].uRef  = s;
		axCross[uNumCross].fTSeg = fTSeg;
		++uNumCross;
	}

	if (uNumCross == 0u)
	{
		return AddSegment(uNodeA, uNodeB, xSpline, eClass);       // no crossing → a single segment
	}

	// 2) Split each crossed existing segment → junction node (stored back into uRef). Splitting one
	//    segment never shifts another's index (soft-delete + append), so the captured s indices hold.
	for (uint32_t i = 0; i < uNumCross; ++i)
	{
		axCross[i].uRef = SplitSegmentAt(axCross[i].uRef, axCross[i].fTSeg);
	}

	// 3) Order the crossings along the new road (insertion sort by fTNew).
	for (uint32_t i = 1; i < uNumCross; ++i)
	{
		const Crossing c = axCross[i];
		uint32_t j = i;
		while (j > 0u && axCross[j - 1].fTNew > c.fTNew) { axCross[j] = axCross[j - 1]; --j; }
		axCross[j] = c;
	}

	// 4) Build the new road as a chain through the junction nodes: A → J1 → ... → Jn → B.
	uint32_t uFirst = INVALID;
	uint32_t uPrev  = uNodeA;
	float    fPrevT = 0.0f;
	for (uint32_t i = 0; i < uNumCross; ++i)
	{
		const uint32_t uJ = axCross[i].uRef;
		const float    fT = axCross[i].fTNew;
		if (uJ == INVALID || uJ == uPrev || (fT - fPrevT) < 0.01f) { continue; }   // split failed / coincident
		const uint32_t uS = AddSegment(uPrev, uJ, xSpline.SubSpline(fPrevT, fT), eClass);
		if (uFirst == INVALID) { uFirst = uS; }
		uPrev  = uJ;
		fPrevT = fT;
	}
	const uint32_t uLast = AddSegment(uPrev, uNodeB, xSpline.SubSpline(fPrevT, 1.0f), eClass);
	return (uFirst != INVALID) ? uFirst : uLast;
}

uint32_t CB_RoadGraph::FindNearestSegment(float fWorldX, float fWorldZ, float fMaxDist) const
{
	const Zenith_Maths::Vector2 xP(fWorldX, fWorldZ);
	uint32_t uBest = INVALID;
	float fBest = fMaxDist;
	for (uint32_t i = 0; i < m_axSegments.GetSize(); ++i)
	{
		const CB_RoadSegment& xS = m_axSegments.Get(i);
		if (!xS.m_bActive) continue;
		const float fD = xS.m_xSpline.DistanceToPoint(xP);
		if (fD <= fBest)
		{
			fBest = fD;
			uBest = i;
		}
	}
	return uBest;
}

uint32_t CB_RoadGraph::CountSegmentsAtNode(uint32_t uNode) const
{
	uint32_t uCount = 0;
	for (uint32_t i = 0; i < m_axSegments.GetSize(); ++i)
	{
		const CB_RoadSegment& xS = m_axSegments.Get(i);
		if (xS.m_bActive && (xS.m_uNodeA == uNode || xS.m_uNodeB == uNode))
		{
			++uCount;
		}
	}
	return uCount;
}

uint32_t CB_RoadGraph::CountJunctions() const
{
	uint32_t uJ = 0u;
	for (uint32_t i = 0; i < m_axNodes.GetSize(); ++i)
	{
		if (m_axNodes.Get(i).m_bActive && CountSegmentsAtNode(i) >= 3u) { ++uJ; }
	}
	return uJ;
}

uint32_t CB_RoadGraph::CountConnectedComponents() const
{
	const uint32_t uN = m_axNodes.GetSize();
	if (uN == 0u) { return 0u; }
	Zenith_Vector<uint32_t> auParent;
	for (uint32_t i = 0; i < uN; ++i) { auParent.PushBack(i); }
	// Union the endpoints of every active segment (iterative find with path-halving).
	for (uint32_t s = 0; s < m_axSegments.GetSize(); ++s)
	{
		const CB_RoadSegment& xS = m_axSegments.Get(s);
		if (!xS.m_bActive || xS.m_uNodeA >= uN || xS.m_uNodeB >= uN) { continue; }
		uint32_t a = xS.m_uNodeA;
		while (auParent.Get(a) != a) { auParent.Get(a) = auParent.Get(auParent.Get(a)); a = auParent.Get(a); }
		uint32_t b = xS.m_uNodeB;
		while (auParent.Get(b) != b) { auParent.Get(b) = auParent.Get(auParent.Get(b)); b = auParent.Get(b); }
		if (a != b) { auParent.Get(a) = b; }
	}
	// One active node per component is its own root → count those.
	uint32_t uComp = 0u;
	for (uint32_t i = 0; i < uN; ++i)
	{
		if (!m_axNodes.Get(i).m_bActive) { continue; }
		uint32_t r = i;
		while (auParent.Get(r) != r) { r = auParent.Get(r); }
		if (r == i) { ++uComp; }
	}
	return uComp;
}

float CB_RoadGraph::MinDistanceToAnyRoad(float fWorldX, float fWorldZ) const
{
	const Zenith_Maths::Vector2 xP(fWorldX, fWorldZ);
	float fBest = 1e30f;
	for (uint32_t i = 0; i < m_axSegments.GetSize(); ++i)
	{
		const CB_RoadSegment& xS = m_axSegments.Get(i);
		if (!xS.m_bActive) continue;
		const float fD = xS.m_xSpline.DistanceToPoint(xP);
		if (fD < fBest) fBest = fD;
	}
	return fBest;
}

void CB_RoadGraph::Clear()
{
	m_axNodes.Clear();
	m_axSegments.Clear();
	m_uActiveSegments = 0;
	m_uActiveNodes    = 0;
}

float CB_RoadGraph::GetTotalActiveLength() const
{
	float fTotal = 0.0f;
	for (uint32_t i = 0; i < m_axSegments.GetSize(); ++i)
	{
		const CB_RoadSegment& xSeg = m_axSegments.Get(i);
		if (xSeg.m_bActive) { fTotal += xSeg.m_xSpline.Length(); }
	}
	return fTotal;
}

void CB_RoadGraph::WriteToDataStream(Zenith_DataStream& xStream) const
{
	CB_Serialize::WriteVec(xStream, m_axNodes);
	CB_Serialize::WriteVec(xStream, m_axSegments);
	xStream << m_uActiveSegments;
	xStream << m_uActiveNodes;
}

void CB_RoadGraph::ReadFromDataStream(Zenith_DataStream& xStream)
{
	CB_Serialize::ReadVec(xStream, m_axNodes);
	CB_Serialize::ReadVec(xStream, m_axSegments);
	xStream >> m_uActiveSegments;
	xStream >> m_uActiveNodes;
}
