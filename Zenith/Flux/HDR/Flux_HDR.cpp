#include "Zenith.h"

#include <atomic>
#include "Flux/HDR/Flux_HDRImpl.h"
#include "Core/Zenith_Engine.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "UI/Zenith_UICanvas.h"

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
	g_xEngine.HDR().m_fExposure = dbg_fHDRExposure;
	g_xEngine.HDR().m_fBloomIntensity = dbg_fHDRBloomIntensity;
	g_xEngine.HDR().m_fBloomThreshold = dbg_fHDRBloomThreshold;
	g_xEngine.HDR().m_eToneMappingOperator = static_cast<ToneMappingOperator>(dbg_uHDRToneMappingOperator);
	g_xEngine.HDR().m_fAdaptationSpeed = dbg_fHDRAdaptationSpeed;
	g_xEngine.HDR().m_fTargetLuminance = dbg_fHDRTargetLuminance;
	g_xEngine.HDR().m_fMinExposure = dbg_fHDRMinExposure;
	g_xEngine.HDR().m_fMaxExposure = dbg_fHDRMaxExposure;
}

void Flux_HDRImpl::BuildPipelines()
{
	// Tone mapping — Slang-registered (HDR_ToneMapping). Five tone curves
	// + debug overlays in a single fragment program.
	Flux_PipelineHelper::BuildFullscreenPipeline(
		g_xEngine.HDR().m_xToneMappingShader, g_xEngine.HDR().m_xToneMappingPipeline,
		FluxShaderProgram::HDR_ToneMapping, FINAL_RT_FORMAT);

	// Bloom passes — all migrated to the Slang shader registry. They share
	// Common.Fullscreen for vsMain so a single .slang module per pass holds
	// both stages.
	Flux_PipelineHelper::BuildFullscreenPipeline(
		g_xEngine.HDR().m_xBloomThresholdShader, g_xEngine.HDR().m_xBloomThresholdPipeline,
		FluxShaderProgram::BloomThreshold, BLOOM_FORMAT);

	Flux_PipelineHelper::BuildFullscreenPipeline(
		g_xEngine.HDR().m_xBloomDownsampleShader, g_xEngine.HDR().m_xBloomDownsamplePipeline,
		FluxShaderProgram::BloomDownsample, BLOOM_FORMAT);

	{
		Flux_PipelineSpecification xSpec = Flux_PipelineHelper::CreateFullscreenSpec(
			g_xEngine.HDR().m_xBloomUpsampleShader, FluxShaderProgram::BloomUpsample, BLOOM_FORMAT);
		xSpec.m_axBlendStates[0].m_bBlendEnabled = true;
		xSpec.m_axBlendStates[0].m_eSrcBlendFactor = BLEND_FACTOR_ONE;
		xSpec.m_axBlendStates[0].m_eDstBlendFactor = BLEND_FACTOR_ONE;
		Flux_PipelineBuilder::FromSpecification(g_xEngine.HDR().m_xBloomUpsamplePipeline, xSpec);
	}

	// Auto-exposure compute pipelines — shader Initialise + RootSig +
	// ComputePipelineBuilder. The VRAM buffers (g_xEngine.HDR().m_xHistogramBuffer /
	// g_xEngine.HDR().m_xExposureBuffer) live in Initialise and are not touched here.
	g_xEngine.HDR().m_xLuminanceHistogramShader.Initialise(FluxShaderProgram::HDR_Luminance);
	Flux_RootSigBuilder::FromReflection(g_xEngine.HDR().m_xLuminanceRootSig, g_xEngine.HDR().m_xLuminanceHistogramShader.GetReflection());
	Flux_ComputePipelineBuilder::BuildFromShader(g_xEngine.HDR().m_xLuminanceHistogramPipeline,
		g_xEngine.HDR().m_xLuminanceHistogramShader, g_xEngine.HDR().m_xLuminanceRootSig);

	g_xEngine.HDR().m_xAdaptationShader.Initialise(FluxShaderProgram::HDR_Adaptation);
	Flux_RootSigBuilder::FromReflection(g_xEngine.HDR().m_xAdaptationRootSig, g_xEngine.HDR().m_xAdaptationShader.GetReflection());
	Flux_ComputePipelineBuilder::BuildFromShader(g_xEngine.HDR().m_xAdaptationPipeline,
		g_xEngine.HDR().m_xAdaptationShader, g_xEngine.HDR().m_xAdaptationRootSig);
}

static void PreExecuteLuminanceHistogram(void* pUserData);
static void ExecuteLuminanceHistogram(Flux_CommandList* pxCommandList, void* pUserData);
static void ExecuteAdaptation(Flux_CommandList* pxCommandList, void* pUserData);
static void ExecuteBloomThreshold(Flux_CommandList* pxCommandList, void* pUserData);
static void ExecuteBloomDownsample(Flux_CommandList* pxCommandList, void* pUserData);
static void ExecuteBloomUpsample(Flux_CommandList* pxCommandList, void* pUserData);
static void ExecuteToneMapping(Flux_CommandList* pxCommandList, void* pUserData);

void Flux_HDRImpl::Initialise()
{
	BuildPipelines();

	// One-time setup that hot-reload must NOT repeat (would leak VRAM /
	// double-register debug variables).
	{
		// Histogram buffer (256 bins of u_int).
		u_int auZeroHistogram[256] = { 0 };
		Flux_MemoryManager::InitialiseReadWriteBuffer(auZeroHistogram, 256 * sizeof(u_int), g_xEngine.HDR().m_xHistogramBuffer);
		Zenith_Assert(g_xEngine.HDR().m_xHistogramBuffer.GetBuffer().m_xVRAMHandle.IsValid(),
			"Flux_HDR: Failed to create histogram buffer");

		// Exposure buffer (4 floats: avgLum, currentExp, targetExp, pad).
		float afInitialExposure[4] = { 0.18f, 1.0f, 1.0f, 0.0f };
		Flux_MemoryManager::InitialiseReadWriteBuffer(afInitialExposure, 4 * sizeof(float), g_xEngine.HDR().m_xExposureBuffer);
		Zenith_Assert(g_xEngine.HDR().m_xExposureBuffer.GetBuffer().m_xVRAMHandle.IsValid(),
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
	g_xEngine.HDR().m_pxGraph = nullptr;

	// Cleanup auto-exposure compute resources
	Flux_MemoryManager::DestroyReadWriteBuffer(g_xEngine.HDR().m_xHistogramBuffer);
	Flux_MemoryManager::DestroyReadWriteBuffer(g_xEngine.HDR().m_xExposureBuffer);

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_HDR shut down");
}

void Flux_HDRImpl::Reset()
{
	g_xEngine.HDR().m_fCurrentExposure = 1.0f;
	g_xEngine.HDR().m_fAverageLuminance = 0.18f;

	// Clear histogram buffer to prevent stale data after scene transitions
	// This ensures auto-exposure starts fresh when enabled
	if (g_xEngine.HDR().m_xHistogramBuffer.GetBuffer().m_xVRAMHandle.IsValid())
	{
		static u_int auZeroHistogram[256] = { 0 };
		Flux_MemoryManager::UploadBufferData(
			g_xEngine.HDR().m_xHistogramBuffer.GetBuffer().m_xVRAMHandle,
			auZeroHistogram,
			256 * sizeof(u_int));
	}

	// Reset exposure buffer to default values
	if (g_xEngine.HDR().m_xExposureBuffer.GetBuffer().m_xVRAMHandle.IsValid())
	{
		float afInitialExposure[4] = { 0.18f, 1.0f, 1.0f, 0.0f };
		Flux_MemoryManager::UploadBufferData(
			g_xEngine.HDR().m_xExposureBuffer.GetBuffer().m_xVRAMHandle,
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

	// SYNC NOTE: UploadBufferData() below stages into the memory transfer
	// command buffer. EndFrame() submits that CB and signals s_xMemorySemaphore,
	// which the subsequent render submission waits on, so any uploads issued
	// from a Phase-0 OnPrepare callback are guaranteed to be visible before any
	// pass that reads them. This replaces the old explicit memory-update
	// ordering token that the static_assert in EndFrame used to enforce.
	g_xEngine.HDR().SyncDebugVariables();
	const bool bAutoExposure = Zenith_GraphicsOptions::Get().m_bHDRAutoExposureEnabled;
	bool bAutoExposureJustEnabled = bAutoExposure && !g_xEngine.HDR().m_bAutoExposureWasEnabled;
	g_xEngine.HDR().m_bAutoExposureWasEnabled = bAutoExposure;
	if (bAutoExposureJustEnabled && g_xEngine.HDR().m_xExposureBuffer.GetBuffer().m_xVRAMHandle.IsValid())
	{
		float afInitialExposure[4] = { 0.18f, 1.0f, 1.0f, 0.0f };
		Flux_MemoryManager::UploadBufferData(
			g_xEngine.HDR().m_xExposureBuffer.GetBuffer().m_xVRAMHandle,
			afInitialExposure,
			4 * sizeof(float));
	}

	// Clear histogram buffer before compute (only when auto-exposure or histogram visualization is active)
	if ((bAutoExposure || dbg_bHDRShowHistogram) && g_xEngine.HDR().m_xHistogramBuffer.GetBuffer().m_xVRAMHandle.IsValid())
	{
		static u_int auZeroHistogram[256] = { 0 };
		Flux_MemoryManager::UploadBufferData(
			g_xEngine.HDR().m_xHistogramBuffer.GetBuffer().m_xVRAMHandle,
			auZeroHistogram,
			256 * sizeof(u_int));
	}
}

static void ExecuteLuminanceHistogram(Flux_CommandList* pxCommandList, void* pUserData)
{
	(void)pUserData;

	if (!g_xEngine.HDR().m_xHistogramBuffer.GetBuffer().m_xVRAMHandle.IsValid())
	{
		return;
	}

	// Buffer uploads are handled in PreExecuteLuminanceHistogram

	pxCommandList->AddCommand<Flux_CommandBindComputePipeline>(&g_xEngine.HDR().m_xLuminanceHistogramPipeline);

	LuminanceConstants xConsts;
	xConsts.m_uImageWidth = Flux_Swapchain::GetWidth();
	xConsts.m_uImageHeight = Flux_Swapchain::GetHeight();
	xConsts.m_fMinLogLum = g_xEngine.HDR().m_fMinLogLuminance;
	xConsts.m_fLogLumRange = g_xEngine.HDR().m_fLogLuminanceRange;

	Flux_ShaderBinder xBinder(*pxCommandList);
	xBinder.BindDrawConstants(g_xEngine.HDR().m_xLuminanceHistogramShader, "LuminanceConstants", &xConsts, sizeof(xConsts));
	xBinder.BindSRV(g_xEngine.HDR().m_xLuminanceHistogramShader, "g_xHDRTex", &g_xEngine.HDR().GetHDRSceneTarget().SRV());
	xBinder.BindUAV_Buffer(g_xEngine.HDR().m_xLuminanceHistogramShader, "g_auHistogram", &g_xEngine.HDR().m_xHistogramBuffer.GetUAV());

	u_int uGroupsX = (Flux_Swapchain::GetWidth() + 15) / 16;
	u_int uGroupsY = (Flux_Swapchain::GetHeight() + 15) / 16;
	pxCommandList->AddCommand<Flux_CommandDispatch>(uGroupsX, uGroupsY, 1);
}

static void ExecuteAdaptation(Flux_CommandList* pxCommandList, void* pUserData)
{
	(void)pUserData;
	if (!g_xEngine.HDR().m_xHistogramBuffer.GetBuffer().m_xVRAMHandle.IsValid() ||
		!g_xEngine.HDR().m_xExposureBuffer.GetBuffer().m_xVRAMHandle.IsValid())
	{
		return;
	}

	pxCommandList->AddCommand<Flux_CommandBindComputePipeline>(&g_xEngine.HDR().m_xAdaptationPipeline);

	AdaptationConstants xConsts;
	xConsts.m_fMinLogLum = g_xEngine.HDR().m_fMinLogLuminance;
	xConsts.m_fLogLumRange = g_xEngine.HDR().m_fLogLuminanceRange;
	xConsts.m_fDeltaTime = g_xEngine.Frame().GetDt();
	xConsts.m_fAdaptationSpeed = dbg_bHDRFreezeExposure ? 0.0f : g_xEngine.HDR().m_fAdaptationSpeed;
	xConsts.m_fTargetLuminance = g_xEngine.HDR().m_fTargetLuminance;
	xConsts.m_fMinExposure = g_xEngine.HDR().m_fMinExposure;
	xConsts.m_fMaxExposure = g_xEngine.HDR().m_fMaxExposure;
	xConsts.m_fLowPercentile = 0.05f;
	xConsts.m_fHighPercentile = 0.95f;
	xConsts.m_uTotalPixels = Flux_Swapchain::GetWidth() * Flux_Swapchain::GetHeight();
	xConsts.m_uPad0 = 0;
	xConsts.m_uPad1 = 0;

	Flux_ShaderBinder xBinder(*pxCommandList);
	xBinder.BindDrawConstants(g_xEngine.HDR().m_xAdaptationShader, "AdaptationConstants", &xConsts, sizeof(xConsts));
	xBinder.BindUAV_Buffer(g_xEngine.HDR().m_xAdaptationShader, "g_auHistogram", &g_xEngine.HDR().m_xHistogramBuffer.GetUAV());
	xBinder.BindUAV_Buffer(g_xEngine.HDR().m_xAdaptationShader, "g_afExposureData", &g_xEngine.HDR().m_xExposureBuffer.GetUAV());

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

static void ExecuteBloomThreshold(Flux_CommandList* pxCommandList, void* pUserData)
{
	(void)pUserData;
	Flux_RenderAttachment& xBloom0 = g_xEngine.HDR().GetBloomChainAttachment(0);
	BloomConstants xBloomConsts;
	xBloomConsts.m_fThreshold = g_xEngine.HDR().m_fBloomThreshold;
	xBloomConsts.m_fIntensity = g_xEngine.HDR().m_fBloomIntensity;
	xBloomConsts.m_xTexelSize = Zenith_Maths::Vector2(1.0f / xBloom0.m_xSurfaceInfo.m_uWidth, 1.0f / xBloom0.m_xSurfaceInfo.m_uHeight);

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&g_xEngine.HDR().m_xBloomThresholdPipeline);
	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);
	xBinder.BindSRV(g_xEngine.HDR().m_xBloomThresholdShader, "g_xHDRTex", &g_xEngine.HDR().GetHDRSceneTarget().SRV());
	xBinder.BindDrawConstants(g_xEngine.HDR().m_xBloomThresholdShader, "BloomConstants", &xBloomConsts, sizeof(BloomConstants));

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
}

static void ExecuteBloomDownsample(Flux_CommandList* pxCommandList, void* pUserData)
{
	const u_int uMipIndex = *static_cast<const u_int*>(pUserData);

	Flux_RenderAttachment& xTarget = g_xEngine.HDR().GetBloomChainAttachment(uMipIndex);
	BloomConstants xBloomConsts;
	xBloomConsts.m_fThreshold = g_xEngine.HDR().m_fBloomThreshold;
	xBloomConsts.m_fIntensity = g_xEngine.HDR().m_fBloomIntensity;
	xBloomConsts.m_xTexelSize = Zenith_Maths::Vector2(1.0f / xTarget.m_xSurfaceInfo.m_uWidth, 1.0f / xTarget.m_xSurfaceInfo.m_uHeight);

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&g_xEngine.HDR().m_xBloomDownsamplePipeline);
	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);
	xBinder.BindSRV(g_xEngine.HDR().m_xBloomDownsampleShader, "g_xSourceTex", &g_xEngine.HDR().GetBloomChainAttachment(uMipIndex - 1).SRV());
	xBinder.BindDrawConstants(g_xEngine.HDR().m_xBloomDownsampleShader, "BloomConstants", &xBloomConsts, sizeof(BloomConstants));

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
}

static void ExecuteBloomUpsample(Flux_CommandList* pxCommandList, void* pUserData)
{
	const u_int uIndex = *static_cast<const u_int*>(pUserData);
	const u_int uTargetMip = 3 - uIndex;
	const u_int uSourceMip = uTargetMip + 1;

	Flux_RenderAttachment& xTarget = g_xEngine.HDR().GetBloomChainAttachment(uTargetMip);
	BloomConstants xBloomConsts;
	xBloomConsts.m_fThreshold = g_xEngine.HDR().m_fBloomThreshold;
	xBloomConsts.m_fIntensity = g_xEngine.HDR().m_fBloomIntensity;
	xBloomConsts.m_xTexelSize = Zenith_Maths::Vector2(1.0f / xTarget.m_xSurfaceInfo.m_uWidth, 1.0f / xTarget.m_xSurfaceInfo.m_uHeight);

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&g_xEngine.HDR().m_xBloomUpsamplePipeline);
	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);
	xBinder.BindSRV(g_xEngine.HDR().m_xBloomUpsampleShader, "g_xSourceTex", &g_xEngine.HDR().GetBloomChainAttachment(uSourceMip).SRV());
	xBinder.BindDrawConstants(g_xEngine.HDR().m_xBloomUpsampleShader, "BloomConstants", &xBloomConsts, sizeof(BloomConstants));

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
}

static void ExecuteToneMapping(Flux_CommandList* pxCommandList, void* pUserData)
{
	(void)pUserData;
	ToneMappingConstants xConsts;
	xConsts.m_fExposure = g_xEngine.HDR().m_fExposure;
	xConsts.m_fBloomIntensity = Zenith_GraphicsOptions::Get().m_bHDRBloomEnabled ? g_xEngine.HDR().m_fBloomIntensity : 0.0f;
	xConsts.m_uToneMappingOperator = static_cast<u_int>(g_xEngine.HDR().m_eToneMappingOperator);
	xConsts.m_uDebugMode = dbg_uHDRDebugMode;
	xConsts.m_bShowHistogram = dbg_bHDRShowHistogram ? 1 : 0;
	xConsts.m_bAutoExposure = Zenith_GraphicsOptions::Get().m_bHDRAutoExposureEnabled ? 1 : 0;
	xConsts.m_uPad0 = 0;
	xConsts.m_uPad1 = 0;

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&g_xEngine.HDR().m_xToneMappingPipeline);
	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetIndexBuffer());

	{
		Flux_ShaderBinder xBinder(*pxCommandList);
		xBinder.BindSRV(g_xEngine.HDR().m_xToneMappingShader, "g_xHDRTex", &g_xEngine.HDR().GetHDRSceneTarget().SRV());
		xBinder.BindSRV(g_xEngine.HDR().m_xToneMappingShader, "g_xBloomTex", &g_xEngine.HDR().GetBloomChainAttachment(0).SRV());
		// Slang reflection keys on the variable name (not the GLSL block
		// name) — match the names declared in Flux_ToneMapping.slang.
		xBinder.BindUAV_Buffer(g_xEngine.HDR().m_xToneMappingShader, "g_auHistogram",   &g_xEngine.HDR().m_xHistogramBuffer.GetUAV());
		xBinder.BindUAV_Buffer(g_xEngine.HDR().m_xToneMappingShader, "g_afExposureData", &g_xEngine.HDR().m_xExposureBuffer.GetUAV());
		xBinder.BindDrawConstants(g_xEngine.HDR().m_xToneMappingShader, "ToneMappingConstants", &xConsts, sizeof(ToneMappingConstants));
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
	g_xEngine.HDR().m_pxGraph = &xGraph;

	// Create transient HDR scene target early — many subsystems write to
	// g_xEngine.HDR().GetHDRSceneTarget() during their SetupRenderGraph, so the handle must
	// exist before they run.
	Flux_TransientTextureDesc xHDRDesc;
	xHDRDesc.m_uWidth = Flux_Swapchain::GetWidth();
	xHDRDesc.m_uHeight = Flux_Swapchain::GetHeight();
	xHDRDesc.m_eFormat = HDR_SCENE_FORMAT;
	xHDRDesc.m_uMemoryFlags = (1u << MEMORY_FLAGS__SHADER_READ);
	g_xEngine.HDR().m_xHDRSceneTargetHandle = xGraph.CreateTransient(xHDRDesc);
}

void Flux_HDRImpl::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	// g_xEngine.HDR().m_pxGraph already set by SetupTransients(). HDR scene target transient
	// already created there too.

	u_int uBloomWidth = Flux_Swapchain::GetWidth() / 2;
	u_int uBloomHeight = Flux_Swapchain::GetHeight() / 2;

	for (u_int i = 0; i < 5; i++)
	{
		Flux_TransientTextureDesc xDesc;
		xDesc.m_uWidth = uBloomWidth;
		xDesc.m_uHeight = uBloomHeight;
		xDesc.m_eFormat = BLOOM_FORMAT;
		xDesc.m_uMemoryFlags = (1u << MEMORY_FLAGS__SHADER_READ);
		g_xEngine.HDR().m_axBloomChainHandles[i] = xGraph.CreateTransient(xDesc);

		uBloomWidth = std::max(1u, uBloomWidth / 2);
		uBloomHeight = std::max(1u, uBloomHeight / 2);
	}

	xGraph.AddPass("HDR_LuminanceHistogram", ExecuteLuminanceHistogram)
		.Prepare    (PreExecuteLuminanceHistogram)
		.Reads      (g_xEngine.HDR().GetHDRSceneTarget(),                     RESOURCE_ACCESS_READ_SRV)
		.WritesBuffer(g_xEngine.HDR().m_xHistogramBuffer.GetBuffer(),          RESOURCE_ACCESS_WRITE_UAV);

	// Histogram is bound as UAV_Buffer (see ExecuteAdaptation); declare as
	// READWRITE_UAV so the graph barrier matches the compute-stage access.
	xGraph.AddPass("HDR_Adaptation", ExecuteAdaptation)
		.ReadsBuffer (g_xEngine.HDR().m_xHistogramBuffer.GetBuffer(), RESOURCE_ACCESS_READWRITE_UAV)
		.WritesBuffer(g_xEngine.HDR().m_xExposureBuffer.GetBuffer(),  RESOURCE_ACCESS_WRITE_UAV);

	xGraph.AddPass("HDR_BloomThreshold", ExecuteBloomThreshold)
		.ClearTargets()
		.Reads          (g_xEngine.HDR().GetHDRSceneTarget(),             RESOURCE_ACCESS_READ_SRV)
		.WritesTransient(g_xEngine.HDR().m_axBloomChainHandles[0],        RESOURCE_ACCESS_WRITE_RTV);

	static const char* s_aszBloomDownsampleNames[] = {
		"HDR_BloomDownsample Mip1", "HDR_BloomDownsample Mip2",
		"HDR_BloomDownsample Mip3", "HDR_BloomDownsample Mip4",
	};
	for (u_int i = 1; i < 5; i++)
	{
		xGraph.AddPass(s_aszBloomDownsampleNames[i - 1], ExecuteBloomDownsample, &s_axBloomMipUserData[i])
			.ClearTargets()
			.ReadsTransient (g_xEngine.HDR().m_axBloomChainHandles[i - 1], RESOURCE_ACCESS_READ_SRV)
			.WritesTransient(g_xEngine.HDR().m_axBloomChainHandles[i],     RESOURCE_ACCESS_WRITE_RTV);
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
			.ReadsTransient (g_xEngine.HDR().m_axBloomChainHandles[uSourceMip], RESOURCE_ACCESS_READ_SRV)
			.WritesTransient(g_xEngine.HDR().m_axBloomChainHandles[uTargetMip], RESOURCE_ACCESS_WRITE_RTV);
	}

	// Tone mapping samples exposure (and optionally histogram for a debug
	// overlay). Both are bound as UAV_Buffer in ExecuteToneMapping so declare
	// as READWRITE_UAV — the barrier must match the binding's access mode
	// rather than the shader's actual read-only intent.
	xGraph.AddPass("HDR_ToneMapping", ExecuteToneMapping)
		.ClearTargets()
		.Reads         (g_xEngine.HDR().GetHDRSceneTarget(),                    RESOURCE_ACCESS_READ_SRV)
		.Writes        (g_xEngine.FluxGraphics().GetFinalRenderTarget(),  RESOURCE_ACCESS_WRITE_RTV)
		.ReadsBuffer   (g_xEngine.HDR().m_xHistogramBuffer.GetBuffer(),         RESOURCE_ACCESS_READWRITE_UAV)
		.ReadsBuffer   (g_xEngine.HDR().m_xExposureBuffer.GetBuffer(),          RESOURCE_ACCESS_READWRITE_UAV)
		.ReadsTransient(g_xEngine.HDR().m_axBloomChainHandles[0],               RESOURCE_ACCESS_READ_SRV);
}

Flux_ShaderResourceView& Flux_HDRImpl::GetHDRSceneSRV()
{
	return g_xEngine.HDR().GetHDRSceneTarget().SRV();
}

Flux_RenderAttachment& Flux_HDRImpl::GetHDRSceneTarget()
{
	Zenith_Assert(g_xEngine.HDR().m_pxGraph, "Flux_HDRImpl::GetHDRSceneTarget: graph pointer is null");
	return g_xEngine.HDR().m_pxGraph->GetTransientAttachment(g_xEngine.HDR().m_xHDRSceneTargetHandle);
}

void Flux_HDRImpl::GetHDRSceneTargetSetup(Flux_RenderAttachment* apxColourAttachments[], uint32_t& uNumColour, Flux_RenderAttachment*& pxDepthStencil)
{
	apxColourAttachments[0] = &g_xEngine.HDR().GetHDRSceneTarget();
	uNumColour = 1;
	pxDepthStencil = nullptr;
}

void Flux_HDRImpl::GetHDRSceneTargetSetupWithDepth(Flux_RenderAttachment* apxColourAttachments[], uint32_t& uNumColour, Flux_RenderAttachment*& pxDepthStencil)
{
	apxColourAttachments[0] = &g_xEngine.HDR().GetHDRSceneTarget();
	uNumColour = 1;
	pxDepthStencil = &g_xEngine.FluxGraphics().GetDepthAttachment();
}



void Flux_HDRImpl::SetBloomIntensity(float fIntensity)
{
	g_xEngine.HDR().m_fBloomIntensity = std::clamp(fIntensity, 0.0f, 10.0f);
}

void Flux_HDRImpl::SetBloomThreshold(float fThreshold)
{
	g_xEngine.HDR().m_fBloomThreshold = std::clamp(fThreshold, 0.0f, 100.0f);
}

float Flux_HDRImpl::GetCurrentExposure()
{
	return g_xEngine.HDR().m_fCurrentExposure;
}

float Flux_HDRImpl::GetAverageLuminance()
{
	return g_xEngine.HDR().m_fAverageLuminance;
}

void Flux_HDRImpl::SetAdaptationSpeed(float fSpeed)
{
	g_xEngine.HDR().m_fAdaptationSpeed = fSpeed;
}

void Flux_HDRImpl::SetTargetLuminance(float fLuminance)
{
	g_xEngine.HDR().m_fTargetLuminance = fLuminance;
}

void Flux_HDRImpl::SetExposureRange(float fMin, float fMax)
{
	g_xEngine.HDR().m_fMinExposure = fMin;
	g_xEngine.HDR().m_fMaxExposure = fMax;
}

bool Flux_HDRImpl::IsAutoExposureEnabled()
{
	return Zenith_GraphicsOptions::Get().m_bHDRAutoExposureEnabled;
}

float Flux_HDRImpl::GetAdaptationSpeed()
{
	return g_xEngine.HDR().m_fAdaptationSpeed;
}

float Flux_HDRImpl::GetTargetLuminance()
{
	return g_xEngine.HDR().m_fTargetLuminance;
}

Flux_RenderAttachment& Flux_HDRImpl::GetBloomChainAttachment(u_int uIndex)
{
	Zenith_Assert(uIndex < 5, "Flux_HDRImpl::GetBloomChainAttachment: index %u out of range", uIndex);
	Zenith_Assert(g_xEngine.HDR().m_pxGraph, "Flux_HDRImpl::GetBloomChainAttachment: graph pointer is null");
	return g_xEngine.HDR().m_pxGraph->GetTransientAttachment(g_xEngine.HDR().m_axBloomChainHandles[uIndex]);
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
	Zenith_DebugVariables::AddUInt32({ "Flux", "HDR", "DebugMode" }, dbg_uHDRDebugMode, 0, HDR_DEBUG_COUNT - 1);
	Zenith_DebugVariables::AddFloat({ "Flux", "HDR", "Exposure" }, dbg_fHDRExposure, 0.01f, 10.0f);
	Zenith_DebugVariables::AddFloat({ "Flux", "HDR", "BloomIntensity" }, dbg_fHDRBloomIntensity, 0.0f, 2.0f);
	Zenith_DebugVariables::AddFloat({ "Flux", "HDR", "BloomThreshold" }, dbg_fHDRBloomThreshold, 0.0f, 5.0f);
	Zenith_DebugVariables::AddUInt32({ "Flux", "HDR", "ToneMappingOperator" }, dbg_uHDRToneMappingOperator, 0, TONEMAPPING_COUNT - 1);
	Zenith_DebugVariables::AddBoolean({ "Flux", "HDR", "ShowHistogram" }, dbg_bHDRShowHistogram);
	Zenith_DebugVariables::AddBoolean({ "Flux", "HDR", "FreezeExposure" }, dbg_bHDRFreezeExposure);
	Zenith_DebugVariables::AddFloat({ "Flux", "HDR", "AdaptationSpeed" }, dbg_fHDRAdaptationSpeed, 0.1f, 10.0f);
	Zenith_DebugVariables::AddFloat({ "Flux", "HDR", "TargetLuminance" }, dbg_fHDRTargetLuminance, 0.01f, 1.0f);
	Zenith_DebugVariables::AddFloat({ "Flux", "HDR", "MinExposure" }, dbg_fHDRMinExposure, 0.01f, 1.0f);
	Zenith_DebugVariables::AddFloat({ "Flux", "HDR", "MaxExposure" }, dbg_fHDRMaxExposure, 1.0f, 20.0f);
}

// Debug-SRV callbacks — mirror g_xEngine.FluxGraphics().GetDebugSRV_MRTDiffuse. Register
// via Zenith_DebugVariables::AddTextureCallback in Flux::LateInitialise; the
// callback is invoked on every ImGui draw so the preview tracks transient
// rebuilds on resize without leaving a dangling SRV.
const Flux_ShaderResourceView* Flux_HDRImpl::GetDebugSRV_HDRScene()
{
	if (g_xEngine.HDR().m_pxGraph == nullptr) return nullptr;
	return &g_xEngine.HDR().m_pxGraph->GetTransientAttachment(g_xEngine.HDR().m_xHDRSceneTargetHandle).SRV();
}
const Flux_ShaderResourceView* Flux_HDRImpl::GetDebugSRV_Bloom0()
{
	if (g_xEngine.HDR().m_pxGraph == nullptr) return nullptr;
	return &g_xEngine.HDR().m_pxGraph->GetTransientAttachment(g_xEngine.HDR().m_axBloomChainHandles[0]).SRV();
}
const Flux_ShaderResourceView* Flux_HDRImpl::GetDebugSRV_Bloom1()
{
	if (g_xEngine.HDR().m_pxGraph == nullptr) return nullptr;
	return &g_xEngine.HDR().m_pxGraph->GetTransientAttachment(g_xEngine.HDR().m_axBloomChainHandles[1]).SRV();
}
const Flux_ShaderResourceView* Flux_HDRImpl::GetDebugSRV_Bloom2()
{
	if (g_xEngine.HDR().m_pxGraph == nullptr) return nullptr;
	return &g_xEngine.HDR().m_pxGraph->GetTransientAttachment(g_xEngine.HDR().m_axBloomChainHandles[2]).SRV();
}
#endif
