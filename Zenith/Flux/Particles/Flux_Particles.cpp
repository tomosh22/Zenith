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

static Flux_CommandBuffer s_xCommandBuffer;

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

static Flux_Texture* s_pxParticleTexture = nullptr;

void Flux_Particles::Initialise()
{
	s_xCommandBuffer.Initialise();

	s_xShader.Initialise("Particles/Flux_Particles.vert", "Particles/Flux_Particles.frag");

	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_TRIANGLES;
	xVertexDesc.m_xPerVertexLayout.GetElements().push_back(SHADER_DATA_TYPE_FLOAT3);
	xVertexDesc.m_xPerVertexLayout.GetElements().push_back(SHADER_DATA_TYPE_FLOAT2);
	xVertexDesc.m_xPerVertexLayout.CalculateOffsetsAndStrides();
	xVertexDesc.m_xPerInstanceLayout.GetElements().push_back(SHADER_DATA_TYPE_FLOAT4);//position radius
	xVertexDesc.m_xPerInstanceLayout.GetElements().push_back(SHADER_DATA_TYPE_FLOAT4);//colour
	xVertexDesc.m_xPerInstanceLayout.CalculateOffsetsAndStrides();

	std::vector<Flux_BlendState> xBlendStates;
	xBlendStates.push_back({ BLEND_FACTOR_SRCALPHA, BLEND_FACTOR_ONE, true });

	Flux_PipelineSpecification xPipelineSpec(
		xVertexDesc,
		&s_xShader,
		xBlendStates,
		true,
		false, //#TO don't write to depth, need to make sure nothing can draw over particles later in the frame
		DEPTH_COMPARE_FUNC_LESSEQUAL,
		DEPTHSTENCIL_FORMAT_D32_SFLOAT,
		true,
		false,
		{ 1,1 },
		{ 0,0 },
		Flux_Graphics::s_xFinalRenderTarget
	);

	Flux_PipelineBuilder::FromSpecification(s_xPipeline, xPipelineSpec);

	Flux_MemoryManager::InitialiseDynamicVertexBuffer(nullptr, s_uMaxParticles * sizeof(Particle), s_xInstanceBuffer, false);

	Zenith_AssetHandler::AddTexture2D(Zenith_GUID(), "Particle", "C:/dev/Zenith/Games/Test/Assets/Textures/particle.ztx");
	Zenith_AssetHandler::AddTexture2D(Zenith_GUID(), "ParticleSwirl", "C:/dev/Zenith/Games/Test/Assets/Textures/particleSwirl.ztx");
	s_pxParticleTexture = &Zenith_AssetHandler::GetTexture("ParticleSwirl");

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({ "Render", "Enable", "Particles" }, dbg_bEnable);
#endif

	Zenith_Log("Flux_Particles initialised");
}

void UploadInstanceData()
{
	Particle axParticles[] =
	{
		{{200.,1500 + sin(Zenith_Core::GetTimePassed()) * 200, 200.,300.}, {1.,0.,0.,1.}},
		{{400.,1500 + sin(Zenith_Core::GetTimePassed()) * 200, 400.,300.}, {0.,1.,0.,1.}},
		{{800.,1500 + sin(Zenith_Core::GetTimePassed()) * 200, 800.,300.}, {0.,0.,1.,1.}},
	};

	//#TO_TODO: need a buffer per frame in flight
	Flux_MemoryManager::UploadBufferData(s_xInstanceBuffer.GetBuffer(), axParticles, sizeof(axParticles));
}

void Flux_Particles::Render()
{
	if (!dbg_bEnable)
	{
		return;
	}

	UploadInstanceData();

	s_xCommandBuffer.BeginRecording();

	s_xCommandBuffer.SubmitTargetSetup(Flux_Graphics::s_xFinalRenderTarget);

	s_xCommandBuffer.SetPipeline(&s_xPipeline);

	s_xCommandBuffer.SetVertexBuffer(Flux_Graphics::s_xQuadMesh.GetVertexBuffer(), 0);
	s_xCommandBuffer.SetIndexBuffer(Flux_Graphics::s_xQuadMesh.GetIndexBuffer());
	s_xCommandBuffer.SetVertexBuffer(s_xInstanceBuffer, 1);

	s_xCommandBuffer.BeginBind(BINDING_FREQUENCY_PER_FRAME);
	s_xCommandBuffer.BindBuffer(&Flux_Graphics::s_xFrameConstantsBuffer.GetBuffer(), 0);
	s_xCommandBuffer.BindTexture(s_pxParticleTexture, 1);

	s_xCommandBuffer.DrawIndexed(6, 3);

	s_xCommandBuffer.EndRecording(RENDER_ORDER_PARTICLES);
}