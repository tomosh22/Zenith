# PHYS design — simulation, events, queries and determinism

This file is part of the PHYS design family; the entry document is `Zenith2/Docs/Design/PHYS.md`. It specifies how the Physics module runs a world's simulation inside the frame phases: the frame-graph nodes and their declared access, the module's per-world state, how ECS components become bodies, the ingest of commands and external writes, the fixed substep loop and its cap, the write-back to components and transforms, contact, trigger and interaction events with their deterministic order, constraints and breakage, continuous collision, sleeping, kinematic bodies and moving platforms, the streaming freeze and origin shift, buoyancy, query execution and deferred queries, the determinism rules and boundaries, large coordinates, save and restore, and the state machines. The character controller, ragdolls and vehicles are `Zenith2/Docs/Design/PHYS/CharacterRagdollVehicle.md`; backend mechanics are `Zenith2/Docs/Design/PHYS/Backends.md`.

## Scope of this file

| Topic | Heading |
|---|---|
| nodes, phases, access | "Frame-graph placement" |
| the module's per-world data | "Module world state", "Ragdoll part identity" |
| components to bodies | "Body composition", "Ingest algorithm", "Excluded pairs maintenance", "External writes", "Pending operations" |
| the step | "Substep loop", "Write-back", "Physics-to-transform sync" |
| events | "Event emission", "Trigger semantics", "Interaction detection" |
| constraints | "Constraint kinds", "Joint breakage" |
| motion features | "Continuous collision detection", "Sleeping", "Kinematic bodies and moving platforms", "Buoyancy algorithm" |
| streaming | "Region freeze", "Origin shift", "Cell bodies" |
| queries | "Query execution", "Primitive cache", "Deferred queries" |
| determinism | "Determinism rules", "Determinism boundaries", "Large coordinates" |
| persistence | "Save and restore algorithm", "Slot allocation" |
| state machines | "Body lifecycle state machine", "Constraint state machine", "Deferred query state machine" |

## Frame-graph placement

Every PHYS node is a `Zenith_SystemDesc` registered by `AttachWorld` with `JOB_AFFINITY_ANY` (`Zenith2/Docs/Design/ECS/SystemAPI.md` → "Zenith_System header"); the debug stage is an epilogue stage. Property lists name the IDs of `Zenith2/Docs/Design/PHYS/ComponentsAPI.md`; "by name" means the type and property IDs are resolved from the frozen type registry at `AttachWorld` by stable name, and the declaration is omitted when the type is not registered (a game without the module that owns it).

| Node | Phase | Writer source | Declared access | Sub-jobs | Cost hint |
|---|---|---|---|---|---|
| `Physics.CharacterMove` | 4 MovementArbitration | — | `Write` CharacterController 30–39; `Read` CharacterController 1–23; `Read` `Zenith.Animation.RootMotion` 1–3 by name; `Read` `Zenith.LocalTransform` 1–2 (own row); `ReadResource` `Zenith.Physics.World`; `EmitsCommands` | `ParallelFor` over controller rows, grain 4 | 200 |
| `Physics.Step` | 5 Physics | — | `Write` RigidBody 20–23, Joint 21, Vehicle 30–32; `Read` RigidBody 1–14, Collider 1–8, Joint 1–20, CharacterController 1–15 and 35–36, Ragdoll 3–5, Vehicle 1–23, Buoyancy 1–5, `Zenith.LocalTransform` 1–3 (own row); `ReadAnyRow` `Zenith.WorldTransform` 1–7 (parents and bodies' entities); `Read` `Zenith.Streaming.RegionBound` 2 by name; `ReadResource` `Zenith.WorldOrigin` by name, `Zenith.FluidQuery` by name, `Zenith.Animation.World` and `Zenith.Animation.Residency` by name (the pose source looks every instance up through ANIM's residency, row AN6 (f)); `WriteResource` `Zenith.Physics.World`, `Zenith.CollisionQuery`; `EmitsEvent` the six PHYS events | ingest and write-back `ParallelFor` over chunks (grain 1 chunk); PhysX drain leaves | 2000 |
| `Physics.SyncTransforms` | 6 PostPhysics | `WRITER_SOURCE_PHYSICS` | `Write` `Zenith.LocalTransform` 1–2; `Read` RigidBody 1, CharacterController 35–36; `ReadAnyRow` `Zenith.WorldTransform` 1–7 (the parents' rows, with their change versions, "Physics-to-transform sync"); `ReadResource` `Zenith.Physics.World` | `ParallelFor` over chunks | 300 |
| `Physics.RagdollBlend` | 6 PostPhysics | `WRITER_SOURCE_RAGDOLL` | `Write` Ragdoll 1–2, `Zenith.LocalTransform` 1–2; `Read` Ragdoll 3–5; `ReadAnyRow` `Zenith.WorldTransform` 1–7 (the parents of parented characters, PHYS/CharacterRagdollVehicle.md "Ragdoll blend and root"); `ReadResource` `Zenith.Physics.World`; `EmitsCommands` | none (at most `physics.maxActiveRagdolls` rows) | 50 |
| `Physics.DeferredQueries` | 6 PostPhysics | — | `ReadResource` `Zenith.Physics.World`; `After("Physics.SyncTransforms")` | `ParallelFor` over the tick's queries, grain 8 | 50 |
| `Physics.DebugDraw` | epilogue stage (stage 12) | — | `ReadResource` `Zenith.Physics.World`; `Read` CharacterController 1–2, 30–36, Ragdoll 1–2 | none | 100 |

- Sync-point work: the command playback functions of PHYS.md "Command kinds" run at step 7 of every sync point in merge order, each finding its world's module state by `xWorld.GetWorldTag()` ("Module world state"); the lifecycle listener runs at step 7 and at the end-of-tick playback (`LIFECYCLE_EVENT_DESTROYED`), and at the end of step 7 of the sync point whose `Scene.ShiftWorldOrigin` plays (`LIFECYCLE_EVENT_ORIGIN_SHIFTED`, "Origin shift"), so the backend world is shifted before any node of the next phase reads it.
- `Physics.DebugDraw` is registered only when `Zenith_IsShippingBuild()` is false and the module start context offers the debug-draw recorder (PHYS.md "Assumed interfaces").
- `Physics.Step` depends on every phase-4 writer of controller state by the phase boundary, so it always sees this tick's targets; `Physics.SyncTransforms` and `Physics.RagdollBlend` write `Zenith.LocalTransform` with distinct writer sources, which the precedence arbitration of `Zenith2/Docs/Architecture/FramePhases.md` → "Arbitration rules" merges (ragdoll over physics; a cinematic over both).
- ANIM's `Animation.RagdollOwnership` and `Animation.PostPhysicsBlend` read Ragdoll 1–2 in phase 6 after `Physics.RagdollBlend` writes them (same-phase readers after the writer, ordered by the hazard rule of `Zenith2/Docs/Architecture.md` → "CPU frame graph and scheduler" and by their `After("Physics.RagdollBlend")`), and `Animation.Significance` reads Ragdoll 1 State in phase 3 (the value phase 6 of the previous tick or a sync point wrote), row AN6 (f).
- The ragdoll enter playback calls `Zenith_RagdollPoseSource::GetEntryModelPose` on the sync-point node, which runs alone, so it needs no declaration; ANIM's implementation reads `Zenith.Animation.Residency` and `Zenith.Animation.FinalPoses` there (row AN6 (f)).
- Root motion (ruling Q72): `Physics.CharacterMove` is the reader that applies `Zenith.Animation.RootMotion` (properties 1 DeltaPosition, 2 DeltaRotation and 3 Flags, published by `Animation.Evaluate` in phase 3, `Zenith2/Docs/Design/ANIM/Graph.md` → "Root motion publication") to every entity that carries `Zenith.Physics.CharacterController`, moving it through collision; the ordering edge is the phase boundary 3 → 4 with sync point 3, after which the delta is final. Entities without a controller are moved by ANIM's own phase-4 node; the two nodes act on disjoint entity sets (ANIM's query carries `WITHOUT Zenith.Physics.CharacterController`, cross-area row PH12) and write disjoint properties, so no intra-phase edge between them is declared or needed. GAME registers no phase-4 node (row GA15).

## Module world state

`Zenith_PhysicsWorldState` (`ZenithPhysics/Internal/World/Zenith_PhysicsWorldState.h`) is one per world, created by `AttachWorld` and destroyed by `DetachWorld`; every container below is a CORE container from the root heap, sized at attach from the configuration keys, and none grows in a tick.

| Member | Type | Content |
|---|---|---|
| `m_xBackendWorld` | `Zenith_Owned<Zenith_PhysicsWorld>` | the backend world |
| `m_xResource` | `Zenith_PhysicsWorldResource` | the world resource |
| `m_xCollisionQuery` | `Zenith_PhysicsCollisionQuery` | GAME's `Zenith.CollisionQuery` |
| `m_xBodies` | `Zenith_HashMap<uint64, Zenith_PhysicsBodyRecord>` | simulation ID → body record of every body entity, implicit static and split trigger child; iteration only through `m_aulBodyOrder`. Controller capsules live in `m_xControllers` and ragdoll parts in `m_xRagdolls`, never here ("Ragdoll part identity") |
| `m_aulBodyOrder` | `Zenith_Array<uint64>` | every simulation ID with a body, ascending; maintained by sorted insert and remove |
| `m_xConstraints` | `Zenith_HashMap<uint64, Zenith_PhysicsConstraintRecord>` | joint entity simulation ID → record, plus `m_aulConstraintOrder` |
| `m_xPendingOps` | `Zenith_Array<Zenith_PhysicsPendingOp>` | commands played back since the last ingest, in playback (merge) order |
| `m_xContacts` | `Zenith_PhysicsContactSink` | this tick's contact records |
| `m_xPairs` | `Zenith_Array<Zenith_PhysicsPairRecord>` | touching body pairs, sorted by (low simulation ID, high simulation ID) |
| `m_xTriggerPairs` | `Zenith_Array<Zenith_PhysicsPairRecord>` | trigger overlaps, sorted the same way |
| `m_xControllers` | `Zenith_HashMap<uint64, Zenith_PhysicsControllerRecord>` | character simulation ID → controller record: `m_xCapsule` (the capsule body handle), `m_xWrittenLocalPosition` / `m_xWrittenLocalRotation` (the exact `Zenith.LocalTransform` 1–2 bits `Physics.SyncTransforms` last wrote, `L_w` of PHYS/CharacterRagdollVehicle.md "Character move algorithm" step 1), `m_ulParentVersion`, `m_xLastTargetPosition` / `m_xLastTargetRotation` (the kinematic capsule's last targets), `m_fCarriedYawRate` (the ground body's yaw rate step 4 last applied) and `m_bCapsuleQueryOnly` (the capsule's filtering while a ragdoll is not NONE); every field is saved (PHYS/Formats.md "Save section layout") |
| `m_xRagdolls` | `Zenith_HashMap<uint64, Zenith_PhysicsRagdollRecord>` | character simulation ID → live ragdoll instance: state, `w₀`, k, blend seconds, captured recover root, `m_bFrozen` ("Region freeze"), and per part (definition order) the body handle, the constraint handle to its parent part, that constraint's priority, the part's motion type before a freeze and its velocities saved while frozen ("Ragdoll part identity"), plus `m_aulRagdollOrder` (ascending) |
| `m_xVehicles` | `Zenith_HashMap<uint64, Zenith_PhysicsVehicleRecord>` | vehicle internal state (engine, clutch, wheel integrators) |
| `m_xDeferredInbox` | `Zenith_Array<Zenith_PhysicsDeferredQuery>` | committed deferred queries, in commit (completion-ID) order |
| `m_xStepArena` | `Zenith_Arena` | step scratch, `physics.stepScratchBytes`, reset at the start of each `Physics.Step` |
| `m_aulExcludedPairs` | `Zenith_Array<uint64>` | the body pairs that never collide because a constraint between them has CollideConnected false (ragdoll joints included): each entry `(low body index << 32) | high body index`, sorted ascending, rebuilt by ingest step 12 when `m_bExcludedPairsDirty` is set and handed to the backend with `SetExcludedPairs` ("Excluded pairs maintenance"; PHYS/Backends.md "Constraint mapping"); capacity `physics.maxConstraints` |
| `m_bExcludedPairsDirty` | `bool` | set by every creation or destruction of a constraint with CollideConnected false on any path, and by a restore; cleared by ingest step 12 ("Excluded pairs maintenance") |
| `m_aulEndedPairIDs` | `Zenith_Array<uint64>` | the **ended list**: simulation IDs all of whose bodies left the simulation since the last event emission, so every pair and trigger pair of each ends at the next emission without a record ("Event emission" step 2); kept sorted ascending with duplicates removed (sorted insert); capacity `physics.maxBodies`. Appended by every path that removes all of an ID's simulated bodies: ingest step 3 (removals), ingest step 6 (a controller capsule removed), ingest step 10 (a body or a ragdoll frozen), the `LIFECYCLE_EVENT_DESTROYED` listener at the end-of-tick playback, the `Physics.EnterRagdoll` playback (the character's capsule becomes query-only, PHYS/CharacterRagdollVehicle.md "Ragdoll enter procedure" step 7) and the `Physics.RemoveRagdollBodies` playback (the parts destroyed); cleared by "Event emission" step 2; saved and restored (PHYS/Formats.md "Save section layout") |
| `m_xBuoyancySamples` | `Zenith_HashMap<uint64, Zenith_PhysicsBuoyancySamples>` | simulation ID → the buoyancy samples of every body entity with `Zenith.Physics.Buoyancy` whose shape is not a single cooked collider: `{ uint32 m_uCount; Zenith_Vector4 m_axSamples[uPHYSICS_MAX_BUOYANCY_SAMPLES]; }` (local position xyz, volume), computed by ingest step 4 from the composed shape ("Buoyancy algorithm"); a pure function of the components, so it is not saved; capacity `physics.maxBodies`, looked up, never iterated |
| `m_uOriginRebaseCount` | `uint32` | the `Zenith.WorldOrigin::m_uRebaseCount` value whose shift the world last applied ("Origin shift") |
| `m_adAppliedOrigin` | `double[3]` | the `Zenith.WorldOrigin` origin (m_dX, m_dY, m_dZ) after the shift the world last applied: the shift of a missed notification is the difference between the resource's origin and this value, exact in binary64 because both are sums of integer shifts ("Origin shift") |
| `m_ulLastTick` | `uint64` | the tick index of the last step |

The module instance keeps the per-world states in `Zenith_PhysicsModule::m_xWorldStates`, a `Zenith_HashMap<uint16, Zenith_PhysicsWorldState*>` keyed by the world tag, filled by `AttachWorld` and emptied by `DetachWorld`. A command playback function receives the module instance as `m_pUserData` and finds its world with `m_xWorldStates.Find(xWorld.GetWorldTag())` (`Zenith2/Docs/Architecture/ECSModel.md` → "Playback access and supporting types", `Zenith_WorldPlaybackAccess::GetWorldTag`); a tag with no state (the world detached between recording and playback, which a sync point never sees) drops the command and counts `physics.command.no_world_delta`. The map is looked up, never iterated.

```cpp
// ZenithPhysics/Internal/World/Zenith_PhysicsWorldState.h (excerpt)
struct Zenith_PhysicsBodyRecord                     // 128 bytes (static_assert beside the definition)
{
	Zenith_PhysicsBodyHandle m_xBody;
	Zenith_PhysicsShapeHandle m_xShape;             // the reference this body holds
	Zenith_EntityHandle m_xEntity;
	uint64 m_ulShapeHash = 0;                      // Zenith_Hash64 of the composition inputs (Body composition)
	Zenith_Vector3 m_xWrittenLocalPosition;         // the exact Zenith.LocalTransform 1 bits PHYS last wrote or adopted (External writes)
	Zenith_Quaternion m_xWrittenLocalRotation;      // the exact Zenith.LocalTransform 2 bits
	Zenith_Vector3 m_xWrittenLinearVelocity;        // the exact RigidBody 20 bits PHYS last wrote
	Zenith_Vector3 m_xWrittenAngularVelocity;       // the exact RigidBody 21 bits
	Zenith_Vector3 m_xFrozenLinearVelocity;         // saved while frozen
	Zenith_Vector3 m_xFrozenAngularVelocity;
	uint64 m_ulParentVersion = 0;                  // the parent's Zenith.WorldTransform change version at the last local write (0: a root)
	Zenith_PhysicsMotionType m_eMotionType;
	uint8 m_uState;                                // Zenith_PhysicsBodyRecordState ("Body lifecycle state machine")
	uint8 m_uFlags;                                // bit 0 implicit static, bit 1 controller, bit 2 reserved (0), bit 3 frozen, bit 4 trigger, bit 5 has parent,
	                                                //   bit 6 asleep after the last step (RigidBody 22 as write-back last wrote it; "Sleeping")
	uint8 m_uSubstepsMoved;
};

struct Zenith_PhysicsPairRecord                     // 56 bytes (static_assert beside the definition)
{
	uint64 m_ulLow = 0;                            // simulation IDs, low < high (never equal: two bodies of one simulation ID form no pair, "Event emission")
	uint64 m_ulHigh = 0;
	uint32 m_uSubShapeContacts = 0;                // sub-shape contacts of the pair, the suspended ones included
	uint16 m_uSuspendedContacts = 0;               // of those, the contacts a sleeping body suspended ("Sleeping"); never above m_uSubShapeContacts
	uint16 m_uImpactCooldown = 0;                  // ticks
	uint8 m_uFlags = 0;                            // bit 0 began this tick, bit 1 ended this tick, bit 2 sliding, bit 3 ended by removal this emission
	uint8 m_uReserved[3] = {};
	Zenith_Vector3 m_xPoint;
	Zenith_Vector3 m_xNormal;
};
```

The hash maps exist for O(1) lookup alone; no algorithm iterates a hash map, so no result depends on bucket layout ([Z2-N-ARCH-884]).

## Ragdoll part identity

A ragdoll part is a body with no entity of its own. Its identity is defined once, here, and every table, key, event, query and save block uses it:

| Aspect | Rule |
|---|---|
| user data | the character entity's simulation ID (`Zenith_PhysicsBodyDesc::m_ulUserData`), the same value its controller capsule carries |
| part code | `Zenith_PhysicsBodyDesc::m_uRagdollPart` = part index + 1 (1 … 64, definition order); 0 for every other body (entity bodies, implicit statics, trigger children, controller capsules). The backend keeps it per body slot beside the user data (`Zenith_PhysicsWorldBase`, PHYS/Backends.md "Body identity on both backends") and stamps it into every contact record it appends (`Zenith_PhysicsContactRecord::m_uPartA`, `m_uPartB`) |
| module tables | the part's body handle, its constraint handle to the parent part and that constraint's priority are fields of the character's `Zenith_PhysicsRagdollRecord` in `m_xRagdolls`, in part order; a part has no `m_xBodies` record and no flag there |
| contact record key | ordered by (user data, part code): body A is the body with the lower pair, and the key of "Event emission" step 1 carries both part codes, so two parts touching one ground triangle have distinct keys and the key is a total order of every record a step can append (D5) |
| pairs and events | a record whose two user data are equal (two parts of one character, or a part and its own capsule) is a **self-contact**: it updates no pair, raises no event and is counted `physics.contacts.self_delta`; a record between a part and another entity updates the pair (character, other) by simulation IDs alone, so a ragdoll's pair with the floor begins when its first part touches and ends when its last part leaves, with EntityA ≠ EntityB always |
| query hits | a hit on a part maps to the character's entity (its user data); the collector key (distance, user data, sub-shape, body index) stays total because each part has its own body index |
| excluded pairs | each part's constraint to its parent is a CollideConnected-false constraint of "Excluded pairs maintenance", with the part's and the parent part's body indices |
| save | the ragdolls block of PHYS/Formats.md "Save section layout" records each part's handles, priority, motion type, pose, velocities and sleep state, and "Save and restore algorithm" re-creates them |

While a character's State is not NONE its parts are its only simulated bodies: the controller capsule is query-only (PHYS/CharacterRagdollVehicle.md "Ragdoll enter procedure" step 7), and an enter on an entity that also carries `Zenith.Physics.RigidBody` is refused. Every pair of the character's simulation ID therefore comes from its parts, which is what lets the removal of the parts end all of them through the ended list (`m_aulEndedPairIDs`, "Event emission" step 2).

**One body per (user data, part code).** A controller capsule carries its character's simulation ID with part code 0, the pair an entity body or an implicit static of the same entity would also carry. A controller is therefore refused on an entity that carries `Zenith.Physics.RigidBody` or its own `Zenith.Physics.Collider` ("Body composition" rule 10), so no two live bodies ever share (user data, part code), and the record key stays a total order with controllers present. Colliders on the character's child entities are unaffected: they have their own simulation IDs.

## Body composition

A world's bodies are a pure function of its components:

1. An entity with `Zenith.Physics.RigidBody` is a **body entity**.
2. An entity with `Zenith.Physics.Collider` and no body entity among itself and its ancestors is an **implicit static body** (motion type STATIC, layer = the collider's StaticLayer).
3. A collider entity without a rigid body whose nearest ancestor body entity is B is a **child collider** of B.
4. B's shape is a compound of its own collider (if any) and its child colliders, ordered by ascending simulation ID; with exactly one collider and an identity local transform relative to B the shape is that collider's shape directly. More than `uPHYSICS_MAX_COMPOUND_CHILDREN` children: the first 64 by simulation ID are used and `physics.body.children_truncated_delta` counts the rest.
5. A child's local transform relative to B is `inverse(W_B) · W_child · T(collider LocalPosition, LocalRotation)`, computed from the previous tick's `Zenith.WorldTransform` rows (the hierarchy is propagated in phase 8), so composition never reads an in-flight transform.
6. The entity's world scale (the `Zenith.WorldTransform` scale) multiplies primitive dimensions: a sphere's radius by the largest absolute axis scale, a box's half extents per axis, a capsule's or cylinder's radius by the larger of |sx|, |sz| and its half height by |sy|; cooked shapes take the scale as `Zenith_PhysicsCookedShapeView::m_xScale`.
7. `m_ulShapeHash` is `Zenith_Hash64` over the ordered list of (child simulation ID, shape kind, size, shape asset ID, material asset ID, local position, local rotation, scale) as bytes; ingest rebuilds the shape exactly when the hash changes.
8. A trigger: a body entity whose own collider has IsTrigger gets `PHYSICS_BODY_FLAG_TRIGGER`; a trigger child collider of a non-trigger body is split off into its own kinematic trigger body that follows B (the backends cannot mix sensor and solid children in one body), counted in `physics.body.trigger_children` (gauge).
9. Every trigger body is created KINEMATIC and `PHYSICS_MOTION_QUALITY_DISCRETE` whatever its entity's MotionType, its MotionQuality (RigidBody property 6) or its implicit-static origin (a static trigger is a kinematic trigger whose target never moves; Jolt: "A sensor can only use the discrete motion quality type at this moment", the "Sensors" section of the pinned release's architecture document), with `PHYSICS_BODY_FLAG_ALLOW_SLEEPING` cleared, so it stays active. That is the one configuration in which both backends report overlaps with sleeping bodies (Jolt: "When you make a sensor Kinematic or Dynamic and activate it, it will also detect collisions with sleeping bodies", the "Sensors" section of the pinned Jolt release's own architecture document; PhysX: trigger pairs persist while their shapes overlap) and in which the reach rules of "Trigger semantics" hold identically (PHYS/Backends.md "Jolt layer mapping", "PhysX filter shader"). A DYNAMIC RigidBody whose own collider IsTrigger is therefore simulated as a kinematic trigger that follows its entity's composed pose and never falls; the scene validation of PHYS.md "Spawn clearance rule" warns about it (`PHYSICS_COOK_DIAG_DYNAMIC_TRIGGER`).
10. An entity with `Zenith.Physics.CharacterController` that also carries `Zenith.Physics.RigidBody` or its own `Zenith.Physics.Collider` gets **no controller**: its body (rules 1–2) is composed as usual, its controller row is refused at ingest step 6 (on creation and on every change of either component) and counted `physics.controller.invalid_delta`, a refusal row of PHYS/CharacterRagdollVehicle.md "Controller tuning rules"; the scene validation of PHYS.md "Spawn clearance rule" warns about it (`PHYSICS_COOK_DIAG_CONTROLLER_WITH_BODY`). This keeps one body per (user data, part code) ("Ragdoll part identity"); a character puts its hit volumes and interaction triggers on child entities.

## Ingest algorithm

The first stage of `Physics.Step`, on the node's thread and its sub-jobs, before any substep:

1. **Origin check.** The shift itself was applied at the sync point that played it ("Origin shift"), so normally `Zenith.WorldOrigin::m_uRebaseCount` equals `m_uOriginRebaseCount` and this step does nothing. When it differs (a defect: the notification of a shift did not reach the listener), apply "Origin shift" steps 1–4 now with `S = (O_resource − m_adAppliedOrigin)` per axis in binary64 (exact: both are sums of integer shifts), log once at error severity `physics: world <tag>: origin shift applied late at ingest (count <a> → <b>)` and count `physics.origin.late_shift_delta`; the conformance suite asserts the counter stays 0. A component of that S above 2^24 m in magnitude is applied by the rounding rule of "Origin shift" step 1 and also counts `physics.origin.invalid_shift_delta`; step 4 of "Origin shift" stores the count and the origin, so the late path runs once per missed shift and never repeats.
2. **Scan.** `ParallelFor` over the chunks of the query `WITH Zenith.Physics.Collider OR Zenith.Physics.RigidBody` (two queries, merged by simulation ID): each leaf writes, into a per-leaf slot of the step arena, the rows whose structural epoch, Collider or RigidBody change version, or parent world transform changed since the previous tick (the change filter of `Zenith2/Docs/Design/ECS/Queries.md` → "Change-filtered queries", using `GetPreviousReadVersion`; a version of 0 means every row). The slots are concatenated in leaf order and sorted by simulation ID (`Zenith_Sort` with the simulation ID as the whole key), so the change list is independent of chunk layout.
3. **Removals.** For every body record whose entity no longer qualifies (component removed, `pending destroy`, or a child collider whose body entity changed), in ascending simulation ID: destroy its constraints, its body, release its shape reference, erase its `m_xBuoyancySamples` entry, and insert its simulation ID into the ended list `m_aulEndedPairIDs`, so its pairs end at this tick's emission ("Event emission" step 2).
4. **Creations and rebuilds.** For every qualifying changed entity in ascending simulation ID: compose the shape ("Body composition"); create it (`CreateShape` for primitives and compounds, the asset's shape for a single cooked collider); create the body (`CreateBody`, user data = simulation ID; position and rotation `W_parent · L`, with `W_parent` the parent's `Zenith.WorldTransform` of the previous tick (identity for a root) and `L` the entity's own `Zenith.LocalTransform` 1–2 after sync point 4; initial velocities from properties 11–12 on creation; then the record's written local values are set to `L`'s exact bits and `m_ulParentVersion` to the parent row's change version) or replace its shape (`SetBodyShape` with mass update) when it exists. For a body entity with `Zenith.Physics.Buoyancy` whose shape is not a single cooked collider, the same step (re)computes its `m_xBuoyancySamples` entry from the composed shape by `Zenith_PhysicsPrimitiveBuoyancy` (the lattice of PHYS/Formats.md "Buoyancy samples", a pure function of the shape kinds, dimensions and child poses), so buoyancy never reads the process's primitive cache ("Buoyancy algorithm"). More than 64 creations in one tick go through `CreateBodies` in one batch, in the same order. The world assigns each new body the lowest free body index and that slot's next generation ("Slot allocation"), so the handle a creation receives depends only on the set of live slots and the per-slot generations, never on the order of earlier destructions. A creation refused for capacity (`PHYSICS_ERROR_BODY_CAPACITY`) leaves the entity bodiless, counted `physics.bodies.refused`, retried on its next change; the refusal set is deterministic because creations run in ascending simulation ID.
5. **Constraints.** For every joint entity whose joint changed, or whose bodies were created or removed in steps 3–4, in ascending simulation ID: destroy and re-create its constraint (`m_uPriority` = a per-world counter incremented per creation, so the solver's constraint order is creation order; the constraint receives the lowest free constraint index, "Slot allocation"). With BodyB null, frame B is `W_joint · T(FrameBPosition, FrameBRotation)` with `W_joint` the joint entity's `Zenith.WorldTransform` of the previous tick, so a world-anchored joint moves with origin shifts and streaming root translations like any other entity (PHYS/ComponentsAPI.md "Joint properties"). Every creation or destruction here of a constraint with CollideConnected false sets `m_bExcludedPairsDirty`; the array is rebuilt once, by step 12 ("Excluded pairs maintenance").
6. **Controllers, ragdolls, vehicles.** Create or remove controller capsule bodies (kinematic, `PHYSICS_BODY_FLAG_QUERY_ONLY` off, layer = the controller's) and vehicle records for changed rows, ascending. A controller row on an entity that carries `Zenith.Physics.RigidBody` or its own `Zenith.Physics.Collider` is refused ("Body composition" rule 10): no capsule is created, and an existing capsule is removed when either component is added. A removed capsule inserts its character's simulation ID into `m_aulEndedPairIDs` unless the character's State is not NONE (its capsule is then query-only and already left the simulation at the enter playback).
7. **External writes.** "External writes".
8. **Pending operations.** "Pending operations", in `m_xPendingOps` order, then clear it.
9. **Kinematic targets.** For every kinematic body in ascending simulation ID, compute this tick's target pose ("Kinematic bodies and moving platforms"); for every controller, the target is CharacterController 35–36 written in phase 4.
10. **Region freeze.** "Region freeze".
11. **Ragdoll drives.** For every ragdoll in ACTIVE, ENTERING or RECOVERING that is not frozen ("Region freeze") with DriveStrength > 0, set the motor targets from `Zenith_RagdollPoseSource::GetAnimatedModelPose` (PHYS/CharacterRagdollVehicle.md "Powered ragdolls").
12. **Excluded pairs.** When `m_bExcludedPairsDirty` is set: "Excluded pairs maintenance". It is the last ingest stage, so it runs after every creation and destruction of the ingest and before the first `StepOnce` of the tick.

Complexity: O(c log c) for c changed rows plus O(b) for b kinematic bodies, plus O(k log k) for the k CollideConnected-false constraints in a tick whose step 12 rebuilds; an unchanged world costs the chunk scan alone. On Jolt, re-creating F bodies at indices freed earlier costs O(F · L) inside `CreateBodyWithID`, with L the length of Jolt's own body free list (PHYS/Backends.md "Backend behaviour differences"), which `Zenith_Bench_BudgetsPHYS`'s cell-churn case measures under Z2-B-PHYS-002.

## Excluded pairs maintenance

The Jolt group filter reads `m_aulExcludedPairs` during `PhysicsSystem::Update` alone (PHYS/Backends.md "Jolt layer mapping"), so the array must describe the live constraints at every `StepOnce`, whatever path created or destroyed one. Lowest-free-index allocation ("Slot allocation") hands a freed body index to the very next creation, so a stale entry would make two unrelated new bodies pass through each other on Jolt alone.

- **Who sets the dirty flag.** Every path that creates or destroys a constraint with CollideConnected false sets `m_bExcludedPairsDirty` in the same call: ingest steps 3 and 5 (joint entities), the `Physics.EnterRagdoll` playback (each part's constraint, PHYS/CharacterRagdollVehicle.md "Ragdoll enter procedure" step 5), the `Physics.RemoveRagdollBodies` playback (sync point 6 of the tick whose phase-6 run reached w = 0), the `LIFECYCLE_EVENT_DESTROYED` listener at the end-of-tick playback (the entity's constraints and ragdoll parts), region freeze and thaw at ingest step 10 (a frozen body's constraints become UNRESOLVED, "Constraint state machine"; a frozen ragdoll keeps its part constraints, both of whose bodies are frozen, so it changes no excluded pair), and the restore of "Save and restore algorithm".
- **Who rebuilds.** Ingest step 12 alone, when the flag is set: one entry `(low body index << 32) | high body index` per live constraint with two bodies and CollideConnected false, taken from `m_xConstraints` (in `m_aulConstraintOrder`) and from every ragdoll record's part constraints (in `m_aulRagdollOrder`, then part order); duplicates removed; `Zenith_Sort` ascending; `SetExcludedPairs(m_aulExcludedPairs)`; flag cleared. `SetExcludedPairs` has no other caller.
- **Why that is enough.** The array is consulted only inside `StepOnce`, `StepOnce` runs only in `Physics.Step` after its ingest, and step 12 is the ingest's last stage, after every creation that could reuse a freed index (steps 4–6) and after every destruction of the tick's ingest (steps 3, 5, 10). Every other path runs at a sync point, at the end-of-tick playback or out of tick, before the next ingest. A stale entry may exist between a sync point and the next ingest, but no step reads it.
- On PhysX the array is accepted and unused (the joint flag excludes the same pairs), so the rule is the same on both backends.

`Test_PhysicsIngest_ExcludedPairsDirtyOnEveryPath` (fake backend), `Test_PhysicsJolt_ExcludedPairsFollowEveryDestructionPath` (integration suite, Jolt) and `Test_PhysicsConformance_ReusedIndicesCollide[<Backend>]` assert it (PHYS/Tests.md "Ingest and sync tests", "Ragdoll tests", "General conformance cases").

## External writes

A value of a PHYS-owned runtime property or of an entity's transform that differs from what PHYS last wrote was written by someone else (a command, a teleport, another arbitration source), and ingest adopts it. Every comparison is made **in the space PHYS wrote**: the observed `Zenith.LocalTransform` 1–2 bits against the exact local bits PHYS last wrote (or adopted) for that entity, never a world pose recomposed from a parent's transform. A recomposition `W_parent · inverse(W_parent) · W_body` is not bit-equal to `W_body` in binary32 (and never is under a non-uniform parent scale), so comparing world poses would make every parented body an external write in every tick.

| Observed | Comparison (bit-exact, per component) | Action |
|---|---|---|
| RigidBody 20 or 21 ≠ `m_xWrittenLinearVelocity` / `m_xWrittenAngularVelocity` | the property bits against the written bits | `SetBodyVelocity` with the observed values; counter `physics.external.velocity_delta` |
| `Zenith.LocalTransform` 1–2 ≠ `m_xWrittenLocalPosition` / `m_xWrittenLocalRotation` for a dynamic body | the local bits against the written local bits | `SetBodyTransform` to `W_parent · L_observed` (`W_parent` the parent's previous-tick `Zenith.WorldTransform`, identity for a root; velocities kept, `PHYSICS_ACTIVATION_ACTIVATE`); the written local values become the observed bits; counter `physics.external.pose_delta` |
| the same for a static body | as above | `SetBodyTransform`; counter `physics.static_moved_delta` and one development-build warning per body |
| a kinematic body | none: its target is its entity's composed pose every tick ("Kinematic bodies and moving platforms"), so any writer moves it by design | — |
| a controller's `Zenith.LocalTransform` 1–2 ≠ the local bits `Physics.SyncTransforms` last wrote for it | the local bits against the controller record's written local bits | handled in phase 4 by the controller (PHYS/CharacterRagdollVehicle.md "Character move algorithm" step 1) |

Teleports never reach this table: the lifecycle listener applies them at their sync point (PHYS.md "Lifecycle listener") and sets the written local values to the teleported local bits. An origin shift never reaches it either: the `LIFECYCLE_EVENT_ORIGIN_SHIFTED` listener applies SCENE's own formula `(float)((double)p − S)` to the written local position of every root body and controller, so the comparison stays bit-equal ("Origin shift").

**A dynamic body under a moving parent.** A dynamic body's entity is simulated in world space: the solver alone moves it, and its parent's motion is not applied to it (a body is not dragged by the hierarchy; a kinematic body is the way to make a body follow its parent). `Physics.SyncTransforms` re-expresses the simulated pose in the parent's current frame ("Physics-to-transform sync"), so the entity's world transform equals the body pose again from the next propagation. The scene validation of PHYS.md "Spawn clearance rule" warns about every dynamic body whose ancestor carries a dynamic or kinematic body, an animation component or a cinematic binding (`PHYSICS_COOK_DIAG_DYNAMIC_UNDER_MOVING_PARENT`), because such authoring usually expects the hierarchy to carry the body.

## Pending operations

Command playback functions append a `Zenith_PhysicsPendingOp { uint8 m_eKind; uint64 m_ulSimulationID; Zenith_PhysicsBodyCommandPayload m_xPayload; }` to `m_xPendingOps` (capacity `physics.maxPendingOpsPerTick`, design default 16,384, `QUEUE_FULL_POLICY_REJECT`: a full list drops the command with `physics.command.dropped_delta`, deterministic because playback is in merge order). Ingest step 8 applies them in list order:

| Kind | Applied as |
|---|---|
| ADD_IMPULSE, ADD_ANGULAR_IMPULSE | `AddImpulse` / `AddAngularImpulse` before the first substep |
| ADD_FORCE, ADD_TORQUE | accumulated per body (sum in list order) and applied before every substep of this tick |
| SET_VELOCITY | `SetBodyVelocity` (only the present parts) |
| SET_SLEEPING | `SetSleeping` |
| SET_GRAVITY | `SetGravity` (the world's; saved with the module state) |

An operation on an entity without a body is dropped (`physics.command.no_body_delta`). Ragdoll commands are not pending operations: their playback acts at the sync point (PHYS/CharacterRagdollVehicle.md "Ragdoll state machine").

## Substep loop

```
n = physics.substepsPerTick                       (1 … uPHYSICS_MAX_SUBSTEPS_PER_TICK = 8, validated by the schema)
h = tickDurationUs / 1e6 / n                      (float; ≤ fPHYSICS_MAX_STEP_SECONDS, else PHYSICS_ERROR_INVALID_DESC at attach)
for i in 0 … n−1:
	pre-substep (single-threaded, ascending simulation ID):
		kinematic bodies: MoveKinematic(target interpolated at (i+1)/n, h)       — position lerp, rotation nlerp from the tick-start pose
		accumulated forces and torques: AddForce / AddTorque
		vehicles: PHYS/CharacterRagdollVehicle.md "Vehicle algorithm" for substep i
		buoyancy: "Buoyancy algorithm"
	backend: StepOnce(h, jobs, contacts)
	post-substep: joint breakage checks ("Joint breakage")
```

- The loop runs exactly n times per tick; no other path calls `StepOnce`. The number of ticks per frame is the simulation clock's, bounded by the catch-up row Z2-B-JOBS-002 (BR-DROP, `sim.dropped_ticks`), so the physics steps per frame never exceed that row × 8. A hitch therefore drops simulated time instead of running an unbounded burst of steps (the old engine's lesson: an unbounded loop sank a resting capsule 0.61 m through terrain during a 0.49 s load hitch; PHYS/Tests.md "Substep and hitch tests").
- With the PhysX backend the loop is a chain of node continuations: stage k runs substep k's pre-substep work and `StepOnce`, which schedules one drain wave and returns pending; while a step result is pending the stage registers a resume continuation with `Zenith_JobContext::Continue` that calls `ResumeStep` (another wave, or completion); when substep k completes, its post-substep work runs and stage k+1 is registered the same way; the last stage registers the write-back stage. The wave count of a substep is reported in `physics.step.waves` (last tick). The Jolt backend runs every substep on the node's thread in one stage (PHYS/Backends.md "Jolt step and the job system").
- A `PHYSICS_ERROR_STEP_FAILED` from the backend is logged with its code, counted `physics.step.failed_delta`, and the remaining substeps of the tick still run; it is a defect report, never a simulation branch, and the conformance suite asserts it never occurs.

## Write-back

The last stage of `Physics.Step`: `ParallelFor` over the chunks of `WITH Zenith.Physics.RigidBody`; for each row whose body is dynamic or kinematic and was active this tick (or changed sleep state), read `GetBodyState` and write RigidBody 20–22 through the chunk view, then store the velocities' exact bits into the body record's written velocity values; RigidBody 23 Frozen is written for every row whose frozen state changed at ingest step 10 ("Region freeze"). Vehicles write 30–32; joints write 21 when broken. Poses are not written here: `Zenith.LocalTransform` belongs to phase 6.

## Physics-to-transform sync

`Physics.SyncTransforms` (phase 6, `WRITER_SOURCE_PHYSICS`) writes `Zenith.LocalTransform` 1–2 into the Physics staging column for:

- every body entity whose body is dynamic and was active this tick;
- every dynamic body entity whose parent's `Zenith.WorldTransform` change version differs from the record's `m_ulParentVersion` (the parent moved, even if the body sleeps), counted `physics.sync.parent_moved_delta`;
- every controller.

The value: for a root entity the body pose (controllers: TargetPosition and TargetRotation); for an entity with a parent, `Local = inverse(W_parent) · W_body` with `W_parent` the parent's `Zenith.WorldTransform` of the previous tick (the newest the hierarchy has propagated, phase 8). After the write the record's `m_xWrittenLocalPosition` / `m_xWrittenLocalRotation` hold the exact bits written and `m_ulParentVersion` the parent's version that was read, which is what "External writes" compares against. Scale (property 3) is never written by PHYS. Sleeping bodies with an unmoved parent are not written, so the arbitration keeps their value ([Z2-N-ARCH-495]).

A parent moved during tick t (in phases 1 to 7) is propagated in phase 8 of tick t, after this node read its tick t − 1 transform, so for that one tick the child's world transform is the old local under the new parent; the next tick's write sees the new version and restores `W_child = W_body` within one recomposition rounding. That one-tick lag is the defined behaviour for dynamic bodies under moving parents ("External writes"); kinematic bodies and controllers do not have it, because they follow their entity's composed pose. The rotation written is the body's full orientation for every shape kind: an oriented box keeps its rotation, and no shape is ever reduced to an axis-aligned box whose rotation would be forced to identity and reach the saved scene (the old engine's lesson; `Test_PhysicsSync_RotatedBoxKeepsRotation[<Backend>]`).

## Event emission

After the last substep, `Physics.Step` turns the sorted contact records into events:

1. `m_xContacts.SortAndGet()` returns records sorted by (user data A, part code A, user data B, part code B, sub-shape A, sub-shape B, substep, kind) — the **record key**, with A the body of the lower (user data, part code) pair ("Ragdoll part identity"). Two records with equal keys would be the same report of one step, so the key is a total order. Every record names a body that was live when the backend appended it: a backend drops each `PHYSICS_CONTACT_REMOVED` report of a body that is no longer its slot's live occupant before it reaches the sink, counted `physics.contacts.stale_removed_delta` (PHYS/Backends.md "Body identity on both backends").
2. **Removal endings, before any record.** When `m_aulEndedPairIDs` is not empty, one pass over `m_xPairs` and one over `m_xTriggerPairs` find every pair with a member in the list (binary search of each member in the sorted list, O(P log E)); each such pair gets `m_uSubShapeContacts = 0`, `m_uSuspendedContacts = 0` and bit 3 "ended by removal". The list is then cleared.
3. **Walk** the records in key order. A self-contact (user data A = user data B: two parts of one ragdoll) is skipped and counted `physics.contacts.self_delta`. Every other record updates the pair (user data A, user data B), whatever its part codes, in `m_xPairs` (or `m_xTriggerPairs` for trigger pairs, record flag bit 0):
   - `PHYSICS_CONTACT_ADDED`: when the pair has suspended contacts, one is taken back (`m_uSuspendedContacts −= 1`, the count unchanged: a sleeping body's contact re-confirmed at its wake, "Sleeping"); otherwise the count increments. When the count was 0 (or the pair absent, which inserts it), the pair takes this record's point and normal, the first ADDED record of the pair in key order, and sets bit 0 "began" unless bit 3 is set: a pair ended by removal that a record of the same emission begins again is a **handover** (another body of the same simulation ID continues it: the capsule after a recover, the parts after an enter), emits neither `ContactEnded` nor `ContactBegan`, and counts `physics.contacts.handover_delta` once.
   - `PHYSICS_CONTACT_REMOVED` with record flag bit 1 "sleep": moves one contact from live to suspended (`m_uSuspendedContacts += 1` while it is below the count; the count unchanged).
   - `PHYSICS_CONTACT_REMOVED` without bit 1: decrements the count (and lowers `m_uSuspendedContacts` to the count when it would exceed it); reaching 0 sets bit 1 "ended".
   - Any `PHYSICS_CONTACT_REMOVED` on an absent pair or one whose count is 0 changes nothing: it is the late report of a body that left the simulation through the ended list (a frozen body's, or the capsule's at a ragdoll enter, whose part code 0 sorts it before the parts' records of the same pair), or a record whose ADDED the sink dropped at capacity.
   - `PHYSICS_CONTACT_PERSISTED` updates the pair's tangential speed only.
   A pair can begin and end in one tick.
4. **Wake resolution.** For every pair and trigger pair with `m_uSuspendedContacts > 0` whose two simulation IDs are both awake after this tick's write-back ("Sleeping"): the count loses the suspended contacts (`m_uSubShapeContacts −= m_uSuspendedContacts`, then 0 suspended); reaching 0 sets bit 1 "ended". Those are the contacts no ADDED re-confirmed when their body woke.
5. **Removal endings resolved.** A pair with bit 3 whose count is still 0 sets bit 1 "ended", so it emits `ContactEnded` or `TriggerExited` in group (b) of step 6; one whose count is above 0 is a handover and emits nothing.
6. Emit, walking the **event key IDs** in ascending order — the low simulation ID of every pair with an event and the simulation ID of every broken joint, merged into one ascending sequence — and, for each key ID K, in this fixed group order: (a) for each pair (K, high) with bit 0 in ascending high, `ContactBegan` (non-trigger pairs, when either body reports contacts) or `TriggerEntered`; (b) for each pair (K, high) with bit 1 in ascending high, `ContactEnded` or `TriggerExited`; (c) the interaction events of pairs (K, high) in ascending high ("Interaction detection"); (d) `JointBroken` when K is a joint that broke this tick. Every event of key ID K is emitted through `Zenith_SystemContext::Events(K)`, so its merge key is (system order, K, ordinal, sequence), where all of them share `Physics.Step`'s one invocation ordinal and the sequence is this emission order ([Z2-N-ARCH-478]).
7. Clear bits 0, 1 and 3; remove pairs whose count is 0.

**Delivery order.** Events are delivered in phase 9 in merge-key order: ascending key ID first (so began, ended, interaction and joint events of different IDs interleave by ID), then, within one key ID, in the group order (a) → (d) of step 6 and ascending high simulation ID inside each group. A trigger pair's key ID is its low simulation ID whichever body is the trigger. This is the order Z2-R-PHYS-066 and `Test_PhysicsEvents_SortedByKey` assert.

The event key is therefore a function of simulation IDs, part codes and backend sub-shape keys alone, never of arrival order, worker count or chunk layout. The capacity of each world's event queue is Z2-B-GAME-003, checked first-fit at the merge in merge order (`Zenith2/Docs/Design/ECS/Scheduling.md` → "Capacity acceptance"): an event the queue rejects is a deterministic merge rejection of the emitting node's invocation for that simulation ID, counted, never a crash.

**Which bodies' contacts are recorded.** A backend appends `PHYSICS_CONTACT_ADDED`, `PERSISTED` and `REMOVED` records for every pair in which either body has `PHYSICS_BODY_FLAG_REPORT_CONTACTS` or `PHYSICS_BODY_FLAG_REPORT_INTERACTIONS`, or either is a trigger (PHYS/Backends.md "Jolt contact listener", "PhysX filter shader"); `ContactBegan` and `ContactEnded` are then emitted only for pairs where either body has REPORT_CONTACTS, and interaction events only for pairs where either has REPORT_INTERACTIONS ("Interaction detection").

## Trigger semantics

The rules are the intersection both backends produce with the mappings of PHYS/Backends.md "Jolt layer mapping" and "PhysX filter shader", and the conformance case `Test_PhysicsConformance_TriggerReachMatchesRule[<Backend>]` asserts every row on both:

| Other body | Overlap reported |
|---|---|
| dynamic, awake or sleeping, whose layer the trigger's layer collides with | yes |
| kinematic and not a trigger (moving platforms, controller capsules), whose layer the trigger's layer collides with | yes |
| static (implicit statics, terrain, cell colliders) | never |
| another trigger | never |
| a query-only body (a controller capsule while its ragdoll is not NONE, a frozen body) | never |
| a body whose collision group the trigger shares with IGNORE_SAME_GROUP on either, or a pair in `m_aulExcludedPairs` | never |

- Every trigger body is kinematic and never sleeps ("Body composition" rule 9), so the rules do not depend on the trigger's authored motion type.
- `TriggerEntered { Trigger, Other }`: Trigger is the trigger body's entity (for a split trigger child, the child collider's entity), Other the other body's entity. Two triggers never pair, so exactly one event is emitted per overlap.
- A game that needs a volume test against static geometry or against another trigger uses a query (`OverlapSphere`, `OverlapBox` with `bIncludeTriggers`), not a trigger.
- Triggers are invisible to queries unless `m_bIncludeTriggers` (the old engine's rule that raycasts ignore sensors, kept).
- A trigger that is destroyed, frozen or loses its collider emits `TriggerExited` for every overlap in the step that removes it, with handles that are still readable when removal happens in the same tick and stale otherwise.
- OpenWorldSlice's interaction sample subscribes to `TriggerEntered` and `TriggerExited` (row GA3 (c)); delivery is phase 9 of the same tick.

## Interaction detection

For each touching pair where either body has `PHYSICS_BODY_FLAG_REPORT_INTERACTIONS`, using the materials of the touching sub-shapes (`Zenith_FindPhysicsInteraction`):

- **Impact:** the pair began this tick and its approach speed ≥ the rule's impact speed and its cooldown is 0 → `Interaction { kind IMPACT, speed = approach speed }`; the cooldown is set to the rule's ticks and decremented every tick.
- **Slide start / stop:** for a persisting pair the backend's persisted records carry the tangential speed; when it crosses the rule's slide speed upward (not sliding) → `SLIDE_START`; when it falls below half of it (sliding) → `SLIDE_STOP`; ending the pair while sliding emits `SLIDE_STOP`.

## Constraint kinds

Limits are interpreted per kind; a value outside the range in the table is `PHYSICS_ERROR_INVALID_DESC` at creation.

| Kind | Linear limits | Angular limits | Motor axes |
|---|---|---|---|
| FIXED | — | — | — |
| POINT | — | — | — |
| HINGE | — | x: min/max angle about frame X, [−π, π]; min = max = 0 means free | rotation about X (velocity or position) |
| SLIDER | x: min/max distance along frame X, metres | — | translation along X |
| DISTANCE | x: min/max distance, 0 ≤ min ≤ max | — | — |
| CONE | — | x: half angle of the cone about frame X, [0, π] | — |
| SWING_TWIST | — | x: twist min/max [−π, π]; y: normal half-cone [0, π]; z: plane half-cone [0, π] (stored in max.y, max.z) | orientation (position motor through `m_xMotorTargetRotation`) |
| SIX_DOF | min/max per axis; min = max = 0 locked, min > max free | min/max per axis, [−π, π] | every axis |

## Joint breakage

After each substep, for every constraint with a non-zero BreakForce or BreakTorque, in ascending joint simulation ID: read `GetConstraintImpulse`; the constraint breaks when `linear / h > BreakForce` or `angular / h > BreakTorque` (h the substep duration). Breaking disables the constraint (`SetConstraintEnabled(false)`), sets Joint 21 Broken at write-back and emits `JointBroken`. A broken joint stays broken until a `SetProperty` of Enabled re-creates it. Both backends break through this one rule, so a joint breaks at the same threshold on each.

## Continuous collision detection

- `PHYSICS_MOTION_QUALITY_CONTINUOUS` bodies are swept along their motion each substep (Jolt's linear cast, PhysX's sweep-based CCD; PHYS/Backends.md "Motion quality").
- Every body also gets speculative contacts within `physics.speculativeContactDistance`.
- Character controllers never tunnel because their motion is a shape cast (PHYS/CharacterRagdollVehicle.md).
- The guaranteed envelope is exactly the set of cases the conformance tests of PHYS/Tests.md "Tunnelling tests" run, at `physics.substepsPerTick` 1 on a 60 Hz tick: a CONTINUOUS sphere of radius 0.05 m at 200 m/s never passes a 0.1 m static box wall (`Test_PhysicsTunnel_ContinuousEnvelopeHolds`); a CONTINUOUS sphere of radius 0.02 m at 400 m/s never passes a 0.01 m static box or a heightfield (`Test_PhysicsTunnel_ContinuousBulletStops`); a DISCRETE 0.5 m box at 40 m/s never passes a 0.3 m wall (`Test_PhysicsTunnel_DiscreteThickWallHolds`). Nothing beyond those cases is promised.

## Sleeping

A body with ALLOW_SLEEPING whose linear and every point velocity stay below `physics.sleepVelocity` for `physics.sleepSeconds` goes to sleep (backend timers, saved in the EXACT state); waking happens on contact with an awake body, on any external write, pending operation or `SetSleeping(false)`. Sleeping bodies cost no solver time and are not written back or synchronised; write-back records the sleep state it reads (RigidBody 22) in the body record's flag bit 6.

**A pair persists while a body of it sleeps.** This is the neutral rule both backends' event streams follow, so a settled stack, a crate resting on a pressure plate and a body asleep inside a trigger keep their pairs, and no `ContactEnded`/`ContactBegan` (or `TriggerExited`/`TriggerEntered`) is emitted when a body falls asleep or wakes.

- **Backend side.** Jolt removes every contact of a body that goes to sleep ("as soon as a body goes to sleep the contacts between that body and all other bodies will receive an OnContactRemoved callback", `ContactListener.h` of the pinned release) and reports the touching sub-shape pairs again as added when the body wakes. After each `PhysicsSystem::Update`, the Jolt backend therefore sets record flag bit 1 "sleep" on each `PHYSICS_CONTACT_REMOVED` record of two live bodies at least one of which is non-static and inactive (`BodyInterfaceNoLock::IsActive` false; a static body is never active and never counts), and on no other record (PHYS/Backends.md "Jolt contact listener"). The PhysX backend never sets bit 1: a sleeping PhysX island keeps its touching pairs and reports no lost touch, which the pinning ticket's audit confirms against the pinned release (PHYS/Backends.md "PhysX step and the dispatcher"); if the audit finds otherwise, the PhysX backend marks those records the same way after `fetchResults`, through `PxRigidDynamic::isSleeping`.
- **Module side.** A sleep-flagged REMOVED moves one contact of its pair from live to suspended without changing the count; an ADDED on a pair with suspended contacts takes one back; when both simulation IDs of a pair are awake again, the suspended contacts that no ADDED re-confirmed are subtracted and may end the pair, in that tick ("Event emission" steps 3 and 4). A body that leaves the simulation (destroyed, frozen, or a capsule made query-only) is never suspended: its pairs end through the ended list ("Event emission" step 2), and its late REMOVED records, sleep-flagged or not, change nothing.
- **Awake, for wake resolution.** A simulation ID is awake when its body record's flag bit 6 is clear, when its body is static, a trigger or a controller capsule (none of which sleeps), or, for a ragdoll character, when `GetBodyState` of its root part reports it awake (a ragdoll's parts share one island, so they sleep and wake together).

`Test_PhysicsEvents_PairSurvivesSleepAndWake[<Backend>]` (Integration), `Test_PhysicsEvents_SuspendedContactsResolveAtWake` (Unit) and the per-backend record assertions of `Test_PhysicsConformance_ContactsReported[<Backend>]` prove it (PHYS/Tests.md "Event and trigger tests", "General conformance cases").

## Kinematic bodies and moving platforms

- A kinematic body's target for a tick is its entity's world pose composed at ingest (step 9); the substep loop moves it with `MoveKinematic` toward the target interpolated at (i+1)/n, so the backend gives it the velocity that reaches the target, and dynamic bodies resting on it are carried by friction.
- A moving platform is a kinematic body moved by gameplay (a behaviour recording `SetProperty` commands on its `Zenith.LocalTransform`, an animation, a cinematic); PHYS needs no platform component.
- Character controllers standing on a body read its point velocity at the ground contact (`GetPointVelocity`) in phase 4 and add it to their move, including the yaw change of a rotating platform (PHYS/CharacterRagdollVehicle.md step 4).

## Region freeze

STREAM writes `Zenith.Streaming.RegionBound` 2 InActiveRegion in phase 1 (cross-area row ST8). At ingest step 10, for every dynamic body whose entity carries RegionBound, in ascending simulation ID:

| InActiveRegion | Record frozen | Action |
|---|---|---|
| false | no | save its linear and angular velocity into the record, `SetMotionType(KINEMATIC)`, `SetSleeping(true)`, `SetBodyFiltering(layer, group, flags | PHYSICS_BODY_FLAG_QUERY_ONLY)`, set Frozen (RigidBody 23) at write-back, insert its simulation ID into `m_aulEndedPairIDs` (its pairs and trigger overlaps end at this tick's emission, "Event emission" step 2); counter `physics.frozen_bodies` (gauge) |
| true | yes | `SetBodyFiltering(layer, group, flags)` with the body's own flags (QUERY_ONLY cleared unless its own flags carry it), `SetMotionType(DYNAMIC)`, restore the saved velocities, wake it, clear Frozen |
| as recorded | — | nothing |

A frozen body neither integrates nor collides: QUERY_ONLY takes it out of every simulation pair on both backends (a kinematic sleeping body would otherwise still be an infinite-mass wall to awake dynamic bodies at the region's edge), it stays visible to queries that include query-only bodies, and it overlaps no trigger, so a body never falls through the collision of an unloaded cell and nothing strikes a frozen body. `Test_PhysicsFreeze_FrozenBodyCollidesWithNothing[<Backend>]` drops an awake box onto a frozen one and asserts that it passes through, with no contact and no trigger event, and that a thawed body collides again. **Ragdolls freeze with their character.** A ragdoll part has no entity, so step 10 also visits, in ascending simulation ID, every ragdoll whose State is not NONE and whose character entity carries RegionBound, by the same table applied to every part in definition order: on freezing, each part's motion type and linear and angular velocity are saved in the ragdoll record's part, and the part is made kinematic, put to sleep and given QUERY_ONLY filtering; the record's `m_bFrozen` is set and the character's simulation ID enters `m_aulEndedPairIDs`. On thawing, each part gets its own filtering back (layer Ragdoll 5, collision group 0, the flags of PHYS/CharacterRagdollVehicle.md "Ragdoll definition"), its saved motion type and velocities, and is woken; `m_bFrozen` is cleared. The part constraints are kept throughout (both of their bodies are frozen, so they act on nothing and the excluded pairs do not change). While frozen the ragdoll keeps its State, w and k: `Physics.RagdollBlend` does not advance it (no k increment, no state change) and keeps writing the root from the motionless parts, ingest step 11 sets no drive targets, and a `Physics.EnterRagdoll` or `Physics.RecoverRagdoll` playback on a frozen ragdoll is refused (`physics.ragdoll.enter_refused` with cause "frozen", or `physics.ragdoll.recover_ignored_delta`). `Test_PhysicsFreeze_RegionBoundFreezesAndRestores` asserts the ragdoll case. A ragdolling character whose cell leaves the active region therefore stops with its cell instead of falling through the unloaded collision.

Static colliders, terrain colliders and triggers of a cell are components of the COLLISION-grid cell's entities and enter and leave with the cell's sync points through ordinary body composition ("Cell bodies").

## Cell bodies

When a streamed cell activates, its entities appear at a sync point and the next ingest creates their bodies in one `CreateBodies` batch in ascending simulation ID; their cooked shapes are hard dependencies of the cell scene, so they are resident before activation. A cell's unload destroys its entities, and ingest removes their bodies. No cell code calls PHYS; STREAM's ordering (dependent cells activate after the collision cells beneath them, `Zenith2/Docs/Design/STREAM/Runtime.md` → "Physics coordination") keeps gameplay bodies from activating above absent collision.

## Origin shift

STREAM decides a rebase in phase 1 and records it; SCENE plays it. STREAM's own text: "SCENE plays the shift last in step 7 of sync point 1 (row ST3), after the sync point's spawns (step 6), after `Streaming.ApplyCellState` and `Streaming.BindReferences` (step 7) and after SCENE's own activation records, so a cell admitted this tick with a root translation computed from the pre-shift O is shifted with every other position" (`Zenith2/Docs/Design/STREAM/Runtime.md` → "Rebase rule"), and its consumer table says "PHYS shifts its broadphase" (`Zenith2/Docs/Design/STREAM/Runtime.md` → "Who reacts to a rebase"). The numbers are STREAM's keys, never PHYS constants: the threshold `stream.rebase.thresholdMetres`, the quantum `stream.rebase.quantumMetres` and the minimum interval `stream.rebase.minIntervalTicks` (2,048 m, 2,048 m and 30 ticks at STREAM's defaults). A shift S is, per axis, an integer number of metres: a multiple of `stream.rebase.quantumMetres` when STREAM records it, and any integer when another recorder does (SCENE's `Zenith_WorldOriginShift` admits any integer metres from any recorder, `Zenith2/Docs/Design/STREAM/PublicAPI.md` → "SCENE world origin"), so it is exact in binary64.

**When PHYS shifts.** The physics world shifts at the same sync point as the scene, so phases 2 to 4 of the rebase tick (script queries in phase 2, ANIM's foot-placement ray casts in phase 3, `Physics.CharacterMove` and `Zenith_CollisionQuery` readers after it) see shifted transforms and a shifted physics world together:

SCENE specifies the playback and the dispatch (`Zenith2/Docs/Design/SCENE/ModuleSeams.md` → "Origin shift playback", steps 5–7, and → "Origin shifted lifecycle event"): after every transform and flagged property is shifted, still inside step 7 of the sync point, the `Scene.ShiftWorldOrigin` playback dispatches `LIFECYCLE_EVENT_ORIGIN_SHIFTED` through the world's lifecycle dispatcher with a null entity and `m_pPayload` pointing at the played `Zenith_WorldOriginShift`; a record SCENE ignores (a second shift of the sync point, or a component that is not finite, not integral or above 2^53 m) dispatches nothing. The enumerator itself is Architecture's and joins `Zenith_LifecycleEventKind` through core-doc item X264 (j), the one core dependency left (row PH20, applied by SCENE).

PHYS's lifecycle listener (interest bit `LIFECYCLE_EVENT_ORIGIN_SHIFTED`) then, on the sync-point node, for the world being played back:

1. Converts the payload to `xShift = (float)S` per axis. SCENE has validated that every component is a finite integer number of metres of magnitude at most 2^53 m (a non-finite or non-integral component never reaches the listener, which `ZENITH_ASSERT`s it). A component of magnitude at most 2^24 m is exact in binary32, so the conversion is exact, the case STREAM's rule always produces. A component above 2^24 m is a defect of the recorder: it is converted with one rounding to nearest (binary32 values above 2^24 are integers, so `xShift` is still an integer number of metres), the shift is applied by steps 2–4 with that `xShift` (the backend's origins then differ from SCENE's `(float)((double)p − S)` by at most one rounding, while step 3 still applies SCENE's own formula to the module's state), it is logged once at error severity `physics: world <tag>: origin shift component above 2^24 m rounded`, and `physics.origin.invalid_shift_delta` counts it once; step 4 records the count and the origin as for any shift, so neither the listener nor ingest step 1 applies it again.
2. Calls `ShiftOrigin(xShift)` on the backend world: PhysX through its native `PxScene::shiftOrigin`, Jolt body by body (PHYS/Backends.md "Origin shift on Jolt"). The body origin is set to the IEEE binary32 difference `p − xShift`, which equals SCENE's `(float)((double)p − S)` because `xShift` is exact and a binary32 subtraction is correctly rounded; the backend's internal centre-of-mass position may differ from `p − xShift + R·c` by one rounding (Jolt stores the centre of mass and recomputes it, PHYS/Backends.md "Origin shift on Jolt"), so a body origin read back after a shift is within one binary32 rounding of `p − xShift`, not bit-equal. No module comparison depends on backend bits after a shift: "External writes" compares the module's own written local bits, which step 3 shifts by SCENE's formula.
3. Applies SCENE's formula `(float)((double)p − S)` to the module's position-valued state in ascending simulation ID: the written local position of every root body and every root controller, each controller's last target position, each ragdoll's captured recover root, the points of `m_xPairs` and `m_xTriggerPairs`, and the vehicle contact points. Child entities' written local values are unchanged (SCENE does not shift a child's local transform). CharacterController 35 TargetPosition carries `PROPERTY_FLAG_WORLD_POSITION` (PHYS/ComponentsAPI.md "Zenith_CharacterController header"), so SCENE has already shifted the component value by the same formula in the same playback.
4. Stores `m_uOriginRebaseCount = Zenith.WorldOrigin::m_uRebaseCount` and `m_adAppliedOrigin` = the resource's origin, and counts `physics.origin_shifts` (cumulative).

Kinematic targets are recomputed from the shifted entity poses at the next ingest; frozen bodies store velocities only; world-anchored joint frames follow their joint entity (ingest step 5) and, inside the backend, are moved by `ShiftOrigin` itself. `Test_PhysicsLarge_QueriesConsistentInRebaseTick[<Backend>]` asserts the result: phase-2, phase-3 and phase-4 queries issued in the rebase tick hit the same entities as the same queries in the tick before, at positions equal to the previous ones minus S within one binary32 rounding of the shifted value, and a controller standing on a static floor in the rebase tick neither falls, snaps nor depenetrates.

## Buoyancy algorithm

For every dynamic body with `Zenith.Physics.Buoyancy` Enabled and a `Zenith.FluidQuery` resource present, before each substep, in ascending simulation ID:

1. Take the body's buoyancy samples (up to 32 local points with volumes `V / N` each): for a body whose shape is a single cooked collider, the samples cooked with the shape (PHYS/Formats.md "Buoyancy samples section"), held by its `Zenith_CollisionShapeAsset`, which stays ACTIVE while the collider references it; for every other body (primitives and runtime compounds), the body's `m_xBuoyancySamples` entry, which ingest step 4 computed from the composed shape by the lattice of PHYS/Formats.md "Buoyancy samples". Buoyancy never reads the process's primitive cache, so a query of another world evicting a cache entry cannot touch a sample buoyancy is reading.
2. For each sample at world position p: `Sample(p.x, p.z)`; depth `d = surface − p.y`; submerged fraction `f = clamp((d + r_s) / (2 r_s), 0, 1)` with `r_s = (V / N)^(1/3) / 2`, the cube root through CORE's deterministic power function (rule D9).
3. Buoyant force at p: `ρ × V/N × f × (−gravity) × VolumeScale` (ρ = FluidDensity, or the sample's density when 0); drag at p: `−LinearDrag × (v(p) − flow) × f × mass / N`.
4. Angular drag: `−AngularDrag × ω × submerged fraction mean × inertia scale`.
5. `AddForce` at each p and `AddTorque`; the force sum is accumulated in sample order, so the result is a pure function of the body and fluid state.

## Query execution

Every query of `Zenith_PhysicsWorldResource` and `Zenith_PhysicsCollisionQuery`:

1. Validate the arguments (finite, unit directions within 1e-4, non-negative distances and radii); invalid → false or 0, `physics.query.invalid_delta`. This is the C++ contract alone (engine and module callers, AI, GAME's camera): a script never reaches it with invalid arguments, because the `Physics` bindings validate first and raise the catalogued Luau error (PHYS/ComponentsAPI.md "Script bindings", [Z2-N-CONTRACT-473]).
2. Build the filter (`Zenith_PhysicsQueryFilter`, whose class gates are applied identically by both backends: a trigger body is visible exactly when `m_bIncludeTriggers`, a query-only body exactly when `m_bIncludeQueryOnly`, any other body when its motion type's gate (`m_bIncludeStatic`, or `m_bIncludeDynamic` for dynamic and kinematic) is set; then the layer mask, then `m_uIgnoreGroup`, then the ignore list; PHYS/Backends.md "Jolt queries", "PhysX scene query filter"); for an ignore root, collect the root's and its descendants' simulation IDs (depth-first through `Zenith_EntityTable::GetChildren`, at most the hierarchy depth limit and 256 entities; more are ignored by root only, `physics.query.ignore_truncated_delta`) and sort them.
3. For a shape cast, an overlap or a collide query with a primitive shape (sphere, box, capsule, cylinder), acquire a **pinned** shape from the process's primitive cache ("Primitive cache" below).
4. Run the backend query into a stack `Zenith_PhysicsHitCollector` (CLOSEST for single-hit queries, ALL with the caller's span for multi-hit queries).
5. Unpin the shape of step 3 (or release the query's uncached shape).
6. Map each hit's user data to the entity: the body records of `m_xBodies`, then `m_xControllers` (a capsule), then `m_xRagdolls` (a part, "Ragdoll part identity"); a hit whose user data names no live record (a body destroyed this tick) is skipped.
7. Every query counts `physics.queries_delta`; development builds record the query into the debug ring for PHYSICS_DEBUG_VIEW_QUERIES.

Queries run concurrently from many nodes (phases 2, 3, 4 and 6); they mutate nothing but the primitive cache, whose rules below keep every shape a query uses alive until that query has finished.

## Primitive cache

`Zenith_PhysicsPrimitiveCache` (`ZenithPhysics/Internal/Query/Zenith_PhysicsPrimitiveCache.cpp`), one per process, owned by the module instance, holds at most `uPHYSICS_PRIMITIVE_CACHE_ENTRIES` = 4,096 entries. Each entry is `{ key: (shape kind, the dimension floats' bit patterns: radius, half height, half extents), Zenith_PhysicsShapeHandle m_xShape (the cache's one reference), uint32 m_uPins, uint64 m_ulLastUse }`, found through an open-addressing index of 8,192 slots keyed by `Zenith_Hash64` of the key. Its mutex is at level 500 (`uLOCK_LEVEL_PHYSICS_HANDLE_TABLES`, whose row names the primitive cache's entries, pins and index, PHYS.md "Lock-order entries"): it is a different mutex from the service's handle-table mutex of the same level, the two are never held together, and the cache's mutex is never held across a call into `Zenith_Physics` (whose `CreateShape` and `ReleaseShape` take the handle-table mutex), so row 500 needs no same-level-by-address rule.

**Acquire(key)** (query step 3):

1. Lock. Find the key. **Hit:** `m_uPins += 1`, `m_ulLastUse = ++m_ulClock`, unlock, return the entry's shape (pinned).
2. **Miss:** unlock; `CreateShape` from the key (no cache lock held); lock again and find the key once more. If another query inserted it meanwhile, pin that entry as in step 1, unlock, and `ReleaseShape` the shape just created (outside the lock): the duplicate is discarded. A failed `CreateShape` (`PHYSICS_ERROR_SHAPE_CAPACITY`, `PHYSICS_ERROR_OUT_OF_MEMORY`) inserts nothing, and the query returns no hit (false or 0), counted `physics.query.primitive_create_failed_delta`.
3. Otherwise insert the new shape with `m_uPins = 1`. When the cache is full, the victim is the entry with `m_uPins == 0` and the lowest `m_ulLastUse`; it leaves the index under the lock and its shape is released after the unlock (`ReleaseShape`, outside the lock). When every entry is pinned (more than 4,096 queries in flight at once, which no worker count reaches), the new shape is not inserted: the query uses it uncached and releases it itself at step 5 (`physics.query.primitive_uncached_delta`).

**Unpin(entry)** (query step 5): lock, `m_uPins -= 1`, unlock. A pinned entry is never a victim, so its shape stays valid, and its slot generation unchanged, for the whole backend query; a cached shape is released only after its last pin is gone and an insertion chooses it.

- Which entries are resident, and whether a query used a cached or an uncached shape, depends on thread timing; no result does, because equal keys build identical shapes and a query reads only the shape's geometry (D6).
- The cache is emptied at module shutdown (step 6 of PHYS.md "Construction and shutdown"), after every world has detached, so no pin is outstanding.
- `Test_PhysicsQuery_PrimitiveCacheBounded` (single-threaded: at most 4,096 entries, the least recently used unpinned entry is the victim, a pinned entry never is, a full cache of pinned entries serves an uncached shape) and `Test_PhysicsQuery_PrimitiveCacheConcurrentStress` (PHYS/Tests.md "Gate steps for PHYS diffs") assert it.

## Deferred queries

A behaviour's `Physics.RequestQuery(query)` appends a `SIDE_EFFECT_KIND_ASYNC_REQUEST` record whose payload is byte 0 = the value of `ASYNC_SOURCE_PHYSICS_QUERY` (the registry of `Zenith2/Docs/Scripting.md` → "Asynchronous source kind registry" assigns it; PHYS writes the enumerator and never a number, [Z2-N-SCRIPT-925]) followed by the 64-byte `Zenith_PhysicsQueryRequest` (PHYS/Formats.md "Deferred query payloads"). `Physics.RequestQuery` is the binding that implements the registry row today written `ctx:RequestQuery` (core-doc item X264 (h) renames that row and the "Merge-time capacity" table's entry to the binding). At commit SCRIPT assigns its completion ID in merge order ([Z2-N-SCRIPT-334]); the runtime multiplexer calls PHYS's `Zenith_PhysicsDeferredQueryConsumer::Consume(worldTag, kind, completionID, payload)` on the committing merge node, which appends `{ completionID, request }` to that world's `m_xDeferredInbox` (capacity Z2-B-SCRIPT-026's physics-query value; the capacity was accepted at the merge, so Consume never fails).

`Physics.DeferredQueries` (phase 6, after the step of this tick): take up to `physics.deferredQueriesPerTick` inbox entries in completion-ID order, run each query (`ParallelFor`, grain 8, each leaf writing its own result slot), then park each result in completion-ID order through the module publication sink (`Park(completionID, ASYNC_SOURCE_PHYSICS_QUERY, PUBLICATION_KIND_GENERIC_RECOMPUTED, result)`, the single sink of the Q76 (a) ruling), and only then remove the parked entries from `m_xDeferredInbox`. An entry leaves the inbox only when it is parked. The node never checks `Zenith_JobContext::IsCancelled`: a cancelled schedule instance skips only nodes that have not started and still runs every started node and its continuations ([Z2-N-ARCH-412]), so a node that started parks every entry it executed, and a node skipped before it started leaves its entries INBOX for the next tick or for `DetachWorld`. **The remainder waits for the next tick, in completion-ID order**: nothing is dropped or failed for exceeding the per-tick count, because every inbox entry was already accepted against Z2-B-SCRIPT-026 at its merge, so the inbox never holds more than that row's value and the backlog drains in at most `ceil(value / physics.deferredQueriesPerTick)` ticks (`Test_PhysicsDeferred_ExcessWaitsInOrder`). The result becomes visible at the next tick boundary ([Z2-N-ARCH-783]). A request committed in phase 2 of tick t therefore executes in phase 6 of tick t and publishes at the boundary before tick t+1; one committed in phase 10 of tick t executes in phase 6 of tick t+1 and publishes at the boundary before tick t+2. Because both the execution tick and the state it reads are functions of the commit order and the simulation, a replay recomputes the identical result and publishes it at the recorded tick ([Z2-N-SCRIPT-336]).

**Every completion ID is completed exactly once** (the producer obligation of cross-doc items X255 and X265, `Zenith2/Docs/Design/GAME/WorldHost.md` → "Retired world asynchronous results"). PHYS completes each committed query either by parking its result in `Physics.DeferredQueries` or, when the world is detached first (a swap or a shutdown), in `DetachWorld`: before the world state is destroyed it parks every INBOX entry, in completion-ID order, with the cancelled result `{ m_bHit = 0, m_uCount = 0, m_uFlags = PHYSICS_QUERY_RESULT_FLAG_CANCELLED, everything else 0 }` through the same sink, into the world's table, which (retired at GAME's swap step 6) discards and counts it. PHYS counts nothing under `world.async_discarded`; the world host alone publishes it ([Z2-N-ARCH-558]). PHYS's own CUMULATIVE `physics.deferred.cancelled_at_detach` counts the cancelled completions it parked. A save restore re-issues the saved INBOX entries under their recorded completion IDs and allocates none ("Save and restore algorithm").

<!-- assumption: pending MOD (row PH23) -->
The module publication sink (`Zenith_WorldAttachContext::GetPublicationSink()`, the single sink of the Q76 (a) ruling) stays valid until `DetachWorld` returns and parks into the world's `Zenith_AsyncResultTable`, live or retired, so a park from `DetachWorld` completes the ID in the retired table.
<!-- /assumption -->

The result decoder `Zenith_PhysicsDecodeQueryResult` turns the 64-byte `Zenith_PhysicsQueryResult` into the Luau table `{ hit, count, entity, position, normal, distance, cancelled }` (seven fields, within `uSCRIPT_ASYNC_RESULT_MAX_FIELDS`); a zero-length payload, SAVE's "abandoned at a restore" publication (`Zenith2/Docs/Design/SAVE/Formats.md` → "Async section"), decodes as `{ hit = false, count = 0, cancelled = true }` with the other fields nil.

## Determinism rules

The guarantee is Z2-D-019 per physics backend (Z2-D-003): for one backend and one build, identical content and inputs give bit-identical physics results whatever the worker count, chunk layout and scheduling. The rules that make it hold:

| # | Rule | Where it is enforced |
|---|---|---|
| D1 | Every call that mutates a backend world is made in an order that is a function of simulation IDs and merge order alone: ingest in ascending simulation ID, pending operations in playback order, pre-substep work in ascending simulation ID | "Ingest algorithm", "Substep loop" |
| D2 | Body and constraint handles are a function of the live set and the saved per-slot generations alone: the lowest free index, that slot's generation + 1; the slot tables are saved and restored, and every Jolt body is created with the BodyID derived from its neutral handle, so an EXACT restore hands the first creation after a load the same handle and BodyID as the uninterrupted run | "Slot allocation"; `Zenith_PhysicsWorldBase` (Internal) |
| D3 | Constraint solve order is creation order (`m_uPriority` unique and increasing) | "Ingest algorithm" step 5 |
| D4 | The Jolt step runs on one thread (the compliant baseline; the conditional parallel step of PHYS/Backends.md "Conditional Jolt parallel step" keeps the rule through `mDeterministicSimulation`); the PhysX step's result is independent of how many drain leaves run its tasks (PhysX enhanced determinism) | PHYS/Backends.md |
| D5 | Contact records are reduced to a set independent of arrival order (sink keeps the lowest keys of a total record key that carries both bodies' user data and ragdoll part codes) and sorted before use; events are emitted in key order through simulation-ID merge keys | "Event emission" |
| D6 | Query results are a pure function of the candidate set (collector key; ties to the lowest simulation ID); queries never run concurrently with a mutation | `Zenith_PhysicsHitCollector`, "Frame-graph placement" |
| D7 | Controllers read only the previous step's world in phase 4, so one controller never observes another's in-flight move | PHYS/CharacterRagdollVehicle.md |
| D8 | No PHYS algorithm iterates a hash map, reads a clock, a thread identity, an address or a worker index for a result ([Z2-N-ARCH-884], [Z2-N-ARCH-885]) | review; `Test_PhysicsConformance_LayoutAndWorkerIndependent` |
| D9 | The floating-point environment is the fixed one on every thread that runs physics, and backends never change it ([Z2-N-CONTRACT-360]); the backends are compiled with precise floating point and no contraction (PHYS/Backends.md "Build options"); no simulation-affecting PHYS code (buoyancy, vehicles, the controller, ragdoll drives, the cook steps) calls a CRT transcendental (`<cmath>` `sin`, `cos`, `tan`, `atan`, `atan2`, `exp`, `log`, `pow`, `cbrt`): it uses CORE's deterministic functions, the orchestrator's X314-FMA3 ruling (CORE owns fixed-polynomial binary32/binary64 `sin`, `cos`, `atan2`, `exp`, `log` and `pow`, cross-doc item X314; PHYS calls them by the names CORE's follow-up ticket gives them); `std::sqrt`, which IEEE-754 rounds correctly, is allowed | `Test_PhysicsJolt_StepKeepsFloatEnvironment`, `Test_PhysicsPhysX_StepKeepsFloatEnvironment` |
| D10 | Every simulation-affecting tunable is a configuration key flagged simulation-affecting, recorded in the replay header | ComponentsAPI.md "Configuration keys" |
| D11 | Deferred query results are recomputed from committed state at a fixed tick; nothing physics computes depends on wall-clock arrival | "Deferred queries" |
| D12 | Memory pressure never changes physics state (simulation sheds nothing, `Zenith2/Docs/Architecture.md` → "Memory governor") | "Memory consumer" |

## Determinism boundaries

| Promised | Not promised |
|---|---|
| bit-identical per (backend, build, platform) across worker counts 1, 2, 8 and all, chunk capacities and compaction, including ticks with script failures | equality between Jolt and PhysX (Z2-D-003, [Z2-N-ARCH-882]) |
| an EXACT restore (Jolt) continuing bit-identically, including through destructions before the save and creations after the restore (D2) | equality between Windows and Android, or between configurations (Debug, Release, Shipping): Jolt is built with its cross-platform-deterministic floating-point settings (PHYS/Backends.md "Build options"), but the rest of the simulation (ECS iteration, scripts, the ingest of authored data) is held to per-build determinism only ([Z2-N-ARCH-882]), so no test asserts a cross-platform equality |
| a COHERENT restore (PhysX) that is consistent: same bodies, poses, velocities, sleep states, constraints and controller state, no new interpenetration beyond the solver slop | a PhysX restore continuing bit-identically (the saved world is marked `COHERENT_ONLY`) |
| identical event sequences for identical runs | identical event sequences when `physics.maxContactRecordsPerTick` or any simulation-affecting key differs |

## Large coordinates

Physics runs in single precision (Jolt without double precision, PhysX's float API) and adopts the programme's precision target as STREAM states it: "a simulation position keeps about 0.25 mm resolution", which holds for every |coordinate| below `uSTREAMING_PRECISION_RADIUS_METRES` = 4,096 m, where the binary32 spacing is at most 2^−12 m = 0.244 mm (`Zenith2/Docs/Design/STREAM/Runtime.md` → "Precision target", derived from `Zenith2/Docs/Design/CORE/Maths.md` → "Coordinate systems"). From that target:

| Distance from O (per axis) | binary32 spacing | Consequence for PHYS |
|---|---|---|
| below 4,096 m (the precision radius W) | ≤ 0.244 mm | the stability tolerances of PHYS/Tests.md hold unchanged (penetration slop 20 mm, stack drift 5 mm are ≥ 20 spacings) |
| 4,096 m to 8,192 m | 0.488 mm | supported without rebasing; tolerances unchanged |
| 8,192 m to 16,384 m (ENV's large terrain without rebasing, STREAM's row ST19) | 0.977 mm | supported; contact-point and query-distance tolerances widen to 2 mm |

A world whose content stays within W needs no rebasing (STREAM's default, `stream.rebase.enabled` false). A world that enables it is rebased by STREAM's rule, whose bound is STREAM's: "every such position satisfies `|p| ≤ 2,198 + R_g`, which is below W exactly when `R_g ≤ 1,898` m" (`Zenith2/Docs/Design/STREAM/Runtime.md` → "Rebase rule"); PHYS shifts with each rebase at the sync point that plays it ("Origin shift"). PHYS keeps sub-spacing velocity remainders inside the backend (both backends integrate in their own float state; the stored component position is STREAM's guarantee, not PHYS's). The large-coordinate tests run the stacking, controller and ragdoll scenarios at offsets 0, just inside W (4,000 m) and 12,000 m, and the rebasing scenario with STREAM's default quantum and interval (PHYS/Tests.md "Large-coordinate tests").

## Save and restore algorithm

The world's participant `Zenith.Physics.World` (`Zenith_PhysicsSaveParticipant`, one per world, registered at `AttachWorld` step 7 through `Zenith_WorldAttachContext::RegisterSaveParticipant` (`Zenith2/Docs/Architecture/CompositionRoot.md` → "Startup sequence": it forwards to the world's `Zenith_SaveSystem::RegisterParticipant` and is journaled and undone with the module), row SV17) implements `Zenith2/Docs/Design/SAVE/PublicAPI.md` → "Zenith_SaveParticipant header":

- `GetFlags`: 0 on Jolt (inclusion in every image kind, resume images among them, is SAVE's default; PHYS never sets `uSAVE_PARTICIPANT_FLAG_EXCLUDE_FROM_RESUME`), `uSAVE_PARTICIPANT_FLAG_COHERENT` when the backend lacks `PHYSICS_CAPABILITY_EXACT_STATE_RESTORE` (PhysX). PHYS does not override `GetCaptureFidelity`: SAVE's default reads that flag, so every Jolt capture is `SAVE_FIDELITY_EXACT` and every PhysX capture `SAVE_FIDELITY_COHERENT`, which sets the image's COHERENT_ONLY (the per-capture fidelity of the Q70 (a) ruling, narrowed by Q82 (b); `Zenith2/Docs/Design/SAVE/PublicAPI.md` → "Zenith_SaveParticipant header").
- `GetReissuedSourceKinds`: the bit of `ASYNC_SOURCE_PHYSICS_QUERY` (`1u << ASYNC_SOURCE_PHYSICS_QUERY`), so SAVE leaves every in-flight physics query in flight at a restore and PHYS re-issues it (`Zenith2/Docs/Design/SAVE/Formats.md` → "Async section").
- `DeclareAccess`: `ReadResource` of `Zenith.Physics.World`.
- `CaptureSection` (phase 11 or out of tick): write the section of PHYS/Formats.md "Save section layout": the header (with the applied origin and rebase count), the body and constraint slot tables (`SaveSlotTables`), every body record in ascending simulation ID, every constraint in ascending priority, controllers, ragdolls (each part's handles, constraint priority, motion type, pose, velocities, sleep and frozen state, "Ragdoll part identity"), vehicles, pair sets, the ended list `m_aulEndedPairIDs` (non-empty at a phase-11 capture whenever a sync-point playback removed bodies after this tick's emission), pending operations, the INBOX entries of the deferred inbox (an EXECUTED query is parked in the same node that executed it, so a phase-11 capture sees only INBOX entries and parked results, and parked results are SAVE's unpublished results), then, for EXACT backends, the backend's `SaveState` blob length-prefixed.
- `RestoreSection` (main thread, out of tick, after every entity and world resource exists, `WORLD_RESTORE_STATE_PARTICIPANTS`; `Zenith_PhysicsWorld`'s contract admits its mutating members on the main thread out of tick during a restore, PHYS/PublicAPI.md "Zenith_PhysicsWorld header"): decode and validate the section (untrusted: every count against the configuration capacities and the remaining bytes, `SAVE_ERROR_CORRUPT` otherwise); `RestoreSlotTables`; restore the module state (pending operations, gravity, origin record, pairs, the ended list, controller, ragdoll and vehicle records); re-issue every saved INBOX entry into `m_xDeferredInbox` under its recorded completion ID and call `xContext.m_pxReissue->MarkReissued(completionID)` for each; then record the body and constraint creations and return. It allocates no completion ID. A missing section (`m_bSectionPresent` false) resets the world to its component state. A mismatch between a recorded entity and the restored entity set is `SAVE_ERROR_CORRUPT`.
- `IsRestoreReady` (SAVE's `WORLD_RESTORE_STATE_PARTICIPANTS_READY`, polled once per restore `Step`): the bodies need their cooked shapes, and a restored entity's `Zenith.Physics.Collider` asset handle may still be loading. For every recorded body whose collider shape (or ragdoll definition) asset is neither ACTIVE nor FAILED, `RestoreSection` requested it through `Zenith_AssetLoader::RequestLoad` and kept the operation handle (`Zenith2/Docs/Design/ASSET/RuntimeAPI.md` → "Zenith_AssetLoader header"); `IsRestoreReady` returns false until every such asset is ACTIVE or FAILED, then, once: (1) runs ingest steps 2–6 over every entity with the recorded handles (`CreateBodyWithHandle`, `CreateConstraintWithHandle`, bodies in ascending simulation ID and constraints in ascending priority), serially on the calling thread as one leaf, because out of tick there is no `Zenith_JobContext` to fan step 2 out over; the serial scan concatenates and sorts the same rows, so it produces the same sorted change list by construction; (2) applies each body record's pose, velocities, sleep and frozen state and sets the written local values from the record; (3) for each ragdoll record whose State is not NONE, in ascending simulation ID and then part order, re-creates each part with `CreateBodyWithHandle(recorded handle, …)` from its definition (shape, mass, damping, layer, collision group 0, user data the character's simulation ID, `m_uRagdollPart` = part index + 1) with the recorded motion type, pose, velocities and sleep state (a part recorded frozen is created kinematic, asleep and QUERY_ONLY, and its recorded velocities become the record's saved ones, "Region freeze"), then each part's constraint to its parent with `CreateConstraintWithHandle(recorded handle, …)` at its recorded priority and CollideConnected false; the controller capsule keeps its recorded query-only filtering; a ragdoll whose definition asset FAILED is restored in NONE with no parts, its capsule's own filtering restored, counted `physics.save.restore_shape_failed_delta` and reported as below; (4) sets `m_bExcludedPairsDirty`, so the first ingest after the restore rebuilds the excluded pairs before any step ("Excluded pairs maintenance"); (5) calls `RestoreState(blob)` for EXACT, which overwrites every body's and constraint's simulation state, the parts' included, from the blob; and returns true. On PhysX (COHERENT) step 3's recorded state is the restored state, so a ragdoll restored in mid-fall continues falling from its saved pose and velocities. `ListRestoreWaits` returns the held operation handles, so SAVE's synchronous restore driver can service them (`Zenith2/Docs/Design/SAVE/RuntimeBridge.md` → "Synchronous restore driver"). A body whose shape asset FAILED is not created (its entity is bodiless, `physics.save.restore_shape_failed_delta`), which makes that restore non-identical, reported as below.
- **Fidelity fallback.** When `RestoreState` returns `PHYSICS_ERROR_STATE_MISMATCH` (a corrupted blob, or a blob of another data tag after a dependency update) or a shape failed, the world continues from the coherent restore it already performed, counts `physics.save.coherent_fallbacks`, logs once, and reports the downgrade to SAVE so the restored session is not treated as EXACT:

<!-- assumption: pending SAVE (row PH22) -->
`Zenith_SaveRestoreContext` gains `void ReportCoherentRestore(uint64 ulParticipantKey) const` (callable from `RestoreSection` and `IsRestoreReady`), with which a participant that captured EXACT tells the restore job that its restore is only COHERENT; the job then treats the restored world as restored from a COHERENT image: a replay resumed from it runs without hash comparison and reports that, and a seek through it falls back to FULL replay (the rules of the Q70 (a) ruling).
<!-- /assumption -->

`Test_PhysicsConformance_SaveRestoreExactContinuation[Jolt]` and `Test_PhysicsConformance_SaveRestoreCoherent[PhysX]` prove each mode; SAVE's `Test_SaveCrossSystem_PhysicsAndAnimationCoherent` runs both through the full save path.

## Slot allocation

Both backends allocate body and constraint handles through `Zenith_PhysicsSlotTable` (`ZenithPhysics/Internal/Zenith_PhysicsBackendBase.h`), one per world for bodies (capacity `m_uMaxBodies`) and one for constraints (`m_uMaxConstraints`):

- State: `uint32 m_uHighWater` (slots ever used, so slot indices `[0, m_uHighWater)`), a generation array `uint32 m_auGeneration[capacity]` (28 significant bits; 0 = never used), a live bitset, and a free bitset over `[0, m_uHighWater)` with a 64-ary summary word tree for "lowest set bit" in O(log₆₄ capacity).
- `Allocate()`: the lowest free index i if any, otherwise `m_uHighWater` (which then increments); a full table is the capacity error. The slot's generation becomes `generation + 1`, wrapping from 2^28 − 1 to 1 (never 0); the handle is (i, generation, world tag).
- `Release(i)`: clears live, sets free; the generation is kept for the next allocation of i.
- The handle a creation receives is therefore a function of the live set and the generation array alone, not of the order of earlier destructions (a LIFO free list was history-dependent and is not used).
- `SaveSlotTables` writes `m_uHighWater` and the generations of slots `[0, m_uHighWater)` for both tables (PHYS/Formats.md "Save section layout"); `RestoreSlotTables` sets them before any `CreateBodyWithHandle`, and each `CreateBodyWithHandle(h)` then marks h's slot live with h's generation (`PHYSICS_ERROR_STATE_MISMATCH` when the slot is live or its restored generation differs). After the restore, the free set is `[0, m_uHighWater)` minus the live slots, exactly as in the uninterrupted run, so the next `Allocate` returns the same handle.
- The Jolt backend creates every body, on every path, with `BodyInterface::CreateBodyWithID(JPH::BodyID(index, generation & 0xFF), settings)` from the neutral handle; Jolt's own BodyID allocator (`CreateBody`) is never called, so Jolt's body-manager free list, which `SaveState` does not capture, never decides an ID (PHYS/Backends.md "Jolt state save and restore"). The price is Jolt's own: `BodyManager::AddBodyWithCustomID` removes a reused index from that LIFO free list by walking it (the pinned release's `BodyManager.cpp` comments that the walk is O(N)), so re-creating F bodies after F destructions costs O(F²) pointer chases in the worst case (about 4·10⁶ for F = 2,000); the cell-churn case of `Zenith_Bench_BudgetsPHYS` measures it before Stage 4 (PHYS/Tests.md "Benchmarks and budget evidence").

## Memory consumer

`Zenith_PhysicsMemoryConsumer` implements `Zenith_MemoryConsumer` (`Zenith2/Docs/Architecture.md` → "Memory governor") for `MEMORY_SUBSYSTEM_SIMULATION`; `GetResidentBytes` = the service's `GetResidentBytes` + Σ worlds' `GetResidentBytes` + the module's tables and step arenas. At every pressure level it sheds nothing (the "Simulation" row of the table "Shedding per subsystem"); its row of `Zenith2/Tests/Memory/Internal/Zenith_MemorySheddingCases.h` (`axMEMORY_SHEDDING_CASES`, A5 row CB8) asserts that a forced CRITICAL level changes no physics state hash. Its budget is Z2-B-PHYS-001.

## Body lifecycle state machine

| State | Event | Next | Action |
|---|---|---|---|
| ABSENT | entity qualifies at ingest | LIVE | compose, create shape and body |
| ABSENT | creation refused (capacity) | ABSENT | count; retry on the entity's next change |
| LIVE | composition hash changes | LIVE | `SetBodyShape` with mass update; release the old shape |
| LIVE | InActiveRegion false | FROZEN | save velocities, kinematic, sleep, QUERY_ONLY filtering, end pairs and overlaps |
| FROZEN | InActiveRegion true | LIVE | own filtering restored, dynamic, restore velocities, wake |
| LIVE, FROZEN | teleport playback | same | set pose; zero velocities unless keep; reset contacts |
| LIVE, FROZEN | `DESTROY_PENDING` at a sync point before phase 5 | REMOVED | removed at this tick's ingest (handles still readable) |
| LIVE, FROZEN | `DESTROYED` (end of tick) | REMOVED | destroy body and constraints; the simulation ID enters `m_aulEndedPairIDs`, so its pairs end at the next emission ("Event emission" step 2) |
| LIVE, FROZEN | component removed, re-parented under another body | REMOVED → ABSENT | destroy at ingest step 3, the simulation ID entering `m_aulEndedPairIDs`; recomposed as a child or a new body (a new body of the same simulation ID that touches in the same step continues its pairs by the handover rule of "Event emission" step 3) |
| REMOVED | — | (record erased) | shape reference released |

## Constraint state machine

| State | Event | Next | Action |
|---|---|---|---|
| UNRESOLVED | both bodies resolve and are LIVE | ACTIVE | `CreateConstraint` |
| ACTIVE | a body is removed or frozen | UNRESOLVED | `DestroyConstraint` (re-created on resolve) |
| ACTIVE | Enabled set false | DISABLED | `SetConstraintEnabled(false)` |
| DISABLED | Enabled set true | ACTIVE | enable |
| ACTIVE | break threshold exceeded | BROKEN | disable, Broken = true, `JointBroken` |
| BROKEN | Enabled set (any value written by a command) | ACTIVE | re-create; Broken = false |
| any | joint component removed or entity destroyed | (erased) | destroy |

Every row that creates or destroys a constraint whose CollideConnected is false also sets `m_bExcludedPairsDirty` ("Excluded pairs maintenance"); disabling and breaking keep the constraint object, so its pair stays excluded.

## Deferred query state machine

| State | Event | Next | Action |
|---|---|---|---|
| PROVISIONAL | the invocation rolls back or the merge rejects the request | (discarded) | nothing submitted, no completion ID |
| PROVISIONAL | commit | INBOX | completion ID assigned; `Consume` appends |
| INBOX | `Physics.DeferredQueries` takes it (at most `physics.deferredQueriesPerTick` per tick, in completion-ID order; the rest stay INBOX) | EXECUTED | query run against this tick's post-step state |
| EXECUTED | park, in the same node | PARKED | `Park(completionID, ASYNC_SOURCE_PHYSICS_QUERY, PUBLICATION_KIND_GENERIC_RECOMPUTED, result)`: the ID is completed |
| PARKED | tick boundary | PUBLISHED | visible to `ticket:Result()` (SCRIPT owns the rest) |
| INBOX | `DetachWorld` (world swapped out or shut down) | (completed) | before the world state is destroyed, `Physics.DetachWorld` completes every INBOX entry in completion-ID order through the module publication sink, `Park(completionID, ASYNC_SOURCE_PHYSICS_QUERY, PUBLICATION_KIND_GENERIC_RECOMPUTED, the cancelled zero-hit result)`, into the world's table, which (retired at swap step 6 of GAME's swap) discards and counts it; PHYS counts nothing under `world.async_discarded` (it counts `physics.deferred.cancelled_at_detach`, its own counter) |
| PARKED | world swapped out | (discarded by the retired table) | nothing: the result was already completed into the table, which discards and counts it at `Retire`; PHYS counts nothing |
| INBOX | save captured | INBOX | recorded with its completion ID; at restore SAVE leaves it in flight (PHYS declares the source kind in `GetReissuedSourceKinds`) and `RestoreSection` re-issues it under the same ID and marks it re-issued |
| PARKED | save captured | PARKED | not recorded by PHYS: SAVE's async section saves the unpublished result and puts it back completed at restore |

These rows are cross-doc item X265's replacement rows, adopted with one wording difference: the cancelled result is the zero-hit result X265 names with `PHYSICS_QUERY_RESULT_FLAG_CANCELLED` set, so a script reading it (on a world that survives, which a detach never leaves) could tell a cancellation from a miss. EXECUTED never survives the node that entered it (the entry stays in the inbox until the same node parks it, and a started node is never stopped by a frame cancellation, "Deferred queries"), so no row handles EXECUTED at a detach, a capture or a cancelled frame. `Test_PhysicsIntegration_SwapCompletesInboxQueries` asserts the result of X265: after a swap with queries in INBOX, the retired table's `GetInFlightCount()` reaches 0 within the same prologue and `world.async_discarded` rises by the INBOX plus PARKED count, published by the world host alone.
