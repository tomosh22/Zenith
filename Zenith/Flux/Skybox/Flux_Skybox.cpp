#include "Zenith.h"

#include "Flux/Skybox/Flux_Skybox.h"

#include "Flux/Flux.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Graphics.h"

#include "EntityComponent/Components/Zenith_CameraComponent.h"

static Flux_CommandBuffer s_xCommandBuffer;

static Flux_Shader s_xShader;
static Flux_Pipeline s_xPipeline;

void Flux_Skybox::Initialise()
{
	s_xCommandBuffer.Initialise();

	s_xShader.Initialise("Flux_Fullscreen_UV.vert", "Skybox/Flux_Skybox.frag");

	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_NONE;
	xVertexDesc.m_xPerVertexLayout.GetElements().push_back(SHADER_DATA_TYPE_FLOAT3);
	xVertexDesc.m_xPerVertexLayout.GetElements().push_back(SHADER_DATA_TYPE_FLOAT2);
	xVertexDesc.m_xPerVertexLayout.CalculateOffsetsAndStrides();

	std::vector<Flux_BlendState> xBlendStates;
	xBlendStates.push_back({ BLEND_FACTOR_SRCALPHA, BLEND_FACTOR_ONEMINUSSRCALPHA, true });

	Zenith_Vulkan_PipelineSpecification xPipelineSpec(
		"Flux_Skybox",
		xVertexDesc,
		&s_xShader,
		xBlendStates,
		false,
		false,
		DEPTH_COMPARE_FUNC_ALWAYS,
		{ COLOUR_FORMAT_BGRA8_SRGB },
		DEPTHSTENCIL_FORMAT_NONE,
		"#TO_TODO: delete me",
		false,
		false,
		{ {0,0} },
		//{ {3,0} },
		Flux_Graphics::s_xFinalRenderTarget,
		LOAD_ACTION_CLEAR,
		STORE_ACTION_STORE,
		LOAD_ACTION_DONTCARE,
		STORE_ACTION_DONTCARE,
		RENDER_TARGET_USAGE_PRESENT
	);

	Flux_PipelineBuilder::FromSpecification(s_xPipeline, xPipelineSpec);

	Zenith_Log("Flux_Skybox initialised");
}

void Flux_Skybox::Render()
{
	Zenith_CameraComponent& xCamera = Zenith_Scene::GetCurrentScene().GetMainCamera();

	s_xCommandBuffer.BeginRecording();

	s_xCommandBuffer.SubmitTargetSetup(Flux_Graphics::s_xFinalRenderTarget);

	s_xCommandBuffer.SetPipeline(&s_xPipeline);

	s_xCommandBuffer.SetVertexBuffer(Flux_Graphics::s_xQuadVertexBuffer);
	s_xCommandBuffer.SetIndexBuffer(Flux_Graphics::s_xQuadIndexBuffer);

	s_xCommandBuffer.DrawIndexed(6);

	s_xCommandBuffer.EndRecording(RENDER_ORDER_SKYBOX);
}
