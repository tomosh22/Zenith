#include "Zenith.h"
#include "Flux/Flux_RendererImpl.h"
#include "Core/Zenith_Engine.h"

#include "Flux/SSGI/Flux_SSGIImpl.h"
#include "Flux/SSGI/Flux_SSGIImpl.h"
#include "Flux/HiZ/Flux_HiZImpl.h"
#include "Flux/HiZ/Flux_HiZImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/HDR/Flux_HDRImpl.h"
#include "Flux/Fog/Flux_VolumeFogImpl.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Flux/Flux_BackendTypes.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#ifdef ZENITH_TOOLS
#include "Flux/Slang/Flux_ShaderHotReload.h"
#endif

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

Flux_RenderAttachment& Flux_SSGIImpl::GetRawResultAttachment()
{
	Zenith_Assert(m_pxGraph, "GetRawResultAttachment: graph pointer is null (called before SetupRenderGraph or after Shutdown)");
	return m_pxGraph->GetTransientAttachment(m_xRawResultHandle);
}
Flux_RenderAttachment& Flux_SSGIImpl::GetResolvedAttachment()
{
	Zenith_Assert(m_pxGraph, "GetResolvedAttachment: graph pointer is null (called before SetupRenderGraph or after Shutdown)");
	return m_pxGraph->GetTransientAttachment(m_xResolvedHandle);
}
Flux_RenderAttachment& Flux_SSGIImpl::GetDenoisedAttachment()
{
	Zenith_Assert(m_pxGraph, "GetDenoisedAttachment: graph pointer is null (called before SetupRenderGraph or after Shutdown)");
	return m_pxGraph->GetTransientAttachment(m_xDenoisedHandle);
}

#ifdef ZENITH_DEBUG_VARIABLES
// Debug-texture callbacks — resolved every ImGui draw so the preview follows
// render-graph rebuilds. Returning nullptr before SetupRenderGraph avoids
// tripping the null-guard asserts in the attachment getters above.
static const Flux_ShaderResourceView* DebugGetRawSRV()
{
	auto& xZZ = g_xEngine.SSGI();
	if (xZZ.m_pxGraph == nullptr) return nullptr;
	return &xZZ.GetRawResultAttachment().SRV();
}
static const Flux_ShaderResourceView* DebugGetResolvedSRV()
{
	auto& xZZ = g_xEngine.SSGI();
	if (xZZ.m_pxGraph == nullptr) return nullptr;
	return &xZZ.GetResolvedAttachment().SRV();
}
static const Flux_ShaderResourceView* DebugGetDenoisedSRV()
{
	auto& xZZ = g_xEngine.SSGI();
	if (xZZ.m_pxGraph == nullptr) return nullptr;
	return &xZZ.GetDenoisedAttachment().SRV();
}
#endif

void Flux_SSGIImpl::BuildPipelines()
{
	Flux_PipelineHelper::BuildFullscreenPipeline(
		m_xRayMarchShader, m_xRayMarchPipeline,
		FluxShaderProgram::SSGI_RayMarch, SSGI_FORMAT);

	Flux_PipelineHelper::BuildFullscreenPipeline(
		m_xUpsampleShader, m_xUpsamplePipeline,
		FluxShaderProgram::SSGI_Upsample, SSGI_FORMAT);

	Flux_PipelineHelper::BuildFullscreenPipeline(
		m_xDenoiseHShader, m_xDenoiseHPipeline,
		FluxShaderProgram::SSGI_DenoiseH, SSGI_FORMAT);

	Flux_PipelineHelper::BuildFullscreenPipeline(
		m_xDenoiseVShader, m_xDenoiseVPipeline,
		FluxShaderProgram::SSGI_DenoiseV, SSGI_FORMAT);
}

void Flux_SSGIImpl::Initialise(Flux_Swapchain& xSwapchain, Flux_HiZImpl& xHiZ, Flux_GraphicsImpl& xGraphics, Flux_VolumeFogImpl& xVolumeFog, Flux_RendererImpl& xRenderer)
{
	m_pxSwapchain = &xSwapchain;
	m_pxHiZ       = &xHiZ;
	m_pxGraphics  = &xGraphics;
	m_pxVolumeFog = &xVolumeFog;
	m_pxRenderer  = &xRenderer;

	BuildPipelines();

#ifdef ZENITH_TOOLS
	static const FluxShaderProgram s_axPrograms[] = {
		FluxShaderProgram::SSGI_RayMarch,
		FluxShaderProgram::SSGI_Upsample,
		FluxShaderProgram::SSGI_DenoiseH,
		FluxShaderProgram::SSGI_DenoiseV,
	};
	Flux_ShaderHotReload::RegisterSubsystem([](){ g_xEngine.SSGI().BuildPipelines(); },
		s_axPrograms, sizeof(s_axPrograms) / sizeof(s_axPrograms[0]));
#endif

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
	m_pxSwapchain = nullptr;
	m_pxHiZ       = nullptr;
	m_pxGraphics  = nullptr;
	m_pxVolumeFog = nullptr;
	m_pxRenderer  = nullptr;
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_SSGI shut down");
}

// Pulled out of UpdateSSGIConstants so the executes can build a per-frame
// snapshot without mutating the debug-var-bound storage (which the ImGui UI
// reads/writes every frame). The base value is the user-tuned 1080p target;
// the effective value is bumped for higher screen resolutions and clamped.
u_int Flux_SSGIImpl::ComputeEffectiveBinarySearchIterations() const
{
	const u_int uBase = dbg_xSSGIConstants.m_uBinarySearchIterations;
	const u_int uW    = m_pxSwapchain->GetWidth();
	const u_int uBumped = uBase + (uW > 1920u ? 1u : 0u) + (uW > 2560u ? 1u : 0u);
	// Ceiling at 6 — diffuse GI doesn't need finer than ~1/64 px sub-pixel hits.
	return uBumped > 6u ? 6u : uBumped;
}

void Flux_SSGIImpl::UpdateSSGIConstants()
{
	dbg_xSSGIConstants.m_uDebugMode = dbg_uDebugMode;
	dbg_xSSGIConstants.m_uHiZMipCount = m_pxHiZ->GetMipCount();
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

static void ExecuteSSGIRayMarch(Flux_CommandList* pxCommandList, void*)
{
	auto& xZZ = g_xEngine.SSGI();
	if (!xZZ.IsEnabled() || !xZZ.m_pxHiZ->IsEnabled())
		return;

	Zenith_Profiling::Scope xScope(ZENITH_PROFILE_INDEX__FLUX_SSGI);

	xZZ.UpdateSSGIConstants();

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&xZZ.m_xRayMarchPipeline);

	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&xZZ.m_pxGraphics->m_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&xZZ.m_pxGraphics->m_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);

	// Bind a frame-local snapshot so the resolution-bumped iteration count
	// goes to the GPU without mutating dbg_xSSGIConstants (which the debug-var
	// system reads back into ImGui every frame — a write here would surprise
	// the user with a different value than they typed).
	SSGIConstants xFrameConstants = dbg_xSSGIConstants;
	xFrameConstants.m_uBinarySearchIterations = xZZ.ComputeEffectiveBinarySearchIterations();

	xBinder.BindCBV(xZZ.m_xRayMarchShader, "FrameConstants", &xZZ.m_pxGraphics->m_xFrameConstantsBuffer.GetCBV());
	xBinder.BindDrawConstants(xZZ.m_xRayMarchShader, "SSGIConstants", &xFrameConstants, sizeof(SSGIConstants));

	xBinder.BindSRV(xZZ.m_xRayMarchShader, "g_xDepthTex", xZZ.m_pxGraphics->GetDepthStencilSRV());
	xBinder.BindSRV(xZZ.m_xRayMarchShader, "g_xNormalsTex", xZZ.m_pxGraphics->GetGBufferSRV(MRT_INDEX_NORMALSAMBIENT));
	xBinder.BindSRV(xZZ.m_xRayMarchShader, "g_xMaterialTex", xZZ.m_pxGraphics->GetGBufferSRV(MRT_INDEX_MATERIAL));
	xBinder.BindSRV(xZZ.m_xRayMarchShader, "g_xHiZTex", &xZZ.m_pxHiZ->GetHiZSRV());
	xBinder.BindSRV(xZZ.m_xRayMarchShader, "g_xDiffuseTex", xZZ.m_pxGraphics->GetGBufferSRV(MRT_INDEX_DIFFUSE));
	xBinder.BindSRV(xZZ.m_xRayMarchShader, "g_xBlueNoiseTex", &xZZ.m_pxVolumeFog->GetBlueNoiseTexture()->m_xSRV);

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
}

static void ExecuteSSGIUpsample(Flux_CommandList* pxCommandList, void*)
{
	auto& xZZ = g_xEngine.SSGI();
	if (!xZZ.IsEnabled() || !xZZ.m_pxHiZ->IsEnabled())
		return;

	Zenith_Profiling::Scope xScope(ZENITH_PROFILE_INDEX__FLUX_SSGI);

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&xZZ.m_xUpsamplePipeline);

	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&xZZ.m_pxGraphics->m_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&xZZ.m_pxGraphics->m_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);

	xBinder.BindSRV(xZZ.m_xUpsampleShader, "g_xSSGITex", &xZZ.GetRawResultAttachment().SRV());
	xBinder.BindSRV(xZZ.m_xUpsampleShader, "g_xDepthTex", xZZ.m_pxGraphics->GetDepthStencilSRV());

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
}

// Horizontal denoise sub-pass — reads the upsampled SSGI, writes the H
// intermediate. The same constants drive both H and V; the inner loop
// direction is the only difference between the two shaders.
static void ExecuteSSGIDenoiseH(Flux_CommandList* pxCommandList, void*)
{
	auto& xZZ = g_xEngine.SSGI();
	if (!xZZ.IsEnabled() || !xZZ.m_pxHiZ->IsEnabled())
		return;

	Zenith_Profiling::Scope xScope(ZENITH_PROFILE_INDEX__FLUX_SSGI);

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&xZZ.m_xDenoiseHPipeline);

	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&xZZ.m_pxGraphics->m_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&xZZ.m_pxGraphics->m_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);

	dbg_xSSGIDenoiseConstants.m_bEnabled = Zenith_GraphicsOptions::Get().m_bSSGIDenoiseEnabled ? 1u : 0u;
	xBinder.BindDrawConstants(xZZ.m_xDenoiseHShader, "PushConstants", &dbg_xSSGIDenoiseConstants, sizeof(SSGIDenoiseConstants));

	xBinder.BindSRV(xZZ.m_xDenoiseHShader, "g_xSSGITex",    &xZZ.GetResolvedAttachment().SRV());
	xBinder.BindSRV(xZZ.m_xDenoiseHShader, "g_xDepthTex",   xZZ.m_pxGraphics->GetDepthStencilSRV());
	xBinder.BindSRV(xZZ.m_xDenoiseHShader, "g_xNormalsTex", xZZ.m_pxGraphics->GetGBufferSRV(MRT_INDEX_NORMALSAMBIENT));
	xBinder.BindSRV(xZZ.m_xDenoiseHShader, "g_xAlbedoTex",  xZZ.m_pxGraphics->GetGBufferSRV(MRT_INDEX_DIFFUSE));

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
}

// Vertical denoise sub-pass — reads the H intermediate, writes the final
// denoised result that deferred shading consumes.
static void ExecuteSSGIDenoiseV(Flux_CommandList* pxCommandList, void*)
{
	auto& xZZ = g_xEngine.SSGI();
	if (!xZZ.IsEnabled() || !xZZ.m_pxHiZ->IsEnabled())
		return;

	Zenith_Profiling::Scope xScope(ZENITH_PROFILE_INDEX__FLUX_SSGI);

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&xZZ.m_xDenoiseVPipeline);

	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&xZZ.m_pxGraphics->m_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&xZZ.m_pxGraphics->m_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);

	dbg_xSSGIDenoiseConstants.m_bEnabled = Zenith_GraphicsOptions::Get().m_bSSGIDenoiseEnabled ? 1u : 0u;
	xBinder.BindDrawConstants(xZZ.m_xDenoiseVShader, "PushConstants", &dbg_xSSGIDenoiseConstants, sizeof(SSGIDenoiseConstants));

	xBinder.BindSRV(xZZ.m_xDenoiseVShader, "g_xSSGITex",    &xZZ.m_pxGraph->GetTransientAttachment(xZZ.m_xDenoiseHHandle).SRV());
	xBinder.BindSRV(xZZ.m_xDenoiseVShader, "g_xDepthTex",   xZZ.m_pxGraphics->GetDepthStencilSRV());
	xBinder.BindSRV(xZZ.m_xDenoiseVShader, "g_xNormalsTex", xZZ.m_pxGraphics->GetGBufferSRV(MRT_INDEX_NORMALSAMBIENT));
	xBinder.BindSRV(xZZ.m_xDenoiseVShader, "g_xAlbedoTex",  xZZ.m_pxGraphics->GetGBufferSRV(MRT_INDEX_DIFFUSE));

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
}

// Handles for the H/V denoise sub-passes so ApplyDenoiseSelectionToGraph can
// toggle them together when GraphicsOptions::m_bSSGIDenoiseEnabled changes.
// Last value seen by ApplyDenoiseSelectionToGraph — change triggers a graph rebuild.
// Handle committed at SetupRenderGraph exit. GetSSGIHandle asserts the live
// toggle still resolves to this handle — any runtime toggle without a matching
// g_xEngine.FluxRenderer().RequestGraphRebuild() trips at the point of the mistake.

void Flux_SSGIImpl::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	m_pxGraph = &xGraph;

	const u_int uFullWidth  = m_pxSwapchain->GetWidth();
	const u_int uFullHeight = m_pxSwapchain->GetHeight();
	// Raymarch target resolution = full / m_uRayMarchResolutionDivisor. Track the
	// value used so ApplyDenoiseSelectionToGraph can detect runtime changes.
	const u_int uDivisor    = m_uRayMarchResolutionDivisor < 2u ? 2u : m_uRayMarchResolutionDivisor;
	m_uLastResolutionDivisor = uDivisor;
	const u_int uRayWidth   = uFullWidth  / uDivisor;
	const u_int uRayHeight  = uFullHeight / uDivisor;

	// Raw result is at the divisor's resolution; resolved / denoised are full-res.
	Flux_TransientTextureDesc xRayDesc;
	xRayDesc.m_uWidth       = uRayWidth;
	xRayDesc.m_uHeight      = uRayHeight;
	xRayDesc.m_eFormat      = SSGI_FORMAT;
	xRayDesc.m_uMemoryFlags = (1u << MEMORY_FLAGS__SHADER_READ);
	m_xRawResultHandle = xGraph.CreateTransient(xRayDesc);

	Flux_TransientTextureDesc xFull = xRayDesc;
	xFull.m_uWidth  = uFullWidth;
	xFull.m_uHeight = uFullHeight;
	m_xResolvedHandle = xGraph.CreateTransient(xFull);
	// Intermediate between the H and V denoise sub-passes — same dimensions
	// and format as the resolved/denoised targets.
	m_xDenoiseHHandle = xGraph.CreateTransient(xFull);
	m_xDenoisedHandle = xGraph.CreateTransient(xFull);

	// RayMarch pass (half-res) — reads G-Buffer + HiZ, writes raw result.
	xGraph.AddPass("SSGI RayMarch", ExecuteSSGIRayMarch)
		.ClearTargets()
		.Reads          (m_pxGraphics->GetDepthAttachment(),                       RESOURCE_ACCESS_READ_SRV)
		.Reads          (m_pxHiZ->GetHiZAttachment(),                              RESOURCE_ACCESS_READ_SRV, 0, m_pxHiZ->m_uMipCount)
		.Reads          (m_pxGraphics->GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT), RESOURCE_ACCESS_READ_SRV)
		.Reads          (m_pxGraphics->GetMRTAttachment(MRT_INDEX_MATERIAL),       RESOURCE_ACCESS_READ_SRV)
		.Reads          (m_pxGraphics->GetMRTAttachment(MRT_INDEX_DIFFUSE),        RESOURCE_ACCESS_READ_SRV)
		.WritesTransient(m_xRawResultHandle,                                        RESOURCE_ACCESS_WRITE_RTV);

	// Upsample pass — half→full res bilateral upsample.
	xGraph.AddPass("SSGI Upsample", ExecuteSSGIUpsample)
		.ClearTargets()
		.Reads          (m_pxGraphics->GetDepthAttachment(), RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient (m_xRawResultHandle,                  RESOURCE_ACCESS_READ_SRV)
		.WritesTransient(m_xResolvedHandle,                   RESOURCE_ACCESS_WRITE_RTV);

	// Separable joint-bilateral denoise — split into horizontal then vertical
	// sub-passes. Registered unconditionally; ApplyDenoiseSelectionToGraph
	// toggles both enable bits together when m_bSSGIDenoiseEnabled changes.
	m_xDenoisePassH = xGraph.AddPass("SSGI Denoise H", ExecuteSSGIDenoiseH)
		.ClearTargets()
		.Reads          (m_pxGraphics->GetDepthAttachment(),                       RESOURCE_ACCESS_READ_SRV)
		.Reads          (m_pxGraphics->GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT), RESOURCE_ACCESS_READ_SRV)
		.Reads          (m_pxGraphics->GetMRTAttachment(MRT_INDEX_DIFFUSE),        RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient (m_xResolvedHandle,                                         RESOURCE_ACCESS_READ_SRV)
		.WritesTransient(m_xDenoiseHHandle,                                         RESOURCE_ACCESS_WRITE_RTV);

	m_xDenoisePassV = xGraph.AddPass("SSGI Denoise V", ExecuteSSGIDenoiseV)
		.ClearTargets()
		.Reads          (m_pxGraphics->GetDepthAttachment(),                       RESOURCE_ACCESS_READ_SRV)
		.Reads          (m_pxGraphics->GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT), RESOURCE_ACCESS_READ_SRV)
		.Reads          (m_pxGraphics->GetMRTAttachment(MRT_INDEX_DIFFUSE),        RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient (m_xDenoiseHHandle,                                         RESOURCE_ACCESS_READ_SRV)
		.WritesTransient(m_xDenoisedHandle,                                         RESOURCE_ACCESS_WRITE_RTV);

	const bool bDenoise = Zenith_GraphicsOptions::Get().m_bSSGIDenoiseEnabled;
	xGraph.SetEnabled(m_xDenoisePassH, bDenoise);
	xGraph.SetEnabled(m_xDenoisePassV, bDenoise);

	// Commit the handle the deferred pass will now read. GetSSGIHandle resolves
	// to this value — any runtime toggle without a matching
	// g_xEngine.FluxRenderer().RequestGraphRebuild() trips at the point of the
	// mistake. The composite selection captures BOTH the denoise toggle and the
	// (clamped) resolution divisor, so a divisor change also forces a rebuild.
	m_xSSGISelector.Commit(m_xDenoisedHandle, m_xResolvedHandle, bDenoise, Flux_SSGISelection{ bDenoise, uDivisor });
}

void Flux_SSGIImpl::ApplyDenoiseSelectionToGraph(Flux_RenderGraph& /*xGraph*/)
{
	// The composite selection covers BOTH triggers in one comparison: the
	// denoise toggle and the raymarch resolution divisor (which resizes the
	// raymarch transient). This is where the composite TSelection earns its keep.
	const Flux_SSGISelection xSelection{ Zenith_GraphicsOptions::Get().m_bSSGIDenoiseEnabled, m_uRayMarchResolutionDivisor };
	if (!m_xSSGISelector.RequestRebuildIfSelectionChanged(xSelection))
		return;

	// Full graph rebuild — see Flux_SSR::ApplyBlurSelectionToGraph for the same
	// rationale. MarkDirty() alone would leave Flux_DeferredShading reading the
	// stale transient handle captured at the previous SetupRenderGraph; same
	// applies if the raymarch transient's dimensions change.
	m_pxRenderer->RequestGraphRebuild();
}


Flux_ShaderResourceView& Flux_SSGIImpl::GetSSGISRV()
{
	// Resolve from the committed handle — the value GetSSGIHandle() returned
	// when Flux_DeferredShading declared its Read — NOT the live graphics
	// option. Between a runtime toggle and the requested rebuild landing
	// (next frame), the live option diverges from the graph for one frame;
	// resolving from the option trips AssertBoundResourceDeclared.
	Zenith_Assert(m_pxGraph, "GetSSGISRV: graph pointer is null (called before SetupRenderGraph or after Shutdown)");
	return m_pxGraph->GetTransientAttachment(GetSSGIHandle()).SRV();
}

bool Flux_SSGIImpl::IsEnabled() const
{
	return Zenith_GraphicsOptions::Get().m_bSSGIEnabled && m_bInitialised;
}

