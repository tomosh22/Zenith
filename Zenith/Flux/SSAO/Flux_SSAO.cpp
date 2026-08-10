#include "Zenith.h"
#include "Flux/SSAO/Flux_SSAO_Shaders.h"
#include "Core/Zenith_Engine.h"

#include "Flux/SSAO/Flux_SSAOImpl.h"

#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_RendererImpl.h"
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

// SSAO render target formats. Raw/final stay at the historical R8 footprint;
// the separable intermediate uses R16 to avoid quantising between H and V.
static constexpr TextureFormat SSAO_FORMAT = TEXTURE_FORMAT_R8_UNORM;
static constexpr TextureFormat SSAO_INTERMEDIATE_FORMAT = TEXTURE_FORMAT_R16_UNORM;

static constexpr u_int SSAO_GENERATE_KERNEL_32 = 0u;
static constexpr u_int SSAO_GENERATE_KERNEL_16 = 1u;

// Structural/quality A/B controls. Separable + IGN are the optimised defaults.
// Quarter resolution and the 16-sample kernel remain opt-in until Phase-3
// visual sign-off.
//
// NOTE the non-separable toggle is a KERNEL-SHAPE A/B, not a rollback: the 2-D
// path also picked up point-clamp sampling, CB-supplied texel size and sky-tap
// rejection, none of which are reverted. Do not read it as "the old image".
DEBUGVAR bool dbg_bUseNonSeparableBlur   = false;
DEBUGVAR bool dbg_bQuarterResolution     = false;
DEBUGVAR bool dbg_bUse16SampleKernel     = false;
DEBUGVAR bool dbg_bUseLegacyRotationHash = false;

static struct SSAOGenerateConstants
{
	float m_fRadius = 0.5f;
	float m_fBias = 0.025f;
	float m_fIntensity = 1.5f;
	u_int m_uUseIGNRotation = 1u;
} dbg_xGenerateConstants;
static_assert(sizeof(SSAOGenerateConstants) == 16, "SSAOGenerateConstants must match the Slang constant-buffer layout");

static struct SSAOBlurConstants
{
	float m_fSpatialSigma = 1.5f;
	float m_fDepthSigma = 0.02f;
	float m_fNormalSigma = 0.2f;
	u_int m_uKernelRadius = 3;
	float m_fRcpTexelWidth = 0.0f;
	float m_fRcpTexelHeight = 0.0f;
	float m_fPad0 = 0.0f;
	float m_fPad1 = 0.0f;
} dbg_xBlurConstants;
static_assert(sizeof(SSAOBlurConstants) == 32, "SSAOBlurConstants must match the Slang constant-buffer layout");

// Attachment accessors — always resolve through the graph's transient slot.
// Now non-static members: read their own m_pxGraph / m_ax*Handles instead of
// reaching for g_xEngine.SSAO() (mirror HiZ GetHiZBuffer).
Flux_RenderAttachment& Flux_SSAOImpl::GetRawOcclusion(u_int uViewSlot)
{
	Zenith_Assert(m_pxGraph, "Flux_SSAOImpl::GetRawOcclusion: graph pointer is null (called before SetupRenderGraph or after Shutdown)");
	return m_pxGraph->GetTransientAttachment(m_axRawOcclusionHandles[uViewSlot]);
}
Flux_RenderAttachment& Flux_SSAOImpl::GetSeparableH(u_int uViewSlot)
{
	Zenith_Assert(m_pxGraph, "Flux_SSAOImpl::GetSeparableH: graph pointer is null (called before SetupRenderGraph or after Shutdown)");
	return m_pxGraph->GetTransientAttachment(m_axSeparableHHandles[uViewSlot]);
}
Flux_ShaderResourceView& Flux_SSAOImpl::GetOutputSRV(u_int uViewSlot)
{
	Zenith_Assert(m_pxGraph, "Flux_SSAOImpl::GetOutputSRV: graph pointer is null (called before SetupRenderGraph or after Shutdown)");
	return m_pxGraph->GetTransientAttachment(GetOutputHandle(uViewSlot)).SRV();
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
static const Flux_ShaderResourceView* DebugGetOutputSRV()
{
	Flux_SSAOImpl& xSSAO = g_xEngine.SSAO();
	if (xSSAO.m_pxGraph == nullptr) return nullptr;
	return &xSSAO.GetOutputSRV();
}
#endif

// ---- Init / Shutdown ----

void Flux_SSAOImpl::BuildPipelines()
{
	// One shader/reflection, two Vulkan-specialised kernel variants. The typed
	// generated handle is emitted when FluxCompiler regenerates SSAO.h alongside
	// the newly declared H/V artifacts.
	Flux_PipelineSpecification xGenerateSpec = Flux_PipelineHelper::CreateFullscreenSpec(
		m_xGenerateShader, Flux_SSAOShaders::xSSAO_Main, SSAO_FORMAT);
	static constexpr u_int auKernelSizes[2] = { 32u, 16u };
	for (u_int uVariant = 0; uVariant < 2u; uVariant++)
	{
		xGenerateSpec.m_xSpecConstants = Flux_SpecConstantTable{};
		xGenerateSpec.m_xSpecConstants.AddUInt(
			Flux_Generated_SSAO::SSAO_Main::hscFLUX_SC_SSAO_KERNEL_SIZE,
			auKernelSizes[uVariant]);
		Flux_PipelineBuilder::FromSpecification(m_axGeneratePipelines[uVariant], xGenerateSpec);
	}

	Flux_PipelineHelper::BuildFullscreenPipeline(
		m_xBlurShader, m_xBlurPipeline,
		Flux_SSAOShaders::xSSAO_Blur, SSAO_FORMAT);

	Flux_PipelineHelper::BuildFullscreenPipeline(
		m_xBlurHShader, m_xBlurHPipeline,
		Flux_SSAOShaders::xSSAO_BlurH, SSAO_INTERMEDIATE_FORMAT);

	Flux_PipelineHelper::BuildFullscreenPipeline(
		m_xBlurVShader, m_xBlurVPipeline,
		Flux_SSAOShaders::xSSAO_BlurV, SSAO_FORMAT);

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
	g_xEngine.DebugVariables().AddBoolean({ "Render", "SSAO", "Quality", "Use Non-Separable Blur" }, dbg_bUseNonSeparableBlur);
	g_xEngine.DebugVariables().AddBoolean({ "Render", "SSAO", "Quality", "Quarter Resolution" }, dbg_bQuarterResolution);
	g_xEngine.DebugVariables().AddBoolean({ "Render", "SSAO", "Quality", "Use 16 Sample Kernel" }, dbg_bUse16SampleKernel);
	g_xEngine.DebugVariables().AddBoolean({ "Render", "SSAO", "Quality", "Use Legacy Rotation Hash" }, dbg_bUseLegacyRotationHash);

	g_xEngine.DebugVariables().AddFloat({ "Render", "SSAO", "Blur", "Spatial Sigma" }, dbg_xBlurConstants.m_fSpatialSigma, 0.5f, 4.f);
	g_xEngine.DebugVariables().AddFloat({ "Render", "SSAO", "Blur", "Depth Sigma" }, dbg_xBlurConstants.m_fDepthSigma, 0.005f, 0.1f);
	g_xEngine.DebugVariables().AddFloat({ "Render", "SSAO", "Blur", "Normal Sigma" }, dbg_xBlurConstants.m_fNormalSigma, 0.1f, 1.f);
	g_xEngine.DebugVariables().AddUInt32({ "Render", "SSAO", "Blur", "Kernel Radius" }, dbg_xBlurConstants.m_uKernelRadius, 1, 5);

	// Transient-SRV previews: Output resolves through the committed selector, so
	// it stays descriptor-safe during the one frame between a toggle and rebuild.
	g_xEngine.DebugVariables().AddTextureCallback({ "Render", "SSAO", "Textures", "Raw" },     &DebugGetRawOcclusionSRV);
	g_xEngine.DebugVariables().AddTextureCallback({ "Render", "SSAO", "Textures", "Output" },  &DebugGetOutputSRV);
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

	// The pass's declared view slot selects this view's G-buffer/depth inputs.
	const u_int uViewSlot = Flux_RenderGraph::GetCurrentRecordingPassViewSlot();

	const u_int uPipelineVariant = dbg_bUse16SampleKernel
		? SSAO_GENERATE_KERNEL_16
		: SSAO_GENERATE_KERNEL_32;
	xGraphics.BindFullscreenQuad(*pxCommandList, xSSAO.m_axGeneratePipelines[uPipelineVariant]);

	namespace NS = Flux_Generated_SSAO::SSAO_Main;
	Flux_ShaderBinder xBinder(*pxCommandList);
	// Main/preview passes may record concurrently. Keep the debug-var backing
	// store immutable and patch the hash selection into a callback-local copy.
	SSAOGenerateConstants xConstants = dbg_xGenerateConstants;
	xConstants.m_uUseIGNRotation = dbg_bUseLegacyRotationHash ? 0u : 1u;
	xBinder.BindDrawConstants(NS::hSSAOConstants, &xConstants, sizeof(SSAOGenerateConstants));
	// Depth and normals are data textures: exact texel reads avoid the default
	// anisotropic-linear/repeat sampler blending unrelated geometry or wrapping
	// the screen edge into the opposite side.
	xBinder.BindSRV(NS::hg_xDepthTex, xGraphics.GetDepthStencilSRV(uViewSlot), &xGraphics.m_xPointSampler);
	xBinder.BindSRV(NS::hg_xNormalTex, xGraphics.GetGBufferSRV(MRT_INDEX_NORMALSAMBIENT, uViewSlot), &xGraphics.m_xPointSampler);

	pxCommandList->DrawIndexed(6);
}

// Pure half of the blur-constant fill: base tunables + source dimensions in,
// the CB the shader reads out. Split from the attachment lookup so the
// reciprocal-texel math is unit-testable without a graph or a live target
// (Flux_SSAO.Tests.inl pins it).
static SSAOBlurConstants MakeSSAOBlurConstants(const SSAOBlurConstants& xBase, u_int uWidth, u_int uHeight)
{
	SSAOBlurConstants xConstants = xBase;
	xConstants.m_fRcpTexelWidth = 1.0f / static_cast<float>(uWidth);
	xConstants.m_fRcpTexelHeight = 1.0f / static_cast<float>(uHeight);
	return xConstants;
}

static SSAOBlurConstants BuildSSAOBlurConstants(const Flux_RenderAttachment& xOcclusion, const char* szPassName)
{
	const u_int uOcclusionWidth = xOcclusion.m_xSurfaceInfo.m_uWidth;
	const u_int uOcclusionHeight = xOcclusion.m_xSurfaceInfo.m_uHeight;
	Zenith_Assert(uOcclusionWidth > 0 && uOcclusionHeight > 0,
		"%s: occlusion target has invalid dimensions %ux%u", szPassName, uOcclusionWidth, uOcclusionHeight);

	return MakeSSAOBlurConstants(dbg_xBlurConstants, uOcclusionWidth, uOcclusionHeight);
}

static void ExecuteSSAOBlurLegacy(Flux_CommandBuffer* pxCommandList, void*)
{
	// Blur/algorithm toggles are represented by committed graph pass-enables.
	// Reading the live toggles here would diverge for one frame while the full
	// graph rebuild requested by ApplySelectionToGraph is pending.
	if (!Zenith_GraphicsOptions::Get().m_bSSAOEnabled)
		return;

	Flux_SSAOImpl& xSSAO = g_xEngine.SSAO();
	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();
	const u_int uViewSlot = Flux_RenderGraph::GetCurrentRecordingPassViewSlot();

	xGraphics.BindFullscreenQuad(*pxCommandList, xSSAO.m_xBlurPipeline);
	namespace NS = Flux_Generated_SSAO::SSAO_Blur;
	Flux_ShaderBinder xBinder(*pxCommandList);

	Flux_RenderAttachment& xRawOcclusion = xSSAO.GetRawOcclusion(uViewSlot);
	const SSAOBlurConstants xConstants = BuildSSAOBlurConstants(xRawOcclusion, "ExecuteSSAOBlurLegacy");
	xBinder.BindDrawConstants(NS::hSSAOBlurConstants, &xConstants, sizeof(SSAOBlurConstants));
	xBinder.BindSRV(NS::hg_xOcclusionTex, &xRawOcclusion.SRV(), &xGraphics.m_xPointSampler);
	xBinder.BindSRV(NS::hg_xDepthTex, xGraphics.GetDepthStencilSRV(uViewSlot), &xGraphics.m_xPointSampler);
	xBinder.BindSRV(NS::hg_xNormalTex, xGraphics.GetGBufferSRV(MRT_INDEX_NORMALSAMBIENT, uViewSlot), &xGraphics.m_xPointSampler);

	pxCommandList->DrawIndexed(6);
}

static void ExecuteSSAOBlurH(Flux_CommandBuffer* pxCommandList, void*)
{
	if (!Zenith_GraphicsOptions::Get().m_bSSAOEnabled)
		return;

	Flux_SSAOImpl& xSSAO = g_xEngine.SSAO();
	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();
	const u_int uViewSlot = Flux_RenderGraph::GetCurrentRecordingPassViewSlot();

	xGraphics.BindFullscreenQuad(*pxCommandList, xSSAO.m_xBlurHPipeline);
	namespace NS = Flux_Generated_SSAO::SSAO_BlurH;
	Flux_ShaderBinder xBinder(*pxCommandList);

	Flux_RenderAttachment& xRawOcclusion = xSSAO.GetRawOcclusion(uViewSlot);
	const SSAOBlurConstants xConstants = BuildSSAOBlurConstants(xRawOcclusion, "ExecuteSSAOBlurH");
	xBinder.BindDrawConstants(NS::hSSAOBlurConstants, &xConstants, sizeof(SSAOBlurConstants));
	xBinder.BindSRV(NS::hg_xOcclusionTex, &xRawOcclusion.SRV(), &xGraphics.m_xPointSampler);
	xBinder.BindSRV(NS::hg_xDepthTex, xGraphics.GetDepthStencilSRV(uViewSlot), &xGraphics.m_xPointSampler);
	xBinder.BindSRV(NS::hg_xNormalTex, xGraphics.GetGBufferSRV(MRT_INDEX_NORMALSAMBIENT, uViewSlot), &xGraphics.m_xPointSampler);

	pxCommandList->DrawIndexed(6);
}

static void ExecuteSSAOBlurV(Flux_CommandBuffer* pxCommandList, void*)
{
	if (!Zenith_GraphicsOptions::Get().m_bSSAOEnabled)
		return;

	Flux_SSAOImpl& xSSAO = g_xEngine.SSAO();
	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();
	const u_int uViewSlot = Flux_RenderGraph::GetCurrentRecordingPassViewSlot();

	xGraphics.BindFullscreenQuad(*pxCommandList, xSSAO.m_xBlurVPipeline);
	namespace NS = Flux_Generated_SSAO::SSAO_BlurV;
	Flux_ShaderBinder xBinder(*pxCommandList);

	Flux_RenderAttachment& xHorizontal = xSSAO.GetSeparableH(uViewSlot);
	const SSAOBlurConstants xConstants = BuildSSAOBlurConstants(xHorizontal, "ExecuteSSAOBlurV");
	xBinder.BindDrawConstants(NS::hSSAOBlurConstants, &xConstants, sizeof(SSAOBlurConstants));
	xBinder.BindSRV(NS::hg_xOcclusionTex, &xHorizontal.SRV(), &xGraphics.m_xPointSampler);
	xBinder.BindSRV(NS::hg_xDepthTex, xGraphics.GetDepthStencilSRV(uViewSlot), &xGraphics.m_xPointSampler);
	xBinder.BindSRV(NS::hg_xNormalTex, xGraphics.GetGBufferSRV(MRT_INDEX_NORMALSAMBIENT, uViewSlot), &xGraphics.m_xPointSampler);

	pxCommandList->DrawIndexed(6);
}

// ---- Render graph setup / committed output selection ----

static Flux_SSAOSelection GetLiveSSAOSelection()
{
	return Flux_SSAOSelection{
		Zenith_GraphicsOptions::Get().m_bSSAOBlurEnabled,
		!dbg_bUseNonSeparableBlur,
		dbg_bQuarterResolution ? 4u : 2u
	};
}

void Flux_SSAOImpl::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	m_pxGraph = &xGraph;
	// Snapshot graph-shaping debug/options once so main and preview cannot
	// commit different algorithms or dimensions if a UI toggle lands mid-setup.
	const Flux_SSAOSelection xSelection = GetLiveSSAOSelection();

	// Main view at swapchain dims (byte-equivalent to the historical
	// single-view path), then the preview view at its fixed 512² dims — only
	// while active, so its transients exist exactly when its passes do (the
	// graph's unused-transient validation demands this).
	SetupViewPasses(
		xGraph,
		kuFluxViewSlotMain,
		g_xEngine.FluxGraphics().GetRenderWidth(),
		g_xEngine.FluxGraphics().GetRenderHeight(),
		xSelection);
	if (g_xEngine.FluxGraphics().RenderViews().IsViewActive(kuFluxViewSlotPreview))
		SetupViewPasses(
			xGraph,
			kuFluxViewSlotPreview,
			kuFLUX_PREVIEW_VIEW_SIZE,
			kuFLUX_PREVIEW_VIEW_SIZE,
			xSelection);
}

void Flux_SSAOImpl::ApplySelectionToGraph(Flux_RenderGraph& /*xGraph*/)
{
	// Blur enabled, algorithm, and divisor are view-independent. Every active
	// slot commits the same composite selection, so the main selector covers all
	// views. A full rebuild is required because DeferredShading captures one
	// specific transient handle in its graph Read declaration and divisor changes
	// recreate all SSAO transients at different dimensions.
	if (!m_axOutputSelectors[kuFluxViewSlotMain].RequestRebuildIfSelectionChanged(GetLiveSSAOSelection()))
		return;

	g_xEngine.FluxRenderer().RequestGraphRebuild();
}

void Flux_SSAOImpl::SetupViewPasses(
	Flux_RenderGraph& xGraph,
	u_int uViewSlot,
	u_int uWidth,
	u_int uHeight,
	const Flux_SSAOSelection& xSelection)
{
	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();

	const u_int uSSAOWidth  = std::max(1u, uWidth  / xSelection.m_uResolutionDivisor);
	const u_int uSSAOHeight = std::max(1u, uHeight / xSelection.m_uResolutionDivisor);

	Flux_TransientTextureDesc xSSAODesc;
	xSSAODesc.m_uWidth       = uSSAOWidth;
	xSSAODesc.m_uHeight      = uSSAOHeight;
	xSSAODesc.m_eFormat      = SSAO_FORMAT;
	xSSAODesc.m_uMemoryFlags = (1u << MEMORY_FLAGS__SHADER_READ);
	m_axRawOcclusionHandles[uViewSlot]  = xGraph.CreateTransient(xSSAODesc);
	m_axLegacyBlurredHandles[uViewSlot] = xGraph.CreateTransient(xSSAODesc);
	m_axBlurredHandles[uViewSlot]       = xGraph.CreateTransient(xSSAODesc);

	Flux_TransientTextureDesc xIntermediateDesc = xSSAODesc;
	xIntermediateDesc.m_eFormat = SSAO_INTERMEDIATE_FORMAT;
	m_axSeparableHHandles[uViewSlot] = xGraph.CreateTransient(xIntermediateDesc);

	// Pass names must be per-view unique + static-lifetime (duplicate names
	// are a hard assert). View 0 keeps the historical names (profiling /
	// FindPass stability).
	const bool bMainView = (uViewSlot == kuFluxViewSlotMain);

	xGraph.AddPass(bMainView ? "SSAO Generate" : "SSAO Generate (Preview)", ExecuteSSAOGenerate)
		.View(uViewSlot)
		.ClearTargets()
		.Reads         (xGraphics.GetDepthAttachment(uViewSlot),                         RESOURCE_ACCESS_READ_SRV)
		.Reads         (xGraphics.GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT, uViewSlot), RESOURCE_ACCESS_READ_SRV)
		.WritesTransient(m_axRawOcclusionHandles[uViewSlot],                             RESOURCE_ACCESS_WRITE_RTV);

	const Flux_PassHandle xLegacyBlurPass = xGraph.AddPass(
		bMainView ? "SSAO Blur Legacy" : "SSAO Blur Legacy (Preview)", ExecuteSSAOBlurLegacy)
		.View(uViewSlot)
		.ClearTargets()
		.Reads         (xGraphics.GetDepthAttachment(uViewSlot),                         RESOURCE_ACCESS_READ_SRV)
		.Reads         (xGraphics.GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT, uViewSlot), RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient (m_axRawOcclusionHandles[uViewSlot],                             RESOURCE_ACCESS_READ_SRV)
		.WritesTransient(m_axLegacyBlurredHandles[uViewSlot],                            RESOURCE_ACCESS_WRITE_RTV);

	const Flux_PassHandle xBlurHPass = xGraph.AddPass(
		bMainView ? "SSAO Blur H" : "SSAO Blur H (Preview)", ExecuteSSAOBlurH)
		.View(uViewSlot)
		.ClearTargets()
		.Reads         (xGraphics.GetDepthAttachment(uViewSlot),                         RESOURCE_ACCESS_READ_SRV)
		.Reads         (xGraphics.GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT, uViewSlot), RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient (m_axRawOcclusionHandles[uViewSlot],                             RESOURCE_ACCESS_READ_SRV)
		.WritesTransient(m_axSeparableHHandles[uViewSlot],                               RESOURCE_ACCESS_WRITE_RTV);

	// Keep the historical "SSAO Blur" profiler label on the final/default pass.
	const Flux_PassHandle xBlurVPass = xGraph.AddPass(
		bMainView ? "SSAO Blur" : "SSAO Blur (Preview)", ExecuteSSAOBlurV)
		.View(uViewSlot)
		.ClearTargets()
		.Reads         (xGraphics.GetDepthAttachment(uViewSlot),                         RESOURCE_ACCESS_READ_SRV)
		.Reads         (xGraphics.GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT, uViewSlot), RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient (m_axSeparableHHandles[uViewSlot],                               RESOURCE_ACCESS_READ_SRV)
		.WritesTransient(m_axBlurredHandles[uViewSlot],                                  RESOURCE_ACCESS_WRITE_RTV);

	const bool bUseLegacyBlur = xSelection.m_bBlurEnabled && !xSelection.m_bSeparableBlur;
	const bool bUseSeparableBlur = xSelection.m_bBlurEnabled && xSelection.m_bSeparableBlur;
	xGraph.SetEnabled(xLegacyBlurPass, bUseLegacyBlur);
	xGraph.SetEnabled(xBlurHPass, bUseSeparableBlur);
	xGraph.SetEnabled(xBlurVPass, bUseSeparableBlur);

	const Flux_TransientHandle xFilteredHandle = xSelection.m_bSeparableBlur
		? m_axBlurredHandles[uViewSlot]
		: m_axLegacyBlurredHandles[uViewSlot];
	m_axOutputSelectors[uViewSlot].Commit(
		xFilteredHandle,
		m_axRawOcclusionHandles[uViewSlot],
		xSelection.m_bBlurEnabled,
		xSelection);

	// No upsample/composite pass: DeferredShading declares and binds exactly the
	// committed output handle. Its linear-clamp sample expands half/quarter-res
	// AO into the ambient term; the selector keeps that declaration and binding
	// coherent across runtime changes.
}

// Hosted here because this TU is always linked (the feature registry takes
// Flux_SSAOImpl's method pointers), so the static-init test registrations
// survive MSVC dead-stripping — and the file-static blur-constant helpers above
// are in scope.
#include "Flux/SSAO/Flux_SSAO.Tests.inl"
