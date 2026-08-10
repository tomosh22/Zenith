#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_ScreenSpaceEffectBase.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Flux/RenderViews/Flux_RenderViews.h"

class Flux_RenderGraph;

// SSAO has three graph-shaping choices that must move as one committed value:
// blur off/on selects raw vs filtered output, the blur algorithm selects the
// legacy 2-D pass vs the separable H+V pair, and the divisor sizes every SSAO
// transient. DeferredShading reads only the handle committed with this value.
struct Flux_SSAOSelection
{
	bool  m_bBlurEnabled;
	bool  m_bSeparableBlur;
	u_int m_uResolutionDivisor;
	bool operator==(const Flux_SSAOSelection&) const = default;
};

// Phase 9: state + behaviour for SSAO subsystem.
//
// Cross-subsystem dependencies (FluxGraphics/Swapchain/HDR) are reached via
// g_xEngine at point of use. The non-capturing fn-pointer trampolines (the
// Execute* graph callbacks, the DebugGet* texture callbacks, and the
// ZENITH_TOOLS hot-reload callback) cannot capture state, so they re-enter via
// g_xEngine.SSAO() to reach this singleton instance.
//
// S5b: the raw/filtered occlusion chain and committed output selector are
// per-render-view. Slot 0 follows the swapchain and preview follows its fixed
// view size; both use the committed half/quarter divisor. uViewSlot defaults
// keep single-view callers unchanged.
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
	void ApplySelectionToGraph(Flux_RenderGraph& xGraph);

	// Attachment accessors — resolve through the graph's transient slot. Were
	// file-static helpers reaching for g_xEngine.SSAO(); now non-static members
	// reading this->m_pxGraph / this->m_ax*Handles (mirror HiZ GetHiZBuffer).
	// Kept public (like the SSR transient accessors) because the SSAO Execute*
	// / DebugGet* trampolines re-enter via g_xEngine.SSAO() and call them —
	// unlike HiZ's GetHiZBuffer, which is only reached from other instance
	// methods.
	// Only the two the record callbacks actually bind. The legacy-filtered and
	// separable-filtered targets are reached exclusively through the committed
	// selector below — no per-target accessor exists, so no caller can bind a
	// handle the graph did not declare.
	Flux_RenderAttachment& GetRawOcclusion(u_int uViewSlot = kuFluxViewSlotMain);
	Flux_RenderAttachment& GetSeparableH(u_int uViewSlot = kuFluxViewSlotMain);
	Flux_TransientHandle GetOutputHandle(u_int uViewSlot = kuFluxViewSlotMain) const { return m_axOutputSelectors[uViewSlot].GetCommittedHandle(); }
	Flux_ShaderResourceView& GetOutputSRV(u_int uViewSlot = kuFluxViewSlotMain);

	Flux_Shader   m_xGenerateShader;
	Flux_Shader   m_xBlurShader;
	Flux_Shader   m_xBlurHShader;
	Flux_Shader   m_xBlurVShader;
	Flux_Pipeline m_axGeneratePipelines[2];
	Flux_Pipeline m_xBlurPipeline;
	Flux_Pipeline m_xBlurHPipeline;
	Flux_Pipeline m_xBlurVPipeline;

	Flux_TransientHandle m_axRawOcclusionHandles[FLUX_MAX_RENDER_VIEWS];
	Flux_TransientHandle m_axLegacyBlurredHandles[FLUX_MAX_RENDER_VIEWS];
	Flux_TransientHandle m_axSeparableHHandles[FLUX_MAX_RENDER_VIEWS];
	Flux_TransientHandle m_axBlurredHandles[FLUX_MAX_RENDER_VIEWS];
	Flux_CommittedHandleSelector<Flux_SSAOSelection> m_axOutputSelectors[FLUX_MAX_RENDER_VIEWS];
	Flux_RenderGraph*    m_pxGraph = nullptr;

private:
	// Per-view raw + legacy/separable filter chains (S5b): called for the main
	// view at swapchain dims, then for the preview view at
	// kuFLUX_PREVIEW_VIEW_SIZE² only while it is active.
	void SetupViewPasses(
		Flux_RenderGraph& xGraph,
		u_int uViewSlot,
		u_int uWidth,
		u_int uHeight,
		const Flux_SSAOSelection& xSelection);
};
