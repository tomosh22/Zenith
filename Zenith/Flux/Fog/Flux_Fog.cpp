#include "Zenith.h"

#include "Flux/Fog/Flux_Fog.h"
#include "Flux/Fog/Flux_VolumeFog.h"
#include "Flux/Fog/Flux_GodRaysFog.h"
#include "Flux/Fog/Flux_RaymarchFog.h"
#include "Flux/Fog/Flux_FroxelFog.h"
#include "Flux/Fog/Flux_TemporalFog.h"
#include "Flux/Fog/Flux_LPVFog.h"

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
DEBUGVAR u_int dbg_uVolFogTechnique = 0;  // 0 = Simple, 1 = Froxel, 2 = Raymarch, 3 = LPV, 4 = Temporal+Froxel, 5 = God Rays
u_int dbg_uVolFogDebugMode = 0;  // Debug visualization mode (non-static for external linkage)

static struct Flux_FogConstants
{
	Zenith_Maths::Vector4 m_xColour_Falloff = { 0.5,0.6,0.7,0.000075 };
} dbg_xConstants;

void Flux_Fog::Initialise()
{
	// Initialize simple fog shader
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

	// Initialize shared volumetric fog infrastructure
	Flux_VolumeFog::Initialise();

	// Initialize all volumetric fog techniques
	Flux_GodRaysFog::Initialise();
	Flux_RaymarchFog::Initialise();
	Flux_FroxelFog::Initialise();
	Flux_TemporalFog::Initialise();
	Flux_LPVFog::Initialise();

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({ "Render", "Enable", "Fog" }, dbg_bEnable);
	Zenith_DebugVariables::AddUInt32({ "Render", "Volumetric Fog", "Technique" }, dbg_uVolFogTechnique, 0, 5);
	Zenith_DebugVariables::AddUInt32({ "Render", "Volumetric Fog", "Debug Mode" }, dbg_uVolFogDebugMode, 0, 23);
	Zenith_DebugVariables::AddVector3({ "Render", "Fog", "Colour" }, dbg_xConstants.m_xColour_Falloff, 0., 1.);
	Zenith_DebugVariables::AddFloat({ "Render", "Fog", "Density" }, dbg_xConstants.m_xColour_Falloff.w, 0., 0.02);
#endif

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Fog initialised (6 techniques available)");
}

void Flux_Fog::Reset()
{
	// Reset command list to ensure no stale GPU resource references, including descriptor bindings
	// This is called when the scene is reset (e.g., Play/Stop transitions in editor)
	g_xCommandList.Reset(true);

	// Reset all volumetric fog techniques
	Flux_VolumeFog::Reset();
	Flux_GodRaysFog::Reset();
	Flux_RaymarchFog::Reset();
	Flux_FroxelFog::Reset();
	Flux_TemporalFog::Reset();
	Flux_LPVFog::Reset();

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Fog::Reset() - Reset all fog systems");
}

void Flux_Fog::SubmitRenderTask()
{
	Zenith_TaskSystem::SubmitTask(&g_xRenderTask);
}

void Flux_Fog::WaitForRenderTask()
{
	g_xRenderTask.WaitUntilComplete();
}

void Flux_Fog::RenderSimpleFog()
{
	// Original simple exponential fog
	g_xCommandList.Reset(false);

	g_xCommandList.AddCommand<Flux_CommandSetPipeline>(&s_xPipeline);

	g_xCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	g_xCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

	g_xCommandList.AddCommand<Flux_CommandBeginBind>(0);
	g_xCommandList.AddCommand<Flux_CommandBindCBV>(&Flux_Graphics::s_xFrameConstantsBuffer.GetCBV(), 0);
	g_xCommandList.AddCommand<Flux_CommandBindSRV>(Flux_Graphics::GetDepthStencilSRV(), 1);

	g_xCommandList.AddCommand<Flux_CommandPushConstant>(&dbg_xConstants, sizeof(Flux_FogConstants));

	g_xCommandList.AddCommand<Flux_CommandDrawIndexed>(6);

	Flux::SubmitCommandList(&g_xCommandList, Flux_Graphics::s_xFinalRenderTarget_NoDepth, RENDER_ORDER_FOG);
}

void Flux_Fog::Render(void*)
{
	if (!dbg_bEnable)
	{
		return;
	}

	// Technique selection via debug variable
	// 0 = Simple, 1 = Froxel, 2 = Raymarch, 3 = LPV, 4 = Temporal+Froxel, 5 = God Rays
	switch (dbg_uVolFogTechnique)
	{
		case 0:
			RenderSimpleFog();
			break;

		case 1:
			Flux_FroxelFog::Render(nullptr);
			break;

		case 2:
			Flux_RaymarchFog::Render(nullptr);
			break;

		case 3:
			Flux_LPVFog::Render(nullptr);
			break;

		case 4:
			// Froxel + Temporal reprojection
			Flux_FroxelFog::Render(nullptr);
			Flux_TemporalFog::Render(nullptr);
			break;

		case 5:
			Flux_GodRaysFog::Render(nullptr);
			break;

		default:
			RenderSimpleFog();
			break;
	}
}