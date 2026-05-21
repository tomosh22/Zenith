#pragma once

#include "Flux/Flux.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

// Phase 9: state + behaviour for deferred-shading subsystem.
class Flux_DeferredShadingImpl
{
public:
	Flux_DeferredShadingImpl() = default;
	~Flux_DeferredShadingImpl() = default;

	Flux_DeferredShadingImpl(const Flux_DeferredShadingImpl&) = delete;
	Flux_DeferredShadingImpl& operator=(const Flux_DeferredShadingImpl&) = delete;

	void Initialise();
	void BuildPipelines();
	void SetupRenderGraph(Flux_RenderGraph& xGraph);

	Flux_Shader   m_xShader;
	Flux_Pipeline m_xPipeline;
};
