#pragma once

#include "Flux/Flux.h"

class Flux_CommandList;
class Flux_VolumeFogImpl;
class FrameContext;
class Flux_RendererImpl;
class Flux_GraphicsImpl;
class Flux_ShadowsImpl;

// Phase 9: state + behaviour for RaymarchFog subsystem.
class Flux_RaymarchFogImpl
{
public:
	Flux_RaymarchFogImpl() = default;
	~Flux_RaymarchFogImpl() = default;

	Flux_RaymarchFogImpl(const Flux_RaymarchFogImpl&) = delete;
	Flux_RaymarchFogImpl& operator=(const Flux_RaymarchFogImpl&) = delete;

	void Initialise(Flux_VolumeFogImpl& xVolumeFog, FrameContext& xFrame, Flux_RendererImpl& xFluxRenderer, Flux_GraphicsImpl& xFluxGraphics, Flux_ShadowsImpl& xShadows);
	void BuildPipelines();
	void Reset();
	void Render(Flux_CommandList* pxCommandList);

	Flux_Shader   m_xShader;
	Flux_Pipeline m_xPipeline;

private:
	Flux_VolumeFogImpl* m_pxVolumeFog = nullptr;
	FrameContext* m_pxFrame = nullptr;
	Flux_RendererImpl* m_pxFluxRenderer = nullptr;
	Flux_GraphicsImpl* m_pxFluxGraphics = nullptr;
	Flux_ShadowsImpl* m_pxShadows = nullptr;
};
