#include "Zenith.h"

#include "Flux/SSAO/Flux_SSAO.h"

#include "Flux/Flux.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/HDR/Flux_HDR.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "DebugVariables/Zenith_DebugVariables.h"

// Shaders and pipelines
static Flux_Shader s_xGenerateShader;
static Flux_Shader s_xBlurShader;
static Flux_Shader s_xUpsampleShader;
static Flux_Pipeline s_xGeneratePipeline;
static Flux_Pipeline s_xBlurPipeline;
static Flux_Pipeline s_xUpsamplePipeline;

// Render targets (half-res R8_UNORM)
static Flux_RenderAttachment s_xRawOcclusion;
static Flux_RenderAttachment s_xBlurred;

bool Flux_SSAO::s_bEnabled = true;

// Generate pass constants
DEBUGVAR bool dbg_bEnable = true;
DEBUGVAR bool dbg_bBlurEnable = true;

static struct SSAOGenerateConstants
{
	float m_fRadius = 0.5f;
	float m_fBias = 0.025f;
	float m_fIntensity = 1.5f;
	float m_fKernelSize = 32.f; // Total samples: 8 directions x 4 steps
} dbg_xGenerateConstants;

// Blur pass constants
static struct SSAOBlurConstants
{
	float m_fSpatialSigma = 1.5f;
	float m_fDepthSigma = 0.02f;
	float m_fNormalSigma = 0.2f;   // ~12° normal threshold (production: 0.1-0.2)
	u_int m_uKernelRadius = 3;     // 7x7 kernel (HBAO+ uses radius 4)
} dbg_xBlurConstants;

// Cached binding handles for generate pass
static Flux_BindingHandle s_xGenerate_FrameConstants;
static Flux_BindingHandle s_xGenerate_DepthTex;
static Flux_BindingHandle s_xGenerate_NormalTex;

// Cached binding handles for blur pass
static Flux_BindingHandle s_xBlur_OcclusionTex;
static Flux_BindingHandle s_xBlur_DepthTex;
static Flux_BindingHandle s_xBlur_NormalTex;

// Cached binding handles for upsample pass
static Flux_BindingHandle s_xUpsample_OcclusionTex;
static Flux_BindingHandle s_xUpsample_DepthTex;

void Flux_SSAO::CreateRenderTargets()
{
	u_int uHalfWidth = Flux_Swapchain::GetWidth() / 2;
	u_int uHalfHeight = Flux_Swapchain::GetHeight() / 2;

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_SSAO::CreateRenderTargets() - Half: %ux%u", uHalfWidth, uHalfHeight);

	// Raw SSAO (half-res)
	{
		Flux_RenderAttachmentBuilder xBuilder;
		xBuilder.m_uWidth = uHalfWidth;
		xBuilder.m_uHeight = uHalfHeight;
		xBuilder.m_eFormat = TEXTURE_FORMAT_R8_UNORM;
		xBuilder.m_uMemoryFlags = (1u << MEMORY_FLAGS__SHADER_READ);

		xBuilder.BuildColour(s_xRawOcclusion, "SSAO Raw");
	}

	// Blurred SSAO (half-res)
	{
		Flux_RenderAttachmentBuilder xBuilder;
		xBuilder.m_uWidth = uHalfWidth;
		xBuilder.m_uHeight = uHalfHeight;
		xBuilder.m_eFormat = TEXTURE_FORMAT_R8_UNORM;
		xBuilder.m_uMemoryFlags = (1u << MEMORY_FLAGS__SHADER_READ);

		xBuilder.BuildColour(s_xBlurred, "SSAO Blurred");
	}
}

void Flux_SSAO::DestroyRenderTargets()
{
	auto QueueDeletion = [](Flux_RenderAttachment& xAttachment)
	{
		if (xAttachment.m_xVRAMHandle.IsValid())
		{
			Zenith_Vulkan_VRAM* pxVRAM = Zenith_Vulkan::GetVRAM(xAttachment.m_xVRAMHandle);
			Flux_MemoryManager::QueueVRAMDeletion(pxVRAM, xAttachment.m_xVRAMHandle,
				xAttachment.RTV().m_xImageViewHandle,
				xAttachment.DSV().m_xImageViewHandle,
				xAttachment.SRV().m_xImageViewHandle,
				xAttachment.UAV(0).m_xImageViewHandle);
		}
	};

	QueueDeletion(s_xRawOcclusion);
	QueueDeletion(s_xBlurred);

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_SSAO::DestroyRenderTargets()");
}

void Flux_SSAO::Initialise()
{
	CreateRenderTargets();

	// Generate pass: fullscreen-quad fragment shader rendering raw SSAO into a
	// half-res R8_UNORM target (no blending). NOTE: this is a graphics pipeline,
	// NOT a Vulkan compute dispatch — the name "Generate" reflects the SSAO
	// computation phase, not VK_PIPELINE_BIND_POINT_COMPUTE.
	Flux_PipelineHelper::BuildFullscreenPipeline(
		s_xGenerateShader, s_xGeneratePipeline,
		"SSAO/Flux_SSAO.frag", s_xRawOcclusion.m_xSurfaceInfo.m_eFormat);

	{
		const Flux_ShaderReflection& xReflection = s_xGenerateShader.GetReflection();
		s_xGenerate_FrameConstants = xReflection.GetBinding("FrameConstants");
		s_xGenerate_DepthTex = xReflection.GetBinding("g_xDepthTex");
		s_xGenerate_NormalTex = xReflection.GetBinding("g_xNormalTex");
	}

	// Blur pass: bilateral blur at half-res (no blending)
	Flux_PipelineHelper::BuildFullscreenPipeline(
		s_xBlurShader, s_xBlurPipeline,
		"SSAO/Flux_SSAO_Blur.frag", s_xBlurred.m_xSurfaceInfo.m_eFormat);

	{
		const Flux_ShaderReflection& xReflection = s_xBlurShader.GetReflection();
		s_xBlur_OcclusionTex = xReflection.GetBinding("g_xOcclusionTex");
		s_xBlur_DepthTex = xReflection.GetBinding("g_xDepthTex");
		s_xBlur_NormalTex = xReflection.GetBinding("g_xNormalTex");
	}

	// Upsample pass: bilateral upsample to full-res with multiplicative HDR blend
	{
		Flux_PipelineSpecification xSpec = Flux_PipelineHelper::CreateFullscreenSpec(
			s_xUpsampleShader, "SSAO/Flux_SSAO_Upsample.frag", Flux_HDR::GetHDRSceneTarget().m_xSurfaceInfo.m_eFormat);

		xSpec.m_axBlendStates[0].m_bBlendEnabled = true;
		xSpec.m_axBlendStates[0].m_eSrcBlendFactor = BLEND_FACTOR_ZERO;
		xSpec.m_axBlendStates[0].m_eDstBlendFactor = BLEND_FACTOR_SRCALPHA;

		Flux_PipelineBuilder::FromSpecification(s_xUpsamplePipeline, xSpec);
	}

	{
		const Flux_ShaderReflection& xReflection = s_xUpsampleShader.GetReflection();
		s_xUpsample_OcclusionTex = xReflection.GetBinding("g_xOcclusionTex");
		s_xUpsample_DepthTex = xReflection.GetBinding("g_xDepthTex");
	}

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({ "Render", "Enable", "SSAO" }, dbg_bEnable);
	Zenith_DebugVariables::AddFloat({ "Render", "SSAO", "Radius" }, dbg_xGenerateConstants.m_fRadius, 0.01f, 2.f);
	Zenith_DebugVariables::AddFloat({ "Render", "SSAO", "Bias" }, dbg_xGenerateConstants.m_fBias, 0.01f, 2.f);
	Zenith_DebugVariables::AddFloat({ "Render", "SSAO", "Intensity" }, dbg_xGenerateConstants.m_fIntensity, 0.01f, 2.f);
	Zenith_DebugVariables::AddFloat({ "Render", "SSAO", "Kernel Size" }, dbg_xGenerateConstants.m_fKernelSize, 8.f, 64.f);

	Zenith_DebugVariables::AddBoolean({ "Render", "SSAO", "Blur", "Enable" }, dbg_bBlurEnable);
	Zenith_DebugVariables::AddFloat({ "Render", "SSAO", "Blur", "Spatial Sigma" }, dbg_xBlurConstants.m_fSpatialSigma, 0.5f, 4.f);
	Zenith_DebugVariables::AddFloat({ "Render", "SSAO", "Blur", "Depth Sigma" }, dbg_xBlurConstants.m_fDepthSigma, 0.005f, 0.1f);
	Zenith_DebugVariables::AddFloat({ "Render", "SSAO", "Blur", "Normal Sigma" }, dbg_xBlurConstants.m_fNormalSigma, 0.1f, 1.f);
	Zenith_DebugVariables::AddUInt32({ "Render", "SSAO", "Blur", "Kernel Radius" }, dbg_xBlurConstants.m_uKernelRadius, 1, 5);

	Zenith_DebugVariables::AddTexture({ "Render", "SSAO", "Textures", "Raw" }, s_xRawOcclusion.SRV());
	Zenith_DebugVariables::AddTexture({ "Render", "SSAO", "Textures", "Blurred" }, s_xBlurred.SRV());
#endif

	// Resize callback
	Flux::AddResChangeCallback([]()
	{
		Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_SSAO resize callback triggered");

		DestroyRenderTargets();
		CreateRenderTargets();

#ifdef ZENITH_DEBUG_VARIABLES
		Zenith_DebugVariables::AddTexture({ "Render", "SSAO", "Textures", "Raw" }, s_xRawOcclusion.SRV());
		Zenith_DebugVariables::AddTexture({ "Render", "SSAO", "Textures", "Blurred" }, s_xBlurred.SRV());
#endif

		Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_SSAO resize complete");
	});

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_SSAO initialised");
}

void Flux_SSAO::Shutdown()
{
	DestroyRenderTargets();
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_SSAO shut down");
}

static void ExecuteSSAOGenerate(Flux_CommandList* pxCommandList, void*)
{
	if (!dbg_bEnable || !Flux_SSAO::s_bEnabled)
		return;

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&s_xGeneratePipeline);

	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);
	xBinder.BindCBV(s_xGenerate_FrameConstants, &Flux_Graphics::s_xFrameConstantsBuffer.GetCBV());
	xBinder.PushConstant(&dbg_xGenerateConstants, sizeof(SSAOGenerateConstants));
	xBinder.BindSRV(s_xGenerate_DepthTex, Flux_Graphics::GetDepthStencilSRV());
	xBinder.BindSRV(s_xGenerate_NormalTex, Flux_Graphics::GetGBufferSRV(MRT_INDEX_NORMALSAMBIENT));

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
}

static void ExecuteSSAOBlur(Flux_CommandList* pxCommandList, void*)
{
	if (!dbg_bEnable || !Flux_SSAO::s_bEnabled || !dbg_bBlurEnable)
		return;

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&s_xBlurPipeline);

	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);
	xBinder.PushConstant(&dbg_xBlurConstants, sizeof(SSAOBlurConstants));
	xBinder.BindSRV(s_xBlur_OcclusionTex, &s_xRawOcclusion.SRV());
	xBinder.BindSRV(s_xBlur_DepthTex, Flux_Graphics::GetDepthStencilSRV());
	xBinder.BindSRV(s_xBlur_NormalTex, Flux_Graphics::GetGBufferSRV(MRT_INDEX_NORMALSAMBIENT));

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
}

static void ExecuteSSAOUpsample(Flux_CommandList* pxCommandList, void*)
{
	if (!dbg_bEnable || !Flux_SSAO::s_bEnabled)
		return;

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&s_xUpsamplePipeline);

	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);

	// Bind blurred or raw depending on blur enable state
	if (dbg_bBlurEnable)
	{
		xBinder.BindSRV(s_xUpsample_OcclusionTex, &s_xBlurred.SRV());
	}
	else
	{
		xBinder.BindSRV(s_xUpsample_OcclusionTex, &s_xRawOcclusion.SRV());
	}

	xBinder.BindSRV(s_xUpsample_DepthTex, Flux_Graphics::GetDepthStencilSRV());

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
}

void Flux_SSAO::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	// Always add all passes — enable/disable is handled at runtime in execute callbacks

	// Generate pass — first writer of the raw occlusion target; clear so the
	// initial render-pass LoadOp is valid against the undefined image layout.
	// Implemented as a fullscreen-quad fragment shader pass (graphics pipeline,
	// not a Vulkan compute dispatch).
	{
		u_int uPassIndex = xGraph.AddPass("SSAO Generate", ExecuteSSAOGenerate);
		xGraph.SetClear(uPassIndex, true);

		xGraph.Read(uPassIndex, Flux_Graphics::s_xDepthBuffer, RESOURCE_ACCESS_READ_SRV);
		xGraph.Read(uPassIndex, Flux_Graphics::s_axMRTColourAttachments[MRT_INDEX_NORMALSAMBIENT], RESOURCE_ACCESS_READ_SRV);
		xGraph.Write(uPassIndex, s_xRawOcclusion, RESOURCE_ACCESS_WRITE_RTV);
	}

	// Blur pass — first writer of the blurred target; clear for the same reason.
	{
		u_int uPassIndex = xGraph.AddPass("SSAO Blur", ExecuteSSAOBlur);
		xGraph.SetClear(uPassIndex, true);

		xGraph.Read(uPassIndex, s_xRawOcclusion, RESOURCE_ACCESS_READ_SRV);
		xGraph.Read(uPassIndex, Flux_Graphics::s_xDepthBuffer, RESOURCE_ACCESS_READ_SRV);
		xGraph.Read(uPassIndex, Flux_Graphics::s_axMRTColourAttachments[MRT_INDEX_NORMALSAMBIENT], RESOURCE_ACCESS_READ_SRV);
		xGraph.Write(uPassIndex, s_xBlurred, RESOURCE_ACCESS_WRITE_RTV);
	}

	// Upsample pass — writes HDR scene via blend, do NOT clear.
	{
		u_int uPassIndex = xGraph.AddPass("SSAO Upsample", ExecuteSSAOUpsample);

		xGraph.Read(uPassIndex, s_xBlurred, RESOURCE_ACCESS_READ_SRV);
		xGraph.Read(uPassIndex, s_xRawOcclusion, RESOURCE_ACCESS_READ_SRV);
		xGraph.Read(uPassIndex, Flux_Graphics::s_xDepthBuffer, RESOURCE_ACCESS_READ_SRV);
		// W8: writes HDR scene via blend (upsampled AO modulates existing contents)
		xGraph.Write(uPassIndex, Flux_HDR::GetHDRSceneTarget(), RESOURCE_ACCESS_WRITE_RTV);
	}
}
