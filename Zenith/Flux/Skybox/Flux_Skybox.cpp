#include "Zenith.h"

#include "Flux/Skybox/Flux_Skybox.h"

#include "Flux/Flux.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/DeferredShading/Flux_DeferredShading.h"
#include "AssetHandling/Zenith_AssetHandler.h"

#ifndef ZENITH_MERGE_GBUFFER_PASSES
static Flux_CommandBuffer s_xCommandBuffer;
#endif

static Flux_Shader s_xShader;
static Flux_Pipeline s_xPipeline;

static Flux_Texture* s_pxCubemap = nullptr;

void Flux_Skybox::Initialise()
{
#ifndef ZENITH_MERGE_GBUFFER_PASSES
	s_xCommandBuffer.Initialise();
#endif

	s_xShader.Initialise("Flux_Fullscreen_UV.vert", "Skybox/Flux_Skybox.frag");

	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_NONE;
	xVertexDesc.m_xPerVertexLayout.GetElements().push_back(SHADER_DATA_TYPE_FLOAT3);
	xVertexDesc.m_xPerVertexLayout.GetElements().push_back(SHADER_DATA_TYPE_FLOAT2);
	xVertexDesc.m_xPerVertexLayout.CalculateOffsetsAndStrides();

	std::vector<Flux_BlendState> xBlendStates;
	xBlendStates.push_back({ BLEND_FACTOR_SRCALPHA, BLEND_FACTOR_ONEMINUSSRCALPHA, true });
	xBlendStates.push_back({ BLEND_FACTOR_SRCALPHA, BLEND_FACTOR_ONEMINUSSRCALPHA, true });
	xBlendStates.push_back({ BLEND_FACTOR_SRCALPHA, BLEND_FACTOR_ONEMINUSSRCALPHA, true });
	xBlendStates.push_back({ BLEND_FACTOR_SRCALPHA, BLEND_FACTOR_ONEMINUSSRCALPHA, true });

	Flux_PipelineSpecification xPipelineSpec(
		xVertexDesc,
		&s_xShader,
		xBlendStates,
		false,
		false,
		DEPTH_COMPARE_FUNC_ALWAYS,
		DEPTHSTENCIL_FORMAT_D32_SFLOAT,
		false,
		false,
		{ 1,1 },
		{ 0,0 },
		Flux_Graphics::s_xMRTTarget,
		false,
		false
	);

	Flux_PipelineBuilder::FromSpecification(s_xPipeline, xPipelineSpec);

	s_pxCubemap = &Zenith_AssetHandler::GetTexture("Cubemap");

	Zenith_Log("Flux_Skybox initialised");
}

void Flux_Skybox::Render()
{
#ifdef ZENITH_MERGE_GBUFFER_PASSES
	//#TO_TODO: fix up naming convention
	Flux_CommandBuffer& s_xCommandBuffer = Flux_DeferredShading::GetSkyboxCommandBuffer();

	s_xCommandBuffer.BeginRecording();
#else

	//#TO clearing as this is first pass of the frame
	s_xCommandBuffer.SubmitTargetSetup(Flux_Graphics::s_xMRTTarget, true, true, true);
#endif

	s_xCommandBuffer.SetPipeline(&s_xPipeline);

	s_xCommandBuffer.SetVertexBuffer(Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	s_xCommandBuffer.SetIndexBuffer(Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

	s_xCommandBuffer.BeginBind(BINDING_FREQUENCY_PER_FRAME);
	s_xCommandBuffer.BindBuffer(&Flux_Graphics::s_xFrameConstantsBuffer.GetBuffer(), 0);
	s_xCommandBuffer.BindTexture(s_pxCubemap, 1);

	s_xCommandBuffer.DrawIndexed(6);

#ifdef ZENITH_MERGE_GBUFFER_PASSES
	s_xCommandBuffer.EndRecording(RENDER_ORDER_GBUFFER);
#else
	s_xCommandBuffer.EndRecording(RENDER_ORDER_SKYBOX);
#endif
}