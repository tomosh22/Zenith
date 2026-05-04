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
#include "Core/Zenith_GraphicsOptions.h"

#ifdef ZENITH_TOOLS
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Flux/Slang/Flux_ShaderHotReload.h"
#endif


// Ref-counted copy of Flux_Graphics::s_xCubemapTexture (set during init in Zenith_Main).
// Owned by handle so the cubemap survives any UnloadUnused calls during the frame.
static TextureHandle s_xCubemapTexture;

// Static member definitions
Flux_RenderAttachment Flux_Skybox::s_xTransmittanceLUT;
bool Flux_Skybox::s_bLUTNeedsUpdate = true;

Flux_Pipeline Flux_Skybox::s_xCubemapPipeline;
Flux_Pipeline Flux_Skybox::s_xAtmospherePipeline;
Flux_Pipeline Flux_Skybox::s_xAerialPerspectivePipeline;

Flux_Shader Flux_Skybox::s_xCubemapShader;
Flux_Shader Flux_Skybox::s_xAtmosphereShader;
Flux_Shader Flux_Skybox::s_xAerialPerspectiveShader;

float Flux_Skybox::s_fSunIntensity = AtmosphereConfig::fSUN_INTENSITY;
float Flux_Skybox::s_fRayleighScale = 1.0f;
float Flux_Skybox::s_fMieScale = 1.0f;
float Flux_Skybox::s_fMieG = AtmosphereConfig::fMIE_G;
float Flux_Skybox::s_fAerialPerspectiveStrength = 1.0f;

Flux_DynamicConstantBuffer Flux_Skybox::s_xAtmosphereConstantsBuffer;

Flux_Pipeline Flux_Skybox::s_xSolidColourPipeline;
Flux_Shader Flux_Skybox::s_xSolidColourShader;
Flux_DynamicConstantBuffer Flux_Skybox::s_xSolidColourConstantsBuffer;

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
u_int dbg_uSkyboxDebugMode = SKYBOX_DEBUG_NONE;
float dbg_fSunIntensity = AtmosphereConfig::fSUN_INTENSITY;
float dbg_fRayleighScale = 1.0f;
float dbg_fMieScale = 1.0f;
float dbg_fMieG = AtmosphereConfig::fMIE_G;
float dbg_fAerialStrength = 1.0f;
u_int dbg_uSkySamples = AtmosphereConfig::uDEFAULT_SKY_SAMPLES;
u_int dbg_uLightSamples = AtmosphereConfig::uDEFAULT_LIGHT_SAMPLES;

void Flux_Skybox::BuildPipelines()
{
	// ========== Cubemap skybox pipeline (MRT with no blending) ==========
	{
		Flux_PipelineSpecification xSpec;
		xSpec.m_aeColourAttachmentFormats[MRT_INDEX_DIFFUSE] = MRT_FORMAT_DIFFUSE;
		xSpec.m_aeColourAttachmentFormats[MRT_INDEX_NORMALSAMBIENT] = MRT_FORMAT_NORMALSAMBIENT;
		xSpec.m_aeColourAttachmentFormats[MRT_INDEX_MATERIAL] = MRT_FORMAT_MATERIAL;
		xSpec.m_uNumColourAttachments = MRT_INDEX_COUNT;
		xSpec.m_pxShader = &s_xCubemapShader;
		s_xCubemapShader.Initialise(FluxShaderProgram::SkyboxCubemap);
		s_xCubemapShader.GetReflection().PopulateLayout(xSpec.m_xPipelineLayout);
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
		Flux_PipelineSpecification xSpec;
		xSpec.m_aeColourAttachmentFormats[MRT_INDEX_DIFFUSE] = MRT_FORMAT_DIFFUSE;
		xSpec.m_aeColourAttachmentFormats[MRT_INDEX_NORMALSAMBIENT] = MRT_FORMAT_NORMALSAMBIENT;
		xSpec.m_aeColourAttachmentFormats[MRT_INDEX_MATERIAL] = MRT_FORMAT_MATERIAL;
		xSpec.m_uNumColourAttachments = MRT_INDEX_COUNT;
		xSpec.m_pxShader = &s_xSolidColourShader;
		s_xSolidColourShader.Initialise(FluxShaderProgram::SkyboxSolidColour);
		s_xSolidColourShader.GetReflection().PopulateLayout(xSpec.m_xPipelineLayout);
		for (Flux_BlendState& xBlendState : xSpec.m_axBlendStates)
		{
			xBlendState.m_eSrcBlendFactor = BLEND_FACTOR_ONE;
			xBlendState.m_eDstBlendFactor = BLEND_FACTOR_ZERO;
			xBlendState.m_bBlendEnabled = false;
		}
		Flux_PipelineBuilder::FromSpecification(s_xSolidColourPipeline, xSpec);
	}

	// ========== Atmosphere sky pipeline ==========
	{
		Flux_PipelineSpecification xSpec;
		xSpec.m_aeColourAttachmentFormats[MRT_INDEX_DIFFUSE] = MRT_FORMAT_DIFFUSE;
		xSpec.m_aeColourAttachmentFormats[MRT_INDEX_NORMALSAMBIENT] = MRT_FORMAT_NORMALSAMBIENT;
		xSpec.m_aeColourAttachmentFormats[MRT_INDEX_MATERIAL] = MRT_FORMAT_MATERIAL;
		xSpec.m_uNumColourAttachments = MRT_INDEX_COUNT;
		xSpec.m_pxShader = &s_xAtmosphereShader;
		s_xAtmosphereShader.Initialise(FluxShaderProgram::SkyboxAtmosphere);
		s_xAtmosphereShader.GetReflection().PopulateLayout(xSpec.m_xPipelineLayout);
		Flux_PipelineBuilder::FromSpecification(s_xAtmospherePipeline, xSpec);
	}

	// ========== Aerial perspective pipeline (alpha blending) ==========
	{
		Flux_PipelineSpecification xSpec;
		xSpec.m_aeColourAttachmentFormats[0] = HDR_SCENE_FORMAT;
		xSpec.m_uNumColourAttachments = 1;
		xSpec.m_pxShader = &s_xAerialPerspectiveShader;
		s_xAerialPerspectiveShader.Initialise(FluxShaderProgram::SkyboxAerialPerspective);
		s_xAerialPerspectiveShader.GetReflection().PopulateLayout(xSpec.m_xPipelineLayout);
		xSpec.m_axBlendStates[0].m_bBlendEnabled = true;
		xSpec.m_axBlendStates[0].m_eSrcBlendFactor = BLEND_FACTOR_SRCALPHA;
		xSpec.m_axBlendStates[0].m_eDstBlendFactor = BLEND_FACTOR_ONEMINUSSRCALPHA;
		Flux_PipelineBuilder::FromSpecification(s_xAerialPerspectivePipeline, xSpec);
	}
}

void Flux_Skybox::Initialise()
{
	CreateRenderTargets();

	// Atmosphere & solid-colour CB allocations are one-time — kept in
	// Initialise so hot-reload's BuildPipelines() doesn't leak them.
	Flux_MemoryManager::InitialiseDynamicConstantBuffer(&s_xAtmosphereConstants, sizeof(AtmosphereConstants), s_xAtmosphereConstantsBuffer);
	Flux_MemoryManager::InitialiseDynamicConstantBuffer(&s_xSolidColourConstants, sizeof(SkyboxOverrideConstants), s_xSolidColourConstantsBuffer);

	BuildPipelines();

	// Take a ref-counted copy of the global cubemap handle (set during init in Zenith_Main).
	s_xCubemapTexture = Flux_Graphics::s_xCubemapTexture;

#ifdef ZENITH_TOOLS
	static const FluxShaderProgram s_axPrograms[] = {
		FluxShaderProgram::SkyboxCubemap,
		FluxShaderProgram::SkyboxSolidColour,
		FluxShaderProgram::SkyboxAtmosphere,
		FluxShaderProgram::SkyboxAerialPerspective,
	};
	Flux_ShaderHotReload::RegisterSubsystem(&Flux_Skybox::BuildPipelines,
		s_axPrograms, sizeof(s_axPrograms) / sizeof(s_axPrograms[0]));
#endif

#ifdef ZENITH_DEBUG_VARIABLES
	RegisterDebugVariables();
#endif

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Skybox initialised");
}

void Flux_Skybox::ReleaseAssetReferences()
{
	s_xCubemapTexture.Clear();
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
}

void Flux_Skybox::DestroyRenderTargets()
{
	if (s_xTransmittanceLUT.m_xVRAMHandle.IsValid())
	{
		Flux_VRAM* pxVRAM = Flux_PlatformAPI::GetVRAM(s_xTransmittanceLUT.m_xVRAMHandle);
		Flux_MemoryManager::QueueVRAMDeletion(pxVRAM, s_xTransmittanceLUT.m_xVRAMHandle,
			s_xTransmittanceLUT.RTV().m_xImageViewHandle, s_xTransmittanceLUT.DSV().m_xImageViewHandle,
			s_xTransmittanceLUT.SRV().m_xImageViewHandle, s_xTransmittanceLUT.UAV(0).m_xImageViewHandle);
	}
}


static void PreExecuteSkybox(void*)
{
	const Zenith_GraphicsOptions& xOpts = Zenith_GraphicsOptions::Get();
	// Upload buffer data sequentially before parallel recording
	if (!xOpts.m_bSkyboxEnabled)
	{
		s_xSolidColourConstants.m_xColour = Zenith_Maths::Vector4(xOpts.m_xSkyboxColour, 1.f);
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
	if (!Zenith_GraphicsOptions::Get().m_bSkyboxEnabled)
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
			xBinder.BindCBV(Flux_Skybox::s_xAtmosphereShader, "FrameConstants", &Flux_Graphics::s_xFrameConstantsBuffer.GetCBV());
			xBinder.BindCBV(Flux_Skybox::s_xAtmosphereShader, "AtmosphereConstants", &Flux_Skybox::s_xAtmosphereConstantsBuffer.GetCBV());
		}

		pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
	}
	else
	{
		// Cubemap sky
		Zenith_TextureAsset* pxCubemap = s_xCubemapTexture.GetDirect();
		if (!pxCubemap || !pxCubemap->m_xSRV.m_xImageViewHandle.IsValid())
		{
			return;
		}

		pxCommandList->AddCommand<Flux_CommandSetPipeline>(&Flux_Skybox::s_xCubemapPipeline);
		pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
		pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());
		pxCommandList->AddCommand<Flux_CommandBeginBind>(0);
		pxCommandList->AddCommand<Flux_CommandBindCBV>(&Flux_Graphics::s_xFrameConstantsBuffer.GetCBV(), 0);
		pxCommandList->AddCommand<Flux_CommandBindSRV>(&pxCubemap->m_xSRV, 1);
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
		xBinder.BindCBV(Flux_Skybox::s_xAerialPerspectiveShader, "FrameConstants", &Flux_Graphics::s_xFrameConstantsBuffer.GetCBV());
		xBinder.BindCBV(Flux_Skybox::s_xAerialPerspectiveShader, "AtmosphereConstants", &Flux_Skybox::s_xAtmosphereConstantsBuffer.GetCBV());
		xBinder.BindSRV(Flux_Skybox::s_xAerialPerspectiveShader, "g_xDepthTex", &Flux_Graphics::GetDepthAttachment().SRV());
	}

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
}

void Flux_Skybox::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	// Sky render pass (writes to MRT). Skybox is a fullscreen quad that writes
	// all G-Buffer channels via OutputToGBuffer, so it's the natural place to
	// clear the MRT target (both color — redundantly — and depth, which the
	// subsequent geometry passes need for depth testing).
	xGraph.AddPass("Skybox", ExecuteSkybox)
		.Writes(Flux_Graphics::GetMRTAttachment(MRT_INDEX_DIFFUSE),        RESOURCE_ACCESS_WRITE_RTV)
		.Writes(Flux_Graphics::GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT), RESOURCE_ACCESS_WRITE_RTV)
		.Writes(Flux_Graphics::GetMRTAttachment(MRT_INDEX_MATERIAL),       RESOURCE_ACCESS_WRITE_RTV)
		.Prepare(PreExecuteSkybox)
		.ClearTargets();
}

void Flux_Skybox::SetupAerialPerspectiveRenderGraph(Flux_RenderGraph& xGraph)
{
	// Aerial perspective applies atmospheric scattering via SRC_ALPHA blending
	// on top of existing HDR content. It is registered AFTER DeferredShading
	// (see Flux::SetupRenderGraph) so the writer-chain topological order puts
	// it downstream of the lighting pass — otherwise it would blend into stale
	// last-frame HDR and produce garbage.
	xGraph.AddPass("Aerial Perspective", ExecuteAerialPerspective)
		.Writes(Flux_HDR::GetHDRSceneTarget(),       RESOURCE_ACCESS_WRITE_RTV)
		.Reads (Flux_Graphics::GetDepthAttachment(), RESOURCE_ACCESS_READ_SRV);
}

// Setters (continuous parameters; on/off toggles live in Zenith_GraphicsOptions)
void Flux_Skybox::SetSunIntensity(float fIntensity) { s_fSunIntensity = fIntensity; }
void Flux_Skybox::SetRayleighScale(float fScale) { s_fRayleighScale = fScale; }
void Flux_Skybox::SetMieScale(float fScale) { s_fMieScale = fScale; }
void Flux_Skybox::SetMieG(float fG) { s_fMieG = fG; }
void Flux_Skybox::SetAerialPerspectiveStrength(float fStrength) { s_fAerialPerspectiveStrength = fStrength; }

// Getters
bool Flux_Skybox::IsAtmosphereEnabled() { return Zenith_GraphicsOptions::Get().m_bSkyboxAtmosphereEnabled; }
float Flux_Skybox::GetSunIntensity() { return s_fSunIntensity; }
float Flux_Skybox::GetRayleighScale() { return s_fRayleighScale; }
float Flux_Skybox::GetMieScale() { return s_fMieScale; }
float Flux_Skybox::GetMieG() { return s_fMieG; }
bool Flux_Skybox::IsAerialPerspectiveEnabled() { return Zenith_GraphicsOptions::Get().m_bSkyboxAerialPerspectiveEnabled; }
float Flux_Skybox::GetAerialPerspectiveStrength() { return s_fAerialPerspectiveStrength; }

Flux_ShaderResourceView& Flux_Skybox::GetTransmittanceLUTSRV()
{
	return s_xTransmittanceLUT.SRV();
}

#ifdef ZENITH_DEBUG_VARIABLES
void Flux_Skybox::RegisterDebugVariables()
{
	Zenith_DebugVariables::AddUInt32({ "Flux", "Skybox", "DebugMode" }, dbg_uSkyboxDebugMode, 0, SKYBOX_DEBUG_COUNT - 1);
	Zenith_DebugVariables::AddFloat({ "Flux", "Skybox", "SunIntensity" }, dbg_fSunIntensity, 1.0f, 100.0f);
	Zenith_DebugVariables::AddFloat({ "Flux", "Skybox", "RayleighScale" }, dbg_fRayleighScale, 0.0f, 5.0f);
	Zenith_DebugVariables::AddFloat({ "Flux", "Skybox", "MieScale" }, dbg_fMieScale, 0.0f, 5.0f);
	Zenith_DebugVariables::AddFloat({ "Flux", "Skybox", "MieG" }, dbg_fMieG, 0.0f, 0.99f);
	Zenith_DebugVariables::AddFloat({ "Flux", "Skybox", "AerialStrength" }, dbg_fAerialStrength, 0.0f, 5.0f);
	Zenith_DebugVariables::AddUInt32({ "Flux", "Skybox", "SkySamples" }, dbg_uSkySamples, 4, 64);
	Zenith_DebugVariables::AddUInt32({ "Flux", "Skybox", "LightSamples" }, dbg_uLightSamples, 2, 32);

	Zenith_DebugVariables::AddTexture({ "Flux", "Skybox", "Textures", "TransmittanceLUT" }, s_xTransmittanceLUT.SRV());
}
#endif
