#include "Zenith.h"

#include "Flux/SSGI/Flux_SSGI.h"
#include "Flux/HiZ/Flux_HiZ.h"
#include "Flux/Flux.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/HDR/Flux_HDR.h"
#include "Flux/Fog/Flux_VolumeFog.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
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
static Flux_TransientHandle s_xDenoisedHandle;
static Flux_RenderGraph* s_pxGraph = nullptr;

// SSGI render target format.
static constexpr TextureFormat SSGI_FORMAT = TEXTURE_FORMAT_R16G16B16A16_SFLOAT;

// Static member definitions
bool Flux_SSGI::s_bInitialised = false;

// Debug variables
DEBUGVAR u_int dbg_uDebugMode = SSGI_DEBUG_NONE;

static struct SSGIConstants
{
	float m_fIntensity = 1.0f;          // GI intensity multiplier [0-2]
	float m_fMaxDistance = 30.0f;       // Maximum ray march distance in world units
	float m_fThickness = 0.5f;          // Surface thickness for hit detection
	u_int m_uStepCount = 32;            // Ray march steps (HiZ traversal iterations)
	u_int m_uFrameIndex = 0;            // For noise variation
	u_int m_uHiZMipCount = 1;           // Filled from Flux_HiZ
	u_int m_uDebugMode = 0;
	float m_fRoughnessThreshold = 0.0f; // Below this, skip SSGI (0 = process all)
	u_int m_uStartMip = 4;              // Starting mip for HiZ traversal
	u_int m_uRaysPerPixel = 3;          // Number of hemisphere samples per pixel (1-8, default 3)
	float _pad0;
	float _pad1;
} dbg_xSSGIConstants;

// Denoise constants - joint bilateral filter parameters
// m_bEnabled is populated from Zenith_GraphicsOptions before each shader bind so
// the GPU sees the same value the CPU gating reads.
static struct SSGIDenoiseConstants
{
	float m_fSpatialSigma = 2.0f;       // Spatial Gaussian sigma (pixels)
	float m_fDepthSigma = 0.02f;        // Depth threshold (fraction of local depth)
	float m_fNormalSigma = 0.5f;        // Normal threshold (1 - dot product range)
	float m_fAlbedoSigma = 0.1f;        // Albedo threshold (color distance)
	u_int m_uKernelRadius = 4;          // Filter radius in pixels (4 = 9x9 kernel)
	u_int m_bEnabled = 1;               // Mirrors GraphicsOptions::m_bSSGIDenoiseEnabled
	float _pad0;
	float _pad1;
} dbg_xSSGIDenoiseConstants;

// Shaders and pipelines
static Flux_Shader s_xRayMarchShader;
static Flux_Shader s_xUpsampleShader;
static Flux_Shader s_xDenoiseShader;
static Flux_Pipeline s_xRayMarchPipeline;
static Flux_Pipeline s_xUpsamplePipeline;
static Flux_Pipeline s_xDenoisePipeline;

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
		s_xDenoiseShader, s_xDenoisePipeline,
		FluxShaderProgram::SSGI_Denoise, SSGI_FORMAT);
}

void Flux_SSGI::Initialise()
{
	BuildPipelines();

#ifdef ZENITH_TOOLS
	static const FluxShaderProgram s_axPrograms[] = {
		FluxShaderProgram::SSGI_RayMarch,
		FluxShaderProgram::SSGI_Upsample,
		FluxShaderProgram::SSGI_Denoise,
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
	// Transient-SRV previews: AddTextureCallback re-resolves through s_pxGraph
	// each ImGui draw so the preview survives graph rebuilds (resize, toggle).
	Zenith_DebugVariables::AddTextureCallback({ "Flux", "SSGI", "Textures", "Raw" },      &DebugGetRawSRV);
	Zenith_DebugVariables::AddTextureCallback({ "Flux", "SSGI", "Textures", "Resolved" }, &DebugGetResolvedSRV);
	Zenith_DebugVariables::AddTextureCallback({ "Flux", "SSGI", "Textures", "Denoised" }, &DebugGetDenoisedSRV);

	// Denoise debug variables
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

	UpdateSSGIConstants();

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&s_xRayMarchPipeline);

	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);

	xBinder.BindCBV(s_xRayMarchShader, "FrameConstants", &Flux_Graphics::s_xFrameConstantsBuffer.GetCBV());
	xBinder.BindDrawConstants(s_xRayMarchShader, "SSGIConstants", &dbg_xSSGIConstants, sizeof(SSGIConstants));

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

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&s_xUpsamplePipeline);

	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);

	xBinder.BindSRV(s_xUpsampleShader, "g_xSSGITex", &Flux_SSGI::GetRawResultAttachment().SRV());
	xBinder.BindSRV(s_xUpsampleShader, "g_xDepthTex", Flux_Graphics::GetDepthStencilSRV());

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
}

static void ExecuteSSGIDenoise(Flux_CommandList* pxCommandList, void*)
{
	if (!Flux_SSGI::IsEnabled() || !Flux_HiZ::IsEnabled())
		return;

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&s_xDenoisePipeline);

	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);

	dbg_xSSGIDenoiseConstants.m_bEnabled = Zenith_GraphicsOptions::Get().m_bSSGIDenoiseEnabled ? 1u : 0u;
	xBinder.BindDrawConstants(s_xDenoiseShader, "PushConstants", &dbg_xSSGIDenoiseConstants, sizeof(SSGIDenoiseConstants));

	xBinder.BindSRV(s_xDenoiseShader, "g_xSSGITex", &Flux_SSGI::GetResolvedAttachment().SRV());
	xBinder.BindSRV(s_xDenoiseShader, "g_xDepthTex", Flux_Graphics::GetDepthStencilSRV());
	xBinder.BindSRV(s_xDenoiseShader, "g_xNormalsTex", Flux_Graphics::GetGBufferSRV(MRT_INDEX_NORMALSAMBIENT));
	xBinder.BindSRV(s_xDenoiseShader, "g_xAlbedoTex", Flux_Graphics::GetGBufferSRV(MRT_INDEX_DIFFUSE));

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
}

// Handle for the denoise pass so ApplyDenoiseSelectionToGraph can toggle its
// enable bit when GraphicsOptions::m_bSSGIDenoiseEnabled changes.
static Flux_PassHandle s_xDenoisePass;
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
	const u_int uHalfWidth  = uFullWidth  / 2;
	const u_int uHalfHeight = uFullHeight / 2;

	// Raw result is half-res; resolved / denoised are full-res.
	Flux_TransientTextureDesc xHalf;
	xHalf.m_uWidth       = uHalfWidth;
	xHalf.m_uHeight      = uHalfHeight;
	xHalf.m_eFormat      = SSGI_FORMAT;
	xHalf.m_uMemoryFlags = (1u << MEMORY_FLAGS__SHADER_READ);
	s_xRawResultHandle = xGraph.CreateTransient(xHalf);

	Flux_TransientTextureDesc xFull = xHalf;
	xFull.m_uWidth  = uFullWidth;
	xFull.m_uHeight = uFullHeight;
	s_xResolvedHandle = xGraph.CreateTransient(xFull);
	s_xDenoisedHandle = xGraph.CreateTransient(xFull);

	// RayMarch pass (half-res) — reads G-Buffer + HiZ, writes raw result.
	xGraph.AddPass("SSGI RayMarch", ExecuteSSGIRayMarch)
		.ClearTargets()
		.Reads          (Flux_Graphics::GetDepthAttachment(),                       RESOURCE_ACCESS_READ_SRV)
		.Reads          (Flux_HiZ::GetHiZAttachment(),                              RESOURCE_ACCESS_READ_SRV, 0, Flux_HiZ::s_uMipCount)
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

	// Denoise pass — joint bilateral filter. Registered unconditionally;
	// ApplyDenoiseSelectionToGraph toggles its enable bit.
	s_xDenoisePass = xGraph.AddPass("SSGI Denoise", ExecuteSSGIDenoise)
		.ClearTargets()
		.Reads          (Flux_Graphics::GetDepthAttachment(),                       RESOURCE_ACCESS_READ_SRV)
		.Reads          (Flux_Graphics::GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT), RESOURCE_ACCESS_READ_SRV)
		.Reads          (Flux_Graphics::GetMRTAttachment(MRT_INDEX_DIFFUSE),        RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient (s_xResolvedHandle,                                         RESOURCE_ACCESS_READ_SRV)
		.WritesTransient(s_xDenoisedHandle,                                         RESOURCE_ACCESS_WRITE_RTV);

	const bool bDenoise = Zenith_GraphicsOptions::Get().m_bSSGIDenoiseEnabled;
	xGraph.SetEnabled(s_xDenoisePass, bDenoise);
	s_bLastDenoiseEnabled = bDenoise;

	// Commit the handle the deferred pass will now read. GetSSGIHandle asserts
	// against this value — any runtime toggle without a matching
	// Flux::RequestGraphRebuild() trips at the point of the mistake.
	s_xCommittedSSGIHandle = bDenoise ? s_xDenoisedHandle : s_xResolvedHandle;
}

void Flux_SSGI::ApplyDenoiseSelectionToGraph(Flux_RenderGraph& /*xGraph*/)
{
	if (Zenith_GraphicsOptions::Get().m_bSSGIDenoiseEnabled == s_bLastDenoiseEnabled)
		return;

	// Full graph rebuild — see Flux_SSR::ApplyBlurSelectionToGraph for the same
	// rationale. MarkDirty() alone would leave Flux_DeferredShading reading the
	// stale transient handle captured at the previous SetupRenderGraph.
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
