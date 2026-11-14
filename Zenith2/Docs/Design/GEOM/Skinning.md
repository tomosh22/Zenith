# GEOM design: skinning, morph targets and the skinned-vertex pool

Section file of `Zenith2/Docs/Design/GEOM.md`. It specifies skinned mesh data in the cook and in `.zmesh`, the posed-extent precision check, the producer interface through which poses reach Flux, the skinned-vertex pool, compute skinning and morph targets. The normative source is `Zenith2/Docs/Rendering/VertexData.md → "Skinned vertex pool"`, `Zenith2/Docs/Rendering/GpuResources.md → "Pools"` and the passes `Zenith2/Docs/Rendering/PerPass/Common.md → "ComputeSkinning"` and `Zenith2/Docs/Rendering/PerPass/Common.md → "MorphTargets"`. Skeletons, poses and animation are ANIM's; GEOM owns the mesh-side data and the GPU deformation.

## Scope of this file

| Topic | Heading |
|---|---|
| the skin data a mesh carries | "Skinned mesh data" |
| a rigid prop skinned to one joint | "Rigid attachment setting" |
| the FLOAT16 pool precision check | "Posed-extent check" |
| how poses reach Flux | "Flux_Skinning header" |
| the pool's layout, allocation and double buffering | "Skinned vertex pool layout" |
| the per-frame matrices | "Skinning palette computation" |
| the frame's batch and the compute pass | "Flux_SkinningBatch", "Compute skinning dispatch" |
| morph targets | "Morph targets" |
| motion vectors | "Velocity and history reset" |
| tests | "Skinning tests" |

## Skinned mesh data

A skinned mesh's stream 3 holds joints and weights ("Canonical stream-set layouts" of `Zenith2/Docs/Design/GEOM/VertexFormat.md`); joint values are mesh-local joint indices (0 … J − 1), so a mesh that uses 60 joints of a 200-joint skeleton stays UINT8x4. The `GEOM_SECTION_SKIN` section of `.zmesh`:

<!-- elaboration -->
| Offset | Size | Field |
|---|---|---|
| 0 | 16 | skeleton asset GUID (`Zenith_PersistentID`, binary form of [Z2-N-CONTRACT-067]) |
| 16 | 2 | mesh joint count J (1 … 1,024) |
| 18 | 2 | flags: bit 0 the skinned pool uses the FLOAT32x4 layout (set exactly when the cooked POS is `FLOAT32X3`, "Posed-extent check") |
| 20 | 4 | posed extent E (f32, metres) |
| 24 | 2 × J | skeleton joint index of each mesh joint (u16), the remap |
| 24 + 2J (padded to 4) | 48 × J | inverse bind matrix of each mesh joint, row-major float3x4 (12 × f32) |

- The skeleton itself (hierarchy, bind pose, names) is ANIM's asset; the mesh stores only what deformation needs. The cook verifies that every remap entry is below the skeleton's joint count and that the inverse bind matrices are finite and invertible (`GEOM_COOK_ERROR_SKIN_BIND`).
- **Skeleton properties read (A3 row AN4 (3)).** The skin cook reads the skeleton's properties by the exact names `anim.skeleton` publishes (`Zenith2/Docs/Design/ANIM/Formats.md → "Skeleton published properties"`), each with `ReadDependencyOutput(skeleton, name)`, which records a cook-input edge: `jointCount` (J, u32), `parents` (2 J), `restModel` (48 J), `restModelInverse` (48 J) and `maxReach` (f32). It reads no other skeleton property; `jointNameHashes`, `restPoseHash`, `curveNameHashes` and `shellMeshes` are ANIM's.
- **Rest-pose agreement (A3 row AN4 (2)).** A mesh binds only in the skeleton's rest pose: for every mesh joint j, the mesh's inverse bind matrix `a = IBM_j` and the skeleton's `b = restModelInverse[remap[j]]` must satisfy `max |a_ik − b_ik| ≤ fGEOM_SKIN_REST_POSE_TOLERANCE × max(1, max |b_ik|)` over the 12 affine entries, with `fGEOM_SKIN_REST_POSE_TOLERANCE` = 1e-5, the value of ANIM's `fANIM_REST_POSE_TOLERANCE` (`Zenith2/Docs/Design/ANIM.md`; declared in GEOM's cook as the GEOM/ANIM joint-limit pair is, `uGEOM_MAX_SKELETON_JOINTS` and `uANIM_MAX_JOINTS`). A mesh that fails is refused with `GEOM_COOK_ERROR_SKIN_REST_POSE` (`COOK_ERROR_STEP_FAILED`), its diagnostic naming the mesh, the mesh joint, the skeleton joint and the largest difference; no step rebinds or re-poses the mesh. `Test_Cook_SkinRestPoseMismatchRejected` asserts it.
- **Shell distance (A3 row AN4 (4)).** The skin cook publishes, per skinned mesh, the property named exactly `skinShellDistance` with `PublishProperty`: J little-endian f32 values indexed by SKELETON joint (J the skeleton's `jointCount`), value j the largest bind-pose distance from skeleton joint j (the translation of `inverse(IBM)` of the mesh joint remapped to j) to a vertex with a non-zero quantised weight on that joint, and 0 for a joint no vertex is weighted to. Only ANIM's `anim.clip` step reads it, for the meshes a rig document lists in `shellMeshes`; `anim.skeleton` reads no mesh, so the cook has no cycle. `Test_Cook_SkinShellDistancePublished` asserts it.
- Influences: at most four per vertex after the rule of "Weight quantisation" (`Zenith2/Docs/Design/GEOM/VertexFormat.md`).

## Rigid attachment setting

<!-- elaboration: answers A3 row CH6 (a) and A5 row ChB9 (a) -->
A rigid prop that follows one joint, such as a bow in a hand, is cooked as a skinned mesh of that one joint (`Zenith2/Docs/Design/CHAR/Modular.md → "Rigid attachments"`). The mesh import setting `skin.rigidAttach` (absent by default) selects this. CHAR writes it into the prop's `.meta`:

```json
"skin.rigidAttach": { "skeleton": "<skeleton GUID, 36 characters>", "joint": "<joint name>",
                      "offset": { "position": [x, y, z], "rotation": [x, y, z, w] } }
```

When the setting is present, the mesh step runs these steps after step 1 of "Mesh cook pipeline" (`Zenith2/Docs/Design/GEOM/Meshes.md`):

| Step | Action | Failure |
|---|---|---|
| A1 | the source must be unskinned. The setting must parse: a GUID, a joint name of 1 … 63 UTF-8 bytes, a finite position and a finite rotation whose length is within 1e-3 of 1. The rotation is then normalised | `GEOM_COOK_ERROR_SKIN_RIGID_ATTACH`, naming the member |
| A2 | read the skeleton's published `jointCount`, `restModel`, `restModelInverse`, `maxReach` and `jointNameHashes` with `ReadDependencyOutput`, so the skeleton's key joins the mesh's, and find the joint j whose `jointNameHashes` entry is `Zenith_Hash64` of the name | `GEOM_COOK_ERROR_SKIN_RIGID_ATTACH`, naming the joint when no entry matches |
| A3 | transform every vertex by `T = restModel[j] · TRS(position, rotation, 1)`, composed in `double` and rounded once to `float`: positions by T, normals by the inverse transpose of T's 3 × 3 part, tangent directions by the 3 × 3 part, each normalised, and the tangent sign multiplied by the sign of the determinant | — |
| A4 | make the source skinned: skeleton the setting's GUID, mesh joint count 1, remap `[j]`, the inverse bind matrix the 48 bytes of `restModelInverse[j]`, every vertex's joints (0, 0, 0, 0) and weights (1, 0, 0, 0) | — |

The pipeline then continues at step 2 as for any skinned mesh. Its stream-set has SKIN `J8W8`, and the posed-extent check reads `maxReach`. The rest-pose agreement of "Skinned mesh data" holds by construction, because the inverse bind is the skeleton's own entry. The setting's canonical JSON bytes join the mesh's derived-data key after the preset keys, so a changed socket offset re-cooks the prop. `Test_Cook_RigidAttachSkinsToJoint` cooks a quad attached to joint 2 of a 3-joint fixture skeleton with a 90° offset about +Z. It checks the skin section, the transformed positions within 1e-6 m and that the rest-pose check passes. `Test_Cook_RigidAttachRejected` checks one failing case per row: a skinned source, an unknown joint, a rotation of length 0.5 and a malformed GUID.

## Posed-extent check

The pool stores FLOAT16x4 object-space positions ([Z2-N-REND-891]); the cook checks FLOAT16 precision against a conservative posed extent, because the posed extent is unknown at cook time ([Z2-N-REND-892]):

```
E = |p_root| + R + D
	p_root = the bind-pose object-space position of the root joint (the joint with no parent among the skeleton joints the mesh uses; the skeleton's root when several)
	R      = max over mesh joints j of Σ over the chain root → j of |p_child − p_parent|   (bind-pose joint positions from the inverse of each inverse bind matrix)
	D      = max over vertices of the bind-pose distance from the vertex to its most-weighted joint
halfError(E) = 2^(floor(log2(E)) − 11)               // half of one FLOAT16 ULP at magnitude E
pool layout  = halfError(E) ≤ precision.posedMetres (default 0.001 m) ? FLOAT16x4 : FLOAT32x4
```

<!-- informative -->
Worked values: a human skeleton rooted at the hips (|p_root| ≈ 1.0 m, R ≈ 1.0 m, D ≈ 0.05 m) gives E ≈ 2.05 m, floor(log2 E) = 1 and a half error of 2^-10 m ≈ 0.98 mm, which passes; a 5 m creature gives 1.95 mm and takes the FLOAT32 layout, reported like every fallback.
<!-- /informative -->

- The pool layout and the bind-position format are tied (`Zenith2/Docs/Design/GEOM/VertexFormat.md → "Fallback derivation rule"`): FLOAT32x4 is chosen when the posed-extent check fails or the mesh's position precision check fails, and that choice sets the cooked POS to `FLOAT32X3` as well. The chosen layout sets the mesh-info flag and the skin section flag. The report and the file's precision section follow the record rule of `Zenith2/Docs/Design/GEOM/Meshes.md → "Precision fallback report format"`: a failed posed-extent check writes a `skinnedPool` record with its own metric and tolerance, and, when the position check passed, a forced `position` record carrying `"tiedBy": "bPERM_POSITION32"`. A failed position check alone writes only its `position` record, and the FLOAT32 pool shows in its `rasterStreamSet` (`Test_Cook_PosedExtentFallback`, `Test_Cook_SkinnedPositionTie`). The raster code (`POOL16` or `POOL32`) follows from the cooked code through `Zenith_GetRasterStreamSet`.

## Flux_Skinning header

The producer interface: every frame, render extraction writes one `Flux_RenderSkinnedPose` per skinned instance through `Flux_RenderWorldWriter::AddSkinnedPose` (`Zenith2/Docs/Rendering.md → "The render world hand-off"`); the writer is the runtime's extractor for skinned mesh components, fed by ANIM's pose buffers. GEOM declares the value type and validates it.

```cpp
// Zenith2/Engine/ZenithFlux/Flux_Skinning.h
#pragma once
#include "ZenithCore/Zenith_Span.h"
#include "ZenithFlux/Flux_MeshResource.h"
#include "ZenithFlux/Flux_RenderWorld.h"          // Flux_RenderInstanceKey (RPROD, Zenith2/Docs/Design/RPROD.md "RPROD public root headers")

constexpr uint32 uGEOM_MAX_SKELETON_JOINTS = 1024;
constexpr uint32 uGEOM_MAX_MORPH_TARGETS = 256;
constexpr uint32 uGEOM_MAX_ACTIVE_MORPH_TARGETS = 16;

/// @contract value Flux_JointMatrix
/// Ownership: Plain value; the row-major 3×4 model-space transform of one skeleton joint.
/// Thread affinity: IMMUTABLE.
///   Members non-const: the pose writer fills the rows in place in the render world.
/// Invalidation: None.
/// Units: Metres (translation); dimensionless rotation and scale.
/// Errors: None; Flux validates finiteness.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Flux_JointMatrix { float m_afRows[12]; };

enum Flux_SkinnedPoseFlag : uint32
{
	FLUX_SKINNED_POSE_FLAG_NONE = 0,
	FLUX_SKINNED_POSE_FLAG_HISTORY_RESET = 1u << 0,   // a teleport or first frame: previous positions := current
};

/// @contract value Flux_RenderSkinnedPose
/// Ownership: Written into the sealed render world's arena by the extractor; the spans point into that arena and live
///   until the render world slot is reset (Zenith2/Docs/Rendering.md "The render world hand-off").
/// Thread affinity: IMMUTABLE after Render.Seal.
///   Members non-const: the extractor fills each record field by field in the render world before Render.Seal.
/// Invalidation: The spans are invalid after the render subgraph that read the slot completes.
/// Units: Model-space metres; morph weights dimensionless in [-1, 1].
/// Errors: Flux_SkinningSystem validates each pose; an invalid pose skips that instance's skinning for the frame
///   (the instance draws its previous pool contents) and counts Flux.Skinning.InvalidPoses.
/// Version: since Z2 Stage 3. Never serialized.
/// @endcontract
struct Flux_RenderSkinnedPose
{
	Flux_RenderInstanceKey m_xInstance;                 // the GPU-scene instance this pose deforms (RPROD's key type)
	Flux_MeshID m_xMesh;
	Zenith_Span<const Flux_JointMatrix> m_axJoints;      // indexed by skeleton joint index; count ≥ max remap + 1
	Zenith_Span<const float> m_afMorphWeights;           // indexed by morph target; count equals the mesh's target count, or 0
	uint32 m_uFlags = FLUX_SKINNED_POSE_FLAG_NONE;
};

/// @contract type Flux_SkinningSystem
/// Ownership: Owned by Flux_Renderer; owns the skinned-vertex pool and its slot allocator.
/// Thread affinity: RENDER_SUBMIT_THREAD. Its members run on the Flux.BuildGraph node, which the render-submit thread
///   drives; the palette computation fans out as sub-jobs of that node.
/// Invalidation: None.
/// Units: Bytes; vertices.
/// Errors: Flux_GeometryError: FLUX_GEOMETRY_ERROR_POOL_FULL (the instance is not skinned and not drawn),
///   FLUX_GEOMETRY_ERROR_INVALID_POSE.
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 3.
/// @endcontract
class Flux_SkinningSystem
{
public:
	uint32 GetSkinnedInstanceCount() const;
	uint32 GetSkinnedVertexCount() const;        // this frame
	uint64 GetPoolBytesUsed() const;
	uint64 GetPoolBytesCapacity() const;
};

/// @contract functions Flux_SkinnedPoseValidation
/// Ownership: Pure function.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: As Flux_RenderSkinnedPose.
/// Errors: FLUX_GEOMETRY_ERROR_INVALID_POSE when a joint matrix is non-finite, the joint span is shorter than the
///   mesh's largest remap index + 1, or the morph weight count is neither 0 nor the mesh's target count.
/// Version: since Z2 Stage 3.
/// @endcontract
Zenith_Result<void, Flux_GeometryError> Flux_ValidateSkinnedPose(const Flux_RenderSkinnedPose& xPose,
	const Flux_MeshResource& xMesh);
```

<!-- assumption: pending RPROD -->
`Flux_RenderSkinnedPose` is the element type of `Flux_RenderWorldSection::m_axSkinnedPoses` (Rendering declares the member, not the type); GEOM declares it here and RPROD's render-world design cites this header.
<!-- /assumption -->

### Flux_SkinningBatch

`Flux_AddSkinning(Flux_RenderGraph&, const Flux_SkinningBatch&, Flux_FrameShared&)` (`Zenith2/Docs/Rendering/PerPass/Common.md → "ComputeSkinning"`) is a prologue feature function declared in the internal prologue header (`Zenith2/Docs/Design/RPROD/Passes.md → "Prologue feature functions"`, which names the type GEOM's). Its batch holds upload-ring allocations, whose type lives in `ZenithFlux/Internal/Upload/`, so the batch is declared in `Zenith2/Engine/ZenithFlux/Internal/Skinning/Flux_SkinningBatch.h`, which the prologue header includes:

<!-- elaboration -->
```cpp
// Zenith2/Engine/ZenithFlux/Internal/Skinning/Flux_SkinningBatch.h
#include "ZenithFlux/Flux_Skinning.h"
#include "ZenithFlux/Internal/Upload/Flux_UploadRing.h"

/// @contract value Flux_SkinningDispatch
/// Ownership: Plain value inside a Flux_SkinningBatch.
/// Thread affinity: IMMUTABLE.
///   Members non-const: the builder fills it field by field in the frame arena before it becomes immutable.
/// Invalidation: The descriptor handles are the pools' current buffers for this frame only.
/// Units: Records; thread groups of 64.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Flux_SkinningDispatch
{
	Zenith_VertexStreamSetCode m_xCookedCode;          // the dispatch's cooked skinned code
	uint32 m_uFirstRecord = 0;                         // into the batch's Flux_SkinDispatchRecord array
	uint32 m_uRecordCount = 0;
	uint32 m_uGroupCount = 0;                          // Σ ceil(vertexCount / 64)
	bool m_bJoints16 = false;                           // bPERM_JOINTS16
	bool m_bWeights16 = false;                          // bPERM_WEIGHTS16
	bool m_bPosition32 = false;                         // bPERM_POSITION32
	Zenith_RHIDescriptorHandle m_xSkinSourceStream0;    // the cooked code's skin-source pool, stream 0 (bind positions)
	Zenith_RHIDescriptorHandle m_xSkinSourceStream3;    // the same pool, stream 3 (joints, weights)
	Zenith_RHIDescriptorHandle m_xRasterStream1;        // the raster code's pool, stream 1 (bind QTangent)
};

/// @contract value Flux_SkinningBatch
/// Ownership: Frame-scoped; built by Flux_SkinningSystem on the Flux.BuildGraph node in the frame arena; the upload
///   allocations belong to the frame's upload ring and retire with it.
/// Thread affinity: OWNER_THREAD (the render-submit thread that built it).
/// Invalidation: Invalid after the frame's graph is compiled.
/// Units: Records; bytes; vertices.
/// Errors: None; Flux_AddSkinning cannot fail.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Flux_SkinningBatch
{
	Zenith_Span<const Flux_SkinningDispatch> m_axSkinDispatches;    // ascending cooked code; empty: no ComputeSkinning pass
	Zenith_Span<const Flux_SkinningDispatch> m_axMorphDispatches;   // the same grouping over instances with active targets; empty: no MorphTargets pass
	Flux_UploadAllocation m_xDispatchRecords;                       // Flux_SkinDispatchRecord × every record of both lists
	Flux_UploadAllocation m_xPalette;                               // Flux_JointMatrix × Σ joint counts, pose order
	Flux_UploadAllocation m_xMorphWeights;                          // per morph record: 16 × (target index u32, weight f32), unused entries weight 0
	uint32 m_uMorphScratchVertices = 0;                            // vertex count of the morph scratch transient (36 bytes each)
	uint32 m_uSkinnedVertexCount = 0;                              // Flux.Skinning.Vertices
};
```

`Flux_SkinningSystem` builds the batch in step 7 of `Zenith2/Docs/Design/GEOM.md → "Frame-level flow of GEOM work"`. `Flux_AddSkinning` declares `MorphTargets` when `m_axMorphDispatches` is non-empty and `ComputeSkinning` when `m_axSkinDispatches` is non-empty. Both passes are async compute ([Z2-N-REND-791]), with one dispatch per `Flux_SkinningDispatch`, in order.

## Skinned vertex pool layout

<!-- elaboration -->
| Property | Value |
|---|---|
| buffers | two graph buffers, current and previous, persistent-imported and swapped every frame ([Z2-N-REND-552]) |
| addressing | 16-byte granules; an instance's `m_uPoolSlot` (`Zenith2/Docs/Rendering/GpuResources.md → "Instance record (128 bytes)"`) is its first granule, the same in both halves |
| FLOAT16 layout | per vertex 16 bytes: position FLOAT16x4 (x, y, z, w = 1.0) then QTangent SNORM16x4; vertex v of the slot starting at granule `g` is at byte `g × 16 + v × 16` |
| FLOAT32 layout | per vertex 24 bytes: position FLOAT32x4 then QTangent SNORM16x4; vertex v is at byte `g × 16 + v × 24`, so a vertex may straddle a granule boundary (the buffer is a byte-address buffer; granules are an allocation unit only), and a slot is `ceil(24 × vertexCount / 16)` granules |
| vertex order | LOD 0 part order; the pool holds the instance's LOD 0 alone, and other LODs of a skinned mesh are skinned only when selected (below) |
| capacity | within `Z2-B-GEOM-001` Unified vertex and index pools ([Z2-N-REND-553]); initial 64 MiB per half desktop, 16 MiB mobile, growth by resize-and-copy |
| allocation | a TLSF allocator of granules ("Pool allocator" of `Zenith2/Docs/Design/GEOM/Meshes.md`); a slot is allocated when an instance first appears in a pose list and freed through deferred deletion when it has been absent for 2 frames. `Zenith2/Docs/Design/RHI/Memory.md → "Pool allocation"` gives this pool a best-fit allocator over vertex counts at 64-vertex granularity; GEOM keeps 16-byte granules, because FLOAT16 (16-byte) and FLOAT32 (24-byte) slots share one buffer and a vertex count does not name a byte offset across both. The reconciliation is cross-area row GM23 (assumption below) |
| buffer usage | `STORAGE_READ` \| `STORAGE_WRITE` \| `TRANSFER_SRC` \| `TRANSFER_DST` |

<!-- assumption: pending RHI -->
The skinned pool's allocator unit is the 16-byte granule of the table above and `m_uPoolSlot` is a granule index, not the 64-vertex ranges of `Zenith2/Docs/Design/RHI/Memory.md → "Pool allocation"`; cross-area row GM23 asks RHI to adopt the granule unit (or to cite this table) in its pool-allocation row.
<!-- /assumption -->

- LOD: skinning deforms the LOD the instance drew in the previous frame (its visibility bits and LOD are read back through the culling counters with one frame of latency); a LOD change reallocates the slot to the new LOD's vertex count, marks the instance history-reset for one frame, and counts `Flux.Skinning.LODSwitches`. A skinned instance with no previous frame skins LOD 0.
- Raster passes draw a skinned instance with the raster code `pos-pool16x4|…` or `pos-pool32x4|…`, through the `bPERM_SKINNED = true` permutation of each program. That permutation's vertex input is the program's one struct, `POSITION` included: the pool code has no stream 0, so its specialised table binds `POSITION` to the null stream (SNORM16X4 at offset 0 of stream 7) and the permutation ignores the zero it reads (`Zenith2/Docs/Design/GEOM/VertexFormat.md → "Program stream-set specialisation"`). The vertex stage reads position and QTangent through `Flux_FetchPoolVertex(xPool, uGranule, uVertex, uPosition)`, where `uPosition` is `uFLUX_STREAMSET_POSITION` (the FLOAT16 or FLOAT32 layout above); UV0 and the other stream-1 and stream-2 attributes come through the fixed-function unit from the mesh's raster pool, which is keyed by the raster code (`Zenith2/Docs/Design/GEOM/Meshes.md → "Geometry pools"`).

## Skinning palette computation

On the CPU, in `Flux.BuildGraph`, as a `ParallelFor` over the frame's poses (`Zenith2/Docs/Design/JOBS/PublicAPI.md → "Zenith_JobContext header"`): for each pose and each mesh joint j, `S_j = M_{remap[j]} × B_j` where M is the pose's model-space joint matrix and B the mesh's inverse bind matrix (both 3×4, the implicit fourth row (0, 0, 0, 1)), written row-major into the frame's upload ring as `Flux_JointMatrix`. Poses are processed in render-world order, and each pose's palette offset is its prefix sum of joint counts, so the palette layout is independent of the worker count. The CPU cost at W-Characters (100 characters × 60 joints) is 6,000 matrix products per frame, and is measured inside `Z2-B-RHI-004` Render graph build and compile CPU, the row of the `Flux.BuildGraph` node.

## Compute skinning dispatch

<!-- elaboration -->
`Flux_SkinDispatchRecord` is the `[FluxShared]` struct of `Zenith2/Shaders/Common/Skinning.slang` (32 bytes):

| Offset | Size | Field | Meaning |
|---|---|---|---|
| 0 | 4 | `m_uGroupBase` | prefix sum, over the dispatch's records, of `ceil(vertexCount / 64)` |
| 4 | 4 | `m_uVertexCount` | vertices of the skinned LOD |
| 8 | 4 | `m_iSourceVertexOffset` | first vertex of the skinned LOD in the mesh's skin-source pool (keyed by the cooked code), whose streams 0 and 3 the dispatch reads at this vertex plus the thread's vertex index: the skin-source range's base plus the LOD's first vertex |
| 12 | 4 | `m_uPaletteOffset` | first `Flux_JointMatrix` of this instance in the palette |
| 16 | 4 | `m_uOutputGranule` | the instance's `m_uPoolSlot` |
| 20 | 4 | `m_uPrimitive` | the primitive record of the skinned LOD's first non-empty section. Its position scale and bias dequantise stream 0, and its `m_iVertexOffset` is the LOD's first vertex in the raster pool, where the dispatch reads stream 1 (the bind QTangent). The sections before it in the LOD are empty and hold no vertices, so that offset is the LOD's first |
| 24 | 4 | `m_uMorphScratchOffset` | first vertex of this instance in the morph scratch, `0xFFFFFFFF` when no target is active |
| 28 | 4 | `m_uFlags` | bit 0 history reset |

| Step | Action |
|---|---|
| 1 | group the frame's skinned instances by cooked stream-set code (one dispatch per skinned-mesh layout, as `Zenith2/Docs/Rendering/PerPass/Common.md → "ComputeSkinning"` specifies), in ascending code order, instances in render-world order within a code; each dispatch binds its code's skin-source pool and its raster code's pool and selects its permutation by the table below |
| 2 | write the records and the palette into the upload ring, as the `Flux_SkinningBatch` of "Flux_SkinningBatch" |
| 3 | dispatch `Σ groups` groups of 64 threads; each group finds its record by binary search over `m_uGroupBase`, and each thread one vertex |
| 4 | per vertex: read the bind position (stream 0 of the skin-source pool: SNORM16x4 dequantised with the primitive's scale and bias when `bPERM_POSITION32` is false, FLOAT32x3 when it is true), or the morphed position from the scratch when the record has one; read the QTangent (stream 1 of the raster pool) and decode it; read joints (UINT8x4, or UINT16x4 when `bPERM_JOINTS16`) and weights (UNORM8x4, or UNORM16x4 when `bPERM_WEIGHTS16`) from stream 3 of the skin-source pool; blend `M = Σ_i w_i S_{j_i}` in lane order i = 0 … 3 with kernel operations; `p' = M · p`, `t' = M · t`, `n' = M · n` (upper 3×3) |
| 5 | re-encode the tangent frame with `Flux_EncodeQTangent(t', n', sign)` (the bias rule, [Z2-N-REND-893]) and write position (FLOAT16x4 through `Flux_F32ToF16Bits` when `bPERM_POSITION32` is false, FLOAT32x4 when it is true) and QTangent to the current pool half |

The dispatch uses Rendering's enumerated permutations of `ComputeSkinning`; no format is read from data at run time.

| Permutation constant | Value for a dispatch of cooked code c |
|---|---|
| `uPERM_WEIGHTS_PER_VERTEX` | 4, always |
| `bPERM_JOINTS16` | SKIN(c) ∈ {`J16W8`, `J16W16`} |
| `bPERM_WEIGHTS16` | SKIN(c) ∈ {`J8W16`, `J16W16`} |
| `bPERM_POSITION32` | POS(c) = `FLOAT32X3`, which selects both the FLOAT32x3 bind-position input and the FLOAT32x4 pool output |

One constant serves both position formats because the cook ties them: a skinned mesh has FLOAT32X3 bind positions exactly when its pool layout is FLOAT32x4 ("Posed-extent check"). The cost is 4 extra bytes per bind vertex on the rare mesh that fails only one of the two checks; the alternative, a fourth constant, would double the pass's permutations for that case alone. The shader compiler builds all 8 combinations of the three booleans; `Test_Skinning_PermutationPerCode` asserts the mapping above for each of the 144 cooked skinned stream-sets of `Zenith2/Docs/Design/GEOM/VertexFormat.md → "Declared stream-sets"`.

The skinning is linear blend skinning with at most four weights (`uPERM_WEIGHTS_PER_VERTEX` = 4); dual-quaternion skinning is not provided. Every arithmetic step uses the kernel, so skinning results are identical on every conforming device, which is what lets the velocity pass difference the two halves without noise.

## Morph targets

<!-- elaboration -->
`GEOM_SECTION_MORPH` (one per LOD that has targets):

| Part | Layout |
|---|---|
| header | target count T (u16, at most 256), reserved u16, delta count total u32 |
| target table | T × { name hash u64 (`Zenith_Hash64` of the UTF-8 name), first delta u32, delta count u32, max position delta f32, reserved u32 } (24 bytes) |
| deltas | 24 bytes each: vertex index u32 (LOD-local), position delta FLOAT16x3 + pad (8 bytes), normal delta FLOAT16x3 (6 bytes), tangent delta FLOAT16x3 (6 bytes) |

- The cook keeps a delta only when its position delta exceeds 1e-5 m or its normal or tangent delta exceeds 1e-4 (sparse storage); deltas of a generated LOD are the LOD 0 deltas of the vertices the LOD kept (a simplified LOD's vertices are a subset of LOD 0's), found through the simplifier's vertex remap.
- The deltas live in the morph-delta pool (`Zenith2/Docs/Design/GEOM/Meshes.md → "Geometry pools"`).
- The `MorphTargets` pass (`Zenith2/Docs/Rendering/PerPass/Common.md → "MorphTargets"`) runs before skinning for instances with non-zero weights, grouped by cooked code like the skinning dispatch and with `bPERM_POSITION32` = (POS = `FLOAT32X3`) selecting the bind-position read: it reads bind positions from the skin-source pool and the bind QTangent from the raster pool at the offsets of the skinning record, copies the instance's bind positions, normals and tangents into the morph scratch (a frame transient, 36 bytes per vertex in FLOAT32 form, `[FullCoverage]`), then adds `Σ_k w_k Δ_k` for the at most `uGEOM_MAX_ACTIVE_MORPH_TARGETS` = 16 targets of largest `|w_k|` (ties by target index), deltas applied target by target in ascending target index; skinning reads the scratch.
- A pose's weight count must be 0 or T ("Flux_Skinning header"); weights outside [−1, 1] are clamped and counted in `Flux.Skinning.MorphWeightsClamped`.

## Velocity and history reset

- Each frame the pool halves swap; skinning writes the current half and velocity (the prepass, the G-buffer pass and the visibility-buffer velocity reconstruction) reads the previous half at the same granule ([Z2-N-REND-552], [Z2-N-REND-854]).
- An instance whose pose carries `FLUX_SKINNED_POSE_FLAG_HISTORY_RESET`, whose slot was just allocated or reallocated, or whose instance record carries `FLUX_INSTANCE_FLAG_HISTORY_RESET`, has its skinning output written to both halves that frame, so its velocity is zero (`Test_Skinning_HistoryResetZeroVelocity`).

## Skinning tests

<!-- elaboration -->
| Test | File | Asserts |
|---|---|---|
| `Test_Cook_PosedExtentFallback` | `Zenith2/Tools/ZenithCook/Tests/Geometry/Zenith_CookSkin_Tests.cpp` | the human fixture (E ≈ 2.05 m) keeps FLOAT16x4; a 5 m fixture takes FLOAT32x4 with a report record; E is computed exactly as the formula |
| `Test_Cook_SkinWeightsTopFour` | same | an eight-influence fixture keeps the four largest, ties by lower joint, renormalised, summing to 255 |
| `Test_Cook_SkinJoints16Fallback` | same | a 300-joint mesh takes UINT16x4 joints |
| `Test_Cook_MorphDeltasSparse` | same | deltas below the thresholds are dropped; LOD deltas map through the simplifier remap |
| `Test_Cook_SkinRestPoseMismatchRejected` | same | a mesh whose inverse bind for one joint differs from the skeleton's `restModelInverse` by 2e-5 relative fails with `GEOM_COOK_ERROR_SKIN_REST_POSE` naming both joints and the difference; a difference of 5e-6 relative cooks; a joint whose skeleton entries are all below 1 uses the absolute bound 1e-5 |
| `Test_Cook_SkinShellDistancePublished` | same | a 3-joint skeleton with a 2-joint mesh remapped to joints 0 and 2 publishes `skinShellDistance` of 3 f32 values (12 bytes): joint 1 is 0, joints 0 and 2 equal the largest bind-pose distance of their weighted vertices; a vertex with a quantised weight of 0 on a joint does not count |
| `Test_Cook_RigidAttachSkinsToJoint`, `Test_Cook_RigidAttachRejected` | `Zenith2/Tools/ZenithCook/Tests/Geometry/Zenith_CookSkin_Tests.cpp` | "Rigid attachment setting" |
| `Test_Skinning_MorphTargetNameHash` | `Flux_Skinning_Tests.cpp` | `GetMorphTargetNameHash(k)` returns `Zenith_Hash64` of target k's name in table order for a three-target seed; an index at or beyond `GetMorphTargetCount()` asserts |
| `Test_Skinning_PaletteLayoutWorkerIndependent` | `Zenith2/Engine/ZenithFlux/Tests/Skinning/Flux_Skinning_Tests.cpp` | the palette bytes are identical with 1, 2 and 8 workers |
| `Test_Skinning_MatchesCpuReference` | same | GPU skinning of a fixture pose equals a CPU implementation written with the kernel, bit for bit, on WARP and lavapipe, for all 8 permutations of `bPERM_JOINTS16`, `bPERM_WEIGHTS16` and `bPERM_POSITION32` |
| `Test_Skinning_PermutationPerCode` | same | each of the 144 cooked skinned stream-sets selects the permutation of the table in "Compute skinning dispatch", binds its own skin-source pool and the raster pool of its raster code, and reads stream 1 at its record primitive's `m_iVertexOffset` |
| `Test_Skinning_BatchLayout` | same | for a fixed frame of 12 skinned instances over 3 cooked codes, 2 with active morph targets: the batch has 3 skin dispatches in ascending code order and 1 or 2 morph dispatches, record group bases are prefix sums of `ceil(vertexCount / 64)`, palette offsets are prefix sums of joint counts, the descriptor handles name the matching pools, and an empty frame declares neither pass |
| `Test_Skinning_Float32PoolAddressing` | same | a 5-vertex FLOAT32 slot reads and writes vertex v at byte `g × 16 + v × 24`, including the vertex that straddles a granule, and occupies `ceil(120 / 16)` = 8 granules |
| `Test_Skinning_QTangentReencodedWithBias` | same | skinned frames whose `w` falls below one quantum decode with the correct sign |
| `Test_Skinning_InvalidPoseSkipped` | same | a short joint span and a NaN matrix are rejected, counted, and the instance keeps its previous pool contents |
| `Test_Skinning_HistoryResetZeroVelocity` | same | a reset instance produces zero velocity that frame |
| `Test_Skinning_LODSwitchReallocates` | same | a LOD change reallocates the slot and resets history for one frame |
| `Test_Skinning_MorphApplied` | same | a single-target fixture at weight 0.5 displaces vertices by half the delta, within one FLOAT16 quantum |
| `Test_Skinning_MorphTopSixteen` | same | with 20 non-zero weights, exactly the 16 of largest magnitude apply |
| `Test_GPUScene_SkinnedSameFrameAllPasses` | `Zenith2/Engine/ZenithFlux/Tests/GPUScene/` | the Rendering test of `Zenith2/Docs/Rendering/GpuResources.md → "GPU scene tests"`: skinned meshes drawn in depth, base and shadow passes the frame they are skinned, sync validation clean |
