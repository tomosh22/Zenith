# Zenith2 rendering: render graph, barrier synthesis and transient aliasing

Section file of `Zenith2/Docs/Rendering.md`. It details the entry-file sections "Render graph", "Barrier synthesis" and "Transient aliasing". Normative statements share the `REND` tag family of the entry file. The graph code lives in `Zenith2/Engine/ZenithFlux/` (public headers `Flux_RenderGraph.h`, `Flux_RasterTargets.h`, `Flux_PassContext.h`, `Flux_GraphSettings.h`, `Flux_GraphCaches.h`, `Flux_GraphErrors.h`) and `Zenith2/Engine/ZenithFlux/Internal/Graph/` (compiler, barrier synthesis, placement, recording).

## Graph lifecycle

| Phase | Thread | What happens |
|---|---|---|
| 1 Begin | `Flux.BuildGraph` node (`JOB_AFFINITY_ANY`) | a `Flux_RenderGraph` is constructed over the frame's graph arena; the GPU scene tables, pools, per-view histories and the swapchain image are imported |
| 2 Setup | `Flux.BuildGraph` node | shared producers, then each view's feature functions, declare resources and passes |
| 3 Compile | `Flux.BuildGraph` node | the seven compile steps of "Graph compile", then the descriptor writes of the frame's views ([Z2-N-REND-052]) |
| 4 Record | `Flux.Record` job nodes (`JOB_AFFINITY_ANY`, frame threads), one per command-list partition | each partition's passes are recorded: barriers, timestamps, pass bodies |
| 5 Submit | `Flux.Submit` on the render-submit thread | partitions are submitted in order per queue, with timeline waits and signals |
| 6 End | `Flux.Present` on the render-submit thread | histories are exported to their views, readbacks registered, the arena released after record completes |

- [Z2-N-REND-285] The render graph is constructed, set up and compiled every frame on the GPU-graph build node of the CPU frame graph, over an arena of graph lifetime, and nothing the graph allocates outlives the frame except the physical resources held by its caches (Z2-D-055).
- [Z2-N-REND-286] Setup is single-threaded and runs features in a fixed order (shared producers, then views in view-list order, each view's features in its path's fixed order), so pass declaration order, and therefore every tie-break derived from it, is deterministic.
- [Z2-N-REND-287] Physical resources (transient heaps, placed and dedicated resources, readback buffers) are cached across frames keyed by a hash of their description plus their usage union, and a cached resource unused for `uFLUX_RG_CACHE_RETIRE_FRAMES` = 8 consecutive frames is released through deferred deletion.
- [Z2-N-REND-288] Every pass-lambda capture and every object the graph stores in its arena is trivially destructible, which the `AddRasterPass` template enforces with `static_assert(std::is_trivially_destructible_v<TLambda>)`; the arena runs no destructor.

## Graph public API

```cpp
struct Flux_RGTexture { uint32 m_uIndex = uFLUX_RG_INVALID; uint32 m_uVersion = 0; };
struct Flux_RGBuffer  { uint32 m_uIndex = uFLUX_RG_INVALID; uint32 m_uVersion = 0; };
struct Flux_RGPass    { uint32 m_uIndex = uFLUX_RG_INVALID; };

struct Flux_RGTextureDesc
{
	Zenith_RHIFormat m_eFormat;
	Flux_TextureDimension m_eDimension;     // FLUX_TEXTURE_DIMENSION_2D, _2D_ARRAY, _3D, _CUBE, _CUBE_ARRAY
	uint32 m_uWidth;
	uint32 m_uHeight;
	uint32 m_uDepthOrLayers;
	uint32 m_uMipCount;
	Flux_ClearValue m_xOptimisedClear;      // the clear value drivers may optimise for
	Zenith_RHIFormat m_eStorageViewFormat;       // ZENITH_RHI_FORMAT_UNDEFINED, or the second format of a mutable-format image (GB0: UNORM storage view of an sRGB image)
	Flux_DepthModeReason m_eDepthReason = FLUX_DEPTH_MODE_REASON_DEFAULT;   // depth targets: copied from the owning view's Flux_DepthMode (the D32 check)
	Flux_Justified32BitUse m_eJustified32BitUse = FLUX_32BIT_USE_NONE;      // the 32-bit target check's use ([Z2-N-REND-918])
};

struct Flux_RGBufferDesc
{
	uint64 m_ulSize;
	uint32 m_uStructureStride;             // 0 for byte-address buffers
};

struct Flux_DispatchSize { uint32 m_uX; uint32 m_uY; uint32 m_uZ; };   // in thread groups
Flux_DispatchSize Flux_DispatchFor2D(uint32 uWidth, uint32 uHeight, uint32 uGroupX, uint32 uGroupY);

enum Flux_PassFlags : uint32
{
	FLUX_PASS_FLAG_NONE          = 0,
	FLUX_PASS_FLAG_SIDE_EFFECT    = 1u << 0,   // never culled
	FLUX_PASS_FLAG_ASYNC_COMPUTE  = 1u << 1,   // may run on the async-compute queue ("A" in the frame tables)
	FLUX_PASS_FLAG_COPY_QUEUE     = 1u << 2,   // runs on the copy queue
	FLUX_PASS_FLAG_NEVER_MERGE    = 1u << 3,   // keeps its own rendering scope
};
inline constexpr Flux_PassFlags Flux_SideEffect = FLUX_PASS_FLAG_SIDE_EFFECT;

class Flux_RenderGraph
{
public:
	Flux_RenderGraph(Zenith_Arena& xArena, const Flux_GraphSettings& xSettings, Flux_GraphCaches& xCaches);

	// Resources
	Flux_RGTexture CreateTexture(const char* szName, const Flux_RGTextureDesc& xDesc);
	Flux_RGBuffer CreateBuffer(const char* szName, const Flux_RGBufferDesc& xDesc);
	Flux_RGTexture ImportTexture(const char* szName, Zenith_RHITextureHandle xTexture);
	Flux_RGBuffer ImportBuffer(const char* szName, Zenith_RHIBufferHandle xBuffer);
	void MarkOutput(Flux_RGTexture xTexture);
	void MarkOutput(Flux_RGBuffer xBuffer);

	// Parameter-struct field values
	template<typename TTag> Zenith_RHISRV<TTag> SRV(Flux_RGTexture xTexture, const Flux_RGTextureRange& xRange = Flux_RGTextureRange::All());
	template<typename TTag> Zenith_RHIUAV<TTag> UAV(Flux_RGTexture& xTexture, uint32 uMip = 0);              // writes the successor version into xTexture
	template<typename TTag> Zenith_RHISRV<TTag> SRV(Flux_RGBuffer xBuffer, const Flux_RGBufferRange& xRange = Flux_RGBufferRange::All());
	template<typename TTag> Zenith_RHIUAV<TTag> UAV(Flux_RGBuffer& xBuffer, const Flux_RGBufferRange& xRange = Flux_RGBufferRange::All());
	Zenith_RHIDescriptorHandle PersistentSRV(const Flux_PersistentReadOnlyTexture& xTexture);
	Zenith_RHIDescriptorHandle PersistentSRV(const Flux_PersistentReadOnlyBuffer& xBuffer);
	template<typename TTag> Zenith_RHIUAV<TTag> UAV(const Flux_PersistentReadOnlyTexture&, uint32 = 0) = delete;
	template<typename TTag> Zenith_RHIUAV<TTag> UAV(const Flux_PersistentReadOnlyBuffer&, const Flux_RGBufferRange& = {}) = delete;

	// Parameters
	template<typename TParams> TParams* AllocParams();   // zero-initialised, graph arena

	// Passes
	template<typename TParams> Flux_RGPass AddComputePass(const char* szName, const TParams* pxParams, const typename TParams::Permutation& xPermutation, Flux_DispatchSize xSize, Flux_PassFlags eFlags = FLUX_PASS_FLAG_NONE);
	template<typename TParams> Flux_RGPass AddComputePassIndirect(const char* szName, const TParams* pxParams, const typename TParams::Permutation& xPermutation, const Flux_IndirectArgsRef& xArgs, Flux_PassFlags eFlags = FLUX_PASS_FLAG_NONE);
	template<typename TParams> Flux_RGPass AddFullscreenPass(const char* szName, const TParams* pxParams, const typename TParams::Permutation& xPermutation, const Flux_RasterTargets& xTargets, Flux_PassFlags eFlags = FLUX_PASS_FLAG_NONE);
	template<typename TParams, typename TLambda> Flux_RGPass AddRasterPass(const char* szName, const TParams* pxParams, const Flux_RasterTargets& xTargets, TLambda&& xLambda, Flux_PassFlags eFlags = FLUX_PASS_FLAG_NONE);
	Flux_RGPass AddClearPass(const char* szName, const Flux_RasterTargets& xTargets);
	Flux_RGPass AddClearBuffer(const char* szName, Flux_RGBuffer& xBuffer, uint32 uValue);   // fills every 32-bit word; a full-coverage first write
	Flux_RGPass AddCopyTexture(const char* szName, Flux_RGTexture xSource, Flux_RGTexture& xDestination);
	Flux_RGPass AddCopyBuffer(const char* szName, Flux_RGBuffer xSource, const Flux_RGBufferRange& xSourceRange, Flux_RGBuffer& xDestination, uint64 ulDestinationOffset);
	Zenith_Result<Flux_ReadbackHandle, Flux_ReadbackError> AddReadback(const char* szName, Flux_RGBuffer xSource, const Flux_RGBufferRange& xRange);

	// Compile and record (called by Flux_Renderer, never by features)
	Zenith_Result<const Flux_CompiledGraph*, Flux_GraphErrorList> Compile();
	const Flux_GraphSettings& GetSettings() const;

	// Diagnostics of the last Compile (graph arena; valid until the graph is destroyed)
	Zenith_Span<const Flux_GraphDiagnostic> GetDiagnostics() const;
	Zenith_Span<const Flux_GraphInfoRecord> GetInfoRecords() const;   // culled, folded, merged, split demotions
	// Frame identity, set by Flux_GraphCaches::BeginFrame before construction
	uint64 GetFrameIndex() const;
	// A view-suffix scope: every pass declared inside it gets "[<suffix>]" appended to its name ([Z2-N-REND-312])
	void PushViewSuffix(Zenith_StringView strSuffix);
	void PopViewSuffix();
};

struct Flux_GraphSettings
{
	bool m_bAliasTransients = true;        // runtime toggle; applies at the next compile
	bool m_bPoisonTransients = false;      // development builds: fill transients before their first write
	bool m_bAsyncCompute = true;           // honour FLUX_PASS_FLAG_ASYNC_COMPUTE where a queue exists
	bool m_bSplitBarriers = true;
	bool m_bMergePasses = true;
	bool m_bParallelRecord = true;
	bool m_bTimestamps = true;             // development default true; shipping follows the profiler toggle
	uint32 m_uDumpFrames = 0;             // > 0: write the next N compiled graphs as frame dumps
};
```

- [Z2-N-REND-289] The public graph API is exactly the declarations above plus `Flux_RasterTargets`, the reference types of "Pass declaration", the pass contexts of "Pass contexts", and the declarations of the headers `Flux_GraphCaches.h` and `Flux_GraphErrors.h` (`Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md → "Flux_GraphCaches header"`, "Flux_GraphErrors header": the graph caches, `Flux_RGError`, `Flux_RGInfo`, `Flux_RecordCheck`, `Flux_GraphDiagnostic`, `Flux_GraphInfoRecord`, `Flux_GraphErrorList`); features reach the RHI through nothing else.
- [Z2-N-REND-290] `Flux_GraphSettings` values are read once per frame at the start of `Compile()`, so a toggle applies at the next compile and never part-way through a frame; the on-screen debug UI and the console (`r.graph.aliasTransients`, `r.graph.poisonTransients`, `r.graph.asyncCompute`, `r.graph.splitBarriers`, `r.graph.mergePasses`, `r.graph.dump`) change them.
- [Z2-N-REND-291] `Compile()` returns every error the graph found, not the first; `Flux_Renderer` treats a compile error as a programmer error per `Zenith2/Docs/Contracts.md → "Errors and results"`, while graph unit tests call `Compile()` directly and assert on the returned error codes.

## Resource declaration and versioning

| Phase | Who | What happens to a parameter-struct resource field |
|---|---|---|
| Setup | the feature function | fills the field with a virtual handle from `SRV()`/`UAV()` (resource index, version, view) |
| Compile | the graph | walks the struct's generated field table (`Zenith_RHIParamFieldTable<TParams>::xFIELDS`) and derives, per field, the resource, version, subresource range, access, stages and coverage |
| Record | the graph | patches the field to `{heap index, 0}` with the descriptor `Flux.BuildGraph` wrote for the view after compile, copies the struct into the upload ring, and pushes its location |

Versioning rules:

| Case | Result |
|---|---|
| a read names version v | the pass depends on the pass that produced v (read-after-write edge) |
| a write names the latest version v and produces v+1 | the pass depends on the producer of v (write-after-write) and on every reader of v (write-after-read) |
| `UAV()`, a colour attachment or a writable depth attachment is declared | the handle passed by reference is advanced to the successor version in place |
| a read-write access (`READ_WRITE` UAV, blended colour attachment, `LOAD` load op) | reads v and produces v+1 |
| a write names a version older than the latest | `FLUX_RG_ERROR_STALE_WRITE` (a forked history) |
| a read of a version that is older than the latest | allowed; the reader is ordered before the writer of the next version, and a cycle this creates is `FLUX_RG_ERROR_CYCLE` naming the passes |
| a read of version 0 of a transient | `FLUX_RG_ERROR_READ_BEFORE_WRITE` |
| version 0 of an imported resource | the contents the resource held at the end of the previous frame |

- [Z2-N-REND-292] A resource is declared exactly once per pass, through the parameter-struct field (or the `Flux_RasterTargets` entry) that the pass's shader or fixed-function stage uses; there is no separate `Reads`/`Writes` declaration list (Z2-D-055).
- [Z2-N-REND-293] Every write produces a new version and returns it through the handle the write was declared with, and every dependency the graph orders is derived from these versions as the versioning table above specifies; no ordering comes from declaration order.
- [Z2-N-REND-294] An imported resource's state and owning queue at the end of a frame are recorded in the graph caches' imported-state table, keyed by the RHI handle, and the next frame's first access transitions from that recorded state; importing a resource the table does not know starts from the state its creator registered.
- [Z2-N-REND-295] Every resource that must survive the frame (swapchain image, histories, pools, GPU scene tables, readback sources) is imported, and every imported resource that the frame writes and a subsequent frame reads is marked with `MarkOutput`.

## Pass declaration

### Pass kinds

| Kind | Declared by | Body | Queue |
|---|---|---|---|
| compute | `AddComputePass` | none: the graph binds the program permutation, writes the params, dispatches `xSize` | graphics, or async compute with `FLUX_PASS_FLAG_ASYNC_COMPUTE` |
| indirect compute | `AddComputePassIndirect` | none: as above with `vkCmdDispatchIndirect`/`ExecuteIndirect(dispatch)` | as above |
| fullscreen | `AddFullscreenPass` | none: one full-screen triangle with the program's vertex entry point | graphics |
| raster | `AddRasterPass` | the lambda, receiving a `Flux_RasterContext` | graphics |
| clear | `AddClearPass` | none: clears the declared attachments | graphics (folded when possible) |
| copy | `AddCopyTexture`, `AddCopyBuffer` | none | graphics, or copy with `FLUX_PASS_FLAG_COPY_QUEUE` |
| readback | `AddReadback` | none: a copy into a host-visible readback buffer | graphics or copy |

### Flux_RasterTargets

```cpp
enum Flux_LoadOp : uint8 { FLUX_LOAD_OP_LOAD, FLUX_LOAD_OP_CLEAR, FLUX_LOAD_OP_DONT_CARE };
enum Flux_StoreOp : uint8 { FLUX_STORE_OP_STORE, FLUX_STORE_OP_DONT_CARE };
enum Flux_DepthAccess : uint8 { FLUX_DEPTH_ACCESS_READ_ONLY, FLUX_DEPTH_ACCESS_WRITE };

struct Flux_VertexStreamRef
{
	Flux_RGBuffer m_xGraphBuffer;                  // pools written this frame (skinned, particle, procedural)
	Flux_PersistentReadOnlyBuffer m_xPersistent;   // unified mesh pools; exactly one of the two is set
	uint64 m_ulOffset;
	uint32 m_uStream;                             // Slang [Stream(n)] index
};

struct Flux_IndexBufferRef
{
	Flux_RGBuffer m_xGraphBuffer;
	Flux_PersistentReadOnlyBuffer m_xPersistent;   // exactly one of the two is set; indices are 16-bit
	uint64 m_ulOffset;
};

struct Flux_IndirectArgsRef
{
	Flux_RGBuffer m_xBuffer;                       // always a graph buffer (culling writes it this frame)
	uint64 m_ulOffset;
	uint32 m_uMaxCommands;
	uint32 m_uStride;                             // 24 for draws (Flux_IndirectDrawIndexed), 12 for dispatches
};

class Flux_RasterTargets   // CPU-only value type; never reaches a shader
{
public:
	Flux_RasterTargets& Colour(uint32 uSlot, Flux_RGTexture& xTexture, Flux_LoadOp eLoad, Flux_StoreOp eStore, const Flux_ClearValue& xClear = {}, uint32 uMip = 0, uint32 uLayer = 0);
	Flux_RasterTargets& Depth(Flux_RGTexture& xTexture, Flux_DepthAccess eAccess, Flux_LoadOp eLoad, Flux_StoreOp eStore, const Flux_ClearValue& xClear = {}, uint32 uLayer = 0);
	Flux_RasterTargets& Resolve(uint32 uSlot, Flux_RGTexture& xTexture);      // rejected: no path uses MSAA
	Flux_RasterTargets& VertexStreams(Zenith_Span<const Flux_VertexStreamRef> axStreams);
	Flux_RasterTargets& IndexBuffer(const Flux_IndexBufferRef& xIndices);
	Flux_RasterTargets& IndirectArgs(const Flux_IndirectArgsRef& xArgs, const Flux_IndirectArgsRef& xCount);   // with a count: DrawIndexedIndirectCount, DrawMeshTasksIndirectCount
	Flux_RasterTargets& IndirectArgs(const Flux_IndirectArgsRef& xArgs);                                       // count-less: DrawIndexedIndirect (the transparent commands)
	Flux_RasterTargets& Programs(Zenith_Span<const Zenith_RHIShaderProgramID> axPrograms);   // the declared shader set
	Flux_RasterTargets& ExpectedDraws(uint32 uDrawSets);                             // 0: nothing to draw this frame; also the bound on BeginDrawParams calls
	Flux_RasterTargets& RenderArea(const Flux_Rect& xArea);                            // default: the full attachment extent
	Flux_RasterTargets& FullCoverage();                                                 // DONT_CARE attachments are fully overwritten
};
```

- [Z2-N-REND-296] Render targets, load/store ops, the vertex, index and indirect-argument buffers a raster pass draws from, and its declared shader set live in its `Flux_RasterTargets`, so the compute-written skinned pool, the particle pool and the culling-produced indirect arguments get their vertex-input, index-input and indirect-read barriers synthesised like any other access.
- [Z2-N-REND-297] A raster lambda can bind only the vertex, index and indirect-argument buffers its `Flux_RasterTargets` declared (it refers to them by declared index), and can bind only programs in its declared shader set; `DrawIndexedIndirectCount` and `DrawMeshTasksIndirectCount` use arguments declared with a count (`IndirectArgs(xArgs, xCount)`) and `DrawIndexedIndirect` uses arguments declared without one (`IndirectArgs(xArgs)`), and a call against the other form fails the frame in development builds with `FLUX_RECORD_CHECK_UNDECLARED_RESOURCE`.
- [Z2-N-REND-298] Every `Flux_RasterTargets` of one pass names attachments of a single extent, a render area inside that extent, and a sample count of one; `Resolve()` is rejected at compile with `FLUX_RG_ERROR_MSAA_UNSUPPORTED` because no path uses MSAA (Z2-D-035).
- [Z2-N-REND-299] A pass flagged `Flux_SideEffect`, and every pass on the dependency path to a resource marked with `MarkOutput`, is never culled; the present pass, readbacks, the frame's final writes to histories and pools, and debug-capture passes are the side-effect passes of the engine.

## Pass contexts

```cpp
class Flux_RasterContext
{
public:
	bool BindPipeline(Zenith_RHIShaderProgramID xProgram, const Zenith_RHIPermutationKey& xPermutation, const Flux_RasterState& xRaster);
	template<typename TParams> TParams& BeginDrawParams();   // a copy of the pass's patched params in the upload ring
	void BindVertexStreams(uint32 uFirstDeclared, uint32 uCount);
	void BindIndexBuffer(uint32 uDeclared);
	void Draw(uint32 uVertexCount, uint32 uInstanceCount, uint32 uFirstVertex);
	void DrawIndexed(uint32 uIndexCount, uint32 uInstanceCount, uint32 uFirstIndex, int32 iVertexOffset);
	void DrawIndexedIndirect(uint32 uFirstCommand, uint32 uCommandCount);   // no count buffer: the non-compacted transparent commands
	void DrawIndexedIndirectCount(uint32 uFirstCommand, uint32 uMaxCommands, uint32 uCountIndex);
	void DrawMeshTasksIndirectCount(uint32 uFirstCommand, uint32 uMaxCommands, uint32 uCountIndex);
	void SetScissor(const Flux_Rect& xRect);
	void SetStencilReference(uint8 uReference);
	const Flux_View& GetView() const;
};

class Flux_ComputeContext   // used by graph-internal passes and by AddComputePass's generated body
{
public:
	bool BindPipeline(Zenith_RHIShaderProgramID xProgram, const Zenith_RHIPermutationKey& xPermutation);
	template<typename TParams> TParams& BeginDispatchParams();
	void Dispatch(uint32 uGroupsX, uint32 uGroupsY, uint32 uGroupsZ);
	void DispatchIndirect(uint32 uCommandIndex);
};

// Raster lambda signature: void(Flux_RasterContext& xContext). It is stored as Zenith_Function<void(Flux_RasterContext&)> in the graph arena;
// the pass's patched parameter struct reaches the lambda through xContext.BeginDrawParams<TParams>(), never as an argument.
```

<!-- elaboration -->
| Context operation | Why it is a draw, dispatch or parameter operation |
|---|---|
| `BindPipeline` | a draw needs its PSO: an indirect pass draws several material buckets, each with its own (program, permutation, raster state); the attachment formats are fixed by the pass, so this selects a pipeline and changes no resource state |
| `BeginDrawParams`, `BeginDispatchParams` | the parameter operation: a per-draw or per-dispatch copy of the patched parameter struct |
| `BindVertexStreams`, `BindIndexBuffer` | a draw's inputs; they accept declared indices alone, because buckets of different stream-sets draw from different unified pools |
| `Draw`, `DrawIndexed`, `DrawIndexedIndirect`, `DrawIndexedIndirectCount`, `DrawMeshTasksIndirectCount`, `Dispatch`, `DispatchIndirect` | draws and dispatches |
| `SetScissor` | per-draw clipping of UI and debug batches inside the declared render area; it never widens the render area |
| `SetStencilReference` | the reference value of the stencil-masking draws that the kept S8 aspect exists for (decal masking, outlines) |
| `GetView` | read-only view data a lambda needs to fill plain-data parameter fields (UI projection, per-draw sort distances) |

- [Z2-N-REND-300] The pass contexts expose exactly the operations of the table above: no command list, no barrier, layout, queue-ownership or aliasing operation, no descriptor write, no resource creation or destruction, no render-area or attachment change, no queue or submission control, and no access to any resource the pass did not declare (Z2-D-056).
- [Z2-N-REND-301] A raster lambda has the signature `void(Flux_RasterContext& xContext)`; `xContext.BeginDrawParams<TParams>()` returns a copy of the pass's patched parameter struct in the upload ring, the lambda edits plain-data fields of it (for example `m_uBucketRecordBase`), and the next draw call pushes its location; in development builds the context verifies before each push that every resource field still equals the pass's patched value, failing the frame with `FLUX_RECORD_CHECK_UNDECLARED_RESOURCE` otherwise.
- [Z2-N-REND-302] Draw calls issued after a `BindPipeline` that returned `false` are dropped by the context until the next successful `BindPipeline`, which implements the missing-PSO fallback without any branch in feature code (see `Zenith2/Docs/Rendering/ShaderSystem.md` "Missing-PSO fallback").

## Graph compile

`Compile()` runs seven steps, in this order, over the declared passes and resources.

### Step 1 cull unused passes

| Sub-step | Action |
|---|---|
| 1a | A raster pass with `ExpectedDraws(0)` and no side-effect flag becomes a clear-only pass if any attachment uses `FLUX_LOAD_OP_CLEAR`; otherwise it is removed and each version it would have produced becomes an alias of the version it read (unchanged content). |
| 1b | Roots are the side-effect passes and the passes that produce the final version of each `MarkOutput` resource. |
| 1c | Walk backwards from the roots: a pass is live if it produces a version a live pass reads, or a version a live pass writes over with `LOAD`/read-write semantics. |
| 1d | Every non-live pass is culled and reported in the frame dump with its reason (`noConsumer`, `noDraws`). |
| 1e | Each clear-only pass whose next user of the cleared version is a raster pass using it as an attachment with `FLUX_LOAD_OP_LOAD` is folded into that user: the user's load op becomes `CLEAR` with the clear value; otherwise the clear-only pass stays as a standalone clear. |

### Step 2 topological order

- [Z2-N-REND-303] The pass order is a topological sort by `ZenithGraph` over the read-after-write, write-after-read and write-after-write edges derived from versions, with ties broken first by descending critical-path cost (each pass's cost is its static cost hint: dispatch group count, expected draw count, or copy bytes / 64 KiB, never a measured time) and then by ascending declaration index, so the order is deterministic for the same setup.
- [Z2-N-REND-304] A dependency cycle is `FLUX_RG_ERROR_CYCLE`, and its message names every pass on the cycle and the resource version on each edge.

### Step 3 lifetimes indexed by queue and position

| Sub-step | Action |
|---|---|
| 3a | Provisional queue: a pass flagged `FLUX_PASS_FLAG_ASYNC_COMPUTE` goes to the async-compute queue when `m_bAsyncCompute` is set, the device has an async-compute queue, and at least one graphics pass lies in the topological order strictly between the pass's latest producer and its earliest consumer (the gap rule); otherwise it runs on the graphics queue. Copy-flagged passes go to the copy queue when one exists. |
| 3b | Each pass gets a position within its queue's sub-order and a global index in the topological order. |
| 3c | Each resource's lifetime is the set of (queue, position) pairs of its live accesses, plus a conservative global interval [first global index, last global index]. |
| 3d | A reachability bitset per pass (which passes it is ordered after, through graph edges and same-queue order) is computed for the placement step. |

### Step 4 memory placement

The placement algorithm is "Aliasing placement" below; with `m_bAliasTransients` false, every transient gets its own dedicated cached allocation.

### Step 5 automatic barrier synthesis

The synthesis algorithm is "Barrier state machine" and "Barrier emission" below.

### Step 6 queue assignment

| Sub-step | Action |
|---|---|
| 6a | The provisional queues of step 3 become final; step 6 never moves a pass to another queue. |
| 6b | Each queue's passes, in its sub-order, are cut into submission batches at every cross-queue dependency. |
| 6c | Each cross-queue edge becomes a timeline signal (producer queue, value) at the end of the producer's batch and a timeline wait at the start of the consumer's batch; redundant waits (implied by a subsequent wait on the same queue value) are removed. |

### Step 7 partitioning into command lists

- [Z2-N-REND-305] Each queue's ordered passes are partitioned into command lists of contiguous passes whose summed cost hint is at most `uFLUX_RG_PARTITION_TARGET_COST` = 4,096 cost units, never splitting a merged rendering scope or a submission batch; each partition is recorded by its own job when `m_bParallelRecord` is set, and partitions are submitted in order; command lists are recorded by the `Flux.Record` job nodes (`JOB_AFFINITY_ANY`, on the frame threads; see `Zenith2/Docs/Architecture.md → "CPU frame graph and scheduler"`), and the render-submit thread runs `Flux.Submit` and `Flux.Present` alone, recording no command list.
- [Z2-N-REND-306] Each partition's recording begins by binding the global descriptor set (Vulkan) or the global root signature and descriptor heap (D3D12) for both bind points, then records, for each pass, its batched barriers, its begin timestamp, its body and its end timestamp.

### Compile step obligations

- [Z2-N-REND-307] `Compile()` performs the seven steps above in the order given (cull, topological order, lifetimes, placement, barrier synthesis, queue assignment, partitioning), and each step consumes the previous steps' outputs alone.
- [Z2-N-REND-308] The compile step's CPU time for the frame's graph is measured and reported as `Flux.Graph.CompileMs`, and is budgeted by `Z2-B-RHI-004` Render graph build and compile CPU (pending).

## Pass merging

Consecutive raster passes are merged into one rendering scope (one `vkCmdBeginRendering`/`BeginRenderPass`) when every condition holds:

| # | Condition | Blocked reason in the frame dump |
|---|---|---|
| 1 | `m_bMergePasses` is set and neither pass carries `FLUX_PASS_FLAG_NEVER_MERGE` | `flagged` |
| 2 | both are raster, clear-folded or fullscreen passes on the graphics queue, adjacent in the final order and in the same partition | `notAdjacent` |
| 3 | the attachment sets are identical: the same resources, subresources, formats, depth access mode and render area | `attachmentsDiffer` |
| 4 | load/store is compatible: for every attachment, the first pass stores it and the second loads it (`STORE` then `LOAD`) | `loadStoreIncompatible` |
| 5 | no synthesised barrier falls between them, because dynamic rendering forbids barriers inside a rendering instance (for example, a UAV counter written by the first and read by the second blocks the merge) | `barrierBetween` |

- [Z2-N-REND-309] Passes are merged exactly when every condition of the table above holds; the merged scope uses the first pass's load ops and the last pass's store ops, and each merged pass keeps its own timestamps.
- [Z2-N-REND-310] Pass merging never reads an attachment tile-locally; tile-local-read merging is D-TBDR (open, Stage 5) and is not performed (Z2-D-036).

## GPU timestamps

- [Z2-N-REND-311] Every live pass gets a begin and an end timestamp query automatically (`vkCmdWriteTimestamp2`, `EndQuery(D3D12_QUERY_TYPE_TIMESTAMP)`), written inside merged scopes as well, from a per-queue query pool per frame in flight sized to twice the frame's pass count (at most 8,192 queries per pool); a queue whose timestamp valid bits are 0 reports its passes as "n/a".
- [Z2-N-REND-312] Timestamp results are read after the frame's fence completes, converted with the queue's timestamp period, attributed to the pass name with its view suffix (for example `GTAO[Main]`, `ShadowDepth[Sun.C2]`), and published to the on-screen profiler and Tracy; with `VK_EXT_calibrated_timestamps` (or D3D12 `GetClockCalibration`) the GPU zones are aligned to the CPU timeline.
- [Z2-N-REND-313] In shipping builds timestamps are inserted exclusively while the on-screen profiler is enabled; in development builds they are always inserted.

## Frame dump

The frame dump embeds the core `zenith2.graph-dump` document of one compiled graph verbatim under `"core"` (node and resource IDs, order, lifetimes, culling, structure hash; see `Zenith2/Docs/Architecture.md → "ZenithGraph shared core"`) and adds the GPU front end's data under sibling keys, which refer to core node and resource IDs. It is written for the next `m_uDumpFrames` frames to `Zenith2/out/<checkout-hash>/GraphDumps/frame_<frameIndex>.json` in development builds (the user data directory on Android), on the console command `r.graph.dump <N>`, from the profiler's "Dump graph" button, and automatically as a parity-failure artefact.

```json
{
	"schema": "zenith2.flux-graph-dump",
	"schemaVersion": 1,
	"frontEnd": "gpu",
	"core": { "schema": "zenith2.graph-dump", "schemaVersion": 1, "graph": "Flux/Frame", "frontEnd": "gpu", "frame": 1234, "structureHash": "0x…", "nodes": [ "…one per pass, core fields…" ], "resources": [ "…one per resource, core fields…" ] },
	"settings": { "aliasTransients": true, "poisonTransients": false, "asyncCompute": true, "splitBarriers": true, "mergePasses": true },
	"views": [ { "id": "0x…", "kind": "main", "path": "forwardPlus", "renderExtent": [2560, 1440] } ],
	"passes": [
		{ "node": 21, "name": "ForwardOpaque[Main]", "kind": "raster", "queue": "graphics", "partition": 3, "scope": 7, "cullReason": null,
		  "reads": [ { "resource": 4, "version": 2, "access": "depthReadOnly", "stages": ["earlyFragmentTests"], "range": { "mips": [0, 0], "layers": [0, 0] } } ],
		  "writes": [ { "resource": 17, "version": 1, "access": "colourAttachment", "coverage": "full" } ],
		  "barriersBefore": [ { "resource": 17, "range": "all", "kind": "discard", "from": "undefined", "to": "colourAttachment", "split": false } ],
		  "gpuMs": 2.41 }
	],
	"placement": [
		{ "resource": 17, "name": "SceneColour[Main]", "desc": { "format": "R11G11B10_FLOAT", "extent": [2560, 1440, 1], "mips": 1 }, "usage": ["colourAttachment", "srv", "uav"],
		  "mode": "aliased", "group": 2, "offset": 33554432, "size": 14745600, "sharesWith": [9, 31] }
	],
	"scopes": [ { "id": 7, "passes": [21, 22], "attachments": [17, 4], "blocked": [ { "between": [22, 23], "reason": "barrierBetween" } ] } ],
	"batches": [ { "queue": "asyncCompute", "passes": [14, 15], "waits": [ { "queue": "graphics", "afterPass": 12 } ], "signals": [ { "value": "…" } ] } ],
	"memory": { "transientBytesDedicated": 402653184, "transientBytesAliased": 188743680, "savingsBytes": 213909504, "groups": [ { "id": 2, "heapBytes": 100663296 } ] },
	"errors": []
}
```

- [Z2-N-REND-314] A frame dump contains the core document under `"core"` and every sibling key of the example above (`passes`, `placement`, `scopes`, `batches`, `memory`, `errors`, with `barriersBefore` inside each pass) for every pass, resource, scope and batch of the compiled graph (culled passes included, with their reason), and the graph viewer and the on-screen profiler read nothing but this format.
- [Z2-N-REND-315] Frame dumps are written on a background job after the frame's compile, never delaying its record or submit, and are unavailable in shipping builds.

## Barrier state machine

### Access states

| `Flux_ResourceAccess` | Vulkan stage | Vulkan access | Vulkan image layout | D3D12 sync | D3D12 access | D3D12 texture layout |
|---|---|---|---|---|---|---|
| `SRV_GRAPHICS` (stage from the field) | `VERTEX_SHADER` / `FRAGMENT_SHADER` / `MESH_SHADER_EXT` | `SHADER_SAMPLED_READ` (textures), `SHADER_STORAGE_READ` (buffers) | `SHADER_READ_ONLY_OPTIMAL` | `VERTEX_SHADING` / `PIXEL_SHADING` | `SHADER_RESOURCE` | `SHADER_RESOURCE` |
| `SRV_COMPUTE` | `COMPUTE_SHADER` | as above | `SHADER_READ_ONLY_OPTIMAL` | `COMPUTE_SHADING` | `SHADER_RESOURCE` | `SHADER_RESOURCE` |
| `UAV` (read, write or read-write) | the field's stages | `SHADER_STORAGE_READ` and/or `SHADER_STORAGE_WRITE` | `GENERAL` | the field's stages | `UNORDERED_ACCESS` | `UNORDERED_ACCESS` |
| `COLOUR_ATTACHMENT` | `COLOR_ATTACHMENT_OUTPUT` | `COLOR_ATTACHMENT_WRITE` (+ `COLOR_ATTACHMENT_READ` when blending or `LOAD`) | `COLOR_ATTACHMENT_OPTIMAL` | `RENDER_TARGET` | `RENDER_TARGET` | `RENDER_TARGET` |
| `DEPTH_WRITE` | `EARLY_FRAGMENT_TESTS` + `LATE_FRAGMENT_TESTS` | `DEPTH_STENCIL_ATTACHMENT_READ` + `_WRITE` | `DEPTH_STENCIL_ATTACHMENT_OPTIMAL` | `DEPTH_STENCIL` | `DEPTH_STENCIL_WRITE` | `DEPTH_STENCIL_WRITE` |
| `DEPTH_READ_ONLY` (attachment, optionally also sampled in the same pass) | tests (+ the sampling stage) | `DEPTH_STENCIL_ATTACHMENT_READ` (+ `SHADER_SAMPLED_READ`) | `DEPTH_STENCIL_READ_ONLY_OPTIMAL` | `DEPTH_STENCIL` (+ shading) | `DEPTH_STENCIL_READ` (+ `SHADER_RESOURCE`) | `DEPTH_STENCIL_READ` |
| `VERTEX_BUFFER` | `VERTEX_ATTRIBUTE_INPUT` | `VERTEX_ATTRIBUTE_READ` | — | `VERTEX_SHADING` | `VERTEX_BUFFER` | — |
| `INDEX_BUFFER` | `INDEX_INPUT` | `INDEX_READ` | — | `INDEX_INPUT` | `INDEX_BUFFER` | — |
| `INDIRECT_ARGS` | `DRAW_INDIRECT` | `INDIRECT_COMMAND_READ` | — | `EXECUTE_INDIRECT` | `INDIRECT_ARGUMENT` | — |
| `COPY_SOURCE` | `COPY` | `TRANSFER_READ` | `TRANSFER_SRC_OPTIMAL` | `COPY` | `COPY_SOURCE` | `COPY_SOURCE` |
| `COPY_DEST` | `COPY` | `TRANSFER_WRITE` | `TRANSFER_DST_OPTIMAL` | `COPY` | `COPY_DEST` | `COPY_DEST` |
| `CLEAR` (standalone clears) | `CLEAR` | `TRANSFER_WRITE` | `TRANSFER_DST_OPTIMAL` | `RENDER_TARGET` / `DEPTH_STENCIL` / `CLEAR_UNORDERED_ACCESS_VIEW` | matching | matching |
| `PRESENT` | `NONE` | 0 | `PRESENT_SRC_KHR` | `NONE` | `NO_ACCESS` | `PRESENT` |
| `UNDEFINED` (first use of a transient) | `NONE` | 0 | `UNDEFINED` | `NONE` | `NO_ACCESS` | `UNDEFINED` + `D3D12_TEXTURE_BARRIER_FLAG_DISCARD` |
| `PERSISTENT_READ` (steady state of `Flux_PersistentReadOnly`) | all shader stages | `SHADER_SAMPLED_READ`, `SHADER_STORAGE_READ`, `VERTEX_ATTRIBUTE_READ`, `INDEX_READ` | `SHADER_READ_ONLY_OPTIMAL` | all shading | `SHADER_RESOURCE`, `VERTEX_BUFFER`, `INDEX_BUFFER` | `GENERIC_READ` |

### Tracking

```cpp
struct Flux_SubresourceState   // ZenithFlux/Internal/Graph; one per tracked subresource run
{
	Flux_ResourceAccess m_eAccess;       // the access class of the most recent write, or of the accumulated reads
	uint32 m_uStageMask;                // Flux pipeline-stage bits accumulated since the last write
	uint32 m_uAccessMask;               // Flux access bits accumulated since the last write
	Flux_ImageLayout m_eLayout;          // textures
	Flux_QueueClass m_eQueue;
	uint32 m_uLastWritePass;
	bool m_bLastWasWrite;
};
```

Textures are tracked per (mip, array layer), stored as run-length ranges over the index `mip × layerCount + layer`; depth and stencil aspects are tracked together. Buffers are tracked per declared range, with the whole buffer as one range unless a pass declares a sub-range.

| Transition | Barrier emitted |
|---|---|
| read after read, same layout, same queue | none; stages and accesses are accumulated |
| read after read, different layout | layout transition; source = the accumulated read stages (execution dependency) |
| read after write | memory barrier; source = the write's stages and accesses; destination = the read; layout transition if the layout differs |
| write after read | execution dependency from the accumulated read stages; layout transition if needed |
| write after write (including UAV after UAV) | memory barrier from the earlier write to the subsequent write |
| first use of a transient | discard from `UNDEFINED`; source = every earlier occupant of overlapping memory's last-use stages (the aliasing barrier) |
| access on a different queue | queue transfer (see "Barrier emission"), plus the transition the row above would emit |

- [Z2-N-REND-385] For each subresource, the barrier synthesiser tracks the state through the pass order with `Flux_SubresourceState`, merges compatible reads without a barrier, and emits exactly the barrier the transition table above specifies for every other transition (Z2-D-056).
- [Z2-N-REND-386] Two consecutive UAV accesses to the same subresource in different passes are always separated by a memory barrier; the graph has no "UAV overlap allowed" escape.
- [Z2-N-REND-387] Every access maps to the backend-neutral stage, access and layout vocabulary of `ZenithRHI/Zenith_RHIBarrier.h` through one function in `ZenithFlux/Internal/Graph/`, `Flux_ToRHIAccess`, and each RHI backend maps the neutral values onto its API inside its own `Internal/` (`Zenith_Vk_ToStage`, `Zenith_Vk_ToAccess`, `Zenith_Vk_ToLayout`; `Zenith_D3D12_ToSync`, `Zenith_D3D12_ToAccess`, `Zenith_D3D12_ToLayout`), producing exactly the API values of the access-state table; Flux includes no Vulkan or D3D12 header (see `Zenith2/Docs/Contracts.md → "API boundary"`).
- [Z2-N-REND-388] `Flux_PersistentReadOnly` resources are not tracked per frame: they are transitioned once, after their initial upload, to `PERSISTENT_READ` (Vulkan images with `VK_SHARING_MODE_CONCURRENT` across the graphics, async-compute and copy families, so the copy-queue fill needs no ownership transfer; D3D12 `GENERIC_READ`), and the synthesiser emits no barrier for their accesses.

## Barrier emission

### Batching

- [Z2-N-REND-389] Each pass's barriers are batched into one `vkCmdPipelineBarrier2` call (a `VkDependencyInfo` holding every image, buffer and memory barrier of the pass) or one `ID3D12GraphicsCommandList7::Barrier` call (every texture, buffer and global barrier group), placed before the pass and outside any rendering scope.
- [Z2-N-REND-390] Buffer barriers that involve no queue transfer are collapsed into a single global memory barrier per batch on both APIs.

### Cross-queue transfers

| Resource | Vulkan | D3D12 |
|---|---|---|
| graph buffers | created with `VK_SHARING_MODE_CONCURRENT` across the graphics, async-compute and copy families; no ownership transfer, the timeline wait plus a memory barrier on the consumer queue | a timeline wait plus a buffer barrier on the consumer queue |
| graph textures | `VK_SHARING_MODE_EXCLUSIVE`; a release barrier on the producer queue after its last access and an acquire barrier on the consumer queue before its first access, both naming the source and destination families, with the layout transition performed by the pair | the layout transition is placed on the producer queue before the signal when the producer queue supports both layouts, otherwise on the consumer queue; compute-queue accesses use `SHADER_RESOURCE`, `UNORDERED_ACCESS`, `COMMON` or `GENERIC_READ` layouts |

- [Z2-N-REND-391] A texture accessed on more than one queue in a frame gets a queue-family release/acquire pair on Vulkan, and a D3D12 layout transition placed on a queue type that supports both layouts, at each queue boundary, together with the timeline wait of step 6.
- [Z2-N-REND-392] Queue order alone is never treated as ordering memory access: every cross-queue dependency carries a timeline wait and the matching barrier.

### Split barriers

- [Z2-N-REND-393] When `m_bSplitBarriers` is set and a transition's producer and consumer are on the same queue with at least `uFLUX_RG_SPLIT_BARRIER_MIN_GAP` = 2 passes between them, the barrier is emitted as a split candidate (compile step 5 runs before queue assignment and partitioning, so it cannot know where the halves land): the begin half is placed after the producer (after its rendering scope ends) and the end half before the consumer, as `vkCmdSetEvent2`/`vkCmdWaitEvents2` with the same `VkDependencyInfo` on Vulkan and as a `D3D12_BARRIER_SYNC_SPLIT` begin/end pair on D3D12.
- [Z2-N-REND-394] Split-barrier halves are never placed inside a rendering scope, and a split candidate whose halves would fall in different submission batches (step 6) or in different command-list partitions (step 7, because a D3D12 split barrier ends in the command list that began it) is emitted as an ordinary barrier before the consumer instead, through `Flux_DemoteSplit`, and each such demotion is recorded as `FLUX_RG_INFO_SPLIT_DEMOTED`.

### Barrier validation

- [Z2-N-REND-395] Every graph unit test and every render test runs with the Khronos validation layer's synchronization validation (`VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT`) on Vulkan and GPU-based validation (`ID3D12Debug1::SetEnableGPUBasedValidation`) plus the debug layer on D3D12, and fails on any error or warning message from either.
- [Z2-N-REND-396] A validation-message suppression exists only in `Zenith2/Engine/ZenithFlux/Tests/ValidationSuppressions.json` with a `Z2-D-*` decision ID per entry, matching one message ID each.
- [Z2-N-REND-397] In development builds and every graph test, `Flux_BarrierVerifier` re-derives each subresource's state independently from the compiled barrier list and asserts before every access that the subresource is in a state compatible with it, so a synthesiser defect fails a test even where a validation layer is silent.

## Aliasing placement

| Step | Action |
|---|---|
| 1 | Collect every live transient (created by the graph, not imported) with its lifetime from compile step 3. |
| 2 | Query each transient's memory requirements (size, alignment, memory type bits) without creating it: `vkGetDeviceImageMemoryRequirements`/`vkGetDeviceBufferMemoryRequirements`, `ID3D12Device::GetResourceAllocationInfo2`. |
| 3 | Group transients by heap and memory type: on Vulkan by (memory type index, linear or optimal tiling), which keeps `bufferImageGranularity` out of the problem; on D3D12 by heap category (buffers, render-target and depth-stencil textures, other textures), on every heap tier. |
| 4 | Within each group, sort by size descending, then first use ascending, then resource index ascending. |
| 5 | Place each transient first-fit at the lowest aligned offset that overlaps no already-placed transient it may not share memory with (pseudocode below). |
| 6 | The group's heap size is the maximum placed end offset, rounded up to 4 MiB. |
| 7 | Realise: reuse the group's cached heap if it is large enough, otherwise retire it through deferred deletion and allocate a new one; create or reuse each placed resource from the placed-resource cache (key: description hash, usage union, heap, offset). |

```
may_share(a, b) = every use of a is ordered before every use of b, or every use of b before every use of a,
                  by the compiled order (same-queue position, or a graph-edge path through a timeline wait: the reachability bitsets of step 3d)

place(group):
	sort group by (size desc, firstUse asc, index asc)
	placed = []
	for t in group:
		blockers = [p in placed where not may_share(t, p)], sorted by offset asc
		offset = 0
		for p in blockers:
			offset = align_up(offset, t.alignment)
			if offset + t.size <= p.offset: break
			offset = max(offset, p.offset + p.size)
		t.offset = align_up(offset, t.alignment)
		placed.append(t)
	heapSize = align_up(max(p.offset + p.size for p in placed), 4 MiB)
```

Complexity is O(n²) in the group's transient count n (a few hundred per frame), dominated by the `may_share` tests, each O(1) on the bitsets.

- [Z2-N-REND-435] With aliasing on, transients are grouped per heap and memory type, sorted largest first, and placed first-fit into free lifetime intervals at arbitrary offsets across formats, exactly as the steps and pseudocode above specify (VMA aliasing through `vmaCreateAliasingImage2`/`vmaCreateAliasingBuffer2` on Vulkan, D3D12MA `CreateAliasingResource1` placed resources on D3D12) (Z2-D-057).
- [Z2-N-REND-436] Two transients share memory only when `may_share` holds, so memory reuse across queues is always ordered by a timeline wait, never by queue timing.
- [Z2-N-REND-437] A group heap grows the frame its requirement exceeds its size and shrinks, by reallocation, after its requirement has stayed below half its size for `uFLUX_RG_HEAP_SHRINK_FRAMES` = 300 consecutive frames; a retired heap and its placed resources are freed through deferred deletion.
- [Z2-N-REND-438] Each resource's first use in the frame is preceded by a discard (`UNDEFINED` transition on Vulkan, `D3D12_TEXTURE_BARRIER_FLAG_DISCARD` with `LAYOUT_UNDEFINED` on D3D12), synthesised automatically, whose source scope covers the last use of every earlier occupant of any overlapping memory.

## Aliasing toggle and first-write rule

### The toggle

- [Z2-N-REND-439] `Flux_GraphSettings::m_bAliasTransients` switches placement at the next graph compile: off gives each transient its own dedicated allocation from the dedicated-resource cache (keyed by description hash and usage union, so the allocation is reused frame to frame), on resumes aliased placement; nothing else in the graph changes, and no restart is needed.
- [Z2-N-REND-440] With aliasing off, each transient still receives the same first-use discard as with aliasing on, so the two modes present identical contents to every pass.

### The first-write rule

Fresh GPU memory is undefined even in a dedicated allocation, and masking does not help (`0 × NaN = NaN`), so the first access to every transient subresource is a clear or a declared full-coverage write.

| First access | Counts as full coverage when |
|---|---|
| colour or depth attachment | the load op is `FLUX_LOAD_OP_CLEAR` and the render area is the whole attachment extent; or the load op is `FLUX_LOAD_OP_DONT_CARE` and the pass declared `FullCoverage()` (fullscreen passes declare it implicitly) |
| texture UAV | the field is `[FullCoverage]` and names the whole mip |
| buffer UAV | the field is `[FullCoverage]` and names the whole buffer or the whole declared range that subsequent reads cover |
| copy destination | the copy covers the whole destination subresource or range |
| clear pass (`AddClearPass`, `AddClearBuffer`) | always |

- [Z2-N-REND-441] A transient's first access in the frame is a clear or a declared full-coverage write as the table above defines; a first access that is a write without full coverage is `FLUX_RG_ERROR_PARTIAL_FIRST_WRITE`, and a read (including a `LOAD` load op or a read-write UAV) of any subresource region not yet written is `FLUX_RG_ERROR_READ_BEFORE_WRITE`, both graph compile errors naming the pass, the resource and the subresource.
- [Z2-N-REND-442] In development builds with `m_bPoisonTransients` set, the graph inserts, after each transient's discard and before its first write, a fill of every subresource with the poison value: quiet NaN for float, half and packed-float formats (`0x7FC00000`, `0x7E00`, and each R11G11B10 channel's NaN encoding), `(1, 0, 1, 0.5)` for UNORM and SNORM formats, all-ones for integer formats and 0xFFFFFFFF words for buffers; a shader that claims full coverage without achieving it, or any undefined read the compile could not see, then shows up as NaN or magenta.
- [Z2-N-REND-443] Imported resources are exempt from the first-write rule, because version 0 of an imported resource is its previous frame's content; an imported resource whose previous content is invalid (a history after a camera cut) is read by consumers that treat the history as invalid through `m_bHistoryValid`, never by reading undefined memory.

### Both modes are first-class

- [Z2-N-REND-444] The parity suite renders every parity scene with aliasing on and with aliasing off, poisoning enabled in both, and the images are bit-identical and synchronization validation is clean in both modes (`Test_AliasingParity_<Scene>`); toggling aliasing is the first diagnostic step for any suspected aliasing defect.
- [Z2-N-REND-445] The on-screen profiler shows transient memory with aliasing (the summed group heap sizes), without aliasing (the summed transient sizes), the savings in bytes and percent, per group and in total, and the peak over the last 300 frames; the same values appear in the frame dump's `memory` object.

## Graph errors

| Code | Raised when |
|---|---|
| `FLUX_RG_ERROR_INVALID_HANDLE` | a handle names no resource of this graph, or a pass of another frame's graph (core `GRAPH_ERROR_INVALID_HANDLE`) |
| `FLUX_RG_ERROR_SUBRANGE_OUT_OF_BOUNDS` | a view names a mip, layer or byte range outside its resource (core `GRAPH_ERROR_SUBRANGE_OUT_OF_BOUNDS`) |
| `FLUX_RG_ERROR_SELF_ORDERING` | a pass would be ordered against itself, for example reading and writing one subresource through two fields (core `GRAPH_ERROR_SELF_ORDERING`) |
| `FLUX_RG_ERROR_CAPACITY` | the frame's graph exceeds a node, resource or edge capacity of the core (core `GRAPH_ERROR_CAPACITY`) |
| `FLUX_RG_ERROR_FIELD_UNSET` | a non-`[Optional]` resource field of a parameter struct holds no handle at compile |
| `FLUX_RG_ERROR_VIEW_TYPE_MISMATCH` | a field's tag type disagrees with the view's dimension or element stride |
| `FLUX_RG_ERROR_STALE_WRITE` | a write names a version older than the resource's latest |
| `FLUX_RG_ERROR_CYCLE` | the version edges form a cycle |
| `FLUX_RG_ERROR_READ_BEFORE_WRITE` | a region of a transient is read before any write covers it |
| `FLUX_RG_ERROR_PARTIAL_FIRST_WRITE` | a transient's first access is a write without full coverage |
| `FLUX_RG_ERROR_PERSISTENT_WRITE` | a `Flux_PersistentReadOnly` resource's RHI handle is imported writable, bound as an attachment, or reached through a UAV in the same frame |
| `FLUX_RG_ERROR_UNDECLARED_BUFFER` | a raster lambda refers to a vertex, index or indirect buffer index its targets did not declare |
| `FLUX_RG_ERROR_UNWRITTEN_ATTACHMENT` | a declared program's reflected outputs leave a bound colour attachment unwritten without a zero write mask |
| `FLUX_RG_ERROR_DEPTH_WRITABLE_UNUSED` | a record-time check (development builds and every graph and render test), the one graph error code raised at record rather than at compile: a pass that declared depth `FLUX_DEPTH_ACCESS_WRITE`, issued at least one draw and bound no PSO with depth write enabled fails the frame ([Z2-N-REND-925]) |
| `FLUX_RG_ERROR_D32_UNJUSTIFIED` | a D32F depth target belongs to a view without a precision or capability reason |
| `FLUX_RG_ERROR_32BIT_TARGET_UNJUSTIFIED` | a 32-bit-per-channel render target is not on the justified list |
| `FLUX_RG_ERROR_MSAA_UNSUPPORTED` | a resolve or a sample count above one is declared |
| `FLUX_RG_ERROR_TILE_LOCAL_UNSUPPORTED` | a tile-local access is declared until D-TBDR (open, Stage 5) is ruled |
| `FLUX_RG_ERROR_EXTENT_MISMATCH` | the attachments of one pass differ in extent |
| `FLUX_RG_ERROR_SWAPCHAIN_QUEUE` | a pass on a queue other than graphics accesses the swapchain image (acquire and present live on the graphics queue) |
| `FLUX_RG_ERROR_FORMAT_FEATURE_MISSING` | a declared attachment, blend, storage write or typed load uses a format the "Required format features" table of the entry file does not grant that feature |
| `FLUX_RG_ERROR_TABLE_WRITE` | a pass other than the GPU-scene scatter or growth pass declares a write on a write-restricted import (a GPU scene table) |

- [Z2-N-REND-446] Each graph error in the table above is raised under exactly the stated condition, carries the pass name, the resource name, the version and the subresource range where they apply, and is covered by a failing-fixture graph unit test named `Test_RenderGraph_Error_<Code suffix>` (for example `Test_RenderGraph_Error_ReadBeforeWrite`); each core diagnostic reaches the caller as the Flux code the table "Core error mapping" below assigns, with the core text unchanged inside it.

### Core error mapping

The graph compile hands its declarations to `ZenithGraph`, marking every transient `GRAPH_RESOURCE_FLAG_REQUIRE_DEFINED_READS | GRAPH_RESOURCE_FLAG_FIRST_WRITE_DISCARD_PER_SUBRESOURCE`, each first write declared with its real mode over its real range (`Zenith2/Docs/Architecture/ZenithGraph.md` [Z2-N-ARCH-346]), and wraps each core diagnostic, carrying its text unchanged, as the table "Error codes by front end" of `Zenith2/Docs/Architecture.md → "ZenithGraph shared core"` states:

| Core code | Flux code |
|---|---|
| `GRAPH_ERROR_CYCLE` | `FLUX_RG_ERROR_CYCLE` |
| `GRAPH_ERROR_VERSION_FORK` | `FLUX_RG_ERROR_STALE_WRITE` |
| `GRAPH_ERROR_UNDEFINED_READ` | `FLUX_RG_ERROR_READ_BEFORE_WRITE` |
| `GRAPH_ERROR_FIRST_WRITE_NOT_DISCARD` | `FLUX_RG_ERROR_READ_BEFORE_WRITE` |
| `GRAPH_ERROR_INVALID_HANDLE` | `FLUX_RG_ERROR_INVALID_HANDLE` |
| `GRAPH_ERROR_SUBRANGE_OUT_OF_BOUNDS` | `FLUX_RG_ERROR_SUBRANGE_OUT_OF_BOUNDS` |
| `GRAPH_ERROR_SELF_ORDERING` | `FLUX_RG_ERROR_SELF_ORDERING` |
| `GRAPH_ERROR_CAPACITY` | `FLUX_RG_ERROR_CAPACITY` |

Every other code of the table "Graph errors" is a Flux front-end diagnosis made before the core runs; `FLUX_RG_ERROR_PARTIAL_FIRST_WRITE` in particular is Flux's earlier, more specific diagnosis of a first access that is a write without full coverage, so the core raises `GRAPH_ERROR_FIRST_WRITE_NOT_DISCARD` for a case the front end could not classify and for no other.


## Graph tests

- [Z2-N-REND-447] The graph unit tests listed below exist in `Zenith2/Engine/ZenithFlux/Tests/Graph/`, run on WARP and lavapipe with validation per "Barrier validation", and pass.

| Test | Asserts |
|---|---|
| `Test_RenderGraph_OrderFromVersionsOnly` | two declaration orders of the same passes compile to the same dependency-respecting order and identical results |
| `Test_RenderGraph_CullNoConsumer`, `Test_RenderGraph_SideEffectNeverCulled`, `Test_RenderGraph_MarkOutputKeepsChain` | culling roots and reasons |
| `Test_RenderGraph_EmptyRasterPassCulled`, `Test_RenderGraph_ClearFoldedIntoLoadOp`, `Test_RenderGraph_ClearKeptWhenNextUserIsCompute` | empty-pass culling and clear folding |
| `Test_RenderGraph_MergeIdenticalAttachments`, `Test_RenderGraph_MergeBlockedByBarrier`, `Test_RenderGraph_MergeBlockedByLoadStore` | merge conditions and dump reasons |
| `Test_RenderGraph_AsyncGapRule` | an async-flagged pass without a graphics gap stays on graphics |
| `Test_RenderGraph_CrossQueueTransfer` | release/acquire pairs and timeline waits for a texture written on async compute and sampled on graphics |
| `Test_RenderGraph_SplitBarrierPlaced`, `Test_RenderGraph_SplitBarrierNotAcrossBatches` | split-barrier placement and demotion rules |
| `Test_RenderGraph_SubresourceMipChain` | a bloom-style per-mip chain gets one barrier per mip transition and none redundant |
| `Test_RenderGraph_AliasPlacementFirstFit` | placement offsets for a fixed fixture set equal the pseudocode's hand-computed result |
| `Test_RenderGraph_AliasNeverAcrossUnorderedQueues` | two transients on different queues without a wait path never share memory |
| `Test_RenderGraph_AliasToggleNextCompile` | toggling applies at the next compile and changes nothing but placement |
| `Test_RenderGraph_PoisonDetectsFalseFullCoverage` | a fixture shader that under-covers a `[FullCoverage]` UAV produces NaN downstream under poisoning |
| `Test_RenderGraph_BarrierVerifierCatchesMissingBarrier` | a deliberately corrupted barrier list fails the verifier |
| `Test_RenderGraph_TimestampsPerPass`, `Test_RenderGraph_FrameDumpSchema` | timestamp insertion and dump completeness |
| `Test_RenderGraph_ParallelRecordMatchesSerial` | parallel and serial recording produce identical images |
