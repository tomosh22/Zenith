#include "Zenith.h"

#include "Flux/Fog/Flux_Fog.h"

#include "Flux/Flux.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "TaskSystem/Zenith_TaskSystem.h"

static Zenith_Task g_xRenderTask(ZENITH_PROFILE_INDEX__FLUX_FOG, Flux_Fog::Render, nullptr);

static Flux_CommandList g_xCommandList("Fog");

static Flux_Shader s_xShader;
static Flux_Pipeline s_xPipeline;

DEBUGVAR bool dbg_bEnable = true;

static struct Flux_FogConstants
{
	Zenith_Maths::Vector4 m_xColour_Falloff = { 0.5,0.6,0.7,0.000075 };
} dbg_xConstants;

void Flux_Fog::Initialise()
{

	s_xShader.Initialise("Flux_Fullscreen_UV.vert", "Fog/Flux_Fog.frag");

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

	xPipelineSpec.m_bDepthTestEnabled = false;
	xPipelineSpec.m_bDepthWriteEnabled = false;

	Flux_PipelineBuilder::FromSpecification(s_xPipeline, xPipelineSpec);

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({ "Render", "Enable", "Fog" }, dbg_bEnable);
	Zenith_DebugVariables::AddVector3({ "Render", "Fog", "Colour" }, dbg_xConstants.m_xColour_Falloff, 0., 1.);
	Zenith_DebugVariables::AddFloat({ "Render", "Fog", "Density" }, dbg_xConstants.m_xColour_Falloff.w, 0., 0.02);
#endif

	Zenith_Log("Flux_Fog initialised");
}

void Flux_Fog::SubmitRenderTask()
{
	Zenith_TaskSystem::SubmitTask(&g_xRenderTask);
}

void Flux_Fog::WaitForRenderTask()
{
	g_xRenderTask.WaitUntilComplete();
}

void Flux_Fog::Render(void*)
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
	g_xCommandList.AddCommand<Flux_CommandBindSRV>(Flux_Graphics::GetDepthStencilSRV(), 1);

	g_xCommandList.AddCommand<Flux_CommandPushConstant>(&dbg_xConstants, sizeof(Flux_FogConstants));

	g_xCommandList.AddCommand<Flux_CommandDrawIndexed>(6);

	Flux::SubmitCommandList(&g_xCommandList, Flux_Graphics::s_xFinalRenderTarget_NoDepth, RENDER_ORDER_FOG);
}