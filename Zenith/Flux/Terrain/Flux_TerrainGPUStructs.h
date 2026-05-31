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
#include "Flux/Terrain/Flux_TerrainConfig.h"

// LOD data for a single level
struct Zenith_TerrainLODData
{
	uint32_t m_uFirstIndex;    // Starting index in the index buffer for this LOD
	uint32_t m_uIndexCount;    // Number of indices to draw for this LOD
	uint32_t m_uVertexOffset;  // Base vertex offset (always 0 for combined mesh)
	float m_fMaxDistance;      // Maximum distance (squared) at which this LOD is used
};

// Chunk data structure that gets uploaded to GPU
// Must match the GLSL struct in Flux_TerrainCulling.comp
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
