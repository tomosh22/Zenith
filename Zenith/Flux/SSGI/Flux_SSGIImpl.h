#pragma once

#include "Flux/SSGI/Flux_SSGI.h"
#include "Flux/Flux.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

class Flux_RenderGraph;

// Phase 7f: per-Engine state for SSGI subsystem.
class Flux_SSGIImpl
{
public:
	Flux_SSGIImpl() = default;
	~Flux_SSGIImpl() = default;

	Flux_SSGIImpl(const Flux_SSGIImpl&) = delete;
	Flux_SSGIImpl& operator=(const Flux_SSGIImpl&) = delete;

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
