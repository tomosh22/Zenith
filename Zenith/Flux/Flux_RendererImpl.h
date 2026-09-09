#pragma once

#include "Core/ZenithConfig.h"
#include "Collections/Zenith_Vector.h"
#include "Collections/Zenith_HashMap.h"
#include "Flux/Flux.h"            // Flux_RenderPassEntry, Flux_WorkDistribution, Flux_RenderGraph_AttachmentRef/Pass
#include "Flux/Flux_GPUScene.h"             // Flux_GPUSceneBucketRegistry / Flux_GPUSceneBuildResult (Stage 0)
#include "Flux/Flux_MeshGeometryRegistry.h" // Flux_MeshGeometryRegistry (Stage 0)
#include "Flux/UnifiedMesh/Flux_Skinning.h" // Flux_BonePaletteBuilder / Flux_GPUSkinJob (Stage 5)
#include "Flux/TAA/Flux_VelocityHistory.h"  // Flux_PrevTransformCache (Stage 4.3 per-object motion vectors)
#include "AssetHandling/Zenith_MaterialParamTable.h"  // MaterialBlendMode (the external-item classifier's blend input)

class Flux_RenderGraph;
class Zenith_MeshAsset;    // Stage 5: skinned-pose store keyed by mesh asset
class Flux_MeshInstance;   // Stage 1: shared geometry resolved from the mesh-geometry registry for the unified draw
class Flux_SkeletonInstance; // Stage 5: the pose a SKINNED external submission is compute-skinned with
class Zenith_MaterialAsset; // bucket-key material identity (passed by ptr to the Sync extractors)
// Held by POINTER (forward-declared, heap-allocated in Zenith_Engine::AllocateRenderer /
// freed in Shutdown + the dtor backstop) rather than by value: the snapshot header pulls
// Flux_ModelInstance.h -> AssetHandle ->
// AssetRegistry -> Flux, which would close an include cycle back to this header. Forward-
// decl + by-ptr is the sanctioned header-decoupling pattern (NOT pimpl — the type is a
// public, fully-defined Flux type; this just breaks the include edge).
class Flux_RenderSceneSnapshot;

// ============================================================================
// External-item classification — PURE, and deliberately so.
//
// An external submission (Flux_RendererImpl::Flux_ExternalSceneItem below) is
// consumed by EXACTLY ONE of the sync's walks. Which one is decided here, over
// plain values: no Flux_MeshInstance, no material asset, no renderer, no device.
// That is not stylistic. Every Flux_MeshInstance factory REFUSES degenerate
// vertex/index counts, so "a mesh with verts but no indices" cannot be built at
// all — the only way to cover that row of the matrix is a value-level entry
// point, and it is exactly the row a `pxMesh != nullptr` guard would wave past.
// ============================================================================
enum Flux_ExternalItemClass
{
	// The external walk owns it: BuildStaticSubmeshDesc -> one opaque unified
	// draw, or (translucent/additive material) the preserved translucent list.
	EXTERNAL_ITEM_STATIC,
	// The skinned walk owns it: compute-skinned into the shared arena, drawn
	// through its own per-instance skinned bucket.
	EXTERNAL_ITEM_SKINNED,
	// Nobody draws it.
	EXTERNAL_ITEM_SKIP,
};

// Ordered cascade, FIRST MATCH WINS:
//   1. no mesh instance, or zero verts, or zero indices            -> SKIP
//   2. skeleton AND skinning: translucent/additive                 -> SKIP
//                             otherwise                            -> SKINNED
//   3. everything else                                             -> STATIC
//
// Rule 1 now precedes the blend test, which is a real behaviour change for one
// unreachable shape: a DEGENERATE translucent external item used to reach the
// preserved translucent list (the old walk tested blend mode first and only
// then handed the item to BuildStaticSubmeshDesc). No factory can produce that
// item, so nothing in the tree submitted one — but the ordering is now stated
// once, here, instead of being an accident of two nested ifs.
//
// Rule 3 is the row that matters most: a SKELETON-BEARING item whose mesh is
// NOT skinned lands on the static path rather than the skinned one. Routing it
// to the skinned walk would ask the skinned-pose registry for a bind pose the
// asset does not have, and the mesh would simply stop being drawn.
inline constexpr Flux_ExternalItemClass Flux_ClassifyExternalSceneItem(bool bHasMeshInstance,
	u_int uNumVerts, u_int uNumIndices, bool bHasSkeleton, bool bHasSkinning, MaterialBlendMode eBlend)
{
	if (!bHasMeshInstance || uNumVerts == 0u || uNumIndices == 0u)
	{
		return EXTERNAL_ITEM_SKIP;
	}
	if (bHasSkeleton && bHasSkinning)
	{
		// Compute-skinned translucency does not exist on either path (the forward
		// translucent gather refuses skinned submeshes too — see Flux_Translucency's
		// m_bWarnedAnimatedTranslucent branch), so this is a drop, not a divert.
		if (eBlend == MATERIAL_BLEND_TRANSLUCENT || eBlend == MATERIAL_BLEND_ADDITIVE)
		{
			return EXTERNAL_ITEM_SKIP;
		}
		return EXTERNAL_ITEM_SKINNED;
	}
	return EXTERNAL_ITEM_STATIC;
}

// Which of the sync's walks consumes a classified item. "Exactly once" is the
// whole property, and it is a property of the PAIR of walks — so it is stated
// as one total function over the class rather than as two `continue` conditions
// that could drift into agreeing (drawn twice) or disagreeing (drawn never).
enum Flux_ExternalItemWalk
{
	EXTERNAL_ITEM_WALK_NONE = 0,      // discarded
	EXTERNAL_ITEM_WALK_EXTERNAL,      // Flux_RendererImpl::ExtractExternalSceneItems
	EXTERNAL_ITEM_WALK_SKINNED,       // Flux_RendererImpl::ExtractSkinnedBuckets (second walk)
};

inline constexpr Flux_ExternalItemWalk Flux_RouteExternalItem(Flux_ExternalItemClass eClass)
{
	switch (eClass)
	{
	case EXTERNAL_ITEM_STATIC:  return EXTERNAL_ITEM_WALK_EXTERNAL;
	case EXTERNAL_ITEM_SKINNED: return EXTERNAL_ITEM_WALK_SKINNED;
	case EXTERNAL_ITEM_SKIP:    break;
	}
	return EXTERNAL_ITEM_WALK_NONE;
}

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

	// Release all asset-system references Flux holds: the texture/material handles
	// in Flux_Graphics, Flux_Text, Flux_Particles, Flux_Terrain, Flux_VolumeFog,
	// Zenith_MaterialAsset's own defaults, and the skinned-pose store (whose cached
	// Flux_MeshInstances own MeshHandles). Called between Project_Shutdown and
	// Zenith_AssetRegistry::Shutdown so every handle releases while the registry
	// still owns its assets AND the Vulkan device is still up.
	//
	// Anything Flux holds an asset handle in must drop it HERE, not in Shutdown() —
	// Shutdown() runs after the registry has been drained and deleted, so a Release()
	// from there is a use-after-free.
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

	// ===== External scene items (renderer-level draw submissions) =====
	// The generic seam for content that is NOT in the scene snapshot: the
	// material-preview mesh and the animation-preview rig, each view-masked to
	// its own preview slot. An item is STATIC or SKINNED — a skeleton instance
	// makes it the latter, and the sync's skinned walk compute-skins it exactly
	// like a snapshot character (Flux_ClassifyExternalSceneItem above is the
	// single decision). Two ways in, and they differ only in WHO drives the
	// clock:
	//   - PUSH: SubmitExternalSceneItem, on the MAIN THREAD, on a frame that
	//     will reach SyncUnifiedBucketsFromSnapshot (which consumes then clears
	//     the list). The material preview uses it because it already runs
	//     INSIDE the sync.
	//   - PULL: RegisterExternalSceneItemSource, polled by the sync itself.
	//     Anything submitted from outside the frame loop — a unit test, a panel
	//     draw on a frame the editor declines to render — would otherwise SIT in
	//     the list holding raw Flux_MeshInstance*s until the next real frame:
	//     a use-after-free if the owner died in between, and a frame of latency
	//     if it did not. A source is polled or it is not; nothing is pending.
	// Either way the owner keeps mesh + skeleton + material alive for the frame.
	struct Flux_ExternalSceneItem
	{
		Zenith_Maths::Matrix4  m_xWorldMatrix   = Zenith_Maths::Matrix4(1.0f);
		Flux_MeshInstance*     m_pxMeshInstance = nullptr;
		Zenith_MaterialAsset*  m_pxMaterial     = nullptr;   // null -> blank material
		u_int                  m_uViewMask      = 0u;        // Flux_ViewMask* helpers
		// Non-null + a skinned mesh => the SKINNED path. The skinning matrices are
		// read straight off this instance during the sync (same GetOrAddSkeleton
		// dedup as the snapshot walk), so the submitter must have posed it for
		// this frame already.
		Flux_SkeletonInstance* m_pxSkeletonInstance = nullptr;
		// Disambiguates two skinned submissions that share a skeleton AND a mesh
		// asset — the third field of Flux_SkinnedInstanceKey. Two items with the
		// same key would collapse onto ONE arena slice and one draw, so the sync
		// asserts the id it allocates is fresh.
		u_int                  m_uSubmeshSlot   = 0u;
	};
	void SubmitExternalSceneItem(const Flux_ExternalSceneItem& xItem) { m_axExternalSceneItems.PushBack(xItem); }

	// A pull source: called once per sync, appends this frame's items to xOut.
	// Captureless fn-ptr + context, per engine convention (no std::function).
	using Flux_ExternalSceneItemSourceFn = void(*)(void* pCtx, Zenith_Vector<Flux_ExternalSceneItem>& xOut);

	// Register/replace the source keyed by pCtx (registering the same context
	// twice REPLACES rather than doubling, so a re-resolve cannot double-draw).
	// pCtx is the identity: unregister with the same pointer, and do it before
	// the context dies — a stale source is polled the very next frame.
	void RegisterExternalSceneItemSource(Flux_ExternalSceneItemSourceFn pfn, void* pCtx);
	void UnregisterExternalSceneItemSource(void* pCtx);

	// Poll every registered source into xOut and SUBMIT NOTHING — the seam a
	// test drives to prove a source is wired without a frame, a device or a
	// renderer boot. Deliberately NOT behind #ifdef ZENITH_TESTING: a
	// ZENITH_TEST body is compiled in non-testing configs too (the macro
	// degrades to a plain static function), so an #ifdef'd seam would break
	// those builds at the call site.
	void GatherExternalSceneItemsForTesting(Zenith_Vector<Flux_ExternalSceneItem>& xOut);

	// This frame's translucent/additive-material external items, preserved by
	// ExtractExternalSceneItems (which routes them off the opaque unified path).
	// Consumed by the Translucency per-view gather — a pass Prepare that runs
	// AFTER the sync, same frame; the owner keeps the mesh instance + material
	// alive for the frame. Materials are pre-resolved (never null: a null
	// submission resolves to the blank material, which is opaque).
	const Zenith_Vector<Flux_ExternalSceneItem>& GetExternalTranslucentItems() const { return m_axExternalTranslucentItems; }

	// Stage 1: resolve a bucket's shared mesh geometry (VB/IB built by the mesh-geometry
	// registry's real provider) for the per-bucket indirect draw. nullptr in id-only mode
	// (Stage 0) or on a build failure.
	Flux_MeshInstance* GetUnifiedMeshGeometry(u_int uMeshGeometryId) const
	{
		return static_cast<Flux_MeshInstance*>(m_xUnifiedMeshGeometryRegistry.GetBuilt(uMeshGeometryId));
	}

	// ===== Stage 5: compute-skinning data flow =====
	// Animated skinned meshes are compute-skinned + drawn through the unified path.

	// Per skinned bucket (keyed by the bucket's STABLE skinned id = meshGeometryId low bits): the
	// arena slice base + the mesh whose index buffer the skinned draw binds. GatherUnifiedPacket
	// reads this to resolve a skinned bucket's draw.
	struct Flux_UnifiedSkinnedDraw
	{
		Flux_MeshInstance* m_pxMesh        = nullptr;  // IB + index count
		u_int              m_uVertexOffset = 0u;       // base vertex in the skinned arena (this instance's slice)
	};
	const Zenith_HashMap<u_int, Flux_UnifiedSkinnedDraw>& GetUnifiedSkinnedDrawById() const { return m_xUnifiedSkinnedDrawById; }
	const Zenith_Vector<u_int>&           GetUnifiedBindPosePoolWords() const { return m_xUnifiedSkinnedPoseRegistry.GetPoolWords(); }
	u_int GetUnifiedBindPosePoolGeneration() const { return m_xUnifiedSkinnedPoseRegistry.GetPoolGeneration(); }
	const Zenith_Vector<Zenith_Maths::Matrix4>& GetUnifiedBonePalette() const { return m_xUnifiedBonePalette.Matrices(); }
	const Zenith_Vector<Flux_GPUSkinJob>& GetUnifiedSkinJobs() const { return m_axUnifiedSkinJobs; }
	u_int GetUnifiedSkinMaxVerts()      const { return m_uUnifiedSkinMaxVerts; }
	u_int GetUnifiedSkinTotalOutVerts() const { return m_uUnifiedSkinTotalOutVerts; }
	// Stage 4.3: previous-frame object model matrices, index-locked to m_xUnifiedGPUScene.m_xObjects
	// (one per appended object, in append order). GatherUnifiedPacket uploads these parallel to the
	// Objects buffer when the velocity latch is on; the velocity VS reprojects each object's prev pos.
	const Zenith_Vector<Zenith_Maths::Matrix4>& GetUnifiedPrevTransforms() const { return m_axUnifiedPrevTransforms; }
	// Stage 4.3b: the previous-frame bone palette, laid out at the SAME per-skeleton bases as
	// the current palette (GetUnifiedBonePalette). GatherUnifiedPacket uploads it parallel to the
	// current palette when the velocity latch is on; the prev-pose skinning dispatch reads it.
	const Zenith_Vector<Zenith_Maths::Matrix4>& GetUnifiedPrevBonePalette() const { return m_xUnifiedBonePaletteHistory.PrevPalette(); }

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
	// Persistent per-distinct-skinned-mesh bind-pose cache + its owned grow-only bind-pose pool,
	// reclaimed via the shared refcount-diff sync (see Flux_SkinnedPoseRegistry). One entry per
	// DISTINCT skinned mesh asset drawn (NOT per frame/instance); each holds its Zenith_MeshAsset
	// alive via the Flux_MeshInstance handle. Driven each frame by BeginFrameEvictingPrevious()
	// before the skinned walk; the real provider is wired in LateInitialise; freed in Shutdown.
	Flux_SkinnedPoseRegistry              m_xUnifiedSkinnedPoseRegistry;
	// Stable per-(skeleton,mesh,submesh) skinned-instance id allocator -> the skinned bucket key's
	// meshGeometryId (persistent: ids reused across frames, recycled when an instance stops drawing).
	Flux_SkinnedInstanceIdRegistry        m_xUnifiedSkinnedIdRegistry;

	// Per-frame skin-build state (rebuilt each SyncUnifiedBucketsFromSnapshot, read by GatherUnifiedPacket).
	Flux_BonePaletteBuilder               m_xUnifiedBonePalette;             // dedup skeletons -> concatenated palette
	// Stage 4.3b: previous-frame palette history for skeletal motion vectors. BeginFrame/SubmitSkeleton
	// (once per distinct skeleton, at the current palette's block base)/EndFrame run EVERY frame (so prev
	// poses exist the moment velocity latches — no enable glitch); the GPU prev-pose dispatch only runs
	// while velocity is active. Keyed by skeleton id so an eviction re-pack keeps each skeleton's history.
	Flux_BonePaletteHistory               m_xUnifiedBonePaletteHistory;
	Zenith_Vector<Flux_GPUSkinJob>        m_axUnifiedSkinJobs;               // one per animated submesh-instance
	Zenith_HashMap<u_int, Flux_UnifiedSkinnedDraw> m_xUnifiedSkinnedDrawById; // stable skinned id -> arena slice + IB mesh (per-frame)
	u_int m_uUnifiedSkinMaxVerts      = 0u;   // largest job's vertex count (skinning dispatch X)
	u_int m_uUnifiedSkinTotalOutVerts = 0u;   // arena vertices used this frame

	// ===== Stage 4.3: TAA per-object previous transforms (moving-body motion vectors) =====
	// Double-buffered per-entity prev model-matrix store + the per-frame array index-locked to
	// m_xUnifiedGPUScene.m_xObjects (one prev transform per appended object, in append order).
	// Built EVERY frame (so prev data exists the moment the velocity latch turns on — no enable
	// glitch); uploaded to the GPU only while velocity is active. Key = m_ulEntityIDPacked.
	Flux_PrevTransformCache               m_xUnifiedPrevTransformCache;
	Zenith_Vector<Zenith_Maths::Matrix4>  m_axUnifiedPrevTransforms;

	// Unit tests inspect private state.
	friend class Zenith_UnitTests;

private:
	// Per-source extractors for SyncUnifiedBucketsFromSnapshot — each fills exactly ONE
	// data source into the single unified GPU scene the orchestrator brackets with
	// BeginSync/EndGPUSceneBuild. Pure factoring of the old 320-line body (no behaviour
	// change); defined in Flux_GPUSceneBuilder.cpp next to the orchestrator. pxBlankMaterial is threaded
	// in (rather than re-fetched) so the helpers add no new g_xEngine reaches.
	void ExtractSnapshotStaticBuckets(const Flux_RenderSceneSnapshot& xSnapshot, Zenith_MaterialAsset* pxBlankMaterial);
	void ExtractInstanceGroupBuckets(Zenith_MaterialAsset* pxBlankMaterial);
	void ExtractSkinnedBuckets(const Flux_RenderSceneSnapshot& xSnapshot, Zenith_MaterialAsset* pxBlankMaterial);
	void ExtractExternalSceneItems(Zenith_MaterialAsset* pxBlankMaterial);

	// Poll every registered pull source into m_axExternalSceneItems. Runs at the
	// TOP of the sync, beside the material preview's own Update (the push
	// producer), so both are in the list before ANY extractor reads it.
	void PollExternalSceneItemSources();

	// Stage 4.3: push one previous-frame world matrix into m_axUnifiedPrevTransforms (called
	// immediately after EACH GPU-scene object append, so the array stays index-locked to
	// m_xObjects) and record this frame's matrix as next frame's prev. ulEntityId 0
	// (foliage / external — no stable id) => prev == current => camera-only velocity.
	void RecordUnifiedPrevTransform(u_int64 ulEntityId, const Zenith_Maths::Matrix4& xCurrentWorld);

	// Pending external submissions for this frame — pushed by SubmitExternalSceneItem
	// and pulled from the registered sources at the top of the sync. Read TWICE
	// (ExtractSkinnedBuckets takes the SKINNED items, ExtractExternalSceneItems the
	// rest) and cleared ONCE, at the end of the second walk.
	Zenith_Vector<Flux_ExternalSceneItem> m_axExternalSceneItems;

	// Translucent/additive external items diverted off the opaque unified path —
	// cleared + refilled by ExtractExternalSceneItems each sync (see
	// GetExternalTranslucentItems above for the consumer contract).
	Zenith_Vector<Flux_ExternalSceneItem> m_axExternalTranslucentItems;

	// The registered pull sources (see RegisterExternalSceneItemSource). Small by
	// construction — one per live preview session — so a linear scan keyed on the
	// context pointer is the whole lookup.
	struct Flux_ExternalSceneItemSource
	{
		Flux_ExternalSceneItemSourceFn m_pfnGather = nullptr;
		void*                          m_pContext  = nullptr;
	};
	Zenith_Vector<Flux_ExternalSceneItemSource> m_axExternalSceneItemSources;

	// One-shot latch for the "this submission is drawn by nobody" warning. A per-frame
	// log line would be a wall of text on a preview that has simply not resolved its rig
	// yet; a silent drop is how a missing preview reads as a broken feature.
	bool m_bWarnedDiscardedExternalItem = false;
};

// The forwarder over a live submission: resolves the mesh's counts + skinning and
// the material's blend mode, then defers to the pure cascade above. Defined in
// Flux_GPUSceneBuilder.cpp, where Flux_MeshInstance and Zenith_MaterialAsset are
// complete types.
//
// pxResolvedMaterial is NON-const because Zenith_MaterialAsset::GetResolved() is
// (it flattens the parent chain behind a stamp cache). It must be the RESOLVED
// material the caller would draw — i.e. the blank material for a null submission,
// never nullptr, matching what BuildStaticSubmeshDesc does one line later.
Flux_ExternalItemClass Flux_ClassifyExternalSceneItem(
	const Flux_RendererImpl::Flux_ExternalSceneItem& xItem, Zenith_MaterialAsset* pxResolvedMaterial);
