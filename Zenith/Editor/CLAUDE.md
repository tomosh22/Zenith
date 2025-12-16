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

### Scene State Backup (IMPLEMENTED)

**Current Status:** Play/Stop mode transitions now correctly preserve and restore scene state.

**Implementation:**
- **On Play:** Scene is serialized to a temporary backup file (`editor_backup.zscen`)
- **On Stop:** Scene is restored from the backup file, deferred to next frame's `Update()`
- **Uses existing serialization system** - `Zenith_Scene::SaveToFile()` and `LoadFromFile()`

**Critical: Play/Stop Bug History (December 2024):**

When the user clicks Play a second time (after Stop), two critical bugs were fixed:

#### Bug #1: ScriptComponent Not Serialized

**Symptom:** Camera doesn't respond to controls on second play; player controller doesn't work

**Root Cause:** `ScriptComponent` was NOT in the list of serialized components in `Zenith_Entity.cpp`. When the scene was backed up, the `PlayerController_Behaviour` script was NOT saved. When restored, the script was gone.

**Fix Applied:**
1. Added `Zenith_BehaviourRegistry` - Factory pattern for recreating behaviors by type name
2. Added `ZENITH_BEHAVIOUR_TYPE_NAME(TypeName)` macro - Provides type name and factory registration
3. Added `WriteToDataStream`/`ReadFromDataStream` to `Zenith_ScriptComponent`
4. Added ScriptComponent to serialization in `Zenith_Entity.cpp` and `Zenith_Scene.cpp`
5. Behaviors register with `Zenith_BehaviourRegistry::Get().RegisterBehaviour()` at startup

**Files Modified:**
- [Zenith_ScriptComponent.h](../EntityComponent/Components/Zenith_ScriptComponent.h) - Added `Zenith_BehaviourRegistry`, `ZENITH_BEHAVIOUR_TYPE_NAME` macro, serialization methods
- [Zenith_ScriptComponent.cpp](../EntityComponent/Components/Zenith_ScriptComponent.cpp) - Implemented `WriteToDataStream`/`ReadFromDataStream`
- [Zenith_Entity.cpp](../EntityComponent/Zenith_Entity.cpp) - Added ScriptComponent to serialization
- [Zenith_Scene.cpp](../EntityComponent/Zenith_Scene.cpp) - Added ScriptComponent to deserialization
- [PlayerController_Behaviour.h](../../Games/Test/Components/PlayerController_Behaviour.h) - Added `ZENITH_BEHAVIOUR_TYPE_NAME(PlayerController_Behaviour)`
- [SphereMovement_Behaviour.h](../../Games/Test/Components/SphereMovement_Behaviour.h) - Added macro to `HookesLaw_Behaviour`, `RotationBehaviour_Behaviour`
- [Test_State_InGame.cpp](../../Games/Test/Test_State_InGame.cpp) - Added behavior registration at startup

**How Behavior Serialization Works:**
```cpp
// 1. Behaviors declare their type name
class PlayerController_Behaviour : public Zenith_ScriptBehaviour {
    ZENITH_BEHAVIOUR_TYPE_NAME(PlayerController_Behaviour)  // Adds GetBehaviourTypeName() + factory
    // ...
};

// 2. Behaviors register at startup
void Test_State_InGame::OnEnter() {
    PlayerController_Behaviour::RegisterBehaviour();  // Adds to factory registry
    // ...
}

// 3. Serialization saves type name
void Zenith_ScriptComponent::WriteToDataStream(Zenith_DataStream& xStream) const {
    xStream << bHasBehaviour;
    xStream << m_pxScriptBehaviour->GetBehaviourTypeName();  // "PlayerController_Behaviour"
}

// 4. Deserialization recreates behavior via factory
void Zenith_ScriptComponent::ReadFromDataStream(Zenith_DataStream& xStream) {
    xStream >> strTypeName;
    m_pxScriptBehaviour = Zenith_BehaviourRegistry::Get().CreateBehaviour(strTypeName.c_str(), m_xParentEntity);
    m_pxScriptBehaviour->OnCreate();
}
```

#### Bug #2: TerrainComponent Physics Mesh Incomplete

**Symptom:** Player falls through terrain on second play; terrain has no physics collision

**Root Cause:** In `Zenith_TerrainComponent::ReadFromDataStream()`, only ONE physics chunk was loaded (`Physics_0_0.zmsh`), but the constructor combines ALL 4096 physics chunks (64×64 grid). The physics collider only covered a tiny portion of the terrain.

**Fix Applied:** `ReadFromDataStream()` now loads and combines ALL physics chunks, identical to the constructor logic.

**Files Modified:**
- [Zenith_TerrainComponent.cpp](../EntityComponent/Components/Zenith_TerrainComponent.cpp) - `ReadFromDataStream()` now loads all 4096 physics chunks

#### Bug #3: Blank Textures After Scene Reload (December 2024)

**Symptom:** After pressing Stop in the editor, terrain and models render with blank white textures instead of their proper materials.

**Root Cause:** Materials stored only GPU texture handles (`Flux_VRAMHandle`), not the original file paths. When a scene was serialized and reloaded:
1. Material base color was saved/loaded correctly
2. But texture GPU handles became invalid (old resources destroyed during scene reset)
3. No paths were stored, so textures couldn't be reloaded
4. `GetDiffuse()` returned `GetBlankTexture()` when the handle was invalid

**Fix Applied (Multi-Part):**

**Part 1: Path Storage in Flux_Material**
- Added `m_strDiffusePath`, `m_strNormalPath`, etc. to `Flux_Material`
- Added `SetDiffuseWithPath()`, `SetNormalWithPath()`, etc. that store both texture AND path
- Added `WriteToDataStream()` / `ReadFromDataStream()` with version 2 format
- Added `ReloadTexturesFromPaths()` called during deserialization

**Part 2: Component Serialization Updates**
- `Zenith_TerrainComponent::WriteToDataStream()` now serializes full materials (v2)
- `Zenith_TerrainComponent::ReadFromDataStream()` creates materials and calls `ReadFromDataStream` on them
- `Zenith_ModelComponent` updated similarly

**Part 3: Game Code Updates**
- `Test_State_InGame.cpp` updated to use `SetDiffuseWithPath()` instead of `SetDiffuse()`
- Path passed to `SetDiffuseWithPath()` MUST match the file loaded (e.g., `ASSETS_ROOT"Textures/rock2k/diffuse.ztx"`)

**Files Modified:**
- [Flux_Material.h](../Flux/Flux_Material.h) - Added path storage, `SetWithPath` methods, serialization
- [Flux_Material.cpp](../Flux/Flux_Material.cpp) - Implemented serialization, `ReloadTexturesFromPaths()`
- [Zenith_TerrainComponent.cpp](../EntityComponent/Components/Zenith_TerrainComponent.cpp) - v2 serialization
- [Zenith_ModelComponent.cpp](../EntityComponent/Components/Zenith_ModelComponent.cpp) - v2 serialization
- [Test_State_InGame.cpp](../../Games/Test/Test_State_InGame.cpp) - Use `SetWithPath` methods

**⚠️ LESSON LEARNED:** For any asset that needs to survive serialization, ALWAYS store the source path alongside the GPU handle. GPU handles are transient; paths are persistent.

#### Bug #4: Texture Pool Exhaustion on Repeated Scene Reloads

**Symptom:** After pressing Stop multiple times, assert fires: "Run out of texture slots"

**Root Cause:** Textures created via `ReloadTexturesFromPaths()` during deserialization were never cleaned up. Each scene reload created NEW textures without deleting the old ones. The materials store textures BY VALUE (copy), not by pointer, so the original AssetHandler slot was never freed when components were destroyed.

**Fix Applied:**

1. **Flux_Material::DeleteLoadedTextures()** - New method that deletes textures by their source paths:
   - Calls `Zenith_AssetHandler::DeleteTextureByPath()` for each texture slot with a valid path
   - Clears the texture slots after deletion

2. **Zenith_AssetHandler::DeleteTextureByPath(const std::string& strPath)** - New function that:
   - Searches the texture pool for a texture matching the source path
   - Deletes it via `DeleteTexture()` when found

3. **Zenith_TerrainComponent** - Added `m_bOwnsMaterials` flag:
   - Set to `true` when materials are created during `ReadFromDataStream()`
   - Destructor calls `DeleteLoadedTextures()` and `DeleteMaterial()` only if `m_bOwnsMaterials` is true

4. **Zenith_ModelComponent** - Updated destructor:
   - Calls `DeleteLoadedTextures()` on each material in `m_xCreatedMaterials` before deleting it
   - This ensures textures loaded during `ReadFromDataStream()` are properly cleaned up

**Key Pattern:** When deserializing, materials call `ReloadTexturesFromPaths()` which allocates texture slots. These slots must be freed by calling `DeleteLoadedTextures()` before the material is deleted. Components must track whether they created the materials (ownership) vs received them from external code.

**Critical Consideration:**
Entity pointers (like `s_pxSelectedEntity`) become invalid when the scene is reset. Always clear selection when stopping play mode.

## Physics Mesh Debug Visualization

**Location:** `Zenith_PhysicsMeshGenerator::DebugDrawAllPhysicsMeshes()`

**Purpose:** Renders wireframe visualization of all physics meshes in the scene using Flux_Primitives for debugging and verification.

### Integration with Editor (December 2024)

**CRITICAL:** Physics mesh debug drawing was moved from `Zenith_Physics::Update()` to the main render loop to ensure visualization works in all editor modes, not just during play.

**Old Behavior:**
```cpp
// In Zenith_Physics.cpp - only called during play mode
void Zenith_Physics::Update(float dt)
{
    // ... physics simulation ...

#ifdef ZENITH_TOOLS
    Zenith_PhysicsMeshGenerator::DebugDrawAllPhysicsMeshes();
#endif
}
```

**Problem:** Physics visualization only appeared when the editor was in "Playing" mode. In "Stopped" or "Paused" modes, physics meshes were invisible, making it impossible to verify they were correctly generated during scene load.

**New Behavior:**
```cpp
// In Zenith_Core.cpp - called every frame before rendering
void Zenith_Core::Zenith_MainLoop()
{
    // ... update game logic ...

#ifdef ZENITH_TOOLS
    // CRITICAL: Draw physics meshes BEFORE submitting render tasks
    // This ensures they render in all editor modes (Stopped/Paused/Playing)
    Zenith_PhysicsMeshGenerator::DebugDrawAllPhysicsMeshes();
#endif

    SubmitRenderTasks();
    WaitForRenderTasks();

    // ... end frame ...
}
```

**Benefit:** Physics meshes are now visible in all editor modes, making it easy to verify correct generation during scene deserialization.

### Rendering with Flux_Primitives

Physics meshes are rendered as green wireframe triangles using `Flux_Primitives::AddLine()`:

```cpp
void Zenith_PhysicsMeshGenerator::DebugDrawPhysicsMesh(
    const Flux_MeshGeometry* pxPhysicsMesh,
    const Zenith_Maths::Matrix4& xModelMatrix,
    const Zenith_Maths::Vector3& xColor)
{
    if (!pxPhysicsMesh || !g_xPhysicsMeshConfig.m_bDebugDraw)
        return;

    const Zenith_Maths::Vector3* pPositions = pxPhysicsMesh->m_pxPositions;
    const u_int* pIndices = pxPhysicsMesh->m_pxIndices;
    const u_int uNumIndices = pxPhysicsMesh->GetNumIndices();

    // Draw each triangle edge
    for (u_int i = 0; i < uNumIndices; i += 3)
    {
        // Transform vertices to world space
        Zenith_Maths::Vector3 v0 = xModelMatrix * Vector4(pPositions[pIndices[i + 0]], 1.0f);
        Zenith_Maths::Vector3 v1 = xModelMatrix * Vector4(pPositions[pIndices[i + 1]], 1.0f);
        Zenith_Maths::Vector3 v2 = xModelMatrix * Vector4(pPositions[pIndices[i + 2]], 1.0f);

        // Draw triangle edges
        Flux_Primitives::AddLine(v0, v1, xColor, 0.02f);
        Flux_Primitives::AddLine(v1, v2, xColor, 0.02f);
        Flux_Primitives::AddLine(v2, v0, xColor, 0.02f);
    }
}
```

**Default Color:** Green (0.0, 1.0, 0.0) - configurable via `g_xPhysicsMeshConfig.m_xDebugColor`

**Performance:** Minimal overhead - Flux_Primitives batches all lines into a single draw call per frame.

### Physics Mesh Auto-Generation

**Configuration:** Controlled by `g_xPhysicsMeshConfig` global variable in `Zenith_PhysicsMeshGenerator.cpp`

```cpp
PhysicsMeshConfig g_xPhysicsMeshConfig = {
    PHYSICS_MESH_QUALITY_HIGH,  // m_eQuality: Use full mesh geometry
    1.0f,                        // m_fSimplificationRatio: 1.0 = no simplification
    100,                         // m_uMinTriangles
    10000,                       // m_uMaxTriangles
    true,                        // m_bAutoGenerate: Automatically generate on load
    true,                        // m_bDebugDraw: Enable debug visualization
    Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f)  // m_xDebugColor: Green
};
```

**CRITICAL:** This global config MUST be initialized with proper defaults. Previously it was uninitialized, causing physics meshes to not be generated during scene deserialization.

### Deserialization Integration

**Location:** `Zenith_ModelComponent::ReadFromDataStream()` (lines 196-219)

After loading mesh geometry and materials, physics mesh generation is triggered automatically if enabled:

```cpp
void Zenith_ModelComponent::ReadFromDataStream(Zenith_DataStream& xStream)
{
    // ... load meshes and materials ...

    // Generate physics mesh after deserializing if auto-generation is enabled
    if (g_xPhysicsMeshConfig.m_bAutoGenerate && m_xMeshEntries.GetSize() > 0)
    {
        Zenith_Log("%s Auto-generating physics mesh for deserialized ModelComponent (entity: %s, meshes: %u)",
            LOG_TAG_MODEL_PHYSICS, m_xParentEntity.m_strName.c_str(), m_xMeshEntries.GetSize());
        GeneratePhysicsMesh();

        if (m_pxPhysicsMesh)
        {
            Zenith_Log("%s Physics mesh generated successfully: %u verts, %u tris",
                LOG_TAG_MODEL_PHYSICS, m_pxPhysicsMesh->GetNumVerts(), m_pxPhysicsMesh->GetNumIndices() / 3);
        }
        else
        {
            Zenith_Log("%s WARNING: Physics mesh generation failed!", LOG_TAG_MODEL_PHYSICS);
        }
    }
}
```

**Debug Logging:** Enabled to verify physics mesh generation during scene load. Check console output for confirmation.

## Entity Selection System

### Overview

The entity selection system allows users to pick entities in the 3D viewport using mouse raycasting. It provides the foundation for editor interactions like selecting objects to manipulate with gizmos, inspecting properties, or performing operations.

**CRITICAL (December 2024):** Selection now uses triangle-level raycasting on physics meshes for pixel-perfect accuracy.

**Key Files:**
- `Zenith/Editor/Zenith_SelectionSystem.h` - Selection system interface
- `Zenith/Editor/Zenith_SelectionSystem.cpp` - Ray-AABB intersection and bounding box calculation (~330 lines)
- `Zenith/Editor/Zenith_Editor.cpp` - Editor integration (handles mouse input)
- `Zenith/Editor/Zenith_Gizmo.h/cpp` - Screen-to-world ray conversion

### Raycasting Pipeline

```
1. Mouse Input
   └─→ ImGui captures mouse position in viewport window
       └─→ Mouse coordinates relative to viewport (not entire screen)

2. Screen-to-World Ray Conversion
   └─→ Zenith_Gizmo::ScreenToWorldRay(mousePos, viewportSize, rayOrigin, rayDir)
       │
       ├─→ Normalize to NDC [-1, 1]
       │   └─→ x = (mousePos.x / viewportSize.x) * 2.0 - 1.0
       │   └─→ y = (mousePos.y / viewportSize.y) * 2.0 - 1.0
       │   └─→ NOTE: No Y-inversion! Projection matrix handles Vulkan coords
       │
       ├─→ Clip space → View space (inverse projection)
       │   └─→ rayClip = (x, y, -1.0, 1.0)
       │   └─→ rayEye = inverse(projMatrix) * rayClip
       │
       └─→ View space → World space (inverse view)
           └─→ rayWorld = inverse(viewMatrix) * rayEye
           └─→ rayDir = normalize(rayWorld.xyz)

3. Bounding Box Update
   └─→ Zenith_SelectionSystem::UpdateBoundingBoxes()
       │
       ├─→ Get all entities with ModelComponent
       ├─→ For each entity:
       │   └─→ CalculateBoundingBox()
       │       ├─→ Iterate all vertices in model space
       │       ├─→ Find min/max (AABB in model space)
       │       └─→ Transform to world space via entity transform
       │
       └─→ Store in static map: entityID → BoundingBox

4. Ray-AABB Intersection
   └─→ Zenith_SelectionSystem::RaycastSelect(rayOrigin, rayDir)
       │
       ├─→ For each cached bounding box:
       │   └─→ BoundingBox::Intersects(ray) → distance
       │
       ├─→ Track closest intersection
       └─→ Return entityID of closest entity

5. Selection Update
   └─→ Zenith_Editor sets selected entity
       └─→ Updates gizmo target entity
           └─→ Shows translate/rotate/scale gizmo
```

### Screen-to-World Ray Conversion

**Location:** `Zenith_Gizmo::ScreenToWorldRay()`

**Purpose:** Converts 2D mouse position in viewport to 3D ray in world space for raycasting.

**Algorithm:**
```cpp
Zenith_Maths::Vector3 Zenith_Gizmo::ScreenToWorldRay(
    const Zenith_Maths::Vector2& xMousePos,
    const Zenith_Maths::Vector2& xViewportSize,
    Zenith_Maths::Vector3& xRayOriginOut,
    Zenith_Maths::Vector3& xRayDirOut)
{
    // 1. Normalize mouse position to NDC [-1, 1]
    float x = (xMousePos.x / xViewportSize.x) * 2.0f - 1.0f;
    float y = (xMousePos.y / xViewportSize.y) * 2.0f - 1.0f;
    
    // CRITICAL: No Y-inversion here!
    // The projection matrix already accounts for Vulkan's coordinate system
    // where Y points down in clip space. Adding y = -y would invert it twice.
    
    // 2. Construct clip space point (near plane)
    Zenith_Maths::Vector4 rayClip(x, y, -1.0f, 1.0f);  // z = -1 for near plane
    
    // 3. Transform to view space (camera space)
    Zenith_Maths::Matrix4 invProj = glm::inverse(Zenith_Camera::GetProjectionMatrix());
    Zenith_Maths::Vector4 rayEye = invProj * rayClip;
    rayEye = Zenith_Maths::Vector4(rayEye.x, rayEye.y, -1.0f, 0.0f);  // Direction vector
    
    // 4. Transform to world space
    Zenith_Maths::Matrix4 invView = glm::inverse(Zenith_Camera::GetViewMatrix());
    Zenith_Maths::Vector4 rayWorld = invView * rayEye;
    
    // 5. Extract ray direction and origin
    xRayDirOut = glm::normalize(Zenith_Maths::Vector3(rayWorld));
    
    // Ray origin is camera position
    Zenith_Maths::Vector3 xCamPos;
    Zenith_Camera::GetPosition(xCamPos);
    xRayOriginOut = xCamPos;
    
    return xRayDirOut;
}
```

**Coordinate System Notes:**
- **Screen Space:** Mouse position in pixels (0,0) = top-left of viewport
- **NDC (Normalized Device Coordinates):** [-1, 1] range, origin at center
- **Clip Space:** 4D homogeneous coordinates after projection
- **View Space:** 3D coordinates relative to camera
- **World Space:** 3D coordinates in scene

**CRITICAL BUG HISTORY - Y-Axis Inversion:**
Original implementation had `y = -y` after normalization, which caused inverted Y-axis picking (clicking top hit bottom). The projection matrix already handles Vulkan's Y-down convention, so manual inversion is WRONG. See [Flux/Gizmos/CLAUDE.md - Bug History](../Flux/Gizmos/CLAUDE.md#critical-bug-history) for full details.

### Bounding Box Calculation

**Location:** `Zenith_SelectionSystem::CalculateBoundingBox()`

**Purpose:** Computes axis-aligned bounding box (AABB) for an entity in world space.

**CRITICAL:** Uses physics mesh if available for more accurate selection bounds.

**Algorithm:**
```cpp
BoundingBox Zenith_SelectionSystem::CalculateBoundingBox(Zenith_Entity* pxEntity)
{
    // 1. Get entity's ModelComponent
    Zenith_ModelComponent& xModel = GetComponent<Zenith_ModelComponent>();

    // 2. Initialize to extreme values
    Vector3 xMin(FLT_MAX, FLT_MAX, FLT_MAX);
    Vector3 xMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    // 3. CRITICAL: Use physics mesh if available (more accurate)
    Flux_MeshGeometry* pxPhysicsMesh = xModel.GetPhysicsMesh();

    if (pxPhysicsMesh && pxPhysicsMesh->m_pxPositions && pxPhysicsMesh->GetNumVerts() > 0)
    {
        // Use physics mesh for bounding box (better for selection)
        const Vector3* pPositions = pxPhysicsMesh->m_pxPositions;
        const uint32_t uVertexCount = pxPhysicsMesh->GetNumVerts();

        for (uint32_t v = 0; v < uVertexCount; ++v)
        {
            xMin = glm::min(xMin, pPositions[v]);
            xMax = glm::max(xMax, pPositions[v]);
        }
    }
    else
    {
        // Fallback: Use render meshes if no physics mesh
        for (uint32_t i = 0; i < xModel.GetNumMeshEntries(); ++i)
        {
            Flux_MeshGeometry& xGeometry = xModel.GetMeshGeometryAtIndex(i);
            const Vector3* pPositions = xGeometry.m_pxPositions;
            const uint32_t uVertexCount = xGeometry.GetNumVerts();

            if (!pPositions || uVertexCount == 0) continue;

            // Find min/max in model space
            for (uint32_t v = 0; v < uVertexCount; ++v)
            {
                xMin = glm::min(xMin, pPositions[v]);
                xMax = glm::max(xMax, pPositions[v]);
            }
        }
    }

    // 4. Transform AABB to world space
    Zenith_TransformComponent& xTransform = GetComponent<Zenith_TransformComponent>();
    Matrix4 xTransformMatrix;
    xTransform.BuildModelMatrix(xTransformMatrix);  // Translation × Rotation × Scale

    BoundingBox xBoundingBox;
    xBoundingBox.m_xMin = xMin;
    xBoundingBox.m_xMax = xMax;
    xBoundingBox.Transform(xTransformMatrix);  // Transforms 8 corners, recomputes AABB

    return xBoundingBox;
}
```

**Transform Process:**
```cpp
void BoundingBox::Transform(const Matrix4& transform)
{
    // Create all 8 corners of the AABB
    Vector3 corners[8] = {
        Vector3(m_xMin.x, m_xMin.y, m_xMin.z),  // 000
        Vector3(m_xMax.x, m_xMin.y, m_xMin.z),  // 100
        Vector3(m_xMin.x, m_xMax.y, m_xMin.z),  // 010
        Vector3(m_xMax.x, m_xMax.y, m_xMin.z),  // 110
        Vector3(m_xMin.x, m_xMin.y, m_xMax.z),  // 001
        Vector3(m_xMax.x, m_xMin.y, m_xMax.z),  // 101
        Vector3(m_xMin.x, m_xMax.y, m_xMax.z),  // 011
        Vector3(m_xMax.x, m_xMax.y, m_xMax.z)   // 111
    };
    
    // Transform all corners to world space
    m_xMin = Vector3(FLT_MAX);
    m_xMax = Vector3(-FLT_MAX);
    
    for (int i = 0; i < 8; ++i)
    {
        Vector4 transformed = transform * Vector4(corners[i], 1.0f);
        Vector3 transformedPos = Vector3(transformed) / transformed.w;
        
        // Expand AABB to include transformed corner
        m_xMin = glm::min(m_xMin, transformedPos);
        m_xMax = glm::max(m_xMax, transformedPos);
    }
}
```

**Why Transform All 8 Corners:**
- Rotation can make AABB expand beyond original bounds
- Example: Rotating a box 45° diagonally increases its AABB size
- Transforming only min/max would give incorrect bounds after rotation
- Recomputing axis-aligned bounds ensures correctness

**Performance Notes:**
- **Complexity:** O(n × m) where n = entities, m = avg vertices per entity
- **Called:** Every frame in `UpdateBoundingBoxes()`
- **Cost:** ~0.1-1ms for 100 entities with 1000 verts each

**Optimization Opportunities:**

1. **Cache Model-Space Bounds:**
   ```cpp
   // Store in Flux_MeshGeometry during load
   struct Flux_MeshGeometry {
       BoundingBox m_xModelSpaceBounds;  // Computed once
   };
   
   // Only transform cached bounds, not all vertices
   BoundingBox worldBounds = modelBounds;
   worldBounds.Transform(entityTransform);
   ```

2. **Dirty Flagging:**
   ```cpp
   // Only recompute when transform changes
   if (xTransform.IsDirty())
   {
       UpdateBoundingBox(entity);
       xTransform.ClearDirty();
   }
   ```

3. **Spatial Partitioning:**
   ```cpp
   // Use octree or BVH for large scenes
   // Only test entities in relevant spatial cells
   Octree octree;
   octree.Insert(entity, boundingBox);
   auto candidates = octree.Query(ray);
   ```

### Ray-AABB Intersection Test

**Location:** `BoundingBox::Intersects()`

**Algorithm:** Slab method (efficient, robust)

```cpp
bool BoundingBox::Intersects(
    const Vector3& rayOrigin, 
    const Vector3& rayDir, 
    float& outDistance) const
{
    // Compute inverse ray direction (avoid division in loop)
    Vector3 invDir = 1.0f / rayDir;
    
    // Compute intersection distances for each axis slab
    Vector3 t0 = (m_xMin - rayOrigin) * invDir;  // Entry distances
    Vector3 t1 = (m_xMax - rayOrigin) * invDir;  // Exit distances
    
    // Handle negative ray directions (swap entry/exit)
    Vector3 tmin = glm::min(t0, t1);
    Vector3 tmax = glm::max(t0, t1);
    
    // Find overall entry/exit points
    float tNear = glm::max(glm::max(tmin.x, tmin.y), tmin.z);
    float tFar = glm::min(glm::min(tmax.x, tmax.y), tmax.z);
    
    // Check for intersection
    if (tNear > tFar || tFar < 0.0f)
    {
        return false;  // No intersection or behind ray origin
    }
    
    // Return distance to intersection
    outDistance = tNear > 0.0f ? tNear : tFar;
    return true;
}
```

**Why Slab Method:**
- **Fast:** Only 15-20 operations
- **Robust:** Handles edge cases (ray parallel to axis, inside box)
- **No branching:** SIMD-friendly
- **Industry standard:** Used in most ray tracers

**Intersection Cases:**
```
Ray misses box:
    tNear > tFar  (entry after exit - impossible)

Ray behind origin:
    tFar < 0  (box entirely behind ray)

Ray starts inside box:
    tNear < 0  (entry before origin)
    → Return tFar (exit distance)

Normal intersection:
    0 < tNear < tFar
    → Return tNear (entry distance)
```

### Triangle-Level Raycasting (Physics Mesh)

**Location:** `Zenith_SelectionSystem.cpp`

**Purpose:** Provides pixel-perfect entity selection by raycasting against the actual triangle geometry of physics meshes.

**CRITICAL:** Physics meshes are used preferentially over render meshes for selection accuracy. This requires position data to be retained when loading meshes from scene files.

#### Möller–Trumbore Ray-Triangle Intersection

**Algorithm:** Fast, robust ray-triangle intersection test used in real-time ray tracers.

**Location:** `RayTriangleIntersect()` helper function (lines 79-125 in Zenith_SelectionSystem.cpp)

```cpp
static bool RayTriangleIntersect(
    const Zenith_Maths::Vector3& rayOrigin,
    const Zenith_Maths::Vector3& rayDir,
    const Zenith_Maths::Vector3& v0,
    const Zenith_Maths::Vector3& v1,
    const Zenith_Maths::Vector3& v2,
    float& outT)
{
    const float EPSILON = 0.0000001f;

    // Edge vectors
    Zenith_Maths::Vector3 edge1 = v1 - v0;
    Zenith_Maths::Vector3 edge2 = v2 - v0;

    // Begin calculating determinant
    Zenith_Maths::Vector3 h = Zenith_Maths::Cross(rayDir, edge2);
    float a = Zenith_Maths::Dot(edge1, h);

    // Ray parallel to triangle
    if (a > -EPSILON && a < EPSILON)
        return false;

    float f = 1.0f / a;
    Zenith_Maths::Vector3 s = rayOrigin - v0;
    float u = f * Zenith_Maths::Dot(s, h);

    // Intersection outside triangle (u parameter)
    if (u < 0.0f || u > 1.0f)
        return false;

    Zenith_Maths::Vector3 q = Zenith_Maths::Cross(s, edge1);
    float v = f * Zenith_Maths::Dot(rayDir, q);

    // Intersection outside triangle (v parameter)
    if (v < 0.0f || u + v > 1.0f)
        return false;

    // Calculate distance along ray
    float t = f * Zenith_Maths::Dot(edge2, q);

    if (t > EPSILON)  // Ray intersection
    {
        outT = t;
        return true;
    }

    return false;  // Line intersection, but not ray
}
```

**How It Works:**

1. **Edge Vectors:** Compute two edges of the triangle (`edge1 = v1 - v0`, `edge2 = v2 - v0`)
2. **Determinant:** `a = dot(edge1, cross(rayDir, edge2))` - determines if ray is parallel to triangle
3. **Barycentric Coordinates:** Calculate `u` and `v` parameters
   - `u, v ∈ [0, 1]` and `u + v ≤ 1` means point is inside triangle
4. **Distance:** `t` is the distance along the ray to the intersection point

**Why Möller–Trumbore:**
- **Fast:** Only ~20 operations, minimal branching
- **Robust:** Handles edge cases (backface culling, parallel rays)
- **Numerically stable:** Uses barycentric coordinates
- **Industry standard:** Used in most real-time ray tracers

#### Physics Mesh Raycasting

**Location:** `RaycastPhysicsMesh()` helper function (lines 127-187 in Zenith_SelectionSystem.cpp)

**Purpose:** Tests a ray against all triangles in a physics mesh, returning the closest hit distance.

```cpp
static bool RaycastPhysicsMesh(
    const Flux_MeshGeometry* pxPhysicsMesh,
    const Zenith_Maths::Matrix4& xWorldMatrix,
    const Zenith_Maths::Vector3& rayOrigin,
    const Zenith_Maths::Vector3& rayDir,
    float& outDistance)
{
    if (!pxPhysicsMesh || !pxPhysicsMesh->m_pxPositions || !pxPhysicsMesh->m_pxIndices)
        return false;

    const Zenith_Maths::Vector3* pPositions = pxPhysicsMesh->m_pxPositions;
    const u_int* pIndices = pxPhysicsMesh->m_pxIndices;
    const u_int uNumIndices = pxPhysicsMesh->GetNumIndices();

    bool bHit = false;
    float fClosestDistance = FLT_MAX;

    // Iterate all triangles
    for (u_int i = 0; i < uNumIndices; i += 3)
    {
        // Get triangle vertices in model space
        Zenith_Maths::Vector3 v0 = pPositions[pIndices[i + 0]];
        Zenith_Maths::Vector3 v1 = pPositions[pIndices[i + 1]];
        Zenith_Maths::Vector3 v2 = pPositions[pIndices[i + 2]];

        // Transform to world space
        v0 = Zenith_Maths::Vector3(xWorldMatrix * Zenith_Maths::Vector4(v0, 1.0f));
        v1 = Zenith_Maths::Vector3(xWorldMatrix * Zenith_Maths::Vector4(v1, 1.0f));
        v2 = Zenith_Maths::Vector3(xWorldMatrix * Zenith_Maths::Vector4(v2, 1.0f));

        // Ray-triangle intersection test
        float fDistance;
        if (RayTriangleIntersect(rayOrigin, rayDir, v0, v1, v2, fDistance))
        {
            // CRITICAL BUG FIX (December 2024):
            // Original code compared loop variable 't' instead of 'fDistance'
            if (fDistance < fClosestDistance)
            {
                fClosestDistance = fDistance;  // Was: fClosestDistance = t (WRONG!)
                bHit = true;
            }
        }
    }

    if (bHit)
    {
        outDistance = fClosestDistance;
        return true;
    }

    return false;
}
```

**Key Implementation Details:**

1. **Transform Vertices:** Each triangle vertex is transformed from model space to world space using the entity's transform matrix
2. **Iterate All Triangles:** Loop through indices in groups of 3 (triangle list topology)
3. **Track Closest Hit:** Multiple triangles may be hit; we want the closest one
4. **Early Out:** Could add AABB test before triangle iteration for optimization (not implemented)

**Performance:**
- **Complexity:** O(n) where n = number of triangles in physics mesh
- **Typical Cost:** ~0.1-1ms for 1000 triangle mesh
- **Optimization Opportunity:** Use spatial acceleration structure (BVH, octree) for meshes with >10k triangles

#### Critical Bug History: Distance Comparison (December 2024)

**Symptom:** Entity selection was inaccurate; clicking on models didn't select the correct entity or failed to select anything.

**Root Cause:** In `RaycastPhysicsMesh`, the code was comparing the loop variable `t` (from a previous iteration or uninitialized) instead of the actual intersection distance `fDistance`:

```cpp
// WRONG - uses undefined variable 't'
if (t < fClosestDistance)
{
    fClosestDistance = t;
    bHit = true;
}

// CORRECT - uses the actual distance from RayTriangleIntersect
if (fDistance < fClosestDistance)
{
    fClosestDistance = fDistance;
    bHit = true;
}
```

**How Discovered:** Linter caught the bug after initial implementation, but behavior was still incorrect due to other issues (missing position data).

**Lesson Learned:** Always verify variable names in distance comparisons. Similar variable names (`t`, `outT`, `fDistance`) can cause subtle bugs.

#### Integration with RaycastSelect

**Location:** `Zenith_SelectionSystem::RaycastSelect()` (completely rewritten December 2024)

**Purpose:** Find the closest entity hit by a ray, using triangle-level precision when physics meshes are available.

**Algorithm:**
```cpp
Zenith_EntityID Zenith_SelectionSystem::RaycastSelect(
    const Zenith_Maths::Vector3& rayOrigin,
    const Zenith_Maths::Vector3& rayDir)
{
    Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

    float fClosestDistance = FLT_MAX;
    Zenith_EntityID closestEntity = INVALID_ENTITY_ID;

    // Iterate all entities with ModelComponent
    for (auto& [entityID, entity] : xScene.m_xEntityMap)
    {
        if (!xScene.EntityHasComponent<Zenith_ModelComponent>(entityID))
            continue;

        Zenith_ModelComponent& xModel = entity.GetComponent<Zenith_ModelComponent>();
        Zenith_TransformComponent& xTransform = entity.GetComponent<Zenith_TransformComponent>();

        // Build world transform matrix
        Zenith_Maths::Matrix4 xWorldMatrix;
        xTransform.BuildModelMatrix(xWorldMatrix);

        // CRITICAL: Use physics mesh if available (more accurate)
        Flux_MeshGeometry* pxPhysicsMesh = xModel.GetPhysicsMesh();

        if (pxPhysicsMesh)
        {
            // Triangle-level raycasting on physics mesh
            float fDistance;
            if (RaycastPhysicsMesh(pxPhysicsMesh, xWorldMatrix, rayOrigin, rayDir, fDistance))
            {
                if (fDistance < fClosestDistance)
                {
                    fClosestDistance = fDistance;
                    closestEntity = entityID;
                }
            }
        }
        else
        {
            // Fallback: Use AABB if no physics mesh
            BoundingBox bbox = CalculateBoundingBox(&entity);
            float fDistance;
            if (bbox.Intersects(rayOrigin, rayDir, fDistance))
            {
                if (fDistance < fClosestDistance)
                {
                    fClosestDistance = fDistance;
                    closestEntity = entityID;
                }
            }
        }
    }

    return closestEntity;
}
```

**Selection Strategy:**
1. **Physics Mesh Available:** Use triangle-level raycasting (pixel-perfect selection)
2. **No Physics Mesh:** Fall back to AABB intersection (bounding box approximation)

**Why Physics Mesh:**
- Physics meshes are often simplified versions of render meshes (better performance)
- Position data is retained when loading from scene files (required for raycasting)
- More accurate than AABB for complex shapes (e.g., concave objects, thin geometry)

**CRITICAL:** Physics meshes require position data to be retained during mesh loading:

```cpp
// In Zenith_ModelComponent::ReadFromDataStream()
u_int uRetainFlags = (1 << Flux_MeshGeometry::FLUX_VERTEX_ATTRIBUTE__POSITION);
Flux_MeshGeometry* pxMesh = Zenith_AssetHandler::AddMeshFromFile(
    strMeshPath.c_str(),
    uRetainFlags,  // CRITICAL: Retain position data for physics mesh generation
    true
);
```

Without this flag, position data is deleted after uploading to GPU, making physics mesh generation impossible.

#### Performance Characteristics

**Triangle Raycasting:**
- **100 triangles:** <0.1ms
- **1000 triangles:** ~0.5ms
- **10000 triangles:** ~5ms

**Per-Frame Cost (1000 entities, 1000 tris each):**
- Linear search: ~500ms (too slow!)
- With spatial acceleration: ~5-10ms (acceptable)

**Optimization Strategies:**

1. **Spatial Acceleration Structure:**
   ```cpp
   // Build BVH for each physics mesh (once at load time)
   struct BVHNode {
       BoundingBox bbox;
       uint32_t firstTriangle, numTriangles;
       BVHNode* left;
       BVHNode* right;
   };

   // Traverse BVH instead of testing all triangles
   bool RaycastBVH(BVHNode* node, ray, distance);
   ```

2. **Early Out with AABB:**
   ```cpp
   // Test entity AABB before triangle iteration
   if (!entityAABB.Intersects(ray))
       continue;  // Skip expensive triangle tests
   ```

3. **Frustum Culling:**
   ```cpp
   // Only test entities visible to camera
   if (!frustum.Contains(entityBounds))
       continue;
   ```

### Selection Update Flow

**Location:** `Zenith_Editor.cpp` - viewport mouse handling

```cpp
void Zenith_Editor::HandleViewportInput()
{
    // Get mouse position relative to viewport
    ImVec2 mousePos = ImGui::GetMousePos();
    ImVec2 viewportPos = ImGui::GetCursorScreenPos();
    ImVec2 viewportSize = ImGui::GetContentRegionAvail();

    // Convert to viewport-relative coordinates
    Vector2 relativePos(mousePos.x - viewportPos.x, mousePos.y - viewportPos.y);

    // Check if mouse is inside viewport
    if (relativePos.x < 0 || relativePos.y < 0 ||
        relativePos.x > viewportSize.x || relativePos.y > viewportSize.y)
    {
        return;  // Outside viewport
    }

    // Mouse click for entity selection
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGuizmo::IsOver())
    {
        // Generate ray
        Vector3 rayOrigin, rayDir;
        Zenith_Gizmo::ScreenToWorldRay(relativePos, viewportSize, rayOrigin, rayDir);

        // Update bounding boxes (every frame for now)
        Zenith_SelectionSystem::UpdateBoundingBoxes();

        // Raycast selection - uses triangle-level precision if physics mesh available
        Zenith_EntityID selectedID = Zenith_SelectionSystem::RaycastSelect(rayOrigin, rayDir);

        if (selectedID != INVALID_ENTITY_ID)
        {
            Zenith_Scene& scene = Zenith_Scene::GetCurrentScene();
            Zenith_Entity* pEntity = scene.GetEntityPtr(selectedID);
            SelectEntity(pEntity);

            // Update gizmo target
            Flux_Gizmos::SetTargetEntity(pEntity);
        }
        else
        {
            // Clicked empty space - deselect
            SelectEntity(nullptr);
            Flux_Gizmos::SetTargetEntity(nullptr);
        }
    }
}
```

**Integration with Gizmos:**
```
Entity Selected
    ↓
Zenith_Editor::SelectEntity(pEntity)
    ↓
Flux_Gizmos::SetTargetEntity(pEntity)
    ↓
Gizmo appears at entity position
    ↓
W/E/R keys change gizmo mode
    ↓
Mouse drag transforms entity
```

### Handling Non-Renderable Entities

**Current Limitation:** Only entities with `ModelComponent` can be selected.

**Problem:** Can't select cameras, lights, empty transform nodes, etc.

**Future Solution:**
```cpp
BoundingBox Zenith_SelectionSystem::CalculateBoundingBox(Zenith_Entity* pxEntity)
{
    // Check for ModelComponent first
    if (scene.EntityHasComponent<Zenith_ModelComponent>(entityID))
    {
        // Use mesh geometry bounds (current implementation)
        return CalculateFromMesh(entity);
    }
    
    // Fallback for non-renderable entities
    if (scene.EntityHasComponent<Zenith_TransformComponent>(entityID))
    {
        // Create small cube at transform position
        Vector3 pos;
        transform.GetPosition(pos);
        
        const float radius = 0.5f;  // 1 unit cube
        return BoundingBox(pos - Vector3(radius), pos + Vector3(radius));
    }
    
    // No transform - can't be selected
    return BoundingBox();  // Invalid/empty
}
```

### Performance Characteristics

**UpdateBoundingBoxes() Profile:**
- **100 entities, 1000 verts each:** ~1ms
- **1000 entities, 1000 verts each:** ~10ms
- **1000 entities, 10000 verts each:** ~100ms

**RaycastSelect() Profile:**
- **Linear search:** O(n) where n = number of entities
- **100 entities:** <0.1ms
- **1000 entities:** ~0.5ms
- **10000 entities:** ~5ms

**Optimization Priority:**
1. Cache model-space bounds (10x speedup for UpdateBoundingBoxes)
2. Dirty flagging (skip unchanged entities)
3. Spatial partitioning for large scenes (>5000 entities)

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

1. **Entity pointers can become invalid** - Should use EntityID for robust selection
2. **No undo/redo** - Requires command pattern implementation
3. **No multi-selection** - Only one entity at a time
4. **Selection performance** - Linear search through all entities; needs spatial acceleration for large scenes

### Recently Fixed (December 2024)

- ✅ **Physics mesh visualization** - Now renders in all editor modes (Stopped/Paused/Playing)
- ✅ **Entity selection accuracy** - Triangle-level raycasting on physics meshes for pixel-perfect selection
- ✅ **Physics mesh generation** - Auto-generated during scene deserialization with position data retention
- ✅ **Scene state preservation** - Play/Stop mode correctly saves and restores scene state
- ✅ **ScriptComponent serialization** - Behavior registry pattern for polymorphic behavior serialization
- ✅ **TerrainComponent physics** - All 4096 physics chunks loaded during deserialization
- ✅ **Material texture paths** - Textures reload correctly after scene reset
- ✅ **Axis hit testing** - Proper ray-cylinder intersection in world space for gizmos
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

**Last Updated:** 2025-12-16
**Author:** Claude (Anthropic)
**Status:** Editor fully functional with triangle-level selection, physics mesh visualization, and scene state preservation
