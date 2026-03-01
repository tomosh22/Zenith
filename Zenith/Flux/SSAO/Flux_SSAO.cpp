#include "Zenith.h"

#include "Flux/SSAO/Flux_SSAO.h"

#include "Flux/Flux.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/HDR/Flux_HDR.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "TaskSystem/Zenith_TaskSystem.h"

// Task system
static Zenith_Task g_xRenderTask(ZENITH_PROFILE_INDEX__FLUX_SSAO, Flux_SSAO::Render, nullptr);

// Command lists (one per pass)
static Flux_CommandList g_xComputeCommandList("SSAO Compute");
static Flux_CommandList g_xBlurCommandList("SSAO Blur");
static Flux_CommandList g_xUpsampleCommandList("SSAO Upsample");

// Shaders and pipelines
static Flux_Shader s_xComputeShader;
static Flux_Shader s_xBlurShader;
static Flux_Shader s_xUpsampleShader;
static Flux_Pipeline s_xComputePipeline;
static Flux_Pipeline s_xBlurPipeline;
static Flux_Pipeline s_xUpsamplePipeline;

// Render targets (half-res R8_UNORM)
static Flux_RenderAttachment s_xRawOcclusion;
static Flux_RenderAttachment s_xBlurred;

// Target setups
static Flux_TargetSetup s_xComputeTargetSetup;
static Flux_TargetSetup s_xBlurTargetSetup;

bool Flux_SSAO::s_bEnabled = true;

// Compute pass constants
DEBUGVAR bool dbg_bEnable = true;
DEBUGVAR bool dbg_bBlurEnable = true;

static struct SSAOComputeConstants
{
	float m_fRadius = 0.5f;
	float m_fBias = 0.025f;
	float m_fIntensity = 1.5f;
	float m_fKernelSize = 32.f; // Total samples: 8 directions x 4 steps
} dbg_xComputeConstants;

// Blur pass constants
static struct SSAOBlurConstants
{
	float m_fSpatialSigma = 1.5f;
	float m_fDepthSigma = 0.02f;
	float m_fNormalSigma = 0.2f;   // ~12° normal threshold (production: 0.1-0.2)
	u_int m_uKernelRadius = 3;     // 7x7 kernel (HBAO+ uses radius 4)
} dbg_xBlurConstants;

// Cached binding handles for compute pass
static Flux_BindingHandle s_xCompute_FrameConstants;
static Flux_BindingHandle s_xCompute_DepthTex;
static Flux_BindingHandle s_xCompute_NormalTex;

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

		s_xComputeTargetSetup.m_axColourAttachments[0] = s_xRawOcclusion;
		s_xComputeTargetSetup.m_pxDepthStencil = nullptr;
	}

	// Blurred SSAO (half-res)
	{
		Flux_RenderAttachmentBuilder xBuilder;
		xBuilder.m_uWidth = uHalfWidth;
		xBuilder.m_uHeight = uHalfHeight;
		xBuilder.m_eFormat = TEXTURE_FORMAT_R8_UNORM;
		xBuilder.m_uMemoryFlags = (1u << MEMORY_FLAGS__SHADER_READ);

		xBuilder.BuildColour(s_xBlurred, "SSAO Blurred");

		s_xBlurTargetSetup.m_axColourAttachments[0] = s_xBlurred;
		s_xBlurTargetSetup.m_pxDepthStencil = nullptr;
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
				xAttachment.m_pxRTV.m_xImageViewHandle,
				xAttachment.m_pxDSV.m_xImageViewHandle,
				xAttachment.m_pxSRV.m_xImageViewHandle,
				xAttachment.m_pxUAV.m_xImageViewHandle);
		}
	};

	QueueDeletion(s_xRawOcclusion);
	QueueDeletion(s_xBlurred);

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_SSAO::DestroyRenderTargets()");
}

void Flux_SSAO::Initialise()
{
	CreateRenderTargets();

	// Compute pass: existing SSAO shader, rendering to half-res R8_UNORM (no blending)
	Flux_PipelineHelper::BuildFullscreenPipeline(
		s_xComputeShader, s_xComputePipeline,
		"SSAO/Flux_SSAO.frag", &s_xComputeTargetSetup);

	{
		const Flux_ShaderReflection& xReflection = s_xComputeShader.GetReflection();
		s_xCompute_FrameConstants = xReflection.GetBinding("FrameConstants");
		s_xCompute_DepthTex = xReflection.GetBinding("g_xDepthTex");
		s_xCompute_NormalTex = xReflection.GetBinding("g_xNormalTex");
	}

	// Blur pass: bilateral blur at half-res (no blending)
	Flux_PipelineHelper::BuildFullscreenPipeline(
		s_xBlurShader, s_xBlurPipeline,
		"SSAO/Flux_SSAO_Blur.frag", &s_xBlurTargetSetup);

	{
		const Flux_ShaderReflection& xReflection = s_xBlurShader.GetReflection();
		s_xBlur_OcclusionTex = xReflection.GetBinding("g_xOcclusionTex");
		s_xBlur_DepthTex = xReflection.GetBinding("g_xDepthTex");
		s_xBlur_NormalTex = xReflection.GetBinding("g_xNormalTex");
	}

	// Upsample pass: bilateral upsample to full-res with multiplicative HDR blend
	{
		Flux_PipelineSpecification xSpec = Flux_PipelineHelper::CreateFullscreenSpec(
			s_xUpsampleShader, "SSAO/Flux_SSAO_Upsample.frag", &Flux_HDR::GetHDRSceneTargetSetup());

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
	Zenith_DebugVariables::AddFloat({ "Render", "SSAO", "Radius" }, dbg_xComputeConstants.m_fRadius, 0.01f, 2.f);
	Zenith_DebugVariables::AddFloat({ "Render", "SSAO", "Bias" }, dbg_xComputeConstants.m_fBias, 0.01f, 2.f);
	Zenith_DebugVariables::AddFloat({ "Render", "SSAO", "Intensity" }, dbg_xComputeConstants.m_fIntensity, 0.01f, 2.f);
	Zenith_DebugVariables::AddFloat({ "Render", "SSAO", "Kernel Size" }, dbg_xComputeConstants.m_fKernelSize, 8.f, 64.f);

	Zenith_DebugVariables::AddBoolean({ "Render", "SSAO", "Blur", "Enable" }, dbg_bBlurEnable);
	Zenith_DebugVariables::AddFloat({ "Render", "SSAO", "Blur", "Spatial Sigma" }, dbg_xBlurConstants.m_fSpatialSigma, 0.5f, 4.f);
	Zenith_DebugVariables::AddFloat({ "Render", "SSAO", "Blur", "Depth Sigma" }, dbg_xBlurConstants.m_fDepthSigma, 0.005f, 0.1f);
	Zenith_DebugVariables::AddFloat({ "Render", "SSAO", "Blur", "Normal Sigma" }, dbg_xBlurConstants.m_fNormalSigma, 0.1f, 1.f);
	Zenith_DebugVariables::AddUInt32({ "Render", "SSAO", "Blur", "Kernel Radius" }, dbg_xBlurConstants.m_uKernelRadius, 1, 5);

	Zenith_DebugVariables::AddTexture({ "Render", "SSAO", "Textures", "Raw" }, s_xRawOcclusion.m_pxSRV);
	Zenith_DebugVariables::AddTexture({ "Render", "SSAO", "Textures", "Blurred" }, s_xBlurred.m_pxSRV);
#endif

	// Resize callback
	Flux::AddResChangeCallback([]()
	{
		Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_SSAO resize callback triggered");

		DestroyRenderTargets();
		CreateRenderTargets();

		g_xComputeCommandList.Reset(true);
		g_xBlurCommandList.Reset(true);
		g_xUpsampleCommandList.Reset(true);

#ifdef ZENITH_DEBUG_VARIABLES
		Zenith_DebugVariables::AddTexture({ "Render", "SSAO", "Textures", "Raw" }, s_xRawOcclusion.m_pxSRV);
		Zenith_DebugVariables::AddTexture({ "Render", "SSAO", "Textures", "Blurred" }, s_xBlurred.m_pxSRV);
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

void Flux_SSAO::Reset()
{
	g_xComputeCommandList.Reset(true);
	g_xBlurCommandList.Reset(true);
	g_xUpsampleCommandList.Reset(true);
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_SSAO::Reset()");
}

void Flux_SSAO::SubmitRenderTask()
{
	Zenith_TaskSystem::SubmitTask(&g_xRenderTask);
}

void Flux_SSAO::WaitForRenderTask()
{
	g_xRenderTask.WaitUntilComplete();
}

void Flux_SSAO::Render(void*)
{
	if (!dbg_bEnable || !s_bEnabled)
	{
		return;
	}

	RenderCompute();

	if (dbg_bBlurEnable)
	{
		RenderBlur();
	}

	RenderUpsample();
}

void Flux_SSAO::RenderCompute()
{
	g_xComputeCommandList.Reset(true);

	g_xComputeCommandList.AddCommand<Flux_CommandSetPipeline>(&s_xComputePipeline);

	g_xComputeCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	g_xComputeCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(g_xComputeCommandList);
	xBinder.BindCBV(s_xCompute_FrameConstants, &Flux_Graphics::s_xFrameConstantsBuffer.GetCBV());
	xBinder.PushConstant(&dbg_xComputeConstants, sizeof(SSAOComputeConstants));
	xBinder.BindSRV(s_xCompute_DepthTex, Flux_Graphics::GetDepthStencilSRV());
	xBinder.BindSRV(s_xCompute_NormalTex, Flux_Graphics::GetGBufferSRV(MRT_INDEX_NORMALSAMBIENT));

	g_xComputeCommandList.AddCommand<Flux_CommandDrawIndexed>(6);

	Flux::SubmitCommandList(&g_xComputeCommandList, s_xComputeTargetSetup, RENDER_ORDER_SSAO);
}

void Flux_SSAO::RenderBlur()
{
	g_xBlurCommandList.Reset(true);

	g_xBlurCommandList.AddCommand<Flux_CommandSetPipeline>(&s_xBlurPipeline);

	g_xBlurCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	g_xBlurCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(g_xBlurCommandList);
	xBinder.PushConstant(&dbg_xBlurConstants, sizeof(SSAOBlurConstants));
	xBinder.BindSRV(s_xBlur_OcclusionTex, &s_xRawOcclusion.m_pxSRV);
	xBinder.BindSRV(s_xBlur_DepthTex, Flux_Graphics::GetDepthStencilSRV());
	xBinder.BindSRV(s_xBlur_NormalTex, Flux_Graphics::GetGBufferSRV(MRT_INDEX_NORMALSAMBIENT));

	g_xBlurCommandList.AddCommand<Flux_CommandDrawIndexed>(6);

	Flux::SubmitCommandList(&g_xBlurCommandList, s_xBlurTargetSetup, RENDER_ORDER_SSAO_BLUR);
}

void Flux_SSAO::RenderUpsample()
{
	g_xUpsampleCommandList.Reset(false);

	g_xUpsampleCommandList.AddCommand<Flux_CommandSetPipeline>(&s_xUpsamplePipeline);

	g_xUpsampleCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	g_xUpsampleCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(g_xUpsampleCommandList);

	// Bind blurred or raw depending on blur enable state
	if (dbg_bBlurEnable)
	{
		xBinder.BindSRV(s_xUpsample_OcclusionTex, &s_xBlurred.m_pxSRV);
	}
	else
	{
		xBinder.BindSRV(s_xUpsample_OcclusionTex, &s_xRawOcclusion.m_pxSRV);
	}

	xBinder.BindSRV(s_xUpsample_DepthTex, Flux_Graphics::GetDepthStencilSRV());

	g_xUpsampleCommandList.AddCommand<Flux_CommandDrawIndexed>(6);

	Flux::SubmitCommandList(&g_xUpsampleCommandList, Flux_HDR::GetHDRSceneTargetSetup(), RENDER_ORDER_SSAO_UPSAMPLE);
}
