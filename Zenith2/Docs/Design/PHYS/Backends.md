# PHYS design — backends

This file is part of the PHYS design family; the entry document is `Zenith2/Docs/Design/PHYS.md`. It specifies the two backend libraries behind `Zenith_Physics`: how each third-party engine is pinned and built, how each is constructed and torn down, how each maps the neutral layers, materials, shapes, motion qualities and constraints, how each steps under the job system without a blocking wait, how each reports contacts, saves and restores state and shifts its origin, and where the two differ. Everything the neutral layer computes itself (the character controller, vehicles, buoyancy, joint breakage, query ordering, event emission) is in PHYS/Simulation.md and PHYS/CharacterRagdollVehicle.md and is identical on both backends.

## Scope of this file

| Topic | Heading |
|---|---|
| pins and build | "Third-party pins", "Build options" |
| process-level hooks | "Allocation and third-party global hooks", "Backend data tag", "Resident byte accounting" |
| Jolt | "Jolt construction steps", "Jolt world construction", "Jolt layer mapping", "Jolt step and the job system", "Jolt contact listener", "Jolt queries", "Origin shift on Jolt", "Jolt state save and restore" |
| both | "Body identity on both backends", "Material combination", "Motion quality", "Constraint mapping", "Backend shape payloads", "Heightfield builds" |
| PhysX | "PhysX construction steps", "PhysX world construction", "PhysX filter shader", "PhysX scene query filter", "PhysX step and the dispatcher", "PhysX contact reporting", "PhysX state save and restore", "PhysX error callback" |
| differences and rulings | "Backend behaviour differences", "PhysX on Android evaluation", "Conditional Jolt parallel step" |

## Third-party pins

Zenith2 pins both engines through `Zenith2/ThirdParty/manifest.json` under the schema of `Zenith2/Docs/Design/SHIP/Manifest.md` → "Dependency entry fields"; the entries are added by PHYS's pinning ticket (the "pinning ticket" rows of `Zenith2/Docs/Design/SHIP/Manifest.md` → "Entry names used by consumers"; cross-area row PH10), which is a `needs-human` ticket: a pin is a `needs-human` diff (`Zenith2/Docs/Design/SHIP.md` → "Pin landing order"), and the audit of PhysX's in-task waits it records ("PhysX step and the dispatcher") reads source no loop worker has. The old engine's vendored `Middleware/JoltPhysics-5.4.0` is prior art for the version only: Zenith2 never includes, links or copies it, and fetches the same upstream release into its own dependency cache.

```json
{
	"name": "jolt",
	"version": "v5.4.0",
	"source": { "url": "https://github.com/jrouwe/JoltPhysics/archive/refs/tags/v5.4.0.tar.gz", "sha256": "<64 hex digits>", "stripPrefix": "JoltPhysics-5.4.0", "format": "tar.gz" },
	"licence": "MIT",
	"integration": "cmake-subproject",
	"cmakeSourceDir": "Build",
	"owner": "ZenithPhysics_Jolt",
	"platforms": ["android", "linux", "windows"],
	"language": "cpp",
	"options": { "common": { "…": "the table of Build options" } },
	"targets": [ { "target": "Jolt", "owner": "ZenithPhysics_Jolt", "usage": "runtime" } ],
	"licenceFile": "LICENSE",
	"licenceSha256": "<64 hex digits>",
	"patches": [],
	"usage": "runtime"
}
```

```json
{
	"name": "physx",
	"version": "<the PhysX 5 release tag of NVIDIA-Omniverse/PhysX the pinning ticket records>",
	"source": { "url": "https://github.com/NVIDIA-Omniverse/PhysX/archive/refs/tags/<tag>.tar.gz", "sha256": "<64 hex digits>", "stripPrefix": "PhysX-<tag>", "format": "tar.gz" },
	"licence": "BSD-3-Clause",
	"integration": "zenith-wrapper",
	"owner": "ZenithPhysics_PhysX",
	"platforms": ["android", "windows"],
	"language": "cpp",
	"targets": [ { "target": "physx", "owner": "ZenithPhysics_PhysX", "usage": "runtime" } ],
	"licenceFile": "physx/LICENSE.md",
	"licenceSha256": "<64 hex digits>",
	"patches": [],
	"usage": "runtime"
}
```

- The PhysX entry lists `android` so that the D-SEL-PHYSX-ANDROID (open, Stage 1) harness can build and run `ZenithPhysics_PhysX` on the S24; if the ruling goes against Android, the ruling ticket removes `android` from the entry and changes `ZenithPhysics_PhysX`'s `ZENITH_SUPPORTED` from `ZENITH_GPU` to `ZENITH_WINDOWS` in the same diff (`Zenith2/Docs/Design/MOD/GameBuild.md` → "Backend to target mapping").
- The PhysX wrapper `Zenith2/ThirdParty/physx/CMakeLists.txt` compiles the CPU source groups of `physx/source/` (foundation, common, geomutils, lowlevel, lowlevelaabb, lowleveldynamics, simulationcontroller, scenequery, physx, physxextensions, physxcooking, physxcharacterkinematic is excluded, fastxml excluded) into one static target `physx`, with no GPU, PVD, vehicle, cloth or omni-PVD source; it adds `/fp:precise` (MSVC) and `-ffp-contract=off` (clang) to that target, because the wrapper is the one place a `zenith-wrapper` entry's compile options are set.
- The wrapper also declares `target_compile_definitions(physx PUBLIC PX_PHYSX_STATIC_LIB=1 PX_SUPPORT_PVD=0 PX_SUPPORT_GPU_PHYSX=0 DISABLE_CUDA_PHYSX=1)`. They are `PUBLIC` because PhysX's public headers read them: `PX_PHYSX_STATIC_LIB` selects plain declarations instead of `__declspec(dllimport)` on Windows, so every translation unit of `ZenithPhysics_PhysX` that includes a PhysX header must see it. The manifest entry therefore carries no `definitions` member: SHIP's rule is that "each key–value pair becomes `<KEY>=<value>` in `target_compile_definitions(<target> PRIVATE …)` of every target of the entry" (`Zenith2/Docs/Design/SHIP/Manifest.md` → "Dependency entry fields"), which would reach the `physx` target alone and leave the backend's own TUs with `dllimport` declarations.
- Upgrading either engine is an ordinary dependency update (`Zenith2/Docs/Design/SHIP/Manifest.md` → "Update procedure"): the data tag changes ("Backend data tag"), cooked backend sections are rebuilt by the next cook, saves of the old version still load through the neutral path, and the conformance suite and the PHYS budget rows are re-run (`Zenith2/Docs/Design/SHIP/Manifest.md` → "Evidence invalidation").

## Build options

The `options.common` of the `jolt` entry. Every option of the pinned `Build/CMakeLists.txt` is set explicitly, so a change of an upstream default changes nothing:

| Option | Value | Reason |
|---|---|---|
| `DOUBLE_PRECISION` | `OFF` | single precision with origin rebasing (PHYS/Simulation.md "Large coordinates") |
| `CROSS_PLATFORM_DETERMINISTIC` | `ON` | selects `/fp:precise` on MSVC and `-ffp-contract=off` on clang inside Jolt's own flags, which [Z2-N-ARCH-188] and determinism rule D9 require; with it `OFF` Jolt compiles MSVC with `/fp:fast`. The cross-platform equality it makes possible is not promised (PHYS/Simulation.md "Determinism boundaries") |
| `OBJECT_LAYER_BITS` | `16` | "Jolt layer mapping" |
| `USE_ASSERTS` | `OFF` | Jolt asserts would abort on untrusted data paths; Zenith2 validates before calling |
| `FLOATING_POINT_EXCEPTIONS_ENABLED` | `OFF` | the floating-point environment is fixed by the engine ([Z2-N-CONTRACT-360]); Jolt must not change it |
| `CPP_EXCEPTIONS_ENABLED`, `CPP_RTTI_ENABLED` | `OFF`, `OFF` | the third-party exceptions-off and RTTI-off rows |
| `USE_SSE4_1`, `USE_SSE4_2` | `ON`, `ON` | the x64 baseline every supported Windows machine has |
| `USE_AVX`, `USE_AVX2`, `USE_AVX512`, `USE_LZCNT`, `USE_TZCNT`, `USE_F16C`, `USE_FMADD` | all `OFF` | one instruction set on every x64 machine, so results do not depend on the CI or player CPU; arm64 uses NEON unconditionally |
| `INTERPROCEDURAL_OPTIMIZATION` | `OFF` | the engine's own link settings apply |
| `GENERATE_DEBUG_SYMBOLS` | `OFF` | the third-party debug-information row (`/Z7`, `-g`) supplies them |
| `OVERRIDE_CXX_FLAGS` | `OFF` | the engine's per-configuration flags apply |
| `USE_STATIC_MSVC_RUNTIME_LIBRARY` | `OFF` | `MultiThreadedDLL`, as every Zenith2 target |
| `ENABLE_ALL_WARNINGS` | `OFF` | third-party warnings are not the engine's |
| `DEBUG_RENDERER_IN_DEBUG_AND_RELEASE`, `DEBUG_RENDERER_IN_DISTRIBUTION` | `OFF`, `OFF` | PHYS draws its debug views from neutral data (PHYS.md "Debug views") |
| `PROFILER_IN_DEBUG_AND_RELEASE`, `PROFILER_IN_DISTRIBUTION`, `JPH_USE_EXTERNAL_PROFILE` | `OFF`, `OFF`, `OFF` | the step is one profiler zone of the engine's own |
| `TRACK_BROADPHASE_STATS`, `TRACK_NARROWPHASE_STATS` | `OFF`, `OFF` | — |
| `DISABLE_CUSTOM_ALLOCATOR` | `OFF` | the allocation hooks are required ("Allocation and third-party global hooks") |
| `USE_STD_VECTOR` | `OFF` | Jolt's own array type, through the hooks |
| `ENABLE_OBJECT_STREAM` | `OFF` | serialisation is the engine's own |
| `ENABLE_INSTALL`, `BUILD_SHARED_LIBS` | `OFF`, `OFF` | static library, no install rules |
| `CROSS_COMPILE_ARM`, `USE_WASM_SIMD`, `JPH_USE_WASM64` | `OFF` | — |
| `TARGET_UNIT_TESTS`, `TARGET_HELLO_WORLD`, `TARGET_PERFORMANCE_TEST`, `TARGET_SAMPLES`, `TARGET_VIEWER` | `OFF` | only the `Jolt` library is built |
| `JPH_ENABLE_VULKAN` | `OFF` | Linux only, viewer only |

The sanitizer rows apply to the `Jolt` and `physx` targets in the sanitizer presets ([Z2-N-ARCH-201]); both are compiled from source there.

## Allocation and third-party global hooks

Both engines allocate through hooks routed to the root heap ([Z2-N-CONTRACT-240]).

| Engine | Hook | Implementation (under each backend's `Internal/`) |
|---|---|---|
| Jolt | `JPH::Allocate`, `JPH::Reallocate`, `JPH::Free`, `JPH::AlignedAllocate`, `JPH::AlignedFree` | free functions in `Zenith_JoltAllocator.cpp` calling `Zenith_GetRootHeap().Allocate(size, max(alignment, 16))` and `Free`; `Reallocate` allocates the new block, copies `min(old, new)` bytes and frees the old one |
| Jolt | `JPH::Trace` | a function that discards the message (Jolt traces only from its asserts and samples, both compiled out) |
| Jolt | `JPH::Factory::sInstance` | points at the `JPH::Factory` member of the live `Zenith_PhysicsJolt` object, set in construction step 5 and reset to nullptr in its destructor |
| PhysX | `PxAllocatorCallback` | `Zenith_PhysXAllocator`, a member of the backend object, allocating 16-byte-aligned blocks from `Zenith_PhysicsDesc::m_pxHeap` and counting live bytes ("Resident byte accounting") |
| PhysX | `PxErrorCallback` | `Zenith_PhysXErrorCallback`, a member of the backend object ("PhysX error callback") |

<!-- informative -->
Jolt's hooks are process-wide function pointers with no user context, and `JPH::Factory::sInstance` is a process-wide pointer. They are writable data symbols of the third-party source root, outside the scope of the writable-data scan ([Z2-N-CONTRACT-151] step 1). That scope is an enforcement boundary, not a compliance argument: writing process-wide third-party state outside `main` is an exception to Z2-D-046, and core-doc item X264 (i) records it as one (one Architecture bullet: a backend factory may write a third-party library's process-wide hook pointers only inside its construction and destruction, under a one-instance-per-process refusal). Zenith2 code writes them only inside the backend's construction and destruction, and the one-instance rule (`PHYSICS_ERROR_BACKEND_ALREADY_ACTIVE`) is what keeps that sound. Because the allocation hooks carry no context, the Jolt backend requires `m_pxHeap` to be the installed root heap (`PHYSICS_ERROR_INVALID_DESC` otherwise), and it cannot count its bytes in the hook itself; "Resident byte accounting" says what it counts instead.
<!-- /informative -->

## Backend data tag

`Zenith_PhysicsBackendInfo::m_ulDataTag` is a `Zenith_Hash64` over the little-endian bytes of:

| Field | Jolt | PhysX |
|---|---|---|
| backend kind | 1 | 2 |
| upstream version | `JPH_VERSION_MAJOR`, `JPH_VERSION_MINOR`, `JPH_VERSION_PATCH` | `PX_PHYSICS_VERSION` |
| layout-affecting options | `OBJECT_LAYER_BITS`, `DOUBLE_PRECISION`, `CROSS_PLATFORM_DETERMINISTIC` | the cooking parameters of "Backend shape payloads" |
| pointer size and endianness | 8, little | 8, little |
| `uPHYSICS_BACKEND_PAYLOAD_VERSION` | 1 | 1 |

A backend section is used only when its tag equals the running backend's (PHYS/Formats.md "Backend sections"), so a dependency update or an option change silently falls back to the neutral section until the next cook, and never misreads a payload.

## Resident byte accounting

`GetResidentBytes` feeds the memory consumer (PHYS/Simulation.md "Memory consumer") and the budget row Z2-B-PHYS-001.

- **PhysX** counts exactly: `Zenith_PhysXAllocator` adds `GetUsableSize` on allocation and subtracts it on free, in a relaxed atomic of the backend object; a world's share is not separated, so the service reports the whole and each world reports 0 plus its own Zenith-side tables.
- **Jolt** accounts by construction: per world, the step scratch arena (`m_ulStepScratchBytes`), the capacity-derived tables of `JPH::PhysicsSystem::Init` (a formula over `m_uMaxBodies`, `m_uMaxBodyPairs` and `m_uMaxContactConstraints` with the `sizeof` of the pinned types, computed once at world creation), `sizeof(JPH::Body)` per live body plus `sizeof(JPH::MotionProperties)` per non-static body, and the constraint objects; per service, the sum of the live shapes' `JPH::Shape::GetStats().mSizeBytes` and the material objects. `Test_PhysicsConformance_ResidentBytesTracksHeap[<Backend>]` creates 10,000 bodies and 1,000 constraints in an otherwise idle process and asserts that the reported bytes are within 10 % of the root heap's live-byte delta.

## Jolt construction steps

`Zenith_CreatePhysics_Jolt(const Zenith_PhysicsDesc&)` (the one-parameter `Zenith_PfnCreatePhysics` of `Zenith2/Docs/Architecture/CompositionRoot.md`; no backend needs the job system at construction); each step has a fault point (`Zenith_PhysicsDesc::m_pxFaults`), and a failure unwinds every earlier step in reverse ([Z2-N-CONTRACT-341], `Test_PhysicsJolt_Create_FailsAt<Step>_Unwinds`):

| Step | Action | Failure |
|---|---|---|
| 1 Validate | heap is the root heap, capacities within limits | `PHYSICS_ERROR_INVALID_DESC` |
| 2 Exclusive | `JPH::Factory::sInstance` is nullptr | `PHYSICS_ERROR_BACKEND_ALREADY_ACTIVE` |
| 3 Object | `Zenith_TryNew<Zenith_PhysicsJolt>` | `PHYSICS_ERROR_OUT_OF_MEMORY` |
| 4 Hooks | install the allocation and trace hooks (the same function pointers every time) | — |
| 5 Factory | `sInstance = &m_xFactory`; `JPH::RegisterTypes()` | `PHYSICS_ERROR_BACKEND_INIT` (fault point only; Jolt cannot report one) |
| 6 Tables | the shape and material handle tables (pools of `m_uMaxShapes` and `m_uMaxMaterials` slots) | `PHYSICS_ERROR_OUT_OF_MEMORY` |
| 7 Defaults | the default material: `Zenith_PhysicsMaterialData`'s member defaults (friction 0.5, restitution 0, density 1,000, friction combine AVERAGE, restitution combine MAX, surface `Default`), the material the null handle names on both backends | `PHYSICS_ERROR_OUT_OF_MEMORY` |

The destructor runs in reverse: it ZENITH_CHECKs that no world is alive, releases leaked shapes and materials (counting `physics.leaked_handles`), calls `JPH::UnregisterTypes()` and resets `sInstance`. The allocation hooks stay installed (they point at code, not at the object).

## Jolt world construction

`CreateWorld(const Zenith_PhysicsWorldDesc&)`:

1. Validate the descriptor (capacities against the limits of PHYS.md "Limits and constants"; the layer table against PHYS.md "Layer configuration") → `PHYSICS_ERROR_INVALID_DESC` or `PHYSICS_ERROR_LAYER_CONFIG`.
2. Allocate the world object and its `JPH::TempAllocatorImpl` of `m_ulStepScratchBytes` → `PHYSICS_ERROR_OUT_OF_MEMORY`.
3. Build the layer objects of "Jolt layer mapping".
4. `JPH::PhysicsSystem::Init(m_uMaxBodies, 0, m_uMaxBodyPairs, m_uMaxContactConstraints, …)` with the three layer interfaces.
5. `JPH::PhysicsSettings`: `mDeterministicSimulation = true`, `mNumVelocitySteps` and `mNumPositionSteps` from the solver-iteration keys, `mSpeculativeContactDistance`, `mPenetrationSlop`, `mTimeBeforeSleep`, `mPointVelocitySleepThreshold` from their keys, `mAllowSleeping = true`, `mUseBodyPairContactCache = true`, `mUseManifoldReduction = true`; `SetPhysicsSettings`.
6. `SetGravity`, `SetContactListener(&m_xContactListener)`; no body activation listener (sleep transitions are read at write-back).
7. Create the world's one `Zenith_JoltGroupFilter` ("Jolt layer mapping"), referenced by every body's `JPH::CollisionGroup`.
8. Register the world's tag (1 … 4,095, the 12-bit field of the handle codec, matching the ECS world tag range) in its handle codec.

The world owns `JPH::JobSystemSingleThreaded m_xJobSystem{JPH::cMaxPhysicsJobs}` ("Jolt step and the job system").

## Jolt layer mapping

A Jolt `ObjectLayer` (16 bits) encodes `layerIndex | (bodyClass << 5)`: bits 0–4 the neutral layer index (0 … 31), bits 5–6 the class (0 static, 1 moving, 2 trigger, 3 query-only). The class is a function of the body's motion type and flags: QUERY_ONLY wins, then TRIGGER, then STATIC for static bodies, otherwise MOVING (dynamic and kinematic).

| Jolt interface | Rule |
|---|---|
| `BroadPhaseLayerInterface` | four broad-phase layers in class order: STATIC (0), MOVING (1), TRIGGER (2), QUERY_ONLY (3) |
| `ObjectVsBroadPhaseLayerFilter` | static objects test MOVING only; moving objects test STATIC, MOVING and TRIGGER; triggers test MOVING only; query-only objects test nothing |
| `ObjectLayerPairFilter` | `false` for static–static, trigger–trigger, trigger–static and any query-only object; otherwise the neutral layer table's bit (`Zenith_PhysicsLayerTable::Collides(a, b)`, symmetric by construction) |
| `GroupFilter` | one `Zenith_JoltGroupFilter` per world (below), the group filter of every body of the world |

**Triggers.** Every trigger body is a KINEMATIC Jolt sensor (`mIsSensor = true`, `mMotionType = EMotionType::Kinematic`, `mAllowSleeping = false`, `mCollideKinematicVsNonDynamic = false`; PHYS/Simulation.md "Body composition" rule 9). Jolt's pair predicate `Body::sFindCollidingPairsCanCollide` (`Jolt/Physics/Body/Body.inl` of the pinned release) admits a pair when either body is dynamic or when "A kinematic object can collide with a sensor", and rejects kinematic–static and kinematic–kinematic otherwise, so a kinematic sensor pairs with dynamic and kinematic bodies and with nothing static; the object-layer pair filter removes trigger–trigger. Because the sensor is kinematic and active, it keeps overlaps with sleeping bodies, which a static sensor would lose ("As soon as a body goes to sleep, the contact will be lost", the "Sensors" section of the pinned release's architecture document).

**`Zenith_JoltGroupFilter`** (`ZenithPhysics_Jolt/Internal/Zenith_JoltGroupFilter.cpp`, a `JPH::GroupFilter`). `JPH::CollisionGroup::CanCollide` calls the group filter of the first body when it has one, so one filter shared by every body of the world sees every pair. Each body's `JPH::CollisionGroup` is `{ filter, GroupID = the neutral collision group (0 = none), SubGroupID = the neutral body index | (IGNORE_SAME_GROUP ? 1u << 31 : 0) }`. `CanCollide(g1, g2)`:

1. When both GroupIDs are equal and non-zero and either SubGroupID has bit 31 set, return false (`PHYSICS_BODY_FLAG_IGNORE_SAME_GROUP`, the general collision-group rule; `Zenith_PhysicsBodyDesc::m_uCollisionGroup`, RigidBody property 10).
2. Let i, j be the two body indices (bits 0–22 of the SubGroupIDs), lo = min, hi = max; binary-search `(lo << 32) | hi` in the excluded-pair array the module set with `SetExcludedPairs` (PHYS/Simulation.md "Module world state"); found → false. This realises CollideConnected false (and the ragdoll parent–child rule) on Jolt, which has no collide-connected setting of its own: `TwoBodyConstraintSettings` and `ConstraintSettings` of the pinned release carry no such member, and Jolt never suppresses collision between constrained bodies by itself.
3. Otherwise true.

The filter runs only inside `PhysicsSystem::Update` (on the stepping thread, or on the drain leaves of the conditional parallel step), and `SetExcludedPairs` is called only by ingest step 12, before the tick's first `StepOnce` (PHYS/Simulation.md "Excluded pairs maintenance"), so its array is immutable whenever the filter reads it and describes the live constraints at every step. `SetBodyFiltering` rewrites a body's object layer (`BodyInterface::SetObjectLayer`) and collision group (`Body::SetCollisionGroup`) together.

Queries use the same encoding (PHYS/Simulation.md "Query execution" step 2): the broad-phase filter admits STATIC when `m_bIncludeStatic`, MOVING when `m_bIncludeDynamic`, TRIGGER when `m_bIncludeTriggers` and QUERY_ONLY when `m_bIncludeQueryOnly`; the `ObjectLayerFilter` tests bit `layerIndex` of the mask; the body filter of "Jolt queries" applies `m_uIgnoreGroup` and the ignore list. A query-only body is therefore visible to a Jolt query exactly when the query includes query-only bodies, as on PhysX ("PhysX scene query filter").

## Jolt step and the job system

`StepOnce(h, xJobs, xContacts)` calls `m_xSystem.Update(h, 1, &m_xTempAllocator, &m_xJobSystem)` on the calling thread (the `Physics.Step` node's thread) and returns with `m_bPending = false`; `ResumeStep` is never reached on Jolt (ZENITH_ASSERT).

- `JPH::JobSystemSingleThreaded` executes each job inline when it is queued, so `Update` never waits for another thread: the step satisfies the rule that jobs never block ([Z2-N-CONTRACT-315], Z2-D-006).
- A multithreaded Jolt step needs a `JPH::JobSystem` whose `WaitForJobs` returns only when its barrier's jobs have run, which is a wait inside a job; Zenith2 does not build one in the baseline. Flagged proposal PHYS-1 (`findings/FLAGGED_PROPOSALS.md`) asked for a bracketed help-while-wait exemption; the orchestrator's ruling on Q87 decides it on Stage 1 evidence ("Conditional Jolt parallel step"). Until a recorded decision admits it, `PHYSICS_CAPABILITY_PARALLEL_STEP` is absent from Jolt and its step cost is measured single-threaded against Z2-B-PHYS-002.
- `Update`'s returned `JPH::EPhysicsUpdateError` maps to the stats: `BodyPairCacheFull` → `m_bBodyPairsFull`, `ContactConstraintsFull` → `m_bContactConstraintsFull`, `ManifoldCacheFull` → `m_bContactConstraintsFull`; none is `PHYSICS_ERROR_STEP_FAILED` (the module counts them, PHYS.md "Failure handling").
- Bodies are added in batches at ingest (`BodyInterface::AddBodiesPrepare` / `AddBodiesFinalize` in ascending simulation ID) and the broad phase is optimised (`OptimizeBroadPhase`) after a batch of more than 1,024 additions, at ingest, never inside a substep.

## Jolt contact listener

`Zenith_JoltContactListener` (one per world) implements `JPH::ContactListener`. With the single-threaded job system every callback runs on the step thread, in Jolt's deterministic order; under the conditional parallel step the callbacks run concurrently on the drain leaves and the waiter ("from multiple threads at the same time", `ContactListener.h` of the pinned release), and every write they make to shared state goes through lock 530 ("Conditional Jolt parallel step", paragraph "Contact listener under the parallel step"). Every callback reads only data that is immutable during the step: the world's slot table (user data, part codes, live bits, generations), the materials (`Zenith_JoltMaterial`), the heightfield hole materials and the excluded-pair array:

| Callback | Action |
|---|---|
| `OnContactValidate` | for a pair in which either body's shape is a heightfield whose tile has holes: `RejectContact` when the heightfield sub-shape of `inCollisionResult` is a hole quad (its material is the hole material of "Heightfield builds"), otherwise `AcceptContact`, so the callback keeps running for every subsequent manifold of that body pair; for every other pair `AcceptAllContactsForThisBodyPair` (Jolt's default, which stops the callback for the pair's subsequent manifolds: `ContactListener.h` of the pinned release), because every other filter is complete in the layer and group filters |
| `OnContactAdded` | combine the two sub-shapes' materials ("Material combination") into `ioSettings`; if either body reports contacts or interactions (PHYS/Simulation.md "Event emission") or either is a sensor, append a `PHYSICS_CONTACT_ADDED` record (flag bit 0 when either is a sensor) with the two bodies' user data and part codes from the world's slot table ("Body identity on both backends"), contact point 0 of the manifold (`inManifold.GetWorldSpaceContactPointOn1(0)`), the normal, the approach speed and the sub-shape materials |
| `OnContactPersisted` | combine materials again (sub-shapes may change); append a `PHYSICS_CONTACT_PERSISTED` record only for pairs whose interaction detection needs speeds (PHYS/Simulation.md "Interaction detection") |
| `OnContactRemoved` | Jolt passes the two `BodyID`s and sub-shape IDs only and forbids reading the bodies ("The body may have been removed and destroyed"), so the callback pushes `{ BodyID A, BodyID B, SubShapeID A, SubShapeID B, sensor flag }` into the listener's removed-contact staging `m_axRemovedStaging` (capacity `physics.maxContactConstraints`, reset at each `StepOnce`: Jolt reports at most one removal per cached manifold, and its manifold cache holds at most that many, so the staging never fills; a full staging is a defect, `ZENITH_ASSERT`) and appends nothing yet |

**After `Update`**, still inside `StepOnce` on the step node's thread, the backend resolves the staging, in any order (each entry is resolved independently and the sink's retention is order-independent):

1. **Identity by generation.** For each of the two `BodyID`s, the body is its slot's live occupant exactly when the slot table's live bit for `GetIndex()` is set and `GetSequenceNumber()` equals the slot's `generation & 0xFF` (every Jolt body is created with `BodyID(index, generation & 0xFF)`, "Jolt state save and restore"; a slot is reallocated at most a few times between two steps, far fewer than 256, so the 8-bit sequence cannot alias). An entry either of whose bodies is not its slot's live occupant is a destroyed-body report: it is dropped without reading the slot's user data or part code (which may already belong to a new body) and counted through `Zenith_PhysicsContactSink::CountStaleRemoved` (`physics.contacts.stale_removed_delta`). Its pair ends through the module's ended list instead (PHYS/Simulation.md "Event emission" step 2).
2. **Sleep flag.** For an entry of two live bodies, the record is `PHYSICS_CONTACT_REMOVED` with both bodies' user data and part codes from the slot table, flag bit 0 for a sensor pair, and flag bit 1 "sleep" when at least one of the two bodies is non-static and `BodyInterfaceNoLock::IsActive` is false for it (Jolt removes a body's contacts when it goes to sleep, PHYS/Simulation.md "Sleeping"); a static body is never active and never sets the bit.
3. `Append` it to the sink.

The sink keeps the records with the lowest sort keys when full (PHYS/PublicAPI.md "Zenith_PhysicsWorld header"), so the stored set never depends on callback or resolution order. `Test_PhysicsEvents_DestroyedBodyRecordsDropped[<Backend>]` and `Test_PhysicsConformance_ContactsReported[<Backend>]` assert the two rules.

## Jolt queries

Queries use `GetNarrowPhaseQueryNoLock()` and `GetBodyLockInterfaceNoLock()`; the no-lock variants are correct because the frame graph never runs a query concurrently with a mutating member or a step of the same world (the world resource's declared access, PHYS/PublicAPI.md "Zenith_PhysicsWorld header").

| Neutral query | Jolt call | Settings |
|---|---|---|
| `CastRay` | `CastRay(RRayCast, RayCastSettings, collector, …)` | `mBackFaceModeTriangles = IgnoreBackFaces`, `mTreatConvexAsSolid = true` |
| `CastShape` | `CastShape(RShapeCast, ShapeCastSettings, base offset 0, collector, …)` | `mBackFaceModeTriangles = IgnoreBackFaces`, `mReturnDeepestPoint = true`, `mUseShrunkenShapeAndConvexRadius = true` |
| `Overlap` | `CollideShape` with `CollideShapeSettings{ mMaxSeparationDistance = 0 }` | bodies only (one hit per body, deepest) |
| `CollideShape` | `CollideShape(…)` | the descriptor's maximum separation |

Each Jolt collector forwards every hit to the caller's `Zenith_PhysicsHitCollector`, which keeps the K smallest keys (distance bits, then user data, then sub-shape, then body index), so Jolt's early-out order never affects the result; the forwarding collector reports Jolt's early-out fraction as the collector's current K-th distance once K hits are held. The body filter (`ShouldCollideLocked(const JPH::Body&)`) rejects a body whose `CollisionGroup` GroupID equals a non-zero `m_uIgnoreGroup`, then a body whose user data is in the query's sorted ignore list (binary search). The forwarding collector drops a hit on a heightfield hole quad ("Heightfield builds").

## Origin shift on Jolt

Jolt has no origin shift (its large-world answer is double precision, which Zenith2 does not build). `ShiftOrigin(xShift)`, called by the module's origin-shift listener at the sync point that played the rebase (PHYS/Simulation.md "Origin shift"):

1. For each live body in ascending body index: `BodyInterfaceNoLock::SetPosition(id, position − xShift, EActivation::DontActivate)` and `InvalidateContactCache(id)`, with `position` the body origin `GetPosition` returns. `xShift` is exact in binary32 and the subtraction is correctly rounded, so the origin passed equals SCENE's `(float)((double)p − S)`. Jolt stores the centre-of-mass position and `SetPosition` recomputes it as `fl(p' + R·c)`, so for a body with a non-zero centre-of-mass offset (hulls, compounds) the origin read back afterwards may differ from `p − xShift` by one binary32 rounding; nothing compares backend bits after a shift (PHYS/PublicAPI.md "Zenith_PhysicsWorld header", `ShiftOrigin`), and `Test_PhysicsConformance_OriginShiftPreservesRestingStack[<Backend>]` asserts relative poses within 1e-5 m.
2. For each world-anchored constraint (body B null, Jolt's `Body::sFixedToWorld`) in ascending priority: its world-space frame B lives inside the Jolt constraint, so the backend saves the constraint's solver state (`Constraint::SaveState` into a stack recorder), removes it, re-creates it from its stored neutral descriptor with `m_xFrameBPosition − xShift` and the same priority (the neutral handle keeps pointing at the new object), and restores the saved state into it (`Constraint::RestoreState`), so warm starting continues. The constraint's position in Jolt's manager array changes, which is harmless: with unique priorities the solve order is the priority order (`ConstraintManager::sSortConstraints` of the pinned release compares `GetConstraintPriority()` first), and the EXACT blob saves constraints by priority, never by manager order ("Jolt state save and restore").
3. `OptimizeBroadPhase()`.

The cost is linear in bodies plus world-anchored constraints and runs once per rebase, at most once per `stream.rebase.minIntervalTicks` ticks. `Test_PhysicsConformance_OriginShiftPreservesRestingStack[<Backend>]` shifts a sleeping and an awake 10-box stack and a world-anchored hinge by one quantum of STREAM's defaults on X and Z (2,048 m each) and asserts that every relative pose is unchanged within 1e-5 m, that the hinge's anchor moved by exactly −S, and that the awake stack is still resting 300 ticks after the shift. The stacks, their static ground box and the hinge anchor are placed with every X and Z coordinate in [1,024 m, 4,096 m] (the stacks centred at (2,050, 0, 2,050) and (2,060, 0, 2,050), the hinge anchor at (2,070, 1, 2,050), the ground box spanning X and Z 2,030 … 2,090 m): for every binary32 p in that range `p − 2048` is exact by Sterbenz's lemma (2,048 / 2 ≤ p ≤ 2 × 2,048), so every body origin and the anchor move by exactly −S, and the 1e-5 m bound measures only Jolt's centre-of-mass recomputation (about 2.4e-7 m near 2 m). A stack near the origin would land in [2,048, 4,096) m, whose spacing of 2.44e-4 m rounds each body independently by up to 1.22e-4 m.

## Jolt state save and restore

The pinned release's own rules decide this design. Its architecture document says the simulation is deterministic provided that "bodies and constraints need to be added/removed/modified in exactly the same order", that "adding body A then B is the same as B then A as long as the BodyIDs of A and B are consistent", that constraint addition order is free "as long as ConstraintSettings::mConstraintPriority is unique per constraint", and that "PhysicsSystem::SaveState relies on the ordering of constraints, so you'll have to skip serializing constraints by not setting EStateRecorderState::Constraints and call Constraint::SaveState / Constraint::RestoreState directly yourself" (its sections "Deterministic Simulation" and "Being Sloppy While Still Being Deterministic"); `ConstraintManager::RestoreState` indeed indexes constraints by their position in the manager's array, which depends on the history of removals.

- **IDs.** Every body is created with `CreateBodyWithID(JPH::BodyID(index, generation & 0xFF), …)` from its neutral handle on every path (PHYS/Simulation.md "Slot allocation"); Jolt's own `CreateBody` allocator is never used. Constraint priorities are the module's unique creation ranks.
- `SaveState(xOut)`: into `Zenith_JoltStateRecorder`, a `JPH::StateRecorder` writing into the caller's arena (never Jolt's `StateRecorderImpl`, which uses the standard library's string streams): first `m_xSystem.SaveState(recorder, EStateRecorderState::Global | EStateRecorderState::Bodies | EStateRecorderState::Contacts)`, then `uint32 constraintCount` and, for every constraint in ascending priority, `uint32 priority` followed by `Constraint::SaveState(recorder)`. The bytes are the EXACT blob of PHYS/Formats.md "Save section layout".
- `RestoreState(xState)`: the module has already restored the slot tables and recreated every body with `CreateBodyWithHandle` (so each Jolt body exists with its recorded `BodyID`) and every constraint with `CreateConstraintWithHandle` and its recorded priority; `m_xSystem.RestoreState(reader)` over a `Zenith_JoltStateRecorder` restores the global, body and contact state; then, for each recorded (priority, state), the constraint of that priority restores its state (`Constraint::RestoreState`). A reader that reports `IsFailed()`, a recorded priority with no live constraint, a differing constraint count or bytes left over is `PHYSICS_ERROR_STATE_MISMATCH`, and the module keeps the coherent restore it already made and reports the fallback (PHYS/Simulation.md "Save and restore algorithm").
- `PHYSICS_CAPABILITY_EXACT_STATE_RESTORE` is set. `Test_PhysicsConformance_SaveRestoreExactContinuation[Jolt]` runs the determinism replay's scripted stream (impulses, body and constraint creations and destructions, ragdoll enters and recovers) for 300 ticks with at least 50 destructions before the save, so the free sets have holes; saves at tick 300; restores into a fresh world; continues the same scripted stream, which creates bodies and constraints in its first ticks after the load; and asserts that the canonical state hash of each of the next 600 ticks equals the uninterrupted run's, and that the handle and Jolt BodyID of every creation after the load equal the uninterrupted run's.

## Body identity on both backends

`Zenith_PhysicsWorldBase` (`ZenithPhysics/Internal/Zenith_PhysicsBackendBase.h`) keeps, per body slot, the creating descriptor's user data and ragdoll part code (`uint64 m_aulUserData[capacity]`, `uint8 m_auRagdollPart[capacity]`), written by every creation path (`CreateBody`, `CreateBodies`, `CreateBodyWithHandle`) and left in place by `DestroyBody` until the slot is allocated again. Allocation can follow a destruction before the next step reports it (ingest step 3 frees an index and step 4 of the same ingest reuses it; a `Physics.EnterRagdoll` playback allocates at any sync point), so the table's values are read only for a body that is still its slot's live occupant: every backend checks a removal report's identity before it reads the table (Jolt by the `BodyID` sequence number against the slot's live bit and `generation & 0xFF`, "Jolt contact listener"; PhysX by the pair header's removed-actor flags, "PhysX contact reporting"), drops a report of a destroyed body, and counts it `physics.contacts.stale_removed_delta`. Both backends read a body's identity from that table by its slot index, never from a third-party field that could hold one value only:

| Backend | Slot index of a body | Where it is read |
|---|---|---|
| Jolt | `BodyID::GetIndex()` (the neutral index, "Jolt state save and restore") | the contact listener's records, the query collectors' `m_ulUserData`; `Body::SetUserData` also holds the user data for Jolt-side filters |
| PhysX | the actor's `userData`, set to `reinterpret_cast<void*>(uintptr_t(index))` at creation | `Zenith_PhysXEvents`' records, `Zenith_PhysXQueryFilter` and the query hits |

Every record a backend appends therefore carries the user data and part codes of two bodies that were live when it appended it ("Ragdoll part identity" of PHYS/Simulation.md); no record ever names a destroyed body, whose pairs the module ends through its ended list (PHYS/Simulation.md "Event emission" step 2).

## Material combination

| Combine mode | Formula | PhysX mode |
|---|---|---|
| AVERAGE (0) | (a + b) / 2 | `PxCombineMode::eAVERAGE` |
| MIN (1) | min(a, b) | `eMIN` |
| MULTIPLY (2) | a × b | `eMULTIPLY` |
| MAX (3) | max(a, b) | `eMAX` |

Of the two materials' modes the higher value applies, separately for friction and restitution. Jolt: the contact listener calls `Zenith_CombinePhysicsMaterials` and writes `mCombinedFriction` and `mCombinedRestitution` (Jolt's own combine functions are not used). PhysX: each `PxMaterial` carries the mapped modes, and PhysX's native rule (the higher-valued mode wins; the same formulas) is the same function. `Test_PhysicsConformance_MaterialCombineMatchesFunction[<Backend>]` slides a box on each of 16 material pairs and asserts that the measured deceleration equals `g × μ` with μ from `Zenith_CombinePhysicsMaterials` within 2 %.

Material identity on contacts: each Jolt sub-shape's `JPH::PhysicsMaterial` is a `Zenith_JoltMaterial` carrying the material handle; each PhysX shape's material index maps to the handle through the backend's material table.

## Motion quality

| Neutral | Jolt | PhysX |
|---|---|---|
| `PHYSICS_MOTION_QUALITY_DISCRETE` | `EMotionQuality::Discrete` | no CCD flag |
| `PHYSICS_MOTION_QUALITY_CONTINUOUS` | `EMotionQuality::LinearCast` | `PxRigidBodyFlag::eENABLE_CCD`, plus `PxPairFlag::eDETECT_CCD_CONTACT` from the filter shader when either body has it |

Speculative contacts are on in both (Jolt's `mSpeculativeContactDistance`; PhysX's contact offset set from the same key), which is what keeps a fast discrete body from tunnelling through thick geometry; continuous quality is for thin geometry and small fast bodies (PHYS/Tests.md "Tunnelling tests").

## Constraint mapping

| Neutral kind | Jolt | PhysX |
|---|---|---|
| FIXED | `FixedConstraint` | `PxFixedJoint` |
| POINT | `PointConstraint` | `PxSphericalJoint` without limits |
| HINGE | `HingeConstraint` (limits, motor) | `PxRevoluteJoint` (limit, drive) |
| SLIDER | `SliderConstraint` | `PxPrismaticJoint` |
| DISTANCE | `DistanceConstraint` (min, max, spring) | `PxDistanceJoint` |
| CONE | `ConeConstraint` | `PxSphericalJoint` with a cone limit |
| SWING_TWIST | `SwingTwistConstraint` | `PxD6Joint` with twist and swing limits |
| SIX_DOF | `SixDOFConstraint` | `PxD6Joint` |

Constraint creation order is the module's priority counter (PHYS/Simulation.md "Constraint kinds"); Jolt's `mConstraintPriority` is set from it, and PhysX solves joints in creation order under enhanced determinism.

**Connected-body collision** (`Zenith_PhysicsConstraintDesc::m_bCollideConnected`, `Zenith.Physics.Joint` property 19 CollideConnected) has one meaning on both backends: false (the default) means the constraint's two bodies never collide with each other; true means they collide like any other pair.

| Backend | CollideConnected false | true |
|---|---|---|
| Jolt | the pair is in the world's excluded-pair array, which `Zenith_JoltGroupFilter::CanCollide` rejects ("Jolt layer mapping"); Jolt has no collide-connected setting of its own | not in the array |
| PhysX | `PxConstraintFlag::eCOLLISION_ENABLED` cleared on the joint (PhysX's default, under which PhysX filters the actor pair itself); `SetExcludedPairs` is accepted and unused | the flag set |

A pair joined by two constraints is excluded when either has CollideConnected false. A world-anchored constraint (body B null) has no pair. `Test_PhysicsConformance_JointCollideConnected[<Backend>]` asserts the rule: two overlapping boxes joined by a hinge with CollideConnected false report no contact over 120 ticks and stay interpenetrating; with true they report ContactBegan in the first tick and separate.

## Backend shape payloads

| Shape | Jolt payload | PhysX payload |
|---|---|---|
| convex hull | `Shape::SaveBinaryState` of the `ConvexHullShape` (with its convex radius) | the cooked `PxConvexMesh` stream (`PxCookConvexMesh`, `eCOMPUTE_CONVEX`, vertex limit 255, `eGPU_COMPATIBLE` off) |
| triangle mesh | `MeshShape` with its built BVH (`SaveWithChildren`, materials as palette indices) | the cooked `PxTriangleMesh` stream (BVH34, `eWELD_VERTICES` off because the cook welded already) |
| heightfield | none: built from the neutral section at load by the rules of "Heightfield builds" | none, for the same reason |
| compound | `StaticCompoundShape` with children (`SaveWithChildren`) | none: a PhysX actor carries one `PxShape` per child, built from the neutral children and the cooked hulls of each child's convex payload |

`BakeShape` writes the payload into the cook's arena through `Zenith_JoltStreamOut` / `Zenith_PhysXOutputStream` (arena-backed `JPH::StreamOut` and `PxOutputStream`), and `CreateShapeFromCooked` reads it through the matching input streams; a stream that ends early or reports failure is `PHYSICS_ERROR_COOKED_CORRUPT`, and the loader then builds from the neutral section. The cook links the backends for this (cross-area row PH5); a backend section is an optimisation only, so a cook without it is complete.

## Heightfield builds

The neutral heightfield (PHYS/Formats.md "Neutral geometry section", kind 7) is square: `samples × samples` u16 heights decoding to `heightMin + h / 65535 × (heightMax − heightMin)`, spacing d, the fixed diagonal 0 (every quad split along (s, t)–(s+1, t+1), s along +X, t along +Z), per-quad palette indices (at most `uPHYSICS_MAX_HEIGHTFIELD_PALETTE` = 127 entries) and per-quad hole bits. Each backend builds its runtime shape from it at load:

| Aspect | Jolt (`JPH::HeightFieldShapeSettings`) | PhysX (`PxHeightFieldDesc`, `PxHeightFieldGeometry`) |
|---|---|---|
| samples | `mHeightSamples[t × samples + s]` = the decoded height in metres (row-major, +X fastest, which is Jolt's `y × mSampleCount + x` with x = s, y = t); `mSampleCount = samples`; Jolt rounds the count up to a multiple of `mBlockSize` internally (257 → 258, the constructor's "Round sample count to nearest block size"), and the padded row and column lie outside the tile's neutral bounds, which no contact or query of the tile is accepted beyond | `PxHeightFieldSample::height = (PxI16)(h − 32768)` exactly (the 65,536 neutral levels unchanged), `heightScale = (heightMax − heightMin) / 65535`, the shape's local origin raised by `heightMin + 32768 × heightScale`; PhysX's row r runs along +X (s) and column c along +Z (t), so sample (r, c) is neutral (s = r, t = c); `nbRows = nbColumns = samples` |
| scale | `mScale = (d, 1, d)`, `mOffset = 0` | `rowScale = columnScale = d` |
| compression | `mBlockSize = 2`, `mBitsPerSample = 8` (Jolt's maximum). Jolt quantises twice (`HeightFieldShape.cpp` of the pinned release): first every sample to 16 bits over the tile's own range with `floor` (`scale = 0xfffe / (max − min)`, `DetermineMinAndMaxSample` and the constructor's "Quantize to uint16"), then each sample to 8 bits over its range block's 16-bit [min, max + 1], taken over the 2 × 2 block plus a one-sample border for interior blocks (`max_bx = mBlockSize + 1`). The runtime surface's error against the neutral one therefore depends on the tile's range and on every block's range; neither is bounded by ENV (cliffs are legal), so the error is **measured**, not derived: see "Measured Jolt heightfield error" below | none (exact) |
| diagonal | Jolt's split is fixed: both triangles of quad (x, y) share the edge (x, y)–(x+1, y+1) (`HeightFieldShape::ProjectOntoSurface` of the pinned release interpolates the triangles (x, y), (x, y+1), (x+1, y+1) and (x, y), (x+1, y+1), (x+1, y)), which is neutral diagonal 0 | the tessellation flag set on every quad's first sample (`PxHeightFieldSample::tessFlag`, PhysX's flag that runs a quad's diagonal from vertex (r, c) to (r+1, c+1)), which is neutral diagonal 0 |
| materials | `mMaterialIndices` = the neutral palette index per quad; `mMaterialList` = the palette's `Zenith_JoltMaterial`s plus one hole material at index `palette count` | `materialIndex0 = materialIndex1` = the neutral palette index per quad |
| holes | the quad's material index is the hole material; `Zenith_JoltContactListener::OnContactValidate` rejects every contact with it and the query collectors drop hits on it (Jolt's no-collision value is per sample and would also remove the neighbouring quads) | `materialIndex0 = materialIndex1 = PxHeightFieldMaterial::eHOLE` (PhysX's per-triangle hole) |

PhysX's 7-bit per-triangle material index, whose value 127 is `eHOLE`, is why the neutral palette limit is 127.

**Measured Jolt heightfield error.** After `HeightFieldShapeSettings::Create` succeeds, the Jolt loader computes `e_J,max` = the maximum, over every sample (s, t) of the tile (0 ≤ s, t < samples; the padded row and column are excluded, and holes are quad materials, so every sample has a height), of `|shape.GetPosition(s, t).GetY() − h_n(s, t)|`, with `HeightFieldShape::GetPosition(uint inX, uint inY)` the pinned release's per-sample reconstruction (x = s, y = t, local space, `mOffset` 0) and `h_n` the neutral decoded height `heightMin + h / 65535 × (heightMax − heightMin)`. This is an exact bound on the runtime surface's height error everywhere on the tile, not an estimate: the runtime and the neutral surfaces are both piecewise linear on the same triangles (the same diagonal, the same (x, z) vertices), so their difference is linear on each triangle and its maximum over a triangle is at a vertex. The cost is O(samples) once per load (66,049 reconstructions for a 257² tile). `e_J,max` is stored in the shape's `Zenith_PhysicsShapeInfo::m_fBackendHeightError` (PHYS/PublicAPI.md "Zenith_PhysicsShape header"), the gauge `physics.jolt.heightfield_max_error_mm` reports the largest value over the live heightfield shapes, and a shape whose value exceeds `fPHYSICS_HEIGHTFIELD_ERROR_WARN_METRES` = 0.01 m logs one warning naming the asset (category `physics`). PhysX reports 0.

`Test_PhysicsConformance_HeightfieldMatchesNeutral[<Backend>]` builds a 257² tile with a seeded height field that includes a 12 m cliff across one row of blocks and a palette of 3 with 40 hole quads, casts 4,096 seeded vertical rays and 1,024 seeded oblique rays against the runtime shape, and asserts each hit against the neutral surface (the diagonal-0 interpolation of the decoded samples): within 1e-4 m on PhysX and within `m_fBackendHeightError + 1e-4` m on Jolt, with the hit quad's palette index equal to the neutral one and no hit inside a hole quad; on Jolt it also asserts that `m_fBackendHeightError` equals an independent recomputation of the maximum over the samples; a negative control built with the opposite diagonal fails the same assertions.

## PhysX construction steps

`Zenith_CreatePhysics_PhysX(const Zenith_PhysicsDesc&)`, each step a fault point (`Test_PhysicsPhysX_Create_FailsAt<Step>_Unwinds`):

| Step | Action | Failure |
|---|---|---|
| 1 Validate | heap non-null, capacities within limits | `PHYSICS_ERROR_INVALID_DESC` |
| 2 Exclusive | `PxIsFoundationValid()` is false | `PHYSICS_ERROR_BACKEND_ALREADY_ACTIVE` |
| 3 Object | `Zenith_TryNew<Zenith_PhysicsPhysX>` (holds the allocator and error callbacks) | `PHYSICS_ERROR_OUT_OF_MEMORY` |
| 4 Foundation | `PxCreateFoundation(PX_PHYSICS_VERSION, m_xAllocator, m_xErrors)` | `PHYSICS_ERROR_BACKEND_INIT` |
| 5 Physics | `PxCreatePhysics(PX_PHYSICS_VERSION, *foundation, PxTolerancesScale{ length 1, speed 10 }, false, nullptr)` | `PHYSICS_ERROR_BACKEND_INIT` |
| 6 Extensions | `PxInitExtensions(*physics, nullptr)` | `PHYSICS_ERROR_BACKEND_INIT` |
| 7 Cooking parameters | `PxCookingParams` of "Backend shape payloads", `meshPreprocessParams` fixed | — |
| 8 Tables | shape and material handle tables | `PHYSICS_ERROR_OUT_OF_MEMORY` |
| 9 Defaults | the default material | `PHYSICS_ERROR_OUT_OF_MEMORY` |

Teardown in reverse: `PxCloseExtensions`, `release()` of the physics and the foundation objects, after every world, shape and material is released.

## PhysX world construction

`CreateWorld`: validate as Jolt does; create `Zenith_PhysXDispatcher` ("PhysX step and the dispatcher"); `PxSceneDesc` with gravity, `filterShader = Zenith_PhysXFilterShader`, `simulationEventCallback = &m_xEvents`, `cpuDispatcher = &m_xDispatcher`, `solverType = PxSolverType::eTGS`, `broadPhaseType = PxBroadPhaseType::eABP`, flags `eENABLE_ENHANCED_DETERMINISM | eENABLE_CCD | eENABLE_PCM`, `staticKineFilteringMode = PxPairFilteringMode::eKEEP` and `kineKineFilteringMode = PxPairFilteringMode::eKEEP` (PhysX's defaults kill kinematic–static and kinematic–kinematic pairs before the shader, which would hide a kinematic controller capsule from a kinematic trigger; the shader decides instead, "PhysX filter shader"), limits from the world descriptor (`maxNbActors`-style hints as capacities), `ccdMaxPasses = 1`; `PxCreateScene` → `PHYSICS_ERROR_BACKEND_INIT` on failure. Each dynamic actor gets `setSolverIterationCounts(position, velocity)`, `setSleepThreshold(0.5 × v_sleep²)` (PhysX measures kinetic energy per unit mass) and `setStabilizationThreshold(0)`.

## PhysX filter shader

Every shape carries two `PxFilterData` records set from the same body fields: the simulation filter data (`setSimulationFilterData`) and the query filter data (`setQueryFilterData`). `word0` = `1 << layerIndex`, `word1` = the layer's collision mask from the layer table, `word2` = the body's collision group (0 = none), `word3` = flags: bit 0 trigger, bit 1 report contacts or interactions (either flag, PHYS/Simulation.md "Event emission"), bit 2 continuous, bit 3 query-only, bit 4 IGNORE_SAME_GROUP. Shape flags: a trigger body's shapes are `eTRIGGER_SHAPE | eSCENE_QUERY_SHAPE`; a query-only body's shapes are `eSCENE_QUERY_SHAPE` alone, so they never enter simulation; every other shape is `eSIMULATION_SHAPE | eSCENE_QUERY_SHAPE`. CollideConnected is the joint flag of "Constraint mapping", which PhysX applies before the shader.

```
staticX = PxGetFilterObjectType(attributesX) == PxFilterObjectType::eRIGID_STATIC; kinematicX = PxFilterObjectIsKinematic(attributesX); dynamicX = !staticX && !kinematicX
if either word3 has query-only:                                   return eSUPPRESS   // defensive: such shapes never simulate
if (a.word0 & b.word1) == 0 or (b.word0 & a.word1) == 0:         return eSUPPRESS
if a.word2 != 0 and a.word2 == b.word2 and ((a.word3 | b.word3) & IGNORE_SAME_GROUP): return eSUPPRESS
if either is a trigger:
	if both are triggers:                                          return eSUPPRESS   // trigger–trigger never pairs
	if the non-trigger body is static:                             return eSUPPRESS   // triggers never see statics
	pairFlags = eTRIGGER_DEFAULT;                                  return eDEFAULT
if !dynamicA and !dynamicB:                                        return eSUPPRESS   // static–kinematic, kinematic–kinematic: no response
pairFlags = eCONTACT_DEFAULT
if either reports contacts:                                        pairFlags |= eNOTIFY_TOUCH_FOUND | eNOTIFY_TOUCH_LOST | eNOTIFY_CONTACT_POINTS
if either is continuous:                                           pairFlags |= eDETECT_CCD_CONTACT
return eDEFAULT
```

The scene's `eKEEP` filtering modes ("PhysX world construction") let kinematic pairs reach the shader. Every trigger body is kinematic (PHYS/Simulation.md "Body composition" rule 9), so the trigger rows meet the same pairs as the Jolt mapping: dynamic and kinematic non-trigger bodies, never statics, never triggers, never query-only bodies, never excluded groups or connected pairs. `Test_PhysicsPhysX_FilterShaderTruthTable` enumerates every combination of the three motion types × {plain, trigger, query-only} × layer bit × group and IGNORE_SAME_GROUP and asserts the shader's verdict against this pseudocode; the conformance cases assert the observable behaviour on both backends.

## PhysX scene query filter

`Zenith_PhysXQueryFilter` (`ZenithPhysics_PhysX/Internal/Zenith_PhysXQueries.cpp`, a `PxQueryFilterCallback` used with `PxQueryFlag::ePREFILTER | eSTATIC | eDYNAMIC`) applies `Zenith_PhysicsQueryFilter` in the order of PHYS/Simulation.md "Query execution" step 2, reading each shape's query filter data in `preFilter`: a trigger shape (word3 bit 0) passes only when `m_bIncludeTriggers`; a query-only shape (bit 3) only when `m_bIncludeQueryOnly`; any other shape when its actor's gate is set (`m_bIncludeStatic` for a `PxRigidStatic`, `m_bIncludeDynamic` for dynamic and kinematic actors); then `word0 & m_uLayerMask`; then `word2 != m_uIgnoreGroup` when the latter is non-zero; then the body's user data not in the ignore list. A rejected shape returns `PxQueryHitType::eNONE`; an accepted one `eTOUCH` for a collect-all query and `eBLOCK` for a closest query; the hits feed `Zenith_PhysicsHitCollector` exactly like Jolt's. Heightfield hole triangles produce no PhysX hit.

`Test_PhysicsConformance_QueryOnlyVisibleToQueries[<Backend>]` and `Test_PhysicsConformance_QueryIgnoreGroup[<Backend>]` assert the shared result: a query-only body is hit by a ray, a sweep and an overlap with `m_bIncludeQueryOnly` true and by none with it false, and never collides with a box falling onto it; a body of collision group 7 is skipped by a query with `m_uIgnoreGroup` 7 and hit with 0.

## PhysX step and the dispatcher

`Zenith_PhysXDispatcher` implements `PxCpuDispatcher`:

- `submitTask(PxBaseTask&)` pushes the task into a `Zenith_BoundedQueue<PxBaseTask*>` of `physics.physx.dispatchQueueCapacity` (4,096) entries guarded by `uLOCK_LEVEL_PHYSICS_DISPATCH` (short hold; cross-area row PH3); the queue's full policy is `QUEUE_FULL_POLICY_REJECT`, and on rejection the dispatcher runs the task inline (`run()`, `release()`) on the submitting thread, which is always a drain leaf or the step node, counting `physics.physx.inline_tasks_delta`.
- `getWorkerCount()` returns the W the current `StepOnce` set (below).

`StepOnce(h, xJobs, xContacts)`: set the dispatcher's worker count W = `m_uParallelLeaves` when non-zero, otherwise `xJobs.GetWorkerCount()`, or 1 when `physics.physx.parallelStep` is false (`getWorkerCount()` returns this W while PhysX splits the substep's work); `m_pxScene->simulate(h, nullptr, m_xScratch.data, m_xScratch.size)`, where the scratch block is carved from the world's step arena with what PhysX requires of `scratchMemBlock`: a 16-byte-aligned address and a size that is a multiple of 16 KiB (the arena block is rounded down to the multiple; `physics.stepScratchBytes` is validated ≥ 1 MiB); then one drain wave, `xJobs.ParallelFor(W, 1, DrainLeaf, this)`, and return `m_bPending = true`. A drain leaf pops and runs tasks until the queue is empty and returns; on the Zenith side it never waits.

**PhysX's own internal waits.** "Never waits" describes the Zenith side only. PhysX's task graph is dependency-counted (a task reaches the dispatcher when its predecessors have finished), but its parallel solver tasks also synchronise on progress counters with spin loops inside `run()`. The design relies on one property of those loops: a task spins only for work another task has already claimed and is running, never for work still queued, so progress holds at every leaf count, including one leaf running every task in turn. PhysX is not vendored in this repository, so this design cannot cite the source lines that establish the property; the gap is named and closed by two obligations. The pinning ticket audits the pinned release's `physx/source/lowleveldynamics/` and `physx/source/lowlevel/` for every spin or wait inside a task body and records each, with file and line and the reason it terminates, in this section in the ticket's own diff. The same audit records two contact-reporting facts: that an island going to sleep reports no `eNOTIFY_TOUCH_LOST` for its touching pairs (the PhysX premise of PHYS/Simulation.md "Sleeping"; if it does report them, the ticket adds the post-`fetchResults` sleep marking that section describes), and the exact names of the removed-actor and removed-shape flags "PhysX contact reporting" tests. `Test_PhysicsPhysX_DispatcherStressAcrossWorkerCounts` runs the determinism scenario with the job system restricted to 1, 2, 8 and all workers while W stays at its largest value (so leaves run serially when workers are few), under a 10 s per-step watchdog that fails the test on a livelock, and asserts identical state hashes across the four runs.

`ResumeStep(xJobs, xContacts)` runs in the continuation registered after the wave:

1. If the queue is non-empty (tasks submitted after the last leaf found it empty), start another wave and return pending.
2. Otherwise no task is running or queued, so the task graph is finished; `checkResults(false)` must be true (if not, the step is `PHYSICS_ERROR_STEP_FAILED`, counted, and the scene is reset by `fetchResults(true)` on the next call, which cannot block because nothing is outstanding).
3. `fetchResults(true)` (complete, so it returns at once); the simulation event callbacks run inside it on this thread ("PhysX contact reporting"); fill the stats and return `m_bPending = false`.

`PHYSICS_CAPABILITY_PARALLEL_STEP` is set. With enhanced determinism, the result does not depend on how many leaves ran or in which order tasks executed; `Test_PhysicsDeterminism_ReplayAcrossWorkerCounts[PhysX]` proves it on the determinism scenario at worker counts 1, 2, 8 and all.

## PhysX contact reporting

`Zenith_PhysXEvents` implements `PxSimulationEventCallback`:

| Callback | Action |
|---|---|
| `onContact` | first, per pair header: when `PxContactPairHeader::flags` has `PxContactPairHeaderFlag::eREMOVED_ACTOR_0` or `eREMOVED_ACTOR_1`, that actor pointer may point at a released actor and must not be dereferenced (PhysX's own rule; the pinning ticket confirms the flag names against the pinned release), so every pair of that header is dropped without reading either actor's `userData`, counted through `Zenith_PhysicsContactSink::CountStaleRemoved` (`physics.contacts.stale_removed_delta`), and its pairs end through the module's ended list (PHYS/Simulation.md "Event emission" step 2). Otherwise, for each pair with `eNOTIFY_TOUCH_FOUND` append `PHYSICS_CONTACT_ADDED`, with the two actors' user data and part codes from the slot table through each actor's `userData` (its body index, "Body identity on both backends") and extracted contact point 0 (`PxContactPair::extractContacts` into a 16-point stack buffer), the normal and the approach speed; `eNOTIFY_TOUCH_LOST` (or the removed-shape flag `PxContactPairFlag::eREMOVED_SHAPE_0` or `eREMOVED_SHAPE_1` of a live actor, a shape `SetBodyShape` replaced) appends `PHYSICS_CONTACT_REMOVED`, never with flag bit 1 "sleep" (PHYS/Simulation.md "Sleeping"); persisting pairs append `PHYSICS_CONTACT_PERSISTED` when interaction detection needs speeds |
| `onTrigger` | a `PxTriggerPair` whose removed-shape flag (`PxTriggerPairFlag::eREMOVED_SHAPE_TRIGGER` or `eREMOVED_SHAPE_OTHER`) belongs to a released actor is dropped by the same rule without reading its `userData`, counted `physics.contacts.stale_removed_delta`; otherwise `eNOTIFY_TOUCH_FOUND` → `PHYSICS_CONTACT_ADDED` with flag bit 0 (a trigger pair), `eNOTIFY_TOUCH_LOST` → `PHYSICS_CONTACT_REMOVED` with flag bit 0 |
| `onConstraintBreak`, `onWake`, `onSleep`, `onAdvance` | unused (breakage is the neutral layer's, sleep is read at write-back) |

PhysX's per-pair combined coefficients come from its native combine ("Material combination").

## PhysX state save and restore

PhysX has no exact snapshot of its internal state (contact caches, solver warm starting, broad-phase pairs). `SaveState` returns `PHYSICS_ERROR_UNSUPPORTED`; the module saves the slot tables and the neutral body, constraint and controller blocks, and restores by recreating bodies and constraints with their saved poses, velocities and sleep states (the COHERENT restore of PHYS/Simulation.md "Save and restore algorithm"; `uSAVE_PARTICIPANT_FLAG_COHERENT`). `PHYSICS_CAPABILITY_EXACT_STATE_RESTORE` is absent.

## PhysX error callback

`Zenith_PhysXErrorCallback::reportError(code, message, file, line)` never logs (it may run on any PhysX thread with PhysX locks held) and keeps the message text nowhere; it increments a relaxed atomic per severity in the backend object:

| PhysX code | Treatment |
|---|---|
| `eDEBUG_INFO`, `eDEBUG_WARNING`, `ePERF_WARNING` | counted `physics.physx.warnings` |
| `eINVALID_PARAMETER`, `eINVALID_OPERATION` | counted `physics.physx.misuse`; a defect: the conformance suite asserts 0, because the neutral layer validates every argument first |
| `eINTERNAL_ERROR`, `eABORT`, `eOUT_OF_MEMORY` | counted `physics.physx.errors`; the step or call in progress returns `PHYSICS_ERROR_STEP_FAILED` (or `PHYSICS_ERROR_OUT_OF_MEMORY` for the last) |

The module publishes the three as cumulative counters each tick.

## Backend behaviour differences

The conformance suite runs every scenario on both backends with backend-neutral assertions (PHYS/Tests.md "Conformance suite"); these are the documented differences it tolerates or checks per backend:

| Aspect | Jolt | PhysX | Neutral consequence |
|---|---|---|---|
| step threading | single thread (a parallel step only if a recorded decision admits it, "Conditional Jolt parallel step") | drain waves over workers | same API; PHYS-002 measured per backend |
| connected-body collision | excluded-pair group filter | `eCOLLISION_ENABLED` | identical pairs excluded ("Constraint mapping") |
| triggers | kinematic active sensors | kinematic trigger shapes, `eKEEP` filtering, shader rules | identical reach (PHYS/Simulation.md "Trigger semantics") |
| a body falling asleep | every contact of the body is reported removed and, at its wake, added again; the backend marks those removals with record flag bit 1 "sleep" | the pair persists with no report (audited by the pinning ticket) | identical events: a pair persists while a body of it sleeps (PHYS/Simulation.md "Sleeping") |
| heightfield samples | a 16-bit stage over the tile's range, then 8 bits per sample over each range block (error `e_J,max`, measured per shape at load) | exact 16-bit | tolerance per backend in the heightfield case ("Heightfield builds") |
| body creation at a reused index | `CreateBodyWithID` walks Jolt's own LIFO free list to remove the index (O(free-list length) per creation; the pinned `BodyManager::AddBodyWithCustomID` itself comments that the walk is O(N)) | O(1) | the cell-churn case of `Zenith_Bench_BudgetsPHYS` measures it under Z2-B-PHYS-002 (PHYS/Simulation.md "Slot allocation") |
| state restore | EXACT | COHERENT | the save participant flag differs |
| origin shift | per body (O(bodies)) | native `shiftOrigin` | capability `ORIGIN_SHIFT` on PhysX only |
| backend shape sections | hull, mesh, compound | hull, mesh | heightfields always neutral |
| solver | sequential impulses, Jolt's position steps | TGS | stacking tolerances are per scenario, not per backend |
| results | — | — | never compared across backends ([Z2-N-ARCH-882]) |

## PhysX on Android evaluation

The D-SEL-PHYSX-ANDROID (open, Stage 1) harness `Zenith2/Benchmarks/PhysicsBackends/` (`Zenith_Bench_PhysicsBackends`) is PHYS's Stage 1 deliverable, built before the module exists, against the two engines directly through a thin harness-local adapter (the harness is a benchmark target, which may name both manifest entries). It is two tickets, because recorded run evidence belongs only to `needs-human` tickets (the orchestrator's WORKER-EVIDENCE ruling): a loop ticket builds the harness, whose `--smoke` mode `zenith_add_benchmark` registers as a CTest (`Zenith2/Docs/Architecture/BuildSystem.md` → "Tools, benchmarks and integration suites"): on the Windows host preset every workload of the table below runs 10 ticks on each engine, the thread-pool rows at 1 and 2 threads, and the report file it writes parses against the report schema with every field present; and the `needs-human` ruling ticket runs the harness on the reference desktop and the S24, attaches the report and applies the ruling:

| Criterion (`Zenith2/Docs/Architecture.md` → "Named selection decisions") | Measurement |
|---|---|
| PhysX builds with NDK r27c and 16 KB pages | the `android-arm64` preset builds `physx` and links the harness APK; `ApkCheck` passes the 16 KB alignment check |
| passes the physics conformance suite on the S24 | the harness runs the body-level conformance scenarios' Stage 1 subset (stacking, tunnelling, triggers, queries, and the backend-level determinism check of `Test_PhysicsConformance_StepWorkerIndependent` at worker counts 1 and all) on both engines on the S24 |
| fits the physics budget rows | step CPU of W-Characters, W-Dense and W-OpenWorld scenes on the S24 against the Stage 1 values of Z2-B-PHYS-002, and resident bytes against Z2-B-PHYS-001 |
| (PHYS-1 evidence, the Q87 ruling) what a multithreaded Jolt step would save | a benchmark-only measurement of Jolt stepped with `JPH::JobSystemThreadPool` at 1, 2, 4 and all worker threads, beside the single-threaded `JPH::JobSystemSingleThreaded` figure, for W-Dense and W-OpenWorld on Windows and on the S24; the thread pool is legal only in the harness (a benchmark target, never linked by a Player or a module), its threads are the harness's own and no Zenith job runs while it steps |

The report records, per platform and workload, the single-threaded p95 step time, the thread-pool p95 at each worker count, and the verdict of the trigger of "Conditional Jolt parallel step".

## Conditional Jolt parallel step

The orchestrator ruled flagged proposal PHYS-1 (Q87) on evidence. The single-threaded Jolt step is the compliant baseline and ships unless the trigger fires; this section pre-specifies the exemption in full against the pinned release's job-system interface (`Jolt/Core/JobSystem.h` of 5.4.0), so the Stage 1 ruling ticket applies it without design work.

**Trigger.** At Stage 1 the harness report above gives the single-threaded Jolt p95 step time on the S24 for W-Dense and W-OpenWorld. If both are within the Stage 1 value of Z2-B-PHYS-002, PHYS-1 is **rejected**: the ruling ticket appends the rejection to `Zenith2/Docs/DecisionLog.md` with the measured figures, and nothing below is built. If either exceeds it, the narrow exemption is **admitted**: the ruling ticket appends the decision, with the single-threaded and thread-pool figures, and the Contracts bullet beside [Z2-N-CONTRACT-365] and the lock rows 520 and 530 below go through the core-doc route (core-doc item X264 (m)); the Jolt backend ticket that follows builds `Zenith_JoltJobSystem` exactly as below.

**Why not `JPH::JobSystemWithBarrier`.** Its barrier, `JobSystemWithBarrier::BarrierImpl`, is a private nested class whose `Wait` acquires a semaphore (`JobSystemWithBarrier.h`, `JobSystemWithBarrier.cpp` of the pinned release), so a subclass that overrides `WaitForJobs` can neither read the barrier's jobs nor avoid the semaphore. `Zenith_JoltJobSystem` therefore derives from `JPH::JobSystem` directly and implements its own barrier, the route Jolt's header documents for "your own job system".

**Types.** `Zenith_JoltJobSystem final : public JPH::JobSystem` (`ZenithPhysics_Jolt/Internal/Zenith_JoltJobSystem.h/.cpp`), one per Jolt world, owned by the world object and passed to every `PhysicsSystem::Update` of that world in place of `JPH::JobSystemSingleThreaded`; its nested `Zenith_JoltBarrier final : public JPH::JobSystem::Barrier` (nested in the derived class, so both may name JPH's protected `JobSystem::Job`). State: a fixed pool of `W_jobs` job slots (`physics.jolt.maxJobs`, INT, default 2,048 = `JPH::cMaxPhysicsJobs`, range 256 … 65,536, restart; a key the Jolt backend ticket registers only if PHYS-1 is admitted) with a free list; a `Zenith_BoundedQueue<JPH::JobSystem::Job*>` of the same capacity, `QUEUE_FULL_POLICY_REJECT`; both guarded by one short-hold mutex at lock level 520 `uLOCK_LEVEL_PHYSICS_JOLT_JOBS` (owner ZenithPhysics_Jolt; guards the job pool's free list and the queue's push and pop, and nothing runs under it; not same-level by address; acquired on the step node's thread and the drain leaves; the row joins the lock table through X264 (m)); `std::atomic<uint32> m_uActiveLeaves`; the step node's `Zenith_JobContext*` for the duration of one `StepOnce`; the world's one barrier.

**Members.** The eight overrides Jolt's header lists for a custom job system, plus the barrier's three:

| Member | Implementation |
|---|---|
| `GetMaxConcurrency()` | `W_J`, fixed at world creation: `Zenith_PhysicsWorldDesc::m_uParallelLeaves` when non-zero, else `uPHYSICS_JOLT_JOB_CONCURRENCY` = 8. Never the run's worker count, so the number of jobs Jolt creates per update, and the work each one holds, are the same at every worker count |
| `CreateJob(name, colour, function, dependencies)` | construct a `Job` in a free pool slot (taken under lock 520) and wrap it in a `JobHandle`; when `dependencies` is 0, `QueueJob` it at once (the rule of Jolt's own `JobSystemThreadPool::CreateJob`). An exhausted pool is a defect (`ZENITH_ASSERT`): one `PhysicsSystem::Update` holds at most `JPH::cMaxPhysicsJobs` live jobs and the pool has at least that many slots |
| `FreeJob(job)` | destroy the job in place and return its slot under lock 520 (Jolt calls it from `Job::Release` when the last reference goes) |
| `QueueJob(job)`, `QueueJobs(jobs, n)` | `AddRef()` each job (Jolt's rule for a job stored in the system's own structure) and push them under lock 520 (the queue cannot be full: it has a slot for every live job); nothing else. Jolt calls them with a `Job*` alone, from `Update` on the node's thread or from inside a running job, so they cannot know which `Zenith_JobContext` they run under; the spawn rule therefore runs only where the context is known (below) |
| `CreateBarrier()` | return the world's barrier with its counter at 0; `ZENITH_ASSERT` that it is not in use (one `Update` creates exactly one barrier) |
| `DestroyBarrier(barrier)` | mark it unused |
| `WaitForJobs(barrier)` | "The wait" below |
| `Zenith_JoltBarrier::AddJob(handle)`, `AddJobs(handles, n)` | for each job: `m_uPending.fetch_add(1, relaxed)` **first**, then `SetBarrier(this)`; when `SetBarrier` fails (the job already finished), `m_uPending.fetch_sub(1, relaxed)`. Incrementing before the job can be seen as belonging to the barrier means a job added from inside a running job (`PhysicsSystem.cpp:812`, `:1649` of the pinned release) and finished at once by another leaf can never make its `OnJobFinished` decrement land before the increment, so the counter never reads 0 while a job of the barrier is unfinished (it may read one too many for an instant, which only makes the waiter loop once more); the barrier keeps no job pointer, so it takes no reference |
| `Zenith_JoltBarrier::OnJobFinished(job)` | `m_uPending.fetch_sub(1, release)` |

**Mapping dynamically queued jobs onto Zenith sub-jobs.** Jolt creates jobs inside `Update` and queues more from inside running jobs as their dependencies resolve, so no job count is known when a sub-job is spawned. The mapping is therefore a set of drain leaves that grows with the queue:

1. `StepOnce` stores `&xJobs` and calls `m_xSystem.Update(h, 1, &m_xTempAllocator, &m_xJobSystem)` on the step node's thread. `Update` builds its job graph (the zero-dependency jobs reach `QueueJob` at once) and ends in `WaitForJobs(barrier)`.
2. **Spawn rule.** It runs at exactly three places, each of which holds its own `Zenith_JobContext`: in `WaitForJobs` at entry (covering every job `Update` queued while building its graph) and after each job the waiter runs, both through the step node's context; and in each drain leaf after each job it runs, through that leaf's own context (a leaf receives its context, so no thread-local lookup exists; sub-jobs that spawn sub-jobs are joined before the node completes, `Zenith2/Docs/Architecture/CPUFrameGraph.md` → "ParallelFor"). A job queued from inside a running job is therefore seen by the spawn rule when the thread running that job finishes it. The rule: compute `deficit = min(queued jobs, W_J − 1) − m_uActiveLeaves` (the queue length read under lock 520; one thread of the W_J is the waiter); when it is positive, add it to `m_uActiveLeaves` and spawn that many leaves with `ParallelFor(deficit, 1, Zenith_JoltDrainLeaf, this)`.
3. **A drain leaf** pops one job under lock 520, runs `job->Execute()` outside the lock, calls `job->Release()`, applies the spawn rule, and repeats. A leaf that finds the queue empty decrements `m_uActiveLeaves` and returns at once: it never waits, so a leaf spawned before `Update` queued everything, or after the last job was taken, costs one empty pop. Jobs queued afterwards are taken by the leaves still running, by new leaves the spawn rule adds, or by the waiter.
4. Every leaf has returned before the node completes ([Z2-N-ARCH-395]); a leaf still returning when the next substep's `Update` starts may take that substep's jobs, which belong to the same world's next barrier and are equally its to run.

**The wait.** `WaitForJobs(barrier)` runs on the step node's thread inside `Update`:

1. `const bool bWasJobBody = Zenith_IsJobWorkerThread();`, `Zenith_MarkThreadAsJobWorker(false)` (the bracket of [Z2-N-CONTRACT-365]); then the spawn rule runs once through the node's context.
2. Loop while `barrier.m_uPending.load(acquire) != 0`: pop a job under lock 520; when one is found, run it as a leaf does (help-while-wait; every queued job belongs to this world's one live barrier, so the waiter runs only its own barrier's jobs, the rule Jolt's header states for `WaitForJobs`); when the queue is empty, some of the barrier's jobs are running on leaves or are waiting for a running job to release a dependency, so call `Zenith_YieldThread()` (`ZenithCore/Zenith_Multithreading.h`) and check again.
3. `Zenith_DispatchBlockingWait("physics-barrier", ulWaitTicks)`, called once after the loop of step 2 has returned, with `ulWaitTicks` the measured time of that loop (0 only if the waiter did not wait); then `Zenith_MarkThreadAsJobWorker(bWasJobBody)`.

While waiting it holds no lock across a check and calls no blocking primitive (none of the condition-variable, semaphore, join or long-hold-lock waits [Z2-N-CONTRACT-315] lists). Progress holds at every worker count: the waiter spins only while the queue is empty and the counter is non-zero, which means at least one of the barrier's jobs is executing on another thread, and every job that becomes ready is queued and taken by a leaf or by the waiter itself; with one worker the waiter runs every job in turn. Jolt's own in-job spins terminate as well: `JobSolveVelocityConstraints` and `JobSolvePositionConstraints` call `std::this_thread::yield()` while `LargeIslandSplitter` reports `WaitingForBatch` (`PhysicsSystem.cpp:1465`, `:2483` of the pinned release; the soft-body loop at `:2629`), and the splitter returns `WaitingForBatch` only while a batch another running job has claimed is unprocessed, so each spin waits only for work that is already executing, at every leaf count; the watchdog of `Test_PhysicsJolt_ParallelStepStressAcrossWorkerCounts` covers them.

**Contact listener under the parallel step.** With `Zenith_JoltJobSystem`, Jolt's collision jobs run `Zenith_JoltContactListener`'s callbacks on the drain leaves and the waiter concurrently. Every write a callback makes to shared state is made under one short-hold mutex of the listener at lock level 530 `uLOCK_LEVEL_PHYSICS_CONTACT_SINK` (owner ZenithPhysics_Jolt; guards the world's contact sink's heap and count of dropped records and the listener's removed-contact staging; nothing runs under it and no other lock is taken under it; not same-level by address; acquired on the drain leaves and the waiter, always with nothing held, so never together with 520, which is never held across `Execute`; the row joins the lock table through X264 (m) with 520): `OnContactAdded` and `OnContactPersisted` take it around `Zenith_PhysicsContactSink::Append`, and `OnContactRemoved` around its staging push. `OnContactValidate` and the material combination take no lock: they read only data that is immutable during the step ("Jolt contact listener"). The resolution of the staging after `Update` runs on the step node's thread alone, with no lock. The stored set is deterministic: the sink keeps the capacity's lowest record keys whatever the arrival order, and the staging is resolved entry by entry. With the single-threaded job system the mutex is never taken (the step thread is the only caller), so lock 530 exists only if PHYS-1 is admitted. Per-callback staging that the waiter merges was the alternative; the lock is simpler and the hold is a heap insertion.

**The rest of the exemption.**

- Scope: `Zenith_JoltJobSystem.h/.cpp` alone; no other Zenith2 code uses the bracket. The listener's lock 530 lives in `Zenith_JoltContactListener.cpp`, is taken only while `PHYSICS_CAPABILITY_PARALLEL_STEP` is set, and is no part of the bracket.
- `PHYSICS_CAPABILITY_PARALLEL_STEP` becomes set on Jolt; `mDeterministicSimulation = true` and the fixed `W_J` keep the result independent of the worker count (rule D4 of PHYS/Simulation.md "Determinism rules").
- Tests in the same ticket: `Test_PhysicsJolt_ParallelStepStressAcrossWorkerCounts` (`Pile1000` and `Joints` through the conformance harness, and `Ragdoll8` through the integration test world, for 900 ticks at worker counts 1, 2, 8 and all under a 10 s per-step watchdog, asserting no hang, identical state hashes across the four counts, that the hook's report count equals the number of `WaitForJobs` calls, that every leaf returned, and that the pool and queue are empty after each step; and, with `physics.maxContactRecordsPerTick` set to 256 so the sink drops records in most ticks, that the sorted kept-record list of every tick and the dropped count are identical across the four worker counts), `Test_PhysicsJolt_JobSystemQueuesReadyJobs` (a hand-built job graph of 200 jobs with chained dependencies queued from inside jobs: every job runs once, `OnJobFinished` brings the barrier to 0, `FreeJob` returns every slot), and `Test_PhysicsDeterminism_ReplayAcrossWorkerCounts[Jolt]`, already in the integration suite, at worker counts 1, 2, 8 and all with identical hashes; `Test_JobSystem_NoBlockingWaitInJobs` excludes exactly `"physics-barrier"` besides `"debugger-stop"`.

Until the ruling no ticket builds `Zenith_JoltJobSystem`; Z2-R-PHYS-133 states both branches.

The harness report (`Zenith2/out/<checkout-hash>/bench/PhysicsBackends/<platform>.json`) is the ruling's evidence; the human ruling ticket appends the decision to `Zenith2/Docs/DecisionLog.md` and, if PhysX is not admitted on Android, makes the manifest and `ZENITH_SUPPORTED` edits of "Third-party pins".
