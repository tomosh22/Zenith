# Physics System

## Files

- `Zenith_Physics.h/cpp` - Physics manager, Jolt integration, collision events
- `Zenith_Physics_Fwd.h` - Jolt forward declarations, physics enums (`CollisionVolumeType`, `CollisionEventType`, `RigidBodyType`), and the `Zenith_PhysicsBodyID` value type
- `Zenith_PhysicsMeshGenerator.h/cpp` - Collision mesh generation from render meshes
- `Zenith_PhysicsWorldHooks.h/cpp` - Leaf-safe runtime hook fired on out-of-band body pose changes (teleport/upright/lock-rotation) so the engine can invalidate the owning entity's cached transform; null-safe no-op when unwired
- `Internal/Zenith_PhysicsPCH.cpp` - Precompiled-header translation unit (internal)

## Overview

Integration with Jolt Physics library for rigid body dynamics. Features fixed 60 Hz timestep, multi-threaded simulation, and deferred collision event processing for thread safety.

## Jolt Integration

Uses Jolt Physics types internally:
- `JPH::PhysicsSystem` - Main simulation engine (private member; engine-internal code reaches it via `GetJoltSystem()`)
- `JPH::Body` - Rigid body (stored in `Zenith_ColliderComponent`)
- `JPH::TempAllocatorImpl` - Per-frame scratch memory (10MB)
- `JPH::JobSystemThreadPool` - Multi-threaded physics jobs (uses hardware_concurrency - 1 threads)

Game-facing code never names `JPH::` types. Bodies are identified by
`Zenith_PhysicsBodyID` (`Physics/Zenith_Physics_Fwd.h`), a value type mirroring
`JPH::BodyID`'s uint32 representation: `Zenith_ColliderComponent::GetBodyID()`
returns one, and every `Zenith_Physics` body method takes one
(velocity/force/impulse/friction/restitution/sensor/lock-rotation/teleport).
`Zenith_Physics.h` itself only includes Jolt's `ContactListener.h` (for the
by-value listener member); the heavy Jolt headers live in the `.cpp`s.

## Zenith_Physics Class

Static manager for physics simulation. Key responsibilities:
- Initialize/shutdown Jolt engine
- Update physics with fixed 60 Hz timestep (uses frame time accumulator)
- Manage body lifecycle
- Process deferred collision events on main thread
- Raycast functionality for camera/editor interaction. Two public `Raycast(...)` overloads return a `RaycastResult` (`m_bHit`, `m_xHitPoint`, `m_xHitNormal`, `m_fDistance`, `m_xHitEntity`); the second overload takes a `Zenith_PhysicsBodyID` to ignore. The EntityID-ignore convenience form lives engine-side in `Zenith_PhysicsQuery::RaycastIgnoring`.
- Memory diagnostics: `GetJoltMemoryAllocated()` / `GetJoltAllocationCount()` report Jolt allocator usage for diagnostics

## Collision System

### Layers
Two object layers: `NON_MOVING` (static) and `MOVING` (dynamic). Static objects only collide with dynamic objects. Used for broadphase optimization.

### Event Handling
Contact callbacks executed on worker threads. Events queued as `DeferredCollisionEvent` structs, processed on main thread during `Update()`. Dispatched through the component-meta registry to any component implementing `OnCollisionEnter(Zenith_Entity)`, `OnCollisionStay(Zenith_Entity)`, or `OnCollisionExit(Zenith_EntityID)` (concept-detected).

## Physics Mesh Generation

### Quality Levels
- **LOW** - AABB (12 triangles, fastest)
- **MEDIUM** - Vertex decimation (~60% retention) for a convex approximation. Extreme points are computed for validation/fallback, but the mesh is built by decimating all vertices via `GenerateSimplifiedMesh` (simplification ratio 0.6), not from the extreme points themselves.
- **HIGH** - Simplified triangle mesh with vertex decimation

### Algorithm
Vertex decimation uses spatial hashing. Extreme vertices (min/max on each axis) preserved to maintain bounding volume. Configurable via `PhysicsMeshConfig` including simplification ratio, min/max triangle counts, auto-generation flag, and debug visualization.

### Debug Visualization
`PhysicsMeshConfig::m_xDebugColor` (default green) is available for debug-mesh tinting, but the Physics module itself does no rendering — it is a renderer-neutral leaf and exposes no wireframe overlay. Any visualization is the engine/editor side's responsibility.

## Key Concepts

**Fixed Timestep:** Physics simulates at constant 60 Hz regardless of frame rate. Time accumulator ensures deterministic behavior.

**Thread Safety:** Collision events deferred from worker threads to main thread via mutex-protected queue.

**Gravity:** Per-body gravity control via `SetGravityEnabled()`. Default gravity is -9.81 m/s² on Y-axis (down in left-handed coordinates).

**Material Properties:** Per-body restitution (bounciness) and friction via `SetRestitution()`/`GetRestitution()` and `SetFriction()`/`GetFriction()`. Restitution range 0.0 (no bounce) to 1.0 (perfectly elastic). Jolt combines restitution with `max(body1, body2)` and friction with `sqrt(body1 * body2)`.

**Body Limits:** Max 65536 bodies, 65536 body pairs, 10240 contact constraints. Configured via `static constexpr` constants in the `Zenith_Physics` class header (`Zenith_Physics.h`).

## Scene Load/Reset Integration

**CRITICAL:** In `Zenith_SceneSystem::LoadScene()` (SCENE_LOAD_SINGLE mode), the physics reset hook (`m_xRuntimeHooks.m_pfnResetPhysics()`, which calls `Zenith_Physics::Reset()`) must be called AFTER scene teardown (`UnloadAllNonPersistent()`).

**Why This Order Matters:**
- Scene reset destroys entities including their `Zenith_ColliderComponent`
- Collider destructors call `BodyInterface::DestroyBody()` to remove their physics bodies
- If physics is reset FIRST, the bodies no longer exist and Jolt asserts on destruction
- If physics is NOT reset at all, stale bodies from previous play sessions remain, causing invisible collisions

**Symptom of Missing Physics Reset:**
- First play works correctly
- After Stop→Play: entities blocked by invisible colliders from previous session
- Debug shows entities stopping at unexpected distances (e.g., 1.27 instead of expected 0.57)

## Quaternion Normalization

Jolt Physics requires quaternions to be normalized. Always call `glm::normalize()` on quaternions before passing them to `SetRotation()`, especially after using `glm::slerp()` which can produce slightly denormalized results.

**Symptom of Denormalized Quaternion:**
- Jolt assertion: `Quat.inl: (IsNormalized())`
- Usually occurs in `SetRotation()` or `SetPositionAndRotationInternal()`
