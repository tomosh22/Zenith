#include "Zenith.h"

#include "Flux/DeferredShading/Flux_DeferredShading.h"

#include "Flux/Flux.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"

static Flux_CommandBuffer s_xCommandBuffer;

static Flux_Shader s_xShader;
static Flux_Pipeline s_xPipeline;

void Flux_DeferredShading::Initialise()
{
	s_xCommandBuffer.Initialise();

	s_xShader.Initialise("Flux_Fullscreen_UV.vert", "DeferredShading/Flux_DeferredShading.frag");

	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_NONE;
	xVertexDesc.m_xPerVertexLayout.GetElements().push_back(SHADER_DATA_TYPE_FLOAT3);
	xVertexDesc.m_xPerVertexLayout.GetElements().push_back(SHADER_DATA_TYPE_FLOAT2);
	xVertexDesc.m_xPerVertexLayout.CalculateOffsetsAndStrides();

	std::vector<Flux_BlendState> xBlendStates;
	xBlendStates.push_back({ BLEND_FACTOR_SRCALPHA, BLEND_FACTOR_ONEMINUSSRCALPHA, true });

	Zenith_Vulkan_PipelineSpecification xPipelineSpec(
		xVertexDesc,
		&s_xShader,
		xBlendStates,
		false,
		false,
		DEPTH_COMPARE_FUNC_ALWAYS,
		{ COLOUR_FORMAT_BGRA8_SRGB },
		DEPTHSTENCIL_FORMAT_D32_SFLOAT,
		false,
		false,
		{1,4},
		{0,0},
		Flux_Graphics::s_xFinalRenderTarget,
		LOAD_ACTION_CLEAR,
		STORE_ACTION_STORE,
		LOAD_ACTION_CLEAR,
		STORE_ACTION_STORE,
		RENDER_TARGET_USAGE_RENDERTARGET
	);

	Flux_PipelineBuilder::FromSpecification(s_xPipeline, xPipelineSpec);

	Zenith_Log("Flux_DeferredShading initialised");
}

void Flux_DeferredShading::Render()
{
	s_xCommandBuffer.BeginRecording();

	//#TO clearing as this is first pass of the frame
	s_xCommandBuffer.SubmitTargetSetup(Flux_Graphics::s_xFinalRenderTarget, true, true, true);

	s_xCommandBuffer.SetPipeline(&s_xPipeline);

	s_xCommandBuffer.SetVertexBuffer(Flux_Graphics::s_xQuadVertexBuffer);
	s_xCommandBuffer.SetIndexBuffer(Flux_Graphics::s_xQuadIndexBuffer);

	s_xCommandBuffer.BeginBind(BINDING_FREQUENCY_PER_FRAME);
	s_xCommandBuffer.BindBuffer(&Flux_Graphics::s_xFrameConstantsBuffer.GetBuffer(), 0);
	s_xCommandBuffer.BindTexture(&Flux_Graphics::GetGBufferTexture(MRT_INDEX_DIFFUSE), 1);
	s_xCommandBuffer.BindTexture(&Flux_Graphics::GetGBufferTexture(MRT_INDEX_NORMALSAMBIENT), 2);
	s_xCommandBuffer.BindTexture(&Flux_Graphics::GetGBufferTexture(MRT_INDEX_MATERIAL), 3);
	s_xCommandBuffer.BindTexture(&Flux_Graphics::GetGBufferTexture(MRT_INDEX_WORLDPOS), 4);

	s_xCommandBuffer.DrawIndexed(6);

	s_xCommandBuffer.EndRecording(RENDER_ORDER_APPLY_LIGHTING);
}
