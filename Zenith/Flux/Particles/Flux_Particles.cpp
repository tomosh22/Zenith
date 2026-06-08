#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#include "Flux/Particles/Flux_ParticlesImpl.h"
#include "Flux/Particles/Flux_ParticlesImpl.h"
#include "Flux/Particles/Flux_ParticleData.h"
#include "Flux/Particles/Flux_ParticleEmitterConfig.h"
#include "Flux/Particles/Flux_ParticleGPUImpl.h"

#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/HDR/Flux_HDRImpl.h"
#include "AssetHandling/Zenith_TextureAsset.h"
// Wave 3: emitters are ticked + gathered EC-side; the renderer consumes neutral
// Zenith_ParticleEmitterRenderData (+ Zenith_ParticleData) via g_pfnZenithParticleGather.
#include "Core/Zenith_RenderGather.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "DebugVariables/Zenith_DebugVariables.h"

#ifdef ZENITH_TOOLS
#include "Flux/Slang/Flux_ShaderHotReload.h"
#endif



// Maximum particles across all emitters
static constexpr uint32_t s_uMaxParticles = 4096;

// CPU-side instance buffers for staging (partitioned by blend mode)


// Pinned via TextureHandle so UnloadUnused never frees the particle atlas mid-frame.

static void ExecuteParticles(Flux_CommandList* pxCommandList, void* pUserData);

void Flux_ParticlesImpl::BuildPipelines()
{
	m_xShader.Initialise(FluxShaderProgram::Particles);

	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_TRIANGLES;
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT3);
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT2);
	xVertexDesc.m_xPerVertexLayout.CalculateOffsetsAndStrides();
	// Instance data: position+size (float4), color (float4)
	xVertexDesc.m_xPerInstanceLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT4);
	xVertexDesc.m_xPerInstanceLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT4);
	xVertexDesc.m_xPerInstanceLayout.CalculateOffsetsAndStrides();

	Flux_PipelineSpecification xPipelineSpec;
	xPipelineSpec.m_aeColourAttachmentFormats[0] = HDR_SCENE_FORMAT;
	xPipelineSpec.m_uNumColourAttachments = 1;
	xPipelineSpec.m_pxShader = &m_xShader;
	xPipelineSpec.m_xVertexInputDesc = xVertexDesc;

	m_xShader.GetReflection().PopulateLayout(xPipelineSpec.m_xPipelineLayout);

	xPipelineSpec.m_bDepthWriteEnabled = false;

	// Alpha blending pipeline (SrcAlpha / OneMinusSrcAlpha)
	xPipelineSpec.m_axBlendStates[0].m_bBlendEnabled = true;
	xPipelineSpec.m_axBlendStates[0].m_eSrcBlendFactor = BLEND_FACTOR_SRCALPHA;
	xPipelineSpec.m_axBlendStates[0].m_eDstBlendFactor = BLEND_FACTOR_ONEMINUSSRCALPHA;

	Flux_PipelineBuilder::FromSpecification(m_xPipelineAlpha, xPipelineSpec);

	// Additive blending pipeline (SrcAlpha / One)
	xPipelineSpec.m_axBlendStates[0].m_eDstBlendFactor = BLEND_FACTOR_ONE;

	Flux_PipelineBuilder::FromSpecification(m_xPipelineAdditive, xPipelineSpec);

	// Rebuild the GPU compute pipeline alongside the rasterisation ones so a
	// shader edit to either Particles.slang or ParticleUpdate.slang triggers a
	// single coordinated rebuild. Routed through the injected ParticleGPU member.
	m_pxParticleGPU->BuildPipelines();
}

void Flux_ParticlesImpl::Initialise(Flux_GraphicsImpl& xGraphics, Flux_HDRImpl& xHDR, Flux_ParticleGPUImpl& xParticleGPU)
{
	// Wave-17 DI seam: store the injected cross-subsystem deps. Every later
	// instance-method reach-in routes through these instead of g_xEngine.
	m_pxGraphics    = &xGraphics;
	m_pxHDR         = &xHDR;
	m_pxParticleGPU = &xParticleGPU;

	BuildPipelines();

	// Allocate instance buffers for both blend modes
	auto& xVulkanMemory = g_xEngine.VulkanMemory();
	xVulkanMemory.InitialiseDynamicVertexBuffer(nullptr, s_uMaxParticles * sizeof(Flux_ParticleInstance), m_xInstanceBufferAlpha, false);
	xVulkanMemory.InitialiseDynamicVertexBuffer(nullptr, s_uMaxParticles * sizeof(Flux_ParticleInstance), m_xInstanceBufferAdditive, false);

	// Load default particle texture (pinned)
	if (Zenith_TextureAsset* pxParticle = Zenith_AssetRegistry::Get<Zenith_TextureAsset>(ENGINE_ASSETS_DIR"Textures/Particles/particleSwirl" ZENITH_TEXTURE_EXT))
	{
		m_xParticleTexture.Set(pxParticle);
	}
	else
	{
		Zenith_Log(LOG_CATEGORY_PARTICLES, "Warning: Failed to load particle texture, using white texture");
		m_xParticleTexture = m_pxGraphics->m_xWhiteTexture;
	}

#ifdef ZENITH_TOOLS
	static const FluxShaderProgram s_axPrograms[] = {
		FluxShaderProgram::Particles,
		FluxShaderProgram::ParticleUpdate,
	};
	Flux_ShaderHotReload::RegisterSubsystem([](){ g_xEngine.Particles().BuildPipelines(); },
		s_axPrograms, sizeof(s_axPrograms) / sizeof(s_axPrograms[0]));
#endif

	Zenith_Log(LOG_CATEGORY_PARTICLES, "Flux_Particles initialised (max %u particles)", s_uMaxParticles);
}

void Flux_ParticlesImpl::Reset()
{
	m_uAlphaInstanceCount = 0;
	m_uAdditiveInstanceCount = 0;
	Zenith_Log(LOG_CATEGORY_PARTICLES, "Flux_ParticlesImpl::Reset()");
}

void Flux_ParticlesImpl::ReleaseAssetReferences()
{
	m_xParticleTexture.Clear();
}

void Flux_ParticlesImpl::Shutdown()
{
	// Routed through the injected ParticleGPU member.
	m_pxParticleGPU->Shutdown();
	auto& xVulkanMemory = g_xEngine.VulkanMemory();
	xVulkanMemory.DestroyDynamicVertexBuffer(m_xInstanceBufferAlpha);
	xVulkanMemory.DestroyDynamicVertexBuffer(m_xInstanceBufferAdditive);
	// Drop the injected deps so the instance returns to a clean default state.
	m_pxGraphics    = nullptr;
	m_pxHDR         = nullptr;
	m_pxParticleGPU = nullptr;
	Zenith_Log(LOG_CATEGORY_PARTICLES, "Flux_Particles shut down");
}

void Flux_ParticlesImpl::UpdateEmittersAndBuildInstanceBuffer(float fDt)
{
	m_uAlphaInstanceCount = 0;
	m_uAdditiveInstanceCount = 0;

	// Wave 3: emitters are ticked + queried EC-side (g_pfnZenithParticleGather drives
	// xEmitter.Update(fDt) for every emitter and returns one neutral entry per CPU
	// emitter). This body just builds the GPU instance buffers from that data, so it
	// names no EntityComponent type and reaches no g_xEngine.Scenes().
	Zenith_Vector<Zenith_ParticleEmitterRenderData> xEmitters;
	if (g_pfnZenithParticleGather) g_pfnZenithParticleGather(fDt, xEmitters);

	for (u_int e = 0; e < xEmitters.GetSize(); ++e)
	{
		const Zenith_ParticleEmitterRenderData& xEmitterData = xEmitters.Get(e);

		// Route to the alpha or additive instance buffer based on the emitter's blend mode.
		Flux_ParticleInstance* pxTargetBuffer = xEmitterData.m_bAdditive ? m_axAdditiveInstances : m_axAlphaInstances;
		uint32_t& uTargetCount = xEmitterData.m_bAdditive ? m_uAdditiveInstanceCount : m_uAlphaInstanceCount;

		for (uint32_t i = 0; i < xEmitterData.m_uAliveCount && uTargetCount < s_uMaxParticles; ++i)
		{
			// Zenith_ParticleData (neutral mirror) exposes the same position/size/colour
			// accessors as Flux_Particle, so the renderer builds the instance directly.
			const Zenith_ParticleData& xP = xEmitterData.m_pxParticles[i];
			pxTargetBuffer[uTargetCount] = Flux_ParticleInstance(xP.GetPosition(), xP.GetCurrentSize(), xP.GetCurrentColor());
			uTargetCount++;
		}
	}
}

void Flux_ParticlesImpl::UploadInstanceData()
{
	// Promoted from a file-static free function to an instance member: buffer/count
	// self-references now resolve through 'this'. VulkanMemory() stays a direct
	// g_xEngine lookup (engine-infra carve-out, same as SSAO/Quads).
	auto& xVulkanMemory = g_xEngine.VulkanMemory();
	if (m_uAlphaInstanceCount > 0)
	{
		xVulkanMemory.UploadBufferData(
			m_xInstanceBufferAlpha.GetBuffer().m_xVRAMHandle,
			m_axAlphaInstances,
			m_uAlphaInstanceCount * sizeof(Flux_ParticleInstance)
		);
	}
	if (m_uAdditiveInstanceCount > 0)
	{
		xVulkanMemory.UploadBufferData(
			m_xInstanceBufferAdditive.GetBuffer().m_xVRAMHandle,
			m_axAdditiveInstances,
			m_uAdditiveInstanceCount * sizeof(Flux_ParticleInstance)
		);
	}
}

void Flux_ParticlesImpl::Render(void*)
{
	if (!Zenith_GraphicsOptions::Get().m_bCPUParticlesEnabled)
	{
		return;
	}

	// Update all emitters (both CPU and GPU) and build the CPU instance buffers.
	// Frame() stays a direct g_xEngine lookup (engine-infra carve-out).
	float fDt = g_xEngine.Frame().GetDt();
	UpdateEmittersAndBuildInstanceBuffer(fDt);

	// Upload CPU instance data to GPU
	UploadInstanceData();
}

static void ExecuteParticles(Flux_CommandList* pxCommandList, void* pUserData)
{
	(void)pUserData;
	if (!Zenith_GraphicsOptions::Get().m_bCPUParticlesEnabled)
	{
		return;
	}

	// Emitter sim + instance upload runs once per frame in Render (registered as
	// this pass's Prepare, main thread). This record callback only emits draw
	// commands from the already-populated instance counts/buffers — no ECS
	// mutation (xEmitter.Update) on the worker thread.

	// Non-capturing graph callback (void(*)(Flux_CommandList*, void*)) — it cannot
	// capture, so it re-enters via g_xEngine.Particles() to reach the singleton
	// instance, then routes its FluxGraphics reach-ins through the injected member
	// (mirrors ExecuteSSAOGenerate / ExecuteQuads).
	Flux_ParticlesImpl& xParticles = g_xEngine.Particles();

	// Render CPU particles (alpha-blended first, then additive)
	if (xParticles.m_uAlphaInstanceCount > 0 || xParticles.m_uAdditiveInstanceCount > 0)
	{
		// Alpha-blended particles
		if (xParticles.m_uAlphaInstanceCount > 0)
		{
			pxCommandList->AddCommand<Flux_CommandSetPipeline>(&xParticles.m_xPipelineAlpha);

			pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&xParticles.m_pxGraphics->m_xQuadMesh.GetVertexBuffer(), 0);
			pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&xParticles.m_pxGraphics->m_xQuadMesh.GetIndexBuffer());
			pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&xParticles.m_xInstanceBufferAlpha, 1);

			pxCommandList->AddCommand<Flux_CommandBeginBind>(0);
			pxCommandList->AddCommand<Flux_CommandBindCBV>(&xParticles.m_pxGraphics->m_xFrameConstantsBuffer.GetCBV(), 0);
			pxCommandList->AddCommand<Flux_CommandBindSRV>(&xParticles.m_xParticleTexture.GetDirect()->m_xSRV, 1);

			pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6, xParticles.m_uAlphaInstanceCount);
		}

		// Additive particles
		if (xParticles.m_uAdditiveInstanceCount > 0)
		{
			pxCommandList->AddCommand<Flux_CommandSetPipeline>(&xParticles.m_xPipelineAdditive);

			pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&xParticles.m_pxGraphics->m_xQuadMesh.GetVertexBuffer(), 0);
			pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&xParticles.m_pxGraphics->m_xQuadMesh.GetIndexBuffer());
			pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&xParticles.m_xInstanceBufferAdditive, 1);

			pxCommandList->AddCommand<Flux_CommandBeginBind>(0);
			pxCommandList->AddCommand<Flux_CommandBindCBV>(&xParticles.m_pxGraphics->m_xFrameConstantsBuffer.GetCBV(), 0);
			pxCommandList->AddCommand<Flux_CommandBindSRV>(&xParticles.m_xParticleTexture.GetDirect()->m_xSRV, 1);

			pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6, xParticles.m_uAdditiveInstanceCount);
		}
	}
}

static void ExecuteParticleCompute(Flux_CommandList* pxCmdList, void*)
{
	// Non-capturing graph callback: re-enter via g_xEngine.Particles() to reach
	// the singleton instance, then route the ParticleGPU reach-in through the
	// injected member (mirrors ExecuteParticles).
	g_xEngine.Particles().m_pxParticleGPU->DispatchCompute(pxCmdList);
}

static void PreExecuteParticleCompute(void*)
{
	// Non-capturing Prepare callback: same self-lookup + member-route as above.
	g_xEngine.Particles().m_pxParticleGPU->PreExecuteCompute();
}

void Flux_ParticlesImpl::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	// GPU particle compute pass (updates instance buffer before draw). The
	// instance buffer is managed internally by Flux_ParticleGPU and isn't
	// graph-tracked, so the draw→compute edge is expressed as an explicit
	// DependsOn rather than a resource declaration.
	Flux_PassHandle xComputePass = xGraph.AddPass("Particles Compute", ExecuteParticleCompute)
		.Prepare(PreExecuteParticleCompute);

	// Render (main-thread Prepare) does the emitter sim + instance-buffer upload
	// before any record callback runs; ExecuteParticles (worker) then only emits
	// draw commands. This moves the xEmitter.Update ECS mutation off the worker
	// thread. Render was previously defined but never registered — this is a new,
	// required edge (without it the CPU particle sim/upload would never run).
	xGraph.AddPass("Particles", ExecuteParticles)
		.Prepare([](void* p){ g_xEngine.Particles().Render(p); })
		.Writes(m_pxHDR->GetHDRSceneTarget(), RESOURCE_ACCESS_WRITE_RTV)
		.DependsOn(xComputePass);
}
