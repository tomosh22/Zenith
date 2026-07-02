#include "Zenith.h"
#include "Flux/SSGI/Flux_SSGI_Shaders.h"
#include "Flux/Flux_RendererImpl.h"
#include "Core/Zenith_Engine.h"

#include "Flux/SSGI/Flux_SSGIImpl.h"
#include "Flux/HiZ/Flux_HiZImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/HDR/Flux_HDRImpl.h"
#include "Flux/Fog/Flux_VolumeFogImpl.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Flux/Shaders/Generated/SSGI.h" // typed binding handles
#include "Flux/Flux_BackendTypes.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "DebugVariables/Zenith_DebugVariables.h"

// Graph-owned transient handles — backing Flux_RenderAttachments are allocated
// and destroyed by the render graph, sized from the descriptors set in
// SetupRenderGraph.
// Intermediate target between the H and V denoise sub-passes.

// SSGI render target format.
static constexpr TextureFormat SSGI_FORMAT = TEXTURE_FORMAT_R16G16B16A16_SFLOAT;

// Raymarch is rendered at (full / divisor) resolution, then bilateral-upsampled.
// Quarter-res (4) is the default — diffuse GI is low-frequency so the visual
// difference is small while the cost is 4× cheaper than half-res. The bilateral
// upsample's edge weighting handles the larger reconstruction footprint.
//   2 = half-res    (sharper, costlier — old default)
//   4 = quarter-res (default, big perf win)
//   8 = eighth-res  (very fast, visible blockiness)
// Tracks the value SetupRenderGraph used; if the debug var diverges, request a
// graph rebuild so the transient is recreated at the new dimensions.

// Static member definitions

// Debug variables
DEBUGVAR u_int dbg_uDebugMode = SSGI_DEBUG_NONE;

static struct SSGIConstants
{
	float m_fIntensity = 1.0f;          // GI intensity multiplier [0-2]
	float m_fMaxDistance = 30.0f;       // Maximum ray march distance in world units
	float m_fThickness = 0.5f;          // Surface thickness for hit detection
	u_int m_uStepCount = 24;            // Ray march steps (HiZ traversal iterations). Lowered from 32 — most rays hit early and the rest hit screen edges fast.
	u_int m_uFrameIndex = 0;            // For noise variation
	u_int m_uHiZMipCount = 1;           // Filled from Flux_HiZ
	u_int m_uDebugMode = 0;
	// Below this roughness, skip SSGI (very-smooth diffuse-GI rays alias badly;
	// SSR takes those). 0.05 = leave dielectric mirrors to SSR; everything else
	// goes through SSGI.
	float m_fRoughnessThreshold = 0.05f;
	u_int m_uStartMip = 4;              // Starting mip for HiZ traversal
	u_int m_uRaysPerPixel = 2;          // Hemisphere samples per pixel (1-8). Lowered from 3 with separable denoise.
	// Base binary-search iteration count (calibrated for 1080p). The effective
	// count bound to the GPU is computed in UpdateSSGIConstants — the base is
	// bumped at higher resolutions and clamped to 6.
	u_int m_uBinarySearchIterations = 2;
	float _pad1;
} dbg_xSSGIConstants;

// Denoise constants - joint bilateral filter parameters. Shared between the
// horizontal and vertical sub-passes — the layout is identical, only the
// inner-loop direction differs in the shader.
// m_bEnabled is populated from Zenith_GraphicsOptions before each shader bind so
// the GPU sees the same value the CPU gating reads.
static struct SSGIDenoiseConstants
{
	float m_fSpatialSigma = 1.5f;       // Spatial Gaussian sigma (pixels). Tighter for the smaller separable kernel.
	float m_fDepthSigma = 0.02f;        // Depth threshold (fraction of local depth)
	float m_fNormalSigma = 0.5f;        // Normal threshold (1 - dot product range)
	float m_fAlbedoSigma = 0.1f;        // Albedo threshold (color distance)
	u_int m_uKernelRadius = 3;          // Filter radius in pixels per axis (separable, 3 = 7-tap × 2 passes).
	u_int m_bEnabled = 1;               // Mirrors GraphicsOptions::m_bSSGIDenoiseEnabled
	float _pad0;
	float _pad1;
} dbg_xSSGIDenoiseConstants;

// Shaders and pipelines
// Denoise is split into two separable sub-passes (H then V) for a cheap
// approximation of the 2D joint bilateral. Both share the same CB layout.

Flux_RenderAttachment& Flux_SSGIImpl::GetRawResultAttachment(u_int uViewSlot)
{
	Zenith_Assert(m_pxGraph, "GetRawResultAttachment: graph pointer is null (called before SetupRenderGraph or after Shutdown)");
	return m_pxGraph->GetTransientAttachment(m_axRawResultHandles[uViewSlot]);
}
Flux_RenderAttachment& Flux_SSGIImpl::GetResolvedAttachment(u_int uViewSlot)
{
	Zenith_Assert(m_pxGraph, "GetResolvedAttachment: graph pointer is null (called before SetupRenderGraph or after Shutdown)");
	return m_pxGraph->GetTransientAttachment(m_axResolvedHandles[uViewSlot]);
}
Flux_RenderAttachment& Flux_SSGIImpl::GetDenoisedAttachment(u_int uViewSlot)
{
	Zenith_Assert(m_pxGraph, "GetDenoisedAttachment: graph pointer is null (called before SetupRenderGraph or after Shutdown)");
	return m_pxGraph->GetTransientAttachment(m_axDenoisedHandles[uViewSlot]);
}

#ifdef ZENITH_DEBUG_VARIABLES
// Debug-texture callbacks — resolved every ImGui draw so the preview follows
// render-graph rebuilds. Returning nullptr before SetupRenderGraph avoids
// tripping the null-guard asserts in the attachment getters above.
static const Flux_ShaderResourceView* DebugGetRawSRV()
{
	auto& xSSGI = g_xEngine.SSGI();
	if (xSSGI.m_pxGraph == nullptr) return nullptr;
	return &xSSGI.GetRawResultAttachment().SRV();
}
static const Flux_ShaderResourceView* DebugGetResolvedSRV()
{
	auto& xSSGI = g_xEngine.SSGI();
	if (xSSGI.m_pxGraph == nullptr) return nullptr;
	return &xSSGI.GetResolvedAttachment().SRV();
}
static const Flux_ShaderResourceView* DebugGetDenoisedSRV()
{
	auto& xSSGI = g_xEngine.SSGI();
	if (xSSGI.m_pxGraph == nullptr) return nullptr;
	return &xSSGI.GetDenoisedAttachment().SRV();
}
#endif

void Flux_SSGIImpl::BuildPipelines()
{
	Flux_PipelineHelper::BuildFullscreenPipeline(
		m_xRayMarchShader, m_xRayMarchPipeline,
		Flux_SSGIShaders::xSSGI_RayMarch, SSGI_FORMAT);

	Flux_PipelineHelper::BuildFullscreenPipeline(
		m_xUpsampleShader, m_xUpsamplePipeline,
		Flux_SSGIShaders::xSSGI_Upsample, SSGI_FORMAT);

	Flux_PipelineHelper::BuildFullscreenPipeline(
		m_xDenoiseHShader, m_xDenoiseHPipeline,
		Flux_SSGIShaders::xSSGI_DenoiseH, SSGI_FORMAT);

	Flux_PipelineHelper::BuildFullscreenPipeline(
		m_xDenoiseVShader, m_xDenoiseVPipeline,
		Flux_SSGIShaders::xSSGI_DenoiseV, SSGI_FORMAT);
}

void Flux_SSGIImpl::Initialise()
{
	BuildPipelines();

#ifdef ZENITH_DEBUG_VARIABLES
	g_xEngine.DebugVariables().AddUInt32({ "Flux", "SSGI", "DebugMode" }, dbg_uDebugMode, 0, SSGI_DEBUG_COUNT - 1);
	g_xEngine.DebugVariables().AddFloat({ "Flux", "SSGI", "Intensity" }, dbg_xSSGIConstants.m_fIntensity, 0.0f, 2.0f);
	g_xEngine.DebugVariables().AddFloat({ "Flux", "SSGI", "MaxDistance" }, dbg_xSSGIConstants.m_fMaxDistance, 1.0f, 100.0f);
	g_xEngine.DebugVariables().AddFloat({ "Flux", "SSGI", "Thickness" }, dbg_xSSGIConstants.m_fThickness, 0.01f, 2.0f);
	g_xEngine.DebugVariables().AddUInt32({ "Flux", "SSGI", "StepCount" }, dbg_xSSGIConstants.m_uStepCount, 8, 128);
	g_xEngine.DebugVariables().AddUInt32({ "Flux", "SSGI", "StartMip" }, dbg_xSSGIConstants.m_uStartMip, 0, 10);
	g_xEngine.DebugVariables().AddUInt32({ "Flux", "SSGI", "RaysPerPixel" }, dbg_xSSGIConstants.m_uRaysPerPixel, 1, 8);
	g_xEngine.DebugVariables().AddFloat({ "Flux", "SSGI", "RoughnessThreshold" }, dbg_xSSGIConstants.m_fRoughnessThreshold, 0.0f, 1.0f);
	// Raymarch resolution divisor (full / divisor). Triggers a graph rebuild on change.
	// 2=half, 4=quarter (default), 8=eighth. Quality vs perf knob.
	g_xEngine.DebugVariables().AddUInt32({ "Flux", "SSGI", "ResolutionDivisor" }, m_uRayMarchResolutionDivisor, 2, 8);
	// Base 1080p iteration count — UpdateSSGIConstants bumps the bound value
	// at higher resolutions and clamps to 6.
	g_xEngine.DebugVariables().AddUInt32({ "Flux", "SSGI", "BinarySearchIterations" }, dbg_xSSGIConstants.m_uBinarySearchIterations, 1, 6);
	// Transient-SRV previews: AddTextureCallback re-resolves through g_xEngine.SSGI().m_pxGraph
	// each ImGui draw so the preview survives graph rebuilds (resize, toggle).
	g_xEngine.DebugVariables().AddTextureCallback({ "Flux", "SSGI", "Textures", "Raw" },      &DebugGetRawSRV);
	g_xEngine.DebugVariables().AddTextureCallback({ "Flux", "SSGI", "Textures", "Resolved" }, &DebugGetResolvedSRV);
	g_xEngine.DebugVariables().AddTextureCallback({ "Flux", "SSGI", "Textures", "Denoised" }, &DebugGetDenoisedSRV);

	// Denoise debug variables. KernelRadius is per-axis: total separable footprint is (2r+1) along H then V.
	g_xEngine.DebugVariables().AddUInt32({ "Flux", "SSGI", "Denoise", "KernelRadius" }, dbg_xSSGIDenoiseConstants.m_uKernelRadius, 1, 8);
	g_xEngine.DebugVariables().AddFloat({ "Flux", "SSGI", "Denoise", "SpatialSigma" }, dbg_xSSGIDenoiseConstants.m_fSpatialSigma, 0.5f, 4.0f);
	g_xEngine.DebugVariables().AddFloat({ "Flux", "SSGI", "Denoise", "DepthSigma" }, dbg_xSSGIDenoiseConstants.m_fDepthSigma, 0.01f, 0.1f);
	g_xEngine.DebugVariables().AddFloat({ "Flux", "SSGI", "Denoise", "NormalSigma" }, dbg_xSSGIDenoiseConstants.m_fNormalSigma, 0.1f, 1.0f);
	g_xEngine.DebugVariables().AddFloat({ "Flux", "SSGI", "Denoise", "AlbedoSigma" }, dbg_xSSGIDenoiseConstants.m_fAlbedoSigma, 0.05f, 0.5f);
#endif

	// No resize callback needed — the graph re-creates its transients with
	// updated dimensions when SetupRenderGraph re-runs on resize.

	m_bInitialised = true;
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_SSGI initialised");
}

void Flux_SSGIImpl::ShutdownImpl()
{
	// CRTP hook from Flux_ScreenSpaceEffectBase::Shutdown(); the base owns the
	// m_bInitialised guard and the m_pxGraph / m_bInitialised resets. SSGI has
	// no per-effect GPU resources to release (no CBV), so this only logs.
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_SSGI shut down");
}

// Pulled out of UpdateSSGIConstants so the executes can build a per-frame
// snapshot without mutating the debug-var-bound storage (which the ImGui UI
// reads/writes every frame). The base value is the user-tuned 1080p target;
// the effective value is bumped for higher VIEW resolutions and clamped
// (slot 0 = the swapchain width captured at setup, so the main view is
// unchanged; the preview's 512 base never bumps).
u_int Flux_SSGIImpl::ComputeEffectiveBinarySearchIterations(u_int uViewSlot) const
{
	const u_int uBase = dbg_xSSGIConstants.m_uBinarySearchIterations;
	const u_int uW    = m_auViewWidths[uViewSlot];
	const u_int uBumped = uBase + (uW > 1920u ? 1u : 0u) + (uW > 2560u ? 1u : 0u);
	// Ceiling at 6 — diffuse GI doesn't need finer than ~1/64 px sub-pixel hits.
	return uBumped > 6u ? 6u : uBumped;
}

void Flux_SSGIImpl::UpdateSSGIConstants()
{
	// View-independent refreshes on the debug-var-bound tuning struct. Every
	// view's record callback writes the SAME values here (record callbacks can
	// run on parallel workers), so the shared mutation is benign; the
	// view-dependent fields (HiZ mip count / start-mip clamp / iteration bump)
	// are overridden on ExecuteSSGIRayMarch's frame-local snapshot. The MAIN
	// chain's mip count + clamp stay here so the debug-var UI write-back is
	// preserved.
	dbg_xSSGIConstants.m_uDebugMode = dbg_uDebugMode;
	dbg_xSSGIConstants.m_uHiZMipCount = g_xEngine.HiZ().GetMipCount();
	// SSGI has no temporal accumulation, so rotating the blue-noise field per
	// frame produces visible shimmer rather than integrating across frames.
	// The blue noise itself is still useful for spatial dithering, multi-ray
	// spread, and feeding the spatial DenoiseH/V passes — only the per-frame
	// rotation has no consumer. Pin to 0 so each pixel samples a stable
	// rotation every frame. (Was implicitly 0 historically because
	// Flux::GetFrameCounter() returned 0 forever — db34fcd2 fixed the
	// underlying counter and surfaced this latent design mismatch.)
	dbg_xSSGIConstants.m_uFrameIndex = 0;

	// Clamp start mip to valid range
	if (dbg_xSSGIConstants.m_uStartMip >= dbg_xSSGIConstants.m_uHiZMipCount)
		dbg_xSSGIConstants.m_uStartMip = dbg_xSSGIConstants.m_uHiZMipCount - 1;
}

static void ExecuteSSGIRayMarch(Flux_CommandBuffer* pxCommandList, void*)
{
	auto& xSSGI = g_xEngine.SSGI();
	if (!xSSGI.IsEnabled() || !g_xEngine.HiZ().IsEnabled())
		return;

	Zenith_Profiling::ScopeZone xScope(ZENITH_PROFILE_ZONE("Flux SSGI"));

	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();

	// The pass's declared view slot selects this view's chain (transients,
	// G-buffer, HiZ pyramid).
	const u_int uViewSlot = Flux_RenderGraph::GetCurrentRecordingPassViewSlot();

	xSSGI.UpdateSSGIConstants();

	xGraphics.BindFullscreenQuad(*pxCommandList, xSSGI.m_xRayMarchPipeline);

	Flux_ShaderBinder xBinder(*pxCommandList);
	namespace RM = Flux_Generated_SSGI::SSGI_RayMarch;

	// Bind a frame-local snapshot so the resolution-bumped iteration count
	// goes to the GPU without mutating dbg_xSSGIConstants (which the debug-var
	// system reads back into ImGui every frame — a write here would surprise
	// the user with a different value than they typed).
	SSGIConstants xFrameConstants = dbg_xSSGIConstants;
	xFrameConstants.m_uBinarySearchIterations = xSSGI.ComputeEffectiveBinarySearchIterations(uViewSlot);
	// Per-view HiZ chain: the preview's 512² pyramid has fewer mips than the
	// main chain — override the count + re-clamp the start mip on the
	// snapshot (both no-ops for the main view).
	xFrameConstants.m_uHiZMipCount = g_xEngine.HiZ().GetMipCount(uViewSlot);
	if (xFrameConstants.m_uStartMip >= xFrameConstants.m_uHiZMipCount)
		xFrameConstants.m_uStartMip = xFrameConstants.m_uHiZMipCount - 1;

	xBinder.BindDrawConstants(RM::hSSGIConstants, &xFrameConstants, sizeof(SSGIConstants));

	xBinder.BindSRV(RM::hg_xDepthTex, xGraphics.GetDepthStencilSRV(uViewSlot));
	xBinder.BindSRV(RM::hg_xNormalsTex, xGraphics.GetGBufferSRV(MRT_INDEX_NORMALSAMBIENT, uViewSlot));
	xBinder.BindSRV(RM::hg_xMaterialTex, xGraphics.GetGBufferSRV(MRT_INDEX_MATERIAL, uViewSlot));
	xBinder.BindSRV(RM::hg_xHiZTex, &g_xEngine.HiZ().GetHiZSRV(uViewSlot));
	xBinder.BindSRV(RM::hg_xDiffuseTex, xGraphics.GetGBufferSRV(MRT_INDEX_DIFFUSE, uViewSlot));
	xBinder.BindSRV(RM::hg_xBlueNoiseTex, &g_xEngine.VolumeFog().GetBlueNoiseTexture()->m_xSRV);

	pxCommandList->DrawIndexed(6);
}

static void ExecuteSSGIUpsample(Flux_CommandBuffer* pxCommandList, void*)
{
	auto& xSSGI = g_xEngine.SSGI();
	if (!xSSGI.IsEnabled() || !g_xEngine.HiZ().IsEnabled())
		return;

	Zenith_Profiling::ScopeZone xScope(ZENITH_PROFILE_ZONE("Flux SSGI"));

	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();

	const u_int uViewSlot = Flux_RenderGraph::GetCurrentRecordingPassViewSlot();

	xGraphics.BindFullscreenQuad(*pxCommandList, xSSGI.m_xUpsamplePipeline);

	Flux_ShaderBinder xBinder(*pxCommandList);
	namespace US = Flux_Generated_SSGI::SSGI_Upsample;

	xBinder.BindSRV(US::hg_xSSGITex, &xSSGI.GetRawResultAttachment(uViewSlot).SRV());
	xBinder.BindSRV(US::hg_xDepthTex, xGraphics.GetDepthStencilSRV(uViewSlot));

	pxCommandList->DrawIndexed(6);
}

// Horizontal denoise sub-pass — reads the upsampled SSGI, writes the H
// intermediate. The same constants drive both H and V; the inner loop
// direction is the only difference between the two shaders.
static void ExecuteSSGIDenoiseH(Flux_CommandBuffer* pxCommandList, void*)
{
	auto& xSSGI = g_xEngine.SSGI();
	if (!xSSGI.IsEnabled() || !g_xEngine.HiZ().IsEnabled())
		return;

	Zenith_Profiling::ScopeZone xScope(ZENITH_PROFILE_ZONE("Flux SSGI"));

	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();

	const u_int uViewSlot = Flux_RenderGraph::GetCurrentRecordingPassViewSlot();

	xGraphics.BindFullscreenQuad(*pxCommandList, xSSGI.m_xDenoiseHPipeline);

	Flux_ShaderBinder xBinder(*pxCommandList);
	namespace DH = Flux_Generated_SSGI::SSGI_DenoiseH;

	// Frame-local snapshot so the per-view (main + preview) records — which run on
	// parallel workers — never write the shared file-scope dbg_xSSGIDenoiseConstants
	// (mirrors the RayMarch frame-local snapshot in ExecuteSSGIRayMarch).
	SSGIDenoiseConstants xLocal = dbg_xSSGIDenoiseConstants;
	xLocal.m_bEnabled = Zenith_GraphicsOptions::Get().m_bSSGIDenoiseEnabled ? 1u : 0u;
	xBinder.BindDrawConstants(DH::hPushConstants, &xLocal, sizeof(SSGIDenoiseConstants));

	xBinder.BindSRV(DH::hg_xSSGITex,    &xSSGI.GetResolvedAttachment(uViewSlot).SRV());
	xBinder.BindSRV(DH::hg_xDepthTex,   xGraphics.GetDepthStencilSRV(uViewSlot));
	xBinder.BindSRV(DH::hg_xNormalsTex, xGraphics.GetGBufferSRV(MRT_INDEX_NORMALSAMBIENT, uViewSlot));
	xBinder.BindSRV(DH::hg_xAlbedoTex,  xGraphics.GetGBufferSRV(MRT_INDEX_DIFFUSE, uViewSlot));

	pxCommandList->DrawIndexed(6);
}

// Vertical denoise sub-pass — reads the H intermediate, writes the final
// denoised result that deferred shading consumes.
static void ExecuteSSGIDenoiseV(Flux_CommandBuffer* pxCommandList, void*)
{
	auto& xSSGI = g_xEngine.SSGI();
	if (!xSSGI.IsEnabled() || !g_xEngine.HiZ().IsEnabled())
		return;

	Zenith_Profiling::ScopeZone xScope(ZENITH_PROFILE_ZONE("Flux SSGI"));

	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();

	const u_int uViewSlot = Flux_RenderGraph::GetCurrentRecordingPassViewSlot();

	xGraphics.BindFullscreenQuad(*pxCommandList, xSSGI.m_xDenoiseVPipeline);

	Flux_ShaderBinder xBinder(*pxCommandList);
	namespace DV = Flux_Generated_SSGI::SSGI_DenoiseV;

	// Frame-local snapshot (see ExecuteSSGIDenoiseH): no shared write under parallel per-view record.
	SSGIDenoiseConstants xLocal = dbg_xSSGIDenoiseConstants;
	xLocal.m_bEnabled = Zenith_GraphicsOptions::Get().m_bSSGIDenoiseEnabled ? 1u : 0u;
	xBinder.BindDrawConstants(DV::hPushConstants, &xLocal, sizeof(SSGIDenoiseConstants));

	xBinder.BindSRV(DV::hg_xSSGITex,    &xSSGI.m_pxGraph->GetTransientAttachment(xSSGI.m_axDenoiseHHandles[uViewSlot]).SRV());
	xBinder.BindSRV(DV::hg_xDepthTex,   xGraphics.GetDepthStencilSRV(uViewSlot));
	xBinder.BindSRV(DV::hg_xNormalsTex, xGraphics.GetGBufferSRV(MRT_INDEX_NORMALSAMBIENT, uViewSlot));
	xBinder.BindSRV(DV::hg_xAlbedoTex,  xGraphics.GetGBufferSRV(MRT_INDEX_DIFFUSE, uViewSlot));

	pxCommandList->DrawIndexed(6);
}

// The H/V denoise pass handles are locals in SetupViewPasses (their enable
// bits are set right after declaration). Per-view committed handles are
// re-seeded at SetupRenderGraph exit. GetSSGIHandle asserts the live toggle
// still resolves to the committed handle — any runtime toggle without a
// matching g_xEngine.FluxRenderer().RequestGraphRebuild() trips at the point
// of the mistake.

void Flux_SSGIImpl::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	m_pxGraph = &xGraph;

	// Main view at swapchain dims (byte-equivalent to the historical
	// single-view path), then the preview view at its fixed 512² dims — only
	// while active, so its transients exist exactly when its passes do (the
	// graph's unused-transient validation demands this).
	SetupViewPasses(xGraph, kuFluxViewSlotMain, g_xEngine.FluxSwapchain().GetWidth(), g_xEngine.FluxSwapchain().GetHeight());
	if (g_xEngine.FluxGraphics().RenderViews().IsViewActive(kuFluxViewSlotPreview))
		SetupViewPasses(xGraph, kuFluxViewSlotPreview, kuFLUX_PREVIEW_VIEW_SIZE, kuFLUX_PREVIEW_VIEW_SIZE);
}

void Flux_SSGIImpl::SetupViewPasses(Flux_RenderGraph& xGraph, u_int uViewSlot, u_int uWidth, u_int uHeight)
{
	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();

	// Per-view base width — ComputeEffectiveBinarySearchIterations scales its
	// resolution bump from this at record time.
	m_auViewWidths[uViewSlot] = uWidth;

	// Raymarch target resolution = view base / m_uRayMarchResolutionDivisor.
	// Track the value used so ApplyDenoiseSelectionToGraph can detect runtime
	// changes (the divisor is view-independent).
	const u_int uDivisor    = m_uRayMarchResolutionDivisor < 2u ? 2u : m_uRayMarchResolutionDivisor;
	m_uLastResolutionDivisor = uDivisor;
	const u_int uRayWidth   = uWidth  / uDivisor;
	const u_int uRayHeight  = uHeight / uDivisor;

	// Raw result is at the divisor's resolution; resolved / denoised are full-res.
	Flux_TransientTextureDesc xRayDesc;
	xRayDesc.m_uWidth       = uRayWidth;
	xRayDesc.m_uHeight      = uRayHeight;
	xRayDesc.m_eFormat      = SSGI_FORMAT;
	xRayDesc.m_uMemoryFlags = (1u << MEMORY_FLAGS__SHADER_READ);
	m_axRawResultHandles[uViewSlot] = xGraph.CreateTransient(xRayDesc);

	Flux_TransientTextureDesc xFull = xRayDesc;
	xFull.m_uWidth  = uWidth;
	xFull.m_uHeight = uHeight;
	m_axResolvedHandles[uViewSlot] = xGraph.CreateTransient(xFull);
	// Intermediate between the H and V denoise sub-passes — same dimensions
	// and format as the resolved/denoised targets.
	m_axDenoiseHHandles[uViewSlot] = xGraph.CreateTransient(xFull);
	m_axDenoisedHandles[uViewSlot] = xGraph.CreateTransient(xFull);

	// Pass names must be per-view unique + static-lifetime (duplicate names
	// are a hard assert). View 0 keeps the historical names (profiling /
	// FindPass stability).
	const bool bMainView = (uViewSlot == kuFluxViewSlotMain);

	// RayMarch pass (half-res) — reads G-Buffer + HiZ, writes raw result.
	xGraph.AddPass(bMainView ? "SSGI RayMarch" : "SSGI RayMarch (Preview)", ExecuteSSGIRayMarch)
		.View(uViewSlot)
		.ClearTargets()
		.Reads          (xGraphics.GetDepthAttachment(uViewSlot),                         RESOURCE_ACCESS_READ_SRV)
		.Reads          (g_xEngine.HiZ().GetHiZAttachment(uViewSlot),                     RESOURCE_ACCESS_READ_SRV, 0, g_xEngine.HiZ().GetMipCount(uViewSlot))
		.Reads          (xGraphics.GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT, uViewSlot), RESOURCE_ACCESS_READ_SRV)
		.Reads          (xGraphics.GetMRTAttachment(MRT_INDEX_MATERIAL, uViewSlot),       RESOURCE_ACCESS_READ_SRV)
		.Reads          (xGraphics.GetMRTAttachment(MRT_INDEX_DIFFUSE, uViewSlot),        RESOURCE_ACCESS_READ_SRV)
		.WritesTransient(m_axRawResultHandles[uViewSlot],                                 RESOURCE_ACCESS_WRITE_RTV);

	// Upsample pass — half→full res bilateral upsample.
	xGraph.AddPass(bMainView ? "SSGI Upsample" : "SSGI Upsample (Preview)", ExecuteSSGIUpsample)
		.View(uViewSlot)
		.ClearTargets()
		.Reads          (xGraphics.GetDepthAttachment(uViewSlot), RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient (m_axRawResultHandles[uViewSlot],         RESOURCE_ACCESS_READ_SRV)
		.WritesTransient(m_axResolvedHandles[uViewSlot],          RESOURCE_ACCESS_WRITE_RTV);

	// Separable joint-bilateral denoise — split into horizontal then vertical
	// sub-passes. Registered unconditionally; the enable bits track
	// m_bSSGIDenoiseEnabled (ApplyDenoiseSelectionToGraph forces a rebuild when
	// the toggle changes).
	const Flux_PassHandle xDenoisePassH = xGraph.AddPass(bMainView ? "SSGI Denoise H" : "SSGI Denoise H (Preview)", ExecuteSSGIDenoiseH)
		.View(uViewSlot)
		.ClearTargets()
		.Reads          (xGraphics.GetDepthAttachment(uViewSlot),                         RESOURCE_ACCESS_READ_SRV)
		.Reads          (xGraphics.GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT, uViewSlot), RESOURCE_ACCESS_READ_SRV)
		.Reads          (xGraphics.GetMRTAttachment(MRT_INDEX_DIFFUSE, uViewSlot),        RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient (m_axResolvedHandles[uViewSlot],                                  RESOURCE_ACCESS_READ_SRV)
		.WritesTransient(m_axDenoiseHHandles[uViewSlot],                                  RESOURCE_ACCESS_WRITE_RTV);

	const Flux_PassHandle xDenoisePassV = xGraph.AddPass(bMainView ? "SSGI Denoise V" : "SSGI Denoise V (Preview)", ExecuteSSGIDenoiseV)
		.View(uViewSlot)
		.ClearTargets()
		.Reads          (xGraphics.GetDepthAttachment(uViewSlot),                         RESOURCE_ACCESS_READ_SRV)
		.Reads          (xGraphics.GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT, uViewSlot), RESOURCE_ACCESS_READ_SRV)
		.Reads          (xGraphics.GetMRTAttachment(MRT_INDEX_DIFFUSE, uViewSlot),        RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient (m_axDenoiseHHandles[uViewSlot],                                  RESOURCE_ACCESS_READ_SRV)
		.WritesTransient(m_axDenoisedHandles[uViewSlot],                                  RESOURCE_ACCESS_WRITE_RTV);

	const bool bDenoise = Zenith_GraphicsOptions::Get().m_bSSGIDenoiseEnabled;
	xGraph.SetEnabled(xDenoisePassH, bDenoise);
	xGraph.SetEnabled(xDenoisePassV, bDenoise);

	// Commit the handle the deferred pass will now read for this view.
	// GetSSGIHandle resolves to this value — any runtime toggle without a
	// matching g_xEngine.FluxRenderer().RequestGraphRebuild() trips at the
	// point of the mistake. The composite selection captures BOTH the denoise
	// toggle and the (clamped) resolution divisor, so a divisor change also
	// forces a rebuild.
	m_axSSGISelectors[uViewSlot].Commit(m_axDenoisedHandles[uViewSlot], m_axResolvedHandles[uViewSlot], bDenoise, Flux_SSGISelection{ bDenoise, uDivisor });
}

void Flux_SSGIImpl::ApplyDenoiseSelectionToGraph(Flux_RenderGraph& /*xGraph*/)
{
	// The composite selection covers BOTH triggers in one comparison: the
	// denoise toggle and the raymarch resolution divisor (which resizes the
	// raymarch transient). This is where the composite TSelection earns its
	// keep. The selection is view-independent — every view's selector commits
	// with the same value — so checking the MAIN slot covers all views.
	const Flux_SSGISelection xSelection{ Zenith_GraphicsOptions::Get().m_bSSGIDenoiseEnabled, m_uRayMarchResolutionDivisor };
	if (!m_axSSGISelectors[kuFluxViewSlotMain].RequestRebuildIfSelectionChanged(xSelection))
		return;

	// Full graph rebuild — see Flux_SSR::ApplyBlurSelectionToGraph for the same
	// rationale. MarkDirty() alone would leave Flux_DeferredShading reading the
	// stale transient handle captured at the previous SetupRenderGraph; same
	// applies if the raymarch transient's dimensions change.
	g_xEngine.FluxRenderer().RequestGraphRebuild();
}


Flux_ShaderResourceView& Flux_SSGIImpl::GetSSGISRV(u_int uViewSlot)
{
	// Resolve from the committed handle — the value GetSSGIHandle() returned
	// when Flux_DeferredShading declared its Read — NOT the live graphics
	// option. Between a runtime toggle and the requested rebuild landing
	// (next frame), the live option diverges from the graph for one frame;
	// resolving from the option trips AssertBoundResourceDeclared.
	Zenith_Assert(m_pxGraph, "GetSSGISRV: graph pointer is null (called before SetupRenderGraph or after Shutdown)");
	return m_pxGraph->GetTransientAttachment(GetSSGIHandle(uViewSlot)).SRV();
}

bool Flux_SSGIImpl::IsEnabled() const
{
	return Zenith_GraphicsOptions::Get().m_bSSGIEnabled && m_bInitialised;
}

