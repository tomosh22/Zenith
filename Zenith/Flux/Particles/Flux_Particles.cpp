#include "Zenith.h"

#include "Flux/Particles/Flux_Particles.h"
#include "Flux/Particles/Flux_ParticleData.h"
#include "Flux/Particles/Flux_ParticleEmitterConfig.h"
#include "Flux/Particles/Flux_ParticleGPU.h"

#include "Flux/Flux.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/HDR/Flux_HDR.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Zenith_Query.h"
#include "EntityComponent/Components/Zenith_ParticleEmitterComponent.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "TaskSystem/Zenith_TaskSystem.h"

static Zenith_Task g_xRenderTask(ZENITH_PROFILE_INDEX__FLUX_PFX, Flux_Particles::Render, nullptr);

static Flux_CommandList g_xCommandList("Particles");

static Flux_Shader s_xShader;
static Flux_Pipeline s_xPipelineAlpha;
static Flux_Pipeline s_xPipelineAdditive;

static Flux_DynamicVertexBuffer s_xInstanceBufferAlpha;
static Flux_DynamicVertexBuffer s_xInstanceBufferAdditive;

DEBUGVAR bool dbg_bEnable = true;

// Maximum particles across all emitters
static constexpr uint32_t s_uMaxParticles = 4096;

// CPU-side instance buffers for staging (partitioned by blend mode)
static Flux_ParticleInstance s_axAlphaInstances[s_uMaxParticles];
static uint32_t s_uAlphaInstanceCount = 0;

static Flux_ParticleInstance s_axAdditiveInstances[s_uMaxParticles];
static uint32_t s_uAdditiveInstanceCount = 0;

static Zenith_TextureAsset* s_pxParticleTexture = nullptr;

void Flux_Particles::Initialise()
{
	s_xShader.Initialise("Particles/Flux_Particles.vert", "Particles/Flux_Particles.frag");

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
	xPipelineSpec.m_pxTargetSetup = &Flux_HDR::GetHDRSceneTargetSetupWithDepth();
	xPipelineSpec.m_pxShader = &s_xShader;
	xPipelineSpec.m_xVertexInputDesc = xVertexDesc;

	Flux_PipelineLayout& xLayout = xPipelineSpec.m_xPipelineLayout;
	xLayout.m_uNumDescriptorSets = 1;
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_BUFFER;
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[1].m_eType = DESCRIPTOR_TYPE_TEXTURE;

	xPipelineSpec.m_bDepthWriteEnabled = false;

	// Alpha blending pipeline (SrcAlpha / OneMinusSrcAlpha)
	xPipelineSpec.m_axBlendStates[0].m_bBlendEnabled = true;
	xPipelineSpec.m_axBlendStates[0].m_eSrcBlendFactor = BLEND_FACTOR_SRCALPHA;
	xPipelineSpec.m_axBlendStates[0].m_eDstBlendFactor = BLEND_FACTOR_ONEMINUSSRCALPHA;

	Flux_PipelineBuilder::FromSpecification(s_xPipelineAlpha, xPipelineSpec);

	// Additive blending pipeline (SrcAlpha / One)
	xPipelineSpec.m_axBlendStates[0].m_eDstBlendFactor = BLEND_FACTOR_ONE;

	Flux_PipelineBuilder::FromSpecification(s_xPipelineAdditive, xPipelineSpec);

	// Allocate instance buffers for both blend modes
	Flux_MemoryManager::InitialiseDynamicVertexBuffer(nullptr, s_uMaxParticles * sizeof(Flux_ParticleInstance), s_xInstanceBufferAlpha, false);
	Flux_MemoryManager::InitialiseDynamicVertexBuffer(nullptr, s_uMaxParticles * sizeof(Flux_ParticleInstance), s_xInstanceBufferAdditive, false);

	// Load default particle texture
	s_pxParticleTexture = Zenith_AssetRegistry::Get().Get<Zenith_TextureAsset>("C:/dev/Zenith/Zenith/Assets/Textures/Particles/particleSwirl" ZENITH_TEXTURE_EXT);
	if (!s_pxParticleTexture)
	{
		Zenith_Log(LOG_CATEGORY_PARTICLES, "Warning: Failed to load particle texture, using white texture");
		s_pxParticleTexture = Flux_Graphics::s_pxWhiteTexture;
	}

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({ "Render", "Enable", "Particles" }, dbg_bEnable);
#endif

	Zenith_Log(LOG_CATEGORY_PARTICLES, "Flux_Particles initialised (max %u particles)", s_uMaxParticles);
}

void Flux_Particles::Reset()
{
	// Reset command list to ensure no stale GPU resource references
	g_xCommandList.Reset(true);
	s_uAlphaInstanceCount = 0;
	s_uAdditiveInstanceCount = 0;
	Zenith_Log(LOG_CATEGORY_PARTICLES, "Flux_Particles::Reset()");
}

void Flux_Particles::Shutdown()
{
	Flux_MemoryManager::DestroyDynamicVertexBuffer(s_xInstanceBufferAlpha);
	Flux_MemoryManager::DestroyDynamicVertexBuffer(s_xInstanceBufferAdditive);
	Zenith_Log(LOG_CATEGORY_PARTICLES, "Flux_Particles shut down");
}

static void UpdateEmittersAndBuildInstanceBuffer(float fDt)
{
	s_uAlphaInstanceCount = 0;
	s_uAdditiveInstanceCount = 0;

	// Query all particle emitter components from ALL loaded scenes
	for (uint32_t uSceneSlot = 0; uSceneSlot < Zenith_SceneManager::GetSceneSlotCount(); ++uSceneSlot)
	{
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneDataAtSlot(uSceneSlot);
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

				Flux_ParticleInstance* pxTargetBuffer = bAdditive ? s_axAdditiveInstances : s_axAlphaInstances;
				uint32_t& uTargetCount = bAdditive ? s_uAdditiveInstanceCount : s_uAlphaInstanceCount;

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
	if (s_uAlphaInstanceCount > 0)
	{
		Flux_MemoryManager::UploadBufferData(
			s_xInstanceBufferAlpha.GetBuffer().m_xVRAMHandle,
			s_axAlphaInstances,
			s_uAlphaInstanceCount * sizeof(Flux_ParticleInstance)
		);
	}
	if (s_uAdditiveInstanceCount > 0)
	{
		Flux_MemoryManager::UploadBufferData(
			s_xInstanceBufferAdditive.GetBuffer().m_xVRAMHandle,
			s_axAdditiveInstances,
			s_uAdditiveInstanceCount * sizeof(Flux_ParticleInstance)
		);
	}
}

void Flux_Particles::SubmitRenderTask()
{
	Zenith_TaskSystem::SubmitTask(&g_xRenderTask);
}

void Flux_Particles::WaitForRenderTask()
{
	g_xRenderTask.WaitUntilComplete();
}

void Flux_Particles::Render(void*)
{
	if (!dbg_bEnable)
	{
		return;
	}

	// Update all emitters (both CPU and GPU) and build the CPU instance buffers
	float fDt = Zenith_Core::GetDt();
	UpdateEmittersAndBuildInstanceBuffer(fDt);

	// GPU compute dispatch is disabled until GPU particle rendering is implemented.
	// GPU emitters will fall back to CPU simulation in the component's SetConfig.
	// if (Flux_ParticleGPU::HasGPUEmitters())
	// {
	// 	Flux_ParticleGPU::DispatchCompute();
	// }

	// Upload CPU instance data to GPU
	UploadInstanceData();

	// Render CPU particles (alpha-blended first, then additive)
	if (s_uAlphaInstanceCount > 0 || s_uAdditiveInstanceCount > 0)
	{
		g_xCommandList.Reset(false);

		// Alpha-blended particles
		if (s_uAlphaInstanceCount > 0)
		{
			g_xCommandList.AddCommand<Flux_CommandSetPipeline>(&s_xPipelineAlpha);

			g_xCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer(), 0);
			g_xCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());
			g_xCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&s_xInstanceBufferAlpha, 1);

			g_xCommandList.AddCommand<Flux_CommandBeginBind>(0);
			g_xCommandList.AddCommand<Flux_CommandBindCBV>(&Flux_Graphics::s_xFrameConstantsBuffer.GetCBV(), 0);
			g_xCommandList.AddCommand<Flux_CommandBindSRV>(&s_pxParticleTexture->m_xSRV, 1);

			g_xCommandList.AddCommand<Flux_CommandDrawIndexed>(6, s_uAlphaInstanceCount);
		}

		// Additive particles
		if (s_uAdditiveInstanceCount > 0)
		{
			g_xCommandList.AddCommand<Flux_CommandSetPipeline>(&s_xPipelineAdditive);

			g_xCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer(), 0);
			g_xCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());
			g_xCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&s_xInstanceBufferAdditive, 1);

			g_xCommandList.AddCommand<Flux_CommandBeginBind>(0);
			g_xCommandList.AddCommand<Flux_CommandBindCBV>(&Flux_Graphics::s_xFrameConstantsBuffer.GetCBV(), 0);
			g_xCommandList.AddCommand<Flux_CommandBindSRV>(&s_pxParticleTexture->m_xSRV, 1);

			g_xCommandList.AddCommand<Flux_CommandDrawIndexed>(6, s_uAdditiveInstanceCount);
		}

		Flux::SubmitCommandList(&g_xCommandList, Flux_HDR::GetHDRSceneTargetSetupWithDepth(), RENDER_ORDER_PARTICLES);
	}

	// TODO: GPU particle rendering requires using the compute output buffer as a vertex buffer.
	// The current abstraction doesn't support Flux_ReadWriteBuffer as instance data.
	// Options to implement:
	// 1. Add a Flux_VertexStorageBuffer type that can be used for both compute and vertex input
	// 2. Use indirect drawing with the storage buffer
	// 3. Read back GPU instances and copy to CPU buffer (defeats purpose of GPU particles)
	//
	// For now, GPU compute is disabled until rendering is implemented.
	// GPU emitters will fall back to CPU simulation.
}
