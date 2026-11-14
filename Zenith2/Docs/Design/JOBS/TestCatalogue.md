# JOBS design — test catalogue

This file is part of the JOBS design family. The entry document is `Zenith2/Docs/Design/JOBS.md`; this file names every JOBS test, the case it asserts, its file, its doctest suite and the gate steps that run it. Test IDs follow `Zenith2/Docs/Contracts.md → "Tests and code hygiene gates"`: the test ID `Test_<Type>_<Behaviour>` maps one to one to the doctest name `"ZenithJobs/<Type>/<Behaviour>"`. Tests whose names come from `Zenith2/Docs/Architecture/CPUFrameGraph.md → "Required tests"` or `Zenith2/Docs/Contracts.md → "Threading and concurrency contract"` keep those names and are marked (core) in the tables.

Every test here uses `ZenithTestSupport`'s main and its leak check, prints the seed of any randomness it uses, and carries the CTest label `memory`, so the `Zenith2Memory` category's `Test -Label memory` line re-runs it and its `Sanitizers` lines run it under `win-asan` and `linux-tsan` (`Zenith2/Docs/Roadmap.md → "Stage 2 Foundation"`). No JOBS test needs GRAPH's builder or executor: a test drives frame work through `Zenith_JobSystem::RunRoot`, and launches nodes the way GRAPH's executor does through the internal seam `Internal/Jobs/Zenith_JobNodeLaunch.h` (`Zenith2/Docs/Design/JOBS/Algorithms.md → "Node launch seam"`), which the library's own test target may include. The executor's tests are GRAPH's (`Zenith2/Docs/Design/GRAPH.md → "Test strategy"`).

## Test files

| File (under `Zenith2/Engine/ZenithJobs/Tests/`) | Holds |
|---|---|
| `Zenith_JobSystem_Tests.cpp` | "JobSystem tests" |
| `Zenith_JobContext_Tests.cpp` | "JobContext tests" |
| `Zenith_WorkStealingDeque_Tests.cpp`, `Zenith_BoundedMpmcQueue_Tests.cpp` | "Deque and queue tests" (they include the library's `Internal/` headers, which the library's own test target may) |
| `Zenith_JobNodeLaunch_Tests.cpp` | "Node launch seam tests" |
| `Zenith_SimulationClock_Tests.cpp` | "SimulationClock tests" |
| `Zenith_FloatEnvironment_Tests.cpp` | "FloatEnvironment tests" |
| `Zenith_JobsProfiling_Tests.cpp` | "Profiling tests" |
| `Zenith_JobsStress_Tests.cpp` | "Stress and sanitizer tests" (doctest suite `z2-stress`, 600 s CTest timeout) |
| `Zenith_JobSystem_DefectTests.cpp` | "Death and defect tests" (suites `z2-death-check`, `z2-death-assert`) |
| `Examples/Zenith_JobContext_Example.cpp`, `Examples/Zenith_JobSystem_Example.cpp` | "Examples" |

Worker counts written "1, 2, 8 and all" mean `m_uWorkerCount` = 1, 2, 8 and `Zenith_GetHardwareConcurrency()` clamped to 62; the stress thread counts "2, 8 and the host's hardware concurrency" of [Z2-N-CONTRACT-357] map to the same desc values 2, 8 and all.

## JobSystem tests

| Test | Asserts |
|---|---|
| `Test_JobSystem_WorkerCount_AutoIsCoresMinusTwo` (core) | `m_uWorkerCount = 0` yields logical processors − 2 clamped to [1, 62] |
| `Test_JobSystem_WorkerCount_ExplicitHonoured` | `m_uWorkerCount = 3` gives `GetWorkerCount() == 3`, `GetFrameThreadCount() == 4`, `GetThreadSlotCount() == 5` |
| `Test_JobSystem_InvalidDesc_IsRejected` | each of: deque capacity 100, injection capacity 32, background capacity 3, worker count 63, spin iterations 1,000,001, `m_uMaxConcurrentBackground` above N returns `JOBS_ERROR_INVALID_DESC`, and the lifecycle callback reports no thread start |
| `Test_JobSystem_DequeFull_RunsInline` (core) | with deque capacity 64 a `ParallelFor` whose splits exceed it runs the rejected leaves inline, `jobs.inline_runs` > 0, and every leaf runs exactly once |
| `Test_JobSystem_InjectionFull_ReturnsQueueFull` (core) | inside an iteration the main thread holds open (`Zenith_JobsBeginIteration` … `Zenith_JobsEndIteration`), with `m_uFaultInjectInjectionFullAt = 65`, a test-owned thread's 65th `Zenith_JobsTryInjectNode` returns `JOBS_ERROR_QUEUE_FULL` having taken nothing; the thread retries it, the retry succeeds, all 65 launches run exactly once, and none of them reports index N+1 (the render-submit thread runs no ANY job) |
| `Test_JobSystem_BackgroundYieldsToFrameWork` (core) | with N = 1, a `RunRoot` job pushed while a 2-slice background chain runs starts before the chain's second slice |
| `Test_JobSystem_NoBlockingWaitInJobs` (core) | an installed blocking-wait hook records zero calls made while a job body runs (job-body mark set) across 1,000 iterations, each launching 50 nodes with sub-jobs and continuations through the seam |
| `Test_JobSystem_Shutdown_JoinsAllThreads` (core) | the lifecycle callback records the stop of render-submit first, then workers N, N−1, …, 1 |
| `Test_JobSystem_ThreadLifecycle_StartStopPairs` | every started slot reports exactly one start and one stop, with the names `Z2 Worker 01` … and `Z2 RenderSubmit` |
| `Test_JobSystem_MemoryConsumer_ReportsResidentBytes` | `GetMemoryConsumer()` reports `MEMORY_SUBSYSTEM_OTHER` and resident bytes equal to the published `jobs.memory_bytes`, which grows after `Zenith_JobsReserveInjectionCapacity`; a forced `MEMORY_PRESSURE_CRITICAL` logs once and changes no capacity |
| `Test_JobSystem_RenderSubmitNotStarted_Headless` | `m_bStartRenderSubmitThread = false`: `IsRenderSubmitThreadRunning()` is false, no start is reported for slot N+1, and `GetThreadSlotCount()` is still N+2 |
| `Test_JobSystem_CpuMask_JobsStillRun` | with a worker mask selecting only processor 0, and with a mask selecting processor 63 on a host without it, every job of a 10,000-leaf root runs and the lifecycle callback reports every start |
| `Test_JobSystem_ParseCpuMask_ValidAndInvalid` | `"fffc"` gives 0xFFFC; `""`, `"xyz"`, `"FFFC"` (upper case) and 17 digits return `JOBS_ERROR_INVALID_DESC` |
| `Test_JobSystem_ParkReportsNamedWait` | a capturing blocking-wait hook installed on the system sees exactly one `"jobs.park"` report per park, each with a positive wait, and none while the job-body mark is set |
| `Test_JobSystem_IdleThreadsPark` | after 100 ms with no work, `jobs.parked` ≥ N and every `jobs.idle_ns.<i>` for i in 1..N is above 0 |
| `Test_JobSystem_SchedulingPaths_TakeNoLock` | an installed lock-acquire hook records no lock acquisition during 1,000 iterations of seam launches with sub-jobs and continuations, and only the operation-table lock during background submission and completion |
| `Test_JobSystem_Create_FailsAtAllocateState_Unwinds` | heap fault injection at that step: `JOBS_ERROR_OUT_OF_MEMORY`, zero leaks, no thread start reported |
| `Test_JobSystem_Create_FailsAtAllocateThreadStates_Unwinds` | as above for that step |
| `Test_JobSystem_Create_FailsAtAllocateDeques_Unwinds` | as above; the failing deque is the third of five |
| `Test_JobSystem_Create_FailsAtAllocateQueues_Unwinds` | as above; the failing queue is the background-low queue |
| `Test_JobSystem_Create_FailsAtAllocateOperationTable_Unwinds` | as above |
| `Test_JobSystem_Create_FailsAtStartWorkers_Unwinds` | `m_uFaultInjectThreadCreate = 3`: `JOBS_ERROR_THREAD_CREATE`; workers 2 and 1 report stop, in that order; zero leaks |
| `Test_JobSystem_Create_FailsAtStartRenderSubmit_Unwinds` | fault injection at the render-submit creation: `JOBS_ERROR_THREAD_CREATE`; every worker reports stop in descending order; zero leaks |
| `Test_JobSystem_SubmitBackground_Succeeds` | the handle reaches `OPERATION_STATUS_SUCCEEDED` with progress 1.0 and the job ran once |
| `Test_JobSystem_SubmitBackground_QueueFull_ReturnsError` | `m_uFaultInjectBackgroundFullAfter = 3`: the third submission returns `JOBS_ERROR_QUEUE_FULL`, the operation table's live count is unchanged by it, and no job runs for it |
| `Test_JobSystem_SubmitBackground_InvalidArguments` | `JOB_PRIORITY_FRAME` and a 65-byte payload return `JOBS_ERROR_INVALID_DESC` |
| `Test_JobSystem_SubmitBackgroundCopy_PayloadIsCopied` | overwriting the caller's buffer after submission does not change what the job reads; the pointer the job receives is 16-byte aligned |
| `Test_JobSystem_SubmitBackground_FromForeignThread` | 4 test-owned threads submitting 1,000 jobs each all complete |
| `Test_JobSystem_Background_ContinueRunsSlicesInOrder` | a 5-slice chain runs its slices in order, carries its inline payload across slices, and completes once |
| `Test_JobSystem_Background_ProgressIsMonotonic` | `SetProgress(0.5)` then `SetProgress(0.2)` leaves 0.5; completion leaves 1.0 |
| `Test_JobSystem_Background_ReportFailed` | the handle reports `OPERATION_STATUS_FAILED` with error code 7 |
| `Test_JobSystem_Background_CancelBeforeStart_NothingRuns` | with `m_bFaultInjectPauseBackground`, a job cancelled while queued and then drained by `CancelAndDrainBackground` reports `OPERATION_STATUS_CANCELLED` and its function never ran |
| `Test_JobSystem_Background_CancelWhileRunning_ReleasesThenReports` | the job's slice-1 allocation is freed in the slice that observes cancellation before the handle reports `OPERATION_STATUS_CANCELLED` (a flag written before the free is set when the status is read); zero leaks |
| `Test_JobSystem_Background_SucceedsBeforeCancelObserved` | a cancel requested after the last slice returned leaves `OPERATION_STATUS_SUCCEEDED` |
| `Test_JobSystem_Background_HandleDestroyed_DetachesAndCompletes` | destroying the handle of a copy-submitted 3-slice chain cancels it; the chain completes, the table slot is freed, zero leaks |
| `Test_JobSystem_Background_HeldContinuationNotLost` | with the background queue fault-injected full after the first slice, the re-queued continuation is held and runs; the chain completes |
| `Test_JobSystem_Background_ConcurrencyCap` | N = 4, `m_uMaxConcurrentBackground = 1`: an atomic in-slice counter never exceeds 1 over 1,000 slices |
| `Test_JobSystem_Background_SliceOverrunCounted` | a slice that spins 5 ms increments `jobs.bg.slice_overruns` by 1 |
| `Test_JobSystem_CancelAndDrainBackground_CompletesAll` | 100 queued 10-slice chains are all complete when the call returns, each `CANCELLED` or `SUCCEEDED`; a subsequent submission returns `JOBS_ERROR_SHUTTING_DOWN` |
| `Test_JobSystem_ToolBackground_DescDefaults` | `Zenith_JobSystemDesc{}` has `m_bAcceptToolBackground == false`, `m_uMaxConcurrentToolBackground == 0` and `m_uToolBackgroundCapacity == uJOBS_DEFAULT_TOOL_BACKGROUND_CAPACITY == 1024`; `static_assert(JOB_PRIORITY_BACKGROUND_TOOL == 3)` |
| `Test_JobSystem_ToolBackground_DefaultCapIsHalfTheWorkers` | with `m_bAcceptToolBackground` and the cap left 0, `GetMaxConcurrentToolBackground()` is 1, 1, 1, 2, 3, 4, 31 for N = 1, 2, 3, 4, 6, 8, 62; N = 8 with `m_uMaxConcurrentBackground = 2` gives 2 and with 1 gives 1; without acceptance it is 0 for every N |
| `Test_JobSystem_ToolBackground_InvalidDesc_IsRejected` | each of: no acceptance with cap 1; acceptance, N = 4, cap 3; acceptance, N = 1, cap 2; acceptance, N = 8, background cap 2, cap 3; acceptance with capacity 100, 32 and 2^23 returns `JOBS_ERROR_INVALID_DESC` and reports no thread start; acceptance, N = 4, cap 2 creates (cap 2); no acceptance with capacity 100 creates (the capacity is ignored) |
| `Test_JobSystem_ToolBackground_RefusedWithoutAcceptance` | a default-desc system (N = 2): `SubmitBackground` and `SubmitBackgroundCopy` (8-byte payload) at `JOB_PRIORITY_BACKGROUND_TOOL` each return `JOBS_ERROR_INVALID_DESC`, the operation table's live count stays 0, neither function runs, and a capturing sink sees exactly one `jobs: priority JOB_PRIORITY_BACKGROUND_TOOL refused` warning under `jobs.scheduler` for the two |
| `Test_JobSystem_ToolBackground_SubmitSucceeds` | an accepting system (N = 2): a TOOL job runs once, reads `GetPriority() == JOB_PRIORITY_BACKGROUND_TOOL`, `IsBackground()` true and an invocation ID with bit 63 set; its handle reaches `OPERATION_STATUS_SUCCEEDED` with progress 1.0 |
| `Test_JobSystem_ToolBackground_ConcurrencyCap` | an accepting system, N = 4 (tools-host cap 2, background cap 3): 8 TOOL jobs that each spin 20 ms on `Zenith_GetMonotonicTimeNs` raise an atomic in-slice counter whose maximum is exactly 2 (never above, reached at least once); all 8 succeed |
| `Test_JobSystem_ToolBackground_LongSliceNotAnOverrun` | an accepting system, N = 2: a TOOL job spinning 5 ms, then, after its handle completed, a LOW job spinning 5 ms: the slice-overrun count (read as `Test_JobSystem_Background_SliceOverrunCounted` reads it) rises by exactly 1, after the LOW job alone |
| `Test_JobSystem_ToolBackground_PopOrderHighToolLow` | an accepting system, N = 1 (background cap 1, tools-host cap 1): HIGH job h0 sets a started flag and spins 50 ms; once the test thread sees the flag it submits LOW a, TOOL b and HIGH c in that order; the recorded run order is h0, c, b, a |
| `Test_JobSystem_ToolBackground_HighRunsBesideToolJobs` | an accepting system, N = 3 (background cap 2, tools-host cap 1): TOOL t1 spins until an atomic flag is set or 2 s pass; TOOL t2 is submitted next, then HIGH h, which sets the flag; t1 sees the flag before its 2 s limit (h ran beside it), and t2's start time is not before t1's end time (the cap held) |
| `Test_JobSystem_ToolBackground_HeldContinuationRuns` | an accepting system, N = 2, `m_uFaultInjectBackgroundFullAfter = 2`: a 3-slice TOOL chain whose two re-queues are reported full is held twice and runs slices 1, 2, 3 in order on one worker; its handle reaches `OPERATION_STATUS_SUCCEEDED` once |
| `Test_JobSystem_ToolBackground_QueueFullAndDrain` | an accepting system, N = 2, background and tool capacities 64, `m_bFaultInjectPauseBackground`: 64 TOOL submissions succeed and the 65th returns `JOBS_ERROR_QUEUE_FULL` with the live count unchanged at 64; 64 HIGH and 64 LOW then succeed (192 live; the table has 2 × 64 + 64 + 2 + 2 = 196 slots); `CancelAndDrainBackground` returns with all 192 `OPERATION_STATUS_CANCELLED` and no job function run |
| `Test_JobSystem_ToolBackground_CancelWhileRunning` | an accepting system, N = 2 (tools-host cap 1): TOOL job A does 1 ms work units with `IsCancelled()` between units (at most 10,000); TOOL job B is queued behind it; after A reports 5 units the test cancels A and then B: A runs at most one more unit, calls `ReportCancelled` and its handle is `CANCELLED` within 100 ms of the request; B completes `CANCELLED` without its function running |
| `Test_JobSystem_ToolBackground_CountersPublished` | an accepting system, N = 2, `m_bFaultInjectPauseBackground`: after 3 TOOL submissions one `RunRoot` publishes `jobs.background_tool.high_water` 3, `.full` 0, `.dropped` 0 and `jobs.bg.tool_running` 0; a second accepting system without the pause, whose single 200 ms TOOL job has reported started, publishes `jobs.bg.tool_running` 1 from a `RunRoot` made during the job; a non-accepting system publishes none of the four names |
| `Test_JobSystem_ToolBackground_MemoryBytesIncludesToolStorage` | right after `Create` with N = 2 and the default desc except `m_uToolBackgroundCapacity = 128`, an accepting system's `jobs.memory_bytes` exceeds a non-accepting one's by exactly 128 × 192 + 128 × (64 + `sizeof(Zenith_OperationTicket)`) bytes, and the non-accepting one's includes (2 × 8,192 + 2 + 2) × (64 + `sizeof(Zenith_OperationTicket)`) bytes for the operation table and ticket array |
| `Test_JobSystem_RunRoot_JoinsEverything` | a root with two levels of `ParallelFor` and three continuations has run every leaf and continuation when `RunRoot` returns |

The fault injectors these tests use are the five runtime fields of `Zenith_JobSystemDesc` (`Zenith2/Docs/Design/JOBS/PublicAPI.md → "Zenith_JobSystemDesc"`): `m_uFaultInjectThreadCreate` (0 = off; K = the K-th thread creation fails), `m_uFaultInjectBackgroundFullAfter` (0 = off; K = the K-th and every following background push is reported full), `m_bFaultInjectPauseBackground` (workers take no background record; only `CancelAndDrainBackground` on the main thread does), `m_uFaultInjectInjectionFullAt` (0 = off; K = the K-th injection push alone is reported full — a foreign push then returns `JOBS_ERROR_QUEUE_FULL`, a render-submit push is fatal) and `m_bFaultInjectNestedRootFull` (a `ParallelFor` called from a range body finds its root push rejected, so its leaves run inline).

## JobContext tests

| Test | Asserts |
|---|---|
| `Test_JobContext_ParallelFor_JoinsBeforeSuccessor` (core) | the launch's completion callback (the point at which GRAPH releases successors) observes every leaf's write for counts 0, 1, grain, grain + 1 and 100,000 |
| `Test_JobContext_ParallelFor_NestedJoins` (core) | leaves that call `ParallelFor` three levels deep are all joined before the completion callback |
| `Test_JobContext_Continue_RunsAfterPriorSubJobs` (core) | a continuation observes every leaf spawned before `Continue`; a leaf spawned after it (held running by a test flag) is still running when the continuation starts |
| `Test_JobContext_Continue_ChainsInOrder` (core) | three continuations run in registration order, each after the previous returned |
| `Test_JobContext_MergeOrder_IndependentOfWorkers` (core) | per-leaf results merged by leaf index yield identical bytes with 1, 2, 8 and all workers |
| `Test_JobContext_ParallelFor_LeafPartitionIsPure` | the recorded (begin, end) set equals `Zenith_ParallelForLeaf` for counts {0, 1, 7, 64, 1000, 100000} × grains {1, 3, 64, 1000} × workers {1, 2, 8, all} |
| `Test_JobContext_ParallelFor_InlineKeepsLeaves` | with deque capacity 64 and `jobs.inline_runs` > 0, the leaf set is identical to the uncongested run |
| `Test_JobContext_ParallelFor_NestingOnlyInline` | a per-thread depth counter incremented inside the body: with the default deque capacity and no fault injection, it never exceeds 1 for leaves reached by splitting or stealing; with `m_bFaultInjectNestedRootFull` making a `ParallelFor` called from a range body find its root push rejected, the nested leaves run at depth 2 on the caller's slot (`GetWorkerIndex()` unchanged), and on both runs the set of `(begin, end)` leaf ranges equals `Zenith_ParallelForLeaf` over (count, grain) |
| `Test_JobContext_Continue_NodeCompletesAfterLastSegment` | the completion callback runs only after leaves spawned after the last `Continue` have finished |
| `Test_JobContext_Continue_AffinityInherited` | a continuation of a `JOB_AFFINITY_MAIN` launch runs with worker index 0; of a render-submit launch with index N+1 |
| `Test_JobContext_WorkerIndex_StableWithinJob` | across 100,000 leaves the index read at body start equals the index at body end, and lies in [0, N] |
| `Test_JobContext_AnyWorkNeverOnRenderSubmit` | no ANY-affinity launch, leaf or continuation reports index N+1 over 1,000 iterations, including ANY launches made ready by render-submit launches (pushed through the injection queue) |
| `Test_JobContext_NodeArena_ValidUntilNodeCompletes` | data a body writes to its node arena is read intact by its continuation after 1,000 leaves on other threads; over 1,000 iterations the arena high-water stays constant |
| `Test_JobContext_AllocateCapture_CopiesCapture` | the returned capture equals the source, is aligned to `alignof`, and lives in the node arena |
| `Test_JobContext_InvocationID_UniqueAndStable` | a launched node's stages and sub-jobs report its launch's ID; `RunRoot` jobs report bit 62 set; background jobs report bit 63 set; no job reports 0 |
| `Test_JobContext_CancellationToken_ReflectsCancel` | a token obtained in a launched node reads false, then true after the launch's cancel flag is set; a background token follows its handle's `RequestCancel`; a default token reads false |

## Deque and queue tests

| Test | Asserts |
|---|---|
| `Test_WorkStealingDeque_MatchesOracle` | single-threaded push/pop/steal sequences (16 seeds × 10,000 operations) match a `std::deque` oracle after every operation |
| `Test_WorkStealingDeque_Properties` | size bookkeeping, LIFO pop and FIFO steal order, and no record returned twice, over generated inputs |
| `Test_WorkStealingDeque_FullRejects_LeavesUnchanged` | push at capacity returns false, counts one full event, and leaves contents and size unchanged |
| `Test_WorkStealingDeque_ConcurrentStealEveryElementOnce` | one owner pushing and popping 10^6 records with 2, 8 and all thieves: every record is taken exactly once |
| `Test_WorkStealingDeque_LastElementRace` | 10^6 rounds of owner `Pop` against one thief `Steal` on a one-element deque: exactly one side gets the record each round |
| `Test_WorkStealingDeque_SlotReuseAcrossWrap` | capacity 64, 10^7 push/steal cycles: no loss or duplication while slots wrap |
| `Test_WorkStealingDeque_Create_FailsAtAllocateSlots_Unwinds` | heap fault: `JOBS_ERROR_OUT_OF_MEMORY`, zero leaks |
| `Test_BoundedMpmcQueue_MatchesOracle` | single-threaded sequences match a `std::deque` oracle (16 seeds × 10,000 operations) |
| `Test_BoundedMpmcQueue_Properties` | FIFO order, size bookkeeping and no value returned twice |
| `Test_BoundedMpmcQueue_FullRejects_CountsFullEvent` | the REJECT policy returns false at capacity and counts one full event per rejection |
| `Test_BoundedMpmcQueue_EveryValueOnce` | 2, 8 and all producers and consumers moving 10^6 values: each value is popped exactly once |
| `Test_BoundedMpmcQueue_HighWaterTracked` | the high water equals the largest occupancy reached, and `TakeHighWater` resets it |
| `Test_BoundedMpmcQueue_Create_FailsAtAllocateCells_Unwinds` | heap fault: `JOBS_ERROR_OUT_OF_MEMORY`, zero leaks |

## Node launch seam tests

| Test | Asserts |
|---|---|
| `Test_JobNodeLaunch_CompleteCalledOnceAfterEverything` | for 10,000 launches with sub-jobs and continuations, each completion callback runs exactly once, after the body, every sub-job and every continuation, and observes their writes |
| `Test_JobNodeLaunch_TakeOrderFollowsSpan` | five ANY launches pushed from a worker are taken by that worker in span order (the first as handoff, the rest from the LIFO deque); five main launches are taken by the main thread in span order |
| `Test_JobNodeLaunch_AffinityRouting` | main launches run with index 0, render-submit launches with N+1, ANY launches with an index in [0, N] |
| `Test_JobNodeLaunch_CancelledBeforeStart_SkipsBody` | a launch whose cancel flag is set before it starts runs no body and calls its completion callback once |
| `Test_JobNodeLaunch_CancelledWhileRunning_ContinuationsStillRun` | after the flag is set mid-body, unstarted leaves are skipped, the registered continuation runs with `IsCancelled()` true, and the callback runs once |
| `Test_JobNodeLaunch_TryInjectNode_RunsEveryLaunch` | over 100 iterations, 10,000 launches injected from two test-owned threads through `Zenith_JobsTryInjectNode` (retrying on `JOBS_ERROR_NO_ITERATION` and `JOBS_ERROR_QUEUE_FULL`), a third of them with a null `m_puCancel`, a third with a null `m_pxZone` (zone named `injected`) and all with a producer-chosen `m_ulInvocationID` of 0, each run exactly once, report an invocation ID with bits 62 and 63 both set, unique over the run, on a frame thread (index in [0, N]) and call their completion callback once; the heap allocations the calls made are all freed |
| `Test_JobNodeLaunch_NullPusherOnMainThread_UsesSlot0` | inside an iteration, launches the main thread pushes with `pxPusher == nullptr` go to slot 0's deque (observed through `jobs.deque.high_water` of slot 0) and all run |
| `Test_JobNodeLaunch_RunMainUntilReturnsWhenDone` | `Zenith_JobsRunMainUntil` returns after a worker sets the flag and calls `Zenith_JobsWakeMain`, and not before |
| `Test_JobNodeLaunch_ReserveAffinityCapacity_Grows` | reserving 500 main nodes grows the main-affinity queue to 2,048 (1,256 rounded up to a power of two) and lets 1,256 main launches be queued without a fatal full event |
| `Test_JobNodeLaunch_ReserveInjectionCapacity_Grows` | with `m_uInjectionCapacity = 64`, reserving 1,000 injected nodes grows the injection queue to 4,096 (2,256 rounded up) with 2,256 cells reserved; a render-submit launch then makes 1,000 ANY launches ready in one push without a fatal full event, and every one runs on a frame thread |
| `Test_JobNodeLaunch_ForeignFloodNeverFatalsRenderSubmit` | with 1,000 injected nodes reserved and every frame thread occupied by a node whose leaves spin on an atomic gate (`Zenith_CpuRelax`, no blocking primitive) so nothing pops the injection queue, two test threads inject until each gets `JOBS_ERROR_QUEUE_FULL` (the foreign share, 4,096 − 2,256 = 1,840 launches, is full) while a render-submit launch makes 1,000 ANY launches ready; no fatal, and after the gate opens every foreign and frame launch runs exactly once |
| `Test_JobNodeLaunch_TryInjectNode_OutsideIteration_Rejected` | between `Zenith_JobsEndIteration` and the next `Zenith_JobsBeginIteration`, `Zenith_JobsTryInjectNode` returns `JOBS_ERROR_NO_ITERATION`, allocates nothing and leaves `m_iInjectedInFlight` at 0; the same call succeeds inside the next iteration |
| `Test_JobNodeLaunch_InjectedDrainBounded` | a node injected just before `Zenith_JobsEndIteration`, whose continuation spins 1 ms on `Zenith_GetMonotonicTimeNs`: the iteration completes after it, `jobs.injection.drain_ns` ≥ 1,000,000 and `jobs.injection.accepted` = 1 for that iteration, and both counters are 0 in an iteration with no injection |
| `Test_JobNodeLaunch_InjectedNodeJoinsIteration` | 1,000 iterations, each with a node injected mid-iteration from a test thread whose body calls `Continue` and `ParallelFor` over node-arena captures: the continuation, every leaf and the completion callback finish before `Zenith_JobsEndIteration` returns, and no frame job runs between iterations (a probe counter stays unchanged there); clean under `win-asan` |
| `Test_JobNodeLaunch_BeginIterationReclaimsArenas` | over 1,000 begin–end iterations of identical launches, the record and node arenas' high water stays constant |
| `Test_JobNodeLaunch_CountersPublishedPerIteration` | every counter name of `Zenith2/Docs/Design/JOBS.md → "Counters"`, including `jobs.injection.accepted` and `jobs.injection.drain_ns`, is reported once per `Zenith_JobsEndIteration` over 100 iterations; a system that does not accept `JOB_PRIORITY_BACKGROUND_TOOL` reports none of `jobs.background_tool.*` and `jobs.bg.tool_running`, and an accepting one reports those four as well |

## SimulationClock tests

| Test | Asserts |
|---|---|
| `Test_SimulationClock_FixedTicks` (core) | at 60 Hz, `GetTickDurationUs()` is 16,667, and 60 frames with a real delta of 16,667 µs each yield exactly one tick each, 60 in total |
| `Test_SimulationClock_CatchUpBounded` (core) | with `m_uMaxCatchUpTicks = 4` and `m_uMaxFrameDeltaUs = 250000`, one frame of 1,000,000 µs gives 4 ticks, 10 dropped, `GetDroppedTickCount()` 10 and an accumulator of 16,662 µs |
| `Test_SimulationClock_PauseRunsZeroTicks` (core) | paused frames return 0 ticks and keep alpha constant |
| `Test_SimulationClock_TimeScale` (core) | a 0.5 scale halves the tick rate over 10 s of real time (300 ± 1 ticks at 60 Hz) |
| `Test_SimulationClock_TickDurationRounding` | 60 Hz → 16,667 µs; 144 Hz → 6,944 µs; 1000 Hz → 1,000 µs; 7 Hz → 142,857 µs |
| `Test_SimulationClock_AlphaInUnitInterval` | alpha lies in [0, 1) after every `Advance` of 10^5 random deltas (seeded) |
| `Test_SimulationClock_UnpauseDoesNotCreditPausedTime` | 1 s paused then unpaused yields the same tick count as no pause over the unpaused frames |
| `Test_SimulationClock_TimeScaleClamped` | −1 sets 0, 100 sets 8 |
| `Test_SimulationClock_ExternalTicks_ReturnsGivenCount` | `AdvanceExternal(3)` returns 3, advances `GetTickIndex()` by 3, and alpha is 0 |
| `Test_SimulationClock_ValidateDesc_RejectsOutOfRange` | tick rate 0 and 1001, catch-up 0 and 65, frame delta 999 and 1,000,001 return `JOBS_ERROR_INVALID_DESC`; the defaults with catch-up 4 are valid |
| `Test_SimulationClock_CountersReported` | `sim.ticks`, `sim.dropped_ticks` and `sim.alpha` are reported by every `Advance` with the values returned |

## FloatEnvironment tests

| Test | Asserts |
|---|---|
| `Test_FloatEnvironment_WorkerThreads_MatchMain` (core) | with 8 workers, every worker, the render-submit thread and the main thread read back the expected state |
| `Test_FloatEnvironment_Denormal_FlushesToZero` (core) | `1e-38f * 1e-5f` computed in a job on every worker is exactly 0 |
| `Test_FloatEnvironment_ExpectedValues_PerTarget` | `Zenith_GetExpectedFloatEnvironment()` equals the table value of `Zenith2/Docs/Design/JOBS/PublicAPI.md → "Zenith_FloatEnvironment header"` for the preset being built |
| `Test_FloatEnvironment_ApplyIsIdempotent` | applying twice reads back the same control word |

## Profiling tests

| Test | Asserts |
|---|---|
| `Test_JobsProfiling_ZoneNamesFormatted` | an installed zone hook sees `<Node>/[0,64)`, `<Node>/cont1` and `bg/Test.Job` for a leaf batch and a continuation of a launch whose zone site is named `<Node>`, and for a background job |
| `Test_JobsProfiling_NoFormattingWithoutZoneHook` | with no zone hook installed, the record arena's used bytes after 1,000 leaves equal the records alone (no name storage) |
| `Test_JobsProfiling_LogMessagesUnderCategory` | a capturing log sink sees, under category `jobs.scheduler`, one warning `jobs: background queue '<name>' full` per iteration per full queue (not one per rejected push), one error for a fault-injected thread creation, and one info line per queue growth by `Zenith_JobsReserveAffinityCapacity` or `Zenith_JobsReserveInjectionCapacity`, and no other JOBS message over 100 iterations |
| `Test_JobsProfiling_OverheadCounterReported` | `jobs.overhead_ns` is reported each frame and is below the frame's wall time |

## Stress and sanitizer tests

All in suite `z2-stress`; each runs with 2, 8 and all workers and is clean under `linux-tsan` (TSan and UBSan) and `win-asan`.

| Test | Asserts |
|---|---|
| `Test_JobSystem_Stress_RandomLaunchGraphs` | 10,000 iterations of 200 seeded launches per iteration, each with random sub-job fan-out and 0–3 continuations, run clean under `linux-tsan`, every callback exactly once |
| `Test_JobSystem_Stress_ParallelForStorm` | 10,000 roots each nesting `ParallelFor` and `Continue` three levels deep complete with every leaf counted once |
| `Test_JobSystem_Stress_ParkWakeNoLostWakeups` | 10^6 single-job pushes timed to land while threads park all run; the run never stalls beyond its timeout |
| `Test_JobSystem_Stress_BackgroundAndFrameMix` | 1,000 frames with 10,000 concurrent background chains, 10% cancelled at random slices: every handle completes, zero leaks |
| `Test_JobSystem_Stress_Background1000CancelCycles` | 1,000 issue-then-cancel cycles at randomised slices leave no allocation, no table slot and no queued record ([Z2-N-CONTRACT-339]) |
| `Test_JobSystem_Stress_CreateDestroyUnderLoad` | 200 create, load, drain and destroy cycles join every thread each time, zero leaks |
| `Test_JobSystem_Stress_ToolBackgroundAndFrameMix` | an accepting system: 1,000 `RunRoot` iterations, each a 4,096-item `ParallelFor` with 2 continuations, while 4 test threads keep `2 × GetMaxConcurrentToolBackground()` TOOL chains (3 slices of 10 ms each) in flight and submit 10,000 HIGH and LOW jobs of 0.1 ms, 10% of the tool operations cancelled at random slices (seed printed): every iteration completes, an atomic in-tool-slice counter never exceeds the cap, the slice-overrun count equals the overruns the HIGH and LOW jobs recorded themselves, every handle completes, and a closing `CancelAndDrainBackground` leaves no live operation and zero leaks |

## Death and defect tests

| Test | Suite | Asserts |
|---|---|---|
| `Test_JobSystem_BlockingWaitInJob_Checks` | `z2-death-assert` | a job body calling `Zenith_Semaphore::Wait` fails the Core check ([Z2-N-CONTRACT-315]) |
| `Test_JobSystem_DestroyWithPendingBackground_Checks` | `z2-death-check` | destroying the job system with an incomplete operation fails `ZENITH_CHECK` |
| `Test_JobSystem_RenderSubmitWithoutThread_Checks` | `z2-death-check` | pushing render-submit work in the headless configuration fails `ZENITH_CHECK` |
| `Test_JobNodeLaunch_RenderSubmitInjectionOverflow_Fatal` | `z2-death-check` | with `m_uFaultInjectInjectionFullAt = 1`, a render-submit job whose completion makes one ANY launch ready terminates with `FATAL_CODE_QUEUE_FULL` naming `jobs.injection`, and that launch never runs on the render-submit thread |
| `Test_JobContext_ParallelForFromRenderSubmit_Checks` | `z2-death-check` | `ParallelFor` inside a render-submit job fails `ZENITH_CHECK` |
| `Test_JobNodeLaunch_NullPusherOffMainThread_Checks` | `z2-death-check` | `Zenith_JobsPushNodes` with `pxPusher == nullptr` from a test-owned thread fails `ZENITH_CHECK` |
| `Test_JobSystem_BackgroundNameWithoutPrefix_Asserts` | `z2-death-assert` | a name not starting with `bg/` fails `ZENITH_ASSERT` |
| `Test_JobSystem_RunRootDuringIteration_Checks` | `z2-death-check` | `RunRoot` between `Zenith_JobsBeginIteration` and `Zenith_JobsEndIteration` fails `ZENITH_CHECK` |
| `Test_JobContext_ContinueFromRangeBody_Asserts` | `z2-death-assert` | `Continue` inside a `ParallelFor` body fails `ZENITH_ASSERT` |
| `Test_JobContext_ParallelForInBackground_Asserts` | `z2-death-assert` | `ParallelFor` in a background job fails `ZENITH_ASSERT` |
| `Test_JobContext_ParallelForZeroGrain_Asserts` | `z2-death-assert` | `uGrain == 0` fails `ZENITH_ASSERT` |
| `Test_JobContext_BackgroundCallInFrameJob_Asserts` | `z2-death-assert` | `ReportFailed` in a frame job fails `ZENITH_ASSERT` |
| `Test_JobNodeLaunch_BeginIterationTwice_Checks` | `z2-death-check` | a second `Zenith_JobsBeginIteration` without an end fails `ZENITH_CHECK` |
| `Test_JobNodeLaunch_BeginIterationFrameIndexNotIncreasing_Checks` | `z2-death-check` | the first `Zenith_JobsBeginIteration` after `Create` takes frame index 0 and succeeds (its iteration ends normally); a second call with index 0 again fails `ZENITH_CHECK` |
| `Test_JobNodeLaunch_TryInjectFromMainThread_Checks` | `z2-death-check` | `Zenith_JobsTryInjectNode` called by the main thread between iterations fails `ZENITH_CHECK` |
| `Test_JobNodeLaunch_TryInjectFromJobThread_Checks` | `z2-death-check` | `Zenith_JobsTryInjectNode` called from a job body fails `ZENITH_CHECK` |
| `Test_BoundedMpmcQueue_FatalPolicy_TerminatesWithCode` | `z2-death-check` | a full `QUEUE_FULL_POLICY_FATAL` queue terminates with `FATAL_CODE_QUEUE_FULL` naming the queue |
| `Test_FloatEnvironment_ChangedInJob_AssertsAtNextJob` | `z2-death-assert` | a job that clears FTZ makes the next job on that thread fail the environment assert ([Z2-N-CONTRACT-361]) |
| `Test_SimulationClock_TimeScaleNaN_Asserts` | `z2-death-assert` | `SetTimeScale(NaN)` fails `ZENITH_ASSERT` |

## Examples

| Example | Shows | Cited by |
|---|---|---|
| `Tests/Examples/Zenith_JobContext_Example.cpp` | `ParallelFor` over 10,000 items with per-leaf results in the node arena and a merge `Continue` that folds them in leaf order | the `Zenith_JobContext` contract block |
| `Tests/Examples/Zenith_JobSystem_Example.cpp` | `Create`, `RunRoot`, `SubmitBackgroundCopy` with a 2-slice chain, `CancelAndDrainBackground`, destruction | the `Zenith_JobSystem` contract block |

Both build into `ZenithJobs_Tests` and run as tests ([Z2-N-CONTRACT-421]).

## Benchmarks

| Harness | Target | Measures | Used by |
|---|---|---|---|
| `Zenith2/Benchmarks/Budgets/JOBS/` | `Zenith_Bench_BudgetsJOBS` | the Stage 1 per-system budget harness: `Budgets_JobsSchedulerOnly` of [Z2-N-BUDGET-228] (per frame for 7,200 frames, a fan-out of 1,024 empty jobs joined by one continuation and a chain of 64 dependent empty continuations, on 1, 2, 8 and all workers, frame wall time at p95 per worker count), and the per-frame dispatch overhead of a 200-node, 4,000-leaf synthetic frame, reported as median and 95% CI over 10 repetitions; the reference engine's `Zenith/TaskSystem` figure is measured by the human with that engine's own profiler and entered in the ruling's report | the Stage 1 per-system budget ticket for JOBS (Z2-B-JOBS-001 to -005) |
| `Zenith2/Benchmarks/Jobs/` | `Zenith_Bench_Jobs` | `Zenith_WorkStealingDeque` against a mutex-guarded `std::deque`, and `Zenith_BoundedMpmcQueue` against a mutex-guarded `std::deque`, at 2, 8 and all threads; `ParallelFor` leaf throughput; park-to-wake latency | the two custom-infrastructure justification records; the Stage 2 confirmation of Z2-B-JOBS-001 |

Each harness registers a CTest `--smoke` run in the `Test` step (`Zenith2/Docs/Architecture/BuildSystem.md → "Tools, benchmarks and integration suites"`); full measurements run on the reference desktop and the S24 and commit raw data under `Zenith2/Docs/Research/Jobs/`.

## Tests JOBS depends on in other libraries

| Test | Library | Why JOBS depends on it |
|---|---|---|
| `Test_FrameGraph_ZonesForEveryNode` | ZenithProfiling (DEVT) | proves the zones JOBS emits for sub-jobs and continuations reach the profiler with the node zones GRAPH emits |
| `Test_Profiler_ContentionRowsFromCounters` (pending DEVT) | ZenithProfiling (DEVT) | proves the on-screen profiler's contention rows are computed from the JOBS counters alone, with the formulas of `Zenith2/Docs/Design/JOBS.md → "On-screen profiler rows"` |
| `Test_FrameGraph_AudioUpdateOncePerFrame` | ZenithAudio (AUDIO) | proves the epilogue placement the executor provides |
| `Test_Script_Determinism_WorkerFloatEnvironment` | ZenithScript (SCRIPT) | reads the FP environment JOBS applies, on every worker |
| `Test_FrameGraph_StressUnderTsan`, `Test_FrameGraph_MainAffinity_RunsOnMainThread`, `Test_FrameGraph_RenderSubmitAffinity_RunsOnSubmitThread`, `Test_FrameGraph_CancelledInstance_Completes`, `Test_FrameGraph_PipelinedRender_OverlapsSimulation`, `Test_FrameGraph_SimulationNeverTwoFramesAhead` and the other `Test_FrameGraph_*` tests | ZenithJobs (GRAPH) | run GRAPH's executor on JOBS through the seam; the executor-level cases of `Zenith2/Docs/Architecture/CPUFrameGraph.md → "Required tests"` |
