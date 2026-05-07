#include "Zenith.h"

#include "Flux/Fog/Flux_RaymarchFog.h"
#include "Flux/Fog/Flux_VolumeFog.h"

#include "AssetHandling/Zenith_TextureAsset.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/HDR/Flux_HDR.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Flux/Shadows/Flux_Shadows.h"
#include "DebugVariables/Zenith_DebugVariables.h"

#ifdef ZENITH_TOOLS
#include "Flux/Slang/Flux_ShaderHotReload.h"
#endif

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

void Flux_RaymarchFog::BuildPipelines()
{
	s_xShader.Initialise(FluxShaderProgram::Fog_Raymarch);

	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_NONE;

	Flux_PipelineSpecification xPipelineSpec;
	xPipelineSpec.m_aeColourAttachmentFormats[0] = HDR_SCENE_FORMAT;
	xPipelineSpec.m_uNumColourAttachments = 1;
	xPipelineSpec.m_eDepthStencilFormat = DEPTH_FORMAT;
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
}

void Flux_RaymarchFog::Initialise()
{
	BuildPipelines();

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

#ifdef ZENITH_TOOLS
	static const FluxShaderProgram s_axPrograms[] = {
		FluxShaderProgram::Fog_Raymarch,
	};
	Flux_ShaderHotReload::RegisterSubsystem(&Flux_RaymarchFog::BuildPipelines,
		s_axPrograms, sizeof(s_axPrograms) / sizeof(s_axPrograms[0]));
#endif

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_RaymarchFog initialised");
}

void Flux_RaymarchFog::Reset()
{
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_RaymarchFog::Reset()");
}

void Flux_RaymarchFog::Render(Flux_CommandList* pxCommandList)
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

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&s_xPipeline);

	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);
	xBinder.BindCBV(s_xShader, "FrameConstants", &Flux_Graphics::s_xFrameConstantsBuffer.GetCBV());
	xBinder.BindSRV(s_xShader, "u_xDepthTexture", Flux_Graphics::GetDepthStencilSRV());
	xBinder.BindSRV(s_xShader, "u_xNoiseTexture3D", &Flux_VolumeFog::GetNoiseTexture3D()->m_xSRV);
	xBinder.BindSRV(s_xShader, "u_xBlueNoiseTexture", &Flux_VolumeFog::GetBlueNoiseTexture()->m_xSRV);

	// Bind CSM shadow maps and matrices for volumetric shadows
	static const char* const s_aszCSMNames[ZENITH_FLUX_NUM_CSMS] = { "u_xCSM0", "u_xCSM1", "u_xCSM2", "u_xCSM3" };
	static const char* const s_aszShadowMatrixNames[ZENITH_FLUX_NUM_CSMS] = { "ShadowMatrix0", "ShadowMatrix1", "ShadowMatrix2", "ShadowMatrix3" };
	for (uint32_t u = 0; u < ZENITH_FLUX_NUM_CSMS; u++)
	{
		Flux_ShaderResourceView& xCSMSRV = Flux_Shadows::GetCSMSRV(u);
		xBinder.BindSRV(s_xShader, s_aszCSMNames[u], &xCSMSRV, &Flux_Graphics::s_xClampSampler);
		xBinder.BindCBV(s_xShader, s_aszShadowMatrixNames[u], &Flux_Shadows::GetShadowMatrixBuffer(u).GetCBV());
	}

	xBinder.BindDrawConstants(s_xShader, "RaymarchConstants", &s_xConstants, sizeof(Flux_RaymarchConstants));

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
}
