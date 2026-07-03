#include "Zenith.h"
#include "Flux/SSR/Flux_SSR_Shaders.h"
#include "Flux/Flux_RendererImpl.h"
#include "Core/Zenith_Engine.h"

#include "Flux/SSR/Flux_SSRImpl.h"
#include "Flux/SSR/Flux_SSRImpl.h"
#include "Flux/HiZ/Flux_HiZImpl.h"
#include "Flux/HiZ/Flux_HiZImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/HDR/Flux_HDRImpl.h"
#include "Flux/Fog/Flux_VolumeFogImpl.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Flux/Shaders/Generated/SSR.h" // typed binding handles
#include "AssetHandling/Zenith_TextureAsset.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "DebugVariables/Zenith_DebugVariables.h"
// VulkanMemory owns the SSR constants CBV lifetime + the per-frame upload;
// VulkanSwapchain supplies the render dimensions. Both are reached via
// g_xEngine at point of use and need their full types here.
#include "Flux/Flux_BackendTypes.h"

// Graph-owned transient handles — backing Flux_RenderAttachments are allocated
// and destroyed by the render graph, sized from the descriptors set in
// SetupRenderGraph. One set per render-view slot (S5b); [v] below is the slot:
//   g_xEngine.SSR().m_axRayMarchHandles[v]     — half-res RT0, RGB=colour, A=confidence
//   g_xEngine.SSR().m_axRayMarchAuxHandles[v]  — half-res RT1, RG=hitUV, B=travelDist, A=rayCount
//   g_xEngine.SSR().m_axUpsampledHandles[v]    — full-res RT0, bilateral 2x2 upsample of RT0
//   g_xEngine.SSR().m_axUpsampledAuxHandles[v] — full-res RT1, bilateral 2x2 upsample of RT1
//   g_xEngine.SSR().m_axDenoiseHHandles[v]     — full-res, H pass RT0: rgb=Σ(w·color), a=Σ(w)
//   g_xEngine.SSR().m_axDenoiseHConfHandles[v] — full-res, H pass RT1: r=Σ(w·conf) — Phase 3b accumulator
//   g_xEngine.SSR().m_axDenoiseVHandles[v]     — full-res, written by DenoiseV (final denoised output)
//
// The Aux transients carry per-pixel hit metadata used by Phase 3b denoise
// (BRDF reuse needs neighbour hit UV) and Phase 4 variance estimation
// (radial component). Phase 0 wires them end-to-end with no consumer beyond
// debug-texture preview; later phases plug them in.

// SSR render target format.
static constexpr TextureFormat SSR_FORMAT = TEXTURE_FORMAT_R16G16B16A16_SFLOAT;

// Static member definitions

// Debug variables
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
	u_int m_uStepCount = 32;      // Max iterations for hierarchical traversal — HiZ acceleration converges quickly
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
	// Half-resolution ray-march target dimensions (populated per view each
	// frame in UpdateSSRConstants from the view's base dims). Passed via CBV
	// so the shaders never need GetDimensions() on the bound textures.
	float m_fHalfResWidth     = 0.0f;
	float m_fHalfResHeight    = 0.0f;
	float m_fRcpHalfResWidth  = 0.0f;
	float m_fRcpHalfResHeight = 0.0f;
	// Phase 2 — rays per pixel for surfaces with roughness above the multi-ray
	// threshold.
	u_int m_uRayCount         = 1u;
	// Phase 6 — adaptive step budget floor. The shader picks a per-pixel value
	// between m_uStepCountMin (high-roughness centre-of-frame) and
	// m_uStepCount (mirror-smooth or near-edge).
	u_int m_uStepCountMin     = 16u;
	// Explicit padding so the float4 array below lands on a 16-byte boundary
	// (Slang CBV layout rules); kept as named fields for clarity. Unused.
	u_int m_uPad0             = 0u;
	u_int m_uPad1             = 0u;
	u_int m_uPad2             = 0u;
	// Cached HiZ mip dimensions. Indexed by mip level. Each entry packs
	// (width, height, 1/width, 1/height) into a float4. Populated per view
	// each frame in UpdateSSRConstants from the view's base dims — replaces
	// in-shader GetDimensions(g_xHiZTex, mip, ...) calls in the ray-march
	// loop. Array size matches Flux_HiZImpl::uHIZ_MAX_MIPS (12).
	float m_axHiZMipSizes[12][4] = {};
} dbg_xSSRConstants;

// Shaders and pipelines

// Push-constant struct uploaded per-pass via BindDrawConstants (mirrors the
// SSGI denoise pattern). Kept ≤32 bytes to fit the 128-byte push budget.
// Phase 3b imports Common.Frame in the shader (for matrices needed by the
// half-vector calc), so screen dims now travel via FrameConstants again and
// the trailing slots return to padding (reserved for Phase 4 variance config).
static struct SSRDenoiseConstants
{
	float m_fSpatialSigma     = 1.5f;
	float m_fDepthSigma       = 0.02f;
	float m_fNormalSigma      = 0.2f;
	float m_fRoughnessSigma   = 0.1f;
	u_int m_uKernelRadius     = 4;
	u_int m_bEnabled          = 1;
	float m_fPad0             = 0.0f;
	float m_fPad1             = 0.0f;
} dbg_xSSRDenoiseConstants;

// SSR constants are uploaded once per frame per view (in that view's RayMarch
// pass) and the view's CBV is then bound by every SSR pass that consumes
// them. Driven this way instead of via push constants because the cached HiZ
// mip-size array exceeds the 128-byte push limit, and to honour the project
// convention of passing texture dimensions through CBVs rather than calling
// GetDimensions inside shaders.

// ---- Helpers to get the right attachment regardless of path ----

Flux_RenderAttachment& Flux_SSRImpl::GetRayMarchAttachment(u_int uViewSlot)
{
	Zenith_Assert(m_pxGraph, "Flux_SSRImpl::GetRayMarchAttachment: graph pointer is null (called before SetupRenderGraph or after Shutdown)");
	return m_pxGraph->GetTransientAttachment(m_axRayMarchHandles[uViewSlot]);
}
Flux_RenderAttachment& Flux_SSRImpl::GetRayMarchAuxAttachment(u_int uViewSlot)
{
	Zenith_Assert(m_pxGraph, "Flux_SSRImpl::GetRayMarchAuxAttachment: graph pointer is null (called before SetupRenderGraph or after Shutdown)");
	return m_pxGraph->GetTransientAttachment(m_axRayMarchAuxHandles[uViewSlot]);
}
Flux_RenderAttachment& Flux_SSRImpl::GetUpsampledAttachment(u_int uViewSlot)
{
	Zenith_Assert(m_pxGraph, "Flux_SSRImpl::GetUpsampledAttachment: graph pointer is null (called before SetupRenderGraph or after Shutdown)");
	return m_pxGraph->GetTransientAttachment(m_axUpsampledHandles[uViewSlot]);
}
Flux_RenderAttachment& Flux_SSRImpl::GetUpsampledAuxAttachment(u_int uViewSlot)
{
	Zenith_Assert(m_pxGraph, "Flux_SSRImpl::GetUpsampledAuxAttachment: graph pointer is null (called before SetupRenderGraph or after Shutdown)");
	return m_pxGraph->GetTransientAttachment(m_axUpsampledAuxHandles[uViewSlot]);
}
Flux_RenderAttachment& Flux_SSRImpl::GetDenoiseHAttachment(u_int uViewSlot)
{
	Zenith_Assert(m_pxGraph, "Flux_SSRImpl::GetDenoiseHAttachment: graph pointer is null (called before SetupRenderGraph or after Shutdown)");
	return m_pxGraph->GetTransientAttachment(m_axDenoiseHHandles[uViewSlot]);
}
Flux_RenderAttachment& Flux_SSRImpl::GetDenoiseHConfAttachment(u_int uViewSlot)
{
	Zenith_Assert(m_pxGraph, "Flux_SSRImpl::GetDenoiseHConfAttachment: graph pointer is null (called before SetupRenderGraph or after Shutdown)");
	return m_pxGraph->GetTransientAttachment(m_axDenoiseHConfHandles[uViewSlot]);
}
Flux_RenderAttachment& Flux_SSRImpl::GetDenoiseVAttachment(u_int uViewSlot)
{
	Zenith_Assert(m_pxGraph, "Flux_SSRImpl::GetDenoiseVAttachment: graph pointer is null (called before SetupRenderGraph or after Shutdown)");
	return m_pxGraph->GetTransientAttachment(m_axDenoiseVHandles[uViewSlot]);
}

#ifdef ZENITH_DEBUG_VARIABLES
// Debug-texture callbacks — resolved every ImGui draw so the preview follows
// render-graph rebuilds (the transient SRVs are recreated on resize). Returning
// nullptr before SetupRenderGraph / after Shutdown renders nothing, avoiding
// the null-guard in the getters above. Main view only (default view slot).
static const Flux_ShaderResourceView* DebugGetRayMarchSRV()
{
	auto& xSSR = g_xEngine.SSR();
	if (xSSR.m_pxGraph == nullptr) return nullptr;
	return &xSSR.GetRayMarchAttachment().SRV();
}
static const Flux_ShaderResourceView* DebugGetRayMarchAuxSRV()
{
	auto& xSSR = g_xEngine.SSR();
	if (xSSR.m_pxGraph == nullptr) return nullptr;
	return &xSSR.GetRayMarchAuxAttachment().SRV();
}
static const Flux_ShaderResourceView* DebugGetUpsampledSRV()
{
	auto& xSSR = g_xEngine.SSR();
	if (xSSR.m_pxGraph == nullptr) return nullptr;
	return &xSSR.GetUpsampledAttachment().SRV();
}
static const Flux_ShaderResourceView* DebugGetUpsampledAuxSRV()
{
	auto& xSSR = g_xEngine.SSR();
	if (xSSR.m_pxGraph == nullptr) return nullptr;
	return &xSSR.GetUpsampledAuxAttachment().SRV();
}
static const Flux_ShaderResourceView* DebugGetDenoiseHSRV()
{
	auto& xSSR = g_xEngine.SSR();
	if (xSSR.m_pxGraph == nullptr) return nullptr;
	return &xSSR.GetDenoiseHAttachment().SRV();
}
static const Flux_ShaderResourceView* DebugGetDenoiseHConfSRV()
{
	auto& xSSR = g_xEngine.SSR();
	if (xSSR.m_pxGraph == nullptr) return nullptr;
	return &xSSR.GetDenoiseHConfAttachment().SRV();
}
static const Flux_ShaderResourceView* DebugGetDenoiseVSRV()
{
	auto& xSSR = g_xEngine.SSR();
	if (xSSR.m_pxGraph == nullptr) return nullptr;
	return &xSSR.GetDenoiseVAttachment().SRV();
}
#endif

void Flux_SSRImpl::BuildPipelines()
{
	// RayMarch + Upsample are dual-MRT: RT0 carries colour+confidence (legacy
	// shape), RT1 carries hit metadata (UV / travel distance / ray count) for
	// downstream Phase 3b denoise BRDF reuse and Phase 4 variance.
	{
		const TextureFormat aeFormats[2] = { SSR_FORMAT, SSR_FORMAT };
		Flux_PipelineSpecification xSpec = Flux_PipelineHelper::CreateFullscreenSpecMRT(
			m_xRayMarchShader, Flux_SSRShaders::xSSR_RayMarch, aeFormats, 2u);
		Flux_PipelineBuilder::FromSpecification(m_xRayMarchPipeline, xSpec);
	}

	{
		const TextureFormat aeFormats[2] = { SSR_FORMAT, SSR_FORMAT };
		Flux_PipelineSpecification xSpec = Flux_PipelineHelper::CreateFullscreenSpecMRT(
			m_xUpsampleShader, Flux_SSRShaders::xSSR_Upsample, aeFormats, 2u);
		Flux_PipelineBuilder::FromSpecification(m_xUpsamplePipeline, xSpec);
	}

	// DenoiseH is dual-MRT (Phase 3b): RT0 = Σ(w·color)+Σw, RT1 = Σ(w·conf).
	{
		const TextureFormat aeFormats[2] = { SSR_FORMAT, SSR_FORMAT };
		Flux_PipelineSpecification xSpec = Flux_PipelineHelper::CreateFullscreenSpecMRT(
			m_xDenoiseHShader, Flux_SSRShaders::xSSR_DenoiseH, aeFormats, 2u);
		Flux_PipelineBuilder::FromSpecification(m_xDenoiseHPipeline, xSpec);
	}

	Flux_PipelineHelper::BuildFullscreenPipeline(
		m_xDenoiseVShader, m_xDenoiseVPipeline,
		Flux_SSRShaders::xSSR_DenoiseV, SSR_FORMAT);
}

void Flux_SSRImpl::Initialise()
{
	BuildPipelines();

	// One SSR-constants CBV per render-view slot (S5b): each full-pipeline view
	// uploads its own dims + HiZ mip-size table in UpdateSSRConstants. Inactive
	// slots still carry a valid buffer descriptor (mirrors
	// Flux_GraphicsImpl::m_axViewConstantsBuffers).
	for (u_int u = 0; u < FLUX_MAX_RENDER_VIEWS; u++)
	{
		g_xEngine.FluxMemory().InitialiseDynamicConstantBuffer(
			&dbg_xSSRConstants, sizeof(SSRConstants), m_axSSRConstantsBuffers[u]);
	}

#ifdef ZENITH_DEBUG_VARIABLES
	g_xEngine.DebugVariables().AddUInt32({ "Flux", "SSR", "DebugMode" }, dbg_uDebugMode, 0, 100);  // Extended range for diagnostic mode 99
	g_xEngine.DebugVariables().AddFloat({ "Flux", "SSR", "Intensity" }, dbg_xSSRConstants.m_fIntensity, 0.0f, 2.0f);
	g_xEngine.DebugVariables().AddFloat({ "Flux", "SSR", "MaxDistance" }, dbg_xSSRConstants.m_fMaxDistance, 1.0f, 100.0f);
	g_xEngine.DebugVariables().AddFloat({ "Flux", "SSR", "MaxRoughness" }, dbg_xSSRConstants.m_fMaxRoughness, 0.0f, 1.0f);
	g_xEngine.DebugVariables().AddFloat({ "Flux", "SSR", "Thickness" }, dbg_xSSRConstants.m_fThickness, 0.01f, 1.0f);
	g_xEngine.DebugVariables().AddUInt32({ "Flux", "SSR", "StepCount" }, dbg_xSSRConstants.m_uStepCount, 8, 256);
	g_xEngine.DebugVariables().AddUInt32({ "Flux", "SSR", "StartMip" }, dbg_xSSRConstants.m_uStartMip, 0, 10);
	g_xEngine.DebugVariables().AddFloat({ "Flux", "SSR", "ContactHardeningDist" }, dbg_xSSRConstants.m_fContactHardeningDist, 0.5f, 10.0f);
	g_xEngine.DebugVariables().AddUInt32({ "Flux", "SSR", "RaysPerPixel" }, dbg_xSSRConstants.m_uRayCount, 1, 4);
	g_xEngine.DebugVariables().AddUInt32({ "Flux", "SSR", "StepCountMin" }, dbg_xSSRConstants.m_uStepCountMin, 4, 64);
	// Transient-SRV previews use AddTextureCallback so the SRV is re-resolved
	// through g_xEngine.SSR().m_pxGraph on every ImGui draw. Storing a stale pointer via
	// AddTexture would go dangling once the graph rebuilds (e.g. on resize).
	g_xEngine.DebugVariables().AddTextureCallback({ "Flux", "SSR", "Textures", "RayMarch" },     &DebugGetRayMarchSRV);
	g_xEngine.DebugVariables().AddTextureCallback({ "Flux", "SSR", "Textures", "RayMarchAux" },  &DebugGetRayMarchAuxSRV);
	g_xEngine.DebugVariables().AddTextureCallback({ "Flux", "SSR", "Textures", "Upsampled" },    &DebugGetUpsampledSRV);
	g_xEngine.DebugVariables().AddTextureCallback({ "Flux", "SSR", "Textures", "UpsampledAux" }, &DebugGetUpsampledAuxSRV);
	g_xEngine.DebugVariables().AddTextureCallback({ "Flux", "SSR", "Textures", "DenoiseH" },     &DebugGetDenoiseHSRV);
	g_xEngine.DebugVariables().AddTextureCallback({ "Flux", "SSR", "Textures", "DenoiseHConf" }, &DebugGetDenoiseHConfSRV);
	g_xEngine.DebugVariables().AddTextureCallback({ "Flux", "SSR", "Textures", "DenoiseV" },     &DebugGetDenoiseVSRV);

	// Denoise push-constant tuning knobs.
	g_xEngine.DebugVariables().AddFloat ({ "Flux", "SSR", "Denoise", "SpatialSigma" },   dbg_xSSRDenoiseConstants.m_fSpatialSigma,   0.5f, 6.0f);
	g_xEngine.DebugVariables().AddFloat ({ "Flux", "SSR", "Denoise", "DepthSigma" },     dbg_xSSRDenoiseConstants.m_fDepthSigma,     0.001f, 0.1f);
	g_xEngine.DebugVariables().AddFloat ({ "Flux", "SSR", "Denoise", "NormalSigma" },    dbg_xSSRDenoiseConstants.m_fNormalSigma,    0.05f, 1.0f);
	g_xEngine.DebugVariables().AddFloat ({ "Flux", "SSR", "Denoise", "RoughnessSigma" }, dbg_xSSRDenoiseConstants.m_fRoughnessSigma, 0.01f, 0.5f);
	g_xEngine.DebugVariables().AddUInt32({ "Flux", "SSR", "Denoise", "KernelRadius" },   dbg_xSSRDenoiseConstants.m_uKernelRadius,   1, 8);
#endif

	m_bInitialised = true;
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_SSR initialised");
}

void Flux_SSRImpl::ShutdownImpl()
{
	// CRTP hook from Flux_ScreenSpaceEffectBase::Shutdown(); the base owns the
	// m_bInitialised guard and the m_pxGraph / m_bInitialised resets.
	for (u_int u = 0; u < FLUX_MAX_RENDER_VIEWS; u++)
	{
		g_xEngine.FluxMemory().DestroyDynamicConstantBuffer(m_axSSRConstantsBuffers[u]);
	}

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_SSR shut down");
}

void Flux_SSRImpl::UpdateSSRConstants(u_int uViewSlot)
{
	// View-independent refreshes on the debug-var-bound tuning struct. Every
	// view's record callback writes the SAME values here (record callbacks can
	// run on parallel workers), so the shared mutation is benign; every
	// view-dependent field is overridden on the frame-local snapshot below.
	dbg_xSSRConstants.m_uDebugMode = dbg_uDebugMode;
	// SSR has no temporal accumulation, so rotating the blue-noise field per
	// frame produces visible shimmer rather than integrating across frames.
	// The blue noise itself is still useful for spatial dithering, multi-ray
	// spread, and feeding the spatial DenoiseH/V passes — only the per-frame
	// rotation has no consumer. Pin to 0 so each pixel samples a stable
	// rotation every frame. (Was implicitly 0 historically because
	// Flux::GetFrameCounter() returned 0 forever — db34fcd2 fixed the
	// underlying counter and surfaced this latent design mismatch.)
	dbg_xSSRConstants.m_uFrameIndex = 0;

	// Clamp start mip against the MAIN chain — this write-back is what the
	// debug-var UI reflects; the per-view snapshot re-clamps below (a no-op
	// for the main view).
	const u_int uMainMipCount = g_xEngine.HiZ().GetMipCount();
	if (dbg_xSSRConstants.m_uStartMip >= uMainMipCount)
		dbg_xSSRConstants.m_uStartMip = uMainMipCount - 1;

	// Frame-local per-view snapshot: dims + the HiZ mip chain come from THIS
	// view (slot 0 = swapchain, preview = kuFLUX_PREVIEW_VIEW_SIZE²), so the
	// main view's fill is byte-identical to the historical single-view path.
	SSRConstants xConstants = dbg_xSSRConstants;
	xConstants.m_uHiZMipCount = g_xEngine.HiZ().GetMipCount(uViewSlot);
	if (xConstants.m_uStartMip >= xConstants.m_uHiZMipCount)
		xConstants.m_uStartMip = xConstants.m_uHiZMipCount - 1;

	// Resolution-based binary search iterations for sub-pixel hit precision.
	// Half-res ray origins (Phase B) make sub-1/16-pixel precision wasted —
	// the bilateral upsample reconstructs full-res positions anyway.
	// 1080p (≤1920): 4 iterations
	// 1440p / 4K (>1920): 5 iterations
	const u_int uFullWidth  = m_auViewWidths[uViewSlot];
	const u_int uFullHeight = m_auViewHeights[uViewSlot];
	xConstants.m_uBinarySearchIterations = 4 + (uFullWidth > 1920 ? 1 : 0);

	// Half-res ray-march output dimensions (matches the per-view half-res
	// transient created in SetupViewPasses; both derive from the same base
	// dims so they stay in sync).
	const u_int uHalfWidth  = std::max(1u, uFullWidth  / 2u);
	const u_int uHalfHeight = std::max(1u, uFullHeight / 2u);
	xConstants.m_fHalfResWidth     = (float)uHalfWidth;
	xConstants.m_fHalfResHeight    = (float)uHalfHeight;
	xConstants.m_fRcpHalfResWidth  = 1.0f / (float)uHalfWidth;
	xConstants.m_fRcpHalfResHeight = 1.0f / (float)uHalfHeight;

	// HiZ per-mip dimensions. HiZ mip 0 matches the view's base dims (its
	// depth buffer); each subsequent mip is a 2x2 reduction. Pre-computing the
	// (size, rcp size) here means the ray-march shader doesn't need to
	// call GetDimensions in its inner loop (project convention: dims
	// flow through the CBV).
	const u_int uMipCount = xConstants.m_uHiZMipCount;
	for (u_int uMip = 0; uMip < uMipCount && uMip < 12u; ++uMip)
	{
		const u_int uW = std::max(1u, uFullWidth  >> uMip);
		const u_int uH = std::max(1u, uFullHeight >> uMip);
		xConstants.m_axHiZMipSizes[uMip][0] = (float)uW;
		xConstants.m_axHiZMipSizes[uMip][1] = (float)uH;
		xConstants.m_axHiZMipSizes[uMip][2] = 1.0f / (float)uW;
		xConstants.m_axHiZMipSizes[uMip][3] = 1.0f / (float)uH;
	}

	// Upload straight to this view's CBV (the snapshot is frame-local). Every
	// SSR pass for this view binds the same CBV without re-uploading.
	g_xEngine.FluxMemory().UploadBufferData(
		m_axSSRConstantsBuffers[uViewSlot].GetBuffer().m_xVRAMHandle,
		&xConstants, sizeof(SSRConstants));
}

static void ExecuteSSRRayMarch(Flux_CommandBuffer* pxCommandList, void*)
{
	// Non-capturing graph trampoline: recover the singleton instance first;
	// cross-subsystem deps are reached via g_xEngine at point of use.
	auto& xSSR = g_xEngine.SSR();
	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();

	if (!xSSR.IsEnabled() || !g_xEngine.HiZ().IsEnabled())
		return;

	// The pass's declared view slot selects this view's chain (transients,
	// CBV, G-buffer, HiZ pyramid).
	const u_int uViewSlot = Flux_RenderGraph::GetCurrentRecordingPassViewSlot();

	// First SSR pass of this view's chain: refresh + upload the view's CBV.
	// The Upsample pass binds the same CBV without re-uploading.
	xSSR.UpdateSSRConstants(uViewSlot);

	xGraphics.BindFullscreenQuad(*pxCommandList, xSSR.m_xRayMarchPipeline);

	Flux_ShaderBinder xBinder(*pxCommandList);

	namespace RM = Flux_Generated_SSR::SSR_RayMarch;
	xBinder.BindCBV(RM::hSSRConstants,   &xSSR.m_axSSRConstantsBuffers[uViewSlot].GetCBV());

	xBinder.BindSRV(RM::hg_xDepthTex, xGraphics.GetDepthStencilSRV(uViewSlot));
	xBinder.BindSRV(RM::hg_xNormalsTex, xGraphics.GetGBufferSRV(MRT_INDEX_NORMALSAMBIENT, uViewSlot));
	xBinder.BindSRV(RM::hg_xMaterialTex, xGraphics.GetGBufferSRV(MRT_INDEX_MATERIAL, uViewSlot));
	xBinder.BindSRV(RM::hg_xHiZTex, &g_xEngine.HiZ().GetHiZSRV(uViewSlot));
	xBinder.BindSRV(RM::hg_xDiffuseTex, xGraphics.GetGBufferSRV(MRT_INDEX_DIFFUSE, uViewSlot));
	xBinder.BindSRV(RM::hg_xBlueNoiseTex, &g_xEngine.VolumeFog().GetBlueNoiseTexture()->m_xSRV);

	pxCommandList->DrawIndexed(6);
}

static void ExecuteSSRUpsample(Flux_CommandBuffer* pxCommandList, void*)
{
	auto& xSSR = g_xEngine.SSR();
	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();

	if (!xSSR.IsEnabled() || !g_xEngine.HiZ().IsEnabled())
		return;

	const u_int uViewSlot = Flux_RenderGraph::GetCurrentRecordingPassViewSlot();

	xGraphics.BindFullscreenQuad(*pxCommandList, xSSR.m_xUpsamplePipeline);

	Flux_ShaderBinder xBinder(*pxCommandList);

	namespace US = Flux_Generated_SSR::SSR_Upsample;
	// Half-res dimensions live in the view's SSR CBV — the upsample shader
	// reads them from there rather than calling GetDimensions on g_xSSRTex.
	xBinder.BindCBV(US::hSSRConstants, &xSSR.m_axSSRConstantsBuffers[uViewSlot].GetCBV());

	xBinder.BindSRV(US::hg_xSSRTex,      &xSSR.GetRayMarchAttachment(uViewSlot).SRV());
	xBinder.BindSRV(US::hg_xDepthTex,    xGraphics.GetDepthStencilSRV(uViewSlot));
	xBinder.BindSRV(US::hg_xSSRAuxTex,   &xSSR.GetRayMarchAuxAttachment(uViewSlot).SRV());
	// Phase 5 — KNN composite-similarity scoring needs full-res normals + material.
	xBinder.BindSRV(US::hg_xNormalsTex,  xGraphics.GetGBufferSRV(MRT_INDEX_NORMALSAMBIENT, uViewSlot));
	xBinder.BindSRV(US::hg_xMaterialTex, xGraphics.GetGBufferSRV(MRT_INDEX_MATERIAL, uViewSlot));

	pxCommandList->DrawIndexed(6);
}

static void ExecuteSSRDenoiseH(Flux_CommandBuffer* pxCommandList, void*)
{
	auto& xSSR = g_xEngine.SSR();
	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();

	if (!xSSR.IsEnabled() || !g_xEngine.HiZ().IsEnabled() || !Zenith_GraphicsOptions::Get().m_bSSRRoughnessBlurEnabled)
		return;

	const u_int uViewSlot = Flux_RenderGraph::GetCurrentRecordingPassViewSlot();

	xGraphics.BindFullscreenQuad(*pxCommandList, xSSR.m_xDenoiseHPipeline);

	Flux_ShaderBinder xBinder(*pxCommandList);

	namespace DH = Flux_Generated_SSR::SSR_DenoiseH;
	// Frame-local snapshot so the per-view (main + preview) records — which run on
	// parallel workers — never write the shared file-scope dbg_xSSRDenoiseConstants
	// (mirrors the RayMarch snapshot at UpdateSSRConstants / the SSGI frame-local).
	SSRDenoiseConstants xLocal = dbg_xSSRDenoiseConstants;
	xLocal.m_bEnabled = Zenith_GraphicsOptions::Get().m_bSSRRoughnessBlurEnabled ? 1u : 0u;
	xBinder.BindDrawConstants(DH::hPushConstants, &xLocal, sizeof(SSRDenoiseConstants));

	xBinder.BindSRV(DH::hg_xSSRUpsampledTex,    &xSSR.GetUpsampledAttachment(uViewSlot).SRV());
	xBinder.BindSRV(DH::hg_xDepthTex,           xGraphics.GetDepthStencilSRV(uViewSlot));
	xBinder.BindSRV(DH::hg_xNormalsTex,         xGraphics.GetGBufferSRV(MRT_INDEX_NORMALSAMBIENT, uViewSlot));
	xBinder.BindSRV(DH::hg_xMaterialTex,        xGraphics.GetGBufferSRV(MRT_INDEX_MATERIAL, uViewSlot));
	xBinder.BindSRV(DH::hg_xSSRUpsampledAuxTex, &xSSR.GetUpsampledAuxAttachment(uViewSlot).SRV());

	pxCommandList->DrawIndexed(6);
}

static void ExecuteSSRDenoiseV(Flux_CommandBuffer* pxCommandList, void*)
{
	auto& xSSR = g_xEngine.SSR();
	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();

	if (!xSSR.IsEnabled() || !g_xEngine.HiZ().IsEnabled() || !Zenith_GraphicsOptions::Get().m_bSSRRoughnessBlurEnabled)
		return;

	const u_int uViewSlot = Flux_RenderGraph::GetCurrentRecordingPassViewSlot();

	xGraphics.BindFullscreenQuad(*pxCommandList, xSSR.m_xDenoiseVPipeline);

	Flux_ShaderBinder xBinder(*pxCommandList);

	namespace DV = Flux_Generated_SSR::SSR_DenoiseV;
	// Frame-local snapshot (see ExecuteSSRDenoiseH): no shared write under parallel per-view record.
	SSRDenoiseConstants xLocal = dbg_xSSRDenoiseConstants;
	xLocal.m_bEnabled = Zenith_GraphicsOptions::Get().m_bSSRRoughnessBlurEnabled ? 1u : 0u;
	xBinder.BindDrawConstants(DV::hPushConstants, &xLocal, sizeof(SSRDenoiseConstants));

	xBinder.BindSRV(DV::hg_xSSRDenoiseHColTex,  &xSSR.GetDenoiseHAttachment(uViewSlot).SRV());
	xBinder.BindSRV(DV::hg_xDepthTex,           xGraphics.GetDepthStencilSRV(uViewSlot));
	xBinder.BindSRV(DV::hg_xNormalsTex,         xGraphics.GetGBufferSRV(MRT_INDEX_NORMALSAMBIENT, uViewSlot));
	xBinder.BindSRV(DV::hg_xMaterialTex,        xGraphics.GetGBufferSRV(MRT_INDEX_MATERIAL, uViewSlot));
	xBinder.BindSRV(DV::hg_xSSRUpsampledAuxTex, &xSSR.GetUpsampledAuxAttachment(uViewSlot).SRV());
	xBinder.BindSRV(DV::hg_xSSRDenoiseHConfTex, &xSSR.GetDenoiseHConfAttachment(uViewSlot).SRV());
	xBinder.BindSRV(DV::hg_xSSRUpsampledTex,    &xSSR.GetUpsampledAttachment(uViewSlot).SRV());

	pxCommandList->DrawIndexed(6);
}

// The H/V denoise pass handles are locals in SetupViewPasses (their enable
// bits are set right after declaration). Per-view committed handles are
// re-seeded at SetupRenderGraph exit; GetReflectionHandle asserts the live
// toggle still resolves to the committed handle — otherwise a toggle has
// happened without a corresponding g_xEngine.FluxRenderer().RequestGraphRebuild(),
// which would leave the deferred pass's declared Read referencing the stale
// transient.

void Flux_SSRImpl::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	m_pxGraph = &xGraph;

	// Main view at swapchain dims (byte-equivalent to the historical
	// single-view path), then the preview view at its fixed 512² dims — only
	// while active, so its transients exist exactly when its passes do (the
	// graph's unused-transient validation demands this).
	SetupViewPasses(xGraph, kuFluxViewSlotMain, g_xEngine.FluxGraphics().GetRenderWidth(), g_xEngine.FluxGraphics().GetRenderHeight());
	if (g_xEngine.FluxGraphics().RenderViews().IsViewActive(kuFluxViewSlotPreview))
		SetupViewPasses(xGraph, kuFluxViewSlotPreview, kuFLUX_PREVIEW_VIEW_SIZE, kuFLUX_PREVIEW_VIEW_SIZE);
}

void Flux_SSRImpl::SetupViewPasses(Flux_RenderGraph& xGraph, u_int uViewSlot, u_int uWidth, u_int uHeight)
{
	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();

	// Per-view base dims — UpdateSSRConstants derives the half-res + HiZ
	// mip-size CBV fields from these at record time.
	m_auViewWidths[uViewSlot]  = uWidth;
	m_auViewHeights[uViewSlot] = uHeight;

	// Three-pass SSR pipeline (mirrors SSGI):
	//   RayMarch (half-res) → Upsample (full-res) → Resolve (full-res, optional)
	// The half-res ray-march is the dominant performance win. The Upsample
	// pass is a depth-weighted bilateral 2x2 reconstruction to full-res; it
	// is mandatory so the deferred consumer always reads a full-res image.
	Flux_TransientTextureDesc xHalfDesc;
	xHalfDesc.m_uWidth       = std::max(1u, uWidth  / 2u);
	xHalfDesc.m_uHeight      = std::max(1u, uHeight / 2u);
	xHalfDesc.m_eFormat      = SSR_FORMAT;
	xHalfDesc.m_uMemoryFlags = (1u << MEMORY_FLAGS__SHADER_READ);

	Flux_TransientTextureDesc xFullDesc = xHalfDesc;
	xFullDesc.m_uWidth  = uWidth;
	xFullDesc.m_uHeight = uHeight;

	m_axRayMarchHandles[uViewSlot]     = xGraph.CreateTransient(xHalfDesc);
	m_axRayMarchAuxHandles[uViewSlot]  = xGraph.CreateTransient(xHalfDesc);
	m_axUpsampledHandles[uViewSlot]    = xGraph.CreateTransient(xFullDesc);
	m_axUpsampledAuxHandles[uViewSlot] = xGraph.CreateTransient(xFullDesc);
	m_axDenoiseHHandles[uViewSlot]     = xGraph.CreateTransient(xFullDesc);
	m_axDenoiseHConfHandles[uViewSlot] = xGraph.CreateTransient(xFullDesc);
	m_axDenoiseVHandles[uViewSlot]     = xGraph.CreateTransient(xFullDesc);

	// Pass names must be per-view unique + static-lifetime (duplicate names
	// are a hard assert). View 0 keeps the historical names (profiling /
	// FindPass stability).
	const bool bMainView = (uViewSlot == kuFluxViewSlotMain);

	// RayMarch pass — first writer of its targets; clear so the initial
	// render-pass LoadOp is valid. Dual-MRT: order matters — RT0 = colour,
	// RT1 = aux metadata (must match the shader's SV_Target indices).
	xGraph.AddPass(bMainView ? "SSR RayMarch" : "SSR RayMarch (Preview)", ExecuteSSRRayMarch)
		.View(uViewSlot)
		.ClearTargets()
		.Reads          (xGraphics.GetDepthAttachment(uViewSlot),                         RESOURCE_ACCESS_READ_SRV)
		.Reads          (g_xEngine.HiZ().GetHiZAttachment(uViewSlot),                     RESOURCE_ACCESS_READ_SRV, 0, g_xEngine.HiZ().GetMipCount(uViewSlot))
		.Reads          (xGraphics.GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT, uViewSlot), RESOURCE_ACCESS_READ_SRV)
		.Reads          (xGraphics.GetMRTAttachment(MRT_INDEX_MATERIAL, uViewSlot),       RESOURCE_ACCESS_READ_SRV)
		.Reads          (xGraphics.GetMRTAttachment(MRT_INDEX_DIFFUSE, uViewSlot),        RESOURCE_ACCESS_READ_SRV)
		.WritesTransient(m_axRayMarchHandles[uViewSlot],                                  RESOURCE_ACCESS_WRITE_RTV)
		.WritesTransient(m_axRayMarchAuxHandles[uViewSlot],                               RESOURCE_ACCESS_WRITE_RTV);

	// Upsample pass — depth-weighted bilateral 2x2 from half to full-res.
	// Always enabled; produces the canonical full-res SSR output. Same dual-MRT
	// shape as RayMarch — RT0 colour, RT1 aux — both reconstructed with the
	// same depth weights so they stay spatially aligned.
	xGraph.AddPass(bMainView ? "SSR Upsample" : "SSR Upsample (Preview)", ExecuteSSRUpsample)
		.View(uViewSlot)
		.ClearTargets()
		.Reads          (xGraphics.GetDepthAttachment(uViewSlot),                         RESOURCE_ACCESS_READ_SRV)
		.Reads          (xGraphics.GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT, uViewSlot), RESOURCE_ACCESS_READ_SRV)
		.Reads          (xGraphics.GetMRTAttachment(MRT_INDEX_MATERIAL, uViewSlot),       RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient (m_axRayMarchHandles[uViewSlot],                                  RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient (m_axRayMarchAuxHandles[uViewSlot],                               RESOURCE_ACCESS_READ_SRV)
		.WritesTransient(m_axUpsampledHandles[uViewSlot],                                 RESOURCE_ACCESS_WRITE_RTV)
		.WritesTransient(m_axUpsampledAuxHandles[uViewSlot],                              RESOURCE_ACCESS_WRITE_RTV);

	// DenoiseH pass — separable bilateral with BRDF reuse, horizontal half.
	// Dual-MRT output (Phase 3b): RT0 carries the (Σw·color, Σw) accumulator
	// pair, RT1 carries the (Σw·conf) parallel accumulator. V applies the
	// final ratio. The enable bit tracks m_bSSRRoughnessBlurEnabled. Roughness
	// gating inside the shader skips smooth/rough pixels.
	const Flux_PassHandle xDenoiseHPass = xGraph.AddPass(bMainView ? "SSR DenoiseH" : "SSR DenoiseH (Preview)", ExecuteSSRDenoiseH)
		.View(uViewSlot)
		.ClearTargets()
		.Reads          (xGraphics.GetDepthAttachment(uViewSlot),                         RESOURCE_ACCESS_READ_SRV)
		.Reads          (xGraphics.GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT, uViewSlot), RESOURCE_ACCESS_READ_SRV)
		.Reads          (xGraphics.GetMRTAttachment(MRT_INDEX_MATERIAL, uViewSlot),       RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient (m_axUpsampledHandles[uViewSlot],                                 RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient (m_axUpsampledAuxHandles[uViewSlot],                              RESOURCE_ACCESS_READ_SRV)
		.WritesTransient(m_axDenoiseHHandles[uViewSlot],                                  RESOURCE_ACCESS_WRITE_RTV)
		.WritesTransient(m_axDenoiseHConfHandles[uViewSlot],                              RESOURCE_ACCESS_WRITE_RTV);

	// DenoiseV pass — vertical half, reads H's dual-MRT accumulators + the
	// upsampled colour (passthrough fallback) + aux (BRDF reuse), applies its
	// own bilateral × BRDF kernel, divides numerator/denominator at the end,
	// and outputs the final RGBA the deferred shader consumes.
	const Flux_PassHandle xDenoiseVPass = xGraph.AddPass(bMainView ? "SSR DenoiseV" : "SSR DenoiseV (Preview)", ExecuteSSRDenoiseV)
		.View(uViewSlot)
		.ClearTargets()
		.Reads          (xGraphics.GetDepthAttachment(uViewSlot),                         RESOURCE_ACCESS_READ_SRV)
		.Reads          (xGraphics.GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT, uViewSlot), RESOURCE_ACCESS_READ_SRV)
		.Reads          (xGraphics.GetMRTAttachment(MRT_INDEX_MATERIAL, uViewSlot),       RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient (m_axUpsampledHandles[uViewSlot],                                 RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient (m_axUpsampledAuxHandles[uViewSlot],                              RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient (m_axDenoiseHHandles[uViewSlot],                                  RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient (m_axDenoiseHConfHandles[uViewSlot],                              RESOURCE_ACCESS_READ_SRV)
		.WritesTransient(m_axDenoiseVHandles[uViewSlot],                                  RESOURCE_ACCESS_WRITE_RTV);

	const bool bRoughnessBlur = Zenith_GraphicsOptions::Get().m_bSSRRoughnessBlurEnabled;
	xGraph.SetEnabled(xDenoiseHPass, bRoughnessBlur);
	xGraph.SetEnabled(xDenoiseVPass, bRoughnessBlur);

	// Commit the handle the deferred pass will now read for this view.
	// GetReflectionHandle resolves to this value — any runtime toggle without a
	// matching g_xEngine.FluxRenderer().RequestGraphRebuild() trips at the point
	// of the mistake, not downstream in Validate() or
	// AssertBoundResourceDeclared. When denoise is off, deferred reads the
	// upsampled (full-res) output — never the raw half-res raymarch. The
	// selection is the enabled bool itself.
	m_axReflectionSelectors[uViewSlot].Commit(m_axDenoiseVHandles[uViewSlot], m_axUpsampledHandles[uViewSlot], bRoughnessBlur, bRoughnessBlur);
}

void Flux_SSRImpl::ApplyBlurSelectionToGraph(Flux_RenderGraph& /*xGraph*/)
{
	// The selection (the blur toggle) is view-independent — every view's
	// selector commits with the same bool — so checking the MAIN slot's
	// selector covers all views.
	if (!m_axReflectionSelectors[kuFluxViewSlotMain].RequestRebuildIfSelectionChanged(Zenith_GraphicsOptions::Get().m_bSSRRoughnessBlurEnabled))
		return;

	// Full graph rebuild — not just xGraph.MarkDirty() — because
	// Flux_DeferredShading::SetupRenderGraph captures the current
	// GetReflectionHandle() value when declaring its Read. MarkDirty triggers a
	// re-Compile on the same declared reads/writes, which would leave the
	// deferred pass reading the stale handle (either an orphan Read whose
	// writer is now disabled, or a valid Read on the wrong transient, with the
	// execute callback binding an SRV the graph hasn't transitioned).
	// g_xEngine.FluxRenderer().RequestGraphRebuild() re-runs every subsystem's SetupRenderGraph
	// on the next frame, so the deferred pass's declared Read resolves to the
	// handle matching the new m_bSSRRoughnessBlurEnabled value. SetupRenderGraph
	// above will also re-Commit the per-view selectors (re-seeding the committed
	// handles) and re-set the denoise passes' enable bits.
	g_xEngine.FluxRenderer().RequestGraphRebuild();
}


Flux_ShaderResourceView& Flux_SSRImpl::GetReflectionSRV(u_int uViewSlot)
{
	// Resolve from the committed handle — the value GetReflectionHandle()
	// returned when Flux_DeferredShading declared its Read — NOT the live
	// graphics option. Between a runtime toggle and the requested rebuild
	// landing (next frame), the live option diverges from the graph for one
	// frame; resolving from the option trips AssertBoundResourceDeclared.
	Zenith_Assert(m_pxGraph, "GetReflectionSRV: graph pointer is null (called before SetupRenderGraph or after Shutdown)");
	return m_pxGraph->GetTransientAttachment(GetReflectionHandle(uViewSlot)).SRV();
}

bool Flux_SSRImpl::IsEnabled() const
{
	return Zenith_GraphicsOptions::Get().m_bSSREnabled && m_bInitialised;
}
