#pragma once

#include "Flux/SSR/Flux_SSR.h"
#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

class Flux_RenderGraph;

// Phase 7f: per-Engine state for SSR subsystem.
// SSRConstants / SSRDenoiseConstants are .cpp-local POD; their backing
// stays file-static there.
class Flux_SSRImpl
{
public:
	Flux_SSRImpl() = default;
	~Flux_SSRImpl() = default;

	Flux_SSRImpl(const Flux_SSRImpl&) = delete;
	Flux_SSRImpl& operator=(const Flux_SSRImpl&) = delete;

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
