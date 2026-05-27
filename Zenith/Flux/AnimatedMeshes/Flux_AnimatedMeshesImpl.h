#pragma once

#include "Flux/Flux.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

class Flux_DynamicConstantBuffer;

// Phase 9: state + behaviour for animated-meshes subsystem.
class Flux_AnimatedMeshesImpl
{
public:
	Flux_AnimatedMeshesImpl() = default;
	~Flux_AnimatedMeshesImpl() = default;

	Flux_AnimatedMeshesImpl(const Flux_AnimatedMeshesImpl&) = delete;
	Flux_AnimatedMeshesImpl& operator=(const Flux_AnimatedMeshesImpl&) = delete;

	void Initialise();
	void Shutdown();
	void BuildPipelines();

	void RenderToShadowMap(Flux_CommandList& xCmdBuf, const Flux_DynamicConstantBuffer& xShadowMatrixBuffer);

	void SetupRenderGraph(Flux_RenderGraph& xGraph);

	Flux_Pipeline& GetShadowPipeline() { return m_xShadowPipeline; }

	Flux_Shader   m_xGBufferShader;
	Flux_Pipeline m_xGBufferPipeline;
	Flux_Shader   m_xShadowShader;
	Flux_Pipeline m_xShadowPipeline;
};
