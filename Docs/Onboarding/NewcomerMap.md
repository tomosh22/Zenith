# Zenith Newcomer Map

A starter map for someone new to the Zenith codebase. Read this top-to-bottom on your first day; revisit individual sections when you change subsystems.

If you only have ten minutes, read **Startup Flow** and **Recommended Reading Path** below, then pick a "Safe First Area" task.

---

## 1. Startup Flow

The engine boots in a fixed sequence. Knowing this order is enough to debug 80% of "why isn't X working at startup?" questions.

```
main()                                   Zenith/Windows/Zenith_Windows_Main.cpp
  └─ Zenith_Core::Zenith_Main()          Zenith/Core/Zenith_Main.cpp:231
       ├─ Zenith_Window::Initialise()    GLFW window
       ├─ Zenith_Init()                  Zenith/Core/Zenith_Main.cpp:59
       │     ├─ MemoryManagement
       │     ├─ Multithreading           main thread registers itself
       │     ├─ Profiling
       │     ├─ TaskSystem               spawns worker threads
       │     ├─ AssetRegistry            phase 1: register loaders
       │     ├─ Flux::EarlyInitialise    Vulkan device, before assets need GPU
       │     ├─ Physics
       │     ├─ SceneManager
       │     ├─ AssetRegistry            phase 2: GPU-dependent assets
       │     ├─ Flux::LateInitialise     pipelines + render graph
       │     ├─ Editor::Initialise       (ZENITH_TOOLS only)
       │     └─ Project_RegisterGameComponents()
       ├─ while (!ShouldClose) Zenith_MainLoop()
       └─ Zenith_Shutdown()              reverse order of Init
```

Per-frame loop: input → editor update (if tools) → physics → scene update → render graph compile/execute → swapchain present.

Shutdown is **not** symmetric to init in two places:
- `SceneManager::Reset()` runs before `Physics::Reset()` because collider component destructors need the physics world to remove their bodies. Documented in [Physics/CLAUDE.md](../../Zenith/Physics/CLAUDE.md).
- `Flux::Reset()` happens before scene reset because GPU resources owned by components need to flush.

---

## 2. Recommended Reading Path

Five subsystems, easiest to hardest. Read each subsystem's CLAUDE.md first, then skim the main header.

| Order | Subsystem | Why it's a good start |
|---|---|---|
| 1 | [Input](../../Zenith/Input/CLAUDE.md) | Tiny static API. `IsKeyDown` / `WasKeyPressedThisFrame`. No hidden state beyond `BeginFrame()`. |
| 2 | [DataStream](../../Zenith/DataStream/CLAUDE.md) | Cursor-based binary serialisation. Used everywhere, easy to grasp. |
| 3 | [Collections](../../Zenith/Collections/CLAUDE.md) | `Zenith_Vector` / `Zenith_HashMap` / `Zenith_CircularQueue` / `Zenith_MemoryPool`. You'll see these in every subsystem. |
| 4 | [Sokoban game](../../Games/Sokoban/CLAUDE.md) | A complete game in <1500 lines. Demonstrates project hooks, behaviours, scenes, prefabs. |
| 5 | [TaskSystem](../../Zenith/TaskSystem/CLAUDE.md) | Flat work-stealing pool. **Not a dependency graph** — see the header comment. |

After those five, you'll be ready for the harder ones in any order:
[ECS](../../Zenith/EntityComponent/CLAUDE.md),
[Flux renderer](../../Zenith/Flux/CLAUDE.md),
[Vulkan backend](../../Zenith/Vulkan/CLAUDE.md),
[AI](../../Zenith/AI/CLAUDE.md),
[Editor](../../Zenith/Editor/CLAUDE.md).

---

## 3. Subsystem Ownership

| Subsystem | Primary docs | Primary header |
|---|---|---|
| Core / main loop | [Core/CLAUDE.md](../../Zenith/Core/CLAUDE.md) | `Zenith/Core/Zenith_Core.h`, `Zenith.h` |
| ECS | [EntityComponent/CLAUDE.md](../../Zenith/EntityComponent/CLAUDE.md) | `Zenith_SceneManager.h`, `Zenith_Entity.h`, `Zenith_Scene.h` |
| Renderer | [Flux/CLAUDE.md](../../Zenith/Flux/CLAUDE.md) | `Flux.h`, `Flux_RenderGraph.h`, `Flux_ShaderBinder.h` |
| Vulkan backend | [Vulkan/CLAUDE.md](../../Zenith/Vulkan/CLAUDE.md) | `Zenith_Vulkan.h`, `Zenith_PlatformGraphics_Include.h` |
| Physics | [Physics/CLAUDE.md](../../Zenith/Physics/CLAUDE.md) | `Zenith_Physics.h`, `Zenith_ColliderComponent.h` |
| Tasks | [TaskSystem/CLAUDE.md](../../Zenith/TaskSystem/CLAUDE.md) | `Zenith_TaskSystem.h` |
| AI | [AI/CLAUDE.md](../../Zenith/AI/CLAUDE.md) | `Zenith_NavMesh.h`, `Zenith_AIAgentComponent.h`, `Zenith_PerceptionSystem.h` |
| Assets | [AssetHandling/CLAUDE.md](../../Zenith/AssetHandling/CLAUDE.md) | `Zenith_AssetRegistry.h`, `Zenith_AssetHandle.h` |
| Prefabs | [Prefab/CLAUDE.md](../../Zenith/Prefab/CLAUDE.md) | `Zenith_Prefab.h` |
| Editor | [Editor/CLAUDE.md](../../Zenith/Editor/CLAUDE.md) | `Zenith_Editor.h` |
| UI | [UI/CLAUDE.md](../../Zenith/UI/CLAUDE.md) | `Zenith_UICanvas.h`, `Zenith_UIElement.h` |
| Input | [Input/CLAUDE.md](../../Zenith/Input/CLAUDE.md) | `Zenith_Input.h` |
| DataStream | [DataStream/CLAUDE.md](../../Zenith/DataStream/CLAUDE.md) | `Zenith_DataStream.h` |
| Collections | [Collections/CLAUDE.md](../../Zenith/Collections/CLAUDE.md) | `Zenith_Vector.h`, `Zenith_HashMap.h`, etc. |
| Maths | [Maths/CLAUDE.md](../../Zenith/Maths/CLAUDE.md) | `Zenith_Maths.h` |
| Profiling | [Profiling/CLAUDE.md](../../Zenith/Profiling/CLAUDE.md) | `Zenith_Profiling.h` |
| Debug variables | [DebugVariables/CLAUDE.md](../../Zenith/DebugVariables/CLAUDE.md) | `Zenith_DebugVariables.h` |
| Save data | [SaveData/CLAUDE.md](../../Zenith/SaveData/CLAUDE.md) | `Zenith_SaveData.h` |
| File access | [FileAccess/CLAUDE.md](../../Zenith/FileAccess/CLAUDE.md) | `Zenith_FileAccess.h` |
| Windows platform | [Windows/CLAUDE.md](../../Zenith/Windows/CLAUDE.md) | `Zenith_Windows_Window.h` |
| Android platform | [Android/CLAUDE.md](../../Zenith/Android/CLAUDE.md) | platform-specific |

---

## 3.5 Where the Real Implementation Lives

Two subsystems hide their implementation behind a public-facing facade. If you're searching for "who actually does X", look here.

### ECS — `Zenith_SceneManager` is a static facade

`Zenith_SceneManager` exposes ~120 static methods. Most are one-line forwarders to one of six `Internal/` subsystems. To trace any operation:

| Concern in SceneManager | Owner in `EntityComponent/Internal/` |
|---|---|
| Scene slot table, generations, name cache | `Zenith_SceneRegistry` |
| Scene callbacks (`Loaded`, `Unloading`, `ActiveSceneChanged`) | `Zenith_SceneCallbackBus` |
| Async load/unload queue, operation IDs | `Zenith_SceneOperationQueue` |
| Per-frame `Update`, OnAwake/OnEnable/OnStart/OnUpdate dispatch, lifecycle flags | `Zenith_SceneLifecycleScheduler` |
| `MoveEntityToScene`, persistent entities, slot ownership | `Zenith_SceneEntityOwnership` |
| Per-frame lifecycle state container (read-side surface) | `Zenith_SceneLifecycleContext` |

Boundary contracts and re-entrancy rules: see [EntityComponent/Internal/ARCHITECTURE.md](../../Zenith/EntityComponent/Internal/ARCHITECTURE.md).

### Flux renderer — every subsystem registers passes with the render graph

`Flux/CLAUDE.md` lists each subsystem (StaticMeshes, Shadows, DeferredShading, SSR, SSGI, HiZ, Fog, etc.) but the real "what runs and when" question is answered by:

- **The render graph** at [Flux/RenderGraph/CLAUDE.md](../../Zenith/Flux/RenderGraph/CLAUDE.md) — Setup -> Compile -> Execute lifecycle, fluent builder API, barrier synthesis, transient aliasing.
- **The Print Pass Order debug button** (`Render/RenderGraph/Print Pass Order` in the debug variables panel) — dumps the live topologically-sorted pass list for the current frame. Faster than tracing source code when you only need the answer to "does pass A run before pass B?".

Each subsystem's CLAUDE.md describes its passes' Reads/Writes — that's the input to the topological sort. There is no `RenderOrder` enum and no caller-supplied ordering token.

---

## 4. Safe First Areas

If you want a starter task to get familiar with the build, these areas are well-isolated and unlikely to break anything else:

- **Input/** — add a new key code or gamepad helper.
- **DebugVariables/** — register a new debug toggle for an existing subsystem (great for learning where things live).
- **DataStream/** — add a `<<` overload for a missing type.
- **Collections/** — add a unit test for `Zenith_Vector` or `Zenith_CircularQueue`.
- **Sokoban behaviours** — tweak gameplay; does not touch engine internals.
- **Profiling indices** — add a `ZENITH_PROFILE_INDEX__*` to wrap an unprofiled hotspot.

---

## 5. Known Sharp Edges

Things that have surprised people. If you hit one, this section saves you a debug session.

| Topic | What to know | Where it lives |
|---|---|---|
| Memory allocation | Never mix `Zenith_MemoryManagement::Allocate/Reallocate/Deallocate` with `new[]/delete[]`. They use different heaps. | [Zenith_MemoryManagement.h](../../Zenith/Core/Memory/Zenith_MemoryManagement.h) (header comment) |
| TaskSystem | It's a flat pool, not a dependency graph. To order task B after A, do `A.WaitUntilComplete()` before submitting B. | [Zenith_TaskSystem.h](../../Zenith/TaskSystem/Zenith_TaskSystem.h) (top comment) |
| Entity templates | `AddComponent<T>`, `GetComponent<T>` etc. are declared in `Zenith_Entity.h` but defined in `Zenith_Entity.inl`, included from `Zenith_Scene.h`. To use them you must include `Zenith_Scene.h`, not just `Zenith_Entity.h`. | [Zenith_Entity.h](../../Zenith/ZenithECS/Zenith_Entity.h) |
| SceneData ↔ SceneManager | Each header includes the other at the bottom. The cycle is intentional — don't "fix" it. | [Zenith_SceneData.h:842-855](../../Zenith/ZenithECS/Zenith_SceneData.h) |
| EntityID stability | `EntityID.m_uIndex` is process-wide stable across scene moves (the slot table is global). The 32-bit generation counter detects stale handles. | [ZenithECS/CLAUDE.md](../../Zenith/ZenithECS/CLAUDE.md) "Why entity slots are global" |
| Physics reset order | When a scene unloads, `SceneData::Reset()` must run before `Physics::Reset()`. ColliderComponent destructors call into the physics world. | [Physics/CLAUDE.md](../../Zenith/Physics/CLAUDE.md) |
| Static state across Play/Stop | If a game behaviour holds `static` state, you must reset it in `OnAwake` — Play→Stop→Play does NOT reset statics. | [Editor/CLAUDE.md](../../Zenith/Editor/CLAUDE.md) |
| Render graph order | Pass execution order is the *topological sort* of declared reads/writes, NOT the order of `AddPass()` calls. To inspect at runtime: trigger the `Flux/PrintPassOrder` debug button. | [Flux/CLAUDE.md](../../Zenith/Flux/CLAUDE.md) |
| Async asset loaders | Currently unimplemented for all asset types — calls log a warning and return `nullptr`. Use the synchronous `registry.Get<T>(path)` path. | [AssetHandling/CLAUDE.md](../../Zenith/AssetHandling/CLAUDE.md) |
| NavMesh winding order | Polygon winding direction is critical — wrong winding produces inverted normals and broken pathfinding. | `Zenith/AI/Navigation/Zenith_NavMeshGenerator.cpp` (per-phase comments) |
| Vulkan platform names | `Flux_CommandBuffer`, `Flux_PlatformAPI` etc. are `using` aliases for `Zenith_Vulkan_*` types in [Zenith_PlatformGraphics_Include.h](../../Zenith/Vulkan/Zenith_PlatformGraphics_Include.h). IDE jump-to-definition leads to the alias, not the implementation. |
| Prefab variants | A variant prefab inherits its base's components and applies its own property overrides on top. Cycles (A → B → A) are detected at variant creation. Nested property paths like `"Position.x"` are not yet supported (use whole-field overrides). | [Prefab/CLAUDE.md](../../Zenith/Prefab/CLAUDE.md) |
| Build configurations | Only `*_True` (tools) configs are known good. `*_False` builds are broken at the time of writing — see project memory. | `Build/Sharpmake_*.cs` |

---

## 6. Where Things Aren't What They Look Like

Common "huh?" moments and how to read them.

- **`Flux_CommandBuffer` has no class definition you can jump to.** It's a `using` alias declared in `Zenith/Vulkan/Zenith_PlatformGraphics_Include.h:22-33`, mapping to `Zenith_Vulkan_CommandBuffer`. Search by the underlying name.
- **`Zenith_SceneData` looks like the wrong place for ECS ownership** — it's the actual storage, exposed only to `Zenith_SceneManager`. Game code uses `Zenith_Scene` and `Zenith_Entity` handles.
- **`Zenith_SceneManager.h` is split across three sibling headers.** The top file (`Zenith_SceneManager.h`) holds only the public game-facing API. Engine internals live in `Zenith_SceneManagerInternal.h`; nested RAII guards in `Zenith_SceneManagerGuards.h`. Both are included from inside the class body, so call sites resolve unchanged. Read the public header first; reach for the internal one only when working on the engine itself.
- **There is no `std::function` anywhere.** Engine convention forbids it for performance. Callbacks are raw function pointers (e.g. `Zenith_TaskFunction`, `UIButtonCallback`).
- **`Zenith_Vector<T>` is not `std::vector<T>` and has no STL iterators.** Use index-based loops or its own `Iterator` class. Methods: `PushBack`, `GetSize`, `Get`, `Clear`, `Remove`, `EraseValue`, `RemoveSwap`.
- **`std::mutex` doesn't appear** — it's `Zenith_Mutex` (alias for `Zenith_Windows_Mutex`). Method names are `Lock`, `TryLock`, `Unlock`.
- **`#ifdef ZENITH_TOOLS` blocks vanish in non-tools builds.** Editor, automation, and many debug paths are tools-only. Don't be surprised when grep results disappear depending on configuration.
- **`Zenith.h` is the precompiled header.** Every `.cpp` must include it first. It defines `Zenith_Log`, `Zenith_Error`, `Zenith_Warning`, `Zenith_Assert`, type aliases (`u_int`, `u_int32`, `u_int64`), and the GUID system.
- **Two gizmo systems exist**: `Zenith_Gizmo` (ray utilities) in `Editor/`, and `Flux_Gizmos` (geometry + interaction) in `Flux/Gizmos/`. They're complementary — `Editor::HandleGizmoInteraction()` uses both.

---

## Updates

This map is intended to be edited as you discover things newcomers need to know. If you spent more than 10 minutes confused by something that should have been obvious, add it to **Known Sharp Edges** or **Where Things Aren't What They Look Like** — that's exactly what those sections are for.

For a record of past concerns surfaced by newcomer exploration and how each was resolved, see [JuniorExplorationFollowup.md](JuniorExplorationFollowup.md).
