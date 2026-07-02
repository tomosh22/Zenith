#pragma once

#include "Flux/Flux.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Flux/RenderViews/Flux_RenderViews.h"

class Flux_RenderGraph;

// Phase 9: state + behaviour for SSAO subsystem.
//
// Cross-subsystem dependencies (FluxGraphics/Swapchain/HDR) are reached via
// g_xEngine at point of use. The non-capturing fn-pointer trampolines (the
// Execute* graph callbacks, the DebugGet* texture callbacks, and the
// ZENITH_TOOLS hot-reload callback) cannot capture state, so they re-enter via
// g_xEngine.SSAO() to reach this singleton instance.
//
// S5b: the raw/blurred occlusion transients are per-render-view — slot 0
// (main) at half swapchain res as before; the preview view at 256² when
// active. uViewSlot defaults keep single-view callers unchanged.
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
	// reading this->m_pxGraph / this->m_ax*Handles (mirror HiZ GetHiZBuffer).
	// Kept public (like the SSR transient accessors) because the SSAO Execute*
	// / DebugGet* trampolines re-enter via g_xEngine.SSAO() and call them —
	// unlike HiZ's GetHiZBuffer, which is only reached from other instance
	// methods.
	Flux_RenderAttachment& GetRawOcclusion(u_int uViewSlot = kuFluxViewSlotMain);
	Flux_RenderAttachment& GetBlurred(u_int uViewSlot = kuFluxViewSlotMain);

	Flux_Shader   m_xGenerateShader;
	Flux_Shader   m_xBlurShader;
	Flux_Pipeline m_xGeneratePipeline;
	Flux_Pipeline m_xBlurPipeline;

	Flux_TransientHandle m_axRawOcclusionHandles[FLUX_MAX_RENDER_VIEWS];
	Flux_TransientHandle m_axBlurredHandles[FLUX_MAX_RENDER_VIEWS];
	Flux_RenderGraph*    m_pxGraph = nullptr;

private:
	// Per-view transients + Generate/Blur pass pair (S5b): called for the main
	// view at swapchain dims, then for the preview view at
	// kuFLUX_PREVIEW_VIEW_SIZE² only while it is active — so the main path
	// stays byte-equivalent.
	void SetupViewPasses(Flux_RenderGraph& xGraph, u_int uViewSlot, u_int uWidth, u_int uHeight);
};
