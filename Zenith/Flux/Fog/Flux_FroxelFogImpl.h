#pragma once

#include "Flux/Fog/Flux_FroxelFog.h"
#include "Flux/Flux.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

class Flux_RenderGraph;

// Phase 7c: per-Engine state for FroxelFog subsystem.
// Per-pass constant structs (InjectConstants/LightConstants/ApplyConstants)
// are kept file-local in Flux_FroxelFog.cpp; the Impl stores the trio as
// void* opaques to avoid leaking their definitions into the header.
// Direct typed access is via g_xEngine.FroxelFog().m_xXxxConstants reads
// in the .cpp where the types are visible -- the underlying memory is the
// same.
class Flux_FroxelFogImpl
{
public:
	Flux_FroxelFogImpl() = default;
	~Flux_FroxelFogImpl() = default;

	Flux_FroxelFogImpl(const Flux_FroxelFogImpl&) = delete;
	Flux_FroxelFogImpl& operator=(const Flux_FroxelFogImpl&) = delete;

	// Pipelines (3 pass programs: inject / light / apply).
	Flux_Shader   m_xInjectShader;
	Flux_Pipeline m_xInjectPipeline;
	Flux_RootSig  m_xInjectRootSig;

	Flux_Shader   m_xLightShader;
	Flux_Pipeline m_xLightPipeline;
	Flux_RootSig  m_xLightRootSig;

	Flux_Shader   m_xApplyShader;
	Flux_Pipeline m_xApplyPipeline;

	// Graph-owned transient handles.
	Flux_TransientHandle m_xDensityGridHandle;
	Flux_TransientHandle m_xLightingGridHandle;
	Flux_TransientHandle m_xScatteringGridHandle;
	Flux_RenderGraph*    m_pxGraph = nullptr;
};
