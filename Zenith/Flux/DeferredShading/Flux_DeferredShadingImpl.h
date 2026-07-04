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
	void SetupRenderGraph(Flux_RenderGraph& xGraph);

	Flux_Shader   m_xShader;
	// One pipeline per view-shading mode (Stage 3a). Selected at record time from
	// the recording pass's view slot. Empty spec tables → the variants are
	// byte-identical until Stage 3b bakes the per-mode permission mask.
	Flux_Pipeline m_axPipelines[FLUX_VIEW_SHADING_MODE_COUNT];
};
