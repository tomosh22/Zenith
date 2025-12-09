# Flux Gizmos System - Editor Manipulation Gizmos

## Overview

The Flux Gizmos system provides interactive 3D manipulation tools for the Zenith Editor, allowing users to transform entities (translate, rotate, scale) directly in the 3D viewport using visual gizmos.

**Key Files:**
- [Flux_Gizmos.h](Flux_Gizmos.h) / [Flux_Gizmos.cpp](Flux_Gizmos.cpp) - Core gizmo rendering and interaction
- [Zenith_Gizmo.h](../../Editor/Zenith_Gizmo.h) / [Zenith_Gizmo.cpp](../../Editor/Zenith_Gizmo.cpp) - Utility functions (ScreenToWorldRay)
- [Zenith_Editor.cpp](../../Editor/Zenith_Editor.cpp) - Editor integration and input handling

---

## ⚠️ CRITICAL BUG HISTORY - READ BEFORE MODIFYING

The gizmo system has had several subtle bugs that took significant debugging effort to resolve. Before making ANY changes, understand these pitfalls:

### Bug #1: Interaction State Reset Every Frame (CRITICAL)

**Symptom:** Dragging stops responding after a fraction of a second

**Root Cause:** `RenderGizmos()` was calling `SetTargetEntity()` and `SetGizmoMode()` every frame, which reset `s_bIsInteracting = false`, immediately ending the drag operation.

**Location:** [Zenith_Editor.cpp:693-700](../../Editor/Zenith_Editor.cpp#L693-L700)

**Fix:** Only update target/mode when NOT actively interacting:
```cpp
// CRITICAL: Only update target/mode when NOT interacting!
if (!Flux_Gizmos::IsInteracting())
{
    Flux_Gizmos::SetTargetEntity(pxSelectedEntity);
    Flux_Gizmos::SetGizmoMode(static_cast<GizmoMode>(s_eGizmoMode));
}
```

**Lesson:** Never reset interaction state from render/update functions that run every frame.

---

### Bug #2: Raycast Coordinate Space Mismatch (CRITICAL)

**Symptom:** Gizmos can only be clicked near the world origin; clicking elsewhere misses

**Root Cause:** The ray origin was transformed to "local space" (divided by scale) but the ray direction was NOT, causing the intersection math to mix units:

```cpp
// BROKEN - Mixed coordinate spaces!
Zenith_Maths::Vector3 localRayOrigin = (rayOrigin - xGizmoPos) / s_fGizmoScale;
Zenith_Maths::Vector3 localRayDir = rayDir;  // NOT scaled - unit mismatch!
```

When the quadratic intersection formula uses `localRayOrigin` (in local units) with `localRayDir` (in world units) and `GIZMO_INTERACTION_THRESHOLD` (in local units), the math produces incorrect results.

**Location:** [Flux_Gizmos.cpp:589-601](Flux_Gizmos.cpp#L589-L601)

**Fix:** Do ALL calculations in world space by scaling the thresholds instead:
```cpp
// FIXED - All in world space
Zenith_Maths::Vector3 relativeRayOrigin = rayOrigin - xGizmoPos;  // World units
float worldArrowLength = GIZMO_ARROW_LENGTH * s_fGizmoScale;       // Scale to world
float worldInteractionThreshold = GIZMO_INTERACTION_THRESHOLD * s_fGizmoScale;
```

**Lesson:** Never mix coordinate spaces in intersection math. Either transform everything to local space OR scale constants to world space.

---

### Bug #3: ApplyTranslation Sign Error (CRITICAL)

**Symptom:** Objects move in the wrong direction, often opposite to mouse drag

**Root Cause:** The `w` vector in the line-line closest point formula had the wrong sign:

```cpp
// BROKEN - Sign is inverted!
Zenith_Maths::Vector3 w = rayOrigin - s_xInitialEntityPosition;  // P2 - P1 (WRONG!)
```

The standard formula requires `w = P1 - P2` (axis origin minus ray origin). Getting this backwards causes `t_current` to have the wrong sign.

**Location:** [Flux_Gizmos.cpp:864](Flux_Gizmos.cpp#L864)

**Fix:**
```cpp
// FIXED - Correct sign
Zenith_Maths::Vector3 w = s_xInitialEntityPosition - rayOrigin;  // P1 - P2 (CORRECT!)
```

**Lesson:** Line-line closest point formulas are sign-sensitive. Always verify `w = Line1Origin - Line2Origin`.

---

### Bug #4: Y-Axis Inversion in Screen-to-World Ray

**Symptom:** Clicking top of screen hits bottom gizmos; dragging up moves objects down

**Root Cause:** Unnecessary Y-axis flip in `ScreenToWorldRay()`:

```cpp
// BROKEN - Flip was causing inverted Y interaction
y = -y;  // Invert Y for Vulkan
```

The projection matrix already handles coordinate system differences; the extra flip inverted everything.

**Location:** [Zenith_Gizmo.cpp:486](../../Editor/Zenith_Gizmo.cpp#L486)

**Fix:** Remove the flip:
```cpp
// FIXED - Projection matrix handles coordinate system
// Don't flip Y - causes inverted interaction
```

**Lesson:** Understand your projection matrix's coordinate conventions before adding manual flips.

---

### Bug #5: Interaction Length Multiplier Too Large

**Symptom:** False-positive hits when cursor is far from gizmo in screen space

**Root Cause:** `GIZMO_INTERACTION_LENGTH_MULTIPLIER = 10.0f` extended the interaction cylinder to 10x the visual arrow length.

**Location:** [Flux_Gizmos.cpp:25](Flux_Gizmos.cpp#L25)

**Fix:** Set multiplier to `1.0f` to match visual bounds.

**Lesson:** Debug visualization should match actual interaction bounds. Use the wireframe debug cubes to verify.

---

### Bug #6: Rotation Gizmo Circles Not Rendering (December 2024)

**Symptom:** Rotation mode doesn't render any visible gizmos (though interaction still works)

**Root Cause:** `GenerateCircleGeometry()` generated degenerate triangles for line rendering, but the pipeline uses `MESH_TOPOLOGY_TRIANGLES`:

```cpp
// BROKEN - Degenerate triangles won't render
for (uint32_t i = 0; i < GIZMO_CIRCLE_SEGMENTS; ++i)
{
    indices.PushBack(i);
    indices.PushBack((i + 1) % GIZMO_CIRCLE_SEGMENTS);
    indices.PushBack(i);  // Degenerate triangle - THIS WON'T RENDER
}
```

The comment "will need line topology" indicated the issue, but the circles were never converted to proper geometry.

**Location:** [Flux_Gizmos.cpp:441-501](Flux_Gizmos.cpp#L441-L501)

**Fix:** Convert circles to 3D tube/ribbon geometry with actual triangle quads:

```cpp
// FIXED: Generate circle as a 3D tube/ribbon with actual triangle geometry
const float tubeThickness = 0.02f;  // Thickness of the tube in local space

for (uint32_t i = 0; i < GIZMO_CIRCLE_SEGMENTS; ++i)
{
    float angle = (float)i / GIZMO_CIRCLE_SEGMENTS * 2.0f * 3.14159f;

    Vector3 circlePos = tangent * cosf(angle) * GIZMO_CIRCLE_RADIUS + bitangent * sinf(angle) * GIZMO_CIRCLE_RADIUS;
    Vector3 radialDir = normalize(circlePos);

    // Create inner and outer vertices for tube
    positions.PushBack(circlePos - radialDir * tubeThickness);
    positions.PushBack(circlePos + radialDir * tubeThickness);
}

// Generate quad indices (two triangles per segment)
for (uint32_t i = 0; i < GIZMO_CIRCLE_SEGMENTS; ++i)
{
    uint32_t baseIdx = i * 2;
    uint32_t nextBaseIdx = ((i + 1) % GIZMO_CIRCLE_SEGMENTS) * 2;

    // First triangle of quad
    indices.PushBack(baseIdx);          // Inner current
    indices.PushBack(baseIdx + 1);      // Outer current
    indices.PushBack(nextBaseIdx);      // Inner next

    // Second triangle of quad
    indices.PushBack(baseIdx + 1);      // Outer current
    indices.PushBack(nextBaseIdx + 1);  // Outer next
    indices.PushBack(nextBaseIdx);      // Inner next
}
```

**Lesson:** Always verify topology matches pipeline configuration. Degenerate triangles don't render with standard triangle topology.

---

### Bug #7: Scale Manipulation Too Aggressive (December 2024)

**Symptom:** Scaling objects makes them grow/shrink by extreme amounts with tiny mouse movements

**Root Cause:** `ApplyScale()` calculated scale factor based on camera-to-entity distance instead of axis projection:

```cpp
// BROKEN - Uses camera distance, not axis projection!
Zenith_Maths::Vector3 toRayOrigin = rayOrigin - s_xInitialEntityPosition;
float initialDist = glm::length(s_xInteractionStartPos - s_xInitialEntityPosition);
float currentDist = glm::length(toRayOrigin + rayDir * glm::dot(rayDir, toRayOrigin));
float scaleFactor = currentDist / (initialDist + 0.0001f);
```

This causes the scale to change dramatically with any mouse movement because the camera distance varies widely.

**Location:** [Flux_Gizmos.cpp:799-827](Flux_Gizmos.cpp#L799-L827)

**Fix:** Use the same line-line closest point algorithm as translation, then convert offset to scale factor:

```cpp
// FIXED: Use axis projection like translation does
Vector3 axis = GetAxisForComponent(s_eActiveComponent);

// For uniform scale, use camera view direction as constraint
if (bUniformScale) {
    Vector3 cameraPos = Flux_Graphics::GetCameraPosition();
    axis = normalize(s_xInitialEntityPosition - cameraPos);
}

// Line-line closest point (same as translation)
Vector3 offsetToClick = s_xInteractionStartPos - s_xInitialEntityPosition;
float t_initial = dot(offsetToClick, axis);

Vector3 w = s_xInitialEntityPosition - rayOrigin;
float a = 1.0f;
float b = dot(axis, rayDir);
float c = dot(rayDir, rayDir);
float d = dot(axis, w);
float e = dot(rayDir, w);
float denom = a*c - b*b;

if (abs(denom) < 0.0001f) return;

float t_current = (b*e - c*d) / denom;

// Convert delta to scale factor
float delta_t = t_current - t_initial;
const float scaleSpeed = 0.5f;  // Adjust for sensitivity
float scaleFactor = 1.0f + (delta_t * scaleSpeed);

// Clamp to prevent negative scale
scaleFactor = max(scaleFactor, 0.01f);
```

**Tuning:** Adjust `scaleSpeed` constant for desired sensitivity (0.5 = moderate, 1.0 = fast, 0.2 = slow).

**Lesson:** Reuse proven algorithms. Translation and scale both need axis projection; don't reinvent the math.

---

## Architecture

### System Overview

```
┌─────────────────────────────────────────────────────────────────────┐
│                       Zenith_Editor                                  │
│  ┌──────────────────────────────────────────────────────────────┐   │
│  │ HandleGizmoInteraction()                                      │   │
│  │   1. Get mouse position → viewport-relative coordinates       │   │
│  │   2. ScreenToWorldRay() → ray origin + direction              │   │
│  │   3. On mouse down: BeginInteraction(ray)                     │   │
│  │   4. While dragging: UpdateInteraction(ray)                   │   │
│  │   5. On mouse up: EndInteraction()                            │   │
│  └──────────────────────────────────────────────────────────────┘   │
│                                ↓                                     │
│  ┌──────────────────────────────────────────────────────────────┐   │
│  │ RenderGizmos()                                                │   │
│  │   - SetTargetEntity() (only when not interacting!)            │   │
│  │   - SetGizmoMode()                                            │   │
│  │   - SubmitRenderTask()                                        │   │
│  └──────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────┘
                                 ↓
┌─────────────────────────────────────────────────────────────────────┐
│                       Flux_Gizmos                                    │
│  ┌──────────────────────────────────────────────────────────────┐   │
│  │ BeginInteraction(rayOrigin, rayDir)                           │   │
│  │   1. RaycastGizmo() → which component was clicked             │   │
│  │   2. Store: s_xInitialEntityPosition, s_xInteractionStartPos  │   │
│  │   3. Set: s_bIsInteracting = true, s_eActiveComponent         │   │
│  └──────────────────────────────────────────────────────────────┘   │
│  ┌──────────────────────────────────────────────────────────────┐   │
│  │ UpdateInteraction(rayOrigin, rayDir)                          │   │
│  │   - ApplyTranslation() / ApplyRotation() / ApplyScale()       │   │
│  │   - Modifies entity's TransformComponent                      │   │
│  └──────────────────────────────────────────────────────────────┘   │
│  ┌──────────────────────────────────────────────────────────────┐   │
│  │ Render()                                                      │   │
│  │   - Calculate gizmo scale from camera distance                │   │
│  │   - Build model matrix: translate(entityPos) * scale(gizmoScale)│
│  │   - Submit draw commands for gizmo geometry                   │   │
│  └──────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────┘
```

### Interaction State Machine

```
                    ┌──────────────┐
                    │     IDLE     │
                    │ s_bIsInteracting = false
                    │ s_eActiveComponent = None
                    └──────┬───────┘
                           │
                           │ Mouse Down + RaycastGizmo() hits
                           ↓
                    ┌──────────────┐
                    │  INTERACTING │
                    │ s_bIsInteracting = true
                    │ s_eActiveComponent = X/Y/Z
                    │ s_xInitialEntityPosition stored
                    │ s_xInteractionStartPos stored
                    └──────┬───────┘
                           │
           ┌───────────────┼───────────────┐
           │               │               │
           ↓               ↓               ↓
    UpdateInteraction  UpdateInteraction  Mouse Up
    (every frame)      (every frame)          │
           │               │               │
           └───────────────┴───────────────┘
                           │
                           ↓
                    ┌──────────────┐
                    │     IDLE     │
                    └──────────────┘
```

**CRITICAL:** `SetTargetEntity()` and `SetGizmoMode()` both set `s_bIsInteracting = false`. Never call them during active interaction!

---

## Entity Selection → Gizmo Flow

Before gizmos appear, an entity must be selected:

### 1. Object Picking (HandleObjectPicking)

```cpp
// Get mouse position relative to viewport
Zenith_Maths::Vector2 xViewportMousePos = {
    static_cast<float>(xGlobalMousePos.x - s_xViewportPos.x),
    static_cast<float>(xGlobalMousePos.y - s_xViewportPos.y)
};

// Convert to world ray
Zenith_Maths::Vector3 xRayDir = Zenith_Gizmo::ScreenToWorldRay(
    xViewportMousePos, {0, 0}, s_xViewportSize, xViewMatrix, xProjMatrix
);

// Raycast against scene entities
Zenith_EntityID uHitEntityID = Zenith_SelectionSystem::RaycastSelect(xRayOrigin, xRayDir);

// Store selection (by ID, not pointer!)
SelectEntity(uHitEntityID);
```

### 2. Selection Stored

```cpp
static Zenith_EntityID s_uSelectedEntityID = INVALID_ENTITY_ID;

Zenith_Entity* GetSelectedEntity() {
    if (s_uSelectedEntityID == INVALID_ENTITY_ID)
        return nullptr;

    auto it = xScene.m_xEntityMap.find(s_uSelectedEntityID);
    if (it == xScene.m_xEntityMap.end()) {
        s_uSelectedEntityID = INVALID_ENTITY_ID;  // Entity no longer exists
        return nullptr;
    }
    return &it->second;
}
```

### 3. Gizmo Target Set

```cpp
void RenderGizmos() {
    Zenith_Entity* pxSelectedEntity = GetSelectedEntity();

    // CRITICAL: Only when not interacting!
    if (!Flux_Gizmos::IsInteracting()) {
        Flux_Gizmos::SetTargetEntity(pxSelectedEntity);
    }
}
```

---

## Screen-to-World Ray Mathematics

### The Complete Pipeline

```
Screen Space (pixels)
    ↓  mousePos = (px_x, px_y)
    ↓
Viewport Space (normalized)
    ↓  normalized = mousePos / viewportSize → [0, 1]
    ↓
NDC (Normalized Device Coordinates)
    ↓  ndc = normalized * 2 - 1 → [-1, 1]
    ↓
Clip Space
    ↓  clip = (ndc.x, ndc.y, 0, 1)  // 0 = near plane for Vulkan
    ↓
View Space (Eye Space)
    ↓  eye = inverse(projMatrix) * clip
    ↓  eye.xyz /= eye.w  // Perspective divide
    ↓
World Space
    ↓  world = inverse(viewMatrix) * (eye.xyz, 0)  // w=0 for direction
    ↓  rayDir = normalize(world.xyz)
```

### Implementation

```cpp
Zenith_Maths::Vector3 Zenith_Gizmo::ScreenToWorldRay(
    const Zenith_Maths::Vector2& mousePos,
    const Zenith_Maths::Vector2& viewportPos,  // Usually (0,0) for relative coords
    const Zenith_Maths::Vector2& viewportSize,
    const Zenith_Maths::Matrix4& viewMatrix,
    const Zenith_Maths::Matrix4& projMatrix)
{
    // STEP 1: Screen → NDC
    float x = (mousePos.x / viewportSize.x) * 2.0f - 1.0f;
    float y = (mousePos.y / viewportSize.y) * 2.0f - 1.0f;

    // NOTE: Do NOT flip Y here - projection matrix handles coordinate system
    // (Flipping Y was a bug that caused inverted interaction)

    // STEP 2: Clip space point on near plane
    // For Vulkan: depth = 0 is near plane, depth = 1 is far plane
    Zenith_Maths::Vector4 rayClip(x, y, 0.0f, 1.0f);

    // STEP 3: Unproject to view space
    Zenith_Maths::Matrix4 invProj = glm::inverse(projMatrix);
    Zenith_Maths::Vector4 rayEye = invProj * rayClip;

    // Perspective divide
    rayEye.x /= rayEye.w;
    rayEye.y /= rayEye.w;
    rayEye.z /= rayEye.w;
    rayEye.w = 0.0f;  // Convert to direction (w=0 means no translation)

    // STEP 4: Transform direction to world space
    Zenith_Maths::Matrix4 invView = glm::inverse(viewMatrix);
    Zenith_Maths::Vector4 rayWorld = invView * rayEye;

    // STEP 5: Normalize
    return glm::normalize(Zenith_Maths::Vector3(rayWorld.x, rayWorld.y, rayWorld.z));
}
```

### Coordinate System Reference

| Space | Origin | X | Y | Z | Range |
|-------|--------|---|---|---|-------|
| Screen | Top-left | Right | Down | - | [0, viewport] |
| NDC | Center | Right | Up* | Into screen | [-1, 1] |
| View | Camera | Right | Up | Forward | World units |
| World | World origin | Engine convention | Engine convention | Engine convention | World units |

*Note: NDC Y direction depends on projection matrix conventions. Vulkan uses top-left origin by default.

---

## Ray-Gizmo Intersection Mathematics

### Ray-Cylinder Intersection (Arrow Axes)

The translation/scale gizmo arrows are tested as finite cylinders along each axis.

**Problem:** Find where ray `P = O + t*D` intersects cylinder of radius `r` along axis `A`.

**Setup:**
```
Ray:      P(t) = rayOrigin + t * rayDir
Cylinder: |P - (P·A)*A|² = r²,  where 0 ≤ P·A ≤ length
```

**Derivation:**

The distance from point P to the axis is:
```
distToAxis = |P - (P·A)*A|
```

Substituting the ray equation and squaring:
```
|(O + t*D) - ((O + t*D)·A)*A|² = r²
```

Let:
- `ao = rayOrigin` (relative to gizmo center)
- `dotAxisDir = A · D`
- `dotAxisOrigin = A · ao`

This expands to quadratic form `at² + bt + c = 0`:
```cpp
float a = dot(D, D) - dotAxisDir * dotAxisDir;
float b = 2.0f * (dot(D, ao) - dotAxisDir * dotAxisOrigin);
float c = dot(ao, ao) - dotAxisOrigin * dotAxisOrigin - radius * radius;

float discriminant = b*b - 4*a*c;
if (discriminant < 0) return false;  // No intersection

float t = (-b - sqrt(discriminant)) / (2*a);  // Closer hit
```

**Bounds Check:**

After finding `t`, verify hit point is within arrow length:
```cpp
Vector3 hitPoint = rayOrigin + rayDir * t;
float alongAxis = dot(hitPoint, axis);

if (alongAxis >= 0.0f && alongAxis <= arrowLength)
    return true;  // Valid hit
```

### Ray-Circle Intersection (Rotation Rings)

Rotation gizmo rings are circles (torus cross-sections) in planes perpendicular to each axis.

**Problem:** Find where ray hits the circle of radius `R` in plane with normal `N`.

**Step 1: Ray-Plane Intersection**
```cpp
float denom = dot(N, D);
if (abs(denom) < 0.0001f) return false;  // Ray parallel to plane

float t = -dot(N, rayOrigin) / denom;
if (t < 0) return false;  // Behind camera

Vector3 hitPoint = rayOrigin + rayDir * t;
```

**Step 2: Check Distance to Circle**
```cpp
float distFromCenter = length(hitPoint);

// Hit if we're within threshold of the circle's radius
if (abs(distFromCenter - circleRadius) < threshold)
    return true;
```

### Ray-AABB Intersection (Scale Cube)

The center cube for uniform scaling uses slab-based AABB intersection.

```cpp
Vector3 invDir = 1.0f / rayDir;
Vector3 t0 = (boxMin - rayOrigin) * invDir;
Vector3 t1 = (boxMax - rayOrigin) * invDir;

Vector3 tmin = min(t0, t1);
Vector3 tmax = max(t0, t1);

float tNear = max(max(tmin.x, tmin.y), tmin.z);
float tFar = min(min(tmax.x, tmax.y), tmax.z);

if (tNear <= tFar && tFar >= 0)
    return true;
```

---

## Translation Manipulation Mathematics

The translation gizmo uses **line-line closest point** to project mouse movement onto the constraint axis.

### Problem Statement

Given:
- **Axis line:** `A(t) = entityPos + t * axis` (the constraint axis)
- **Ray line:** `R(s) = rayOrigin + s * rayDir` (current mouse ray)
- **Initial click point:** `s_xInteractionStartPos` (where user first clicked)

Find: How far along the axis to move the entity.

### Line-Line Closest Point Formula

The closest points between two lines minimizes |A(t) - R(s)|².

**Standard notation:**
```
Line 1: P(t) = P1 + t * u    (Axis: entityPos, axis)
Line 2: Q(s) = P2 + s * v    (Ray: rayOrigin, rayDir)

w = P1 - P2                  (CRITICAL: Order matters!)
a = u · u = 1                (axis is unit vector)
b = u · v = axis · rayDir
c = v · v = rayDir · rayDir
d = u · w = axis · w
e = v · w = rayDir · w
```

**Solution for t (position along axis):**
```
denom = a*c - b*b

t = (b*e - c*d) / denom
```

**Implementation:**
```cpp
void Flux_Gizmos::ApplyTranslation(const Vector3& rayOrigin, const Vector3& rayDir)
{
    // Get constraint axis
    Vector3 axis = GetAxisForComponent(s_eActiveComponent);  // (1,0,0), (0,1,0), or (0,0,1)

    // Initial offset: how far along axis did user click?
    Vector3 offsetToClick = s_xInteractionStartPos - s_xInitialEntityPosition;
    float t_initial = dot(offsetToClick, axis);

    // Line-line closest point
    // CRITICAL: w = AxisOrigin - RayOrigin (P1 - P2)
    Vector3 w = s_xInitialEntityPosition - rayOrigin;  // NOT rayOrigin - entityPos!

    float a = 1.0f;                    // axis · axis = 1 (unit vector)
    float b = dot(axis, rayDir);
    float c = dot(rayDir, rayDir);    // ≈1 if normalized
    float d = dot(axis, w);
    float e = dot(rayDir, w);

    float denom = a*c - b*b;

    if (abs(denom) < 0.0001f)
        return;  // Ray parallel to axis

    float t_current = (b*e - c*d) / denom;

    // Move entity by the difference
    float delta_t = t_current - t_initial;
    Vector3 newPosition = s_xInitialEntityPosition + axis * delta_t;

    entity.SetPosition(newPosition);
}
```

### Why the Sign of w Matters

With `w = s_xInitialEntityPosition - rayOrigin` (correct):
- Moving mouse right increases `e` (rayDir · w component)
- This increases `t_current`
- Entity moves in positive axis direction ✓

With `w = rayOrigin - s_xInitialEntityPosition` (WRONG):
- The signs of `d` and `e` are inverted
- `t_current = (b*(-e) - c*(-d)) / denom = -(b*e - c*d) / denom`
- Entity moves in OPPOSITE direction ✗

---

## Scale Manipulation Mathematics

The scale gizmo uses the **same line-line closest point** algorithm as translation, but converts the axis offset to a scale multiplier.

### Problem Statement

Given:
- **Axis line:** `A(t) = entityPos + t * axis` (the constraint axis)
- **Ray line:** `R(s) = rayOrigin + s * rayDir` (current mouse ray)
- **Initial scale:** `s_xInitialEntityScale` (scale when drag started)

Find: The new scale factor to apply.

### Implementation

```cpp
void Flux_Gizmos::ApplyScale(const Vector3& rayOrigin, const Vector3& rayDir)
{
    // Get constraint axis
    Vector3 axis = GetAxisForComponent(s_eActiveComponent);  // (1,0,0), (0,1,0), or (0,0,1)

    // Special case: Uniform scale uses camera view direction
    bool bUniformScale = (s_eActiveComponent == GizmoComponent::ScaleXYZ);
    if (bUniformScale) {
        Vector3 cameraPos = Flux_Graphics::GetCameraPosition();
        axis = normalize(s_xInitialEntityPosition - cameraPos);
    }

    // Initial offset: how far along axis did user click?
    Vector3 offsetToClick = s_xInteractionStartPos - s_xInitialEntityPosition;
    float t_initial = dot(offsetToClick, axis);

    // Line-line closest point (identical to translation)
    Vector3 w = s_xInitialEntityPosition - rayOrigin;

    float a = 1.0f;
    float b = dot(axis, rayDir);
    float c = dot(rayDir, rayDir);
    float d = dot(axis, w);
    float e = dot(rayDir, w);
    float denom = a*c - b*b;

    if (abs(denom) < 0.0001f)
        return;  // Ray parallel to axis

    float t_current = (b*e - c*d) / denom;

    // Convert offset to scale factor
    float delta_t = t_current - t_initial;
    const float scaleSpeed = 0.5f;  // Tunable sensitivity
    float scaleFactor = 1.0f + (delta_t * scaleSpeed);

    // Prevent negative/zero scale
    scaleFactor = max(scaleFactor, 0.01f);

    // Apply per-axis or uniform
    Vector3 newScale = s_xInitialEntityScale;
    if (bUniformScale) {
        newScale *= scaleFactor;
    } else {
        newScale[axisIndex] *= scaleFactor;
    }

    entity.SetScale(newScale);
}
```

### Scale Speed Tuning

The `scaleSpeed` constant controls how much scale changes per unit of mouse movement:

| scaleSpeed | Behavior | Best For |
|------------|----------|----------|
| 0.2 | Slow, precise | Fine-tuning, architectural work |
| 0.5 | Moderate (default) | General use |
| 1.0 | Fast | Large objects, quick iteration |
| 2.0+ | Very fast | Not recommended - too sensitive |

**Formula:** `scaleFactor = 1.0 + (delta_t * scaleSpeed)`

Example:
- Mouse moves 1.0 units along axis
- `delta_t = 1.0`
- With `scaleSpeed = 0.5`: `scaleFactor = 1.0 + (1.0 * 0.5) = 1.5` → 50% larger
- With `scaleSpeed = 1.0`: `scaleFactor = 1.0 + (1.0 * 1.0) = 2.0` → 100% larger (2x)

### Uniform Scale Behavior

When dragging the center cube for uniform scaling:
- Uses camera-to-entity direction as the "constraint axis"
- Allows scaling toward/away from camera
- All three axes (X, Y, Z) scale by the same factor

---

## Gizmo Rendering

### Geometry Generation

Gizmo geometry is generated procedurally at initialization:

**Arrow (Translation/Scale):**
```
Shaft:  8-segment cylinder from origin along axis
Head:   Cone at end of shaft
Total:  ~100 triangles per arrow
```

**Circle (Rotation):**
```
Ring:   64-segment circle in plane perpendicular to axis
Tube:   3D ribbon/tube with inner and outer vertices (tubeThickness = 0.02)
        Each segment creates 2 vertices (inner/outer)
        Each quad between segments = 2 triangles
Total:  128 triangles per ring (64 segments × 2 triangles)
```

**Cube (Uniform Scale):**
```
Standard cube centered at origin
6 faces, 12 triangles
```

### Pipeline Configuration

```cpp
Flux_PipelineSpecification xSpec;
xSpec.m_bDepthTestEnabled = false;   // Always on top
xSpec.m_bDepthWriteEnabled = false;
xSpec.m_axBlendStates[0].m_bBlendEnabled = true;  // For transparency
```

### Auto-Scaling

Gizmos maintain constant screen-space size:

```cpp
float distance = length(entityPos - cameraPos);
float gizmoScale = distance / GIZMO_AUTO_SCALE_DISTANCE;  // 5.0 = reference distance

Matrix4 gizmoMatrix = translate(entityPos) * scale(gizmoScale);
```

At `GIZMO_AUTO_SCALE_DISTANCE` units from camera, gizmo has scale 1.0.

---

## Constants Reference

| Constant | Value | Description |
|----------|-------|-------------|
| `GIZMO_ARROW_LENGTH` | 1.2 | Length of arrow in local units |
| `GIZMO_ARROW_HEAD_LENGTH` | 0.3 | Cone head length |
| `GIZMO_ARROW_HEAD_RADIUS` | 0.1 | Cone head radius |
| `GIZMO_ARROW_SHAFT_RADIUS` | 0.03 | Cylinder shaft radius |
| `GIZMO_CIRCLE_RADIUS` | 1.0 | Rotation ring radius |
| `GIZMO_CUBE_SIZE` | 0.15 | Uniform scale cube size |
| `GIZMO_INTERACTION_THRESHOLD` | 0.2 | Ray-cylinder hit tolerance |
| `GIZMO_INTERACTION_LENGTH_MULTIPLIER` | 1.0 | Interaction bounds multiplier (was 10.0 - bug!) |
| `GIZMO_AUTO_SCALE_DISTANCE` | 5.0 | Distance for scale=1.0 |

**Tuning Tips:**
- Increase `INTERACTION_THRESHOLD` if clicking is too precise
- Increase `INTERACTION_LENGTH_MULTIPLIER` slightly (1.3-1.5) if clicking endpoints is hard
- Never set `INTERACTION_LENGTH_MULTIPLIER` above 2.0 - causes false positives

---

## Debug Features

### Debug Visualization

The system renders wireframe cubes showing interaction bounds:
```cpp
Flux_Primitives::AddWireframeCube(
    xEntityPos + axis * GIZMO_ARROW_LENGTH * 0.5f * s_fGizmoScale,
    Vector3(length/2, threshold, threshold) * s_fGizmoScale,
    axisColor
);
```

### Debug Variables (ZENITH_TOOLS)

- `Editor/Gizmos/Render` - Toggle gizmo rendering
- `Editor/Gizmos/Alpha` - Adjust transparency

### Logging

Key functions log debug info:
```cpp
Zenith_Log("RaycastGizmo: GizmoPos=(%.1f,%.1f,%.1f), Scale=%.2f", ...);
Zenith_Log("ApplyTranslation: t_initial=%.4f, t_current=%.4f, delta_t=%.4f", ...);
```

---

## Common Issues & Debugging

### "Gizmo not responding to clicks"

1. **Check `s_bIsInteracting`** - Is it being reset unexpectedly?
2. **Check raycast** - Log ray origin/direction, verify they're sane
3. **Check coordinate spaces** - Are all values in world space?
4. **Check thresholds** - Is `GIZMO_INTERACTION_THRESHOLD` too small?

### "Object moves wrong direction"

1. **Check `w` sign** in ApplyTranslation - Must be `entityPos - rayOrigin`
2. **Check Y-flip** in ScreenToWorldRay - Should NOT flip Y
3. **Check axis vector** - Verify it's the correct axis for the component

### "False positive hits far from gizmo"

1. **Check `GIZMO_INTERACTION_LENGTH_MULTIPLIER`** - Should be 1.0
2. **Check debug visualization** - Do wireframe boxes match visual gizmo?
3. **Check coordinate space** - Is ray in world space, not local?

### "Dragging stops after starting"

1. **Check `RenderGizmos()`** - Is it calling `SetTargetEntity()` during drag?
2. **Check mode changes** - Is gizmo mode changing during drag?
3. **Check input handling** - Is `IsKeyDown()` returning false unexpectedly?

---

## Integration Checklist

When integrating gizmos into a new editor:

- [ ] Initialize: `Flux_Gizmos::Initialise()` at startup
- [ ] Set target: `SetTargetEntity()` only when NOT interacting
- [ ] Set mode: `SetGizmoMode()` only when NOT interacting
- [ ] Handle input: `BeginInteraction()` on mouse down, `UpdateInteraction()` while held, `EndInteraction()` on release
- [ ] Submit render: `SubmitRenderTask()` every frame
- [ ] Wait for render: `WaitForRenderTask()` before present
- [ ] Shutdown: `Flux_Gizmos::Shutdown()` on exit

---

## References

- **Entity-Component System:** See [Zenith/EntityComponent/CLAUDE.md](../../EntityComponent/CLAUDE.md)
- **Editor Architecture:** See [Zenith/Editor/CLAUDE.md](../../Editor/CLAUDE.md)
- **Main Engine Docs:** See [CLAUDE.md](../../../CLAUDE.md)
- **Line-Line Closest Point:** [Geometric Tools - Distance Between Lines](https://www.geometrictools.com/Documentation/DistanceLine3Line3.pdf)

---

*Last Updated: 2025*
*For Zenith Engine - Flux Gizmos System*
