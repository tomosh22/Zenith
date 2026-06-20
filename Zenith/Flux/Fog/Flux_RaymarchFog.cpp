#include "Zenith.h"
#include "Flux/Flux_RendererImpl.h"
#include "Core/Zenith_Engine.h"
#include "Core/FrameContext.h"

#include "Flux/Fog/Flux_RaymarchFogImpl.h"
#include "Flux/Fog/Flux_RaymarchFogImpl.h"
#include "Flux/Fog/Flux_VolumeFogImpl.h"

#include "AssetHandling/Zenith_TextureAsset.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/HDR/Flux_HDRImpl.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Flux/Shadows/Flux_ShadowsImpl.h"
#include "DebugVariables/Zenith_DebugVariables.h"


// Debug variables
DEBUGVAR u_int dbg_uRaymarchSteps = 64;
DEBUGVAR float dbg_fRaymarchNoiseScale = 0.02f;
DEBUGVAR float dbg_fRaymarchNoiseSpeed = 0.1f;
DEBUGVAR float dbg_fRaymarchMaxDistance = 500.0f;
DEBUGVAR float dbg_fRaymarchHeightFalloff = 0.01f;
// Henyey-Greenstein phase function asymmetry parameter
// -1.0 = pure backscatter, 0.0 = isotropic, 0.6 = typical fog (forward scatter), 1.0 = pure forward
DEBUGVAR float dbg_fRaymarchPhaseG = 0.6f;
// Volumetric shadow parameters (unified with Froxel fog for consistent shadow softness)
DEBUGVAR float dbg_fRaymarchShadowBias = 0.001f;       // Shadow bias - prevents self-shadowing artifacts
DEBUGVAR float dbg_fRaymarchShadowConeRadius = 0.002f; // Cone spread - controls soft shadow edge

void Flux_RaymarchFogImpl::BuildPipelines()
{
	m_xShader.Initialise(FluxShaderProgram::Fog_Raymarch);

	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_NONE;

	Flux_PipelineSpecification xPipelineSpec;
	xPipelineSpec.m_aeColourAttachmentFormats[0] = HDR_SCENE_FORMAT;
	xPipelineSpec.m_uNumColourAttachments = 1;
	xPipelineSpec.m_eDepthStencilFormat = DEPTH_FORMAT;
	xPipelineSpec.m_pxShader = &m_xShader;
	xPipelineSpec.m_xVertexInputDesc = xVertexDesc;

	m_xShader.GetReflection().PopulateLayout(xPipelineSpec.m_xPipelineLayout);

	xPipelineSpec.m_bDepthTestEnabled = false;
	xPipelineSpec.m_bDepthWriteEnabled = false;

	// Alpha blending for fog overlay
	xPipelineSpec.m_axBlendStates[0].m_bBlendEnabled = true;
	xPipelineSpec.m_axBlendStates[0].m_eSrcBlendFactor = BLEND_FACTOR_SRCALPHA;
	xPipelineSpec.m_axBlendStates[0].m_eDstBlendFactor = BLEND_FACTOR_ONEMINUSSRCALPHA;

	Flux_PipelineBuilder::FromSpecification(m_xPipeline, xPipelineSpec);
}

void Flux_RaymarchFogImpl::Initialise()
{
	BuildPipelines();

#ifdef ZENITH_DEBUG_VARIABLES
	g_xEngine.DebugVariables().AddUInt32({ "Render", "Volumetric Fog", "Raymarch", "Step Count" }, dbg_uRaymarchSteps, 8, 256);
	g_xEngine.DebugVariables().AddFloat({ "Render", "Volumetric Fog", "Raymarch", "Noise Scale" }, dbg_fRaymarchNoiseScale, 0.001f, 0.1f);
	g_xEngine.DebugVariables().AddFloat({ "Render", "Volumetric Fog", "Raymarch", "Noise Speed" }, dbg_fRaymarchNoiseSpeed, 0.0f, 1.0f);
	g_xEngine.DebugVariables().AddFloat({ "Render", "Volumetric Fog", "Raymarch", "Max Distance" }, dbg_fRaymarchMaxDistance, 50.0f, 1000.0f);
	g_xEngine.DebugVariables().AddFloat({ "Render", "Volumetric Fog", "Raymarch", "Height Falloff" }, dbg_fRaymarchHeightFalloff, 0.0f, 0.1f);
	g_xEngine.DebugVariables().AddFloat({ "Render", "Volumetric Fog", "Raymarch", "Phase G" }, dbg_fRaymarchPhaseG, -0.9f, 0.9f);
	// Volumetric shadow parameters (unified with Froxel fog)
	g_xEngine.DebugVariables().AddFloat({ "Render", "Volumetric Fog", "Raymarch", "Shadow Bias" }, dbg_fRaymarchShadowBias, 0.0001f, 0.01f);
	g_xEngine.DebugVariables().AddFloat({ "Render", "Volumetric Fog", "Raymarch", "Shadow Cone Radius" }, dbg_fRaymarchShadowConeRadius, 0.0001f, 0.01f);
#endif

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_RaymarchFog initialised");
}

void Flux_RaymarchFogImpl::Reset()
{
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_RaymarchFogImpl::Reset()");
}

void Flux_RaymarchFogImpl::Render(Flux_CommandBuffer* pxCommandList)
{
	// Get shared fog parameters
	Flux_VolumeFogImpl& xVolumeFog = g_xEngine.VolumeFog();
	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();
	Flux_ShadowsImpl& xShadows = g_xEngine.Shadows();
	Flux_VolumeFogConstants& xShared = xVolumeFog.GetSharedConstants();

	// Update constants
	m_xConstants.m_xFogColour = xShared.m_xFogColour;
	m_xConstants.m_xFogParams = Zenith_Maths::Vector4(
		xShared.m_fDensity,
		xShared.m_fScatteringCoeff,
		xShared.m_fAbsorptionCoeff,
		dbg_fRaymarchMaxDistance
	);

	// Get elapsed time for noise animation using actual frame delta time
	// Wrap time to prevent float precision loss after extended runtime
	static float s_fTime = 0.0f;
	float fDeltaTime = g_xEngine.Frame().GetDt();  // Actual frame delta, frame-rate independent
	s_fTime += fDeltaTime * dbg_fRaymarchNoiseSpeed;
	s_fTime = std::fmod(s_fTime, 1000.0f);  // Wrap every 1000 seconds to maintain precision

	m_xConstants.m_xNoiseParams = Zenith_Maths::Vector4(
		dbg_fRaymarchNoiseScale,
		dbg_fRaymarchNoiseSpeed,
		1.0f,  // detail
		s_fTime
	);

	m_xConstants.m_xHeightParams = Zenith_Maths::Vector4(
		0.0f,  // base height
		dbg_fRaymarchHeightFalloff,
		0.0f,
		0.0f
	);

	m_xConstants.m_uNumSteps = dbg_uRaymarchSteps;

	// Check current debug mode
	extern u_int dbg_uVolFogDebugMode;
	m_xConstants.m_uDebugMode = dbg_uVolFogDebugMode;
	m_xConstants.m_uFrameIndex = g_xEngine.Frame().GetFrameIndex();
	m_xConstants.m_fPhaseG = dbg_fRaymarchPhaseG;

	// Volumetric shadow parameters (unified with Froxel fog for consistent shadow softness)
	m_xConstants.m_fVolShadowBias = dbg_fRaymarchShadowBias;
	m_xConstants.m_fVolShadowConeRadius = dbg_fRaymarchShadowConeRadius;
	m_xConstants.m_fAmbientIrradianceRatio = xShared.m_fAmbientIrradianceRatio;
	m_xConstants.m_fNoiseWorldScale = xShared.m_fNoiseWorldScale;

	pxCommandList->SetPipeline(&m_xPipeline);

	pxCommandList->SetVertexBuffer(xGraphics.m_xQuadMesh.GetVertexBuffer());
	pxCommandList->SetIndexBuffer(xGraphics.m_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);
	xBinder.BindCBV(m_xShader, "FrameConstants", &xGraphics.m_xFrameConstantsBuffer.GetCBV());
	xBinder.BindSRV(m_xShader, "u_xDepthTexture", xGraphics.GetDepthStencilSRV());
	xBinder.BindSRV(m_xShader, "u_xNoiseTexture3D", &xVolumeFog.GetNoiseTexture3D()->m_xSRV);
	xBinder.BindSRV(m_xShader, "u_xBlueNoiseTexture", &xVolumeFog.GetBlueNoiseTexture()->m_xSRV);

	// Bind CSM shadow maps and matrices for volumetric shadows
	static const char* const s_aszCSMNames[ZENITH_FLUX_NUM_CSMS] = { "u_xCSM0", "u_xCSM1", "u_xCSM2", "u_xCSM3" };
	static const char* const s_aszShadowMatrixNames[ZENITH_FLUX_NUM_CSMS] = { "ShadowMatrix0", "ShadowMatrix1", "ShadowMatrix2", "ShadowMatrix3" };
	for (uint32_t u = 0; u < ZENITH_FLUX_NUM_CSMS; u++)
	{
		Flux_ShaderResourceView& xCSMSRV = xShadows.GetCSMSRV(u);
		xBinder.BindSRV(m_xShader, s_aszCSMNames[u], &xCSMSRV, &xGraphics.m_xClampSampler);
		xBinder.BindCBV(m_xShader, s_aszShadowMatrixNames[u], &xShadows.GetShadowMatrixBuffer(u).GetCBV());
	}

	xBinder.BindDrawConstants(m_xShader, "RaymarchConstants", &m_xConstants, sizeof(Flux_RaymarchConstants));

	pxCommandList->DrawIndexed(6);
}
