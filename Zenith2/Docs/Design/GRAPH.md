# GRAPH design — ZenithGraph shared core and the CPU frame graph

This is the design document of area GRAPH (`Z2 GRAPH — Graph core & CPU frame graph`). It elaborates the specification in `Zenith2/Docs/Architecture/ZenithGraph.md` and `Zenith2/Docs/Architecture/CPUFrameGraph.md` into what a worker needs to write the code and its tests: the library and file layout, the complete public headers, the internal data structures, every algorithm step by step, the error, threading and lifetime contract, the diagnostics, the tests, the budget rows and the stage plan. Obligations in this document are traceable to the requirement catalogue `Zenith2/Docs/Requirements/GRAPH.md`; each one names its `Z2-R-GRAPH-NNN`.

A line `<!-- elaboration -->` marks a detail this document designs beyond the specification. A block `<!-- assumption: pending <AREA> -->` states an interface this design assumes from a design document that does not exist yet; each one has a row in the cross-area notes and is reconciled by review.

Document family:

| File | Holds | Its `##` headings |
|---|---|---|
| `Zenith2/Docs/Design/GRAPH.md` (this file) | the skeleton sections, the public headers, the error contract, the formats, the stage plan | the skeleton of the A3 brief, in order |
| `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` | ZenithGraph internals: declaration tables, the thirteen compile steps, reprioritisation, JSON writing, diagnostic text | Internal file map · Declaration tables · Declaration algorithms · Compile pipeline · Reprioritisation · JSON writing · Diagnostic text formats · Complexity and memory summary |
| `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` | the CPU front end and back end inside ZenithJobs: translation, emission, the schedule cache, the frame executor, pipelining, the fixed-order reduction, the determinism obligations | Internal file map · Builder storage · Translation pipeline · Schedule emission · Schedule cache · Executor state machines · Frame executor · Render-extraction pipelining · Fixed-order reduction · Determinism proof obligations |
| `Zenith2/Docs/Design/GRAPH/PublicAPI.md` | the listing of `Zenith_FrameSchedule.h` (moved out of the entry for length) | Zenith_FrameSchedule.h listing |
| `Zenith2/Docs/Design/GRAPH/Formats.md` | the JSON Schemas of the graph dump and the frame dump | Graph dump JSON Schema · Frame dump JSON Schema |
| `Zenith2/Docs/Design/GRAPH/Tests.md` | every test, conformance scenario, benchmark and fixture of the area, with its assertion | Test files · Graph core unit tests · Graph conformance suite · CPU frame graph tests · Benchmarks · Fixtures and golden files |

## Purpose and scope

### What this area owns

GRAPH owns the one dependency-graph engine of Zenith2 and the CPU frame graph built on it (Z2-D-006). Concretely:

| Piece | Library | Location | Inventory items |
|---|---|---|---|
| `Zenith_Graph`: node and resource declarations with access modes and subranges, versioned resource handles, explicit orderings, outputs | ZenithGraph | `Engine/ZenithGraph/Zenith_Graph.h`, `Engine/ZenithGraph/Internal/Zenith_GraphDeclare.cpp` | Z2-I-GRAPH-01, Z2-I-GRAPH-03 |
| hazard derivation (RAW, WAR, WAW over the version chain and overlapping subranges), culling | ZenithGraph | `Engine/ZenithGraph/Internal/Zenith_GraphHazards.cpp`, `Zenith_GraphIntervalMap.cpp`, `Zenith_GraphCull.cpp` | Z2-I-GRAPH-02 |
| Kahn order with critical-path priority, transitive reduction, dependency counts, reprioritisation, structure hash | ZenithGraph | `Engine/ZenithGraph/Internal/Zenith_GraphOrder.cpp`, `Zenith_GraphReduce.cpp`, `Zenith_GraphHash.cpp` | Z2-I-GRAPH-04 |
| cycle detection and readable diagnostics | ZenithGraph | `Engine/ZenithGraph/Internal/Zenith_GraphCycles.cpp`, `Zenith_GraphText.cpp` | Z2-I-GRAPH-05 |
| lifetime intervals, and the CPU front end's transient scratch placement from them | ZenithGraph, ZenithJobs | `Engine/ZenithGraph/Internal/Zenith_GraphLifetimes.cpp`; `Engine/ZenithJobs/Internal/FrameGraph/Zenith_FrameTransients.cpp` | Z2-I-GRAPH-06 |
| the `zenith2.graph-dump` JSON dump and the CPU `zenith2.frame-dump` | ZenithGraph, ZenithJobs | `Engine/ZenithGraph/Internal/Zenith_GraphJson.cpp`; `Engine/ZenithJobs/Internal/FrameGraph/Zenith_FrameDump.cpp` | Z2-I-GRAPH-07 |
| the frame-graph builder, its translation into the core, schedule emission, the schedule cache | ZenithJobs (the GRAPH part) | `Engine/ZenithJobs/Zenith_FrameGraph.h`, `Zenith_FrameSchedule.h`, `Internal/FrameGraph/**` | Z2-I-GRAPH-08 |
| the frame executor: dependency counters, sub-job joins at node level, continuations at node level, affinity, cancellation of an instance | ZenithJobs (the GRAPH part) | `Engine/ZenithJobs/Zenith_FrameExecutor.h`, `Internal/FrameGraph/Zenith_FrameExecutor*.cpp` | Z2-I-GRAPH-09, Z2-I-GRAPH-10, Z2-I-GRAPH-11 |
| render-extraction pipelining: the three subgraphs (TICK, EPILOGUE, RENDER), the iteration barrier, the frame timeline | ZenithJobs (the GRAPH part) | `Engine/ZenithJobs/Internal/FrameGraph/Zenith_FrameExecutorIteration.cpp` | Z2-I-GRAPH-12 |
| deterministic ordering: deterministic compile, priority independence, deterministic invocation IDs, `Zenith_FixedOrderReduce` | ZenithGraph, ZenithJobs | `Engine/ZenithJobs/Zenith_FixedOrderReduce.h` and the sources above | Z2-I-GRAPH-13 |
| the graph conformance suite shared with Flux, its core and CPU builders, and the specification of the Flux builder | test targets | `Engine/ZenithGraph/Tests/Conformance/**`, `Engine/ZenithJobs/Tests/GraphConformance/**`, `Engine/ZenithFlux/Tests/GraphConformance/**` | Z2-I-GRAPH-14 |

### What this area does not own

| Capability | Owner | Governing decision | Where it is specified |
|---|---|---|---|
| the work-stealing job system, worker threads, job queues, `ParallelFor`, `Continue`, join counters, background jobs, the floating-point environment | JOBS | Z2-D-006 | `Zenith2/Docs/Architecture/CPUFrameGraph.md` → "Job system"; `Zenith2/Docs/Architecture/CPUFrameGraph.md` → "Dynamic sub-jobs and continuations"; `Zenith2/Docs/Design/JOBS.md` |
| the simulation clock (fixed ticks, interpolation, pause, time scale, bounded catch-up) | JOBS (Z2-I-JOBS-10 to Z2-I-JOBS-14) | Z2-D-006 | `Zenith2/Docs/Architecture/CPUFrameGraph.md` → "Execution" |
| the twelve phases, per-field authority, arbitration rules and their schedule-compile checks | GAME (contract), ECS (checks) | Z2-D-080 | `Zenith2/Docs/Architecture/FramePhases.md` → "Per-field authority" |
| translating systems, phases, sync points, arbitration and extractors into frame-graph declarations (`Zenith_WorldSimulation::EmitInto`) and the node trampoline that skips a disabled system | ECS | Z2-D-080 | `Zenith2/Docs/Architecture/ECSModel.md` → "Worlds and backends" |
| the script nodes (`Zenith_ScriptWorld::RegisterScriptNodes`) | SCRIPT | Z2-D-018 | `Zenith2/Docs/Scripting.md` → "Behaviours and chunk systems" |
| the frame loop, the recompile triggers, engine stage nodes (`Platform.Pump`, `Render.Seal`) | MOD | Z2-D-081 | `Zenith2/Docs/Architecture/CompositionRoot.md` → "The frame loop" |
| `Flux_RenderWorld`, its two-slot ring, render build, record, submit | RPROD | Z2-D-050 | `Zenith2/Docs/Rendering.md` → "Purpose and scope" |
| the render graph's use of the core (passes, barriers, placement, queues) | RHI | Z2-D-055 | `Zenith2/Docs/Rendering/RenderGraph.md` → "Graph compile" |
| the on-screen profiler, the offline graph viewer, dump readers | DEVT | Z2-D-005 | `Zenith2/Docs/Budgets.md` → "Per-system rows" |
| fibers | excluded | Z2-D-006 | `Zenith2/Docs/Architecture/CPUFrameGraph.md` → "Settled rules and ownership" |

Fibers are excluded by Z2-D-006: no GRAPH code creates, switches or waits on a fiber or a user-mode context, and no GRAPH code path blocks a job on another job (Z2-R-GRAPH-087).

### Inventory items covered

| Item | Text | Specified in |
|---|---|---|
| Z2-I-GRAPH-01 | `ZenithGraph`: access declarations | § "Zenith_Graph.h"; `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Declaration algorithms" |
| Z2-I-GRAPH-02 | `ZenithGraph`: hazard edges | `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Compile pipeline" |
| Z2-I-GRAPH-03 | `ZenithGraph`: versioned handles | § "Zenith_Graph.h"; `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Declaration algorithms" |
| Z2-I-GRAPH-04 | `ZenithGraph`: topological sort with critical-path priority | `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Compile pipeline" |
| Z2-I-GRAPH-05 | `ZenithGraph`: cycle diagnostics | `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Diagnostic text formats" |
| Z2-I-GRAPH-06 | `ZenithGraph`: lifetime intervals | `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Compile pipeline"; `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Schedule emission" |
| Z2-I-GRAPH-07 | `ZenithGraph`: JSON dump | § "Graph dump schema zenith2.graph-dump version 1"; § "Frame dump schema zenith2.frame-dump version 1" |
| Z2-I-GRAPH-08 | CPU frame graph: compiled schedule | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Translation pipeline", "Schedule emission", "Schedule cache" |
| Z2-I-GRAPH-09 | CPU frame graph: dynamic sub-jobs | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Frame executor" |
| Z2-I-GRAPH-10 | CPU frame graph: continuations with no fibers | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Frame executor" |
| Z2-I-GRAPH-11 | CPU frame graph: affinity | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Frame executor" |
| Z2-I-GRAPH-12 | CPU frame graph: render-extraction pipelining | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Render-extraction pipelining" |
| Z2-I-GRAPH-13 | CPU frame graph: deterministic ordering | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Determinism proof obligations" |
| Z2-I-GRAPH-14 | a graph conformance suite shared with Flux | `Zenith2/Docs/Design/GRAPH/Tests.md` → "Graph conformance suite" |

### Relationship to the specification

The specification of record is `Zenith2/Docs/Architecture/ZenithGraph.md` (every `##` of it) and `Zenith2/Docs/Architecture/CPUFrameGraph.md` → "Frame graph builder", "Schedule compilation and caching", "Execution", "Render extraction and pipelining", "Worlds in the graph", "Cancellation and shutdown", "Profiling" and "Required tests". This document restates no normative bullet of those files; it cites them and adds what a worker lacks. When this document and the specification disagree, the specification wins and the disagreement is a contradiction to report (`Zenith2/Docs/Contracts.md` → "Purpose and scope"). The phase structure the CPU frame graph schedules is `Zenith2/Docs/Architecture/FramePhases.md` → "The twelve phases"; this area schedules phases as ordered stages and never interprets a phase number.

## Dependencies

### Library dependencies

| Library | Declared direct dependencies (`Zenith2/Docs/Architecture.md` → "Library dependency graph") | Layer |
|---|---|---|
| ZenithGraph | ZenithCore | 1 |
| ZenithJobs (the GRAPH part lives here) | ZenithCore, ZenithGraph, ZenithProfiling | 2 |

ZenithGraph uses no third-party dependency, and neither does the GRAPH part of ZenithJobs. Neither library declares an edge beyond the table Architecture fixes (Z2-R-GRAPH-004, Z2-R-GRAPH-061).

### Headings this design relies on

| Area | Heading | What GRAPH uses |
|---|---|---|
| CORE (Contracts) | `Zenith2/Docs/Contracts/Memory.md` → "Zenith_Arena" | `Zenith_Arena::TryAllocate`, `GetMarker`, `ResetToMarker`, `Reset`, `Zenith_TryConstruct`; the graph and the schedule allocate only through `TryAllocate` |
| CORE (Contracts) | `Zenith2/Docs/Contracts/Memory.md` → "Zenith_Heap" | `Zenith_Heap::TryAllocate`, `m_ulFailAfterAllocations` for failure injection |
| CORE (Contracts) | `Zenith2/Docs/Contracts/Memory.md` → "Zenith_Owned" | `Zenith_Owned<Zenith_FrameSchedule>` and the other owned objects, `Zenith_TryNew` |
| CORE (Contracts) | `Zenith2/Docs/Contracts.md` → "Errors and results" | `Zenith_Result`, `Zenith_Err`, `ZENITH_TRY`, the assertion macros, `Zenith_FatalCode` |
| CORE (Contracts) | `Zenith2/Docs/Contracts.md` → "Threading and concurrency contract" | `Zenith_Mutex` at `uLOCK_LEVEL_JOBS_SCHEDULE`, `Zenith_QueueFullPolicy`, the shutdown ownership order, the reentrancy notation |
| CORE (Contracts) | `Zenith2/Docs/Contracts.md` → "No mutable global state" | `Zenith_ZoneSite`, `Zenith_GetProfilerHooks`, the counter hook |
| ARCH | `Zenith2/Docs/Architecture.md` → "Purpose and scope" (table "Assumed types") | `Zenith_Span`, `Zenith_StringView`, `Zenith_JsonWriter`, `Zenith_Hash64` |
| JOBS | `Zenith2/Docs/Architecture/CPUFrameGraph.md` → "Job system"; `Zenith2/Docs/Design/JOBS/PublicAPI.md` → "Zenith_JobTypes header" | `Zenith_JobSystem`, `Zenith_JobContext`, `Zenith_JobAffinity` and `Zenith_PfnJob` (from `Zenith_JobTypes.h`), the queues and their full policies |
| JOBS | `Zenith2/Docs/Architecture/CPUFrameGraph.md` → "Dynamic sub-jobs and continuations" | `ParallelFor` and `Continue` inside nodes, and the join rule [Z2-N-ARCH-395] |
| ECS | `Zenith2/Docs/Architecture/ECSModel.md` → "Worlds and backends" | `Zenith_WorldSimulation::EmitInto(Zenith_FrameGraphBuilder&)`, the main producer of declarations |
| ECS | `Zenith2/Docs/Architecture/ECSModel.md` → "Deterministic reductions" | the tree shape `Zenith_FixedOrderReduce` implements [Z2-N-ARCH-704] |
| GAME | `Zenith2/Docs/Architecture/FramePhases.md` → "Sync points and the tick boundary" | what a stage-barrier body does (this area only runs it) |
| MOD | `Zenith2/Docs/Architecture/CompositionRoot.md` → "The frame loop" | the caller of `Zenith_FrameExecutor::RunFrame` and of the schedule cache |
| RPROD | `Zenith2/Docs/Rendering.md` → "Purpose and scope" | the `Flux_RenderWorld` hand-off and its ring of `uRENDER_WORLD_RING_SIZE` slots |
| RHI | `Zenith2/Docs/Rendering/RenderGraph.md` → "Graph compile" | how Flux drives the core, which the Flux conformance builder follows |

### The JOBS boundary

GRAPH and JOBS both put code into ZenithJobs. The boundary, per `Zenith2/Docs/Architecture/CPUFrameGraph.md` → "Settled rules and ownership" and the inventory rows:

| Piece | Owner | Files |
|---|---|---|
| `Zenith_JobSystem`, `Zenith_JobContext`, `Zenith_JobSystemDesc`, `Zenith_JobAffinity`, `Zenith_JobPriority`, `Zenith_PfnJob`, `Zenith_PfnParallelForBody`, threads, deques, injection, affinity and background queues, stealing, parking, `ParallelFor`, `Continue`, per-node join counters, continuation lists, background jobs and their cancellation, `Zenith_FloatEnvironment.h` | JOBS | `Engine/ZenithJobs/Zenith_JobSystem.h`, `Zenith_JobContext.h`, `Zenith_FloatEnvironment.h`, `Internal/Jobs/**`, `Internal/Windows/**`, `Internal/Android/**`, `Internal/Linux/**` |
| `Zenith_SimulationClock` | JOBS | `Engine/ZenithJobs/Zenith_SimulationClock.h`, `Internal/Clock/**` |
| `Zenith_JobsError` (the JOBS error enumeration, as `Zenith2/Docs/Design/JOBS/PublicAPI.md` → "Zenith_JobsErrors header" declares it) | JOBS | `Engine/ZenithJobs/Zenith_JobsErrors.h` (the JOBS-owned declarations) |
| `Zenith_FrameGraphError`, `Zenith_FrameGraphErrorCode`, `Zenith_FrameGraphDiagnostic` | GRAPH | `Engine/ZenithJobs/Zenith_JobsErrors.h` (the GRAPH-owned declarations, § "Frame-graph errors in Zenith_JobsErrors.h") |
| the frame-graph builder and its translation into ZenithGraph; schedule emission (run tables, dependency counters, affinity counts, transient placement); the schedule cache; the frame executor (subgraph instances, node dependency counters, the node trampoline, node completion, instance sequencing, the iteration barrier, cancellation of an instance, timings); the frame dump; `Zenith_FixedOrderReduce` | GRAPH | `Engine/ZenithJobs/Zenith_FrameGraph.h`, `Zenith_FrameSchedule.h`, `Zenith_FrameExecutor.h`, `Zenith_FixedOrderReduce.h`, `Internal/FrameGraph/**`, `Tests/Zenith_Frame*_Tests.cpp`, `Tests/Zenith_FixedOrderReduce_Tests.cpp`, `Tests/GraphConformance/**` |

The rule on the seam: JOBS decides how a job reaches a thread; GRAPH decides when a node is ready and what happens when it completes. A GRAPH ticket edits no file of the JOBS rows and a JOBS ticket edits no file of the GRAPH row; the one shared public header, `Zenith_JobsErrors.h`, is edited declaration-wise by its two owners.

### The executor to job-system seam

The frame executor launches nodes and learns of their completion through an internal interface that JOBS owns and specifies: `Zenith2/Docs/Design/JOBS/Algorithms.md` → "Node launch seam" (header `Zenith2/Engine/ZenithJobs/Internal/Jobs/Zenith_JobNodeLaunch.h`). The executor is ZenithJobs code, so it includes it legally ([Z2-N-CONTRACT-024]). What GRAPH uses from it:

| Seam member | GRAPH's use |
|---|---|
| `Zenith_JobNodeLaunch` (`m_pfnBody`, `m_pBodyData`, `m_pfnOnComplete`, `m_pCompleteData`, `m_pxZone`, `m_puCancel`, `m_ulInvocationID`, `m_eAffinity`) | one launch per ready node: the GRAPH trampoline, the node instance, the completion callback, the node's zone site, the executor's cancel flag, the deterministic invocation ID, the node's affinity |
| `Zenith_JobsPushNodes(jobs, pusher, span)` | pushes a batch sorted by descending priority; JOBS guarantees each destination's next take is the batch's earliest element for it |
| `Zenith_JobsRunMainUntil(jobs, uDone)`, `Zenith_JobsWakeMain(jobs)` | the main thread helps until the iteration-done flag is set; the thread completing the iteration wakes it |
| `Zenith_JobsBeginIteration(jobs, k)`, `Zenith_JobsEndIteration(jobs)` | bracket every `RunFrame` iteration on the main thread |
| `Zenith_JobsReserveAffinityCapacity(jobs, uMain, uRenderSubmit)` | called by `SetSchedule` with the new schedule's `GetAffinityNodeCount` values |
| `Zenith_JobsReserveInjectionCapacity(jobs, uInjectedNodes)` | called by `SetSchedule`, beside the affinity call, with the new schedule's `GetInjectedNodeCount()` ([Z2-N-ARCH-392]) |
| `Zenith_JobsAssertFloatEnvironment()` | the trampoline's per-node float-environment check |

Pushers: `RunFrame` launches its first instances from the main thread with `pxPusher = nullptr` (legal between `Zenith_JobsBeginIteration` and `Zenith_JobsEndIteration`); every other push, including the launch of a next tick instance, the epilogue or a batch of ready successors, is made from a completion path and passes that path's `&xContext`.

The contract GRAPH relies on for `m_pfnOnComplete` is the seam's "Completion" rule: JOBS calls it exactly once per launch, after the body has returned, after every sub-job the body spawned (transitively) has finished and after every continuation it registered has finished, on the thread that finished the last of them, with every write of the body, its sub-jobs and its continuations visible to that thread; a launch whose cancel flag was set before it started runs no body and completes at once. This is [Z2-N-ARCH-395] seen from the executor; Z2-R-GRAPH-084 and Z2-R-GRAPH-086 test it. Invocation IDs of frame nodes keep bits 62 and 63 clear, which the seam reserves for `RunRoot` and background jobs.

### Consumers and the interfaces they call

| Consumer | Calls | From |
|---|---|---|
| ECS (`Zenith_WorldSimulation::EmitInto`) | `Zenith_FrameGraphBuilder::AddNode`, `AddResource`, `Read`, `Write`, `Before`, `After`, `SetStageBarrier`, `SetTransientSize` | the main thread, during a (re)compile |
| SCRIPT (`Zenith_ScriptWorld::RegisterScriptNodes`) | the same builder calls | the main thread, during a (re)compile |
| MOD (`ZenithRuntime`) | engine stage nodes through the builder; `Zenith_FrameScheduleCache::Acquire`, `Reprioritise`, `ReleaseUnused`; `Zenith_FrameExecutor::Create`, `SetSchedule`, `RunFrame`, `RequestCancel` | the main thread |
| RHI and RPROD (`ZenithFlux`) | `Zenith_Graph` directly, one per frame, with `m_bCullUnusedNodes = true` | the render-graph build node |
| DEVT (on-screen profiler, graph viewer) | `Zenith_FrameExecutor::GetLastTimeline`, `GetLastTimings`, `WriteFrameDump`; reads `zenith2.graph-dump` and `zenith2.frame-dump` files | the main thread (profiler), offline (viewer) |

### Upward-callback interfaces

GRAPH declares no upward notification interface. Its only calls into higher libraries are the node bodies registered through `Zenith_FrameNodeDesc::m_pfnRun` and `Zenith_StageBarrierDesc::m_pfnRun`: plain `Zenith_PfnJob` function pointers with user data, which the higher library passes down when it declares its nodes. The callee type is declared by ZenithJobs, the lower library, so the registration points downward ([Z2-N-CONTRACT-175]). The pointers live only in a schedule binding's run table and are dropped when the binding is released (Z2-R-GRAPH-076).

### CORE types used

Every CORE type GRAPH uses is declared by the CORE design; GRAPH cites it and assumes nothing:

| Type or function | Declared in | GRAPH's use |
|---|---|---|
| `Zenith_StringView(const char*, uint64)`, `GetData()`, `GetLength()`, byte-wise `operator<=>` | `Zenith2/Docs/Design/CORE/Text.md` → "Zenith_StringView" | names, reasons, texts (16 bytes, so the record sizes of `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Declaration tables" hold) |
| `Zenith_JsonWriter(Zenith_Array<char>&, Zenith_JsonWriteStyle)`, `BeginArray(Zenith_JsonArrayLayout)`, `GetStatus()`, `Finish()` | `Zenith2/Docs/Design/CORE/Text.md` → "JSON writer" | every dump; the byte form of each style is that heading's table |
| `Zenith_Hash64(Zenith_ByteSpan, uint64 ulSeed = 0)`, `Zenith_Hash64Stream` (`UpdateU32`, `UpdateU64`, `Update`, `Finish`) | `Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_Hash64" | the structure hash, streamed with no scratch |
| `Zenith_GetMonotonicTimeNs()` | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Clocks" | timings and the frame timeline (profiling only) |
| the error-record form of `Zenith_Result<T, E>` (`m_eCode` first) | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Results and the error taxonomy" | `Zenith_GraphCompileError`, `Zenith_FrameGraphError` |
| `Zenith_Array<T>(Zenith_Arena&)` (arena tier, `TryPushBack`, `TryReserve`, strong guarantee) | `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_Array" | every declaration table of `Zenith_Graph` and of the builder |
| `Zenith_TryStableSort`, `Zenith_LowerBound` | `Zenith2/Docs/Design/CORE/Containers.md` → "Sorting and searching" | every sort of the compile and the translation |

## Library and file layout

### ZenithGraph

`Zenith2/Engine/ZenithGraph/CMakeLists.txt` is `zenith_add_library(ZenithGraph DEPENDS ZenithCore)` with the default matrix (Z2-R-GRAPH-004).

| Path (repo-root relative) | Kind | Contents |
|---|---|---|
| `Zenith2/Engine/ZenithGraph/Zenith_Graph.h` | public header, the only one | every public declaration of § "Zenith_Graph.h" |
| `Zenith2/Engine/ZenithGraph/Internal/Zenith_GraphInternal.h` | private header | the record types and the `Zenith_Graph` private member layout (`Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Declaration tables") |
| `Zenith2/Engine/ZenithGraph/Internal/Zenith_GraphDeclare.cpp` | source | constructor, `AddNode`, `AddResource`, `DeclareRead`, `DeclareWrite`, `DeclareAccess`, `AddOrdering`, `MarkOutput`, getters, `SetFrontEndTag` |
| `Zenith2/Engine/ZenithGraph/Internal/Zenith_GraphCompile.cpp` | source | `Zenith_Graph::Compile`: the step driver of `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Compile step list" |
| `Zenith2/Engine/ZenithGraph/Internal/Zenith_GraphHazards.cpp` | source | steps 1 to 3 |
| `Zenith2/Engine/ZenithGraph/Internal/Zenith_GraphIntervalMap.h`, `Zenith_GraphIntervalMap.cpp` | private header and source | the per-resource subresource interval map |
| `Zenith2/Engine/ZenithGraph/Internal/Zenith_GraphCycles.cpp` | source | steps 4 and 5 |
| `Zenith2/Engine/ZenithGraph/Internal/Zenith_GraphCull.cpp` | source | step 6 |
| `Zenith2/Engine/ZenithGraph/Internal/Zenith_GraphOrder.cpp` | source | steps 7, 8 and 10, and `Zenith_GraphReprioritise` |
| `Zenith2/Engine/ZenithGraph/Internal/Zenith_GraphReduce.cpp` | source | step 9 |
| `Zenith2/Engine/ZenithGraph/Internal/Zenith_GraphLifetimes.cpp` | source | step 11 |
| `Zenith2/Engine/ZenithGraph/Internal/Zenith_GraphHash.cpp` | source | step 12 and `Zenith_GraphStructureHash` |
| `Zenith2/Engine/ZenithGraph/Internal/Zenith_GraphText.h`, `Zenith_GraphText.cpp` | private header and source | `Zenith_GraphTextWriter` and every diagnostic message format |
| `Zenith2/Engine/ZenithGraph/Internal/Zenith_GraphJson.cpp` | source | `Zenith_Graph::WriteJson`, `Zenith_GraphDumpVersionIsReadable`, `Zenith_GraphDumpVersionMessage` |
| `Zenith2/Engine/ZenithGraph/Internal/Zenith_GraphStrings.cpp` | source | the `Zenith_ToString` overloads |
| `Zenith2/Engine/ZenithGraph/Tests/` | tests | `Zenith2/Docs/Design/GRAPH/Tests.md` → "Test files" |

ZenithGraph has no `Internal/Windows|Android|Linux` files: it is platform-neutral code.

### The GRAPH part of ZenithJobs

`Zenith2/Engine/ZenithJobs/CMakeLists.txt` is `zenith_add_library(ZenithJobs DEPENDS ZenithCore ZenithGraph ZenithProfiling)` (JOBS owns the file; GRAPH adds no dependency).

| Path (repo-root relative) | Kind | Contents |
|---|---|---|
| `Zenith2/Engine/ZenithJobs/Zenith_FrameGraph.h` | public header | § "Zenith_FrameGraph.h" |
| `Zenith2/Engine/ZenithJobs/Zenith_FrameSchedule.h` | public header | § "Zenith_FrameSchedule.h" |
| `Zenith2/Engine/ZenithJobs/Zenith_FrameExecutor.h` | public header | § "Zenith_FrameExecutor.h" |
| `Zenith2/Engine/ZenithJobs/Zenith_FixedOrderReduce.h` | public header | § "Zenith_FixedOrderReduce.h" |
| `Zenith2/Engine/ZenithJobs/Zenith_JobsErrors.h` | public header shared with JOBS | the GRAPH declarations of § "Frame-graph errors in Zenith_JobsErrors.h" |
| `Zenith2/Engine/ZenithJobs/Internal/FrameGraph/Zenith_FrameGraphInternal.h` | private header | builder records, schedule structure and binding layouts, executor instance records |
| `Zenith2/Engine/ZenithJobs/Internal/FrameGraph/Zenith_FrameGraphBuilder.cpp` | source | the builder's declaration calls |
| `Zenith2/Engine/ZenithJobs/Internal/FrameGraph/Zenith_FrameTranslate.cpp` | source | translation steps 0 to 5 |
| `Zenith2/Engine/ZenithJobs/Internal/FrameGraph/Zenith_FrameLower.cpp` | source | translation steps 6 and 7 |
| `Zenith2/Engine/ZenithJobs/Internal/FrameGraph/Zenith_FrameAssemble.cpp` | source | `Zenith_AssembleFrameSchedule`, run tables, affinity counts, zone sites, the combined hash |
| `Zenith2/Engine/ZenithJobs/Internal/FrameGraph/Zenith_FrameTransients.cpp` | source | transient scratch placement |
| `Zenith2/Engine/ZenithJobs/Internal/FrameGraph/Zenith_FrameSchedule.cpp` | source | `Zenith_FrameSchedule` members |
| `Zenith2/Engine/ZenithJobs/Internal/FrameGraph/Zenith_FrameScheduleCache.cpp` | source | `Zenith_FrameScheduleCache` |
| `Zenith2/Engine/ZenithJobs/Internal/FrameGraph/Zenith_FrameExecutor.cpp` | source | `Zenith_FrameExecutor::Create`, destructor, `SetSchedule`, `RequestCancel`, accessors |
| `Zenith2/Engine/ZenithJobs/Internal/FrameGraph/Zenith_FrameExecutorIteration.cpp` | source | `RunFrame`, instance launch, the trampoline, node and instance completion, the iteration barrier |
| `Zenith2/Engine/ZenithJobs/Internal/FrameGraph/Zenith_FrameExecutorTimings.cpp` | source | timing capture, the cost EWMA, the timeline, the counters |
| `Zenith2/Engine/ZenithJobs/Internal/FrameGraph/Zenith_FrameDump.cpp` | source | `Zenith_FrameSchedule::WriteJson`, `WriteSubgraphJson`, `Zenith_FrameExecutor::WriteFrameDump` |
| `Zenith2/Engine/ZenithJobs/Internal/FrameGraph/Zenith_FrameGraphText.cpp` | source | the frame-graph diagnostic message formats, `Zenith_ToString(Zenith_FrameGraphErrorCode)` |
| `Zenith2/Engine/ZenithJobs/Internal/FrameGraph/Zenith_FixedOrderReduce.cpp` | source | `Zenith_FixedOrderReduceSerial`, `Zenith_FixedOrderReduceParallel` |

The GRAPH part has no platform files: every platform difference sits below it, in JOBS' threads and in ZenithCore's `Zenith_Multithreading`.

### Test targets and test files

| Target | Built from | Links | Runs in |
|---|---|---|---|
| `ZenithGraph_Tests` | `Zenith2/Engine/ZenithGraph/Tests/**` except `Conformance/` | ZenithGraph, ZenithTestSupport, doctest | `Test` step; `Sanitizers` lines of `Zenith2Memory` |
| `ZenithGraph_Conformance` | `Zenith2/Engine/ZenithGraph/Tests/Conformance/**`, `Zenith2/Engine/ZenithJobs/Tests/GraphConformance/**`, `Zenith2/Engine/ZenithFlux/Tests/GraphConformance/**` (Flux sources only in `ZENITH_GPU` presets) | ZenithGraph, ZenithJobs, ZenithFlux where supported, ZenithTestSupport, doctest | as above; the Flux builder is absent from `linux-tsan` |
| `ZenithJobs_Tests` | `Zenith2/Engine/ZenithJobs/Tests/**` except `GraphConformance/` (GRAPH's files and JOBS' files together) | ZenithJobs and its dependencies, ZenithTestSupport, doctest | as above |
| `Zenith_Bench_GraphCompile`, `Zenith_Bench_ScheduleRecompile` | `Zenith2/Benchmarks/Graph/` and `Zenith2/Benchmarks/Budgets/GRAPH/` | ZenithGraph, ZenithJobs, nanobench | reported, never gated; CTest smoke in the `Test` step |

The target shapes are those of `Zenith2/Docs/Architecture/BuildSystem.md` → "Test, conformance and fuzz targets". Every test file and test is listed in `Zenith2/Docs/Design/GRAPH/Tests.md` → "Test files".

### Supported matrix

| Target | `SUPPORTED` | Reason |
|---|---|---|
| ZenithGraph | `ZENITH_ALL` (default) | CPU-only, platform-neutral |
| ZenithJobs | `ZENITH_ALL` | CPU-only; its tests run under `linux-tsan` |
| ZenithGraph_Conformance | the `ZENITH_HOST` presets | a test target; the Flux builder's sources join it only where ZenithFlux is supported (`win`, `win-asan`) |

## Public API

The headers below are complete: every declaration, every contract block, in file order. Indentation in the headers uses tabs. A declaration the specification already gives keeps the specification's name and signature; § "Declarations added to the specification" lists every declaration this design adds (Z2-R-GRAPH-004, Z2-R-GRAPH-061).

### Zenith_Graph.h

```cpp
// Zenith2/Engine/ZenithGraph/Zenith_Graph.h
#pragma once

#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithCore/Zenith_JsonWriter.h"

class Zenith_Arena;

/// @contract functions Zenith_GraphConstants
/// Ownership: Compile-time constants; nothing is owned.
/// Thread affinity: IMMUTABLE. Usable from any thread.
/// Invalidation: None.
/// Units: Counts of nodes, resources, accesses, orderings, subresources and bytes.
/// Errors: Exceeding a limit records GRAPH_ERROR_CAPACITY at the declaration that exceeds it; nothing asserts.
/// Version: since Z2 Stage 2. A limit may rise in a subsequent fix version; it never falls within a release line.
/// @endcontract
inline constexpr uint32 uGRAPH_INVALID_INDEX = 0xFFFFFFFFu;
inline constexpr uint32 uGRAPH_POISON_VERSION = 0xFFFFFFFFu;
inline constexpr uint32 uGRAPH_MAX_NODES = 1u << 20;
inline constexpr uint32 uGRAPH_MAX_RESOURCES = 1u << 22;
inline constexpr uint32 uGRAPH_MAX_ACCESSES = 1u << 24;
inline constexpr uint32 uGRAPH_MAX_ORDERINGS = 1u << 22;
inline constexpr uint32 uGRAPH_MAX_SUBRESOURCES = 1u << 24;
inline constexpr uint32 uGRAPH_MAX_VERSIONS = 1u << 24;
inline constexpr uint32 uGRAPH_MAX_NAME_BYTES = 255;
inline constexpr uint32 uGRAPH_DUMP_SCHEMA_VERSION = 1;

/// @contract value Zenith_GraphNodeHandle
/// Ownership: Plain value naming one node of one Zenith_Graph; the graph owns the node.
/// Thread affinity: IMMUTABLE.
///   Members non-const: a handle is copy-assigned and stored in containers and in other types' members.
/// Invalidation: Invalid once the graph's arena is reset or the graph is destroyed; frame-scoped
///   (Contracts.md, "Frame-scoped graph handles"). Never valid in another graph (serial compare).
/// Units: None.
/// Errors: A handle of another graph, or an index at or above the node count, is a programmer error (ZENITH_ASSERT)
///   and records GRAPH_ERROR_INVALID_HANDLE in every build. An invalid handle carrying this graph's serial is poisoned
///   (see Zenith_Graph) and is ignored silently.
/// Version: since Z2 Stage 2. Never serialized.
/// @endcontract
struct Zenith_GraphNodeHandle
{
	uint32 m_uIndex = uGRAPH_INVALID_INDEX;
	uint32 m_uGraphSerial = 0;

	constexpr bool IsValid() const { return m_uIndex != uGRAPH_INVALID_INDEX; }
};

/// @contract value Zenith_GraphResourceHandle
/// Ownership: Plain value naming one version of one resource of one Zenith_Graph; the graph owns the resource.
/// Thread affinity: IMMUTABLE.
///   Members non-const: a handle is copy-assigned and stored in containers and in other types' members.
/// Invalidation: As Zenith_GraphNodeHandle. A handle whose version is not the latest stays valid for reads
///   (reads of an older version are legal) and is a version fork for writes.
/// Units: m_uVersion counts the writes declared on the resource before this version; version 0 is the state
///   before any write of this graph.
/// Errors: As Zenith_GraphNodeHandle. m_uVersion == uGRAPH_POISON_VERSION marks a poisoned handle.
/// Version: since Z2 Stage 2. Never serialized.
/// @endcontract
struct Zenith_GraphResourceHandle
{
	uint32 m_uIndex = uGRAPH_INVALID_INDEX;
	uint32 m_uVersion = 0;
	uint32 m_uGraphSerial = 0;

	constexpr bool IsValid() const { return m_uIndex != uGRAPH_INVALID_INDEX; }
};

/// @contract enum Zenith_GraphAccessMode
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: A value outside the enumerators is a programmer error (ZENITH_ASSERT) and records GRAPH_ERROR_INVALID_HANDLE.
/// Version: since Z2 Stage 2. The numeric values are part of the structure hash and the dump; they never change.
/// @endcontract
enum Zenith_GraphAccessMode : uint8
{
	GRAPH_ACCESS_READ = 0,
	GRAPH_ACCESS_WRITE = 1,
	GRAPH_ACCESS_READ_WRITE = 2,
	GRAPH_ACCESS_WRITE_DISCARD = 3,
};

/// @contract value Zenith_GraphSubrange
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: Compile writes the schedule's subrange table element by element.
/// Invalidation: None.
/// Units: Indices in the resource's flat subresource index space; m_uCount subresources from m_uFirst.
/// Errors: m_uCount == 0 or m_uFirst + m_uCount > the subresource count records GRAPH_ERROR_SUBRANGE_OUT_OF_BOUNDS.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_GraphSubrange
{
	uint32 m_uFirst = 0;
	uint32 m_uCount = 0;
};

/// @contract enum Zenith_GraphResourceFlags
/// Ownership: Plain bit flags.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: Unknown bits are a programmer error (ZENITH_ASSERT) and are cleared.
/// Version: since Z2 Stage 2. Bit positions are part of the structure hash and the dump; they never change.
/// @endcontract
enum Zenith_GraphResourceFlags : uint32
{
	GRAPH_RESOURCE_FLAG_NONE = 0,
	GRAPH_RESOURCE_FLAG_IMPORTED = 1u << 0,
	GRAPH_RESOURCE_FLAG_OUTPUT = 1u << 1,
	GRAPH_RESOURCE_FLAG_REQUIRE_DEFINED_READS = 1u << 2,
	GRAPH_RESOURCE_FLAG_FIRST_WRITE_DISCARD = 1u << 3,                 // the first write of a non-imported resource is WRITE_DISCARD over every subresource
	GRAPH_RESOURCE_FLAG_FIRST_WRITE_DISCARD_PER_SUBRESOURCE = 1u << 4,  // the first write of each subresource interval is WRITE_DISCARD over that interval
};

/// @contract value Zenith_GraphResourceDesc
/// Ownership: Plain value; m_strName is copied into the graph arena by AddResource, so the caller's bytes may die
///   after the call.
/// Thread affinity: IMMUTABLE. Members non-const: callers default-construct it and set the fields they need.
/// Invalidation: None.
/// Units: m_uSubresourceCount in subresources, at least 1, at most uGRAPH_MAX_SUBRESOURCES.
/// Errors: A zero subresource count or a name longer than uGRAPH_MAX_NAME_BYTES records GRAPH_ERROR_SUBRANGE_OUT_OF_BOUNDS
///   respectively GRAPH_ERROR_CAPACITY at AddResource.
/// Version: since Z2 Stage 2. m_ulBackendPayload is opaque to ZenithGraph and excluded from the structure hash.
/// @endcontract
struct Zenith_GraphResourceDesc
{
	Zenith_StringView m_strName;
	uint32 m_uSubresourceCount = 1;
	uint32 m_uFlags = GRAPH_RESOURCE_FLAG_NONE;
	uint64 m_ulBackendPayload = 0;
};

/// @contract enum Zenith_GraphNodeFlags
/// Ownership: Plain bit flags.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: Unknown bits are a programmer error (ZENITH_ASSERT) and are cleared.
/// Version: since Z2 Stage 2.
/// @endcontract
enum Zenith_GraphNodeFlags : uint32
{
	GRAPH_NODE_FLAG_NONE = 0,
	GRAPH_NODE_FLAG_SIDE_EFFECT = 1u << 0,
};

/// @contract value Zenith_GraphNodeDesc
/// Ownership: Plain value; m_strName is copied into the graph arena by AddNode.
/// Thread affinity: IMMUTABLE. Members non-const: callers default-construct it and set the fields they need.
/// Invalidation: None.
/// Units: m_uCost is a relative cost in the front end's unit (CPU: microseconds; GPU: the static cost hint of
///   Rendering/RenderGraph.md); values below 1 are clamped to 1.
/// Errors: A name longer than uGRAPH_MAX_NAME_BYTES records GRAPH_ERROR_CAPACITY.
/// Version: since Z2 Stage 2. m_uLane and m_ulBackendPayload are opaque; m_uLane is part of the structure hash,
///   m_uCost and m_ulBackendPayload are not.
/// @endcontract
struct Zenith_GraphNodeDesc
{
	Zenith_StringView m_strName;
	uint32 m_uLane = 0;
	uint32 m_uCost = 1;
	uint32 m_uFlags = GRAPH_NODE_FLAG_NONE;
	uint64 m_ulBackendPayload = 0;
};

/// @contract value Zenith_GraphAccessDesc
/// Ownership: Plain value; m_axSubranges is copied into the graph arena by DeclareAccess.
/// Thread affinity: IMMUTABLE. Members non-const: callers default-construct it and set the fields they need.
/// Invalidation: None.
/// Units: As Zenith_GraphSubrange. An empty m_axSubranges means every subresource.
/// Errors: As DeclareRead and DeclareWrite.
/// Version: since Z2 Stage 2. m_uBackendUsage is opaque and part of the structure hash.
/// @endcontract
struct Zenith_GraphAccessDesc
{
	Zenith_GraphResourceHandle m_xResource;
	Zenith_GraphAccessMode m_eMode = GRAPH_ACCESS_READ;
	Zenith_Span<const Zenith_GraphSubrange> m_axSubranges;
	uint32 m_uBackendUsage = 0;
};

/// @contract value Zenith_GraphCompileOptions
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: callers default-construct it and set the fields they need.
/// Invalidation: None.
/// Units: m_uTransitiveReductionNodeLimit in non-culled nodes.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_GraphCompileOptions
{
	bool m_bCullUnusedNodes = false;
	bool m_bTransitiveReduction = true;
	uint32 m_uTransitiveReductionNodeLimit = 4096;
};

/// @contract enum Zenith_GraphEdgeReason
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2. The numeric order is the deduplication precedence (RAW first); it never changes.
/// @endcontract
enum Zenith_GraphEdgeReason : uint8
{
	GRAPH_EDGE_RAW = 0,
	GRAPH_EDGE_WAR = 1,
	GRAPH_EDGE_WAW = 2,
	GRAPH_EDGE_ORDERING = 3,
};

/// @contract value Zenith_GraphEdge
/// Ownership: Plain value stored in a schedule arena.
/// Thread affinity: IMMUTABLE. Members non-const: Compile writes and sorts the edge table in place.
/// Invalidation: With the schedule arena.
/// Units: Node and resource declaration indices; m_uVersion is the version whose hazard created the edge.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_GraphEdge
{
	uint32 m_uFrom = uGRAPH_INVALID_INDEX;
	uint32 m_uTo = uGRAPH_INVALID_INDEX;
	Zenith_GraphEdgeReason m_eReason = GRAPH_EDGE_ORDERING;
	uint32 m_uResource = uGRAPH_INVALID_INDEX;
	uint32 m_uVersion = 0;
	uint32 m_uOrderingIndex = uGRAPH_INVALID_INDEX;
};

/// @contract value Zenith_GraphLifetime
/// Ownership: Plain value stored in a schedule arena.
/// Thread affinity: IMMUTABLE. Members non-const: Compile writes the lifetime table in place.
/// Invalidation: With the schedule arena.
/// Units: Positions in Zenith_GraphSchedule::m_auOrder; both uGRAPH_INVALID_INDEX when no non-culled node accesses
///   the resource.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_GraphLifetime
{
	uint32 m_uFirstPosition = uGRAPH_INVALID_INDEX;
	uint32 m_uLastPosition = uGRAPH_INVALID_INDEX;
};

/// @contract value Zenith_GraphAccessRecord
/// Ownership: Plain value stored in a schedule arena.
/// Thread affinity: IMMUTABLE. Members non-const: Compile writes the access table in place.
/// Invalidation: With the schedule arena.
/// Units: Declaration indices and versions; m_uFirstSubrange indexes Zenith_GraphSchedule::m_axSubranges.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_GraphAccessRecord
{
	uint32 m_uNode;
	uint32 m_uResource;
	uint32 m_uVersionRead;
	uint32 m_uVersionWritten;
	Zenith_GraphAccessMode m_eMode;
	uint32 m_uFirstSubrange;
	uint32 m_uSubrangeCount;
	uint32 m_uBackendUsage;
};

/// @contract value Zenith_GraphSchedule
/// Ownership: A view over storage in the arena passed to Compile or Zenith_GraphReprioritise; that arena owns every span.
/// Thread affinity: IMMUTABLE. Any number of threads may read it once Compile has returned.
///   Members non-const: Compile fills it field by field.
/// Invalidation: Every span dies when the schedule arena is reset or destroyed; the graph's arena and the graph's
///   lifetime do not matter.
/// Units: Node and resource declaration indices; positions in m_auOrder; priorities in the cost unit.
/// Errors: None.
/// Version: since Z2 Stage 2. Byte-identical for identical declarations, options and costs.
/// @endcontract
struct Zenith_GraphSchedule
{
	Zenith_Span<const uint32> m_auOrder;
	Zenith_Span<const uint32> m_auPosition;
	Zenith_Span<const uint32> m_auDependencyCount;
	Zenith_Span<const Zenith_GraphEdge> m_axEdges;
	Zenith_Span<const uint32> m_auSuccessorOffset;
	Zenith_Span<const uint64> m_aulPriority;
	Zenith_Span<const Zenith_GraphLifetime> m_axLifetimes;
	Zenith_Span<const Zenith_GraphAccessRecord> m_axAccesses;
	Zenith_Span<const uint32> m_auAccessOffset;
	Zenith_Span<const Zenith_GraphSubrange> m_axSubranges;
	uint64 m_ulStructureHash = 0;
	uint32 m_uCulledNodeCount = 0;
	uint32 m_uReducedEdgeCount = 0;
	bool m_bReductionSkipped = false;
};

/// @contract enum Zenith_GraphErrorCode
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: GRAPH_ERROR_CYCLE: a strongly connected component of more than one node. GRAPH_ERROR_VERSION_FORK: two writes
///   from one version. GRAPH_ERROR_UNDEFINED_READ: a read of a never-written part of a defined-read resource.
///   GRAPH_ERROR_FIRST_WRITE_NOT_DISCARD: a partial first write of a first-write-discard resource.
///   GRAPH_ERROR_INVALID_HANDLE: a foreign or out-of-range handle, or an invalid enumerator. GRAPH_ERROR_SUBRANGE_OUT_OF_BOUNDS:
///   a subrange outside the resource or a zero subresource count. GRAPH_ERROR_SELF_ORDERING: an ordering of a node
///   before itself. GRAPH_ERROR_CAPACITY: an arena exhausted or a uGRAPH_MAX_* limit exceeded.
/// Version: since Z2 Stage 2. Enumerators keep their values and meanings; a new failure gets a new enumerator.
/// @endcontract
enum Zenith_GraphErrorCode : uint8
{
	GRAPH_ERROR_CYCLE = 1,
	GRAPH_ERROR_VERSION_FORK = 2,
	GRAPH_ERROR_UNDEFINED_READ = 3,
	GRAPH_ERROR_FIRST_WRITE_NOT_DISCARD = 4,
	GRAPH_ERROR_INVALID_HANDLE = 5,
	GRAPH_ERROR_SUBRANGE_OUT_OF_BOUNDS = 6,
	GRAPH_ERROR_SELF_ORDERING = 7,
	GRAPH_ERROR_CAPACITY = 8,
};

/// @contract value Zenith_GraphDiagnostic
/// Ownership: Plain value held by a Zenith_GraphDiagnosticReport; m_strText points into the schedule arena passed to
///   Compile (or at static text for the arena-exhausted case).
/// Thread affinity: IMMUTABLE. Members non-const: the diagnostic table is written and sorted in place.
/// Invalidation: With the schedule arena.
/// Units: Node and resource declaration indices.
/// Errors: None.
/// Version: since Z2 Stage 2. The text formats are those of Architecture/ZenithGraph.md, "Error codes and message formats".
/// @endcontract
struct Zenith_GraphDiagnostic
{
	Zenith_GraphErrorCode m_eCode;
	uint32 m_uNodeA = uGRAPH_INVALID_INDEX;
	uint32 m_uNodeB = uGRAPH_INVALID_INDEX;
	uint32 m_uResource = uGRAPH_INVALID_INDEX;
	uint32 m_uVersion = 0;
	Zenith_StringView m_strText;
};

/// @contract value Zenith_GraphDiagnosticReport
/// Ownership: Caller-owned; Compile and Zenith_GraphReprioritise fill it. Not an error type: the text lives here.
/// Thread affinity: IMMUTABLE once filled; each concurrent Compile uses its own report ([Z2-N-ARCH-320]).
///   Members non-const: an out-parameter that Compile fills.
/// Invalidation: The span and every text in it die when the schedule arena of the call that filled it is reset.
/// Units: None.
/// Errors: None. After a successful call the span is empty.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_GraphDiagnosticReport
{
	Zenith_Span<const Zenith_GraphDiagnostic> m_axDiagnostics;  // in the schedule arena; sorted by code, then node A
};

/// @contract value Zenith_GraphCompileError
/// Ownership: The error record of [Z2-N-CONTRACT-480] (Design/CORE/Diagnostics.md, "Results and the error taxonomy"):
///   16 bytes of fixed-size context, trivially copyable, returned by value from Zenith_Result::Error(); no text.
/// Thread affinity: IMMUTABLE. Members non-const: it is the E of a Zenith_Result, whose move assignment assigns it.
/// Invalidation: None.
/// Units: m_uDiagnosticCount counts the entries of the caller's Zenith_GraphDiagnosticReport.
/// Errors: m_eCode is the code of the report's first diagnostic; m_uDiagnosticCount is at least 1; m_ulStructureHash
///   is the hash of the declarations that failed. ZENITH_TRY propagates within this record type; a front end maps it.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_GraphCompileError
{
	Zenith_GraphErrorCode m_eCode = GRAPH_ERROR_CAPACITY;
	uint32 m_uDiagnosticCount = 0;
	uint64 m_ulStructureHash = 0;
};
static_assert(sizeof(Zenith_GraphCompileError) <= 32);

/// @contract value Zenith_GraphNodeTiming
/// Ownership: Plain value supplied by a front end.
/// Thread affinity: IMMUTABLE. Members non-const: an element of the timing ring, overwritten every frame.
/// Invalidation: None.
/// Units: Nanoseconds relative to the frame start; m_uWorker is the front end's worker or queue index.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_GraphNodeTiming
{
	uint32 m_uNode;
	uint32 m_uWorker;
	uint64 m_ulStartNs;
	uint64 m_ulEndNs;
};

/// @contract value Zenith_GraphTimings
/// Ownership: A view over timings owned by the front end.
/// Thread affinity: IMMUTABLE. Members non-const: a slot of the timing ring, overwritten every frame.
/// Invalidation: With the front end's storage.
/// Units: As Zenith_GraphNodeTiming.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_GraphTimings
{
	uint64 m_ulFrameIndex = 0;
	Zenith_Span<const Zenith_GraphNodeTiming> m_axNodes;
};

/// @contract type Zenith_Graph
/// Ownership: Constructed by a front end with Zenith_TryConstruct in an arena and ended with Zenith_Destroy(xArena,
///   pxGraph) before that arena is reset or destroyed ([Z2-N-CONTRACT-229], [Z2-N-CONTRACT-230]); every allocation it
///   makes comes from that arena, which outlives it. Not trivially destructible: its arena-tier Zenith_Array members
///   declare a destructor (which frees nothing), so the arena counts the graph as a live non-trivial object until
///   Zenith_Destroy. Not copyable, not movable.
/// Thread affinity: ANY_THREAD_EXCEPT(constructor, AddNode, AddResource, DeclareRead, DeclareWrite, DeclareAccess,
///   AddOrdering, MarkOutput, SetFrontEndTag). The listed members run on one thread, before any concurrent reader;
///   Compile, WriteJson and the getters are const and may run concurrently afterwards.
/// Invalidation: Handles die with the graph; schedules from Compile live in their own arena and outlive the graph.
///   References returned by GetNodeDesc and GetResourceDesc are valid until the next AddNode or AddResource.
/// Units: As the descriptor types.
/// Errors: Declaration calls never fail visibly; each error is recorded, and Compile writes every recorded and every
///   derived diagnostic into xReportOut and returns one Zenith_GraphCompileError (first code, count, hash; [Z2-N-ARCH-349]). A declaration that fails returns a poisoned handle (index
///   uGRAPH_INVALID_INDEX, this graph's serial, version uGRAPH_POISON_VERSION); a subsequent call that receives a poisoned
///   handle of this graph records nothing and does not assert. WriteJson with a schedule not compiled from this graph
///   is a programmer error (ZENITH_CHECK). A serial of 0 is a programmer error (ZENITH_CHECK).
/// Reentrancy: NOT_APPLICABLE. The type invokes no callbacks.
/// Version: since Z2 Stage 2. The structure hash and the dump format are stable within schema version 1.
/// Example: Zenith2/Engine/ZenithGraph/Tests/Examples/Zenith_Graph_Example.cpp
/// @endcontract
class Zenith_Graph
{
public:
	Zenith_Graph(Zenith_Arena& xArena, Zenith_StringView strName, uint32 uSerial);
	Zenith_Graph(const Zenith_Graph&) = delete;
	Zenith_Graph& operator=(const Zenith_Graph&) = delete;

	Zenith_GraphResourceHandle AddResource(const Zenith_GraphResourceDesc& xDesc);
	Zenith_GraphNodeHandle AddNode(const Zenith_GraphNodeDesc& xDesc);

	void DeclareRead(Zenith_GraphNodeHandle xNode, Zenith_GraphResourceHandle xResource,
		Zenith_Span<const Zenith_GraphSubrange> axSubranges = {}, uint32 uBackendUsage = 0);

	Zenith_GraphResourceHandle DeclareWrite(Zenith_GraphNodeHandle xNode,
		Zenith_GraphResourceHandle xResource, Zenith_GraphAccessMode eMode,
		Zenith_Span<const Zenith_GraphSubrange> axSubranges = {}, uint32 uBackendUsage = 0);

	Zenith_GraphResourceHandle DeclareAccess(Zenith_GraphNodeHandle xNode,
		const Zenith_GraphAccessDesc& xAccess);

	void AddOrdering(Zenith_GraphNodeHandle xBefore, Zenith_GraphNodeHandle xAfter,
		Zenith_StringView strReason);

	void MarkOutput(Zenith_GraphResourceHandle xResource);

	void SetFrontEndTag(Zenith_StringView strTag);

	Zenith_Result<Zenith_GraphSchedule, Zenith_GraphCompileError> Compile(
		const Zenith_GraphCompileOptions& xOptions, Zenith_Arena& xScheduleArena,
		Zenith_GraphDiagnosticReport& xReportOut) const;

	Zenith_Result<void, Zenith_JsonWriteError> WriteJson(Zenith_JsonWriter& xWriter,
		const Zenith_GraphSchedule* pxSchedule, const Zenith_GraphTimings* pxTimings) const;

	uint32 GetNodeCount() const;
	uint32 GetResourceCount() const;
	Zenith_StringView GetName() const;
	const Zenith_GraphNodeDesc& GetNodeDesc(Zenith_GraphNodeHandle xNode) const;
	const Zenith_GraphResourceDesc& GetResourceDesc(Zenith_GraphResourceHandle xResource) const;
	uint32 GetLatestVersion(Zenith_GraphResourceHandle xResource) const;
	uint32 GetSerial() const;
	bool HasDeclarationErrors() const;
	Zenith_StringView GetFrontEndTag() const;

private:
	// The private layout is Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md, "Declaration tables": the arena reference,
	// the name, the serial, the front-end tag, five arena-tier Zenith_Array tables (nodes, resources, accesses, subranges, orderings),
	// the version-writer table, the error table and the capacity-failed flag. Its size is identical in every configuration.
};

/// @contract functions Zenith_GraphFunctions
/// Ownership: Zenith_GraphReprioritise allocates the new schedule in xScheduleArena, copying every span (the result
///   holds no pointer into the input schedule's arena). The name functions return static strings.
/// Thread affinity: ANY_THREAD. The functions touch only their arguments.
/// Invalidation: None beyond the arenas named.
/// Units: xNewCosts in the front end's cost unit, one entry per node declaration index.
/// Errors: The only failure of Zenith_GraphReprioritise is GRAPH_ERROR_CAPACITY (arena exhausted), with its diagnostic
///   in xReportOut; an xNewCosts whose length differs from the node count is a programmer error (ZENITH_CHECK in every
///   build), because the front end always passes one cost per node index. Zenith_GraphCanonicalBytes returns
///   ALLOC_ERROR_CAPACITY_EXCEEDED when its arena is exhausted. Zenith_GraphDumpVersionMessage returns an empty view when
///   the arena is exhausted.
/// Version: since Z2 Stage 2.
/// @endcontract
Zenith_Result<Zenith_GraphSchedule, Zenith_GraphCompileError> Zenith_GraphReprioritise(
	const Zenith_GraphSchedule& xSchedule, Zenith_Span<const uint32> xNewCosts,
	Zenith_Arena& xScheduleArena, Zenith_GraphDiagnosticReport& xReportOut);

uint64 Zenith_GraphStructureHash(const Zenith_Graph& xGraph);
Zenith_Result<Zenith_ByteSpan, Zenith_AllocError> Zenith_GraphCanonicalBytes(
	const Zenith_Graph& xGraph, Zenith_Arena& xArena);

bool Zenith_GraphDumpVersionIsReadable(uint32 uSchemaVersion);
Zenith_StringView Zenith_GraphDumpVersionMessage(uint32 uSchemaVersion, Zenith_Arena& xArena);

constexpr const char* Zenith_ToString(Zenith_GraphErrorCode eCode);
constexpr const char* Zenith_ToString(Zenith_GraphAccessMode eMode);
constexpr const char* Zenith_ToString(Zenith_GraphEdgeReason eReason);
```

The `Zenith_ToString` strings are the enumerator names without their category prefix: `CYCLE`, `VERSION_FORK`, `UNDEFINED_READ`, `FIRST_WRITE_NOT_DISCARD`, `INVALID_HANDLE`, `SUBRANGE_OUT_OF_BOUNDS`, `SELF_ORDERING`, `CAPACITY`; `READ`, `WRITE`, `READ_WRITE`, `WRITE_DISCARD`; `RAW`, `WAR`, `WAW`, `ORDERING` (Z2-R-GRAPH-041). The `constexpr` bodies are in the header itself, below the declarations, as a `switch` with `ZENITH_UNREACHABLE()` in the default branch.

### Declarations added to the specification

<!-- elaboration -->
| Declaration | Header | Why |
|---|---|---|
| `uGRAPH_POISON_VERSION`, `uGRAPH_MAX_*`, `uGRAPH_MAX_NAME_BYTES` | `Zenith_Graph.h` | the poisoned-handle rule (Z2-R-GRAPH-006) and the capacity limits (Z2-R-GRAPH-014) |
| `uGRAPH_DUMP_SCHEMA_VERSION`, `Zenith_GraphDumpVersionIsReadable`, `Zenith_GraphDumpVersionMessage` | `Zenith_Graph.h` | one place for the dump version and the refusal message readers use [Z2-N-ARCH-341] (Z2-R-GRAPH-057) |
| explicit enumerator values; `Zenith_GraphErrorCode` numbered from 1 | `Zenith_Graph.h` | [Z2-N-CONTRACT-465]; values reach the structure hash and the dump (Z2-R-GRAPH-041) |
| `Zenith_Graph::SetFrontEndTag`, `GetFrontEndTag` | `Zenith_Graph.h` | the dump's `frontEnd` key: `core` by default, `cpu` for the CPU front end, `gpu` for Flux (Z2-R-GRAPH-054) |
| `Zenith_GraphCanonicalBytes` | `Zenith_Graph.h` | the canonical stream the structure hash covers, so the CPU cache can confirm a hash hit byte for byte (Z2-R-GRAPH-076) |
| `Zenith_Graph::GetSerial`, `HasDeclarationErrors` | `Zenith_Graph.h` | front ends skip compile work for a graph already in error and check serial ownership |
| `Zenith_ToString` overloads | `Zenith_Graph.h` | [Z2-N-CONTRACT-466], and the dump and message texts |
| `uFRAME_SUBGRAPH_COUNT`, `uFRAME_GRAPH_MAX_NAME_BYTES`, `Zenith_FrameGraphBuilder::SetTransientSize`, `GetNodeCount`, `GetResourceCount` | `Zenith_FrameGraph.h` | transient scratch placement (Z2-R-GRAPH-053) and bounds |
| `Zenith_FrameNodeRuntime`, `Zenith_FrameTransientDesc`, `Zenith_FrameSubgraphInput`, `Zenith_FrameAssemblyDesc`, `Zenith_AssembleFrameSchedule` | `Zenith_FrameSchedule.h` | the CPU back end's emission from core graphs, used by the builder and by the conformance CPU builder (Z2-R-GRAPH-073) |
| `Zenith_FrameSchedule::WriteSubgraphJson`, `GetGraph`, `GetNodeRuntime`, `GetTransientBytes` | `Zenith_FrameSchedule.h` | one dump document per subgraph [Z2-N-ARCH-429] and executor access |
| `Zenith_FrameScheduleCache` and its descriptor, stats and cost types | `Zenith_FrameSchedule.h` | the cache the specification keys by structure hash [Z2-N-ARCH-407] (Z2-R-GRAPH-076) |
| `Zenith_FrameExecutorDesc` overload of `Create`, `GetCurrentSchedule`, `GetSchedulesInUse`, `GetMeasuredCosts`, `GetLastTimeline`, `GetLastTimings`, `GetTransientStorage`, `WriteFrameDump`, `GetStats`, `Zenith_FrameTimeline`, `Zenith_FrameExecutorStats` | `Zenith_FrameExecutor.h` | pipelining telemetry, the profiler's data, transients, the cache hand-back |
| `Zenith_FixedOrderReduce.h` | ZenithJobs | the fixed-order reduction tree [Z2-N-ARCH-704] as a reusable helper (Z2-R-GRAPH-105) |
| `Zenith_FrameGraphDiagnosticReport` and the report parameters of `Zenith_AssembleFrameSchedule`; `Zenith_FrameSchedule::GetInjectedNodeCount`; `uFRAME_MAX_SCHEDULES_IN_USE` | `Zenith_FrameSchedule.h`, `Zenith_FrameExecutor.h` | assembly outside a builder needs a report for its text ([Z2-N-CONTRACT-480]); the injection-queue reservation of [Z2-N-ARCH-392] (Z2-R-GRAPH-091); the `GetSchedulesInUse` bound |
| `Zenith_FrameGraphDiagnostic::m_uCoreCode` | `Zenith_JobsErrors.h` | the wrapped core code, so callers and the conformance CPU builder can branch on it without parsing text (Z2-R-GRAPH-072) |
| `FRAME_GRAPH_ERROR_*` codes beyond the specification's five | `Zenith_JobsErrors.h` | the validation steps of the translation (Z2-R-GRAPH-062 to Z2-R-GRAPH-069) |

### Zenith_FrameGraph.h

```cpp
// Zenith2/Engine/ZenithJobs/Zenith_FrameGraph.h
#pragma once

#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithGraph/Zenith_Graph.h"
#include "ZenithJobs/Zenith_JobTypes.h"           // Zenith_JobAffinity, Zenith_PfnJob (JOBS)
#include "ZenithJobs/Zenith_JobsErrors.h"

class Zenith_Arena;
class Zenith_Heap;
class Zenith_FrameSchedule;              // Zenith_FrameSchedule.h, which includes this header
struct Zenith_FrameCompileOptions;

/// @contract enum Zenith_FrameSubgraph
/// Ownership: Plain enumeration; the values index per-subgraph arrays.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: A value at or above uFRAME_SUBGRAPH_COUNT records FRAME_GRAPH_ERROR_INVALID_DECLARATION.
/// Version: since Z2 Stage 2. Values never change.
/// @endcontract
enum Zenith_FrameSubgraph : uint8
{
	FRAME_SUBGRAPH_TICK = 0,
	FRAME_SUBGRAPH_EPILOGUE = 1,
	FRAME_SUBGRAPH_RENDER = 2,
};

inline constexpr uint32 uFRAME_SUBGRAPH_COUNT = 3;
inline constexpr uint32 uFRAME_GRAPH_MAX_NAME_BYTES = 127;

/// @contract enum Zenith_FrameNodeKind
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: FRAME_NODE_KIND_SYNC_POINT is generated by the builder alone; declaring it records
///   FRAME_GRAPH_ERROR_INVALID_DECLARATION.
/// Version: since Z2 Stage 2.
/// @endcontract
enum Zenith_FrameNodeKind : uint8
{
	FRAME_NODE_KIND_SYSTEM = 0,
	FRAME_NODE_KIND_BEHAVIOURS = 1,
	FRAME_NODE_KIND_STAGE = 2,
	FRAME_NODE_KIND_ARBITRATION = 3,
	FRAME_NODE_KIND_SYNC_POINT = 4,
	FRAME_NODE_KIND_TICK_BOUNDARY = 5,
};

/// @contract value Zenith_FrameNodeID
/// Ownership: Plain value naming one node declared on one Zenith_FrameGraphBuilder.
/// Thread affinity: IMMUTABLE.
///   Members non-const: a handle is copy-assigned and stored in containers and in other types' members.
/// Invalidation: Frame-scoped (Contracts.md, "Frame-scoped graph handles"): valid for the builder that issued it and,
///   as a key, for schedules compiled from that builder.
/// Units: Declaration index, 0-based.
/// Errors: An index from another builder, or out of range, records FRAME_GRAPH_ERROR_INVALID_DECLARATION.
/// Version: since Z2 Stage 2. Never serialized.
/// @endcontract
struct Zenith_FrameNodeID { uint32 m_uIndex = uGRAPH_INVALID_INDEX; };

/// @contract value Zenith_FrameResourceID
/// Ownership: Plain value naming one resource declared on one Zenith_FrameGraphBuilder.
/// Thread affinity: IMMUTABLE.
///   Members non-const: a handle is copy-assigned and stored in containers and in other types' members.
/// Invalidation: As Zenith_FrameNodeID.
/// Units: Declaration index, 0-based.
/// Errors: As Zenith_FrameNodeID.
/// Version: since Z2 Stage 2. Never serialized.
/// @endcontract
struct Zenith_FrameResourceID { uint32 m_uIndex = uGRAPH_INVALID_INDEX; };

/// @contract value Zenith_FrameNodeDesc
/// Ownership: Plain value; m_szName is copied into the builder arena; m_pData is owned by the registrant and outlives
///   every schedule compiled from this declaration.
/// Thread affinity: IMMUTABLE. Members non-const: callers default-construct it and set the fields they need.
/// Invalidation: None.
/// Units: m_uCostHint in microseconds, clamped to at least 1. m_uStage orders nodes inside (subgraph, group).
/// Errors: A null or over-long name, a null m_pfnRun, FRAME_NODE_KIND_SYNC_POINT, or JOB_AFFINITY_RENDER_SUBMIT outside
///   FRAME_SUBGRAPH_RENDER records FRAME_GRAPH_ERROR_INVALID_DECLARATION.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_FrameNodeDesc
{
	const char* m_szName = nullptr;
	Zenith_FrameSubgraph m_eSubgraph = FRAME_SUBGRAPH_TICK;
	Zenith_FrameNodeKind m_eKind = FRAME_NODE_KIND_STAGE;
	Zenith_JobAffinity m_eAffinity = JOB_AFFINITY_ANY;
	uint16 m_uGroup = 0;
	uint16 m_uStage = 0;
	Zenith_PfnJob m_pfnRun = nullptr;
	void* m_pData = nullptr;
	uint32 m_uCostHint = 1;
};

/// @contract value Zenith_FrameResourceDesc
/// Ownership: Plain value; m_szName is copied into the builder arena.
/// Thread affinity: IMMUTABLE. Members non-const: callers default-construct it and set the fields they need.
/// Invalidation: None.
/// Units: m_ulKey is an opaque unique key (ECS: hash of world tag, type ID, property ID).
/// Errors: A key declared twice in one builder records FRAME_GRAPH_ERROR_DUPLICATE_RESOURCE_KEY.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_FrameResourceDesc
{
	const char* m_szName = nullptr;
	uint64 m_ulKey = 0;
	uint16 m_uGroup = 0;
	bool m_bPersistent = true;
};

/// @contract value Zenith_StageBarrierDesc
/// Ownership: Plain value; m_pData is owned by the registrant and outlives every schedule compiled from it.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: A second barrier for the same (subgraph, group, stage) records FRAME_GRAPH_ERROR_DUPLICATE_STAGE_BARRIER;
///   a null m_pfnRun records FRAME_GRAPH_ERROR_INVALID_DECLARATION.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_StageBarrierDesc
{
	const Zenith_FrameSubgraph m_eSubgraph = FRAME_SUBGRAPH_TICK;
	const uint16 m_uGroup = 0;
	const uint16 m_uStage = 0;
	const Zenith_PfnJob m_pfnRun = nullptr;
	void* const m_pData = nullptr;
};

/// @contract type Zenith_FrameGraphBuilder
/// Ownership: Created on the stack or in an arena by the composition root for one (re)compile; every allocation comes
///   from the arena passed to its constructor. Not copyable.
/// Thread affinity: MAIN_THREAD. Built and compiled on the main thread in the frame loop's recompile step.
/// Invalidation: IDs it issued die with it; schedules it compiled or that a cache acquired from it do not.
/// Units: As the descriptor types.
/// Errors: Declaration calls never fail visibly; a failed Compile (or Zenith_FrameScheduleCache::Acquire) logs every
///   diagnostic, keeps them sorted by (code, node A, node B) in the builder's arena for GetDiagnostics until the builder
///   is destroyed, and returns a Zenith_FrameGraphError with the first code and the count ([Z2-N-ARCH-440]).
///   Arena exhaustion is FRAME_GRAPH_ERROR_OUT_OF_MEMORY.
/// Reentrancy: NOT_APPLICABLE. The builder invokes no callbacks; node bodies run only in the executor.
/// Version: since Z2 Stage 2.
/// Example: Zenith2/Engine/ZenithJobs/Tests/Examples/Zenith_FrameGraph_Example.cpp
/// @endcontract
class Zenith_FrameGraphBuilder
{
public:
	explicit Zenith_FrameGraphBuilder(Zenith_Arena& xArena);
	Zenith_FrameGraphBuilder(const Zenith_FrameGraphBuilder&) = delete;
	Zenith_FrameGraphBuilder& operator=(const Zenith_FrameGraphBuilder&) = delete;

	Zenith_FrameNodeID AddNode(const Zenith_FrameNodeDesc& xDesc);
	Zenith_FrameResourceID AddResource(const Zenith_FrameResourceDesc& xDesc);
	void Read(Zenith_FrameNodeID xNode, Zenith_FrameResourceID xResource);
	void Write(Zenith_FrameNodeID xNode, Zenith_FrameResourceID xResource, bool bAlsoReads);
	void Before(Zenith_FrameNodeID xNode, const char* szOtherNodeName, bool bRequired);
	void After(Zenith_FrameNodeID xNode, const char* szOtherNodeName, bool bRequired);
	void SetStageBarrier(const Zenith_StageBarrierDesc& xDesc);
	void SetTransientSize(Zenith_FrameResourceID xResource, uint64 ulBytes, uint32 uAlignment);

	uint32 GetNodeCount() const;
	uint32 GetResourceCount() const;

	Zenith_Result<Zenith_Owned<Zenith_FrameSchedule>, Zenith_FrameGraphError> Compile(
		const Zenith_FrameCompileOptions& xOptions, Zenith_Heap& xHeap);
	Zenith_Span<const Zenith_FrameGraphDiagnostic> GetDiagnostics() const;   // the last Compile's or Acquire's; in the builder's arena

private:
	// Layout: Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md, "Builder storage".
};
```

`Before(xNode, "B", true)` orders `xNode` before the node named `B`; `After(xNode, "A", true)` orders `xNode` after `A`; `bRequired = false` is the `BeforeIfPresent`/`AfterIfPresent` form of the ECS level. Name forms are `Name` (the declaring node's group), `Name@W<tag>` (group `<tag>` in decimal) and `Name@W*` (that name in every non-zero group present); the full resolution rules are `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Step 1 node names and qualified references" (Z2-R-GRAPH-064). `SetTransientSize` gives a non-persistent resource a byte size and alignment, so the executor places it in per-instance scratch memory by lifetime (Z2-R-GRAPH-053); a persistent resource given a size records `FRAME_GRAPH_ERROR_INVALID_DECLARATION`.

### Zenith_FrameSchedule.h

The complete listing of `Zenith2/Engine/ZenithJobs/Zenith_FrameSchedule.h` (every declaration with its contract block) is `Zenith2/Docs/Design/GRAPH/PublicAPI.md` → "Zenith_FrameSchedule.h listing". It declares `Zenith_FrameCompileOptions`, `Zenith_FrameNodeRuntime`, `Zenith_FrameTransientDesc`, `Zenith_FrameSubgraphInput`, `Zenith_FrameAssemblyDesc`, `Zenith_FrameGraphDiagnosticReport`, `Zenith_FrameSchedule`, `Zenith_AssembleFrameSchedule`, `Zenith_FrameMeasuredCosts`, `Zenith_FrameScheduleCacheDesc`, `Zenith_FrameScheduleCacheStats` and `Zenith_FrameScheduleCache`.

`Zenith_FrameGraph.h` does not include `Zenith_FrameSchedule.h`: it forward-declares `Zenith_FrameSchedule` and `Zenith_FrameCompileOptions`, which is enough for the declaration of `Zenith_FrameGraphBuilder::Compile` (a function declaration may name an incomplete return and parameter type), and `Zenith_FrameSchedule.h` includes `Zenith_FrameGraph.h` for `Zenith_FrameSubgraph`, `uFRAME_SUBGRAPH_COUNT` and the ID types. A caller of `Compile` includes `Zenith_FrameSchedule.h`.

### Zenith_FrameExecutor.h

```cpp
// Zenith2/Engine/ZenithJobs/Zenith_FrameExecutor.h
#pragma once

#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_JsonWriter.h"
#include "ZenithJobs/Zenith_JobsErrors.h"
#include "ZenithJobs/Zenith_FrameGraph.h"
#include "ZenithJobs/Zenith_FrameSchedule.h"

class Zenith_JobSystem;
class Zenith_Heap;

/// @contract value Zenith_FrameParams
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: callers set its fields for each frame.
/// Invalidation: None.
/// Units: m_uTickCount ticks this frame (from Zenith_SimulationClock::Advance).
/// Errors: m_ulFrameIndex not one above the previous call's is a programmer error (ZENITH_CHECK), except the first call.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_FrameParams
{
	uint64 m_ulFrameIndex = 0;
	uint32 m_uTickCount = 0;
	bool m_bRunRender = true;
};

inline constexpr uint32 uFRAME_MAX_SCHEDULES_IN_USE = 4;   // the GetSchedulesInUse bound (covered by the block above)

/// @contract value Zenith_FrameExecutorDesc
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: callers default-construct it and set the fields they need.
/// Invalidation: None.
/// Units: m_uTimingRingFrames in frames (2 to 16); m_uReprioritiseIntervalFrames in frames (0 disables).
/// Errors: Out-of-range values are clamped and logged once. m_pxConsole and m_pxFaultInjector are borrowed for the
///   executor's lifetime; the console registration is the fault point FRAME_EXECUTOR_FAULT_POINT_CONSOLE_REGISTRATION.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_FrameExecutorDesc
{
	uint32 m_uTimingRingFrames = 4;
	uint32 m_uReprioritiseIntervalFrames = 256;
	bool m_bCaptureTimings = true;
	Zenith_DevConsole* m_pxConsole = nullptr;           // DEVT's console (type name per DEVT's design); nullptr registers nothing
	Zenith_FaultInjector* m_pxFaultInjector = nullptr;  // [Z2-N-CONTRACT-225]; nullptr in production
};

enum Zenith_FrameExecutorFaultPoint : uint32         // the non-allocation construction steps ShouldFail is asked about
{
	FRAME_EXECUTOR_FAULT_POINT_CONSOLE_REGISTRATION = 1,
};

/// @contract value Zenith_FrameTimeline
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: a slot of the timeline ring, overwritten every iteration.
/// Invalidation: None.
/// Units: Nanoseconds from Zenith_GetMonotonicTimeNs; 0 for a stamp whose part did not run.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_FrameTimeline
{
	uint64 m_ulFrameIndex = 0;
	uint64 m_ulRenderedFrameIndex = 0;        // the frame RENDER rendered in this iteration (frame index - 1); 0 when no RENDER ran
	uint32 m_uTickCount = 0;
	uint32 m_uPipelineDepth = 0;              // 1 when RENDER ran, else 0
	uint64 m_ulIterationStartNs = 0;
	uint64 m_ulSimulationEndNs = 0;           // completion of the last tick instance (the iteration start when T = 0)
	uint64 m_ulEpilogueEndNs = 0;             // includes Render.Seal
	uint64 m_ulRenderStartNs = 0;
	uint64 m_ulRenderEndNs = 0;
	uint64 m_ulIterationEndNs = 0;
};

/// @contract value Zenith_FrameExecutorStats
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: counters the executor accumulates.
/// Invalidation: None.
/// Units: Counts since creation; nanoseconds where named.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_FrameExecutorStats
{
	uint64 m_ulFrames = 0;
	uint64 m_ulNodesRun = 0;
	uint64 m_ulNodesSkippedCancelled = 0;
	uint64 m_ulScheduleSwaps = 0;
	uint64 m_ulDispatchNs = 0;                // executor-side dispatch time (counter frame.dispatch_ns)
	uint64 m_ulIterationArenaHighWater = 0;
};

/// @contract type Zenith_FrameExecutor
/// Ownership: Created by Create as a Zenith_Owned; owned by ZenithRuntime; destroyed before the job system it borrows
///   (Contracts.md, "Shutdown ownership order", step 7 comes after the executor's destruction in step 6).
/// Thread affinity: MAIN_THREAD. Every member runs on the main thread except GetTransientStorage, which a node body of
///   the named subgraph calls on its own thread while that subgraph's instance runs.
/// Invalidation: Timelines, timings and costs returned by value or span stay valid until the next RunFrame.
/// Units: As Zenith_FrameParams and Zenith_FrameTimeline.
/// Errors: Create returns JOBS_ERROR_OUT_OF_MEMORY on allocation failure after unwinding. RunFrame without a schedule,
///   SetSchedule during RunFrame, and destruction during RunFrame are programmer errors (ZENITH_CHECK).
/// Reentrancy: NONE. A node body calls no executor member except GetTransientStorage.
/// Locks: uLOCK_LEVEL_JOBS_SCHEDULE, around the schedule swap at the start of RunFrame and around GetCurrentSchedule.
/// Version: since Z2 Stage 2.
/// Example: Zenith2/Engine/ZenithJobs/Tests/Examples/Zenith_FrameExecutor_Example.cpp
/// @endcontract
class Zenith_FrameExecutor
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_FrameExecutor>, Zenith_JobsError> Create(
		Zenith_JobSystem& xJobs, Zenith_Heap& xHeap);
	static Zenith_Result<Zenith_Owned<Zenith_FrameExecutor>, Zenith_JobsError> Create(
		Zenith_JobSystem& xJobs, Zenith_Heap& xHeap, const Zenith_FrameExecutorDesc& xDesc);
	~Zenith_FrameExecutor();

	void SetSchedule(const Zenith_FrameSchedule& xSchedule);
	void RunFrame(const Zenith_FrameParams& xParams);
	void RequestCancel();

	const Zenith_FrameSchedule* GetCurrentSchedule() const;
	/// @deviation Errors: returns the count written (at most uFRAME_MAX_SCHEDULES_IN_USE: current, pending, superseded
	///   pending, render); a span shorter than uFRAME_MAX_SCHEDULES_IN_USE is ZENITH_ASSERT.
	uint32 GetSchedulesInUse(Zenith_Span<const Zenith_FrameSchedule*> axOut) const;
	Zenith_FrameMeasuredCosts GetMeasuredCosts() const;
	bool IsReprioritiseDue() const;
	bool IsRenderWorldResyncPending() const;   // m_bResyncRenderWorld: the next epilogue extracts full state
	Zenith_FrameTimeline GetLastTimeline() const;
	Zenith_GraphTimings GetLastTimings(Zenith_FrameSubgraph eSubgraph) const;
	Zenith_MutableByteSpan GetTransientStorage(Zenith_FrameSubgraph eSubgraph,
		Zenith_FrameResourceID xResource) const;
	Zenith_Result<void, Zenith_JsonWriteError> WriteFrameDump(Zenith_FrameSubgraph eSubgraph,
		Zenith_JsonWriter& xWriter) const;
	Zenith_FrameExecutorStats GetStats() const;
};
```

`RequestCancel` is the one member that may also be called from a node body of the running frame (for example the shutdown request path); it only stores a flag (Z2-R-GRAPH-115). The executor's contract therefore lists it as the deviation `/// @deviation Thread affinity: ANY_THREAD. Stores the cancel flag; never blocks.` on its declaration in the implementation.

### Zenith_FixedOrderReduce.h

```cpp
// Zenith2/Engine/ZenithJobs/Zenith_FixedOrderReduce.h
#pragma once

#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithJobs/Zenith_JobContext.h"

using Zenith_PfnReduceCombine = void (*)(const void* pLeft, const void* pRight, void* pOut, void* pUserData);
using Zenith_PfnReduceDone = void (*)(Zenith_JobContext& xContext, const void* pResult, void* pUserData);

/// @contract value Zenith_FixedOrderReduceDesc
/// Ownership: Plain value; m_pUserData is owned by the caller and outlives the reduction.
/// Thread affinity: IMMUTABLE. Members non-const: callers default-construct it and set the fields they need.
/// Invalidation: None.
/// Units: m_uElementBytes and m_uElementAlignment in bytes (alignment a power of two <= 64); m_uParallelGrain in pairs.
/// Errors: A zero element size or a non-power-of-two alignment is a programmer error (ZENITH_ASSERT).
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_FixedOrderReduceDesc
{
	uint32 m_uElementBytes = 0;
	uint32 m_uElementAlignment = 8;
	Zenith_PfnReduceCombine m_pfnCombine = nullptr;
	void* m_pUserData = nullptr;
	uint32 m_uParallelGrain = 1024;
};

/// @contract functions Zenith_FixedOrderReduce
/// Ownership: Scratch comes from the calling node's arena (Zenith_JobContext::GetNodeArena); the result is written to
///   xResult (serial) or passed to pfnDone (parallel) and lives until the node completes.
/// Thread affinity: OWNER_THREAD. Called from inside a frame-graph node or its continuation.
/// Invalidation: The pointer passed to pfnDone is valid only during that call.
/// Units: uCount elements of m_uElementBytes each, in merge order.
/// Errors: uCount == 0 writes nothing and calls pfnDone with nullptr. No other failure.
/// Version: since Z2 Stage 2. The tree shape (level-wise, left-to-right pairs, an odd last element carried up
///   unchanged) never changes; serial and parallel forms produce identical bytes for any worker count.
/// @endcontract
void Zenith_FixedOrderReduceSerial(Zenith_JobContext& xContext, const Zenith_FixedOrderReduceDesc& xDesc,
	Zenith_ByteSpan xElements, uint32 uCount, Zenith_MutableByteSpan xResult);

void Zenith_FixedOrderReduceParallel(Zenith_JobContext& xContext, const Zenith_FixedOrderReduceDesc& xDesc,
	Zenith_ByteSpan xElements, uint32 uCount, Zenith_PfnReduceDone pfnDone, void* pDoneData);
```

### Frame-graph errors in Zenith_JobsErrors.h

The GRAPH declarations of the shared header (the specification's five codes keep their order; the others are added, Z2-R-GRAPH-125):

```cpp
/// @contract enum Zenith_FrameGraphErrorCode
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: Each enumerator's condition and message are the table "Frame-graph error codes" of Design/GRAPH.md.
/// Version: since Z2 Stage 2. Values and meanings never change; a new failure gets a new enumerator.
/// @endcontract
enum Zenith_FrameGraphErrorCode : uint8
{
	FRAME_GRAPH_ERROR_UNORDERED_WRITERS = 1,
	FRAME_GRAPH_ERROR_ORDERING_CONTRADICTS_STAGE = 2,
	FRAME_GRAPH_ERROR_ORDERING_TARGET_MISSING = 3,
	FRAME_GRAPH_ERROR_DUPLICATE_NODE_NAME = 4,
	FRAME_GRAPH_ERROR_GRAPH = 5,
	FRAME_GRAPH_ERROR_INVALID_DECLARATION = 6,
	FRAME_GRAPH_ERROR_DUPLICATE_RESOURCE_KEY = 7,
	FRAME_GRAPH_ERROR_CROSS_GROUP_ACCESS = 8,
	FRAME_GRAPH_ERROR_DUPLICATE_STAGE_BARRIER = 9,
	FRAME_GRAPH_ERROR_OUT_OF_MEMORY = 10,
};

/// @contract value Zenith_FrameGraphDiagnostic
/// Ownership: Plain value; m_strText points into the builder's arena (for GetDiagnostics) or into the diagnostic arena
///   passed to Zenith_AssembleFrameSchedule.
/// Thread affinity: IMMUTABLE. Members non-const: the diagnostic table is written and sorted in place.
/// Invalidation: With that arena.
/// Units: Builder node indices (Zenith_FrameNodeID), or uGRAPH_INVALID_INDEX.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_FrameGraphDiagnostic
{
	Zenith_FrameGraphErrorCode m_eCode;
	uint32 m_uNodeA = uGRAPH_INVALID_INDEX;
	uint32 m_uNodeB = uGRAPH_INVALID_INDEX;
	uint8 m_uCoreCode = 0;           // the Zenith_GraphErrorCode value for FRAME_GRAPH_ERROR_GRAPH, 0 otherwise
	Zenith_StringView m_strText;
};

/// @contract value Zenith_FrameGraphError
/// Ownership: The error record of [Z2-N-CONTRACT-480]: 16 bytes of fixed-size context, trivially copyable, returned by
///   value; the diagnostics are read from the builder (GetDiagnostics) or from the assembly's report.
/// Thread affinity: IMMUTABLE. Members non-const: it is the E of a Zenith_Result, whose move assignment assigns it.
/// Invalidation: None.
/// Units: m_uDiagnosticCount counts the diagnostics kept for the failed call.
/// Errors: m_eCode is the first sorted diagnostic's code; m_uDiagnosticCount is at least 1; m_ulStructureHash is the
///   combined structure hash of the lowered subgraphs (0 when lowering did not run). ZENITH_TRY propagates within it.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_FrameGraphError
{
	Zenith_FrameGraphErrorCode m_eCode = FRAME_GRAPH_ERROR_GRAPH;
	uint32 m_uDiagnosticCount = 0;
	uint64 m_ulStructureHash = 0;
};
static_assert(sizeof(Zenith_FrameGraphError) <= 32);

constexpr const char* Zenith_ToString(Zenith_FrameGraphErrorCode eCode);
```

### Handle kinds used

| Handle | Kind | Owner scope |
|---|---|---|
| `Zenith_GraphNodeHandle`, `Zenith_GraphResourceHandle` | frame-scoped graph handles (`Zenith2/Docs/Contracts.md` → "Handle kinds", "Frame-scoped graph handles") | the issuing `Zenith_Graph`; validated by serial and index |
| `Zenith_FrameNodeID`, `Zenith_FrameResourceID` | frame-scoped graph handles | the issuing builder; used as keys into schedules compiled from it |
| `const Zenith_FrameSchedule*` from the cache | a non-owning pointer with a documented lifetime | the cache, until `ReleaseUnused` |
| `Zenith_Owned<Zenith_FrameSchedule>`, `Zenith_Owned<Zenith_FrameExecutor>`, `Zenith_Owned<Zenith_FrameScheduleCache>` | owned objects (`Zenith2/Docs/Contracts/Memory.md` → "Zenith_Owned") | the caller |

GRAPH defines no persistent ID, world handle, asset handle, device handle or operation handle.

## Data structures and formats

### Summary of in-memory structures

| Structure | Where | Byte size | Detail |
|---|---|---|---|
| node record | ZenithGraph graph arena | 40 | `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Node records" |
| resource record | graph arena | 48 | `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Resource records" |
| access record (internal form of `Zenith_GraphAccessRecord`) | graph arena | 32 | `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Access records and subrange pool" |
| ordering record | graph arena | 24 | `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Ordering records" |
| error record | graph arena | 32 | `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Error records and poisoned handles" |
| `Zenith_GraphSchedule` spans | schedule arena | 4 × N (order) + 4 × N (position) + 4 × N (counts) + 24 × E (edges) + 4 × (N+1) (CSR) + 8 × N (priority) + 8 × R (lifetimes) + 32 × A (accesses) + 4 × (N+1) + 8 × S | the public struct above |
| adjacency | schedule arena | CSR: `m_auSuccessorOffset` (N+1 entries) over `m_axEdges` sorted by (from, to) | kept edges only |
| builder records | builder arena | node 48, resource 40, access 16, ordering 24, barrier 24 | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Builder storage" |
| schedule structure and binding | schedule arena (per `Zenith_FrameSchedule`) | structure: three graphs and three core schedules; binding: 32 bytes per node run plus zone sites | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Structures and bindings" |
| node instance | executor iteration arena | 16 (`std::atomic<uint32>` pending count, `uint32` node, instance pointer) | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Executor storage" |
| subgraph instance | executor iteration arena | 80 plus the node instances, the timing slots and the transient block | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Executor storage" |

The records, instances and `std::atomic<uint32>` counters stored in arenas are trivially destructible. A `Zenith_Graph` is not: its arena-tier `Zenith_Array` members declare a destructor, so the arena's live non-trivial count includes it ([Z2-N-CONTRACT-229]), and every owner ends it with `Zenith_Destroy(xArena, pxGraph)` before resetting or destroying that arena, because `Reset`, `ResetToMarker` and the arena's destructor `ZENITH_CHECK` that no such object is live in every configuration ([Z2-N-CONTRACT-230]). The owners are: the schedule cache (a failed or hit `Acquire` ends the lowering's graphs before destroying the lowering arena; destroying a root ends its three graphs before its arena, `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Structures and bindings", "Acquire"); a `Zenith_FrameSchedule` that owns its structure (its destruction does the same); `Zenith_AssembleFrameSchedule` on failure; the conformance builders' owned arenas; and the Flux front end, which ends its per-frame graph with `Zenith_Destroy` before it resets its frame arena. Compile scratch (`Zenith_Array<Zenith_GraphCandidateEdge>`, the text writer's `Zenith_Array<char>`) is local to `Compile`, whose destructors run normally.

<!-- assumption: pending ARCH -->
[Z2-N-ARCH-319] still reads that every object stored in a graph or schedule arena is trivially destructible; Architecture is rewording it (CROSS_DOC_ITEMS X58) to "graph captures are trivially destructible ([Z2-N-CONTRACT-232]); every other object stored in a graph or schedule arena is trivially destructible or is ended with `Zenith_Destroy` before the arena is reset ([Z2-N-CONTRACT-229])". This design follows the reworded form.
<!-- /assumption -->

### Canonical structure serialization

The structure hash ([Z2-N-ARCH-338]) is `Zenith_Hash64` over this byte stream, all integers little-endian, streamed through `Zenith_Hash64Stream` in this order with no scratch; `Zenith_GraphCanonicalBytes` writes the same bytes into a caller arena (Z2-R-GRAPH-047):

| Order | Field | Encoding |
|---|---|---|
| 1 | magic | the 8 bytes `ZGRAPHS1` |
| 2 | node count N | `uint32` |
| 3 | per node, in declaration order | name length `uint32`, name bytes, `m_uLane` `uint32`, `m_uFlags` `uint32` |
| 4 | resource count R | `uint32` |
| 5 | per resource, in declaration order | name length `uint32`, name bytes, `m_uSubresourceCount` `uint32`, flags `uint32` (including the OUTPUT bit set by `MarkOutput`) |
| 6 | access count A | `uint32` |
| 7 | per access, in declaration order | node `uint32`, resource `uint32`, mode `uint8`, three zero bytes, version read `uint32`, version written `uint32`, subrange count `uint32`, then per subrange first `uint32` and count `uint32` (an empty span is written as the one subrange `{0, m_uSubresourceCount}`), backend usage `uint32` |
| 8 | ordering count O | `uint32` |
| 9 | per ordering, in declaration order | before `uint32`, after `uint32` |

Costs, backend payloads, ordering reasons, the graph name, the serial and the front-end tag are excluded, so two front ends' graphs with identical structure hash equal. The CPU front end combines the three subgraph hashes into the frame schedule's hash as `Zenith_Hash64` over the 36 bytes `ZFRAME01` followed by `uFRAME_LOWERING_VERSION` (`uint32`, value 1, bumped by any change to the translation of `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Translation pipeline") and the three `uint64` subgraph hashes in `Zenith_FrameSubgraph` order, with 0 for an empty subgraph (Z2-R-GRAPH-076).

### Graph dump schema zenith2.graph-dump version 1

The dump's content and ordering rules are `Zenith2/Docs/Architecture/ZenithGraph.md` → "JSON dump". This design fixes the presence rules and gives the JSON Schema (draft 2020-12). The committed copy is `Zenith2/Engine/ZenithGraph/Tests/Data/zenith2.graph-dump.v1.schema.json`, and every golden dump is validated against it (Z2-R-GRAPH-054, Z2-R-GRAPH-055, Z2-R-GRAPH-056).

The dump is written into a writer the caller constructs; the caller chooses the style and calls `Finish` (`Zenith2/Docs/Design/CORE/Text.md` → "JSON writer"). Every golden file and every byte-for-byte comparison uses `JSON_WRITE_STYLE_COMPACT`, which has no whitespace at all; `jobs.dump` and the profiler's "Dump graph" button use `JSON_WRITE_STYLE_PRETTY` (tabs and LF), which is validated against the schema but never compared byte for byte. The array layout of every array is fixed by `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "JSON writing".

Presence rules:

| Key | Present when |
|---|---|
| `schema`, `schemaVersion`, `graph`, `frontEnd`, `structureHash`, `nodes`, `resources`, `accesses`, `orderings` | always |
| `frame` | timings are passed (`m_ulFrameIndex`) |
| `edges`, `schedule`; per node `culled`; per resource `lifetime` | a schedule is passed |
| per node `position`, `priority`, `dependencyCount` | a schedule is passed and the node is not culled |
| `timings` | timings are passed |
| per access `versionRead` | the mode reads (`READ`, `READ_WRITE`) |
| per access `versionWritten` | the mode writes (`WRITE`, `READ_WRITE`, `WRITE_DISCARD`) |
| per edge `resource`, `version` | the reason is not `ORDERING` |
| per edge `ordering` | the reason is `ORDERING` (the ordering's declaration index) |
| per resource `lifetime` value `null` | the resource has no non-culled access |

Integers below 2^53 are JSON numbers; any integer at or above 2^53 (only priorities can reach it) is written as a `0x`-prefixed 16-digit lower-case hexadecimal string, and 64-bit hashes and payloads are always hexadecimal strings.

The complete JSON Schema is `Zenith2/Docs/Design/GRAPH/Formats.md` → "Graph dump JSON Schema".

`additionalProperties: true` is how "readers ignore unknown keys" is expressed; the key order the writer uses is the order of `Zenith2/Docs/Architecture/ZenithGraph.md` → "JSON dump", which the schema cannot state and the golden-file test pins.

### Frame dump schema zenith2.frame-dump version 1

One document per subgraph ([Z2-N-ARCH-429]); the core document is embedded verbatim under `core` ([Z2-N-ARCH-347]); `Zenith_FrameSchedule::WriteJson` writes the three documents of one schedule as a JSON array (layout `JSON_ARRAY_LAYOUT_MULTILINE`) in `Zenith_FrameSubgraph` order (Z2-R-GRAPH-058, Z2-R-GRAPH-059). Key order: `schema`, `schemaVersion`, `subgraph`, `frame`, `scheduleHash`, `core`, `runs`, `affinity`, `transients`, `timeline`.

The complete JSON Schema is `Zenith2/Docs/Design/GRAPH/Formats.md` → "Frame dump JSON Schema".

`frame` and `timeline` are present in the executor's dump (`WriteFrameDump`, which passes the last timings and timeline; the caller owns the writer's style and `Finish`, as for the graph dump) and absent in a schedule-only dump (`WriteSubgraphJson` without timings). The committed copy is `Zenith2/Engine/ZenithJobs/Tests/Data/zenith2.frame-dump.v1.schema.json`.

## Algorithms and state machines

### Algorithm index

| Algorithm | Specified in | Complexity |
|---|---|---|
| declarations, sticky errors, poisoned handles | `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Declaration algorithms" | O(1) amortised per call plus O(S) subranges |
| the thirteen compile steps (flatten, hazards, interval map, edge dedup, Tarjan, cycle path, cull, priority, Kahn order, transitive reduction, counts, lifetimes, hash, diagnostics) | `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Compile pipeline" | `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Complexity and memory summary" |
| reprioritisation | `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Reprioritisation" | O((N + E) log N) |
| JSON writing and diagnostic text | `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "JSON writing", "Diagnostic text formats" | O(N + R + A + E) |
| translation (validation, names, keys, fences, orderings, version threading, lowering) | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Translation pipeline" | O(D log D) for D declarations |
| emission (run tables, affinity counts, transient placement, zone sites, combined hash) | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Schedule emission" | O(N + E) run tables; transient placement O(E · ⌈T_acc / 64⌉ + T²) for T sized transients with T_acc accessor nodes |
| schedule cache | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Schedule cache" | O(C) lookup for C ≤ 16 entries plus O(bytes) compare |
| frame execution | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Frame executor" | O(N + E) per subgraph instance, plus O(R_n log R_n) per completion for R_n ready successors |
| fixed-order reduction | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Fixed-order reduction" | O(n) combines, O(log n) levels |

### State machines

The executor walks four state machines, specified as tables of states × events in `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Executor state machines": the iteration machine (`EXECUTOR_STATE_IDLE`, `EXECUTOR_STATE_RUNNING`, `EXECUTOR_STATE_DRAINING`), the subgraph instance machine (`INSTANCE_STATE_UNUSED`, `INSTANCE_STATE_RUNNING`, `INSTANCE_STATE_COMPLETE`), the node instance machine (`NODE_STATE_WAITING`, `NODE_STATE_READY`, `NODE_STATE_RUNNING`, `NODE_STATE_BODY_DONE`, `NODE_STATE_COMPLETE`) and the schedule cache entry machine (`CACHE_ENTRY_STATE_LIVE`, `CACHE_ENTRY_STATE_IDLE`). ZenithGraph itself has no state machine beyond the graph's two states: accepting declarations, and capacity-failed (every further declaration is a no-op returning a poisoned handle).

## Threading lifetime and error contract

### Thread affinity per type

| Type | Affinity keyword | Detail |
|---|---|---|
| `Zenith_Graph` | `ANY_THREAD_EXCEPT(constructor, AddNode, AddResource, DeclareRead, DeclareWrite, DeclareAccess, AddOrdering, MarkOutput, SetFrontEndTag)` | declarations on one thread; `Compile`, `WriteJson`, getters concurrently afterwards, each compile with its own schedule arena (Z2-R-GRAPH-017) |
| `Zenith_GraphSchedule` and every value type of `Zenith_Graph.h` | `IMMUTABLE` | — |
| `Zenith_FrameGraphBuilder` | `MAIN_THREAD` | — |
| `Zenith_FrameSchedule` | `IMMUTABLE` | — |
| `Zenith_FrameScheduleCache` | `MAIN_THREAD` | — |
| `Zenith_FrameExecutor` | `MAIN_THREAD` | `GetTransientStorage` from node bodies; `RequestCancel` from any thread (deviation) |
| `Zenith_FixedOrderReduce*` | `OWNER_THREAD` | inside a node, its sub-jobs or continuations |

### Frame-graph nodes this area runs

GRAPH declares no node of its own and runs every declared node through its trampoline. It generates one kind of node, the stage fence (`FRAME_NODE_KIND_SYNC_POINT`, name `Sync.<Subgraph>.G<group>.S<stage>`), whose body is the declared stage barrier's body or empty; a fence declares no resource access and is ordered by `AddOrdering` alone (`Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Step 3 stage fences"). The schedule recompile itself is not a node: it runs on the main thread between iterations inside the zone `Frame.ScheduleRecompile`, which the composition root opens around its rebuild and `Acquire` call and which Z2-B-GRAPH-001 measures (Z2-R-GRAPH-082).

### Lock-order entries

GRAPH uses exactly one lock: the executor's schedule mutex, `Zenith_LockDesc{ "jobs.schedule", uLOCK_LEVEL_JOBS_SCHEDULE /*300*/, false, false }`, the existing row of `Zenith2/Docs/Contracts.md` → "Threading and concurrency contract" (owner ZenithJobs, guards the compiled-schedule swap, main thread). It is taken by `RunFrame` for the swap and by `GetCurrentSchedule` and `GetSchedulesInUse`, and never from a job (Z2-R-GRAPH-114). ZenithGraph takes no lock. The cache takes none (main thread only). No GRAPH lock row is added.

### Memory ordering

| Atomic | Operation | Order | Why |
|---|---|---|---|
| node pending counter | decrement on predecessor completion | `acq_rel` | the thread reaching 0 acquires every predecessor's writes before pushing the node |
| instance remaining counter | decrement per node completion | `acq_rel` | the thread reaching 0 sees every node's writes before launching the next instance |
| iteration parts counter | decrement per EPILOGUE and RENDER completion | `acq_rel` | the last part sees both parts' writes |
| iteration-done flag | store 1 by the last part; load in `Zenith_JobsRunMainUntil` | `release` / `acquire` | `RunFrame` returns with every write of the iteration visible to the main thread |
| cancel flag | store by `RequestCancel`; load in the trampoline | `release` / `acquire` | a node started after the store skips its body |
| pending counters at launch | initial store | `relaxed` | published by the release in the job push |

Each atomic is `std::atomic<uint32>` from `<atomic>`, every operation names its order ([Z2-N-CONTRACT-312]).

### Cancellation

`RequestCancel` implements [Z2-N-ARCH-412]: nodes not yet started complete immediately without running their body, running bodies observe `Zenith_JobContext::IsCancelled()` through `m_puCancel` and return at their next check, every counter still reaches zero, and `RunFrame` returns. After a cancelled iteration the executor is `EXECUTOR_STATE_IDLE` and usable; the composition root calls `RequestCancel` for world destruction and, when MOD's shutdown request uses it (§ "Shutdown"), for shutdown (Z2-R-GRAPH-115).

Sealed is decided by the flag, not by counting skipped bodies: EPILOGUE(k) is sealed exactly when the cancel flag is clear as the epilogue's completing thread reads it (acquire). So a cancel that lands while `Render.Seal` runs, and makes it return early at its cancel check, leaves frame k unsealed although no body was skipped; `m_uSkippedBodies` is a counter only. An unsealed frame is never rendered ([Z2-N-ARCH-417]): iteration k+1 launches no RENDER(k), its pipeline depth is 0, and the screen keeps the last presented image. A cancel that lands after EPILOGUE(k) completed leaves world k sealed, and iteration k+1 renders it. RENDER(k) reads ring slot k % 2 only when EPILOGUE(k) completed with the cancel flag clear; a slot left in any other state (untouched, half-written, written but unsealed) is read by no one and is reset by the epilogue that next reuses it ([Z2-N-ARCH-418]).

A render world carries deltas, not full state ([Z2-N-REND-008]), so a world that is never consumed drops its deltas. After any iteration in which the cancel flag was set, the executor therefore sets `m_bResyncRenderWorld`; the next epilogue that seals a render world performs a full-state extraction instead of a delta, and the executor clears the flag at that seal. The invariant: the render world a RENDER instance consumes is the delta successor of the world the previous RENDER instance consumed, or a full-state extraction. The extract nodes read the flag through the render-world writer, and Rendering documents the full-extraction path it shares with device-loss recovery (CROSS_DOC_ITEMS X57); the flag, its ordering, its ring interaction and its test are in `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Cancellation path". Background-job cancellation is JOBS' ([Z2-N-ARCH-425]).

### Shutdown

The executor's place in `Zenith2/Docs/Contracts.md` → "Shutdown ownership order" (Z2-R-GRAPH-116):

<!-- assumption: pending MOD -->
The composition root (MOD) runs this sequence; the Contracts table's step 6 does not yet name the executor and the cache, and the cross-area notes ask MOD to add them in that order.

1. Step 2: a shutdown request takes effect when the current iteration returns, as [Z2-N-ARCH-426] and `Zenith2/Docs/Architecture/CompositionRoot.md` → "The frame loop" step 4 state: `RunFrame(N+1)` completes (simulation N+1 finishes and RENDER(N) submits), and the composition root calls no further `RunFrame`. The in-flight render, the render world N+1 that EPILOGUE(N+1) extracted, is discarded and never rendered; nothing is presented after the request, and the resync flag is irrelevant because no RENDER follows. The executor is idle. Whether the request also calls `RequestCancel`, so that the iteration ends at the current tick boundary with world N+1 unsealed, is MOD's choice; the executor's behaviour is the same either way. MOD aligns its shutdown sequence and frame-loop driver with this (CROSS_DOC_ITEMS X57).
2. Step 6: the runtime destroys the executor with the other subsystems it owns, before the job system; the executor's destructor checks (`ZENITH_CHECK`) that no iteration is running, releases its iteration arena to the heap it was created with, and joins nothing (it starts no thread).
3. The schedule cache is destroyed after the executor, because the executor may hold pointers to cached schedules until its destruction.
4. Step 7: the job system joins its threads (JOBS).
<!-- /assumption -->

### Bounded queues and full policies

| Structure | Capacity | Full policy | Counter names |
|---|---|---|---|
| per-completion ready batch (stack array in the completion path) | 64 launches; a larger ready set is pushed in several batches of 64, each sorted, in descending priority order overall | none needed: batches are consecutive, never dropped | — |
| iteration arena | computed exactly from the current schedules at swap time (FrameGraphInternals.md, "Executor storage") | `QUEUE_FULL_POLICY_FATAL` semantics: exceeding it is a defect, `ZENITH_FATAL(FATAL_CODE_ARENA_EXHAUSTED)` | `frame.iteration_arena.high_water` |
| timing ring | `m_uTimingRingFrames` frames (default 4) | `QUEUE_FULL_POLICY_DROP_OLDEST` | `frame.timings.dropped`, `frame.timings.high_water` |
| pending schedule slot | 1 | `QUEUE_FULL_POLICY_DROP_OLDEST` (a newer `SetSchedule` supersedes the pending one; the superseded one is reported by `GetSchedulesInUse` until the next `RunFrame` so the cache does not release it early) | `frame.schedule.superseded` |
| schedule cache | `m_uCapacity` structures (default 4) | `QUEUE_FULL_POLICY_DROP_OLDEST` over unpinned structures (least recently acquired), temporary overshoot by one when all are pinned | `frame.schedule_cache.evictions`, `frame.schedule_cache.over_capacity` |
| affinity job queues | reserved by `SetSchedule` through `Zenith_JobsReserveAffinityCapacity` from `GetAffinityNodeCount` | `QUEUE_FULL_POLICY_FATAL` (JOBS' queue) | `jobs.main.full`, `jobs.render_submit.full` |
| injection queue (the render-submit thread's pushes of ANY successors) | reserved by `SetSchedule` through `Zenith_JobsReserveInjectionCapacity` as `max(m_uInjectionCapacity, 2 × GetInjectedNodeCount() + 256)` | `QUEUE_FULL_POLICY_FATAL` for the render-submit producer (JOBS' queue) | `jobs.injection.full` |

`GetAffinityNodeCount(e)` returns the sum over the three subgraphs of the scheduled nodes with affinity `e` (one tick instance counted once, because tick instances run in sequence). At any moment each main-affinity node has at most one job in its queue (the node itself or its next continuation, [Z2-N-ARCH-395]), and at most one tick instance or the epilogue plus the render instance are live, so JOBS' capacity of 2 × that count + 256 is never reached; `Test_FrameSchedule_AffinityNodeCountBoundsQueue` checks the bound under stress (Z2-R-GRAPH-091).

### Error codes

`Zenith_GraphErrorCode` is § "Zenith_Graph.h"; its conditions and message formats are `Zenith2/Docs/Architecture/ZenithGraph.md` → "Error codes and message formats", and `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Other diagnostics" gives every field of every format (Z2-R-GRAPH-040).

Frame-graph error codes (Z2-R-GRAPH-124, Z2-R-GRAPH-125):

| Code | Raised when | Message format (the diagnostic text) |
|---|---|---|
| `FRAME_GRAPH_ERROR_UNORDERED_WRITERS` | two nodes of one (subgraph, group, stage) write one frame resource | `frame graph: '<Resource>' has <n> writers in <Subgraph> group <g> stage <s>: '<A>', '<B>'[, …]; order them by stage or merge them through an arbitration node` |
| `FRAME_GRAPH_ERROR_ORDERING_CONTRADICTS_STAGE` | `Before` targets a node of a preceding stage of the same group, or `After` a node of a subsequent stage | `schedule: '<A>' (phase <P>) declares Before('<B>') but '<B>' runs in preceding phase <Q>` for `Before`; for `After`: `schedule: '<A>' (phase <P>) declares After('<B>') but '<B>' runs in subsequent phase <Q>` (both forms are [Z2-N-ARCH-404]'s; "phase" is the stage number) |
| `FRAME_GRAPH_ERROR_ORDERING_TARGET_MISSING` | a required `Before`/`After` names no node of the same subgraph | `frame graph: '<A>' declares <Before|After>('<name>') but no node '<name>' exists in <Subgraph> group <g>` (for `@W*`: `in any world of <Subgraph>`) |
| `FRAME_GRAPH_ERROR_DUPLICATE_NODE_NAME` | two nodes of one (subgraph, group) share a name | `frame graph: node name '<name>' declared twice in <Subgraph> group <g> (declarations <i> and <j>)` |
| `FRAME_GRAPH_ERROR_GRAPH` | the core compile of a subgraph failed | the core diagnostic text, unchanged, prefixed `frame graph: <Subgraph>: ` |
| `FRAME_GRAPH_ERROR_INVALID_DECLARATION` | a null or over-long name, a null body, a subgraph value out of range, a declared SYNC_POINT kind, RENDER_SUBMIT affinity outside RENDER, a foreign or out-of-range ID, `SetTransientSize` on a persistent resource or with a zero size or bad alignment, a second TICK_BOUNDARY node in one group, a TICK_BOUNDARY outside the TICK subgraph or not in its group's lowest stage, an `@` in a node name | `frame graph: invalid declaration of '<name or #index>': <reason>` with `<reason>` one of `null name`, `name longer than 127 bytes`, `null body`, `subgraph <v> out of range`, `SYNC_POINT is generated`, `RENDER_SUBMIT affinity outside Render`, `id <i> not issued by this builder`, `transient size on a persistent resource`, `transient size <b> or alignment <a> invalid`, `second tick boundary in group <g>`, `tick boundary not first in group <g>`, `'@' in node name` |
| `FRAME_GRAPH_ERROR_DUPLICATE_RESOURCE_KEY` | two `AddResource` calls carry one `m_ulKey` | `frame graph: resource key 0x<key> declared twice ('<A>' and '<B>')` |
| `FRAME_GRAPH_ERROR_CROSS_GROUP_ACCESS` | a node of group g accesses a resource of group h ≠ g | `frame graph: '<node>' (group <g>) accesses '<resource>' of group <h>; resources are per world` |
| `FRAME_GRAPH_ERROR_DUPLICATE_STAGE_BARRIER` | two barriers for one (subgraph, group, stage) | `frame graph: stage barrier declared twice for <Subgraph> group <g> stage <s>` |
| `FRAME_GRAPH_ERROR_OUT_OF_MEMORY` | the builder arena, a schedule arena or the heap failed | `frame graph: out of memory while <step>` with `<step>` one of `declaring`, `translating`, `compiling <Subgraph>`, `assembling`, `caching` |

`<Subgraph>` is `Tick`, `Epilogue` or `Render`. Every compile reports every error in one pass, sorted by (code, node A, node B), and a jobs-level error found before lowering stops the lowering of that subgraph only (the other subgraphs are still lowered and compiled, so their core errors are reported too) (Z2-R-GRAPH-072).

### Front-end error mapping

The CPU front end reports core errors as `Zenith2/Docs/Architecture/ZenithGraph.md` → "Error codes and message formats" (table "Error codes by front end") states: every core diagnostic becomes one `FRAME_GRAPH_ERROR_GRAPH` diagnostic carrying the core text unchanged after the prefix, `GRAPH_ERROR_VERSION_FORK` cannot arise because `FRAME_GRAPH_ERROR_UNORDERED_WRITERS` is diagnosed first and the lowering then declares no second writer, and `GRAPH_ERROR_UNDEFINED_READ` and `GRAPH_ERROR_FIRST_WRITE_NOT_DISCARD` cannot arise because the lowering sets neither flag. The builder formats every diagnostic into its own arena and returns only the record; the ECS level reads `GetDiagnostics()` and wraps each as `SCHEDULE_ERROR_GRAPH` (`Zenith2/Docs/Architecture/FramePhases.md` → "Schedule-compile errors"). Node indices in `Zenith_FrameGraphDiagnostic` are builder IDs: the lowering maps core node indices back through the run table's `m_uBuilderNode`; a generated fence maps to `uGRAPH_INVALID_INDEX`.

### Asserts versus results

| Condition | Mechanism |
|---|---|
| a foreign or out-of-range graph handle, an invalid enumerator or unknown flag bits | `ZENITH_ASSERT` plus a recorded error (declarations are engine code; callers are engine front ends) |
| a graph serial of 0; `WriteJson` with a foreign schedule; executor misuse (RunFrame without schedule, SetSchedule or destruction during RunFrame); a frame index that does not advance by one | `ZENITH_CHECK` |
| wrong affinity at the trampoline (a JOBS defect) | `ZENITH_CHECK` |
| iteration arena exhaustion | `ZENITH_FATAL(FATAL_CODE_ARENA_EXHAUSTED)` |
| every declaration mistake a module, a data schema or a hot-reloaded script system can cause | a `Zenith_Result` error, never an assert |

No GRAPH code raises a Luau error: a script system's schedule error reaches SCRIPT as `SCHEDULE_ERROR_GRAPH` through ECS, and SCRIPT rejects the reload ([Z2-N-ARCH-409], Z2-R-GRAPH-079).

## Diagnostics and profiling

### Profiling zones

| Zone | Opened by | Thread | Name source |
|---|---|---|---|
| `<NodeName>` for every declared node | the node trampoline, around the body | the thread running the body | the node's name in the schedule arena, via one `Zenith_ZoneSite` per node built at assembly |
| `Sync.<Subgraph>.G<group>.S<stage>` | the trampoline, around a fence body | as above | generated name |
| `<NodeName>/[<begin>,<end>)`, `<NodeName>/cont<n>` | JOBS, for sub-jobs and continuations | their threads | the node's zone site passed in the launch ([Z2-N-ARCH-427]) |
| `Frame.RunFrame` | `RunFrame`, main thread, whole iteration | main | literal |
| `Frame.ScheduleRecompile` | the composition root around rebuild and `Acquire` | main | literal; measured by Z2-B-GRAPH-001 |
| `Graph.Compile` | `Zenith_Graph::Compile` | caller | literal |

The node span that MP-CPUZONE measures (start of the body to completion including joined sub-jobs) is the executor's timing record (start stamp in the trampoline, end stamp in `m_pfnOnComplete`), exposed through `GetLastTimings`, because a zone cannot begin on one thread and end on another (Z2-R-GRAPH-118).

### Counters

Published every frame through the counter hook (Z2-R-GRAPH-119):

| Counter | Meaning |
|---|---|
| `frame.schedule_recompiles` | recompiles (cache misses) this frame ([Z2-N-ARCH-428]) |
| `frame.schedule_cache.hits`, `frame.schedule_cache.misses`, `frame.schedule_cache.collisions`, `frame.schedule_cache.evictions`, `frame.schedule_cache.over_capacity` | cache activity |
| `frame.reprioritisations` | reprioritisations applied |
| `frame.schedule.superseded` | pending schedules replaced before use |
| `frame.nodes_run`, `frame.nodes_skipped_cancelled` | per frame |
| `frame.dispatch_ns` | executor-side time in launch and completion paths (an input to Z2-B-JOBS-001) |
| `frame.critical_path_us` | the root priority of the tick subgraph, in µs (the scheduled critical path) |
| `frame.iteration_arena.high_water` | bytes |
| `frame.timings.dropped`, `frame.timings.high_water` | timing ring |
| `frame.cost_table_mismatch` | measured-cost spans ignored |
| `pipeline.depth` | 1 when RENDER ran this iteration, else 0 ([Z2-N-ARCH-421]) |
| `graph.compiles`, `graph.compile_us`, `graph.reduction_skipped` | core compiles run by the CPU front end, their time, skipped reductions |

### Log categories and messages

Category `graph` (ZenithGraph never logs itself; front ends log its results) and category `frame`:

| Message | Level | When |
|---|---|---|
| `frame: schedule compiled: hash 0x<h>, nodes <p>/<t>/<e>/<r>, edges <n>, <us> us` | info | a cache miss compiled |
| `frame: schedule cache hit 0x<h>` | debug | a hit |
| `frame: schedule compile failed; keeping 0x<old>` followed by each diagnostic text on its own line | error | a failed recompile |
| `graph: transitive reduction skipped for '<graph>': <n> nodes above the limit <l>` | warning | once per graph name per process ([Z2-N-ARCH-334], Z2-R-GRAPH-120) |
| `frame: reprioritised 0x<h> from measured costs` | debug | a reprioritisation |
| `frame: executor iteration arena <bytes> bytes for schedule 0x<h>` | debug | at each swap |

### On-screen profiler rows

<!-- assumption: pending DEVT -->
The on-screen profiler (DEVT) draws from GRAPH: the per-worker timeline from the node timings and zones; the frame timeline bars (ticks, epilogue, render; the prologue is the gap before the iteration start) from `Zenith_FrameTimeline`; the counters above; and the graph view from the frame dump. GRAPH owns the data, DEVT the drawing. Every DEVT reader of a `zenith2.graph-dump` or `zenith2.frame-dump` document refuses a `schemaVersion` above `uGRAPH_DUMP_SCHEMA_VERSION` with the text of `Zenith_GraphDumpVersionMessage` ([Z2-N-ARCH-341]).
<!-- /assumption -->

### Console commands

Registered by the executor in development builds only (Z2-R-GRAPH-121).

<!-- assumption: pending DEVT -->
DEVT's development console offers a registration call taking a command name, a handler function pointer with user data and a one-line help text, callable on the main thread by a subsystem the composition root passes the console to, and absent (a no-op) in Shipping builds; the executor registers the two commands below through it and unregisters them in its destructor.
<!-- /assumption -->

| Command | Effect |
|---|---|
| `jobs.recompile` | marks the schedule dirty; the next frame loop recompile step rebuilds and re-acquires, bypassing the cache (a forced miss) |
| `jobs.dump <path>` | writes `<path>/frame_<k>_<Subgraph>.json` for the three subgraphs of the current schedule, in `JSON_WRITE_STYLE_PRETTY`, with the last timings, through `WriteFrameDump` ([Z2-N-ARCH-429]) |

### Frame-dump entries and telemetry

In development builds `WriteFrameDump` is callable every frame by the profiler. Telemetry per frame: `pipeline.depth`, the frame timeline stamps (for the input-latency attribution of [Z2-N-ARCH-422]; the input sequence and present stamps are RPROD's), `frame.schedule_recompiles`.

## Authoring and cooking

GRAPH has no authored data and no cooked data: the schedule is derived at run time from registrations made in code and from data-defined declarations that other areas cook (component schemas: ECS; script systems: SCRIPT). There is no importer, no cook step and no migration hook. The graph dumps are diagnostic output under `Zenith2/out/` or the device's user data directory, read by DEVT's on-screen profiler and graph viewer; they are never inputs. The one committed data artefact is the pair of JSON Schema files under `Tests/Data/`, test data owned by this area.

## Failure handling

### Declaration errors

A declaration error never stops declaration: it is recorded, a poisoned handle is returned, and `Compile` reports it with every other error (Z2-R-GRAPH-005, Z2-R-GRAPH-006, Z2-R-GRAPH-039). The CPU builder behaves the same way with its own codes.

### Arena exhaustion

`Zenith_Graph` allocates with `TryAllocate`; the first failure records one `GRAPH_ERROR_CAPACITY` with the counts reached, sets the capacity-failed flag, and turns every subsequent declaration into a no-op returning a poisoned handle. `Compile` first reserves one diagnostic and 512 text bytes; if even that fails, `xReportOut` receives a one-entry span over a static `constexpr` diagnostic with text `ZenithGraph: schedule arena exhausted before compile`, and the returned record carries `GRAPH_ERROR_CAPACITY` with a count of 1. A capacity failure never crashes and never asserts (Z2-R-GRAPH-014).

### Compile failure at startup

A failing first `Acquire` stops startup: the builder has already logged every diagnostic ([Z2-N-ARCH-440]); the composition root reads `GetDiagnostics()` for its startup report and exits with code 3 (`Zenith2/Docs/Architecture/CompositionRoot.md` → "Exit codes"; [Z2-N-ARCH-409]).

### Compile failure on recompile

A failing `Acquire` leaves the cache, the executor's current schedule and every live binding unchanged; the composition root keeps running the previous schedule, reads the builder's `GetDiagnostics()` (already logged), and rejects the change that triggered the recompile (for a script hot reload, SCRIPT reverts to the previous module declaration) (Z2-R-GRAPH-079).

### Allocation failure in the cache and executor

`Zenith_FrameScheduleCache::Create` and `Zenith_FrameExecutor::Create` unwind exactly what they built when a step fails ([Z2-N-CONTRACT-340]); each step has a failure-injection test (Z2-R-GRAPH-117): allocation steps through `Zenith_HeapDesc::m_ulFailAfterAllocations`, and the executor's console registration through `Zenith_FrameExecutorDesc::m_pxFaultInjector` at `FRAME_EXECUTOR_FAULT_POINT_CONSOLE_REGISTRATION` ([Z2-N-CONTRACT-225]). An `Acquire` whose heap allocation fails returns `FRAME_GRAPH_ERROR_OUT_OF_MEMORY` and changes nothing. The executor's iteration arena is sized exactly at swap time; its block is allocated with `TryAllocate` at swap time, and a failure there keeps the previous schedule current and reports `JOBS_ERROR_OUT_OF_MEMORY` through the log (the swap is retried at the next `RunFrame`).

### Cancellation mid-frame

§ "Cancellation" (Z2-R-GRAPH-115).

### Budget breach

Z2-B-GRAPH-001 is `BR-ACCEPT`: a breach blocks the stage acceptance measuring it and files a BUG on the GRAPH epic (`Zenith2/Docs/Budgets.md` → "Purpose versioning and change rule"). There is no run-time response to a slow recompile; it shows as a hitch frame in the hitch rows.

### Device loss and headless runs

GRAPH has no device. On device loss or in a headless configuration the composition root passes `m_bRunRender = false`, and no RENDER instance runs (Z2-R-GRAPH-099); the render subgraph stays in the schedule unchanged.

## Test strategy

### Test targets and gate steps

| Target | Gate step | Presets |
|---|---|---|
| `ZenithGraph_Tests`, `ZenithJobs_Tests` (GRAPH files), `ZenithGraph_Conformance` | `Test` | `win-debug`, `win-release` |
| the same | `Sanitizers -Preset win-asan`, `Sanitizers -Preset linux-tsan` (category `Zenith2Memory`, change-triggered by the job-graph paths) | `win-asan`, `linux-tsan` (no Flux builder) |
| `z2-death-*` suites of the GRAPH files | `Test` (child processes) | every preset their suite names |
| `z2-stress` tests (`Test_FrameGraph_StressUnderTsan` and the conformance random runs) | `Test`; `Sanitizers -Preset linux-tsan` | as above |
| `Zenith_Bench_GraphCompile`, `Zenith_Bench_ScheduleRecompile` smoke | `Test` | `win-release` |

### Test index

Every test, its file and its assertion is in `Zenith2/Docs/Design/GRAPH/Tests.md`: "Graph core unit tests", "Graph conformance suite" (with the scenario table, the per-builder expectations and the Flux builder mapping), "CPU frame graph tests" and "Benchmarks". The specification's test tables (`Zenith2/Docs/Architecture/ZenithGraph.md` → "Conformance suite", `Zenith2/Docs/Architecture/CPUFrameGraph.md` → "Required tests") are included there by name, each with its owner.

### Fixtures and golden files

`Zenith2/Docs/Design/GRAPH/Tests.md` → "Fixtures and golden files".

### Sanitizer and stress runs

Every GRAPH concurrency path has a `linux-tsan` stress test with 2, 8 and the host's hardware-concurrency workers ([Z2-N-CONTRACT-357]): `Test_GraphConformance_ConcurrentCompileOfOneGraph`, `Test_FrameGraph_StressUnderTsan`, `Test_FrameExecutor_CancelStress` (Z2-R-GRAPH-123).

## Budget rows

### Rows this area owns

| Row | Name | Status | Procedure | Workloads | Breach | Producer |
|---|---|---|---|---|---|---|
| Z2-B-GRAPH-001 | CPU frame-graph schedule recompile | pending | MP-CPUZONE over the zone `Frame.ScheduleRecompile` | W-OpenWorld, W-Editor | BR-ACCEPT | Stage 1 per-system budget ticket for GRAPH |

### Rows this area is measured against

| Row | Name | GRAPH's contribution |
|---|---|---|
| Z2-B-JOBS-001 | Scheduler overhead per frame | the executor's dispatch time, counter `frame.dispatch_ns`, is part of the scheduler overhead JOBS measures |
| Z2-B-JOBS-003 | CPU critical-path allocation headroom | critical-path priorities order ready work; `frame.critical_path_us` reports the scheduled critical path |
| Z2-B-JOBS-002 | Bounded catch-up fixed steps per frame | the executor's 8-bit instance ordinal bounds T at 253 per frame (`Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Invocation IDs"); the row's value (JOBS range 1 to 64) stays below it (Z2-R-GRAPH-104) |
| Z2-B-RPROD-021 | CPU render lag (adopted: 1 frame) | the iteration barrier guarantees it; `pipeline.depth` evidences it (Z2-R-GRAPH-096, Z2-R-GRAPH-097) |
| Z2-B-RPROD-027 | Input-to-photon latency | the frame timeline stamps (Z2-R-GRAPH-098) |
| Z2-B-RHI-004 | Render graph build and compile CPU | the core compile Flux runs each frame is part of it |
| Z2-B-DEVT-002 | On-screen profiler overhead CPU | timing capture and per-frame dumps in development builds |

No new row is needed.

### Measurement harness

`Zenith2/Benchmarks/Budgets/GRAPH/` (Stage 1 harness ticket): `Zenith_Bench_ScheduleRecompile` builds the frame graphs of the workload games that exist (synthetic replicas of W-OpenWorld's and W-Editor's registrations until those games exist: 12 phases × 2 worlds × 60 systems, 1,800 property resources, 4 extractors per world) and times rebuild plus `Acquire` with a forced miss, 10 repetitions, median and 95% CI; its CTest smoke `Test_BudgetHarness_Graph_Smoke` runs one repetition (Z2-R-GRAPH-002).

## Stage plan

### Stage 1

| Ticket | Kind | Content | Gated by |
|---|---|---|---|
| The CPU frame-graph and ZenithGraph specification (Roadmap Stage 1 item 10) | decision | reviews `Zenith2/Docs/Architecture/ZenithGraph.md`, `Zenith2/Docs/Architecture/CPUFrameGraph.md` and this design against the JOBS, ECS, MOD and RHI designs, resolves the GRAPH rows of the cross-area notes, and appends the confirmation ruling to `Zenith2/Docs/DecisionLog.md` with its own `Z2-D-NNN` (Z2-R-GRAPH-001) | Z2-D-006, Z2-D-050, Z2-D-080 |
| Stage 1 per-system budget harness for GRAPH | harness | § "Measurement harness" (Z2-R-GRAPH-002) | the specification ticket |
| Stage 1 per-system budget ticket for GRAPH | budget (`needs-human`) | runs the harness on the reference desktop and the S24, writes Z2-B-GRAPH-001 (Z2-R-GRAPH-003) | the harness; the CORE, JOBS and RPROD budget tickets |

### Stage 1 pull-forward

Once the specification ticket is Done, the ZenithGraph core has no unresolved dependency (its CORE dependencies are the `Zenith_Arena`, `Zenith_Result` and `Zenith_Span` contracts plus the assumed CORE types), so these Stage 2 tickets are pulled into `Stage 1 — Architecture validation` with fix version `Z2 Stage 2` (`Zenith2/Docs/Roadmap.md` → "Pull-forward rule"):

1. **API-only:** `Zenith_Graph.h` with every contract block, the value types' inline members, the `Zenith_ToString` bodies, the compiled example restricted to value types, and `Test_Graph_ValueTypes_*`; `HeaderSelfContained` and `ApiContractCheck` green. Non-inline members are declared and not yet referenced by any test, so the library links.
2. Declaration tables on `Zenith_Array` arena tier, declarations, sticky errors, poisoned handles, capacity.
3. Hazard derivation and the interval map (compile steps 1 to 3).
4. Cycles and diagnostic text (steps 4, 5, 13).
5. Culling, priority, order, reduction, counts, lifetimes (steps 6 to 11).
6. Structure hash, `Zenith_GraphStructureHash`, `Zenith_GraphReprioritise`.
7. JSON dump, the schema file, the golden file.
8. The conformance suite with the core builder, the random property tests and `Zenith_Bench_GraphCompile`.

### Stage 2

In dependency order (each ticket in `Stage 2 — Foundation`, fix version `Z2 Stage 2`):

1. **API-only:** `Zenith_FrameGraph.h`, `Zenith_FrameSchedule.h`, `Zenith_FrameExecutor.h`, `Zenith_FixedOrderReduce.h` and the GRAPH declarations of `Zenith_JobsErrors.h`, with contract blocks. This unblocks ECS (`EmitInto`), SCRIPT (`RegisterScriptNodes`) and MOD (the frame loop) to compile against the builder. Gated by the JOBS API-only ticket for `Zenith_JobSystem.h` and `Zenith_JobContext.h`.
2. Builder storage and translation steps 0 to 5, with their tests.
3. Lowering (steps 6 and 7) and `Zenith_AssembleFrameSchedule` with run tables, affinity counts, zone sites and transient placement.
4. The schedule cache and reprioritisation.
5. The executor core: instances, the trampoline, completion, sequencing, affinity, cancellation. Gated by the JOBS job-system implementation ticket and the JOBS `Zenith_JobNodeLaunch` seam.
6. Pipelining: the iteration barrier, the render subgraph across swaps, the timeline, `pipeline.depth`.
7. Timings, the cost EWMA, the frame dump, console commands, counters.
8. `Zenith_FixedOrderReduce`.
9. The CPU conformance builder.
10. The determinism suite and the `linux-tsan` stress tests.
11. The Flux conformance builder, gated by the RHI render-graph compile ticket (it is written in `Zenith2/Engine/ZenithFlux/Tests/GraphConformance/`, test code only).
12. The first measurement of Z2-B-GRAPH-001 in the workloads that exist (Z2-R-GRAPH-082), as part of the Stage 2 budget confirmation (`Zenith2/Docs/Roadmap.md` → "Stage 2 Foundation").

### Stages 3 to 7

| Stage | GRAPH work |
|---|---|
| Stage 3 | none new: the loopback proof (NET) runs headless with `m_bRunRender = false`, which Z2-R-GRAPH-099 already covers |
| Stage 4 | Z2-B-GRAPH-001 met on W-OpenWorld and W-Editor, Windows and S24, in the Stage 4 acceptance (Z2-R-GRAPH-083) |
| Stage 5 | none new: the render view-set trigger of advanced multi-view (RPROD) uses the existing recompile path |
| Stage 6 | none new; the Stage 6 sustained runs include Z2-B-GRAPH-001 in their evidence |
| Stage 7 | none: dedicated servers (NET) reuse the headless path; XR adds no GRAPH concept |

### Ticket slices

Every Stage 1 and Stage 2 slice above is one TASK of at most one library and roughly 8 files; tests land in the same ticket as their code ([Z2-N-CONTRACT-410]). The two API-only tickets come first in their stages so that dependents become claimable as early as possible.

## Decisions

### Settled decisions governing the area

| Decision | What it fixes for GRAPH |
|---|---|
| Z2-D-006 | the CPU frame is a DAG on the shared ZenithGraph core; no fibers; compiled, cached schedule; sub-jobs joined before completion; affinity; bounded queues; orderly shutdown |
| Z2-D-050 | frame pipelining through render extraction: one frame of CPU render lag |
| Z2-D-055 | the GPU render graph is built on ZenithGraph every frame in an arena (the Flux front end of the core and its conformance builder) |
| Z2-D-080 | the phase and stage structure the CPU frame graph schedules |
| Z2-D-019 | the determinism guarantee: schedule and merge order never depend on workers or timing |
| Z2-D-040 | arena-only allocation, `Zenith_Owned`, no smart pointers |
| Z2-D-041 | no custom container: GRAPH uses CORE's `Zenith_Array` and sort functions |
| Z2-D-045 | `Zenith_Result` errors, asserts only for programmer errors |
| Z2-D-046 | no mutable global state: the executor, cache and graphs hold every piece of state as members |
| Z2-D-047 | one contract block per public type |
| Z2-D-012 | Z2-B-GRAPH-001 and the rows GRAPH is measured against |

### Open decisions the area depends on

| Decision | Stage | Effect on GRAPH |
|---|---|---|
| D-SEL-HASH (open, Stage 1) | 1 | the function behind `Zenith_Hash64`; the structure hash and the golden dump's `structureHash` change with it, so the golden file is regenerated by the ruling's revalidation |
| D-SEL-JSON (open, Stage 1) | 1 | the backing of `Zenith_JsonWriter`; the dump bytes are fixed by this design regardless |
| D-PHASES refinement (open, Stage 1) | 1 | may change phase-to-stage mapping in ECS; GRAPH is unaffected because it never interprets stage numbers |
| D-SEL-HEAP (open, Stage 1) | 1 | none: GRAPH allocates through `Zenith_Heap` and arenas only |

### Selection and classification decisions owned

GRAPH owns no selection or classification decision.

### Custom infrastructure records this area adds

GRAPH adds no custom container, allocator, heap wrapper or threading primitive, so it appends no justification record. Every growable table (the node, resource, access, subrange, ordering and error tables of `Zenith_Graph`, and every record table of `Zenith_FrameGraphBuilder`) is a `Zenith_Array<T>` in its arena tier (`Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_Array"), grown with `TryPushBack`; a failed push leaves the table unchanged and turns into `GRAPH_ERROR_CAPACITY` or `FRAME_GRAPH_ERROR_OUT_OF_MEMORY`. The interval map, the edge arrays, the executor's instance arrays and the transient placement arrays are plain arena allocations sized exactly before they are filled (Z2-R-GRAPH-016).

## Requirement index

| Requirement | Design headings |
|---|---|
| Z2-R-GRAPH-001 | `Zenith2/Docs/Design/GRAPH.md` → "Stage 1"; `Zenith2/Docs/Design/GRAPH.md` → "Relationship to the specification" |
| Z2-R-GRAPH-002 | `Zenith2/Docs/Design/GRAPH.md` → "Measurement harness"; `Zenith2/Docs/Design/GRAPH/Tests.md` → "Benchmarks" |
| Z2-R-GRAPH-003 | `Zenith2/Docs/Design/GRAPH.md` → "Rows this area owns"; `Zenith2/Docs/Design/GRAPH.md` → "Stage 1" |
| Z2-R-GRAPH-004 | `Zenith2/Docs/Design/GRAPH.md` → "Zenith_Graph.h"; `Zenith2/Docs/Design/GRAPH.md` → "Declarations added to the specification"; `Zenith2/Docs/Design/GRAPH.md` → "ZenithGraph" |
| Z2-R-GRAPH-005 | `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Declaration algorithms"; `Zenith2/Docs/Design/GRAPH.md` → "Declaration errors" |
| Z2-R-GRAPH-006 | `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Error records and poisoned handles" |
| Z2-R-GRAPH-007 | `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Error records and poisoned handles" |
| Z2-R-GRAPH-008 | `Zenith2/Docs/Design/GRAPH.md` → "Zenith_Graph.h"; `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Step 6 lowering into ZenithGraph" |
| Z2-R-GRAPH-009 | `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "DeclareRead DeclareWrite and DeclareAccess"; `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Step 2 hazard derivation per resource" |
| Z2-R-GRAPH-010 | `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "DeclareRead DeclareWrite and DeclareAccess"; `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "AddNode and AddResource" |
| Z2-R-GRAPH-011 | `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "AddOrdering and MarkOutput"; `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "AddNode and AddResource" |
| Z2-R-GRAPH-012 | `Zenith2/Docs/Design/GRAPH.md` → "Canonical structure serialization"; `Zenith2/Docs/Design/GRAPH.md` → "Zenith_Graph.h" |
| Z2-R-GRAPH-013 | `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Compile step list"; `Zenith2/Docs/Design/GRAPH/Tests.md` → "Scenario table" |
| Z2-R-GRAPH-014 | `Zenith2/Docs/Design/GRAPH.md` → "Arena exhaustion"; `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Step 13 diagnostics assembly"; `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Name storage" |
| Z2-R-GRAPH-015 | `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Name storage" |
| Z2-R-GRAPH-016 | `Zenith2/Docs/Design/GRAPH.md` → "Custom infrastructure records this area adds"; `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Table storage" |
| Z2-R-GRAPH-017 | `Zenith2/Docs/Design/GRAPH.md` → "Thread affinity per type"; `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Compile step list" |
| Z2-R-GRAPH-018 | `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "DeclareRead DeclareWrite and DeclareAccess" |
| Z2-R-GRAPH-019 | `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "DeclareRead DeclareWrite and DeclareAccess"; `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Other diagnostics" |
| Z2-R-GRAPH-020 | `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Step 1 flatten and group accesses"; `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Step 2 hazard derivation per resource" |
| Z2-R-GRAPH-021 | `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Step 2 hazard derivation per resource" |
| Z2-R-GRAPH-022 | `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Step 2 hazard derivation per resource" |
| Z2-R-GRAPH-023 | `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Step 2 hazard derivation per resource" |
| Z2-R-GRAPH-024 | `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Step 2 hazard derivation per resource" |
| Z2-R-GRAPH-025 | `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "The subresource interval map" |
| Z2-R-GRAPH-026 | `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Step 2 hazard derivation per resource" |
| Z2-R-GRAPH-027 | `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Step 2 hazard derivation per resource" |
| Z2-R-GRAPH-028 | `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Step 3 edge sort and deduplication" |
| Z2-R-GRAPH-029 | `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "AddOrdering and MarkOutput"; `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Step 3 edge sort and deduplication" |
| Z2-R-GRAPH-030 | `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Step 3 edge sort and deduplication" |
| Z2-R-GRAPH-031 | `Zenith2/Docs/Design/GRAPH/Tests.md` → "Scenario table"; `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Determinism proof obligations" |
| Z2-R-GRAPH-032 | `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "The subresource interval map" |
| Z2-R-GRAPH-033 | `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Step 6 culling" |
| Z2-R-GRAPH-034 | `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Step 6 culling" |
| Z2-R-GRAPH-035 | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Step 7 compile and wrap" |
| Z2-R-GRAPH-036 | `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Step 4 cycle detection" |
| Z2-R-GRAPH-037 | `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Step 5 cycle path extraction and text" |
| Z2-R-GRAPH-038 | `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Cycle block"; `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Worked examples" |
| Z2-R-GRAPH-039 | `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Compile step list"; `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Step 13 diagnostics assembly" |
| Z2-R-GRAPH-040 | `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Other diagnostics"; `Zenith2/Docs/Design/GRAPH.md` → "Error codes" |
| Z2-R-GRAPH-041 | `Zenith2/Docs/Design/GRAPH.md` → "Zenith_Graph.h" |
| Z2-R-GRAPH-042 | `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Step 8 execution order" |
| Z2-R-GRAPH-043 | `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Step 7 critical-path priority" |
| Z2-R-GRAPH-044 | `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Step 9 transitive reduction" |
| Z2-R-GRAPH-045 | `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Step 10 dependency counts and successor ranges" |
| Z2-R-GRAPH-046 | `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Reprioritisation" |
| Z2-R-GRAPH-047 | `Zenith2/Docs/Design/GRAPH.md` → "Canonical structure serialization"; `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Step 12 structure hash" |
| Z2-R-GRAPH-048 | `Zenith2/Docs/Design/GRAPH.md` → "Zenith_Graph.h"; `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Compile step list" |
| Z2-R-GRAPH-049 | `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Complexity and memory summary"; `Zenith2/Docs/Design/GRAPH/Tests.md` → "Benchmarks" |
| Z2-R-GRAPH-050 | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Determinism proof obligations" |
| Z2-R-GRAPH-051 | `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Step 11 lifetimes" |
| Z2-R-GRAPH-052 | `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Step 11 lifetimes" |
| Z2-R-GRAPH-053 | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Transient scratch placement" |
| Z2-R-GRAPH-054 | `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "JSON writing"; `Zenith2/Docs/Design/GRAPH.md` → "Graph dump schema zenith2.graph-dump version 1" |
| Z2-R-GRAPH-055 | `Zenith2/Docs/Design/GRAPH.md` → "Graph dump schema zenith2.graph-dump version 1" |
| Z2-R-GRAPH-056 | `Zenith2/Docs/Design/GRAPH/Formats.md` → "Graph dump JSON Schema"; `Zenith2/Docs/Design/GRAPH/Formats.md` → "Frame dump JSON Schema" |
| Z2-R-GRAPH-057 | `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "JSON writing" |
| Z2-R-GRAPH-058 | `Zenith2/Docs/Design/GRAPH.md` → "Frame dump schema zenith2.frame-dump version 1" |
| Z2-R-GRAPH-059 | `Zenith2/Docs/Design/GRAPH.md` → "Frame dump schema zenith2.frame-dump version 1"; `Zenith2/Docs/Design/GRAPH.md` → "Console commands" |
| Z2-R-GRAPH-060 | `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "JSON writing" |
| Z2-R-GRAPH-061 | `Zenith2/Docs/Design/GRAPH.md` → "Zenith_FrameGraph.h"; `Zenith2/Docs/Design/GRAPH/PublicAPI.md` → "Zenith_FrameSchedule.h listing"; `Zenith2/Docs/Design/GRAPH.md` → "Zenith_FrameExecutor.h" |
| Z2-R-GRAPH-062 | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Step 0 validation"; `Zenith2/Docs/Design/GRAPH.md` → "Error codes" |
| Z2-R-GRAPH-063 | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Step 1 node names and qualified references" |
| Z2-R-GRAPH-064 | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Step 1 node names and qualified references" |
| Z2-R-GRAPH-065 | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Step 3 stage fences" |
| Z2-R-GRAPH-066 | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Step 4 ordering resolution" |
| Z2-R-GRAPH-067 | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Step 5 version threading" |
| Z2-R-GRAPH-068 | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Step 5 version threading" |
| Z2-R-GRAPH-069 | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Step 2 resource keys and groups" |
| Z2-R-GRAPH-070 | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Step 2 resource keys and groups"; `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Step 3 stage fences" |
| Z2-R-GRAPH-071 | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Step 6 lowering into ZenithGraph" |
| Z2-R-GRAPH-072 | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Step 7 compile and wrap"; `Zenith2/Docs/Design/GRAPH.md` → "Front-end error mapping" |
| Z2-R-GRAPH-073 | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Schedule emission"; `Zenith2/Docs/Design/GRAPH/PublicAPI.md` → "Zenith_FrameSchedule.h listing" |
| Z2-R-GRAPH-074 | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Run tables"; `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Affinity counts and queue sizing" |
| Z2-R-GRAPH-075 | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "RunFrame step by step" |
| Z2-R-GRAPH-076 | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Acquire"; `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Structures and bindings"; `Zenith2/Docs/Design/GRAPH/PublicAPI.md` → "Zenith_FrameSchedule.h listing" |
| Z2-R-GRAPH-077 | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Cache invalidation triggers" |
| Z2-R-GRAPH-078 | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Release at iteration end"; `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "RunFrame step by step" |
| Z2-R-GRAPH-079 | `Zenith2/Docs/Design/GRAPH.md` → "Compile failure on recompile"; `Zenith2/Docs/Design/GRAPH.md` → "Compile failure at startup" |
| Z2-R-GRAPH-080 | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Reprioritise"; `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Timing capture and cost measurement" |
| Z2-R-GRAPH-081 | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Cache invalidation triggers" |
| Z2-R-GRAPH-082 | `Zenith2/Docs/Design/GRAPH.md` → "Frame-graph nodes this area runs"; `Zenith2/Docs/Design/GRAPH.md` → "Rows this area owns" |
| Z2-R-GRAPH-083 | `Zenith2/Docs/Design/GRAPH.md` → "Rows this area owns"; `Zenith2/Docs/Design/GRAPH.md` → "Stages 3 to 7" |
| Z2-R-GRAPH-084 | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Node completion"; `Zenith2/Docs/Design/GRAPH.md` → "The executor to job-system seam" |
| Z2-R-GRAPH-085 | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Determinism proof obligations" |
| Z2-R-GRAPH-086 | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Node completion" |
| Z2-R-GRAPH-087 | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "The node trampoline"; `Zenith2/Docs/Design/GRAPH.md` → "What this area does not own" |
| Z2-R-GRAPH-088 | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "The node trampoline" |
| Z2-R-GRAPH-089 | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "The node trampoline" |
| Z2-R-GRAPH-090 | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Step 0 validation" |
| Z2-R-GRAPH-091 | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Affinity counts and queue sizing"; `Zenith2/Docs/Design/GRAPH.md` → "Bounded queues and full policies" |
| Z2-R-GRAPH-092 | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Push order and priority" |
| Z2-R-GRAPH-093 | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Frame executor iteration state machine"; `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Instance completion and sequencing" |
| Z2-R-GRAPH-094 | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Instance completion and sequencing" |
| Z2-R-GRAPH-095 | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "The iteration barrier" |
| Z2-R-GRAPH-096 | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "The iteration barrier"; `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Hand-off to Flux_RenderWorld" |
| Z2-R-GRAPH-097 | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Frame timeline and latency stamps"; `Zenith2/Docs/Design/GRAPH.md` → "Counters" |
| Z2-R-GRAPH-098 | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Frame timeline and latency stamps" |
| Z2-R-GRAPH-099 | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Headless and first-frame behaviour" |
| Z2-R-GRAPH-100 | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Hand-off to Flux_RenderWorld" |
| Z2-R-GRAPH-101 | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Release at iteration end" |
| Z2-R-GRAPH-102 | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Determinism proof obligations" |
| Z2-R-GRAPH-103 | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Determinism proof obligations" |
| Z2-R-GRAPH-104 | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Invocation IDs"; `Zenith2/Docs/Design/GRAPH.md` → "Rows this area is measured against" |
| Z2-R-GRAPH-105 | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Fixed-order reduction"; `Zenith2/Docs/Design/GRAPH.md` → "Zenith_FixedOrderReduce.h" |
| Z2-R-GRAPH-106 | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "The node trampoline" |
| Z2-R-GRAPH-107 | `Zenith2/Docs/Design/GRAPH/Tests.md` → "Test files"; `Zenith2/Docs/Design/GRAPH/Tests.md` → "Scenario model" |
| Z2-R-GRAPH-108 | `Zenith2/Docs/Design/GRAPH/Tests.md` → "Comparison rules"; `Zenith2/Docs/Design/GRAPH/Tests.md` → "Expectations per builder" |
| Z2-R-GRAPH-109 | `Zenith2/Docs/Design/GRAPH/Tests.md` → "Builders" |
| Z2-R-GRAPH-110 | `Zenith2/Docs/Design/GRAPH/Tests.md` → "The Flux builder mapping" |
| Z2-R-GRAPH-111 | `Zenith2/Docs/Design/GRAPH.md` → "Test targets and gate steps"; `Zenith2/Docs/Design/GRAPH.md` → "Supported matrix" |
| Z2-R-GRAPH-112 | `Zenith2/Docs/Design/GRAPH/Tests.md` → "Scenario table" |
| Z2-R-GRAPH-113 | `Zenith2/Docs/Design/GRAPH.md` → "Thread affinity per type"; `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "RunFrame step by step" |
| Z2-R-GRAPH-114 | `Zenith2/Docs/Design/GRAPH.md` → "Lock-order entries" |
| Z2-R-GRAPH-115 | `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Cancellation path"; `Zenith2/Docs/Design/GRAPH.md` → "Cancellation" |
| Z2-R-GRAPH-116 | `Zenith2/Docs/Design/GRAPH.md` → "Shutdown" |
| Z2-R-GRAPH-117 | `Zenith2/Docs/Design/GRAPH.md` → "Allocation failure in the cache and executor"; `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Executor storage" |
| Z2-R-GRAPH-118 | `Zenith2/Docs/Design/GRAPH.md` → "Profiling zones"; `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Zone sites" |
| Z2-R-GRAPH-119 | `Zenith2/Docs/Design/GRAPH.md` → "Counters" |
| Z2-R-GRAPH-120 | `Zenith2/Docs/Design/GRAPH.md` → "Log categories and messages" |
| Z2-R-GRAPH-121 | `Zenith2/Docs/Design/GRAPH.md` → "Console commands" |
| Z2-R-GRAPH-122 | `Zenith2/Docs/Design/GRAPH.md` → "Bounded queues and full policies"; `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Executor storage" |
| Z2-R-GRAPH-123 | `Zenith2/Docs/Design/GRAPH.md` → "Sanitizer and stress runs"; `Zenith2/Docs/Design/GRAPH/Tests.md` → "Stress and sanitizer tests" |
| Z2-R-GRAPH-124 | `Zenith2/Docs/Design/GRAPH.md` → "Error codes" |
| Z2-R-GRAPH-125 | `Zenith2/Docs/Design/GRAPH.md` → "Frame-graph errors in Zenith_JobsErrors.h" |
