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

#ifdef ZENITH_TOOLS
#include "DebugVariables/Zenith_DebugVariables.h"
#endif


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
	s_bLUTNeedsUpdate = true;
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Skybox::Reset()");
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


static void PreExecuteSkybox(void*)
{
	// Upload buffer data sequentially before parallel recording
	if (!Flux_Skybox::s_bEnabled)
	{
		s_xSolidColourConstants.m_xColour = Zenith_Maths::Vector4(Flux_Skybox::s_xOverrideColour, 1.f);
		Flux_MemoryManager::UploadBufferData(Flux_Skybox::s_xSolidColourConstantsBuffer.GetBuffer().m_xVRAMHandle, &s_xSolidColourConstants, sizeof(SkyboxOverrideConstants));
	}
	else if (Flux_Skybox::IsAtmosphereEnabled())
	{
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
		s_xAtmosphereConstants.m_fMieG = Flux_Skybox::GetMieG();
		s_xAtmosphereConstants.m_fSunIntensity = Flux_Skybox::GetSunIntensity();

		s_xAtmosphereConstants.m_fRayleighScale = Flux_Skybox::GetRayleighScale();
		s_xAtmosphereConstants.m_fMieScale = Flux_Skybox::GetMieScale();
		s_xAtmosphereConstants.m_fAerialPerspectiveStrength = Flux_Skybox::GetAerialPerspectiveStrength();
		s_xAtmosphereConstants.m_uDebugMode = dbg_uSkyboxDebugMode;

		s_xAtmosphereConstants.m_uSkySamples = dbg_uSkySamples;
		s_xAtmosphereConstants.m_uLightSamples = dbg_uLightSamples;
		s_xAtmosphereConstants.m_xPad = Zenith_Maths::Vector2(0.0f);

		Flux_MemoryManager::UploadBufferData(Flux_Skybox::s_xAtmosphereConstantsBuffer.GetBuffer().m_xVRAMHandle, &s_xAtmosphereConstants, sizeof(AtmosphereConstants));
	}
}

static void ExecuteSkybox(Flux_CommandList* pxCommandList, void*)
{
	if (!Flux_Skybox::s_bEnabled)
	{
		// Solid colour override (buffer uploaded in PreExecuteSkybox)

		pxCommandList->AddCommand<Flux_CommandSetPipeline>(&Flux_Skybox::s_xSolidColourPipeline);
		pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
		pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());
		pxCommandList->AddCommand<Flux_CommandBeginBind>(0);
		pxCommandList->AddCommand<Flux_CommandBindCBV>(&Flux_Skybox::s_xSolidColourConstantsBuffer.GetCBV(), 0);
		pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
		return;
	}

	if (Flux_Skybox::IsAtmosphereEnabled())
	{
		// Atmosphere sky (constants uploaded in PreExecuteSkybox)
		pxCommandList->AddCommand<Flux_CommandSetPipeline>(&Flux_Skybox::s_xAtmospherePipeline);
		pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
		pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

		{
			Flux_ShaderBinder xBinder(*pxCommandList);
			xBinder.BindCBV(s_xAtmosFrameConstantsBinding, &Flux_Graphics::s_xFrameConstantsBuffer.GetCBV());
			xBinder.BindCBV(s_xAtmosConstantsBinding, &Flux_Skybox::s_xAtmosphereConstantsBuffer.GetCBV());
		}

		pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
	}
	else
	{
		// Cubemap sky
		if (!s_pxCubemapTexture || !s_pxCubemapTexture->m_xSRV.m_xImageViewHandle.IsValid())
		{
			return;
		}

		pxCommandList->AddCommand<Flux_CommandSetPipeline>(&Flux_Skybox::s_xCubemapPipeline);
		pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
		pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());
		pxCommandList->AddCommand<Flux_CommandBeginBind>(0);
		pxCommandList->AddCommand<Flux_CommandBindCBV>(&Flux_Graphics::s_xFrameConstantsBuffer.GetCBV(), 0);
		pxCommandList->AddCommand<Flux_CommandBindSRV>(&s_pxCubemapTexture->m_xSRV, 1);
		pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
	}
}

static void ExecuteAerialPerspective(Flux_CommandList* pxCommandList, void*)
{
	if (!Flux_Skybox::IsAtmosphereEnabled() || !Flux_Skybox::IsAerialPerspectiveEnabled())
	{
		return;
	}

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&Flux_Skybox::s_xAerialPerspectivePipeline);
	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

	{
		Flux_ShaderBinder xBinder(*pxCommandList);
		xBinder.BindCBV(s_xAerialFrameConstantsBinding, &Flux_Graphics::s_xFrameConstantsBuffer.GetCBV());
		xBinder.BindCBV(s_xAerialAtmosConstantsBinding, &Flux_Skybox::s_xAtmosphereConstantsBuffer.GetCBV());
		xBinder.BindSRV(s_xAerialDepthTexBinding, &Flux_Graphics::s_xDepthBuffer.m_pxSRV);
	}

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
}

void Flux_Skybox::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	// Sky render pass (writes to MRT). Skybox is a fullscreen quad that writes
	// all G-Buffer channels via OutputToGBuffer, so it's the natural place to
	// clear the MRT target (both color — redundantly — and depth, which the
	// subsequent geometry passes need for depth testing).
	u_int uSkyPassIndex = xGraph.AddPass("Skybox", ExecuteSkybox);
	xGraph.SetTargetSetup(uSkyPassIndex, Flux_Graphics::s_xMRTTarget);
	xGraph.SetPrepare(uSkyPassIndex, PreExecuteSkybox);
	xGraph.SetClear(uSkyPassIndex, true);

	// Writes: MRT colour attachments
	for (u_int u = 0; u < MRT_INDEX_COUNT; u++)
	{
		xGraph.Write(uSkyPassIndex, Flux_Graphics::s_xMRTTarget.m_axColourAttachments[u], RESOURCE_ACCESS_WRITE_RTV);
	}
	// Skybox owns the depth clear and the pipeline uses default depth-test+write
	// enabled — declare the depth as a write so the graph emits the transition
	// to ATTACHMENT_OPTIMAL before the renderpass begins.
	xGraph.Write(uSkyPassIndex, Flux_Graphics::s_xDepthBuffer, RESOURCE_ACCESS_WRITE_DSV);
}

void Flux_Skybox::SetupAerialPerspectiveRenderGraph(Flux_RenderGraph& xGraph)
{
	// Aerial perspective applies atmospheric scattering via SRC_ALPHA blending
	// on top of existing HDR content. It is registered AFTER DeferredShading
	// (see Flux::SetupRenderGraph) so the writer-chain topological order puts
	// it downstream of the lighting pass — otherwise it would blend into stale
	// last-frame HDR and produce garbage.
	u_int uAerialPassIndex = xGraph.AddPass("Aerial Perspective", ExecuteAerialPerspective);
	xGraph.SetTargetSetup(uAerialPassIndex, Flux_HDR::GetHDRSceneTargetSetup());

	// Reads: depth buffer
	xGraph.Read(uAerialPassIndex, Flux_Graphics::s_xDepthBuffer, RESOURCE_ACCESS_READ_SRV);

	// Writes: HDR scene target (blends into existing contents, does NOT clear)
	xGraph.Write(uAerialPassIndex, Flux_HDR::GetHDRSceneTarget(), RESOURCE_ACCESS_WRITE_RTV);
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
