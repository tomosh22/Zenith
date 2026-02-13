#include "Zenith.h"

#include "Flux/SSGI/Flux_SSGI.h"
#include "Flux/HiZ/Flux_HiZ.h"
#include "Flux/Flux.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/HDR/Flux_HDR.h"
#include "Flux/Fog/Flux_VolumeFog.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "TaskSystem/Zenith_TaskSystem.h"

// Static member definitions
Flux_RenderAttachment Flux_SSGI::s_xRawResult;
Flux_RenderAttachment Flux_SSGI::s_xResolved;
Flux_RenderAttachment Flux_SSGI::s_xDenoised;
Flux_TargetSetup Flux_SSGI::s_xRayMarchTargetSetup;
Flux_TargetSetup Flux_SSGI::s_xUpsampleTargetSetup;
Flux_TargetSetup Flux_SSGI::s_xDenoiseTargetSetup;
bool Flux_SSGI::s_bEnabled = true;
bool Flux_SSGI::s_bInitialised = false;

// Debug variables
DEBUGVAR bool dbg_bSSGIEnable = false;
DEBUGVAR u_int dbg_uDebugMode = SSGI_DEBUG_NONE;

static struct SSGIConstants
{
	float m_fIntensity = 1.0f;          // GI intensity multiplier [0-2]
	float m_fMaxDistance = 30.0f;       // Maximum ray march distance in world units
	float m_fThickness = 0.5f;          // Surface thickness for hit detection
	u_int m_uStepCount = 32;            // Ray march steps (HiZ traversal iterations)
	u_int m_uFrameIndex = 0;            // For noise variation
	u_int m_uHiZMipCount = 1;           // Filled from Flux_HiZ
	u_int m_uDebugMode = 0;
	float m_fRoughnessThreshold = 0.0f; // Below this, skip SSGI (0 = process all)
	u_int m_uStartMip = 4;              // Starting mip for HiZ traversal
	u_int m_uRaysPerPixel = 3;          // Number of hemisphere samples per pixel (1-8, default 3)
	float _pad0;
	float _pad1;
} dbg_xSSGIConstants;

// Denoise constants - joint bilateral filter parameters
static struct SSGIDenoiseConstants
{
	float m_fSpatialSigma = 2.0f;       // Spatial Gaussian sigma (pixels)
	float m_fDepthSigma = 0.02f;        // Depth threshold (fraction of local depth)
	float m_fNormalSigma = 0.5f;        // Normal threshold (1 - dot product range)
	float m_fAlbedoSigma = 0.1f;        // Albedo threshold (color distance)
	u_int m_uKernelRadius = 4;          // Filter radius in pixels (4 = 9x9 kernel)
	u_int m_bEnabled = 1;               // Enable/disable denoise pass
	float _pad0;
	float _pad1;
} dbg_xSSGIDenoiseConstants;

// Task system
static Zenith_Task g_xRenderTask(ZENITH_PROFILE_INDEX__FLUX_SSGI, Flux_SSGI::Render, nullptr);

// Command lists
static Flux_CommandList g_xRayMarchCommandList("SSGI RayMarch");
static Flux_CommandList g_xUpsampleCommandList("SSGI Upsample");
static Flux_CommandList g_xDenoiseCommandList("SSGI Denoise");

// Shaders and pipelines
static Flux_Shader s_xRayMarchShader;
static Flux_Shader s_xUpsampleShader;
static Flux_Shader s_xDenoiseShader;
static Flux_Pipeline s_xRayMarchPipeline;
static Flux_Pipeline s_xUpsamplePipeline;
static Flux_Pipeline s_xDenoisePipeline;

// Cached binding handles for ray march pass
static Flux_BindingHandle s_xRM_FrameConstantsBinding;
static Flux_BindingHandle s_xRM_DepthTexBinding;
static Flux_BindingHandle s_xRM_NormalsTexBinding;
static Flux_BindingHandle s_xRM_MaterialTexBinding;
static Flux_BindingHandle s_xRM_HiZTexBinding;
static Flux_BindingHandle s_xRM_DiffuseTexBinding;
static Flux_BindingHandle s_xRM_BlueNoiseTexBinding;

// Cached binding handles for upsample pass
static Flux_BindingHandle s_xUS_SSGITexBinding;
static Flux_BindingHandle s_xUS_DepthTexBinding;

// Cached binding handles for denoise pass
static Flux_BindingHandle s_xDN_SSGITexBinding;
static Flux_BindingHandle s_xDN_DepthTexBinding;
static Flux_BindingHandle s_xDN_NormalsTexBinding;
static Flux_BindingHandle s_xDN_AlbedoTexBinding;

void Flux_SSGI::CreateRenderTargets()
{
	u_int uFullWidth = Flux_Swapchain::GetWidth();
	u_int uFullHeight = Flux_Swapchain::GetHeight();
	u_int uHalfWidth = uFullWidth / 2;
	u_int uHalfHeight = uFullHeight / 2;

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_SSGI::CreateRenderTargets() - Full: %ux%u, Half: %ux%u",
		uFullWidth, uFullHeight, uHalfWidth, uHalfHeight);

	// Create raw ray march result (half-res, RGBA16F)
	{
		Flux_RenderAttachmentBuilder xBuilder;
		xBuilder.m_uWidth = uHalfWidth;
		xBuilder.m_uHeight = uHalfHeight;
		xBuilder.m_eFormat = TEXTURE_FORMAT_R16G16B16A16_SFLOAT;
		xBuilder.m_uMemoryFlags = (1u << MEMORY_FLAGS__SHADER_READ);

		xBuilder.BuildColour(s_xRawResult, "SSGI RayMarch Result");

		s_xRayMarchTargetSetup.m_axColourAttachments[0] = s_xRawResult;
		s_xRayMarchTargetSetup.m_pxDepthStencil = nullptr;
	}

	// Create resolved/upsampled result (full-res, RGBA16F)
	{
		Flux_RenderAttachmentBuilder xBuilder;
		xBuilder.m_uWidth = uFullWidth;
		xBuilder.m_uHeight = uFullHeight;
		xBuilder.m_eFormat = TEXTURE_FORMAT_R16G16B16A16_SFLOAT;
		xBuilder.m_uMemoryFlags = (1u << MEMORY_FLAGS__SHADER_READ);

		xBuilder.BuildColour(s_xResolved, "SSGI Resolved");

		s_xUpsampleTargetSetup.m_axColourAttachments[0] = s_xResolved;
		s_xUpsampleTargetSetup.m_pxDepthStencil = nullptr;
	}

	// Create denoised result (full-res, RGBA16F)
	{
		Flux_RenderAttachmentBuilder xBuilder;
		xBuilder.m_uWidth = uFullWidth;
		xBuilder.m_uHeight = uFullHeight;
		xBuilder.m_eFormat = TEXTURE_FORMAT_R16G16B16A16_SFLOAT;
		xBuilder.m_uMemoryFlags = (1u << MEMORY_FLAGS__SHADER_READ);

		xBuilder.BuildColour(s_xDenoised, "SSGI Denoised");

		s_xDenoiseTargetSetup.m_axColourAttachments[0] = s_xDenoised;
		s_xDenoiseTargetSetup.m_pxDepthStencil = nullptr;
	}
}

void Flux_SSGI::DestroyRenderTargets()
{
	auto QueueDeletion = [](Flux_RenderAttachment& xAttachment)
	{
		if (xAttachment.m_xVRAMHandle.IsValid())
		{
			Zenith_Vulkan_VRAM* pxVRAM = Zenith_Vulkan::GetVRAM(xAttachment.m_xVRAMHandle);
			Flux_MemoryManager::QueueVRAMDeletion(pxVRAM, xAttachment.m_xVRAMHandle,
				xAttachment.m_pxRTV.m_xImageViewHandle,
				xAttachment.m_pxDSV.m_xImageViewHandle,
				xAttachment.m_pxSRV.m_xImageViewHandle,
				xAttachment.m_pxUAV.m_xImageViewHandle);
		}
	};

	QueueDeletion(s_xRawResult);
	QueueDeletion(s_xResolved);
	QueueDeletion(s_xDenoised);

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_SSGI::DestroyRenderTargets()");
}

void Flux_SSGI::Initialise()
{
	CreateRenderTargets();

	// Initialize ray march shader and pipeline
	Flux_PipelineHelper::BuildFullscreenPipeline(
		s_xRayMarchShader, s_xRayMarchPipeline,
		"SSGI/Flux_SSGI_RayMarch.frag", &s_xRayMarchTargetSetup);

	{
		const Flux_ShaderReflection& xReflection = s_xRayMarchShader.GetReflection();
		s_xRM_FrameConstantsBinding = xReflection.GetBinding("FrameConstants");
		s_xRM_DepthTexBinding = xReflection.GetBinding("g_xDepthTex");
		s_xRM_NormalsTexBinding = xReflection.GetBinding("g_xNormalsTex");
		s_xRM_MaterialTexBinding = xReflection.GetBinding("g_xMaterialTex");
		s_xRM_HiZTexBinding = xReflection.GetBinding("g_xHiZTex");
		s_xRM_DiffuseTexBinding = xReflection.GetBinding("g_xDiffuseTex");
		s_xRM_BlueNoiseTexBinding = xReflection.GetBinding("g_xBlueNoiseTex");
	}

	// Initialize upsample shader and pipeline
	Flux_PipelineHelper::BuildFullscreenPipeline(
		s_xUpsampleShader, s_xUpsamplePipeline,
		"SSGI/Flux_SSGI_Upsample.frag", &s_xUpsampleTargetSetup);

	{
		const Flux_ShaderReflection& xReflection = s_xUpsampleShader.GetReflection();
		s_xUS_SSGITexBinding = xReflection.GetBinding("g_xSSGITex");
		s_xUS_DepthTexBinding = xReflection.GetBinding("g_xDepthTex");
	}

	// Initialize denoise shader and pipeline
	Flux_PipelineHelper::BuildFullscreenPipeline(
		s_xDenoiseShader, s_xDenoisePipeline,
		"SSGI/Flux_SSGI_Denoise.frag", &s_xDenoiseTargetSetup);

	{
		const Flux_ShaderReflection& xReflection = s_xDenoiseShader.GetReflection();
		s_xDN_SSGITexBinding = xReflection.GetBinding("g_xSSGITex");
		s_xDN_DepthTexBinding = xReflection.GetBinding("g_xDepthTex");
		s_xDN_NormalsTexBinding = xReflection.GetBinding("g_xNormalsTex");
		s_xDN_AlbedoTexBinding = xReflection.GetBinding("g_xAlbedoTex");
	}

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({ "Flux", "SSGI", "Enable" }, dbg_bSSGIEnable);
	Zenith_DebugVariables::AddUInt32({ "Flux", "SSGI", "DebugMode" }, dbg_uDebugMode, 0, SSGI_DEBUG_COUNT - 1);
	Zenith_DebugVariables::AddFloat({ "Flux", "SSGI", "Intensity" }, dbg_xSSGIConstants.m_fIntensity, 0.0f, 2.0f);
	Zenith_DebugVariables::AddFloat({ "Flux", "SSGI", "MaxDistance" }, dbg_xSSGIConstants.m_fMaxDistance, 1.0f, 100.0f);
	Zenith_DebugVariables::AddFloat({ "Flux", "SSGI", "Thickness" }, dbg_xSSGIConstants.m_fThickness, 0.01f, 2.0f);
	Zenith_DebugVariables::AddUInt32({ "Flux", "SSGI", "StepCount" }, dbg_xSSGIConstants.m_uStepCount, 8, 128);
	Zenith_DebugVariables::AddUInt32({ "Flux", "SSGI", "StartMip" }, dbg_xSSGIConstants.m_uStartMip, 0, 10);
	Zenith_DebugVariables::AddUInt32({ "Flux", "SSGI", "RaysPerPixel" }, dbg_xSSGIConstants.m_uRaysPerPixel, 1, 8);
	Zenith_DebugVariables::AddTexture({ "Flux", "SSGI", "Textures", "Raw" }, s_xRawResult.m_pxSRV);
	Zenith_DebugVariables::AddTexture({ "Flux", "SSGI", "Textures", "Resolved" }, s_xResolved.m_pxSRV);
	Zenith_DebugVariables::AddTexture({ "Flux", "SSGI", "Textures", "Denoised" }, s_xDenoised.m_pxSRV);

	// Denoise debug variables
	Zenith_DebugVariables::AddBoolean({ "Flux", "SSGI", "Denoise", "Enable" }, reinterpret_cast<bool&>(dbg_xSSGIDenoiseConstants.m_bEnabled));
	Zenith_DebugVariables::AddUInt32({ "Flux", "SSGI", "Denoise", "KernelRadius" }, dbg_xSSGIDenoiseConstants.m_uKernelRadius, 1, 8);
	Zenith_DebugVariables::AddFloat({ "Flux", "SSGI", "Denoise", "SpatialSigma" }, dbg_xSSGIDenoiseConstants.m_fSpatialSigma, 0.5f, 4.0f);
	Zenith_DebugVariables::AddFloat({ "Flux", "SSGI", "Denoise", "DepthSigma" }, dbg_xSSGIDenoiseConstants.m_fDepthSigma, 0.01f, 0.1f);
	Zenith_DebugVariables::AddFloat({ "Flux", "SSGI", "Denoise", "NormalSigma" }, dbg_xSSGIDenoiseConstants.m_fNormalSigma, 0.1f, 1.0f);
	Zenith_DebugVariables::AddFloat({ "Flux", "SSGI", "Denoise", "AlbedoSigma" }, dbg_xSSGIDenoiseConstants.m_fAlbedoSigma, 0.05f, 0.5f);
#endif

	// Register resize callback
	Flux::AddResChangeCallback([]()
	{
		Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_SSGI resize callback triggered");

		DestroyRenderTargets();
		CreateRenderTargets();

		g_xRayMarchCommandList.Reset(true);
		g_xUpsampleCommandList.Reset(true);
		g_xDenoiseCommandList.Reset(true);

#ifdef ZENITH_DEBUG_VARIABLES
		Zenith_DebugVariables::AddTexture({ "Flux", "SSGI", "Textures", "Raw" }, s_xRawResult.m_pxSRV);
		Zenith_DebugVariables::AddTexture({ "Flux", "SSGI", "Textures", "Resolved" }, s_xResolved.m_pxSRV);
		Zenith_DebugVariables::AddTexture({ "Flux", "SSGI", "Textures", "Denoised" }, s_xDenoised.m_pxSRV);
#endif

		Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_SSGI resize complete");
	});

	s_bInitialised = true;
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_SSGI initialised");
}

void Flux_SSGI::Shutdown()
{
	if (!s_bInitialised)
		return;

	DestroyRenderTargets();

	s_bInitialised = false;
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_SSGI shut down");
}

void Flux_SSGI::Reset()
{
	g_xRayMarchCommandList.Reset(true);
	g_xUpsampleCommandList.Reset(true);
	g_xDenoiseCommandList.Reset(true);
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_SSGI::Reset()");
}

void Flux_SSGI::SubmitRenderTask()
{
	Zenith_TaskSystem::SubmitTask(&g_xRenderTask);
}

void Flux_SSGI::WaitForRenderTask()
{
	g_xRenderTask.WaitUntilComplete();
}

void Flux_SSGI::Render(void*)
{
#pragma warning(push)
#pragma warning(disable: 4127) // conditional expression is constant (dbg_bSSGIEnable is const in non-tools builds)
	if (!dbg_bSSGIEnable || !s_bEnabled || !s_bInitialised)
		return;
#pragma warning(pop)

	// SSGI requires Hi-Z buffer for accelerated ray marching
	if (!Flux_HiZ::IsEnabled())
	{
		static bool s_bHiZWarningShown = false;
		if (!s_bHiZWarningShown)
		{
			Zenith_Warning(LOG_CATEGORY_RENDERER,
				"Flux_SSGI: SSGI is enabled but HiZ is disabled. "
				"SSGI requires Hi-Z for hierarchical ray marching. "
				"Enable HiZ via 'Flux/HiZ/Enable' debug variable, or disable SSGI.");
			s_bHiZWarningShown = true;
		}
		return;
	}

	// Update constants
	dbg_xSSGIConstants.m_uDebugMode = dbg_uDebugMode;
	dbg_xSSGIConstants.m_uHiZMipCount = Flux_HiZ::GetMipCount();
	dbg_xSSGIConstants.m_uFrameIndex = Flux::GetFrameCounter();

	// Clamp start mip to valid range
	if (dbg_xSSGIConstants.m_uStartMip >= dbg_xSSGIConstants.m_uHiZMipCount)
		dbg_xSSGIConstants.m_uStartMip = dbg_xSSGIConstants.m_uHiZMipCount - 1;

	RenderRayMarch();
	RenderUpsample();
	RenderDenoise();
}

void Flux_SSGI::RenderRayMarch()
{
	g_xRayMarchCommandList.Reset(true);

	g_xRayMarchCommandList.AddCommand<Flux_CommandSetPipeline>(&s_xRayMarchPipeline);

	g_xRayMarchCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	g_xRayMarchCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(g_xRayMarchCommandList);

	xBinder.BindCBV(s_xRM_FrameConstantsBinding, &Flux_Graphics::s_xFrameConstantsBuffer.GetCBV());
	xBinder.PushConstant(&dbg_xSSGIConstants, sizeof(SSGIConstants));

	xBinder.BindSRV(s_xRM_DepthTexBinding, Flux_Graphics::GetDepthStencilSRV());
	xBinder.BindSRV(s_xRM_NormalsTexBinding, Flux_Graphics::GetGBufferSRV(MRT_INDEX_NORMALSAMBIENT));
	xBinder.BindSRV(s_xRM_MaterialTexBinding, Flux_Graphics::GetGBufferSRV(MRT_INDEX_MATERIAL));
	xBinder.BindSRV(s_xRM_HiZTexBinding, &Flux_HiZ::GetHiZSRV());
	xBinder.BindSRV(s_xRM_DiffuseTexBinding, Flux_Graphics::GetGBufferSRV(MRT_INDEX_DIFFUSE));
	xBinder.BindSRV(s_xRM_BlueNoiseTexBinding, &Flux_VolumeFog::GetBlueNoiseTexture()->m_xSRV);

	g_xRayMarchCommandList.AddCommand<Flux_CommandDrawIndexed>(6);

	Flux::SubmitCommandList(&g_xRayMarchCommandList, s_xRayMarchTargetSetup, RENDER_ORDER_SSGI_RAYMARCH);
}

void Flux_SSGI::RenderUpsample()
{
	g_xUpsampleCommandList.Reset(true);

	g_xUpsampleCommandList.AddCommand<Flux_CommandSetPipeline>(&s_xUpsamplePipeline);

	g_xUpsampleCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	g_xUpsampleCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(g_xUpsampleCommandList);

	xBinder.BindSRV(s_xUS_SSGITexBinding, &s_xRawResult.m_pxSRV);
	xBinder.BindSRV(s_xUS_DepthTexBinding, Flux_Graphics::GetDepthStencilSRV());

	g_xUpsampleCommandList.AddCommand<Flux_CommandDrawIndexed>(6);

	Flux::SubmitCommandList(&g_xUpsampleCommandList, s_xUpsampleTargetSetup, RENDER_ORDER_SSGI_UPSAMPLE);
}

void Flux_SSGI::RenderDenoise()
{
	g_xDenoiseCommandList.Reset(true);

	g_xDenoiseCommandList.AddCommand<Flux_CommandSetPipeline>(&s_xDenoisePipeline);

	g_xDenoiseCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	g_xDenoiseCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(g_xDenoiseCommandList);

	// Push denoise constants
	xBinder.PushConstant(&dbg_xSSGIDenoiseConstants, sizeof(SSGIDenoiseConstants));

	// Bind textures
	xBinder.BindSRV(s_xDN_SSGITexBinding, &s_xResolved.m_pxSRV);
	xBinder.BindSRV(s_xDN_DepthTexBinding, Flux_Graphics::GetDepthStencilSRV());
	xBinder.BindSRV(s_xDN_NormalsTexBinding, Flux_Graphics::GetGBufferSRV(MRT_INDEX_NORMALSAMBIENT));
	xBinder.BindSRV(s_xDN_AlbedoTexBinding, Flux_Graphics::GetGBufferSRV(MRT_INDEX_DIFFUSE));

	g_xDenoiseCommandList.AddCommand<Flux_CommandDrawIndexed>(6);

	Flux::SubmitCommandList(&g_xDenoiseCommandList, s_xDenoiseTargetSetup, RENDER_ORDER_SSGI_DENOISE);
}

Flux_ShaderResourceView& Flux_SSGI::GetSSGISRV()
{
	// Return denoised output if denoise is enabled, otherwise return upsampled result
	if (dbg_xSSGIDenoiseConstants.m_bEnabled)
		return s_xDenoised.m_pxSRV;
	return s_xResolved.m_pxSRV;
}

bool Flux_SSGI::IsEnabled()
{
	return dbg_bSSGIEnable && s_bEnabled && s_bInitialised;
}

bool Flux_SSGI::IsInitialised()
{
	return s_bInitialised;
}
