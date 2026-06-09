#pragma once

#include "Flux/Flux.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

class Flux_CommandList;
class Flux_RenderGraph;

class Flux_VolumeFogImpl;
class Flux_GodRaysFogImpl;
class Flux_RaymarchFogImpl;
class Flux_FroxelFogImpl;
class Flux_HDRImpl;
class Flux_GraphicsImpl;
class Flux_RendererImpl;
class Flux_ShadowsImpl;
class FrameContext;

// Phase 9: state + behaviour for top-level Fog orchestrator subsystem.
class Flux_FogImpl
{
public:
	Flux_FogImpl() = default;
	~Flux_FogImpl() = default;

	Flux_FogImpl(const Flux_FogImpl&) = delete;
	Flux_FogImpl& operator=(const Flux_FogImpl&) = delete;

	void Initialise(Flux_VolumeFogImpl& xVolumeFog, Flux_GodRaysFogImpl& xGodRaysFog,
		Flux_RaymarchFogImpl& xRaymarchFog, Flux_FroxelFogImpl& xFroxelFog,
		Flux_HDRImpl& xHDR, Flux_GraphicsImpl& xFluxGraphics, Flux_RendererImpl& xFluxRenderer,
		Flux_ShadowsImpl& xShadows, FrameContext& xFrame);
	void BuildPipelines();
	void Reset();

	void SetupRenderGraph(Flux_RenderGraph& xGraph);

	void ApplyTechniqueSelectionToGraph(Flux_RenderGraph& xGraph);

	// NOTE: the bespoke game-override path (SetExternallyOverridden /
	// IsExternallyOverridden / ReapplyOverrideToCurrentGraph / DisableAllFogPasses
	// / m_bExternallyOverridden) was removed. A game disables engine fog generically
	// via the render graph's force-disable overlay — SetOwnerForceDisabled("Fog")
	// masks all 6 fog passes (owner "Fog" = this feature's setup-step name) without
	// touching their base enable bits. ApplyTechniqueSelectionToGraph keeps the
	// base bits current; lifting the override restores the active technique exactly.

	Flux_PassHandle m_xSimpleFogPass;
	Flux_PassHandle m_xFroxelInjectPass;
	Flux_PassHandle m_xFroxelLightPass;
	Flux_PassHandle m_xFroxelApplyPass;
	Flux_PassHandle m_xRaymarchPass;
	Flux_PassHandle m_xGodRaysPass;

	u_int           m_uLastFogTechnique = UINT32_MAX;

	Flux_Shader     m_xShader;
	Flux_Pipeline   m_xPipeline;

	// Injected engine-infra / sibling-subsystem dependencies (de-globalization).
	Flux_VolumeFogImpl*   m_pxVolumeFog    = nullptr;
	Flux_GodRaysFogImpl*  m_pxGodRaysFog   = nullptr;
	Flux_RaymarchFogImpl* m_pxRaymarchFog  = nullptr;
	Flux_FroxelFogImpl*   m_pxFroxelFog    = nullptr;
	Flux_HDRImpl*         m_pxHDR          = nullptr;
	Flux_GraphicsImpl*    m_pxFluxGraphics = nullptr;
	Flux_RendererImpl*    m_pxFluxRenderer = nullptr;
	Flux_ShadowsImpl*     m_pxShadows      = nullptr;
	FrameContext*         m_pxFrame        = nullptr;
};
