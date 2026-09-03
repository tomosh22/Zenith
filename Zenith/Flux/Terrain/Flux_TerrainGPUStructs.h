#pragma once

// GPU-layout structs for terrain culling + rendering.
//
// Relocated here (Wave-18) from Zenith_TerrainComponent.h as part of the
// ownership-relocation that decouples the ECS terrain component from Flux.
// These describe the exact byte layout the terrain culling compute shader and
// vertex shader consume, so they live on the Flux side next to the streaming
// state that owns them. The names keep their historical Zenith_ prefix because
// downstream code (Flux_TerrainStreamingManager, Zenith_TerrainComponent.cpp)
// references them by those names and the .zscen byte format is unaffected
// (these structs are never serialised — they're rebuilt from residency state).

#include "Maths/Zenith_Maths.h"
#include "Maths/Zenith_FrustumCulling.h"
#include "Flux/Terrain/Flux_TerrainConfig.h"
#include "Flux/Terrain/Flux_TerrainShadowCull.h"

// LOD data for a single level
struct Zenith_TerrainLODData
{
	uint32_t m_uFirstIndex;    // Starting index in the index buffer for this LOD
	uint32_t m_uIndexCount;    // Number of indices to draw for this LOD
	uint32_t m_uVertexOffset;  // Base vertex offset (always 0 for combined mesh)
	float m_fMaxDistance;      // Maximum distance (squared) at which this LOD is used
};

// Chunk data structure that gets uploaded to GPU
// Must match the struct in Flux_TerrainCulling.slang
struct Zenith_TerrainChunkData
{
	Zenith_Maths::Vector4 m_xAABBMin;                            // xyz = min corner, w = padding
	Zenith_Maths::Vector4 m_xAABBMax;                            // xyz = max corner, w = padding
	Zenith_TerrainLODData m_axLODs[Flux_TerrainConfig::LOD_COUNT]; // LOD mesh data (HIGH=0, LOW=1)
};

// Frustum plane structure for GPU upload
struct Zenith_FrustumPlaneGPU
{
	Zenith_Maths::Vector4 m_xNormalAndDistance;  // xyz = normal, w = distance
};

// Camera culling data structure for GPU upload
struct Zenith_CameraDataGPU
{
	Zenith_FrustumPlaneGPU m_axFrustumPlanes[6];  // 6 frustum planes
	Zenith_Maths::Vector4 m_xCameraPosition;      // xyz = camera position, w = padding
};

// ========== Per-cascade shadow culling data (GPU upload) ==========
// Must match ShadowCullBufferLayout in Flux_TerrainCulling.slang. Cascade-major:
// cascade c's six planes are m_axFrustumPlanes[c*6 .. c*6+5] in the same
// Left/Right/Bottom/Top/Near/Far order the camera block uses, extracted by the
// same Zenith_Frustum helper from the cascade's snapped ortho view-proj. A
// cascade at or past m_xParams.x is INACTIVE this frame (shadows off, or the
// view slot not staged): its planes are left zero and the cull skips it, so
// its slot stays at the all-zero state the reset pass wrote.
//   m_xParams.x = active cascade count (0 .. uFLUX_TERRAIN_SHADOW_CULL_VIEWS)
//   m_xParams.y = force-LOW-LOD-from cascade (uFLUX_TERRAIN_SHADOW_FORCE_LOW_NEVER = never)
//   m_xParams.z / .w = 0 (reserved)
struct Zenith_TerrainShadowCullGPU
{
	Zenith_FrustumPlaneGPU m_axFrustumPlanes[uFLUX_TERRAIN_SHADOW_CULL_VIEWS * 6];
	Zenith_Maths::UVector4 m_xParams;
};
static_assert(sizeof(Zenith_TerrainShadowCullGPU) == uFLUX_TERRAIN_SHADOW_CULL_VIEWS * 6u * 16u + 16u,
	"Zenith_TerrainShadowCullGPU must be a dense float4 array + one uint4 (std140-clean)");

// Fills the per-cascade cull block from the cascades' view-proj matrices.
// paxCascadeViewProj must hold at least uActiveCascades matrices (only those are
// read); the rest of the block is zeroed. uActiveCascades is clamped to the slot
// count. PURE (no engine access) so the derivation is unit-testable: the planes
// written for cascade c are exactly Zenith_Frustum::ExtractFromViewProjection
// of paxCascadeViewProj[c] — the same extraction, and therefore the same
// inward-normal / [0,1]-depth convention, the camera block goes through.
inline void Flux_BuildTerrainShadowCullData(const Zenith_Maths::Matrix4* paxCascadeViewProj,
	uint32_t uActiveCascades, uint32_t uForceLowFromCascade, Zenith_TerrainShadowCullGPU& xOut)
{
	if (uActiveCascades > uFLUX_TERRAIN_SHADOW_CULL_VIEWS) uActiveCascades = uFLUX_TERRAIN_SHADOW_CULL_VIEWS;

	for (uint32_t u = 0; u < uFLUX_TERRAIN_SHADOW_CULL_VIEWS * 6u; ++u)
	{
		xOut.m_axFrustumPlanes[u].m_xNormalAndDistance = Zenith_Maths::Vector4(0.0f);
	}
	for (uint32_t uCascade = 0; uCascade < uActiveCascades; ++uCascade)
	{
		Zenith_Frustum xFrustum;
		xFrustum.ExtractFromViewProjection(paxCascadeViewProj[uCascade]);
		for (uint32_t uPlane = 0; uPlane < 6u; ++uPlane)
		{
			xOut.m_axFrustumPlanes[uCascade * 6u + uPlane].m_xNormalAndDistance = Zenith_Maths::Vector4(
				xFrustum.m_axPlanes[uPlane].m_xNormal,
				xFrustum.m_axPlanes[uPlane].m_fDistance);
		}
	}
	xOut.m_xParams = Zenith_Maths::UVector4(uActiveCascades, uForceLowFromCascade, 0u, 0u);
}
