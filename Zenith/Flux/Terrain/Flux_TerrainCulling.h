#pragma once

#include "Flux/Flux.h"
#include "Maths/Zenith_FrustumCulling.h"

/**
 * GPU-Driven Terrain Chunk Culling System
 * 
 * This system performs frustum culling of terrain chunks on the GPU using a compute shader.
 * Instead of rendering all terrain geometry in one draw call, it:
 * 1. Divides the terrain into chunks (64x64 grid)
 * 2. Runs a compute shader to test each chunk against the camera frustum
 * 3. Writes indirect draw commands for only visible chunks
 * 4. Issues a multi-draw-indirect call to render only visible chunks
 * 
 * This dramatically reduces vertex shader workload by skipping vertices for off-screen chunks.
 */

// Chunk data structure that gets uploaded to GPU
// Must match the GLSL struct in Flux_TerrainCulling.comp
struct Flux_TerrainChunkData
{
	Zenith_Maths::Vector4 m_xAABBMin;  // xyz = min corner, w = padding
	Zenith_Maths::Vector4 m_xAABBMax;  // xyz = max corner, w = padding
	uint32_t m_uFirstIndex;            // Starting index in the index buffer
	uint32_t m_uIndexCount;            // Number of indices to draw for this chunk
	uint32_t m_uVertexOffset;          // Base vertex offset
	uint32_t m_uPadding;               // Struct padding for alignment
};

// Frustum plane structure for GPU upload
// Must match the GLSL struct in Flux_TerrainCulling.comp
struct Flux_FrustumPlaneGPU
{
	Zenith_Maths::Vector4 m_xNormalAndDistance;  // xyz = normal, w = distance
};

class Flux_TerrainCulling
{
public:
	static void Initialise();
	static void Shutdown();

	/**
	 * Dispatch the terrain culling compute shader
	 * This tests all terrain chunks against the camera frustum and writes
	 * indirect draw commands for visible chunks
	 * 
	 * @param xViewProjMatrix The camera's view-projection matrix
	 */
	static void DispatchCulling(const Zenith_Maths::Matrix4& xViewProjMatrix);

	/**
	 * Get the indirect draw buffer for rendering
	 * This buffer contains VkDrawIndexedIndirectCommand structs written by the compute shader
	 */
	static const Flux_IndirectBuffer& GetIndirectDrawBuffer();

	/**
	 * Get the maximum number of draw commands (= 4096, the theoretical maximum)
	 */
	static uint32_t GetMaxDrawCount();

private:
	static void BuildChunkData();
	static void ExtractFrustumPlanes(const Zenith_Maths::Matrix4& xViewProjMatrix, Flux_FrustumPlaneGPU* pxOutPlanes);
};
