#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#include "Flux/Flux.h"
#include "Flux/Flux_PerFrame.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Skybox/Flux_SkyboxImpl.h"
#include "Flux/StaticMeshes/Flux_StaticMeshesImpl.h"
#include "Flux/AnimatedMeshes/Flux_AnimatedMeshesImpl.h"
#include "Flux/Terrain/Flux_TerrainImpl.h"
#ifdef ZENITH_WINDOWS
#include "Flux/Slang/Flux_SlangCompiler.h"
#endif
#ifdef ZENITH_TOOLS
#include "Flux/Gizmos/Flux_GizmosImpl.h"
#include "Flux/Slang/Flux_ShaderHotReload.h"
#endif
#include "Flux/Primitives/Flux_PrimitivesImpl.h"
#include "Flux/DeferredShading/Flux_DeferredShadingImpl.h"
#include "Flux/SSAO/Flux_SSAOImpl.h"
#include "Flux/Fog/Flux_FogImpl.h"
#include "Flux/Fog/Flux_VolumeFogImpl.h"
#include "Flux/Zenith_GameRenderHook.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "Flux/SDFs/Flux_SDFsImpl.h"
#include "Flux/Shadows/Flux_ShadowsImpl.h"
#include "Flux/Particles/Flux_ParticlesImpl.h"
#include "Flux/Text/Flux_TextImpl.h"
#include "Flux/Quads/Flux_QuadsImpl.h"
#include "Flux/InstancedMeshes/Flux_InstancedMeshesImpl.h"
#include "Flux/HDR/Flux_HDRImpl.h"
#include "Flux/IBL/Flux_IBLImpl.h"
#include "Flux/HiZ/Flux_HiZImpl.h"
#include "Flux/SSR/Flux_SSRImpl.h"
#include "Flux/SSGI/Flux_SSGIImpl.h"
#include "Flux/Vegetation/Flux_GrassImpl.h"
#include "Flux/DynamicLights/Flux_DynamicLightsImpl.h"
#include "Flux/DynamicLights/Flux_LightClusteringImpl.h"
#include "Flux/Decals/Flux_DecalsImpl.h"
#include "DebugVariables/Zenith_DebugVariables.h"

#include "Flux/Flux_RendererImpl.h"
#include "Flux/Flux_FeatureRegistry.h"

// Phase 6a-1: Flux namespace state moved off Flux class onto
// Flux_RendererImpl held by Zenith_Engine. Static facade methods below
// forward through g_xEngine.FluxRenderer().m_xXxx.

const uint32_t Flux_RendererImpl::GetFrameCounter() { return g_xEngine.FluxRenderer().m_uFrameCounter; }

void Flux_RendererImpl::SubmitCommandList(const Flux_CommandList* pxCmdList,
	const Flux_RenderGraph_AttachmentRef* axColourAttachments, uint32_t uNumColour,
	const Flux_RenderGraph_AttachmentRef& xDepthStencil,
	bool bClearTargets, bool bDepthIsReadOnly, const Flux_RenderGraph_Pass* pxPass)
{
	Zenith_Assert(pxCmdList != nullptr, "SubmitCommandList: Command list is null");
	Zenith_Assert(pxPass != nullptr, "SubmitCommandList: pass pointer is null — bypass path no longer supported");
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "SubmitCommandList: must be called from the main thread (Flux_RenderGraph::Execute Phase 2)");
	Flux_CommandListEntry xEntry;
	xEntry.m_pxCmdList = pxCmdList;
	for (uint32_t i = 0; i < uNumColour; i++) xEntry.m_axColourAttachments[i] = axColourAttachments[i];
	xEntry.m_uNumColourAttachments = uNumColour;
	xEntry.m_xDepthStencil = xDepthStencil;
	xEntry.m_pxPass = pxPass;
	xEntry.m_bClearTargets = bClearTargets;
	xEntry.m_bDepthIsReadOnly = bDepthIsReadOnly;
	g_xEngine.FluxRenderer().m_xPendingCommandLists.PushBack(xEntry);
}

void Flux_RendererImpl::AddResChangeCallback(void(*pfnCallback)())
{
	g_xEngine.FluxRenderer().m_xResChangeCallbacks.PushBack(pfnCallback);
}

void Flux_RendererImpl::ClearPendingCommandLists()
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(),
		"ClearPendingCommandLists: main-thread only");
	g_xEngine.FluxRenderer().m_xPendingCommandLists.Clear();
}

Flux_RenderGraph& Flux_RendererImpl::GetRenderGraph()    { return *g_xEngine.FluxRenderer().m_pxRenderGraph; }
bool              Flux_RendererImpl::IsRenderGraphValid(){ return g_xEngine.FluxRenderer().m_pxRenderGraph != nullptr; }

void Flux_RendererImpl::RequestGraphRebuild() { g_xEngine.FluxRenderer().m_bGraphRebuildRequested = true; }
bool Flux_RendererImpl::ConsumeGraphRebuildRequest()
{
	Flux_RendererImpl& xRenderer = g_xEngine.FluxRenderer();
	bool b = xRenderer.m_bGraphRebuildRequested;
	xRenderer.m_bGraphRebuildRequested = false;
	return b;
}

Zenith_Vector<Flux_CommandListEntry>& Flux_RendererImpl::GetPendingCommandLists()
{
	return g_xEngine.FluxRenderer().m_xPendingCommandLists;
}

// Debug-variable backing store for the transient-aliasing runtime toggle.
// Synced into the render graph at each SetupRenderGraph via SetAliasingEnabled;
// changes trigger MarkDirty so the next Compile rebuilds the pool layout.
DEBUGVAR bool dbg_bTransientAliasing = true;

// No-op record callback used by the final-layout-transition barrier pass — the
// pass exists purely so the render graph emits a prologue barrier transitioning
// the Final Render Target into SHADER_READ_ONLY_OPTIMAL at end-of-frame, ready
// for the swapchain copy command buffer (which lives outside the graph) to
// sample it without a layout mismatch.
static void Flux_FinalLayoutTransitionNoOp(Flux_CommandList*, void*)
{
}

void Flux_PipelineHelper::BuildFullscreenPipeline(
	Flux_Shader& xShader,
	Flux_Pipeline& xPipeline,
	FluxShaderProgram eProgram,
	TextureFormat eColourFormat,
	TextureFormat eDepthStencilFormat)
{
	Flux_PipelineSpecification xSpec = CreateFullscreenSpec(xShader, eProgram, eColourFormat, eDepthStencilFormat);
	Flux_PipelineBuilder::FromSpecification(xPipeline, xSpec);
}

Flux_PipelineSpecification Flux_PipelineHelper::CreateFullscreenSpec(
	Flux_Shader& xShader,
	FluxShaderProgram eProgram,
	TextureFormat eColourFormat,
	TextureFormat eDepthStencilFormat)
{
	// Single-RTV is the N==1 case. Taking the address of the by-value param
	// yields a valid 1-element array for the duration of this call.
	return CreateFullscreenSpecMRT(xShader, eProgram, &eColourFormat, 1u, eDepthStencilFormat);
}

Flux_PipelineSpecification Flux_PipelineHelper::CreateFullscreenSpecMRT(
	Flux_Shader& xShader,
	FluxShaderProgram eProgram,
	const TextureFormat* aeColourFormats,
	u_int uNumColourAttachments,
	TextureFormat eDepthStencilFormat)
{
	Zenith_Assert(uNumColourAttachments >= 1 && uNumColourAttachments <= FLUX_MAX_TARGETS,
		"CreateFullscreenSpecMRT: uNumColourAttachments %u out of range [1, %u]", uNumColourAttachments, FLUX_MAX_TARGETS);

	xShader.Initialise(eProgram);

	Flux_PipelineSpecification xSpec;
	for (u_int u = 0; u < uNumColourAttachments; u++)
	{
		xSpec.m_aeColourAttachmentFormats[u] = aeColourFormats[u];
	}
	xSpec.m_uNumColourAttachments = uNumColourAttachments;
	xSpec.m_eDepthStencilFormat = eDepthStencilFormat;
	xSpec.m_pxShader = &xShader;
	xSpec.m_xVertexInputDesc.m_eTopology = MESH_TOPOLOGY_NONE;
	xSpec.m_bDepthTestEnabled = false;
	xSpec.m_bDepthWriteEnabled = false;

	// Fullscreen passes are conceptually overwrite operations — the fragment
	// shader is expected to write every pixel. Default Flux_BlendState has
	// alpha blending enabled, which for passes like SSR RayMarch would blend
	// the new output into stale last-frame contents (alpha < 1 preserves old
	// pixels, producing ghosting). Callers that actually want a blend mode
	// (e.g. SSAO Upsample, Skybox Aerial Perspective) override this explicitly.
	for (Flux_BlendState& xBlendState : xSpec.m_axBlendStates)
	{
		xBlendState.m_bBlendEnabled = false;
		xBlendState.m_eSrcBlendFactor = BLEND_FACTOR_ONE;
		xBlendState.m_eDstBlendFactor = BLEND_FACTOR_ZERO;
	}

	xShader.GetReflection().PopulateLayout(xSpec.m_xPipelineLayout);

	return xSpec;
}

void Flux_RendererImpl::EarlyInitialise()
{
	// Flux_PerFrame must initialise BEFORE backend Initialise so backends can
	// register their begin/end-frame callbacks during their own setup. The
	// counter starts at 0 and the ring index is correctly defined from the
	// first call.
	PerFrameInitialise();

	g_xEngine.FluxBackend().Initialise();
	g_xEngine.FluxMemory().Initialise();
	g_xEngine.FluxBackend().InitialisePerFrameResources(); // Must be after memory manager init
	g_xEngine.FluxGraphics().InitialiseSamplers(); // Must be before any CreateShaderResourceView calls (bindless registration)
}

void Flux_RendererImpl::LateInitialise()
{
	// Subsystem dependency graph (A -> B means A must init before B):
	//
	// MemoryManager -> SlangCompiler -> Swapchain -> Graphics
	// Graphics -> HDR -> DeferredShading
	// Graphics -> Shadows, Skybox, StaticMeshes, AnimatedMeshes, InstancedMeshes, Primitives
	// Skybox -> IBL (environment probes use skybox cubemap)
	// Terrain -> Grass (vegetation placed on terrain)
	// HiZ -> SSR, SSGI (screen-space effects use Hi-Z pyramid)
	// SSR, SSGI -> DeferredShading (composites SSR/SSGI results)
	// DeferredShading -> DynamicLights (lights applied after deferred setup)
	// [Tools] PlatformAPI -> ImGui -> Gizmos, ShaderHotReload
	//
	// Independent (no ordering constraint): SSAO, Fog, SDFs, Particles, Quads, Text

	g_xEngine.FluxMemory().BeginFrame();

#ifdef ZENITH_WINDOWS
	// Initialize Slang compiler before any shader loading
	Flux_SlangCompiler::Initialise();
	// Tell the modern session API where to resolve `loadModule` paths from.
	// Required for FluxShaderProgram-based runtime compilation; the legacy
	// per-file paths embed their own search root via spAddSearchPath.
	Flux_SlangCompiler::AddSearchPath(SHADER_SOURCE_ROOT);
#endif

	g_xEngine.FluxSwapchain().Initialise();

#ifdef ZENITH_TOOLS
	// Bring up the hot-reload watcher BEFORE any subsystem Initialise() so
	// the RegisterSubsystem calls inside those Initialise bodies see a live
	// watcher. (The static registration list is also safe to append to
	// before Initialise, but starting the file watcher early means the
	// `firing N rebuild callback(s)` log line picks up edits made even
	// during engine boot.)
	Flux_ShaderHotReload::Initialise();
#endif

	g_xEngine.FluxGraphics().Initialise(g_xEngine.FluxMemory(), g_xEngine.FluxSwapchain(), g_xEngine.Shadows());

#ifdef ZENITH_TOOLS
	// ImGui is the tail of the inline prologue. It depends only on the Vulkan
	// device + swapchain format (see Zenith_Vulkan::InitialiseImGuiRenderPass),
	// not on any registry feature, so bringing it up here — before the
	// FluxGraphics-onward feature walk — is dependency-safe. Gizmos (which DOES
	// depend on ImGui) is registered as a feature and initialised by the walk.
	g_xEngine.FluxBackend().InitialiseImGui();
#endif

	// Wave-13.B: the per-subsystem Initialise() ladder that used to live inline
	// here now walks the Flux_FeatureRegistry. RegisterDefaultFeatures() emits
	// the features in the SAME init order (FluxGraphics is brought up inline
	// above; the walk starts at HDR — FluxGraphics' registry init trampoline is
	// null) and a debug golden-order assert backs the sequence. The DI seams
	// (HiZ / SSAO) gather their dependency params from g_xEngine inside their
	// init trampolines — see Flux_FeatureRegistry.cpp. The dependency rationale
	// documented at the top of this function still holds; the registry preserves
	// that order rather than replacing it.
	Flux_FeatureRegistry::RegisterDefaultFeatures();
	const Flux_FeatureRegistry& xRegistry = Flux_FeatureRegistry::Get();
	for (u_int uFeature = 0; uFeature < xRegistry.GetNumFeatures(); uFeature++)
	{
		const Flux_FeatureDesc& xDesc = xRegistry.GetFeatures()[uFeature];
		if (xDesc.m_pfnInitialise != nullptr)
			xDesc.m_pfnInitialise();
	}

	g_xEngine.FluxMemory().EndFrame(false);

	// Create and compile the render graph
	g_xEngine.FluxRenderer().m_pxRenderGraph = new Flux_RenderGraph();

#ifdef ZENITH_DEBUG_VARIABLES
	// Debug-variable tree-path convention: most renderer variables live under
	// "Render/...", but a handful of subsystems (HDR and HiZ) established a
	// "Flux/<Subsystem>/..." convention before the "Render/" top-level
	// consolidated. New HDR / HiZ variables follow their subsystem's existing
	// convention; generic renderer-level variables go under "Render/". A
	// future cleanup pass may migrate HDR / HiZ under "Render/HDR" and
	// "Render/HiZ" but is deliberately NOT done here (would require an editor
	// config migration for users who saved expanded-tree state).

	// Debug toggle for transient memory aliasing. The graph reads this value
	// at each SetupRenderGraph invocation and calls SetAliasingEnabled; a
	// change triggers MarkDirty so the next Compile rebuilds the pool layout.
	g_xEngine.DebugVariables().AddBoolean({ "Render", "RenderGraph", "Transient Aliasing" }, dbg_bTransientAliasing);

	// Click-to-log button: prints the compiled render-graph pass order. Useful
	// for newcomers asking "what runs when?" — the answer is the topological
	// sort of the pass DAG, not the order of AddPass() calls in SetupRenderGraph.
	g_xEngine.DebugVariables().AddButton({ "Render", "RenderGraph", "Print Pass Order" }, []() {
		if (g_xEngine.FluxRenderer().m_pxRenderGraph != nullptr)
		{
			const std::string strOrder = g_xEngine.FluxRenderer().m_pxRenderGraph->GetPassOrderDescription();
			Zenith_Log(LOG_CATEGORY_RENDERER, "Render-graph pass order: %s", strOrder.c_str());
		}
		else
		{
			Zenith_Warning(LOG_CATEGORY_RENDERER, "Render graph not initialised yet — try again after the first frame.");
		}
	});

	// MRT / depth / HDR / bloom transient previews. Registered once here (not in
	// SetupTransients, which re-runs on resize) and resolve the current SRV
	// via callback every ImGui draw — rebuilds that invalidate the underlying
	// TransientResource don't leave a dangling pointer in the tree.
	g_xEngine.DebugVariables().AddTextureCallback({ "Render", "Debug", "MRT Diffuse" },       [](){ return g_xEngine.FluxGraphics().GetDebugSRV_MRTDiffuse(); });
	g_xEngine.DebugVariables().AddTextureCallback({ "Render", "Debug", "MRT NormalsAO" },     [](){ return g_xEngine.FluxGraphics().GetDebugSRV_MRTNormalsAO(); });
	g_xEngine.DebugVariables().AddTextureCallback({ "Render", "Debug", "MRT Material" },      [](){ return g_xEngine.FluxGraphics().GetDebugSRV_MRTMaterial(); });
	g_xEngine.DebugVariables().AddTextureCallback({ "Render", "Debug", "Depth" },             [](){ return g_xEngine.FluxGraphics().GetDebugSRV_Depth(); });
	// HDR textures follow Flux_HDR.cpp's established "Flux/HDR/..." convention.
	g_xEngine.DebugVariables().AddTextureCallback({ "Flux",   "HDR",   "Textures", "HDRScene"  }, [](){ return g_xEngine.HDR().GetDebugSRV_HDRScene(); });
	g_xEngine.DebugVariables().AddTextureCallback({ "Flux",   "HDR",   "Textures", "BloomMip0" }, [](){ return g_xEngine.HDR().GetDebugSRV_Bloom0(); });
	g_xEngine.DebugVariables().AddTextureCallback({ "Flux",   "HDR",   "Textures", "BloomMip1" }, [](){ return g_xEngine.HDR().GetDebugSRV_Bloom1(); });
	g_xEngine.DebugVariables().AddTextureCallback({ "Flux",   "HDR",   "Textures", "BloomMip2" }, [](){ return g_xEngine.HDR().GetDebugSRV_Bloom2(); });
#endif

	SetupRenderGraph();
	// Free-function wrapper for AddResChangeCallback — pfnCallback is a
	// `void(*)()` callable, which a member function pointer can't satisfy
	// directly without an instance bound. The instance is the singleton on
	// g_xEngine, so a trivial wrapper routes through.
	AddResChangeCallback(+[](){ g_xEngine.FluxRenderer().SetupRenderGraph(); });
}

void Flux_RendererImpl::ApplySubsystemGraphSelections(Flux_RenderGraph& xGraph)
{
	// Order matters here. Both subsystems below run BEFORE Compile() so any
	// SetPassEnabled / MarkDirty mutations they perform take effect on the
	// same frame. Neither can live as a pass OnPrepare callback because
	// Phase 0 only fires OnPrepare for *enabled* passes — once a system has
	// disabled its previously-active pass, an OnPrepare-based switcher would
	// never run again.
	//
	// Fog must run BEFORE IBL so that if the user changes fog technique this
	// frame, ApplyTechniqueSelectionToGraph calls MarkDirty() *before* IBL's
	// UpdateGraphPassEnables checks IsDirty() — which lets IBL force-enable
	// all 49 of its passes for the upcoming full Compile() so the validator
	// sees a writer for every IBL texture that DeferredShading reads.
	g_xEngine.Fog().ApplyTechniqueSelectionToGraph(xGraph);
	// SSR / SSGI runtime output toggles: when blur or denoise flip, these
	// enable/disable their post-pass and MarkDirty so the deferred-lighting
	// pass re-reads the correct handle (see g_xEngine.SSR().GetReflectionHandle).
	// Must run BEFORE IBL's UpdateGraphPassEnables for the same MarkDirty
	// propagation reason described above.
	g_xEngine.SSR().ApplyBlurSelectionToGraph(xGraph);
	g_xEngine.SSGI().ApplyDenoiseSelectionToGraph(xGraph);
	g_xEngine.IBL().UpdateGraphPassEnables(xGraph);
}

void Flux_RendererImpl::SyncRenderGraphDebugToggles()
{
	if (g_xEngine.FluxRenderer().m_pxRenderGraph == nullptr)
		return;
#ifdef ZENITH_DEBUG_VARIABLES
	// Transient aliasing toggle — SetAliasingEnabled is a no-op when the
	// value is unchanged and MarkDirty's on change, so calling this every
	// frame is cheap and propagates editor flips on the next Compile.
	// Guard uses ZENITH_DEBUG_VARIABLES (not ZENITH_TOOLS) to match the
	// declaration guard on dbg_bTransientAliasing and the AddBoolean /
	// AddTextureCallback registrations above — the macros imply one another
	// (enforced in Zenith.h) so the choice is purely for reader clarity:
	// anything touching a debug variable guards on ZENITH_DEBUG_VARIABLES.
	g_xEngine.FluxRenderer().m_pxRenderGraph->SetAliasingEnabled(dbg_bTransientAliasing);
#endif
}

void Flux_RendererImpl::SetupRenderGraph()
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(),
		"SetupRenderGraph: must run on the main thread; pending command lists are accessed without locking here.");

	// Clear pending command lists first — they hold pointers to the graph's command lists
	// which will be destroyed by Clear(). Caller must have already drained the GPU.
	ClearPendingCommandLists();
	Flux_RendererImpl& xRenderer = g_xEngine.FluxRenderer();
	xRenderer.m_pxRenderGraph->Clear();

	// Sync the transient-aliasing debug toggle into the graph. SetAliasingEnabled
	// is a no-op if the value is unchanged; on change it calls MarkDirty so the
	// next Compile rebuilds the pool layout.
	SyncRenderGraphDebugToggles();

	// Core render targets FIRST — every other subsystem depends on these.
	g_xEngine.FluxGraphics().SetupTransients(*xRenderer.m_pxRenderGraph);
	g_xEngine.HDR().SetupTransients(*xRenderer.m_pxRenderGraph); // HDR scene target used by many subsystems

	// Wave-13.B: the feature SetupRenderGraph ladder now walks the
	// Flux_FeatureRegistry in four sub-walks separated by the inline irregulars
	// below. The order is identical to the pre-refactor sequence and is backed
	// by the registry's debug golden-order assert. The irregulars are NOT plain
	// feature SetupRenderGraph calls and so stay inline:
	//   - FluxGraphics/HDR SetupTransients (above, before phase 1)
	//   - Skybox aerial perspective (separate method, after phase 1)
	//   - post-fog game render hook (after phase 2)
	//   - HDR's second SetupRenderGraph (it IS a feature, in phase 3)
	//   - final-RT layout-transition pass (below, after phase 4)
	const Flux_FeatureRegistry& xRegistry = Flux_FeatureRegistry::Get();

	// Phase 1: preprocessing (IBL, Skybox) -> geometry G-buffer writers
	// (Shadows, StaticMeshes, Terrain, Primitives, AnimatedMeshes,
	// InstancedMeshes, Grass) -> decals (after writers, before readers) ->
	// screen-space effects (HiZ, SSR, SSGI) -> clustering + deferred lighting.
	// Clustering precedes DeferredShading so the per-cluster light lists are
	// declared as writers before the shading pass reads them.
	xRegistry.RunSetupPhase(*xRenderer.m_pxRenderGraph, FLUX_SETUP_PHASE_PREPASS_TO_LIGHTING);

	// Aerial perspective runs after DeferredShading — it blends scattering on
	// top of the already-lit HDR scene. Registering here keeps the writer-chain
	// topological order correct.
	g_xEngine.Skybox().SetupAerialPerspectiveRenderGraph(*xRenderer.m_pxRenderGraph);

	// Phase 2: SSAO + Fog.
	xRegistry.RunSetupPhase(*xRenderer.m_pxRenderGraph, FLUX_SETUP_PHASE_SSAO_FOG);

	// Game-side post-fog hook: contracted to fire AFTER engine fog passes are
	// registered and BEFORE any post-processing passes. Games that disable the
	// engine fog system (g_xEngine.Fog().SetExternallyOverridden(true)) and substitute
	// their own atmospheric pass register here. See Zenith_GameRenderHook.h.
	Zenith_GameRenderHook::InvokePostFogRegistrations(*xRenderer.m_pxRenderGraph);

	// Phase 3: post-processing (SDFs, Particles, HDR composite).
	xRegistry.RunSetupPhase(*xRenderer.m_pxRenderGraph, FLUX_SETUP_PHASE_POST_PROCESS);

	// Phase 4: UI & presentation (Quads, Text, [tools] Gizmos).
	xRegistry.RunSetupPhase(*xRenderer.m_pxRenderGraph, FLUX_SETUP_PHASE_UI);

	// Final-layout transition pass — leaves the Final Render Target in
	// SHADER_READ_ONLY_OPTIMAL so the swapchain copy command buffer (which
	// lives outside the render graph) can sample it without a layout
	// mismatch. The pass has no commands and no target setup; it exists
	// purely so the graph emits a prologue barrier transitioning the Final
	// RT from WRITE_RTV (set by tonemap / last writer) to READ_SRV.
	xRenderer.m_pxRenderGraph->AddPass("Final RT Layout Transition", Flux_FinalLayoutTransitionNoOp)
		.Reads(g_xEngine.FluxGraphics().GetFinalRenderTarget(), RESOURCE_ACCESS_READ_SRV);

	// Clear() already left the graph dirty — no explicit MarkDirty() needed.
}

void Flux_RendererImpl::ReleaseAssetReferences()
{
	// Drop refs to Flux-side assets so Zenith_AssetRegistry::Shutdown can delete them
	// cleanly. Each subsystem releases the handles it owns.
	g_xEngine.FluxGraphics().ReleaseAssetReferences();
	g_xEngine.Text().ReleaseAssetReferences();
	g_xEngine.Particles().ReleaseAssetReferences();
	g_xEngine.Terrain().ReleaseAssetReferences();
	g_xEngine.Skybox().ReleaseAssetReferences();
	g_xEngine.VolumeFog().ReleaseAssetReferences();

	// Material defaults live in AssetHandling but are part of the same pre-registry
	// release window — this is the natural place to drop them.
	Zenith_MaterialAsset::ReleaseDefaults();
}

void Flux_RendererImpl::Shutdown()
{
	Flux_RendererImpl& xRenderer = g_xEngine.FluxRenderer();
	delete xRenderer.m_pxRenderGraph;
	xRenderer.m_pxRenderGraph = nullptr;
	// Clear res-change callbacks so OnResChange has nothing to invoke — the
	// callbacks would otherwise deref the now-null m_pxRenderGraph and crash.
	xRenderer.m_xResChangeCallbacks.Clear();

	// Shutdown Flux subsystems in REVERSE order of initialization. This ensures
	// dependencies are destroyed after their dependents.
	//
	// Wave-13.B: the explicit reverse-order .Shutdown() ladder (Text -> ... ->
	// Shadows) now walks the Flux_FeatureRegistry's explicit shutdown order
	// (transcribed exactly from the former ladder, NOT a mechanical reverse of
	// init; backed by the registry's debug golden-order assert). Features with
	// no Shutdown() trampoline (Fog — RAII / stateless) are skipped. The
	// non-feature teardown below (Slang / HotReload / ImGui) and the three
	// inline-shutdown features (Gizmos, HDR, FluxGraphics — each kept inline
	// here, deliberately absent from the registry shutdown walk) follow.
	Flux_FeatureRegistry::Get().RunShutdown();

#ifdef ZENITH_WINDOWS
	Flux_SlangCompiler::Shutdown();
#endif

#ifdef ZENITH_TOOLS
	Flux_ShaderHotReload::Shutdown();
	g_xEngine.Gizmos().Shutdown();
	g_xEngine.FluxBackend().ShutdownImGui();
#endif

	// HDR must shutdown after other render systems that target HDR buffer
	g_xEngine.HDR().Shutdown();

	// Shutdown core graphics (render targets, depth buffer, quad mesh, frame constants)
	g_xEngine.FluxGraphics().Shutdown();

	// Shutdown swapchain-owned file-static shader/pipeline state before the
	// Vulkan device and memory-manager registries go away.
	g_xEngine.FluxSwapchain().Shutdown();

	// Shutdown memory manager (VMA allocator, handle registries)
	g_xEngine.FluxMemory().Shutdown();

	// Clear PerFrame callback arrays so a subsequent Flux_RendererImpl::Initialise starts
	// from a known empty state (matters for unit tests that re-init Flux
	// within one process). Frame counter is intentionally left alone.
	PerFrameShutdown();

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux shut down");
}

void Flux_RendererImpl::OnResChange()
{
	Flux_RendererImpl& xRenderer = g_xEngine.FluxRenderer();
	for (u_int i = 0; i < xRenderer.m_xResChangeCallbacks.GetSize(); i++)
	{
		xRenderer.m_xResChangeCallbacks.Get(i)();
	}
}

bool Flux_RendererImpl::PrepareFrame(Flux_WorkDistribution& xOutDistribution)
{
	static_assert(FLUX_NUM_WORKER_THREADS > 0, "FLUX_NUM_WORKER_THREADS must be positive");

	xOutDistribution.Clear();

	// The render graph submits command lists in topological order — no sort needed here.

	Flux_RendererImpl& xRenderer = g_xEngine.FluxRenderer();

	// Count total commands
	for (u_int i = 0; i < xRenderer.m_xPendingCommandLists.GetSize(); i++)
	{
		xOutDistribution.uTotalCommandCount += xRenderer.m_xPendingCommandLists.Get(i).m_pxCmdList->GetCommandCount();
	}

	if (xOutDistribution.uTotalCommandCount == 0)
	{
		return false;
	}

	// Distribute work across threads based on command count
	const u_int uTargetCommandsPerThread = (xOutDistribution.uTotalCommandCount + FLUX_NUM_WORKER_THREADS - 1) / FLUX_NUM_WORKER_THREADS;
	u_int uCurrentThreadIndex = 0;
	u_int uCurrentThreadCommandCount = 0;

	xOutDistribution.auStartIndex[0] = 0;

	for (u_int uIndex = 0; uIndex < xRenderer.m_xPendingCommandLists.GetSize(); uIndex++)
	{
		const u_int uCommandCount = xRenderer.m_xPendingCommandLists.Get(uIndex).m_pxCmdList->GetCommandCount();

		if (uCurrentThreadCommandCount > 0 &&
			uCurrentThreadCommandCount + uCommandCount > uTargetCommandsPerThread &&
			uCurrentThreadIndex < FLUX_NUM_WORKER_THREADS - 1)
		{
			xOutDistribution.auEndIndex[uCurrentThreadIndex] = uIndex;

			uCurrentThreadIndex++;
			uCurrentThreadCommandCount = 0;
			xOutDistribution.auStartIndex[uCurrentThreadIndex] = uIndex;
		}

		uCurrentThreadCommandCount += uCommandCount;
	}

	Zenith_Assert(uCurrentThreadIndex < FLUX_NUM_WORKER_THREADS,
		"PrepareFrame: Thread index %u out of bounds (max %u)", uCurrentThreadIndex, FLUX_NUM_WORKER_THREADS);

	if (uCurrentThreadIndex < FLUX_NUM_WORKER_THREADS)
	{
		xOutDistribution.auEndIndex[uCurrentThreadIndex] = xRenderer.m_xPendingCommandLists.GetSize();
	}

	// Explicitly write empty (N, N) ranges for any worker that did not receive
	// a command-list slice. The invariant is relied on by the consumer; writing
	// it here defensively protects against xOutDistribution.Clear() behaviour
	// changing in future.
	const u_int uPendingSize = xRenderer.m_xPendingCommandLists.GetSize();
	for (u_int u = uCurrentThreadIndex + 1; u < FLUX_NUM_WORKER_THREADS; u++)
	{
		xOutDistribution.auStartIndex[u] = uPendingSize;
		xOutDistribution.auEndIndex[u] = uPendingSize;
	}

	return true;
}
