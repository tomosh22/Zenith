# Zenith Editor Implementation Plan

## Overview

This document provides a comprehensive implementation plan for the Zenith Editor system, including entity gizmos, scene serialization, and play/pause/stop functionality. This plan is designed to enable Claude Sonnet 4.5 to successfully implement the complete editor system.

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [File Structure](#file-structure)
3. [Component Dependencies](#component-dependencies)
4. [Implementation Phases](#implementation-phases)
5. [Detailed Implementation Guide](#detailed-implementation-guide)
6. [Testing Strategy](#testing-strategy)

---

## Architecture Overview

### System Components

The editor system consists of three main components:

1. **Zenith_Editor** - Main editor window and state management
2. **Zenith_SelectionSystem** - Entity picking and bounding box management
3. **Zenith_Gizmo** - Interactive transform gizmos (translate, rotate, scale)

### Integration Points

- **ECS System**: Interfaces with `Zenith_Scene` and `Zenith_Entity`
- **Rendering System**: Uses `Flux_Graphics` for rendering game viewport
- **Input System**: Uses `Zenith_Input` for mouse and keyboard input
- **Camera System**: Uses `Zenith_CameraComponent` for viewport navigation

### Data Flow

```
User Input (Mouse/Keyboard)
    ?
Zenith_Editor::Update()
    ?
HandleObjectPicking() ? Zenith_SelectionSystem::RaycastSelect()
    ?
Zenith_Gizmo::Manipulate() ? Transform entity
    ?
Render loop ? RenderGizmos() + Viewport rendering
```

---

## File Structure

### Existing Files

```
Zenith/Editor/
??? Zenith_Editor.h          - Editor main class header
??? Zenith_Editor.cpp        - Editor main class implementation
??? Zenith_SelectionSystem.h - Selection and picking header
??? Zenith_SelectionSystem.cpp - Selection and picking implementation
??? Zenith_Gizmo.h           - Gizmo manipulation header
??? Zenith_Gizmo.cpp         - Gizmo manipulation implementation
??? EDITOR_IMPLEMENTATION_PLAN.md - This document
```

### New Files Required

```
Zenith/Editor/
??? Zenith_EditorCamera.h/.cpp - Editor-specific camera controller (optional)
```

---

## Component Dependencies

### Zenith_Editor Dependencies

- **ImGui**: For UI rendering
- **Zenith_Scene**: Scene management and entity access
- **Zenith_Entity**: Entity manipulation
- **Zenith_SelectionSystem**: Object picking
- **Zenith_Gizmo**: Transform manipulation
- **Flux_Graphics**: Viewport rendering
- **Zenith_Input**: User input handling

### Zenith_SelectionSystem Dependencies

- **Zenith_Scene**: Access to entities
- **Zenith_Entity**: Entity querying
- **Zenith_ModelComponent**: Mesh data for bounding boxes
- **Zenith_TransformComponent**: Transform data
- **Zenith_Maths**: Vector and ray math

### Zenith_Gizmo Dependencies

- **Zenith_Entity**: Transform modification
- **Zenith_TransformComponent**: Component access
- **Zenith_Input**: Mouse input
- **Zenith_Maths**: Transform calculations
- **Rendering system**: For drawing gizmo geometry (optional, can use debug lines)

---

## Implementation Phases

### Phase 1: Core Editor Infrastructure ? (Mostly Complete)
- [x] Editor window setup with ImGui docking
- [x] Toolbar with play/pause/stop buttons
- [x] Hierarchy panel structure
- [x] Properties panel structure
- [x] Viewport panel with game rendering
- [ ] Scene backup/restore mechanism

### Phase 2: Entity Selection System
- [ ] Bounding box calculation for all entities
- [ ] Screen-to-world ray conversion
- [ ] Ray-AABB intersection testing
- [ ] Entity picking on mouse click
- [ ] Selection highlighting

### Phase 3: Gizmo System
- [ ] Gizmo geometry rendering (arrows, rings, boxes)
- [ ] Mouse interaction with gizmo axes
- [ ] Translate gizmo functionality
- [ ] Rotate gizmo functionality
- [ ] Scale gizmo functionality
- [ ] Snapping support

### Phase 4: Scene Serialization
- [ ] Component serialization interface
- [ ] Entity serialization
- [ ] Scene save/load functionality
- [ ] File dialog integration

### Phase 5: Play Mode System
- [ ] Scene state backup before play
- [ ] Scene state restoration on stop
- [ ] Pause functionality
- [ ] Editor camera vs. game camera switching

---

## Detailed Implementation Guide

## 1. Zenith_Editor Implementation

### 1.1 Scene State Management

#### SetEditorMode() - Scene backup/restore

```cpp
void Zenith_Editor::SetEditorMode(EditorMode eMode)
{
    if (s_eEditorMode == eMode)
        return;
    
    EditorMode oldMode = s_eEditorMode;
    s_eEditorMode = eMode;
    
    // IMPLEMENT: Transition from Stopped to Playing
    if (oldMode == EditorMode::Stopped && eMode == EditorMode::Playing)
    {
        Zenith_Log("Editor: Entering Play Mode");
        
        // TODO: Create a deep copy of the current scene
        // 1. Allocate new scene: s_pxBackupScene = new Zenith_Scene()
        // 2. Copy all entities from current scene
        // 3. For each entity:
        //    a. Create new entity in backup scene
        //    b. Copy all components (Transform, Model, Camera, etc.)
        //    c. Preserve entity relationships (parent-child)
        // 4. Store main camera reference
        
        // NOTE: This requires implementing proper scene copying/cloning
        // since Zenith_Scene doesn't have a copy constructor
    }
    
    // IMPLEMENT: Transition from Playing/Paused to Stopped
    else if (oldMode != EditorMode::Stopped && eMode == EditorMode::Stopped)
    {
        Zenith_Log("Editor: Stopping Play Mode");
        
        // TODO: Restore the backed-up scene
        // 1. Clear current scene
        // 2. Copy all entities from backup scene
        // 3. Restore main camera
        // 4. Delete backup scene
        if (s_pxBackupScene)
        {
            // Clear current scene
            Zenith_Scene::GetCurrentScene().Reset();
            
            // TODO: Implement scene restoration logic
            // This is the reverse of the backup process
            
            delete s_pxBackupScene;
            s_pxBackupScene = nullptr;
        }
    }
    
    // IMPLEMENT: Pause/Resume transitions
    else if (eMode == EditorMode::Paused)
    {
        Zenith_Log("Editor: Pausing");
        // Scene update will be skipped in main loop when paused
    }
    else if (oldMode == EditorMode::Paused && eMode == EditorMode::Playing)
    {
        Zenith_Log("Editor: Resuming");
        // Scene update will resume in main loop
    }
}
```

**Implementation Strategy:**
1. Create `Zenith_Scene::Clone()` method
2. Implement component-level copy constructors
3. Handle entity ID mapping during copy
4. Preserve parent-child relationships

### 1.2 Hierarchy Panel

#### RenderHierarchyPanel() - Display all entities

```cpp
void Zenith_Editor::RenderHierarchyPanel()
{
    ImGui::Begin("Hierarchy");
    
    ImGui::Text("Scene Entities:");
    ImGui::Separator();
    
    // TODO: Iterate through all entities in the scene
    // 1. Get reference to current scene
    Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
    
    // 2. Access m_xEntityMap (this is currently private - may need friend class)
    // FOR EACH entity in m_xEntityMap:
    //   a. Create selectable ImGui item with entity name
    //   b. Check if clicked - if so, call SelectEntity()
    //   c. Highlight if this is the currently selected entity
    //   d. TODO (Future): Handle hierarchy (indentation for children)
    
    // EXAMPLE PSEUDO-CODE:
    // for (auto& [entityID, entity] : xScene.m_xEntityMap)
    // {
    //     bool bIsSelected = (s_pxSelectedEntity && s_pxSelectedEntity->GetEntityID() == entityID);
    //     if (ImGui::Selectable(entity.m_strName.c_str(), bIsSelected))
    //     {
            //         SelectEntity(&entity);  // NOTE: Need to handle entity lifetime
    //     }
    // }
    
    // IMPLEMENTATION NOTE:
    // - m_xEntityMap is private - either make it public or add friend class
    // - Entity pointers may become invalid - consider storing EntityID instead
    // - Need to handle case where entity is deleted while selected
    
    ImGui::End();
}
```

**Implementation Challenges:**
1. `m_xEntityMap` is private - needs accessor or friend declaration
2. Entity pointers in `s_pxSelectedEntity` may become invalid
3. Consider storing `Zenith_EntityID` instead of pointer

**Recommended Solution:**
- Add `GetAllEntities()` method to `Zenith_Scene`
- Store selected entity as `Zenith_EntityID` instead of pointer
- Add safety checks for entity validity

### 1.3 Object Picking

#### HandleObjectPicking() - Mouse-to-world raycasting

```cpp
void Zenith_Editor::HandleObjectPicking()
{
    // IMPLEMENT: Only pick when viewport is hovered and mouse is clicked
    if (!s_bViewportHovered)
        return;
    
    // Check if left mouse button was just pressed (not held)
    if (!Zenith_Input::WasKeyPressedThisFrame(ZENITH_MOUSE_BUTTON_LEFT))
        return;
    
    // TODO: Get mouse position in viewport space
    // 1. Get global mouse position using Zenith_Input::GetMousePosition()
    Zenith_Maths::Vector2_64 xGlobalMousePos;
    Zenith_Input::GetMousePosition(xGlobalMousePos);
    
    // 2. Convert to viewport-relative coordinates
    // xViewportMousePos = xGlobalMousePos - s_xViewportPos;
    Zenith_Maths::Vector2 xViewportMousePos = {
        static_cast<float>(xGlobalMousePos.x - s_xViewportPos.x),
        static_cast<float>(xGlobalMousePos.y - s_xViewportPos.y)
    };
    
    // 3. Check if mouse is within viewport bounds
    if (xViewportMousePos.x < 0 || xViewportMousePos.x > s_xViewportSize.x ||
        xViewportMousePos.y < 0 || xViewportMousePos.y > s_xViewportSize.y)
        return;
    
    // TODO: Convert screen position to world ray
    // 1. Get camera matrices
    Zenith_CameraComponent& xCamera = Zenith_Scene::GetCurrentScene().GetMainCamera();
    Zenith_Maths::Matrix4 xViewMatrix, xProjMatrix;
    xCamera.BuildViewMatrix(xViewMatrix);
    xCamera.BuildProjectionMatrix(xProjMatrix);
    
    // 2. Use Zenith_Gizmo::ScreenToWorldRay() to get ray direction
    // (Need to implement this function first)
    Zenith_Maths::Vector3 xRayOrigin, xRayDir;
    // xRayDir = Zenith_Gizmo::ScreenToWorldRay(xViewportMousePos, {0,0}, s_xViewportSize, xViewMatrix, xProjMatrix);
    
    // For now, use camera position as ray origin
    xCamera.GetPosition(xRayOrigin);
    
    // TODO: Perform raycast using selection system
    Zenith_Entity* pxHitEntity = Zenith_SelectionSystem::RaycastSelect(xRayOrigin, xRayDir);
    
    if (pxHitEntity)
    {
        SelectEntity(pxHitEntity);
    }
    else
    {
        ClearSelection();
    }
}
```

**Dependencies:**
1. Implement `Zenith_Gizmo::ScreenToWorldRay()`
2. Implement `Zenith_SelectionSystem::RaycastSelect()`
3. Ensure viewport position/size are properly tracked

### 1.4 Gizmo Rendering

#### RenderGizmos() - Draw transform gizmos

```cpp
void Zenith_Editor::RenderGizmos()
{
    // IMPLEMENT: Only render gizmos if an entity is selected
    if (!s_pxSelectedEntity)
        return;
    
    // Only render gizmos in stopped or paused mode (not during play)
    if (s_eEditorMode == EditorMode::Playing)
        return;
    
    // TODO: Get camera matrices for gizmo rendering
    Zenith_CameraComponent& xCamera = Zenith_Scene::GetCurrentScene().GetMainCamera();
    Zenith_Maths::Matrix4 xViewMatrix, xProjMatrix;
    xCamera.BuildViewMatrix(xViewMatrix);
    xCamera.BuildProjectionMatrix(xProjMatrix);
    
    // TODO: Call gizmo manipulation
    // This both renders the gizmo AND handles interaction
    GizmoOperation eOperation = static_cast<GizmoOperation>(s_eGizmoMode);
    bool bWasManipulated = Zenith_Gizmo::Manipulate(
        s_pxSelectedEntity,
        eOperation,
        xViewMatrix,
        xProjMatrix,
        s_xViewportPos,
        s_xViewportSize
    );
    
    // TODO (Optional): Render selection bounding box
    // Zenith_SelectionSystem::RenderSelectedBoundingBox(s_pxSelectedEntity);
}
```

**Integration Point:**
- Call `RenderGizmos()` after `RenderViewport()` in `Render()` method
- Gizmos should be rendered on top of the scene

### 1.5 Viewport State Tracking

#### RenderViewport() - Track viewport info for picking

```cpp
void Zenith_Editor::RenderViewport()
{
    ImGui::Begin("Viewport");
    
    // IMPLEMENT: Track viewport position and size
    // These are needed for mouse picking
    ImVec2 xViewportPanelPos = ImGui::GetCursorScreenPos();
    s_xViewportPos = { xViewportPanelPos.x, xViewportPanelPos.y };
    
    ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
    s_xViewportSize = { viewportPanelSize.x, viewportPanelSize.y };
    
    // IMPLEMENT: Track hover and focus state
    s_bViewportHovered = ImGui::IsWindowHovered();
    s_bViewportFocused = ImGui::IsWindowFocused();
    
    // Get the final render target SRV
    Flux_ShaderResourceView& xGameRenderSRV = Flux_Graphics::s_xFinalRenderTarget.m_axColourAttachments[0].m_pxSRV;
    
    if (xGameRenderSRV.m_xImageView != VK_NULL_HANDLE)
    {
        // Register the texture with ImGui
        VkDescriptorSet xDescriptorSet = ImGui_ImplVulkan_AddTexture(
            Flux_Graphics::s_xRepeatSampler.GetSampler(),
            xGameRenderSRV.m_xImageView,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );
        
        // Display the game render target as an image
        ImGui::Image((ImTextureID)(uintptr_t)xDescriptorSet, viewportPanelSize);
    }
    else
    {
        ImGui::Text("Game render target not available");
    }
    
    ImGui::End();
}
```

---

## 2. Zenith_SelectionSystem Implementation

### 2.1 Bounding Box Management

#### UpdateBoundingBoxes() - Calculate all entity bounds

```cpp
void Zenith_SelectionSystem::UpdateBoundingBoxes()
{
    s_xEntityBoundingBoxes.clear();
    
    Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
    
    // IMPLEMENT: Get all entities with model components
    Zenith_Vector<Zenith_ModelComponent*> xModelComponents;
    xScene.GetAllOfComponentType<Zenith_ModelComponent>(xModelComponents);
    
    // FOR EACH model component:
    for (u_int i = 0; i < xModelComponents.GetSize(); ++i)
    {
        Zenith_ModelComponent* pxModel = xModelComponents.Get(i);
        Zenith_Entity xEntity = pxModel->GetParentEntity();
        Zenith_EntityID uEntityID = xEntity.GetEntityID();
        
        // Calculate and cache bounding box
        BoundingBox xBoundingBox = CalculateBoundingBox(&xEntity);
        s_xEntityBoundingBoxes[uEntityID] = xBoundingBox;
    }
}
```

**When to Call:**
- Once per frame in editor update loop
- After any entity transform changes
- After entities are added/removed

**Performance Consideration:**
- This is O(n) where n = number of entities
- Consider dirty flagging for optimization

### 2.2 Bounding Box Calculation

#### CalculateBoundingBox() - Compute AABB from mesh

```cpp
BoundingBox Zenith_SelectionSystem::CalculateBoundingBox(Zenith_Entity* pxEntity)
{
    BoundingBox xBoundingBox;
    
    if (!pxEntity)
        return xBoundingBox;
    
    Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
    
    // Check if entity has a model component
    if (!xScene.EntityHasComponent<Zenith_ModelComponent>(pxEntity->GetEntityID()))
    {
        // FALLBACK: Create small default box at entity position
        if (xScene.EntityHasComponent<Zenith_TransformComponent>(pxEntity->GetEntityID()))
        {
            Zenith_TransformComponent& xTransform = xScene.GetComponentFromEntity<Zenith_TransformComponent>(pxEntity->GetEntityID());
            Zenith_Maths::Vector3 xPosition;
            xTransform.GetPosition(xPosition);
            
            // Small 1-unit cube for non-renderable entities
            xBoundingBox.m_xMin = xPosition - Zenith_Maths::Vector3(0.5f);
            xBoundingBox.m_xMax = xPosition + Zenith_Maths::Vector3(0.5f);
        }
        return xBoundingBox;
    }
    
    Zenith_ModelComponent& xModel = xScene.GetComponentFromEntity<Zenith_ModelComponent>(pxEntity->GetEntityID());
    
    // Initialize min/max to extreme values
    Zenith_Maths::Vector3 xMin(std::numeric_limits<float>::max());
    Zenith_Maths::Vector3 xMax(std::numeric_limits<float>::lowest());
    
    // IMPLEMENT: Iterate through all mesh entries in the model
    for (u_int i = 0; i < xModel.GetNumMeshEntires(); ++i)
    {
        Flux_MeshGeometry& xGeometry = xModel.GetMeshGeometryAtIndex(i);
        
        // Use the position array directly
        const Zenith_Maths::Vector3* pPositions = xGeometry.m_pxPositions;
        const u_int uVertexCount = xGeometry.GetNumVerts();
        
        if (!pPositions || uVertexCount == 0)
            continue;
        
        // Find min/max from positions
        for (u_int v = 0; v < uVertexCount; ++v)
        {
            xMin = glm::min(xMin, pPositions[v]);
            xMax = glm::max(xMax, pPositions[v]);
        }
    }
    
    // IMPLEMENT: Apply entity transform
    if (xScene.EntityHasComponent<Zenith_TransformComponent>(pxEntity->GetEntityID()))
    {
        Zenith_TransformComponent& xTransform = xScene.GetComponentFromEntity<Zenith_TransformComponent>(pxEntity->GetEntityID());
        Zenith_Maths::Matrix4 xTransformMatrix;
        xTransform.BuildModelMatrix(xTransformMatrix);
        
        xBoundingBox.m_xMin = xMin;
        xBoundingBox.m_xMax = xMax;
        xBoundingBox.Transform(xTransformMatrix);
    }
    else
    {
        xBoundingBox.m_xMin = xMin;
        xBoundingBox.m_xMax = xMax;
    }
    
    return xBoundingBox;
}
```

### 2.3 Raycasting

#### RaycastSelect() - Find entity under mouse

```cpp
Zenith_Entity* Zenith_SelectionSystem::RaycastSelect(const Zenith_Maths::Vector3& rayOrigin, const Zenith_Maths::Vector3& rayDir)
{
    float closestDistance = std::numeric_limits<float>::max();
    Zenith_EntityID closestEntityID = 0;
    bool bFoundHit = false;
    
    // IMPLEMENT: Check all cached bounding boxes
    for (auto& [uEntityID, xBoundingBox] : s_xEntityBoundingBoxes)
    {
        float distance;
        if (xBoundingBox.Intersects(rayOrigin, rayDir, distance))
        {
            if (distance < closestDistance)
            {
                closestDistance = distance;
                closestEntityID = uEntityID;
                bFoundHit = true;
            }
        }
    }
    
    // IMPLEMENT: Return the closest hit entity
    if (bFoundHit)
    {
        Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
        Zenith_Entity xEntity = xScene.GetEntityByID(closestEntityID);
        
        // PROBLEM: Returning pointer to local variable!
        // SOLUTION OPTIONS:
        // 1. Return EntityID instead of pointer
        // 2. Store entity in static variable
        // 3. Use shared_ptr or entity handle system
        
        // TEMPORARY FIX: Store in static variable
        s_pxSelectedEntity = new Zenith_Entity(xEntity);
        return s_pxSelectedEntity;
    }
    
    return nullptr;
}
```

**Critical Issue:**
- Returning pointer to local `Zenith_Entity` is unsafe
- **Recommended Fix**: Change API to return `Zenith_EntityID` instead

### 2.4 Debug Rendering

#### RenderSelectedBoundingBox() - Visual feedback

```cpp
void Zenith_SelectionSystem::RenderSelectedBoundingBox(Zenith_Entity* pxEntity)
{
    if (!pxEntity)
        return;
    
    BoundingBox xBox = GetEntityBoundingBox(pxEntity);
    
    // TODO: Render wireframe box
    // IMPLEMENTATION OPTIONS:
    
    // OPTION 1: Use debug line renderer (recommended)
    // Need to create or integrate with existing debug rendering system
    // Color color = Color::sYellow;
    // DrawWireBox(xBox, color);
    
    // OPTION 2: Use Flux command list to submit line primitives
    // More complex but integrates with rendering pipeline
    
    // OPTION 3: Use ImGui draw list (viewport overlay)
    // Project 3D box corners to screen space
    // Draw lines in ImGui viewport overlay
    
    // PSEUDO-CODE for ImGui approach:
    // Get all 8 corners of box
    // Project to screen space using camera
    // Draw lines between corners using ImGui::GetWindowDrawList()
}
```

**Implementation Recommendation:**
- Start with ImGui overlay approach for simplicity
- Later integrate with proper 3D debug rendering system

---

## 3. Zenith_Gizmo Implementation

### 3.1 Screen-to-World Ray Conversion

#### ScreenToWorldRay() - Convert mouse to 3D ray

```cpp
Zenith_Maths::Vector3 Zenith_Gizmo::ScreenToWorldRay(
    const Zenith_Maths::Vector2& mousePos,
    const Zenith_Maths::Vector2& viewportPos,
    const Zenith_Maths::Vector2& viewportSize,
    const Zenith_Maths::Matrix4& viewMatrix,
    const Zenith_Maths::Matrix4& projMatrix)
{
    // TODO: Implement screen-to-world ray conversion
    
    // STEP 1: Normalize mouse position to [-1, 1] range (NDC)
    // x = (mousePos.x / viewportSize.x) * 2.0 - 1.0
    // y = (mousePos.y / viewportSize.y) * 2.0 - 1.0
    float x = (mousePos.x / viewportSize.x) * 2.0f - 1.0f;
    float y = (mousePos.y / viewportSize.y) * 2.0f - 1.0f;
    
    // NOTE: Y may need to be inverted depending on coordinate system
    // y = 1.0f - (mousePos.y / viewportSize.y) * 2.0f;
    
    // STEP 2: Create clip space coordinates
    // Near plane (z = -1 or 0 depending on API)
    Zenith_Maths::Vector4 xRayClip = { x, y, -1.0f, 1.0f };
    
    // STEP 3: Transform to view space
    Zenith_Maths::Matrix4 xInvProjection = glm::inverse(projMatrix);
    Zenith_Maths::Vector4 xRayEye = xInvProjection * xRayClip;
    
    // Set z forward, w = 0 for direction vector
    xRayEye.z = -1.0f;
    xRayEye.w = 0.0f;
    
    // STEP 4: Transform to world space
    Zenith_Maths::Matrix4 xInvView = glm::inverse(viewMatrix);
    Zenith_Maths::Vector4 xRayWorld4 = xInvView * xRayEye;
    
    // STEP 5: Extract and normalize direction
    Zenith_Maths::Vector3 xRayWorld = { xRayWorld4.x, xRayWorld4.y, xRayWorld4.z };
    xRayWorld = glm::normalize(xRayWorld);
    
    return xRayWorld;
}
```

**Alternative Implementation (Using Camera):**
```cpp
// Can also use Zenith_CameraComponent::ScreenSpaceToWorldSpace()
// but need to adapt for ray direction
```

### 3.2 Translate Gizmo

#### HandleTranslateGizmo() - Drag to move entity

```cpp
bool Zenith_Gizmo::HandleTranslateGizmo(
    Zenith_Entity* pxEntity,
    const Zenith_Maths::Matrix4& viewMatrix,
    const Zenith_Maths::Matrix4& projMatrix,
    const Zenith_Maths::Vector2& viewportPos,
    const Zenith_Maths::Vector2& viewportSize)
{
    if (!pxEntity || !pxEntity->HasComponent<Zenith_TransformComponent>())
        return false;
    
    Zenith_TransformComponent& xTransform = pxEntity->GetComponent<Zenith_TransformComponent>();
    Zenith_Maths::Vector3 xEntityPos;
    xTransform.GetPosition(xEntityPos);
    
    // IMPLEMENT: State machine for gizmo interaction
    
    // STATE 1: Not manipulating - check for mouse hover and click
    if (!s_bIsManipulating)
    {
        // Get mouse position
        Zenith_Maths::Vector2_64 xMousePos64;
        Zenith_Input::GetMousePosition(xMousePos64);
        Zenith_Maths::Vector2 xMousePos = { 
            static_cast<float>(xMousePos64.x - viewportPos.x),
            static_cast<float>(xMousePos64.y - viewportPos.y)
        };
        
        // TODO: Check if mouse is over any gizmo axis
        // 1. Project gizmo axes to screen space
        // 2. Check distance from mouse to each axis line
        // 3. Highlight hovered axis
        GizmoAxis eHoveredAxis = DetermineHoveredAxis(xEntityPos, xMousePos, viewMatrix, projMatrix, viewportSize);
        
        // If clicked on an axis, start manipulation
        if (eHoveredAxis != GizmoAxis::None && 
            Zenith_Input::WasKeyPressedThisFrame(ZENITH_MOUSE_BUTTON_LEFT))
        {
            s_bIsManipulating = true;
            s_eActiveAxis = eHoveredAxis;
            s_xManipulationStartPos = xEntityPos;
            s_xMouseStartPos = xMousePos;
        }
    }
    
    // STATE 2: Manipulating - update entity position
    else
    {
        // Get current mouse position
        Zenith_Maths::Vector2_64 xMousePos64;
        Zenith_Input::GetMousePosition(xMousePos64);
        Zenith_Maths::Vector2 xMousePos = { 
            static_cast<float>(xMousePos64.x - viewportPos.x),
            static_cast<float>(xMousePos64.y - viewportPos.y)
        };
        
        // TODO: Calculate translation delta
        // 1. Create world-space ray from mouse position
        Zenith_Maths::Vector3 xRayDir = ScreenToWorldRay(xMousePos, viewportPos, viewportSize, viewMatrix, projMatrix);
        
        // 2. Get camera position (ray origin)
        Zenith_Maths::Vector3 xCameraPos;
        Zenith_CameraComponent& xCamera = Zenith_Scene::GetCurrentScene().GetMainCamera();
        xCamera.GetPosition(xCameraPos);
        
        // 3. Determine constraint plane based on active axis
        Zenith_Maths::Vector3 xPlaneNormal;
        if (s_eActiveAxis == GizmoAxis::X)
        {
            // Plane perpendicular to X axis (YZ plane)
            xPlaneNormal = { 1.0f, 0.0f, 0.0f };
        }
        else if (s_eActiveAxis == GizmoAxis::Y)
        {
            // Plane perpendicular to Y axis (XZ plane)
            xPlaneNormal = { 0.0f, 1.0f, 0.0f };
        }
        else if (s_eActiveAxis == GizmoAxis::Z)
        {
            // Plane perpendicular to Z axis (XY plane)
            xPlaneNormal = { 0.0f, 0.0f, 1.0f };
        }
        
        // 4. Intersect ray with plane
        float fIntersectionT = RayPlaneIntersection(xCameraPos, xRayDir, s_xManipulationStartPos, xPlaneNormal);
        
        if (fIntersectionT >= 0.0f)
        {
            // 5. Calculate new position
            Zenith_Maths::Vector3 xIntersectionPoint = xCameraPos + xRayDir * fIntersectionT;
            Zenith_Maths::Vector3 xNewPos = s_xManipulationStartPos;
            
            // Apply movement only along active axis
            if (s_eActiveAxis == GizmoAxis::X)
                xNewPos.x = xIntersectionPoint.x;
            else if (s_eActiveAxis == GizmoAxis::Y)
                xNewPos.y = xIntersectionPoint.y;
            else if (s_eActiveAxis == GizmoAxis::Z)
                xNewPos.z = xIntersectionPoint.z;
            
            // 6. Apply snapping if enabled
            if (s_bSnapEnabled)
            {
                xNewPos = glm::round(xNewPos / s_fSnapValue) * s_fSnapValue;
            }
            
            // 7. Update entity transform
            xTransform.SetPosition(xNewPos);
        }
        
        // Check for mouse release to end manipulation
        if (!Zenith_Input::IsKeyDown(ZENITH_MOUSE_BUTTON_LEFT))
        {
            s_bIsManipulating = false;
            s_eActiveAxis = GizmoAxis::None;
        }
    }
    
    // TODO: Render the gizmo
    RenderTranslateGizmo(xEntityPos, viewMatrix, projMatrix);
    
    return s_bIsManipulating;
}
```

**Helper Function Needed:**
```cpp
// Determine which axis the mouse is hovering over
GizmoAxis DetermineHoveredAxis(
    const Zenith_Maths::Vector3& gizmoPos,
    const Zenith_Maths::Vector2& mousePos,
    const Zenith_Maths::Matrix4& viewMatrix,
    const Zenith_Maths::Matrix4& projMatrix,
    const Zenith_Maths::Vector2& viewportSize);
```

### 3.3 Gizmo Rendering

#### RenderTranslateGizmo() - Draw visual representation

```cpp
void Zenith_Gizmo::RenderTranslateGizmo(
    const Zenith_Maths::Vector3& position,
    const Zenith_Maths::Matrix4& viewMatrix,
    const Zenith_Maths::Matrix4& projMatrix)
{
    // TODO: Render three colored arrows (X=red, Y=green, Z=blue)
    
    // IMPLEMENTATION APPROACH 1: ImGui Overlay (Simple)
    // 1. Project gizmo geometry to screen space
    // 2. Draw using ImGui::GetBackgroundDrawList() or GetForegroundDrawList()
    
    // IMPLEMENTATION APPROACH 2: 3D Geometry (Better)
    // 1. Create arrow mesh geometry
    // 2. Submit to rendering system
    // 3. Render with unlit shader on top of scene
    
    // PSEUDO-CODE for ImGui approach:
    // ImDrawList* pDrawList = ImGui::GetForegroundDrawList();
    // 
    // // X Axis (Red)
    // Vector3 xAxisEnd = position + Vector3(s_fGizmoSize, 0, 0);
    // Vector2 screenStart = WorldToScreen(position);
    // Vector2 screenEnd = WorldToScreen(xAxisEnd);
    // pDrawList->AddLine(screenStart, screenEnd, IM_COL32(255, 0, 0, 255), 3.0f);
    // // Add arrow head...
    // 
    // // Y Axis (Green)
    // // Z Axis (Blue)
}
```

**Gizmo Rendering Requirements:**
1. Always render on top (disable depth test or render last)
2. Scale with distance to maintain constant screen size
3. Highlight hovered/active axis
4. Simple geometric shapes (arrows, rings, boxes)

### 3.4 Rotate Gizmo (Similar Pattern)

#### HandleRotateGizmo() - Drag to rotate entity

```cpp
bool Zenith_Gizmo::HandleRotateGizmo(...)
{
    // TODO: Similar to translate, but:
    // 1. Show three colored circles/rings around object
    // 2. Dragging on ring rotates around that axis
    // 3. Calculate rotation angle from mouse delta
    // 4. Apply to entity's rotation component
    
    // MATHEMATICAL APPROACH:
    // 1. Project mouse movement to rotation plane
    // 2. Calculate angle from start position
    // 3. Convert to quaternion rotation
    // 4. Apply to entity transform
}
```

### 3.5 Scale Gizmo (Similar Pattern)

#### HandleScaleGizmo() - Drag to scale entity

```cpp
bool Zenith_Gizmo::HandleScaleGizmo(...)
{
    // TODO: Similar to translate, but:
    // 1. Show three colored boxes/cubes at axis ends
    // 2. Dragging scales along that axis
    // 3. Can include uniform scale option (all axes)
    
    // IMPLEMENTATION:
    // 1. Calculate scale factor from mouse delta
    // 2. Multiply entity scale by factor
    // 3. Support minimum scale limit (prevent negative/zero scale)
}
```

---

## 4. Scene Serialization

### 4.1 Component Serialization Interface

Each component needs to implement serialization:

```cpp
// In Zenith_TransformComponent
void Serialize(std::ofstream& out) const
{
    out << "Transform\n";
    out << m_xPosition.x << " " << m_xPosition.y << " " << m_xPosition.z << "\n";
    // Serialize rotation (quaternion)
    out << m_xRotation.x << " " << m_xRotation.y << " " << m_xRotation.z << " " << m_xRotation.w << "\n";
    // Serialize scale
    out << m_xScale.x << " " << m_xScale.y << " " << m_xScale.z << "\n";
}

void Deserialize(std::ifstream& in)
{
    std::string componentType;
    in >> componentType; // Should be "Transform"
    in >> m_xPosition.x >> m_xPosition.y >> m_xPosition.z;
    in >> m_xRotation.x >> m_xRotation.y >> m_xRotation.z >> m_xRotation.w;
    in >> m_xScale.x >> m_xScale.y >> m_xScale.z;
}
```

### 4.2 Entity Serialization

```cpp
void Zenith_Entity::Serialize(std::ofstream& out)
{
    // Write entity metadata
    out << "Entity\n";
    out << m_uEntityID << "\n";
    out << m_strName << "\n";
    out << m_uParentEntityID << "\n";
    
    // Write component count
    auto& componentMap = m_pxParentScene->m_xEntityComponents.Get(m_uEntityID);
    out << componentMap.size() << "\n";
    
    // Serialize each component
    // TODO: Need component type registry to know how to serialize each type
    for (auto& [typeID, index] : componentMap)
    {
        out << typeID << "\n";
        
        // Dispatch to appropriate serialization function based on typeID
        // This requires a type registry mapping TypeID -> serialization function
    }
}
```

### 4.3 Scene Serialization

```cpp
void Zenith_Scene::Serialize(const std::string& strFilename)
{
    std::ofstream out(strFilename, std::ios::binary);
    
    if (!out.is_open())
    {
        Zenith_Log("Failed to open file for scene serialization: %s", strFilename.c_str());
        return;
    }
    
    // Write header
    out << "ZenithScene\n";
    out << "Version 1\n";
    
    // Write entity count
    out << m_xEntityMap.size() << "\n";
    
    // Serialize each entity
    for (auto& [id, entity] : m_xEntityMap)
    {
        entity.Serialize(out);
    }
    
    out.close();
}
```

### 4.4 Component Type Registry

**Problem**: Need to know which component type corresponds to each TypeID for deserialization

**Solution**: Create a component type registry

```cpp
class ComponentTypeRegistry
{
public:
    using SerializeFunc = void(*)(void* pComponent, std::ofstream& out);
    using DeserializeFunc = void(*)(void* pComponent, std::ifstream& in);
    using CreateFunc = void*(*)(Zenith_Scene* pScene, Zenith_EntityID entityID);
    
    struct ComponentTypeInfo
    {
        std::string strName;
        SerializeFunc pfnSerialize;
        DeserializeFunc pfnDeserialize;
        CreateFunc pfnCreate;
    };
    
    template<typename T>
    static void RegisterComponent(const std::string& name)
    {
        TypeID typeID = Zenith_Scene::TypeIDGenerator::GetTypeID<T>();
        s_xRegistry[typeID] = {
            name,
            [](void* p, std::ofstream& out) { static_cast<T*>(p)->Serialize(out); },
            [](void* p, std::ifstream& in) { static_cast<T*>(p)->Deserialize(in); },
            [](Zenith_Scene* scene, Zenith_EntityID id) -> void* {
                return &scene->CreateComponent<T>(id);
            }
        };
    }
    
    static ComponentTypeInfo* GetTypeInfo(TypeID typeID)
    {
        auto it = s_xRegistry.find(typeID);
        return (it != s_xRegistry.end()) ? &it->second : nullptr;
    }
    
private:
    static std::unordered_map<TypeID, ComponentTypeInfo> s_xRegistry;
};
```

**Usage:**
```cpp
// At engine initialization
ComponentTypeRegistry::RegisterComponent<Zenith_TransformComponent>("Transform");
ComponentTypeRegistry::RegisterComponent<Zenith_ModelComponent>("Model");
ComponentTypeRegistry::RegisterComponent<Zenith_CameraComponent>("Camera");
// etc...
```

---

## 5. Integration Points

### 5.1 Main Loop Integration

```cpp
// In main game loop or editor loop
void EditorUpdate()
{
    // Update bounding boxes for all entities
    Zenith_SelectionSystem::UpdateBoundingBoxes();
    
    // Update editor state
    Zenith_Editor::Update();
    
    // Only update scene if in play mode and not paused
    if (Zenith_Editor::GetEditorMode() == EditorMode::Playing)
    {
        Zenith_Scene::Update(deltaTime);
    }
}

void EditorRender()
{
    // Render game scene to render target
    // ... existing game rendering code ...
    
    // Render editor UI
    Zenith_Editor::Render();
    
    // Render editor overlays (gizmos, selection, etc.)
    // This happens within Zenith_Editor::Render() -> RenderGizmos()
}
```

### 5.2 Input Handling

```cpp
// Ensure viewport gets input priority
void HandleInput()
{
    // ImGui consumes input automatically
    // Zenith_Input will only receive input if ImGui doesn't consume it
    
    // Editor handles object picking
    // Gizmo manipulation is handled in Zenith_Gizmo
}
```

---

## 6. Testing Strategy

### Phase 1: Selection System Testing
1. Create test scene with multiple entities
2. Click entities to select them
3. Verify selection is highlighted in hierarchy
4. Verify properties panel updates

### Phase 2: Gizmo Testing
1. Select entity
2. Test translate gizmo on each axis
3. Test rotate gizmo on each axis
4. Test scale gizmo on each axis
5. Verify snapping works correctly

### Phase 3: Play Mode Testing
1. Set up scene in editor
2. Press Play - verify scene state is preserved
3. Modify entities during play
4. Press Stop - verify scene reverts to edit state
5. Test Pause/Resume functionality

### Phase 4: Serialization Testing
1. Create complex scene
2. Save scene to file
3. Clear scene
4. Load scene from file
5. Verify all entities and components are restored

---

## 7. Common Pitfalls and Solutions

### Pitfall 1: Entity Pointer Lifetime
**Problem**: Storing `Zenith_Entity*` that becomes invalid when entity is deleted

**Solution**: Store `Zenith_EntityID` instead and look up entity when needed

### Pitfall 2: Scene Copy Performance
**Problem**: Deep copying entire scene is slow

**Solution**: Implement incremental dirty tracking or use copy-on-write techniques

### Pitfall 3: Gizmo Interaction Conflicts
**Problem**: Gizmo and viewport camera both want mouse input

**Solution**: Check if gizmo is being manipulated before allowing camera movement

### Pitfall 4: Coordinate Space Confusion
**Problem**: Mixing screen space, viewport space, and world space coordinates

**Solution**: Clearly document and name variables based on their coordinate space

### Pitfall 5: ImGui Widget IDs
**Problem**: ImGui widgets with same label cause ID collisions

**Solution**: Use `ImGui::PushID()` / `ImGui::PopID()` or unique label strings

---

## 8. Future Enhancements

### Multi-Selection
- Allow selecting multiple entities
- Transform all selected entities together
- Grouped operations

### Undo/Redo System
- Command pattern for all editor operations
- Undo stack with configurable depth
- Redo functionality

### Prefab System
- Save entity hierarchies as prefabs
- Instantiate prefabs in scene
- Prefab inheritance and overrides

### Advanced Gizmos
- Local vs. World space modes
- Pivot point selection
- Custom gizmo shapes for different component types

### Grid Snapping
- Visual grid overlay
- Snap to grid during translation
- Configurable grid size

---

## 9. API Reference

### Key Functions to Implement

#### Zenith_Editor
- `void SetEditorMode(EditorMode)`
- `void HandleObjectPicking()`
- `void RenderGizmos()`
- `void RenderHierarchyPanel()`
- `void SelectEntity(Zenith_Entity*)`

#### Zenith_SelectionSystem
- `void UpdateBoundingBoxes()`
- `BoundingBox CalculateBoundingBox(Zenith_Entity*)`
- `Zenith_Entity* RaycastSelect(Vector3, Vector3)`
- `void RenderSelectedBoundingBox(Zenith_Entity*)`

#### Zenith_Gizmo
- `bool Manipulate(entity, operation, view, proj, viewport)`
- `Vector3 ScreenToWorldRay(mouse, viewport, view, proj)`
- `void RenderTranslateGizmo(pos, view, proj)`
- `void RenderRotateGizmo(pos, view, proj)`
- `void RenderScaleGizmo(pos, view, proj)`

#### Zenith_Scene
- `void Serialize(string filename)`
- `void Deserialize(string filename)`
- `Zenith_Scene* Clone()`

---

## 10. Implementation Checklist

### Core Editor [ ]
- [ ] Scene backup/restore on play/stop
- [ ] Hierarchy panel entity list
- [ ] Entity selection handling
- [ ] Properties panel updates

### Selection System [ ]
- [ ] Bounding box calculation
- [ ] Bounding box caching
- [ ] Mouse picking raycasting
- [ ] Selection highlighting

### Gizmo System [ ]
- [ ] Screen-to-world ray conversion
- [ ] Translate gizmo rendering
- [ ] Translate gizmo interaction
- [ ] Rotate gizmo rendering
- [ ] Rotate gizmo interaction
- [ ] Scale gizmo rendering
- [ ] Scale gizmo interaction
- [ ] Snapping support

### Serialization [ ]
- [ ] Component serialization interface
- [ ] Entity serialization
- [ ] Scene serialization
- [ ] Component type registry
- [ ] Deserialization
- [ ] File format specification

### Integration [ ]
- [ ] Main loop integration
- [ ] Input handling
- [ ] Camera switching (editor vs. game)
- [ ] Viewport state tracking
- [ ] Performance optimization

---

## Conclusion

This implementation plan provides a comprehensive guide for implementing the Zenith Editor system. Each section includes detailed pseudo-code, implementation notes, and considerations for common pitfalls.

The key to successful implementation is to:
1. Start with core infrastructure (Phase 1-2)
2. Implement one gizmo type completely before moving to others
3. Test each phase thoroughly before moving to the next
4. Keep performance in mind (bounding box caching, etc.)
5. Handle edge cases (null pointers, invalid entities, etc.)

When implementing, refer to the detailed function comments in the source files for specific implementation guidance.
