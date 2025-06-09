#include "Zenith.h"

#include "Flux/DeferredShading/Flux_DeferredShading.h"


#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Shadows/Flux_Shadows.h"

static Flux_CommandBuffer s_xCommandBuffer;

#ifdef ZENITH_MERGE_GBUFFER_PASSES
static Flux_CommandBuffer s_xGBufferCommandBuffer;
static Flux_CommandBuffer s_xSkyboxCommandBuffer;
static Flux_CommandBuffer s_xStaticMeshesCommandBuffer;
static Flux_CommandBuffer s_xAnimatedMeshesCommandBuffer;
static Flux_CommandBuffer s_xTerrainCommandBuffer;
#endif

static Flux_Shader s_xShader;
static Flux_Pipeline s_xPipeline;

Flux_CommandBuffer& Flux_DeferredShading::GetSkyboxCommandBuffer()
{
	return s_xSkyboxCommandBuffer;
}

Flux_CommandBuffer& Flux_DeferredShading::GetStaticMeshesCommandBuffer()
{
	return s_xStaticMeshesCommandBuffer;
}

Flux_CommandBuffer& Flux_DeferredShading::GetAnimatedMeshesCommandBuffer()
{
	return s_xAnimatedMeshesCommandBuffer;
}

Flux_CommandBuffer& Flux_DeferredShading::GetTerrainCommandBuffer()
{
	return s_xTerrainCommandBuffer;
}

void Flux_DeferredShading::Initialise()
{
	s_xCommandBuffer.Initialise();

	#ifdef ZENITH_MERGE_GBUFFER_PASSES
	s_xGBufferCommandBuffer.Initialise();

	s_xSkyboxCommandBuffer.Initialise(COMMANDTYPE_GRAPHICS, true);
	s_xStaticMeshesCommandBuffer.Initialise(COMMANDTYPE_GRAPHICS, true);
	s_xAnimatedMeshesCommandBuffer.Initialise(COMMANDTYPE_GRAPHICS, true);
	s_xTerrainCommandBuffer.Initialise(COMMANDTYPE_GRAPHICS, true);

	s_xGBufferCommandBuffer.CreateChild(s_xSkyboxCommandBuffer);
	s_xGBufferCommandBuffer.CreateChild(s_xStaticMeshesCommandBuffer);
	s_xGBufferCommandBuffer.CreateChild(s_xAnimatedMeshesCommandBuffer);
	s_xGBufferCommandBuffer.CreateChild(s_xTerrainCommandBuffer);
	#endif

	s_xShader.Initialise("Flux_Fullscreen_UV.vert", "DeferredShading/Flux_DeferredShading.frag");

	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_NONE;

	std::vector<Flux_BlendState> xBlendStates;
	xBlendStates.push_back({ BLEND_FACTOR_ONE, BLEND_FACTOR_ONE, false });

	Flux_PipelineSpecification xPipelineSpec;
	xPipelineSpec.m_pxTargetSetup = &Flux_Graphics::s_xFinalRenderTarget;
	xPipelineSpec.m_pxShader = &s_xShader;
	xPipelineSpec.m_xVertexInputDesc = xVertexDesc;

	Flux_PipelineLayout& xLayout = xPipelineSpec.m_xPipelineLayout;
	xLayout.m_uNumDescriptorSets = 1;
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_BUFFER;
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[1].m_eType = DESCRIPTOR_TYPE_BUFFER;
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[2].m_eType = DESCRIPTOR_TYPE_BUFFER;
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[3].m_eType = DESCRIPTOR_TYPE_BUFFER;
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[4].m_eType = DESCRIPTOR_TYPE_TEXTURE;
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[5].m_eType = DESCRIPTOR_TYPE_TEXTURE;
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[6].m_eType = DESCRIPTOR_TYPE_TEXTURE;
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[7].m_eType = DESCRIPTOR_TYPE_TEXTURE;
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[8].m_eType = DESCRIPTOR_TYPE_TEXTURE;
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[9].m_eType = DESCRIPTOR_TYPE_TEXTURE;
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[10].m_eType = DESCRIPTOR_TYPE_TEXTURE;
	
#if 0(
		xVertexDesc,
		&s_xShader,
		xBlendStates,
		false,
		false,
		DEPTH_COMPARE_FUNC_ALWAYS,
		DEPTHSTENCIL_FORMAT_D32_SFLOAT,
		false,
		false,
		{ 4,7 },
		{ 0,0 },
		Flux_Graphics::s_xFinalRenderTarget,
		false
	);
#endif

	Flux_PipelineBuilder::FromSpecification(s_xPipeline, xPipelineSpec);

	Zenith_Log("Flux_DeferredShading initialised");
}

void Flux_DeferredShading::BeginFrame()
{
	s_xGBufferCommandBuffer.BeginRecording();

	s_xGBufferCommandBuffer.SubmitTargetSetup(Flux_Graphics::s_xMRTTarget, true, true, true);
}

void Flux_DeferredShading::Render()
{
	s_xCommandBuffer.BeginRecording();

	s_xCommandBuffer.SubmitTargetSetup(Flux_Graphics::s_xFinalRenderTarget, true, false, false);

	s_xCommandBuffer.SetPipeline(&s_xPipeline);

	s_xCommandBuffer.SetVertexBuffer(Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	s_xCommandBuffer.SetIndexBuffer(Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

	s_xCommandBuffer.BeginBind(0);
	s_xCommandBuffer.BindBuffer(&Flux_Graphics::s_xFrameConstantsBuffer.GetBuffer(), 0);
	s_xCommandBuffer.BindTexture(&Flux_Graphics::GetGBufferTexture(MRT_INDEX_DIFFUSE), 4);
	s_xCommandBuffer.BindTexture(&Flux_Graphics::GetGBufferTexture(MRT_INDEX_NORMALSAMBIENT), 5);
	s_xCommandBuffer.BindTexture(&Flux_Graphics::GetGBufferTexture(MRT_INDEX_MATERIAL), 6);
	s_xCommandBuffer.BindTexture(&Flux_Graphics::GetGBufferTexture(MRT_INDEX_WORLDPOS), 7);

	constexpr uint32_t uFirstShadowTexBind = 8;
	for (uint32_t u = 0; u < ZENITH_FLUX_NUM_CSMS; u++)
	{
		s_xCommandBuffer.BindTexture(&Flux_Shadows::GetCSMTexture(u), uFirstShadowTexBind + u);
	}
	constexpr uint32_t uFirstShadowBufferBind = 1;
	for (uint32_t u = 0; u < ZENITH_FLUX_NUM_CSMS; u++)
	{
		s_xCommandBuffer.BindBuffer(&Flux_Shadows::GetShadowMatrixBuffer(u).GetBuffer(), uFirstShadowBufferBind + u);
	}

	s_xCommandBuffer.DrawIndexed(6);

	#ifdef ZENITH_MERGE_GBUFFER_PASSES
	s_xGBufferCommandBuffer.ExecuteChild(s_xSkyboxCommandBuffer);
	s_xGBufferCommandBuffer.ExecuteChild(s_xStaticMeshesCommandBuffer);
	s_xGBufferCommandBuffer.ExecuteChild(s_xAnimatedMeshesCommandBuffer);
	s_xGBufferCommandBuffer.ExecuteChild(s_xTerrainCommandBuffer);
	s_xGBufferCommandBuffer.EndRecording(RENDER_ORDER_GBUFFER);
	#endif

	s_xCommandBuffer.EndRecording(RENDER_ORDER_APPLY_LIGHTING);
}