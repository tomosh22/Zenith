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
#include "DebugVariables/Zenith_DebugVariables.h"

// Shaders and pipelines
static Flux_Shader s_xGenerateShader;
static Flux_Shader s_xBlurShader;
static Flux_Shader s_xUpsampleShader;
static Flux_Pipeline s_xGeneratePipeline;
static Flux_Pipeline s_xBlurPipeline;
static Flux_Pipeline s_xUpsamplePipeline;

// ---- Transient path (graph-owned allocation) ----
static Flux_TransientHandle s_xRawOcclusionHandle;
static Flux_TransientHandle s_xBlurredHandle;
static Flux_RenderGraph* s_pxGraph = nullptr;

// ---- Fallback path (subsystem-owned allocation) ----
static Flux_RenderAttachment s_xRawOcclusion_Owned;
static Flux_RenderAttachment s_xBlurred_Owned;

// ---- Toggle: which path is active this frame ----
// Read from the render graph's global AreTransientsEnabled() at SetupRenderGraph time.
static bool s_bUsingTransients = false;

// SSAO render target format (constant — pipeline building doesn't depend on allocation path)
static constexpr TextureFormat SSAO_FORMAT = TEXTURE_FORMAT_R8_UNORM;

bool Flux_SSAO::s_bEnabled = true;

DEBUGVAR bool dbg_bEnable = true;
DEBUGVAR bool dbg_bBlurEnable = true;

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

static Flux_BindingHandle s_xGenerate_FrameConstants;
static Flux_BindingHandle s_xGenerate_DepthTex;
static Flux_BindingHandle s_xGenerate_NormalTex;
static Flux_BindingHandle s_xBlur_OcclusionTex;
static Flux_BindingHandle s_xBlur_DepthTex;
static Flux_BindingHandle s_xBlur_NormalTex;
static Flux_BindingHandle s_xUpsample_OcclusionTex;
static Flux_BindingHandle s_xUpsample_DepthTex;

// ---- Helpers to get the right attachment regardless of path ----

static Flux_RenderAttachment& GetRawOcclusion()
{
	if (s_bUsingTransients)
		return s_pxGraph->GetTransientAttachment(s_xRawOcclusionHandle);
	return s_xRawOcclusion_Owned;
}

static Flux_RenderAttachment& GetBlurred()
{
	if (s_bUsingTransients)
		return s_pxGraph->GetTransientAttachment(s_xBlurredHandle);
	return s_xBlurred_Owned;
}

// ---- Fallback: subsystem-owned create/destroy ----

static void CreateOwnedRenderTargets()
{
	u_int uHalfWidth = Flux_Swapchain::GetWidth() / 2;
	u_int uHalfHeight = Flux_Swapchain::GetHeight() / 2;

	{
		Flux_RenderAttachmentBuilder xBuilder;
		xBuilder.m_uWidth = uHalfWidth;
		xBuilder.m_uHeight = uHalfHeight;
		xBuilder.m_eFormat = SSAO_FORMAT;
		xBuilder.m_uMemoryFlags = (1u << MEMORY_FLAGS__SHADER_READ);
		xBuilder.BuildColour(s_xRawOcclusion_Owned, "SSAO Raw (owned)");
	}
	{
		Flux_RenderAttachmentBuilder xBuilder;
		xBuilder.m_uWidth = uHalfWidth;
		xBuilder.m_uHeight = uHalfHeight;
		xBuilder.m_eFormat = SSAO_FORMAT;
		xBuilder.m_uMemoryFlags = (1u << MEMORY_FLAGS__SHADER_READ);
		xBuilder.BuildColour(s_xBlurred_Owned, "SSAO Blurred (owned)");
	}
}

static void DestroyOwnedRenderTargets()
{
	auto QueueDeletion = [](Flux_RenderAttachment& xAttachment)
	{
		if (xAttachment.m_xVRAMHandle.IsValid())
		{
			Flux_VRAM* pxVRAM = Flux_PlatformAPI::GetVRAM(xAttachment.m_xVRAMHandle);
			Flux_MemoryManager::QueueVRAMDeletion(pxVRAM, xAttachment.m_xVRAMHandle,
				xAttachment.RTV().m_xImageViewHandle,
				xAttachment.DSV().m_xImageViewHandle,
				xAttachment.SRV().m_xImageViewHandle,
				xAttachment.UAV(0).m_xImageViewHandle);
			xAttachment.m_xVRAMHandle = Flux_VRAMHandle();
		}
	};
	QueueDeletion(s_xRawOcclusion_Owned);
	QueueDeletion(s_xBlurred_Owned);
}

// ---- Init / Shutdown ----

void Flux_SSAO::Initialise()
{
	// Owned targets are always created — used when transients are toggled off.
	// TODO: free owned render targets when transients are enabled (and recreate
	// if transients are later disabled). Currently they sit idle, wasting VRAM.
	// This applies to all subsystems that support the transient path.
	CreateOwnedRenderTargets();

	Flux_PipelineHelper::BuildFullscreenPipeline(
		s_xGenerateShader, s_xGeneratePipeline,
		"SSAO/Flux_SSAO.frag", SSAO_FORMAT);

	{
		const Flux_ShaderReflection& xReflection = s_xGenerateShader.GetReflection();
		s_xGenerate_FrameConstants = xReflection.GetBinding("FrameConstants");
		s_xGenerate_DepthTex = xReflection.GetBinding("g_xDepthTex");
		s_xGenerate_NormalTex = xReflection.GetBinding("g_xNormalTex");
	}

	Flux_PipelineHelper::BuildFullscreenPipeline(
		s_xBlurShader, s_xBlurPipeline,
		"SSAO/Flux_SSAO_Blur.frag", SSAO_FORMAT);

	{
		const Flux_ShaderReflection& xReflection = s_xBlurShader.GetReflection();
		s_xBlur_OcclusionTex = xReflection.GetBinding("g_xOcclusionTex");
		s_xBlur_DepthTex = xReflection.GetBinding("g_xDepthTex");
		s_xBlur_NormalTex = xReflection.GetBinding("g_xNormalTex");
	}

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

	Zenith_DebugVariables::AddTexture({ "Render", "SSAO", "Textures", "Raw" }, s_xRawOcclusion_Owned.SRV());
	Zenith_DebugVariables::AddTexture({ "Render", "SSAO", "Textures", "Blurred" }, s_xBlurred_Owned.SRV());
#endif

	Flux::AddResChangeCallback([]()
	{
		DestroyOwnedRenderTargets();
		CreateOwnedRenderTargets();

#ifdef ZENITH_DEBUG_VARIABLES
		Zenith_DebugVariables::AddTexture({ "Render", "SSAO", "Textures", "Raw" }, s_xRawOcclusion_Owned.SRV());
		Zenith_DebugVariables::AddTexture({ "Render", "SSAO", "Textures", "Blurred" }, s_xBlurred_Owned.SRV());
#endif
	});

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_SSAO initialised");
}

void Flux_SSAO::Shutdown()
{
	DestroyOwnedRenderTargets();
	s_pxGraph = nullptr;
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_SSAO shut down");
}

// ---- Execute callbacks (use GetRawOcclusion/GetBlurred helpers) ----

static void ExecuteSSAOGenerate(Flux_CommandList* pxCommandList, void*)
{
	if (!dbg_bEnable || !Flux_SSAO::s_bEnabled)
		return;

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&s_xGeneratePipeline);
	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);
	xBinder.BindCBV(s_xGenerate_FrameConstants, &Flux_Graphics::s_xFrameConstantsBuffer.GetCBV());
	xBinder.BindDrawConstants(&dbg_xGenerateConstants, sizeof(SSAOGenerateConstants));
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
	xBinder.BindDrawConstants(&dbg_xBlurConstants, sizeof(SSAOBlurConstants));
	xBinder.BindSRV(s_xBlur_OcclusionTex, &GetRawOcclusion().SRV());
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
	if (dbg_bBlurEnable)
		xBinder.BindSRV(s_xUpsample_OcclusionTex, &GetBlurred().SRV());
	else
		xBinder.BindSRV(s_xUpsample_OcclusionTex, &GetRawOcclusion().SRV());

	xBinder.BindSRV(s_xUpsample_DepthTex, Flux_Graphics::GetDepthStencilSRV());

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
}

// ---- Render graph setup (chooses transient vs owned based on toggle) ----

void Flux_SSAO::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	s_pxGraph = &xGraph;
	s_bUsingTransients = xGraph.AreTransientsEnabled();

	u_int uHalfWidth = Flux_Swapchain::GetWidth() / 2;
	u_int uHalfHeight = Flux_Swapchain::GetHeight() / 2;

	// Declare transient resources if enabled
	if (s_bUsingTransients)
	{
		Flux_TransientTextureDesc xSSAODesc;
		xSSAODesc.m_uWidth = uHalfWidth;
		xSSAODesc.m_uHeight = uHalfHeight;
		xSSAODesc.m_eFormat = SSAO_FORMAT;
		xSSAODesc.m_uMemoryFlags = (1u << MEMORY_FLAGS__SHADER_READ);

		s_xRawOcclusionHandle = xGraph.CreateTransient(xSSAODesc);
		s_xBlurredHandle = xGraph.CreateTransient(xSSAODesc);
	}

	// Generate pass
	{
		u_int uPassIndex = xGraph.AddPass("SSAO Generate", ExecuteSSAOGenerate);
		xGraph.SetClear(uPassIndex, true);

		xGraph.Read(uPassIndex, Flux_Graphics::GetDepthAttachment(), RESOURCE_ACCESS_READ_SRV);
		xGraph.Read(uPassIndex, Flux_Graphics::GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT), RESOURCE_ACCESS_READ_SRV);

		if (s_bUsingTransients)
			xGraph.WriteTransient(uPassIndex, s_xRawOcclusionHandle, RESOURCE_ACCESS_WRITE_RTV);
		else
			xGraph.Write(uPassIndex, s_xRawOcclusion_Owned, RESOURCE_ACCESS_WRITE_RTV);
	}

	// Blur pass
	{
		u_int uPassIndex = xGraph.AddPass("SSAO Blur", ExecuteSSAOBlur);
		xGraph.SetClear(uPassIndex, true);

		if (s_bUsingTransients)
		{
			xGraph.ReadTransient(uPassIndex, s_xRawOcclusionHandle, RESOURCE_ACCESS_READ_SRV);
			xGraph.WriteTransient(uPassIndex, s_xBlurredHandle, RESOURCE_ACCESS_WRITE_RTV);
		}
		else
		{
			xGraph.Read(uPassIndex, s_xRawOcclusion_Owned, RESOURCE_ACCESS_READ_SRV);
			xGraph.Write(uPassIndex, s_xBlurred_Owned, RESOURCE_ACCESS_WRITE_RTV);
		}

		xGraph.Read(uPassIndex, Flux_Graphics::GetDepthAttachment(), RESOURCE_ACCESS_READ_SRV);
		xGraph.Read(uPassIndex, Flux_Graphics::GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT), RESOURCE_ACCESS_READ_SRV);
	}

	// Upsample pass
	{
		u_int uPassIndex = xGraph.AddPass("SSAO Upsample", ExecuteSSAOUpsample);

		if (s_bUsingTransients)
		{
			xGraph.ReadTransient(uPassIndex, s_xBlurredHandle, RESOURCE_ACCESS_READ_SRV);
			xGraph.ReadTransient(uPassIndex, s_xRawOcclusionHandle, RESOURCE_ACCESS_READ_SRV);
		}
		else
		{
			xGraph.Read(uPassIndex, s_xBlurred_Owned, RESOURCE_ACCESS_READ_SRV);
			xGraph.Read(uPassIndex, s_xRawOcclusion_Owned, RESOURCE_ACCESS_READ_SRV);
		}

		xGraph.Read(uPassIndex, Flux_Graphics::GetDepthAttachment(), RESOURCE_ACCESS_READ_SRV);
		xGraph.Write(uPassIndex, Flux_HDR::GetHDRSceneTarget(), RESOURCE_ACCESS_WRITE_RTV);
	}
}
