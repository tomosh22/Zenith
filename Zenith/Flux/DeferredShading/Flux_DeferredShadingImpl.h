#pragma once

#include "Flux/Flux.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Flux/RenderViews/Flux_RenderViews.h"   // FluxViewShadingMode (Stage 3a per-mode pipeline variants)

// Phase 9: state + behaviour for deferred-shading subsystem.
class Flux_DeferredShadingImpl
{
public:
	Flux_DeferredShadingImpl() = default;
	~Flux_DeferredShadingImpl() = default;

	Flux_DeferredShadingImpl(const Flux_DeferredShadingImpl&) = delete;
	Flux_DeferredShadingImpl& operator=(const Flux_DeferredShadingImpl&) = delete;

	void Initialise();
	void Shutdown();
	void BuildPipelines();
	// Walks Flux_RenderViewRegistry::ForEachActiveFullPipelineView and declares one
	// "Apply Lighting" pass per active full-pipeline view, in ascending slot order.
	void SetupRenderGraph(Flux_RenderGraph& xGraph);

	// One view's whole lighting declaration, called once per active full-pipeline
	// view by SetupRenderGraph's walk. Everything it declares is either indexed by
	// uSlot or genuinely shared (CSM array, cluster buffers, IBL) — there is no
	// per-slot branch inside it. The Flux_RenderView the walk supplies is accepted
	// and UNUSED on purpose: nothing about the view discriminates the pass, and the
	// flag word that changes the shading is Flux_RenderView::m_xConstants.m_uViewFlags,
	// which ExecuteApplyLighting reads at RECORD time from the recording pass's slot.
	void SetupViewPasses(Flux_RenderGraph& xGraph, u_int uSlot, const Flux_RenderView& xView);

	Flux_Shader   m_xShader;
	// One pipeline per view-shading mode (Stage 3a). Selected at record time from
	// the recording pass's view slot. Empty spec tables → the variants are
	// byte-identical until Stage 3b bakes the per-mode permission mask.
	Flux_Pipeline m_axPipelines[FLUX_VIEW_SHADING_MODE_COUNT];
};
