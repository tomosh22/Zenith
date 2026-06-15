#pragma once
#include "Maths/Zenith_Maths.h"
#include "Collections/Zenith_Vector.h"

// Renderer-neutral generated collision geometry. The leaf generator fills this
// (positions + smooth normals + indices); the engine-side caller turns it into a
// Zenith_MeshGeometryAsset (the asset side owns the Flux_MeshGeometry build), so
// the Physics leaf names no renderer / asset type.
struct Zenith_GeneratedPhysicsMesh
{
	Zenith_Vector<Zenith_Maths::Vector3> m_xPositions;
	Zenith_Vector<Zenith_Maths::Vector3> m_xNormals;
	Zenith_Vector<uint32_t> m_xIndices;

	bool IsValid() const { return m_xPositions.GetSize() >= 3 && m_xIndices.GetSize() >= 3; }
};

/**
 * Zenith_PhysicsMeshGenerator - Generates approximate physics collision geometry
 *
 * This system creates simplified collision meshes from detailed render meshes,
 * suitable for physics simulation. The generated meshes trade fidelity for
 * robustness and performance.
 *
 * Input is renderer-neutral: callers describe each source mesh as a
 * Zenith_PhysicsMeshView (positions + indices spans). This keeps the generator
 * free of any Flux/renderer type so Physics names no renderer symbol. The output
 * is a renderer-neutral Zenith_GeneratedPhysicsMesh POD (positions/normals/indices);
 * the engine-side caller turns it into the Zenith_MeshGeometryAsset, so the leaf
 * names no renderer / asset type.
 *
 * Quality Levels:
 * - LOW: Axis-aligned bounding box (fastest, least accurate)
 * - MEDIUM: Convex hull approximation (good balance)
 * - HIGH: Simplified mesh with vertex decimation (most accurate)
 *
 * Usage:
 *   Zenith_Vector<Zenith_PhysicsMeshView> xViews; // built by the caller from its geometry
 *   Zenith_GeneratedPhysicsMesh xMesh = Zenith_PhysicsMeshGenerator::GeneratePhysicsMesh(
 *       xViews, PHYSICS_MESH_QUALITY_MEDIUM);
 */

// Renderer-neutral view of a single source mesh's collision-relevant geometry.
// The generator only reads positions + indices, so a non-owning span pair is all
// it needs. The caller owns the underlying storage for the call's duration.
struct Zenith_PhysicsMeshView
{
	const Zenith_Maths::Vector3* m_pxPositions = nullptr;
	uint32_t m_uNumVerts = 0;
	const uint32_t* m_puIndices = nullptr;
	uint32_t m_uNumIndices = 0;
};

// Quality level enum for physics mesh generation
enum PhysicsMeshQuality : uint8_t
{
	PHYSICS_MESH_QUALITY_LOW = 0,      // AABB bounding box
	PHYSICS_MESH_QUALITY_MEDIUM = 1,   // Convex hull approximation
	PHYSICS_MESH_QUALITY_HIGH = 2,     // Simplified triangle mesh
	PHYSICS_MESH_QUALITY_COUNT
};

// Configuration for physics mesh generation
struct PhysicsMeshConfig
{
	PhysicsMeshQuality m_eQuality = PHYSICS_MESH_QUALITY_HIGH;

	// For HIGH quality: target triangle reduction ratio (0.0-1.0, 1.0 = no simplification)
	float m_fSimplificationRatio = 1.0f;  // Keep all triangles for exact match to render mesh

	// Minimum number of triangles to keep (prevents over-simplification)
	uint32_t m_uMinTriangles = 100;  // Higher minimum for better shape representation

	// Maximum number of triangles allowed (cap for very complex models)
	uint32_t m_uMaxTriangles = 10000;  // Very high limit to preserve detail

	// Whether to generate physics mesh automatically on model load
	bool m_bAutoGenerate = true;

	// Debug rendering options
	Zenith_Maths::Vector3 m_xDebugColor = Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f); // Green by default
};

// Global configuration (can be modified at runtime)
extern PhysicsMeshConfig g_xPhysicsMeshConfig;

class Zenith_PhysicsMeshGenerator
{
public:
	/**
	 * Generate a physics mesh from a collection of renderer-neutral mesh views
	 *
	 * @param xMeshViews Views of the source meshes (positions + indices spans)
	 * @param eQuality Quality level for generation (overrides global config)
	 * @return Zenith_GeneratedPhysicsMesh POD (returned by value); check IsValid() for failure
	 */
	static Zenith_GeneratedPhysicsMesh GeneratePhysicsMesh(
		const Zenith_Vector<Zenith_PhysicsMeshView>& xMeshViews,
		PhysicsMeshQuality eQuality = PHYSICS_MESH_QUALITY_MEDIUM);

	/**
	 * Generate a physics mesh using the global configuration
	 */
	static Zenith_GeneratedPhysicsMesh GeneratePhysicsMeshWithConfig(
		const Zenith_Vector<Zenith_PhysicsMeshView>& xMeshViews,
		const PhysicsMeshConfig& xConfig);

	/**
	 * Get a string description of the quality level
	 */
	static const char* GetQualityName(PhysicsMeshQuality eQuality);

	// The following three helpers operate purely on neutral position/index data.
	// They are public because the file-static generation helpers (in the .cpp) and
	// the unit tests (Zenith_NavMeshGenerator.Tests.inl) both call them; all other
	// generation helpers are file-static in the .cpp (they produce neutral geometry,
	// and the asset side builds the Flux mesh, so Physics names no renderer type).

	/**
	 * Find extreme vertex indices along all 6 axis directions
	 * @param xPositions Vertex positions to scan
	 * @param auOutIndices Output: [minX, maxX, minY, maxY, minZ, maxZ]
	 */
	static void FindExtremeVertexIndices(
		const Zenith_Vector<Zenith_Maths::Vector3>& xPositions,
		uint32_t auOutIndices[6]);

	/**
	 * Compute AABB from a positions array
	 */
	static void ComputeAABBFromPositions(
		const Zenith_Vector<Zenith_Maths::Vector3>& xPositions,
		Zenith_Maths::Vector3& xMinOut,
		Zenith_Maths::Vector3& xMaxOut);

	/**
	 * Compute smooth vertex normals from positions and indices
	 */
	static void ComputeVertexNormals(
		Zenith_Maths::Vector3* pxNormals,
		const Zenith_Maths::Vector3* pxPositions,
		uint32_t uNumVerts,
		const uint32_t* puIndices,
		uint32_t uNumIndices);
};
