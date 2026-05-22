#pragma once

#include "Flux/Flux.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

class Flux_CommandList;
class Flux_RenderGraph;

// Phase 9: state + behaviour for FroxelFog subsystem.
class Flux_FroxelFogImpl
{
public:
	Flux_FroxelFogImpl() = default;
	~Flux_FroxelFogImpl() = default;

	Flux_FroxelFogImpl(const Flux_FroxelFogImpl&) = delete;
	Flux_FroxelFogImpl& operator=(const Flux_FroxelFogImpl&) = delete;

	void Initialise();
	void BuildPipelines();
	void Reset();

	void SetupTransients(Flux_RenderGraph& xGraph);

	void RenderInject(Flux_CommandList* pxCommandList);
	void RenderLight(Flux_CommandList* pxCommandList);
	void RenderApply(Flux_CommandList* pxCommandList);

	Flux_RenderAttachment& GetDensityGrid();
	Flux_RenderAttachment& GetLightingGrid();
	Flux_RenderAttachment& GetScatteringGrid();

	Flux_TransientHandle GetDensityGridHandle() const    { return m_xDensityGridHandle; }
	Flux_TransientHandle GetLightingGridHandle() const   { return m_xLightingGridHandle; }
	Flux_TransientHandle GetScatteringGridHandle() const { return m_xScatteringGridHandle; }

	Flux_RenderAttachment& GetDebugSliceTexture();

	float GetNearZ();
	float GetFarZ();

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
