#pragma once

#include "Collections/Zenith_Vector.h"
#include "Maths/Zenith_Maths.h"
#include <cstdint>

class Zenith_DataStream;

/**
 * Zenith_NavMeshPolygon - A convex polygon in the navigation mesh
 *
 * Stores vertex indices, neighbor connections, and cached spatial data.
 * All vertices are stored in counter-clockwise winding order.
 */
struct Zenith_NavMeshPolygon
{
	// Indices into the NavMesh vertex array (CCW winding)
	Zenith_Vector<uint32_t> m_axVertexIndices;

	// Indices of adjacent polygons (-1 if no neighbor on that edge)
	// Edge i connects vertices [i] and [(i+1) % vertexCount]
	Zenith_Vector<int32_t> m_axNeighborIndices;

	// Cached spatial data
	Zenith_Maths::Vector3 m_xCenter;
	Zenith_Maths::Vector3 m_xNormal;
	float m_fArea;

	// For pathfinding
	uint32_t m_uFlags = 0;  // Custom flags (e.g., walkability modifiers)
	float m_fCost = 1.0f;   // Traversal cost multiplier

	// Polygon flag bits — keep room for renderer hints / game-specific tags by
	// reserving the low byte for engine use. BLOCKED is the dynamic-obstacle
	// gate consulted in Zenith_Pathfinding::ExpandNeighbor: when set, A* skips
	// the polygon entirely, which is how DPDoor (and any future toggleable
	// blocker) cuts the navmesh without rebuilding it.
	static constexpr uint32_t FLAG_BLOCKED = 1u << 0;

	bool IsBlocked() const { return (m_uFlags & FLAG_BLOCKED) != 0u; }

	// Serialization
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

	// Compute center, normal, and area from vertices
	void ComputeSpatialData(const Zenith_Vector<Zenith_Maths::Vector3>& axVertices);

	// Check if a point (projected to polygon plane) is inside
	bool ContainsPoint(const Zenith_Maths::Vector3& xPoint,
		const Zenith_Vector<Zenith_Maths::Vector3>& axVertices) const;

	// Get closest point on this polygon to the given point
	Zenith_Maths::Vector3 GetClosestPoint(const Zenith_Maths::Vector3& xPoint,
		const Zenith_Vector<Zenith_Maths::Vector3>& axVertices) const;
};

/**
 * Zenith_NavMesh - Navigation mesh for pathfinding
 *
 * A set of convex polygons representing walkable areas.
 * Provides spatial queries for point location and raycasting.
 */
class Zenith_NavMesh
{
public:
	Zenith_NavMesh() = default;
	~Zenith_NavMesh() = default;

	// Move semantics
	Zenith_NavMesh(Zenith_NavMesh&&) noexcept = default;
	Zenith_NavMesh& operator=(Zenith_NavMesh&&) noexcept = default;

	// No copying (large data structure)
	Zenith_NavMesh(const Zenith_NavMesh&) = delete;
	Zenith_NavMesh& operator=(const Zenith_NavMesh&) = delete;

	// ========== Building ==========

	/**
	 * Clear all data
	 */
	void Clear();

	/**
	 * Add a vertex to the mesh
	 * @return Index of the new vertex
	 */
	uint32_t AddVertex(const Zenith_Maths::Vector3& xVertex);

	/**
	 * Add a polygon to the mesh
	 * @param axVertexIndices Indices of vertices (CCW winding)
	 * @return Index of the new polygon
	 */
	uint32_t AddPolygon(const Zenith_Vector<uint32_t>& axVertexIndices);

	/**
	 * Set neighbor relationship between two polygons
	 * @param uPoly1 First polygon index
	 * @param uEdge1 Edge index in first polygon
	 * @param uPoly2 Second polygon index
	 */
	void SetNeighbor(uint32_t uPoly1, uint32_t uEdge1, uint32_t uPoly2);

	/**
	 * Compute all spatial data (call after building)
	 */
	void ComputeSpatialData();

	/**
	 * Automatically compute polygon adjacency by finding shared edges
	 * Call after all polygons have been added
	 */
	void ComputeAdjacency();

	/**
	 * Build spatial acceleration grid (call after ComputeSpatialData)
	 */
	void BuildSpatialGrid();

	// ========== Queries ==========

	/**
	 * Find the nearest polygon to a point
	 * @param xPoint Query point
	 * @param uPolyOut Output: nearest polygon index
	 * @param xNearestOut Output: nearest point on the navmesh
	 * @param fMaxDist Maximum search distance
	 * @return True if a polygon was found within range
	 */
	bool FindNearestPolygon(const Zenith_Maths::Vector3& xPoint,
		uint32_t& uPolyOut, Zenith_Maths::Vector3& xNearestOut,
		float fMaxDist = 10.0f) const;

	/**
	 * Check if a point is on the navigation mesh
	 * @param xPoint Query point
	 * @param fMaxVerticalDist Maximum vertical distance from mesh surface
	 * @return True if point is on the navmesh
	 */
	bool IsPointOnNavMesh(const Zenith_Maths::Vector3& xPoint,
		float fMaxVerticalDist = 0.5f) const;

	/**
	 * Find which polygon contains a point
	 * @param xPoint Query point
	 * @param fMaxVerticalDist Maximum vertical distance from mesh surface
	 * @return Polygon index, or UINT32_MAX if not found
	 */
	uint32_t FindPolygonContaining(const Zenith_Maths::Vector3& xPoint,
		float fMaxVerticalDist = 0.5f) const;

	/**
	 * Cast a ray against the navmesh
	 * @param xStart Ray start point
	 * @param xEnd Ray end point
	 * @param xHitOut Output: hit point on navmesh
	 * @return True if ray hit the navmesh
	 */
	bool Raycast(const Zenith_Maths::Vector3& xStart,
		const Zenith_Maths::Vector3& xEnd,
		Zenith_Maths::Vector3& xHitOut) const;

	/**
	 * Project a point onto the navmesh surface
	 * @param xPoint Point to project
	 * @param xProjectedOut Output: projected point
	 * @param fMaxDist Maximum search distance
	 * @return True if projection succeeded
	 */
	bool ProjectPoint(const Zenith_Maths::Vector3& xPoint,
		Zenith_Maths::Vector3& xProjectedOut,
		float fMaxDist = 10.0f) const;

	/**
	 * Sample a uniformly-random point that is REACHABLE on the navmesh from
	 * the source center within a horizontal radius. Reachable here means
	 * "path-connected via polygon neighbours starting from the polygon
	 * nearest the center". This matches Unreal's UNavigationSystem
	 * GetRandomReachablePointInRadius semantics — points on a disconnected
	 * island are NOT returned, even if they happen to lie within the radius.
	 *
	 * Algorithm:
	 *  1. Find the nearest polygon to xCenter (the source island).
	 *  2. BFS over polygon neighbours; cap each polygon at horizontal
	 *     distance ≤ fRadius from xCenter.
	 *  3. Pick a polygon weighted by area; pick a triangle inside the
	 *     polygon's fan-triangulation weighted by triangle area; sample a
	 *     uniform barycentric point inside that triangle.
	 *  4. Project the candidate to the navmesh surface.
	 *  5. Verify horizontal distance ≤ fRadius. Retry up to uMaxAttempts.
	 *
	 * @param xCenter Source point. The result is reachable from the polygon
	 *                nearest this point.
	 * @param fRadius Horizontal radius (XZ-plane). Vertical distance is not
	 *                bounded; the surface position is taken from
	 *                ProjectPoint.
	 * @param xOutPoint Output: random reachable point. Untouched on false.
	 * @param uMaxAttempts Per-polygon-pick rejection-sampling budget. With
	 *                area-weighted selection, even 16 attempts converges fast.
	 * @return True if a point was found, false if the source polygon could
	 *         not be located, the reachable region is empty, or every
	 *         attempt's sample fell outside the radius.
	 */
	bool GetRandomReachablePointInRadius(const Zenith_Maths::Vector3& xCenter,
		float fRadius,
		Zenith_Maths::Vector3& xOutPoint,
		uint32_t uMaxAttempts = 16) const;

	// ========== Dynamic obstacles ==========

	/**
	 * Toggle a polygon's BLOCKED flag. Blocked polygons are skipped by
	 * Zenith_Pathfinding::FindPath, so the caller can carve transient
	 * obstacles (e.g., closed doors) out of an otherwise static navmesh
	 * without re-running mesh generation. Idempotent.
	 *
	 * Marked `const` against the mesh's pathing semantics — the topology
	 * does not change. Mutates the polygon's flag field via const_cast
	 * internally so any thread that already holds a const navmesh handle
	 * can still flip dynamic-obstacle state. Caller must serialise
	 * concurrent toggles externally.
	 */
	void SetPolygonBlocked(uint32_t uPoly, bool bBlocked) const;

	/**
	 * Block / unblock every polygon whose 2D footprint contains the given
	 * world point. Convenience for "find polygon under door pivot and
	 * toggle it" — DPDoor uses this so it doesn't have to know which
	 * navmesh polygon corresponds to its mesh footprint.
	 *
	 * @return Number of polygons toggled (0 if no polygon contains xPoint).
	 */
	uint32_t SetBlockedAtPoint(const Zenith_Maths::Vector3& xPoint, bool bBlocked,
		float fMaxVerticalDist = 1.5f) const;

	// ========== Accessors ==========

	uint32_t GetVertexCount() const { return m_axVertices.GetSize(); }
	uint32_t GetPolygonCount() const { return m_axPolygons.GetSize(); }

	const Zenith_Maths::Vector3& GetVertex(uint32_t uIndex) const { return m_axVertices.Get(uIndex); }
	const Zenith_NavMeshPolygon& GetPolygon(uint32_t uIndex) const { return m_axPolygons.Get(uIndex); }

	const Zenith_Vector<Zenith_Maths::Vector3>& GetVertices() const { return m_axVertices; }
	const Zenith_Vector<Zenith_NavMeshPolygon>& GetPolygons() const { return m_axPolygons; }

	const Zenith_Maths::Vector3& GetBoundsMin() const { return m_xBoundsMin; }
	const Zenith_Maths::Vector3& GetBoundsMax() const { return m_xBoundsMax; }

	// ========== Serialization ==========

	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

	/**
	 * Load from file (.znavmesh)
	 */
	static Zenith_NavMesh* LoadFromFile(const std::string& strPath);

	/**
	 * Save to file (.znavmesh)
	 */
	bool SaveToFile(const std::string& strPath) const;

	// ========== Debug Visualization ==========

#ifdef ZENITH_TOOLS
	void DebugDraw() const;
#endif

private:
	friend class Zenith_UnitTests;

	// Mesh data
	Zenith_Vector<Zenith_Maths::Vector3> m_axVertices;
	Zenith_Vector<Zenith_NavMeshPolygon> m_axPolygons;

	// Bounding box
	Zenith_Maths::Vector3 m_xBoundsMin;
	Zenith_Maths::Vector3 m_xBoundsMax;

	// Spatial acceleration grid
	struct GridCell
	{
		Zenith_Vector<uint32_t> m_axPolygonIndices;
	};

	float m_fGridCellSize = 5.0f;
	uint32_t m_uGridWidth = 0;
	uint32_t m_uGridHeight = 0;
	Zenith_Vector<GridCell> m_axGridCells;

	// Helper to get grid cell for a position
	void GetGridCoords(const Zenith_Maths::Vector3& xPos, int32_t& iX, int32_t& iZ) const;
	uint32_t GetGridCellIndex(int32_t iX, int32_t iZ) const;

	/**
	 * Search a single grid cell for the nearest polygon to a point
	 * @param uCellIndex Index into m_axGridCells
	 * @param xPoint Query point
	 * @param fMinDistSq In/out: current minimum distance squared
	 * @param uPolyOut In/out: current nearest polygon index
	 * @param xNearestOut In/out: current nearest point on navmesh
	 */
	void FindNearestPolygonInCell(uint32_t uCellIndex, const Zenith_Maths::Vector3& xPoint,
		float& fMinDistSq, uint32_t& uPolyOut, Zenith_Maths::Vector3& xNearestOut) const;

	/**
	 * Compute the 2D (XZ plane) axis-aligned bounding box of a polygon
	 * @param xPoly The polygon to compute bounds for
	 * @param xPolyMinOut Output: minimum bounds (only x and z are set)
	 * @param xPolyMaxOut Output: maximum bounds (only x and z are set)
	 */
	static void ComputePolygonBounds2D(const Zenith_NavMeshPolygon& xPoly,
		const Zenith_Vector<Zenith_Maths::Vector3>& axVertices,
		Zenith_Maths::Vector3& xPolyMinOut, Zenith_Maths::Vector3& xPolyMaxOut);

#ifdef ZENITH_TOOLS
	// DebugDraw helpers (each draws one visual section per polygon)
	void DebugDrawEdges(const Zenith_NavMeshPolygon& xPoly, const Zenith_Maths::Vector3& xOffset,
		const Zenith_Maths::Vector3& xEdgeColor) const;
	void DebugDrawBoundaryEdges(const Zenith_NavMeshPolygon& xPoly, const Zenith_Maths::Vector3& xOffset,
		const Zenith_Maths::Vector3& xBoundaryColor) const;
	void DebugDrawPolygonFill(const Zenith_NavMeshPolygon& xPoly, const Zenith_Maths::Vector3& xOffset,
		const Zenith_Maths::Vector3& xWalkableColor) const;
	void DebugDrawNeighborConnections(uint32_t uPoly, const Zenith_NavMeshPolygon& xPoly,
		const Zenith_Maths::Vector3& xOffset, const Zenith_Maths::Vector3& xNeighborColor) const;
#endif
};
