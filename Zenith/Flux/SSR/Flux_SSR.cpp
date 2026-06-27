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
// SetupRenderGraph.
//   g_xEngine.SSR().m_xRayMarchHandle     — half-res RT0, RGB=colour, A=confidence
//   g_xEngine.SSR().m_xRayMarchAuxHandle  — half-res RT1, RG=hitUV, B=travelDist, A=rayCount
//   g_xEngine.SSR().m_xUpsampledHandle    — full-res RT0, bilateral 2x2 upsample of RT0
//   g_xEngine.SSR().m_xUpsampledAuxHandle — full-res RT1, bilateral 2x2 upsample of RT1
//   g_xEngine.SSR().m_xDenoiseHHandle     — full-res, H pass RT0: rgb=Σ(w·color), a=Σ(w)
//   g_xEngine.SSR().m_xDenoiseHConfHandle — full-res, H pass RT1: r=Σ(w·conf) — Phase 3b accumulator
//   g_xEngine.SSR().m_xDenoiseVHandle     — full-res, written by DenoiseV (final denoised output)
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
	// Half-resolution ray-march target dimensions (populated each frame in
	// UpdateSSRConstants from the swapchain). Passed via CBV so the shaders
	// never need GetDimensions() on the bound textures.
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
	// (width, height, 1/width, 1/height) into a float4. Populated each
	// frame in UpdateSSRConstants from the swapchain — replaces in-shader
	// GetDimensions(g_xHiZTex, mip, ...) calls in the ray-march loop.
	// Array size matches Flux_HiZImpl::uHIZ_MAX_MIPS (12).
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

// SSR constants are uploaded once per frame (in the RayMarch pass) and the
// CBV is then bound by every SSR pass that consumes them. Driven this way
// instead of via push constants because the cached HiZ mip-size array
// exceeds the 128-byte push limit, and to honour the project convention of
// passing texture dimensions through CBVs rather than calling GetDimensions
// inside shaders.

// ---- Helpers to get the right attachment regardless of path ----

Flux_RenderAttachment& Flux_SSRImpl::GetRayMarchAttachment()
{
	Zenith_Assert(m_pxGraph, "Flux_SSRImpl::GetRayMarchAttachment: graph pointer is null (called before SetupRenderGraph or after Shutdown)");
	return m_pxGraph->GetTransientAttachment(m_xRayMarchHandle);
}
Flux_RenderAttachment& Flux_SSRImpl::GetRayMarchAuxAttachment()
{
	Zenith_Assert(m_pxGraph, "Flux_SSRImpl::GetRayMarchAuxAttachment: graph pointer is null (called before SetupRenderGraph or after Shutdown)");
	return m_pxGraph->GetTransientAttachment(m_xRayMarchAuxHandle);
}
Flux_RenderAttachment& Flux_SSRImpl::GetUpsampledAttachment()
{
	Zenith_Assert(m_pxGraph, "Flux_SSRImpl::GetUpsampledAttachment: graph pointer is null (called before SetupRenderGraph or after Shutdown)");
	return m_pxGraph->GetTransientAttachment(m_xUpsampledHandle);
}
Flux_RenderAttachment& Flux_SSRImpl::GetUpsampledAuxAttachment()
{
	Zenith_Assert(m_pxGraph, "Flux_SSRImpl::GetUpsampledAuxAttachment: graph pointer is null (called before SetupRenderGraph or after Shutdown)");
	return m_pxGraph->GetTransientAttachment(m_xUpsampledAuxHandle);
}
Flux_RenderAttachment& Flux_SSRImpl::GetDenoiseHAttachment()
{
	Zenith_Assert(m_pxGraph, "Flux_SSRImpl::GetDenoiseHAttachment: graph pointer is null (called before SetupRenderGraph or after Shutdown)");
	return m_pxGraph->GetTransientAttachment(m_xDenoiseHHandle);
}
Flux_RenderAttachment& Flux_SSRImpl::GetDenoiseHConfAttachment()
{
	Zenith_Assert(m_pxGraph, "Flux_SSRImpl::GetDenoiseHConfAttachment: graph pointer is null (called before SetupRenderGraph or after Shutdown)");
	return m_pxGraph->GetTransientAttachment(m_xDenoiseHConfHandle);
}
Flux_RenderAttachment& Flux_SSRImpl::GetDenoiseVAttachment()
{
	Zenith_Assert(m_pxGraph, "Flux_SSRImpl::GetDenoiseVAttachment: graph pointer is null (called before SetupRenderGraph or after Shutdown)");
	return m_pxGraph->GetTransientAttachment(m_xDenoiseVHandle);
}

#ifdef ZENITH_DEBUG_VARIABLES
// Debug-texture callbacks — resolved every ImGui draw so the preview follows
// render-graph rebuilds (the transient SRVs are recreated on resize). Returning
// nullptr before SetupRenderGraph / after Shutdown renders nothing, avoiding
// the null-guard in the getters above.
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

	g_xEngine.FluxMemory().InitialiseDynamicConstantBuffer(
		&dbg_xSSRConstants, sizeof(SSRConstants), m_xSSRConstantsBuffer);

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
	g_xEngine.FluxMemory().DestroyDynamicConstantBuffer(m_xSSRConstantsBuffer);

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_SSR shut down");
}

void Flux_SSRImpl::UpdateSSRConstants()
{
	Flux_Swapchain& xSwapchain = g_xEngine.FluxSwapchain();

	// Update constants from debug variables and HiZ system
	dbg_xSSRConstants.m_uDebugMode = dbg_uDebugMode;
	dbg_xSSRConstants.m_uHiZMipCount = g_xEngine.HiZ().GetMipCount();
	// SSR has no temporal accumulation, so rotating the blue-noise field per
	// frame produces visible shimmer rather than integrating across frames.
	// The blue noise itself is still useful for spatial dithering, multi-ray
	// spread, and feeding the spatial DenoiseH/V passes — only the per-frame
	// rotation has no consumer. Pin to 0 so each pixel samples a stable
	// rotation every frame. (Was implicitly 0 historically because
	// Flux::GetFrameCounter() returned 0 forever — db34fcd2 fixed the
	// underlying counter and surfaced this latent design mismatch.)
	dbg_xSSRConstants.m_uFrameIndex = 0;

	// Resolution-based binary search iterations for sub-pixel hit precision.
	// Half-res ray origins (Phase B) make sub-1/16-pixel precision wasted —
	// the bilateral upsample reconstructs full-res positions anyway.
	// 1080p (≤1920): 4 iterations
	// 1440p / 4K (>1920): 5 iterations
	u_int uWidth = xSwapchain.GetWidth();
	dbg_xSSRConstants.m_uBinarySearchIterations = 4 + (uWidth > 1920 ? 1 : 0);

	// Clamp start mip to valid range
	if (dbg_xSSRConstants.m_uStartMip >= dbg_xSSRConstants.m_uHiZMipCount)
		dbg_xSSRConstants.m_uStartMip = dbg_xSSRConstants.m_uHiZMipCount - 1;

	// Half-res ray-march output dimensions (matches the half-res transient
	// created in SetupRenderGraph; both derive from the swapchain so they
	// stay in sync).
	const u_int uHalfWidth  = std::max(1u, xSwapchain.GetWidth()  / 2u);
	const u_int uHalfHeight = std::max(1u, xSwapchain.GetHeight() / 2u);
	dbg_xSSRConstants.m_fHalfResWidth     = (float)uHalfWidth;
	dbg_xSSRConstants.m_fHalfResHeight    = (float)uHalfHeight;
	dbg_xSSRConstants.m_fRcpHalfResWidth  = 1.0f / (float)uHalfWidth;
	dbg_xSSRConstants.m_fRcpHalfResHeight = 1.0f / (float)uHalfHeight;

	// HiZ per-mip dimensions. HiZ mip 0 matches the swapchain (the depth
	// buffer); each subsequent mip is a 2x2 reduction. Pre-computing the
	// (size, rcp size) here means the ray-march shader doesn't need to
	// call GetDimensions in its inner loop (project convention: dims
	// flow through the CBV).
	const u_int uFullWidth  = xSwapchain.GetWidth();
	const u_int uFullHeight = xSwapchain.GetHeight();
	const u_int uMipCount   = dbg_xSSRConstants.m_uHiZMipCount;
	for (u_int uMip = 0; uMip < uMipCount && uMip < 12u; ++uMip)
	{
		const u_int uW = std::max(1u, uFullWidth  >> uMip);
		const u_int uH = std::max(1u, uFullHeight >> uMip);
		dbg_xSSRConstants.m_axHiZMipSizes[uMip][0] = (float)uW;
		dbg_xSSRConstants.m_axHiZMipSizes[uMip][1] = (float)uH;
		dbg_xSSRConstants.m_axHiZMipSizes[uMip][2] = 1.0f / (float)uW;
		dbg_xSSRConstants.m_axHiZMipSizes[uMip][3] = 1.0f / (float)uH;
	}
}

static void ExecuteSSRRayMarch(Flux_CommandBuffer* pxCommandList, void*)
{
	// Non-capturing graph trampoline: recover the singleton instance first;
	// cross-subsystem deps are reached via g_xEngine at point of use.
	auto& xSSR = g_xEngine.SSR();
	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();

	if (!xSSR.IsEnabled() || !g_xEngine.HiZ().IsEnabled())
		return;

	xSSR.UpdateSSRConstants();

	// First SSR pass of the frame: refresh the CBV. The Resolve (and the
	// new Upsample) pass binds the same CBV without re-uploading.
	g_xEngine.FluxMemory().UploadBufferData(
		xSSR.m_xSSRConstantsBuffer.GetBuffer().m_xVRAMHandle,
		&dbg_xSSRConstants, sizeof(SSRConstants));

	pxCommandList->SetPipeline(&xSSR.m_xRayMarchPipeline);

	pxCommandList->SetVertexBuffer(xGraphics.m_xQuadMesh.GetVertexBuffer());
	pxCommandList->SetIndexBuffer(xGraphics.m_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);

	namespace RM = Flux_Generated_SSR::SSR_RayMarch;
	xBinder.BindCBV(RM::hSSRConstants,   &xSSR.m_xSSRConstantsBuffer.GetCBV());

	xBinder.BindSRV(RM::hg_xDepthTex, xGraphics.GetDepthStencilSRV());
	xBinder.BindSRV(RM::hg_xNormalsTex, xGraphics.GetGBufferSRV(MRT_INDEX_NORMALSAMBIENT));
	xBinder.BindSRV(RM::hg_xMaterialTex, xGraphics.GetGBufferSRV(MRT_INDEX_MATERIAL));
	xBinder.BindSRV(RM::hg_xHiZTex, &g_xEngine.HiZ().GetHiZSRV());
	xBinder.BindSRV(RM::hg_xDiffuseTex, xGraphics.GetGBufferSRV(MRT_INDEX_DIFFUSE));
	xBinder.BindSRV(RM::hg_xBlueNoiseTex, &g_xEngine.VolumeFog().GetBlueNoiseTexture()->m_xSRV);

	pxCommandList->DrawIndexed(6);
}

static void ExecuteSSRUpsample(Flux_CommandBuffer* pxCommandList, void*)
{
	auto& xSSR = g_xEngine.SSR();
	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();

	if (!xSSR.IsEnabled() || !g_xEngine.HiZ().IsEnabled())
		return;

	pxCommandList->SetPipeline(&xSSR.m_xUpsamplePipeline);

	pxCommandList->SetVertexBuffer(xGraphics.m_xQuadMesh.GetVertexBuffer());
	pxCommandList->SetIndexBuffer(xGraphics.m_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);

	namespace US = Flux_Generated_SSR::SSR_Upsample;
	// Half-res dimensions live in the SSR CBV — the upsample shader reads
	// them from there rather than calling GetDimensions on g_xSSRTex.
	xBinder.BindCBV(US::hSSRConstants, &xSSR.m_xSSRConstantsBuffer.GetCBV());

	xBinder.BindSRV(US::hg_xSSRTex,      &xSSR.GetRayMarchAttachment().SRV());
	xBinder.BindSRV(US::hg_xDepthTex,    xGraphics.GetDepthStencilSRV());
	xBinder.BindSRV(US::hg_xSSRAuxTex,   &xSSR.GetRayMarchAuxAttachment().SRV());
	// Phase 5 — KNN composite-similarity scoring needs full-res normals + material.
	xBinder.BindSRV(US::hg_xNormalsTex,  xGraphics.GetGBufferSRV(MRT_INDEX_NORMALSAMBIENT));
	xBinder.BindSRV(US::hg_xMaterialTex, xGraphics.GetGBufferSRV(MRT_INDEX_MATERIAL));

	pxCommandList->DrawIndexed(6);
}

static void ExecuteSSRDenoiseH(Flux_CommandBuffer* pxCommandList, void*)
{
	auto& xSSR = g_xEngine.SSR();
	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();

	if (!xSSR.IsEnabled() || !g_xEngine.HiZ().IsEnabled() || !Zenith_GraphicsOptions::Get().m_bSSRRoughnessBlurEnabled)
		return;

	pxCommandList->SetPipeline(&xSSR.m_xDenoiseHPipeline);

	pxCommandList->SetVertexBuffer(xGraphics.m_xQuadMesh.GetVertexBuffer());
	pxCommandList->SetIndexBuffer(xGraphics.m_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);

	namespace DH = Flux_Generated_SSR::SSR_DenoiseH;
	dbg_xSSRDenoiseConstants.m_bEnabled = Zenith_GraphicsOptions::Get().m_bSSRRoughnessBlurEnabled ? 1u : 0u;
	xBinder.BindDrawConstants(DH::hPushConstants, &dbg_xSSRDenoiseConstants, sizeof(SSRDenoiseConstants));

	xBinder.BindSRV(DH::hg_xSSRUpsampledTex,    &xSSR.GetUpsampledAttachment().SRV());
	xBinder.BindSRV(DH::hg_xDepthTex,           xGraphics.GetDepthStencilSRV());
	xBinder.BindSRV(DH::hg_xNormalsTex,         xGraphics.GetGBufferSRV(MRT_INDEX_NORMALSAMBIENT));
	xBinder.BindSRV(DH::hg_xMaterialTex,        xGraphics.GetGBufferSRV(MRT_INDEX_MATERIAL));
	xBinder.BindSRV(DH::hg_xSSRUpsampledAuxTex, &xSSR.GetUpsampledAuxAttachment().SRV());

	pxCommandList->DrawIndexed(6);
}

static void ExecuteSSRDenoiseV(Flux_CommandBuffer* pxCommandList, void*)
{
	auto& xSSR = g_xEngine.SSR();
	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();

	if (!xSSR.IsEnabled() || !g_xEngine.HiZ().IsEnabled() || !Zenith_GraphicsOptions::Get().m_bSSRRoughnessBlurEnabled)
		return;

	pxCommandList->SetPipeline(&xSSR.m_xDenoiseVPipeline);

	pxCommandList->SetVertexBuffer(xGraphics.m_xQuadMesh.GetVertexBuffer());
	pxCommandList->SetIndexBuffer(xGraphics.m_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);

	namespace DV = Flux_Generated_SSR::SSR_DenoiseV;
	dbg_xSSRDenoiseConstants.m_bEnabled = Zenith_GraphicsOptions::Get().m_bSSRRoughnessBlurEnabled ? 1u : 0u;
	xBinder.BindDrawConstants(DV::hPushConstants, &dbg_xSSRDenoiseConstants, sizeof(SSRDenoiseConstants));

	xBinder.BindSRV(DV::hg_xSSRDenoiseHColTex,  &xSSR.GetDenoiseHAttachment().SRV());
	xBinder.BindSRV(DV::hg_xDepthTex,           xGraphics.GetDepthStencilSRV());
	xBinder.BindSRV(DV::hg_xNormalsTex,         xGraphics.GetGBufferSRV(MRT_INDEX_NORMALSAMBIENT));
	xBinder.BindSRV(DV::hg_xMaterialTex,        xGraphics.GetGBufferSRV(MRT_INDEX_MATERIAL));
	xBinder.BindSRV(DV::hg_xSSRUpsampledAuxTex, &xSSR.GetUpsampledAuxAttachment().SRV());
	xBinder.BindSRV(DV::hg_xSSRDenoiseHConfTex, &xSSR.GetDenoiseHConfAttachment().SRV());
	xBinder.BindSRV(DV::hg_xSSRUpsampledTex,    &xSSR.GetUpsampledAttachment().SRV());

	pxCommandList->DrawIndexed(6);
}

// Pass handles so ApplyBlurSelectionToGraph can toggle their enable bits when
// m_bSSRRoughnessBlurEnabled changes — both H and V toggle together.
// Last value seen by ApplyBlurSelectionToGraph — change triggers a graph rebuild.
// Handle committed at SetupRenderGraph exit. GetReflectionHandle asserts the
// live toggle still resolves to this handle — otherwise a toggle has happened
// without a corresponding g_xEngine.FluxRenderer().RequestGraphRebuild(), which would leave the
// deferred pass's declared Read referencing the stale transient.

void Flux_SSRImpl::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	m_pxGraph = &xGraph;

	Flux_Swapchain& xSwapchain = g_xEngine.FluxSwapchain();
	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();

	// Three-pass SSR pipeline (mirrors SSGI):
	//   RayMarch (half-res) → Upsample (full-res) → Resolve (full-res, optional)
	// The half-res ray-march is the dominant performance win. The Upsample
	// pass is a depth-weighted bilateral 2x2 reconstruction to full-res; it
	// is mandatory so the deferred consumer always reads a full-res image.
	Flux_TransientTextureDesc xHalfDesc;
	xHalfDesc.m_uWidth       = std::max(1u, xSwapchain.GetWidth()  / 2u);
	xHalfDesc.m_uHeight      = std::max(1u, xSwapchain.GetHeight() / 2u);
	xHalfDesc.m_eFormat      = SSR_FORMAT;
	xHalfDesc.m_uMemoryFlags = (1u << MEMORY_FLAGS__SHADER_READ);

	Flux_TransientTextureDesc xFullDesc = xHalfDesc;
	xFullDesc.m_uWidth  = xSwapchain.GetWidth();
	xFullDesc.m_uHeight = xSwapchain.GetHeight();

	m_xRayMarchHandle     = xGraph.CreateTransient(xHalfDesc);
	m_xRayMarchAuxHandle  = xGraph.CreateTransient(xHalfDesc);
	m_xUpsampledHandle    = xGraph.CreateTransient(xFullDesc);
	m_xUpsampledAuxHandle = xGraph.CreateTransient(xFullDesc);
	m_xDenoiseHHandle     = xGraph.CreateTransient(xFullDesc);
	m_xDenoiseHConfHandle = xGraph.CreateTransient(xFullDesc);
	m_xDenoiseVHandle     = xGraph.CreateTransient(xFullDesc);

	// RayMarch pass — first writer of its targets; clear so the initial
	// render-pass LoadOp is valid. Dual-MRT: order matters — RT0 = colour,
	// RT1 = aux metadata (must match the shader's SV_Target indices).
	xGraph.AddPass("SSR RayMarch", ExecuteSSRRayMarch)
		.ClearTargets()
		.Reads          (xGraphics.GetDepthAttachment(),                       RESOURCE_ACCESS_READ_SRV)
		.Reads          (g_xEngine.HiZ().GetHiZAttachment(),                   RESOURCE_ACCESS_READ_SRV, 0, g_xEngine.HiZ().m_uMipCount)
		.Reads          (xGraphics.GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT), RESOURCE_ACCESS_READ_SRV)
		.Reads          (xGraphics.GetMRTAttachment(MRT_INDEX_MATERIAL),       RESOURCE_ACCESS_READ_SRV)
		.Reads          (xGraphics.GetMRTAttachment(MRT_INDEX_DIFFUSE),        RESOURCE_ACCESS_READ_SRV)
		.WritesTransient(m_xRayMarchHandle,                                     RESOURCE_ACCESS_WRITE_RTV)
		.WritesTransient(m_xRayMarchAuxHandle,                                  RESOURCE_ACCESS_WRITE_RTV);

	// Upsample pass — depth-weighted bilateral 2x2 from half to full-res.
	// Always enabled; produces the canonical full-res SSR output. Same dual-MRT
	// shape as RayMarch — RT0 colour, RT1 aux — both reconstructed with the
	// same depth weights so they stay spatially aligned.
	xGraph.AddPass("SSR Upsample", ExecuteSSRUpsample)
		.ClearTargets()
		.Reads          (xGraphics.GetDepthAttachment(),                       RESOURCE_ACCESS_READ_SRV)
		.Reads          (xGraphics.GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT), RESOURCE_ACCESS_READ_SRV)
		.Reads          (xGraphics.GetMRTAttachment(MRT_INDEX_MATERIAL),       RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient (m_xRayMarchHandle,                                     RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient (m_xRayMarchAuxHandle,                                  RESOURCE_ACCESS_READ_SRV)
		.WritesTransient(m_xUpsampledHandle,                                    RESOURCE_ACCESS_WRITE_RTV)
		.WritesTransient(m_xUpsampledAuxHandle,                                 RESOURCE_ACCESS_WRITE_RTV);

	// DenoiseH pass — separable bilateral with BRDF reuse, horizontal half.
	// Dual-MRT output (Phase 3b): RT0 carries the (Σw·color, Σw) accumulator
	// pair, RT1 carries the (Σw·conf) parallel accumulator. V applies the
	// final ratio. ApplyBlurSelectionToGraph toggles its enable bit based on
	// m_bSSRRoughnessBlurEnabled. Roughness gating inside the shader skips
	// smooth/rough pixels.
	m_xDenoiseHPass = xGraph.AddPass("SSR DenoiseH", ExecuteSSRDenoiseH)
		.ClearTargets()
		.Reads          (xGraphics.GetDepthAttachment(),                       RESOURCE_ACCESS_READ_SRV)
		.Reads          (xGraphics.GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT), RESOURCE_ACCESS_READ_SRV)
		.Reads          (xGraphics.GetMRTAttachment(MRT_INDEX_MATERIAL),       RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient (m_xUpsampledHandle,                                    RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient (m_xUpsampledAuxHandle,                                 RESOURCE_ACCESS_READ_SRV)
		.WritesTransient(m_xDenoiseHHandle,                                     RESOURCE_ACCESS_WRITE_RTV)
		.WritesTransient(m_xDenoiseHConfHandle,                                 RESOURCE_ACCESS_WRITE_RTV);

	// DenoiseV pass — vertical half, reads H's dual-MRT accumulators + the
	// upsampled colour (passthrough fallback) + aux (BRDF reuse), applies its
	// own bilateral × BRDF kernel, divides numerator/denominator at the end,
	// and outputs the final RGBA the deferred shader consumes.
	m_xDenoiseVPass = xGraph.AddPass("SSR DenoiseV", ExecuteSSRDenoiseV)
		.ClearTargets()
		.Reads          (xGraphics.GetDepthAttachment(),                       RESOURCE_ACCESS_READ_SRV)
		.Reads          (xGraphics.GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT), RESOURCE_ACCESS_READ_SRV)
		.Reads          (xGraphics.GetMRTAttachment(MRT_INDEX_MATERIAL),       RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient (m_xUpsampledHandle,                                    RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient (m_xUpsampledAuxHandle,                                 RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient (m_xDenoiseHHandle,                                     RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient (m_xDenoiseHConfHandle,                                 RESOURCE_ACCESS_READ_SRV)
		.WritesTransient(m_xDenoiseVHandle,                                     RESOURCE_ACCESS_WRITE_RTV);

	const bool bRoughnessBlur = Zenith_GraphicsOptions::Get().m_bSSRRoughnessBlurEnabled;
	xGraph.SetEnabled(m_xDenoiseHPass, bRoughnessBlur);
	xGraph.SetEnabled(m_xDenoiseVPass, bRoughnessBlur);

	// Commit the handle the deferred pass will now read. GetReflectionHandle
	// resolves to this value — any runtime toggle without a matching
	// g_xEngine.FluxRenderer().RequestGraphRebuild() trips at the point of the
	// mistake, not downstream in Validate() or AssertBoundResourceDeclared.
	// When denoise is off, deferred reads the upsampled (full-res) output —
	// never the raw half-res raymarch. The selection is the enabled bool itself.
	m_xReflectionSelector.Commit(m_xDenoiseVHandle, m_xUpsampledHandle, bRoughnessBlur, bRoughnessBlur);
}

void Flux_SSRImpl::ApplyBlurSelectionToGraph(Flux_RenderGraph& /*xGraph*/)
{
	if (!m_xReflectionSelector.RequestRebuildIfSelectionChanged(Zenith_GraphicsOptions::Get().m_bSSRRoughnessBlurEnabled))
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
	// above will also re-Commit the selector (re-seeding the committed handle)
	// and re-set the resolve pass's enable bit.
	g_xEngine.FluxRenderer().RequestGraphRebuild();
}


Flux_ShaderResourceView& Flux_SSRImpl::GetReflectionSRV()
{
	// Resolve from the committed handle — the value GetReflectionHandle()
	// returned when Flux_DeferredShading declared its Read — NOT the live
	// graphics option. Between a runtime toggle and the requested rebuild
	// landing (next frame), the live option diverges from the graph for one
	// frame; resolving from the option trips AssertBoundResourceDeclared.
	Zenith_Assert(m_pxGraph, "GetReflectionSRV: graph pointer is null (called before SetupRenderGraph or after Shutdown)");
	return m_pxGraph->GetTransientAttachment(GetReflectionHandle()).SRV();
}

bool Flux_SSRImpl::IsEnabled() const
{
	return Zenith_GraphicsOptions::Get().m_bSSREnabled && m_bInitialised;
}

