# GAME requirements

## Purpose

The requirement catalogue of the GAME area (gameplay framework: game and world lifecycle, cameras, reusable gameplay primitives, script integration, the frame-phase contract and lifecycle commands, and the game-side OpenWorldSlice samples). Each requirement is one testable obligation, traced to the design family `Zenith2/Docs/Design/GAME.md` and its split files. Game-side requirements cover the OpenWorldSlice Luau samples of Z2-D-077; their tickets touch only `Zenith2/Games/OpenWorldSlice/Scripts/`.

## Requirements

### Z2-R-GAME-001 ZenithGameplay library declaration

- **Statement:** The `ZenithGameplay` target must be declared by one `zenith_add_library` call with KIND MODULE, MODULE_NAME Gameplay, DEPENDS exactly ZenithRuntime ZenithScene ZenithECS ZenithReflect ZenithCore and SUPPORTED ZENITH_GPU, and must never depend on ZenithJobs, ZenithFlux or any other module.
- **Covers:** Z2-I-GAME-01, Z2-I-GAME-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-024, Z2-D-002
- **Budgets:** none
- **Acceptance evidence:** the Libgraph gate step report for the target; `Test_GameplayModule_RegistersExpectedTypes`
- **Design:** `Zenith2/Docs/Design/GAME.md` → "Target summary"; `Zenith2/Docs/Design/GAME.md` → "Libraries GAME code depends on"

### Z2-R-GAME-002 Module entry points

- **Statement:** The root header `Zenith_Module_Gameplay.h` must declare exactly the two module entry points, and the module must register its types, configuration keys, command kinds, bindings and memory consumer only through the start and attach contexts.
- **Covers:** Z2-I-GAME-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-024, Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_GameplayModule_RegistersExpectedTypes`, `Test_GameplayModule_StartAttach_FaultSweep` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME.md` → "Zenith_Module_Gameplay header"; `Zenith2/Docs/Design/GAME/Framework.md` → "Component and resource registration"

### Z2-R-GAME-003 Start and attach unwind on failure

- **Statement:** Every allocation or registration failure in the module's `Start` or `AttachWorld` must unwind everything that call registered and return the mapped `Zenith_ModuleError`, leaving no leak and no registration.
- **Covers:** Z2-I-GAME-01, Z2-I-GAME-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_GameplayModule_StartAttach_FaultSweep` in the Test gate step (`ZenithGameplay_Tests`) and the Sanitizers gate step
- **Design:** `Zenith2/Docs/Design/GAME.md` → "Construction and shutdown"; `Zenith2/Docs/Design/GAME.md` → "Failure table"

### Z2-R-GAME-004 No game rule names in the engine

- **Statement:** No stable name, command kind, binding, configuration key or source file the engine registers or ships may name a quest, dialogue, inventory, item or interaction concept; those exist only as Luau inside OpenWorldSlice.
- **Covers:** Z2-I-GAME-10, Z2-I-GAME-11, Z2-I-GAME-12, Z2-I-GAME-13, Z2-I-GAME-14
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-077
- **Budgets:** none
- **Acceptance evidence:** `Test_Gameplay_RegistersNoGameNames` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/OpenWorldSlice.md` → "Slice layout and ticket rule"; `Zenith2/Docs/Design/GAME/Tests.md` → "ZenithGameplay unit tests"

### Z2-R-GAME-005 Missing engine types fail the attach

- **Statement:** An attach in a registry lacking `Zenith.Render.Camera` with properties FovYDegrees and CutRequest of the documented types must fail with `GAMEPLAY_ERROR_MISSING_TYPE`, mapped to `MODULE_ERROR_REGISTRATION`, and never assert.
- **Covers:** Z2-I-GAME-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_GameplayModule_MissingRenderCameraFailsAttach` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Cameras.md` → "Declared access"; `Zenith2/Docs/Design/GAME/Framework.md` → "Gameplay limits and enumerations"

### Z2-R-GAME-006 Gameplay memory consumer

- **Statement:** The module must register one simulation-subsystem memory consumer whose resident bytes equal the sum over attached worlds of the request queue, camera scratch and world-state records.
- **Covers:** Z2-I-GAME-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_GameplayModule_ConfigAndCounters` (the consumer's resident bytes for 1 and 3 worlds) in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Framework.md` → "Gameplay memory accounting"

### Z2-R-GAME-007 D-PHASES analysis files

- **Statement:** The Stage 1 analysis ticket must write `Zenith2/Docs/Research/Phases/AuthorityCensus.md` and `RefinementCandidates.md` covering every property group of every signed-off design and every candidate R1–R13 with its normative IDs, assumption sites and contradictions.
- **Covers:** Z2-I-GAME-22
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-080, Z2-D-084, D-PHASES (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** the two research files, reviewed in the D-PHASES ruling ticket
- **Design:** `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Analysis ticket"; `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Census format"; `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Refinement candidates"

### Z2-R-GAME-008 D-PHASES ruling recorded

- **Statement:** The `needs-human` ruling ticket must decide every candidate, append one `Z2-D-NNN D-PHASES refinement` entry to DecisionLog.md and apply each amendment to FramePhases.md or LifecycleCommands.md in the same diff.
- **Covers:** Z2-I-GAME-22
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-080, Z2-D-085, Z2-D-097, D-PHASES (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** the DecisionLog.md entry and its diff; the Coverage gate step over the amended bullets
- **Design:** `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Ruling ticket"; `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Change control"

### Z2-R-GAME-009 Every engine component property has an owning phase

- **Statement:** Every component, resource and event property registered by any engine library or module that a Lifecycle suite links must have exactly one census row, of a linked library, whose owning phase and arbitration equal the frozen registry's; test types (stable names under `Zenith.Test.`) never have rows and are ignored by the comparison.
- **Covers:** Z2-I-GAME-22
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-080, D-PHASES (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** at S1: `Zenith2/Docs/Research/Phases/AuthorityCensus.md` (with its `Stage` column), reviewed in the D-PHASES ruling ticket; from S2: the stage-aware comparison `Zenith_CheckAuthorityCensus` run by `Test_Gameplay_AuthorityCensusMatchesRegistry` in the Test gate step (the Stage 2 suite `Zenith_Integration_Lifecycle`, suite stage 2), `Test_AuthorityCensus_CharacterModules` in the Test gate step (the Stage 3 suite `Zenith_Integration_LifecycleCharacters`, suite stage 3) and `Test_AuthorityCensus_OpenWorldModules` in the Test gate step (the Stage 4 suite `Zenith_Integration_LifecycleOpenWorld`, suite stage 4), each green on the day its suite lands; and the same three tests in every stage acceptance run from Stage 2 on (category `Zenith2Acceptance`, Z2-D-083) with `ZENITH_ACCEPTANCE_STAGE` set to the run's stage, which fails a linked row of that stage or below whose type never registered
- **Design:** `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Census of signed-off designs"; `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Census format"; `Zenith2/Docs/Design/GAME/Tests.md` → "Lifecycle per-command tests"

### Z2-R-GAME-010 Contested fields carry an arbitration rule

- **Statement:** Every property with more than one writer source must carry an arbitration rule in the census and in its registration.
- **Covers:** Z2-I-GAME-22
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-080, D-PHASES (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** at S1: the `Arbitration` column of `Zenith2/Docs/Research/Phases/AuthorityCensus.md`, reviewed in the D-PHASES ruling ticket; from S2: the arbitration comparison (rule 4 of the census format) of `Test_Gameplay_AuthorityCensusMatchesRegistry` in the Test gate step (the Stage 2 suite `Zenith_Integration_Lifecycle`), `Test_AuthorityCensus_CharacterModules` in the Test gate step (the Stage 3 suite `Zenith_Integration_LifecycleCharacters`) and `Test_AuthorityCensus_OpenWorldModules` in the Test gate step (the Stage 4 suite `Zenith_Integration_LifecycleOpenWorld`)
- **Design:** `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Census of signed-off designs"; `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Refinement candidates"

### Z2-R-GAME-011 Census extended by every subsequent design

- **Statement:** Every design signed off after GAME that registers a component, resource or event property must add its census rows, with `Stage` set to the registering ticket's stage, in the same diff.
- **Covers:** Z2-I-GAME-22
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-085, D-PHASES (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** the census test of the suite that links the registering library — `Test_Gameplay_AuthorityCensusMatchesRegistry` (ZenithRuntime and below), `Test_AuthorityCensus_CharacterModules` (+ ZenithGameplay, ZenithPhysics, ZenithAnimation), `Test_AuthorityCensus_OpenWorldModules` (+ ZenithCinematics, ZenithStreaming, ZenithAI) — failing in the Test gate step on a registered property of a linked library without a row; review of the registering diff for a library no Lifecycle suite links
- **Design:** `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Census rows added by subsequent designs"; `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Census format"

### Z2-R-GAME-012 GAME systems placed by the contract

- **Statement:** `Gameplay.WorldEvents` must run in phase 1, `Gameplay.Rules` in phase 7 and `Gameplay.Cameras` in phase 7 after `Gameplay.Rules`, with exactly the declared accesses of the design.
- **Covers:** Z2-I-GAME-22, Z2-I-GAME-01, Z2-I-GAME-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_GameplayModule_SchedulePlacement` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Phase placement of GAME systems"; `Zenith2/Docs/Design/GAME/Framework.md` → "Declared access"; `Zenith2/Docs/Design/GAME/Cameras.md` → "Declared access"

### Z2-R-GAME-013 Event queue capacity row derivation

- **Statement:** The Stage 1 per-system budget ticket for GAME must allocate Z2-B-GAME-003 by the documented power-of-two rule from the W-Dense scale and OpenWorldSlice content-specification figures only (no measurement, so no harness half), as the value of `ecs.maxEventsPerTick`, and must be blocks-linked from "Stage 1 workload scale ticket for W-Dense", "Stage 1 flagship acceptance ticket for OpenWorldSlice" and "Stage 1 workload scale ticket for W-Characters" besides the CORE, JOBS and RPROD rulings.
- **Covers:** Z2-I-GAME-05, Z2-I-GAME-22
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-012, D-PHASES (open, Stage 1)
- **Budgets:** Z2-B-GAME-003
- **Acceptance evidence:** the ruling's allocation table under `Zenith2/Docs/Research/` and the Budgets.md row; the ticket's blocks links and the `Blocked by` cell of `Zenith2/Docs/Budgets.md` → "Producer tickets" (cross-doc item X254)
- **Design:** `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Event queue capacity"; `Zenith2/Docs/Design/GAME.md` → "Rows owned"

### Z2-R-GAME-014 Phase CPU rows allocated

- **Statement:** The same budget ticket must allocate Z2-B-GAME-001 and Z2-B-GAME-002 with their MP-CPUZONE spans of phases 4 and 9 of the primary world, phase 4 holding PHYS's `Physics.CharacterMove` and ANIM's `Animation.ApplyRootMotion` and no GAME node, from published figures without a reference-engine measurement.
- **Covers:** Z2-I-GAME-22
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-GAME-001, Z2-B-GAME-002
- **Acceptance evidence:** the ruling's allocation table and the Budgets.md rows
- **Design:** `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Movement arbitration phase CPU"; `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Event delivery CPU"

### Z2-R-GAME-015 Pending GAME row needs filed

- **Statement:** The budget ticket must allocate the three pending rows "Gameplay framework CPU" (a BR-REPORT sub-row of Z2-B-AI-002, never summed into CPU-SIM twice), "Gameplay request capacity per tick" and "Gameplay framework memory per world".
- **Covers:** Z2-I-GAME-01, Z2-I-GAME-05
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-AI-002
- **Acceptance evidence:** the Budgets.md rows added by the ticket
- **Design:** `Zenith2/Docs/Design/GAME.md` → "Row needs recorded as pending"

### Z2-R-GAME-016 Spawn per-command test

- **Statement:** The per-command test `Test_Lifecycle_Spawn_VisibleToNextPhase` must exist under the name given and assert the command's publication point and first observer as the lifecycle command contract matrix states, on every ECS backend.
- **Covers:** Z2-I-GAME-23, Z2-I-GAME-22
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Lifecycle_Spawn_VisibleToNextPhase` in the Test gate step (`ZenithRuntime_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Lifecycle command contract matrix"; `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Per-command tests"

### Z2-R-GAME-017 Destroy per-command test

- **Statement:** The per-command test `Test_Lifecycle_Destroy_DeferredToTickEnd` must exist under the name given and assert the command's publication point and first observer as the lifecycle command contract matrix states, on every ECS backend.
- **Covers:** Z2-I-GAME-24, Z2-I-GAME-22
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Lifecycle_Destroy_DeferredToTickEnd` in the Test gate step (`ZenithRuntime_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Lifecycle command contract matrix"; `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Per-command tests"

### Z2-R-GAME-018 Teleport per-command test

- **Statement:** The per-command test `Test_Lifecycle_Teleport_ResetsAllHistories` must exist under the name given and assert the command's publication point and first observer as the lifecycle command contract matrix states, on every ECS backend.
- **Covers:** Z2-I-GAME-25, Z2-I-GAME-22
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Lifecycle_Teleport_ResetsAllHistories` in the Test gate step (`ZenithRuntime_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Lifecycle command contract matrix"; `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Per-command tests"

### Z2-R-GAME-019 Ragdoll enter per-command test

- **Statement:** The per-command test `Test_Lifecycle_Ragdoll_EnterAndRecoverBlend` must exist under the name given and assert the command's publication point and first observer as the lifecycle command contract matrix states, on every ECS backend.
- **Covers:** Z2-I-GAME-26, Z2-I-GAME-22
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Lifecycle_Ragdoll_EnterAndRecoverBlend` in the Test gate step (the Stage 3 suite `Zenith_Integration_LifecycleCharacters`)
- **Design:** `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Lifecycle command contract matrix"; `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Per-command tests"

### Z2-R-GAME-020 Ragdoll recover per-command test

- **Statement:** The per-command test `Test_Lifecycle_Ragdoll_EnterAndRecoverBlend` must exist under the name given and assert the command's publication point and first observer as the lifecycle command contract matrix states, on every ECS backend, with the recover blend of the orchestrator's PHYS-RECOVER ruling: the weight keeps its playback value in the first phase 6 after the recover playback and falls by the per-tick step from the next tick.
- **Covers:** Z2-I-GAME-27, Z2-I-GAME-22
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Lifecycle_Ragdoll_EnterAndRecoverBlend` in the Test gate step (the Stage 3 suite `Zenith_Integration_LifecycleCharacters`)
- **Design:** `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Lifecycle command contract matrix"; `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Per-command tests"

### Z2-R-GAME-021 Cinematic start per-command test

- **Statement:** The per-command test `Test_Lifecycle_Cinematic_SkipRestoresGameplayState` must exist under the name given and assert the command's publication point and first observer as the lifecycle command contract matrix states, on every ECS backend.
- **Covers:** Z2-I-GAME-28, Z2-I-GAME-22
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Lifecycle_Cinematic_SkipRestoresGameplayState` in the Test gate step (the Stage 4 suite `Zenith_Integration_LifecycleOpenWorld`)
- **Design:** `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Lifecycle command contract matrix"; `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Per-command tests"

### Z2-R-GAME-022 Cinematic interrupt per-command test

- **Statement:** The per-command test `Test_Lifecycle_Cinematic_InterruptBlendsOut` must exist under the name given and assert the command's publication point and first observer as the lifecycle command contract matrix states, on every ECS backend.
- **Covers:** Z2-I-GAME-29, Z2-I-GAME-22
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Lifecycle_Cinematic_InterruptBlendsOut` in the Test gate step (the Stage 4 suite `Zenith_Integration_LifecycleOpenWorld`)
- **Design:** `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Lifecycle command contract matrix"; `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Per-command tests"

### Z2-R-GAME-023 Cinematic skip restore per-command test

- **Statement:** The per-command test `Test_Lifecycle_Cinematic_SkipRestoresGameplayState` must exist under the name given and assert the command's publication point and first observer as the lifecycle command contract matrix states, on every ECS backend.
- **Covers:** Z2-I-GAME-30, Z2-I-GAME-22
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Lifecycle_Cinematic_SkipRestoresGameplayState` in the Test gate step (the Stage 4 suite `Zenith_Integration_LifecycleOpenWorld`)
- **Design:** `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Lifecycle command contract matrix"; `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Per-command tests"

### Z2-R-GAME-024 World transition per-command test

- **Statement:** The per-command test `Test_Lifecycle_WorldTransition_SwapsAtTickBoundary` must exist under the name given and assert the command's publication point and first observer as the lifecycle command contract matrix states, on every ECS backend.
- **Covers:** Z2-I-GAME-31, Z2-I-GAME-22
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Lifecycle_WorldTransition_SwapsAtTickBoundary` in the Test gate step (`ZenithRuntime_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Lifecycle command contract matrix"; `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Per-command tests"

### Z2-R-GAME-025 Shutdown per-command test

- **Statement:** The per-command test `Test_Lifecycle_Shutdown_ReversePhaseTeardown` must exist under the name given and assert the command's publication point and first observer as the lifecycle command contract matrix states, on every ECS backend.
- **Covers:** Z2-I-GAME-32, Z2-I-GAME-22
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Lifecycle_Shutdown_ReversePhaseTeardown` in the Test gate step (`ZenithRuntime_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Lifecycle command contract matrix"; `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Per-command tests"

### Z2-R-GAME-026 Scenario teleport during ragdoll

- **Statement:** The scenario test `Test_Scenario_TeleportDuringRagdoll` must exist in the suite of its stage and assert every expected outcome its scenario heading lists, on every ECS backend.
- **Covers:** Z2-I-GAME-22
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080, D-PHASES (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_Scenario_TeleportDuringRagdoll` in the Test gate step (the Stage 3 suite `Zenith_Integration_LifecycleCharacters`)
- **Design:** `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Scenario suite"; `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Suite target and fixtures"; `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Scenario TeleportDuringRagdoll"

### Z2-R-GAME-027 Scenario skip cinematic mid-blend

- **Statement:** The scenario test `Test_Scenario_SkipCinematicMidBlend` must exist in the suite of its stage and assert every expected outcome its scenario heading lists, on every ECS backend.
- **Covers:** Z2-I-GAME-22
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080, D-PHASES (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_Scenario_SkipCinematicMidBlend` in the Test gate step (the Stage 4 suite `Zenith_Integration_LifecycleOpenWorld`)
- **Design:** `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Scenario suite"; `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Suite target and fixtures"; `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Scenario SkipCinematicMidBlend"

### Z2-R-GAME-028 Scenario spawn and destroy in one tick

- **Statement:** The scenario test `Test_Scenario_SpawnAndDestroySameTick` must exist in the suite of its stage and assert every expected outcome its scenario heading lists, on every ECS backend.
- **Covers:** Z2-I-GAME-22
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080, D-PHASES (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_Scenario_SpawnAndDestroySameTick` in the Test gate step (the Stage 2 suite `Zenith_Integration_Lifecycle`)
- **Design:** `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Scenario suite"; `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Suite target and fixtures"; `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Scenario SpawnAndDestroySameTick"

### Z2-R-GAME-029 Scenario transition with in-flight results

- **Statement:** The scenario test `Test_Scenario_WorldTransitionWithInFlightAsync` must exist in the suite of its stage, request its transition through the sink in the same tick as its 40 asynchronous requests, and assert every expected outcome its scenario heading lists, including that `world.async_discarded` rises by exactly the old world's results reaching the retired table (cancelled ones included) and that the overflow and never-completed counters stay unchanged, on every ECS backend.
- **Covers:** Z2-I-GAME-22
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080, D-PHASES (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_Scenario_WorldTransitionWithInFlightAsync` in the Test gate step (the Stage 4 suite `Zenith_Integration_LifecycleOpenWorld`)
- **Design:** `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Scenario suite"; `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Suite target and fixtures"; `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Scenario WorldTransitionWithInFlightAsync"

### Z2-R-GAME-030 Scenario save during streaming

- **Statement:** The scenario test `Test_Scenario_SaveAtSnapshotDuringStreaming` must exist in the suite of its stage and assert every expected outcome its scenario heading lists, asserting tick-hash reproduction only for its EXACT case and SAVE's coherence assertions for its COHERENT case, on every ECS backend.
- **Covers:** Z2-I-GAME-22
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080, D-PHASES (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_Scenario_SaveAtSnapshotDuringStreaming` in the Test gate step (the Stage 4 suite `Zenith_Integration_LifecycleOpenWorld`)
- **Design:** `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Scenario suite"; `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Suite target and fixtures"; `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Scenario SaveAtSnapshotDuringStreaming"

### Z2-R-GAME-031 Scenario replay matrix

- **Statement:** Every scenario run recorded with `--record` (except the COHERENT save case) must replay with identical tick hashes on the backend it was recorded on, with 1, 2, 8 and all workers, forced compaction and a second chunk capacity.
- **Covers:** Z2-I-GAME-22
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_Scenario_ReplayMatrix` in the Test gate step (the Stage 2 suite `Zenith_Integration_Lifecycle`), `Test_Scenario_ReplayMatrixCharacters` in the Test gate step (the Stage 3 suite `Zenith_Integration_LifecycleCharacters`), `Test_Scenario_ReplayMatrixOpenWorld` in the Test gate step (the Stage 4 suite `Zenith_Integration_LifecycleOpenWorld`) (doctest suite `z2-stress`)
- **Design:** `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Replay matrix"

### Z2-R-GAME-032 World lifecycle state machine

- **Statement:** Every world must pass only the transitions of the world lifecycle table, and any other transition must be unreachable.
- **Covers:** Z2-I-GAME-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldTransition_StateMachineOrder`, `Test_GameFlow_States` in the Test gate step (`ZenithRuntime_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/WorldHost.md` → "World lifecycle state machine"

### Z2-R-GAME-033 Game flow state machine

- **Statement:** The run must pass BOOTING, RUNNING, TRANSITIONING and SHUTTING_DOWN only as the game flow table states, published as `runtime.game.flow`.
- **Covers:** Z2-I-GAME-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_GameFlow_States` in the Test gate step (`ZenithRuntime_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/WorldHost.md` → "Game flow state machine"

### Z2-R-GAME-034 Isolated world creation

- **Statement:** A transition target world must be created by the step-16 sequence with `m_bIsolated` set, register GAME's per-world state before modules attach, freeze its system registry before any publication, and contribute no node to the running schedule until its swap.
- **Covers:** Z2-I-GAME-02, Z2-I-GAME-31
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080, Z2-D-039
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldTransition_StateMachineOrder` in the Test gate step (`ZenithRuntime_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/WorldHost.md` → "World creation"

### Z2-R-GAME-035 Deterministic world seed derivation

- **Statement:** An isolated world's seed must be `Zenith_Hash64` of the old seed and next session ordinal, and a replay must compare it with the logged WORLD_BEGIN seed.
- **Covers:** Z2-I-GAME-02, Z2-I-GAME-31
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldTransition_ReplaySwapsAtLoggedTick` in the Test gate step (`ZenithRuntime_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/WorldHost.md` → "World creation"; `Zenith2/Docs/Design/GAME/WorldHost.md` → "Replay of a transition"

### Z2-R-GAME-036 World state resource constant within a world

- **Statement:** `Zenith.Runtime.WorldState` must be written only out of tick at world creation and at a swap, so every tick of a world reads identical values.
- **Covers:** Z2-I-GAME-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldHost_WorldStateResourceConstant` in the Test gate step (`ZenithRuntime_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/WorldHost.md` → "World state resource"

### Z2-R-GAME-037 World started event

- **Statement:** `Gameplay.WorldEvents` must emit `Zenith.Gameplay.WorldStarted` exactly once per world, for every origin and carrying it, in the tick whose index equals the resource's FirstTick.
- **Covers:** Z2-I-GAME-02, Z2-I-GAME-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-GAME-003
- **Acceptance evidence:** `Test_GameplayWorldEvents_StartedOnce` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Framework.md` → "The world events system"

### Z2-R-GAME-038 Transition intake refusals

- **Statement:** A direct transition request must return RUNTIME_ERROR_TRANSITION_IN_PROGRESS while another is in flight or pending — completing the world-load ID it allocated with the IN_PROGRESS failure payload when the refusal follows the allocation —, RUNTIME_ERROR_TRANSITION_PEAK_REFUSED on a refused reservation, and start no preparation in either case.
- **Covers:** Z2-I-GAME-31
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** Z2-B-CORE-007
- **Acceptance evidence:** `Test_WorldTransition_InProgressRefused`, `Test_WorldTransition_PeakRefused` in the Test gate step (`ZenithRuntime_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/WorldHost.md` → "Transition request intake"

### Z2-R-GAME-039 Peak reservation before loading

- **Statement:** The world-transition peak must be reserved before preparation starts, sized as documented, and released when the old world is destroyed or the transition ends unsuccessfully.
- **Covers:** Z2-I-GAME-31
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-CORE-007
- **Acceptance evidence:** `Test_WorldTransition_PeakRefused`, `Test_WorldTransition_FaultSweep` in the Test gate step (`ZenithRuntime_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/WorldHost.md` → "Transition record state machine"

### Z2-R-GAME-040 Script transition requests through the sink

- **Statement:** A `Scene.RequestTransition` playback must allocate the old world's completion ID on the merge chain, occupy the world's single sink slot, and be taken at the next prologue; a second request of the same world before then must be dropped and counted.
- **Covers:** Z2-I-GAME-31, Z2-I-GAME-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldTransition_SinkRequestFromScript` in the Test gate step (`ZenithRuntime_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/WorldHost.md` → "Transition request intake"

### Z2-R-GAME-041 Transition record state machine

- **Statement:** A transition must pass the states of the transition record table in order, each event producing exactly the next state and action tabled.
- **Covers:** Z2-I-GAME-31
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldTransition_StateMachineOrder` in the Test gate step (`ZenithRuntime_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/WorldHost.md` → "Transition record state machine"

### Z2-R-GAME-042 Load completion published at a tick boundary

- **Statement:** An activated transition target must be published into the old world as `PUBLICATION_KIND_WORLD_LOADED` carrying the issue ID, at a tick boundary, and every completion ID the host allocates or receives must be completed exactly once, with the failure payload of its code for every refusal or failure after allocation.
- **Covers:** Z2-I-GAME-31
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_Lifecycle_WorldTransition_SwapsAtTickBoundary`, `Test_WorldTransition_PublicationHandlerRows` in the Test gate step (`ZenithRuntime_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/WorldHost.md` → "Publication payloads"

### Z2-R-GAME-043 Swap in prologue step 7

- **Statement:** The swap must run in step 7 of the first prologue after the frame that ran the publishing tick, never in the prologue that completed activation, and the new world's first tick index must be 1.
- **Covers:** Z2-I-GAME-31
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Lifecycle_WorldTransition_SwapsAtTickBoundary` in the Test gate step (`ZenithRuntime_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/WorldHost.md` → "Prologue step 7 procedure"; `Zenith2/Docs/Design/GAME/WorldHost.md` → "Swap procedure"

### Z2-R-GAME-044 Swap is all or nothing

- **Statement:** A trial-compile, capture or activation failure during the swap must destroy the new world, leave the old world live and unchanged, and complete the operation FAILED with SCHEDULE or CARRY_OVER.
- **Covers:** Z2-I-GAME-31
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldTransition_SwapFailureLeavesOldWorld`, `Test_WorldTransition_ScheduleFailureLeavesOldWorld` in the Test gate step (`ZenithRuntime_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/WorldHost.md` → "Swap procedure"

### Z2-R-GAME-045 Transition cancellation

- **Statement:** A cancel before AWAITING_PUBLICATION must release the isolated world, preparation and reservation, publish the cancelled payload and leave the old world's tick hashes unchanged; a subsequent cancel must be ignored and counted.
- **Covers:** Z2-I-GAME-31
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldTransition_CancelBeforePublication`, `Test_WorldTransition_CancelAfterPublicationIgnored` in the Test gate step (`ZenithRuntime_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/WorldHost.md` → "Transition failure and cancellation"

### Z2-R-GAME-046 Paused old world defers its swap

- **Statement:** A load completion addressed to a paused old world must publish only after it resumes, and its swap must wait for that publication.
- **Covers:** Z2-I-GAME-31
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldTransition_PausedOldWorldDefersSwap` in the Test gate step (`ZenithRuntime_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/WorldHost.md` → "Pause arbitration"; `Zenith2/Docs/Design/GAME/WorldHost.md` → "Transition record state machine"

### Z2-R-GAME-047 Transition replay

- **Statement:** A replay must reproduce the swap after the logged tick, complete activation before a logged `WORLD_LOADED` publication reaches the old world, and on any divergence end an in-flight record FAILED (REPLAY_DIVERGED) through the failure application procedure, log it, call `Zenith_Runtime::RecordTestAssertionFailure()` and request shutdown after the frame, so the run exits with code 5; a failure publication for an ID the host never held is never a divergence.
- **Covers:** Z2-I-GAME-31
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldTransition_ReplaySwapsAtLoggedTick`, `Test_WorldTransition_ReplayDivergenceExit5`, `Test_WorldTransition_ReplayIntakeFollowsLog` case (c) in the Test gate step (`ZenithRuntime_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/WorldHost.md` → "Replay of a transition"

### Z2-R-GAME-048 Replay records of a swap

- **Statement:** A recorded swap must write WORLD_SWAP with the old world's last executed tick followed by the new world's WORLD_BEGIN, and a restored world's checkpoint after it.
- **Covers:** Z2-I-GAME-31
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_Lifecycle_WorldTransition_SwapsAtTickBoundary`, `Test_WorldTransition_FromSave` in the Test gate step (`ZenithRuntime_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/WorldHost.md` → "Swap procedure"

### Z2-R-GAME-049 Carry-over root list

- **Statement:** The carry-over roots must be the request's IDs then every enabled `Zenith.Runtime.CarryOver` entity in ascending simulation ID, with duplicates and descendants removed, at most 256, excess and ID-less entities counted.
- **Covers:** Z2-I-GAME-31
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldTransition_CarryOverListAndComponent` in the Test gate step (`ZenithRuntime_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/WorldHost.md` → "Carry-over"

### Z2-R-GAME-050 Carry-over values preserved

- **Statement:** Carried entities must keep their persistent IDs and every serialized field bit for bit in the new world.
- **Covers:** Z2-I-GAME-31
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Lifecycle_WorldTransition_SwapsAtTickBoundary`, `Test_Scenario_WorldTransitionWithInFlightAsync`
- **Design:** `Zenith2/Docs/Design/GAME/WorldHost.md` → "Carry-over"

### Z2-R-GAME-051 Gameplay time continues across a swap

- **Statement:** The new world's continuity tick base must be the old base plus the old world's last executed tick, so every stored gameplay tick keeps its distance from the current tick.
- **Covers:** Z2-I-GAME-31, Z2-I-GAME-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldTransition_GameplayTimeContinues` in the Test gate step (`ZenithRuntime_Tests`), `Test_GameplayTime_ContinuityBase` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Framework.md` → "Gameplay time"; `Zenith2/Docs/Design/GAME/WorldHost.md` → "Swap procedure"

### Z2-R-GAME-052 Results of a swapped-out world discarded

- **Statement:** Results completed but unpublished at a swap, and results completing into a swapped-out world's or a destroyed isolated world's retired table afterwards (including those its producers complete after cancelling them at `DetachWorld` and those a background job parks through a runtime sink after the world was destroyed), must be discarded, never published, and counted once in `world.async_discarded`, which the world host alone publishes at the table's release; the scene and module publication sinks of the world must be retired with its table, so no completion reaches freed memory.
- **Covers:** Z2-I-GAME-31
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldTransition_RetiredResultsDiscarded` (including cases (i) and (ii)) in the Test gate step (`ZenithRuntime_Tests`), case (i) also under the Sanitizers gate step (`win-asan`)
- **Design:** `Zenith2/Docs/Design/GAME/WorldHost.md` → "Retired world asynchronous results"

### Z2-R-GAME-053 Transition from a save

- **Statement:** A transition whose source is a one-world prepared save must restore it into an isolated world within `save.restoreBudgetUs` per prologue and swap like a scene transition; a save of any other world count must fail with SAVE_WORLD_COUNT.
- **Covers:** Z2-I-GAME-31
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldTransition_FromSave` in the Test gate step (`ZenithRuntime_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/WorldHost.md` → "Transition sources"; `Zenith2/Docs/Design/GAME/WorldHost.md` → "Transition record state machine"

### Z2-R-GAME-054 Transition fault sweep

- **Statement:** Every allocation failure during intake, isolated-world creation and swap must unwind with no leak, reservation or tag left.
- **Covers:** Z2-I-GAME-31
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldTransition_FaultSweep` in the Test gate step (`ZenithRuntime_Tests`) and the Sanitizers gate step
- **Design:** `Zenith2/Docs/Design/GAME/WorldHost.md` → "Transition failure and cancellation"

### Z2-R-GAME-055 Pause reasons and application

- **Statement:** A world must be paused exactly while its requested pause mask without reserved bits is non-zero, applied in prologue step 7 and taking effect at its next tick boundary.
- **Covers:** Z2-I-GAME-01, Z2-I-GAME-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldHost_PauseReasons` in the Test gate step (`ZenithRuntime_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/WorldHost.md` → "Pause arbitration"

### Z2-R-GAME-056 Gameplay pause deadline

- **Statement:** A GAMEPLAY pause reason must clear itself at the first prologue past its real-time deadline, which prologue step 7 computes on the main thread from the requested duration (no simulation code reads the clock).
- **Covers:** Z2-I-GAME-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldHost_PauseReasons` in the Test gate step (`ZenithRuntime_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/WorldHost.md` → "Pause arbitration"

### Z2-R-GAME-057 Pause does not change results

- **Statement:** A run with any pause pattern must have the same tick-hash sequence as the same run without pauses.
- **Covers:** Z2-I-GAME-01, Z2-I-GAME-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldHost_PauseDoesNotChangeHashes` in the Test gate step (`ZenithRuntime_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/WorldHost.md` → "Pause arbitration"

### Z2-R-GAME-058 Quit from a script

- **Statement:** `Game.Quit` must end the run after the frame in which its playback ran, with every tick that frame started drained.
- **Covers:** Z2-I-GAME-01, Z2-I-GAME-32
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldHost_QuitFromScript` in the Test gate step (`ZenithRuntime_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/WorldHost.md` → "Game command kinds"; `Zenith2/Docs/Design/GAME/WorldHost.md` → "Shutdown request"

### Z2-R-GAME-059 Script test failure report

- **Statement:** `Game.ReportTestFailure` must log its text at error severity and call `Zenith_Runtime::RecordTestAssertionFailure()`, so the run exits with code 5; GAME keeps no failure count that decides the exit code.
- **Covers:** Z2-I-GAME-01, Z2-I-GAME-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldHost_ReportTestFailureExit5` in the Test gate step (`ZenithRuntime_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/WorldHost.md` → "Game command kinds"; `Zenith2/Docs/Design/GAME/WorldHost.md` → "Game script bindings"

### Z2-R-GAME-060 Game namespace misuse

- **Statement:** Every misuse of a `Game` binding must raise its code ZS4113–ZS4117 and record nothing, and the ticket that registers the namespace must widen GAME's row of `Zenith2/Docs/Scripting/MisuseCatalogue.md` → "Code ranges" to ZS4101–ZS4117.
- **Covers:** Z2-I-GAME-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldHost_GameBindingMisuse` in the Test gate step (`ZenithRuntime_Tests`); the "Code ranges" row in the same diff (review)
- **Design:** `Zenith2/Docs/Design/GAME/WorldHost.md` → "Game script bindings"

### Z2-R-GAME-061 Lifecycle shutdown of each world

- **Statement:** Shutdown must cancel an in-flight transition first, then tear each live world down in descending tag order, logging commands recorded after the final sync point; a live world's asynchronous result table must still exist while its modules' `DetachWorld` runs and is destroyed after them, a table with uncompleted IDs being logged and counted.
- **Covers:** Z2-I-GAME-32
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Lifecycle_Shutdown_ReversePhaseTeardown` in the Test gate step (`ZenithRuntime_Tests`); `Test_WorldTransition_RetiredResultsDiscarded` case (ii) in the Test gate step (`ZenithRuntime_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/WorldHost.md` → "World lifecycle shutdown"

### Z2-R-GAME-062 Teardown order

- **Statement:** Each world must be torn down in the nine steps of `axWORLD_TEARDOWN_ORDER` — the scene world state after the modules' detach, and the asynchronous result table detached at step `SCRIPTS` and destroyed or retired at step `ASYNC_RESULTS`, after `INPUT` and before `SIMULATION` — the same for swapped-out, isolated and shut-down worlds.
- **Covers:** Z2-I-GAME-32, Z2-I-GAME-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080, D-PHASES (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_Lifecycle_Shutdown_ReversePhaseTeardown` in the Test gate step (`ZenithRuntime_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/WorldHost.md` → "Teardown order algorithm"

### Z2-R-GAME-063 Camera components registered

- **Statement:** The rig (144 bytes) and director (120 bytes) components must be registered with the property IDs, defaults and authorities of their tables.
- **Covers:** Z2-I-GAME-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_GameplayModule_RegistersExpectedTypes` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Cameras.md` → "Zenith_GameplayCameraRigComponent"; `Zenith2/Docs/Design/GAME/Cameras.md` → "Zenith_GameplayCameraDirectorComponent"

### Z2-R-GAME-064 Rig selection

- **Statement:** Each director must select its manual rig, else the highest-priority eligible active rig with ties to the lowest simulation ID, and keep its last output when none is eligible; a rig already selected by a lower-simulation-ID director in the same tick is not eligible.
- **Covers:** Z2-I-GAME-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Cameras_RigSelection` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Cameras.md` → "Rig selection"

### Z2-R-GAME-065 Camera follows the possessed pawn

- **Statement:** A rig with FollowPossessedPawn must target its director's player's pawn in the same tick a possession takes effect.
- **Covers:** Z2-I-GAME-03, Z2-I-GAME-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Cameras_FollowPossessedPawn` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Cameras.md` → "Rig selection"; `Zenith2/Docs/Design/GAME/Framework.md` → "Possession algorithm"; `Zenith2/Docs/Design/GAME/Framework.md` → "Rules algorithm"

### Z2-R-GAME-066 Same-tick target pose

- **Statement:** A target's pose evaluated in phase 7 must equal the phase-8 world transform of the same tick bit for bit.
- **Covers:** Z2-I-GAME-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Cameras_TargetPoseFromLocalTransforms` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Cameras.md` → "Target pose evaluation"

### Z2-R-GAME-067 Rig kinds

- **Statement:** Each of the five rig kinds must produce the pivot, rotation and position of the rig kind table.
- **Covers:** Z2-I-GAME-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Cameras_RigKinds` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Cameras.md` → "Rig kind evaluation"

### Z2-R-GAME-068 Look input integration

- **Statement:** Yaw, pitch and arm must integrate their rates per tick with the documented wrap and clamps, treating non-finite rates as 0.
- **Covers:** Z2-I-GAME-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Cameras_LookInputIntegration`, `Test_CameraMath_WrapAndArm` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Cameras.md` → "Look input integration"

### Z2-R-GAME-069 Camera smoothing

- **Statement:** Pivot and rotation smoothing must use α = 1 − exp(−dt/lag), and FIXED and TRACK_ENTITY rigs must never be smoothed.
- **Covers:** Z2-I-GAME-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Cameras_SmoothingByKind`, `Test_CameraMath_LagAlpha` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Cameras.md` → "Smoothing"

### Z2-R-GAME-070 Camera collision

- **Statement:** A colliding follow rig must pull in at once to the sweep hit (at least MinDistance) and recover with the position lag.
- **Covers:** Z2-I-GAME-17
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Cameras_CollisionPullInAndRecover` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Cameras.md` → "Collision resolution"

### Z2-R-GAME-071 Camera without collision provider

- **Statement:** Without `Zenith.CollisionQuery` a rig must keep its arm length and count `gameplay.camera.no_collision_provider` once per world.
- **Covers:** Z2-I-GAME-17
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Cameras_NoCollisionProvider` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Cameras.md` → "Collision query interface"; `Zenith2/Docs/Design/GAME/Cameras.md` → "Collision resolution"

### Z2-R-GAME-072 Collision query interface

- **Statement:** `Zenith_CollisionQuery` must be declared in ZenithScene as specified and implemented per world by the Physics module with the documented determinism rule.
- **Covers:** Z2-I-GAME-17
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Cameras_CollisionPullInAndRecover` in the Test gate step (`ZenithGameplay_Tests`) and PHYS's conformance test of its implementation
- **Design:** `Zenith2/Docs/Design/GAME/Cameras.md` → "Collision query interface"

### Z2-R-GAME-073 Camera blending

- **Statement:** A switch to a rig with a blend must reach the rig's pose after BlendInSeconds with the chosen curve, and a switch mid-blend must start from the blended output.
- **Covers:** Z2-I-GAME-18
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Cameras_BlendBetweenRigs`, `Test_CameraMath_BlendCurves` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Cameras.md` → "Blending"

### Z2-R-GAME-074 Camera cut on teleport

- **Statement:** A teleport of the active rig's target must snap the camera and increment the render camera's CutRequest exactly once.
- **Covers:** Z2-I-GAME-03, Z2-I-GAME-25
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Cameras_SnapAndCutOnTeleport` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Cameras.md` → "Output and cuts"

### Z2-R-GAME-075 Scripted camera cut

- **Statement:** Incrementing a director's CutCounter must produce exactly one cut.
- **Covers:** Z2-I-GAME-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Cameras_ScriptedCut` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Cameras.md` → "Output and cuts"; `Zenith2/Docs/Design/GAME/Cameras.md` → "Camera bindings"

### Z2-R-GAME-076 Camera output visible the same tick

- **Statement:** The output pose and field of view must be written by commands at sync point 7 so phase 8 and the frame's extraction use them.
- **Covers:** Z2-I-GAME-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Cameras_OutputCommandsVisibleInPhase8` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Cameras.md` → "Output and cuts"

### Z2-R-GAME-077 Cinematic camera suspension

- **Statement:** A director bound to a playing cinematic must write nothing until the cinematic ends or is skipped, then resume with a blend from the camera's current pose.
- **Covers:** Z2-I-GAME-03, Z2-I-GAME-28, Z2-I-GAME-30
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Cameras_CinematicSuspension` in the Test gate step (`ZenithGameplay_Tests`), `Test_Lifecycle_Cinematic_SkipRestoresGameplayState`, `Test_Lifecycle_Cinematic_InterruptBlendsOut` in the Test gate step (the Stage 4 suite `Zenith_Integration_LifecycleOpenWorld`)
- **Design:** `Zenith2/Docs/Design/GAME/Cameras.md` → "Cinematic suspension"

### Z2-R-GAME-078 Camera debug view

- **Statement:** The console command `gameplay.camera.debug` must toggle the camera overlay in development builds without changing simulation state.
- **Covers:** Z2-I-GAME-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_GameplayDeterminism_DebugOverlayNoEffect` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Cameras.md` → "Camera debug view"; `Zenith2/Docs/Design/GAME.md` → "Debug views and console commands"

### Z2-R-GAME-079 Tag text and hierarchical matching

- **Statement:** Gameplay tags must be valid dotted names and match queries by exact equality or a dot-separated prefix.
- **Covers:** Z2-I-GAME-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_GameplayTags_HierarchicalMatch`, `Test_GameplayTags_ValidText` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Framework.md` → "Tag component"; `Zenith2/Docs/Design/GAME/Framework.md` → "Tag matching"

### Z2-R-GAME-080 Tag reference counting

- **Statement:** Tag requests must apply the reference-counting table, keep insertion order and emit exactly the tabled events.
- **Covers:** Z2-I-GAME-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_GameplayTags_RefCountAndOrder` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Framework.md` → "Tag reference counting"

### Z2-R-GAME-081 Tag capacity

- **Statement:** A 17th distinct tag must be rejected with `TagRejected`, counted and logged once per entity per tick.
- **Covers:** Z2-I-GAME-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_GameplayTags_SeventeenthRejected` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Framework.md` → "Tag reference counting"

### Z2-R-GAME-082 Gameplay event types

- **Statement:** The seventeen `Zenith.Gameplay.*` event types must be registered with the documented members and emitted only by the systems tabled.
- **Covers:** Z2-I-GAME-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** Z2-B-GAME-003
- **Acceptance evidence:** `Test_GameplayModule_RegistersExpectedTypes` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Framework.md` → "Event types"

### Z2-R-GAME-083 Event order is merge order

- **Statement:** Gameplay events must be emitted per entity buffer with sequences in rules order, so delivery is identical for any worker count and backend.
- **Covers:** Z2-I-GAME-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_GameplayDeterminism_ThreeBackendsEqualHashes` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Framework.md` → "Event types"; `Zenith2/Docs/Design/GAME/Framework.md` → "Rules algorithm"

### Z2-R-GAME-084 Event capacity rejection keeps row writes

- **Statement:** An entity whose gameplay events exceed the merge capacity must lose that tick's events, keep its row writes, and be counted and logged.
- **Covers:** Z2-I-GAME-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** Z2-B-GAME-003
- **Acceptance evidence:** `Test_GameplayRules_EventCapacityRejectKeepsRows` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Framework.md` → "Rules algorithm"

### Z2-R-GAME-085 Generic message event

- **Statement:** `Zenith.Gameplay.Message` must carry channel, instigator, target, magnitude and payload, emitted by `Gameplay.SendMessage` and filterable hierarchically by channel.
- **Covers:** Z2-I-GAME-05, Z2-I-GAME-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-GAME-003
- **Acceptance evidence:** `Scripts/Tests/Messages.test.luau`; `Test_GameplayBindings_Golden` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Framework.md` → "The message event"; `Zenith2/Docs/Design/GAME/Scripting.md` → "Message helpers"

### Z2-R-GAME-086 Request command kinds

- **Statement:** Every change to a rule-owned gameplay property must be one of the ten module command kinds with the documented payloads, recordable in phases 1–10.
- **Covers:** Z2-I-GAME-06, Z2-I-GAME-07, Z2-I-GAME-08, Z2-I-GAME-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080, Z2-D-016
- **Budgets:** none
- **Acceptance evidence:** `Test_GameplayModule_RegistersExpectedTypes` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Framework.md` → "Command kinds and payloads"

### Z2-R-GAME-087 Request queue bounds

- **Statement:** The per-world request queue must hold `gameplay.maxRequestsPerTick` records, drop the newest when full, and report drops with `RequestsDropped` in the next tick.
- **Covers:** Z2-I-GAME-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_GameplayRequests_QueueFullDropsNewest` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Framework.md` → "Queue record format"; `Zenith2/Docs/Design/GAME/Framework.md` → "Playback into the queue"

### Z2-R-GAME-088 Request drain order

- **Statement:** Requests must drain in phase 7 in the order: sync points 7–10 of the previous tick, then 1–6 of this tick, each in merge order.
- **Covers:** Z2-I-GAME-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_GameplayRequests_DrainOrderIsMergeOrder` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Framework.md` → "Playback into the queue"

### Z2-R-GAME-089 Malformed requests never assert

- **Statement:** Malformed, foreign-world, stale or component-less requests must be counted and skipped without an assert, and a request's playback must append only to the queue of the world whose sync point plays it.
- **Covers:** Z2-I-GAME-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_GameplayRequests_MalformedNeverAsserts`, `Test_GameplayRules_StaleAndMissingComponent` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Framework.md` → "Playback into the queue"; `Zenith2/Docs/Design/GAME/Framework.md` → "Rules algorithm"

### Z2-R-GAME-090 Rules algorithm order

- **Statement:** `Gameplay.Rules` must run its nine steps in order once per tick on one node, reading and writing every own-row-only column only on the row of the entity it has made current.
- **Covers:** Z2-I-GAME-06, Z2-I-GAME-07, Z2-I-GAME-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_GameplayRules_RowOwnership`, `Test_GameplayDeterminism_ThreeBackendsEqualHashes` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Framework.md` → "Rules algorithm"; `Zenith2/Docs/Design/GAME/Framework.md` → "Row ownership in the rules node"

### Z2-R-GAME-091 Gameplay time

- **Statement:** Every absolute tick a gameplay component stores must be a gameplay tick, the tick index plus the world's continuity tick base.
- **Covers:** Z2-I-GAME-08, Z2-I-GAME-19
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_GameplayTime_ContinuityBase` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Framework.md` → "Gameplay time"

### Z2-R-GAME-092 Gameplay determinism across backends

- **Statement:** A world exercising every request kind must produce identical tick hashes on the three ECS backends and with 1, 4 and 16 workers.
- **Covers:** Z2-I-GAME-06, Z2-I-GAME-07, Z2-I-GAME-08, Z2-I-GAME-19
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-039
- **Budgets:** none
- **Acceptance evidence:** `Test_GameplayDeterminism_ThreeBackendsEqualHashes` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Framework.md` → "Rules algorithm"

### Z2-R-GAME-093 Gameplay replay

- **Statement:** A recorded gameplay run must replay with identical tick hashes.
- **Covers:** Z2-I-GAME-06, Z2-I-GAME-07, Z2-I-GAME-08, Z2-I-GAME-19
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_GameplayDeterminism_ReplayEqual` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Framework.md` → "Rules algorithm"

### Z2-R-GAME-094 State machine primitive

- **Statement:** State requests must set, reset and remove machines as tabled, reject a fifth machine, and emit one `StateChanged` per effective change.
- **Covers:** Z2-I-GAME-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_GameplayStateMachines_SetResetRemove` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Framework.md` → "State machines"

### Z2-R-GAME-095 State machine Luau helper

- **Statement:** `StateMachine.Define` must validate its spec at load and route inputs through parent states, recording only `SetState` requests.
- **Covers:** Z2-I-GAME-06, Z2-I-GAME-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-016
- **Budgets:** none
- **Acceptance evidence:** `Scripts/Tests/StateMachine.test.luau`
- **Design:** `Zenith2/Docs/Design/GAME/Scripting.md` → "State machine helper"

### Z2-R-GAME-096 Timer primitive

- **Statement:** Timers must start, stop, pause, resume and fire as tabled, in ticks converted from seconds by ceil(seconds × tickHz).
- **Covers:** Z2-I-GAME-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_GameplayTimers_OneShotRepeatPause` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Framework.md` → "Timers"

### Z2-R-GAME-097 Timer fire clamp

- **Statement:** At most 64 fires per timer per tick must be emitted, with the remainder rescheduled and counted.
- **Covers:** Z2-I-GAME-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_GameplayTimers_FireClamp` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Framework.md` → "Timers"

### Z2-R-GAME-098 Action kinds

- **Statement:** Each of the eight action kinds must succeed, fail and time out exactly as the action evaluation table states, a stale, foreign or (for the tag kinds) pending-destroy subject finishing from a preset outcome without a probe, and a `WAIT_ABILITY_END` start must record the ability's activation count in an integer field.
- **Covers:** Z2-I-GAME-19
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_GameplayActions_EachKind`, `Test_GameplayActions_WaitAbilityEndSameDrain` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Framework.md` → "Action evaluation"

### Z2-R-GAME-099 Action names and retention

- **Statement:** Actions must be addressed by unique per-owner names, a duplicate or full start must finish FAILED with ID 0, and a finished record must stay readable for 2 ticks or until acknowledged.
- **Covers:** Z2-I-GAME-19
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_GameplayActions_DuplicateNameAndRetention` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Framework.md` → "Action component"; `Zenith2/Docs/Design/GAME/Framework.md` → "Action retention"

### Z2-R-GAME-100 Async await helper

- **Statement:** `Async.Await` must yield until the named action leaves RUNNING and return its outcome, reading "Expired" for a missing record, with no serialized ticket.
- **Covers:** Z2-I-GAME-19, Z2-I-GAME-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-023
- **Budgets:** none
- **Acceptance evidence:** `Scripts/Tests/Async.test.luau`
- **Design:** `Zenith2/Docs/Design/GAME/Scripting.md` → "Async helpers"

### Z2-R-GAME-101 Player and pawn components

- **Statement:** Player, pawn and owner components must be registered as tabled, with one slot-0 player per world and a second one ignored and logged.
- **Covers:** Z2-I-GAME-15, Z2-I-GAME-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-076
- **Budgets:** none
- **Acceptance evidence:** `Test_GameplayPossession_AutoPossessAndSlotZero` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Framework.md` → "Player and pawn components"

### Z2-R-GAME-102 Possession algorithm

- **Statement:** Possess and unpossess requests must apply steps 1–4 of the possession algorithm through follow-up records, leaving both players and both pawns consistent within the same rules pass, with `PossessionChanged` events delivered in merge order.
- **Covers:** Z2-I-GAME-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_GameplayPossession_Algorithm` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Framework.md` → "Possession algorithm"

### Z2-R-GAME-103 Auto-possession

- **Statement:** At a world's first tick, unless the world was restored from a save, each player without a pawn must possess the lowest-simulation-ID unpossessed auto-possess pawn.
- **Covers:** Z2-I-GAME-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_GameplayPossession_AutoPossessAndSlotZero` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Framework.md` → "Possession algorithm"

### Z2-R-GAME-104 Destroyed pawn unpossessed

- **Statement:** When a possessed pawn is marked destroy-pending, its player's Pawn and its PossessedBy must be null from that sync point on, and the next rules pass must emit one PossessionChanged with a null new pawn.
- **Covers:** Z2-I-GAME-16, Z2-I-GAME-24
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_GameplayPossession_DestroyedPawnUnpossessed` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Framework.md` → "Possession algorithm"

### Z2-R-GAME-105 Owning player resolution

- **Statement:** The owning player of an entity must be resolved through at most 8 owner links, returning null for a chain without a player.
- **Covers:** Z2-I-GAME-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Scripts/Tests/Ownership.test.luau`
- **Design:** `Zenith2/Docs/Design/GAME/Framework.md` → "Ownership"; `Zenith2/Docs/Design/GAME/Scripting.md` → "Read helpers"

### Z2-R-GAME-106 Ability activation

- **Statement:** Ability activation must check the tabled conditions in order, the cost against the attribute's evaluated value at the request, and emit `AbilityActivated` or `AbilityFailed` with the first failing reason.
- **Covers:** Z2-I-GAME-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_GameplayAbilities_ActivationAndFailures` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Abilities.md` → "Ability activation algorithm"

### Z2-R-GAME-107 Ability cooldown and expiry

- **Statement:** Abilities must enter cooldown on start or end per flag, expire after MaxDuration and cancel on a blocked tag when flagged.
- **Covers:** Z2-I-GAME-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_GameplayAbilities_CooldownAndExpiry` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Abilities.md` → "Ability end and expiry"

### Z2-R-GAME-108 Ability spec validation

- **Statement:** An invalid ability spec must be rejected with the first invalid field reported, from C++ and from the Luau builder.
- **Covers:** Z2-I-GAME-07, Z2-I-GAME-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_GameplayAbilities_SpecValidation` in the Test gate step (`ZenithGameplay_Tests`); `Scripts/Tests/Abilities.test.luau`
- **Design:** `Zenith2/Docs/Design/GAME/Abilities.md` → "Ability component"; `Zenith2/Docs/Design/GAME/Scripting.md` → "Effect and ability spec builders"

### Z2-R-GAME-109 Attribute definition and requests

- **Statement:** Attribute requests must define, set, add and range-limit base values as tabled, rejecting non-finite values.
- **Covers:** Z2-I-GAME-20
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_GameplayAttributes_Requests` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Abilities.md` → "Attribute component"; `Zenith2/Docs/Design/GAME/Abilities.md` → "Attribute requests"

### Z2-R-GAME-110 Attribute evaluation

- **Statement:** Current values must be evaluated from base and active modifiers in the documented order with clamping, emitting `AttributeChanged` only on bit changes.
- **Covers:** Z2-I-GAME-20, Z2-I-GAME-21
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_GameplayAttributes_EvaluationOrder` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Abilities.md` → "Current value evaluation"

### Z2-R-GAME-111 Effect application and stacking

- **Statement:** Effects must apply, stack, refresh or reject per policy, with rejection reasons 1–7.
- **Covers:** Z2-I-GAME-21
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_GameplayEffects_ApplyStackExpire` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Abilities.md` → "Effect application algorithm"; `Zenith2/Docs/Design/GAME/Abilities.md` → "Effect stacking"

### Z2-R-GAME-112 Effect expiry and periods

- **Statement:** Effects must expire at their end tick, execute periods, and be removed with their destroyed source when flagged.
- **Covers:** Z2-I-GAME-21
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_GameplayEffects_ApplyStackExpire` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Abilities.md` → "Effect expiry and periods"

### Z2-R-GAME-113 Effect spec validation

- **Statement:** `Zenith_GameplayValidateEffectSpec` must return the number of the first invalid field, and the Luau builder must fail with the field name.
- **Covers:** Z2-I-GAME-21, Z2-I-GAME-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_GameplayEffects_SpecValidation` in the Test gate step (`ZenithGameplay_Tests`); `Scripts/Tests/Effects.test.luau`
- **Design:** `Zenith2/Docs/Design/GAME/Abilities.md` → "Effect specification"; `Zenith2/Docs/Design/GAME/Scripting.md` → "Effect and ability spec builders"

### Z2-R-GAME-114 Gameplay binding namespace

- **Statement:** The `Gameplay` binding namespace must register exactly the functions of the binding table with the documented effects, side-effect masks and contexts.
- **Covers:** Z2-I-GAME-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-016
- **Budgets:** none
- **Acceptance evidence:** `Test_GameplayBindings_Golden` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Scripting.md` → "Binding registration"; `Zenith2/Docs/Design/GAME/Scripting.md` → "Binding functions"

### Z2-R-GAME-115 Gameplay binding misuse codes

- **Statement:** Each misuse of a `Gameplay` binding must raise its catalogued code ZS4101–ZS4112 and never assert.
- **Covers:** Z2-I-GAME-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_GameplayBindings_Golden` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Scripting.md` → "Misuse errors"

### Z2-R-GAME-116 Gameplay Luau library

- **Statement:** The `@gameplay` library modules must be strict, frozen, stateless and pass the chunk-system stateless analysis.
- **Covers:** Z2-I-GAME-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-016
- **Budgets:** none
- **Acceptance evidence:** the Luau library tests of `Zenith2/Docs/Design/GAME/Tests.md` and the cook's stateless report
- **Design:** `Zenith2/Docs/Design/GAME/Scripting.md` → "Library layout"

### Z2-R-GAME-117 Read helpers

- **Statement:** The tag, attribute and ownership read helpers must interpret declared views without any binding call.
- **Covers:** Z2-I-GAME-04, Z2-I-GAME-09, Z2-I-GAME-15, Z2-I-GAME-20
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Scripts/Tests/Tags.test.luau`, `Attributes.test.luau`, `Ownership.test.luau`
- **Design:** `Zenith2/Docs/Design/GAME/Scripting.md` → "Read helpers"

### Z2-R-GAME-118 Camera Luau helpers

- **Statement:** The `@gameplay/Cameras` helpers must record only SetProperty commands on rig and director properties.
- **Covers:** Z2-I-GAME-03, Z2-I-GAME-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Scripts/Tests/Cameras.test.luau` (SCRIPT's Luau test runner); `Test_Cameras_ScriptedCut` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Cameras.md` → "Camera bindings"

### Z2-R-GAME-119 Interaction sample

- **Statement:** OpenWorldSlice must implement interaction as the Luau behaviours Interactable and Interactor using GAME messages, touching only the slice's Scripts directory.
- **Covers:** Z2-I-GAME-11
- **Stage:** S4
- **Classification:** game-side
- **Decisions:** Z2-D-077, Z2-D-002
- **Budgets:** none
- **Acceptance evidence:** `Scripts/Tests/Interaction.test.luau` in the leaf ticket "OpenWorldSlice interaction sample"; integration evidence `Game_OpenWorldSlice_InteractionFlow` (the sample integration ticket)
- **Design:** `Zenith2/Docs/Design/GAME/OpenWorldSlice.md` → "Interaction"; `Zenith2/Docs/Design/GAME/OpenWorldSlice.md` → "Slice layout and ticket rule"; `Zenith2/Docs/Design/GAME/OpenWorldSlice.md` → "Sample tests"

### Z2-R-GAME-120 Inventory sample

- **Statement:** OpenWorldSlice must implement inventory as Luau behaviour state with stack caps, all-or-nothing removal and consumables as GAME effects, touching only the slice's Scripts directory.
- **Covers:** Z2-I-GAME-14
- **Stage:** S4
- **Classification:** game-side
- **Decisions:** Z2-D-077, Z2-D-002
- **Budgets:** none
- **Acceptance evidence:** `Scripts/Tests/Inventory.test.luau` in the leaf ticket "OpenWorldSlice inventory sample"; integration evidence `Game_OpenWorldSlice_InteractionFlow` (the sample integration ticket)
- **Design:** `Zenith2/Docs/Design/GAME/OpenWorldSlice.md` → "Inventory"; `Zenith2/Docs/Design/GAME/OpenWorldSlice.md` → "Slice layout and ticket rule"; `Zenith2/Docs/Design/GAME/OpenWorldSlice.md` → "Sample tests"

### Z2-R-GAME-121 Quest sample

- **Statement:** OpenWorldSlice must implement quests as a Luau quest log over data tables, using GAME tags for quest flags, touching only the slice's Scripts directory.
- **Covers:** Z2-I-GAME-12
- **Stage:** S4
- **Classification:** game-side
- **Decisions:** Z2-D-077, Z2-D-002
- **Budgets:** none
- **Acceptance evidence:** `Scripts/Tests/QuestLog.test.luau` in the leaf ticket "OpenWorldSlice quest sample"; integration evidence `Game_OpenWorldSlice_QuestFlow` (the sample integration ticket)
- **Design:** `Zenith2/Docs/Design/GAME/OpenWorldSlice.md` → "Quest"; `Zenith2/Docs/Design/GAME/OpenWorldSlice.md` → "Slice layout and ticket rule"; `Zenith2/Docs/Design/GAME/OpenWorldSlice.md` → "Sample tests"

### Z2-R-GAME-122 Dialogue sample

- **Statement:** OpenWorldSlice must implement dialogue as a Luau runner over data tables using GAME state machines, timers and input contexts, restartable at the saved node.
- **Covers:** Z2-I-GAME-13
- **Stage:** S4
- **Classification:** game-side
- **Decisions:** Z2-D-077, Z2-D-002, Z2-D-023
- **Budgets:** none
- **Acceptance evidence:** `Scripts/Tests/DialogueRunner.test.luau` in the leaf ticket "OpenWorldSlice dialogue sample"; integration evidence `Game_OpenWorldSlice_DialogueFlowA` and `Game_OpenWorldSlice_DialogueFlowB` (the sample integration ticket)
- **Design:** `Zenith2/Docs/Design/GAME/OpenWorldSlice.md` → "Dialogue"; `Zenith2/Docs/Design/GAME/OpenWorldSlice.md` → "Slice layout and ticket rule"; `Zenith2/Docs/Design/GAME/OpenWorldSlice.md` → "Sample tests"

### Z2-R-GAME-123 Sample data tables

- **Statement:** The slice's item, quest and dialogue tables must have the documented shapes with every cross-reference resolved.
- **Covers:** Z2-I-GAME-11, Z2-I-GAME-12, Z2-I-GAME-13, Z2-I-GAME-14
- **Stage:** S4
- **Classification:** game-side
- **Decisions:** Z2-D-077
- **Budgets:** none
- **Acceptance evidence:** `Scripts/Tests/DataTables.test.luau` in the leaf ticket "OpenWorldSlice dialogue sample"
- **Design:** `Zenith2/Docs/Design/GAME/OpenWorldSlice.md` → "Sample data tables"

### Z2-R-GAME-124 Samples survive save and transition

- **Statement:** The samples' state must survive a save and load and a world transition unchanged; the requirement is the sample integration ticket's, not a game-side leaf's.
- **Covers:** Z2-I-GAME-10, Z2-I-GAME-12, Z2-I-GAME-14
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-077
- **Budgets:** none
- **Acceptance evidence:** `Game_OpenWorldSlice_SaveLoadQuest`, `Game_OpenWorldSlice_TransitionCarry`, both registered by the ticket "OpenWorldSlice sample integration" and run in the Test gate step
- **Design:** `Zenith2/Docs/Design/GAME/OpenWorldSlice.md` → "Sample tests"; `Zenith2/Docs/Design/GAME/OpenWorldSlice.md` → "Slice layout and ticket rule"

### Z2-R-GAME-125 Samples use only framework APIs

- **Statement:** The samples must use only `@zenith`, `@types`, `@gameplay` and other modules' bindings, so the framework carries no rule of one game; the requirement is the sample integration ticket's, which cooks the slice.
- **Covers:** Z2-I-GAME-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-077
- **Budgets:** none
- **Acceptance evidence:** the slice's cook in the ticket "OpenWorldSlice sample integration" (unresolvable requires fail it) and `Test_Gameplay_RegistersNoGameNames` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/OpenWorldSlice.md` → "Gameplay sample rules"; `Zenith2/Docs/Design/GAME/OpenWorldSlice.md` → "Slice layout and ticket rule"

### Z2-R-GAME-126 Sample integration ticket

- **Statement:** The ticket "OpenWorldSlice sample integration" (epic GAME, Stage 4) must place the samples in the slice's scenes, add the streaming sources, the Stage 4 asserts and every sample gametest with its hand-written input script, owning everything of the slice outside the samples' Scripts directories.
- **Covers:** Z2-I-GAME-11, Z2-I-GAME-12, Z2-I-GAME-13, Z2-I-GAME-14
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-077, Z2-D-021
- **Budgets:** none
- **Acceptance evidence:** the gametests of the sample tests table (`Game_OpenWorldSlice_InteractionFlow`, `_QuestFlow`, `_DialogueFlowA`, `_DialogueFlowB`, `_SaveLoadQuest`, `_TransitionCarry`, `_PlaythroughS01`) registered by that ticket and run in the Test gate step
- **Design:** `Zenith2/Docs/Design/GAME/OpenWorldSlice.md` → "Slice layout and ticket rule"; `Zenith2/Docs/Design/GAME/OpenWorldSlice.md` → "Sample tests"

### Z2-R-GAME-127 OpenWorldSlice acceptance package defined

- **Statement:** The Stage 1 flagship acceptance ticket for OpenWorldSlice must write Playthrough.md, FeatureChecklist.md, Budgets.md, BugBar.md and SignOff.md in the documented formats.
- **Covers:** Z2-I-GAME-10
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-021, Z2-D-012
- **Budgets:** Z2-B-SHIP-019, Z2-B-SHIP-020, Z2-B-SHIP-023
- **Acceptance evidence:** the five files under `Zenith2/Games/OpenWorldSlice/Acceptance/`, reviewed by the OpenWorldSlice acceptance ticket (category `Zenith2Acceptance`)
- **Design:** `Zenith2/Docs/Design/GAME/OpenWorldSlice.md` → "Acceptance package"

### Z2-R-GAME-128 Playthrough replay as text

- **Statement:** `Playthrough.replay` must be a `zenith2.input-script` version 1 JSON text file replayable with `--input-script`, recorded only by the `needs-human` recording tickets and never written by a worker ticket.
- **Covers:** Z2-I-GAME-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-021, Z2-D-019
- **Budgets:** Z2-B-SHIP-023
- **Acceptance evidence:** the diff of the `needs-human` ticket "OpenWorldSlice playthrough recording S01–S04" (Z2T-GEOM-015 shape), replayed by the Stage 4 acceptance run (category `Zenith2Acceptance`)
- **Design:** `Zenith2/Docs/Design/GAME/OpenWorldSlice.md` → "Acceptance package"; `Zenith2/Docs/Design/GAME/OpenWorldSlice.md` → "Slice layout and ticket rule"

### Z2-R-GAME-129 First playable playthrough

- **Statement:** Segments S01–S04 must play back from the replay with every segment assertion passing on Windows and the S24.
- **Covers:** Z2-I-GAME-10, Z2-I-GAME-11, Z2-I-GAME-12, Z2-I-GAME-13, Z2-I-GAME-14
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-021, Z2-D-083
- **Budgets:** Z2-B-SHIP-019
- **Acceptance evidence:** the run records of the Stage 4 acceptance ticket (category `Zenith2Acceptance`, a human run on Windows and the S24 playing the S01–S04 replay)
- **Design:** `Zenith2/Docs/Design/GAME/OpenWorldSlice.md` → "Acceptance package"

### Z2-R-GAME-130 Full playthrough

- **Statement:** All eight segments, 20–30 minutes in total, must play back with every assertion passing and the slice's budget rows met.
- **Covers:** Z2-I-GAME-10
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-021, Z2-D-012, Z2-D-083
- **Budgets:** Z2-B-SHIP-019, Z2-B-SHIP-020, Z2-B-SHIP-023
- **Acceptance evidence:** the integration evidence of "OpenWorldSlice playthrough integration": the gametests `Game_OpenWorldSlice_PlaythroughS05`, `_PlaythroughS06`, `_PlaythroughS07` and `_PlaythroughS08` with their hand-written input scripts in the Test gate step; the full replay of the `needs-human` ticket "OpenWorldSlice full playthrough recording", the run records of the Stage 6 acceptance ticket (category `Zenith2Acceptance`) and the SignOff.md verdicts of the `needs-human` sign-off tickets
- **Design:** `Zenith2/Docs/Design/GAME/OpenWorldSlice.md` → "Acceptance package"; `Zenith2/Docs/Design/GAME/OpenWorldSlice.md` → "Sample tests"

### Z2-R-GAME-131 Playthrough smoke in CI

- **Statement:** Segment S01 must run headless as `Game_OpenWorldSlice_PlaythroughS01` in CI from a hand-written input script, independent of any recording.
- **Covers:** Z2-I-GAME-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-021
- **Budgets:** none
- **Acceptance evidence:** `Game_OpenWorldSlice_PlaythroughS01` in the Test gate step
- **Design:** `Zenith2/Docs/Design/GAME/OpenWorldSlice.md` → "Acceptance package"

### Z2-R-GAME-132 Feature checklist coverage

- **Statement:** Every GAME requirement assigned to OpenWorldSlice must appear in at least one FeatureChecklist item.
- **Covers:** Z2-I-GAME-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-021
- **Budgets:** none
- **Acceptance evidence:** FeatureChecklist.md reviewed against the catalogue's coverage table
- **Design:** `Zenith2/Docs/Design/GAME/OpenWorldSlice.md` → "Acceptance package"

### Z2-R-GAME-133 Gameplay counters

- **Statement:** The module must publish every counter of the gameplay counter table with its kind.
- **Covers:** Z2-I-GAME-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_GameplayModule_ConfigAndCounters` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME.md` → "Counters"

### Z2-R-GAME-134 World host counters and logs

- **Statement:** The world host must publish the world counters and log every transition, refusal and divergence with the quoted texts.
- **Covers:** Z2-I-GAME-02, Z2-I-GAME-31
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldHost_CountersAndLogs`, `Test_WorldTransition_RetiredResultsDiscarded` in the Test gate step (`ZenithRuntime_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/WorldHost.md` → "World host counters and logs"

### Z2-R-GAME-135 Configuration keys

- **Statement:** The keys `gameplay.maxRequestsPerTick` and `world.transitionReserveFloorBytes` must be registered with the documented ranges and defaults, and out-of-range values must fail the start with INVALID_CONFIG.
- **Covers:** Z2-I-GAME-01, Z2-I-GAME-31
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_GameplayModule_ConfigAndCounters` in the Test gate step (`ZenithGameplay_Tests`) (`gameplay.maxRequestsPerTick`); `Test_WorldTransition_PeakRefused` in the Test gate step (`ZenithRuntime_Tests`) (`world.transitionReserveFloorBytes`)
- **Design:** `Zenith2/Docs/Design/GAME.md` → "Configuration keys"

### Z2-R-GAME-136 Failure table behaviour

- **Statement:** Every failure of the GAME failure table must produce its documented behaviour and never a crash.
- **Covers:** Z2-I-GAME-01, Z2-I-GAME-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** the tests named in each row of the failure table
- **Design:** `Zenith2/Docs/Design/GAME.md` → "Failure table"

### Z2-R-GAME-137 Player slots and ownership under replication

- **Statement:** Player slots above 0 and owner-to-network-authority mapping must be implemented with NET's replication.
- **Covers:** Z2-I-GAME-15, Z2-I-GAME-16
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** NET's Stage 7 replication suite
- **Design:** `Zenith2/Docs/Design/GAME/Framework.md` → "Player and pawn components"; `Zenith2/Docs/Design/GAME.md` → "Explicit exclusions"

### Z2-R-GAME-138 XR-ready camera output

- **Statement:** The camera director must output a stereo-capable pose pair for OpenXR multi-view.
- **Covers:** Z2-I-GAME-03
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** the Stage 7 XR acceptance run
- **Design:** `Zenith2/Docs/Design/GAME/Cameras.md` → "Camera model"; `Zenith2/Docs/Design/GAME.md` → "Explicit exclusions"

### Z2-R-GAME-139 New world schedule validated before the swap

- **Statement:** An isolated world must freeze its system registry at creation and the swap must compile the post-swap frame graph before any state of the old world changes; either failure must end the transition FAILED with SCHEDULE and leave the old world live and unchanged.
- **Covers:** Z2-I-GAME-31
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldTransition_ScheduleFailureLeavesOldWorld`, `Test_WorldTransition_SwapCompilesOnce` in the Test gate step (`ZenithRuntime_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/WorldHost.md` → "World creation"; `Zenith2/Docs/Design/GAME/WorldHost.md` → "Swap procedure"

### Z2-R-GAME-140 Replay intake follows the log

- **Statement:** In a replay a transition intake must take its outcome from the recorded publication of its completion ID, never from the live memory governor, operation table, asset registry or record state: intake-time codes end the intake with the recorded code, running codes and a missing publication proceed live (a save source ends at intake on any recorded failure), a recorded taking that meets a record the recording had already ended first ends it with its recorded code, a live failure ahead of the recorded one keeps the record busy until its recorded ending, and the host must call no Complete for ASYNC_SOURCE_WORLD_LOAD.
- **Covers:** Z2-I-GAME-31
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053
- **Budgets:** Z2-B-CORE-007
- **Acceptance evidence:** `Test_WorldTransition_ReplayIntakeFollowsLog` (cases (a)–(i), the two-world case (i) included) in the Test gate step (`ZenithRuntime_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/WorldHost.md` → "Intake in a replay"; `Zenith2/Docs/Design/GAME/WorldHost.md` → "Replay of a transition"

### Z2-R-GAME-141 Rules node row ownership

- **Statement:** `Gameplay.Rules` and `Gameplay.Cameras` must declare no ReadAnyRow of a property any phase-7 node writes, and must read and write own-row-only columns only on the row of the entity being processed, carrying cross-entity facts through follow-up records and node-arena lists.
- **Covers:** Z2-I-GAME-06, Z2-I-GAME-03, Z2-I-GAME-16, Z2-I-GAME-22
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_GameplayRules_RowOwnership`, `Test_GameplayModule_SchedulePlacement`, `Test_Cameras_FollowPossessedPawn` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Framework.md` → "Row ownership in the rules node"; `Zenith2/Docs/Design/GAME/Cameras.md` → "Declared access"; `Zenith2/Docs/Design/GAME/Cameras.md` → "Director algorithm"

### Z2-R-GAME-142 Changed attribute rows re-evaluated

- **Statement:** The rules system must re-evaluate every attribute row its own steps marked dirty and every row whose row change version is newer than its previous read version, emitting AttributeChanged only on bit changes.
- **Covers:** Z2-I-GAME-20
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_GameplayAttributes_ReevaluatesChangedRows` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Framework.md` → "Rules algorithm"; `Zenith2/Docs/Design/GAME/Abilities.md` → "Attribute component"

### Z2-R-GAME-143 Continuity tick base saved with the world

- **Statement:** `Zenith.Runtime.WorldState` must register ContinuityTickBase serialized and its other properties transient, and a save-source swap must keep the restored base.
- **Covers:** Z2-I-GAME-02, Z2-I-GAME-31
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldTransition_FromSave` in the Test gate step (`ZenithRuntime_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/WorldHost.md` → "World state resource"; `Zenith2/Docs/Design/GAME/WorldHost.md` → "Swap procedure"

### Z2-R-GAME-144 Camera follow target from the rules pass

- **Statement:** `Gameplay.Rules` must write each camera director's FollowPawn from the possession of the player that names it, in the same pass that changes the possession, and count a director named by two players.
- **Covers:** Z2-I-GAME-03, Z2-I-GAME-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Cameras_FollowPossessedPawn`, `Test_GameplayPossession_DestroyedPawnUnpossessed` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Framework.md` → "Rules algorithm"; `Zenith2/Docs/Design/GAME/Cameras.md` → "Rig selection"

### Z2-R-GAME-145 Cinematic interrupt blends out

- **Statement:** A cinematic interrupt must blend its writer source out from the weight it has at the interrupt, restore the marked properties at the tick boundary after the weight reaches zero, and leave the bound camera director suspended until the cinematic ends.
- **Covers:** Z2-I-GAME-29, Z2-I-GAME-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Lifecycle_Cinematic_InterruptBlendsOut` in the Test gate step (the Stage 4 suite `Zenith_Integration_LifecycleOpenWorld`); `Test_Cameras_CinematicSuspension` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Per-command tests"; `Zenith2/Docs/Design/GAME/Cameras.md` → "Cinematic suspension"

### Z2-R-GAME-146 World-load publications keyed by world and completion ID

- **Statement:** Each world's publication handler must match an `ASYNC_SOURCE_WORLD_LOAD` publication against the transition record by the pair (world tag, completion ID) and apply exactly the row of the handler table for the run mode, ignoring in both modes a failure publication for an ID the host never held.
- **Covers:** Z2-I-GAME-31
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldTransition_PublicationHandlerRows` in the Test gate step (`ZenithRuntime_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/WorldHost.md` → "Publication payloads"; `Zenith2/Docs/Design/GAME/WorldHost.md` → "World creation"

### Z2-R-GAME-147 One failure procedure releases a transition in order

- **Statement:** Every transition failure or cancellation before the swap must end through the failure application procedure, which stops the preparation, the activation or restore, destroys the isolated world, releases the inputs, the reservation and the operation and publishes the failure, in that order, outside a replay.
- **Covers:** Z2-I-GAME-31
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045, Z2-D-019
- **Budgets:** Z2-B-CORE-007
- **Acceptance evidence:** `Test_WorldTransition_CancelBeforePublication`, `Test_WorldTransition_FaultSweep` in the Test gate step (`ZenithRuntime_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/WorldHost.md` → "Failure application procedure"; `Zenith2/Docs/Design/GAME/WorldHost.md` → "Transition record state machine"

### Z2-R-GAME-148 Recorded failures and in-flight recordings replay without divergence

- **Statement:** A replay must end a record at the tick of its recorded failure through the failure application procedure without calling Complete, follow a recorded refusal of the direct call, the sink or the hand-off without touching the record, end a save-source intake at once on a recorded failure of any code, and replay a recording that ended with a transition in flight to the log's end, none of these being a divergence.
- **Covers:** Z2-I-GAME-31
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldTransition_ReplayIntakeFollowsLog` cases (d) to (i) in the Test gate step (`ZenithRuntime_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/WorldHost.md` → "Intake in a replay"; `Zenith2/Docs/Design/GAME/WorldHost.md` → "Replay of a transition"; `Zenith2/Docs/Design/GAME/WorldHost.md` → "Prologue step 7 procedure"

### Z2-R-GAME-149 A failed save load leaves the requesting world unchanged

- **Statement:** A script `Save.Load` that fails before the hand-off, at the hand-off or after it must leave the requesting world's tick hashes unchanged, complete the world-load ID exactly once with its failure code (none being allocated when SAVE's intake cannot allocate it), release the restore job, the isolated world, the prepared save and the reservation in that order, and write no swap record; its replay — through `Zenith_WorldHost_BeginTransitionFromSave` with a null save when the log holds no restored session — must create no isolated world and no restore job and exit with code 0.
- **Covers:** Z2-I-GAME-31
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053
- **Budgets:** Z2-B-CORE-007
- **Acceptance evidence:** `Test_WorldTransition_FailedSaveLoadLeavesWorld` (cases (a)–(e)) in the Test gate step (`ZenithRuntime_Tests`); `Test_WorldTransition_ReplayIntakeFollowsLog` cases (c), (g) and (h)
- **Design:** `Zenith2/Docs/Design/GAME/WorldHost.md` → "Failed save loads"; `Zenith2/Docs/Design/GAME/WorldHost.md` → "Failure application procedure"

### Z2-R-GAME-150 Retired tables are never released early and never stop the run

- **Statement:** A retired result table must be released, with the scene and module publication sinks retired with it, only when its in-flight count is 0 or at shutdown; a fifth or subsequent held table must go to the overflow list with its warning and counter instead of stopping the run; and a table released at shutdown, or a live world's table destroyed at shutdown, with uncompleted IDs must be logged and counted.
- **Covers:** Z2-I-GAME-31
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldTransition_RetiredResultsDiscarded` in the Test gate step (`ZenithRuntime_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/WorldHost.md` → "Retired world asynchronous results"

### Z2-R-GAME-151 Census comparison is stage-aware and excludes test types

- **Statement:** Each census test must skip rows of unlinked libraries and rows whose type is not registered and whose Stage is neither below the suite stage nor, in a stage acceptance run, at or below the acceptance stage, fail on any other unregistered row, and ignore every `Zenith.Test.` type in both directions, so that it passes on the day its suite lands and a row of any stage whose type never registers is reported by the acceptance run of its stage.
- **Covers:** Z2-I-GAME-22
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080, D-PHASES (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_Gameplay_AuthorityCensusMatchesRegistry` (with its three SUBCASEs over inline census texts, the third run with acceptance stages 0 and 5) in the Test gate step (the Stage 2 suite `Zenith_Integration_Lifecycle`); `Test_AuthorityCensus_CharacterModules` and `Test_AuthorityCensus_OpenWorldModules` in the Test gate step (the Stage 3 and Stage 4 suites); the three tests in each stage acceptance run with `ZENITH_ACCEPTANCE_STAGE` set (`Zenith2Acceptance`)
- **Design:** `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Census format"; `Zenith2/Docs/Design/GAME/Tests.md` → "Lifecycle per-command tests"

### Z2-R-GAME-152 Same-drain attribute reads use the evaluated value

- **Statement:** The ability cost check and `WAIT_ATTRIBUTE` must read the attribute value `Zenith_GameplayEvaluateAttribute` returns over the row as it stands, never the stored Current, so a second activation in one drain is checked against the base the first lowered.
- **Covers:** Z2-I-GAME-07, Z2-I-GAME-19, Z2-I-GAME-20
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_GameplayAbilities_ActivationAndFailures` (same-drain cost case), `Test_GameplayActions_EachKind` in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Abilities.md` → "Ability activation algorithm"; `Zenith2/Docs/Design/GAME/Framework.md` → "Action evaluation"

### Z2-R-GAME-153 One selecting director per rig per tick

- **Statement:** Directors must select rigs in ascending simulation ID, a rig taken by a lower-ID director being ineligible for the directors that follow, which count `gameplay.camera.rig_contended` and fall back to their next eligible rig.
- **Covers:** Z2-I-GAME-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Cameras_RigSelection` (contention cases) in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Cameras.md` → "Rig selection"; `Zenith2/Docs/Design/GAME/Cameras.md` → "Director algorithm"

### Z2-R-GAME-154 Playthrough integration gametests

- **Statement:** The ticket "OpenWorldSlice playthrough integration" (epic GAME, Stage 6) must add the content, asserts and segment markers of S05–S08 and the four gametests PlaythroughS05 to PlaythroughS08, each with a hand-written input script and its start scene, checking its segment's end assertions.
- **Covers:** Z2-I-GAME-10
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-021, Z2-D-077
- **Budgets:** none
- **Acceptance evidence:** `Game_OpenWorldSlice_PlaythroughS05`, `_PlaythroughS06`, `_PlaythroughS07`, `_PlaythroughS08` registered by that ticket and run in the Test gate step
- **Design:** `Zenith2/Docs/Design/GAME/OpenWorldSlice.md` → "Slice layout and ticket rule"; `Zenith2/Docs/Design/GAME/OpenWorldSlice.md` → "Sample tests"

### Z2-R-GAME-155 Transition entry point defined from Stage 2

- **Statement:** From Stage 2 `Zenith_WorldHost_BeginTransition` must be defined, so that every executable linking `Zenith_Runtime::RequestWorldTransition` links; until the Stage 3 body lands it must allocate nothing, change no record and refuse with `RUNTIME_ERROR_TRANSITION_IN_PROGRESS` and its log line.
- **Covers:** Z2-I-GAME-31
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** the Build and Test gate steps of Z2T-MOD-024, which is blocked by "GAME world host: lifecycle shutdown and teardown order" (`ZenithRuntime_Tests` links and runs, `Test_Runtime_WorldTransition_UnknownTagRefused` included); from S3 the transition tests of `Zenith2/Docs/Design/GAME/Tests.md` → "World host tests" prove the replacing body
- **Design:** `Zenith2/Docs/Design/GAME/WorldHost.md` → "World host files and split with MOD"; `Zenith2/Docs/Design/GAME.md` → "Ticket slices and ordering"

### Z2-R-GAME-156 Primary-world ticks counted across a swap

- **Statement:** `--exit-after-ticks n` must run exactly n primary-world ticks summed across a world swap, counted on the world the input-world mark names, and exit with code 0.
- **Covers:** Z2-I-GAME-31, Z2-I-GAME-32
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** the swap case of MOD's `Test_Runtime_ExitAfterTicks_RunsExactlyN` (`--exit-after-ticks 7`, the swap after tick 3), added by "GAME world host: swap and carry-over", in the Test gate step (`ZenithRuntime_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/Tests.md` → "World host tests"; `Zenith2/Docs/Design/GAME/WorldHost.md` → "Swap procedure"

### Z2-R-GAME-157 A save restore that never becomes ready fails

- **Statement:** Outside a replay, a save-source transition record still in `ACTIVATING` with its restore job not done `runtime.startupLoadTimeoutMs` after entering `ACTIVATING` must fail with `RESTORE` through the failure application procedure, logging the first unready participant's key, so that the world-load ID and the world's single world-load slot are released; in a replay the recorded outcome decides instead.
- **Covers:** Z2-I-GAME-31
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldTransition_FailedSaveLoadLeavesWorld` case (e) in the Test gate step (`ZenithRuntime_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/WorldHost.md` → "Transition record state machine"; `Zenith2/Docs/Design/GAME/WorldHost.md` → "Prologue step 7 procedure"

### Z2-R-GAME-158 Single step runs exactly n ticks

- **Statement:** `Zenith_WorldControl::RequestStep(n)` for 1 ≤ n ≤ 600 on a world whose only applied pause reasons are EDITOR and/or DEBUGGER must run exactly n fixed ticks, `min(remaining, max(1, T))` per frame without consuming or filling the accumulator, then leave the world paused; a DEBUGGER reason added during the step must cancel it, and a request outside 1 … 600 or on a world with any other reason must be refused, logged and counted.
- **Covers:** Z2-I-GAME-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldHost_RequestStepRunsExactlyN` in the Test gate step (`ZenithRuntime_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/WorldHost.md` → "Single step"

### Z2-R-GAME-159 No gameplay framework in edit and preview worlds

- **Statement:** In a world whose purpose is `WORLD_PURPOSE_EDIT` or `WORLD_PURPOSE_PREVIEW` the Gameplay module must add no system, listener or world-events node (AttachWorld step 3 adds nothing), so no camera director, timer, ability or rule runs while a scene is edited or previewed; a play world runs all of them.
- **Covers:** Z2-I-GAME-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-081
- **Budgets:** none
- **Acceptance evidence:** `Test_GameplayModule_SchedulePlacement` SUBCASE "edit and preview worlds" in the Test gate step (`ZenithGameplay_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME.md` → "Zenith_Module_Gameplay header"

### Z2-R-GAME-160 OpenWorldSlice menus, saving and accessible-flow route

- **Statement:** OpenWorldSlice must carry a title world with main and settings menus, a pause menu that saves, loads and quits to the title, and the accessible-flow route file `Zenith2/Tests/UIAccessibility/Routes/OpenWorldSlice.route.json`, all as game-side Luau and data.
- **Covers:** Z2-I-GAME-10
- **Stage:** S4
- **Classification:** game-side
- **Decisions:** Z2-D-021, Z2-D-077
- **Budgets:** none
- **Acceptance evidence:** `Game_OpenWorldSlice_TitleToPlay` in the Test gate step
- **Design:** `Zenith2/Docs/Design/GAME/OpenWorldSlice.md` → "Menus, save and accessible-flow route"

### Z2-R-GAME-161 OpenWorldSlice online content

- **Statement:** OpenWorldSlice must carry the Stage 4 online content of ONLINE's flagship slice integration — its catalogue of eight achievements, one entitlement and three cloud kinds, the account and cloud-conflict flows and the title world's hold until the first identity event — as game-side Luau and data.
- **Covers:** Z2-I-GAME-10
- **Stage:** S4
- **Classification:** game-side
- **Decisions:** Z2-D-021, Z2-D-077
- **Budgets:** none
- **Acceptance evidence:** `Game_OpenWorldSlice_OnlineCatalogue` in the Test gate step
- **Design:** `Zenith2/Docs/Design/GAME/OpenWorldSlice.md` → "Online content"

### Z2-R-GAME-162 No carry-over across a save profile change

- **Statement:** When the old world's SAVE binding profile differs from SAVE's active profile at a swap, the world host must carry no root over (neither requested IDs nor `Zenith.Runtime.CarryOver` entities), count `world.carryover.profile_changed` and log it, and complete the swap otherwise unchanged.
- **Covers:** Z2-I-GAME-31
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldTransition_ProfileChangeCarriesNothing` in the Test gate step (`ZenithRuntime_Tests`)
- **Design:** `Zenith2/Docs/Design/GAME/WorldHost.md` → "Carry-over"

### Z2-R-GAME-163 Voice locale as a game simulation input

- **Statement:** OpenWorldSlice must hold the voice-locale tag in a `CommandsOnly` component written only by its settings script from a recorded effective-locale input, saved with the world and re-issued after a restore whose tag differs, and its dialogue and equipment presentation must follow CHAR's playback and part-switching rules.
- **Covers:** Z2-I-GAME-10
- **Stage:** S4
- **Classification:** game-side
- **Decisions:** Z2-D-019, Z2-D-077
- **Budgets:** none
- **Acceptance evidence:** `Scripts/Tests/VoiceLocale.test.luau` through SCRIPT's Luau test runner in the Test gate step
- **Design:** `Zenith2/Docs/Design/GAME/OpenWorldSlice.md` → "Voice locale and character parts"

### Z2-R-GAME-164 Time of day and weather in the slice

- **Statement:** OpenWorldSlice's outdoor scenes must run a day/night script that sets the clock at world start, switches lamps at sunset and sunrise, requests the midday rain and tags the session while it rains, consuming ENV's `Zenith.Environment.TimeOfDay` and `Zenith.Environment.WeatherChanged` events.
- **Covers:** Z2-I-GAME-10
- **Stage:** S4
- **Classification:** game-side
- **Decisions:** Z2-D-077
- **Budgets:** none
- **Acceptance evidence:** `Game_OpenWorldSlice_DayNight` in the Test gate step
- **Design:** `Zenith2/Docs/Design/GAME/OpenWorldSlice.md` → "Time of day and weather"

## Coverage of inventory items

| Item | Requirements |
|---|---|
| Z2-I-GAME-01 | Z2-R-GAME-001, Z2-R-GAME-002, Z2-R-GAME-003, Z2-R-GAME-006, Z2-R-GAME-012, Z2-R-GAME-015, Z2-R-GAME-033, Z2-R-GAME-055, Z2-R-GAME-056, Z2-R-GAME-057, Z2-R-GAME-058, Z2-R-GAME-059, Z2-R-GAME-133, Z2-R-GAME-135, Z2-R-GAME-136, Z2-R-GAME-158, Z2-R-GAME-159 |
| Z2-I-GAME-02 | Z2-R-GAME-003, Z2-R-GAME-032, Z2-R-GAME-034, Z2-R-GAME-035, Z2-R-GAME-036, Z2-R-GAME-037, Z2-R-GAME-055, Z2-R-GAME-057, Z2-R-GAME-062, Z2-R-GAME-134, Z2-R-GAME-136, Z2-R-GAME-143 |
| Z2-I-GAME-03 | Z2-R-GAME-005, Z2-R-GAME-012, Z2-R-GAME-063, Z2-R-GAME-064, Z2-R-GAME-065, Z2-R-GAME-066, Z2-R-GAME-067, Z2-R-GAME-068, Z2-R-GAME-069, Z2-R-GAME-074, Z2-R-GAME-075, Z2-R-GAME-076, Z2-R-GAME-077, Z2-R-GAME-078, Z2-R-GAME-118, Z2-R-GAME-138, Z2-R-GAME-141, Z2-R-GAME-144, Z2-R-GAME-145, Z2-R-GAME-153 |
| Z2-I-GAME-04 | Z2-R-GAME-079, Z2-R-GAME-080, Z2-R-GAME-081, Z2-R-GAME-117 |
| Z2-I-GAME-05 | Z2-R-GAME-013, Z2-R-GAME-015, Z2-R-GAME-037, Z2-R-GAME-082, Z2-R-GAME-083, Z2-R-GAME-084, Z2-R-GAME-085 |
| Z2-I-GAME-06 | Z2-R-GAME-086, Z2-R-GAME-090, Z2-R-GAME-092, Z2-R-GAME-093, Z2-R-GAME-094, Z2-R-GAME-095, Z2-R-GAME-141 |
| Z2-I-GAME-07 | Z2-R-GAME-086, Z2-R-GAME-090, Z2-R-GAME-092, Z2-R-GAME-093, Z2-R-GAME-106, Z2-R-GAME-107, Z2-R-GAME-108, Z2-R-GAME-152 |
| Z2-I-GAME-08 | Z2-R-GAME-051, Z2-R-GAME-086, Z2-R-GAME-090, Z2-R-GAME-091, Z2-R-GAME-092, Z2-R-GAME-093, Z2-R-GAME-096, Z2-R-GAME-097 |
| Z2-I-GAME-09 | Z2-R-GAME-001, Z2-R-GAME-040, Z2-R-GAME-059, Z2-R-GAME-060, Z2-R-GAME-085, Z2-R-GAME-086, Z2-R-GAME-087, Z2-R-GAME-088, Z2-R-GAME-089, Z2-R-GAME-095, Z2-R-GAME-100, Z2-R-GAME-108, Z2-R-GAME-113, Z2-R-GAME-114, Z2-R-GAME-115, Z2-R-GAME-116, Z2-R-GAME-117, Z2-R-GAME-118 |
| Z2-I-GAME-10 | Z2-R-GAME-004, Z2-R-GAME-124, Z2-R-GAME-125, Z2-R-GAME-127, Z2-R-GAME-128, Z2-R-GAME-129, Z2-R-GAME-130, Z2-R-GAME-131, Z2-R-GAME-132, Z2-R-GAME-154, Z2-R-GAME-160, Z2-R-GAME-161, Z2-R-GAME-163, Z2-R-GAME-164 |
| Z2-I-GAME-11 | Z2-R-GAME-004, Z2-R-GAME-119, Z2-R-GAME-123, Z2-R-GAME-126, Z2-R-GAME-129 |
| Z2-I-GAME-12 | Z2-R-GAME-004, Z2-R-GAME-121, Z2-R-GAME-123, Z2-R-GAME-124, Z2-R-GAME-126, Z2-R-GAME-129 |
| Z2-I-GAME-13 | Z2-R-GAME-004, Z2-R-GAME-122, Z2-R-GAME-123, Z2-R-GAME-126, Z2-R-GAME-129 |
| Z2-I-GAME-14 | Z2-R-GAME-004, Z2-R-GAME-120, Z2-R-GAME-123, Z2-R-GAME-124, Z2-R-GAME-126, Z2-R-GAME-129 |
| Z2-I-GAME-15 | Z2-R-GAME-101, Z2-R-GAME-105, Z2-R-GAME-117, Z2-R-GAME-137 |
| Z2-I-GAME-16 | Z2-R-GAME-065, Z2-R-GAME-101, Z2-R-GAME-102, Z2-R-GAME-103, Z2-R-GAME-104, Z2-R-GAME-137, Z2-R-GAME-141, Z2-R-GAME-144 |
| Z2-I-GAME-17 | Z2-R-GAME-070, Z2-R-GAME-071, Z2-R-GAME-072 |
| Z2-I-GAME-18 | Z2-R-GAME-073 |
| Z2-I-GAME-19 | Z2-R-GAME-091, Z2-R-GAME-092, Z2-R-GAME-093, Z2-R-GAME-098, Z2-R-GAME-099, Z2-R-GAME-100, Z2-R-GAME-152 |
| Z2-I-GAME-20 | Z2-R-GAME-109, Z2-R-GAME-110, Z2-R-GAME-117, Z2-R-GAME-142, Z2-R-GAME-152 |
| Z2-I-GAME-21 | Z2-R-GAME-110, Z2-R-GAME-111, Z2-R-GAME-112, Z2-R-GAME-113 |
| Z2-I-GAME-22 | Z2-R-GAME-007, Z2-R-GAME-008, Z2-R-GAME-009, Z2-R-GAME-010, Z2-R-GAME-011, Z2-R-GAME-012, Z2-R-GAME-013, Z2-R-GAME-014, Z2-R-GAME-016, Z2-R-GAME-017, Z2-R-GAME-018, Z2-R-GAME-019, Z2-R-GAME-020, Z2-R-GAME-021, Z2-R-GAME-022, Z2-R-GAME-023, Z2-R-GAME-024, Z2-R-GAME-025, Z2-R-GAME-026, Z2-R-GAME-027, Z2-R-GAME-028, Z2-R-GAME-029, Z2-R-GAME-030, Z2-R-GAME-031, Z2-R-GAME-141, Z2-R-GAME-151 |
| Z2-I-GAME-23 | Z2-R-GAME-016 |
| Z2-I-GAME-24 | Z2-R-GAME-017, Z2-R-GAME-104 |
| Z2-I-GAME-25 | Z2-R-GAME-018, Z2-R-GAME-074 |
| Z2-I-GAME-26 | Z2-R-GAME-019 |
| Z2-I-GAME-27 | Z2-R-GAME-020 |
| Z2-I-GAME-28 | Z2-R-GAME-021, Z2-R-GAME-077 |
| Z2-I-GAME-29 | Z2-R-GAME-022, Z2-R-GAME-145 |
| Z2-I-GAME-30 | Z2-R-GAME-023, Z2-R-GAME-077 |
| Z2-I-GAME-31 | Z2-R-GAME-024, Z2-R-GAME-034, Z2-R-GAME-035, Z2-R-GAME-038, Z2-R-GAME-039, Z2-R-GAME-040, Z2-R-GAME-041, Z2-R-GAME-042, Z2-R-GAME-043, Z2-R-GAME-044, Z2-R-GAME-045, Z2-R-GAME-046, Z2-R-GAME-047, Z2-R-GAME-048, Z2-R-GAME-049, Z2-R-GAME-050, Z2-R-GAME-051, Z2-R-GAME-052, Z2-R-GAME-053, Z2-R-GAME-054, Z2-R-GAME-134, Z2-R-GAME-135, Z2-R-GAME-139, Z2-R-GAME-140, Z2-R-GAME-143, Z2-R-GAME-146, Z2-R-GAME-147, Z2-R-GAME-148, Z2-R-GAME-149, Z2-R-GAME-150, Z2-R-GAME-155, Z2-R-GAME-156, Z2-R-GAME-157, Z2-R-GAME-162 |
| Z2-I-GAME-32 | Z2-R-GAME-025, Z2-R-GAME-058, Z2-R-GAME-061, Z2-R-GAME-062, Z2-R-GAME-156 |

## Excluded and deferred

| Subject | Classification | Decision | Requirement or rationale |
|---|---|---|---|
| local multiplayer and split-screen (several local players, several cameras) | excluded | Z2-D-076 | one slot-0 player per world; a second is ignored and logged (`Zenith2/Docs/Design/GAME/Framework.md` → "Player and pawn components") |
| networked player slots and ownership authority | deferred | Z2-D-073 | Z2-R-GAME-137 |
| XR camera output | deferred | Z2-D-072 | Z2-R-GAME-138 |
| quest, dialogue, inventory, interaction as engine features | excluded from the engine | Z2-D-077 | game-side requirements of OpenWorldSlice instead |
| a gameplay time-scale control | excluded | Z2-D-080 | the simulation clock's `sim.timeScale` (JOBS) is process-wide; GAME adds no per-world time scale |
