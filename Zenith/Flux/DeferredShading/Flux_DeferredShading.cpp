#include "Zenith.h"

#include "Flux/DeferredShading/Flux_DeferredShading.h"


#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Shadows/Flux_Shadows.h"
#include "TaskSystem/Zenith_TaskSystem.h"

static Zenith_Task g_xRenderTask(ZENITH_PROFILE_INDEX__FLUX_DEFERRED_SHADING, Flux_DeferredShading::Render, nullptr);

static Flux_CommandList g_xCommandList("Apply Lighting");

static Flux_Shader s_xShader;
static Flux_Pipeline s_xPipeline;

void Flux_DeferredShading::Initialise()
{
	s_xShader.Initialise("Flux_Fullscreen_UV.vert", "DeferredShading/Flux_DeferredShading.frag");

	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_NONE;

	Flux_PipelineSpecification xPipelineSpec;
	xPipelineSpec.m_pxTargetSetup = &Flux_Graphics::s_xFinalRenderTarget_NoDepth;
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
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[11].m_eType = DESCRIPTOR_TYPE_TEXTURE;

	xPipelineSpec.m_axBlendStates[0].m_eSrcBlendFactor = BLEND_FACTOR_ONE;
	xPipelineSpec.m_axBlendStates[0].m_eDstBlendFactor = BLEND_FACTOR_ONE;
	xPipelineSpec.m_axBlendStates[0].m_bBlendEnabled = false;

	xPipelineSpec.m_bDepthTestEnabled = false;
	xPipelineSpec.m_bDepthWriteEnabled = false;

	Flux_PipelineBuilder::FromSpecification(s_xPipeline, xPipelineSpec);

	Zenith_Log("Flux_DeferredShading initialised");
}

void Flux_DeferredShading::SubmitRenderTask()
{
	Zenith_TaskSystem::SubmitTask(&g_xRenderTask);
}

void Flux_DeferredShading::WaitForRenderTask()
{
	g_xRenderTask.WaitUntilComplete();
}

void Flux_DeferredShading::Render(void*)
{
	g_xCommandList.Reset(true);

	g_xCommandList.AddCommand<Flux_CommandSetPipeline>(&s_xPipeline);

	g_xCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	g_xCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

	g_xCommandList.AddCommand<Flux_CommandBeginBind>(0);
	g_xCommandList.AddCommand<Flux_CommandBindBuffer>(&Flux_Graphics::s_xFrameConstantsBuffer.GetBuffer(), 0);
	g_xCommandList.AddCommand<Flux_CommandBindTexture>(&Flux_Graphics::GetGBufferTexture(MRT_INDEX_DIFFUSE), 4);
	g_xCommandList.AddCommand<Flux_CommandBindTexture>(&Flux_Graphics::GetGBufferTexture(MRT_INDEX_NORMALSAMBIENT), 5);
	g_xCommandList.AddCommand<Flux_CommandBindTexture>(&Flux_Graphics::GetGBufferTexture(MRT_INDEX_MATERIAL), 6);
	g_xCommandList.AddCommand<Flux_CommandBindTexture>(&Flux_Graphics::GetGBufferTexture(MRT_INDEX_WORLDPOS), 7);
	g_xCommandList.AddCommand<Flux_CommandBindTexture>(&Flux_Graphics::GetDepthStencilTexture(), 8);

	constexpr uint32_t uFirstShadowTexBind = 9;
	for (uint32_t u = 0; u < ZENITH_FLUX_NUM_CSMS; u++)
	{
		g_xCommandList.AddCommand<Flux_CommandBindTexture>(&Flux_Shadows::GetCSMTexture(u), uFirstShadowTexBind + u);
	}
	constexpr uint32_t uFirstShadowBufferBind = 1;
	for (uint32_t u = 0; u < ZENITH_FLUX_NUM_CSMS; u++)
	{
		g_xCommandList.AddCommand<Flux_CommandBindBuffer>(&Flux_Shadows::GetShadowMatrixBuffer(u).GetBuffer(), uFirstShadowBufferBind + u);
	}

	g_xCommandList.AddCommand<Flux_CommandDrawIndexed>(6);

	Flux::SubmitCommandList(&g_xCommandList, Flux_Graphics::s_xFinalRenderTarget_NoDepth, RENDER_ORDER_APPLY_LIGHTING);
}