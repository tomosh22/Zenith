# RPROD design: the render graph front end

This file is part of the RPROD design family; the entry document is `Zenith2/Docs/Design/RPROD.md`. It specifies the front half of the Flux render graph that RPROD owns under the division of `Zenith2/Docs/Design/RHI.md → "Division of Flux code with RPROD"`: the public graph API and its contract blocks, setup and the field-table walk, compile steps 1, 2, 4 and 5, pass merging, the compile-time rule checks of the GPU performance rules, the first-write rule and poisoning, timestamp index insertion, the pass contexts, the record-time checks and the front-end keys of the frame dump. Steps 3, 6 and 7, recording orchestration, barrier translation, submission, the imported-state table and the dump writer are RHI's (`Zenith2/Docs/Design/RHI/RenderGraphBackend.md → "Ownership of the compile steps"`); this file calls them by the entry points named in "Compile driver".

## Scope of this file

The specification is `Zenith2/Docs/Rendering/RenderGraph.md` (every heading) and `Zenith2/Docs/Rendering.md → "Render graph"`, "Barrier synthesis", "Transient aliasing" and "GPU performance rules and lints". Where this file repeats a declaration of the specification it adds only the contract block and the members the specification leaves unnamed; each addition is marked `<!-- elaboration -->`. Every file named here lives in `Zenith2/Engine/ZenithFlux/` unless another path is given.

## Graph public headers

| Header (`Zenith2/Engine/ZenithFlux/`) | Declares | Owner |
|---|---|---|
| `Flux_RenderGraph.h` | `Flux_RGTexture`, `Flux_RGBuffer`, `Flux_RGPass`, `Flux_TextureDimension`, `Flux_ClearValue`, `Flux_Rect`, `Flux_RGTextureRange`, `Flux_RGBufferRange`, `Flux_RGTextureDesc`, `Flux_RGBufferDesc`, `Flux_Justified32BitUse`, `Flux_DispatchSize`, `Flux_DispatchFor2D`, `Flux_PassFlags`, `Flux_SideEffect`, `Flux_RenderGraph` | RPROD |
| `Flux_RasterTargets.h` | `Flux_LoadOp`, `Flux_StoreOp`, `Flux_DepthAccess`, `Flux_VertexStreamRef`, `Flux_IndexBufferRef`, `Flux_IndirectArgsRef`, `Flux_RasterTargets` | RPROD |
| `Flux_PassContext.h` | `Flux_RasterContext`, `Flux_ComputeContext` | RPROD |
| `Flux_GraphSettings.h` | `Flux_GraphSettings` | RPROD |
| `Flux_GraphCaches.h` | `Flux_GraphCaches`, `Flux_GraphCachesDesc`, `Flux_TransientMemoryStats` | RPROD (its imported-state member is RHI's type); Rendering's graph header list carries it since X97 |
| `Flux_GraphErrors.h` | `Flux_RGError`, `Flux_RGInfo`, `Flux_RecordCheck`, `Flux_GraphDiagnostic`, `Flux_GraphInfoRecord`, `Flux_GraphErrorList`, `Zenith_ToString` overloads | RPROD; Rendering's graph header list carries it since X97 |
| `Flux_PSO.h` | `Flux_RasterState`, `Flux_AttachmentFormats`, `Flux_PSOKey` and the fixed-table indices the raster context takes | RPROD (`Zenith2/Docs/Design/RPROD/PSO.md → "Flux_PSO header"`) |

Every header includes only ZenithCore, ZenithRHI, ZenithGraph and ZenithFlux public headers, and compiles alone in the HeaderSelfContained gate step.

## Flux_RenderGraph header

The member list of `Flux_RenderGraph`, the handle structs, the two descriptor structs and `Flux_PassFlags` are exactly `Zenith2/Docs/Rendering/RenderGraph.md → "Graph public API"`. This header adds the supporting value types, the constants and the contract blocks below.

<!-- elaboration -->
```cpp
// Zenith2/Engine/ZenithFlux/Flux_RenderGraph.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_Function.h"
#include "ZenithRHI/Zenith_RHITypes.h"
#include "ZenithRHI/Zenith_DeviceHandle.h"
#include "ZenithRHI/Zenith_RHIShaderInterface.h"
#include "ZenithFlux/Flux_RasterTargets.h"
#include "ZenithFlux/Flux_PassContext.h"
#include "ZenithFlux/Flux_GraphSettings.h"
#include "ZenithFlux/Flux_GraphErrors.h"
#include "ZenithFlux/Flux_DepthMode.h"              // Flux_DepthModeReason
#include "ZenithFlux/Flux_PersistentResources.h"   // RHI's Flux_PersistentReadOnlyTexture/Buffer
#include "ZenithFlux/Flux_Readback.h"              // RHI's Flux_ReadbackHandle, Flux_ReadbackError

class Zenith_Arena;
class Flux_GraphCaches;
class Flux_CompiledGraph;                          // Internal/Graph/Flux_CompiledGraph.h

/// @contract functions Flux_RenderGraphConstants
/// Ownership: Compile-time constants.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: Counts; bytes for uFLUX_RG_BUFFER_GRANULE_BYTES; frames for the two retirement ages; cost units for the partition target.
/// Errors: Exceeding a count limit records FLUX_RG_ERROR_CAPACITY at the declaration that exceeds it.
/// Version: since Z2 Stage 2. A limit may rise in a subsequent fix version; it never falls within a release line.
/// @endcontract
inline constexpr uint32 uFLUX_RG_INVALID = 0xFFFFFFFFu;
inline constexpr uint32 uFLUX_RG_MAX_PASSES = 16384;           // per frame graph
inline constexpr uint32 uFLUX_RG_MAX_RESOURCES = 65536;        // textures and buffers together
inline constexpr uint32 uFLUX_RG_MAX_VIEWS = 262144;           // Flux_RGViewRecord entries per frame graph: 262,144 × 32 bytes = 8 MiB of the
                                                                // graph arena, the same bound as the access table, because every view a pass
                                                                // uses is named by one access record
inline constexpr uint32 uFLUX_RG_MAX_VIEWS_PER_PASS = 65536;   // distinct views one pass names: the 16-bit view index of Zenith_RHISRV word 1
                                                                // into the pass's view table (Rendering.md "DescriptorHandle usage")
inline constexpr uint32 uFLUX_RG_MAX_VERSIONS = 65536;         // per resource (the 16-bit version of word 1)
inline constexpr uint32 uFLUX_RG_MAX_ACCESSES = 262144;        // per frame graph
inline constexpr uint32 uFLUX_RG_BUFFER_GRANULE_BYTES = 256;   // buffer subresource size handed to ZenithGraph
inline constexpr uint32 uFLUX_RG_CACHE_RETIRE_FRAMES = 8;      // [Z2-N-REND-287]; 2 under memory-pressure action 2
inline constexpr uint32 uFLUX_RG_HEAP_SHRINK_FRAMES = 300;     // [Z2-N-REND-437]; 0 under action 3
inline constexpr uint32 uFLUX_RG_PARTITION_TARGET_COST = 4096; // [Z2-N-REND-305]
inline constexpr uint32 uFLUX_RG_SPLIT_BARRIER_MIN_GAP = 2;    // [Z2-N-REND-393]

/// @contract enum Flux_TextureDimension
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2. Values map one to one onto Zenith_RHITextureDimension (static_assert in Flux_RenderGraph.cpp).
/// @endcontract
enum Flux_TextureDimension : uint8
{
	FLUX_TEXTURE_DIMENSION_2D = 1,
	FLUX_TEXTURE_DIMENSION_2D_ARRAY,
	FLUX_TEXTURE_DIMENSION_3D,
	FLUX_TEXTURE_DIMENSION_CUBE,
	FLUX_TEXTURE_DIMENSION_CUBE_ARRAY,
};

/// @contract value Flux_ClearValue
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: it is copy-assigned as a field of the descriptors and records that hold it.
/// Invalidation: None.
/// Units: Linear colour for float formats, normalised for UNORM, raw for integer formats; depth in [0, 1].
/// Errors: None. A depth clear value is produced by Flux_DepthModeOps::GetClearValue (Zenith2/Docs/Design/RPROD/Views.md
///   "Flux_DepthMode header") and never written by hand ([Z2-N-REND-595]).
/// Version: since Z2 Stage 2.
/// @endcontract
struct Flux_ClearValue
{
	float m_afColour[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	uint32 m_auColour[4] = { 0, 0, 0, 0 };          // integer formats (the visibility ID clears to 0xFFFFFFFF)
	float m_fDepth = 0.0f;
	uint8 m_uStencil = 0;
};

/// @contract value Flux_Rect
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: it is copy-assigned as a field of the descriptors and records that hold it.
/// Invalidation: None.
/// Units: Texels of the attachment's mip 0 (render areas) or output pixels (scissors).
/// Errors: None; a render area outside the attachment extent is FLUX_RG_ERROR_EXTENT_MISMATCH at compile.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Flux_Rect
{
	uint32 m_uX = 0;
	uint32 m_uY = 0;
	uint32 m_uWidth = 0;    // 0: the full attachment extent
	uint32 m_uHeight = 0;
};

/// @contract value Flux_RGTextureRange
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: it is copy-assigned as a field of the descriptors and records that hold it.
/// Invalidation: None.
/// Units: Mips and array layers; uFLUX_RG_INVALID counts mean "to the last".
/// Errors: A range outside the resource records FLUX_RG_ERROR_SUBRANGE_OUT_OF_BOUNDS.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Flux_RGTextureRange
{
	uint32 m_uFirstMip = 0;
	uint32 m_uMipCount = uFLUX_RG_INVALID;
	uint32 m_uFirstLayer = 0;
	uint32 m_uLayerCount = uFLUX_RG_INVALID;
	Zenith_RHIFormat m_eViewFormat = ZENITH_RHI_FORMAT_UNDEFINED;   // UNDEFINED: the texture's format
	static constexpr Flux_RGTextureRange All() { return {}; }
	static constexpr Flux_RGTextureRange Mip(uint32 uMip) { return { uMip, 1, 0, uFLUX_RG_INVALID, ZENITH_RHI_FORMAT_UNDEFINED }; }
};

/// @contract value Flux_RGBufferRange
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: it is copy-assigned as a field of the descriptors and records that hold it.
/// Invalidation: None.
/// Units: Bytes. m_ulSize 0 means "to the end of the buffer". Offsets of structured views are multiples of the stride;
///   byte-address views are multiples of 16.
/// Errors: A range outside the buffer records FLUX_RG_ERROR_SUBRANGE_OUT_OF_BOUNDS.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Flux_RGBufferRange
{
	uint64 m_ulOffset = 0;
	uint64 m_ulSize = 0;
	static constexpr Flux_RGBufferRange All() { return {}; }
};

/// @contract enum Flux_Justified32BitUse
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: A 32-bit-per-channel render target whose use is FLUX_32BIT_USE_NONE, or whose (format, use) pair is absent
///   from Internal/Graph/Justified32BitFormats.json, is FLUX_RG_ERROR_32BIT_TARGET_UNJUSTIFIED ([Z2-N-REND-918]).
/// Version: since Z2 Stage 2. A new enumerator lands with its Justified32BitFormats.json entry and decision ID.
/// @endcontract
enum Flux_Justified32BitUse : uint8
{
	FLUX_32BIT_USE_NONE = 0,
	FLUX_32BIT_USE_VISIBILITY_ID = 1,        // R32_UINT, the visibility-buffer ID target
	FLUX_32BIT_USE_DEPTH_VIEW = 2,           // D32_FLOAT_S8X24_UINT of a view with a precision or capability reason
};
```

[Z2-N-REND-289] makes the public graph API "exactly the declarations above" of `Zenith2/Docs/Rendering/RenderGraph.md → "Graph public API"`. Rendering's "Graph public API" admits every addition this design makes (X97 applied): the two `Flux_RGTextureDesc` members below, the five `Flux_RenderGraph` members of "Members added to the class by this design", and the headers `Flux_GraphCaches.h` and `Flux_GraphErrors.h`, which carry the inputs the D32 and 32-bit checks ([Z2-N-REND-596], [Z2-N-REND-918]) and the per-pass timing names ([Z2-N-REND-312]) need.

`Flux_RGTextureDesc` gains, after the members Rendering lists, two members, so that the two graph compile checks of "Compile-time rule checks" can be evaluated from the description alone:

```cpp
	Flux_DepthModeReason m_eDepthReason = FLUX_DEPTH_MODE_REASON_DEFAULT;   // depth targets: copied from the owning view's Flux_DepthMode
	Flux_Justified32BitUse m_eJustified32BitUse = FLUX_32BIT_USE_NONE;
```

Contract blocks of the specification's declarations:

```cpp
/// @contract value Flux_RGTexture
/// Ownership: Plain value naming one version of one texture of one frame's graph; the graph owns the resource.
/// Thread affinity: IMMUTABLE.
///   Members non-const: it is copy-assigned into descriptors, records and tables.
/// Invalidation: Frame-scoped ([Z2-N-CONTRACT-095]): invalid once the graph that issued it is destroyed; never valid in
///   another frame's graph (the graph's frame serial is checked in every build).
/// Units: m_uVersion counts the writes declared on the resource before this version; version 0 of a transient is
///   "nothing written", version 0 of an import is the previous frame's content.
/// Errors: A handle of another graph or an out-of-range index records FLUX_RG_ERROR_INVALID_HANDLE; nothing asserts.
/// Version: since Z2 Stage 2. Never serialized.
/// @endcontract
// Flux_RGBuffer and Flux_RGPass carry the same block with "buffer" and "pass" substituted.

/// @contract type Flux_RenderGraph
/// Ownership: Constructed by Flux_Renderer (and by graph tests) with Zenith_Construct in the frame's graph arena and
///   destroyed with Zenith_Destroy before that arena is reset; every allocation it makes comes from that arena
///   ([Z2-N-REND-285]). It owns its declaration tables, the ZenithGraph it builds at compile and the compiled graph;
///   physical resources belong to the Flux_GraphCaches passed in, which outlives every graph.
/// Thread affinity: OWNER_THREAD. Construction, setup (every Create/Import/SRV/UAV/Add* call) and Compile run on the
///   thread of the Flux.BuildGraph node; after Compile returns, the compiled graph is IMMUTABLE and the record jobs of
///   Flux.Record read it concurrently.
/// Invalidation: Every handle, view value and pass index dies with the graph; the compiled graph dies with the graph's
///   arena. AllocParams memory lives until the arena is reset.
/// Units: As the descriptor types; cost hints in the cost units of [Z2-N-REND-303].
/// Errors: Declaration calls never fail visibly; each error is recorded and Compile returns
///   Zenith_Result<const Flux_CompiledGraph*, Flux_GraphErrorList> whose error names the first code and the count,
///   the full list being GetDiagnostics() ([Z2-N-REND-291]). AddReadback returns Flux_ReadbackError. A capture that is
///   not trivially destructible is a compile error (static_assert, [Z2-N-REND-288]).
/// Reentrancy: NOT_APPLICABLE. The type invokes no callback during setup or compile; raster lambdas run at record, on
///   record jobs, each receiving its own Flux_RasterContext.
/// Version: since Z2 Stage 2. Source API; the frame dump format it feeds is zenith2.flux-graph-dump version 1.
/// Example: Zenith2/Engine/ZenithFlux/Tests/Examples/Flux_RenderGraph_Example.cpp
/// @endcontract
```

Members added to the class by this design (admitted by X97, as above):

```cpp
	// Diagnostics of the last Compile (graph arena; valid until the graph is destroyed)
	Zenith_Span<const Flux_GraphDiagnostic> GetDiagnostics() const;
	Zenith_Span<const Flux_GraphInfoRecord> GetInfoRecords() const;   // culled, folded, merged ([Z2-N-REND-915])
	// Frame identity, set by Flux_GraphCaches::BeginFrame before construction
	uint64 GetFrameIndex() const;
	// A view-suffix scope: every pass declared inside it gets "[<suffix>]" appended to its name ([Z2-N-REND-312])
	void PushViewSuffix(Zenith_StringView strSuffix);
	void PopViewSuffix();
```

`AllocParams<TParams>()` zero-initialises the struct, then writes the unset marker `{ uFLUX_RG_INVALID, 0 }` into every resource field listed in `Zenith_RHIParamFieldTable<TParams>::xFIELDS`, so that a field the feature function never sets is detectable at the field-table walk ([Z2-N-REND-066]).

## Flux_RasterTargets header

The declarations are exactly `Zenith2/Docs/Rendering/RenderGraph.md → "Pass declaration"` (heading "Flux_RasterTargets"). Contract blocks:

```cpp
/// @contract value Flux_VertexStreamRef
/// Ownership: Plain value; names a graph buffer or a persistent read-only buffer, never both.
/// Thread affinity: IMMUTABLE.
///   Members non-const: it is copy-assigned as a field of the descriptors and records that hold it.
/// Invalidation: A graph-buffer reference is frame-scoped; a persistent reference is valid while the persistent
///   resource is live (the asset system holds it for at least the frame).
/// Units: Bytes for m_ulOffset; m_uStream is the Slang [Stream(n)] index.
/// Errors: Both or neither set records FLUX_RG_ERROR_UNDECLARED_BUFFER at the declaring AddRasterPass.
/// Version: since Z2 Stage 2.
/// @endcontract
// Flux_IndexBufferRef and Flux_IndirectArgsRef carry the same block; Flux_IndirectArgsRef has no persistent form.

/// @contract type Flux_RasterTargets
/// Ownership: A CPU-only value built on the stack of a feature function and copied into the graph arena by the
///   Add*Pass call that takes it; it never reaches a shader.
/// Thread affinity: OWNER_THREAD (the build node).
/// Invalidation: The Flux_RGTexture references passed to Colour and Depth are advanced to their successor version in
///   place when the attachment is written (CLEAR, DONT_CARE or LOAD with store STORE, and depth WRITE); the object
///   itself holds copies, so it may die after the Add*Pass call.
/// Units: Slots 0–7; mips and layers; render areas in texels.
/// Errors: More than 8 colour slots, a slot declared twice, Resolve(), a second depth, or more than 16 vertex
///   streams record the graph error named in "Compile-time rule checks" at the declaring Add*Pass call.
/// Version: since Z2 Stage 2.
/// @endcontract
```

<!-- elaboration -->
| Builder call | Graph accesses it declares |
|---|---|
| `Colour(slot, tex, LOAD, STORE)` | `COLOUR_ATTACHMENT` read-write: reads version v, writes v + 1 (the handle advances) |
| `Colour(slot, tex, CLEAR, STORE)` | `COLOUR_ATTACHMENT` write, full coverage when the render area is the whole attachment extent |
| `Colour(slot, tex, DONT_CARE, STORE)` | `COLOUR_ATTACHMENT` write, full coverage only when `FullCoverage()` was called (fullscreen passes imply it) |
| `Colour(slot, tex, *, DONT_CARE)` | as above; the written version's contents are undefined after the pass, so a read of it is `FLUX_RG_ERROR_READ_BEFORE_WRITE` |
| `Depth(tex, WRITE, …)` | `DEPTH_WRITE` with the load-op rules of the colour rows |
| `Depth(tex, READ_ONLY, LOAD, …)` | `DEPTH_READ_ONLY` read; the handle does not advance; a same-pass SRV of the same subresource merges into `DEPTH_READ_ONLY` with the sampling stage ([Z2-N-REND-381]) |
| `VertexStreams`, `IndexBuffer` | `VERTEX_BUFFER`, `INDEX_BUFFER` reads of graph buffers; persistent references declare nothing |
| `IndirectArgs(args, count)`, `IndirectArgs(args)` | `INDIRECT_ARGS` reads of both buffers |
| `Programs` | no access: the declared shader set ([Z2-N-REND-920]) |

## Flux_PassContext header

The declarations are exactly `Zenith2/Docs/Rendering/RenderGraph.md → "Pass contexts"`. Contract blocks:

```cpp
/// @contract type Flux_RasterContext
/// Ownership: Constructed by the graph recorder on the stack of one record job for one pass (or one pass inside a
///   merged scope); the lambda borrows it for the duration of its call.
/// Thread affinity: OWNER_THREAD (the record job that owns the pass's partition).
/// Invalidation: Invalid after the lambda returns; BeginDrawParams memory lives in the frame's upload ring until the
///   frame slot is reused.
/// Units: Vertex, index, instance and command counts; stencil reference 0–255.
/// Errors: BindPipeline returns false for a PSO that is not READY and applies the missing-PSO fallback
///   (Zenith2/Docs/Design/RPROD/PSO.md "Missing-PSO fallback realisation"); every other misuse is a record-time check of
///   "Record-time checks" in development builds and is ignored (the call is dropped) in Shipping.
/// Reentrancy: NOT_APPLICABLE. The context invokes no callback.
/// Version: since Z2 Stage 2. The operation set is closed ([Z2-N-REND-300]).
/// @endcontract

/// @contract type Flux_ComputeContext
/// Ownership: As Flux_RasterContext, for the graph's own compute bodies and graph-internal passes.
/// Thread affinity: OWNER_THREAD (the record job).
/// Invalidation: As Flux_RasterContext.
/// Units: Thread-group counts; command indices of the pass's declared indirect arguments.
/// Errors: As Flux_RasterContext; a compute PSO miss skips the pass and fills its full-coverage outputs (PSO.md).
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 2.
/// @endcontract
```

## Flux_GraphSettings header

The declaration is exactly `Zenith2/Docs/Rendering/RenderGraph.md → "Graph public API"` (`Flux_GraphSettings`).

```cpp
/// @contract value Flux_GraphSettings
/// Ownership: Plain value owned by Flux_Renderer (Zenith2/Docs/Design/RPROD/Renderer.md "Flux_Renderer header"), copied
///   into each frame's graph at construction.
/// Thread affinity: IMMUTABLE once copied into a graph; Flux_Renderer's copy is written on the main thread in the frame
///   prologue alone (console, debug UI and configuration changes, [Z2-N-REND-290]).
///   Members non-const: Flux_Renderer's copy is written field by field in the frame prologue.
/// Invalidation: None.
/// Units: m_uDumpFrames in frames.
/// Errors: None. m_bPoisonTransients and m_uDumpFrames are ignored in Shipping builds.
/// Version: since Z2 Stage 2.
/// @endcontract
```

| Member | Configuration key (registered by `Zenith_RegisterFluxConfigKeys`) | Console | Default | Shipping |
|---|---|---|---|---|
| `m_bAliasTransients` | `r.graph.aliasTransients` (BOOL) | `r.graph.aliasTransients 0\|1` | true | honoured |
| `m_bPoisonTransients` | `r.graph.poisonTransients` (BOOL) | `r.graph.poisonTransients 0\|1` | false (true in every graph and render test) | forced false |
| `m_bAsyncCompute` | `r.graph.asyncCompute` (BOOL) | as named | true | honoured |
| `m_bSplitBarriers` | `r.graph.splitBarriers` (BOOL) | as named | true | honoured |
| `m_bMergePasses` | `r.graph.mergePasses` (BOOL) | as named | true | honoured |
| `m_bParallelRecord` | `r.graph.parallelRecord` (BOOL) | as named | true | honoured |
| `m_bTimestamps` | `r.graph.timestamps` (BOOL) | as named | true in development | follows the on-screen profiler toggle ([Z2-N-REND-313]) |
| `m_uDumpFrames` | none (a command, not a setting) | `r.graph.dump <N>` | 0 | forced 0 ([Z2-N-REND-315]) |

## Flux_GraphCaches header

<!-- elaboration -->
```cpp
// Zenith2/Engine/ZenithFlux/Flux_GraphCaches.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithRHI/Zenith_RHIDevice.h"
#include "ZenithFlux/Flux_GraphErrors.h"

class Zenith_Heap;
class Flux_ImportedStateTable;       // RHI's, Internal/Graph/Flux_ImportedStateTable.h

/// @contract value Flux_GraphCachesDesc
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: callers default-construct it and fill its fields.
/// Invalidation: None.
/// Units: Frames; bytes.
/// Errors: A null device creates a deviceless cache for compile steps 1 to 3 alone (graph conformance tests,
///   Zenith2/Docs/Design/RHI/RenderGraphBackend.md "Flux_RenderGraphTestAccess").
/// Version: since Z2 Stage 2.
/// @endcontract
struct Flux_GraphCachesDesc
{
	Zenith_RHIDevice* m_pxDevice = nullptr;
	Zenith_Heap* m_pxHeap = nullptr;
	uint32 m_uRetireFrames = uFLUX_RG_CACHE_RETIRE_FRAMES;
	uint32 m_uHeapShrinkFrames = uFLUX_RG_HEAP_SHRINK_FRAMES;
};

/// @contract value Flux_TransientMemoryStats
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: the producer default-constructs it and fills its fields before returning it.
/// Invalidation: None; a snapshot of the last compiled frame.
/// Units: Bytes; percent in [0, 100].
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Flux_TransientMemoryStats
{
	uint64 m_ulDedicatedBytes = 0;      // the summed transient sizes (aliasing off equivalent)
	uint64 m_ulAliasedBytes = 0;        // the summed group heap sizes
	uint64 m_ulSavingsBytes = 0;        // dedicated minus aliased
	float m_fSavingsPercent = 0.0f;
	uint64 m_ulPeakAliasedBytes300 = 0; // peak over the last 300 frames ([Z2-N-REND-445])
	uint32 m_uGroupCount = 0;
	bool m_bAliasingOn = true;
};

/// @contract type Flux_GraphCaches
/// Ownership: Created by Flux_Renderer through Create as a Zenith_Owned, one per device, destroyed after the last frame
///   has retired and before the device; owns the transient group heaps, the placed-resource cache, the
///   dedicated-resource cache, the imported-state table (RHI's type, [Z2-N-REND-294]) and the 300-frame memory history.
/// Thread affinity: OWNER_THREAD. BeginFrame and every graph compile run on the Flux.BuildGraph node; the imported-state
///   table's Commit runs at Flux.Submit on the render-submit thread, which the render subgraph orders after the build
///   node, so no two users overlap (the rule of RenderGraphBackend.md "Imported-state table").
/// Invalidation: Cached physical resources are released through the retirement queue with the last-use values of the
///   last frame that used them; handles held by a compiled graph stay valid until that graph's frame retires.
/// Units: Frames; bytes.
/// Errors: Create returns FLUX_RG_ERROR_CAPACITY for an allocation failure of its own tables; a heap or placed-resource
///   creation failure during a compile is reported by that compile as FLUX_RG_ERROR_CAPACITY with the resource named,
///   after the memory-pressure response of Zenith2/Docs/Design/RHI/Memory.md "Device memory exhaustion" ran.
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 2.
/// @endcontract
class Flux_GraphCaches
{
public:
	static Zenith_Result<Zenith_Owned<Flux_GraphCaches>, Flux_RGError> Create(const Flux_GraphCachesDesc& xDesc);
	~Flux_GraphCaches();
	void BeginFrame(uint64 ulFrameIndex);                           // ages entries, applies pressure actions 2, 3, 7
	void SetPressureOverrides(uint32 uRetireFrames, uint32 uHeapShrinkFrames, bool bReleaseUnusedNow);
	Flux_ImportedStateTable& GetImportedStateTable();
	const Flux_TransientMemoryStats& GetTransientMemoryStats() const;
	Zenith_RHIDevice* GetDevice() const;               // null when deviceless
	uint32 GetCachedResourceCount() const;
private:
	Flux_GraphCaches(const Flux_GraphCachesDesc& xDesc);
};
```

## Flux_GraphErrors header

<!-- elaboration -->
```cpp
// Zenith2/Engine/ZenithFlux/Flux_GraphErrors.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_StringView.h"

/// @contract enum Flux_RGError
/// Ownership: Plain enumeration; the graph error codes of Zenith2/Docs/Rendering/RenderGraph.md "Graph errors", in
///   table order, plus FLUX_RG_ERROR_SWAPCHAIN_QUEUE (Zenith2/Docs/Design/RHI/RenderGraphBackend.md
///   "Queue-family ownership transfers"), which RHI's compile step 6 raises (row 6d of its "Step 6 queue assignment").
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: Each enumerator is raised under exactly the condition of the Rendering table ([Z2-N-REND-446]); the core
///   mapping is the table "Core error mapping" of that file.
/// Version: since Z2 Stage 2. Numbered from 1 ([Z2-N-CONTRACT-465]); an enumerator keeps its meaning forever.
/// @endcontract
enum Flux_RGError : uint8
{
	FLUX_RG_ERROR_INVALID_HANDLE = 1,
	FLUX_RG_ERROR_SUBRANGE_OUT_OF_BOUNDS = 2,
	FLUX_RG_ERROR_SELF_ORDERING = 3,
	FLUX_RG_ERROR_CAPACITY = 4,
	FLUX_RG_ERROR_FIELD_UNSET = 5,
	FLUX_RG_ERROR_VIEW_TYPE_MISMATCH = 6,
	FLUX_RG_ERROR_STALE_WRITE = 7,
	FLUX_RG_ERROR_CYCLE = 8,
	FLUX_RG_ERROR_READ_BEFORE_WRITE = 9,
	FLUX_RG_ERROR_PARTIAL_FIRST_WRITE = 10,
	FLUX_RG_ERROR_PERSISTENT_WRITE = 11,
	FLUX_RG_ERROR_UNDECLARED_BUFFER = 12,
	FLUX_RG_ERROR_UNWRITTEN_ATTACHMENT = 13,
	FLUX_RG_ERROR_DEPTH_WRITABLE_UNUSED = 14,
	FLUX_RG_ERROR_D32_UNJUSTIFIED = 15,
	FLUX_RG_ERROR_32BIT_TARGET_UNJUSTIFIED = 16,
	FLUX_RG_ERROR_MSAA_UNSUPPORTED = 17,
	FLUX_RG_ERROR_TILE_LOCAL_UNSUPPORTED = 18,
	FLUX_RG_ERROR_EXTENT_MISMATCH = 19,
	FLUX_RG_ERROR_FORMAT_FEATURE_MISSING = 20,
	FLUX_RG_ERROR_TABLE_WRITE = 21,
	FLUX_RG_ERROR_SWAPCHAIN_QUEUE = 22,
};

/// @contract enum Flux_RGInfo
/// Ownership: Plain enumeration of informational compile records (never errors).
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2. The names are the check names of Zenith2/Docs/Rendering.md "GPU performance rules and lints".
/// @endcontract
enum Flux_RGInfo : uint8
{
	FLUX_RG_INFO_PASS_CULLED = 1,
	FLUX_RG_INFO_CLEAR_FOLDED = 2,
	FLUX_RG_INFO_PASSES_MERGED = 3,
	FLUX_RG_INFO_SPLIT_DEMOTED = 4,          // a split candidate emitted unsplit by the back end
};

/// @contract enum Flux_RecordCheck
/// Ownership: Plain enumeration of the development-build record-time checks.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: A failed check drops the offending call, logs once per (pass, program) and counts Flux.Record.CheckFailures;
///   a test's validation sink fails the test on any failure (Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md
///   "Record-time checks").
/// Version: since Z2 Stage 2.
/// @endcontract
enum Flux_RecordCheck : uint8
{
	FLUX_RECORD_CHECK_UNWRITTEN_ATTACHMENT = 1,   // a program outside the declared set, or one leaving a bound attachment unwritten
	FLUX_RECORD_CHECK_UNDECLARED_RESOURCE = 2,    // a buffer index, indirect form or params resource field the pass did not declare
	FLUX_RECORD_CHECK_BLEND_UNSUPPORTED = 3,      // a PSO blending an attachment whose format lacks the B feature
	FLUX_RECORD_CHECK_DRAW_PARAMS_EXCEEDED = 4,   // more BeginDrawParams calls than ExpectedDraws declared ([Z2-N-REND-496])
};

/// @contract value Flux_GraphDiagnostic
/// Ownership: Plain value stored in the graph arena; m_strText points into the graph arena.
/// Thread affinity: IMMUTABLE after Compile.
///   Members non-const: Compile fills each record field by field in the graph arena.
/// Invalidation: With the graph.
/// Units: Pass and resource declaration indices; subresource indices mip × layerCount + layer, or buffer bytes.
/// Errors: None.
/// Version: since Z2 Stage 2. m_strText follows "Diagnostic text" of this file; a core diagnostic's text is carried
///   unchanged after the Flux prefix.
/// @endcontract
struct Flux_GraphDiagnostic
{
	Flux_RGError m_eCode;
	uint32 m_uPass = 0xFFFFFFFFu;
	uint32 m_uResource = 0xFFFFFFFFu;
	uint32 m_uVersion = 0;
	uint64 m_ulRangeFirst = 0;              // first subresource, or first byte for a buffer
	uint64 m_ulRangeCount = 0;
	Zenith_StringView m_strText;
};

/// @contract value Flux_GraphInfoRecord
/// Ownership: Plain value stored in the graph arena.
/// Thread affinity: IMMUTABLE after Compile.
///   Members non-const: Compile fills each record field by field in the graph arena.
/// Invalidation: With the graph.
/// Units: Pass declaration indices.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Flux_GraphInfoRecord { Flux_RGInfo m_eInfo; uint32 m_uPassA; uint32 m_uPassB; uint32 m_uReason; };

/// @contract value Flux_GraphErrorList
/// Ownership: The error record of Flux_RenderGraph::Compile ([Z2-N-CONTRACT-480]): 24 bytes, trivially copyable, no
///   pointer; the list itself is Flux_RenderGraph::GetDiagnostics().
/// Thread affinity: IMMUTABLE.
///   Members non-const: the producer default-constructs it and fills its fields before returning it.
/// Invalidation: None.
/// Units: m_uErrorCount counts GetDiagnostics() entries.
/// Errors: m_eCode is the first diagnostic's code after sorting by (code, pass); m_uErrorCount is at least 1.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Flux_GraphErrorList
{
	Flux_RGError m_eCode = FLUX_RG_ERROR_CAPACITY;
	uint32 m_uErrorCount = 0;
	uint32 m_uFirstPass = 0xFFFFFFFFu;
	uint64 m_ulStructureHash = 0;
};
static_assert(sizeof(Flux_GraphErrorList) <= 32);

constexpr const char* Zenith_ToString(Flux_RGError eError);      // the enumerator without FLUX_RG_ERROR_
constexpr const char* Zenith_ToString(Flux_RGInfo eInfo);
constexpr const char* Zenith_ToString(Flux_RecordCheck eCheck);
```

### Diagnostic text

| Code | Text (one line; `<p>` pass name with view suffix, `<r>` resource name, `<v>` version, `<range>` `mips a–b layers c–d` or `bytes [x, y)`) |
|---|---|
| `INVALID_HANDLE` | `graph: pass '<p>' uses a handle of another frame's graph or an unknown resource (index <i>)` |
| `SUBRANGE_OUT_OF_BOUNDS` | `graph: pass '<p>' names <range> outside resource '<r>'` |
| `SELF_ORDERING` | `graph: pass '<p>' reads and writes <range> of '<r>' through two fields` |
| `CAPACITY` | `graph: <what> capacity <n> exceeded at pass '<p>'` |
| `FIELD_UNSET` | `graph: pass '<p>' field '<field>' of <params> is unset and not [Optional]` |
| `VIEW_TYPE_MISMATCH` | `graph: pass '<p>' field '<field>' expects <tag> but '<r>' is <dimension or stride>` |
| `STALE_WRITE` | `graph: pass '<p>' writes '<r>' version <v> but version <latest> exists (forked history)` |
| `CYCLE` | `graph: dependency cycle: '<p1>' -(<r1> v<v1>)-> '<p2>' … -> '<p1>'` (the core cycle block, [Z2-N-REND-304]) |
| `READ_BEFORE_WRITE` | `graph: pass '<p>' reads <range> of transient '<r>' before any write covers it` |
| `PARTIAL_FIRST_WRITE` | `graph: pass '<p>' first-writes '<r>' without a clear or declared full coverage` |
| `PERSISTENT_WRITE` | `graph: pass '<p>' writes persistent read-only resource '<r>'` |
| `UNDECLARED_BUFFER` | `graph: pass '<p>' declares an invalid <vertex|index|indirect> reference <i>` |
| `UNWRITTEN_ATTACHMENT` | `graph: pass '<p>' binds colour slot <s> ('<r>') that no declared program writes` |
| `DEPTH_WRITABLE_UNUSED` | `graph: pass '<p>' binds '<r>' writable but no pipeline it bound writes depth` |
| `D32_UNJUSTIFIED` | `graph: depth target '<r>' is D32F_S8 without a precision or capability reason` |
| `32BIT_TARGET_UNJUSTIFIED` | `graph: render target '<r>' is <format> without a justified-list entry` |
| `MSAA_UNSUPPORTED` | `graph: pass '<p>' declares a resolve or a sample count above one` |
| `TILE_LOCAL_UNSUPPORTED` | `graph: pass '<p>' field '<field>' is a tile-local read; not available until D-TBDR is ruled` |
| `EXTENT_MISMATCH` | `graph: pass '<p>' attachments differ in extent (<w1>x<h1> vs <w2>x<h2>) or its render area leaves them` |
| `FORMAT_FEATURE_MISSING` | `graph: pass '<p>' uses <format> as <feature> which the format-feature table does not grant` |
| `TABLE_WRITE` | `graph: pass '<p>' writes GPU scene table '<r>'; only the scatter and growth passes may` |
| `SWAPCHAIN_QUEUE` | `graph: pass '<p>' on the <queue> queue writes the swapchain image` |

## Internal graph files

| File (`Zenith2/Engine/ZenithFlux/Internal/Graph/`) | Owner | Contents |
|---|---|---|
| `Flux_RenderGraph.cpp` | RPROD | construction, the resource and view calls, AddParams, every Add*Pass, the compile driver |
| `Flux_RenderGraphInternal.h` | RPROD | the internal entry points of "Graph internal entry points" and the setup tables |
| `Flux_GraphTables.h/.cpp` | RPROD | the setup tables of "Setup tables" and the version-alias table |
| `Flux_FieldTableWalk.h/.cpp` | RPROD | "Field-table walk" |
| `Flux_CompiledGraph.h` | RPROD (back-end members RHI) | "Flux_CompiledGraph" |
| `Flux_GraphCull.cpp` | RPROD | step 1 |
| `Flux_GraphOrder.cpp` | RPROD | step 2 and the core declaration, including the first-write check of "First-access widening" |
| `Flux_GraphPlacement.cpp` | RPROD | step 4 |
| `Flux_GraphCaches.cpp` | RPROD | "Physical resource caches" |
| `Flux_BarrierTypes.h` | RPROD | `Flux_ResourceAccess`, `Flux_ImageLayout`, `Flux_BarrierState`, `Flux_BarrierRecord`, `Flux_SplitRole`, `Flux_SubresourceState` |
| `Flux_BarrierSynthesis.h/.cpp` | RPROD | step 5 and "Split-barrier candidates" |
| `Flux_BarrierVerifier.h/.cpp` | RPROD | "Barrier verifier" |
| `Flux_GraphMerge.cpp` | RPROD | "Pass merging algorithm" |
| `Flux_GraphChecks.cpp` | RPROD | "Compile-time rule checks" |
| `Justified32BitFormats.json`, `GenerateJustifiedFormats.cmake` | RPROD | the justified list and its generator (output `${ZENITH_OUT}/generated/ZenithFlux/Flux_Justified32BitFormats.generated.h`, declared through `GENERATES`) |
| `Flux_Poison.cpp` | RPROD | "First-write rule and poisoning" |
| `Flux_Timestamps.cpp` | RPROD | "Timestamp insertion and attribution" |
| `Flux_PassContext.cpp` | RPROD | "Pass context implementation" |
| `Flux_RecordChecks.cpp` | RPROD | "Record-time checks" |
| `Flux_FrameDumpFrontEnd.cpp` | RPROD | "Front-end frame dump keys" |
| `Flux_GraphQueues.*`, `Flux_GraphPartition.*`, `Flux_ToRHIBarrier.*`, `Flux_GraphRecorder.*`, `Flux_GraphSubmitter.*`, `Flux_ImportedStateTable.*`, `Flux_FrameDumpWriter.*` | RHI | `Zenith2/Docs/Design/RHI/RenderGraphBackend.md → "Internal file map"` |

## Graph internal entry points

<!-- elaboration -->
`Internal/Graph/Flux_RenderGraphInternal.h` declares the entries the public templates, the RHI-owned GPU scene and the test access call. Every public `Add*Pass` template reads its parameter struct's field table and forwards to these, so the test access of `Zenith2/Docs/Design/RHI/RenderGraphBackend.md → "Flux_RenderGraphTestAccess"` exercises the production path.

```cpp
struct Flux_GraphPassDecl                  // one declared pass, before its fields are walked
{
	Zenith_StringView m_strName;
	Flux_PassKind m_eKind;                 // the enumeration below
	uint32 m_uFlags;                      // Flux_PassFlags | uFLUX_PASS_INTERNAL_FLAG_TABLE_WRITER
	uint32 m_uCostHint;                   // [Z2-N-REND-303]
	Zenith_RHIShaderProgramID m_xProgram;  // zero for program-less passes
	Zenith_RHIPermutationKey m_xPermutation;
	const void* m_pParams;                 // graph-arena copy target of the parameter struct
	uint32 m_uParamsSize;
	Zenith_Span<const Zenith_RHIParamFieldDesc> m_axFields;
};
enum Flux_PassKind : uint8
{
	FLUX_PASS_KIND_COMPUTE = 1, FLUX_PASS_KIND_COMPUTE_INDIRECT, FLUX_PASS_KIND_FULLSCREEN, FLUX_PASS_KIND_RASTER,
	FLUX_PASS_KIND_CLEAR, FLUX_PASS_KIND_CLEAR_BUFFER, FLUX_PASS_KIND_COPY_TEXTURE, FLUX_PASS_KIND_COPY_BUFFER, FLUX_PASS_KIND_READBACK,
};
inline constexpr uint32 uFLUX_PASS_INTERNAL_FLAG_TABLE_WRITER = 1u << 31;   // the GPU scene scatter and growth passes alone

uint32 Flux_GraphInternal_AddPass(Flux_RenderGraph& xGraph, const Flux_GraphPassDecl& xDecl);   // returns the pass index
void Flux_GraphInternal_AddBufferAccess(Flux_RenderGraph& xGraph, uint32 uPass, Flux_RGBuffer xBuffer, Zenith_GraphAccessMode eMode,
	uint64 ulOffset, uint64 ulBytes, Flux_ResourceAccess eAccess, uint16 uShaderStages);
void Flux_GraphInternal_AddTextureAccess(Flux_RenderGraph& xGraph, uint32 uPass, Flux_RGTexture xTexture, Zenith_GraphAccessMode eMode,
	const Flux_RGTextureRange& xRange, Flux_ResourceAccess eAccess, uint16 uShaderStages, uint32 uAccessFlags);
void Flux_GraphInternal_AddOrdering(Flux_RenderGraph& xGraph, uint32 uBefore, uint32 uAfter, Zenith_StringView strReason);   // test access only
void Flux_GraphInternal_SetRasterTargets(Flux_RenderGraph& xGraph, uint32 uPass, const Flux_RasterTargets& xTargets);
void Flux_GraphInternal_SetBody(Flux_RenderGraph& xGraph, uint32 uPass, const Zenith_Function<void(Flux_RasterContext&)>& xBody);
Flux_RGBuffer Flux_GraphInternal_ImportWriteRestricted(Flux_RenderGraph& xGraph, const char* szName, Zenith_RHIBufferHandle xBuffer);   // GPU scene tables
const Flux_CompiledGraph* Flux_GraphInternal_GetCompiled(const Flux_RenderGraph& xGraph);
```

| Access flag (`uAccessFlags`) | Meaning |
|---|---|
| `FLUX_ACCESS_FLAG_FULL_COVERAGE` (bit 0) | the access writes every texel or byte of its range |
| `FLUX_ACCESS_FLAG_BLEND_OR_LOAD` (bit 1) | a colour attachment that is loaded or may be blended (adds `COLOUR_ATTACHMENT_READ`) |
| `FLUX_ACCESS_FLAG_ALSO_SAMPLED` (bit 2) | a read-only depth attachment also sampled in the same pass |
| `FLUX_ACCESS_FLAG_STORAGE_VIEW_FORMAT` (bit 3) | the view uses the texture's `m_eStorageViewFormat` (GB0's UNORM view, [Z2-N-REND-844]) |

## Setup tables

<!-- elaboration -->
Every table is a `Zenith_Array` in the graph arena (`Zenith2/Docs/Design/CORE/Containers.md → "Zenith_Array"`), reserved at construction to the previous frame's high-water mark plus 25 %, so a steady frame allocates each table once.

| Record | Bytes | Fields |
|---|---|---|
| `Flux_RGResourceRecord` | 96 | name (arena view), kind (texture or buffer), texture or buffer description, flags (`TRANSIENT`, `IMPORTED`, `OUTPUT`, `WRITE_RESTRICTED`, `SWAPCHAIN`, `PERSISTENT_READ_ONLY`), the imported RHI handle, subresource count (mips × layers, or `ceil(size / 256)` granules), latest version, first-access pass, the core resource handle |
| `Flux_RGViewRecord` | 32 | resource index, SRV or UAV, texture range or buffer range, view format, tag dimension and element stride (from the field) |
| `Flux_RGPassRecord` | 128 | the `Flux_GraphPassDecl` values, the parameter-struct copy, the dispatch size or indirect reference, the raster-targets copy, the body, the access-record range, the view-suffix index, the cull reason |
| `Flux_RGAccessRecord` | 40 | pass, resource, version read, version written, `Flux_ResourceAccess`, shader stages, range (first, count in subresources, or byte offset and size), access flags |
| version alias | 8 | `(resource, version) → version` for versions a culled no-draw pass would have produced (sub-step 1a of `Zenith2/Docs/Rendering/RenderGraph.md → "Step 1 cull unused passes"`) |

## Field-table walk

At each `AddComputePass`, `AddComputePassIndirect`, `AddFullscreenPass` and `AddRasterPass`, after copying the parameter struct into the arena, the graph walks `Zenith_RHIParamFieldTable<TParams>::xFIELDS` in table order:

| Step | Action |
|---|---|
| 1 | read the 8 bytes at the field's offset as `{word0, word1}`; `word0 == uFLUX_RG_INVALID` is unset: `[Optional]` fields (`ZENITH_RHI_PARAM_FIELD_FLAG_OPTIONAL`) declare nothing and are patched to slot 0 at record; any other unset field records `FLUX_RG_ERROR_FIELD_UNSET` |
| 2 | a `PERSISTENT_SRV` field declares nothing ([Z2-N-REND-068]) |
| 3 | resolve the view (word1 bits 0–15) and its resource (word0); a foreign or out-of-range value records `FLUX_RG_ERROR_INVALID_HANDLE` |
| 4 | compare the field's tag (`m_eDim`, `m_uElementStride`) with the view: a mismatch records `FLUX_RG_ERROR_VIEW_TYPE_MISMATCH` ([Z2-N-REND-067]) |
| 5 | `TILE_LOCAL_READ` records `FLUX_RG_ERROR_TILE_LOCAL_UNSUPPORTED` ([Z2-N-REND-011]) |
| 6 | derive the access: SRV kinds → `SRV_COMPUTE` in compute passes, `SRV_GRAPHICS` otherwise, with the field's stage mask intersected with the program's stages; UAV kinds → `UAV` with `READ_WRITE` or `WRITE` from the field; `FULL_COVERAGE` from the field flag, valid only when the view names a whole mip (textures) or the whole buffer or the whole declared range (buffers) — a `[FullCoverage]` field over a partial view records its access as partial |
| 7 | map to the core mode: SRV → `GRAPH_ACCESS_READ`; UAV `WRITE` with full coverage → `WRITE_DISCARD`; UAV `WRITE` partial → `WRITE`; UAV `READ_WRITE` → `READ_WRITE` |
| 8 | append the access record; a second access of the same pass to an overlapping range of the same resource with a different access kind merges when both are reads of one layout, or when one is a read-only depth attachment and the other its SRV (`ALSO_SAMPLED`); any other overlap records `FLUX_RG_ERROR_SELF_ORDERING` |

Raster targets are walked after the fields with the table of "Flux_RasterTargets header". O(fields + attachments) per pass.

## Flux_CompiledGraph

<!-- elaboration -->
The compiled graph is the one interface between the RPROD front end and the RHI back end ([Z2-N-REND-307]; `Zenith2/Docs/Design/RHI/RenderGraphBackend.md → "Ownership of the compile steps"`). It lives in the graph arena and is immutable once `Compile()` returns.

```cpp
// Zenith2/Engine/ZenithFlux/Internal/Graph/Flux_CompiledGraph.h
struct Flux_CompiledPass                         // indexed by live-pass index (global order)
{
	// front end: RPROD
	uint32 m_uDeclIndex;                        // index into the setup pass table
	uint32 m_uCoreNode;                         // ZenithGraph node index
	Flux_PassKind m_eKind;
	uint32 m_uFlags;
	uint32 m_uCost;
	uint32 m_uScope;                            // merged rendering scope index; uFLUX_RG_INVALID for non-raster passes
	bool m_bScopeFirst;
	bool m_bScopeLast;
	uint32 m_uFirstBarrier;                     // into m_axBarriers: this pass's batch, recorded before it
	uint32 m_uBarrierCount;
	uint32 m_uFirstSplitEnd;                    // into m_auSplitEnds: split end halves due before this pass
	uint32 m_uSplitEndCount;
	uint32 m_uFirstSplitBegin;                  // into m_auSplitBegins: split begin halves after this pass
	uint32 m_uSplitBeginCount;
	uint32 m_uFirstRelease;                     // into m_auReleases: cross-queue release records after this pass's last access
	uint32 m_uReleaseCount;
	uint32 m_uTimestampIndex;                   // 2 × queue position; uFLUX_RG_INVALID when timestamps are off
	uint32 m_uBreadcrumbIndex;                  // the live-pass index
	uint32 m_uPoisonFill;                       // into m_axPoisonFills, uFLUX_RG_INVALID when none precede the pass
	// back end: RHI, filled by steps 3, 6 and 7 through m_xQueuePlan and m_xPartitionPlan (the same index)
};

struct Flux_ResourceLifetime                     // filled by RHI step 3c
{
	uint32 m_uFirstGlobal;                      // first live access, global index
	uint32 m_uLastGlobal;
	uint32 m_auFirstPosition[FLUX_QUEUE_COUNT]; // per queue; uFLUX_RG_INVALID when unused on that queue
	uint32 m_auLastPosition[FLUX_QUEUE_COUNT];
	uint32 m_uLastUseStages;                    // Zenith_RHIStage bits of the last access on each queue, OR-ed
};

struct Flux_PlacedTransient                      // step 4 output, one per live transient
{
	uint32 m_uResource;
	uint32 m_uGroup;                            // uFLUX_RG_INVALID with aliasing off (dedicated)
	uint64 m_ulOffset;
	uint64 m_ulSize;
	uint32 m_uFirstEarlierOccupant;             // into m_auOccupants: earlier transients overlapping this one's bytes
	uint32 m_uEarlierOccupantCount;
	Zenith_RHIResourceHandle m_xPhysical;        // the placed or dedicated RHI resource realised this frame
};

class Flux_CompiledGraph
{
public:
	Zenith_Span<const Flux_CompiledPass> m_axPasses;
	Zenith_Span<const uint32> m_auPredecessorOffset;     // CSR: live-pass → direct predecessors (graph edges, folded passes contracted)
	Zenith_Span<const uint32> m_auPredecessors;
	Zenith_Span<const uint32> m_auSuccessorOffset;
	Zenith_Span<const uint32> m_auSuccessors;
	Zenith_Span<Flux_ResourceLifetime> m_axLifetimes;     // indexed by resource; written by RHI step 3c
	Zenith_Span<const Flux_PlacedTransient> m_axPlacement;
	Zenith_Span<const uint32> m_auOccupants;
	Zenith_Span<const Flux_BarrierRecord> m_axBarriers;
	Zenith_Span<const uint32> m_auSplitEnds;             // record indices
	Zenith_Span<const uint32> m_auSplitBegins;
	Zenith_Span<const uint32> m_auReleases;
	Zenith_Span<const Flux_PoisonFill> m_axPoisonFills;
	Zenith_Span<const Flux_MergedScope> m_axScopes;
	Flux_QueuePlan m_xQueuePlan;                          // RHI, step 3 and step 6
	Flux_PartitionPlan m_xPartitionPlan;                  // RHI, step 7
	const Zenith_GraphSchedule* m_pxCoreSchedule;         // the core schedule of step 2
	uint64 m_ulStructureHash;
	uint64 m_ulFrameIndex;
	uint32 m_uTimestampCount[FLUX_QUEUE_COUNT];
	Flux_TransientMemoryStats m_xMemory;
};
```

`Flux_MergedScope` holds the scope's pass range, its attachment set (the first pass's resolved targets with its load ops and the last pass's store ops) and the render area; `Flux_PoisonFill` holds the resource, the subresource or byte range and the poison pattern of "First-write rule and poisoning".

## Compile driver

`Flux_RenderGraph::Compile()` runs, on the build node, in this order ([Z2-N-REND-307]); a step never reads a subsequent step's output, and every step runs even after an earlier one recorded errors when its inputs exist, so one compile reports every error ([Z2-N-REND-291]):

| # | Step | Owner | Entry |
|---|---|---|---|
| 0 | read `Flux_GraphSettings` once ([Z2-N-REND-290]); freeze the setup tables | RPROD | `Flux_RenderGraph::Compile` |
| 1 | cull and fold, sub-steps 1a and 1e around the core compile | RPROD | "Step 1 culling and clear folding" |
| 2 | declare to ZenithGraph and compile the core schedule (1b–1d and the order) | RPROD | "Step 2 ordering through ZenithGraph" |
| 2c | the compile-time rule checks that need no queue | RPROD | "Compile-time rule checks" |
| 3 | provisional queues, positions, lifetimes (3c), reachability | RHI | `Flux_GraphBuildQueuePlan(Flux_CompiledGraph&, const Zenith_RHICapabilities&, const Flux_GraphSettings&, Zenith_Arena&)` |
| 4 | memory placement | RPROD | "Step 4 placement realisation" |
| 5 | barrier synthesis, split candidates, poison fills | RPROD | "Step 5 barrier synthesis realisation" |
| 6 | queue assignment, batches, waits | RHI | `Flux_GraphAssignQueues(Flux_CompiledGraph&, Zenith_Arena&)` |
| 6d | `FLUX_RG_ERROR_SWAPCHAIN_QUEUE`: a pass on the async-compute or copy queue that writes the swapchain image | RHI | inside step 6 (`Zenith2/Docs/Design/RHI/RenderGraphBackend.md → "Step 6 queue assignment"`, row 6d) |
| 6m | pass merging | RPROD | "Pass merging algorithm" |
| 7 | partitioning; split demotion across partitions | RHI | `Flux_GraphPartition(Flux_CompiledGraph&, Zenith_Arena&)`, calling `Flux_DemoteSplit` of "Split-barrier candidates" |
| 8 | timestamp indices, breadcrumb indices | RPROD | "Timestamp insertion and attribution" |
| 9 | the barrier verifier (development builds and every graph test) | RPROD | "Barrier verifier" |
| 10 | the dump model copy when `m_uDumpFrames > 0` | RPROD front-end keys; RHI writer | "Front-end frame dump keys" |

The three back-end entries are RHI's declarations, answering row RP3: `Flux_GraphBuildQueuePlan` and `Flux_GraphAssignQueues` in `ZenithFlux/Internal/Graph/Flux_GraphQueues.h`, `Flux_GraphPartition` in `Flux_GraphPartition.h`, each `void …`, allocating from the graph arena and reporting through the compiled graph's diagnostic list (`Zenith2/Docs/Design/RHI/RenderGraphBackend.md → "Ownership of the compile steps"`). Step 6 demotes a split candidate whose halves fall in different submission batches through `Flux_DemoteSplit` in its row 6e (`→ "Step 6 queue assignment"`), and step 7 demotes a candidate whose halves would cross partitions through the same call (`→ "Step 7 command-list partitions"`); each demotion records `FLUX_RG_INFO_SPLIT_DEMOTED`. `FLUX_RG_ERROR_SWAPCHAIN_QUEUE` is step 6's row 6d.

`Flux.Graph.CompileMs` is the wall time of steps 0–10, published every frame and budgeted by `Z2-B-RHI-004` ([Z2-N-REND-308]).

## Step 1 culling and clear folding

| Sub-step | Realisation |
|---|---|
| 1a | before the core is declared: every raster pass with `ExpectedDraws(0)` and no side-effect flag becomes a clear-only pass when any attachment loads with `CLEAR` (its lambda is dropped, its accesses stay) and is otherwise removed; each version a removed pass would have produced is entered in the version-alias table as the version it read, so its consumers read the unchanged content; the pass is recorded `FLUX_RG_INFO_PASS_CULLED` with reason `noDraws` |
| 1b–1d | the core compile with `m_bCullUnusedNodes = true`: roots are the `GRAPH_NODE_FLAG_SIDE_EFFECT` nodes and the final writers of `GRAPH_RESOURCE_FLAG_OUTPUT` resources; liveness propagates over RAW and preserving-WAW edges (`Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md → "Step 6 culling"`), which is Rendering's "a live pass reads, or writes over with `LOAD`/read-write semantics" rule, because a `LOAD` or read-write access is a core `READ_WRITE` and a partial write is a preserving `WRITE`; each culled core node is recorded `FLUX_RG_INFO_PASS_CULLED` with reason `noConsumer` |
| 1e | after the core order: a clear-only pass C whose cleared version's first reader in the order is a raster pass U using it as an attachment with `LOAD` is folded: U's load op becomes `CLEAR` with C's clear value, C leaves the live list, every direct predecessor of C becomes a direct predecessor of U in the contracted CSR, and `FLUX_RG_INFO_CLEAR_FOLDED` records (C, U); a clear whose next user is a compute pass, a copy, or a raster pass reading it as an SRV stays a standalone clear ([Z2-N-REND-915]) |

## Step 2 ordering through ZenithGraph

The front end declares one `Zenith_Graph` (`Zenith2/Docs/Design/GRAPH.md → "Zenith_Graph.h"`) in the graph arena, named `Flux/Frame`, front-end tag `gpu`, serial `(frame index & 0x7FFFFFFF) | 1`:

| Flux element | Core declaration |
|---|---|
| transient texture | `AddResource` with `m_uSubresourceCount` = mips × layers, flags `REQUIRE_DEFINED_READS | FIRST_WRITE_DISCARD_PER_SUBRESOURCE` ([Z2-N-ARCH-346]); every access with its real mode over its real range ("First-access widening") |
| transient buffer | as above, `m_uSubresourceCount = ceil(size / uFLUX_RG_BUFFER_GRANULE_BYTES)`; a byte range maps to granules `[floor(offset / 256), ceil((offset + size) / 256))`, which can only add dependencies, never remove one |
| imported resource | `GRAPH_RESOURCE_FLAG_IMPORTED`, plus `OUTPUT` when `MarkOutput` was called |
| surviving pass (declaration order) | `AddNode`: cost = the cost hint, lane = 0 graphics-eligible, 1 async-flagged, 2 copy-flagged, `GRAPH_NODE_FLAG_SIDE_EFFECT` for `Flux_SideEffect` |
| access | `DeclareRead`/`DeclareWrite` with the mode of "Field-table walk" step 7, `m_uBackendUsage` = access kind (bits 0–7) | shader stages (8–23) | access flags (24–31) |

The core returns the order (descending critical-path cost, then ascending node index, which is ascending declaration index: [Z2-N-REND-303]), edges with reasons and lifetimes. Core diagnostics become Flux diagnostics through the table "Core error mapping" of `Zenith2/Docs/Rendering/RenderGraph.md → "Graph errors"`, texts unchanged after the `graph: ` prefix. Compile options: `m_bCullUnusedNodes = true`, `m_bTransitiveReduction = false` (the back end reads every direct edge), core schedule arena = the graph arena.

## First-access widening

<!-- elaboration -->
The core's first-write rule for GPU transients is per subresource interval (`GRAPH_RESOURCE_FLAG_FIRST_WRITE_DISCARD_PER_SUBRESOURCE`, [Z2-N-ARCH-346]: the first write of each subresource interval is `GRAPH_ACCESS_WRITE_DISCARD` over that interval, `Zenith2/Docs/Architecture/ZenithGraph.md → "The derivation algorithm"` step 3), and Rendering's is per subresource and per byte ([Z2-N-REND-441]): a bloom chain's first pass writes only chain mip 0. The two are the same rule at two granularities (the core sees a buffer in 256-byte granules), so the front end declares nothing wider than an access really is. The heading keeps its name for its citations; no first write is widened:

1. Before declaring to the core, the front end checks [Z2-N-REND-441] itself, exactly: per transient it keeps a written-interval set (per subresource for textures; exact byte intervals for buffers) and walks the resource's accesses in declaration order; a first access of an unwritten region that is a write without full coverage records `FLUX_RG_ERROR_PARTIAL_FIRST_WRITE`, and any read (including `LOAD` and read-write) of an unwritten region records `FLUX_RG_ERROR_READ_BEFORE_WRITE`, naming the pass, the resource and the region. O(A log A) per resource.
2. Every transient is declared with `GRAPH_RESOURCE_FLAG_REQUIRE_DEFINED_READS | GRAPH_RESOURCE_FLAG_FIRST_WRITE_DISCARD_PER_SUBRESOURCE` ("Step 2 ordering through ZenithGraph"), and every access with its real mode over its real range (the mode of "Field-table walk" step 7). A first write that passed step 1 covers its region fully, so its mode is already `GRAPH_ACCESS_WRITE_DISCARD` over exactly that region; no access is re-declared over subresources it does not touch. The barrier synthesiser emits each subresource's first-use discard at the pass that first writes it ([Z2-N-REND-438], [Z2-N-REND-440]).
3. The core therefore raises `GRAPH_ERROR_FIRST_WRITE_NOT_DISCARD` and `GRAPH_ERROR_UNDEFINED_READ` only for an access the front end failed to classify, which `Test_RenderGraph_Error_ReadBeforeWrite` and `Test_RenderGraph_Error_PartialFirstWrite` fix as front-end diagnoses.

### Consequence of the widening

The heading is kept for its citations; with per-subresource declaration there is no widening and therefore no added ordering:

| Case | Core edges between first writers | Why |
|---|---|---|
| first writers of disjoint texture subresources (six faces of one transient cube written by six views; tiles of an atlas array layer-written by several shadow views) | none | each writer's interval is unwritten when it writes, so the core derives no `WAW` (`Zenith2/Docs/Architecture/ZenithGraph.md → "The derivation algorithm"` step 2 adds `WAW` only from an interval's last writer) |
| a chain whose later first writers read earlier regions (the bloom downsample chain, every mip chain built by successive downsample passes) | the `RAW` edges of the reads only | the ordering is data flow's |
| first writers of disjoint byte ranges that share a 256-byte granule | one `WAW` per shared granule | the granule mapping of "Step 2 ordering through ZenithGraph" can only add dependencies, never remove one; the reference of `Test_RenderGraph_FirstWriteAddsNoEdge` uses the same granules, so such an edge is in both sets |

So no compiled edge joins two passes the frame's data flow leaves unordered, no cross-queue wait ([Z2-N-REND-392]) or loss of `may_share` freedom ([Z2-N-REND-436]) comes from a first write, and a capture cubemap or a layer-written atlas can be declared as one transient. `Test_RenderGraph_FirstWriteAddsNoEdge` (`Zenith2/Docs/Design/RPROD/Tests.md` → "Widened first write test") asserts the closure equality over the frame fixtures, and `Test_RenderGraph_IndependentFacesNoEdge` asserts that six disjoint face writers compile with no edge between them. The flag itself is GRAPH's (`Test_GraphConformance_PerSubresourceFirstWriteDiscard`, `Zenith2/Docs/Architecture/ZenithGraph.md → "Conformance suite"`).

## Step 4 placement realisation

The algorithm is `Zenith2/Docs/Rendering/RenderGraph.md → "Aliasing placement"` ([Z2-N-REND-435]); this design fixes its inputs:

| Input | Source |
|---|---|
| live transients and lifetimes | the setup table's `TRANSIENT` records with a live access; `Flux_ResourceLifetime` from RHI step 3c |
| requirements | `Zenith_RHIDevice::GetTextureMemoryRequirements` / `GetBufferMemoryRequirements` of the transient's RHI description; the description's usage bits are the union of every access kind the frame declares on it and its queue mask is every queue step 3 placed an access on (buffers always all three, [Z2-N-REND-391]) |
| group | `Zenith_RHIMemoryRequirements::m_uPlacementGroup` (`Zenith2/Docs/Design/RHI/Memory.md → "Heap alignment and placement"`) |
| `may_share(a, b)` | with `L(a)` the accesses of a that no other access of a follows and `F(b)` the accesses of b that no other access of b precedes, `may_share` holds when every pass of `F(b)` has every pass of `L(a)` set in its reachability row (`Flux_QueuePlan::m_aulReachability`), or symmetrically; one queue gives `|L| = |F| = 1`, so the test is one bit lookup ([Z2-N-REND-436]) |
| earlier occupants | for each placed transient, the placed transients of the same group whose byte ranges overlap and whose uses precede it; recorded in `m_auOccupants` for the discard's source scope ([Z2-N-REND-438]) |
| aliasing off | every transient takes a dedicated allocation from the dedicated cache keyed by (description hash, usage union, queue mask, ordinal among identical keys this frame) ([Z2-N-REND-439]) |

Realisation (Rendering step 7) goes through "Physical resource caches". Placement is O(n²) per group in its transient count n.

## Physical resource caches

<!-- elaboration -->
| Cache | Key | Value | Retirement |
|---|---|---|---|
| group heaps | placement group | one `Zenith_RHIHeapHandle` of size ≥ the frame's requirement rounded up to 4 MiB | grow in the frame the requirement exceeds it; shrink by reallocation after the requirement stayed below half for `m_uHeapShrinkFrames` frames ([Z2-N-REND-437]) |
| placed resources | (description hash, usage union, queue mask, heap handle, offset) | the placed texture or buffer | unused for `m_uRetireFrames` frames ([Z2-N-REND-287]); always when its heap retires |
| dedicated resources | (description hash, usage union, queue mask, ordinal) | a committed texture or buffer | unused for `m_uRetireFrames` frames |

The description hash is `Zenith_Hash64` (`Zenith2/Docs/Design/CORE/Maths.md → "Zenith_Hash64"`) of the canonical little-endian bytes of the RHI description without its debug name. Every retired entry goes to the RHI retirement queue with the last-use values of the last frame that used it (`Zenith2/Docs/Design/RHI.md → "Retirement rule"`). `BeginFrame` applies the memory-pressure actions 2, 3 and 7 of `Zenith2/Docs/Rendering/GpuResources.md → "Responses to memory pressure"` through `SetPressureOverrides`, called from the rendering memory consumer (RHI's) at the prologue.

## Step 5 barrier synthesis realisation

<!-- elaboration -->
```cpp
// Internal/Graph/Flux_BarrierTypes.h
enum Flux_ResourceAccess : uint8
{
	FLUX_RESOURCE_ACCESS_UNDEFINED = 1, FLUX_RESOURCE_ACCESS_SRV_GRAPHICS, FLUX_RESOURCE_ACCESS_SRV_COMPUTE, FLUX_RESOURCE_ACCESS_UAV,
	FLUX_RESOURCE_ACCESS_COLOUR_ATTACHMENT, FLUX_RESOURCE_ACCESS_DEPTH_WRITE, FLUX_RESOURCE_ACCESS_DEPTH_READ_ONLY,
	FLUX_RESOURCE_ACCESS_VERTEX_BUFFER, FLUX_RESOURCE_ACCESS_INDEX_BUFFER, FLUX_RESOURCE_ACCESS_INDIRECT_ARGS,
	FLUX_RESOURCE_ACCESS_COPY_SOURCE, FLUX_RESOURCE_ACCESS_COPY_DEST, FLUX_RESOURCE_ACCESS_CLEAR, FLUX_RESOURCE_ACCESS_PRESENT,
	FLUX_RESOURCE_ACCESS_PERSISTENT_READ,
};
enum Flux_ImageLayout : uint8 { /* the values of Zenith_RHITextureLayout, one to one, FLUX_IMAGE_LAYOUT_ prefix; static_assert per value */ };
enum Flux_SplitRole : uint8 { FLUX_SPLIT_NONE = 0, FLUX_SPLIT_CANDIDATE = 1, FLUX_SPLIT_DEMOTED = 2 };

struct Flux_BarrierState                // 12 bytes
{
	Flux_ResourceAccess m_eAccess;
	Flux_ImageLayout m_eLayout;         // textures; FLUX_IMAGE_LAYOUT_UNDEFINED for buffers
	Flux_QueueClass m_eQueue;
	uint8 m_uPad;
	uint32 m_uStages;                  // Zenith_RHIStage bits (the neutral vocabulary is the Flux stage vocabulary)
	uint32 m_uAccessBits;              // Zenith_RHIAccess bits
};

struct Flux_BarrierRecord               // 40 bytes; RHI's Flux_ToRHIBarrier reads it
{
	uint32 m_uResource;
	uint32 m_uFirstSubresource;        // textures: mip × layerCount + layer; buffers: byte offset / 16
	uint32 m_uSubresourceCount;        // buffers: bytes / 16; 0 = whole resource
	Flux_BarrierState m_xFrom;
	Flux_BarrierState m_xTo;
	Flux_SplitRole m_eSplit;
	bool m_bDiscard;                    // first use of a transient: from UNDEFINED, source = earlier occupants' last-use stages
	uint16 m_uPad;
	uint32 m_uProducerPass;            // live-pass index of the last access in m_xFrom (split begin, cross-queue release)
};
```

`Flux_SubresourceState` is the declaration of `Zenith2/Docs/Rendering/RenderGraph.md → "Barrier state machine"` (heading "Tracking"), with `m_uStageMask` and `m_uAccessMask` holding `Zenith_RHIStage` and `Zenith_RHIAccess` bits. Algorithm, over the live passes in global order:

| Step | Action |
|---|---|
| 1 | per resource, initialise the run list over its subresources: a transient starts `UNDEFINED` (its first access will be a discard); an import starts from its imported-state table entry (`Zenith2/Docs/Design/RHI/RenderGraphBackend.md → "Imported-state table"`: `Find`, runs, owning queue), or from `UNDEFINED` flagged discarded when unregistered |
| 2 | for each pass P and each access of P, map (access kind, field stages, flags) to its neutral (stages, access bits, layout) through RHI's `Flux_ToRHIAccess` (`Zenith2/Docs/Design/RHI/RenderGraphBackend.md → "Neutral barrier translation"`), and walk the resource's runs intersecting the access range |
| 3 | for each intersecting run, classify the transition by the table "Tracking" of Rendering and emit exactly that barrier ([Z2-N-REND-385]): read after read, same layout, same queue: no record, the stages and access bits accumulate into the run; read after read with a layout change: a layout transition whose source is the accumulated read stages; read after write: a memory barrier from the write to the read; write after read: an execution dependency from the accumulated reads; write after write (UAV after UAV included, [Z2-N-REND-386]): a memory barrier; first use of a transient: a discard record whose source stages are the OR of the last-use stages of the transient's earlier occupants (step 4) and `NONE` when it has none; a queue change: a record with `m_xFrom.m_eQueue ≠ m_xTo.m_eQueue`, also listed in the producer pass's release list |
| 4 | the read-run look-ahead: the record emitted before the first reader of a version (read after write, or a layout change) takes as its destination the union of the stages and access bits of every read of that version that follows on the same queue with the same layout before the next write, so subsequent readers in the run need no barrier and "merges compatible reads" holds with no hazard |
| 5 | update the run state; split and merge runs at access boundaries so that adjacent subresources with equal state form one run |
| 6 | coalesce the pass's records: records of one resource with equal from and to states and contiguous ranges merge; the pass's list is its one batch ([Z2-N-REND-389]); buffer records without a queue change are marked for folding into the batch's global barrier by the back end ([Z2-N-REND-390]) |
| 7 | the imported resources' final run states and owning queues become the frame's commit input for RHI's imported-state table (Submission step 3) |

`PERSISTENT_READ` resources and `PersistentSRV` fields produce no record ([Z2-N-REND-388]). Complexity O(A × R̄) with R̄ the mean runs touched per access (1 in practice).

## Split-barrier candidates

- In step 5, a record whose producer and consumer are on the same queue with at least `uFLUX_RG_SPLIT_BARRIER_MIN_GAP` passes between them in that queue's order, when `m_bSplitBarriers` is set, is emitted as `FLUX_SPLIT_CANDIDATE`: its index enters the producer's split-begin list and the consumer's split-end list instead of the consumer's batch ([Z2-N-REND-393]).
- `Flux_DemoteSplit(Flux_CompiledGraph&, uint32 uRecord)` (RPROD, `Flux_BarrierSynthesis.h`) turns a candidate into an ordinary record in the consumer's batch and removes it from both lists; RHI's step 6 calls it for every candidate whose halves fall in different submission batches ([Z2-N-REND-394]); each demotion records `FLUX_RG_INFO_SPLIT_DEMOTED`.

[Z2-N-REND-393] makes step 5 emit a split candidate rather than a split (step 5 runs before queue assignment and partitioning, so it cannot know where the halves land), and [Z2-N-REND-394] has step 7 demote every candidate whose halves fall in different command-list partitions (`Zenith2/Docs/Design/RHI/RenderGraphBackend.md → "Step 7 command-list partitions"`: D3D12 split barriers end in the command list that began them), through `Flux_DemoteSplit`, recorded `FLUX_RG_INFO_SPLIT_DEMOTED` (X98 applied; RHI's step 7 demotes through `Flux_DemoteSplit`, row RP3).
- A split begin half due after a pass that is not the last of its merged scope is a barrier between two passes, so it blocks that merge (condition 5 of "Pass merging algorithm"); a begin half is therefore always recorded after its scope has ended.

<!-- elaboration -->
This realises reconciliation row R1 (RHI's division of Flux code: "the synthesiser emits a split barrier only where both halves stay in one partition") in the only order the steps allow: partitions exist after step 7, so step 5 emits candidates and steps 6 and 7 demote the ones that would straddle a batch or a partition; the realisation is reconciliation row RP1.

## Barrier verifier

`Flux_BarrierVerifier` ([Z2-N-REND-397]) runs after step 8 in development builds and in every graph test, and shares no code with step 5 beyond `Flux_ToRHIAccess`:

1. For each resource it starts from the same initial state as step 5 and replays the compiled order, applying only the recorded barriers (batches, split halves at their recorded positions, releases and acquires) to a per-subresource state that stores the last write's stages and access bits and the set of stages made visible since.
2. Before each access it asserts that the subresource's layout equals the access's layout, that the last write was made visible to the access's stages and access bits (or the access is on the write's own stage with a UAV barrier between), that a write after reads has an execution dependency from every accumulated read stage, and that a first transient access is preceded by a discard.
3. A failure records a `FLUX_RG_ERROR_READ_BEFORE_WRITE`-coded diagnostic carrying `verifier:` text naming the pass, the resource, the subresource and the missing dependency, and fails the compile; `Test_RenderGraph_BarrierVerifierCatchesMissingBarrier` corrupts a list and asserts it.

O(A) per frame; in Shipping the verifier is not compiled into the call path (the step is skipped by a build-configuration constant, not an `#if` in a class body).

## Pass merging algorithm

For each queue's passes in queue order, after step 6: a raster, clear-folded or fullscreen pass B joins the scope of the preceding pass A exactly when the five conditions of `Zenith2/Docs/Rendering/RenderGraph.md → "Pass merging"` hold ([Z2-N-REND-309]); this design evaluates them as:

| # | Test | Blocked reason |
|---|---|---|
| 1 | `m_bMergePasses` and neither flagged `FLUX_PASS_FLAG_NEVER_MERGE` | `flagged` |
| 2 | both graphics-queue raster, clear-folded or fullscreen passes, adjacent in the graphics queue order, in the same submission batch (partitions are cut afterwards and never split a scope) | `notAdjacent` |
| 3 | equal resolved attachment arrays (resource, subresource, view format), equal depth access mode and equal render area | `attachmentsDiffer` |
| 4 | for every attachment, A stores it and B loads it | `loadStoreIncompatible` |
| 5 | B's barrier batch is empty, no split end half is due before B, no split begin half follows A, and no cross-queue release follows A | `barrierBetween` |

Rendering's condition 2 reads "in the same partition". It is satisfied by construction: merging runs at step 6m, before step 7 cuts partitions, and step 7 never splits a merged scope ([Z2-N-REND-305]), so two passes merged into one scope always land in one partition; no partition index exists at step 6m for the test to read, and test 2 above is the one that decides the same question.

The merged scope takes the first pass's load ops and the last pass's store ops, each pass keeps its timestamps, and every blocked adjacency is recorded with its reason for the dump's `scopes[].blocked` ([Z2-N-REND-314]). No merge reads an attachment tile-locally ([Z2-N-REND-310]). O(P).

## Compile-time rule checks

`Flux_GraphChecks.cpp` evaluates the GPU performance rules the graph enforces ([Z2-N-REND-915] to [Z2-N-REND-920]); each failure is a diagnostic of the named code, and every check runs on every compile.

| Check | Rule | Code |
|---|---|---|
| a D32F_S8 depth attachment or depth texture whose `m_eDepthReason` is neither `PRECISION` nor `CAPABILITY` | Depth format ([Z2-N-REND-596]) | `FLUX_RG_ERROR_D32_UNJUSTIFIED` |
| a texture with colour-attachment, depth or storage usage whose format has 32 bits per channel and whose (format, `m_eJustified32BitUse`) pair is not in the generated justified table | No full 32-bit render targets ([Z2-N-REND-918]) | `FLUX_RG_ERROR_32BIT_TARGET_UNJUSTIFIED` |
| a bound colour slot that no program of the pass's declared shader set writes in any compiled permutation (the permutation flags of the shader container, `Zenith2/Docs/Design/RPROD/PSO.md → "Shader blob loading"`) | Don't bind attachments unnecessarily ([Z2-N-REND-919], [Z2-N-REND-920]) | `FLUX_RG_ERROR_UNWRITTEN_ATTACHMENT` |
| a raster pass whose declared shader set is empty | as above | `FLUX_RG_ERROR_UNWRITTEN_ATTACHMENT` |
| an attachment, storage write, typed load (a read-write texture UAV) or sampled read of a format whose `Zenith_GetRequiredFormatFeatures` lacks the feature (`Zenith2/Docs/Design/RHI/PublicAPI.md → "Zenith_FormatFeatures header"`); blending is checked where the PSO is known ("Record-time checks", and the PSO lint) | Required format features ([Z2-N-REND-038]) | `FLUX_RG_ERROR_FORMAT_FEATURE_MISSING` |
| a pass's attachments of different extents, or a render area outside them | [Z2-N-REND-298] | `FLUX_RG_ERROR_EXTENT_MISMATCH` |
| `Resolve()` called, or a sample count other than one | [Z2-N-REND-010], [Z2-N-REND-298] | `FLUX_RG_ERROR_MSAA_UNSUPPORTED` |
| a write access on a `WRITE_RESTRICTED` import by a pass without the table-writer flag | [Z2-N-REND-536] | `FLUX_RG_ERROR_TABLE_WRITE` |
| any write access to, or attachment of, an import whose RHI handle the imported-state table flags `PERSISTENT_READ_ONLY` | [Z2-N-REND-547] | `FLUX_RG_ERROR_PERSISTENT_WRITE` |
| a vertex, index or indirect reference with both or neither of its buffers set, or an index into the pass's declared list out of range | [Z2-N-REND-297] | `FLUX_RG_ERROR_UNDECLARED_BUFFER` |

`FLUX_RG_ERROR_DEPTH_WRITABLE_UNUSED` depends on the raster states the lambda binds, which no compile can see, so it is evaluated at the end of the pass's recording (development builds and every graph and render test), the record-time placement Rendering states since X101 (e) (reconciliation row RP2): a pass that declared depth `WRITE`, issued at least one draw, and bound no PSO with depth write enabled appends the diagnostic, with its `Flux_RGError` code, to the frame's record-check report, which fails the test ([Z2-N-REND-919]).

<!-- elaboration -->
`Justified32BitFormats.json` is `{ "schemaVersion": 1, "entries": [ { "format": "ZENITH_RHI_FORMAT_R32_UINT", "use": "FLUX_32BIT_USE_VISIBILITY_ID", "decision": "Z2-D-014", "reason": "visibility ID" }, { "format": "ZENITH_RHI_FORMAT_D32_FLOAT_S8X24_UINT", "use": "FLUX_32BIT_USE_DEPTH_VIEW", "decision": "Z2-D-061", "reason": "justified depth views" } ] }`; the generator fails the build on an entry without a `Z2-D-` decision, an unknown format or use, or a duplicate pair.

## First-write rule and poisoning

- The rule is enforced by "First-access widening" step 1 ([Z2-N-REND-441]); imported resources are exempt ([Z2-N-REND-443]).
- With `m_bPoisonTransients` set (development builds), step 5 appends, for each transient, a `Flux_PoisonFill` after its discard and before its first write, covering every subresource or byte: a `ClearColourTexture` with the pattern of [Z2-N-REND-442] for colour and storage textures (float, half and packed float: quiet NaN `0x7FC00000` / `0x7E00` / per-channel R11G11B10 NaN; UNORM and SNORM: `(1, 0, 1, 0.5)`; integer: all ones), a depth clear to the depth mode's far value for depth textures (a depth NaN is not representable in D24), and a `FillBuffer` of `0xFFFFFFFF` for buffers; the fill is recorded as a clear access, so the synthesiser orders it and the first write follows it.
- With aliasing off the transient still receives its discard, and poisoning behaves identically in both modes ([Z2-N-REND-440]), which is what makes `Test_AliasingParity_<Scene>` a bit-identical comparison ([Z2-N-REND-444]).

## Timestamp insertion and attribution

- After step 7, each live pass on queue q receives `m_uTimestampIndex = 2 × (its position in q)` when `m_bTimestamps` is set; the per-queue counts size the frame slot's query pools (at most 8,192 queries, [Z2-N-REND-311]); a queue whose `m_auTimestampValidBits` is 0 gets no index and reports "n/a".
- RHI records the queries (`Zenith2/Docs/Design/RHI/RenderGraphBackend.md → "Per-pass recording order"`, orders 4 and 9) and reads the ticks when the frame slot is reused (`Zenith2/Docs/Design/RHI.md → "Frame-in-flight rotation"`, step 4), then hands them to `Flux_PassTimingPublisher::Publish` (RPROD, `Internal/Diagnostics/`), which converts ticks with the queue's period, aligns them with the calibration of `Zenith_RHIClockCalibration` when valid, and publishes one GPU zone per pass named with its view suffix to the profiler hooks and Tracy ([Z2-N-REND-312]).

The hand-off is RHI's, answering row RP4: `PrepareNextFrame` step 4 calls `Flux_PassTimingPublisher::Publish(const Flux_FrameTimestampRecord&, Zenith_Span<const uint64> xTicks, uint32 uQueue)` once per queue with a non-zero pair count, and a frames-in-flight change publishes the old slots' pending frames before retiring their pools (`Zenith2/Docs/Design/RHI.md → "Frame-in-flight rotation"`); `Flux_FrameTimestampRecord` (frame index, per-queue pair counts, per-queue pass-name views into a slot-owned pool) is RHI's, declared in `Internal/Lifetime/Flux_FrameLifetime.h`.

## Pass context implementation

<!-- elaboration -->
| Operation | Realisation over `Zenith_RHICommandList` (`Zenith2/Docs/Design/RHI/PublicAPIDevice.md → "Zenith_RHICommandList header"`) |
|---|---|
| `BindPipeline` | build the `Flux_PSOKey` from the program, permutation, raster state and the pass's resolved attachment formats ([Z2-N-REND-196]); look it up in the PSO table (PSO.md "PSO table and states"); `READY`: `BindPipeline(handle)`; otherwise apply the missing-PSO fallback and return false ([Z2-N-REND-302]) |
| `BeginDrawParams<T>` | `static_assert` that `T` is the pass's params type; copy the patched struct into the frame's upload ring (lock-free bump, [Z2-N-REND-496]) and return it; counts against `ExpectedDraws` |
| draw calls | push `{ring buffer index, offset}` with `SetPushConstants`, then on D3D12 `SetDrawIndexConstant(0)` before a direct draw ([Z2-N-REND-072]); indirect draws pass the declared buffers' physical handles and offsets; draws after a failed `BindPipeline` are dropped |
| `BindVertexStreams`, `BindIndexBuffer` | resolve the declared references (graph buffers to their physical handles, persistent buffers to their RHI buffers) and call `BindVertexBuffers` / `BindIndexBuffer16` |
| the null vertex stream | when the bound PSO's entry has `m_bNullStream` (its specialised table carries stream 7 at stride 0, `Zenith2/Docs/Design/RPROD/PSO.md → "PSO builder"`), the context binds GEOM's `Flux_NullVertexStream` (the `uVERTEX_NULL_STREAM_BYTES` (32-byte) zero-filled `Flux_PersistentReadOnlyBuffer` with usage `VERTEX`, `Zenith2/Docs/Design/GEOM/VertexFormat.md → "Program stream-set specialisation"`, `Internal/Geometry/Flux_NullVertexStream.h`, created with the renderer) at binding 7 before the next draw, through the persistent-reference form of `Flux_VertexStreamRef` (`m_xPersistent` set, `m_ulOffset` 0, `m_uStream` 7): on Vulkan `vkCmdBindVertexBuffers` at binding 7 offset 0 (the stride 0 is the PSO's binding description), on D3D12 `IASetVertexBuffers` slot 7 with `D3D12_VERTEX_BUFFER_VIEW { BufferLocation, SizeInBytes = 32, StrideInBytes = 0 }` (RHI's `BindVertexBuffers` carries the stride per binding). The pass declares nothing for it: the buffer is persistent read-only and constant, so it needs no barrier ([Z2-N-REND-388]) and is outside the declared-reference check of "Record-time checks"; it stays bound until a PSO without a null stream is bound (GEOM row GM25; `Zenith2/Docs/Design/GEOM/VertexFormat.md → "Program stream-set specialisation"`) |
| `SetScissor` | clamped to the render area ([Z2-N-REND-300]) |
| viewport | set once per pass by the recorder from the render area, with the negative-height Vulkan convention of `Zenith2/Docs/Design/RHI/Vulkan.md → "Command recording mapping"` |

## Record-time checks

In development builds and in every test, the contexts evaluate the checks of `Flux_RecordCheck` and `FLUX_RG_ERROR_DEPTH_WRITABLE_UNUSED` (the latter at record time, "Compile-time rule checks"):

| Check | When |
|---|---|
| `FLUX_RECORD_CHECK_UNWRITTEN_ATTACHMENT` | at `BindPipeline`: the program is not in the pass's declared set, or its permutation's written-output mask leaves a bound colour slot unwritten while the PSO's write mask for that slot is not zero ([Z2-N-REND-920]) |
| `FLUX_RECORD_CHECK_UNDECLARED_RESOURCE` | at each push: a resource field of the draw's params differs from the pass's patched value ([Z2-N-REND-301]); at `BindVertexStreams`/`BindIndexBuffer` with an undeclared index; an indirect call of the other form than the one declared ([Z2-N-REND-297]) |
| `FLUX_RECORD_CHECK_BLEND_UNSUPPORTED` | at `BindPipeline`: a blend-enabled attachment whose format lacks `ZENITH_RHI_FORMAT_FEATURE_BLEND` |
| `FLUX_RECORD_CHECK_DRAW_PARAMS_EXCEEDED` | at `BeginDrawParams` beyond `ExpectedDraws` ([Z2-N-REND-496]) |

A failure drops the offending call, logs `flux.graph` once per (pass, program, check), increments `Flux.Record.CheckFailures`, and is appended to the frame's record-check list, which the test support's frame sink turns into a test failure; "fails the frame" in [Z2-N-REND-297] and [Z2-N-REND-920] means exactly this. Shipping builds compile the checks out of the call path by a configuration constant.

## Front-end frame dump keys

The front end fills, into the dump model copied at compile step 10, the keys of `Zenith2/Docs/Rendering/RenderGraph.md → "Frame dump"` that RHI's writer does not produce (`Zenith2/Docs/Design/RHI/RenderGraphBackend.md → "Frame dump back-end sections"`): `settings`, `views` (from the renderer's view list), per pass `node`, `name`, `kind`, `scope`, `cullReason`, `reads`, `writes` (resource, version, access, stages, range, coverage) and `gpuMs` (the last published value of that pass name), `placement` (mode, group, offset, size, `sharesWith`), `scopes` (passes, attachments, `blocked` with reasons), `memory` (`transientBytesDedicated`, `transientBytesAliased`, `savingsBytes`, groups) and `errors` (every diagnostic's code and text). Culled passes appear with their `cullReason` and no queue ([Z2-N-REND-314]).

## Graph limits complexity and memory

| Quantity | Bound | On breach |
|---|---|---|
| passes, resources, views, accesses per frame | the `uFLUX_RG_MAX_*` constants | `FLUX_RG_ERROR_CAPACITY` at the declaration |
| versions per resource | 65,536 | `FLUX_RG_ERROR_CAPACITY` |
| graph arena | the renderer's graph arena block size (`r.graph.arenaBytes`, default 32 MiB, growing by blocks through the general heap) | an arena allocation failure is `FLUX_RG_ERROR_CAPACITY` naming the table |

| Step | Complexity |
|---|---|
| setup | O(fields + attachments) per pass |
| step 1 and 2 | the core's O((N + E) + N log N) plus O(A log A) for the first-write check |
| step 4 | O(n²) per placement group |
| step 5 | O(A × runs touched) |
| merging, timestamps | O(P) |
| verifier | O(A) |
