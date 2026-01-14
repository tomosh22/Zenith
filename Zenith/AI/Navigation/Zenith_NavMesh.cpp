#include "Zenith.h"
#include "AI/Navigation/Zenith_NavMesh.h"
#include "AI/Zenith_AIDebugVariables.h"
#include "DataStream/Zenith_DataStream.h"
#include "FileAccess/Zenith_FileAccess.h"

#ifdef ZENITH_TOOLS
#include "Flux/Primitives/Flux_Primitives.h"
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
		Zenith_Maths::Vector3 xPolyMin = m_axVertices.Get(xPoly.m_axVertexIndices.Get(0));
		Zenith_Maths::Vector3 xPolyMax = xPolyMin;

		for (uint32_t u = 1; u < xPoly.m_axVertexIndices.GetSize(); ++u)
		{
			const Zenith_Maths::Vector3& xV = m_axVertices.Get(xPoly.m_axVertexIndices.Get(u));
			xPolyMin.x = std::min(xPolyMin.x, xV.x);
			xPolyMin.z = std::min(xPolyMin.z, xV.z);
			xPolyMax.x = std::max(xPolyMax.x, xV.x);
			xPolyMax.z = std::max(xPolyMax.z, xV.z);
		}

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

		// Draw edges (controlled by s_bDrawNavMeshEdges)
		if (Zenith_AIDebugVariables::s_bDrawNavMeshEdges)
		{
			for (uint32_t u = 0; u < xPoly.m_axVertexIndices.GetSize(); ++u)
			{
				const Zenith_Maths::Vector3& xV1 = m_axVertices.Get(xPoly.m_axVertexIndices.Get(u));
				const Zenith_Maths::Vector3& xV2 = m_axVertices.Get(xPoly.m_axVertexIndices.Get((u + 1) % xPoly.m_axVertexIndices.GetSize()));

				Flux_Primitives::AddLine(xV1 + xOffset, xV2 + xOffset, xEdgeColor, 0.02f);
			}
		}

		// Draw boundary edges (edges with no neighbor) in red
		if (Zenith_AIDebugVariables::s_bDrawNavMeshBoundary)
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
					Flux_Primitives::AddLine(xV1 + xOffset, xV2 + xOffset, xBoundaryColor, 0.04f);
				}
			}
		}

		// Draw polygon fill as triangles (controlled by s_bDrawNavMeshPolygons)
		if (Zenith_AIDebugVariables::s_bDrawNavMeshPolygons)
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

					Flux_Primitives::AddTriangle(xV0, xV1, xV2, xWalkableColor);
				}
			}
		}

		// Draw neighbor connections (controlled by s_bDrawNavMeshNeighbors)
		if (Zenith_AIDebugVariables::s_bDrawNavMeshNeighbors)
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
						Flux_Primitives::AddLine(
							xPoly.m_xCenter + xOffset,
							xNeighbor.m_xCenter + xNeighbor.m_xNormal * 0.05f,
							xNeighborColor, 0.01f);
					}
				}
			}
		}
	}
}
#endif
