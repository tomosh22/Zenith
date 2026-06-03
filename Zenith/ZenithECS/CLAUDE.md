# ZenithECS — the ECS leaf library

`ZenithECS` is the **L1 leaf static library** of the engine: the generic
entity-component machinery (entities, scenes, queries, events, the component-meta
registry). It depends **only on `ZenithBase` (L0)** and has **no** link/symbol
dependency on `g_xEngine`, Flux, Physics/Jolt, UI, Editor, AssetHandling, AI, or
any concrete component. That boundary is enforced three ways: the Sharpmake
source partition (`Build/Sharpmake_ZenithECS.cs`), the `SentinelECS` link-proof
exe (links only `zenithecs.lib + zenithbase.lib`), and the ECS-leaf ratchet (B)
in `Tools/layering_gate.ps1` (one allowlisted edge — see below).

> **Library vs directory:** this directory IS the leaf — the files were physically
> relocated here out of `Zenith/EntityComponent/`. The concrete built-in components
> and the ECS↔engine glue (the registrar, camera resolver, editor registry) stay in
> [../EntityComponent/](../EntityComponent/CLAUDE.md) and compile into the aggregate
> engine lib, because they name concrete component / Flux / engine types.

## Files

ZenithECS uses a **public-headers-at-root, everything-else-in-`Internal/`** layout: only the headers engine/games/tools include *directly* live at the root; all `.cpp` and all private headers live under `Internal/`. A public header may pull a private `Internal/` header transitively — consumers only ever name the root headers.

The public surface was aggressively minimised (2026-06-02 API-reduction pass): **7 public headers** (below); ~30 implementation-detail types relocated into `Internal/` detail headers (consumers name only `Zenith_EventDispatcher` / `Zenith_ComponentMetaRegistry` / `Zenith_Query`, not the machinery behind them); ~50 members moved to `private:`/`friend`; scene metadata is fetched via the single `Zenith_SceneSystem::GetSceneInfo()` POD; and the editor reaches its `Zenith_SceneData` ops through the engine-side `Zenith/Editor/Zenith_EditorSceneAccess.h` friend-class wrapper rather than calling them directly.

### Public headers (`ZenithECS/` root)
- **`Zenith_SceneSystem.h`** — the single scene API, reached via `g_xEngine.Scenes()`. One class (`Zenith_SceneSystem`) that merged the former Registry / OperationQueue / LifecycleScheduler / CallbackBus / EntityOwnership subsystems. Inlines the `SCENE_LOAD_*` enum and the RAII guard types (`Zenith_LifecycleDeferralGuard`, `Zenith_PrefabInstantiationGuard`, `Zenith_SceneUpdateDeferralGuard`, `Zenith_SceneCreationTargetScope`). Pulls `Internal/Zenith_ECSRuntimeHooks.h` transitively. (The scene-lifecycle event structs that once lived here — `Zenith_Event_Scene{Loaded,Unloading,Unloaded}` / `Zenith_Event_ActiveSceneChanged` — were removed as unused; the system no longer emits scene-lifecycle notifications.) There is **no** `Zenith_SceneSystem.cpp` — its implementation is the `Internal/Zenith_SceneSystem_*.cpp` TUs (below). Regression suite: `Zenith/UnitTests/Zenith_SceneTests.h`.
- `Zenith_Scene.h` - Opaque scene handle struct (`int m_iHandle` + generation). Impl in `Internal/Zenith_Scene.cpp`.
- `Zenith_Entity.h` - Entity handle class. Its **sole** data member is the entity ID (`m_xEntityID`); all per-entity state (parent ID, child list, name, lifecycle flags) lives on the `Zenith_EntitySlot`, resolved through the slot. Impl in `Internal/Zenith_Entity.cpp`; template bodies in `Internal/Zenith_Entity.inl`.
- `Zenith_SceneData.h` - Scene storage (entity pools, components, metadata). At its bottom it pulls the private `Internal/Zenith_{ComponentPool,RenderTaskState,EntityStore}.h` + `Internal/Zenith_Entity.inl`. Most of its surface is now `private:` (component-mutation templates, lifecycle/state internals, the editor-only verbs behind `Zenith_EditorSceneAccess`); the public surface is per-entity/per-component resolution (`GetEntity`/`Query`/`EntityHasComponent`/...) + `Reset`/`Save`/`Load`. Impl in `Internal/Zenith_SceneData.cpp` + `Internal/Zenith_SceneData_Serialization.cpp` (`.zscen` read/write/validate).
- `Zenith_Query.h` - Multi-component entity queries (`ForEach` / `Count` / `First` / `Any`). Scratch-pool machinery is in `Internal/Zenith_QueryScratch.h`.
- `Zenith_EventSystem.h` - Type-safe event dispatcher with deferred queue (`Zenith_EventDispatcher`). Type-erasure layer is in `Internal/Zenith_EventSystem_Detail.h`. Impl in `Internal/Zenith_EventSystem.cpp`.
- `Zenith_ComponentMeta.h` - Component reflection/registration **machinery** (`Zenith_ComponentMetaRegistry` + the `ZENITH_REGISTER_COMPONENT*` macros). Names no concrete component — the engine installs the concrete set via a registrar (see "Registration" below). The reflection metaprogramming is in `Internal/Zenith_ComponentMeta_Detail.h`. Impl in `Internal/Zenith_ComponentMeta.cpp`.

A TU that calls the entity-component templates must include `Zenith_Scene.h` (which pulls `Zenith_SceneData.h` → `Internal/Zenith_Entity.inl`), not just `Zenith_Entity.h`.

### Internal/ (implementation TUs + private headers)
Nothing here is included by engine/games/tools directly — only by other ZenithECS files.

**Private headers:**
- `Internal/Zenith_ComponentPool.h` - Per-type dense component pool (real swap-and-pop removal) + the `Zenith_Component` concept.
- `Internal/Zenith_RenderTaskState.h` - Free-function forwarder `Zenith_AreRenderTasksActive()` (breaks a header cycle; leaf-safe predicate).
- `Internal/Zenith_ECSRuntimeHooks.h` - The leaf-safe hook struct the engine installs via `Zenith_SceneSystem::SetRuntimeHooks` (IsMainThread / ResetRenderSystems / UnloadUnusedAssets / ResetPhysics / AddDefaultComponents). All defaults safe — the leaf runs with null hooks (what the sentinel proves). Pulled in transitively by `Zenith_SceneSystem.h`.
- `Internal/Zenith_Entity.inl` - Template bodies for `AddComponent` / `GetComponent` / etc.
- `Internal/Zenith_EntityStore.h` - Process-wide entity slot storage (`Zenith_EntitySlot` + the three global arrays), owned by `Zenith_SceneSystem`, reached via the leaf forwarder `Zenith_ECS_EntityStore()`. Internalized in the API-reduction pass (8→7 public headers); still pulled inline by `Zenith_SceneData.h`'s hot accessors (deliberately NOT de-inlined, to preserve `--bench-ecs` perf — so the type stays transitively visible, but it is no longer a directly-includable public header).
- `Internal/Zenith_ComponentMeta_Detail.h` - The component-reflection metaprogramming (10 fn-ptr typedefs, `HasOn*`/`HasSchemaVersion` concepts, the `*Wrapper` templates) behind `Zenith_ComponentMetaRegistry`.
- `Internal/Zenith_EventSystem_Detail.h` - The event type-erasure layer (`Zenith_EventBase`/`Wrapper`, `Zenith_CallbackBase`/`Wrapper`/`LambdaCallbackWrapper`, `Zenith_EventTypeID`) behind `Zenith_EventDispatcher`.
- `Internal/Zenith_QueryScratch.h` - The per-thread query scratch-pool (`Zenith_QueryScratchPool`/`Checkout`) behind `Zenith_Query`.
- `Internal/Zenith_SceneSystem_InternalScopes.h` - Internal RAII scope (`Zenith_SceneCreationTargetScope`).

**Implementation TUs:**
- `Internal/Zenith_ECS.cpp` - The per-lib PCH-create TU (`#include "Zenith.h"` only). `ZenithECS` CREATEs its own `Zenith.pch` from this; every other ECS TU compiles `/Yu"Zenith.h"` against it.
- `Internal/Zenith_{Entity,Scene,SceneData,SceneData_Serialization,EventSystem,ComponentMeta}.cpp` - implementations of the matching public headers.
- The `Zenith_SceneSystem` implementation, split across TUs purely for file size — every function is a member of the single `Zenith_SceneSystem`; state lives as **private members**. `CreateEntity` / `CreateEntityBare` are public **instance** methods; the cross-scene ownership ops (`MoveEntityToScene` / `MarkEntityPersistent` / `Destroy*`) are **private** instance methods invoked by `Zenith_Entity`'s public lifecycle verbs (`Destroy` / `DestroyImmediate` / `DontDestroyOnLoad` / `MoveToScene`); bootstrap orchestrators (`InitialiseSubsystems` / `ShutdownSubsystems` / `ResetForNextTest`) are `static`:
  - `Internal/Zenith_SceneSystem_Registry.cpp` - Slot table, generations, freelist, name cache, build-index registry, scene queries, creation/activation, `CanonicalisePath`
  - `Internal/Zenith_SceneSystem_Operations.cpp` - `LoadScene` / `LoadSceneByIndex`, unload + bulk teardown, render-system reset (via the hook)
  - `Internal/Zenith_SceneSystem_Lifecycle.cpp` - Bootstrap, per-frame `Update`, fixed-timestep, circular-load stacks, RAII guard bodies, `Shutdown`, `Zenith_AreRenderTasksActive()` definition
  - `Internal/Zenith_SceneSystem_Callbacks.cpp` - the active-scene reselection-on-unload helper `FireUnloadCallbacksAndSelectNewActive` (the scene-lifecycle callback bus this TU once owned, and the later active-scene suppression scope, were both removed)
  - `Internal/Zenith_SceneSystem_EntityOwnership.cpp` - `MoveEntityToScene`, `MarkEntityPersistent`, `Destroy*` (cross-scene entity moves)

The full design (synchronous LoadScene flow, re-entrancy rules, state-access convention, Unity-parity invariants) is in [Internal/ARCHITECTURE.md](Internal/ARCHITECTURE.md).

### Concrete components live in the ENGINE lib, not here
The concrete built-in components live in [../EntityComponent/Components/](../EntityComponent/CLAUDE.md) and compile into the aggregate engine lib — they name Flux / Physics types and so cannot live in the leaf. The leaf stores and dispatches them generically through `Zenith_ComponentMeta` without ever naming a concrete type.

> **Zero concrete-component coupling.** The leaf has **no** reference to any concrete engine component — no include, no complete-type use, no `friend`, no forward declaration, not even a comment mention. The former `CollectResetHierarchy` concept-guard (which used to need a complete concrete-component type) was reworked to a slot-based root check (`Zenith_Entity::IsRoot()`, which reads the slot's parent link — Phase 5 relocated the hierarchy onto the slot), so the last leaf `#include` of a concrete component was dropped. `Tools/ecs_leaf_allowlist.txt` is now empty.

## Architecture

### Entity
A lightweight handle whose only data member is the entity ID (`m_xEntityID`). The hierarchy (parent ID, child ID list) and the entity name live on the `Zenith_EntitySlot` in the entity store, not on the handle; `GetName()`/`SetName()` and the parent/child accessors all delegate to the slot. Entity creation runs through `g_xEngine.Scenes().CreateEntity(...)` (the handle has no entity-creating constructor).

#### Why entity slots are global, not per-scene
The entity slot table (`m_axEntitySlots` on the process-wide `Zenith_EntityStore`, owned by `Zenith_SceneSystem` and reached via `Zenith_ECS_EntityStore()`) is intentionally global, not a per-scene pool. This keeps `EntityID.m_uIndex` stable when an entity moves between scenes (via `MoveEntityToScene` or persistence), so cached `Zenith_EntityID` handles survive cross-scene transfers. The slot entry itself records the owning scene handle, so destination resolution stays correct.

Safety: slot reuse uses a 32-bit generation counter. A stale handle's `IsValid()` check reads the current slot's generation and rejects the handle when they differ.

### Scene Management (Multi-Scene Architecture)
Zenith uses a multi-scene architecture inspired by Unity:
- **Zenith_Scene** - Opaque handle struct (`int m_iHandle` + `uint32_t m_uGeneration`). Copy freely. The generation counter lets `IsValid()` detect handles to scene slots that have been unloaded and recycled.
- **Zenith_SceneData** - Internal class storing entities, components, and metadata. Not accessed directly by game code.
- **Zenith_SceneSystem** - The single scene system, reached via `g_xEngine.Scenes()`. Owns the slot table, callbacks, (synchronous) load/unload, the per-frame `Update`, and cross-scene entity ownership.

Multiple scenes can be loaded simultaneously (additive loading). One scene is "active" at any time. Scene loading is **synchronous** — `LoadScene` returns a real `Zenith_Scene` once Awake/OnEnable have run and the scene has flipped to LOADED.

### Scene Loading Modes
| Mode | Enum | Behavior |
|------|------|----------|
| Single | `SCENE_LOAD_SINGLE` | Unload all non-persistent scenes, load new |
| Additive | `SCENE_LOAD_ADDITIVE` | Keep existing scenes, add new |
| Additive Without Loading | `SCENE_LOAD_ADDITIVE_WITHOUT_LOADING` | Create a new empty scene without reading from disk. Use for procedural/editor-new scenes. |

### Persistent Scene
A special scene that is always loaded and never unloaded. Call `entity.DontDestroyOnLoad()` to move an entity here (root entities only). Entities in the persistent scene survive `SCENE_LOAD_SINGLE` operations.

### Common Patterns
```cpp
// Get active scene and create entity (CreateEntity runs the engine-installed
// default-components hook, which adds the engine's default component(s); use
// CreateEntityBare for an entity with no default components — load/prefab uses bare).
Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(xScene, "MyEntity");

// Load scene additively (synchronous — returns a valid handle)
Zenith_Scene xLoaded = g_xEngine.Scenes().LoadScene("Levels/Level1.zscen", SCENE_LOAD_ADDITIVE);

// Entity lifecycle verbs live on Zenith_Entity:
xPlayerEntity.DontDestroyOnLoad();   // persist across scene loads
xEntity.MoveToScene(xTargetScene);   // updates the handle in-place
xEntity.Destroy();                   // end of frame
xEntity.DestroyImmediate();          // this frame (editor / tests)

// NOTE: the built-in scene-lifecycle events (Zenith_Event_Scene{Loaded,
// Unloading,Unloaded} / Zenith_Event_ActiveSceneChanged) were removed as
// unused. Zenith_EventDispatcher remains for game-defined events — Subscribe
// with your own payload type.
```

### Component Pools
Each component type has a dedicated `Zenith_ComponentPool<T>` (`Zenith_ComponentPool.h`). The pool is **dense**: live components occupy slots `[0, m_uSize)` with no holes; the per-entity component index is stored in exactly one place — `Zenith_ECS_EntityStore().m_axEntityComponents.Get(entityIdx)[typeID]`.

**Swap-and-Pop Removal (real):** `RemoveAtSwapAndPop(uIndex)` captures the owner of the last live element, destructs the slot at `uIndex`, and — unless it was already last — **move-constructs** (not memcpy, so components owning VRAM / Jolt handles transfer ownership) the last element into `uIndex`, then decrements `m_uSize`. It returns the `Zenith_EntityID` that owned the moved element. The single call site (`Zenith_SceneData::RemoveComponentFromEntity<T>`) repoints the moved owner's stored index. Component indices are therefore unstable after any removal — never cache a raw pool index across a removal; look it up through `m_axEntityComponents`.

## Component Meta System

`Zenith_ComponentMeta` provides type-erased component operations using function pointers (no `std::function`). The **machinery** is the leaf; the **concrete component set is installed by the engine** (registration is inverted so the leaf names no concrete type):

- The engine calls `Zenith_ComponentMetaRegistry::Get().SetComponentRegistrar(&Zenith_RegisterEngineComponents)` then `EnsureInitialized()` in `Zenith_Engine::Initialise`. `Zenith_RegisterEngineComponents()` (in `../EntityComponent/Zenith_ComponentMeta_Registration.cpp`) calls `RegisterComponent<T>(name, order)` for each built-in with an explicit serialization order, plus `Zenith_AI_RegisterComponents()`.
- Game/script components register via the `ZENITH_REGISTER_COMPONENT(Type, "Name"[, order])` macro (which enqueues a thunk drained by `EnsureInitialized()`) or the `ZENITH_BEHAVIOUR_TYPE_NAME` script macro.

### Lifecycle Hooks
Components may implement (detected at compile-time via C++20 concepts): `OnAwake`, `OnStart`, `OnEnable`, `OnDisable`, `OnUpdate(float)`, `OnLateUpdate(float)`, `OnFixedUpdate(float)`, `OnDestroy`. Missing hooks → null function pointer, skipped. Dispatch via `Zenith_ComponentMetaRegistry::DispatchOn*()`.

## Query System
```cpp
scene.Query<ComponentA, ComponentB>()
    .ForEach([](Zenith_EntityID uID, ComponentA& xA, ComponentB& xB) { /* ... */ });
```
Verbs: `ForEach(callback)`, `Count()`, `First()` (or `INVALID_ENTITY_ID`), `Any()`. Cross-scene form: `g_xEngine.Scenes().QueryAllScenes<Ts...>()`.

## Event System
Type-safe dispatcher with immediate (`Dispatch`) and deferred (`QueueEvent` + `ProcessDeferredEvents`, thread-safe) modes; `Subscribe<EventType>(callable)` (capturing lambdas supported via `Zenith_CallbackWrapper`) / `Unsubscribe`. The dispatcher is fully generic — the engine defines no built-in event payloads (the former scene-lifecycle events were removed as unused); games define and dispatch their own payload structs.

## Key Concepts
**Serialization:** components implement `WriteToDataStream()`/`ReadFromDataStream()`; the ComponentMeta system enforces serialization order (lower = first). Scene/prefab format is v7 (v6→v7 migration reads the legacy parent file-index out of the owning component's payload and routes it through `Zenith_SceneData::SetPendingParentFileIndex` → `ResolvePendingParents`).

**Scene Loading Reset Order** (`SCENE_LOAD_SINGLE`): render systems reset (via the runtime hook) → `SceneData::Reset()` (destroys entities/components) → `Zenith_Physics::Reset()` (via the hook). Physics reset MUST come after scene reset because collider destructors need the physics world. See [../Physics/CLAUDE.md](../Physics/CLAUDE.md).

## Design Rationale (why `Zenith_Entity` is a class, not a POD)
Entity lifecycle is real work — creation allocates a slot, registers with the scene, runs the engine-installed default-components hook; destruction/persistence/cross-scene moves run non-trivial bookkeeping. `SetEnabled()` validates and dispatches `OnEnable`/`OnDisable`. The handle is a freely-copyable value type (slot index + generation). Converting to a POD would scatter this logic without reducing it.
