# JOBS design — algorithms and state machines

This file is part of the JOBS design family. The entry document is `Zenith2/Docs/Design/JOBS.md`; this file holds the internal data structures, the algorithms step by step, their memory orderings, complexity and failure behaviour, and the state machines of `ZenithJobs`' JOBS-owned half. The public types it refers to are in `Zenith2/Docs/Design/JOBS/PublicAPI.md`.

Every atomic operation below states its memory order, as `Zenith2/Docs/Contracts.md → "Threading and concurrency contract"` requires; an implementation that uses a stronger order than stated needs no justification, a weaker one is a defect. `seq_cst` appears only where the step names the store-load ordering it protects.

## Internal types and files

JOBS's internal files sit under `Internal/Jobs/` and `Internal/Clock/`, beside GRAPH's `Internal/FrameGraph/` in the same library (`Zenith2/Docs/Design/GRAPH.md → "The JOBS boundary"`); a JOBS ticket edits no GRAPH file and the reverse.

| File under `Zenith2/Engine/ZenithJobs/Internal/` | Holds |
|---|---|
| `Jobs/Zenith_JobSystemState.h`, `Jobs/Zenith_JobSystem.cpp` | `Zenith_JobSystemState`: the thread states, queues, idle mask, operation table and ticket array, shutdown flags; `Create`, the destructor, `SubmitBackground*`, `RunRoot`, `CancelAndDrainBackground`, `Zenith_ParseCpuMask` |
| `Jobs/Zenith_JobThreadState.h` | `Zenith_JobThreadState` (one per thread slot) and `Zenith_JobThreadCounters` |
| `Jobs/Zenith_JobThreadLoop.h`, `Jobs/Zenith_JobThreadLoop.cpp` | the worker loop, the render-submit loop, the main-thread helping loop, job invocation and stage-return handling |
| `Jobs/Zenith_JobRecords.h` | `Zenith_JobRecord`, `Zenith_BackgroundRecord`, `Zenith_JobNodeState`, `Zenith_JobSegment`, `Zenith_JobContinuation` |
| `Jobs/Zenith_WorkStealingDeque.h`, `Jobs/Zenith_WorkStealingDeque.cpp` | `Zenith_WorkStealingDeque` (Chase–Lev, fixed capacity) |
| `Jobs/Zenith_BoundedMpmcQueue.h` | `Zenith_BoundedMpmcQueue<T>` (Vyukov, fixed capacity, trivially copyable `T`) |
| `Jobs/Zenith_JobContext.cpp` | `Zenith_JobContext` members, `ParallelFor`, `Continue`, `Zenith_CancellationToken` |
| `Jobs/Zenith_JobNodeLaunch.h`, `Jobs/Zenith_JobNodeLaunch.cpp` | the node launch seam GRAPH's executor calls (see "Node launch seam") |
| `Jobs/Zenith_JobsNames.h`, `Jobs/Zenith_JobsNames.cpp` | the constant counter-name tables, the log category, and the allocation-free decimal formatter for zone names |
| `Jobs/Zenith_FloatEnvironment.cpp` | `Zenith_IsExpectedFloatEnvironment` (platform-neutral) |
| `Clock/Zenith_SimulationClock.cpp` | the clock and `Zenith_ValidateSimulationClockDesc` |
| `Windows/Zenith_FloatEnvironment_Windows.cpp`, `Linux/Zenith_FloatEnvironment_Linux.cpp`, `Android/Zenith_FloatEnvironment_Android.cpp` | the floating-point environment (`Zenith2/Docs/Design/JOBS/PublicAPI.md → "Zenith_FloatEnvironment header"`) |

Everything platform-specific the scheduler needs except the floating-point environment comes from ZenithCore: threads and their names, priorities and affinity masks, semaphores, the processor count `Zenith_GetHardwareConcurrency`, the spin hint `Zenith_CpuRelax` and the monotonic clock `Zenith_GetMonotonicTimeNs` (`Zenith2/Docs/Design/CORE/Threading.md → "Threads"`). JOBS has no platform shim of its own.

## Thread state and the no-global rule

`Zenith_JobThreadState` exists once per thread slot, allocated by `Create` from the job system's heap in one array of `N + 2` entries, each aligned to 128 bytes so that no two threads' hot fields share a cache line pair:

| Field | Type | Owner writes | Read by |
|---|---|---|---|
| `m_uIndex` | `uint32` | Create | everyone |
| `m_xDeque` | `Zenith_WorkStealingDeque` (absent for the render-submit slot) | owner (push, pop) | thieves (steal), the publisher (its atomic high water and full count) |
| `m_xRecordArena` | `Zenith_Arena` with `Zenith_ArenaDesc{ "jobs.records.<i>", 64 KiB, 16 MiB, 1 }` | owner | readers of the records it holds |
| `m_xNodeArena`, `m_xWorkerArena` | `Zenith_Arena` with `Zenith_ArenaDesc{ "jobs.node.<i>", 64 KiB, 16 MiB, 1 }` and `{ "jobs.worker.<i>", 64 KiB, 16 MiB, 1 }` | owner | owner |
| `m_ulSeenFrameEpoch` | `uint64` | owner | owner |
| `m_ulStealState` | `uint64` xorshift64* state, seeded `0x9E3779B97F4A7C15 ^ (m_uIndex + 1)` | owner | owner |
| `m_pxHandoffHead`, `m_pxHandoffTail` | `Zenith_JobRecord*`, both null when empty | owner | owner |
| `m_xParkSemaphore` | `Zenith_SemaphoreNoProfiling(0, 1u << 30)` (unprofiled: the park reports its own named wait, step 5) | wakers post, owner waits | — |
| `m_pxHeldBackgroundContinuation` | `Zenith_BackgroundRecord*` pointing at `m_xHeldBackgroundStorage` when held, else null | owner | owner |
| `m_xHeldBackgroundStorage` | `Zenith_BackgroundRecord` | owner | owner |
| `m_xCounters` | `Zenith_JobThreadCounters` (below) | owner | the main thread at publication |
| `m_axCounterSnapshot` | `uint64[uJOBS_COUNTER_COUNT]`, the values last published | the main thread | the main thread |
| `m_pxProfilerLane` | opaque lane handle from `Zenith_Profiler`, or null | Create | owner |

The handoff chain is a FIFO of records owned by the thread alone: a record joins it through its `m_pxNextHandoff` link (see "Record types"), the thread's loop takes the head before any queue (worker step 1, main step 2, render-submit loop), and no other thread ever reads it, so it needs no atomics and has no capacity: every record in it already lives in an arena. A record enters it in three cases, each at the end of a job rather than inside one: the first ANY launch of a push (the handoff of "Push order and routing" step 3); a continuation dispatched by `OpenGate` on a thread whose affinity allows it ("Stages segments and continuations"; this is how the render-submit thread takes a render-submit continuation); and a launch whose deque push was rejected ("Push order and routing" step 4).

The arena caps (16 MiB each per slot) are design defaults bound to Z2-B-JOBS-004 "Job system resident memory" ([Z2-N-BUDGET-230]; `Zenith2/Docs/Design/JOBS.md → "Rows JOBS owns"`); reaching a cap is `ZENITH_FATAL(FATAL_CODE_ARENA_EXHAUSTED)` through `Zenith_Arena::Allocate` ([Z2-N-CONTRACT-227]), because a frame's job records are bounded by its schedule and its `ParallelFor` leaf counts.

- The job system holds no namespace-scope, static or `thread_local` state (`Zenith2/Docs/Contracts.md → "No mutable global state"`, [Z2-N-ARCH-387]). A thread reaches its state only through the pointer passed to its entry function, and a job reaches it only through `Zenith_JobContext::m_pThread`.
- The main thread's slot (index 0) is used only while the main thread is inside `Zenith_JobsRunMainUntil` (called by GRAPH's `RunFrame`), `RunRoot` or `CancelAndDrainBackground`; outside those calls nothing pushes into its deque, because every push goes to the pushing thread's own slot and the main thread runs no job outside them.
- The render-submit slot (index N+1) has no deque: the ANY launches its jobs make ready go to the injection queue, and it runs no ANY job itself (see "Queues and their full policies").

`Zenith_JobSystemState`, one per job system, holds the shared state; every field and its memory orders:

| Field | Type, initial value | Written by (order) | Read by (order) |
|---|---|---|---|
| `m_pxHeap` | `Zenith_Heap*` | Create | Create, destructor, `Zenith_JobsReserve*Capacity` |
| `m_uWorkerCount` | `uint32` N | Create | everyone |
| `m_uMaxConcurrentBackground` | `uint32` | Create | workers |
| `m_bAcceptToolBackground` | `bool`, the desc's value | Create | submitters (admission of `JOB_PRIORITY_BACKGROUND_TOOL`) |
| `m_uMaxConcurrentToolBackground` | `uint32`, resolved by `Create` step 1; 0 when the priority is not accepted | Create | workers (the tools-host gate), `GetMaxConcurrentToolBackground` |
| `m_ulMainThreadID` | `uint64`, `Zenith_GetCurrentThreadID()` of the thread that called `Create` | Create | `Zenith_JobsPushNodes`, `RunRoot`, `CancelAndDrainBackground`, the destructor |
| `m_axThreads` | `Zenith_JobThreadState*`, N + 2 entries | Create | everyone |
| `m_axThreadHandles` | `Zenith_Thread[N + 1]` (workers, then render-submit) | Create | destructor (join) |
| `m_xInjection`, `m_xMainQueue`, `m_xRenderSubmitQueue` | `Zenith_Owned<Zenith_BoundedMpmcQueue<Zenith_JobRecord*>>` | Create; the reserve calls between iterations | producers and consumers (the queue's own orders) |
| `m_xBackgroundHigh`, `m_xBackgroundLow` | `Zenith_Owned<Zenith_BoundedMpmcQueue<Zenith_BackgroundRecord>>` | Create | producers and workers |
| `m_xBackgroundTool` | `Zenith_Owned<Zenith_BoundedMpmcQueue<Zenith_BackgroundRecord>>` of `m_uToolBackgroundCapacity` cells; null when the priority is not accepted | Create | producers and workers ("Tools-host background priority") |
| `m_xOperations` | `Zenith_Owned<Zenith_OperationTable>`, `2 × m_uBackgroundCapacity + T + N + 2` slots, T = `m_uToolBackgroundCapacity` when the tools-host priority is accepted, else 0 | Create | submitters, workers |
| `m_axTickets` | `Zenith_OperationTicket*` array of the same length | the submitter (move in), the completing slice (move out) | the slice running the operation (handed over by the queue's acquire/release) |
| `m_ulFrameEpoch` | `std::atomic<uint64>`, 0 | main (`release`) | every thread (`acquire`) |
| `m_bMainHelping` | `std::atomic<uint32>`, 0 | main (`relaxed`; only the main thread reads it) | `Zenith_JobsPushNodes` on the main thread |
| `m_bIterationActive` | `std::atomic<uint32>`, 0 | main: `store(1, seq_cst)` in `Zenith_JobsBeginIteration` and at the start of `RunRoot`, `store(0, seq_cst)` in `Zenith_JobsEndIteration` and at the end of `RunRoot`, each before that call's injected-node drain | `Zenith_JobsTryInjectNode` (`seq_cst`, the Dekker pair with `m_iInjectedInFlight`); the `RunRoot` and `Zenith_JobsBeginIteration` checks |
| `m_ulIterationFrameIndex`, `m_bIterationFrameIndexSeen` | `uint64`, 0; `bool`, false | main (`Zenith_JobsBeginIteration`, which sets the flag on its first call) | main (the strictly-increasing check of every call after the first) |
| `m_ulInjectCounter` | `std::atomic<uint64>`, 1 | `Zenith_JobsTryInjectNode` (`fetch_add(1, relaxed)`) | the same (the injected launch's invocation ID) |
| `m_ulInjectedAccepted` | `std::atomic<uint64>`, 0 | `Zenith_JobsTryInjectNode` (`fetch_add(1, relaxed)` before its push) | the publisher (`exchange(0, relaxed)` after the drain, which the release/acquire pair of `m_iInjectedInFlight` orders after every accepted node's increment) |
| `m_iInjectedInFlight` | `std::atomic<sint64>`, 0 | `Zenith_JobsTryInjectNode` (`fetch_add(1, seq_cst)` before its window check; `fetch_sub(1, release)` when it refuses); the completing thread of an injected node (`fetch_sub(1, release)` after the completion callback, then the `Zenith_JobsWakeMain` pair) | the injected-node drain of `Zenith_JobsEndIteration` and `RunRoot` (first load `seq_cst`, then `acquire`) |
| `m_iForeignQueued` | `std::atomic<sint64>`, 0 | `Zenith_JobsTryInjectNode` (`fetch_add(1, relaxed)`, undone with `fetch_sub(1, relaxed)` on refusal); the thread that pops a heap-owned record from the injection queue (`fetch_sub(1, relaxed)`) | `Zenith_JobsTryInjectNode` (the value its own `fetch_add` returned) |
| `m_uInjectionReserved` | `uint32`, 256 | main (`Zenith_JobsReserveInjectionCapacity`, between iterations) | `Zenith_JobsTryInjectNode` (published by the `seq_cst` store of `m_bIterationActive` that opens the next iteration) |
| `m_ulIdleMask` | `std::atomic<uint64>`, 0 | parkers (`fetch_or seq_cst`, `fetch_and relaxed`), wakers (`fetch_and acq_rel`) | wakers (`relaxed` after a `seq_cst` fence) |
| `m_bRenderSubmitIdle` | `std::atomic<uint32>`, 0 | the render-submit thread (`relaxed` store, then a `seq_cst` fence) | render-submit pushers (`relaxed` after a `seq_cst` fence), then `exchange(0, acq_rel)` |
| `m_iBackgroundRunning` | `std::atomic<sint32>`, 0 | workers (`fetch_add acq_rel`, `fetch_sub release`) | workers |
| `m_iToolBackgroundRunning` | `std::atomic<sint32>`, 0 | workers (`fetch_add acq_rel` to take a tools-host slot, `fetch_sub release` to return it; a worker inside a `JOB_PRIORITY_BACKGROUND_TOOL` slice holds one slot of each counter) | workers; the publisher (`relaxed`, `jobs.bg.tool_running`) |
| `m_bToolRefusalLogged` | `std::atomic<uint32>`, 0 | the first refused tools-host submission (`exchange(1, relaxed)`) | the same |
| `m_iBackgroundInFlight` | `std::atomic<sint64>`, 0 | submitters (`fetch_add relaxed`), completing slices (`fetch_sub release`, then the `Zenith_JobsWakeMain` pair: `seq_cst` fence, `m_ulIdleMask` relaxed load, `fetch_and(~1, acq_rel) & 1` → `Post()` slot 0) | `CancelAndDrainBackground` (its completion condition, after the park fence), the destructor (`acquire`) |
| `m_iLaunchedNotStarted` | `std::atomic<sint64>`, 0 | pushers (`fetch_add relaxed`), stage-0 invocations (`fetch_sub relaxed`) | pushers (high water, `relaxed`) |
| `m_ulReadyHighWater` | `std::atomic<uint64>`, 0 | pushers (relaxed compare-exchange maximum) | the publisher (`exchange(0, relaxed)`) |
| `m_bStopRequested` | `std::atomic<uint32>`, 0 | destructor, failed `Create` (`release`) | every loop before parking and after waking (`acquire`) |
| `m_bShuttingDown` | `std::atomic<uint32>`, 0 | `CancelAndDrainBackground` (`release`) | submitters (`acquire`) |
| `m_uFaultInjectInjectionPushes` | `std::atomic<uint32>`, 0 | injection pushers (`fetch_add relaxed`) | the same, against `m_uFaultInjectInjectionFullAt` |

### Counters and their memory model

`Zenith_JobThreadCounters` holds one `std::atomic<uint64>` per per-thread counter: steals, steal failures, inline runs, idle nanoseconds, busy nanoseconds, parks, background slices completed, cancelled, failed, slice overruns. Each counter is written by its owning thread alone, as `store(load(relaxed) + delta, relaxed)` (one writer, so no read-modify-write is needed), and only ever increases. The publisher (`Zenith_JobsEndIteration`, or the end of `RunRoot`, on the main thread) reads every counter with a `relaxed` load, publishes the difference from `m_axCounterSnapshot` and stores the new value there. The deque's high water and full count and the MPMC queues' high water and full count are atomics in their structures; the publisher reads them with `exchange(0, relaxed)`.

- Frame-path counters (steals, steal failures, inline runs, the deque and queue high waters and full counts, the ready high water) are incremented before the completion decrement of the job that caused them, so the iteration's `acq_rel` completion chain makes every such increment of the iteration visible to the publisher: the published value of the iteration is exact.
- Counters written outside that chain (idle and busy nanoseconds and parks, written after a thread's `Wait()` returns at an arbitrary time; every background counter, written by slices that run while the main thread publishes) are read concurrently with their writes. They are relaxed atomics, so there is no data race; an increment that lands after the publisher's load is attributed to the next iteration, never lost.
- No counter access is a plain read or write of shared memory, so `linux-tsan` needs no suppression for them.

## Job records and the memory model

### Record types

```cpp
enum Zenith_JobRecordKind : uint8
{
	JOB_RECORD_KIND_NODE_STAGE,     // a node's body (stage 0) or one of its continuations (stage >= 1)
	JOB_RECORD_KIND_RANGE,          // a ParallelFor range [m_uBegin, m_uEnd) covering whole leaves
	JOB_RECORD_KIND_ROOT,           // RunRoot's root job
};

struct alignas(16) Zenith_JobRecord   // 64 bytes (56 used), trivially copyable and destructible
{
	void* m_pFunction;                  // Zenith_PfnJob (NODE_STAGE, ROOT) or Zenith_PfnParallelForBody (RANGE)
	void* m_pData;
	Zenith_JobNodeState* m_pxNode;      // the launched node the job belongs to (never null for frame records)
	Zenith_JobSegment* m_pxSegment;     // RANGE: the segment it counts against; NODE_STAGE: the stage's continuation or null
	uint32 m_uBegin;                   // RANGE: first item; NODE_STAGE: stage number
	uint32 m_uEnd;                     // RANGE: one past the last item
	uint32 m_uGrain;                   // RANGE: the call's grain (>= 1)
	Zenith_JobRecordKind m_eKind;
	uint8 m_auPad[3];
	Zenith_JobRecord* m_pxNextHandoff;  // the owning thread's handoff chain link; null outside it
};

struct alignas(16) Zenith_BackgroundRecord   // 128 bytes; its queue cell with the sequence word is 192 bytes
{
	Zenith_PfnJob m_pfnJob;
	void* m_pData;                      // submitter's pData; unused while m_bPayloadInline is true
	uint32 m_uTicketIndex;             // the slot of the operation's Zenith_OperationTicket in the job system's ticket array
	const char* m_szName;               // "bg/<Name>", static lifetime
	uint64 m_ulIssueID;                // the operation's issue ID
	alignas(16) uint8 m_aPayload[uJOBS_BACKGROUND_PAYLOAD_BYTES];
	uint16 m_uSliceCount;              // slices run so far
	Zenith_JobPriority m_ePriority;
	bool m_bPayloadInline;              // true: the job receives the address of the executing copy's m_aPayload
};
```

A background record moves by value (into its queue cell, onto the running worker's stack, into a re-queued continuation), so an inline payload's address changes between slices. The runner therefore passes, for an inline-payload record, the address of the copy it is executing; a `Continue(pfn, pData)` whose `pData` equals that address keeps the payload inline, and the next slice receives its own copy's address. Any other `pData` passed to `Continue` clears `m_bPayloadInline` and is passed through unchanged.

### Lifetimes

| Memory | Allocated by | From | Lives until | Reclaimed by |
|---|---|---|---|---|
| frame record (`Zenith_JobRecord`), segment, continuation | the thread that pushes or registers it | that thread's `m_xRecordArena` | the end of the iteration (every job of the iteration has returned) | the owner, lazily, at its first frame action of the next iteration (see "Frame epochs") |
| node state (`Zenith_JobNodeState`, one per launch) | the thread that calls `Zenith_JobsPushNodes` | that thread's `m_xRecordArena` | the end of the iteration | the owner, lazily, as above |
| injected node: node state, segment 0 and stage-0 record in one block | `Zenith_JobsTryInjectNode` | `m_pxHeap` (`Zenith_New`), `m_bHeapOwned = true` | its completion callback, which returns before the iteration it joined ends | `CompleteNode`, through `Zenith_Delete` on `m_pxHeap` after the callback; its continuations, sub-jobs and scratch come from the executing threads' arenas like any node's |
| GRAPH's instance records and iteration state | GRAPH's executor | its iteration arena (`Zenith2/Docs/Design/GRAPH.md → "Data structures and formats"`) | the end of the iteration | GRAPH |
| node-arena and worker-arena allocations of frame jobs | the job | the executing thread's `m_xNodeArena` / `m_xWorkerArena` | the end of the iteration (so at least until the node instance completes, as the API promises) | the owner, lazily, as above: `Zenith2/Docs/Architecture/CPUFrameGraph.md → "Public types"` declares `GetNodeArena` "valid until the owning node completes; reclaimed at the next frame boundary", because a per-node reset is impossible when one thread interleaves the jobs of several nodes |
| background record | `SubmitBackground*` | copied by value into its background queue cell; a popped record lives on the running worker's stack or as its held continuation | the operation's completion | nothing to free |
| arena allocations of background jobs | the job | the executing thread's arenas | the end of the current slice | the owner, lazily, as above, never during a slice |

- Every object stored in an arena is trivially destructible (`Zenith2/Docs/Contracts/Memory.md → "Zenith_Arena"`, [Z2-N-CONTRACT-232]); `std::atomic<uint32>` and `std::atomic<uint64>` members qualify, and records are created with `Zenith_Construct` alone (JOBS contains no placement `new`, rule F2).
- A record is dereferenced by a thread other than its allocator only after that thread has acquired its pointer through a queue operation whose release the allocator performed after writing the record.
- The last access a thread makes to iteration memory for a job is the atomic decrement that may complete the job's node, instance or iteration; the zone end, the counter updates and the FP-environment check come before it. After that decrement the thread touches only its own stack and thread state. This is what makes lazy reclamation safe.

### Frame epochs

- `Zenith_JobSystemState::m_ulFrameEpoch` (`std::atomic<uint64>`) is incremented with `release` by the main thread in `Zenith_JobsBeginIteration` (called by GRAPH's `RunFrame` before its first launch) and at the start of `RunRoot`, before the first job of the iteration is pushed.
- Every thread, before it runs any frame job, compares `m_ulFrameEpoch.load(acquire)` with `m_ulSeenFrameEpoch`; when they differ it calls `Reset()` on its record, node and worker arenas and stores the new epoch. A thread that runs no frame work in an iteration keeps its arenas from the previous one, which is harmless.
- An injected node belongs to the iteration that accepted it (`Zenith_JobsTryInjectNode` refuses outside one), and `Zenith_JobsEndIteration` and `RunRoot` drain every accepted injected node before they return, so the epoch never advances while an injected node, its continuations or its sub-jobs can still run: no record or arena allocation outlives its epoch on the injection path either.
- A background slice never triggers a reset while it runs: the check happens only before a frame job, and a thread runs one job at a time. After a background slice returns and before the thread's next background slice, the thread also resets the three arenas when its epoch is stale, so a long-running background chain does not grow them.

## Work-stealing deque

`Zenith_WorkStealingDeque` is a fixed-capacity Chase–Lev deque of `Zenith_JobRecord*` in the formulation of Lê, Pop, Cohen and Zappa Nardelli, "Correct and Efficient Work-Stealing for Weak Memory Models" (PPoPP 2013), without the growth step.

```cpp
class Zenith_WorkStealingDeque   // Internal
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_WorkStealingDeque>, Zenith_JobsError> Create(
		uint32 uCapacity, Zenith_Heap& xHeap);        // power of two, [64, 2^20]
	bool TryPush(Zenith_JobRecord* pxRecord);   // owner only; false = full (REJECT)
	Zenith_JobRecord* Pop();                    // owner only; LIFO; null = empty
	Zenith_StealResult Steal(Zenith_JobRecord*& pxOut);   // any thread; FIFO
	sint64 GetApproximateSize() const;         // bottom - top, relaxed loads, clamped at 0
private:
	alignas(64) std::atomic<sint64> m_iTop;       // own cache line
	alignas(64) std::atomic<sint64> m_iBottom;    // own cache line
	alignas(64) std::atomic<Zenith_JobRecord*>* m_apxSlots;   // uCapacity slots from the heap, value-initialised
	uint64 m_ulMask;                              // uCapacity - 1
	std::atomic<uint64> m_ulHighWater;            // owner-written (relaxed), exchanged by the publisher
	std::atomic<uint64> m_ulFullEvents;           // owner-written (relaxed), exchanged by the publisher
};

enum Zenith_StealResult : uint8
{
	STEAL_RESULT_EMPTY,    // the victim held nothing (steps 1–4)
	STEAL_RESULT_ABORT,    // the victim held work but another thread won the compare-exchange (step 6)
	STEAL_RESULT_TAKEN,    // pxOut is the stolen record
};
```

`TryPush(p)`:

1. `b = m_iBottom.load(relaxed)`; `t = m_iTop.load(acquire)`.
2. If `b - t >= capacity`: increment `m_ulFullEvents` (`relaxed` load and store), return false.
3. `m_apxSlots[b & mask].store(p, relaxed)`.
4. `m_iBottom.store(b + 1, release)` — publishes the slot and the record to a thief that acquires `m_iBottom`.
5. If `b + 1 - t` exceeds `m_ulHighWater.load(relaxed)`, store it (`relaxed`); return true.

`Pop()`:

1. `b = m_iBottom.load(relaxed) - 1`; `m_iBottom.store(b, relaxed)`.
2. `std::atomic_thread_fence(seq_cst)` — orders the bottom store before the top load against a concurrent thief's top load and bottom load (the store-load pair that decides who takes the last element).
3. `t = m_iTop.load(relaxed)`.
4. If `t > b`: `m_iBottom.store(b + 1, relaxed)`, return null (empty).
5. `p = m_apxSlots[b & mask].load(relaxed)`.
6. If `t == b` (last element): `won = m_iTop.compare_exchange_strong(t, t + 1, seq_cst, relaxed)`; `m_iBottom.store(b + 1, relaxed)`; return `won ? p : null`.
7. Otherwise return p.

`Steal()`:

1. `t = m_iTop.load(acquire)`.
2. `std::atomic_thread_fence(seq_cst)` — the thief side of the store-load pair in `Pop` step 2.
3. `b = m_iBottom.load(acquire)`.
4. If `t >= b`: return `STEAL_RESULT_EMPTY`.
5. `p = m_apxSlots[t & mask].load(relaxed)`.
6. If `!m_iTop.compare_exchange_strong(t, t + 1, seq_cst, relaxed)`: return `STEAL_RESULT_ABORT` (another thief or the owner took it).
7. `pxOut = p`; return `STEAL_RESULT_TAKEN`; the caller dereferences p only now.

ABA handling: `m_iTop` and `m_iBottom` are 64-bit indices that only increase (the owner's temporary decrement in `Pop` is undone before it returns), so an index value is never reused within 2^63 operations; a slot is reused only after both indices have moved past it, and a thief holding a stale `t` fails its compare-exchange in step 6 before it can use the slot's value. Because the deque never grows, no slot array is ever freed while a thief may read it. Because the slot holds a pointer that a thief dereferences only after winning the compare-exchange, a torn or stale slot read is never used, and every slot access is an atomic access, so the algorithm is free of data races as ThreadSanitizer defines them.

Complexity: every operation is O(1) and wait-free except `Steal`, which is lock-free (one compare-exchange attempt; the caller retries with another victim).

## Bounded MPMC queue

`Zenith_BoundedMpmcQueue<T>` is Dmitry Vyukov's bounded multi-producer multi-consumer array queue. `T` is trivially copyable; each cell is `{ std::atomic<uint64> m_ulSequence; T m_xValue; }` padded to a multiple of 64 bytes.

```cpp
template<typename T>
class Zenith_BoundedMpmcQueue   // Internal
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_BoundedMpmcQueue>, Zenith_JobsError> Create(
		const char* szName, uint32 uCapacity, Zenith_QueueFullPolicy eFullPolicy, Zenith_Heap& xHeap);
	// uCapacity: a power of two in [64, 2^22] (cells are indexed with pos & (uCapacity - 1)); every grown
	// capacity is rounded up to the next power of two before it reaches Create.
	bool TryPush(const T& xValue);   // false = full; FATAL policy: ZENITH_FATAL(FATAL_CODE_QUEUE_FULL)
	bool TryPop(T& xOut);            // false = empty
	uint64 GetApproximateSize() const;
	uint64 TakeHighWater();         // returns and resets (main thread, between iterations)
	uint64 TakeFullEvents();
private:
	alignas(64) std::atomic<uint64> m_ulEnqueuePos;
	alignas(64) std::atomic<uint64> m_ulDequeuePos;
	alignas(64) std::atomic<uint64> m_ulHighWater;
	std::atomic<uint64> m_ulFullEvents;
	// cell array, mask, name, policy
};
```

`TryPush(v)`: loop: `pos = m_ulEnqueuePos.load(relaxed)`; `cell = cells[pos & mask]`; `seq = cell.m_ulSequence.load(acquire)`; if `seq == pos`, try `m_ulEnqueuePos.compare_exchange_weak(pos, pos + 1, relaxed)` and on success write `cell.m_xValue = v`, `cell.m_ulSequence.store(pos + 1, release)`, update the high water with a relaxed compare-exchange maximum of `pos + 1 - m_ulDequeuePos.load(relaxed)`, and return true; if `seq < pos` the queue is full: increment `m_ulFullEvents` (relaxed) and apply the full policy (`REJECT`: return false; `FATAL`: `ZENITH_FATAL(FATAL_CODE_QUEUE_FULL, "<name>")`); otherwise reload and retry.

`TryPop(out)`: loop: `pos = m_ulDequeuePos.load(relaxed)`; `seq = cell.m_ulSequence.load(acquire)`; if `seq == pos + 1`, try `m_ulDequeuePos.compare_exchange_weak(pos, pos + 1, relaxed)` and on success copy `out = cell.m_xValue`, `cell.m_ulSequence.store(pos + capacity, release)`, return true; if `seq < pos + 1`, return false (empty); otherwise retry.

The cell value is accessed non-atomically, but only by the one thread that won the position, between an acquire of the cell's sequence and a release of it, so the accesses are ordered and race-free. Both operations are lock-free and O(1) apart from contention retries.

## Queues and their full policies

| Queue | Type | Capacity | Full policy | On full | Counter prefix |
|---|---|---|---|---|---|
| per-thread deque (slots 0..N) | `Zenith_WorkStealingDeque` | `m_uDequeCapacity` | `QUEUE_FULL_POLICY_REJECT` | the pushing thread runs the job itself, per "ParallelFor splitting and inline execution" and "Push order and routing" | `jobs.deque` (sums and maxima over threads) |
| injection | `Zenith_BoundedMpmcQueue<Zenith_JobRecord*>` | `max(m_uInjectionCapacity, 2 × uInjectedNodes + 256)` rounded up to the next power of two, grown from the schedule by `Zenith_JobsReserveInjectionCapacity`; the last `m_uInjectionReserved = 2 × uInjectedNodes + 256` cells are reserved for the frame producer | frame producer (the render-submit thread): `QUEUE_FULL_POLICY_FATAL`; foreign producer: `QUEUE_FULL_POLICY_REJECT` at `capacity − m_uInjectionReserved` | render-submit push: `ZENITH_FATAL(FATAL_CODE_QUEUE_FULL, "jobs.injection")`, a scheduler defect, unreachable under any foreign load; `Zenith_JobsTryInjectNode` returns `JOBS_ERROR_QUEUE_FULL` having taken nothing once the foreign share is used, and the caller retries; `SubmitBackground*` never uses the queue | `jobs.injection` |
| main-affinity | `Zenith_BoundedMpmcQueue<Zenith_JobRecord*>` | 2 × main-affinity nodes of one frame + 256, from the schedule, rounded up to the next power of two | `QUEUE_FULL_POLICY_FATAL` | `ZENITH_FATAL(FATAL_CODE_QUEUE_FULL, "jobs.main")`: a scheduler defect | `jobs.main` |
| render-submit | `Zenith_BoundedMpmcQueue<Zenith_JobRecord*>` | 2 × render-submit nodes of one frame + 64, from the schedule, rounded up to the next power of two | `QUEUE_FULL_POLICY_FATAL` | `ZENITH_FATAL(FATAL_CODE_QUEUE_FULL, "jobs.render_submit")` | `jobs.render_submit` |
| background high, background low | `Zenith_BoundedMpmcQueue<Zenith_BackgroundRecord>` | `m_uBackgroundCapacity` each | `QUEUE_FULL_POLICY_REJECT` | `SubmitBackground*` returns `JOBS_ERROR_QUEUE_FULL`; a slice's continuation is held by its worker (see "Background jobs and operations") | `jobs.background_high`, `jobs.background_low` |
| background tool (tool hosts only: created when `m_bAcceptToolBackground`, absent otherwise) | `Zenith_BoundedMpmcQueue<Zenith_BackgroundRecord>` | `m_uToolBackgroundCapacity` (default 1,024) | `QUEUE_FULL_POLICY_REJECT` | as the other two background queues ("Tools-host background priority") | `jobs.background_tool` (published only where the queue exists) |

The injection queue has two kinds of producer. The render-submit thread pushes the ANY launches its jobs make ready (GRAPH's successors of render-submit nodes) and never anything else, because a render-submit job never calls `ParallelFor` (`ZENITH_CHECK`: render-submit work does not fan out) and its continuations keep render-submit affinity; The schedule bounds this producer strictly: an ANY node is pushed at most once per subgraph instance, and the instances counted in `uInjectedNodes` (the ANY nodes of one frame with a render-submit predecessor, counted over the instances one iteration runs) are all the render-submit thread can make ready in an iteration, so its occupancy never exceeds `uInjectedNodes`. Threads with no job context — I/O, test and tool threads, the foreign producers of `Zenith2/Docs/Architecture/CPUFrameGraph.md → "Queues"` — push through `Zenith_JobsTryInjectNode` with the REJECT policy, and they may use only `capacity − m_uInjectionReserved` cells: each push first takes a slot of `m_iForeignQueued` (`fetch_add(1, relaxed)`) and is refused, with the slot given back, when the value it returned is at least `capacity − m_uInjectionReserved`; a worker that pops a heap-owned (foreign) record gives its slot back. `m_iForeignQueued` never counts fewer foreign records than the queue holds, so foreign occupancy is at most `capacity − 2 × uInjectedNodes − 256` whatever the number of foreign threads, and the frame producer always finds its `2 × uInjectedNodes + 256` cells, of which it needs at most `uInjectedNodes`. A full queue from the frame producer is therefore a defect, never a load condition, as for the affinity queues ([Z2-N-ARCH-392]); an exact counter is used rather than the queue's approximate size, which concurrent pushers could each read as one below the limit. Like every `ZENITH_FATAL` it is a fatal, non-replayable diagnostic event, whose tick carries `REPLAY_TICK_FLAG_NON_REPLAYABLE` ([Z2-N-ARCH-790]). The render-submit thread never runs an ANY job on any path ([Z2-N-ARCH-390]).

The table restates [Z2-N-ARCH-391] and [Z2-N-CONTRACT-364] and gives each queue its counter names: `<prefix>.full`, `<prefix>.dropped` (always 0: no job queue drops) and `<prefix>.high_water`, published once per iteration (`Zenith2/Docs/Contracts.md → "Bounded queues"`). The affinity queues are created by `Create` with the minimum capacities (256 and 64), the injection queue with `max(m_uInjectionCapacity, 512)` and `m_uInjectionReserved = 256` (so the foreign share is at least 256 before the first `SetSchedule`; after growth it is `capacity − m_uInjectionReserved` computed in signed 64-bit arithmetic with a floor of 0, never a wrapped `uint32`), and they are grown (to the next power of two of the stated size) by `Zenith_JobsReserveAffinityCapacity` and `Zenith_JobsReserveInjectionCapacity`, which GRAPH's `SetSchedule` calls in the frame prologue, when no job runs: it allocates the larger queue through `Zenith_Heap::Allocate` (the OOM handler path), moves nothing (the queues are empty between iterations) and frees the old one. JOBS takes no `Zenith_Mutex` on any queue, push, pop, steal, park or completion path; background submission and completion reach CORE's operation-table lock (`uLOCK_LEVEL_OPERATION_TABLE`, 380, short-hold) inside `Zenith_OperationTable`.

## Worker loop

Each worker thread (indices 1..N) runs this loop from its entry function until shutdown. "Run" means "Invoke the job" below.

1. If the handoff chain is non-empty, take its head and go to 7.
2. Pop from its own deque (LIFO end). Found: go to 7.
3. Pop from the injection queue. Found: go to 7.
4. Steal: up to `2 × N` attempts. The candidates are `c = [0, 1, …, N]` with the thread's own index removed (N entries, in slot order); each attempt draws `x = xorshift64*(m_ulStealState)` and steals from `c[x mod N]`. `STEAL_RESULT_TAKEN`: count a steal; if the victim's approximate size is still at least 1, wake one parked thread (the wake chain that ramps a `ParallelFor` fan-out up); go to 7. `STEAL_RESULT_ABORT`: count a steal failure and try the next draw. `STEAL_RESULT_EMPTY`: try the next draw, counting nothing.
5. Background: if steps 2–4 found nothing in this pass and `m_iBackgroundRunning.fetch_add(1, acq_rel) < m_uMaxConcurrentBackground` (the background gate), take the first of:
   1. the held background continuation, if the thread holds one: a HIGH or LOW record runs at once; a `JOB_PRIORITY_BACKGROUND_TOOL` record runs only if `m_iToolBackgroundRunning.fetch_add(1, acq_rel) < m_uMaxConcurrentToolBackground` (the tools-host gate), and otherwise both increments are undone (`fetch_sub(1, release)` each) and the thread goes to 6 without popping any queue, because it can hold one record only;
   2. a pop of the high background queue;
   3. when `m_xBackgroundTool` exists: if the tools-host gate passes, a pop of the tool queue; an empty pop undoes the tools-host increment; a failed gate undoes it at once and skips the queue;
   4. a pop of the low background queue.

   Found: run the slice (see "Background jobs and operations"), then `m_iToolBackgroundRunning.fetch_sub(1, release)` when it was a tool slice, then `m_iBackgroundRunning.fetch_sub(1, release)`, and go to 1. Nothing found, or the background gate failed: undo the background increment and go to 6.
6. Idle: spin up to `m_uSpinIterations` iterations of `Zenith_CpuRelax()`, re-running steps 2 and 3 every iteration and step 4 every 32nd iteration; then park (see "Parking and wake-up"). If the stop flag is set with nothing found, exit the loop. After waking, go to 1.
7. Invoke the job (see "Job invocation"), then go to 1.

Step 5's gate implements "if no frame work exists anywhere" of `Zenith2/Docs/Architecture/CPUFrameGraph.md → "Worker loop"` as "the thread's last full sweep of the deques and the injection queue found none": frame work that becomes visible after the sweep is taken at the next step 2 after the current slice, so a slice delays it by at most one slice (Z2-D-006, [Z2-N-ARCH-393]), and at most `m_uMaxConcurrentBackground` (default `N - 1`, at least 1) workers are inside background slices at once, so with N ≥ 2 at least one worker always sweeps frame work. A tools-host slice is the one exception to "at most one slice" ([Z2-N-ARCH-443]): it may run for seconds, so the tools-host gate keeps at most `m_uMaxConcurrentToolBackground` ≤ max(1, ⌊N / 2⌋) workers inside such slices, and every other worker is either free for frame work or at most 2 ms from it ("Tools-host background priority" gives the bounds per N). Both gates use the increment-then-compare pattern: `fetch_add` returns the count before the increment and admits only when it is below the cap, so the cap is never exceeded; a racing worker may see a gate momentarily full and skip it, which costs one pass, never a lost record.

## Main thread helping

The main thread runs this loop inside `Zenith_JobsRunMainUntil` (GRAPH's `RunFrame`), `RunRoot` and `CancelAndDrainBackground` until the loop's completion condition holds (the `uDone` flag GRAPH passes is non-zero; the root has completed; the background in-flight count is 0; for the injected-node drain of `Zenith_JobsEndIteration` and the end of `RunRoot`, `m_iInjectedInFlight` is 0):

1. Pop the main-affinity queue. Found: invoke, go to 1. Main-affinity work always comes first ([Z2-N-ARCH-389]).
2. If the handoff chain is non-empty, invoke its head, go to 1.
3. Pop its own deque (slot 0), then the injection queue, then steal as in the worker loop step 4 (frame work only). Found: invoke, go to 1.
4. `CancelAndDrainBackground` only: pop the high, then the tool (when it exists), then the low background queue, taking neither the background gate nor the tools-host gate (no frame work runs during the drain, so neither cap protects anything). Found: run the slice, go to 1.
5. If the completion condition holds, return.
6. Spin as the worker loop step 6 does (re-checking steps 1, 3 and the completion condition), then park on slot 0's semaphore with the handshake of "Parking and wake-up" (idle bit 0, `seq_cst` fence, re-check including the completion condition). The main thread is woken by any push to the main-affinity queue, by the wake policy of "Parking and wake-up" for ANY work, and by the thread that completes the iteration.

The main thread never runs background work in `Zenith_JobsRunMainUntil` or `RunRoot` ([Z2-N-ARCH-389] "frame work only").

## Render-submit thread

The render-submit thread (index N+1, started when `m_bStartRenderSubmitThread`) applies `Zenith_ApplyFloatEnvironment()` (CORE's thread start has already applied the name `Z2 RenderSubmit` and `m_ulRenderSubmitCpuMask`), then repeats: pop the render-submit queue; found: invoke, go on; not found: spin `m_uSpinIterations` then park; exit on the stop flag. It never pops the injection queue, never steals and never runs background work ([Z2-N-ARCH-390]). The ANY launches its jobs make ready are pushed to the injection queue (FATAL when full, see "Queues and their full policies") and run by frame threads; a launch or continuation that is `JOB_AFFINITY_RENDER_SUBMIT` is taken as its handoff. It never runs an ANY job. Render-submit work is the RENDER subgraph's `Flux.Submit` and `Flux.Present` nodes (`Zenith2/Docs/Architecture/CPUFrameGraph.md → "Frame structure"`): phases 5 Submit and 6 End of `Zenith2/Docs/Rendering/RenderGraph.md → "Graph lifecycle"`, which include the only CPU waits on a GPU timeline ([Z2-N-REND-486]). Command-list recording (phase 4) fans out as sub-jobs of the ANY node `Flux.Record`, so the `ZENITH_CHECK` on `ParallelFor` from a render-submit job loses no legitimate fan-out. Pushing a render-submit job when the thread is not running is `ZENITH_CHECK` ("render-submit affinity with no render-submit thread").

## Parking and wake-up

State: `m_ulIdleMask` (`std::atomic<uint64>`), bit i set while slot i (0..N) is parked or about to park; the render-submit thread has its own flag `m_bRenderSubmitIdle` (`std::atomic<uint32>`).

Park (thread i):

1. `m_ulIdleMask.fetch_or(1 << i, seq_cst)` (the render-submit thread: `m_bRenderSubmitIdle.store(1, relaxed)`).
2. `std::atomic_thread_fence(seq_cst)`.
3. Re-check the sources the thread may take (worker: own deque, injection, one steal sweep, and the background gate; main: its step 1 and 3 sources and its completion condition; render-submit: its queue and the stop flag).
4. Something found: clear the bit (`fetch_and(~(1 << i), relaxed)`; render-submit: `store(0, relaxed)`) and resume.
5. Otherwise count a park, record the park start time and `m_xParkSemaphore.Wait()`. On return, call `Zenith_DispatchBlockingWait("jobs.park", <parked ns>)` once, add the parked time to the idle counter, clear the bit (`relaxed`) and resume.

The handshake rests on the `seq_cst` fence rule of C++20 [atomics.order]: if an atomic modification A of an object M is sequenced before a `seq_cst` fence Y, an atomic load B of M is sequenced after a `seq_cst` fence X, and Y precedes X in the single total order S, then B observes A or a modification of M that follows A in its modification order. The pusher does *queue store → `seq_cst` fence → idle-mask load*; the parker does *idle-mask store → `seq_cst` fence → queue loads*. Whichever fence comes first in S, the other thread's load after its own fence observes the first thread's store: either the pusher sees the idle bit and posts, or the parker's re-check sees the job. Neither side relies on the `seq_cst` of the `fetch_or` alone, which on arm64 would let the parker's relaxed queue loads be satisfied before the bit is visible. The same pair orders `Zenith_JobsWakeMain` (the completing thread's `uDone` release store → fence → mask load) against the main thread's park (bit 0 → fence → `uDone` load).

Wake after a push of ANY work (any pushing thread):

1. `std::atomic_thread_fence(seq_cst)` after the queue's release store (the pusher half of the fence–fence pair above).
2. `mask = m_ulIdleMask.load(relaxed)`; if 0, return.
3. Choose the lowest set bit among workers (bits 1..N); if none, bit 0 (main, which may run ANY work while helping).
4. `if (m_ulIdleMask.fetch_and(~bit, acq_rel) & bit)`: `Post()` that slot's semaphore. A lost race (another waker cleared it) retries step 2 once.

A push to the main-affinity queue always signals slot 0 when bit 0 is set (steps 1, 2, 4 with bit 0 only); a push to the render-submit queue, after the same fence, loads `m_bRenderSubmitIdle` (`relaxed`) and, when it is 1 and `exchange(0, acq_rel)` returns 1, posts the render-submit semaphore. Each slot's `Zenith_Semaphore` is created with initial count 0 and maximum count 2^30; a post that races the parker's own step 4 leaves one stale count, which makes the next `Wait()` return at once, so a wake-up that finds nothing is harmless: the thread re-runs its loop and parks again. A park is a blocking wait outside every job body, one of the three allowed by [Z2-N-ARCH-394], and the Debug job-body mark is clear while it runs (see "Job invocation").

## Job invocation

Invoking record r on thread s:

1. Frame record and `m_ulSeenFrameEpoch` stale: reset the thread's arenas (see "Frame epochs").
2. Build `Zenith_JobContext x(&thread s, r)` on the stack.
3. Cancellation: a `RANGE` record, or a `NODE_STAGE` record of stage 0, whose launch's cancel flag (`*m_puCancel`, acquire load, skipped when `m_puCancel` is null, which only an injected launch may carry) is non-zero skips its function (steps 4–6) and goes to 7.
4. Wherever `ZENITH_ASSERTS_ENABLED=1`: `ZENITH_ASSERT(Zenith_IsExpectedFloatEnvironment(Zenith_ReadFloatEnvironment()))` ([Z2-N-CONTRACT-361]).
5. Begin the zone for a `RANGE` batch, a continuation or a background slice (stage 0 is zoned by GRAPH's trampoline; names per `Zenith2/Docs/Design/JOBS.md → "Zones"`); `Zenith_MarkThreadAsJobWorker(true)`; record the start time for the busy counter.
6. Call the function: `NODE_STAGE`/`ROOT`: `pfn(x, r.m_pData)`; `RANGE`: the range runner of "ParallelFor splitting and inline execution".
7. `Zenith_MarkThreadAsJobWorker(false)`; end the zone; add the elapsed time to the busy counter.
8. Completion accounting: `RANGE`: `CompleteSegmentItem(r.m_pxSegment)`; `NODE_STAGE`/`ROOT`: `OnStageReturned(node, stage)` (see "Stages segments and continuations").

Steps 5 and 7 use CORE's `Zenith_MarkThreadAsJobWorker(bool bRunningJob)`: the flag means "a job body is running on this thread now", so a blocking primitive called from a body fails its check while the scheduler's own park between bodies passes (`Zenith2/Docs/Design/CORE/Threading.md → "Debug thread state"`).

## Stages segments and continuations

A node instance runs as a sequence of stages: stage 0 is the node's body; stage i ≥ 1 is the i-th registered continuation. Sub-jobs are counted in segments: segment 0 opens when the node starts; each `Continue` call closes the current segment and opens a new one.

Rules (they restate and complete `Zenith2/Docs/Architecture/CPUFrameGraph.md → "Continuations"`):

1. Stage i starts when stage i − 1 has returned and every sub-job spawned before continuation i was registered has finished (segment i − 1 has drained; segments below i − 1 drained before stage i − 1 started).
2. Sub-jobs spawned after continuation i was registered belong to a subsequent segment: stage i does not wait for them, the next continuation registered after them does, and node completion does.
3. The node completes when the last stage has returned, no further continuation is registered, and the last segment has drained.
4. `Continue` is called only by a stage (the body or a continuation), never by a range body, so registrations of one node are made by one thread at a time, in program order.

Example: body = `ParallelFor(A); Continue(c1); ParallelFor(B); Continue(c2)`. Stage 1 (`c1`) starts after the body returned and all of A finished; B may still run. Stage 2 (`c2`) starts after `c1` returned and all of B finished. The node completes after `c2` returned and every sub-job spawned after `Continue(c2)` — by the body or by `c1` or `c2` — finished.

State (all in iteration memory, trivially destructible):

```cpp
struct Zenith_JobSegment        // 64-byte aligned
{
	std::atomic<uint32> m_uPending;           // unfinished sub-jobs + 1 while the segment is open (the seal token)
	Zenith_JobContinuation* m_pxSealedBy;      // the continuation whose registration sealed it; null for the last segment
	Zenith_JobNodeState* m_pxNode;
};

struct Zenith_JobContinuation   // 64-byte aligned
{
	std::atomic<uint32> m_uGate;              // 2: the previous stage's return + the sealed segment's drain
	Zenith_PfnJob m_pfnContinuation;
	void* m_pData;
	uint32 m_uStage;                          // 1, 2, …
	Zenith_JobContinuation* m_pxNext;          // registration order; written by the registering stage only
};

struct Zenith_JobNodeState      // 64-byte aligned; one per launch
{
	Zenith_JobNodeLaunch m_xLaunch;            // a copy of the launch GRAPH passed (see "Node launch seam")
	Zenith_JobSegment* m_pxOpenSegment;        // written by the running stage only
	Zenith_JobContinuation* m_pxFirstContinuation;
	Zenith_JobContinuation* m_pxLastContinuation;
	Zenith_JobContinuation* m_pxRunningStage;  // null while stage 0 runs
	bool m_bHeapOwned;                         // true: a Zenith_JobsTryInjectNode block on m_pxHeap, freed by CompleteNode
};
```

Algorithms (each atomic step names its order):

- **Node start** (the thread that pushed the launch, when it creates the node state): allocate segment 0 with `m_uPending = 1`, `m_pxSealedBy = null`; `m_pxOpenSegment = segment 0`.
- **Spawn a sub-job** (`ParallelFor`, from a stage or a range body): `seg = the calling job's segment` (a stage: `node.m_pxOpenSegment`; a range body: its own record's segment); `seg.m_uPending.fetch_add(1, relaxed)` before the push (the relaxed increment is ordered before the push's release, and the counter only needs to be non-zero while the item is queued); the record carries `seg`.
- **CompleteSegmentItem(seg)** (a range job has finished): `if (seg.m_uPending.fetch_sub(1, acq_rel) == 1)` → `OnSegmentDrained(seg)`.
- **Continue(pfn, pData)** (from stage s): allocate `c` with `m_uGate = 2`; append it to the node's list; allocate the new segment `g'` with `m_uPending = 1`; the old open segment `g`: `g.m_pxSealedBy = c` (plain store, published by the release below); `node.m_pxOpenSegment = g'`; then drop g's seal token: `if (g.m_uPending.fetch_sub(1, acq_rel) == 1)` → `OnSegmentDrained(g)`.
- **OnSegmentDrained(seg)**: if `seg.m_pxSealedBy` is non-null: `OpenGate(seg.m_pxSealedBy)`; else (the last segment, sealed at node finish) → `CompleteNode(node)`: call `m_xLaunch.m_pfnOnComplete(x, m_xLaunch.m_pCompleteData)` on this thread; when `m_bHeapOwned`, then free the node's block through `Zenith_Delete` on `m_pxHeap` and do `m_iInjectedInFlight.fetch_sub(1, release)` followed by the `Zenith_JobsWakeMain` pair.
- **OpenGate(c)**: `if (c.m_uGate.fetch_sub(1, acq_rel) == 1)` → dispatch c as stage `c.m_uStage` with the node's affinity (see "Node launch seam"; the thread that opens the gate appends it to its handoff chain when the affinity allows, and otherwise pushes it to its affinity queue).
- **OnStageReturned(node, s)** (the thread that ran stage s): `next` = the continuation after stage s in registration order (the first continuation when s = 0; `m_pxNext` of the running continuation otherwise). If `next` exists: `OpenGate(next)`. Otherwise seal the open segment: `seg = node.m_pxOpenSegment`; `if (seg.m_uPending.fetch_sub(1, acq_rel) == 1)` → `CompleteNode(node)`.

The `acq_rel` decrements form the happens-before chain every rule relies on: a sub-job's writes happen before its decrement, the decrement that reaches zero acquires them, and the thread that then runs the next stage or completes the node therefore observes every write the rules promise. `CompleteNode` calls the launch's completion callback exactly once; what completion means for successors, instances and the iteration is GRAPH's (`Zenith2/Docs/Design/GRAPH.md → "The executor to job-system seam"`).

Cancellation interacts with stages only at the start: a stage-0 record whose launch's cancel flag is set skips the body (so the node registers nothing and completes at once through `OnStageReturned`); continuations of a node that already started still run, and see `IsCancelled()` true, so they can release what the node holds and skip publishing partial results (see "Cancellation semantics").

## ParallelFor splitting and inline execution

`ParallelFor(uCount, uGrain, pfnBody, pData)` from job j:

1. Frame-job check (`ZENITH_ASSERT(!IsBackground())`; in a background job the leaves run inline in ascending order and the call returns).
2. `uCount == 0`: return. `g = max(uGrain, 1)`; `L = Zenith_ParallelForLeafCount(uCount, g)`.
3. Create a `RANGE` record over leaves `[0, L)` (items `[0, uCount)`), spawn it (segment increment), and `TryPush` it on the calling thread's deque (a render-submit job never reaches this step: `ParallelFor` from it is `ZENITH_CHECK`; `m_bFaultInjectNestedRootFull` rejects the push when the caller is a range body). Pushed: wake one parked thread; return.
4. Rejected: `++inline_runs`; run the range runner on the record synchronously inside this call, then `CompleteSegmentItem`; return.

The range runner for a record covering leaves `[lb, le)`:

1. While `le - lb > 1`: `lm = lb + (le - lb) / 2`; create a `RANGE` record over leaves `[lm, le)`, spawn it in the same segment as this record, `TryPush` it; pushed: `le = lm` and continue the loop; rejected: `++inline_runs`, undo the spawn (`fetch_sub`, the undo never reaches zero because this record's own count is still held), and break out of the loop with `[lb, le)` unchanged.
2. For each leaf k in `[lb, le)` in ascending order: if the launch's cancel flag is set, stop; otherwise call `pfnBody(x, k × g, min((k + 1) × g, uCount), pData)`.

Properties:

- **Leaf partition.** Every body call covers exactly one leaf `Zenith_ParallelForLeaf(uCount, g, k)`, whether the leaf ran on a thief, on the pushing thread or inline, so the set of body calls is a function of `(uCount, uGrain)` alone ([Z2-N-ARCH-396], [Z2-N-ARCH-397]).
- **Queue pressure.** A chain of splits pushes at most ⌈log2 L⌉ records per range, so a thread's deque holds O(log L) records per active `ParallelFor`.
- **When nesting happens.** One job runs inside another on the same thread exactly when a `ParallelFor` root push is rejected (step 4), whichever kind of job made the call: a stage, or a range body that itself calls `ParallelFor`. A leaf reached by splitting or stealing never runs inside another body: splitting pushes happen in the range runner before any body call of that record, and a rejected split runs the remaining leaves sequentially in the runner's own loop. The leaf partition stays a pure function of `(uCount, uGrain)` on every path, which is what the per-worker VM contract relies on (`Zenith2/Docs/Design/JOBS.md → "Per-worker Luau VM affinity contract"`).
- **Join.** Every leaf is counted in the segment of the job that called `ParallelFor`, so the node's next stage or its completion waits for it ([Z2-N-ARCH-395]).

Grain rule: in simulation code the grain is a constant or a function of the item count alone, never of `GetWorkerCount()`, so that a caller that merges per-leaf results by leaf index gets identical bytes for every worker count (`Test_JobContext_MergeOrder_IndependentOfWorkers`).

## Node launch seam

GRAPH's frame executor decides when a node is ready and what its completion means; JOBS decides how a ready node reaches a thread (`Zenith2/Docs/Design/GRAPH.md → "The JOBS boundary"`). The seam is the internal header `Zenith2/Engine/ZenithJobs/Internal/Jobs/Zenith_JobNodeLaunch.h`, owned by JOBS; GRAPH's executor is ZenithJobs code and includes it legally ([Z2-N-CONTRACT-024]). JOBS adopts the declarations GRAPH assumed (`Zenith2/Docs/Design/GRAPH.md → "The executor to job-system seam"`) unchanged and adds the calls marked "JOBS addition":

```cpp
// Zenith2/Engine/ZenithJobs/Internal/Jobs/Zenith_JobNodeLaunch.h
using Zenith_PfnNodeComplete = void (*)(Zenith_JobContext& xContext, void* pCompleteData);

struct Zenith_JobNodeLaunch                              // trivially copyable; copied into Zenith_JobNodeState
{
	Zenith_PfnJob m_pfnBody = nullptr;                   // the GRAPH node trampoline
	void* m_pBodyData = nullptr;                         // a GRAPH node instance
	Zenith_PfnNodeComplete m_pfnOnComplete = nullptr;    // called exactly once, see below
	void* m_pCompleteData = nullptr;
	const Zenith_ZoneSite* m_pxZone = nullptr;           // the node's zone; names its sub-job and continuation zones
	const std::atomic<uint32>* m_puCancel = nullptr;    // non-zero: IsCancelled() true in the node, its sub-jobs and continuations
	uint64 m_ulInvocationID = 0;                        // returned by Zenith_JobContext::GetInvocationID(); never 0
	Zenith_JobAffinity m_eAffinity = JOB_AFFINITY_ANY;
};

void Zenith_JobsPushNodes(Zenith_JobSystem& xJobs, Zenith_JobContext* pxPusher,
	Zenith_Span<const Zenith_JobNodeLaunch> axLaunches);
void Zenith_JobsRunMainUntil(Zenith_JobSystem& xJobs, const std::atomic<uint32>& uDone);
void Zenith_JobsWakeMain(Zenith_JobSystem& xJobs);
void Zenith_JobsAssertFloatEnvironment();

// JOBS addition: main thread, before the first launch of an iteration (GRAPH's RunFrame start). Advances the frame
// epoch, marks the main thread as helping and opens the injection window (m_bIterationActive = 1, seq_cst);
// ZENITH_CHECK when an iteration or RunRoot is already in progress. The first call after Create accepts any
// ulFrameIndex (the composition root's first frame is 0, as GRAPH's RunFrame accepts any value on its first call);
// each subsequent call's index is ZENITH_CHECKed to be greater than the previous call's (stored in m_ulIterationFrameIndex;
// m_bIterationFrameIndexSeen marks that a first call happened).
void Zenith_JobsBeginIteration(Zenith_JobSystem& xJobs, uint64 ulFrameIndex);
// JOBS addition: main thread, after the iteration completed (GRAPH's RunFrame end). Closes the injection window
// (m_bIterationActive = 0, seq_cst), helps until every injected node the iteration accepted has completed
// (m_iInjectedInFlight == 0), then publishes the JOBS counters of the iteration and clears the helping mark.
void Zenith_JobsEndIteration(Zenith_JobSystem& xJobs);
// JOBS addition: main thread, between iterations (GRAPH's SetSchedule). Grows the main-affinity queue to
// 2 × uMainNodes + 256 and the render-submit queue to 2 × uRenderSubmitNodes + 64 when they are smaller, each rounded
// up to the next power of two.
void Zenith_JobsReserveAffinityCapacity(Zenith_JobSystem& xJobs, uint32 uMainNodes, uint32 uRenderSubmitNodes);
// JOBS addition (review round 1): main thread, between iterations (GRAPH's SetSchedule). Grows the injection queue to
// max(m_uInjectionCapacity, 2 × uInjectedNodes + 256) rounded up to the next power of two, and sets
// m_uInjectionReserved = 2 × uInjectedNodes + 256; uInjectedNodes = the ANY nodes with a render-submit predecessor.
void Zenith_JobsReserveInjectionCapacity(Zenith_JobSystem& xJobs, uint32 uInjectedNodes);
// JOBS addition: a foreign producer (a thread with no job context: I/O, test and tool threads; never a JOBS thread,
// never the main thread). Pushes one ANY launch to the injection queue inside the current iteration. Having taken
// nothing, returns JOBS_ERROR_NO_ITERATION outside an iteration and JOBS_ERROR_QUEUE_FULL when the foreign share
// is used; the producer keeps the launch in its own storage and retries at its own next wake-up (an I/O
// completion, its loop's next pass), never by spinning on the call: JOBS signals no window.
// An injected launch is frame work of the iteration that accepted it: its body and each continuation obey the
// frame-job rules and are sized like a background slice (at most 2 ms each, [Z2-N-ARCH-393]); the iteration end
// waits for it (jobs.injection.drain_ns). Field contract: m_pfnBody and m_pfnOnComplete non-null (ZENITH_ASSERT);
// m_puCancel may be null (never cancelled: IsCancelled() is false and "Job invocation" step 3 skips the load);
// m_ulInvocationID is ignored and JOBS assigns (3ull << 62) | m_ulInjectCounter; m_pxZone null names the zone
// "injected"; m_eAffinity must be JOB_AFFINITY_ANY (ZENITH_ASSERT).
Zenith_Result<void, Zenith_JobsError> Zenith_JobsTryInjectNode(Zenith_JobSystem& xJobs,
	const Zenith_JobNodeLaunch& xLaunch);
```

### Push order and routing

`Zenith_JobsPushNodes(xJobs, pxPusher, axLaunches)` runs these steps on the calling thread:

1. For each launch, create a `Zenith_JobNodeState` (copying the launch) and its segment 0 in the calling thread's record arena, and a `NODE_STAGE` record for stage 0.
2. Identify the calling thread: `pxPusher` non-null gives its slot through the context; `pxPusher == nullptr` is legal only on the main thread inside an iteration, recognised by `Zenith_GetCurrentThreadID() == m_ulMainThreadID` with `m_bMainHelping` set (slot 0); any other null-pusher call is `ZENITH_CHECK` ("use Zenith_JobsTryInjectNode"). Route each record by `m_eAffinity`: `JOB_AFFINITY_MAIN` → the main-affinity queue; `JOB_AFFINITY_RENDER_SUBMIT` → the render-submit queue (`ZENITH_CHECK` when that thread is not running); `JOB_AFFINITY_ANY` → the calling thread's deque when it is a frame thread (slots 0..N), the injection queue when it is the render-submit thread.
3. Order: the span is sorted by descending critical-path priority (GRAPH's guarantee). For a FIFO queue (main, render-submit, injection) the records are pushed in span order; for the calling thread's LIFO deque they are pushed in reverse span order, except that the first ANY launch becomes the calling thread's handoff when `pxPusher` is non-null and the caller may run ANY work, so the consumer's next take from every destination is the earliest element of the span bound for it ([Z2-N-ARCH-411]). A thief takes from the deque's FIFO end, which holds the lowest-priority launch of the batch: that affects only which thread runs what and when, and the pushing thread, which keeps the critical path, takes the highest-priority launches first.
4. Rejected pushes: a full deque appends the record to the calling thread's handoff chain (an inline run is counted), run by the thread's loop after the current job returns, never nested; a full injection queue from the render-submit thread and a full affinity queue are `ZENITH_FATAL(FATAL_CODE_QUEUE_FULL)` naming the queue.
5. Wake: one parked frame thread per ANY record pushed (see "Parking and wake-up"); slot 0 for main-affinity records; the render-submit thread for render-submit records.
6. Count: `m_iLaunchedNotStarted.fetch_add(|span|, relaxed)` and a relaxed compare-exchange maximum into `m_ulReadyHighWater`; each stage-0 invocation decrements it (relaxed).

`Zenith_JobsTryInjectNode(xJobs, xLaunch)` runs these steps on the calling foreign thread:

1. `ZENITH_CHECK` that the caller is not a JOBS thread (`Zenith_IsJobWorkerThread`) and not the main thread (`m_ulMainThreadID`).
2. Window: `m_iInjectedInFlight.fetch_add(1, seq_cst)`, then `m_bIterationActive.load(seq_cst)`. Clear: `m_iInjectedInFlight.fetch_sub(1, release)` and return `JOBS_ERROR_NO_ITERATION`. This is a Dekker pair with the main thread's *`m_bIterationActive.store(0, seq_cst)` → `m_iInjectedInFlight.load(seq_cst)`* at the start of the drain: in the single total order S either the main thread's load sees the increment (and the drain waits for the node), or the injector's load sees the window closed (and it refuses). No injected node is accepted into an iteration whose drain has already found the count at 0.
3. Share: in signed 64-bit arithmetic, with `share = capacity > m_uInjectionReserved ? capacity − m_uInjectionReserved : 0`, `if (m_iForeignQueued.fetch_add(1, relaxed) >= share)` → `fetch_sub(1, relaxed)` on both counters (`release` for the in-flight one) and return `JOBS_ERROR_QUEUE_FULL`. `m_uFaultInjectInjectionFullAt == K` makes the K-th injection push (from any producer) report full at this point, or, from the render-submit thread, at its push.
4. Allocate one block from `m_pxHeap` holding the node state (`m_bHeapOwned = true`), segment 0 and the stage-0 record (a foreign thread has no arena); allocation failure undoes steps 2 and 3 and returns `JOBS_ERROR_OUT_OF_MEMORY`.
5. `m_ulInjectedAccepted.fetch_add(1, relaxed)`; `TryPush` the record on the injection queue (it cannot be full: step 3 kept the foreign share within bounds, and the share is never negative, see "Queues and their full policies"), wake one parked frame thread, return success.

A thread that pops a record from the injection queue whose node is heap-owned does `m_iForeignQueued.fetch_sub(1, relaxed)` before invoking it. The node completes through `CompleteNode`, which frees the block and decrements `m_iInjectedInFlight` ("Stages segments and continuations"). `Zenith_JobsEndIteration` and `RunRoot` (which sets `m_bIterationActive` at its start and clears it at its end, like an iteration) then drain: after the closing store then run the main-thread helping loop until `m_iInjectedInFlight` is 0, timing that loop into `jobs.injection.drain_ns` (main-thread nanoseconds from the closing store to the count reaching 0, exact), so an injected node, its continuations and its sub-jobs finish inside the iteration that accepted them, before the epoch can advance and before the quiescence point of `Zenith2/Docs/Design/JOBS.md → "Per-worker Luau VM affinity contract"`.

Priority, routing, handoff and steal order change which thread runs a node and when, never what it computes (Z2-D-019).

### Completion

JOBS calls `m_pfnOnComplete` exactly once per launch: after the body has returned, after every sub-job it (or its sub-jobs, transitively) spawned has finished, and after every continuation it registered has finished, on the thread that finished the last of them, with every write of the body, its sub-jobs and its continuations visible to that thread through the `acq_rel` chain of "Stages segments and continuations" ([Z2-N-ARCH-395]). The callback is the last access JOBS makes to the launch's frame memory on that path (see "Lifetimes"). A launch whose cancel flag was set before stage 0 started runs no body and calls the callback at once, which is how GRAPH's cancelled instance still completes every counter ([Z2-N-ARCH-412]).

### Main-thread helping and wake

`Zenith_JobsRunMainUntil(xJobs, uDone)` runs the loop of "Main thread helping" until `uDone.load(acquire)` is non-zero; `Zenith_JobsWakeMain` posts slot 0's semaphore when bit 0 of the idle mask is set (with the fence of "Parking and wake-up"), and GRAPH calls it from the thread that completes the iteration. `Zenith_JobsAssertFloatEnvironment` is the check of "Job invocation" step 4 exposed for GRAPH's trampoline.

### Invocation IDs

`Zenith_JobContext::GetInvocationID()` returns, for a job of a launched node (its stages and sub-jobs), the launch's `m_ulInvocationID`, which GRAPH computes deterministically from the frame, the subgraph instance and the node (`Zenith2/Docs/Design/GRAPH.md → "Algorithms and state machines"`). For a `RunRoot` job it returns `(1ull << 62) | (frame epoch & 0x3FFFFFFFFFFFFFFF)`; for a background job `(1ull << 63) | issueID`, where the issue ID is the operation table's strictly increasing counter. The ID is never 0, and it is used for view validation alone (`Zenith2/Docs/Architecture/ECSModel.md → "Views"`), never for ordering or keying simulation state ([Z2-N-ARCH-884]).

JOBS reserves bits 62 and 63 of the invocation ID for `RunRoot` and background jobs; GRAPH's launch IDs keep both clear, since the frame index is truncated to 34 bits above a 28-bit ordinal and node field (`Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md → "Invocation IDs"`). An injected launch gets `(3ull << 62) | m_ulInjectCounter`, with both bits set: distinct from GRAPH's IDs (both clear), `RunRoot`'s (bit 62 alone) and background IDs (bit 63 alone, as the operation table's issue counter stays below 2^62).

## Background jobs and operations

- **Submit** (`SubmitBackground`, `SubmitBackgroundCopy`; any thread): validate (`pfnJob` non-null, priority HIGH, LOW, or TOOL with `m_bAcceptToolBackground`, name prefix `bg/`, payload ≤ 64 bytes; a TOOL submission to a system without it is refused as "Tools-host background priority" step 1 states); if the shutting-down flag is set, return `JOBS_ERROR_SHUTTING_DOWN`; issue an operation from the operation table (full → `JOBS_ERROR_QUEUE_FULL`); build the record (copy the payload and point `m_pData` at the copy for the copy variant); `TryPush` it on its priority's queue; rejected → complete the operation slot as never started (so the table slot is free again), log `jobs: background queue '<name>' full`, return `JOBS_ERROR_QUEUE_FULL`; pushed → `m_iBackgroundInFlight.fetch_add(1, relaxed)`, wake one parked worker, return the handle. The operation table capacity is `2 × m_uBackgroundCapacity + N + 2`, so a full table implies full queues.
- **Slice** (a worker, gate of the worker loop step 5; the ticket is `m_axTickets[GetSlotID().m_uSlot]`, recorded in the record's `m_uTicketIndex` at submission): if the operation's cancel flag is set before the first slice, complete it `OPERATION_STATUS_CANCELLED` without running anything. Otherwise `SetRunning`, build the context (background, cancellation token = the operation's flag), run the zone `m_szName`, call `pfnJob(x, m_pData)`, measure the slice; a HIGH or LOW slice longer than `ulJOBS_BACKGROUND_SLICE_NS` increments `jobs.bg.slice_overruns` (the 2 ms rule is enforced by review, [Z2-N-ARCH-393]; the counter makes a violation visible); a TOOL slice never does, whatever its length ([Z2-N-ARCH-443]).
- **Outcome**: after the slice returns: if it called `Continue`, re-push a copy of the record with the continuation's function and data at the same priority, a TOOL continuation to the tool queue (`++m_uSliceCount`); a rejected re-push stores it as the thread's held continuation, which the thread runs before any other background work at its next background step. If it did not, complete the operation: `ReportFailed` → `OPERATION_STATUS_FAILED` with the code; `ReportCancelled` → `OPERATION_STATUS_CANCELLED`; neither → `OPERATION_STATUS_SUCCEEDED` ([Z2-N-CONTRACT-337]); then `m_iBackgroundInFlight.fetch_sub(1, release)` followed by the `Zenith_JobsWakeMain` pair (`seq_cst` fence, `m_ulIdleMask.load(relaxed)`, and when bit 0 is set `fetch_and(~1, acq_rel) & 1` → `Post()` slot 0), the waker half of the main thread's park in `CancelAndDrainBackground` (bit 0 → fence → the in-flight count).
- **Progress** is written through the operation slot; the table keeps it monotonic ([Z2-N-CONTRACT-094]).
- Background jobs never call `ParallelFor` into the frame machinery, never touch a world, and never use a script VM; they are for I/O-shaped and cache-shaped work whose result is published at a tick boundary by its owner ([Z2-N-CONTRACT-093]).

### Tools-host background priority

`JOB_PRIORITY_BACKGROUND_TOOL` is the one background priority whose jobs may run past the 2 ms slice of [Z2-N-ARCH-393]: a lightmap tile, a generator region, a BVH build, a whole partition plan, a navigation tile bake. [Z2-N-ARCH-443] (orchestrator ruling X350 (a), human may override) admits it in a tool host alone, on at most `m_uMaxConcurrentToolBackground` workers at once, and excludes it from `jobs.bg.slice_overruns`. This subsection is how ZenithJobs implements that rule.

**The gate is a run-time desc field.** A job system accepts the priority exactly when it was created with `Zenith_JobSystemDesc::m_bAcceptToolBackground = true`, which two composition roots set in code: the editor's (MOD's tool-mode `Zenith_RuntimeToolAccess::Create`, startup step 9) and the cook's (`Zenith_CookServices` item 2, `Zenith2/Docs/Design/ASSET/Cook.md → "Cook services and composition"`). A compile-time gate cannot express "tool hosts only", for four reasons:

1. `ZENITH_TOOLS_HOST` is a value of the CMake `SUPPORTED` matrix naming the configure preset `win` (`Zenith2/Docs/Architecture/BuildSystem.md → "The ZENITH_SUPPORTED matrix"`), not a compile definition, so no C++ source can test it.
2. ZenithJobs is `SUPPORTED ZENITH_ALL` and `zenith_add_library` creates one `ZenithJobs` target per configure preset; the `win` preset links that one library into the Player and into every host tool alike. A per-binary definition would need the library compiled twice in one preset under different definitions, which the helper does not do (`Zenith2/Docs/Architecture/BuildSystem.md → "The zenith_add_library helper"`).
3. A conditional around tools-only behaviour in an engine library is what Zenith2 rules out: tools-only code lives in tooling targets and is never wrapped in a preprocessor conditional ([Z2-N-CONTRACT-425]: "rule F11 forbids such conditionals").
4. The ruling names two hosts, the editor and the cook, while the `ZENITH_TOOLS_HOST` matrix value also covers ZenithShaderCompiler, ZenithNewGame, ZenithDevTool and two tool libraries, so even a working build condition would admit the wrong set.

The field is not a configuration key, so a Player cannot be switched into accepting the priority by `--set`; a Player's composition root never sets it and never maps `jobs.maxConcurrentToolBackground`. It is not a test-mode switch either (`Zenith2/Docs/Contracts.md → "Tests and code hygiene gates"`): it is host configuration, and a test that exercises the priority sets it in the desc it creates, as a tool host does.

**Algorithm.**

1. **Admission** (`SubmitBackground*`, after the null and name checks): `ePriority == JOB_PRIORITY_BACKGROUND_TOOL && !m_bAcceptToolBackground` → if `m_bToolRefusalLogged.exchange(1, relaxed) == 0`, log `jobs: priority JOB_PRIORITY_BACKGROUND_TOOL refused: this job system does not accept tools-host background work` (warning, `jobs.scheduler`, once per job system); return `JOBS_ERROR_INVALID_DESC` having issued nothing and pushed nothing. There is no assert: a tool library linked into a test host that did not opt in gets a refusal it can report, and the refusal is observable in every build. An accepted TOOL submission continues exactly as HIGH and LOW do, with the tool queue as its destination.
2. **Resolution** (`Create` step 1, after N and the background cap are resolved): with `m_bAcceptToolBackground`, `uLimit = min(max(1, ⌊N / 2⌋), m_uMaxConcurrentBackground)`; `m_uMaxConcurrentToolBackground == 0` resolves to `uLimit`; an explicit value outside `[1, uLimit]` is `JOBS_ERROR_INVALID_DESC`; `m_uToolBackgroundCapacity` must be a power of two in [64, 2^22]. Without it, `m_uMaxConcurrentToolBackground` must be 0 (`JOBS_ERROR_INVALID_DESC` otherwise), the capacity is ignored and the resolved cap is 0.
3. **Storage** (`Create` steps 5 and 6): the tool queue `jobs.background_tool` of `m_uToolBackgroundCapacity` cells (REJECT) is created after the low queue, only with acceptance; the operation table has `2 × m_uBackgroundCapacity + T + N + 2` slots with T = `m_uToolBackgroundCapacity` (else 0), and the ticket array the same length, so a full table still implies full queues.
4. **Scheduling**: worker loop step 5 ("Worker loop"): the background gate, then the held continuation, the high queue, the tool queue behind the tools-host gate, the low queue. The main thread runs TOOL slices only inside `CancelAndDrainBackground` ("Main thread helping" step 4).
5. **Slices**: a TOOL slice runs like any background slice (context, zone `bg/<Name>`, `Continue` re-queued to the tool queue, outcomes, invocation ID `(1ull << 63) | issueID`), except that its length is not compared with `ulJOBS_BACKGROUND_SLICE_NS`.

Every step is O(1); the tools-host gate adds one `fetch_add` and at most one `fetch_sub` per worker pass.

**Bounds per worker count** (defaults: background cap `max(1, N − 1)`, tools-host cap `min(max(1, ⌊N / 2⌋), background cap)`):

| N | background cap | tools-host cap | background slots left for HIGH and LOW while every tools-host slot is busy | workers never inside a tool slice |
|---|---|---|---|---|
| 1 | 1 | 1 | 0 | 0 (frame work runs on the main thread) |
| 2 | 1 | 1 | 0 | 1 |
| 3 | 2 | 1 | 1 | 2 |
| 4 | 3 | 2 | 1 | 2 |
| 6 | 5 | 3 | 2 | 3 |
| 8 | 7 | 4 | 3 | 4 |
| 14 | 13 | 7 | 6 | 7 |
| 62 | 61 | 31 | 30 | 31 |

For every N ≥ 2 the explicit range is `[1, ⌊N / 2⌋]` (clamped to the background cap), so at most half the workers run tool slices and at least ⌈N / 2⌉ workers are, at any instant, free for frame work or inside a HIGH or LOW slice that returns within 2 ms; the main thread always helps frame work. N = 1 is the one case where `max(1, ·)` of [Z2-N-ARCH-443] admits a tool slice on the only worker, which then serves frame work only through the main thread for the slice's length; a tool host runs N = 1 only when `jobs.workerCount=1` is forced.

**Pop order, starvation and fairness.**

- HIGH before TOOL: a tick boundary waiting on a HIGH result is never queued behind tool work on a worker that may take either. For N ≥ 3 at the default caps, at least one background slot (column four) can never hold a tool slice, and such a slot frees at least every 2 ms, so HIGH and LOW work keeps draining at the rate of those slots even while every tools-host slot is busy for minutes. For N ≤ 2 (and wherever an explicit background cap equals the tools-host cap) the single background slot is shared: a HIGH or LOW job submitted while a tool slice runs waits until that slice returns, a bound equal to the running tool job's remaining length, which its owner documents. Tool hosts at the default worker count (logical processors − 2) have N ≥ 3 on any machine with at least 5 logical processors.
- TOOL before LOW: LOW is speculative (prefetch, cache warming), and a worker that fails the tools-host gate still pops LOW, so LOW waits only when no background slot outside the tools-host cap exists (N ≤ 2), and then only while the tool queue is non-empty.
- TOOL starvation: tool work waits only behind HIGH. A HIGH backlog never holds the queue forever, because every HIGH job is a 2 ms slice and the backlog drains as its tick-boundary consumers publish; there is no aging.
- Among tool jobs the queue is FIFO: jobs start in submission order, so two operations that each keep at most `2 × GetMaxConcurrentToolBackground()` jobs in flight share the tools-host slots in proportion to their submissions. That in-flight bound is the consumer obligation of every tool operation (WORLD's three operations and AI's navigation bake), and a refused submission (`JOBS_ERROR_QUEUE_FULL`) is retried at the operation's next poll, never by spinning.
- A held TOOL continuation (its re-queue was refused) waits for a tools-host slot; while it waits, its worker runs no other background slice, because a worker holds one record at most. The worker still runs frame work and parks normally.

**Cancellation latency.** JOBS observes a TOOL operation's cancellation at two points only: the slice start (a queued operation whose flag is set completes `OPERATION_STATUS_CANCELLED` without running) and every `IsCancelled()` call the job makes. A running tool job therefore stops within its own longest interval between `IsCancelled()` calls, which its owner states in its contract block ([Z2-N-CONTRACT-338]; for WORLD's bake items, one row of 64 texels). A queued, cancelled tool operation completes when a worker next pops it, which needs a free tools-host slot: when every running tool job was cancelled too (one operation cancelling all its jobs, or `CancelAndDrainBackground`), that is within the longest cancellation interval among them; when another operation's tool jobs keep running, it is within the shortest remaining length among the running tool jobs. `CancelAndDrainBackground` pops the tool queue on the main thread without the gates, so queued tool operations complete at once and shutdown step 4 waits only for the running tool slices to reach a cancellation point.

**Rules for tool jobs.** A TOOL job obeys every background rule except the slice length: it never calls `ParallelFor` (ZENITH_ASSERT, the leaves then run inline), never waits, never touches a world or a script VM, and publishes nothing itself — its owner observes completion through the handle from its own poll. Its arena allocations live until its slice returns, and the arenas keep their 16 MiB per-slot caps, so a tool job that needs more working memory (a BVH over 16,777,216 triangles) allocates it from its owner's storage, never from `GetWorkerArena`. Its results never depend on which worker ran it or how its slices interleaved with others (Z2-D-019).

**Memory.** A tool host adds, at the default `m_uToolBackgroundCapacity` of 1,024: the queue's 1,024 cells of 192 bytes (196,608 bytes), 1,024 operation-table slots of 64 bytes (65,536 bytes) and 1,024 ticket-array entries, all counted in `jobs.memory_bytes`. A Player creates none of them, so the Player figures of `Zenith2/Docs/Design/JOBS.md → "In-memory layouts"`, and every WS-FRAME measurement, contain none of them; no Z2-B-JOBS row bounds the tool queue, whose capacity is the design constant `uJOBS_DEFAULT_TOOL_BACKGROUND_CAPACITY`.

**Diagnostics.** Only where the tool queue exists: `jobs.background_tool.full`, `.dropped` (0) and `.high_water`, and `jobs.bg.tool_running` (workers inside tool slices when the publisher reads `m_iToolBackgroundRunning`, relaxed; may be stale by the slices that start or end during publication). `jobs.bg.completed`, `cancelled`, `failed` and `in_flight` count tool operations with the rest.

## Cancellation semantics

| Kind | Requested by | Observed at | Before the job starts | While it runs | Reports complete | Partial results |
|---|---|---|---|---|---|---|
| frame iteration | GRAPH's `Zenith_FrameExecutor::RequestCancel()`, which sets the flag every launch names in `m_puCancel` | stage 0 of every launched node not yet started; every range leaf not yet started; `IsCancelled()` | the body is skipped and the completion callback is called at once | a running stage, range body or continuation finishes normally; continuations of started nodes still run and see `IsCancelled()` true | every launch's completion callback runs, so GRAPH's counters reach zero and `RunFrame` returns ([Z2-N-ARCH-412]) | frame memory is reclaimed at the next iteration; a node's continuation that sees `IsCancelled()` publishes nothing; the iteration's simulation state is not a valid tick and ZenithRuntime proceeds to shutdown |
| background operation | `Zenith_OperationHandle::RequestCancel()` or handle destruction (any thread, never blocks), or `CancelAndDrainBackground` | the slice start; `IsCancelled()` at the job's own cancellation points, each at most 2 ms apart (a TOOL job's at the intervals its owner documents; latency bounds in "Tools-host background priority") | completes `OPERATION_STATUS_CANCELLED` with nothing run | the job releases what it holds on its own thread, calls `ReportCancelled()`, returns | after the release, as `OPERATION_STATUS_CANCELLED` ([Z2-N-CONTRACT-335]); a job that finished its work first reports `SUCCEEDED` ([Z2-N-CONTRACT-337]) | none remain: the requester owns only the handle, and results only on `SUCCEEDED` ([Z2-N-CONTRACT-336]) |

Each background job kind documents its cancellation points in its own owner's contract block ([Z2-N-CONTRACT-338]); JOBS's own points are the slice start and every `IsCancelled()` call.

## Simulation clock arithmetic

State: `m_ulTickDurationNs = ((1'000'000 + tickHz / 2) / tickHz) × 1000` in `uint64` integer arithmetic (the tick duration rounded to whole microseconds, so `GetTickDurationUs()` = `m_ulTickDurationNs / 1000`: 60 Hz → 16,667 µs); `m_ulAccumulatorNs`; the counters.

`Advance(ulRealDeltaUs)` (REAL_TIME mode):

1. `ulDeltaUs = min(ulRealDeltaUs, m_uMaxFrameDeltaUs)`.
2. Paused: `m_uLastDroppedTicks = 0`; return 0 (the accumulator and alpha are unchanged).
3. `ulScaledNs = m_fTimeScale == 1.0f ? ulDeltaUs × 1000 : (uint64)((double)ulDeltaUs × 1000.0 × (double)m_fTimeScale + 0.5)` (round half up in `double`; the unscaled path is exact integer arithmetic); `m_ulAccumulatorNs += ulScaledNs`.
4. `T = m_ulAccumulatorNs / m_ulTickDurationNs`.
5. If `T > m_uMaxCatchUpTicks`: `dropped = T − m_uMaxCatchUpTicks`; `T = m_uMaxCatchUpTicks`; `m_ulAccumulatorNs %= m_ulTickDurationNs` (the remainder below one tick is kept; the dropped ticks' time is discarded); `m_ulDroppedTicks += dropped`; `m_uLastDroppedTicks = dropped`. Otherwise `m_ulAccumulatorNs -= T × m_ulTickDurationNs`; `m_uLastDroppedTicks = 0`.
6. `m_ulTickIndex += T`; return T.

`GetInterpolationAlpha()` = `(float)((double)m_ulAccumulatorNs / (double)m_ulTickDurationNs)`, which lies in [0, 1) because the accumulator is below one tick after step 5; a result that rounds to 1.0f in `float` is returned as the largest `float` below 1.

`AdvanceExternal(uTicks)` (EXTERNAL_TICKS mode): paused: return 0 and advance nothing. Otherwise `ZENITH_CHECK(uTicks <= m_uMaxCatchUpTicks)`; `m_ulTickIndex += uTicks`; the accumulator stays 0 and alpha 0; return uTicks.

`SetTimeScale(f)`: NaN → `ZENITH_ASSERT`, then 0; otherwise clamp to [0, 8]. `SetPaused(false)` after a pause does not credit the paused real time (it was never accumulated).

Worked values (the `Test_SimulationClock_*` cases): 60 Hz, 60 frames of 16,667 µs → T = 1 each, 60 total, accumulator 0 after each; max catch-up 4, one frame of 1,000,000 µs → clamped to 250,000 µs = 250,000,000 ns = 14 ticks of 16,667,000 ns (233,338,000 ns) with remainder 16,662,000 ns → T = 4, 10 dropped, accumulator 16,662,000 ns (16,662 µs); scale 0.5 over 10 s of 16,667 µs frames → 300 ticks ± 1.

## Floating-point environment application

| Thread | Applied by | When | Checked |
|---|---|---|---|
| main | the composition root (`Zenith_ApplyFloatEnvironment()` before the first frame, [Z2-N-CONTRACT-360]); `Zenith_JobSystem::Create` asserts it | before `Create` returns | at the start of every job it runs |
| workers | the worker entry function, first statement | before the thread reports started | after applying (`ZENITH_ASSERT`) and at the start of every job |
| render-submit | its entry function, first statement | as for workers | as for workers |

A test executable's main thread applies it in the test body that creates the job system. Nothing in ZenithJobs changes the environment after start-up ([Z2-N-CONTRACT-362]).

## Create and failure unwinding

`Zenith_JobSystem::Create(desc)` runs these steps; every step's failure returns its error after the members constructed so far unwind through their owners (`Zenith2/Docs/Contracts/Memory.md → "Fallible construction"`), and each step has its failure-injection test `Test_JobSystem_Create_FailsAt<Step>_Unwinds` (named in `Zenith2/Docs/Design/JOBS/TestCatalogue.md → "JobSystem tests"`):

| # | Step | Failure | Error | Unwinds |
|---|---|---|---|---|
| 1 | `ValidateDesc`: ranges of every field; resolve N (0 → `Zenith_GetHardwareConcurrency()` − 2 clamped to [1, 62]); `m_uMaxConcurrentBackground` (0 → max(1, N − 1)); then the tools-host fields ("Tools-host background priority" step 2: with `m_bAcceptToolBackground`, `m_uMaxConcurrentToolBackground` 0 → `min(max(1, ⌊N / 2⌋), background cap)` and explicit values within `[1, that limit]`, `m_uToolBackgroundCapacity` a power of two in [64, 2^22]; without it, `m_uMaxConcurrentToolBackground` must be 0) | invalid field | `JOBS_ERROR_INVALID_DESC` | nothing |
| 2 | `AllocateState`: `Zenith_TryNew<Zenith_JobSystemState>` | heap | `JOBS_ERROR_OUT_OF_MEMORY` | nothing |
| 3 | `AllocateThreadStates`: the N + 2 slot array | heap | `JOBS_ERROR_OUT_OF_MEMORY` | the state |
| 4 | `AllocateDeques`: slots 0..N | heap | `JOBS_ERROR_OUT_OF_MEMORY` | deques created so far, the slot array, the state |
| 5 | `AllocateQueues`: injection (`m_uInjectionCapacity`), main (256), render-submit (64), background high and low (`m_uBackgroundCapacity` each), then, only with `m_bAcceptToolBackground`, background tool (`m_uToolBackgroundCapacity`) | heap | `JOBS_ERROR_OUT_OF_MEMORY` | queues created so far and everything above |
| 6 | `AllocateOperationTable`: `Zenith_OperationTable::Create` with `2 × m_uBackgroundCapacity + T + N + 2` slots (T = `m_uToolBackgroundCapacity` with `m_bAcceptToolBackground`, else 0), and the ticket array of the same length | heap (`OPERATION_ERROR_OUT_OF_MEMORY` mapped) | `JOBS_ERROR_OUT_OF_MEMORY` | everything above |
| 7 | `StartWorkers`: `Zenith_Thread::Create` for indices 1..N in ascending order, each with `Zenith_ThreadDesc{ "Z2 Worker NN" (two digits), entry, &slot, 1 MiB stack, THREAD_PRIORITY_NORMAL, m_ulWorkerCpuMask }` | thread creation, or `m_uFaultInjectThreadCreate == K` at the K-th creation | `JOBS_ERROR_THREAD_CREATE` | sets the stop flag, wakes and joins the started workers in descending index order, then everything above |
| 8 | `StartRenderSubmit` (when enabled): `Zenith_ThreadDesc{ "Z2 RenderSubmit", entry, &slot, 1 MiB, THREAD_PRIORITY_ABOVE_NORMAL, m_ulRenderSubmitCpuMask }` | as above | `JOBS_ERROR_THREAD_CREATE` | as step 7 |
| 9 | `RegisterProfilerLanes` (when `m_pxProfiler`): one lane per slot, names `Z2 Main`, `Z2 Worker NN`, `Z2 RenderSubmit` | none (lanes are best-effort; failure is logged) | — | — |

CORE's thread start applies the name, priority and mask (a refused mask is logged by CORE in `core.threading` and the thread runs on OS default placement). The JOBS entry function then applies the FP environment, calls the lifecycle callback, and enters its loop; it runs no job until `Create` has returned (the stop flag and queues are ready before the thread starts, and nothing can be pushed before `Create` returns the object).

Memory accounting: `Create` also builds the job system's `Zenith_MemoryConsumer` (`Zenith2/Docs/Architecture.md → "Memory governor"`), subsystem `MEMORY_SUBSYSTEM_OTHER`, whose `GetResidentBytes()` returns `jobs.memory_bytes`: the deques, the five queues (six in a tool host, "Tools-host background priority"), the operation table's slots (64 bytes each, CORE's slot layout, `Zenith2/Docs/Design/CORE/Threading.md → "Zenith_OperationHandle implementation"`) and the ticket array, the thread states and the arena blocks currently held, kept in a relaxed atomic that `Create`, the reserve calls and arena growth and reset update. The bytes are reported through `GetMemoryConsumer()` into `MEMORY_SUBSYSTEM_OTHER`, whose budget includes Z2-B-JOBS-004 (`Zenith2/Docs/Architecture.md → "Memory governor"`, table "Budgets"; the row joins that table through cross-doc X59, pending ARCH); the row itself is measured by MP-SUBMEM, breach BR-GOVERNOR. JOBS's shedding policy under a pressure change is to shed nothing, and it says so rather than pretending otherwise: the queues are fixed by the schedule and the rows, and every arena already returns to its one retained block at each frame boundary, so the only memory it could release is its working set for the next frame. `OnPressureLevelChanged` logs the new level once under `jobs.scheduler`; a breach of Z2-B-JOBS-004 is corrected by re-budgeting the queue capacities (Z2-B-JOBS-005) or the arena caps, never at run time.

## Shutdown sequence

JOBS's part of `Zenith2/Docs/Contracts.md → "Shutdown ownership order"`:

| Contracts step | JOBS action |
|---|---|
| 2 (drain the in-flight frame) | GRAPH's executor drains the frame (`Zenith2/Docs/Design/GRAPH.md → "Threading lifetime and error contract"`); JOBS's part is that every launch of that last iteration has called its completion callback before `Zenith_JobsRunMainUntil` returns, so no frame job is left on any queue |
| 4 (cancel outstanding operations) | `CancelAndDrainBackground()`: set the shutting-down flag (release); request cancel on every live operation slot; help run background work on the main thread, the tool queue included and without the gates, until `m_iBackgroundInFlight == 0` (acquire); a running tool slice ends at its next cancellation point ("Tools-host background priority") |
| 7 (destroy the job system) | the destructor: `ZENITH_CHECK` no iteration in progress and no background operation in flight; set the stop flag (release); post every slot's semaphore and the render-submit semaphore; join the render-submit thread; join workers N, N−1, …, 1 ([Z2-N-ARCH-432]); free queues, deques, arenas, the operation table and the state in reverse creation order |

A worker leaves its loop only at step 6 of "Worker loop" with the stop flag set and nothing found, so a thread never exits while work it could run is queued; the destructor's checks make "nothing is queued" a precondition rather than a race.

## State machines

### Worker thread

| State | Job visible | Nothing after spin | Post | Stop flag with nothing found |
|---|---|---|---|---|
| RUNNING (in the loop, between jobs) | → EXECUTING: invoke | → PARKING: set idle bit | ignored (the semaphore count is consumed at the next park) | → EXITED |
| EXECUTING (inside a job body) | — | — | queued in the semaphore count | — (checked after the job) |
| PARKING (bit set, re-checking) | → RUNNING: clear bit | → PARKED: `Wait()` | → RUNNING | → EXITED |
| PARKED (in `Wait()`) | — | — | → RUNNING: clear bit, add idle time | → RUNNING (then EXITED) |
| EXITED | — | — | — | — |

### Background operation

| State | Popped by a worker | Cancel requested | Slice returns with `Continue` | Slice returns without | Shutdown drain |
|---|---|---|---|---|---|
| QUEUED | cancel flag set → CANCELLED (nothing run); else → RUNNING | flag set (observed at pop) | — | — | flag set |
| RUNNING (a slice executes) | — | flag set; `IsCancelled()` true | → QUEUED (or HELD when the queue is full) | → SUCCEEDED, FAILED or CANCELLED per the report | flag set |
| HELD (the worker's held continuation) | the holding worker runs it next → RUNNING (a TOOL record once the holder takes a tools-host slot) | flag set | — | — | flag set; the holder runs it (it sees the flag) |
| SUCCEEDED, FAILED, CANCELLED | — | ignored (idempotent) | — | — | — |

### Launched node

| State | — | Stage returns, next continuation exists | Stage returns, none | Gate opens | Last segment drains |
|---|---|---|---|---|---|
| READY (queued or handoff, after `Zenith_JobsPushNodes` or an accepted `Zenith_JobsTryInjectNode`) | — | — | — | — | — |
| STAGE_RUNNING(s) | — | → STAGE_WAITING(s+1): open one gate count | → DRAINING: seal the open segment | — | — |
| STAGE_WAITING(s) | — | — | — | → STAGE_RUNNING(s) | — |
| DRAINING | — | — | — | — | → COMPLETE: call `m_pfnOnComplete` once; a heap-owned node then frees its block and decrements `m_iInjectedInFlight` |
| COMPLETE | — | — | — | — | — |

A READY node becomes STAGE_RUNNING(0) when a thread invokes it, or COMPLETE directly (callback called, no body run) when its launch's cancel flag is set. The WAITING state before readiness is GRAPH's (dependency counters).

## Complexity

| Operation | Time | Memory |
|---|---|---|
| deque push, pop | O(1) wait-free | none |
| deque steal | O(1) lock-free, one compare-exchange | none |
| MPMC push, pop | O(1) lock-free | none |
| `ParallelFor` over L leaves | O(L) body calls, O(L) records in total, O(log L) queued per split chain | 64 bytes per record |
| `Continue` | O(1) | one continuation + one segment (128 bytes) |
| `Zenith_JobsPushNodes` of R launches | O(R) | one node state, one segment and one record per launch (≈ 256 bytes) |
| wake-up | O(1) | none |
| steal sweep | O(N) attempts | none |
| counter publication | O(N + queues) per iteration | none |
| `Advance` | O(1) | none |
