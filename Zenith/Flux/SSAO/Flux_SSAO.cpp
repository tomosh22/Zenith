#include "Zenith.h"
#include "Flux/SSAO/Flux_SSAO_Shaders.h"
#include "Core/Zenith_Engine.h"

#include "Flux/SSAO/Flux_SSAOImpl.h"

#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/HDR/Flux_HDRImpl.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Flux/Shaders/Generated/SSAO.h" // typed binding handles
#include "Core/Zenith_GraphicsOptions.h"
#include "DebugVariables/Zenith_DebugVariables.h"
// GetWidth/GetHeight on the swapchain (reached via g_xEngine at point of use)
// need the full type here.
#include "Flux/Flux_BackendTypes.h"

// Shaders and pipelines

// Graph-owned transient handles — backing Flux_RenderAttachments are allocated
// and destroyed by the render graph, sized from the descriptors set in
// SetupRenderGraph.

// SSAO render target format (half-res single-channel).
static constexpr TextureFormat SSAO_FORMAT = TEXTURE_FORMAT_R8_UNORM;

static struct SSAOGenerateConstants
{
	float m_fRadius = 0.5f;
	float m_fBias = 0.025f;
	float m_fIntensity = 1.5f;
	float m_fKernelSize = 32.f;
} dbg_xGenerateConstants;

static struct SSAOBlurConstants
{
	float m_fSpatialSigma = 1.5f;
	float m_fDepthSigma = 0.02f;
	float m_fNormalSigma = 0.2f;
	u_int m_uKernelRadius = 3;
} dbg_xBlurConstants;

// Attachment accessors — always resolve through the graph's transient slot.
// Now non-static members: read their own m_pxGraph / m_x*Handle instead of
// reaching for g_xEngine.SSAO() (mirror HiZ GetHiZBuffer).
Flux_RenderAttachment& Flux_SSAOImpl::GetRawOcclusion()
{
	Zenith_Assert(m_pxGraph, "Flux_SSAOImpl::GetRawOcclusion: graph pointer is null (called before SetupRenderGraph or after Shutdown)");
	return m_pxGraph->GetTransientAttachment(m_xRawOcclusionHandle);
}
Flux_RenderAttachment& Flux_SSAOImpl::GetBlurred()
{
	Zenith_Assert(m_pxGraph, "Flux_SSAOImpl::GetBlurred: graph pointer is null (called before SetupRenderGraph or after Shutdown)");
	return m_pxGraph->GetTransientAttachment(m_xBlurredHandle);
}

#ifdef ZENITH_DEBUG_VARIABLES
// Debug-texture callbacks — resolved every ImGui draw so the preview follows
// render-graph rebuilds. Returning nullptr before SetupRenderGraph avoids the
// null-guard asserts on the attachment getters above. Non-capturing fn-pointer
// trampolines: re-enter via g_xEngine.SSAO() to reach the singleton instance.
static const Flux_ShaderResourceView* DebugGetRawOcclusionSRV()
{
	Flux_SSAOImpl& xSSAO = g_xEngine.SSAO();
	if (xSSAO.m_pxGraph == nullptr) return nullptr;
	return &xSSAO.GetRawOcclusion().SRV();
}
static const Flux_ShaderResourceView* DebugGetBlurredSRV()
{
	Flux_SSAOImpl& xSSAO = g_xEngine.SSAO();
	if (xSSAO.m_pxGraph == nullptr) return nullptr;
	return &xSSAO.GetBlurred().SRV();
}
#endif

// ---- Init / Shutdown ----

void Flux_SSAOImpl::BuildPipelines()
{
	Flux_PipelineHelper::BuildFullscreenPipeline(
		m_xGenerateShader, m_xGeneratePipeline,
		Flux_SSAOShaders::xSSAO_Main, SSAO_FORMAT);

	Flux_PipelineHelper::BuildFullscreenPipeline(
		m_xBlurShader, m_xBlurPipeline,
		Flux_SSAOShaders::xSSAO_Blur, SSAO_FORMAT);

	// The old post-lighting upsample pipeline (multiplicative composite onto the
	// HDR scene) is retired: SSAO is now folded into the deferred ambient term
	// (see Flux_DeferredShading), so it darkens only ambient/IBL — never direct
	// light. The blurred (or raw) half-res target is sampled there directly.
}

void Flux_SSAOImpl::Initialise()
{
	BuildPipelines();

#ifdef ZENITH_DEBUG_VARIABLES
	g_xEngine.DebugVariables().AddFloat({ "Render", "SSAO", "Radius" }, dbg_xGenerateConstants.m_fRadius, 0.01f, 2.f);
	g_xEngine.DebugVariables().AddFloat({ "Render", "SSAO", "Bias" }, dbg_xGenerateConstants.m_fBias, 0.01f, 2.f);
	g_xEngine.DebugVariables().AddFloat({ "Render", "SSAO", "Intensity" }, dbg_xGenerateConstants.m_fIntensity, 0.01f, 2.f);
	g_xEngine.DebugVariables().AddFloat({ "Render", "SSAO", "Kernel Size" }, dbg_xGenerateConstants.m_fKernelSize, 8.f, 64.f);

	g_xEngine.DebugVariables().AddFloat({ "Render", "SSAO", "Blur", "Spatial Sigma" }, dbg_xBlurConstants.m_fSpatialSigma, 0.5f, 4.f);
	g_xEngine.DebugVariables().AddFloat({ "Render", "SSAO", "Blur", "Depth Sigma" }, dbg_xBlurConstants.m_fDepthSigma, 0.005f, 0.1f);
	g_xEngine.DebugVariables().AddFloat({ "Render", "SSAO", "Blur", "Normal Sigma" }, dbg_xBlurConstants.m_fNormalSigma, 0.1f, 1.f);
	g_xEngine.DebugVariables().AddUInt32({ "Render", "SSAO", "Blur", "Kernel Radius" }, dbg_xBlurConstants.m_uKernelRadius, 1, 5);

	// Transient-SRV previews: AddTextureCallback re-resolves through g_xEngine.SSAO().m_pxGraph
	// each ImGui draw so the preview survives graph rebuilds (resize, toggle).
	g_xEngine.DebugVariables().AddTextureCallback({ "Render", "SSAO", "Textures", "Raw" },     &DebugGetRawOcclusionSRV);
	g_xEngine.DebugVariables().AddTextureCallback({ "Render", "SSAO", "Textures", "Blurred" }, &DebugGetBlurredSRV);
#endif

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_SSAO initialised");
}

void Flux_SSAOImpl::Shutdown()
{
	m_pxGraph = nullptr;
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_SSAO shut down");
}

// ---- Execute callbacks (use GetRawOcclusion/GetBlurred accessors) ----

static void ExecuteSSAOGenerate(Flux_CommandBuffer* pxCommandList, void*)
{
	if (!Zenith_GraphicsOptions::Get().m_bSSAOEnabled)
		return;

	// Non-capturing graph callback (void(*)(Flux_CommandBuffer*, void*)) — it
	// cannot capture, so it re-enters via g_xEngine.SSAO() to reach the singleton
	// instance; other cross-subsystem deps are reached via g_xEngine at point of
	// use (mirrors ExecuteHiZMip).
	Flux_SSAOImpl& xSSAO = g_xEngine.SSAO();
	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();

	xGraphics.BindFullscreenQuad(*pxCommandList, xSSAO.m_xGeneratePipeline);

	namespace NS = Flux_Generated_SSAO::SSAO_Main;
	Flux_ShaderBinder xBinder(*pxCommandList);
	xBinder.BindDrawConstants(NS::hSSAOConstants, &dbg_xGenerateConstants, sizeof(SSAOGenerateConstants));
	xBinder.BindSRV(NS::hg_xDepthTex, xGraphics.GetDepthStencilSRV());
	xBinder.BindSRV(NS::hg_xNormalTex, xGraphics.GetGBufferSRV(MRT_INDEX_NORMALSAMBIENT));

	pxCommandList->DrawIndexed(6);
}

static void ExecuteSSAOBlur(Flux_CommandBuffer* pxCommandList, void*)
{
	const Zenith_GraphicsOptions& xOpts = Zenith_GraphicsOptions::Get();
	if (!xOpts.m_bSSAOEnabled || !xOpts.m_bSSAOBlurEnabled)
		return;

	Flux_SSAOImpl& xSSAO = g_xEngine.SSAO();
	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();

	xGraphics.BindFullscreenQuad(*pxCommandList, xSSAO.m_xBlurPipeline);

	namespace NS = Flux_Generated_SSAO::SSAO_Blur;
	Flux_ShaderBinder xBinder(*pxCommandList);
	xBinder.BindDrawConstants(NS::hSSAOBlurConstants, &dbg_xBlurConstants, sizeof(SSAOBlurConstants));
	xBinder.BindSRV(NS::hg_xOcclusionTex, &xSSAO.GetRawOcclusion().SRV());
	xBinder.BindSRV(NS::hg_xDepthTex, xGraphics.GetDepthStencilSRV());
	xBinder.BindSRV(NS::hg_xNormalTex, xGraphics.GetGBufferSRV(MRT_INDEX_NORMALSAMBIENT));

	pxCommandList->DrawIndexed(6);
}

// ---- Render graph setup (chooses transient vs owned based on toggle) ----

void Flux_SSAOImpl::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	m_pxGraph = &xGraph;

	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();

	const u_int uHalfWidth  = g_xEngine.FluxSwapchain().GetWidth()  / 2;
	const u_int uHalfHeight = g_xEngine.FluxSwapchain().GetHeight() / 2;

	Flux_TransientTextureDesc xSSAODesc;
	xSSAODesc.m_uWidth       = uHalfWidth;
	xSSAODesc.m_uHeight      = uHalfHeight;
	xSSAODesc.m_eFormat      = SSAO_FORMAT;
	xSSAODesc.m_uMemoryFlags = (1u << MEMORY_FLAGS__SHADER_READ);
	m_xRawOcclusionHandle = xGraph.CreateTransient(xSSAODesc);
	m_xBlurredHandle      = xGraph.CreateTransient(xSSAODesc);

	xGraph.AddPass("SSAO Generate", ExecuteSSAOGenerate)
		.ClearTargets()
		.Reads         (xGraphics.GetDepthAttachment(),                       RESOURCE_ACCESS_READ_SRV)
		.Reads         (xGraphics.GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT), RESOURCE_ACCESS_READ_SRV)
		.WritesTransient(m_xRawOcclusionHandle,                               RESOURCE_ACCESS_WRITE_RTV);

	xGraph.AddPass("SSAO Blur", ExecuteSSAOBlur)
		.ClearTargets()
		.Reads         (xGraphics.GetDepthAttachment(),                       RESOURCE_ACCESS_READ_SRV)
		.Reads         (xGraphics.GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT), RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient (m_xRawOcclusionHandle,                               RESOURCE_ACCESS_READ_SRV)
		.WritesTransient(m_xBlurredHandle,                                    RESOURCE_ACCESS_WRITE_RTV);

	// No upsample/composite pass: DeferredShading reads m_xBlurredHandle (or the
	// raw handle when the blur is toggled off) and multiplies the AO into its
	// ambient term. The graph orders these passes before DeferredShading via that
	// read; SSAO registers ahead of DeferredShading in the feature setup walk.
}
