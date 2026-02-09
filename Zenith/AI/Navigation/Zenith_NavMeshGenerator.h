#pragma once

#include "AI/Navigation/Zenith_NavMesh.h"
#include "Maths/Zenith_Maths.h"
#include "Collections/Zenith_Vector.h"

class Zenith_SceneData;

/**
 * NavMeshGenerationConfig - Parameters for navmesh generation
 */
struct NavMeshGenerationConfig
{
	// Agent parameters
	float m_fAgentRadius = 0.4f;       // Agent collision radius
	float m_fAgentHeight = 1.8f;       // Agent height
	float m_fMaxStepHeight = 0.3f;     // Maximum traversable step height
	float m_fMaxSlope = 45.0f;         // Maximum walkable slope (degrees)

	// Voxelization parameters
	float m_fCellSize = 0.3f;          // Horizontal voxel size
	float m_fCellHeight = 0.2f;        // Vertical voxel size

	// Region parameters
	uint32_t m_uMinRegionArea = 8;     // Minimum region area in cells
	uint32_t m_uMergeRegionArea = 20;  // Merge regions smaller than this

	// Simplification
	float m_fMaxEdgeError = 1.3f;      // Max contour simplification error
	uint32_t m_uMaxEdgeLength = 12;    // Max edge length in cells

	// Detail mesh (not used in this simplified implementation)
	float m_fDetailSampleDist = 6.0f;
	float m_fDetailMaxError = 1.0f;
};

/**
 * Zenith_NavMeshGenerator - Generates navigation meshes from scene geometry
 *
 * Pipeline based on Recast-style generation:
 * 1. Collect geometry from static ColliderComponents
 * 2. Voxelize into 3D heightfield
 * 3. Filter walkable spans (slope, step height, clearance)
 * 4. Build regions via watershed
 * 5. Trace contours
 * 6. Build polygon mesh
 * 7. Compute adjacency
 */
class Zenith_NavMeshGenerator
{
public:
	/**
	 * Generate a navigation mesh from scene static geometry
	 * @param xScene Scene containing ColliderComponents
	 * @param xConfig Generation parameters
	 * @return Newly allocated NavMesh (caller owns), or nullptr on failure
	 */
	static Zenith_NavMesh* GenerateFromScene(Zenith_SceneData& xScene, const NavMeshGenerationConfig& xConfig);

	/**
	 * Generate a navigation mesh from explicit geometry
	 * @param axVertices Vertex positions
	 * @param axIndices Triangle indices
	 * @param xConfig Generation parameters
	 * @return Newly allocated NavMesh (caller owns), or nullptr on failure
	 */
	static Zenith_NavMesh* GenerateFromGeometry(
		const Zenith_Vector<Zenith_Maths::Vector3>& axVertices,
		const Zenith_Vector<uint32_t>& axIndices,
		const NavMeshGenerationConfig& xConfig);

private:
	// Internal data structures

	// Voxel span in heightfield
	struct VoxelSpan
	{
		uint16_t m_uMinY;      // Bottom of span (in cell heights)
		uint16_t m_uMaxY;      // Top of span
		uint16_t m_uRegion;    // Region ID (0 = unwalkable)
		uint8_t m_uAreaType;   // Area flags
		VoxelSpan* m_pxNext;   // Next span in same column (linked list)
	};

	// Heightfield column
	struct HeightfieldColumn
	{
		VoxelSpan* m_pxFirstSpan = nullptr;
	};

	// Compact heightfield for efficient processing
	struct CompactSpan
	{
		uint16_t m_uY;           // Height
		uint16_t m_uRegion;      // Region ID
		uint8_t m_uNeighbors[4]; // Neighbor connections (0 = no connection)
	};

	// Contour vertex
	struct ContourVertex
	{
		int32_t m_iX, m_iY, m_iZ;
		uint16_t m_uRegion;
	};

	// Generation context - uses RAII for automatic memory cleanup
	struct GenerationContext
	{
		~GenerationContext();  // Frees heightfield spans automatically

		// Prevent copying to avoid double-delete
		GenerationContext() = default;
		GenerationContext(const GenerationContext&) = delete;
		GenerationContext& operator=(const GenerationContext&) = delete;

		NavMeshGenerationConfig m_xConfig;

		// World bounds
		Zenith_Maths::Vector3 m_xBoundsMin;
		Zenith_Maths::Vector3 m_xBoundsMax;

		// Heightfield dimensions
		int32_t m_iWidth = 0;   // X cells
		int32_t m_iHeight = 0;  // Z cells
		int32_t m_iDepth = 0;   // Y cells

		// Heightfield columns
		Zenith_Vector<HeightfieldColumn> m_axColumns;

		// Compact spans
		Zenith_Vector<CompactSpan> m_axCompactSpans;
		Zenith_Vector<uint32_t> m_axColumnSpanCounts;  // Span count per column
		Zenith_Vector<uint32_t> m_axColumnSpanStarts;  // First span index per column

		// Contours per region
		Zenith_Vector<Zenith_Vector<ContourVertex>> m_axContours;

		// Output vertices and polygons
		Zenith_Vector<Zenith_Maths::Vector3> m_axOutputVertices;
		Zenith_Vector<Zenith_Vector<uint32_t>> m_axOutputPolygons;
	};

	// Pipeline stages
	static bool CollectGeometryFromScene(Zenith_SceneData& xScene,
		Zenith_Vector<Zenith_Maths::Vector3>& axVerticesOut,
		Zenith_Vector<uint32_t>& axIndicesOut);

	static bool ComputeBounds(
		const Zenith_Vector<Zenith_Maths::Vector3>& axVertices,
		GenerationContext& xContext);

	static bool VoxelizeTriangles(
		const Zenith_Vector<Zenith_Maths::Vector3>& axVertices,
		const Zenith_Vector<uint32_t>& axIndices,
		GenerationContext& xContext);

	static void RasterizeTriangle(
		const Zenith_Maths::Vector3& xV0,
		const Zenith_Maths::Vector3& xV1,
		const Zenith_Maths::Vector3& xV2,
		GenerationContext& xContext);

	static bool FilterWalkableSpans(GenerationContext& xContext);

	static bool BuildCompactHeightfield(GenerationContext& xContext);

	static bool BuildRegions(GenerationContext& xContext);

	static bool TraceContours(GenerationContext& xContext);

	static bool BuildPolygonMesh(GenerationContext& xContext);

	static Zenith_NavMesh* BuildNavMesh(GenerationContext& xContext);

	// Helpers
	static int32_t GetColumnIndex(int32_t iX, int32_t iZ, int32_t iWidth);
	static bool IsWalkableSlope(const Zenith_Maths::Vector3& xNormal, float fMaxSlopeDeg);
	static void AddSpan(HeightfieldColumn& xColumn, uint16_t uMinY, uint16_t uMaxY, uint8_t uAreaType);
	static void FreeHeightfield(GenerationContext& xContext);
};
