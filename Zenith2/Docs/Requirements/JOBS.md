# JOBS requirement catalogue

The requirement catalogue of the JOBS area (Jobs & frame scheduling). Every requirement is one testable obligation with the inventory items it covers, its stage, classification, governing decisions, budget rows, acceptance evidence and the design headings that specify it. The design is `Zenith2/Docs/Design/JOBS.md` and its split files under `Zenith2/Docs/Design/JOBS/`.

## Purpose

JOBS owns the work-stealing scheduler that runs the node launches of GRAPH's frame executor (`Zenith_JobSystem`, `Zenith_JobContext`, the node launch seam), its sub-jobs, continuations and background operations, cancellation, thread affinity, contention diagnostics, the simulation clock (fixed steps, interpolation, pause, time scale, bounded catch-up), the per-thread floating-point environment and orderly shutdown, all in `Zenith2/Engine/ZenithJobs/`. GRAPH owns the frame-graph model, hazard analysis, schedule compilation and the frame executor whose node launches JOBS runs (`Zenith2/Docs/Design/JOBS.md → "Boundary with GRAPH"`). This catalogue is authoritative for coverage (Z2-D-066, Z2-D-067): every inventory item `Z2-I-JOBS-01` … `Z2-I-JOBS-15` is covered by at least one requirement below.

Evidence phrases: "the `Zenith2Memory` lines" are the category's `Test -Label memory` line (from hand-over) and its `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan` lines (from Stage 2); "death-test suites" are the `z2-death-check` and `z2-death-assert` doctest suites run as separate CTest tests (`Zenith2/Docs/Contracts.md → "Tests and code hygiene gates"`). Every named test is specified in `Zenith2/Docs/Design/JOBS/TestCatalogue.md`.

## Requirements

### Z2-R-JOBS-001 ZenithJobs library target

- **Statement:** ZenithJobs is one static library at `Zenith2/Engine/ZenithJobs/` whose CMakeLists.txt is exactly `zenith_add_library(ZenithJobs DEPENDS ZenithCore ZenithGraph ZenithProfiling)` with the default `SUPPORTED ZENITH_ALL` matrix, and it depends on no other Zenith2 library.
- **Covers:** Z2-I-JOBS-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-001, Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** the `Configure`, `Build`, `Libgraph` (no used-but-undeclared edge, no cycle) and `CompileCoverage` steps on every preset including linux-tsan and android-arm64
- **Design:** Zenith2/Docs/Design/JOBS.md → "ZenithJobs target"; Zenith2/Docs/Design/JOBS.md → "Libraries"

### Z2-R-JOBS-002 Threads from Zenith_Multithreading only

- **Statement:** Every thread, semaphore and mutex ZenithJobs uses comes from ZenithCore's `Zenith_Multithreading` headers; no ZenithJobs file includes `<thread>`, `<mutex>`, `<condition_variable>`, `<semaphore>`, an SDL3 header or calls an SDL3 threading function.
- **Covers:** Z2-I-JOBS-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** the `ForbiddenCheck` step (rules F4 and F18) and the `Libgraph` step
- **Design:** Zenith2/Docs/Design/JOBS.md → "Zenith_Multithreading is a Core header"; Zenith2/Docs/Design/JOBS.md → "Libraries"

### Z2-R-JOBS-003 Platform code confined to Internal platform directories

- **Statement:** Platform-specific ZenithJobs code (the floating-point environment; the processor count, CPU-mask application and the spin hint come from the ZenithCore header `Zenith_Multithreading`) exists only in the files under `Internal/Windows/`, `Internal/Linux/` and `Internal/Android/` listed in the design, and every other ZenithJobs file is identical on every platform.
- **Covers:** Z2-I-JOBS-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-001
- **Budgets:** none
- **Acceptance evidence:** the `SourceLayout`, `CompileCoverage` and `ForbiddenCheck` (F11, F18) steps
- **Design:** Zenith2/Docs/Design/JOBS.md → "Platform files"; Zenith2/Docs/Design/JOBS/Algorithms.md → "Internal types and files"

### Z2-R-JOBS-004 No mutable global state in ZenithJobs

- **Statement:** ZenithJobs holds no namespace-scope, static-member, function-local-static or `thread_local` mutable state; per-thread state is reached only through the pointer passed to a thread's entry function or through `Zenith_JobContext`.
- **Covers:** Z2-I-JOBS-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** the `ForbiddenCheck` step (F9 and the F16 writable-data symbol scan with no ZenithJobs allowlist entry)
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Thread state and the no-global rule"; Zenith2/Docs/Design/JOBS.md → "Job memory model"

### Z2-R-JOBS-005 Justification records for the custom threading primitives

- **Statement:** The diff that adds `Zenith_WorkStealingDeque` or `Zenith_BoundedMpmcQueue` appends a custom-infrastructure justification record for it to `Zenith2/Docs/DecisionLog.md` with the next free explicit `Z2-D-NNN`, citing the `Zenith_Bench_Jobs` measurement against a mutex-guarded `std::deque`.
- **Covers:** Z2-I-JOBS-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-041, Z2-D-097
- **Budgets:** none
- **Acceptance evidence:** the DecisionLog entries and `Zenith2/Docs/Research/Jobs/` raw data, confirmed in review; `Zenith_Bench_Jobs` smoke run in the `Test` step
- **Design:** Zenith2/Docs/Design/JOBS.md → "Justification records JOBS adds"; Zenith2/Docs/Design/JOBS/TestCatalogue.md → "Benchmarks"

### Z2-R-JOBS-006 Automatic worker count

- **Statement:** With `m_uWorkerCount = 0` the job system starts logical processors minus 2 workers, clamped to [1, 62].
- **Covers:** Z2-I-JOBS-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobSystem_WorkerCount_AutoIsCoresMinusTwo` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS/PublicAPI.md → "Zenith_JobSystemDesc"; Zenith2/Docs/Design/JOBS/Algorithms.md → "Create and failure unwinding"

### Z2-R-JOBS-007 Thread slots and counts

- **Statement:** The job system numbers its threads 0 (main), 1..N (workers) and N+1 (render-submit), and reports N, N+1 and N+2 through `GetWorkerCount`, `GetFrameThreadCount` and `GetThreadSlotCount`.
- **Covers:** Z2-I-JOBS-01, Z2-I-JOBS-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobSystem_WorkerCount_ExplicitHonoured` and `Test_JobSystem_RenderSubmitNotStarted_Headless` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS/PublicAPI.md → "Zenith_JobSystem"; Zenith2/Docs/Design/JOBS.md → "API summary"

### Z2-R-JOBS-008 Desc validation

- **Statement:** `Zenith_JobSystem::Create` rejects every desc field outside its documented range with `JOBS_ERROR_INVALID_DESC` before it allocates or starts anything.
- **Covers:** Z2-I-JOBS-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobSystem_InvalidDesc_IsRejected` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS/PublicAPI.md → "Zenith_JobSystemDesc"; Zenith2/Docs/Design/JOBS/Algorithms.md → "Create and failure unwinding"

### Z2-R-JOBS-009 Chase-Lev deque per frame thread

- **Statement:** Each of threads 0..N owns a fixed-capacity Chase-Lev deque: the owner pushes and pops at the bottom (LIFO), thieves steal at the top (FIFO), with the memory orders and fences of the design.
- **Covers:** Z2-I-JOBS-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_WorkStealingDeque_MatchesOracle`, `Test_WorkStealingDeque_Properties`, `Test_WorkStealingDeque_ConcurrentStealEveryElementOnce` and `Test_WorkStealingDeque_LastElementRace` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Work-stealing deque"

### Z2-R-JOBS-010 Deque ABA safety

- **Statement:** Deque indices are 64-bit values that never decrease across an operation, slots hold record pointers that a thief dereferences only after winning its compare-exchange, and the deque never grows, so no stale slot value is ever used and every slot access is atomic.
- **Covers:** Z2-I-JOBS-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_WorkStealingDeque_SlotReuseAcrossWrap` and `Test_WorkStealingDeque_ConcurrentStealEveryElementOnce` clean under the `Sanitizers -Preset linux-tsan` and `Sanitizers -Preset win-asan` lines of `Zenith2Memory` and the Stage 2 acceptance sanitizer set
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Work-stealing deque"

### Z2-R-JOBS-011 Bounded MPMC queue

- **Statement:** The injection, affinity and background queues are Vyukov bounded array queues whose values are copied only between an acquire and a release of their cell sequence, with high-water and full-event tracking.
- **Covers:** Z2-I-JOBS-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_BoundedMpmcQueue_MatchesOracle`, `Test_BoundedMpmcQueue_Properties`, `Test_BoundedMpmcQueue_EveryValueOnce`, `Test_BoundedMpmcQueue_HighWaterTracked` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Bounded MPMC queue"

### Z2-R-JOBS-012 Queue capacities and full policies

- **Statement:** Every job queue has the capacity and full policy of the design's queue table: deques REJECT with the pushing thread running the job itself, the background queues and a foreign thread's injection push REJECT with `JOBS_ERROR_QUEUE_FULL`, and the two affinity queues and the render-submit thread's injection push are FATAL; every grown capacity is rounded up to the next power of two, and the capacities are the values of Z2-B-JOBS-005 once it is allocated.
- **Covers:** Z2-I-JOBS-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** Z2-B-JOBS-005
- **Acceptance evidence:** `Test_JobSystem_DequeFull_RunsInline`, `Test_WorkStealingDeque_FullRejects_LeavesUnchanged`, `Test_JobSystem_InjectionFull_ReturnsQueueFull`, `Test_JobSystem_SubmitBackground_QueueFull_ReturnsError`, `Test_BoundedMpmcQueue_FullRejects_CountsFullEvent`, `Test_JobNodeLaunch_ReserveAffinityCapacity_Grows` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`; `Test_BoundedMpmcQueue_FatalPolicy_TerminatesWithCode` and `Test_JobNodeLaunch_RenderSubmitInjectionOverflow_Fatal` in the `Test` step (death-test suites, win-debug)
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Queues and their full policies"

### Z2-R-JOBS-013 Worker loop order

- **Statement:** A worker takes, in order, the head of its handoff chain, its own deque, the injection queue, a steal of up to 2N victims, and only after a sweep finds no frame work the held background continuation or the high then low background queue, then spins and parks.
- **Covers:** Z2-I-JOBS-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobSystem_BackgroundYieldsToFrameWork` and `Test_JobSystem_Stress_ParallelForStorm` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Worker loop"

### Z2-R-JOBS-014 Steal victims and wake chain

- **Statement:** A thief picks each victim as `c[x mod N]`, where `c` is the slots 0..N without its own in slot order and `x` its own xorshift64* draw seeded from its slot index; it counts a steal for `STEAL_RESULT_TAKEN` and a steal failure for `STEAL_RESULT_ABORT` (never for an empty victim), and wakes one parked thread when the victim still holds work after a successful steal.
- **Covers:** Z2-I-JOBS-01, Z2-I-JOBS-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobNodeLaunch_CountersPublishedPerIteration` and `Test_JobSystem_Stress_ParallelForStorm` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Worker loop"

### Z2-R-JOBS-015 Parking without lost wake-ups

- **Statement:** An idle thread parks on its own semaphore only after publishing itself in the idle mask, issuing a `seq_cst` fence and re-checking its sources, and every push of work issues a `seq_cst` fence after its queue store before loading the idle mask, so by the fence rule of [atomics.order] no pushed job is left waiting while a thread that may run it stays parked.
- **Covers:** Z2-I-JOBS-01, Z2-I-JOBS-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobSystem_Stress_ParkWakeNoLostWakeups` and `Test_JobSystem_IdleThreadsPark` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Parking and wake-up"

### Z2-R-JOBS-016 Spin before park

- **Statement:** An idle thread spins up to `m_uSpinIterations` CPU-relax iterations, re-checking its own deque and the injection queue each iteration and stealing every 32nd, before it parks.
- **Covers:** Z2-I-JOBS-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobSystem_IdleThreadsPark` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`; confirmed in review
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Worker loop"; Zenith2/Docs/Design/JOBS.md → "Configuration keys"

### Z2-R-JOBS-017 Main thread helping order

- **Statement:** Inside `Zenith_JobsRunMainUntil` and `RunRoot` the main thread runs main-affinity jobs first, then frame work from its deque, the injection queue and steals, never background work, and parks only while the iteration is incomplete and nothing it may run is visible.
- **Covers:** Z2-I-JOBS-01, Z2-I-JOBS-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobNodeLaunch_AffinityRouting` and `Test_JobSystem_BackgroundYieldsToFrameWork` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Main thread helping"

### Z2-R-JOBS-018 Background concurrency cap

- **Statement:** At most `m_uMaxConcurrentBackground` workers (default N minus 1, at least 1) run background slices at once, so with two or more workers at least one worker always sweeps frame work.
- **Covers:** Z2-I-JOBS-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobSystem_Background_ConcurrencyCap` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Worker loop"; Zenith2/Docs/Design/JOBS/PublicAPI.md → "Zenith_JobSystemDesc"

### Z2-R-JOBS-019 No lock on scheduling paths

- **Statement:** No push, pop, steal, park, wake, invocation or completion path of JOBS acquires a `Zenith_Mutex`; background submission and completion reach only CORE's short-hold operation-table lock.
- **Covers:** Z2-I-JOBS-01, Z2-I-JOBS-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobSystem_SchedulingPaths_TakeNoLock` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Queues and their full policies"; Zenith2/Docs/Design/JOBS.md → "Lock-order entries"

### Z2-R-JOBS-020 Launches from the GRAPH executor

- **Statement:** The job system runs every node launch GRAPH's executor passes to `Zenith_JobsPushNodes` exactly once, as stage 0 of a node whose sub-jobs and continuations follow the rules of the design, and recomputes nothing of the schedule.
- **Covers:** Z2-I-JOBS-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobNodeLaunch_CompleteCalledOnceAfterEverything` and `Test_JobSystem_Stress_RandomLaunchGraphs` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`; GRAPH's executor tests run on the seam
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Node launch seam"; Zenith2/Docs/Design/JOBS.md → "Boundary with GRAPH"

### Z2-R-JOBS-021 Completion callback exactly once

- **Statement:** JOBS calls a launch's completion callback exactly once, after the body returned, every transitively spawned sub-job finished and every registered continuation finished, on the thread that finished the last of them, with all their writes visible to it.
- **Covers:** Z2-I-JOBS-02, Z2-I-JOBS-03, Z2-I-JOBS-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobNodeLaunch_CompleteCalledOnceAfterEverything` and `Test_JobContext_ParallelFor_JoinsBeforeSuccessor` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Completion"; Zenith2/Docs/Design/JOBS/Algorithms.md → "Stages segments and continuations"

### Z2-R-JOBS-022 Take order follows the launch span

- **Statement:** For a span of launches sorted by descending critical-path priority, the consumer's next take from every destination queue is the earliest launch of the span bound for it, the pushing thread taking the first ANY launch as its handoff, and take order never changes a result.
- **Covers:** Z2-I-JOBS-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_JobNodeLaunch_TakeOrderFollowsSpan` and `Test_JobContext_MergeOrder_IndependentOfWorkers` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Push order and routing"

### Z2-R-JOBS-023 Launch routing by affinity

- **Statement:** A launch is routed by its affinity: main to the main-affinity queue, render-submit to the render-submit queue, ANY to the pushing frame thread's deque or, from the render-submit thread, to the injection queue; a null pusher is accepted only from the main thread inside an iteration, identified by the thread ID recorded at `Create`.
- **Covers:** Z2-I-JOBS-02, Z2-I-JOBS-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobNodeLaunch_AffinityRouting` and `Test_JobNodeLaunch_NullPusherOnMainThread_UsesSlot0` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`; `Test_JobNodeLaunch_NullPusherOffMainThread_Checks` in the `Test` step (death-test suites, win-debug)
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Push order and routing"

### Z2-R-JOBS-024 Concurrent launches share one pool

- **Statement:** Launches of different subgraph instances that GRAPH has in flight at once (the render subgraph of frame k minus 1 beside the ticks of frame k) run concurrently in the same pool with no ordering between them other than GRAPH's.
- **Covers:** Z2-I-JOBS-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-050
- **Budgets:** Z2-B-RPROD-021
- **Acceptance evidence:** `Test_JobSystem_Stress_RandomLaunchGraphs` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`; GRAPH's `Test_FrameGraph_PipelinedRender_OverlapsSimulation`
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Push order and routing"; Zenith2/Docs/Design/JOBS.md → "Boundary with GRAPH"

### Z2-R-JOBS-025 Main-thread helping for the executor

- **Statement:** `Zenith_JobsRunMainUntil` runs main-affinity work first and other frame work after it on the main thread, parks only while nothing it may run is visible, returns once the given flag is set, and `Zenith_JobsWakeMain` wakes it from any thread.
- **Covers:** Z2-I-JOBS-02, Z2-I-JOBS-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobNodeLaunch_RunMainUntilReturnsWhenDone` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Main-thread helping and wake"; Zenith2/Docs/Design/JOBS/Algorithms.md → "Main thread helping"

### Z2-R-JOBS-026 Iteration boundaries

- **Statement:** `Zenith_JobsBeginIteration` advances the frame epoch and marks the main thread as helping, a nested call fails a `ZENITH_CHECK`, and `Zenith_JobsEndIteration` publishes the iteration's counters.
- **Covers:** Z2-I-JOBS-02, Z2-I-JOBS-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobNodeLaunch_BeginIterationReclaimsArenas` and `Test_JobNodeLaunch_CountersPublishedPerIteration` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`; `Test_JobNodeLaunch_BeginIterationTwice_Checks` in the `Test` step (death-test suites, win-debug)
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Node launch seam"; Zenith2/Docs/Design/JOBS/Algorithms.md → "Frame epochs"

### Z2-R-JOBS-027 Affinity queue capacity from the schedule

- **Statement:** `Zenith_JobsReserveAffinityCapacity` grows the main-affinity queue to twice the schedule's main-affinity node count plus 256 and the render-submit queue to twice its render-submit count plus 64 before the schedule runs.
- **Covers:** Z2-I-JOBS-02, Z2-I-JOBS-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** Z2-B-GRAPH-001
- **Acceptance evidence:** `Test_JobNodeLaunch_ReserveAffinityCapacity_Grows` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Queues and their full policies"; Zenith2/Docs/Design/JOBS/Algorithms.md → "Node launch seam"

### Z2-R-JOBS-028 Root iterations for tools and tests

- **Statement:** `Zenith_JobSystem::RunRoot` runs one root frame job on the main thread with a full job context and returns only when the root, its sub-jobs and its continuations have finished, and is refused inside an executor iteration.
- **Covers:** Z2-I-JOBS-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobSystem_RunRoot_JoinsEverything` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`; `Test_JobSystem_RunRootDuringIteration_Checks` in the `Test` step (death-test suites, win-debug)
- **Design:** Zenith2/Docs/Design/JOBS/PublicAPI.md → "Zenith_JobSystem"; Zenith2/Docs/Design/JOBS/Algorithms.md → "Main thread helping"

### Z2-R-JOBS-029 ParallelFor call semantics

- **Statement:** `Zenith_JobContext::ParallelFor` schedules its body over [0, uCount) and returns immediately, does nothing for a zero count, treats a zero grain as 1 after an assert, and is valid only in frame jobs.
- **Covers:** Z2-I-JOBS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobContext_ParallelFor_JoinsBeforeSuccessor` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`; `Test_JobContext_ParallelForZeroGrain_Asserts` and `Test_JobContext_ParallelForInBackground_Asserts` in the `Test` step (death-test suites, win-debug)
- **Design:** Zenith2/Docs/Design/JOBS/PublicAPI.md → "Zenith_JobContext"; Zenith2/Docs/Design/JOBS/Algorithms.md → "ParallelFor splitting and inline execution"

### Z2-R-JOBS-030 Pure leaf partition

- **Statement:** Every body call of a `ParallelFor` covers exactly one leaf `Zenith_ParallelForLeaf(uCount, uGrain, k)`, so the set of body calls depends only on the count and the grain, never on the worker count, the thief or inline execution.
- **Covers:** Z2-I-JOBS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_JobContext_ParallelFor_LeafPartitionIsPure` and `Test_JobContext_ParallelFor_InlineKeepsLeaves` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS.md → "Determinism obligations"; Zenith2/Docs/Design/JOBS/Algorithms.md → "ParallelFor splitting and inline execution"

### Z2-R-JOBS-031 Lazy binary splitting

- **Statement:** A range record pushes its upper half at grain-aligned midpoints before running any body call of its own, so a split chain queues at most the ceiling of log2 of the leaf count records.
- **Covers:** Z2-I-JOBS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobContext_ParallelFor_NestingOnlyInline` and `Test_JobSystem_Stress_ParallelForStorm` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "ParallelFor splitting and inline execution"

### Z2-R-JOBS-032 Sub-jobs joined before node completion

- **Statement:** A node completes only after its body returned, every sub-job it or its sub-jobs spawned finished and every continuation it registered finished, so every successor observes every sub-job's writes.
- **Covers:** Z2-I-JOBS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobContext_ParallelFor_JoinsBeforeSuccessor`, `Test_JobContext_ParallelFor_NestedJoins` and `Test_JobContext_Continue_NodeCompletesAfterLastSegment` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Stages segments and continuations"

### Z2-R-JOBS-033 Inline execution when the deque is full

- **Statement:** A rejected root push runs every leaf inside the `ParallelFor` call and a rejected split runs the remaining leaves sequentially in the range runner, each counted in `jobs.inline_runs`, and no job is dropped or blocked.
- **Covers:** Z2-I-JOBS-03, Z2-I-JOBS-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobSystem_DequeFull_RunsInline` and `Test_JobContext_ParallelFor_InlineKeepsLeaves` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "ParallelFor splitting and inline execution"

### Z2-R-JOBS-034 Grain independent of worker count

- **Statement:** Simulation code passes a `ParallelFor` grain that is a constant or a function of the item count alone, never of the worker count.
- **Covers:** Z2-I-JOBS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_JobContext_MergeOrder_IndependentOfWorkers` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`; confirmed in review of every simulation `ParallelFor` call site
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "ParallelFor splitting and inline execution"; Zenith2/Docs/Design/JOBS.md → "Determinism obligations"

### Z2-R-JOBS-035 Stage and segment rules

- **Statement:** A node's continuation i starts when stage i minus 1 has returned and every sub-job spawned before continuation i was registered has finished, and the node completes when the last stage has returned and its last segment has drained.
- **Covers:** Z2-I-JOBS-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobContext_Continue_RunsAfterPriorSubJobs` and `Test_JobContext_Continue_NodeCompletesAfterLastSegment` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Stages segments and continuations"

### Z2-R-JOBS-036 Continuations in registration order

- **Statement:** Continuations registered by one node run in registration order, each after the previous one returned.
- **Covers:** Z2-I-JOBS-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobContext_Continue_ChainsInOrder` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Stages segments and continuations"

### Z2-R-JOBS-037 Later sub-jobs are not awaited

- **Statement:** A continuation does not wait for sub-jobs spawned after its own registration; the next continuation registered after them, or node completion, does.
- **Covers:** Z2-I-JOBS-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobContext_Continue_RunsAfterPriorSubJobs` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Stages segments and continuations"

### Z2-R-JOBS-038 Continuation affinity

- **Statement:** A continuation runs with its node's affinity: a main-affinity node's continuations on thread 0, a render-submit node's on thread N+1, an ANY node's on any frame thread.
- **Covers:** Z2-I-JOBS-04, Z2-I-JOBS-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobContext_Continue_AffinityInherited` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Stages segments and continuations"; Zenith2/Docs/Design/JOBS/Algorithms.md → "Push order and routing"

### Z2-R-JOBS-039 Continue callable from stages only

- **Statement:** `Continue` is accepted from a node body or a continuation and refused with an assert from a `ParallelFor` body, so one node's registrations are made by one thread at a time.
- **Covers:** Z2-I-JOBS-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobContext_ContinueFromRangeBody_Asserts` in the `Test` step (death-test suites, win-debug)
- **Design:** Zenith2/Docs/Design/JOBS/PublicAPI.md → "Zenith_JobContext"; Zenith2/Docs/Design/JOBS/Algorithms.md → "Stages segments and continuations"

### Z2-R-JOBS-040 Background continuations as slices

- **Statement:** A background slice that calls `Continue` is re-queued at its priority after it returns, a re-queue that finds the queue full is held by its worker and run before any other background work, and the operation completes when a slice returns without a continuation.
- **Covers:** Z2-I-JOBS-04, Z2-I-JOBS-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobSystem_Background_ContinueRunsSlicesInOrder` and `Test_JobSystem_Background_HeldContinuationNotLost` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Background jobs and operations"

### Z2-R-JOBS-041 No fibers

- **Statement:** Every job runs start to finish on the native stack of one thread: ZenithJobs contains no fiber, coroutine-stack or context-switch API (`CreateFiber`, `SwitchToFiber`, `ucontext`, `swapcontext`), and a job's worker index is constant for the whole invocation.
- **Covers:** Z2-I-JOBS-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobContext_WorkerIndex_StableWithinJob` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`; confirmed in review
- **Design:** Zenith2/Docs/Design/JOBS.md → "Per-worker Luau VM affinity contract"; Zenith2/Docs/Design/JOBS/Algorithms.md → "Job invocation"

### Z2-R-JOBS-042 Worker index contract for per-worker state

- **Statement:** A thread's index never changes while the job system lives, ANY-affinity work always reports an index in [0, N], and render-submit work alone reports N+1, so a per-slot array of `GetFrameThreadCount()` entries indexed by `GetWorkerIndex()` is used by one thread at a time without a lock.
- **Covers:** Z2-I-JOBS-05, Z2-I-JOBS-08, Z2-I-SCRIPT-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-018
- **Budgets:** none
- **Acceptance evidence:** `Test_JobContext_WorkerIndex_StableWithinJob` and `Test_JobContext_AnyWorkNeverOnRenderSubmit` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS.md → "Per-worker Luau VM affinity contract"

### Z2-R-JOBS-043 Nesting rule for per-worker state

- **Statement:** One job runs inside another on the same thread exactly when a `ParallelFor` root push is rejected, whether the caller is a stage or a range body; a leaf reached by splitting or stealing never runs inside another body; and on every path the leaf partition is the pure function of (count, grain) and a nested leaf runs on its caller's slot, which is what makes a per-slot VM table safe.
- **Covers:** Z2-I-JOBS-05, Z2-I-SCRIPT-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-018
- **Budgets:** none
- **Acceptance evidence:** `Test_JobContext_ParallelFor_NestingOnlyInline` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS.md → "Per-worker Luau VM affinity contract"; Zenith2/Docs/Design/JOBS/Algorithms.md → "ParallelFor splitting and inline execution"

### Z2-R-JOBS-044 Quiescence points for per-worker state

- **Statement:** Between two iterations (from `Zenith_JobsEndIteration` or the end of `RunRoot` to the next iteration) no frame job runs, including an injected one, and background jobs never run chunk systems or touch script VMs, so a per-worker VM slot can be created or destroyed by the main thread there without a lock.
- **Covers:** Z2-I-JOBS-05, Z2-I-SCRIPT-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-018
- **Budgets:** none
- **Acceptance evidence:** `Test_JobNodeLaunch_BeginIterationReclaimsArenas` and `Test_JobNodeLaunch_InjectedNodeJoinsIteration` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`; confirmed in review of SCRIPT's VM teardown
- **Design:** Zenith2/Docs/Design/JOBS.md → "Per-worker Luau VM affinity contract"; Zenith2/Docs/Design/JOBS/Algorithms.md → "Background jobs and operations"

### Z2-R-JOBS-045 Job-body mark around every body

- **Statement:** The job system sets ZenithCore's job-body mark before and clears it after every job body, so a blocking primitive called from a body fails its Core check wherever asserts are enabled while the scheduler's own parks pass.
- **Covers:** Z2-I-JOBS-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobSystem_BlockingWaitInJob_Checks` in the `Test` step (death-test suites, win-debug); `Test_JobSystem_IdleThreadsPark` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Job invocation"

### Z2-R-JOBS-046 Only three blocking waits

- **Statement:** ZenithJobs blocks only when a thread parks with nothing to run, when the render-submit thread waits inside its own GPU-timeline or present jobs through the RHI, and when the main thread parks inside `Zenith_JobsRunMainUntil`, `RunRoot` or `CancelAndDrainBackground`; its API offers no wait on a job or operation.
- **Covers:** Z2-I-JOBS-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobSystem_NoBlockingWaitInJobs` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`; the public headers are confirmed to declare no wait in review
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Parking and wake-up"; Zenith2/Docs/Design/JOBS.md → "API summary"

### Z2-R-JOBS-047 Zero blocking waits from job bodies

- **Statement:** Across 1,000 iterations of 50 launches with sub-jobs and continuations, the blocking-wait hook records no call made while a job body runs.
- **Covers:** Z2-I-JOBS-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobSystem_NoBlockingWaitInJobs` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Job invocation"; Zenith2/Docs/Design/JOBS/TestCatalogue.md → "JobSystem tests"

### Z2-R-JOBS-048 Cancelled launches complete

- **Statement:** A launch whose cancel flag is set before its body starts runs no body and calls its completion callback at once, so every launch of a cancelled iteration completes.
- **Covers:** Z2-I-JOBS-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobNodeLaunch_CancelledBeforeStart_SkipsBody` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`; GRAPH's `Test_FrameGraph_CancelledInstance_Completes`
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Cancellation semantics"; Zenith2/Docs/Design/JOBS/Algorithms.md → "Completion"

### Z2-R-JOBS-049 Cancel flag read from the launch

- **Statement:** `IsCancelled()` in a launched node, its sub-jobs and its continuations reads the flag the launch names, with an acquire load, from any thread, and GRAPH alone sets it.
- **Covers:** Z2-I-JOBS-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobContext_CancellationToken_ReflectsCancel` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Node launch seam"; Zenith2/Docs/Design/JOBS/PublicAPI.md → "Zenith_JobContext"

### Z2-R-JOBS-050 Started work under frame cancellation

- **Statement:** After cancellation, `ParallelFor` leaves not yet started are skipped and counted done, while running stages finish and the continuations of started nodes still run with `IsCancelled()` true so they release what the node holds and publish nothing.
- **Covers:** Z2-I-JOBS-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobNodeLaunch_CancelledWhileRunning_ContinuationsStillRun` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Cancellation semantics"; Zenith2/Docs/Design/JOBS/Algorithms.md → "Stages segments and continuations"

### Z2-R-JOBS-051 Background cancel before start

- **Statement:** A background operation whose cancellation was requested before its first slice completes `OPERATION_STATUS_CANCELLED` without its function running.
- **Covers:** Z2-I-JOBS-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobSystem_Background_CancelBeforeStart_NothingRuns` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Background jobs and operations"; Zenith2/Docs/Design/JOBS/Algorithms.md → "Cancellation semantics"

### Z2-R-JOBS-052 Background cancel releases before reporting

- **Statement:** A background job that observes cancellation releases what it holds on its own thread and only then reports `OPERATION_STATUS_CANCELLED`, and each of its slices lasts at most 2 ms so it observes the request within one slice.
- **Covers:** Z2-I-JOBS-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobSystem_Background_CancelWhileRunning_ReleasesThenReports` and `Test_JobSystem_Background_SliceOverrunCounted` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Cancellation semantics"

### Z2-R-JOBS-053 Success before observation stays success

- **Statement:** A background operation that finishes before observing a cancellation request reports `OPERATION_STATUS_SUCCEEDED`, and its requester owns its results as for any success.
- **Covers:** Z2-I-JOBS-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobSystem_Background_SucceedsBeforeCancelObserved` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Cancellation semantics"

### Z2-R-JOBS-054 Handle destruction cancels and detaches

- **Statement:** Destroying an incomplete operation handle requests cancellation and detaches; the operation still completes, frees its table slot and leaves no allocation.
- **Covers:** Z2-I-JOBS-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobSystem_Background_HandleDestroyed_DetachesAndCompletes` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Cancellation semantics"; Zenith2/Docs/Design/JOBS/PublicAPI.md → "Zenith_JobSystem"

### Z2-R-JOBS-055 Cancellation stress

- **Statement:** One thousand issue-then-cancel cycles at randomised slices leave no allocation, operation-table slot or queued record behind.
- **Covers:** Z2-I-JOBS-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobSystem_Stress_Background1000CancelCycles` and `Test_JobSystem_Stress_BackgroundAndFrameMix` in the `Sanitizers -Preset linux-tsan` and `Sanitizers -Preset win-asan` lines of `Zenith2Memory` and the Stage 2 acceptance sanitizer set
- **Design:** Zenith2/Docs/Design/JOBS/TestCatalogue.md → "Stress and sanitizer tests"; Zenith2/Docs/Design/JOBS/Algorithms.md → "Cancellation semantics"

### Z2-R-JOBS-056 Cancellation token

- **Statement:** `Zenith_JobContext::GetCancellationToken` returns a token that reads the same flag as `IsCancelled` from any thread until the job returns, and a default token never reads cancelled.
- **Covers:** Z2-I-JOBS-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobContext_CancellationToken_ReflectsCancel` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS/PublicAPI.md → "Zenith_CancellationToken"

### Z2-R-JOBS-057 Cancel and drain background

- **Statement:** `CancelAndDrainBackground` refuses subsequent submissions with `JOBS_ERROR_SHUTTING_DOWN`, requests cancellation of every incomplete operation, and returns only when every operation has reported complete.
- **Covers:** Z2-I-JOBS-07, Z2-I-JOBS-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobSystem_CancelAndDrainBackground_CompletesAll` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Shutdown sequence"; Zenith2/Docs/Design/JOBS/PublicAPI.md → "Zenith_JobSystem"

### Z2-R-JOBS-058 Main-thread affinity

- **Statement:** Nodes and continuations with `JOB_AFFINITY_MAIN` run on thread 0 alone, through the main-affinity queue that the main thread drains first.
- **Covers:** Z2-I-JOBS-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobNodeLaunch_AffinityRouting` and `Test_JobContext_Continue_AffinityInherited` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Main thread helping"; Zenith2/Docs/Design/JOBS/Algorithms.md → "Push order and routing"

### Z2-R-JOBS-059 Render-submit affinity

- **Statement:** Nodes and continuations with `JOB_AFFINITY_RENDER_SUBMIT` run on thread N+1 alone, and that thread never steals, never pops the injection queue, never runs background work and never runs a `JOB_AFFINITY_ANY` job on any path, including a full injection queue.
- **Covers:** Z2-I-JOBS-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_JobNodeLaunch_AffinityRouting` and `Test_JobContext_AnyWorkNeverOnRenderSubmit` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Render-submit thread"

### Z2-R-JOBS-060 Headless configuration without a render-submit thread

- **Statement:** With `m_bStartRenderSubmitThread = false` no render-submit thread starts, the slot numbering is unchanged, and pushing render-submit work fails a `ZENITH_CHECK`.
- **Covers:** Z2-I-JOBS-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobSystem_RenderSubmitNotStarted_Headless` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`; `Test_JobSystem_RenderSubmitWithoutThread_Checks` in the `Test` step (death-test suites, win-debug)
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Render-submit thread"; Zenith2/Docs/Design/JOBS/PublicAPI.md → "Zenith_JobSystemDesc"

### Z2-R-JOBS-061 Optional OS CPU masks

- **Statement:** Workers and the render-submit thread run on OS default placement unless a non-zero mask is configured, which JOBS passes to CORE's `Zenith_ThreadDesc::m_ulAffinityMask`, and `Zenith_ParseCpuMask` rejects malformed configuration values.
- **Covers:** Z2-I-JOBS-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobSystem_CpuMask_JobsStillRun` and `Test_JobSystem_ParseCpuMask_ValidAndInvalid` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS/PublicAPI.md → "CPU masks"; Zenith2/Docs/Design/JOBS.md → "Configuration keys"

### Z2-R-JOBS-062 Thread names

- **Statement:** The job system starts its threads through CORE's thread desc with the names `Z2 Worker NN` (two decimal digits) and `Z2 RenderSubmit`, 1 MiB stacks, `THREAD_PRIORITY_NORMAL` for workers and `THREAD_PRIORITY_ABOVE_NORMAL` for the render-submit thread, and the configured CPU masks, and registers the same names as profiler lanes when a profiler is supplied.
- **Covers:** Z2-I-JOBS-08, Z2-I-JOBS-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobSystem_ThreadLifecycle_StartStopPairs` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`; a Tracy capture on the reference desktop in the Stage 2 acceptance
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Create and failure unwinding"; Zenith2/Docs/Design/JOBS.md → "On-screen profiler rows"

### Z2-R-JOBS-063 Thread lifecycle callback

- **Statement:** When `m_pfnThreadLifecycle` is set, every JOBS thread calls it once with `bStarted = true` after its start-up and once with `bStarted = false` as its last statement.
- **Covers:** Z2-I-JOBS-08, Z2-I-JOBS-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobSystem_ThreadLifecycle_StartStopPairs` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS/PublicAPI.md → "Zenith_JobSystemDesc"; Zenith2/Docs/Design/JOBS.md → "Upward interfaces JOBS declares"

### Z2-R-JOBS-064 Per-iteration scheduler counters

- **Statement:** At the end of every iteration the job system reports through the installed counter hook each counter of the design's counter table, including `jobs.steals`, `jobs.steal_failures`, `jobs.inline_runs`, `jobs.idle_ns.<i>`, `jobs.parked` and `jobs.ready_high_water`.
- **Covers:** Z2-I-JOBS-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_JobNodeLaunch_CountersPublishedPerIteration` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS.md → "Counters"

### Z2-R-JOBS-065 Queue counters

- **Statement:** Every job queue reports `<queue>.full`, `<queue>.dropped` and `<queue>.high_water` once per iteration under the names of the design.
- **Covers:** Z2-I-JOBS-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobNodeLaunch_CountersPublishedPerIteration` and `Test_BoundedMpmcQueue_HighWaterTracked` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS.md → "Counters"; Zenith2/Docs/Design/JOBS/Algorithms.md → "Queues and their full policies"

### Z2-R-JOBS-066 Profiling zones and names

- **Statement:** Every sub-job leaf batch, continuation and background slice emits a zone through the installed hooks, named `<NodeName>/[<begin>,<end>)`, `<NodeName>/cont<n>` and `bg/<Name>` from the launch's zone site, with no name formatting when no zone hook is installed.
- **Covers:** Z2-I-JOBS-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_JobsProfiling_ZoneNamesFormatted` and `Test_JobsProfiling_NoFormattingWithoutZoneHook` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`; `Test_FrameGraph_ZonesForEveryNode` in ZenithProfiling's tests
- **Design:** Zenith2/Docs/Design/JOBS.md → "Zones"

### Z2-R-JOBS-067 Contention rows fed by counters alone

- **Statement:** The on-screen profiler's contention rows (steal rate, steal-failure ratio, idle percentage per thread, parks, inline runs, queue high-water marks, background backlog) are computed from the JOBS counters and nothing else.
- **Covers:** Z2-I-JOBS-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** pending DEVT: `Test_Profiler_ContentionRowsFromCounters`; review of the DEVT row definitions against this requirement
- **Design:** Zenith2/Docs/Design/JOBS.md → "On-screen profiler rows"

### Z2-R-JOBS-068 Scheduler overhead counter

- **Statement:** The job system reports `jobs.overhead_ns` per iteration as the sum over threads of time spent outside job bodies and outside spinning and parking while an iteration runs.
- **Covers:** Z2-I-JOBS-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** Z2-B-JOBS-001
- **Acceptance evidence:** `Test_JobsProfiling_OverheadCounterReported` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`; the Stage 2 `Zenith_Bench_Jobs` report under `Zenith2/Docs/Research/Jobs/`
- **Design:** Zenith2/Docs/Design/JOBS.md → "Counters"; Zenith2/Docs/Design/JOBS.md → "Rows JOBS owns"

### Z2-R-JOBS-069 Background slice overruns counted

- **Statement:** A `JOB_PRIORITY_BACKGROUND_HIGH` or `JOB_PRIORITY_BACKGROUND_LOW` slice longer than 2 ms increments `jobs.bg.slice_overruns`; a `JOB_PRIORITY_BACKGROUND_TOOL` slice never does (Z2-R-JOBS-121).
- **Covers:** Z2-I-JOBS-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobSystem_Background_SliceOverrunCounted` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Background jobs and operations"; Zenith2/Docs/Design/JOBS.md → "Counters"

### Z2-R-JOBS-070 Fixed tick duration

- **Statement:** The tick duration in nanoseconds is `((1,000,000 + tickHz / 2) / tickHz) × 1000` in unsigned 64-bit integer arithmetic (10^6 / `sim.tickHz` rounded to the nearest whole microsecond) for tick rates 1 to 1,000 Hz.
- **Covers:** Z2-I-JOBS-10, Z2-I-NET-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_SimulationClock_FixedTicks` and `Test_SimulationClock_TickDurationRounding` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Simulation clock arithmetic"

### Z2-R-JOBS-071 Accumulator arithmetic

- **Statement:** `Advance` clamps the real delta to `m_uMaxFrameDeltaUs`, adds the scaled delta in nanoseconds to the accumulator and returns the whole number of ticks it holds, subtracting them, exactly as the design's steps state.
- **Covers:** Z2-I-JOBS-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_SimulationClock_FixedTicks` and `Test_SimulationClock_CatchUpBounded` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Simulation clock arithmetic"

### Z2-R-JOBS-072 Simulation reads tick time only

- **Statement:** Simulation code reads simulated time only as the tick index times the tick duration, passed by the clock to the frame loop and by the world to its systems, and ZenithJobs offers no wall-clock read to jobs.
- **Covers:** Z2-I-JOBS-10, Z2-I-NET-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** the `ForbiddenCheck` step (F14) and the determinism suites of `Tests/Determinism/`; confirmed in review
- **Design:** Zenith2/Docs/Design/JOBS.md → "Determinism obligations"; Zenith2/Docs/Design/JOBS/PublicAPI.md → "Zenith_SimulationClock header"

### Z2-R-JOBS-073 External tick mode

- **Statement:** In `SIM_CLOCK_MODE_EXTERNAL_TICKS` the clock returns exactly the tick count it is given (the replay log's, or `sim.ticksPerFrame`), up to the catch-up limit, with alpha 0.
- **Covers:** Z2-I-JOBS-10, Z2-I-NET-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_SimulationClock_ExternalTicks_ReturnsGivenCount` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS/PublicAPI.md → "Zenith_SimulationClock header"; Zenith2/Docs/Design/JOBS/Algorithms.md → "Simulation clock arithmetic"

### Z2-R-JOBS-074 Clock desc validation

- **Statement:** `Zenith_ValidateSimulationClockDesc` returns `JOBS_ERROR_INVALID_DESC` for a tick rate outside [1, 1000], a catch-up limit outside [1, 64] or a frame-delta clamp outside [1,000, 1,000,000] µs, and the composition root stops startup with exit code 2 naming the key on that error.
- **Covers:** Z2-I-JOBS-10, Z2-I-JOBS-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** Z2-B-JOBS-002
- **Acceptance evidence:** `Test_SimulationClock_ValidateDesc_RejectsOutOfRange` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`; the composition root startup tests for exit code 2
- **Design:** Zenith2/Docs/Design/JOBS/PublicAPI.md → "Zenith_SimulationClock header"

### Z2-R-JOBS-075 Interpolation alpha

- **Statement:** After every `Advance` the interpolation alpha is the accumulator divided by the tick duration and lies in [0, 1), and in external-tick mode it is 0.
- **Covers:** Z2-I-JOBS-11
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_SimulationClock_AlphaInUnitInterval` and `Test_SimulationClock_ExternalTicks_ReturnsGivenCount` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Simulation clock arithmetic"

### Z2-R-JOBS-076 Alpha reported and handed to extraction

- **Statement:** The clock reports `sim.alpha` (alpha times 10^6) and `sim.ticks` with every `Advance`, and the alpha read in the frame prologue is the one extraction uses for that frame's interpolation.
- **Covers:** Z2-I-JOBS-11, Z2-I-JOBS-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_SimulationClock_CountersReported` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS.md → "Counters"; Zenith2/Docs/Design/JOBS/PublicAPI.md → "Zenith_SimulationClock header"

### Z2-R-JOBS-077 Pause runs zero ticks

- **Statement:** While paused `Advance` returns 0 and leaves the accumulator, and so the alpha, unchanged.
- **Covers:** Z2-I-JOBS-12
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_SimulationClock_PauseRunsZeroTicks` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Simulation clock arithmetic"

### Z2-R-JOBS-078 Unpause credits no paused time

- **Statement:** Real time that passes while paused is never added to the accumulator, so unpausing produces no catch-up burst.
- **Covers:** Z2-I-JOBS-12
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_SimulationClock_UnpauseDoesNotCreditPausedTime` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Simulation clock arithmetic"

### Z2-R-JOBS-079 Time scale range

- **Statement:** `SetTimeScale` clamps its argument to [0, 8] and turns NaN into 0 after an assert.
- **Covers:** Z2-I-JOBS-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_SimulationClock_TimeScaleClamped` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`; `Test_SimulationClock_TimeScaleNaN_Asserts` in the `Test` step (death-test suites, win-debug)
- **Design:** Zenith2/Docs/Design/JOBS/PublicAPI.md → "Zenith_SimulationClock header"; Zenith2/Docs/Design/JOBS/Algorithms.md → "Simulation clock arithmetic"

### Z2-R-JOBS-080 Time scale applied to the accumulator

- **Statement:** A time scale s adds s times the clamped real delta to the accumulator (exactly the delta for s = 1), so a scale of 0.5 halves the tick rate.
- **Covers:** Z2-I-JOBS-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_SimulationClock_TimeScale` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Simulation clock arithmetic"

### Z2-R-JOBS-081 Catch-up bounded per frame

- **Statement:** When the accumulator holds more than `m_uMaxCatchUpTicks` ticks, `Advance` returns the limit, drops the excess, keeps only the remainder below one tick, and adds the dropped count to `GetDroppedTickCount()` and the `sim.dropped_ticks` counter.
- **Covers:** Z2-I-JOBS-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** Z2-B-JOBS-002
- **Acceptance evidence:** `Test_SimulationClock_CatchUpBounded` and `Test_SimulationClock_CountersReported` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Simulation clock arithmetic"

### Z2-R-JOBS-082 Catch-up limit from the budget row

- **Statement:** `m_uMaxCatchUpTicks` is filled from `sim.maxCatchUpTicks`, whose value is the budget row Z2-B-JOBS-002, and `AdvanceExternal` refuses a tick count above it.
- **Covers:** Z2-I-JOBS-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** Z2-B-JOBS-002
- **Acceptance evidence:** `Test_SimulationClock_ValidateDesc_RejectsOutOfRange` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`; the Stage 2 acceptance configuration dump showing the allocated row value
- **Design:** Zenith2/Docs/Design/JOBS/PublicAPI.md → "Zenith_SimulationClock header"; Zenith2/Docs/Design/JOBS.md → "Rows JOBS owns"

### Z2-R-JOBS-083 Dropped time never reaches a replay

- **Statement:** A replay drives the clock in external-tick mode with the recorded tick counts, so dropped real time never changes the executed tick sequence.
- **Covers:** Z2-I-JOBS-14, Z2-I-NET-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** the determinism replay suites of `Tests/Determinism/` in the `Test` step
- **Design:** Zenith2/Docs/Design/JOBS/PublicAPI.md → "Zenith_SimulationClock header"; Zenith2/Docs/Design/JOBS.md → "Determinism obligations"

### Z2-R-JOBS-084 Nothing queued after the last iteration

- **Statement:** When the final iteration's `Zenith_JobsRunMainUntil` returns, every launch of that iteration has called its completion callback and no frame job remains on any queue, so shutdown step 4 starts from an empty frame pool.
- **Covers:** Z2-I-JOBS-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_JobNodeLaunch_CompleteCalledOnceAfterEverything` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`; the composition root shutdown tests in `Tests/Lifecycle/`
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Shutdown sequence"; Zenith2/Docs/Design/JOBS/Algorithms.md → "Completion"

### Z2-R-JOBS-085 Join order

- **Statement:** The job system destructor sets the stop flag, wakes every thread, joins the render-submit thread and then workers N down to 1.
- **Covers:** Z2-I-JOBS-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobSystem_Shutdown_JoinsAllThreads` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Shutdown sequence"

### Z2-R-JOBS-086 Destruction preconditions

- **Statement:** Destroying the job system during an iteration or with an incomplete background operation fails a `ZENITH_CHECK` rather than racing.
- **Covers:** Z2-I-JOBS-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobSystem_DestroyWithPendingBackground_Checks` in the `Test` step (death-test suites, win-debug)
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Shutdown sequence"

### Z2-R-JOBS-087 Create unwinds each step

- **Statement:** Every fallible step of `Zenith_JobSystem::Create` unwinds exactly what earlier steps built, joining started threads in descending order, leaving zero leaks, and has its own failure-injection test.
- **Covers:** Z2-I-JOBS-15, Z2-I-JOBS-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobSystem_Create_FailsAtAllocateState_Unwinds`, `Test_JobSystem_Create_FailsAtAllocateThreadStates_Unwinds`, `Test_JobSystem_Create_FailsAtAllocateDeques_Unwinds`, `Test_JobSystem_Create_FailsAtAllocateQueues_Unwinds`, `Test_JobSystem_Create_FailsAtAllocateOperationTable_Unwinds`, `Test_JobSystem_Create_FailsAtStartWorkers_Unwinds`, `Test_JobSystem_Create_FailsAtStartRenderSubmit_Unwinds`, `Test_WorkStealingDeque_Create_FailsAtAllocateSlots_Unwinds` and `Test_BoundedMpmcQueue_Create_FailsAtAllocateCells_Unwinds` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Create and failure unwinding"

### Z2-R-JOBS-088 Create and destroy under load

- **Statement:** Two hundred create, load, drain and destroy cycles join every thread each time and leave zero leaks.
- **Covers:** Z2-I-JOBS-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobSystem_Stress_CreateDestroyUnderLoad` in the `Sanitizers -Preset linux-tsan` and `Sanitizers -Preset win-asan` lines of `Zenith2Memory` and the Stage 2 acceptance sanitizer set
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Shutdown sequence"; Zenith2/Docs/Design/JOBS/TestCatalogue.md → "Stress and sanitizer tests"

### Z2-R-JOBS-089 Floating-point environment applied on every job thread

- **Statement:** Every worker and the render-submit thread call `Zenith_ApplyFloatEnvironment()` as the first statement of their entry function, and `Create` asserts that the calling main thread already holds the fixed state.
- **Covers:** Z2-I-JOBS-01, Z2-I-SCRIPT-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_FloatEnvironment_WorkerThreads_MatchMain` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Floating-point environment application"

### Z2-R-JOBS-090 Floating-point environment asserted per job

- **Statement:** Wherever asserts are enabled, every job start compares `Zenith_ReadFloatEnvironment()` with the fixed state, so a change made by earlier code on that thread fails at the next job.
- **Covers:** Z2-I-JOBS-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_FloatEnvironment_ChangedInJob_AssertsAtNextJob` in the `Test` step (death-test suites, win-debug)
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Job invocation"

### Z2-R-JOBS-091 Fixed floating-point state per target

- **Statement:** The fixed state is MXCSR 0x9FC0 compared under mask 0xFFC0 on x86-64 and FPCR 0x03000000 compared under mask 0x03C89F03 on arm64, so denormals flush to zero on every job thread.
- **Covers:** Z2-I-JOBS-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_FloatEnvironment_ExpectedValues_PerTarget`, `Test_FloatEnvironment_Denormal_FlushesToZero` and `Test_FloatEnvironment_ApplyIsIdempotent` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`; the S24 device run record for arm64
- **Design:** Zenith2/Docs/Design/JOBS/PublicAPI.md → "Zenith_FloatEnvironment header"

### Z2-R-JOBS-092 Results independent of worker count

- **Statement:** No value a job computes through ZenithJobs depends on the worker count, the thread that ran it or the dispatch order when callers merge by leaf index or node order, as the per-leaf merge test shows for 1, 2, 8 and all workers.
- **Covers:** Z2-I-JOBS-03, Z2-I-GRAPH-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_JobContext_MergeOrder_IndependentOfWorkers` and `Test_JobContext_ParallelFor_LeafPartitionIsPure` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS.md → "Determinism obligations"

### Z2-R-JOBS-093 Deterministic invocation IDs

- **Statement:** A launched node's stages and sub-jobs report the launch's invocation ID, `RunRoot` jobs an ID with bit 62 set and background jobs one with bit 63 set; no ID is 0 and IDs are used for view validation only.
- **Covers:** Z2-I-JOBS-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_JobContext_InvocationID_UniqueAndStable` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Invocation IDs"

### Z2-R-JOBS-094 Iteration memory lifetimes

- **Statement:** Frame records, node states, segments, continuations and node-arena and worker-arena allocations stay valid until the end of their iteration and are reclaimed by their owning thread before its first frame job of a subsequent iteration, never while any job of their iteration can still read them.
- **Covers:** Z2-I-JOBS-03, Z2-I-JOBS-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_JobContext_NodeArena_ValidUntilNodeCompletes` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan` and the Stage 2 acceptance sanitizer set
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Job records and the memory model"; Zenith2/Docs/Design/JOBS.md → "Job memory model"

### Z2-R-JOBS-095 Trivially destructible captures

- **Statement:** `AllocateCapture` accepts only trivially copyable, trivially destructible types, enforced by `static_assert`, and every object ZenithJobs stores in an arena is trivially destructible.
- **Covers:** Z2-I-JOBS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_JobContext_AllocateCapture_CopiesCapture` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`; the `Build` step compiles the static assertions
- **Design:** Zenith2/Docs/Design/JOBS/PublicAPI.md → "Zenith_JobContext"; Zenith2/Docs/Design/JOBS.md → "Job memory model"

### Z2-R-JOBS-096 Job data outlives the node

- **Statement:** The `pData` of every `ParallelFor` and `Continue` points at memory that outlives the node instance (node arena or registrant-owned memory), never at the calling job's stack.
- **Covers:** Z2-I-JOBS-03, Z2-I-JOBS-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** confirmed in review of every call site; `win-asan` runs of the JOBS tests catch a stack use after return
- **Design:** Zenith2/Docs/Design/JOBS.md → "Job memory model"

### Z2-R-JOBS-097 Background payload copy

- **Statement:** `SubmitBackgroundCopy` copies at most 64 bytes into the operation record and hands each slice a 16-byte-aligned pointer to its own copy, carried across slices by `Continue`.
- **Covers:** Z2-I-JOBS-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_JobSystem_SubmitBackgroundCopy_PayloadIsCopied` and `Test_JobSystem_Background_ContinueRunsSlicesInOrder` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Record types"; Zenith2/Docs/Design/JOBS/PublicAPI.md → "Zenith_JobSystem"

### Z2-R-JOBS-098 Background submission results

- **Statement:** `SubmitBackground` and `SubmitBackgroundCopy` return an operation handle on success and, having taken nothing, `JOBS_ERROR_QUEUE_FULL` (the background queue, or the operation table of `2 × m_uBackgroundCapacity + N + 2` slots, is full), `JOBS_ERROR_SHUTTING_DOWN` or `JOBS_ERROR_INVALID_DESC` otherwise.
- **Covers:** Z2-I-JOBS-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobSystem_SubmitBackground_Succeeds`, `Test_JobSystem_SubmitBackground_QueueFull_ReturnsError`, `Test_JobSystem_SubmitBackground_InvalidArguments` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`; `Test_JobSystem_BackgroundNameWithoutPrefix_Asserts` in the `Test` step (death-test suites, win-debug)
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Background jobs and operations"; Zenith2/Docs/Design/JOBS.md → "Error codes"

### Z2-R-JOBS-099 Background submission from any thread

- **Statement:** Background work may be submitted from any thread, including threads the job system does not own.
- **Covers:** Z2-I-JOBS-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobSystem_SubmitBackground_FromForeignThread` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS/PublicAPI.md → "Zenith_JobSystem"

### Z2-R-JOBS-100 Background outcome reporting

- **Statement:** A background operation completes `OPERATION_STATUS_FAILED` with its code after `ReportFailed`, `OPERATION_STATUS_CANCELLED` after `ReportCancelled`, `OPERATION_STATUS_SUCCEEDED` otherwise, and its progress is monotonic and reaches 1.0 no earlier than success.
- **Covers:** Z2-I-JOBS-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobSystem_Background_ReportFailed` and `Test_JobSystem_Background_ProgressIsMonotonic` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`; `Test_JobContext_BackgroundCallInFrameJob_Asserts` in the `Test` step (death-test suites, win-debug)
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Background jobs and operations"

### Z2-R-JOBS-101 Scheduler overhead within its row

- **Statement:** The scheduler overhead per frame, measured with MP-JOBOVERHEAD (`jobs.overhead_ns` at p95 over every WS-FRAME workload, plus `Budgets_JobsSchedulerOnly` at p95 per worker count), stays within Z2-B-JOBS-001 on the reference desktop and the S24.
- **Covers:** Z2-I-JOBS-09, Z2-I-JOBS-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-JOBS-001
- **Acceptance evidence:** the workload evidence records of the Stage 2 acceptance (`Run-Committed.ps1 -Acceptance -Stage 2`), once Z2-B-JOBS-001 is allocated
- **Design:** Zenith2/Docs/Design/JOBS.md → "Rows JOBS owns"

### Z2-R-JOBS-102 Stage 1 JOBS budget harness

- **Statement:** The ordinary harness ticket "Stage 1 per-system budget harness for JOBS" builds `Zenith2/Benchmarks/Budgets/JOBS/` with a CTest smoke run and the scheduler-only micro-workload `Budgets_JobsSchedulerOnly` ([Z2-N-BUDGET-228]: per frame for 7,200 frames, a fan-out of 1,024 empty jobs joined by one continuation and a chain of 64 dependent empty continuations, on 1, 2, 8 and all workers, reported at p95 per worker count), run on the reference engine's `Zenith/TaskSystem` in Stage 1 and on ZenithJobs from Stage 2, together with the per-frame dispatch overhead of the 200-node, 4,000-leaf synthetic frame, for the ruling that allocates Z2-B-JOBS-001 to -005.
- **Covers:** Z2-I-JOBS-09, Z2-I-JOBS-14
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-084, Z2-D-012
- **Budgets:** Z2-B-JOBS-001, Z2-B-JOBS-002, Z2-B-JOBS-003, Z2-B-JOBS-004, Z2-B-JOBS-005
- **Acceptance evidence:** the harness smoke run in the `Test` step; the ruling report under `Zenith2/Docs/Research/Budgets/`
- **Design:** Zenith2/Docs/Design/JOBS/TestCatalogue.md → "Benchmarks"; Zenith2/Docs/Design/JOBS.md → "Stage 1"

### Z2-R-JOBS-103 CPU critical-path headroom allocated first

- **Statement:** The JOBS per-system budget ruling allocates Z2-B-JOBS-003 before every other CPU row is allocated against the CPU-SIM and CPU-RENDER bounds.
- **Covers:** Z2-I-JOBS-02
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-JOBS-003
- **Acceptance evidence:** the Stage 1 per-system budget ticket for JOBS, its DecisionLog entry and `Check-Allocation.ps1` output
- **Design:** Zenith2/Docs/Design/JOBS.md → "Rows JOBS owns"

### Z2-R-JOBS-104 Thread-sanitizer stress coverage

- **Statement:** Every JOBS concurrency primitive and launch path has a `z2-stress` test run with 2, 8 and all workers that is clean under TSan and UBSan on `linux-tsan`.
- **Covers:** Z2-I-JOBS-01, Z2-I-JOBS-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** the tests of the "Stress and sanitizer tests" table under the `Sanitizers -Preset linux-tsan` and `Sanitizers -Preset win-asan` lines of `Zenith2Memory` and the Stage 2 acceptance sanitizer set
- **Design:** Zenith2/Docs/Design/JOBS/TestCatalogue.md → "Stress and sanitizer tests"; Zenith2/Docs/Design/JOBS.md → "Sanitizer and device runs"

### Z2-R-JOBS-105 Device run on the S24

- **Statement:** The ZenithJobs tests built for `android-arm64`, including the floating-point read-back and worker-count tests, pass on the Galaxy S24 Ultra in the HWASan build type, recorded as a device run record.
- **Covers:** Z2-I-JOBS-01, Z2-I-JOBS-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-010, Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** the needs-human device run record of the Stage 2 acceptance (HWASan on the S24)
- **Design:** Zenith2/Docs/Design/JOBS.md → "Sanitizer and device runs"

### Z2-R-JOBS-106 Examples compiled as tests

- **Statement:** The two JOBS usage examples cited by the contract blocks build into `ZenithJobs_Tests` and run as tests.
- **Covers:** Z2-I-JOBS-03, Z2-I-JOBS-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-047
- **Budgets:** none
- **Acceptance evidence:** the `Test` step and `ApiContractCheck` (`Example:` paths exist and compile)
- **Design:** Zenith2/Docs/Design/JOBS/TestCatalogue.md → "Examples"

### Z2-R-JOBS-107 Injection capacity from the schedule

- **Statement:** `Zenith_JobsReserveInjectionCapacity(n)` grows the injection queue to at least `2n + 256` cells, rounded up to a power of two, and reserves the last `2n + 256` of them for the render-submit thread, so no foreign load can fill them; an injection push by the render-submit thread that still finds the queue full is `ZENITH_FATAL(FATAL_CODE_QUEUE_FULL)` naming `jobs.injection`, a non-replayable fatal diagnostic.
- **Covers:** Z2-I-JOBS-02, Z2-I-JOBS-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_JobNodeLaunch_ReserveInjectionCapacity_Grows` and `Test_JobNodeLaunch_ForeignFloodNeverFatalsRenderSubmit` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`; `Test_JobNodeLaunch_RenderSubmitInjectionOverflow_Fatal` in the `Test` step (death-test suites, win-debug)
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Queues and their full policies"; Zenith2/Docs/Design/JOBS/Algorithms.md → "Node launch seam"; Zenith2/Docs/Design/JOBS.md → "Error codes"

### Z2-R-JOBS-108 Foreign-thread injection rejects when full

- **Statement:** A foreign producer (a thread with no job context: I/O, test and tool threads) launches frame work only through `Zenith_JobsTryInjectNode`, which returns `JOBS_ERROR_QUEUE_FULL` having taken nothing once the foreign share (`capacity − m_uInjectionReserved`, counted exactly by `m_iForeignQueued`) is used; a retried launch runs exactly once on a frame thread; a call from a job thread or the main thread fails `ZENITH_CHECK`.
- **Covers:** Z2-I-JOBS-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobSystem_InjectionFull_ReturnsQueueFull`, `Test_JobNodeLaunch_TryInjectNode_RunsEveryLaunch` and `Test_JobNodeLaunch_ForeignFloodNeverFatalsRenderSubmit` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`; `Test_JobNodeLaunch_TryInjectFromJobThread_Checks` and `Test_JobNodeLaunch_TryInjectFromMainThread_Checks` in the `Test` step (death-test suites, win-debug)
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Push order and routing"; Zenith2/Docs/Design/JOBS.md → "Failure handling"

### Z2-R-JOBS-109 Render-submit work does not fan out

- **Statement:** A `ParallelFor` called from a render-submit job fails `ZENITH_CHECK`, and continuations of render-submit work keep render-submit affinity, so the render-submit thread never pushes a sub-job.
- **Covers:** Z2-I-JOBS-08, Z2-I-JOBS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_JobContext_ParallelForFromRenderSubmit_Checks` in the `Test` step (death-test suites, win-debug); `Test_JobContext_Continue_AffinityInherited` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Queues and their full policies"; Zenith2/Docs/Design/JOBS/PublicAPI.md → "Zenith_JobContext"

### Z2-R-JOBS-110 Relaxed atomic scheduler counters

- **Statement:** Every per-thread scheduler counter and every queue statistic is a `std::atomic<uint64>` written by one thread with `relaxed` operations and read by the publisher with `relaxed` loads; frame-path counters are exact per iteration and counters written outside the completion chain may move an increment to the following iteration, so the `linux-tsan` run needs no suppression.
- **Covers:** Z2-I-JOBS-09, Z2-I-JOBS-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** `Test_JobNodeLaunch_CountersPublishedPerIteration` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan` and `Test_JobSystem_Stress_RandomLaunchGraphs` clean with no suppression under the `Sanitizers -Preset linux-tsan` and `Sanitizers -Preset win-asan` lines of `Zenith2Memory` and the Stage 2 acceptance sanitizer set
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Counters and their memory model"; Zenith2/Docs/Design/JOBS.md → "Counters"

### Z2-R-JOBS-111 Scheduler log category and messages

- **Statement:** JOBS logs under the category `jobs.scheduler` exactly the messages of the design (a full background queue, once per iteration per queue, as a warning; a failed thread creation as an error; a grown affinity or injection queue as info; a memory-pressure level change as info), and no other log call runs on a job's hot path.
- **Covers:** Z2-I-JOBS-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobsProfiling_LogMessagesUnderCategory` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`; confirmed in review
- **Design:** Zenith2/Docs/Design/JOBS.md → "Log category"

### Z2-R-JOBS-112 Injected nodes join the iteration

- **Statement:** `Zenith_JobsTryInjectNode` accepts a node only inside an iteration (`RunFrame` or `RunRoot`), checked by the `seq_cst` Dekker pair of `m_iInjectedInFlight` and `m_bIterationActive`, and returns `JOBS_ERROR_NO_ITERATION` having taken nothing otherwise; `Zenith_JobsEndIteration` and the end of `RunRoot` close the window and help until every accepted injected node, its continuations and its sub-jobs have completed, so no injected record or arena allocation outlives its frame epoch.
- **Covers:** Z2-I-JOBS-02, Z2-I-JOBS-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_JobNodeLaunch_TryInjectNode_OutsideIteration_Rejected` and `Test_JobNodeLaunch_InjectedNodeJoinsIteration` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan` and the Stage 2 acceptance sanitizer set
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Push order and routing"; Zenith2/Docs/Design/JOBS/Algorithms.md → "Frame epochs"; Zenith2/Docs/Design/JOBS.md → "Per-worker Luau VM affinity contract"

### Z2-R-JOBS-113 Heap-owned injected node state

- **Statement:** An injected node's node state, segment 0 and stage-0 record are one block on the job system's heap, marked `m_bHeapOwned`, freed by `CompleteNode` after the completion callback; its continuations, sub-jobs and scratch come from the executing threads' arenas.
- **Covers:** Z2-I-JOBS-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_JobNodeLaunch_TryInjectNode_RunsEveryLaunch` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan` (every heap allocation freed) and the Stage 2 acceptance sanitizer set
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Stages segments and continuations"; Zenith2/Docs/Design/JOBS/Algorithms.md → "Lifetimes"

### Z2-R-JOBS-114 Memory-governor consumer

- **Statement:** The job system exposes a `Zenith_MemoryConsumer` whose resident bytes equal `jobs.memory_bytes`, reported into `MEMORY_SUBSYSTEM_OTHER`, whose budget includes Z2-B-JOBS-004 (Architecture's memory-governor Budgets table, through cross-doc X59, pending ARCH); the row itself is measured by MP-SUBMEM; its declared BR-GOVERNOR policy is to shed nothing and log each pressure level change once.
- **Covers:** Z2-I-JOBS-09, Z2-I-JOBS-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** Z2-B-JOBS-004
- **Acceptance evidence:** `Test_JobSystem_MemoryConsumer_ReportsResidentBytes` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Create and failure unwinding"; Zenith2/Docs/Design/JOBS.md → "Rows JOBS owns"; Zenith2/Docs/Design/JOBS/PublicAPI.md → "Zenith_JobSystem"

### Z2-R-JOBS-115 Monotonic iteration frame index

- **Statement:** `Zenith_JobsBeginIteration` stores the frame index it is given; its first call after `Create` accepts any index, including the composition root's frame 0, and every subsequent call fails `ZENITH_CHECK` when the index is not greater than the previous call's.
- **Covers:** Z2-I-JOBS-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobNodeLaunch_BeginIterationFrameIndexNotIncreasing_Checks` in the `Test` step (death-test suites, win-debug)
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Node launch seam"

### Z2-R-JOBS-116 Injected work bounded and counted

- **Statement:** An injected launch is frame work of the iteration that accepted it: its body and each continuation obey the frame-job rules and are sized like a background slice (at most 2 ms each), the iteration end waits for it, and the job system reports `jobs.injection.accepted` (launches accepted this iteration) and `jobs.injection.drain_ns` (main-thread time from the closing store until the last accepted node completed), both exact, once per iteration.
- **Covers:** Z2-I-JOBS-02, Z2-I-JOBS-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobNodeLaunch_InjectedDrainBounded` and `Test_JobNodeLaunch_CountersPublishedPerIteration` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`; the 2 ms slice size of each injected job confirmed in review of every producer
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Node launch seam"; Zenith2/Docs/Design/JOBS/Algorithms.md → "Push order and routing"; Zenith2/Docs/Design/JOBS.md → "Counters"

### Z2-R-JOBS-117 Injected launch field contract

- **Statement:** For a launch passed to `Zenith_JobsTryInjectNode`, `m_puCancel` may be null (the node is never cancelled and no load of it happens), the producer's `m_ulInvocationID` is replaced by `(3ull << 62) | n` from a per-system counter, a null `m_pxZone` names the zone `injected`, the affinity must be `JOB_AFFINITY_ANY`, and a refusal with `JOBS_ERROR_NO_ITERATION` or `JOBS_ERROR_QUEUE_FULL` is retried at the producer's own next wake-up, never by spinning on the call.
- **Covers:** Z2-I-JOBS-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_JobNodeLaunch_TryInjectNode_RunsEveryLaunch` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`; the retry discipline confirmed in review of every producer
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Node launch seam"; Zenith2/Docs/Design/JOBS/Algorithms.md → "Invocation IDs"; Zenith2/Docs/Design/JOBS.md → "Error codes"

### Z2-R-JOBS-118 Tools-host priority admitted by a run-time desc field

- **Statement:** `JOB_PRIORITY_BACKGROUND_TOOL` is accepted by `SubmitBackground` and `SubmitBackgroundCopy` only in a job system created with `Zenith_JobSystemDesc::m_bAcceptToolBackground = true`, which the editor's and the cook's composition roots alone set in code; every other job system returns `JOBS_ERROR_INVALID_DESC` for it having issued and pushed nothing, logs one `jobs.scheduler` warning per job system, and no build condition, configuration key or test-mode switch selects the behaviour.
- **Covers:** Z2-I-JOBS-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_JobSystem_ToolBackground_DescDefaults`, `Test_JobSystem_ToolBackground_RefusedWithoutAcceptance` and `Test_JobSystem_ToolBackground_SubmitSucceeds` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`; `Test_JobSystem_Stress_ToolBackgroundAndFrameMix` in `z2-stress`
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Tools-host background priority"; Zenith2/Docs/Design/JOBS/PublicAPI.md → "Zenith_JobSystemDesc"; Zenith2/Docs/Design/JOBS.md → "Error codes"

### Z2-R-JOBS-119 Tools-host concurrency cap

- **Statement:** At most `m_uMaxConcurrentToolBackground` workers run `JOB_PRIORITY_BACKGROUND_TOOL` slices at once, each also counted against `m_uMaxConcurrentBackground`; with acceptance the cap defaults to `min(max(1, ⌊N / 2⌋), resolved m_uMaxConcurrentBackground)` and an explicit value outside `[1, that limit]` is `JOBS_ERROR_INVALID_DESC`; without acceptance a non-zero cap is `JOBS_ERROR_INVALID_DESC` and `GetMaxConcurrentToolBackground()` returns 0.
- **Covers:** Z2-I-JOBS-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobSystem_ToolBackground_DefaultCapIsHalfTheWorkers`, `Test_JobSystem_ToolBackground_InvalidDesc_IsRejected` and `Test_JobSystem_ToolBackground_ConcurrencyCap` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`; `Test_JobSystem_Stress_ToolBackgroundAndFrameMix` in `z2-stress`
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Tools-host background priority"; Zenith2/Docs/Design/JOBS/Algorithms.md → "Worker loop"; Zenith2/Docs/Design/JOBS/Algorithms.md → "Create and failure unwinding"

### Z2-R-JOBS-120 Tools-host queue, operation table and pop order

- **Statement:** A job system that accepts the tools-host priority creates a third REJECT background queue `jobs.background_tool` of `m_uToolBackgroundCapacity` cells (default 1,024, a power of two in [64, 2^22]) and an operation table of `2 × m_uBackgroundCapacity + m_uToolBackgroundCapacity + N + 2` slots; one that does not creates neither the queue nor the extra slots; a worker that passed the background gate takes its held continuation, then the high queue, then the tool queue behind the tools-host gate, then the low queue, and a held tool continuation waits for a tools-host slot without the worker taking any other background record.
- **Covers:** Z2-I-JOBS-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobSystem_ToolBackground_PopOrderHighToolLow`, `Test_JobSystem_ToolBackground_HighRunsBesideToolJobs`, `Test_JobSystem_ToolBackground_HeldContinuationRuns` and `Test_JobSystem_ToolBackground_QueueFullAndDrain` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`; `Test_JobSystem_Stress_ToolBackgroundAndFrameMix` in `z2-stress`
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Tools-host background priority"; Zenith2/Docs/Design/JOBS/Algorithms.md → "Queues and their full policies"; Zenith2/Docs/Design/JOBS/Algorithms.md → "Worker loop"

### Z2-R-JOBS-121 Tools-host slices outside the overrun count, with their own counters

- **Statement:** A `JOB_PRIORITY_BACKGROUND_TOOL` slice never increments `jobs.bg.slice_overruns`, whatever its length, and a job system that accepts the priority publishes `jobs.background_tool.full`, `jobs.background_tool.dropped` (0), `jobs.background_tool.high_water` and the gauge `jobs.bg.tool_running` once per iteration, while one that does not publishes none of the four, so a Player's counter list is unchanged.
- **Covers:** Z2-I-JOBS-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_JobSystem_ToolBackground_LongSliceNotAnOverrun` and `Test_JobSystem_ToolBackground_CountersPublished` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`; `Test_JobSystem_Stress_ToolBackgroundAndFrameMix` in `z2-stress`
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Background jobs and operations"; Zenith2/Docs/Design/JOBS.md → "Counters"

### Z2-R-JOBS-122 Tools-host drain, cancellation and accounting

- **Statement:** `CancelAndDrainBackground` pops the tool queue on the main thread without the background or tools-host gate, so every queued tool operation completes `OPERATION_STATUS_CANCELLED` without running; a running tool job stops at its next `IsCancelled()` check; and `jobs.memory_bytes` counts the operation table's 64-byte slots and its ticket array in every job system and, in a tools-host one, the tool queue's 192-byte cells and its extra table slots.
- **Covers:** Z2-I-JOBS-07, Z2-I-JOBS-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-040
- **Budgets:** Z2-B-JOBS-004
- **Acceptance evidence:** `Test_JobSystem_ToolBackground_QueueFullAndDrain`, `Test_JobSystem_ToolBackground_CancelWhileRunning` and `Test_JobSystem_ToolBackground_MemoryBytesIncludesToolStorage` in the `Test` step (win-debug, win-release) and the `Zenith2Memory` lines `Test -Label memory`, `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan`; `Test_JobSystem_Stress_ToolBackgroundAndFrameMix` in `z2-stress`
- **Design:** Zenith2/Docs/Design/JOBS/Algorithms.md → "Tools-host background priority"; Zenith2/Docs/Design/JOBS/Algorithms.md → "Shutdown sequence"; Zenith2/Docs/Design/JOBS.md → "In-memory layouts"

## Coverage of inventory items

| Inventory item | Item | Requirements |
|---|---|---|
| Z2-I-JOBS-01 | a work-stealing scheduler on `Zenith_Multithreading` | Z2-R-JOBS-001, Z2-R-JOBS-002, Z2-R-JOBS-003, Z2-R-JOBS-004, Z2-R-JOBS-005, Z2-R-JOBS-006, Z2-R-JOBS-007, Z2-R-JOBS-008, Z2-R-JOBS-009, Z2-R-JOBS-010, Z2-R-JOBS-011, Z2-R-JOBS-012, Z2-R-JOBS-013, Z2-R-JOBS-014, Z2-R-JOBS-015, Z2-R-JOBS-016, Z2-R-JOBS-017, Z2-R-JOBS-018, Z2-R-JOBS-019, Z2-R-JOBS-087, Z2-R-JOBS-089, Z2-R-JOBS-090, Z2-R-JOBS-091, Z2-R-JOBS-101, Z2-R-JOBS-104, Z2-R-JOBS-105, Z2-R-JOBS-114, Z2-R-JOBS-118, Z2-R-JOBS-119, Z2-R-JOBS-120 |
| Z2-I-JOBS-02 | scheduler driven by the GRAPH schedule | Z2-R-JOBS-020, Z2-R-JOBS-021, Z2-R-JOBS-022, Z2-R-JOBS-023, Z2-R-JOBS-024, Z2-R-JOBS-025, Z2-R-JOBS-026, Z2-R-JOBS-027, Z2-R-JOBS-028, Z2-R-JOBS-093, Z2-R-JOBS-103, Z2-R-JOBS-107, Z2-R-JOBS-108, Z2-R-JOBS-112, Z2-R-JOBS-115, Z2-R-JOBS-116, Z2-R-JOBS-117 |
| Z2-I-JOBS-03 | dynamic parallel-for sub-jobs | Z2-R-JOBS-021, Z2-R-JOBS-029, Z2-R-JOBS-030, Z2-R-JOBS-031, Z2-R-JOBS-032, Z2-R-JOBS-033, Z2-R-JOBS-034, Z2-R-JOBS-092, Z2-R-JOBS-094, Z2-R-JOBS-095, Z2-R-JOBS-096, Z2-R-JOBS-106, Z2-R-JOBS-109 |
| Z2-I-JOBS-04 | continuations | Z2-R-JOBS-021, Z2-R-JOBS-035, Z2-R-JOBS-036, Z2-R-JOBS-037, Z2-R-JOBS-038, Z2-R-JOBS-039, Z2-R-JOBS-040, Z2-R-JOBS-094, Z2-R-JOBS-096, Z2-R-JOBS-112, Z2-R-JOBS-113 |
| Z2-I-JOBS-05 | no fibers | Z2-R-JOBS-041, Z2-R-JOBS-042, Z2-R-JOBS-043, Z2-R-JOBS-044 |
| Z2-I-JOBS-06 | no blocking waits | Z2-R-JOBS-015, Z2-R-JOBS-019, Z2-R-JOBS-033, Z2-R-JOBS-045, Z2-R-JOBS-046, Z2-R-JOBS-047, Z2-R-JOBS-104, Z2-R-JOBS-110 |
| Z2-I-JOBS-07 | cancellation | Z2-R-JOBS-040, Z2-R-JOBS-048, Z2-R-JOBS-049, Z2-R-JOBS-050, Z2-R-JOBS-051, Z2-R-JOBS-052, Z2-R-JOBS-053, Z2-R-JOBS-054, Z2-R-JOBS-055, Z2-R-JOBS-056, Z2-R-JOBS-057, Z2-R-JOBS-097, Z2-R-JOBS-098, Z2-R-JOBS-099, Z2-R-JOBS-100, Z2-R-JOBS-106, Z2-R-JOBS-122 |
| Z2-I-JOBS-08 | thread affinity | Z2-R-JOBS-007, Z2-R-JOBS-017, Z2-R-JOBS-023, Z2-R-JOBS-025, Z2-R-JOBS-027, Z2-R-JOBS-038, Z2-R-JOBS-042, Z2-R-JOBS-058, Z2-R-JOBS-059, Z2-R-JOBS-060, Z2-R-JOBS-061, Z2-R-JOBS-062, Z2-R-JOBS-063, Z2-R-JOBS-105, Z2-R-JOBS-107, Z2-R-JOBS-109 |
| Z2-I-JOBS-09 | contention diagnostics | Z2-R-JOBS-014, Z2-R-JOBS-026, Z2-R-JOBS-062, Z2-R-JOBS-064, Z2-R-JOBS-065, Z2-R-JOBS-066, Z2-R-JOBS-067, Z2-R-JOBS-068, Z2-R-JOBS-069, Z2-R-JOBS-076, Z2-R-JOBS-101, Z2-R-JOBS-102, Z2-R-JOBS-110, Z2-R-JOBS-111, Z2-R-JOBS-114, Z2-R-JOBS-116, Z2-R-JOBS-121 |
| Z2-I-JOBS-10 | fixed simulation steps | Z2-R-JOBS-070, Z2-R-JOBS-071, Z2-R-JOBS-072, Z2-R-JOBS-073, Z2-R-JOBS-074 |
| Z2-I-JOBS-11 | interpolation | Z2-R-JOBS-075, Z2-R-JOBS-076 |
| Z2-I-JOBS-12 | pause | Z2-R-JOBS-077, Z2-R-JOBS-078 |
| Z2-I-JOBS-13 | time scaling | Z2-R-JOBS-079, Z2-R-JOBS-080 |
| Z2-I-JOBS-14 | bounded catch-up | Z2-R-JOBS-074, Z2-R-JOBS-081, Z2-R-JOBS-082, Z2-R-JOBS-083, Z2-R-JOBS-102 |
| Z2-I-JOBS-15 | orderly shutdown | Z2-R-JOBS-057, Z2-R-JOBS-063, Z2-R-JOBS-084, Z2-R-JOBS-085, Z2-R-JOBS-086, Z2-R-JOBS-087, Z2-R-JOBS-088, Z2-R-JOBS-122 |

Requirements that also serve another area's item name it in their Covers line and remain owned by JOBS: Z2-I-NET-03 (fixed ticks, shared with the JOBS fixed steps, owned by NET), Z2-I-SCRIPT-16 (per-worker VMs, owned by SCRIPT; JOBS owns the worker-index, nesting, quiescence and floating-point contracts those VMs rely on) and Z2-I-GRAPH-13 (deterministic ordering, owned by GRAPH; JOBS owns the worker-count independence of its own mechanisms).

## Excluded and deferred

No JOBS inventory item is excluded, deferred or game-side: all fifteen are required in Stage 2, and the two Stage 1 requirements serve the Stage 1 budget allocation. Out of JOBS's scope, with the owner named: fibers and any blocking job-level wait, excluded by Z2-D-006 itself (Z2-R-JOBS-041, Z2-R-JOBS-046 and Z2-R-JOBS-047 prove their absence); frame-graph construction, hazard analysis, schedule compilation, caching and recompile triggers (GRAPH); system and phase declarations (ECS, GAME for the phase contract Z2-D-080); render extraction contents and interpolation maths applied to transforms (RPROD, SCENE); the audio device thread and I/O threads (AUDIO, PLAT); per-world pause (GAME world lifecycle); NUMA placement and processor groups beyond group 0 on Windows, which no Z2-D-010 platform needs.
