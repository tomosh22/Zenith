#include "Zenith.h"

#include "Flux/SSAO/Flux_SSAO.h"

#include "Flux/Flux.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/HDR/Flux_HDR.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Zenith_PlatformGraphics_Include.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "DebugVariables/Zenith_DebugVariables.h"

// Shaders and pipelines
static Flux_Shader s_xGenerateShader;
static Flux_Shader s_xBlurShader;
static Flux_Shader s_xUpsampleShader;
static Flux_Pipeline s_xGeneratePipeline;
static Flux_Pipeline s_xBlurPipeline;
static Flux_Pipeline s_xUpsamplePipeline;

// Graph-owned transient handles — backing Flux_RenderAttachments are allocated
// and destroyed by the render graph, sized from the descriptors set in
// SetupRenderGraph.
static Flux_TransientHandle s_xRawOcclusionHandle;
static Flux_TransientHandle s_xBlurredHandle;
static Flux_RenderGraph* s_pxGraph = nullptr;

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
	Zenith_Assert(s_pxGraph, "Flux_SSAO::GetRawOcclusion: graph pointer is null (called before SetupRenderGraph or after Shutdown)");
	return s_pxGraph->GetTransientAttachment(s_xRawOcclusionHandle);
}
static Flux_RenderAttachment& GetBlurred()
{
	Zenith_Assert(s_pxGraph, "Flux_SSAO::GetBlurred: graph pointer is null (called before SetupRenderGraph or after Shutdown)");
	return s_pxGraph->GetTransientAttachment(s_xBlurredHandle);
}

#ifdef ZENITH_DEBUG_VARIABLES
// Debug-texture callbacks — resolved every ImGui draw so the preview follows
// render-graph rebuilds. Returning nullptr before SetupRenderGraph avoids the
// null-guard asserts on the attachment getters above.
static const Flux_ShaderResourceView* DebugGetRawOcclusionSRV()
{
	if (s_pxGraph == nullptr) return nullptr;
	return &GetRawOcclusion().SRV();
}
static const Flux_ShaderResourceView* DebugGetBlurredSRV()
{
	if (s_pxGraph == nullptr) return nullptr;
	return &GetBlurred().SRV();
}
#endif

// ---- Init / Shutdown ----

void Flux_SSAO::Initialise()
{
	Flux_PipelineHelper::BuildFullscreenPipeline(
		s_xGenerateShader, s_xGeneratePipeline,
		"SSAO/Flux_SSAO.frag", SSAO_FORMAT);

	Flux_PipelineHelper::BuildFullscreenPipeline(
		s_xBlurShader, s_xBlurPipeline,
		"SSAO/Flux_SSAO_Blur.frag", SSAO_FORMAT);

	{
		Flux_PipelineSpecification xSpec = Flux_PipelineHelper::CreateFullscreenSpec(
			s_xUpsampleShader, "SSAO/Flux_SSAO_Upsample.frag", HDR_SCENE_FORMAT);

		xSpec.m_axBlendStates[0].m_bBlendEnabled = true;
		xSpec.m_axBlendStates[0].m_eSrcBlendFactor = BLEND_FACTOR_ZERO;
		xSpec.m_axBlendStates[0].m_eDstBlendFactor = BLEND_FACTOR_SRCALPHA;

		Flux_PipelineBuilder::FromSpecification(s_xUpsamplePipeline, xSpec);
	}

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddFloat({ "Render", "SSAO", "Radius" }, dbg_xGenerateConstants.m_fRadius, 0.01f, 2.f);
	Zenith_DebugVariables::AddFloat({ "Render", "SSAO", "Bias" }, dbg_xGenerateConstants.m_fBias, 0.01f, 2.f);
	Zenith_DebugVariables::AddFloat({ "Render", "SSAO", "Intensity" }, dbg_xGenerateConstants.m_fIntensity, 0.01f, 2.f);
	Zenith_DebugVariables::AddFloat({ "Render", "SSAO", "Kernel Size" }, dbg_xGenerateConstants.m_fKernelSize, 8.f, 64.f);

	Zenith_DebugVariables::AddFloat({ "Render", "SSAO", "Blur", "Spatial Sigma" }, dbg_xBlurConstants.m_fSpatialSigma, 0.5f, 4.f);
	Zenith_DebugVariables::AddFloat({ "Render", "SSAO", "Blur", "Depth Sigma" }, dbg_xBlurConstants.m_fDepthSigma, 0.005f, 0.1f);
	Zenith_DebugVariables::AddFloat({ "Render", "SSAO", "Blur", "Normal Sigma" }, dbg_xBlurConstants.m_fNormalSigma, 0.1f, 1.f);
	Zenith_DebugVariables::AddUInt32({ "Render", "SSAO", "Blur", "Kernel Radius" }, dbg_xBlurConstants.m_uKernelRadius, 1, 5);

	// Transient-SRV previews: AddTextureCallback re-resolves through s_pxGraph
	// each ImGui draw so the preview survives graph rebuilds (resize, toggle).
	Zenith_DebugVariables::AddTextureCallback({ "Render", "SSAO", "Textures", "Raw" },     &DebugGetRawOcclusionSRV);
	Zenith_DebugVariables::AddTextureCallback({ "Render", "SSAO", "Textures", "Blurred" }, &DebugGetBlurredSRV);
#endif

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_SSAO initialised");
}

void Flux_SSAO::Shutdown()
{
	s_pxGraph = nullptr;
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_SSAO shut down");
}

// ---- Execute callbacks (use GetRawOcclusion/GetBlurred helpers) ----

static void ExecuteSSAOGenerate(Flux_CommandList* pxCommandList, void*)
{
	if (!Zenith_GraphicsOptions::Get().m_bSSAOEnabled)
		return;

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&s_xGeneratePipeline);
	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);
	xBinder.BindCBV(s_xGenerateShader, "FrameConstants", &Flux_Graphics::s_xFrameConstantsBuffer.GetCBV());
	xBinder.BindDrawConstants(s_xGenerateShader, "SSAOConstants", &dbg_xGenerateConstants, sizeof(SSAOGenerateConstants));
	xBinder.BindSRV(s_xGenerateShader, "g_xDepthTex", Flux_Graphics::GetDepthStencilSRV());
	xBinder.BindSRV(s_xGenerateShader, "g_xNormalTex", Flux_Graphics::GetGBufferSRV(MRT_INDEX_NORMALSAMBIENT));

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
}

static void ExecuteSSAOBlur(Flux_CommandList* pxCommandList, void*)
{
	const Zenith_GraphicsOptions& xOpts = Zenith_GraphicsOptions::Get();
	if (!xOpts.m_bSSAOEnabled || !xOpts.m_bSSAOBlurEnabled)
		return;

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&s_xBlurPipeline);
	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);
	xBinder.BindDrawConstants(s_xBlurShader, "SSAOBlurConstants", &dbg_xBlurConstants, sizeof(SSAOBlurConstants));
	xBinder.BindSRV(s_xBlurShader, "g_xOcclusionTex", &GetRawOcclusion().SRV());
	xBinder.BindSRV(s_xBlurShader, "g_xDepthTex", Flux_Graphics::GetDepthStencilSRV());
	xBinder.BindSRV(s_xBlurShader, "g_xNormalTex", Flux_Graphics::GetGBufferSRV(MRT_INDEX_NORMALSAMBIENT));

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
}

static void ExecuteSSAOUpsample(Flux_CommandList* pxCommandList, void*)
{
	const Zenith_GraphicsOptions& xOpts = Zenith_GraphicsOptions::Get();
	if (!xOpts.m_bSSAOEnabled)
		return;

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&s_xUpsamplePipeline);
	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);
	if (xOpts.m_bSSAOBlurEnabled)
		xBinder.BindSRV(s_xUpsampleShader, "g_xOcclusionTex", &GetBlurred().SRV());
	else
		xBinder.BindSRV(s_xUpsampleShader, "g_xOcclusionTex", &GetRawOcclusion().SRV());

	xBinder.BindSRV(s_xUpsampleShader, "g_xDepthTex", Flux_Graphics::GetDepthStencilSRV());

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
}

// ---- Render graph setup (chooses transient vs owned based on toggle) ----

void Flux_SSAO::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	s_pxGraph = &xGraph;

	const u_int uHalfWidth  = Flux_Swapchain::GetWidth()  / 2;
	const u_int uHalfHeight = Flux_Swapchain::GetHeight() / 2;

	Flux_TransientTextureDesc xSSAODesc;
	xSSAODesc.m_uWidth       = uHalfWidth;
	xSSAODesc.m_uHeight      = uHalfHeight;
	xSSAODesc.m_eFormat      = SSAO_FORMAT;
	xSSAODesc.m_uMemoryFlags = (1u << MEMORY_FLAGS__SHADER_READ);
	s_xRawOcclusionHandle = xGraph.CreateTransient(xSSAODesc);
	s_xBlurredHandle      = xGraph.CreateTransient(xSSAODesc);

	xGraph.AddPass("SSAO Generate", ExecuteSSAOGenerate)
		.ClearTargets()
		.Reads         (Flux_Graphics::GetDepthAttachment(),                       RESOURCE_ACCESS_READ_SRV)
		.Reads         (Flux_Graphics::GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT), RESOURCE_ACCESS_READ_SRV)
		.WritesTransient(s_xRawOcclusionHandle,                                     RESOURCE_ACCESS_WRITE_RTV);

	xGraph.AddPass("SSAO Blur", ExecuteSSAOBlur)
		.ClearTargets()
		.Reads         (Flux_Graphics::GetDepthAttachment(),                       RESOURCE_ACCESS_READ_SRV)
		.Reads         (Flux_Graphics::GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT), RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient (s_xRawOcclusionHandle,                                     RESOURCE_ACCESS_READ_SRV)
		.WritesTransient(s_xBlurredHandle,                                          RESOURCE_ACCESS_WRITE_RTV);

	// Upsample pass — writes AO factor onto the HDR scene via multiplicative blend.
	xGraph.AddPass("SSAO Upsample", ExecuteSSAOUpsample)
		.Reads         (Flux_Graphics::GetDepthAttachment(), RESOURCE_ACCESS_READ_SRV)
		.Writes        (Flux_HDR::GetHDRSceneTarget(),       RESOURCE_ACCESS_WRITE_RTV)
		.ReadsTransient(s_xBlurredHandle,                    RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient(s_xRawOcclusionHandle,               RESOURCE_ACCESS_READ_SRV);
}
