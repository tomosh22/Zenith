# CHAR requirement catalogue

## Purpose

This catalogue lists every testable obligation of the CHAR area (character production: rig and control authoring, pose debugging, the production side of animation update budgeting, modular characters, animation and physics authority for characters, facial animation and lip-sync workflows, the D-SEL-MOTIONMATCH ruling, the classification of hair alternatives, and the complete character workflow with its timed walkthrough), one requirement per obligation. Each names the inventory items it covers (`Zenith2/Docs/Requirements/Inventory.md → "Inventory v1"`, section "CHAR Character production", items Z2-I-CHAR-01 to Z2-I-CHAR-16), the stage whose acceptance it serves (its fix version), its classification, its governing decisions and budget rows, the artefact that proves it, and the headings of the CHAR design family (`Zenith2/Docs/Design/CHAR.md` and `Zenith2/Docs/Design/CHAR/*.md`) that specify it; the design entry's "Requirement index" is the reverse map. CHAR owns every requirement here. The runtime of item Z2-I-CHAR-04 (animation update budgeting) is ANIM's, by [Z2-N-BUDGET-438], and is covered by ANIM's requirements `Z2-R-ANIM-138` and `Z2-R-ANIM-140` … `Z2-R-ANIM-146` (`Zenith2/Docs/Design/ANIM.md → "Cross-area items ANIM implements"`); this catalogue covers the item's production side. Every named test is specified in `Zenith2/Docs/Design/CHAR/Tests.md`; every test runs headless and generates its fixtures at test time.

## Requirements

### Z2-R-CHAR-001 Tool-side code placement

- **Statement:** CHAR's code lives only in `Zenith2/Tools/Authoring/Internal/CHAR/` with the root header `Zenith2/Tools/Authoring/Zenith_AuthoringCharacter.h`, in `Zenith2/Tools/ZenithCook/Internal/Character/` and in `Zenith2/Tools/ZenithEditor/Internal/Character/`, with tests under the matching `Tests/` directories; no CHAR source is in an engine library.
- **Covers:** Z2-I-CHAR-01, Z2-I-CHAR-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-024, Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `SourceLayout` and `Libgraph` gate steps over the CHAR directories
- **Design:** `Zenith2/Docs/Design/CHAR.md` → "Targets"; `Zenith2/Docs/Design/CHAR.md` → "ZenithAuthoring CHAR files"; `Zenith2/Docs/Design/CHAR.md` → "ZenithCook CHAR files"; `Zenith2/Docs/Design/CHAR.md` → "ZenithEditor CHAR files"

### Z2-R-CHAR-002 No runtime surface

- **Statement:** CHAR adds no runtime library, Luau binding, misuse code, asset kind, asynchronous source kind, save participant or frame-graph node; every CHAR target is a tool target outside every Player link closure.
- **Covers:** Z2-I-CHAR-06, Z2-I-CHAR-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-081, Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Libgraph` gate step (tool targets outside Shipping closures); review of the design's split
- **Design:** `Zenith2/Docs/Design/CHAR.md` → "Split with ANIM"; `Zenith2/Docs/Design/CHAR.md` → "Design rulings owned by CHAR"; `Zenith2/Docs/Design/CHAR.md` → "Luau errors and asserts"

### Z2-R-CHAR-003 Public header contracts

- **Statement:** Every public type of `Zenith_AuthoringCharacter.h` and of CHAR's ZenithCook header carries one contract block, and every fallible public function returns `Zenith_Result<…, Zenith_CharError>`.
- **Covers:** Z2-I-CHAR-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-047, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `ApiContractCheck` and `HeaderSelfContained` gate steps
- **Design:** `Zenith2/Docs/Design/CHAR.md` → "API rules"; `Zenith2/Docs/Design/CHAR.md` → "Header map"

### Z2-R-CHAR-004 Error and diagnostic enumerations

- **Statement:** `Zenith_CharError`, `Zenith_CharSeverity` and `Zenith_CharDiagnosticCode` hold exactly the values of the design with fixed numbers that never move, and `Zenith_ToString` names each.
- **Covers:** Z2-I-CHAR-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_CharDocuments_RegistersFiveTypes` exercises the codes; `ApiContractCheck`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR.md` → "Errors and diagnostics"; `Zenith2/Docs/Design/CHAR.md` → "Error enumerations"

### Z2-R-CHAR-005 API-only slice first

- **Statement:** The declarations of `Zenith_AuthoringCharacter.h` that the Stage 3 authority lint needs (the entity record, the asset query interface, the diagnostics, the lint) land as an API-only slice before their implementation tickets.
- **Covers:** Z2-I-CHAR-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-047
- **Budgets:** none
- **Acceptance evidence:** `HeaderSelfContained` gate step on the slice
- **Design:** `Zenith2/Docs/Design/CHAR.md` → "API-only slices"

### Z2-R-CHAR-006 Character authority matrix

- **Statement:** For each character state of the authority matrix (L1, L2, L3, R1, R2, R3, C1, T1), the root transform, the root-motion delta, the bone pose and the controller are written by exactly the system the matrix names, with the writer source it names.
- **Covers:** Z2-I-CHAR-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080, Z2-D-019, D-PHASES (open, Stage 1)
- **Budgets:** Z2-B-GAME-001
- **Acceptance evidence:** the cases of "Character integration suite"; `Test` gate step and stage acceptance, `Zenith_Integration_Character`
- **Design:** `Zenith2/Docs/Design/CHAR/Authority.md` → "Fields in the matrix"; `Zenith2/Docs/Design/CHAR/Authority.md` → "Character authority matrix"; `Zenith2/Docs/Design/CHAR/Authority.md` → "State transitions and their tick"

### Z2-R-CHAR-007 Authority lint rules AU1 to AU8

- **Statement:** `Zenith_CharLintCharacterAuthority` reports each of AU1 to AU8 with its severity and diagnostic for a resolved document, reading property values by ID with registered defaults, and reports nothing for a clean character.
- **Covers:** Z2-I-CHAR-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080, D-PHASES (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_CharAuthority_AU1AnimatorIncomplete` … `Test_CharAuthority_AU8ReducedRateThroughCollision`, `Test_CharAuthority_CleanCharacterPasses`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Authority.md` → "Authority lint inputs"; `Zenith2/Docs/Design/CHAR/Authority.md` → "Authority lint rules"

### Z2-R-CHAR-008 Character integration suite

- **Statement:** The integration suite `Zenith_Integration_Character` is registered with its dependencies, builds headless worlds on both physics backends, and drives the fixture character by commands from a test system.
- **Covers:** Z2-I-CHAR-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-003, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** the suite registration and its cases; `Test` gate step and stage acceptance, `Zenith_Integration_Character`
- **Design:** `Zenith2/Docs/Design/CHAR/Authority.md` → "Character integration suite"; `Zenith2/Docs/Design/CHAR.md` → "Integration suite and benchmark files"

### Z2-R-CHAR-009 Root motion through the controller

- **Statement:** A character with a controller in a root-motion mode moves by exactly the clip's root motion through collision, with no ANIM root-motion command and no controller adoption.
- **Covers:** Z2-I-CHAR-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** Z2-B-GAME-001
- **Acceptance evidence:** `Test_CharIntegration_RootMotionThroughController[<Backend>]`; `Test` gate step and stage acceptance, `Zenith_Integration_Character`
- **Design:** `Zenith2/Docs/Design/CHAR/Authority.md` → "Character authority matrix"; `Zenith2/Docs/Design/CHAR/Authority.md` → "Character integration suite"

### Z2-R-CHAR-010 Root motion without a controller

- **Statement:** A character without a controller moves by its root motion through ANIM's phase-4 commands, one per tick.
- **Covers:** Z2-I-CHAR-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** Z2-B-GAME-001
- **Acceptance evidence:** `Test_CharIntegration_RootMotionWithoutController[<Backend>]`; `Test` gate step and stage acceptance, `Zenith_Integration_Character`
- **Design:** `Zenith2/Docs/Design/CHAR/Authority.md` → "Character authority matrix"; `Zenith2/Docs/Design/CHAR/Authority.md` → "Character integration suite"

### Z2-R-CHAR-011 Velocity mode ignores root motion

- **Statement:** A controller in VELOCITY mode moves by script intent alone while its animator extracts root motion, and the lint reports AU2 for it.
- **Covers:** Z2-I-CHAR-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_CharIntegration_ControllerVelocityModeIgnoresRootMotion[<Backend>]`; `Test` gate step and stage acceptance, `Zenith_Integration_Character`
- **Design:** `Zenith2/Docs/Design/CHAR/Authority.md` → "Character authority matrix"; `Zenith2/Docs/Design/CHAR/Authority.md` → "Authority lint rules"

### Z2-R-CHAR-012 Ragdoll ownership sequence

- **Statement:** Through enter, active, recover and back to locomotion, the pose weight equals PHYS's closed forms bit for bit each tick, the root follows the ragdoll writer, the first recover tick shows the ragdoll pose, and the controller adopts the root once after NONE.
- **Covers:** Z2-I-CHAR-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080, Z2-D-019, D-PHASES (open, Stage 1)
- **Budgets:** Z2-B-ANIM-003
- **Acceptance evidence:** `Test_CharIntegration_RagdollOwnershipSequence[<Backend>]`; `Test` gate step and stage acceptance, `Zenith_Integration_Character`
- **Design:** `Zenith2/Docs/Design/CHAR/Authority.md` → "Character authority matrix"; `Zenith2/Docs/Design/CHAR/Authority.md` → "State transitions and their tick"; `Zenith2/Docs/Design/CHAR/Authority.md` → "Character integration suite"

### Z2-R-CHAR-013 Teleport during locomotion and ragdoll

- **Statement:** A teleport moves the character by exactly the teleport with no double root-motion step, and during an active ragdoll moves every part rigidly.
- **Covers:** Z2-I-CHAR-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_CharIntegration_TeleportDuringLocomotionAndRagdoll[<Backend>]`; `Test` gate step and stage acceptance, `Zenith_Integration_Character`
- **Design:** `Zenith2/Docs/Design/CHAR/Authority.md` → "Character authority matrix"; `Zenith2/Docs/Design/CHAR/Authority.md` → "Character integration suite"

### Z2-R-CHAR-014 Authority determinism

- **Statement:** The character suite's walk and ragdoll sequences give identical per-tick world-state hashes with 1, 2, 8 and all workers and with forced chunk compaction, per physics backend.
- **Covers:** Z2-I-CHAR-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_CharIntegration_AuthorityDeterminism`; `Test` gate step and stage acceptance, `Zenith_Integration_Character`
- **Design:** `Zenith2/Docs/Design/CHAR/Authority.md` → "Character integration suite"

### Z2-R-CHAR-015 Integration fixtures generated at test time

- **Statement:** The suite's content (skeleton, clips, graph, ragdoll, meshes, character prefab, and from Stage 4 a lip-sync clip) is generated and cooked by the fixture set `CHAR_IntegrationFixtures`, a ZenithCook setup case, into `${ZENITH_OUT}/test-fixtures/CHAR/Integration/` from named constants (the fixture identities and the integration fixture assembly); at Stage 3 the prefab holds the root entity alone; no binary fixture is committed and the suite links no tool library.
- **Covers:** Z2-I-CHAR-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Fixture_CharIntegration_Generate` of `CHAR_IntegrationFixtures` (`ZenithCook_Tests`, suite `z2-fixture-setup`); `Test` gate step and stage acceptance, `Zenith_Integration_Character`
- **Design:** `Zenith2/Docs/Design/CHAR/Authority.md` → "Integration fixtures"; `Zenith2/Docs/Design/CHAR/Tests.md` → "Fixtures and generators"

### Z2-R-CHAR-016 Cinematic hold of a character

- **Statement:** A cinematic holding a character writes its root and pose with the Cinematic writer, and on end the gameplay fields are restored at the next tick boundary and the controller resumes from the cinematic's last pose.
- **Covers:** Z2-I-CHAR-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_CharIntegration_CinematicHoldRestores[<Backend>]`; `Test` gate step and stage acceptance, `Zenith_Integration_Character`
- **Design:** `Zenith2/Docs/Design/CHAR/Authority.md` → "Character authority matrix"; `Zenith2/Docs/Design/CHAR/Authority.md` → "Character integration suite"

### Z2-R-CHAR-017 Fixture prefab equals the generator

- **Statement:** The root entity the Stage 3 fixture writer produces and the root entity of the assembly generator's prefab for the fixture assembly are byte-identical.
- **Covers:** Z2-I-CHAR-06, Z2-I-CHAR-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CharFixtures_StageThreeRootMatchesGenerator`; `Test` gate step, `ZenithCook_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Authority.md` → "Integration fixtures"; `Zenith2/Docs/Design/CHAR/Modular.md` → "Generated prefab layout"

### Z2-R-CHAR-018 Flagship characters lint clean

- **Statement:** Every committed flagship character prefab has no authority-lint error.
- **Covers:** Z2-I-CHAR-06, Z2-I-CHAR-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-021
- **Budgets:** none
- **Acceptance evidence:** `Test_CharAuthority_FlagshipCharactersClean`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Authority.md` → "Authority lint rules"

### Z2-R-CHAR-019 Motion-matching authoring-cost harness

- **Statement:** `Zenith_Bench_CharMotionMatchAuthoring` measures, for the blend-tree and the motion-matching candidates, the database build time and bytes for 10, 50 and 200 clips, the clip-tagging and parameter counts, the template document bytes and the iteration time through a harness-local evaluator, using only ANIM's Stage 3 motion-matching header and pose operations, never node kind 15 or CHAR's Stage 4 template code; its smoke case runs in the `Test` step.
- **Covers:** Z2-I-CHAR-09
- **Stage:** S3
- **Classification:** research
- **Decisions:** Z2-D-084, D-SEL-MOTIONMATCH (open, Stage 3)
- **Budgets:** Z2-B-ANIM-001, Z2-B-ANIM-002
- **Acceptance evidence:** `Bench_CharMotionMatchAuthoring_Smoke` (`Test` gate step); the report under `Zenith2/Docs/Research/MotionMatching/` committed by the ruling ticket
- **Design:** `Zenith2/Docs/Design/CHAR/Workflow.md` → "Motion-matching authoring-cost harness"; `Zenith2/Docs/Design/CHAR.md` → "Classification and selection decisions CHAR owns"

### Z2-R-CHAR-020 D-SEL-MOTIONMATCH ruled

- **Statement:** A `needs-human` ruling ticket rules D-SEL-MOTIONMATCH at Stage 3 from ANIM's runtime harness report and CHAR's authoring-cost report against its acceptance rule, commits both reports under `Zenith2/Docs/Research/MotionMatching/`, and appends the ruling with the next free `Z2-D-NNN` to `Zenith2/Docs/DecisionLog.md`.
- **Covers:** Z2-I-CHAR-09
- **Stage:** S3
- **Classification:** research
- **Decisions:** Z2-D-084, Z2-D-097, D-SEL-MOTIONMATCH (open, Stage 3)
- **Budgets:** Z2-B-ANIM-001, Z2-B-ANIM-002
- **Acceptance evidence:** the DecisionLog entry and the reports; the Stage 3 acceptance list
- **Design:** `Zenith2/Docs/Design/CHAR.md` → "Classification and selection decisions CHAR owns"; `Zenith2/Docs/Design/CHAR.md` → "Open decisions"

### Z2-R-CHAR-021 Motion-matching template follows the ruling

- **Statement:** The template `char.humanoidMotionMatching` is offered by the catalogue exactly when the D-SEL-MOTIONMATCH ruling classifies motion matching required; an exclusion removes it through the inventory change rule in the ruling's diff.
- **Covers:** Z2-I-CHAR-09, Z2-I-CHAR-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-084, D-SEL-MOTIONMATCH (open, Stage 3)
- **Budgets:** none
- **Acceptance evidence:** `Test_CharTemplate_InstantiatesAndCompiles` over the shipped catalogue; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Workflow.md` → "Graph template catalogue"; `Zenith2/Docs/Design/CHAR.md` → "Classification and selection decisions CHAR owns"

### Z2-R-CHAR-022 Walkthrough row allocated

- **Statement:** The Stage 1 per-system budget ticket for CHAR allocates `Z2-B-CHAR-001` with a value and tolerance, commits the walkthrough step list as `Zenith2/Docs/Research/Walkthroughs/CharacterWorkflow.md` from "Walkthrough character workflow", and appends its ruling to `Zenith2/Docs/DecisionLog.md`.
- **Covers:** Z2-I-CHAR-16
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-012, Z2-D-084
- **Budgets:** Z2-B-CHAR-001
- **Acceptance evidence:** the `Budgets.md` row value and the committed step list; Stage 1 acceptance
- **Design:** `Zenith2/Docs/Design/CHAR.md` → "Rows owned"; `Zenith2/Docs/Design/CHAR/Workflow.md` → "Walkthrough character workflow"

### Z2-R-CHAR-023 Rig profile table

- **Statement:** CHAR ships five rig profiles (Zenith humanoid, Mixamo, mannequin-style, Rigify deform, generic) as constexpr tables with ascending roles and unique normalised names.
- **Covers:** Z2-I-CHAR-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_CharRigProfile_TablesWellFormed`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Rigging.md` → "Rig profile table"

### Z2-R-CHAR-024 Joint name normalisation

- **Statement:** Joint names are normalised by namespace stripping, ASCII lower-casing, `_` substitution, collapsing and trimming, and tokenised at separators and case and digit boundaries, exactly as specified.
- **Covers:** Z2-I-CHAR-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_CharRigNames_Normalise`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Rigging.md` → "Joint name normalisation"

### Z2-R-CHAR-025 Profile matching and detection

- **Statement:** `Zenith_CharMatchRigProfile` assigns roles by profile names and `Zenith_CharDetectRigProfile` returns the best profile by score, assigned count and ID, with every unassigned required role reported.
- **Covers:** Z2-I-CHAR-01, Z2-I-CHAR-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_CharRigProfile_DetectsMixamo`, `…DetectsMannequinStyle`, `…DetectsRigify`, `…DetectsZenithHumanoid`, `Test_CharRigProfile_TieBreaksByAssignedThenID`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Rigging.md` → "Profile matching algorithm"

### Z2-R-CHAR-026 Generic profile assignment

- **Statement:** The generic profile assigns hips, legs, head, spine column, arms, fingers and eyes from tokens and topology with the specified tie-breaks, and never assigns a face role.
- **Covers:** Z2-I-CHAR-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_CharRigGeneric_AssignsUnnamedHumanoid`, `Test_CharRigGeneric_KeywordNotReusedDownChain`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Rigging.md` → "Generic profile assignment"

### Z2-R-CHAR-027 Topology verification

- **Statement:** A role whose joint is not a strict descendant of its table parent is unassigned and reported, and a side whose rest position is on the wrong side is reported without reassignment.
- **Covers:** Z2-I-CHAR-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_CharRigTopology_UnassignsNonDescendant`, `Test_CharRigTopology_ReportsSwappedSides`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Rigging.md` → "Topology verification"

### Z2-R-CHAR-028 Mirror pair detection

- **Statement:** Mirror pairs are found by side-token swap, accepted only within the positional tolerance of the X mirror plane, symmetric, and every unpaired joint maps to itself.
- **Covers:** Z2-I-CHAR-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_CharMirror_DetectsAndChecksPositions`, `Test_CharMirror_AmbiguousLeftUnpaired`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Rigging.md` → "Mirror pair detection"

### Z2-R-CHAR-029 Chains and IK definitions

- **Statement:** From a complete profile CHAR generates the spine, neck, arm and leg chains and the two-bone, foot and look-at IK definitions with the specified hints, in table order.
- **Covers:** Z2-I-CHAR-01, Z2-I-CHAR-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_CharChains_GeneratedForCompleteProfile`, `Test_CharChains_StraightLegHintIsFront`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Rigging.md` → "Chain and IK template detection"

### Z2-R-CHAR-030 Rest pose classification

- **Statement:** The rest pose is classified T, A or other per arm elevation, and an up-axis error, a backwards or mirrored facing, a height outside the class range and scaled joints are each reported with the corrective setting, without changing a transform.
- **Covers:** Z2-I-CHAR-01, Z2-I-CHAR-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_CharRestPose_ClassifiesTAndA`, `Test_CharRestPose_DetectsBackwardsFacing`, `Test_CharRestPose_DetectsMirrored`, `Test_CharRestPose_ReportsUnitError`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Rigging.md` → "Rest pose classification"

### Z2-R-CHAR-031 Rig document generation

- **Statement:** The rig document members are written as one undoable edit list that keeps authored roles unless `overwrite`, unions shell meshes, and is empty when applied twice.
- **Covers:** Z2-I-CHAR-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048, Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_CharRigDocument_ApplyTwiceIsNoOp`, `Test_CharRigDocument_KeepsAuthoredRoleUnlessOverwrite`, `Test_CharRigDocument_ShellMeshesFromSource`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Rigging.md` → "Rig document generation"

### Z2-R-CHAR-032 Generated rig cooks humanoid complete

- **Statement:** A generated rig document cooks through ANIM's `anim.skeleton` step into a skeleton whose humanoid profile is complete.
- **Covers:** Z2-I-CHAR-01, Z2-I-CHAR-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_CharRigDocument_CookAcceptsGenerated`; `Test` gate step, `ZenithCook_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Rigging.md` → "Rig document generation"

### Z2-R-CHAR-033 Facial rig template

- **Statement:** The facial template adds the profile's face roles and the sixteen viseme-set curves with their kinds and defaults, and refuses a result above the curve limit before any edit.
- **Covers:** Z2-I-CHAR-01, Z2-I-CHAR-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_CharFacialTemplate_AddsVisemeCurves`, `Test_CharFacialTemplate_CurveLimitRefused`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Rigging.md` → "Facial rig template"; `Zenith2/Docs/Design/CHAR/Facial.md` → "Viseme set"

### Z2-R-CHAR-034 Default ragdoll generation

- **Statement:** A default ragdoll document is generated from the profile with the specified parts, shapes, constraints and mass fractions (summing to 0.93), its capsule offsets and rotations computed with CORE's deterministic functions and every number rounded to 1e-4, and PHYS's step cooks it.
- **Covers:** Z2-I-CHAR-01, Z2-I-CHAR-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-003
- **Budgets:** none
- **Acceptance evidence:** `Test_CharRagdoll_DefaultFromProfile`, `Test_CharRagdoll_IndependentOfCrtFma3Path`, `Test_CharRagdoll_CookAccepts`; `Test` gate step, `ZenithAuthoring_Tests` and `Test` gate step, `ZenithCook_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Rigging.md` → "Default ragdoll generation"

### Z2-R-CHAR-035 Shared library rig rule

- **Statement:** Library rigs carry a complete profile, unit rest scales, +Z facing and a T or A rest pose; no CHAR tool writes a mesh, a skin weight or an inverse bind matrix.
- **Covers:** Z2-I-CHAR-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_CharLibraryRig_ValidationRules`; `Test` gate step, `ZenithAuthoring_Tests`; review
- **Design:** `Zenith2/Docs/Design/CHAR/Rigging.md` → "Shared library rig rule"

### Z2-R-CHAR-036 Retarget document creation

- **Statement:** A retarget document from a library rig to a character uses the role mapping, scales root translation by hips height, maps absent face roles to the target rest, lists unmapped joints when a profile is incomplete, and names the clips given or every clip of the source.
- **Covers:** Z2-I-CHAR-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_CharRetarget_CreateWritesRoleMapping`, `Test_CharRetarget_IncompleteProfileListsJoints`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Rigging.md` → "Retarget setup"; `Zenith2/Docs/Design/CHAR/Workflow.md` → "Retarget step"

### Z2-R-CHAR-037 Retarget rest-to-rest verification

- **Statement:** Creating a retarget document applies ANIM's retarget map to the source rest pose and reports every target joint whose result differs from the target rest by more than 1e-5.
- **Covers:** Z2-I-CHAR-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_CharRetarget_RestCheckUsesAnimMap`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Rigging.md` → "Retarget setup"

### Z2-R-CHAR-038 Rigging limits

- **Statement:** Every rigging limit is enforced at the limit and at limit + 1.
- **Covers:** Z2-I-CHAR-01, Z2-I-CHAR-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_CharRigLimits_AtAndAboveLimit`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Rigging.md` → "Rigging limits"

### Z2-R-CHAR-039 Control set document

- **Statement:** The control set document type holds Scalar and Point controls with Parameter, Curve, Pose, JointRotation and IKGoal targets, validated against the skeleton and graph without failing a load.
- **Covers:** Z2-I-CHAR-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_CharDocuments_CanonicalRoundTrip`, `Test_CharDocumentImporter_ValidatesWithoutProducts`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Rigging.md` → "Control set document"

### Z2-R-CHAR-040 Control evaluation

- **Statement:** Controls are evaluated in the order parameters, curves, poses, joint rotations, IK goals, with values clamped and non-finite inputs ignored and counted.
- **Covers:** Z2-I-CHAR-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_CharControls_EvaluateOrderAndClamp`, `Test_CharControls_PoseWeightFromRange`, `Test_CharControls_NonFiniteIgnored`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Rigging.md` → "Control evaluation"

### Z2-R-CHAR-041 Control poses match the runtime

- **Statement:** A Pose control at full weight produces the same pose as ANIM's `POSE_BLEND` over the cooked pose book within 1e-5.
- **Covers:** Z2-I-CHAR-02, Z2-I-CHAR-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_CharControls_PoseMatchesAnimPoseBlend`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Rigging.md` → "Control evaluation"

### Z2-R-CHAR-042 Pose book document

- **Statement:** The pose book document type holds up to 255 named poses of joint components and curve values that differ from rest.
- **Covers:** Z2-I-CHAR-02, Z2-I-CHAR-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CharDocuments_CanonicalRoundTrip`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Rigging.md` → "Pose book document"

### Z2-R-CHAR-043 Pose book cook

- **Statement:** The `char.posebook` importer writes an `anim.clip` intermediate with frame 0 the rest pose and frame k pose k − 1, step interpolation, one marker per pose and the pose-library flag, byte-identical for identical documents.
- **Covers:** Z2-I-CHAR-02, Z2-I-CHAR-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CharPoseBook_CookFramesAndMarkers`, `Test_CharPoseBook_DeterministicIntermediate`; `Test` gate step, `ZenithCook_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Rigging.md` → "Pose book cook"

### Z2-R-CHAR-044 Pose book name validation

- **Statement:** Pose book names are validated against the skeleton names file read as an import input, and a missing names file warns and defers validation to ANIM's step.
- **Covers:** Z2-I-CHAR-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CharPoseBook_CookFramesAndMarkers` (an unknown name case); `Test` gate step, `ZenithCook_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Rigging.md` → "Pose book name validation"

### Z2-R-CHAR-045 Pose capture and mirroring

- **Statement:** Capturing writes only joint components and curves that differ from rest, and mirroring reflects through the mirror table and swaps side names and side curves.
- **Covers:** Z2-I-CHAR-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_CharPoseBook_CaptureWritesOnlyDifferences`, `Test_CharPoseBook_MirrorSwapsSidesAndCurves`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Rigging.md` → "Pose capture and mirroring"

### Z2-R-CHAR-046 Posing panel

- **Statement:** The posing panel drives an animated-character preview from a control set and edits pose books only through commands, one record per interaction.
- **Covers:** Z2-I-CHAR-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_CharCommands_EachChangingCommandIsOneRecord` (pose book commands); `Test` gate step, `ZenithEditor_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Workflow.md` → "Posing panel"

### Z2-R-CHAR-047 Viseme set

- **Statement:** The viseme set `zenith15` is fifteen viseme curves and one energy curve with the specified names, defaults and estimator coverage.
- **Covers:** Z2-I-CHAR-07, Z2-I-CHAR-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_CharFacialTemplate_AddsVisemeCurves`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Facial.md` → "Viseme set"

### Z2-R-CHAR-048 Face graph template

- **Statement:** The humanoid template's face part chains the masked `char.lipsync` and `cine.face` slots and a `POSE_BLEND` over the face pose book, so empty slots keep body face animation and a lip-sync clip changes no joint.
- **Covers:** Z2-I-CHAR-07, Z2-I-CHAR-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_CharFaceTemplate_GraphCompilesClean`, `Test_CharFaceTemplate_EmptySlotsKeepBodyFace`, `Test_CharFaceTemplate_LipSyncCurvesOnly`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Facial.md` → "Face graph template"

### Z2-R-CHAR-049 Automatic viseme pose books

- **Statement:** For morph-target faces CHAR generates a viseme pose book by direct morph names first and the ARKit-style combination table second, reporting unmapped visemes.
- **Covers:** Z2-I-CHAR-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_CharVisemePoseBook_DirectNamesWin`, `Test_CharVisemePoseBook_ArkitCombination`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Facial.md` → "Automatic viseme pose books"

### Z2-R-CHAR-050 Character project settings document

- **Statement:** Each game has exactly one project document holding the voice locales, the viseme set, the front marker node, the full-rate threshold and the character classes, validated as specified.
- **Covers:** Z2-I-CHAR-08, Z2-I-CHAR-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CharDocuments_CanonicalRoundTrip`, `Test_CharValidation_RunsEveryRuleOnce` (CV13); `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Facial.md` → "Character project settings document"

### Z2-R-CHAR-051 Lip-sync document

- **Statement:** The lip-sync document type holds per bank and face skeleton the locales, the settings with their ranges, and per line and locale the source, audio, visemes, phrases and overrides.
- **Covers:** Z2-I-CHAR-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CharDocuments_CanonicalRoundTrip`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Facial.md` → "Lip-sync document"

### Z2-R-CHAR-052 Sync from bank

- **Statement:** Sync from bank adds, updates and removes lines from AUDIO's bank, keeps authored sources and overrides, copies phrase markers, refuses paths outside the document's directory, and makes no edit when the bank is unchanged.
- **Covers:** Z2-I-CHAR-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CharLipSyncSync_AddsUpdatesRemovesLines`, `…PathOutsideRefused`, `…KeepsAuthoredSources`, `…SecondRunNoEdit`; `Test` gate step, `ZenithEditor_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Facial.md` → "Sync from bank"

### Z2-R-CHAR-053 Estimator input conditioning

- **Statement:** The estimator downmixes in channel order, anti-alias filters with the specified 31-tap FIR at the cut-off min(7,600, 0.475 R) Hz and resamples to 16 kHz by linear interpolation over a zero extension, in binary64; a 16 kHz input passes unfiltered.
- **Covers:** Z2-I-CHAR-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CharEstimator_ClassifiesGeneratedSignals` (its 48 kHz cases and its 16 kHz identity case); `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Facial.md` → "Estimator input"

### Z2-R-CHAR-054 Estimator framing and spectrum

- **Statement:** Frames are 512 samples at a 160-sample hop with a Hann window, timed with the lead, and the 512-point FFT equals a direct DFT within 1e-9 relative.
- **Covers:** Z2-I-CHAR-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CharFft_MatchesDirectDft`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Facial.md` → "Estimator framing and spectrum"

### Z2-R-CHAR-055 Estimator features

- **Statement:** Each frame's energy, five band ratios, centroid, flatness and zero-crossing rate are computed exactly as specified.
- **Covers:** Z2-I-CHAR-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CharEstimator_ClassifiesGeneratedSignals`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Facial.md` → "Estimator features"

### Z2-R-CHAR-056 Estimator classification

- **Statement:** Frames are classified by rules C1 to C4 and the plosive pass, never assigning TH, DD, KK or RR, so the generated test signals give their expected classes.
- **Covers:** Z2-I-CHAR-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CharEstimator_ClassifiesGeneratedSignals`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Facial.md` → "Estimator classification"

### Z2-R-CHAR-057 Estimator smoothing and weights

- **Statement:** Targets are smoothed with the attack and release constants and normalised so that, at every key of the binary64 frame track before reduction and storage, the non-silence weights sum to at most 1 + 1e-12 and silence is their complement within 1e-12.
- **Covers:** Z2-I-CHAR-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CharEstimator_WeightsSumToOneBeforeReduction`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Facial.md` → "Estimator smoothing and normalisation"

### Z2-R-CHAR-058 Key reduction

- **Statement:** Keys are reduced per curve by Ramer–Douglas–Peucker within `reduceEpsilon` in binary64 and stored as f32, keeping the rest keys at time 0 and at the source duration, so the fifteen stored viseme weights, each sampled by interpolation of its own keys, sum to 1 within 15 × `reduceEpsilon` + 1e-6 at every key time.
- **Covers:** Z2-I-CHAR-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CharEstimator_KeyReductionWithinEpsilon`, `Test_CharEstimator_WeightsSumWithinReductionTolerance`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Facial.md` → "Estimator smoothing and normalisation"

### Z2-R-CHAR-059 Phrase markers

- **Statement:** Every phrase start forces rest over the 50 ms before it, with rest keys inserted at both ends.
- **Covers:** Z2-I-CHAR-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CharEstimator_PhraseStartsForceRest`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Facial.md` → "Phrase markers"

### Z2-R-CHAR-060 Estimator determinism

- **Statement:** The estimator gives identical bits on every machine running one build: it calls no CRT transcendental, computes in binary64 in a fixed order and compiles without floating-point contraction, so two runs, a copy of the input and both CRT FMA3 paths give byte-identical binary64 frame features, frame tracks and stored tracks.
- **Covers:** Z2-I-CHAR-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CharEstimator_DeterministicOutput`, `Test_CharEstimator_IndependentOfCrtFma3Path`; `Test` gate step, `ZenithAuthoring_Tests`; the cross-host digest comparison of Z2-R-CHAR-134
- **Design:** `Zenith2/Docs/Design/CHAR/Facial.md` → "Estimator input"; `Zenith2/Docs/Design/CHAR.md` → "Determinism obligations"

### Z2-R-CHAR-061 External viseme import

- **Statement:** A `zenith2.visemes` file is converted to curves through the same smoothing and rejected with its key path when malformed or unordered.
- **Covers:** Z2-I-CHAR-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CharVisemeImport_ConvertsCues`, `Test_CharVisemeImport_RejectsUnorderedTimes`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Facial.md` → "External viseme import"

### Z2-R-CHAR-062 Manual overrides

- **Statement:** Overrides replace a curve's keys over their range in array order and warn when the non-silence sum exceeds 1.05.
- **Covers:** Z2-I-CHAR-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CharLipSyncOverrides_ReplaceRange`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Facial.md` → "Manual overrides"

### Z2-R-CHAR-063 Locale-strided lip-sync clip

- **Statement:** One clip per line holds every voice locale in windows of stride S = ceil((max duration + pad) × rate − 1e-9) / rate, an imported entry's duration being its audio's when it has audio, rest-bounded, with the duration limit of ANIM enforced.
- **Covers:** Z2-I-CHAR-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CharLipSyncClip_StrideAndWindows`, `Test_CharLipSyncClip_StrideCeilTolerance`, `Test_CharLipSyncClip_DurationLimitRefused`, `Test_CharVisemeImport_DurationFromAudio`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Facial.md` → "Locale-strided lip-sync clip"

### Z2-R-CHAR-064 Window markers and end events

- **Statement:** Each window has the marker `char.lipsync.<tag>` at its start and the gameplay event `char.lipsync.windowEnd` with payload l after its content, written through the product's annotation settings.
- **Covers:** Z2-I-CHAR-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CharLipSyncImporter_ProductsPerLine`; `Test` gate step, `ZenithCook_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Facial.md` → "Window markers and events"

### Z2-R-CHAR-065 Clip intermediate writer

- **Statement:** CHAR's clip times are rounded to f32 once with equal pairs collapsed, and CHAR's clips are written through ANIM's curve-and-pose intermediate writer, never through a CHAR copy of the format; ANIM's `anim.clip` step cooks and samples the result.
- **Covers:** Z2-I-CHAR-08, Z2-I-CHAR-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CharClipTimes_RoundAndCollapse` (`Test` gate step, `ZenithAuthoring_Tests`), `Test_CharClipIntermediate_AnimCookAccepts` (`Test` gate step, `ZenithCook_Tests`)
- **Design:** `Zenith2/Docs/Design/CHAR/Facial.md` → "Clip intermediate writer"

### Z2-R-CHAR-066 Lip-sync importer

- **Statement:** The out-of-process `char.lipsync` importer declares one clip product per line, decodes audio through CONTENT's decoder, and fails a missing audio file naming the line, locale and path.
- **Covers:** Z2-I-CHAR-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CharLipSyncImporter_ProductsPerLine`, `Test_CharLipSyncImporter_MissingAudioFails`; `Test` gate step, `ZenithCook_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Facial.md` → "Lip-sync importer"

### Z2-R-CHAR-067 Lip-sync reimport identity

- **Statement:** Every audio, visemes and project file read is an import-input edge, and reimporting unchanged inputs gives identical intermediates and the same import key.
- **Covers:** Z2-I-CHAR-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CharLipSyncImporter_ReimportBitIdentical`; `Test` gate step, `ZenithCook_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Facial.md` → "Lip-sync importer"

### Z2-R-CHAR-068 Lip-sync playback contract

- **Statement:** A player resolves the window from the voice-locale tag through the clip's own markers (the exact tag, then its language subtag, then window 0), starts the clip in `char.lipsync` with the full-rate slot tag and time-locked dialogue audio, and stops it on the first window-end event of that play, so the slot's clip time never reaches the next window.
- **Covers:** Z2-I-CHAR-08, Z2-I-CHAR-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_CharIntegration_LipSyncPlaybackContract`; `Test` gate step and stage acceptance, `Zenith_Integration_Character`
- **Design:** `Zenith2/Docs/Design/CHAR/Facial.md` → "Lip-sync playback contract"

### Z2-R-CHAR-069 Voice locale as a simulation input

- **Statement:** The voice-locale tag is game simulation state changed only by a recorded command at a tick boundary, so a replay reproduces the face exactly under any presentation dialogue locale.
- **Covers:** Z2-I-CHAR-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_CharIntegration_LipSyncLocaleIsSimulationInput`; `Test` gate step and stage acceptance, `Zenith_Integration_Character`
- **Design:** `Zenith2/Docs/Design/CHAR/Facial.md` → "Voice locale as a simulation input"

### Z2-R-CHAR-070 Lip-sync panel

- **Statement:** The lip-sync panel shows each line's per-locale status, edits curves as overrides and phrases through commands, and auditions a line's sound with the cooked clip from its locale window.
- **Covers:** Z2-I-CHAR-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_CharCommands_EachChangingCommandIsOneRecord` (lip-sync commands); `Test` gate step, `ZenithEditor_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Workflow.md` → "Lip-sync panel"

### Z2-R-CHAR-071 Facial and lip-sync limits

- **Statement:** Every facial and lip-sync limit is enforced at the limit and at limit + 1.
- **Covers:** Z2-I-CHAR-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CharLipSyncLimits_AtAndAboveLimit`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Facial.md` → "Facial and lip-sync limits"

### Z2-R-CHAR-072 Assembly document

- **Statement:** The assembly document type holds the skeleton, graph, ragdoll, regions, slots, parts, loadouts, root components and generator records with the specified rules.
- **Covers:** Z2-I-CHAR-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CharDocuments_CanonicalRoundTrip`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Modular.md` → "Assembly document"; `Zenith2/Docs/Design/CHAR/Modular.md` → "Assembly model"

### Z2-R-CHAR-073 Body region visibility

- **Statement:** Part visibility per loadout follows the three region rules, and holes and overlaps are warned.
- **Covers:** Z2-I-CHAR-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_CharAssembly_VisibilityRules`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Modular.md` → "Body regions"

### Z2-R-CHAR-074 Slots and part caps

- **Statement:** A loadout holds at most one part per exclusive slot and at most 16 visible parts; an assembly at most 64 parts and 32 loadouts.
- **Covers:** Z2-I-CHAR-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_CharAssemblyValidation_EachRule` (MV4, MV5); `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Modular.md` → "Slots and loadouts"; `Zenith2/Docs/Design/CHAR/Modular.md` → "Part caps and cost"

### Z2-R-CHAR-075 Generated prefab layout

- **Statement:** The generated base prefab has a root with the animator, root motion, pose, IK goals and ragdoll components and the root components verbatim, and one follower child per part named `Part.<slot>.<part name>`.
- **Covers:** Z2-I-CHAR-05, Z2-I-CHAR-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_CharAssembly_GeneratedPrefabResolves`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Modular.md` → "Generated prefab layout"

### Z2-R-CHAR-076 Derived entity identity

- **Statement:** Generated entity IDs derive from the prefab ID and the part's persistent ID, so a rename, reorder or added part changes no surviving ID and scene overrides stay valid.
- **Covers:** Z2-I-CHAR-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_CharAssembly_DerivedIDsStableAcrossRegeneration`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Modular.md` → "Generated entity identity"

### Z2-R-CHAR-077 Loadout variants

- **Statement:** Each non-default loadout is a variant of the base prefab whose override block holds only the visibility changes.
- **Covers:** Z2-I-CHAR-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_CharAssembly_VariantOverridesOnlyVisibility`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Modular.md` → "Loadout variants"

### Z2-R-CHAR-078 Prefab generation

- **Statement:** Generation validates first, writes only changed documents and the generator records in one journaled batch, trashes retired loadouts, and writes nothing on a second run.
- **Covers:** Z2-I-CHAR-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_CharAssembly_GenerationIsCanonicalAndIdempotent`, `Test_CharAssembly_RetiredLoadoutTrashed`; `Test` gate step, `ZenithEditor_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Modular.md` → "Prefab generation algorithm"

### Z2-R-CHAR-079 Hand-edit protection

- **Statement:** A generated prefab whose digest over its canonical re-serialisation differs is never overwritten without `force`, a line-ending conversion is not an edit, and keeping it as a variant moves the edit into a variant that survives regeneration.
- **Covers:** Z2-I-CHAR-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_CharAssembly_HandEditRefusedWithoutForce`, `Test_CharAssembly_DigestIgnoresLineEndings`, `Test_CharAssembly_KeepAsVariantPreservesEdit`; `Test` gate step, `ZenithEditor_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Modular.md` → "Hand-edit protection"; `Zenith2/Docs/Design/CHAR/Modular.md` → "Prefab generation algorithm"

### Z2-R-CHAR-080 Assembly validation

- **Statement:** Assembly validation reports rules MV1 to MV13 with their severities, MV12 reserving the ragdoll component while the assembly names a ragdoll, and nothing for the walkthrough assembly.
- **Covers:** Z2-I-CHAR-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_CharAssemblyValidation_EachRule`, `Test_CharAssemblyValidation_CleanFixturePasses`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Modular.md` → "Assembly validation rules"

### Z2-R-CHAR-081 Rigid attachments

- **Statement:** A RigidAttachment part's prop mesh carries GEOM's rigid-attach setting with the socket's offset, so it is a one-joint skinned follower; without the setting validation refuses it.
- **Covers:** Z2-I-CHAR-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079, Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_CharRigidAttachment_WritesSocketSetting`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Modular.md` → "Rigid attachments"

### Z2-R-CHAR-082 Modular part cost

- **Statement:** Modular characters' extraction cost is reported against the pending row "Modular part extraction overhead" in the W-Characters modular variant, inside the render-extraction span.
- **Covers:** Z2-I-CHAR-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-RPROD-003, Z2-B-GEOM-004
- **Acceptance evidence:** the W-Characters modular-variant run (stage acceptance, `needs-human`)
- **Design:** `Zenith2/Docs/Design/CHAR/Modular.md` → "Part caps and cost"; `Zenith2/Docs/Design/CHAR.md` → "Pending row needs"

### Z2-R-CHAR-083 Runtime part switching

- **Statement:** Switching a part's visibility at run time by `SetProperty` on its named entity stops extracting the hidden part's pose and starts the shown part's, with no follower refused.
- **Covers:** Z2-I-CHAR-05, Z2-I-CHAR-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-077
- **Budgets:** none
- **Acceptance evidence:** `Test_CharIntegration_ModularVisibilitySwitch`; `Test` gate step and stage acceptance, `Zenith_Integration_Character`
- **Design:** `Zenith2/Docs/Design/CHAR/Modular.md` → "Runtime part switching"

### Z2-R-CHAR-084 Assembly panel

- **Statement:** The assembly panel seeds, edits and previews assemblies and runs generation through commands, showing each generated document's status.
- **Covers:** Z2-I-CHAR-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_CharCommands_EachChangingCommandIsOneRecord` (assembly commands); `Test` gate step, `ZenithEditor_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Workflow.md` → "Assembly panel"

### Z2-R-CHAR-085 Hair alternatives classified

- **Statement:** Hair cards are classified required (authoring rules at Stage 4, anisotropic highlight at Stage 5) and recorded as a dated amendment of Z2-D-078.
- **Covers:** Z2-I-CHAR-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-078, Z2-D-043
- **Budgets:** none
- **Acceptance evidence:** the DecisionLog amendment of Z2-D-078 (core-doc item X327); A4 sign-off
- **Design:** `Zenith2/Docs/Design/CHAR/Modular.md` → "Hair alternatives classification"

### Z2-R-CHAR-086 Hair shells excluded

- **Statement:** Hair shells and strand-based hair are excluded: no requirement, ticket or code implements them.
- **Covers:** Z2-I-CHAR-10
- **Stage:** S4
- **Classification:** excluded
- **Decisions:** Z2-D-078, Z2-D-043
- **Budgets:** none
- **Acceptance evidence:** the rationale of "Hair alternatives classification" and the DecisionLog amendment (X327)
- **Design:** `Zenith2/Docs/Design/CHAR/Modular.md` → "Hair alternatives classification"

### Z2-R-CHAR-087 Hair card authoring rules

- **Statement:** A hair-card part is checked by rules HC1 to HC5.
- **Covers:** Z2-I-CHAR-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_CharHairRules_EachRule`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Modular.md` → "Hair card authoring rules"

### Z2-R-CHAR-088 Hair card anisotropic highlight

- **Statement:** From Stage 5 hair-card materials use the anisotropy surface model, and rule HC3 warns when it is absent; the appearance is a CinematicSlice sign-off item.
- **Covers:** Z2-I-CHAR-10
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_CharHairRules_EachRule` (HC3); CinematicSlice sign-off (`needs-human`)
- **Design:** `Zenith2/Docs/Design/CHAR/Modular.md` → "Hair card authoring rules"

### Z2-R-CHAR-089 Hair card secondary motion

- **Statement:** Hair-card joint motion is authored animation; a simulated drive follows the D-CLASS-SIM ruling through that ruling's revalidation.
- **Covers:** Z2-I-CHAR-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-078, D-CLASS-SIM (open, Stage 2)
- **Budgets:** none
- **Acceptance evidence:** review of the revalidation ticket of D-CLASS-SIM
- **Design:** `Zenith2/Docs/Design/CHAR/Modular.md` → "Hair alternatives classification"

### Z2-R-CHAR-090 Pose debugger reads without writing

- **Statement:** The pose debugger reads play-world, preview and file poses once per editor frame and changes no simulation state.
- **Covers:** Z2-I-CHAR-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_CharPoseDebugger_ReadsWithoutWriting`; `Test` gate step, `ZenithEditor_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Debugging.md` → "Pose debugger panel"; `Zenith2/Docs/Design/CHAR/Debugging.md` → "Pose sources"

### Z2-R-CHAR-091 Joint table

- **Statement:** The joint table shows per joint the local and model transforms, the delta from rest and the shear, non-finite and scale flags, decomposed from the model pose.
- **Covers:** Z2-I-CHAR-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_CharPoseDebugger_JointTableFromModelPose`; `Test` gate step, `ZenithEditor_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Debugging.md` → "Joint table"

### Z2-R-CHAR-092 Skeleton overlays

- **Statement:** The pose debugger draws bones by rate tier, joint axes, the rest ghost, mirror links, sockets and names in its viewport and, with the preference, in scene views.
- **Covers:** Z2-I-CHAR-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** review of the panel; `Test_CharEditorExtension_RegistersEverything`; `Test` gate step, `ZenithEditor_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Debugging.md` → "Skeleton overlays"

### Z2-R-CHAR-093 Pose snapshot format

- **Statement:** Pose snapshots round-trip exactly and the reader refuses unknown versions and over-limit content.
- **Covers:** Z2-I-CHAR-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CharPoseSnapshot_RoundTripExact`, `…RejectsUnknownVersion`, `…RejectsOverLimits`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Debugging.md` → "Pose snapshot format"

### Z2-R-CHAR-094 Pose diff

- **Statement:** Pose diffs pair joints by index or name and report translation, sign-independent rotation and scale deltas beyond tolerance.
- **Covers:** Z2-I-CHAR-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_CharPoseDiff_DetectsEachDelta`, `…SignIndependentRotation`, `…PairsByNameAcrossRigs`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Debugging.md` → "Pose diff algorithm"

### Z2-R-CHAR-095 Pose history recorder

- **Statement:** The recorder keeps a ring of final poses per recorded character, scrubbable, shedding its oldest half under memory pressure.
- **Covers:** Z2-I-CHAR-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_CharPoseHistory_RingOrderAndCapacity`, `Test_CharPoseHistory_ShedsHalfUnderPressure`; `Test` gate step, `ZenithEditor_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Debugging.md` → "Pose history recorder"

### Z2-R-CHAR-096 Pose assertions in automation

- **Statement:** The automation step `assertPoseMatches` compares a live pose with a snapshot within tolerances and fails with the worst joints.
- **Covers:** Z2-I-CHAR-03, Z2-I-CHAR-16
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_CharAutomation_StepsValidateMembers`; `Test` gate step, `ZenithEditor_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Workflow.md` → "Automation steps"; `Zenith2/Docs/Design/CHAR/Debugging.md` → "Pose diff algorithm"

### Z2-R-CHAR-097 Character class presets

- **Statement:** Generated animators take their significance bias and rate flags from their character class.
- **Covers:** Z2-I-CHAR-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-ANIM-002
- **Acceptance evidence:** `Test_CharAssembly_GeneratedPrefabResolves` (animator fields); `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Debugging.md` → "Character class presets"; `Zenith2/Docs/Design/CHAR/Modular.md` → "Generated prefab layout"

### Z2-R-CHAR-098 Budget panel

- **Statement:** The budget panel shows ANIM's rate tiers and per-character significance and predicts divisors through ANIM's own rate function.
- **Covers:** Z2-I-CHAR-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-ANIM-002
- **Acceptance evidence:** `Test_CharBudget_PredictMatchesAnimRule` (`Test` gate step, `ZenithAuthoring_Tests`), `Test_CharBudget_PanelRowCap` (`Test` gate step, `ZenithEditor_Tests`)
- **Design:** `Zenith2/Docs/Design/CHAR/Debugging.md` → "Budget panel"

### Z2-R-CHAR-099 Class rate lint

- **Statement:** Rules AU9, CB1 and CB2 report class-rate mismatches, bias overrides and too many full-rate characters per scene.
- **Covers:** Z2-I-CHAR-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** `Test_CharAuthority_AU9ClassRate`, `Test_CharValidation_RunsEveryRuleOnce`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Debugging.md` → "Class rate lint"

### Z2-R-CHAR-100 Content rule measures

- **Statement:** Character classes select their ContentSpecs rows through the `character-<class>` label CHAR's import wizard writes, while the measure of every quantity is registered by its owner (ANIM, GEOM) and never by CHAR.
- **Covers:** Z2-I-CHAR-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** `Test_CharContentRules_ClassLabelSelectsLimit`; `Test` gate step, `ZenithCook_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Debugging.md` → "Content rule measures"

### Z2-R-CHAR-101 Budgeting measured in W-Characters

- **Statement:** W-Characters' budgeting run with the default class presets reports the reduced-rate cost against ANIM's rows.
- **Covers:** Z2-I-CHAR-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-ANIM-002, Z2-B-ANIM-003
- **Acceptance evidence:** the W-Characters budgeting run of ANIM's Stage 4 budget ruling (`needs-human`), with CHAR's presets
- **Design:** `Zenith2/Docs/Design/CHAR/Debugging.md` → "Character class presets"; `Zenith2/Docs/Design/CHAR.md` → "Rows measured against"

### Z2-R-CHAR-102 Editor extension

- **Statement:** CHAR's editor extension registers its nine panel types, four document handlers, commands, automation steps and preferences exactly once.
- **Covers:** Z2-I-CHAR-11, Z2-I-CHAR-12, Z2-I-CHAR-13, Z2-I-CHAR-14, Z2-I-CHAR-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079, Z2-D-024
- **Budgets:** none
- **Acceptance evidence:** `Test_CharEditorExtension_RegistersEverything`; `Test` gate step, `ZenithEditor_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Workflow.md` → "Editor extension"

### Z2-R-CHAR-103 Character commands

- **Statement:** Every character command is automatable, validates its arguments, accepts asset paths with product keys, hierarchy paths and position references wherever it names an asset or an entity, and every document-changing command produces exactly one history record whose undo restores every document.
- **Covers:** Z2-I-CHAR-11, Z2-I-CHAR-12, Z2-I-CHAR-13, Z2-I-CHAR-14, Z2-I-CHAR-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_CharCommands_EachChangingCommandIsOneRecord`, `Test_CharCommands_InvalidArgsRefused`, `Test_CharCommands_ArgumentFormsResolve`; `Test` gate step, `ZenithEditor_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Workflow.md` → "Character commands"; `Zenith2/Docs/Design/CHAR/Workflow.md` → "Command argument forms"

### Z2-R-CHAR-104 Automation steps and events

- **Statement:** CHAR's automation steps validate their members before the first step and its events are published at their defined points.
- **Covers:** Z2-I-CHAR-16
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_CharAutomation_StepsValidateMembers`; `Test` gate step, `ZenithEditor_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Workflow.md` → "Automation steps"; `Zenith2/Docs/Design/CHAR/Workflow.md` → "Editor events"

### Z2-R-CHAR-105 Character import wizard

- **Statement:** The import wizard assigns the source's GUID, writes the character preset, class labels and expectation settings, imports, and applies the front-marker setting in a second pass, all in one record, and refuses FBX unless D-SEL-FBX selects a path.
- **Covers:** Z2-I-CHAR-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079, D-SEL-FBX (open, Stage 4)
- **Budgets:** none
- **Acceptance evidence:** `Test_CharImportWizard_WritesCharacterSettings`, `Test_CharImportWizard_FrontMarkerSecondPass`; `Test` gate step, `ZenithEditor_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Workflow.md` → "Character import wizard"

### Z2-R-CHAR-106 Import facing and unit report

- **Statement:** After import the wizard reports facing, height and skeleton count and offers the corrective setting as one record.
- **Covers:** Z2-I-CHAR-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_CharImportWizard_ReportsBackwardsFacing`; `Test` gate step, `ZenithEditor_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Workflow.md` → "Character import wizard"; `Zenith2/Docs/Design/CHAR/Rigging.md` → "Rest pose classification"

### Z2-R-CHAR-107 Workflow panel

- **Statement:** The workflow panel computes each step's state from documents and session state and recomputes it on history and cook events.
- **Covers:** Z2-I-CHAR-11, Z2-I-CHAR-12, Z2-I-CHAR-13, Z2-I-CHAR-14, Z2-I-CHAR-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_CharWorkflowPanel_StepStatesFromDocuments`, `Test_CharWorkflowPanel_DriveRunsTimeline`; `Test` gate step, `ZenithEditor_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Workflow.md` → "Workflow panel"; `Zenith2/Docs/Design/CHAR/Workflow.md` → "Workflow overview"

### Z2-R-CHAR-108 Rig profile panel

- **Statement:** The rig profile panel shows detection, diagnostics and the diff of the rig document before Apply, and applies through one command.
- **Covers:** Z2-I-CHAR-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_CharCommands_EachChangingCommandIsOneRecord` (`character.rig.applyProfile`); `Test` gate step, `ZenithEditor_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Workflow.md` → "Rig profile panel"

### Z2-R-CHAR-109 Retarget step

- **Statement:** The workflow's retarget step creates the retarget from the class's `libraryRig` of the project document and opens ANIM's retarget panel.
- **Covers:** Z2-I-CHAR-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_CharWorkflowPanel_StepStatesFromDocuments`, `Test_Walkthrough_CharacterWorkflow_Rehearsal` (step 3); `Test` gate step, `ZenithEditor_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Workflow.md` → "Retarget step"

### Z2-R-CHAR-110 Graph template catalogue

- **Statement:** The catalogue ships `char.humanoidLocomotion` and `char.simpleLoop` with their role tables, parameters, machine, slots, IK and editor members.
- **Covers:** Z2-I-CHAR-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_CharTemplate_InstantiatesAndCompiles`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Workflow.md` → "Graph template catalogue"

### Z2-R-CHAR-111 Template clip binding

- **Statement:** Clips bind to template roles by required and excluded tokens with the specified tie-breaks, explicit bindings win, and a missing required role fails naming the candidates.
- **Covers:** Z2-I-CHAR-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_CharTemplateBinding_MatchesByTokens`, `…ExplicitBindingWins`, `…RequiredMissingFails`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Workflow.md` → "Template clip binding"

### Z2-R-CHAR-112 Template instantiation

- **Statement:** Instantiation substitutes stable derived IDs, removes unbound optional nodes, adds the face part on request, and compiles clean.
- **Covers:** Z2-I-CHAR-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_CharTemplate_InstantiatesAndCompiles`, `Test_CharTemplate_ReinstantiationKeepsIDs`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Workflow.md` → "Template instantiation"

### Z2-R-CHAR-113 Template slots for cinematics and get-up

- **Statement:** Every humanoid template declares the slots `Action`, `Zenith.GetUp`, `cine.body`, `char.lipsync` and `cine.face`.
- **Covers:** Z2-I-CHAR-13, Z2-I-CHAR-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_CharTemplate_HasCinematicAndGetUpSlots`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Workflow.md` → "Graph template catalogue"

### Z2-R-CHAR-114 Preview scenarios

- **Statement:** Preview scenarios are parameter timelines with stateAt, eventCount, rootDistance, finite and poseMatches assertions, and the built-ins ship with their templates.
- **Covers:** Z2-I-CHAR-14
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_CharScenario_RunsBuiltInsOnFixtureGraph` (the `lipsync-line` case on a clip built in the test); `Test` gate step, `ZenithCook_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Workflow.md` → "Preview scenarios"

### Z2-R-CHAR-115 Scenario runner

- **Statement:** The scenario runner steps ANIM's preview at the scenario rate without a renderer, reports failed assertions with observed values, and is deterministic.
- **Covers:** Z2-I-CHAR-14
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_CharScenario_FailingAssertionReported`, `Test_CharScenario_Deterministic`; `Test` gate step, `ZenithCook_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Workflow.md` → "Scenario runner"

### Z2-R-CHAR-116 In-game step

- **Statement:** A generated prefab spawned in a play world binds within 10 ticks and moves under a driven scenario timeline.
- **Covers:** Z2-I-CHAR-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_CharInGame_SpawnBindsWithinTicks`; `Test` gate step, `ZenithEditor_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Workflow.md` → "In-game step"

### Z2-R-CHAR-117 Validation catalogue

- **Statement:** The validation catalogue runs rules CV1 to CV16 for the documents of a request and returns diagnostics in the defined order.
- **Covers:** Z2-I-CHAR-11, Z2-I-CHAR-12, Z2-I-CHAR-13, Z2-I-CHAR-14, Z2-I-CHAR-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_CharValidation_RunsEveryRuleOnce`, `Test_CharValidation_SortedDeterministically`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Debugging.md` → "Validation catalogue"

### Z2-R-CHAR-118 Validation panel and command

- **Statement:** The validation panel refreshes on save and on demand, and `character.validate` runs the catalogue and publishes `characterValidated`.
- **Covers:** Z2-I-CHAR-11, Z2-I-CHAR-12, Z2-I-CHAR-13, Z2-I-CHAR-14, Z2-I-CHAR-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_CharAutomation_StepsValidateMembers` (`assertCharacterValid`); `Test` gate step, `ZenithEditor_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Debugging.md` → "Validation panel"

### Z2-R-CHAR-119 Walkthrough step list

- **Statement:** The character workflow walkthrough has the eight steps, starting state and check of "Walkthrough character workflow".
- **Covers:** Z2-I-CHAR-16
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** Z2-B-CHAR-001
- **Acceptance evidence:** the committed `Zenith2/Docs/Research/Walkthroughs/CharacterWorkflow.md`
- **Design:** `Zenith2/Docs/Design/CHAR/Workflow.md` → "Walkthrough character workflow"

### Z2-R-CHAR-120 Walkthrough rehearsal

- **Statement:** The rehearsal script performs every walkthrough step that is not a pure widget gesture through commands whose arguments are literals (paths, product keys, hierarchy paths, explicit IDs) on generated content in an EditorFixture copy that composes the Audio module, and its test asserts the walkthrough's check, headless.
- **Covers:** Z2-I-CHAR-16, Z2-I-CHAR-11, Z2-I-CHAR-12, Z2-I-CHAR-13, Z2-I-CHAR-14, Z2-I-CHAR-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_Walkthrough_CharacterWorkflow_Rehearsal`; `Test` gate step, `ZenithEditor_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Workflow.md` → "Walkthrough rehearsal"; `Zenith2/Docs/Design/CHAR/Workflow.md` → "Command argument forms"

### Z2-R-CHAR-121 Walkthrough fixtures

- **Statement:** The walkthrough sources are generated into a scratch copy of the editor fixture game from constants, and no binary is committed.
- **Covers:** Z2-I-CHAR-16
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CharWalkthrough_SetupWritesSources`; `Test` gate step, `ZenithEditor_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Workflow.md` → "Walkthrough fixtures"

### Z2-R-CHAR-122 Timed walkthrough run

- **Statement:** A person completes the character workflow walkthrough on CinematicSlice within `Z2-B-CHAR-001` plus its tolerance, with the check passing.
- **Covers:** Z2-I-CHAR-16
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-012, Z2-D-021
- **Budgets:** Z2-B-CHAR-001
- **Acceptance evidence:** the MP-WALKTHROUGH evidence record with per-step times (Stage 4 acceptance, `needs-human`)
- **Design:** `Zenith2/Docs/Design/CHAR/Workflow.md` → "Walkthrough character workflow"; `Zenith2/Docs/Design/CHAR.md` → "Acceptance runs"

### Z2-R-CHAR-123 Document types registered tool-side

- **Statement:** `Zenith_RegisterCharacterDocumentTypes` registers the five CHAR document types with their stable names, schema tags, suffixes, canonical schemas and migration chains.
- **Covers:** Z2-I-CHAR-02, Z2-I-CHAR-05, Z2-I-CHAR-08, Z2-I-CHAR-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CharDocuments_RegistersFiveTypes`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR.md` → "Document registration API"; `Zenith2/Docs/Design/CHAR.md` → "Document type table"

### Z2-R-CHAR-124 Canonical documents

- **Statement:** CHAR documents round-trip canonically byte for byte and preserve unknown members with a warning.
- **Covers:** Z2-I-CHAR-02, Z2-I-CHAR-05, Z2-I-CHAR-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_CharDocuments_CanonicalRoundTrip`, `Test_CharDocuments_UnknownMembersPreserved`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR.md` → "Migration and re-cook"

### Z2-R-CHAR-125 Document migration fixtures

- **Statement:** Each CHAR document type has a version-1 migration fixture under `Zenith2/Tools/Authoring/Tests/Fixtures/Migration/CHAR/<Type>/`.
- **Covers:** Z2-I-CHAR-02, Z2-I-CHAR-05, Z2-I-CHAR-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_CharDocuments_MigrationFixtures`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR.md` → "Migration and re-cook"

### Z2-R-CHAR-126 Document type list

- **Statement:** CHAR's five document types join the authoring-document type list of [Z2-N-CONTRACT-552] in the diff that introduces them.
- **Covers:** Z2-I-CHAR-02, Z2-I-CHAR-05, Z2-I-CHAR-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** the Contracts list (core-doc item X328); review
- **Design:** `Zenith2/Docs/Design/CHAR.md` → "Migration and re-cook"; `Zenith2/Docs/Design/CHAR.md` → "Reconciliation items"

### Z2-R-CHAR-127 Importer registration

- **Statement:** `Zenith_RegisterCharacterImporters` registers `char.lipsync`, `char.posebook` and `char.document` with their suffixes at the cook services' importer slot, and CHAR registers no cook step and no content-rule measure.
- **Covers:** Z2-I-CHAR-08, Z2-I-CHAR-02, Z2-I-CHAR-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-024
- **Budgets:** none
- **Acceptance evidence:** `Test_CharImporters_RegisterThree`; `Test` gate step, `ZenithCook_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR.md` → "Importers and cook registration"

### Z2-R-CHAR-128 Document validation importer

- **Statement:** The `char.document` importer imports assembly, control set and project documents and visemes, pose-book names, scenario and snapshot files with zero products and reports their rule diagnostics, so none is an unrecognised file.
- **Covers:** Z2-I-CHAR-05, Z2-I-CHAR-02, Z2-I-CHAR-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CharDocumentImporter_ValidatesWithoutProducts`; `Test` gate step, `ZenithCook_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR.md` → "Document validation importer"

### Z2-R-CHAR-129 Reader robustness

- **Statement:** CHAR's six readers of untrusted input reject every input of a deterministic mutated corpus of 10,000 inputs per in-process seed without an assert, a crash or a leak, and each case asserts its input count ran; CHAR's tools are absent from `win-asan`, so the corpus runs in `ZenithAuthoring_Tests` in place of fuzz targets, without ASan coverage.
- **Covers:** Z2-I-CHAR-08, Z2-I-CHAR-05, Z2-I-CHAR-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** the `Test_CharRobustness_*` cases; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Tests.md` → "Fuzz targets"; `Zenith2/Docs/Design/CHAR.md` → "Character document limits"

### Z2-R-CHAR-130 Parser limits recorded

- **Statement:** CHAR's document limits join the Architecture parser-limits table and each is tested at the limit and at limit + 1.
- **Covers:** Z2-I-CHAR-08, Z2-I-CHAR-05, Z2-I-CHAR-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_CharRigLimits_AtAndAboveLimit`, `Test_CharLipSyncLimits_AtAndAboveLimit`; the Architecture row (core-doc item X329)
- **Design:** `Zenith2/Docs/Design/CHAR.md` → "Character document limits"

### Z2-R-CHAR-131 Diagnostics and counters

- **Statement:** CHAR's tools publish their editor counters, log categories and the cook report member `characters` as specified.
- **Covers:** Z2-I-CHAR-11, Z2-I-CHAR-12, Z2-I-CHAR-13, Z2-I-CHAR-14, Z2-I-CHAR-15, Z2-I-CHAR-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_CharLipSyncImporter_ProductsPerLine` (the report member); `Test` gate step, `ZenithCook_Tests`; review
- **Design:** `Zenith2/Docs/Design/CHAR.md` → "Counters"; `Zenith2/Docs/Design/CHAR.md` → "Log categories"; `Zenith2/Docs/Design/CHAR.md` → "Cook report member"

### Z2-R-CHAR-132 Shipped document migration fixtures

- **Statement:** Every CHAR document version shipped in a release keeps its migration fixture, and the Stage 6 compatibility demonstration migrates each.
- **Covers:** Z2-I-CHAR-02, Z2-I-CHAR-05, Z2-I-CHAR-08
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_CharDocuments_MigrationFixtures` in Stage 6 acceptance
- **Design:** `Zenith2/Docs/Design/CHAR.md` → "Migration and re-cook"

### Z2-R-CHAR-133 No CRT transcendental in CHAR code

- **Statement:** No CHAR source, fixture generator or harness source calls a CRT transcendental ([Z2-N-ARCH-888]): sines, cosines, exponentials, logarithms and arctangents are CORE's deterministic binary64 functions of `Zenith_Transcendental.h`, and square roots are `Zenith_Sqrt`; no CHAR source includes `<cmath>` or `<math.h>`.
- **Covers:** Z2-I-CHAR-01, Z2-I-CHAR-07, Z2-I-CHAR-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** the CTest `ZenithAuthoring.CharNoCrtTranscendental` (`cmake -P` source scan; `Test` gate step); `Test_CharEstimator_IndependentOfCrtFma3Path`, `Test_CharRagdoll_IndependentOfCrtFma3Path` (`Test` gate step, `ZenithAuthoring_Tests`); the programme-wide `ForbiddenCheck` rule of core-doc item X362 once its PROC change lands
- **Design:** `Zenith2/Docs/Design/CHAR/Facial.md` → "Estimator input"; `Zenith2/Docs/Design/CHAR.md` → "Determinism obligations"

### Z2-R-CHAR-134 Cross-host determinism of baked outputs

- **Statement:** CHAR's estimator tracks and default ragdoll documents are byte-identical on two reference hosts of one build.
- **Covers:** Z2-I-CHAR-01, Z2-I-CHAR-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_CharDeterminism_WritesDigests` (`Test` gate step, `ZenithAuthoring_Tests`) and the comparison of the two hosts' digest files in Stage 4 acceptance (`needs-human`)
- **Design:** `Zenith2/Docs/Design/CHAR/Facial.md` → "Estimator input"; `Zenith2/Docs/Design/CHAR.md` → "Acceptance runs"

### Z2-R-CHAR-135 Estimator key-time domain

- **Statement:** A frame becomes a key only when its time lies strictly between 0 and the source duration, and every curve's first and last keys are rest keys at 0 and at the source duration, with strictly increasing times.
- **Covers:** Z2-I-CHAR-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_CharEstimator_KeyTimeDomain`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Facial.md` → "Estimator framing and spectrum"; `Zenith2/Docs/Design/CHAR/Facial.md` → "Estimator smoothing and normalisation"

### Z2-R-CHAR-136 Voice locale after a save restore

- **Statement:** A save stores the voice-locale tag in effect, and when the user's setting differs after a restore, the first restored tick issues the recorded voice-locale command so lips and voice agree, while a replay applies only its recorded commands.
- **Covers:** Z2-I-CHAR-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-077
- **Budgets:** none
- **Acceptance evidence:** `Test_CharIntegration_LipSyncLocaleAfterRestore`, `Test_CharIntegration_LipSyncLocaleUnchangedAfterRestore`; `Test` gate step and stage acceptance, `Zenith_Integration_Character`
- **Design:** `Zenith2/Docs/Design/CHAR/Facial.md` → "Voice locale as a simulation input"

### Z2-R-CHAR-137 Lip-sync padding bound

- **Statement:** `padSeconds` lies in [0.2, 1], so an event-driven stop at 30 Hz or above never reads a clip time at or beyond the next window's start.
- **Covers:** Z2-I-CHAR-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_CharIntegration_LipSyncPlaybackContract` (the per-tick slot-time assertion at 0.2), `Test_CharLipSyncLimits_AtAndAboveLimit`; `Test` gate step
- **Design:** `Zenith2/Docs/Design/CHAR/Facial.md` → "Lip-sync playback contract"; `Zenith2/Docs/Design/CHAR/Facial.md` → "Lip-sync document"

### Z2-R-CHAR-138 Keep as variant mapping

- **Statement:** Keep as variant maps every added or removed entity and component and every changed property of a hand-edited generated prefab into a new variant's entities and override block, and refuses with `CHAR_ERROR_EDIT_UNREPRESENTABLE`, writing nothing, when an edit has no override form.
- **Covers:** Z2-I-CHAR-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_CharAssembly_KeepAsVariantPreservesEdit`, `Test_CharAssembly_KeepAsVariantOfLoadoutVariant`; `Test` gate step, `ZenithEditor_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Modular.md` → "Keep as variant algorithm"; `Zenith2/Docs/Design/CHAR/Modular.md` → "Hand-edit protection"

### Z2-R-CHAR-139 Generator-owned root components

- **Statement:** An assembly's `rootComponents` holds no component the generator writes, including `Zenith.Physics.Ragdoll` while the assembly names a ragdoll, so no generated root carries two components of one stable name.
- **Covers:** Z2-I-CHAR-05, Z2-I-CHAR-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_CharAssemblyValidation_EachRule` (the three MV12 cases); `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Modular.md` → "Assembly validation rules"; `Zenith2/Docs/Design/CHAR/Modular.md` → "Assembly document"

### Z2-R-CHAR-140 Library rig stored per class

- **Statement:** Each class's library rig is the project document's `classes.*.libraryRig`, which validation checks, the retarget step reads and the workflow panel's retarget state is computed from.
- **Covers:** Z2-I-CHAR-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_CharWorkflowPanel_StepStatesFromDocuments`, `Test_CharValidation_RunsEveryRuleOnce`; `Test` gate step, `ZenithEditor_Tests` and `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Facial.md` → "Character project settings document"; `Zenith2/Docs/Design/CHAR/Workflow.md` → "Workflow panel"

### Z2-R-CHAR-141 Class labels select content-rule rows

- **Statement:** Validation warns about a project class whose label selects no `CHARACTER_CLASS_*` row of the content-rule table (rule CV17).
- **Covers:** Z2-I-CHAR-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** `Test_CharValidation_RunsEveryRuleOnce`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Debugging.md` → "Validation catalogue"; `Zenith2/Docs/Design/CHAR/Debugging.md` → "Content rule measures"

### Z2-R-CHAR-142 Bank default locale agrees with window 0

- **Statement:** Validation warns when a bank line's first locale entry is not the project's first voice locale (rule CV18), so AUDIO's fallback sound and the lip-sync fallback window agree.
- **Covers:** Z2-I-CHAR-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_CharLipSyncSync_ReportsBankDefaultLocale` (`ZenithEditor_Tests`), `Test_CharValidation_RunsEveryRuleOnce` (`ZenithAuthoring_Tests`); `Test` gate step
- **Design:** `Zenith2/Docs/Design/CHAR/Facial.md` → "Sync from bank"; `Zenith2/Docs/Design/CHAR/Debugging.md` → "Validation catalogue"

### Z2-R-CHAR-143 Walkthrough starting content

- **Statement:** CinematicSlice's walkthrough starting content (the two binary sources, the dialogue WAVs, the bank with line 1's two phrase markers, the project document, the scene, its schema and its behaviour) is committed by CINE's `needs-human` content-production ticket, never by a worker.
- **Covers:** Z2-I-CHAR-16
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-012, Z2-D-021
- **Budgets:** Z2-B-CHAR-001
- **Acceptance evidence:** the content-production ticket's committed files under `Zenith2/Games/CinematicSlice/` (`needs-human`), checked against the starting state of the step list
- **Design:** `Zenith2/Docs/Design/CHAR/Workflow.md` → "Walkthrough character workflow"; `Zenith2/Docs/Design/CHAR.md` → "Stage table"

### Z2-R-CHAR-144 Scriptable command arguments

- **Statement:** Every CHAR command and automation step that names an asset, a document or an entity accepts a persistent ID, a path with an optional product key, a hierarchy path or a position reference, resolved before execution.
- **Covers:** Z2-I-CHAR-11, Z2-I-CHAR-12, Z2-I-CHAR-13, Z2-I-CHAR-14, Z2-I-CHAR-15, Z2-I-CHAR-16
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_CharCommands_ArgumentFormsResolve`, `Test_Walkthrough_CharacterWorkflow_Rehearsal`; `Test` gate step, `ZenithEditor_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Workflow.md` → "Command argument forms"; `Zenith2/Docs/Design/CHAR/Workflow.md` → "Walkthrough rehearsal"

### Z2-R-CHAR-145 Member types declared in full

- **Statement:** Every member type and coded-field enumeration of `Zenith_AuthoringCharacter.h` is declared in full with typed members and one contract block.
- **Covers:** Z2-I-CHAR-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-047, Z2-D-034
- **Budgets:** none
- **Acceptance evidence:** `ApiContractCheck` and `HeaderSelfContained` gate steps
- **Design:** `Zenith2/Docs/Design/CHAR/Types.md` → "Scope of this file"; `Zenith2/Docs/Design/CHAR.md` → "Header map"

### Z2-R-CHAR-146 Control set resolution

- **Statement:** Before evaluation a control set is resolved against its skeleton and pose books: every joint and pose target gets its index, an unresolvable name is reported and its control is skipped and counted, and evaluation refuses value and point spans of the wrong length.
- **Covers:** Z2-I-CHAR-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_CharControls_ResolveFillsIndices`, `Test_CharControls_SpanLengthsChecked`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CHAR/Rigging.md` → "Control evaluation"; `Zenith2/Docs/Design/CHAR.md` → "Controls and pose book API"

## Coverage of inventory items

| Item | Text (Inventory v1) | Requirements |
|---|---|---|
| Z2-I-CHAR-01 | rig authoring | Z2-R-CHAR-001, Z2-R-CHAR-023, Z2-R-CHAR-024, Z2-R-CHAR-025, Z2-R-CHAR-026, Z2-R-CHAR-027, Z2-R-CHAR-028, Z2-R-CHAR-029, Z2-R-CHAR-030, Z2-R-CHAR-031, Z2-R-CHAR-032, Z2-R-CHAR-033, Z2-R-CHAR-034, Z2-R-CHAR-038, Z2-R-CHAR-133, Z2-R-CHAR-134 |
| Z2-I-CHAR-02 | control authoring | Z2-R-CHAR-029, Z2-R-CHAR-038, Z2-R-CHAR-039, Z2-R-CHAR-040, Z2-R-CHAR-041, Z2-R-CHAR-042, Z2-R-CHAR-043, Z2-R-CHAR-044, Z2-R-CHAR-045, Z2-R-CHAR-046, Z2-R-CHAR-065, Z2-R-CHAR-123, Z2-R-CHAR-124, Z2-R-CHAR-125, Z2-R-CHAR-126, Z2-R-CHAR-127, Z2-R-CHAR-128, Z2-R-CHAR-130, Z2-R-CHAR-132, Z2-R-CHAR-146 |
| Z2-I-CHAR-03 | pose debugging | Z2-R-CHAR-090, Z2-R-CHAR-091, Z2-R-CHAR-092, Z2-R-CHAR-093, Z2-R-CHAR-094, Z2-R-CHAR-095, Z2-R-CHAR-096, Z2-R-CHAR-129 |
| Z2-I-CHAR-04 | animation update budgeting | Z2-R-CHAR-050, Z2-R-CHAR-097, Z2-R-CHAR-098, Z2-R-CHAR-099, Z2-R-CHAR-100, Z2-R-CHAR-101, Z2-R-CHAR-123, Z2-R-CHAR-128, Z2-R-CHAR-141 (runtime: ANIM's Z2-R-ANIM-138, Z2-R-ANIM-140 … Z2-R-ANIM-146) |
| Z2-I-CHAR-05 | modular characters | Z2-R-CHAR-017, Z2-R-CHAR-072, Z2-R-CHAR-073, Z2-R-CHAR-074, Z2-R-CHAR-075, Z2-R-CHAR-076, Z2-R-CHAR-077, Z2-R-CHAR-078, Z2-R-CHAR-079, Z2-R-CHAR-080, Z2-R-CHAR-081, Z2-R-CHAR-082, Z2-R-CHAR-083, Z2-R-CHAR-084, Z2-R-CHAR-123, Z2-R-CHAR-124, Z2-R-CHAR-125, Z2-R-CHAR-126, Z2-R-CHAR-127, Z2-R-CHAR-128, Z2-R-CHAR-129, Z2-R-CHAR-130, Z2-R-CHAR-132, Z2-R-CHAR-138, Z2-R-CHAR-139 |
| Z2-I-CHAR-06 | animation/physics authority | Z2-R-CHAR-001, Z2-R-CHAR-002, Z2-R-CHAR-003, Z2-R-CHAR-004, Z2-R-CHAR-005, Z2-R-CHAR-006, Z2-R-CHAR-007, Z2-R-CHAR-008, Z2-R-CHAR-009, Z2-R-CHAR-010, Z2-R-CHAR-011, Z2-R-CHAR-012, Z2-R-CHAR-013, Z2-R-CHAR-014, Z2-R-CHAR-015, Z2-R-CHAR-016, Z2-R-CHAR-017, Z2-R-CHAR-018, Z2-R-CHAR-034, Z2-R-CHAR-139, Z2-R-CHAR-145 |
| Z2-I-CHAR-07 | facial animation workflow | Z2-R-CHAR-033, Z2-R-CHAR-041, Z2-R-CHAR-042, Z2-R-CHAR-043, Z2-R-CHAR-047, Z2-R-CHAR-048, Z2-R-CHAR-049, Z2-R-CHAR-113, Z2-R-CHAR-133 |
| Z2-I-CHAR-08 | lip-sync workflow | Z2-R-CHAR-002, Z2-R-CHAR-047, Z2-R-CHAR-050, Z2-R-CHAR-051, Z2-R-CHAR-052, Z2-R-CHAR-053, Z2-R-CHAR-054, Z2-R-CHAR-055, Z2-R-CHAR-056, Z2-R-CHAR-057, Z2-R-CHAR-058, Z2-R-CHAR-059, Z2-R-CHAR-060, Z2-R-CHAR-061, Z2-R-CHAR-062, Z2-R-CHAR-063, Z2-R-CHAR-064, Z2-R-CHAR-065, Z2-R-CHAR-066, Z2-R-CHAR-067, Z2-R-CHAR-068, Z2-R-CHAR-069, Z2-R-CHAR-070, Z2-R-CHAR-071, Z2-R-CHAR-123, Z2-R-CHAR-124, Z2-R-CHAR-125, Z2-R-CHAR-126, Z2-R-CHAR-127, Z2-R-CHAR-129, Z2-R-CHAR-130, Z2-R-CHAR-131, Z2-R-CHAR-132, Z2-R-CHAR-133, Z2-R-CHAR-134, Z2-R-CHAR-135, Z2-R-CHAR-136, Z2-R-CHAR-137, Z2-R-CHAR-142 |
| Z2-I-CHAR-09 | decision on motion matching (D-SEL-MOTIONMATCH) | Z2-R-CHAR-019, Z2-R-CHAR-020, Z2-R-CHAR-021 |
| Z2-I-CHAR-10 | decision on hair alternatives: hair excluded; cards/shells classified | Z2-R-CHAR-085, Z2-R-CHAR-086, Z2-R-CHAR-087, Z2-R-CHAR-088, Z2-R-CHAR-089 |
| Z2-I-CHAR-11 | complete character workflow: import | Z2-R-CHAR-030, Z2-R-CHAR-102, Z2-R-CHAR-103, Z2-R-CHAR-105, Z2-R-CHAR-106, Z2-R-CHAR-107, Z2-R-CHAR-117, Z2-R-CHAR-118, Z2-R-CHAR-120, Z2-R-CHAR-131, Z2-R-CHAR-144 |
| Z2-I-CHAR-12 | complete character workflow: rig/retarget | Z2-R-CHAR-025, Z2-R-CHAR-032, Z2-R-CHAR-035, Z2-R-CHAR-036, Z2-R-CHAR-037, Z2-R-CHAR-102, Z2-R-CHAR-103, Z2-R-CHAR-107, Z2-R-CHAR-108, Z2-R-CHAR-109, Z2-R-CHAR-117, Z2-R-CHAR-118, Z2-R-CHAR-120, Z2-R-CHAR-131, Z2-R-CHAR-140, Z2-R-CHAR-144 |
| Z2-I-CHAR-13 | complete character workflow: author the animation graph | Z2-R-CHAR-021, Z2-R-CHAR-048, Z2-R-CHAR-102, Z2-R-CHAR-103, Z2-R-CHAR-107, Z2-R-CHAR-110, Z2-R-CHAR-111, Z2-R-CHAR-112, Z2-R-CHAR-113, Z2-R-CHAR-117, Z2-R-CHAR-118, Z2-R-CHAR-120, Z2-R-CHAR-131, Z2-R-CHAR-144 |
| Z2-I-CHAR-14 | complete character workflow: preview | Z2-R-CHAR-102, Z2-R-CHAR-103, Z2-R-CHAR-107, Z2-R-CHAR-114, Z2-R-CHAR-115, Z2-R-CHAR-117, Z2-R-CHAR-118, Z2-R-CHAR-120, Z2-R-CHAR-131, Z2-R-CHAR-144 |
| Z2-I-CHAR-15 | complete character workflow: in-game | Z2-R-CHAR-018, Z2-R-CHAR-068, Z2-R-CHAR-075, Z2-R-CHAR-083, Z2-R-CHAR-102, Z2-R-CHAR-103, Z2-R-CHAR-107, Z2-R-CHAR-116, Z2-R-CHAR-117, Z2-R-CHAR-118, Z2-R-CHAR-120, Z2-R-CHAR-131, Z2-R-CHAR-144 |
| Z2-I-CHAR-16 | character workflow usability acceptance as a timed scripted walkthrough | Z2-R-CHAR-022, Z2-R-CHAR-096, Z2-R-CHAR-104, Z2-R-CHAR-119, Z2-R-CHAR-120, Z2-R-CHAR-121, Z2-R-CHAR-122, Z2-R-CHAR-143, Z2-R-CHAR-144 |

## Excluded and deferred

| Requirement or capability | Classification | Decision | Rationale |
|---|---|---|---|
| Z2-R-CHAR-086 Hair shells excluded | excluded | Z2-D-078, Z2-D-043 (dated amendment queued as core-doc item X327) | each shell is a further alpha-tested layer over the same pixels, multiplying the overdraw the early-Z rules keep minimal and the S24 pays in fragment bandwidth; no flagship slice needs fur; strand hair and groom are excluded outright (`Zenith2/Docs/Design/CHAR/Modular.md → "Hair alternatives classification"`) |
| strand-based hair and groom simulation | excluded | Z2-D-043, Z2-D-078 | a specialist programme with no consuming slice |
| a CHAR runtime library, Luau bindings or misuse codes | not provided | Z2-D-081 | every runtime behaviour a character needs is ANIM's, PHYS's, GEOM's, AUDIO's or CINE's; CHAR reserves the misuse block ZS2800–ZS2899 (ruling MISUSE-BLOCKS-2, core-doc item X326) and raises no code |

No CHAR requirement is deferred: every inventory item is required at Stage 4 except Z2-I-CHAR-06 and Z2-I-CHAR-09 (Stage 3); requirements of other stages are the Stage 1 budget producer (Z2-R-CHAR-022), the Stage 5 hair highlight and the Stage 6 migration fixtures. Requirements 133 to 145 were added by design revision 1 and 146 by revision 2. Two requirements are classified research: the motion-matching authoring-cost harness and the D-SEL-MOTIONMATCH ruling, whose proof is the ruling ticket. Game-side obligations the design states (equipment switching, the voice-locale setting, dialogue lip-sync in the flagship samples) are GAME's requirements, requested in cross-area row CH4.
