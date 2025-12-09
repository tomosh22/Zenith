# Zenith Editor - Technical Documentation

## Overview

The Zenith Editor provides an in-engine scene authoring environment with entity manipulation, gizmo tools, and play/pause/stop functionality. This document covers critical implementation details, Vulkan integration, and common pitfalls.

## Architecture

```
Zenith_Editor (Main Controller)
    ├── Zenith_SelectionSystem (Entity Picking)
    ├── Zenith_Gizmo (Transform Manipulation)
    └── ImGui UI (Panels & Viewport)
```

### File Structure

- **Zenith_Editor.h/.cpp** - Main editor controller, UI panels, mode management
- **Zenith_SelectionSystem.h/.cpp** - Ray-AABB intersection, bounding box calculations
- **Zenith_Gizmo.h/.cpp** - Interactive gizmos for translation/rotation/scaling
- **EDITOR_IMPLEMENTATION_PLAN.md** - Detailed implementation guide (comprehensive)
- **CLAUDE.md** - This file - critical technical notes

## Critical: Vulkan Descriptor Set Management

### Problem 1: Descriptor Set Exhaustion

**Symptom:**
```
vkUpdateDescriptorSets(): pDescriptorWrites[0].dstSet is VK_NULL_HANDLE
```

**Root Cause:**
ImGui's Vulkan backend allocates descriptor sets when you call `ImGui_ImplVulkan_AddTexture()`. If called every frame without caching, the descriptor pool gets exhausted quickly.

**Solution:** Cache descriptor sets and only reallocate when the underlying resource changes.

### Problem 2: Freeing In-Use Descriptor Sets ⚠️ CRITICAL

**Symptom:**
```
vkFreeDescriptorSets(): pDescriptorSets[0] can't be called on VkDescriptorSet 0x...
that is currently in use by VkCommandBuffer 0x...
```

**Root Cause:**
Vulkan uses command buffer pipelining (typically 2-3 frames in flight). When you free a descriptor set, the GPU might still be executing commands that reference it from previous frames.

**Vulkan Synchronization Rule:**
> All submitted commands that refer to any element of pDescriptorSets must have completed execution
>
> — [Vulkan Spec §14.2.3](https://vulkan.lunarg.com/doc/view/1.4.313.1/windows/antora/spec/latest/chapters/descriptorsets.html#VUID-vkFreeDescriptorSets-pDescriptorSets-00309)

**Solution Implemented: Deferred Deletion Queue**

```cpp
// Queue old descriptor sets for deletion after N frames
struct PendingDescriptorSetDeletion
{
    VkDescriptorSet descriptorSet;
    u_int framesUntilDeletion;
};
static std::vector<PendingDescriptorSetDeletion> s_xPendingDeletions;

// When resource changes (e.g., window resize):
if (s_xCachedImageView != xGameRenderSRV.m_xImageView)
{
    if (s_xCachedGameTextureDescriptorSet != VK_NULL_HANDLE)
    {
        // Queue for deferred deletion - wait 3 frames before freeing
        constexpr u_int FRAMES_TO_WAIT = 3;
        s_xPendingDeletions.push_back({
            s_xCachedGameTextureDescriptorSet,
            FRAMES_TO_WAIT
        });
    }

    // Allocate new descriptor set
    s_xCachedGameTextureDescriptorSet = ImGui_ImplVulkan_AddTexture(...);
    s_xCachedImageView = xGameRenderSRV.m_xImageView;
}

// In Update() - process pending deletions each frame
for (auto it = s_xPendingDeletions.begin(); it != s_xPendingDeletions.end(); )
{
    if (it->framesUntilDeletion == 0)
    {
        // Safe to delete now - GPU has finished with this descriptor set
        ImGui_ImplVulkan_RemoveTexture(it->descriptorSet);
        it = s_xPendingDeletions.erase(it);
    }
    else
    {
        it->framesUntilDeletion--;
        ++it;
    }
}
```

**Why Wait 3 Frames?**
- Most engines use double or triple buffering (2-3 frames in flight)
- Waiting 3 frames ensures the GPU has finished with the descriptor set
- This is conservative but safe for all window resize scenarios

**Key Principles:**
1. **Never** free Vulkan resources immediately if they might be in use
2. **Always** defer deletion until GPU has finished with them
3. **Track** frame completion or wait N frames (simpler approach)
4. **At shutdown**, process all pending deletions (GPU will be idle anyway)

**Alternative Solutions:**
- **Fence-based**: Wait for specific frame fences before deletion (more complex, more precise)
- **Device idle**: Call `vkDeviceWaitIdle()` before deletion (causes frame hitch on resize)
- **Never free**: Let ImGui manage its pool (may leak descriptors but safest)

### Vulkan Image Layout Transitions

The game viewport texture must be in `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL` layout when passed to ImGui. The Flux renderer handles this transition automatically in `TransitionTargetsAfterRenderPass()`.

**If you see layout errors:**
- Verify `Flux_Graphics::s_xFinalRenderTarget` is properly transitioned
- Check that the render pass sequence completes before editor rendering
- Ensure no concurrent access to the render target

## Editor Modes & State Management

### Mode Transitions

```cpp
EditorMode::Stopped  ──Play──>  EditorMode::Playing
                    <──Stop───

EditorMode::Playing  ──Pause──>  EditorMode::Paused
                    <──Resume──
```

### Scene State Backup (TODO)

**Current Status:** Mode transitions work, but scene state is NOT preserved when stopping play mode.

**Future Implementation:**
The proper solution requires one of:
1. **Scene::Clone()** - Deep copy all entities and components
2. **Serialize to memory** - Most robust, uses existing serialization
3. **Copy-on-write** - Track modified entities only

**Critical Consideration:**
Entity pointers (like `s_pxSelectedEntity`) become invalid when the scene is reset. Always clear selection when stopping play mode.

## Entity Selection System

### Raycasting Pipeline

```
Mouse Click
    ↓
Screen Position → Viewport Position
    ↓
ScreenToWorldRay() → (Origin, Direction)
    ↓
UpdateBoundingBoxes() → AABB for all entities
    ↓
Ray-AABB Intersection Test
    ↓
Select Closest Hit Entity
```

### Bounding Box Calculation

**Location:** `Zenith_SelectionSystem::CalculateBoundingBox()`

**Algorithm:**
1. Iterate through all vertices in entity's model
2. Find min/max in model space
3. Transform AABB corners to world space
4. Recompute axis-aligned bounds (may expand due to rotation)

**Performance:**
- O(n * m) where n = entities, m = average vertices
- Called every frame in `UpdateBoundingBoxes()`
- **Optimization opportunity:** Cache model-space bounds, only transform when entity moves

### Important: Entity Pointer Lifetime

**Problem:**
```cpp
Zenith_Entity xEntity = scene.GetEntityByID(id);  // Returns by value
SelectEntity(&xEntity);  // DANGER: Pointer to local variable!
```

**Current Solution:**
```cpp
// Hierarchy panel iterates the entity map directly
for (auto& [entityID, entity] : xScene.m_xEntityMap)
{
    SelectEntity(&entity);  // Pointer to map element (valid until map is modified)
}
```

**Warning:** This pointer becomes invalid if:
- Entity is deleted from the map
- Map is resized/rehashed
- Scene is reset

**Future Improvement:**
Store `Zenith_EntityID` instead of `Zenith_Entity*` for robust selection.

## Gizmo System

**⚠️ CRITICAL: Full gizmo documentation is in [Flux/Gizmos/CLAUDE.md](../Flux/Gizmos/CLAUDE.md)**

The gizmo system is implemented in `Zenith/Flux/Gizmos/` as true 3D geometry rendered via the Flux pipeline. The Editor provides the integration layer between user input and the Flux_Gizmos system.

### Architecture Overview

```
Zenith_Editor.cpp (Integration Layer)
    │
    ├── Entity Selection (via Zenith_SelectionSystem)
    │   └── Sets Flux_Gizmos::SetTargetEntity()
    │
    ├── Gizmo Mode (W/E/R keys → Translate/Rotate/Scale)
    │   └── Sets Flux_Gizmos::SetGizmoMode()
    │
    ├── Mouse Input Forwarding
    │   ├── MouseDown → Flux_Gizmos::BeginInteraction()
    │   ├── MouseMove → Flux_Gizmos::UpdateInteraction()
    │   └── MouseUp   → Flux_Gizmos::EndInteraction()
    │
    └── Ray Generation
        └── Zenith_Gizmo::ScreenToWorldRay()
```

### Coordinate Systems

The gizmo system deals with multiple coordinate spaces:

1. **Screen Space** - Mouse position from input system (pixels)
2. **Viewport Space** - Relative to ImGui viewport window
3. **Clip Space** - After projection matrix (NDC)
4. **View Space** - After view matrix (camera space)
5. **World Space** - Scene coordinates

### Screen-to-World Ray Conversion

**Location:** `Zenith_Gizmo::ScreenToWorldRay()`

**Algorithm:**
```cpp
// 1. Normalize to NDC [-1, 1]
float x = (mousePos.x / viewportSize.x) * 2.0f - 1.0f;
float y = (mousePos.y / viewportSize.y) * 2.0f - 1.0f;
// NOTE: No Y-inversion! The projection matrix already handles coordinate system

// 2. Clip space
Vector4 rayClip(x, y, -1.0, 1.0);

// 3. View space
Vector4 rayEye = inverse(projMatrix) * rayClip;
rayEye = Vector4(rayEye.x, rayEye.y, -1.0, 0.0);  // Direction vector

// 4. World space
Vector4 rayWorld = inverse(viewMatrix) * rayEye;
Vector3 rayDir = normalize(rayWorld.xyz);
```

**⚠️ BUG HISTORY - Y-Axis Inversion:**
There was originally a `y = -y` line here that caused inverted Y-axis interaction (clicking top hit bottom).
The projection matrix already handles the Vulkan coordinate system convention, so NO manual Y-inversion is needed.
See [Flux/Gizmos/CLAUDE.md - Bug History](../Flux/Gizmos/CLAUDE.md#critical-bug-history) for details.

### Editor Integration with Flux_Gizmos

**Location:** `Zenith_Editor.cpp` - `RenderGizmos()` function

**CRITICAL: State Update Guard**

The editor must NOT update gizmo state while an interaction is in progress:

```cpp
void Zenith_Editor::RenderGizmos()
{
    // CRITICAL: Only update target/mode when NOT interacting!
    // Otherwise we reset s_bIsInteracting = false every frame
    if (!Flux_Gizmos::IsInteracting())
    {
        Flux_Gizmos::SetTargetEntity(pxSelectedEntity);
        Flux_Gizmos::SetGizmoMode(static_cast<GizmoMode>(s_eGizmoMode));
    }

    // ... mouse input handling ...
}
```

**Why This Matters:**
`SetTargetEntity()` resets `s_bIsInteracting = false` to clear stale interaction state.
If called every frame during dragging, it terminates the drag after ~1 frame.

### Mouse Input Flow

```cpp
// In RenderGizmos():

// 1. Generate ray from mouse position
Zenith_Maths::Vector3 rayOrigin, rayDir;
Zenith_Gizmo::ScreenToWorldRay(mousePos, viewportSize, rayOrigin, rayDir);

// 2. Handle mouse events
if (mousePressed && !wasInteracting)
{
    Flux_Gizmos::BeginInteraction(rayOrigin, rayDir);
}
else if (mouseDown && Flux_Gizmos::IsInteracting())
{
    Flux_Gizmos::UpdateInteraction(rayOrigin, rayDir);
}
else if (mouseReleased)
{
    Flux_Gizmos::EndInteraction();
}
```

### Gizmo Rendering

**Method:** True 3D geometry rendered via Flux pipeline

The gizmos are rendered as actual 3D geometry (cylinders, cones, circles, cubes) using:
- Custom Flux shader (`Gizmo.shader`)
- Per-axis vertex/index buffers
- Auto-scaling based on camera distance

**Render Pipeline Position:**
Gizmos render AFTER the main scene in a forward pass, writing to the final render target.
They use depth testing against the scene but write their own depth values.

### Constant Screen-Space Size

```cpp
// In Flux_Gizmos.cpp
float distanceToCamera = Zenith_Maths::Length(xGizmoPos - cameraPos);
s_fGizmoScale = distanceToCamera * GIZMO_SCALE_FACTOR;  // 0.15f
```

This ensures the gizmo appears the same size on screen regardless of camera distance.

## ImGui Integration

### Docking System

The editor uses ImGui's docking feature to create a flexible multi-panel layout:

```cpp
ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;
// ... (make it fullscreen and transparent)

ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
```

### Memory Management

ImGui uses custom allocators. When including ImGui headers in Zenith:

```cpp
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
```

This prevents conflicts between Zenith's custom allocator and ImGui's allocations.

### Input Handling

ImGui automatically consumes input events. The engine's input system (`Zenith_Input`) only receives events that ImGui doesn't consume.

**Example:**
- Clicking on a UI button → ImGui consumes, `Zenith_Input::WasKeyPressedThisFrame()` returns false
- Clicking in viewport → ImGui doesn't consume, input system receives event

## Performance Considerations

### UpdateBoundingBoxes()

**Current:** Called every frame, O(n * m) complexity

**Optimizations to consider:**
1. **Dirty flagging** - Only update entities whose transforms changed
2. **Spatial partitioning** - Use octree/BVH for large scenes
3. **Model-space caching** - Store untransformed AABB, apply transform matrix only
4. **Async update** - Run on worker threads

### Selection Raycasting

**Current:** Linear search through all entities

**Optimizations:**
1. Use spatial data structure (octree) for O(log n) queries
2. Frustum culling to exclude off-screen entities
3. LOD-based bounding volumes (coarse test first)

## Friend Class Pattern

The editor needs access to private scene data. Instead of making everything public:

```cpp
// In Zenith_Scene.h
#ifdef ZENITH_TOOLS
    friend class Zenith_Editor;
    friend class Zenith_SelectionSystem;
#endif
```

**Benefits:**
- Clean public API for game code
- Editor has privileged access only in tools builds
- Compile-time guarded (no editor overhead in release)

## Known Limitations & Future Work

### Current Limitations

1. **Scene state not preserved** in play mode (see "Scene State Backup" above)
2. **Entity pointers can become invalid** - Should use EntityID
3. **No undo/redo** - Requires command pattern implementation
4. **No multi-selection** - Only one entity at a time

### Recently Fixed (2025)

- ✅ **Axis hit testing** - Now uses proper ray-cylinder intersection in world space
- ✅ **3D gizmo rendering** - True 3D geometry via Flux pipeline with depth testing
- ✅ **Rotate/scale gizmo geometry** - Generated and renderable (interaction WIP)
- ✅ **Translation gizmo** - Fully functional with line-line closest point math

### Roadmap

**Short Term:**
- [ ] Implement rotate gizmo interaction
- [ ] Implement scale gizmo interaction
- [ ] Store selection by EntityID instead of pointer
- [ ] Add keyboard shortcuts (Q/W/E/R for gizmo modes)

**Medium Term:**
- [ ] Scene::Clone() for proper play mode state preservation
- [ ] Component serialization system
- [ ] Undo/redo command stack
- [ ] Multi-entity selection with bounding box visualization

**Long Term:**
- [ ] Prefab system
- [ ] Visual scripting integration
- [ ] Asset browser panel

## Debugging Tips

### Vulkan Validation Layers

Always run with Vulkan validation layers enabled during development:

```cpp
// Check for descriptor set issues
vkUpdateDescriptorSets(): pDescriptorWrites[0].dstSet is VK_NULL_HANDLE
→ Check descriptor set caching in RenderViewport()

// Check for layout issues
vkCmdDraw(): Image layout mismatch
→ Verify render target is in SHADER_READ_ONLY_OPTIMAL
```

### Common Issues

**Gizmo not appearing:**
1. Check entity has TransformComponent
2. Verify camera matrices are valid
3. Check gizmo is not behind camera (negative Z)
4. Ensure viewport is focused/hovered
5. Verify `Flux_Gizmos::Initialise()` was called at startup

**Gizmo not responding to clicks:**
1. Check that `SetTargetEntity()` is NOT called during interaction (see "State Update Guard" above)
2. Verify ray origin and direction are in world space
3. Check `GIZMO_INTERACTION_LENGTH_MULTIPLIER` is 1.0 (not larger)
4. Ensure gizmo scale (`s_fGizmoScale`) is reasonable for camera distance

**Gizmo clicks register in wrong position:**
1. Check `ScreenToWorldRay()` for Y-axis issues (should NOT have `y = -y`)
2. Verify viewport position offset is subtracted from mouse position
3. Check camera view/projection matrices are correct
4. See [Flux/Gizmos/CLAUDE.md - Bug History](../Flux/Gizmos/CLAUDE.md#critical-bug-history)

**Selection not working:**
1. Verify UpdateBoundingBoxes() is called
2. Check viewport bounds are correct
3. Ensure entity has ModelComponent (for bounding box)
4. Verify ray direction is correct (watch for coordinate system issues)

**Crash on entity selection:**
1. Entity pointer may be invalid
2. Check if entity was deleted
3. Verify selection is cleared when scene resets

### Profiling

The editor adds overhead. Profile with:
```cpp
ZENITH_PROFILING_FUNCTION_WRAPPER(
    UpdateBoundingBoxes,
    ZENITH_PROFILE_INDEX__EDITOR,
    args...
);
```

Expected costs:
- UpdateBoundingBoxes: ~0.5ms for 1000 entities
- Raycasting: ~0.1ms
- ImGui rendering: ~2-5ms
- Gizmo rendering: <0.1ms

## Best Practices

### When Adding New Editor Features

1. **Document Vulkan usage** - Especially descriptor sets, image layouts
2. **Cache GPU resources** - Never allocate per-frame
3. **Handle entity lifetime** - Prefer EntityID over pointers
4. **Profile impact** - Editor should add <10ms overhead
5. **Guard with ZENITH_TOOLS** - No editor code in release builds

### When Modifying Core Systems

If you change the core engine and break the editor:

1. Check if `friend class` declarations need updating
2. Verify component interfaces are intact
3. Update serialization if adding new component data
4. Test play/pause/stop transitions

## References

- **Flux Gizmos Documentation** - See [Flux/Gizmos/CLAUDE.md](../Flux/Gizmos/CLAUDE.md) - **CRITICAL: Read before modifying gizmo code**
- **EDITOR_IMPLEMENTATION_PLAN.md** - Comprehensive implementation guide with detailed algorithms
- **ImGui Documentation** - https://github.com/ocornut/imgui
- **Vulkan Spec** - https://www.khronos.org/vulkan/
- **Zenith ECS** - See `Zenith/EntityComponent/CLAUDE.md`

---

**Last Updated:** 2025-12
**Author:** Claude (Anthropic)
**Status:** Editor functional, translation gizmo working, scene state preservation pending
