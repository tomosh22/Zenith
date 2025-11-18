# Flux Gizmos System - Editor Manipulation Gizmos

## Overview

The Flux Gizmos system provides interactive 3D manipulation tools for the Zenith Editor, allowing users to transform entities (translate, rotate, scale) directly in the 3D viewport using visual gizmos.

## Architecture

### Components

**1. Gizmo Rendering** ([Flux_Gizmos.h](Flux_Gizmos.h), [Flux_Gizmos.cpp](Flux_Gizmos.cpp))
- Custom geometry generation for arrows, circles, and cubes
- Forward rendering with depth testing disabled (always on top)
- Highlight system for hover and active states
- Auto-scaling based on camera distance for consistent screen size

**2. Mouse Interaction**
- Ray-gizmo intersection testing
- Constraint-based dragging (axis-aligned translation, planar rotation, etc.)
- Real-time entity transform manipulation

**3. Shader Pipeline** ([Flux_Gizmos.vert](../Shaders/Gizmos/Flux_Gizmos.vert), [Flux_Gizmos.frag](../Shaders/Gizmos/Flux_Gizmos.frag))
- Unlit rendering for clear visibility
- Per-vertex color with highlight intensity
- Push constants for per-gizmo transform and state

## Gizmo Modes

### Translation Mode
**Visual:** Three colored arrows (X=Red, Y=Green, Z=Blue)
**Interaction:** Click and drag arrow to move entity along that axis
**Implementation:** Line-ray closest point calculation projects mouse movement onto axis

### Rotation Mode
**Visual:** Three colored circles/toruses around each axis
**Interaction:** Click circle and drag to rotate around that axis
**Implementation:** Projects mouse onto rotation plane, calculates angle from initial click point

### Scale Mode
**Visual:** Three colored arrows (like translation) + center cube for uniform scale
**Interaction:**
- Click arrow: Scale along that axis
- Click center cube: Uniform scale (all axes)
**Implementation:** Calculates scale factor from distance change along ray

## Usage

### Initialization

```cpp
#ifdef ZENITH_TOOLS
void MyEditor::Initialise() {
    Flux_Gizmos::Initialise();  // Load shaders, create pipelines, generate geometry
}
```

### Rendering

```cpp
void MyEditor::Update() {
    // Set target entity to manipulate
    if (selectedEntity) {
        Flux_Gizmos::SetTargetEntity(selectedEntity);
    }

    // Set gizmo mode based on user input (hotkeys, toolbar, etc.)
    if (Input::IsKeyPressed(KEY_W))
        Flux_Gizmos::SetGizmoMode(GizmoMode::Translate);
    else if (Input::IsKeyPressed(KEY_E))
        Flux_Gizmos::SetGizmoMode(GizmoMode::Rotate);
    else if (Input::IsKeyPressed(KEY_R))
        Flux_Gizmos::SetGizmoMode(GizmoMode::Scale);

    // Submit rendering task
    Flux_Gizmos::SubmitRenderTask();
}

void MyEditor::Render() {
    Flux_Gizmos::WaitForRenderTask();
}
```

### Mouse Interaction

```cpp
void MyEditor::HandleMouseInput() {
    // Build ray from mouse cursor
    Zenith_Physics::RaycastInfo rayInfo = Zenith_Physics::BuildRayFromMouse(camera);

    // Begin interaction on mouse down
    if (Input::IsMouseButtonPressed(MOUSE_LEFT)) {
        Flux_Gizmos::BeginInteraction(rayInfo.m_xOrigin, rayInfo.m_xDirection);
    }

    // Update interaction while dragging
    if (Flux_Gizmos::IsInteracting()) {
        Flux_Gizmos::UpdateInteraction(rayInfo.m_xOrigin, rayInfo.m_xDirection);
    }

    // End interaction on mouse release
    if (Input::IsMouseButtonReleased(MOUSE_LEFT)) {
        Flux_Gizmos::EndInteraction();
    }
}
```

## Technical Details

### Geometry Generation

**Arrow Geometry** (Translation & Scale):
- Cylinder shaft: 8-segment circular cross-section
- Cone head: Tapered tip for directional indication
- Generated procedurally on initialization
- Stored in GPU vertex/index buffers

**Circle Geometry** (Rotation):
- 64-segment torus ring around axis
- Rendered as connected line segments (degenerate triangles)
- Perpendicular to rotation axis

**Cube Geometry** (Uniform Scale):
- Simple 6-face cube at gizmo origin
- Only visible in Scale mode

### Raycasting

**Ray-Cylinder Intersection** (Arrows):
```cpp
// Quadratic formula for ray-infinite-cylinder intersection
// Then clamp to arrow length (0 to GIZMO_ARROW_LENGTH)
float a = dot(rayDir, rayDir) - dot(axis, rayDir)^2;
float b = 2 * (dot(rayDir, rayOrigin) - dot(axis, rayDir) * dot(axis, rayOrigin));
float c = dot(rayOrigin, rayOrigin) - dot(axis, rayOrigin)^2 - radius^2;

float t = (-b ± sqrt(b^2 - 4ac)) / 2a;
if (0 <= dot(hitPoint, axis) <= arrowLength) -> HIT
```

**Ray-Circle Intersection** (Rotation):
```cpp
// 1. Intersect ray with circle plane (perpendicular to axis)
float t = -dot(axis, rayOrigin) / dot(axis, rayDir);
Vector3 hitPoint = rayOrigin + rayDir * t;

// 2. Check if hit point is on the circle (distance from center ≈ radius)
if (abs(length(hitPoint) - circleRadius) < threshold) -> HIT
```

**Ray-AABB Intersection** (Cube):
```cpp
// Standard slab method
Vector3 tMin = (boxMin - rayOrigin) / rayDir;
Vector3 tMax = (boxMax - rayOrigin) / rayDir;

float tNear = max(tMin.x, tMin.y, tMin.z);
float tFar = min(tMax.x, tMax.y, tMax.z);

if (tNear <= tFar && tFar >= 0) -> HIT
```

### Transform Manipulation

**Translation:**
```cpp
// Project ray onto constraint axis to find closest point
// This gives the translation amount along that axis
Vector3 toRayOrigin = rayOrigin - initialEntityPosition;
float rayDotAxis = dot(rayDir, axis);
float originDotAxis = dot(toRayOrigin, axis);

// Line-line closest point problem
float t = originDotAxis - dot(rayDir - axis * rayDotAxis, toRayOrigin) /
          (1.0 - rayDotAxis * rayDotAxis);

Vector3 newPosition = initialEntityPosition + axis * t;
```

**Rotation:**
```cpp
// 1. Intersect current ray with rotation plane
Vector3 currentPoint = rayOrigin + rayDir * t;

// 2. Calculate angle between initial and current vectors
Vector3 initialVec = normalize(interactionStartPos - entityPosition);
Vector3 currentVec = normalize(currentPoint - entityPosition);

float angle = acos(dot(initialVec, currentVec));

// 3. Determine rotation direction using cross product
if (dot(cross(initialVec, currentVec), axis) < 0)
    angle = -angle;

// 4. Apply delta rotation to initial rotation
Quaternion deltaRotation = angleAxis(angle, axis);
Quaternion newRotation = deltaRotation * initialEntityRotation;
```

**Scale:**
```cpp
// Calculate distance ratio from interaction start to current position
float initialDist = length(interactionStartPos - entityPosition);
float currentDist = length(currentRayPoint - entityPosition);

float scaleFactor = currentDist / initialDist;

// Apply to appropriate axis (or all axes for uniform scale)
Vector3 newScale = initialEntityScale;
switch (activeComponent) {
    case ScaleX: newScale.x *= scaleFactor; break;
    case ScaleY: newScale.y *= scaleFactor; break;
    case ScaleZ: newScale.z *= scaleFactor; break;
    case ScaleXYZ: newScale *= scaleFactor; break;  // Uniform
}
```

### Rendering Pipeline

**Vertex Input:**
- Position (vec3) + Color (vec3) interleaved
- 6 floats per vertex (24 bytes)

**Pipeline State:**
- **Depth Test:** Disabled (gizmos always on top)
- **Depth Write:** Disabled
- **Blending:** Enabled (for semi-transparent hover states)
- **Topology:** Triangle list
- **Cull Mode:** Back-face culling

**Descriptor Sets:**
- Set 0, Binding 0: Frame constants (view/projection matrices)

**Push Constants:**
```cpp
struct GizmoPushConstants {
    mat4 modelMatrix;           // Gizmo world transform
    float highlightIntensity;   // 0=normal, 0.5=hovered, 1.0=active
    float padding[3];
};
```

**Shaders:**
- Vertex: Transforms geometry, applies highlight to color
- Fragment: Simple unlit output (gizmos should be clearly visible)

### Auto-Scaling

Gizmos automatically scale with camera distance to maintain consistent screen size:

```cpp
float distance = length(entityPosition - cameraPosition);
float gizmoScale = distance / AUTO_SCALE_DISTANCE;  // AUTO_SCALE_DISTANCE = 5.0

Matrix4 gizmoMatrix = translate(entityPosition) * scale(gizmoScale);
```

This ensures gizmos are:
- Large enough to interact with when far away
- Small enough not to obscure the entity when close

## Integration with Editor

### Required Steps

1. **Initialize in Editor Startup:**
```cpp
Flux_Gizmos::Initialise();
```

2. **Update in Editor Loop:**
```cpp
// Set target and mode
Flux_Gizmos::SetTargetEntity(selectedEntity);
Flux_Gizmos::SetGizmoMode(currentMode);

// Handle mouse input
HandleGizmoInteraction();

// Submit rendering
Flux_Gizmos::SubmitRenderTask();
```

3. **Wait Before Present:**
```cpp
Flux_Gizmos::WaitForRenderTask();
```

4. **Shutdown on Exit:**
```cpp
Flux_Gizmos::Shutdown();
```

### Example Editor Integration

```cpp
void Zenith_Editor::RenderGizmos() {
    if (!s_pxSelectedEntity)
        return;

    // Update gizmo target
    Flux_Gizmos::SetTargetEntity(s_pxSelectedEntity);
    Flux_Gizmos::SetGizmoMode(s_eGizmoMode);

    // Handle mouse interaction
    if (s_bViewportHovered) {
        Zenith_CameraComponent& camera = GetEditorCamera();
        auto rayInfo = Zenith_Physics::BuildRayFromMouse(camera);

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            Flux_Gizmos::BeginInteraction(rayInfo.m_xOrigin, rayInfo.m_xDirection);
        }

        if (Flux_Gizmos::IsInteracting()) {
            Flux_Gizmos::UpdateInteraction(rayInfo.m_xOrigin, rayInfo.m_xDirection);
        }

        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            Flux_Gizmos::EndInteraction();
        }
    }

    // Render
    Flux_Gizmos::SubmitRenderTask();
}
```

## Debug Features

**Debug Variables** (ZENITH_TOOLS only):
- `Editor/Gizmos/Render` - Toggle gizmo rendering
- `Editor/Gizmos/Alpha` - Adjust gizmo opacity

**Profiling:**
- `ZENITH_PROFILE_INDEX__FLUX_GIZMOS` tracks rendering performance
- Viewable in ImGui profiler

## Performance Considerations

**Geometry:**
- Static geometry generated once at initialization
- Stored in GPU memory (no per-frame uploads)
- Low poly count: ~300 triangles per gizmo mode

**Rendering:**
- Single draw call per gizmo component (3-4 total per frame)
- No depth writes or complex shaders (fast fragment processing)
- Auto-scales to avoid distant overdraw

**Interaction:**
- Raycasting is CPU-only (no GPU queries)
- O(1) tests per frame (max 3-4 components)
- Transforms applied directly to entity component (no intermediate state)

## Limitations & Future Improvements

**Current Limitations:**
1. **No Multi-Selection:** Only one entity can have gizmo at a time
2. **No Custom Axes:** Rotation/translation always world-aligned (not local-space)
3. **No Snapping:** No grid/angle snapping for precise transformations
4. **No Undo/Redo:** Transform changes aren't tracked for history

**Potential Improvements:**
1. **Local vs World Space Toggle:** Allow rotation/translation in entity's local coordinate frame
2. **Multi-Entity Manipulation:** Apply transformations to multiple selected entities
3. **Snapping System:** Grid snapping for translation, angle snapping for rotation
4. **Plane Manipulation:** Add XY, YZ, XZ plane handles for 2-axis translation
5. **Scale Planes:** Add planes for 2-axis scaling (scale XY together, etc.)
6. **Better Visual Feedback:** Add visual guides (grid projection, angle arc, etc.)
7. **Coordinate Display:** Show numerical values during manipulation
8. **Camera-Relative Mode:** Rotate/translate relative to camera view
9. **Touch/Tablet Support:** Multi-touch gestures for manipulation

## File Structure

```
Zenith/Flux/Gizmos/
├── Flux_Gizmos.h              # Public API and data structures
├── Flux_Gizmos.cpp            # Implementation
├── CLAUDE.md                  # This documentation
└── ../Shaders/Gizmos/
    ├── Flux_Gizmos.vert       # Vertex shader (GLSL)
    ├── Flux_Gizmos.frag       # Fragment shader (GLSL)
    ├── Flux_Gizmos.vert.spv   # Compiled SPIR-V (generated)
    └── Flux_Gizmos.frag.spv   # Compiled SPIR-V (generated)
```

## References

- **Entity-Component System:** See `Zenith/EntityComponent/CLAUDE.md`
- **Flux Rendering:** See `Zenith/Flux/CLAUDE.md`
- **Editor Architecture:** See `Games/Test/Build/Zenith/Editor/`
- **Physics Raycasting:** See `Zenith/Physics/Zenith_Physics.h`

---

*Last Updated: 2025*
*For Zenith Engine - Flux Gizmos System*
