#pragma once

#include "Flux/Flux.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

class Flux_CommandList;
class Flux_RenderGraph;

// Phase 9: state + behaviour for top-level Fog orchestrator subsystem.
class Flux_FogImpl
{
public:
	Flux_FogImpl() = default;
	~Flux_FogImpl() = default;

	Flux_FogImpl(const Flux_FogImpl&) = delete;
	Flux_FogImpl& operator=(const Flux_FogImpl&) = delete;

	void Initialise();
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
};
