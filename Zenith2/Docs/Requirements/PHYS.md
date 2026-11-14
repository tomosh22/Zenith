# PHYS requirement catalogue

## Purpose

This catalogue breaks the PHYS inventory items (`Zenith2/Docs/Requirements/Inventory.md` → "Inventory v1", section "PHYS Physics", Z2-I-PHYS-01 … Z2-I-PHYS-29) into testable requirements `Z2-R-PHYS-NNN`. Each requirement states one observable obligation, the items it covers, its stage and classification, the decisions and budget rows it answers to, the evidence that proves it, and the design headings that specify it (`Zenith2/Docs/Design/PHYS.md` and its family). Tickets cite requirements by ID; the design is the specification, this catalogue the acceptance list.

## Requirements

### Z2-R-PHYS-001 PhysX on Android selection harness

- **Statement:** `Zenith_Bench_PhysicsBackends` builds both engines for `android-arm64` and Windows, runs the body-level Stage 1 conformance subset (stacking, tunnelling, triggers, queries, and the backend-level determinism check at worker counts 1 and all) and the W-Characters, W-Dense and W-OpenWorld step and memory measurements on the S24 and the reference desktop, adds the benchmark-only `JPH::JobSystemThreadPool` Jolt measurement of Z2-R-PHYS-132, and writes the report that is the evidence of the D-SEL-PHYSX-ANDROID ruling and of the PHYS-1 trigger.
- **Covers:** Z2-I-PHYS-03
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-084, Z2-D-003
- **Budgets:** Z2-B-PHYS-001, Z2-B-PHYS-002
- **Acceptance evidence:** The harness `--smoke` CTest registered by the loop harness ticket (`Test` step); the harness report `Zenith2/out/<checkout-hash>/bench/PhysicsBackends/<platform>.json` attached to the `needs-human` harness-run ticket, which applies the ruling and appends its decision to the DecisionLog (recorded run evidence only in a `needs-human` ticket, the WORKER-EVIDENCE ruling).
- **Design:** Zenith2/Docs/Design/PHYS/Backends.md → "PhysX on Android evaluation"; Zenith2/Docs/Design/PHYS.md → "Open decisions"

### Z2-R-PHYS-002 PhysX Android ruling applied

- **Statement:** The D-SEL-PHYSX-ANDROID ruling ticket either keeps `android` in the `physx` manifest entry and `ZENITH_GPU` on `ZenithPhysics_PhysX`, or removes `android` and sets `ZENITH_WINDOWS` in the same diff, so `game.json` validation refuses PhysX for Android exactly when the ruling says so.
- **Covers:** Z2-I-PHYS-03
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-084, Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** The ruling diff; MOD's fixture for `backends.physics.android: PhysX` passes or refuses per the ruling (`Test` step of the CMake fixtures).
- **Design:** Zenith2/Docs/Design/PHYS/Backends.md → "Third-party pins"; Zenith2/Docs/Design/PHYS.md → "ZENITH_SUPPORTED matrix"

### Z2-R-PHYS-003 Jolt and PhysX manifest pins

- **Statement:** The `jolt` (v5.4.0, `cmake-subproject`, `cmakeSourceDir: Build`, MIT, owner `ZenithPhysics_Jolt`) and `physx` (PhysX 5, `zenith-wrapper`, BSD-3-Clause, owner `ZenithPhysics_PhysX`) entries exist in `Zenith2/ThirdParty/manifest.json` with verified archive digests and licence files, and no Zenith2 target includes or links the old engine's vendored Jolt.
- **Covers:** Z2-I-PHYS-02, Z2-I-PHYS-03
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** SHIP's manifest validation (`Configure` step) passes with both entries; `Libgraph` shows the third-party headers reached only from the backends' `Internal/`.
- **Design:** Zenith2/Docs/Design/PHYS/Backends.md → "Third-party pins"; Zenith2/Docs/Design/PHYS/Backends.md → "Build options"

### Z2-R-PHYS-004 PHYS per-system budget measurement

- **Statement:** The Stage 1 per-system budget ticket for PHYS measures Z2-B-PHYS-001 and Z2-B-PHYS-002 under MP-SUBMEM and MP-CPUZONE on the reference desktop and the S24 with both engines through the harness, including the W-OpenWorld cell-churn case (4,096 bodies destroyed and re-created at the freed indices in consecutive ticks, which measures Jolt's free-list walk in `CreateBodyWithID`), and records the Stage 1 values and the pending capacity rows' measured high-water marks.
- **Covers:** Z2-I-PHYS-02, Z2-I-PHYS-03
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-084
- **Budgets:** Z2-B-PHYS-001, Z2-B-PHYS-002
- **Acceptance evidence:** The budget harness's `--smoke` CTest (`Test` step), registered by the loop budget-harness ticket; the measured records under `Zenith2/out/<checkout-hash>/bench/` attached to the `needs-human` budget-value ticket, which fills the Budgets row values.
- **Design:** Zenith2/Docs/Design/PHYS.md → "Rows owned"; Zenith2/Docs/Design/PHYS.md → "Pending row needs"; Zenith2/Docs/Design/PHYS/Tests.md → "Benchmarks and budget evidence"

### Z2-R-PHYS-005 Cloth classification

- **Statement:** The D-CLASS-SIM ruling classifies cloth as required, deferred or excluded, names its owner and a library candidate (Jolt soft bodies or a custom solver), and records the flagship-slice need, the S24 cost estimate and the library support it weighed.
- **Covers:** Z2-I-PHYS-17
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-084
- **Budgets:** none
- **Acceptance evidence:** The D-CLASS-SIM ruling entry in `Zenith2/Docs/DecisionLog.md` (human ruling ticket).
- **Design:** Zenith2/Docs/Design/PHYS.md → "Open decisions"

### Z2-R-PHYS-006 Soft-body classification

- **Statement:** The D-CLASS-SIM ruling classifies soft bodies as required, deferred or excluded with an owner and a library candidate.
- **Covers:** Z2-I-PHYS-18
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-084
- **Budgets:** none
- **Acceptance evidence:** The D-CLASS-SIM ruling entry in `Zenith2/Docs/DecisionLog.md`.
- **Design:** Zenith2/Docs/Design/PHYS.md → "Open decisions"

### Z2-R-PHYS-007 Destruction classification

- **Statement:** The D-CLASS-SIM ruling classifies destruction (fracture, pre-fractured swaps) as required, deferred or excluded with an owner and a library candidate.
- **Covers:** Z2-I-PHYS-19
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-084
- **Budgets:** none
- **Acceptance evidence:** The D-CLASS-SIM ruling entry in `Zenith2/Docs/DecisionLog.md`.
- **Design:** Zenith2/Docs/Design/PHYS.md → "Open decisions"

### Z2-R-PHYS-008 API-only slice

- **Statement:** The first diffs of the Stage 3 sprint land the seven neutral API headers with contract blocks, no third-party types and header-only tests of the handle codec, hit collector, contact sink and material combine function, so dependent areas compile against the API before any backend exists (re-staged from S2 by the orchestrator's KIND-CODE-ORDER-2 and PHYS-R1-RULINGS rulings, because the slice names the asset kinds 36 and 37 that land in the Stage 3 sprint).
- **Covers:** Z2-I-PHYS-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-003, Z2-D-042
- **Budgets:** none
- **Acceptance evidence:** `ApiContractCheck`, `HeaderSelfContained` and the `ZenithPhysics_Tests` header-only cases (`Test` step).
- **Design:** Zenith2/Docs/Design/PHYS.md → "API-only slices"; Zenith2/Docs/Design/PHYS/PublicAPI.md → "Header list"

### Z2-R-PHYS-009 Module library targets

- **Statement:** `ZenithPhysics` (`KIND MODULE`, `MODULE_NAME Physics`) and the two `KIND BACKEND` libraries exist with exactly the `DEPENDS` lists of "Target summary", `ZENITH_GPU` support, public headers in the library roots and everything else under `Internal/`; a game without `Physics` in `game.json` links no physics symbol.
- **Covers:** Z2-I-PHYS-01, Z2-I-PHYS-02, Z2-I-PHYS-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-024, Z2-D-001, Z2-D-042
- **Budgets:** none
- **Acceptance evidence:** `Libgraph`, `SourceLayout`, `CompileCoverage`; pending ARCH row PH1 for the three added edges.
- **Design:** Zenith2/Docs/Design/PHYS.md → "Target summary"; Zenith2/Docs/Design/PHYS.md → "ZenithPhysics files"; Zenith2/Docs/Design/PHYS.md → "Backend library files"

### Z2-R-PHYS-010 Module entry points

- **Statement:** `Zenith_Module_Physics_RegisterTypes` and `Zenith_Module_Physics_Start` perform the steps of "Module start and world attach" in order and register the components, events, configuration keys, kind loaders, command kinds, bindings, asynchronous source and memory consumer of the module.
- **Covers:** Z2-I-PHYS-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-024, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsModule_HeadlessRuns` and the registration checks in `ZenithPhysics_Tests`; `ApiContractCheck` over `Zenith_Module_Physics.h`.
- **Design:** Zenith2/Docs/Design/PHYS.md → "Module start and world attach"; Zenith2/Docs/Design/PHYS/PublicAPI.md → "Zenith_Module_Physics header"

### Z2-R-PHYS-011 Module construction unwinds

- **Statement:** Every step of module start and of world attach has a failure-injection test that fails that step alone and asserts the module error, zero leaks and no registration, node or resource left behind.
- **Covers:** Z2-I-PHYS-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045, Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsModule_Create_FailsAt<Step>_Unwinds` for every step (`Test` step).
- **Design:** Zenith2/Docs/Design/PHYS.md → "Module start and world attach"; Zenith2/Docs/Design/PHYS/Tests.md → "Failure injection tests"

### Z2-R-PHYS-012 Neutral physics interface

- **Statement:** `Zenith_Physics` and `Zenith_PhysicsWorld` are the only way the module reaches a physics engine; they expose shapes, materials, worlds, bodies, constraints, stepping, queries and state save with the signatures, contract blocks and errors of the public API, and no third-party type or header crosses them.
- **Covers:** Z2-I-PHYS-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-003, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `ApiContractCheck`, `HeaderSelfContained`, `Libgraph` leakage rule; the conformance suite exercises every member on both backends.
- **Design:** Zenith2/Docs/Design/PHYS/PublicAPI.md → "Zenith_Physics header"; Zenith2/Docs/Design/PHYS/PublicAPI.md → "Zenith_PhysicsWorld header"; Zenith2/Docs/Design/PHYS.md → "Rules across the PHYS API"

### Z2-R-PHYS-013 Physics error codes

- **Statement:** Every recoverable physics failure is a `Zenith_PhysicsError` with the meaning of "Error codes"; untrusted input never asserts; `Zenith_ToString` names every enumerator.
- **Covers:** Z2-I-PHYS-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsHandle_NullHandleRejected`, `Test_PhysicsQuery_InvalidArgumentsCounted`, `Test_PhysicsFormat_TruncatedSectionsCorrupt` (`Test` step).
- **Design:** Zenith2/Docs/Design/PHYS.md → "Error codes"; Zenith2/Docs/Design/PHYS/PublicAPI.md → "Zenith_PhysicsErrors header"

### Z2-R-PHYS-014 Generational physics handles

- **Statement:** Body, constraint, shape and material handles are generational 64-bit values with the codec's bit layout; a destroyed object's handle, a handle of another world and the null handle are rejected with their errors on every member, and generations wrap without reaching zero.
- **Covers:** Z2-I-PHYS-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** The tests of "Handle and table tests" in `ZenithPhysics_Tests`.
- **Design:** Zenith2/Docs/Design/PHYS/PublicAPI.md → "Zenith_PhysicsTypes header"; Zenith2/Docs/Design/PHYS/Tests.md → "Handle and table tests"

### Z2-R-PHYS-015 Capacities refuse without side effects

- **Statement:** Body, constraint and shape creation beyond the configured capacity returns the capacity error and leaves the world unchanged; the module counts the refusal and retries an entity on its next change.
- **Covers:** Z2-I-PHYS-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** Z2-B-PHYS-001
- **Acceptance evidence:** `Test_PhysicsTable_BodyCapacityExactlyReached`, `Test_PhysicsTable_ConstraintCapacityExactlyReached`.
- **Design:** Zenith2/Docs/Design/PHYS.md → "Failure table"; Zenith2/Docs/Design/PHYS/Simulation.md → "Body lifecycle state machine"

### Z2-R-PHYS-016 Backend factories and one instance

- **Statement:** `Zenith_CreatePhysics_Jolt` and `Zenith_CreatePhysics_PhysX` follow their construction step tables, unwind every earlier step on failure, and refuse a second live instance of the same backend with `PHYSICS_ERROR_BACKEND_ALREADY_ACTIVE`.
- **Covers:** Z2-I-PHYS-02, Z2-I-PHYS-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-003, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** The `Test_PhysicsJolt_Create_FailsAt*_Unwinds` and `Test_PhysicsPhysX_Create_FailsAt*_Unwinds` tests; `Test_PhysicsJolt_SecondInstanceRefused`, `Test_PhysicsPhysX_SecondInstanceRefused`.
- **Design:** Zenith2/Docs/Design/PHYS/Backends.md → "Jolt construction steps"; Zenith2/Docs/Design/PHYS/Backends.md → "PhysX construction steps"

### Z2-R-PHYS-017 Jolt backend

- **Statement:** The Jolt backend implements every member of the neutral interface with the layer mapping, contact listener, queries, constraint mapping, shapes and state recorder of its design, built with the option table of "Build options".
- **Covers:** Z2-I-PHYS-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-003
- **Budgets:** Z2-B-PHYS-002
- **Acceptance evidence:** The whole conformance suite registered as `[Jolt]` (`Test` step); the Jolt backend unit tests.
- **Design:** Zenith2/Docs/Design/PHYS/Backends.md → "Jolt world construction"; Zenith2/Docs/Design/PHYS/Backends.md → "Jolt layer mapping"; Zenith2/Docs/Design/PHYS/Backends.md → "Jolt contact listener"; Zenith2/Docs/Design/PHYS/Backends.md → "Jolt queries"

### Z2-R-PHYS-018 Jolt step never blocks

- **Statement:** In the baseline the Jolt backend steps with `JPH::JobSystemSingleThreaded` on the step node's thread, so no job waits for another; it reports no parallel-step capability unless the conditional decision of Z2-R-PHYS-133 admits the exemption, and maps Jolt's update errors to step statistics.
- **Covers:** Z2-I-PHYS-02, Z2-I-PHYS-22
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** Z2-B-PHYS-002
- **Acceptance evidence:** `Test_PhysicsJolt_UpdateErrorsMapToStats`; the `Sanitizers -Preset win-asan` run of the conformance suite; review of the step against [Z2-N-CONTRACT-315].
- **Design:** Zenith2/Docs/Design/PHYS/Backends.md → "Jolt step and the job system"; Zenith2/Docs/Design/PHYS/Backends.md → "Conditional Jolt parallel step"

### Z2-R-PHYS-019 PhysX backend

- **Statement:** The PhysX backend implements every member of the neutral interface with the filter shader, contact reporting, constraint mapping, shapes and error callback of its design, built CPU-only through the wrapper.
- **Covers:** Z2-I-PHYS-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-003
- **Budgets:** Z2-B-PHYS-002
- **Acceptance evidence:** The whole conformance suite registered as `[PhysX]`; the PhysX backend unit tests.
- **Design:** Zenith2/Docs/Design/PHYS/Backends.md → "PhysX world construction"; Zenith2/Docs/Design/PHYS/Backends.md → "PhysX filter shader"; Zenith2/Docs/Design/PHYS/Backends.md → "PhysX contact reporting"; Zenith2/Docs/Design/PHYS/Backends.md → "PhysX error callback"

### Z2-R-PHYS-020 PhysX step in drain waves

- **Statement:** The PhysX step schedules drain waves through `ParallelFor` and resumes from continuations until the task graph is complete; no leaf waits, the dispatcher queue is bounded with an inline fallback, and the result does not depend on the number of leaves.
- **Covers:** Z2-I-PHYS-03, Z2-I-PHYS-22
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-019
- **Budgets:** Z2-B-PHYS-002
- **Acceptance evidence:** `Test_PhysicsPhysX_ResumeStepUntilComplete`, `Test_PhysicsPhysX_DispatcherInlinesWhenFull`, `Test_PhysicsDeterminism_ReplayAcrossWorkerCounts[PhysX]`.
- **Design:** Zenith2/Docs/Design/PHYS/Backends.md → "PhysX step and the dispatcher"; Zenith2/Docs/Design/PHYS/PublicAPI.md → "Zenith_PhysicsWorld header"

### Z2-R-PHYS-021 Third-party allocation routed to the root heap

- **Statement:** Every Jolt and PhysX allocation goes through hooks that allocate from the root heap; the Jolt process hooks are installed only by its factory and the backend refuses a heap other than the root heap.
- **Covers:** Z2-I-PHYS-02, Z2-I-PHYS-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-046, Z2-D-040
- **Budgets:** Z2-B-PHYS-001
- **Acceptance evidence:** `Test_PhysicsPhysX_AllocatorCountsLiveBytes`; root-heap leak reports of every physics test executable (exit code 3 on a leak); `Sanitizers -Preset win-asan`.
- **Design:** Zenith2/Docs/Design/PHYS/Backends.md → "Allocation and third-party global hooks"

### Z2-R-PHYS-022 Resident byte accounting

- **Statement:** Each backend reports resident bytes per its accounting rule and the module's memory consumer adds its own tables; the reported value tracks the root heap's live-byte delta within 10 %.
- **Covers:** Z2-I-PHYS-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** Z2-B-PHYS-001
- **Acceptance evidence:** `Test_PhysicsConformance_ResidentBytesTracksHeap[<Backend>]`.
- **Design:** Zenith2/Docs/Design/PHYS/Backends.md → "Resident byte accounting"; Zenith2/Docs/Design/PHYS/Simulation.md → "Memory consumer"

### Z2-R-PHYS-023 Memory pressure sheds nothing

- **Statement:** The physics memory consumer reports `MEMORY_SUBSYSTEM_SIMULATION` bytes and changes no physics state at WARNING or CRITICAL; PHYS's row in the shedding cases table asserts it.
- **Covers:** Z2-I-PHYS-01, Z2-I-PHYS-29
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** Z2-B-PHYS-001
- **Acceptance evidence:** PHYS's row in `Zenith2/Tests/Memory/Internal/Zenith_MemorySheddingCases.h` (CB8) run by the memory integration suite.
- **Design:** Zenith2/Docs/Design/PHYS/Simulation.md → "Memory consumer"; Zenith2/Docs/Design/PHYS.md → "Failure table"

### Z2-R-PHYS-024 Backend conformance suite

- **Statement:** `ZenithPhysics_Conformance` links both backends and runs every backend-level case (a case needing only a `Zenith_PhysicsWorld` and a job context) against each with identical, backend-neutral assertions, and `Zenith_Integration_Physics` runs every module-level case per backend the same way; no case compares results across backends.
- **Covers:** Z2-I-PHYS-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-003, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** CTest registration of every backend-level case as `<name>[Jolt]` and `<name>[PhysX]` in `ZenithPhysics_Conformance` and of every module-level `[<Backend>]` case in `Zenith_Integration_Physics` (`Test` step).
- **Design:** Zenith2/Docs/Design/PHYS/Tests.md → "Conformance suite"; Zenith2/Docs/Design/PHYS/Tests.md → "Scenario definitions"; Zenith2/Docs/Design/PHYS/Backends.md → "Backend behaviour differences"; Zenith2/Docs/Design/PHYS/Tests.md → "Test levels"

### Z2-R-PHYS-025 Capability matrix

- **Statement:** Each backend reports exactly the capabilities of the matrix, and every member gated by a capability returns `PHYSICS_ERROR_UNSUPPORTED` when it is absent.
- **Covers:** Z2-I-PHYS-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-003
- **Budgets:** none
- **Acceptance evidence:** Conformance case `Test_PhysicsConformance_CapabilitiesMatchMatrix[<Backend>]`.
- **Design:** Zenith2/Docs/Design/PHYS.md → "Capability matrix"

### Z2-R-PHYS-026 Per-backend determinism across workers and layout

- **Statement:** For a fixed backend, build and platform, the canonical state hash of every tick of the determinism scenario is identical at worker counts 1, 2, 8 and all, at chunk capacities 1, 7 and the default, and with forced compaction.
- **Covers:** Z2-I-PHYS-05, Z2-I-PHYS-29
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-003
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsDeterminism_ReplayAcrossWorkerCounts[<Backend>]` and `Test_PhysicsConformance_LayoutAndWorkerIndependent[<Backend>]` (integration suite, per ECS backend), `Test_PhysicsConformance_StepWorkerIndependent[<Backend>]` (conformance) (`Test` step).
- **Design:** Zenith2/Docs/Design/PHYS/Tests.md → "Determinism replay"; Zenith2/Docs/Design/PHYS/Simulation.md → "Determinism rules"; Zenith2/Docs/Design/PHYS/Tests.md → "Test levels"

### Z2-R-PHYS-027 Determinism with script failures

- **Statement:** Ticks in which a script node fails produce the same physics results at every worker count, because failed records are discarded before playback.
- **Covers:** Z2-I-PHYS-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsDeterminism_ReplayWithScriptFailures[<Backend>]`.
- **Design:** Zenith2/Docs/Design/PHYS/Tests.md → "Determinism replay"; Zenith2/Docs/Design/PHYS/Simulation.md → "Determinism rules"

### Z2-R-PHYS-028 Determinism rules in code

- **Statement:** No PHYS algorithm orders by address, slot, worker index or time; all orderings are by simulation ID or sort key; the floating-point environment is the engine's fixed one after every backend call; and no simulation-affecting PHYS code calls a CRT transcendental: buoyancy, vehicles, the controller and the cook steps use CORE's deterministic functions (the X314-FMA3 ruling), `std::sqrt` alone being allowed.
- **Covers:** Z2-I-PHYS-05, Z2-I-PHYS-29
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsJolt_StepKeepsFloatEnvironment`, `Test_PhysicsPhysX_StepKeepsFloatEnvironment`; review against rules D1–D12 (D9 names the transcendental rule).
- **Design:** Zenith2/Docs/Design/PHYS/Simulation.md → "Determinism rules"; Zenith2/Docs/Design/PHYS/Backends.md → "Build options"

### Z2-R-PHYS-029 Determinism boundaries stated

- **Statement:** Nothing in PHYS promises equality across backends, platforms or configurations, and no test compares them; a PhysX restore is COHERENT and marks the save COHERENT_ONLY.
- **Covers:** Z2-I-PHYS-29
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-003
- **Budgets:** none
- **Acceptance evidence:** Review of the conformance suite (no cross-backend comparison); `Test_PhysicsConformance_SaveRestoreCoherent[PhysX]`.
- **Design:** Zenith2/Docs/Design/PHYS/Simulation.md → "Determinism boundaries"

### Z2-R-PHYS-030 Explicit substep loop

- **Statement:** Each tick runs exactly `physics.substepsPerTick` substeps of equal length, with kinematic targets interpolated, forces applied and joints checked per substep, and no other path steps the backend.
- **Covers:** Z2-I-PHYS-22
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** Z2-B-PHYS-002
- **Acceptance evidence:** `Test_PhysicsSubstep_ExactlyNStepsPerTick`, `Test_PhysicsSubstep_KinematicTargetInterpolated[<Backend>]`.
- **Design:** Zenith2/Docs/Design/PHYS/Simulation.md → "Substep loop"

### Z2-R-PHYS-031 Substep cap and hitch behaviour

- **Statement:** No tick runs more than 8 substeps, steps per frame never exceed the catch-up row's ticks times 8, and a long hitch drops simulated time instead of bursting steps, so a resting body does not sink.
- **Covers:** Z2-I-PHYS-22
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** Z2-B-PHYS-002, Z2-B-JOBS-002
- **Acceptance evidence:** `Test_PhysicsSubstep_CapAtEight`, `Test_PhysicsSubstep_HitchDropsTimeNotSinks[<Backend>]`.
- **Design:** Zenith2/Docs/Design/PHYS/Simulation.md → "Substep loop"; Zenith2/Docs/Design/PHYS/Tests.md → "Substep and hitch tests"; Zenith2/Docs/Design/PHYS.md → "Limits and constants"

### Z2-R-PHYS-032 Physics nodes and phases

- **Statement:** The module registers `Physics.CharacterMove` (phase 4), `Physics.Step` (5), `Physics.SyncTransforms`, `Physics.RagdollBlend` and `Physics.DeferredQueries` (6) and the debug epilogue stage with exactly the declared access of the node table.
- **Covers:** Z2-I-PHYS-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** Z2-B-PHYS-002, Z2-B-GAME-001
- **Acceptance evidence:** The schedule compiler accepts the declarations (no schedule-compile error); `Test_Gameplay_AuthorityCensusMatchesRegistry` with PHYS's census rows.
- **Design:** Zenith2/Docs/Design/PHYS/Simulation.md → "Frame-graph placement"; Zenith2/Docs/Design/PHYS/ComponentsAPI.md → "Authority census rows"

### Z2-R-PHYS-033 Ingest of components into bodies

- **Statement:** At the start of the step the module creates, recomposes and destroys bodies from `Zenith.Physics.RigidBody` and `Zenith.Physics.Collider` in ascending simulation ID, composing descendant colliders into the ancestor body.
- **Covers:** Z2-I-PHYS-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-PHYS-002
- **Acceptance evidence:** `Test_PhysicsIngest_ComposesCollidersOfSubtree`; `Test_PhysicsIntegration_SceneBodiesFromAuthoredScene`.
- **Design:** Zenith2/Docs/Design/PHYS/Simulation.md → "Ingest algorithm"; Zenith2/Docs/Design/PHYS/Simulation.md → "Body composition"

### Z2-R-PHYS-034 External writes

- **Statement:** A local transform or velocity changed by another writer since PHYS last wrote it is applied to the body as a teleport (to `W_parent · L`) or a velocity set and counted, the comparison being between the observed `Zenith.LocalTransform` bits and the local bits PHYS wrote, never a recomposed world pose.
- **Covers:** Z2-I-PHYS-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsIngest_ExternalWriteTeleportsBody`, `Test_PhysicsIngest_ParentedBodyNoSpuriousExternalWrite` (`Test` step).
- **Design:** Zenith2/Docs/Design/PHYS/Simulation.md → "External writes"

### Z2-R-PHYS-035 Pending body operations

- **Statement:** Impulses, forces, velocity sets and sleep requests recorded as commands apply in command-merge order at the next step, bounded by `physics.maxPendingOpsPerTick` with counted drops.
- **Covers:** Z2-I-PHYS-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsIngest_PendingOpsAppliedInOrder`, `Test_PhysicsIngest_PendingOpsCapacity`.
- **Design:** Zenith2/Docs/Design/PHYS/Simulation.md → "Pending operations"; Zenith2/Docs/Design/PHYS.md → "Command kinds"

### Z2-R-PHYS-036 Command kinds and recording functions

- **Statement:** The eight command kinds are registered in order and each recording function takes `const Zenith_PhysicsCommandKinds&` and records nothing (counted) when the kinds are unresolved.
- **Covers:** Z2-I-PHYS-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsCommands_RecordAndPlayback` in `ZenithPhysics_Tests`; pending ARCH row PH6 and MOD row PH7.
- **Design:** Zenith2/Docs/Design/PHYS.md → "Command kinds"; Zenith2/Docs/Design/PHYS/ComponentsAPI.md → "Zenith_PhysicsCommands header"

### Z2-R-PHYS-037 Write-back to components

- **Statement:** After the substeps the step writes RigidBody 20–22 for every active dynamic or kinematic body, RigidBody 23 for every body whose frozen state changed, Joint 21 and Vehicle 30–32, in chunk order, without touching transforms.
- **Covers:** Z2-I-PHYS-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** Z2-B-PHYS-002
- **Acceptance evidence:** `Test_PhysicsWriteBack_ActiveBodiesOnly` in `ZenithPhysics_Tests`.
- **Design:** Zenith2/Docs/Design/PHYS/Simulation.md → "Write-back"

### Z2-R-PHYS-038 Physics-to-transform sync

- **Statement:** `Physics.SyncTransforms` writes `Zenith.LocalTransform` position and rotation with writer source Physics for every active dynamic body, every dynamic body whose parent moved and every controller, as parent-relative poses for parented entities, records the exact bits written, never writes scale and never writes a sleeping body under an unmoved parent; an oriented shape keeps its full rotation.
- **Covers:** Z2-I-PHYS-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** Z2-B-PHYS-002
- **Acceptance evidence:** `Test_PhysicsSync_RotatedBoxKeepsRotation[<Backend>]`, `Test_PhysicsSync_ChildBodyWritesLocalPose`, `Test_PhysicsSync_SleepingBodyNotWritten`, `Test_PhysicsSync_DynamicBodyIgnoresMovingParent` (`Test` step).
- **Design:** Zenith2/Docs/Design/PHYS/Simulation.md → "Physics-to-transform sync"

### Z2-R-PHYS-039 Sleeping

- **Statement:** Bodies sleep after `physics.sleepSeconds` below `physics.sleepVelocity` and wake on contact, command or external write; a sleeping body produces no transform writes; and a pair persists while a body of it sleeps, so neither falling asleep nor waking emits a contact or trigger event on either backend.
- **Covers:** Z2-I-PHYS-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-PHYS-002
- **Acceptance evidence:** `Test_PhysicsStack_Stack10SettlesAndSleeps[<Backend>]`, `Test_PhysicsTriggers_SleepingBodyStaysInside`, `Test_PhysicsEvents_PairSurvivesSleepAndWake[<Backend>]` (`Test` step).
- **Design:** Zenith2/Docs/Design/PHYS/Simulation.md → "Sleeping"; Zenith2/Docs/Design/PHYS/Simulation.md → "Event emission"

### Z2-R-PHYS-040 Body lifecycle

- **Statement:** Bodies follow the body lifecycle state machine, including frozen and destroyed states, and every path that removes all of a simulation ID's bodies (ingest removal, a removed capsule, region freeze, the end-of-tick destruction, a ragdoll enter's capsule and a ragdoll's removed parts) ends every pair of that ID exactly once through the ended list, and every constraint of the body.
- **Covers:** Z2-I-PHYS-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsEvents_EndedAfterDestroy` (every path); `Test_PhysicsTable_ShapeReleaseIsRefcounted` (`Test` step).
- **Design:** Zenith2/Docs/Design/PHYS/Simulation.md → "Body lifecycle state machine"; Zenith2/Docs/Design/PHYS/Simulation.md → "Module world state"; Zenith2/Docs/Design/PHYS/Simulation.md → "Event emission"

### Z2-R-PHYS-041 Lifecycle listener

- **Statement:** The module's lifecycle listener applies teleports (including to ragdoll parts and controllers), destroy-pending and destroyed events as the table specifies.
- **Covers:** Z2-I-PHYS-01, Z2-I-PHYS-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsLifecycle_TeleportMovesBody` in `ZenithPhysics_Tests`; GAME's `Test_Scenario_TeleportDuringRagdoll`.
- **Design:** Zenith2/Docs/Design/PHYS.md → "Lifecycle listener"; Zenith2/Docs/Design/PHYS/CharacterRagdollVehicle.md → "Teleport during a ragdoll"

### Z2-R-PHYS-042 Configuration keys

- **Statement:** Every `physics.*` key of the table is registered with its type, default, range and simulation-affecting and restart flags, and invalid values are refused by the schema.
- **Covers:** Z2-I-PHYS-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsConfig_SubstepRangeEnforced`, `Test_PhysicsConfig_StepLongerThanMaxRefused`.
- **Design:** Zenith2/Docs/Design/PHYS/ComponentsAPI.md → "Configuration keys"

### Z2-R-PHYS-043 Authority census rows

- **Statement:** Every PHYS component property has its census row with owning phase, writer sources and readers, added in the diff that registers the type.
- **Covers:** Z2-I-PHYS-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Gameplay_AuthorityCensusMatchesRegistry` (GAME's census test) with PHYS's rows.
- **Design:** Zenith2/Docs/Design/PHYS/ComponentsAPI.md → "Authority census rows"

### Z2-R-PHYS-044 Physics components

- **Statement:** `Zenith.Physics.RigidBody`, `Collider`, `Joint`, `CharacterController`, `Ragdoll`, `Buoyancy` and `Vehicle` are registered reflected components with the property IDs, types, ranges and units of their headers, and IDs are never reused.
- **Covers:** Z2-I-PHYS-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsComponents_RegistrationMatchesTables` (`Test` step); `ApiContractCheck`.
- **Design:** Zenith2/Docs/Design/PHYS/ComponentsAPI.md → "Zenith_PhysicsComponents header"; Zenith2/Docs/Design/PHYS/ComponentsAPI.md → "Rigid body properties"; Zenith2/Docs/Design/PHYS/ComponentsAPI.md → "Collider properties"; Zenith2/Docs/Design/PHYS/Tests.md → "Component and asset tests"

### Z2-R-PHYS-045 Collision shape asset kind

- **Statement:** Kind 16 `Physics.CollisionShape` loads `.zcol` files into `Zenith_CollisionShapeAsset` on loader jobs, holding the backend shape and its material palette until the registry retires it.
- **Covers:** Z2-I-PHYS-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsLoader_CollisionShapeActivatesAndRetires`, `Test_PhysicsCook_BackendSectionMatchesNeutral[<Backend>]` (`Test` step).
- **Design:** Zenith2/Docs/Design/PHYS/Formats.md → "Collision shape file"; Zenith2/Docs/Design/PHYS/PublicAPI.md → "Zenith_CollisionShapeAsset header"; Zenith2/Docs/Design/PHYS.md → "Asset kinds"; Zenith2/Docs/Design/PHYS/Tests.md → "Loader tests"

### Z2-R-PHYS-046 Neutral geometry is authoritative

- **Statement:** Every `.zcol` carries a neutral geometry section from which either backend builds the shape; a backend section is used only when its backend, data tag and neutral hash match, and otherwise the neutral path is taken and counted.
- **Covers:** Z2-I-PHYS-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsCook_StaleBackendSectionIgnored`, `Test_PhysicsCook_BackendSectionMatchesNeutral[<Backend>]`.
- **Design:** Zenith2/Docs/Design/PHYS/Formats.md → "Neutral geometry section"; Zenith2/Docs/Design/PHYS/Formats.md → "Backend sections"; Zenith2/Docs/Design/PHYS/Backends.md → "Backend data tag"

### Z2-R-PHYS-047 Backend shape payloads

- **Statement:** `BakeShape` writes each backend's binary shape for hulls, meshes and (Jolt) compounds through arena streams, and `CreateShapeFromCooked` reads it back or reports corruption.
- **Covers:** Z2-I-PHYS-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsCook_BackendSectionMatchesNeutral[<Backend>]`; pending ARCH row PH5.
- **Design:** Zenith2/Docs/Design/PHYS/Backends.md → "Backend shape payloads"

### Z2-R-PHYS-048 Collision shape documents and importer settings

- **Statement:** Collision shape documents and a mesh's `physics` importer settings cook into `.zcol` files deterministically, with hulls computed independently of input order and degenerate hulls refused with a message.
- **Covers:** Z2-I-PHYS-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsCook_ConvexHullDeterministic`, `Test_PhysicsCook_DegenerateHullRefused`.
- **Design:** Zenith2/Docs/Design/PHYS/Formats.md → "Collision shape document"; Zenith2/Docs/Design/PHYS/Formats.md → "Collision importer settings"; Zenith2/Docs/Design/PHYS.md → "Cook steps and derived-data keys"

### Z2-R-PHYS-049 Terrain heightfield collision

- **Statement:** `physics.heightfield` cooks ENV's `ZPHI` intermediate into a kind-7 shape whose triangles follow ENV's fixed diagonal and heights, with holes producing no triangles and per-quad materials from the palette.
- **Covers:** Z2-I-PHYS-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsCook_HeightfieldMatchesEnvHeights`; pending ENV row PH9.
- **Design:** Zenith2/Docs/Design/PHYS/Formats.md → "Heightfield intermediate"; Zenith2/Docs/Design/PHYS/Formats.md → "Neutral geometry section"

### Z2-R-PHYS-050 Foliage compound collision

- **Statement:** `physics.foliageCompound` cooks ENV's `ZPCI` intermediate into one static compound per cell with one child per record in record order.
- **Covers:** Z2-I-PHYS-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsCook_FoliageCompoundFromIntermediate`.
- **Design:** Zenith2/Docs/Design/PHYS/Formats.md → "Compound intermediate"

### Z2-R-PHYS-051 Cooked parser limits

- **Statement:** Every PHYS parser enforces the limits of its table, accepting each limit and refusing limit + 1 with a recoverable error, and every truncation of a valid file is `PHYSICS_ERROR_COOKED_CORRUPT`.
- **Covers:** Z2-I-PHYS-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsFormat_ZcolParserLimits`, `Test_PhysicsFormat_TruncatedSectionsCorrupt`; `Zenith_Fuzz_PhysicsCooked`; pending ARCH row PH15.
- **Design:** Zenith2/Docs/Design/PHYS/Formats.md → "Parser limits"; Zenith2/Docs/Design/PHYS/Formats.md → "Common rules"

### Z2-R-PHYS-052 Physics document types and migration

- **Statement:** The three physics document types preserve unknown members, migrate with preview, backup and journaled writes, and join the authoring document type list.
- **Covers:** Z2-I-PHYS-06, Z2-I-PHYS-25, Z2-I-PHYS-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsDocument_UnknownMembersPreserved`; pending CORE row PH14.
- **Design:** Zenith2/Docs/Design/PHYS.md → "Document types and migration"; Zenith2/Docs/Design/PHYS/Formats.md → "Material document"

### Z2-R-PHYS-053 Layer configuration

- **Statement:** The layer names and collision pairs are parsed from their configuration keys with the documented grammar into a symmetric table; invalid configuration refuses module start with the token position.
- **Covers:** Z2-I-PHYS-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsLayers_ParseNamesAndPairs`, `Test_PhysicsLayers_RejectsUnknownName`, `Test_PhysicsLayers_ParserLimits`.
- **Design:** Zenith2/Docs/Design/PHYS.md → "Layer configuration"

### Z2-R-PHYS-054 Layer filtering on both backends

- **Statement:** Bodies collide exactly when the layer table allows it, with the static, moving, trigger and query-only classes filtered identically on Jolt (object layers and broad-phase filters) and PhysX (filter shader with `eKEEP` kinematic filtering), and queries see each class exactly when the query filter's gate for that class is set, on both backends.
- **Covers:** Z2-I-PHYS-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-003
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsJolt_PairFilterMatchesLayerTable`, `Test_PhysicsPhysX_FilterShaderTruthTable`, conformance cases `Test_PhysicsConformance_LayerMatrixRespected[<Backend>]` and `Test_PhysicsConformance_QueryOnlyVisibleToQueries[<Backend>]` (`Test` step).
- **Design:** Zenith2/Docs/Design/PHYS/Backends.md → "Jolt layer mapping"; Zenith2/Docs/Design/PHYS/Backends.md → "PhysX filter shader"; Zenith2/Docs/Design/PHYS/Backends.md → "PhysX scene query filter"; Zenith2/Docs/Design/PHYS/Simulation.md → "Query execution"

### Z2-R-PHYS-055 Script layer names

- **Statement:** The script name table `physicsLayer` lists the configured layers so scripts resolve layer names at compile time.
- **Covers:** Z2-I-PHYS-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsLayers_ScriptNameTableMatches`.
- **Design:** Zenith2/Docs/Design/PHYS.md → "Layer configuration"; Zenith2/Docs/Design/PHYS/ComponentsAPI.md → "Script bindings"

### Z2-R-PHYS-056 Synchronous queries

- **Statement:** Ray casts, shape casts and overlaps through `Zenith_PhysicsWorldResource` validate their arguments, filter by layer mask and ignore root, and return hits mapped to entities from committed state.
- **Covers:** Z2-I-PHYS-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsQuery_IgnoreRootSubtree`, `Test_PhysicsQuery_InvalidArgumentsCounted`, conformance case `Test_PhysicsConformance_QueriesHitExpected[<Backend>]`.
- **Design:** Zenith2/Docs/Design/PHYS/Simulation.md → "Query execution"; Zenith2/Docs/Design/PHYS/ComponentsAPI.md → "Zenith_PhysicsWorldResource header"

### Z2-R-PHYS-057 Query determinism

- **Statement:** Multi-hit queries return the K smallest keys in ascending order and single-hit queries break distance ties by lowest simulation ID, independent of backend traversal order.
- **Covers:** Z2-I-PHYS-08, Z2-I-PHYS-29
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsQuery_ClosestHitTieBreak`, `Test_PhysicsQuery_AllHitsSortedAndCapped`.
- **Design:** Zenith2/Docs/Design/PHYS/PublicAPI.md → "Zenith_PhysicsQuery header"; Zenith2/Docs/Design/PHYS/Simulation.md → "Query execution"

### Z2-R-PHYS-058 Collision query for cameras

- **Statement:** The module implements `Zenith_CollisionQuery` per world as a world resource owned by the Physics phase, whose `Sweep` equals a ray cast for radius 0 and a sphere cast of the sweep's radius otherwise, with the same layer mask and ignore root.
- **Covers:** Z2-I-PHYS-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_CollisionQuery_SweepMatchesSphereAndRayCast` (`Test` step); GAME's camera tests.
- **Design:** Zenith2/Docs/Design/PHYS/ComponentsAPI.md → "Zenith_CollisionQuery"; Zenith2/Docs/Design/PHYS/Simulation.md → "Query execution"

### Z2-R-PHYS-059 Primitive cache

- **Statement:** Shape casts, overlaps and collide queries with primitive shapes take a shape pinned in the process primitive cache (at most 4,096 entries) for the whole backend query: lookup, pin and LRU touch under the cache's own level-500 mutex, creation and release outside it, a duplicate from a racing miss discarded, eviction only of unpinned entries (least recently used), and an uncached shape the query releases itself when every entry is pinned; the cache's mutex is never held across a call into `Zenith_Physics`.
- **Covers:** Z2-I-PHYS-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsQuery_PrimitiveCacheBounded`, `Test_PhysicsQuery_PrimitiveCacheConcurrentStress` (`Test` step).
- **Design:** Zenith2/Docs/Design/PHYS/Simulation.md → "Query execution"; Zenith2/Docs/Design/PHYS.md → "Lock-order entries"; Zenith2/Docs/Design/PHYS/Simulation.md → "Primitive cache"

### Z2-R-PHYS-060 Deferred script queries

- **Statement:** A behaviour's `Physics.RequestQuery` becomes a request of the source kind `ASYNC_SOURCE_PHYSICS_QUERY` (named by its enumerator, the value the registry assigns); a request committed in phase 2 of tick t executes in phase 6 of tick t and one committed in phase 10 in phase 6 of tick t + 1, from that tick's post-step state; requests beyond `physics.deferredQueriesPerTick` wait for the next tick in completion-ID order and are never dropped; every result becomes visible at a tick boundary in completion-ID order.
- **Covers:** Z2-I-PHYS-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** Z2-B-SCRIPT-026
- **Acceptance evidence:** `Test_PhysicsDeferred_ResultAtFixedTick`, `Test_PhysicsDeferred_ExcessWaitsInOrder`, `Test_PhysicsDeferred_DecoderHandlesCancelledAndEmpty`, `Test_PhysicsIntegration_ScriptQueriesAndImpulses` (`Test` step); core-doc item X264 (h) for the request's one spelling.
- **Design:** Zenith2/Docs/Design/PHYS/Simulation.md → "Deferred queries"; Zenith2/Docs/Design/PHYS/Formats.md → "Deferred query payloads"

### Z2-R-PHYS-061 Deferred query cancellation and save

- **Statement:** Every completion ID of a deferred query is completed exactly once: by its parked result, or, when its world is detached first, by the cancelled zero-hit result parked from `DetachWorld` into the world's table, live or retired; PHYS never counts `world.async_discarded`; cancelling at any point leaves no request, uncompleted ID or allocation; INBOX requests at a save are re-issued at restore under their recorded completion IDs and marked re-issued, and the restore allocates no completion ID.
- **Covers:** Z2-I-PHYS-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** Z2-B-SCRIPT-026
- **Acceptance evidence:** `Test_PhysicsDeferred_DetachCompletesInbox`, `Test_PhysicsDeferred_CancelStress1000Cycles` (`z2-stress`), `Test_PhysicsDeferred_SavedRequestsReissued`, `Test_PhysicsIntegration_SwapCompletesInboxQueries` (`Test` step).
- **Design:** Zenith2/Docs/Design/PHYS/Simulation.md → "Deferred query state machine"; Zenith2/Docs/Design/PHYS/Simulation.md → "Deferred queries"; Zenith2/Docs/Design/PHYS/Simulation.md → "Save and restore algorithm"

### Z2-R-PHYS-062 Trigger events

- **Statement:** A dynamic or kinematic non-trigger body entering and leaving a trigger produces exactly one `Zenith.Physics.TriggerEntered` and one `TriggerExited` `{ Trigger, Other }`, emitted in merge-key order and delivered in phase 9; static bodies, other triggers, query-only bodies and excluded-group bodies never produce one, identically on both backends.
- **Covers:** Z2-I-PHYS-09, Z2-I-PHYS-24
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** Z2-B-GAME-003
- **Acceptance evidence:** `Test_PhysicsTriggers_EnterExitOncePerOverlap`; conformance cases `Test_PhysicsConformance_TriggersReported[<Backend>]` and `Test_PhysicsConformance_TriggerReachMatchesRule[<Backend>]` (`Test` step).
- **Design:** Zenith2/Docs/Design/PHYS/Simulation.md → "Trigger semantics"; Zenith2/Docs/Design/PHYS/Simulation.md → "Body composition"; Zenith2/Docs/Design/PHYS/ComponentsAPI.md → "Zenith_PhysicsEvents header"

### Z2-R-PHYS-063 Trigger lifetime semantics

- **Statement:** Destroying a trigger or an overlapping body ends its overlaps with exit events in ascending order; a body that sleeps inside a trigger stays inside.
- **Covers:** Z2-I-PHYS-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsTriggers_TriggerDestroyedEndsOverlaps`, `Test_PhysicsTriggers_SleepingBodyStaysInside`.
- **Design:** Zenith2/Docs/Design/PHYS/Simulation.md → "Trigger semantics"

### Z2-R-PHYS-064 Contact events

- **Statement:** Bodies flagged to report contacts produce `ContactBegan` and `ContactEnded` events with point, normal and approach speed; ending a pair by destruction reports the destroyed entity; both backends' records follow one semantics, Jolt's sleep-time removals being flagged and suspended rather than ending the pair.
- **Covers:** Z2-I-PHYS-23, Z2-I-PHYS-24
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** Z2-B-GAME-003
- **Acceptance evidence:** `Test_PhysicsEvents_EndedAfterDestroy`, `Test_PhysicsEvents_SuspendedContactsResolveAtWake`; conformance case `Test_PhysicsConformance_ContactsReported[<Backend>]` with its per-backend record assertions (`Test` step).
- **Design:** Zenith2/Docs/Design/PHYS/Simulation.md → "Event emission"; Zenith2/Docs/Design/PHYS/Simulation.md → "Sleeping"; Zenith2/Docs/Design/PHYS/ComponentsAPI.md → "Zenith_PhysicsEvents header"

### Z2-R-PHYS-065 Contact ordering and capacity

- **Statement:** Contact records are sorted by key before use and, when the sink is full, the lowest keys are kept and the rest counted, so the kept set never depends on backend report order.
- **Covers:** Z2-I-PHYS-23
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsEvents_SortedByKey`, `Test_PhysicsEvents_SinkKeepsLowestKeys`.
- **Design:** Zenith2/Docs/Design/PHYS/PublicAPI.md → "Zenith_PhysicsWorld header"; Zenith2/Docs/Design/PHYS/Simulation.md → "Event emission"

### Z2-R-PHYS-066 Event emission order

- **Statement:** All physics events of a tick are delivered in merge-key order: ascending event key ID (a pair's low simulation ID, a broken joint's simulation ID), then began or entered, ended or exited, interaction and joint-broken groups, each in ascending high simulation ID, identically across worker counts and report orders.
- **Covers:** Z2-I-PHYS-24
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-080
- **Budgets:** Z2-B-GAME-003
- **Acceptance evidence:** `Test_PhysicsEvents_SortedByKey` (`Test` step); the event sequence in the canonical state hash of the determinism replay.
- **Design:** Zenith2/Docs/Design/PHYS/Simulation.md → "Event emission"

### Z2-R-PHYS-067 Physical material asset

- **Statement:** The `Physics.Material` kind loads `.zpmat` files with friction, restitution, density, combine modes, surface type and interaction rules; a missing material substitutes the default material.
- **Covers:** Z2-I-PHYS-25
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsMaterial_LoaderReadsSections` (`Test` step); pending ASSET row PH4.
- **Design:** Zenith2/Docs/Design/PHYS/Formats.md → "Material file"; Zenith2/Docs/Design/PHYS/PublicAPI.md → "Zenith_PhysicsMaterial header"; Zenith2/Docs/Design/PHYS.md → "Asset kinds"; Zenith2/Docs/Design/PHYS/Tests.md → "Loader tests"

### Z2-R-PHYS-068 Material combination

- **Statement:** The combined friction and restitution of a contact follow the combine table with the higher-valued mode winning, identically on both backends.
- **Covers:** Z2-I-PHYS-25
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-003
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsMaterial_CombineTable`, `Test_PhysicsConformance_MaterialCombineMatchesFunction[<Backend>]`.
- **Design:** Zenith2/Docs/Design/PHYS/Backends.md → "Material combination"

### Z2-R-PHYS-069 Material documents

- **Statement:** Material documents cook into `.zpmat` with validated ranges, surface types and at most 64 interaction rules.
- **Covers:** Z2-I-PHYS-25
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsMaterial_CookValidatesRanges` (`ZenithCook_Tests`, `Test` step).
- **Design:** Zenith2/Docs/Design/PHYS/Formats.md → "Material document"; Zenith2/Docs/Design/PHYS/Tests.md → "Cook and format tests"

### Z2-R-PHYS-070 Physical interaction events

- **Statement:** A contact whose material pair names an interaction raises one `Zenith.Physics.Interaction` event when its approach speed reaches the rule's impact threshold (or slide speed its slide threshold), using the lookup order of the rules.
- **Covers:** Z2-I-PHYS-26
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** Z2-B-GAME-003
- **Acceptance evidence:** `Test_PhysicsMaterial_InteractionLookupOrder`, `Test_PhysicsInteraction_ThresholdsUseApproachSpeed`.
- **Design:** Zenith2/Docs/Design/PHYS/Simulation.md → "Interaction detection"

### Z2-R-PHYS-071 Interaction cooldown

- **Statement:** An interaction fires at most once per pair per cooldown window of its rule.
- **Covers:** Z2-I-PHYS-26
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsInteraction_CooldownPerPair`.
- **Design:** Zenith2/Docs/Design/PHYS/Simulation.md → "Interaction detection"

### Z2-R-PHYS-072 Continuous collision detection

- **Statement:** Bodies with continuous motion quality never pass thin geometry at the speeds of the tunnelling scenario, on both backends.
- **Covers:** Z2-I-PHYS-21
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-003
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsTunnel_ContinuousBulletStops[<Backend>]`, `Test_PhysicsTunnel_ContinuousEnvelopeHolds[<Backend>]` (`Test` step).
- **Design:** Zenith2/Docs/Design/PHYS/Simulation.md → "Continuous collision detection"; Zenith2/Docs/Design/PHYS/Backends.md → "Motion quality"

### Z2-R-PHYS-073 Speculative contacts for discrete bodies

- **Statement:** Discrete bodies at moderate speed do not tunnel through thick walls, through speculative contacts on both backends.
- **Covers:** Z2-I-PHYS-21
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-003
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsTunnel_DiscreteThickWallHolds[<Backend>]`.
- **Design:** Zenith2/Docs/Design/PHYS/Backends.md → "Motion quality"; Zenith2/Docs/Design/PHYS/Tests.md → "Tunnelling tests"

### Z2-R-PHYS-074 Linear speed clamp

- **Statement:** Velocities above `physics.maxLinearSpeed` are clamped and counted.
- **Covers:** Z2-I-PHYS-21
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsTunnel_MaxLinearSpeedClamped[<Backend>]`.
- **Design:** Zenith2/Docs/Design/PHYS/Simulation.md → "Continuous collision detection"

### Z2-R-PHYS-075 Kinematic bodies

- **Statement:** Kinematic bodies reach their tick target at the last substep by interpolation and push dynamic bodies without being pushed.
- **Covers:** Z2-I-PHYS-27
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsSubstep_KinematicTargetInterpolated[<Backend>]`.
- **Design:** Zenith2/Docs/Design/PHYS/Simulation.md → "Kinematic bodies and moving platforms"

### Z2-R-PHYS-076 Moving platforms

- **Statement:** Controllers and resting bodies on a moving or rotating kinematic platform are carried with it and keep their platform-local position.
- **Covers:** Z2-I-PHYS-27, Z2-I-PHYS-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** Z2-B-GAME-001
- **Acceptance evidence:** `Test_CharacterMove_RidesMovingPlatform[<Backend>]`.
- **Design:** Zenith2/Docs/Design/PHYS/Simulation.md → "Kinematic bodies and moving platforms"; Zenith2/Docs/Design/PHYS/CharacterRagdollVehicle.md → "Character move algorithm"

### Z2-R-PHYS-077 Stacking stability

- **Statement:** Stacks and pyramids settle, sleep and hold within the documented tolerances on both backends, and authored bodies with clearance do not pop on their first step.
- **Covers:** Z2-I-PHYS-04, Z2-I-PHYS-22
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-003
- **Budgets:** Z2-B-PHYS-002
- **Acceptance evidence:** The tests of "Stacking stability tests".
- **Design:** Zenith2/Docs/Design/PHYS/Tests.md → "Stacking stability tests"; Zenith2/Docs/Design/PHYS.md → "Spawn clearance rule"

### Z2-R-PHYS-078 Character controller component

- **Statement:** `Zenith.Physics.CharacterController` carries its configuration, input and state properties with the documented phases and modes (velocity, root motion with gravity, root motion).
- **Covers:** Z2-I-PHYS-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** Z2-B-GAME-001
- **Acceptance evidence:** `Test_PhysicsComponents_RegistrationMatchesTables` (`Test` step); the census rows compared by GAME's census test.
- **Design:** Zenith2/Docs/Design/PHYS/ComponentsAPI.md → "Zenith_CharacterController header"; Zenith2/Docs/Design/PHYS/ComponentsAPI.md → "Authority census rows"

### Z2-R-PHYS-079 Character move algorithm

- **Statement:** `Physics.CharacterMove` moves each controller through collision with skin width, step-up, slope limit, depenetration and ground detection, writing its state properties.
- **Covers:** Z2-I-PHYS-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** Z2-B-GAME-001
- **Acceptance evidence:** `Test_CharacterMove_WalksUpStep[<Backend>]`, `Test_CharacterMove_SlopeLimit[<Backend>]`, `Test_PhysicsTunnel_ControllerNeverPassesWall[<Backend>]`.
- **Design:** Zenith2/Docs/Design/PHYS/CharacterRagdollVehicle.md → "Character move algorithm"; Zenith2/Docs/Design/PHYS/CharacterRagdollVehicle.md → "Controller tuning rules"

### Z2-R-PHYS-080 Root motion through the controller

- **Statement:** `Physics.CharacterMove` applies `Zenith.Animation.RootMotion` to every row with a controller (and only those), moving through collision; rows without a controller are left to ANIM's phase-4 node.
- **Covers:** Z2-I-PHYS-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** Z2-B-GAME-001
- **Acceptance evidence:** `Test_CharacterMove_RootMotionThroughCollision[<Backend>]`, `Test_CharacterMove_OnlyControllerRowsConsumeRootMotion`; pending ANIM row PH12.
- **Design:** Zenith2/Docs/Design/PHYS/Simulation.md → "Frame-graph placement"; Zenith2/Docs/Design/PHYS/CharacterRagdollVehicle.md → "Character move algorithm"

### Z2-R-PHYS-081 Controller interaction with bodies

- **Statement:** A controller pushes dynamic bodies with a bounded impulse and is never pushed through static geometry.
- **Covers:** Z2-I-PHYS-10, Z2-I-PHYS-26
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_CharacterMove_PushesDynamicBody[<Backend>]`.
- **Design:** Zenith2/Docs/Design/PHYS/CharacterRagdollVehicle.md → "Controller interaction with bodies"

### Z2-R-PHYS-082 Controller teleport

- **Statement:** A teleported controller clears its ground and platform state and moves from the teleported pose.
- **Covers:** Z2-I-PHYS-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_CharacterMove_TeleportResetsState`.
- **Design:** Zenith2/Docs/Design/PHYS/CharacterRagdollVehicle.md → "Character move algorithm"; Zenith2/Docs/Design/PHYS.md → "Lifecycle listener"

### Z2-R-PHYS-083 Constraint kinds

- **Statement:** Fixed, point, hinge, slider, distance, cone, swing-twist and six-degree-of-freedom constraints with limits map onto both backends as the mapping table states.
- **Covers:** Z2-I-PHYS-11
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-003
- **Budgets:** none
- **Acceptance evidence:** Conformance case `Test_PhysicsConformance_ConstraintLimitsHold[<Backend>]` on the `Joints` scenario.
- **Design:** Zenith2/Docs/Design/PHYS/Simulation.md → "Constraint kinds"; Zenith2/Docs/Design/PHYS/Backends.md → "Constraint mapping"

### Z2-R-PHYS-084 Constraint ordering

- **Statement:** Constraints are solved in creation order from the module's priority counter on both backends.
- **Covers:** Z2-I-PHYS-11, Z2-I-PHYS-23
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsDeterminism_ReplayAcrossWorkerCounts[<Backend>]` (the `Joints` scenario, `Test` step).
- **Design:** Zenith2/Docs/Design/PHYS/Backends.md → "Constraint mapping"; Zenith2/Docs/Design/PHYS/Simulation.md → "Constraint state machine"

### Z2-R-PHYS-085 Joint component

- **Statement:** `Zenith.Physics.Joint` creates and updates a constraint between its entity's body and its target's body with the documented properties and phases.
- **Covers:** Z2-I-PHYS-12
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsJoint_ComponentCreatesConstraint` (`Test` step); the census rows.
- **Design:** Zenith2/Docs/Design/PHYS/ComponentsAPI.md → "Joint properties"; Zenith2/Docs/Design/PHYS/Simulation.md → "Constraint state machine"; Zenith2/Docs/Design/PHYS/Tests.md → "Component and asset tests"

### Z2-R-PHYS-086 Joint motors

- **Statement:** Hinge, slider and six-degree-of-freedom joints drive to velocity or position targets set by scripts in the early-scripts phase.
- **Covers:** Z2-I-PHYS-12
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** Conformance case `Test_PhysicsConformance_MotorReachesTarget[<Backend>]`.
- **Design:** Zenith2/Docs/Design/PHYS/ComponentsAPI.md → "Joint properties"; Zenith2/Docs/Design/PHYS/Simulation.md → "Constraint kinds"

### Z2-R-PHYS-087 Joint breakage

- **Statement:** A joint whose impulse exceeds its threshold breaks after the substep, emits `JointBroken` once and writes its broken property.
- **Covers:** Z2-I-PHYS-12
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** Z2-B-GAME-003
- **Acceptance evidence:** `Test_PhysicsJoints_BreakEmitsOnce`.
- **Design:** Zenith2/Docs/Design/PHYS/Simulation.md → "Joint breakage"

### Z2-R-PHYS-088 Ragdoll asset

- **Statement:** The `Physics.Ragdoll` kind loads `.zragdoll` definitions validated against a skeleton's joint names and hierarchy, cooked from ragdoll documents.
- **Covers:** Z2-I-PHYS-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsCook_RagdollJointMissingRefused`; pending ASSET row PH4.
- **Design:** Zenith2/Docs/Design/PHYS/Formats.md → "Ragdoll file"; Zenith2/Docs/Design/PHYS/Formats.md → "Ragdoll document"; Zenith2/Docs/Design/PHYS/CharacterRagdollVehicle.md → "Ragdoll definition"

### Z2-R-PHYS-089 Ragdoll state machine

- **Statement:** Ragdolls follow the state machine (none, entering, active, recovering), an active ragdoll being one whose State is not NONE, with refused transitions counted (among the causes: no `Zenith.Physics.Ragdoll` at playback and an entity that also carries `Zenith.Physics.RigidBody`), under the active-ragdoll capacity.
- **Covers:** Z2-I-PHYS-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsRagdoll_StateMachineTransitions`, `Test_PhysicsRagdoll_CapacityRefusesEnter`.
- **Design:** Zenith2/Docs/Design/PHYS/CharacterRagdollVehicle.md → "Ragdoll state machine"

### Z2-R-PHYS-090 Ragdoll enter

- **Statement:** Entering a ragdoll creates parts at the animated pose's joint transforms with its joint velocities, dispatches the enter lifecycle event, and leaves the entity animated when refused.
- **Covers:** Z2-I-PHYS-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsRagdoll_EnterFromPoseKeepsVelocity[<Backend>]`; GAME's `Test_Lifecycle_Ragdoll_EnterAndRecoverBlend`.
- **Design:** Zenith2/Docs/Design/PHYS/CharacterRagdollVehicle.md → "Ragdoll enter procedure"

### Z2-R-PHYS-091 Ragdoll blend and root

- **Statement:** `Physics.RagdollBlend` advances blend weights and keeps the entity's root under the ragdoll's root part with writer source Ragdoll, writing `inverse(W_parent) · W_root` with the parent's previous-tick world transform for a parented character.
- **Covers:** Z2-I-PHYS-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** Z2-B-ANIM-003
- **Acceptance evidence:** `Test_PhysicsRagdoll_BlendWritesRootFollowsPart` (`Test` step); ANIM's ragdoll tests.
- **Design:** Zenith2/Docs/Design/PHYS/CharacterRagdollVehicle.md → "Ragdoll blend and root"; Zenith2/Docs/Design/PHYS/Simulation.md → "Frame-graph placement"

### Z2-R-PHYS-092 Ragdoll recover

- **Statement:** Recovering captures and holds the root pose, dispatches the recover lifecycle event before ANIM's listener runs, holds the blend weight for the first `Physics.RagdollBlend` run after the playback and lowers it from the next run, and removes the parts and their constraints when the weight reaches 0; every weight is computed from the run count and the stored playback weight by the pinned binary32 expressions of the state machine, never incrementally.
- **Covers:** Z2-I-PHYS-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsRagdoll_RecoverHoldsRoot`, `Test_PhysicsRagdoll_BlendScheduleClosedForms` (`Test` step); GAME's `Test_Lifecycle_Ragdoll_EnterAndRecoverBlend`.
- **Design:** Zenith2/Docs/Design/PHYS/CharacterRagdollVehicle.md → "Ragdoll recover procedure"; Zenith2/Docs/Design/PHYS/CharacterRagdollVehicle.md → "Ragdoll state machine"

### Z2-R-PHYS-093 Powered ragdolls

- **Statement:** Powered ragdolls drive their joints toward the pose source's target with bounded torque.
- **Covers:** Z2-I-PHYS-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsRagdoll_PoweredFollowsPose[<Backend>]`.
- **Design:** Zenith2/Docs/Design/PHYS/CharacterRagdollVehicle.md → "Powered ragdolls"

### Z2-R-PHYS-094 Ragdoll self-collision

- **Statement:** A ragdoll part never collides with its parent part and every other pair of parts of the ragdoll collides, through the one connected-body rule (CollideConnected false on each part's constraint) on both backends.
- **Covers:** Z2-I-PHYS-14, Z2-I-PHYS-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-003
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsRagdoll_ParentChildNeverCollide[<Backend>]`.
- **Design:** Zenith2/Docs/Design/PHYS/CharacterRagdollVehicle.md → "Ragdoll definition"; Zenith2/Docs/Design/PHYS/Backends.md → "Constraint mapping"; Zenith2/Docs/Design/PHYS/Backends.md → "Jolt layer mapping"

### Z2-R-PHYS-095 Ragdoll pose source and body poses

- **Statement:** ANIM installs a `Zenith_RagdollPoseSource` on the world resource, and the resource returns each ragdoll body's world pose by joint name hash after the step.
- **Covers:** Z2-I-PHYS-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** Z2-B-ANIM-003
- **Acceptance evidence:** `Test_PhysicsRagdoll_BodyPosesByJointHash` (`Test` step); ANIM's ragdoll transition tests.
- **Design:** Zenith2/Docs/Design/PHYS/ComponentsAPI.md → "Zenith_Ragdoll header"; Zenith2/Docs/Design/PHYS/ComponentsAPI.md → "Zenith_PhysicsWorldResource header"

### Z2-R-PHYS-096 Teleport during a ragdoll

- **Statement:** A teleport of a ragdolled entity moves every part rigidly by the offset, zeroes velocities and leaves the blend weight unchanged.
- **Covers:** Z2-I-PHYS-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** GAME's `Test_Scenario_TeleportDuringRagdoll` with PHYS's assertions.
- **Design:** Zenith2/Docs/Design/PHYS/CharacterRagdollVehicle.md → "Teleport during a ragdoll"

### Z2-R-PHYS-097 Physics debug views

- **Statement:** Each debug view bit draws its geometry from neutral data through the debug-draw recorder in development builds and draws nothing in Shipping.
- **Covers:** Z2-I-PHYS-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsDebug_ViewsRecordGeometry` (`Test` step), recording into a test `Zenith_DebugDraw`; pending MOD row PH7.
- **Design:** Zenith2/Docs/Design/PHYS.md → "Debug views"; Zenith2/Docs/Design/PHYS/ComponentsAPI.md → "Zenith_PhysicsDebug header"

### Z2-R-PHYS-098 Physics dump

- **Statement:** `physics.dump` writes every body, constraint, controller and ragdoll in simulation-ID order with the canonical state hash.
- **Covers:** Z2-I-PHYS-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsDump_SortedAndHashed` in `ZenithPhysics_Tests`.
- **Design:** Zenith2/Docs/Design/PHYS.md → "Physics dump command"

### Z2-R-PHYS-099 Counters and zones

- **Statement:** The module publishes the counters of the table with their kinds and opens the documented profiler zones, over which Z2-B-PHYS-002 is measured.
- **Covers:** Z2-I-PHYS-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** Z2-B-PHYS-002
- **Acceptance evidence:** `Test_PhysicsCounters_RegisteredWithKinds` (`Test` step); the budget harness reads the zones.
- **Design:** Zenith2/Docs/Design/PHYS.md → "Counters"; Zenith2/Docs/Design/PHYS.md → "Profiler zones"

### Z2-R-PHYS-100 Physics save participant

- **Statement:** The module registers one save participant per world that saves the section of its layout; its flags are 0 on Jolt and `uSAVE_PARTICIPANT_FLAG_COHERENT` on PhysX, never the exclude-from-resume flag, so SAVE's default capture fidelity reports EXACT and COHERENT; it declares `ASYNC_SOURCE_PHYSICS_QUERY` for re-issue, and restores EXACT on Jolt and COHERENT on PhysX, calling the world's mutating members on the main thread out of tick and running the restore's ingest serially.
- **Covers:** Z2-I-PHYS-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsSave_FlagsAndFidelity`, `Test_PhysicsConformance_SaveRestoreExactContinuation[Jolt]`, `Test_PhysicsConformance_SaveRestoreCoherent[PhysX]` (`Test` step), SAVE's `Test_SaveCrossSystem_PhysicsAndAnimationCoherent`.
- **Design:** Zenith2/Docs/Design/PHYS/Simulation.md → "Save and restore algorithm"; Zenith2/Docs/Design/PHYS/Backends.md → "Jolt state save and restore"; Zenith2/Docs/Design/PHYS/Backends.md → "PhysX state save and restore"

### Z2-R-PHYS-101 Save section format

- **Statement:** The physics save section has the documented layout, including the applied origin, the slot tables, the controller block's exact fields, each pair's suspended count, the ended list and the INBOX entries alone, refuses future versions, migrates every older version from bytes its migration test generates at test time (no committed binary), and falls back to a coherent restore, reported to SAVE, when an EXACT blob does not match.
- **Covers:** Z2-I-PHYS-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsSave_SectionRoundTrip`, `Test_PhysicsSave_FutureVersionRefused`, `Test_PhysicsSave_ExactBlobMismatchFallsBack` (`Test` step).
- **Design:** Zenith2/Docs/Design/PHYS/Formats.md → "Save section layout"

### Z2-R-PHYS-102 Script bindings

- **Statement:** The `Physics` Luau namespace exposes the documented functions, which validate their arguments before anything else and raise the catalogued controlled Luau error for every misuse class of the code table (never an assert, never a silent false), and record commands for every mutation; `READ_WORLD` functions raise SCRIPT's ZS1101 and ZS1102 for stale and foreign entity arguments, and `Physics.GetVelocity` of a live bodiless entity returns two zero vectors.
- **Covers:** Z2-I-PHYS-08, Z2-I-PHYS-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsBindings_MisuseRaisesCatalogueError`, `Test_PhysicsIntegration_ScriptQueriesAndImpulses` (`Test` step); core-doc item X264 (k) for the code block.
- **Design:** Zenith2/Docs/Design/PHYS/ComponentsAPI.md → "Script bindings"

### Z2-R-PHYS-103 Physics integration suite

- **Statement:** The integration suite runs authored scenes, scripts, streaming and save/load on the composed runtime with every ECS backend and both physics backends, and every module-level per-backend case of the catalogue as `<name>[Jolt]` and `<name>[PhysX]`.
- **Covers:** Z2-I-PHYS-01, Z2-I-PHYS-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-003
- **Budgets:** none
- **Acceptance evidence:** The tests of `Zenith_Integration_Physics` (`Test` step).
- **Design:** Zenith2/Docs/Design/PHYS/Tests.md → "Integration suite"

### Z2-R-PHYS-104 Vehicles

- **Statement:** The vehicle component simulates up to 8 ray-cast wheels with suspension, tyre friction, engine, gears and brakes in neutral code before each substep.
- **Covers:** Z2-I-PHYS-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-003
- **Budgets:** Z2-B-PHYS-002
- **Acceptance evidence:** `Test_PhysicsVehicle_StraightLineAcceleration[<Backend>]`, `Test_PhysicsVehicle_SuspensionRestHeight[<Backend>]`.
- **Design:** Zenith2/Docs/Design/PHYS/CharacterRagdollVehicle.md → "Vehicle algorithm"; Zenith2/Docs/Design/PHYS/ComponentsAPI.md → "Zenith_PhysicsVehicle header"

### Z2-R-PHYS-105 Buoyancy

- **Statement:** Bodies with the buoyancy component integrate buoyant force and drag against `Zenith_FluidQuery` before each substep, from the samples cooked with a single cooked collider or, for every other shape, from lattice samples computed at composition into the world's own table, never from the process's primitive cache.
- **Covers:** Z2-I-PHYS-26
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-003
- **Budgets:** Z2-B-PHYS-002
- **Acceptance evidence:** `Test_PhysicsBuoyancy_FloatsAtDensityRatio[<Backend>]`, `Test_PhysicsBuoyancy_SamplesSumToVolume`, `Test_FluidQuery_AbsentResourceNoBuoyancy`.
- **Design:** Zenith2/Docs/Design/PHYS/Simulation.md → "Buoyancy algorithm"; Zenith2/Docs/Design/PHYS/Formats.md → "Buoyancy samples"; Zenith2/Docs/Design/PHYS/Simulation.md → "Module world state"; Zenith2/Docs/Design/PHYS/ComponentsAPI.md → "Buoyancy properties"

### Z2-R-PHYS-106 Fluid query interface

- **Statement:** `Zenith_FluidQuery` is declared in ZenithScene (SCENE's header) and implemented by ENV, so buoyancy reads water without a module-to-module edge, and a world without the resource integrates buoyancy bodies as plain bodies.
- **Covers:** Z2-I-PHYS-26
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-024
- **Budgets:** none
- **Acceptance evidence:** `Test_FluidQuery_AbsentResourceNoBuoyancy` (`Test` step); SCENE's header (row PH8, applied by SCENE); `Libgraph` shows no Physics–Environment edge.
- **Design:** Zenith2/Docs/Design/PHYS/ComponentsAPI.md → "Zenith_FluidQuery"; Zenith2/Docs/Design/SCENE/ModuleSeams.md → "Zenith_FluidQuery header"

### Z2-R-PHYS-107 Streaming cell bodies

- **Statement:** Static colliders and triggers of COLLISION-grid cell content exist exactly between their cell's activation and deactivation sync points.
- **Covers:** Z2-I-PHYS-16
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** Z2-B-PHYS-001
- **Acceptance evidence:** `Test_PhysicsCells_CellContentBodiesFollowSyncPoints`, `Test_PhysicsIntegration_StreamingCellsAndFreeze`.
- **Design:** Zenith2/Docs/Design/PHYS/Simulation.md → "Cell bodies"

### Z2-R-PHYS-108 Region freeze

- **Statement:** A dynamic body whose entity is outside the active region is frozen (kinematic, asleep, query-only filtering so it collides with nothing and overlaps no trigger, velocities saved, pairs ended) and restored exactly, with its own filtering, when the region returns; a ragdoll whose character leaves the active region freezes every part the same way and holds its state, weight and run count until the thaw.
- **Covers:** Z2-I-PHYS-16
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsFreeze_RegionBoundFreezesAndRestores` (bodies and a ragdoll), `Test_PhysicsFreeze_FrozenBodyCollidesWithNothing[<Backend>]` (`Test` step).
- **Design:** Zenith2/Docs/Design/PHYS/Simulation.md → "Region freeze"

### Z2-R-PHYS-109 Origin shift

- **Statement:** On a world-origin rebase the backend world, every module pose and the controller targets shift at the sync point where SCENE plays the shift, by SCENE's formula, so every phase-2 to phase-4 reader of the rebase tick sees a consistent world; resting contacts and query results are preserved; the applied origin is stored as three doubles beside the rebase count.
- **Covers:** Z2-I-PHYS-28, Z2-I-PHYS-16
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsConformance_OriginShiftPreservesRestingStack[<Backend>]`, `Test_PhysicsLarge_QueriesAfterRebase[<Backend>]`, `Test_PhysicsLarge_QueriesConsistentInRebaseTick[<Backend>]`, `Test_PhysicsLarge_ShiftedStateMatchesSceneFormula`, `Test_PhysicsIntegration_OriginShiftAtSyncPoint` (`Test` step).
- **Design:** Zenith2/Docs/Design/PHYS/Simulation.md → "Origin shift"; Zenith2/Docs/Design/PHYS/Backends.md → "Origin shift on Jolt"; Zenith2/Docs/Design/PHYS/Simulation.md → "Module world state"

### Z2-R-PHYS-110 Large coordinates

- **Statement:** Single-precision simulation inside STREAM's 4,096 m precision radius meets the stacking tolerances unchanged, simulation out to 16,384 m without rebasing meets them with 2 mm contact tolerances, and rebased worlds use STREAM's keys (`stream.rebase.thresholdMetres`, `stream.rebase.quantumMetres`, `stream.rebase.minIntervalTicks`), never PHYS constants.
- **Covers:** Z2-I-PHYS-28
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsLarge_PrecisionInsideRadius[<Backend>]`, `Test_PhysicsLarge_StabilityBeyondRadius[<Backend>]`, `Test_PhysicsLarge_RebaseKeepsContacts[<Backend>]`, `Test_PhysicsIntegration_RebaseParametersMatchStream` (`Test` step).
- **Design:** Zenith2/Docs/Design/PHYS/Simulation.md → "Large coordinates"; Zenith2/Docs/Design/PHYS/Tests.md → "Large-coordinate tests"

### Z2-R-PHYS-111 Specialist simulations per classification

- **Statement:** Every capability the D-CLASS-SIM ruling classifies as required and assigns to PHYS gets its own design addendum in the PHYS family and is implemented behind the neutral API with conformance cases on both backends.
- **Covers:** Z2-I-PHYS-20
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-084, Z2-D-003
- **Budgets:** none
- **Acceptance evidence:** The addendum and its conformance cases, once the ruling exists.
- **Design:** Zenith2/Docs/Design/PHYS.md → "Stage table"; Zenith2/Docs/Design/PHYS.md → "Open decisions"

### Z2-R-PHYS-112 Physics budget qualification

- **Statement:** Z2-B-PHYS-001 and Z2-B-PHYS-002 are measured on the S24 and the reference desktop for the selected backend of each platform on every listed workload and meet their values.
- **Covers:** Z2-I-PHYS-02, Z2-I-PHYS-03
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-084
- **Budgets:** Z2-B-PHYS-001, Z2-B-PHYS-002
- **Acceptance evidence:** The qualification run of `Zenith_Bench_BudgetsPHYS` attached to the Stage 6 acceptance ticket.
- **Design:** Zenith2/Docs/Design/PHYS.md → "Rows owned"; Zenith2/Docs/Design/PHYS/Tests.md → "Benchmarks and budget evidence"

### Z2-R-PHYS-113 Cooked reader fuzzing

- **Statement:** The `.zcol`, `.zpmat` and `.zragdoll` readers run under the programme's fuzzing rules without a crash, assert or sanitizer report.
- **Covers:** Z2-I-PHYS-06
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Zenith_Fuzz_PhysicsCooked` runs in the sanitizer preset.
- **Design:** Zenith2/Docs/Design/PHYS/Tests.md → "Cook and format tests"

### Z2-R-PHYS-114 Connected-body collision on both backends

- **Statement:** A constraint with CollideConnected false keeps its two bodies from colliding on both backends (Jolt through the world's excluded-pair group filter, PhysX through `eCOLLISION_ENABLED` cleared) and one with true lets them collide.
- **Covers:** Z2-I-PHYS-11, Z2-I-PHYS-12, Z2-I-PHYS-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-003
- **Budgets:** none
- **Acceptance evidence:** Conformance case `Test_PhysicsConformance_JointCollideConnected[<Backend>]`, `Test_PhysicsJolt_GroupFilterRules` (`Test` step).
- **Design:** Zenith2/Docs/Design/PHYS/Backends.md → "Constraint mapping"; Zenith2/Docs/Design/PHYS/Backends.md → "Jolt layer mapping"; Zenith2/Docs/Design/PHYS/Simulation.md → "Ingest algorithm"; Zenith2/Docs/Design/PHYS/Simulation.md → "Excluded pairs maintenance"

### Z2-R-PHYS-115 Collision groups on both backends

- **Statement:** Two bodies of the same non-zero collision group never collide when either carries IGNORE_SAME_GROUP, on Jolt (group filter) and PhysX (filter shader word2 and word3 bit 4) alike, and a query with a non-zero `m_uIgnoreGroup` skips bodies of that group on both.
- **Covers:** Z2-I-PHYS-07, Z2-I-PHYS-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-003
- **Budgets:** none
- **Acceptance evidence:** Conformance cases `Test_PhysicsConformance_CollisionGroupIgnoresSameGroup[<Backend>]`, `Test_PhysicsConformance_QueryIgnoreGroup[<Backend>]` (`Test` step).
- **Design:** Zenith2/Docs/Design/PHYS/Backends.md → "Jolt layer mapping"; Zenith2/Docs/Design/PHYS/Backends.md → "PhysX filter shader"; Zenith2/Docs/Design/PHYS/Backends.md → "PhysX scene query filter"

### Z2-R-PHYS-116 Query-only bodies identical on both backends

- **Statement:** A query-only body never collides and never overlaps a trigger, and is found by every query exactly when its filter includes query-only bodies, on both backends.
- **Covers:** Z2-I-PHYS-07, Z2-I-PHYS-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-003
- **Budgets:** none
- **Acceptance evidence:** Conformance case `Test_PhysicsConformance_QueryOnlyVisibleToQueries[<Backend>]` (`Test` step).
- **Design:** Zenith2/Docs/Design/PHYS/Backends.md → "Jolt layer mapping"; Zenith2/Docs/Design/PHYS/Backends.md → "PhysX scene query filter"; Zenith2/Docs/Design/PHYS/PublicAPI.md → "Zenith_PhysicsQuery header"

### Z2-R-PHYS-117 Trigger bodies kinematic and awake

- **Statement:** Every trigger body is created kinematic and discrete and never sleeps whatever its authored motion type or motion quality, so it reports overlaps with sleeping bodies and its reach is identical on both backends; scene validation warns about a dynamic rigid body whose own collider is a trigger.
- **Covers:** Z2-I-PHYS-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-003
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsTriggers_SleepingBodyStaysInside`, conformance case `Test_PhysicsConformance_TriggerReachMatchesRule[<Backend>]`, `Test_PhysicsIntegration_SceneBodiesFromAuthoredScene` (its `PHYSICS_COOK_DIAG_DYNAMIC_TRIGGER` assertion) (`Test` step).
- **Design:** Zenith2/Docs/Design/PHYS/Simulation.md → "Body composition"; Zenith2/Docs/Design/PHYS/Simulation.md → "Trigger semantics"; Zenith2/Docs/Design/PHYS/Backends.md → "Jolt layer mapping"; Zenith2/Docs/Design/PHYS.md → "Spawn clearance rule"

### Z2-R-PHYS-118 Dynamic bodies under moving parents

- **Statement:** A dynamic body is simulated in world space and never follows a moving parent; its local transform is rewritten when the parent moves, with at most a one-tick lag in its world transform, no external-write count, and a scene-validation warning for such authoring.
- **Covers:** Z2-I-PHYS-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsSync_DynamicBodyIgnoresMovingParent` (`Test` step); the cook's `PHYSICS_COOK_DIAG_DYNAMIC_UNDER_MOVING_PARENT` warning in `Test_PhysicsIntegration_SceneBodiesFromAuthoredScene`.
- **Design:** Zenith2/Docs/Design/PHYS/Simulation.md → "External writes"; Zenith2/Docs/Design/PHYS/Simulation.md → "Physics-to-transform sync"; Zenith2/Docs/Design/PHYS.md → "Spawn clearance rule"

### Z2-R-PHYS-119 Parented controllers adopt in local space

- **Statement:** A character controller adopts an external pose only when its observed local transform differs from the local bits PHYS wrote, starting otherwise from its exact world target, so a controller under a static parent is never adopted by recomposition rounding.
- **Covers:** Z2-I-PHYS-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsIngest_ParentedBodyNoSpuriousExternalWrite` (`Test` step).
- **Design:** Zenith2/Docs/Design/PHYS/CharacterRagdollVehicle.md → "Character move algorithm"; Zenith2/Docs/Design/PHYS/Simulation.md → "External writes"

### Z2-R-PHYS-120 Origin shift at the rebase sync point

- **Statement:** PHYS's lifecycle listener shifts the backend world on `LIFECYCLE_EVENT_ORIGIN_SHIFTED` at the sync point that plays `Scene.ShiftWorldOrigin`, before any node of the next phase; the ingest applies a missed shift late only as a counted defect; and a shift component above 2^24 m is applied once with one rounding, counted, and never re-applied.
- **Covers:** Z2-I-PHYS-28, Z2-I-PHYS-16
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsIntegration_OriginShiftAtSyncPoint`, `Test_PhysicsLarge_QueriesConsistentInRebaseTick[<Backend>]`, `Test_PhysicsLarge_ShiftedStateMatchesSceneFormula` (`Test` step); SCENE's playback and dispatch (row PH20, applied) and core-doc item X264 (j).
- **Design:** Zenith2/Docs/Design/PHYS/Simulation.md → "Origin shift"; Zenith2/Docs/Design/PHYS/Simulation.md → "Ingest algorithm"; Zenith2/Docs/Design/PHYS.md → "Lifecycle listener"

### Z2-R-PHYS-121 World-anchored joints follow their entity

- **Statement:** With BodyB null a joint's frame B is relative to the joint entity's world transform at creation, and an origin shift moves the backend's world-anchored frames by the same shift as the bodies.
- **Covers:** Z2-I-PHYS-12, Z2-I-PHYS-28
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsConformance_OriginShiftPreservesRestingStack[<Backend>]` (its world-anchored hinge, `Test` step).
- **Design:** Zenith2/Docs/Design/PHYS/ComponentsAPI.md → "Joint properties"; Zenith2/Docs/Design/PHYS/Backends.md → "Origin shift on Jolt"; Zenith2/Docs/Design/PHYS/Simulation.md → "Ingest algorithm"

### Z2-R-PHYS-122 Deterministic slot allocation

- **Statement:** Body and constraint handles are the lowest free index and that slot's next generation, a function of the live set and the saved per-slot generations alone, never of destruction order.
- **Covers:** Z2-I-PHYS-05, Z2-I-PHYS-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsTable_SlotAllocationLowestFree` (`Test` step).
- **Design:** Zenith2/Docs/Design/PHYS/Simulation.md → "Slot allocation"; Zenith2/Docs/Design/PHYS/Simulation.md → "Determinism rules"

### Z2-R-PHYS-123 Slot tables saved and restored

- **Statement:** The save section carries both slot tables, and a restore sets them before re-creating any handle, so the first creation after a load receives the handle the uninterrupted run gives it.
- **Covers:** Z2-I-PHYS-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsTable_SlotTablesRoundTrip` (`Test` step).
- **Design:** Zenith2/Docs/Design/PHYS/Simulation.md → "Slot allocation"; Zenith2/Docs/Design/PHYS/Formats.md → "Save section layout"; Zenith2/Docs/Design/PHYS/PublicAPI.md → "Zenith_PhysicsWorld header"

### Z2-R-PHYS-124 Jolt bodies created from neutral IDs

- **Statement:** The Jolt backend creates every body on every path with the BodyID derived from its neutral handle and never calls Jolt's own BodyID allocator.
- **Covers:** Z2-I-PHYS-02, Z2-I-PHYS-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsJolt_BodiesCreatedWithNeutralIDs` (`Test` step).
- **Design:** Zenith2/Docs/Design/PHYS/Backends.md → "Jolt state save and restore"; Zenith2/Docs/Design/PHYS/Simulation.md → "Slot allocation"

### Z2-R-PHYS-125 EXACT continuation through creations after a load

- **Statement:** A Jolt EXACT restore continues bit-identically through destructions before the save and creations after the restore, with constraint states saved and restored by priority rather than by Jolt's manager order.
- **Covers:** Z2-I-PHYS-05, Z2-I-PHYS-29
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsConformance_SaveRestoreExactContinuation[Jolt]` (`Test` step).
- **Design:** Zenith2/Docs/Design/PHYS/Backends.md → "Jolt state save and restore"; Zenith2/Docs/Design/PHYS/Simulation.md → "Determinism boundaries"

### Z2-R-PHYS-126 Restore waits for resident shapes

- **Statement:** The physics participant creates restored bodies only when every recorded collider and ragdoll asset is ACTIVE or FAILED, reporting the waits it holds, and a failed shape leaves its entity bodiless and the restore reported coherent.
- **Covers:** Z2-I-PHYS-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsSave_RestoreWaitsForShapes` (`Test` step).
- **Design:** Zenith2/Docs/Design/PHYS/Simulation.md → "Save and restore algorithm"

### Z2-R-PHYS-127 Restore fallback reported to SAVE

- **Statement:** An EXACT restore that falls back to the coherent path, for a mismatched blob or a failed shape, is reported to SAVE through `ReportCoherentRestore`, so the restored session is not treated as EXACT.
- **Covers:** Z2-I-PHYS-05, Z2-I-PHYS-29
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsSave_ExactBlobMismatchFallsBack` (`Test` step); pending SAVE row PH22.
- **Design:** Zenith2/Docs/Design/PHYS/Simulation.md → "Save and restore algorithm"; Zenith2/Docs/Design/PHYS.md → "Failure table"

### Z2-R-PHYS-128 Command playback finds its world by tag

- **Statement:** Every PHYS command playback function finds the per-world module state by `Zenith_WorldPlaybackAccess::GetWorldTag`, so a command never changes another world's state, and a command for a detached world is dropped and counted.
- **Covers:** Z2-I-PHYS-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsCommands_PlaybackFindsWorldByTag` (`Test` step).
- **Design:** Zenith2/Docs/Design/PHYS/Simulation.md → "Module world state"; Zenith2/Docs/Design/PHYS.md → "Command kinds"

### Z2-R-PHYS-129 Physics misuse codes

- **Statement:** PHYS's binding errors use the misuse-catalogue block ZS2300–ZS2399 that the orchestrator's MISUSE-BLOCKS-2 ruling assigns to PHYS (codes ZS2301–ZS2305), and SCRIPT's codes for the conditions the dispatcher and loader detect.
- **Covers:** Z2-I-PHYS-08, Z2-I-PHYS-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsBindings_MisuseRaisesCatalogueError` (`Test` step); the catalogue row of core-doc item X264 (k).
- **Design:** Zenith2/Docs/Design/PHYS/ComponentsAPI.md → "Script bindings"; Zenith2/Docs/Design/PHYS.md → "Luau errors and asserts"

### Z2-R-PHYS-130 Heightfield runtime fidelity per backend

- **Statement:** Each backend's runtime heightfield matches the neutral diagonal-0 surface: exactly on PhysX (16-bit samples, tessellation flag, per-triangle holes) and on Jolt within the error the loader measures for that shape, the maximum over every sample of the difference between Jolt's reconstructed height (`HeightFieldShape::GetPosition`) and the neutral decoded height, which covers both of Jolt's quantisation stages, is stored in the shape info and reported; holes are a rejected material on Jolt with the contact-validation results stated per pair; palette indices are preserved and no hit lands in a hole.
- **Covers:** Z2-I-PHYS-06, Z2-I-PHYS-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-003
- **Budgets:** none
- **Acceptance evidence:** Conformance case `Test_PhysicsConformance_HeightfieldMatchesNeutral[<Backend>]` with its cliff tile and negative control, `Test_PhysicsJolt_HeightfieldErrorMeasured`, `Test_PhysicsJolt_ContactValidateReturnValues` (`Test` step).
- **Design:** Zenith2/Docs/Design/PHYS/Backends.md → "Heightfield builds"; Zenith2/Docs/Design/PHYS/Formats.md → "Neutral geometry section"; Zenith2/Docs/Design/PHYS/Backends.md → "Jolt contact listener"

### Z2-R-PHYS-131 Square heightfields with a 127-entry palette

- **Statement:** A cooked heightfield is square and its palette has at most 127 entries; the cook refuses other inputs with a message naming them.
- **Covers:** Z2-I-PHYS-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsCook_HeightfieldPaletteAndSquareRefused` (`Test` step); pending ENV row PH21.
- **Design:** Zenith2/Docs/Design/PHYS/Formats.md → "Neutral geometry section"; Zenith2/Docs/Design/PHYS/Formats.md → "Heightfield intermediate"; Zenith2/Docs/Design/PHYS/Formats.md → "Parser limits"

### Z2-R-PHYS-132 Multithreaded Jolt benchmark measurement

- **Statement:** The D-SEL-PHYSX-ANDROID harness measures Jolt stepped with `JPH::JobSystemThreadPool` at 1, 2, 4 and all worker threads beside the single-threaded figure, for W-Dense and W-OpenWorld on Windows and the S24, benchmark-only, and reports the PHYS-1 trigger's verdict.
- **Covers:** Z2-I-PHYS-02
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-084, Z2-D-006
- **Budgets:** Z2-B-PHYS-002
- **Acceptance evidence:** The harness report `Zenith2/out/<checkout-hash>/bench/PhysicsBackends/<platform>.json` with the thread-pool figures, attached to the `needs-human` harness-run ticket; the loop harness ticket's `--smoke` CTest covers the thread-pool rows at 1 and 2 threads (`Test` step).
- **Design:** Zenith2/Docs/Design/PHYS/Backends.md → "PhysX on Android evaluation"; Zenith2/Docs/Design/PHYS/Backends.md → "Conditional Jolt parallel step"

### Z2-R-PHYS-133 Conditional Jolt parallel step

- **Statement:** When the Stage 1 report shows single-threaded Jolt within Z2-B-PHYS-002 on the S24 for W-Dense and W-OpenWorld, PHYS-1 is recorded as rejected and Jolt stays single-threaded; otherwise the exemption is recorded and built exactly as pre-specified: a `Zenith_JoltJobSystem` derived from `JPH::JobSystem` with its own barrier counter, Jolt's dynamically queued jobs drained by a growing set of non-waiting sub-jobs of the step node with a fixed job concurrency, a help-while-wait that runs only its own barrier's jobs and spins with `Zenith_YieldThread` only while one of them runs elsewhere, Jolt-internal only, no lock held across a wait and no blocking primitive, reported to the blocking-wait hook as `physics-barrier`, the barrier counter incremented before `SetBarrier`, the spawn rule run only where a job context is known, lock row 520, the contact callbacks' writes to the sink and the removed-contact staging made under lock row 530, and stress and determinism tests at worker counts 1, 2, 8 and all, including a dropping sink whose kept records are identical at every worker count.
- **Covers:** Z2-I-PHYS-02, Z2-I-PHYS-22
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-084
- **Budgets:** Z2-B-PHYS-002
- **Acceptance evidence:** The PHYS-1 ruling entry in `Zenith2/Docs/DecisionLog.md` (the `needs-human` ruling ticket) citing the report; if admitted, `Test_PhysicsJolt_JobSystemQueuesReadyJobs`, `Test_PhysicsJolt_ParallelStepStressAcrossWorkerCounts` and `Test_PhysicsDeterminism_ReplayAcrossWorkerCounts[Jolt]` (`Test` step).
- **Design:** Zenith2/Docs/Design/PHYS/Backends.md → "Conditional Jolt parallel step"; Zenith2/Docs/Design/PHYS.md → "Conditional decision PHYS-1"

### Z2-R-PHYS-134 PhysX dispatcher progress at every worker count

- **Statement:** The PhysX step completes at worker counts 1, 2, 8 and all while its reported worker count stays at its maximum, with identical results, and the pinning ticket records every in-task wait of the pinned PhysX source with the reason it terminates.
- **Covers:** Z2-I-PHYS-03, Z2-I-PHYS-22
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsPhysX_DispatcherStressAcrossWorkerCounts` (`Test` step); the audit table in the `needs-human` pinning ticket's design-doc diff.
- **Design:** Zenith2/Docs/Design/PHYS/Backends.md → "PhysX step and the dispatcher"

### Z2-R-PHYS-135 Concurrent primitive cache under stress

- **Statement:** Under concurrent queries from 16 leaves that fill and evict the primitive cache, every shape a query uses stays valid until the query unpins it, the cache never exceeds 4,096 entries, every pin is released, and every uncached or discarded shape is released.
- **Covers:** Z2-I-PHYS-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsQuery_PrimitiveCacheConcurrentStress` (`Test` step).
- **Design:** Zenith2/Docs/Design/PHYS/Tests.md → "Gate steps for PHYS diffs"; Zenith2/Docs/Design/PHYS/Simulation.md → "Query execution"; Zenith2/Docs/Design/PHYS/Simulation.md → "Primitive cache"

### Z2-R-PHYS-136 Excluded pairs follow every destruction path

- **Statement:** Every creation or destruction of a CollideConnected-false constraint on any path (ingest, the ragdoll enter and removal playbacks, the destroyed-entity listener, region freeze and thaw, restore) marks the world's excluded pairs dirty, and `Physics.Step`'s ingest rebuilds and sets them, as its only caller of `SetExcludedPairs`, before the tick's first step, so two new bodies that reuse freed indices collide on both backends.
- **Covers:** Z2-I-PHYS-11, Z2-I-PHYS-12, Z2-I-PHYS-14, Z2-I-PHYS-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-003, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsIngest_ExcludedPairsDirtyOnEveryPath`, conformance case `Test_PhysicsConformance_ReusedIndicesCollide[<Backend>]`, `Test_PhysicsJolt_ExcludedPairsFollowEveryDestructionPath` (integration suite) (`Test` step).
- **Design:** Zenith2/Docs/Design/PHYS/Simulation.md → "Excluded pairs maintenance"; Zenith2/Docs/Design/PHYS/Simulation.md → "Ingest algorithm"; Zenith2/Docs/Design/PHYS/PublicAPI.md → "Zenith_PhysicsWorld header"

### Z2-R-PHYS-137 Ragdoll part identity

- **Statement:** A ragdoll part body carries its character's simulation ID as user data and its part index + 1 as a part code kept per body slot and stamped into every contact record; parts live in the ragdoll record, never in the body table; the contact record key orders by (user data, part code) and is a total order; contacts between two parts of one ragdoll raise no event; a part's contacts form the character's pairs, so no event names one entity twice; query hits on a part map to the character. A controller is refused on an entity that carries its own rigid body or collider, so no two live bodies share (user data, part code).
- **Covers:** Z2-I-PHYS-14, Z2-I-PHYS-23, Z2-I-PHYS-24
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsRagdoll_PartContactsKeyedTotally`, `Test_PhysicsRagdoll_SelfContactsEmitNoEvents[<Backend>]`, `Test_PhysicsQuery_HitOnPartMapsToCharacter`, `Test_PhysicsTable_SlotIdentityKeptUntilReuse`, `Test_PhysicsController_RefusedOnBodyEntity` (`Test` step).
- **Design:** Zenith2/Docs/Design/PHYS/Simulation.md → "Ragdoll part identity"; Zenith2/Docs/Design/PHYS/Simulation.md → "Event emission"; Zenith2/Docs/Design/PHYS/Backends.md → "Body identity on both backends"; Zenith2/Docs/Design/PHYS/CharacterRagdollVehicle.md → "Ragdoll definition"; Zenith2/Docs/Design/PHYS/Simulation.md → "Body composition"; Zenith2/Docs/Design/PHYS/CharacterRagdollVehicle.md → "Controller tuning rules"

### Z2-R-PHYS-138 Ragdolls saved and restored mid-fall

- **Statement:** The save section records each ragdoll part's handles, constraint priority, motion type, pose, velocities and sleep state, and the restore re-creates every part and its constraint with the recorded handles and priority before the EXACT blob is applied, so a ragdoll saved in mid-fall continues on both backends (bit-identically on Jolt).
- **Covers:** Z2-I-PHYS-14, Z2-I-PHYS-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-003
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsSave_RagdollMidFallRestores[<Backend>]`, `Test_PhysicsSave_SectionRoundTrip` (`Test` step).
- **Design:** Zenith2/Docs/Design/PHYS/Simulation.md → "Save and restore algorithm"; Zenith2/Docs/Design/PHYS/Formats.md → "Save section layout"

### Z2-R-PHYS-139 Module-level cases run per backend

- **Statement:** Every case that needs components, the module's nodes, sync points, the lifecycle listener, the pose source, SCENE's playback, ECS layout or the simulation clock runs in the integration suite once per physics backend through the header-only test world built on MOD's runtime test access, while backend-level cases run in the conformance target over `Zenith_PhysicsWorld` alone.
- **Covers:** Z2-I-PHYS-04, Z2-I-PHYS-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-003, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** CTest registration of each `[<Backend>]` case of `Zenith_Integration_Physics` as `<name>[Jolt]` and `<name>[PhysX]`; `Test_PhysicsDeterminism_ReplayAcrossWorkerCounts[<Backend>]` (`Test` step).
- **Design:** Zenith2/Docs/Design/PHYS/Tests.md → "Test levels"; Zenith2/Docs/Design/PHYS/Tests.md → "Test targets"; Zenith2/Docs/Design/PHYS/Tests.md → "Test support"

### Z2-R-PHYS-140 Frame cancellation never splits a step or a park

- **Statement:** PHYS checks frame cancellation only in `Physics.Step` after a substep has completed, never with a step pending, and then skips the remaining substeps, write-back and emission, after which the world is not stepped again before `DetachWorld`; `Physics.DeferredQueries` removes an inbox entry only when it parks it; so a cancelled frame leaves no PhysX step without its completion and no executed query unparked.
- **Covers:** Z2-I-PHYS-22, Z2-I-PHYS-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsStep_CancelAfterSubstepDetachesClean`, the cancelled-wave case of `Test_PhysicsPhysX_ResumeStepUntilComplete`, `Test_PhysicsDeferred_CancelStress1000Cycles` (its cancelled-frame cycles) (`Test` step).
- **Design:** Zenith2/Docs/Design/PHYS.md → "Cancellation"; Zenith2/Docs/Design/PHYS/Simulation.md → "Deferred queries"; Zenith2/Docs/Design/PHYS/Simulation.md → "Deferred query state machine"

### Z2-R-PHYS-141 Physical-material names

- **Statement:** ENV's string physical-material names resolve at PHYS's heightfield and compound cook steps to the `Physics.Material` asset at `content:/Physics/Materials/<N>.physmat.json`, whose persistent ID enters the cooked palette as a hard reference, an unresolved name failing the cook item with a diagnostic naming it; the 32-bit physical-material ID of a name is `Zenith_MakePhysicsMaterialNameID`, and the intermediate writers take names and refuse invalid names, unbound IDs and colliding IDs.
- **Covers:** Z2-I-PHYS-25, Z2-I-PHYS-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsCook_MaterialNamesResolveByTable` (`ZenithCook_Tests`), `Test_PhysicsMaterial_NameIDFormula` (`Test` step); pending ASSET row PH26.
- **Design:** Zenith2/Docs/Design/PHYS/Formats.md → "Physical-material names"; Zenith2/Docs/Design/PHYS/Formats.md → "Heightfield intermediate"; Zenith2/Docs/Design/PHYS/Formats.md → "Compound intermediate"; Zenith2/Docs/Design/PHYS/PublicAPI.md → "Zenith_PhysicsMaterial header"

### Z2-R-PHYS-142 Destroyed-body contact records and pair handover

- **Statement:** A backend never reads the identity of a body that is no longer its slot's live occupant: Jolt checks each removal report's body sequence number against the slot's live bit and generation, PhysX tests the pair header's removed-actor flags before any `userData` read, and such reports are dropped and counted; the pairs of such bodies end through the ended list, which is processed before the step's records are walked, so a pair another body of the same simulation ID continues in the same step is a counted handover with no event, and the ended list is saved.
- **Covers:** Z2-I-PHYS-23, Z2-I-PHYS-24
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsEvents_DestroyedBodyRecordsDropped[<Backend>]`, `Test_PhysicsEvents_HandoverKeepsPair`, `Test_PhysicsSave_SectionRoundTrip` (its ended-list case) (`Test` step).
- **Design:** Zenith2/Docs/Design/PHYS/Backends.md → "Jolt contact listener"; Zenith2/Docs/Design/PHYS/Backends.md → "Body identity on both backends"; Zenith2/Docs/Design/PHYS/Backends.md → "PhysX contact reporting"; Zenith2/Docs/Design/PHYS/Simulation.md → "Event emission"

### Z2-R-PHYS-143 Pairs persist while a body sleeps

- **Statement:** A pair persists while a body of it sleeps: the Jolt backend flags each removal of a contact of two live bodies one of which is non-static and asleep, the module moves such a contact to the pair's suspended count without changing it, an added contact re-confirms a suspended one, and when both bodies are awake again the unconfirmed suspended contacts are subtracted and may end the pair in that tick; the PhysX backend, whose sleeping islands keep their pairs, appends no such removal.
- **Covers:** Z2-I-PHYS-23, Z2-I-PHYS-24, Z2-I-PHYS-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-003, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsEvents_PairSurvivesSleepAndWake[<Backend>]`, `Test_PhysicsEvents_SuspendedContactsResolveAtWake`, `Test_PhysicsConformance_ContactsReported[<Backend>]` (`Test` step); the PhysX sleep audit of the pinning ticket.
- **Design:** Zenith2/Docs/Design/PHYS/Simulation.md → "Sleeping"; Zenith2/Docs/Design/PHYS/Simulation.md → "Event emission"; Zenith2/Docs/Design/PHYS/Backends.md → "Jolt contact listener"; Zenith2/Docs/Design/PHYS/Backends.md → "Backend behaviour differences"

### Z2-R-PHYS-144 Ragdolls freeze with their region

- **Statement:** A ragdoll not in NONE whose character entity's RegionBound reports it outside the active region has every part frozen (motion type and velocities saved, kinematic, asleep, query-only filtering), keeps its part constraints, state, weight and run count, refuses enter and recover playbacks, and is restored exactly at the thaw; frozen parts are saved and restored frozen.
- **Covers:** Z2-I-PHYS-14, Z2-I-PHYS-16
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicsFreeze_RegionBoundFreezesAndRestores` (its ragdoll case), `Test_PhysicsSave_SectionRoundTrip` (a frozen part) (`Test` step).
- **Design:** Zenith2/Docs/Design/PHYS/Simulation.md → "Region freeze"; Zenith2/Docs/Design/PHYS/CharacterRagdollVehicle.md → "Ragdoll state machine"; Zenith2/Docs/Design/PHYS/Formats.md → "Save section layout"

## Coverage of inventory items

| Item | Text | Stage | Requirements |
|---|---|---|---|
| Z2-I-PHYS-01 | the neutral `Zenith_Physics` API | S3 | Z2-R-PHYS-008, Z2-R-PHYS-009, Z2-R-PHYS-010, Z2-R-PHYS-011, Z2-R-PHYS-012, Z2-R-PHYS-013, Z2-R-PHYS-014, Z2-R-PHYS-015, Z2-R-PHYS-022, Z2-R-PHYS-023, Z2-R-PHYS-032, Z2-R-PHYS-033, Z2-R-PHYS-034, Z2-R-PHYS-035, Z2-R-PHYS-036, Z2-R-PHYS-037, Z2-R-PHYS-038, Z2-R-PHYS-039, Z2-R-PHYS-040, Z2-R-PHYS-041, Z2-R-PHYS-042, Z2-R-PHYS-043, Z2-R-PHYS-044, Z2-R-PHYS-102, Z2-R-PHYS-103, Z2-R-PHYS-118, Z2-R-PHYS-122, Z2-R-PHYS-128, Z2-R-PHYS-129 |
| Z2-I-PHYS-02 | the Jolt backend | S3 | Z2-R-PHYS-003, Z2-R-PHYS-004, Z2-R-PHYS-009, Z2-R-PHYS-016, Z2-R-PHYS-017, Z2-R-PHYS-018, Z2-R-PHYS-021, Z2-R-PHYS-112, Z2-R-PHYS-124, Z2-R-PHYS-132, Z2-R-PHYS-133 |
| Z2-I-PHYS-03 | the PhysX backend | S3 | Z2-R-PHYS-001, Z2-R-PHYS-002, Z2-R-PHYS-003, Z2-R-PHYS-004, Z2-R-PHYS-009, Z2-R-PHYS-016, Z2-R-PHYS-019, Z2-R-PHYS-020, Z2-R-PHYS-021, Z2-R-PHYS-112, Z2-R-PHYS-134 |
| Z2-I-PHYS-04 | a shared physics conformance suite | S3 | Z2-R-PHYS-024, Z2-R-PHYS-025, Z2-R-PHYS-077, Z2-R-PHYS-103, Z2-R-PHYS-114, Z2-R-PHYS-115, Z2-R-PHYS-130, Z2-R-PHYS-136, Z2-R-PHYS-139, Z2-R-PHYS-143 |
| Z2-I-PHYS-05 | per-backend determinism | S3 | Z2-R-PHYS-026, Z2-R-PHYS-027, Z2-R-PHYS-028, Z2-R-PHYS-100, Z2-R-PHYS-101, Z2-R-PHYS-122, Z2-R-PHYS-123, Z2-R-PHYS-124, Z2-R-PHYS-125, Z2-R-PHYS-126, Z2-R-PHYS-127, Z2-R-PHYS-138, Z2-R-PHYS-139 |
| Z2-I-PHYS-06 | collision cooking | S3 | Z2-R-PHYS-045, Z2-R-PHYS-046, Z2-R-PHYS-047, Z2-R-PHYS-048, Z2-R-PHYS-049, Z2-R-PHYS-050, Z2-R-PHYS-051, Z2-R-PHYS-052, Z2-R-PHYS-113, Z2-R-PHYS-130, Z2-R-PHYS-131, Z2-R-PHYS-141 |
| Z2-I-PHYS-07 | collision filtering | S3 | Z2-R-PHYS-053, Z2-R-PHYS-054, Z2-R-PHYS-055, Z2-R-PHYS-094, Z2-R-PHYS-115, Z2-R-PHYS-116 |
| Z2-I-PHYS-08 | physics queries | S3 | Z2-R-PHYS-056, Z2-R-PHYS-057, Z2-R-PHYS-058, Z2-R-PHYS-059, Z2-R-PHYS-060, Z2-R-PHYS-061, Z2-R-PHYS-102, Z2-R-PHYS-116, Z2-R-PHYS-129, Z2-R-PHYS-135, Z2-R-PHYS-140 |
| Z2-I-PHYS-09 | triggers | S3 | Z2-R-PHYS-062, Z2-R-PHYS-063, Z2-R-PHYS-117 |
| Z2-I-PHYS-10 | character controllers | S3 | Z2-R-PHYS-076, Z2-R-PHYS-078, Z2-R-PHYS-079, Z2-R-PHYS-080, Z2-R-PHYS-081, Z2-R-PHYS-082, Z2-R-PHYS-119 |
| Z2-I-PHYS-11 | constraints | S3 | Z2-R-PHYS-083, Z2-R-PHYS-084, Z2-R-PHYS-114, Z2-R-PHYS-136 |
| Z2-I-PHYS-12 | joints | S3 | Z2-R-PHYS-085, Z2-R-PHYS-086, Z2-R-PHYS-087, Z2-R-PHYS-114, Z2-R-PHYS-121, Z2-R-PHYS-136 |
| Z2-I-PHYS-13 | vehicles | S4 | Z2-R-PHYS-104 |
| Z2-I-PHYS-14 | ragdolls | S3 | Z2-R-PHYS-041, Z2-R-PHYS-052, Z2-R-PHYS-088, Z2-R-PHYS-089, Z2-R-PHYS-090, Z2-R-PHYS-091, Z2-R-PHYS-092, Z2-R-PHYS-093, Z2-R-PHYS-094, Z2-R-PHYS-095, Z2-R-PHYS-096, Z2-R-PHYS-136, Z2-R-PHYS-137, Z2-R-PHYS-138, Z2-R-PHYS-144 |
| Z2-I-PHYS-15 | physics debug visualization | S3 | Z2-R-PHYS-097, Z2-R-PHYS-098, Z2-R-PHYS-099 |
| Z2-I-PHYS-16 | physics streaming | S4 | Z2-R-PHYS-107, Z2-R-PHYS-108, Z2-R-PHYS-109, Z2-R-PHYS-120, Z2-R-PHYS-144 |
| Z2-I-PHYS-17 | cloth, classified (D-CLASS-SIM) | S2 | Z2-R-PHYS-005 |
| Z2-I-PHYS-18 | soft bodies, classified (D-CLASS-SIM) | S2 | Z2-R-PHYS-006 |
| Z2-I-PHYS-19 | destruction, classified (D-CLASS-SIM) | S2 | Z2-R-PHYS-007 |
| Z2-I-PHYS-20 | specialist simulations, as classified extensions | S5 | Z2-R-PHYS-111 |
| Z2-I-PHYS-21 | CCD | S3 | Z2-R-PHYS-072, Z2-R-PHYS-073, Z2-R-PHYS-074 |
| Z2-I-PHYS-22 | substepping | S3 | Z2-R-PHYS-018, Z2-R-PHYS-020, Z2-R-PHYS-030, Z2-R-PHYS-031, Z2-R-PHYS-077, Z2-R-PHYS-133, Z2-R-PHYS-134, Z2-R-PHYS-140 |
| Z2-I-PHYS-23 | contact ordering | S3 | Z2-R-PHYS-064, Z2-R-PHYS-065, Z2-R-PHYS-084, Z2-R-PHYS-137, Z2-R-PHYS-142, Z2-R-PHYS-143 |
| Z2-I-PHYS-24 | event ordering | S3 | Z2-R-PHYS-062, Z2-R-PHYS-064, Z2-R-PHYS-066, Z2-R-PHYS-137, Z2-R-PHYS-142, Z2-R-PHYS-143 |
| Z2-I-PHYS-25 | physical materials | S3 | Z2-R-PHYS-052, Z2-R-PHYS-067, Z2-R-PHYS-068, Z2-R-PHYS-069, Z2-R-PHYS-141 |
| Z2-I-PHYS-26 | physical interactions | S3 | Z2-R-PHYS-070, Z2-R-PHYS-071, Z2-R-PHYS-081, Z2-R-PHYS-105, Z2-R-PHYS-106 |
| Z2-I-PHYS-27 | moving platforms | S3 | Z2-R-PHYS-075, Z2-R-PHYS-076 |
| Z2-I-PHYS-28 | large-coordinate behaviour | S4 | Z2-R-PHYS-109, Z2-R-PHYS-110, Z2-R-PHYS-120, Z2-R-PHYS-121 |
| Z2-I-PHYS-29 | determinism boundaries | S3 | Z2-R-PHYS-023, Z2-R-PHYS-026, Z2-R-PHYS-028, Z2-R-PHYS-029, Z2-R-PHYS-057, Z2-R-PHYS-125, Z2-R-PHYS-127 |

## Excluded and deferred

Every PHYS inventory item is required at its inventory stage: Z2-I-PHYS-17, -18 and -19 at Stage 2, whose deliverable is the D-CLASS-SIM classification itself (not a runtime feature); Z2-I-PHYS-20 at Stage 5, delivering whatever that ruling classifies as a required physics feature; -13, -16 and -28 at Stage 4; the rest at Stage 3. No PHYS requirement is deferred, excluded or game-side.

| Not in PHYS | Rationale | Decision or owner |
|---|---|---|
| 2D physics | `Zenith2D` owns it under D-SEL-2DPHYS (open, Stage 2) | TWOD; Z2-I-TWOD-06 |
| root-motion extraction and application to entities without a controller | ANIM publishes the delta and applies it to every row without a controller (the orchestrator's Q72 ruling) | ANIM |
| the animated side of ragdolls (pose ownership, get-up, pose matching) | ANIM's ragdoll transitions | ANIM |
| camera collision policy | GAME uses `Zenith_CollisionQuery` | GAME |
| water simulation and sampling | ENV implements `Zenith_FluidQuery` | ENV |
| cloth, soft bodies and destruction as runtime features | classified by D-CLASS-SIM (open, Stage 2); built only if classified required | the owner the ruling names |
| GPU physics | both backends are built CPU-only | PHYS; Z2-D-003 |
