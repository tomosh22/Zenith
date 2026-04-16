#include "Zenith.h"

#include <atomic>
#include "Flux_HDR.h"
#include "Flux/Flux.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Flux_CommandList.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Zenith_PlatformGraphics_Include.h"
#include "Core/Zenith_Core.h"
#include "UI/Zenith_UICanvas.h"

#ifdef ZENITH_TOOLS
#include "DebugVariables/Zenith_DebugVariables.h"
#endif

// Strongly-typed per-pass user data: each downsample / upsample pass is given
// a pointer into one of these arrays so the execute callback recovers the mip
// index without going through a void*-as-int reinterpret_cast.
static u_int s_axBloomMipUserData[5]      = { 0, 1, 2, 3, 4 };
static u_int s_axBloomUpsampleUserData[4] = { 0, 1, 2, 3 };

// Shaders
static Flux_Shader s_xToneMappingShader;
static Flux_Shader s_xBloomThresholdShader;
static Flux_Shader s_xBloomDownsampleShader;
static Flux_Shader s_xBloomUpsampleShader;

// ---- Transient path (graph-owned allocation) for bloom chain ----
static Flux_TransientHandle s_axBloomChainHandles[5];
static Flux_RenderGraph* s_pxGraph = nullptr;

// ---- Toggle: which path is active this frame ----
static bool s_bUsingTransients = false;

// Bloom chain format (constant - pipeline building doesn't depend on allocation path)
static constexpr TextureFormat BLOOM_FORMAT = TEXTURE_FORMAT_R16G16B16A16_SFLOAT;

// Static member definitions
Flux_RenderAttachment Flux_HDR::s_xHDRSceneTarget_Owned;
Flux_TransientHandle Flux_HDR::s_xHDRSceneTargetHandle;

Flux_RenderAttachment Flux_HDR::s_axBloomChain[5];

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
Flux_Pipeline Flux_HDR::s_xLuminanceHistogramPipeline;
Flux_Pipeline Flux_HDR::s_xAdaptationPipeline;
Flux_Shader Flux_HDR::s_xLuminanceHistogramShader;
Flux_Shader Flux_HDR::s_xAdaptationShader;
Flux_RootSig Flux_HDR::s_xLuminanceRootSig;
Flux_RootSig Flux_HDR::s_xAdaptationRootSig;

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
		"HDR/Flux_ToneMapping.frag", FINAL_RT_FORMAT);

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

	// Initialize bloom threshold shader and pipeline (use constant format)
	Flux_PipelineHelper::BuildFullscreenPipeline(
		s_xBloomThresholdShader, s_xBloomThresholdPipeline,
		"HDR/Flux_BloomThreshold.frag", BLOOM_FORMAT);

	// Cache binding handles from reflection and validate
	const Flux_ShaderReflection& xThresholdReflection = s_xBloomThresholdShader.GetReflection();
	s_xBloomThresholdHDRTexBinding = xThresholdReflection.GetBinding("g_xHDRTex");
	s_xBloomThresholdConstantsBinding = xThresholdReflection.GetBinding("BloomConstants");

	if (!s_xBloomThresholdHDRTexBinding.IsValid())
		Zenith_Error(LOG_CATEGORY_RENDERER, "Flux_HDR: g_xHDRTex binding not found in bloom threshold shader");
	if (!s_xBloomThresholdConstantsBinding.IsValid())
		Zenith_Error(LOG_CATEGORY_RENDERER, "Flux_HDR: BloomConstants binding not found in bloom threshold shader");

	// Initialize bloom downsample shader and pipeline (use constant format)
	Flux_PipelineHelper::BuildFullscreenPipeline(
		s_xBloomDownsampleShader, s_xBloomDownsamplePipeline,
		"HDR/Flux_BloomDownsample.frag", BLOOM_FORMAT);

	// Cache binding handles from reflection and validate
	const Flux_ShaderReflection& xDownsampleReflection = s_xBloomDownsampleShader.GetReflection();
	s_xBloomDownsampleSourceBinding = xDownsampleReflection.GetBinding("g_xSourceTex");
	s_xBloomDownsampleConstantsBinding = xDownsampleReflection.GetBinding("BloomConstants");

	if (!s_xBloomDownsampleSourceBinding.IsValid())
		Zenith_Error(LOG_CATEGORY_RENDERER, "Flux_HDR: g_xSourceTex binding not found in bloom downsample shader");
	if (!s_xBloomDownsampleConstantsBinding.IsValid())
		Zenith_Error(LOG_CATEGORY_RENDERER, "Flux_HDR: BloomConstants binding not found in bloom downsample shader");

	// Initialize bloom upsample shader and pipeline (additive blending, use constant format)
	{
		Flux_PipelineSpecification xSpec = Flux_PipelineHelper::CreateFullscreenSpec(
			s_xBloomUpsampleShader, "HDR/Flux_BloomUpsample.frag", BLOOM_FORMAT);
		xSpec.m_axBlendStates[0].m_bBlendEnabled = true;
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
	s_pxGraph = nullptr;

	// Cleanup auto-exposure compute resources
	Flux_MemoryManager::DestroyReadWriteBuffer(s_xHistogramBuffer);
	Flux_MemoryManager::DestroyReadWriteBuffer(s_xExposureBuffer);

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_HDR shut down");
}

void Flux_HDR::Reset()
{
	s_fCurrentExposure = 1.0f;
	s_fAverageLuminance = 0.18f;

	// Clear histogram buffer to prevent stale data after scene transitions
	// This ensures auto-exposure starts fresh when enabled
	if (s_xHistogramBuffer.GetBuffer().m_xVRAMHandle.IsValid())
	{
		static u_int auZeroHistogram[256] = { 0 };
		Flux_MemoryManager::UploadBufferData(
			s_xHistogramBuffer.GetBuffer().m_xVRAMHandle,
			auZeroHistogram,
			256 * sizeof(u_int));
	}

	// Reset exposure buffer to default values
	if (s_xExposureBuffer.GetBuffer().m_xVRAMHandle.IsValid())
	{
		float afInitialExposure[4] = { 0.18f, 1.0f, 1.0f, 0.0f };
		Flux_MemoryManager::UploadBufferData(
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
	xBuilder.m_eFormat = HDR_SCENE_FORMAT;

	xBuilder.BuildColour(s_xHDRSceneTarget_Owned, "Flux HDR Scene Target");

	u_int uBloomWidth = Flux_Swapchain::GetWidth() / 2;
	u_int uBloomHeight = Flux_Swapchain::GetHeight() / 2;

	for (u_int i = 0; i < 5; i++)
	{
		xBuilder.m_uWidth = uBloomWidth;
		xBuilder.m_uHeight = uBloomHeight;
		xBuilder.BuildColour(s_axBloomChain[i], "Flux Bloom Chain " + std::to_string(i));

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
			Flux_VRAM* pxVRAM = Flux_PlatformAPI::GetVRAM(xAttachment.m_xVRAMHandle);
			Flux_MemoryManager::QueueVRAMDeletion(pxVRAM, xAttachment.m_xVRAMHandle,
				xAttachment.RTV().m_xImageViewHandle, xAttachment.DSV().m_xImageViewHandle,
				xAttachment.SRV().m_xImageViewHandle, xAttachment.UAV(0).m_xImageViewHandle);
		}
	};

	DestroyAttachment(s_xHDRSceneTarget_Owned);

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
	Flux_MemoryManager::InitialiseReadWriteBuffer(auZeroHistogram, 256 * sizeof(u_int), Flux_HDR::s_xHistogramBuffer);

	// Validate histogram buffer creation
	if (!Flux_HDR::s_xHistogramBuffer.GetBuffer().m_xVRAMHandle.IsValid())
	{
		Zenith_Error(LOG_CATEGORY_RENDERER, "Flux_HDR: Failed to create histogram buffer");
		return;
	}

	// Create exposure buffer (4 floats: avgLum, currentExp, targetExp, pad)
	// Initialize with default values
	float afInitialExposure[4] = { 0.18f, 1.0f, 1.0f, 0.0f };
	Flux_MemoryManager::InitialiseReadWriteBuffer(afInitialExposure, 4 * sizeof(float), Flux_HDR::s_xExposureBuffer);

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
	Flux_RootSigBuilder::FromReflection(Flux_HDR::s_xLuminanceRootSig, xLuminanceReflection);

	// Cache binding handles from reflection for use at render time
	s_xLuminanceConstantsBinding = xLuminanceReflection.GetBinding("LuminanceConstants");
	s_xLuminanceHDRTexBinding = xLuminanceReflection.GetBinding("g_xHDRTex");
	s_xLuminanceHistogramBinding = xLuminanceReflection.GetBinding("g_auHistogram");

	// Build luminance histogram pipeline
	Flux_ComputePipelineBuilder xLuminanceBuilder;
	xLuminanceBuilder.WithShader(Flux_HDR::s_xLuminanceHistogramShader)
		.WithLayout(Flux_HDR::s_xLuminanceRootSig.m_xLayout)
		.Build(Flux_HDR::s_xLuminanceHistogramPipeline);
	Flux_HDR::s_xLuminanceHistogramPipeline.m_xRootSig = Flux_HDR::s_xLuminanceRootSig;

	// Initialize adaptation compute shader
	Flux_HDR::s_xAdaptationShader.InitialiseCompute("HDR/Flux_Adaptation.comp");

	// Build adaptation root signature from shader reflection
	const Flux_ShaderReflection& xAdaptationReflection = Flux_HDR::s_xAdaptationShader.GetReflection();
	Flux_RootSigBuilder::FromReflection(Flux_HDR::s_xAdaptationRootSig, xAdaptationReflection);

	// Cache binding handles from reflection for use at render time
	s_xAdaptationConstantsBinding = xAdaptationReflection.GetBinding("AdaptationConstants");
	s_xAdaptationHistogramBinding = xAdaptationReflection.GetBinding("g_auHistogram");
	s_xAdaptationExposureBinding = xAdaptationReflection.GetBinding("g_afExposureData");

	// Build adaptation pipeline
	Flux_ComputePipelineBuilder xAdaptationBuilder;
	xAdaptationBuilder.WithShader(Flux_HDR::s_xAdaptationShader)
		.WithLayout(Flux_HDR::s_xAdaptationRootSig.m_xLayout)
		.Build(Flux_HDR::s_xAdaptationPipeline);
	Flux_HDR::s_xAdaptationPipeline.m_xRootSig = Flux_HDR::s_xAdaptationRootSig;

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_HDR: Auto-exposure compute pipelines initialized");
}

void Flux_HDR::PreExecuteLuminanceHistogram(void* pUserData)
{
	(void)pUserData;

	// SYNC NOTE: UploadBufferData() below stages into the memory transfer
	// command buffer. EndFrame() submits that CB and signals s_xMemorySemaphore,
	// which the subsequent render submission waits on, so any uploads issued
	// from a Phase-0 OnPrepare callback are guaranteed to be visible before any
	// pass that reads them. This replaces the old RENDER_ORDER_MEMORY_UPDATE
	// invariant that the static_assert in EndFrame used to enforce.
	SyncDebugVariables();
	bool bAutoExposureJustEnabled = s_bAutoExposure && !s_bAutoExposureWasEnabled;
	s_bAutoExposureWasEnabled = s_bAutoExposure;
	if (bAutoExposureJustEnabled && s_xExposureBuffer.GetBuffer().m_xVRAMHandle.IsValid())
	{
		float afInitialExposure[4] = { 0.18f, 1.0f, 1.0f, 0.0f };
		Flux_MemoryManager::UploadBufferData(
			s_xExposureBuffer.GetBuffer().m_xVRAMHandle,
			afInitialExposure,
			4 * sizeof(float));
	}

	// Clear histogram buffer before compute (only when auto-exposure or histogram visualization is active)
	if ((s_bAutoExposure || dbg_bHDRShowHistogram) && s_xHistogramBuffer.GetBuffer().m_xVRAMHandle.IsValid())
	{
		static u_int auZeroHistogram[256] = { 0 };
		Flux_MemoryManager::UploadBufferData(
			s_xHistogramBuffer.GetBuffer().m_xVRAMHandle,
			auZeroHistogram,
			256 * sizeof(u_int));
	}
}

void Flux_HDR::ExecuteLuminanceHistogram(Flux_CommandList* pxCommandList, void* pUserData)
{
	(void)pUserData;

	if (!s_xHistogramBuffer.GetBuffer().m_xVRAMHandle.IsValid())
	{
		return;
	}

	// Buffer uploads are handled in PreExecuteLuminanceHistogram

	pxCommandList->AddCommand<Flux_CommandBindComputePipeline>(&s_xLuminanceHistogramPipeline);

	LuminanceConstants xConsts;
	xConsts.m_uImageWidth = Flux_Swapchain::GetWidth();
	xConsts.m_uImageHeight = Flux_Swapchain::GetHeight();
	xConsts.m_fMinLogLum = s_fMinLogLuminance;
	xConsts.m_fLogLumRange = s_fLogLuminanceRange;

	Flux_ShaderBinder xBinder(*pxCommandList);
	xBinder.BindDrawConstants(s_xLuminanceConstantsBinding, &xConsts, sizeof(xConsts));
	xBinder.BindSRV(s_xLuminanceHDRTexBinding, &GetHDRSceneTarget().SRV());
	xBinder.BindUAV_Buffer(s_xLuminanceHistogramBinding, &s_xHistogramBuffer.GetUAV());

	u_int uGroupsX = (Flux_Swapchain::GetWidth() + 15) / 16;
	u_int uGroupsY = (Flux_Swapchain::GetHeight() + 15) / 16;
	pxCommandList->AddCommand<Flux_CommandDispatch>(uGroupsX, uGroupsY, 1);
}

void Flux_HDR::ExecuteAdaptation(Flux_CommandList* pxCommandList, void* pUserData)
{
	(void)pUserData;
	if (!s_xHistogramBuffer.GetBuffer().m_xVRAMHandle.IsValid() ||
		!s_xExposureBuffer.GetBuffer().m_xVRAMHandle.IsValid())
	{
		return;
	}

	pxCommandList->AddCommand<Flux_CommandBindComputePipeline>(&s_xAdaptationPipeline);

	AdaptationConstants xConsts;
	xConsts.m_fMinLogLum = s_fMinLogLuminance;
	xConsts.m_fLogLumRange = s_fLogLuminanceRange;
	xConsts.m_fDeltaTime = Zenith_Core::GetDt();
	xConsts.m_fAdaptationSpeed = dbg_bHDRFreezeExposure ? 0.0f : s_fAdaptationSpeed;
	xConsts.m_fTargetLuminance = s_fTargetLuminance;
	xConsts.m_fMinExposure = s_fMinExposure;
	xConsts.m_fMaxExposure = s_fMaxExposure;
	xConsts.m_fLowPercentile = 0.05f;
	xConsts.m_fHighPercentile = 0.95f;
	xConsts.m_uTotalPixels = Flux_Swapchain::GetWidth() * Flux_Swapchain::GetHeight();
	xConsts.m_uPad0 = 0;
	xConsts.m_uPad1 = 0;

	Flux_ShaderBinder xBinder(*pxCommandList);
	xBinder.BindDrawConstants(s_xAdaptationConstantsBinding, &xConsts, sizeof(xConsts));
	xBinder.BindUAV_Buffer(s_xAdaptationHistogramBinding, &s_xHistogramBuffer.GetUAV());
	xBinder.BindUAV_Buffer(s_xAdaptationExposureBinding, &s_xExposureBuffer.GetUAV());

	pxCommandList->AddCommand<Flux_CommandDispatch>(1, 1, 1);
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

void Flux_HDR::ExecuteBloomThreshold(Flux_CommandList* pxCommandList, void* pUserData)
{
	(void)pUserData;
	Flux_RenderAttachment& xBloom0 = GetBloomChainAttachment(0);
	BloomConstants xBloomConsts;
	xBloomConsts.m_fThreshold = s_fBloomThreshold;
	xBloomConsts.m_fIntensity = s_fBloomIntensity;
	xBloomConsts.m_xTexelSize = Zenith_Maths::Vector2(1.0f / xBloom0.m_xSurfaceInfo.m_uWidth, 1.0f / xBloom0.m_xSurfaceInfo.m_uHeight);

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&s_xBloomThresholdPipeline);
	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);
	xBinder.BindSRV(s_xBloomThresholdHDRTexBinding, &GetHDRSceneTarget().SRV());
	xBinder.BindDrawConstants(s_xBloomThresholdConstantsBinding, &xBloomConsts, sizeof(BloomConstants));

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
}

void Flux_HDR::ExecuteBloomDownsample(Flux_CommandList* pxCommandList, void* pUserData)
{
	const u_int uMipIndex = *static_cast<const u_int*>(pUserData);

	Flux_RenderAttachment& xTarget = GetBloomChainAttachment(uMipIndex);
	BloomConstants xBloomConsts;
	xBloomConsts.m_fThreshold = s_fBloomThreshold;
	xBloomConsts.m_fIntensity = s_fBloomIntensity;
	xBloomConsts.m_xTexelSize = Zenith_Maths::Vector2(1.0f / xTarget.m_xSurfaceInfo.m_uWidth, 1.0f / xTarget.m_xSurfaceInfo.m_uHeight);

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&s_xBloomDownsamplePipeline);
	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);
	xBinder.BindSRV(s_xBloomDownsampleSourceBinding, &GetBloomChainAttachment(uMipIndex - 1).SRV());
	xBinder.BindDrawConstants(s_xBloomDownsampleConstantsBinding, &xBloomConsts, sizeof(BloomConstants));

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
}

void Flux_HDR::ExecuteBloomUpsample(Flux_CommandList* pxCommandList, void* pUserData)
{
	const u_int uIndex = *static_cast<const u_int*>(pUserData);
	const u_int uTargetMip = 3 - uIndex;
	const u_int uSourceMip = uTargetMip + 1;

	Flux_RenderAttachment& xTarget = GetBloomChainAttachment(uTargetMip);
	BloomConstants xBloomConsts;
	xBloomConsts.m_fThreshold = s_fBloomThreshold;
	xBloomConsts.m_fIntensity = s_fBloomIntensity;
	xBloomConsts.m_xTexelSize = Zenith_Maths::Vector2(1.0f / xTarget.m_xSurfaceInfo.m_uWidth, 1.0f / xTarget.m_xSurfaceInfo.m_uHeight);

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&s_xBloomUpsamplePipeline);
	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);
	xBinder.BindSRV(s_xBloomUpsampleSourceBinding, &GetBloomChainAttachment(uSourceMip).SRV());
	xBinder.BindDrawConstants(s_xBloomUpsampleConstantsBinding, &xBloomConsts, sizeof(BloomConstants));

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
}

void Flux_HDR::ExecuteToneMapping(Flux_CommandList* pxCommandList, void* pUserData)
{
	(void)pUserData;
	ToneMappingConstants xConsts;
	xConsts.m_fExposure = s_fExposure;
	xConsts.m_fBloomIntensity = s_bBloomEnabled ? s_fBloomIntensity : 0.0f;
	xConsts.m_uToneMappingOperator = static_cast<u_int>(s_eToneMappingOperator);
	xConsts.m_uDebugMode = dbg_uHDRDebugMode;
	xConsts.m_bShowHistogram = dbg_bHDRShowHistogram ? 1 : 0;
	xConsts.m_bAutoExposure = s_bAutoExposure ? 1 : 0;
	xConsts.m_uPad0 = 0;
	xConsts.m_uPad1 = 0;

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&s_xToneMappingPipeline);
	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

	{
		Flux_ShaderBinder xBinder(*pxCommandList);
	xBinder.BindSRV(s_xToneMappingHDRTexBinding, &GetHDRSceneTarget().SRV());
	xBinder.BindSRV(s_xToneMappingBloomTexBinding, &GetBloomChainAttachment(0).SRV());
		xBinder.BindUAV_Buffer(s_xToneMappingHistogramBinding, &s_xHistogramBuffer.GetUAV());
		xBinder.BindUAV_Buffer(s_xToneMappingExposureBinding, &s_xExposureBuffer.GetUAV());
		xBinder.BindDrawConstants(s_xToneMappingConstantsBinding, &xConsts, sizeof(ToneMappingConstants));
	}

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);

	// Submit histogram labels if ShowHistogram is enabled
	if (dbg_bHDRShowHistogram)
	{
		SubmitHistogramLabels();
	}
}

void Flux_HDR::SetupTransients(Flux_RenderGraph& xGraph)
{
	s_pxGraph = &xGraph;
	s_bUsingTransients = xGraph.AreTransientsEnabled();

	// Create transient HDR scene target early — many subsystems write to
	// GetHDRSceneTarget() during their SetupRenderGraph, so the handle must
	// exist before they run.
	if (s_bUsingTransients)
	{
		Flux_TransientTextureDesc xHDRDesc;
		xHDRDesc.m_uWidth = Flux_Swapchain::GetWidth();
		xHDRDesc.m_uHeight = Flux_Swapchain::GetHeight();
		xHDRDesc.m_eFormat = HDR_SCENE_FORMAT;
		xHDRDesc.m_uMemoryFlags = (1u << MEMORY_FLAGS__SHADER_READ);
		s_xHDRSceneTargetHandle = xGraph.CreateTransient(xHDRDesc);
	}
}

void Flux_HDR::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	// s_pxGraph and s_bUsingTransients already set by SetupTransients().
	// HDR scene target transient already created there too.

	if (s_bUsingTransients)
	{
		u_int uBloomWidth = Flux_Swapchain::GetWidth() / 2;
		u_int uBloomHeight = Flux_Swapchain::GetHeight() / 2;

		for (u_int i = 0; i < 5; i++)
		{
			Flux_TransientTextureDesc xDesc;
			xDesc.m_uWidth = uBloomWidth;
			xDesc.m_uHeight = uBloomHeight;
			xDesc.m_eFormat = BLOOM_FORMAT;
			xDesc.m_uMemoryFlags = (1u << MEMORY_FLAGS__SHADER_READ);
			s_axBloomChainHandles[i] = xGraph.CreateTransient(xDesc);

			uBloomWidth = std::max(1u, uBloomWidth / 2);
			uBloomHeight = std::max(1u, uBloomHeight / 2);
		}
	}

	u_int uHistogramPass = UINT32_MAX;
	{
		uHistogramPass = xGraph.AddPass("HDR_LuminanceHistogram", ExecuteLuminanceHistogram);
		xGraph.SetPrepare(uHistogramPass, PreExecuteLuminanceHistogram);
		xGraph.Read(uHistogramPass, GetHDRSceneTarget(), RESOURCE_ACCESS_READ_SRV);
		xGraph.WriteBuffer(uHistogramPass, s_xHistogramBuffer.GetBuffer(), RESOURCE_ACCESS_WRITE_UAV);
	}

	{
		u_int uPass = xGraph.AddPass("HDR_Adaptation", ExecuteAdaptation);
		// Histogram is bound as UAV_Buffer (see ExecuteAdaptation); declare as
		// READWRITE_UAV so the graph barrier matches the actual compute-stage access.
		xGraph.ReadBuffer(uPass, s_xHistogramBuffer.GetBuffer(), RESOURCE_ACCESS_READWRITE_UAV);
		xGraph.WriteBuffer(uPass, s_xExposureBuffer.GetBuffer(), RESOURCE_ACCESS_WRITE_UAV);
	}

	{
		u_int uPass = xGraph.AddPass("HDR_BloomThreshold", ExecuteBloomThreshold);
		xGraph.SetClear(uPass, true);
		xGraph.Read(uPass, GetHDRSceneTarget(), RESOURCE_ACCESS_READ_SRV);

		if (s_bUsingTransients)
			xGraph.WriteTransient(uPass, s_axBloomChainHandles[0], RESOURCE_ACCESS_WRITE_RTV);
		else
			xGraph.Write(uPass, s_axBloomChain[0], RESOURCE_ACCESS_WRITE_RTV);
	}

	static const char* s_aszBloomDownsampleNames[] = {
		"HDR_BloomDownsample Mip1", "HDR_BloomDownsample Mip2",
		"HDR_BloomDownsample Mip3", "HDR_BloomDownsample Mip4",
	};
	for (u_int i = 1; i < 5; i++)
	{
		u_int uPass = xGraph.AddPass(s_aszBloomDownsampleNames[i - 1], ExecuteBloomDownsample, &s_axBloomMipUserData[i]);
		xGraph.SetClear(uPass, true);

		if (s_bUsingTransients)
		{
			xGraph.ReadTransient(uPass, s_axBloomChainHandles[i - 1], RESOURCE_ACCESS_READ_SRV);
			xGraph.WriteTransient(uPass, s_axBloomChainHandles[i], RESOURCE_ACCESS_WRITE_RTV);
		}
		else
		{
			xGraph.Read(uPass, s_axBloomChain[i - 1], RESOURCE_ACCESS_READ_SRV);
			xGraph.Write(uPass, s_axBloomChain[i], RESOURCE_ACCESS_WRITE_RTV);
		}
	}

	static const char* s_aszBloomUpsampleNames[] = {
		"HDR_BloomUpsample Mip3", "HDR_BloomUpsample Mip2",
		"HDR_BloomUpsample Mip1", "HDR_BloomUpsample Mip0",
	};
	for (u_int i = 0; i < 4; i++)
	{
		u_int uTargetMip = 3 - i;
		u_int uSourceMip = uTargetMip + 1;

		u_int uPass = xGraph.AddPass(s_aszBloomUpsampleNames[i], ExecuteBloomUpsample, &s_axBloomUpsampleUserData[i]);

		if (s_bUsingTransients)
		{
			xGraph.ReadTransient(uPass, s_axBloomChainHandles[uSourceMip], RESOURCE_ACCESS_READ_SRV);
			xGraph.WriteTransient(uPass, s_axBloomChainHandles[uTargetMip], RESOURCE_ACCESS_WRITE_RTV);
		}
		else
		{
			xGraph.Read(uPass, s_axBloomChain[uSourceMip], RESOURCE_ACCESS_READ_SRV);
			xGraph.Write(uPass, s_axBloomChain[uTargetMip], RESOURCE_ACCESS_WRITE_RTV);
		}
	}

	{
		u_int uPass = xGraph.AddPass("HDR_ToneMapping", ExecuteToneMapping);
		xGraph.SetClear(uPass, true);
		xGraph.Read(uPass, GetHDRSceneTarget(), RESOURCE_ACCESS_READ_SRV);

		if (s_bUsingTransients)
			xGraph.ReadTransient(uPass, s_axBloomChainHandles[0], RESOURCE_ACCESS_READ_SRV);
		else
			xGraph.Read(uPass, s_axBloomChain[0], RESOURCE_ACCESS_READ_SRV);

		xGraph.Write(uPass, Flux_Graphics::GetFinalRenderTarget_NoDepth(), RESOURCE_ACCESS_WRITE_RTV);

		// Tone mapping also reads exposure (and optionally samples histogram for
		// debug overlay). Both are bound as UAV_Buffer in ExecuteToneMapping so
		// declare as READWRITE_UAV — the barrier must match the binding's access
		// mode rather than the shader's actual read-only intent.
		xGraph.ReadBuffer(uPass, s_xHistogramBuffer.GetBuffer(), RESOURCE_ACCESS_READWRITE_UAV);
		xGraph.ReadBuffer(uPass, s_xExposureBuffer.GetBuffer(), RESOURCE_ACCESS_READWRITE_UAV);
	}
}

Flux_ShaderResourceView& Flux_HDR::GetHDRSceneSRV()
{
	return GetHDRSceneTarget().SRV();
}

Flux_RenderAttachment& Flux_HDR::GetHDRSceneTarget()
{
	if (s_bUsingTransients)
	{
		Zenith_Assert(s_pxGraph, "Flux_HDR::GetHDRSceneTarget: graph pointer is null");
		return s_pxGraph->GetTransientAttachment(s_xHDRSceneTargetHandle);
	}
	return s_xHDRSceneTarget_Owned;
}

void Flux_HDR::GetHDRSceneTargetSetup(Flux_RenderAttachment* apxColourAttachments[], uint32_t& uNumColour, Flux_RenderAttachment*& pxDepthStencil)
{
	apxColourAttachments[0] = &GetHDRSceneTarget();
	uNumColour = 1;
	pxDepthStencil = nullptr;
}

void Flux_HDR::GetHDRSceneTargetSetupWithDepth(Flux_RenderAttachment* apxColourAttachments[], uint32_t& uNumColour, Flux_RenderAttachment*& pxDepthStencil)
{
	apxColourAttachments[0] = &GetHDRSceneTarget();
	uNumColour = 1;
	pxDepthStencil = &Flux_Graphics::GetDepthAttachment();
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

Flux_RenderAttachment& Flux_HDR::GetBloomChainAttachment(u_int uIndex)
{
	Zenith_Assert(uIndex < 5, "Flux_HDR::GetBloomChainAttachment: index %u out of range", uIndex);
	if (s_bUsingTransients)
		return s_pxGraph->GetTransientAttachment(s_axBloomChainHandles[uIndex]);
	return s_axBloomChain[uIndex];
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

	Zenith_DebugVariables::AddTexture({ "Flux", "HDR", "Textures", "HDRScene" }, s_xHDRSceneTarget_Owned.SRV());
	Zenith_DebugVariables::AddTexture({ "Flux", "HDR", "Textures", "BloomMip0" }, s_axBloomChain[0].SRV());
	Zenith_DebugVariables::AddTexture({ "Flux", "HDR", "Textures", "BloomMip1" }, s_axBloomChain[1].SRV());
	Zenith_DebugVariables::AddTexture({ "Flux", "HDR", "Textures", "BloomMip2" }, s_axBloomChain[2].SRV());
}
#endif
