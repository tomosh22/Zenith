#include "Zenith.h"

#include "Flux/Fog/Flux_Fog.h"
#include "Flux/Fog/Flux_VolumeFog.h"
#include "Flux/Fog/Flux_GodRaysFog.h"
#include "Flux/Fog/Flux_RaymarchFog.h"
#include "Flux/Fog/Flux_FroxelFog.h"

#include "Flux/Flux.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/HDR/Flux_HDR.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "TaskSystem/Zenith_TaskSystem.h"

bool Flux_Fog::s_bEnabled = true;

static Zenith_Task g_xRenderTask(ZENITH_PROFILE_INDEX__FLUX_FOG, Flux_Fog::Render, nullptr);

static Flux_CommandList g_xCommandList("Fog");

static Flux_Shader s_xShader;
static Flux_Pipeline s_xPipeline;

DEBUGVAR bool dbg_bEnable = true;
DEBUGVAR u_int dbg_uVolFogTechnique = 0;  // 0 = Simple, 1 = Froxel, 2 = Raymarch, 3 = God Rays
u_int dbg_uVolFogDebugMode = 0;  // Debug visualization mode (non-static for external linkage)

static struct Flux_FogConstants
{
	Zenith_Maths::Vector4 m_xColour_Falloff = { 0.5,0.6,0.7,0.000075 };
	// Henyey-Greenstein phase function asymmetry parameter
	// g = 0.0: isotropic, g = 0.8: typical atmospheric haze, g = 0.95: Mie scattering
	float m_fPhaseG = 0.8f;
	float m_fPad[3] = { 0.f, 0.f, 0.f };
} dbg_xConstants;

// Cached binding handles from shader reflection
static Flux_BindingHandle s_xFrameConstantsBinding;
static Flux_BindingHandle s_xDepthBinding;

void Flux_Fog::Initialise()
{
	// Initialize simple fog shader
	s_xShader.Initialise("Flux_Fullscreen_UV.vert", "Fog/Flux_Fog.frag");

	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_NONE;

	Flux_PipelineSpecification xPipelineSpec;
	xPipelineSpec.m_pxTargetSetup = &Flux_HDR::GetHDRSceneTargetSetup();
	xPipelineSpec.m_pxShader = &s_xShader;
	xPipelineSpec.m_xVertexInputDesc = xVertexDesc;

	s_xShader.GetReflection().PopulateLayout(xPipelineSpec.m_xPipelineLayout);

	xPipelineSpec.m_bDepthTestEnabled = false;
	xPipelineSpec.m_bDepthWriteEnabled = false;

	Flux_PipelineBuilder::FromSpecification(s_xPipeline, xPipelineSpec);

	// Cache binding handles from shader reflection
	const Flux_ShaderReflection& xReflection = s_xShader.GetReflection();
	s_xFrameConstantsBinding = xReflection.GetBinding("FrameConstants");
	s_xDepthBinding = xReflection.GetBinding("g_xDepthTex");

	// Initialize shared volumetric fog infrastructure
	Flux_VolumeFog::Initialise();

	// Initialize all volumetric fog techniques (spatial-only, no temporal)
	Flux_GodRaysFog::Initialise();
	Flux_RaymarchFog::Initialise();
	Flux_FroxelFog::Initialise();

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({ "Render", "Enable", "Fog" }, dbg_bEnable);
	Zenith_DebugVariables::AddUInt32({ "Render", "Volumetric Fog", "Technique" }, dbg_uVolFogTechnique, 0, 3);
	Zenith_DebugVariables::AddUInt32({ "Render", "Volumetric Fog", "Debug Mode" }, dbg_uVolFogDebugMode, 0, 23);
	Zenith_DebugVariables::AddVector3({ "Render", "Fog", "Colour" }, dbg_xConstants.m_xColour_Falloff, 0., 1.);
	Zenith_DebugVariables::AddFloat({ "Render", "Fog", "Density" }, dbg_xConstants.m_xColour_Falloff.w, 0., 0.02);
	Zenith_DebugVariables::AddFloat({ "Render", "Fog", "Phase G" }, dbg_xConstants.m_fPhaseG, -0.99f, 0.99f);
#endif

	// Note: Fog ambient irradiance ratio is unified at 0.25 in Flux_VolumetricCommon.fxh
	// To make it runtime-adjustable, add to Flux_VolumeFogConstants and pass through uniform buffers
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Fog initialised (4 spatial-only techniques: Simple, Froxel, Raymarch, GodRays)");
}

void Flux_Fog::Reset()
{
	// Reset command list to ensure no stale GPU resource references, including descriptor bindings
	// This is called when the scene is reset (e.g., Play/Stop transitions in editor)
	g_xCommandList.Reset(true);

	// Reset all volumetric fog techniques (spatial-only, no temporal)
	Flux_VolumeFog::Reset();
	Flux_GodRaysFog::Reset();
	Flux_RaymarchFog::Reset();
	Flux_FroxelFog::Reset();

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

	Flux_ShaderBinder xBinder(g_xCommandList);
	xBinder.BindCBV(s_xFrameConstantsBinding, &Flux_Graphics::s_xFrameConstantsBuffer.GetCBV());
	xBinder.BindSRV(s_xDepthBinding, Flux_Graphics::GetDepthStencilSRV());
	xBinder.PushConstant(&dbg_xConstants, sizeof(Flux_FogConstants));

	g_xCommandList.AddCommand<Flux_CommandDrawIndexed>(6);

	Flux::SubmitCommandList(&g_xCommandList, Flux_HDR::GetHDRSceneTargetSetup(), RENDER_ORDER_FOG);
}

void Flux_Fog::Render(void*)
{
	if (!dbg_bEnable || !s_bEnabled)
	{
		return;
	}

	// Technique selection via debug variable (all spatial-only, no temporal effects)
	// 0 = Simple, 1 = Froxel, 2 = Raymarch, 3 = God Rays
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
			Flux_GodRaysFog::Render(nullptr);
			break;

		default:
			RenderSimpleFog();
			break;
	}
}