# EntityComponent — concrete components + ECS↔engine glue

This directory holds the **engine-side** half of the entity-component system: the
concrete built-in components and the glue that wires the generic ECS to the rest
of the engine. Everything here compiles into the aggregate **`Zenith`** engine lib
(NOT the leaf), because these files name concrete component / Flux / Physics /
editor types.

> The **generic ECS machinery** — entities, scenes, queries, events, the
> component-meta registry, the entity store — is the **`ZenithECS` leaf library**,
> physically located in [../ZenithECS/](../ZenithECS/CLAUDE.md). Read that doc for
> the ECS core (`Zenith_SceneSystem`, `Zenith_Entity`, `Zenith_Query`,
> `Zenith_EventDispatcher`, `Zenith_ComponentMeta` machinery, component pools,
> scene loading, lifecycle). This doc covers only what stays engine-side.

## Files

### `Components/` — the concrete built-in components
Each component lives in `Components/Zenith_*Component.{h,cpp}` and self-registers
via `ZENITH_REGISTER_COMPONENT` semantics (the explicit set + order is installed by
`Zenith_ComponentMeta_Registration.cpp`, below):

| Component | Purpose |
|---|---|
| `Zenith_TransformComponent` | Position, rotation, scale, model matrix; reads ECS slot hierarchy for parenting |
| `Zenith_CameraComponent` | View/projection matrices |
| `Zenith_ModelComponent` | Renderable mesh with material |
| `Zenith_LightComponent` | Dynamic lights |
| `Zenith_ColliderComponent` | Physics collision shapes (Jolt) |
| `Zenith_TerrainComponent` | Heightmap-based terrain |
| `Zenith_InstancedMeshComponent` | GPU-instanced mesh rendering |
| `Zenith_ParticleEmitterComponent` | Particle effect emitters |
| `Zenith_GraphComponent` | Behaviour Graph host (designer-authored .bgraph logic; see below) |
| `Zenith_UIComponent` | UI element support |
| `Zenith_AnimatorComponent` | Skeletal animation state machine (separate from ModelComponent) |
| `Zenith_TweenComponent` | Property tweening |

These name Flux / Physics types and so cannot live in the leaf. The leaf stores and
dispatches them generically through `Zenith_ComponentMeta` without naming a concrete
type.

### ECS↔engine glue (the wiring the leaf cannot contain)
- **`Zenith_ComponentMeta_Registration.cpp`** — defines `Zenith_RegisterEngineComponents()`, which calls `RegisterComponent<T>(name, order)` for the 12 built-ins (Transform=0, Model=10, Tween=12, Animator=15, Camera=20, Light=25, Terrain=40, Collider=50, Graph=60, UI=70, InstancedMesh=80, ParticleEmitter=85) then `Zenith_AI_RegisterComponents()` (AIAgent=90). In `ZENITH_TOOLS` builds it also mirrors the set into the editor "Add Component" registry. `Zenith_Engine::Initialise` installs this via `SetComponentRegistrar(&Zenith_RegisterEngineComponents)` then `EnsureInitialized()`. **Orders are centralised here.**
- **`Zenith_CameraResolve.h/cpp`** — the engine-side main-camera resolver. The leaf exposes only `FindMainCameraEntityAcrossScenes()` / `GetMainCameraEntity()` (EntityID-only); `Zenith_GetMainCamera()` / `Zenith_GetMainCameraAcrossScenes()` here wrap that and resolve to a `Zenith_CameraComponent&` (naming the concrete type, hence engine-side).
- **`Zenith_ComponentEditorRegistry.h/cpp`** — the editor "Add Component" registry (display name + has/render/add callbacks per component type). Consumed by the editor property/hierarchy panels.

## How it wires to the leaf
`Zenith_Engine::Initialise` (`../Core/Zenith_Engine.cpp`) installs everything the
leaf needs through leaf-safe seams, so the leaf names no engine symbol:
1. `SetComponentRegistrar(&Zenith_RegisterEngineComponents)` + `EnsureInitialized()` — the concrete component set.
2. `Scenes().SetRuntimeHooks(xHooks)` — a `Zenith_ECSRuntimeHooks` with `m_pfnIsMainThread`, `m_pfnResetRenderSystems`, `m_pfnUnloadUnusedAssets`, `m_pfnResetPhysics`, and `m_pfnAddDefaultComponents` (the last adds a `Zenith_TransformComponent` on non-bare `CreateEntity`).

The leaf works with null hooks and no registrar (that is exactly what the
`SentinelECS` link-proof exercises); the engine just installs richer behaviour.

## Gameplay logic: Behaviour Graphs + game components
The old C++ script-behaviour system was fully removed. Gameplay logic now has
two homes:

1. **Behaviour Graphs** (`Zenith_GraphComponent` + `.bgraph` assets) — designer-authored,
   hot-reloadable node graphs interpreted by `Zenith/Scripting/`. Slots hold
   `{asset path, graph instance, blackboard overrides}`; attach via the editor panel,
   content-browser drag-drop, or `Zenith_EditorAutomation::AddStep_AttachGraph("game:Graphs/Foo.bgraph")`.
2. **Game ECS components** (systems-tier C++) — plain classes registered with
   `ZENITH_REGISTER_COMPONENT(Type, "Name", order)` from the game's main .cpp; the
   meta registry concept-detects the lifecycle hooks (`OnAwake`/`OnStart`/`OnUpdate`/
   `OnCollisionEnter`/...). Per-entity tuning uses `ZENITH_PROPERTY`.

Doctrine: systems are ECS components (C++); gameplay logic is graphs.

## Static-init dead-strip pitfall
`ZENITH_REGISTER_COMPONENT` does not fire if the component's `.obj` is never
referenced (MSVC dead-strips it). Engine components are safe (the registrar names
them explicitly); a game component that is only added at runtime can hit this —
add it from code (`AddComponent<T>()` in `OnAwake`) rather than relying on the
static registrar.
