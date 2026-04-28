# Scene system internals

This document describes the post-refactor architecture of the scene system —
five internal subsystem TUs under `Zenith/EntityComponent/Internal/` plus a
slim public facade (`Zenith_SceneManager`).

## Subsystem boundaries

```
                       Zenith_SceneManager           ← public facade
                       (~ thin forwarders, RAII guard
                        types, blocking helpers)
                              │
            ┌─────────────────┼─────────────────────┬─────────────────┐
            │                 │                     │                 │
            ▼                 ▼                     ▼                 ▼
   Zenith_SceneRegistry  Zenith_SceneCallbackBus  Zenith_SceneOperationQueue  Zenith_SceneLifecycleScheduler
        │                                                                        │
        │                                                                        │
        └────────────────────────── Zenith_SceneEntityOwnership ─────────────────┘
                            (cross-scene moves, persistence,
                             destruction)
```

| Subsystem | Owns |
|-----------|------|
| **`Zenith_SceneCallbackBus`** | Six callback lists, handle allocator, deferred-removal queue, fire-depth counter, `ActiveSceneChangeSuppressionScope`. |
| **`Zenith_SceneRegistry`** | Slot table, generations, freelist, persistent + active scene handles, name cache, build-index registry, scene queries, `RenameScene`. |
| **`Zenith_SceneOperationQueue`** | Operation map, async load + unload jobs, Phase 1 / Phase 2 phase machines, queue-stall predicate, async config knobs, `CompletePriorOperationsForBlockingLoad`. |
| **`Zenith_SceneLifecycleScheduler`** | Per-frame `Update`, fixed-timestep accumulator, lifecycle-deferral flags (`s_bIsLoadingScene`, `s_bIsPrefabInstantiating`, `s_bIsUpdating`, `s_bIsMainLoopRunning`), two circular-load stacks, animation task globals, creation-target stack. |
| **`Zenith_SceneEntityOwnership`** | `MoveEntityToScene`, `MoveEntityInternal`, `MergeScenes`, `MarkEntityPersistent`, `Destroy*`. |

## Cross-subsystem boundary contract

Two rules govern how the subsystems talk to each other:

1. **All cross-subsystem reads go through `Zenith_SceneLifecycleContext`**
   (`Internal/Zenith_SceneLifecycleContext.h`). It is the single read-side
   surface — a free-function namespace with accessors like `IsLoadingScene()`,
   `IsUpdating()`, `IsMainLoopRunning()`, `GetCurrentCreationTarget()`,
   `IsActiveSceneSuppressed()`. No subsystem reads another's private statics.

2. **All cross-subsystem writes go through public RAII types declared on
   `Zenith_SceneManager`**:
   - `LifecycleDeferralGuard` — save/restore on a `bool&` (test framework + bootstrap)
   - `PrefabInstantiationGuard` — `s_bIsPrefabInstantiating` save/restore
   - `SceneUpdateDeferralGuard` — `s_bIsUpdating` save/restore
   - `PendingBuildIndexGuard` — `s_iPendingBuildIndex` save/restore
   - `ActiveSceneChangeSuppressionScope` — bus-owned suppression with required `Complete()`/`Cancel()`
   - `SceneCreationTargetScope` — push/pop on the creation-target stack

This means every cross-subsystem state mutation is visible at its call site as
an RAII declaration, not an opaque boolean flip.

## `LoadScene` contract (post-B4)

`Zenith_SceneManager::LoadScene(path, mode)` and `LoadSceneByIndex(idx, mode)`
are **queue-and-defer**. They:

1. Validate the path / index.
2. Call `Zenith_SceneOperationQueue::CompletePriorOperationsForBlockingLoad()`
   to flush any in-flight async load + unload ops (Unity flush-prior-async
   semantic — top-level only; skipped under re-entrancy).
3. Queue the load via `LoadSceneAsync*`.
4. Stash the resulting op-id in `s_ulLastDeferredLoadOp`.
5. Return `Zenith_Scene::INVALID_SCENE`.

The new scene's lifecycle (`Awake → OnEnable → SceneLoaded`) fires during a
subsequent `Zenith_SceneManager::Update` tick, inside Phase 1 / Phase 2 of the
queue's job machine.

### Recovering the result

| Caller context | API |
|----------------|-----|
| **Pre-main-loop bootstrap** (per-game `Project_LoadInitialScene`) | `LoadSceneBlockingForBootstrap` / `LoadSceneByIndexBlockingForBootstrap` — pumps `Update` internally; returns a real `Zenith_Scene`. Asserts it's only invoked when `IsBootstrapLoadContext()` holds (pre-main-loop **or** inside a `LifecycleDeferralGuard`). |
| **Editor commands** (Open Scene menu, Play/Stop transitions) | `LoadSceneBlocking_ToolsOnly` / `LoadSceneByIndexBlocking_ToolsOnly` — same pumping behaviour, no main-loop guard, compiled out of non-tools builds. |
| **Gameplay** (script `OnUpdate`, UI button callbacks) | `LoadScene` queues; recover via `GetLastDeferredLoadOp()` and `GetOperation(opId)->GetResultScene()` once `IsComplete()` is true. |
| **Async-aware code paths** | `LoadSceneAsync*` directly; retain the operation id. |

### Re-entrancy

`CompletePriorOperationsForBlockingLoad` and the blocking helpers' internal
pump both early-return when:

- `Zenith_SceneOperationQueue::s_uProcessingAsyncLoadsDepth > 0` (we're inside
  `ProcessPendingAsyncLoads`, e.g. dispatching a `SceneLoaded` callback), or
- `Zenith_SceneLifecycleContext::IsUpdating()` (we're inside the engine's
  `Update` tick).

In those contexts the queue is mid-process; pumping it again would re-enter
the firing op's Phase 2. The new op is still queued and recoverable via
`GetLastDeferredLoadOp()` — the helper just degrades to the queue-and-defer
contract instead of forcing a synchronous result.

## Other Unity-parity invariants worth knowing

- **`SCENE_LOAD_SINGLE` auto-fires `UnloadUnusedAssets()`** after
  `UnloadAllNonPersistent` and before `Physics::Reset`, in both the sync sync
  teardown path (`PerformSingleModeTeardownAndSwap`-equivalent inside Phase 2)
  and the async Phase 1 SINGLE branch.
- **AsyncOperation queue stalls behind an activation-paused load head**
  (`SetActivationAllowed(false)` at progress 0.9). Both `ProcessPendingAsyncLoads`
  (for behind-the-head jobs at index `> 0`) and `ProcessPendingAsyncUnloads`
  (entire pass) gate on `IsAsyncQueueBlockedByActivationPausedHead()`.
- **`MarkEntityPersistent` is strict root-only.** Non-root callers are rejected
  with a logged error and no scene change. Callers wanting subtree promotion
  walk to the hierarchy root first; the editor hierarchy panel does this when
  the user picks "Move to DontDestroyOnLoad" on a non-root entity.
- **`SceneCreationTargetScope` drives `GetDefaultCreationScene()`.** While a
  scope is open (around scene load, deserialization, and activation),
  `Zenith_SceneManager::CreateEntity(name)` and `Zenith_Prefab::Instantiate(name)`
  target the loading scene rather than the active scene.
- **`GetSceneDataForEntity(uID)` is the correct ownership predicate.**
  `Zenith_SceneData::EntityExists(uID)` reads the process-wide slot table —
  it does **not** prove the slot is owned by the receiving scene. Always
  resolve through the manager helper or `Zenith_Entity::GetScene()` /
  `Zenith_Entity::GetSceneData()` for ownership checks across multi-scene
  setups.
