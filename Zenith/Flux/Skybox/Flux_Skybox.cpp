#include "Zenith.h"

#include "Flux/Skybox/Flux_Skybox.h"

#include "AssetHandling/Zenith_AssetHandler.h"
#include "Flux/Flux.h"
#include "Flux/Flux_CommandList.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/DeferredShading/Flux_DeferredShading.h"
#include "TaskSystem/Zenith_TaskSystem.h"

static Zenith_Task g_xRenderTask(ZENITH_PROFILE_INDEX__FLUX_SKYBOX, Flux_Skybox::Render, nullptr);

static Flux_CommandList g_xCommandList("Skybox");

static Flux_Shader s_xShader;
static Flux_Pipeline s_xPipeline;

static Flux_Texture s_xCubemapTexture;

void Flux_Skybox::Initialise()
{
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

	Flux_PipelineBuilder::FromSpecification(s_xPipeline, xPipelineSpec);

	s_xCubemapTexture = Zenith_AssetHandler::GetTexture("Cubemap");

	Zenith_Log("Flux_Skybox initialised");
}

void Flux_Skybox::SubmitRenderTask()
{
	Zenith_TaskSystem::SubmitTask(&g_xRenderTask);
}

void Flux_Skybox::WaitForRenderTask()
{
	g_xRenderTask.WaitUntilComplete();
}

void Flux_Skybox::Render(void*)
{
	g_xCommandList.Reset(true);
	g_xCommandList.AddCommand<Flux_CommandSetPipeline>(&s_xPipeline);
	g_xCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	g_xCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());
	g_xCommandList.AddCommand<Flux_CommandBeginBind>(0);
	g_xCommandList.AddCommand<Flux_CommandBindBuffer>(&Flux_Graphics::s_xFrameConstantsBuffer.GetBuffer(), 0);
	g_xCommandList.AddCommand<Flux_CommandBindTexture>(&s_xCubemapTexture, 1);
	g_xCommandList.AddCommand<Flux_CommandDrawIndexed>(6);
	Flux::SubmitCommandList(&g_xCommandList, Flux_Graphics::s_xMRTTarget, RENDER_ORDER_SKYBOX);
}