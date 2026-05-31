#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "Flux/Particles/Flux_ParticleData.h"

class Flux_CommandList;
class Flux_RenderGraph;

// Cross-subsystem dependencies injected into Initialise (Wave-17 DI seam, built
// on the WS9.2 Flux_HiZImpl / Wave-11 Flux_SSAOImpl / Wave-14 Flux_QuadsImpl
// template). Forward-declared here; the full headers are pulled in by
// Flux_Particles.cpp. This is the heaviest leaf seam — THREE cross-subsystem
// deps: Flux_GraphicsImpl (frame constants / quad mesh), Flux_HDRImpl (HDR scene
// target write) and the sibling Flux_ParticleGPUImpl (GPU-particle compute).
// Frame()/VulkanMemory() stay direct g_xEngine lookups (engine-infra carve-out,
// same as SSAO/Quads), and the g_xEngine.Scenes() ECS Prepare-gather inside the
// emitter-sim free function STAYS self-routed (injecting ECS would reopen the
// Flux<->ECS layering gate).
class Flux_GraphicsImpl;
class Flux_HDRImpl;
class Flux_ParticleGPUImpl;

// Phase 9: state + behaviour for Particles subsystem.
//
// Wave-17 DI seam (mirrors Flux_SSAOImpl): the three cross-subsystem dependencies
// are INJECTED through Initialise as explicit references and stored as member
// pointers, rather than reached for via g_xEngine.X() inside the instance
// methods. The only place g_xEngine self-lookup survives is the non-capturing
// fn-pointer trampolines (the ExecuteParticles / ExecuteParticleCompute /
// PreExecuteParticleCompute graph callbacks, the Render-Prepare trampoline, and
// the ZENITH_TOOLS hot-reload callback) — those cannot capture state, so they
// re-enter via g_xEngine.Particles() to reach this singleton instance and then
// route their other reach-ins through the injected members.
class Flux_ParticlesImpl
{
public:
	static constexpr uint32_t s_uMaxParticles = 4096;

	Flux_ParticlesImpl() = default;
	~Flux_ParticlesImpl() = default;

	Flux_ParticlesImpl(const Flux_ParticlesImpl&) = delete;
	Flux_ParticlesImpl& operator=(const Flux_ParticlesImpl&) = delete;

	// Cross-subsystem deps are injected here and stored into the member pointers
	// below. This is the WS9.2 DI template: explicit ref params -> stored member
	// pointers.
	void Initialise(Flux_GraphicsImpl& xGraphics, Flux_HDRImpl& xHDR, Flux_ParticleGPUImpl& xParticleGPU);
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

	// Injected cross-subsystem dependencies (stored by Initialise). Default
	// nullptr so a default-constructed instance is headless-safe; the real boot
	// path wires them in via the Particles init trampoline
	// (Flux_FeatureRegistry.cpp).
	Flux_GraphicsImpl*       m_pxGraphics    = nullptr;
	Flux_HDRImpl*            m_pxHDR         = nullptr;
	Flux_ParticleGPUImpl*    m_pxParticleGPU = nullptr;
};
