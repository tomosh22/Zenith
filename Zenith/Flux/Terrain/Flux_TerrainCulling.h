#pragma once

#include "Maths/Zenith_FrustumCulling.h"
#include "Collections/Zenith_Vector.h"

// Forward declarations
class Zenith_TerrainComponent;
class Zenith_CameraComponent;
struct Flux_Buffer;
class Flux_Pipeline;
class Flux_Shader;
class Flux_CommandList;

/**
 * Terrain Frustum Culling System
 *
 * This system provides both CPU and GPU-based frustum culling for terrain components.
 *
 * CPU Mode:
 * - Extracts camera frustum each frame
 * - Tests each terrain component's AABB against frustum
 * - Only visible components are submitted for rendering
 *
 * GPU Mode:
 * - All terrain AABBs and camera frustum uploaded to GPU
 * - Compute shader performs culling on GPU
 * - Uses indirect draw to render only visible terrain
 * - More efficient for large terrain counts (100+)
 *
 * Usage:
 *   Flux_TerrainCulling::Initialise();  // Once at startup
 *   Flux_TerrainCulling::SetGPUCullingEnabled(true);  // Enable GPU culling
 *   // In render loop:
 *   Flux_TerrainCulling::PerformCulling(camera, terrainComponents);
 *   // Render using GetVisibleTerrainComponents() or SubmitGPUCulledDraws()
 */
namespace Flux_TerrainCulling
{
	/**
	 * Initialize the culling system
	 * Must be called before any culling operations
	 */
	void Initialise();

	/**
	 * Shutdown and clean up resources
	 */
	void Shutdown();

	/**
	 * Enable or disable GPU-based culling
	 * When disabled, CPU culling is used instead
	 */
	void SetGPUCullingEnabled(bool bEnabled);

	/**
	 * Check if GPU culling is currently enabled
	 */
	bool IsGPUCullingEnabled();

	/**
	 * Generate AABB for a terrain component from its mesh geometry
	 * This should be called once per terrain when it's created
	 *
	 * @param xTerrain The terrain component
	 * @return AABB encompassing the terrain's render mesh
	 */
	Zenith_AABB GenerateTerrainAABB(const Zenith_TerrainComponent& xTerrain);

	/**
	 * Perform frustum culling on terrain components
	 *
	 * This function:
	 * - Extracts frustum from camera
	 * - Tests all terrain AABBs (CPU or GPU)
	 * - Populates visible terrain list
	 *
	 * @param xCamera The camera to cull against
	 * @param xAllTerrain All terrain components in the scene
	 */
	void PerformCulling(
		const Zenith_CameraComponent& xCamera,
		const Zenith_Vector<Zenith_TerrainComponent*>& xAllTerrain
	);

	/**
	 * Get the list of visible terrain components after CPU culling
	 * Only valid after PerformCulling() has been called
	 *
	 * @return Vector of terrain components that passed frustum test
	 */
	const Zenith_Vector<Zenith_TerrainComponent*>& GetVisibleTerrainComponents();

	/**
	 * Get current frame's frustum (for debug visualization)
	 */
	const Zenith_Frustum& GetCurrentFrustum();

	/**
	 * Debug: Get statistics for the last culling operation
	 */
	struct CullingStats
	{
		uint32_t m_uTotalTerrain = 0;
		uint32_t m_uVisibleTerrain = 0;
		uint32_t m_uCulledTerrain = 0;
		float m_fCullingTimeMS = 0.0f;
		bool m_bUsedGPUCulling = false;
	};
	const CullingStats& GetCullingStats();

	/**
	 * Debug: Render visualization of AABBs and frustum
	 *
	 * @param xCamera Camera for the debug view
	 * @param bShowCulledAABBs If true, show AABBs of culled terrain in red
	 * @param bShowVisibleAABBs If true, show AABBs of visible terrain in green
	 * @param bShowFrustum If true, show camera frustum planes
	 */
	void RenderDebugVisualization(
		const Zenith_CameraComponent& xCamera,
		bool bShowCulledAABBs = false,
		bool bShowVisibleAABBs = true,
		bool bShowFrustum = true
	);

	// === GPU Culling Specific ===

	/**
	 * Submit GPU-culled indirect draw commands
	 * Only valid when GPU culling is enabled
	 *
	 * This should be called instead of manually iterating visible terrain
	 * when GPU culling is active.
	 *
	 * @param xCmdList Command list to record draw commands into
	 */
	void SubmitGPUCulledDraws(Flux_CommandList& xCmdList);

	/**
	 * Get the GPU culling compute pipeline (for advanced usage)
	 * Returns nullptr when GPU culling is not implemented
	 */
	void* GetGPUCullingPipeline();
}
