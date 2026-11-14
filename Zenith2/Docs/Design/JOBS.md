# JOBS design — Jobs & frame scheduling

The design of the JOBS area: the work-stealing scheduler that executes the compiled CPU frame-graph schedule, its dynamic sub-jobs and continuations, background operations and their cancellation, thread affinity, contention diagnostics, the simulation clock and orderly shutdown. It elaborates `Zenith2/Docs/Architecture/CPUFrameGraph.md` (the specification of the scheduler, queues, affinity, the clock and shutdown) and `Zenith2/Docs/Contracts.md → "Threading and concurrency contract"`, and never restates their bullets; where it adds a detail a worker needs, it says so. The requirement catalogue is `Zenith2/Docs/Requirements/JOBS.md`.

This entry document holds the skeleton and the top-level contracts. Three split files hold the rest:

| File | Holds | Its `##` headings |
|---|---|---|
| `Zenith2/Docs/Design/JOBS/PublicAPI.md` | every public declaration of the JOBS headers, with contract blocks | Header list · Zenith_JobsErrors header · Zenith_JobTypes header · Zenith_JobContext header · Zenith_JobSystem header · Zenith_SimulationClock header · Zenith_FloatEnvironment header |
| `Zenith2/Docs/Design/JOBS/Algorithms.md` | internal structures, algorithms with memory orders, state machines, complexity | Internal types and files · Thread state and the no-global rule · Job records and the memory model · Work-stealing deque · Bounded MPMC queue · Queues and their full policies · Worker loop · Main thread helping · Render-submit thread · Parking and wake-up · Job invocation · Stages segments and continuations · ParallelFor splitting and inline execution · Node launch seam · Background jobs and operations · Cancellation semantics · Simulation clock arithmetic · Floating-point environment application · Create and failure unwinding · Shutdown sequence · State machines · Complexity |
| `Zenith2/Docs/Design/JOBS/TestCatalogue.md` | every test with its case, file, suite and gate steps | Test files · JobSystem tests · JobContext tests · Deque and queue tests · Node launch seam tests · SimulationClock tests · FloatEnvironment tests · Profiling tests · Stress and sanitizer tests · Death and defect tests · Examples · Benchmarks · Tests JOBS depends on in other libraries |

`<!-- assumption: pending <AREA> -->` marks an interface JOBS assumes from a design that is being written concurrently or a core-document inconsistency JOBS resolved one way; each is listed in `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md` for reconciliation.

## Purpose and scope

### What JOBS owns

| Piece | Where | Specified in |
|---|---|---|
| the job system: threads, per-thread deques, queues, the worker loop, parking, stealing, main-thread helping, the render-submit thread | `Zenith_JobSystem`, `Internal/Jobs/` | `Zenith2/Docs/Design/JOBS/Algorithms.md → "Worker loop"` and neighbours |
| job contexts, `ParallelFor`, per-node join counters, continuation lists, the leaf partition | `Zenith_JobContext` | `Zenith2/Docs/Design/JOBS/Algorithms.md → "Stages segments and continuations"` |
| the node launch seam that GRAPH's executor drives: routing by affinity, take order, completion callbacks, main-thread helping, iteration begin and end | `Internal/Jobs/Zenith_JobNodeLaunch.h` | `Zenith2/Docs/Design/JOBS/Algorithms.md → "Node launch seam"` |
| background operations and their cancellation | `Zenith_JobSystem::SubmitBackground*` | `Zenith2/Docs/Design/JOBS/Algorithms.md → "Background jobs and operations"` |
| the simulation clock: fixed steps, interpolation alpha, pause, time scale, bounded catch-up, external ticks | `Zenith_SimulationClock`, `Internal/Clock/` | `Zenith2/Docs/Design/JOBS/Algorithms.md → "Simulation clock arithmetic"` |
| the per-thread floating-point environment `Zenith_FloatEnvironmentState` (cross-doc item X25) | `Zenith_FloatEnvironment.h` | `Zenith2/Docs/Design/JOBS/PublicAPI.md → "Zenith_FloatEnvironment header"` |
| contention diagnostics: scheduler counters, sub-job, continuation and background zones, per-thread profiler lanes | the job system | § "Diagnostics and profiling" |
| orderly shutdown of every thread JOBS starts | the job system destructor, `CancelAndDrainBackground` | `Zenith2/Docs/Design/JOBS/Algorithms.md → "Shutdown sequence"` |
| the `Zenith_JobsError` enumeration in the shared `Zenith_JobsErrors.h` | ZenithJobs | § "Error codes" |

### What JOBS does not own

| Not owned | Owner | Why |
|---|---|---|
| `Zenith_FrameGraphBuilder`, `Zenith_FrameSchedule`, compilation, the schedule cache, recompile triggers, translation, cost re-prioritisation, `jobs.recompile`, `jobs.dump`; the frame executor (`Zenith_FrameExecutor`: instances, node dependency counters, the node trampoline and node zones, instance sequencing, the iteration barrier, cancellation of an instance, timings, the frame timeline); `Zenith_FixedOrderReduce` | GRAPH (inside ZenithJobs) | `Zenith2/Docs/Design/GRAPH.md → "The JOBS boundary"`; § "Boundary with GRAPH" |
| `Zenith_Graph` hazard derivation, ordering, priorities, lifetimes, the JSON dump schema | GRAPH (`ZenithGraph`) | `Zenith2/Docs/Architecture/ZenithGraph.md → "Role of the graph core"` |
| systems, `Zenith_SystemContext`, phases, sync-point playback, disabling failed systems | ECS, GAME (phase contract Z2-D-080) | ZenithJobs knows no ECS concept ([Z2-N-ARCH-381]) |
| the frame loop, prologue steps, the engine stages and their bodies | MOD (`ZenithRuntime`) | `Zenith2/Docs/Architecture/CompositionRoot.md → "The frame loop"` |
| `Flux_RenderWorld`, extraction contents, applying the interpolation alpha to transforms | RPROD, SCENE | `Zenith2/Docs/Architecture/CPUFrameGraph.md → "Render extraction and pipelining"` |
| `Zenith_Multithreading`, `Zenith_Mutex`, `Zenith_Semaphore` and `Zenith_SemaphoreNoProfiling`, the Debug thread state, `Zenith_OperationHandle` and `Zenith_OperationTable`, the lock-order checker, arenas, the monotonic clock, logging | CORE | § "Zenith_Multithreading is a Core header" |
| per-worker Luau VMs | SCRIPT | JOBS supplies the thread contract they rely on (§ "Per-worker Luau VM affinity contract") |
| the audio device thread, I/O threads, the profiler flush thread | AUDIO, PLAT, DEVT | each owner joins its own threads (`Zenith2/Docs/Contracts.md → "Threading and concurrency contract"`, table "Shutdown ownership order") |
| per-world pause and world lifecycle | GAME | the clock is process-wide; a world's own pause is a world-lifecycle concern |

Excluded outright by Z2-D-006: fibers, coroutine stacks, and any API that lets a job wait on another job.

### Inventory items covered

JOBS covers `Z2-I-JOBS-01` (work-stealing scheduler on `Zenith_Multithreading`), `-02` (driven by the GRAPH schedule), `-03` (dynamic parallel-for sub-jobs), `-04` (continuations), `-05` (no fibers), `-06` (no blocking waits), `-07` (cancellation), `-08` (thread affinity), `-09` (contention diagnostics), `-10` (fixed simulation steps), `-11` (interpolation), `-12` (pause), `-13` (time scaling), `-14` (bounded catch-up) and `-15` (orderly shutdown), all required in Stage 2. The mapping to requirements is `Zenith2/Docs/Requirements/JOBS.md → "Coverage of inventory items"`.

`Z2-I-JOBS-02` ("scheduler driven by the GRAPH schedule") is JOBS's side of the executor seam: GRAPH's executor turns the compiled schedule into ready nodes and launches them through `Zenith_JobsPushNodes`, and JOBS runs every launch on a thread of its affinity, in the take order GRAPH's priorities give, and reports its completion; GRAPH's `Z2-I-GRAPH-09` … `-12` cover the executor itself. This split follows `Zenith2/Docs/Architecture/CPUFrameGraph.md → "Settled rules and ownership"` and `Zenith2/Docs/Design/GRAPH.md → "The JOBS boundary"`; both documents give the simulation clock to JOBS (`Z2-I-JOBS-10` … `-14`).

## Dependencies

### Libraries

`ZenithJobs` is a layer-2 engine library declaring `DEPENDS ZenithCore ZenithGraph ZenithProfiling` (`Zenith2/Docs/Architecture.md → "Library dependency graph"`; `Zenith2/Docs/Architecture/BuildSystem.md → "The zenith_add_library helper"`). No third-party library is linked. What each edge is for:

| Dependency | Used for | Headings relied on |
|---|---|---|
| ZenithCore | types, results, asserts, `Zenith_Heap`, `Zenith_Owned`, `Zenith_Arena`, `Zenith_Span`, `Zenith_Multithreading` (`Zenith_Thread`, `Zenith_ThreadDesc`, `Zenith_GetHardwareConcurrency`, `Zenith_MarkThreadAsJobWorker`), `Zenith_Semaphore` and `Zenith_SemaphoreNoProfiling`, `Zenith_OperationTable` and its tickets, the profiler hooks, `Zenith_GetMonotonicTimeNs`, `ZENITH_LOG_*` (`Zenith2/Docs/Design/CORE/Threading.md → "Threads"`, `Zenith2/Docs/Design/CORE/Threading.md → "Zenith_OperationHandle implementation"`) | `Zenith2/Docs/Contracts.md → "Ownership and allocation"`, `Zenith2/Docs/Contracts/Memory.md → "Zenith_Arena"`, `Zenith2/Docs/Contracts.md → "Handle kinds"`, `Zenith2/Docs/Contracts.md → "No mutable global state"`, `Zenith2/Docs/Contracts.md → "Threading and concurrency contract"` |
| ZenithGraph | nothing in JOBS's own files; the edge exists for GRAPH's half of the same library (the frame-graph builder and executor), and no JOBS file includes a ZenithGraph header | `Zenith2/Docs/Design/GRAPH.md → "Library dependencies"` |
| ZenithProfiling | the optional `Zenith_Profiler*` of the desc, for per-thread lanes of the on-screen timeline | § "On-screen profiler rows" |

### Zenith_Multithreading is a Core header

`Zenith_Multithreading.h`, `Zenith_Mutex.h`, `Zenith_ConditionVariable.h` and `Zenith_Semaphore.h` are ZenithCore headers, with their Win32 and pthreads implementations under `ZenithCore/Internal/Windows/`, `Internal/Android/` and `Internal/Linux/`, as `Zenith2/Docs/Contracts.md → "Threading and concurrency contract"` ([Z2-N-CONTRACT-311]) fixes. The dependency graph requires it: ZenithCore itself takes locks (the OOM handler, log sinks, the callback registry, the leak table), and ZenithPlatform (layer 2, a peer of ZenithJobs that does not depend on it) starts I/O threads, so the primitives cannot live above layer 0. JOBS's own platform files (§ "Platform files") hold only what the scheduler alone needs: the processor count, CPU-mask application, the CPU-relax instruction and the floating-point environment.

### Boundary with GRAPH

GRAPH builds and sequences; JOBS runs. The rule, in `Zenith2/Docs/Design/GRAPH.md → "The JOBS boundary"`: JOBS decides how a job reaches a thread; GRAPH decides when a node is ready and what happens when it completes. The two meet at one internal header JOBS owns, `Internal/Jobs/Zenith_JobNodeLaunch.h` (`Zenith2/Docs/Design/JOBS/Algorithms.md → "Node launch seam"`), whose declarations are the ones GRAPH assumed in `Zenith2/Docs/Design/GRAPH.md → "The executor to job-system seam"`, plus five JOBS additions (`Zenith_JobsBeginIteration`, `Zenith_JobsEndIteration`, `Zenith_JobsReserveAffinityCapacity`, `Zenith_JobsReserveInjectionCapacity`, `Zenith_JobsTryInjectNode`) recorded for GRAPH in the cross-area notes (J3, J16).

| Question | GRAPH answers | JOBS answers |
|---|---|---|
| which nodes exist, their edges, dependency counts and priorities | builder, compile, schedule | — |
| when a node is ready; what its completion means for successors, instances and the iteration | the executor | — |
| on which thread a ready node runs, and in which order ready nodes are taken | — | `Zenith_JobsPushNodes`: routing by affinity, take order = span order |
| when a node is finished (body, sub-jobs and continuations) | — | the completion callback, called exactly once |
| sub-jobs and continuations inside a node | — | `Zenith_JobContext` |
| cancellation of an iteration | `RequestCancel` sets the flag each launch names | skips unstarted bodies and leaves; runs started continuations with `IsCancelled()` true |
| node zones, timings, the frame timeline | the trampoline and the executor | sub-job, continuation and background zones; per-thread lanes |
| affinity- and injection-queue capacity | `GetAffinityNodeCount` and the count of ANY nodes with a render-submit predecessor | `Zenith_JobsReserveAffinityCapacity` and `Zenith_JobsReserveInjectionCapacity` size and grow the queues |

A GRAPH ticket edits no JOBS file and a JOBS ticket edits no GRAPH file; the shared `Zenith_JobsErrors.h` is edited declaration-wise by each owner.

### Headings JOBS relies on

| Heading | For |
|---|---|
| `Zenith2/Docs/Architecture/CPUFrameGraph.md → "Frame structure"` | the three subgraphs, their order and concurrency, and the frame prologue (which is not an executor subgraph) |
| `Zenith2/Docs/Architecture/CPUFrameGraph.md → "Job system"` | the public types, threads, queues and worker loop this design elaborates |
| `Zenith2/Docs/Architecture/CPUFrameGraph.md → "Dynamic sub-jobs and continuations"` | `ParallelFor` and `Continue` semantics |
| `Zenith2/Docs/Architecture/CPUFrameGraph.md → "Execution"` | the simulation clock (the executor there is GRAPH's) |
| `Zenith2/Docs/Design/GRAPH.md → "The executor to job-system seam"` | what GRAPH's executor calls and relies on |
| `Zenith2/Docs/Architecture/CPUFrameGraph.md → "Cancellation and shutdown"`, `Zenith2/Docs/Architecture/CPUFrameGraph.md → "Profiling"`, `Zenith2/Docs/Architecture/CPUFrameGraph.md → "Required tests"` | cancellation, shutdown, zones, counters and the required test names |
| `Zenith2/Docs/Architecture/FramePhases.md → "Sync points and the tick boundary"` | what sync-point and tick-boundary nodes do (their bodies are ECS's; JOBS runs them) |
| `Zenith2/Docs/Architecture/ECSModel.md → "Deterministic reductions"`, `Zenith2/Docs/Architecture.md → "Determinism guarantee and mechanisms"` | the fixed-order tree and the determinism mechanisms JOBS must not break |
| `Zenith2/Docs/Architecture/CompositionRoot.md → "Startup sequence"`, `Zenith2/Docs/Architecture/CompositionRoot.md → "The frame loop"` | when the job system and the executor are created and called |
| `Zenith2/Docs/Scripting.md → "VM model and threading"` | the per-worker VMs that rely on § "Per-worker Luau VM affinity contract" |
| `Zenith2/Docs/Budgets.md → "Per-system rows"` | the JOBS rows |
| `Zenith2/Docs/Architecture/BuildSystem.md → "The ZENITH_SUPPORTED matrix"` | `ZENITH_TOOLS_HOST` is a preset matrix value, so the tools-host gate of [Z2-N-ARCH-443] is a run-time desc field (`Zenith2/Docs/Design/JOBS/Algorithms.md → "Tools-host background priority"`) |

### Needs recorded for other areas

`C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md` carries each item in full; in short: CORE — `Zenith_MarkThreadAsJobWorker` must mean "a job body is running" (as drafted, the flag fails the scheduler's own parks), and `Zenith_OperationHandle::GetSlotID`; GRAPH — the five seam additions and the injection count `SetSchedule` passes and the reserve and iteration-window rules (J16, J20), the two citations of renamed PublicAPI headings (J17), invocation-ID bits 62 and 63; DEVT — `Zenith_Profiler` lane registration and a zone hook that copies dynamic names; SCRIPT — the floating-point table and the worker-VM slot count; ECS — sub-jobs use their own context's arena; MOD — the `memory` CTest label and registering `GetMemoryConsumer()` with the memory governor (J23); DEVT — the contention rows' exactness (J22); ARCH — Z2-B-JOBS-004 in the `MEMORY_SUBSYSTEM_OTHER` budget (J23, X59); PLAT, tools and STREAM — the 2 ms and no-spin obligations of any injecting producer (J24). The Budgets items (the procedure of Z2-B-JOBS-001, the breach code of Z2-B-JOBS-002, the resident-memory and queue-capacity rows) have landed as Z2-B-JOBS-001 to -005. The tools-host priority of ruling X350 (a) (JOBS design patch 2026-10-16) adds: WORLD and AI — the names, the refusal, the cap accessor and the in-flight bound their tool operations follow (rows J25, J26); MOD and ASSET — the editor's tool-mode step 9 and the cook's composition root set `m_bAcceptToolBackground` (rows J27, J28); ARCH — the enum value in `Zenith2/Docs/Architecture/CPUFrameGraph.md`'s declaration block and [Z2-N-ARCH-443]'s host wording (cross-doc X378); Budgets — [Z2-N-BUDGET-230]'s list of what `jobs.memory_bytes` counts (cross-doc X379).

### Upward interfaces JOBS declares

ZenithJobs calls up into higher code only through types it declares: the job function pointers (`Zenith_PfnJob`, `Zenith_PfnParallelForBody`) that node bodies and callers pass down, and the optional thread lifecycle callback `Zenith_PfnJobThreadLifecycle` that the composition root passes in the desc (a crash reporter registers its threads with it) ([Z2-N-CONTRACT-175]). It stores no pointer into a higher library otherwise.

## Library and file layout

### ZenithJobs target

```cmake
# Zenith2/Engine/ZenithJobs/CMakeLists.txt — the whole file
zenith_add_library(ZenithJobs DEPENDS ZenithCore ZenithGraph ZenithProfiling)
```

`KIND ENGINE` (the default), `SUPPORTED ZENITH_ALL` (the default: `win`, `win-asan`, `linux-tsan`, `android-arm64`, `android-x86_64`; `Zenith2/Docs/Architecture/BuildSystem.md → "The ZENITH_SUPPORTED matrix"`), `EXCEPTIONS OFF`. The same directory holds GRAPH's frame-graph builder sources; both areas' files are one target.

### Public headers

JOBS-owned public headers, all at `Zenith2/Engine/ZenithJobs/` (specified in `Zenith2/Docs/Design/JOBS/PublicAPI.md → "Header list"`): `Zenith_JobsErrors.h` (shared with GRAPH), `Zenith_JobTypes.h`, `Zenith_JobContext.h`, `Zenith_JobSystem.h`, `Zenith_SimulationClock.h`, `Zenith_FloatEnvironment.h`. GRAPH-owned public headers in the same library: `Zenith_FrameGraph.h`, `Zenith_FrameSchedule.h`, `Zenith_FrameExecutor.h`, `Zenith_FixedOrderReduce.h`. Every public header is in the Z2 categories' `reviewPaths`, so a change to any of them needs an API review record ([Z2-N-CONTRACT-492]).

### Internal files

The complete list with contents is `Zenith2/Docs/Design/JOBS/Algorithms.md → "Internal types and files"`: under `Internal/Jobs/` — `Zenith_JobSystemState.h`, `Zenith_JobSystem.cpp`, `Zenith_JobThreadState.h`, `Zenith_JobThreadLoop.h`, `Zenith_JobThreadLoop.cpp`, `Zenith_JobRecords.h`, `Zenith_WorkStealingDeque.h`, `Zenith_WorkStealingDeque.cpp`, `Zenith_BoundedMpmcQueue.h`, `Zenith_JobContext.cpp`, `Zenith_JobNodeLaunch.h`, `Zenith_JobNodeLaunch.cpp`, `Zenith_JobsNames.h`, `Zenith_JobsNames.cpp`, `Zenith_FloatEnvironment.cpp`; under `Internal/Clock/` — `Zenith_SimulationClock.cpp`. GRAPH's files live under `Internal/FrameGraph/`.

### Platform files

| File | Contents |
|---|---|
| `Zenith2/Engine/ZenithJobs/Internal/Windows/Zenith_FloatEnvironment_Windows.cpp` | MXCSR apply and read |
| `Zenith2/Engine/ZenithJobs/Internal/Linux/Zenith_FloatEnvironment_Linux.cpp` | MXCSR apply and read |
| `Zenith2/Engine/ZenithJobs/Internal/Android/Zenith_FloatEnvironment_Android.cpp` | FPCR (`__aarch64__`) or MXCSR (`__x86_64__`) apply and read |

JOBS includes no platform SDK header; the intrinsic header `<xmmintrin.h>` appears in these files alone (rule F18; `Zenith2/Docs/Contracts.md → "Tests and code hygiene gates"`). Threads, their names and affinity masks, semaphores, the processor count and the spin hint `Zenith_CpuRelax()` are CORE's platform code (`Zenith2/Docs/Design/CORE/Threading.md → "Threads"`); JOBS has no platform file other than these three.

### Test files

`Zenith2/Engine/ZenithJobs/Tests/`: `Zenith_JobSystem_Tests.cpp`, `Zenith_JobContext_Tests.cpp`, `Zenith_WorkStealingDeque_Tests.cpp`, `Zenith_BoundedMpmcQueue_Tests.cpp`, `Zenith_JobNodeLaunch_Tests.cpp`, `Zenith_SimulationClock_Tests.cpp`, `Zenith_FloatEnvironment_Tests.cpp`, `Zenith_JobsProfiling_Tests.cpp`, `Zenith_JobsStress_Tests.cpp`, `Zenith_JobSystem_DefectTests.cpp`, `Examples/Zenith_JobContext_Example.cpp`, `Examples/Zenith_JobSystem_Example.cpp`, all built into `ZenithJobs_Tests` ([Z2-N-CONTRACT-405]). GRAPH's `Tests/GraphConformance/` adapter and `Tests/Zenith_Frame*_Tests.cpp` live in the same `Tests/` directory.

### Benchmark and research files

`Zenith2/Benchmarks/Budgets/JOBS/` (`Zenith_Bench_BudgetsJOBS`, the Stage 1 harness) and `Zenith2/Benchmarks/Jobs/` (`Zenith_Bench_Jobs`), each with a `--smoke` CTest run; raw data under `Zenith2/Docs/Research/Budgets/` and `Zenith2/Docs/Research/Jobs/` (`Zenith2/Docs/Design/JOBS/TestCatalogue.md → "Benchmarks"`).

## Public API

### API summary

The full declarations, with contract blocks, are `Zenith2/Docs/Design/JOBS/PublicAPI.md`. In outline:

| Type or function | Header | Thread affinity | Purpose |
|---|---|---|---|
| `Zenith_JobsError`, `Zenith_ToString` | `Zenith_JobsErrors.h` | IMMUTABLE | the error enumeration (§ "Error codes") |
| `Zenith_JobAffinity`, `Zenith_JobPriority`, `Zenith_PfnJob`, `Zenith_PfnParallelForBody`, `Zenith_ParseCpuMask`, the limits | `Zenith_JobTypes.h` | IMMUTABLE | the vocabulary |
| `Zenith_JobContext`, `Zenith_CancellationToken`, `Zenith_ParallelForRange`, `Zenith_ParallelForLeafCount`, `Zenith_ParallelForLeaf` | `Zenith_JobContext.h` | OWNER_THREAD (context), ANY_THREAD (token, pure functions) | sub-jobs, continuations, cancellation, arenas, captures, identity |
| `Zenith_JobSystemDesc`, `Zenith_JobSystem`, `Zenith_PfnJobThreadLifecycle` | `Zenith_JobSystem.h` | ANY_THREAD_EXCEPT(Create, RunRoot, CancelAndDrainBackground, destructor) | threads, background work, root iterations, shutdown |
| `Zenith_SimulationClockDesc`, `Zenith_SimulationClockMode`, `Zenith_SimulationClock`, `Zenith_ValidateSimulationClockDesc` | `Zenith_SimulationClock.h` | MAIN_THREAD | fixed ticks, alpha, pause, scale, catch-up |
| `Zenith_FloatEnvironmentState` and its four functions | `Zenith_FloatEnvironment.h` | per calling thread | the fixed FP state |
| `Zenith_JobNodeLaunch` and the seam functions | `Internal/Jobs/Zenith_JobNodeLaunch.h` (internal; GRAPH's executor alone) | as each function states | launching frame-graph nodes |

The API has no function that waits for a job or an operation; waiting is an edge or a continuation (Z2-D-006). Operation handles are polled (`IsComplete`) and their results published at tick boundaries by their owners ([Z2-N-CONTRACT-093]). Deterministic folding of per-leaf results uses GRAPH's `Zenith_FixedOrderReduce` (`Zenith2/Docs/Design/GRAPH.md → "Zenith_FixedOrderReduce.h"`).

### Per-worker Luau VM affinity contract

SCRIPT runs chunk systems on one Luau VM per (worker thread, world) and behaviours on each world's main VM on the main thread (Z2-D-018; `Zenith2/Docs/Scripting.md → "VM model and threading"`). JOBS guarantees the thread properties that design relies on:

1. **Slot indices are stable.** `Zenith_JobContext::GetWorkerIndex()` returns 0 on the main thread, 1..N on workers and N+1 on the render-submit thread; a thread's index never changes while the job system lives, and no two threads share one.
2. **Frame work stays on frame threads.** Every `JOB_AFFINITY_ANY` job — ANY nodes, their continuations, and every `ParallelFor` leaf of any node — runs on a thread with index in [0, N]; only render-submit work reports N+1. A per-slot table of `Zenith_JobSystem::GetFrameThreadCount()` (N+1) entries indexed by `GetWorkerIndex()` therefore covers every thread that can run a chunk-system leaf. The main thread helping inside an iteration can run chunk-system leaves, so slot 0 of SCRIPT's worker-VM table belongs to the main thread and is distinct from the world's main VM; SCRIPT's VM index "1 + worker index" gives it VM index 1.
3. **No migration; nesting only on the inline path.** A job body runs start to finish on one thread with no fibers. One job runs inside another on the same thread exactly when a `ParallelFor` root push is rejected: the call then runs its leaves itself, inside the calling body, whether that body is a stage or a range body that itself calls `ParallelFor` (`Zenith2/Docs/Design/JOBS/Algorithms.md → "ParallelFor splitting and inline execution"`). A leaf reached by splitting or stealing never runs inside another body. The contract SCRIPT relies on is therefore not "nesting cannot happen" but two properties that hold on every path: the leaf partition is a pure function of (count, grain) (§ "Determinism obligations"), so which thread runs a leaf and whether it ran inline never changes what the leaf computes; and a nested leaf runs on the same slot as its caller, so it reaches the same per-slot VM. SCRIPT therefore never calls `ParallelFor` from inside a Luau invocation (the nested leaf would re-enter that slot's VM while it is mid-call) and never from a chunk-system leaf; it calls it only from a node body outside any Luau invocation.
4. **Main-thread work stays on the main thread.** `JOB_AFFINITY_MAIN` nodes and their continuations run on index 0 alone; behaviours and every main-VM operation live there.
5. **Quiescence.** Between two iterations (from `Zenith_JobsEndIteration`, or the end of `RunRoot`, to the next `Zenith_JobsBeginIteration` or `RunRoot`) no frame job runs on any thread, including an injected one: `Zenith_JobsTryInjectNode` refuses outside an iteration with `JOBS_ERROR_NO_ITERATION`, and the end of every iteration drains the injected nodes it accepted (`Zenith2/Docs/Design/JOBS/Algorithms.md → "Push order and routing"`), and background jobs never run chunk systems or touch a VM, so the main thread may create or destroy a worker-VM slot there without a lock ([Z2-N-SCRIPT-075]).
6. **Identical floating-point state on every slot**, per § "Determinism obligations".

### Job memory model

| Data | Owner | Allowed memory | Lifetime rule |
|---|---|---|---|
| a node's `m_pData` | the registrant | registrant memory that outlives the schedule | `Zenith2/Docs/Architecture/CPUFrameGraph.md → "Frame graph builder"` |
| `ParallelFor` and `Continue` `pData` | the calling job | the node arena (`GetNodeArena`, `AllocateCapture`) or registrant memory; never the caller's stack | valid until the node instance completes |
| captures | the calling job | the node arena, through `AllocateCapture`, trivially copyable and destructible (`static_assert`) | as above ([Z2-N-CONTRACT-232]) |
| scratch | the executing job | `GetNodeArena`, `GetWorkerArena` of its own context | frame jobs: until the node instance completes; background: until the slice returns |
| background `pData` | the submitter | submitter memory | until the handle reports complete (and until the job returned, if the handle is destroyed early) |
| background payload | the operation | copied inline, at most 64 bytes | each slice gets its own copy's address |
| internal records | the pushing thread | its record arena; for a node injected by a foreign thread, one block on the job system's heap | until the end of the iteration (an injected node's block until its completion callback, inside the iteration that accepted it) |

Nothing a job allocates through JOBS touches the general heap on a hot path: records, segments, continuations and scratch come from per-thread arenas that are reset, not freed, at iteration boundaries. The general heap is used by `Create`, by `Zenith_JobsReserveAffinityCapacity` and `Zenith_JobsReserveInjectionCapacity` when a queue grows, by `Zenith_JobsTryInjectNode` (a foreign thread has no arena), and by arena block growth up to each arena's cap.

### Configuration keys

| Key | Type, range | Default | Read by | Maps to |
|---|---|---|---|---|
| `jobs.workerCount` | u32, 0 or 1..62 | 0 | composition root, step 9 | `Zenith_JobSystemDesc::m_uWorkerCount` |
| `jobs.spinIterations` | u32, 0..1,000,000 | 2000 | step 9 | `m_uSpinIterations` |
| `jobs.maxConcurrentBackground` | u32, 0..N | 0 | step 9 | `m_uMaxConcurrentBackground` |
| `jobs.maxConcurrentToolBackground` | u32, 0..max(1, ⌊N / 2⌋) | 0 (the default cap `min(max(1, ⌊N / 2⌋), background cap)`) | the editor's and the cook's composition roots alone, which also set `m_bAcceptToolBackground` in code; a Player never reads it | `m_uMaxConcurrentToolBackground` (`Zenith2/Docs/Design/JOBS/Algorithms.md → "Tools-host background priority"`) |
| `jobs.workerCpuMask`, `jobs.renderSubmitCpuMask` | lower-case hex string, 1–16 digits | empty (0) | step 9, through `Zenith_ParseCpuMask` | `m_ulWorkerCpuMask`, `m_ulRenderSubmitCpuMask` |
| `runtime.headless` | bool | false | step 9 | `m_bStartRenderSubmitThread = !headless` |
| `sim.tickHz` | u32, 1..1000 | 60 | composition root | `Zenith_SimulationClockDesc::m_uTickHz` (recorded in the replay header) |
| `sim.maxCatchUpTicks` | u32, 1..64 | the Z2-B-JOBS-002 value | composition root | `m_uMaxCatchUpTicks`; 0 or out of range stops startup with exit code 2 ([Z2-N-ARCH-436]) |
| `sim.maxFrameDeltaUs` | u32, 1,000..1,000,000 | 250000 | composition root | `m_uMaxFrameDeltaUs` |
| `sim.ticksPerFrame` | u32, 0..64 | 0 | composition root | 0 = real-time mode; N ≥ 1 = `SIM_CLOCK_MODE_EXTERNAL_TICKS` with N ticks per frame (headless smoke and determinism runs) |
| `sim.paused` | bool | false | frame prologue | `SetPaused` |
| `sim.timeScale` | f32, 0..8 | 1.0 | frame prologue | `SetTimeScale` |

`sim.tickHz`, `sim.maxCatchUpTicks` and `sim.ticksPerFrame` are registered as simulation-affecting configuration, so they are written into the replay header (`Zenith2/Docs/Architecture.md → "Multiplayer foundations"`). During `--replay` the composition root never sets `sim.paused` and drives the clock in external-tick mode from the log, so `AdvanceExternal`'s paused case (it returns 0 and advances nothing) is never reached by a replay.

## Data structures and formats

### In-memory layouts

| Structure | Size and alignment | Where specified |
|---|---|---|
| `Zenith_JobThreadState` | one per slot, 128-byte aligned, N + 2 entries | `Zenith2/Docs/Design/JOBS/Algorithms.md → "Thread state and the no-global rule"` |
| `Zenith_JobRecord` | 64 bytes (56 used, including the handoff link), 16-byte aligned | `Zenith2/Docs/Design/JOBS/Algorithms.md → "Record types"` |
| `Zenith_BackgroundRecord` | 128 bytes; 192-byte queue cell with its sequence word | as above |
| `Zenith_JobNodeState`, `Zenith_JobSegment`, `Zenith_JobContinuation` | 64-byte aligned each | `Zenith2/Docs/Design/JOBS/Algorithms.md → "Stages segments and continuations"` |
| deque | two 64-bit indices on separate cache lines + `capacity` pointer slots (8 bytes each) | `Zenith2/Docs/Design/JOBS/Algorithms.md → "Work-stealing deque"` |
| MPMC queue | two positions on separate cache lines + `capacity` cells, each a multiple of 64 bytes | `Zenith2/Docs/Design/JOBS/Algorithms.md → "Bounded MPMC queue"` |
| `Zenith_SimulationClock` | 64 bytes, no allocation | `Zenith2/Docs/Design/JOBS/PublicAPI.md → "Zenith_SimulationClock header"` |

Default storage at N = 6 (the S24 with the automatic count): 7 deques × 4,096 × 8 B = 224 KiB; injection 16,384 × 64 B = 1 MiB; two background queues 8,192 × 192 B each = 3 MiB; affinity queues ≤ 64 KiB; the background operation table (2 × 8,192 + 6 + 2 = 16,392 slots × 64 B = 1,049,088 B) and its ticket array (16,392 `Zenith_OperationTicket` entries); thread states ≈ 8 KiB; arenas one 64 KiB block each initially (3 per slot, 8 slots: 1.5 MiB), each capped at 16 MiB. A tool host that accepts `JOB_PRIORITY_BACKGROUND_TOOL` adds, at the default `m_uToolBackgroundCapacity` of 1,024, the tool queue (1,024 × 192 B = 196,608 B), 1,024 more operation-table slots (65,536 B) and 1,024 more tickets; a Player has none of them, so the X350 tools-host priority changes no WS-FRAME measurement (`Zenith2/Docs/Design/JOBS/Algorithms.md → "Tools-host background priority"`). The total is reported under `jobs.memory_bytes`, reported through `GetMemoryConsumer()` into `MEMORY_SUBSYSTEM_OTHER`, whose budget includes Z2-B-JOBS-004 (`Zenith2/Docs/Architecture.md → "Memory governor"`, table "Budgets"; the row joins that table through cross-doc X59, pending ARCH); the row itself is measured by MP-SUBMEM (§ "Rows JOBS owns"). Grown queues are rounded up to the next power of two (the MPMC queue indexes with a mask).

### File formats

JOBS reads and writes no file. Its only persistent traces are the configuration keys above (owned by PLAT's configuration layering), the counters and zones consumed by DEVT, and the dumps GRAPH's `jobs.dump` writes (`Zenith2/Docs/Architecture/ZenithGraph.md → "JSON dump"`), which include the node timings GRAPH measures.

## Algorithms and state machines

Every algorithm and state machine is specified step by step, with memory orders, in `Zenith2/Docs/Design/JOBS/Algorithms.md`:

| Algorithm | Heading there | Complexity |
|---|---|---|
| Chase–Lev deque push, pop, steal; ABA handling | "Work-stealing deque" | O(1); steal lock-free |
| Vyukov MPMC queue | "Bounded MPMC queue" | O(1) lock-free |
| worker loop, main-thread helping, render-submit loop | "Worker loop", "Main thread helping", "Render-submit thread" | O(N) per steal sweep |
| parking and wake-up (Dekker handshake) | "Parking and wake-up" | O(1) |
| job invocation, job-body mark, FP assert | "Job invocation" | O(1) |
| stages, segments and continuations | "Stages segments and continuations" | O(1) per spawn, registration and completion |
| `ParallelFor` lazy binary splitting, inline execution, leaf partition | "ParallelFor splitting and inline execution" | O(L) calls, O(log L) queued |
| node launches: routing, take order, completion, main-thread helping, iteration begin and end, invocation IDs | "Node launch seam" | O(R) per push of R launches |
| background submission, slices, outcomes | "Background jobs and operations" | O(1) per submission and slice |
| the tools-host priority: admission, cap, pop order, drain ([Z2-N-ARCH-443]) | "Tools-host background priority" | O(1) per submission and worker pass |
| cancellation of iterations and operations | "Cancellation semantics" | — |
| the simulation clock | "Simulation clock arithmetic" | O(1) |
| create steps with unwinding; shutdown | "Create and failure unwinding", "Shutdown sequence" | O(N) |

State machines (worker thread, background operation, launched node) are tables in `Zenith2/Docs/Design/JOBS/Algorithms.md → "State machines"`.

## Threading lifetime and error contract

### Thread affinity per type

| Type | Keyword | Notes |
|---|---|---|
| `Zenith_JobSystem` | `ANY_THREAD_EXCEPT(Create, RunRoot, CancelAndDrainBackground, ~Zenith_JobSystem)` | the four run on the main thread outside job bodies and outside an iteration |
| the seam functions | `Zenith_JobsPushNodes`: a JOBS thread through its context, or the main thread inside an iteration with a null pusher; `Zenith_JobsTryInjectNode`: a foreign producer — a thread with no job context, such as an I/O, test or tool thread (`Zenith2/Docs/Architecture/CPUFrameGraph.md → "Queues"`), never a JOBS thread or the main thread; an injected launch is frame work of the iteration that accepted it, its body and each continuation obeying the frame-job rules and sized like a background slice (at most 2 ms each, [Z2-N-ARCH-393]), and the iteration end waits for it; `Zenith_JobsWakeMain`: any thread; `Zenith_JobsAssertFloatEnvironment`: the calling thread; the other five (`Zenith_JobsRunMainUntil`, `Zenith_JobsBeginIteration`, `Zenith_JobsEndIteration`, the two `Zenith_JobsReserve…Capacity`): `MAIN_THREAD` | GRAPH's executor calls them; `Zenith_JobsTryInjectNode` is called by foreign producers inside an iteration |
| `Zenith_JobContext` | `OWNER_THREAD` | valid only in its invocation |
| `Zenith_CancellationToken` | `ANY_THREAD` | until its job returns |
| `Zenith_SimulationClock` | `MAIN_THREAD` | values handed to nodes by value |
| value types and enums | `IMMUTABLE` | — |

### Frame-graph nodes and declared access

ZenithJobs's JOBS half contributes no frame-graph node: it runs the nodes GRAPH launches. The frame prologue step that calls JOBS (`Zenith_SimulationClock::Advance`, step 2) runs on the main thread with no job running (`Zenith2/Docs/Architecture/CPUFrameGraph.md → "Frame structure"`). JOBS's own bookkeeping (segments, gates, records) is private state no node declares.

### Lock-order entries

JOBS adds no row to `Zenith2/Docs/Contracts.md → "Threading and concurrency contract"` (table "Lock ordering table") and acquires no `Zenith_Mutex` of its own:

| Path | Locks reached | Level | Short-hold | Thread |
|---|---|---|---|---|
| every frame path: push, pop, steal, park, wake, invocation, segment and gate updates, completion callbacks | none | — | — | any |
| `SubmitBackground*`, background completion | CORE's `uLOCK_LEVEL_OPERATION_TABLE` inside `Zenith_OperationTable` | 380 | yes | any |
| `Create`, `Zenith_JobsReserveAffinityCapacity`, `Zenith_JobsReserveInjectionCapacity`, `Zenith_JobsTryInjectNode`, arena growth | CORE's `uLOCK_LEVEL_HEAP_OOM` only on the OOM path | 900 | no | main, the injecting thread or the growing thread |
| the messages of § "Log category" | CORE's `uLOCK_LEVEL_LOG_SINKS` | 920 | yes | any |

`uLOCK_LEVEL_JOBS_SCHEDULE` (300, owner ZenithJobs) is acquired by GRAPH's executor code in the same library, never by a JOBS file (`Zenith2/Docs/Design/GRAPH.md → "Threading lifetime and error contract"`). Because a background submission may come from a job body, the operation-table lock is short-hold, as [Z2-N-CONTRACT-315] requires.

### Cancellation and shutdown summary

Cancellation completes as `Zenith2/Docs/Design/JOBS/Algorithms.md → "Cancellation semantics"` tabulates: a cancelled iteration still completes every counter; a cancelled operation releases on its own thread before it reports `OPERATION_STATUS_CANCELLED`; an operation that finished first reports success. Shutdown follows `Zenith2/Docs/Design/JOBS/Algorithms.md → "Shutdown sequence"`: drain the in-flight frame (Contracts step 2), `CancelAndDrainBackground` (step 4), then the destructor joins the render-submit thread and workers N..1 (step 7).

### Error codes

| Enumerator | Returned by | Meaning | Caller's response |
|---|---|---|---|
| `JOBS_ERROR_THREAD_CREATE` | `Zenith_JobSystem::Create` | a thread could not be started | composition root exits with code 4 (a required platform service could not be created) |
| `JOBS_ERROR_QUEUE_FULL` | `SubmitBackground`, `SubmitBackgroundCopy`; `Zenith_JobsTryInjectNode` | the background queue (or the operation table) is full; the foreign share (`capacity − m_uInjectionReserved`) of the injection queue is used | retry at a subsequent tick, or drop the optional work; a foreign injecting thread retries the push; nothing was taken in either case |
| `JOBS_ERROR_OUT_OF_MEMORY` | `Create`, internal `Create`s, and GRAPH's `Zenith_FrameExecutor::Create` (which uses this enumeration) | the system's own storage could not be allocated | composition root exits with its startup failure code |
| `JOBS_ERROR_INVALID_DESC` | `Create`, `Zenith_ValidateSimulationClockDesc`, `Zenith_ParseCpuMask`, submissions with invalid arguments (including `JOB_PRIORITY_BACKGROUND_TOOL` to a job system that does not accept it) | a value is outside its range | composition root exits with code 2 naming the key; a refused tools-host submission is a host defect the submitter reports (its operation does not start) |
| `JOBS_ERROR_SHUTTING_DOWN` | submissions after `CancelAndDrainBackground` began | shutdown is in progress | the submitter abandons the work |
| `JOBS_ERROR_NO_ITERATION` | `Zenith_JobsTryInjectNode` | no iteration (`RunFrame` or `RunRoot`) is active, so the node could not join one | nothing was taken; the producer keeps the launch in its own storage and retries at its own next wake-up (an I/O completion, its loop's next pass), never by spinning on the call: JOBS signals no window |

Programmer errors (a null function pointer, `Continue` from a range body, a job-body blocking wait, a nested `Zenith_JobsBeginIteration`, a render-submit push with no thread, `ParallelFor` from a render-submit job, a null-pusher `Zenith_JobsPushNodes` from any thread but the main thread inside an iteration, a destroyed job system with live operations) are `ZENITH_ASSERT` or `ZENITH_CHECK`, listed per member in `Zenith2/Docs/Design/JOBS/PublicAPI.md`. A full affinity queue, and a full injection queue pushed by the render-submit thread, are `ZENITH_FATAL(FATAL_CODE_QUEUE_FULL)` naming the queue: both capacities are derived from the compiled schedule, so reaching either is a scheduler defect ([Z2-N-ARCH-392]). Like every fatal diagnostic it is non-replayable: the tick it interrupts carries `REPLAY_TICK_FLAG_NON_REPLAYABLE`, and a replay stops before it ([Z2-N-ARCH-790]).

`Zenith_Result` accepts either an error enumeration or a trivially copyable struct of at most 32 bytes with an `m_eCode` enumeration ([Z2-N-CONTRACT-480], [Z2-N-CONTRACT-481]). JOBS chooses the enumeration form (`Zenith2/Docs/Design/JOBS/PublicAPI.md → "Zenith_JobsErrors header"`): every JOBS failure is fully described by its code, and the queue or step a context struct could carry is logged at the point of failure instead. `Zenith2/Docs/Architecture/CPUFrameGraph.md → "Frame graph builder"` declares the same enumeration; `JOBS_ERROR_NO_ITERATION` is appended there through cross-doc item X56.

### Luau errors versus asserts

ZenithJobs has no Luau binding and raises no Luau error. Scripts never reach JOBS directly: chunk-system invocations are run by SCRIPT inside the job bodies JOBS calls, and every script misuse becomes a controlled Luau error inside SCRIPT's protected entry, never a JOBS assert ([Z2-N-CONTRACT-473]). A Luau error never unwinds through a JOBS frame, because every entry into Luau is a protected call inside `ZenithScript/Internal` and job functions are compiled with exceptions disabled.

### Determinism obligations

JOBS is scheduling machinery; the determinism guarantee (Z2-D-019) holds only if nothing observable through it depends on the scheduling. The obligations:

- **Leaf partition.** `ParallelFor` body calls are a pure function of (count, grain) (`Zenith2/Docs/Design/JOBS/PublicAPI.md → "ParallelFor leaf partition"`); callers merge per-leaf results by leaf index or with GRAPH's `Zenith_FixedOrderReduce`, whose shape depends only on the element count, never in completion order ([Z2-N-ARCH-396]).
- **Grain.** Simulation code never derives a grain from the worker count.
- **Take order.** Routing, handoff, take order and steal order change only which thread runs what and when ([Z2-N-ARCH-411]).
- **Identity.** Frame invocation IDs come from the launch (GRAPH computes them from frame, instance and node) and are used for view validation alone; worker indices are never keyed or hashed by simulation code ([Z2-N-ARCH-884]).
- **Floating point.** Every job thread holds the same fixed FP environment, asserted at every job start ([Z2-N-CONTRACT-360], [Z2-N-CONTRACT-361]).
- **Time.** Simulated time is tick index × tick duration; replays drive the clock in external-tick mode from the log, so dropped real time never changes the tick sequence ([Z2-N-ARCH-414]).
- **Tests.** Every JOBS mechanism that could leak scheduling into results has a 1, 2, 8 and all-workers test (`Zenith2/Docs/Design/JOBS/TestCatalogue.md → "JobContext tests"`).

## Diagnostics and profiling

### Counters

Reported through `Zenith_DispatchCounter` (`Zenith2/Docs/Contracts.md → "No mutable global state"`) once per iteration by the main thread in `Zenith_JobsEndIteration` (and at the end of `RunRoot`), with values for that iteration unless marked cumulative. Names are constant strings from `Internal/Jobs/Zenith_JobsNames.cpp` (`<i>` is the decimal slot index, 0..N+1).

| Counter | Value |
|---|---|
| `jobs.steals` | successful steals |
| `jobs.steal_failures` | failed steal attempts on non-empty victims |
| `jobs.inline_runs` | jobs run by the pushing thread after a rejected push |
| `jobs.idle_ns.<i>` | nanoseconds slot i spent spinning or parked (may spill into the next iteration, see below) |
| `jobs.busy_ns.<i>` | nanoseconds slot i spent inside job bodies |
| `jobs.parked` | parks performed |
| `jobs.ready_high_water` | the largest number of launched, not yet started nodes |
| `jobs.overhead_ns` | Σ over threads of `max(0, iteration wall time − busy − idle)` (scheduler bookkeeping); approximate, because busy and idle may spill (below) |
| `jobs.deque.full`, `jobs.deque.dropped`, `jobs.deque.high_water` | summed full events, 0, the maximum over threads |
| `jobs.injection.*`, `jobs.main.*`, `jobs.render_submit.*`, `jobs.background_high.*`, `jobs.background_low.*` | `full`, `dropped` (0), `high_water` per queue |
| `jobs.background_tool.*` | `full`, `dropped` (0), `high_water` of the tools-host queue; published only by a job system that accepts `JOB_PRIORITY_BACKGROUND_TOOL`, so a Player's counter list is unchanged |
| `jobs.injection.accepted` | foreign launches `Zenith_JobsTryInjectNode` accepted this iteration (exact: the drain orders every increment before publication) |
| `jobs.injection.drain_ns` | main-thread nanoseconds `Zenith_JobsEndIteration` or the end of `RunRoot` spent waiting for accepted injected nodes after closing the window (exact) |
| `jobs.bg.in_flight` | incomplete background operations at iteration end |
| `jobs.bg.completed`, `jobs.bg.cancelled`, `jobs.bg.failed` | operations that completed this iteration, by outcome |
| `jobs.bg.slice_overruns` | HIGH and LOW slices longer than 2 ms; a `JOB_PRIORITY_BACKGROUND_TOOL` slice is never counted ([Z2-N-ARCH-443]) |
| `jobs.bg.tool_running` | workers inside tools-host slices when the publisher reads (a gauge; may be stale by slices that start or end during publication); tools-host job systems only |
| `jobs.memory_bytes` | the job system's resident storage (queues, deques, thread states, arena blocks) |
| `sim.ticks`, `sim.dropped_ticks`, `sim.alpha` | reported by `Zenith_SimulationClock::Advance`: T, the ticks dropped this frame, alpha × 10^6 |

Every per-thread counter is a `std::atomic<uint64>` written by its own thread alone with `relaxed` load and store, and every queue statistic is an atomic of its queue; the publisher reads them with `relaxed` loads (`Zenith2/Docs/Design/JOBS/Algorithms.md → "Counters and their memory model"`). The frame-path counters (steals, steal failures, inline runs, queue and deque statistics, the ready high water) are exact per iteration, because each increment precedes a completion decrement of the iteration's `acq_rel` chain. The counters written outside that chain — `jobs.idle_ns.<i>`, `jobs.busy_ns.<i>` and `jobs.parked` (written when a thread wakes or finishes a background slice) and every `jobs.bg.*` counter — may attribute an increment to the following iteration, never lose one. No counter access is a data race, so `linux-tsan` needs no suppression for them.

`frame.schedule_recompiles`, `frame.dispatch_ns` and the other `frame.*` counters are GRAPH's (`Zenith2/Docs/Design/GRAPH.md → "Diagnostics and profiling"`). The counter list is exactly the one `Test_JobNodeLaunch_CountersPublishedPerIteration` checks.

### Zones

Every `ParallelFor` leaf batch (one zone per range-runner call, spanning its leaves), continuation and background slice emits one zone through the installed zone hooks, and GRAPH's trampoline emits the node's own zone ([Z2-N-ARCH-427]):

| Job | Zone name | Site |
|---|---|---|
| node body (GRAPH's trampoline) | `<NodeName>` | the launch's `m_pxZone`, which lives as long as the schedule |
| `ParallelFor` batch | `<NodeName>/[<begin>,<end>)` in decimal | formatted into the thread's record arena by `Internal/Jobs/Zenith_JobsNames.cpp` (no `<cstdio>`) |
| continuation i | `<NodeName>/cont<i>` | formatted likewise |
| background slice | the submitter's `bg/<Name>` literal | a stack site with that static name |
| `RunRoot` | its `szName` | a stack site |

Formatting happens only when `Zenith_IsZoneHookInstalled()` is true, and the begin and end calls are `Zenith_DispatchZoneBegin` and `Zenith_DispatchZoneEnd` (an installed table always holds both), so an uninstrumented build pays nothing.

<!-- assumption: pending DEVT -->
A formatted name lives in the thread's record arena until the next iteration; JOBS assumes the ZenithProfiling zone-begin hook copies the name text into its own buffer at the call (it does not retain the pointer). Recorded in the cross-area notes.

### On-screen profiler rows

DEVT's overlay draws; GRAPH supplies the node timings and frame timeline, and JOBS the thread lanes and contention data. The CPU frame-graph timeline has one lane per thread slot, named `Z2 Main`, `Z2 Worker NN` and `Z2 RenderSubmit`, registered through `m_pxProfiler` at `Create`, filled from the zones above. The contention rows are computed from the counters alone ([Z2-N-ARCH-433]):

| Row | Formula | Exactness |
|---|---|---|
| Jobs: steals per frame | `jobs.steals` | exact |
| Jobs: steal failure ratio | `jobs.steal_failures / (jobs.steals + jobs.steal_failures)` | exact |
| Jobs: idle % per thread | `jobs.idle_ns.<i> / frame wall time` | may spill |
| Jobs: busy % per thread | `jobs.busy_ns.<i> / frame wall time` | may spill |
| Jobs: parks, inline runs | `jobs.parked`, `jobs.inline_runs` | parks may spill; inline runs exact |
| Jobs: queue high water | each `*.high_water`, against its capacity | exact |
| Jobs: background backlog | `jobs.bg.in_flight`, completed/cancelled/failed, overruns; in a tool host also `jobs.bg.tool_running` against `GetMaxConcurrentToolBackground()` | may spill |
| Jobs: injected work | `jobs.injection.accepted`, `jobs.injection.drain_ns` | exact |
| Jobs: scheduler overhead | `jobs.overhead_ns`, against Z2-B-JOBS-001 | approximate (derived from may-spill counters) |
| Sim: ticks, dropped, alpha | `sim.ticks`, `sim.dropped_ticks`, `sim.alpha / 10^6`, against Z2-B-JOBS-002 | exact |

"May spill" means an increment can land in the following iteration's value (§ "Counters"), so a row over several such counters satisfies no per-frame identity: idle % + busy % + overhead need not equal 100 % in any one frame, only over an interval. DEVT's row tests assert interval sums, never per-frame identities (cross-area note J22).

<!-- assumption: pending DEVT -->
`Zenith_Profiler` (ZenithProfiling) is assumed to offer `Zenith_ProfilerLaneID RegisterThreadLane(uint32 uLaneIndex, const char* szName)`, callable once per lane at start-up. Recorded in the cross-area notes.

### Log category

JOBS logs under the category `jobs.scheduler` (`ZENITH_LOG_CATEGORY(xLOG_CATEGORY_JOBS_SCHEDULER, "jobs.scheduler", LOG_LEVEL_TRACE)` in `Internal/Jobs/Zenith_JobsNames.h`; `Zenith2/Docs/Design/CORE/Diagnostics.md → "Structured logging"`) through ZenithCore's structured log: `jobs: background queue '<name>' full` (warning, once per iteration per queue), `jobs: thread <i> create failed: <platform error>` (error), `jobs: queue '<name>' grown to <n>` (info, once per growth of an affinity or the injection queue), `jobs: memory pressure <level>` (info, once per level change reported to the memory consumer). No log call happens on a job's hot path other than these rate-limited warnings.

### Debug views and dumps

JOBS adds no console command and no dump of its own. GRAPH's executor owns `jobs.recompile`, `jobs.dump <path>` and the frame dump with node timings ([Z2-N-ARCH-429]); the thread lanes JOBS registers give those timings their per-thread rows.

### Telemetry

Frame telemetry (`Zenith2/Docs/Architecture/CompositionRoot.md → "The frame loop"`, step 4) carries `jobs.overhead_ns`, `jobs.inline_runs`, `sim.ticks`, `sim.dropped_ticks` and the worker count per frame; workload evidence records read `jobs.overhead_ns` for Z2-B-JOBS-001 and `sim.ticks` for Z2-B-JOBS-002.

## Authoring and cooking

JOBS has no authoring surface and no cook step: it reads no asset, document or cooked format. Its only authored inputs are the configuration keys of § "Configuration keys", which are layered and edited through PLAT's configuration layering (`Zenith2/Docs/Architecture/PlatformLayer.md → "Configuration layering"`); the editor exposes them through EDIT's project-settings surface. ZenithCook links ZenithJobs to run its parallel cook passes through `RunRoot` and `ParallelFor`, which is a use of the API, not an authoring feature.

## Failure handling

| Failure | Where | Defined behaviour |
|---|---|---|
| invalid desc or clock desc | `Create`, `Zenith_ValidateSimulationClockDesc` | `JOBS_ERROR_INVALID_DESC`, nothing allocated; composition root exits 2 naming the key |
| allocation failure at start-up | `Create` steps 2–6 | `JOBS_ERROR_OUT_OF_MEMORY`; everything built so far unwinds |
| thread creation failure | `Create` steps 7–8 | `JOBS_ERROR_THREAD_CREATE`; started threads joined N..1; composition root exits 4 |
| CPU mask refused by the OS | thread start (CORE) | the thread runs on OS default placement; CORE logs it in `core.threading` |
| deque full | any push | the pushing thread runs the job itself (a launch or continuation joins its handoff chain; a `ParallelFor` root runs its leaves inside the call); `jobs.inline_runs`; nothing blocks or drops |
| injection queue full | render-submit ANY push | `ZENITH_FATAL(FATAL_CODE_QUEUE_FULL, "jobs.injection")`: the capacity comes from the schedule, so this is a defect; the render-submit thread never runs an ANY job |
| injection queue's foreign share used | `Zenith_JobsTryInjectNode` from a foreign producer | `JOBS_ERROR_QUEUE_FULL`, nothing taken; the caller retries; the frame producer's reserved cells stay free, so no foreign load can make its push fatal |
| foreign injection outside an iteration | `Zenith_JobsTryInjectNode` | `JOBS_ERROR_NO_ITERATION`, nothing taken; the caller retries at its own next wake-up, never by spinning on the call |
| background queue or operation table full | submission | `JOBS_ERROR_QUEUE_FULL`, nothing taken; a slice's re-queue is held by its worker |
| affinity queue full | main or render-submit push | `ZENITH_FATAL(FATAL_CODE_QUEUE_FULL)`: the schedule-derived capacity makes it a defect |
| arena block allocation failure or cap reached during a frame | record, node or worker arena (16 MiB cap each) | `Zenith_Arena::Allocate` routes through the OOM handler; exhaustion is `ZENITH_FATAL(FATAL_CODE_ARENA_EXHAUSTED)` ([Z2-N-CONTRACT-227]) |
| affinity- or injection-queue growth failure | `Zenith_JobsReserveAffinityCapacity`, `Zenith_JobsReserveInjectionCapacity` | the OOM handler path (`Zenith_Heap::Allocate`) |
| FP environment changed by third-party code | next job start | `ZENITH_ASSERT` where asserts are enabled; the wrapping rule of [Z2-N-CONTRACT-362] prevents it |
| a job body blocks | Core primitive | `ZENITH_CHECK` where asserts are enabled; the blocking-wait hook reports it in every build |
| frame cancellation | the launch cancel flag | unstarted bodies and leaves skip; started nodes finish; every completion callback runs |
| background cancellation | handle | release on its own thread, then `OPERATION_STATUS_CANCELLED` |
| background slice longer than 2 ms | slice end | HIGH or LOW: `jobs.bg.slice_overruns`; frame work is still served by the workers outside the background cap. TOOL: not counted, by design ([Z2-N-ARCH-443]); frame work keeps every worker outside the tools-host cap |
| `JOB_PRIORITY_BACKGROUND_TOOL` submitted to a job system created without `m_bAcceptToolBackground` (a Player, a test host that did not opt in) | submission | `JOBS_ERROR_INVALID_DESC`, nothing issued or pushed; one `jobs.scheduler` warning per job system (`Zenith2/Docs/Design/JOBS/Algorithms.md → "Tools-host background priority"`) |
| tools-host desc fields out of range (a cap above `min(max(1, ⌊N / 2⌋), background cap)`, a non-zero cap without acceptance, a capacity not a power of two in [64, 2^22]) | `Create` step 1 | `JOBS_ERROR_INVALID_DESC`, nothing allocated |
| tools-host queue full | TOOL submission | `JOBS_ERROR_QUEUE_FULL`, nothing taken; the tool operation retries at its next poll |
| submission during shutdown | submission | `JOBS_ERROR_SHUTTING_DOWN` |
| destruction with live work | destructor | `ZENITH_CHECK` |
| catch-up excess | `Advance` | ticks dropped, remainder kept, `sim.dropped_ticks` |
| device loss, suspend | — | not a JOBS failure: the runtime pauses the clock (`SetPaused(true)`) at a tick boundary and RHI recovers (`Zenith2/Docs/Architecture/PlatformLayer.md → "Android lifecycle and surface recovery"`) |
| budget breach of Z2-B-JOBS-001 | workload run | BR-ACCEPT: blocks the stage acceptance until fixed or re-budgeted |
| memory pressure or a breach of Z2-B-JOBS-004 | the memory governor (BR-GOVERNOR), which sees JOBS's bytes in `MEMORY_SUBSYSTEM_OTHER` (its budget includes Z2-B-JOBS-004 once cross-doc X59, pending ARCH, lands) | JOBS's declared policy is to shed nothing, matching the "Other \| none \| none" row of `Zenith2/Docs/Architecture.md → "Memory governor"` ("Shedding per subsystem"): it logs the level once; its queues are fixed by the schedule and Z2-B-JOBS-005, and its arenas already return to one retained block each frame (`Zenith2/Docs/Design/JOBS/Algorithms.md → "Create and failure unwinding"`) |
| a queue at its Z2-B-JOBS-005 capacity | any push | the declared full policy of the queue table (BR-REFUSE: REJECT with the inline run or `JOBS_ERROR_QUEUE_FULL`, [Z2-N-BUDGET-229]); the FATAL queues cannot reach it from load |

## Test strategy

The tests, one per case, with files and suites, are `Zenith2/Docs/Design/JOBS/TestCatalogue.md`. The strategy:

| Kind | What | Where |
|---|---|---|
| unit | the public behaviour of every type, including every failure scenario | "JobSystem tests", "JobContext tests", "SimulationClock tests", "FloatEnvironment tests", "Profiling tests" |
| container-style | oracle, property, failure and overflow tests of the two internal primitives ([Z2-N-CONTRACT-288]) | "Deque and queue tests" |
| the seam | routing, take order, completion, cancellation and iteration boundaries as GRAPH's executor uses them | "Node launch seam tests" |
| failure injection | every `Create` step ([Z2-N-CONTRACT-341]) | "JobSystem tests" |
| death | every programmer-error check | "Death and defect tests" |
| stress and sanitizer | TSan and UBSan on `linux-tsan`, ASan on `win-asan`, thread counts 2, 8, all ([Z2-N-CONTRACT-357]) | "Stress and sanitizer tests" |
| determinism | 1, 2, 8 and all workers, bit-identical | "JobContext tests" |
| examples | compiled usage examples | "Examples" |
| benchmarks | the Stage 1 harness and the justification benchmarks, smoke-run in `Test` | "Benchmarks" |

### Gate steps per test group

| Group | `Test` (win-debug, win-release) | `Test -Label memory` | `Sanitizers -Preset win-asan` | `Sanitizers -Preset linux-tsan` | Stage 2 acceptance |
|---|---|---|---|---|---|
| unit, integration, determinism, examples | yes | yes | yes | yes | yes, plus `android-arm64` device run |
| death suites | yes (separate CTest tests) | yes | yes | yes | yes |
| `z2-stress` | yes (600 s timeout) | yes | yes | yes | yes |
| benchmarks | `--smoke` only | — | — | — | full runs by the human for the ruling tickets |

The `Zenith2Memory` category's paths include the job graph, so every diff under `Zenith2/Engine/ZenithJobs/` unions in its `Test -Label memory` line from hand-over and its two `Sanitizers` lines from Stage 2 (`Zenith2/Docs/Contracts.md → "Enforceability matrix"`).

<!-- assumption: pending MOD -->
Every test of `ZenithJobs_Tests` carries the CTest label `memory`. JOBS assumes `zenith_add_library` applies that label to every test of a library under the `Zenith2Memory` category's paths; if the build instead labels per test, each JOBS test file sets it. Recorded in the cross-area notes (the build helper is specified in `Zenith2/Docs/Architecture/BuildSystem.md → "Test, conformance and fuzz targets"`).

### Sanitizer and device runs

- `linux-tsan` runs every JOBS test with TSan and UBSan; suppressions are allowed only with a decision ID, and JOBS needs none: every shared access is atomic or ordered by an acquire-release pair.
- `win-asan` runs every JOBS test; it proves the lifetime rules of § "Job memory model" (an arena read after its iteration's reset is poisoned).
- On the S24, the `android-arm64` test executables are not registered with CTest ([Z2-N-ARCH-180]); the Stage 2 acceptance's HWASan run on the device executes `ZenithJobs_Tests` through its needs-human device step and records the FP read-back and the automatic worker count.

## Budget rows

### Rows JOBS owns

| Row | Name | Status | Procedure | JOBS's measurement |
|---|---|---|---|---|
| Z2-B-JOBS-001 | Scheduler overhead per frame | pending, allocated by the Stage 1 per-system budget ticket for JOBS | MP-JOBOVERHEAD, breach BR-ACCEPT ([Z2-N-BUDGET-227]) | `jobs.overhead_ns` per frame at p95 over every WS-FRAME workload, plus `Budgets_JobsSchedulerOnly` at p95 per worker count |
| Z2-B-JOBS-002 | Bounded catch-up fixed steps per frame | pending, same producer | MP-COUNTER, breach BR-DROP | the value of `sim.maxCatchUpTicks`; `sim.ticks` never exceeds it by construction, and the excess is dropped with `sim.dropped_ticks` |
| Z2-B-JOBS-003 | CPU critical-path allocation headroom | pending, same producer | MP-ALLOC, breach BR-TEST | allocated first, before every other CPU row ([Z2-N-BUDGET-023]) |
| Z2-B-JOBS-004 | Job system resident memory | pending, same producer | MP-SUBMEM, breach BR-GOVERNOR ([Z2-N-BUDGET-230]) | `jobs.memory_bytes`, reported through `GetMemoryConsumer()` into `MEMORY_SUBSYSTEM_OTHER`, whose budget includes Z2-B-JOBS-004 (`Zenith2/Docs/Architecture.md → "Memory governor"`, table "Budgets"; the row joins that table through cross-doc X59, pending ARCH); the row itself is measured by MP-SUBMEM; the per-slot arena caps (16 MiB each) are defaults bound to this row; the count includes the background operation table and its ticket array (§ "In-memory layouts"), and a tool host's tools-host queue and its table slots, which no WS-FRAME workload contains (cross-doc item X379 restates [Z2-N-BUDGET-230] to match) |
| Z2-B-JOBS-005 | Job queue capacities | pending, same producer | MP-COUNTER, breach BR-REFUSE ([Z2-N-BUDGET-229]) | one value each for the worker deque, the injection queue, the affinity queues and the two background queues; `<queue>.high_water` and `<queue>.full`; until allocated, the `Zenith_JobSystemDesc` defaults; the tools-host queue (tool hosts only) is outside the row, its capacity the design constant `uJOBS_DEFAULT_TOOL_BACKGROUND_CAPACITY` (1,024) |

The producer is split per Z2-D-084: the ordinary harness ticket "Stage 1 per-system budget harness for JOBS" and the `needs-human` ruling ticket "Stage 1 per-system budget ticket for JOBS" (`Zenith2/Docs/Budgets.md → "Purpose versioning and change rule"`).

### Rows JOBS is measured against

- Z2-B-RPROD-021 (CPU render lag, adopted, 1 frame): GRAPH's executor structures the iteration; JOBS's part is running RENDER(k−1) launches concurrently with the tick launches in the same pool (Z2-D-050).
- Z2-B-GRAPH-001 (CPU frame-graph schedule recompile, pending): `Zenith_JobsReserveAffinityCapacity` and `Zenith_JobsReserveInjectionCapacity` run inside the swap the row times.
- The CPU-SIM and CPU-RENDER allocation bounds, of which Z2-B-JOBS-001 is a CPU-SIM member (`Zenith2/Docs/Budgets.md → "Purpose versioning and change rule"`, table "Allocation sums").
- Z2-B-SHIP-001 and Z2-B-SHIP-002 (frame time p95 and p99), which scheduler stalls would breach.

### Row needs recorded as pending

None outstanding. The two needs and the two corrections JOBS recorded (cross-doc X45) have landed in `Zenith2/Docs/Budgets.md`: Z2-B-JOBS-004 and Z2-B-JOBS-005 exist, Z2-B-JOBS-001 is measured with MP-JOBOVERHEAD, and Z2-B-JOBS-002 breaches with BR-DROP (§ "Rows JOBS owns").

## Stage plan

### Stage 1

- The harness ticket "Stage 1 per-system budget harness for JOBS" (`Zenith2/Benchmarks/Budgets/JOBS/`, CTest smoke), which builds the scheduler-only micro-workload `Budgets_JobsSchedulerOnly` of [Z2-N-BUDGET-228] — per frame for 7,200 frames, a fan-out of 1,024 empty jobs joined by one continuation and a chain of 64 dependent empty continuations, on 1, 2, 8 and all workers, reported at p95 per worker count — and runs it on the reference engine's `Zenith/TaskSystem` in Stage 1 and on ZenithJobs from Stage 2; and the `needs-human` ruling that allocates Z2-B-JOBS-001 to -005 (Z2-D-084). The ruling is blocked by no other allocation and blocks every other per-system ruling ([Z2-N-BUDGET-023]).
- Contribution to the Stage 1 "ECS, job and Luau lifetime specifications" ticket (`Zenith2/Docs/Roadmap.md → "Stage 1 Architecture validation"`): confirming this design's per-worker VM contract and FP environment with SCRIPT.
- Pull-forward (`Zenith2/Docs/Roadmap.md → "Stage 1 pull-forward"`, cross-doc item X131): the `ZenithJobs` API-only slices — the public headers with their contract blocks and the library target, the node launch seam header `Internal/Jobs/Zenith_JobNodeLaunch.h`, the injection-queue API in that seam, and the `Zenith_SimulationClock` header (Z2T-JOBS-005 to Z2T-JOBS-008) — are pulled forward once the Stage 1 "ECS, job and Luau lifetime specifications" ticket is Done, which is their only gate, because declaration-only slices need none of CORE's `Zenith_Multithreading` implementation tickets; a slice already filed into `Stage 2 — Foundation` moves with `zagent sprint add` and keeps its fix version. The behavioural JOBS slices below are pull-forward candidates under `Zenith2/Docs/Roadmap.md → "Pull-forward rule"` once that specification ticket and CORE's `Zenith_Multithreading` tickets are Done. JOBS depends on no open decision except through the heap backing (D-SEL-HEAP (open, Stage 1)), which changes no JOBS code. Pulled-forward tickets keep fix version `Z2 Stage 2`.

### Stage 2

In dependency order; each ticket lands its tests in the same diff:

| # | Slice | Kind | Blocked by |
|---|---|---|---|
| 1 | API-only: the JOBS public headers with contract blocks, `Zenith_JobsError`, `Zenith_ToString`, the limits, and the internal seam header `Internal/Jobs/Zenith_JobNodeLaunch.h`; out-of-line functions are declared only (nothing links them before their slice), and the header-only pieces (`Zenith_ParallelForLeaf*`) land with their tests | api | CORE memory and threading headers |
| 2 | `Zenith_FloatEnvironmentState` and its functions for the three platform directories, with their tests | implementation | 1 |
| 3 | `Zenith_WorkStealingDeque` and `Zenith_BoundedMpmcQueue`, their justification records and `Zenith_Bench_Jobs` | implementation | 1 |
| 4 | the job system core: `Create` with unwinding, thread states, the worker, main-helping and render-submit loops, parking, stealing, `RunRoot`, the lifecycle callback, CPU masks, `Zenith_ParseCpuMask` | implementation | 2, 3; CORE threads and semaphores |
| 5 | `Zenith_JobContext`: `ParallelFor`, stages, segments, continuations, arenas, captures, invocation IDs, the cancellation token | implementation | 4 |
| 6 | the node launch seam: `Zenith_JobsPushNodes`, completion callbacks, `Zenith_JobsRunMainUntil`, `Zenith_JobsWakeMain`, iteration begin and end, affinity and injection capacity, `Zenith_JobsTryInjectNode` | implementation | 5 |
| 7 | background operations: submission, slices, outcomes, cancellation, `CancelAndDrainBackground`, and the tools-host priority `JOB_PRIORITY_BACKGROUND_TOOL` of [Z2-N-ARCH-443] (admission, cap, queue, pop order, overrun exclusion; then its drain, counters and accounting, and its mixed-load stress test) | implementation | 4; CORE operation table |
| 8 | `Zenith_SimulationClock` | implementation | 1 |
| 9 | diagnostics: counters, zones, lanes, overhead measurement | implementation | 5, 6, 7 |
| 10 | stress, TSan and ASan suites | implementation | 5, 6, 7 |
| 11 | integration: the composition root's step 9, the frame loop's clock calls, shutdown steps 4 and 7; GRAPH's executor on the seam | integration | 6, 8; MOD's composition root; GRAPH's executor |
| 12 | Stage 2 acceptance contribution: sanitizer set, S24 HWASan device run, Z2-B-JOBS-001 confirmation | acceptance input | 10, 11 |

Slices 1 and 6 unblock GRAPH's executor tickets; slice 1 also unblocks the ECS, SCRIPT and MOD tickets that name JOBS types (`Zenith2/Docs/Roadmap.md → "Board conventions"`).

### API-only slices

Slice 1 is the one API-only ticket; it has no behaviour beyond the header-only functions, and every subsequent slice implements declarations it already fixed. A change to a declaration after slice 1 is an API change with review ([Z2-N-CONTRACT-491]).

### Later stages

JOBS delivers nothing new after Stage 2. It is measured in every subsequent stage's workloads (Z2-B-JOBS-001, -002) and its S24 behaviour (thermal throttling moving work between cores) is observed in the Stage 6 sustained runs; a regression there files a BUG on the JOBS epic.

## Decisions

### Settled decisions

| ID | Governs |
|---|---|
| Z2-D-006 | the CPU frame as a DAG; no fibers; waiting only as edges and continuations; the work-stealing pool on `Zenith_Multithreading`; affinity; bounded queues; orderly shutdown |
| Z2-D-050 | pipelining: RENDER(k−1) concurrent with simulation of k; one frame of CPU lag |
| Z2-D-019 | determinism regardless of worker count and scheduling (§ "Determinism obligations") |
| Z2-D-046 | no mutable global state: no `thread_local` or static state in ZenithJobs |
| Z2-D-018 | per-worker VMs, which the affinity contract serves |
| Z2-D-040, Z2-D-041 | ownership; custom infrastructure needs a justification record |
| Z2-D-045 | `Zenith_Result` errors, asserts for programmer errors |
| Z2-D-011 | the sanitizer presets the stress suites run in |
| Z2-D-012, Z2-D-084 | budgets and the harness-plus-ruling split for the JOBS rows |
| Z2-D-080 | the phase structure whose tick instances GRAPH's executor launches through the seam |

### Open decisions

- D-SEL-HEAP (open, Stage 1): the general-heap backing. It affects JOBS only through `Create`'s allocations and arena block growth; the design is independent of the outcome, and JOBS tickets are not blocked by it.
- D-PHASES refinement (open, Stage 1): may admit chunk systems to further phases; the executor runs whatever phase nodes the schedule holds, so the ruling changes no JOBS code.

### Selection decisions JOBS owns

JOBS owns no named selection decision. The choice of deque algorithm is made in this design and justified by the records below: Chase–Lev (fixed capacity, pointer slots, Lê et al. orderings) over a lock-based deque and over a third-party scheduler (enkiTS, oneTBB, marl), because the node launch seam needs per-node join counters, affinity classes and continuation segments that those libraries do not expose without their own schedulers, and marl is fiber-based (excluded by Z2-D-006).

### Justification records JOBS adds

Each is appended to `Zenith2/Docs/DecisionLog.md` with the next free `Z2-D-NNN` in the diff of Stage 2 slice 3 (Z2-D-097), in the template of `Zenith2/Docs/Contracts.md → "Containers and standard library policy"`:

| Record title | Standard alternative | Improvement claimed | Measured evidence | Acceptance criteria |
|---|---|---|---|---|
| `Custom threading primitive: Zenith_WorkStealingDeque` | `std::deque` guarded by `Zenith_Mutex` (the STL in `Internal/`, with an allowlist entry) | lock-free owner operations, LIFO locality, no lock on the scheduling path | `Zenith_Bench_Jobs` push/pop/steal throughput at 2, 8, all threads; medians and 95% CIs, 10 repetitions, reference desktop and S24 | at least 2× the baseline's throughput at 8 threads and no regression at 2 |
| `Custom threading primitive: Zenith_BoundedMpmcQueue` | the same guarded `std::deque` | bounded, allocation-free, lock-free multi-producer queue | the same harness, MPMC mode | at least 2× the baseline at 8 producers and consumers |

If a record's criteria are not met, the established alternative wins ([Z2-N-CONTRACT-279]) and the design is revised in that diff.

## Requirement index

Every requirement of `Zenith2/Docs/Requirements/JOBS.md` and the design headings that specify it (paths relative to `Zenith2/Docs/Design/`):

| Requirement | Title | Design headings |
|---|---|---|
| Z2-R-JOBS-001 | ZenithJobs library target | JOBS.md → "ZenithJobs target"; JOBS.md → "Libraries" |
| Z2-R-JOBS-002 | Threads from Zenith_Multithreading only | JOBS.md → "Zenith_Multithreading is a Core header"; JOBS.md → "Libraries" |
| Z2-R-JOBS-003 | Platform code confined to Internal platform directories | JOBS.md → "Platform files"; JOBS/Algorithms.md → "Internal types and files" |
| Z2-R-JOBS-004 | No mutable global state in ZenithJobs | JOBS/Algorithms.md → "Thread state and the no-global rule"; JOBS.md → "Job memory model" |
| Z2-R-JOBS-005 | Justification records for the custom threading primitives | JOBS.md → "Justification records JOBS adds"; JOBS/TestCatalogue.md → "Benchmarks" |
| Z2-R-JOBS-006 | Automatic worker count | JOBS/PublicAPI.md → "Zenith_JobSystemDesc"; JOBS/Algorithms.md → "Create and failure unwinding" |
| Z2-R-JOBS-007 | Thread slots and counts | JOBS/PublicAPI.md → "Zenith_JobSystem"; JOBS.md → "API summary" |
| Z2-R-JOBS-008 | Desc validation | JOBS/PublicAPI.md → "Zenith_JobSystemDesc"; JOBS/Algorithms.md → "Create and failure unwinding" |
| Z2-R-JOBS-009 | Chase-Lev deque per frame thread | JOBS/Algorithms.md → "Work-stealing deque" |
| Z2-R-JOBS-010 | Deque ABA safety | JOBS/Algorithms.md → "Work-stealing deque" |
| Z2-R-JOBS-011 | Bounded MPMC queue | JOBS/Algorithms.md → "Bounded MPMC queue" |
| Z2-R-JOBS-012 | Queue capacities and full policies | JOBS/Algorithms.md → "Queues and their full policies" |
| Z2-R-JOBS-013 | Worker loop order | JOBS/Algorithms.md → "Worker loop" |
| Z2-R-JOBS-014 | Steal victims and wake chain | JOBS/Algorithms.md → "Worker loop" |
| Z2-R-JOBS-015 | Parking without lost wake-ups | JOBS/Algorithms.md → "Parking and wake-up" |
| Z2-R-JOBS-016 | Spin before park | JOBS/Algorithms.md → "Worker loop"; JOBS.md → "Configuration keys" |
| Z2-R-JOBS-017 | Main thread helping order | JOBS/Algorithms.md → "Main thread helping" |
| Z2-R-JOBS-018 | Background concurrency cap | JOBS/Algorithms.md → "Worker loop"; JOBS/PublicAPI.md → "Zenith_JobSystemDesc" |
| Z2-R-JOBS-019 | No lock on scheduling paths | JOBS/Algorithms.md → "Queues and their full policies"; JOBS.md → "Lock-order entries" |
| Z2-R-JOBS-020 | Launches from the GRAPH executor | JOBS/Algorithms.md → "Node launch seam"; JOBS.md → "Boundary with GRAPH" |
| Z2-R-JOBS-021 | Completion callback exactly once | JOBS/Algorithms.md → "Completion"; JOBS/Algorithms.md → "Stages segments and continuations" |
| Z2-R-JOBS-022 | Take order follows the launch span | JOBS/Algorithms.md → "Push order and routing" |
| Z2-R-JOBS-023 | Launch routing by affinity | JOBS/Algorithms.md → "Push order and routing" |
| Z2-R-JOBS-024 | Concurrent launches share one pool | JOBS/Algorithms.md → "Push order and routing"; JOBS.md → "Boundary with GRAPH" |
| Z2-R-JOBS-025 | Main-thread helping for the executor | JOBS/Algorithms.md → "Main-thread helping and wake"; JOBS/Algorithms.md → "Main thread helping" |
| Z2-R-JOBS-026 | Iteration boundaries | JOBS/Algorithms.md → "Node launch seam"; JOBS/Algorithms.md → "Frame epochs" |
| Z2-R-JOBS-027 | Affinity queue capacity from the schedule | JOBS/Algorithms.md → "Queues and their full policies"; JOBS/Algorithms.md → "Node launch seam" |
| Z2-R-JOBS-028 | Root iterations for tools and tests | JOBS/PublicAPI.md → "Zenith_JobSystem"; JOBS/Algorithms.md → "Main thread helping" |
| Z2-R-JOBS-029 | ParallelFor call semantics | JOBS/PublicAPI.md → "Zenith_JobContext"; JOBS/Algorithms.md → "ParallelFor splitting and inline execution" |
| Z2-R-JOBS-030 | Pure leaf partition | JOBS.md → "Determinism obligations"; JOBS/Algorithms.md → "ParallelFor splitting and inline execution" |
| Z2-R-JOBS-031 | Lazy binary splitting | JOBS/Algorithms.md → "ParallelFor splitting and inline execution" |
| Z2-R-JOBS-032 | Sub-jobs joined before node completion | JOBS/Algorithms.md → "Stages segments and continuations" |
| Z2-R-JOBS-033 | Inline execution when the deque is full | JOBS/Algorithms.md → "ParallelFor splitting and inline execution" |
| Z2-R-JOBS-034 | Grain independent of worker count | JOBS/Algorithms.md → "ParallelFor splitting and inline execution"; JOBS.md → "Determinism obligations" |
| Z2-R-JOBS-035 | Stage and segment rules | JOBS/Algorithms.md → "Stages segments and continuations" |
| Z2-R-JOBS-036 | Continuations in registration order | JOBS/Algorithms.md → "Stages segments and continuations" |
| Z2-R-JOBS-037 | Later sub-jobs are not awaited | JOBS/Algorithms.md → "Stages segments and continuations" |
| Z2-R-JOBS-038 | Continuation affinity | JOBS/Algorithms.md → "Stages segments and continuations"; JOBS/Algorithms.md → "Push order and routing" |
| Z2-R-JOBS-039 | Continue callable from stages only | JOBS/PublicAPI.md → "Zenith_JobContext"; JOBS/Algorithms.md → "Stages segments and continuations" |
| Z2-R-JOBS-040 | Background continuations as slices | JOBS/Algorithms.md → "Background jobs and operations" |
| Z2-R-JOBS-041 | No fibers | JOBS.md → "Per-worker Luau VM affinity contract"; JOBS/Algorithms.md → "Job invocation" |
| Z2-R-JOBS-042 | Worker index contract for per-worker state | JOBS.md → "Per-worker Luau VM affinity contract" |
| Z2-R-JOBS-043 | Nesting rule for per-worker state | JOBS.md → "Per-worker Luau VM affinity contract"; JOBS/Algorithms.md → "ParallelFor splitting and inline execution" |
| Z2-R-JOBS-044 | Quiescence points for per-worker state | JOBS.md → "Per-worker Luau VM affinity contract"; JOBS/Algorithms.md → "Background jobs and operations" |
| Z2-R-JOBS-045 | Job-body mark around every body | JOBS/Algorithms.md → "Job invocation" |
| Z2-R-JOBS-046 | Only three blocking waits | JOBS/Algorithms.md → "Parking and wake-up"; JOBS.md → "API summary" |
| Z2-R-JOBS-047 | Zero blocking waits from job bodies | JOBS/Algorithms.md → "Job invocation"; JOBS/TestCatalogue.md → "JobSystem tests" |
| Z2-R-JOBS-048 | Cancelled launches complete | JOBS/Algorithms.md → "Cancellation semantics"; JOBS/Algorithms.md → "Completion" |
| Z2-R-JOBS-049 | Cancel flag read from the launch | JOBS/Algorithms.md → "Node launch seam"; JOBS/PublicAPI.md → "Zenith_JobContext" |
| Z2-R-JOBS-050 | Started work under frame cancellation | JOBS/Algorithms.md → "Cancellation semantics"; JOBS/Algorithms.md → "Stages segments and continuations" |
| Z2-R-JOBS-051 | Background cancel before start | JOBS/Algorithms.md → "Background jobs and operations"; JOBS/Algorithms.md → "Cancellation semantics" |
| Z2-R-JOBS-052 | Background cancel releases before reporting | JOBS/Algorithms.md → "Cancellation semantics" |
| Z2-R-JOBS-053 | Success before observation stays success | JOBS/Algorithms.md → "Cancellation semantics" |
| Z2-R-JOBS-054 | Handle destruction cancels and detaches | JOBS/Algorithms.md → "Cancellation semantics"; JOBS/PublicAPI.md → "Zenith_JobSystem" |
| Z2-R-JOBS-055 | Cancellation stress | JOBS/TestCatalogue.md → "Stress and sanitizer tests"; JOBS/Algorithms.md → "Cancellation semantics" |
| Z2-R-JOBS-056 | Cancellation token | JOBS/PublicAPI.md → "Zenith_CancellationToken" |
| Z2-R-JOBS-057 | Cancel and drain background | JOBS/Algorithms.md → "Shutdown sequence"; JOBS/PublicAPI.md → "Zenith_JobSystem" |
| Z2-R-JOBS-058 | Main-thread affinity | JOBS/Algorithms.md → "Main thread helping"; JOBS/Algorithms.md → "Push order and routing" |
| Z2-R-JOBS-059 | Render-submit affinity | JOBS/Algorithms.md → "Render-submit thread" |
| Z2-R-JOBS-060 | Headless configuration without a render-submit thread | JOBS/Algorithms.md → "Render-submit thread"; JOBS/PublicAPI.md → "Zenith_JobSystemDesc" |
| Z2-R-JOBS-061 | Optional OS CPU masks | JOBS/PublicAPI.md → "CPU masks"; JOBS.md → "Configuration keys" |
| Z2-R-JOBS-062 | Thread names | JOBS/Algorithms.md → "Create and failure unwinding"; JOBS.md → "On-screen profiler rows" |
| Z2-R-JOBS-063 | Thread lifecycle callback | JOBS/PublicAPI.md → "Zenith_JobSystemDesc"; JOBS.md → "Upward interfaces JOBS declares" |
| Z2-R-JOBS-064 | Per-iteration scheduler counters | JOBS.md → "Counters" |
| Z2-R-JOBS-065 | Queue counters | JOBS.md → "Counters"; JOBS/Algorithms.md → "Queues and their full policies" |
| Z2-R-JOBS-066 | Profiling zones and names | JOBS.md → "Zones" |
| Z2-R-JOBS-067 | Contention rows fed by counters alone | JOBS.md → "On-screen profiler rows" |
| Z2-R-JOBS-068 | Scheduler overhead counter | JOBS.md → "Counters"; JOBS.md → "Rows JOBS owns" |
| Z2-R-JOBS-069 | Background slice overruns counted | JOBS/Algorithms.md → "Background jobs and operations"; JOBS.md → "Counters" |
| Z2-R-JOBS-070 | Fixed tick duration | JOBS/Algorithms.md → "Simulation clock arithmetic" |
| Z2-R-JOBS-071 | Accumulator arithmetic | JOBS/Algorithms.md → "Simulation clock arithmetic" |
| Z2-R-JOBS-072 | Simulation reads tick time only | JOBS.md → "Determinism obligations"; JOBS/PublicAPI.md → "Zenith_SimulationClock header" |
| Z2-R-JOBS-073 | External tick mode | JOBS/PublicAPI.md → "Zenith_SimulationClock header"; JOBS/Algorithms.md → "Simulation clock arithmetic" |
| Z2-R-JOBS-074 | Clock desc validation | JOBS/PublicAPI.md → "Zenith_SimulationClock header" |
| Z2-R-JOBS-075 | Interpolation alpha | JOBS/Algorithms.md → "Simulation clock arithmetic" |
| Z2-R-JOBS-076 | Alpha reported and handed to extraction | JOBS.md → "Counters"; JOBS/PublicAPI.md → "Zenith_SimulationClock header" |
| Z2-R-JOBS-077 | Pause runs zero ticks | JOBS/Algorithms.md → "Simulation clock arithmetic" |
| Z2-R-JOBS-078 | Unpause credits no paused time | JOBS/Algorithms.md → "Simulation clock arithmetic" |
| Z2-R-JOBS-079 | Time scale range | JOBS/PublicAPI.md → "Zenith_SimulationClock header"; JOBS/Algorithms.md → "Simulation clock arithmetic" |
| Z2-R-JOBS-080 | Time scale applied to the accumulator | JOBS/Algorithms.md → "Simulation clock arithmetic" |
| Z2-R-JOBS-081 | Catch-up bounded per frame | JOBS/Algorithms.md → "Simulation clock arithmetic" |
| Z2-R-JOBS-082 | Catch-up limit from the budget row | JOBS/PublicAPI.md → "Zenith_SimulationClock header"; JOBS.md → "Rows JOBS owns" |
| Z2-R-JOBS-083 | Dropped time never reaches a replay | JOBS/PublicAPI.md → "Zenith_SimulationClock header"; JOBS.md → "Determinism obligations" |
| Z2-R-JOBS-084 | Nothing queued after the last iteration | JOBS/Algorithms.md → "Shutdown sequence"; JOBS/Algorithms.md → "Completion" |
| Z2-R-JOBS-085 | Join order | JOBS/Algorithms.md → "Shutdown sequence" |
| Z2-R-JOBS-086 | Destruction preconditions | JOBS/Algorithms.md → "Shutdown sequence" |
| Z2-R-JOBS-087 | Create unwinds each step | JOBS/Algorithms.md → "Create and failure unwinding" |
| Z2-R-JOBS-088 | Create and destroy under load | JOBS/Algorithms.md → "Shutdown sequence"; JOBS/TestCatalogue.md → "Stress and sanitizer tests" |
| Z2-R-JOBS-089 | Floating-point environment applied on every job thread | JOBS/Algorithms.md → "Floating-point environment application" |
| Z2-R-JOBS-090 | Floating-point environment asserted per job | JOBS/Algorithms.md → "Job invocation" |
| Z2-R-JOBS-091 | Fixed floating-point state per target | JOBS/PublicAPI.md → "Zenith_FloatEnvironment header" |
| Z2-R-JOBS-092 | Results independent of worker count | JOBS.md → "Determinism obligations" |
| Z2-R-JOBS-093 | Deterministic invocation IDs | JOBS/Algorithms.md → "Invocation IDs" |
| Z2-R-JOBS-094 | Iteration memory lifetimes | JOBS/Algorithms.md → "Job records and the memory model"; JOBS.md → "Job memory model" |
| Z2-R-JOBS-095 | Trivially destructible captures | JOBS/PublicAPI.md → "Zenith_JobContext"; JOBS.md → "Job memory model" |
| Z2-R-JOBS-096 | Job data outlives the node | JOBS.md → "Job memory model" |
| Z2-R-JOBS-097 | Background payload copy | JOBS/Algorithms.md → "Record types"; JOBS/PublicAPI.md → "Zenith_JobSystem" |
| Z2-R-JOBS-098 | Background submission results | JOBS/Algorithms.md → "Background jobs and operations"; JOBS.md → "Error codes" |
| Z2-R-JOBS-099 | Background submission from any thread | JOBS/PublicAPI.md → "Zenith_JobSystem" |
| Z2-R-JOBS-100 | Background outcome reporting | JOBS/Algorithms.md → "Background jobs and operations" |
| Z2-R-JOBS-101 | Scheduler overhead within its row | JOBS.md → "Rows JOBS owns" |
| Z2-R-JOBS-102 | Stage 1 JOBS budget harness | JOBS/TestCatalogue.md → "Benchmarks"; JOBS.md → "Stage 1" |
| Z2-R-JOBS-103 | CPU critical-path headroom allocated first | JOBS.md → "Rows JOBS owns" |
| Z2-R-JOBS-104 | Thread-sanitizer stress coverage | JOBS/TestCatalogue.md → "Stress and sanitizer tests"; JOBS.md → "Sanitizer and device runs" |
| Z2-R-JOBS-105 | Device run on the S24 | JOBS.md → "Sanitizer and device runs" |
| Z2-R-JOBS-106 | Examples compiled as tests | JOBS/TestCatalogue.md → "Examples" |
| Z2-R-JOBS-107 | Injection capacity from the schedule | JOBS/Algorithms.md → "Queues and their full policies"; JOBS/Algorithms.md → "Node launch seam"; JOBS.md → "Error codes" |
| Z2-R-JOBS-108 | Foreign-thread injection rejects when full | JOBS/Algorithms.md → "Push order and routing"; JOBS.md → "Failure handling" |
| Z2-R-JOBS-109 | Render-submit work does not fan out | JOBS/Algorithms.md → "Queues and their full policies"; JOBS/PublicAPI.md → "Zenith_JobContext" |
| Z2-R-JOBS-110 | Relaxed atomic scheduler counters | JOBS/Algorithms.md → "Counters and their memory model"; JOBS.md → "Counters" |
| Z2-R-JOBS-111 | Scheduler log category and messages | JOBS.md → "Log category" |
| Z2-R-JOBS-112 | Injected nodes join the iteration | JOBS/Algorithms.md → "Push order and routing"; JOBS/Algorithms.md → "Frame epochs"; JOBS.md → "Per-worker Luau VM affinity contract" |
| Z2-R-JOBS-113 | Heap-owned injected node state | JOBS/Algorithms.md → "Stages segments and continuations"; JOBS/Algorithms.md → "Lifetimes" |
| Z2-R-JOBS-114 | Memory-governor consumer | JOBS/Algorithms.md → "Create and failure unwinding"; JOBS.md → "Rows JOBS owns"; JOBS/PublicAPI.md → "Zenith_JobSystem" |
| Z2-R-JOBS-115 | Monotonic iteration frame index | JOBS/Algorithms.md → "Node launch seam" |
| Z2-R-JOBS-116 | Injected work bounded and counted | JOBS/Algorithms.md → "Node launch seam"; JOBS/Algorithms.md → "Push order and routing"; JOBS.md → "Counters" |
| Z2-R-JOBS-117 | Injected launch field contract | JOBS/Algorithms.md → "Node launch seam"; JOBS/Algorithms.md → "Invocation IDs"; JOBS.md → "Error codes" |
| Z2-R-JOBS-118 | Tools-host priority admitted by a run-time desc field | JOBS/Algorithms.md → "Tools-host background priority"; JOBS/PublicAPI.md → "Zenith_JobSystemDesc"; JOBS.md → "Error codes" |
| Z2-R-JOBS-119 | Tools-host concurrency cap | JOBS/Algorithms.md → "Tools-host background priority"; JOBS/Algorithms.md → "Worker loop"; JOBS/Algorithms.md → "Create and failure unwinding" |
| Z2-R-JOBS-120 | Tools-host queue, operation table and pop order | JOBS/Algorithms.md → "Tools-host background priority"; JOBS/Algorithms.md → "Queues and their full policies"; JOBS/Algorithms.md → "Worker loop" |
| Z2-R-JOBS-121 | Tools-host slices outside the overrun count, with their own counters | JOBS/Algorithms.md → "Background jobs and operations"; JOBS.md → "Counters" |
| Z2-R-JOBS-122 | Tools-host drain, cancellation and accounting | JOBS/Algorithms.md → "Tools-host background priority"; JOBS/Algorithms.md → "Shutdown sequence"; JOBS.md → "In-memory layouts" |
