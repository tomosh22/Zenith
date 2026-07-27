#pragma once

#include "Collections/Zenith_Vector.h"
#include "Maths/Zenith_Maths.h"
#include <cstdint>

class Zenith_DataStream;

// ============================================================================
// "ZNAV" wire format (version 1)
//
// Shared by the writer, the VALIDATING reader and the unit tests that hand-build
// corrupt files. The format is unchanged by the SC1b hardening — a valid mesh
// round-trips byte-identically; only the reader's rejection behaviour changed.
//
// Layout:
//   char[4]  "ZNAV"
//   u32      version (== uZENITH_NAVMESH_WIRE_VERSION)
//   u32      vertexCount, then vertexCount * (f32 x, y, z)
//   u32      polygonCount, then polygonCount * polygon records
//   f32[6]   boundsMin.xyz, boundsMax.xyz
//
// A polygon record is:
//   u32      vertexCount, then vertexCount * u32 vertex index
//   u32      neighbourCount (== vertexCount), then neighbourCount * i32
//   f32[7]   center.xyz, normal.xyz, area   (RECOMPUTED on load — see below)
//   u32      flags
//   f32      cost
//
// The per-polygon center/normal/area and the mesh bounds are DEAD BYTES on the
// read path: ReadFromDataStream finishes with BuildSpatialGrid(), which calls
// ComputeSpatialData() and overwrites all of them from the vertices. They are
// still written (churn-free v1) and still read (to advance the cursor), but they
// are deliberately NOT validated — a corrupt value there cannot survive the
// load. Mesh bounds ARE validated: BuildSpatialGrid() early-outs on a zero-
// polygon mesh, so for a well-formed empty mesh the read bounds do stand.
// ============================================================================

constexpr uint32_t uZENITH_NAVMESH_WIRE_VERSION = 1u;

// Hard ceiling on one polygon's vertex count. The generator emits quads; this is
// a corruption guard, not a design limit.
constexpr uint32_t uZENITH_NAVMESH_MAX_POLYGON_VERTICES = 64u;

// The "this edge has no neighbour" sentinel stored in m_axNeighborIndices.
constexpr int32_t iZENITH_NAVMESH_NO_NEIGHBOUR = -1;

/**
 * Zenith_NavMeshDebugDrawFlags - which visual sections Zenith_NavMesh::DebugDraw
 * emits. Per-caller (the editor panel owns one per component), so two navmeshes
 * can be visualised differently at the same time; there is no global draw state.
 */
struct Zenith_NavMeshDebugDrawFlags
{
	bool m_bEdges = true;             // every polygon edge
	bool m_bBoundaryEdges = true;     // edges with no neighbour, thicker
	bool m_bFilled = false;           // fan-triangulated polygon interiors
	bool m_bAdjacencyLinks = false;   // center-to-center portal links
	bool m_bCentersAndNormals = false;// a cross at each center + its normal
	bool m_bHighlightBlocked = true;  // BLOCKED polygons filled in warning colour

	// Lift applied along each polygon's normal so the visualisation does not
	// z-fight the surface it was voxelised from.
	float m_fSurfaceOffset = 0.15f;
};

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

	// Serialization.
	//
	// The read is VALIDATING and TOTAL: it is handed the mesh-level vertex and
	// polygon counts so every index is range-checked BEFORE it is stored, and it
	// returns false — after a Zenith_Assert and a Zenith_Error — on any
	// violation. It takes the counts rather than being reachable through
	// Zenith_DataStream::operator>>, because a polygon cannot be validated
	// without its mesh's counts; the operator would have to trust the file.
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	bool ReadFromDataStream(Zenith_DataStream& xStream,
		uint32_t uMeshVertexCount, uint32_t uMeshPolygonCount);

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

	/**
	 * Stitch a navmesh portal at the given world point along the given
	 * "wall axis" (the door's left-right vector, perpendicular to the
	 * door's facing direction). Used by DPDoor in OnStart -- the wall
	 * authoring in DP's GameLevel uses wall-section colliders that fully
	 * separate adjacent rooms in the voxel heightfield even after the
	 * door's own collider is excluded; the gap between wall endpoints is
	 * voxelised but the resulting polygons on each side end up in
	 * different connected components because the gap polygons are too
	 * narrow / too few cells to bridge them automatically.
	 *
	 * StitchPortalAt scans for the nearest walkable polygon on each side
	 * of the point (offset by `fProbeDistance` along ±xAxis), and adds
	 * a mutual neighbour link between the two on their nearest-facing
	 * edges. After stitching, A* can traverse from one side to the other
	 * directly. The polygons' geometry is unchanged -- this is purely a
	 * graph-connectivity addition.
	 *
	 * Safety:
	 * - If either side fails to find a polygon, the call is a no-op.
	 * - If the two polygons are already neighbours, the call is a no-op.
	 * - Each polygon stores a fixed-size neighbour list (one slot per
	 *   edge). If both polygons have a free slot, the stitch lands on
	 *   the nearest edges. If no free slot is available, the call logs
	 *   and bails (better than overwriting an existing legitimate
	 *   neighbour).
	 *
	 * @return true if a portal stitch was actually added; false on no-op.
	 */
	bool StitchPortalAt(const Zenith_Maths::Vector3& xPoint,
		const Zenith_Maths::Vector3& xAxis,
		float fProbeDistance = 0.6f,
		float fMaxVerticalDist = 1.5f);

	// ========== Accessors ==========

	uint32_t GetVertexCount() const { return m_axVertices.GetSize(); }
	uint32_t GetPolygonCount() const { return m_axPolygons.GetSize(); }

	const Zenith_Maths::Vector3& GetVertex(uint32_t uIndex) const { return m_axVertices.Get(uIndex); }
	const Zenith_NavMeshPolygon& GetPolygon(uint32_t uIndex) const { return m_axPolygons.Get(uIndex); }

	const Zenith_Vector<Zenith_Maths::Vector3>& GetVertices() const { return m_axVertices; }
	const Zenith_Vector<Zenith_NavMeshPolygon>& GetPolygons() const { return m_axPolygons; }

	const Zenith_Maths::Vector3& GetBoundsMin() const { return m_xBoundsMin; }
	const Zenith_Maths::Vector3& GetBoundsMax() const { return m_xBoundsMax; }

#ifdef ZENITH_INPUT_SIMULATOR
	// ========== Test instrumentation (MVP-0.4.4) ==========
	//
	// Per-process counter incremented every time Zenith_Pathfinding::FindPath
	// is called. Used by tests asserting "the priest issued at most N path
	// queries during the test window" without needing to instrument
	// Priest_Behaviour or the AI agent. Owned by the navmesh namespace
	// because the counter is a property of the navigation system as a whole
	// rather than any single NavMesh instance.
	//
	// API:
	//   GetQueryCountForTest() -- returns the live counter.
	//   ResetQueryCountForTest() -- zero the counter (call at test setup).
	//   IncrementQueryCountForTest_Internal() -- called by FindPath. Not
	//       intended for direct use; the suffix flags it as engine-internal.
	static u_int GetQueryCountForTest();
	static void  ResetQueryCountForTest();
	static void  IncrementQueryCountForTest_Internal();
#endif

	// ========== Serialization ==========

	void WriteToDataStream(Zenith_DataStream& xStream) const;

	/**
	 * Read a "ZNAV" v1 mesh from xStream, validating every section before it is
	 * used. EVERY rejection fires a Zenith_Assert (a malformed navmesh is a
	 * defect, not an expected state — the asset is committed) plus a
	 * Zenith_Error, and returns false.
	 *
	 * DEFINED FAILURE: on false the mesh is left EMPTY (Clear()ed), never
	 * half-populated. All validation is ordered ahead of every Reserve and every
	 * indexed Get, so a corrupt count can neither drive an allocation nor read
	 * out of bounds before the assert fires.
	 *
	 * @return true if the whole stream parsed and the spatial grid was rebuilt.
	 */
	bool ReadFromDataStream(Zenith_DataStream& xStream);

	/**
	 * Load from file (.znavmesh).
	 * @return A newly allocated mesh the caller owns, or nullptr on ANY failure
	 *         (empty path, missing file, unreadable file, invalid contents). The
	 *         partially-read mesh is deleted rather than handed back.
	 */
	static Zenith_NavMesh* LoadFromFile(const std::string& strPath);

	/**
	 * Save to file (.znavmesh).
	 * @return true only if the bytes are back on disk afterwards. Zenith_FileAccess
	 *         ::WriteFile returns void, so an existence re-check is the only
	 *         truthful success signal available here.
	 */
	bool SaveToFile(const std::string& strPath) const;

	// ========== Debug Visualization ==========

#ifdef ZENITH_TOOLS
	// Emit the requested visual sections through the Zenith_AI_DebugDraw* seam.
	// Draw state is the CALLER's (see Zenith_NavMeshDebugDrawFlags) — this
	// consults no global, so the editor's per-component toggles are the whole
	// story and two meshes can be visualised differently at once.
	void DebugDraw(const Zenith_NavMeshDebugDrawFlags& xFlags) const;
#endif

private:
	friend class Zenith_UnitTests;

	// Mesh data
	Zenith_Vector<Zenith_Maths::Vector3> m_axVertices;
	Zenith_Vector<Zenith_NavMeshPolygon> m_axPolygons;

	// Sampling stream for GetRandomReachablePointInRadius. Per-INSTANCE and
	// seeded to a fixed constant -- NEVER std::random_device, which is what
	// this used to be and which made every call non-reproducible, silently
	// contradicting the determinism contract documented in CLAUDE.md.
	// `mutable` because sampling is logically const but MUST advance, so
	// repeated calls still return different points (an agent wandering must
	// not be handed the same destination forever). Reset by Clear(), so a
	// reloaded mesh -- and every test -- starts from the same stream position
	// instead of inheriting whatever the previous consumer left behind.
	// Main-thread (graph-tick) use only, matching its sole caller.
	static constexpr uint64_t k_ulSampleRngSeed = 0x9E3779B97F4A7C15ull;
	mutable uint64_t m_ulSampleRngState = k_ulSampleRngSeed;

	// xorshift64* -> uniform float in [0,1). Mirrors the graph Random* nodes'
	// PRNG so the engine has one sampling idiom, not two.
	float SampleUnit() const;

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

	// ReadFromDataStream sections. Each validates its own section fully and
	// returns false (having asserted + logged) rather than throwing the caller a
	// half-read stream. Split out so the orchestrator stays a flat conjunction.
	bool ReadHeaderFromDataStream(Zenith_DataStream& xStream);
	bool ReadVerticesFromDataStream(Zenith_DataStream& xStream);
	bool ReadPolygonsFromDataStream(Zenith_DataStream& xStream);
	bool ReadBoundsFromDataStream(Zenith_DataStream& xStream);

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
	void DebugDrawCenterAndNormal(const Zenith_NavMeshPolygon& xPoly, const Zenith_Maths::Vector3& xOffset,
		const Zenith_Maths::Vector3& xCenterColor) const;
#endif
};
