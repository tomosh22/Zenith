#pragma once

#include "Flux/Fog/Flux_Fog.h"
#include "Flux/Flux.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

// Phase 7d: per-Engine state for top-level Fog subsystem -- per-pass
// handles, last-technique cache, base simple fog pipeline.
// Flux_FogConstants is a .cpp-local POD; its backing storage stays
// file-static.
class Flux_FogImpl
{
public:
	Flux_FogImpl() = default;
	~Flux_FogImpl() = default;

	Flux_FogImpl(const Flux_FogImpl&) = delete;
	Flux_FogImpl& operator=(const Flux_FogImpl&) = delete;

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
