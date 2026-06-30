#pragma once

#include "Maths/Zenith_Maths.h"
#include "Collections/Zenith_Vector.h"
#include "Collections/Zenith_HashMap.h"
#include "Flux/Flux_RefcountDiffRegistry.h"
#include <cstddef>   // offsetof, size_t
#include <cmath>     // sqrtf

// ============================================================================
// Flux GPU-scene records (Stage 0 — unified GPU-driven opaque-mesh pipeline)
//
// Two GPU-resident record arrays describe the opaque scene for compute culling +
// indirect drawing, mirroring UE5's PrimitiveSceneData + mesh-draw-command split:
//   - Flux_GPUSceneObject   : one per scene item  (model transform + flags).
//   - Flux_GPUSceneDrawItem : one per SUBMESH      (-> objectIndex + owning
//     bucket + the submesh's local-space bounding sphere for per-submesh
//     frustum culling).
// A draw-item's material/VAT are resolved per BUCKET (one DrawIndexedIndirect per
// bucket) so they are NOT stored here — the shading VS reads the material from the
// per-bucket DrawConstants, keeping the material index draw-uniform.
//
// These records are filled on the CPU each frame (Flux_RendererImpl::
// SyncUnifiedBucketsFromSnapshot) and uploaded + sampled by the Flux_UnifiedMesh
// cull/draw passes. The pure builder + bucket registry below are headless-unit-
// tested (no GPU, no renderer boot) — which is why their input is a POD descriptor
// (Flux_GPUSceneSourceItem), not the snapshot's Flux_ModelInstance (that drags in
// GPU buffers). The non-pure snapshot->POD adapter lives in the renderer.
//
// LAYOUT RULE: byte-identical to GPUSceneObject / GPUSceneDrawItem in
// Common/SceneObjects.slang (added in Stage 1 when a shader first reads them).
// All members are 16-byte aligned so the std430 structured-buffer layout matches
// with no scalar-packing surprises; pad fields are explicitly zeroed by the pack
// helpers so a full-struct byte feed is deterministic.
// ============================================================================

// Per-object flags (m_uFlags).
inline constexpr u_int uFLUX_GPUSCENE_OBJFLAG_VAT     = 0x1u;  // object uses VAT (foliage); gates SampleVAT in the VS
inline constexpr u_int uFLUX_GPUSCENE_OBJFLAG_SKINNED = 0x2u;  // object is a compute-skinned animated instance (Stage 5)

// Bucket meshGeometryId high bit: a SKINNED bucket's id is NOT a mesh-geometry-registry id
// but uFLUX_GPUSCENE_SKINNED_MESH_BIT | perInstanceSkinIndex (the low bits index the renderer's
// per-frame skinned-draw array). The draw branches on this bit: skinned buckets bind the shared
// skinned-vertex arena + the mesh's index buffer; static buckets resolve the registry geometry.
// Per-instance ids keep each skinned instance its OWN bucket in Stage 5 (Stage 6 drops them to
// share a bucket + emit per-instance MultiDrawIndirect commands).
inline constexpr u_int uFLUX_GPUSCENE_SKINNED_MESH_BIT = 0x80000000u;
inline constexpr bool Flux_IsSkinnedMeshGeometryId(u_int uId) { return (uId & uFLUX_GPUSCENE_SKINNED_MESH_BIT) != 0u; }
inline constexpr u_int Flux_SkinnedMeshGeometryIndex(u_int uId) { return uId & ~uFLUX_GPUSCENE_SKINNED_MESH_BIT; }

// INVARIANT: this 80-byte layout is locked to GPUSceneObject in
// Common/SceneObjects.slang (see LAYOUT RULE above); the static_asserts below
// enforce it. Do not add/remove/reorder members without editing the shader in lockstep.
struct Flux_GPUSceneObject
{
	Zenith_Maths::Matrix4 m_xModelMatrix;     // offset  0 — object world transform
	u_int                 m_uFlags;           // offset 64 — object flags (uFLUX_GPUSCENE_OBJFLAG_*)
	u_int                 m_uBonePaletteRef;  // offset 68 — bone-palette base (reserved -> Stage 5)
	u_int                 m_uVATAnimPacked;   // offset 72 — VAT: animIndex (low 16) | frameCount (high 16); 0 = no VAT
	u_int                 m_uVATAnimTime;     // offset 76 — VAT: normalised anim time (float bits)
};
static_assert(sizeof(Flux_GPUSceneObject) == 80, "Flux_GPUSceneObject must be 80 bytes (mirrors GPUSceneObject in Common/SceneObjects.slang)");
static_assert(offsetof(Flux_GPUSceneObject, m_uFlags) == 64, "Flux_GPUSceneObject::m_uFlags must sit at offset 64");
static_assert(offsetof(Flux_GPUSceneObject, m_uBonePaletteRef) == 68, "Flux_GPUSceneObject::m_uBonePaletteRef must sit at offset 68");
static_assert(offsetof(Flux_GPUSceneObject, m_uVATAnimPacked) == 72, "Flux_GPUSceneObject::m_uVATAnimPacked must sit at offset 72");
static_assert(offsetof(Flux_GPUSceneObject, m_uVATAnimTime) == 76, "Flux_GPUSceneObject::m_uVATAnimTime must sit at offset 76 (last word of the 80B record)");

// INVARIANT: this 32-byte layout is locked to GPUSceneDrawItem in
// Common/SceneObjects.slang (see LAYOUT RULE above); the static_asserts below
// enforce it. Change the shader in lockstep with any member edit.
struct Flux_GPUSceneDrawItem
{
	u_int                 m_uObjectIndex;       // offset  0 — index into the object array
	u_int                 m_uBucketIndex;       // offset  4 — owning bucket (one indirect draw)
	u_int                 m_uColorTintPacked;   // offset  8 — per-instance tint (RGBA8); white = no tint
	u_int                 m_uFlags;             // offset 12 — draw-item flags (reserved)
	Zenith_Maths::Vector4 m_xLocalBoundsSphere; // offset 16 — submesh local-space bounding sphere (center.xyz, radius.w)
};
static_assert(sizeof(Flux_GPUSceneDrawItem) == 32, "Flux_GPUSceneDrawItem must be 32 bytes (mirrors GPUSceneDrawItem in Common/SceneObjects.slang)");
static_assert(offsetof(Flux_GPUSceneDrawItem, m_uBucketIndex) == 4, "Flux_GPUSceneDrawItem::m_uBucketIndex must sit at offset 4");
static_assert(offsetof(Flux_GPUSceneDrawItem, m_uColorTintPacked) == 8, "Flux_GPUSceneDrawItem::m_uColorTintPacked must sit at offset 8");
static_assert(offsetof(Flux_GPUSceneDrawItem, m_uFlags) == 12, "Flux_GPUSceneDrawItem::m_uFlags must sit at offset 12");
static_assert(offsetof(Flux_GPUSceneDrawItem, m_xLocalBoundsSphere) == 16, "Flux_GPUSceneDrawItem::m_xLocalBoundsSphere must sit at offset 16");

// Packed RGBA8 "no tint" default for the per-instance colour tint.
inline constexpr u_int uFLUX_GPUSCENE_TINT_WHITE = 0xffffffffu;

// Bucket cull-mode bins (mirrors the InstancedMeshes one-sided / two-sided pass split).
inline constexpr u_int uFLUX_GPUSCENE_CULL_ONE_SIDED = 0u;  // CULL_MODE_BACK (default)
inline constexpr u_int uFLUX_GPUSCENE_CULL_TWO_SIDED = 1u;  // CULL_MODE_NONE (material m_bTwoSided)

// ---- pure pack helpers (field-by-field; unused VAT fields explicitly zeroed) -
// uVATAnimPacked = animIndex|(frameCount<<16); uVATAnimTime = the float bits of the
// normalised anim time. Static (non-foliage) objects pass 0/0 and a flags word without
// uFLUX_GPUSCENE_OBJFLAG_VAT, so the VS skips VAT and the bytes match the old zeroed pads.
inline void Flux_BuildGPUSceneObject(Flux_GPUSceneObject& xOut, const Zenith_Maths::Matrix4& xModel,
	u_int uFlags, u_int uBonePaletteRef, u_int uVATAnimPacked = 0u, u_int uVATAnimTimeBits = 0u)
{
	xOut.m_xModelMatrix    = xModel;
	xOut.m_uFlags          = uFlags;
	xOut.m_uBonePaletteRef = uBonePaletteRef;
	xOut.m_uVATAnimPacked  = uVATAnimPacked;
	xOut.m_uVATAnimTime    = uVATAnimTimeBits;
}

// Pack a VAT animation descriptor into the object's m_uVATAnimPacked word.
inline u_int Flux_PackVATAnim(u_int uAnimIndex, u_int uFrameCount)
{
	return (uAnimIndex & 0xFFFFu) | ((uFrameCount & 0xFFFFu) << 16);
}

inline void Flux_BuildGPUSceneDrawItem(Flux_GPUSceneDrawItem& xOut, u_int uObjectIndex, u_int uBucketIndex,
	u_int uColorTintPacked, u_int uFlags, const Zenith_Maths::Vector4& xLocalBoundsSphere)
{
	xOut.m_uObjectIndex       = uObjectIndex;
	xOut.m_uBucketIndex       = uBucketIndex;
	xOut.m_uColorTintPacked   = uColorTintPacked;
	xOut.m_uFlags             = uFlags;
	xOut.m_xLocalBoundsSphere = xLocalBoundsSphere;
}

// Conservative bounding sphere (center.xyz, radius.w) enclosing a local-space AABB
// — the per-submesh cull primitive. radius = half the box diagonal (rotation-
// invariant under the object transform, so the cull shader just transforms the
// centre by the model matrix and scales the radius by the max axis scale). The
// per-submesh AABB comes from each submesh's Flux_MeshInstance::GetLocalBounds()
// (one mesh instance per model submesh) — no mesh-asset/bake change needed.
inline Zenith_Maths::Vector4 Flux_LocalBoundsSphereFromAABB(
	const Zenith_Maths::Vector3& xMin, const Zenith_Maths::Vector3& xMax)
{
	const Zenith_Maths::Vector3 xCenter = (xMin + xMax) * 0.5f;
	const Zenith_Maths::Vector3 xHalf   = (xMax - xMin) * 0.5f;
	const float fRadius = sqrtf(xHalf.x * xHalf.x + xHalf.y * xHalf.y + xHalf.z * xHalf.z);
	return Zenith_Maths::Vector4(xCenter.x, xCenter.y, xCenter.z, fRadius);
}

// ---- per-bucket cull-output partitioning (CPU side of the GPU-driven path) ---
// Exclusive prefix sum of per-bucket draw-item counts: out[b] is the base index of
// bucket b's slice in the shared visible-index buffer (the cull compute scatters a
// survivor for bucket b into [out[b], out[b]+count[b])). The Stage-1 gather's core,
// extracted so it is pure + headless-testable (and reusable by the per-view shadow
// cull in Stage 2). out is sized to match auCounts; retired/empty buckets contribute 0.
void Flux_BuildBucketOffsets(const Zenith_Vector<u_int>& auCounts, Zenith_Vector<u_int>& auOffsetsOut);

// ---- per-submesh frustum cull (CPU mirror of Flux_UnifiedMesh_Culling.slang) -
// Largest world-space scale = max basis-vector length of the upper-3x3, so a sphere
// scaled by it stays conservative under non-uniform scale (mirrors the shader's GetMaxScale).
float Flux_MaxScaleFromMatrix(const Zenith_Maths::Matrix4& xModel);

// True iff the draw-item's object-transformed local bounding sphere is at least partially
// inside the frustum — the canonical CPU reference for Flux_UnifiedMesh_Culling.slang's
// TestSphereFrustum. Planes are (xyz = INWARD normal, w = distance), the Flux_FrustumPlaneGPU
// layout. worldCenter = model * localCenter; worldRadius = localRadius * maxScale(model);
// culled iff dot(n, worldCenter) + d < -worldRadius for any plane.
bool Flux_CullDrawItemAgainstFrustum(const Zenith_Maths::Matrix4& xModel,
	const Zenith_Maths::Vector4& xLocalBoundsSphere, const Zenith_Maths::Vector4 axPlanes[6]);

// ---- reset-indirect packer (CPU mirror of Flux_UnifiedMesh_Reset.slang) ------
// Packs one VkDrawIndexedIndirectCommand (5 uints): [indexCount, instanceCount=0,
// firstIndex=0, vertexOffset, firstInstance=0]. The cull compute atomically increments
// word 1 (instanceCount); every other word stays as packed. Lives next to kuINDIRECT_WORDS.
// uVertexOffset is the bucket's base vertex into a shared vertex arena — 0 for static /
// foliage buckets (geometry bound directly), and the skinned submesh-instance's arena
// slice base for Stage-5 skinned buckets (the compute pre-pass writes there). Defaulting
// it to 0 keeps every existing caller byte-identical to the pre-Stage-5 layout.
inline constexpr u_int uFLUX_GPUSCENE_INDIRECT_WORDS = 5u;   // VkDrawIndexedIndirectCommand
inline void Flux_PackResetIndirectCommand(u_int auOut[uFLUX_GPUSCENE_INDIRECT_WORDS], u_int uIndexCount,
	u_int uVertexOffset = 0u)
{
	auOut[0] = uIndexCount;    // indexCount
	auOut[1] = 0u;             // instanceCount (cull-incremented)
	auOut[2] = 0u;             // firstIndex
	auOut[3] = uVertexOffset;  // vertexOffset (skinned-arena slice base; 0 for static/foliage)
	auOut[4] = 0u;             // firstInstance
}

// ---- multi-view cull-output index math (Stage 2) ----------------------------
// The shared cull-output buffers fan out over VIEWS (view 0 = camera, views 1..N =
// the shadow cascades), laid out view-major:
//   visibleIndex : per view a uTotalDrawItems-wide slice (base v*uTotalDrawItems),
//                  internally partitioned per bucket by the (view-invariant) prefix-
//                  sum bucketOffset[b].
//   indirect     : one VkDrawIndexedIndirectCommand per (view,bucket), command (v,b)
//                  at word offset (v*uNumBuckets + b)*uFLUX_GPUSCENE_INDIRECT_WORDS.
// These are the single C++ source of truth for the reset/cull/draw kernels' addressing
// (the .slang shaders mirror them); they are pure so a stride drift fails a unit test,
// not the GPU. Used by Flux_UnifiedMeshImpl's GBuffer + shadow indirect draws.

// Word offset of view v / bucket b's VkDrawIndexedIndirectCommand in the indirect buffer.
inline u_int Flux_UnifiedIndirectCommandWord(u_int uView, u_int uBucket, u_int uNumBuckets)
{
	return (uView * uNumBuckets + uBucket) * uFLUX_GPUSCENE_INDIRECT_WORDS;
}

// Index into the visible-index buffer for view v, given the bucket's within-view base
// (bucketOffset[b]) and a local survivor slot. The cull writes here; the draw reads from
// the bucket base (uLocalSlot 0) for uBucketOffset = v*uTotalDrawItems + bucketOffset[b].
inline u_int Flux_UnifiedVisibleWriteIndex(u_int uView, u_int uTotalDrawItems, u_int uBucketOffset, u_int uLocalSlot)
{
	return uView * uTotalDrawItems + uBucketOffset + uLocalSlot;
}

// ============================================================================
// Bucket model. A BUCKET is one DrawIndexedIndirect unit: every draw-item sharing
// the same (meshGeometry, cullMode, material, VAT) identity. The key is STABLE
// asset/handle identity, NEVER a per-frame-resolved table index (the material
// table index can change frame-to-frame on a material edit; keying topology on it
// would thrash the render graph). The resolved numeric indices live in the
// bucket's per-frame DrawConstants instead (Stage 1).
// ============================================================================
struct Flux_GPUSceneBucketKey
{
	u_int   m_uMeshGeometryId   = 0u;  // shared mesh-geometry registry id (Stage 0b)
	u_int   m_uCullMode         = 0u;  // uFLUX_GPUSCENE_CULL_*
	// In-SESSION grouping identity for the material — the adapter feeds the raw
	// Zenith_MaterialAsset* (blank-material ptr for null). This key only decides which
	// draw-items SHARE an indirect draw; the rendered material is the per-frame
	// DrawConstants table index (Stage 1), and this topology is rebuilt every frame and
	// never serialized — so a recycled material address producing a new-but-equal key is
	// benign (it would draw the recycled material, which is correct). A path/name hash
	// would be WORSE here: procedural materials can share an empty path and would then
	// collide into one bucket. Keep the pointer; do NOT swap it for a path id.
	u_int64 m_ulMaterialAssetId = 0u;
	u_int64 m_ulVATTextureId    = 0u;  // stable VAT asset-handle identity (Stage 0c); 0 = no VAT

	bool operator==(const Flux_GPUSceneBucketKey& xOther) const
	{
		return m_uMeshGeometryId   == xOther.m_uMeshGeometryId
		    && m_uCullMode         == xOther.m_uCullMode
		    && m_ulMaterialAssetId == xOther.m_ulMaterialAssetId
		    && m_ulVATTextureId    == xOther.m_ulVATTextureId;
	}
};

// FNV-1a over the key's fields (byte-fed; the key is fully-initialised POD), so
// the bucket map needs no std::hash specialisation for the composite key.
template<>
struct Zenith_Hash<Flux_GPUSceneBucketKey>
{
	u_int64 operator()(const Flux_GPUSceneBucketKey& xKey) const noexcept
	{
		u_int64 uHash = 0xcbf29ce484222325ull;
		auto Bytes = [&uHash](const void* p, size_t n)
		{
			const u_int8* pb = static_cast<const u_int8*>(p);
			for (size_t i = 0; i < n; ++i) { uHash ^= pb[i]; uHash *= 0x100000001b3ull; }
		};
		Bytes(&xKey.m_uMeshGeometryId,   sizeof(xKey.m_uMeshGeometryId));
		Bytes(&xKey.m_uCullMode,         sizeof(xKey.m_uCullMode));
		Bytes(&xKey.m_ulMaterialAssetId, sizeof(xKey.m_ulMaterialAssetId));
		Bytes(&xKey.m_ulVATTextureId,    sizeof(xKey.m_ulVATTextureId));
		return uHash;
	}
};

// ---- per-frame source descriptors (POD; the pure builder's input) -----------
// Extracted from the render-scene snapshot by the (non-pure) adapter in Stage 0e.
// Keeping the builder POD-only is what makes it headless-testable.
struct Flux_GPUSceneSourceSubmesh
{
	u_int   m_uMeshGeometryId   = 0u;
	u_int   m_uCullMode         = uFLUX_GPUSCENE_CULL_ONE_SIDED;
	u_int64 m_ulMaterialAssetId = 0u;
	u_int64 m_ulVATTextureId    = 0u;
	Zenith_Maths::Vector4 m_xLocalBoundsSphere = Zenith_Maths::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
	u_int   m_uColorTintPacked  = uFLUX_GPUSCENE_TINT_WHITE;
	u_int   m_uFlags            = 0u;
};

struct Flux_GPUSceneSourceItem
{
	Zenith_Maths::Matrix4 m_xWorldMatrix = Zenith_Maths::Matrix4(1.0f);
	u_int   m_uFlags           = 0u;   // uFLUX_GPUSCENE_OBJFLAG_* (e.g. VAT)
	u_int   m_uBonePaletteRef  = 0u;
	u_int   m_uVATAnimPacked   = 0u;   // VAT animIndex|frameCount (0 = none)
	u_int   m_uVATAnimTimeBits = 0u;   // VAT normalised time (float bits)
	Zenith_Vector<Flux_GPUSceneSourceSubmesh> m_xSubmeshes;
};

struct Flux_GPUSceneBuildResult
{
	Zenith_Vector<Flux_GPUSceneObject>   m_xObjects;
	Zenith_Vector<Flux_GPUSceneDrawItem> m_xDrawItems;
	bool m_bTopologyChanged = false;
};

// ============================================================================
// Flux_GPUSceneBucketRegistry — stable (mesh,cull,material,VAT) -> bucketIndex
// mapping with a refcount-diff sync. The set of LIVE buckets is the render-graph
// topology: a bucket created (first reference) or retired (last reference gone)
// flags a topology change -> the caller issues RequestGraphRebuild. A bucket's
// per-frame draw-item COUNT is data, not topology. Pure CPU (no GPU) -> headless
// unit-tested. bucketIndex is stable across frames for a live key; freed indices
// are recycled (retire + create both flag topology, so a recycled index never
// silently aliases — Stage 1 owns the in-flight per-bucket buffer lifetime).
//
// Thin instantiation of Flux_RefcountDiffRegistry<Key> (no payload, no provider):
// the sync machinery + topology signal + slot iteration live in the base; this
// class only re-exposes them under bucket-domain names.
// ============================================================================
class Flux_GPUSceneBucketRegistry : public Flux_RefcountDiffRegistry<Flux_GPUSceneBucketKey>
{
public:
	// BeginSync / Reference / EndSync / WasTopologyChangedThisSync are inherited.
	u_int GetLiveBucketCount()      const { return GetLiveCount(); }
	u_int GetHighWaterBucketSlots() const { return GetHighWaterSlots(); }

	// Read-only lookups (also exercised by tests).
	bool  TryGetBucketIndex(const Flux_GPUSceneBucketKey& xKey, u_int& uOut) const { return TryGetId(xKey, uOut); }
	bool  HasBucket(const Flux_GPUSceneBucketKey& xKey) const { return HasKey(xKey); }
	u_int GetBucketRefcount(const Flux_GPUSceneBucketKey& xKey) const { return GetCommittedRefcount(xKey); }

	// Live-bucket iteration for the draw consumer (Stage 1). Slots are stable
	// indices in [0, GetBucketSlotCount()); a retired slot returns nullptr until
	// recycled. The returned key gives the consumer the meshGeometryId / material /
	// cull mode for the bucket's indirect draw.
	u_int GetBucketSlotCount() const { return GetSlotCount(); }
	const Flux_GPUSceneBucketKey* TryGetBucketKey(u_int uSlot) const { return TryGetKey(uSlot); }
};

// Pure build: walk source items -> object + draw-item arrays, driving the bucket
// registry's refcount-diff sync in the same single-pass main-thread walk.
void Flux_BuildGPUScene(const Zenith_Vector<Flux_GPUSceneSourceItem>& xItems,
	Flux_GPUSceneBucketRegistry& xRegistry, Flux_GPUSceneBuildResult& xOut);

// ---- incremental build API (Stage 3) ----------------------------------------
// Lets the renderer fold TWO sources into ONE scene under a single bucket-registry
// sync: snapshot statics (multi-submesh model items) AND instance-group foliage
// (single-submesh per instance). Drive: Begin -> (AppendItem | AppendInstance)* -> End.
// Flux_BuildGPUScene above is now a thin wrapper over these (so its golden-hash tests
// still pin the exact output).
void Flux_BeginGPUSceneBuild(Flux_GPUSceneBuildResult& xOut, Flux_GPUSceneBucketRegistry& xRegistry);

// Append one multi-submesh source item (snapshot-static shape): 1 object + N draw-items.
void Flux_AppendGPUSceneItem(const Flux_GPUSceneSourceItem& xItem,
	Flux_GPUSceneBucketRegistry& xRegistry, Flux_GPUSceneBuildResult& xOut);

// Append one single-submesh instance (foliage shape): 1 object (carrying the instance
// transform + VAT anim) + 1 draw-item, with NO per-instance source-item allocation —
// the path that scales to thousands of trees built CPU-side each frame.
void Flux_AppendGPUSceneInstance(Flux_GPUSceneBucketRegistry& xRegistry, Flux_GPUSceneBuildResult& xOut,
	const Zenith_Maths::Matrix4& xModel, u_int uObjFlags, u_int uVATAnimPacked, u_int uVATAnimTimeBits,
	const Flux_GPUSceneBucketKey& xKey, const Zenith_Maths::Vector4& xLocalBoundsSphere, u_int uColorTintPacked);

// Append one compute-skinned animated submesh-instance (Stage 5): 1 object (carrying the
// instance transform + its skeleton's bone-palette base + the SKINNED flag; no VAT) + 1
// draw-item in a per-instance skinned bucket (xKey.m_uMeshGeometryId =
// uFLUX_GPUSCENE_SKINNED_MESH_BIT | skinIndex). The submesh's bind-pose bounding sphere
// (inflated for animation) is the cull primitive; the skinned arena slice the draw reads is
// resolved per-bucket at gather (via the skinIndex), not stored here.
void Flux_AppendGPUSceneSkinnedInstance(Flux_GPUSceneBucketRegistry& xRegistry, Flux_GPUSceneBuildResult& xOut,
	const Zenith_Maths::Matrix4& xModel, u_int uBonePaletteBase,
	const Flux_GPUSceneBucketKey& xKey, const Zenith_Maths::Vector4& xLocalBoundsSphere, u_int uColorTintPacked);

void Flux_EndGPUSceneBuild(Flux_GPUSceneBuildResult& xOut, Flux_GPUSceneBucketRegistry& xRegistry);

// FNV-1a golden hash over the built record arrays (deterministic: every struct
// pad is explicitly zeroed by the pack helpers). For characterisation tests.
u_int64 Flux_HashGPUSceneForTest(const Flux_GPUSceneBuildResult& xResult);
