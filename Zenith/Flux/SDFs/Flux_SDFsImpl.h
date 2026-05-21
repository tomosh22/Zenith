#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"

class Flux_CommandList;
class Flux_RenderGraph;

// Phase 9: state + behaviour for SDFs subsystem.
class Flux_SDFsImpl
{
public:
	Flux_SDFsImpl() = default;
	~Flux_SDFsImpl() = default;

	Flux_SDFsImpl(const Flux_SDFsImpl&) = delete;
	Flux_SDFsImpl& operator=(const Flux_SDFsImpl&) = delete;

	void Initialise();
	void BuildPipelines();
	void Shutdown();

	void Render(void*);

	void SetupRenderGraph(Flux_RenderGraph& xGraph);

	Flux_Shader                m_xShader;
	Flux_Pipeline              m_xPipeline;
	Flux_DynamicConstantBuffer m_xSpheresBuffer;
};
