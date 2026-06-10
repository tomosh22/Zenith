#pragma once

#include "Core/ZenithConfig.h"
#include "Collections/Zenith_Vector.h"
#include "Flux/Flux.h"            // Flux_CommandListEntry, Flux_WorkDistribution, Flux_RenderGraph_AttachmentRef/Pass, Flux_CommandList

class Flux_RenderGraph;

// Per-Engine state + behaviour for the Flux renderer. Replaces the two
// public static-facade classes that used to live in Flux.h / Flux_PerFrame.h:
//   - `class Flux`         (render graph pointer, pending command-list queue,
//                           resolution-change callback list, graph rebuild flag)
//   - `class Flux_PerFrame` (per-frame begin/end work dispatch)
//
// The monotonic frame counter that used to live here moved to FrameContext
// (g_xEngine.Frame()) — the single frame-index variable engine-wide, advanced
// only by Zenith_MainLoop.
//
// Accessed via g_xEngine.FluxRenderer(). The historical separation between
// `Flux::Initialise` (renderer bootstrap) and `Flux_PerFrame::Initialise`
// (per-frame ring bootstrap) is preserved via paired method names —
// `Initialise`/`Shutdown` for the main renderer, `PerFrameInitialise`/
// `PerFrameShutdown` for the per-frame ring scheduler.
class Flux_RendererImpl
{
public:
	Flux_RendererImpl() = default;
	~Flux_RendererImpl() = default;

	Flux_RendererImpl(const Flux_RendererImpl&) = delete;
	Flux_RendererImpl& operator=(const Flux_RendererImpl&) = delete;

	// ===== Main renderer lifecycle (was class Flux) =====
	void EarlyInitialise();
	void LateInitialise();

	// Release all asset-system references held by Flux statics (texture/material
	// handles in Flux_Graphics, Flux_Text, Flux_Particles, Flux_Terrain,
	// Flux_VolumeFog, plus Zenith_MaterialAsset's own defaults). Called between
	// Project_Shutdown and Zenith_AssetRegistry::Shutdown so handles release
	// while the registry still owns its assets.
	void ReleaseAssetReferences();

	void Shutdown();

	// Submit a command list for Vulkan recording. Only called from
	// Flux_RenderGraph::Execute Phase 2, sequentially on the main thread.
	void SubmitCommandList(const Flux_CommandList* pxCmdList,
		const Flux_RenderGraph_AttachmentRef* axColourAttachments, uint32_t uNumColour,
		const Flux_RenderGraph_AttachmentRef& xDepthStencil,
		bool bClearTargets, bool bDepthIsReadOnly, const Flux_RenderGraph_Pass* pxPass);

	// Prepare frame for rendering — distributes work across worker threads.
	// Returns false if there is no work to do.
	bool PrepareFrame(Flux_WorkDistribution& xOutDistribution);

	void AddResChangeCallback(void(*pfnCallback)());
	void OnResChange();

	// Clear all pending command lists. CALLER GUARANTEES that no worker thread
	// is currently submitting (i.e. graph Execute is not in flight) and that
	// the GPU has finished consuming the previous frame's lists.
	void ClearPendingCommandLists();

	Flux_RenderGraph& GetRenderGraph();
	bool IsRenderGraphValid();
	void SetupRenderGraph();

	// Called every frame from Zenith_Core::ExecuteRenderGraph before Compile.
	// Forwards the current value of debug variables that the render graph
	// cares about into the graph via their setters.
	void SyncRenderGraphDebugToggles();

	// Called every frame from Zenith_Core::ExecuteRenderGraph before Compile.
	// Forwards per-subsystem runtime selections (Fog technique, SSR blur,
	// SSGI denoise, IBL pass enable set) into the graph. Each call may
	// SetPassEnabled / MarkDirty so that editor-toggle changes take effect
	// on the same frame. Order is load-bearing: Fog before IBL, SSR/SSGI
	// before IBL — see the function body for the MarkDirty-propagation
	// rationale.
	void ApplySubsystemGraphSelections(Flux_RenderGraph& xGraph);

	// Request a full graph rebuild (Clear + SetupRenderGraph) at the start of
	// the next frame.
	void RequestGraphRebuild();
	bool ConsumeGraphRebuildRequest();

	// Public access to pending command lists for platform layer.
	Zenith_Vector<Flux_CommandListEntry>& GetPendingCommandLists();

	// ===== Per-frame ring scheduler (was class Flux_PerFrame) =====
	// Names are prefixed `PerFrame` where they would otherwise collide with
	// the main renderer's own Initialise/Shutdown.
	void PerFrameInitialise();
	void PerFrameShutdown();

	// BeginFrame issues the backend's per-frame begin work (fence wait, pool
	// reset, deletion-queue drain, scratch reset) for the current ring slot
	// (read from g_xEngine.Frame()). ProcessFrameEnd drives the deferred-VRAM-
	// deletion clock. Both are no-ops in headless (no backend). The frame index
	// itself is owned and advanced by FrameContext via Zenith_MainLoop — never
	// here, so a skipped frame can run ProcessFrameEnd without moving the ring.
	void BeginFrame();
	void ProcessFrameEnd();

	// ===== Data members =====

	// Render graph. Allocated in LateInitialise, freed in Shutdown.
	Flux_RenderGraph*                     m_pxRenderGraph = nullptr;

	// Pending command-list queue (filled by Phase 2 of graph execution,
	// drained by the platform layer).
	Zenith_Vector<Flux_CommandListEntry>  m_xPendingCommandLists;

	// Resolution-change callback list (subsystems register here at init).
	Zenith_Vector<void(*)()>              m_xResChangeCallbacks;

	// Graph rebuild request flag — consumed by next Compile().
	bool                                  m_bGraphRebuildRequested = false;

	// Unit tests inspect private state.
	friend class Zenith_UnitTests;
};
