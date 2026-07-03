#include "Zenith.h"
#include "Flux/HDR/Flux_HDR_Shaders.h"

#include <atomic>
#include "Profiling/Zenith_Profiling.h"
#include "Flux/HDR/Flux_HDRImpl.h"
#include "Core/Zenith_Engine.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Flux/Shaders/Generated/HDR.h" // typed binding handles
#include "Core/Zenith_GraphicsOptions.h"
#include "Flux/Text/Flux_TextQueue.h"
// VulkanMemory owns the auto-exposure VRAM buffers + uploads, the swapchain
// supplies the back-buffer dims, FrameContext supplies the per-frame
// delta-time. Method bodies need the full types.
#include "Flux/Flux_BackendTypes.h"
#include "Core/FrameContext.h"

#ifdef ZENITH_TOOLS
#include "DebugVariables/Zenith_DebugVariables.h"
#endif

// Phase 7h: subsystem state moved to Flux_HDRImpl held by Zenith_Engine.

// Strongly-typed per-pass user data: each downsample / upsample pass is given
// a pointer into one of these arrays so the execute callback recovers the mip
// index without going through a void*-as-int reinterpret_cast. These now live
// as Flux_HDRImpl members (m_axBloomMipUserData / m_axBloomUpsampleUserData) so
// they are not module-scope statics; SetupRenderGraph feeds &member[i] to
// AddPass and the pointers stay stable for the graph's lifetime (the Impl is
// engine-owned and outlives the graph).

// Bloom chain format.
static constexpr TextureFormat BLOOM_FORMAT = TEXTURE_FORMAT_R16G16B16A16_SFLOAT;

// Debug variables
DEBUGVAR u_int dbg_uHDRDebugMode = HDR_DEBUG_NONE;
DEBUGVAR float dbg_fHDRExposure = 1.0f;
DEBUGVAR float dbg_fHDRBloomIntensity = 0.5f;
// HDR (pre-exposure) luminance above which bloom is extracted. Raised 1.0 -> 3.0
// for the brighter sun calibration: a near-white DIFFUSE surface (e.g. the grid-
// textured platform) now reaches HDR ~2-3 under direct sun and was lens-blooming,
// which is non-physical. 3.0 keeps bloom on genuine sources (sky ~7, sun disk,
// emissive, hot speculars) while white diffuse surfaces stop glowing.
DEBUGVAR float dbg_fHDRBloomThreshold = 3.0f;
DEBUGVAR u_int dbg_uHDRToneMappingOperator = TONEMAPPING_AGX;
DEBUGVAR bool dbg_bHDRShowHistogram = false;
DEBUGVAR bool dbg_bHDRFreezeExposure = false;
DEBUGVAR float dbg_fHDRAdaptationSpeed = 4.0f;
// Auto-exposure key (target geometric-mean luminance). Slightly below the 0.18
// linear-average mid-grey convention because log-average metering on outdoor
// scenes (bright sky + shadows) reads low and the standard key over-exposes.
DEBUGVAR float dbg_fHDRTargetLuminance = 0.14f;
DEBUGVAR float dbg_fHDRMinExposure = 0.05f;
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
		Flux_HDRShaders::xHDR_ToneMapping, FINAL_RT_FORMAT);

	// Bloom passes — all migrated to the Slang shader registry. They share
	// Common.Fullscreen for vsMain so a single .slang module per pass holds
	// both stages.
	Flux_PipelineHelper::BuildFullscreenPipeline(
		m_xBloomThresholdShader, m_xBloomThresholdPipeline,
		Flux_HDRShaders::xBloomThreshold, BLOOM_FORMAT);

	Flux_PipelineHelper::BuildFullscreenPipeline(
		m_xBloomDownsampleShader, m_xBloomDownsamplePipeline,
		Flux_HDRShaders::xBloomDownsample, BLOOM_FORMAT);

	{
		Flux_PipelineSpecification xSpec = Flux_PipelineHelper::CreateFullscreenSpec(
			m_xBloomUpsampleShader, Flux_HDRShaders::xBloomUpsample, BLOOM_FORMAT);
		xSpec.m_axBlendStates[0].m_bBlendEnabled = true;
		xSpec.m_axBlendStates[0].m_eSrcBlendFactor = BLEND_FACTOR_ONE;
		xSpec.m_axBlendStates[0].m_eDstBlendFactor = BLEND_FACTOR_ONE;
		Flux_PipelineBuilder::FromSpecification(m_xBloomUpsamplePipeline, xSpec);
	}

	// Auto-exposure compute pipelines — shader Initialise + RootSig +
	// ComputePipelineBuilder. The VRAM buffers (m_xHistogramBuffer /
	// m_xExposureBuffer) live in Initialise and are not touched here.
	m_xLuminanceHistogramShader.Initialise(Flux_HDRShaders::xHDR_Luminance);
	Flux_RootSigBuilder::FromReflection(m_xLuminanceRootSig, m_xLuminanceHistogramShader.GetReflection());
	Flux_ComputePipelineBuilder::BuildFromShader(m_xLuminanceHistogramPipeline,
		m_xLuminanceHistogramShader, m_xLuminanceRootSig);

	m_xAdaptationShader.Initialise(Flux_HDRShaders::xHDR_Adaptation);
	Flux_RootSigBuilder::FromReflection(m_xAdaptationRootSig, m_xAdaptationShader.GetReflection());
	Flux_ComputePipelineBuilder::BuildFromShader(m_xAdaptationPipeline,
		m_xAdaptationShader, m_xAdaptationRootSig);
}

static void PreExecuteLuminanceHistogram(void* pUserData);
static void ExecuteLuminanceHistogram(Flux_CommandBuffer* pxCommandList, void* pUserData);
static void ExecuteAdaptation(Flux_CommandBuffer* pxCommandList, void* pUserData);
static void ExecuteBloomThreshold(Flux_CommandBuffer* pxCommandList, void* pUserData);
static void ExecuteBloomDownsample(Flux_CommandBuffer* pxCommandList, void* pUserData);
static void ExecuteBloomUpsample(Flux_CommandBuffer* pxCommandList, void* pUserData);
static void ExecuteToneMapping(Flux_CommandBuffer* pxCommandList, void* pUserData);

void Flux_HDRImpl::Initialise()
{
	BuildPipelines();

	// One-time setup that hot-reload must NOT repeat (would leak VRAM /
	// double-register debug variables).
	{
		Flux_MemoryManager& xVulkanMemory = g_xEngine.FluxMemory();

		// Histogram buffer (256 bins of u_int).
		u_int auZeroHistogram[256] = { 0 };
		xVulkanMemory.InitialiseReadWriteBuffer(auZeroHistogram, 256 * sizeof(u_int), m_xHistogramBuffer);
		Zenith_Assert(m_xHistogramBuffer.GetBuffer().m_xVRAMHandle.IsValid(),
			"Flux_HDR: Failed to create histogram buffer");

		// Exposure buffer (4 floats: avgLum, currentExp, targetExp, pad).
		// Seed exposure near a daylight value (0.4), not 1.0, so the scene is
		// well-exposed immediately and stays close across the smoke runner's
		// scene reloads (which re-fire this seed).
		float afInitialExposure[4] = { 0.4f, 0.4f, 0.4f, 0.0f };
		xVulkanMemory.InitialiseReadWriteBuffer(afInitialExposure, 4 * sizeof(float), m_xExposureBuffer);
		Zenith_Assert(m_xExposureBuffer.GetBuffer().m_xVRAMHandle.IsValid(),
			"Flux_HDR: Failed to create exposure buffer");
	}

#ifdef ZENITH_TOOLS
	RegisterDebugVariables();
#endif

	// HDR's render targets are graph-owned transients: the shared scene target is
	// created by Flux_Graphics; the private bloom chain in HDR::SetupRenderGraph. No
	// resize callback needed — the graph re-creates transients on every
	// SetupRenderGraph pass, which is already a resize callback.

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_HDR Initialised");
}

void Flux_HDRImpl::Shutdown()
{
	m_pxGraph = nullptr;

	// Cleanup auto-exposure compute resources
	g_xEngine.FluxMemory().DestroyReadWriteBuffer(m_xHistogramBuffer);
	g_xEngine.FluxMemory().DestroyReadWriteBuffer(m_xExposureBuffer);

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
		g_xEngine.FluxMemory().UploadBufferData(
			m_xHistogramBuffer.GetBuffer().m_xVRAMHandle,
			auZeroHistogram,
			256 * sizeof(u_int));
	}

	// Reset exposure buffer to default values
	if (m_xExposureBuffer.GetBuffer().m_xVRAMHandle.IsValid())
	{
		// Seed exposure near a daylight value (0.4), not 1.0, so the scene is
		// well-exposed immediately and stays close across the smoke runner's
		// scene reloads (which re-fire this seed).
		float afInitialExposure[4] = { 0.4f, 0.4f, 0.4f, 0.0f };
		g_xEngine.FluxMemory().UploadBufferData(
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
	ZENITH_PROFILE_SCOPE("Flux HDR Histogram Setup");
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
		// Seed exposure near a daylight value (0.4), not 1.0, so the scene is
		// well-exposed immediately and stays close across the smoke runner's
		// scene reloads (which re-fire this seed).
		float afInitialExposure[4] = { 0.4f, 0.4f, 0.4f, 0.0f };
		g_xEngine.FluxMemory().UploadBufferData(
			xHDR.m_xExposureBuffer.GetBuffer().m_xVRAMHandle,
			afInitialExposure,
			4 * sizeof(float));
	}

	// Clear histogram buffer before compute (only when auto-exposure or histogram visualization is active)
	if ((bAutoExposure || dbg_bHDRShowHistogram) && xHDR.m_xHistogramBuffer.GetBuffer().m_xVRAMHandle.IsValid())
	{
		static u_int auZeroHistogram[256] = { 0 };
		g_xEngine.FluxMemory().UploadBufferData(
			xHDR.m_xHistogramBuffer.GetBuffer().m_xVRAMHandle,
			auZeroHistogram,
			256 * sizeof(u_int));
	}
}

static void ExecuteLuminanceHistogram(Flux_CommandBuffer* pxCommandList, void* pUserData)
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

	pxCommandList->BindComputePipeline(&xHDR.m_xLuminanceHistogramPipeline);

	LuminanceConstants xConsts;
	// RENDER dims: the histogram reads the RAW render-res HDR scene target (bloom/tonemap read the
	// upscaled post-FX colour instead). The coverage bound + UV normaliser must match that texture.
	xConsts.m_uImageWidth = g_xEngine.FluxGraphics().GetRenderWidth();
	xConsts.m_uImageHeight = g_xEngine.FluxGraphics().GetRenderHeight();
	xConsts.m_fMinLogLum = xHDR.m_fMinLogLuminance;
	xConsts.m_fLogLumRange = xHDR.m_fLogLuminanceRange;

	namespace LUM = Flux_Generated_HDR::HDR_Luminance;
	Flux_ShaderBinder xBinder(*pxCommandList);
	xBinder.BindDrawConstants(LUM::hLuminanceConstants, &xConsts, sizeof(xConsts));
	xBinder.BindSRV(LUM::hg_xHDRTex, &g_xEngine.FluxGraphics().GetHDRSceneTarget().SRV());
	xBinder.BindUAV_Buffer(LUM::hg_auHistogram, &xHDR.m_xHistogramBuffer.GetUAV());

	u_int uGroupsX = (g_xEngine.FluxGraphics().GetRenderWidth() + 15) / 16;
	u_int uGroupsY = (g_xEngine.FluxGraphics().GetRenderHeight() + 15) / 16;
	pxCommandList->Dispatch(uGroupsX, uGroupsY, 1);
}

static void ExecuteAdaptation(Flux_CommandBuffer* pxCommandList, void* pUserData)
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

	pxCommandList->BindComputePipeline(&xHDR.m_xAdaptationPipeline);

	AdaptationConstants xConsts;
	xConsts.m_fMinLogLum = xHDR.m_fMinLogLuminance;
	xConsts.m_fLogLumRange = xHDR.m_fLogLuminanceRange;
	xConsts.m_fDeltaTime = g_xEngine.Frame().GetDt();
	xConsts.m_fAdaptationSpeed = dbg_bHDRFreezeExposure ? 0.0f : xHDR.m_fAdaptationSpeed;
	xConsts.m_fTargetLuminance = xHDR.m_fTargetLuminance;
	xConsts.m_fMinExposure = xHDR.m_fMinExposure;
	xConsts.m_fMaxExposure = xHDR.m_fMaxExposure;
	xConsts.m_fLowPercentile = 0.05f;
	xConsts.m_fHighPercentile = 0.95f;
	// RENDER-res pixel count: the histogram bins were built over the render-res HDR scene, so the
	// percentile denominator must be the render-res pixel count (else auto-exposure metering skews).
	xConsts.m_uTotalPixels = g_xEngine.FluxGraphics().GetRenderWidth() * g_xEngine.FluxGraphics().GetRenderHeight();
	xConsts.m_uPad0 = 0;
	xConsts.m_uPad1 = 0;

	namespace ADP = Flux_Generated_HDR::HDR_Adaptation;
	Flux_ShaderBinder xBinder(*pxCommandList);
	xBinder.BindDrawConstants(ADP::hAdaptationConstants, &xConsts, sizeof(xConsts));
	xBinder.BindUAV_Buffer(ADP::hg_auHistogram, &xHDR.m_xHistogramBuffer.GetUAV());
	xBinder.BindUAV_Buffer(ADP::hg_afExposureData, &xHDR.m_xExposureBuffer.GetUAV());

	pxCommandList->Dispatch(1, 1, 1);
}

static void SubmitHistogramLabels()
{
	// Histogram position matches shader: bottom-left corner
	// fMargin = 0.02, fHistWidth = 0.3, fHistHeight = 0.15
	const float fScreenWidth = static_cast<float>(g_xEngine.FluxSwapchain().GetWidth());
	const float fScreenHeight = static_cast<float>(g_xEngine.FluxSwapchain().GetHeight());

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
	Zenith_Vector<Flux::Flux_TextEntry>& xTextEntries = Flux::Flux_TextQueue::GetPending();

	// Title above histogram
	{
		Flux::Flux_TextEntry xEntry;
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
		Flux::Flux_TextEntry xEntry;
		xEntry.m_strText = "Shadows";
		xEntry.m_xPosition = Zenith_Maths::Vector2(fHistLeft + fZoneWidth * 0.1f, fLabelY);
		xEntry.m_fSize = fLabelSize;
		xEntry.m_xColor = xGray;
		xTextEntries.PushBack(xEntry);
	}

	// Midtones label (middle third)
	{
		Flux::Flux_TextEntry xEntry;
		xEntry.m_strText = "Mids";
		xEntry.m_xPosition = Zenith_Maths::Vector2(fHistLeft + fZoneWidth * 1.3f, fLabelY);
		xEntry.m_fSize = fLabelSize;
		xEntry.m_xColor = xGray;
		xTextEntries.PushBack(xEntry);
	}

	// Highlights label (right third)
	{
		Flux::Flux_TextEntry xEntry;
		xEntry.m_strText = "Highs";
		xEntry.m_xPosition = Zenith_Maths::Vector2(fHistLeft + fZoneWidth * 2.3f, fLabelY);
		xEntry.m_fSize = fLabelSize;
		xEntry.m_xColor = xGray;
		xTextEntries.PushBack(xEntry);
	}
}

static void ExecuteBloomThreshold(Flux_CommandBuffer* pxCommandList, void* pUserData)
{
	(void)pUserData;

	// Non-capturing graph trampoline: recover this subsystem instance, then
	// route reach-ins through it and its injected member pointers.
	Flux_HDRImpl& xHDR = g_xEngine.HDR();

	// The pass's declared view slot selects this view's bloom chain + HDR scene
	// target (slot 0 = main/swapchain, preview = 512² → 256² base). Texel sizes
	// come from the per-view attachment, so no explicit viewport work is needed.
	const u_int uViewSlot = Flux_RenderGraph::GetCurrentRecordingPassViewSlot();

	Flux_RenderAttachment& xBloom0 = xHDR.GetBloomChainAttachment(0, uViewSlot);
	BloomConstants xBloomConsts;
	xBloomConsts.m_fThreshold = xHDR.m_fBloomThreshold;
	xBloomConsts.m_fIntensity = xHDR.m_fBloomIntensity;
	xBloomConsts.m_xTexelSize = Zenith_Maths::Vector2(1.0f / xBloom0.m_xSurfaceInfo.m_uWidth, 1.0f / xBloom0.m_xSurfaceInfo.m_uHeight);

	pxCommandList->SetPipeline(&xHDR.m_xBloomThresholdPipeline);
	pxCommandList->SetVertexBuffer(g_xEngine.FluxGraphics().m_xQuadMesh.GetVertexBuffer());
	pxCommandList->SetIndexBuffer(g_xEngine.FluxGraphics().m_xQuadMesh.GetIndexBuffer());

	namespace BT = Flux_Generated_HDR::BloomThreshold;
	Flux_ShaderBinder xBinder(*pxCommandList);
	xBinder.BindSRV(BT::hg_xHDRTex, &g_xEngine.FluxGraphics().GetSceneColourForPostFX(uViewSlot).SRV());
	xBinder.BindDrawConstants(BT::hBloomConstants, &xBloomConsts, sizeof(BloomConstants));

	pxCommandList->DrawIndexed(6);
}

static void ExecuteBloomDownsample(Flux_CommandBuffer* pxCommandList, void* pUserData)
{
	const u_int uMipIndex = *static_cast<const u_int*>(pUserData);

	// Non-capturing graph trampoline: recover this subsystem instance, then
	// route reach-ins through it and its injected member pointers.
	Flux_HDRImpl& xHDR = g_xEngine.HDR();

	// UserData carries the MIP only; the view comes from the recording pass's
	// declared slot (mirrors the HiZ per-view conversion).
	const u_int uViewSlot = Flux_RenderGraph::GetCurrentRecordingPassViewSlot();

	// The 13-tap downsample samples the SOURCE mip, so its tap offsets must be in
	// SOURCE-texel units. Using the destination (half-res) texel size spread the
	// taps 2x too far -> over-blurred + aliased bloom chain. Use the source mip.
	Flux_RenderAttachment& xSource = xHDR.GetBloomChainAttachment(uMipIndex - 1, uViewSlot);
	BloomConstants xBloomConsts;
	xBloomConsts.m_fThreshold = xHDR.m_fBloomThreshold;
	xBloomConsts.m_fIntensity = xHDR.m_fBloomIntensity;
	xBloomConsts.m_xTexelSize = Zenith_Maths::Vector2(1.0f / xSource.m_xSurfaceInfo.m_uWidth, 1.0f / xSource.m_xSurfaceInfo.m_uHeight);

	pxCommandList->SetPipeline(&xHDR.m_xBloomDownsamplePipeline);
	pxCommandList->SetVertexBuffer(g_xEngine.FluxGraphics().m_xQuadMesh.GetVertexBuffer());
	pxCommandList->SetIndexBuffer(g_xEngine.FluxGraphics().m_xQuadMesh.GetIndexBuffer());

	namespace BD = Flux_Generated_HDR::BloomDownsample;
	Flux_ShaderBinder xBinder(*pxCommandList);
	xBinder.BindSRV(BD::hg_xSourceTex, &xSource.SRV());
	xBinder.BindDrawConstants(BD::hBloomConstants, &xBloomConsts, sizeof(BloomConstants));

	pxCommandList->DrawIndexed(6);
}

static void ExecuteBloomUpsample(Flux_CommandBuffer* pxCommandList, void* pUserData)
{
	const u_int uIndex = *static_cast<const u_int*>(pUserData);
	const u_int uTargetMip = 3 - uIndex;
	const u_int uSourceMip = uTargetMip + 1;

	// Non-capturing graph trampoline: recover this subsystem instance, then
	// route reach-ins through it and its injected member pointers.
	Flux_HDRImpl& xHDR = g_xEngine.HDR();

	// UserData carries the upsample step only; the view comes from the recording
	// pass's declared slot (mirrors the HiZ per-view conversion).
	const u_int uViewSlot = Flux_RenderGraph::GetCurrentRecordingPassViewSlot();

	Flux_RenderAttachment& xTarget = xHDR.GetBloomChainAttachment(uTargetMip, uViewSlot);
	BloomConstants xBloomConsts;
	xBloomConsts.m_fThreshold = xHDR.m_fBloomThreshold;
	xBloomConsts.m_fIntensity = xHDR.m_fBloomIntensity;
	xBloomConsts.m_xTexelSize = Zenith_Maths::Vector2(1.0f / xTarget.m_xSurfaceInfo.m_uWidth, 1.0f / xTarget.m_xSurfaceInfo.m_uHeight);

	pxCommandList->SetPipeline(&xHDR.m_xBloomUpsamplePipeline);
	pxCommandList->SetVertexBuffer(g_xEngine.FluxGraphics().m_xQuadMesh.GetVertexBuffer());
	pxCommandList->SetIndexBuffer(g_xEngine.FluxGraphics().m_xQuadMesh.GetIndexBuffer());

	namespace BU = Flux_Generated_HDR::BloomUpsample;
	Flux_ShaderBinder xBinder(*pxCommandList);
	xBinder.BindSRV(BU::hg_xSourceTex, &xHDR.GetBloomChainAttachment(uSourceMip, uViewSlot).SRV());
	xBinder.BindDrawConstants(BU::hBloomConstants, &xBloomConsts, sizeof(BloomConstants));

	pxCommandList->DrawIndexed(6);
}

static void ExecuteToneMapping(Flux_CommandBuffer* pxCommandList, void* pUserData)
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

	pxCommandList->SetPipeline(&xHDR.m_xToneMappingPipeline);
	pxCommandList->SetVertexBuffer(g_xEngine.FluxGraphics().m_xQuadMesh.GetVertexBuffer());
	pxCommandList->SetIndexBuffer(g_xEngine.FluxGraphics().m_xQuadMesh.GetIndexBuffer());

	{
		namespace TM = Flux_Generated_HDR::HDR_ToneMapping;
		Flux_ShaderBinder xBinder(*pxCommandList);
		xBinder.BindSRV(TM::hg_xHDRTex, &g_xEngine.FluxGraphics().GetSceneColourForPostFX().SRV());
		xBinder.BindSRV(TM::hg_xBloomTex, &xHDR.GetBloomChainAttachment(0).SRV());
		// Slang reflection keys on the variable name (not the GLSL block
		// name) — match the names declared in Flux_ToneMapping.slang.
		xBinder.BindUAV_Buffer(TM::hg_auHistogram,   &xHDR.m_xHistogramBuffer.GetUAV());
		xBinder.BindUAV_Buffer(TM::hg_afExposureData, &xHDR.m_xExposureBuffer.GetUAV());
		xBinder.BindDrawConstants(TM::hToneMappingConstants, &xConsts, sizeof(ToneMappingConstants));
	}

	pxCommandList->DrawIndexed(6);

	// Submit histogram labels if ShowHistogram is enabled
	if (dbg_bHDRShowHistogram)
	{
		SubmitHistogramLabels();
	}
}

// Preview-view tonemap (S5a/S5c): FIXED exposure, no auto-exposure — the
// preview must read the same every frame regardless of the main scene's
// adaptation state. Bloom IS real (S5c): the preview owns its own bloom chain,
// whose mip 0 binds here with m_fBloomIntensity forwarded exactly like the main
// tonemap, so emissive materials glow in the preview. Reuses the tonemap
// pipeline (the persistent preview LDR is FINAL_RT_FORMAT); the
// histogram/exposure UAVs bind the shared buffers purely for descriptor
// validity (m_bAutoExposure=0 skips the read).
static void ExecutePreviewTonemap(Flux_CommandBuffer* pxCommandList, void*)
{
	Flux_HDRImpl& xHDR = g_xEngine.HDR();
	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();

	ToneMappingConstants xConsts;
	xConsts.m_fExposure = 1.0f;
	xConsts.m_fBloomIntensity = Zenith_GraphicsOptions::Get().m_bHDRBloomEnabled ? xHDR.m_fBloomIntensity : 0.0f;
	xConsts.m_uToneMappingOperator = static_cast<u_int>(xHDR.m_eToneMappingOperator);
	xConsts.m_uDebugMode = 0;
	xConsts.m_bShowHistogram = 0;
	xConsts.m_bAutoExposure = 0;
	xConsts.m_uPad0 = 0;
	xConsts.m_uPad1 = 0;

	pxCommandList->SetPipeline(&xHDR.m_xToneMappingPipeline);
	pxCommandList->SetVertexBuffer(xGraphics.m_xQuadMesh.GetVertexBuffer());
	pxCommandList->SetIndexBuffer(xGraphics.m_xQuadMesh.GetIndexBuffer());

	{
		namespace TM = Flux_Generated_HDR::HDR_ToneMapping;
		Flux_ShaderBinder xBinder(*pxCommandList);
		xBinder.BindSRV(TM::hg_xHDRTex, &xGraphics.GetHDRSceneTarget(kuFluxViewSlotPreview).SRV());
		xBinder.BindSRV(TM::hg_xBloomTex, &xHDR.GetBloomChainAttachment(0, kuFluxViewSlotPreview).SRV());
		xBinder.BindUAV_Buffer(TM::hg_auHistogram,   &xHDR.m_xHistogramBuffer.GetUAV());
		xBinder.BindUAV_Buffer(TM::hg_afExposureData, &xHDR.m_xExposureBuffer.GetUAV());
		xBinder.BindDrawConstants(TM::hToneMappingConstants, &xConsts, sizeof(ToneMappingConstants));
	}

	pxCommandList->DrawIndexed(6);
}

// No-op record: the .Reads(m_xPreviewLDR) declaration makes the graph leave the
// persistent preview LDR in SHADER_READ_ONLY for the editor's ImGui sample
// (mirrors the Present feature's final-RT layout-transition pass).
static void ExecutePreviewLDRTransition(Flux_CommandBuffer*, void*)
{
}

// Per-view bloom chain: creates one view's 5-mip transients (half the view's
// dims at the base) then declares its threshold / downsample / upsample pass
// chain against that view's HDR scene target. Pass names must be per-view
// unique + static-lifetime (duplicate names are a hard assert): view 0 keeps
// the historical names (profiling / FindPass stability); the preview gets its
// own " (Preview)" tables. The executes derive the view from the recording
// pass's slot (declared via .View below), so the per-mip UserData stays
// mip-only — mirrors the HiZ per-view conversion.
void Flux_HDRImpl::SetupBloomViewPasses(Flux_RenderGraph& xGraph, u_int uViewSlot, u_int uWidth, u_int uHeight)
{
	u_int uBloomWidth = uWidth / 2;
	u_int uBloomHeight = uHeight / 2;

	for (u_int i = 0; i < uHDR_BLOOM_MIP_COUNT; i++)
	{
		Flux_TransientTextureDesc xDesc;
		xDesc.m_uWidth = uBloomWidth;
		xDesc.m_uHeight = uBloomHeight;
		xDesc.m_eFormat = BLOOM_FORMAT;
		xDesc.m_uMemoryFlags = (1u << MEMORY_FLAGS__SHADER_READ);
		m_aaxBloomChainHandles[uViewSlot][i] = xGraph.CreateTransient(xDesc);

		uBloomWidth = std::max(1u, uBloomWidth / 2);
		uBloomHeight = std::max(1u, uBloomHeight / 2);
	}

	const bool bMainView = (uViewSlot == kuFluxViewSlotMain);

	xGraph.AddPass(bMainView ? "HDR_BloomThreshold" : "HDR_BloomThreshold (Preview)", ExecuteBloomThreshold)
		.View(uViewSlot)
		.ClearTargets()
		.Reads          (g_xEngine.FluxGraphics().GetSceneColourForPostFX(uViewSlot),       RESOURCE_ACCESS_READ_SRV)
		.WritesTransient(m_aaxBloomChainHandles[uViewSlot][0],        RESOURCE_ACCESS_WRITE_RTV);

	static const char* s_aszBloomDownsampleNames[] = {
		"HDR_BloomDownsample Mip1", "HDR_BloomDownsample Mip2",
		"HDR_BloomDownsample Mip3", "HDR_BloomDownsample Mip4",
	};
	static const char* s_aszBloomDownsamplePreviewNames[] = {
		"HDR_BloomDownsample Mip1 (Preview)", "HDR_BloomDownsample Mip2 (Preview)",
		"HDR_BloomDownsample Mip3 (Preview)", "HDR_BloomDownsample Mip4 (Preview)",
	};
	const char* const* pszDownsampleNames = bMainView ? s_aszBloomDownsampleNames : s_aszBloomDownsamplePreviewNames;
	for (u_int i = 1; i < uHDR_BLOOM_MIP_COUNT; i++)
	{
		xGraph.AddPass(pszDownsampleNames[i - 1], ExecuteBloomDownsample, &m_axBloomMipUserData[i])
			.View(uViewSlot)
			.ClearTargets()
			.ReadsTransient (m_aaxBloomChainHandles[uViewSlot][i - 1], RESOURCE_ACCESS_READ_SRV)
			.WritesTransient(m_aaxBloomChainHandles[uViewSlot][i],     RESOURCE_ACCESS_WRITE_RTV);
	}

	static const char* s_aszBloomUpsampleNames[] = {
		"HDR_BloomUpsample Mip3", "HDR_BloomUpsample Mip2",
		"HDR_BloomUpsample Mip1", "HDR_BloomUpsample Mip0",
	};
	static const char* s_aszBloomUpsamplePreviewNames[] = {
		"HDR_BloomUpsample Mip3 (Preview)", "HDR_BloomUpsample Mip2 (Preview)",
		"HDR_BloomUpsample Mip1 (Preview)", "HDR_BloomUpsample Mip0 (Preview)",
	};
	const char* const* pszUpsampleNames = bMainView ? s_aszBloomUpsampleNames : s_aszBloomUpsamplePreviewNames;
	for (u_int i = 0; i < uHDR_BLOOM_MIP_COUNT - 1; i++)
	{
		const u_int uTargetMip = 3 - i;
		const u_int uSourceMip = uTargetMip + 1;
		xGraph.AddPass(pszUpsampleNames[i], ExecuteBloomUpsample, &m_axBloomUpsampleUserData[i])
			.View(uViewSlot)
			.ReadsTransient (m_aaxBloomChainHandles[uViewSlot][uSourceMip], RESOURCE_ACCESS_READ_SRV)
			.WritesTransient(m_aaxBloomChainHandles[uViewSlot][uTargetMip], RESOURCE_ACCESS_WRITE_RTV);
	}
}

void Flux_HDRImpl::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	m_pxGraph = &xGraph;

	// The HDR scene target is created + owned by Flux_Graphics (a shared target,
	// created before the first writer); HDR creates only its private per-view
	// bloom chains here (SetupBloomViewPasses), then declares its bloom /
	// tonemap / exposure passes.

	xGraph.AddPass("HDR_LuminanceHistogram", ExecuteLuminanceHistogram)
		.Prepare    (PreExecuteLuminanceHistogram)
		.Reads      (g_xEngine.FluxGraphics().GetHDRSceneTarget(),                     RESOURCE_ACCESS_READ_SRV)
		.WritesBuffer(m_xHistogramBuffer.GetBuffer(),          RESOURCE_ACCESS_WRITE_UAV);

	// Histogram is bound as UAV_Buffer (see ExecuteAdaptation); declare as
	// READWRITE_UAV so the graph barrier matches the compute-stage access.
	xGraph.AddPass("HDR_Adaptation", ExecuteAdaptation)
		.ReadsBuffer (m_xHistogramBuffer.GetBuffer(), RESOURCE_ACCESS_READWRITE_UAV)
		.WritesBuffer(m_xExposureBuffer.GetBuffer(),  RESOURCE_ACCESS_WRITE_UAV);

	// Main-view bloom chain — half swapchain dims, historical pass names.
	SetupBloomViewPasses(xGraph, kuFluxViewSlotMain,
		g_xEngine.FluxSwapchain().GetWidth(), g_xEngine.FluxSwapchain().GetHeight());

	// Tone mapping samples exposure (and optionally histogram for a debug
	// overlay). Both are bound as UAV_Buffer in ExecuteToneMapping so declare
	// as READWRITE_UAV — the barrier must match the binding's access mode
	// rather than the shader's actual read-only intent.
	xGraph.AddPass("HDR_ToneMapping", ExecuteToneMapping)
		.ClearTargets()
		.Reads         (g_xEngine.FluxGraphics().GetSceneColourForPostFX(),              RESOURCE_ACCESS_READ_SRV)
		.Writes        (g_xEngine.FluxGraphics().GetFinalRenderTarget(),  RESOURCE_ACCESS_WRITE_RTV)
		.ReadsBuffer   (m_xHistogramBuffer.GetBuffer(),         RESOURCE_ACCESS_READWRITE_UAV)
		.ReadsBuffer   (m_xExposureBuffer.GetBuffer(),          RESOURCE_ACCESS_READWRITE_UAV)
		.ReadsTransient(m_aaxBloomChainHandles[kuFluxViewSlotMain][0], RESOURCE_ACCESS_READ_SRV);

	// Preview view (S5a/S5c): the preview's own bloom chain (256² base) + a
	// fixed-exposure tonemap of the preview HDR into the persistent preview LDR
	// (now reading the preview bloom mip 0 — emissive materials glow) + a no-op
	// reader pass that leaves the LDR in SHADER_READ_ONLY for the editor's
	// ImGui sample. The histogram/exposure UAV binds are descriptor-validity
	// only (auto-exposure off) but must be declared so the graph's bind
	// validator + barriers stay honest.
	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();
	if (xGraphics.RenderViews().IsViewActive(kuFluxViewSlotPreview))
	{
		SetupBloomViewPasses(xGraph, kuFluxViewSlotPreview, kuFLUX_PREVIEW_VIEW_SIZE, kuFLUX_PREVIEW_VIEW_SIZE);

		xGraph.AddPass("HDR_ToneMapping (Preview)", ExecutePreviewTonemap)
			.View(kuFluxViewSlotPreview)
			.ClearTargets()
			.Reads         (xGraphics.GetHDRSceneTarget(kuFluxViewSlotPreview),        RESOURCE_ACCESS_READ_SRV)
			.Writes        (xGraphics.GetPreviewLDR(),                                 RESOURCE_ACCESS_WRITE_RTV)
			.ReadsBuffer   (m_xHistogramBuffer.GetBuffer(),                            RESOURCE_ACCESS_READWRITE_UAV)
			.ReadsBuffer   (m_xExposureBuffer.GetBuffer(),                             RESOURCE_ACCESS_READWRITE_UAV)
			.ReadsTransient(m_aaxBloomChainHandles[kuFluxViewSlotPreview][0],          RESOURCE_ACCESS_READ_SRV);

		xGraph.AddPass("Preview LDR Transition", ExecutePreviewLDRTransition)
			.Reads(xGraphics.GetPreviewLDR(), RESOURCE_ACCESS_READ_SRV);
	}
}

// GetHDRSceneTarget / GetHDRSceneSRV / GetHDRSceneTargetSetup{,WithDepth} moved to
// Flux_GraphicsImpl — the HDR scene target is a shared Flux_Graphics-owned transient
// now. HDR reads it via g_xEngine.FluxGraphics().GetHDRSceneTarget().



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

Flux_RenderAttachment& Flux_HDRImpl::GetBloomChainAttachment(u_int uIndex, u_int uViewSlot)
{
	Zenith_Assert(uIndex < uHDR_BLOOM_MIP_COUNT, "Flux_HDRImpl::GetBloomChainAttachment: index %u out of range", uIndex);
	Zenith_Assert(uViewSlot < FLUX_MAX_RENDER_VIEWS, "Flux_HDRImpl::GetBloomChainAttachment: view slot %u out of range", uViewSlot);
	Zenith_Assert(m_pxGraph, "Flux_HDRImpl::GetBloomChainAttachment: graph pointer is null");
	return m_pxGraph->GetTransientAttachment(m_aaxBloomChainHandles[uViewSlot][uIndex]);
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
	auto& xEngine = g_xEngine;
	xEngine.DebugVariables().AddUInt32({ "Flux", "HDR", "DebugMode" }, dbg_uHDRDebugMode, 0, HDR_DEBUG_COUNT - 1);
	xEngine.DebugVariables().AddFloat({ "Flux", "HDR", "Exposure" }, dbg_fHDRExposure, 0.01f, 10.0f);
	xEngine.DebugVariables().AddFloat({ "Flux", "HDR", "BloomIntensity" }, dbg_fHDRBloomIntensity, 0.0f, 2.0f);
	xEngine.DebugVariables().AddFloat({ "Flux", "HDR", "BloomThreshold" }, dbg_fHDRBloomThreshold, 0.0f, 5.0f);
	xEngine.DebugVariables().AddUInt32({ "Flux", "HDR", "ToneMappingOperator" }, dbg_uHDRToneMappingOperator, 0, TONEMAPPING_COUNT - 1);
	xEngine.DebugVariables().AddBoolean({ "Flux", "HDR", "ShowHistogram" }, dbg_bHDRShowHistogram);
	xEngine.DebugVariables().AddBoolean({ "Flux", "HDR", "FreezeExposure" }, dbg_bHDRFreezeExposure);
	xEngine.DebugVariables().AddFloat({ "Flux", "HDR", "AdaptationSpeed" }, dbg_fHDRAdaptationSpeed, 0.1f, 10.0f);
	xEngine.DebugVariables().AddFloat({ "Flux", "HDR", "TargetLuminance" }, dbg_fHDRTargetLuminance, 0.01f, 1.0f);
	xEngine.DebugVariables().AddFloat({ "Flux", "HDR", "MinExposure" }, dbg_fHDRMinExposure, 0.01f, 1.0f);
	xEngine.DebugVariables().AddFloat({ "Flux", "HDR", "MaxExposure" }, dbg_fHDRMaxExposure, 1.0f, 20.0f);
}

// Debug-SRV callbacks — mirror g_xEngine.FluxGraphics().GetDebugSRV_MRTDiffuse. Register
// via g_xEngine.DebugVariables().AddTextureCallback in Flux::LateInitialise; the
// callback is invoked on every ImGui draw so the preview tracks transient
// rebuilds on resize without leaving a dangling SRV.
const Flux_ShaderResourceView* Flux_HDRImpl::GetDebugSRV_Bloom0()
{
	if (m_pxGraph == nullptr) return nullptr;
	return &m_pxGraph->GetTransientAttachment(m_aaxBloomChainHandles[kuFluxViewSlotMain][0]).SRV();
}
const Flux_ShaderResourceView* Flux_HDRImpl::GetDebugSRV_Bloom1()
{
	if (m_pxGraph == nullptr) return nullptr;
	return &m_pxGraph->GetTransientAttachment(m_aaxBloomChainHandles[kuFluxViewSlotMain][1]).SRV();
}
const Flux_ShaderResourceView* Flux_HDRImpl::GetDebugSRV_Bloom2()
{
	if (m_pxGraph == nullptr) return nullptr;
	return &m_pxGraph->GetTransientAttachment(m_aaxBloomChainHandles[kuFluxViewSlotMain][2]).SRV();
}
#endif
