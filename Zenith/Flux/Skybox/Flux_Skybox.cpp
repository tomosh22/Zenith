#include "Zenith.h"

#include "Flux/Skybox/Flux_SkyboxImpl.h"
#include "Flux/Skybox/Flux_SkyboxImpl.h"
#include "Core/Zenith_Engine.h"

#include "AssetHandling/Zenith_TextureAsset.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/HDR/Flux_HDRImpl.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Core/Zenith_GraphicsOptions.h"

#ifdef ZENITH_TOOLS
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Flux/Slang/Flux_ShaderHotReload.h"
#endif

// Phase 7g: subsystem state moved to Flux_SkyboxImpl held by Zenith_Engine.
// AtmosphereConstants / SkyboxOverrideConstants CB structs + their scratch
// instances now live on Flux_SkyboxImpl (m_xAtmosphereConstants /
// m_xSolidColourConstants); see Flux_SkyboxImpl.h.

// Debug variables
u_int dbg_uSkyboxDebugMode = SKYBOX_DEBUG_NONE;
float dbg_fSunIntensity = AtmosphereConfig::fSUN_INTENSITY;
float dbg_fRayleighScale = 1.0f;
float dbg_fMieScale = 1.0f;
float dbg_fMieG = AtmosphereConfig::fMIE_G;
float dbg_fAerialStrength = 1.0f;
u_int dbg_uSkySamples = AtmosphereConfig::uDEFAULT_SKY_SAMPLES;
u_int dbg_uLightSamples = AtmosphereConfig::uDEFAULT_LIGHT_SAMPLES;

void Flux_SkyboxImpl::BuildPipelines()
{
	// ========== Cubemap skybox pipeline (MRT with no blending) ==========
	{
		Flux_PipelineSpecification xSpec;
		xSpec.m_aeColourAttachmentFormats[MRT_INDEX_DIFFUSE] = MRT_FORMAT_DIFFUSE;
		xSpec.m_aeColourAttachmentFormats[MRT_INDEX_NORMALSAMBIENT] = MRT_FORMAT_NORMALSAMBIENT;
		xSpec.m_aeColourAttachmentFormats[MRT_INDEX_MATERIAL] = MRT_FORMAT_MATERIAL;
		xSpec.m_aeColourAttachmentFormats[MRT_INDEX_EMISSIVE] = MRT_FORMAT_EMISSIVE;
		xSpec.m_uNumColourAttachments = MRT_INDEX_COUNT;
		// Attach depth only so the render-pass clear resets scene depth to far.
		// The fullscreen sky draw must not depth-test or write depth.
		xSpec.m_eDepthStencilFormat = DEPTH_FORMAT;
		xSpec.m_bDepthTestEnabled = false;
		xSpec.m_bDepthWriteEnabled = false;
		xSpec.m_pxShader = &this->m_xCubemapShader;
		this->m_xCubemapShader.Initialise(FluxShaderProgram::SkyboxCubemap);
		this->m_xCubemapShader.GetReflection().PopulateLayout(xSpec.m_xPipelineLayout);
		for (Flux_BlendState& xBlendState : xSpec.m_axBlendStates)
		{
			xBlendState.m_eSrcBlendFactor = BLEND_FACTOR_ONE;
			xBlendState.m_eDstBlendFactor = BLEND_FACTOR_ZERO;
			xBlendState.m_bBlendEnabled = false;
		}
		Flux_PipelineBuilder::FromSpecification(this->m_xCubemapPipeline, xSpec);
	}

	// ========== Solid colour override pipeline (MRT with no blending) ==========
	{
		Flux_PipelineSpecification xSpec;
		xSpec.m_aeColourAttachmentFormats[MRT_INDEX_DIFFUSE] = MRT_FORMAT_DIFFUSE;
		xSpec.m_aeColourAttachmentFormats[MRT_INDEX_NORMALSAMBIENT] = MRT_FORMAT_NORMALSAMBIENT;
		xSpec.m_aeColourAttachmentFormats[MRT_INDEX_MATERIAL] = MRT_FORMAT_MATERIAL;
		xSpec.m_aeColourAttachmentFormats[MRT_INDEX_EMISSIVE] = MRT_FORMAT_EMISSIVE;
		xSpec.m_uNumColourAttachments = MRT_INDEX_COUNT;
		// Attach depth only so the render-pass clear resets scene depth to far.
		// The fullscreen sky draw must not depth-test or write depth.
		xSpec.m_eDepthStencilFormat = DEPTH_FORMAT;
		xSpec.m_bDepthTestEnabled = false;
		xSpec.m_bDepthWriteEnabled = false;
		xSpec.m_pxShader = &this->m_xSolidColourShader;
		this->m_xSolidColourShader.Initialise(FluxShaderProgram::SkyboxSolidColour);
		this->m_xSolidColourShader.GetReflection().PopulateLayout(xSpec.m_xPipelineLayout);
		for (Flux_BlendState& xBlendState : xSpec.m_axBlendStates)
		{
			xBlendState.m_eSrcBlendFactor = BLEND_FACTOR_ONE;
			xBlendState.m_eDstBlendFactor = BLEND_FACTOR_ZERO;
			xBlendState.m_bBlendEnabled = false;
		}
		Flux_PipelineBuilder::FromSpecification(this->m_xSolidColourPipeline, xSpec);
	}

	// ========== Atmosphere sky pipeline ==========
	{
		Flux_PipelineSpecification xSpec;
		xSpec.m_aeColourAttachmentFormats[MRT_INDEX_DIFFUSE] = MRT_FORMAT_DIFFUSE;
		xSpec.m_aeColourAttachmentFormats[MRT_INDEX_NORMALSAMBIENT] = MRT_FORMAT_NORMALSAMBIENT;
		xSpec.m_aeColourAttachmentFormats[MRT_INDEX_MATERIAL] = MRT_FORMAT_MATERIAL;
		xSpec.m_aeColourAttachmentFormats[MRT_INDEX_EMISSIVE] = MRT_FORMAT_EMISSIVE;
		xSpec.m_uNumColourAttachments = MRT_INDEX_COUNT;
		// Attach depth only so the render-pass clear resets scene depth to far.
		// The fullscreen sky draw must not depth-test or write depth.
		xSpec.m_eDepthStencilFormat = DEPTH_FORMAT;
		xSpec.m_bDepthTestEnabled = false;
		xSpec.m_bDepthWriteEnabled = false;
		xSpec.m_pxShader = &this->m_xAtmosphereShader;
		this->m_xAtmosphereShader.Initialise(FluxShaderProgram::SkyboxAtmosphere);
		this->m_xAtmosphereShader.GetReflection().PopulateLayout(xSpec.m_xPipelineLayout);
		Flux_PipelineBuilder::FromSpecification(this->m_xAtmospherePipeline, xSpec);
	}

	// ========== Aerial perspective pipeline (alpha blending) ==========
	{
		Flux_PipelineSpecification xSpec;
		xSpec.m_aeColourAttachmentFormats[0] = HDR_SCENE_FORMAT;
		xSpec.m_uNumColourAttachments = 1;
		xSpec.m_pxShader = &this->m_xAerialPerspectiveShader;
		this->m_xAerialPerspectiveShader.Initialise(FluxShaderProgram::SkyboxAerialPerspective);
		this->m_xAerialPerspectiveShader.GetReflection().PopulateLayout(xSpec.m_xPipelineLayout);
		xSpec.m_axBlendStates[0].m_bBlendEnabled = true;
		xSpec.m_axBlendStates[0].m_eSrcBlendFactor = BLEND_FACTOR_SRCALPHA;
		xSpec.m_axBlendStates[0].m_eDstBlendFactor = BLEND_FACTOR_ONEMINUSSRCALPHA;
		Flux_PipelineBuilder::FromSpecification(this->m_xAerialPerspectivePipeline, xSpec);
	}
}

void Flux_SkyboxImpl::Initialise()
{
	CreateRenderTargets();

	// Atmosphere & solid-colour CB allocations are one-time — kept in
	// Initialise so hot-reload's BuildPipelines() doesn't leak them.
	g_xEngine.FluxMemory().InitialiseDynamicConstantBuffer(&this->m_xAtmosphereConstants, sizeof(AtmosphereConstants), this->m_xAtmosphereConstantsBuffer);
	g_xEngine.FluxMemory().InitialiseDynamicConstantBuffer(&this->m_xSolidColourConstants, sizeof(SkyboxOverrideConstants), this->m_xSolidColourConstantsBuffer);

	BuildPipelines();

	// Take a ref-counted copy of the global cubemap handle (set during init in Zenith_Main).
	this->m_xCubemapTexture = g_xEngine.FluxGraphics().m_xCubemapTexture;

#ifdef ZENITH_TOOLS
	static const FluxShaderProgram s_axPrograms[] = {
		FluxShaderProgram::SkyboxCubemap,
		FluxShaderProgram::SkyboxSolidColour,
		FluxShaderProgram::SkyboxAtmosphere,
		FluxShaderProgram::SkyboxAerialPerspective,
	};
	Flux_ShaderHotReload::RegisterSubsystem([](){ g_xEngine.Skybox().BuildPipelines(); },
		s_axPrograms, sizeof(s_axPrograms) / sizeof(s_axPrograms[0]));
#endif

#ifdef ZENITH_DEBUG_VARIABLES
	RegisterDebugVariables();
#endif

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Skybox initialised");
}

void Flux_SkyboxImpl::ReleaseAssetReferences()
{
	this->m_xCubemapTexture.Clear();
}

void Flux_SkyboxImpl::Shutdown()
{
	DestroyRenderTargets();
	g_xEngine.FluxMemory().DestroyDynamicConstantBuffer(this->m_xAtmosphereConstantsBuffer);
	g_xEngine.FluxMemory().DestroyDynamicConstantBuffer(this->m_xSolidColourConstantsBuffer);
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Skybox shut down");
}

void Flux_SkyboxImpl::Reset()
{
	this->m_bLUTNeedsUpdate = true;
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_SkyboxImpl::Reset()");
}

void Flux_SkyboxImpl::CreateRenderTargets()
{
	// Create transmittance LUT for atmosphere
	Flux_RenderAttachmentBuilder xBuilder;
	xBuilder.m_uWidth = AtmosphereConfig::uTRANSMITTANCE_LUT_WIDTH;
	xBuilder.m_uHeight = AtmosphereConfig::uTRANSMITTANCE_LUT_HEIGHT;
	xBuilder.m_uMemoryFlags = 1u << MEMORY_FLAGS__SHADER_READ;
	xBuilder.m_eFormat = TEXTURE_FORMAT_R16G16B16A16_SFLOAT;

	xBuilder.BuildColour(this->m_xTransmittanceLUT, "Skybox Transmittance LUT");
}

void Flux_SkyboxImpl::DestroyRenderTargets()
{
	if (this->m_xTransmittanceLUT.m_xVRAMHandle.IsValid())
	{
		g_xEngine.FluxMemory().QueueVRAMDeletion(this->m_xTransmittanceLUT.m_xVRAMHandle,
			this->m_xTransmittanceLUT.RTV().m_xImageViewHandle, this->m_xTransmittanceLUT.DSV().m_xImageViewHandle,
			this->m_xTransmittanceLUT.SRV().m_xImageViewHandle, this->m_xTransmittanceLUT.UAV(0).m_xImageViewHandle);
	}
}


static void PreExecuteSkybox(void*)
{
	// Non-capturing graph callback (void(*)(void*)) — it cannot capture, so it
	// re-enters via g_xEngine.Skybox() to reach the singleton instance;
	// VulkanMemory is reached via g_xEngine at point of use (mirrors ExecuteSkybox).
	Flux_SkyboxImpl& xSkybox = g_xEngine.Skybox();

	const Zenith_GraphicsOptions& xOpts = Zenith_GraphicsOptions::Get();
	// Upload buffer data sequentially before parallel recording
	if (!xOpts.m_bSkyboxEnabled)
	{
		xSkybox.m_xSolidColourConstants.m_xColour = Zenith_Maths::Vector4(xOpts.m_xSkyboxColour, 1.f);
		g_xEngine.FluxMemory().UploadBufferData(xSkybox.m_xSolidColourConstantsBuffer.GetBuffer().m_xVRAMHandle, &xSkybox.m_xSolidColourConstants, sizeof(SkyboxOverrideConstants));
	}
	else if (xSkybox.IsAtmosphereEnabled())
	{
		xSkybox.m_xAtmosphereConstants.m_xRayleighScatter = Zenith_Maths::Vector4(
			AtmosphereConfig::afRAYLEIGH_SCATTER[0],
			AtmosphereConfig::afRAYLEIGH_SCATTER[1],
			AtmosphereConfig::afRAYLEIGH_SCATTER[2],
			AtmosphereConfig::fRAYLEIGH_SCALE_HEIGHT);

		xSkybox.m_xAtmosphereConstants.m_xMieScatter = Zenith_Maths::Vector4(
			AtmosphereConfig::fMIE_SCATTER,
			AtmosphereConfig::fMIE_SCATTER,
			AtmosphereConfig::fMIE_SCATTER,
			AtmosphereConfig::fMIE_SCALE_HEIGHT);

		xSkybox.m_xAtmosphereConstants.m_fPlanetRadius = AtmosphereConfig::fEARTH_RADIUS;
		xSkybox.m_xAtmosphereConstants.m_fAtmosphereRadius = AtmosphereConfig::fATMOSPHERE_RADIUS;
		xSkybox.m_xAtmosphereConstants.m_fMieG = xSkybox.GetMieG();
		xSkybox.m_xAtmosphereConstants.m_fSunIntensity = xSkybox.GetSunIntensity();

		xSkybox.m_xAtmosphereConstants.m_fRayleighScale = xSkybox.GetRayleighScale();
		xSkybox.m_xAtmosphereConstants.m_fMieScale = xSkybox.GetMieScale();
		xSkybox.m_xAtmosphereConstants.m_fAerialPerspectiveStrength = xSkybox.GetAerialPerspectiveStrength();
		xSkybox.m_xAtmosphereConstants.m_uDebugMode = dbg_uSkyboxDebugMode;

		xSkybox.m_xAtmosphereConstants.m_uSkySamples = dbg_uSkySamples;
		xSkybox.m_xAtmosphereConstants.m_uLightSamples = dbg_uLightSamples;
		xSkybox.m_xAtmosphereConstants.m_xPad = Zenith_Maths::Vector2(0.0f);

		g_xEngine.FluxMemory().UploadBufferData(xSkybox.m_xAtmosphereConstantsBuffer.GetBuffer().m_xVRAMHandle, &xSkybox.m_xAtmosphereConstants, sizeof(AtmosphereConstants));
	}
}

static void ExecuteSkybox(Flux_CommandBuffer* pxCommandList, void*)
{
	if (!Zenith_GraphicsOptions::Get().m_bSkyboxEnabled)
	{
		// Solid colour override (buffer uploaded in PreExecuteSkybox)

		// Non-capturing graph callback (void(*)(Flux_CommandBuffer*, void*)) — it
		// cannot capture, so it re-enters via g_xEngine.Skybox() to reach the
		// singleton instance; FluxGraphics is reached via g_xEngine at point of
		// use (mirrors ExecuteSDFs).
		Flux_SkyboxImpl& xSkybox = g_xEngine.Skybox();

		pxCommandList->SetPipeline(&xSkybox.m_xSolidColourPipeline);
		pxCommandList->SetVertexBuffer(g_xEngine.FluxGraphics().m_xQuadMesh.GetVertexBuffer());
		pxCommandList->SetIndexBuffer(g_xEngine.FluxGraphics().m_xQuadMesh.GetIndexBuffer());
		pxCommandList->BindCBV(&xSkybox.m_xSolidColourConstantsBuffer.GetCBV(), Flux_BindingSlot{ 0, 0, true });
		pxCommandList->DrawIndexed(6);
		return;
	}

	Flux_SkyboxImpl& xSkybox = g_xEngine.Skybox();
	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();

	if (xSkybox.IsAtmosphereEnabled())
	{
		// Atmosphere sky (constants uploaded in PreExecuteSkybox)
		pxCommandList->SetPipeline(&xSkybox.m_xAtmospherePipeline);
		pxCommandList->SetVertexBuffer(xGraphics.m_xQuadMesh.GetVertexBuffer());
		pxCommandList->SetIndexBuffer(xGraphics.m_xQuadMesh.GetIndexBuffer());

		{
			Flux_ShaderBinder xBinder(*pxCommandList);
			xBinder.BindCBV(xSkybox.m_xAtmosphereShader, "FrameConstants", &xGraphics.m_xFrameConstantsBuffer.GetCBV());
			xBinder.BindCBV(xSkybox.m_xAtmosphereShader, "AtmosphereConstants", &xSkybox.m_xAtmosphereConstantsBuffer.GetCBV());
		}

		pxCommandList->DrawIndexed(6);
	}
	else
	{
		// Cubemap sky
		Zenith_TextureAsset* pxCubemap = xSkybox.m_xCubemapTexture.GetDirect();
		if (!pxCubemap || !pxCubemap->m_xSRV.m_xImageViewHandle.IsValid())
		{
			return;
		}

		pxCommandList->SetPipeline(&xSkybox.m_xCubemapPipeline);
		pxCommandList->SetVertexBuffer(xGraphics.m_xQuadMesh.GetVertexBuffer());
		pxCommandList->SetIndexBuffer(xGraphics.m_xQuadMesh.GetIndexBuffer());
		pxCommandList->BindCBV(&xGraphics.m_xFrameConstantsBuffer.GetCBV(), Flux_BindingSlot{ 0, 0, true });
		pxCommandList->BindSRV(&pxCubemap->m_xSRV, 1);
		pxCommandList->DrawIndexed(6);
	}
}

static void ExecuteAerialPerspective(Flux_CommandBuffer* pxCommandList, void*)
{
	// Non-capturing graph callback (void(*)(Flux_CommandBuffer*, void*)) — it cannot
	// capture, so it re-enters via g_xEngine.Skybox() to reach the singleton
	// instance; FluxGraphics is reached via g_xEngine at point of use.
	Flux_SkyboxImpl& xSkybox = g_xEngine.Skybox();

	if (!xSkybox.IsAtmosphereEnabled() || !xSkybox.IsAerialPerspectiveEnabled())
	{
		return;
	}

	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();
	pxCommandList->SetPipeline(&xSkybox.m_xAerialPerspectivePipeline);
	pxCommandList->SetVertexBuffer(xGraphics.m_xQuadMesh.GetVertexBuffer());
	pxCommandList->SetIndexBuffer(xGraphics.m_xQuadMesh.GetIndexBuffer());

	{
		Flux_ShaderBinder xBinder(*pxCommandList);
		xBinder.BindCBV(xSkybox.m_xAerialPerspectiveShader, "FrameConstants", &xGraphics.m_xFrameConstantsBuffer.GetCBV());
		xBinder.BindCBV(xSkybox.m_xAerialPerspectiveShader, "AtmosphereConstants", &xSkybox.m_xAtmosphereConstantsBuffer.GetCBV());
		xBinder.BindSRV(xSkybox.m_xAerialPerspectiveShader, "g_xDepthTex", &xGraphics.GetDepthAttachment().SRV());
	}

	pxCommandList->DrawIndexed(6);
}

void Flux_SkyboxImpl::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	// Sky render pass (writes to MRT). Skybox is a fullscreen quad that writes
	// all G-Buffer channels via OutputToGBuffer, so it's the natural place to
	// clear the MRT target (both color — redundantly — and depth, which the
	// subsequent geometry passes need for depth testing).
	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();
	xGraph.AddPass("Skybox", ExecuteSkybox)
		.Writes(xGraphics.GetMRTAttachment(MRT_INDEX_DIFFUSE),        RESOURCE_ACCESS_WRITE_RTV)
		.Writes(xGraphics.GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT), RESOURCE_ACCESS_WRITE_RTV)
		.Writes(xGraphics.GetMRTAttachment(MRT_INDEX_MATERIAL),       RESOURCE_ACCESS_WRITE_RTV)
		.Writes(xGraphics.GetMRTAttachment(MRT_INDEX_EMISSIVE),       RESOURCE_ACCESS_WRITE_RTV)
		// Depth is attached purely so ClearTargets() clears it to 1.0; the
		// skybox pipelines disable depth test/write, so the draw does not touch it.
		.Writes(xGraphics.GetDepthAttachment(),                       RESOURCE_ACCESS_WRITE_DSV)
		.Prepare(PreExecuteSkybox)
		.ClearTargets();
}

void Flux_SkyboxImpl::SetupAerialPerspectiveRenderGraph(Flux_RenderGraph& xGraph)
{
	// Aerial perspective applies atmospheric scattering via SRC_ALPHA blending
	// on top of existing HDR content. It is registered AFTER DeferredShading
	// (see Flux::SetupRenderGraph) so the writer-chain topological order puts
	// it downstream of the lighting pass — otherwise it would blend into stale
	// last-frame HDR and produce garbage.
	xGraph.AddPass("Aerial Perspective", ExecuteAerialPerspective)
		.Writes(g_xEngine.HDR().GetHDRSceneTarget(),                RESOURCE_ACCESS_WRITE_RTV)
		.Reads (g_xEngine.FluxGraphics().GetDepthAttachment(),      RESOURCE_ACCESS_READ_SRV);
}

// Setters (continuous parameters; on/off toggles live in Zenith_GraphicsOptions)

// Getters
bool Flux_SkyboxImpl::IsAtmosphereEnabled() const { return Zenith_GraphicsOptions::Get().m_bSkyboxAtmosphereEnabled; }
bool Flux_SkyboxImpl::IsAerialPerspectiveEnabled() const { return Zenith_GraphicsOptions::Get().m_bSkyboxAerialPerspectiveEnabled; }

Flux_ShaderResourceView& Flux_SkyboxImpl::GetTransmittanceLUTSRV()
{
	return this->m_xTransmittanceLUT.SRV();
}

#ifdef ZENITH_DEBUG_VARIABLES
void Flux_SkyboxImpl::RegisterDebugVariables()
{
	g_xEngine.DebugVariables().AddUInt32({ "Flux", "Skybox", "DebugMode" }, dbg_uSkyboxDebugMode, 0, SKYBOX_DEBUG_COUNT - 1);
	g_xEngine.DebugVariables().AddFloat({ "Flux", "Skybox", "SunIntensity" }, dbg_fSunIntensity, 1.0f, 100.0f);
	g_xEngine.DebugVariables().AddFloat({ "Flux", "Skybox", "RayleighScale" }, dbg_fRayleighScale, 0.0f, 5.0f);
	g_xEngine.DebugVariables().AddFloat({ "Flux", "Skybox", "MieScale" }, dbg_fMieScale, 0.0f, 5.0f);
	g_xEngine.DebugVariables().AddFloat({ "Flux", "Skybox", "MieG" }, dbg_fMieG, 0.0f, 0.99f);
	g_xEngine.DebugVariables().AddFloat({ "Flux", "Skybox", "AerialStrength" }, dbg_fAerialStrength, 0.0f, 5.0f);
	g_xEngine.DebugVariables().AddUInt32({ "Flux", "Skybox", "SkySamples" }, dbg_uSkySamples, 4, 64);
	g_xEngine.DebugVariables().AddUInt32({ "Flux", "Skybox", "LightSamples" }, dbg_uLightSamples, 2, 32);

	g_xEngine.DebugVariables().AddTexture({ "Flux", "Skybox", "Textures", "TransmittanceLUT" }, this->m_xTransmittanceLUT.SRV());
}
#endif
