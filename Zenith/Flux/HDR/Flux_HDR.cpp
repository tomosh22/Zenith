#include "Zenith.h"

#include <atomic>
#include "Flux/HDR/Flux_HDRImpl.h"
#include "Core/Zenith_Engine.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "UI/Zenith_UICanvas.h"
// Injected deps: VulkanMemory owns the auto-exposure VRAM buffers + uploads,
// the swapchain supplies the back-buffer dims, FrameContext supplies the
// per-frame delta-time. Method bodies need the full types.
#include "Vulkan/Zenith_Vulkan_MemoryManager.h"
#include "Vulkan/Zenith_Vulkan_Swapchain.h"
#include "Core/FrameContext.h"

#ifdef ZENITH_TOOLS
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Flux/Slang/Flux_ShaderHotReload.h"
#endif

// Phase 7h: subsystem state moved to Flux_HDRImpl held by Zenith_Engine.

// Strongly-typed per-pass user data: each downsample / upsample pass is given
// a pointer into one of these arrays so the execute callback recovers the mip
// index without going through a void*-as-int reinterpret_cast.
static u_int s_axBloomMipUserData[5]      = { 0, 1, 2, 3, 4 };
static u_int s_axBloomUpsampleUserData[4] = { 0, 1, 2, 3 };

// Bloom chain format.
static constexpr TextureFormat BLOOM_FORMAT = TEXTURE_FORMAT_R16G16B16A16_SFLOAT;

// Debug variables
DEBUGVAR u_int dbg_uHDRDebugMode = HDR_DEBUG_NONE;
DEBUGVAR float dbg_fHDRExposure = 1.0f;
DEBUGVAR float dbg_fHDRBloomIntensity = 0.5f;
DEBUGVAR float dbg_fHDRBloomThreshold = 1.0f;
DEBUGVAR u_int dbg_uHDRToneMappingOperator = TONEMAPPING_ACES;
DEBUGVAR bool dbg_bHDRShowHistogram = false;
DEBUGVAR bool dbg_bHDRFreezeExposure = false;
DEBUGVAR float dbg_fHDRAdaptationSpeed = 2.0f;
DEBUGVAR float dbg_fHDRTargetLuminance = 0.18f;
DEBUGVAR float dbg_fHDRMinExposure = 0.1f;
DEBUGVAR float dbg_fHDRMaxExposure = 10.0f;

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

void Flux_HDRImpl::SyncDebugVariables()
{
	m_fExposure = dbg_fHDRExposure;
	m_fBloomIntensity = dbg_fHDRBloomIntensity;
	m_fBloomThreshold = dbg_fHDRBloomThreshold;
	m_eToneMappingOperator = static_cast<ToneMappingOperator>(dbg_uHDRToneMappingOperator);
	m_fAdaptationSpeed = dbg_fHDRAdaptationSpeed;
	m_fTargetLuminance = dbg_fHDRTargetLuminance;
	m_fMinExposure = dbg_fHDRMinExposure;
	m_fMaxExposure = dbg_fHDRMaxExposure;
}

void Flux_HDRImpl::BuildPipelines()
{
	// Tone mapping — Slang-registered (HDR_ToneMapping). Five tone curves
	// + debug overlays in a single fragment program.
	Flux_PipelineHelper::BuildFullscreenPipeline(
		m_xToneMappingShader, m_xToneMappingPipeline,
		FluxShaderProgram::HDR_ToneMapping, FINAL_RT_FORMAT);

	// Bloom passes — all migrated to the Slang shader registry. They share
	// Common.Fullscreen for vsMain so a single .slang module per pass holds
	// both stages.
	Flux_PipelineHelper::BuildFullscreenPipeline(
		m_xBloomThresholdShader, m_xBloomThresholdPipeline,
		FluxShaderProgram::BloomThreshold, BLOOM_FORMAT);

	Flux_PipelineHelper::BuildFullscreenPipeline(
		m_xBloomDownsampleShader, m_xBloomDownsamplePipeline,
		FluxShaderProgram::BloomDownsample, BLOOM_FORMAT);

	{
		Flux_PipelineSpecification xSpec = Flux_PipelineHelper::CreateFullscreenSpec(
			m_xBloomUpsampleShader, FluxShaderProgram::BloomUpsample, BLOOM_FORMAT);
		xSpec.m_axBlendStates[0].m_bBlendEnabled = true;
		xSpec.m_axBlendStates[0].m_eSrcBlendFactor = BLEND_FACTOR_ONE;
		xSpec.m_axBlendStates[0].m_eDstBlendFactor = BLEND_FACTOR_ONE;
		Flux_PipelineBuilder::FromSpecification(m_xBloomUpsamplePipeline, xSpec);
	}

	// Auto-exposure compute pipelines — shader Initialise + RootSig +
	// ComputePipelineBuilder. The VRAM buffers (m_xHistogramBuffer /
	// m_xExposureBuffer) live in Initialise and are not touched here.
	m_xLuminanceHistogramShader.Initialise(FluxShaderProgram::HDR_Luminance);
	Flux_RootSigBuilder::FromReflection(m_xLuminanceRootSig, m_xLuminanceHistogramShader.GetReflection());
	Flux_ComputePipelineBuilder::BuildFromShader(m_xLuminanceHistogramPipeline,
		m_xLuminanceHistogramShader, m_xLuminanceRootSig);

	m_xAdaptationShader.Initialise(FluxShaderProgram::HDR_Adaptation);
	Flux_RootSigBuilder::FromReflection(m_xAdaptationRootSig, m_xAdaptationShader.GetReflection());
	Flux_ComputePipelineBuilder::BuildFromShader(m_xAdaptationPipeline,
		m_xAdaptationShader, m_xAdaptationRootSig);
}

static void PreExecuteLuminanceHistogram(void* pUserData);
static void ExecuteLuminanceHistogram(Flux_CommandList* pxCommandList, void* pUserData);
static void ExecuteAdaptation(Flux_CommandList* pxCommandList, void* pUserData);
static void ExecuteBloomThreshold(Flux_CommandList* pxCommandList, void* pUserData);
static void ExecuteBloomDownsample(Flux_CommandList* pxCommandList, void* pUserData);
static void ExecuteBloomUpsample(Flux_CommandList* pxCommandList, void* pUserData);
static void ExecuteToneMapping(Flux_CommandList* pxCommandList, void* pUserData);

void Flux_HDRImpl::Initialise(Flux_GraphicsImpl& xGraphics, Zenith_Vulkan_MemoryManager& xVulkanMemory,
                             Zenith_Vulkan_Swapchain& xSwapchain, FrameContext& xFrame)
{
	m_pxGraphics     = &xGraphics;
	m_pxVulkanMemory = &xVulkanMemory;
	m_pxSwapchain    = &xSwapchain;
	m_pxFrame        = &xFrame;

	BuildPipelines();

	// One-time setup that hot-reload must NOT repeat (would leak VRAM /
	// double-register debug variables).
	{
		// Histogram buffer (256 bins of u_int).
		u_int auZeroHistogram[256] = { 0 };
		m_pxVulkanMemory->InitialiseReadWriteBuffer(auZeroHistogram, 256 * sizeof(u_int), m_xHistogramBuffer);
		Zenith_Assert(m_xHistogramBuffer.GetBuffer().m_xVRAMHandle.IsValid(),
			"Flux_HDR: Failed to create histogram buffer");

		// Exposure buffer (4 floats: avgLum, currentExp, targetExp, pad).
		float afInitialExposure[4] = { 0.18f, 1.0f, 1.0f, 0.0f };
		m_pxVulkanMemory->InitialiseReadWriteBuffer(afInitialExposure, 4 * sizeof(float), m_xExposureBuffer);
		Zenith_Assert(m_xExposureBuffer.GetBuffer().m_xVRAMHandle.IsValid(),
			"Flux_HDR: Failed to create exposure buffer");
	}

#ifdef ZENITH_TOOLS
	RegisterDebugVariables();

	// Hot reload — every HDR shader reloads via BuildPipelines().
	static const FluxShaderProgram s_axPrograms[] = {
		FluxShaderProgram::HDR_ToneMapping,
		FluxShaderProgram::BloomThreshold,
		FluxShaderProgram::BloomDownsample,
		FluxShaderProgram::BloomUpsample,
		FluxShaderProgram::HDR_Luminance,
		FluxShaderProgram::HDR_Adaptation,
	};
	Flux_ShaderHotReload::RegisterSubsystem([](){ g_xEngine.HDR().BuildPipelines(); },
		s_axPrograms, sizeof(s_axPrograms) / sizeof(s_axPrograms[0]));
#endif

	// Render targets are graph-owned transients, created in SetupTransients.
	// No resize callback needed — the graph re-creates them on every
	// SetupRenderGraph pass, which is already a resize callback.

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_HDR Initialised");
}

void Flux_HDRImpl::Shutdown()
{
	m_pxGraph = nullptr;

	// Cleanup auto-exposure compute resources
	m_pxVulkanMemory->DestroyReadWriteBuffer(m_xHistogramBuffer);
	m_pxVulkanMemory->DestroyReadWriteBuffer(m_xExposureBuffer);

	m_pxGraphics     = nullptr;
	m_pxVulkanMemory = nullptr;
	m_pxSwapchain    = nullptr;
	m_pxFrame        = nullptr;

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_HDR shut down");
}

void Flux_HDRImpl::Reset()
{
	m_fCurrentExposure = 1.0f;
	m_fAverageLuminance = 0.18f;

	// Clear histogram buffer to prevent stale data after scene transitions
	// This ensures auto-exposure starts fresh when enabled
	if (m_xHistogramBuffer.GetBuffer().m_xVRAMHandle.IsValid())
	{
		static u_int auZeroHistogram[256] = { 0 };
		m_pxVulkanMemory->UploadBufferData(
			m_xHistogramBuffer.GetBuffer().m_xVRAMHandle,
			auZeroHistogram,
			256 * sizeof(u_int));
	}

	// Reset exposure buffer to default values
	if (m_xExposureBuffer.GetBuffer().m_xVRAMHandle.IsValid())
	{
		float afInitialExposure[4] = { 0.18f, 1.0f, 1.0f, 0.0f };
		m_pxVulkanMemory->UploadBufferData(
			m_xExposureBuffer.GetBuffer().m_xVRAMHandle,
			afInitialExposure,
			4 * sizeof(float));
	}
}


// InitializeAutoExposure() inlined into BuildPipelines() (compute pipelines)
// and Initialise() (one-time VRAM buffer creation) — split was needed so
// hot-reload can rebuild the compute pipelines without leaking the buffers.

static void PreExecuteLuminanceHistogram(void* pUserData)
{
	(void)pUserData;

	// Non-capturing graph trampoline: recover this subsystem instance, then
	// route the VulkanMemory reach-ins through the injected member pointer.
	Flux_HDRImpl& xHDR = g_xEngine.HDR();

	// SYNC NOTE: UploadBufferData() below stages into the memory transfer
	// command buffer. EndFrame() submits that CB and signals s_xMemorySemaphore,
	// which the subsequent render submission waits on, so any uploads issued
	// from a Phase-0 OnPrepare callback are guaranteed to be visible before any
	// pass that reads them. This replaces the old explicit memory-update
	// ordering token that the static_assert in EndFrame used to enforce.
	xHDR.SyncDebugVariables();
	const bool bAutoExposure = Zenith_GraphicsOptions::Get().m_bHDRAutoExposureEnabled;
	bool bAutoExposureJustEnabled = bAutoExposure && !xHDR.m_bAutoExposureWasEnabled;
	xHDR.m_bAutoExposureWasEnabled = bAutoExposure;
	if (bAutoExposureJustEnabled && xHDR.m_xExposureBuffer.GetBuffer().m_xVRAMHandle.IsValid())
	{
		float afInitialExposure[4] = { 0.18f, 1.0f, 1.0f, 0.0f };
		xHDR.m_pxVulkanMemory->UploadBufferData(
			xHDR.m_xExposureBuffer.GetBuffer().m_xVRAMHandle,
			afInitialExposure,
			4 * sizeof(float));
	}

	// Clear histogram buffer before compute (only when auto-exposure or histogram visualization is active)
	if ((bAutoExposure || dbg_bHDRShowHistogram) && xHDR.m_xHistogramBuffer.GetBuffer().m_xVRAMHandle.IsValid())
	{
		static u_int auZeroHistogram[256] = { 0 };
		xHDR.m_pxVulkanMemory->UploadBufferData(
			xHDR.m_xHistogramBuffer.GetBuffer().m_xVRAMHandle,
			auZeroHistogram,
			256 * sizeof(u_int));
	}
}

static void ExecuteLuminanceHistogram(Flux_CommandList* pxCommandList, void* pUserData)
{
	(void)pUserData;

	// Non-capturing graph trampoline: recover this subsystem instance, then
	// route reach-ins through it and its injected member pointers.
	Flux_HDRImpl& xHDR = g_xEngine.HDR();

	if (!xHDR.m_xHistogramBuffer.GetBuffer().m_xVRAMHandle.IsValid())
	{
		return;
	}

	// Buffer uploads are handled in PreExecuteLuminanceHistogram

	pxCommandList->AddCommand<Flux_CommandBindComputePipeline>(&xHDR.m_xLuminanceHistogramPipeline);

	LuminanceConstants xConsts;
	xConsts.m_uImageWidth = xHDR.m_pxSwapchain->GetWidth();
	xConsts.m_uImageHeight = xHDR.m_pxSwapchain->GetHeight();
	xConsts.m_fMinLogLum = xHDR.m_fMinLogLuminance;
	xConsts.m_fLogLumRange = xHDR.m_fLogLuminanceRange;

	Flux_ShaderBinder xBinder(*pxCommandList);
	xBinder.BindDrawConstants(xHDR.m_xLuminanceHistogramShader, "LuminanceConstants", &xConsts, sizeof(xConsts));
	xBinder.BindSRV(xHDR.m_xLuminanceHistogramShader, "g_xHDRTex", &xHDR.GetHDRSceneTarget().SRV());
	xBinder.BindUAV_Buffer(xHDR.m_xLuminanceHistogramShader, "g_auHistogram", &xHDR.m_xHistogramBuffer.GetUAV());

	u_int uGroupsX = (xHDR.m_pxSwapchain->GetWidth() + 15) / 16;
	u_int uGroupsY = (xHDR.m_pxSwapchain->GetHeight() + 15) / 16;
	pxCommandList->AddCommand<Flux_CommandDispatch>(uGroupsX, uGroupsY, 1);
}

static void ExecuteAdaptation(Flux_CommandList* pxCommandList, void* pUserData)
{
	(void)pUserData;

	// Non-capturing graph trampoline: recover this subsystem instance, then
	// route reach-ins through it and its injected member pointers.
	Flux_HDRImpl& xHDR = g_xEngine.HDR();

	if (!xHDR.m_xHistogramBuffer.GetBuffer().m_xVRAMHandle.IsValid() ||
		!xHDR.m_xExposureBuffer.GetBuffer().m_xVRAMHandle.IsValid())
	{
		return;
	}

	pxCommandList->AddCommand<Flux_CommandBindComputePipeline>(&xHDR.m_xAdaptationPipeline);

	AdaptationConstants xConsts;
	xConsts.m_fMinLogLum = xHDR.m_fMinLogLuminance;
	xConsts.m_fLogLumRange = xHDR.m_fLogLuminanceRange;
	xConsts.m_fDeltaTime = xHDR.m_pxFrame->GetDt();
	xConsts.m_fAdaptationSpeed = dbg_bHDRFreezeExposure ? 0.0f : xHDR.m_fAdaptationSpeed;
	xConsts.m_fTargetLuminance = xHDR.m_fTargetLuminance;
	xConsts.m_fMinExposure = xHDR.m_fMinExposure;
	xConsts.m_fMaxExposure = xHDR.m_fMaxExposure;
	xConsts.m_fLowPercentile = 0.05f;
	xConsts.m_fHighPercentile = 0.95f;
	xConsts.m_uTotalPixels = xHDR.m_pxSwapchain->GetWidth() * xHDR.m_pxSwapchain->GetHeight();
	xConsts.m_uPad0 = 0;
	xConsts.m_uPad1 = 0;

	Flux_ShaderBinder xBinder(*pxCommandList);
	xBinder.BindDrawConstants(xHDR.m_xAdaptationShader, "AdaptationConstants", &xConsts, sizeof(xConsts));
	xBinder.BindUAV_Buffer(xHDR.m_xAdaptationShader, "g_auHistogram", &xHDR.m_xHistogramBuffer.GetUAV());
	xBinder.BindUAV_Buffer(xHDR.m_xAdaptationShader, "g_afExposureData", &xHDR.m_xExposureBuffer.GetUAV());

	pxCommandList->AddCommand<Flux_CommandDispatch>(1, 1, 1);
}

static void SubmitHistogramLabels()
{
	// Non-capturing helper reached only from the ExecuteToneMapping trampoline:
	// recover this subsystem instance, then route the swapchain reach-in through
	// its injected member pointer.
	Flux_HDRImpl& xHDR = g_xEngine.HDR();

	// Histogram position matches shader: bottom-left corner
	// fMargin = 0.02, fHistWidth = 0.3, fHistHeight = 0.15
	const float fScreenWidth = static_cast<float>(xHDR.m_pxSwapchain->GetWidth());
	const float fScreenHeight = static_cast<float>(xHDR.m_pxSwapchain->GetHeight());

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

static void ExecuteBloomThreshold(Flux_CommandList* pxCommandList, void* pUserData)
{
	(void)pUserData;

	// Non-capturing graph trampoline: recover this subsystem instance, then
	// route reach-ins through it and its injected member pointers.
	Flux_HDRImpl& xHDR = g_xEngine.HDR();

	Flux_RenderAttachment& xBloom0 = xHDR.GetBloomChainAttachment(0);
	BloomConstants xBloomConsts;
	xBloomConsts.m_fThreshold = xHDR.m_fBloomThreshold;
	xBloomConsts.m_fIntensity = xHDR.m_fBloomIntensity;
	xBloomConsts.m_xTexelSize = Zenith_Maths::Vector2(1.0f / xBloom0.m_xSurfaceInfo.m_uWidth, 1.0f / xBloom0.m_xSurfaceInfo.m_uHeight);

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&xHDR.m_xBloomThresholdPipeline);
	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&xHDR.m_pxGraphics->m_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&xHDR.m_pxGraphics->m_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);
	xBinder.BindSRV(xHDR.m_xBloomThresholdShader, "g_xHDRTex", &xHDR.GetHDRSceneTarget().SRV());
	xBinder.BindDrawConstants(xHDR.m_xBloomThresholdShader, "BloomConstants", &xBloomConsts, sizeof(BloomConstants));

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
}

static void ExecuteBloomDownsample(Flux_CommandList* pxCommandList, void* pUserData)
{
	const u_int uMipIndex = *static_cast<const u_int*>(pUserData);

	// Non-capturing graph trampoline: recover this subsystem instance, then
	// route reach-ins through it and its injected member pointers.
	Flux_HDRImpl& xHDR = g_xEngine.HDR();

	Flux_RenderAttachment& xTarget = xHDR.GetBloomChainAttachment(uMipIndex);
	BloomConstants xBloomConsts;
	xBloomConsts.m_fThreshold = xHDR.m_fBloomThreshold;
	xBloomConsts.m_fIntensity = xHDR.m_fBloomIntensity;
	xBloomConsts.m_xTexelSize = Zenith_Maths::Vector2(1.0f / xTarget.m_xSurfaceInfo.m_uWidth, 1.0f / xTarget.m_xSurfaceInfo.m_uHeight);

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&xHDR.m_xBloomDownsamplePipeline);
	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&xHDR.m_pxGraphics->m_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&xHDR.m_pxGraphics->m_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);
	xBinder.BindSRV(xHDR.m_xBloomDownsampleShader, "g_xSourceTex", &xHDR.GetBloomChainAttachment(uMipIndex - 1).SRV());
	xBinder.BindDrawConstants(xHDR.m_xBloomDownsampleShader, "BloomConstants", &xBloomConsts, sizeof(BloomConstants));

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
}

static void ExecuteBloomUpsample(Flux_CommandList* pxCommandList, void* pUserData)
{
	const u_int uIndex = *static_cast<const u_int*>(pUserData);
	const u_int uTargetMip = 3 - uIndex;
	const u_int uSourceMip = uTargetMip + 1;

	// Non-capturing graph trampoline: recover this subsystem instance, then
	// route reach-ins through it and its injected member pointers.
	Flux_HDRImpl& xHDR = g_xEngine.HDR();

	Flux_RenderAttachment& xTarget = xHDR.GetBloomChainAttachment(uTargetMip);
	BloomConstants xBloomConsts;
	xBloomConsts.m_fThreshold = xHDR.m_fBloomThreshold;
	xBloomConsts.m_fIntensity = xHDR.m_fBloomIntensity;
	xBloomConsts.m_xTexelSize = Zenith_Maths::Vector2(1.0f / xTarget.m_xSurfaceInfo.m_uWidth, 1.0f / xTarget.m_xSurfaceInfo.m_uHeight);

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&xHDR.m_xBloomUpsamplePipeline);
	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&xHDR.m_pxGraphics->m_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&xHDR.m_pxGraphics->m_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);
	xBinder.BindSRV(xHDR.m_xBloomUpsampleShader, "g_xSourceTex", &xHDR.GetBloomChainAttachment(uSourceMip).SRV());
	xBinder.BindDrawConstants(xHDR.m_xBloomUpsampleShader, "BloomConstants", &xBloomConsts, sizeof(BloomConstants));

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
}

static void ExecuteToneMapping(Flux_CommandList* pxCommandList, void* pUserData)
{
	(void)pUserData;

	// Non-capturing graph trampoline: recover this subsystem instance, then
	// route reach-ins through it and its injected member pointers.
	Flux_HDRImpl& xHDR = g_xEngine.HDR();

	ToneMappingConstants xConsts;
	xConsts.m_fExposure = xHDR.m_fExposure;
	xConsts.m_fBloomIntensity = Zenith_GraphicsOptions::Get().m_bHDRBloomEnabled ? xHDR.m_fBloomIntensity : 0.0f;
	xConsts.m_uToneMappingOperator = static_cast<u_int>(xHDR.m_eToneMappingOperator);
	xConsts.m_uDebugMode = dbg_uHDRDebugMode;
	xConsts.m_bShowHistogram = dbg_bHDRShowHistogram ? 1 : 0;
	xConsts.m_bAutoExposure = Zenith_GraphicsOptions::Get().m_bHDRAutoExposureEnabled ? 1 : 0;
	xConsts.m_uPad0 = 0;
	xConsts.m_uPad1 = 0;

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&xHDR.m_xToneMappingPipeline);
	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&xHDR.m_pxGraphics->m_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&xHDR.m_pxGraphics->m_xQuadMesh.GetIndexBuffer());

	{
		Flux_ShaderBinder xBinder(*pxCommandList);
		xBinder.BindSRV(xHDR.m_xToneMappingShader, "g_xHDRTex", &xHDR.GetHDRSceneTarget().SRV());
		xBinder.BindSRV(xHDR.m_xToneMappingShader, "g_xBloomTex", &xHDR.GetBloomChainAttachment(0).SRV());
		// Slang reflection keys on the variable name (not the GLSL block
		// name) — match the names declared in Flux_ToneMapping.slang.
		xBinder.BindUAV_Buffer(xHDR.m_xToneMappingShader, "g_auHistogram",   &xHDR.m_xHistogramBuffer.GetUAV());
		xBinder.BindUAV_Buffer(xHDR.m_xToneMappingShader, "g_afExposureData", &xHDR.m_xExposureBuffer.GetUAV());
		xBinder.BindDrawConstants(xHDR.m_xToneMappingShader, "ToneMappingConstants", &xConsts, sizeof(ToneMappingConstants));
	}

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);

	// Submit histogram labels if ShowHistogram is enabled
	if (dbg_bHDRShowHistogram)
	{
		SubmitHistogramLabels();
	}
}

void Flux_HDRImpl::SetupTransients(Flux_RenderGraph& xGraph)
{
	m_pxGraph = &xGraph;

	// Create transient HDR scene target early — many subsystems write to
	// GetHDRSceneTarget() during their SetupRenderGraph, so the handle must
	// exist before they run.
	Flux_TransientTextureDesc xHDRDesc;
	xHDRDesc.m_uWidth = m_pxSwapchain->GetWidth();
	xHDRDesc.m_uHeight = m_pxSwapchain->GetHeight();
	xHDRDesc.m_eFormat = HDR_SCENE_FORMAT;
	xHDRDesc.m_uMemoryFlags = (1u << MEMORY_FLAGS__SHADER_READ);
	m_xHDRSceneTargetHandle = xGraph.CreateTransient(xHDRDesc);
}

void Flux_HDRImpl::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	// m_pxGraph already set by SetupTransients(). HDR scene target transient
	// already created there too.

	u_int uBloomWidth = m_pxSwapchain->GetWidth() / 2;
	u_int uBloomHeight = m_pxSwapchain->GetHeight() / 2;

	for (u_int i = 0; i < 5; i++)
	{
		Flux_TransientTextureDesc xDesc;
		xDesc.m_uWidth = uBloomWidth;
		xDesc.m_uHeight = uBloomHeight;
		xDesc.m_eFormat = BLOOM_FORMAT;
		xDesc.m_uMemoryFlags = (1u << MEMORY_FLAGS__SHADER_READ);
		m_axBloomChainHandles[i] = xGraph.CreateTransient(xDesc);

		uBloomWidth = std::max(1u, uBloomWidth / 2);
		uBloomHeight = std::max(1u, uBloomHeight / 2);
	}

	xGraph.AddPass("HDR_LuminanceHistogram", ExecuteLuminanceHistogram)
		.Prepare    (PreExecuteLuminanceHistogram)
		.Reads      (GetHDRSceneTarget(),                     RESOURCE_ACCESS_READ_SRV)
		.WritesBuffer(m_xHistogramBuffer.GetBuffer(),          RESOURCE_ACCESS_WRITE_UAV);

	// Histogram is bound as UAV_Buffer (see ExecuteAdaptation); declare as
	// READWRITE_UAV so the graph barrier matches the compute-stage access.
	xGraph.AddPass("HDR_Adaptation", ExecuteAdaptation)
		.ReadsBuffer (m_xHistogramBuffer.GetBuffer(), RESOURCE_ACCESS_READWRITE_UAV)
		.WritesBuffer(m_xExposureBuffer.GetBuffer(),  RESOURCE_ACCESS_WRITE_UAV);

	xGraph.AddPass("HDR_BloomThreshold", ExecuteBloomThreshold)
		.ClearTargets()
		.Reads          (GetHDRSceneTarget(),             RESOURCE_ACCESS_READ_SRV)
		.WritesTransient(m_axBloomChainHandles[0],        RESOURCE_ACCESS_WRITE_RTV);

	static const char* s_aszBloomDownsampleNames[] = {
		"HDR_BloomDownsample Mip1", "HDR_BloomDownsample Mip2",
		"HDR_BloomDownsample Mip3", "HDR_BloomDownsample Mip4",
	};
	for (u_int i = 1; i < 5; i++)
	{
		xGraph.AddPass(s_aszBloomDownsampleNames[i - 1], ExecuteBloomDownsample, &s_axBloomMipUserData[i])
			.ClearTargets()
			.ReadsTransient (m_axBloomChainHandles[i - 1], RESOURCE_ACCESS_READ_SRV)
			.WritesTransient(m_axBloomChainHandles[i],     RESOURCE_ACCESS_WRITE_RTV);
	}

	static const char* s_aszBloomUpsampleNames[] = {
		"HDR_BloomUpsample Mip3", "HDR_BloomUpsample Mip2",
		"HDR_BloomUpsample Mip1", "HDR_BloomUpsample Mip0",
	};
	for (u_int i = 0; i < 4; i++)
	{
		const u_int uTargetMip = 3 - i;
		const u_int uSourceMip = uTargetMip + 1;
		xGraph.AddPass(s_aszBloomUpsampleNames[i], ExecuteBloomUpsample, &s_axBloomUpsampleUserData[i])
			.ReadsTransient (m_axBloomChainHandles[uSourceMip], RESOURCE_ACCESS_READ_SRV)
			.WritesTransient(m_axBloomChainHandles[uTargetMip], RESOURCE_ACCESS_WRITE_RTV);
	}

	// Tone mapping samples exposure (and optionally histogram for a debug
	// overlay). Both are bound as UAV_Buffer in ExecuteToneMapping so declare
	// as READWRITE_UAV — the barrier must match the binding's access mode
	// rather than the shader's actual read-only intent.
	xGraph.AddPass("HDR_ToneMapping", ExecuteToneMapping)
		.ClearTargets()
		.Reads         (GetHDRSceneTarget(),                    RESOURCE_ACCESS_READ_SRV)
		.Writes        (m_pxGraphics->GetFinalRenderTarget(),  RESOURCE_ACCESS_WRITE_RTV)
		.ReadsBuffer   (m_xHistogramBuffer.GetBuffer(),         RESOURCE_ACCESS_READWRITE_UAV)
		.ReadsBuffer   (m_xExposureBuffer.GetBuffer(),          RESOURCE_ACCESS_READWRITE_UAV)
		.ReadsTransient(m_axBloomChainHandles[0],               RESOURCE_ACCESS_READ_SRV);
}

Flux_ShaderResourceView& Flux_HDRImpl::GetHDRSceneSRV()
{
	return GetHDRSceneTarget().SRV();
}

Flux_RenderAttachment& Flux_HDRImpl::GetHDRSceneTarget()
{
	Zenith_Assert(m_pxGraph, "Flux_HDRImpl::GetHDRSceneTarget: graph pointer is null");
	return m_pxGraph->GetTransientAttachment(m_xHDRSceneTargetHandle);
}

void Flux_HDRImpl::GetHDRSceneTargetSetup(Flux_RenderAttachment* apxColourAttachments[], uint32_t& uNumColour, Flux_RenderAttachment*& pxDepthStencil)
{
	apxColourAttachments[0] = &GetHDRSceneTarget();
	uNumColour = 1;
	pxDepthStencil = nullptr;
}

void Flux_HDRImpl::GetHDRSceneTargetSetupWithDepth(Flux_RenderAttachment* apxColourAttachments[], uint32_t& uNumColour, Flux_RenderAttachment*& pxDepthStencil)
{
	apxColourAttachments[0] = &GetHDRSceneTarget();
	uNumColour = 1;
	pxDepthStencil = &m_pxGraphics->GetDepthAttachment();
}



void Flux_HDRImpl::SetExposure(float fExposure)
{
	m_fExposure = std::clamp(fExposure, 0.001f, 1000.0f);
}

void Flux_HDRImpl::SetBloomIntensity(float fIntensity)
{
	m_fBloomIntensity = std::clamp(fIntensity, 0.0f, 10.0f);
}

void Flux_HDRImpl::SetBloomThreshold(float fThreshold)
{
	m_fBloomThreshold = std::clamp(fThreshold, 0.0f, 100.0f);
}

float Flux_HDRImpl::GetCurrentExposure()
{
	return m_fCurrentExposure;
}

float Flux_HDRImpl::GetAverageLuminance()
{
	return m_fAverageLuminance;
}

void Flux_HDRImpl::SetAdaptationSpeed(float fSpeed)
{
	m_fAdaptationSpeed = fSpeed;
}

void Flux_HDRImpl::SetTargetLuminance(float fLuminance)
{
	m_fTargetLuminance = fLuminance;
}

void Flux_HDRImpl::SetExposureRange(float fMin, float fMax)
{
	m_fMinExposure = fMin;
	m_fMaxExposure = fMax;
}

bool Flux_HDRImpl::IsAutoExposureEnabled()
{
	return Zenith_GraphicsOptions::Get().m_bHDRAutoExposureEnabled;
}

float Flux_HDRImpl::GetAdaptationSpeed()
{
	return m_fAdaptationSpeed;
}

float Flux_HDRImpl::GetTargetLuminance()
{
	return m_fTargetLuminance;
}

Flux_RenderAttachment& Flux_HDRImpl::GetBloomChainAttachment(u_int uIndex)
{
	Zenith_Assert(uIndex < 5, "Flux_HDRImpl::GetBloomChainAttachment: index %u out of range", uIndex);
	Zenith_Assert(m_pxGraph, "Flux_HDRImpl::GetBloomChainAttachment: graph pointer is null");
	return m_pxGraph->GetTransientAttachment(m_axBloomChainHandles[uIndex]);
}

bool Flux_HDRImpl::IsEnabled()
{
	// HDR pipeline is always active (tone mapping always runs)
	// This returns true if any HDR post-processing features are enabled
	const Zenith_GraphicsOptions& xOpts = Zenith_GraphicsOptions::Get();
	return xOpts.m_bHDRAutoExposureEnabled || xOpts.m_bHDRBloomEnabled;
}

#ifdef ZENITH_TOOLS
void Flux_HDRImpl::RegisterDebugVariables()
{
	g_xEngine.DebugVariables().AddUInt32({ "Flux", "HDR", "DebugMode" }, dbg_uHDRDebugMode, 0, HDR_DEBUG_COUNT - 1);
	g_xEngine.DebugVariables().AddFloat({ "Flux", "HDR", "Exposure" }, dbg_fHDRExposure, 0.01f, 10.0f);
	g_xEngine.DebugVariables().AddFloat({ "Flux", "HDR", "BloomIntensity" }, dbg_fHDRBloomIntensity, 0.0f, 2.0f);
	g_xEngine.DebugVariables().AddFloat({ "Flux", "HDR", "BloomThreshold" }, dbg_fHDRBloomThreshold, 0.0f, 5.0f);
	g_xEngine.DebugVariables().AddUInt32({ "Flux", "HDR", "ToneMappingOperator" }, dbg_uHDRToneMappingOperator, 0, TONEMAPPING_COUNT - 1);
	g_xEngine.DebugVariables().AddBoolean({ "Flux", "HDR", "ShowHistogram" }, dbg_bHDRShowHistogram);
	g_xEngine.DebugVariables().AddBoolean({ "Flux", "HDR", "FreezeExposure" }, dbg_bHDRFreezeExposure);
	g_xEngine.DebugVariables().AddFloat({ "Flux", "HDR", "AdaptationSpeed" }, dbg_fHDRAdaptationSpeed, 0.1f, 10.0f);
	g_xEngine.DebugVariables().AddFloat({ "Flux", "HDR", "TargetLuminance" }, dbg_fHDRTargetLuminance, 0.01f, 1.0f);
	g_xEngine.DebugVariables().AddFloat({ "Flux", "HDR", "MinExposure" }, dbg_fHDRMinExposure, 0.01f, 1.0f);
	g_xEngine.DebugVariables().AddFloat({ "Flux", "HDR", "MaxExposure" }, dbg_fHDRMaxExposure, 1.0f, 20.0f);
}

// Debug-SRV callbacks — mirror g_xEngine.FluxGraphics().GetDebugSRV_MRTDiffuse. Register
// via g_xEngine.DebugVariables().AddTextureCallback in Flux::LateInitialise; the
// callback is invoked on every ImGui draw so the preview tracks transient
// rebuilds on resize without leaving a dangling SRV.
const Flux_ShaderResourceView* Flux_HDRImpl::GetDebugSRV_HDRScene()
{
	if (m_pxGraph == nullptr) return nullptr;
	return &m_pxGraph->GetTransientAttachment(m_xHDRSceneTargetHandle).SRV();
}
const Flux_ShaderResourceView* Flux_HDRImpl::GetDebugSRV_Bloom0()
{
	if (m_pxGraph == nullptr) return nullptr;
	return &m_pxGraph->GetTransientAttachment(m_axBloomChainHandles[0]).SRV();
}
const Flux_ShaderResourceView* Flux_HDRImpl::GetDebugSRV_Bloom1()
{
	if (m_pxGraph == nullptr) return nullptr;
	return &m_pxGraph->GetTransientAttachment(m_axBloomChainHandles[1]).SRV();
}
const Flux_ShaderResourceView* Flux_HDRImpl::GetDebugSRV_Bloom2()
{
	if (m_pxGraph == nullptr) return nullptr;
	return &m_pxGraph->GetTransientAttachment(m_axBloomChainHandles[2]).SRV();
}
#endif
