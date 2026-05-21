#pragma once

#include "Flux/Flux.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

class Flux_RenderGraph;

enum SSGI_DebugMode : u_int
{
	SSGI_DEBUG_NONE = 0,
	SSGI_DEBUG_RAY_DIRECTIONS,
	SSGI_DEBUG_HIT_POSITIONS,
	SSGI_DEBUG_CONFIDENCE,
	SSGI_DEBUG_FINAL_RESULT,
	SSGI_DEBUG_COUNT
};

// Phase 9: state + behaviour for SSGI subsystem.
class Flux_SSGIImpl
{
public:
	Flux_SSGIImpl() = default;
	~Flux_SSGIImpl() = default;

	Flux_SSGIImpl(const Flux_SSGIImpl&) = delete;
	Flux_SSGIImpl& operator=(const Flux_SSGIImpl&) = delete;

	void Initialise();
	void Shutdown();
	void BuildPipelines();

	void SetupRenderGraph(Flux_RenderGraph& xGraph);
	void ApplyDenoiseSelectionToGraph(Flux_RenderGraph& xGraph);

	Flux_TransientHandle GetSSGIHandle() const { return m_xCommittedSSGIHandle; }
	Flux_ShaderResourceView& GetSSGISRV();
	bool IsEnabled() const;
	bool IsInitialised() const { return m_bInitialised; }

	Flux_RenderAttachment& GetRawResultAttachment();
	Flux_RenderAttachment& GetResolvedAttachment();
	Flux_RenderAttachment& GetDenoisedAttachment();

	Flux_TransientHandle m_xRawResultHandle;
	Flux_TransientHandle m_xResolvedHandle;
	Flux_TransientHandle m_xDenoiseHHandle;
	Flux_TransientHandle m_xDenoisedHandle;
	Flux_RenderGraph*    m_pxGraph = nullptr;

	u_int                m_uRayMarchResolutionDivisor = 4u;
	u_int                m_uLastResolutionDivisor     = 4u;

	bool                 m_bInitialised = false;

	Flux_Shader          m_xRayMarchShader;
	Flux_Shader          m_xUpsampleShader;
	Flux_Shader          m_xDenoiseHShader;
	Flux_Shader          m_xDenoiseVShader;
	Flux_Pipeline        m_xRayMarchPipeline;
	Flux_Pipeline        m_xUpsamplePipeline;
	Flux_Pipeline        m_xDenoiseHPipeline;
	Flux_Pipeline        m_xDenoiseVPipeline;

	Flux_PassHandle      m_xDenoisePassH;
	Flux_PassHandle      m_xDenoisePassV;
	bool                 m_bLastDenoiseEnabled = true;
	Flux_TransientHandle m_xCommittedSSGIHandle;
};
