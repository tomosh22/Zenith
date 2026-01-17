#pragma once
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Maths/Zenith_Maths.h"
#include "Collections/Zenith_Vector.h"

// Forward declarations
class Zenith_MeshGeometryAsset;

/**
 * Zenith_PhysicsMeshGenerator - Generates approximate physics collision geometry
 *
 * This system creates simplified collision meshes from detailed render meshes,
 * suitable for physics simulation. The generated meshes trade fidelity for
 * robustness and performance.
 *
 * Quality Levels:
 * - LOW: Axis-aligned bounding box (fastest, least accurate)
 * - MEDIUM: Convex hull approximation (good balance)
 * - HIGH: Simplified mesh with vertex decimation (most accurate)
 *
 * Usage:
 *   Flux_MeshGeometry* pxPhysicsMesh = Zenith_PhysicsMeshGenerator::GeneratePhysicsMesh(
 *       xMeshEntries, PHYSICS_MESH_QUALITY_MEDIUM);
 */

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
	bool m_bDebugDraw = true;
	Zenith_Maths::Vector3 m_xDebugColor = Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f); // Green by default
};

// Global configuration (can be modified at runtime)
extern PhysicsMeshConfig g_xPhysicsMeshConfig;

// Global debug flag for drawing all physics meshes (controlled via debug variables)
extern bool g_bDebugDrawAllPhysicsMeshes;

class Zenith_PhysicsMeshGenerator
{
public:
	/**
	 * Generate a physics mesh from a collection of render mesh entries
	 *
	 * @param xMeshGeometries Vector of pointers to source mesh geometries
	 * @param eQuality Quality level for generation (overrides global config)
	 * @return Registry-managed asset containing the physics mesh, or nullptr on failure
	 */
	static Zenith_MeshGeometryAsset* GeneratePhysicsMesh(
		const Zenith_Vector<Flux_MeshGeometry*>& xMeshGeometries,
		PhysicsMeshQuality eQuality = PHYSICS_MESH_QUALITY_MEDIUM);

	/**
	 * Generate a physics mesh using the global configuration
	 */
	static Zenith_MeshGeometryAsset* GeneratePhysicsMeshWithConfig(
		const Zenith_Vector<Flux_MeshGeometry*>& xMeshGeometries,
		const PhysicsMeshConfig& xConfig);

	/**
	 * Render debug visualization of a physics mesh
	 * Uses Flux_Primitives to draw wireframe representation
	 * 
	 * @param pxPhysicsMesh The physics mesh to visualize
	 * @param xTransform World transform matrix to apply
	 * @param xColor Debug color for wireframe
	 */
	static void DebugDrawPhysicsMesh(
		const Flux_MeshGeometry* pxPhysicsMesh,
		const Zenith_Maths::Matrix4& xTransform,
		const Zenith_Maths::Vector3& xColor);

	/**
	 * Draw debug physics meshes for all model components in the current scene
	 * Call this once per frame when debug drawing is enabled
	 * Checks g_bDebugDrawAllPhysicsMeshes and individual component flags
	 */
	static void DebugDrawAllPhysicsMeshes();

	/**
	 * Get a string description of the quality level
	 */
	static const char* GetQualityName(PhysicsMeshQuality eQuality);

private:
	// Internal generation methods for each quality level
	
	/**
	 * Generate an AABB box mesh (LOW quality)
	 * Creates a simple 12-triangle box that bounds all input geometry
	 */
	static Flux_MeshGeometry* GenerateAABBMesh(
		const Zenith_Vector<Flux_MeshGeometry*>& xMeshGeometries);

	/**
	 * Generate a convex hull mesh (MEDIUM quality)
	 * Uses quickhull algorithm to compute convex hull of all vertices
	 */
	static Flux_MeshGeometry* GenerateConvexHullMesh(
		const Zenith_Vector<Flux_MeshGeometry*>& xMeshGeometries);

	/**
	 * Generate a simplified triangle mesh (HIGH quality)
	 * Combines all meshes and applies vertex decimation
	 */
	static Flux_MeshGeometry* GenerateSimplifiedMesh(
		const Zenith_Vector<Flux_MeshGeometry*>& xMeshGeometries,
		const PhysicsMeshConfig& xConfig);

	/**
	 * Compute AABB bounds from mesh geometries
	 */
	static void ComputeAABB(
		const Zenith_Vector<Flux_MeshGeometry*>& xMeshGeometries,
		Zenith_Maths::Vector3& xMinOut,
		Zenith_Maths::Vector3& xMaxOut);

	/**
	 * Collect all vertex positions from mesh geometries
	 */
	static void CollectAllPositions(
		const Zenith_Vector<Flux_MeshGeometry*>& xMeshGeometries,
		Zenith_Vector<Zenith_Maths::Vector3>& xPositionsOut);

	/**
	 * Simple vertex decimation using spatial hashing
	 * Groups nearby vertices and replaces them with a single representative
	 */
	static void DecimateVertices(
		const Zenith_Vector<Zenith_Maths::Vector3>& xPositions,
		const Zenith_Vector<uint32_t>& xIndices,
		Zenith_Vector<Zenith_Maths::Vector3>& xPositionsOut,
		Zenith_Vector<uint32_t>& xIndicesOut,
		float fCellSize);

	/**
	 * Build a box mesh geometry from min/max bounds
	 */
	static Flux_MeshGeometry* CreateBoxMesh(
		const Zenith_Maths::Vector3& xMin,
		const Zenith_Maths::Vector3& xMax);

	/**
	 * Build a mesh geometry from position and index arrays
	 */
	static Flux_MeshGeometry* CreateMeshFromData(
		const Zenith_Vector<Zenith_Maths::Vector3>& xPositions,
		const Zenith_Vector<uint32_t>& xIndices);
};
