#pragma once

#include "Core/ZenithConfig.h"
#include "Collections/Zenith_Vector.h"
#include "Collections/Zenith_HashMap.h"
#include "Flux/Flux.h"            // Flux_RenderPassEntry, Flux_WorkDistribution, Flux_RenderGraph_AttachmentRef/Pass
#include "Flux/Flux_GPUScene.h"             // Flux_GPUSceneBucketRegistry / Flux_GPUSceneBuildResult (Stage 0)
#include "Flux/Flux_MeshGeometryRegistry.h" // Flux_MeshGeometryRegistry (Stage 0)
#include "Flux/UnifiedMesh/Flux_Skinning.h" // Flux_BonePaletteBuilder / Flux_GPUSkinJob (Stage 5)

class Flux_RenderGraph;
class Zenith_MeshAsset;    // Stage 5: skinned-pose store keyed by mesh asset
class Flux_MeshInstance;   // Stage 1: shared geometry resolved from the mesh-geometry registry for the unified draw
// Held by POINTER (forward-declared, heap-allocated in Zenith_Engine::AllocateRenderer /
// freed in Shutdown + the dtor backstop) rather than by value: the snapshot header pulls
// Flux_ModelInstance.h -> AssetHandle ->
// AssetRegistry -> Flux, which would close an include cycle back to this header. Forward-
// decl + by-ptr is the sanctioned header-decoupling pattern (NOT pimpl — the type is a
// public, fully-defined Flux type; this just breaks the include edge).
class Flux_RenderSceneSnapshot;

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
	// User-declared (defined in Flux.cpp where Flux_RenderSceneSnapshot is complete): frees
	// the by-ptr snapshot. The headless boot never calls Shutdown, so the dtor is the
	// backstop free on `delete m_pxFluxRenderer`. Safe alongside Shutdown's early free (nulls).
	~Flux_RendererImpl();

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

	// Queue one render-graph pass for backend recording. Only called from
	// Flux_RenderGraph::SubmitRecordedLists, sequentially on the main thread.
	void QueueRenderPass(const Flux_RenderGraph* pxGraph,
		const Flux_RenderGraph_AttachmentRef* axColourAttachments, uint32_t uNumColour,
		const Flux_RenderGraph_AttachmentRef& xDepthStencil,
		bool bClearTargets, bool bDepthIsReadOnly, const Flux_RenderGraph_Pass* pxPass);

	// Record + drain this frame's queued passes. Called synchronously from
	// Flux_RenderGraph::Execute (in the render-task safe window, before the
	// frame memory submit). Distributes the queued passes across worker threads
	// (PrepareFrame), drives the backend to record them directly into its worker
	// command buffers (FluxBackend().RecordFrame), then clears the queue. Sets
	// HasRecordedFrameWork() for the subsequent EndFrame submit.
	void RecordFrame();
	bool HasRecordedFrameWork() const { return m_bHasRenderWork; }

	// Prepare frame for rendering — distributes the queued passes across worker
	// threads. Returns false if there is no work to do.
	bool PrepareFrame(Flux_WorkDistribution& xOutDistribution);

	void AddResChangeCallback(void(*pfnCallback)());
	void OnResChange();

	// Clear all queued render passes. CALLER GUARANTEES that no worker thread
	// is currently recording (i.e. graph RecordFrame is not in flight) and that
	// the GPU has finished consuming the previous frame's command buffers.
	void ClearPendingRenderPasses();

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

	// Public access to the queued render passes for the platform layer.
	Zenith_Vector<Flux_RenderPassEntry>& GetPendingRenderPasses();

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

	// ===== Scene-graph snapshot (Phase 2) =====
	// The renderer OWNS the uncullled Flux_RenderSceneSnapshot and rebuilds it EXACTLY
	// ONCE per frame, triggered from Zenith_Core.cpp right before SetRenderTasksActive(true)
	// (after UI().Update() drains deferred scene loads + ImGui transform edits, so no entry
	// captures a stale/dangling Flux_ModelInstance*). The epoch is passed in explicitly
	// (Zenith_SceneSystem::GetRenderMutationEpoch()) so this class holds no g_xEngine reach;
	// the fill fn is the EC-defined g_pfnZenithSceneSnapshotFill. Reset() on Shutdown drops
	// the non-owning instance pointers so none survives a teardown/reinit.
	void RebuildSceneSnapshot(uint64_t uRenderMutationEpoch, const Zenith_Maths::Matrix4& xCameraViewProj, bool bCameraValid);
	const Flux_RenderSceneSnapshot& GetSceneSnapshot() const { return *m_pxSceneSnapshot; }

	// ===== Unified GPU-driven mesh scene =====
	// Built once per frame from the scene snapshot, on the main thread right after
	// RebuildSceneSnapshot: the (mesh,cull,material,VAT) bucket topology + the GPU-scene
	// object/draw-item record arrays. Consumed by the Flux_UnifiedMesh feature's cull/draw
	// passes (camera G-buffer + the shadow cascades) via GatherUnifiedPacket, which uploads the
	// records and dispatches the reset→cull→draw kernels. This is THE opaque static + instanced
	// mesh pipeline (Stage 4 retired the legacy per-object StaticMeshes/InstancedMeshes paths).
	void SyncUnifiedBucketsFromSnapshot();
	const Flux_GPUSceneBuildResult&    GetUnifiedGPUScene()       const { return m_xUnifiedGPUScene; }
	const Flux_GPUSceneBucketRegistry& GetUnifiedBucketRegistry() const { return m_xUnifiedBucketRegistry; }

	// Stage 1: resolve a bucket's shared mesh geometry (VB/IB built by the mesh-geometry
	// registry's real provider) for the per-bucket indirect draw. nullptr in id-only mode
	// (Stage 0) or on a build failure.
	Flux_MeshInstance* GetUnifiedMeshGeometry(u_int uMeshGeometryId) const
	{
		return static_cast<Flux_MeshInstance*>(m_xUnifiedMeshGeometryRegistry.GetBuilt(uMeshGeometryId));
	}

	// ===== Stage 5: compute-skinning data flow =====
	// Animated skinned meshes are compute-skinned + drawn through the unified path.

	// Per skinned bucket (indexed by the bucket's skinIndex = meshGeometryId low bits): the
	// arena slice base + the mesh whose index buffer the skinned draw binds. GatherUnifiedPacket
	// reads this to resolve a skinned bucket's draw.
	struct Flux_UnifiedSkinnedDraw
	{
		Flux_MeshInstance* m_pxMesh        = nullptr;  // IB + index count
		u_int              m_uVertexOffset = 0u;       // base vertex in the skinned arena (this instance's slice)
	};
	const Zenith_Vector<Flux_UnifiedSkinnedDraw>& GetUnifiedSkinnedDraws() const { return m_axUnifiedSkinnedDraws; }
	const Zenith_Vector<u_int>&           GetUnifiedBindPosePoolWords() const { return m_auUnifiedBindPosePoolWords; }
	const Zenith_Vector<Zenith_Maths::Matrix4>& GetUnifiedBonePalette() const { return m_xUnifiedBonePalette.Matrices(); }
	const Zenith_Vector<Flux_GPUSkinJob>& GetUnifiedSkinJobs() const { return m_axUnifiedSkinJobs; }
	u_int GetUnifiedSkinMaxVerts()      const { return m_uUnifiedSkinMaxVerts; }
	u_int GetUnifiedSkinTotalOutVerts() const { return m_uUnifiedSkinTotalOutVerts; }

	// ===== Data members =====

	// Render graph. Allocated in LateInitialise, freed in Shutdown.
	Flux_RenderGraph*                     m_pxRenderGraph = nullptr;

	// Queued render passes (filled by Flux_RenderGraph::SubmitRecordedLists,
	// drained by RecordFrame after the backend records them).
	Zenith_Vector<Flux_RenderPassEntry>   m_xPendingRenderPasses;

	// Set by RecordFrame each frame: true iff the backend recorded render work
	// this frame (non-empty queue). Read by the backend's EndFrame to decide
	// whether to submit render command buffers. Reset to false at frame begin.
	bool                                  m_bHasRenderWork = false;

	// Resolution-change callback list (subsystems register here at init).
	Zenith_Vector<void(*)()>              m_xResChangeCallbacks;

	// Graph rebuild request flag — consumed by next Compile().
	bool                                  m_bGraphRebuildRequested = false;

	// Phase 2: the uncullled master scene snapshot. Owned by pointer (heap-allocated in
	// Zenith_Engine::AllocateRenderer — unconditionally, so the headless boot is safe — and
	// freed in Shutdown + the dtor backstop) to break the snapshot-header include cycle (see
	// the forward-decl note above). Rebuilt once per frame via RebuildSceneSnapshot; injected
	// (by pointer) into the geometry consumers at the composition root.
	Flux_RenderSceneSnapshot*             m_pxSceneSnapshot = nullptr;

	// Unified GPU-driven mesh scene. All by value (light, default-constructed with the impl).
	// The mesh-geometry registry's real provider (built shared VB/IB) is wired in
	// LateInitialise; the Flux_UnifiedMesh feature owns the per-bucket GPU buffers.
	Flux_MeshGeometryRegistry             m_xUnifiedMeshGeometryRegistry;
	Flux_GPUSceneBucketRegistry           m_xUnifiedBucketRegistry;
	Flux_GPUSceneBuildResult              m_xUnifiedGPUScene;

	// ===== Stage 5: compute-skinning state =====
	// Persistent per-distinct-skinned-mesh cache (keyed by Zenith_MeshAsset*): the bind-pose
	// vertices as raw words (compute input) + the shared mesh instance for IB/counts. Built lazily
	// (GetOrBuildSkinnedPose) and freed in Shutdown.
	struct Flux_SkinnedPoseEntry
	{
		Zenith_Vector<u_int> m_auBindPoseWords;   // 104B-per-vertex interleaved (uFLUX_SKIN_INPUT_WORDS/vert)
		Flux_MeshInstance*   m_pxMesh    = nullptr; // CreateSkinnedFromAsset → IB + index count + bounds
		u_int                m_uNumVerts = 0u;
	};
	Flux_SkinnedPoseEntry* GetOrBuildSkinnedPose(Zenith_MeshAsset* pxAsset);  // defined in Flux.cpp

	Zenith_Vector<Flux_SkinnedPoseEntry*> m_axUnifiedSkinnedPoseStore;        // owned entries
	Zenith_HashMap<const void*, u_int>    m_xUnifiedSkinnedPoseByAsset;       // asset -> store index

	// Per-frame skin-build state (rebuilt each SyncUnifiedBucketsFromSnapshot, read by GatherUnifiedPacket).
	Flux_BonePaletteBuilder               m_xUnifiedBonePalette;             // dedup skeletons -> concatenated palette
	Zenith_Vector<u_int>                  m_auUnifiedBindPosePoolWords;      // concatenated bind-pose words for this frame's jobs
	Zenith_HashMap<const void*, u_int>    m_xUnifiedBindPosePoolBaseByMesh;  // asset -> base VERTEX in the pool (this frame)
	Zenith_Vector<Flux_GPUSkinJob>        m_axUnifiedSkinJobs;               // one per animated submesh-instance
	Zenith_Vector<Flux_UnifiedSkinnedDraw> m_axUnifiedSkinnedDraws;          // per skinned bucket (by skinIndex)
	u_int m_uUnifiedSkinMaxVerts      = 0u;   // largest job's vertex count (skinning dispatch X)
	u_int m_uUnifiedSkinTotalOutVerts = 0u;   // arena vertices used this frame

	// Unit tests inspect private state.
	friend class Zenith_UnitTests;
};
