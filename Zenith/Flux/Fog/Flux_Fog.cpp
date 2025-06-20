#include "Zenith.h"

#include "Flux/Fog/Flux_Fog.h"

#include "Flux/Flux.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"
#include "DebugVariables/Zenith_DebugVariables.h"

static Flux_CommandBuffer s_xCommandBuffer;

static Flux_Shader s_xShader;
static Flux_Pipeline s_xPipeline;

DEBUGVAR bool dbg_bEnable = true;

static struct Flux_FogConstants
{
	Zenith_Maths::Vector4 m_xColour_Falloff = { 0.5,0.6,0.7,0.000075 };
} dbg_xConstants;

void Flux_Fog::Initialise()
{
	s_xCommandBuffer.Initialise();

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
#if 0
	(
		xVertexDesc,
		&s_xShader,
		xBlendStates,
		false,
		false,
		DEPTH_COMPARE_FUNC_ALWAYS,
		DEPTHSTENCIL_FORMAT_D32_SFLOAT,
		true,
		false,
		{ 1,1 },
		{ 0,0 },
		Flux_Graphics::s_xFinalRenderTarget,
		false
	);
#endif

	Flux_PipelineBuilder::FromSpecification(s_xPipeline, xPipelineSpec);

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({ "Render", "Enable", "Fog" }, dbg_bEnable);
	Zenith_DebugVariables::AddVector3({ "Render", "Fog", "Colour" }, dbg_xConstants.m_xColour_Falloff, 0., 1.);
	Zenith_DebugVariables::AddFloat({ "Render", "Fog", "Density" }, dbg_xConstants.m_xColour_Falloff.w, 0., 0.02);
#endif

	Zenith_Log("Flux_Fog initialised");
}

void Flux_Fog::Render()
{
	if (!dbg_bEnable)
	{
		return;
	}

	s_xCommandBuffer.BeginRecording();

	s_xCommandBuffer.SubmitTargetSetup(Flux_Graphics::s_xFinalRenderTarget_NoDepth);

	s_xCommandBuffer.SetPipeline(&s_xPipeline);

	s_xCommandBuffer.SetVertexBuffer(Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	s_xCommandBuffer.SetIndexBuffer(Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

	s_xCommandBuffer.BeginBind(0);
	s_xCommandBuffer.BindBuffer(&Flux_Graphics::s_xFrameConstantsBuffer.GetBuffer(), 0);
	s_xCommandBuffer.BindTexture(&Flux_Graphics::GetDepthStencilTexture(), 1);

	s_xCommandBuffer.PushConstant(&dbg_xConstants, sizeof(Flux_FogConstants));

	s_xCommandBuffer.DrawIndexed(6);

	s_xCommandBuffer.EndRecording(RENDER_ORDER_FOG);
}