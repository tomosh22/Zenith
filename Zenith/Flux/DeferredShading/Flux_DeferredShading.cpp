#include "Zenith.h"
#include "Flux/DeferredShading/Flux_DeferredShading_Shaders.h"

#include "Flux/Flux_RendererImpl.h"
#include "Flux/DeferredShading/Flux_DeferredShadingImpl.h"
#include "Core/Zenith_Engine.h"

#include "Flux/Flux_RenderTargets.h"
#include "Flux/HDR/Flux_HDRImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Shadows/Flux_ShadowsImpl.h"
#include "Flux/IBL/Flux_IBLImpl.h"
#include "Flux/IBL/Flux_IBLImpl.h"
#include "Flux/SSR/Flux_SSRImpl.h"
#include "Flux/SSGI/Flux_SSGIImpl.h"
#include "Flux/SSAO/Flux_SSAOImpl.h"
#include "Flux/DynamicLights/Flux_DynamicLightsImpl.h"
#include "Flux/DynamicLights/Flux_LightClusteringImpl.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Flux/Shaders/Generated/DeferredShading.h" // typed binding handles

// Phase 7b: state on Flux_DeferredShadingImpl held by Zenith_Engine.

DEBUGVAR bool dbg_bVisualiseCSMs = false;
// Deferred debug views. Mirrors the g_uDebugMode branches in
// Shaders/DeferredShading/Flux_DeferredShading.slang — keep the two in step, and
// keep uDS_DEBUG_MODE_MAX equal to the highest mode the shader implements or the
// slider silently stops short of the newest view (which is what it used to do).
// 0=normal 1=cyan 2=depth 3=albedo 4=metallic 5=roughness 6=AO 7=normal 8=NdotL
// 9=shadowFactor 10=shadingModel 11=IBLambient
static constexpr u_int uDS_DEBUG_MODE_MAX = 11;
DEBUGVAR u_int dbg_uDeferredShadingDebugMode = 0;
DEBUGVAR float dbg_fAmbientFallbackIntensity = 0.03f;  // Ambient when IBL disabled (0.01-0.1 typical)

void Flux_DeferredShadingImpl::BuildPipelines()
{
	m_xShader.Initialise(Flux_DeferredShadingShaders::xDeferredShading);

	Flux_PipelineSpecification xPipelineSpec;
	xPipelineSpec.m_aeColourAttachmentFormats[0] = HDR_SCENE_FORMAT;
	xPipelineSpec.m_uNumColourAttachments = 1;
	xPipelineSpec.m_pxShader = &m_xShader;
	// No vertex input: this is a vertex-pulling / fullscreen program whose VS reads
	// no attributes. m_pxVertexLayout stays null, which is the canonical spelling the
	// validation tripwire matches against an empty reflection table.
	xPipelineSpec.m_eTopology = MESH_TOPOLOGY_NONE;

	m_xShader.GetReflection().PopulateLayout(xPipelineSpec.m_xPipelineLayout);

	xPipelineSpec.m_axBlendStates[0].m_eSrcBlendFactor = BLEND_FACTOR_ONE;
	xPipelineSpec.m_axBlendStates[0].m_eDstBlendFactor = BLEND_FACTOR_ONE;
	xPipelineSpec.m_axBlendStates[0].m_bBlendEnabled = false;

	xPipelineSpec.m_bDepthTestEnabled = false;
	xPipelineSpec.m_bDepthWriteEnabled = false;

	// One pipeline per view-shading mode (Stage 3a/3b). Bake the per-mode
	// view-shading permission mask (Stage 3b spec constants): FULL permits shadows +
	// cluster lights (identical to pre-3b since the constants default true), BASIC
	// bakes both FALSE so the compiler strips the CSM/cluster clauses for
	// cascade-derived + material-preview views. Resolved by NAME via reflection in
	// the backend — C++ never hardcodes an ID.
	namespace DSGen = Flux_Generated_DeferredShading::DeferredShading;
	for (u_int uMode = 0; uMode < FLUX_VIEW_SHADING_MODE_COUNT; uMode++)
	{
		const bool bPermitLit = (uMode == FLUX_VIEW_SHADING_MODE_FULL);
		xPipelineSpec.m_xSpecConstants = Flux_SpecConstantTable{};
		xPipelineSpec.m_xSpecConstants.AddBool(DSGen::hscFLUX_SC_VIEW_SHADOWS_PERMITTED,        bPermitLit);
		xPipelineSpec.m_xSpecConstants.AddBool(DSGen::hscFLUX_SC_VIEW_CLUSTER_LIGHTS_PERMITTED, bPermitLit);
		Flux_PipelineBuilder::FromSpecification(m_axPipelines[uMode], xPipelineSpec);
	}
}

void Flux_DeferredShadingImpl::Initialise()
{
	BuildPipelines();

	#ifdef ZENITH_DEBUG_VARIABLES
	g_xEngine.DebugVariables().AddBoolean({ "Render", "Shadows", "Visualise CSMs" }, dbg_bVisualiseCSMs);
	g_xEngine.DebugVariables().AddUInt32({ "Render", "DeferredShading", "DebugMode" }, dbg_uDeferredShadingDebugMode, 0, uDS_DEBUG_MODE_MAX);
	g_xEngine.DebugVariables().AddFloat({ "Render", "DeferredShading", "AmbientFallback" }, dbg_fAmbientFallbackIntensity, 0.0f, 0.2f);
	#endif

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_DeferredShading initialised");
}

void Flux_DeferredShadingImpl::Shutdown()
{
	// Pipeline references its shader, so destroy pipelines first.
	for (u_int uMode = 0; uMode < FLUX_VIEW_SHADING_MODE_COUNT; uMode++)
	{
		m_axPipelines[uMode].Reset();
	}
	m_xShader.Reset();
}

static void ExecuteApplyLighting(Flux_CommandBuffer* pxCommandList, void*)
{
	Flux_DeferredShadingImpl& xDS = g_xEngine.DeferredShading();
	Flux_GraphicsImpl& xFluxGraphics = g_xEngine.FluxGraphics();
	Flux_ShadowsImpl& xShadows = g_xEngine.Shadows();
	Flux_IBLImpl& xIBL = g_xEngine.IBL();
	Flux_SSRImpl& xSSR = g_xEngine.SSR();
	Flux_SSGIImpl& xSSGI = g_xEngine.SSGI();
	Flux_SSAOImpl& xSSAO = g_xEngine.SSAO();

	// The SAME callback lights every full-pipeline view: the recording pass's
	// declared view slot selects that view's G-buffer (and its VIEW set was bound
	// by SetPipeline). S5b: every full-pipeline view owns its own SSAO/SSR/SSGI
	// chains too, so the screen-space binds below are per-view as well.
	const u_int uViewSlot = Flux_RenderGraph::GetCurrentRecordingPassViewSlot();

	// Select the view-shading pipeline variant (Stage 3a/3b): FULL for the main
	// view, BASIC for the material preview. Reads the per-view CONSTANTS flag word
	// (m_xConstants.m_uViewFlags) — the SAME field the shader branches on via
	// g_xView.g_uViewFlags — so the picked variant's baked permission mask is always
	// consistent with the runtime flag the shader reads (single source of truth).
	const FluxViewShadingMode eShadingMode =
		Flux_ViewShadingModeFromFlags(xFluxGraphics.RenderViews().View(uViewSlot).m_xConstants.m_uViewFlags);
	pxCommandList->SetPipeline(&xDS.m_axPipelines[eShadingMode]);

	pxCommandList->SetIndexBuffer(xFluxGraphics.m_xQuadMesh.GetIndexBuffer());

	// Use named bindings via shader binder (auto-manages descriptor set switches)
	Flux_ShaderBinder xBinder(*pxCommandList);

	namespace DS = Flux_Generated_DeferredShading::DeferredShading;

	// Bind G-buffer textures (named bindings)
	xBinder.BindSRV(DS::hg_xDiffuseTex, xFluxGraphics.GetGBufferSRV(MRT_INDEX_DIFFUSE, uViewSlot));
	xBinder.BindSRV(DS::hg_xNormalsAmbientTex, xFluxGraphics.GetGBufferSRV(MRT_INDEX_NORMALSAMBIENT, uViewSlot));
	xBinder.BindSRV(DS::hg_xMaterialTex, xFluxGraphics.GetGBufferSRV(MRT_INDEX_MATERIAL, uViewSlot));
	xBinder.BindSRV(DS::hg_xGBufferEmissiveTex, xFluxGraphics.GetGBufferSRV(MRT_INDEX_EMISSIVE, uViewSlot));
	// Point-sampled: the contact-shadow march compares reconstructed view Z against
	// this depth, and a bilinear tap across a silhouette invents an intermediate
	// depth that reads as a false occluder (contact-shadow fringes on every edge).
	xBinder.BindSRV(DS::hg_xDepthTex, xFluxGraphics.GetDepthStencilSRV(uViewSlot), &xFluxGraphics.m_xPointSampler);

	// CSM (g_xCSM) AND the all-cascade ShadowMatrices SSBO (g_xShadowMatrices) are now in
	// the persistent VIEW set (Phase 5.4) — written once/frame in PreparePersistentSets /
	// WritePersistentView*; no per-pass bind. The CSM graph Read() below stays.

	// Shadow sampling parameters (per-cascade splits / texel sizes / depth ranges
	// + global filtering config). Packed + uploaded by Flux_Shadows; bound here as
	// a regular CBV (mirrors ShadowSamplingLayout in Flux_DeferredShading.slang).
	xBinder.BindCBV(DS::hShadowSampling, &xShadows.GetShadowSamplingBuffer().GetCBV());

	// (IBL textures — BRDF LUT + irradiance/prefiltered cubes — are in the persistent VIEW
	// set now, Phase 5.4; read via g_xViewSet. The graph Reads stay in SetupRenderGraph.)

	// Always bind SSR texture if initialised (shader checks g_bSSREnabled before sampling)
	// This avoids Vulkan validation errors for unbound descriptors. S5b: every
	// full-pipeline view owns its own reflection chain, so the bind resolves
	// through the recording view's committed handle.
	if (xSSR.IsInitialised())
	{
		xBinder.BindSRV(DS::hg_xSSRTex, &xSSR.GetReflectionSRV(uViewSlot));
	}
	else
	{
		// Fallback: placeholder to satisfy descriptor validation
		xBinder.BindSRV(DS::hg_xSSRTex, xFluxGraphics.GetGBufferSRV(MRT_INDEX_DIFFUSE, uViewSlot));
	}

	// Always bind SSGI texture if initialised (shader checks g_bSSGIEnabled before sampling)
	if (xSSGI.IsInitialised())
	{
		xBinder.BindSRV(DS::hg_xSSGITex, &xSSGI.GetSSGISRV(uViewSlot));
	}
	else
	{
		// Fallback: placeholder to satisfy descriptor validation
		xBinder.BindSRV(DS::hg_xSSGITex, xFluxGraphics.GetGBufferSRV(MRT_INDEX_DIFFUSE, uViewSlot));
	}

	// SSAO modulates the ambient term inside the shader (g_bSSAOEnabled gates the
	// sample). Bind the graph-committed output, never the live blur/debug toggles:
	// the latter can diverge for one frame while their requested rebuild lands.
	// Linear-clamp filtering performs the direct half/quarter-res expansion and
	// prevents the opposite screen edge wrapping into the sample footprint.
	xBinder.BindSRV(DS::hg_xSSAOTex, &xSSAO.GetOutputSRV(uViewSlot), &xFluxGraphics.m_xClampSampler);

	// (Clustered dynamic lights — LightBuffer + cluster counts/indices — are in the
	// persistent VIEW set now (Phase 5.4); read via g_xViewSet, no per-pass bind. The
	// cluster ReadBuffer decls in SetupRenderGraph drive the UAV→SRV barrier.)

	// Pass constants to shader
	struct DeferredShadingConstants
	{
		u_int m_bVisualiseCSMs;
		u_int m_bIBLEnabled;
		u_int m_uDebugMode;  // 0=normal, 1=cyan (verify running), 2=depth, 3=diffuse
		u_int m_bIBLDiffuseEnabled;
		u_int m_bIBLSpecularEnabled;
		u_int m_bShowBRDFLUT;
		u_int m_bForceRoughness;
		float m_fForcedRoughness;
		u_int m_bSSREnabled;
		u_int m_bSSGIEnabled;
		float m_fAmbientFallbackIntensity;  // Configurable ambient when IBL disabled
		u_int m_bSSAOEnabled;               // gate the ambient-only SSAO sample
	};
	DeferredShadingConstants xConstants;
	xConstants.m_bVisualiseCSMs = dbg_bVisualiseCSMs ? 1u : 0u;
	// Only enable IBL if both enabled AND ready (textures have been generated)
	xConstants.m_bIBLEnabled = (xIBL.IsEnabled() && xIBL.IsReady()) ? 1 : 0;
	xConstants.m_uDebugMode = dbg_uDeferredShadingDebugMode;
#ifdef ZENITH_WINDOWS
	// Diagnostic CLI override: --ds-debug=N forces the deferred debug view
	// (0=normal 2=depth 3=albedo 4=metallic 5=roughness 6=AO 7=normal 8=NdotL
	// 9=shadowFactor 10=shadingModel 11=IBLambient). Lets a capture harness dump
	// any G-buffer channel without a debug-panel toggle.
	{
		// Magic-static (thread-safe one-time init): this callback now records for
		// BOTH the main and preview lighting passes, potentially on different
		// workers concurrently — a hand-rolled check-then-write static here would
		// be a data race.
		static const int s_iDsDebugOverride = []
		{
			for (int i = 1; i < __argc; i++)
				if (std::strncmp(__argv[i], "--ds-debug=", 11) == 0)
					return std::atoi(__argv[i] + 11);
			return -1;
		}();
		if (s_iDsDebugOverride >= 0) xConstants.m_uDebugMode = (u_int)s_iDsDebugOverride;
	}
#endif
	xConstants.m_bIBLDiffuseEnabled = xIBL.IsDiffuseEnabled() ? 1 : 0;
	xConstants.m_bIBLSpecularEnabled = xIBL.IsSpecularEnabled() ? 1 : 0;
	xConstants.m_bShowBRDFLUT = xIBL.IsShowBRDFLUT() ? 1 : 0;
	xConstants.m_bForceRoughness = xIBL.IsForceRoughness() ? 1 : 0;
	xConstants.m_fForcedRoughness = xIBL.GetForcedRoughness();
	// Screen-space effects are per-view since S5b (each full-pipeline view owns
	// its own chains), so the gates are view-independent again.
	xConstants.m_bSSREnabled = xSSR.IsEnabled() ? 1 : 0;
	xConstants.m_bSSGIEnabled = xSSGI.IsEnabled() ? 1 : 0;
	xConstants.m_fAmbientFallbackIntensity = dbg_fAmbientFallbackIntensity;
	xConstants.m_bSSAOEnabled = Zenith_GraphicsOptions::Get().m_bSSAOEnabled ? 1 : 0;

	xBinder.BindDrawConstants(DS::hDeferredShadingConstants, &xConstants, sizeof(xConstants));

	pxCommandList->DrawIndexed(6);
}

void Flux_DeferredShadingImpl::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	// First writer of the HDR scene target — clear overwrites every pixel.
	// Capture the handle via the implicit conversion; builder temporary dies
	// at the semicolon. All loop/conditional declarations below go through
	// the graph's Read/ReadTransient helpers with the captured handle.
	const Flux_PassHandle xPass = xGraph.AddPass("Apply Lighting", ExecuteApplyLighting)
		.Writes(g_xEngine.FluxGraphics().GetHDRSceneTarget(), RESOURCE_ACCESS_WRITE_RTV)
		.ClearTargets();

	Flux_GraphicsImpl& xFluxGraphics = g_xEngine.FluxGraphics();

	for (u_int u = 0; u < uFLUX_MRT_CORE_COUNT; u++)   // lighting reads the 4 core G-buffer MRTs, never velocity
		xGraph.Read(xPass, xFluxGraphics.GetMRTAttachment(static_cast<MRTIndex>(u)), RESOURCE_ACCESS_READ_SRV);

	xGraph.Read(xPass, xFluxGraphics.GetDepthAttachment(), RESOURCE_ACCESS_READ_SRV);

	// SSAO feeds the ambient term in-shader. Its selector commits one output
	// handle (raw, legacy-filtered, or separable-filtered) for this graph build;
	// declaring only that handle keeps the graph Read identical to the SRV bound
	// by ExecuteApplyLighting during the one-frame runtime-toggle transition.
	Flux_SSAOImpl& xSSAO = g_xEngine.SSAO();
	xGraph.ReadTransient(xPass, xSSAO.GetOutputHandle(kuFluxViewSlotMain), RESOURCE_ACCESS_READ_SRV);

	// Shadow maps — one 4-cascade depth array (Phase 4b). Read ALL layers so the
	// graph transitions every cascade WRITE_DSV → SHADER_READ before this pass
	// (and, transitively, before the fog passes that also sample it).
	xGraph.ReadTransient(xPass, g_xEngine.Shadows().GetCSMArrayHandle(), RESOURCE_ACCESS_READ_SRV, 0, 1, 0, FLUX_RG_ALL_LAYERS);

	// Clustered-deferred cluster-output buffers — declaring the reads here causes
	// the graph to order this pass after Flux_LightClustering's compute writes,
	// with the necessary UAV→SRV barrier emitted automatically. The LightBuffer
	// itself is a frame-indexed Flux_DynamicReadWriteBuffer and so is NOT
	// graph-tracked — see the RENDER-GRAPH CONTRACT on Flux_FrameIndexedBufferBase
	// (Flux_Buffers.h).
	Flux_LightClusteringImpl& xLightClustering = g_xEngine.LightClustering();
	if (xLightClustering.IsInitialised())
	{
		xGraph.ReadBuffer(xPass, xLightClustering.GetClusterLightCountsBuffer().GetBuffer(),
			RESOURCE_ACCESS_READ_SRV);
		xGraph.ReadBuffer(xPass, xLightClustering.GetClusterLightIndicesBuffer().GetBuffer(),
			RESOURCE_ACCESS_READ_SRV);
	}

	// SSR / SSGI single-handle declarations. The subsystem decides which of
	// its internal handles serves as "the output" based on its debug toggles
	// at SetupRenderGraph time. Runtime toggles trigger g_xEngine.FluxRenderer().RequestGraphRebuild()
	// via ApplyBlurSelectionToGraph / ApplyDenoiseSelectionToGraph, which re-runs
	// this SetupRenderGraph and re-resolves the handle.
	if (g_xEngine.SSR().IsInitialised())
		xGraph.ReadTransient(xPass, g_xEngine.SSR().GetReflectionHandle(), RESOURCE_ACCESS_READ_SRV);
	if (g_xEngine.SSGI().IsInitialised())
		xGraph.ReadTransient(xPass, g_xEngine.SSGI().GetSSGIHandle(), RESOURCE_ACCESS_READ_SRV);

	// IBL textures — BRDF LUT + both double-buffered cubemaps (see
	// Flux_IBLImpl::DeclareConsumerReads).
	Flux_IBLImpl& xIBL = g_xEngine.IBL();
	xIBL.DeclareConsumerReads(xGraph, xPass);

	// Preview view (S5a): a second lighting instance over the preview view's own
	// G-buffer, writing its HDR target. Same record callback — the pass's view
	// slot selects the per-view G-buffer accessors + the preview VIEW set (whose
	// flags disable shadow/cluster sampling). S5b: the preview owns its own
	// SSAO/SSR/SSGI chains, so the same screen-space reads as the main pass are
	// declared against the preview-slot handles.
	// The CSM/cluster/IBL reads are still declared: the shader STATICALLY samples
	// those persistent VIEW members, so the graph-Read validator demands them.
	if (xFluxGraphics.RenderViews().IsViewActive(kuFluxViewSlotPreview))
	{
		const Flux_PassHandle xPreviewPass = xGraph.AddPass("Apply Lighting (Preview)", ExecuteApplyLighting)
			.View(kuFluxViewSlotPreview)
			.Writes(xFluxGraphics.GetHDRSceneTarget(kuFluxViewSlotPreview), RESOURCE_ACCESS_WRITE_RTV)
			.ClearTargets();
		for (u_int u = 0; u < uFLUX_MRT_CORE_COUNT; u++)   // lighting reads the 4 core G-buffer MRTs, never velocity
			xGraph.Read(xPreviewPass, xFluxGraphics.GetMRTAttachment(static_cast<MRTIndex>(u), kuFluxViewSlotPreview), RESOURCE_ACCESS_READ_SRV);
		xGraph.Read(xPreviewPass, xFluxGraphics.GetDepthAttachment(kuFluxViewSlotPreview), RESOURCE_ACCESS_READ_SRV);
		xGraph.ReadTransient(xPreviewPass, xSSAO.GetOutputHandle(kuFluxViewSlotPreview), RESOURCE_ACCESS_READ_SRV);
		xGraph.ReadTransient(xPreviewPass, g_xEngine.Shadows().GetCSMArrayHandle(), RESOURCE_ACCESS_READ_SRV, 0, 1, 0, FLUX_RG_ALL_LAYERS);
		if (xLightClustering.IsInitialised())
		{
			xGraph.ReadBuffer(xPreviewPass, xLightClustering.GetClusterLightCountsBuffer().GetBuffer(),
				RESOURCE_ACCESS_READ_SRV);
			xGraph.ReadBuffer(xPreviewPass, xLightClustering.GetClusterLightIndicesBuffer().GetBuffer(),
				RESOURCE_ACCESS_READ_SRV);
		}
		if (g_xEngine.SSR().IsInitialised())
			xGraph.ReadTransient(xPreviewPass, g_xEngine.SSR().GetReflectionHandle(kuFluxViewSlotPreview), RESOURCE_ACCESS_READ_SRV);
		if (g_xEngine.SSGI().IsInitialised())
			xGraph.ReadTransient(xPreviewPass, g_xEngine.SSGI().GetSSGIHandle(kuFluxViewSlotPreview), RESOURCE_ACCESS_READ_SRV);
		xIBL.DeclareConsumerReads(xGraph, xPreviewPass);
	}
}
