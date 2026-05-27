#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#include "Flux/Fog/Flux_GodRaysFogImpl.h"
#include "Flux/Fog/Flux_GodRaysFogImpl.h"
#include "Flux/Fog/Flux_VolumeFogImpl.h"

#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/HDR/Flux_HDRImpl.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "DebugVariables/Zenith_DebugVariables.h"

#ifdef ZENITH_TOOLS
#include "Flux/Slang/Flux_ShaderHotReload.h"
#endif


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
// God rays decay factor per sample along radial blur
// Decay = 0.97 means 3% intensity loss per sample
// With 64 samples: final_intensity = 0.97^64 = ~14% of original
// Lower values (0.9) give shorter, more defined shafts
// Higher values (0.99) give longer, softer shafts
// Range: [0.9, 1.0], typical: 0.95-0.98
DEBUGVAR float dbg_fGodRaysDecay = 0.97f;
DEBUGVAR float dbg_fGodRaysExposure = 0.3f;
DEBUGVAR float dbg_fGodRaysDensity = 1.0f;
DEBUGVAR float dbg_fGodRaysWeight = 0.5f;

// Cached constants for push constant (per-frame transient -- kept file-static).
static Flux_GodRaysConstants s_xConstants;


void Flux_GodRaysFogImpl::BuildPipelines()
{
	g_xEngine.GodRaysFog().m_xShader.Initialise(FluxShaderProgram::Fog_GodRays);

	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_NONE;

	Flux_PipelineSpecification xPipelineSpec;
	xPipelineSpec.m_aeColourAttachmentFormats[0] = HDR_SCENE_FORMAT;
	xPipelineSpec.m_uNumColourAttachments = 1;
	xPipelineSpec.m_eDepthStencilFormat = DEPTH_FORMAT;
	xPipelineSpec.m_pxShader = &g_xEngine.GodRaysFog().m_xShader;
	xPipelineSpec.m_xVertexInputDesc = xVertexDesc;

	g_xEngine.GodRaysFog().m_xShader.GetReflection().PopulateLayout(xPipelineSpec.m_xPipelineLayout);

	xPipelineSpec.m_bDepthTestEnabled = false;
	xPipelineSpec.m_bDepthWriteEnabled = false;

	// Additive blending for god rays
	xPipelineSpec.m_axBlendStates[0].m_bBlendEnabled = true;
	xPipelineSpec.m_axBlendStates[0].m_eSrcBlendFactor = BLEND_FACTOR_ONE;
	xPipelineSpec.m_axBlendStates[0].m_eDstBlendFactor = BLEND_FACTOR_ONE;

	Flux_PipelineBuilder::FromSpecification(g_xEngine.GodRaysFog().m_xPipeline, xPipelineSpec);
}

void Flux_GodRaysFogImpl::Initialise()
{
	BuildPipelines();

#ifdef ZENITH_DEBUG_VARIABLES
	g_xEngine.DebugVariables().AddUInt32({ "Render", "Volumetric Fog", "God Rays", "Sample Count" }, dbg_uGodRaysSamples, 8, 128);
	g_xEngine.DebugVariables().AddFloat({ "Render", "Volumetric Fog", "God Rays", "Decay" }, dbg_fGodRaysDecay, 0.9f, 1.0f);
	g_xEngine.DebugVariables().AddFloat({ "Render", "Volumetric Fog", "God Rays", "Exposure" }, dbg_fGodRaysExposure, 0.0f, 1.0f);
	g_xEngine.DebugVariables().AddFloat({ "Render", "Volumetric Fog", "God Rays", "Density" }, dbg_fGodRaysDensity, 0.0f, 2.0f);
	g_xEngine.DebugVariables().AddFloat({ "Render", "Volumetric Fog", "God Rays", "Weight" }, dbg_fGodRaysWeight, 0.0f, 1.0f);
#endif

#ifdef ZENITH_TOOLS
	static const FluxShaderProgram s_axPrograms[] = {
		FluxShaderProgram::Fog_GodRays,
	};
	Flux_ShaderHotReload::RegisterSubsystem([](){ g_xEngine.GodRaysFog().BuildPipelines(); },
		s_axPrograms, sizeof(s_axPrograms) / sizeof(s_axPrograms[0]));
#endif

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_GodRaysFog initialised");
}

void Flux_GodRaysFogImpl::Reset()
{
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_GodRaysFogImpl::Reset()");
}

void Flux_GodRaysFogImpl::Render(Flux_CommandList* pxCommandList)
{
	// Get sun direction from frame constants and project to screen space
	const Zenith_Maths::Vector3& xSunDir = g_xEngine.FluxGraphics().m_xFrameConstants.m_xSunDir_Pad;
	const Zenith_Maths::Matrix4& xViewProj = g_xEngine.FluxGraphics().m_xFrameConstants.m_xViewProjMat;
	const Zenith_Maths::Vector3& xCamPos = g_xEngine.FluxGraphics().m_xFrameConstants.m_xCamPos_Pad;

	// Calculate sun position far along sun direction from camera
	// Distance to place virtual sun position for screen-space projection
	// Must be far enough that sun direction remains consistent across screen
	// but not so far that floating-point precision becomes an issue
	// 10000m is sufficient for scenes up to ~1km while maintaining precision
	const float GOD_RAYS_SUN_PROJECTION_DISTANCE = 10000.0f;
	Zenith_Maths::Vector3 xSunWorldPos = xCamPos - xSunDir * GOD_RAYS_SUN_PROJECTION_DISTANCE;

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

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&g_xEngine.GodRaysFog().m_xPipeline);

	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);
	xBinder.BindCBV(g_xEngine.GodRaysFog().m_xShader, "FrameConstants", &g_xEngine.FluxGraphics().m_xFrameConstantsBuffer.GetCBV());
	xBinder.BindSRV(g_xEngine.GodRaysFog().m_xShader, "g_xDepthTex", g_xEngine.FluxGraphics().GetDepthStencilSRV());

	xBinder.BindDrawConstants(g_xEngine.GodRaysFog().m_xShader, "GodRaysConstants", &s_xConstants, sizeof(Flux_GodRaysConstants));

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
}
