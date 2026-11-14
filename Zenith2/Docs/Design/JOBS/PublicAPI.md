# JOBS design — public API

This file is part of the JOBS design family. The entry document is `Zenith2/Docs/Design/JOBS.md`; this file holds the complete public API of the JOBS-owned headers of `Zenith2/Engine/ZenithJobs/`, header by header, each type with its contract block in the grammar of `Zenith2/Docs/Contracts.md → "Contract blocks per type"`. The GRAPH-owned headers of the same library (`Zenith_FrameGraph.h`, `Zenith_FrameSchedule.h`, `Zenith_FrameExecutor.h`, `Zenith_FixedOrderReduce.h`) are specified by `Zenith2/Docs/Design/GRAPH.md → "Public API"`; this file uses none of their types. The one JOBS interface GRAPH's executor calls is internal: `Zenith2/Docs/Design/JOBS/Algorithms.md → "Node launch seam"`.

Every declaration below is the one the implementation writes, with the names and parameter names shown. Types these headers use and do not declare: `uint8`…`uint64` (`ZenithCore/Zenith_Types.h`), `Zenith_Result`, `Zenith_Err` (`ZenithCore/Zenith_Result.h`), `Zenith_Owned` (`ZenithCore/Zenith_Owned.h`), `Zenith_Heap` (`ZenithCore/Zenith_Heap.h`), `Zenith_Arena`, `Zenith_Construct` (`ZenithCore/Zenith_Arena.h`), `Zenith_Span` (`ZenithCore/Zenith_Span.h`), `Zenith_OperationHandle` (`ZenithCore/Zenith_OperationHandle.h`), and `Zenith_ByteSpan` (ZenithCore).

## Header list

| Header (all under `Zenith2/Engine/ZenithJobs/`) | Declares | Includes |
|---|---|---|
| `Zenith_JobsErrors.h` | `Zenith_JobsError`, `Zenith_ToString(Zenith_JobsError)` | `ZenithCore/Zenith_Types.h` |
| `Zenith_JobTypes.h` | `Zenith_JobAffinity`, `Zenith_JobPriority`, `Zenith_PfnJob`, `Zenith_PfnParallelForBody`, `Zenith_ParseCpuMask`, the constants of "Limits and constants" | `Zenith_JobsErrors.h`, `ZenithCore/Zenith_Types.h`, `ZenithCore/Zenith_Result.h` |
| `Zenith_JobContext.h` | `Zenith_CancellationToken`, `Zenith_JobContext`, `Zenith_ParallelForRange`, `Zenith_ParallelForLeafCount`, `Zenith_ParallelForLeaf` | `Zenith_JobTypes.h`, `ZenithCore/Zenith_Arena.h`, `<type_traits>` |
| `Zenith_JobSystem.h` | `Zenith_PfnJobThreadLifecycle`, `Zenith_JobSystemDesc`, `Zenith_JobSystem` | `Zenith_JobsErrors.h`, `Zenith_JobTypes.h`, `ZenithCore/Zenith_Result.h`, `ZenithCore/Zenith_Owned.h`, `ZenithCore/Zenith_OperationHandle.h`, `ZenithCore/Zenith_Span.h` |
| `Zenith_SimulationClock.h` | `Zenith_SimulationClockMode`, `Zenith_SimulationClockDesc`, `Zenith_SimulationClock`, `Zenith_ValidateSimulationClockDesc` | `Zenith_JobsErrors.h`, `ZenithCore/Zenith_Result.h` |
| `Zenith_FloatEnvironment.h` | `Zenith_FloatEnvironmentState`, `Zenith_ApplyFloatEnvironment`, `Zenith_ReadFloatEnvironment`, `Zenith_GetExpectedFloatEnvironment`, `Zenith_IsExpectedFloatEnvironment` | `ZenithCore/Zenith_Types.h` |

Every header forward-declares a type it names only by pointer or reference (`class Zenith_Profiler;`, `class Zenith_Heap;`, `class Zenith_JobContext;`, `class Zenith_MemoryConsumer;`) and includes nothing else. `Zenith_JobsErrors.h` is shared with GRAPH, which adds its frame-graph error declarations to it declaration-wise (`Zenith2/Docs/Design/GRAPH.md → "Frame-graph errors in Zenith_JobsErrors.h"`); the declarations below are JOBS's part.

## Zenith_JobsErrors header

`Zenith_Result` accepts either an error enumeration or a trivially copyable struct of at most 32 bytes with an `m_eCode` enumeration ([Z2-N-CONTRACT-480], [Z2-N-CONTRACT-481]). JOBS chooses the enumeration form as a design decision: every JOBS failure is fully described by its code, the queue name that a context struct could carry is logged at the point of failure instead ([Z2-N-CONTRACT-464]), and `Zenith_GetErrorCode(eError)` returns the enumeration itself. There is no `Zenith_JobsErrorCode` type; `Zenith2/Docs/Architecture/CPUFrameGraph.md → "Frame graph builder"` declares the same enumeration. JOBS appends `JOBS_ERROR_NO_ITERATION` (value 6) for a foreign injection outside an iteration; Architecture's enumeration takes it through the cross-doc queue (X56), and values are never reused.

```cpp
/// @contract enum Zenith_JobsError
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. A value type; safe everywhere.
/// Invalidation: None.
/// Units: None.
/// Errors: This is the error enumeration of ZenithJobs. JOBS_ERROR_THREAD_CREATE: Create could not start a worker
///   or the render-submit thread. JOBS_ERROR_QUEUE_FULL: SubmitBackground or SubmitBackgroundCopy found its
///   background queue full, or the operation table full; Zenith_JobsTryInjectNode found the foreign share of the
///   injection queue used. JOBS_ERROR_OUT_OF_MEMORY: the system's own storage could not be allocated.
///   JOBS_ERROR_INVALID_DESC: a desc field is outside its range (see its field comment), or a submission argument is
///   invalid (JOB_PRIORITY_FRAME, a payload above 64 bytes, or JOB_PRIORITY_BACKGROUND_TOOL submitted to a job system
///   created without m_bAcceptToolBackground). JOBS_ERROR_SHUTTING_DOWN: a
///   background submission after CancelAndDrainBackground started. JOBS_ERROR_NO_ITERATION: Zenith_JobsTryInjectNode
///   was called while no iteration (RunFrame or RunRoot) is active; the caller keeps the launch and retries at its
///   own next wake-up, never by spinning on the call (JOBS signals no window).
/// Version: since Z2 Stage 2. Enumerator values never change meaning once shipped.
/// @endcontract
enum Zenith_JobsError : uint8
{
	JOBS_ERROR_THREAD_CREATE = 1,
	JOBS_ERROR_QUEUE_FULL,
	JOBS_ERROR_OUT_OF_MEMORY,
	JOBS_ERROR_INVALID_DESC,
	JOBS_ERROR_SHUTTING_DOWN,
	JOBS_ERROR_NO_ITERATION,
};

constexpr const char* Zenith_ToString(Zenith_JobsError eError);
	// "JOBS_ERROR_THREAD_CREATE", … exactly the enumerator spelling; "JOBS_ERROR_<unknown>" for any other value
```

## Zenith_JobTypes header

### Limits and constants

```cpp
/// @contract functions Zenith_JobLimits
/// Ownership: constexpr constants; no state.
/// Thread affinity: IMMUTABLE. Compile-time constants.
/// Invalidation: None.
/// Units: As each comment states: threads, bytes, nanoseconds, jobs, pause iterations.
/// Errors: None; Create and the submit functions validate against them.
/// Version: since Z2 Stage 2. Raising a limit is an API change; lowering one is a breaking change.
/// @endcontract
inline constexpr uint32 uJOBS_MAX_WORKERS = 62;                 // worker indices 1..62; 0 = main; 63 = render-submit at most
inline constexpr uint32 uJOBS_MAIN_THREAD_INDEX = 0;
inline constexpr uint32 uJOBS_BACKGROUND_PAYLOAD_BYTES = 64;    // inline payload of SubmitBackgroundCopy
inline constexpr uint64 ulJOBS_BACKGROUND_SLICE_NS = 2000000;   // the 2 ms slice of [Z2-N-ARCH-393]
inline constexpr uint32 uJOBS_MIN_DEQUE_CAPACITY = 64;
inline constexpr uint32 uJOBS_MAX_DEQUE_CAPACITY = 1u << 20;
inline constexpr uint32 uJOBS_MIN_QUEUE_CAPACITY = 64;
inline constexpr uint32 uJOBS_MAX_QUEUE_CAPACITY = 1u << 22;
inline constexpr uint32 uJOBS_MAX_SPIN_ITERATIONS = 1000000;
inline constexpr uint32 uJOBS_DEFAULT_TOOL_BACKGROUND_CAPACITY = 1024;   // tools-host background queue cells (a design
                                                                         // constant of tool hosts; no Z2-B-JOBS row bounds it)
```

### Affinity and priority

```cpp
/// @contract enum Zenith_JobAffinity
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. A value type; safe everywhere.
/// Invalidation: None.
/// Units: None.
/// Errors: None; an out-of-range value passed to the job system is ZENITH_ASSERT.
/// Version: since Z2 Stage 2. The three classes are closed; a new class is an API change.
/// @endcontract
enum Zenith_JobAffinity : uint8
{
	JOB_AFFINITY_ANY,             // any worker, or the main thread while it helps inside RunFrame or RunRoot
	JOB_AFFINITY_MAIN,            // the main thread (index 0): SDL platform pump, each world's main Luau VM
	JOB_AFFINITY_RENDER_SUBMIT,   // the render-submit thread (index N+1): queue submission and present
};

/// @contract enum Zenith_JobPriority
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. A value type; safe everywhere.
/// Invalidation: None.
/// Units: None.
/// Errors: None; JOB_PRIORITY_FRAME passed to SubmitBackground is ZENITH_ASSERT and JOBS_ERROR_INVALID_DESC;
///   JOB_PRIORITY_BACKGROUND_TOOL passed to a job system created without m_bAcceptToolBackground is
///   JOBS_ERROR_INVALID_DESC with nothing taken (no assert: Algorithms.md "Tools-host background priority").
/// Version: since Z2 Stage 2. JOB_PRIORITY_BACKGROUND_TOOL since Z2 Stage 2 (ruling X350 (a), [Z2-N-ARCH-443]).
/// @endcontract
enum Zenith_JobPriority : uint8
{
	JOB_PRIORITY_FRAME,           // frame-graph nodes, their sub-jobs and continuations; never submitted directly
	JOB_PRIORITY_BACKGROUND_HIGH, // asynchronous work whose result a tick boundary is waiting to publish
	JOB_PRIORITY_BACKGROUND_LOW,  // prefetch, cache warming, PSO precache of predicted sets
	JOB_PRIORITY_BACKGROUND_TOOL, // tool hosts alone (the editor, the cook): a job that may run past the 2 ms slice,
	                              // on at most GetMaxConcurrentToolBackground() workers at once ([Z2-N-ARCH-443])
};

class Zenith_JobContext;

/// @contract functions Zenith_JobFunctionTypes
/// Ownership: Function pointers; the pointee is code with static lifetime.
/// Thread affinity: IMMUTABLE. The pointer values are constants; the functions run on the thread the job system picks.
/// Invalidation: None.
/// Units: uBegin and uEnd are item indices of a ParallelFor range, uBegin < uEnd.
/// Errors: A job function reports nothing through its return; it never throws, waits or aborts (see Zenith_JobContext).
/// Version: since Z2 Stage 2.
/// @endcontract
using Zenith_PfnJob = void (*)(Zenith_JobContext& xContext, void* pData);
using Zenith_PfnParallelForBody = void (*)(Zenith_JobContext& xContext, uint32 uBegin, uint32 uEnd, void* pData);
```

### CPU masks

CPU masks are the `uint64` affinity masks of CORE's `Zenith_ThreadDesc::m_ulAffinityMask` (bit i = logical processor i; 0 = OS default placement), which CORE's `Zenith_Thread::Create` applies best-effort and logs on refusal (`Zenith2/Docs/Design/CORE/Threading.md → "Threads"`). The configuration spelling (`jobs.workerCpuMask`, `jobs.renderSubmitCpuMask`) is a lower-case hexadecimal string of 1 to 16 digits without prefix, least significant digit last (`"fffc"` selects processors 2–15); the composition root parses it with the function below, and an unparsable value stops startup with exit code 2 naming the key (`Zenith2/Docs/Architecture/CompositionRoot.md → "Exit codes"`).

```cpp
/// @contract functions Zenith_CpuMaskParsing
/// Ownership: A pure function; no state.
/// Thread affinity: ANY_THREAD. Pure computation.
/// Invalidation: None.
/// Units: Bit i of the result is logical processor i.
/// Errors: JOBS_ERROR_INVALID_DESC for a null or empty string, a character outside [0-9a-f], or more than 16 digits.
/// Version: since Z2 Stage 2.
/// @endcontract
Zenith_Result<uint64, Zenith_JobsError> Zenith_ParseCpuMask(const char* szHex);
```

## Zenith_JobContext header

### Zenith_CancellationToken

```cpp
/// @contract value Zenith_CancellationToken
/// Ownership: A borrowed view of a cancellation source: the frame executor's cancel flag named by the node launch
///   (frame jobs), or the operation ticket the job system holds (background jobs); copying never extends its lifetime.
/// Thread affinity: ANY_THREAD. IsCancellationRequested may be polled from any thread holding the token.
/// Invalidation: Valid until the job that obtained it returns (frame jobs: the node instance completes; background
///   jobs: the operation reports complete). A default-constructed token is never cancelled.
/// Units: None.
/// Errors: None; reading a token after invalidation is a programmer error the sanitizers catch.
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_CancellationToken
{
public:
	constexpr Zenith_CancellationToken() = default;
	bool IsCancellationRequested() const;   // one acquire load; false for a null token
private:
	friend class Zenith_JobContext;
	constexpr Zenith_CancellationToken(const void* pSource, uint8 uKind) : m_pSource(pSource), m_uKind(uKind) {}
	const void* m_pSource = nullptr;   // kind 1: const std::atomic<uint32>* (launch cancel flag); kind 2: Zenith_OperationTicket*
	uint8 m_uKind = 0;                // 0: null token, never cancelled
};
```

### Zenith_JobContext

```cpp
/// @contract type Zenith_JobContext
/// Ownership: Constructed by the job system on the executing thread's stack for one job invocation (a node stage,
///   a ParallelFor leaf batch, a continuation, a background slice) and destroyed when that invocation returns; never
///   copied, moved or stored. It borrows the executing thread's state (deque, arenas, steal generator, counters).
/// Thread affinity: OWNER_THREAD. Valid only on the thread and inside the invocation it was passed to.
/// Invalidation: Every reference it hands out (the arenas) follows the lifetimes in the member comments.
///   The context itself is invalid after the invocation returns.
/// Units: Worker indices are thread slots (0 main, 1..N workers, N+1 render-submit). uCount and uGrain are item counts.
/// Errors: Misuse is a programmer error: ZENITH_ASSERT where noted, and in every build the misused call does nothing.
/// Reentrancy: FULL. ParallelFor may invoke its body synchronously (the inline path); every member may be called from
///   inside that body, on that body's own context, and no member holds state across the invocation.
/// Version: since Z2 Stage 2. Source API.
/// Example: Zenith2/Engine/ZenithJobs/Tests/Examples/Zenith_JobContext_Example.cpp
/// @endcontract
class Zenith_JobContext
{
public:
	Zenith_JobContext(const Zenith_JobContext&) = delete;
	Zenith_JobContext& operator=(const Zenith_JobContext&) = delete;

	uint32 GetWorkerIndex() const;        // 0 = main thread, 1..N workers, N+1 render-submit
	uint32 GetWorkerCount() const;        // N, as Zenith_JobSystem::GetWorkerCount
	Zenith_JobPriority GetPriority() const; // FRAME, or the operation's HIGH, LOW or TOOL priority
	bool IsBackground() const;              // true for HIGH, LOW and TOOL

	// Sub-jobs. Frame jobs only (ZENITH_ASSERT in a background job; the call then runs the body inline).
	// Schedules pfnBody over [0, uCount) in the leaves of Zenith_ParallelForLeaf(uCount, uGrain, …) and returns at once;
	// the node does not complete until every leaf has run. uCount == 0 does nothing. uGrain == 0 is ZENITH_ASSERT and
	// is treated as 1. pData must outlive the node instance (node arena or registrant memory, never the caller's stack).
	// When the calling thread's deque is full the call processes every leaf itself, in ascending order, before it
	// returns (counted in jobs.inline_runs); a caller therefore tolerates its body running synchronously inside the call,
	// whether the caller is a stage or a range body. Calling it from a render-submit job is ZENITH_CHECK: render-submit
	// work never fans out (Algorithms.md "Queues and their full policies").
	void ParallelFor(uint32 uCount, uint32 uGrain, Zenith_PfnParallelForBody pfnBody, void* pData);

	// Continuations. Callable from a node body or a continuation (ZENITH_ASSERT from a ParallelFor body; ignored there).
	// Frame job: registers stage i+1 of the node, run after the registering stage returns and after every sub-job
	// spawned before this call has finished (Algorithms.md "Stages segments and continuations").
	// Background job: re-submits pfnContinuation at the same priority after the current slice returns; the operation
	// completes when a slice returns without registering a continuation. At most one continuation per background slice.
	void Continue(Zenith_PfnJob pfnContinuation, void* pData);

	bool IsCancelled() const;               // frame: the executor's cancel; background: the operation's
	Zenith_CancellationToken GetCancellationToken() const;

	// Background jobs only (ZENITH_ASSERT in a frame job; ignored there). Idempotent; the last call before completion wins.
	void SetProgress(float fProgress);                    // clamped to [0, 1]; monotonic (a lower value is ignored)
	void ReportFailed(uint16 uFailureCode);              // the operation completes OPERATION_STATUS_FAILED (code != 0)
	void ReportCancelled();                               // the operation completes OPERATION_STATUS_CANCELLED

	// Arenas of the executing thread. OWNER_THREAD: use only the arena of your own context; a sub-job never uses the
	// arena of the context that spawned it.
	// Frame jobs: memory stays valid until the owning node instance has completed; it is reclaimed at the next
	// frame boundary. Background jobs: memory stays valid until the current slice returns.
	Zenith_Arena& GetNodeArena();
	Zenith_Arena& GetWorkerArena();         // same lifetime; separate accounting (jobs.arena.*)

	// Copies a trivially copyable, trivially destructible capture into the node arena and returns it; use its address
	// as the pData of ParallelFor or Continue.
	template<typename TCapture>
	TCapture* AllocateCapture(const TCapture& xCapture);

	uint64 GetInvocationID() const;       // Algorithms.md "Invocation IDs"; never 0

private:
	friend class Zenith_JobSystem;
	friend class Zenith_JobThreadLoop;                            // Internal
	Zenith_JobContext(void* pThread, void* pJob);        // Internal state is type-erased in this header
	void* m_pThread = nullptr;                                    // Zenith_JobThreadState (Internal)
	void* m_pJob = nullptr;                                       // Zenith_JobRecord or Zenith_BackgroundRecord (Internal)
};

template<typename TCapture>
TCapture* Zenith_JobContext::AllocateCapture(const TCapture& xCapture)
{
	static_assert(std::is_trivially_copyable_v<TCapture>, "job captures are trivially copyable");
	static_assert(std::is_trivially_destructible_v<TCapture>, "job captures are trivially destructible (Contracts.md, Ownership and allocation)");
	return Zenith_Construct<TCapture>(GetNodeArena(), xCapture);
}
```

### ParallelFor leaf partition

The leaf partition of a `ParallelFor` is a pure function of `uCount` and `uGrain`; it never depends on the worker count, on which thread runs a leaf, or on whether the call ran inline. Each `pfnBody` call receives exactly one leaf.

```cpp
/// @contract value Zenith_ParallelForRange
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. A value type; safe everywhere.
/// Invalidation: None.
/// Units: Item indices, half-open [m_uBegin, m_uEnd).
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_ParallelForRange
{
	const uint32 m_uBegin = 0;
	const uint32 m_uEnd = 0;
};

/// @contract functions Zenith_ParallelForPartition
/// Ownership: Pure functions; no state.
/// Thread affinity: ANY_THREAD. Pure computations.
/// Invalidation: None.
/// Units: Item indices and counts.
/// Errors: uGrain == 0 is treated as 1; uLeafIndex >= the leaf count is ZENITH_ASSERT and returns an empty range.
/// Version: since Z2 Stage 2. The partition formula is part of the determinism contract: changing it is an API change.
/// @endcontract

constexpr uint32 Zenith_ParallelForLeafCount(uint32 uCount, uint32 uGrain)
{
	const uint32 uG = uGrain == 0 ? 1u : uGrain;
	return uCount == 0 ? 0u : static_cast<uint32>((static_cast<uint64>(uCount) + uG - 1) / uG);
}

constexpr Zenith_ParallelForRange Zenith_ParallelForLeaf(uint32 uCount, uint32 uGrain, uint32 uLeafIndex);
	// { uLeafIndex * g, min((uLeafIndex + 1) * g, uCount) } with g = max(uGrain, 1), computed in uint64
```

A body that needs a per-leaf result slot computes its leaf index as `uBegin / max(uGrain, 1)` and writes into an array of `Zenith_ParallelForLeafCount(uCount, uGrain)` slots allocated before the call; a continuation then folds the slots in index order, or with GRAPH's fixed-order tree (`Zenith2/Docs/Design/GRAPH.md → "Zenith_FixedOrderReduce.h"`), so the combine order is fixed by the leaf partition alone.

## Zenith_JobSystem header

### Zenith_JobSystemDesc

```cpp
class Zenith_Profiler;   // ZenithProfiling; optional

/// @contract functions Zenith_JobThreadLifecycleCallback
/// Ownership: A function pointer and user data supplied by the composition root (a crash reporter's thread
///   registration, a test recorder); the job system stores them for its lifetime.
/// Thread affinity: IMMUTABLE. The pointer is fixed at Create; the callback runs on the JOBS thread it describes.
/// Invalidation: None.
/// Units: uThreadIndex is a thread slot (1..N workers, N+1 render-submit).
/// Errors: None. The callback runs outside every job body, may not call the job system, and must not block
///   for longer than it takes to register the thread with its owner.
/// Version: since Z2 Stage 2.
/// @endcontract
using Zenith_PfnJobThreadLifecycle = void (*)(uint32 uThreadIndex, const char* szThreadName, bool bStarted, void* pUserData);

/// @contract value Zenith_JobSystemDesc
/// Ownership: Plain value copied by Create; the pointers are borrowed and outlive the job system.
/// Thread affinity: IMMUTABLE. Read once by Create.
///   Members non-const: callers default-construct it and set the fields they need.
/// Invalidation: None.
/// Units: Capacities in jobs; spin iterations in pause-instruction loops.
/// Errors: Create returns JOBS_ERROR_INVALID_DESC for any field outside the range in its comment.
/// Version: since Z2 Stage 2. New fields are appended with defaults that keep old behaviour.
/// @endcontract
struct Zenith_JobSystemDesc
{
	uint32 m_uWorkerCount = 0;               // 0 = logical processors - 2, clamped to [1, 62]; explicit 1..62
	uint32 m_uDequeCapacity = 4096;          // per thread, power of two in [64, 2^20]
	uint32 m_uInjectionCapacity = 16384;     // minimum injection capacity, power of two in [64, 2^22]; grown from the
	                                          // schedule to a power of two, whose last 2 × I + 256 cells the frame producer reserves
	uint32 m_uBackgroundCapacity = 8192;     // per background priority, power of two in [64, 2^22]
	uint32 m_uSpinIterations = 2000;         // before an idle thread parks, [0, 10^6]
	uint32 m_uMaxConcurrentBackground = 0;   // 0 = max(1, N - 1); explicit 1..N: workers running background at once
	bool m_bAcceptToolBackground = false;     // true only in a tool host's composition root (the editor, the cook):
	                                          // admits JOB_PRIORITY_BACKGROUND_TOOL; false: that priority is refused
	uint32 m_uMaxConcurrentToolBackground = 0;   // with m_bAcceptToolBackground: 0 = min(max(1, N / 2), resolved
	                                          // m_uMaxConcurrentBackground); explicit 1..min(max(1, N / 2), resolved
	                                          // m_uMaxConcurrentBackground); without m_bAcceptToolBackground it must be 0
	uint32 m_uToolBackgroundCapacity = uJOBS_DEFAULT_TOOL_BACKGROUND_CAPACITY;   // with m_bAcceptToolBackground: the
	                                          // tools-host queue's cells, power of two in [64, 2^22]; without it: ignored
	bool m_bStartRenderSubmitThread = true;   // false in the headless configuration
	uint64 m_ulWorkerCpuMask = 0;            // 0 = OS default placement (the default on every platform)
	uint64 m_ulRenderSubmitCpuMask = 0;      // 0 = OS default placement
	Zenith_Heap* m_pxHeap = nullptr;          // general heap for the system's own storage; nullptr = the root heap
	Zenith_Profiler* m_pxProfiler = nullptr;  // optional: thread lanes for the on-screen profiler; nullptr = none
	Zenith_PfnJobThreadLifecycle m_pfnThreadLifecycle = nullptr;   // optional: called on each JOBS thread at start and exit
	void* m_pThreadLifecycleUserData = nullptr;
	uint32 m_uFaultInjectThreadCreate = 0;   // fault injection: 0 = off; K = the Kth thread creation fails
	uint32 m_uFaultInjectBackgroundFullAfter = 0;   // fault injection: 0 = off; K = the Kth and every following background pushes fail
	bool m_bFaultInjectPauseBackground = false;      // fault injection: workers take no background record
	uint32 m_uFaultInjectInjectionFullAt = 0;       // fault injection: 0 = off; K = the Kth injection push alone is reported full
	bool m_bFaultInjectNestedRootFull = false;       // fault injection: a ParallelFor called from a range body finds its root push rejected
};
```

The composition root fills the desc from configuration: `jobs.workerCount` → `m_uWorkerCount`, `jobs.spinIterations` → `m_uSpinIterations`, `jobs.maxConcurrentBackground` → `m_uMaxConcurrentBackground`, `jobs.workerCpuMask` and `jobs.renderSubmitCpuMask` → the two masks through `Zenith_ParseCpuMask`, `runtime.headless` → `!m_bStartRenderSubmitThread` (`Zenith2/Docs/Architecture/CompositionRoot.md → "Startup sequence"`, step 9). `m_bAcceptToolBackground` is not a configuration key: it is set to `true` in code by the two tool hosts' composition roots alone — the editor's, through MOD's tool-mode `Zenith_RuntimeToolAccess::Create` at its step 9, and the cook's, `Zenith_CookServices` item 2 — and stays `false` in every Player, test host that does not set it, and other tool; only a composition root that sets it also maps `jobs.maxConcurrentToolBackground` → `m_uMaxConcurrentToolBackground` (a Player never reads that key, so no `--set` can enable the priority). Why the gate is a run-time desc field and not a build condition, and what the three fields do, is `Zenith2/Docs/Design/JOBS/Algorithms.md → "Tools-host background priority"`. The capacity fields keep their defaults until Z2-B-JOBS-005 "Job queue capacities" is allocated (`Zenith2/Docs/Design/JOBS.md → "Rows JOBS owns"`); the composition root then fills them from the row. The callback points downward in the layering DAG: ZenithJobs declares its type and the higher library supplies it (`Zenith2/Docs/Contracts.md → "Layering DAG"`, [Z2-N-CONTRACT-175]). It is called with `bStarted = true` after the thread has applied the FP environment (CORE's thread start has already applied the name, priority and mask), and with `bStarted = false` as the last statement before the thread function returns. The five `m_uFaultInject…`/`m_bFaultInject…` fields are runtime fields present in every configuration; tests set them, the Player never does (`Zenith2/Docs/Contracts.md → "Tests and code hygiene gates"`, no test-mode switch).

### Zenith_JobSystem

```cpp
/// @contract type Zenith_JobSystem
/// Ownership: Created by Zenith_JobSystem::Create as a Zenith_Owned<Zenith_JobSystem>, owned by the composition root
///   (ZenithRuntime) for the process lifetime; its storage comes from m_pxHeap. It owns its N worker threads and,
///   when started, the render-submit thread, and joins them in its destructor.
/// Thread affinity: ANY_THREAD_EXCEPT(Create, RunRoot, CancelAndDrainBackground, ~Zenith_JobSystem). Those four run
///   on the main thread (the thread that called Create), outside every job body and never during RunFrame.
/// Invalidation: Operation handles it issued stay valid until they are destroyed; destroying the job system while an
///   operation is incomplete is ZENITH_CHECK. The thread indices it assigns never change.
/// Units: Counts of threads and jobs.
/// Errors: Create returns Zenith_Result with JOBS_ERROR_INVALID_DESC, JOBS_ERROR_OUT_OF_MEMORY or
///   JOBS_ERROR_THREAD_CREATE and has then joined every thread it started. SubmitBackground returns
///   JOBS_ERROR_QUEUE_FULL, JOBS_ERROR_SHUTTING_DOWN or JOBS_ERROR_INVALID_DESC (an invalid argument, including
///   JOB_PRIORITY_BACKGROUND_TOOL without m_bAcceptToolBackground) and has then taken nothing. A null function
///   pointer is ZENITH_ASSERT.
/// Reentrancy: FULL. Job functions may call every member their thread affinity allows; the four main-thread members
///   (Create, RunRoot, CancelAndDrainBackground, the destructor) are excluded by thread affinity, not by a guard: each
///   fails ZENITH_CHECK when called from a job body (Zenith_IsJobWorkerThread) or from a thread other than the one
///   that called Create (m_ulMainThreadID).
/// Version: since Z2 Stage 2. Source API.
/// Locks: none of its own. SubmitBackground* and background completion reach CORE's uLOCK_LEVEL_OPERATION_TABLE
///   (380, short-hold) inside the operation table; no frame path acquires any Zenith_Mutex.
/// Example: Zenith2/Engine/ZenithJobs/Tests/Examples/Zenith_JobSystem_Example.cpp
/// @endcontract
class Zenith_JobSystem
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_JobSystem>, Zenith_JobsError> Create(
		const Zenith_JobSystemDesc& xDesc);
	~Zenith_JobSystem();                                 // wakes every thread, joins render-submit, then workers N..1
	Zenith_JobSystem(const Zenith_JobSystem&) = delete;
	Zenith_JobSystem& operator=(const Zenith_JobSystem&) = delete;

	uint32 GetWorkerCount() const;       // N; excludes main and render-submit
	uint32 GetFrameThreadCount() const;  // N + 1: the threads that run JOB_AFFINITY_ANY work (0..N)
	uint32 GetThreadSlotCount() const;   // N + 2: every index a context can report
	bool IsRenderSubmitThreadRunning() const;
	// The resolved tools-host cap: the most workers that run JOB_PRIORITY_BACKGROUND_TOOL slices at once; 0 when the
	// system was created without m_bAcceptToolBackground (the priority is then refused). Fixed at Create. A tool
	// operation keeps at most 2 × this value of its jobs in flight (Algorithms.md "Tools-host background priority").
	uint32 GetMaxConcurrentToolBackground() const;

	// The job system's memory-governor consumer: resident bytes = jobs.memory_bytes (relaxed loads), counted under
	// MEMORY_SUBSYSTEM_OTHER, whose budget includes Z2-B-JOBS-004 (Architecture.md "Memory governor", table "Budgets";
	// cross-doc X59, pending ARCH). The composition root registers it with Zenith_MemoryGovernor after Create and
	// unregisters it before destruction. JOBS sheds nothing on a pressure change (Algorithms.md "Create and failure
	// unwinding" states why); it logs the new level once.
	Zenith_MemoryConsumer& GetMemoryConsumer();

	// Background work. pData must stay valid until the returned handle reports IsComplete(); destroying the handle
	// earlier requests cancellation and detaches, and pData must still stay valid until the job has returned, so a
	// submitter that cannot guarantee that uses SubmitBackgroundCopy. szName is a string literal "bg/<Name>" (static
	// lifetime; ZENITH_ASSERT on the prefix) used as the profiling zone name. ePriority is HIGH, LOW, or TOOL in a job
	// system created with m_bAcceptToolBackground; TOOL elsewhere returns JOBS_ERROR_INVALID_DESC having taken nothing.
	Zenith_Result<Zenith_OperationHandle, Zenith_JobsError> SubmitBackground(
		Zenith_PfnJob pfnJob, void* pData, Zenith_JobPriority ePriority, const char* szName = "bg/unnamed");

	// As SubmitBackground, but copies xPayload (at most uJOBS_BACKGROUND_PAYLOAD_BYTES; more is ZENITH_ASSERT and
	// JOBS_ERROR_INVALID_DESC) into the operation record; each slice receives a pointer to the copy, 16-byte aligned,
	// valid for that slice; a slice that calls Continue with that pointer carries the payload to the next slice
	// (Algorithms.md "Record types").
	Zenith_Result<Zenith_OperationHandle, Zenith_JobsError> SubmitBackgroundCopy(
		Zenith_PfnJob pfnJob, Zenith_ByteSpan xPayload, Zenith_JobPriority ePriority, const char* szName = "bg/unnamed");

	// Runs pfnJob as a root frame job on the main thread with a full context (ParallelFor, Continue and the arenas
	// work as in a node), helping until it, every sub-job and every continuation have finished, then returns.
	// For tools, benchmarks and tests, on the thread that called Create; never inside an iteration of GRAPH's executor
	// (ZENITH_CHECK for either). It is an iteration:
	// it advances the frame epoch (node-arena memory of the previous iteration is reclaimed) and publishes the counters.
	void RunRoot(Zenith_PfnJob pfnJob, void* pData, const char* szName);

	// Shutdown step 4 of Contracts.md "Shutdown ownership order": refuses further background submissions
	// (JOBS_ERROR_SHUTTING_DOWN), requests cancellation of every incomplete background operation, then runs
	// background jobs on the main thread (and lets workers run them) until every operation has reported complete.
	void CancelAndDrainBackground();

private:
	struct PrivateTag {};
public:
	Zenith_JobSystem(PrivateTag, Zenith_Heap& xHeap);   // infallible; Create runs the fallible steps
private:
	void* m_pxState = nullptr;                                     // Zenith_JobSystemState (Internal), owned
};
```

<!-- informative -->
`m_pxState` is a type-erased owner of the Internal state so that the public header includes no `Internal/` header (`Zenith2/Docs/Contracts.md → "API boundary"`); it is not a pimpl for compile-time reasons: the state holds atomics and platform handles whose layout the public header must not expose. The destructor frees it through `Zenith_Delete` inside `Internal/Zenith_JobSystem.cpp` by casting back to `Zenith_JobSystemState*`; that file holds the one `Zenith_Owned<Zenith_JobSystemState>` released into the pointer.
<!-- /informative -->

`SubmitBackground` issues its `Zenith_OperationHandle` from a `Zenith_OperationTable` the job system owns, created with `Zenith_OperationTableDesc{ "jobs.background", 2 × m_uBackgroundCapacity + T + N + 2 }`, where T = `m_uToolBackgroundCapacity` when the system accepts the tools-host priority and 0 otherwise (so a Player's table is unchanged by it: 16,392 slots at N = 6 and the default desc; a tool host's at the same N gains 1,024); `Issue()` returns the handle for the requester and a move-only `Zenith_OperationTicket` for the executor, which the job system keeps in a per-slot ticket array (`Zenith2/Docs/Design/CORE/Threading.md → "Zenith_OperationHandle implementation"`). `OPERATION_ERROR_TABLE_FULL` maps to `JOBS_ERROR_QUEUE_FULL` and `OPERATION_ERROR_OUT_OF_MEMORY` to `JOBS_ERROR_OUT_OF_MEMORY` at this boundary ([Z2-N-CONTRACT-463]).

The ticket array is indexed by `GetSlotID().m_uSlot`, which CORE guarantees is below the table's `m_uMaxOperations`, and failure codes are `uint16`, as `Zenith_OperationTicket::CompleteFailed(uint16)` takes them (`Zenith2/Docs/Design/CORE/Threading.md → "Zenith_OperationHandle implementation"`).

## Zenith_SimulationClock header

`Zenith2/Docs/Architecture/CPUFrameGraph.md → "Simulation clock"` declares the clock; JOBS keeps every member and adds the fixed-ticks mode (used by `--replay`, where T comes from the log, by `--exit-after-ticks` smoke runs and by the determinism suites) and desc validation.

```cpp
/// @contract functions Zenith_SimulationClockLimits
/// Ownership: constexpr constants; no state.
/// Thread affinity: IMMUTABLE. Compile-time constants.
/// Invalidation: None.
/// Units: ticks, Hz, a dimensionless scale factor.
/// Errors: None; Zenith_ValidateSimulationClockDesc checks against them.
/// Version: since Z2 Stage 2.
/// @endcontract
inline constexpr uint32 uSIM_MAX_CATCH_UP_TICKS_LIMIT = 64;   // validation bound; the budget row picks the value
inline constexpr uint32 uSIM_MIN_TICK_HZ = 1;
inline constexpr uint32 uSIM_MAX_TICK_HZ = 1000;
inline constexpr float fSIM_MAX_TIME_SCALE = 8.0f;

/// @contract enum Zenith_SimulationClockMode
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. A value type; safe everywhere.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
enum Zenith_SimulationClockMode : uint8
{
	SIM_CLOCK_MODE_REAL_TIME,        // Advance: accumulator over real time, scale, pause, bounded catch-up
	SIM_CLOCK_MODE_EXTERNAL_TICKS,   // AdvanceExternal: T supplied by the caller (replay log, sim.ticksPerFrame); alpha 0
};

/// @contract value Zenith_SimulationClockDesc
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Read by the constructor.
///   Members non-const: callers default-construct it and set the fields they need.
/// Invalidation: None.
/// Units: Hz, ticks, microseconds.
/// Errors: Zenith_ValidateSimulationClockDesc returns JOBS_ERROR_INVALID_DESC outside the ranges below; the
///   constructor asserts validity (ZENITH_CHECK), because the composition root validates first ([Z2-N-ARCH-436]).
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_SimulationClockDesc
{
	uint32 m_uTickHz = 60;                    // sim.tickHz, [1, 1000]
	uint32 m_uMaxCatchUpTicks = 0;            // sim.maxCatchUpTicks = Z2-B-JOBS-002, [1, 64]; 0 is invalid
	uint32 m_uMaxFrameDeltaUs = 250000;       // sim.maxFrameDeltaUs, [1000, 1000000]
	Zenith_SimulationClockMode m_eMode = SIM_CLOCK_MODE_REAL_TIME;
};

Zenith_Result<void, Zenith_JobsError> Zenith_ValidateSimulationClockDesc(const Zenith_SimulationClockDesc& xDesc);

/// @contract type Zenith_SimulationClock
/// Ownership: A plain object owned by ZenithRuntime (one per process); no allocation.
/// Thread affinity: MAIN_THREAD. Advance, AdvanceExternal and the setters run in the frame prologue; the getters
///   are read on the main thread, and the values nodes need (tick index, alpha) are passed to them by value.
/// Invalidation: None.
/// Units: Microseconds for durations; ticks for counts; alpha is a fraction of one tick.
/// Errors: SetTimeScale with NaN is ZENITH_ASSERT and sets 0; AdvanceExternal in REAL_TIME mode (or Advance in
///   EXTERNAL_TICKS mode) is ZENITH_CHECK; AdvanceExternal with uTicks > m_uMaxCatchUpTicks is ZENITH_CHECK.
/// Version: since Z2 Stage 2. sim.tickHz is recorded in the replay header; the arithmetic below is part of the contract.
/// @endcontract
class Zenith_SimulationClock
{
public:
	explicit Zenith_SimulationClock(const Zenith_SimulationClockDesc& xDesc);
	uint32 Advance(uint64 ulRealDeltaUs);     // REAL_TIME: returns the tick count T of this frame
	uint32 AdvanceExternal(uint32 uTicks);    // EXTERNAL_TICKS: returns uTicks (0 while paused); alpha stays 0
	void SetPaused(bool bPaused);                             // sim.paused
	void SetTimeScale(float fScale);                          // sim.timeScale, clamped to [0, 8]
	bool IsPaused() const;
	float GetTimeScale() const;
	float GetInterpolationAlpha() const;        // in [0, 1)
	uint64 GetTickIndex() const;               // ticks issued since construction (sum of T)
	uint64 GetTickDurationUs() const;          // round(1e6 / tickHz)
	uint64 GetDroppedTickCount() const;        // cumulative
	uint32 GetLastDroppedTicks() const;        // dropped by the last Advance
	uint64 GetAccumulatorNs() const;           // for tests and the sim.alpha counter
private:
	Zenith_SimulationClockDesc m_xDesc;
	uint64 m_ulTickDurationNs = 0;
	uint64 m_ulAccumulatorNs = 0;
	uint64 m_ulTickIndex = 0;
	uint64 m_ulDroppedTicks = 0;
	uint32 m_uLastDroppedTicks = 0;
	float m_fTimeScale = 1.0f;
	bool m_bPaused = false;
};
```

`GetTickIndex` counts ticks the clock has issued. A world's own tick index is advanced by that world's tick boundary (`Zenith2/Docs/Architecture/FramePhases.md → "The tick boundary"`, step 4) and is what systems read through `Zenith_SystemContext::GetTickIndex` (`Zenith2/Docs/Architecture/ECSModel.md → "Scheduling contract"`); the two agree for a world created before the first tick and never paused on its own.

## Zenith_FloatEnvironment header

The type is `Zenith_FloatEnvironmentState`, as `Zenith2/Docs/Contracts.md → "Floating-point environment"` ([Z2-N-CONTRACT-360]) and `Zenith2/Docs/Scripting.md → "VM model and threading"` ([Z2-N-SCRIPT-086]) both name it; the fixed values are those of Contracts' table, which Scripting's table also carries. Contracts' arm64 row names `mrs`/`msr` inline assembly; JOBS uses the equivalent compiler builtins `__builtin_arm_rsr64("fpcr")` / `__builtin_arm_wsr64("fpcr", …)`, which compile to exactly those instructions. The remaining differences in Scripting's table (its MSVC sequence uses `_controlfp_s` where Contracts fixes `_mm_setcsr`; its arm64 clear mask omits bits 24–25, which are then set anyway; its file names lack the `_<Platform>` suffix) are the SCRIPT owner's cross-doc item X-e; JOBS's files and sequences below are the implementation.

```cpp
/// @contract value Zenith_FloatEnvironmentState
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. A value type; Read and Apply act on the calling thread's registers.
/// Invalidation: None.
/// Units: m_ulControl holds MXCSR (x86-64) or FPCR (arm64) exactly as read; m_ulCompareMask the bits that are compared.
/// Errors: None.
/// Version: since Z2 Stage 2. The fixed state is part of the determinism guarantee (Z2-D-019); changing it changes
///   simulation results and needs a decision.
/// @endcontract
struct Zenith_FloatEnvironmentState
{
	const uint64 m_ulControl = 0;
	const uint64 m_ulCompareMask = 0;
};

void Zenith_ApplyFloatEnvironment();                                    // sets the fixed state on this thread
Zenith_FloatEnvironmentState Zenith_ReadFloatEnvironment();          // this thread's current registers
Zenith_FloatEnvironmentState Zenith_GetExpectedFloatEnvironment();   // the fixed state for this target
bool Zenith_IsExpectedFloatEnvironment(const Zenith_FloatEnvironmentState& xRead);
	// (xRead.m_ulControl & mask) == (expected.m_ulControl & mask), mask = expected.m_ulCompareMask
```

| Target (presets) | Implementation file | Apply | Expected `m_ulControl` | `m_ulCompareMask` |
|---|---|---|---|---|
| MSVC x64 (`win-*`) | `Zenith2/Engine/ZenithJobs/Internal/Windows/Zenith_FloatEnvironment_Windows.cpp` | `_mm_setcsr((_mm_getcsr() & ~0xFFC0u) \| 0x9FC0u)` | `0x9FC0` (FTZ bit 15, DAZ bit 6, mask bits 7–12, RC = 00) | `0xFFC0` (sticky flag bits 0–5 ignored) |
| clang x86-64 (`linux-tsan`) | `Zenith2/Engine/ZenithJobs/Internal/Linux/Zenith_FloatEnvironment_Linux.cpp` | as MSVC x64 | `0x9FC0` | `0xFFC0` |
| clang x86-64 (`android-x86_64`) | `Zenith2/Engine/ZenithJobs/Internal/Android/Zenith_FloatEnvironment_Android.cpp`, `__x86_64__` branch | as MSVC x64 | `0x9FC0` | `0xFFC0` |
| clang arm64 (`android-arm64`) | the same file, `__aarch64__` branch | `__builtin_arm_wsr64("fpcr", (__builtin_arm_rsr64("fpcr") & ~0x03C89F03ull) \| 0x03000000ull)` | `0x03000000` (FZ bit 24, DN bit 25; RMode 00; FIZ, AH, FZ16 and every trap enable 0) | `0x03C89F03` |

The x86-64 files include `<xmmintrin.h>` (a compiler intrinsic header, confined to these `Internal/<Platform>/` files). The arm64 branch uses compiler builtins and includes nothing. The branch between the two Android architectures is on the architecture macros `__aarch64__` and `__x86_64__`, which rule F11 does not list (`Zenith2/Docs/Contracts.md → "Tests and code hygiene gates"`), as in the Scripting table and CORE's `Zenith_CpuRelax`.
