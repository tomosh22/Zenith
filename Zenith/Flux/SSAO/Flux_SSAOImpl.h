#pragma once

#include "Flux/Flux.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

class Flux_RenderGraph;

// Phase 7d: per-Engine state for SSAO subsystem.
class Flux_SSAOImpl
{
public:
	Flux_SSAOImpl() = default;
	~Flux_SSAOImpl() = default;

	Flux_SSAOImpl(const Flux_SSAOImpl&) = delete;
	Flux_SSAOImpl& operator=(const Flux_SSAOImpl&) = delete;

	Flux_Shader   m_xGenerateShader;
	Flux_Shader   m_xBlurShader;
	Flux_Shader   m_xUpsampleShader;
	Flux_Pipeline m_xGeneratePipeline;
	Flux_Pipeline m_xBlurPipeline;
	Flux_Pipeline m_xUpsamplePipeline;

	Flux_TransientHandle m_xRawOcclusionHandle;
	Flux_TransientHandle m_xBlurredHandle;
	Flux_RenderGraph*    m_pxGraph = nullptr;
	// SSAOGenerateConstants / SSAOBlurConstants are .cpp-local types --
	// their backing storage stays file-static there.
};
