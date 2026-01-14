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
};
