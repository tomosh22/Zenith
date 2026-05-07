#include "Zenith.h"

#include "Flux/Flux.h"
#include "Flux/Flux_PerFrame.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Skybox/Flux_Skybox.h"
#include "Flux/StaticMeshes/Flux_StaticMeshes.h"
#include "Flux/AnimatedMeshes/Flux_AnimatedMeshes.h"
#include "Flux/Terrain/Flux_Terrain.h"
#ifdef ZENITH_WINDOWS
#include "Flux/Slang/Flux_SlangCompiler.h"
#endif
#ifdef ZENITH_TOOLS
#include "Flux/Gizmos/Flux_Gizmos.h"
#include "Flux/Slang/Flux_ShaderHotReload.h"
#endif
#include "Flux/Primitives/Flux_Primitives.h"
#include "Flux/DeferredShading/Flux_DeferredShading.h"
#include "Flux/SSAO/Flux_SSAO.h"
#include "Flux/Fog/Flux_Fog.h"
#include "Flux/Fog/Flux_VolumeFog.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "Flux/SDFs/Flux_SDFs.h"
#include "Flux/Shadows/Flux_Shadows.h"
#include "Flux/Particles/Flux_Particles.h"
#include "Flux/Text/Flux_Text.h"
#include "Flux/Quads/Flux_Quads.h"
#include "Flux/InstancedMeshes/Flux_InstancedMeshes.h"
#include "Flux/HDR/Flux_HDR.h"
#include "Flux/IBL/Flux_IBL.h"
#include "Flux/HiZ/Flux_HiZ.h"
#include "Flux/SSR/Flux_SSR.h"
#include "Flux/SSGI/Flux_SSGI.h"
#include "Flux/Vegetation/Flux_Grass.h"
#include "Flux/DynamicLights/Flux_DynamicLights.h"
#include "Flux/DynamicLights/Flux_LightClustering.h"
#include "Flux/Decals/Flux_Decals.h"
#include "DebugVariables/Zenith_DebugVariables.h"

uint32_t Flux::s_uFrameCounter = 0;
Zenith_Vector<void(*)()> Flux::s_xResChangeCallbacks;
Zenith_Vector<Flux_CommandListEntry> Flux::s_xPendingCommandLists;
Flux_RenderGraph* Flux::s_pxRenderGraph = nullptr;
bool Flux::s_bGraphRebuildRequested = false;

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
	xShader.Initialise(eProgram);

	Flux_PipelineSpecification xSpec;
	xSpec.m_aeColourAttachmentFormats[0] = eColourFormat;
	xSpec.m_uNumColourAttachments = 1;
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

void Flux::EarlyInitialise()
{
	// Flux_PerFrame must initialise BEFORE backend Initialise so backends can
	// register their begin/end-frame callbacks during their own setup. The
	// counter starts at 0 and the ring index is correctly defined from the
	// first call.
	Flux_PerFrame::Initialise();

	Flux_PlatformAPI::Initialise();
	Flux_MemoryManager::Initialise();
	Flux_PlatformAPI::InitialiseScratchBuffers(); // Must be after memory manager init
	Flux_Graphics::InitialiseSamplers(); // Must be before any CreateShaderResourceView calls (bindless registration)
}

void Flux::LateInitialise()
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

	Flux_MemoryManager::BeginFrame();

#ifdef ZENITH_WINDOWS
	// Initialize Slang compiler before any shader loading
	Flux_SlangCompiler::Initialise();
	// Tell the modern session API where to resolve `loadModule` paths from.
	// Required for FluxShaderProgram-based runtime compilation; the legacy
	// per-file paths embed their own search root via spAddSearchPath.
	Flux_SlangCompiler::AddSearchPath(SHADER_SOURCE_ROOT);
#endif

	Flux_Swapchain::Initialise();

#ifdef ZENITH_TOOLS
	// Bring up the hot-reload watcher BEFORE any subsystem Initialise() so
	// the RegisterSubsystem calls inside those Initialise bodies see a live
	// watcher. (The static registration list is also safe to append to
	// before Initialise, but starting the file watcher early means the
	// `firing N rebuild callback(s)` log line picks up edits made even
	// during engine boot.)
	Flux_ShaderHotReload::Initialise();
#endif

	Flux_Graphics::Initialise();
	Flux_HDR::Initialise();  // Must be before DeferredShading - deferred renders to HDR target
#ifdef ZENITH_TOOLS
	Flux_PlatformAPI::InitialiseImGui();
	Flux_Gizmos::Initialise();
#endif
	Flux_Shadows::Initialise();
	Flux_Skybox::Initialise();       // Cubemap skybox + procedural atmosphere
	Flux_IBL::Initialise();          // Image-based lighting (BRDF LUT, environment probes)
	Flux_StaticMeshes::Initialise();
	Flux_AnimatedMeshes::Initialise();
	Flux_InstancedMeshes::Initialise();
	Flux_Terrain::Initialise();
	Flux_Grass::Initialise();        // Grass/vegetation (after terrain)
	Flux_Primitives::Initialise();
	Flux_HiZ::Initialise();          // Hi-Z depth pyramid (needed by SSR)
	Flux_SSR::Initialise();          // Screen-space reflections (uses Hi-Z, needed by DeferredShading)
	Flux_SSGI::Initialise();         // Screen-space GI (uses Hi-Z, needed by DeferredShading)
	Flux_DynamicLights::Initialise();   // Light gather + upload (front-end for clustered deferred)
	Flux_LightClustering::Initialise(); // Per-cluster light culling compute (must precede DeferredShading)
	Flux_DeferredShading::Initialise(); // Reads cluster buffers + light buffer in fragment shader
	Flux_Decals::Initialise();          // Deferred screen-space box decals (writes G-buffer pre-readers)
	Flux_SSAO::Initialise();
	Flux_Fog::Initialise();
	Flux_SDFs::Initialise();
	Flux_Particles::Initialise();
	Flux_Quads::Initialise();
	Flux_Text::Initialise();
	Flux_MemoryManager::EndFrame(false);

	// Create and compile the render graph
	s_pxRenderGraph = new Flux_RenderGraph();

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
	Zenith_DebugVariables::AddBoolean({ "Render", "RenderGraph", "Transient Aliasing" }, dbg_bTransientAliasing);

	// Click-to-log button: prints the compiled render-graph pass order. Useful
	// for newcomers asking "what runs when?" — the answer is the topological
	// sort of the pass DAG, not the order of AddPass() calls in SetupRenderGraph.
	Zenith_DebugVariables::AddButton({ "Render", "RenderGraph", "Print Pass Order" }, []() {
		if (Flux::s_pxRenderGraph != nullptr)
		{
			const std::string strOrder = Flux::s_pxRenderGraph->GetPassOrderDescription();
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
	Zenith_DebugVariables::AddTextureCallback({ "Render", "Debug", "MRT Diffuse" },       &Flux_Graphics::GetDebugSRV_MRTDiffuse);
	Zenith_DebugVariables::AddTextureCallback({ "Render", "Debug", "MRT NormalsAO" },     &Flux_Graphics::GetDebugSRV_MRTNormalsAO);
	Zenith_DebugVariables::AddTextureCallback({ "Render", "Debug", "MRT Material" },      &Flux_Graphics::GetDebugSRV_MRTMaterial);
	Zenith_DebugVariables::AddTextureCallback({ "Render", "Debug", "Depth" },             &Flux_Graphics::GetDebugSRV_Depth);
	// HDR textures follow Flux_HDR.cpp's established "Flux/HDR/..." convention.
	Zenith_DebugVariables::AddTextureCallback({ "Flux",   "HDR",   "Textures", "HDRScene"  }, &Flux_HDR::GetDebugSRV_HDRScene);
	Zenith_DebugVariables::AddTextureCallback({ "Flux",   "HDR",   "Textures", "BloomMip0" }, &Flux_HDR::GetDebugSRV_Bloom0);
	Zenith_DebugVariables::AddTextureCallback({ "Flux",   "HDR",   "Textures", "BloomMip1" }, &Flux_HDR::GetDebugSRV_Bloom1);
	Zenith_DebugVariables::AddTextureCallback({ "Flux",   "HDR",   "Textures", "BloomMip2" }, &Flux_HDR::GetDebugSRV_Bloom2);
#endif

	SetupRenderGraph();
	AddResChangeCallback(SetupRenderGraph);
}

void Flux::SyncRenderGraphDebugToggles()
{
	if (s_pxRenderGraph == nullptr)
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
	s_pxRenderGraph->SetAliasingEnabled(dbg_bTransientAliasing);
#endif
}

void Flux::SetupRenderGraph()
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(),
		"SetupRenderGraph: must run on the main thread; pending command lists are accessed without locking here.");

	// Clear pending command lists first — they hold pointers to the graph's command lists
	// which will be destroyed by Clear(). Caller must have already drained the GPU.
	ClearPendingCommandLists();
	s_pxRenderGraph->Clear();

	// Sync the transient-aliasing debug toggle into the graph. SetAliasingEnabled
	// is a no-op if the value is unchanged; on change it calls MarkDirty so the
	// next Compile rebuilds the pool layout.
	SyncRenderGraphDebugToggles();

	// Core render targets FIRST — every other subsystem depends on these.
	Flux_Graphics::SetupTransients(*s_pxRenderGraph);
	Flux_HDR::SetupTransients(*s_pxRenderGraph); // HDR scene target used by many subsystems

	// Preprocessing
	Flux_IBL::SetupRenderGraph(*s_pxRenderGraph);
	Flux_Skybox::SetupRenderGraph(*s_pxRenderGraph);

	// Geometry (all write to G-Buffer + Depth)
	Flux_Shadows::SetupRenderGraph(*s_pxRenderGraph);
	Flux_StaticMeshes::SetupRenderGraph(*s_pxRenderGraph);
	Flux_Terrain::SetupRenderGraph(*s_pxRenderGraph);
	Flux_Primitives::SetupRenderGraph(*s_pxRenderGraph);
	Flux_AnimatedMeshes::SetupRenderGraph(*s_pxRenderGraph);
	Flux_InstancedMeshes::SetupRenderGraph(*s_pxRenderGraph);
	Flux_Grass::SetupRenderGraph(*s_pxRenderGraph);

	// Decals (read G-Buffer + depth, write G-Buffer MRTs). Must be
	// registered after all G-buffer writers and before all G-buffer
	// readers so the topo sort places it correctly. Grass doesn't write
	// the G-buffer, so its position relative to decals is irrelevant.
	Flux_Decals::SetupRenderGraph(*s_pxRenderGraph);

	// Screen-space effects
	Flux_HiZ::SetupRenderGraph(*s_pxRenderGraph);
	Flux_SSR::SetupRenderGraph(*s_pxRenderGraph);
	Flux_SSGI::SetupRenderGraph(*s_pxRenderGraph);

	// Lighting & composition
	// Clustering runs first — its outputs (per-cluster light index lists) are
	// read by the deferred-shading fragment shader. The graph orders these via
	// .ReadsBuffer / .WritesBuffer declarations, but registering in this order
	// keeps the source-side intent explicit.
	Flux_LightClustering::SetupRenderGraph(*s_pxRenderGraph);
	Flux_DeferredShading::SetupRenderGraph(*s_pxRenderGraph);
	// Aerial perspective runs after DeferredShading — it blends scattering on
	// top of the already-lit HDR scene. Registering here keeps the writer-chain
	// topological order correct.
	Flux_Skybox::SetupAerialPerspectiveRenderGraph(*s_pxRenderGraph);
	Flux_SSAO::SetupRenderGraph(*s_pxRenderGraph);
	Flux_Fog::SetupRenderGraph(*s_pxRenderGraph);

	// Post-processing
	Flux_SDFs::SetupRenderGraph(*s_pxRenderGraph);
	Flux_Particles::SetupRenderGraph(*s_pxRenderGraph);
	Flux_HDR::SetupRenderGraph(*s_pxRenderGraph);

	// UI & presentation
	Flux_Quads::SetupRenderGraph(*s_pxRenderGraph);
	Flux_Text::SetupRenderGraph(*s_pxRenderGraph);
#ifdef ZENITH_TOOLS
	Flux_Gizmos::SetupRenderGraph(*s_pxRenderGraph);
#endif

	// Final-layout transition pass — leaves the Final Render Target in
	// SHADER_READ_ONLY_OPTIMAL so the swapchain copy command buffer (which
	// lives outside the render graph) can sample it without a layout
	// mismatch. The pass has no commands and no target setup; it exists
	// purely so the graph emits a prologue barrier transitioning the Final
	// RT from WRITE_RTV (set by tonemap / last writer) to READ_SRV.
	s_pxRenderGraph->AddPass("Final RT Layout Transition", Flux_FinalLayoutTransitionNoOp)
		.Reads(Flux_Graphics::GetFinalRenderTarget(), RESOURCE_ACCESS_READ_SRV);

	// Clear() already left the graph dirty — no explicit MarkDirty() needed.
}

void Flux::ReleaseAssetReferences()
{
	// Drop refs to Flux-side assets so Zenith_AssetRegistry::Shutdown can delete them
	// cleanly. Each subsystem releases the handles it owns.
	Flux_Graphics::ReleaseAssetReferences();
	Flux_Text::ReleaseAssetReferences();
	Flux_Particles::ReleaseAssetReferences();
	Flux_Terrain::ReleaseAssetReferences();
	Flux_Skybox::ReleaseAssetReferences();
	Flux_VolumeFog::ReleaseAssetReferences();

	// Material defaults live in AssetHandling but are part of the same pre-registry
	// release window — this is the natural place to drop them.
	Zenith_MaterialAsset::ReleaseDefaults();
}

void Flux::Shutdown()
{
	delete s_pxRenderGraph;
	s_pxRenderGraph = nullptr;
	// Clear res-change callbacks so OnResChange has nothing to invoke — the
	// callbacks would otherwise deref the now-null s_pxRenderGraph and crash.
	s_xResChangeCallbacks.Clear();

	// Shutdown Flux subsystems in REVERSE order of initialization
	// This ensures dependencies are destroyed after their dependents
	// NOTE: Some subsystems (Fog, DeferredShading, AnimatedMeshes, StaticMeshes)
	// don't have Shutdown() methods - they rely on RAII or are stateless
	Flux_Text::Shutdown();
	Flux_Quads::Shutdown();
	Flux_Particles::Shutdown();
	Flux_SDFs::Shutdown();
	// Flux_Fog, Flux_DeferredShading - no Shutdown() methods
	Flux_SSAO::Shutdown();           // SSAO render targets
	Flux_Decals::Shutdown();          // Deferred decal renderer (frees instance buffer + IB)
	Flux_LightClustering::Shutdown(); // Cluster compute pass (frees cluster buffers)
	Flux_DynamicLights::Shutdown();   // Light gather front-end (frees unified light buffer)
	Flux_SSGI::Shutdown();         // Before HiZ (SSGI uses Hi-Z)
	Flux_SSR::Shutdown();          // Before HiZ (SSR uses Hi-Z)
	Flux_HiZ::Shutdown();          // Hi-Z depth pyramid
	Flux_Primitives::Shutdown();   // Debug primitives (reverse of init: between HiZ and Grass)
	Flux_Grass::Shutdown();        // After Terrain (depends on terrain data)
	Flux_Terrain::Shutdown();
	Flux_InstancedMeshes::Shutdown();
	// Flux_AnimatedMeshes, Flux_StaticMeshes - no Shutdown() methods
	Flux_IBL::Shutdown();          // After Skybox (uses skybox for environment)
	Flux_Skybox::Shutdown();
	Flux_Shadows::Shutdown();

#ifdef ZENITH_WINDOWS
	Flux_SlangCompiler::Shutdown();
#endif

#ifdef ZENITH_TOOLS
	Flux_ShaderHotReload::Shutdown();
	Flux_Gizmos::Shutdown();
	Flux_PlatformAPI::ShutdownImGui();
#endif

	// HDR must shutdown after other render systems that target HDR buffer
	Flux_HDR::Shutdown();

	// Shutdown core graphics (render targets, depth buffer, quad mesh, frame constants)
	Flux_Graphics::Shutdown();

	// Shutdown memory manager (VMA allocator, handle registries)
	Flux_MemoryManager::Shutdown();

	// Clear PerFrame callback arrays so a subsequent Flux::Initialise starts
	// from a known empty state (matters for unit tests that re-init Flux
	// within one process). Frame counter is intentionally left alone.
	Flux_PerFrame::Shutdown();

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux shut down");
}

void Flux::OnResChange()
{
	for (u_int i = 0; i < s_xResChangeCallbacks.GetSize(); i++)
	{
		s_xResChangeCallbacks.Get(i)();
	}
}

bool Flux::PrepareFrame(Flux_WorkDistribution& xOutDistribution)
{
	static_assert(FLUX_NUM_WORKER_THREADS > 0, "FLUX_NUM_WORKER_THREADS must be positive");

	xOutDistribution.Clear();

	// The render graph submits command lists in topological order — no sort needed here.

	// Count total commands
	for (u_int i = 0; i < s_xPendingCommandLists.GetSize(); i++)
	{
		xOutDistribution.uTotalCommandCount += s_xPendingCommandLists.Get(i).m_pxCmdList->GetCommandCount();
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

	for (u_int uIndex = 0; uIndex < s_xPendingCommandLists.GetSize(); uIndex++)
	{
		const u_int uCommandCount = s_xPendingCommandLists.Get(uIndex).m_pxCmdList->GetCommandCount();

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
		xOutDistribution.auEndIndex[uCurrentThreadIndex] = s_xPendingCommandLists.GetSize();
	}

	// Explicitly write empty (N, N) ranges for any worker that did not receive
	// a command-list slice. The invariant is relied on by the consumer; writing
	// it here defensively protects against xOutDistribution.Clear() behaviour
	// changing in future.
	const u_int uPendingSize = s_xPendingCommandLists.GetSize();
	for (u_int u = uCurrentThreadIndex + 1; u < FLUX_NUM_WORKER_THREADS; u++)
	{
		xOutDistribution.auStartIndex[u] = uPendingSize;
		xOutDistribution.auEndIndex[u] = uPendingSize;
	}

	return true;
}
