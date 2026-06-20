#pragma once

#include "Flux/Flux.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

class Flux_RenderGraph;

// State + behaviour for the Present subsystem — the final-frame blit that copies
// the Flux "final render target" into the acquired swapchain backbuffer.
//
// This is the backend-NEUTRAL owner of the present shader + pipeline (formerly
// held directly by Zenith_Vulkan_Swapchain). The genuinely API-specific present
// orchestration — swapchain image acquire, render-pass begin against the
// backbuffer, queue submit, semaphores, presentKHR, screenshot readback —
// stays in the backend swapchain, which calls RecordBlit() to record the
// fullscreen copy through the neutral Flux_CommandBuffer recorder. So when a
// second backend (D3D12, Metal, ...) is implemented it reuses this pipeline +
// recording verbatim; only the acquire/submit/present plumbing is per-backend.
//
// Cross-subsystem deps (FluxGraphics for the final RT + quad mesh + G-buffer
// SRVs, FluxSwapchain for the backbuffer format) are reached via g_xEngine at
// point of use, mirroring the other Flux feature impls.
class Flux_PresentImpl
{
public:
	Flux_PresentImpl() = default;
	~Flux_PresentImpl() = default;

	Flux_PresentImpl(const Flux_PresentImpl&) = delete;
	Flux_PresentImpl& operator=(const Flux_PresentImpl&) = delete;

	void Initialise();
	void Shutdown();
	void BuildPipelines();
	void SetupRenderGraph(Flux_RenderGraph& xGraph);

	// Records the fullscreen final-RT -> backbuffer copy into the supplied
	// command buffer. Called by the backend swapchain's present step INSIDE an
	// already-begun render pass targeting the acquired backbuffer. Uses only the
	// neutral Flux_CommandBuffer recorder surface, so it is backend-agnostic.
	void RecordBlit(Flux_CommandBuffer& xCmd);

	Flux_Shader   m_xPresentShader;
	Flux_Pipeline m_xPresentPipeline;
};
