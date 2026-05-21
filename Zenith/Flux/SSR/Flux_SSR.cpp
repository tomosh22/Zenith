#include "Zenith.h"
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
#include "AssetHandling/Zenith_TextureAsset.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#ifdef ZENITH_TOOLS
#include "Flux/Slang/Flux_ShaderHotReload.h"
#endif

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
	Zenith_Assert(g_xEngine.SSR().m_pxGraph, "g_xEngine.SSR().GetRayMarchAttachment: graph pointer is null (called before SetupRenderGraph or after Shutdown)");
	return g_xEngine.SSR().m_pxGraph->GetTransientAttachment(g_xEngine.SSR().m_xRayMarchHandle);
}
Flux_RenderAttachment& Flux_SSRImpl::GetRayMarchAuxAttachment()
{
	Zenith_Assert(g_xEngine.SSR().m_pxGraph, "g_xEngine.SSR().GetRayMarchAuxAttachment: graph pointer is null (called before SetupRenderGraph or after Shutdown)");
	return g_xEngine.SSR().m_pxGraph->GetTransientAttachment(g_xEngine.SSR().m_xRayMarchAuxHandle);
}
Flux_RenderAttachment& Flux_SSRImpl::GetUpsampledAttachment()
{
	Zenith_Assert(g_xEngine.SSR().m_pxGraph, "g_xEngine.SSR().GetUpsampledAttachment: graph pointer is null (called before SetupRenderGraph or after Shutdown)");
	return g_xEngine.SSR().m_pxGraph->GetTransientAttachment(g_xEngine.SSR().m_xUpsampledHandle);
}
Flux_RenderAttachment& Flux_SSRImpl::GetUpsampledAuxAttachment()
{
	Zenith_Assert(g_xEngine.SSR().m_pxGraph, "g_xEngine.SSR().GetUpsampledAuxAttachment: graph pointer is null (called before SetupRenderGraph or after Shutdown)");
	return g_xEngine.SSR().m_pxGraph->GetTransientAttachment(g_xEngine.SSR().m_xUpsampledAuxHandle);
}
Flux_RenderAttachment& Flux_SSRImpl::GetDenoiseHAttachment()
{
	Zenith_Assert(g_xEngine.SSR().m_pxGraph, "g_xEngine.SSR().GetDenoiseHAttachment: graph pointer is null (called before SetupRenderGraph or after Shutdown)");
	return g_xEngine.SSR().m_pxGraph->GetTransientAttachment(g_xEngine.SSR().m_xDenoiseHHandle);
}
Flux_RenderAttachment& Flux_SSRImpl::GetDenoiseHConfAttachment()
{
	Zenith_Assert(g_xEngine.SSR().m_pxGraph, "g_xEngine.SSR().GetDenoiseHConfAttachment: graph pointer is null (called before SetupRenderGraph or after Shutdown)");
	return g_xEngine.SSR().m_pxGraph->GetTransientAttachment(g_xEngine.SSR().m_xDenoiseHConfHandle);
}
Flux_RenderAttachment& Flux_SSRImpl::GetDenoiseVAttachment()
{
	Zenith_Assert(g_xEngine.SSR().m_pxGraph, "g_xEngine.SSR().GetDenoiseVAttachment: graph pointer is null (called before SetupRenderGraph or after Shutdown)");
	return g_xEngine.SSR().m_pxGraph->GetTransientAttachment(g_xEngine.SSR().m_xDenoiseVHandle);
}

#ifdef ZENITH_DEBUG_VARIABLES
// Debug-texture callbacks — resolved every ImGui draw so the preview follows
// render-graph rebuilds (the transient SRVs are recreated on resize). Returning
// nullptr before SetupRenderGraph / after Shutdown renders nothing, avoiding
// the null-guard in the getters above.
static const Flux_ShaderResourceView* DebugGetRayMarchSRV()
{
	if (g_xEngine.SSR().m_pxGraph == nullptr) return nullptr;
	return &g_xEngine.SSR().GetRayMarchAttachment().SRV();
}
static const Flux_ShaderResourceView* DebugGetRayMarchAuxSRV()
{
	if (g_xEngine.SSR().m_pxGraph == nullptr) return nullptr;
	return &g_xEngine.SSR().GetRayMarchAuxAttachment().SRV();
}
static const Flux_ShaderResourceView* DebugGetUpsampledSRV()
{
	if (g_xEngine.SSR().m_pxGraph == nullptr) return nullptr;
	return &g_xEngine.SSR().GetUpsampledAttachment().SRV();
}
static const Flux_ShaderResourceView* DebugGetUpsampledAuxSRV()
{
	if (g_xEngine.SSR().m_pxGraph == nullptr) return nullptr;
	return &g_xEngine.SSR().GetUpsampledAuxAttachment().SRV();
}
static const Flux_ShaderResourceView* DebugGetDenoiseHSRV()
{
	if (g_xEngine.SSR().m_pxGraph == nullptr) return nullptr;
	return &g_xEngine.SSR().GetDenoiseHAttachment().SRV();
}
static const Flux_ShaderResourceView* DebugGetDenoiseHConfSRV()
{
	if (g_xEngine.SSR().m_pxGraph == nullptr) return nullptr;
	return &g_xEngine.SSR().GetDenoiseHConfAttachment().SRV();
}
static const Flux_ShaderResourceView* DebugGetDenoiseVSRV()
{
	if (g_xEngine.SSR().m_pxGraph == nullptr) return nullptr;
	return &g_xEngine.SSR().GetDenoiseVAttachment().SRV();
}
#endif

void Flux_SSRImpl::BuildPipelines()
{
	// RayMarch + Upsample are dual-MRT: RT0 carries colour+confidence (legacy
	// shape), RT1 carries hit metadata (UV / travel distance / ray count) for
	// downstream Phase 3b denoise BRDF reuse and Phase 4 variance.
	{
		Flux_PipelineSpecification xSpec = Flux_PipelineHelper::CreateFullscreenSpec(
			g_xEngine.SSR().m_xRayMarchShader, FluxShaderProgram::SSR_RayMarch, SSR_FORMAT);
		xSpec.m_aeColourAttachmentFormats[1] = SSR_FORMAT;
		xSpec.m_uNumColourAttachments        = 2;
		Flux_PipelineBuilder::FromSpecification(g_xEngine.SSR().m_xRayMarchPipeline, xSpec);
	}

	{
		Flux_PipelineSpecification xSpec = Flux_PipelineHelper::CreateFullscreenSpec(
			g_xEngine.SSR().m_xUpsampleShader, FluxShaderProgram::SSR_Upsample, SSR_FORMAT);
		xSpec.m_aeColourAttachmentFormats[1] = SSR_FORMAT;
		xSpec.m_uNumColourAttachments        = 2;
		Flux_PipelineBuilder::FromSpecification(g_xEngine.SSR().m_xUpsamplePipeline, xSpec);
	}

	// DenoiseH is dual-MRT (Phase 3b): RT0 = Σ(w·color)+Σw, RT1 = Σ(w·conf).
	{
		Flux_PipelineSpecification xSpec = Flux_PipelineHelper::CreateFullscreenSpec(
			g_xEngine.SSR().m_xDenoiseHShader, FluxShaderProgram::SSR_DenoiseH, SSR_FORMAT);
		xSpec.m_aeColourAttachmentFormats[1] = SSR_FORMAT;
		xSpec.m_uNumColourAttachments        = 2;
		Flux_PipelineBuilder::FromSpecification(g_xEngine.SSR().m_xDenoiseHPipeline, xSpec);
	}

	Flux_PipelineHelper::BuildFullscreenPipeline(
		g_xEngine.SSR().m_xDenoiseVShader, g_xEngine.SSR().m_xDenoiseVPipeline,
		FluxShaderProgram::SSR_DenoiseV, SSR_FORMAT);
}

void Flux_SSRImpl::Initialise()
{
	BuildPipelines();

	Flux_MemoryManager::InitialiseDynamicConstantBuffer(
		&dbg_xSSRConstants, sizeof(SSRConstants), g_xEngine.SSR().m_xSSRConstantsBuffer);

#ifdef ZENITH_TOOLS
	static const FluxShaderProgram s_axPrograms[] = {
		FluxShaderProgram::SSR_RayMarch,
		FluxShaderProgram::SSR_Upsample,
		FluxShaderProgram::SSR_DenoiseH,
		FluxShaderProgram::SSR_DenoiseV,
	};
	Flux_ShaderHotReload::RegisterSubsystem([](){ g_xEngine.SSR().BuildPipelines(); },
		s_axPrograms, sizeof(s_axPrograms) / sizeof(s_axPrograms[0]));
#endif

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddUInt32({ "Flux", "SSR", "DebugMode" }, dbg_uDebugMode, 0, 100);  // Extended range for diagnostic mode 99
	Zenith_DebugVariables::AddFloat({ "Flux", "SSR", "Intensity" }, dbg_xSSRConstants.m_fIntensity, 0.0f, 2.0f);
	Zenith_DebugVariables::AddFloat({ "Flux", "SSR", "MaxDistance" }, dbg_xSSRConstants.m_fMaxDistance, 1.0f, 100.0f);
	Zenith_DebugVariables::AddFloat({ "Flux", "SSR", "MaxRoughness" }, dbg_xSSRConstants.m_fMaxRoughness, 0.0f, 1.0f);
	Zenith_DebugVariables::AddFloat({ "Flux", "SSR", "Thickness" }, dbg_xSSRConstants.m_fThickness, 0.01f, 1.0f);
	Zenith_DebugVariables::AddUInt32({ "Flux", "SSR", "StepCount" }, dbg_xSSRConstants.m_uStepCount, 8, 256);
	Zenith_DebugVariables::AddUInt32({ "Flux", "SSR", "StartMip" }, dbg_xSSRConstants.m_uStartMip, 0, 10);
	Zenith_DebugVariables::AddFloat({ "Flux", "SSR", "ContactHardeningDist" }, dbg_xSSRConstants.m_fContactHardeningDist, 0.5f, 10.0f);
	Zenith_DebugVariables::AddUInt32({ "Flux", "SSR", "RaysPerPixel" }, dbg_xSSRConstants.m_uRayCount, 1, 4);
	Zenith_DebugVariables::AddUInt32({ "Flux", "SSR", "StepCountMin" }, dbg_xSSRConstants.m_uStepCountMin, 4, 64);
	// Transient-SRV previews use AddTextureCallback so the SRV is re-resolved
	// through g_xEngine.SSR().m_pxGraph on every ImGui draw. Storing a stale pointer via
	// AddTexture would go dangling once the graph rebuilds (e.g. on resize).
	Zenith_DebugVariables::AddTextureCallback({ "Flux", "SSR", "Textures", "RayMarch" },     &DebugGetRayMarchSRV);
	Zenith_DebugVariables::AddTextureCallback({ "Flux", "SSR", "Textures", "RayMarchAux" },  &DebugGetRayMarchAuxSRV);
	Zenith_DebugVariables::AddTextureCallback({ "Flux", "SSR", "Textures", "Upsampled" },    &DebugGetUpsampledSRV);
	Zenith_DebugVariables::AddTextureCallback({ "Flux", "SSR", "Textures", "UpsampledAux" }, &DebugGetUpsampledAuxSRV);
	Zenith_DebugVariables::AddTextureCallback({ "Flux", "SSR", "Textures", "DenoiseH" },     &DebugGetDenoiseHSRV);
	Zenith_DebugVariables::AddTextureCallback({ "Flux", "SSR", "Textures", "DenoiseHConf" }, &DebugGetDenoiseHConfSRV);
	Zenith_DebugVariables::AddTextureCallback({ "Flux", "SSR", "Textures", "DenoiseV" },     &DebugGetDenoiseVSRV);

	// Denoise push-constant tuning knobs.
	Zenith_DebugVariables::AddFloat ({ "Flux", "SSR", "Denoise", "SpatialSigma" },   dbg_xSSRDenoiseConstants.m_fSpatialSigma,   0.5f, 6.0f);
	Zenith_DebugVariables::AddFloat ({ "Flux", "SSR", "Denoise", "DepthSigma" },     dbg_xSSRDenoiseConstants.m_fDepthSigma,     0.001f, 0.1f);
	Zenith_DebugVariables::AddFloat ({ "Flux", "SSR", "Denoise", "NormalSigma" },    dbg_xSSRDenoiseConstants.m_fNormalSigma,    0.05f, 1.0f);
	Zenith_DebugVariables::AddFloat ({ "Flux", "SSR", "Denoise", "RoughnessSigma" }, dbg_xSSRDenoiseConstants.m_fRoughnessSigma, 0.01f, 0.5f);
	Zenith_DebugVariables::AddUInt32({ "Flux", "SSR", "Denoise", "KernelRadius" },   dbg_xSSRDenoiseConstants.m_uKernelRadius,   1, 8);
#endif

	g_xEngine.SSR().m_bInitialised = true;
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_SSR initialised");
}

void Flux_SSRImpl::Shutdown()
{
	if (!g_xEngine.SSR().m_bInitialised)
		return;

	Flux_MemoryManager::DestroyDynamicConstantBuffer(g_xEngine.SSR().m_xSSRConstantsBuffer);

	g_xEngine.SSR().m_pxGraph = nullptr;
	g_xEngine.SSR().m_bInitialised = false;
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_SSR shut down");
}

static void UpdateSSRConstants()
{
	// Update constants from debug variables and HiZ system
	dbg_xSSRConstants.m_uDebugMode = dbg_uDebugMode;
	dbg_xSSRConstants.m_uHiZMipCount = g_xEngine.HiZ().GetMipCount();
	dbg_xSSRConstants.m_uFrameIndex = Flux::GetFrameCounter();

	// Resolution-based binary search iterations for sub-pixel hit precision.
	// Half-res ray origins (Phase B) make sub-1/16-pixel precision wasted —
	// the bilateral upsample reconstructs full-res positions anyway.
	// 1080p (≤1920): 4 iterations
	// 1440p / 4K (>1920): 5 iterations
	u_int uWidth = Flux_Swapchain::GetWidth();
	dbg_xSSRConstants.m_uBinarySearchIterations = 4 + (uWidth > 1920 ? 1 : 0);

	// Clamp start mip to valid range
	if (dbg_xSSRConstants.m_uStartMip >= dbg_xSSRConstants.m_uHiZMipCount)
		dbg_xSSRConstants.m_uStartMip = dbg_xSSRConstants.m_uHiZMipCount - 1;

	// Half-res ray-march output dimensions (matches the half-res transient
	// created in SetupRenderGraph; both derive from the swapchain so they
	// stay in sync).
	const u_int uHalfWidth  = std::max(1u, Flux_Swapchain::GetWidth()  / 2u);
	const u_int uHalfHeight = std::max(1u, Flux_Swapchain::GetHeight() / 2u);
	dbg_xSSRConstants.m_fHalfResWidth     = (float)uHalfWidth;
	dbg_xSSRConstants.m_fHalfResHeight    = (float)uHalfHeight;
	dbg_xSSRConstants.m_fRcpHalfResWidth  = 1.0f / (float)uHalfWidth;
	dbg_xSSRConstants.m_fRcpHalfResHeight = 1.0f / (float)uHalfHeight;

	// HiZ per-mip dimensions. HiZ mip 0 matches the swapchain (the depth
	// buffer); each subsequent mip is a 2x2 reduction. Pre-computing the
	// (size, rcp size) here means the ray-march shader doesn't need to
	// call GetDimensions in its inner loop (project convention: dims
	// flow through the CBV).
	const u_int uFullWidth  = Flux_Swapchain::GetWidth();
	const u_int uFullHeight = Flux_Swapchain::GetHeight();
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

static void ExecuteSSRRayMarch(Flux_CommandList* pxCommandList, void*)
{
	if (!g_xEngine.SSR().IsEnabled() || !g_xEngine.HiZ().IsEnabled())
		return;

	UpdateSSRConstants();

	// First SSR pass of the frame: refresh the CBV. The Resolve (and the
	// new Upsample) pass binds the same CBV without re-uploading.
	Flux_MemoryManager::UploadBufferData(
		g_xEngine.SSR().m_xSSRConstantsBuffer.GetBuffer().m_xVRAMHandle,
		&dbg_xSSRConstants, sizeof(SSRConstants));

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&g_xEngine.SSR().m_xRayMarchPipeline);

	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);

	xBinder.BindCBV(g_xEngine.SSR().m_xRayMarchShader, "FrameConstants", &g_xEngine.FluxGraphics().m_xFrameConstantsBuffer.GetCBV());
	xBinder.BindCBV(g_xEngine.SSR().m_xRayMarchShader, "SSRConstants",   &g_xEngine.SSR().m_xSSRConstantsBuffer.GetCBV());

	xBinder.BindSRV(g_xEngine.SSR().m_xRayMarchShader, "g_xDepthTex", g_xEngine.FluxGraphics().GetDepthStencilSRV());
	xBinder.BindSRV(g_xEngine.SSR().m_xRayMarchShader, "g_xNormalsTex", g_xEngine.FluxGraphics().GetGBufferSRV(MRT_INDEX_NORMALSAMBIENT));
	xBinder.BindSRV(g_xEngine.SSR().m_xRayMarchShader, "g_xMaterialTex", g_xEngine.FluxGraphics().GetGBufferSRV(MRT_INDEX_MATERIAL));
	xBinder.BindSRV(g_xEngine.SSR().m_xRayMarchShader, "g_xHiZTex", &g_xEngine.HiZ().GetHiZSRV());
	xBinder.BindSRV(g_xEngine.SSR().m_xRayMarchShader, "g_xDiffuseTex", g_xEngine.FluxGraphics().GetGBufferSRV(MRT_INDEX_DIFFUSE));
	xBinder.BindSRV(g_xEngine.SSR().m_xRayMarchShader, "g_xBlueNoiseTex", &g_xEngine.VolumeFog().GetBlueNoiseTexture()->m_xSRV);

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
}

static void ExecuteSSRUpsample(Flux_CommandList* pxCommandList, void*)
{
	if (!g_xEngine.SSR().IsEnabled() || !g_xEngine.HiZ().IsEnabled())
		return;

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&g_xEngine.SSR().m_xUpsamplePipeline);

	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);

	// Half-res dimensions live in the SSR CBV — the upsample shader reads
	// them from there rather than calling GetDimensions on g_xSSRTex.
	xBinder.BindCBV(g_xEngine.SSR().m_xUpsampleShader, "SSRConstants", &g_xEngine.SSR().m_xSSRConstantsBuffer.GetCBV());

	xBinder.BindSRV(g_xEngine.SSR().m_xUpsampleShader, "g_xSSRTex",      &g_xEngine.SSR().GetRayMarchAttachment().SRV());
	xBinder.BindSRV(g_xEngine.SSR().m_xUpsampleShader, "g_xDepthTex",    g_xEngine.FluxGraphics().GetDepthStencilSRV());
	xBinder.BindSRV(g_xEngine.SSR().m_xUpsampleShader, "g_xSSRAuxTex",   &g_xEngine.SSR().GetRayMarchAuxAttachment().SRV());
	// Phase 5 — KNN composite-similarity scoring needs full-res normals + material.
	xBinder.BindSRV(g_xEngine.SSR().m_xUpsampleShader, "g_xNormalsTex",  g_xEngine.FluxGraphics().GetGBufferSRV(MRT_INDEX_NORMALSAMBIENT));
	xBinder.BindSRV(g_xEngine.SSR().m_xUpsampleShader, "g_xMaterialTex", g_xEngine.FluxGraphics().GetGBufferSRV(MRT_INDEX_MATERIAL));

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
}

static void ExecuteSSRDenoiseH(Flux_CommandList* pxCommandList, void*)
{
	if (!g_xEngine.SSR().IsEnabled() || !g_xEngine.HiZ().IsEnabled() || !Zenith_GraphicsOptions::Get().m_bSSRRoughnessBlurEnabled)
		return;

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&g_xEngine.SSR().m_xDenoiseHPipeline);

	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);

	xBinder.BindCBV(g_xEngine.SSR().m_xDenoiseHShader, "FrameConstants", &g_xEngine.FluxGraphics().m_xFrameConstantsBuffer.GetCBV());
	dbg_xSSRDenoiseConstants.m_bEnabled = Zenith_GraphicsOptions::Get().m_bSSRRoughnessBlurEnabled ? 1u : 0u;
	xBinder.BindDrawConstants(g_xEngine.SSR().m_xDenoiseHShader, "PushConstants", &dbg_xSSRDenoiseConstants, sizeof(SSRDenoiseConstants));

	xBinder.BindSRV(g_xEngine.SSR().m_xDenoiseHShader, "g_xSSRUpsampledTex",    &g_xEngine.SSR().GetUpsampledAttachment().SRV());
	xBinder.BindSRV(g_xEngine.SSR().m_xDenoiseHShader, "g_xDepthTex",           g_xEngine.FluxGraphics().GetDepthStencilSRV());
	xBinder.BindSRV(g_xEngine.SSR().m_xDenoiseHShader, "g_xNormalsTex",         g_xEngine.FluxGraphics().GetGBufferSRV(MRT_INDEX_NORMALSAMBIENT));
	xBinder.BindSRV(g_xEngine.SSR().m_xDenoiseHShader, "g_xMaterialTex",        g_xEngine.FluxGraphics().GetGBufferSRV(MRT_INDEX_MATERIAL));
	xBinder.BindSRV(g_xEngine.SSR().m_xDenoiseHShader, "g_xSSRUpsampledAuxTex", &g_xEngine.SSR().GetUpsampledAuxAttachment().SRV());

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
}

static void ExecuteSSRDenoiseV(Flux_CommandList* pxCommandList, void*)
{
	if (!g_xEngine.SSR().IsEnabled() || !g_xEngine.HiZ().IsEnabled() || !Zenith_GraphicsOptions::Get().m_bSSRRoughnessBlurEnabled)
		return;

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&g_xEngine.SSR().m_xDenoiseVPipeline);

	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);

	xBinder.BindCBV(g_xEngine.SSR().m_xDenoiseVShader, "FrameConstants", &g_xEngine.FluxGraphics().m_xFrameConstantsBuffer.GetCBV());
	dbg_xSSRDenoiseConstants.m_bEnabled = Zenith_GraphicsOptions::Get().m_bSSRRoughnessBlurEnabled ? 1u : 0u;
	xBinder.BindDrawConstants(g_xEngine.SSR().m_xDenoiseVShader, "PushConstants", &dbg_xSSRDenoiseConstants, sizeof(SSRDenoiseConstants));

	xBinder.BindSRV(g_xEngine.SSR().m_xDenoiseVShader, "g_xSSRDenoiseHColTex",  &g_xEngine.SSR().GetDenoiseHAttachment().SRV());
	xBinder.BindSRV(g_xEngine.SSR().m_xDenoiseVShader, "g_xDepthTex",           g_xEngine.FluxGraphics().GetDepthStencilSRV());
	xBinder.BindSRV(g_xEngine.SSR().m_xDenoiseVShader, "g_xNormalsTex",         g_xEngine.FluxGraphics().GetGBufferSRV(MRT_INDEX_NORMALSAMBIENT));
	xBinder.BindSRV(g_xEngine.SSR().m_xDenoiseVShader, "g_xMaterialTex",        g_xEngine.FluxGraphics().GetGBufferSRV(MRT_INDEX_MATERIAL));
	xBinder.BindSRV(g_xEngine.SSR().m_xDenoiseVShader, "g_xSSRUpsampledAuxTex", &g_xEngine.SSR().GetUpsampledAuxAttachment().SRV());
	xBinder.BindSRV(g_xEngine.SSR().m_xDenoiseVShader, "g_xSSRDenoiseHConfTex", &g_xEngine.SSR().GetDenoiseHConfAttachment().SRV());
	xBinder.BindSRV(g_xEngine.SSR().m_xDenoiseVShader, "g_xSSRUpsampledTex",    &g_xEngine.SSR().GetUpsampledAttachment().SRV());

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
}

// Pass handles so ApplyBlurSelectionToGraph can toggle their enable bits when
// m_bSSRRoughnessBlurEnabled changes — both H and V toggle together.
// Last value seen by ApplyBlurSelectionToGraph — change triggers a graph rebuild.
// Handle committed at SetupRenderGraph exit. GetReflectionHandle asserts the
// live toggle still resolves to this handle — otherwise a toggle has happened
// without a corresponding Flux::RequestGraphRebuild(), which would leave the
// deferred pass's declared Read referencing the stale transient.

void Flux_SSRImpl::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	g_xEngine.SSR().m_pxGraph = &xGraph;

	// Three-pass SSR pipeline (mirrors SSGI):
	//   RayMarch (half-res) → Upsample (full-res) → Resolve (full-res, optional)
	// The half-res ray-march is the dominant performance win. The Upsample
	// pass is a depth-weighted bilateral 2x2 reconstruction to full-res; it
	// is mandatory so the deferred consumer always reads a full-res image.
	Flux_TransientTextureDesc xHalfDesc;
	xHalfDesc.m_uWidth       = std::max(1u, Flux_Swapchain::GetWidth()  / 2u);
	xHalfDesc.m_uHeight      = std::max(1u, Flux_Swapchain::GetHeight() / 2u);
	xHalfDesc.m_eFormat      = SSR_FORMAT;
	xHalfDesc.m_uMemoryFlags = (1u << MEMORY_FLAGS__SHADER_READ);

	Flux_TransientTextureDesc xFullDesc = xHalfDesc;
	xFullDesc.m_uWidth  = Flux_Swapchain::GetWidth();
	xFullDesc.m_uHeight = Flux_Swapchain::GetHeight();

	g_xEngine.SSR().m_xRayMarchHandle     = xGraph.CreateTransient(xHalfDesc);
	g_xEngine.SSR().m_xRayMarchAuxHandle  = xGraph.CreateTransient(xHalfDesc);
	g_xEngine.SSR().m_xUpsampledHandle    = xGraph.CreateTransient(xFullDesc);
	g_xEngine.SSR().m_xUpsampledAuxHandle = xGraph.CreateTransient(xFullDesc);
	g_xEngine.SSR().m_xDenoiseHHandle     = xGraph.CreateTransient(xFullDesc);
	g_xEngine.SSR().m_xDenoiseHConfHandle = xGraph.CreateTransient(xFullDesc);
	g_xEngine.SSR().m_xDenoiseVHandle     = xGraph.CreateTransient(xFullDesc);

	// RayMarch pass — first writer of its targets; clear so the initial
	// render-pass LoadOp is valid. Dual-MRT: order matters — RT0 = colour,
	// RT1 = aux metadata (must match the shader's SV_Target indices).
	xGraph.AddPass("SSR RayMarch", ExecuteSSRRayMarch)
		.ClearTargets()
		.Reads          (g_xEngine.FluxGraphics().GetDepthAttachment(),                       RESOURCE_ACCESS_READ_SRV)
		.Reads          (g_xEngine.HiZ().GetHiZAttachment(),                              RESOURCE_ACCESS_READ_SRV, 0, g_xEngine.HiZ().m_uMipCount)
		.Reads          (g_xEngine.FluxGraphics().GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT), RESOURCE_ACCESS_READ_SRV)
		.Reads          (g_xEngine.FluxGraphics().GetMRTAttachment(MRT_INDEX_MATERIAL),       RESOURCE_ACCESS_READ_SRV)
		.Reads          (g_xEngine.FluxGraphics().GetMRTAttachment(MRT_INDEX_DIFFUSE),        RESOURCE_ACCESS_READ_SRV)
		.WritesTransient(g_xEngine.SSR().m_xRayMarchHandle,                                         RESOURCE_ACCESS_WRITE_RTV)
		.WritesTransient(g_xEngine.SSR().m_xRayMarchAuxHandle,                                      RESOURCE_ACCESS_WRITE_RTV);

	// Upsample pass — depth-weighted bilateral 2x2 from half to full-res.
	// Always enabled; produces the canonical full-res SSR output. Same dual-MRT
	// shape as RayMarch — RT0 colour, RT1 aux — both reconstructed with the
	// same depth weights so they stay spatially aligned.
	xGraph.AddPass("SSR Upsample", ExecuteSSRUpsample)
		.ClearTargets()
		.Reads          (g_xEngine.FluxGraphics().GetDepthAttachment(),                       RESOURCE_ACCESS_READ_SRV)
		.Reads          (g_xEngine.FluxGraphics().GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT), RESOURCE_ACCESS_READ_SRV)
		.Reads          (g_xEngine.FluxGraphics().GetMRTAttachment(MRT_INDEX_MATERIAL),       RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient (g_xEngine.SSR().m_xRayMarchHandle,                                         RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient (g_xEngine.SSR().m_xRayMarchAuxHandle,                                      RESOURCE_ACCESS_READ_SRV)
		.WritesTransient(g_xEngine.SSR().m_xUpsampledHandle,                                        RESOURCE_ACCESS_WRITE_RTV)
		.WritesTransient(g_xEngine.SSR().m_xUpsampledAuxHandle,                                     RESOURCE_ACCESS_WRITE_RTV);

	// DenoiseH pass — separable bilateral with BRDF reuse, horizontal half.
	// Dual-MRT output (Phase 3b): RT0 carries the (Σw·color, Σw) accumulator
	// pair, RT1 carries the (Σw·conf) parallel accumulator. V applies the
	// final ratio. ApplyBlurSelectionToGraph toggles its enable bit based on
	// m_bSSRRoughnessBlurEnabled. Roughness gating inside the shader skips
	// smooth/rough pixels.
	g_xEngine.SSR().m_xDenoiseHPass = xGraph.AddPass("SSR DenoiseH", ExecuteSSRDenoiseH)
		.ClearTargets()
		.Reads          (g_xEngine.FluxGraphics().GetDepthAttachment(),                       RESOURCE_ACCESS_READ_SRV)
		.Reads          (g_xEngine.FluxGraphics().GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT), RESOURCE_ACCESS_READ_SRV)
		.Reads          (g_xEngine.FluxGraphics().GetMRTAttachment(MRT_INDEX_MATERIAL),       RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient (g_xEngine.SSR().m_xUpsampledHandle,                                        RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient (g_xEngine.SSR().m_xUpsampledAuxHandle,                                     RESOURCE_ACCESS_READ_SRV)
		.WritesTransient(g_xEngine.SSR().m_xDenoiseHHandle,                                         RESOURCE_ACCESS_WRITE_RTV)
		.WritesTransient(g_xEngine.SSR().m_xDenoiseHConfHandle,                                     RESOURCE_ACCESS_WRITE_RTV);

	// DenoiseV pass — vertical half, reads H's dual-MRT accumulators + the
	// upsampled colour (passthrough fallback) + aux (BRDF reuse), applies its
	// own bilateral × BRDF kernel, divides numerator/denominator at the end,
	// and outputs the final RGBA the deferred shader consumes.
	g_xEngine.SSR().m_xDenoiseVPass = xGraph.AddPass("SSR DenoiseV", ExecuteSSRDenoiseV)
		.ClearTargets()
		.Reads          (g_xEngine.FluxGraphics().GetDepthAttachment(),                       RESOURCE_ACCESS_READ_SRV)
		.Reads          (g_xEngine.FluxGraphics().GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT), RESOURCE_ACCESS_READ_SRV)
		.Reads          (g_xEngine.FluxGraphics().GetMRTAttachment(MRT_INDEX_MATERIAL),       RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient (g_xEngine.SSR().m_xUpsampledHandle,                                        RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient (g_xEngine.SSR().m_xUpsampledAuxHandle,                                     RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient (g_xEngine.SSR().m_xDenoiseHHandle,                                         RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient (g_xEngine.SSR().m_xDenoiseHConfHandle,                                     RESOURCE_ACCESS_READ_SRV)
		.WritesTransient(g_xEngine.SSR().m_xDenoiseVHandle,                                         RESOURCE_ACCESS_WRITE_RTV);

	const bool bRoughnessBlur = Zenith_GraphicsOptions::Get().m_bSSRRoughnessBlurEnabled;
	xGraph.SetEnabled(g_xEngine.SSR().m_xDenoiseHPass, bRoughnessBlur);
	xGraph.SetEnabled(g_xEngine.SSR().m_xDenoiseVPass, bRoughnessBlur);
	g_xEngine.SSR().m_bLastBlurEnabled = bRoughnessBlur;

	// Commit the handle the deferred pass will now read. GetReflectionHandle
	// asserts against this value on every call — any runtime toggle without a
	// matching Flux::RequestGraphRebuild() will trip at the point of the
	// mistake, not downstream in Validate() or AssertBoundResourceDeclared.
	// When denoise is off, deferred reads the upsampled (full-res) output —
	// never the raw half-res raymarch.
	g_xEngine.SSR().m_xCommittedReflectionHandle = bRoughnessBlur ? g_xEngine.SSR().m_xDenoiseVHandle : g_xEngine.SSR().m_xUpsampledHandle;
}

void Flux_SSRImpl::ApplyBlurSelectionToGraph(Flux_RenderGraph& /*xGraph*/)
{
	if (Zenith_GraphicsOptions::Get().m_bSSRRoughnessBlurEnabled == g_xEngine.SSR().m_bLastBlurEnabled)
		return;

	// Full graph rebuild — not just xGraph.MarkDirty() — because
	// Flux_DeferredShading::SetupRenderGraph captures the current
	// GetReflectionHandle() value when declaring its Read. MarkDirty triggers a
	// re-Compile on the same declared reads/writes, which would leave the
	// deferred pass reading the stale handle (either an orphan Read whose
	// writer is now disabled, or a valid Read on the wrong transient, with the
	// execute callback binding an SRV the graph hasn't transitioned).
	// Flux::RequestGraphRebuild() re-runs every subsystem's SetupRenderGraph
	// on the next frame, so the deferred pass's declared Read resolves to the
	// handle matching the new m_bSSRRoughnessBlurEnabled value. SetupRenderGraph
	// above will also re-seed g_xEngine.SSR().m_xCommittedReflectionHandle and re-set the
	// resolve pass's enable bit.
	Flux::RequestGraphRebuild();
}


Flux_ShaderResourceView& Flux_SSRImpl::GetReflectionSRV()
{
	// Must match GetReflectionHandle so bind-time-declared-access assertions
	// see the same resource the graph has a Read declared on.
	if (Zenith_GraphicsOptions::Get().m_bSSRRoughnessBlurEnabled)
		return GetDenoiseVAttachment().SRV();
	return GetUpsampledAttachment().SRV();
}

bool Flux_SSRImpl::IsEnabled() const
{
	return Zenith_GraphicsOptions::Get().m_bSSREnabled && g_xEngine.SSR().m_bInitialised;
}

