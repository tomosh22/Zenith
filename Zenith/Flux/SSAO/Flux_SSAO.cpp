#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#include "Flux/SSAO/Flux_SSAOImpl.h"

#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/HDR/Flux_HDRImpl.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "DebugVariables/Zenith_DebugVariables.h"
// Injected dep: GetWidth/GetHeight on the stored swapchain pointer need the
// full type (only transitively available before this seam).
#include "Vulkan/Zenith_Vulkan_Swapchain.h"
#ifdef ZENITH_TOOLS
#include "Flux/Slang/Flux_ShaderHotReload.h"
#endif

// Wave-11 DI seam (2nd leaf seam after WS9.2 HiZ): cross-subsystem deps are
// injected into Initialise and stored as member pointers; g_xEngine.SSAO()
// self-lookup survives only in the non-capturing fn-pointer trampolines below.

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
		FluxShaderProgram::SSAO_Main, SSAO_FORMAT);

	Flux_PipelineHelper::BuildFullscreenPipeline(
		m_xBlurShader, m_xBlurPipeline,
		FluxShaderProgram::SSAO_Blur, SSAO_FORMAT);

	{
		Flux_PipelineSpecification xSpec = Flux_PipelineHelper::CreateFullscreenSpec(
			m_xUpsampleShader, FluxShaderProgram::SSAO_Upsample, HDR_SCENE_FORMAT);

		xSpec.m_axBlendStates[0].m_bBlendEnabled = true;
		xSpec.m_axBlendStates[0].m_eSrcBlendFactor = BLEND_FACTOR_ZERO;
		xSpec.m_axBlendStates[0].m_eDstBlendFactor = BLEND_FACTOR_SRCALPHA;

		Flux_PipelineBuilder::FromSpecification(m_xUpsamplePipeline, xSpec);
	}
}

void Flux_SSAOImpl::Initialise(Flux_GraphicsImpl& xGraphics, Zenith_Vulkan_Swapchain& xSwapchain, Flux_HDRImpl& xHDR)
{
	// Wave-11 DI seam: store the injected cross-subsystem deps. Every later
	// instance-method reach-in routes through these instead of g_xEngine.
	m_pxGraphics  = &xGraphics;
	m_pxSwapchain = &xSwapchain;
	m_pxHDR       = &xHDR;

	BuildPipelines();

#ifdef ZENITH_TOOLS
	static const FluxShaderProgram s_axPrograms[] = {
		FluxShaderProgram::SSAO_Main,
		FluxShaderProgram::SSAO_Blur,
		FluxShaderProgram::SSAO_Upsample,
	};
	// Non-capturing fn-pointer trampoline: re-enters via g_xEngine.SSAO() to
	// reach the singleton instance (it cannot capture `this`).
	Flux_ShaderHotReload::RegisterSubsystem([](){ g_xEngine.SSAO().BuildPipelines(); },
		s_axPrograms, sizeof(s_axPrograms) / sizeof(s_axPrograms[0]));
#endif

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
	// Drop the injected deps so the instance returns to a clean default state.
	m_pxGraphics  = nullptr;
	m_pxSwapchain = nullptr;
	m_pxHDR       = nullptr;
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_SSAO shut down");
}

// ---- Execute callbacks (use GetRawOcclusion/GetBlurred accessors) ----

static void ExecuteSSAOGenerate(Flux_CommandList* pxCommandList, void*)
{
	if (!Zenith_GraphicsOptions::Get().m_bSSAOEnabled)
		return;

	// Non-capturing graph callback (void(*)(Flux_CommandList*, void*)) — it
	// cannot capture, so it re-enters via g_xEngine.SSAO() to reach the singleton
	// instance, then routes its FluxGraphics reach-ins through the injected
	// member (mirrors ExecuteHiZMip).
	Flux_SSAOImpl& xSSAO = g_xEngine.SSAO();

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&xSSAO.m_xGeneratePipeline);
	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&xSSAO.m_pxGraphics->m_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&xSSAO.m_pxGraphics->m_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);
	xBinder.BindCBV(xSSAO.m_xGenerateShader, "FrameConstants", &xSSAO.m_pxGraphics->m_xFrameConstantsBuffer.GetCBV());
	xBinder.BindDrawConstants(xSSAO.m_xGenerateShader, "SSAOConstants", &dbg_xGenerateConstants, sizeof(SSAOGenerateConstants));
	xBinder.BindSRV(xSSAO.m_xGenerateShader, "g_xDepthTex", xSSAO.m_pxGraphics->GetDepthStencilSRV());
	xBinder.BindSRV(xSSAO.m_xGenerateShader, "g_xNormalTex", xSSAO.m_pxGraphics->GetGBufferSRV(MRT_INDEX_NORMALSAMBIENT));

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
}

static void ExecuteSSAOBlur(Flux_CommandList* pxCommandList, void*)
{
	const Zenith_GraphicsOptions& xOpts = Zenith_GraphicsOptions::Get();
	if (!xOpts.m_bSSAOEnabled || !xOpts.m_bSSAOBlurEnabled)
		return;

	Flux_SSAOImpl& xSSAO = g_xEngine.SSAO();

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&xSSAO.m_xBlurPipeline);
	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&xSSAO.m_pxGraphics->m_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&xSSAO.m_pxGraphics->m_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);
	xBinder.BindDrawConstants(xSSAO.m_xBlurShader, "SSAOBlurConstants", &dbg_xBlurConstants, sizeof(SSAOBlurConstants));
	xBinder.BindSRV(xSSAO.m_xBlurShader, "g_xOcclusionTex", &xSSAO.GetRawOcclusion().SRV());
	xBinder.BindSRV(xSSAO.m_xBlurShader, "g_xDepthTex", xSSAO.m_pxGraphics->GetDepthStencilSRV());
	xBinder.BindSRV(xSSAO.m_xBlurShader, "g_xNormalTex", xSSAO.m_pxGraphics->GetGBufferSRV(MRT_INDEX_NORMALSAMBIENT));

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
}

static void ExecuteSSAOUpsample(Flux_CommandList* pxCommandList, void*)
{
	const Zenith_GraphicsOptions& xOpts = Zenith_GraphicsOptions::Get();
	if (!xOpts.m_bSSAOEnabled)
		return;

	Flux_SSAOImpl& xSSAO = g_xEngine.SSAO();

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&xSSAO.m_xUpsamplePipeline);
	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&xSSAO.m_pxGraphics->m_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&xSSAO.m_pxGraphics->m_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);
	if (xOpts.m_bSSAOBlurEnabled)
		xBinder.BindSRV(xSSAO.m_xUpsampleShader, "g_xOcclusionTex", &xSSAO.GetBlurred().SRV());
	else
		xBinder.BindSRV(xSSAO.m_xUpsampleShader, "g_xOcclusionTex", &xSSAO.GetRawOcclusion().SRV());

	xBinder.BindSRV(xSSAO.m_xUpsampleShader, "g_xDepthTex", xSSAO.m_pxGraphics->GetDepthStencilSRV());

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
}

// ---- Render graph setup (chooses transient vs owned based on toggle) ----

void Flux_SSAOImpl::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	m_pxGraph = &xGraph;

	const u_int uHalfWidth  = m_pxSwapchain->GetWidth()  / 2;
	const u_int uHalfHeight = m_pxSwapchain->GetHeight() / 2;

	Flux_TransientTextureDesc xSSAODesc;
	xSSAODesc.m_uWidth       = uHalfWidth;
	xSSAODesc.m_uHeight      = uHalfHeight;
	xSSAODesc.m_eFormat      = SSAO_FORMAT;
	xSSAODesc.m_uMemoryFlags = (1u << MEMORY_FLAGS__SHADER_READ);
	m_xRawOcclusionHandle = xGraph.CreateTransient(xSSAODesc);
	m_xBlurredHandle      = xGraph.CreateTransient(xSSAODesc);

	xGraph.AddPass("SSAO Generate", ExecuteSSAOGenerate)
		.ClearTargets()
		.Reads         (m_pxGraphics->GetDepthAttachment(),                       RESOURCE_ACCESS_READ_SRV)
		.Reads         (m_pxGraphics->GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT), RESOURCE_ACCESS_READ_SRV)
		.WritesTransient(m_xRawOcclusionHandle,                                   RESOURCE_ACCESS_WRITE_RTV);

	xGraph.AddPass("SSAO Blur", ExecuteSSAOBlur)
		.ClearTargets()
		.Reads         (m_pxGraphics->GetDepthAttachment(),                       RESOURCE_ACCESS_READ_SRV)
		.Reads         (m_pxGraphics->GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT), RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient (m_xRawOcclusionHandle,                                   RESOURCE_ACCESS_READ_SRV)
		.WritesTransient(m_xBlurredHandle,                                        RESOURCE_ACCESS_WRITE_RTV);

	// Upsample pass — writes AO factor onto the HDR scene via multiplicative blend.
	xGraph.AddPass("SSAO Upsample", ExecuteSSAOUpsample)
		.Reads         (m_pxGraphics->GetDepthAttachment(), RESOURCE_ACCESS_READ_SRV)
		.Writes        (m_pxHDR->GetHDRSceneTarget(),       RESOURCE_ACCESS_WRITE_RTV)
		.ReadsTransient(m_xBlurredHandle,                   RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient(m_xRawOcclusionHandle,              RESOURCE_ACCESS_READ_SRV);
}
