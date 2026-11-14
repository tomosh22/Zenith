# GRAPH design — CPU frame graph internals

This file is part of the GRAPH design family; the entry document is `Zenith2/Docs/Design/GRAPH.md`. It specifies the GRAPH part of `Engine/ZenithJobs`: the builder's storage, the translation of frame-graph declarations into ZenithGraph, schedule emission, the schedule cache, the frame executor with its state machines, render-extraction pipelining, the fixed-order reduction and the determinism proof obligations. The public headers are `Zenith2/Docs/Design/GRAPH.md` → "Zenith_FrameGraph.h", "Zenith_FrameSchedule.h", "Zenith_FrameExecutor.h" and "Zenith_FixedOrderReduce.h"; the specification is `Zenith2/Docs/Architecture/CPUFrameGraph.md`. Code blocks use tabs.

## Internal file map

| File | Contents |
|---|---|
| `Engine/ZenithJobs/Internal/FrameGraph/Zenith_FrameGraphInternal.h` | the builder records (in `Zenith_Array` arena-tier tables), `Zenith_FrameLowering`, `Zenith_FrameScheduleStructure`, the private layout of `Zenith_FrameSchedule`, `Zenith_FrameSubgraphInstance`, `Zenith_FrameNodeInstance`, the executor's private layout |
| `Zenith_FrameGraphBuilder.cpp` | declaration calls and their declaration-time checks |
| `Zenith_FrameTranslate.cpp` | translation steps 0 to 5 |
| `Zenith_FrameLower.cpp` | steps 6 and 7 |
| `Zenith_FrameAssemble.cpp`, `Zenith_FrameTransients.cpp` | schedule emission |
| `Zenith_FrameSchedule.cpp`, `Zenith_FrameScheduleCache.cpp` | schedules, structures, bindings, the cache |
| `Zenith_FrameExecutor.cpp`, `Zenith_FrameExecutorIteration.cpp`, `Zenith_FrameExecutorTimings.cpp` | the executor |
| `Zenith_FrameDump.cpp`, `Zenith_FrameGraphText.cpp`, `Zenith_FixedOrderReduce.cpp` | dumps, messages, the reduction |

## Builder storage

The builder stores declarations in `Zenith_Array<T>` tables in the arena tier of the builder arena (`Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_Array"), one per record kind, appended with `TryPushBack` in amortised O(1) and read in declaration order. IDs are declaration indices; a declaration call validates only the ID range (O(1)), everything else is checked by the translation. GRAPH adds no container (`Zenith2/Docs/Design/GRAPH.md` → "Custom infrastructure records this area adds").

| Record | Bytes | Fields |
|---|---|---|
| node | 48 | name (arena copy, `Zenith_StringView`), subgraph, kind, affinity (`uint8` each), group, stage (`uint16` each), `m_pfnRun`, `m_pData`, cost hint |
| resource | 40 | name, key, group, persistent flag, transient bytes (`uint64`, 0 = none), transient alignment |
| access | 16 | node, resource, bits: write, also-reads |
| ordering | 24 | node, direction (Before or After), required flag, target name (arena copy) |
| barrier | 24 | subgraph, group, stage, `m_pfnRun`, `m_pData` |
| declaration error | 24 | code, reason index, the offending IDs or values |

A declaration call that receives an ID at or above the relevant count records `FRAME_GRAPH_ERROR_INVALID_DECLARATION` (reason `id <i> not issued by this builder`) and is skipped. Arena exhaustion sets the builder's out-of-memory flag; the next `Compile` reports one `FRAME_GRAPH_ERROR_OUT_OF_MEMORY` (step `declaring`) and translates nothing. The builder also holds its diagnostic table: every `Compile` or `Acquire` over it clears the table, then formats every diagnostic of that call into the builder arena, logs each at the point of failure, sorts them by (code, node A, node B), and `GetDiagnostics()` returns them until the builder is destroyed ([Z2-N-ARCH-440]); the builder reserves 16 KiB of its arena for this table at its first declaration, so an out-of-memory compile still reports its one diagnostic. `Read` and `Write` of the same (node, resource) pair merge: `Write(n, r, b)` after `Read(n, r)` becomes `Write(n, r, true)`; a second `Write` keeps the OR of the also-reads bits.

## Translation pipeline

`Compile` (and `Zenith_FrameScheduleCache::Acquire`) run steps 0 to 7 in order; each step records errors and continues, and lowering (step 6) skips only the subgraphs that have a jobs-level error (Z2-R-GRAPH-062 to Z2-R-GRAPH-072). D is the total declaration count.

### Step 0 validation

Every node, resource, transient size and barrier is checked against the `FRAME_GRAPH_ERROR_INVALID_DECLARATION` reasons of `Zenith2/Docs/Design/GRAPH.md` → "Error codes", in declaration order. Node names additionally may not contain `@` (reason `'@' in node name`), because `@` introduces a group qualifier. Per (TICK subgraph, group) at most one `FRAME_NODE_KIND_TICK_BOUNDARY` node exists, and it is at the group's lowest stage with no other node at that stage. O(D).

<!-- assumption: pending ECS -->
`Zenith_WorldSimulation::EmitInto` (ECS) declares its nodes so that these checks pass: no declared node name contains `@`; each world declares exactly one `FRAME_NODE_KIND_TICK_BOUNDARY` node, alone at the world's lowest TICK stage; arbitration staging resources are declared with `m_bPersistent = false`; and a failed system is disabled inside ECS's own node trampoline, which completes the node without running the system ([Z2-N-ARCH-410]), because the executor has no disable path.
<!-- /assumption -->

### Step 1 node names and qualified references

1. For each (subgraph, group), build an array of `(name hash, node index)` with `Zenith_Hash64` over the name bytes, sort by `(hash, name bytes, node index)`; equal names are `FRAME_GRAPH_ERROR_DUPLICATE_NODE_NAME` (both indices).
2. A reference `szOtherNodeName` resolves within the declaring node's subgraph:
   - `Name`: in the declaring node's group;
   - `Name@W<tag>`, `<tag>` a decimal number 0 to 65535 without leading zeros: in group `<tag>`;
   - `Name@W*`: in every non-zero group of the subgraph that has a node `Name` (possibly none).
   The suffix is the last `@W` in the string. Lookup is a binary search on the hash, then a byte comparison.
3. A required reference that resolves to nothing is `FRAME_GRAPH_ERROR_ORDERING_TARGET_MISSING`; an optional one (`bRequired = false`) is dropped silently. A wildcard resolving to nothing is missing when required (Z2-R-GRAPH-064).

O(D log D).

### Step 2 resource keys and groups

1. Sort resources by `(m_ulKey, declaration index)`; equal keys are `FRAME_GRAPH_ERROR_DUPLICATE_RESOURCE_KEY`.
2. For each access, the node's group and the resource's group are equal, otherwise `FRAME_GRAPH_ERROR_CROSS_GROUP_ACCESS` and the access is dropped (Z2-R-GRAPH-069).
3. A resource is present in a subgraph's lowering exactly when a node of that subgraph accesses it; the same resource accessed from two subgraphs becomes two core resources, one per subgraph graph, each starting at version 0.

### Step 3 stage fences

For each (subgraph, group) (Z2-R-GRAPH-065):

1. Collect the distinct stages of its nodes and of its barriers, ascending: s_0 < s_1 < … < s_m.
2. Two barriers for one (subgraph, group, stage) are `FRAME_GRAPH_ERROR_DUPLICATE_STAGE_BARRIER`.
3. For each s_i that has a barrier or has a successor stage s_(i+1), generate the fence node `Sync.<Subgraph>.G<group>.S<s_i>`: kind `FRAME_NODE_KIND_SYNC_POINT`, affinity `JOB_AFFINITY_ANY`, cost 1, body = the barrier's `m_pfnRun`/`m_pData` or none.
4. Orderings: from every node of stage s_i to the fence, reason `stage <s_i> fence`; from the fence to every node of stage s_(i+1), reason `stage <s_i> -> <s_(i+1)>`.

The fences make stage order a chain with 2 × (nodes) edges instead of a quadratic bipartite set. A barrier at a stage with no node still produces its fence between the neighbouring stages.

### Step 4 ordering resolution

For each ordering record in declaration order and each target B it resolved to (A is the declaring node):

| Case | Result |
|---|---|
| `Before`, same group, stage(A) > stage(B) | `FRAME_GRAPH_ERROR_ORDERING_CONTRADICTS_STAGE`, message with `Before` |
| `After`, same group, stage(A) < stage(B) | `FRAME_GRAPH_ERROR_ORDERING_CONTRADICTS_STAGE`, message with `After` |
| same group, otherwise; or a different group | an explicit ordering `A → B` (Before) or `B → A` (After), reason `explicit Before` or `explicit After` |
| A == B | kept; the core reports `GRAPH_ERROR_SELF_ORDERING` |

Orderings to a node of a preceding stage are what [Z2-N-ARCH-404] forbids; orderings between stages in the stage direction are redundant with the fences and removed by transitive reduction (Z2-R-GRAPH-066).

### Step 5 version threading

For each subgraph, each resource it accesses, and each stage of the resource's group in ascending order (Z2-R-GRAPH-067, Z2-R-GRAPH-068):

1. Let W be the stage's writers of the resource. More than one is `FRAME_GRAPH_ERROR_UNORDERED_WRITERS` (all writers named, in declaration order); the resource's threading then stops for this subgraph.
2. Without a writer: every reader of the stage reads the current version v.
3. With the writer w: a reader r that has an explicit ordering `r → w` from step 4 (r declared `Before(w)`, or w declared `After(r)`) is a pre-reader; every other reader is a post-reader. Emit, in this order: each pre-reader `READ v`; w's access, `WRITE` (or `READ_WRITE` when it also reads) from v, producing v + 1; each post-reader `READ v + 1`. The current version becomes v + 1.
4. Readers and pre-readers are emitted in node declaration order.

Arbitration needs no special case: each writer source writes its own staging resource, the ECS-generated arbitration node reads every staging resource (post-reader of each) and read-writes the property (the stage's single writer), and the stage's other readers read its version.

The emitted access list, per resource, is in version order, which is the order the core requires (a write must name the latest version).

### Step 6 lowering into ZenithGraph

For each subgraph with at least one node and no jobs-level error (Z2-R-GRAPH-071):

1. Construct a `Zenith_Graph` in the lowering arena with `Zenith_TryConstruct` (it is ended with `Zenith_Destroy` before that arena is reset or destroyed, [Z2-N-CONTRACT-229]), name `CPUFrame/<Subgraph>`, serial from the lowering's counter (starting at 1), front-end tag `cpu`.
2. Nodes in canonical order: sorted by `(group, stage, declaration index)`, each stage's fence immediately after that stage's nodes. Core name: the declared name for group 0, `<name>@W<group>` otherwise (so diagnostics, zones and dumps are unambiguous across worlds). Lane: the affinity value (`JOB_AFFINITY_ANY` 0, `JOB_AFFINITY_MAIN` 1, `JOB_AFFINITY_RENDER_SUBMIT` 2). Cost: the measured cost when `m_auMeasuredCostsUs` matches the builder's node count, else the cost hint; fences cost 1. Flags: `GRAPH_NODE_FLAG_SIDE_EFFECT` on every node. Payload: the builder node index + 1, 0 for a fence.
3. Resources in `(group, key)` order, core name `<declared name>` (ECS already names them `<TypeName>.<property>@W<tag>`); persistent → `GRAPH_RESOURCE_FLAG_IMPORTED`; transient → no flag ([Z2-N-ARCH-346]); one subresource.
4. Accesses: resource by resource in the order of 3, each resource's emitted list from step 5, through `DeclareRead`/`DeclareWrite` with the handle of the version step 5 named.
5. Orderings: fence orderings by `(group, stage)`, then explicit orderings in declaration order.
6. The run table in core node order (`Zenith_FrameNodeRuntime` per node) and the builder-resource → core-resource map.

### Step 7 compile and wrap

Each lowered graph is compiled with `{ m_bCullUnusedNodes = false, m_bTransitiveReduction = options, m_uTransitiveReductionNodeLimit = 4096 }` into a temporary arena, with a `Zenith_GraphDiagnosticReport` per subgraph. Every entry of a failed subgraph's report becomes one `FRAME_GRAPH_ERROR_GRAPH` diagnostic in the builder's table, text `frame graph: <Subgraph>: <core text>` copied into the builder arena (the temporary arena is released afterwards), `m_uCoreCode` the core code, node A and B mapped to builder indices through the run table (fences map to `uGRAPH_INVALID_INDEX`). All diagnostics of steps 0 to 7 are sorted by `(code, node A, node B)`; the call returns `Zenith_FrameGraphError{ first code, count, combined structure hash }` and the texts stay readable through `GetDiagnostics()` (Z2-R-GRAPH-072).

## Schedule emission

`Zenith_AssembleFrameSchedule` is the CPU back end's emission from core graphs; the builder's compile path calls it after step 6, and the conformance CPU builder calls it directly with scenario graphs (Z2-R-GRAPH-073). Its input graphs live in an arena whose ownership moves into the schedule: `Zenith_FrameAssemblyDesc` carries `Zenith_Owned<Zenith_Arena> m_xGraphArena` and is passed by rvalue reference (`Zenith2/Docs/Design/GRAPH/PublicAPI.md` → "Zenith_FrameSchedule.h listing"). Its diagnostics go to the caller's `Zenith_FrameGraphDiagnosticReport`, formatted in the caller's diagnostic arena; the builder passes its own arena and copies the report into its diagnostic table.

### Run tables

Per subgraph, one `Zenith_FrameNodeRuntime` per core node, copied into the binding arena with names copied; the roots (dependency count 0) sorted by `(priority descending, index ascending)` are precomputed as a span, so a launch pushes them without sorting (Z2-R-GRAPH-074).

### Affinity counts and queue sizing

`m_auAffinityCount[e]` = Σ over the three subgraphs of scheduled nodes with lane e. `GetAffinityNodeCount(e)` returns it; `Zenith_FrameExecutor::SetSchedule` passes the main and render-submit counts to `Zenith_JobsReserveAffinityCapacity`, which sizes the main-affinity queue at 2 × count + 256 and the render-submit queue at 2 × count + 64 ([Z2-N-ARCH-392]), which the bound in `Zenith2/Docs/Design/GRAPH.md` → "Bounded queues and full policies" makes unreachable (Z2-R-GRAPH-091).

`m_uInjectedNodeCount` = the number of RENDER-subgraph nodes of lane `JOB_AFFINITY_ANY` that have at least one kept-edge predecessor of lane `JOB_AFFINITY_RENDER_SUBMIT` (computed after transitive reduction by one pass over the kept edges, O(E)); those are exactly the launches the render-submit thread pushes to the injection queue ([Z2-N-ARCH-390]); TICK and EPILOGUE contribute 0 because render-submit affinity is legal only in RENDER. `GetInjectedNodeCount()` returns it, and `SetSchedule` passes it to `Zenith_JobsReserveInjectionCapacity`, which grows the injection queue to `max(m_uInjectionCapacity, 2 × count + 256)`; each such node is pushed at most once per render instance, so the queue's fatal policy for that producer is unreachable (Z2-R-GRAPH-091).

### Transient scratch placement

Per subgraph, for the transients given a size (Z2-R-GRAPH-053):

1. Order them by `(bytes descending, core resource index ascending)`.
2. Number the T_acc distinct nodes that access a sized transient. Walk all N nodes in reverse topological order and give each node a bitset of T_acc bits: the accessors it reaches through the kept edges (the union of its successors' sets plus each successor that is an accessor). This is O(E · ⌈T_acc / 64⌉) time and O(N · T_acc / 8) bytes of scratch, because reachability still propagates through every node, not only through accessors.
3. Two transients conflict unless every accessor of one reaches every accessor of the other (in either direction); this implies their lifetimes are disjoint, and the lifetime test is kept only as a cheap pre-filter before the bitset test.
4. Place each transient first-fit at the lowest aligned offset where it overlaps no already-placed transient it conflicts with: O(T²) conflict checks in total.
5. The block size is the highest end offset, aligned to 64.

The reachability condition is [Z2-N-ARCH-337]'s: workers run unordered nodes concurrently, so disjoint positions alone do not prove non-overlapping execution. At run time the executor allocates one block per instance from the iteration arena; the tick instance's block is reused by each tick instance. Content is undefined at a transient's first access; wherever `ZENITH_ASSERTS_ENABLED=1` the launch fills the block with `0xCD` (a behaviour-only difference inside `Internal/`, no layout change, Z2-R-GRAPH-053). `GetTransientStorage` returns `{ block + offset, bytes }` through the binding's builder-resource map.

### Zone sites

One `Zenith_ZoneSite` per node, in the binding arena: `m_szName` the core node name, `m_szFile` and `m_uLine` the trampoline's own location, `m_uColour` by kind (system `0x4C8BF5`, behaviours `0xF5A623`, stage `0x7ED321`, arbitration `0xBD10E0`, sync point `0x9B9B9B`, tick boundary `0x50E3C2`) (Z2-R-GRAPH-118).

### Combined structure hash

As `Zenith2/Docs/Design/GRAPH.md` → "Canonical structure serialization": `Zenith_Hash64` over `ZFRAME01`, `uFRAME_LOWERING_VERSION` and the three subgraph hashes (36 bytes). The collision check of the cache compares the three graphs' canonical byte streams, obtained through `Zenith_GraphCanonicalBytes(const Zenith_Graph&, Zenith_Arena&)` (a ZenithGraph function returning the stream step 12 hashes, `Zenith_Result<Zenith_ByteSpan, Zenith_AllocError>`, as declared in `Zenith_Graph.h`).

## Schedule cache

### Structures and bindings

```cpp
// Engine/ZenithJobs/Internal/FrameGraph/Zenith_FrameGraphInternal.h
struct Zenith_FrameScheduleStructure                 // one priority generation of one structure
{
	Zenith_Owned<Zenith_Arena> m_xArena;               // owns core schedules (and, for a root, the graphs)
	Zenith_Graph* m_apxGraphs[uFRAME_SUBGRAPH_COUNT];           // constructed in the root's arena; ended with Zenith_Destroy
	Zenith_GraphSchedule m_axCore[uFRAME_SUBGRAPH_COUNT];
	Zenith_FrameTransientPlacement m_axPlacement[uFRAME_SUBGRAPH_COUNT];
	uint32 m_auAffinityCount[3];
	uint64 m_ulHash;
	Zenith_ByteSpan m_axCanonicalBytes[uFRAME_SUBGRAPH_COUNT];   // root only; in the root's arena; empty for an empty subgraph
	Zenith_FrameScheduleStructure* m_pxRoot;           // itself for a root
	Zenith_FrameScheduleStructure* m_pxNewer;          // the next generation, nullptr for the latest
	uint32 m_uBindingCount;
	uint64 m_ulLastAcquire;                           // recency counter for eviction
};
```

A `Zenith_FrameSchedule` is a binding: a pointer to one structure generation plus its own arena holding the run tables, zone sites, roots, the builder-resource map and the builder node count. A schedule returned by `Zenith_FrameGraphBuilder::Compile` or `Zenith_AssembleFrameSchedule` owns its structure (`Zenith_Owned` member); one handed out by the cache borrows it. Every structure generation borrows the graphs of its root, so the root outlives its generations (the cache destroys generations before their root). Destroying a root (eviction, cache destruction, or the destruction of a `Zenith_FrameSchedule` that owns its structure) destroys its generations first, then ends each non-null `m_apxGraphs[s]` with `Zenith_Destroy(*m_xArena, m_apxGraphs[s])`, then destroys `m_xArena`; a graph is never abandoned in an arena, because `Zenith_Graph` is not trivially destructible and the arena's destructor checks its live count ([Z2-N-CONTRACT-229], [Z2-N-CONTRACT-230]).

### Acquire

`Acquire(builder, options)` (Z2-R-GRAPH-076, Z2-R-GRAPH-077):

1. Clear the builder's diagnostic table. Create a lowering arena from the heap (`Zenith_TryNew<Zenith_Arena>`); translate and lower (steps 0 to 6) into it, with diagnostics in the builder's table (the builder's arena outlives the call, so the cache keeps no failure arena). On a jobs-level error end every graph constructed so far with `Zenith_Destroy`, then destroy the lowering arena, increment `m_ulFailedCompiles` and return `{ first code, count, 0 }`; the texts stay readable through the builder's `GetDiagnostics()`.
2. Compute the three canonical streams with `Zenith_GraphCanonicalBytes` into the lowering arena (an empty span for an empty subgraph) and the combined hash. Its only failure is `Zenith_AllocError`, which is `FRAME_GRAPH_ERROR_OUT_OF_MEMORY` (step `caching`) through the failure path below.
3. Find a root with that hash. If found and, for every subgraph s in `Zenith_FrameSubgraph` order, the new stream s and the root's `m_axCanonicalBytes[s]` have equal length and equal bytes (an empty and a non-empty stream never match): **hit** — create a binding over the root's latest generation from the new lowering's run tables and maps, end the three new graphs with `Zenith_Destroy`, destroy the lowering arena, increment hits, update the recency counter, return.
4. Equal hash with different bytes: count a collision and proceed as a miss.
5. **Miss** — compile the three graphs into a temporary arena (step 7; a failure puts the core diagnostics into the builder's table, ends the three graphs with `Zenith_Destroy`, destroys the temporary and lowering arenas and returns `{ FRAME_GRAPH_ERROR_GRAPH, count, combined hash }`), move each core schedule into the lowering arena with `Zenith_GraphReprioritise` using the declared costs, release the temporary arena, run emission, make the lowering arena the new root's arena, create a binding, increment misses, record the compile time.
6. If the cache now holds more roots than `m_uCapacity`, evict the least recently acquired root with no binding in any generation (with its generations); if every root is bound, keep the overshoot and count it.

A failure after step 1 (allocation) destroys everything this call created (each graph with `Zenith_Destroy` before its arena), records one `FRAME_GRAPH_ERROR_OUT_OF_MEMORY` diagnostic (step `caching`) in the builder's table and returns `{ FRAME_GRAPH_ERROR_OUT_OF_MEMORY, 1, combined hash }`; nothing previously handed out changes (Z2-R-GRAPH-079).

### Reprioritise

`Reprioritise(costs)` takes the executor's measured costs for the schedule it names (Z2-R-GRAPH-080):

1. Let G be that schedule's generation and L its root's latest generation.
2. Build a new generation: a fresh arena; per subgraph `Zenith_GraphReprioritise(L.m_axCore[s], costs[s], arena, xReport)` with a local report (its only failure is arena exhaustion, which is logged and returned as `{ FRAME_GRAPH_ERROR_OUT_OF_MEMORY, 1, hash }`); transient placement recomputed; affinity counts copied; hash unchanged; graphs borrowed from the root.
3. Link it as the root's latest (`L.m_pxNewer`), create a binding over it copying the run tables of the costs' schedule, and return it.

Edges, dependency counts and the structure hash never change; only priorities, order, positions, lifetimes and placement do.

### Release at iteration end

`ReleaseUnused(inUse)` runs on the main thread after each `RunFrame`, with the executor's `GetSchedulesInUse` output (current, pending, and the render schedule of the next iteration): every binding not listed is destroyed; every generation that is not its root's latest and has no binding is destroyed; eviction then applies as in Acquire step 6 (Z2-R-GRAPH-078).

### Cache invalidation triggers

The cache itself has no triggers: a trigger makes the composition root rebuild a builder and call `Acquire`, and structural equality decides hit or miss. The triggers are the specification's table (`Zenith2/Docs/Architecture/CPUFrameGraph.md` → "Schedule compilation and caching"):

| Trigger | Who calls | Expected outcome |
|---|---|---|
| world created or destroyed, world-transition swap | ZenithRuntime at the start of the frame loop's recompile step | miss (new groups) or hit (a world of the same structure replaced) |
| script system hot-reloaded with a changed access set, phase or system set | queued by ZenithScript; applied in frame-prologue step 6, and the recompile runs in prologue step 8, before the frame's first tick boundary | miss; an unchanged declaration is a hit ([Z2-N-SCRIPT-153]) |
| component schemas re-registered | the editor between play sessions | miss |
| module set changed | startup | first compile |
| render view set changed shape | ZenithFlux marks dirty | miss or hit |
| every `m_uReprioritiseIntervalFrames` frames (256) | ZenithRuntime when `IsReprioritiseDue()` | `Reprioritise`, no structure change |
| `jobs.recompile` | development console | forced miss (the cache is bypassed for that call) |

Disabling a failing system never reaches the cache: its node stays and completes without running its body, through the ECS trampoline ([Z2-N-ARCH-410], Z2-R-GRAPH-081).

## Executor state machines

### Frame executor iteration state machine

One `Zenith_FrameExecutor` walks this machine once per `RunFrame` (Z2-R-GRAPH-093 to Z2-R-GRAPH-100). Events are raised by the main thread (RunFrame, SetSchedule, RequestCancel) or by the thread that completes an instance.

| State | Event | Next state | Action |
|---|---|---|---|
| `EXECUTOR_STATE_IDLE` | `SetSchedule(s)` | `EXECUTOR_STATE_IDLE` | store `s` as pending (replacing an earlier pending one); call `Zenith_JobsReserveAffinityCapacity(jobs, s.GetAffinityNodeCount(JOB_AFFINITY_MAIN), s.GetAffinityNodeCount(JOB_AFFINITY_RENDER_SUBMIT))` and `Zenith_JobsReserveInjectionCapacity(jobs, s.GetInjectedNodeCount())` |
| `EXECUTOR_STATE_IDLE` | `RunFrame(p)` with no current and no pending schedule | — | `ZENITH_CHECK` failure |
| `EXECUTOR_STATE_IDLE` | `RunFrame(p)` | `EXECUTOR_STATE_RUNNING` | swap pending into current under `uLOCK_LEVEL_JOBS_SCHEDULE` (render keeps the previous epilogue's schedule); reset the iteration arena; stamp `m_ulIterationStartNs`; set parts = 1 + (render runs ? 1 : 0); launch TICK(k,1) when T > 0, else stamp simulation end and launch EPILOGUE(k); and, if `m_bRunRender` and a previous epilogue ran, RENDER(k−1); enter `Zenith_JobsRunMainUntil` (the frame prologue ran on the main thread before `RunFrame`, [Z2-N-ARCH-437]) |
| `EXECUTOR_STATE_RUNNING` | TICK(k,t) complete, t < T | `EXECUTOR_STATE_RUNNING` | reinitialise the tick instance; launch TICK(k,t+1) |
| `EXECUTOR_STATE_RUNNING` | TICK(k,T) complete | `EXECUTOR_STATE_RUNNING` | stamp simulation end; launch EPILOGUE(k) |
| `EXECUTOR_STATE_RUNNING` | EPILOGUE complete, cancel flag clear (acquire load by the completing thread) | `EXECUTOR_STATE_RUNNING` or `EXECUTOR_STATE_DRAINING` | stamp epilogue end; `m_bEpilogueSealed = true` (frame k is rendered next iteration); `m_bResyncRenderWorld = false` (this seal consumed any pending resync); decrement parts; if parts reaches 0, go to `EXECUTOR_STATE_DRAINING` |
| `EXECUTOR_STATE_RUNNING` | EPILOGUE complete, cancel flag set | as above | leave `m_ulEpilogueEndNs` 0; `m_bEpilogueSealed = false` (frame k is never rendered, even when no body was skipped because `Render.Seal` returned early at its cancel check); decrement parts; if 0, `EXECUTOR_STATE_DRAINING` |
| `EXECUTOR_STATE_RUNNING` | RENDER complete | as above | stamp render end; decrement parts; if 0, `EXECUTOR_STATE_DRAINING` |
| `EXECUTOR_STATE_DRAINING` | (the completing thread) | `EXECUTOR_STATE_DRAINING` | store the iteration-done flag (release) and `Zenith_JobsWakeMain` |
| `EXECUTOR_STATE_DRAINING` | main thread returns from `Zenith_JobsRunMainUntil` | `EXECUTOR_STATE_IDLE` | stamp iteration end; publish the timeline, timings and counters; update the cost EWMA; if the cancel flag is set, set `m_bResyncRenderWorld`; clear the cancel flag; return from `RunFrame` |
| `EXECUTOR_STATE_RUNNING` | `RequestCancel` | `EXECUTOR_STATE_RUNNING` | store the cancel flag (release); every node not yet started skips its body; the remaining transitions happen unchanged, with no further TICK instance launched after the current one (the epilogue still runs, with every body skipped) |
| any | destructor while `EXECUTOR_STATE_RUNNING` | — | `ZENITH_CHECK` failure |

### Subgraph instance state machine

| State | Event | Next state | Action |
|---|---|---|---|
| `INSTANCE_STATE_UNUSED` | launch | `INSTANCE_STATE_RUNNING` | copy `m_auDependencyCount` into the node instances' pending counters (relaxed stores, published by the push's release); remaining = scheduled node count; assign the instance serial; push the roots (nodes with count 0) sorted by (priority descending, index ascending) through `Zenith_JobsPushNodes` |
| `INSTANCE_STATE_RUNNING` | node complete, remaining > 1 after decrement | `INSTANCE_STATE_RUNNING` | decrement successors; push the successors that reached 0 |
| `INSTANCE_STATE_RUNNING` | node complete, remaining reaches 0 | `INSTANCE_STATE_COMPLETE` | raise the instance-complete event of the iteration machine on this thread |
| `INSTANCE_STATE_RUNNING` | launch of an instance with no scheduled node | `INSTANCE_STATE_COMPLETE` | raise instance-complete immediately on the launching thread |
| `INSTANCE_STATE_COMPLETE` | reinitialise (tick t+1) | `INSTANCE_STATE_UNUSED` | none; the next launch reuses the storage |

### Node instance state machine

| State | Event | Next state | Action |
|---|---|---|---|
| `NODE_STATE_WAITING` | pending counter reaches 0 | `NODE_STATE_READY` | the decrementing thread collects the node for its push batch |
| `NODE_STATE_READY` | a thread takes the job | `NODE_STATE_RUNNING` | trampoline: stamp start, assert affinity and the float environment, open the zone |
| `NODE_STATE_RUNNING` | cancel flag set before the body starts | `NODE_STATE_BODY_DONE` | skip the body; count `frame.nodes_skipped_cancelled` |
| `NODE_STATE_RUNNING` | body returns | `NODE_STATE_BODY_DONE` | close the zone; JOBS now joins sub-jobs and runs continuations |
| `NODE_STATE_BODY_DONE` | JOBS calls `m_pfnOnComplete` | `NODE_STATE_COMPLETE` | stamp end; decrement successors; decrement the instance's remaining count |

### Schedule cache entry state machine

| State | Event | Next state | Action |
|---|---|---|---|
| (absent) | `Acquire` miss | `CACHE_ENTRY_STATE_LIVE` | compile the structure, create a binding; if the cache holds `m_uCapacity` structures, evict the least recently acquired structure with no live binding (a structure pinned by a binding is never evicted; if every one is pinned, the cache temporarily exceeds capacity by one and counts `frame.schedule_cache.over_capacity`) |
| `CACHE_ENTRY_STATE_LIVE` | `Acquire` hit | `CACHE_ENTRY_STATE_LIVE` | create a new binding over the structure; update the recency counter |
| `CACHE_ENTRY_STATE_LIVE` | `Reprioritise` | `CACHE_ENTRY_STATE_LIVE` | create a child structure (new core schedules, borrowed graphs) pinned to its parent, and a binding over it |
| `CACHE_ENTRY_STATE_LIVE` | `ReleaseUnused` omits all of its bindings | `CACHE_ENTRY_STATE_IDLE` | destroy those bindings; the structure stays cached |
| `CACHE_ENTRY_STATE_IDLE` | eviction | (absent) | destroy the structure as § "Structures and bindings" orders it: its generations (which cannot be pinned when it is idle), then `Zenith_Destroy` of its three graphs, then its arena |

## Frame executor

### Executor storage

```cpp
struct Zenith_FrameNodeInstance                   // 16 bytes
{
	std::atomic<uint32> m_uPending;              // remaining predecessors
	uint32 m_uNode;                              // core node index
	Zenith_FrameSubgraphInstance* m_pxInstance;
};

struct Zenith_FrameSubgraphInstance               // 80 bytes plus arrays: 7 pointers (56), two atomics (8), two uint8 padded to 8, the executor pointer (8)
{
	const Zenith_FrameSchedule* m_pxSchedule;
	const Zenith_GraphSchedule* m_pxCore;
	Zenith_FrameNodeInstance* m_axNodes;          // one per core node index
	uint64* m_aulStartNs;                        // per node, relative to the iteration start
	uint64* m_aulEndNs;
	uint32* m_auWorker;
	uint8* m_pTransientBlock;
	std::atomic<uint32> m_uRemaining;
	std::atomic<uint32> m_uSkippedBodies;       // nodes of this instance that skipped their body (a counter only; sealed is decided by the cancel flag)
	Zenith_FrameSubgraph m_eSubgraph;
	uint8 m_uOrdinal;                            // 1 render, 2 epilogue, 2 + t for tick t (t = 1 … 253)
	Zenith_FrameExecutor* m_pxExecutor;
};
static_assert(sizeof(Zenith_FrameSubgraphInstance) == 80);
```

The executor holds (every member, no global, Z2-D-046): the job system and heap references; the descriptor; the schedule mutex (`uLOCK_LEVEL_JOBS_SCHEDULE`); pending, current and render-schedule pointers and the have-epilogue flag; `bool m_bEpilogueSealed` (written by the epilogue's completing thread before its parts decrement, read by the main thread in step 8 after the iteration-done acquire); `bool m_bResyncRenderWorld` (false at creation; cleared by a sealed epilogue's completing thread, set by the main thread in step 8, read by the main thread through `IsRenderWorldResyncPending()` between iterations; every access is ordered by the instance and iteration-done chains, so it needs no atomic); the iteration arena (`Zenith_Owned<Zenith_Arena>`); the three instance records (tick, epilogue, render); the atomics `m_uIterationParts`, `m_uIterationDone`, `m_uCancel`, `m_uIterationRunning`; the frame index; the timeline ring (`m_uTimingRingFrames` entries, `QUEUE_FULL_POLICY_DROP_OLDEST`); the last frame's timings per subgraph and the cost EWMA per subgraph (heap buffers `Create` reserves for 256 nodes per subgraph, the `FailsAtCostBuffers` step, and the swap grows when a schedule has more); the counters; the stats.

Iteration-arena size, computed at each swap from the current schedule and the render schedule: Σ over tick (once) and epilogue of the current schedule and render of the render schedule of (`sizeof(Zenith_FrameSubgraphInstance)` + `sizeof(Zenith_FrameNodeInstance)` × N_s + 20 × N_s timing bytes + transient block + 64 alignment slack), plus 4 KiB; `m_ulMaxBytes` is exactly that, and a growth reallocates the arena (Z2-R-GRAPH-117, Z2-R-GRAPH-122).

### RunFrame step by step

`RunFrame(p)` on the main thread (Z2-R-GRAPH-093, Z2-R-GRAPH-113):

1. `ZENITH_CHECK`: state idle; a current or pending schedule exists; `p.m_ulFrameIndex` is the previous + 1 (any value on the first call); `p.m_uTickCount` ≤ 253 (the 8-bit ordinal: 2 + T ≤ 255; Z2-B-JOBS-002's range 1 to 64 stays below it, Z2-R-GRAPH-104).
2. Under the schedule mutex: if a schedule is pending, make it current and clear pending (the affinity and injection capacities were already reserved by `SetSchedule` through `Zenith_JobsReserveAffinityCapacity` and `Zenith_JobsReserveInjectionCapacity`); resize the iteration arena, the timing buffers and the EWMA if the node counts changed (the EWMA is kept when the new schedule's root structure equals the old one's, reset otherwise); reset the frames-since-swap counter.
3. `Reset()` the iteration arena and carve the instance records.
4. `renderRuns = p.m_bRunRender && m_bHaveEpilogue`; `m_uIterationParts = 1 + renderRuns`; `m_uIterationDone = 0`.
5. `Zenith_JobsBeginIteration(jobs, k)`. Store `m_uIterationRunning = 1` (release). Stamp the iteration start; with the current schedule launch TICK(k,1) when T > 0, otherwise stamp the simulation end (equal to the iteration start) and launch EPILOGUE(k); if `renderRuns`, launch RENDER(k − 1) with the render schedule; all from the main thread (pusher null).
6. `Zenith_JobsRunMainUntil(jobs, m_uIterationDone)`: the main thread runs main-affinity jobs and helps with frame work until done.
7. `Zenith_JobsEndIteration(jobs)`. Stamp the iteration end; publish the timeline into the ring, the timings, the counters (§ "Timing capture and cost measurement"); update the EWMA.
8. Store `m_uIterationRunning = 0`. The render schedule for the next iteration becomes the current schedule (the epilogue that just ran used it); `m_bHaveEpilogue = m_bEpilogueSealed` (an epilogue that completed under a set cancel flag left render world k unsealed, so RENDER(k) is not launched in iteration k+1, [Z2-N-ARCH-417]); if `m_uCancel` (acquire) is set, set `m_bResyncRenderWorld = true`; then clear `m_uCancel`; increment the frames-since-swap counter; return.

### Launching a subgraph instance

`Launch(instance, schedule, subgraph, ordinal, pusher)`:

1. Bind the core schedule; `N_sched = m_auOrder.GetCount()`.
2. If `N_sched == 0`, run the instance-complete path immediately on this thread.
3. For each scheduled node n: `m_uPending = m_auDependencyCount[n]` (relaxed); clear its timing slot. Wherever `ZENITH_ASSERTS_ENABLED=1`, fill the transient block with `0xCD`.
4. `m_uRemaining = N_sched` (relaxed; published by the push).
5. Build `Zenith_JobNodeLaunch` entries for the precomputed roots and push them in batches of 64 through `Zenith_JobsPushNodes`.

### The node trampoline

`Zenith_FrameNodeTrampoline(ctx, pNodeInstance)` (Z2-R-GRAPH-088, Z2-R-GRAPH-089):

1. `t0 = Zenith_GetMonotonicTimeNs()`; record start and `ctx.GetWorkerIndex()`.
2. `ZENITH_CHECK(m_uIterationRunning.load(acquire) == 1)` (no frame-graph job starts between iterations, the GRAPH half of `Test_FrameGraph_PrologueRunsNoJobs`; `RunFrame` stores 1 before its first launch and 0 after `Zenith_JobsRunMainUntil` returns). `ZENITH_CHECK` the affinity: `JOB_AFFINITY_MAIN` needs worker index 0; `JOB_AFFINITY_RENDER_SUBMIT` needs `ctx.GetWorkerCount() + 1`; `Zenith_JobsAssertFloatEnvironment()`.
3. If `m_uCancel` (acquire) is set: increment the instance's `m_uSkippedBodies` (relaxed; read by the completing thread after the instance's acq_rel remaining-count chain) and the `frame.nodes_skipped_cancelled` counter, and return (the completion path still runs).
4. If the run has a body: begin the zone (its site), call `m_pfnRun(ctx, m_pData)`, end the zone. A fence without a barrier has no body.

The trampoline never waits; a body that needs its sub-jobs' results registers a continuation ([Z2-N-ARCH-398], Z2-R-GRAPH-087).

### Node completion

`Zenith_FrameNodeComplete(ctx, pNodeInstance)`, called by JOBS once the body, its sub-jobs and its continuations have all finished (Z2-R-GRAPH-084, Z2-R-GRAPH-086):

1. Record the end stamp.
2. For each kept successor s of n (CSR range): `if (m_axNodes[s].m_uPending.fetch_sub(1, std::memory_order_acq_rel) == 1)` append s to a stack array of 64 launches; when full, sort it and push it, then continue.
3. Sort the remaining launches by `(priority descending, index ascending)` (insertion sort; R ≤ 64) and push them with `pusher = &ctx`.
4. `if (m_uRemaining.fetch_sub(1, std::memory_order_acq_rel) == 1)` run the instance-complete path on this thread.
5. Add the path's duration to `frame.dispatch_ns` (relaxed atomic add).

### Instance completion and sequencing

| Completed | Cancel flag clear | Cancel flag set |
|---|---|---|
| TICK(k,t) | t < T: launch TICK(k,t+1) (same storage); t = T: stamp simulation end, launch EPILOGUE(k) | stamp simulation end; launch EPILOGUE(k) |
| EPILOGUE(k) | stamp its end; the epilogue is sealed and clears `m_bResyncRenderWorld`; then the parts decrement below | the epilogue is unsealed whether or not any body skipped (its end stamp stays 0); then the parts decrement below |
| RENDER(k−1) | stamp its end | same (skipped render bodies submit nothing; the screen keeps the last presented image) |
| either part | `if (m_uIterationParts.fetch_sub(1, acq_rel) == 1) { m_uIterationDone.store(1, release); Zenith_JobsWakeMain(jobs); }` | same |

Tick instances therefore run strictly in sequence ([Z2-N-ARCH-383]), phase 12 runs once per frame even when T is 0 ([Z2-N-ARCH-384]), and the launch of the next instance happens on the thread that completed the previous one, with no wait (Z2-R-GRAPH-094).

### Push order and priority

Every push batch is sorted by `(priority descending, index ascending)`, and `Zenith_JobsPushNodes` guarantees that the consumer's next take is the batch's first element for its queue (`Zenith2/Docs/Design/GRAPH.md` → "The executor to job-system seam"). So among nodes made ready together, the one on the longest remaining path is taken first ([Z2-N-ARCH-411], Z2-R-GRAPH-092). Priority changes wall-clock order only.

### Invocation IDs

`m_ulInvocationID = ((frame index & 0x3FFFFFFFF) << 28) | (ordinal << 20) | core node index`: the frame index truncated to 34 bits (bits 28 to 61), the instance ordinal (8 bits: render 1, epilogue 2, tick t → 2 + t; it starts at 1, so the ID is never 0, and T ≤ 253 keeps it within 8 bits) and the core node index (20 bits, below `uGRAPH_MAX_NODES`). Bits 62 and 63 are clear, as the JOBS seam requires; the ID is unique per node invocation within 2^34 frames (about 9 years at 60 Hz) and identical on every run with the same frame and tick sequence (Z2-R-GRAPH-104).

### Timing capture and cost measurement

- Timing capture is on when `m_bCaptureTimings` (default) and costs one monotonic read at trampoline entry and one at completion per node.
- After each iteration: `GetLastTimings(s)` returns the last instance's per-node `{node, worker, start, end}` for subgraph s; `GetLastTimeline()` the timeline; the ring keeps the last `m_uTimingRingFrames` timelines and drops the oldest.
- EWMA per subgraph per node, fixed point with 8 fractional bits: `ewma += (sample_us × 256 − ewma) / 32` (integer arithmetic on `sint64`), where the sample is `(end − start) / 1000` (the tick subgraph uses the mean over the frame's T instances); nodes that did not run (T = 0, cancelled, not scheduled) keep their value; the initial value is the declared cost × 256. `GetMeasuredCosts()` returns `max(1, ewma >> 8)` per node.
- `IsReprioritiseDue()` is true when `m_uReprioritiseIntervalFrames > 0` and at least that many frames ran since the last swap (Z2-R-GRAPH-080).

### Cancellation path

`RequestCancel` stores `m_uCancel = 1` (release). The trampoline skips every body it starts afterwards, the completion path runs unchanged, no further tick instance is launched, the epilogue and a running render instance complete with skipped bodies, and `RunFrame` returns; step 8 sets the resync flag and clears the cancel flag.

A frame whose epilogue completed under a set cancel flag is never rendered. The epilogue's completing thread loads `m_uCancel` (acquire) before its parts decrement; set means unsealed, whether bodies were skipped (they started after the store) or a running body returned early at its cancel check (for example `Render.Seal`, which runs after every extractor, so no body after it remains to be skipped). Step 8 leaves `m_bHaveEpilogue` false: iteration k+1 launches no RENDER(k), its `m_uPipelineDepth` is 0 and `m_ulRenderedFrameIndex` is 0, and the render schedule pointer still advances. A cancel that lands after the epilogue completed (only render bodies are cut short) leaves world k sealed, and iteration k+1 renders it (`Test_FrameExecutor_CancelledFrameIsNotRendered`, Z2-R-GRAPH-115).

Interaction with the two-slot ring (`Zenith2/Docs/Rendering.md` → "The render world hand-off"): RENDER(k) reads slot k % 2 only when EPILOGUE(k) completed with the cancel flag clear. A slot left in any other state (untouched, half-written by the extractors that ran, written but unsealed) is read by no one and is reset at the start of the epilogue that next reuses it, EPILOGUE(k+2) ([Z2-N-ARCH-418]); iteration k+1's epilogue writes slot (k+1) % 2 as usual.

Render-world resync (Z2-R-GRAPH-115). Because a render world carries deltas ([Z2-N-REND-008]), a world that is never consumed drops its deltas, and a render instance cut short may have consumed its world only in part. So:

1. Set: in step 8 of any iteration in which the cancel flag was set, the main thread sets `m_bResyncRenderWorld = true` (before clearing the flag). This covers an unsealed epilogue and a render instance cut short under a sealed one.
2. Read: between iterations, on the main thread, through `IsRenderWorldResyncPending()`. The composition root hands the value to Rendering's render-world writer before `RunFrame(k+1)`, as it hands over the frame index to render (§ "Hand-off to Flux_RenderWorld" step 5), and the extract nodes of EPILOGUE(k+1) read it through the writer and perform a full-state extraction instead of a delta. Rendering documents that full-extraction path, shared with device-loss recovery (`Zenith2/Docs/Rendering/GpuResources.md` → "Device loss"; CROSS_DOC_ITEMS X57).
3. Clear: at seal. The completing thread of an epilogue that completes with the cancel flag clear stores `m_bResyncRenderWorld = false`. An epilogue that is itself cancelled clears nothing, and step 8 sets the flag again, so the resync moves to the next epilogue that seals.
4. Ring: the full-state world is written into its own slot after that slot's reset like any delta world; RENDER of it, in the next iteration, rebuilds the GPU scene from it, and each world after it is a delta against it. The slot capacity a full-state world needs is Rendering's (X57).

The invariant this keeps: the render world a RENDER instance consumes is the delta successor of the world the previous RENDER instance consumed, or a full-state extraction. When a cancel cuts RENDER(k−1) short while EPILOGUE(k) seals, world k (a delta against world k−1) is still rendered in iteration k+1. Whether the GPU-scene update of RENDER(k−1) can return early at its cancel check is Rendering's (X57); if it can, the divergence lasts for that one frame, and the full-state world k+1 repairs it in iteration k+2.

`Test_FrameExecutor_CancelResyncsRenderWorld` covers all four steps. A cancel during TICK(k) leaves `IsRenderWorldResyncPending()` true after `RunFrame(k)`. A probe extractor in EPILOGUE(k+1) records the writer's full-extraction mode as true, and the flag is false after `RunFrame(k+1)`. A second cancel during EPILOGUE(k+1) keeps the flag true until the first uncancelled epilogue. A cancel that lands while RENDER(k−1) runs after EPILOGUE(k) sealed also sets it. Twenty uncancelled frames never set it.

The launch also passes `&m_uCancel` as `m_puCancel`, so running bodies see `IsCancelled()` (Z2-R-GRAPH-115).

## Render-extraction pipelining

### The iteration barrier

After the frame prologue (main-thread steps before `RunFrame`, [Z2-N-ARCH-437]), iteration k runs TICK(k,1…T), EPILOGUE(k) and, concurrently, RENDER(k − 1); `RunFrame` returns only when both the epilogue part and the render part are done, and iteration k + 1 starts only in the next `RunFrame`. So RENDER(k − 1) and the simulation of k overlap with no edge and no lock, and simulation is never more than one frame ahead of rendering ([Z2-N-ARCH-385], [Z2-N-ARCH-386], Z2-R-GRAPH-095, Z2-R-GRAPH-096).

### Hand-off to Flux_RenderWorld

GRAPH's side of the hand-off (`Zenith2/Docs/Rendering.md` → "Purpose and scope"):

<!-- assumption: pending MOD -->
1. The extraction nodes (EPILOGUE stage 12 of each world) and `Render.Seal` (EPILOGUE stage 13, group 0, declared by MOD) are EPILOGUE nodes ([Z2-N-ARCH-439]). The generated orderings of [Z2-N-ARCH-439] are declared by `Zenith_WorldSimulation::EmitInto`: each world's extractor nodes declare `Before("Render.Seal@W0", true)` (group 0 is addressable as `@W0` under § "Step 1 node names and qualified references"), so the seal completes after every extractor and after no epilogue stage. MOD, the composition root, also tells Flux the frame index k − 1 to render before calling `RunFrame(k)` (step 4), calls `Zenith_FrameScheduleCache::Acquire` inside the zone `Frame.ScheduleRecompile` in prologue step 8, calls `ReleaseUnused(GetSchedulesInUse())` after every `RunFrame`, and calls `Reprioritise` then `SetSchedule` when `IsReprioritiseDue()`.
2. RENDER(k − 1) is launched in iteration k with the schedule whose epilogue produced render world k − 1, so a schedule swap between iterations never pairs a render subgraph with a render world built under another schedule (Z2-R-GRAPH-100).
3. GRAPH places no edge between EPILOGUE(k) and RENDER(k − 1); the ring rule of `uRENDER_WORLD_RING_SIZE` = 2 (epilogue k writes slot k % 2, render reads (k − 1) % 2, the slot reset happens at the start of the epilogue that reuses it) is what keeps them apart, and the iteration barrier is what makes the reset safe: RENDER(k − 2) finished in iteration k − 1.
4. The composition root tells Flux the frame index k − 1 to render before calling `RunFrame(k)`; the executor records it as `m_ulRenderedFrameIndex` when RENDER(k − 1) is launched, and `m_ulRenderedFrameIndex` is 0 in any iteration in which no RENDER instance ran (the first frame, headless, device loss, the iteration after an unsealed epilogue).
5. Before calling `RunFrame(k)`, the composition root also hands `IsRenderWorldResyncPending()` to Rendering's render-world writer, and EPILOGUE(k)'s extract nodes read it through the writer (§ "Cancellation path"; CROSS_DOC_ITEMS X57).
<!-- /assumption -->

### Frame timeline and latency stamps

`Zenith_FrameTimeline` is published every frame (Z2-R-GRAPH-097, Z2-R-GRAPH-098): `m_uPipelineDepth` is 1 exactly when RENDER ran (the counter `pipeline.depth`), and the stamps order as iteration start ≤ simulation end ≤ epilogue end ≤ iteration end and iteration start ≤ render start ≤ render end ≤ iteration end. RPROD combines them with its submit and present stamps and the input sequence carried in `Flux_RenderWorld` for the input-latency attribution ([Z2-N-ARCH-422]).

### Headless and first-frame behaviour

With `m_bRunRender = false` (headless configurations, the first frame, device loss) no RENDER instance is launched, `m_uIterationParts` is 1 and `m_uPipelineDepth` is 0; the render schedule pointer still advances, so re-enabling render renders the last extracted frame (Z2-R-GRAPH-099). Those are the three uses of `m_bRunRender = false`, and none of them needs the resync flag: a headless run never re-enables render, the first frame has no earlier world, and device loss re-extracts full state through Rendering's own path (`Zenith2/Docs/Rendering/GpuResources.md` → "Device loss").

## Fixed-order reduction

`Zenith_FixedOrderReduceSerial` and `Zenith_FixedOrderReduceParallel` implement the tree of [Z2-N-ARCH-704] over elements already sorted in merge order (Z2-R-GRAPH-105):

1. Level 0 is the input: n elements.
2. Level l + 1 has ⌈n_l / 2⌉ elements: element i = `combine(level_l[2i], level_l[2i + 1])` for i < ⌊n_l / 2⌋; when n_l is odd, the last element is `level_l[n_l − 1]` copied unchanged.
3. The result is the single element of the last level.

The serial form walks the levels on the calling thread with two scratch buffers from the node arena. The parallel form, per level with at least `m_uParallelGrain` pairs, runs `ParallelFor` over the pairs and registers the next level as a continuation (smaller levels run inline in the continuation); the final continuation calls `pfnDone`. Every output element depends only on its pair, so both forms produce identical bytes for any worker count and any chunk-to-worker assignment.

## Determinism proof obligations

The determinism guarantee (Z2-D-019, `Zenith2/Docs/Architecture.md` → "Determinism guarantee and mechanisms") rests on these GRAPH obligations, each with its evidence (Z2-R-GRAPH-102 to Z2-R-GRAPH-105, Z2-R-GRAPH-050):

| # | Obligation | Why it holds | Evidence |
|---|---|---|---|
| P1 | identical declarations compile to byte-identical schedules | every step iterates in declaration or canonical order, sorts are stable with total keys, no address or hash-bucket order is used | `Test_GraphConformance_OrderIsDeterministic`, `Test_FrameGraph_LoweringCanonicalOrder` |
| P2 | every pair of conflicting accesses is ordered by a path | hazard derivation's RAW, WAR and WAW rules over overlapping subranges; reduction keeps reachability | `Test_GraphConformance_RandomDagProperty` (brute-force reference) |
| P3 | a node's successors observe all of its writes, including its sub-jobs' and continuations' | completion is signalled by JOBS after the join, and the dependency decrement is acquire-release | `Test_FrameGraph_SuccessorSeesSubJobWrites`, `Test_FrameGraph_MergeContinuationBeforeSuccessor` |
| P4 | same-stage writers never race | two writers of one resource in one stage are a compile error; arbitration merges through one node | `Test_FrameGraph_UnorderedWriters_IsError` |
| P5 | priorities and measured costs change only wall-clock order | costs are excluded from the structure hash and never change edges; P2 makes the result of every order equal | `Test_FrameGraph_Determinism_CostPerturbation` |
| P6 | results do not depend on the worker count or on stealing | P2 and P3; node bodies' merges use merge order (ECS) or `Zenith_FixedOrderReduce` | `Test_FrameGraph_Determinism_WorkerCounts`, `Test_FixedOrderReduce_IdenticalAcrossWorkerCounts` |
| P7 | tick instances are sequential and phases are ordered within a tick | the instance sequencing and the stage fences | `Test_FrameExecutor_TicksSequential`, `Test_FrameGraph_StageBarrier_OrdersStages` |
| P8 | invocation IDs are identical across runs | the formula of § "Invocation IDs" | `Test_FrameExecutor_InvocationIDsDeterministic` |
| P9 | the render subgraph never reads simulation state | the render world holds copies only, and there is no edge from simulation to render within an iteration | RPROD's `Test_RenderWorld_HoldsNoSimulationPointer`; `Test_FrameGraph_PipelinedRender_OverlapsSimulation` |

Merge order itself (system order, logical invocation ID, sequence) is the ECS model's (`Zenith2/Docs/Architecture/ECSModel.md` → "Command buffers"); GRAPH's part is that a system's merge step runs as its node's final continuation or on the stage's sync fence, both of which the executor completes before any successor starts.
