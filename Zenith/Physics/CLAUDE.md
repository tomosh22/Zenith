# Physics System

## Files

- `Zenith_Physics.h/cpp` - Physics manager, Jolt integration, collision events
- `Zenith_PhysicsMeshGenerator.h/cpp` - Collision mesh generation from render meshes

## Overview

Integration with Jolt Physics library for rigid body dynamics. Features fixed 60 Hz timestep, multi-threaded simulation, and deferred collision event processing for thread safety.

## Jolt Integration

Uses Jolt Physics types:
- `JPH::PhysicsSystem` - Main simulation engine
- `JPH::Body` - Rigid body (stored in `Zenith_ColliderComponent`)
- `JPH::BodyID` - Body identifier with sequence number
- `JPH::TempAllocatorImpl` - Per-frame scratch memory (10MB)
- `JPH::JobSystemThreadPool` - Multi-threaded physics jobs (uses hardware_concurrency - 1 threads)

## Zenith_Physics Class

Static manager for physics simulation. Key responsibilities:
- Initialize/shutdown Jolt engine
- Update physics with fixed 60 Hz timestep (uses frame time accumulator)
- Manage body lifecycle
- Process deferred collision events on main thread
- Raycast functionality for camera/editor interaction

## Collision System

### Layers
Two object layers: `NON_MOVING` (static) and `MOVING` (dynamic). Static objects only collide with dynamic objects. Used for broadphase optimization.

### Event Handling
Contact callbacks executed on worker threads. Events queued as `DeferredCollisionEvent` structs, processed on main thread during `Update()`. Dispatched to `Zenith_ScriptComponent` callbacks: `OnCollisionEnter()`, `OnCollisionStay()`, `OnCollisionExit()`.

## Physics Mesh Generation

### Quality Levels
- **LOW** - AABB (12 triangles, fastest)
- **MEDIUM** - Convex hull from extreme points
- **HIGH** - Simplified triangle mesh with vertex decimation

### Algorithm
Vertex decimation uses spatial hashing. Extreme vertices (min/max on each axis) preserved to maintain bounding volume. Configurable via `PhysicsMeshConfig` including simplification ratio, min/max triangle counts, auto-generation flag, and debug visualization.

### Debug Visualization
Green wireframe overlay rendered via `Flux_Primitives`. Controlled by `g_xPhysicsMeshConfig.m_bDebugDraw`. Visible in all editor modes.

## Key Concepts

**Fixed Timestep:** Physics simulates at constant 60 Hz regardless of frame rate. Time accumulator ensures deterministic behavior.

**Thread Safety:** Collision events deferred from worker threads to main thread via mutex-protected queue.

**Gravity:** Per-body gravity control via `SetGravityEnabled()`. Default gravity is -9.81 m/s² on Y-axis (down in left-handed coordinates).

**Body Limits:** Max 65536 bodies, 65536 body pairs, 10240 contact constraints. Configured via constants in implementation file.

## Scene Load/Reset Integration

**CRITICAL:** In `Zenith_Scene::LoadFromFile()`, `Zenith_Physics::Reset()` must be called AFTER `Scene::Reset()`.

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
