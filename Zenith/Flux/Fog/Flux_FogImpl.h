#pragma once

#include "Flux/Flux.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

class Flux_RenderGraph;
class Flux_GraphicsImpl;

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
	// No-op: Fog owns no teardown-managed state (RAII / stateless), so there is
	// nothing to release at shutdown. Present only to satisfy the uniform
	// FluxRenderFeature interface.
	void Shutdown() {}

	void SetupRenderGraph(Flux_RenderGraph& xGraph);

	// ONE view's "Fog_Simple" declaration. Driven once per ACTIVE FULL-PIPELINE
	// view by SetupRenderGraph's ForEachActiveFullPipelineView walk, in ascending
	// slot order; the five froxel/raymarch/god-ray passes are NOT in the walk (see
	// SetupRenderGraph). The graphics reference is passed in rather than re-reached
	// through g_xEngine: SetupRenderGraph already holds it hoisted, and this
	// subsystem sits at its singleton-allowlist ceiling.
	void SetupViewPasses(Flux_RenderGraph& xGraph, u_int uSlot, Flux_GraphicsImpl& xGraphics);

	void ApplyTechniqueSelectionToGraph(Flux_RenderGraph& xGraph);

	// NOTE: the bespoke game-override path (SetExternallyOverridden /
	// IsExternallyOverridden / ReapplyOverrideToCurrentGraph / DisableAllFogPasses
	// / m_bExternallyOverridden) was removed. A game disables engine fog generically
	// via the render graph's force-disable overlay — SetOwnerForceDisabled("Fog")
	// masks all 6 fog passes (owner "Fog" = this feature's setup-step name) without
	// touching their base enable bits. ApplyTechniqueSelectionToGraph keeps the
	// base bits current; lifting the override restores the active technique exactly.

	// ★ THE MAIN VIEW'S "Fog_Simple" HANDLE ONLY — deliberately not a per-slot
	// array. ApplyTechniqueSelectionToGraph disables this pass whenever the
	// technique is not 0; every other view's instance must stay PERMANENTLY
	// ENABLED, because the five alternative technique passes are main-only, so
	// toggling a non-main instance would leave that view with no fog pass rather
	// than a different one — and the per-view oracle samples GetPasses(), which
	// still lists a disabled pass, so nothing would catch it. Full reasoning at
	// Flux_FogImpl::SetupViewPasses.
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
