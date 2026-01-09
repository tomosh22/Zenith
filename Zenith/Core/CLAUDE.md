# Core

Engine core utilities, configuration, and main loop.

## Files

### Root
- `Zenith.h/cpp` - Master header, type definitions, logging, assertions, GUID system
- `Zenith_Core.h/cpp` - Main loop, frame timing
- `ZenithConfig.h` - Central configuration constants
- `Zenith_String.h` - String utilities

### Subdirectories
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

Provides `GetDt()` for delta time and `GetTimePassed()` for accumulated time. Uses high-resolution clock for timing.

## ZenithConfig.h

Central configuration avoiding magic numbers scattered in code.

### Key Constants
- `MAX_FRAMES_IN_FLIGHT` - Frame pipelining (typically 2 for double buffering)
- `FLUX_NUM_WORKER_THREADS` - Render command buffer recording threads (8)
- Asset limits: `ZENITH_MAX_TEXTURES`, `ZENITH_MAX_MESHES`, `ZENITH_MAX_MATERIALS`
- Vulkan limits: `FLUX_MAX_TARGETS`, `FLUX_MAX_DESCRIPTOR_BINDINGS`
- Vertex format: `STATIC_MESH_VERTEX_STRIDE` (60 bytes)

## Memory Management

Location: `Memory/Zenith_MemoryManagement.h/cpp`

Memory allocation abstraction layer. Overloads global `new`/`delete` operators. Currently delegates to standard `malloc`/`realloc`/`free` but provides hook point for custom allocators. File/line tracking is stubbed but not yet implemented.

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
