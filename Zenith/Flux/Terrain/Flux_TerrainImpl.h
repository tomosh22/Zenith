#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "Collections/Zenith_Vector.h"
#include "Flux/Terrain/Flux_TerrainStreamingManagerImpl.h" // Wave 3: Flux_TerrainRenderRecord (by-value in the render list)

class Flux_DynamicConstantBuffer;

// Phase 9: state + behaviour for Terrain subsystem.
class Flux_TerrainImpl
{
public:
	Flux_TerrainImpl() = default;
	~Flux_TerrainImpl() = default;

	Flux_TerrainImpl(const Flux_TerrainImpl&) = delete;
	Flux_TerrainImpl& operator=(const Flux_TerrainImpl&) = delete;

	void Initialise();
	void BuildPipelines();

	void ReleaseAssetReferences();

	void Shutdown();
	void Reset();

	// Mirrors the other casters' contract (cascade index; the all-cascade ShadowMatrices
	// SSBO is in the persistent VIEW set, Phase 5.4). STUBBED — terrain does not currently
	// cast (the call in Flux_Shadows.cpp ExecuteShadowCascade is commented out); kept
	// signature-aligned so enabling it is a pure C++ change.
	void RenderToShadowMap(Flux_CommandBuffer& xCmdBuf, u_int uCascade);

	void SetupRenderGraph(Flux_RenderGraph& xGraph);

	void PreRenderUpdate(void* pUserData);

	Flux_Pipeline& GetShadowPipeline()                          { return m_xTerrainShadowPipeline; }
	Flux_DynamicConstantBuffer& GetTerrainConstantsBuffer()     { return m_xTerrainConstantsBuffer; }
	Flux_Pipeline& GetCullingPipeline()                         { return m_xCullingPipeline; }

	u_int& GetDebugMode();
	bool& GetWireframeMode();

	// Lazily-built 1x1 fallback splatmap SRV. Public so the ExecuteGBuffer
	// graph trampoline (which re-acquires the singleton) can reach it.
	const Flux_ShaderResourceView& GetFallbackSplatmapSRV();

	// Per-frame list of terrain components contributing to the current draw.
	// Wave 3: per-frame terrain render records (Flux state + asset handles), gathered
	// EC-side via g_pfnZenithTerrainGather — Flux_Terrain names no Zenith_TerrainComponent.
	Zenith_Vector<Flux_TerrainRenderRecord> m_xTerrainRenderRecords;

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
	uint32_t m_uLastVisibleChunks  = 0;
	float    m_fCullingTimeMs      = 0.0f;
	float    m_fStreamingTimeMs    = 0.0f;

	// Terrain constants buffer (TerrainConstants GPU struct is .cpp-local).
	Flux_DynamicConstantBuffer m_xTerrainConstantsBuffer;
};
