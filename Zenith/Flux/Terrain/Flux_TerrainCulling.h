#pragma once

#include "Flux/Flux.h"
#include "Maths/Zenith_FrustumCulling.h"

/**
 * GPU-Driven Terrain Chunk Culling System with LOD Support
 * 
 * This system performs frustum culling of terrain chunks on the GPU using a compute shader,
 * with automatic Level-of-Detail (LOD) selection based on distance from camera.
 * 
 * Features:
 * 1. Divides the terrain into chunks (64x64 grid)
 * 2. Each chunk has multiple LOD levels that draw progressively fewer triangles
 * 3. Runs a compute shader to test each chunk against the camera frustum
 * 4. Selects appropriate LOD based on distance from camera to chunk center
 * 5. Writes indirect draw commands for only visible chunks with correct LOD
 * 6. Issues a multi-draw-indirect call to render only visible chunks at appropriate detail levels
 * 
 * LOD Strategy:
 * - Instead of separate meshes per LOD, we use the same vertex buffer but draw fewer indices
 * - LOD0: 100% indices (high detail, 0-100 units)
 * - LOD1: 50% indices (medium detail, 100-250 units)
 * - LOD2: 25% indices (low detail, 250-500 units)
 * - LOD3: 12.5% indices (lowest detail, 500+ units)
 * 
 * This dramatically reduces vertex shader workload by:
 * - Skipping vertices for off-screen chunks (frustum culling)
 * - Rendering fewer triangles for distant chunks (LOD selection)
 */

// Number of LOD levels per terrain chunk
#define TERRAIN_LOD_COUNT 4

// LOD data for a single level
struct Flux_TerrainLODData
{
	uint32_t m_uFirstIndex;    // Starting index in the index buffer for this LOD
	uint32_t m_uIndexCount;    // Number of indices to draw for this LOD
	uint32_t m_uVertexOffset;  // Base vertex offset (always 0 for combined mesh)
	float m_fMaxDistance;      // Maximum distance (squared) at which this LOD is used
};

// Chunk data structure that gets uploaded to GPU
// Must match the GLSL struct in Flux_TerrainCulling.comp
struct Flux_TerrainChunkData
{
	Zenith_Maths::Vector4 m_xAABBMin;                      // xyz = min corner, w = padding
	Zenith_Maths::Vector4 m_xAABBMax;                      // xyz = max corner, w = padding
	Flux_TerrainLODData m_axLODs[TERRAIN_LOD_COUNT];       // LOD mesh data (LOD0=highest detail)
};

// Frustum plane structure for GPU upload
// Must match the GLSL struct in Flux_TerrainCulling.comp
struct Flux_FrustumPlaneGPU
{
	Zenith_Maths::Vector4 m_xNormalAndDistance;  // xyz = normal, w = distance
};

// Camera culling data structure for GPU upload
struct Flux_CameraDataGPU
{
	Flux_FrustumPlaneGPU m_axFrustumPlanes[6];  // 6 frustum planes
	Zenith_Maths::Vector4 m_xCameraPosition;     // xyz = camera position, w = padding
};

class Flux_TerrainCulling
{
public:
	static void Initialise();
	static void Shutdown();

	/**
	 * Dispatch the terrain culling compute shader
	 * This tests all terrain chunks against the camera frustum, calculates distance to camera,
	 * selects appropriate LOD level, sorts visible chunks front-to-back, and writes compacted 
	 * indirect draw commands
	 * 
	 * @param xViewProjMatrix The camera's view-projection matrix
	 */
	static void DispatchCulling(const Zenith_Maths::Matrix4& xViewProjMatrix);

	/**
	 * Get the indirect draw buffer for rendering
	 * This buffer contains VkDrawIndexedIndirectCommand structs written by the compute shader
	 * The commands are compacted (only visible chunks) and sorted front-to-back
	 */
	static const Flux_IndirectBuffer& GetIndirectDrawBuffer();

	/**
	 * Get the actual number of visible chunks (for indirect draw count)
	 * This should be read back from GPU after culling compute completes
	 */
	static const Flux_IndirectBuffer& GetVisibleCountBuffer();

	/**
	 * Get the maximum number of draw commands (= 4096, the theoretical maximum)
	 */
	static uint32_t GetMaxDrawCount();

	/**
	 * Get the LOD level buffer for visualization
	 * This buffer contains the LOD level (0-3) for each visible chunk
	 */
	static Flux_ReadWriteBuffer& GetLODLevelBuffer();

private:
	static void BuildChunkData();
	static void ExtractFrustumPlanes(const Zenith_Maths::Matrix4& xViewProjMatrix, Flux_FrustumPlaneGPU* pxOutPlanes);
};
