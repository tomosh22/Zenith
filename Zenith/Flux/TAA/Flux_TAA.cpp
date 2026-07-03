#include "Zenith.h"

#include "Flux/TAA/Flux_TAAImpl.h"
#include "Flux/TAA/Flux_TAA_Shaders.h"

#include "Core/Zenith_Engine.h"
#include "Flux/Flux_RendererImpl.h"
#include "Flux/Flux_GraphicsImpl.h"          // Get*Attachment / IsVelocityMRTActive / HDR_SCENE_FORMAT
#include "Flux/Flux_RenderTargets.h"         // Flux_RootSigBuilder / Flux_ComputePipelineBuilder
#include "Flux/Flux_RenderResources.h"       // Flux_RenderAttachmentBuilder (persistent history target)
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Flux/Flux_BackendTypes.h"          // FluxSwapchain().GetWidth/GetHeight
#include "DebugVariables/Zenith_DebugVariables.h"

// GPU constant-buffer layouts — mirror the *.slang structs byte-for-byte.
namespace
{
	// 4 uints (16B) + 8 floats (32B) = 48B (three 16-byte std140 rows).
	struct TAAResolveConstants
	{
		u_int m_uOutputWidth;
		u_int m_uOutputHeight;
		u_int m_uHistoryValid;                  // 0 => ignore history (first frame after a rebuild)
		u_int m_uPad0;
		float m_fBlendMinAlpha;
		float m_fBlendMaxAlpha;
		float m_fVelocityRejectionThreshold;
		float m_fHistoryClampStrength;
		float m_fDisocclusionDepthThreshold;
		u_int m_uRenderWidth;                   // TEMPORAL UPSCALING: render (source) dims — == output when off
		u_int m_uRenderHeight;
		float m_fUpscaleSigma;                  // Gaussian gather sigma (render pixels, ~0.47); unused at render==output
	};
	static_assert(sizeof(TAAResolveConstants) == 48,
		"TAAResolveConstants must be 48 bytes — render dims reuse the former pad slots; mirror TAAResolveConstantsLayout in Flux_TAA_Resolve.slang");

	struct TAACopyConstants
	{
		u_int m_uWidth;
		u_int m_uHeight;
		u_int m_uPad0;
		u_int m_uPad1;
	};
	static_assert(sizeof(TAACopyConstants) == 16,
		"TAACopyConstants must be 16 bytes — mirror TAACopyConstantsLayout in Flux_TAA_CopyToHistory.slang");

	struct TAASharpenConstants
	{
		u_int m_uWidth;
		u_int m_uHeight;
		float m_fSharpenAmount;
		float m_fPad0;
	};
	static_assert(sizeof(TAASharpenConstants) == 16,
		"TAASharpenConstants must be 16 bytes — mirror TAASharpenConstantsLayout in Flux_TAA_Sharpen.slang");

	constexpr u_int kuTAA_GROUP = 8u;   // 8x8 compute threadgroup (matches [numthreads(8,8,1)] in every TAA shader)
}

void Flux_TAAImpl::BuildHistoryTarget()
{
	// Idempotent: Destroy any prior attachment first so this doubles as the
	// rebuild-on-resize path (matches IBL's CreateRenderTargets / HiZ resize idiom).
	Flux_RenderAttachmentBuilder::Destroy(m_xHistory);

	Flux_RenderAttachmentBuilder xBuilder;
	xBuilder.m_uWidth       = g_xEngine.FluxSwapchain().GetWidth();
	xBuilder.m_uHeight      = g_xEngine.FluxSwapchain().GetHeight();
	xBuilder.m_eFormat      = HDR_SCENE_FORMAT;   // RGBA16F: rgb = resolved colour, a = linear view depth
	xBuilder.m_uMemoryFlags = (1u << MEMORY_FLAGS__UNORDERED_ACCESS) | (1u << MEMORY_FLAGS__SHADER_READ);
	xBuilder.BuildColour(m_xHistory, "TAA History");
}

void Flux_TAAImpl::BuildPipelines()
{
	m_xResolveShader.Initialise(Flux_TAAShaders::xTAA_Resolve);
	Flux_RootSigBuilder::FromReflection(m_xResolveRootSig, m_xResolveShader.GetReflection());
	Flux_ComputePipelineBuilder::BuildFromShader(m_xResolvePipeline, m_xResolveShader, m_xResolveRootSig);

	m_xCopyShader.Initialise(Flux_TAAShaders::xTAA_CopyToHistory);
	Flux_RootSigBuilder::FromReflection(m_xCopyRootSig, m_xCopyShader.GetReflection());
	Flux_ComputePipelineBuilder::BuildFromShader(m_xCopyPipeline, m_xCopyShader, m_xCopyRootSig);

	m_xSharpenShader.Initialise(Flux_TAAShaders::xTAA_Sharpen);
	Flux_RootSigBuilder::FromReflection(m_xSharpenRootSig, m_xSharpenShader.GetReflection());
	Flux_ComputePipelineBuilder::BuildFromShader(m_xSharpenPipeline, m_xSharpenShader, m_xSharpenRootSig);
}

void Flux_TAAImpl::Initialise()
{
	BuildHistoryTarget();
	BuildPipelines();
	m_bHistoryValid = false;

	// Recreate the persistent history at the new dims on resize + invalidate. Non-capturing
	// fn-ptr trampoline (re-enters via g_xEngine.TAA()), mirroring HiZ's resize callback.
	g_xEngine.FluxRenderer().AddResChangeCallback([]() { g_xEngine.TAA().OnResolutionChanged(); });

#ifdef ZENITH_DEBUG_VARIABLES
	g_xEngine.DebugVariables().AddFloat({ "Render", "TAA", "Blend Min Alpha" },        m_fBlendMinAlpha,              0.0f, 1.0f);
	g_xEngine.DebugVariables().AddFloat({ "Render", "TAA", "Blend Max Alpha" },        m_fBlendMaxAlpha,              0.0f, 1.0f);
	g_xEngine.DebugVariables().AddFloat({ "Render", "TAA", "Velocity Reject (px)" },   m_fVelocityRejectionThreshold, 1.0f, 128.0f);
	g_xEngine.DebugVariables().AddFloat({ "Render", "TAA", "History Clamp Strength" }, m_fHistoryClampStrength,       0.0f, 4.0f);
	g_xEngine.DebugVariables().AddFloat({ "Render", "TAA", "Disocclusion Threshold" }, m_fDisocclusionDepthThreshold, 0.0f, 1.0f);
	g_xEngine.DebugVariables().AddFloat({ "Render", "TAA", "Sharpen Amount" },         m_fSharpenAmount,              0.0f, 1.0f);
#endif

	m_bInitialised = true;
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_TAA initialised");
}

void Flux_TAAImpl::OnResolutionChanged()
{
	if (!m_bInitialised)
	{
		return;
	}
	BuildHistoryTarget();
	m_bHistoryValid = false;   // freshly-allocated history holds garbage — ignore it for one frame
}

void Flux_TAAImpl::Shutdown()
{
	if (!m_bInitialised)
	{
		return;
	}
	Flux_RenderAttachmentBuilder::Destroy(m_xHistory);
	m_pxGraph = nullptr;
	m_bResolveActiveThisBuild = false;
	m_bHistoryValid = false;
	m_bInitialised = false;
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_TAA shut down");
}

// Prepare (main thread, before any recording): snapshot the history validity this
// frame, then mark valid for next frame. Keeps the worker-thread record callback
// reading only an immutable snapshot (no cross-thread write to m_bHistoryValid).
static void PreTAAResolve(void* /*pUserData*/)
{
	Flux_TAAImpl& xTAA = g_xEngine.TAA();
	xTAA.m_bResolveHistoryValidThisFrame = xTAA.m_bHistoryValid;
	xTAA.m_bHistoryValid = true;
}

static void ExecuteTAAResolve(Flux_CommandBuffer* pxCmd, void* /*pUserData*/)
{
	Flux_TAAImpl& xTAA = g_xEngine.TAA();
	if (!xTAA.IsResolveActive())
	{
		return;
	}
	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();
	const u_int uW = g_xEngine.FluxSwapchain().GetWidth();
	const u_int uH = g_xEngine.FluxSwapchain().GetHeight();

	TAAResolveConstants xConsts = {};
	xConsts.m_uOutputWidth               = uW;
	xConsts.m_uOutputHeight              = uH;
	xConsts.m_uHistoryValid              = xTAA.m_bResolveHistoryValidThisFrame ? 1u : 0u;
	xConsts.m_fBlendMinAlpha             = xTAA.m_fBlendMinAlpha;
	xConsts.m_fBlendMaxAlpha             = xTAA.m_fBlendMaxAlpha;
	xConsts.m_fVelocityRejectionThreshold = xTAA.m_fVelocityRejectionThreshold;
	xConsts.m_fHistoryClampStrength      = xTAA.m_fHistoryClampStrength;
	xConsts.m_fDisocclusionDepthThreshold = xTAA.m_fDisocclusionDepthThreshold;
	// Temporal upscaling: the resolve DISPATCHES + writes at OUTPUT res (uW,uH) but SAMPLES the
	// render-res HDR/velocity/depth. GetRenderDims() == output when upscaling is off, so the shader
	// takes its byte-identical single-tap path; below output it runs the Gaussian gather.
	const Zenith_Maths::UVector2 xRenderDims = xGraphics.GetRenderDims();
	xConsts.m_uRenderWidth               = xRenderDims.x;
	xConsts.m_uRenderHeight              = xRenderDims.y;
	xConsts.m_fUpscaleSigma              = 0.47f;

	pxCmd->BindComputePipeline(&xTAA.m_xResolvePipeline);

	Flux_ShaderBinder xBinder(*pxCmd);
	// CLAMP sampler on the render-res sources: the upscale gather (and the 3x3 neighbourhood /
	// closest-depth dilation) step OUTSIDE [0,1] at screen edges; the default (null) sampler is
	// REPEAT, which would wrap the opposite edge's colour/depth/velocity into a ~1-render-pixel
	// border strip. Clamp-to-edge is the correct TAA edge behaviour. The single centre tap (the
	// scale-1 / non-upscaling path) is always in-bounds, so this leaves that path unchanged. History
	// is clamp-sampled too: its in-bounds guard (below) is centre-only AND inclusive, so a reprojected
	// UV within half a texel of an edge passes the guard yet the bilinear footprint would still wrap
	// the opposite edge in (into both the resolved rgb and the alpha depth that feeds disocclusion).
	xBinder.BindSRV          (xTAA.m_xResolveShader, "g_xHDRTex",       &xGraphics.GetHDRSceneTarget(kuFluxViewSlotMain).SRV(), &xGraphics.m_xClampSampler);
	xBinder.BindSRV          (xTAA.m_xResolveShader, "g_xVelocityTex",  &xGraphics.GetVelocityAttachment(kuFluxViewSlotMain).SRV(), &xGraphics.m_xClampSampler);
	xBinder.BindSRV          (xTAA.m_xResolveShader, "g_xDepthTex",      xGraphics.GetDepthStencilSRV(kuFluxViewSlotMain), &xGraphics.m_xClampSampler);
	xBinder.BindSRV          (xTAA.m_xResolveShader, "g_xHistoryTex",   &xTAA.m_xHistory.SRV(), &xGraphics.m_xClampSampler);
	xBinder.BindUAV_Texture  (xTAA.m_xResolveShader, "g_xOutputTex",    &xTAA.m_pxGraph->GetTransientAttachment(xTAA.m_xResolvedOutputHandle).UAV(0));
	xBinder.BindDrawConstants(xTAA.m_xResolveShader, "g_xTAAConstants", &xConsts, sizeof(xConsts));

	pxCmd->Dispatch((uW + kuTAA_GROUP - 1u) / kuTAA_GROUP, (uH + kuTAA_GROUP - 1u) / kuTAA_GROUP, 1u);
}

static void ExecuteTAACopyToHistory(Flux_CommandBuffer* pxCmd, void* /*pUserData*/)
{
	Flux_TAAImpl& xTAA = g_xEngine.TAA();
	if (!xTAA.IsResolveActive())
	{
		return;
	}
	const u_int uW = g_xEngine.FluxSwapchain().GetWidth();
	const u_int uH = g_xEngine.FluxSwapchain().GetHeight();

	TAACopyConstants xConsts = {};
	xConsts.m_uWidth  = uW;
	xConsts.m_uHeight = uH;

	pxCmd->BindComputePipeline(&xTAA.m_xCopyPipeline);

	Flux_ShaderBinder xBinder(*pxCmd);
	xBinder.BindSRV          (xTAA.m_xCopyShader, "g_xSourceTex",     &xTAA.m_pxGraph->GetTransientAttachment(xTAA.m_xResolvedOutputHandle).SRV());
	xBinder.BindUAV_Texture  (xTAA.m_xCopyShader, "g_xHistoryTex",    &xTAA.m_xHistory.UAV(0));
	xBinder.BindDrawConstants(xTAA.m_xCopyShader, "g_xCopyConstants", &xConsts, sizeof(xConsts));

	pxCmd->Dispatch((uW + kuTAA_GROUP - 1u) / kuTAA_GROUP, (uH + kuTAA_GROUP - 1u) / kuTAA_GROUP, 1u);
}

static void ExecuteTAASharpen(Flux_CommandBuffer* pxCmd, void* /*pUserData*/)
{
	Flux_TAAImpl& xTAA = g_xEngine.TAA();
	if (!xTAA.IsResolveActive())
	{
		return;
	}
	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();
	const u_int uW = g_xEngine.FluxSwapchain().GetWidth();
	const u_int uH = g_xEngine.FluxSwapchain().GetHeight();

	TAASharpenConstants xConsts = {};
	xConsts.m_uWidth        = uW;
	xConsts.m_uHeight       = uH;
	xConsts.m_fSharpenAmount = xTAA.m_fSharpenAmount;

	pxCmd->BindComputePipeline(&xTAA.m_xSharpenPipeline);

	Flux_ShaderBinder xBinder(*pxCmd);
	// CLAMP sampler: the RCAS 5-tap cross steps outside [0,1] at every screen-edge pixel with no
	// in-bounds guard, so the default (null) REPEAT sampler would wrap the opposite edge's colour
	// into the neighbour average AND the min/max clamp envelope. Clamp-to-edge matches the resolve.
	xBinder.BindSRV          (xTAA.m_xSharpenShader, "g_xSourceTex",        &xTAA.m_pxGraph->GetTransientAttachment(xTAA.m_xResolvedOutputHandle).SRV(), &xGraphics.m_xClampSampler);
	xBinder.BindUAV_Texture  (xTAA.m_xSharpenShader, "g_xOutputTex",        &xTAA.m_pxGraph->GetTransientAttachment(xTAA.m_xSharpenedOutputHandle).UAV(0));
	xBinder.BindDrawConstants(xTAA.m_xSharpenShader, "g_xSharpenConstants", &xConsts, sizeof(xConsts));

	pxCmd->Dispatch((uW + kuTAA_GROUP - 1u) / kuTAA_GROUP, (uH + kuTAA_GROUP - 1u) / kuTAA_GROUP, 1u);
}

void Flux_TAAImpl::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	m_pxGraph = &xGraph;
	m_bResolveActiveThisBuild = false;

	// MAIN view only, and only while the velocity latch is on (TAA active). When off,
	// no pass is declared and GetSceneColourForPostFX falls through to raw HDR
	// (byte-identical pre-TAA path). A rebuild means the graph transients changed, so
	// the history is stale relative to the new frame layout — invalidate it.
	if (!m_bInitialised || !g_xEngine.FluxGraphics().IsVelocityMRTActive())
	{
		return;
	}
	m_bHistoryValid = false;

	const u_int uW = g_xEngine.FluxSwapchain().GetWidth();
	const u_int uH = g_xEngine.FluxSwapchain().GetHeight();

	// Resolved output (rgb + linear depth in a) and sharpened output (the post-FX
	// source), both RGBA16F at swapchain dims. UAV|SHADER_READ so the compute passes
	// write them and the downstream reads sample them.
	Flux_TransientTextureDesc xDesc;
	xDesc.m_uWidth       = uW;
	xDesc.m_uHeight      = uH;
	xDesc.m_eFormat      = HDR_SCENE_FORMAT;
	xDesc.m_uMemoryFlags = (1u << MEMORY_FLAGS__UNORDERED_ACCESS) | (1u << MEMORY_FLAGS__SHADER_READ);
	m_xResolvedOutputHandle  = xGraph.CreateTransient(xDesc);
	m_xSharpenedOutputHandle = xGraph.CreateTransient(xDesc);

	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();

	// Resolve: current HDR + velocity + depth + previous-frame history -> resolved output.
	xGraph.AddPass("TAA Resolve", ExecuteTAAResolve)
		.View(kuFluxViewSlotMain)
		.Prepare(PreTAAResolve)
		.Reads(xGraphics.GetHDRSceneTarget(kuFluxViewSlotMain),   RESOURCE_ACCESS_READ_SRV)
		.Reads(xGraphics.GetVelocityAttachment(kuFluxViewSlotMain), RESOURCE_ACCESS_READ_SRV)
		.Reads(xGraphics.GetDepthAttachment(kuFluxViewSlotMain),  RESOURCE_ACCESS_READ_SRV)
		.Reads(m_xHistory,                                        RESOURCE_ACCESS_READ_SRV)
		.WritesTransient(m_xResolvedOutputHandle,                 RESOURCE_ACCESS_WRITE_UAV);

	// Persist the resolved output into the history for next frame (read-then-write WAR
	// on history orders this strictly after the resolve).
	xGraph.AddPass("TAA CopyToHistory", ExecuteTAACopyToHistory)
		.View(kuFluxViewSlotMain)
		.ReadsTransient(m_xResolvedOutputHandle, RESOURCE_ACCESS_READ_SRV)
		.Writes(m_xHistory,                      RESOURCE_ACCESS_WRITE_UAV);

	// Sharpen the resolved output -> the colour the HDR bloom/tonemap read.
	xGraph.AddPass("TAA Sharpen", ExecuteTAASharpen)
		.View(kuFluxViewSlotMain)
		.ReadsTransient(m_xResolvedOutputHandle,  RESOURCE_ACCESS_READ_SRV)
		.WritesTransient(m_xSharpenedOutputHandle, RESOURCE_ACCESS_WRITE_UAV);

	m_bResolveActiveThisBuild = true;
}

Flux_RenderAttachment& Flux_TAAImpl::GetResolvedOutput()
{
	Zenith_Assert(m_bResolveActiveThisBuild, "Flux_TAA::GetResolvedOutput: TAA resolve is not active this frame");
	Zenith_Assert(m_pxGraph, "Flux_TAA::GetResolvedOutput: graph pointer is null — call SetupRenderGraph first");
	// The sharpened output is the final post-TAA colour the post-FX chain consumes.
	return m_pxGraph->GetTransientAttachment(m_xSharpenedOutputHandle);
}

void Flux_TAAImpl::SetEnabled(bool bEnabled)
{
	// Force the velocity/TAA latch on/off (automation/tests). The next per-frame
	// UpdateVelocityTargetSelection applies it + triggers the graph rebuild.
	g_xEngine.FluxGraphics().SetTAAEnableOverride(bEnabled ? 1 : 0);
}

void Flux_TAAImpl::ClearEnabledOverride()
{
	// Restore normal control (debug var / --taa CLI).
	g_xEngine.FluxGraphics().SetTAAEnableOverride(-1);
}
