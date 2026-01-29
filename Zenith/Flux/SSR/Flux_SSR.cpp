#include "Zenith.h"

#include "Flux/SSR/Flux_SSR.h"
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
Flux_RenderAttachment Flux_SSR::s_xRayMarchResult;
Flux_RenderAttachment Flux_SSR::s_xResolvedReflection;
Flux_TargetSetup Flux_SSR::s_xRayMarchTargetSetup;
Flux_TargetSetup Flux_SSR::s_xResolveTargetSetup;
bool Flux_SSR::s_bEnabled = true;
bool Flux_SSR::s_bInitialised = false;

// Debug variables
DEBUGVAR bool dbg_bSSREnable = true;
DEBUGVAR bool dbg_bRoughnessBlur = true;
DEBUGVAR u_int dbg_uDebugMode = SSR_DEBUG_NONE;

static struct SSRConstants
{
	float m_fIntensity = 1.0f;     // Reflection intensity multiplier [0-2]
	// Maximum ray march distance in world units (meters)
	// Longer = more accurate distant reflections but slower
	// 50m appropriate for outdoor/indoor scenes with moderate reflection distances
	float m_fMaxDistance = 50.0f;
	float m_fMaxRoughness = 1.0f;  // Allow all roughness values - confidence falloff handles blending to IBL
	// Surface thickness for hit detection in world units (meters)
	// Controls how thick surfaces appear during ray march - prevents back-face rejection issues
	// 0.5m = 50cm, appropriate for typical walls/floors; increase for thin geometry
	float m_fThickness = 0.5f;
	u_int m_uStepCount = 64;      // Max iterations for hierarchical traversal
	u_int m_uDebugMode = 0;
	u_int m_uHiZMipCount = 1;      // Filled in at render time from Flux_HiZ
	u_int m_uStartMip = 5;         // Starting mip for hierarchical traversal (higher = coarser, 5 = 1/32 res)
	u_int m_uFrameIndex = 0;       // For stochastic ray direction noise variation
	// Resolution-based binary search iterations for sub-pixel hit precision
	// Each iteration halves the search range: 6 iterations = 1/64 precision
	// 1080p: 6 iterations, 1440p: 7 iterations, 4K: 8 iterations
	u_int m_uBinarySearchIterations = 6;
	// Contact hardening distance in world units (meters)
	// Reflections are sharp within this distance, blur beyond
	// 2.0m is appropriate for human-scale environments (floor reflections)
	float m_fContactHardeningDist = 2.0f;
} dbg_xSSRConstants;

// Task system
static Zenith_Task g_xRenderTask(ZENITH_PROFILE_INDEX__FLUX_SSR, Flux_SSR::Render, nullptr);

// Command lists
static Flux_CommandList g_xRayMarchCommandList("SSR RayMarch");
static Flux_CommandList g_xResolveCommandList("SSR Resolve");

// Shaders and pipelines
static Flux_Shader s_xRayMarchShader;
static Flux_Shader s_xResolveShader;
static Flux_Pipeline s_xRayMarchPipeline;
static Flux_Pipeline s_xResolvePipeline;

// Cached binding handles for ray march pass (from shader reflection)
static Flux_BindingHandle s_xRM_FrameConstantsBinding;
static Flux_BindingHandle s_xRM_DepthTexBinding;
static Flux_BindingHandle s_xRM_NormalsTexBinding;
static Flux_BindingHandle s_xRM_MaterialTexBinding;
static Flux_BindingHandle s_xRM_HiZTexBinding;
static Flux_BindingHandle s_xRM_DiffuseTexBinding;
static Flux_BindingHandle s_xRM_BlueNoiseTexBinding;

// Cached binding handles for resolve pass (from shader reflection)
static Flux_BindingHandle s_xRS_FrameConstantsBinding;
static Flux_BindingHandle s_xRS_RayMarchResultBinding;
static Flux_BindingHandle s_xRS_NormalsTexBinding;
static Flux_BindingHandle s_xRS_MaterialTexBinding;
static Flux_BindingHandle s_xRS_DepthTexBinding;

void Flux_SSR::CreateRenderTargets()
{
	u_int uWidth = Flux_Swapchain::GetWidth();
	u_int uHeight = Flux_Swapchain::GetHeight();

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_SSR::CreateRenderTargets() - Resolution: %ux%u", uWidth, uHeight);

	// Create ray march result target (RGBA16F)
	{
		Flux_RenderAttachmentBuilder xBuilder;
		xBuilder.m_uWidth = uWidth;
		xBuilder.m_uHeight = uHeight;
		xBuilder.m_eFormat = TEXTURE_FORMAT_R16G16B16A16_SFLOAT;
		xBuilder.m_uMemoryFlags = (1u << MEMORY_FLAGS__SHADER_READ);

		xBuilder.BuildColour(s_xRayMarchResult, "SSR RayMarch Result");

		s_xRayMarchTargetSetup.m_axColourAttachments[0] = s_xRayMarchResult;
		s_xRayMarchTargetSetup.m_pxDepthStencil = nullptr;
	}

	// Create resolved reflection target (RGBA16F)
	{
		Flux_RenderAttachmentBuilder xBuilder;
		xBuilder.m_uWidth = uWidth;
		xBuilder.m_uHeight = uHeight;
		xBuilder.m_eFormat = TEXTURE_FORMAT_R16G16B16A16_SFLOAT;
		xBuilder.m_uMemoryFlags = (1u << MEMORY_FLAGS__SHADER_READ);

		xBuilder.BuildColour(s_xResolvedReflection, "SSR Resolved");

		s_xResolveTargetSetup.m_axColourAttachments[0] = s_xResolvedReflection;
		s_xResolveTargetSetup.m_pxDepthStencil = nullptr;
	}
}

void Flux_SSR::DestroyRenderTargets()
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
			xAttachment.m_xVRAMHandle = Flux_VRAMHandle();
		}
	};

	QueueDeletion(s_xRayMarchResult);
	QueueDeletion(s_xResolvedReflection);

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_SSR::DestroyRenderTargets()");
}

void Flux_SSR::Initialise()
{
	CreateRenderTargets();

	{
		// Initialize ray march shader and pipeline
		s_xRayMarchShader.Initialise("Flux_Fullscreen_UV.vert", "SSR/Flux_SSR_RayMarch.frag");

		const Flux_ShaderReflection& xReflection = s_xRayMarchShader.GetReflection();
		s_xRM_FrameConstantsBinding = xReflection.GetBinding("FrameConstants");
		s_xRM_DepthTexBinding = xReflection.GetBinding("g_xDepthTex");
		s_xRM_NormalsTexBinding = xReflection.GetBinding("g_xNormalsTex");
		s_xRM_MaterialTexBinding = xReflection.GetBinding("g_xMaterialTex");
		s_xRM_HiZTexBinding = xReflection.GetBinding("g_xHiZTex");
		s_xRM_DiffuseTexBinding = xReflection.GetBinding("g_xDiffuseTex");
		s_xRM_BlueNoiseTexBinding = xReflection.GetBinding("g_xBlueNoiseTex");

		Flux_VertexInputDescription xVertexDesc;
		xVertexDesc.m_eTopology = MESH_TOPOLOGY_NONE;

		Flux_PipelineSpecification xPipelineSpec;
		xPipelineSpec.m_pxTargetSetup = &s_xRayMarchTargetSetup;
		xPipelineSpec.m_pxShader = &s_xRayMarchShader;
		xPipelineSpec.m_xVertexInputDesc = xVertexDesc;

		xPipelineSpec.m_bDepthTestEnabled = false;
		xPipelineSpec.m_bDepthWriteEnabled = false;

		xReflection.PopulateLayout(xPipelineSpec.m_xPipelineLayout);

		Flux_PipelineBuilder::FromSpecification(s_xRayMarchPipeline, xPipelineSpec);
	}

	{
		// Initialize resolve shader and pipeline
		s_xResolveShader.Initialise("Flux_Fullscreen_UV.vert", "SSR/Flux_SSR_Resolve.frag");

		const Flux_ShaderReflection& xReflection = s_xResolveShader.GetReflection();
		s_xRS_FrameConstantsBinding = xReflection.GetBinding("FrameConstants");
		s_xRS_RayMarchResultBinding = xReflection.GetBinding("g_xRayMarchTex");
		s_xRS_NormalsTexBinding = xReflection.GetBinding("g_xNormalsTex");
		s_xRS_MaterialTexBinding = xReflection.GetBinding("g_xMaterialTex");
		s_xRS_DepthTexBinding = xReflection.GetBinding("g_xDepthTex");

		Flux_VertexInputDescription xVertexDesc;
		xVertexDesc.m_eTopology = MESH_TOPOLOGY_NONE;

		Flux_PipelineSpecification xPipelineSpec;
		xPipelineSpec.m_pxTargetSetup = &s_xResolveTargetSetup;
		xPipelineSpec.m_pxShader = &s_xResolveShader;
		xPipelineSpec.m_xVertexInputDesc = xVertexDesc;

		xPipelineSpec.m_bDepthTestEnabled = false;
		xPipelineSpec.m_bDepthWriteEnabled = false;

		xReflection.PopulateLayout(xPipelineSpec.m_xPipelineLayout);

		Flux_PipelineBuilder::FromSpecification(s_xResolvePipeline, xPipelineSpec);
	}

	// Cache binding handles from shader reflection for resolve pass
	{
		
	}

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({ "Flux", "SSR", "Enable" }, dbg_bSSREnable);
	Zenith_DebugVariables::AddBoolean({ "Flux", "SSR", "RoughnessBlur" }, dbg_bRoughnessBlur);
	Zenith_DebugVariables::AddUInt32({ "Flux", "SSR", "DebugMode" }, dbg_uDebugMode, 0, 100);  // Extended range for diagnostic mode 99
	Zenith_DebugVariables::AddFloat({ "Flux", "SSR", "Intensity" }, dbg_xSSRConstants.m_fIntensity, 0.0f, 2.0f);
	Zenith_DebugVariables::AddFloat({ "Flux", "SSR", "MaxDistance" }, dbg_xSSRConstants.m_fMaxDistance, 1.0f, 100.0f);
	Zenith_DebugVariables::AddFloat({ "Flux", "SSR", "MaxRoughness" }, dbg_xSSRConstants.m_fMaxRoughness, 0.0f, 1.0f);
	Zenith_DebugVariables::AddFloat({ "Flux", "SSR", "Thickness" }, dbg_xSSRConstants.m_fThickness, 0.01f, 1.0f);
	Zenith_DebugVariables::AddUInt32({ "Flux", "SSR", "StepCount" }, dbg_xSSRConstants.m_uStepCount, 8, 256);
	Zenith_DebugVariables::AddUInt32({ "Flux", "SSR", "StartMip" }, dbg_xSSRConstants.m_uStartMip, 0, 10);
	Zenith_DebugVariables::AddFloat({ "Flux", "SSR", "ContactHardeningDist" }, dbg_xSSRConstants.m_fContactHardeningDist, 0.5f, 10.0f);
	Zenith_DebugVariables::AddTexture({ "Flux", "SSR", "Textures", "RayMarch" }, s_xRayMarchResult.m_pxSRV);
	Zenith_DebugVariables::AddTexture({ "Flux", "SSR", "Textures", "Resolved" }, s_xResolvedReflection.m_pxSRV);
#endif

	// Register resize callback to recreate render targets on window resize
	Flux::AddResChangeCallback([]()
	{
		Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_SSR resize callback triggered");

		DestroyRenderTargets();
		CreateRenderTargets();

		// Reset command lists to clear any cached descriptor bindings pointing to old textures
		g_xRayMarchCommandList.Reset(true);
		g_xResolveCommandList.Reset(true);

#ifdef ZENITH_DEBUG_VARIABLES
		// Re-register debug textures with the new SRVs (old ones were destroyed)
		Zenith_DebugVariables::AddTexture({ "Flux", "SSR", "Textures", "RayMarch" }, s_xRayMarchResult.m_pxSRV);
		Zenith_DebugVariables::AddTexture({ "Flux", "SSR", "Textures", "Resolved" }, s_xResolvedReflection.m_pxSRV);
#endif

		Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_SSR resize complete - textures re-registered");
	});

	s_bInitialised = true;
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_SSR initialised");
}

void Flux_SSR::Shutdown()
{
	if (!s_bInitialised)
		return;

	DestroyRenderTargets();

	s_bInitialised = false;
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_SSR shut down");
}

void Flux_SSR::Reset()
{
	g_xRayMarchCommandList.Reset(true);
	g_xResolveCommandList.Reset(true);
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_SSR::Reset()");
}

void Flux_SSR::SubmitRenderTask()
{
	Zenith_TaskSystem::SubmitTask(&g_xRenderTask);
}

void Flux_SSR::WaitForRenderTask()
{
	g_xRenderTask.WaitUntilComplete();
}

void Flux_SSR::Render(void*)
{
	if (!dbg_bSSREnable || !s_bInitialised)
		return;

	// SSR REQUIRES Hi-Z buffer for hierarchical ray marching
	// Without HiZ, SSR would need O(N) linear marching instead of O(log N)
	if (!Flux_HiZ::IsEnabled())
	{
		static bool s_bHiZWarningShown = false;
		if (!s_bHiZWarningShown)
		{
			Zenith_Warning(LOG_CATEGORY_RENDERER,
				"Flux_SSR: SSR is enabled but HiZ is disabled. "
				"SSR requires Hi-Z for hierarchical ray marching. "
				"Enable HiZ via 'Flux/HiZ/Enable' debug variable, or disable SSR.");
			s_bHiZWarningShown = true;
		}
		return;
	}

	// Update constants from debug variables and HiZ system
	dbg_xSSRConstants.m_uDebugMode = dbg_uDebugMode;
	dbg_xSSRConstants.m_uHiZMipCount = Flux_HiZ::GetMipCount();
	dbg_xSSRConstants.m_uFrameIndex = Flux::GetFrameCounter();

	// Resolution-based binary search iterations for sub-pixel hit precision
	// 1080p (1920): 6 iterations (1/64 pixel precision)
	// 1440p (2560): 7 iterations (1/128 pixel precision)
	// 4K (3840): 8 iterations (1/256 pixel precision)
	// Using standard resolution breakpoints for accurate thresholding
	u_int uWidth = Flux_Swapchain::GetWidth();
	dbg_xSSRConstants.m_uBinarySearchIterations = 6 + (uWidth > 1920 ? 1 : 0) + (uWidth > 2560 ? 1 : 0);

	// Clamp start mip to valid range
	if (dbg_xSSRConstants.m_uStartMip >= dbg_xSSRConstants.m_uHiZMipCount)
		dbg_xSSRConstants.m_uStartMip = dbg_xSSRConstants.m_uHiZMipCount - 1;

	RenderRayMarch();

	if (dbg_bRoughnessBlur)
	{
		RenderResolve();
	}
}

void Flux_SSR::RenderRayMarch()
{
	g_xRayMarchCommandList.Reset(true);  // Full reset to update frame constants each frame

	g_xRayMarchCommandList.AddCommand<Flux_CommandSetPipeline>(&s_xRayMarchPipeline);

	g_xRayMarchCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	g_xRayMarchCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

	// Use shader binder for ALL bindings - ALWAYS use hardcoded bindings for debugging
	Flux_ShaderBinder xBinder(g_xRayMarchCommandList);

	// Bind frame constants (always hardcoded for now)
	xBinder.BindCBV(s_xRM_FrameConstantsBinding, &Flux_Graphics::s_xFrameConstantsBuffer.GetCBV());
	xBinder.PushConstant(&dbg_xSSRConstants, sizeof(SSRConstants));

	// Bind textures - ALWAYS use hardcoded binding indices matching shader layout
	xBinder.BindSRV(s_xRM_DepthTexBinding, Flux_Graphics::GetDepthStencilSRV());
	xBinder.BindSRV(s_xRM_NormalsTexBinding, Flux_Graphics::GetGBufferSRV(MRT_INDEX_NORMALSAMBIENT));
	xBinder.BindSRV(s_xRM_MaterialTexBinding, Flux_Graphics::GetGBufferSRV(MRT_INDEX_MATERIAL));
	xBinder.BindSRV(s_xRM_HiZTexBinding, &Flux_HiZ::GetHiZSRV());
	xBinder.BindSRV(s_xRM_DiffuseTexBinding, Flux_Graphics::GetGBufferSRV(MRT_INDEX_DIFFUSE));
	xBinder.BindSRV(s_xRM_BlueNoiseTexBinding, &Flux_VolumeFog::GetBlueNoiseTexture()->m_xSRV);

	g_xRayMarchCommandList.AddCommand<Flux_CommandDrawIndexed>(6);

	// Debug: log command count to verify commands are being added
	static u_int s_uFrameCount = 0;
	if (s_uFrameCount++ % 60 == 0)
	{
		Zenith_Log(LOG_CATEGORY_RENDERER, "[SSR RayMarch] Commands: %u, DebugMode: %u",
			g_xRayMarchCommandList.GetCommandCount(), dbg_xSSRConstants.m_uDebugMode);
	}

	Flux::SubmitCommandList(&g_xRayMarchCommandList, s_xRayMarchTargetSetup, RENDER_ORDER_SSR_RAYMARCH);
}

void Flux_SSR::RenderResolve()
{
	g_xResolveCommandList.Reset(true);  // Full reset to update frame constants each frame

	g_xResolveCommandList.AddCommand<Flux_CommandSetPipeline>(&s_xResolvePipeline);

	g_xResolveCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	g_xResolveCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

	// Use shader binder for ALL bindings - ALWAYS use hardcoded bindings for debugging
	Flux_ShaderBinder xBinder(g_xResolveCommandList);

	// Bind frame constants (always hardcoded for now)
	xBinder.BindCBV(s_xRS_FrameConstantsBinding, &Flux_Graphics::s_xFrameConstantsBuffer.GetCBV());
	xBinder.PushConstant(&dbg_xSSRConstants, sizeof(SSRConstants));

	// Bind textures - ALWAYS use hardcoded binding indices matching shader layout
	xBinder.BindSRV(s_xRS_RayMarchResultBinding, &s_xRayMarchResult.m_pxSRV);
	xBinder.BindSRV(s_xRS_NormalsTexBinding, Flux_Graphics::GetGBufferSRV(MRT_INDEX_NORMALSAMBIENT));
	xBinder.BindSRV(s_xRS_MaterialTexBinding, Flux_Graphics::GetGBufferSRV(MRT_INDEX_MATERIAL));
	xBinder.BindSRV(s_xRS_DepthTexBinding, Flux_Graphics::GetDepthStencilSRV());

	g_xResolveCommandList.AddCommand<Flux_CommandDrawIndexed>(6);

	Flux::SubmitCommandList(&g_xResolveCommandList, s_xResolveTargetSetup, RENDER_ORDER_SSR_RESOLVE);
}

Flux_ShaderResourceView& Flux_SSR::GetReflectionSRV()
{
	// Return resolved if blur is enabled, otherwise raw ray march result
	if (dbg_bRoughnessBlur)
		return s_xResolvedReflection.m_pxSRV;
	return s_xRayMarchResult.m_pxSRV;
}

bool Flux_SSR::IsEnabled()
{
	return dbg_bSSREnable && s_bInitialised;
}

bool Flux_SSR::IsInitialised()
{
	return s_bInitialised;
}
