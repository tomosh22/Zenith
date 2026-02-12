# Profiling System

## Overview

Per-thread CPU profiling with hierarchical timing regions. Uses `std::chrono::high_resolution_clock` for accurate timing. Provides ImGui visualization in tools builds.

## Files

- `Zenith_Profiling.h` - Profiling API, scope helper, profile index enum

## Public API

| Function | Description |
|----------|-------------|
| `Initialise()` | Initialize profiling system |
| `RegisterThread()` | Register current thread for profiling |
| `BeginFrame()` / `EndFrame()` | Frame boundaries |
| `BeginProfile(index)` / `EndProfile(index)` | Manual region start/stop |
| `GetCurrentIndex()` | Get active profile section |
| `GetEvents()` | Get per-thread event map |

### RAII Scope Helper

`Zenith_Profiling::Scope` provides automatic begin/end:
```cpp
{
    Zenith_Profiling::Scope xScope(ZENITH_PROFILE_INDEX__PHYSICS);
    // Physics work...
} // EndProfile called automatically
```

### Function Wrapper Macro

`ZENITH_PROFILING_FUNCTION_WRAPPER(func, profile_index, ...)` wraps a function call with begin/end profiling.

## Profile Categories

~69 categories covering all engine systems:

| Area | Examples |
|------|---------|
| Frame | `TOTAL_FRAME` |
| Tasks | `WAIT_FOR_TASK_SYSTEM`, `WAIT_FOR_MUTEX` |
| Game | `ANIMATION`, `SCENE_UPDATE`, `PHYSICS`, `VISIBILITY_CHECK` |
| Shadows | `FLUX_SHADOWS`, `FLUX_SHADOWS_UPDATE_MATRICES` |
| Rendering | `FLUX_DEFERRED_SHADING`, `FLUX_DYNAMIC_LIGHTS`, `FLUX_SKYBOX` |
| Meshes | `FLUX_STATIC_MESHES`, `FLUX_ANIMATED_MESHES`, `FLUX_INSTANCED_MESHES` |
| Terrain | `FLUX_TERRAIN`, `FLUX_TERRAIN_CULLING`, `FLUX_TERRAIN_STREAMING` + sub-categories |
| Effects | `FLUX_GRASS`, `FLUX_PFX`, `FLUX_WATER`, `FLUX_FOG` |
| Post-Process | `FLUX_SSAO`, `FLUX_HIZ`, `FLUX_SSR`, `FLUX_SSGI`, `FLUX_HDR` |
| Vulkan | `VULKAN_UPDATE_DESCRIPTOR_SETS`, `VULKAN_MEMORY_MANAGER_UPLOAD`, `VULKAN_WAIT_FOR_GPU` |
| AI | `AI_PERCEPTION_UPDATE`, `AI_SQUAD_UPDATE`, `AI_PATHFINDING`, `AI_NAVMESH_GENERATE` |
| Assets | `ASSET_LOAD`, `FLUX_MESH_GEOMETRY_LOAD_FROM_FILE` |
| Tools | `RENDER_IMGUI`, `RENDER_IMGUI_PROFILING` |

## Tools Visualization (ZENITH_TOOLS only)

- `RenderToImGui()` - Main profiling display
- `RenderTimelineView(...)` - Timeline with zoom/scroll/depth filtering
- `RenderThreadBreakdown(...)` - Per-thread breakdown

## Key Patterns

- Per-thread event collection via thread-local storage
- Push/pop nesting with depth tracking
- Events stored in `Zenith_Vector<Event>` keyed by thread ID
- String name array `g_aszProfileNames[]` validates against enum count via `static_assert`
