#include "Zenith.h"

#include "Flux/SSGI/Flux_SSGI.h"
#include "Flux/HiZ/Flux_HiZ.h"
#include "Flux/HiZ/Flux_HiZImpl.h"
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
static Flux_TransientHandle s_xRawResultHandle;
static Flux_TransientHandle s_xResolvedHandle;
// Intermediate target between the H and V denoise sub-passes.
static Flux_TransientHandle s_xDenoiseHHandle;
static Flux_TransientHandle s_xDenoisedHandle;
static Flux_RenderGraph* s_pxGraph = nullptr;

// SSGI render target format.
static constexpr TextureFormat SSGI_FORMAT = TEXTURE_FORMAT_R16G16B16A16_SFLOAT;

// Raymarch is rendered at (full / divisor) resolution, then bilateral-upsampled.
// Quarter-res (4) is the default — diffuse GI is low-frequency so the visual
// difference is small while the cost is 4× cheaper than half-res. The bilateral
// upsample's edge weighting handles the larger reconstruction footprint.
//   2 = half-res    (sharper, costlier — old default)
//   4 = quarter-res (default, big perf win)
//   8 = eighth-res  (very fast, visible blockiness)
static u_int s_uRayMarchResolutionDivisor = 4u;
// Tracks the value SetupRenderGraph used; if the debug var diverges, request a
// graph rebuild so the transient is recreated at the new dimensions.
static u_int s_uLastResolutionDivisor    = 4u;

// Static member definitions
bool Flux_SSGI::s_bInitialised = false;

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
static Flux_Shader s_xRayMarchShader;
static Flux_Shader s_xUpsampleShader;
// Denoise is split into two separable sub-passes (H then V) for a cheap
// approximation of the 2D joint bilateral. Both share the same CB layout.
static Flux_Shader s_xDenoiseHShader;
static Flux_Shader s_xDenoiseVShader;
static Flux_Pipeline s_xRayMarchPipeline;
static Flux_Pipeline s_xUpsamplePipeline;
static Flux_Pipeline s_xDenoiseHPipeline;
static Flux_Pipeline s_xDenoiseVPipeline;

Flux_RenderAttachment& Flux_SSGI::GetRawResultAttachment()
{
	Zenith_Assert(s_pxGraph, "Flux_SSGI::GetRawResultAttachment: graph pointer is null (called before SetupRenderGraph or after Shutdown)");
	return s_pxGraph->GetTransientAttachment(s_xRawResultHandle);
}
Flux_RenderAttachment& Flux_SSGI::GetResolvedAttachment()
{
	Zenith_Assert(s_pxGraph, "Flux_SSGI::GetResolvedAttachment: graph pointer is null (called before SetupRenderGraph or after Shutdown)");
	return s_pxGraph->GetTransientAttachment(s_xResolvedHandle);
}
Flux_RenderAttachment& Flux_SSGI::GetDenoisedAttachment()
{
	Zenith_Assert(s_pxGraph, "Flux_SSGI::GetDenoisedAttachment: graph pointer is null (called before SetupRenderGraph or after Shutdown)");
	return s_pxGraph->GetTransientAttachment(s_xDenoisedHandle);
}

#ifdef ZENITH_DEBUG_VARIABLES
// Debug-texture callbacks — resolved every ImGui draw so the preview follows
// render-graph rebuilds. Returning nullptr before SetupRenderGraph avoids
// tripping the null-guard asserts in the attachment getters above.
static const Flux_ShaderResourceView* DebugGetRawSRV()
{
	if (s_pxGraph == nullptr) return nullptr;
	return &Flux_SSGI::GetRawResultAttachment().SRV();
}
static const Flux_ShaderResourceView* DebugGetResolvedSRV()
{
	if (s_pxGraph == nullptr) return nullptr;
	return &Flux_SSGI::GetResolvedAttachment().SRV();
}
static const Flux_ShaderResourceView* DebugGetDenoisedSRV()
{
	if (s_pxGraph == nullptr) return nullptr;
	return &Flux_SSGI::GetDenoisedAttachment().SRV();
}
#endif

void Flux_SSGI::BuildPipelines()
{
	Flux_PipelineHelper::BuildFullscreenPipeline(
		s_xRayMarchShader, s_xRayMarchPipeline,
		FluxShaderProgram::SSGI_RayMarch, SSGI_FORMAT);

	Flux_PipelineHelper::BuildFullscreenPipeline(
		s_xUpsampleShader, s_xUpsamplePipeline,
		FluxShaderProgram::SSGI_Upsample, SSGI_FORMAT);

	Flux_PipelineHelper::BuildFullscreenPipeline(
		s_xDenoiseHShader, s_xDenoiseHPipeline,
		FluxShaderProgram::SSGI_DenoiseH, SSGI_FORMAT);

	Flux_PipelineHelper::BuildFullscreenPipeline(
		s_xDenoiseVShader, s_xDenoiseVPipeline,
		FluxShaderProgram::SSGI_DenoiseV, SSGI_FORMAT);
}

void Flux_SSGI::Initialise()
{
	BuildPipelines();

#ifdef ZENITH_TOOLS
	static const FluxShaderProgram s_axPrograms[] = {
		FluxShaderProgram::SSGI_RayMarch,
		FluxShaderProgram::SSGI_Upsample,
		FluxShaderProgram::SSGI_DenoiseH,
		FluxShaderProgram::SSGI_DenoiseV,
	};
	Flux_ShaderHotReload::RegisterSubsystem(&Flux_SSGI::BuildPipelines,
		s_axPrograms, sizeof(s_axPrograms) / sizeof(s_axPrograms[0]));
#endif

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddUInt32({ "Flux", "SSGI", "DebugMode" }, dbg_uDebugMode, 0, SSGI_DEBUG_COUNT - 1);
	Zenith_DebugVariables::AddFloat({ "Flux", "SSGI", "Intensity" }, dbg_xSSGIConstants.m_fIntensity, 0.0f, 2.0f);
	Zenith_DebugVariables::AddFloat({ "Flux", "SSGI", "MaxDistance" }, dbg_xSSGIConstants.m_fMaxDistance, 1.0f, 100.0f);
	Zenith_DebugVariables::AddFloat({ "Flux", "SSGI", "Thickness" }, dbg_xSSGIConstants.m_fThickness, 0.01f, 2.0f);
	Zenith_DebugVariables::AddUInt32({ "Flux", "SSGI", "StepCount" }, dbg_xSSGIConstants.m_uStepCount, 8, 128);
	Zenith_DebugVariables::AddUInt32({ "Flux", "SSGI", "StartMip" }, dbg_xSSGIConstants.m_uStartMip, 0, 10);
	Zenith_DebugVariables::AddUInt32({ "Flux", "SSGI", "RaysPerPixel" }, dbg_xSSGIConstants.m_uRaysPerPixel, 1, 8);
	Zenith_DebugVariables::AddFloat({ "Flux", "SSGI", "RoughnessThreshold" }, dbg_xSSGIConstants.m_fRoughnessThreshold, 0.0f, 1.0f);
	// Raymarch resolution divisor (full / divisor). Triggers a graph rebuild on change.
	// 2=half, 4=quarter (default), 8=eighth. Quality vs perf knob.
	Zenith_DebugVariables::AddUInt32({ "Flux", "SSGI", "ResolutionDivisor" }, s_uRayMarchResolutionDivisor, 2, 8);
	// Base 1080p iteration count — UpdateSSGIConstants bumps the bound value
	// at higher resolutions and clamps to 6.
	Zenith_DebugVariables::AddUInt32({ "Flux", "SSGI", "BinarySearchIterations" }, dbg_xSSGIConstants.m_uBinarySearchIterations, 1, 6);
	// Transient-SRV previews: AddTextureCallback re-resolves through s_pxGraph
	// each ImGui draw so the preview survives graph rebuilds (resize, toggle).
	Zenith_DebugVariables::AddTextureCallback({ "Flux", "SSGI", "Textures", "Raw" },      &DebugGetRawSRV);
	Zenith_DebugVariables::AddTextureCallback({ "Flux", "SSGI", "Textures", "Resolved" }, &DebugGetResolvedSRV);
	Zenith_DebugVariables::AddTextureCallback({ "Flux", "SSGI", "Textures", "Denoised" }, &DebugGetDenoisedSRV);

	// Denoise debug variables. KernelRadius is per-axis: total separable footprint is (2r+1) along H then V.
	Zenith_DebugVariables::AddUInt32({ "Flux", "SSGI", "Denoise", "KernelRadius" }, dbg_xSSGIDenoiseConstants.m_uKernelRadius, 1, 8);
	Zenith_DebugVariables::AddFloat({ "Flux", "SSGI", "Denoise", "SpatialSigma" }, dbg_xSSGIDenoiseConstants.m_fSpatialSigma, 0.5f, 4.0f);
	Zenith_DebugVariables::AddFloat({ "Flux", "SSGI", "Denoise", "DepthSigma" }, dbg_xSSGIDenoiseConstants.m_fDepthSigma, 0.01f, 0.1f);
	Zenith_DebugVariables::AddFloat({ "Flux", "SSGI", "Denoise", "NormalSigma" }, dbg_xSSGIDenoiseConstants.m_fNormalSigma, 0.1f, 1.0f);
	Zenith_DebugVariables::AddFloat({ "Flux", "SSGI", "Denoise", "AlbedoSigma" }, dbg_xSSGIDenoiseConstants.m_fAlbedoSigma, 0.05f, 0.5f);
#endif

	// No resize callback needed — the graph re-creates its transients with
	// updated dimensions when SetupRenderGraph re-runs on resize.

	s_bInitialised = true;
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_SSGI initialised");
}

void Flux_SSGI::Shutdown()
{
	if (!s_bInitialised)
		return;

	s_pxGraph = nullptr;
	s_bInitialised = false;
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_SSGI shut down");
}

// Pulled out of UpdateSSGIConstants so the executes can build a per-frame
// snapshot without mutating the debug-var-bound storage (which the ImGui UI
// reads/writes every frame). The base value is the user-tuned 1080p target;
// the effective value is bumped for higher screen resolutions and clamped.
static u_int ComputeEffectiveBinarySearchIterations()
{
	const u_int uBase = dbg_xSSGIConstants.m_uBinarySearchIterations;
	const u_int uW    = Flux_Swapchain::GetWidth();
	const u_int uBumped = uBase + (uW > 1920u ? 1u : 0u) + (uW > 2560u ? 1u : 0u);
	// Ceiling at 6 — diffuse GI doesn't need finer than ~1/64 px sub-pixel hits.
	return uBumped > 6u ? 6u : uBumped;
}

static void UpdateSSGIConstants()
{
	dbg_xSSGIConstants.m_uDebugMode = dbg_uDebugMode;
	dbg_xSSGIConstants.m_uHiZMipCount = Flux_HiZ::GetMipCount();
	dbg_xSSGIConstants.m_uFrameIndex = Flux::GetFrameCounter();

	// Clamp start mip to valid range
	if (dbg_xSSGIConstants.m_uStartMip >= dbg_xSSGIConstants.m_uHiZMipCount)
		dbg_xSSGIConstants.m_uStartMip = dbg_xSSGIConstants.m_uHiZMipCount - 1;
}

static void ExecuteSSGIRayMarch(Flux_CommandList* pxCommandList, void*)
{
	if (!Flux_SSGI::IsEnabled() || !Flux_HiZ::IsEnabled())
		return;

	Zenith_Profiling::Scope xScope(ZENITH_PROFILE_INDEX__FLUX_SSGI);

	UpdateSSGIConstants();

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&s_xRayMarchPipeline);

	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);

	// Bind a frame-local snapshot so the resolution-bumped iteration count
	// goes to the GPU without mutating dbg_xSSGIConstants (which the debug-var
	// system reads back into ImGui every frame — a write here would surprise
	// the user with a different value than they typed).
	SSGIConstants xFrameConstants = dbg_xSSGIConstants;
	xFrameConstants.m_uBinarySearchIterations = ComputeEffectiveBinarySearchIterations();

	xBinder.BindCBV(s_xRayMarchShader, "FrameConstants", &g_xEngine.FluxGraphics().m_xFrameConstantsBuffer.GetCBV());
	xBinder.BindDrawConstants(s_xRayMarchShader, "SSGIConstants", &xFrameConstants, sizeof(SSGIConstants));

	xBinder.BindSRV(s_xRayMarchShader, "g_xDepthTex", Flux_Graphics::GetDepthStencilSRV());
	xBinder.BindSRV(s_xRayMarchShader, "g_xNormalsTex", Flux_Graphics::GetGBufferSRV(MRT_INDEX_NORMALSAMBIENT));
	xBinder.BindSRV(s_xRayMarchShader, "g_xMaterialTex", Flux_Graphics::GetGBufferSRV(MRT_INDEX_MATERIAL));
	xBinder.BindSRV(s_xRayMarchShader, "g_xHiZTex", &Flux_HiZ::GetHiZSRV());
	xBinder.BindSRV(s_xRayMarchShader, "g_xDiffuseTex", Flux_Graphics::GetGBufferSRV(MRT_INDEX_DIFFUSE));
	xBinder.BindSRV(s_xRayMarchShader, "g_xBlueNoiseTex", &Flux_VolumeFog::GetBlueNoiseTexture()->m_xSRV);

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
}

static void ExecuteSSGIUpsample(Flux_CommandList* pxCommandList, void*)
{
	if (!Flux_SSGI::IsEnabled() || !Flux_HiZ::IsEnabled())
		return;

	Zenith_Profiling::Scope xScope(ZENITH_PROFILE_INDEX__FLUX_SSGI);

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&s_xUpsamplePipeline);

	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);

	xBinder.BindSRV(s_xUpsampleShader, "g_xSSGITex", &Flux_SSGI::GetRawResultAttachment().SRV());
	xBinder.BindSRV(s_xUpsampleShader, "g_xDepthTex", Flux_Graphics::GetDepthStencilSRV());

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
}

// Horizontal denoise sub-pass — reads the upsampled SSGI, writes the H
// intermediate. The same constants drive both H and V; the inner loop
// direction is the only difference between the two shaders.
static void ExecuteSSGIDenoiseH(Flux_CommandList* pxCommandList, void*)
{
	if (!Flux_SSGI::IsEnabled() || !Flux_HiZ::IsEnabled())
		return;

	Zenith_Profiling::Scope xScope(ZENITH_PROFILE_INDEX__FLUX_SSGI);

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&s_xDenoiseHPipeline);

	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);

	dbg_xSSGIDenoiseConstants.m_bEnabled = Zenith_GraphicsOptions::Get().m_bSSGIDenoiseEnabled ? 1u : 0u;
	xBinder.BindDrawConstants(s_xDenoiseHShader, "PushConstants", &dbg_xSSGIDenoiseConstants, sizeof(SSGIDenoiseConstants));

	xBinder.BindSRV(s_xDenoiseHShader, "g_xSSGITex",    &Flux_SSGI::GetResolvedAttachment().SRV());
	xBinder.BindSRV(s_xDenoiseHShader, "g_xDepthTex",   Flux_Graphics::GetDepthStencilSRV());
	xBinder.BindSRV(s_xDenoiseHShader, "g_xNormalsTex", Flux_Graphics::GetGBufferSRV(MRT_INDEX_NORMALSAMBIENT));
	xBinder.BindSRV(s_xDenoiseHShader, "g_xAlbedoTex",  Flux_Graphics::GetGBufferSRV(MRT_INDEX_DIFFUSE));

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
}

// Vertical denoise sub-pass — reads the H intermediate, writes the final
// denoised result that deferred shading consumes.
static void ExecuteSSGIDenoiseV(Flux_CommandList* pxCommandList, void*)
{
	if (!Flux_SSGI::IsEnabled() || !Flux_HiZ::IsEnabled())
		return;

	Zenith_Profiling::Scope xScope(ZENITH_PROFILE_INDEX__FLUX_SSGI);

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&s_xDenoiseVPipeline);

	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);

	dbg_xSSGIDenoiseConstants.m_bEnabled = Zenith_GraphicsOptions::Get().m_bSSGIDenoiseEnabled ? 1u : 0u;
	xBinder.BindDrawConstants(s_xDenoiseVShader, "PushConstants", &dbg_xSSGIDenoiseConstants, sizeof(SSGIDenoiseConstants));

	xBinder.BindSRV(s_xDenoiseVShader, "g_xSSGITex",    &s_pxGraph->GetTransientAttachment(s_xDenoiseHHandle).SRV());
	xBinder.BindSRV(s_xDenoiseVShader, "g_xDepthTex",   Flux_Graphics::GetDepthStencilSRV());
	xBinder.BindSRV(s_xDenoiseVShader, "g_xNormalsTex", Flux_Graphics::GetGBufferSRV(MRT_INDEX_NORMALSAMBIENT));
	xBinder.BindSRV(s_xDenoiseVShader, "g_xAlbedoTex",  Flux_Graphics::GetGBufferSRV(MRT_INDEX_DIFFUSE));

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
}

// Handles for the H/V denoise sub-passes so ApplyDenoiseSelectionToGraph can
// toggle them together when GraphicsOptions::m_bSSGIDenoiseEnabled changes.
static Flux_PassHandle s_xDenoisePassH;
static Flux_PassHandle s_xDenoisePassV;
// Last value seen by ApplyDenoiseSelectionToGraph — change triggers a graph rebuild.
static bool s_bLastDenoiseEnabled = true;
// Handle committed at SetupRenderGraph exit. GetSSGIHandle asserts the live
// toggle still resolves to this handle — any runtime toggle without a matching
// Flux::RequestGraphRebuild() trips at the point of the mistake.
static Flux_TransientHandle s_xCommittedSSGIHandle;

void Flux_SSGI::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	s_pxGraph = &xGraph;

	const u_int uFullWidth  = Flux_Swapchain::GetWidth();
	const u_int uFullHeight = Flux_Swapchain::GetHeight();
	// Raymarch target resolution = full / s_uRayMarchResolutionDivisor. Track the
	// value used so ApplyDenoiseSelectionToGraph can detect runtime changes.
	const u_int uDivisor    = s_uRayMarchResolutionDivisor < 2u ? 2u : s_uRayMarchResolutionDivisor;
	s_uLastResolutionDivisor = uDivisor;
	const u_int uRayWidth   = uFullWidth  / uDivisor;
	const u_int uRayHeight  = uFullHeight / uDivisor;

	// Raw result is at the divisor's resolution; resolved / denoised are full-res.
	Flux_TransientTextureDesc xRayDesc;
	xRayDesc.m_uWidth       = uRayWidth;
	xRayDesc.m_uHeight      = uRayHeight;
	xRayDesc.m_eFormat      = SSGI_FORMAT;
	xRayDesc.m_uMemoryFlags = (1u << MEMORY_FLAGS__SHADER_READ);
	s_xRawResultHandle = xGraph.CreateTransient(xRayDesc);

	Flux_TransientTextureDesc xFull = xRayDesc;
	xFull.m_uWidth  = uFullWidth;
	xFull.m_uHeight = uFullHeight;
	s_xResolvedHandle = xGraph.CreateTransient(xFull);
	// Intermediate between the H and V denoise sub-passes — same dimensions
	// and format as the resolved/denoised targets.
	s_xDenoiseHHandle = xGraph.CreateTransient(xFull);
	s_xDenoisedHandle = xGraph.CreateTransient(xFull);

	// RayMarch pass (half-res) — reads G-Buffer + HiZ, writes raw result.
	xGraph.AddPass("SSGI RayMarch", ExecuteSSGIRayMarch)
		.ClearTargets()
		.Reads          (Flux_Graphics::GetDepthAttachment(),                       RESOURCE_ACCESS_READ_SRV)
		.Reads          (Flux_HiZ::GetHiZAttachment(),                              RESOURCE_ACCESS_READ_SRV, 0, g_xEngine.HiZ().m_uMipCount)
		.Reads          (Flux_Graphics::GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT), RESOURCE_ACCESS_READ_SRV)
		.Reads          (Flux_Graphics::GetMRTAttachment(MRT_INDEX_MATERIAL),       RESOURCE_ACCESS_READ_SRV)
		.Reads          (Flux_Graphics::GetMRTAttachment(MRT_INDEX_DIFFUSE),        RESOURCE_ACCESS_READ_SRV)
		.WritesTransient(s_xRawResultHandle,                                        RESOURCE_ACCESS_WRITE_RTV);

	// Upsample pass — half→full res bilateral upsample.
	xGraph.AddPass("SSGI Upsample", ExecuteSSGIUpsample)
		.ClearTargets()
		.Reads          (Flux_Graphics::GetDepthAttachment(), RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient (s_xRawResultHandle,                  RESOURCE_ACCESS_READ_SRV)
		.WritesTransient(s_xResolvedHandle,                   RESOURCE_ACCESS_WRITE_RTV);

	// Separable joint-bilateral denoise — split into horizontal then vertical
	// sub-passes. Registered unconditionally; ApplyDenoiseSelectionToGraph
	// toggles both enable bits together when m_bSSGIDenoiseEnabled changes.
	s_xDenoisePassH = xGraph.AddPass("SSGI Denoise H", ExecuteSSGIDenoiseH)
		.ClearTargets()
		.Reads          (Flux_Graphics::GetDepthAttachment(),                       RESOURCE_ACCESS_READ_SRV)
		.Reads          (Flux_Graphics::GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT), RESOURCE_ACCESS_READ_SRV)
		.Reads          (Flux_Graphics::GetMRTAttachment(MRT_INDEX_DIFFUSE),        RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient (s_xResolvedHandle,                                         RESOURCE_ACCESS_READ_SRV)
		.WritesTransient(s_xDenoiseHHandle,                                         RESOURCE_ACCESS_WRITE_RTV);

	s_xDenoisePassV = xGraph.AddPass("SSGI Denoise V", ExecuteSSGIDenoiseV)
		.ClearTargets()
		.Reads          (Flux_Graphics::GetDepthAttachment(),                       RESOURCE_ACCESS_READ_SRV)
		.Reads          (Flux_Graphics::GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT), RESOURCE_ACCESS_READ_SRV)
		.Reads          (Flux_Graphics::GetMRTAttachment(MRT_INDEX_DIFFUSE),        RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient (s_xDenoiseHHandle,                                         RESOURCE_ACCESS_READ_SRV)
		.WritesTransient(s_xDenoisedHandle,                                         RESOURCE_ACCESS_WRITE_RTV);

	const bool bDenoise = Zenith_GraphicsOptions::Get().m_bSSGIDenoiseEnabled;
	xGraph.SetEnabled(s_xDenoisePassH, bDenoise);
	xGraph.SetEnabled(s_xDenoisePassV, bDenoise);
	s_bLastDenoiseEnabled = bDenoise;

	// Commit the handle the deferred pass will now read. GetSSGIHandle asserts
	// against this value — any runtime toggle without a matching
	// Flux::RequestGraphRebuild() trips at the point of the mistake.
	s_xCommittedSSGIHandle = bDenoise ? s_xDenoisedHandle : s_xResolvedHandle;
}

void Flux_SSGI::ApplyDenoiseSelectionToGraph(Flux_RenderGraph& /*xGraph*/)
{
	const bool bDenoiseChanged    = Zenith_GraphicsOptions::Get().m_bSSGIDenoiseEnabled != s_bLastDenoiseEnabled;
	const bool bResolutionChanged = s_uRayMarchResolutionDivisor != s_uLastResolutionDivisor;

	if (!bDenoiseChanged && !bResolutionChanged)
		return;

	// Full graph rebuild — see Flux_SSR::ApplyBlurSelectionToGraph for the same
	// rationale. MarkDirty() alone would leave Flux_DeferredShading reading the
	// stale transient handle captured at the previous SetupRenderGraph; same
	// applies if the raymarch transient's dimensions change.
	Flux::RequestGraphRebuild();
}

Flux_TransientHandle Flux_SSGI::GetSSGIHandle()
{
	const Flux_TransientHandle xLive = Zenith_GraphicsOptions::Get().m_bSSGIDenoiseEnabled ? s_xDenoisedHandle : s_xResolvedHandle;
	Zenith_Assert(!s_xCommittedSSGIHandle.IsValid() || xLive == s_xCommittedSSGIHandle,
		"Flux_SSGI::GetSSGIHandle: live handle (idx=%u gen=%u) disagrees with the handle committed at SetupRenderGraph exit (idx=%u gen=%u). "
		"Denoise toggle changed without Flux::RequestGraphRebuild() being called in ApplyDenoiseSelectionToGraph — "
		"MarkDirty() alone does not re-run SetupRenderGraph, so the deferred pass's declared Read is stale.",
		xLive.m_uIndex, xLive.m_uGeneration,
		s_xCommittedSSGIHandle.m_uIndex, s_xCommittedSSGIHandle.m_uGeneration);
	return xLive;
}

Flux_ShaderResourceView& Flux_SSGI::GetSSGISRV()
{
	// Must match GetSSGIHandle so bind-time-declared-access assertions see the
	// same resource the graph has a Read declared on.
	if (Zenith_GraphicsOptions::Get().m_bSSGIDenoiseEnabled)
		return GetDenoisedAttachment().SRV();
	return GetResolvedAttachment().SRV();
}

bool Flux_SSGI::IsEnabled()
{
	return Zenith_GraphicsOptions::Get().m_bSSGIEnabled && s_bInitialised;
}

bool Flux_SSGI::IsInitialised()
{
	return s_bInitialised;
}
