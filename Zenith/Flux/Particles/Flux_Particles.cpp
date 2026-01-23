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
#include "EntityComponent/Zenith_Query.h"
#include "EntityComponent/Components/Zenith_ParticleEmitterComponent.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "TaskSystem/Zenith_TaskSystem.h"

static Zenith_Task g_xRenderTask(ZENITH_PROFILE_INDEX__FLUX_PFX, Flux_Particles::Render, nullptr);

static Flux_CommandList g_xCommandList("Particles");

static Flux_Shader s_xShader;
static Flux_Pipeline s_xPipeline;

static Flux_DynamicVertexBuffer s_xInstanceBuffer;

DEBUGVAR bool dbg_bEnable = true;

// Maximum particles across all emitters
static constexpr uint32_t s_uMaxParticles = 4096;

// CPU-side instance buffer for staging particle render data
static Flux_ParticleInstance s_axCPUInstances[s_uMaxParticles];
static uint32_t s_uInstanceCount = 0;

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

	// Enable alpha blending
	xPipelineSpec.m_axBlendStates[0].m_bBlendEnabled = true;
	xPipelineSpec.m_axBlendStates[0].m_eSrcBlendFactor = BLEND_FACTOR_SRCALPHA;
	xPipelineSpec.m_axBlendStates[0].m_eDstBlendFactor = BLEND_FACTOR_ONEMINUSSRCALPHA;

	Flux_PipelineBuilder::FromSpecification(s_xPipeline, xPipelineSpec);

	// Allocate instance buffer for max particles
	Flux_MemoryManager::InitialiseDynamicVertexBuffer(nullptr, s_uMaxParticles * sizeof(Flux_ParticleInstance), s_xInstanceBuffer, false);

	// Load default particle texture
	s_pxParticleTexture = Zenith_AssetRegistry::Get().Get<Zenith_TextureAsset>("C:/dev/Zenith/Games/Test/Assets/Textures/particleSwirl" ZENITH_TEXTURE_EXT);
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
	s_uInstanceCount = 0;
	Zenith_Log(LOG_CATEGORY_PARTICLES, "Flux_Particles::Reset()");
}

void Flux_Particles::Shutdown()
{
	Flux_MemoryManager::DestroyDynamicVertexBuffer(s_xInstanceBuffer);
	Zenith_Log(LOG_CATEGORY_PARTICLES, "Flux_Particles shut down");
}

static void UpdateEmittersAndBuildInstanceBuffer(float fDt)
{
	s_uInstanceCount = 0;

	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

	// Query all particle emitter components
	xScene.Query<Zenith_ParticleEmitterComponent>()
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

			// Copy alive particles to instance buffer
			const Zenith_Vector<Flux_Particle>& axParticles = xEmitter.GetParticles();
			uint32_t uAliveCount = xEmitter.GetAliveCount();

			for (uint32_t i = 0; i < uAliveCount && s_uInstanceCount < s_uMaxParticles; ++i)
			{
				s_axCPUInstances[s_uInstanceCount] = Flux_ParticleInstance::FromParticle(axParticles.Get(i));
				s_uInstanceCount++;
			}
		});
}

static void UploadInstanceData()
{
	if (s_uInstanceCount == 0)
	{
		return;
	}

	Flux_MemoryManager::UploadBufferData(
		s_xInstanceBuffer.GetBuffer().m_xVRAMHandle,
		s_axCPUInstances,
		s_uInstanceCount * sizeof(Flux_ParticleInstance)
	);
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

	// Update all emitters (both CPU and GPU) and build the CPU instance buffer
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

	// Render CPU particles
	if (s_uInstanceCount > 0)
	{
		g_xCommandList.Reset(false);

		g_xCommandList.AddCommand<Flux_CommandSetPipeline>(&s_xPipeline);

		g_xCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer(), 0);
		g_xCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());
		g_xCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&s_xInstanceBuffer, 1);

		g_xCommandList.AddCommand<Flux_CommandBeginBind>(0);
		g_xCommandList.AddCommand<Flux_CommandBindCBV>(&Flux_Graphics::s_xFrameConstantsBuffer.GetCBV(), 0);
		g_xCommandList.AddCommand<Flux_CommandBindSRV>(&s_pxParticleTexture->m_xSRV, 1);

		// Draw CPU particles (6 indices per quad, s_uInstanceCount instances)
		g_xCommandList.AddCommand<Flux_CommandDrawIndexed>(6, s_uInstanceCount);

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
