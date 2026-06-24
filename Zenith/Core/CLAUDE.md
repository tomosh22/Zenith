# Core

Engine core utilities, configuration, and main loop.

## Files

### Root
- `Zenith.h/cpp` - Master header, type definitions, logging, assertions, GUID system
- `Zenith_Core.h/cpp` - Main loop, frame timing
- `Zenith_Engine.h/cpp` - Process-level engine owner (`g_xEngine`) — holds every mutable subsystem
- `FrameContext.h` - Per-frame timing state owned by the engine
- `ZenithConfig.h` - Central configuration constants
- `Zenith_String.h` - String utilities
- `Zenith_CommandLine.h/cpp` - Parsed once at process start, read-only after
- `Zenith_GraphicsOptions.h/cpp` - Set once by `Project_SetGraphicsOptions`, read-only after

### Subdirectories
- `Callstack/` - Callstack capture and symbolication (`Zenith_Callstack`) used by memory tracking and editor panels
- `Memory/` - Memory management system
- `Multithreading/` - Threading utilities

## Zenith.h (Master Header)

Precompiled header included by all .cpp files. Provides:
- Type aliases: `u_int`, `u_int8/16/32/64`
- Logging macros: `Zenith_Log(eCategory, ...)`, `Zenith_Error(eCategory, ...)`, `Zenith_Warning(eCategory, ...)` - require `Zenith_LogCategory` enum as first parameter
- Debug assertion: `Zenith_Assert(condition, ...)` - breaks in debug builds, supports variadic message arguments
- GUID system: 64-bit unique identifiers, hashable for use in containers

## Zenith_Core

Main loop controller and timing manager. Executes frame sequence:
1. Begin frame (platform setup, input polling, acquire swapchain)
2. Editor update (deferred scene loads)
3. Game logic (physics, scene updates)
4. Render task submission
5. Synchronization
6. End frame (present)

Per-frame timing state (`GetDt()`, `GetTimePassed()`) was moved onto `FrameContext` (Phase 2 refactor) and is read via `g_xEngine.Frame().GetDt()` / `g_xEngine.Frame().GetTimePassed()`. `Zenith_Core` itself exports the main loop (`Zenith_MainLoop`) and the per-frame timer tick (`UpdateTimers`), which writes the new frame's dt / accumulated time into `g_xEngine.Frame()` using a high-resolution clock.

## ZenithConfig.h

Central configuration avoiding magic numbers scattered in code.

### Key Constants
- `MAX_FRAMES_IN_FLIGHT` - Frame pipelining (typically 2 for double buffering)
- `FLUX_NUM_WORKER_THREADS` - Render command buffer recording threads (8)
- Asset limits: `ZENITH_MAX_TEXTURES`, `ZENITH_MAX_MESHES`, `ZENITH_MAX_MATERIALS`
- Vulkan limits: `FLUX_MAX_TARGETS`, `FLUX_MAX_BINDINGS_PER_GROUP`

## Memory Management

Location: `Memory/Zenith_MemoryManagement.h/cpp`

Memory allocation abstraction layer. Overloads global `new`/`delete` operators. Currently delegates to standard `malloc`/`realloc`/`free` but provides hook point for custom allocators. File/line tracking is stubbed but not yet implemented.

### Allocation Consistency (CRITICAL)

**Never mix allocation strategies:**
- `Allocate()`/`Reallocate()`/`Deallocate()` use `malloc`/`realloc`/`free`
- `new[]`/`delete[]` may use a different heap

```cpp
// WRONG: Heap corruption!
data = new T[count];
data = Reallocate(data, newSize);  // realloc() on new[]-allocated memory = UB

// CORRECT: Use consistent allocation
data = static_cast<T*>(Allocate(count * sizeof(T)));
data = static_cast<T*>(Reallocate(data, newSize));  // Works correctly
Deallocate(data);
```

For classes that may have buffers reallocated (e.g., `Flux_MeshGeometry` used by terrain streaming), always use `Allocate()`/`Deallocate()` for buffer allocation.

## Multithreading

Location: `Multithreading/Zenith_Multithreading.h/cpp`

Cross-platform threading utilities:
- Thread creation with naming
- Thread registration with engine
- Thread ID querying
- Main thread identification

Provides `Zenith_ScopedMutexLock` RAII wrapper for locking.

## Key Concepts

**Precompiled Header:** All .cpp files must include "Zenith.h" first for consistent compilation environment.

**Configuration Centralization:** All engine-wide constants in ZenithConfig.h for easy tuning and preventing duplication.

**Tools-Only Code:** Editor features wrapped in `#ifdef ZENITH_TOOLS` to exclude from shipping builds.

**Type Safety:** Sized integer types (`u_int32`, `u_int64`) with static assertions prevent platform-specific bugs.

## Design Rationale

### Why Zenith_Core Is a Namespace, Not a Class

`Zenith_Core` was converted from a static-only class to a namespace. This is idiomatic C++:

1. **Classes Should Have Instance State**: A class with only static members is semantically a namespace disguised as a class. In C++, namespaces exist specifically for grouping related functions and data.

2. **No Instantiation Concerns**: A static-only class can technically be instantiated (without effect). A namespace cannot be instantiated, which better reflects intent.

3. **Same Usage Syntax**: The conversion is transparent to callers - `Zenith_Core::GetDt()` works identically whether `Zenith_Core` is a class or namespace.

4. **Variable Naming**: Changed from `s_` (static member) prefix to `g_` (global) prefix following the codebase naming conventions, since namespace-scope variables are essentially globals.

## Zenith_Engine — Process-Level Engine Owner

`Zenith_Engine` is the single owner of every mutable engine subsystem. There is exactly one instance: the global `g_xEngine`, defined in `Zenith_Engine.cpp`. All subsystem state that used to live in module-scope `g_*` / `s_*` variables now lives behind `g_xEngine.X()` accessors.

```cpp
// Reading subsystem state — always via g_xEngine
float fDt = g_xEngine.Frame().GetDt();
g_xEngine.Physics().Update(fDt);
g_xEngine.Tasks().SubmitTask(&xTask);
g_xEngine.SSR().ApplyBlurSelectionToGraph(xGraph);
```

### constinit-safe Global

`g_xEngine` is `constinit`-eligible: its default constructor and destructor are trivial. All subsystem state is held as **raw pointers to forward-declared `*Impl` classes**, allocated explicitly in `Zenith_Engine::Initialise()` and deleted in `Zenith_Engine::Shutdown()`. This means:

- No work happens at static-init time — no chance of the static-init-order-fiasco that the old `g_*` module variables were vulnerable to.
- The engine header (`Zenith_Engine.h`) forward-declares every subsystem class; full headers stay out of the engine header to keep its include footprint small. Accessor bodies (`Zenith_TaskSystem& Tasks() { return *m_pxTasks; }`) live in `Zenith_Engine.cpp` where the full subsystem headers are available.
- Subsystem boot/teardown order lives in `Zenith_Engine::Initialise/Shutdown`, replacing the old `Zenith_Init` / `Zenith_Shutdown` free functions. The order matters — load-bearing comments in those functions encode dependency rules.

### Subsystem Class Convention

Each mutable subsystem now has an `*Impl` class:

- `Zenith_Physics`, `Zenith_TaskSystem`, `Zenith_AssetRegistry`, `Flux_RendererImpl`, etc. (Phase 1+ renames are dropping the `Impl` suffix incrementally.)
- Public surface uses the historical name (`Zenith_Physics`, `Zenith_Profiling`, ...) as either a thin namespace of free functions that forward to `g_xEngine.X()`, or — for trivially-replaceable static facades — was deleted entirely.

For most subsystems, access state directly via `g_xEngine.X().Y()`; the transitional free-function forwarders were swept to zero.

**Deliberate exception — `Zenith_AssetRegistry`.** Its static API (`Zenith_AssetRegistry::Get<T>(path)`, `Create<T>()`, `Save`, `UnloadUnused`, ...) is the *canonical* access path, not a forwarder slated for removal. The engine owns the single instance (`g_xEngine.Assets()`, published via `s_pxInstance` in `InitialiseAssets`); the static methods delegate to it. Call sites use the static form throughout — there is no planned migration to `g_xEngine.Assets().X()`.

### Macro-Driven APIs

Some APIs were kept as namespaces (not classes) because they're surfaced through macros that other TUs include heavily — `ZENITH_PROFILING_FUNCTION_WRAPPER`, `Zenith_DebugVariables::Add*`. The namespace form lets the macro body expand without requiring callers to drag in subsystem implementation details. The state still lives on `g_xEngine`; only the dispatch surface differs.

## What Stays Static (and Why)

Not everything moves onto the engine. These carve-outs are deliberate and the asymmetry is intentional — adding new statics is a code-review red flag *unless* it fits one of these categories:

### Process-level I/O and OS resources
- **Logging** (`Zenith_Log`, `Zenith_Error`, `Zenith_Warning`, `Zenith_Assert`) — stdout/stderr is a process-level sink. There's nothing per-engine to track.
- **Memory allocator** (`Zenith_MemoryManagement`) — overloads global `new`/`delete`. Has to be process-level by C++ language semantics.
- **`Zenith_Window::GetInstance()`** — wraps the OS window, which is itself a process-level resource on every platform we target.

### Per-thread state (not per-engine)
- **`Zenith_Multithreading` thread-locals** — thread name + thread id are TLS, by definition not per-engine. The thread-registry (mapping thread ids to friendly names) *did* move onto the engine; only the `thread_local` storage stays in module scope.

### Pure / stateless
- **Math + utility namespaces** — `Zenith_String`, GLM wrappers, `Collections/`, `Maths/`. Stateless functions only; nothing to own.
- **Compile-time constants** — `ZenithConfig.h`. Read-only.

### Read-only after boot
- **`Zenith_CommandLine`** — parsed once during process startup. Read-only thereafter.
- **`Zenith_GraphicsOptions`** — populated once by `Project_SetGraphicsOptions` at boot. Read-only thereafter.

### Static-init registration side-lists
- **`ZENITH_REGISTER_COMPONENT` macro** — populates a process-level side-list at static-init time. `Zenith_Engine::Initialise()` reads from this list during construction; moving it onto the engine would create a chicken-and-egg ordering problem with the macro.
- **`Zenith_ComponentMetaRegistry::Get()`** — populated by `ZENITH_REGISTER_COMPONENT`. Belongs alongside the registration macros (same lifetime, same hazard if moved).
- **`Zenith_EventDispatcher::Get()`** — process-level event bus populated by static-init registrations. Same rationale.

### Test / automation helpers
- **`Zenith_InputSimulator`** — test/automation helper. Lives outside the engine's normal-runtime surface so it can be poked at from automated-test driver code without an engine reference.

### The documented singleton
- **`Zenith_Engine g_xEngine` itself** — the one and only intentional process-level singleton. Everything else hangs off it.

**Practical rule of thumb:** if you find yourself reaching for `static` for new mutable state, check this list. If your case isn't on it, the answer is probably "put it on an `*Impl` class held by `Zenith_Engine`" — see how any of the existing subsystems are wired up for the pattern.
