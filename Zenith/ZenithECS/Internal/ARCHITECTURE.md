# Scene system internals

The scene system is **one class** — `Zenith_SceneSystem` — reached everywhere via
`g_xEngine.Scenes()`. The former five subsystems (Registry, OperationQueue,
LifecycleScheduler, CallbackBus, EntityOwnership) were merged into it; there are
no per-subsystem classes or accessors any more. Scene loading is **fully
synchronous** — the old async operation-queue pipeline was retired.

## File layout

The class is declared in `ZenithECS/Zenith_SceneSystem.h`. Its
implementation is split across translation units purely for file size — every
function below is a member of the same `Zenith_SceneSystem`:

| TU | Owns |
|----|------|
| `Internal/Zenith_SceneSystem_Registry.cpp` | Slot table, generations, freelist, persistent/active handles, name cache, build-index registry, scene queries, `AllocateEmptyScene`, `SetActiveScene`, `CanonicalisePath`. |
| `Internal/Zenith_SceneSystem_Operations.cpp` | `LoadScene`/`LoadSceneByIndex`, `UnloadScene`/`UnloadSceneForced`, bulk teardown (`UnloadAllNonPersistent` + helpers), render-system reset. |
| `Internal/Zenith_SceneSystem_Lifecycle.cpp` | Bootstrap (`InitialiseSubsystems`/`ShutdownSubsystems`/`ResetForNextTest`), per-frame `Update`, fixed-timestep accumulator, circular-load stacks, creation-target stack, the RAII guard bodies, `Shutdown`. |
| `Internal/Zenith_SceneSystem_Callbacks.cpp` | The active-scene reselection-on-unload helper `FireUnloadCallbacksAndSelectNewActive`. (The callback bus this TU once owned, and the later active-scene suppression scope, were both removed.) |
| `Internal/Zenith_SceneSystem_EntityOwnership.cpp` | `CreateEntity`, `MoveEntityToScene`/`MoveEntityInternal`, `MarkEntityPersistent`, `Destroy*`. |

There is **no** `Zenith_SceneSystem.cpp`.

## State access convention

All data members are **private**. Because every implementation TU defines
members of the same class, they reach the state directly:

- **Instance methods** (Registry / Operations / Lifecycle / Callbacks, plus the
  entity-creation API `CreateEntity` / `CreateEntityBare` and the now-private
  cross-scene ownership ops `MoveEntityToScene` / `MarkEntityPersistent` /
  `Destroy*`) use bare `m_xXxx` / `Foo()` — `this` already is the
  singleton. The ownership ops are private; `Zenith_Entity` (a friend) exposes the
  public lifecycle verbs (`Destroy` / `DestroyImmediate` / `DontDestroyOnLoad` /
  `MoveToScene`) and forwards to them through `Zenith_SceneSystem::Get()`.
- **Static methods** (the bootstrap orchestrators — `InitialiseSubsystems` /
  `ShutdownSubsystems` / `ResetForNextTest`) have no `this`, so they reach the
  singleton through `Zenith_SceneSystem::Get()`.
- **External code** never touches the members. The one controlled hole is
  `MutableLifecycleLoadingFlagForGuard()`, which hands the bootstrap a `bool&`
  for a `Zenith_LifecycleDeferralGuard`.

`Zenith_SceneData` declares `friend class Zenith_SceneSystem` so the system can
reach scene-data privates. The RAII guard structs are friended so their
ctor/dtor bodies can flip the lifecycle flags.

## `LoadScene` contract (synchronous)

`LoadScene(path, mode)` does everything before returning a real `Zenith_Scene`:

1. Validate the path; reject if empty / missing / circular.
2. If called **re-entrantly** (`m_bIsUpdating` or `m_bIsLoadingScene` set), stash
   the request in `m_xPendingLoad` and return `INVALID_SCENE`. The stash drains
   once the outer pass unwinds — via `Zenith_SceneUpdateDeferralGuard`'s
   destructor, the `Update` post-amble, or `LoadScene`'s own post-dispatch drain.
   Only the most recent request survives (chained loads collapse to the last).
3. For `SCENE_LOAD_SINGLE`: tear the old world down **first** — reset render
   systems, `UnloadAllNonPersistent`, `UnloadUnusedAssets`, then `Physics::Reset`
   (physics last, so collider destructors still have a world to remove bodies
   from). The intermediate active-handle flip to `INVALID` during the teardown is
   not externally observable — the `ActiveSceneChanged` event that this once
   suppressed (via an `ActiveSceneChangeSuppressionScope`) was removed as unused,
   so both the event and the suppression scope are gone.
4. Create the new scene, deserialise into the post-reset subsystems, dispatch
   `Awake → OnEnable`, then flip to `SCENE_STATE_LOADED`. `Start`
   runs on the first subsequent `Update` (Unity timing).
5. Clear `m_bIsLoadingScene` and drain any stashed request.

`LoadSceneByIndex` saves/restores `m_iPendingBuildIndex` around a `LoadScene`
call so the new scene gets stamped with the build index.

`LoadScene(SINGLE)` only checks that the file **exists** before tearing down the
current world — it does **not** pre-validate the contents. A file that exists but
fails to deserialise (corrupt body, unsupported version) is caught *after*
teardown: the half-built scene is rolled back (`UnloadSceneForced`) and
`INVALID_SCENE` is returned, but the previous world is already gone, so the
engine is left scene-less. Don't rely on SINGLE being atomic across a bad file —
validate up front, or stage via ADDITIVE, if that matters.

## Re-entrancy safety

- **Circular load**: `m_axCurrentlyLoadingPaths` (set during the file read) and
  `m_axLifecycleLoadStack` (pushed around `Awake` dispatch) both feed
  `IsCircularLoadDependency`, catching a scene that tries to load itself from a
  ctor or an `OnAwake`.

## Unity-parity invariants worth knowing

- **`SCENE_LOAD_SINGLE` auto-fires `UnloadUnusedAssets()`** between teardown and
  the new scene's load.
- **The persistent ("DontDestroyOnLoad") scene** is created un-activated and can
  never be the active scene. `MarkEntityPersistent` is strict root-only; non-root
  callers are rejected with a logged error (walk to the root yourself first).
- **Entity creation always names its target scene explicitly** —
  `CreateEntity(Zenith_Scene, name)` / `CreateEntity(Zenith_SceneData*, name)`.
  There is no implicit "creation-target scope": the loader and prefab paths create
  entities directly into the `Zenith_SceneData*` they are populating (the loader via
  the private `Zenith_SceneData::CreateEntity()` slot allocator;
  `Zenith_Prefab::Instantiate` via `CreateEntity(pxSceneData, name)`).
- **`GetSceneDataForEntity(id)` is the correct cross-scene ownership predicate.**
  `Zenith_SceneData::EntityExists(id)` only reads the process-wide slot table — it
  does not prove the slot is owned by a particular scene.
- **Entity destruction is deferred to the next `Update`** (`Destroy` marks;
  `ProcessPendingDestructions` drains). `DestroyImmediate` is synchronous.
  `HasPendingDestructions()` reports whether any loaded scene still has marked-
  but-not-drained entities (timed `Destroy(e, delay)` is excluded).
