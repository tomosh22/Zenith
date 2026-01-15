#include "Zenith.h"

#include "Flux/Fog/Flux_GodRaysFog.h"
#include "Flux/Fog/Flux_VolumeFog.h"

#include "Flux/Flux.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "TaskSystem/Zenith_TaskSystem.h"

static Zenith_Task g_xRenderTask(ZENITH_PROFILE_INDEX__FLUX_FOG, Flux_GodRaysFog::Render, nullptr);

static Flux_CommandList g_xCommandList("GodRays");

static Flux_Shader s_xShader;
static Flux_Pipeline s_xPipeline;

// God rays specific parameters
struct Flux_GodRaysConstants
{
	Zenith_Maths::Vector4 m_xLightScreenPos_Pad;  // xy = light screen pos (0-1), zw = unused
	Zenith_Maths::Vector4 m_xParams;              // x = decay, y = exposure, z = density, w = weight
	u_int m_uNumSamples;
	u_int m_uDebugMode;
	float m_fPad0;
	float m_fPad1;
};

// Debug variables
DEBUGVAR u_int dbg_uGodRaysSamples = 64;
DEBUGVAR float dbg_fGodRaysDecay = 0.97f;
DEBUGVAR float dbg_fGodRaysExposure = 0.3f;
DEBUGVAR float dbg_fGodRaysDensity = 1.0f;
DEBUGVAR float dbg_fGodRaysWeight = 0.5f;

// Cached constants for push constant
static Flux_GodRaysConstants s_xConstants;

// Cached binding handles from shader reflection
static Flux_BindingHandle s_xFrameConstantsBinding;
static Flux_BindingHandle s_xDepthBinding;

void Flux_GodRaysFog::Initialise()
{
	s_xShader.Initialise("Flux_Fullscreen_UV.vert", "Fog/Flux_GodRays.frag");

	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_NONE;

	Flux_PipelineSpecification xPipelineSpec;
	xPipelineSpec.m_pxTargetSetup = &Flux_Graphics::s_xFinalRenderTarget_NoDepth;
	xPipelineSpec.m_pxShader = &s_xShader;
	xPipelineSpec.m_xVertexInputDesc = xVertexDesc;

	Flux_PipelineLayout& xLayout = xPipelineSpec.m_xPipelineLayout;
	xLayout.m_uNumDescriptorSets = 1;
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_BUFFER;   // Frame constants
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[1].m_eType = DESCRIPTOR_TYPE_BUFFER;   // Scratch buffer for push constants
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[2].m_eType = DESCRIPTOR_TYPE_TEXTURE;  // Depth texture

	xPipelineSpec.m_bDepthTestEnabled = false;
	xPipelineSpec.m_bDepthWriteEnabled = false;

	// Additive blending for god rays
	xPipelineSpec.m_axBlendStates[0].m_bBlendEnabled = true;
	xPipelineSpec.m_axBlendStates[0].m_eSrcBlendFactor = BLEND_FACTOR_ONE;
	xPipelineSpec.m_axBlendStates[0].m_eDstBlendFactor = BLEND_FACTOR_ONE;

	Flux_PipelineBuilder::FromSpecification(s_xPipeline, xPipelineSpec);

	// Cache binding handles from shader reflection
	const Flux_ShaderReflection& xReflection = s_xShader.GetReflection();
	s_xFrameConstantsBinding = xReflection.GetBinding("FrameConstants");
	s_xDepthBinding = xReflection.GetBinding("g_xDepthTex");

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddUInt32({ "Render", "Volumetric Fog", "God Rays", "Sample Count" }, dbg_uGodRaysSamples, 8, 128);
	Zenith_DebugVariables::AddFloat({ "Render", "Volumetric Fog", "God Rays", "Decay" }, dbg_fGodRaysDecay, 0.9f, 1.0f);
	Zenith_DebugVariables::AddFloat({ "Render", "Volumetric Fog", "God Rays", "Exposure" }, dbg_fGodRaysExposure, 0.0f, 1.0f);
	Zenith_DebugVariables::AddFloat({ "Render", "Volumetric Fog", "God Rays", "Density" }, dbg_fGodRaysDensity, 0.0f, 2.0f);
	Zenith_DebugVariables::AddFloat({ "Render", "Volumetric Fog", "God Rays", "Weight" }, dbg_fGodRaysWeight, 0.0f, 1.0f);
#endif

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_GodRaysFog initialised");
}

void Flux_GodRaysFog::Reset()
{
	g_xCommandList.Reset(true);
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_GodRaysFog::Reset()");
}

void Flux_GodRaysFog::SubmitRenderTask()
{
	Zenith_TaskSystem::SubmitTask(&g_xRenderTask);
}

void Flux_GodRaysFog::WaitForRenderTask()
{
	g_xRenderTask.WaitUntilComplete();
}

void Flux_GodRaysFog::Render(void*)
{
	// Get sun direction from frame constants and project to screen space
	const Zenith_Maths::Vector3& xSunDir = Flux_Graphics::s_xFrameConstants.m_xSunDir_Pad;
	const Zenith_Maths::Matrix4& xViewProj = Flux_Graphics::s_xFrameConstants.m_xViewProjMat;
	const Zenith_Maths::Vector3& xCamPos = Flux_Graphics::s_xFrameConstants.m_xCamPos_Pad;

	// Calculate sun position far along sun direction from camera
	Zenith_Maths::Vector3 xSunWorldPos = xCamPos - xSunDir * 10000.0f;

	// Project to clip space
	Zenith_Maths::Vector4 xClipPos = xViewProj * Zenith_Maths::Vector4(xSunWorldPos, 1.0f);

	// Perspective divide
	Zenith_Maths::Vector2 xSunScreenPos;
	if (xClipPos.w > 0.0f)
	{
		xSunScreenPos.x = (xClipPos.x / xClipPos.w) * 0.5f + 0.5f;
		xSunScreenPos.y = (xClipPos.y / xClipPos.w) * 0.5f + 0.5f;
	}
	else
	{
		// Sun behind camera - place off screen
		xSunScreenPos.x = -1.0f;
		xSunScreenPos.y = -1.0f;
	}

	// Update constants
	s_xConstants.m_xLightScreenPos_Pad = Zenith_Maths::Vector4(xSunScreenPos.x, xSunScreenPos.y, 0.0f, 0.0f);
	s_xConstants.m_xParams = Zenith_Maths::Vector4(dbg_fGodRaysDecay, dbg_fGodRaysExposure, dbg_fGodRaysDensity, dbg_fGodRaysWeight);
	s_xConstants.m_uNumSamples = dbg_uGodRaysSamples;

	// Check current debug mode
	extern u_int dbg_uVolFogDebugMode;
	s_xConstants.m_uDebugMode = dbg_uVolFogDebugMode;

	g_xCommandList.Reset(false);

	g_xCommandList.AddCommand<Flux_CommandSetPipeline>(&s_xPipeline);

	g_xCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	g_xCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(g_xCommandList);
	xBinder.BindCBV(s_xFrameConstantsBinding, &Flux_Graphics::s_xFrameConstantsBuffer.GetCBV());
	xBinder.BindSRV(s_xDepthBinding, Flux_Graphics::GetDepthStencilSRV());

	xBinder.PushConstant(&s_xConstants, sizeof(Flux_GodRaysConstants));

	g_xCommandList.AddCommand<Flux_CommandDrawIndexed>(6);

	Flux::SubmitCommandList(&g_xCommandList, Flux_Graphics::s_xFinalRenderTarget_NoDepth, RENDER_ORDER_FOG);
}
