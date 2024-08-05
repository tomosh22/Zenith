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

DEBUGVAR bool dbg_Enable = true;

static struct Flux_FogConstants
{
	float m_fFalloff;
} s_xConstants;

void Flux_Fog::Initialise()
{
	s_xCommandBuffer.Initialise();

	s_xShader.Initialise("Flux_Fullscreen_UV.vert", "Fog/Flux_Fog.frag");

	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_NONE;
	xVertexDesc.m_xPerVertexLayout.GetElements().push_back(SHADER_DATA_TYPE_FLOAT3);
	xVertexDesc.m_xPerVertexLayout.GetElements().push_back(SHADER_DATA_TYPE_FLOAT2);
	xVertexDesc.m_xPerVertexLayout.CalculateOffsetsAndStrides();

	std::vector<Flux_BlendState> xBlendStates;
	xBlendStates.push_back({ BLEND_FACTOR_SRCALPHA, BLEND_FACTOR_ONE, true });


	Flux_PipelineSpecification xPipelineSpec(
		xVertexDesc,
		&s_xShader,
		xBlendStates,
		false,
		false,
		DEPTH_COMPARE_FUNC_ALWAYS,
		DEPTHSTENCIL_FORMAT_D32_SFLOAT,
		true,
		false,
		{1,1},
		{0,0},
		Flux_Graphics::s_xFinalRenderTarget
	);

	Flux_PipelineBuilder::FromSpecification(s_xPipeline, xPipelineSpec);

#ifdef DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({ "Render", "Enable", "Fog" }, dbg_Enable);
	Zenith_DebugVariables::AddFloat({ "Render", "Fog", "Density" }, s_xConstants.m_fFalloff, 0., 0.0001);
#endif

	Zenith_Log("Flux_Fog initialised");
}

void Flux_Fog::Render()
{
	if (!dbg_Enable)
	{
		return;
	}

	s_xCommandBuffer.BeginRecording();

	s_xCommandBuffer.SubmitTargetSetup(Flux_Graphics::s_xFinalRenderTarget);

	s_xCommandBuffer.SetPipeline(&s_xPipeline);

	s_xCommandBuffer.SetVertexBuffer(Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	s_xCommandBuffer.SetIndexBuffer(Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

	s_xCommandBuffer.BeginBind(BINDING_FREQUENCY_PER_FRAME);
	s_xCommandBuffer.BindBuffer(&Flux_Graphics::s_xFrameConstantsBuffer.GetBuffer(), 0);
	s_xCommandBuffer.BindTexture(&Flux_Graphics::GetDepthStencilTexture(), 1);

	s_xCommandBuffer.PushConstant(&s_xConstants, sizeof(s_xConstants));

	s_xCommandBuffer.DrawIndexed(6);

	s_xCommandBuffer.EndRecording(RENDER_ORDER_FOG);
}
