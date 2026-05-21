#include "Zenith.h"

#include "Flux/Skybox/Flux_Skybox.h"
#include "Flux/Skybox/Flux_SkyboxImpl.h"
#include "Core/Zenith_Engine.h"

#include "AssetHandling/Zenith_TextureAsset.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/HDR/Flux_HDR.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Core/Zenith_GraphicsOptions.h"

#ifdef ZENITH_TOOLS
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Flux/Slang/Flux_ShaderHotReload.h"
#endif

// Phase 7g: subsystem state moved to Flux_SkyboxImpl held by Zenith_Engine.

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
		// Attach depth only so the render-pass clear resets scene depth to far.
		// The fullscreen sky draw must not depth-test or write depth.
		xSpec.m_eDepthStencilFormat = DEPTH_FORMAT;
		xSpec.m_bDepthTestEnabled = false;
		xSpec.m_bDepthWriteEnabled = false;
		xSpec.m_pxShader = &g_xEngine.Skybox().m_xCubemapShader;
		g_xEngine.Skybox().m_xCubemapShader.Initialise(FluxShaderProgram::SkyboxCubemap);
		g_xEngine.Skybox().m_xCubemapShader.GetReflection().PopulateLayout(xSpec.m_xPipelineLayout);
		for (Flux_BlendState& xBlendState : xSpec.m_axBlendStates)
		{
			xBlendState.m_eSrcBlendFactor = BLEND_FACTOR_ONE;
			xBlendState.m_eDstBlendFactor = BLEND_FACTOR_ZERO;
			xBlendState.m_bBlendEnabled = false;
		}
		Flux_PipelineBuilder::FromSpecification(g_xEngine.Skybox().m_xCubemapPipeline, xSpec);
	}

	// ========== Solid colour override pipeline (MRT with no blending) ==========
	{
		Flux_PipelineSpecification xSpec;
		xSpec.m_aeColourAttachmentFormats[MRT_INDEX_DIFFUSE] = MRT_FORMAT_DIFFUSE;
		xSpec.m_aeColourAttachmentFormats[MRT_INDEX_NORMALSAMBIENT] = MRT_FORMAT_NORMALSAMBIENT;
		xSpec.m_aeColourAttachmentFormats[MRT_INDEX_MATERIAL] = MRT_FORMAT_MATERIAL;
		xSpec.m_uNumColourAttachments = MRT_INDEX_COUNT;
		// Attach depth only so the render-pass clear resets scene depth to far.
		// The fullscreen sky draw must not depth-test or write depth.
		xSpec.m_eDepthStencilFormat = DEPTH_FORMAT;
		xSpec.m_bDepthTestEnabled = false;
		xSpec.m_bDepthWriteEnabled = false;
		xSpec.m_pxShader = &g_xEngine.Skybox().m_xSolidColourShader;
		g_xEngine.Skybox().m_xSolidColourShader.Initialise(FluxShaderProgram::SkyboxSolidColour);
		g_xEngine.Skybox().m_xSolidColourShader.GetReflection().PopulateLayout(xSpec.m_xPipelineLayout);
		for (Flux_BlendState& xBlendState : xSpec.m_axBlendStates)
		{
			xBlendState.m_eSrcBlendFactor = BLEND_FACTOR_ONE;
			xBlendState.m_eDstBlendFactor = BLEND_FACTOR_ZERO;
			xBlendState.m_bBlendEnabled = false;
		}
		Flux_PipelineBuilder::FromSpecification(g_xEngine.Skybox().m_xSolidColourPipeline, xSpec);
	}

	// ========== Atmosphere sky pipeline ==========
	{
		Flux_PipelineSpecification xSpec;
		xSpec.m_aeColourAttachmentFormats[MRT_INDEX_DIFFUSE] = MRT_FORMAT_DIFFUSE;
		xSpec.m_aeColourAttachmentFormats[MRT_INDEX_NORMALSAMBIENT] = MRT_FORMAT_NORMALSAMBIENT;
		xSpec.m_aeColourAttachmentFormats[MRT_INDEX_MATERIAL] = MRT_FORMAT_MATERIAL;
		xSpec.m_uNumColourAttachments = MRT_INDEX_COUNT;
		// Attach depth only so the render-pass clear resets scene depth to far.
		// The fullscreen sky draw must not depth-test or write depth.
		xSpec.m_eDepthStencilFormat = DEPTH_FORMAT;
		xSpec.m_bDepthTestEnabled = false;
		xSpec.m_bDepthWriteEnabled = false;
		xSpec.m_pxShader = &g_xEngine.Skybox().m_xAtmosphereShader;
		g_xEngine.Skybox().m_xAtmosphereShader.Initialise(FluxShaderProgram::SkyboxAtmosphere);
		g_xEngine.Skybox().m_xAtmosphereShader.GetReflection().PopulateLayout(xSpec.m_xPipelineLayout);
		Flux_PipelineBuilder::FromSpecification(g_xEngine.Skybox().m_xAtmospherePipeline, xSpec);
	}

	// ========== Aerial perspective pipeline (alpha blending) ==========
	{
		Flux_PipelineSpecification xSpec;
		xSpec.m_aeColourAttachmentFormats[0] = HDR_SCENE_FORMAT;
		xSpec.m_uNumColourAttachments = 1;
		xSpec.m_pxShader = &g_xEngine.Skybox().m_xAerialPerspectiveShader;
		g_xEngine.Skybox().m_xAerialPerspectiveShader.Initialise(FluxShaderProgram::SkyboxAerialPerspective);
		g_xEngine.Skybox().m_xAerialPerspectiveShader.GetReflection().PopulateLayout(xSpec.m_xPipelineLayout);
		xSpec.m_axBlendStates[0].m_bBlendEnabled = true;
		xSpec.m_axBlendStates[0].m_eSrcBlendFactor = BLEND_FACTOR_SRCALPHA;
		xSpec.m_axBlendStates[0].m_eDstBlendFactor = BLEND_FACTOR_ONEMINUSSRCALPHA;
		Flux_PipelineBuilder::FromSpecification(g_xEngine.Skybox().m_xAerialPerspectivePipeline, xSpec);
	}
}

void Flux_Skybox::Initialise()
{
	CreateRenderTargets();

	// Atmosphere & solid-colour CB allocations are one-time — kept in
	// Initialise so hot-reload's BuildPipelines() doesn't leak them.
	Flux_MemoryManager::InitialiseDynamicConstantBuffer(&s_xAtmosphereConstants, sizeof(AtmosphereConstants), g_xEngine.Skybox().m_xAtmosphereConstantsBuffer);
	Flux_MemoryManager::InitialiseDynamicConstantBuffer(&s_xSolidColourConstants, sizeof(SkyboxOverrideConstants), g_xEngine.Skybox().m_xSolidColourConstantsBuffer);

	BuildPipelines();

	// Take a ref-counted copy of the global cubemap handle (set during init in Zenith_Main).
	g_xEngine.Skybox().m_xCubemapTexture = g_xEngine.FluxGraphics().m_xCubemapTexture;

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
	g_xEngine.Skybox().m_xCubemapTexture.Clear();
}

void Flux_Skybox::Shutdown()
{
	DestroyRenderTargets();
	Flux_MemoryManager::DestroyDynamicConstantBuffer(g_xEngine.Skybox().m_xAtmosphereConstantsBuffer);
	Flux_MemoryManager::DestroyDynamicConstantBuffer(g_xEngine.Skybox().m_xSolidColourConstantsBuffer);
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Skybox shut down");
}

void Flux_Skybox::Reset()
{
	g_xEngine.Skybox().m_bLUTNeedsUpdate = true;
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

	xBuilder.BuildColour(g_xEngine.Skybox().m_xTransmittanceLUT, "Skybox Transmittance LUT");
}

void Flux_Skybox::DestroyRenderTargets()
{
	if (g_xEngine.Skybox().m_xTransmittanceLUT.m_xVRAMHandle.IsValid())
	{
		Flux_VRAM* pxVRAM = Flux_PlatformAPI::GetVRAM(g_xEngine.Skybox().m_xTransmittanceLUT.m_xVRAMHandle);
		Flux_MemoryManager::QueueVRAMDeletion(pxVRAM, g_xEngine.Skybox().m_xTransmittanceLUT.m_xVRAMHandle,
			g_xEngine.Skybox().m_xTransmittanceLUT.RTV().m_xImageViewHandle, g_xEngine.Skybox().m_xTransmittanceLUT.DSV().m_xImageViewHandle,
			g_xEngine.Skybox().m_xTransmittanceLUT.SRV().m_xImageViewHandle, g_xEngine.Skybox().m_xTransmittanceLUT.UAV(0).m_xImageViewHandle);
	}
}


static void PreExecuteSkybox(void*)
{
	const Zenith_GraphicsOptions& xOpts = Zenith_GraphicsOptions::Get();
	// Upload buffer data sequentially before parallel recording
	if (!xOpts.m_bSkyboxEnabled)
	{
		s_xSolidColourConstants.m_xColour = Zenith_Maths::Vector4(xOpts.m_xSkyboxColour, 1.f);
		Flux_MemoryManager::UploadBufferData(g_xEngine.Skybox().m_xSolidColourConstantsBuffer.GetBuffer().m_xVRAMHandle, &s_xSolidColourConstants, sizeof(SkyboxOverrideConstants));
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

		Flux_MemoryManager::UploadBufferData(g_xEngine.Skybox().m_xAtmosphereConstantsBuffer.GetBuffer().m_xVRAMHandle, &s_xAtmosphereConstants, sizeof(AtmosphereConstants));
	}
}

static void ExecuteSkybox(Flux_CommandList* pxCommandList, void*)
{
	if (!Zenith_GraphicsOptions::Get().m_bSkyboxEnabled)
	{
		// Solid colour override (buffer uploaded in PreExecuteSkybox)

		pxCommandList->AddCommand<Flux_CommandSetPipeline>(&g_xEngine.Skybox().m_xSolidColourPipeline);
		pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetVertexBuffer());
		pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetIndexBuffer());
		pxCommandList->AddCommand<Flux_CommandBeginBind>(0);
		pxCommandList->AddCommand<Flux_CommandBindCBV>(&g_xEngine.Skybox().m_xSolidColourConstantsBuffer.GetCBV(), 0);
		pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
		return;
	}

	if (Flux_Skybox::IsAtmosphereEnabled())
	{
		// Atmosphere sky (constants uploaded in PreExecuteSkybox)
		pxCommandList->AddCommand<Flux_CommandSetPipeline>(&g_xEngine.Skybox().m_xAtmospherePipeline);
		pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetVertexBuffer());
		pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetIndexBuffer());

		{
			Flux_ShaderBinder xBinder(*pxCommandList);
			xBinder.BindCBV(g_xEngine.Skybox().m_xAtmosphereShader, "FrameConstants", &g_xEngine.FluxGraphics().m_xFrameConstantsBuffer.GetCBV());
			xBinder.BindCBV(g_xEngine.Skybox().m_xAtmosphereShader, "AtmosphereConstants", &g_xEngine.Skybox().m_xAtmosphereConstantsBuffer.GetCBV());
		}

		pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
	}
	else
	{
		// Cubemap sky
		Zenith_TextureAsset* pxCubemap = g_xEngine.Skybox().m_xCubemapTexture.GetDirect();
		if (!pxCubemap || !pxCubemap->m_xSRV.m_xImageViewHandle.IsValid())
		{
			return;
		}

		pxCommandList->AddCommand<Flux_CommandSetPipeline>(&g_xEngine.Skybox().m_xCubemapPipeline);
		pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetVertexBuffer());
		pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetIndexBuffer());
		pxCommandList->AddCommand<Flux_CommandBeginBind>(0);
		pxCommandList->AddCommand<Flux_CommandBindCBV>(&g_xEngine.FluxGraphics().m_xFrameConstantsBuffer.GetCBV(), 0);
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

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&g_xEngine.Skybox().m_xAerialPerspectivePipeline);
	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetIndexBuffer());

	{
		Flux_ShaderBinder xBinder(*pxCommandList);
		xBinder.BindCBV(g_xEngine.Skybox().m_xAerialPerspectiveShader, "FrameConstants", &g_xEngine.FluxGraphics().m_xFrameConstantsBuffer.GetCBV());
		xBinder.BindCBV(g_xEngine.Skybox().m_xAerialPerspectiveShader, "AtmosphereConstants", &g_xEngine.Skybox().m_xAtmosphereConstantsBuffer.GetCBV());
		xBinder.BindSRV(g_xEngine.Skybox().m_xAerialPerspectiveShader, "g_xDepthTex", &Flux_Graphics::GetDepthAttachment().SRV());
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
		// Depth is attached purely so ClearTargets() clears it to 1.0; the
		// skybox pipelines disable depth test/write, so the draw does not touch it.
		.Writes(Flux_Graphics::GetDepthAttachment(),                       RESOURCE_ACCESS_WRITE_DSV)
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
void Flux_Skybox::SetSunIntensity(float fIntensity) { g_xEngine.Skybox().m_fSunIntensity = fIntensity; }
void Flux_Skybox::SetRayleighScale(float fScale) { g_xEngine.Skybox().m_fRayleighScale = fScale; }
void Flux_Skybox::SetMieScale(float fScale) { g_xEngine.Skybox().m_fMieScale = fScale; }
void Flux_Skybox::SetMieG(float fG) { g_xEngine.Skybox().m_fMieG = fG; }
void Flux_Skybox::SetAerialPerspectiveStrength(float fStrength) { g_xEngine.Skybox().m_fAerialPerspectiveStrength = fStrength; }

// Getters
bool Flux_Skybox::IsAtmosphereEnabled() { return Zenith_GraphicsOptions::Get().m_bSkyboxAtmosphereEnabled; }
float Flux_Skybox::GetSunIntensity() { return g_xEngine.Skybox().m_fSunIntensity; }
float Flux_Skybox::GetRayleighScale() { return g_xEngine.Skybox().m_fRayleighScale; }
float Flux_Skybox::GetMieScale() { return g_xEngine.Skybox().m_fMieScale; }
float Flux_Skybox::GetMieG() { return g_xEngine.Skybox().m_fMieG; }
bool Flux_Skybox::IsAerialPerspectiveEnabled() { return Zenith_GraphicsOptions::Get().m_bSkyboxAerialPerspectiveEnabled; }
float Flux_Skybox::GetAerialPerspectiveStrength() { return g_xEngine.Skybox().m_fAerialPerspectiveStrength; }

Flux_ShaderResourceView& Flux_Skybox::GetTransmittanceLUTSRV()
{
	return g_xEngine.Skybox().m_xTransmittanceLUT.SRV();
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

	Zenith_DebugVariables::AddTexture({ "Flux", "Skybox", "Textures", "TransmittanceLUT" }, g_xEngine.Skybox().m_xTransmittanceLUT.SRV());
}
#endif
