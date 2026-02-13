#include "Zenith.h"

#include "Flux/Skybox/Flux_Skybox.h"

#include "AssetHandling/Zenith_TextureAsset.h"
#include "Flux/Flux.h"
#include "Flux/Flux_CommandList.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/HDR/Flux_HDR.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "TaskSystem/Zenith_TaskSystem.h"

#ifdef ZENITH_TOOLS
#include "DebugVariables/Zenith_DebugVariables.h"
#endif

// Tasks
static Zenith_Task g_xRenderTask(ZENITH_PROFILE_INDEX__FLUX_SKYBOX, Flux_Skybox::Render, nullptr);
static Zenith_Task g_xAerialPerspectiveTask(ZENITH_PROFILE_INDEX__FLUX_SKYBOX, Flux_Skybox::RenderAerialPerspective, nullptr);

// Command lists
static Flux_CommandList g_xSkyCommandList("Skybox");
static Flux_CommandList g_xAerialCommandList("Skybox_Aerial");

// Cubemap texture reference
static Zenith_TextureAsset* s_pxCubemapTexture = nullptr;

// Static member definitions
Flux_RenderAttachment Flux_Skybox::s_xTransmittanceLUT;
Flux_TargetSetup Flux_Skybox::s_xTransmittanceLUTSetup;
bool Flux_Skybox::s_bLUTNeedsUpdate = true;

Flux_Pipeline Flux_Skybox::s_xCubemapPipeline;
Flux_Pipeline Flux_Skybox::s_xAtmospherePipeline;
Flux_Pipeline Flux_Skybox::s_xAerialPerspectivePipeline;

Flux_Shader Flux_Skybox::s_xCubemapShader;
Flux_Shader Flux_Skybox::s_xAtmosphereShader;
Flux_Shader Flux_Skybox::s_xAerialPerspectiveShader;

bool Flux_Skybox::s_bAtmosphereEnabled = false;
float Flux_Skybox::s_fSunIntensity = AtmosphereConfig::fSUN_INTENSITY;
float Flux_Skybox::s_fRayleighScale = 1.0f;
float Flux_Skybox::s_fMieScale = 1.0f;
float Flux_Skybox::s_fMieG = AtmosphereConfig::fMIE_G;
bool Flux_Skybox::s_bAerialPerspectiveEnabled = true;  // Must match dbg_bAerialEnabled default
float Flux_Skybox::s_fAerialPerspectiveStrength = 1.0f;

Flux_DynamicConstantBuffer Flux_Skybox::s_xAtmosphereConstantsBuffer;

bool Flux_Skybox::s_bEnabled = true;
Zenith_Maths::Vector3 Flux_Skybox::s_xOverrideColour = Zenith_Maths::Vector3(0.f);

Flux_Pipeline Flux_Skybox::s_xSolidColourPipeline;
Flux_Shader Flux_Skybox::s_xSolidColourShader;
Flux_DynamicConstantBuffer Flux_Skybox::s_xSolidColourConstantsBuffer;

// Cached binding handles for cubemap
static Flux_BindingHandle s_xCubemapFrameConstantsBinding;
static Flux_BindingHandle s_xCubemapTextureBinding;

// Cached binding handles for atmosphere
static Flux_BindingHandle s_xAtmosFrameConstantsBinding;
static Flux_BindingHandle s_xAtmosConstantsBinding;

// Cached binding handles for aerial perspective
static Flux_BindingHandle s_xAerialFrameConstantsBinding;
static Flux_BindingHandle s_xAerialAtmosConstantsBinding;
static Flux_BindingHandle s_xAerialDepthTexBinding;

// Solid colour override constants
struct SkyboxOverrideConstants
{
	Zenith_Maths::Vector4 m_xColour;
};
static SkyboxOverrideConstants s_xSolidColourConstants;

// Atmosphere constants buffer structure
struct AtmosphereConstants
{
	// Scattering coefficients
	Zenith_Maths::Vector4 m_xRayleighScatter;      // RGB coefficients, W = scale height
	Zenith_Maths::Vector4 m_xMieScatter;           // RGB = scatter, W = scale height

	// Planet parameters
	float m_fPlanetRadius;
	float m_fAtmosphereRadius;
	float m_fMieG;                                   // Henyey-Greenstein asymmetry
	float m_fSunIntensity;

	// Configuration
	float m_fRayleighScale;
	float m_fMieScale;
	float m_fAerialPerspectiveStrength;
	u_int m_uDebugMode;

	// Ray march settings
	u_int m_uSkySamples;
	u_int m_uLightSamples;
	Zenith_Maths::Vector2 m_xPad;
};
static AtmosphereConstants s_xAtmosphereConstants;

// Debug variables
bool dbg_bAtmosphereEnable = false;
u_int dbg_uSkyboxDebugMode = SKYBOX_DEBUG_NONE;
float dbg_fSunIntensity = AtmosphereConfig::fSUN_INTENSITY;
float dbg_fRayleighScale = 1.0f;
float dbg_fMieScale = 1.0f;
float dbg_fMieG = AtmosphereConfig::fMIE_G;
bool dbg_bAerialEnabled = true;
float dbg_fAerialStrength = 1.0f;
u_int dbg_uSkySamples = AtmosphereConfig::uDEFAULT_SKY_SAMPLES;
u_int dbg_uLightSamples = AtmosphereConfig::uDEFAULT_LIGHT_SAMPLES;

void Flux_Skybox::Initialise()
{
	CreateRenderTargets();

	// Initialize atmosphere constants buffer
	Flux_MemoryManager::InitialiseDynamicConstantBuffer(&s_xAtmosphereConstants, sizeof(AtmosphereConstants), s_xAtmosphereConstantsBuffer);

	// ========== Cubemap skybox pipeline (MRT with no blending) ==========
	{
		Flux_PipelineSpecification xSpec = Flux_PipelineHelper::CreateFullscreenSpec(
			s_xCubemapShader, "Skybox/Flux_Skybox.frag", &Flux_Graphics::s_xMRTTarget);
		for (Flux_BlendState& xBlendState : xSpec.m_axBlendStates)
		{
			xBlendState.m_eSrcBlendFactor = BLEND_FACTOR_ONE;
			xBlendState.m_eDstBlendFactor = BLEND_FACTOR_ZERO;
			xBlendState.m_bBlendEnabled = false;
		}
		Flux_PipelineBuilder::FromSpecification(s_xCubemapPipeline, xSpec);
	}

	// ========== Solid colour override pipeline (MRT with no blending) ==========
	{
		Flux_PipelineSpecification xSpec = Flux_PipelineHelper::CreateFullscreenSpec(
			s_xSolidColourShader, "Skybox/Flux_SkyboxSolidColour.frag", &Flux_Graphics::s_xMRTTarget);
		for (Flux_BlendState& xBlendState : xSpec.m_axBlendStates)
		{
			xBlendState.m_eSrcBlendFactor = BLEND_FACTOR_ONE;
			xBlendState.m_eDstBlendFactor = BLEND_FACTOR_ZERO;
			xBlendState.m_bBlendEnabled = false;
		}
		Flux_PipelineBuilder::FromSpecification(s_xSolidColourPipeline, xSpec);
	}

	// Initialize solid colour constants buffer
	Flux_MemoryManager::InitialiseDynamicConstantBuffer(&s_xSolidColourConstants, sizeof(SkyboxOverrideConstants), s_xSolidColourConstantsBuffer);

	// Use the global cubemap texture pointer set during initialization in Zenith_Main.cpp
	s_pxCubemapTexture = Flux_Graphics::s_pxCubemapTexture;

	// ========== Atmosphere sky pipeline ==========
	Flux_PipelineHelper::BuildFullscreenPipeline(
		s_xAtmosphereShader, s_xAtmospherePipeline,
		"Skybox/Flux_Atmosphere.frag", &Flux_Graphics::s_xMRTTarget);

	{
		const Flux_ShaderReflection& xReflection = s_xAtmosphereShader.GetReflection();
		s_xAtmosFrameConstantsBinding = xReflection.GetBinding("FrameConstants");
		s_xAtmosConstantsBinding = xReflection.GetBinding("AtmosphereConstants");
	}

	// ========== Aerial perspective pipeline (alpha blending) ==========
	{
		Flux_PipelineSpecification xSpec = Flux_PipelineHelper::CreateFullscreenSpec(
			s_xAerialPerspectiveShader, "Skybox/Flux_AerialPerspective.frag", &Flux_HDR::GetHDRSceneTargetSetup());
		xSpec.m_axBlendStates[0].m_bBlendEnabled = true;
		xSpec.m_axBlendStates[0].m_eSrcBlendFactor = BLEND_FACTOR_SRCALPHA;
		xSpec.m_axBlendStates[0].m_eDstBlendFactor = BLEND_FACTOR_ONEMINUSSRCALPHA;
		Flux_PipelineBuilder::FromSpecification(s_xAerialPerspectivePipeline, xSpec);
	}

	{
		const Flux_ShaderReflection& xReflection = s_xAerialPerspectiveShader.GetReflection();
		s_xAerialFrameConstantsBinding = xReflection.GetBinding("FrameConstants");
		s_xAerialAtmosConstantsBinding = xReflection.GetBinding("AtmosphereConstants");
		s_xAerialDepthTexBinding = xReflection.GetBinding("g_xDepthTex");
	}

#ifdef ZENITH_DEBUG_VARIABLES
	RegisterDebugVariables();
#endif

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Skybox initialised");
}

void Flux_Skybox::Shutdown()
{
	DestroyRenderTargets();
	Flux_MemoryManager::DestroyDynamicConstantBuffer(s_xAtmosphereConstantsBuffer);
	Flux_MemoryManager::DestroyDynamicConstantBuffer(s_xSolidColourConstantsBuffer);
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Skybox shut down");
}

void Flux_Skybox::Reset()
{
	g_xSkyCommandList.Reset(true);
	g_xAerialCommandList.Reset(true);
	s_bLUTNeedsUpdate = true;
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Skybox::Reset() - Reset command lists");
}

void Flux_Skybox::CreateRenderTargets()
{
	// Create transmittance LUT for atmosphere
	Flux_RenderAttachmentBuilder xBuilder;
	xBuilder.m_uWidth = AtmosphereConfig::uTRANSMITTANCE_LUT_WIDTH;
	xBuilder.m_uHeight = AtmosphereConfig::uTRANSMITTANCE_LUT_HEIGHT;
	xBuilder.m_uMemoryFlags = 1u << MEMORY_FLAGS__SHADER_READ;
	xBuilder.m_eFormat = TEXTURE_FORMAT_R16G16B16A16_SFLOAT;

	xBuilder.BuildColour(s_xTransmittanceLUT, "Skybox Transmittance LUT");
	s_xTransmittanceLUTSetup.m_axColourAttachments[0] = s_xTransmittanceLUT;
}

void Flux_Skybox::DestroyRenderTargets()
{
	if (s_xTransmittanceLUT.m_xVRAMHandle.IsValid())
	{
		Zenith_Vulkan_VRAM* pxVRAM = Zenith_Vulkan::GetVRAM(s_xTransmittanceLUT.m_xVRAMHandle);
		Flux_MemoryManager::QueueVRAMDeletion(pxVRAM, s_xTransmittanceLUT.m_xVRAMHandle,
			s_xTransmittanceLUT.m_pxRTV.m_xImageViewHandle, s_xTransmittanceLUT.m_pxDSV.m_xImageViewHandle,
			s_xTransmittanceLUT.m_pxSRV.m_xImageViewHandle, s_xTransmittanceLUT.m_pxUAV.m_xImageViewHandle);
	}
}

void Flux_Skybox::SubmitRenderTask()
{
	Zenith_TaskSystem::SubmitTask(&g_xRenderTask);
}

void Flux_Skybox::WaitForRenderTask()
{
	g_xRenderTask.WaitUntilComplete();
}

void Flux_Skybox::SubmitAerialPerspectiveTask()
{
	Zenith_TaskSystem::SubmitTask(&g_xAerialPerspectiveTask);
}

void Flux_Skybox::WaitForAerialPerspectiveTask()
{
	g_xAerialPerspectiveTask.WaitUntilComplete();
}

void Flux_Skybox::Render(void*)
{
	if (!s_bEnabled)
	{
		RenderSolidColour();
		return;
	}

	if (s_bAtmosphereEnabled)
	{
		RenderAtmosphereSky();
	}
	else
	{
		RenderCubemapSky();
	}
}

void Flux_Skybox::RenderCubemapSky()
{
	g_xSkyCommandList.Reset(true);

	// Check if cubemap texture is valid
	if (!s_pxCubemapTexture || !s_pxCubemapTexture->m_xSRV.m_xImageViewHandle.IsValid())
	{
		// Still submit with clear to ensure render targets are cleared even without skybox
		Flux::SubmitCommandList(&g_xSkyCommandList, Flux_Graphics::s_xMRTTarget, RENDER_ORDER_SKYBOX);
		return;
	}

	g_xSkyCommandList.AddCommand<Flux_CommandSetPipeline>(&s_xCubemapPipeline);
	g_xSkyCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	g_xSkyCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());
	g_xSkyCommandList.AddCommand<Flux_CommandBeginBind>(0);
	g_xSkyCommandList.AddCommand<Flux_CommandBindCBV>(&Flux_Graphics::s_xFrameConstantsBuffer.GetCBV(), 0);
	g_xSkyCommandList.AddCommand<Flux_CommandBindSRV>(&s_pxCubemapTexture->m_xSRV, 1);
	g_xSkyCommandList.AddCommand<Flux_CommandDrawIndexed>(6);
	Flux::SubmitCommandList(&g_xSkyCommandList, Flux_Graphics::s_xMRTTarget, RENDER_ORDER_SKYBOX);
}

void Flux_Skybox::RenderSolidColour()
{
	s_xSolidColourConstants.m_xColour = Zenith_Maths::Vector4(s_xOverrideColour, 1.f);
	Flux_MemoryManager::UploadBufferData(s_xSolidColourConstantsBuffer.GetBuffer().m_xVRAMHandle, &s_xSolidColourConstants, sizeof(SkyboxOverrideConstants));

	g_xSkyCommandList.Reset(true);

	g_xSkyCommandList.AddCommand<Flux_CommandSetPipeline>(&s_xSolidColourPipeline);
	g_xSkyCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	g_xSkyCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());
	g_xSkyCommandList.AddCommand<Flux_CommandBeginBind>(0);
	g_xSkyCommandList.AddCommand<Flux_CommandBindCBV>(&s_xSolidColourConstantsBuffer.GetCBV(), 0);
	g_xSkyCommandList.AddCommand<Flux_CommandDrawIndexed>(6);
	Flux::SubmitCommandList(&g_xSkyCommandList, Flux_Graphics::s_xMRTTarget, RENDER_ORDER_SKYBOX);
}

void Flux_Skybox::RenderAtmosphereSky()
{
	// Update atmosphere constants
	s_xAtmosphereConstants.m_xRayleighScatter = Zenith_Maths::Vector4(
		AtmosphereConfig::afRAYLEIGH_SCATTER[0],
		AtmosphereConfig::afRAYLEIGH_SCATTER[1],
		AtmosphereConfig::afRAYLEIGH_SCATTER[2],
		AtmosphereConfig::fRAYLEIGH_SCALE_HEIGHT);

	s_xAtmosphereConstants.m_xMieScatter = Zenith_Maths::Vector4(
		AtmosphereConfig::fMIE_SCATTER,
		AtmosphereConfig::fMIE_SCATTER,
		AtmosphereConfig::fMIE_SCATTER,
		AtmosphereConfig::fMIE_SCALE_HEIGHT);

	s_xAtmosphereConstants.m_fPlanetRadius = AtmosphereConfig::fEARTH_RADIUS;
	s_xAtmosphereConstants.m_fAtmosphereRadius = AtmosphereConfig::fATMOSPHERE_RADIUS;
	s_xAtmosphereConstants.m_fMieG = s_fMieG;
	s_xAtmosphereConstants.m_fSunIntensity = s_fSunIntensity;

	s_xAtmosphereConstants.m_fRayleighScale = s_fRayleighScale;
	s_xAtmosphereConstants.m_fMieScale = s_fMieScale;
	s_xAtmosphereConstants.m_fAerialPerspectiveStrength = s_fAerialPerspectiveStrength;
	s_xAtmosphereConstants.m_uDebugMode = dbg_uSkyboxDebugMode;

	s_xAtmosphereConstants.m_uSkySamples = dbg_uSkySamples;
	s_xAtmosphereConstants.m_uLightSamples = dbg_uLightSamples;
	s_xAtmosphereConstants.m_xPad = Zenith_Maths::Vector2(0.0f);

	Flux_MemoryManager::UploadBufferData(s_xAtmosphereConstantsBuffer.GetBuffer().m_xVRAMHandle, &s_xAtmosphereConstants, sizeof(AtmosphereConstants));

	// Clear=true because skybox is first to render to MRT
	g_xSkyCommandList.Reset(true);

	g_xSkyCommandList.AddCommand<Flux_CommandSetPipeline>(&s_xAtmospherePipeline);
	g_xSkyCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	g_xSkyCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

	{
		Flux_ShaderBinder xBinder(g_xSkyCommandList);
		xBinder.BindCBV(s_xAtmosFrameConstantsBinding, &Flux_Graphics::s_xFrameConstantsBuffer.GetCBV());
		xBinder.BindCBV(s_xAtmosConstantsBinding, &s_xAtmosphereConstantsBuffer.GetCBV());
	}

	g_xSkyCommandList.AddCommand<Flux_CommandDrawIndexed>(6);

	Flux::SubmitCommandList(&g_xSkyCommandList, Flux_Graphics::s_xMRTTarget, RENDER_ORDER_SKYBOX);
}

void Flux_Skybox::RenderAerialPerspective(void*)
{
	if (!s_bAtmosphereEnabled || !s_bAerialPerspectiveEnabled)
	{
		return;
	}

	g_xAerialCommandList.Reset(false);

	g_xAerialCommandList.AddCommand<Flux_CommandSetPipeline>(&s_xAerialPerspectivePipeline);
	g_xAerialCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	g_xAerialCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

	{
		Flux_ShaderBinder xBinder(g_xAerialCommandList);
		xBinder.BindCBV(s_xAerialFrameConstantsBinding, &Flux_Graphics::s_xFrameConstantsBuffer.GetCBV());
		xBinder.BindCBV(s_xAerialAtmosConstantsBinding, &s_xAtmosphereConstantsBuffer.GetCBV());
		xBinder.BindSRV(s_xAerialDepthTexBinding, &Flux_Graphics::s_xDepthBuffer.m_pxSRV);
	}

	g_xAerialCommandList.AddCommand<Flux_CommandDrawIndexed>(6);

	Flux::SubmitCommandList(&g_xAerialCommandList, Flux_HDR::GetHDRSceneTargetSetup(), RENDER_ORDER_AERIAL_PERSPECTIVE);
}

// Setters
void Flux_Skybox::SetAtmosphereEnabled(bool bEnabled) { s_bAtmosphereEnabled = bEnabled; }
void Flux_Skybox::SetSunIntensity(float fIntensity) { s_fSunIntensity = fIntensity; }
void Flux_Skybox::SetRayleighScale(float fScale) { s_fRayleighScale = fScale; }
void Flux_Skybox::SetMieScale(float fScale) { s_fMieScale = fScale; }
void Flux_Skybox::SetMieG(float fG) { s_fMieG = fG; }
void Flux_Skybox::SetAerialPerspectiveEnabled(bool bEnabled) { s_bAerialPerspectiveEnabled = bEnabled; }
void Flux_Skybox::SetAerialPerspectiveStrength(float fStrength) { s_fAerialPerspectiveStrength = fStrength; }

// Getters
bool Flux_Skybox::IsAtmosphereEnabled() { return s_bAtmosphereEnabled; }
float Flux_Skybox::GetSunIntensity() { return s_fSunIntensity; }
float Flux_Skybox::GetRayleighScale() { return s_fRayleighScale; }
float Flux_Skybox::GetMieScale() { return s_fMieScale; }
float Flux_Skybox::GetMieG() { return s_fMieG; }
bool Flux_Skybox::IsAerialPerspectiveEnabled() { return s_bAerialPerspectiveEnabled; }
float Flux_Skybox::GetAerialPerspectiveStrength() { return s_fAerialPerspectiveStrength; }

Flux_ShaderResourceView& Flux_Skybox::GetTransmittanceLUTSRV()
{
	return s_xTransmittanceLUT.m_pxSRV;
}

#ifdef ZENITH_DEBUG_VARIABLES
void Flux_Skybox::RegisterDebugVariables()
{
	Zenith_DebugVariables::AddBoolean({ "Flux", "Skybox", "Atmosphere Enable" }, dbg_bAtmosphereEnable);
	Zenith_DebugVariables::AddUInt32({ "Flux", "Skybox", "DebugMode" }, dbg_uSkyboxDebugMode, 0, SKYBOX_DEBUG_COUNT - 1);
	Zenith_DebugVariables::AddFloat({ "Flux", "Skybox", "SunIntensity" }, dbg_fSunIntensity, 1.0f, 100.0f);
	Zenith_DebugVariables::AddFloat({ "Flux", "Skybox", "RayleighScale" }, dbg_fRayleighScale, 0.0f, 5.0f);
	Zenith_DebugVariables::AddFloat({ "Flux", "Skybox", "MieScale" }, dbg_fMieScale, 0.0f, 5.0f);
	Zenith_DebugVariables::AddFloat({ "Flux", "Skybox", "MieG" }, dbg_fMieG, 0.0f, 0.99f);
	Zenith_DebugVariables::AddBoolean({ "Flux", "Skybox", "AerialPerspective" }, dbg_bAerialEnabled);
	Zenith_DebugVariables::AddFloat({ "Flux", "Skybox", "AerialStrength" }, dbg_fAerialStrength, 0.0f, 5.0f);
	Zenith_DebugVariables::AddUInt32({ "Flux", "Skybox", "SkySamples" }, dbg_uSkySamples, 4, 64);
	Zenith_DebugVariables::AddUInt32({ "Flux", "Skybox", "LightSamples" }, dbg_uLightSamples, 2, 32);

	Zenith_DebugVariables::AddTexture({ "Flux", "Skybox", "Textures", "TransmittanceLUT" }, s_xTransmittanceLUT.m_pxSRV);
}
#endif
