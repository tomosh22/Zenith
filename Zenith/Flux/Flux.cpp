#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#include "Flux/Flux.h"
#include "Flux/Flux_PerFrame.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/SceneGraph/Flux_RenderSceneSnapshot.h"   // complete type for the by-ptr snapshot (alloc/rebuild/free)
#include "Flux/Skybox/Flux_SkyboxImpl.h"
#include "Flux/StaticMeshes/Flux_StaticMeshesImpl.h"
#include "Flux/AnimatedMeshes/Flux_AnimatedMeshesImpl.h"
#include "Flux/Terrain/Flux_TerrainImpl.h"
#ifdef ZENITH_WINDOWS
#include "Flux/Slang/Flux_SlangCompiler.h"
#endif
#ifdef ZENITH_TOOLS
#include "Flux/Gizmos/Flux_GizmosImpl.h"
#include "Flux/MaterialPreview/Flux_MaterialPreviewImpl.h"
#include "Flux/Slang/Flux_ShaderHotReload.h"
#endif
#include "Flux/Primitives/Flux_PrimitivesImpl.h"
#include "Flux/DeferredShading/Flux_DeferredShadingImpl.h"
#include "Flux/SSAO/Flux_SSAOImpl.h"
#include "Flux/Fog/Flux_FogImpl.h"
#include "Flux/Fog/Flux_VolumeFogImpl.h"
#include "Flux/Zenith_GameRenderFeatures.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "Flux/SDFs/Flux_SDFsImpl.h"
#include "Flux/Shadows/Flux_ShadowsImpl.h"
#include "Flux/Flux_PersistentSetLayouts.h"   // VIEW-set binding indices (Phase 5.4)
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
#include "Flux/Slang/Flux_ShaderCatalog.h" // ValidateFeatureParity boot check

// (The frame counter that used to be read from here moved to FrameContext —
// g_xEngine.Frame().GetFrameIndex().)

void Flux_RendererImpl::QueueRenderPass(const Flux_RenderGraph* pxGraph,
	const Flux_RenderGraph_AttachmentRef* axColourAttachments, uint32_t uNumColour,
	const Flux_RenderGraph_AttachmentRef& xDepthStencil,
	bool bClearTargets, bool bDepthIsReadOnly, const Flux_RenderGraph_Pass* pxPass)
{
	Zenith_Assert(pxGraph != nullptr, "QueueRenderPass: graph pointer is null");
	Zenith_Assert(pxPass != nullptr, "QueueRenderPass: pass pointer is null — bypass path no longer supported");
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "QueueRenderPass: must be called from the main thread (Flux_RenderGraph::Execute)");
	Flux_RenderPassEntry xEntry;
	xEntry.m_pxGraph = pxGraph;
	for (uint32_t i = 0; i < uNumColour; i++) xEntry.m_axColourAttachments[i] = axColourAttachments[i];
	xEntry.m_uNumColourAttachments = uNumColour;
	xEntry.m_xDepthStencil = xDepthStencil;
	xEntry.m_pxPass = pxPass;
	xEntry.m_bClearTargets = bClearTargets;
	xEntry.m_bDepthIsReadOnly = bDepthIsReadOnly;
	m_xPendingRenderPasses.PushBack(xEntry);
}

void Flux_RendererImpl::AddResChangeCallback(void(*pfnCallback)())
{
	m_xResChangeCallbacks.PushBack(pfnCallback);
}

void Flux_RendererImpl::RecordFrame()
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "RecordFrame: must be called from the main thread (Flux_RenderGraph::Execute)");

	// Distribute the queued passes across worker threads. No work this frame →
	// drain the (already-empty) queue and report no render work so EndFrame skips
	// the render submit.
	Flux_WorkDistribution xWorkDistribution;
	xWorkDistribution.Clear();
	if (!PrepareFrame(xWorkDistribution))
	{
		m_bHasRenderWork = false;
		ClearPendingRenderPasses();
		return;
	}

	// Upload the GPU material table for this frame BEFORE any pass records. All
	// material registration (index assignment + record build) happened during the
	// gather Prepare callbacks (CallPrepareCallbacks ran before this, on the main
	// thread); this copies the CPU records into the current frame-in-flight buffer.
	// Frame-indexed + host-coherent → no graph barrier (Flux_FrameIndexedBufferBase
	// contract). Main-thread + before worker recording, so it is race-free.
	g_xEngine.FluxGraphics().MaterialTable().Upload();

	// Phase 5.1: write the current frame's persistent GLOBAL/VIEW descriptor sets from
	// the spine constant buffers, also BEFORE any worker records (write-before-bind).
	// Main-thread + frame-indexed sets → race-free, no graph barrier. The workers then
	// bind these persistent sets per pipeline instead of allocating sets 0/1 per draw.
	g_xEngine.FluxBackend().PreparePersistentSets(
		g_xEngine.FluxGraphics().GetGlobalConstantsBufferHandle(),
		g_xEngine.FluxGraphics().MaterialTable().GetSRV().m_xBufferDescHandle,
		g_xEngine.FluxGraphics().GetViewConstantsBufferHandle());

	// Phase 5.4: write the view-frequency SRVs promoted into the persistent VIEW set.
	// The CSM array is always allocated (cleared-to-far when shadows are disabled), so
	// VIEW binding 1 is always valid; consumers sample g_xViewSet.g_xCSM and declare the
	// graph Read() (enforced by the Flux_ViewSetBinding validator). Same write-before-bind
	// window as PreparePersistentSets.
	g_xEngine.FluxBackend().WritePersistentViewImage(
		Flux_PersistentSetLayouts::kuViewBinding_CSM,
		g_xEngine.Shadows().GetCSMArraySRV(),
		g_xEngine.FluxGraphics().m_xClampSampler);

	// Phase 5.4: the all-cascade ShadowMatrices SSBO is also a VIEW-frequency resource.
	// It is a frame-indexed Flux_DynamicReadWriteBuffer (graph-invisible by contract, like
	// g_xView / g_axMaterials), so GetShadowMatricesSRV() yields THIS frame's buffer view —
	// re-written into the persistent VIEW set every frame (no staleness). Updated earlier
	// this frame by the shadow PreExecute (UpdateShadowMatrices), which runs before record.
	g_xEngine.FluxBackend().WritePersistentViewBuffer(
		Flux_PersistentSetLayouts::kuViewBinding_ShadowMatrices,
		g_xEngine.Shadows().GetShadowMatricesSRV());

	// Phase 5.4: the clustered-lighting read buffers also live in the persistent VIEW set.
	// g_xLightBuffer is a frame-indexed dynamic buffer (current-frame SRV each frame); the
	// cluster count/index buffers are GPU-only (graph-tracked — the LightClustering compute
	// writes them via UAV, consumers read these persistent SRVs with graph-driven barriers).
	// All three are always allocated post-init, so the descriptor is always valid — VIEW
	// bindings 3-5 must never be left unwritten (consumers statically sample them).
	g_xEngine.FluxBackend().WritePersistentViewBuffer(
		Flux_PersistentSetLayouts::kuViewBinding_LightBuffer,
		g_xEngine.DynamicLights().GetLightBufferSRV());
	g_xEngine.FluxBackend().WritePersistentViewBuffer(
		Flux_PersistentSetLayouts::kuViewBinding_ClusterLightCounts,
		g_xEngine.LightClustering().GetClusterLightCountsSRV());
	g_xEngine.FluxBackend().WritePersistentViewBuffer(
		Flux_PersistentSetLayouts::kuViewBinding_ClusterLightIndices,
		g_xEngine.LightClustering().GetClusterLightIndicesSRV());

	// Drive the backend to record every queued pass directly into its worker
	// command buffers (Vulkan: parallel worker task; D3D12 null backend: serial
	// callback loop). Runs synchronously inside the render-task safe window and
	// BEFORE the frame memory submit, so record-callback uploads land this frame
	// and ECS reads stay inside the window.
	g_xEngine.FluxBackend().RecordFrame(xWorkDistribution);
	m_bHasRenderWork = true;

	// The backend has finished recording (command buffers retain the recorded
	// commands until EndFrame submits them); the queue entries are no longer
	// needed and the next frame's passes will be queued fresh.
	ClearPendingRenderPasses();
}

void Flux_RendererImpl::ClearPendingRenderPasses()
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(),
		"ClearPendingRenderPasses: main-thread only");
	m_xPendingRenderPasses.Clear();
}

Flux_RenderGraph& Flux_RendererImpl::GetRenderGraph()    { return *m_pxRenderGraph; }
bool              Flux_RendererImpl::IsRenderGraphValid(){ return m_pxRenderGraph != nullptr; }

void Flux_RendererImpl::RequestGraphRebuild() { m_bGraphRebuildRequested = true; }

// Phase 2: rebuild the uncullled scene snapshot for this frame via the EC fill seam and
// stamp the render-mutation epoch (passed in by the caller — keeps this off g_xEngine).
// Called once per frame from Zenith_Core.cpp before SetRenderTasksActive(true), so it
// runs on the main thread and every render-pass Prepare that derives from it sees the
// freshly built list.
Flux_RendererImpl::~Flux_RendererImpl()
{
	// Backstop free for the by-ptr snapshot (Shutdown's early free nulls it first when it
	// runs; headless never calls Shutdown, so this is the only free there). delete-null safe.
	delete m_pxSceneSnapshot;
	m_pxSceneSnapshot = nullptr;
}

void Flux_RendererImpl::RebuildSceneSnapshot(uint64_t uRenderMutationEpoch, const Zenith_Maths::Matrix4& xCameraViewProj, bool bCameraValid)
{
	m_pxSceneSnapshot->Rebuild(g_pfnZenithSceneSnapshotFill, uRenderMutationEpoch);
	// Phase 3: stamp the frame's camera frustum so the geometry consumers camera-cull
	// against it without reaching FluxGraphics themselves — but ONLY when the camera
	// actually resolved this frame. Rebuild cleared the frustum-valid flag, so an invalid
	// camera frame leaves it unset and the consumers cull nothing (vs culling every off-
	// origin object against an identity/stale view-proj).
	if (bCameraValid)
	{
		m_pxSceneSnapshot->SetCameraFrustum(xCameraViewProj);
	}
}
bool Flux_RendererImpl::ConsumeGraphRebuildRequest()
{
	bool b = m_bGraphRebuildRequested;
	m_bGraphRebuildRequested = false;
	return b;
}

Zenith_Vector<Flux_RenderPassEntry>& Flux_RendererImpl::GetPendingRenderPasses()
{
	return m_xPendingRenderPasses;
}

// Debug-variable backing store for the transient-aliasing runtime toggle.
// Synced into the render graph at each SetupRenderGraph via SetAliasingEnabled;
// changes trigger MarkDirty so the next Compile rebuilds the pool layout.
DEBUGVAR bool dbg_bTransientAliasing = true;

// (Flux_FinalLayoutTransitionNoOp moved to Flux_FeatureRegistry.cpp, where the
// final-RT layout-transition setup step that uses it now lives.)

void Flux_PipelineHelper::BuildFullscreenPipeline(
	Flux_Shader& xShader,
	Flux_Pipeline& xPipeline,
	const Flux_ShaderDecl& xDecl,
	TextureFormat eColourFormat,
	TextureFormat eDepthStencilFormat)
{
	Flux_PipelineSpecification xSpec = CreateFullscreenSpec(xShader, xDecl, eColourFormat, eDepthStencilFormat);
	Flux_PipelineBuilder::FromSpecification(xPipeline, xSpec);
}

Flux_PipelineSpecification Flux_PipelineHelper::CreateFullscreenSpec(
	Flux_Shader& xShader,
	const Flux_ShaderDecl& xDecl,
	TextureFormat eColourFormat,
	TextureFormat eDepthStencilFormat)
{
	// Single-RTV is the N==1 case. Taking the address of the by-value param
	// yields a valid 1-element array for the duration of this call.
	return CreateFullscreenSpecMRT(xShader, xDecl, &eColourFormat, 1u, eDepthStencilFormat);
}

Flux_PipelineSpecification Flux_PipelineHelper::CreateFullscreenSpecMRT(
	Flux_Shader& xShader,
	const Flux_ShaderDecl& xDecl,
	const TextureFormat* aeColourFormats,
	u_int uNumColourAttachments,
	TextureFormat eDepthStencilFormat)
{
	Zenith_Assert(uNumColourAttachments >= 1 && uNumColourAttachments <= FLUX_MAX_TARGETS,
		"CreateFullscreenSpecMRT: uNumColourAttachments %u out of range [1, %u]", uNumColourAttachments, FLUX_MAX_TARGETS);

	xShader.Initialise(xDecl);

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
	// (e.g. forward translucency) override this explicitly.
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

#if defined(ZENITH_WINDOWS) && defined(ZENITH_VULKAN)
	// Initialize Slang compiler before any shader loading. Slang is the Vulkan
	// SPIR-V toolchain; the D3D12 null backend loads pre-baked reflection only.
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

#ifdef ZENITH_TOOLS
	// ImGui depends only on the Vulkan device + swapchain format (see
	// Zenith_Vulkan::InitialiseImGuiRenderPass), not on any registry feature
	// (FluxGraphics included), so bringing it up before the feature walk is
	// dependency-safe. Gizmos (which DOES depend on ImGui) is registered as a
	// feature and initialised by the walk.
	g_xEngine.FluxBackend().InitialiseImGui();
#endif

	// The per-subsystem Initialise() ladder walks the Flux_FeatureRegistry in
	// registration order. FluxGraphics is registered FIRST, so the walk brings it
	// up before every dependent feature — it used to be a separate inline init,
	// but it is now an ordinary RegisterFeature<> entry like everything else. The
	// dependency rationale documented at the top of this function is encoded as
	// the registration order in Flux_FeatureRegistry.cpp::RegisterDefaultFeatures().
	Flux_FeatureRegistry::RegisterDefaultFeatures();

	// Catalog<->feature parity. Backend-independent; runs in ALL configs right
	// after registration so a forgotten catalog include or RegisterFeature line
	// fails loudly at boot instead of as a silent stale/missing-shader bug.
	{
		std::string strParityErr;
		const bool bParity = Flux_ShaderCatalog::ValidateFeatureParity(Flux_FeatureRegistry::Get(), strParityErr);
		Zenith_Assert(bParity, "Flux shader catalog/feature parity failed: %s", strParityErr.c_str());
	}

	const Flux_FeatureRegistry& xRegistry = Flux_FeatureRegistry::Get();
	for (u_int uFeature = 0; uFeature < xRegistry.GetNumFeatures(); uFeature++)
	{
		const Flux_FeatureDesc& xDesc = xRegistry.GetFeatures()[uFeature];
		if (xDesc.m_pfnInitialise != nullptr)
			xDesc.m_pfnInitialise();
	}

#ifdef ZENITH_TOOLS
	// Hot-reload: derive every engine shader program's rebuild callback from the
	// feature registry — no per-subsystem RegisterSubsystem needed. Runs after the
	// feature table is populated (BuildPipelines trampolines exist) and after the
	// watcher came up above. Game-side / non-feature owners register afterwards.
	Flux_ShaderHotReload::AutoRegisterFeatures();
#endif

	// Drain the GPU uploads staged by swapchain init + the feature walk above.
	g_xEngine.FluxMemory().Flush();

	// Create and compile the render graph
	m_pxRenderGraph = new Flux_RenderGraph();

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
	g_xEngine.DebugVariables().AddTextureCallback({ "Flux",   "HDR",   "Textures", "HDRScene"  }, [](){ return g_xEngine.FluxGraphics().GetDebugSRV_HDRScene(); });
	g_xEngine.DebugVariables().AddTextureCallback({ "Flux",   "HDR",   "Textures", "BloomMip0" }, [](){ return g_xEngine.HDR().GetDebugSRV_Bloom0(); });
	g_xEngine.DebugVariables().AddTextureCallback({ "Flux",   "HDR",   "Textures", "BloomMip1" }, [](){ return g_xEngine.HDR().GetDebugSRV_Bloom1(); });
	g_xEngine.DebugVariables().AddTextureCallback({ "Flux",   "HDR",   "Textures", "BloomMip2" }, [](){ return g_xEngine.HDR().GetDebugSRV_Bloom2(); });
#endif

	// Initialise any game render features registered BEFORE Flux came up (their
	// Initialise was deferred since the graph wasn't valid yet). Runs after the
	// graph exists and before the first SetupRenderGraph walk so their setup is
	// picked up immediately. No-op for the common late-registration case (games
	// that register during Project init, after this point — those initialise on
	// Register and request a rebuild).
	Zenith_GameRenderFeatures::InitialiseAllPending();

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
	// Skybox transmittance/sky-view LUT enables. Must run BEFORE IBL's (and after
	// any graph-dirtying system above) so a dirty compile force-enables the LUT
	// writers the "Skybox" pass reads — same MarkDirty-propagation reason as IBL.
	g_xEngine.Skybox().UpdateGraphPassEnables(xGraph);
	g_xEngine.IBL().UpdateGraphPassEnables(xGraph);
}

void Flux_RendererImpl::SyncRenderGraphDebugToggles()
{
	if (m_pxRenderGraph == nullptr)
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
	m_pxRenderGraph->SetAliasingEnabled(dbg_bTransientAliasing);
#endif
}

void Flux_RendererImpl::SetupRenderGraph()
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(),
		"SetupRenderGraph: must run on the main thread; the pending render-pass queue is accessed without locking here.");

	// Clear the queued render passes first — they hold pointers to the graph's
	// passes which will be destroyed by Clear(). Caller must have already drained the GPU.
	ClearPendingRenderPasses();
	Flux_RendererImpl& xRenderer = g_xEngine.FluxRenderer();
	xRenderer.m_pxRenderGraph->Clear();

	// Sync the transient-aliasing debug toggle into the graph. SetAliasingEnabled
	// is a no-op if the value is unchanged; on change it calls MarkDirty so the
	// next Compile rebuilds the pool layout.
	SyncRenderGraphDebugToggles();

	// Single ordered setup walk — there are NO discrete render phases. The render
	// graph computes pass execution order by topologically sorting each pass's
	// declared Reads/Writes; the walk's declaration order seeds that sort where it
	// matters — producers must precede consumers (a reader links only to an
	// earlier-declared writer of the same resource) and same-resource writers run
	// in declaration order (see the ORDERING note in Flux_FeatureRegistry.h). The
	// walk (built in Flux_FeatureRegistry::RegisterDefaultFeatures) folds the former
	// inline irregulars — FluxGraphics/HDR transient creation, the post-fog game
	// hook, and the final-RT layout-transition
	// pass — in as ordinary ordered steps at their exact prior positions, so the
	// compiled order is unchanged.
	Flux_FeatureRegistry::Get().RunSetup(*xRenderer.m_pxRenderGraph);

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

	// Phase 2: free the snapshot (drops its non-owning Flux_ModelInstance* entries) so no
	// stale pointer survives into a fresh renderer on engine reinit.
	delete xRenderer.m_pxSceneSnapshot;
	xRenderer.m_pxSceneSnapshot = nullptr;

	// Shut down any still-registered game render features (reverse registration
	// order) AFTER the graph is gone, so a feature's Shutdown can't touch a live
	// graph — but while the Vulkan device / FluxBackend are still up, so
	// device-touching resource deletes in a feature Shutdown are safe (matches
	// where the engine features shut down below). Games that explicitly Unregister
	// in their own teardown already ran their Shutdown; this is the backstop.
	Zenith_GameRenderFeatures::ShutdownAll();

	// Clear res-change callbacks so OnResChange has nothing to invoke — the
	// callbacks would otherwise deref the now-null m_pxRenderGraph and crash.
	xRenderer.m_xResChangeCallbacks.Clear();

	// Shut down every Flux feature in REVERSE registration order. RunShutdown now
	// covers ALL features — including FluxGraphics, HDR, Gizmos and MaterialPreview,
	// which used to be torn down inline here — because no feature's Shutdown reads
	// another feature (only foundation + own state, verified), so reverse-of-init is
	// a correct teardown order with no hand-tuning. Features with no Shutdown (Fog —
	// RAII / stateless) are skipped. Only the NON-feature teardown (Slang /
	// HotReload / ImGui / swapchain / memory) stays inline below; it runs after the
	// feature walk while the Vulkan device + memory manager are still up.
	Flux_FeatureRegistry::Get().RunShutdown();

#if defined(ZENITH_WINDOWS) && defined(ZENITH_VULKAN)
	Flux_SlangCompiler::Shutdown();
#endif

#ifdef ZENITH_TOOLS
	Flux_ShaderHotReload::Shutdown();
	g_xEngine.FluxBackend().ShutdownImGui();
#endif

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

	// The render graph queues passes in topological order — no sort needed here.
	Flux_RendererImpl& xRenderer = g_xEngine.FluxRenderer();

	const u_int uTotalPasses = xRenderer.m_xPendingRenderPasses.GetSize();
	xOutDistribution.uTotalPasses = uTotalPasses;
	if (uTotalPasses == 0)
	{
		return false;
	}

	// Distribute the passes across worker threads as contiguous, roughly-equal
	// index ranges. Topological order is preserved within and across workers
	// (worker i records a lower index range than worker i+1, and the worker
	// command buffers are submitted in order) — that ordering + the graph's
	// inline barriers is what enforces dependencies. Per-pass GPU cost is not
	// known here, so pass-count slicing is the cheap, balanced default.
	const u_int uTargetPassesPerThread = (uTotalPasses + FLUX_NUM_WORKER_THREADS - 1) / FLUX_NUM_WORKER_THREADS;
	u_int uCurrentThreadIndex = 0;

	xOutDistribution.auStartIndex[0] = 0;
	for (u_int uIndex = 0; uIndex < uTotalPasses; uIndex++)
	{
		const u_int uCountThisThread = uIndex - xOutDistribution.auStartIndex[uCurrentThreadIndex];
		if (uCountThisThread >= uTargetPassesPerThread &&
			uCurrentThreadIndex < FLUX_NUM_WORKER_THREADS - 1)
		{
			xOutDistribution.auEndIndex[uCurrentThreadIndex] = uIndex;
			uCurrentThreadIndex++;
			xOutDistribution.auStartIndex[uCurrentThreadIndex] = uIndex;
		}
	}

	Zenith_Assert(uCurrentThreadIndex < FLUX_NUM_WORKER_THREADS,
		"PrepareFrame: Thread index %u out of bounds (max %u)", uCurrentThreadIndex, FLUX_NUM_WORKER_THREADS);
	xOutDistribution.auEndIndex[uCurrentThreadIndex] = uTotalPasses;

	// Explicitly write empty (N, N) ranges for any worker that did not receive a
	// slice. The invariant is relied on by the consumer; writing it here
	// defensively protects against xOutDistribution.Clear() behaviour changing.
	for (u_int u = uCurrentThreadIndex + 1; u < FLUX_NUM_WORKER_THREADS; u++)
	{
		xOutDistribution.auStartIndex[u] = uTotalPasses;
		xOutDistribution.auEndIndex[u] = uTotalPasses;
	}

	return true;
}
