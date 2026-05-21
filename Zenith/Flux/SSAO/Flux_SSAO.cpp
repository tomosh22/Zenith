#include "Zenith.h"

#include "Flux/SSAO/Flux_SSAO.h"
#include "Flux/SSAO/Flux_SSAOImpl.h"

#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/HDR/Flux_HDR.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#ifdef ZENITH_TOOLS
#include "Flux/Slang/Flux_ShaderHotReload.h"
#endif

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
static Flux_RenderAttachment& GetRawOcclusion()
{
	Zenith_Assert(g_xEngine.SSAO().m_pxGraph, "Flux_SSAO::GetRawOcclusion: graph pointer is null (called before SetupRenderGraph or after Shutdown)");
	return g_xEngine.SSAO().m_pxGraph->GetTransientAttachment(g_xEngine.SSAO().m_xRawOcclusionHandle);
}
static Flux_RenderAttachment& GetBlurred()
{
	Zenith_Assert(g_xEngine.SSAO().m_pxGraph, "Flux_SSAO::GetBlurred: graph pointer is null (called before SetupRenderGraph or after Shutdown)");
	return g_xEngine.SSAO().m_pxGraph->GetTransientAttachment(g_xEngine.SSAO().m_xBlurredHandle);
}

#ifdef ZENITH_DEBUG_VARIABLES
// Debug-texture callbacks — resolved every ImGui draw so the preview follows
// render-graph rebuilds. Returning nullptr before SetupRenderGraph avoids the
// null-guard asserts on the attachment getters above.
static const Flux_ShaderResourceView* DebugGetRawOcclusionSRV()
{
	if (g_xEngine.SSAO().m_pxGraph == nullptr) return nullptr;
	return &GetRawOcclusion().SRV();
}
static const Flux_ShaderResourceView* DebugGetBlurredSRV()
{
	if (g_xEngine.SSAO().m_pxGraph == nullptr) return nullptr;
	return &GetBlurred().SRV();
}
#endif

// ---- Init / Shutdown ----

void Flux_SSAO::BuildPipelines()
{
	Flux_PipelineHelper::BuildFullscreenPipeline(
		g_xEngine.SSAO().m_xGenerateShader, g_xEngine.SSAO().m_xGeneratePipeline,
		FluxShaderProgram::SSAO_Main, SSAO_FORMAT);

	Flux_PipelineHelper::BuildFullscreenPipeline(
		g_xEngine.SSAO().m_xBlurShader, g_xEngine.SSAO().m_xBlurPipeline,
		FluxShaderProgram::SSAO_Blur, SSAO_FORMAT);

	{
		Flux_PipelineSpecification xSpec = Flux_PipelineHelper::CreateFullscreenSpec(
			g_xEngine.SSAO().m_xUpsampleShader, FluxShaderProgram::SSAO_Upsample, HDR_SCENE_FORMAT);

		xSpec.m_axBlendStates[0].m_bBlendEnabled = true;
		xSpec.m_axBlendStates[0].m_eSrcBlendFactor = BLEND_FACTOR_ZERO;
		xSpec.m_axBlendStates[0].m_eDstBlendFactor = BLEND_FACTOR_SRCALPHA;

		Flux_PipelineBuilder::FromSpecification(g_xEngine.SSAO().m_xUpsamplePipeline, xSpec);
	}
}

void Flux_SSAO::Initialise()
{
	BuildPipelines();

#ifdef ZENITH_TOOLS
	static const FluxShaderProgram s_axPrograms[] = {
		FluxShaderProgram::SSAO_Main,
		FluxShaderProgram::SSAO_Blur,
		FluxShaderProgram::SSAO_Upsample,
	};
	Flux_ShaderHotReload::RegisterSubsystem(&Flux_SSAO::BuildPipelines,
		s_axPrograms, sizeof(s_axPrograms) / sizeof(s_axPrograms[0]));
#endif

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddFloat({ "Render", "SSAO", "Radius" }, dbg_xGenerateConstants.m_fRadius, 0.01f, 2.f);
	Zenith_DebugVariables::AddFloat({ "Render", "SSAO", "Bias" }, dbg_xGenerateConstants.m_fBias, 0.01f, 2.f);
	Zenith_DebugVariables::AddFloat({ "Render", "SSAO", "Intensity" }, dbg_xGenerateConstants.m_fIntensity, 0.01f, 2.f);
	Zenith_DebugVariables::AddFloat({ "Render", "SSAO", "Kernel Size" }, dbg_xGenerateConstants.m_fKernelSize, 8.f, 64.f);

	Zenith_DebugVariables::AddFloat({ "Render", "SSAO", "Blur", "Spatial Sigma" }, dbg_xBlurConstants.m_fSpatialSigma, 0.5f, 4.f);
	Zenith_DebugVariables::AddFloat({ "Render", "SSAO", "Blur", "Depth Sigma" }, dbg_xBlurConstants.m_fDepthSigma, 0.005f, 0.1f);
	Zenith_DebugVariables::AddFloat({ "Render", "SSAO", "Blur", "Normal Sigma" }, dbg_xBlurConstants.m_fNormalSigma, 0.1f, 1.f);
	Zenith_DebugVariables::AddUInt32({ "Render", "SSAO", "Blur", "Kernel Radius" }, dbg_xBlurConstants.m_uKernelRadius, 1, 5);

	// Transient-SRV previews: AddTextureCallback re-resolves through g_xEngine.SSAO().m_pxGraph
	// each ImGui draw so the preview survives graph rebuilds (resize, toggle).
	Zenith_DebugVariables::AddTextureCallback({ "Render", "SSAO", "Textures", "Raw" },     &DebugGetRawOcclusionSRV);
	Zenith_DebugVariables::AddTextureCallback({ "Render", "SSAO", "Textures", "Blurred" }, &DebugGetBlurredSRV);
#endif

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_SSAO initialised");
}

void Flux_SSAO::Shutdown()
{
	g_xEngine.SSAO().m_pxGraph = nullptr;
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_SSAO shut down");
}

// ---- Execute callbacks (use GetRawOcclusion/GetBlurred helpers) ----

static void ExecuteSSAOGenerate(Flux_CommandList* pxCommandList, void*)
{
	if (!Zenith_GraphicsOptions::Get().m_bSSAOEnabled)
		return;

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&g_xEngine.SSAO().m_xGeneratePipeline);
	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);
	xBinder.BindCBV(g_xEngine.SSAO().m_xGenerateShader, "FrameConstants", &g_xEngine.FluxGraphics().m_xFrameConstantsBuffer.GetCBV());
	xBinder.BindDrawConstants(g_xEngine.SSAO().m_xGenerateShader, "SSAOConstants", &dbg_xGenerateConstants, sizeof(SSAOGenerateConstants));
	xBinder.BindSRV(g_xEngine.SSAO().m_xGenerateShader, "g_xDepthTex", Flux_Graphics::GetDepthStencilSRV());
	xBinder.BindSRV(g_xEngine.SSAO().m_xGenerateShader, "g_xNormalTex", Flux_Graphics::GetGBufferSRV(MRT_INDEX_NORMALSAMBIENT));

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
}

static void ExecuteSSAOBlur(Flux_CommandList* pxCommandList, void*)
{
	const Zenith_GraphicsOptions& xOpts = Zenith_GraphicsOptions::Get();
	if (!xOpts.m_bSSAOEnabled || !xOpts.m_bSSAOBlurEnabled)
		return;

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&g_xEngine.SSAO().m_xBlurPipeline);
	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);
	xBinder.BindDrawConstants(g_xEngine.SSAO().m_xBlurShader, "SSAOBlurConstants", &dbg_xBlurConstants, sizeof(SSAOBlurConstants));
	xBinder.BindSRV(g_xEngine.SSAO().m_xBlurShader, "g_xOcclusionTex", &GetRawOcclusion().SRV());
	xBinder.BindSRV(g_xEngine.SSAO().m_xBlurShader, "g_xDepthTex", Flux_Graphics::GetDepthStencilSRV());
	xBinder.BindSRV(g_xEngine.SSAO().m_xBlurShader, "g_xNormalTex", Flux_Graphics::GetGBufferSRV(MRT_INDEX_NORMALSAMBIENT));

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
}

static void ExecuteSSAOUpsample(Flux_CommandList* pxCommandList, void*)
{
	const Zenith_GraphicsOptions& xOpts = Zenith_GraphicsOptions::Get();
	if (!xOpts.m_bSSAOEnabled)
		return;

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&g_xEngine.SSAO().m_xUpsamplePipeline);
	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);
	if (xOpts.m_bSSAOBlurEnabled)
		xBinder.BindSRV(g_xEngine.SSAO().m_xUpsampleShader, "g_xOcclusionTex", &GetBlurred().SRV());
	else
		xBinder.BindSRV(g_xEngine.SSAO().m_xUpsampleShader, "g_xOcclusionTex", &GetRawOcclusion().SRV());

	xBinder.BindSRV(g_xEngine.SSAO().m_xUpsampleShader, "g_xDepthTex", Flux_Graphics::GetDepthStencilSRV());

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
}

// ---- Render graph setup (chooses transient vs owned based on toggle) ----

void Flux_SSAO::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	g_xEngine.SSAO().m_pxGraph = &xGraph;

	const u_int uHalfWidth  = Flux_Swapchain::GetWidth()  / 2;
	const u_int uHalfHeight = Flux_Swapchain::GetHeight() / 2;

	Flux_TransientTextureDesc xSSAODesc;
	xSSAODesc.m_uWidth       = uHalfWidth;
	xSSAODesc.m_uHeight      = uHalfHeight;
	xSSAODesc.m_eFormat      = SSAO_FORMAT;
	xSSAODesc.m_uMemoryFlags = (1u << MEMORY_FLAGS__SHADER_READ);
	g_xEngine.SSAO().m_xRawOcclusionHandle = xGraph.CreateTransient(xSSAODesc);
	g_xEngine.SSAO().m_xBlurredHandle      = xGraph.CreateTransient(xSSAODesc);

	xGraph.AddPass("SSAO Generate", ExecuteSSAOGenerate)
		.ClearTargets()
		.Reads         (Flux_Graphics::GetDepthAttachment(),                       RESOURCE_ACCESS_READ_SRV)
		.Reads         (Flux_Graphics::GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT), RESOURCE_ACCESS_READ_SRV)
		.WritesTransient(g_xEngine.SSAO().m_xRawOcclusionHandle,                                     RESOURCE_ACCESS_WRITE_RTV);

	xGraph.AddPass("SSAO Blur", ExecuteSSAOBlur)
		.ClearTargets()
		.Reads         (Flux_Graphics::GetDepthAttachment(),                       RESOURCE_ACCESS_READ_SRV)
		.Reads         (Flux_Graphics::GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT), RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient (g_xEngine.SSAO().m_xRawOcclusionHandle,                                     RESOURCE_ACCESS_READ_SRV)
		.WritesTransient(g_xEngine.SSAO().m_xBlurredHandle,                                          RESOURCE_ACCESS_WRITE_RTV);

	// Upsample pass — writes AO factor onto the HDR scene via multiplicative blend.
	xGraph.AddPass("SSAO Upsample", ExecuteSSAOUpsample)
		.Reads         (Flux_Graphics::GetDepthAttachment(), RESOURCE_ACCESS_READ_SRV)
		.Writes        (Flux_HDR::GetHDRSceneTarget(),       RESOURCE_ACCESS_WRITE_RTV)
		.ReadsTransient(g_xEngine.SSAO().m_xBlurredHandle,                    RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient(g_xEngine.SSAO().m_xRawOcclusionHandle,               RESOURCE_ACCESS_READ_SRV);
}
