#include "Zenith.h"

#include "Flux/Skybox/Flux_Skybox.h"

#include "Flux/Flux.h"
#include "Flux/Flux_CommandList.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/DeferredShading/Flux_DeferredShading.h"
#include "AssetHandling/Zenith_AssetHandler.h"

#ifndef ZENITH_MERGE_GBUFFER_PASSES
static Flux_CommandBuffer s_xCommandBuffer;
#endif
static Flux_CommandList g_xCommandList;

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
	xVertexDesc.m_xPerVertexLayout.CalculateOffsetsAndStrides();

	Flux_PipelineSpecification xPipelineSpec;
	xPipelineSpec.m_pxTargetSetup = &Flux_Graphics::s_xMRTTarget;
	xPipelineSpec.m_pxShader = &s_xShader;
	xPipelineSpec.m_xVertexInputDesc = xVertexDesc;

	Flux_PipelineLayout& xLayout = xPipelineSpec.m_xPipelineLayout;
	xLayout.m_uNumDescriptorSets = 1;
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_BUFFER;
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[1].m_eType = DESCRIPTOR_TYPE_TEXTURE;

	for (Flux_BlendState& xBlendState : xPipelineSpec.m_axBlendStates)
	{
		xBlendState.m_eSrcBlendFactor = BLEND_FACTOR_ONE;
		xBlendState.m_eDstBlendFactor = BLEND_FACTOR_ZERO;
		xBlendState.m_bBlendEnabled = false;
	}

	xPipelineSpec.m_bDepthTestEnabled = false;
#if 0
	(
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
		false
	);
#endif

	Flux_PipelineBuilder::FromSpecification(s_xPipeline, xPipelineSpec);

	s_pxCubemap = Zenith_AssetHandler::GetTexture("Cubemap");

	Zenith_Log("Flux_Skybox initialised");
}

void Flux_Skybox::Render()
{
#ifdef ZENITH_MERGE_GBUFFER_PASSES
	//#TO_TODO: fix up naming convention
	Flux_CommandBuffer& s_xCommandBuffer = Flux_DeferredShading::GetSkyboxCommandBuffer();

	s_xCommandBuffer.BeginRecording();
#else
	s_xCommandBuffer.BeginRecording();
	//#TO clearing as this is first pass of the frame
	s_xCommandBuffer.SubmitTargetSetup(Flux_Graphics::s_xMRTTarget, true, true, true);
#endif


	g_xCommandList.Reset();
	g_xCommandList.AddCommand<Flux_CommandSetPipeline>(&s_xPipeline);
	g_xCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	g_xCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());
	g_xCommandList.AddCommand<Flux_CommandBeginBind>(0);
	g_xCommandList.AddCommand<Flux_CommandBindBuffer>(&Flux_Graphics::s_xFrameConstantsBuffer.GetBuffer(), 0);
	g_xCommandList.AddCommand<Flux_CommandBindTexture>(s_pxCubemap, 1);
	g_xCommandList.AddCommand<Flux_CommandDrawIndexed>(6);
	g_xCommandList.IterateCommands(&s_xCommandBuffer);



#ifdef ZENITH_MERGE_GBUFFER_PASSES
	s_xCommandBuffer.EndRecording(RENDER_ORDER_GBUFFER);
#else
	s_xCommandBuffer.EndRecording(RENDER_ORDER_SKYBOX);
#endif
}