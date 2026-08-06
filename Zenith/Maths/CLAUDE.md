# Maths

GLM wrapper with engine-specific extensions.

## Files

- `Zenith_Maths.h/cpp` - Type aliases, wrapper functions
- `Zenith_Maths_Intersections.h` - Ray intersection tests
- `Zenith_FrustumCulling.h` - Frustum and AABB utilities
- `Zenith_Noise.h` - Deterministic integer-hash value noise (see below)

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
- Vector math: `Normalize()`, `Length()`, `LengthSq()` (squared length via self-dot), `Dot()`, `Cross()`
- Matrix transforms: `Translate()`, `Scale()`, `Rotate()`, `EulerRotationToMatrix4()` (angle-axis rotation matrix from degrees + axis, in `Zenith_Maths.cpp`)
- Quaternion operations: `AngleAxis()`, `Mat4Cast()`, `QuatCast()`, `QuatLookAt()`, `RotateVector()` (rotate a vector by a quaternion), `QuatFromEuler()` (quaternion from pitch/yaw/roll in radians)
- Generic: `Clamp()` (template clamp of a value to a `[min, max]` range)

### Constants
- `Pi` - `constexpr double` value of pi
- `RadToDeg` - `constexpr double` radians-to-degrees factor (`180/Pi`)

## Intersection Tests

In `Zenith_Maths_Intersections.h`, in the nested `Zenith_Maths::Intersections` namespace (not top-level `Zenith_Maths`). Static functions for:
- `RayIntersectsCircle()` - Ray-plane intersection with circular boundary
- `RayIntersectsAABB()` - Slab method for axis-aligned bounding boxes
- `RayIntersectsCylinder()` - Finite cylinder with quadratic solution

Used for gizmo picking, entity selection, and collision detection.

## Frustum Culling

In `Zenith_FrustumCulling.h`.

### Data Structures
- `Zenith_AABB` - Min/max corners with utility methods: `ExpandToInclude()`, `GetCenter()`, `GetExtents()`, `IsValid()` (min <= max on all axes), `Reset()` (back to empty/invalid for expansion)
- `Zenith_Plane` - Normal and distance from origin; `GetSignedDistance()` (+front/-behind), `Normalize()` (rescale normal+distance to unit length)
- `Zenith_Frustum` - Six planes (left, right, bottom, top, near, far)

### Functions
- `TestAABBFrustum()` - P-vertex/N-vertex culling algorithm (conservative, no false negatives)
- `GenerateAABBFromVertices()` - Compute tight AABB from vertex positions
- `TransformAABB()` - Transform AABB by matrix, recalculate axis-aligned bounds
- `Zenith_Frustum::ExtractFromViewProjection()` - Gribb-Hartmann method to extract planes from matrix

## Noise

In `Zenith_Noise.h`, header-only, in the `Zenith_TerrainNoise` namespace — **not**
`Zenith_Maths`, despite living here. It sits in Maths because it is runtime-shared:
the terrain editor's Noise brush, procedural generation and auto-splat jitter all
consume it, and so does the GPU grass.

- `HashUInt()` / `HashCoords()` - 32-bit avalanche hash; `HashCoords` mixes (x, y, seed)
- `HashToFloat01()` / `ValueAt()` - uniform `[0,1)` from a hash / from a lattice node
- `ValueNoise()` - smoothstep-interpolated 2D value noise, `[0,1]`
- `FBM()` / `RidgedFBM()` - fractal sums over `ValueNoise`, amplitude-normalized to
  `[0,1]`; octaves clamped to `[1,12]`. Ridged uses a squared tent for sharp crests
- `XorShift32` - small deterministic PRNG for droplet simulation (erosion)

**Determinism is load-bearing.** RenderTest regenerates its terrain from a fixed
seed and CI hash-compares the output across runs, and every grass blade is a pure
function of its lattice node. So: no `std::mt19937` (its stream is
implementation-defined across standard libraries) and no float-order ambiguity —
every value derives from integer hashing.

**This header is the SOURCE OF TRUTH for the GPU mirror**,
`Flux/Shaders/Common/Noise.slang` (`Flux_NoiseHashUInt` / `…HashCoords` / …), a
function-for-function transcription meant to be diffed against this file side by
side. The three integer entry points are **bit-exact** on both sides — pure u32
shift/xor/multiply plus one exactly-representable scale — so any decision keyed
off the hash is reproducible on the CPU for tests and tooling. `ValueNoise` /
`FBM` are **not** claimed bit-exact (floor, FMA contraction and float modes all
differ per target); never gate a CPU/GPU consistency check on those values.

## Key Concepts

**Zero Overhead:** Type aliasing provides convenience without runtime cost. GLM functions directly accessible.

**Vulkan-Specific:** Configuration matches Vulkan conventions (depth, handedness) preventing coordinate space bugs.

**Conservative Culling:** Frustum culling uses conservative approach - objects on boundary treated as visible, preventing incorrect culling.

**Left-Handed:** Positive Z forward, positive Y up. Consistent with DirectX convention.
