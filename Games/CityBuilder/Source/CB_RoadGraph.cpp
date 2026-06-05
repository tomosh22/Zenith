#include "Zenith.h"

#include "CityBuilder/Source/CB_RoadGraph.h"
#include "CityBuilder/Source/CB_Serialize.h"

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
