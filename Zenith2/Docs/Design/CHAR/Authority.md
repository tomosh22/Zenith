# CHAR design: animation and physics authority for characters

Section file of `Zenith2/Docs/Design/CHAR.md`. It states, for a character in each of its states, which system writes each field that animation and physics both touch, as a consequence of rules other documents already settle; it then specifies the authoring lint that keeps character prefabs and scenes inside those rules, and the integration suite that proves the whole chain on one character the way the character workflow builds it. CHAR introduces no writer source, no arbitration rule and no system: the phase contract is Z2-D-080 (`Zenith2/Docs/Architecture/FramePhases.md → "The twelve phases"`, `Zenith2/Docs/Architecture/FramePhases.md → "Arbitration rules"`), root motion is ANIM's to extract and PHYS's controller's to apply (the orchestrator's Q72 ruling, `Zenith2/Docs/Design/ANIM/Runtime.md → "Animation.ApplyRootMotion"`, `Zenith2/Docs/Design/PHYS/CharacterRagdollVehicle.md → "Character move algorithm"`), ragdoll ownership is PHYS's schedule and ANIM's blend (`Zenith2/Docs/Design/PHYS/CharacterRagdollVehicle.md → "Ragdoll state machine"`, `Zenith2/Docs/Design/ANIM/Runtime.md → "Ragdoll transitions"`, the PHYS-RECOVER ruling), and cinematics take precedence (`Zenith2/Docs/Architecture/LifecycleCommands.md → "Cinematic start, interrupt and skip"`).

## Scope of this file

| Topic | Heading |
|---|---|
| who writes what, per character state | "Character authority matrix" |
| the fields involved | "Fields in the matrix" |
| transitions between states | "State transitions and their tick" |
| the authoring checks | "Authority lint inputs", "Authority lint rules" |
| the end-to-end proof | "Character integration suite", "Integration fixtures" |

## Fields in the matrix

| Field | Owner of the type | Authority (owning phase) |
|---|---|---|
| the root entity's `Zenith.LocalTransform` 1 Position, 2 Rotation | SCENE (`Zenith2/Docs/Design/SCENE/Transforms.md → "Transform components"`) | `PostPhysics`, precedence-arbitrated (cinematic > ragdoll > physics > animation > script) |
| `Zenith.Animation.RootMotion` 1–3 | ANIM | `Animation` (phase 3), written by `Animation.Evaluate` |
| `Zenith.Animation.Pose` 1 PhysicsWeight | ANIM | `PostPhysics`, precedence over (Cinematic, Ragdoll, Animation) |
| the bone pose (`Zenith.Animation.FinalPoses`) | ANIM | `PostPhysics`, written by `Animation.PostPhysicsBlend` |
| `Zenith.Physics.CharacterController` 35 TargetPosition, 36 TargetRotation, 30 Velocity | PHYS (`Zenith2/Docs/Design/PHYS/ComponentsAPI.md → "Zenith_CharacterController header"`) | phase 4, written by `Physics.CharacterMove` |
| `Zenith.Physics.Ragdoll` 1 State, 2 BlendWeight | PHYS | `PostPhysics`, written by `Physics.RagdollBlend` and PHYS's playbacks |

## Character authority matrix

<!-- assumption: pending PHYS -->
The PHYS column entries cite PHYS's revision 2 text (`Zenith2/Docs/Design/PHYS/Simulation.md → "Frame-graph placement"`: `Physics.SyncTransforms` writes `Zenith.LocalTransform` 1–2 in phase 6 with `WRITER_SOURCE_PHYSICS`, `Physics.RagdollBlend` with `WRITER_SOURCE_RAGDOLL`); the matrix restates no rule and changes when PHYS's signed-off text does, in the diff of the PHYS ticket that changes it.
<!-- /assumption -->

| Character state | Root transform written by (phase, writer source) | Root-motion delta | Bone pose owner (PhysicsWeight) | Controller |
|---|---|---|---|---|
| L1 locomotion with a controller (Mode `ROOT_MOTION_WITH_GRAVITY` or `ROOT_MOTION`) | `Physics.SyncTransforms` (6, Physics) from the capsule PHYS moved to the controller's targets | extracted by `Animation.Evaluate` (3), applied by `Physics.CharacterMove` (4) through collision | Animation (0) | moves |
| L2 locomotion with a controller in `VELOCITY` mode | `Physics.SyncTransforms` (6, Physics) | extracted, not applied (`Animator.RootMotion` should be `IGNORE`, rule AU2) | Animation (0) | moves by script intent |
| L3 locomotion without a controller | `Animation.ApplyRootMotion` (4, through `SetProperty` commands applied at sync point 4; arbitration in phase 6 keeps them unless a phase-6 writer writes the entity) | extracted (3), applied by ANIM (4) | Animation (0) | none |
| R1 ragdoll ENTERING | `Physics.RagdollBlend` (6, Ragdoll): the root follows the root part | extracted; not applied (controller SUSPENDED; without a controller ANIM's command is out-ranked by the Ragdoll writer) | Ragdoll (w rising by PHYS's closed form) | suspended, capsule query-only |
| R2 ragdoll ACTIVE | `Physics.RagdollBlend` (6, Ragdoll) | as R1 | Ragdoll (1) | suspended |
| R3 ragdoll RECOVERING | `Physics.RagdollBlend` (6, Ragdoll): the root held at the recover capture | extracted from the get-up clip; not applied until NONE | Ragdoll (w falling; the first phase 6 after the recover playback keeps w₀, PHYS-RECOVER) | suspended; adopts the root pose on its next move after NONE (`ADOPTED_EXTERNAL`) |
| C1 held by a cinematic (any of the above underneath) | CINE's transform track writer (6, Cinematic), when the sequence has one | as underneath; a cinematic body slot may carry root motion through the slot | Cinematic (CINE's writer writes 0: the cinematic's animation is the pose) | adopts the cinematic pose (cross-area row CN11 with PHYS) |
| T1 the tick of a teleport playback | the teleport playback (sync point), then the state's writer | reset (`RootMotion` Flags bit 1): the identity, so nothing moves twice | unchanged (the ragdoll blend state is kept) | targets set to the teleported pose; not an adoption |

<!-- elaboration -->
Two writers never write the root of one entity in one phase with the same writer source, and every state's winner follows from the default precedence; CHAR's suite (below) proves the matrix holds for the character shape the workflow generates.

## State transitions and their tick

| Transition | Command (owner) | First tick the new row applies |
|---|---|---|
| L* → R1 | `Physics.EnterRagdoll` (PHYS) recorded in phases 1–4 | phase 5 of the same tick ([Z2-N-ARCH-552]); recorded in 5–10: the next tick |
| R2 → R3 | `Physics.RecoverRagdoll` (PHYS), with a get-up clip played in ANIM's `Zenith.GetUp` slot | the playback's sync point; ANIM's first recover tick sees w = 1 |
| R3 → L* | none: PHYS's weight reaches 0 | the second phase 6 after a 0 s recover; `1 + ceil(T × tickHz)` phases 6 for recover time T |
| any → C1 | `Cinematics.Start` (CINE) | the tick after the start playback (tracks start writing in the next tick's phases, `Zenith2/Docs/Architecture/LifecycleCommands.md → "Cinematic start, interrupt and skip"`) |
| C1 → previous | CINE's end, interrupt or skip | the restore at the next tick boundary ([Z2-N-ARCH-554]) |
| any → T1 | `Zenith_RecordTeleport` or the Luau `Scene.Teleport` (SCENE's lifecycle command, `Zenith2/Docs/Architecture/LifecycleCommands.md → "Teleport"`) | its playback |

## Authority lint inputs

`Zenith_CharLintCharacterAuthority(Zenith_Span<const Zenith_CharEntityRecord> axEntities, Zenith_CharAssetQuery& xQuery, Zenith_Array<Zenith_CharDiagnostic>& axOut)` (declared in `Zenith2/Docs/Design/CHAR.md → "Validation API"`) checks a resolved document: the caller resolves a scene or prefab with SCENE's resolver (`Zenith2/Docs/Design/SCENE/Prefabs.md → "Resolution algorithm"`) and passes one record per entity, in ascending entity ID: the entity's ID, its parent's index (or `uCHAR_NO_PARENT`), and its components as (stable name, `Zenith_JsonNode` of the component object's `fields`) pairs sorted by stable name. Property values are read from the JSON field-set form by their property IDs (`Zenith2/Docs/Design/ECS/Reflect.md → "JSON field-set form"`), with each type's registered default for an absent ID; the IDs used are ANIM's (`Zenith2/Docs/Design/ANIM.md → "Component registration"`), PHYS's (`Zenith2/Docs/Design/PHYS/ComponentsAPI.md → "Zenith_CharacterController header"`, `"Zenith_Ragdoll header"`, `"Zenith_PhysicsComponents header"`) and RPROD's (`Zenith2/Docs/Design/RPROD/Extractors.md → "Flux_RenderComponents header"`). The lint is O(entities × components).

## Authority lint rules

| Rule | Check | Severity | Diagnostic |
|---|---|---|---|
| AU1 | an entity with `Zenith.Animation.Animator` also has `Zenith.Animation.RootMotion` and `Zenith.Animation.Pose` | error | `CHAR_DIAG_AUTHORITY_ANIMATOR_INCOMPLETE` |
| AU2 | Animator 3 RootMotion is `EXTRACT` while the entity's controller 11 Mode is `VELOCITY` | warning | `CHAR_DIAG_AUTHORITY_ROOT_MOTION_UNUSED` (state L2: extraction work no reader applies) |
| AU3 | controller 11 Mode is `ROOT_MOTION_WITH_GRAVITY` or `ROOT_MOTION` and the entity has no Animator with RootMotion `EXTRACT` | error | `CHAR_DIAG_AUTHORITY_CONTROLLER_STARVED` (the controller never moves horizontally) |
| AU4 | an entity with `Zenith.Physics.Ragdoll` has an Animator, and the ragdoll definition's skeleton (`xQuery.GetRagdollSkeleton`) equals the animator graph's skeleton (`xQuery.GetGraphSkeleton`) | error | `CHAR_DIAG_AUTHORITY_RAGDOLL_MISMATCH` |
| AU5 | an entity with an Animator, or any ancestor or descendant of one up to the character root, has `Zenith.Physics.RigidBody` with property 1 MotionType = 3 (`PHYSICS_MOTION_TYPE_DYNAMIC`, the default when the ID is absent, `Zenith2/Docs/Design/PHYS/ComponentsAPI.md → "Zenith_PhysicsComponents header"`) | error | `CHAR_DIAG_AUTHORITY_DYNAMIC_BODY_ON_CHARACTER` (the Physics writer would fight root motion every tick; PHYS's scene validation already warns about the descendant case with `PHYSICS_COOK_DIAG_DYNAMIC_UNDER_MOVING_PARENT`, `Zenith2/Docs/Design/PHYS.md → "Spawn clearance rule"`) |
| AU6 | an entity with RootMotion `EXTRACT`, no `Zenith.Physics.CharacterController`, no `Zenith.Physics.RigidBody`, and `Zenith.Physics.Collider` | warning | `CHAR_DIAG_AUTHORITY_MOVED_STATIC_COLLIDER` (root motion moves an implicit static body; a kinematic body or a controller is meant) |
| AU7 | `Zenith.Animation.PoseFollower` 1 Leader names an entity with an Animator; the follower has no Animator itself; the leader is not a follower; the follower's mesh skeleton equals the leader graph's skeleton | error | `CHAR_DIAG_AUTHORITY_FOLLOWER_INVALID` (ANIM refuses it at extraction, `Zenith2/Docs/Design/ANIM/Runtime.md → "Pose followers"`) |
| AU8 | Animator 6 Flags has `ANIMATOR_FLAG_ALLOW_REDUCED_ROOT_MOTION` and the controller Mode is `ROOT_MOTION` | warning | `CHAR_DIAG_AUTHORITY_REDUCED_RATE_THROUGH_COLLISION` (spread steps lag collision by up to k − 1 ticks, `Zenith2/Docs/Design/ANIM/Runtime.md → "Root motion under reduced rates"`) |
| AU9 | the entity's class (from the generating assembly, or the `.meta` class label of its prefab) has `forceFullRate` and Animator Flags lacks `ANIMATOR_FLAG_FORCE_FULL_RATE` | warning | `CHAR_DIAG_AUTHORITY_CLASS_RATE` (Stage 4, "Character class presets" of `Zenith2/Docs/Design/CHAR/Debugging.md`) |

Rules AU1 to AU8 land at Stage 3 with the lint; AU9 at Stage 4. Every rule has one unit case in `Zenith2/Tools/Authoring/Tests/CHAR/Zenith_CharAuthority_Tests.cpp` (`Test_CharAuthority_AU1AnimatorIncomplete` … `Test_CharAuthority_AU9ClassRate`), each with a fixture breaking only that rule, and `Test_CharAuthority_CleanCharacterPasses` asserts no diagnostic on the suite's fixture character. From Stage 4 the editor's validation panel runs the lint on every open scene and prefab and on save ("Validation catalogue" of `Zenith2/Docs/Design/CHAR/Debugging.md`), and `Test_CharAuthority_FlagshipCharactersClean` (a ZenithAuthoring test over the flagship slices' committed character prefabs, resolved with SCENE's resolver through ZenithAuthoring's links) asserts no error on any of them.

## Character integration suite

`Zenith2/Tests/Character/`, registered with `zenith_add_integration_tests(Character DEPENDS ZenithRuntime ZenithAnimation ZenithPhysics ZenithPhysics_Jolt ZenithPhysics_PhysX ZenithScript)` (`Zenith2/Docs/Architecture/BuildSystem.md → "The ZENITH_SUPPORTED matrix"` narrows it to its dependencies' presets; `ZenithCinematics` joins at Stage 4 for the C1 case, and `ZenithAudio` at Stage 4 for the lip-sync cases, whose playback contract posts `Audio.PlayDialogue` time-locked), runs in the `Test` step and in stage acceptance. Each scenario builds a headless world through the runtime's test access (`Zenith2/Docs/Design/MOD.md → "Zenith_RuntimeTestAccess.h"`) on both physics backends, spawns the fixture character of "Integration fixtures", drives it by commands recorded from a test system in phase 2, and asserts per tick:

| Test | Scenario | Asserts |
|---|---|---|
| `Test_CharIntegration_RootMotionThroughController[<Backend>]` | state L1: the fixture walk clip for 60 ticks at 30 Hz on flat ground | the root's world position after tick 60 equals the integral of the clip's root motion within 1 mm; `anim.rootmotion.applied_delta` is 0 every tick; `physics.controller.adopted_external_delta` is 0 |
| `Test_CharIntegration_RootMotionWithoutController[<Backend>]` | state L3, the same clip, controller removed | `anim.rootmotion.applied_delta` is 1 per tick; the same position within 1 mm |
| `Test_CharIntegration_ControllerVelocityModeIgnoresRootMotion[<Backend>]` | state L2 with Animator RootMotion `EXTRACT` | the root moves by the scripted desired velocity alone; the authority lint on the same entity reports AU2 |
| `Test_CharIntegration_RagdollOwnershipSequence[<Backend>]` | L1 → R1 (blend 0.1 s) → R2 for 30 ticks → R3 (blend 0.4 s, get-up clip) → L1 | per tick, `Zenith.Animation.Pose.PhysicsWeight` equals PHYS's closed forms (`fEnter`, `fRecover`) bit for bit; during R1 to R3 the root equals the `Physics.RagdollBlend` value; the first R3 tick's final pose equals the ragdoll pose (ANIM's no-double-offset rule); after NONE the controller adopts once and walks |
| `Test_CharIntegration_TeleportDuringLocomotionAndRagdoll[<Backend>]` | a teleport mid-walk and one during R2 | the teleported tick moves the root by exactly the teleport (root-motion reset), the next tick resumes root motion; in R2 every part moves rigidly by the teleport (GAME's `Test_Scenario_TeleportDuringRagdoll` rule, here on the workflow's character shape) |
| `Test_CharIntegration_CinematicHoldRestores[<Backend>]` (Stage 4) | a two-second cinematic with a transform track and a body slot over L1 | during the hold the root equals the track and PhysicsWeight is 0 from the Cinematic writer; after end the gameplay fields are restored at the next tick boundary and the controller resumes from the cinematic's last pose |
| `Test_CharIntegration_AuthorityDeterminism` | the ragdoll sequence and the walk, with 1, 2, 8 and all workers and forced chunk compaction | the per-tick world-state hashes (`Zenith2/Docs/Design/ECS/Scheduling.md → "World state hash"`) are identical across worker counts, per backend |

The cases need no GPU; the suite's fixture set builds the content at test time ("Integration fixtures"), and no binary is committed (Q55).

## Integration fixtures

The fixture set `CHAR_IntegrationFixtures` (a CTest `FIXTURES_SETUP` case, [Z2-N-ARCH-257]) is the setup case `Fixture_CharIntegration_Generate` (`ZenithCook_Tests --test-suite=z2-fixture-setup`, case file `Zenith2/Tools/ZenithCook/Tests/Character/Zenith_CharFixtures_Generate.cpp`). It writes sources into `${ZENITH_OUT}/test-fixtures/CHAR/Integration/` from the declared constants of `Zenith2/Tools/Authoring/Tests/Support/Zenith_CharTestFixtures.h` and cooks them there with the cook test access (ruling GEOM-N2); the suite reads only the cooked output, so its own `DEPENDS` (above) stay runtime libraries and it never links ZenithAuthoring or ZenithCook:

| Content | Stage 3 | Stage 4 |
|---|---|---|
| the 62-joint humanoid in T-pose with a complete `ZENITH_HUMANOID` profile (`Zenith_CharMakeHumanoidGltf(CHAR_FIXTURE_NAMING_ZENITH, CHAR_FIXTURE_MESHES_WALKTHROUGH, 0, false)`, its eight meshes, `.meta` GUID `xCHAR_FIXTURE_HUMANOID_SOURCE_GUID`; `Zenith2/Docs/Design/CHAR/Tests.md → "Fixtures and generators"`) and its rig document | yes | yes |
| a 2-second walk clip with `XZYaw` root motion at 1.4 m/s and a 1-second get-up clip (the rows `IntegrationWalk` and `GetUp` of the clip table of `Zenith_CharMakeLocomotionClips`) | yes | yes |
| a graph with one state machine (Walk, Idle) and the five slots of the humanoid template (`Zenith2/Docs/Design/CHAR/Workflow.md → "Graph template catalogue"`), as JSON text | yes | yes |
| a ragdoll document of "Default ragdoll generation" (`Zenith2/Docs/Design/CHAR/Rigging.md`) | written as JSON text from the constants (the generator does not exist yet) | written by `Zenith_CharBuildDefaultRagdoll` |
| the character prefab | the **root entity alone**, written by the setup case's Stage 3 writer `Zenith_CharWriteStageThreeRootPrefab` from the fixture constants (`Zenith2/Docs/Design/CHAR/Tests.md → "Fixtures and generators"`, tables "Fixture identities" and "Integration fixture assembly"): `Zenith.LocalTransform`, `Zenith.Animation.Animator`, `Zenith.Animation.RootMotion`, `Zenith.Animation.Pose`, `Zenith.Physics.Ragdoll` and the assembly's `Zenith.Physics.CharacterController` | the base prefab and its Travel variant, generated by `Zenith_CharBuildAssemblyPrefabs` from `Zenith_CharMakeFixtureAssembly()` (root plus eight parts with `Zenith.Animation.PoseFollower` and `Zenith.Render.MeshInstance`) |
| a lip-sync document over two generated WAVs in two locales, cooked through `char.lipsync` | no | yes |

The Stage 3 prefab has no part entities because the part rows need `Zenith.Animation.PoseFollower` (and `Zenith.Animation.IKGoals` with an IK graph), which ANIM registers at Stage 4 (`Zenith2/Docs/Design/ANIM.md → "Component registration"`); the Stage 3 authority scenarios need only the root, which carries every field of "Fields in the matrix". The Stage 4 ticket that adds the generator path also adds `Test_CharFixtures_StageThreeRootMatchesGenerator` (`ZenithCook_Tests`), which asserts that the root entity object the Stage 3 writer produces is byte-identical to the root entity object of the generator's output for `Zenith_CharMakeFixtureAssembly()` (both take every ID and field from the same named constants), so the Stage 3 scenarios kept testing the root the workflow produces. The Stage 4 cases (`Test_CharIntegration_CinematicHoldRestores`, `Test_CharIntegration_ModularVisibilitySwitch`, the lip-sync cases) read the generated prefab and the lip-sync clip.
