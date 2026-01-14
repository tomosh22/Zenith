#pragma once

#include "Collections/Zenith_Vector.h"
#include "Maths/Zenith_Maths.h"

class Zenith_NavMesh;

/**
 * Zenith_PathResult - Result of a pathfinding query
 */
struct Zenith_PathResult
{
	enum class Status
	{
		SUCCESS,    // Path found to destination
		PARTIAL,    // Path found to closest reachable point
		FAILED      // No path found
	};

	Status m_eStatus = Status::FAILED;
	Zenith_Vector<Zenith_Maths::Vector3> m_axWaypoints;
	float m_fTotalDistance = 0.0f;
};

/**
 * Zenith_Pathfinding - A* pathfinding on navigation meshes
 *
 * Finds paths through connected NavMesh polygons using A* algorithm.
 * Includes string-pulling for path smoothing.
 */
class Zenith_Pathfinding
{
public:
	/**
	 * Find a path between two points
	 * @param xNavMesh Navigation mesh to search
	 * @param xStart Start position (will be projected to navmesh)
	 * @param xEnd End position (will be projected to navmesh)
	 * @return Path result with waypoints
	 */
	static Zenith_PathResult FindPath(const Zenith_NavMesh& xNavMesh,
		const Zenith_Maths::Vector3& xStart,
		const Zenith_Maths::Vector3& xEnd);

	/**
	 * Smooth a path using string-pulling (funnel algorithm)
	 * @param axPath Path to smooth (modified in place)
	 * @param xNavMesh Navigation mesh for validity checks
	 */
	static void SmoothPath(Zenith_Vector<Zenith_Maths::Vector3>& axPath,
		const Zenith_NavMesh& xNavMesh);

	/**
	 * Calculate total distance of a path
	 */
	static float CalculatePathDistance(const Zenith_Vector<Zenith_Maths::Vector3>& axPath);

private:
	// A* node for priority queue
	struct AStarNode
	{
		uint32_t m_uPolygonIndex;
		uint32_t m_uParentIndex;  // Index in closed list
		float m_fGCost;           // Cost from start
		float m_fHCost;           // Heuristic to end
		float m_fFCost;           // Total cost (G + H)

		bool operator>(const AStarNode& xOther) const
		{
			return m_fFCost > xOther.m_fFCost;
		}
	};

	// Get midpoint of shared edge between two polygons
	static Zenith_Maths::Vector3 GetPortalMidpoint(const Zenith_NavMesh& xNavMesh,
		uint32_t uPoly1, uint32_t uPoly2);

	// Get the portal (shared edge) between two adjacent polygons
	static bool GetPortal(const Zenith_NavMesh& xNavMesh,
		uint32_t uPoly1, uint32_t uPoly2,
		Zenith_Maths::Vector3& xLeft, Zenith_Maths::Vector3& xRight);

	// Funnel algorithm helper
	static float TriArea2D(const Zenith_Maths::Vector3& xA,
		const Zenith_Maths::Vector3& xB,
		const Zenith_Maths::Vector3& xC);

	// ========================================================================
	// Batch Parallel Pathfinding API
	// ========================================================================

public:
	/**
	 * Path request for batch processing
	 */
	struct PathRequest
	{
		const Zenith_NavMesh* m_pxNavMesh = nullptr;
		Zenith_Maths::Vector3 m_xStart;
		Zenith_Maths::Vector3 m_xEnd;
		Zenith_PathResult m_xResult;  // Output - filled by FindPathsBatch
	};

	/**
	 * Find multiple paths in parallel using TaskArray
	 * Blocks until all paths are computed
	 * @param pxRequests Array of path requests (results written to m_xResult)
	 * @param uNumRequests Number of requests in array
	 */
	static void FindPathsBatch(PathRequest* pxRequests, uint32_t uNumRequests);

private:
	// Internal pathfinding without profiling (for batch processing)
	static Zenith_PathResult FindPathInternal(const Zenith_NavMesh& xNavMesh,
		const Zenith_Maths::Vector3& xStart,
		const Zenith_Maths::Vector3& xEnd);

	// TaskArray callback for parallel pathfinding
	static void PathfindingTaskFunc(void* pData, u_int uInvocationIndex, u_int uNumInvocations);
};
