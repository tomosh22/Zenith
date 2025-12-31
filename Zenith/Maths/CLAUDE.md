# Maths

GLM wrapper with engine-specific extensions.

## Files

- `Zenith_Maths.h/cpp` - Type aliases, wrapper functions
- `Zenith_Maths_Intersections.h` - Ray intersection tests
- `Zenith_FrustumCulling.h` - Frustum and AABB utilities

## GLM Configuration

Engine configures GLM before including headers:
- `GLM_ENABLE_EXPERIMENTAL` - Enables experimental features
- `GLM_FORCE_DEPTH_ZERO_TO_ONE` - Vulkan depth range [0, 1] instead of OpenGL's [-1, 1]
- `GLM_FORCE_LEFT_HANDED` - Left-handed coordinate system

## Type System

All types in `Zenith_Maths` namespace as aliases to GLM types:
- Vectors: `Vector2/3/4`, `UVector2/4` (unsigned), double-precision variants with `_64` suffix
- Matrices: `Matrix2/3/4`, double-precision variants with `_64` suffix
- Quaternions: `Quat` and `Quaternion` (aliases)

Direct GLM aliasing means zero overhead - types are identical to underlying GLM types.

## Wrapper Functions

Static helper functions avoid namespace verbosity. Covers common operations:
- Projection: `PerspectiveProjection()`, `OrthographicProjection()`
- Vector math: `Normalize()`, `Length()`, `Dot()`, `Cross()`
- Matrix transforms: `Translate()`, `Scale()`, `Rotate()`
- Quaternion operations: `AngleAxis()`, `Mat4Cast()`, `QuatCast()`, `QuatLookAt()`

## Intersection Tests

In `Zenith_Maths_Intersections.h`. Static functions for:
- `RayIntersectsCircle()` - Ray-plane intersection with circular boundary
- `RayIntersectsAABB()` - Slab method for axis-aligned bounding boxes
- `RayIntersectsCylinder()` - Finite cylinder with quadratic solution

Used for gizmo picking, entity selection, and collision detection.

## Frustum Culling

In `Zenith_FrustumCulling.h`.

### Data Structures
- `Zenith_AABB` - Min/max corners with utility methods
- `Zenith_Plane` - Normal and distance from origin
- `Zenith_Frustum` - Six planes (left, right, bottom, top, near, far)

### Functions
- `TestAABBFrustum()` - P-vertex/N-vertex culling algorithm (conservative, no false negatives)
- `GenerateAABBFromVertices()` - Compute tight AABB from vertex positions
- `TransformAABB()` - Transform AABB by matrix, recalculate axis-aligned bounds
- `Zenith_Frustum::ExtractFromViewProjection()` - Gribb-Hartmann method to extract planes from matrix

## Key Concepts

**Zero Overhead:** Type aliasing provides convenience without runtime cost. GLM functions directly accessible.

**Vulkan-Specific:** Configuration matches Vulkan conventions (depth, handedness) preventing coordinate space bugs.

**Conservative Culling:** Frustum culling uses conservative approach - objects on boundary treated as visible, preventing incorrect culling.

**Left-Handed:** Positive Z forward, positive Y up. Consistent with DirectX convention.
