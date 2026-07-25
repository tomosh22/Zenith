#include "Zenith.h"
#include "AI/Navigation/Zenith_NavMeshStats.h"

#include "AI/Navigation/Zenith_NavMesh.h"

namespace
{
	// Per-polygon adjacency tally: how many of this polygon's edges point at a
	// polygon index that is actually in range, and how many of those links are
	// counted here (only from the LOWER-indexed side, so a mutual link is one
	// portal rather than two).
	struct NavMeshPolygonLinkTally
	{
		u_int m_uInRangeNeighbours = 0;
		u_int m_uPortalsOwnedByThisPolygon = 0;
	};

	NavMeshPolygonLinkTally TallyPolygonLinks(const Zenith_NavMeshPolygon& xPoly,
		u_int uPolyIndex, u_int uPolygonCount)
	{
		NavMeshPolygonLinkTally xTally;

		for (u_int uEdge = 0; uEdge < xPoly.m_axNeighborIndices.GetSize(); ++uEdge)
		{
			const int32_t iNeighbour = xPoly.m_axNeighborIndices.Get(uEdge);
			if (iNeighbour < 0 || static_cast<u_int>(iNeighbour) >= uPolygonCount)
			{
				continue;
			}

			++xTally.m_uInRangeNeighbours;
			if (uPolyIndex < static_cast<u_int>(iNeighbour))
			{
				++xTally.m_uPortalsOwnedByThisPolygon;
			}
		}

		return xTally;
	}
}

Zenith_NavMeshStats Zenith_NavMeshStats::Compute(const Zenith_NavMesh& xNavMesh)
{
	Zenith_NavMeshStats xStats;

	xStats.m_uVertexCount = xNavMesh.GetVertexCount();
	xStats.m_uPolygonCount = xNavMesh.GetPolygonCount();
	xStats.m_xBoundsMin = xNavMesh.GetBoundsMin();
	xStats.m_xBoundsMax = xNavMesh.GetBoundsMax();
	xStats.m_xExtents = xStats.m_xBoundsMax - xStats.m_xBoundsMin;

	uint64_t ulPolygonBytes = 0ull;

	for (u_int uPoly = 0; uPoly < xStats.m_uPolygonCount; ++uPoly)
	{
		const Zenith_NavMeshPolygon& xPoly = xNavMesh.GetPolygon(uPoly);

		const float fArea = xPoly.m_fArea;
		xStats.m_fTotalArea += fArea;
		if (uPoly == 0u || fArea < xStats.m_fMinPolygonArea)
		{
			xStats.m_fMinPolygonArea = fArea;
		}
		if (uPoly == 0u || fArea > xStats.m_fMaxPolygonArea)
		{
			xStats.m_fMaxPolygonArea = fArea;
		}

		const NavMeshPolygonLinkTally xTally = TallyPolygonLinks(xPoly, uPoly, xStats.m_uPolygonCount);
		xStats.m_uPortalLinkCount += xTally.m_uPortalsOwnedByThisPolygon;
		if (xTally.m_uInRangeNeighbours == 0u)
		{
			++xStats.m_uIsolatedPolygonCount;
		}

		if (xPoly.IsBlocked())
		{
			++xStats.m_uBlockedPolygonCount;
		}

		ulPolygonBytes += sizeof(Zenith_NavMeshPolygon);
		ulPolygonBytes += static_cast<uint64_t>(xPoly.m_axVertexIndices.GetSize()) * sizeof(uint32_t);
		ulPolygonBytes += static_cast<uint64_t>(xPoly.m_axNeighborIndices.GetSize()) * sizeof(int32_t);
	}

	if (xStats.m_uPolygonCount > 0u)
	{
		xStats.m_fAveragePolygonArea = xStats.m_fTotalArea / static_cast<float>(xStats.m_uPolygonCount);
	}

	xStats.m_ulApproxMemoryBytes =
		static_cast<uint64_t>(xStats.m_uVertexCount) * sizeof(Zenith_Maths::Vector3) + ulPolygonBytes;

	return xStats;
}
