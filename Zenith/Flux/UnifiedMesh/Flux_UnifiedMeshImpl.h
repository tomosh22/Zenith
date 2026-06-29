#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Collections/Zenith_Vector.h"
#include "Maths/Zenith_Maths.h"

class Flux_ShaderBinder;
class Flux_MeshInstance;
class Flux_AnimationTexture;

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
// This is THE opaque static + instanced-foliage mesh pipeline (Stage 4 retired the legacy
// per-object StaticMeshes/InstancedMeshes G-buffer + shadow draw loops): it draws every
// opaque bucket to the camera G-buffer and casts every shadow cascade. The passes self-guard
// on the per-frame data (no draw-items / no buckets -> early-out) rather than any toggle.
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
	// VAT (Stage 3): the bucket's vertex-animation texture (null = static / no VAT) + the
	// per-bucket params (texW, texH, enabled, 0) bound into the DRAW set each draw.
	const Flux_AnimationTexture* m_pxVATTexture = nullptr;
	Zenith_Maths::Vector4        m_xVATParams   = Zenith_Maths::Vector4(0.0f);
	// Stage 5: a SKINNED bucket draws from the shared skinned-vertex arena (bound at this
	// instance's byte offset) instead of m_pxMesh's vertex buffer; m_pxMesh then supplies only
	// the index buffer + index count. m_uVertexOffset is the arena slice base IN VERTICES.
	bool  m_bSkinned      = false;
	u_int m_uVertexOffset = 0u;
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

	// Stage 2: per-cascade shadow caster draw, invoked from Flux_Shadows::ExecuteShadowCascade
	// (worker thread, inside cascade pass uCascade's record) AFTER the cull has populated the
	// shadow-view partitions of the shared cull-output buffers. Binds its own shadow pipeline
	// (depth-only) + PASS block, then one DrawIndexedIndirect per live bucket at view
	// (uCascade+1). No-op when the unified path is off / shadows view inactive / no buckets.
	void RenderToShadowMap(Flux_CommandBuffer& xCmdBuf, u_int uCascade);

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
	Flux_Shader   m_xShadowShader;             // Stage 2: depth-only caster (one pipeline, all buckets)
	Flux_Pipeline m_xShadowPipeline;
	Flux_Shader   m_xCullingShader;
	Flux_Pipeline m_xCullingPipeline;
	Flux_RootSig  m_xCullingRootSig;
	Flux_Shader   m_xResetShader;
	Flux_Pipeline m_xResetPipeline;
	Flux_RootSig  m_xResetRootSig;

	// Stage 5: compute-skinning pre-pass (skeletal animated meshes -> object-space arena).
	// Skins each animated submesh-instance to the 72-byte static layout in the persistent
	// arena; the cull/draw/shadow kernels then consume it as ordinary static geometry. The
	// pass self-guards on m_uSkinJobCount == 0 (no animated work this frame -> no dispatch).
	Flux_Shader   m_xSkinningShader;
	Flux_Pipeline m_xSkinningPipeline;
	Flux_RootSig  m_xSkinningRootSig;

	// Persistent, grow-only (graph-tracked) skinned-vertex arena: 72B static verts the
	// skinned buckets draw via SetVertexBuffer(this) + per-bucket indirect vertexOffset.
	// Created with BOTH UAV (compute write) and VERTEX (draw) usage.
	Flux_ReadWriteBuffer        m_xSkinnedArenaBuffer;
	u_int m_uSkinnedArenaVertCapacity = 0u;   // output verts (72B / 18 words each)

	// Frame-indexed dynamic skinning inputs (NOT graph-tracked). The bind-pose pool holds STATIC
	// content (rest-pose verts) so it is host-uploaded only on GROWTH (m_uBindPosePool* below); the
	// palette + skin-jobs are genuinely per-frame and upload every frame.
	Flux_DynamicReadWriteBuffer m_xBindPosePoolBuffer;   // 104B bind-pose verts, raw words (SRV)
	Flux_DynamicReadWriteBuffer m_xBonePaletteBuffer;    // all live skeletons' skinning matrices
	Flux_DynamicReadWriteBuffer m_xSkinJobsBuffer;       // Flux_GPUSkinJob[]
	Flux_DynamicConstantBuffer  m_xSkinConstantsBuffer;
	u_int m_uBindPosePoolWordCapacity = 0u;  // 26-word input verts
	u_int m_uBonePaletteMatCapacity   = 0u;
	u_int m_uSkinJobCapacity          = 0u;
	// Bind-pose pool is static + persistent: re-upload only after a CHANGE (grow or eviction
	// re-pack — the renderer bumps a pool generation), for MAX_FRAMES_IN_FLIGHT frames (so every
	// frame-indexed physical copy is refreshed), then skip on steady-state frames.
	u_int m_uBindPosePoolUploadedGen = 0u;   // last pool generation uploaded to every copy
	u_int m_uBindPosePoolDirtyFrames = 0u;   // frames left to re-upload after a change

	bool m_bResourcesReady = false;

	// Per-frame frozen state (gather → execute).
	u_int m_uTotalDrawItems  = 0u;
	u_int m_uBucketSlotCount = 0u;
	u_int m_uNumViews        = 1u;  // Stage 2: 1 (camera) or 1+ZENITH_FLUX_NUM_CSMS (shadows on)
	Zenith_Vector<Flux_UnifiedBucketDraw> m_axBucketDraws;

	// Stage 5 per-frame skinning state (gather -> skinning execute).
	u_int m_uSkinJobCount     = 0u;  // animated submesh-instances to skin this frame (0 = no dispatch)
	u_int m_uSkinMaxVertCount = 0u;  // largest job's vertex count (skinning dispatch X dimension)
	u_int m_uBonePaletteCount = 0u;  // matrices uploaded into the palette (compute bounds-guard)

	// Reusable scratch for the per-bucket metadata (avoids per-frame allocation).
	Zenith_Vector<u_int> m_auBucketCountScratch;
	Zenith_Vector<u_int> m_auBucketOffsetScratch;
	Zenith_Vector<u_int> m_auBucketIndexCountScratch;
};
