#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

class Flux_RenderGraph;

enum SSR_DebugMode : u_int
{
	SSR_DEBUG_NONE = 0,
	SSR_DEBUG_RAY_DIRECTIONS,
	SSR_DEBUG_SCREEN_DIRECTIONS,
	SSR_DEBUG_HIT_POSITIONS,
	SSR_DEBUG_REFLECTION_UVS,
	SSR_DEBUG_CONFIDENCE,
	SSR_DEBUG_DEPTH_COMPARISON,
	SSR_DEBUG_EDGE_FADE,
	SSR_DEBUG_MARCH_DISTANCE,
	SSR_DEBUG_FINAL_RESULT,
	SSR_DEBUG_ROUGHNESS,
	SSR_DEBUG_WORLD_NORMAL_Y,
	SSR_DEBUG_RAY_COUNT,
	SSR_DEBUG_COUNT
};

// Phase 9: state + behaviour for SSR subsystem.
class Flux_SSRImpl
{
public:
	Flux_SSRImpl() = default;
	~Flux_SSRImpl() = default;

	Flux_SSRImpl(const Flux_SSRImpl&) = delete;
	Flux_SSRImpl& operator=(const Flux_SSRImpl&) = delete;

	void Initialise();
	void Shutdown();
	void BuildPipelines();

	void SetupRenderGraph(Flux_RenderGraph& xGraph);
	void ApplyBlurSelectionToGraph(Flux_RenderGraph& xGraph);

	Flux_TransientHandle GetReflectionHandle() const { return m_xCommittedReflectionHandle; }
	Flux_ShaderResourceView& GetReflectionSRV();
	bool IsEnabled() const;
	bool IsInitialised() const { return m_bInitialised; }

	Flux_RenderAttachment& GetRayMarchAttachment();
	Flux_RenderAttachment& GetRayMarchAuxAttachment();
	Flux_RenderAttachment& GetUpsampledAttachment();
	Flux_RenderAttachment& GetUpsampledAuxAttachment();
	Flux_RenderAttachment& GetDenoiseHAttachment();
	Flux_RenderAttachment& GetDenoiseHConfAttachment();
	Flux_RenderAttachment& GetDenoiseVAttachment();

	Flux_TransientHandle m_xRayMarchHandle;
	Flux_TransientHandle m_xRayMarchAuxHandle;
	Flux_TransientHandle m_xUpsampledHandle;
	Flux_TransientHandle m_xUpsampledAuxHandle;
	Flux_TransientHandle m_xDenoiseHHandle;
	Flux_TransientHandle m_xDenoiseHConfHandle;
	Flux_TransientHandle m_xDenoiseVHandle;
	Flux_RenderGraph*    m_pxGraph = nullptr;

	bool                 m_bInitialised = false;

	Flux_Shader          m_xRayMarchShader;
	Flux_Shader          m_xUpsampleShader;
	Flux_Shader          m_xDenoiseHShader;
	Flux_Shader          m_xDenoiseVShader;
	Flux_Pipeline        m_xRayMarchPipeline;
	Flux_Pipeline        m_xUpsamplePipeline;
	Flux_Pipeline        m_xDenoiseHPipeline;
	Flux_Pipeline        m_xDenoiseVPipeline;

	Flux_DynamicConstantBuffer m_xSSRConstantsBuffer;

	Flux_PassHandle      m_xDenoiseHPass;
	Flux_PassHandle      m_xDenoiseVPass;
	bool                 m_bLastBlurEnabled = true;
	Flux_TransientHandle m_xCommittedReflectionHandle;
};
