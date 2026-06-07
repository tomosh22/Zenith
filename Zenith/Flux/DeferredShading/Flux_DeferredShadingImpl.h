#pragma once

#include "Flux/Flux.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

class Flux_GraphicsImpl;
class Flux_HDRImpl;
class Flux_ShadowsImpl;
class Flux_IBLImpl;
class Flux_SSRImpl;
class Flux_SSGIImpl;
class Flux_DynamicLightsImpl;
class Flux_LightClusteringImpl;

// Phase 9: state + behaviour for deferred-shading subsystem.
class Flux_DeferredShadingImpl
{
public:
	Flux_DeferredShadingImpl() = default;
	~Flux_DeferredShadingImpl() = default;

	Flux_DeferredShadingImpl(const Flux_DeferredShadingImpl&) = delete;
	Flux_DeferredShadingImpl& operator=(const Flux_DeferredShadingImpl&) = delete;

	void Initialise(Flux_GraphicsImpl& xFluxGraphics, Flux_HDRImpl& xHDR, Flux_ShadowsImpl& xShadows,
		Flux_IBLImpl& xIBL, Flux_SSRImpl& xSSR, Flux_SSGIImpl& xSSGI,
		Flux_DynamicLightsImpl& xDynamicLights, Flux_LightClusteringImpl& xLightClustering);
	void Shutdown();
	void BuildPipelines();
	void SetupRenderGraph(Flux_RenderGraph& xGraph);

	Flux_Shader   m_xShader;
	Flux_Pipeline m_xPipeline;

	Flux_GraphicsImpl*        m_pxFluxGraphics = nullptr;
	Flux_HDRImpl*             m_pxHDR = nullptr;
	Flux_ShadowsImpl*         m_pxShadows = nullptr;
	Flux_IBLImpl*             m_pxIBL = nullptr;
	Flux_SSRImpl*             m_pxSSR = nullptr;
	Flux_SSGIImpl*            m_pxSSGI = nullptr;
	Flux_DynamicLightsImpl*   m_pxDynamicLights = nullptr;
	Flux_LightClusteringImpl* m_pxLightClustering = nullptr;
};
