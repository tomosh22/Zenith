# RHI design: the render-graph back end

This file is part of the RHI design family. The entry document is `Zenith2/Docs/Design/RHI.md`. It specifies the back half of the Flux render graph that the RHI area owns: queue scheduling and lifetimes (compile step 3), queue assignment (step 6), command-list partitioning (step 7), the translation of synthesised barriers into RHI calls, recording, submission, the imported-state table that carries resource state across frames, the realisation of the six tracked resource kinds, queue-family ownership transfers, the back-end sections of the frame dump, and the test-only access type the graph conformance suite needs. The graph's public API, culling, ordering, placement, barrier synthesis and pass merging are RPROD's (entry document "Division of Flux code with RPROD"); this file cites them.

## Scope of this file

The specification is `Zenith2/Docs/Rendering/RenderGraph.md → "Graph lifecycle"`, "Graph compile" (steps 3, 6 and 7), "Barrier emission", "Frame dump" and `Zenith2/Docs/Rendering/GpuResources.md → "Tracked resource kinds"`. Every file named here lives in `Zenith2/Engine/ZenithFlux/Internal/Graph/` unless another path is given.

## Ownership of the compile steps

| Step or phase of `Zenith2/Docs/Rendering/RenderGraph.md` | Owner | Files |
|---|---|---|
| Begin, Setup | RPROD | `Flux_RenderGraph.cpp` |
| step 1 cull unused passes | RPROD | `Flux_GraphCull.cpp` |
| step 2 topological order | RPROD, through ZenithGraph | `Flux_GraphOrder.cpp` |
| step 3 lifetimes indexed by queue and position | RHI | `Flux_GraphQueues.h/.cpp` |
| step 4 memory placement | RPROD | `Flux_GraphPlacement.cpp` |
| step 5 automatic barrier synthesis | RPROD | `Flux_BarrierSynthesis.cpp`, `Flux_BarrierVerifier.cpp` |
| step 6 queue assignment | RHI | `Flux_GraphQueues.cpp` |
| step 7 partitioning into command lists | RHI | `Flux_GraphPartition.h/.cpp` |
| pass merging | RPROD | `Flux_GraphMerge.cpp` |
| Record phase | RHI (orchestration, barrier translation, timestamps and breadcrumbs placement); RPROD (pass bodies and contexts) | `Flux_GraphRecorder.h/.cpp`, `Flux_ToRHIBarrier.h/.cpp` |
| Submit phase | RHI | `Flux_GraphSubmitter.h/.cpp` |
| End phase | RHI | `Flux_GraphSubmitter.cpp` (history export, readback registration), `Flux_ImportedStateTable.h/.cpp` |
| frame dump | RHI | `Flux_FrameDumpWriter.h/.cpp` |

The interface between the halves is the compiled graph `Flux_CompiledGraph` (RPROD's, `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md → "Flux_CompiledGraph"`). RPROD's compile fills the per-pass barrier ranges (`m_uFirstBarrier`/`m_uBarrierCount` into `m_axBarriers`), the split-end and split-begin lists, the release lists, the timestamp and breadcrumb indices and the merged scopes; this area fills `m_axLifetimes` (step 3c), `m_xQueuePlan` (steps 3 and 6: the final queue, queue position and batch of each pass) and `m_xPartitionPlan` (step 7: the partition of each pass), indexed by the same live-pass index.

The three entries RPROD's compile driver calls (`Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md → "Compile driver"`, rows 3, 6 and 7; cross-area row RP3) are this area's declarations:

```cpp
// Zenith2/Engine/ZenithFlux/Internal/Graph/Flux_GraphQueues.h
// Step 3 (sub-steps 3a to 3d): fills m_xQueuePlan's provisional queues, positions and reachability, and m_axLifetimes.
void Flux_GraphBuildQueuePlan(Flux_CompiledGraph& xGraph, const Zenith_RHICapabilities& xCapabilities, const Flux_GraphSettings& xSettings, Zenith_Arena& xArena);
// Step 6 (sub-steps 6a to 6e): final queues, batches, waits; records FLUX_RG_ERROR_SWAPCHAIN_QUEUE (6d) and demotes
// cross-batch split candidates through Flux_DemoteSplit (6e).
void Flux_GraphAssignQueues(Flux_CompiledGraph& xGraph, Zenith_Arena& xArena);

// Zenith2/Engine/ZenithFlux/Internal/Graph/Flux_GraphPartition.h
// Step 7, after RPROD's pass merging: fills m_xPartitionPlan; demotes cross-partition split candidates through Flux_DemoteSplit.
void Flux_GraphPartition(Flux_CompiledGraph& xGraph, Zenith_Arena& xArena);
```

Each runs on the `Flux.BuildGraph` node inside `Flux_RenderGraph::Compile()`, allocates only from `xArena` (the graph arena), and reports a graph error by appending a `Flux_GraphDiagnostic` to the compiled graph's diagnostic list (RPROD's, `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md → "Flux_GraphErrors header"`), never through a return value, so every step still runs after an earlier error and one compile reports every error ([Z2-N-REND-291]). `xSettings` supplies `m_bAsyncCompute` for the gap rule of 3a; `xCapabilities` the dedicated queues (`HasDedicatedQueue`).

### RPROD types the back end consumes

<!-- assumption: pending RPROD -->
`Zenith2/Docs/Design/RPROD.md` (the entry document) is in drafting; its split file `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` exists and declares the types below. The back end assumes exactly these shapes; a change to any of them is a two-area ticket (the RHI and RPROD split ruling of the round-1 review).

| RPROD type or entry | Declared at | Shape the back end relies on |
|---|---|---|
| `Flux_CompiledGraph`, `Flux_CompiledPass` | `RenderGraphFrontEnd.md → "Flux_CompiledGraph"` | immutable once `Compile()` returns; `m_axPasses` in global (topological) order; per pass `m_uFirstBarrier`/`m_uBarrierCount`, `m_uFirstSplitEnd`/`m_uSplitEndCount`, `m_uFirstSplitBegin`/`m_uSplitBeginCount`, `m_uFirstRelease`/`m_uReleaseCount`, `m_uTimestampIndex`, `m_uBreadcrumbIndex`, `m_uScope`, `m_bScopeFirst`/`m_bScopeLast`, `m_uCost`, `m_uFlags`; the CSR predecessor and successor arrays; the writable `m_axLifetimes` span; the `m_xQueuePlan` and `m_xPartitionPlan` members of this file; `m_uTimestampCount[FLUX_QUEUE_COUNT]` |
| `Flux_ResourceLifetime` | as above | `m_uFirstGlobal`, `m_uLastGlobal`, per-queue first and last positions (`uFLUX_RG_INVALID` when unused), `m_uLastUseStages`; written by step 3c alone |
| `Flux_BarrierRecord`, `Flux_BarrierState`, `Flux_SplitRole` | `RenderGraphFrontEnd.md → "Step 5 barrier synthesis realisation"` | 40-byte record: resource, first subresource and count (buffers in 16-byte granules, 0 = whole), from and to `Flux_BarrierState` (access, layout, queue, neutral stage and access bits), split role, discard flag, producer pass |
| `Flux_ResourceAccess`, `Flux_ImageLayout` | as above | the access-state enumerators of "Neutral barrier translation" (the rows of that table, one to one); `Flux_ImageLayout` equal to `Zenith_RHITextureLayout` value for value |
| `Flux_GraphCaches` | `RenderGraphFrontEnd.md → "Flux_GraphCaches header"` | one per device; owns this file's `Flux_ImportedStateTable` and returns it through `GetImportedStateTable()` |
| `Flux_GraphInternal_AddPass`, `Flux_GraphInternal_AddBufferAccess`, `Flux_GraphInternal_AddOrdering`, `Flux_GraphInternal_GetCompiled` | `RenderGraphFrontEnd.md → "Graph internal entry points"` | the production declaration entries the public templates forward to; `AddOrdering` reachable from `Flux_RenderGraphTestAccess` alone |
| `Flux_RGError`, `Flux_GraphErrorList` | `RenderGraphFrontEnd.md → "Flux_GraphErrors header"` | `Compile()` returns `Zenith_Result<const Flux_CompiledGraph*, Flux_GraphErrorList>` (Rendering's `Zenith2/Docs/Rendering/RenderGraph.md → "Graph lifecycle"` signature); `Flux_RGError` holds the codes, including `FLUX_RG_ERROR_SWAPCHAIN_QUEUE`, which this file raises (step 6d) |
| `Flux_Renderer` | RPROD's `Zenith2/Docs/Design/RPROD/Renderer.md` (in drafting); named in `Zenith2/Docs/Architecture.md → "Assumed types"` | owns the RHI-area objects of entry document "RHI-owned files in ZenithFlux" and destroys them in reverse creation order |

## Internal file map

| File | Contents |
|---|---|
| `Flux_GraphQueues.h/.cpp` | `Flux_QueuePlan`, steps 3 and 6 |
| `Flux_GraphPartition.h/.cpp` | `Flux_PartitionPlan`, step 7 |
| `Flux_ToRHIBarrier.h/.cpp` | `Flux_ToRHIBarrier`, `Flux_ToRHIAccess`: Flux access states to the neutral barrier vocabulary |
| `Flux_GraphRecorder.h/.cpp` | `Flux_GraphRecorder`: the `Flux.Record` node body and its per-partition sub-jobs |
| `Flux_GraphSubmitter.h/.cpp` | `Flux_GraphSubmitter`: the `Flux.Submit` node body, the swapchain acquire and present, the End phase |
| `Flux_ImportedStateTable.h/.cpp` | the imported-state table |
| `Flux_FrameDumpWriter.h/.cpp` | the frame dump's back-end sections and the background write |
| `Zenith2/Engine/ZenithFlux/Tests/GraphConformance/Flux_RenderGraphTestAccess.h` | the test-only access type ("Flux_RenderGraphTestAccess") |

## Step 3 queues lifetimes and reachability

The step's four sub-steps are `Zenith2/Docs/Rendering/RenderGraph.md → "Graph compile"` (heading "Step 3 lifetimes indexed by queue and position"). This design fixes their data and algorithms.

```cpp
struct Flux_PassQueueInfo            // one per live pass, graph arena
{
	Flux_QueueClass m_eQueue;        // final after step 6
	uint32 m_uQueuePosition;        // position within the queue's sub-order
	uint32 m_uGlobalIndex;          // position in the topological order
	uint32 m_uBatch;                // submission batch within the queue (step 6)
	uint32 m_uPartition;            // command list within the queue (step 7)
};

struct Flux_QueuePlan                // graph arena
{
	Zenith_Span<Flux_PassQueueInfo> m_axPasses;                 // indexed by live-pass index
	Zenith_Span<uint64> m_aulReachability;                     // live × ceil(live / 64) bits: bit j of row i = pass i is ordered after pass j
	uint32 m_auPassCount[FLUX_QUEUE_COUNT];
};
```

| Sub-step | Algorithm | Complexity |
|---|---|---|
| 3a provisional queue | for each live pass in topological order: `FLUX_PASS_FLAG_COPY_QUEUE` → copy when `HasDedicatedQueue(COPY)`; `FLUX_PASS_FLAG_ASYNC_COMPUTE` → async compute when `m_bAsyncCompute`, `HasDedicatedQueue(ASYNC_COMPUTE)` and the gap rule holds; every other pass → graphics. The gap rule: let p be the maximum global index of the pass's producers (0 when none) and c the minimum global index of its consumers (the live-pass count when none); the rule holds when some graphics-queue pass has a global index strictly between p and c. Only passes carrying the "A" mark of `Zenith2/Docs/Rendering/FrameAnatomy.md` carry the flag | O(P + E) with a prefix count of graphics passes by global index |
| 3b positions | queue positions are the passes of each queue in topological order | O(P) |
| 3c lifetimes | per resource, the (queue, position) pairs of its live accesses, and the interval [first, last] of their global indices; recorded in the resource's `Flux_ResourceLifetime` | O(accesses) |
| 3d reachability | rows in topological order: row(i) = OR over the direct predecessors j of i of (row(j) with bit j set), where the direct predecessors are the graph edges into i plus the previous pass on i's queue | O(P × E / 64) |

A pass on one queue is ordered after a pass on another queue only through a graph edge, which step 6 turns into a timeline wait, so reachability is exactly "ordered by a wait or by queue order", which is what `may_share` of `Zenith2/Docs/Rendering/RenderGraph.md → "Aliasing placement"` reads ([Z2-N-REND-436]).

## Step 6 queue assignment

| Sub-step | Rule |
|---|---|
| 6a | the provisional queues become final; nothing moves |
| 6b | walk each queue's passes in queue order; start a new batch before a pass that has a producer on another queue whose batch has not been waited on by this queue yet, and after a pass that has a consumer on another queue (so the producer's batch ends, and signals, right after it) |
| 6c | each cross-queue edge (producer P on queue A, consumer C on queue B) becomes `signal(A, batch(P))` and `wait(B, batch(C)) on value(A, batch(P))`; the waits of one batch are reduced to one per source queue (the maximum value), and a wait already implied by an earlier batch's wait on the same queue for an equal or greater value is removed |
| 6d elaboration | a pass on the async-compute or copy queue that writes the swapchain image records `FLUX_RG_ERROR_SWAPCHAIN_QUEUE`; the final graphics batch is the one that writes the swapchain image; it carries `m_pxSignalPresentReady`, and the first graphics batch that writes the swapchain image carries `m_pxWaitAcquire` |
| 6e | every split candidate (RPROD's `FLUX_SPLIT_CANDIDATE`, `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md → "Split-barrier candidates"`) whose producer and consumer fall in different submission batches is demoted through RPROD's `Flux_DemoteSplit(xGraph, uRecord)`, which records `FLUX_RG_INFO_SPLIT_DEMOTED` ([Z2-N-REND-394]) |

Timeline values are not known at compile time: each batch carries a symbolic value, and the submitter resolves it to the value `Submit` returned for that batch. Batches are submitted in an order that respects every wait: the submitter walks the three queues' batch lists and submits the next batch of any queue whose waits name only already-submitted batches, graphics first on a tie, then async compute, then copy. The walk cannot deadlock because the graph is acyclic.

## Step 7 command-list partitions

`Zenith2/Docs/Rendering/RenderGraph.md → "Graph compile"` [Z2-N-REND-305] fixes the target cost (4,096 units) and the two no-split rules. The partitioner additionally keeps every split barrier's two halves in one partition: after the partitions are fixed, a candidate whose halves would cross partitions is demoted through RPROD's `Flux_DemoteSplit(xGraph, uRecord)` (`Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md → "Split-barrier candidates"`), which turns it into an ordinary barrier in the consumer's batch before the consumer and records `FLUX_RG_INFO_SPLIT_DEMOTED` — the same fallback as the cross-batch case of row 6e — because D3D12 split barriers end in the command list that began them. The rewording of [Z2-N-REND-393] and [Z2-N-REND-394] that admits this demotion is cross-document item X98, which already carries the Rendering wording.

<!-- elaboration: the partition limit and the target-raise rule -->
```cpp
struct Flux_Partition
{
	Flux_QueueClass m_eQueue;
	uint32 m_uBatch;
	uint32 m_uFirstPass;            // queue position of its first pass
	uint32 m_uPassCount;
	uint32 m_uCost;
};

struct Flux_PartitionPlan
{
	Zenith_Span<Flux_Partition> m_axPartitions;                // queue order, then batch order
};
```

Algorithm: for each queue and each batch, walk the passes; add a pass to the current partition while the partition's cost plus the pass's cost hint is at most the target, or while the pass is inside a merged scope that started in the current partition; otherwise close the partition and open a new one. At most `uZENITH_RHI_MAX_PARTITIONS_PER_QUEUE` = 64 partitions exist per queue; a frame that would need more raises the target cost to `ceil(total / 64)` for that queue, which is counted in `Flux.Graph.PartitionTargetRaised`.

## Neutral barrier translation

`Flux_ToRHIAccess` maps each `Flux_ResourceAccess` of `Zenith2/Docs/Rendering/RenderGraph.md → "Barrier state machine"` (table "Access states") onto the neutral stage, access and layout of `Zenith2/Engine/ZenithRHI/Zenith_RHIBarrier.h`; each backend then maps the neutral values onto its API in one function (`Zenith2/Docs/Design/RHI/Vulkan.md → "Barrier translation"`, `Zenith2/Docs/Design/RHI/D3D12.md → "Enhanced barrier translation"`), so the two tables together are exactly the Vulkan and D3D12 columns of Rendering's table.

| `Flux_ResourceAccess` | Neutral stages | Neutral access | Neutral layout |
|---|---|---|---|
| `SRV_GRAPHICS` | the field's graphics stages (`VERTEX_SHADER`, `PIXEL_SHADER`, `MESH_SHADER`, `TASK_SHADER`) | `SHADER_SAMPLED_READ` (texture) or `SHADER_STORAGE_READ` (buffer) | `SHADER_READ` |
| `SRV_COMPUTE` | `COMPUTE_SHADER` | as above | `SHADER_READ` |
| `UAV` | the field's stages | `SHADER_STORAGE_READ` and/or `SHADER_STORAGE_WRITE` by the field's access | `GENERAL` |
| `COLOUR_ATTACHMENT` | `COLOUR_ATTACHMENT_OUTPUT` | `COLOUR_ATTACHMENT_WRITE`, plus `COLOUR_ATTACHMENT_READ` when blending or `LOAD` | `COLOUR_ATTACHMENT` |
| `DEPTH_WRITE` | `EARLY_FRAGMENT_TESTS \| LATE_FRAGMENT_TESTS` | `DEPTH_STENCIL_READ \| DEPTH_STENCIL_WRITE` | `DEPTH_STENCIL_WRITE` |
| `DEPTH_READ_ONLY` | the tests plus any sampling stage | `DEPTH_STENCIL_READ`, plus `SHADER_SAMPLED_READ` when also sampled | `DEPTH_STENCIL_READ` |
| `VERTEX_BUFFER` | `VERTEX_ATTRIBUTE_INPUT` | `VERTEX_ATTRIBUTE_READ` | — |
| `INDEX_BUFFER` | `INDEX_INPUT` | `INDEX_READ` | — |
| `INDIRECT_ARGS` | `DRAW_INDIRECT` | `INDIRECT_COMMAND_READ` | — |
| `COPY_SOURCE` / `COPY_DEST` | `COPY` | `TRANSFER_READ` / `TRANSFER_WRITE` | `TRANSFER_SRC` / `TRANSFER_DST` |
| `CLEAR` | `CLEAR` | `TRANSFER_WRITE` | `TRANSFER_DST` |
| `PRESENT` | `NONE` | `NONE` | `PRESENT` |
| `UNDEFINED` | `NONE` | `NONE` | `UNDEFINED`, `m_bDiscard = true` |
| `PERSISTENT_READ` | `ALL_COMMANDS` | `SHADER_SAMPLED_READ \| SHADER_STORAGE_READ \| VERTEX_ATTRIBUTE_READ \| INDEX_READ` | `PERSISTENT_READ` |

`Flux_ToRHIBarrier(Zenith_Span<const Flux_BarrierRecord>, Zenith_Arena&) → Zenith_RHIBarrierBatch` converts one pass's synthesised records (RPROD's `Flux_BarrierRecord`: subresource run, the from and to `Flux_BarrierState` with their queues, discard flag, split role; "RPROD types the back end consumes") into one batch: texture records become `Zenith_RHITextureBarrier`s; buffer records with no queue change are folded into the batch's global barrier by OR-ing their stages and accesses ([Z2-N-REND-390]); buffer records with a queue change become `Zenith_RHIBufferBarrier`s. The mapping sits in Flux and names no Vulkan or D3D12 type, which keeps [Z2-N-CONTRACT-043]; it is the neutral half of the "one function per backend" mapping of [Z2-N-REND-387] (entry document "Reconciliation items", R2).

## Recording

The `Flux.Record` node runs one sub-job per partition through `Zenith_JobContext::ParallelFor(uPartitionCount, 1, …)` (`Zenith2/Docs/Design/JOBS/PublicAPI.md → "Zenith_JobContext"`) when `m_bParallelRecord` is set, and one serial loop otherwise; a partition's command list is acquired with `AcquireCommandList(queue, frameSlot, partitionIndex)` inside its own sub-job, so partitions share no mutable state.

### Partition prologue

1. `BindGlobalDescriptors()` ([Z2-N-REND-306]).
2. `BeginDebugLabel("<frame index> <queue> P<partition>")` in development builds.
3. When the partition is the first of its batch on its queue: the batch's acquire halves of cross-queue transfers (recorded as one batch).

### Per-pass recording order

| Order | Action |
|---|---|
| 1 | the split-barrier end halves due before this pass (`Zenith_RHIRecordSplitBarrierEnd`) |
| 2 | the pass's barrier batch (`Zenith_RHIRecordBarriers`), outside any rendering scope |
| 3 | `WriteBreadcrumb(passIndex, BEGIN)` when breadcrumbs are on |
| 4 | `WriteTimestamp(pool, 2 × passIndex)` when timestamps are on ([Z2-N-REND-311]) |
| 5 | `BeginDebugLabel(pass name)` in development builds |
| 6 | for the first pass of a merged scope: `BeginRendering` with the scope's merged attachments |
| 7 | the pass body (RPROD's: the raster lambda through `Flux_RasterContext`, or the graph's own body for compute, fullscreen, copy, clear and readback passes) |
| 8 | for the last pass of a merged scope: `EndRendering` |
| 9 | `EndDebugLabel()`, `WriteTimestamp(pool, 2 × passIndex + 1)`, `WriteBreadcrumb(passIndex, END)` |
| 10 | the split-barrier begin halves this pass produces (`Zenith_RHIRecordSplitBarrierBegin`), after its rendering scope ended ([Z2-N-REND-394]) |

A merged scope writes timestamps and breadcrumbs for each of its passes inside the scope (timestamps are legal inside dynamic rendering; breadcrumbs inside a scope use the marker path of Recovery.md "Breadcrumbs" or report at scope granularity). The last partition of a queue in the frame additionally records the release halves of cross-queue transfers of its final batch and `ResolveTimestamps(pool, 0, 2 × queuePassCount)`.

### Parallel recording jobs

- Every pass body reads only the compiled graph, its own patched parameter struct and the device; descriptor writes happen on the `Flux.BuildGraph` node after compile, never inside a pass lambda or a `Flux.Record` job ([Z2-N-REND-052]), so recording jobs read slot indices already written and never write shared state.
- Upload-ring `Allocate` calls from record jobs are lock-free bumps inside the frame reservation ([Z2-N-REND-496]), which is final before any record job starts (`Zenith2/Docs/Design/RHI/Memory.md → "Build-node upload order"`).
- `Test_RenderGraph_ParallelRecordMatchesSerial` (RPROD's list, run by this area) proves parallel and serial recording produce identical images.

## Submission

The `Flux.Submit` node (render-submit thread) runs:

| Step | Action |
|---|---|
| 1 | acquire the swapchain image when the frame writes one (`AcquireNextImage(frameSlot)`); `SWAPCHAIN_OUT_OF_DATE` or `SURFACE_UNAVAILABLE` marks the swapchain for recreation, and the frame's passes that write the swapchain image record into a stand-in transient of the same format and extent, so every other pass of the frame still runs and its results (histories, readbacks) stay valid; the frame is not presented |
| 2 | submit the batches in the order of "Step 6 queue assignment", each with its resolved waits; record each batch's returned value |
| 3 | record the last use of every imported resource the frame accessed, per queue, as the value of the last batch on that queue that accessed it (`Flux_ImportedStateTable::Commit`), and of every descriptor slot the frame patched (the transient view descriptors, freed with those values); `Flux_GPUScene::OnFrameSubmitted` with the frame's final values; stamp the staged upload ranges the frame's `StreamingUpload` pass copied; enqueue the frame's client batches (at most one `CLIENT` entry per kind, `Zenith2/Docs/Design/RHI/Memory.md → "Retirement queue"`) |
| 4 | `DrainReleaseRequests(uFLUX_RETIREMENT_DRAIN_HEADROOM)` on the retirement queue (`Zenith2/Docs/Design/RHI/Memory.md → "Drain headroom"`), so a release requested during this frame, and a descriptor slot released with it, retire after this frame's submissions |
| 5 | End phase: export the view histories' final versions to their views, register the frame's readbacks with their completion values, write the frame-slot record `{frame index, final value per queue}` (`Flux_FrameLifetime::RecordFinalValues`, which advances `m_ulSubmittedFrames`) |

A frame that submits nothing (`Flux_FrameLifetime::GetFrameOutcome()` is `FLUX_FRAME_OUTCOME_DEFERRED` or `FLUX_FRAME_OUTCOME_FAILED`, Memory.md "Frame deferral" and "Frame failures") records no partition, runs step 4 alone (the build node already called `Flux_GPUScene::OnFrameDeferred` and failed the frame's readbacks), and its `Flux.Present` presents nothing and runs `PrepareNextFrame`.

The `Flux.Present` node then presents (`Present(image)`), handles its result (Recovery.md "Acquire and present"), captures if a capture is armed, and runs `Flux_FrameLifetime::PrepareNextFrame` (entry document "Frame-in-flight rotation").

## Imported-state table

```cpp
struct Flux_ImportedStateRun            // 16 bytes
{
	uint32 m_uFirstSubresource;        // mip × layerCount + layer ([Z2-N-REND-385] indexing)
	uint32 m_uCount;
	Flux_ResourceAccess m_eAccess;      // uint8
	Flux_ImageLayout m_eLayout;         // uint8
	Flux_QueueClass m_eQueue;           // uint8: the queue that owns it (Vulkan exclusive textures)
	uint8 m_uPad;
	uint32 m_uReserved;
};

struct Flux_ImportedStateEntry          // 48 bytes
{
	Zenith_RHIResourceHandle m_xResource;
	uint32 m_uSubresourceCount;        // 1 for buffers
	uint32 m_uFirstRun;                // index into the run pool
	uint16 m_uRunCount;
	uint16 m_uFlags;                   // FLUX_IMPORTED_FLAG_WRITE_RESTRICTED (GPU scene tables), _PERSISTENT_READ_ONLY, _SWAPCHAIN
	uint32 m_uPad;
	Flux_QueueLastUse m_xLastUse;       // 24 bytes
};

class Flux_ImportedStateTable          // owned by RPROD's Flux_GraphCaches; one per device
{
public:
	void Register(Zenith_RHIResourceHandle xResource, uint32 uSubresourceCount, Flux_ResourceAccess eInitial, Flux_ImageLayout eLayout, Flux_QueueClass eQueue, uint16 uFlags);
	const Flux_ImportedStateEntry* Find(Zenith_RHIResourceHandle xResource) const;
	void Commit(const Flux_CompiledGraph& xGraph, const Flux_BatchValues& xValues);   // step 3 of Submission
	void Unregister(Zenith_RHIResourceHandle xResource);                                // at the resource's retirement (OnResourceRetired)
	uint32 GetEntryCount() const;
};
```

<!-- elaboration: the storage, its capacity and its single-writer rule -->
- Storage: a `Zenith_HashMap<uint64, uint32>` from handle bits to entry index (`Zenith2/Docs/Design/CORE/Containers.md → "Zenith_HashMap and Zenith_HashSet"`), a `Zenith_Array` of entries and a pooled `Zenith_Array` of runs; capacity 65,536 entries, growing through the general heap. The table holds no mutex and has no lock-order row: it is synchronised by the single-writer rule below.
- Single-writer rule (thread affinity `OWNER_THREAD`): every member runs either on the render subgraph's nodes, which run one after another — `Flux.BuildGraph` (`Register` at Begin for the acquire list and for build-node creators, `Find`), `Flux.Submit` (`Commit`, render-submit thread), `Flux.Present` (`Unregister` through `OnResourceRetired` inside `PrepareNextFrame`'s `RetireCompleted`) — or on the main thread at a render-idle point (`Zenith2/Docs/Design/RHI.md → "Render-idle points"`: renderer creation, swapchain recreation, a frames-in-flight change, recovery). Ownership moves only along the render subgraph's edges and the iteration barrier ([Z2-N-ARCH-386]: `Flux.Present(f − 1)` completes before `Flux.BuildGraph(f)`), so no two members ever overlap. Asset loader jobs never call it: a persistent resource they create is registered when it joins the acquire list (`Zenith2/Docs/Design/RHI/Memory.md → "Persistent resource creation and the acquire list"`), and it leaves the table when it retires.
- Overlap detector: the table has a `std::atomic<uint32> m_uAccessDepth` member in every build; where `ZENITH_ASSERTS_ENABLED=1`, each member does `ZENITH_ASSERT(m_uAccessDepth.fetch_add(1, acquire) == 0)` on entry and `fetch_sub(1, release)` on exit, so any concurrent use asserts; `Test_ImportedState_ConcurrentLoaderCreationDuringBuild` runs the loader-concurrency case under it.
- `Register` is called by the creator of every resource that will be imported, in the contexts above: persistent render targets and histories (the view registry on the build node; `SHADER_READ` or `UNDEFINED` with the first frame's first-write rule exempt, [Z2-N-REND-443]), pools and GPU scene tables (renderer creation or build-node growth; `UAV` or `SHADER_READ`, write-restricted for tables), swapchain images (render-idle points; `UNDEFINED`, flagged), readback buffers (renderer creation), persistent read-only resources (at acquire, on the build node). A second `Register` of a live handle overwrites its runs (a history invalidated by a camera cut is re-registered `UNDEFINED`). Importing an unregistered resource is `ZENITH_ASSERT` and, without asserts, starts from `UNDEFINED` with the resource treated as discarded.
- At the next frame's first access the graph transitions from the recorded run state ([Z2-N-REND-294]); a resource imported on a different queue than it was left on gets the ownership transfer of "Queue-family ownership transfers".
- `Unregister` is called by `Flux_DeferredDeletion::OnResourceRetired` for every table resource the device destroys (`Zenith2/Docs/Design/RHI/Memory.md → "Flux_DeferredDeletion"`), before the resource's table slot is freed, so a retired handle's state never leaks into a reused slot (the handle's generation differs as well). `Commit` skips a resource the frame accessed that is no longer registered.

## Tracked resource kinds realised

The six kinds of `Zenith2/Docs/Rendering/GpuResources.md → "Tracked resource kinds"`, with the back-end mechanism of each:

| Kind | Back-end mechanism |
|---|---|
| imported resources | the imported-state table above: state, layout and owning queue per subresource run across frames, last use per queue |
| persistent histories | two entries per history (the ping-pong halves), each imported every frame; the half written this frame is committed with its final state, the half read keeps its read state; a history invalidated by a camera cut is re-registered `UNDEFINED` so its next first access is a discard |
| subresource ranges | runs of `Flux_ImportedStateRun` over the subresource index, split and merged at every commit (adjacent runs with equal state merge) |
| read-modify-write access | a read-write access commits the state of the write it produced; the table holds no version, because versions are per frame |
| queue-family ownership transfers | the owning queue per run, and the release/acquire pairs below |
| aliasing barriers | per placed-resource group, the last-use stages of every occupant, which the first use's discard names as its source scope ([Z2-N-REND-438]); kept in RPROD's placement result and translated here into the batch's global barrier source stages |

## Queue-family ownership transfers

| Case | Vulkan | D3D12 |
|---|---|---|
| a graph texture written on queue A and read on queue B in the same frame | the producer's partition records a release half (`m_bReleaseHalf`, src family A, dst family B, the layout transition) after the texture's last access on A; the consumer's partition records the acquire half (`m_bAcquireHalf`, same families and layouts) before its first access on B; the batch boundary between them carries the timeline wait ([Z2-N-REND-391]) | a layout transition on the queue whose type supports both layouts (the direct queue supports every layout; the compute queue `SHADER_RESOURCE`, `UNORDERED_ACCESS`, `COMMON`, `GENERIC_READ`; the copy queue `COMMON`, `COPY_SOURCE`, `COPY_DEST`), placed before the signal when that is the producer queue, otherwise after the wait |
| an imported texture left on queue A last frame and first accessed on queue B this frame | the release half is recorded at the start of this frame's first batch on A (an empty batch is created for it when A has no work, `Flux.Graph.TransferOnlyBatches`), then the acquire half on B after a wait on that batch | the transition on B after a wait on A's last value (already complete in practice) |
| graph buffers | `VK_SHARING_MODE_CONCURRENT` over the three families: no transfer; the timeline wait plus a buffer or global barrier on the consumer | the timeline wait plus the barrier |
| `Flux_PersistentReadOnly` textures | created concurrent over the three families ([Z2-N-REND-388]); no transfer ever | `GENERIC_READ` after the one-time acquire; no transfer |
| the swapchain image | acquired and presented on the graphics queue alone; a pass on another queue that writes it is a graph compile error (`FLUX_RG_ERROR_SWAPCHAIN_QUEUE`, added by this design) | as Vulkan |

Queue order alone never orders memory access: every transfer is paired with a timeline wait ([Z2-N-REND-392]).

## Frame dump back-end sections

The frame dump's schema, triggers and file location are `Zenith2/Docs/Rendering/RenderGraph.md → "Frame dump"`. `Flux_FrameDumpWriter` owns the document: it embeds the core `zenith2.graph-dump` document (`Zenith2/Docs/Design/GRAPH.md → "Graph dump schema zenith2.graph-dump version 1"`), RPROD's front-end keys (`passes`, `placement`, `scopes`, `memory`, `errors`), and writes the back-end keys this area produces:

| Key | Content | Source |
|---|---|---|
| `passes[].queue`, `passes[].partition`, `passes[].barriersBefore` | the final queue, the partition index, the translated barriers (kind, from, to, split, queue transfer) | steps 6 and 7, `Flux_ToRHIBarrier` |
| `batches` | per queue, per batch: its passes, waits (queue, after-pass, value once resolved), signals | step 6 and the submitter |
| `device` (added by this design, elaboration) | `{ "backend", "adapter", "driver", "tier", "framesInFlight", "frameSlot", "heapCapacities", "descriptorOccupancy", "pendingRetirements", "uploadRingBytes", "uploadRingUsedBytes", "readbackPoolUsedBytes" }` | the device and the lifetime objects |
| `memory.heapBudgets` (added) | per device heap `{ "budget", "usage", "deviceLocal" }` | `QueryHeapBudgets` |

Writing: the compiled graph's dump model is copied into a dump arena at the end of compile and formatted by a background job with `Zenith_JsonWriter` in `JSON_WRITE_STYLE_PRETTY` (`Zenith2/Docs/Design/CORE/Text.md → "JSON writer"`), never delaying record or submit ([Z2-N-REND-315]); the file is written through the platform's journaled atomic write. <!-- elaboration -->
The last three frames' dump models are kept in a ring of three dump arenas (development builds) so that a device-loss report can write them (Recovery.md "Device-loss report"); Shipping builds keep none and write no dump.

## Flux_RenderGraphTestAccess

The test-only access type that the graph conformance suite's Flux builder uses (`Zenith2/Docs/Design/GRAPH/Tests.md → "The Flux builder mapping"`), in `Zenith2/Engine/ZenithFlux/Tests/GraphConformance/Flux_RenderGraphTestAccess.h`, header-only, compiled into `ZenithGraph_Conformance` (through ZenithFlux's `Tests/GraphConformance/` partition) and `ZenithFlux_Tests`:

```cpp
class Flux_RenderGraphTestAccess
{
public:
	static uint32 AddRawComputePass(Flux_RenderGraph& xGraph, Zenith_StringView strName, uint32 uCostHint, bool bSideEffect);
	static void AddRawAccess(Flux_RenderGraph& xGraph, uint32 uPass, Flux_RGBuffer xBuffer, Zenith_GraphAccessMode eMode, uint64 ulOffset, uint64 ulBytes);
	static void AddRawOrdering(Flux_RenderGraph& xGraph, uint32 uBefore, uint32 uAfter, Zenith_StringView strReason);
	static Zenith_Result<const Zenith_GraphSchedule*, Flux_GraphErrorList> CompileWithoutDevice(Flux_RenderGraph& xGraph);
	static uint32 GetCoreNodeIndex(const Flux_RenderGraph& xGraph, uint32 uPass);
};
```

| Member | Behaviour |
|---|---|
| `AddRawComputePass` | declares a compute pass with no program and no parameter struct, the given cost hint and `FLUX_PASS_FLAG_SIDE_EFFECT` when asked; returns the pass index. It is implemented on `Flux_GraphInternal_AddPass` (`Internal/Graph/Flux_RenderGraphInternal.h`), the same entry `AddComputePass` uses after it reads the field table, so the raw path exercises the production declaration path |
| `AddRawAccess` | declares one buffer access over `[ulOffset, ulOffset + ulBytes)` with the core mode: `READ` → SRV read, `WRITE` → partial UAV write, `READ_WRITE` → UAV read-write, `WRITE_DISCARD` → `[FullCoverage]` UAV write; implemented on `Flux_GraphInternal_AddBufferAccess`, the entry the field-table walk uses per field |
| `AddRawOrdering` | adds a core `AddOrdering` edge through `Flux_GraphInternal_AddOrdering`; production code has no ordering API, because Flux orders by versions alone ([Z2-N-REND-293]); the internal entry exists for this access type and is unreachable from the public API |
| `CompileWithoutDevice` | runs compile steps 1, 2 and 3 with the device replaced by a capability record in which every dedicated queue is absent, and returns the core schedule; steps 4 to 7 need the device and do not run |
| `GetCoreNodeIndex` | maps a Flux pass index to the core node index of the compiled schedule |

`Flux_RenderGraph` declares `friend class Flux_RenderGraphTestAccess;`; the friend declaration adds no code or data to the shipping library, and no production source includes the header (the TestSeparationCheck gate step's rule 3 would also fail a doctest token outside `Tests/`).

## Back-end tests

The tests of this file are listed in `Zenith2/Docs/Design/RHI/Tests.md → "Graph back-end tests"`.
