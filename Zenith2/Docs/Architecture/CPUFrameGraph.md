# Architecture — CPU frame graph and scheduler

This file is part of the Architecture document family. The entry document is `Zenith2/Docs/Architecture.md`; this file holds the full specification of its section "CPU frame graph and scheduler". Normative statements carry `[Z2-N-ARCH-NNN]` tags in the range 381–469.

## Settled rules and ownership

The CPU frame is a directed acyclic graph compiled on the shared `ZenithGraph` core, with dynamic sub-jobs, edges derived from declared access plus explicit ordering, no fibers, and frames pipelined through render extraction (Z2-D-006, Z2-D-050). The frame's phase structure and field authority are Z2-D-080 (Architecture.md → "Frame phases and authority"). The determinism guarantee is Z2-D-019.

| Piece | Library | Area |
|---|---|---|
| work-stealing job system, job contexts, sub-jobs, continuations, background jobs | `ZenithJobs` | JOBS |
| frame-graph builder, schedule compilation and caching, frame executor | `ZenithJobs` | GRAPH |
| simulation clock (`Zenith_SimulationClock`, advanced by `ZenithRuntime` in step 2 of the frame prologue) | `ZenithJobs` | JOBS |
| the per-thread floating-point environment (`Zenith_FloatEnvironment.h`: `Zenith_ApplyFloatEnvironment()`, `Zenith_ReadFloatEnvironment()` returning `Zenith_FloatEnvironmentState`), applied by the job system at the start of every worker and of the render-submit thread and by the composition root on the main thread at startup step 9 (Zenith2/Docs/Contracts.md → "Threading and concurrency contract") | `ZenithJobs` | JOBS |
| monotonic and wall clocks (`Zenith_Clock.h`: `Zenith_GetMonotonicTimeUs`, `Zenith_GetMonotonicTimeNs`), read by the frame loop, the profiler and the job system | `ZenithCore` | CORE |
| system declarations over components, fields and world resources; phase mapping | `ZenithECS` | ECS |
| engine stages (platform pump, extraction, render build, record, submit), the frame loop | `ZenithRuntime` | MOD |
| `Flux_RenderWorld`, render build, record and submit | `ZenithFlux` | RPROD |

- [Z2-N-ARCH-381] `ZenithJobs` knows no ECS, phase, component or render concept: it schedules nodes grouped into ordered stages over opaque resources, and `ZenithECS` and `ZenithRuntime` translate systems, phases and engine stages into those terms.
- [Z2-N-ARCH-382] There are no fibers and no blocking waits between jobs: a job never waits on another job, and every dependency is expressed as a graph edge, a sub-job join or a continuation (Zenith2/Docs/Contracts.md → "Threading and concurrency contract"); the one exception is a development-build debugger stop of a Luau VM, bracketed by `Zenith_MarkThreadAsJobWorker(false)` and `Zenith_MarkThreadAsJobWorker(true)`, which the blocking-wait hook reports under `szPrimitive = "debugger-stop"` (Zenith2/Docs/Scripting.md → "Debugging and profiling").

<!-- informative -->
Prior art: Unreal's Task Graph, `UE::Tasks` prerequisites and tick groups; Frostbite's job graph and FrameGraph; Destiny's job graph with its render extraction pipeline; Unity DOTS `JobHandle` dependencies; Bevy and flecs access-derived system scheduling. Naughty Dog's fiber job system was considered and rejected: fibers complicate debuggers, ThreadSanitizer, Android and the Luau VMs, all of which Zenith2 relies on. Zenith's task system was a single mutex-protected queue of 128 tasks with a blocking `WaitUntilComplete`, which this design replaces completely.
<!-- /informative -->

## Frame structure

One frame iteration k is the frame prologue (main-thread loop steps, § "The frame prologue") followed by `Zenith_FrameExecutor::RunFrame(k)`, which runs three subgraphs:

| Subgraph | Runs | Contains |
|---|---|---|
| `FRAME_SUBGRAPH_TICK` | T times per frame, T = the tick count of the simulation clock (0 to the catch-up limit) | per world: the tick boundary, phases 1–11 with a sync point after each, the end-of-tick playback |
| `FRAME_SUBGRAPH_EPILOGUE` | once per frame, after the last tick instance | stage 12, per world: the extractors (phase 12, render extraction into `Flux_RenderWorld` k) and the epilogue stages such as `Audio.Update` (§ "Audio update"), concurrent by declared access; stage 12, engine-wide (group 0): `Assets.Service` (§ "Engine stage nodes"); stage 13, engine-wide (group 0): `Render.Seal`, ordered after every extractor of every world |
| `FRAME_SUBGRAPH_RENDER` | once per frame, for frame k−1 | `Flux.BuildGraph`, `Flux.Record` (parallel), `Flux.Submit` and `Flux.Present` (render-submit thread) |

```text
iteration k:
  prologue(k) [main thread, no job running] ──► RunFrame(k):
      TICK(k,1) ──► TICK(k,2) ──► … ──► TICK(k,T) ──► EPILOGUE(k) ──┐
                                                                     ├──► prologue(k+1)
      RENDER(k-1) ──── runs concurrently with the ticks and epilogue ┘
```

- [Z2-N-ARCH-383] Tick instances within one frame run strictly in sequence: tick instance t+1 starts after every node of tick instance t has completed, and each world's tick boundary node is the first node of its tick.
- [Z2-N-ARCH-384] Phase 12 (render extraction) runs once per frame, in the epilogue after the frame's last tick instance; when T is 0 (paused, or no accumulated time) the epilogue still runs and extracts with the updated interpolation alpha.
- [Z2-N-ARCH-385] The render subgraph of frame k−1 runs concurrently with the tick and epilogue subgraphs of frame k in the same worker pool, with no edge between them and no lock on game state, because it reads only the sealed `Flux_RenderWorld` k−1.
- [Z2-N-ARCH-386] Iteration k+1 begins only after every subgraph of iteration k (including RENDER(k−1)) has completed, so simulation never runs more than one frame ahead of rendering.

### The frame prologue

The frame prologue of iteration k is the sequence of main-thread steps of `Zenith_Runtime::RunFrameLoop` between the completion of iteration k−1 (every subgraph, including RENDER(k−2)) and the call `RunFrame(k)`. No job of the frame graph runs during it. In order:

1. `Platform.Pump`: the SDL event pump and the lifecycle, account and low-memory events it produces (Architecture/PlatformLayer.md → "SDL3 responsibilities"). Step 1b follows it: the package service's prologue, `Zenith_RuntimePackage_RunPrologue` (the delivery poll and mount changes, `Zenith_AssetRegistry::OnMountsChanged` when the mounts changed, and the completion of `ASYNC_SOURCE_PACKAGE` results into each attached world's asynchronous result table; `Zenith2/Docs/Design/PACK/Algorithms.md` → "Event publication and replay").
2. `Zenith_SimulationClock::Advance` computes the tick count T (during `--replay`, T is capped so that the tick named by the next `WORLD_SWAP` record is the frame's last tick); then `Zenith_LogRouter::SetFrameIndex(k)` stamps the frame index on subsequent log records.
3. Queued user-layer configuration writes are applied: `Zenith_Config` commits them through `Zenith_ConfigStore::CommitStagedUserValues`, which dispatches the change callbacks (Architecture/PlatformLayer.md → "Configuration layering").
4. The composition root reports the process and device-local usage (`Zenith_MemoryGovernor::ReportProcessUsage` with `Zenith_QueryProcessResidentBytes()` and the RHI's device-local usage) and publishes the `core.heap.*` and `core.strings.*` counters; then `Zenith_MemoryGovernor::Update` computes the level and notifies consumers (Architecture.md → "Memory governor"); then, when a renderer exists, `Flux_Renderer::RunPrologue()` (`MAIN_THREAD`, a render-idle point) runs the device-loss handler's prologue event, and while the device-loss state is `FLUX_DEVICE_LOSS_STATE_NORMAL` every pending swapchain recreation and a frames-in-flight change (Zenith2/Docs/Rendering.md → "Recovery").
5. Thermal actions are applied (Architecture/PlatformLayer.md → "Thermal adaptation").
6. When a registry change is queued (a script hot reload, an editor schema re-registration), `ZenithRuntime` opens the hot-reload window, applies every queued change and closes it (Architecture.md → "Reflection and runtime type registration"); then, in every frame, `Zenith_ScriptRuntime::CommitPendingReloads()` commits the changed script-system declarations through `Zenith_SystemReloadScope` (Architecture/CompositionRoot.md → "The frame loop").
7. Isolated worlds whose preparation succeeded are created; `Zenith_SceneLoader::StepActivations` starts every pending additive-scene preparation (`BeginPrepare`, world-tag order then commit order) and then advances isolated-world activation within `scene.activationBudgetUs`; then pending world-transition swaps whose load completion has been published and logged are performed (Architecture/LifecycleCommands.md → "World transition").
8. When a recompile trigger fired, the schedule is recompiled and passed to `Zenith_FrameExecutor::SetSchedule`.

- [Z2-N-ARCH-437] The frame prologue is exactly the eight main-thread steps above (step 1 including its step 1b), in that order, with no frame-graph job running; it is not an executor subgraph, and every document that places work "in the frame prologue" means one of these steps.
- [Z2-N-ARCH-438] The frame prologue precedes every world's first tick boundary of its frame and is not itself a tick boundary; tick boundaries 2 to T of a catch-up frame are not prologues, and no prologue step runs at them.
- [Z2-N-ARCH-439] The EPILOGUE subgraph has two stages: stage 12 holds every world's extractors and epilogue stages, which run concurrently as their declared accesses allow; stage 13 holds the engine-wide `Render.Seal` node, which `Zenith_WorldSimulation::EmitInto` orders after every extractor by generated orderings and not after epilogue stages, because an epilogue stage writes nothing the render world holds.

### Audio update

<!-- elaboration -->
Audio is updated once per frame, not per tick, as the epilogue node `Audio.Update@W<tag>` of each world that has the Audio module (`FRAME_NODE_KIND_STAGE`, `JOB_AFFINITY_ANY`). It reads the listener's and every audio emitter's world transform and the audio components (registered with `Zenith_WorldAttachContext::AddEpilogueStage`, Architecture/CompositionRoot.md → "Startup sequence", whose returned `Zenith_SystemBuilder` declares its reads, so it runs concurrently with the world's extraction, which reads as well; the frame's audio commands arrive as the world resource `Zenith_AudioCommandQueue`, filled at sync-point playback, which it drains and therefore declares with `WriteResource` as its sole consumer, Architecture/ECSModel.md bullet 727), applies the frame's audio commands that simulation recorded (triggers, parameter changes, stops, all merged in merge order at their sync points) and updates that world's voices, intents and candidates alone. The Audio module's one engine-wide EPILOGUE stage `Audio.Mix` (§ "Engine stage nodes", registered once at module start with `Zenith_ModuleStartContext::AddEngineEpilogueStage`) runs after every world's `Audio.Update@W<tag>`, selects the real voices over every updated world, and publishes one mixer snapshot per frame to the audio device thread through a bounded lock-free channel (the snapshot triple buffer: a newer snapshot supersedes an older one; `Zenith2/Docs/Design/AUDIO.md` → "Audio.Mix step by step"). The audio device thread is owned by the Audio module and mixes from the newest snapshot it has received (bullet 435). The AUDIO design document (`Zenith2/Docs/Design/AUDIO.md`) specifies the mixer and the device thread.

- [Z2-N-ARCH-434] Audio is updated by one epilogue stage node per world, `Audio.Update@W<tag>`, registered through `Zenith_WorldAttachContext::AddEpilogueStage`, which drains its world's `Zenith_AudioCommandQueue` (declared as a write), reads simulation state only through its declared access and updates that world's audio state alone; the module's engine-wide stage `Audio.Mix` (bullet 442), ordered after every `Audio.Update@W<tag>`, hands the device thread one mixer snapshot per frame through a bounded lock-free channel; no simulation node touches the audio device thread's state.
- [Z2-N-ARCH-435] The audio device thread never waits on, locks against or allocates for any job or engine thread; a missing snapshot makes it mix the previous one again.

### Engine stage nodes

`ZenithRuntime` registers the engine-wide stage nodes (group 0) once per frame-schedule compile (`Internal/Zenith_EngineStages.cpp`, Zenith2/Docs/Design/MOD.md → "Frame-graph nodes registered by ZenithRuntime"); modules add their per-world nodes through the attach context, and at most one engine-wide EPILOGUE stage each through the start context (`Zenith_ModuleStartContext::AddEngineEpilogueStage`, Architecture/CompositionRoot.md → "Module interfaces").

| Node | Subgraph, stage | Kind, affinity | Declared access | Body | Headless |
|---|---|---|---|---|---|
| `Assets.Service` | `FRAME_SUBGRAPH_EPILOGUE`, 12 | `FRAME_NODE_KIND_STAGE`, `JOB_AFFINITY_ANY` | a write of the world-independent resource `Zenith_AssetRegistry`, and nothing else | `Zenith_AssetLoader::ServiceFrame`: advance the frame, poll I/O completions, kick the background service when work is pending, bounded by `asset.service.frameBudgetUs` (Zenith2/Docs/Design/ASSET.md) | present |
| `Render.Seal` | `FRAME_SUBGRAPH_EPILOGUE`, 13 | `FRAME_NODE_KIND_STAGE`, `JOB_AFFINITY_ANY` | ordered after every extractor by the extractors' own `Before("Render.Seal@W0")` (bullet 439) | § "Render extraction and pipelining" | present |
| a module's engine-wide stage (`Audio.Mix` today) | `FRAME_SUBGRAPH_EPILOGUE`, 13 | `FRAME_NODE_KIND_STAGE`, `JOB_AFFINITY_ANY` | a write of one world-independent resource of the module (`Zenith.Audio.System`) and nothing else; generated `After` edges from every world's epilogue stage named by the desc's `m_szAfterWorldStage` (`Audio.Update`) | the module's (`Zenith2/Docs/Design/AUDIO.md` → "Audio.Mix step by step") | present |
| `Flux.BuildGraph`, `Flux.Record` (parallel sub-jobs) | `FRAME_SUBGRAPH_RENDER` | `FRAME_NODE_KIND_STAGE`, `JOB_AFFINITY_ANY` | the sealed render world k−1 | Zenith2/Docs/Rendering.md → "Render graph" | absent |
| `Flux.Submit`, `Flux.Present` | `FRAME_SUBGRAPH_RENDER` | `FRAME_NODE_KIND_STAGE`, `JOB_AFFINITY_RENDER_SUBMIT` | — | Zenith2/Docs/Rendering.md → "GPU lifetime and synchronisation" | absent |

- [Z2-N-ARCH-441] `ZenithRuntime` registers `Assets.Service` in every configuration, headless included, as the one engine-wide EPILOGUE stage-12 node, declaring a write of `Zenith_AssetRegistry` alone, so no world node orders against it, and its body returns once it has spent `asset.service.frameBudgetUs` of node time in the frame.
- [Z2-N-ARCH-442] A module's engine-wide EPILOGUE stage, registered with `Zenith_ModuleStartContext::AddEngineEpilogueStage` (at most one per module, journaled and undone with the module), is a group-0 stage-13 node compiled into every frame schedule from the next compile on, in every configuration, headless included and with no world attached; its builder accepts a write of one world-independent resource alone, and the schedule compiler orders it after every world's epilogue stage named `m_szAfterWorldStage` through generated `After` edges, so a world attached in this frame's prologue is simply absent from its edges until its first stage runs.

## Job system

### Public types

Declared in `Engine/ZenithJobs/Zenith_JobSystem.h` and `Engine/ZenithJobs/Zenith_JobContext.h`:

<!-- elaboration -->
```cpp
enum Zenith_JobAffinity : uint8
{
	JOB_AFFINITY_ANY,             // any worker, or the main thread while it helps
	JOB_AFFINITY_MAIN,            // the main thread: SDL platform pump, each world's main Luau VM (behaviours)
	JOB_AFFINITY_RENDER_SUBMIT,   // the render-submit thread: queue submission and present
};

enum Zenith_JobPriority : uint8
{
	JOB_PRIORITY_FRAME,           // frame-graph nodes and their sub-jobs
	JOB_PRIORITY_BACKGROUND_HIGH, // asynchronous work whose result a tick boundary is waiting to publish
	JOB_PRIORITY_BACKGROUND_LOW,  // prefetch, cache warming
	JOB_PRIORITY_BACKGROUND_TOOL, // tool hosts alone ([Z2-N-ARCH-443])
};

class Zenith_JobContext;
using Zenith_PfnJob = void (*)(Zenith_JobContext& xContext, void* pData);
using Zenith_PfnParallelForBody = void (*)(Zenith_JobContext& xContext, uint32 uBegin, uint32 uEnd, void* pData);

struct Zenith_JobSystemDesc
{
	uint32 m_uWorkerCount = 0;               // 0 = logical cores - 2, clamped to [1, 62]
	uint32 m_uDequeCapacity = 4096;          // per worker, power of two
	uint32 m_uInjectionCapacity = 16384;     // multi-producer queue for non-worker submitters
	uint32 m_uBackgroundCapacity = 8192;     // per background priority
	uint32 m_uSpinIterations = 2000;         // before an idle worker parks
	bool m_bAcceptToolBackground = false;     // a tool host's composition root alone ([Z2-N-ARCH-443])
	uint32 m_uMaxConcurrentToolBackground = 0;
	// partial excerpt: Zenith2/Docs/Design/JOBS/PublicAPI.md → "Zenith_JobSystem header" is the declaration of record
	Zenith_Heap* m_pxHeap = nullptr;          // general heap for the system's own storage
};

// Contract: ANY_THREAD_EXCEPT(Create, destructor). Create and destruction on the main thread.
// Owns its worker threads and the render-submit thread; the destructor joins them.
class Zenith_JobSystem
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_JobSystem>, Zenith_JobsError> Create(
		const Zenith_JobSystemDesc& xDesc);
	~Zenith_JobSystem();

	uint32 GetWorkerCount() const;       // excludes main and render-submit
	Zenith_Result<Zenith_OperationHandle, Zenith_JobsError> SubmitBackground(
		Zenith_PfnJob pfnJob, void* pData, Zenith_JobPriority ePriority);
};

// Contract: OWNER_THREAD — valid only inside the job invocation it was passed to.
class Zenith_JobContext
{
public:
	uint32 GetWorkerIndex() const;        // 0 = main thread, 1..N workers, N+1 render-submit
	uint32 GetWorkerCount() const;
	void ParallelFor(uint32 uCount, uint32 uGrain, Zenith_PfnParallelForBody pfnBody, void* pData);
	void Continue(Zenith_PfnJob pfnContinuation, void* pData);
	bool IsCancelled() const;
	Zenith_Arena& GetNodeArena();          // valid until the owning node completes; reclaimed at the next frame boundary
	Zenith_Arena& GetWorkerArena();        // per worker, reset at each frame boundary
	uint64 GetInvocationID() const;       // unique per node invocation (see ECS model)
};
```

- [Z2-N-ARCH-387] Per-worker state (deque, arenas, steal RNG, profiler buffer handle) is reached only through the `Zenith_JobContext&` passed to every job; no job-system state is `thread_local` or global (Zenith2/Docs/Contracts.md → "No mutable global state").
- [Z2-N-ARCH-388] Every job function, sub-job body and continuation returns normally (it carries no exception specification, and exceptions are off outside the Luau firewall, Z2-D-141), and a job that must stop early returns; there is no job-level exception, abort or wait API.

### Threads

| Thread | Index | Count | Runs | Steals |
|---|---|---|---|---|
| main | 0 | 1 | `JOB_AFFINITY_MAIN` jobs first, then any `JOB_PRIORITY_FRAME` job while the frame is incomplete | yes, frame work only |
| worker | 1 … N | `m_uWorkerCount` | its own deque, then steals frame work, then background work | yes |
| render-submit | N+1 | 1 | `JOB_AFFINITY_RENDER_SUBMIT` jobs only | no |

- [Z2-N-ARCH-389] The main thread runs the SDL platform pump and every world's main Luau VM (behaviours); per-worker chunk-system VMs belong to their workers and need no pinning (Zenith2/Docs/Scripting.md → "VM model and threading").
- [Z2-N-ARCH-390] The render-submit thread executes only render-submit jobs and never steals: the `JOB_AFFINITY_ANY` jobs its jobs make ready go to the injection queue and it never runs one on any path, so a submit or present is never delayed behind simulation work.

### Queues

| Queue | Producers | Consumers | Capacity | Full policy |
|---|---|---|---|---|
| per-worker deque (Chase–Lev) | its owning worker | owner (LIFO), thieves (FIFO) | `m_uDequeCapacity` | `QUEUE_FULL_POLICY_REJECT`: the job system handles the rejected push by running the job inline on the pushing worker |
| injection queue (MPMC) | the render-submit thread (the `JOB_AFFINITY_ANY` jobs its jobs make ready: the frame producer); threads with no job context, such as I/O, test and tool threads (foreign producers) | workers | sized at schedule compile: `max(m_uInjectionCapacity, 2 × I + 256)`, I = the `JOB_AFFINITY_ANY` nodes of one frame with a render-submit predecessor | frame producer: `QUEUE_FULL_POLICY_FATAL` (`ZENITH_FATAL(FATAL_CODE_QUEUE_FULL, "jobs.injection")`); foreign producer: `QUEUE_FULL_POLICY_REJECT`, the submitter gets `JOBS_ERROR_QUEUE_FULL` with nothing queued and retries |
| main-affinity queue (MPSC) | any thread | main | sized at schedule compile: 2 × main-affinity nodes of one frame + 256 | `QUEUE_FULL_POLICY_FATAL` |
| render-submit queue (MPSC) | any thread | render-submit | sized at schedule compile: 2 × render-submit nodes of one frame + 64 | `QUEUE_FULL_POLICY_FATAL` |
| background queues (MPMC, one per background priority) | any thread | workers when no frame work is visible | `m_uBackgroundCapacity` (the tools-host queue: `m_uToolBackgroundCapacity`, tool hosts only) | `QUEUE_FULL_POLICY_REJECT`: `SubmitBackground` returns `JOBS_ERROR_QUEUE_FULL` |

- [Z2-N-ARCH-391] Every job queue is bounded with the capacity and full policy of the table above, and counts its full events and high-water mark under the counter names `jobs.<queue>.full` and `jobs.<queue>.high_water` (Zenith2/Docs/Contracts.md → "Threading and concurrency contract").
- [Z2-N-ARCH-392] The affinity queues and the injection queue are sized from the compiled schedule, so their capacity bounds exactly the jobs one frame can enqueue into them; reaching a fatal full policy is a scheduler defect, never a load condition.

### Worker loop

Each worker repeats:

1. Pop from its own deque (LIFO end); if a job is found, run it and repeat.
2. Pop from the injection queue; if found, run and repeat.
3. Try to steal frame work: pick up to `2 × N` victims with the worker's own xorshift generator (seeded with its index), steal from the FIFO end; if found, run and repeat.
4. If no frame work exists anywhere (the executor's ready counter is zero), pop the high then the low background queue.
5. Spin `m_uSpinIterations` iterations with a CPU pause instruction, re-checking 1–4.
6. Park on the worker's own `Zenith_SemaphoreNoProfiling` (the park reports its own `jobs.park` wait) after publishing itself in the idle bitmask; a push that finds the idle bitmask non-empty wakes one parked worker.

- [Z2-N-ARCH-393] A background job never delays frame work by more than one slice: a background job body runs for at most 2 ms of wall-clock time before it re-submits its remainder as a continuation, and review checks every background job for this; bullet 443 is the one exception.
- [Z2-N-ARCH-443] In a tool host alone (the editor and the cook, whose composition roots create their job system with `Zenith_JobSystemDesc::m_bAcceptToolBackground`; `ZENITH_TOOLS_HOST` is a preset matrix value and selects no code), a background job submitted at the tools-host priority `JOB_PRIORITY_BACKGROUND_TOOL` may run past the 2 ms slice of bullet 393 (a lightmap tile, a generator region, a BVH build, a whole partition plan); at most `m_uMaxConcurrentToolBackground` workers run such jobs at once (default min(max(1, ⌊N / 2⌋), `m_uMaxConcurrentBackground`) for N workers), so frame work keeps at least half the workers plus the main thread, `jobs.bg.slice_overruns` does not count them, and a job system created without that field refuses it with `JOBS_ERROR_INVALID_DESC` (orchestrator ruling X350 (a), human may override; the API is `Zenith2/Docs/Design/JOBS/PublicAPI.md` → "Zenith_JobSystem header").
- [Z2-N-ARCH-394] The only blocking waits in the scheduler are a worker parking when it has nothing to run, the render-submit thread waiting for the GPU timeline or present, and the main thread parking when the frame is incomplete and nothing it may run is visible; none of them occurs inside a job body, except the development-build debugger stop of bullet 382, which runs outside the job-worker bracket and is excluded from `Test_JobSystem_NoBlockingWaitInJobs`.

## Dynamic sub-jobs and continuations

### ParallelFor

`Zenith_JobContext::ParallelFor(uCount, uGrain, pfnBody, pData)` schedules `pfnBody` over `[0, uCount)` in ranges of at most `uGrain` items (at least 1) and returns immediately. Splitting is lazy binary: the call pushes one job for the whole range; a job whose range exceeds `uGrain` pushes its upper half and processes its lower half, recursively, so queue pressure stays O(log(uCount / uGrain)) per worker.

- [Z2-N-ARCH-395] Sub-jobs spawned by a node are joined before the node completes: the node's completion is signalled after its body has returned, every sub-job it (or its sub-jobs, transitively) spawned has finished, and every continuation it registered has finished; successors see a finished node.
- [Z2-N-ARCH-396] A sub-job's results are merged in a deterministic order defined by the caller — index order for plain ranges, the merge key of the ECS model for simulation work (Architecture.md → "ECS model") — never in completion order.
- [Z2-N-ARCH-397] `ParallelFor` over ECS chunks passes the chunk count as `uCount`; the chunk-to-worker assignment is dynamic and varies between runs, and nothing observable to simulation depends on it.

### Continuations

`Zenith_JobContext::Continue(pfnContinuation, pData)` registers a continuation that runs once every sub-job the node spawned before the `Continue` call has finished. Continuations registered by one node run in registration order, each after the previous one and after the sub-jobs spawned before it. A continuation inherits its node's affinity and may itself spawn sub-jobs and continuations.

- [Z2-N-ARCH-398] Anything that looks like waiting inside a node is written as a continuation: a node that needs its sub-jobs' results registers a continuation to consume them, and the node's deterministic merge step is its final continuation.

<!-- informative -->
The join counter per node is one atomic `uint32` in the frame arena, incremented before each push and decremented at each sub-job's end; the thread that decrements it to zero runs the next continuation, or, when none remains, signals node completion and decrements each successor's dependency counter.
<!-- /informative -->

## Frame graph builder

### Declarations at the jobs level

Declared in `Engine/ZenithJobs/Zenith_FrameGraph.h`:

```cpp
enum Zenith_FrameSubgraph : uint8
{
	FRAME_SUBGRAPH_TICK,
	FRAME_SUBGRAPH_EPILOGUE,
	FRAME_SUBGRAPH_RENDER,
};

enum Zenith_FrameNodeKind : uint8
{
	FRAME_NODE_KIND_SYSTEM,         // an ECS system of one world (C++ or Luau chunk system)
	FRAME_NODE_KIND_BEHAVIOURS,     // the Luau behaviours of one world and one script phase (main thread)
	FRAME_NODE_KIND_STAGE,          // an engine stage (pump, clock, physics step, audio update, extraction, render nodes)
	FRAME_NODE_KIND_ARBITRATION,    // a generated arbitration merge for one arbitrated field
	FRAME_NODE_KIND_SYNC_POINT,     // generated: command playback after one stage of one world
	FRAME_NODE_KIND_TICK_BOUNDARY,  // generated: the first node of a world's tick
};

struct Zenith_FrameNodeID { uint32 m_uIndex = uGRAPH_INVALID_INDEX; };
struct Zenith_FrameResourceID { uint32 m_uIndex = uGRAPH_INVALID_INDEX; };

struct Zenith_FrameNodeDesc
{
	const char* m_szName = nullptr;       // unique within (subgraph, group)
	Zenith_FrameSubgraph m_eSubgraph = FRAME_SUBGRAPH_TICK;
	Zenith_FrameNodeKind m_eKind = FRAME_NODE_KIND_STAGE;
	Zenith_JobAffinity m_eAffinity = JOB_AFFINITY_ANY;
	uint16 m_uGroup = 0;                 // isolation group: the world tag, or 0 for engine-wide nodes
	uint16 m_uStage = 0;                 // ordered stage inside the subgraph (the phase number for ECS nodes)
	Zenith_PfnJob m_pfnRun = nullptr;
	void* m_pData = nullptr;              // owned by the registrant; outlives the schedule
	uint32 m_uCostHint = 1;
};

struct Zenith_FrameResourceDesc
{
	const char* m_szName = nullptr;
	uint64 m_ulKey = 0;                  // unique key (ECS: hash of world tag, type ID, property ID)
	uint16 m_uGroup = 0;
	bool m_bPersistent = true;            // state that exists before the frame (imported into ZenithGraph)
};

// Contract: IMMUTABLE. m_uStage: the barrier runs after every node of this stage; m_pfnRun: the sync-point body (command playback).
struct Zenith_StageBarrierDesc
{
	const Zenith_FrameSubgraph m_eSubgraph = FRAME_SUBGRAPH_TICK;
	const uint16 m_uGroup = 0;
	const uint16 m_uStage = 0;
	const Zenith_PfnJob m_pfnRun = nullptr;
	void* const m_pData = nullptr;
};

// Contract: OWNER_THREAD (main thread). Built afresh for each (re)compile; arena-backed.
class Zenith_FrameGraphBuilder
{
public:
	explicit Zenith_FrameGraphBuilder(Zenith_Arena& xArena);

	Zenith_FrameNodeID AddNode(const Zenith_FrameNodeDesc& xDesc);
	Zenith_FrameResourceID AddResource(const Zenith_FrameResourceDesc& xDesc);
	void Read(Zenith_FrameNodeID xNode, Zenith_FrameResourceID xResource);
	void Write(Zenith_FrameNodeID xNode, Zenith_FrameResourceID xResource, bool bAlsoReads);
	void Before(Zenith_FrameNodeID xNode, const char* szOtherNodeName, bool bRequired);
	void After(Zenith_FrameNodeID xNode, const char* szOtherNodeName, bool bRequired);
	void SetStageBarrier(const Zenith_StageBarrierDesc& xDesc);

	Zenith_Result<Zenith_Owned<Zenith_FrameSchedule>, Zenith_FrameGraphError> Compile(
		const Zenith_FrameCompileOptions& xOptions, Zenith_Heap& xHeap);
	Zenith_Span<const Zenith_FrameGraphDiagnostic> GetDiagnostics() const;   // the last Compile's; in the builder's arena
};
```

```cpp
// Engine/ZenithJobs/Zenith_JobsErrors.h
// The enumeration form of [Z2-N-CONTRACT-480]; the queue or step involved is logged at the point of failure.
enum Zenith_JobsError : uint8
{
	JOBS_ERROR_THREAD_CREATE = 1,    // a worker or the render-submit thread could not be created
	JOBS_ERROR_QUEUE_FULL,           // a background queue, or the injection queue for a foreign producer, rejected the job
	JOBS_ERROR_OUT_OF_MEMORY,        // the system's own storage could not be allocated
	JOBS_ERROR_INVALID_DESC,         // a desc field is outside its range
	JOBS_ERROR_SHUTTING_DOWN,        // a background submission after background shutdown began
	JOBS_ERROR_NO_ITERATION = 6,     // a foreign inject while no iteration is active; the caller retries at its next wake-up
};

enum Zenith_FrameGraphErrorCode : uint8
{
	FRAME_GRAPH_ERROR_UNORDERED_WRITERS = 1,
	FRAME_GRAPH_ERROR_ORDERING_CONTRADICTS_STAGE,
	FRAME_GRAPH_ERROR_ORDERING_TARGET_MISSING,
	FRAME_GRAPH_ERROR_DUPLICATE_NODE_NAME,
	FRAME_GRAPH_ERROR_GRAPH,          // a ZenithGraph compile error; its diagnostics are the builder's
};

struct Zenith_FrameGraphDiagnostic
{
	Zenith_FrameGraphErrorCode m_eCode;
	uint32 m_uNodeA = uGRAPH_INVALID_INDEX;
	uint32 m_uNodeB = uGRAPH_INVALID_INDEX;
	Zenith_StringView m_strText;       // the formatted message
};

// The error record of [Z2-N-CONTRACT-480]; the diagnostics are read from the builder.
struct Zenith_FrameGraphError
{
	Zenith_FrameGraphErrorCode m_eCode = FRAME_GRAPH_ERROR_GRAPH;  // the code of the first diagnostic
	uint32 m_uDiagnosticCount = 0;                                // at least 1
	uint64 m_ulStructureHash = 0;
};
static_assert(sizeof(Zenith_FrameGraphError) <= 32);
```

- [Z2-N-ARCH-440] A failed `Zenith_FrameGraphBuilder::Compile` logs every diagnostic at the point of failure and keeps them, sorted by code then node, for `GetDiagnostics`, which returns them until the builder is destroyed; the returned `Zenith_FrameGraphError` carries the first diagnostic's code and the count and no text.

### Declarations at the ECS level

Systems are declared through `Zenith_SystemRegistry` (ZenithECS, `Engine/ZenithECS/Zenith_System.h`); this is the syntax modules and the script runtime use. `Zenith_TypeID` and `Zenith_PropertyID` are reflection IDs (Architecture.md → "Reflection and runtime type registration").

```cpp
class Zenith_SystemContext;
using Zenith_PfnSystemRun = void (*)(Zenith_SystemContext& xContext);

struct Zenith_SystemDesc
{
	const char* m_szName = nullptr;               // "<Module>.<System>", unique per world
	Zenith_Phase m_ePhase = PHASE_NONE;           // Architecture.md → "Frame phases and authority"
	Zenith_PfnSystemRun m_pfnRun = nullptr;
	void* m_pUserData = nullptr;                  // owned by the registrant; outlives the world
	Zenith_JobAffinity m_eAffinity = JOB_AFFINITY_ANY;
	Zenith_WriterSource m_eWriterSource = WRITER_SOURCE_NONE;  // for arbitrated fields
	uint32 m_uCostHint = 1;
};

// Contract: OWNER_THREAD (main thread), valid until Zenith_SystemRegistry::Freeze.
class Zenith_SystemBuilder
{
public:
	Zenith_SystemBuilder& Read(Zenith_TypeID xType);                                   // every property
	Zenith_SystemBuilder& Read(Zenith_TypeID xType, Zenith_Span<const Zenith_PropertyID> axProperties);
	Zenith_SystemBuilder& Write(Zenith_TypeID xType);                                  // every property
	Zenith_SystemBuilder& Write(Zenith_TypeID xType, Zenith_Span<const Zenith_PropertyID> axProperties);
	Zenith_SystemBuilder& ReadResource(Zenith_TypeID xResourceType);
	Zenith_SystemBuilder& WriteResource(Zenith_TypeID xResourceType);
	Zenith_SystemBuilder& EmitsCommands();
	Zenith_SystemBuilder& EmitsEvent(Zenith_TypeID xEventType);
	Zenith_SystemBuilder& Before(const char* szSystemName);
	Zenith_SystemBuilder& After(const char* szSystemName);
	Zenith_SystemBuilder& BeforeIfPresent(const char* szSystemName);
	Zenith_SystemBuilder& AfterIfPresent(const char* szSystemName);
	Zenith_SystemBuilder& Query(const Zenith_QueryDesc& xQuery, Zenith_QuerySlot& xOutSlot);
};

// Convenience wrappers over the type-erased calls, for C++-registered types:
template<typename TComponent> Zenith_SystemBuilder& Zenith_Read(Zenith_SystemBuilder& xBuilder);
template<typename TComponent> Zenith_SystemBuilder& Zenith_Write(Zenith_SystemBuilder& xBuilder);
```

A system registration, as a module writes it:

```cpp
Zenith_SystemBuilder& xBuilder = xRegistrar.Systems().AddSystem(Zenith_SystemDesc{
	.m_szName = "AI.Perception",
	.m_ePhase = PHASE_AI_NAVIGATION,
	.m_pfnRun = &Zenith_AIPerception_Run,
	.m_pUserData = pxPerceptionState,
	.m_uCostHint = 40 });
const Zenith_PropertyID axTransformRead[] = { Zenith_WorldTransform::uPROP_POSITION, Zenith_WorldTransform::uPROP_ROTATION };
xBuilder.Read(Zenith_WorldTransform::xTYPE_ID, axTransformRead)
	.Write(Zenith_AIPerceptionState::xTYPE_ID)
	.ReadResource(Zenith_PhysicsWorldResource::xTYPE_ID)
	.EmitsEvent(Zenith_AIStimulusEvent::xTYPE_ID)
	.After("AI.NavRequests")
	.Query(xPerceiverQueryDesc, pxPerceptionState->m_xPerceiverQuery);
```

- [Z2-N-ARCH-399] A system reaches component data, world resources, queries, command buffers and event emitters only through the `Zenith_SystemContext&` it is invoked with, and the context grants exactly what its registration declared; an undeclared access is a programmer-error assert in builds with asserts on and is rejected by the `Zenith_SystemContext` accessor in every build.
- [Z2-N-ARCH-400] Declaring a whole component (`Read(xType)` or `Write(xType)`) expands to every property of that component when the schedule compiles, so access and authority are always checked per property.
- [Z2-N-ARCH-401] `Before`/`After` name systems or stages by their registered names; an unknown name is a schedule-compile error, while `BeforeIfPresent`/`AfterIfPresent` are ignored when the named node is absent (for example, a module the game did not select).

### Translation into the graph core

`Zenith_WorldSimulation::EmitInto(Zenith_FrameGraphBuilder&)` (ZenithECS; it reads the world's `Zenith_SystemRegistry`, knows the world tag and supplies the sync-point playback bodies — Architecture/ECSModel.md → "Worlds and backends") and the jobs-level compile translate declarations as follows:

1. **Resources.** One frame resource per (world, component type, property) that any system of that world accesses, named `<TypeName>.<property>@W<worldTag>`, persistent; one per (world, world-resource type); one per arbitrated (world, property, writer source) staging slot, transient.
2. **Stage barriers.** Each ECS phase p of world w is stage p of group w in the tick subgraph; the sync-point body of stage p is the world's command playback (Architecture.md → "Frame phases and authority"). The jobs level inserts a `FRAME_NODE_KIND_SYNC_POINT` node per (group, stage) and ordering edges from every node of stage p to it and from it to every node of stage p+1.
3. **Version threading within a stage.** For each resource and each stage in ascending order: the stage's writer of the resource consumes the latest version and produces the next; every reader in the same stage reads the version the writer produced (it runs after the writer), except a reader that declared `Before(<writer>)`, which reads the version before the write (it runs before the writer).
4. **Arbitrated properties.** Each writer writes its own staging resource; a generated `FRAME_NODE_KIND_ARBITRATION` node reads every staging resource of the property and read-writes the property; the stage's readers read the arbitration node's version.
5. **Readers in other stages** read the latest version produced by a preceding stage; the stage ordering already orders them, and transitive reduction removes the redundant hazard edges.
6. **Explicit orderings.** `Before`/`After` become `AddOrdering` calls; an ordering from a subsequent stage to a preceding one is an error (below).
7. **Groups.** Resources are per group, so nodes of different worlds share no resource and run concurrently; engine-wide stage nodes (group 0) order against world nodes through explicit orderings alone.
8. **Merge nodes.** A system that declares `EmitsCommands`, `EmitsEvent`, a reduction or `ConsumesMergeCapacity`, or is a script system, gets a merge node ordered after its system node and declared as the writer of every property the system writes; the merge nodes of one stage of one world are chained in the stage's topological order — Kahn's algorithm over the stage's hazard and ordering edges, the chain itself excluded, ties broken by registration index — the same order that defines read versions (Architecture/ECSModel.md → "Command buffers", bullet 692).

- [Z2-N-ARCH-402] Edges are derived automatically from read-after-write, write-after-read and write-after-write hazards on the resources above, plus the stage orderings and the explicit `Before`/`After` constraints; no other mechanism orders two nodes.
- [Z2-N-ARCH-403] Two writers of one frame resource in one stage without an arbitration node are a jobs-level compile error (`FRAME_GRAPH_ERROR_UNORDERED_WRITERS`); for ECS properties the authority check of Architecture.md → "Frame phases and authority" reports it first, with the authority message.
- [Z2-N-ARCH-404] An explicit ordering that contradicts the stage order — `Before('<B>')` declared by a node in a stage subsequent to `<B>`'s, or `After('<B>')` declared by a node in a stage preceding `<B>`'s — is `FRAME_GRAPH_ERROR_ORDERING_CONTRADICTS_STAGE`, reported as `schedule: '<A>' (phase <P>) declares Before('<B>') but '<B>' runs in preceding phase <Q>` or `schedule: '<A>' (phase <P>) declares After('<B>') but '<B>' runs in subsequent phase <Q>` respectively.
- [Z2-N-ARCH-405] A system's order index — the merge-key component "system order" of the ECS model — is its registration index in the world's system registry, which the composition root's fixed module order makes identical on every run; it never depends on the compiled execution order.

## Schedule compilation and caching

```cpp
struct Zenith_FrameCompileOptions
{
	bool m_bTransitiveReduction = true;
	Zenith_Span<const uint32> m_auMeasuredCostsUs;   // empty on a first compile
};

// Contract: IMMUTABLE after Compile. Shared by the executor and the profiler. Members non-const: Compile fills the
// schedule in place in its second buffer after construction.
class Zenith_FrameSchedule
{
public:
	const Zenith_GraphSchedule& GetSubgraph(Zenith_FrameSubgraph eSubgraph) const;
	uint64 GetStructureHash() const;       // combined hash of the three subgraphs
	uint32 GetAffinityNodeCount(Zenith_JobAffinity eAffinity) const;
	Zenith_Result<void, Zenith_JsonWriteError> WriteJson(Zenith_JsonWriter& xWriter) const;
};
```

- [Z2-N-ARCH-406] The frame graph is compiled once into a static schedule — per subgraph a topological order, per-node dependency counts and critical-path priorities, from `ZenithGraph` — and each frame executes that schedule without recomputing any of it.
- [Z2-N-ARCH-407] The compiled schedule is cached and keyed by its structure hash; it is recompiled only when a trigger in the table "Schedule recompile triggers" fires.
- [Z2-N-ARCH-408] Recompilation runs on the main thread in step 8 of the frame prologue, compiles into a second schedule buffer, and swaps under `uLOCK_LEVEL_JOBS_SCHEDULE` before `RunFrame` of that frame; the previous schedule is released after the render subgraph that used it completes.
- [Z2-N-ARCH-409] A recompile that fails keeps the previous schedule, rejects the change that triggered it (for example, a hot-reloaded script system whose new access declaration breaks authority) and reports the schedule-compile diagnostics; a first compile that fails stops startup with the diagnostics.

Schedule recompile triggers:

| Trigger | Where it fires | Recompiled subgraphs |
|---|---|---|
| world created or destroyed (including a world transition's swap) | `ZenithRuntime`, in step 7 of the frame prologue, before every world's first tick boundary of the frame | tick, epilogue |
| a script system hot-reloaded with a changed access set, phase or system set | queued by the `ZenithScript` reload path, committed in step 6 of the frame prologue by `Zenith_ScriptRuntime::CommitPendingReloads()` through `Zenith_SystemReloadScope` and recompiled in step 8 of the same frame (Zenith2/Docs/Scripting.md → "Quarantine and hot reload") | tick || component schemas re-registered | the editor, between play sessions | all |
| module set changed | startup only (modules are static) | all |
| render view set changed shape (a view kind added or removed) | `ZenithFlux` | render |
| cost re-prioritisation every 256 frames | `ZenithJobs` | none: `Zenith_GraphReprioritise` in place, no structure change |
| the development console command `jobs.recompile` | development builds | all |

- [Z2-N-ARCH-410] Disabling a failing system or behaviour (from the next tick boundary, Zenith2/Docs/Scripting.md → "Commit and failure semantics") never recompiles the schedule: the disabled node stays in place and completes immediately without running its body.

## Execution

```cpp
struct Zenith_FrameParams
{
	uint64 m_ulFrameIndex = 0;
	uint32 m_uTickCount = 0;            // from Zenith_SimulationClock
	bool m_bRunRender = true;            // false for headless configurations, the first frame, and while the renderer's
	                                     // device-loss state is not FLUX_DEVICE_LOSS_STATE_NORMAL (Architecture/CompositionRoot.md)
};

// Contract: MAIN_THREAD.
class Zenith_FrameExecutor
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_FrameExecutor>, Zenith_JobsError> Create(
		Zenith_JobSystem& xJobs, Zenith_Heap& xHeap);
	void SetSchedule(const Zenith_FrameSchedule& xSchedule);   // effective at the next RunFrame
	void RunFrame(const Zenith_FrameParams& xParams);          // returns when iteration k completes
	void RequestCancel();                                      // shutdown and world destruction
};
```

Per subgraph instance, the executor allocates in the frame arena: one atomic dependency counter per node (copied from `m_auDependencyCount`), one join counter per node, and one instance-completion counter. It pushes every node with count zero, respecting affinity. When a node completes, the completing thread decrements each successor's counter and pushes those that reach zero; the thread that completes the last node of an instance starts the next tick instance, the epilogue, or marks the frame's part complete.

- [Z2-N-ARCH-411] Ready nodes are pushed in descending critical-path priority, so among simultaneously ready nodes the one on the longest remaining path is taken first; priority changes the wall-clock schedule only, never a result.
- [Z2-N-ARCH-412] A cancelled schedule instance (`RequestCancel`) still completes every node: nodes that have not started skip their bodies and complete immediately, so every counter reaches zero and no thread waits for work that will never run.

### Simulation clock

Declared in `Engine/ZenithJobs/Zenith_SimulationClock.h`:

```cpp
struct Zenith_SimulationClockDesc
{
	uint32 m_uTickHz = 60;                 // configuration key sim.tickHz (elaboration default)
	uint32 m_uMaxCatchUpTicks = 0;         // sim.maxCatchUpTicks: the row "Bounded catch-up fixed steps per frame"; 0 is invalid
	uint32 m_uMaxFrameDeltaUs = 250000;    // real-time clamp per frame (elaboration default)
};

// Contract: MAIN_THREAD.
class Zenith_SimulationClock
{
public:
	explicit Zenith_SimulationClock(const Zenith_SimulationClockDesc& xDesc);
	uint32 Advance(uint64 ulRealDeltaUs);     // returns the tick count T for this frame
	void SetPaused(bool bPaused);                             // sim.paused
	void SetTimeScale(float fScale);                          // sim.timeScale, clamped to [0, 8]
	float GetInterpolationAlpha() const;        // in [0, 1)
	uint64 GetTickIndex() const;               // ticks completed since world start
	uint64 GetTickDurationUs() const;          // 1e6 / tickHz, rounded to nearest
	uint64 GetDroppedTickCount() const;
};
```

`Advance` computes: `delta = min(ulRealDeltaUs, m_uMaxFrameDeltaUs)`; when paused, `T = 0`; otherwise `accumulator += delta × timeScale`, `T = floor(accumulator / tickDuration)`; if `T > m_uMaxCatchUpTicks`, the excess ticks are dropped (`accumulator` keeps the remainder below one tick, and the dropped count increments); `accumulator -= T × tickDuration`; `alpha = accumulator / tickDuration`.

- [Z2-N-ARCH-413] Simulation runs on fixed ticks: every tick advances simulated time by exactly `GetTickDurationUs()`, systems read simulated time only from the tick index and tick duration, and no simulation code reads wall-clock time.
- [Z2-N-ARCH-414] Catch-up is bounded by `m_uMaxCatchUpTicks` per frame; dropped ticks increment the counter `sim.dropped_ticks` and never run, and a replay records the tick sequence actually executed, so dropped real time never affects replay equality (Architecture.md → "Multiplayer foundations").
- [Z2-N-ARCH-436] `m_uMaxCatchUpTicks` is filled by the composition root from the configuration key `sim.maxCatchUpTicks`, whose value is the budget row "Bounded catch-up fixed steps per frame" (Z2-B-JOBS-002, pending, allocated by the Stage 1 per-system budget ticket for JOBS) written into the platform configuration layer; a value of 0 stops startup with exit code 2 naming the key.
- [Z2-N-ARCH-415] Rendering interpolates between the previous and current tick's world transforms with `GetInterpolationAlpha()` at extraction, and a teleport resets the interpolation history of the teleported entity (Architecture.md → "Lifecycle commands").

## Render extraction and pipelining

### The hand-off object

`Flux_RenderWorld` is a ZenithFlux type declared and owned by Zenith2/Docs/Rendering.md → "Purpose and scope" (its subsection on the render world hand-off); this document specifies what the CPU frame produces into it and when.

Per frame k, the extraction nodes of every extracted world produce, through the per-extractor writer partitions Rendering declares:

| Produced | Source |
|---|---|
| the frame index k and the simulation tick of the frame's last tick | `Zenith_FrameParams`, the world simulation |
| the interpolation alpha of the frame | `Zenith_SimulationClock::GetInterpolationAlpha()` |
| the newest input sequence consumed by simulation and its platform sample time | the Input phase's command log |
| per world, tagged with the world tag: the instance, light, decal, skinned-pose, emitter-command, view-request, UI-list, debug-draw, environment and history-reset records of Rendering's declaration | the registered extractors (below) |

Extractors are registered per world by modules and by `ZenithRuntime` for the engine's own components:

```cpp
// Engine/ZenithRuntime/Zenith_Extractor.h
using Zenith_PfnExtract = void (*)(Zenith_SystemContext& xContext, Flux_RenderWorldWriter& xWriter, void* pUserData);

struct Zenith_ExtractorDesc
{
	const char* m_szName = nullptr;          // "<Module>.Extract<What>", unique per world
	Zenith_PfnExtract m_pfnExtract = nullptr;
	void* m_pUserData = nullptr;             // owned by the registrant; outlives the world
	uint32 m_uCostHint = 1;
};

// Zenith_WorldAttachContext::AddExtractor returns the system builder for the extractor's declared reads
// (phase PHASE_RENDER_EXTRACTION; reads only):
//     Zenith_SystemBuilder& AddExtractor(const Zenith_ExtractorDesc& xDesc);
```

`uRENDER_WORLD_RING_SIZE` (2) is the number of render-world storage slots the CPU pipelining rule below needs.

- [Z2-N-ARCH-416] The extraction node of each world (phase 12) snapshots every render-relevant value into `Flux_RenderWorld` k by copying it; the render world holds no pointer, view or handle into ECS storage, script memory or any other simulation-owned memory, only copied values, asset handles and device handles.
- [Z2-N-ARCH-417] `Flux_RenderWorld` is immutable after `Render.Seal`; the render subgraph reads it without locks, and no simulation node reads or writes any render world.
- [Z2-N-ARCH-418] The render worlds live in a ring of `uRENDER_WORLD_RING_SIZE` (2) arenas: frame k's epilogue writes slot `k % 2` while RENDER(k−1) reads slot `(k−1) % 2`, and a slot is reset only at the start of the epilogue that reuses it, after the render subgraph that read it has completed (guaranteed by the iteration boundary).
- [Z2-N-ARCH-419] Modules contribute to extraction only through extractors registered with `Zenith_WorldAttachContext::AddExtractor` (Architecture/CompositionRoot.md → "Startup sequence"); each extractor declares its reads like any system, runs as a phase-12 node, and writes only its own `Flux_RenderWorldWriter` partition, merged in extractor registration order and then chunk order.
- [Z2-N-ARCH-420] Flux never includes a header of or links `ZenithScene`, `ZenithECS` or `ZenithScript`; the render world is the one data path from simulation into Flux, and libgraph fails any edge from `ZenithFlux` to those libraries.

### Pipelining and latency

<!-- informative -->
This is the Destiny model: the extract node snapshots render-relevant state into an immutable render world for frame N; frame N's GPU-graph build, record and submit then run concurrently with frame N+1's simulation, in the same pool. It costs exactly one frame of CPU-side render lag and buys full overlap of simulation and render preparation without locks on game state. GPU frames in flight (two or three, configurable) are separate and are specified in Zenith2/Docs/Rendering.md → "GPU lifetime and synchronisation".
<!-- /informative -->

- [Z2-N-ARCH-421] The CPU pipeline depth is exactly one frame: simulation of frame N+1 overlaps rendering of frame N and never runs further ahead, and telemetry records `pipeline.depth = 1` with every frame.
- [Z2-N-ARCH-422] The extra frame of latency is accounted for in the input-latency budget row: each frame records the input sample time, simulation end, extraction seal, submit and present timestamps, and the input sequence number travels from the Input phase through `Flux_RenderWorld` to the present so the latency measurement can attribute each present to its input (Zenith2/Docs/Budgets.md → "Measured in every workload run").

## Worlds in the graph

- [Z2-N-ARCH-423] Each world contributes its own tick-boundary, phase, sync-point and extraction nodes in its own group; nodes of different worlds share no frame resource, so worlds tick concurrently and one world's failure or pause never blocks another world's nodes.
- [Z2-N-ARCH-424] A world being loaded in isolation for a world transition contributes no nodes to the running schedule until its swap at a tick boundary (Architecture.md → "Lifecycle commands").

## Cancellation and shutdown

- [Z2-N-ARCH-425] Background jobs are cancelled cooperatively through their `Zenith_OperationHandle`: `RequestCancel` sets the flag that `Zenith_JobContext::IsCancelled` reports, the job returns at its next check (within one 2 ms slice), releases what it holds on its own thread, and only then reports `OPERATION_STATUS_CANCELLED` (Zenith2/Docs/Contracts.md → "Threading and concurrency contract").
- [Z2-N-ARCH-426] Scheduler shutdown is orderly and follows the shutdown ownership order of Zenith2/Docs/Contracts.md → "Threading and concurrency contract": the executor first drains the in-flight frame (simulation N+1 finishes, rendering N submits).
- [Z2-N-ARCH-431] After the drain, the executor cancels every background operation and runs jobs until each reports completion.
- [Z2-N-ARCH-432] The job system's destructor then wakes every thread and joins them in reverse dependency order: the render-submit thread first (it consumes the workers' output), then the workers, which are peers, in descending index order.

## Profiling

- [Z2-N-ARCH-427] Every frame-graph node, sub-job, continuation and background job emits a profiling zone through the installed profiler hooks: nodes as `<NodeName>`, sub-jobs as `<NodeName>/[<begin>,<end>)`, continuations as `<NodeName>/cont<n>`, background jobs as `bg/<submitter name>`; the zones feed the on-screen profiler's per-worker timeline and Tracy.
- [Z2-N-ARCH-428] The scheduler publishes these counters every frame: `jobs.steals`, `jobs.steal_failures`, `jobs.inline_runs`, `jobs.idle_ns.<worker>`, `jobs.parked`, `jobs.ready_high_water`, the queue counters of § "Queues", `sim.ticks`, `sim.dropped_ticks`, `sim.alpha`, and `frame.schedule_recompiles`.
- [Z2-N-ARCH-433] The on-screen profiler's contention diagnostics (steal rate, idle time per worker, queue high-water marks) are computed from the counters of bullet 428 and nothing else.
- [Z2-N-ARCH-429] The frame schedule's JSON dump (the `zenith2.graph-dump` schema of Architecture.md → "ZenithGraph shared core", one document per subgraph, with timings) is available to the on-screen profiler every frame in development builds and is written on demand by the console command `jobs.dump <path>`.

## Required tests

The table "CPU frame graph tests" lists the tests of this section; they live in `Engine/ZenithJobs/Tests/` unless the table names another location, and they run in the `Test` step and, for the `Zenith2Memory` category, under `win-asan` and `linux-tsan`.

- [Z2-N-ARCH-430] Every test in the table "CPU frame graph tests" exists under the name given and asserts the case given.

CPU frame graph tests:

| Test | Asserts |
|---|---|
| `Test_JobSystem_WorkerCount_AutoIsCoresMinusTwo` | `m_uWorkerCount = 0` yields logical cores − 2 clamped to [1, 62] |
| `Test_JobSystem_DequeFull_RunsInline` | pushing past deque capacity runs the job inline and increments `jobs.inline_runs` |
| `Test_JobSystem_InjectionFull_ReturnsQueueFull` | a push from a thread with no job context that finds the injection queue full returns `JOBS_ERROR_QUEUE_FULL` with nothing queued; the caller retries and every job runs exactly once |
| `Test_JobSystem_BackgroundYieldsToFrameWork` | a frame node pushed while a background job runs starts before the next background slice |
| `Test_JobSystem_NoBlockingWaitInJobs` | the profiler blocking-wait hook reports zero waits from job bodies across a 1,000-frame run, waits reported with `szPrimitive = "debugger-stop"` excluded |
| `Test_JobSystem_Shutdown_JoinsAllThreads` | destruction joins the render-submit thread then workers in descending order (recorded order) |
| `Test_JobContext_ParallelFor_JoinsBeforeSuccessor` | a successor observes every sub-job's write for counts 0, 1, grain, grain + 1 and 100,000 |
| `Test_JobContext_ParallelFor_NestedJoins` | sub-jobs spawning sub-jobs are all joined before node completion |
| `Test_JobContext_Continue_RunsAfterPriorSubJobs` | a continuation sees all sub-jobs spawned before it and none spawned after |
| `Test_JobContext_Continue_ChainsInOrder` | three continuations run in registration order |
| `Test_JobContext_MergeOrder_IndependentOfWorkers` | a merge by index yields identical bytes with 1, 2, 8 and all workers |
| `Test_FrameGraph_MainAffinity_RunsOnMainThread` | main-affinity nodes run with worker index 0 |
| `Test_FrameGraph_RenderSubmitAffinity_RunsOnSubmitThread` | render-submit nodes run with index N+1 and the submit thread never runs other work |
| `Test_FrameGraph_StageBarrier_OrdersStages` | every node of stage p completes before the sync point, and the sync point before stage p+1 |
| `Test_FrameGraph_ReaderAfterWriterWithinStage` | a same-stage reader observes the writer's value |
| `Test_FrameGraph_BeforeFlipsReaderToPreWrite` | a reader declaring `Before(writer)` observes the prior value |
| `Test_FrameGraph_UnorderedWriters_IsError` | two same-stage writers fail with `FRAME_GRAPH_ERROR_UNORDERED_WRITERS` |
| `Test_FrameGraph_OrderingContradictsStage_IsError` | a backward `Before` fails with the message format above |
| `Test_FrameGraph_AfterIfPresent_IgnoresMissing` | an `AfterIfPresent` on an absent node compiles; `After` on it fails |
| `Test_FrameGraph_WorldsShareNoEdges` | two worlds' nodes have no edge between them and run concurrently |
| `Test_FrameGraph_ScheduleCache_HitsWithoutRecompile` | 1,000 frames with no trigger perform zero recompiles |
| `Test_FrameGraph_Recompile_OnWorldCreated` | creating a world recompiles once, at the next prologue |
| `Test_FrameGraph_RecompileFailure_KeepsPreviousSchedule` | an invalid change keeps the old schedule and reports the diagnostic |
| `Test_FrameGraph_DisabledNode_SkipsWithoutRecompile` | disabling a system completes its node immediately with zero recompiles |
| `Test_FrameGraph_CancelledInstance_Completes` | `RequestCancel` mid-frame completes every counter and returns from `RunFrame` |
| `Test_FrameGraph_PipelinedRender_OverlapsSimulation` | RENDER(k−1) and TICK(k) zones overlap in time on a 4-worker run |
| `Test_FrameGraph_SimulationNeverTwoFramesAhead` | across 10,000 frames, extraction k never starts before RENDER(k−2) completes |
| `Test_RenderWorld_ImmutableAfterSeal` | a builder write after `Seal` is rejected (assert disabled) and leaves the world unchanged |
| `Test_RenderWorld_RingSlotReuse` | slot `k % 2` is reset only after RENDER(k−2) completed |
| `Test_RenderWorld_HoldsNoSimulationPointer` | an extracted world stays valid after its source world is destroyed |
| `Test_SimulationClock_FixedTicks` | at 60 Hz, `GetTickDurationUs()` is 16,667, and 60 frames with a real delta of 16,667 µs each yield exactly one tick each, 60 in total |
| `Test_SimulationClock_CatchUpBounded` | with `m_uMaxCatchUpTicks = 4` (set by the test) and `m_uMaxFrameDeltaUs = 250000`, one frame with a real delta of 1,000,000 µs is clamped to 250,000 µs = 14 whole ticks (remainder 16,662 µs): 4 ticks run, 10 are dropped, `GetDroppedTickCount()` is 10 and the accumulator keeps 16,662 µs |
| `Test_SimulationClock_PauseRunsZeroTicks` | paused frames return 0 ticks and keep alpha constant |
| `Test_SimulationClock_TimeScale` | a 0.5 scale halves the tick rate over 10 s of real time |
| `Test_FrameGraph_ZonesForEveryNode` (in the `Zenith2/Tests/DevTools/` integration suite, which links ZenithProfiling, ZenithJobs and ZenithGraph) | a captured frame has one zone per executed node and per sub-job |
| `Test_FrameGraph_AudioUpdateOncePerFrame` (in `Engine/ZenithAudio/Tests/`) | three catch-up ticks in one frame run `Audio.Update` once, after the last tick, concurrently with extraction |
| `Test_FrameGraph_PrologueRunsNoJobs` | across 1,000 frames, no frame-graph job starts between the completion of iteration k−1 and `RunFrame(k)`, and the eight prologue steps run in order |
| `Test_FrameGraph_AssetsServiceEveryFrame` (in `Engine/ZenithRuntime/Tests/`) | windowed and headless, `Assets.Service` runs once per frame in EPILOGUE stage 12 of group 0 with no edge to or from any world node |
| `Test_FrameGraph_StressUnderTsan` | 10,000 frames of a 200-node random graph with sub-jobs run clean under `linux-tsan` |
