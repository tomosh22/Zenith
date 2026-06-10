#pragma once

#include "Flux/Flux.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

class Flux_RenderGraph;

// Phase 9: state + behaviour for SSAO subsystem.
//
// Cross-subsystem dependencies (FluxGraphics/Swapchain/HDR) are reached via
// g_xEngine at point of use. The non-capturing fn-pointer trampolines (the
// Execute* graph callbacks, the DebugGet* texture callbacks, and the
// ZENITH_TOOLS hot-reload callback) cannot capture state, so they re-enter via
// g_xEngine.SSAO() to reach this singleton instance.
class Flux_SSAOImpl
{
public:
	Flux_SSAOImpl() = default;
	~Flux_SSAOImpl() = default;

	Flux_SSAOImpl(const Flux_SSAOImpl&) = delete;
	Flux_SSAOImpl& operator=(const Flux_SSAOImpl&) = delete;

	void Initialise();
	void Shutdown();
	void BuildPipelines();
	void SetupRenderGraph(Flux_RenderGraph& xGraph);

	// Attachment accessors — resolve through the graph's transient slot. Were
	// file-static helpers reaching for g_xEngine.SSAO(); now non-static members
	// reading this->m_pxGraph / this->m_x*Handle (mirror HiZ GetHiZBuffer). Kept
	// public (like the SSR transient accessors) because the SSAO Execute* /
	// DebugGet* trampolines re-enter via g_xEngine.SSAO() and call them — unlike
	// HiZ's GetHiZBuffer, which is only reached from other instance methods.
	Flux_RenderAttachment& GetRawOcclusion();
	Flux_RenderAttachment& GetBlurred();

	Flux_Shader   m_xGenerateShader;
	Flux_Shader   m_xBlurShader;
	Flux_Shader   m_xUpsampleShader;
	Flux_Pipeline m_xGeneratePipeline;
	Flux_Pipeline m_xBlurPipeline;
	Flux_Pipeline m_xUpsamplePipeline;

	Flux_TransientHandle m_xRawOcclusionHandle;
	Flux_TransientHandle m_xBlurredHandle;
	Flux_RenderGraph*    m_pxGraph = nullptr;
};
