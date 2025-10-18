#include "Zenith.h"

#include "Flux/SSAO/Flux_SSAO.h"

#include "Flux/Flux.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "TaskSystem/Zenith_TaskSystem.h"

static Zenith_Task g_xRenderTask(ZENITH_PROFILE_INDEX__FLUX_SSAO, Flux_SSAO::Render, nullptr);

static Flux_CommandList g_xCommandList("SSAO");

static Flux_Shader s_xShader;
static Flux_Pipeline s_xPipeline;

DEBUGVAR bool dbg_bEnable = true;

static struct Flux_FogConstants
{
	float m_fRadius = 0.058f;
	float m_fBias = 0.15f;
	float m_fIntensity = 1.f;
	float m_fKernelSize = 16.f;
} dbg_xConstants;

void Flux_SSAO::Initialise()
{
	s_xShader.Initialise("Flux_Fullscreen_UV.vert", "SSAO/Flux_SSAO.frag");

	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_NONE;

	Flux_PipelineSpecification xPipelineSpec;
	xPipelineSpec.m_pxTargetSetup = &Flux_Graphics::s_xFinalRenderTarget_NoDepth;
	xPipelineSpec.m_pxShader = &s_xShader;
	xPipelineSpec.m_xVertexInputDesc = xVertexDesc;

	Flux_PipelineLayout& xLayout = xPipelineSpec.m_xPipelineLayout;
	xLayout.m_uNumDescriptorSets = 1;
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_BUFFER;
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[1].m_eType = DESCRIPTOR_TYPE_TEXTURE;
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[2].m_eType = DESCRIPTOR_TYPE_TEXTURE;

	xPipelineSpec.m_bDepthTestEnabled = false;
	xPipelineSpec.m_bDepthWriteEnabled = false;

	xPipelineSpec.m_axBlendStates[0].m_bBlendEnabled = true;
	xPipelineSpec.m_axBlendStates[0].m_eSrcBlendFactor = BLEND_FACTOR_ZERO;
	xPipelineSpec.m_axBlendStates[0].m_eDstBlendFactor = BLEND_FACTOR_SRCALPHA;

	Flux_PipelineBuilder::FromSpecification(s_xPipeline, xPipelineSpec);

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({ "Render", "Enable", "SSAO" }, dbg_bEnable);
	Zenith_DebugVariables::AddFloat({ "Render", "SSAO", "Radius" }, dbg_xConstants.m_fRadius, 0.01f, 2.f);
	Zenith_DebugVariables::AddFloat({ "Render", "SSAO", "Bias" }, dbg_xConstants.m_fBias, 0.01f, 2.f);
	Zenith_DebugVariables::AddFloat({ "Render", "SSAO", "Intensity" }, dbg_xConstants.m_fIntensity, 0.01f, 2.f);
	Zenith_DebugVariables::AddFloat({ "Render", "SSAO", "Kernel Size" }, dbg_xConstants.m_fKernelSize, 16.f, 64.f);
#endif

	Zenith_Log("Flux_SSAO initialised");
}

void Flux_SSAO::SubmitRenderTask()
{
	Zenith_TaskSystem::SubmitTask(&g_xRenderTask);
}

void Flux_SSAO::WaitForRenderTask()
{
	g_xRenderTask.WaitUntilComplete();
}

void Flux_SSAO::Render(void*)
{
	if (!dbg_bEnable)
	{
		return;
	}

	g_xCommandList.Reset(false);

	g_xCommandList.AddCommand<Flux_CommandSetPipeline>(&s_xPipeline);

	g_xCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	g_xCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

	g_xCommandList.AddCommand<Flux_CommandBeginBind>(0);
	g_xCommandList.AddCommand<Flux_CommandBindBuffer>(&Flux_Graphics::s_xFrameConstantsBuffer.GetBuffer(), 0);
	g_xCommandList.AddCommand<Flux_CommandBindTexture>(&Flux_Graphics::GetDepthStencilTexture(), 1);
	g_xCommandList.AddCommand<Flux_CommandBindTexture>(&Flux_Graphics::GetGBufferTexture(MRT_INDEX_NORMALSAMBIENT), 1);

	g_xCommandList.AddCommand<Flux_CommandPushConstant>(&dbg_xConstants, sizeof(Flux_FogConstants));

	g_xCommandList.AddCommand<Flux_CommandDrawIndexed>(6);

	Flux::SubmitCommandList(&g_xCommandList, Flux_Graphics::s_xFinalRenderTarget_NoDepth, RENDER_ORDER_SSAO);
}