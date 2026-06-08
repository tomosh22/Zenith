#include "Zenith.h"
#include "Core/Zenith_Engine.h"
#include "AI/Navigation/Zenith_NavMesh.h"
#include "AI/Zenith_AIDebugVariables.h"

#include <random>

#ifdef ZENITH_TOOLS
#include "Flux/Primitives/Flux_PrimitivesImpl.h"
#endif

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

void Zenith_NavMeshPolygon::ReadFromDataStream(Zenith_DataStream& xStream)
{
	// Read vertex indices
	uint32_t uVertCount = 0;
	xStream >> uVertCount;
	m_axVertexIndices.Clear();
	m_axVertexIndices.Reserve(uVertCount);
	for (uint32_t u = 0; u < uVertCount; ++u)
	{
		uint32_t uIdx = 0;
		xStream >> uIdx;
		m_axVertexIndices.PushBack(uIdx);
	}

	// Read neighbor indices
	uint32_t uNeighborCount = 0;
	xStream >> uNeighborCount;
	m_axNeighborIndices.Clear();
	m_axNeighborIndices.Reserve(uNeighborCount);
	for (uint32_t u = 0; u < uNeighborCount; ++u)
	{
		int32_t iIdx = 0;
		xStream >> iIdx;
		m_axNeighborIndices.PushBack(iIdx);
	}

	// Read spatial data
	xStream >> m_xCenter.x;
	xStream >> m_xCenter.y;
	xStream >> m_xCenter.z;
	xStream >> m_xNormal.x;
	xStream >> m_xNormal.y;
	xStream >> m_xNormal.z;
	xStream >> m_fArea;

	// Read flags and cost
	xStream >> m_uFlags;
	xStream >> m_fCost;
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
	// Visited as parallel array of bools (not a hash set) for speed.
	const uint32_t uPolyCount = m_axPolygons.GetSize();
	Zenith_Vector<bool> axVisited;
	axVisited.Reserve(uPolyCount);
	for (uint32_t i = 0; i < uPolyCount; ++i) axVisited.PushBack(false);

	Zenith_Vector<uint32_t> axReachable;       // polygons reachable within fRadius
	Zenith_Vector<uint32_t> axQueue;            // BFS frontier

	const float fRadiusSq = fRadius * fRadius;
	auto fnHorizontalDistSq = [](const Zenith_Maths::Vector3& a, const Zenith_Maths::Vector3& b)
	{
		const float fDx = a.x - b.x;
		const float fDz = a.z - b.z;
		return fDx * fDx + fDz * fDz;
	};

	// 2D AABB-vs-disc test. Returns true when ANY part of the polygon's
	// horizontal footprint sits inside the sphere of `fRadius` around
	// xCenter. The previous "polygon centre inside radius" criterion would
	// reject a polygon that covers the entire sphere if its centre happened
	// to sit outside — exactly the DevilsPlayground case where the
	// synthetic flat navmesh is one 300 m quad centred far from any
	// gameplay-positioned agent. With this test the priest's 15 m
	// patrol radius still finds the one-and-only flat polygon as
	// reachable, sampling continues normally, and Phase 4's per-sample
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
	// frontier and rejection-sample within whatever we have so far.
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
			axReachable.PushBack(uPoly);
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
	}

	if (axReachable.GetSize() == 0)
	{
		// Center polygon is itself outside fRadius (caller is far from any
		// reachable polygon). Bail.
		return false;
	}

	// ---- Phase 3: build cumulative area weights for polygon selection ------
	Zenith_Vector<float> afCumulativeArea;
	afCumulativeArea.Reserve(axReachable.GetSize());
	float fTotalArea = 0.0f;
	for (uint32_t i = 0; i < axReachable.GetSize(); ++i)
	{
		const uint32_t uIdx = axReachable.Get(i);
		const float fArea = m_axPolygons.Get(uIdx).m_fArea;
		// Guard zero-area polys (degenerate triangles) — skip from sampling
		// by treating them as zero weight, which they already are.
		fTotalArea += fArea;
		afCumulativeArea.PushBack(fTotalArea);
	}

	if (fTotalArea <= 0.0f) return false;

	thread_local std::mt19937 s_xRng(std::random_device{}());
	auto fnSampleUnit = [&]() -> float
	{
		std::uniform_real_distribution<float> xDist(0.0f, 1.0f);
		return xDist(s_xRng);
	};

	// ---- Phase 4: rejection sampling --------------------------------------
	for (uint32_t uAttempt = 0; uAttempt < uMaxAttempts; ++uAttempt)
	{
		// Pick a polygon weighted by area.
		const float fPolyPick = fnSampleUnit() * fTotalArea;
		uint32_t uPickedPolyArrayIdx = 0;
		for (uint32_t i = 0; i < afCumulativeArea.GetSize(); ++i)
		{
			if (fPolyPick <= afCumulativeArea.Get(i))
			{
				uPickedPolyArrayIdx = i;
				break;
			}
		}
		const uint32_t uPolyIdx = axReachable.Get(uPickedPolyArrayIdx);
		const Zenith_NavMeshPolygon& xPoly = m_axPolygons.Get(uPolyIdx);

		const uint32_t uVerts = xPoly.m_axVertexIndices.GetSize();
		if (uVerts < 3) continue;

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

		if (fTriTotal <= 0.0f) continue;

		// Pick a triangle.
		const float fTriPick = fnSampleUnit() * fTriTotal;
		uint32_t uTriIdx = 0;
		for (uint32_t i = 0; i < afTriCumArea.GetSize(); ++i)
		{
			if (fTriPick <= afTriCumArea.Get(i))
			{
				uTriIdx = i;
				break;
			}
		}

		// Uniform barycentric sample inside the triangle.
		const Zenith_Maths::Vector3& xVa = m_axVertices.Get(xPoly.m_axVertexIndices.Get(uTriIdx + 1));
		const Zenith_Maths::Vector3& xVb = m_axVertices.Get(xPoly.m_axVertexIndices.Get(uTriIdx + 2));
		float fU = fnSampleUnit();
		float fV = fnSampleUnit();
		if (fU + fV > 1.0f)
		{
			// Fold back into the triangle (Turk's barycentric trick).
			fU = 1.0f - fU;
			fV = 1.0f - fV;
		}
		const float fW = 1.0f - fU - fV;
		const Zenith_Maths::Vector3 xCandidate = fW * xV0 + fU * xVa + fV * xVb;

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

void Zenith_NavMesh::ReadFromDataStream(Zenith_DataStream& xStream)
{
	Clear();

	// Read and verify magic header
	char szMagic[5] = {};
	xStream.Read(szMagic, 4);
	if (strncmp(szMagic, "ZNAV", 4) != 0)
	{
		Zenith_Log(LOG_CATEGORY_AI, "Invalid navmesh file format");
		return;
	}

	// Read version
	uint32_t uVersion = 0;
	xStream >> uVersion;
	if (uVersion != 1)
	{
		Zenith_Log(LOG_CATEGORY_AI, "Unsupported navmesh version: %u", uVersion);
		return;
	}

	// Read vertices
	uint32_t uVertexCount = 0;
	xStream >> uVertexCount;
	m_axVertices.Clear();
	m_axVertices.Reserve(uVertexCount);
	for (uint32_t u = 0; u < uVertexCount; ++u)
	{
		Zenith_Maths::Vector3 xVert;
		xStream >> xVert.x;
		xStream >> xVert.y;
		xStream >> xVert.z;
		m_axVertices.PushBack(xVert);
	}

	// Read polygons
	uint32_t uPolyCount = 0;
	xStream >> uPolyCount;
	m_axPolygons.Clear();
	m_axPolygons.Reserve(uPolyCount);
	for (uint32_t u = 0; u < uPolyCount; ++u)
	{
		Zenith_NavMeshPolygon xPoly;
		xPoly.ReadFromDataStream(xStream);
		m_axPolygons.PushBack(std::move(xPoly));
	}

	// Read bounds
	xStream >> m_xBoundsMin.x;
	xStream >> m_xBoundsMin.y;
	xStream >> m_xBoundsMin.z;
	xStream >> m_xBoundsMax.x;
	xStream >> m_xBoundsMax.y;
	xStream >> m_xBoundsMax.z;

	// Rebuild spatial grid
	BuildSpatialGrid();
}

Zenith_NavMesh* Zenith_NavMesh::LoadFromFile(const std::string& strPath)
{
	Zenith_DataStream xStream;
	xStream.ReadFromFile(strPath.c_str());
	if (!xStream.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "Failed to load navmesh: %s", strPath.c_str());
		return nullptr;
	}

	Zenith_NavMesh* pxNavMesh = new Zenith_NavMesh();
	pxNavMesh->ReadFromDataStream(xStream);
	return pxNavMesh;
}

bool Zenith_NavMesh::SaveToFile(const std::string& strPath) const
{
	Zenith_DataStream xStream;
	WriteToDataStream(xStream);

	xStream.WriteToFile(strPath.c_str());

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

		g_xEngine.Primitives().AddLine(xV1 + xOffset, xV2 + xOffset, xEdgeColor, 0.02f);
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
			int32_t iNeighborIdx = xPoly.m_axNeighborIndices.Get(u);
			bEdgeHasNeighbor = (iNeighborIdx >= 0 && iNeighborIdx != static_cast<int32_t>(UINT32_MAX));
		}

		// Draw boundary edge if no neighbor on this edge
		if (!bEdgeHasNeighbor)
		{
			const Zenith_Maths::Vector3& xV1 = m_axVertices.Get(xPoly.m_axVertexIndices.Get(u));
			const Zenith_Maths::Vector3& xV2 = m_axVertices.Get(xPoly.m_axVertexIndices.Get((u + 1) % xPoly.m_axVertexIndices.GetSize()));
			g_xEngine.Primitives().AddLine(xV1 + xOffset, xV2 + xOffset, xBoundaryColor, 0.04f);
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

			g_xEngine.Primitives().AddTriangle(xV0, xV1, xV2, xWalkableColor);
		}
	}
}

void Zenith_NavMesh::DebugDrawNeighborConnections(uint32_t uPoly, const Zenith_NavMeshPolygon& xPoly,
	const Zenith_Maths::Vector3& xOffset, const Zenith_Maths::Vector3& xNeighborColor) const
{
	for (uint32_t n = 0; n < xPoly.m_axNeighborIndices.GetSize(); ++n)
	{
		uint32_t uNeighborIdx = xPoly.m_axNeighborIndices.Get(n);
		if (uNeighborIdx != UINT32_MAX && uNeighborIdx < m_axPolygons.GetSize())
		{
			// Only draw if this poly index is less than neighbor to avoid duplicates
			if (uPoly < uNeighborIdx)
			{
				const Zenith_NavMeshPolygon& xNeighbor = m_axPolygons.Get(uNeighborIdx);
				g_xEngine.Primitives().AddLine(
					xPoly.m_xCenter + xOffset,
					xNeighbor.m_xCenter + xNeighbor.m_xNormal * 0.05f,
					xNeighborColor, 0.01f);
			}
		}
	}
}

void Zenith_NavMesh::DebugDraw() const
{
	if (!Zenith_AIDebugVariables::s_bEnableAllAIDebug)
	{
		return;
	}

	const Zenith_Maths::Vector3 xWalkableColor(0.2f, 0.8f, 0.2f);
	const Zenith_Maths::Vector3 xEdgeColor(0.1f, 0.5f, 0.1f);
	const Zenith_Maths::Vector3 xBoundaryColor(0.8f, 0.2f, 0.2f);
	const Zenith_Maths::Vector3 xNeighborColor(0.2f, 0.5f, 0.8f);

	// Draw each polygon
	// Use a small offset to lift visualization above underlying geometry
	// (NavMesh polygons may be slightly below surfaces due to voxelization)
	const float fVisualOffset = 0.15f;

	// Debug: Log sample polygon heights (only once per NavMesh)
	static bool s_bLoggedHeights = false;
	if (!s_bLoggedHeights && m_axPolygons.GetSize() > 0)
	{
		s_bLoggedHeights = true;

		// Find min/max Y of polygon vertices
		float fMinY = FLT_MAX, fMaxY = -FLT_MAX;
		for (uint32_t u = 0; u < m_axVertices.GetSize(); ++u)
		{
			fMinY = std::min(fMinY, m_axVertices.Get(u).y);
			fMaxY = std::max(fMaxY, m_axVertices.Get(u).y);
		}
		Zenith_Log(LOG_CATEGORY_AI, "NavMesh DebugDraw: %u polygons, vertex Y range [%.2f, %.2f], visual offset %.2f",
			m_axPolygons.GetSize(), fMinY, fMaxY, fVisualOffset);

		// Log first floor polygon details
		for (uint32_t uPoly = 0; uPoly < m_axPolygons.GetSize() && uPoly < 3; ++uPoly)
		{
			const Zenith_NavMeshPolygon& xP = m_axPolygons.Get(uPoly);
			Zenith_Log(LOG_CATEGORY_AI, "  Poly %u: center Y=%.2f, normal=(%.2f,%.2f,%.2f), rendered at Y=%.2f",
				uPoly, xP.m_xCenter.y, xP.m_xNormal.x, xP.m_xNormal.y, xP.m_xNormal.z,
				xP.m_xCenter.y + xP.m_xNormal.y * fVisualOffset);
		}
	}

	for (uint32_t uPoly = 0; uPoly < m_axPolygons.GetSize(); ++uPoly)
	{
		const Zenith_NavMeshPolygon& xPoly = m_axPolygons.Get(uPoly);
		Zenith_Maths::Vector3 xOffset = xPoly.m_xNormal * fVisualOffset;

		if (Zenith_AIDebugVariables::s_bDrawNavMeshEdges)
		{
			DebugDrawEdges(xPoly, xOffset, xEdgeColor);
		}

		if (Zenith_AIDebugVariables::s_bDrawNavMeshBoundary)
		{
			DebugDrawBoundaryEdges(xPoly, xOffset, xBoundaryColor);
		}

		if (Zenith_AIDebugVariables::s_bDrawNavMeshPolygons)
		{
			DebugDrawPolygonFill(xPoly, xOffset, xWalkableColor);
		}

		if (Zenith_AIDebugVariables::s_bDrawNavMeshNeighbors)
		{
			DebugDrawNeighborConnections(uPoly, xPoly, xOffset, xNeighborColor);
		}
	}
}
#endif

#ifdef ZENITH_TESTING
#include "AI/Navigation/Zenith_NavMesh.Tests.inl"
#endif
