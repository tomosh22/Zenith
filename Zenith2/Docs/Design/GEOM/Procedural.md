# GEOM design: procedural geometry

Section file of `Zenith2/Docs/Design/GEOM.md`. It specifies procedural geometry: vertices regenerated on the GPU into the procedural-geometry pool by a generator compute program and drawn like any GPU-scene instance. It covers the `procedural:` stream-sets, the pool and its slot layout, the generator interface `Zenith_ProceduralGeometrySource` that modules above ZenithFlux implement, the per-instance render-world record, `Flux_ProceduralGeometrySystem`, the frame value `Flux_ProceduralFrame`, the feature function `Flux_AddProceduralGeometry`, the draw path and the tests. The normative sources are `Zenith2/Docs/Rendering/GpuResources.md → "Pools"` (the procedural-geometry pool, its producer and consumers, [Z2-N-REND-551] and [Z2-N-REND-553]), `Zenith2/Docs/Rendering/PerPass/Common.md → "ProceduralGeometry"` (the pass) and `Zenith2/Docs/Rendering/FrameAnatomy.md → "Common prologue"` (row 5). `Zenith2/Docs/Design/RPROD/Passes.md → "Prologue feature functions"` assigns `Flux_AddProceduralGeometry` and `Flux_ProceduralFrame` to GEOM.

<!-- assumption: pending human ruling Q38 (orchestrator-ruled: GEOM owns procedural geometry; a human may override) -->
GEOM owns procedural geometry by the orchestrator's ruling on Q38. The plan's Appendix A row for GEOM does not list it. Everything else points at GEOM: the three core-doc references above, `Z2-B-GEOM-005` Procedural geometry regeneration GPU, the pool's memory inside `Z2-B-GEOM-001` ([Z2-N-REND-553]), and RPROD's pass table. If a human reassigns it, this file moves to the new owner with its requirements `Z2-R-GEOM-160` … `Z2-R-GEOM-167`, and the `PROCEDURAL` family value and `Z2-B-GEOM-005` go with it.
<!-- /assumption -->

## Scope of this file

| Topic | Heading |
|---|---|
| the words this file uses | "Procedural terms" |
| the stream-sets, the pool and the byte layout of a slot | "Procedural stream-sets and pool layout" |
| every public declaration | "Flux_ProceduralGeometry header" |
| registering a generator and admitting an instance | "Registration and sizing" |
| what happens each frame | "Per-frame regeneration" |
| the pass and its contract | "Flux_AddProceduralGeometry contract" |
| how procedural instances are drawn | "Drawing procedural instances" |
| failures | "Procedural failure handling" |
| tests | "Procedural tests" |

What GEOM does not own: procedural *placement* of meshes and foliage is ENV's (Z2-I-ENV-10), reproducible procedural *world generation* is WORLD's (Z2-I-WORLD-05), and GPU particles are VFX's. Those systems may implement a `Zenith_ProceduralGeometrySource`. They never write the pool themselves.

## Procedural terms

| Term | Meaning |
|---|---|
| generator | one registered `Zenith_ProceduralGeometrySource` and its compute program under `Zenith2/Shaders/Passes/Procedural/`, identified by its generator key, the `Zenith_Hash64` of its UTF-8 name (`Zenith2/Docs/Design/CORE/Maths.md → "Zenith_Hash64"`) |
| procedural instance | one GPU-scene instance with `FLUX_INSTANCE_FLAG_PROCEDURAL`, one generator, one fixed topology (vertex count, index list, bounds) and one pool slot |
| topology parameters | at most 256 bytes that fix an instance's vertex count, index list and bounds; they are read at admission and whenever the instance's topology version changes |
| frame parameters | exactly the generator's declared byte count (a multiple of 16, at most 256) that its program reads to compute vertex positions and attributes; they are sent in any frame that regenerates the instance |
| slot | the instance's contiguous vertex range in the pool, the same range in both stream regions |
| regeneration | the generator program rewriting every vertex of a slot in the `ProceduralGeometry` pass |

## Procedural stream-sets and pool layout

### Procedural stream-sets

The PROCEDURAL family (FAMILY 2 of `Zenith2/Docs/Design/GEOM/VertexFormat.md → "Stream-set code fields"`) has the two declared stream-sets of "Declared stream-sets". Both use the default stream layout of `Zenith2/Docs/Rendering/VertexData.md → "Default stream layout"`, which the Rendering pool table names:

| Code | Canonical name | Stream 0 | Stream 1 | Chosen when |
|---|---|---|---|---|
| `0x2010` (the family default) | `procedural:pos-snorm16x4|surface-qt16-uv16` | `POSITION` SNORM16X4, stride 8 | `QTANGENT` SNORM16X4, `TEXCOORD0` UNORM16X2, stride 12 | the generator declares `FLUX_PROCEDURAL_UV_FORMAT_UNORM16X2` |
| `0x2030` | `procedural:pos-snorm16x4|surface-qt16-uvf16` | as above | `QTANGENT` SNORM16X4, `TEXCOORD0` FLOAT16X2, stride 12 | the generator declares `FLUX_PROCEDURAL_UV_FORMAT_FLOAT16X2` (world-space tiling UVs, whose magnitude an UNORM16 box would quantise too coarsely) |

<!-- elaboration -->
- Positions are SNORM16X4 over the instance's declared bounds box, which `Zenith_MakePositionBox` builds from `Flux_ProceduralSizing::m_xBounds`. The half-quantum error of SNORM16 over any box is at most `7.6e-6 ×` its diagonal. That is below the position tolerance of [Z2-N-REND-879] (`max(2⁻¹⁶ × diagonal, 1 mm)`) for every box, as the derivation in `Zenith2/Docs/Rendering/VertexData.md → "Precision checks and fallbacks"` shows. Procedural geometry has no import preset that could tighten that tolerance, so the FLOAT32X3 position fallback can never be selected and the family declares none.
- The UV0 format is part of the generator's declaration and is not measured at run time, because generated UVs do not exist when the instance is admitted. For an UNORM16X2 generator, admission computes the bound `0.5 × max(uvMax − uvMin) / 65535 × m_uUVReferenceResolution` texels. When the bound exceeds 1/8 texel, admission raises the warning counter `Flux.Procedural.UVPrecisionWarnings` and logs once per generator under `flux.procedural`. This is the runtime counterpart of `GEOM_COOK_WARNING_UV_PRECISION`.
- The family has no SKIN, UV1 or colour values. `Zenith_IsReachableStreamSet` accepts exactly the two codes above for the family (`Zenith2/Docs/Design/GEOM/VertexFormat.md → "Reachable stream-set rule"`).

### Pool layout

The procedural-geometry pool is one buffer (`Flux_FrameShared::m_xProceduralPool`), owned by `Flux_ProceduralGeometrySystem`. It is persistent-imported every frame ([Z2-N-REND-551]) and holds two stream regions that share one vertex allocator:

<!-- elaboration -->
| Property | Value |
|---|---|
| capacity | C vertices; initial C = 262,144 on `FLUX_CAPABILITY_TIER_DESKTOP` and 65,536 on `FLUX_CAPABILITY_TIER_MOBILE`. These are proposed values, and the Stage 1 per-system budget ticket for GEOM replaces them inside `Z2-B-GEOM-001` |
| region 0 (stream 0) | bytes `[0, 8C)`: vertex v's position at byte `8v` |
| region 1 (stream 1) | bytes `[R1, R1 + 12C)` with `R1 = align(8C, 256)`: vertex v's QTangent at byte `R1 + 12v`, its UV0 at `R1 + 12v + 8` |
| total size | `R1 + 12C` bytes |
| slot | a vertex range `[first, first + n)` from the pool's TLSF allocator (`Zenith2/Docs/Design/GEOM/Meshes.md → "Pool allocator"`, element = one vertex); the instance record's `m_uPoolSlot` and the primitive record's `m_iVertexOffset` are both `first` |
| buffer usage union ([Z2-N-REND-889]) | `VERTEX` \| `STORAGE_READ` \| `STORAGE_WRITE` \| `TRANSFER_SRC` \| `TRANSFER_DST` |
| growth | when an allocation finds no block: the new capacity is `max(2C, C + n)`, clamped so that the sum of the geometry pools stays within `Z2-B-GEOM-001` ([Z2-N-REND-553]). The `GeometryPoolGrow` pass copies region 0 to the new region 0 and region 1 to the new region 1 (two buffer copies, because R1 moves). Every slot keeps its vertex index, so no record changes. The old buffer retires through deferred deletion |
| at the clamp | the admission fails with `FLUX_GEOMETRY_ERROR_POOL_FULL`, the instance is not drawn, `Flux.Procedural.PoolFull` increments, and nothing resident is evicted |
| index data | an instance's 16-bit indices live in the unified index pool (`Zenith2/Docs/Design/GEOM/Meshes.md → "Geometry pools"`). They are uploaded once at admission and again on a topology change, as `FLUX_UPLOAD_CLASS_MESH`. Index values are slot-local, because the draw's `vertexOffset` is the slot's first vertex |

The pool is single-buffered: Rendering's table gives it no previous half. A procedural instance's velocity therefore comes from its instance transform alone ([Z2-N-REND-552] double-buffers only the skinned pool), and vertex motion inside a slot does not reach the velocity target.

<!-- assumption: pending RHI -->
`Zenith2/Docs/Design/RHI/Memory.md → "Pool allocation"` gives the procedural-geometry pool "a range allocator over bytes at 256-byte granularity". GEOM's design needs a vertex allocator shared by the two stream regions, because one `vertexOffset` addresses both fixed-function streams of a draw. GEOM therefore allocates vertex ranges with its own TLSF allocator, keeps the 256-byte granularity only for `R1`, and hands the buffer to the GPU scene's frame import (`Zenith2/Docs/Design/RHI/Memory.md → "Flux_GPUScene"`, `ImportTables`). The reconciliation is cross-area row GM23.
<!-- /assumption -->

## Flux_ProceduralGeometry header

```cpp
// Zenith2/Engine/ZenithFlux/Flux_ProceduralGeometry.h
#pragma once
#include "ZenithCore/Zenith_Bounds.h"
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithRHI/Zenith_RHIShaderInterface.h"
#include "ZenithFlux/Flux_GeometryErrors.h"
#include "ZenithFlux/Flux_RenderWorld.h"          // Flux_RenderInstanceKey (RPROD, Zenith2/Docs/Design/RPROD.md "RPROD public root headers")

constexpr uint32 uGEOM_MAX_PROCEDURAL_GENERATORS = 64;
constexpr uint32 uGEOM_MAX_PROCEDURAL_INSTANCES = 16384;
constexpr uint32 uGEOM_MAX_PROCEDURAL_VERTICES = 65535;          // per instance: 16-bit slot-local indices
constexpr uint32 uGEOM_MAX_PROCEDURAL_INDICES = 393216;          // per instance, a multiple of 3
constexpr uint32 uGEOM_MAX_PROCEDURAL_PARAMETER_BYTES = 256;     // topology and frame parameters each
constexpr uint32 uGEOM_PROCEDURAL_THREADS_PER_GROUP = 64;        // one thread per vertex

/// @contract enum Flux_ProceduralUVFormat
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; RegisterSource rejects any other value (FLUX_GEOMETRY_ERROR_INVALID_DESC).
/// Version: since Z2 Stage 3.
/// @endcontract
enum Flux_ProceduralUVFormat : uint8
{
	FLUX_PROCEDURAL_UV_FORMAT_UNORM16X2 = 1,    // code 0x2010
	FLUX_PROCEDURAL_UV_FORMAT_FLOAT16X2,        // code 0x2030
};

/// @contract value Flux_ProceduralGeneratorDesc
/// Ownership: Plain value returned by Zenith_ProceduralGeometrySource::GetDesc; m_szName points to storage with static
///   duration (a string literal).
/// Thread affinity: IMMUTABLE.
///   Members non-const: callers default-construct it and fill its fields.
/// Invalidation: None.
/// Units: Bytes; texels.
/// Errors: Flux_ProceduralGeometrySystem::RegisterSource validates it ("Registration and sizing").
/// Version: since Z2 Stage 3.
/// @endcontract
struct Flux_ProceduralGeneratorDesc
{
	const char* m_szName = nullptr;                   // unique among registered generators; its Zenith_Hash64 is the key
	Zenith_RHIShaderProgramID m_xProgram;             // the generator's compute program, Zenith2/Shaders/Passes/Procedural/
	uint32 m_uFrameParameterBytes = 0;               // 16 … 256, a multiple of 16
	Flux_ProceduralUVFormat m_eUV0Format = FLUX_PROCEDURAL_UV_FORMAT_UNORM16X2;
	uint32 m_uUVReferenceResolution = 4096;          // texels; for the UNORM16 precision warning only
};

/// @contract value Flux_ProceduralSizing
/// Ownership: Plain value returned by Zenith_ProceduralGeometrySource::ComputeSizing.
/// Thread affinity: IMMUTABLE.
///   Members non-const: the producer default-constructs it and fills its fields before returning it.
/// Invalidation: None.
/// Units: Object-space metres (bounds); UV units.
/// Errors: Validated at admission ("Registration and sizing").
/// Version: since Z2 Stage 3.
/// @endcontract
struct Flux_ProceduralSizing
{
	uint32 m_uVertexCount = 0;                       // 1 … uGEOM_MAX_PROCEDURAL_VERTICES
	uint32 m_uIndexCount = 0;                        // 3 … uGEOM_MAX_PROCEDURAL_INDICES, a multiple of 3
	Zenith_AABB m_xBounds;                            // every position the generator writes lies inside (the writer clamps)
	Zenith_Vector2 m_xUV0Min;                         // every UV0 the generator writes lies inside [min, max]
	Zenith_Vector2 m_xUV0Max;
};

/// @contract interface Zenith_ProceduralGeometrySource
/// Ownership: Implemented by a module above ZenithFlux (ENV water surfaces, VFX ribbons, gameplay cables), or by
///   ZenithFlux itself for its ribbon generator; registered by reference through Flux_ProceduralGeometrySystem::RegisterSource;
///   the implementer keeps it alive until UnregisterSource returns.
/// Thread affinity: RENDER_SUBMIT_THREAD. Every member is called on the Flux.BuildGraph node alone, with bytes that come from
///   the sealed render world; a member reads no simulation state and no mutable state of its own.
/// Invalidation: None.
/// Units: As Flux_ProceduralSizing.
/// Errors: ComputeSizing and WriteIndices return Flux_GeometryError (INVALID_DESC for parameters they cannot interpret,
///   LIMIT for a topology over the limits); the instance is then rejected and counted, never asserted on.
/// Reentrancy: NONE. A member never calls back into Flux_ProceduralGeometrySystem.
/// Version: since Z2 Stage 3. Source API.
/// @endcontract
class Zenith_ProceduralGeometrySource
{
public:
	virtual ~Zenith_ProceduralGeometrySource() = default;
	// Called once, by RegisterSource; the result is kept for the generator's lifetime.
	virtual Flux_ProceduralGeneratorDesc GetDesc() const = 0;
	// Pure function of the topology parameters (at most 256 bytes).
	virtual Zenith_Result<Flux_ProceduralSizing, Flux_GeometryError> ComputeSizing(
		Zenith_ByteSpan xTopologyParameters) const = 0;
	// Writes exactly ComputeSizing(xTopologyParameters).m_uIndexCount slot-local indices, each below m_uVertexCount,
	// counter-clockwise front faces ([Z2-N-REND-885]); pure function of the topology parameters.
	virtual Zenith_Result<void, Flux_GeometryError> WriteIndices(Zenith_ByteSpan xTopologyParameters,
		Zenith_Span<uint16> auIndicesOut) const = 0;
};

/// @contract enum Flux_ProceduralInstanceFlag
/// Ownership: Plain enumeration of Flux_RenderProceduralInstance::m_uFlags bits.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; unknown bits reject the record.
/// Version: since Z2 Stage 3.
/// @endcontract
enum Flux_ProceduralInstanceFlag : uint32
{
	FLUX_PROCEDURAL_INSTANCE_FLAG_NONE = 0,
	FLUX_PROCEDURAL_INSTANCE_FLAG_REGENERATE = 1u << 0,   // regenerate the slot this frame from m_xFrameParameters
};

/// @contract value Flux_RenderProceduralInstance
/// Ownership: Written into the sealed render world's arena by an extractor; the spans point into that arena and live until the
///   render world slot is reset (Zenith2/Docs/Rendering.md "The render world hand-off").
/// Thread affinity: IMMUTABLE after Render.Seal.
///   Members non-const: the extractor fills each record field by field in the render world before Render.Seal.
/// Invalidation: The spans are invalid after the render subgraph that read the slot completes.
/// Units: Bytes.
/// Errors: Flux_ProceduralGeometrySystem validates each record; an invalid record drops that instance's change for the frame
///   and counts Flux.Procedural.Rejected.
/// Version: since Z2 Stage 3. Never serialized.
/// @endcontract
struct Flux_RenderProceduralInstance
{
	Flux_RenderInstanceKey m_xInstance;                // the GPU-scene instance (RPROD's key type)
	uint64 m_ulGeneratorKey = 0;                      // Zenith_Hash64 of the generator's name
	uint32 m_uTopologyVersion = 0;                    // a change from the admitted value re-runs sizing and indices
	uint32 m_uFlags = FLUX_PROCEDURAL_INSTANCE_FLAG_NONE;
	Zenith_ByteSpan m_xTopologyParameters;             // at most 256 bytes; read at admission and on a version change
	Zenith_ByteSpan m_xFrameParameters;                // the generator's m_uFrameParameterBytes when REGENERATE is set, else empty
};

/// @contract type Flux_ProceduralGeometrySystem
/// Ownership: Owned by Flux_Renderer, created after Flux_MeshManager (whose index pool it suballocates) and destroyed before it;
///   owns the procedural-geometry pool, its allocator, the generator table and the per-instance state.
/// Thread affinity: RENDER_SUBMIT_THREAD for every member; its per-frame work runs on the Flux.BuildGraph node.
/// Invalidation: A generator key is invalid after UnregisterSource.
/// Units: Bytes; vertices.
/// Errors: Flux_GeometryError: INVALID_DESC, LIMIT, SHUTTING_DOWN.
/// Reentrancy: NOT_APPLICABLE. It calls the sources, which never call back.
/// Version: since Z2 Stage 3.
/// @endcontract
class Flux_ProceduralGeometrySystem
{
public:
	Zenith_Result<uint64, Flux_GeometryError> RegisterSource(Zenith_ProceduralGeometrySource& xSource);
	void UnregisterSource(uint64 ulGeneratorKey);   // retires the generator's instances; returns after their slots are queued for retirement
	uint32 GetGeneratorCount() const;
	uint32 GetInstanceCount() const;
	uint64 GetPoolBytesUsed() const;
	uint64 GetPoolBytesCapacity() const;
};
```

`Flux_RenderInstanceKey` is RPROD's. The frame value and the members the renderer calls are internal, because `Flux_AddProceduralGeometry` is declared in the internal prologue header (`Zenith2/Docs/Design/RPROD/Passes.md → "Prologue feature functions"`) and `Flux_UploadAllocation` (`Zenith2/Docs/Rendering/GpuResources.md → "Upload ring"`) lives in `ZenithFlux/Internal/Upload/`. They are declared in `Zenith2/Engine/ZenithFlux/Internal/Procedural/Flux_ProceduralInternal.h`, which the prologue header includes:

```cpp
// Zenith2/Engine/ZenithFlux/Internal/Procedural/Flux_ProceduralInternal.h
#include "ZenithFlux/Flux_ProceduralGeometry.h"
#include "ZenithFlux/Internal/Upload/Flux_UploadRing.h"

/// @contract value Flux_ProceduralVertexRange
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: the producer default-constructs it and fills its fields before returning it.
/// Invalidation: None.
/// Units: Vertices of the procedural pool.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Flux_ProceduralVertexRange
{
	uint32 m_uFirstVertex = 0;
	uint32 m_uVertexCount = 0;
};

/// @contract value Flux_ProceduralDispatch
/// Ownership: Plain value inside a Flux_ProceduralFrame.
/// Thread affinity: IMMUTABLE.
///   Members non-const: the builder fills it field by field in the frame arena before it becomes immutable.
/// Invalidation: None.
/// Units: Records; thread groups of uGEOM_PROCEDURAL_THREADS_PER_GROUP.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Flux_ProceduralDispatch
{
	uint64 m_ulGeneratorKey = 0;
	Zenith_RHIShaderProgramID m_xProgram;
	uint32 m_uFirstRecord = 0;            // into the frame's Flux_ProceduralRecord array
	uint32 m_uRecordCount = 0;
	uint32 m_uGroupCount = 0;             // Σ ceil(vertexCount / 64) over the dispatch's records
};

/// @contract value Flux_ProceduralFrame
/// Ownership: Frame-scoped; built by Flux_ProceduralGeometrySystem in the frame's arena; the upload allocation belongs to
///   the frame's upload ring and retires with it.
/// Thread affinity: OWNER_THREAD (the render-submit thread that built it).
/// Invalidation: Invalid after the frame's graph is compiled.
/// Units: Bytes; vertices.
/// Errors: None; Flux_AddProceduralGeometry cannot fail.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Flux_ProceduralFrame
{
	Zenith_Span<const Flux_ProceduralDispatch> m_axDispatches;         // ascending generator key; empty: no pass is declared
	Flux_UploadAllocation m_xGeneratorInputs;                          // the Flux_ProceduralRecord array, then the frame parameters
	Zenith_Span<const Flux_ProceduralVertexRange> m_axRegenerated;     // ascending, adjacent ranges merged
	uint32 m_uRegion1Offset = 0;                                      // R1, bytes
	uint32 m_uPoolCapacityVertices = 0;                               // C
};

struct Flux_ProceduralPrimitive                 // what the GPU scene needs for one procedural instance
{
	uint32 m_uPoolFirstVertex = 0xFFFFFFFF;    // the instance record's m_uPoolSlot; 0xFFFFFFFF while not drawable
	Zenith_VertexStreamSetCode m_xStreamSet;    // 0x2010 or 0x2030
	bool m_bDrawable = false;                   // admitted, index upload complete, regenerated at least once
};
void Flux_ProceduralApplyRecords(Flux_ProceduralGeometrySystem& xSystem, const Flux_RenderWorld& xWorld);
Flux_ProceduralPrimitive Flux_ProceduralResolve(const Flux_ProceduralGeometrySystem& xSystem,
	Flux_RenderInstanceKey xInstance);
void Flux_ProceduralWritePrimitiveRecord(const Flux_ProceduralGeometrySystem& xSystem, Flux_RenderInstanceKey xInstance,
	Flux_PrimitiveRecord& xRecordOut);
const Flux_ProceduralFrame& Flux_ProceduralBuildFrame(Flux_ProceduralGeometrySystem& xSystem,
	Flux_UploadRing& xRing, Zenith_Arena& xFrameArena);
void Flux_ProceduralOnDeviceRecovered(Flux_ProceduralGeometrySystem& xSystem);
```

<!-- assumption: pending RPROD -->
`Flux_RenderWorldSection` (`Zenith2/Docs/Rendering.md → "The render world hand-off"`) gains `Zenith_Span<const Flux_RenderProceduralInstance> m_axProceduralInstances`, and `Flux_RenderWorldWriter` gains `AddProceduralInstance(const Flux_RenderProceduralInstance&)`, returning `Zenith_Result<void, Flux_RenderWorldError>` like its siblings. A procedural instance's `Flux_RenderInstanceDelta` carries `FLUX_INSTANCE_FLAG_PROCEDURAL` and a null `Flux_MeshID`, and material override 0 is its material. The GPU scene resolves the instance's primitive, stream-set and pool slot through `Flux_ProceduralResolve` and fills the primitive record through `Flux_ProceduralWritePrimitiveRecord`. Flux_Renderer exposes `Flux_ProceduralGeometrySystem& GetProceduralGeometry()`. A module registers its sources through that accessor, during its initialisation and before the first frame. This is cross-area row GM22.
<!-- /assumption -->

## Registration and sizing

### RegisterSource

| Check (in order) | On failure |
|---|---|
| shutdown has not begun | `FLUX_GEOMETRY_ERROR_SHUTTING_DOWN` |
| fewer than `uGEOM_MAX_PROCEDURAL_GENERATORS` generators are registered | `FLUX_GEOMETRY_ERROR_LIMIT` |
| `m_szName` is non-null, 1 … 64 bytes of `[A-Za-z0-9_.]`, and its key collides with no registered generator | `FLUX_GEOMETRY_ERROR_INVALID_DESC` |
| `m_xProgram` names a compute program of the loaded shader container whose parameter struct begins with `Flux_ProceduralDispatchData` ("Flux_AddProceduralGeometry contract") | `FLUX_GEOMETRY_ERROR_INVALID_DESC` |
| `m_uFrameParameterBytes` is in [16, 256] and a multiple of 16; `m_eUV0Format` is an enumerator; `m_uUVReferenceResolution` is in [1, 16384] | `FLUX_GEOMETRY_ERROR_INVALID_DESC` |

On success, the generator is appended to the generator table, which is kept sorted by key, and the key is returned. `UnregisterSource` retires every instance of the generator as the instance-delta removal below does, then removes the table entry. A record naming an unregistered key is rejected.

### Admission of an instance

A record whose instance key has no state, or whose `m_uTopologyVersion` differs from the admitted one, is admitted as follows. The steps run on the `Flux.BuildGraph` node, in render-world order:

<!-- elaboration -->
| Step | Action | On failure |
|---|---|---|
| 1 | the generator key resolves; the topology parameters are at most 256 bytes; the flags hold no unknown bit | rejected |
| 2 | `ComputeSizing(topology)` → sizing; `m_uVertexCount` in [1, 65,535], `m_uIndexCount` in [3, 393,216] and a multiple of 3, bounds and UV range finite with `min ≤ max` | rejected, with the source's error or `INVALID_DESC` |
| 3 | build the position box `Zenith_MakePositionBox(bounds)` and, for UNORM16X2, the UV box `Zenith_MakeUVBox(uvMin, uvMax)` (the FLOAT16X2 box is the identity: scale 1, bias 0); compute the UV precision bound of "Procedural stream-sets" | rejected on a non-finite box; a bound above 1/8 texel only warns |
| 4 | allocate the slot (n = vertex count) from the pool allocator, growing the pool as "Pool layout" states, and an index range (index count rounded up to even) from the unified index pool | `POOL_FULL`; nothing is left allocated |
| 5 | `WriteIndices(topology, staging)` into upload-ring staging of the index count; every index is checked to be below n while it is copied | rejected; the ranges of step 4 are freed through deferred deletion |
| 6 | `RequestUpload` the indices (`FLUX_UPLOAD_CLASS_MESH`, splittable) | the ranges are freed; `UPLOAD_FAILED` is counted |
| 7 | the instance is admitted: state `PENDING` (not drawable) until the index upload completes and a regeneration has run | — |

A topology change of an admitted instance runs steps 2 to 7 for the new version. The old slot and index range retire through deferred deletion with the last-use values of the last frame that drew them. The instance draws the old geometry until the new version becomes drawable, so a topology change never shows an unwritten slot.

### Instance states

<!-- elaboration -->
| State | Event | Next | Action |
|---|---|---|---|
| — | an admissible record for a new key | `PENDING` | admission steps 1 to 7 |
| `PENDING` | the index upload reports `COMPLETE` and the record's slot has been regenerated once | `DRAWABLE` | `Flux_ProceduralResolve` reports `m_bDrawable` |
| `PENDING`, `DRAWABLE` | the instance delta removes the key, or `UnregisterSource` | `RETIRING` | `Cancel()` a queued index upload; free the slot and the index range through deferred deletion |
| `DRAWABLE` | a record with a new topology version | `DRAWABLE` (old) with a `PENDING` replacement | as "Admission of an instance"; from this frame the key owns two slots, and every `REGENERATE` record for the key regenerates the replacement's slot ("Per-frame regeneration", step 1) |
| `DRAWABLE` (old) with a `PENDING` replacement | the replacement becomes `DRAWABLE` (its index upload completed and its slot regenerated once) | `DRAWABLE` (the replacement) | the old slot and index range retire through deferred deletion with the last-use values of the last frame that drew them; the old slot is never regenerated after the replacement was admitted, because its frame parameters belong to the old topology |
| `DRAWABLE` (old) with a `PENDING` replacement | a record with a further topology version | `DRAWABLE` (old) with the new `PENDING` replacement | the superseded replacement's queued index upload is cancelled and its slot and index range are freed through deferred deletion; admission runs for the new version |
| `RETIRING` | retirement completes | — | the state is erased |
| any | device loss | `PENDING` | `Flux_ProceduralOnDeviceRecovered`: the pool is recreated at its current capacity, every instance's indices are uploaded again, and every instance is regenerated from the next full-state extraction's records ([Z2-N-REND-008]; the recovery requests full extraction through `Flux_RenderWorldRing::RequestFullExtraction`) |

A new instance whose record does not set `REGENERATE` is admitted but stays `PENDING` until a record regenerates it, because no frame parameters exist. `Flux.Procedural.PendingUnregenerated` counts such instances each frame.

## Per-frame regeneration

`Flux_ProceduralApplyRecords` runs as step 3 of `Zenith2/Docs/Design/GEOM.md → "Frame-level flow of GEOM work"`, before the GPU scene applies the render-world deltas (step 4), so a new instance's pool slot and stream-set are resolvable when its delta is applied. `Flux_ProceduralBuildFrame` runs in the same step after the records are applied.

<!-- elaboration -->
1. Collect every record of the frame with `REGENERATE` set whose instance is `PENDING` (index upload complete or in flight) or `DRAWABLE`, and whose frame parameters are exactly the generator's byte count. The record's target slot is the key's newest admitted version: for a key with a `PENDING` replacement ("Instance states") it is the replacement's slot, and the record's `m_uFirstVertex`, vertex count, boxes and UV format are the replacement's; the old slot of that key is never a target. A second record for the same key in one frame replaces the first: the last one in render-world order wins, and `Flux.Procedural.Coalesced` counts it.
2. Sort them by (generator key, instance key) with `Zenith_Sort` (stable).
3. Allocate one upload-ring block of `64 × records + Σ frame parameter bytes`, 16-byte aligned. Write the `Flux_ProceduralRecord` array first. After it, write each record's frame parameters at a 16-byte-aligned offset in the sorted order.
4. Per generator, write one `Flux_ProceduralDispatch`. `m_uGroupBase` of each record is the prefix sum, within its dispatch, of `ceil(vertexCount / 64)`, and `m_uGroupCount` is the total.
5. `m_axRegenerated` is the sorted list of the records' slots, with adjacent ranges merged.
6. An instance becomes `DRAWABLE` in the first frame whose regeneration has been recorded and whose index upload is complete. Culling draws it from that frame on, and the graph orders the regeneration before every reader.

`Flux_ProceduralRecord` is the `[FluxShared]` struct of `Zenith2/Shaders/Common/Procedural.slang` (scalar layout, 64 bytes):

| Offset | Size | Type | Field | Meaning |
|---|---|---|---|---|
| 0 | 4 | `uint` | `m_uFirstVertex` | the slot's first vertex |
| 4 | 4 | `uint` | `m_uVertexCount` | n |
| 8 | 4 | `uint` | `m_uGroupBase` | prefix sum of `ceil(n / 64)` within the dispatch |
| 12 | 4 | `uint` | `m_uParameterOffset` | byte offset of the frame parameters in `m_xGeneratorInputs` |
| 16 | 12 | `float3` | `m_xPositionScale` | the instance's position box scale |
| 28 | 4 | `uint` | `m_uFlags` | bit 0: UV0 is FLOAT16X2; bits 1–31 zero |
| 32 | 12 | `float3` | `m_xPositionBias` | the box bias |
| 44 | 4 | `uint` | `m_uReserved` | 0 |
| 48 | 8 | `float2` | `m_xUV0Scale` | the UV box scale (identity for FLOAT16X2) |
| 56 | 8 | `float2` | `m_xUV0Bias` | the UV box bias |

`Procedural.slang` also declares `Flux_ProceduralDispatchData { uint m_uFirstRecord; uint m_uRecordCount; uint m_uRegion1Offset; uint m_uPoolCapacityVertices; }` (16 bytes, `[FluxShared]`) and the two functions every generator uses:

- `Flux_ProceduralRecord Flux_ProceduralFindRecord(ByteAddressBuffer xInputs, Flux_ProceduralDispatchData xData, uint uGroup, uint uGroupThread, out uint uLocalVertex)`: a binary search over `m_uGroupBase` of the dispatch's records. `uLocalVertex = (uGroup − m_uGroupBase) × 64 + uGroupThread`, and a thread with `uLocalVertex ≥ m_uVertexCount` writes nothing.
- `void Flux_ProceduralWriteVertex(RWByteAddressBuffer xPool, Flux_ProceduralDispatchData xData, Flux_ProceduralRecord xRecord, uint uLocalVertex, float3 xPositionOS, float3 xTangent, float3 xNormal, float fSign, float2 xUV0)`. It clamps the position to the box, then quantises it with `Flux_VfMul(Flux_VfSub(p, bias), Flux_VfRcp(scale))` and `Flux_EncodeSnorm16x4`, `w` = 0 (`Zenith2/Docs/Design/GEOM/VertexFormat.md → "Position and UV quantisation"`). It encodes the frame with `Flux_EncodeQTangent(t, n, fSign)` (the bias rule, [Z2-N-REND-893]), and UV0 with the record's box and `Flux_EncodeUnorm16x2`, or with `Flux_F32ToF16Bits` per lane when flag bit 0 is set. It stores the position at byte `8 × (m_uFirstVertex + uLocalVertex)` and the QTangent and UV0 at `m_uRegion1Offset + 12 × (m_uFirstVertex + uLocalVertex)`. Every step uses the kernel, so the written bytes equal the CPU codec's for the same inputs.

A generator program `Zenith2/Shaders/Passes/Procedural/<Generator>.slang` declares `Flux_<Generator>Params` beginning with `Flux_ProceduralDispatchData m_xDispatch`, then the two resource fields of `Zenith2/Docs/Rendering/PerPass/Common.md → "ProceduralGeometry"` (`m_xGeneratorInputs`, `m_xProceduralPool`), and any per-generator permutations. After the two mandated fields a generator's `Params` may add further resource fields and plain-data fields, read through the pass table's parameter struct like any pass's fields. A further resource field names a `Flux_PersistentReadOnly` resource (a height tile, a density map, a splat texture), which stays in the steady `PERSISTENT_READ` state and which the graph does not track per frame ([Z2-N-REND-546]), so the `ProceduralGeometry` pass's declared access is unchanged. It runs 64 threads per group and computes one vertex per thread from the record's frame parameters with explicit arithmetic only: no texture sampling with implicit gradients, and no reads of another instance's slot. The engine ships one generator, `Zenith2/Shaders/Passes/Procedural/Ribbon.slang` with its source `Flux_RibbonSource` (`Zenith2/Engine/ZenithFlux/Internal/Procedural/Flux_RibbonSource.cpp`, registered by `Flux_Renderer` at construction, name `flux.ribbon`):

| Ribbon element | Value |
|---|---|
| topology parameters (32 bytes) | `uint32 m_uPointCount` (2 … 16), `uint32 m_uReserved`, `Zenith_Vector3 m_xBoundsMin`, `Zenith_Vector3 m_xBoundsMax` |
| sizing | vertices `2 × m_uPointCount`, indices `6 × (m_uPointCount − 1)`, bounds as declared, UV0 range [0, 1]² |
| indices | quad i uses vertices 2i, 2i+1, 2i+2, 2i+3 as triangles (2i, 2i+2, 2i+1) and (2i+1, 2i+2, 2i+3) |
| frame parameters (256 bytes) | 16 × `float4`: point xyz in object space, w half-width in metres; points beyond `m_uPointCount` ignored |
| vertex 2i + s (s = 0, 1) | position `p_i + (2s − 1) × w_i × side_i`, with `side_i` the normalised cross of the segment direction and +Y (the +X axis when parallel), tangent the segment direction, normal `cross(side_i, direction)`, sign +1, UV0 `(i / (m_uPointCount − 1), s)` |

## Flux_AddProceduralGeometry contract

```cpp
// Zenith2/Engine/ZenithFlux/Internal/Passes/Flux_PrologueFeatures.h (RPROD's header; GEOM implements this function)
void Flux_AddProceduralGeometry(Flux_RenderGraph& xGraph, const Flux_ProceduralFrame& xFrame, Flux_FrameShared& xShared);
```

| Rule | Specification |
|---|---|
| declaration | exactly one compute pass `ProceduralGeometry` when `xFrame.m_axDispatches` is non-empty, and none otherwise. It is declared at row 5 of `Zenith2/Docs/Rendering/FrameAnatomy.md → "Common prologue"` with `FLUX_PASS_FLAG_ASYNC_COMPUTE` ([Z2-N-REND-791]) |
| resources | reads the upload allocation `m_xGeneratorInputs` (imported as a buffer range, the SRV of the pass table), and writes `xShared.m_xProceduralPool` (the UAV of the pass table). The write is `[FullCoverage]` over every range of `m_axRegenerated` in both regions, because the generator writes every byte of every vertex of a regenerated slot. The rest of the pool keeps its contents, since the pool is persistent-imported and never discarded |
| dispatches | per `Flux_ProceduralDispatch` in order: bind the generator's program, fill `Flux_ProceduralDispatchData` from the dispatch and the frame, and issue `Dispatch(m_uGroupCount, 1, 1)`. Nothing else is recorded |
| ordering | the graph orders the pass before every reader of `m_xProceduralPool` in the frame (depth, colour and shadow passes drawing procedural instances), and after the `GeometryPoolGrow` copy when the pool grew this frame. The pass has no barrier call of its own (Z2-D-056) |
| failure | none: every validation happened in `Flux_ProceduralApplyRecords`, and the function allocates nothing outside the graph arena |
| timing | the graph's timestamps alone, measured against `Z2-B-GEOM-005` Procedural geometry regeneration GPU |

## Drawing procedural instances

<!-- elaboration -->
| Aspect | Rule |
|---|---|
| primitive record | one per instance (L = 1, S = 1), filled by `Flux_ProceduralWritePrimitiveRecord` in the fields of `Zenith2/Docs/Rendering/GpuResources.md → "Primitive record (128 bytes)"`: `m_iVertexOffset` = the slot's first vertex, `m_uFirstIndex` and `m_uIndexCount` = the index range, position and UV0 scale and bias = the boxes, bounds centre, radius and half-extent = the centre, half-diagonal and half-extent of `m_xBounds`, `m_uStreamSetAndLOD` = the procedural code with LOD 0, `m_fLODScreenSize` 0, the position-only index fields invalid (`0xFFFFFFFF`), no meshlets |
| instance record | `FLUX_INSTANCE_FLAG_PROCEDURAL`, `m_uPoolSlot` = the slot's first vertex, the GPU scene's other fields from the instance delta |
| stream-set, buckets | the instance's raster stream-set is its procedural code (0x2010 or 0x2030), which is also its cooked code. Its buckets are keyed by that code (`Zenith2/Docs/Design/RPROD/Culling.md → "Bucket registry"`), and `Flux_MaterialSlotCache` acquires them for the (procedural instance key, material) range as for a mesh (`Zenith2/Docs/Design/GEOM/Materials.md → "Material slots and constants"`) |
| vertex input | fixed-function: every draw of a procedural bucket binds the one pool buffer as vertex stream 0 at byte 0 and as vertex stream 1 at byte `R1`, so one bucket binds one pool set ([Z2-N-REND-887]). The raster pass declares its read through the pass table's `m_xProceduralPool` field |
| programs | `Flux_MeshVertex` programs draw the PROCEDURAL family ("Program stream-set specialisation" of `Zenith2/Docs/Design/GEOM/VertexFormat.md`). Clip position comes from `Flux_TransformObjectToClip` like a mesh's, so depth-equal passes agree ("Position decode invariance") |
| passes | depth prepass, opaque, shadow depth, G-buffer and transparent, as the instance's material domain and flags decide. The visibility-buffer path draws a procedural instance with the instance-level fallback draw (no meshlets) |
| LOD | none: a procedural instance has one LOD |

## Procedural failure handling

| Failure | Behaviour | Counter |
|---|---|---|
| a record names an unregistered generator, carries unknown flags, or has parameters of the wrong size | the change is dropped for the frame; an admitted instance keeps its previous geometry | `Flux.Procedural.Rejected` |
| `ComputeSizing` or `WriteIndices` fails, or an index is out of range | the instance is not admitted (or keeps its previous version); logged once per (generator, run) | `Flux.Procedural.Rejected` |
| the pool or the index pool at its clamp | the instance is not drawn; nothing resident is evicted | `Flux.Procedural.PoolFull` |
| the index upload fails | the instance stays `PENDING`; its ranges are freed | `Flux.Procedural.UploadFailures` |
| the instance table full | RPROD's rule ([Z2-N-REND-545]) | `Flux.GPUScene.TableFull` |
| device loss | "Instance states", row "any" | — |
| shutdown | shutdown step 4 of `Zenith2/Docs/Design/GEOM.md → "Construction and shutdown"` cancels every queued index upload, and `RegisterSource` returns `SHUTTING_DOWN` | — |

## Procedural tests

Runtime tests live in `Zenith2/Engine/ZenithFlux/Tests/Procedural/Flux_Procedural_Tests.cpp` and run on WARP and lavapipe in the `Test` step.

<!-- elaboration -->
| Test | Asserts |
|---|---|
| `Test_Procedural_RegisterValidates` | one failing descriptor per row of "RegisterSource" returns its error (a 65th generator is `LIMIT`; a duplicate name, a 257-byte parameter count, a 24-byte parameter count and an unknown UV format are each `INVALID_DESC`); a valid descriptor returns `Zenith_Hash64` of its name |
| `Test_Procedural_AdmissionLimits` | vertex counts 1 and 65,535 are admitted and 0 and 65,536 rejected; index counts 3 and 393,216 are admitted and 4, 0 and 393,219 rejected; a `WriteIndices` fixture writing index n is rejected and leaves no range allocated |
| `Test_Procedural_StreamSetPerUVFormat` | a UNORM16X2 generator's instances draw with code `0x2010`, a FLOAT16X2 generator's with `0x2030`; both codes are in `axFLUX_STREAMSET_LAYOUTS`; a UNORM16X2 generator with UV range [0, 64] at 4,096 texels raises `Flux.Procedural.UVPrecisionWarnings` once and still admits |
| `Test_Procedural_WriteVertexMatchesCpuCodec` | for 10^5 fixed random vertices, the pool bytes written by `Flux_ProceduralWriteVertex` equal `Zenith_QuantisePositions`, `Zenith_EncodeQTangentFromFrame` and `Zenith_QuantiseUVs` (or `Zenith_F32ToF16`) of the same inputs, bit for bit, at the addresses of "Pool layout" |
| `Test_Procedural_RegeneratesOnlyFlaggedSlots` | with 100 admitted instances and 7 records flagged `REGENERATE`, the frame has one dispatch per generator involved, 7 records sorted by (generator key, instance key), and `m_axRegenerated` holds exactly their merged slots; the other 93 slots are bit-identical before and after the frame |
| `Test_Procedural_FrameDeterministic` | the `Flux_ProceduralFrame` contents and the generator-input bytes are identical across two runs of a fixed 200-frame record script, and do not depend on the order in which extractor partitions wrote the records ([Z2-N-REND-008] merge order) |
| `Test_Procedural_DrawableAfterRegeneration` | a new instance records zero draws until the frame in which its index upload has completed and its first regeneration is recorded, and draws from that frame |
| `Test_Procedural_TopologyChangeKeepsOldUntilReady` | a topology-version change keeps drawing the old slot until the new one is drawable, then retires the old slot and index range; a `REGENERATE` record for the key while the replacement is `PENDING` writes the replacement's slot and leaves the old slot's bytes bit-identical; a second topology change while a replacement is `PENDING` frees the superseded replacement and keeps drawing the old slot |
| `Test_Procedural_Lifecycle1000Cycles` | 1,000 randomised admissions, topology changes and removals (fixed seed, printed) leave the pool's free vertices, the index pool's free indices, the primitive table's free count and the test heap's live bytes at their start values |
| `Test_Procedural_PoolGrowPreservesContents` | growth relocates region 1 and preserves every slot's bytes in both regions bit for bit; every instance redraws identically |
| `Test_Procedural_PoolFullNotDrawn` | an admission beyond the clamp fails with `POOL_FULL`, counts `Flux.Procedural.PoolFull`, and evicts nothing |
| `Test_Procedural_DrawsInEveryPass` | a ribbon instance regenerated this frame is drawn in the depth prepass, the opaque pass and a shadow view of the same frame; zero `EQUAL`-test failures between prepass and opaque; synchronization validation clean |
| `Test_Procedural_PassDeclaredOnlyWhenDirty` | a frame with no regeneration declares no `ProceduralGeometry` pass; a frame with one declares exactly one, flagged async compute, whose UAV full-coverage ranges equal `m_axRegenerated` in both regions |
| `Test_Procedural_DeviceLossRegenerates` | after a simulated device loss, every instance is `PENDING`, then `DRAWABLE` after the full-state records regenerate it, and renders bit-identically to before the loss |
| `Test_Procedural_RibbonAnalytic` | the ribbon source's sizing and indices for 2, 3 and 16 points equal the closed forms of the ribbon table; a straight three-point ribbon's decoded positions equal the analytic offsets within one SNORM16 quantum of its box |
| `Test_GPUScene_ProceduralRegeneratedMidFrame` | the Rendering test of `Zenith2/Docs/Rendering/GpuResources.md → "GPU scene tests"`, which RHI implements in `Zenith2/Engine/ZenithFlux/Tests/GPUScene/`. GEOM supplies its generator (the ribbon source) and asserts, as the test requires, that geometry regenerated between two consumers is seen by the second consumer only |
