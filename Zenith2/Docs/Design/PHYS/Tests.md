# PHYS design — tests

This file is part of the PHYS design family; the entry document is `Zenith2/Docs/Design/PHYS.md`. It is the test catalogue of the Physics module and its two backends: which test target each test lives in, the shared scenarios, the determinism replay, the named stability, tunnelling, substep, large-coordinate, handle, controller, ragdoll, save, failure-injection and cook tests, the integration suite, and the benchmarks that produce the budget evidence. Every name here is unique in the programme; ANIM's `Test_Ragdoll*` and `Test_Anim*` names, GAME's `Test_Lifecycle_*` and `Test_Scenario_*` names and SAVE's `Test_Save*` names are theirs, and PHYS only contributes assertions to the ones this file lists under "Tests PHYS contributes to".

## Scope of this file

| Topic | Heading |
|---|---|
| where tests live | "Test targets", "Test levels", "Test support" |
| module unit tests | "Handle and table tests", "Configuration and layer tests", "Ingest and sync tests", "Event and trigger tests", "Query tests", "Deferred query tests", "Binding tests", "Material and interaction tests", "Component and asset tests", "Loader tests" |
| per-backend cases | "Conformance suite", "General conformance cases", "Scenario definitions", "Determinism replay", "Stacking stability tests", "Tunnelling tests", "Substep and hitch tests", "Large-coordinate tests", "Character controller tests", "Ragdoll tests", "Vehicle and buoyancy tests", "Save and restore tests", "Failure injection tests", "Backend unit tests" |
| data | "Cook and format tests" |
| beyond the library | "Integration suite", "Tests PHYS contributes to", "Benchmarks and budget evidence", "Gate steps for PHYS diffs" |

## Test targets

| Target | Sources | Links | Runs |
|---|---|---|---|
| `ZenithPhysics_Tests` | `Zenith2/Engine/ZenithPhysics/Tests/**` except `Conformance/` and `Support/` | ZenithPhysics, its declared dependencies, `ZenithECS_Custom` (a test dependency), `ZenithTestSupport`, doctest | the module against `Zenith_PhysicsFakeBackend` in headless test worlds of `Zenith_PhysicsTestWorld.h` ("Test support"), and the module's types as themselves; no third-party engine |
| `ZenithPhysics_Jolt_Tests` | `Zenith2/Engine/ZenithPhysics_Jolt/Tests/**` | the Jolt backend and its dependencies | Jolt-specific unit tests ("Backend unit tests") |
| `ZenithPhysics_PhysX_Tests` | `Zenith2/Engine/ZenithPhysics_PhysX/Tests/**` | the PhysX backend | PhysX-specific unit tests, on the presets PhysX supports |
| `ZenithPhysics_Conformance` | `Zenith2/Engine/ZenithPhysics/Tests/Conformance/**` | ZenithPhysics (and through it ZenithJobs) and both backends ([Z2-N-CONTRACT-041]) | the **backend-level** cases ("Test levels"), each once per backend as `<name>[Jolt]` and `<name>[PhysX]` ([Z2-N-CONTRACT-407]), over a `Zenith_PhysicsWorld` stepped by `Zenith_PhysicsConformanceHarness.h`; no ECS world, no runtime |
| `Zenith_Integration_Physics` | `Zenith2/Tests/Physics/` (`zenith_add_integration_tests(Physics DEPENDS ZenithRuntime ZenithPhysics ZenithPhysics_Jolt ZenithPhysics_PhysX ZenithScene ZenithECS_Custom ZenithECS_Flecs ZenithECS_EnTT ZenithSave ZenithScript ZenithStreaming)`) | the composed runtime | the **module-level per-backend** cases ("Test levels"), each `[<Backend>]` case registered as `<name>[Jolt]` and `<name>[PhysX]` (the PhysX variant only on presets where `ZenithPhysics_PhysX` is supported), plus the cases of "Integration suite" (its registration rule) |
| `ZenithCook_Tests` (PHYS's files) | `Zenith2/Tools/ZenithCook/Tests/Physics/**` | the cook | "Cook and format tests" (the cook steps and writers; the runtime loaders' tests are in `ZenithPhysics_Tests`, "Loader tests") |
| `Zenith_Bench_PhysicsBackends` | `Zenith2/Benchmarks/PhysicsBackends/` | both engines directly (benchmark target) | the D-SEL-PHYSX-ANDROID harness (PHYS/Backends.md "PhysX on Android evaluation"); its `--smoke` CTest |
| `Zenith_Bench_BudgetsPHYS` | `Zenith2/Benchmarks/Budgets/PHYS/` | the composed runtime | the budget-row measurements ("Benchmarks and budget evidence"); its `--smoke` CTest |

Every test is pass/fail with no pinned count ([Z2-N-CONTRACT-408]); every test arrives in the same ticket as the code it covers. A test that cannot run on a preset (PhysX on `linux`) is not registered there; it never reports a pass it did not run.

## Test levels

Each case lives at the lowest level that can run it, and its table row says where (column "Target": **Unit** = `ZenithPhysics_Tests`, **Conformance** = `ZenithPhysics_Conformance`, **Integration** = `Zenith_Integration_Physics`; a table without the column is the target its heading names):

| Level | A case belongs here when it needs | Target | Built with |
|---|---|---|---|
| backend-level | a `Zenith_PhysicsWorld`, bodies, constraints, shapes, queries, the contact sink and a `Zenith_JobContext`, and nothing else: no component, node, sync point, listener, pose source, SCENE playback, ECS chunk layout or simulation clock | Conformance | the body-level builders of `Zenith_PhysicsScenarios.h` |
| module-level, per backend | any of those module or world pieces and a real backend's collision response | Integration | `Zenith_PhysicsTestWorld.h` with the Jolt or PhysX factory |
| module-level, backend-free | the module's own logic (ordering, tables, events, schedules, bindings) with no collision response, or a record stream it builds by hand | Unit | `Zenith_PhysicsTestWorld.h` with the fake backend's factory, or the type under test alone |

The determinism replay, the controllers, the ragdolls, the region freeze, the substep loop's kinematic and hitch behaviour, the vehicles, the origin-shift listener and every rebase-tick case are module-level; the stacks, piles, joints, filters, triggers at body level, queries, heightfields, tunnelling of plain bodies and the backend's own origin shift are backend-level.

## Test support

`Zenith2/Engine/ZenithPhysics/Tests/Support/` holds header-only helpers (`inline` functions and templates, compiled by no target of their own, [Z2-N-ARCH-255] and the X184 ruling), included by `ZenithPhysics_Tests`, `ZenithPhysics_Conformance`, the backends' test targets and `Zenith_Integration_Physics`:

| Header | Content |
|---|---|
| `Zenith_PhysicsScenarios.h` | the body-level builders of "Scenario definitions": each a function from a seed and a `Zenith_PhysicsWorld&` to the created bodies and constraints |
| `Zenith_PhysicsConformanceHarness.h` | `Zenith_PhysicsConformanceRun`: creates a `Zenith_JobSystem` with the case's worker count (`Zenith_JobSystemDesc::m_uWorkerCount`), the backend service through `Zenith_CreatePhysics_Jolt` or `Zenith_CreatePhysics_PhysX` and one world, and runs each tick as one `Zenith_JobSystem::RunRoot` iteration (`Zenith2/Docs/Design/JOBS/PublicAPI.md` → "Zenith_JobSystem") whose root job runs n substeps of h = 1/60 s ÷ n with the module's continuation chain (`StepOnce`, then `ResumeStep` continuations while pending, PHYS/Simulation.md "Substep loop"); a case asserts the contact sink's records, body states and query results |
| `Zenith_PhysicsTestWorld.h` | `Zenith_PhysicsTestWorld`: a headless world with the Physics module attached, created through MOD's runtime test access (`Zenith_RuntimeTestAccess::Create` with a hand-built composition naming the Physics module, the requested physics factory (fake, Jolt or PhysX) and the requested ECS backend, `Zenith2/Docs/Design/MOD.md` → "Zenith_RuntimeTestAccess.h"), the same pattern as AI's test world; it sets `ecs.chunkCapacityRows` on the test command line, forces compaction through `Zenith_WorldTestAccess::ForceCompaction` (`Zenith2/Docs/Design/ECS/WorldAPI.md` → "Zenith_World header"), records commands from a test system, runs ticks, and installs `Zenith_PhysicsTestPoseSource` (a `Zenith_RagdollPoseSource` serving the fixed 15-joint skeleton of the test ragdoll definition, a legal implementation of a PHYS interface, never a double of a concrete class). It also holds the entity form of every scenario |
| `Zenith_PhysicsFakeBackend.h` | a `Zenith_Physics` / `Zenith_PhysicsWorld` implementation with no collision response, whose state model is: slot identity and handles exactly as `Zenith_PhysicsWorldBase` allocates them (lowest free index, per-slot generations, per-slot user data and part codes); gravity from the world descriptor and `SetGravity` (the cases that need a resting body set `physics.gravityY` = 0); explicit Euler integration of every awake dynamic body at h; kinematic bodies at exactly their `MoveKinematic` target after the step; `SetSleeping` honoured (a sleeping body does not integrate and reports sleeping in `GetBodyState`) and automatic sleep after `physics.sleepSeconds` with linear and angular speed below `physics.sleepVelocity`; queries answered analytically against each body's bounding sphere; every call recorded in order with its arguments (the call log `Test_PhysicsIngest_ExcludedPairsDirtyOnEveryPath` reads); failures injected by call index; and `ScriptRecords(uint32 uSubstep, Zenith_Span<const Zenith_PhysicsContactRecord> axRecords)`, a test-only member of the fake world (not of the interface) whose records the next `StepOnce` of that substep index appends to the step's `Zenith_PhysicsContactSink` in span order, so module-level Unit cases feed hand-built record streams through the real emission path |
| `Zenith_PhysicsStateHash.h` | the canonical hash of "Determinism replay" (a body-and-constraint part for backend-level cases, plus controllers, ragdolls and the event sequence for module-level ones) |
| `Zenith_PhysicsSeedFiles.h` | minimal valid `.zcol`, `.zpmat` and `.zragdoll` section sets written field by field from PHYS/Formats.md and wrapped by ASSET's cooked-file writer, for "Loader tests" |
| `Zenith_PhysicsSaveSectionV<N>.h` | one per superseded save-section version, from the diff that supersedes it (PHYS/Formats.md "Save section layout"); none exists while the section is at version 1 |

- `Zenith_Physics`, `Zenith_PhysicsWorld` and `Zenith_RagdollPoseSource` are interfaces, so the fake backend and the test pose source are legal implementations; no test derives from or replaces a concrete class (the module's world state, the collectors and the sinks are tested as themselves; the FINAL-DOUBLES ruling).
- The fake and the helpers are never linked into a shipping target (`TestSeparationCheck`).
- No test reads a committed binary file: seed files and older save sections are generated at test time (the Q55 ruling).

## Handle and table tests

`ZenithPhysics_Tests`, for the handle codec and the module's tables (PHYS/PublicAPI.md "Zenith_PhysicsTypes header"):

| Test | Asserts |
|---|---|
| `Test_PhysicsHandle_BitLayoutRoundTrips` | index (23 bits), generation (28 bits) and world tag round-trip through the codec at 0, 1 and each maximum |
| `Test_PhysicsHandle_StaleAfterDestroy` | a destroyed body's handle is `PHYSICS_ERROR_STALE_HANDLE` on every member that takes it; the slot's next handle differs in generation |
| `Test_PhysicsHandle_GenerationWrapSkipsZero` | after 2^28 − 1 reuses of one slot the generation wraps to 1, never 0, and the slot is retired when the world says so |
| `Test_PhysicsHandle_WrongWorldRejected` | a handle stamped with another world's tag is `PHYSICS_ERROR_WRONG_WORLD` |
| `Test_PhysicsHandle_NullHandleRejected` | the null handle is `PHYSICS_ERROR_NULL_HANDLE`, never an assert |
| `Test_PhysicsTable_BodyCapacityExactlyReached` | `m_uMaxBodies` bodies succeed, the next is `PHYSICS_ERROR_BODY_CAPACITY` and leaves the world unchanged |
| `Test_PhysicsTable_ConstraintCapacityExactlyReached` | the same for constraints (`PHYSICS_ERROR_CONSTRAINT_CAPACITY`) |
| `Test_PhysicsTable_ShapeReleaseIsRefcounted` | a shape used by two bodies survives the first body's destruction and is released after the second |
| `Test_PhysicsTable_SimulationIdOrderStable` | simulation IDs are dense, ascending in creation order, and survive compaction of the ECS tables |
| `Test_PhysicsTable_SlotAllocationLowestFree` | after creating 10 bodies and destroying indices 7, 2 and 5 in that order, the next three creations receive indices 2, 5 and 7 with generations 2; destroying the same three in the order 5, 7, 2 instead yields the same three handles (allocation is independent of destruction order); the same for constraints |
| `Test_PhysicsTable_SlotTablesRoundTrip` | `SaveSlotTables` then `RestoreSlotTables` into an empty world, followed by `CreateBodyWithHandle` of the live handles, makes the next 20 `CreateBody` calls return exactly the handles the original world returns; a high-water mark above the capacity and a restore into a world with a live body are `PHYSICS_ERROR_STATE_MISMATCH` |
| `Test_PhysicsTable_WorldTagRange` | `CreateWorld` accepts tags 1 and 4,095 and refuses 0 and 4,096 with `PHYSICS_ERROR_INVALID_DESC`; a handle of tag 4,095 round-trips |
| `Test_PhysicsTable_SlotIdentityKeptUntilReuse` | the world base's per-slot user data and part code are written by `CreateBody`, `CreateBodies` and `CreateBodyWithHandle`, survive `DestroyBody`, and are replaced only when the slot is allocated again; `m_uRagdollPart` 65 is `PHYSICS_ERROR_INVALID_DESC` |
| `Test_PhysicsModule_HeadlessRuns` | (fake backend) a headless world with the module attached runs 60 ticks; every node, resource, command kind, binding and the memory consumer is registered |
| `Test_PhysicsCommands_RecordAndPlayback` | each of the eight command kinds records its payload and plays back in merge order; unresolved kinds record nothing and count |
| `Test_PhysicsCommands_PlaybackFindsWorldByTag` | two attached worlds (tags 3 and 9); an impulse recorded in each plays back into its own world's pending operations alone (the playback keys the module state by `Zenith_WorldPlaybackAccess::GetWorldTag`); a command whose world was detached is dropped and counted `physics.command.no_world_delta` |
| `Test_PhysicsLifecycle_TeleportMovesBody` | a teleport moves the body, zeroes its velocities, resets a controller's ground state and is not counted as an external write |
| `Test_PhysicsDump_SortedAndHashed` | `physics.dump` lists bodies in simulation-ID order and prints the canonical state hash |

## Configuration and layer tests

| Test | Asserts |
|---|---|
| `Test_PhysicsConfig_SubstepRangeEnforced` | `physics.substepsPerTick` 0 and 9 are refused by the schema; 1 and 8 are accepted |
| `Test_PhysicsConfig_StepLongerThanMaxRefused` | a tick of 0.25 s with 1 substep (h > `fPHYSICS_MAX_STEP_SECONDS`) is refused at attach with `PHYSICS_ERROR_INVALID_DESC` |
| `Test_PhysicsLayers_ParseNamesAndPairs` | the grammar of PHYS.md "Layer configuration" builds the expected symmetric table |
| `Test_PhysicsLayers_RejectsUnknownName` | a pair naming an undeclared layer is `PHYSICS_ERROR_LAYER_CONFIG` with the token's position |
| `Test_PhysicsLayers_ParserLimits` | 32 names accepted, 33 refused; 1,024 pair tokens accepted, 1,025 refused; 8 KiB accepted, 8 KiB + 1 refused ([Z2-N-ARCH-953]) |
| `Test_PhysicsLayers_ScriptNameTableMatches` | the script name table `physicsLayer` lists the layer names in declaration order |

## Ingest and sync tests

| Test | Target | Asserts |
|---|---|---|
| `Test_PhysicsIngest_ComposesCollidersOfSubtree` | Unit | colliders on descendants without their own rigid body are composed into the ancestor body's compound in ascending simulation ID |
| `Test_PhysicsIngest_ExternalWriteTeleportsBody` | Unit | a local transform written by another writer since the last sync (a root body and a body under a static parent) moves the body to `W_parent · L`, zeroes nothing else and counts `physics.external.pose_delta` once |
| `Test_PhysicsIngest_ParentedBodyNoSpuriousExternalWrite` | Unit | (fake backend, `physics.gravityY` = 0, so the body is at rest and falls asleep by the fake's sleep model) a dynamic body and a character controller under a static parent with non-uniform scale (1, 2, 0.5) and a 30° rotation, and the same pair as roots, run 300 ticks: `physics.external.pose_delta` and `physics.controller.adopted_external_delta` stay 0, the body falls asleep (no ACTIVATE was issued) and the controller's Velocity is never zeroed by an adoption |
| `Test_PhysicsSync_DynamicBodyIgnoresMovingParent` | Unit | (fake backend, `physics.gravityY` = 0) a resting dynamic body under a parent that translates 0.1 m per tick: the body's simulated pose never moves with the parent (world space), its world transform equals the body pose within 1e-5 m in every tick except the tick the parent moves, `physics.sync.parent_moved_delta` counts each rewrite, and `physics.external.pose_delta` stays 0 |
| `Test_PhysicsIngest_PendingOpsAppliedInOrder` | Unit | impulses, forces and velocity sets recorded from several nodes apply in command-merge order |
| `Test_PhysicsIngest_PendingOpsCapacity` | Unit | `physics.maxPendingOpsPerTick` operations apply; the next is dropped and counted |
| `Test_PhysicsIngest_ExcludedPairsDirtyOnEveryPath` | Unit | through the fake backend's call log: each path of PHYS/Simulation.md "Excluded pairs maintenance" (a joint entity's constraint changed, destroyed by its entity's `DESTROYED` event, a ragdoll entered, its parts removed by `Physics.RemoveRagdollBodies`, a body frozen and thawed, a restore) is followed, before the next `StepOnce` and after every `CreateBody` of that ingest, by exactly one `SetExcludedPairs` whose span equals the pairs of the live CollideConnected-false constraints; a tick that changes none calls it 0 times; no `SetExcludedPairs` is ever called outside ingest step 12 |
| `Test_PhysicsWriteBack_ActiveBodiesOnly` | Unit | write-back touches RigidBody 20–22 of active bodies only, 23 only on a frozen-state change, and never a transform |
| `Test_PhysicsSync_RotatedBoxKeepsRotation[<Backend>]` | Integration | an oriented box at rest on a slope keeps its full rotation in `Zenith.LocalTransform` and in the saved scene bytes (the old engine's AABB-forced-identity lesson) |
| `Test_PhysicsSync_ChildBodyWritesLocalPose` | Unit | a body whose entity has a parent writes `inverse(W_parent) · W_body` |
| `Test_PhysicsSync_SleepingBodyNotWritten` | Unit | a sleeping body writes nothing and the arbitration keeps the previous value |
| `Test_PhysicsFreeze_FrozenBodyCollidesWithNothing[<Backend>]` | Integration | PHYS/Simulation.md "Region freeze": an awake box dropped onto a body frozen through `Zenith.Streaming.RegionBound` passes through it with no contact and no trigger event, and collides again after the thaw |
| `Test_PhysicsFreeze_RegionBoundFreezesAndRestores` | Unit | `InActiveRegion = false` freezes a moving body (kinematic, asleep, query-only filtering, velocities saved); `true` restores its own filtering and the saved velocities exactly; the same for an ACTIVE ragdoll whose character carries RegionBound (every part frozen with its motion type and velocities saved, the character's pairs ended once, w and k unchanged and no drive target set while frozen, an enter or recover playback refused, every part restored exactly at the thaw) |
| `Test_PhysicsCells_CellContentBodiesFollowSyncPoints` | Unit | a COLLISION-grid cell's static colliders and triggers exist exactly between the cell's activation and deactivation sync points |

## Event and trigger tests

Unit cases run in `ZenithPhysics_Tests`, their record streams built by hand and fed through the fake backend's `ScriptRecords` ("Test support") into the step's `Zenith_PhysicsContactSink`, so report order and content are controlled; Integration cases run per backend:

| Test | Target | Asserts |
|---|---|---|
| `Test_PhysicsEvents_SortedByKey` | Unit | the events of one tick (began, ended, trigger, interaction and joint-broken events over 30 pairs and 3 joints, fed in a shuffled report order) are delivered in ascending event key ID, then group order began/entered → ended/exited → interaction → joint broken, then ascending high simulation ID, identically for 10 shuffles |
| `Test_PhysicsEvents_SinkKeepsLowestKeys` | Unit | with `physics.maxContactRecordsPerTick` = 4 and 10 reports, the 4 lowest keys are kept and 6 are counted dropped |
| `Test_PhysicsEvents_EndedAfterDestroy` | Unit | destroying one body of a touching pair emits `ContactEnded` with the destroyed entity's handle, by every path of the ended list: an entity destroyed at the end-of-tick playback (the `DESTROYED` listener), a `RigidBody` removed (ingest step 3), a body frozen (ingest step 10) and a ragdoll's parts removed by `Physics.RemoveRagdollBodies`; in each the pair ends exactly once, in the next emission |
| `Test_PhysicsEvents_DestroyedBodyRecordsDropped[<Backend>]` | Integration | PHYS/Backends.md "Jolt contact listener", "PhysX contact reporting": run 1, an entity touching the floor is destroyed in the same ingest that creates a box at its freed body index (asserted through the handle); run 2, the entity is destroyed at the end of tick t and a ragdoll entered at sync point 1 of tick t + 1 receives its freed index for a part. In both: exactly one `ContactEnded` (entity, floor); the box's or the part's floor pair begins exactly once; no pair count changes because of the destroyed body's late report; `physics.contacts.stale_removed_delta` rises (Jolt) or the removed-actor pair is dropped without a `userData` read (PhysX, under `win-asan`) |
| `Test_PhysicsEvents_HandoverKeepsPair` | Unit | hand-built records: a ragdoll's parts are removed by `Physics.RemoveRagdollBodies` while it lies on a dynamic crate, and in the next step its restored capsule's ADDED record against the crate arrives; and, reversed, a character entering a ragdoll on the crate (the capsule's late REMOVED record and the parts' ADDED records in one step): no `ContactEnded` or `ContactBegan` for (character, crate) in either, the pair keeps the new record's point and normal, and `physics.contacts.handover_delta` counts 1 each time |
| `Test_PhysicsEvents_PairSurvivesSleepAndWake[<Backend>]` | Integration | PHYS/Simulation.md "Sleeping": a `REPORT_CONTACTS` box lands on a static floor (one `ContactBegan`), falls asleep (RigidBody 22), stays asleep for 120 ticks, is woken by `Physics.SetSleeping(false)` (no event), and is lifted by `Physics.SetVelocity` (exactly one `ContactEnded`); the same with a 3-box sleeping stack, whose two box–box pairs and floor pair emit nothing through sleep and wake |
| `Test_PhysicsEvents_SuspendedContactsResolveAtWake` | Unit | hand-built records on a pair whose box the fake puts to sleep: a sleep-flagged REMOVED (record flag bit 1) changes neither the count nor any event and raises `m_uSuspendedContacts`; at the wake an ADDED re-confirms it (no event); in a second run no ADDED arrives at the wake, and the pair ends with one `ContactEnded` in that tick; a sleep-flagged REMOVED of a body in the ended list changes nothing |
| `Test_PhysicsEvents_InteractionOnlyBodiesRecorded` | Unit | a pair whose bodies carry REPORT_INTERACTIONS and not REPORT_CONTACTS produces records, raises its `Interaction` events and no `ContactBegan`; a pair with neither flag produces no record |
| `Test_PhysicsRagdoll_PartContactsKeyedTotally` | Unit | two parts of one ragdoll (user data 7, part codes 3 and 5) touching one ground triangle (user data 2, part code 0, the same sub-shape) and each other, fed in 10 shuffled orders with sink capacities 64 and 3: the record keys are pairwise distinct, the kept set and the sorted order are identical for every shuffle, the floor pair (2, 7) begins once, and the part–part record is a self-contact counted `physics.contacts.self_delta` with no event |
| `Test_PhysicsTriggers_EnterExitOncePerOverlap` | Unit | an entity passing through a trigger yields exactly one `TriggerEntered` and one `TriggerExited` |
| `Test_PhysicsTriggers_TriggerDestroyedEndsOverlaps` | Unit | destroying a trigger emits `TriggerExited` for every overlapping entity in ascending order |
| `Test_PhysicsTriggers_SleepingBodyStaysInside` | Unit | a body falling asleep inside a trigger emits no exit |
| `Test_PhysicsJoints_BreakEmitsOnce` | Unit | a joint whose impulse exceeds its threshold breaks after the substep, emits `JointBroken` once and writes Joint 21 |

## Query tests

| Test | Asserts |
|---|---|
| `Test_PhysicsQuery_ClosestHitTieBreak` | two hits at equal distance return the lower simulation ID (GA3) |
| `Test_PhysicsQuery_AllHitsSortedAndCapped` | `RayCastAll` returns hits in ascending key order, capped at the span, and the kept set is the K smallest |
| `Test_PhysicsQuery_IgnoreRootSubtree` | a query with an ignore root never hits the root or its descendants |
| `Test_PhysicsQuery_InvalidArgumentsCounted` | NaN origins, non-unit directions and negative radii return false and count `physics.query.invalid_delta`; no assert |
| `Test_PhysicsQuery_HitOnPartMapsToCharacter` | hits on a controller capsule and on a ragdoll part map to the character's entity through `m_xControllers` and `m_xRagdolls`; a hit whose user data names no live record is skipped |
| `Test_PhysicsQuery_PrimitiveCacheBounded` | single-threaded, PHYS/Simulation.md "Primitive cache": at most 4,096 entries; a miss inserts with one pin; the victim of a full cache is the unpinned entry with the lowest last use; a pinned entry is never a victim and its shape stays valid; with every entry pinned the query gets an uncached shape it releases itself (`physics.query.primitive_uncached_delta`); a duplicate created by a racing miss is released, not inserted |
| `Test_PhysicsQuery_PrimitiveCacheConcurrentStress` | "Gate steps for PHYS diffs" |
| `Test_CollisionQuery_SweepMatchesSphereAndRayCast` | `Zenith_CollisionQuery::Sweep` with radius 0 equals `RayCast`, and with radius r > 0 equals `SphereCast` of radius r, with the same layer mask and ignore root (the GAME camera's interface) |
| `Test_FluidQuery_AbsentResourceNoBuoyancy` | without a `Zenith.FluidQuery` resource a buoyancy body integrates as a plain body |

## Deferred query tests

| Test | Asserts |
|---|---|
| `Test_PhysicsDeferred_ResultAtFixedTick` | a request committed in phase 2 of tick t executes in phase 6 of tick t, against tick t's post-step state, and publishes at the boundary before tick t + 1; a request committed in phase 10 of tick t executes in phase 6 of tick t + 1, against that tick's post-step state, and publishes at the boundary before tick t + 2 |
| `Test_PhysicsDeferred_ExcessWaitsInOrder` | with `physics.deferredQueriesPerTick` = 4 and 10 requests committed in one tick, 4 execute that tick, 4 the next and 2 the one after, all in completion-ID order; none fails or is dropped |
| `Test_PhysicsDeferred_DetachCompletesInbox` | detaching a world with 6 INBOX entries and 2 parked results parks the 6 with the cancelled zero-hit result (`PHYSICS_QUERY_RESULT_FLAG_CANCELLED`) in completion-ID order through the sink before the world state is destroyed, so the table's in-flight count is 0; PHYS's counter `physics.deferred.cancelled_at_detach` rises by 6 and PHYS publishes no `world.async_discarded` |
| `Test_PhysicsDeferred_CancelStress1000Cycles` | 1,000 cycles of request then cancel at each point (before commit, in inbox, at completion, at detach, and a frame whose schedule instance is cancelled before `Physics.DeferredQueries` starts and, in another cycle, while it runs) leave no request, no uncompleted completion ID, no EXECUTED entry and no allocation behind: a cancelled instance that skipped the node leaves its entries INBOX, and one that cancelled after the node started still parks every entry the node executed |
| `Test_PhysicsDeferred_SavedRequestsReissued` | INBOX requests at a save are re-issued by `RestoreSection` under their recorded completion IDs and marked re-issued (`MarkReissued` returns true for each); the restore allocates no completion ID; a parked result at the save is not in PHYS's section and is published once, by SAVE's async section, at the boundary of t + 1 |
| `Test_PhysicsDeferred_DecoderHandlesCancelledAndEmpty` | the decoder maps a cancelled result and a zero-length payload to `cancelled = true`, `hit = false`, within 7 fields |

## Binding tests

| Test | Asserts |
|---|---|
| `Test_PhysicsBindings_MisuseRaisesCatalogueError` | each row of the misuse table of PHYS/ComponentsAPI.md "Script bindings" (nil entity, wrong type, a stale and a foreign entity passed to `GetVelocity` and as a cast's `ignore`, non-unit direction, negative or non-finite distance, radius, half height and blend time, non-finite vector and non-unit rotation, an undeclared layer bit, a `maxHits` of 0 and 17, an unknown layer name at load and at the call) raises a Luau error with that code (ZS1101 and ZS1102 for the stale and foreign entities), records no command and leaves every PHYS counter unchanged; `GetVelocity` of a live bodiless entity returns two zero vectors and raises nothing; a stale entity and a ragdoll call on an entity without `Zenith.Physics.Ragdoll`, through `BUFFERED` functions, raise nothing and are counted at playback (the enter with cause "no Zenith.Physics.Ragdoll component") |

## Material and interaction tests

| Test | Asserts |
|---|---|
| `Test_PhysicsMaterial_CombineTable` | the 16 mode pairs × representative coefficients give the formulas of PHYS/Backends.md "Material combination" |
| `Test_PhysicsMaterial_InteractionLookupOrder` | the rule order of `Zenith_FindPhysicsInteraction` (A names B, B names A, A's `*`, B's `*`) |
| `Test_PhysicsMaterial_NameIDFormula` | `Zenith_MakePhysicsMaterialNameID` of a fixed list of names (`Grass`, `Rock`, `Sand`, `Wood`, the empty name) equals the low 32 bits of `Zenith_Hash64` over the bytes (1 where that is 0; 0 for the empty name) |
| `Test_PhysicsInteraction_CooldownPerPair` | an interaction fires once per pair per cooldown window |
| `Test_PhysicsInteraction_ThresholdsUseApproachSpeed` | an impact below `impactSpeed` raises nothing; at the threshold it raises one `Interaction` event |

## Component and asset tests

`ZenithPhysics_Tests`; each names one acceptance artefact of the catalogue:

| Test | Asserts |
|---|---|
| `Test_PhysicsComponents_RegistrationMatchesTables` | after `Zenith_RegisterPhysicsComponents` and `Freeze`, `Zenith.Physics.RigidBody`, `Collider`, `Joint`, `CharacterController`, `Ragdoll`, `Buoyancy` and `Vehicle` exist with exactly the property IDs, names, types, defaults, ranges, units, authorities and serialized flags of the tables of PHYS/ComponentsAPI.md, property 35 carries `PROPERTY_FLAG_WORLD_POSITION`, no ID is reused, and the event and value types are registered with their versions |
| `Test_PhysicsJoint_ComponentCreatesConstraint` | a `Zenith.Physics.Joint` between two body entities creates its constraint at the next ingest with the component's kind, frames, limits and motor; a `SetProperty` of a limit re-creates it with a new priority; a joint naming a bodiless entity stays UNRESOLVED (`physics.joint.unresolved_delta`) until the body exists; with BodyB null frame B is `W_joint · T(FrameB)` |
| `Test_PhysicsRagdoll_BlendWritesRootFollowsPart` | `Physics.RagdollBlend` writes `Zenith.LocalTransform` 1–2 under `WRITER_SOURCE_RAGDOLL`: in ENTERING and ACTIVE the root part's joint position lowered by the root height offset and the yaw of its forward axis, in RECOVERING the captured root, and for a parented character `inverse(W_parent) · W_root` with the parent's previous-tick transform |
| `Test_PhysicsRagdoll_BodyPosesByJointHash` | `GetRagdollBodyPoses` returns one pose per part with the part's joint name hash, in definition order, the joint's world pose after the step, valid from phase 6; a non-ragdoll entity returns 0 |
| `Test_PhysicsDebug_ViewsRecordGeometry` | with a recording `Zenith_DebugDraw` in a development configuration, each of the eleven `Zenith_PhysicsDebugView` bits records geometry for its view alone (shape wireframes, bounds, contacts, triggers, constraints, controllers, ragdolls, queries, vehicles, layers, frozen), from neutral data; with no recorder (Shipping) `Physics.DebugDraw` is not registered and nothing is recorded |
| `Test_PhysicsCounters_RegisteredWithKinds` | every counter of PHYS.md "Counters" is published with its kind (gauge, cumulative, delta) after one tick, the delta counters reset each tick, and the profiler zones of PHYS.md "Profiler zones" open with their names |

## Conformance suite

`ZenithPhysics_Conformance` runs every case marked Conformance against both backends with the same assertions, through `Zenith_PhysicsConformanceHarness.h` ("Test support"). A case asserts only what the neutral contract promises; numeric tolerances are per scenario and identical for both backends. No case compares the two backends with each other ([Z2-N-ARCH-882]). The module-level cases of the same topics run per backend in `Zenith_Integration_Physics` (Target "Integration").

Every conformance case runs with 1 and 4 substeps per fixed 60 Hz tick unless it says otherwise, with the floating-point environment checked unchanged after each step (`Test_PhysicsJolt_StepKeepsFloatEnvironment`, `Test_PhysicsPhysX_StepKeepsFloatEnvironment` in the backend test targets cover the same from inside).

### General conformance cases

| Test | Asserts |
|---|---|
| `Test_PhysicsConformance_CapabilitiesMatchMatrix[<Backend>]` | the reported capabilities equal the matrix of `Zenith2/Docs/Design/PHYS.md` → "Capability matrix", and every gated member returns `PHYSICS_ERROR_UNSUPPORTED` when absent |
| `Test_PhysicsConformance_LayerMatrixRespected[<Backend>]` | for a 4-layer table, every pair of layers collides exactly when the table says so, for static, moving, trigger and query-only classes; and for each class, a ray, a sweep and an overlap see a body of that class exactly when the filter's gate for the class is set (`m_bIncludeStatic`, `m_bIncludeDynamic`, `m_bIncludeTriggers`, `m_bIncludeQueryOnly`) |
| `Test_PhysicsConformance_QueryOnlyVisibleToQueries[<Backend>]` | PHYS/Backends.md "PhysX scene query filter" |
| `Test_PhysicsConformance_QueryIgnoreGroup[<Backend>]` | PHYS/Backends.md "PhysX scene query filter" |
| `Test_PhysicsConformance_CollisionGroupIgnoresSameGroup[<Backend>]` | two dynamic boxes of group 5 dropped onto each other pass through when either has IGNORE_SAME_GROUP and collide when neither has it; group 0 never suppresses; boxes of groups 5 and 6 collide |
| `Test_PhysicsConformance_JointCollideConnected[<Backend>]` | PHYS/Backends.md "Constraint mapping" (the contact sink holds no record for the CollideConnected-false pair over 120 ticks, and an ADDED record in the first tick with true) |
| `Test_PhysicsConformance_ReusedIndicesCollide[<Backend>]` | two overlapping boxes joined by a hinge with CollideConnected false, with `SetExcludedPairs` holding their pair; the hinge and both boxes are destroyed and `SetExcludedPairs` is called with the pairs of the remaining constraints (none); two new overlapping dynamic boxes then receive the freed indices (asserted) and collide in their first step (ADDED records, separation); the same with the array left stale makes them pass through on Jolt, the negative control that shows the case can fail |
| `Test_PhysicsConformance_TriggerReachMatchesRule[<Backend>]` | every row of PHYS/Simulation.md "Trigger semantics", as trigger records in the sink: a trigger body reports a dynamic body awake and asleep, a kinematic platform and a kinematic capsule body (the controller's capsule shape); never a static box, the terrain heightfield, another overlapping trigger, a query-only body or an excluded-group body; one ADDED trigger record per overlap |
| `Test_PhysicsConformance_HeightfieldMatchesNeutral[<Backend>]` | PHYS/Backends.md "Heightfield builds" |
| `Test_PhysicsConformance_QueriesHitExpected[<Backend>]` | rays, sphere, capsule and box casts and overlaps against every shape kind return the analytically expected body, distance (1e-4 m) and normal (1e-3) |
| `Test_PhysicsConformance_TriggersReported[<Backend>]` | the `Triggers` scenario yields exactly the expected trigger ADDED and REMOVED records, and none for its static boxes and its overlapping trigger pair |
| `Test_PhysicsConformance_ContactsReported[<Backend>]` | contact ADDED and REMOVED records for the `Stack10` settle and a box knocked off it match the expected pairs, with the record semantics of each backend (PHYS/Simulation.md "Sleeping"): on Jolt, every REMOVED record of a pair one of whose bodies fell asleep in that step carries flag bit 1 and no other REMOVED record does, and the wake re-adds the pairs; on PhysX, no REMOVED record is appended when the stack falls asleep and none carries bit 1 |
| `Test_PhysicsConformance_ConstraintLimitsHold[<Backend>]` | every constraint of the `Joints` scenario stays within its limits (1° or 1 mm beyond slop) for 600 ticks under load |
| `Test_PhysicsConformance_MotorReachesTarget[<Backend>]` | hinge and slider motors reach their velocity and position targets within 2 % in 120 ticks |
| `Test_PhysicsConformance_StepWorkerIndependent[<Backend>]` | `Pile1000`, `Joints` and `Stack10` run together for 300 ticks at worker counts 1, 2, 8 and all; the body-and-constraint part of the canonical state hash and the sorted record list of every tick are identical across the four runs (the backend's own determinism, beneath the module's replay) |

## Scenario definitions

Every random choice comes from `Zenith_RandomSource` seeded by the case. The first seven scenarios have a body-level form in `Zenith_PhysicsScenarios.h` (for Conformance cases) and an entity form in `Zenith_PhysicsTestWorld.h` that spawns the same content, with the same seeds and placements, as entities carrying PHYS's components (for module-level cases); the last three are module-level only:

| Scenario | Forms | Content |
|---|---|---|
| `Stack10` | body, entity | 10 unit boxes stacked on a static ground box, 1 mm gaps |
| `Pyramid20` | body, entity | a 20-row box pyramid (210 boxes) |
| `Pile1000` | body, entity | 1,000 mixed spheres, boxes, capsules and convex hulls dropped into a static bowl mesh, seed-placed |
| `Bullet` | body, entity | a 2 cm sphere at 400 m/s towards a 1 cm thick static box, and the same towards a 5 cm terrain heightfield; a 5 cm sphere at 200 m/s towards a 0.1 m static box wall |
| `Joints` | body, entity | one chain of each constraint kind, with motors and break thresholds |
| `Triggers` | body, entity | 50 dynamic and 5 kinematic bodies passing through 10 triggers, 4 static boxes inside the triggers, and 2 triggers overlapping each other |
| `Heightfield257` | body, entity | the seeded 257² tile of PHYS/Backends.md "Heightfield builds" |
| `Ragdoll8` | entity | 8 humanoid ragdolls (the 15-part test definition, posed by `Zenith_PhysicsTestPoseSource`) dropped onto steps |
| `Characters100` | entity | 100 controllers walking random-seeded paths over a heightfield with steps, slopes and a moving platform |
| `FarOrigin` | entity | `Stack10` and `Characters100` placed at (4,000, 0, 4,000) and (12,000, 0, −12,000) without rebasing, and a travelling group (a `Stack10`, 10 controllers and the 40 m × 40 m static floor they walk on, all moved with the traveller as one streamed group) going from the origin to (16,000, 0, 16,000) at 300 m/s (STREAM's rapid-traversal speed) with origin shifts of STREAM's default quantum (2,048 m per rebased axis) at most every 30 ticks (the defaults of `stream.rebase.quantumMetres` and `stream.rebase.minIntervalTicks`), played by SCENE and applied through the module's origin-shift listener |

## Determinism replay

Integration, per physics backend and per ECS backend (the three of the suite). `Test_PhysicsDeterminism_ReplayAcrossWorkerCounts[<Backend>]`: the entity forms of `Pile1000`, `Ragdoll8`, `Characters100` and `Joints` run together for 900 ticks with a scripted stream of impulses, ragdoll enters and recovers, joint breaks and body creations and destructions, recorded by a test system of `Zenith_PhysicsTestWorld`. After each tick the canonical state hash (`Zenith_PhysicsStateHash`: every body's simulation ID, pose, velocities and sleep state as bit patterns, every constraint's state, every controller's state, every ragdoll part's state, and the tick's event sequence, in ascending simulation ID and part order) is recorded. The run is repeated at worker counts 1, 2, 8 and all, at chunk capacities 1, 7 and the default (`ecs.chunkCapacityRows`), and with a compaction forced every 50 ticks (`Zenith_WorldTestAccess::ForceCompaction`); for each ECS backend, every run's 900 hashes equal that backend's first run.

`Test_PhysicsDeterminism_ReplayWithScriptFailures[<Backend>]` (Integration) repeats it with a script node that fails on fixed ticks (the failure's records are discarded, [Z2-N-SCRIPT-334] shape) and asserts equality across worker counts. `Test_PhysicsConformance_LayoutAndWorkerIndependent[<Backend>]` (Integration, despite its name: it needs ECS layout) is the short form (120 ticks, worker counts 1 and all, chunk capacities 1 and the default) every PHYS diff runs. The backend-level counterpart is `Test_PhysicsConformance_StepWorkerIndependent[<Backend>]` ("General conformance cases").

## Stacking stability tests

Conformance:

| Test | Asserts |
|---|---|
| `Test_PhysicsStack_Stack10SettlesAndSleeps[<Backend>]` | after 300 ticks every box sleeps, the top box's drift from its settled position is < 5 mm, and nothing interpenetrates beyond the penetration slop |
| `Test_PhysicsStack_Pyramid20Holds[<Backend>]` | after 600 ticks no box has moved more than 2 cm horizontally |
| `Test_PhysicsStack_SpawnWithClearanceDoesNotPop[<Backend>]` | bodies placed with the clearance rule (PHYS.md "Authoring and cooking") gain no upward velocity > 0.1 m/s on their first tick (the old engine's "authored bodies spawn with clearance" lesson) |
| `Test_PhysicsStack_RestingOnHeightfield[<Backend>]` | a capsule and a box resting on a heightfield for 3,600 ticks sink < 1 mm |

## Tunnelling tests

| Test | Target | Asserts |
|---|---|---|
| `Test_PhysicsTunnel_ContinuousBulletStops[<Backend>]` | Conformance | the `Bullet` 2 cm sphere with CONTINUOUS quality never passes the 1 cm box or the heightfield, at 1 substep |
| `Test_PhysicsTunnel_ContinuousEnvelopeHolds[<Backend>]` | Conformance | the `Bullet` 5 cm sphere at 200 m/s with CONTINUOUS quality never passes the 0.1 m wall, at 1 substep (the envelope of PHYS/Simulation.md "Continuous collision detection") |
| `Test_PhysicsTunnel_DiscreteThickWallHolds[<Backend>]` | Conformance | a DISCRETE 0.5 m box at 40 m/s never passes a 0.3 m wall (speculative contacts) |
| `Test_PhysicsTunnel_ControllerNeverPassesWall[<Backend>]` | Integration | a controller pushed at its maximum speed into a 5 cm wall for 600 ticks stays on its side |
| `Test_PhysicsTunnel_MaxLinearSpeedClamped[<Backend>]` | Conformance | a velocity set above the world's `m_fMaxLinearSpeed` is clamped to it by the next step |

## Substep and hitch tests

| Test | Target | Asserts |
|---|---|---|
| `Test_PhysicsSubstep_ExactlyNStepsPerTick` | Unit | (fake backend) `StepOnce` is called exactly n times per tick for n = 1 … 8 |
| `Test_PhysicsStep_CancelAfterSubstepDetachesClean` | Unit | (fake backend, 4 substeps) the frame's schedule instance is cancelled after substep 2: `Physics.Step` calls `StepOnce` exactly twice, never with a step pending, performs no write-back and emits no event; the world is not stepped again; `DetachWorld` then completes the INBOX queries and destroys the backend world with no `StepOnce` or `ResumeStep` outstanding and nothing left allocated |
| `Test_PhysicsSubstep_CapAtEight` | Unit | no configuration, tick length or catch-up path calls `StepOnce` more than 8 times per tick, and the steps per frame never exceed the catch-up row's ticks × 8 |
| `Test_PhysicsSubstep_HitchDropsTimeNotSinks[<Backend>]` | Integration | a capsule resting on terrain through a simulated 0.49 s load hitch (the frame clock jumps; the catch-up row drops the excess ticks) sinks < 1 cm; the old engine's unbounded loop sank it 0.61 m |
| `Test_PhysicsSubstep_KinematicTargetInterpolated[<Backend>]` | Integration | a kinematic platform entity reaches its tick target exactly at the last substep and moves linearly between |

## Large-coordinate tests

| Test | Target | Asserts |
|---|---|---|
| `Test_PhysicsLarge_PrecisionInsideRadius[<Backend>]` | Conformance | `Stack10` at (4,000, 0, 4,000), inside the 4,096 m precision radius, without rebasing settles within the same tolerances as at the origin |
| `Test_PhysicsLarge_StabilityBeyondRadius[<Backend>]` | Conformance | `Stack10` at (12,000, 0, −12,000) without rebasing settles with drift < 5 mm and no interpenetration beyond the slop; contact points within 2 mm of the analytic ones (PHYS/Simulation.md "Large coordinates") |
| `Test_PhysicsConformance_OriginShiftPreservesRestingStack[<Backend>]` | Conformance | PHYS/Backends.md "Origin shift on Jolt" |
| `Test_PhysicsLarge_RebaseKeepsContacts[<Backend>]` | Integration | the travelling `FarOrigin` run: the stack carried with the traveller stays asleep through every shift, the travelling controllers keep walking on their carried floor with no snag, no event fires because of a shift, `physics.origin.late_shift_delta` stays 0, and every body of the travelling group stays within 4,096 m of the origin on each rebased axis (the stationary groups at (4,000, 0, 4,000) and (12,000, 0, −12,000) and the origin's ground are left behind by design and are not asserted on) |
| `Test_PhysicsLarge_QueriesAfterRebase[<Backend>]` | Integration | a ray cast before and after a rebase played by SCENE returns the same entity and a distance equal within 1e-4 m |
| `Test_PhysicsLarge_QueriesConsistentInRebaseTick[<Backend>]` | Integration | in the tick whose sync point 1 plays a shift: a phase-2 ray cast, a phase-3 job's ray cast (ANIM's foot-placement pattern), `Physics.CharacterMove` (phase 4) and a `Zenith_CollisionQuery::Sweep` hit the same entities as in the tick before, at positions equal to the previous ones minus S within one binary32 rounding of the shifted value; a controller standing on a static floor neither falls, snaps nor depenetrates (`physics.controller.depenetrations_delta` and its Grounded flag unchanged) |
| `Test_PhysicsLarge_ShiftedStateMatchesSceneFormula` | Unit | (fake backend) after a shift every root body's and controller's written local position equals `(float)((double)p − S)` bit for bit, as SCENE writes it, so the next ingest counts no external write; a shift with a component of 2^25 + 2^24 + 1 m (above 2^24 m) is applied once with the one-rounding conversion of PHYS/Simulation.md "Origin shift" step 1, counts `physics.origin.invalid_shift_delta` exactly once, and neither the next ingest's late path nor any subsequent tick applies it again (`physics.origin.late_shift_delta` stays 0) |

## Character controller tests

| Test | Target | Asserts |
|---|---|---|
| `Test_CharacterMove_WalksUpStep[<Backend>]` | Integration | a step ≤ StepHeight is climbed with no vertical velocity spike; a higher one blocks |
| `Test_CharacterMove_SlopeLimit[<Backend>]` | Integration | slopes above MaxSlope are not climbed and slide by the configured rule |
| `Test_CharacterMove_RootMotionThroughCollision[<Backend>]` | Integration | a ROOT_MOTION controller fed a published RootMotion delta into a wall stops at the wall and its TargetPosition records the clipped move |
| `Test_CharacterMove_OnlyControllerRowsConsumeRootMotion` | Unit | rows WITH a controller are moved by `Physics.CharacterMove`, rows WITHOUT are untouched by it (ANIM's node owns them, row PH12) |
| `Test_CharacterMove_RidesMovingPlatform[<Backend>]` | Integration | a controller standing on a rotating, translating kinematic platform stays at its platform-local position within 1 cm and yaws with it |
| `Test_CharacterMove_PushesDynamicBody[<Backend>]` | Integration | the push impulse on a dynamic box follows "Controller interaction with bodies" and the box's response is bounded |
| `Test_CharacterMove_TeleportResetsState` | Unit | a teleport clears the ground state and carried-platform state; the next move starts from the teleported pose |
| `Test_PhysicsController_RefusedOnBodyEntity` | Unit | a controller on an entity that also carries `Zenith.Physics.RigidBody`, and one on an entity with its own `Zenith.Physics.Collider`, get no capsule and count `physics.controller.invalid_delta`, while the entity's body is composed as usual; adding a `RigidBody` to a controller entity removes its capsule at the next ingest; no two live bodies of the world then share (user data, part code), checked over the slot table; the scene validation reports `PHYSICS_COOK_DIAG_CONTROLLER_WITH_BODY` for both entities |

## Ragdoll tests

| Test | Target | Asserts |
|---|---|---|
| `Test_PhysicsRagdoll_EnterFromPoseKeepsVelocity[<Backend>]` | Integration | entering from an animated pose creates parts at the pose's joint transforms with the pose's joint velocities, each part with the character's user data and its part code |
| `Test_PhysicsRagdoll_StateMachineTransitions` | Unit | every row of the ragdoll state machine, including refused transitions (each refusal cause, among them "no Zenith.Physics.Ragdoll component" and "rigid body") and their counters |
| `Test_PhysicsRagdoll_CapacityRefusesEnter` | Unit | an enter beyond `physics.maxActiveRagdolls` is refused and counted; the entity stays animated |
| `Test_PhysicsRagdoll_PoweredFollowsPose[<Backend>]` | Integration | a powered ragdoll with a static target pose holds each joint within 5° |
| `Test_PhysicsRagdoll_RecoverHoldsRoot` | Unit | through RECOVERING the root pose is the captured one and `LIFECYCLE_EVENT_RAGDOLL_RECOVER` is dispatched before ANIM's listener |
| `Test_PhysicsRagdoll_BlendScheduleClosedForms` | Unit | with blend times 0, 0.1 s and 0.4 s at 60 Hz, every weight `Physics.RagdollBlend` writes is bit-equal to the pinned binary32 expressions of PHYS/CharacterRagdollVehicle.md "Ragdoll state machine" (`fDelta`, `fEnter`, `fRecover`, evaluated by the test with the same expressions): ENTERING from the first run after the playback (k = 1); RECOVERING from ACTIVE holding 1 at k = 1, reaching NONE and recording `Physics.RemoveRagdollBodies` in the run where `fRecover` is 0; a recover during ENTERING at w = 0.5 holding 0.5 at k = 1 |
| `Test_PhysicsRagdoll_ParentChildNeverCollide[<Backend>]` | Integration | adjacent parts never report contacts; non-adjacent parts do (as self-contacts, counted `physics.contacts.self_delta`) |
| `Test_PhysicsRagdoll_SelfContactsEmitNoEvents[<Backend>]` | Integration | a ragdoll folded so that 6 pairs of non-adjacent parts touch, falling onto a floor with REPORT_CONTACTS: no event names the character twice (EntityA ≠ EntityB in every event), the floor pair's `ContactBegan` fires once when the first part lands and `ContactEnded` once when the parts are removed, and the self-contacts are counted |
| `Test_PhysicsJolt_ExcludedPairsFollowEveryDestructionPath` | Integration (Jolt only: PhysX keeps no array) | a ragdoll recovered to NONE (its parts removed at sync point 6) and, in a second run, an entity with a CollideConnected-false hinge destroyed at the end of a tick, a body frozen and thawed in a third: in the next tick two new overlapping dynamic boxes receive the freed body indices (asserted through their handles) and collide (an ADDED record and separation within 10 ticks) |

## Vehicle and buoyancy tests

Stage 4:

| Test | Target | Asserts |
|---|---|---|
| `Test_PhysicsVehicle_StraightLineAcceleration[<Backend>]` | Integration | a vehicle on flat ground reaches the speed the engine curve predicts within 5 % |
| `Test_PhysicsVehicle_SuspensionRestHeight[<Backend>]` | Integration | at rest every wheel's compression equals mass share / stiffness within 1 mm |
| `Test_PhysicsBuoyancy_FloatsAtDensityRatio[<Backend>]` | Integration | a box of half water density floats at half submersion within 2 % after 600 ticks |
| `Test_PhysicsBuoyancy_SamplesSumToVolume` | Unit | cooked and primitive samples sum to the shape's volume |

## Save and restore tests

| Test | Target | Asserts |
|---|---|---|
| `Test_PhysicsConformance_SaveRestoreExactContinuation[Jolt]` | Integration (Jolt) | PHYS/Backends.md "Jolt state save and restore" |
| `Test_PhysicsConformance_SaveRestoreCoherent[PhysX]` | Integration (PhysX) | after a COHERENT restore: same bodies, poses, velocities, sleep states, constraints, controllers and ragdoll parts; no new interpenetration beyond the slop over the next 60 ticks |
| `Test_PhysicsSave_RagdollMidFallRestores[<Backend>]` | Integration | a save taken while one ragdoll is ACTIVE in mid-fall (parts awake, velocities ≥ 2 m/s) and another is RECOVERING: after the restore every part exists with its recorded handle, `m_uRagdollPart` and constraint priority, its pose and velocities equal the saved bits (PhysX) or the uninterrupted run's state hash continues (Jolt), the states, weights and k are restored, the excluded pairs are rebuilt before the first step, and the falling ragdoll lands within the uninterrupted run's tick (Jolt) or within 5 ticks of it (PhysX) |
| `Test_PhysicsSave_SectionRoundTrip` | Unit | (fake backend) encode then decode the section of PHYS/Formats.md "Save section layout" reproduces every block field by field: the ragdolls block's per-part fields (a frozen part included), the controllers block (written local bits, parent version, last targets, carried yaw rate, capsule query-only), each pair's suspended count, and a non-empty ended list captured at phase 11 of a tick whose sync point 6 removed ragdoll parts, after whose restore the next emission ends the restored pairs exactly as the uninterrupted run does |
| `Test_PhysicsSave_FutureVersionRefused` | Unit | a section of version 2 is refused with `SAVE_ERROR_CORRUPT` |
| `Test_PhysicsSave_ExactBlobMismatchFallsBack` | Unit | a corrupted EXACT blob and a blob with another data tag are each `PHYSICS_ERROR_STATE_MISMATCH`; the restore completes coherently, counts `physics.save.coherent_fallbacks` and calls `ReportCoherentRestore` with the participant key exactly once |
| `Test_PhysicsSave_RestoreWaitsForShapes` | Unit | a restore whose collider asset is still loading: `IsRestoreReady` returns false and `ListRestoreWaits` lists the load until it is ACTIVE, creates no body before then, and then, on the main thread with no job context, creates every body with its recorded handle in ascending simulation ID and returns true; a FAILED asset leaves that entity bodiless, counted, and reports a coherent restore |
| `Test_PhysicsSave_FlagsAndFidelity` | Unit | the participant's `GetFlags` is 0 on Jolt and `uSAVE_PARTICIPANT_FLAG_COHERENT` on PhysX, never `uSAVE_PARTICIPANT_FLAG_EXCLUDE_FROM_RESUME`; SAVE's default `GetCaptureFidelity` then answers EXACT and COHERENT; `GetReissuedSourceKinds` is exactly the bit of `ASYNC_SOURCE_PHYSICS_QUERY` |

The section is at version 1, so no migration test exists yet; the diff that introduces version 2 adds `Test_PhysicsSave_MigratesV1Section` with its generated input (PHYS/Formats.md "Save section layout").

## Failure injection tests

Each construction sequence has one fault point per step and one test per step, named `Test_<Type>_Create_FailsAt<Step>_Unwinds`, asserting the returned error, that every earlier step was undone ([Z2-N-CONTRACT-341]), and that the root heap's live allocations return to the pre-call count:

| Sequence | Tests |
|---|---|
| Jolt service (PHYS/Backends.md "Jolt construction steps") | `Test_PhysicsJolt_Create_FailsAtValidate_Unwinds`, `…FailsAtExclusive_Unwinds`, `…FailsAtObject_Unwinds`, `…FailsAtFactory_Unwinds`, `…FailsAtTables_Unwinds`, `…FailsAtDefaults_Unwinds` |
| PhysX service | `Test_PhysicsPhysX_Create_FailsAtValidate_Unwinds`, `…FailsAtExclusive_Unwinds`, `…FailsAtObject_Unwinds`, `…FailsAtFoundation_Unwinds`, `…FailsAtPhysics_Unwinds`, `…FailsAtExtensions_Unwinds`, `…FailsAtTables_Unwinds`, `…FailsAtDefaults_Unwinds` |
| world creation (both; Conformance) | `Test_PhysicsWorld_Create_FailsAt<Step>_Unwinds[<Backend>]` for each step of "Jolt world construction" / "PhysX world construction" |
| module start and world attach (Unit) | `Test_PhysicsModule_Create_FailsAt<Step>_Unwinds` for each row of PHYS.md "Module start and world attach" |

`Test_PhysicsJolt_SecondInstanceRefused` and `Test_PhysicsPhysX_SecondInstanceRefused` assert `PHYSICS_ERROR_BACKEND_ALREADY_ACTIVE` while one instance lives, and success after it is destroyed.

## Backend unit tests

| Target | Tests |
|---|---|
| `ZenithPhysics_Jolt_Tests` | `Test_PhysicsJolt_LayerEncodingRoundTrips`, `Test_PhysicsJolt_PairFilterMatchesLayerTable`, `Test_PhysicsJolt_GroupFilterRules` (IGNORE_SAME_GROUP, group 0, excluded pairs found by binary search, and every other pair admitted), `Test_PhysicsJolt_BodiesCreatedWithNeutralIDs` (every body on every path, restore included, has `BodyID(index, generation & 0xFF)` of its neutral handle), `Test_PhysicsJolt_ContactValidateReturnValues` (`AcceptContact` and `RejectContact` for a body–heightfield pair whose tile has holes, so a subsequent hole manifold of the same pair is still rejected; `AcceptAllContactsForThisBodyPair` for every other pair), `Test_PhysicsJolt_HeightfieldErrorMeasured` (the loader's `m_fBackendHeightError` equals the maximum per-sample `GetPosition` deviation for a flat tile, a 100 m-range tile and a tile with a 12 m cliff, and the gauge reports the largest live value), `Test_PhysicsJolt_StateRecorderArenaRoundTrip`, `Test_PhysicsJolt_UpdateErrorsMapToStats`, `Test_PhysicsJolt_StepKeepsFloatEnvironment`, `Test_PhysicsJolt_DataTagChangesWithOptions`; only if PHYS-1 is admitted, `Test_PhysicsJolt_JobSystemQueuesReadyJobs` and `Test_PhysicsJolt_ParallelStepStressAcrossWorkerCounts` (PHYS/Backends.md "Conditional Jolt parallel step") |
| `ZenithPhysics_PhysX_Tests` | `Test_PhysicsPhysX_FilterShaderTruthTable`, `Test_PhysicsPhysX_DispatcherInlinesWhenFull`, `Test_PhysicsPhysX_ResumeStepUntilComplete` (including a schedule instance cancelled while a wave is pending: the resume continuation still runs to the `fetchResults` that completes the substep, so `simulate` is never left without it, and the cancellation check then stops the tick), `Test_PhysicsPhysX_DispatcherStressAcrossWorkerCounts` (PHYS/Backends.md "PhysX step and the dispatcher"), `Test_PhysicsPhysX_ScratchBlockAlignment` (the scratch block is 16-byte aligned and a multiple of 16 KiB for every `physics.stepScratchBytes` in the key's range), `Test_PhysicsPhysX_ErrorCallbackCounts`, `Test_PhysicsPhysX_StepKeepsFloatEnvironment`, `Test_PhysicsPhysX_AllocatorCountsLiveBytes` |

## Cook and format tests

`ZenithCook_Tests`, files under `Zenith2/Tools/ZenithCook/Tests/Physics/`:

| Test | Asserts |
|---|---|
| `Test_PhysicsCook_ConvexHullDeterministic` | the same points in any order cook to identical bytes |
| `Test_PhysicsCook_DegenerateHullRefused` | coplanar points are a cook error with a message naming the document |
| `Test_PhysicsCook_HeightfieldMatchesEnvHeights` | every triangle of the cooked heightfield passes through ENV's interpolated heights at its vertices and centroid within 1 mm (EN4) |
| `Test_PhysicsCook_FoliageCompoundFromIntermediate` | a `ZPCI` intermediate cooks into a compound with one child per record in record order |
| `Test_PhysicsCook_BackendSectionMatchesNeutral[<Backend>]` | a shape loaded from its backend section and the same shape built from its neutral section answer 1,000 seeded ray casts identically |
| `Test_PhysicsCook_StaleBackendSectionIgnored` | a section with another data tag or neutral hash is ignored and the neutral path counted |
| `Test_PhysicsCook_RagdollJointMissingRefused` | a ragdoll document naming a joint the skeleton lacks is refused |
| `Test_PhysicsMaterial_CookValidatesRanges` | `physics.material` cooks a valid document into the 24-byte material record and its interaction records; friction above 10, restitution outside 0 … 1, a non-positive density, an unknown combine name, a surface type outside its pattern, 65 interaction rules and a 64-byte interaction name are each refused with `PHYSICS_COOK_DIAG_MATERIAL_INVALID` naming the member; unknown members are preserved and warned |
| `Test_PhysicsCook_MaterialNamesResolveByTable` | PHYS/Formats.md "Physical-material names" |
| `Test_PhysicsDocument_UnknownMembersPreserved` | the three document types re-emit unknown members ([Z2-N-CONTRACT-543]) |
| `Test_PhysicsCook_HeightfieldPaletteAndSquareRefused` | a `ZPHI` intermediate with 128 palette names, and a heightfield cook input with samplesX ≠ samplesZ, are cook errors naming the input |

## Loader tests

`ZenithPhysics_Tests`, over the runtime loaders of `ZenithPhysics/Internal/Assets/`, which only their own library's tests reach, mounted in a real `Zenith_AssetRegistry` through ASSET's header-only `Zenith2/Engine/ZenithAssets/Tests/Support/Zenith_AssetKindLoaderHarness.h` (no double of the registry; ruling clarification (d) 2 of the ticket brief) over seed files built in code by `Zenith2/Engine/ZenithPhysics/Tests/Support/Zenith_PhysicsSeedFiles.h` (header-only):

| Test | Asserts |
|---|---|
| `Test_PhysicsLoader_CollisionShapeActivatesAndRetires` | a `.zcol` of each cooked kind (hull, mesh, heightfield, compound) loads on a loader job into a `Zenith_CollisionShapeAsset` whose shape, kind, bounds, volume, palette and buoyancy samples equal the seed's; its palette materials are resident first; retiring the asset releases its shape reference while a body built on it keeps the shape alive |
| `Test_PhysicsMaterial_LoaderReadsSections` | a `.zpmat` loads into `Zenith_PhysicsMaterialAsset` with every field of sections 0x0201 and 0x0202 and its strings interned; a missing material asset is substituted with the default material (the kind's SUBSTITUTE fallback) |
| `Test_PhysicsFormat_ZcolParserLimits` | each limit of PHYS/Formats.md "Parser limits" accepted at the limit and refused at limit + 1 by the loaders |
| `Test_PhysicsFormat_TruncatedSectionsCorrupt` | every truncation point of a valid `.zcol`, `.zpmat` and `.zragdoll` makes the loader fail with `PHYSICS_ERROR_COOKED_CORRUPT` in its log line, never an assert, with nothing left allocated |

The `.zcol`, `.zpmat` and `.zragdoll` readers are also fuzz targets (`Zenith_Fuzz_PhysicsCooked`) under the programme's fuzzing rules.

## Integration suite

`Zenith2/Tests/Physics/`, on the composed runtime with all three ECS backends. Besides every case this file marks Integration, the suite holds the cases below. A name ending `[<Backend>]` is registered as `<name>[Jolt]` and `<name>[PhysX]`; a name ending `[Jolt]` or `[PhysX]`, or a row restricted to one backend, is registered for that backend alone; a name without the suffix runs once, on the Jolt backend:

| Test | Asserts |
|---|---|
| `Test_PhysicsIntegration_SceneBodiesFromAuthoredScene` | a scene with every component kind loads; every body exists with its authored pose; unloading destroys them all; the scene cook's validation reports `PHYSICS_COOK_DIAG_DYNAMIC_UNDER_MOVING_PARENT` for the scene's one dynamic body under an animated parent and `PHYSICS_COOK_DIAG_DYNAMIC_TRIGGER` for its one DYNAMIC rigid body whose own collider is a trigger (which stays at its authored pose while the scene's other dynamic bodies fall), and for no other body |
| `Test_PhysicsIntegration_ScriptQueriesAndImpulses` | a Luau behaviour's `Physics.RayCast`, `AddImpulse` and `RequestQuery` produce the documented results and completion order |
| `Test_PhysicsIntegration_StreamingCellsAndFreeze` | cells streaming in and out create and destroy their static colliders; region-bound bodies freeze and thaw (ST8) |
| `Test_PhysicsIntegration_SaveLoadMidSimulation[<Backend>]` | a save at tick 400 of `Pile1000` loaded into a new process continues per the backend's restore mode |
| `Test_PhysicsIntegration_ResourceAndBackendSelection` | a game built with `backends.physics` Jolt and one with PhysX both start, attach and step |
| `Test_PhysicsIntegration_SwapCompletesInboxQueries` | X265's case: a world swap with 8 deferred queries in INBOX and 3 parked: the retired table's `GetInFlightCount()` reaches 0 within the same prologue, and `world.async_discarded` rises by 11, published by the world host alone |
| `Test_PhysicsIntegration_RebaseParametersMatchStream` | the registered defaults of `stream.rebase.quantumMetres` and `stream.rebase.minIntervalTicks` equal the values the `FarOrigin` scenario uses, so the test parameters cannot drift from STREAM's keys |
| `Test_PhysicsIntegration_OriginShiftAtSyncPoint` | with `Streaming` selected and rebasing enabled, the physics world is shifted in the same sync point as SCENE's transforms (the backend positions read by a phase-2 query of the rebase tick are already shifted) and `physics.origin.late_shift_delta` stays 0 |

## Tests PHYS contributes to

| Test | Owner | PHYS's assertions |
|---|---|---|
| `Test_Lifecycle_Ragdoll_EnterAndRecoverBlend` | GAME (`Zenith2/Tests/LifecycleCharacters/`, `Zenith2/Docs/Design/GAME/PhaseContract.md`, row GA3) | the enter and recover procedures and the two closed forms, as the pinned binary32 expressions of PHYS/CharacterRagdollVehicle.md "Ragdoll state machine" (recover delayed by one tick relative to enter) |
| `Test_Scenario_TeleportDuringRagdoll` | GAME (`Zenith2/Docs/Architecture/LifecycleCommands.md` → "Scenario tests") | every ragdoll body moved rigidly by the offset, velocities zero, blend weight unchanged |
| `Test_SaveCrossSystem_PhysicsAndAnimationCoherent` | SAVE | the physics participant's EXACT and COHERENT paths |

## Benchmarks and budget evidence

`Zenith_Bench_BudgetsPHYS` measures the PHYS rows under the protocols named in `Zenith2/Docs/Budgets.md`; its `--smoke` CTest runs each case for 10 ticks in the `Test` step, and the value-producing runs on the reference desktop and the S24 belong to the `needs-human` budget ticket that attaches them (the WORKER-EVIDENCE ruling):

| Row | Measurement |
|---|---|
| Z2-B-PHYS-001 | `GetResidentBytes` of the physics consumer at the peak of each workload (MP-SUBMEM) on Windows and the S24 |
| Z2-B-PHYS-002 | the summed span of `Physics.Step`, `Physics.SyncTransforms`, `Physics.RagdollBlend` and `Physics.DeferredQueries` per tick (MP-CPUZONE applied to each node, the definition core-doc item X264 (e) asks Budgets to state), p95 over the workload's capture, per backend; for Jolt also the benchmark-only thread-pool figures of PHYS/Backends.md "PhysX on Android evaluation" (evidence for the PHYS-1 trigger, never a row value) |
| Z2-B-PHYS-002, cell churn (W-OpenWorld) | the p95 and the maximum of `Physics.Step` in a tick that destroys 4,096 bodies of an unloaded cell and, in the next tick, creates 4,096 bodies of a loaded one at the freed indices (the O(F²) free-list walk of Jolt's `AddBodyWithCustomID`, PHYS/Simulation.md "Slot allocation"), per backend, before Stage 4 |
| the pending capacity rows | the high-water marks of bodies, constraints, contact records and active ragdolls per workload ("Budget rows" of PHYS.md) |

## Gate steps for PHYS diffs

A diff under `Zenith2/Engine/ZenithPhysics*/**`, `Zenith2/Tools/ZenithCook/**/Physics/**` or `Zenith2/Tests/Physics/**` runs `Build` (all three presets), `Test` (the unit, conformance, backend and integration targets and the benchmarks' smoke runs), `SourceLayout`, `Libgraph`, `Complexity`, `ForbiddenCheck`, `Format`, `HeaderSelfContained`, `ApiContractCheck`, `TestSeparationCheck` and `Sanitizers -Preset win-asan` (`linux-tsan` does not build the Physics module, whose `ZENITH_SUPPORTED` is `ZENITH_GPU`), per `Zenith2/Docs/Contracts.md` → "Enforceability matrix". PHYS asks PROC to add `Zenith2/Engine/ZenithPhysics*/**` to the `Zenith2Memory` paths, because both backends route third-party allocation hooks and parse untrusted cooked data (cross-area row PH16).

**The race-detection gap, stated.** No preset runs a race detector over PHYS's concurrent code: the PhysX dispatcher queue, the primitive cache and the PhysX contact sink run only where `linux-tsan` does not build the module, and AddressSanitizer detects no data race. A determinism hash catches only races that change a result. The mitigations are targeted stress tests under the watchdog, which fail on a hang, a lost task, a use of a released shape or a differing hash: `Test_PhysicsPhysX_DispatcherStressAcrossWorkerCounts`; `Test_PhysicsQuery_PrimitiveCacheConcurrentStress` (16 query leaves running 10,000 shape casts over 6,000 distinct primitive keys, so the cache is full and evicts throughout; each query checks, after its backend query returns and before it unpins, that `GetShapeInfo` of the pinned shape succeeds with the key's kind and dimensions; at the end the cache holds at most 4,096 entries, every pin count is 0, and every shape created and not cached was released: the service's live-shape count equals the cached entries plus the bodies' shapes; run under `win-asan`, where a release of a pinned shape would be a use-after-free); and the determinism replay at worker counts 1, 2, 8 and all. If a core change lets `linux-tsan` build `ZENITH_GPU` libraries headless, the module gains that preset in the same diff.
