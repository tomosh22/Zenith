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
	// TAA velocity variant (5-attachment: 4 core MRTs + velocity). Selected at record time INSTEAD
	// of the base pipeline when the velocity latch is on (IsVelocityMRTActive).
	Flux_Shader   m_xTerrainGBufferVelocityShader;
	Flux_Pipeline m_xTerrainGBufferVelocityPipeline;
	Flux_Shader   m_xTerrainShadowShader;
	Flux_Pipeline m_xTerrainShadowPipeline;
	// Wireframe twins of the two G-buffer pipelines above. Wireframe is rasterizer state
	// (polygon mode), orthogonal to the attachment set, so the record-time choice is a full
	// 2x2 over (velocity latch, wireframe debug var) — see Flux_TerrainSelectGBufferVariant.
	// The VELOCITY twin has to exist because TAA ships ON: without it the wireframe branch is
	// unreachable and the debug toggle does nothing in every default run.
	Flux_Pipeline m_xTerrainWireframePipeline;          // 4 attachments, base G-buffer shader
	Flux_Pipeline m_xTerrainWireframeVelocityPipeline;  // 5 attachments, velocity shader

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

};

// Byte size of the TerrainConstants CB. The struct itself stays .cpp-local (it
// is pinned there against the reflected layout), but Zenith_TerrainComponent has
// to size one PER TERRAIN, so the size -- and only the size -- is published.
uint32_t Flux_TerrainConstantsBufferBytes();
