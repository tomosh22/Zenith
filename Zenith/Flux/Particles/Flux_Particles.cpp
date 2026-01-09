#include "Zenith.h"

#include "Flux/Particles/Flux_Particles.h"

#include "Flux/Flux.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"
#include "AssetHandling/Zenith_AssetHandler.h"
#include "EntityComponent/Zenith_Scene.h"
//#include "EntityComponent/Components/Zenith_ParticleSystemComponent.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "TaskSystem/Zenith_TaskSystem.h"

static Zenith_Task g_xRenderTask(ZENITH_PROFILE_INDEX__FLUX_PFX, Flux_Particles::Render, nullptr);

static Flux_CommandList g_xCommandList("Particles");

static Flux_Shader s_xShader;
static Flux_Pipeline s_xPipeline;

static Flux_DynamicVertexBuffer s_xInstanceBuffer;

DEBUGVAR bool dbg_bEnable = true;

static constexpr uint32_t s_uMaxParticles = 1024;

struct Particle
{
	Zenith_Maths::Vector4 m_xPosition_Radius;
	Zenith_Maths::Vector4 m_xColour;
};

static Flux_Texture s_xParticleTexture;

void Flux_Particles::Initialise()
{
	s_xShader.Initialise("Particles/Flux_Particles.vert", "Particles/Flux_Particles.frag");

	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_TRIANGLES;
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT3);
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT2);
	xVertexDesc.m_xPerVertexLayout.CalculateOffsetsAndStrides();
	xVertexDesc.m_xPerInstanceLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT4);//position radius
	xVertexDesc.m_xPerInstanceLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT4);//colour
	xVertexDesc.m_xPerInstanceLayout.CalculateOffsetsAndStrides();

	Flux_PipelineSpecification xPipelineSpec;
	xPipelineSpec.m_pxTargetSetup = &Flux_Graphics::s_xFinalRenderTarget;
	xPipelineSpec.m_pxShader = &s_xShader;
	xPipelineSpec.m_xVertexInputDesc = xVertexDesc;

	Flux_PipelineLayout& xLayout = xPipelineSpec.m_xPipelineLayout;
	xLayout.m_uNumDescriptorSets = 1;
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_BUFFER;
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[1].m_eType = DESCRIPTOR_TYPE_TEXTURE;

	xPipelineSpec.m_bDepthWriteEnabled = false;

	Flux_PipelineBuilder::FromSpecification(s_xPipeline, xPipelineSpec);

	Flux_MemoryManager::InitialiseDynamicVertexBuffer(nullptr, s_uMaxParticles * sizeof(Particle), s_xInstanceBuffer, false);

	Zenith_AssetHandler::TextureData xParticleTexData = Zenith_AssetHandler::LoadTexture2DFromFile("C:/dev/Zenith/Games/Test/Assets/Textures/particle" ZENITH_TEXTURE_EXT);
	Zenith_AssetHandler::AddTexture(xParticleTexData);  // Created but not used currently
	xParticleTexData.FreeAllocatedData();

	Zenith_AssetHandler::TextureData xParticleSwirlTexData = Zenith_AssetHandler::LoadTexture2DFromFile("C:/dev/Zenith/Games/Test/Assets/Textures/particleSwirl" ZENITH_TEXTURE_EXT);
	Flux_Texture* pxParticleSwirlTex = Zenith_AssetHandler::AddTexture(xParticleSwirlTexData);
	xParticleSwirlTexData.FreeAllocatedData();

	if (pxParticleSwirlTex)
	{
		s_xParticleTexture = *pxParticleSwirlTex;
	}

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({ "Render", "Enable", "Particles" }, dbg_bEnable);
#endif

	Zenith_Log(LOG_CATEGORY_PARTICLES, "Flux_Particles initialised");
}

void Flux_Particles::Reset()
{
	// Reset command list to ensure no stale GPU resource references, including descriptor bindings
	// This is called when the scene is reset (e.g., Play/Stop transitions in editor)
	g_xCommandList.Reset(true);
	Zenith_Log(LOG_CATEGORY_PARTICLES, "Flux_Particles::Reset() - Reset command list");
}

void Flux_Particles::Shutdown()
{
	Flux_MemoryManager::DestroyDynamicVertexBuffer(s_xInstanceBuffer);
	Zenith_Log(LOG_CATEGORY_PARTICLES, "Flux_Particles shut down");
}

void UploadInstanceData()
{
	Particle axParticles[] =
	{
		{{2000,500 + sin(Zenith_Core::GetTimePassed()) * 200, 1500,300.}, {1.,0.,0.,1.}},
		{{2100,500 + sin(Zenith_Core::GetTimePassed()) * 200, 1600,300.}, {0.,1.,0.,1.}},
		{{2200,500 + sin(Zenith_Core::GetTimePassed()) * 200, 1700,300.}, {0.,0.,1.,1.}},
	};

	Flux_MemoryManager::UploadBufferData(s_xInstanceBuffer.GetBuffer().m_xVRAMHandle, axParticles, sizeof(axParticles));
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

	UploadInstanceData();

	g_xCommandList.Reset(false);

	g_xCommandList.AddCommand<Flux_CommandSetPipeline>(&s_xPipeline);

	g_xCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer(), 0);
	g_xCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());
	g_xCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&s_xInstanceBuffer, 1);

	g_xCommandList.AddCommand<Flux_CommandBeginBind>(0);
	g_xCommandList.AddCommand<Flux_CommandBindCBV>(&Flux_Graphics::s_xFrameConstantsBuffer.GetCBV(), 0);
	g_xCommandList.AddCommand<Flux_CommandBindSRV>(&s_xParticleTexture.m_xSRV, 1);

	g_xCommandList.AddCommand<Flux_CommandDrawIndexed>(6, 3);

	Flux::SubmitCommandList(&g_xCommandList, Flux_Graphics::s_xFinalRenderTarget, RENDER_ORDER_PARTICLES);
}