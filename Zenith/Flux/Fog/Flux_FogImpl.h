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

	void SetExternallyOverridden(bool bOverridden);
	bool IsExternallyOverridden() const { return m_bExternallyOverridden; }

	void ReapplyOverrideToCurrentGraph();

	Flux_PassHandle m_xSimpleFogPass;
	Flux_PassHandle m_xFroxelInjectPass;
	Flux_PassHandle m_xFroxelLightPass;
	Flux_PassHandle m_xFroxelApplyPass;
	Flux_PassHandle m_xRaymarchPass;
	Flux_PassHandle m_xGodRaysPass;

	u_int           m_uLastFogTechnique = UINT32_MAX;
	bool            m_bExternallyOverridden = false;

	Flux_Shader     m_xShader;
	Flux_Pipeline   m_xPipeline;
};
