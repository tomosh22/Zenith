#include "Zenith.h"
#include "AI/Navigation/Zenith_Pathfinding.h"
#include "AI/Navigation/Zenith_NavMesh.h"
#include "Profiling/Zenith_Profiling.h"
#include "TaskSystem/Zenith_TaskSystemImpl.h"
#include <queue>
#include <unordered_set>
#include <algorithm>

namespace
{
	// A* node for the priority queue. Lives at file scope (anonymous namespace)
	// so file-scope helpers can take it directly without polluting the public
	// header with <queue> / <unordered_set> / <unordered_map>.
	struct AStarNode
	{
		uint32_t m_uPolygonIndex;
		uint32_t m_uParentIndex;  // Index into the closed list, or UINT32_MAX for the start node.
		float m_fGCost;           // Cost from start.
		float m_fHCost;           // Heuristic to end.
		float m_fFCost;           // Total cost (G + H).

		bool operator>(const AStarNode& xOther) const
		{
			return m_fFCost > xOther.m_fFCost;
		}
	};

	using AStarOpenSet = std::priority_queue<AStarNode, std::vector<AStarNode>, std::greater<AStarNode>>;

	// Process one neighbour: skip if closed; compute edge/heuristic costs; add or
	// update the open-set entry. Encapsulates the inner for-loop of FindPathInternal
	// so the driver focuses on A* control flow rather than per-edge bookkeeping.
	void ExpandNeighbor(uint32_t uNeighbor,
		const AStarNode& xCurrent,
		uint32_t uCurrentClosedIndex,
		const Zenith_NavMesh& xNavMesh,
		const Zenith_Maths::Vector3& xCurrentCenter,
		const Zenith_Maths::Vector3& xEndProjected,
		AStarOpenSet& xOpenSet,
		const std::unordered_set<uint32_t>& xClosedSet,
		std::unordered_map<uint32_t, float>& xOpenSetGCosts)
	{
		if (xClosedSet.count(uNeighbor) > 0) return;

		Zenith_Assert(uNeighbor < xNavMesh.GetPolygonCount(),
			"Pathfinding: Neighbor index %u out of bounds", uNeighbor);

		const Zenith_NavMeshPolygon& xNeighborPoly = xNavMesh.GetPolygon(uNeighbor);

		// Dynamic-obstacle gate: blocked polygons (closed doors, transient
		// blockers) are invisible to A*. They are NOT added to the closed set
		// either, so unblocking later via SetPolygonBlocked(false) makes the
		// polygon available on the next path query without any rebuild.
		if (xNeighborPoly.IsBlocked()) return;

		float fEdgeCost = Zenith_Maths::Length(xNeighborPoly.m_xCenter - xCurrentCenter);
		fEdgeCost *= xNeighborPoly.m_fCost;  // Apply area cost multiplier.

		const float fNewGCost = xCurrent.m_fGCost + fEdgeCost;

		auto itOpen = xOpenSetGCosts.find(uNeighbor);
		if (itOpen != xOpenSetGCosts.end())
		{
			if (fNewGCost >= itOpen->second) return;
			itOpen->second = fNewGCost;
		}
		else
		{
			xOpenSetGCosts[uNeighbor] = fNewGCost;
		}

		const float fHCost = Zenith_Maths::Length(xEndProjected - xNeighborPoly.m_xCenter);

		AStarNode xNeighborNode;
		xNeighborNode.m_uPolygonIndex = uNeighbor;
		xNeighborNode.m_uParentIndex = uCurrentClosedIndex;
		xNeighborNode.m_fGCost = fNewGCost;
		xNeighborNode.m_fHCost = fHCost;
		xNeighborNode.m_fFCost = fNewGCost + fHCost;
		xOpenSet.push(xNeighborNode);
	}

	struct PathEndpoints
	{
		uint32_t m_uStartPoly = 0;
		uint32_t m_uEndPoly = 0;
		Zenith_Maths::Vector3 m_xStartProjected{ 0.0f };
		Zenith_Maths::Vector3 m_xEndProjected{ 0.0f };
	};

	// Localise start and end positions to nearest nav-mesh polygons.
	// Returns false (with an AI log) if either point can't be projected.
	bool LocaliseEndpoints(const Zenith_NavMesh& xNavMesh,
		const Zenith_Maths::Vector3& xStart,
		const Zenith_Maths::Vector3& xEnd,
		PathEndpoints& xOut)
	{
		if (!xNavMesh.FindNearestPolygon(xStart, xOut.m_uStartPoly, xOut.m_xStartProjected, 5.0f))
		{
			Zenith_Log(LOG_CATEGORY_AI, "Pathfinding: Start position not on navmesh");
			return false;
		}
		if (!xNavMesh.FindNearestPolygon(xEnd, xOut.m_uEndPoly, xOut.m_xEndProjected, 5.0f))
		{
			Zenith_Log(LOG_CATEGORY_AI, "Pathfinding: End position not on navmesh");
			return false;
		}
		return true;
	}

	// Walk the closed-list parent chain from uTerminalIndex back to the start
	// node and emit a forward-order polygon path.
	Zenith_Vector<uint32_t> ReconstructPolygonPath(const Zenith_Vector<AStarNode>& axClosedList, uint32_t uTerminalIndex)
	{
		Zenith_Vector<uint32_t> axPolygonPath;
		uint32_t uTraceIndex = uTerminalIndex;
		while (uTraceIndex != UINT32_MAX)
		{
			axPolygonPath.PushBack(axClosedList.Get(uTraceIndex).m_uPolygonIndex);
			uTraceIndex = axClosedList.Get(uTraceIndex).m_uParentIndex;
		}
		axPolygonPath.Reverse();
		return axPolygonPath;
	}
}

Zenith_PathResult Zenith_Pathfinding::FindPath(const Zenith_NavMesh& xNavMesh,
	const Zenith_Maths::Vector3& xStart,
	const Zenith_Maths::Vector3& xEnd)
{
	Zenith_Profiling::Scope xProfileScope(ZENITH_PROFILE_INDEX__AI_PATHFINDING);
#ifdef ZENITH_INPUT_SIMULATOR
	// MVP-0.4.4: count every public FindPath call so tests can assert
	// query-volume contracts (e.g. "priest issued <= N path queries
	// during the test window"). FindPathInternal is called from
	// AsyncWorker too; we only count the public-facing call so the
	// async-internal recompute path doesn't double-count.
	Zenith_NavMesh::IncrementQueryCountForTest_Internal();
#endif
	return FindPathInternal(xNavMesh, xStart, xEnd);
}

void Zenith_Pathfinding::BuildWaypointsFromPolygonPath(const Zenith_NavMesh& xNavMesh,
	const Zenith_Vector<uint32_t>& axPolygonPath,
	const Zenith_Maths::Vector3& xStartPoint,
	const Zenith_Maths::Vector3& xEndPoint,
	Zenith_PathResult& xResult)
{
	xResult.m_axWaypoints.PushBack(xStartPoint);
	for (uint32_t u = 0; u + 1 < axPolygonPath.GetSize(); ++u)
	{
		Zenith_Maths::Vector3 xMidpoint = GetPortalMidpoint(xNavMesh,
			axPolygonPath.Get(u), axPolygonPath.Get(u + 1));
		xResult.m_axWaypoints.PushBack(xMidpoint);
	}
	xResult.m_axWaypoints.PushBack(xEndPoint);

	SmoothPath(xResult.m_axWaypoints, xNavMesh);
	xResult.m_fTotalDistance = CalculatePathDistance(xResult.m_axWaypoints);
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

	PathEndpoints xEndpoints;
	if (!LocaliseEndpoints(xNavMesh, xStart, xEnd, xEndpoints)) return xResult;

	// Dynamic-obstacle gate at the endpoint level. ExpandNeighbor already
	// skips FLAG_BLOCKED polygons during A* traversal, but that gate fires
	// AFTER the same-polygon shortcut below — so a query that starts AND
	// ends inside one blocked polygon (closed door footprint, transient
	// blocker, or any scenario on the flat one-polygon DP navmesh) would
	// return a straight SUCCESS path walking through the blocker.
	// Reporting FAILED here matches the semantics callers expect when the
	// requested destination is unreachable through the navmesh's current
	// blocker state.
	if (xNavMesh.GetPolygon(xEndpoints.m_uStartPoly).IsBlocked() ||
		xNavMesh.GetPolygon(xEndpoints.m_uEndPoly).IsBlocked())
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"Pathfinding: endpoint inside blocked polygon (startPoly=%u blocked=%d, endPoly=%u blocked=%d)",
			xEndpoints.m_uStartPoly, xNavMesh.GetPolygon(xEndpoints.m_uStartPoly).IsBlocked() ? 1 : 0,
			xEndpoints.m_uEndPoly,   xNavMesh.GetPolygon(xEndpoints.m_uEndPoly).IsBlocked() ? 1 : 0);
		return xResult;
	}

	// Same polygon — direct path, no A* required.
	if (xEndpoints.m_uStartPoly == xEndpoints.m_uEndPoly)
	{
		xResult.m_eStatus = Zenith_PathResult::Status::SUCCESS;
		xResult.m_axWaypoints.PushBack(xEndpoints.m_xStartProjected);
		xResult.m_axWaypoints.PushBack(xEndpoints.m_xEndProjected);
		xResult.m_fTotalDistance = Zenith_Maths::Length(xEndpoints.m_xEndProjected - xEndpoints.m_xStartProjected);
		return xResult;
	}

	AStarOpenSet xOpenSet;
	std::unordered_set<uint32_t> xClosedSet;
	std::unordered_map<uint32_t, float> xOpenSetGCosts;  // Best g-cost seen for nodes still in the open set.
	Zenith_Vector<AStarNode> axClosedList;

	AStarNode xStartNode;
	xStartNode.m_uPolygonIndex = xEndpoints.m_uStartPoly;
	xStartNode.m_uParentIndex = UINT32_MAX;
	xStartNode.m_fGCost = 0.0f;
	xStartNode.m_fHCost = Zenith_Maths::Length(xEndpoints.m_xEndProjected - xEndpoints.m_xStartProjected);
	xStartNode.m_fFCost = xStartNode.m_fGCost + xStartNode.m_fHCost;
	xOpenSet.push(xStartNode);
	xOpenSetGCosts[xEndpoints.m_uStartPoly] = 0.0f;

	uint32_t uBestPartialPoly = xEndpoints.m_uStartPoly;
	uint32_t uBestPartialClosedIndex = 0;
	float fBestPartialDist = xStartNode.m_fHCost;

	while (!xOpenSet.empty())
	{
		AStarNode xCurrent = xOpenSet.top();
		xOpenSet.pop();

		if (xClosedSet.count(xCurrent.m_uPolygonIndex) > 0) continue;

		const uint32_t uCurrentClosedIndex = axClosedList.GetSize();
		axClosedList.PushBack(xCurrent);
		xClosedSet.insert(xCurrent.m_uPolygonIndex);

		if (xCurrent.m_fHCost < fBestPartialDist)
		{
			fBestPartialDist = xCurrent.m_fHCost;
			uBestPartialPoly = xCurrent.m_uPolygonIndex;
			uBestPartialClosedIndex = uCurrentClosedIndex;
		}

		if (xCurrent.m_uPolygonIndex == xEndpoints.m_uEndPoly)
		{
			xResult.m_eStatus = Zenith_PathResult::Status::SUCCESS;
			BuildWaypointsFromPolygonPath(xNavMesh,
				ReconstructPolygonPath(axClosedList, uCurrentClosedIndex),
				xEndpoints.m_xStartProjected, xEndpoints.m_xEndProjected, xResult);
			return xResult;
		}

		Zenith_Assert(xCurrent.m_uPolygonIndex < xNavMesh.GetPolygonCount(),
			"Pathfinding: Polygon index %u out of bounds (count=%u)",
			xCurrent.m_uPolygonIndex, xNavMesh.GetPolygonCount());
		if (xCurrent.m_uPolygonIndex >= xNavMesh.GetPolygonCount())
		{
			continue;  // Skip invalid polygon in release builds.
		}

		const Zenith_NavMeshPolygon& xPoly = xNavMesh.GetPolygon(xCurrent.m_uPolygonIndex);
		const Zenith_Maths::Vector3& xCurrentCenter = xPoly.m_xCenter;

		for (uint32_t u = 0; u < xPoly.m_axNeighborIndices.GetSize(); ++u)
		{
			const int32_t iNeighbor = xPoly.m_axNeighborIndices.Get(u);
			if (iNeighbor < 0) continue;
			ExpandNeighbor(static_cast<uint32_t>(iNeighbor), xCurrent, uCurrentClosedIndex,
				xNavMesh, xCurrentCenter, xEndpoints.m_xEndProjected,
				xOpenSet, xClosedSet, xOpenSetGCosts);
		}
	}

	// No complete path — emit partial path to the closest node we expanded.
	if (uBestPartialPoly == xEndpoints.m_uStartPoly) return xResult;

	xResult.m_eStatus = Zenith_PathResult::Status::PARTIAL;
	const Zenith_NavMeshPolygon& xFinalPoly = xNavMesh.GetPolygon(uBestPartialPoly);
	BuildWaypointsFromPolygonPath(xNavMesh,
		ReconstructPolygonPath(axClosedList, uBestPartialClosedIndex),
		xEndpoints.m_xStartProjected, xFinalPoly.m_xCenter, xResult);

	return xResult;
}

namespace
{
	// Walks a line segment in `kSamples` steps and returns true if any
	// sampled point lies inside a polygon flagged FLAG_BLOCKED. Used by
	// SmoothPath to refuse a shortcut that would slice through a closed
	// door or any other transient blocker — Zenith_NavMesh::Raycast doesn't
	// know about the flag (it tests ray-vs-polygon-plane, which on a flat
	// navmesh always misses), so without this probe smoothing happily
	// straight-lines the path back through the very obstacle A* routed
	// around.
	//
	// 12 samples gives sub-metre spacing for typical 5–10 m shortcut
	// candidates without measurably impacting per-path planning cost.
	bool SegmentCrossesBlockedPolygon(const Zenith_NavMesh& xNavMesh,
		const Zenith_Maths::Vector3& xA, const Zenith_Maths::Vector3& xB)
	{
		constexpr int kSamples = 12;
		for (int i = 1; i < kSamples; ++i)
		{
			const float fT = static_cast<float>(i) / static_cast<float>(kSamples);
			const Zenith_Maths::Vector3 xP = xA + (xB - xA) * fT;
			const uint32_t uPoly = xNavMesh.FindPolygonContaining(xP, /*fMaxVerticalDist=*/1.5f);
			if (uPoly == UINT32_MAX) continue;
			if (xNavMesh.GetPolygon(uPoly).IsBlocked()) return true;
		}
		return false;
	}

	// Walks a line segment in `kSamples` steps and returns true if any
	// sampled point is NOT on the navmesh at roughly the same height as
	// the endpoints -- i.e., the line passes through a carved-out region
	// (e.g., under a wall) where no floor polygon exists at the agent's
	// current vertical level. The existing `Zenith_NavMesh::Raycast` is a
	// ray-vs-polygon-plane test that misses these gaps because the ray
	// runs parallel to the navmesh polygons and finds no plane to hit; A*
	// can return a "neighbour" between two floor polygons on opposite
	// sides of a wall because adjacency is computed from shared vertex
	// indices and the grid keeps vertex indices stable across the carve.
	//
	// Why the tight vertical tolerance: with the default 1.5m tolerance
	// (matching SegmentCrossesBlockedPolygon), a sample at floor height
	// y=0.1 inside the wall's carved-out footprint would still "find" the
	// wall-top polygon at y=1.0 (0.9m above) and return false (not a
	// hole). We need a tolerance closer to the agent's max step height
	// (0.3m) so polygons at the WRONG vertical level don't mask the gap.
	//
	// Tuning notes:
	//   * 24 samples gives ~0.25m spacing for a 6m shortcut. The wall test
	//     needs at least one sample inside the 0.6m-thick wall footprint;
	//     0.25m spacing leaves ~0.35m of slack either side of the wall.
	//   * Vertical tolerance = mean of endpoint Y ± kVERT_SLACK. Picking
	//     against the actual segment endpoints (vs a fixed 0.3m) lets the
	//     probe work for ramps + multi-level scenes where the line itself
	//     crosses a Y delta.
	//   * Sample indices 1..N-1 only (skip endpoints) -- endpoints are by
	//     construction on the navmesh (A* placed them there).
	bool SegmentExitsNavMesh(const Zenith_NavMesh& xNavMesh,
		const Zenith_Maths::Vector3& xA, const Zenith_Maths::Vector3& xB)
	{
		constexpr int   kSamples    = 24;
		constexpr float kVERT_SLACK = 0.5f;  // half the default 1.0m floor-to-ceiling envelope
		for (int i = 1; i < kSamples; ++i)
		{
			const float fT = static_cast<float>(i) / static_cast<float>(kSamples);
			const Zenith_Maths::Vector3 xP = xA + (xB - xA) * fT;
			const uint32_t uPoly = xNavMesh.FindPolygonContaining(xP, kVERT_SLACK);
			if (uPoly == UINT32_MAX) return true;
		}
		return false;
	}
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
			const bool bRaycastBlocked =
				xNavMesh.Raycast(axPath.Get(uCurrent), axPath.Get(u), xHit);
			// Dynamic-obstacle gate — see SegmentCrossesBlockedPolygon
			// above. The geometric Raycast misses FLAG_BLOCKED polygons
			// on a flat navmesh, so a closed door would be invisible to
			// the smoother and the funnel-equivalent shortcut would
			// cut through it.
			const bool bBlockedByObstacle =
				SegmentCrossesBlockedPolygon(xNavMesh,
					axPath.Get(uCurrent), axPath.Get(u));
			// Static-geometry gate — see SegmentExitsNavMesh above. The
			// geometric Raycast also misses HOLES in the navmesh (carved
			// cells under a wall), so without this check the smoother
			// shortcuts across walls.
			const bool bExitsNavMesh =
				SegmentExitsNavMesh(xNavMesh,
					axPath.Get(uCurrent), axPath.Get(u));
			if (!bRaycastBlocked && !bBlockedByObstacle && !bExitsNavMesh)
			{
				// Path is geometrically clear, doesn't cross a blocked
				// polygon, AND stays on the navmesh — safe to extend.
				uFurthest = u;
			}
			else
			{
				// Either we'd leave the navmesh, cross a blocked polygon,
				// or pass through a carved-out region. Subsequent waypoints
				// would also be unreachable from uCurrent by the same line,
				// so stop searching.
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

	g_xEngine.Tasks().SubmitTaskArray(&xPathTask);
	xPathTask.WaitUntilComplete();
}

#ifdef ZENITH_TESTING
#include "AI/Navigation/Zenith_Pathfinding.Tests.inl"
#endif
