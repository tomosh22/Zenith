#include "Zenith.h"
#include "AI/Navigation/Zenith_Pathfinding.h"
#include "AI/Navigation/Zenith_NavMesh.h"
#include "Profiling/Zenith_Profiling.h"
#include "TaskSystem/Zenith_TaskSystem.h"
#include <queue>
#include <unordered_set>
#include <algorithm>

Zenith_PathResult Zenith_Pathfinding::FindPath(const Zenith_NavMesh& xNavMesh,
	const Zenith_Maths::Vector3& xStart,
	const Zenith_Maths::Vector3& xEnd)
{
	Zenith_Profiling::Scope xProfileScope(ZENITH_PROFILE_INDEX__AI_PATHFINDING);
	return FindPathInternal(xNavMesh, xStart, xEnd);
}

// Internal implementation without profiling (for batch processing)
Zenith_PathResult Zenith_Pathfinding::FindPathInternal(const Zenith_NavMesh& xNavMesh,
	const Zenith_Maths::Vector3& xStart,
	const Zenith_Maths::Vector3& xEnd)
{
	Zenith_PathResult xResult;
	xResult.m_eStatus = Zenith_PathResult::Status::FAILED;

	if (xNavMesh.GetPolygonCount() == 0)
	{
		Zenith_Log(LOG_CATEGORY_AI, "Pathfinding: NavMesh has 0 polygons");
		return xResult;
	}

	// Find start and end polygons
	uint32_t uStartPoly;
	Zenith_Maths::Vector3 xStartProjected;
	if (!xNavMesh.FindNearestPolygon(xStart, uStartPoly, xStartProjected, 5.0f))
	{
		Zenith_Log(LOG_CATEGORY_AI, "Pathfinding: Start position not on navmesh");
		return xResult;
	}

	uint32_t uEndPoly;
	Zenith_Maths::Vector3 xEndProjected;
	if (!xNavMesh.FindNearestPolygon(xEnd, uEndPoly, xEndProjected, 5.0f))
	{
		Zenith_Log(LOG_CATEGORY_AI, "Pathfinding: End position not on navmesh");
		return xResult;
	}

	// Same polygon - direct path
	if (uStartPoly == uEndPoly)
	{
		xResult.m_eStatus = Zenith_PathResult::Status::SUCCESS;
		xResult.m_axWaypoints.PushBack(xStartProjected);
		xResult.m_axWaypoints.PushBack(xEndProjected);
		xResult.m_fTotalDistance = Zenith_Maths::Length(xEndProjected - xStartProjected);
		return xResult;
	}

	// A* search
	std::priority_queue<AStarNode, std::vector<AStarNode>, std::greater<AStarNode>> xOpenSet;
	std::unordered_set<uint32_t> xClosedSet;
	std::unordered_map<uint32_t, float> xOpenSetGCosts;  // Track best g-cost for nodes in open set
	Zenith_Vector<AStarNode> axClosedList;

	// Initialize with start node
	AStarNode xStartNode;
	xStartNode.m_uPolygonIndex = uStartPoly;
	xStartNode.m_uParentIndex = UINT32_MAX;
	xStartNode.m_fGCost = 0.0f;
	xStartNode.m_fHCost = Zenith_Maths::Length(xEndProjected - xStartProjected);
	xStartNode.m_fFCost = xStartNode.m_fGCost + xStartNode.m_fHCost;
	xOpenSet.push(xStartNode);
	xOpenSetGCosts[uStartPoly] = 0.0f;

	uint32_t uBestPartialPoly = uStartPoly;
	float fBestPartialDist = xStartNode.m_fHCost;

	while (!xOpenSet.empty())
	{
		AStarNode xCurrent = xOpenSet.top();
		xOpenSet.pop();

		// Skip if already processed
		if (xClosedSet.count(xCurrent.m_uPolygonIndex) > 0)
		{
			continue;
		}

		// Add to closed set
		uint32_t uCurrentClosedIndex = axClosedList.GetSize();
		axClosedList.PushBack(xCurrent);
		xClosedSet.insert(xCurrent.m_uPolygonIndex);

		// Track best partial result
		if (xCurrent.m_fHCost < fBestPartialDist)
		{
			fBestPartialDist = xCurrent.m_fHCost;
			uBestPartialPoly = xCurrent.m_uPolygonIndex;
		}

		// Check if we reached the goal
		if (xCurrent.m_uPolygonIndex == uEndPoly)
		{
			// Reconstruct path
			xResult.m_eStatus = Zenith_PathResult::Status::SUCCESS;

			// Build polygon path backwards
			Zenith_Vector<uint32_t> axPolygonPath;
			uint32_t uTraceIndex = uCurrentClosedIndex;
			while (uTraceIndex != UINT32_MAX)
			{
				axPolygonPath.PushBack(axClosedList.Get(uTraceIndex).m_uPolygonIndex);
				uTraceIndex = axClosedList.Get(uTraceIndex).m_uParentIndex;
			}

			// Reverse to get start-to-end order
			axPolygonPath.Reverse();

			// Convert polygon path to waypoints
			xResult.m_axWaypoints.PushBack(xStartProjected);

			for (uint32_t u = 0; u + 1 < axPolygonPath.GetSize(); ++u)
			{
				Zenith_Maths::Vector3 xMidpoint = GetPortalMidpoint(xNavMesh,
					axPolygonPath.Get(u), axPolygonPath.Get(u + 1));
				xResult.m_axWaypoints.PushBack(xMidpoint);
			}

			xResult.m_axWaypoints.PushBack(xEndProjected);

			// Smooth the path
			SmoothPath(xResult.m_axWaypoints, xNavMesh);

			xResult.m_fTotalDistance = CalculatePathDistance(xResult.m_axWaypoints);
			return xResult;
		}

		// Bounds check before accessing polygon
		Zenith_Assert(xCurrent.m_uPolygonIndex < xNavMesh.GetPolygonCount(),
			"Pathfinding: Polygon index %u out of bounds (count=%u)",
			xCurrent.m_uPolygonIndex, xNavMesh.GetPolygonCount());
		if (xCurrent.m_uPolygonIndex >= xNavMesh.GetPolygonCount())
		{
			continue;  // Skip invalid polygon in release builds
		}

		// Expand neighbors
		const Zenith_NavMeshPolygon& xPoly = xNavMesh.GetPolygon(xCurrent.m_uPolygonIndex);
		const Zenith_Maths::Vector3& xCurrentCenter = xPoly.m_xCenter;

		for (uint32_t u = 0; u < xPoly.m_axNeighborIndices.GetSize(); ++u)
		{
			int32_t iNeighbor = xPoly.m_axNeighborIndices.Get(u);
			if (iNeighbor < 0)
			{
				continue;  // No neighbor on this edge
			}

			uint32_t uNeighbor = static_cast<uint32_t>(iNeighbor);
			if (xClosedSet.count(uNeighbor) > 0)
			{
				continue;  // Already processed
			}

			Zenith_Assert(uNeighbor < xNavMesh.GetPolygonCount(), "Pathfinding: Neighbor index %u out of bounds", uNeighbor);

			const Zenith_NavMeshPolygon& xNeighborPoly = xNavMesh.GetPolygon(uNeighbor);

			// Calculate costs
			float fEdgeCost = Zenith_Maths::Length(xNeighborPoly.m_xCenter - xCurrentCenter);
			fEdgeCost *= xNeighborPoly.m_fCost;  // Apply area cost multiplier

			float fNewGCost = xCurrent.m_fGCost + fEdgeCost;

			// Check if this node is already in open set with a better path
			auto itOpen = xOpenSetGCosts.find(uNeighbor);
			if (itOpen != xOpenSetGCosts.end())
			{
				// Node already in open set - only add if we found a better path
				if (fNewGCost >= itOpen->second)
				{
					continue;  // Existing path is better or equal, skip
				}
				// Update best known g-cost for this node
				itOpen->second = fNewGCost;
			}
			else
			{
				// First time seeing this node
				xOpenSetGCosts[uNeighbor] = fNewGCost;
			}

			float fHCost = Zenith_Maths::Length(xEndProjected - xNeighborPoly.m_xCenter);

			AStarNode xNeighborNode;
			xNeighborNode.m_uPolygonIndex = uNeighbor;
			xNeighborNode.m_uParentIndex = uCurrentClosedIndex;
			xNeighborNode.m_fGCost = fNewGCost;
			xNeighborNode.m_fHCost = fHCost;
			xNeighborNode.m_fFCost = fNewGCost + fHCost;

			xOpenSet.push(xNeighborNode);
		}
	}

	// No complete path found - return partial result if we got closer
	if (uBestPartialPoly != uStartPoly)
	{
		// Find the best partial node
		for (uint32_t u = 0; u < axClosedList.GetSize(); ++u)
		{
			if (axClosedList.Get(u).m_uPolygonIndex == uBestPartialPoly)
			{
				xResult.m_eStatus = Zenith_PathResult::Status::PARTIAL;

				// Build path to best partial
				Zenith_Vector<uint32_t> axPolygonPath;
				uint32_t uTraceIndex = u;
				while (uTraceIndex != UINT32_MAX)
				{
					axPolygonPath.PushBack(axClosedList.Get(uTraceIndex).m_uPolygonIndex);
					uTraceIndex = axClosedList.Get(uTraceIndex).m_uParentIndex;
				}

				// Reverse
				for (uint32_t i = 0; i < axPolygonPath.GetSize() / 2; ++i)
				{
					uint32_t uTemp = axPolygonPath.Get(i);
					uint32_t uOtherIdx = axPolygonPath.GetSize() - 1 - i;
					axPolygonPath.Get(i) = axPolygonPath.Get(uOtherIdx);
					axPolygonPath.Get(uOtherIdx) = uTemp;
				}

				// Convert to waypoints
				xResult.m_axWaypoints.PushBack(xStartProjected);
				for (uint32_t i = 0; i + 1 < axPolygonPath.GetSize(); ++i)
				{
					Zenith_Maths::Vector3 xMidpoint = GetPortalMidpoint(xNavMesh,
						axPolygonPath.Get(i), axPolygonPath.Get(i + 1));
					xResult.m_axWaypoints.PushBack(xMidpoint);
				}

				// Add center of final polygon as destination
				const Zenith_NavMeshPolygon& xFinalPoly = xNavMesh.GetPolygon(uBestPartialPoly);
				xResult.m_axWaypoints.PushBack(xFinalPoly.m_xCenter);

				SmoothPath(xResult.m_axWaypoints, xNavMesh);
				xResult.m_fTotalDistance = CalculatePathDistance(xResult.m_axWaypoints);
				break;
			}
		}
	}

	return xResult;
}

void Zenith_Pathfinding::SmoothPath(Zenith_Vector<Zenith_Maths::Vector3>& axPath,
	const Zenith_NavMesh& xNavMesh)
{
	if (axPath.GetSize() <= 2)
	{
		return;  // Nothing to smooth
	}

	// Simple line-of-sight smoothing
	// A full implementation would use the funnel algorithm

	Zenith_Vector<Zenith_Maths::Vector3> axSmoothed;
	axSmoothed.PushBack(axPath.Get(0));

	uint32_t uCurrent = 0;
	while (uCurrent < axPath.GetSize() - 1)
	{
		// Find furthest visible waypoint using line-of-sight checks
		uint32_t uFurthest = uCurrent + 1;

		for (uint32_t u = uCurrent + 2; u < axPath.GetSize(); ++u)
		{
			// Check if we can see directly from current to u
			Zenith_Maths::Vector3 xHit;
			if (!xNavMesh.Raycast(axPath.Get(uCurrent), axPath.Get(u), xHit))
			{
				// Raycast didn't hit anything on navmesh, path is clear
				uFurthest = u;
			}
			else
			{
				// Raycast hit something - we can't shortcut past this point
				// Stop looking further since subsequent waypoints would also be blocked
				break;
			}
		}

		uCurrent = uFurthest;
		axSmoothed.PushBack(axPath.Get(uCurrent));
	}

	axPath = std::move(axSmoothed);
}

float Zenith_Pathfinding::CalculatePathDistance(const Zenith_Vector<Zenith_Maths::Vector3>& axPath)
{
	float fTotal = 0.0f;
	for (uint32_t u = 0; u + 1 < axPath.GetSize(); ++u)
	{
		fTotal += Zenith_Maths::Length(axPath.Get(u + 1) - axPath.Get(u));
	}
	return fTotal;
}

Zenith_Maths::Vector3 Zenith_Pathfinding::GetPortalMidpoint(const Zenith_NavMesh& xNavMesh,
	uint32_t uPoly1, uint32_t uPoly2)
{
	Zenith_Maths::Vector3 xLeft, xRight;
	if (GetPortal(xNavMesh, uPoly1, uPoly2, xLeft, xRight))
	{
		return (xLeft + xRight) * 0.5f;
	}

	// Fallback to average of centers
	const Zenith_NavMeshPolygon& xP1 = xNavMesh.GetPolygon(uPoly1);
	const Zenith_NavMeshPolygon& xP2 = xNavMesh.GetPolygon(uPoly2);
	return (xP1.m_xCenter + xP2.m_xCenter) * 0.5f;
}

bool Zenith_Pathfinding::GetPortal(const Zenith_NavMesh& xNavMesh,
	uint32_t uPoly1, uint32_t uPoly2,
	Zenith_Maths::Vector3& xLeft, Zenith_Maths::Vector3& xRight)
{
	const Zenith_NavMeshPolygon& xP1 = xNavMesh.GetPolygon(uPoly1);

	// Find shared edge
	for (uint32_t u1 = 0; u1 < xP1.m_axVertexIndices.GetSize(); ++u1)
	{
		if (xP1.m_axNeighborIndices.Get(u1) == static_cast<int32_t>(uPoly2))
		{
			// Found the edge - vertices u1 and (u1+1) form the portal
			uint32_t uV1 = xP1.m_axVertexIndices.Get(u1);
			uint32_t uV2 = xP1.m_axVertexIndices.Get((u1 + 1) % xP1.m_axVertexIndices.GetSize());

			xLeft = xNavMesh.GetVertex(uV1);
			xRight = xNavMesh.GetVertex(uV2);
			return true;
		}
	}

	return false;
}

// ============================================================================
// Batch Parallel Pathfinding
// ============================================================================

void Zenith_Pathfinding::PathfindingTaskFunc(void* pData, u_int uInvocationIndex, u_int)
{
	PathRequest* pxRequests = static_cast<PathRequest*>(pData);
	PathRequest& xRequest = pxRequests[uInvocationIndex];

	if (xRequest.m_pxNavMesh != nullptr)
	{
		xRequest.m_xResult = FindPathInternal(*xRequest.m_pxNavMesh, xRequest.m_xStart, xRequest.m_xEnd);
	}
	else
	{
		xRequest.m_xResult.m_eStatus = Zenith_PathResult::Status::FAILED;
	}
}

void Zenith_Pathfinding::FindPathsBatch(PathRequest* pxRequests, uint32_t uNumRequests)
{
	if (uNumRequests == 0 || pxRequests == nullptr)
	{
		return;
	}

	// Single request - just do it directly (no TaskArray overhead)
	if (uNumRequests == 1)
	{
		Zenith_Profiling::Scope xProfileScope(ZENITH_PROFILE_INDEX__AI_PATHFINDING);
		if (pxRequests[0].m_pxNavMesh != nullptr)
		{
			pxRequests[0].m_xResult = FindPathInternal(
				*pxRequests[0].m_pxNavMesh,
				pxRequests[0].m_xStart,
				pxRequests[0].m_xEnd);
		}
		else
		{
			pxRequests[0].m_xResult.m_eStatus = Zenith_PathResult::Status::FAILED;
		}
		return;
	}

	// Multiple requests - use TaskArray for parallel processing
	// The TaskArray distributes work across worker threads
	Zenith_TaskArray xPathTask(
		ZENITH_PROFILE_INDEX__AI_PATHFINDING,
		PathfindingTaskFunc,
		pxRequests,
		uNumRequests,
		true  // Submitting thread joins - main thread helps process tasks
	);

	Zenith_TaskSystem::SubmitTaskArray(&xPathTask);
	xPathTask.WaitUntilComplete();
}
