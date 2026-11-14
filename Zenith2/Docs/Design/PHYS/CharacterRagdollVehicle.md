# PHYS design — character controllers, ragdolls and vehicles

This file is part of the PHYS design family; the entry document is `Zenith2/Docs/Design/PHYS.md`. It specifies the three engine-side physics features that are written once in `ZenithPhysics` over the neutral `Zenith_PhysicsWorld` interface, so they behave the same way on both backends: the kinematic character controller and its phase-4 move, ragdolls (enter, drive, blend, recover, teleport) and the raycast vehicle.

## Scope of this file

| Topic | Heading |
|---|---|
| the controller's phase-4 move | "Character move algorithm", "Controller tuning rules" |
| controller and dynamic bodies | "Controller interaction with bodies" |
| ragdolls | "Ragdoll definition", "Ragdoll state machine", "Ragdoll enter procedure", "Powered ragdolls", "Ragdoll blend and root", "Ragdoll recover procedure", "Teleport during a ragdoll" |
| vehicles | "Vehicle algorithm" |

## Character move algorithm

`Physics.CharacterMove` (phase 4) runs, for every row of the query `WITH Zenith.Physics.CharacterController, Zenith.LocalTransform`, the pure function `Zenith_ComputeCharacterMove` (`ZenithPhysics/Internal/Controller/Zenith_CharacterMove.h`) over the world as the previous `Physics.Step` left it. Inputs: the component (configuration, input and last state), the entity's arbitrated `Zenith.LocalTransform` 1–2 as phase 6 of the previous tick left it (L₀), the controller record's written local bits (the exact `Zenith.LocalTransform` 1–2 bits `Physics.SyncTransforms` last wrote for it, `L_w`), the last TargetPosition and TargetRotation (world pose relative to O, the kinematic capsule's target), the parent's previous-tick `Zenith.WorldTransform` `W_parent` (identity for a root), the optional root-motion row, the tick index t and duration Δt, gravity g. Notation: up = +Y, r = Radius, h = Height, s = SkinWidth, the capsule centre is p + up·(h/2) and its half segment `hs = h/2 − r`.

1. **Start pose and external adoption.** The comparison is in the space PHYS wrote: when L₀ equals `L_w` bit for bit, the controller owned its transform last tick and the move starts from the exact world pose it targeted, `(p₀, q₀)` = (TargetPosition, TargetRotation), never from a pose recomposed through the parent (`W_parent · L₀` is not bit-equal to the target in binary32, and never under a non-uniform parent scale, so recomposing would make every parented controller an adoption every tick). When L₀ differs from `L_w`, another writer source owned the transform last tick (a ragdoll, a cinematic, a `SetProperty` command): adopt `(p₀, q₀) = W_parent · L₀`, set `L_w = L₀`, set Velocity to 0 and set ADOPTED_EXTERNAL; counter `physics.controller.adopted_external_delta`. Teleport playback sets the targets and `L_w` to the teleported pose, and the origin-shift listener shifts both with SCENE's formula (PHYS/Simulation.md "Origin shift"), so neither is an adoption. A parented controller is supported under a parent that does not move; under a moving parent its target is in world space and the parent's motion is not applied (the rule of dynamic bodies, PHYS/Simulation.md "External writes"); a platform the controller stands on is followed through step 4 instead.
2. **Suspension.** With input flag SUSPENDED, write the targets equal to `(p₀, q₀)`, Velocity 0, and stop.
3. **Intent.**
   - VELOCITY: `Δh = (DesiredVelocity.x, 0, DesiredVelocity.z) · Δt`; rotation `q₁ = q₀`, or the yaw `DesiredYawRadians` (`Zenith_MakeEuler(yaw, 0, 0)`) when USE_DESIRED_YAW.
   - ROOT_MOTION_WITH_GRAVITY: when root-motion Flags bit 0 (valid) is set and bit 1 (reset) is clear, `Δrm = Zenith_Rotate(q₀, DeltaPosition)` and `Δh = (Δrm.x, 0, Δrm.z)`; `q₁ = q₀ · yaw(DeltaRotation)` (the yaw component alone, `Zenith_ToEuler` then `Zenith_MakeEuler(yaw, 0, 0)`); without a valid row, Δh = 0 and q₁ = q₀. Flags bit 2 (spread step, `Zenith2/Docs/Design/ANIM/Runtime.md` → "Animation.ApplyRootMotion") is informational: a spread step is applied exactly like any other delta.
   - ROOT_MOTION: `Δ = Zenith_Rotate(q₀, DeltaPosition)` whole, `q₁ = q₀ · DeltaRotation` (yaw only, as above, so the capsule stays upright); vertical velocity is not integrated while grounded.
4. **Platform carry.** When the last state was grounded on a GroundEntity with a body: `Δplat = GroundVelocity · Δt`, and for a rotating ground body the yaw rate ω_y of its angular velocity rotates q₁ by `ω_y · Δt` and rotates the offset from the body's centre of mass by the same angle.
5. **Vertical velocity.** vy = Velocity.y. Grounded and no jump: vy = 0. A jump is requested when JumpRequestTick > LastJumpTick and JumpRequestTick ≤ t; it is taken when grounded or AirTicks ≤ CoyoteTicks: vy = JumpSpeed, LastJumpTick = t, set JUMPED. Otherwise vy = max(vy + g.y · GravityScale · Δt, −MaxFallSpeed). ROOT_MOTION mode grounded: vy = Δ.y / Δt.
6. **Desired displacement.** `D = Δh + Δplat + up · (vy · Δt)` (ROOT_MOTION: `D = Δ + Δplat` when grounded).
7. **Depenetration.** Up to 4 iterations: `CollideShape(capsule at p, max separation s)` with the controller's mask, ignoring its own capsule body and triggers; take the hits with depth > 0 in collector order; if none, stop; otherwise `p += Σ (normal · (depth + 1e-4))` over at most 8 hits, set DEPENETRATED; counter `physics.controller.depenetrations_delta`.
8. **Collide and slide.** remaining = D; for i in 0 … 3:
   1. `CastShape(capsule, from p, direction = normalize(remaining), distance = |remaining| + s)`; |remaining| < 1e-5 → stop.
   2. No hit: `p += remaining`; stop.
   3. Hit at distance d with normal n: `p += dir · max(0, d − s)`; `left = remaining · (1 − max(0, d − s)/|remaining|)`.
   4. **Step up** (grounded last tick, the hit is not walkable — `n · up < cos(MaxSlopeRadians)` — and the hit point is at most StepHeight above the feet): cast up by StepHeight (stop at a ceiling), cast along the horizontal part of `left`, cast down by StepHeight + s; when the down cast lands on a walkable normal, accept the stepped position (set STEPPED_UP) and continue with `left` minus the horizontal distance covered; otherwise discard the step attempt.
   5. **Slide:** `left −= n · (left · n)`; on a non-walkable hit while grounded, also remove the upward component (`left.y = min(left.y, 0)`), set ON_STEEP_SLOPE; a hit with `n · up < −0.7` while vy > 0 is a ceiling: vy = 0, set HIT_CEILING.
   6. Record the hit for "Controller interaction with bodies"; remaining = left.
9. **Ground probe and snap.** Cast down from p by `GroundSnapDistance + s`. A walkable hit when the last state was grounded or vy ≤ 0: `p.y −= max(0, d − s)`, Grounded = true, GroundNormal = n, GroundEntity = the hit entity, GroundVelocity = `GetPointVelocity` at the hit point (0 for static); AirTicks = 0. Otherwise Grounded = false, AirTicks += 1.
10. **Outputs.** TargetPosition = p, TargetRotation = q₁, Velocity = `((p − p₀) / Δt)` with its y replaced by vy when airborne, StateFlags as set.

The slope test's `cos(MaxSlopeRadians)` and the yaw extraction of `Zenith_ToEuler` / `Zenith_MakeEuler` use CORE's deterministic functions (rule D9 of PHYS/Simulation.md "Determinism rules"). Complexity: at most 4 + 4 × 3 + 1 shape casts and 4 collide queries per controller. Every query reads the immutable post-step world, and the outputs of one controller depend only on its own row and that world, so `ParallelFor` partitioning, worker count and chunk layout never change a result.

## Controller tuning rules

| Rule | Check | Failure |
|---|---|---|
| r > 0, h ≥ 2r, StepHeight < h − 2r, SkinWidth in (0, r/2) | at controller creation | the entity gets no controller, `physics.controller.invalid_delta` |
| MaxSlopeRadians in (0, π/2) | creation | as above |
| the entity carries neither `Zenith.Physics.RigidBody` nor its own `Zenith.Physics.Collider` (PHYS/Simulation.md "Body composition" rule 10: one body per (user data, part code)) | creation, and every change of either component (ingest step 6) | as above; an existing capsule is removed; the scene validation warns `PHYSICS_COOK_DIAG_CONTROLLER_WITH_BODY` (PHYS.md "Spawn clearance rule") |
| Δt ≤ 0.1 s | from the clock | asserted by JOBS' tick bounds |

## Controller interaction with bodies

- Each controller owns one kinematic capsule body (the record's `m_xBody`), moved in the substep loop to TargetPosition with `MoveKinematic`, so dynamic bodies collide with the character and are pushed with infinite mass.
- For every slide hit (step 8.6) on a dynamic body, `Physics.CharacterMove` records `Physics.AddImpulse` on that body: `impulse = −n · min(MaxPush, max(0, v_into) · Mass · PushStrength)` at the hit point, with `v_into = (D / Δt) · (−n)` and MaxPush = Mass × 10 N·s; commands are recorded through `Commands(controller simulation ID)`, so their merge order is the controllers' ascending simulation ID.
- Controllers see each other's capsules as kinematic bodies of the previous step and slide along them.

## Ragdoll definition

A ragdoll definition (asset kind `Physics.Ragdoll`, PHYS/Formats.md "Ragdoll file") lists up to 64 parts in parent-before-child order: each part binds one skeleton joint by name hash, has a shape (capsule, box, sphere or convex hull) with a local offset relative to the joint, a mass, linear and angular damping, and a constraint to its parent part (SWING_TWIST with twist and two cone limits, or HINGE for knees and elbows), with the joint frame at the child joint's origin. **Self-collision rule (one rule, both backends):** a part never collides with its parent part, and every other pair of parts of the same ragdoll collides. Each part's constraint to its parent is created with CollideConnected false, which puts the (part, parent) body pair into the world's excluded pairs (PHYS/Simulation.md "Excluded pairs maintenance") on Jolt and leaves `PxConstraintFlag::eCOLLISION_ENABLED` off on PhysX (PHYS/Backends.md "Constraint mapping"). Parts carry collision group 0 and no IGNORE_SAME_GROUP flag, so the general collision-group rule of PHYS/Backends.md "Jolt layer mapping" and "PhysX filter shader" never applies to them and no ragdoll-specific filter exists. `Test_PhysicsRagdoll_ParentChildNeverCollide[<Backend>]` asserts the rule.

**Part identity.** Every part body carries the character's simulation ID as its user data and `m_uRagdollPart` = part index + 1, and lives in the character's ragdoll record, never in the body table; contacts between two parts of one ragdoll are self-contacts that raise no event, and a part's contacts with other entities form the character's pairs (PHYS/Simulation.md "Ragdoll part identity"). Parts are created with `PHYSICS_BODY_FLAG_ALLOW_SLEEPING | PHYSICS_BODY_FLAG_REPORT_INTERACTIONS` (a falling body's impacts reach AUDIO and VFX) and without REPORT_CONTACTS. `Test_PhysicsRagdoll_PartContactsKeyedTotally` and `Test_PhysicsRagdoll_SelfContactsEmitNoEvents[<Backend>]` assert it.

## Ragdoll state machine

| State | Event | Next | Action (where) |
|---|---|---|---|
| NONE | `Physics.EnterRagdoll` playback | ENTERING | "Ragdoll enter procedure" (sync point) |
| NONE | enter refused (no `Zenith.Physics.Ragdoll` component on the entity at playback, no definition resident, no pose source, joint missing, the entity carries `Zenith.Physics.RigidBody` ("Ragdoll part identity" of PHYS/Simulation.md), cap `physics.maxActiveRagdolls` reached, the ragdoll is frozen by its region (PHYS/Simulation.md "Region freeze")) | NONE | `physics.ragdoll.enter_refused`, log entry naming the cause |
| ENTERING | each `Physics.RagdollBlend` run (phase 6), k = 1, 2, … counted from the first run after the playback | ENTERING | `w = min(1, w₀ + k · Δ_e)` with `w₀` the weight at the playback (0 from NONE) and `Δ_e = 1 / (enter blend seconds × tickHz)`; the first run after the playback already advances |
| ENTERING | w reaches 1 | ACTIVE | — |
| ACTIVE | `Physics.RecoverRagdoll` playback | RECOVERING | "Ragdoll recover procedure" |
| ENTERING | `Physics.RecoverRagdoll` playback | RECOVERING | the blend reverses from the current w ([Z2-N-ARCH-553]); parts made kinematic |
| RECOVERING | each `Physics.RagdollBlend` run (phase 6), k = 1, 2, … counted from the first run after the playback | RECOVERING | `w = max(0, w₀ − (k − 1) · Δ_r)` with `w₀` the weight at the recover playback (exactly 1 from ACTIVE) and `Δ_r = 1 / (recover blend seconds × tickHz)`: the first run after the playback leaves `w = w₀` (the weight ANIM's first `Animation.PostPhysicsBlend` after `LIFECYCLE_EVENT_RAGDOLL_RECOVER` reads, so its first recover tick shows exactly the ragdoll pose), and the decrement starts with the second run, after ANIM has read the unchanged weight (the PHYS-RECOVER ruling, row AN6 (e)) |
| RECOVERING | `Physics.EnterRagdoll` playback | ENTERING | reverses from the current w; parts made dynamic again with zero velocity |
| RECOVERING | w reaches 0 | NONE | `Physics.RagdollBlend` records `Physics.RemoveRagdollBodies`; its playback at sync point 6 destroys every part's constraint and body (constraints first, in part order), clears the record's parts, sets `m_bExcludedPairsDirty`, inserts the character's simulation ID into the ended list `m_aulEndedPairIDs` (PHYS/Simulation.md "Excluded pairs maintenance", "Event emission" step 2: the parts' pairs end, and the restored capsule's contacts in the next step continue them as a handover) and restores the controller capsule's own filtering |
| any but NONE | the character's RegionBound InActiveRegion false at ingest step 10 | same, frozen | every part frozen with its motion type and velocities saved; w and k held while frozen; thawed by the same table (PHYS/Simulation.md "Region freeze") |
| any but NONE | teleport playback | same | "Teleport during a ragdoll" |
| any but NONE | entity destroyed | (erased) | bodies and constraints destroyed with the entity by the `LIFECYCLE_EVENT_DESTROYED` listener, which sets `m_bExcludedPairsDirty` (PHYS.md "Lifecycle listener") |

The two closed forms are the whole schedule, evaluated in exactly these binary32 expressions so that every test can assert bit equality (`fW0` is the stored `float` weight at the playback, `fBlendSeconds` the enter or recover blend seconds, `uTickHz` the simulation clock's tick rate, k the run count as `uint32`):

```cpp
const float fDelta = (fBlendSeconds == 0.0f) ? 1.0f : 1.0f / (fBlendSeconds * (float)uTickHz);
const float fEnter = Zenith_Min(1.0f, fW0 + (float)k * fDelta);            // ENTERING, k = 1, 2, … (Zenith2/Docs/Design/CORE/Maths.md)
const float fRecover = Zenith_Max(0.0f, fW0 - (float)(k - 1u) * fDelta);   // RECOVERING, k = 1, 2, …
```

Each run recomputes w from k and the stored `fW0` (never by an incremental decrement), the state changes when `fEnter == 1.0f` or `fRecover == 0.0f`, and GAME's `Test_Lifecycle_Ragdoll_EnterAndRecoverBlend` cites these expressions. A blend time of 0 gives `Δ = 1` per run: ENTERING reaches w = 1 at k = 1 (the first phase 6 after the playback, so an instant ragdoll is visible at once), RECOVERING keeps `w₀` at k = 1 and reaches 0 at k = 2. A reversal (an enter during RECOVERING or a recover during ENTERING) restarts k at the reversing playback with `w₀` the weight it found. In RECOVERING the weight is non-increasing and reaches 0, lowered in phase 6 before ANIM's phase-6 nodes, which declare `After("Physics.RagdollBlend")` (row AN6). ENTERING keeps the immediate advance on purpose: ANIM's enter needs no first-tick identity (`Zenith2/Docs/Design/ANIM/Runtime.md` → "Ragdoll transitions"), and a delayed enter would hide a zero-blend ragdoll for one tick. GAME's `Test_Lifecycle_Ragdoll_EnterAndRecoverBlend` (in `Zenith2/Tests/LifecycleCharacters/`, `Zenith2/Docs/Design/GAME/PhaseContract.md`) asserts both forms exactly, the recover one delayed by one tick relative to the enter one (row GA3 (b)); PHYS's own `Test_PhysicsRagdoll_BlendScheduleClosedForms` asserts them against `Physics.RagdollBlend` with blend times 0, 0.1 s and 0.4 s and a reversal.

## Ragdoll enter procedure

The playback function of `Physics.EnterRagdoll` (step 7 of the sync point after the issuing phase):

1. Resolve the entity; stale → drop (`physics.command.stale_delta`).
2. Validate (the refusal row above). In RECOVERING, reverse (state ENTERING, parts dynamic) and stop.
3. Write Ragdoll 1 State = ENTERING and 2 BlendWeight = 0 through the playback access.
4. Dispatch `LIFECYCLE_EVENT_RAGDOLL_ENTER` (payload `&Zenith_RagdollEnterDesc`) through the world's `Zenith_LifecycleDispatcher` (`Zenith_SceneWorldState::GetLifecycleDispatcher`, `Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_LifecycleListener header"), for listeners that act on an enter (none of ANIM's: `GetEntryModelPose` returns ANIM's final model pose of the last phase 6, which no node changes before this sync point, so ANIM takes no snapshot, `Zenith2/Docs/Design/ANIM/Runtime.md` → "Ragdoll enter").
5. `GetEntryModelPose(simulationID, pose)`; for each part in definition order: `W_part = W_entity · pose[joint] · T(part offset)`, with W_entity the entity's current world transform; create a dynamic body (layer = Ragdoll 5, collision group 0, mass and damping from the definition, linear velocity = the entity's controller Velocity when it has one, else zero, user data = the character's simulation ID, `m_uRagdollPart` = part index + 1, the flags of "Ragdoll definition") and store its handle in the ragdoll record; then create each part's constraint to its parent in part order, each with CollideConnected false, storing its handle and priority, and set `m_bExcludedPairsDirty` (the next ingest rebuilds the excluded pairs before the parts are first stepped, PHYS/Simulation.md "Excluded pairs maintenance").
6. Record the impulse (m_uImpulseBone's part, `uPHYSICS_NO_IMPULSE_BONE` none) as a pending ADD_IMPULSE at the part's centre of mass, applied at the next ingest.
7. The controller, if any, gets input flag SUSPENDED semantics while the ragdoll is not NONE (its capsule body is disabled from collision by `SetBodyFiltering` with QUERY_ONLY, recorded in the controller record's `m_bCapsuleQueryOnly`), so the ragdoll is not pushed by its own capsule. The capsule thereby leaves the simulation, so the playback inserts the character's simulation ID into `m_aulEndedPairIDs`: the capsule's pairs end at the next emission, and the parts' contacts in that same step continue them as a handover, with no `ContactEnded` or `ContactBegan` (PHYS/Simulation.md "Event emission" steps 2–5).

Bodies therefore exist from this playback: issued in phases 1–4 they are simulated by phase 5 of the same tick, issued in phases 5–10 by phase 5 of the next ([Z2-N-ARCH-552]).

## Powered ragdolls

With DriveStrength k > 0, ingest step 11 reads `GetAnimatedModelPose` and, for each part with a parent, sets the constraint's position motor target to the parent-relative rotation of the animated pose (`inverse(R_parent_model) · R_child_model`, expressed in the constraint's frames), with motor torque limit `k × the definition's MaxMotorTorque` and the definition's frequency and damping. k = 0 turns motors off (a limp ragdoll). When the pose source returns false, the previous targets are kept (`physics.ragdoll.pose_unavailable_delta`).

## Ragdoll blend and root

`Physics.RagdollBlend` (phase 6, `WRITER_SOURCE_RAGDOLL`) advances w per the state machine and writes, for every ragdoll not in NONE, the entity's `Zenith.LocalTransform` 1–2 into the Ragdoll staging column so the character entity follows the ragdoll: in ENTERING and ACTIVE the root follows the root part: position = root part's joint world position projected along −up by the definition's root height offset, rotation = the yaw of the root part's forward axis (model front +Z); in RECOVERING the root stays at the pose captured at the recover playback. Both are world poses `W_root`; the value written is `W_root` itself for a root character and `inverse(W_parent) · W_root` for a character with a parent (a rider, a character attached to a moving hierarchy), with `W_parent` the parent's `Zenith.WorldTransform` of the previous tick, which the node reads through its `ReadAnyRow` declaration (PHYS/Simulation.md "Frame-graph placement"), the rule `Physics.SyncTransforms` applies to dynamic bodies, including its one-tick lag under a moving parent (PHYS/Simulation.md "Physics-to-transform sync"). ANIM converts the body poses (`GetRagdollBodyPoses`, valid in phase 6 after the step) into model space against this arbitrated transform (`Zenith2/Docs/Design/ANIM/Runtime.md` → "Ragdoll blend"). The precedence arbitration gives Ragdoll priority over Physics, and Cinematic over both.

## Ragdoll recover procedure

The playback function of `Physics.RecoverRagdoll`:

1. Resolve and validate (state ENTERING or ACTIVE; otherwise drop with `physics.ragdoll.recover_ignored_delta`).
2. Write State = RECOVERING; store the recover blend seconds, `w₀` = the current BlendWeight and k = 0 (the schedule of "Ragdoll state machine"); capture the root pose (as "Ragdoll blend and root").
3. Make every part kinematic at its current pose with zero velocity, so the ragdoll pose ANIM matches against stays still during the blend.
4. Dispatch `LIFECYCLE_EVENT_RAGDOLL_RECOVER` (payload `&Zenith_RagdollRecoverDesc`, whose `m_xGetUpClip` is a persistent ID ANIM resolves).
5. When w reaches 0 the parts and their constraints are removed by the `Physics.RemoveRagdollBodies` playback (state machine), the controller's capsule regains its filtering, and the controller adopts the root pose on its next move (step 1).

## Teleport during a ragdoll

PHYS's lifecycle listener on `LIFECYCLE_EVENT_TELEPORTED` for an entity with a live ragdoll: compute `ΔT = T_target · inverse(T_entity_before)`; apply it to every part (`SetBodyTransform` with `PHYSICS_ACTIVATION_ACTIVATE`), zero every part's velocity unless `TELEPORT_FLAG_KEEP_VELOCITY` (then rotate the velocities by ΔT's rotation), keep the state and w ([Z2-N-ARCH-551]), and move the captured recover root by ΔT. The parts therefore move rigidly by exactly the teleport's transform, and `Test_Scenario_TeleportDuringRagdoll` asserts it bit for bit (GAME's suite).

## Vehicle algorithm

Stage 4. A vehicle is a dynamic chassis body with `Zenith.Physics.Vehicle`; wheels are not bodies. Before each substep (pre-substep hook, ascending simulation ID), for each wheel w with attachment a (chassis space), steer angle δ (Steer × MaxSteerRadians, rate-limited to 4 rad/s), suspension axis −up_chassis:

1. **Suspension cast.** `CastShape(sphere of the wheel radius, from W_chassis · a, along −up_chassis, distance = rest + travel)` with GroundMask, ignoring the chassis body itself (the query filter's `m_aulIgnoreUserData` holds the chassis simulation ID; wheels are not bodies, so nothing else of the vehicle can be hit). Hit at distance d → compression `c = clamp(rest + travel − d, 0, travel)`, contact point, normal n, ground entity; no hit → c = 0, no contact.
2. **Spring and damper.** `F_s = SpringRate · c + DamperRate · (c − c_prev) / h`, clamped ≥ 0, applied along n at the contact point (on the chassis; the equal and opposite force goes to a dynamic ground body).
3. **Anti-roll.** For each axle pair (wheels 0–1, 2–3, …): `F_ar = AntiRollStiffness · (c_left − c_right)`, added to the left and subtracted from the right.
4. **Engine and gearbox.** Engine RPM = max(IdleRPM, mean driven-wheel angular speed × gear ratio × final drive × 60/2π); torque = Throttle × MaxEngineTorque × (1 − (RPM/MaxRPM)²) clamped ≥ 0; automatic shifting up at ShiftUpRPM, down at ShiftDownRPM (at most one shift per 0.5 s), reverse when Throttle > 0 is commanded with Brake > 0.5 at speed < 1 m/s.
5. **Wheel spin.** For each driven wheel: drive torque = engine torque × ratio × final drive / driven count; brake torque = Brake × BrakeTorque + Handbrake × HandbrakeTorque (applied against spin); `ω += (drive − brake·sign(ω) − F_x · radius) / Inertia · h`.
6. **Tyre forces.** Contact frame: forward f = steer-rotated chassis forward projected on the contact plane, side = n × f. Slip: longitudinal `κ = (ω r − v_f) / max(|v_f|, 1)`, lateral `α = atan2(v_side, max(|v_f|, 1))`. Forces: `F_x = Grip · N · μ_x(κ)`, `F_y = −Grip · N · μ_y(α)` with N = F_s and the curves `μ(x) = D · sin(C · atan(B x))` (B = 10, C = 1.9, D = 1 for x = κ; B = 8, C = 1.3, D = 1 for x = α), combined friction circle `|F| ≤ Grip · N`. Apply at the contact point.
7. Write WheelStates (compression, ω, rotation, steer, slips, contact) at write-back; EngineRPM and Gear.

Every quantity is computed from the previous substep's state in fixed wheel order with CORE maths, the tyre curves' `sin`, `atan` and `atan2` through CORE's deterministic functions and never the CRT's (rule D9 of PHYS/Simulation.md "Determinism rules", the X314-FMA3 ruling), so the vehicle is deterministic per backend and identical in algorithm on both.
