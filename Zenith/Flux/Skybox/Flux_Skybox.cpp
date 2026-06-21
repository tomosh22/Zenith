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
#include "Profiling/Zenith_Profiling.h"

#ifdef ZENITH_TOOLS
#include "DebugVariables/Zenith_DebugVariables.h"
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
		// Skybox now renders after the opaque geometry passes, so it depth-TESTS
		// against scene depth instead of disabling the test: the fullscreen quad
		// rasterises at NDC z=1.0 (far), and LESSEQUAL passes only where the stored
		// depth is still the far-cleared 1.0 (i.e. sky pixels), rejecting anything
		// geometry already drew. Depth WRITE stays off (the sky must not modify
		// depth); the attachment is still bound so the clear-request can reset it.
		xSpec.m_eDepthStencilFormat = DEPTH_FORMAT;
		xSpec.m_bDepthTestEnabled = true;
		xSpec.m_eDepthCompareFunc = DEPTH_COMPARE_FUNC_LESSEQUAL;
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
		// Skybox now renders after the opaque geometry passes, so it depth-TESTS
		// against scene depth instead of disabling the test: the fullscreen quad
		// rasterises at NDC z=1.0 (far), and LESSEQUAL passes only where the stored
		// depth is still the far-cleared 1.0 (i.e. sky pixels), rejecting anything
		// geometry already drew. Depth WRITE stays off (the sky must not modify
		// depth); the attachment is still bound so the clear-request can reset it.
		xSpec.m_eDepthStencilFormat = DEPTH_FORMAT;
		xSpec.m_bDepthTestEnabled = true;
		xSpec.m_eDepthCompareFunc = DEPTH_COMPARE_FUNC_LESSEQUAL;
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
		// Skybox now renders after the opaque geometry passes, so it depth-TESTS
		// against scene depth instead of disabling the test: the fullscreen quad
		// rasterises at NDC z=1.0 (far), and LESSEQUAL passes only where the stored
		// depth is still the far-cleared 1.0 (i.e. sky pixels), rejecting anything
		// geometry already drew. Depth WRITE stays off (the sky must not modify
		// depth); the attachment is still bound so the clear-request can reset it.
		xSpec.m_eDepthStencilFormat = DEPTH_FORMAT;
		xSpec.m_bDepthTestEnabled = true;
		xSpec.m_eDepthCompareFunc = DEPTH_COMPARE_FUNC_LESSEQUAL;
		xSpec.m_bDepthWriteEnabled = false;
		xSpec.m_pxShader = &this->m_xAtmosphereShader;
		this->m_xAtmosphereShader.Initialise(FluxShaderProgram::SkyboxAtmosphere);
		this->m_xAtmosphereShader.GetReflection().PopulateLayout(xSpec.m_xPipelineLayout);
		Flux_PipelineBuilder::FromSpecification(this->m_xAtmospherePipeline, xSpec);
	}

	// ========== Transmittance LUT generation pipeline (single RG/RGBA16F RT) ==========
	// Fullscreen bake into m_xTransmittanceLUT; no depth, no blend — mirrors the
	// IBL BRDF-LUT pipeline.
	Flux_PipelineHelper::BuildFullscreenPipeline(
		this->m_xTransmittanceLUTShader, this->m_xTransmittanceLUTPipeline,
		FluxShaderProgram::SkyboxTransmittanceLUT, this->m_xTransmittanceLUT.m_xSurfaceInfo.m_eFormat);

	// ========== Sky-view LUT generation pipeline (single RGBA16F RT) ==========
	Flux_PipelineHelper::BuildFullscreenPipeline(
		this->m_xSkyViewLUTShader, this->m_xSkyViewLUTPipeline,
		FluxShaderProgram::SkyboxSkyViewLUT, this->m_xSkyViewLUT.m_xSurfaceInfo.m_eFormat);

	// The transmittance / sky-view LUTs depend on these shaders, so any (re)build
	// must regenerate them. Already the default at first init; the load-bearing
	// case is hot-reload, where the shader-rebuild auto-wire fires BuildPipelines
	// directly (no per-subsystem callback to also flip this flag).
	this->m_bLUTNeedsUpdate = true;
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

	// Shader hot-reload is wired automatically from the feature registry
	// (Flux_ShaderHotReload::AutoRegisterFeatures) — every "Skybox"-subsystem
	// program rebuilds via this feature's BuildPipelines(), which also re-flags
	// the LUTs for refresh. No per-subsystem registration needed here.

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

	// Sky-view LUT (low-res lat-long). Raymarched once per frame and sampled by
	// the fullscreen sky pass. Same memory flags + format as the transmittance LUT.
	xBuilder.m_uWidth = AtmosphereConfig::uSKYVIEW_LUT_WIDTH;
	xBuilder.m_uHeight = AtmosphereConfig::uSKYVIEW_LUT_HEIGHT;
	xBuilder.BuildColour(this->m_xSkyViewLUT, "Skybox Sky-View LUT");
}

void Flux_SkyboxImpl::DestroyRenderTargets()
{
	if (this->m_xTransmittanceLUT.m_xVRAMHandle.IsValid())
	{
		g_xEngine.FluxMemory().QueueVRAMDeletion(this->m_xTransmittanceLUT.m_xVRAMHandle,
			this->m_xTransmittanceLUT.RTV().m_xImageViewHandle, this->m_xTransmittanceLUT.DSV().m_xImageViewHandle,
			this->m_xTransmittanceLUT.SRV().m_xImageViewHandle, this->m_xTransmittanceLUT.UAV(0).m_xImageViewHandle);
	}
	if (this->m_xSkyViewLUT.m_xVRAMHandle.IsValid())
	{
		g_xEngine.FluxMemory().QueueVRAMDeletion(this->m_xSkyViewLUT.m_xVRAMHandle,
			this->m_xSkyViewLUT.RTV().m_xImageViewHandle, this->m_xSkyViewLUT.DSV().m_xImageViewHandle,
			this->m_xSkyViewLUT.SRV().m_xImageViewHandle, this->m_xSkyViewLUT.UAV(0).m_xImageViewHandle);
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
	else
	{
		// Fill + upload the atmosphere constants whenever the skybox is enabled
		// (atmosphere OR cubemap mode), not just atmosphere mode: the transmittance
		// LUT generator reads these and may run (e.g. on a graph recompile) even in
		// cubemap mode. The cubemap shader ignores them; the upload is ~80 bytes.
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
			xBinder.BindSRV(xSkybox.m_xAtmosphereShader, "g_xSkyViewLUT", &xSkybox.m_xSkyViewLUT.SRV());
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

static void ExecuteTransmittanceLUT(Flux_CommandBuffer* pxCommandList, void*)
{
	// Non-capturing graph callback — recover the singleton via g_xEngine.Skybox().
	// Bakes the 256x64 transmittance LUT from AtmosphereConstants. Disabled passes
	// are skipped before record, so this only runs when the LUT needs a refresh.
	Flux_SkyboxImpl& xSkybox = g_xEngine.Skybox();
	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();

	pxCommandList->SetPipeline(&xSkybox.m_xTransmittanceLUTPipeline);
	pxCommandList->SetVertexBuffer(xGraphics.m_xQuadMesh.GetVertexBuffer());
	pxCommandList->SetIndexBuffer(xGraphics.m_xQuadMesh.GetIndexBuffer());

	{
		Flux_ShaderBinder xBinder(*pxCommandList);
		xBinder.BindCBV(xSkybox.m_xTransmittanceLUTShader, "AtmosphereConstants", &xSkybox.m_xAtmosphereConstantsBuffer.GetCBV());
	}

	pxCommandList->DrawIndexed(6);

	// Clear the dirty flag from the SUCCESSFUL record callback (not from
	// UpdateGraphPassEnables before execution) so a skipped/failed frame can't
	// drop the regeneration.
	xSkybox.m_bLUTNeedsUpdate = false;
}

static void ExecuteSkyViewLUT(Flux_CommandBuffer* pxCommandList, void*)
{
	// Non-capturing graph callback. Raymarches the atmosphere once per frame into
	// the low-res sky-view LUT, sampling the transmittance LUT for the sun-ray
	// term. The fullscreen sky pass then samples this LUT per screen pixel.
	Flux_SkyboxImpl& xSkybox = g_xEngine.Skybox();
	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();

	pxCommandList->SetPipeline(&xSkybox.m_xSkyViewLUTPipeline);
	pxCommandList->SetVertexBuffer(xGraphics.m_xQuadMesh.GetVertexBuffer());
	pxCommandList->SetIndexBuffer(xGraphics.m_xQuadMesh.GetIndexBuffer());

	{
		Flux_ShaderBinder xBinder(*pxCommandList);
		xBinder.BindCBV(xSkybox.m_xSkyViewLUTShader, "FrameConstants", &xGraphics.m_xFrameConstantsBuffer.GetCBV());
		xBinder.BindCBV(xSkybox.m_xSkyViewLUTShader, "AtmosphereConstants", &xSkybox.m_xAtmosphereConstantsBuffer.GetCBV());
		xBinder.BindSRV(xSkybox.m_xSkyViewLUTShader, "g_xTransmittanceLUT", &xSkybox.m_xTransmittanceLUT.SRV());
	}

	pxCommandList->DrawIndexed(6);
}

void Flux_SkyboxImpl::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	// Transmittance LUT generation (256x64). Camera-independent; runs only when
	// the LUT needs a refresh (gated by UpdateGraphPassEnables). Declared before
	// the "Skybox" pass for producer-before-consumer; the Read/Write edge below
	// also orders it ahead of the sky draw regardless of geometry placement.
	this->m_xTransmittanceLUTPassHandle = xGraph.AddPass("Skybox Transmittance LUT", ExecuteTransmittanceLUT)
		.ClearTargets()
		.Writes(this->m_xTransmittanceLUT, RESOURCE_ACCESS_WRITE_RTV);

	// Sky-view LUT generation. Reads the transmittance LUT, raymarches the
	// atmosphere once per frame into the low-res sky-view LUT. Enabled every frame
	// in atmosphere mode (tracks the moving sun); see UpdateGraphPassEnables.
	this->m_xSkyViewLUTPassHandle = xGraph.AddPass("Skybox Sky-View LUT", ExecuteSkyViewLUT)
		.ClearTargets()
		.Reads (this->m_xTransmittanceLUT, RESOURCE_ACCESS_READ_SRV)
		.Writes(this->m_xSkyViewLUT,       RESOURCE_ACCESS_WRITE_RTV);

	// Sky render pass (writes the G-buffer MRT). Registered AFTER the opaque
	// geometry passes (see the Flux_FeatureRegistry setup walk) so the fullscreen
	// sky draw depth-TESTS against scene depth and only shades pixels where sky is
	// visible (depth still at the far-cleared 1.0), instead of shading the whole
	// screen and being overdrawn. It still requests the clear (.ClearTargets());
	// the render graph assigns the actual clear to the FIRST opaque writer in
	// execution order (CollectClearRequirements / AssignClearFlags), so geometry is
	// cleared before it draws. In a scene with no opaque geometry the skybox is the
	// first/only writer and clears itself.
	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();
	xGraph.AddPass("Skybox", ExecuteSkybox)
		.Writes(xGraphics.GetMRTAttachment(MRT_INDEX_DIFFUSE),        RESOURCE_ACCESS_WRITE_RTV)
		.Writes(xGraphics.GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT), RESOURCE_ACCESS_WRITE_RTV)
		.Writes(xGraphics.GetMRTAttachment(MRT_INDEX_MATERIAL),       RESOURCE_ACCESS_WRITE_RTV)
		.Writes(xGraphics.GetMRTAttachment(MRT_INDEX_EMISSIVE),       RESOURCE_ACCESS_WRITE_RTV)
		// Depth is attached as WRITE_DSV so the clear-request floats up to the
		// first opaque writer (depth -> 1.0). The skybox pipelines depth-TEST
		// (LESSEQUAL) but disable depth WRITE, so this pass reads scene depth to
		// reject occluded pixels without modifying it.
		.Writes(xGraphics.GetDepthAttachment(),                       RESOURCE_ACCESS_WRITE_DSV)
		// Atmosphere path samples the sky-view LUT generated above. Declared
		// unconditionally (the cubemap/solid branches just don't sample it); the
		// sky-view writer is force-enabled on any dirty compile so this read always
		// has an enabled writer (see UpdateGraphPassEnables).
		.Reads (this->m_xSkyViewLUT,                                  RESOURCE_ACCESS_READ_SRV)
		.Prepare(PreExecuteSkybox)
		.ClearTargets();
}

void Flux_SkyboxImpl::UpdateGraphPassEnables(Flux_RenderGraph& xGraph)
{
	ZENITH_PROFILE_SCOPE("Flux Skybox LUT Setup");
	// Called from Flux_RendererImpl::ApplySubsystemGraphSelections BEFORE Compile.
	// On a dirty compile the validator (ValidateOrphanedReads) requires every
	// enabled read to have an enabled writer: the "Skybox" pass reads the
	// transmittance LUT, so force the LUT writer on for this compile. The fresh
	// attachment is also in UNDEFINED layout after a rebuild and must be
	// re-rendered. Mirrors Flux_IBLImpl::ResetIBLRegenStateForRecompile.
	if (xGraph.IsDirty())
		m_bLUTNeedsUpdate = true;

	// Regenerate when a transmittance-affecting param changes. Only the Rayleigh
	// and Mie SCALES are runtime-mutable; the coefficients/scale-heights/radii are
	// compile-time constants. Sun intensity (post-scatter multiplier), Mie-G
	// (phase function) and the sky-sample count do NOT affect transmittance and
	// are deliberately excluded.
	if (m_fRayleighScale != m_fLastLUTRayleighScale || m_fMieScale != m_fLastLUTMieScale)
	{
		m_bLUTNeedsUpdate = true;
		m_fLastLUTRayleighScale = m_fRayleighScale;
		m_fLastLUTMieScale = m_fMieScale;
	}

	// Cheap enable toggle (no MarkDirty): re-enables the writer the frame after a
	// param change without a full recompile; the flag is cleared in the record
	// callback so the pass runs exactly once per refresh.
	xGraph.SetEnabled(m_xTransmittanceLUTPassHandle, m_bLUTNeedsUpdate);

	// Sky-view LUT: regenerate EVERY frame in atmosphere mode (it tracks the
	// moving sun). Also force-enable on a dirty compile so the "Skybox" pass's
	// unconditional read of the sky-view LUT always has an enabled writer — even
	// in cubemap/solid mode, where the LUT is generated once on recompile but
	// never sampled.
	const bool bRunSkyView = IsAtmosphereEnabled() || xGraph.IsDirty();
	xGraph.SetEnabled(m_xSkyViewLUTPassHandle, bRunSkyView);
}

// Setters (continuous parameters; on/off toggles live in Zenith_GraphicsOptions)

// Getters
bool Flux_SkyboxImpl::IsAtmosphereEnabled() const { return Zenith_GraphicsOptions::Get().m_bSkyboxAtmosphereEnabled; }

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
	g_xEngine.DebugVariables().AddUInt32({ "Flux", "Skybox", "SkySamples" }, dbg_uSkySamples, 4, 64);
	g_xEngine.DebugVariables().AddUInt32({ "Flux", "Skybox", "LightSamples" }, dbg_uLightSamples, 2, 32);

	g_xEngine.DebugVariables().AddTexture({ "Flux", "Skybox", "Textures", "TransmittanceLUT" }, this->m_xTransmittanceLUT.SRV());
}
#endif
