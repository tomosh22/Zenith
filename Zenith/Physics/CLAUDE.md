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
- Raycast functionality for camera/editor interaction. Two public `Raycast(...)` overloads return a `RaycastResult` (`m_bHit`, `m_xHitPoint`, `m_xHitNormal`, `m_fDistance`, `m_xHitEntity`); the second overload takes a `Zenith_PhysicsBodyID` to ignore. The EntityID-ignore convenience form lives engine-side in `Zenith_PhysicsQuery::RaycastIgnoring`. **Both overloads IGNORE SENSOR BODIES** — see below.

### Ordinary raycasts ignore sensors (ZM-D-173)

**The contract:** both public `Raycast` overloads skip any body whose
`JPH::Body::IsSensor()` is true. The body-id overload additionally skips the body
it was asked to ignore; passing an INVALID id still skips sensors, so it degrades
to the single-argument overload rather than to an unfiltered cast.

**Why.** Every shipped caller is a line-of-sight / occlusion query — camera-arm
occlusion, AI sight cones, ground and step probes, editor picking — all asking
"is something *solid* between these two points?". A sensor is by definition a
volume the player walks through, so a sensor answering one of those queries is
always a wrong answer, and it shipped as one: Zenithmon's `HomeDoorTrigger`
collapsed the overworld camera arm at the doorway it exists to open.

**How.** Two file-local filters in `Zenith_Physics.cpp`: `SkipSensorBodyFilter`
(a `JPH::BodyFilter` whose `ShouldCollideLocked` rejects sensors) and
`SkipSensorIgnoreSingleBodyFilter` (a `JPH::IgnoreSingleBodyFilter` that adds the
same locked check while *inheriting* `ShouldCollide(BodyID)`, which is where the
ignore actually lives — so the two rules compose instead of one replacing the
other). The flag is read in the LOCKED callback, so it is the live body state:
`SetIsSensor` takes effect on the very next cast.

**No sensor-including overload exists, deliberately.** A caller census at
ZM-D-173 found nothing that wants trigger volumes back from an occlusion query.
A future feature that genuinely needs to probe triggers must request an
explicitly named API rather than silently re-broadening what every ordinary LOS
query means. Coverage: `Physics/Raycast{SkipsSensorBodies,ReachesSolidBehindSensor,IgnoreBodyComposesWithSensorSkip,SensorToggleObservedLive}`
in `Zenith/EntityComponent/Zenith_Physics.Tests.inl`.
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

**★ THE SUBSTEP CAP IS LOAD-BEARING (ZM-D-184).** `Update()` runs at most **8**
substeps (~133 ms of simulation) per call and **DISCARDS** the leftover accumulator.
That loop used to be unbounded, and it caused a shipped fall-through bug: the first
two frames after a scene load take ~0.49 s (asset loads, pipeline creation), which
drained as **~29 consecutive substeps** — half a second of simulation applied in one
go to bodies that had just been created. A body resting exactly on the ground
free-falls through that burst, and once a capsule's **lower sphere centre** passes
below a one-sided triangle mesh (terrain collision is a surface, not a solid) the
contact normal inverts and the solver expels it **downward**, permanently. Measured
on Zenithmon's `Npc_RivalVesper`: 0.61 m of sink by frame 2, then gone; the player
cleared it by ~2 cm on the same load, which is why it presented as an intermittent
one-character bug.

Discarding the remainder rather than carrying it is deliberate — carrying it is the
spiral of death (each frame owes more substeps than the last). The trade is that
physics runs slower than wall-clock across a hitch, which is correct; the alternative
is bodies teleporting through geometry. A `Zenith_Warning` fires whenever the cap
engages: expected once at boot / scene load, and a signal that the game is
simulation-bound if it fires every frame.

**★ Corollary for content:** an authored DYNAMIC body should spawn with clearance
above its resting pose, never in exact contact — it must not depend on the solver
catching it on the first tick. Zenithmon spells this as `ZM_Dawnmere*SpawnY`.

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
