#include "Zenith.h"

#include "Flux/Fog/Flux_RaymarchFog.h"
#include "Flux/Fog/Flux_VolumeFog.h"

#include "AssetHandling/Zenith_TextureAsset.h"
#include "Flux/Flux.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/HDR/Flux_HDR.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Flux/Shadows/Flux_Shadows.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "TaskSystem/Zenith_TaskSystem.h"

static Zenith_Task g_xRenderTask(ZENITH_PROFILE_INDEX__FLUX_FOG, Flux_RaymarchFog::Render, nullptr);

static Flux_CommandList g_xCommandList("RaymarchFog");

static Flux_Shader s_xShader;
static Flux_Pipeline s_xPipeline;

// Ray march specific parameters
struct Flux_RaymarchConstants
{
	Zenith_Maths::Vector4 m_xFogColour;        // RGB = fog color, A = unused
	Zenith_Maths::Vector4 m_xFogParams;        // x = density, y = scattering, z = absorption, w = max distance
	Zenith_Maths::Vector4 m_xNoiseParams;      // x = scale, y = speed, z = detail, w = time
	Zenith_Maths::Vector4 m_xHeightParams;     // x = base height, y = falloff, z = unused, w = unused
	u_int m_uNumSteps;
	u_int m_uDebugMode;
	u_int m_uFrameIndex;
	float m_fPhaseG;                           // Henyey-Greenstein asymmetry: -1=back, 0=isotropic, 0.6=forward
	float m_fVolShadowBias;                    // Shadow bias for volumetric samples (matches Froxel fog)
	float m_fVolShadowConeRadius;              // Cone spread radius in shadow space (matches Froxel fog)
	float m_fAmbientIrradianceRatio;           // Sky/sun light ratio for ambient fog contribution
	float m_fNoiseWorldScale;                  // World-to-texture coordinate scale for noise sampling
};

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

static Flux_RaymarchConstants s_xConstants;

// Cached binding handles from shader reflection
static Flux_BindingHandle s_xFrameConstantsBinding;
static Flux_BindingHandle s_xDepthBinding;
static Flux_BindingHandle s_xNoise3DBinding;
static Flux_BindingHandle s_xBlueNoiseBinding;
// CSM shadow bindings for volumetric shadows
static Flux_BindingHandle s_axCSMBindings[ZENITH_FLUX_NUM_CSMS];
static Flux_BindingHandle s_axShadowMatrixBindings[ZENITH_FLUX_NUM_CSMS];

void Flux_RaymarchFog::Initialise()
{
	s_xShader.Initialise("Flux_Fullscreen_UV.vert", "Fog/Flux_RaymarchFog.frag");

	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_NONE;

	Flux_PipelineSpecification xPipelineSpec;
	xPipelineSpec.m_pxTargetSetup = &Flux_HDR::GetHDRSceneTargetSetup();
	xPipelineSpec.m_pxShader = &s_xShader;
	xPipelineSpec.m_xVertexInputDesc = xVertexDesc;

	s_xShader.GetReflection().PopulateLayout(xPipelineSpec.m_xPipelineLayout);

	xPipelineSpec.m_bDepthTestEnabled = false;
	xPipelineSpec.m_bDepthWriteEnabled = false;

	// Alpha blending for fog overlay
	xPipelineSpec.m_axBlendStates[0].m_bBlendEnabled = true;
	xPipelineSpec.m_axBlendStates[0].m_eSrcBlendFactor = BLEND_FACTOR_SRCALPHA;
	xPipelineSpec.m_axBlendStates[0].m_eDstBlendFactor = BLEND_FACTOR_ONEMINUSSRCALPHA;

	Flux_PipelineBuilder::FromSpecification(s_xPipeline, xPipelineSpec);

	// Cache binding handles from shader reflection
	const Flux_ShaderReflection& xReflection = s_xShader.GetReflection();
	s_xFrameConstantsBinding = xReflection.GetBinding("FrameConstants");
	s_xDepthBinding = xReflection.GetBinding("u_xDepthTexture");
	s_xNoise3DBinding = xReflection.GetBinding("u_xNoiseTexture3D");
	s_xBlueNoiseBinding = xReflection.GetBinding("u_xBlueNoiseTexture");

	// Cache CSM shadow bindings for volumetric shadows
	s_axCSMBindings[0] = xReflection.GetBinding("u_xCSM0");
	s_axCSMBindings[1] = xReflection.GetBinding("u_xCSM1");
	s_axCSMBindings[2] = xReflection.GetBinding("u_xCSM2");
	s_axCSMBindings[3] = xReflection.GetBinding("u_xCSM3");
	s_axShadowMatrixBindings[0] = xReflection.GetBinding("ShadowMatrix0");
	s_axShadowMatrixBindings[1] = xReflection.GetBinding("ShadowMatrix1");
	s_axShadowMatrixBindings[2] = xReflection.GetBinding("ShadowMatrix2");
	s_axShadowMatrixBindings[3] = xReflection.GetBinding("ShadowMatrix3");

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddUInt32({ "Render", "Volumetric Fog", "Raymarch", "Step Count" }, dbg_uRaymarchSteps, 8, 256);
	Zenith_DebugVariables::AddFloat({ "Render", "Volumetric Fog", "Raymarch", "Noise Scale" }, dbg_fRaymarchNoiseScale, 0.001f, 0.1f);
	Zenith_DebugVariables::AddFloat({ "Render", "Volumetric Fog", "Raymarch", "Noise Speed" }, dbg_fRaymarchNoiseSpeed, 0.0f, 1.0f);
	Zenith_DebugVariables::AddFloat({ "Render", "Volumetric Fog", "Raymarch", "Max Distance" }, dbg_fRaymarchMaxDistance, 50.0f, 1000.0f);
	Zenith_DebugVariables::AddFloat({ "Render", "Volumetric Fog", "Raymarch", "Height Falloff" }, dbg_fRaymarchHeightFalloff, 0.0f, 0.1f);
	Zenith_DebugVariables::AddFloat({ "Render", "Volumetric Fog", "Raymarch", "Phase G" }, dbg_fRaymarchPhaseG, -0.9f, 0.9f);
	// Volumetric shadow parameters (unified with Froxel fog)
	Zenith_DebugVariables::AddFloat({ "Render", "Volumetric Fog", "Raymarch", "Shadow Bias" }, dbg_fRaymarchShadowBias, 0.0001f, 0.01f);
	Zenith_DebugVariables::AddFloat({ "Render", "Volumetric Fog", "Raymarch", "Shadow Cone Radius" }, dbg_fRaymarchShadowConeRadius, 0.0001f, 0.01f);
#endif

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_RaymarchFog initialised");
}

void Flux_RaymarchFog::Reset()
{
	g_xCommandList.Reset(true);
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_RaymarchFog::Reset()");
}

void Flux_RaymarchFog::SubmitRenderTask()
{
	Zenith_TaskSystem::SubmitTask(&g_xRenderTask);
}

void Flux_RaymarchFog::WaitForRenderTask()
{
	g_xRenderTask.WaitUntilComplete();
}

void Flux_RaymarchFog::Render(void*)
{
	// Get shared fog parameters
	Flux_VolumeFogConstants& xShared = Flux_VolumeFog::GetSharedConstants();

	// Update constants
	s_xConstants.m_xFogColour = xShared.m_xFogColour;
	s_xConstants.m_xFogParams = Zenith_Maths::Vector4(
		xShared.m_fDensity,
		xShared.m_fScatteringCoeff,
		xShared.m_fAbsorptionCoeff,
		dbg_fRaymarchMaxDistance
	);

	// Get elapsed time for noise animation using actual frame delta time
	// Wrap time to prevent float precision loss after extended runtime
	static float s_fTime = 0.0f;
	float fDeltaTime = Zenith_Core::GetDt();  // Actual frame delta, frame-rate independent
	s_fTime += fDeltaTime * dbg_fRaymarchNoiseSpeed;
	s_fTime = std::fmod(s_fTime, 1000.0f);  // Wrap every 1000 seconds to maintain precision

	s_xConstants.m_xNoiseParams = Zenith_Maths::Vector4(
		dbg_fRaymarchNoiseScale,
		dbg_fRaymarchNoiseSpeed,
		1.0f,  // detail
		s_fTime
	);

	s_xConstants.m_xHeightParams = Zenith_Maths::Vector4(
		0.0f,  // base height
		dbg_fRaymarchHeightFalloff,
		0.0f,
		0.0f
	);

	s_xConstants.m_uNumSteps = dbg_uRaymarchSteps;

	// Check current debug mode
	extern u_int dbg_uVolFogDebugMode;
	s_xConstants.m_uDebugMode = dbg_uVolFogDebugMode;
	s_xConstants.m_uFrameIndex = Flux::GetFrameCounter();
	s_xConstants.m_fPhaseG = dbg_fRaymarchPhaseG;

	// Volumetric shadow parameters (unified with Froxel fog for consistent shadow softness)
	s_xConstants.m_fVolShadowBias = dbg_fRaymarchShadowBias;
	s_xConstants.m_fVolShadowConeRadius = dbg_fRaymarchShadowConeRadius;
	s_xConstants.m_fAmbientIrradianceRatio = xShared.m_fAmbientIrradianceRatio;
	s_xConstants.m_fNoiseWorldScale = xShared.m_fNoiseWorldScale;

	g_xCommandList.Reset(false);

	g_xCommandList.AddCommand<Flux_CommandSetPipeline>(&s_xPipeline);

	g_xCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	g_xCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(g_xCommandList);
	xBinder.BindCBV(s_xFrameConstantsBinding, &Flux_Graphics::s_xFrameConstantsBuffer.GetCBV());
	xBinder.BindSRV(s_xDepthBinding, Flux_Graphics::GetDepthStencilSRV());
	xBinder.BindSRV(s_xNoise3DBinding, &Flux_VolumeFog::GetNoiseTexture3D()->m_xSRV);
	xBinder.BindSRV(s_xBlueNoiseBinding, &Flux_VolumeFog::GetBlueNoiseTexture()->m_xSRV);

	// Bind CSM shadow maps and matrices for volumetric shadows
	for (uint32_t u = 0; u < ZENITH_FLUX_NUM_CSMS; u++)
	{
		Flux_ShaderResourceView& xCSMSRV = Flux_Shadows::GetCSMSRV(u);
		xBinder.BindSRV(s_axCSMBindings[u], &xCSMSRV, &Flux_Graphics::s_xClampSampler);
		xBinder.BindCBV(s_axShadowMatrixBindings[u], &Flux_Shadows::GetShadowMatrixBuffer(u).GetCBV());
	}

	xBinder.PushConstant(&s_xConstants, sizeof(Flux_RaymarchConstants));

	g_xCommandList.AddCommand<Flux_CommandDrawIndexed>(6);

	Flux::SubmitCommandList(&g_xCommandList, Flux_HDR::GetHDRSceneTargetSetup(), RENDER_ORDER_FOG);
}
