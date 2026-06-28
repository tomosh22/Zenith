#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Collections/Zenith_Vector.h"

class Flux_ShaderBinder;
class Flux_MeshInstance;

// =====================================================================
// Flux_UnifiedMeshImpl — the unified GPU-driven opaque static-mesh feature
// (Stage 1). Promotes the InstancedMeshes cull->indirect-draw core to draw the
// opaque static meshes the renderer gathers into the GPU scene
// (Flux_RendererImpl::SyncUnifiedBucketsFromSnapshot → m_xUnifiedGPUScene +
// m_xUnifiedBucketRegistry). Three passes:
//   reset (compute)  — per BUCKET: seed the indirect command (indexCount from the
//                      bucket's shared mesh; instanceCount 0).
//   cull  (compute)  — per DRAW-ITEM: object-transformed per-submesh sphere vs
//                      frustum; survivors scatter into per-bucket partitions of the
//                      shared visible-index buffer + bump that bucket's instanceCount.
//   GBuffer (graphics)— one DrawIndexedIndirect per live bucket.
//
// BUFFER MODEL (refinement over per-bucket buffers): TWO global persistent cull-
// output buffers partitioned by per-bucket offsets — m_xVisibleIndexBuffer (sized to
// the total draw-item count) + m_xIndirectBuffer (one VkDrawIndexedIndirectCommand
// per bucket SLOT). Both are graph-tracked + cyclic-barrier-seeded. The render-graph
// STRUCTURE is therefore static regardless of bucket count (only two persistent
// buffers declared) — no per-topology-change rebuild. The GPU-scene records + the
// per-bucket offset/index-count metadata are host-uploaded each frame into frame-
// indexed dynamic buffers (not graph-tracked, like InstancedMeshes' transform buffer).
//
// Gated on Flux_RendererImpl::IsUnifiedGPUPathEnabled(): when on, this draws the
// opaque statics and Flux_StaticMeshes' G-buffer draw skips (A/B); when off, this is
// a no-op and the old StaticMeshes path renders. Shadows stay on the old path until
// Stage 2.
// =====================================================================

// One live bucket's CPU-side draw record, frozen by the main-thread gather and read
// by the (worker) GBuffer record callback.
struct Flux_UnifiedBucketDraw
{
	u_int              m_uSlot          = 0u;  // stable bucket slot (indexes the indirect buffer)
	u_int              m_uMaterialIndex = 0u;  // index into g_axMaterials (GLOBAL set 0)
	u_int              m_uVisibleOffset = 0u;  // base offset of the bucket's slice in the visible-index buffer
	u_int              m_uCullMode      = 0u;  // uFLUX_GPUSCENE_CULL_* (one-sided / two-sided pipeline)
	Flux_MeshInstance* m_pxMesh         = nullptr; // shared geometry (VB/IB) for the indirect draw
};

class Flux_UnifiedMeshImpl
{
public:
	Flux_UnifiedMeshImpl() = default;
	~Flux_UnifiedMeshImpl() = default;

	Flux_UnifiedMeshImpl(const Flux_UnifiedMeshImpl&) = delete;
	Flux_UnifiedMeshImpl& operator=(const Flux_UnifiedMeshImpl&) = delete;

	void Initialise();
	void BuildPipelines();
	void Shutdown();
	void Reset();

	void SetupRenderGraph(Flux_RenderGraph& xGraph);

	// WS7 keystone main-thread gather (hung via .Prepare on the cull pass). Reads the
	// renderer's GPU scene + bucket registry (frozen by SyncUnifiedBucketsFromSnapshot
	// earlier this frame), registers each bucket's material with the GPU table, grows
	// the GPU buffers if needed, computes per-bucket offsets / index-counts / material
	// indices, and host-uploads the dynamic buffers + culling constants. After it
	// returns the per-frame state is frozen; the record callbacks are pure readers.
	void GatherUnifiedPacket(void*);

	// ===== Data =====

	// Persistent, graph-tracked cull outputs (cyclic-barrier seeded).
	Flux_ReadWriteBuffer        m_xVisibleIndexBuffer;   // uint[totalDrawItems], partitioned by bucket
	Flux_IndirectBuffer         m_xIndirectBuffer;       // VkDrawIndexedIndirectCommand[bucketSlots]

	// Frame-indexed dynamic inputs (host-uploaded each frame; NOT graph-tracked).
	Flux_DynamicReadWriteBuffer m_xObjectsBuffer;        // GPUSceneObject[]
	Flux_DynamicReadWriteBuffer m_xDrawItemsBuffer;      // GPUSceneDrawItem[]
	Flux_DynamicReadWriteBuffer m_xBucketOffsetBuffer;   // uint[bucketSlots] prefix-sum
	Flux_DynamicReadWriteBuffer m_xBucketIndexCountBuffer;// uint[bucketSlots] mesh index count
	Flux_DynamicConstantBuffer  m_xCullingConstantsBuffer;

	// Capacities (element counts) — grow-only; growth reuses the same buffer object so
	// the graph's barrier keys (by C++ object address) stay valid.
	u_int m_uVisibleIndexCapacity   = 0u;  // uints
	u_int m_uIndirectBucketCapacity = 0u;  // commands (5 uints each)
	u_int m_uObjectCapacity         = 0u;
	u_int m_uDrawItemCapacity       = 0u;
	u_int m_uBucketMetaCapacity     = 0u;

	// Shaders / pipelines.
	Flux_Shader   m_xGBufferShader;
	Flux_Pipeline m_xGBufferPipeline;          // one-sided (CULL_MODE_BACK)
	Flux_Pipeline m_xGBufferPipelineTwoSided;  // two-sided (CULL_MODE_NONE)
	Flux_Shader   m_xCullingShader;
	Flux_Pipeline m_xCullingPipeline;
	Flux_RootSig  m_xCullingRootSig;
	Flux_Shader   m_xResetShader;
	Flux_Pipeline m_xResetPipeline;
	Flux_RootSig  m_xResetRootSig;

	bool m_bResourcesReady = false;

	// Per-frame frozen state (gather → execute).
	u_int m_uTotalDrawItems  = 0u;
	u_int m_uBucketSlotCount = 0u;
	Zenith_Vector<Flux_UnifiedBucketDraw> m_axBucketDraws;

	// Reusable scratch for the per-bucket metadata (avoids per-frame allocation).
	Zenith_Vector<u_int> m_auBucketCountScratch;
	Zenith_Vector<u_int> m_auBucketOffsetScratch;
	Zenith_Vector<u_int> m_auBucketIndexCountScratch;
};
