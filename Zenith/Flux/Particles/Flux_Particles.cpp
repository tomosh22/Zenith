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
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneSystem.h"
#include "EntityComponent/Zenith_Query.h"
#include "EntityComponent/Components/Zenith_ParticleEmitterComponent.h"
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
	g_xEngine.Particles().m_xShader.Initialise(FluxShaderProgram::Particles);

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
	xPipelineSpec.m_pxShader = &g_xEngine.Particles().m_xShader;
	xPipelineSpec.m_xVertexInputDesc = xVertexDesc;

	g_xEngine.Particles().m_xShader.GetReflection().PopulateLayout(xPipelineSpec.m_xPipelineLayout);

	xPipelineSpec.m_bDepthWriteEnabled = false;

	// Alpha blending pipeline (SrcAlpha / OneMinusSrcAlpha)
	xPipelineSpec.m_axBlendStates[0].m_bBlendEnabled = true;
	xPipelineSpec.m_axBlendStates[0].m_eSrcBlendFactor = BLEND_FACTOR_SRCALPHA;
	xPipelineSpec.m_axBlendStates[0].m_eDstBlendFactor = BLEND_FACTOR_ONEMINUSSRCALPHA;

	Flux_PipelineBuilder::FromSpecification(g_xEngine.Particles().m_xPipelineAlpha, xPipelineSpec);

	// Additive blending pipeline (SrcAlpha / One)
	xPipelineSpec.m_axBlendStates[0].m_eDstBlendFactor = BLEND_FACTOR_ONE;

	Flux_PipelineBuilder::FromSpecification(g_xEngine.Particles().m_xPipelineAdditive, xPipelineSpec);

	// Rebuild the GPU compute pipeline alongside the rasterisation ones so a
	// shader edit to either Particles.slang or ParticleUpdate.slang triggers a
	// single coordinated rebuild.
	g_xEngine.ParticleGPU().BuildPipelines();
}

void Flux_ParticlesImpl::Initialise()
{
	BuildPipelines();

	// Allocate instance buffers for both blend modes
	g_xEngine.VulkanMemory().InitialiseDynamicVertexBuffer(nullptr, s_uMaxParticles * sizeof(Flux_ParticleInstance), g_xEngine.Particles().m_xInstanceBufferAlpha, false);
	g_xEngine.VulkanMemory().InitialiseDynamicVertexBuffer(nullptr, s_uMaxParticles * sizeof(Flux_ParticleInstance), g_xEngine.Particles().m_xInstanceBufferAdditive, false);

	// Load default particle texture (pinned)
	if (Zenith_TextureAsset* pxParticle = Zenith_AssetRegistry::Get<Zenith_TextureAsset>(ENGINE_ASSETS_DIR"Textures/Particles/particleSwirl" ZENITH_TEXTURE_EXT))
	{
		g_xEngine.Particles().m_xParticleTexture.Set(pxParticle);
	}
	else
	{
		Zenith_Log(LOG_CATEGORY_PARTICLES, "Warning: Failed to load particle texture, using white texture");
		g_xEngine.Particles().m_xParticleTexture = g_xEngine.FluxGraphics().m_xWhiteTexture;
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
	g_xEngine.Particles().m_uAlphaInstanceCount = 0;
	g_xEngine.Particles().m_uAdditiveInstanceCount = 0;
	Zenith_Log(LOG_CATEGORY_PARTICLES, "Flux_ParticlesImpl::Reset()");
}

void Flux_ParticlesImpl::ReleaseAssetReferences()
{
	g_xEngine.Particles().m_xParticleTexture.Clear();
}

void Flux_ParticlesImpl::Shutdown()
{
	g_xEngine.ParticleGPU().Shutdown();
	g_xEngine.VulkanMemory().DestroyDynamicVertexBuffer(g_xEngine.Particles().m_xInstanceBufferAlpha);
	g_xEngine.VulkanMemory().DestroyDynamicVertexBuffer(g_xEngine.Particles().m_xInstanceBufferAdditive);
	Zenith_Log(LOG_CATEGORY_PARTICLES, "Flux_Particles shut down");
}

static void UpdateEmittersAndBuildInstanceBuffer(float fDt)
{
	g_xEngine.Particles().m_uAlphaInstanceCount = 0;
	g_xEngine.Particles().m_uAdditiveInstanceCount = 0;

	// Query all particle emitter components from ALL loaded scenes
	for (uint32_t uSceneSlot = 0; uSceneSlot < g_xEngine.Scenes().GetSceneSlotCount(); ++uSceneSlot)
	{
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneDataAtSlot(uSceneSlot);
		if (!pxSceneData || !pxSceneData->IsLoaded() || pxSceneData->IsUnloading())
		{
			continue;
		}

		pxSceneData->Query<Zenith_ParticleEmitterComponent>()
			.ForEach([fDt](Zenith_EntityID, Zenith_ParticleEmitterComponent& xEmitter)
			{
				// Update ALL emitters (handles spawning for both CPU and GPU)
				xEmitter.Update(fDt);

				// Only build instance buffer for CPU emitters
				// GPU emitters have their instances built by the compute shader
				if (xEmitter.UsesGPUCompute())
				{
					return;
				}

				// Copy alive particles to the appropriate instance buffer based on blend mode
				const Zenith_Vector<Flux_Particle>& axParticles = xEmitter.GetParticles();
				uint32_t uAliveCount = xEmitter.GetAliveCount();

				Flux_ParticleEmitterConfig* pxConfig = xEmitter.GetConfig();
				bool bAdditive = (pxConfig != nullptr && pxConfig->m_bAdditiveBlending);

				Flux_ParticleInstance* pxTargetBuffer = bAdditive ? g_xEngine.Particles().m_axAdditiveInstances : g_xEngine.Particles().m_axAlphaInstances;
				uint32_t& uTargetCount = bAdditive ? g_xEngine.Particles().m_uAdditiveInstanceCount : g_xEngine.Particles().m_uAlphaInstanceCount;

				for (uint32_t i = 0; i < uAliveCount && uTargetCount < s_uMaxParticles; ++i)
				{
					pxTargetBuffer[uTargetCount] = Flux_ParticleInstance::FromParticle(axParticles.Get(i));
					uTargetCount++;
				}
			});
	}
}

static void UploadInstanceData()
{
	if (g_xEngine.Particles().m_uAlphaInstanceCount > 0)
	{
		g_xEngine.VulkanMemory().UploadBufferData(
			g_xEngine.Particles().m_xInstanceBufferAlpha.GetBuffer().m_xVRAMHandle,
			g_xEngine.Particles().m_axAlphaInstances,
			g_xEngine.Particles().m_uAlphaInstanceCount * sizeof(Flux_ParticleInstance)
		);
	}
	if (g_xEngine.Particles().m_uAdditiveInstanceCount > 0)
	{
		g_xEngine.VulkanMemory().UploadBufferData(
			g_xEngine.Particles().m_xInstanceBufferAdditive.GetBuffer().m_xVRAMHandle,
			g_xEngine.Particles().m_axAdditiveInstances,
			g_xEngine.Particles().m_uAdditiveInstanceCount * sizeof(Flux_ParticleInstance)
		);
	}
}

void Flux_ParticlesImpl::Render(void*)
{
	if (!Zenith_GraphicsOptions::Get().m_bCPUParticlesEnabled)
	{
		return;
	}

	// Update all emitters (both CPU and GPU) and build the CPU instance buffers
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

	// CPU-side work: update emitters and upload instance data
	float fDt = g_xEngine.Frame().GetDt();
	UpdateEmittersAndBuildInstanceBuffer(fDt);
	UploadInstanceData();

	// Render CPU particles (alpha-blended first, then additive)
	if (g_xEngine.Particles().m_uAlphaInstanceCount > 0 || g_xEngine.Particles().m_uAdditiveInstanceCount > 0)
	{
		// Alpha-blended particles
		if (g_xEngine.Particles().m_uAlphaInstanceCount > 0)
		{
			pxCommandList->AddCommand<Flux_CommandSetPipeline>(&g_xEngine.Particles().m_xPipelineAlpha);

			pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetVertexBuffer(), 0);
			pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetIndexBuffer());
			pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&g_xEngine.Particles().m_xInstanceBufferAlpha, 1);

			pxCommandList->AddCommand<Flux_CommandBeginBind>(0);
			pxCommandList->AddCommand<Flux_CommandBindCBV>(&g_xEngine.FluxGraphics().m_xFrameConstantsBuffer.GetCBV(), 0);
			pxCommandList->AddCommand<Flux_CommandBindSRV>(&g_xEngine.Particles().m_xParticleTexture.GetDirect()->m_xSRV, 1);

			pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6, g_xEngine.Particles().m_uAlphaInstanceCount);
		}

		// Additive particles
		if (g_xEngine.Particles().m_uAdditiveInstanceCount > 0)
		{
			pxCommandList->AddCommand<Flux_CommandSetPipeline>(&g_xEngine.Particles().m_xPipelineAdditive);

			pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetVertexBuffer(), 0);
			pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetIndexBuffer());
			pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&g_xEngine.Particles().m_xInstanceBufferAdditive, 1);

			pxCommandList->AddCommand<Flux_CommandBeginBind>(0);
			pxCommandList->AddCommand<Flux_CommandBindCBV>(&g_xEngine.FluxGraphics().m_xFrameConstantsBuffer.GetCBV(), 0);
			pxCommandList->AddCommand<Flux_CommandBindSRV>(&g_xEngine.Particles().m_xParticleTexture.GetDirect()->m_xSRV, 1);

			pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6, g_xEngine.Particles().m_uAdditiveInstanceCount);
		}
	}
}

static void ExecuteParticleCompute(Flux_CommandList* pxCmdList, void*)
{
	g_xEngine.ParticleGPU().DispatchCompute(pxCmdList);
}

static void PreExecuteParticleCompute(void*)
{
	g_xEngine.ParticleGPU().PreExecuteCompute();
}

void Flux_ParticlesImpl::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	// GPU particle compute pass (updates instance buffer before draw). The
	// instance buffer is managed internally by Flux_ParticleGPU and isn't
	// graph-tracked, so the draw→compute edge is expressed as an explicit
	// DependsOn rather than a resource declaration.
	Flux_PassHandle xComputePass = xGraph.AddPass("Particles Compute", ExecuteParticleCompute)
		.Prepare(PreExecuteParticleCompute);

	xGraph.AddPass("Particles", ExecuteParticles)
		.Writes(g_xEngine.HDR().GetHDRSceneTarget(), RESOURCE_ACCESS_WRITE_RTV)
		.DependsOn(xComputePass);
}
