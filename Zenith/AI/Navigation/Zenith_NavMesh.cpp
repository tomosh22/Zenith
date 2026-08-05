#include "Zenith.h"
#include "AI/Navigation/Zenith_NavMesh.h"
#include "AI/Zenith_AIWorldHooks.h"
#include "DataStream/Zenith_DataStream.h"
#include "FileAccess/Zenith_FileAccess.h"


namespace
{
	// Bytes still readable from xStream. GetCapacity() is the buffer length —
	// which IS the file length for a stream produced by ReadFromFile — and
	// GetCursor() is how far the read has advanced. For an OWNED write-then-
	// rewind stream the capacity over-reports (the allocator doubles past the
	// bytes written), which makes the plausibility checks below weaker but never
	// wrong: they exist to stop a corrupt count driving a huge Reserve, and the
	// per-field bounds checks behind them are exact either way.
	uint64_t NavMeshBytesRemaining(const Zenith_DataStream& xStream)
	{
		const uint64_t ulCapacity = xStream.GetCapacity();
		const uint64_t ulCursor = xStream.GetCursor();
		return (ulCursor < ulCapacity) ? (ulCapacity - ulCursor) : 0ull;
	}

	bool NavMeshIsFiniteVector(const Zenith_Maths::Vector3& xVector)
	{
		return std::isfinite(xVector.x) && std::isfinite(xVector.y) && std::isfinite(xVector.z);
	}

	// Smallest byte cost of one encoded polygon record: vertexCount(4) + 3
	// indices(12) + neighbourCount(4) + 3 neighbours(12) + center/normal/area(28)
	// + flags(4) + cost(4). Used ONLY to reject an absurd polygon count before
	// Reserve; every field is bounds-checked again as it is read.
	constexpr uint64_t ulNAVMESH_MIN_POLYGON_BYTES = 68ull;

	constexpr uint64_t ulNAVMESH_VERTEX_BYTES = 3ull * sizeof(float);

	// Shared by GetRandomReachablePointInRadius's polygon pick (Phase 4) and
	// SampleUniformPointInPolygon's triangle pick: walk a cumulative-weight
	// array and return the index of the first entry the scaled sample lands
	// in. fPick = SampleUnit() * fTotal is always < the final cumulative value
	// (SampleUnit is exclusive of 1.0), so the fallback below is unreachable
	// in practice; it defaults to index 0 rather than GetSize()-1 to match
	// both call sites' pre-extraction behaviour exactly.
	uint32_t PickWeightedIndex(const Zenith_Vector<float>& afCumulative, float fPick)
	{
		for (uint32_t i = 0; i < afCumulative.GetSize(); ++i)
		{
			if (fPick <= afCumulative.Get(i))
			{
				return i;
			}
		}
		return 0;
	}
}

// ========== Zenith_NavMeshPolygon ==========

void Zenith_NavMeshPolygon::ComputeSpatialData(const Zenith_Vector<Zenith_Maths::Vector3>& axVertices)
{
	if (m_axVertexIndices.GetSize() < 3)
	{
		m_xCenter = Zenith_Maths::Vector3(0.0f);
		m_xNormal = Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f);
		m_fArea = 0.0f;
		return;
	}

	// Compute center as average of vertices
	m_xCenter = Zenith_Maths::Vector3(0.0f);
	for (uint32_t u = 0; u < m_axVertexIndices.GetSize(); ++u)
	{
		m_xCenter += axVertices.Get(m_axVertexIndices.Get(u));
	}
	m_xCenter /= static_cast<float>(m_axVertexIndices.GetSize());

	// Compute normal using Newell's method (handles non-planar polygons)
	m_xNormal = Zenith_Maths::Vector3(0.0f);
	for (uint32_t u = 0; u < m_axVertexIndices.GetSize(); ++u)
	{
		const Zenith_Maths::Vector3& xCurrent = axVertices.Get(m_axVertexIndices.Get(u));
		const Zenith_Maths::Vector3& xNext = axVertices.Get(m_axVertexIndices.Get((u + 1) % m_axVertexIndices.GetSize()));

		m_xNormal.x += (xCurrent.y - xNext.y) * (xCurrent.z + xNext.z);
		m_xNormal.y += (xCurrent.z - xNext.z) * (xCurrent.x + xNext.x);
		m_xNormal.z += (xCurrent.x - xNext.x) * (xCurrent.y + xNext.y);
	}

	float fNormalLength = Zenith_Maths::Length(m_xNormal);
	if (fNormalLength > 0.0001f)
	{
		m_xNormal /= fNormalLength;
	}
	else
	{
		m_xNormal = Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f);
	}

	// Compute area using triangulation from center
	m_fArea = 0.0f;
	for (uint32_t u = 0; u < m_axVertexIndices.GetSize(); ++u)
	{
		const Zenith_Maths::Vector3& xCurrent = axVertices.Get(m_axVertexIndices.Get(u));
		const Zenith_Maths::Vector3& xNext = axVertices.Get(m_axVertexIndices.Get((u + 1) % m_axVertexIndices.GetSize()));

		Zenith_Maths::Vector3 xEdge1 = xCurrent - m_xCenter;
		Zenith_Maths::Vector3 xEdge2 = xNext - m_xCenter;
		Zenith_Maths::Vector3 xCross = Zenith_Maths::Cross(xEdge1, xEdge2);
		m_fArea += Zenith_Maths::Length(xCross) * 0.5f;
	}
}

bool Zenith_NavMeshPolygon::ContainsPoint(const Zenith_Maths::Vector3& xPoint,
	const Zenith_Vector<Zenith_Maths::Vector3>& axVertices) const
{
	if (m_axVertexIndices.GetSize() < 3)
	{
		return false;
	}

	// Project point to polygon plane
	float fDist = Zenith_Maths::Dot(xPoint - m_xCenter, m_xNormal);
	Zenith_Maths::Vector3 xProjected = xPoint - m_xNormal * fDist;

	// Check if point is on the same side of all edges (for convex polygon)
	for (uint32_t u = 0; u < m_axVertexIndices.GetSize(); ++u)
	{
		const Zenith_Maths::Vector3& xV1 = axVertices.Get(m_axVertexIndices.Get(u));
		const Zenith_Maths::Vector3& xV2 = axVertices.Get(m_axVertexIndices.Get((u + 1) % m_axVertexIndices.GetSize()));

		Zenith_Maths::Vector3 xEdge = xV2 - xV1;
		Zenith_Maths::Vector3 xToPoint = xProjected - xV1;
		Zenith_Maths::Vector3 xCross = Zenith_Maths::Cross(xEdge, xToPoint);

		if (Zenith_Maths::Dot(xCross, m_xNormal) < -0.0001f)
		{
			return false;
		}
	}

	return true;
}

Zenith_Maths::Vector3 Zenith_NavMeshPolygon::GetClosestPoint(const Zenith_Maths::Vector3& xPoint,
	const Zenith_Vector<Zenith_Maths::Vector3>& axVertices) const
{
	if (m_axVertexIndices.GetSize() < 3)
	{
		return xPoint;
	}

	// First check if point projects inside polygon
	float fDist = Zenith_Maths::Dot(xPoint - m_xCenter, m_xNormal);
	Zenith_Maths::Vector3 xProjected = xPoint - m_xNormal * fDist;

	if (ContainsPoint(xProjected, axVertices))
	{
		return xProjected;
	}

	// Find closest point on edges
	Zenith_Maths::Vector3 xClosest = axVertices.Get(m_axVertexIndices.Get(0));
	float fMinDistSq = Zenith_Maths::LengthSq(xPoint - xClosest);

	for (uint32_t u = 0; u < m_axVertexIndices.GetSize(); ++u)
	{
		const Zenith_Maths::Vector3& xV1 = axVertices.Get(m_axVertexIndices.Get(u));
		const Zenith_Maths::Vector3& xV2 = axVertices.Get(m_axVertexIndices.Get((u + 1) % m_axVertexIndices.GetSize()));

		Zenith_Maths::Vector3 xEdge = xV2 - xV1;
		float fEdgeLengthSq = Zenith_Maths::LengthSq(xEdge);

		if (fEdgeLengthSq < 0.0001f)
		{
			continue;
		}

		float fT = Zenith_Maths::Dot(xPoint - xV1, xEdge) / fEdgeLengthSq;
		fT = std::max(0.0f, std::min(1.0f, fT));

		Zenith_Maths::Vector3 xEdgePoint = xV1 + xEdge * fT;
		float fDistSq = Zenith_Maths::LengthSq(xPoint - xEdgePoint);

		if (fDistSq < fMinDistSq)
		{
			fMinDistSq = fDistSq;
			xClosest = xEdgePoint;
		}
	}

	return xClosest;
}

void Zenith_NavMeshPolygon::WriteToDataStream(Zenith_DataStream& xStream) const
{
	// Write vertex indices
	uint32_t uVertCount = m_axVertexIndices.GetSize();
	xStream << uVertCount;
	for (uint32_t u = 0; u < uVertCount; ++u)
	{
		xStream << m_axVertexIndices.Get(u);
	}

	// Write neighbor indices
	uint32_t uNeighborCount = m_axNeighborIndices.GetSize();
	xStream << uNeighborCount;
	for (uint32_t u = 0; u < uNeighborCount; ++u)
	{
		xStream << m_axNeighborIndices.Get(u);
	}

	// Write spatial data
	xStream << m_xCenter.x;
	xStream << m_xCenter.y;
	xStream << m_xCenter.z;
	xStream << m_xNormal.x;
	xStream << m_xNormal.y;
	xStream << m_xNormal.z;
	xStream << m_fArea;

	// Write flags and cost
	xStream << m_uFlags;
	xStream << m_fCost;
}

bool Zenith_NavMeshPolygon::ReadFromDataStream(Zenith_DataStream& xStream,
	uint32_t uMeshVertexCount, uint32_t uMeshPolygonCount)
{
	m_axVertexIndices.Clear();
	m_axNeighborIndices.Clear();

	// ---- vertex indices ----------------------------------------------------
	if (NavMeshBytesRemaining(xStream) < sizeof(uint32_t))
	{
		Zenith_Assert(false, "NavMesh load: truncated before a polygon's vertex count");
		Zenith_Error(LOG_CATEGORY_AI, "NavMesh load: truncated before a polygon's vertex count");
		return false;
	}

	uint32_t uVertCount = 0;
	xStream >> uVertCount;
	if (uVertCount < 3u || uVertCount > uZENITH_NAVMESH_MAX_POLYGON_VERTICES)
	{
		Zenith_Assert(false, "NavMesh load: polygon vertex count %u out of range [3, %u]",
			uVertCount, uZENITH_NAVMESH_MAX_POLYGON_VERTICES);
		Zenith_Error(LOG_CATEGORY_AI, "NavMesh load: polygon vertex count %u out of range [3, %u]",
			uVertCount, uZENITH_NAVMESH_MAX_POLYGON_VERTICES);
		return false;
	}

	if (static_cast<uint64_t>(uVertCount) * sizeof(uint32_t) > NavMeshBytesRemaining(xStream))
	{
		Zenith_Assert(false, "NavMesh load: truncated inside a polygon's %u vertex indices", uVertCount);
		Zenith_Error(LOG_CATEGORY_AI, "NavMesh load: truncated inside a polygon's %u vertex indices", uVertCount);
		return false;
	}

	m_axVertexIndices.Reserve(uVertCount);
	for (uint32_t u = 0; u < uVertCount; ++u)
	{
		uint32_t uIdx = 0;
		xStream >> uIdx;
		if (uIdx >= uMeshVertexCount)
		{
			// Checked BEFORE it is stored: an out-of-range index would otherwise
			// reach ComputeSpatialData's unchecked axVertices.Get().
			Zenith_Assert(false, "NavMesh load: polygon vertex index %u >= vertex count %u",
				uIdx, uMeshVertexCount);
			Zenith_Error(LOG_CATEGORY_AI, "NavMesh load: polygon vertex index %u >= vertex count %u",
				uIdx, uMeshVertexCount);
			return false;
		}
		m_axVertexIndices.PushBack(uIdx);
	}

	// ---- neighbour indices -------------------------------------------------
	if (NavMeshBytesRemaining(xStream) < sizeof(uint32_t))
	{
		Zenith_Assert(false, "NavMesh load: truncated before a polygon's neighbour count");
		Zenith_Error(LOG_CATEGORY_AI, "NavMesh load: truncated before a polygon's neighbour count");
		return false;
	}

	uint32_t uNeighborCount = 0;
	xStream >> uNeighborCount;

	// At LEAST one neighbour slot per edge (AddPolygon / ComputeAdjacency size the
	// list 1-per-edge), but NOT exactly: StitchPortalAt deliberately APPENDS a
	// phantom slot past the vertex count to bridge two rooms that share no edge
	// (DPDoor relies on it). Requiring equality here would hard-assert on a
	// perfectly legitimate stitched mesh the moment it was baked and reloaded --
	// which is exactly the workflow this feature exists to enable. The bound that
	// actually protects memory is the per-index range check below; this one only
	// rejects a count too small to be a real polygon or too large to be anything
	// but corruption.
	if (uNeighborCount < uVertCount || uNeighborCount > uZENITH_NAVMESH_MAX_POLYGON_VERTICES)
	{
		Zenith_Assert(false, "NavMesh load: polygon neighbour count %u outside [%u, %u]",
			uNeighborCount, uVertCount, uZENITH_NAVMESH_MAX_POLYGON_VERTICES);
		Zenith_Error(LOG_CATEGORY_AI, "NavMesh load: polygon neighbour count %u outside [%u, %u]",
			uNeighborCount, uVertCount, uZENITH_NAVMESH_MAX_POLYGON_VERTICES);
		return false;
	}

	if (static_cast<uint64_t>(uNeighborCount) * sizeof(int32_t) > NavMeshBytesRemaining(xStream))
	{
		Zenith_Assert(false, "NavMesh load: truncated inside a polygon's %u neighbour indices", uNeighborCount);
		Zenith_Error(LOG_CATEGORY_AI, "NavMesh load: truncated inside a polygon's %u neighbour indices", uNeighborCount);
		return false;
	}

	m_axNeighborIndices.Reserve(uNeighborCount);
	for (uint32_t u = 0; u < uNeighborCount; ++u)
	{
		int32_t iIdx = 0;
		xStream >> iIdx;
		const bool bValid = (iIdx == iZENITH_NAVMESH_NO_NEIGHBOUR) ||
			(iIdx >= 0 && static_cast<uint32_t>(iIdx) < uMeshPolygonCount);
		if (!bValid)
		{
			Zenith_Assert(false, "NavMesh load: polygon neighbour index %d outside {%d} u [0, %u)",
				iIdx, iZENITH_NAVMESH_NO_NEIGHBOUR, uMeshPolygonCount);
			Zenith_Error(LOG_CATEGORY_AI, "NavMesh load: polygon neighbour index %d outside {%d} u [0, %u)",
				iIdx, iZENITH_NAVMESH_NO_NEIGHBOUR, uMeshPolygonCount);
			return false;
		}
		m_axNeighborIndices.PushBack(iIdx);
	}

	// ---- cached spatial data + flags/cost -----------------------------------
	// center(12) + normal(12) + area(4) + flags(4) + cost(4).
	constexpr uint64_t ulTAIL_BYTES = 36ull;
	if (NavMeshBytesRemaining(xStream) < ulTAIL_BYTES)
	{
		Zenith_Assert(false, "NavMesh load: truncated inside a polygon's cached spatial data");
		Zenith_Error(LOG_CATEGORY_AI, "NavMesh load: truncated inside a polygon's cached spatial data");
		return false;
	}

	// Read but deliberately NOT validated: BuildSpatialGrid -> ComputeSpatialData
	// overwrites center/normal/area from the vertices on every load, so a corrupt
	// value here cannot survive (see the wire-format comment in the header).
	xStream >> m_xCenter.x;
	xStream >> m_xCenter.y;
	xStream >> m_xCenter.z;
	xStream >> m_xNormal.x;
	xStream >> m_xNormal.y;
	xStream >> m_xNormal.z;
	xStream >> m_fArea;

	xStream >> m_uFlags;
	xStream >> m_fCost;

	// The cost multiplies every A* edge weight, so a non-finite value poisons
	// pathfinding rather than being recomputed away.
	if (!std::isfinite(m_fCost))
	{
		Zenith_Assert(false, "NavMesh load: polygon traversal cost is not finite");
		Zenith_Error(LOG_CATEGORY_AI, "NavMesh load: polygon traversal cost is not finite");
		return false;
	}

	return true;
}

// ========== Zenith_NavMesh ==========

#ifdef ZENITH_INPUT_SIMULATOR
namespace
{
	// MVP-0.4.4: per-process FindPath query counter. Lives in the anonymous
	// namespace so it stays a TU-local static; the three accessors below
	// expose it through the class API.
	u_int s_uFindPathQueryCount = 0;
}

u_int Zenith_NavMesh::GetQueryCountForTest()
{
	return s_uFindPathQueryCount;
}

void Zenith_NavMesh::ResetQueryCountForTest()
{
	s_uFindPathQueryCount = 0;
}

void Zenith_NavMesh::IncrementQueryCountForTest_Internal()
{
	++s_uFindPathQueryCount;
}
#endif

void Zenith_NavMesh::Clear()
{
	m_axVertices.Clear();
	m_axPolygons.Clear();
	m_axGridCells.Clear();
	m_uGridWidth = 0;
	m_uGridHeight = 0;
	m_xBoundsMin = Zenith_Maths::Vector3(0.0f);
	m_xBoundsMax = Zenith_Maths::Vector3(0.0f);
	// Rewind the sampling stream too: a reloaded mesh must replay the same
	// sequence, not continue the previous mesh's.
	m_ulSampleRngState = k_ulSampleRngSeed;
}

uint32_t Zenith_NavMesh::AddVertex(const Zenith_Maths::Vector3& xVertex)
{
	uint32_t uIndex = m_axVertices.GetSize();
	m_axVertices.PushBack(xVertex);
	return uIndex;
}

uint32_t Zenith_NavMesh::AddPolygon(const Zenith_Vector<uint32_t>& axVertexIndices)
{
	uint32_t uIndex = m_axPolygons.GetSize();

	Zenith_NavMeshPolygon xPoly;
	xPoly.m_axVertexIndices = axVertexIndices;

	// Initialize neighbor indices to -1 (no neighbor)
	xPoly.m_axNeighborIndices.Clear();
	xPoly.m_axNeighborIndices.Reserve(axVertexIndices.GetSize());
	for (uint32_t u = 0; u < axVertexIndices.GetSize(); ++u)
	{
		xPoly.m_axNeighborIndices.PushBack(-1);
	}

	m_axPolygons.PushBack(std::move(xPoly));
	return uIndex;
}

void Zenith_NavMesh::SetNeighbor(uint32_t uPoly1, uint32_t uEdge1, uint32_t uPoly2)
{
	Zenith_Assert(uPoly1 < m_axPolygons.GetSize(), "Polygon index out of bounds");
	Zenith_Assert(uPoly2 < m_axPolygons.GetSize(), "Polygon index out of bounds");

	Zenith_NavMeshPolygon& xPoly1 = m_axPolygons.Get(uPoly1);
	Zenith_Assert(uEdge1 < xPoly1.m_axNeighborIndices.GetSize(), "Edge index out of bounds");

	xPoly1.m_axNeighborIndices.Get(uEdge1) = static_cast<int32_t>(uPoly2);
}

void Zenith_NavMesh::ComputeSpatialData()
{
	if (m_axVertices.GetSize() == 0)
	{
		return;
	}

	// Compute bounds
	m_xBoundsMin = m_axVertices.Get(0);
	m_xBoundsMax = m_axVertices.Get(0);

	for (uint32_t u = 1; u < m_axVertices.GetSize(); ++u)
	{
		m_xBoundsMin.x = std::min(m_xBoundsMin.x, m_axVertices.Get(u).x);
		m_xBoundsMin.y = std::min(m_xBoundsMin.y, m_axVertices.Get(u).y);
		m_xBoundsMin.z = std::min(m_xBoundsMin.z, m_axVertices.Get(u).z);

		m_xBoundsMax.x = std::max(m_xBoundsMax.x, m_axVertices.Get(u).x);
		m_xBoundsMax.y = std::max(m_xBoundsMax.y, m_axVertices.Get(u).y);
		m_xBoundsMax.z = std::max(m_xBoundsMax.z, m_axVertices.Get(u).z);
	}

	// Compute polygon spatial data
	for (uint32_t u = 0; u < m_axPolygons.GetSize(); ++u)
	{
		m_axPolygons.Get(u).ComputeSpatialData(m_axVertices);
	}
}

void Zenith_NavMesh::ComputeAdjacency()
{
	// For each polygon, check all other polygons for shared edges
	for (uint32_t uPoly1 = 0; uPoly1 < m_axPolygons.GetSize(); ++uPoly1)
	{
		Zenith_NavMeshPolygon& xPoly1 = m_axPolygons.Get(uPoly1);

		// Initialize neighbor indices to -1 (no neighbor)
		xPoly1.m_axNeighborIndices.Clear();
		for (uint32_t u = 0; u < xPoly1.m_axVertexIndices.GetSize(); ++u)
		{
			xPoly1.m_axNeighborIndices.PushBack(-1);
		}
	}

	// Find shared edges between polygons
	for (uint32_t uPoly1 = 0; uPoly1 < m_axPolygons.GetSize(); ++uPoly1)
	{
		Zenith_NavMeshPolygon& xPoly1 = m_axPolygons.Get(uPoly1);

		for (uint32_t uPoly2 = uPoly1 + 1; uPoly2 < m_axPolygons.GetSize(); ++uPoly2)
		{
			Zenith_NavMeshPolygon& xPoly2 = m_axPolygons.Get(uPoly2);

			// Check each edge of poly1 against each edge of poly2
			for (uint32_t uEdge1 = 0; uEdge1 < xPoly1.m_axVertexIndices.GetSize(); ++uEdge1)
			{
				uint32_t uV1a = xPoly1.m_axVertexIndices.Get(uEdge1);
				uint32_t uV1b = xPoly1.m_axVertexIndices.Get((uEdge1 + 1) % xPoly1.m_axVertexIndices.GetSize());

				for (uint32_t uEdge2 = 0; uEdge2 < xPoly2.m_axVertexIndices.GetSize(); ++uEdge2)
				{
					uint32_t uV2a = xPoly2.m_axVertexIndices.Get(uEdge2);
					uint32_t uV2b = xPoly2.m_axVertexIndices.Get((uEdge2 + 1) % xPoly2.m_axVertexIndices.GetSize());

					// Edges share if they have the same vertices (in opposite order for adjacent polys)
					if ((uV1a == uV2b && uV1b == uV2a) || (uV1a == uV2a && uV1b == uV2b))
					{
						xPoly1.m_axNeighborIndices.Get(uEdge1) = static_cast<int32_t>(uPoly2);
						xPoly2.m_axNeighborIndices.Get(uEdge2) = static_cast<int32_t>(uPoly1);
					}
				}
			}
		}
	}
}

void Zenith_NavMesh::BuildSpatialGrid()
{
	if (m_axPolygons.GetSize() == 0)
	{
		return;
	}

	// Always ensure spatial data is computed (bounds and polygon centers/normals needed)
	// This is safe to call multiple times as it just recomputes the same values
	ComputeSpatialData();

	// Calculate grid dimensions
	Zenith_Maths::Vector3 xSize = m_xBoundsMax - m_xBoundsMin;
	m_uGridWidth = static_cast<uint32_t>(std::ceil(xSize.x / m_fGridCellSize)) + 1;
	m_uGridHeight = static_cast<uint32_t>(std::ceil(xSize.z / m_fGridCellSize)) + 1;

	// Clamp to reasonable size
	m_uGridWidth = std::min(m_uGridWidth, 256u);
	m_uGridHeight = std::min(m_uGridHeight, 256u);

	// Allocate grid
	m_axGridCells.Clear();
	uint32_t uGridSize = m_uGridWidth * m_uGridHeight;
	m_axGridCells.Reserve(uGridSize);
	for (uint32_t u = 0; u < uGridSize; ++u)
	{
		m_axGridCells.PushBack(GridCell());
	}

	// Add each polygon to all cells it overlaps
	for (uint32_t uPoly = 0; uPoly < m_axPolygons.GetSize(); ++uPoly)
	{
		const Zenith_NavMeshPolygon& xPoly = m_axPolygons.Get(uPoly);

		// Get polygon bounds
		Zenith_Maths::Vector3 xPolyMin, xPolyMax;
		ComputePolygonBounds2D(xPoly, m_axVertices, xPolyMin, xPolyMax);

		// Get cell range
		int32_t iMinX, iMinZ, iMaxX, iMaxZ;
		GetGridCoords(xPolyMin, iMinX, iMinZ);
		GetGridCoords(xPolyMax, iMaxX, iMaxZ);

		// Add polygon to all overlapping cells
		for (int32_t iZ = iMinZ; iZ <= iMaxZ; ++iZ)
		{
			for (int32_t iX = iMinX; iX <= iMaxX; ++iX)
			{
				uint32_t uCellIndex = GetGridCellIndex(iX, iZ);
				if (uCellIndex < m_axGridCells.GetSize())
				{
					m_axGridCells.Get(uCellIndex).m_axPolygonIndices.PushBack(uPoly);
				}
			}
		}
	}
}

void Zenith_NavMesh::ComputePolygonBounds2D(const Zenith_NavMeshPolygon& xPoly,
	const Zenith_Vector<Zenith_Maths::Vector3>& axVertices,
	Zenith_Maths::Vector3& xPolyMinOut, Zenith_Maths::Vector3& xPolyMaxOut)
{
	Zenith_Assert(xPoly.m_axVertexIndices.GetSize() > 0, "Polygon has no vertices");

	xPolyMinOut = axVertices.Get(xPoly.m_axVertexIndices.Get(0));
	xPolyMaxOut = xPolyMinOut;

	for (uint32_t u = 1; u < xPoly.m_axVertexIndices.GetSize(); ++u)
	{
		const Zenith_Maths::Vector3& xV = axVertices.Get(xPoly.m_axVertexIndices.Get(u));
		xPolyMinOut.x = std::min(xPolyMinOut.x, xV.x);
		xPolyMinOut.z = std::min(xPolyMinOut.z, xV.z);
		xPolyMaxOut.x = std::max(xPolyMaxOut.x, xV.x);
		xPolyMaxOut.z = std::max(xPolyMaxOut.z, xV.z);
	}
}

void Zenith_NavMesh::GetGridCoords(const Zenith_Maths::Vector3& xPos, int32_t& iX, int32_t& iZ) const
{
	iX = static_cast<int32_t>((xPos.x - m_xBoundsMin.x) / m_fGridCellSize);
	iZ = static_cast<int32_t>((xPos.z - m_xBoundsMin.z) / m_fGridCellSize);
	iX = std::max(0, std::min(static_cast<int32_t>(m_uGridWidth) - 1, iX));
	iZ = std::max(0, std::min(static_cast<int32_t>(m_uGridHeight) - 1, iZ));
}

uint32_t Zenith_NavMesh::GetGridCellIndex(int32_t iX, int32_t iZ) const
{
	return static_cast<uint32_t>(iZ) * m_uGridWidth + static_cast<uint32_t>(iX);
}

void Zenith_NavMesh::FindNearestPolygonInCell(uint32_t uCellIndex, const Zenith_Maths::Vector3& xPoint,
	float& fMinDistSq, uint32_t& uPolyOut, Zenith_Maths::Vector3& xNearestOut) const
{
	if (uCellIndex >= m_axGridCells.GetSize())
	{
		return;
	}

	const GridCell& xCell = m_axGridCells.Get(uCellIndex);

	for (uint32_t u = 0; u < xCell.m_axPolygonIndices.GetSize(); ++u)
	{
		uint32_t uPoly = xCell.m_axPolygonIndices.Get(u);
		const Zenith_NavMeshPolygon& xPoly = m_axPolygons.Get(uPoly);

		Zenith_Maths::Vector3 xClosest = xPoly.GetClosestPoint(xPoint, m_axVertices);
		float fDistSq = Zenith_Maths::LengthSq(xPoint - xClosest);

		if (fDistSq < fMinDistSq)
		{
			fMinDistSq = fDistSq;
			uPolyOut = uPoly;
			xNearestOut = xClosest;
		}
	}
}

bool Zenith_NavMesh::FindNearestPolygon(const Zenith_Maths::Vector3& xPoint,
	uint32_t& uPolyOut, Zenith_Maths::Vector3& xNearestOut, float fMaxDist) const
{
	if (m_axPolygons.GetSize() == 0)
	{
		return false;
	}

	float fMinDistSq = fMaxDist * fMaxDist;
	uPolyOut = UINT32_MAX;

	// Get candidate polygons from spatial grid
	int32_t iCenterX, iCenterZ;
	GetGridCoords(xPoint, iCenterX, iCenterZ);

	int32_t iSearchRadius = static_cast<int32_t>(std::ceil(fMaxDist / m_fGridCellSize));

	// Search expanding rings
	for (int32_t iRing = 0; iRing <= iSearchRadius; ++iRing)
	{
		for (int32_t iDz = -iRing; iDz <= iRing; ++iDz)
		{
			for (int32_t iDx = -iRing; iDx <= iRing; ++iDx)
			{
				// Only process cells on the ring boundary
				if (std::abs(iDx) != iRing && std::abs(iDz) != iRing)
				{
					continue;
				}

				int32_t iX = iCenterX + iDx;
				int32_t iZ = iCenterZ + iDz;

				if (iX < 0 || iX >= static_cast<int32_t>(m_uGridWidth) ||
					iZ < 0 || iZ >= static_cast<int32_t>(m_uGridHeight))
				{
					continue;
				}

				uint32_t uCellIndex = GetGridCellIndex(iX, iZ);
				FindNearestPolygonInCell(uCellIndex, xPoint, fMinDistSq, uPolyOut, xNearestOut);
			}
		}

		// Early out if we found something in the current ring
		if (uPolyOut != UINT32_MAX)
		{
			break;
		}
	}

	return uPolyOut != UINT32_MAX;
}

bool Zenith_NavMesh::IsPointOnNavMesh(const Zenith_Maths::Vector3& xPoint, float fMaxVerticalDist) const
{
	uint32_t uPoly;
	Zenith_Maths::Vector3 xNearest;
	if (!FindNearestPolygon(xPoint, uPoly, xNearest, fMaxVerticalDist * 2.0f))
	{
		return false;
	}

	// Check vertical distance
	float fVerticalDist = std::abs(xPoint.y - xNearest.y);
	return fVerticalDist <= fMaxVerticalDist;
}

uint32_t Zenith_NavMesh::FindPolygonContaining(const Zenith_Maths::Vector3& xPoint, float fMaxVerticalDist) const
{
	if (m_axPolygons.GetSize() == 0)
	{
		return UINT32_MAX;
	}

	// Get candidate polygons from spatial grid
	int32_t iX, iZ;
	GetGridCoords(xPoint, iX, iZ);
	uint32_t uCellIndex = GetGridCellIndex(iX, iZ);

	if (uCellIndex >= m_axGridCells.GetSize())
	{
		return UINT32_MAX;
	}

	const GridCell& xCell = m_axGridCells.Get(uCellIndex);

	for (uint32_t u = 0; u < xCell.m_axPolygonIndices.GetSize(); ++u)
	{
		uint32_t uPoly = xCell.m_axPolygonIndices.Get(u);
		const Zenith_NavMeshPolygon& xPoly = m_axPolygons.Get(uPoly);

		// Check vertical distance to polygon plane
		float fVertDist = std::abs(Zenith_Maths::Dot(xPoint - xPoly.m_xCenter, xPoly.m_xNormal));
		if (fVertDist > fMaxVerticalDist)
		{
			continue;
		}

		// Check if point is inside polygon
		if (xPoly.ContainsPoint(xPoint, m_axVertices))
		{
			return uPoly;
		}
	}

	return UINT32_MAX;
}

void Zenith_NavMesh::SetPolygonBlocked(uint32_t uPoly, bool bBlocked) const
{
	// Hot-update the flag field. We intentionally cast away const-ness: the
	// mesh's topology (verts, polys, adjacency, spatial grid) is invariant;
	// only the dynamic-obstacle flag toggles. Callers that hand out
	// `const Zenith_NavMesh*` (e.g., DP_AI::GetOrBuildLevelNavMesh) can still
	// invoke this without forcing every consumer to take a mutable handle.
	if (uPoly >= m_axPolygons.GetSize()) return;
	Zenith_NavMeshPolygon& xPoly =
		const_cast<Zenith_NavMeshPolygon&>(m_axPolygons.Get(uPoly));
	if (bBlocked) xPoly.m_uFlags |=  Zenith_NavMeshPolygon::FLAG_BLOCKED;
	else          xPoly.m_uFlags &= ~Zenith_NavMeshPolygon::FLAG_BLOCKED;
}

bool Zenith_NavMesh::StitchPortalAt(const Zenith_Maths::Vector3& xPoint,
	const Zenith_Maths::Vector3& xAxis,
	float fProbeDistance,
	float fMaxVerticalDist)
{
	// Probe each side of the door point along ±xAxis. We want polygons in
	// the two adjacent rooms that the door physically separates -- those
	// polygons exist in the navmesh (the rooms are walkable) but the
	// generator didn't link them as neighbours because the wall section
	// between them stayed as obstruction spans, and the doorway gap polys
	// (if any were emitted) ended up in their own tiny island.
	if (m_axPolygons.GetSize() == 0) return false;

	const float fAxisLen = Zenith_Maths::Length(xAxis);
	if (fAxisLen < 0.0001f) return false;
	const Zenith_Maths::Vector3 xUnit = xAxis / fAxisLen;

	const Zenith_Maths::Vector3 xProbePos = xPoint + xUnit * fProbeDistance;
	const Zenith_Maths::Vector3 xProbeNeg = xPoint - xUnit * fProbeDistance;

	// FindNearestPolygon (vs FindPolygonContaining): the probe point can
	// fall in a navmesh "hole" (e.g., over a wall footprint that was
	// carved out). We want the nearest polygon -- the room's interior --
	// not strict containment. Range 2m so we don't accidentally bridge to
	// a far-away polygon on the wrong side of another wall.
	uint32_t uPolyA = UINT32_MAX, uPolyB = UINT32_MAX;
	Zenith_Maths::Vector3 xNearestA, xNearestB;
	const bool bFoundA = FindNearestPolygon(xProbePos, uPolyA, xNearestA, /*fMaxDist=*/2.0f);
	const bool bFoundB = FindNearestPolygon(xProbeNeg, uPolyB, xNearestB, /*fMaxDist=*/2.0f);
	(void)fMaxVerticalDist;
	if (!bFoundA || !bFoundB) return false;
	if (uPolyA == uPolyB) return false; // already same polygon -- no portal needed

	// Check if they're already linked.
	const Zenith_NavMeshPolygon& xPolyA = m_axPolygons.Get(uPolyA);
	for (uint32_t u = 0; u < xPolyA.m_axNeighborIndices.GetSize(); ++u)
	{
		if (xPolyA.m_axNeighborIndices.Get(u) == static_cast<int32_t>(uPolyB))
		{
			return false; // already neighbours
		}
	}

	// Append a phantom neighbour slot to each polygon's neighbour list.
	// The neighbour list is normally sized 1-per-edge by ComputeAdjacency,
	// so all edge slots are taken on interior polygons that have natural
	// neighbours on every side. Pushing an EXTRA slot beyond the
	// vertex-count is safe by construction:
	//
	// * A* (Zenith_Pathfinding::FindPathInternal) iterates the FULL
	//   m_axNeighborIndices list, so the phantom neighbour is visited.
	// * GetPortal (used by GetPortalMidpoint) only scans neighbour slots
	//   indexed BY EDGE (i.e., u < m_axVertexIndices.GetSize()), so the
	//   phantom is invisible to it -- and GetPortalMidpoint then falls
	//   back to averaging polygon centers. That fallback is correct for
	//   a phantom portal: there is no real shared edge between the two
	//   rooms, so "midpoint between rooms" is the right interpolation.
	// * SmoothPath's SegmentExitsNavMesh probe still gates the shortcut
	//   correctly -- the smoothed line from polyA's centre to polyB's
	//   centre passes through the door's position, where the navmesh
	//   has walkable polygons (the doorway gap cells the generator
	//   emitted with the door collider excluded), so the probe finds
	//   them and doesn't reject the shortcut.
	Zenith_NavMeshPolygon& xMutA = m_axPolygons.Get(uPolyA);
	Zenith_NavMeshPolygon& xMutB = m_axPolygons.Get(uPolyB);
	xMutA.m_axNeighborIndices.PushBack(static_cast<int32_t>(uPolyB));
	xMutB.m_axNeighborIndices.PushBack(static_cast<int32_t>(uPolyA));
	return true;
}

uint32_t Zenith_NavMesh::SetBlockedAtPoint(const Zenith_Maths::Vector3& xPoint,
	bool bBlocked, float fMaxVerticalDist) const
{
	// Walk the spatial grid cell at xPoint and flip every polygon whose
	// 2D footprint contains the point. A door's pivot typically lands
	// inside exactly one polygon, but a corner pivot could straddle two —
	// flipping all of them keeps the API safe in that case.
	if (m_axPolygons.GetSize() == 0) return 0;
	int32_t iX, iZ;
	GetGridCoords(xPoint, iX, iZ);
	const uint32_t uCellIndex = GetGridCellIndex(iX, iZ);
	if (uCellIndex >= m_axGridCells.GetSize()) return 0;

	uint32_t uToggled = 0;
	const GridCell& xCell = m_axGridCells.Get(uCellIndex);
	for (uint32_t u = 0; u < xCell.m_axPolygonIndices.GetSize(); ++u)
	{
		const uint32_t uPoly = xCell.m_axPolygonIndices.Get(u);
		const Zenith_NavMeshPolygon& xPoly = m_axPolygons.Get(uPoly);
		const float fVertDist =
			std::abs(Zenith_Maths::Dot(xPoint - xPoly.m_xCenter, xPoly.m_xNormal));
		if (fVertDist > fMaxVerticalDist) continue;
		if (!xPoly.ContainsPoint(xPoint, m_axVertices)) continue;
		SetPolygonBlocked(uPoly, bBlocked);
		++uToggled;
	}
	return uToggled;
}

bool Zenith_NavMesh::Raycast(const Zenith_Maths::Vector3& xStart,
	const Zenith_Maths::Vector3& xEnd, Zenith_Maths::Vector3& xHitOut) const
{
	// Simple implementation: check all polygons in cells along the ray
	// A more sophisticated implementation would use DDA traversal

	Zenith_Maths::Vector3 xDir = xEnd - xStart;
	float fLength = Zenith_Maths::Length(xDir);
	if (fLength < 0.0001f)
	{
		return false;
	}
	xDir /= fLength;

	float fMinT = fLength;
	bool bHit = false;

	// Step along the ray
	float fStep = m_fGridCellSize * 0.5f;
	for (float fT = 0.0f; fT < fLength; fT += fStep)
	{
		Zenith_Maths::Vector3 xPos = xStart + xDir * fT;

		int32_t iX, iZ;
		GetGridCoords(xPos, iX, iZ);
		uint32_t uCellIndex = GetGridCellIndex(iX, iZ);

		if (uCellIndex >= m_axGridCells.GetSize())
		{
			continue;
		}

		const GridCell& xCell = m_axGridCells.Get(uCellIndex);

		for (uint32_t u = 0; u < xCell.m_axPolygonIndices.GetSize(); ++u)
		{
			uint32_t uPoly = xCell.m_axPolygonIndices.Get(u);
			const Zenith_NavMeshPolygon& xPoly = m_axPolygons.Get(uPoly);

			// Ray-plane intersection
			float fDenom = Zenith_Maths::Dot(xDir, xPoly.m_xNormal);
			if (std::abs(fDenom) < 0.0001f)
			{
				continue;  // Ray parallel to plane
			}

			float fPlaneT = Zenith_Maths::Dot(xPoly.m_xCenter - xStart, xPoly.m_xNormal) / fDenom;
			if (fPlaneT < 0.0f || fPlaneT >= fMinT)
			{
				continue;
			}

			Zenith_Maths::Vector3 xIntersect = xStart + xDir * fPlaneT;
			if (xPoly.ContainsPoint(xIntersect, m_axVertices))
			{
				fMinT = fPlaneT;
				xHitOut = xIntersect;
				bHit = true;
			}
		}
	}

	return bHit;
}

bool Zenith_NavMesh::ProjectPoint(const Zenith_Maths::Vector3& xPoint,
	Zenith_Maths::Vector3& xProjectedOut, float fMaxDist) const
{
	uint32_t uPoly;
	return FindNearestPolygon(xPoint, uPoly, xProjectedOut, fMaxDist);
}

float Zenith_NavMesh::SampleUnit() const
{
	// xorshift64* (same constants as the graph Random* nodes).
	m_ulSampleRngState ^= m_ulSampleRngState >> 12;
	m_ulSampleRngState ^= m_ulSampleRngState << 25;
	m_ulSampleRngState ^= m_ulSampleRngState >> 27;
	const uint64_t ulValue = m_ulSampleRngState * 0x2545F4914F6CDD1Dull;
	// Top 24 bits -> [0,1). 24 is float's exact-integer width, so every
	// representable step is hit exactly once and the result never reaches 1.0.
	return static_cast<float>(ulValue >> 40) * (1.0f / 16777216.0f);
}

bool Zenith_NavMesh::CollectReachablePolygons(uint32_t uCenterPoly, const Zenith_Maths::Vector3& xCenter,
	float fRadiusSq, Zenith_Vector<uint32_t>& axReachableOut) const
{
	// Visited as parallel array of bools (not a hash set) for speed.
	const uint32_t uPolyCount = m_axPolygons.GetSize();
	Zenith_Vector<bool> axVisited;
	axVisited.Reserve(uPolyCount);
	for (uint32_t i = 0; i < uPolyCount; ++i) axVisited.PushBack(false);

	Zenith_Vector<uint32_t> axQueue;   // BFS frontier

	// 2D AABB-vs-disc test. Returns true when ANY part of the polygon's
	// horizontal footprint sits inside the sphere of `fRadius` around
	// xCenter. The previous "polygon centre inside radius" criterion would
	// reject a polygon that covers the entire sphere if its centre happened
	// to sit outside — exactly the DevilsPlayground case where the
	// synthetic flat navmesh is one 300 m quad centred far from any
	// gameplay-positioned agent. With this test the priest's 15 m
	// patrol radius still finds the one-and-only flat polygon as
	// reachable, sampling continues normally, and the caller's per-sample
	// distance check enforces the actual disc constraint.
	auto fnPolygonOverlapsDisc = [&](uint32_t uPolyIdx) -> bool
	{
		const Zenith_NavMeshPolygon& xQ = m_axPolygons.Get(uPolyIdx);
		Zenith_Maths::Vector3 xMin, xMax;
		ComputePolygonBounds2D(xQ, m_axVertices, xMin, xMax);
		const float fClosestX = std::max(xMin.x, std::min(xCenter.x, xMax.x));
		const float fClosestZ = std::max(xMin.z, std::min(xCenter.z, xMax.z));
		const float fDx = fClosestX - xCenter.x;
		const float fDz = fClosestZ - xCenter.z;
		return (fDx * fDx + fDz * fDz) <= fRadiusSq;
	};

	axQueue.PushBack(uCenterPoly);
	axVisited.Get(uCenterPoly) = true;

	// Soft visit-count cap as a runaway-BFS fallback. Hitting this is a
	// warning, not a hard correctness limit — we just stop expanding the
	// frontier and the caller rejection-samples within whatever we have so far.
	constexpr uint32_t uMAX_BFS_VISITS = 256;
	uint32_t uVisitCount = 0;

	while (axQueue.GetSize() > 0 && uVisitCount < uMAX_BFS_VISITS)
	{
		const uint32_t uPoly = axQueue.Get(0);
		// Pop front. Zenith_Vector has no efficient pop_front so use RemoveSwap
		// — order doesn't matter for BFS-as-flood-fill (only completeness does).
		axQueue.RemoveSwap(0);
		++uVisitCount;

		const Zenith_NavMeshPolygon& xPoly = m_axPolygons.Get(uPoly);

		// Reachable iff the polygon's 2D footprint overlaps the disc AND it
		// isn't currently flagged as a dynamic blocker. A closed door's
		// polygon (FLAG_BLOCKED) must not contribute a candidate sample —
		// the priest's patrol target selection would otherwise drop a
		// MoveTo destination on top of the blocker, and MoveTo would either
		// fail at the endpoint-blocked check or walk the priest into the
		// door collider.
		if (!xPoly.IsBlocked() && fnPolygonOverlapsDisc(uPoly))
		{
			axReachableOut.PushBack(uPoly);
		}

		// Expand to neighbours whose footprint also overlaps the disc. The
		// previous "neighbour centre inside radius" criterion silently
		// pruned the frontier on coarse meshes — same fix applies. Blocked
		// neighbours stay OUT of the frontier (same rationale as the
		// reachable-set filter above).
		const uint32_t uNeighbourCount = xPoly.m_axNeighborIndices.GetSize();
		for (uint32_t i = 0; i < uNeighbourCount; ++i)
		{
			const int32_t iNeighbour = xPoly.m_axNeighborIndices.Get(i);
			if (iNeighbour < 0) continue;
			const uint32_t uNeighbour = static_cast<uint32_t>(iNeighbour);
			if (uNeighbour >= uPolyCount) continue;
			if (axVisited.Get(uNeighbour)) continue;

			if (m_axPolygons.Get(uNeighbour).IsBlocked()) continue;
			if (!fnPolygonOverlapsDisc(uNeighbour)) continue;

			axVisited.Get(uNeighbour) = true;
			axQueue.PushBack(uNeighbour);
		}
	}

	if (uVisitCount >= uMAX_BFS_VISITS)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"NavMesh::GetRandomReachablePointInRadius hit BFS visit cap (%u). "
			"Sampling within partial reachable set.", uMAX_BFS_VISITS);
		return false;
	}
	return true;
}

float Zenith_NavMesh::BuildAreaCDF(const Zenith_Vector<uint32_t>& axReachable,
	Zenith_Vector<float>& afCumulativeAreaOut) const
{
	afCumulativeAreaOut.Reserve(axReachable.GetSize());
	float fTotalArea = 0.0f;
	for (uint32_t i = 0; i < axReachable.GetSize(); ++i)
	{
		const uint32_t uIdx = axReachable.Get(i);
		const float fArea = m_axPolygons.Get(uIdx).m_fArea;
		// Guard zero-area polys (degenerate triangles) — skip from sampling
		// by treating them as zero weight, which they already are.
		fTotalArea += fArea;
		afCumulativeAreaOut.PushBack(fTotalArea);
	}
	return fTotalArea;
}

bool Zenith_NavMesh::SampleUniformPointInPolygon(const Zenith_NavMeshPolygon& xPoly,
	Zenith_Maths::Vector3& xOut) const
{
	const uint32_t uVerts = xPoly.m_axVertexIndices.GetSize();
	if (uVerts < 3) return false;

	// Fan-triangulate around vertex 0; weight per-triangle area.
	const Zenith_Maths::Vector3& xV0 = m_axVertices.Get(xPoly.m_axVertexIndices.Get(0));
	Zenith_Vector<float> afTriCumArea;
	afTriCumArea.Reserve(uVerts - 2);
	float fTriTotal = 0.0f;
	for (uint32_t t = 1; t + 1 < uVerts; ++t)
	{
		const Zenith_Maths::Vector3& xVa = m_axVertices.Get(xPoly.m_axVertexIndices.Get(t));
		const Zenith_Maths::Vector3& xVb = m_axVertices.Get(xPoly.m_axVertexIndices.Get(t + 1));
		const Zenith_Maths::Vector3 xCross = glm::cross(xVa - xV0, xVb - xV0);
		const float fTriArea = 0.5f * glm::length(xCross);
		fTriTotal += fTriArea;
		afTriCumArea.PushBack(fTriTotal);
	}

	if (fTriTotal <= 0.0f) return false;

	// Pick a triangle, weighted by area.
	const uint32_t uTriIdx = PickWeightedIndex(afTriCumArea, SampleUnit() * fTriTotal);

	// Uniform barycentric sample inside the triangle.
	const Zenith_Maths::Vector3& xVa = m_axVertices.Get(xPoly.m_axVertexIndices.Get(uTriIdx + 1));
	const Zenith_Maths::Vector3& xVb = m_axVertices.Get(xPoly.m_axVertexIndices.Get(uTriIdx + 2));
	float fU = SampleUnit();
	float fV = SampleUnit();
	if (fU + fV > 1.0f)
	{
		// Fold back into the triangle (Turk's barycentric trick).
		fU = 1.0f - fU;
		fV = 1.0f - fV;
	}
	const float fW = 1.0f - fU - fV;
	xOut = fW * xV0 + fU * xVa + fV * xVb;
	return true;
}

bool Zenith_NavMesh::GetRandomReachablePointInRadius(const Zenith_Maths::Vector3& xCenter,
	float fRadius,
	Zenith_Maths::Vector3& xOutPoint,
	uint32_t uMaxAttempts) const
{
	if (fRadius <= 0.0f) return false;
	if (m_axPolygons.GetSize() == 0) return false;

	// ---- Phase 1: locate the source polygon ---------------------------------
	// Use a search distance comfortably larger than the requested radius so a
	// caller standing slightly off-mesh still finds a starting island.
	uint32_t uCenterPoly = UINT32_MAX;
	Zenith_Maths::Vector3 xNearestOnMesh;
	const float fLocateMaxDist = fRadius + 5.0f;
	if (!FindNearestPolygon(xCenter, uCenterPoly, xNearestOnMesh, fLocateMaxDist))
	{
		return false;
	}

	// ---- Phase 2: BFS over polygon adjacency, bounded by horizontal radius --
	const float fRadiusSq = fRadius * fRadius;
	Zenith_Vector<uint32_t> axReachable;   // polygons reachable within fRadius
	CollectReachablePolygons(uCenterPoly, xCenter, fRadiusSq, axReachable);

	if (axReachable.GetSize() == 0)
	{
		// Center polygon is itself outside fRadius (caller is far from any
		// reachable polygon). Bail.
		return false;
	}

	// ---- Phase 3: build cumulative area weights for polygon selection ------
	Zenith_Vector<float> afCumulativeArea;
	const float fTotalArea = BuildAreaCDF(axReachable, afCumulativeArea);
	if (fTotalArea <= 0.0f) return false;

	// ---- Phase 4: rejection sampling --------------------------------------
	auto fnHorizontalDistSq = [](const Zenith_Maths::Vector3& a, const Zenith_Maths::Vector3& b)
	{
		const float fDx = a.x - b.x;
		const float fDz = a.z - b.z;
		return fDx * fDx + fDz * fDz;
	};

	for (uint32_t uAttempt = 0; uAttempt < uMaxAttempts; ++uAttempt)
	{
		// Pick a polygon weighted by area.
		const uint32_t uPickedPolyArrayIdx = PickWeightedIndex(afCumulativeArea, SampleUnit() * fTotalArea);
		const uint32_t uPolyIdx = axReachable.Get(uPickedPolyArrayIdx);
		const Zenith_NavMeshPolygon& xPoly = m_axPolygons.Get(uPolyIdx);

		Zenith_Maths::Vector3 xCandidate;
		if (!SampleUniformPointInPolygon(xPoly, xCandidate)) continue;

		// Snap to the navmesh surface.
		Zenith_Maths::Vector3 xSnapped;
		if (!ProjectPoint(xCandidate, xSnapped, 5.0f)) continue;

		// Final radius check (the snap may have moved the point off the
		// triangle and outside fRadius — verify before returning).
		if (fnHorizontalDistSq(xSnapped, xCenter) > fRadiusSq) continue;

		xOutPoint = xSnapped;
		return true;
	}

	return false;
}

void Zenith_NavMesh::WriteToDataStream(Zenith_DataStream& xStream) const
{
	// Write magic header
	const char* szMagic = "ZNAV";
	xStream.Write(szMagic, 4);

	// Write version
	uint32_t uVersion = 1;
	xStream << uVersion;

	// Write vertices
	uint32_t uVertexCount = m_axVertices.GetSize();
	xStream << uVertexCount;
	for (uint32_t u = 0; u < uVertexCount; ++u)
	{
		xStream << m_axVertices.Get(u).x;
		xStream << m_axVertices.Get(u).y;
		xStream << m_axVertices.Get(u).z;
	}

	// Write polygons
	uint32_t uPolyCount = m_axPolygons.GetSize();
	xStream << uPolyCount;
	for (uint32_t u = 0; u < uPolyCount; ++u)
	{
		m_axPolygons.Get(u).WriteToDataStream(xStream);
	}

	// Write bounds
	xStream << m_xBoundsMin.x;
	xStream << m_xBoundsMin.y;
	xStream << m_xBoundsMin.z;
	xStream << m_xBoundsMax.x;
	xStream << m_xBoundsMax.y;
	xStream << m_xBoundsMax.z;
}

bool Zenith_NavMesh::ReadHeaderFromDataStream(Zenith_DataStream& xStream)
{
	// magic(4) + version(4).
	if (NavMeshBytesRemaining(xStream) < 8ull)
	{
		Zenith_Assert(false, "NavMesh load: stream too short to hold a ZNAV header");
		Zenith_Error(LOG_CATEGORY_AI, "NavMesh load: stream too short to hold a ZNAV header (%llu bytes left)",
			NavMeshBytesRemaining(xStream));
		return false;
	}

	char szMagic[5] = {};
	xStream.Read(szMagic, 4);
	if (strncmp(szMagic, "ZNAV", 4) != 0)
	{
		Zenith_Assert(false, "NavMesh load: bad magic '%s' (expected ZNAV)", szMagic);
		Zenith_Error(LOG_CATEGORY_AI, "NavMesh load: bad magic '%s' (expected ZNAV)", szMagic);
		return false;
	}

	uint32_t uVersion = 0;
	xStream >> uVersion;
	if (uVersion != uZENITH_NAVMESH_WIRE_VERSION)
	{
		Zenith_Assert(false, "NavMesh load: unsupported wire version %u (this build writes %u)",
			uVersion, uZENITH_NAVMESH_WIRE_VERSION);
		Zenith_Error(LOG_CATEGORY_AI, "NavMesh load: unsupported wire version %u (this build writes %u)",
			uVersion, uZENITH_NAVMESH_WIRE_VERSION);
		return false;
	}

	return true;
}

bool Zenith_NavMesh::ReadVerticesFromDataStream(Zenith_DataStream& xStream)
{
	if (NavMeshBytesRemaining(xStream) < sizeof(uint32_t))
	{
		Zenith_Assert(false, "NavMesh load: truncated before the vertex count");
		Zenith_Error(LOG_CATEGORY_AI, "NavMesh load: truncated before the vertex count");
		return false;
	}

	uint32_t uVertexCount = 0;
	xStream >> uVertexCount;

	// Plausibility BEFORE Reserve: a corrupt count must never drive an allocation.
	if (static_cast<uint64_t>(uVertexCount) * ulNAVMESH_VERTEX_BYTES > NavMeshBytesRemaining(xStream))
	{
		Zenith_Assert(false, "NavMesh load: vertex count %u needs more bytes than the stream holds", uVertexCount);
		Zenith_Error(LOG_CATEGORY_AI, "NavMesh load: vertex count %u needs more bytes than the stream holds (%llu left)",
			uVertexCount, NavMeshBytesRemaining(xStream));
		return false;
	}

	m_axVertices.Reserve(uVertexCount);
	for (uint32_t u = 0; u < uVertexCount; ++u)
	{
		Zenith_Maths::Vector3 xVert;
		xStream >> xVert.x;
		xStream >> xVert.y;
		xStream >> xVert.z;
		if (!NavMeshIsFiniteVector(xVert))
		{
			// A NaN vertex silently poisons bounds, the spatial grid and every
			// distance comparison downstream.
			Zenith_Assert(false, "NavMesh load: vertex %u is not finite", u);
			Zenith_Error(LOG_CATEGORY_AI, "NavMesh load: vertex %u is not finite", u);
			return false;
		}
		m_axVertices.PushBack(xVert);
	}

	return true;
}

bool Zenith_NavMesh::ReadPolygonsFromDataStream(Zenith_DataStream& xStream)
{
	if (NavMeshBytesRemaining(xStream) < sizeof(uint32_t))
	{
		Zenith_Assert(false, "NavMesh load: truncated before the polygon count");
		Zenith_Error(LOG_CATEGORY_AI, "NavMesh load: truncated before the polygon count");
		return false;
	}

	uint32_t uPolyCount = 0;
	xStream >> uPolyCount;

	if (static_cast<uint64_t>(uPolyCount) * ulNAVMESH_MIN_POLYGON_BYTES > NavMeshBytesRemaining(xStream))
	{
		Zenith_Assert(false, "NavMesh load: polygon count %u needs more bytes than the stream holds", uPolyCount);
		Zenith_Error(LOG_CATEGORY_AI, "NavMesh load: polygon count %u needs more bytes than the stream holds (%llu left)",
			uPolyCount, NavMeshBytesRemaining(xStream));
		return false;
	}

	m_axPolygons.Reserve(uPolyCount);
	for (uint32_t u = 0; u < uPolyCount; ++u)
	{
		Zenith_NavMeshPolygon xPoly;
		// The polygon read asserts + logs the precise violation itself.
		if (!xPoly.ReadFromDataStream(xStream, m_axVertices.GetSize(), uPolyCount))
		{
			return false;
		}
		m_axPolygons.PushBack(std::move(xPoly));
	}

	return true;
}

bool Zenith_NavMesh::ReadBoundsFromDataStream(Zenith_DataStream& xStream)
{
	constexpr uint64_t ulBOUNDS_BYTES = 6ull * sizeof(float);
	if (NavMeshBytesRemaining(xStream) < ulBOUNDS_BYTES)
	{
		Zenith_Assert(false, "NavMesh load: truncated inside the mesh bounds");
		Zenith_Error(LOG_CATEGORY_AI, "NavMesh load: truncated inside the mesh bounds");
		return false;
	}

	xStream >> m_xBoundsMin.x;
	xStream >> m_xBoundsMin.y;
	xStream >> m_xBoundsMin.z;
	xStream >> m_xBoundsMax.x;
	xStream >> m_xBoundsMax.y;
	xStream >> m_xBoundsMax.z;

	// Validated even though ComputeSpatialData normally overwrites them: it
	// early-outs on a zero-vertex mesh, and BuildSpatialGrid early-outs on a
	// zero-polygon mesh, so for a well-formed EMPTY mesh these values do stand.
	if (!NavMeshIsFiniteVector(m_xBoundsMin) || !NavMeshIsFiniteVector(m_xBoundsMax))
	{
		Zenith_Assert(false, "NavMesh load: mesh bounds are not finite");
		Zenith_Error(LOG_CATEGORY_AI, "NavMesh load: mesh bounds are not finite");
		return false;
	}

	return true;
}

bool Zenith_NavMesh::ReadFromDataStream(Zenith_DataStream& xStream)
{
	Clear();

	if (ReadHeaderFromDataStream(xStream) &&
		ReadVerticesFromDataStream(xStream) &&
		ReadPolygonsFromDataStream(xStream) &&
		ReadBoundsFromDataStream(xStream))
	{
		// Recomputes bounds + every polygon's center/normal/area from the
		// vertices, then bins the polygons for point queries.
		BuildSpatialGrid();
		return true;
	}

	// DEFINED failure state: an empty mesh, never a half-populated one.
	Clear();
	return false;
}

Zenith_NavMesh* Zenith_NavMesh::LoadFromFile(const std::string& strPath)
{
	if (strPath.empty())
	{
		Zenith_Assert(false, "NavMesh load: empty path");
		Zenith_Error(LOG_CATEGORY_AI, "NavMesh load: empty path");
		return nullptr;
	}

	// Checked ahead of ReadFromFile so a missing asset reports THAT, rather than
	// tripping Zenith_DataStream's generic read assert with no navmesh context.
	if (!Zenith_FileAccess::FileExists(strPath.c_str()))
	{
		Zenith_Assert(false, "NavMesh load: file does not exist: %s", strPath.c_str());
		Zenith_Error(LOG_CATEGORY_AI, "NavMesh load: file does not exist: %s", strPath.c_str());
		return nullptr;
	}

	Zenith_DataStream xStream;
	xStream.ReadFromFile(strPath.c_str());
	if (!xStream.IsValid())
	{
		Zenith_Assert(false, "NavMesh load: unreadable file: %s", strPath.c_str());
		Zenith_Error(LOG_CATEGORY_AI, "NavMesh load: unreadable file: %s", strPath.c_str());
		return nullptr;
	}

	Zenith_NavMesh* pxNavMesh = new Zenith_NavMesh();
	if (!pxNavMesh->ReadFromDataStream(xStream))
	{
		// The read already asserted + logged the precise violation. Never hand a
		// partially-parsed mesh back to the caller.
		delete pxNavMesh;
		return nullptr;
	}

	Zenith_Log(LOG_CATEGORY_AI, "Loaded navmesh: %s (%u vertices, %u polygons)",
		strPath.c_str(), pxNavMesh->GetVertexCount(), pxNavMesh->GetPolygonCount());
	return pxNavMesh;
}

bool Zenith_NavMesh::SaveToFile(const std::string& strPath) const
{
	if (strPath.empty())
	{
		Zenith_Assert(false, "NavMesh save: empty path");
		Zenith_Error(LOG_CATEGORY_AI, "NavMesh save: empty path");
		return false;
	}

	Zenith_DataStream xStream;
	WriteToDataStream(xStream);

	xStream.WriteToFile(strPath.c_str());

	// Zenith_FileAccess::WriteFile returns void, so the file being back on disk
	// is the only truthful success signal available here.
	if (!Zenith_FileAccess::FileExists(strPath.c_str()))
	{
		Zenith_Assert(false, "NavMesh save: file absent after write: %s", strPath.c_str());
		Zenith_Error(LOG_CATEGORY_AI, "NavMesh save: file absent after write: %s", strPath.c_str());
		return false;
	}

	Zenith_Log(LOG_CATEGORY_AI, "Saved navmesh: %s (%u vertices, %u polygons)",
		strPath.c_str(), m_axVertices.GetSize(), m_axPolygons.GetSize());
	return true;
}

#ifdef ZENITH_TOOLS
void Zenith_NavMesh::DebugDrawEdges(const Zenith_NavMeshPolygon& xPoly, const Zenith_Maths::Vector3& xOffset,
	const Zenith_Maths::Vector3& xEdgeColor) const
{
	for (uint32_t u = 0; u < xPoly.m_axVertexIndices.GetSize(); ++u)
	{
		const Zenith_Maths::Vector3& xV1 = m_axVertices.Get(xPoly.m_axVertexIndices.Get(u));
		const Zenith_Maths::Vector3& xV2 = m_axVertices.Get(xPoly.m_axVertexIndices.Get((u + 1) % xPoly.m_axVertexIndices.GetSize()));

		Zenith_AI_DebugDrawLine(xV1 + xOffset, xV2 + xOffset, xEdgeColor, 0.02f);
	}
}

void Zenith_NavMesh::DebugDrawBoundaryEdges(const Zenith_NavMeshPolygon& xPoly, const Zenith_Maths::Vector3& xOffset,
	const Zenith_Maths::Vector3& xBoundaryColor) const
{
	for (uint32_t u = 0; u < xPoly.m_axVertexIndices.GetSize(); ++u)
	{
		// Check if THIS specific edge has a neighbor (check per-edge, not per-polygon)
		bool bEdgeHasNeighbor = false;
		if (u < xPoly.m_axNeighborIndices.GetSize())
		{
			const int32_t iNeighborIdx = xPoly.m_axNeighborIndices.Get(u);
			bEdgeHasNeighbor = (iNeighborIdx >= 0 && iNeighborIdx != iZENITH_NAVMESH_NO_NEIGHBOUR);
		}

		// Draw boundary edge if no neighbor on this edge
		if (!bEdgeHasNeighbor)
		{
			const Zenith_Maths::Vector3& xV1 = m_axVertices.Get(xPoly.m_axVertexIndices.Get(u));
			const Zenith_Maths::Vector3& xV2 = m_axVertices.Get(xPoly.m_axVertexIndices.Get((u + 1) % xPoly.m_axVertexIndices.GetSize()));
			Zenith_AI_DebugDrawLine(xV1 + xOffset, xV2 + xOffset, xBoundaryColor, 0.04f);
		}
	}
}

void Zenith_NavMesh::DebugDrawPolygonFill(const Zenith_NavMeshPolygon& xPoly, const Zenith_Maths::Vector3& xOffset,
	const Zenith_Maths::Vector3& xWalkableColor) const
{
	// Triangulate the polygon using fan triangulation from first vertex
	// Works for convex polygons (which NavMesh polygons should be)
	if (xPoly.m_axVertexIndices.GetSize() >= 3)
	{
		const Zenith_Maths::Vector3& xV0 = m_axVertices.Get(xPoly.m_axVertexIndices.Get(0)) + xOffset;

		for (uint32_t v = 1; v + 1 < xPoly.m_axVertexIndices.GetSize(); ++v)
		{
			const Zenith_Maths::Vector3& xV1 = m_axVertices.Get(xPoly.m_axVertexIndices.Get(v)) + xOffset;
			const Zenith_Maths::Vector3& xV2 = m_axVertices.Get(xPoly.m_axVertexIndices.Get(v + 1)) + xOffset;

			Zenith_AI_DebugDrawTriangle(xV0, xV1, xV2, xWalkableColor);
		}
	}
}

void Zenith_NavMesh::DebugDrawNeighborConnections(uint32_t uPoly, const Zenith_NavMeshPolygon& xPoly,
	const Zenith_Maths::Vector3& xOffset, const Zenith_Maths::Vector3& xNeighborColor) const
{
	for (uint32_t n = 0; n < xPoly.m_axNeighborIndices.GetSize(); ++n)
	{
		const int32_t iNeighborIdx = xPoly.m_axNeighborIndices.Get(n);
		if (iNeighborIdx != iZENITH_NAVMESH_NO_NEIGHBOUR && iNeighborIdx >= 0 &&
			static_cast<uint32_t>(iNeighborIdx) < m_axPolygons.GetSize())
		{
			const uint32_t uNeighborIdx = static_cast<uint32_t>(iNeighborIdx);
			// Only draw if this poly index is less than neighbor to avoid duplicates
			if (uPoly < uNeighborIdx)
			{
				const Zenith_NavMeshPolygon& xNeighbor = m_axPolygons.Get(uNeighborIdx);
				Zenith_AI_DebugDrawLine(
					xPoly.m_xCenter + xOffset,
					xNeighbor.m_xCenter + xNeighbor.m_xNormal * 0.05f,
					xNeighborColor, 0.01f);
			}
		}
	}
}

void Zenith_NavMesh::DebugDrawCenterAndNormal(const Zenith_NavMeshPolygon& xPoly,
	const Zenith_Maths::Vector3& xOffset, const Zenith_Maths::Vector3& xCenterColor) const
{
	const Zenith_Maths::Vector3 xCenter = xPoly.m_xCenter + xOffset;
	Zenith_AI_DebugDrawCross(xCenter, 0.15f, xCenterColor);
	Zenith_AI_DebugDrawLine(xCenter, xCenter + xPoly.m_xNormal * 0.5f, xCenterColor, 0.015f);
}

void Zenith_NavMesh::DebugDraw(const Zenith_NavMeshDebugDrawFlags& xFlags) const
{
	const Zenith_Maths::Vector3 xWalkableColor(0.2f, 0.8f, 0.2f);
	const Zenith_Maths::Vector3 xEdgeColor(0.1f, 0.5f, 0.1f);
	const Zenith_Maths::Vector3 xBoundaryColor(0.8f, 0.2f, 0.2f);
	const Zenith_Maths::Vector3 xNeighborColor(0.2f, 0.5f, 0.8f);
	const Zenith_Maths::Vector3 xCenterColor(0.9f, 0.9f, 0.2f);
	const Zenith_Maths::Vector3 xBlockedColor(0.9f, 0.35f, 0.05f);

	for (uint32_t uPoly = 0; uPoly < m_axPolygons.GetSize(); ++uPoly)
	{
		const Zenith_NavMeshPolygon& xPoly = m_axPolygons.Get(uPoly);

		// Lift along the polygon's own normal so the visualisation clears the
		// surface it was voxelised from instead of z-fighting it.
		const Zenith_Maths::Vector3 xOffset = xPoly.m_xNormal * xFlags.m_fSurfaceOffset;

		if (xFlags.m_bEdges)
		{
			DebugDrawEdges(xPoly, xOffset, xEdgeColor);
		}

		if (xFlags.m_bBoundaryEdges)
		{
			DebugDrawBoundaryEdges(xPoly, xOffset, xBoundaryColor);
		}

		// A blocked polygon is filled in the warning colour whether or not the
		// walkable fill is on, so a dynamic obstacle is visible at a glance.
		const bool bBlocked = xPoly.IsBlocked();
		if (xFlags.m_bHighlightBlocked && bBlocked)
		{
			DebugDrawPolygonFill(xPoly, xOffset, xBlockedColor);
		}
		else if (xFlags.m_bFilled)
		{
			DebugDrawPolygonFill(xPoly, xOffset, xWalkableColor);
		}

		if (xFlags.m_bAdjacencyLinks)
		{
			DebugDrawNeighborConnections(uPoly, xPoly, xOffset, xNeighborColor);
		}

		if (xFlags.m_bCentersAndNormals)
		{
			DebugDrawCenterAndNormal(xPoly, xOffset, xCenterColor);
		}
	}
}
#endif
