#include "Zenith.h"

#include "Flux/SSR/Flux_SSR.h"
#include "Flux/HiZ/Flux_HiZ.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/HDR/Flux_HDR.h"
#include "Flux/Fog/Flux_VolumeFog.h"
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
//   s_xRayMarchHandle     — half-res RT0, RGB=colour, A=confidence
//   s_xRayMarchAuxHandle  — half-res RT1, RG=hitUV, B=travelDist, A=rayCount
//   s_xUpsampledHandle    — full-res RT0, bilateral 2x2 upsample of RT0
//   s_xUpsampledAuxHandle — full-res RT1, bilateral 2x2 upsample of RT1
//   s_xDenoiseHHandle     — full-res, H pass RT0: rgb=Σ(w·color), a=Σ(w)
//   s_xDenoiseHConfHandle — full-res, H pass RT1: r=Σ(w·conf) — Phase 3b accumulator
//   s_xDenoiseVHandle     — full-res, written by DenoiseV (final denoised output)
//
// The Aux transients carry per-pixel hit metadata used by Phase 3b denoise
// (BRDF reuse needs neighbour hit UV) and Phase 4 variance estimation
// (radial component). Phase 0 wires them end-to-end with no consumer beyond
// debug-texture preview; later phases plug them in.
static Flux_TransientHandle s_xRayMarchHandle;
static Flux_TransientHandle s_xRayMarchAuxHandle;
static Flux_TransientHandle s_xUpsampledHandle;
static Flux_TransientHandle s_xUpsampledAuxHandle;
static Flux_TransientHandle s_xDenoiseHHandle;
static Flux_TransientHandle s_xDenoiseHConfHandle;
static Flux_TransientHandle s_xDenoiseVHandle;
static Flux_RenderGraph* s_pxGraph = nullptr;

// SSR render target format.
static constexpr TextureFormat SSR_FORMAT = TEXTURE_FORMAT_R16G16B16A16_SFLOAT;

// Static member definitions
bool Flux_SSR::s_bInitialised = false;

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
	// Array size matches Flux_HiZ::uHIZ_MAX_MIPS (12).
	float m_axHiZMipSizes[12][4] = {};
} dbg_xSSRConstants;

// Shaders and pipelines
static Flux_Shader s_xRayMarchShader;
static Flux_Shader s_xUpsampleShader;
static Flux_Shader s_xDenoiseHShader;
static Flux_Shader s_xDenoiseVShader;
static Flux_Pipeline s_xRayMarchPipeline;
static Flux_Pipeline s_xUpsamplePipeline;
static Flux_Pipeline s_xDenoiseHPipeline;
static Flux_Pipeline s_xDenoiseVPipeline;

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
static Flux_DynamicConstantBuffer s_xSSRConstantsBuffer;

// ---- Helpers to get the right attachment regardless of path ----

Flux_RenderAttachment& Flux_SSR::GetRayMarchAttachment()
{
	Zenith_Assert(s_pxGraph, "Flux_SSR::GetRayMarchAttachment: graph pointer is null (called before SetupRenderGraph or after Shutdown)");
	return s_pxGraph->GetTransientAttachment(s_xRayMarchHandle);
}
Flux_RenderAttachment& Flux_SSR::GetRayMarchAuxAttachment()
{
	Zenith_Assert(s_pxGraph, "Flux_SSR::GetRayMarchAuxAttachment: graph pointer is null (called before SetupRenderGraph or after Shutdown)");
	return s_pxGraph->GetTransientAttachment(s_xRayMarchAuxHandle);
}
Flux_RenderAttachment& Flux_SSR::GetUpsampledAttachment()
{
	Zenith_Assert(s_pxGraph, "Flux_SSR::GetUpsampledAttachment: graph pointer is null (called before SetupRenderGraph or after Shutdown)");
	return s_pxGraph->GetTransientAttachment(s_xUpsampledHandle);
}
Flux_RenderAttachment& Flux_SSR::GetUpsampledAuxAttachment()
{
	Zenith_Assert(s_pxGraph, "Flux_SSR::GetUpsampledAuxAttachment: graph pointer is null (called before SetupRenderGraph or after Shutdown)");
	return s_pxGraph->GetTransientAttachment(s_xUpsampledAuxHandle);
}
Flux_RenderAttachment& Flux_SSR::GetDenoiseHAttachment()
{
	Zenith_Assert(s_pxGraph, "Flux_SSR::GetDenoiseHAttachment: graph pointer is null (called before SetupRenderGraph or after Shutdown)");
	return s_pxGraph->GetTransientAttachment(s_xDenoiseHHandle);
}
Flux_RenderAttachment& Flux_SSR::GetDenoiseHConfAttachment()
{
	Zenith_Assert(s_pxGraph, "Flux_SSR::GetDenoiseHConfAttachment: graph pointer is null (called before SetupRenderGraph or after Shutdown)");
	return s_pxGraph->GetTransientAttachment(s_xDenoiseHConfHandle);
}
Flux_RenderAttachment& Flux_SSR::GetDenoiseVAttachment()
{
	Zenith_Assert(s_pxGraph, "Flux_SSR::GetDenoiseVAttachment: graph pointer is null (called before SetupRenderGraph or after Shutdown)");
	return s_pxGraph->GetTransientAttachment(s_xDenoiseVHandle);
}

#ifdef ZENITH_DEBUG_VARIABLES
// Debug-texture callbacks — resolved every ImGui draw so the preview follows
// render-graph rebuilds (the transient SRVs are recreated on resize). Returning
// nullptr before SetupRenderGraph / after Shutdown renders nothing, avoiding
// the null-guard in the getters above.
static const Flux_ShaderResourceView* DebugGetRayMarchSRV()
{
	if (s_pxGraph == nullptr) return nullptr;
	return &Flux_SSR::GetRayMarchAttachment().SRV();
}
static const Flux_ShaderResourceView* DebugGetRayMarchAuxSRV()
{
	if (s_pxGraph == nullptr) return nullptr;
	return &Flux_SSR::GetRayMarchAuxAttachment().SRV();
}
static const Flux_ShaderResourceView* DebugGetUpsampledSRV()
{
	if (s_pxGraph == nullptr) return nullptr;
	return &Flux_SSR::GetUpsampledAttachment().SRV();
}
static const Flux_ShaderResourceView* DebugGetUpsampledAuxSRV()
{
	if (s_pxGraph == nullptr) return nullptr;
	return &Flux_SSR::GetUpsampledAuxAttachment().SRV();
}
static const Flux_ShaderResourceView* DebugGetDenoiseHSRV()
{
	if (s_pxGraph == nullptr) return nullptr;
	return &Flux_SSR::GetDenoiseHAttachment().SRV();
}
static const Flux_ShaderResourceView* DebugGetDenoiseHConfSRV()
{
	if (s_pxGraph == nullptr) return nullptr;
	return &Flux_SSR::GetDenoiseHConfAttachment().SRV();
}
static const Flux_ShaderResourceView* DebugGetDenoiseVSRV()
{
	if (s_pxGraph == nullptr) return nullptr;
	return &Flux_SSR::GetDenoiseVAttachment().SRV();
}
#endif

void Flux_SSR::BuildPipelines()
{
	// RayMarch + Upsample are dual-MRT: RT0 carries colour+confidence (legacy
	// shape), RT1 carries hit metadata (UV / travel distance / ray count) for
	// downstream Phase 3b denoise BRDF reuse and Phase 4 variance.
	{
		Flux_PipelineSpecification xSpec = Flux_PipelineHelper::CreateFullscreenSpec(
			s_xRayMarchShader, FluxShaderProgram::SSR_RayMarch, SSR_FORMAT);
		xSpec.m_aeColourAttachmentFormats[1] = SSR_FORMAT;
		xSpec.m_uNumColourAttachments        = 2;
		Flux_PipelineBuilder::FromSpecification(s_xRayMarchPipeline, xSpec);
	}

	{
		Flux_PipelineSpecification xSpec = Flux_PipelineHelper::CreateFullscreenSpec(
			s_xUpsampleShader, FluxShaderProgram::SSR_Upsample, SSR_FORMAT);
		xSpec.m_aeColourAttachmentFormats[1] = SSR_FORMAT;
		xSpec.m_uNumColourAttachments        = 2;
		Flux_PipelineBuilder::FromSpecification(s_xUpsamplePipeline, xSpec);
	}

	// DenoiseH is dual-MRT (Phase 3b): RT0 = Σ(w·color)+Σw, RT1 = Σ(w·conf).
	{
		Flux_PipelineSpecification xSpec = Flux_PipelineHelper::CreateFullscreenSpec(
			s_xDenoiseHShader, FluxShaderProgram::SSR_DenoiseH, SSR_FORMAT);
		xSpec.m_aeColourAttachmentFormats[1] = SSR_FORMAT;
		xSpec.m_uNumColourAttachments        = 2;
		Flux_PipelineBuilder::FromSpecification(s_xDenoiseHPipeline, xSpec);
	}

	Flux_PipelineHelper::BuildFullscreenPipeline(
		s_xDenoiseVShader, s_xDenoiseVPipeline,
		FluxShaderProgram::SSR_DenoiseV, SSR_FORMAT);
}

void Flux_SSR::Initialise()
{
	BuildPipelines();

	Flux_MemoryManager::InitialiseDynamicConstantBuffer(
		&dbg_xSSRConstants, sizeof(SSRConstants), s_xSSRConstantsBuffer);

#ifdef ZENITH_TOOLS
	static const FluxShaderProgram s_axPrograms[] = {
		FluxShaderProgram::SSR_RayMarch,
		FluxShaderProgram::SSR_Upsample,
		FluxShaderProgram::SSR_DenoiseH,
		FluxShaderProgram::SSR_DenoiseV,
	};
	Flux_ShaderHotReload::RegisterSubsystem(&Flux_SSR::BuildPipelines,
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
	// through s_pxGraph on every ImGui draw. Storing a stale pointer via
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

	s_bInitialised = true;
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_SSR initialised");
}

void Flux_SSR::Shutdown()
{
	if (!s_bInitialised)
		return;

	Flux_MemoryManager::DestroyDynamicConstantBuffer(s_xSSRConstantsBuffer);

	s_pxGraph = nullptr;
	s_bInitialised = false;
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_SSR shut down");
}

static void UpdateSSRConstants()
{
	// Update constants from debug variables and HiZ system
	dbg_xSSRConstants.m_uDebugMode = dbg_uDebugMode;
	dbg_xSSRConstants.m_uHiZMipCount = Flux_HiZ::GetMipCount();
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
	if (!Flux_SSR::IsEnabled() || !Flux_HiZ::IsEnabled())
		return;

	UpdateSSRConstants();

	// First SSR pass of the frame: refresh the CBV. The Resolve (and the
	// new Upsample) pass binds the same CBV without re-uploading.
	Flux_MemoryManager::UploadBufferData(
		s_xSSRConstantsBuffer.GetBuffer().m_xVRAMHandle,
		&dbg_xSSRConstants, sizeof(SSRConstants));

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&s_xRayMarchPipeline);

	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);

	xBinder.BindCBV(s_xRayMarchShader, "FrameConstants", &g_xEngine.FluxGraphics().m_xFrameConstantsBuffer.GetCBV());
	xBinder.BindCBV(s_xRayMarchShader, "SSRConstants",   &s_xSSRConstantsBuffer.GetCBV());

	xBinder.BindSRV(s_xRayMarchShader, "g_xDepthTex", Flux_Graphics::GetDepthStencilSRV());
	xBinder.BindSRV(s_xRayMarchShader, "g_xNormalsTex", Flux_Graphics::GetGBufferSRV(MRT_INDEX_NORMALSAMBIENT));
	xBinder.BindSRV(s_xRayMarchShader, "g_xMaterialTex", Flux_Graphics::GetGBufferSRV(MRT_INDEX_MATERIAL));
	xBinder.BindSRV(s_xRayMarchShader, "g_xHiZTex", &Flux_HiZ::GetHiZSRV());
	xBinder.BindSRV(s_xRayMarchShader, "g_xDiffuseTex", Flux_Graphics::GetGBufferSRV(MRT_INDEX_DIFFUSE));
	xBinder.BindSRV(s_xRayMarchShader, "g_xBlueNoiseTex", &Flux_VolumeFog::GetBlueNoiseTexture()->m_xSRV);

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
}

static void ExecuteSSRUpsample(Flux_CommandList* pxCommandList, void*)
{
	if (!Flux_SSR::IsEnabled() || !Flux_HiZ::IsEnabled())
		return;

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&s_xUpsamplePipeline);

	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);

	// Half-res dimensions live in the SSR CBV — the upsample shader reads
	// them from there rather than calling GetDimensions on g_xSSRTex.
	xBinder.BindCBV(s_xUpsampleShader, "SSRConstants", &s_xSSRConstantsBuffer.GetCBV());

	xBinder.BindSRV(s_xUpsampleShader, "g_xSSRTex",      &Flux_SSR::GetRayMarchAttachment().SRV());
	xBinder.BindSRV(s_xUpsampleShader, "g_xDepthTex",    Flux_Graphics::GetDepthStencilSRV());
	xBinder.BindSRV(s_xUpsampleShader, "g_xSSRAuxTex",   &Flux_SSR::GetRayMarchAuxAttachment().SRV());
	// Phase 5 — KNN composite-similarity scoring needs full-res normals + material.
	xBinder.BindSRV(s_xUpsampleShader, "g_xNormalsTex",  Flux_Graphics::GetGBufferSRV(MRT_INDEX_NORMALSAMBIENT));
	xBinder.BindSRV(s_xUpsampleShader, "g_xMaterialTex", Flux_Graphics::GetGBufferSRV(MRT_INDEX_MATERIAL));

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
}

static void ExecuteSSRDenoiseH(Flux_CommandList* pxCommandList, void*)
{
	if (!Flux_SSR::IsEnabled() || !Flux_HiZ::IsEnabled() || !Zenith_GraphicsOptions::Get().m_bSSRRoughnessBlurEnabled)
		return;

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&s_xDenoiseHPipeline);

	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);

	xBinder.BindCBV(s_xDenoiseHShader, "FrameConstants", &g_xEngine.FluxGraphics().m_xFrameConstantsBuffer.GetCBV());
	dbg_xSSRDenoiseConstants.m_bEnabled = Zenith_GraphicsOptions::Get().m_bSSRRoughnessBlurEnabled ? 1u : 0u;
	xBinder.BindDrawConstants(s_xDenoiseHShader, "PushConstants", &dbg_xSSRDenoiseConstants, sizeof(SSRDenoiseConstants));

	xBinder.BindSRV(s_xDenoiseHShader, "g_xSSRUpsampledTex",    &Flux_SSR::GetUpsampledAttachment().SRV());
	xBinder.BindSRV(s_xDenoiseHShader, "g_xDepthTex",           Flux_Graphics::GetDepthStencilSRV());
	xBinder.BindSRV(s_xDenoiseHShader, "g_xNormalsTex",         Flux_Graphics::GetGBufferSRV(MRT_INDEX_NORMALSAMBIENT));
	xBinder.BindSRV(s_xDenoiseHShader, "g_xMaterialTex",        Flux_Graphics::GetGBufferSRV(MRT_INDEX_MATERIAL));
	xBinder.BindSRV(s_xDenoiseHShader, "g_xSSRUpsampledAuxTex", &Flux_SSR::GetUpsampledAuxAttachment().SRV());

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
}

static void ExecuteSSRDenoiseV(Flux_CommandList* pxCommandList, void*)
{
	if (!Flux_SSR::IsEnabled() || !Flux_HiZ::IsEnabled() || !Zenith_GraphicsOptions::Get().m_bSSRRoughnessBlurEnabled)
		return;

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&s_xDenoiseVPipeline);

	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);

	xBinder.BindCBV(s_xDenoiseVShader, "FrameConstants", &g_xEngine.FluxGraphics().m_xFrameConstantsBuffer.GetCBV());
	dbg_xSSRDenoiseConstants.m_bEnabled = Zenith_GraphicsOptions::Get().m_bSSRRoughnessBlurEnabled ? 1u : 0u;
	xBinder.BindDrawConstants(s_xDenoiseVShader, "PushConstants", &dbg_xSSRDenoiseConstants, sizeof(SSRDenoiseConstants));

	xBinder.BindSRV(s_xDenoiseVShader, "g_xSSRDenoiseHColTex",  &Flux_SSR::GetDenoiseHAttachment().SRV());
	xBinder.BindSRV(s_xDenoiseVShader, "g_xDepthTex",           Flux_Graphics::GetDepthStencilSRV());
	xBinder.BindSRV(s_xDenoiseVShader, "g_xNormalsTex",         Flux_Graphics::GetGBufferSRV(MRT_INDEX_NORMALSAMBIENT));
	xBinder.BindSRV(s_xDenoiseVShader, "g_xMaterialTex",        Flux_Graphics::GetGBufferSRV(MRT_INDEX_MATERIAL));
	xBinder.BindSRV(s_xDenoiseVShader, "g_xSSRUpsampledAuxTex", &Flux_SSR::GetUpsampledAuxAttachment().SRV());
	xBinder.BindSRV(s_xDenoiseVShader, "g_xSSRDenoiseHConfTex", &Flux_SSR::GetDenoiseHConfAttachment().SRV());
	xBinder.BindSRV(s_xDenoiseVShader, "g_xSSRUpsampledTex",    &Flux_SSR::GetUpsampledAttachment().SRV());

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
}

// Pass handles so ApplyBlurSelectionToGraph can toggle their enable bits when
// m_bSSRRoughnessBlurEnabled changes — both H and V toggle together.
static Flux_PassHandle s_xDenoiseHPass;
static Flux_PassHandle s_xDenoiseVPass;
// Last value seen by ApplyBlurSelectionToGraph — change triggers a graph rebuild.
static bool s_bLastBlurEnabled = true;
// Handle committed at SetupRenderGraph exit. GetReflectionHandle asserts the
// live toggle still resolves to this handle — otherwise a toggle has happened
// without a corresponding Flux::RequestGraphRebuild(), which would leave the
// deferred pass's declared Read referencing the stale transient.
static Flux_TransientHandle s_xCommittedReflectionHandle;

void Flux_SSR::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	s_pxGraph = &xGraph;

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

	s_xRayMarchHandle     = xGraph.CreateTransient(xHalfDesc);
	s_xRayMarchAuxHandle  = xGraph.CreateTransient(xHalfDesc);
	s_xUpsampledHandle    = xGraph.CreateTransient(xFullDesc);
	s_xUpsampledAuxHandle = xGraph.CreateTransient(xFullDesc);
	s_xDenoiseHHandle     = xGraph.CreateTransient(xFullDesc);
	s_xDenoiseHConfHandle = xGraph.CreateTransient(xFullDesc);
	s_xDenoiseVHandle     = xGraph.CreateTransient(xFullDesc);

	// RayMarch pass — first writer of its targets; clear so the initial
	// render-pass LoadOp is valid. Dual-MRT: order matters — RT0 = colour,
	// RT1 = aux metadata (must match the shader's SV_Target indices).
	xGraph.AddPass("SSR RayMarch", ExecuteSSRRayMarch)
		.ClearTargets()
		.Reads          (Flux_Graphics::GetDepthAttachment(),                       RESOURCE_ACCESS_READ_SRV)
		.Reads          (Flux_HiZ::GetHiZAttachment(),                              RESOURCE_ACCESS_READ_SRV, 0, Flux_HiZ::s_uMipCount)
		.Reads          (Flux_Graphics::GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT), RESOURCE_ACCESS_READ_SRV)
		.Reads          (Flux_Graphics::GetMRTAttachment(MRT_INDEX_MATERIAL),       RESOURCE_ACCESS_READ_SRV)
		.Reads          (Flux_Graphics::GetMRTAttachment(MRT_INDEX_DIFFUSE),        RESOURCE_ACCESS_READ_SRV)
		.WritesTransient(s_xRayMarchHandle,                                         RESOURCE_ACCESS_WRITE_RTV)
		.WritesTransient(s_xRayMarchAuxHandle,                                      RESOURCE_ACCESS_WRITE_RTV);

	// Upsample pass — depth-weighted bilateral 2x2 from half to full-res.
	// Always enabled; produces the canonical full-res SSR output. Same dual-MRT
	// shape as RayMarch — RT0 colour, RT1 aux — both reconstructed with the
	// same depth weights so they stay spatially aligned.
	xGraph.AddPass("SSR Upsample", ExecuteSSRUpsample)
		.ClearTargets()
		.Reads          (Flux_Graphics::GetDepthAttachment(),                       RESOURCE_ACCESS_READ_SRV)
		.Reads          (Flux_Graphics::GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT), RESOURCE_ACCESS_READ_SRV)
		.Reads          (Flux_Graphics::GetMRTAttachment(MRT_INDEX_MATERIAL),       RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient (s_xRayMarchHandle,                                         RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient (s_xRayMarchAuxHandle,                                      RESOURCE_ACCESS_READ_SRV)
		.WritesTransient(s_xUpsampledHandle,                                        RESOURCE_ACCESS_WRITE_RTV)
		.WritesTransient(s_xUpsampledAuxHandle,                                     RESOURCE_ACCESS_WRITE_RTV);

	// DenoiseH pass — separable bilateral with BRDF reuse, horizontal half.
	// Dual-MRT output (Phase 3b): RT0 carries the (Σw·color, Σw) accumulator
	// pair, RT1 carries the (Σw·conf) parallel accumulator. V applies the
	// final ratio. ApplyBlurSelectionToGraph toggles its enable bit based on
	// m_bSSRRoughnessBlurEnabled. Roughness gating inside the shader skips
	// smooth/rough pixels.
	s_xDenoiseHPass = xGraph.AddPass("SSR DenoiseH", ExecuteSSRDenoiseH)
		.ClearTargets()
		.Reads          (Flux_Graphics::GetDepthAttachment(),                       RESOURCE_ACCESS_READ_SRV)
		.Reads          (Flux_Graphics::GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT), RESOURCE_ACCESS_READ_SRV)
		.Reads          (Flux_Graphics::GetMRTAttachment(MRT_INDEX_MATERIAL),       RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient (s_xUpsampledHandle,                                        RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient (s_xUpsampledAuxHandle,                                     RESOURCE_ACCESS_READ_SRV)
		.WritesTransient(s_xDenoiseHHandle,                                         RESOURCE_ACCESS_WRITE_RTV)
		.WritesTransient(s_xDenoiseHConfHandle,                                     RESOURCE_ACCESS_WRITE_RTV);

	// DenoiseV pass — vertical half, reads H's dual-MRT accumulators + the
	// upsampled colour (passthrough fallback) + aux (BRDF reuse), applies its
	// own bilateral × BRDF kernel, divides numerator/denominator at the end,
	// and outputs the final RGBA the deferred shader consumes.
	s_xDenoiseVPass = xGraph.AddPass("SSR DenoiseV", ExecuteSSRDenoiseV)
		.ClearTargets()
		.Reads          (Flux_Graphics::GetDepthAttachment(),                       RESOURCE_ACCESS_READ_SRV)
		.Reads          (Flux_Graphics::GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT), RESOURCE_ACCESS_READ_SRV)
		.Reads          (Flux_Graphics::GetMRTAttachment(MRT_INDEX_MATERIAL),       RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient (s_xUpsampledHandle,                                        RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient (s_xUpsampledAuxHandle,                                     RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient (s_xDenoiseHHandle,                                         RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient (s_xDenoiseHConfHandle,                                     RESOURCE_ACCESS_READ_SRV)
		.WritesTransient(s_xDenoiseVHandle,                                         RESOURCE_ACCESS_WRITE_RTV);

	const bool bRoughnessBlur = Zenith_GraphicsOptions::Get().m_bSSRRoughnessBlurEnabled;
	xGraph.SetEnabled(s_xDenoiseHPass, bRoughnessBlur);
	xGraph.SetEnabled(s_xDenoiseVPass, bRoughnessBlur);
	s_bLastBlurEnabled = bRoughnessBlur;

	// Commit the handle the deferred pass will now read. GetReflectionHandle
	// asserts against this value on every call — any runtime toggle without a
	// matching Flux::RequestGraphRebuild() will trip at the point of the
	// mistake, not downstream in Validate() or AssertBoundResourceDeclared.
	// When denoise is off, deferred reads the upsampled (full-res) output —
	// never the raw half-res raymarch.
	s_xCommittedReflectionHandle = bRoughnessBlur ? s_xDenoiseVHandle : s_xUpsampledHandle;
}

void Flux_SSR::ApplyBlurSelectionToGraph(Flux_RenderGraph& /*xGraph*/)
{
	if (Zenith_GraphicsOptions::Get().m_bSSRRoughnessBlurEnabled == s_bLastBlurEnabled)
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
	// above will also re-seed s_xCommittedReflectionHandle and re-set the
	// resolve pass's enable bit.
	Flux::RequestGraphRebuild();
}

Flux_TransientHandle Flux_SSR::GetReflectionHandle()
{
	// Denoise on → deferred reads the V-pass (final denoised) output.
	// Denoise off → deferred reads the upsampled (full-res, unblurred) output.
	// Never the raw half-res raymarch — that would feed half-res data into
	// the deferred shader at full-res and produce blocky reflections.
	const Flux_TransientHandle xLive = Zenith_GraphicsOptions::Get().m_bSSRRoughnessBlurEnabled ? s_xDenoiseVHandle : s_xUpsampledHandle;
	Zenith_Assert(!s_xCommittedReflectionHandle.IsValid() || xLive == s_xCommittedReflectionHandle,
		"Flux_SSR::GetReflectionHandle: live handle (idx=%u gen=%u) disagrees with the handle committed at SetupRenderGraph exit (idx=%u gen=%u). "
		"m_bSSRRoughnessBlurEnabled changed without Flux::RequestGraphRebuild() being called in ApplyBlurSelectionToGraph — "
		"MarkDirty() alone does not re-run SetupRenderGraph, so the deferred pass's declared Read is stale.",
		xLive.m_uIndex, xLive.m_uGeneration,
		s_xCommittedReflectionHandle.m_uIndex, s_xCommittedReflectionHandle.m_uGeneration);
	return xLive;
}

Flux_ShaderResourceView& Flux_SSR::GetReflectionSRV()
{
	// Must match GetReflectionHandle so bind-time-declared-access assertions
	// see the same resource the graph has a Read declared on.
	if (Zenith_GraphicsOptions::Get().m_bSSRRoughnessBlurEnabled)
		return GetDenoiseVAttachment().SRV();
	return GetUpsampledAttachment().SRV();
}

bool Flux_SSR::IsEnabled()
{
	return Zenith_GraphicsOptions::Get().m_bSSREnabled && s_bInitialised;
}

bool Flux_SSR::IsInitialised()
{
	return s_bInitialised;
}
