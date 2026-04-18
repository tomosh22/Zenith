#include "Zenith.h"

#include "Flux/SSR/Flux_SSR.h"
#include "Flux/HiZ/Flux_HiZ.h"
#include "Flux/Flux.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/HDR/Flux_HDR.h"
#include "Flux/Fog/Flux_VolumeFog.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "DebugVariables/Zenith_DebugVariables.h"

// Graph-owned transient handles — backing Flux_RenderAttachments are allocated
// and destroyed by the render graph, sized from the descriptors set in
// SetupRenderGraph.
static Flux_TransientHandle s_xRayMarchHandle;
static Flux_TransientHandle s_xResolvedHandle;
static Flux_RenderGraph* s_pxGraph = nullptr;

// SSR render target format.
static constexpr TextureFormat SSR_FORMAT = TEXTURE_FORMAT_R16G16B16A16_SFLOAT;

// Static member definitions
bool Flux_SSR::s_bEnabled = true;
bool Flux_SSR::s_bInitialised = false;

// Debug variables
DEBUGVAR bool dbg_bSSREnable = true;
DEBUGVAR bool dbg_bRoughnessBlur = true;
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
	u_int m_uStepCount = 64;      // Max iterations for hierarchical traversal
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
} dbg_xSSRConstants;

// Shaders and pipelines
static Flux_Shader s_xRayMarchShader;
static Flux_Shader s_xResolveShader;
static Flux_Pipeline s_xRayMarchPipeline;
static Flux_Pipeline s_xResolvePipeline;

// ---- Helpers to get the right attachment regardless of path ----

Flux_RenderAttachment& Flux_SSR::GetRayMarchAttachment()
{
	Zenith_Assert(s_pxGraph, "Flux_SSR::GetRayMarchAttachment: graph pointer is null (called before SetupRenderGraph or after Shutdown)");
	return s_pxGraph->GetTransientAttachment(s_xRayMarchHandle);
}
Flux_RenderAttachment& Flux_SSR::GetResolvedAttachment()
{
	Zenith_Assert(s_pxGraph, "Flux_SSR::GetResolvedAttachment: graph pointer is null (called before SetupRenderGraph or after Shutdown)");
	return s_pxGraph->GetTransientAttachment(s_xResolvedHandle);
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
static const Flux_ShaderResourceView* DebugGetResolvedSRV()
{
	if (s_pxGraph == nullptr) return nullptr;
	return &Flux_SSR::GetResolvedAttachment().SRV();
}
#endif

void Flux_SSR::Initialise()
{
	// Initialize ray march shader and pipeline (use constant format)
	Flux_PipelineHelper::BuildFullscreenPipeline(
		s_xRayMarchShader, s_xRayMarchPipeline,
		"SSR/Flux_SSR_RayMarch.frag", SSR_FORMAT);

	// Initialize resolve shader and pipeline (use constant format)
	Flux_PipelineHelper::BuildFullscreenPipeline(
		s_xResolveShader, s_xResolvePipeline,
		"SSR/Flux_SSR_Resolve.frag", SSR_FORMAT);

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({ "Flux", "SSR", "Enable" }, dbg_bSSREnable);
	Zenith_DebugVariables::AddBoolean({ "Flux", "SSR", "RoughnessBlur" }, dbg_bRoughnessBlur);
	Zenith_DebugVariables::AddUInt32({ "Flux", "SSR", "DebugMode" }, dbg_uDebugMode, 0, 100);  // Extended range for diagnostic mode 99
	Zenith_DebugVariables::AddFloat({ "Flux", "SSR", "Intensity" }, dbg_xSSRConstants.m_fIntensity, 0.0f, 2.0f);
	Zenith_DebugVariables::AddFloat({ "Flux", "SSR", "MaxDistance" }, dbg_xSSRConstants.m_fMaxDistance, 1.0f, 100.0f);
	Zenith_DebugVariables::AddFloat({ "Flux", "SSR", "MaxRoughness" }, dbg_xSSRConstants.m_fMaxRoughness, 0.0f, 1.0f);
	Zenith_DebugVariables::AddFloat({ "Flux", "SSR", "Thickness" }, dbg_xSSRConstants.m_fThickness, 0.01f, 1.0f);
	Zenith_DebugVariables::AddUInt32({ "Flux", "SSR", "StepCount" }, dbg_xSSRConstants.m_uStepCount, 8, 256);
	Zenith_DebugVariables::AddUInt32({ "Flux", "SSR", "StartMip" }, dbg_xSSRConstants.m_uStartMip, 0, 10);
	Zenith_DebugVariables::AddFloat({ "Flux", "SSR", "ContactHardeningDist" }, dbg_xSSRConstants.m_fContactHardeningDist, 0.5f, 10.0f);
	// Transient-SRV previews use AddTextureCallback so the SRV is re-resolved
	// through s_pxGraph on every ImGui draw. Storing a stale pointer via
	// AddTexture would go dangling once the graph rebuilds (e.g. on resize).
	Zenith_DebugVariables::AddTextureCallback({ "Flux", "SSR", "Textures", "RayMarch" }, &DebugGetRayMarchSRV);
	Zenith_DebugVariables::AddTextureCallback({ "Flux", "SSR", "Textures", "Resolved" }, &DebugGetResolvedSRV);
#endif

	s_bInitialised = true;
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_SSR initialised");
}

void Flux_SSR::Shutdown()
{
	if (!s_bInitialised)
		return;

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

	// Resolution-based binary search iterations for sub-pixel hit precision
	// 1080p (1920): 6 iterations (1/64 pixel precision)
	// 1440p (2560): 7 iterations (1/128 pixel precision)
	// 4K (3840): 8 iterations (1/256 pixel precision)
	// Using standard resolution breakpoints for accurate thresholding
	u_int uWidth = Flux_Swapchain::GetWidth();
	dbg_xSSRConstants.m_uBinarySearchIterations = 6 + (uWidth > 1920 ? 1 : 0) + (uWidth > 2560 ? 1 : 0);

	// Clamp start mip to valid range
	if (dbg_xSSRConstants.m_uStartMip >= dbg_xSSRConstants.m_uHiZMipCount)
		dbg_xSSRConstants.m_uStartMip = dbg_xSSRConstants.m_uHiZMipCount - 1;
}

static void ExecuteSSRRayMarch(Flux_CommandList* pxCommandList, void*)
{
	if (!dbg_bSSREnable || !Flux_SSR::s_bEnabled || !Flux_SSR::IsInitialised() || !Flux_HiZ::IsEnabled())
		return;

	UpdateSSRConstants();

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&s_xRayMarchPipeline);

	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);

	xBinder.BindCBV(s_xRayMarchShader, "FrameConstants", &Flux_Graphics::s_xFrameConstantsBuffer.GetCBV());
	xBinder.BindDrawConstants(s_xRayMarchShader, "SSRConstants", &dbg_xSSRConstants, sizeof(SSRConstants));

	xBinder.BindSRV(s_xRayMarchShader, "g_xDepthTex", Flux_Graphics::GetDepthStencilSRV());
	xBinder.BindSRV(s_xRayMarchShader, "g_xNormalsTex", Flux_Graphics::GetGBufferSRV(MRT_INDEX_NORMALSAMBIENT));
	xBinder.BindSRV(s_xRayMarchShader, "g_xMaterialTex", Flux_Graphics::GetGBufferSRV(MRT_INDEX_MATERIAL));
	xBinder.BindSRV(s_xRayMarchShader, "g_xHiZTex", &Flux_HiZ::GetHiZSRV());
	xBinder.BindSRV(s_xRayMarchShader, "g_xDiffuseTex", Flux_Graphics::GetGBufferSRV(MRT_INDEX_DIFFUSE));
	xBinder.BindSRV(s_xRayMarchShader, "g_xBlueNoiseTex", &Flux_VolumeFog::GetBlueNoiseTexture()->m_xSRV);

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
}

static void ExecuteSSRResolve(Flux_CommandList* pxCommandList, void*)
{
	if (!dbg_bSSREnable || !Flux_SSR::s_bEnabled || !Flux_SSR::IsInitialised() || !Flux_HiZ::IsEnabled() || !dbg_bRoughnessBlur)
		return;

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&s_xResolvePipeline);

	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);

	xBinder.BindCBV(s_xResolveShader, "FrameConstants", &Flux_Graphics::s_xFrameConstantsBuffer.GetCBV());
	xBinder.BindDrawConstants(s_xResolveShader, "SSRConstants", &dbg_xSSRConstants, sizeof(SSRConstants));

	xBinder.BindSRV(s_xResolveShader, "g_xRayMarchTex", &Flux_SSR::GetRayMarchAttachment().SRV());
	xBinder.BindSRV(s_xResolveShader, "g_xNormalsTex", Flux_Graphics::GetGBufferSRV(MRT_INDEX_NORMALSAMBIENT));
	xBinder.BindSRV(s_xResolveShader, "g_xMaterialTex", Flux_Graphics::GetGBufferSRV(MRT_INDEX_MATERIAL));
	xBinder.BindSRV(s_xResolveShader, "g_xDepthTex", Flux_Graphics::GetDepthStencilSRV());

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
}

// Handle for the resolve pass so ApplyBlurSelectionToGraph can toggle its
// enable bit when dbg_bRoughnessBlur changes.
static Flux_PassHandle s_xResolvePass;
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

	Flux_TransientTextureDesc xSSRDesc;
	xSSRDesc.m_uWidth       = Flux_Swapchain::GetWidth();
	xSSRDesc.m_uHeight      = Flux_Swapchain::GetHeight();
	xSSRDesc.m_eFormat      = SSR_FORMAT;
	xSSRDesc.m_uMemoryFlags = (1u << MEMORY_FLAGS__SHADER_READ);
	s_xRayMarchHandle = xGraph.CreateTransient(xSSRDesc);
	s_xResolvedHandle = xGraph.CreateTransient(xSSRDesc);

	// RayMarch pass — first writer of its target; clear so the initial
	// render-pass LoadOp is valid.
	xGraph.AddPass("SSR RayMarch", ExecuteSSRRayMarch)
		.ClearTargets()
		.Reads          (Flux_Graphics::GetDepthAttachment(),                       RESOURCE_ACCESS_READ_SRV)
		.Reads          (Flux_HiZ::GetHiZAttachment(),                              RESOURCE_ACCESS_READ_SRV, 0, Flux_HiZ::s_uMipCount)
		.Reads          (Flux_Graphics::GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT), RESOURCE_ACCESS_READ_SRV)
		.Reads          (Flux_Graphics::GetMRTAttachment(MRT_INDEX_MATERIAL),       RESOURCE_ACCESS_READ_SRV)
		.Reads          (Flux_Graphics::GetMRTAttachment(MRT_INDEX_DIFFUSE),        RESOURCE_ACCESS_READ_SRV)
		.WritesTransient(s_xRayMarchHandle,                                         RESOURCE_ACCESS_WRITE_RTV);

	// Resolve pass — roughness-weighted blur; clear. Registered unconditionally;
	// ApplyBlurSelectionToGraph toggles its enable bit based on dbg_bRoughnessBlur.
	s_xResolvePass = xGraph.AddPass("SSR Resolve", ExecuteSSRResolve)
		.ClearTargets()
		.Reads          (Flux_Graphics::GetDepthAttachment(),                       RESOURCE_ACCESS_READ_SRV)
		.Reads          (Flux_Graphics::GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT), RESOURCE_ACCESS_READ_SRV)
		.Reads          (Flux_Graphics::GetMRTAttachment(MRT_INDEX_MATERIAL),       RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient (s_xRayMarchHandle,                                         RESOURCE_ACCESS_READ_SRV)
		.WritesTransient(s_xResolvedHandle,                                         RESOURCE_ACCESS_WRITE_RTV);

	xGraph.SetEnabled(s_xResolvePass, dbg_bRoughnessBlur);
	s_bLastBlurEnabled = dbg_bRoughnessBlur;

	// Commit the handle the deferred pass will now read. GetReflectionHandle
	// asserts against this value on every call — any runtime toggle without a
	// matching Flux::RequestGraphRebuild() will trip at the point of the
	// mistake, not downstream in Validate() or AssertBoundResourceDeclared.
	s_xCommittedReflectionHandle = dbg_bRoughnessBlur ? s_xResolvedHandle : s_xRayMarchHandle;
}

void Flux_SSR::ApplyBlurSelectionToGraph(Flux_RenderGraph& /*xGraph*/)
{
	if (dbg_bRoughnessBlur == s_bLastBlurEnabled)
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
	// handle matching the new dbg_bRoughnessBlur value. SetupRenderGraph above
	// will also re-seed s_xCommittedReflectionHandle and re-set the resolve
	// pass's enable bit.
	Flux::RequestGraphRebuild();
}

Flux_TransientHandle Flux_SSR::GetReflectionHandle()
{
	// Blur on → deferred reads the resolved (blurred) output.
	// Blur off → deferred reads the raw ray-march result directly.
	const Flux_TransientHandle xLive = dbg_bRoughnessBlur ? s_xResolvedHandle : s_xRayMarchHandle;
	Zenith_Assert(!s_xCommittedReflectionHandle.IsValid() || xLive == s_xCommittedReflectionHandle,
		"Flux_SSR::GetReflectionHandle: live handle (idx=%u gen=%u) disagrees with the handle committed at SetupRenderGraph exit (idx=%u gen=%u). "
		"dbg_bRoughnessBlur changed without Flux::RequestGraphRebuild() being called in ApplyBlurSelectionToGraph — "
		"MarkDirty() alone does not re-run SetupRenderGraph, so the deferred pass's declared Read is stale.",
		xLive.m_uIndex, xLive.m_uGeneration,
		s_xCommittedReflectionHandle.m_uIndex, s_xCommittedReflectionHandle.m_uGeneration);
	return xLive;
}

Flux_ShaderResourceView& Flux_SSR::GetReflectionSRV()
{
	// Must match GetReflectionHandle so bind-time-declared-access assertions
	// see the same resource the graph has a Read declared on.
	if (dbg_bRoughnessBlur)
		return GetResolvedAttachment().SRV();
	return GetRayMarchAttachment().SRV();
}

bool Flux_SSR::IsEnabled()
{
	return dbg_bSSREnable && s_bEnabled && s_bInitialised;
}

bool Flux_SSR::IsInitialised()
{
	return s_bInitialised;
}
