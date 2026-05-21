#pragma once

#include "Flux/Terrain/Flux_Terrain.h"
#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "Collections/Zenith_Vector.h"

class Zenith_TerrainComponent;

// Phase 7h: per-Engine state for Terrain subsystem.
class Flux_TerrainImpl
{
public:
	Flux_TerrainImpl() = default;
	~Flux_TerrainImpl() = default;

	Flux_TerrainImpl(const Flux_TerrainImpl&) = delete;
	Flux_TerrainImpl& operator=(const Flux_TerrainImpl&) = delete;

	// Per-frame list of terrain components contributing to the current draw.
	Zenith_Vector<Zenith_TerrainComponent*> m_xTerrainComponentsToRender;

	// GBuffer / shadow pass pipelines + shaders.
	Flux_Shader   m_xTerrainGBufferShader;
	Flux_Pipeline m_xTerrainGBufferPipeline;
	Flux_Shader   m_xTerrainShadowShader;
	Flux_Pipeline m_xTerrainShadowPipeline;
	Flux_Pipeline m_xTerrainWireframePipeline;

	// Water pass pipeline + shader + assets.
	Flux_Shader   m_xWaterShader;
	Flux_Pipeline m_xWaterPipeline;
	TextureHandle m_xWaterNormalTexture;
	uint32_t      m_uWaterDisplacementTexHandle = UINT32_MAX;

	// Fallback splatmap (used when terrain instance lacks one).
	TextureHandle m_xFallbackSplatmap;

	// Culling compute resources.
	Flux_Pipeline m_xCullingPipeline;
	Flux_Shader   m_xCullingShader;
	Flux_RootSig  m_xCullingRootSig;

	// Reset-counters compute resources.
	Flux_Pipeline m_xResetCountersPipeline;
	Flux_Shader   m_xResetCountersShader;
	Flux_RootSig  m_xResetCountersRootSig;

	// Per-frame stats.
	uint32_t m_uFrameCounter       = 0;
	uint32_t m_uLastVisibleChunks  = 0;
	float    m_fCullingTimeMs      = 0.0f;
	float    m_fStreamingTimeMs    = 0.0f;

	// Terrain constants buffer (TerrainConstants GPU struct is .cpp-local).
	Flux_DynamicConstantBuffer m_xTerrainConstantsBuffer;
};
