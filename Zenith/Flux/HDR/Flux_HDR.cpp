#include "Zenith.h"

#include <atomic>
#include "Flux_HDR.h"
#include "Flux/Flux.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Flux_CommandList.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Vulkan/Zenith_Vulkan_MemoryManager.h"
#include "TaskSystem/Zenith_TaskSystem.h"
#include "Core/Zenith_Core.h"
#include "UI/Zenith_UICanvas.h"

#ifdef ZENITH_TOOLS
#include "DebugVariables/Zenith_DebugVariables.h"
#endif

// Task for async rendering
static Zenith_Task g_xRenderTask(ZENITH_PROFILE_INDEX__FLUX_HDR, Flux_HDR::Render, nullptr);

// Command lists - separate command list per bloom pass to avoid pointer aliasing
// (SubmitCommandList stores pointers, so each pass needs its own command list)
static Flux_CommandList g_xBloomThresholdCommandList("HDR_Bloom_Threshold");
// 5 bloom chain levels (downsample from threshold -> mip1,2,3,4, so 4 downsample passes)
// Upsample from mip4 -> mip3,2,1,0, so 4 upsample passes
static Flux_CommandList g_axBloomDownsampleCommandLists[4] = {
	Flux_CommandList("HDR_Bloom_Down0"),
	Flux_CommandList("HDR_Bloom_Down1"),
	Flux_CommandList("HDR_Bloom_Down2"),
	Flux_CommandList("HDR_Bloom_Down3")
};
static Flux_CommandList g_axBloomUpsampleCommandLists[4] = {
	Flux_CommandList("HDR_Bloom_Up0"),
	Flux_CommandList("HDR_Bloom_Up1"),
	Flux_CommandList("HDR_Bloom_Up2"),
	Flux_CommandList("HDR_Bloom_Up3")
};
static Flux_CommandList g_xToneMappingCommandList("HDR_ToneMapping");

// Shaders
static Flux_Shader s_xToneMappingShader;
static Flux_Shader s_xBloomThresholdShader;
static Flux_Shader s_xBloomDownsampleShader;
static Flux_Shader s_xBloomUpsampleShader;

// Static member definitions
Flux_RenderAttachment Flux_HDR::s_xHDRSceneTarget;
Flux_TargetSetup Flux_HDR::s_xHDRSceneTargetSetup;
Flux_TargetSetup Flux_HDR::s_xHDRSceneTargetSetupWithDepth;

Flux_RenderAttachment Flux_HDR::s_axBloomChain[5];
Flux_TargetSetup Flux_HDR::s_axBloomChainSetup[5];

Flux_Pipeline Flux_HDR::s_xToneMappingPipeline;
Flux_Pipeline Flux_HDR::s_xBloomDownsamplePipeline;
Flux_Pipeline Flux_HDR::s_xBloomUpsamplePipeline;
Flux_Pipeline Flux_HDR::s_xBloomThresholdPipeline;

float Flux_HDR::s_fExposure = 1.0f;
float Flux_HDR::s_fBloomIntensity = 0.5f;
float Flux_HDR::s_fBloomThreshold = 1.0f;
ToneMappingOperator Flux_HDR::s_eToneMappingOperator = TONEMAPPING_ACES;
bool Flux_HDR::s_bBloomEnabled = true;
bool Flux_HDR::s_bAutoExposure = true;

float Flux_HDR::s_fCurrentExposure = 1.0f;
float Flux_HDR::s_fAverageLuminance = 0.18f;

// Auto-exposure parameters
float Flux_HDR::s_fAdaptationSpeed = 2.0f;
float Flux_HDR::s_fTargetLuminance = 0.18f;
float Flux_HDR::s_fMinExposure = 0.1f;
float Flux_HDR::s_fMaxExposure = 10.0f;
float Flux_HDR::s_fMinLogLuminance = -10.0f;
float Flux_HDR::s_fLogLuminanceRange = 12.0f;

// Auto-exposure compute resources
Flux_ReadWriteBuffer Flux_HDR::s_xHistogramBuffer;
Flux_ReadWriteBuffer Flux_HDR::s_xExposureBuffer;
Zenith_Vulkan_Pipeline Flux_HDR::s_xLuminanceHistogramPipeline;
Zenith_Vulkan_Pipeline Flux_HDR::s_xAdaptationPipeline;
Zenith_Vulkan_Shader Flux_HDR::s_xLuminanceHistogramShader;
Zenith_Vulkan_Shader Flux_HDR::s_xAdaptationShader;
Zenith_Vulkan_RootSig Flux_HDR::s_xLuminanceRootSig;
Zenith_Vulkan_RootSig Flux_HDR::s_xAdaptationRootSig;

// Command lists for compute
static Flux_CommandList g_xLuminanceHistogramCommandList("HDR_LuminanceHistogram");
static Flux_CommandList g_xAdaptationCommandList("HDR_Adaptation");

// Cached binding handles from shader reflection
static Flux_BindingHandle s_xToneMappingConstantsBinding;
static Flux_BindingHandle s_xToneMappingHDRTexBinding;
static Flux_BindingHandle s_xToneMappingBloomTexBinding;
static Flux_BindingHandle s_xToneMappingHistogramBinding;
static Flux_BindingHandle s_xToneMappingExposureBinding;

static Flux_BindingHandle s_xBloomThresholdHDRTexBinding;
static Flux_BindingHandle s_xBloomThresholdConstantsBinding;
static Flux_BindingHandle s_xBloomDownsampleSourceBinding;
static Flux_BindingHandle s_xBloomDownsampleConstantsBinding;
static Flux_BindingHandle s_xBloomUpsampleSourceBinding;
static Flux_BindingHandle s_xBloomUpsampleConstantsBinding;

// Cached binding handles for compute shaders (from shader reflection)
static Flux_BindingHandle s_xLuminanceConstantsBinding;
static Flux_BindingHandle s_xLuminanceHDRTexBinding;
static Flux_BindingHandle s_xLuminanceHistogramBinding;

static Flux_BindingHandle s_xAdaptationConstantsBinding;
static Flux_BindingHandle s_xAdaptationHistogramBinding;
static Flux_BindingHandle s_xAdaptationExposureBinding;

// Debug variables

DEBUGVAR u_int dbg_uHDRDebugMode = HDR_DEBUG_NONE;
DEBUGVAR float dbg_fHDRExposure = 1.0f;
DEBUGVAR bool dbg_bHDRAutoExposure = true;   // Must match s_bAutoExposure default
DEBUGVAR bool dbg_bHDRBloomEnabled = true;   // Must match s_bBloomEnabled default
DEBUGVAR float dbg_fHDRBloomIntensity = 0.5f;
DEBUGVAR float dbg_fHDRBloomThreshold = 1.0f;
DEBUGVAR u_int dbg_uHDRToneMappingOperator = TONEMAPPING_ACES;
DEBUGVAR bool dbg_bHDRShowHistogram = false;
DEBUGVAR bool dbg_bHDRFreezeExposure = false;
DEBUGVAR float dbg_fHDRAdaptationSpeed = 2.0f;
DEBUGVAR float dbg_fHDRTargetLuminance = 0.18f;
DEBUGVAR float dbg_fHDRMinExposure = 0.1f;
DEBUGVAR float dbg_fHDRMaxExposure = 10.0f;

// Track auto-exposure state transitions to ensure clean histogram on enable
static bool s_bAutoExposureWasEnabled = false;

// GPU constant buffer layouts (defined here rather than inline in render functions)
struct BloomConstants
{
	float m_fThreshold;
	float m_fIntensity;
	Zenith_Maths::Vector2 m_xTexelSize;
};

struct ToneMappingConstants
{
	float m_fExposure;
	float m_fBloomIntensity;
	u_int m_uToneMappingOperator;
	u_int m_uDebugMode;
	u_int m_bShowHistogram;
	u_int m_bAutoExposure;
	u_int m_uPad0;
	u_int m_uPad1;
};

struct LuminanceConstants
{
	u_int m_uImageWidth;
	u_int m_uImageHeight;
	float m_fMinLogLum;
	float m_fLogLumRange;
};

struct AdaptationConstants
{
	float m_fMinLogLum;
	float m_fLogLumRange;
	float m_fDeltaTime;
	float m_fAdaptationSpeed;
	float m_fTargetLuminance;
	float m_fMinExposure;
	float m_fMaxExposure;
	float m_fLowPercentile;
	float m_fHighPercentile;
	u_int m_uTotalPixels;
	u_int m_uPad0;
	u_int m_uPad1;
};

void Flux_HDR::SyncDebugVariables()
{
	s_fExposure = dbg_fHDRExposure;
	s_bBloomEnabled = dbg_bHDRBloomEnabled;
	s_fBloomIntensity = dbg_fHDRBloomIntensity;
	s_fBloomThreshold = dbg_fHDRBloomThreshold;
	s_eToneMappingOperator = static_cast<ToneMappingOperator>(dbg_uHDRToneMappingOperator);
	s_bAutoExposure = dbg_bHDRAutoExposure;
	s_fAdaptationSpeed = dbg_fHDRAdaptationSpeed;
	s_fTargetLuminance = dbg_fHDRTargetLuminance;
	s_fMinExposure = dbg_fHDRMinExposure;
	s_fMaxExposure = dbg_fHDRMaxExposure;
}

void Flux_HDR::Initialise()
{
	CreateRenderTargets();

	// Initialize tone mapping shader and pipeline
	Flux_PipelineHelper::BuildFullscreenPipeline(
		s_xToneMappingShader, s_xToneMappingPipeline,
		"HDR/Flux_ToneMapping.frag", &Flux_Graphics::s_xFinalRenderTarget_NoDepth);

	// Cache binding handles from reflection and validate
	const Flux_ShaderReflection& xToneMappingReflection = s_xToneMappingShader.GetReflection();
	s_xToneMappingConstantsBinding = xToneMappingReflection.GetBinding("ToneMappingConstants");
	s_xToneMappingHDRTexBinding = xToneMappingReflection.GetBinding("g_xHDRTex");
	s_xToneMappingBloomTexBinding = xToneMappingReflection.GetBinding("g_xBloomTex");
	s_xToneMappingHistogramBinding = xToneMappingReflection.GetBinding("HistogramBuffer");
	s_xToneMappingExposureBinding = xToneMappingReflection.GetBinding("ExposureBuffer");

	// Validate critical bindings were found (runtime checks for release builds too)
	if (!s_xToneMappingConstantsBinding.IsValid())
		Zenith_Error(LOG_CATEGORY_RENDERER, "Flux_HDR: ToneMappingConstants binding not found in shader");
	if (!s_xToneMappingHDRTexBinding.IsValid())
		Zenith_Error(LOG_CATEGORY_RENDERER, "Flux_HDR: g_xHDRTex binding not found in shader");
	if (!s_xToneMappingBloomTexBinding.IsValid())
		Zenith_Error(LOG_CATEGORY_RENDERER, "Flux_HDR: g_xBloomTex binding not found in shader");
	if (!s_xToneMappingHistogramBinding.IsValid())
		Zenith_Error(LOG_CATEGORY_RENDERER, "Flux_HDR: HistogramBuffer binding not found in shader");
	if (!s_xToneMappingExposureBinding.IsValid())
		Zenith_Error(LOG_CATEGORY_RENDERER, "Flux_HDR: ExposureBuffer binding not found in shader");

	// Initialize bloom threshold shader and pipeline
	Flux_PipelineHelper::BuildFullscreenPipeline(
		s_xBloomThresholdShader, s_xBloomThresholdPipeline,
		"HDR/Flux_BloomThreshold.frag", &s_axBloomChainSetup[0]);

	// Cache binding handles from reflection and validate
	const Flux_ShaderReflection& xThresholdReflection = s_xBloomThresholdShader.GetReflection();
	s_xBloomThresholdHDRTexBinding = xThresholdReflection.GetBinding("g_xHDRTex");
	s_xBloomThresholdConstantsBinding = xThresholdReflection.GetBinding("BloomConstants");

	if (!s_xBloomThresholdHDRTexBinding.IsValid())
		Zenith_Error(LOG_CATEGORY_RENDERER, "Flux_HDR: g_xHDRTex binding not found in bloom threshold shader");
	if (!s_xBloomThresholdConstantsBinding.IsValid())
		Zenith_Error(LOG_CATEGORY_RENDERER, "Flux_HDR: BloomConstants binding not found in bloom threshold shader");

	// Initialize bloom downsample shader and pipeline
	Flux_PipelineHelper::BuildFullscreenPipeline(
		s_xBloomDownsampleShader, s_xBloomDownsamplePipeline,
		"HDR/Flux_BloomDownsample.frag", &s_axBloomChainSetup[1]);

	// Cache binding handles from reflection and validate
	const Flux_ShaderReflection& xDownsampleReflection = s_xBloomDownsampleShader.GetReflection();
	s_xBloomDownsampleSourceBinding = xDownsampleReflection.GetBinding("g_xSourceTex");
	s_xBloomDownsampleConstantsBinding = xDownsampleReflection.GetBinding("BloomConstants");

	if (!s_xBloomDownsampleSourceBinding.IsValid())
		Zenith_Error(LOG_CATEGORY_RENDERER, "Flux_HDR: g_xSourceTex binding not found in bloom downsample shader");
	if (!s_xBloomDownsampleConstantsBinding.IsValid())
		Zenith_Error(LOG_CATEGORY_RENDERER, "Flux_HDR: BloomConstants binding not found in bloom downsample shader");

	// Initialize bloom upsample shader and pipeline (additive blending)
	{
		Flux_PipelineSpecification xSpec = Flux_PipelineHelper::CreateFullscreenSpec(
			s_xBloomUpsampleShader, "HDR/Flux_BloomUpsample.frag", &s_axBloomChainSetup[0]);
		xSpec.m_axBlendStates[0].m_eSrcBlendFactor = BLEND_FACTOR_ONE;
		xSpec.m_axBlendStates[0].m_eDstBlendFactor = BLEND_FACTOR_ONE;
		Flux_PipelineBuilder::FromSpecification(s_xBloomUpsamplePipeline, xSpec);
	}

	// Cache binding handles from reflection and validate
	const Flux_ShaderReflection& xUpsampleReflection = s_xBloomUpsampleShader.GetReflection();
	s_xBloomUpsampleSourceBinding = xUpsampleReflection.GetBinding("g_xSourceTex");
	s_xBloomUpsampleConstantsBinding = xUpsampleReflection.GetBinding("BloomConstants");

	if (!s_xBloomUpsampleSourceBinding.IsValid())
		Zenith_Error(LOG_CATEGORY_RENDERER, "Flux_HDR: g_xSourceTex binding not found in bloom upsample shader");
	if (!s_xBloomUpsampleConstantsBinding.IsValid())
		Zenith_Error(LOG_CATEGORY_RENDERER, "Flux_HDR: BloomConstants binding not found in bloom upsample shader");

	// Initialize auto-exposure compute pipelines
	InitializeAutoExposure();

#ifdef ZENITH_TOOLS
	RegisterDebugVariables();
#endif

	Flux::AddResChangeCallback([]()
	{
		DestroyRenderTargets();
		CreateRenderTargets();
	});

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_HDR Initialised");
}

void Flux_HDR::Shutdown()
{
	DestroyRenderTargets();

	// Cleanup auto-exposure compute resources
	Zenith_Vulkan_MemoryManager::DestroyReadWriteBuffer(s_xHistogramBuffer);
	Zenith_Vulkan_MemoryManager::DestroyReadWriteBuffer(s_xExposureBuffer);

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_HDR shut down");
}

void Flux_HDR::Reset()
{
	g_xBloomThresholdCommandList.Reset(true);
	for (u_int u = 0; u < 4; u++)
	{
		g_axBloomDownsampleCommandLists[u].Reset(true);
		g_axBloomUpsampleCommandLists[u].Reset(true);
	}
	g_xToneMappingCommandList.Reset(true);
	g_xLuminanceHistogramCommandList.Reset(true);
	g_xAdaptationCommandList.Reset(true);
	s_fCurrentExposure = 1.0f;
	s_fAverageLuminance = 0.18f;

	// Clear histogram buffer to prevent stale data after scene transitions
	// This ensures auto-exposure starts fresh when enabled
	if (s_xHistogramBuffer.GetBuffer().m_xVRAMHandle.IsValid())
	{
		static u_int auZeroHistogram[256] = { 0 };
		Zenith_Vulkan_MemoryManager::UploadBufferData(
			s_xHistogramBuffer.GetBuffer().m_xVRAMHandle,
			auZeroHistogram,
			256 * sizeof(u_int));
	}

	// Reset exposure buffer to default values
	if (s_xExposureBuffer.GetBuffer().m_xVRAMHandle.IsValid())
	{
		float afInitialExposure[4] = { 0.18f, 1.0f, 1.0f, 0.0f };
		Zenith_Vulkan_MemoryManager::UploadBufferData(
			s_xExposureBuffer.GetBuffer().m_xVRAMHandle,
			afInitialExposure,
			4 * sizeof(float));
	}
}

void Flux_HDR::CreateRenderTargets()
{
	Flux_RenderAttachmentBuilder xBuilder;
	xBuilder.m_uWidth = Flux_Swapchain::GetWidth();
	xBuilder.m_uHeight = Flux_Swapchain::GetHeight();
	xBuilder.m_uMemoryFlags = 1u << MEMORY_FLAGS__SHADER_READ;
	xBuilder.m_eFormat = TEXTURE_FORMAT_R16G16B16A16_SFLOAT;

	xBuilder.BuildColour(s_xHDRSceneTarget, "Flux HDR Scene Target");
	s_xHDRSceneTargetSetup.m_axColourAttachments[0] = s_xHDRSceneTarget;
	s_xHDRSceneTargetSetup.m_pxDepthStencil = nullptr;  // Explicitly ensure no depth attachment

	// Set up target with depth (for passes like particles/SDFs that need depth testing)
	s_xHDRSceneTargetSetupWithDepth.m_axColourAttachments[0] = s_xHDRSceneTarget;
	s_xHDRSceneTargetSetupWithDepth.AssignDepthStencil(&Flux_Graphics::s_xDepthBuffer);

	u_int uBloomWidth = Flux_Swapchain::GetWidth() / 2;
	u_int uBloomHeight = Flux_Swapchain::GetHeight() / 2;

	for (u_int i = 0; i < 5; i++)
	{
		xBuilder.m_uWidth = uBloomWidth;
		xBuilder.m_uHeight = uBloomHeight;
		xBuilder.BuildColour(s_axBloomChain[i], "Flux Bloom Chain " + std::to_string(i));
		s_axBloomChainSetup[i].m_axColourAttachments[0] = s_axBloomChain[i];
		s_axBloomChainSetup[i].m_pxDepthStencil = nullptr;  // Explicitly no depth for bloom passes

		uBloomWidth = std::max(1u, uBloomWidth / 2);
		uBloomHeight = std::max(1u, uBloomHeight / 2);
	}
}

void Flux_HDR::DestroyRenderTargets()
{
	auto DestroyAttachment = [](Flux_RenderAttachment& xAttachment)
	{
		if (xAttachment.m_xVRAMHandle.IsValid())
		{
			Zenith_Vulkan_VRAM* pxVRAM = Zenith_Vulkan::GetVRAM(xAttachment.m_xVRAMHandle);
			Flux_MemoryManager::QueueVRAMDeletion(pxVRAM, xAttachment.m_xVRAMHandle,
				xAttachment.m_pxRTV.m_xImageViewHandle, xAttachment.m_pxDSV.m_xImageViewHandle,
				xAttachment.m_pxSRV.m_xImageViewHandle, xAttachment.m_pxUAV.m_xImageViewHandle);
		}
	};

	DestroyAttachment(s_xHDRSceneTarget);

	for (u_int i = 0; i < 5; i++)
	{
		DestroyAttachment(s_axBloomChain[i]);
	}
}

void Flux_HDR::InitializeAutoExposure()
{
	// Create histogram buffer (256 bins, each uint32)
	// Initialize with zeros
	u_int auZeroHistogram[256] = { 0 };
	Zenith_Vulkan_MemoryManager::InitialiseReadWriteBuffer(auZeroHistogram, 256 * sizeof(u_int), Flux_HDR::s_xHistogramBuffer);

	// Validate histogram buffer creation
	if (!Flux_HDR::s_xHistogramBuffer.GetBuffer().m_xVRAMHandle.IsValid())
	{
		Zenith_Error(LOG_CATEGORY_RENDERER, "Flux_HDR: Failed to create histogram buffer");
		return;
	}

	// Create exposure buffer (4 floats: avgLum, currentExp, targetExp, pad)
	// Initialize with default values
	float afInitialExposure[4] = { 0.18f, 1.0f, 1.0f, 0.0f };
	Zenith_Vulkan_MemoryManager::InitialiseReadWriteBuffer(afInitialExposure, 4 * sizeof(float), Flux_HDR::s_xExposureBuffer);

	// Validate exposure buffer creation
	if (!Flux_HDR::s_xExposureBuffer.GetBuffer().m_xVRAMHandle.IsValid())
	{
		Zenith_Error(LOG_CATEGORY_RENDERER, "Flux_HDR: Failed to create exposure buffer");
		return;
	}

	// Initialize luminance histogram compute shader
	Flux_HDR::s_xLuminanceHistogramShader.InitialiseCompute("HDR/Flux_Luminance.comp");

	// Build luminance histogram root signature from shader reflection
	const Flux_ShaderReflection& xLuminanceReflection = Flux_HDR::s_xLuminanceHistogramShader.GetReflection();
	Zenith_Vulkan_RootSigBuilder::FromReflection(Flux_HDR::s_xLuminanceRootSig, xLuminanceReflection);

	// Cache binding handles from reflection for use at render time
	s_xLuminanceConstantsBinding = xLuminanceReflection.GetBinding("LuminanceConstants");
	s_xLuminanceHDRTexBinding = xLuminanceReflection.GetBinding("g_xHDRTex");
	s_xLuminanceHistogramBinding = xLuminanceReflection.GetBinding("g_auHistogram");

	// Build luminance histogram pipeline
	Zenith_Vulkan_ComputePipelineBuilder xLuminanceBuilder;
	xLuminanceBuilder.WithShader(Flux_HDR::s_xLuminanceHistogramShader)
		.WithLayout(Flux_HDR::s_xLuminanceRootSig.m_xLayout)
		.Build(Flux_HDR::s_xLuminanceHistogramPipeline);
	Flux_HDR::s_xLuminanceHistogramPipeline.m_xRootSig = Flux_HDR::s_xLuminanceRootSig;

	// Initialize adaptation compute shader
	Flux_HDR::s_xAdaptationShader.InitialiseCompute("HDR/Flux_Adaptation.comp");

	// Build adaptation root signature from shader reflection
	const Flux_ShaderReflection& xAdaptationReflection = Flux_HDR::s_xAdaptationShader.GetReflection();
	Zenith_Vulkan_RootSigBuilder::FromReflection(Flux_HDR::s_xAdaptationRootSig, xAdaptationReflection);

	// Cache binding handles from reflection for use at render time
	s_xAdaptationConstantsBinding = xAdaptationReflection.GetBinding("AdaptationConstants");
	s_xAdaptationHistogramBinding = xAdaptationReflection.GetBinding("g_auHistogram");
	s_xAdaptationExposureBinding = xAdaptationReflection.GetBinding("g_afExposureData");

	// Build adaptation pipeline
	Zenith_Vulkan_ComputePipelineBuilder xAdaptationBuilder;
	xAdaptationBuilder.WithShader(Flux_HDR::s_xAdaptationShader)
		.WithLayout(Flux_HDR::s_xAdaptationRootSig.m_xLayout)
		.Build(Flux_HDR::s_xAdaptationPipeline);
	Flux_HDR::s_xAdaptationPipeline.m_xRootSig = Flux_HDR::s_xAdaptationRootSig;

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_HDR: Auto-exposure compute pipelines initialized");
}

void Flux_HDR::ComputeLuminanceHistogram()
{
	// Guard against uninitialized buffers (can occur if InitializeAutoExposure failed)
	if (!s_xHistogramBuffer.GetBuffer().m_xVRAMHandle.IsValid())
	{
		return;
	}

	// Clear histogram buffer to zero before compute
	// This ensures consistent results regardless of workgroup execution order
	// SYNC NOTE: UploadBufferData() is processed in RENDER_ORDER_MEMORY_UPDATE which
	// executes before RENDER_ORDER_HDR_LUMINANCE. The memory submit waits on a semaphore
	// before render work begins, ensuring the transfer completes before compute reads.
	// TODO: Consider using vkCmdFillBuffer for GPU-side clear to reduce CPU overhead
	static u_int auZeroHistogram[256] = { 0 };
	Zenith_Vulkan_MemoryManager::UploadBufferData(
		s_xHistogramBuffer.GetBuffer().m_xVRAMHandle,
		auZeroHistogram,
		256 * sizeof(u_int));

	g_xLuminanceHistogramCommandList.Reset(false);

	g_xLuminanceHistogramCommandList.AddCommand<Flux_CommandBindComputePipeline>(&s_xLuminanceHistogramPipeline);

	LuminanceConstants xConsts;
	xConsts.m_uImageWidth = Flux_Swapchain::GetWidth();
	xConsts.m_uImageHeight = Flux_Swapchain::GetHeight();
	xConsts.m_fMinLogLum = s_fMinLogLuminance;
	xConsts.m_fLogLumRange = s_fLogLuminanceRange;

	// Use reflection-based binding handles
	Flux_ShaderBinder xBinder(g_xLuminanceHistogramCommandList);
	xBinder.PushConstant(s_xLuminanceConstantsBinding, &xConsts, sizeof(xConsts));
	xBinder.BindSRV(s_xLuminanceHDRTexBinding, &s_xHDRSceneTarget.m_pxSRV);
	xBinder.BindUAV_Buffer(s_xLuminanceHistogramBinding, &s_xHistogramBuffer.GetUAV());

	// Dispatch: divide screen into 16x16 workgroups
	u_int uGroupsX = (Flux_Swapchain::GetWidth() + 15) / 16;
	u_int uGroupsY = (Flux_Swapchain::GetHeight() + 15) / 16;
	g_xLuminanceHistogramCommandList.AddCommand<Flux_CommandDispatch>(uGroupsX, uGroupsY, 1);

	Flux::SubmitCommandList(&g_xLuminanceHistogramCommandList, Flux_Graphics::s_xNullTargetSetup, RENDER_ORDER_HDR_LUMINANCE);
}

void Flux_HDR::ComputeExposureAdaptation()
{
	// Guard against uninitialized buffers (can occur if InitializeAutoExposure failed)
	if (!s_xHistogramBuffer.GetBuffer().m_xVRAMHandle.IsValid() ||
		!s_xExposureBuffer.GetBuffer().m_xVRAMHandle.IsValid())
	{
		return;
	}

	g_xAdaptationCommandList.Reset(false);

	g_xAdaptationCommandList.AddCommand<Flux_CommandBindComputePipeline>(&s_xAdaptationPipeline);

	AdaptationConstants xConsts;
	xConsts.m_fMinLogLum = s_fMinLogLuminance;
	xConsts.m_fLogLumRange = s_fLogLuminanceRange;
	xConsts.m_fDeltaTime = Zenith_Core::GetDt();
	xConsts.m_fAdaptationSpeed = dbg_bHDRFreezeExposure ? 0.0f : s_fAdaptationSpeed;
	xConsts.m_fTargetLuminance = s_fTargetLuminance;
	xConsts.m_fMinExposure = s_fMinExposure;
	xConsts.m_fMaxExposure = s_fMaxExposure;
	xConsts.m_fLowPercentile = 0.05f;   // Ignore darkest 5%
	xConsts.m_fHighPercentile = 0.95f;  // Ignore brightest 5%
	xConsts.m_uTotalPixels = Flux_Swapchain::GetWidth() * Flux_Swapchain::GetHeight();
	xConsts.m_uPad0 = 0;
	xConsts.m_uPad1 = 0;

	// Use reflection-based binding handles
	Flux_ShaderBinder xBinder(g_xAdaptationCommandList);
	xBinder.PushConstant(s_xAdaptationConstantsBinding, &xConsts, sizeof(xConsts));
	xBinder.BindUAV_Buffer(s_xAdaptationHistogramBinding, &s_xHistogramBuffer.GetUAV());
	xBinder.BindUAV_Buffer(s_xAdaptationExposureBinding, &s_xExposureBuffer.GetUAV());

	// Dispatch single workgroup with 256 threads
	g_xAdaptationCommandList.AddCommand<Flux_CommandDispatch>(1, 1, 1);

	Flux::SubmitCommandList(&g_xAdaptationCommandList, Flux_Graphics::s_xNullTargetSetup, RENDER_ORDER_HDR_ADAPTATION);

	// Note: GPU-side exposure values are used directly in shaders.
	// CPU-side readback not currently implemented - using default values for debug display.
	// The auto-exposure still works correctly on the GPU.
}

void Flux_HDR::Render(void*)
{
	SyncDebugVariables();

	// Auto-exposure: compute luminance histogram and adapt exposure
	// Also compute histogram if ShowHistogram is enabled (for visualization)
	// Track state transition to ensure clean histogram when auto-exposure is first enabled
	bool bAutoExposureJustEnabled = s_bAutoExposure && !s_bAutoExposureWasEnabled;
	s_bAutoExposureWasEnabled = s_bAutoExposure;

	if (s_bAutoExposure || dbg_bHDRShowHistogram)
	{
		ComputeLuminanceHistogram();

		// If auto-exposure was just enabled, reset exposure to default
		// to prevent jarring transition from potentially stale values
		if (bAutoExposureJustEnabled)
		{
			float afInitialExposure[4] = { 0.18f, 1.0f, 1.0f, 0.0f };
			Zenith_Vulkan_MemoryManager::UploadBufferData(
				s_xExposureBuffer.GetBuffer().m_xVRAMHandle,
				afInitialExposure,
				4 * sizeof(float));
		}

		// Always run adaptation - it computes histogram max for visualization
		// and exposure adaptation when auto-exposure is enabled
		ComputeExposureAdaptation();
	}

	// Bloom pass
	if (s_bBloomEnabled)
	{
		RenderBloom();
	}

	// Always run tone mapping - deferred shading renders to HDR target,
	// tone mapping converts to final LDR output
	RenderToneMapping();
}

static void SubmitHistogramLabels()
{
	// Histogram position matches shader: bottom-left corner
	// fMargin = 0.02, fHistWidth = 0.3, fHistHeight = 0.15
	const float fScreenWidth = static_cast<float>(Flux_Swapchain::GetWidth());
	const float fScreenHeight = static_cast<float>(Flux_Swapchain::GetHeight());

	const float fMargin = 0.02f;
	const float fHistWidth = 0.3f;
	const float fHistHeight = 0.15f;

	// Convert normalized coords to pixels
	// Bottom of histogram in pixel coords (Y increases downward in screen space)
	const float fHistLeft = fMargin * fScreenWidth;
	const float fHistBottom = (1.0f - fMargin) * fScreenHeight;
	const float fHistTop = fHistBottom - fHistHeight * fScreenHeight;
	const float fHistRight = fHistLeft + fHistWidth * fScreenWidth;

	const float fLabelSize = 14.0f;
	const float fTitleSize = 16.0f;

	// Colors - all white/gray for grayscale histogram
	const Zenith_Maths::Vector4 xWhite(1.0f, 1.0f, 1.0f, 1.0f);
	const Zenith_Maths::Vector4 xGray(0.7f, 0.7f, 0.7f, 1.0f);

	// Get text entries vector
	Zenith_Vector<Zenith_UI::UITextEntry>& xTextEntries = Zenith_UI::Zenith_UICanvas::GetPendingTextEntries();

	// Title above histogram
	{
		Zenith_UI::UITextEntry xEntry;
		xEntry.m_strText = "Luminance Histogram";
		xEntry.m_xPosition = Zenith_Maths::Vector2(fHistLeft, fHistTop - fTitleSize - 5.0f);
		xEntry.m_fSize = fTitleSize;
		xEntry.m_xColor = xWhite;
		xTextEntries.PushBack(xEntry);
	}

	// Zone labels below histogram
	const float fLabelY = fHistBottom + 5.0f;
	const float fZoneWidth = (fHistRight - fHistLeft) / 3.0f;

	// Shadows label (left third)
	{
		Zenith_UI::UITextEntry xEntry;
		xEntry.m_strText = "Shadows";
		xEntry.m_xPosition = Zenith_Maths::Vector2(fHistLeft + fZoneWidth * 0.1f, fLabelY);
		xEntry.m_fSize = fLabelSize;
		xEntry.m_xColor = xGray;
		xTextEntries.PushBack(xEntry);
	}

	// Midtones label (middle third)
	{
		Zenith_UI::UITextEntry xEntry;
		xEntry.m_strText = "Mids";
		xEntry.m_xPosition = Zenith_Maths::Vector2(fHistLeft + fZoneWidth * 1.3f, fLabelY);
		xEntry.m_fSize = fLabelSize;
		xEntry.m_xColor = xGray;
		xTextEntries.PushBack(xEntry);
	}

	// Highlights label (right third)
	{
		Zenith_UI::UITextEntry xEntry;
		xEntry.m_strText = "Highs";
		xEntry.m_xPosition = Zenith_Maths::Vector2(fHistLeft + fZoneWidth * 2.3f, fLabelY);
		xEntry.m_fSize = fLabelSize;
		xEntry.m_xColor = xGray;
		xTextEntries.PushBack(xEntry);
	}
}

void Flux_HDR::RenderBloom()
{
	BloomConstants xBloomConsts;
	xBloomConsts.m_fThreshold = s_fBloomThreshold;
	xBloomConsts.m_fIntensity = s_fBloomIntensity;

	// Bloom threshold pass - extract bright areas from HDR scene to bloom[0]
	{
		g_xBloomThresholdCommandList.Reset(true);
		xBloomConsts.m_xTexelSize = Zenith_Maths::Vector2(1.0f / s_axBloomChain[0].m_xSurfaceInfo.m_uWidth, 1.0f / s_axBloomChain[0].m_xSurfaceInfo.m_uHeight);

		g_xBloomThresholdCommandList.AddCommand<Flux_CommandSetPipeline>(&s_xBloomThresholdPipeline);
		g_xBloomThresholdCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
		g_xBloomThresholdCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

		Flux_ShaderBinder xBinder(g_xBloomThresholdCommandList);
		xBinder.BindSRV(s_xBloomThresholdHDRTexBinding, &s_xHDRSceneTarget.m_pxSRV);
		xBinder.PushConstant(s_xBloomThresholdConstantsBinding, &xBloomConsts, sizeof(BloomConstants));

		g_xBloomThresholdCommandList.AddCommand<Flux_CommandDrawIndexed>(6);
		Flux::SubmitCommandList(&g_xBloomThresholdCommandList, s_axBloomChainSetup[0], RENDER_ORDER_HDR_BLOOM_THRESHOLD);
	}

	// Downsample chain - progressive blur to smaller mips
	// Each pass reads from previous level and writes to current level
	// Separate render order ensures proper layout transitions between passes
	for (u_int i = 1; i < 5; i++)
	{
		Flux_CommandList& xCmdList = g_axBloomDownsampleCommandLists[i - 1];
		xCmdList.Reset(true);
		xBloomConsts.m_xTexelSize = Zenith_Maths::Vector2(1.0f / s_axBloomChain[i].m_xSurfaceInfo.m_uWidth, 1.0f / s_axBloomChain[i].m_xSurfaceInfo.m_uHeight);

		xCmdList.AddCommand<Flux_CommandSetPipeline>(&s_xBloomDownsamplePipeline);
		xCmdList.AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
		xCmdList.AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

		Flux_ShaderBinder xBinder(xCmdList);
		xBinder.BindSRV(s_xBloomDownsampleSourceBinding, &s_axBloomChain[i - 1].m_pxSRV);
		xBinder.PushConstant(s_xBloomDownsampleConstantsBinding, &xBloomConsts, sizeof(BloomConstants));

		xCmdList.AddCommand<Flux_CommandDrawIndexed>(6);
		// Use sub-order within downsample phase to ensure correct execution order
		Flux::SubmitCommandList(&xCmdList, s_axBloomChainSetup[i], RENDER_ORDER_HDR_BLOOM_DOWNSAMPLE, i);
	}

	// Upsample chain (additive blending) - accumulate bloom back up the mip chain
	// Iterates from smallest mip to largest: reads mip[4]->writes mip[3], reads mip[3]->writes mip[2], etc.
	// Each pass uses a unique sub-order within RENDER_ORDER_HDR_BLOOM_UPSAMPLE
	// to ensure correct execution order. Layout transitions between passes are
	// handled by the render target system (ColorAttachment <-> ShaderReadOnly).
	for (u_int i = 0; i < 4; i++)
	{
		u_int uTargetMip = 3 - i;  // Write destination: mip 3, 2, 1, 0
		u_int uSourceMip = uTargetMip + 1;  // Read source: mip 4, 3, 2, 1

		Flux_CommandList& xCmdList = g_axBloomUpsampleCommandLists[i];
		xCmdList.Reset(false);  // Don't clear - we're additively blending
		xBloomConsts.m_xTexelSize = Zenith_Maths::Vector2(1.0f / s_axBloomChain[uTargetMip].m_xSurfaceInfo.m_uWidth, 1.0f / s_axBloomChain[uTargetMip].m_xSurfaceInfo.m_uHeight);

		xCmdList.AddCommand<Flux_CommandSetPipeline>(&s_xBloomUpsamplePipeline);
		xCmdList.AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
		xCmdList.AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

		Flux_ShaderBinder xBinder(xCmdList);
		xBinder.BindSRV(s_xBloomUpsampleSourceBinding, &s_axBloomChain[uSourceMip].m_pxSRV);
		xBinder.PushConstant(s_xBloomUpsampleConstantsBinding, &xBloomConsts, sizeof(BloomConstants));

		xCmdList.AddCommand<Flux_CommandDrawIndexed>(6);
		Flux::SubmitCommandList(&xCmdList, s_axBloomChainSetup[uTargetMip], RENDER_ORDER_HDR_BLOOM_UPSAMPLE, i);
	}
}

void Flux_HDR::RenderToneMapping()
{
	// Reset with clear=true since this is the first pass rendering to the final target
	// Using clear ensures correct layout transition from eUndefined on first frame
	// (content is overwritten by fullscreen quad anyway, so clearing has no visual effect)
	g_xToneMappingCommandList.Reset(true);

	ToneMappingConstants xConsts;
	xConsts.m_fExposure = s_fExposure;  // Manual exposure (used when auto-exposure disabled)
	xConsts.m_fBloomIntensity = s_bBloomEnabled ? s_fBloomIntensity : 0.0f;
	xConsts.m_uToneMappingOperator = static_cast<u_int>(s_eToneMappingOperator);
	xConsts.m_uDebugMode = dbg_uHDRDebugMode;
	xConsts.m_bShowHistogram = dbg_bHDRShowHistogram ? 1 : 0;
	xConsts.m_bAutoExposure = s_bAutoExposure ? 1 : 0;
	xConsts.m_uPad0 = 0;
	xConsts.m_uPad1 = 0;

	g_xToneMappingCommandList.AddCommand<Flux_CommandSetPipeline>(&s_xToneMappingPipeline);
	g_xToneMappingCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	g_xToneMappingCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

	{
		Flux_ShaderBinder xBinder(g_xToneMappingCommandList);
		xBinder.BindSRV(s_xToneMappingHDRTexBinding, &s_xHDRSceneTarget.m_pxSRV);
		xBinder.BindSRV(s_xToneMappingBloomTexBinding, &s_axBloomChain[0].m_pxSRV);
		xBinder.BindUAV_Buffer(s_xToneMappingHistogramBinding, &s_xHistogramBuffer.GetUAV());
		xBinder.BindUAV_Buffer(s_xToneMappingExposureBinding, &s_xExposureBuffer.GetUAV());
		xBinder.PushConstant(s_xToneMappingConstantsBinding, &xConsts, sizeof(ToneMappingConstants));
	}

	g_xToneMappingCommandList.AddCommand<Flux_CommandDrawIndexed>(6);

	Flux::SubmitCommandList(&g_xToneMappingCommandList, Flux_Graphics::s_xFinalRenderTarget_NoDepth, RENDER_ORDER_HDR_TONEMAP);

	// Submit histogram labels if ShowHistogram is enabled
	if (dbg_bHDRShowHistogram)
	{
		SubmitHistogramLabels();
	}
}

void Flux_HDR::SubmitRenderTask()
{
	Zenith_TaskSystem::SubmitTask(&g_xRenderTask);
}

void Flux_HDR::WaitForRenderTask()
{
	g_xRenderTask.WaitUntilComplete();
}

Flux_ShaderResourceView& Flux_HDR::GetHDRSceneSRV()
{
	return s_xHDRSceneTarget.m_pxSRV;
}

Flux_RenderAttachment& Flux_HDR::GetHDRSceneTarget()
{
	return s_xHDRSceneTarget;
}

Flux_TargetSetup& Flux_HDR::GetHDRSceneTargetSetup()
{
	return s_xHDRSceneTargetSetup;
}

Flux_TargetSetup& Flux_HDR::GetHDRSceneTargetSetupWithDepth()
{
	return s_xHDRSceneTargetSetupWithDepth;
}

void Flux_HDR::SetToneMappingOperator(ToneMappingOperator eOperator)
{
	// Validate operator is within valid range
	if (eOperator >= TONEMAPPING_COUNT)
	{
		Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_HDR: Invalid tone mapping operator %u, defaulting to ACES", static_cast<u_int>(eOperator));
		eOperator = TONEMAPPING_ACES;
	}
	s_eToneMappingOperator = eOperator;
}

void Flux_HDR::SetExposure(float fExposure)
{
	s_fExposure = std::clamp(fExposure, 0.01f, 100.0f);
}

void Flux_HDR::SetBloomEnabled(bool bEnabled)
{
	s_bBloomEnabled = bEnabled;
}

void Flux_HDR::SetBloomIntensity(float fIntensity)
{
	s_fBloomIntensity = std::clamp(fIntensity, 0.0f, 10.0f);
}

void Flux_HDR::SetBloomThreshold(float fThreshold)
{
	s_fBloomThreshold = std::clamp(fThreshold, 0.0f, 100.0f);
}

float Flux_HDR::GetCurrentExposure()
{
	return s_fCurrentExposure;
}

float Flux_HDR::GetAverageLuminance()
{
	return s_fAverageLuminance;
}

void Flux_HDR::SetAutoExposureEnabled(bool bEnabled)
{
	s_bAutoExposure = bEnabled;
}

void Flux_HDR::SetAdaptationSpeed(float fSpeed)
{
	s_fAdaptationSpeed = fSpeed;
}

void Flux_HDR::SetTargetLuminance(float fLuminance)
{
	s_fTargetLuminance = fLuminance;
}

void Flux_HDR::SetExposureRange(float fMin, float fMax)
{
	s_fMinExposure = fMin;
	s_fMaxExposure = fMax;
}

bool Flux_HDR::IsAutoExposureEnabled()
{
	return s_bAutoExposure;
}

float Flux_HDR::GetAdaptationSpeed()
{
	return s_fAdaptationSpeed;
}

float Flux_HDR::GetTargetLuminance()
{
	return s_fTargetLuminance;
}

bool Flux_HDR::IsEnabled()
{
	// HDR pipeline is always active (tone mapping always runs)
	// This returns true if any HDR post-processing features are enabled
	return dbg_bHDRAutoExposure || dbg_bHDRBloomEnabled;
}

#ifdef ZENITH_TOOLS
void Flux_HDR::RegisterDebugVariables()
{
	Zenith_DebugVariables::AddUInt32({ "Flux", "HDR", "DebugMode" }, dbg_uHDRDebugMode, 0, HDR_DEBUG_COUNT - 1);
	Zenith_DebugVariables::AddFloat({ "Flux", "HDR", "Exposure" }, dbg_fHDRExposure, 0.01f, 10.0f);
	Zenith_DebugVariables::AddBoolean({ "Flux", "HDR", "AutoExposure" }, dbg_bHDRAutoExposure);
	Zenith_DebugVariables::AddBoolean({ "Flux", "HDR", "BloomEnabled" }, dbg_bHDRBloomEnabled);
	Zenith_DebugVariables::AddFloat({ "Flux", "HDR", "BloomIntensity" }, dbg_fHDRBloomIntensity, 0.0f, 2.0f);
	Zenith_DebugVariables::AddFloat({ "Flux", "HDR", "BloomThreshold" }, dbg_fHDRBloomThreshold, 0.0f, 5.0f);
	Zenith_DebugVariables::AddUInt32({ "Flux", "HDR", "ToneMappingOperator" }, dbg_uHDRToneMappingOperator, 0, TONEMAPPING_COUNT - 1);
	Zenith_DebugVariables::AddBoolean({ "Flux", "HDR", "ShowHistogram" }, dbg_bHDRShowHistogram);
	Zenith_DebugVariables::AddBoolean({ "Flux", "HDR", "FreezeExposure" }, dbg_bHDRFreezeExposure);
	Zenith_DebugVariables::AddFloat({ "Flux", "HDR", "AdaptationSpeed" }, dbg_fHDRAdaptationSpeed, 0.1f, 10.0f);
	Zenith_DebugVariables::AddFloat({ "Flux", "HDR", "TargetLuminance" }, dbg_fHDRTargetLuminance, 0.01f, 1.0f);
	Zenith_DebugVariables::AddFloat({ "Flux", "HDR", "MinExposure" }, dbg_fHDRMinExposure, 0.01f, 1.0f);
	Zenith_DebugVariables::AddFloat({ "Flux", "HDR", "MaxExposure" }, dbg_fHDRMaxExposure, 1.0f, 20.0f);

	Zenith_DebugVariables::AddTexture({ "Flux", "HDR", "Textures", "HDRScene" }, s_xHDRSceneTarget.m_pxSRV);
	Zenith_DebugVariables::AddTexture({ "Flux", "HDR", "Textures", "BloomMip0" }, s_axBloomChain[0].m_pxSRV);
	Zenith_DebugVariables::AddTexture({ "Flux", "HDR", "Textures", "BloomMip1" }, s_axBloomChain[1].m_pxSRV);
	Zenith_DebugVariables::AddTexture({ "Flux", "HDR", "Textures", "BloomMip2" }, s_axBloomChain[2].m_pxSRV);
}
#endif
