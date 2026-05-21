#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "Flux/Particles/Flux_ParticleData.h"

class Flux_CommandList;
class Flux_RenderGraph;

// Phase 9: state + behaviour for Particles subsystem.
class Flux_ParticlesImpl
{
public:
	static constexpr uint32_t s_uMaxParticles = 4096;

	Flux_ParticlesImpl() = default;
	~Flux_ParticlesImpl() = default;

	Flux_ParticlesImpl(const Flux_ParticlesImpl&) = delete;
	Flux_ParticlesImpl& operator=(const Flux_ParticlesImpl&) = delete;

	void Initialise();
	void BuildPipelines();
	void ReleaseAssetReferences();
	void Shutdown();
	void Reset();
	void Render(void*);
	void SetupRenderGraph(Flux_RenderGraph& xGraph);

	Flux_Shader              m_xShader;
	Flux_Pipeline            m_xPipelineAlpha;
	Flux_Pipeline            m_xPipelineAdditive;

	Flux_DynamicVertexBuffer m_xInstanceBufferAlpha;
	Flux_DynamicVertexBuffer m_xInstanceBufferAdditive;

	Flux_ParticleInstance    m_axAlphaInstances[s_uMaxParticles];
	uint32_t                 m_uAlphaInstanceCount = 0;
	Flux_ParticleInstance    m_axAdditiveInstances[s_uMaxParticles];
	uint32_t                 m_uAdditiveInstanceCount = 0;

	TextureHandle            m_xParticleTexture;
};
