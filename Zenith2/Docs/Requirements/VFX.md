# VFX requirements

## Purpose

The requirement catalogue of the VFX area: every testable obligation of `Zenith2/Docs/Design/VFX.md` and its section files, one per `Z2-R-VFX-NNN`, each with the inventory items it covers, the stage whose acceptance it serves, its governing decisions and budget rows, the artefact that proves it and the design headings that specify it. Every VFX inventory item (Z2-I-VFX-01 … Z2-I-VFX-13) is required at Stage 5 (`Zenith2/Docs/Requirements/Inventory.md`); the budget-value ruling serves Stage 1. Tickets implementing Stage 5 requirements that wait on no open decision are scheduled in the Stage 4 sprint with fix version `Z2 Stage 5`, except the ZenithFlux API slice, which the Stage 3 sprint carries (`Zenith2/Docs/Design/VFX.md → "Stage 5 slices"`).

## Requirements

### Z2-R-VFX-001 Module library target

- **Statement:** ZenithVFX is a `KIND MODULE` library declaring exactly the dependencies of "Library dependencies", supported on `ZENITH_GPU`, with public headers in its root and everything else under `Internal/`.
- **Covers:** Z2-I-VFX-01, Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-024
- **Budgets:** none
- **Acceptance evidence:** `Libgraph`, `SourceLayout` and `CompileCoverage` gate steps on the scaffold ticket's diff
- **Design:** Zenith2/Docs/Design/VFX.md → "Library dependencies"; Zenith2/Docs/Design/VFX.md → "Target summary"; Zenith2/Docs/Design/VFX.md → "ZenithVFX files"

### Z2-R-VFX-002 API-only header slice first

- **Statement:** The ZenithFlux particle headers, with definitions of `Flux_AddParticleSimulation` and `Flux_AddParticleDraw` that declare no pass, of the internal particle frame functions that do nothing, and of `Flux_CreateParticleSystem` (validation and allocation), `Flux_ReleaseParticleSystem` and the bodies of the three RPROD-declared `Flux_Renderer` particle members, land in the Stage 3 sprint after RPROD's renderer API ticket and before RPROD's path functions and VFX's module start need them, and the six ZenithVFX headers land after the scaffold; every header carries its contract blocks before any implementation that depends on it and compiles self-contained.
- **Covers:** Z2-I-VFX-01, Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-085
- **Budgets:** none
- **Acceptance evidence:** `HeaderSelfContained` and `ApiContractCheck` gate steps on the two API-only tickets; `Test_ParticleDraw_NoParticleFrameDeclaresNoPass`, `Test_ParticleSystem_EnableDisableLifecycle` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX.md → "Stage 5 slices"; Zenith2/Docs/Design/VFX/PublicAPI.md → "Header map"

### Z2-R-VFX-003 Module entry points

- **Statement:** `Zenith_Module_VFX_RegisterTypes` registers the VFX types and `vfx.*` configuration keys and nothing else, and `Zenith_Module_VFX_Start` performs the nine start steps in order.
- **Covers:** Z2-I-VFX-01, Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-024, Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXEffectComponent_Registration`, `Test_VFXConfig_KeysRegistered` in `ZenithVFX_Tests` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/PublicAPI.md → "Zenith_Module_VFX header"

### Z2-R-VFX-004 Module start unwinds

- **Statement:** A failure at any fallible start step (including the configuration listener and the profiler panel) returns that step's error and undoes every earlier step in reverse, `EnableParticles` through `DisableParticles`, leaving no allocation, registration or thread behind.
- **Covers:** Z2-I-VFX-01, Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXModule_Start_FailsAt<Step>_Unwinds` for each fallible step (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/PublicAPI.md → "Zenith_Module_VFX header"; Zenith2/Docs/Design/VFX/Tests.md → "Failure and fault-injection tests"

### Z2-R-VFX-005 Headless module behaviour

- **Statement:** Without a renderer the module runs lifecycles, triggers, scheduling and the command stream, skips CPU particle updates and creates no particle system.
- **Covers:** Z2-I-VFX-01
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXModule_HeadlessRunsLogic` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "CPU particle output"; Zenith2/Docs/Design/VFX/PublicAPI.md → "Zenith_Module_VFX header"

### Z2-R-VFX-006 Component types registered

- **Statement:** The five components `Zenith.VFX.Effect`, `Zenith.VFX.EffectState`, `Zenith.VFX.Collider`, `Zenith.VFX.Viewer` and `Zenith.VFX.PreviewControl` register with their stable names, property IDs, defaults and serialization flags, the preview control never serialized.
- **Covers:** Z2-I-VFX-01, Z2-I-VFX-05, Z2-I-VFX-09, Z2-I-VFX-12
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXEffectComponent_Registration` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/PublicAPI.md → "Zenith_VFXComponents header"; Zenith2/Docs/Design/VFX/PublicAPI.md → "Component registration"

### Z2-R-VFX-007 Error enumerations

- **Statement:** `Zenith_VFXError` and `Flux_ParticleError` carry exactly the enumerators of their blocks from 1, each with `Zenith_ToString`, and every fallible public function returns one of them through `Zenith_Result`.
- **Covers:** Z2-I-VFX-01, Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `ApiContractCheck` gate step; `Test_VFXSpawnPayload_Validation` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/PublicAPI.md → "Zenith_VFXErrors header"; Zenith2/Docs/Design/VFX.md → "Error enumerations"

### Z2-R-VFX-008 Contract blocks on every public type

- **Statement:** Every public VFX type and free function carries a contract block with the six fields.
- **Covers:** Z2-I-VFX-01, Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `ApiContractCheck` gate step on every VFX ticket
- **Design:** Zenith2/Docs/Design/VFX.md → "Header summary"

### Z2-R-VFX-009 Particles are presentation only

- **Statement:** No simulation node reads particle or emitter presentation state, so a replay with and without the VFX module gives identical simulation state hashes.
- **Covers:** Z2-I-VFX-01, Z2-I-VFX-02, Z2-I-VFX-10
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXPresentation_SimulationUnaffected` in `Zenith_Integration_VFX` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "Presentation-only particles"

### Z2-R-VFX-010 Presentation clock

- **Statement:** Each instance advances by `Δ = clamp((T_k − T_{k−1}) × timeScale, 0, 1/15 s)` from the world's presentation time, and by 0 while paused, while its world is paused or while a preview freezes it.
- **Covers:** Z2-I-VFX-01, Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXClock_DeltaClampAndPause` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "Presentation clock"

### Z2-R-VFX-011 Lifecycle state machine

- **Statement:** `VFX.Lifecycle` applies every row of the lifecycle state machine once per tick per entity, recording the state additions, destroys and events of each row on every ECS backend.
- **Covers:** Z2-I-VFX-10
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-080, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXLifecycle_StateMachine` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "Lifecycle state machine"; Zenith2/Docs/Design/VFX/Simulation.md → "Lifecycle system"

### Z2-R-VFX-012 Destroy after seconds in ticks

- **Statement:** `DestroyAfterSeconds` destroys the entity `ceil(n − e)` ticks after start, `n = double(x) × tickHz` and `e = min(0.5 × ulp(x) × tickHz, 0.4990234375)` for the stored `float` `x` and the configured integer tick rate, which equals the decimal-exact count whenever the authored value times the rate is an integer and `0.5 × ulp(x) × tickHz < 0.5` (every value of the domain at rates up to 127 Hz: 0.5 s is 15, 30 and 60 ticks at 30, 60 and 120 Hz, 0.1 s is 3 at 30 Hz, 86,400 s is 10,368,000 at 120 Hz), and is otherwise `ceil(n)` or one tick fewer, within one tick of `ceil(v × tickHz)` on either side whenever `ulp(x) × tickHz ≤ 1` (every value of the domain at rates up to 128 Hz).
- **Covers:** Z2-I-VFX-10
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXLifecycle_DestroyAfterTicks` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "Lifecycle state machine"

### Z2-R-VFX-013 Invalid component values ignored

- **Statement:** An effect component value outside its domain starts and stops nothing for that entity in that tick and is counted once, never asserting.
- **Covers:** Z2-I-VFX-10
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXLifecycle_InvalidComponentIgnored` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "Lifecycle state machine"

### Z2-R-VFX-014 Started and stopped events

- **Statement:** `Zenith.VFX.EffectStarted` and `Zenith.VFX.EffectStopped` are emitted at the lifecycle transitions with the entity, effect, generation and stop reason, under stable names any module subscribes to.
- **Covers:** Z2-I-VFX-10
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXLifecycle_StateMachine`, `Test_VFXEvents_StableNames` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/PublicAPI.md → "Zenith_VFXEvents header"; Zenith2/Docs/Design/VFX/Simulation.md → "Lifecycle state machine"

### Z2-R-VFX-015 Lifecycle state saved and restored

- **Statement:** A save and load restores every `Zenith.VFX.EffectState` value exactly, and a replay from the save reproduces the uninterrupted run's simulation hash.
- **Covers:** Z2-I-VFX-10
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXSave_StateRoundTrip`, `Test_VFXSave_ReplayIdentical` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "Attached effect lifecycle"

### Z2-R-VFX-016 Spawn command kind

- **Statement:** The `VFX.Spawn` command kind validates its 96-byte payload at record time, is recordable in phases 1–10, and its playback appends one trigger record and writes no ECS state.
- **Covers:** Z2-I-VFX-10
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXSpawnPayload_Validation` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/PublicAPI.md → "Zenith_VFXCommands header"

### Z2-R-VFX-017 Luau spawn bindings

- **Statement:** `VFX.Spawn` and `VFX.SpawnAttached` append trigger records with the given payloads (lead included) through the script side-effect path, `VFX.ParameterIndex` reads the effect only through the calling world's published load result (never `PURE`, never the live registry) and returns `nil` until that result is loaded and the name's slot from that tick on (`nil` for an unknown name), identically on every run whatever the load's completion time, and each misuse raises its MISUSE code (`ZS5601`, `ZS5602`, or SCRIPT's `ZS1510`/`ZS1511` for argument count and type) and never asserts.
- **Covers:** Z2-I-VFX-10
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-016, Z2-D-051, Z2-D-019
- **Budgets:** Z2-B-SCRIPT-024
- **Acceptance evidence:** `Test_VFXScript_SpawnBindings`, `Test_VFXScript_MisuseNeverAsserts` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/PublicAPI.md → "Luau surface"; Zenith2/Docs/Design/VFX.md → "Luau errors and asserts"

### Z2-R-VFX-018 Script trigger consumer

- **Statement:** The VFX trigger consumer registered for `SIDE_EFFECT_KIND_VFX_TRIGGER` appends every trigger a script merge accepted to the world's trigger queue within the script reserve, so none is lost at the capacity of `Z2-B-SCRIPT-024`, even while C++ triggers fill their share.
- **Covers:** Z2-I-VFX-10, Z2-I-VFX-11
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-016
- **Budgets:** Z2-B-SCRIPT-024
- **Acceptance evidence:** `Test_VFXTriggerQueue_ScriptReserve` (`Test`); `Test_VFXStress_TriggerFlood` (`z2-stress`, `Test`)
- **Design:** Zenith2/Docs/Design/VFX/PublicAPI.md → "VFX trigger record"; Zenith2/Docs/Design/VFX/Simulation.md → "Detached effects and the trigger queue"

### Z2-R-VFX-019 Trigger queue full policy

- **Statement:** A world's trigger queue holds `vfx.triggerQueueCapacity` records with `QUEUE_FULL_POLICY_DROP_NEWEST`; C++ `VFX.Spawn` records use at most the capacity minus `vfx.triggerQueueScriptReserve` per frame, and every dropped record is counted.
- **Covers:** Z2-I-VFX-10, Z2-I-VFX-11
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXTriggerQueue_DropNewest` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "Detached effects and the trigger queue"

### Z2-R-VFX-020 Trigger order deterministic

- **Statement:** Trigger records take sequences in (phase, merge) order and drain in sequence order, identically on any worker count.
- **Covers:** Z2-I-VFX-10
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXTriggerQueue_SequenceOrder` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "Detached effects and the trigger queue"

### Z2-R-VFX-021 Detached effects attach to targets

- **Statement:** A detached effect spawned with the attach flag follows its target's interpolated transform with the payload offset, and freezes and releases when the target stops resolving.
- **Covers:** Z2-I-VFX-10
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXDetached_AttachFollowsTarget` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "Detached effects and the trigger queue"

### Z2-R-VFX-022 Detached effect cap and truncation

- **Statement:** At most `vfx.detachedEffectsPerWorld` detached instances live per world, beyond which records are discarded in sequence order, and a looping or endless effect played detached plays one cycle or 10 s.
- **Covers:** Z2-I-VFX-10, Z2-I-VFX-11
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXDetached_CapAndTruncation` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "Detached effects and the trigger queue"

### Z2-R-VFX-023 Emitter instance state machine

- **Statement:** Each emitter of each instance follows the emitter instance state machine and `VFX.Extract` writes exactly the commands its transitions name.
- **Covers:** Z2-I-VFX-01, Z2-I-VFX-02, Z2-I-VFX-07
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXExtract_CommandStream` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "Emitter instance state machine"; Zenith2/Docs/Design/VFX/Simulation.md → "Commands per frame"

### Z2-R-VFX-024 Rate spawning exact

- **Statement:** Rate spawning carries its fractional accumulator in `double` by the rule of "Rate spawning", so a rate r over a frame sequence spawns exactly the count that rule gives, within one particle of `floor(r × ΣΔ)`.
- **Covers:** Z2-I-VFX-01, Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXSchedule_RateSpawnCounts` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "Rate spawning"

### Z2-R-VFX-025 Burst spawning

- **Statement:** Bursts fire at their times in every timeline cycle with their cycle and interval rules, and a looping timeline neither loses nor repeats a burst.
- **Covers:** Z2-I-VFX-01, Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXSchedule_BurstCycles` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "Burst spawning"

### Z2-R-VFX-026 Distance spawning

- **Statement:** Distance spawning emits one particle per `spacing` metres of emitter travel at interpolated positions, and none across a teleport.
- **Covers:** Z2-I-VFX-01, Z2-I-VFX-02, Z2-I-VFX-03
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXSchedule_DistanceSpawning` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "Distance spawning"

### Z2-R-VFX-027 Prewarm

- **Statement:** An emitter with `prewarm` P starts with `min(floor(r_0 × P × S), C)` particles aged evenly over [0, P) and integrated as specified.
- **Covers:** Z2-I-VFX-01, Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXSchedule_PrewarmAges`, `Test_ParticleParity_PrewarmAndBursts` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "Prewarm"

### Z2-R-VFX-028 Particle random numbers

- **Statement:** `Flux_ParticleHash`, `Flux_ParticleRandomU32` and `Flux_ParticleRandomUnit` return identical bits in C++ and Slang, with unit values in [0, 1).
- **Covers:** Z2-I-VFX-01, Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticleRandom_KnownValues`, `Test_ParticleRandom_UnitRange` (`Test`, WARP and lavapipe)
- **Design:** Zenith2/Docs/Design/VFX/PublicAPI.md → "Particle random functions"; Zenith2/Docs/Design/VFX/Simulation.md → "Particle random numbers"

### Z2-R-VFX-029 Instance keys and seeds

- **Statement:** Attached instance keys derive from the entity's stable simulation ID, the `uint16` world tag and the generation (never the persistent ID), detached keys from the world tag and trigger sequence; the two kinds are disjoint, a restart produces a new key, and default seeds derive deterministically (attached: simulation ID and generation, independent of the world tag) unless the effect fixes one.
- **Covers:** Z2-I-VFX-01, Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticleInstanceKey_AttachedDetachedDisjoint` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/PublicAPI.md → "Instance keys"

### Z2-R-VFX-030 Ring slots by spawn ordinal

- **Statement:** A particle of spawn ordinal o occupies slot `o mod C` of its emitter's range, with no alive or dead list, so a burst larger than the capacity keeps the newest C.
- **Covers:** Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-058
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticleEmit_RingSlots` (`Test`, WARP and lavapipe)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "Spawn ordinals and capacity"; Zenith2/Docs/Design/VFX/Simulation.md → "Particle pool layout"

### Z2-R-VFX-031 Particle initialisation and update order

- **Statement:** Initialisation modules and update modules run in the fixed orders and with the arithmetic of their sections on both simulators.
- **Covers:** Z2-I-VFX-01, Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticleParity_Integration` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "Initialisation algorithms"; Zenith2/Docs/Design/VFX/Simulation.md → "Particle update order"

### Z2-R-VFX-032 CPU emitter simulation

- **Statement:** CPU emitters are simulated in `VFX.Update` by a worker-independent `ParallelFor`, giving bit-identical particle spans on 1, 2 and 8 workers.
- **Covers:** Z2-I-VFX-01
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-VFX-002
- **Acceptance evidence:** `Test_VFXCpuSim_WorkerCountIndependent` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "CPU update algorithm"; Zenith2/Docs/Design/VFX/Simulation.md → "CPU emitter storage"

### Z2-R-VFX-033 CPU particle output

- **Statement:** Each admitted CPU emitter's live particles reach the render world in spawn order and are drawn by the same programs as GPU particles, and a world's render-world records stay within the per-world caps stated against `Z2-B-CORE-005`.
- **Covers:** Z2-I-VFX-01
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-CORE-005
- **Acceptance evidence:** `Test_ParticleParity_CpuTargetMatchesGpu`, `Test_VFXExtract_SlotArenaDemand` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "CPU particle output"; Zenith2/Docs/Design/VFX/Rendering.md → "Draw records and indirect commands"

### Z2-R-VFX-034 CPU admission per world

- **Statement:** CPU emitters are admitted per world by significance, then key, within `vfx.cpuParticlesPerWorld` capacity and `vfx.cpuEmittersPerWorld` count, the rest budget-culled with their cull behaviour.
- **Covers:** Z2-I-VFX-01, Z2-I-VFX-11
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-VFX-002
- **Acceptance evidence:** `Test_VFXAdmission_CpuPerWorld` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "CPU admission per world"

### Z2-R-VFX-035 CPU particle update cost

- **Statement:** The CPU particle update of the `W-Cinematic` and `W-OpenWorld` workloads stays within `Z2-B-VFX-002` on Windows and the S24.
- **Covers:** Z2-I-VFX-01, Z2-I-VFX-11
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-VFX-002
- **Acceptance evidence:** the Stage 5 acceptance budget run of `Zenith2/Benchmarks/Budgets/VFX/` on the reference hardware and the S24 (`needs-human`, `gpu-reason:visual-measure`)
- **Design:** Zenith2/Docs/Design/VFX.md → "Owned rows"

### Z2-R-VFX-036 CPU storage accounted

- **Statement:** The module's memory consumer reports its CPU particle storage and trigger queues under `MEMORY_SUBSYSTEM_RENDERING`, equal to the bytes allocated.
- **Covers:** Z2-I-VFX-01, Z2-I-VFX-11
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-VFX-001
- **Acceptance evidence:** `Test_VFXMemory_ConsumerReportsCpuStorage` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/PublicAPI.md → "Zenith_Module_VFX header"; Zenith2/Docs/Design/VFX/Simulation.md → "CPU emitter storage"

### Z2-R-VFX-037 Particle pool through RHI pools

- **Statement:** The particle pool is one RHI pool with state, trail and event regions whose emitter ranges are allocated and freed through RHI's pool allocation and never overlap a live range.
- **Covers:** Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-058
- **Budgets:** Z2-B-VFX-001
- **Acceptance evidence:** `Test_ParticleSystem_PoolRangeReuse` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "Particle pool layout"; Zenith2/Docs/Design/VFX/Simulation.md → "Emitter pool ranges"

### Z2-R-VFX-038 Particle system construction unwinds

- **Statement:** `EnableParticles` builds the system in its six steps and a failure at any step returns its error, undoes the earlier steps and leaves `GetParticleSystem()` null.
- **Covers:** Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticleSystem_Create_FailsAt<Step>_Unwinds` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/PublicAPI.md → "Flux_ParticleSystem"

### Z2-R-VFX-039 Simulation pass declarations

- **Statement:** The four simulation passes declare exactly the fields, access, stages, queue, flags and permutations of their tables, and a frame with no admitted emitter declares none.
- **Covers:** Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-055, Z2-D-056, Z2-D-054
- **Budgets:** Z2-B-VFX-003
- **Acceptance evidence:** `Test_ParticlePasses_DeclareTableFields` (`Test`, WARP and lavapipe)
- **Design:** Zenith2/Docs/Design/VFX/Rendering.md → "ParticleEmit table"; Zenith2/Docs/Design/VFX/Rendering.md → "ParticleSimulate table"; Zenith2/Docs/Design/VFX/Rendering.md → "ParticleCompactAndSortKeys table"

### Z2-R-VFX-040 Dead slots untouched

- **Statement:** The simulate pass never writes a slot that holds no live particle.
- **Covers:** Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticleSimulate_DeadSlotsUntouched` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "ParticleSimulate dispatch"

### Z2-R-VFX-041 GPU admission per frame

- **Statement:** GPU emitters are admitted per frame across worlds by significance, then key, each connected event graph as one unit and units carrying preview catch-up steps first, within `vfx.gpuActiveCapacity` counting simulate threads plus event-consumer threads (`C_src × E_edge × k` per target) summed over every step of the frame, and within `vfx.gpuEmittersPerFrame`; a refused catch-up unit keeps its steps pending.
- **Covers:** Z2-I-VFX-02, Z2-I-VFX-11
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-VFX-003
- **Acceptance evidence:** `Test_VFXAdmission_GpuPerFrame`, `Test_VFXStress_ManyWorlds` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "GPU admission per frame"

### Z2-R-VFX-042 Frame building

- **Statement:** `Flux_ParticleBuildFrame` builds, in order, the admission, the emitter commands of every step (numbered in arrival order, kept steps first, each with its own step tag), the per-step dispatch batches recorded in `Flux_ParticleFrame::m_axSteps`, the sort setup and the per-view draw lists from each emitter's last step.
- **Covers:** Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticlePasses_DeclareTableFields`, `Test_ParticleDraw_IndirectRunsPerPSO` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "Frame building"; Zenith2/Docs/Design/VFX/Rendering.md → "Draw pipeline overview"

### Z2-R-VFX-043 GPU simulation reproducible

- **Statement:** The same command sequence gives a bit-identical pool, sorted pairs and colour target when run twice on one backend, device and build.
- **Covers:** Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticleSim_RunTwiceIdentical` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "Determinism rules"

### Z2-R-VFX-044 Async compute identical

- **Statement:** Particle results are bit-identical with async compute on and off.
- **Covers:** Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticleSim_AsyncComputeIdentical` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "Determinism rules"; Zenith2/Docs/Design/VFX/Rendering.md → "Capability and quality tiers"

### Z2-R-VFX-045 Aliasing invariant

- **Statement:** Particle frames are bit-identical with transient aliasing on and off and poisoning enabled, with clean synchronization validation.
- **Covers:** Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-057
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticleSim_AliasingIdentical` (`Test`, WARP and lavapipe); the `Particles*` parity scenes
- **Design:** Zenith2/Docs/Design/VFX/Tests.md → "GPU simulation tests"; Zenith2/Docs/Design/VFX/Tests.md → "Render tests"

### Z2-R-VFX-046 CPU and GPU parity of modules

- **Statement:** The GPU simulator agrees with the CPU reference within the parity tolerances for every update module.
- **Covers:** Z2-I-VFX-01, Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticleParity_Integration` (`Test`, WARP and lavapipe)
- **Design:** Zenith2/Docs/Design/VFX/Tests.md → "CPU reference parity tests"

### Z2-R-VFX-047 Local-space parity

- **Statement:** Local-space emitters on a moving, rotating transform agree between the simulators within tolerance.
- **Covers:** Z2-I-VFX-01, Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticleParity_LocalSpace` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Tests.md → "CPU reference parity tests"

### Z2-R-VFX-048 Capacity overflow parity

- **Statement:** Prewarm and bursts beyond capacity agree between the simulators, overwriting the oldest slots.
- **Covers:** Z2-I-VFX-01, Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticleParity_PrewarmAndBursts` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Tests.md → "CPU reference parity tests"

### Z2-R-VFX-049 CPU and GPU targets agree

- **Statement:** The same document cooked for the CPU and the GPU target produces particle states within the parity tolerances.
- **Covers:** Z2-I-VFX-01, Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticleParity_CpuTargetMatchesGpu` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Tests.md → "CPU reference parity tests"

### Z2-R-VFX-050 Command validation

- **Statement:** A command failing a validation rule of `Flux_RenderEmitterCommand` is dropped and counted and the emitter keeps its previous state.
- **Covers:** Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticleSystem_CommandValidation` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/PublicAPI.md → "Flux_RenderEmitterCommand"; Zenith2/Docs/Design/VFX/Rendering.md → "Render-side failure behaviour"

### Z2-R-VFX-051 Deferred frames keep commands

- **Statement:** A deferred frame's commands are merged into the pending list and applied in order in the next submitted frame, the list bounded and its overflow counted.
- **Covers:** Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticleSystem_DeferredFrameKeepsCommands` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "Deferred and failed frames"

### Z2-R-VFX-052 Device loss recovery

- **Statement:** After a device loss every live GPU emitter's range and every effect constants buffer is recreated, GPU particles restart empty and spawning resumes on the first recovered frame.
- **Covers:** Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticleSystem_DeviceLossRecovery` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "Device loss"

### Z2-R-VFX-053 Full-state resync

- **Statement:** A full-state frame destroys every instance of its world the section does not mention and keeps the GPU particles of those it mentions.
- **Covers:** Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticleSystem_FullStateResync` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "Full-state frames"

### Z2-R-VFX-054 Effect constants image

- **Statement:** The effect constants buffer created on first use is byte-identical to the asset's constants image, which both simulators read.
- **Covers:** Z2-I-VFX-01, Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticleEffect_ConstantsImageMatchesGpu` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/EffectFormat.md → "Effect constants image"

### Z2-R-VFX-055 Code-authored update types

- **Statement:** An emitter naming a code-authored `Flux_ParticleUpdateType` runs that type in emission and simulation with its `Params` at the reflected offsets.
- **Covers:** Z2-I-VFX-02, Z2-I-VFX-12
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-079, Z2-D-015
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticleUpdateType_CustomTypeRuns` (`Test`, WARP and lavapipe)
- **Design:** Zenith2/Docs/Design/VFX/PublicAPI.md → "Flux_ParticleUpdateType"; Zenith2/Docs/Design/VFX/Authoring.md → "Code-authored effects"

### Z2-R-VFX-056 Particle simulation GPU time

- **Statement:** The particle simulation passes of the `W-Cinematic` and `W-OpenWorld` workloads stay within `Z2-B-VFX-003` on Windows and the S24.
- **Covers:** Z2-I-VFX-02, Z2-I-VFX-11
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-VFX-003
- **Acceptance evidence:** the Stage 5 acceptance budget run (`needs-human`, `gpu-reason:visual-measure`)
- **Design:** Zenith2/Docs/Design/VFX.md → "Owned rows"

### Z2-R-VFX-057 Particle memory

- **Statement:** Particle pools and CPU particle data of the `W-Cinematic` and `W-OpenWorld` workloads stay within `Z2-B-VFX-001`, measured by the sub-memory probe.
- **Covers:** Z2-I-VFX-01, Z2-I-VFX-02, Z2-I-VFX-11
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-VFX-001
- **Acceptance evidence:** the Stage 5 acceptance budget run (`needs-human`, `gpu-reason:visual-measure`)
- **Design:** Zenith2/Docs/Design/VFX.md → "Owned rows"; Zenith2/Docs/Design/VFX.md → "Proposed values"

### Z2-R-VFX-058 Emitter ribbons connect consecutive ordinals

- **Statement:** An emitter ribbon draws one segment between consecutive spawn ordinals only when both are live, so it breaks at a dead particle and never joins the ring's head to its tail.
- **Covers:** Z2-I-VFX-03
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-VFX-004
- **Acceptance evidence:** `Test_ParticleDraw_RibbonStrip` (`Test`, WARP and lavapipe)
- **Design:** Zenith2/Docs/Design/VFX/Rendering.md → "Emitter ribbons"

### Z2-R-VFX-059 Ribbon UV modes

- **Statement:** Ribbon `stretch` and `tile` UV modes and `tailFade` follow their formulas over the CPU-known ordinal span, on CPU and GPU emitters alike, with no GPU-written quantity.
- **Covers:** Z2-I-VFX-03
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticleDraw_RibbonUVModes` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Rendering.md → "Emitter ribbons"

### Z2-R-VFX-060 CPU ribbons

- **Statement:** A CPU emitter draws ribbons through the same ribbon program as a GPU emitter.
- **Covers:** Z2-I-VFX-01, Z2-I-VFX-03
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** the `ParticlesBasic` parity scene (`Debug_Ribbon`) under `BackendParity` and `PathParity`
- **Design:** Zenith2/Docs/Design/VFX/Rendering.md → "Emitter ribbons"; Zenith2/Docs/Design/VFX/Authoring.md → "Engine effect content"

### Z2-R-VFX-061 Trail sampling

- **Statement:** An emitter with the trail module keeps 2 to 8 samples per particle, advancing the head at its sample interval identically on both simulators.
- **Covers:** Z2-I-VFX-04
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticleParity_Trails` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "Particle trails"

### Z2-R-VFX-062 Trail ribbons

- **Statement:** Trail ribbons connect a particle's position to its samples newest first, with width and alpha from the render module's curves, and draw nothing for dead slots.
- **Covers:** Z2-I-VFX-04
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-VFX-004
- **Acceptance evidence:** `Test_ParticleDraw_RibbonStrip`, `Test_ParticleDraw_TrailWidthAlphaCurves` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Rendering.md → "Particle trail ribbons"

### Z2-R-VFX-063 Trail and event region sizing

- **Statement:** The cook sizes each emitter's trail samples and its event ring of exactly `C_src × E` records by the rules of "Capacity sizing", refusing a ring above 262,144 records, and the pool regions hold them.
- **Covers:** Z2-I-VFX-04, Z2-I-VFX-10
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-VFX-001
- **Acceptance evidence:** `Test_VFXCook_CapacitySizing` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Authoring.md → "Capacity sizing"; Zenith2/Docs/Design/VFX/Simulation.md → "Particle pool layout"

### Z2-R-VFX-064 Analytic colliders

- **Statement:** Particles collide with plane, sphere, box and capsule colliders with the documented signed distances, restitution, friction and kill rules, identically on both simulators.
- **Covers:** Z2-I-VFX-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticleParity_Collision` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "Analytic colliders"

### Z2-R-VFX-065 Collider gathering

- **Statement:** Each world sends at most 64 enabled colliders in simulation-ID order, counting the rest, and disables a collider with an invalid dimension.
- **Covers:** Z2-I-VFX-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXColliders_GatherCapAndInvalid` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "Analytic colliders"; Zenith2/Docs/Design/VFX/PublicAPI.md → "Flux_RenderParticleCollider"

### Z2-R-VFX-066 Depth-buffer collision

- **Statement:** GPU particles with depth collision collide with the sort view's previous HiZ pyramid within one texel's depth, and collide with nothing without history.
- **Covers:** Z2-I-VFX-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticleDepthCollision_PreviousHiZ` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "Depth-buffer collision"

### Z2-R-VFX-067 Collision events

- **Statement:** Collision and death events spawn children of the target emitter in the next frame, deterministically.
- **Covers:** Z2-I-VFX-05, Z2-I-VFX-10
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticleParity_Events` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "Event spawning"

### Z2-R-VFX-068 Precipitation occlusion

- **Statement:** The precipitation occlusion module kills particles below ENV's occlusion capture and is inert, and counted, without one.
- **Covers:** Z2-I-VFX-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticlePrecipitation_OcclusionFallback` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "Precipitation occlusion"; Zenith2/Docs/Design/VFX/Authoring.md → "Weather effects for ENV"

### Z2-R-VFX-069 No physics dependency

- **Statement:** Particle collision uses VFX colliders and the depth buffer alone; ZenithVFX declares no edge to ZenithPhysics.
- **Covers:** Z2-I-VFX-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-024
- **Budgets:** none
- **Acceptance evidence:** `Libgraph` gate step on the scaffold ticket
- **Design:** Zenith2/Docs/Design/VFX.md → "Library dependencies"; Zenith2/Docs/Design/VFX.md → "VFX rulings"

### Z2-R-VFX-070 Baked bounds

- **Statement:** The cook bakes each emitter's emitter-frame bounds (an event target's spawn-relative box) with the CPU reference simulation over its seeds (world terms excluded, wind relaxing towards zero wind, distance spawning at its reference rate), expanded by draw extents and margin, and computes the world displacement box with its cube flag, the inherited-velocity gain, the wind gain, the bounds history and the estimate flag analytically, composed along event chains, so held-out seeds of the linear module class stay inside the composed bounds on rotated and moving tracks, event targets and under a uniform 40 m/s wind included.
- **Covers:** Z2-I-VFX-06
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXCook_BoundsBake` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Authoring.md → "Bounds baking"; Zenith2/Docs/Design/VFX/PublicAPI.md → "Zenith_VFXReference header"

### Z2-R-VFX-071 Fixed bounds and code-authored types

- **Statement:** Fixed bounds are copied from the document, and an emitter with a code-authored update type under baked bounds fails the cook.
- **Covers:** Z2-I-VFX-06, Z2-I-VFX-12
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXCook_BoundsBake` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Authoring.md → "Bounds baking"

### Z2-R-VFX-072 Runtime bounds

- **Statement:** Runtime bounds compose the transformed emitter-frame box over the emitter's bounds history, the world displacement box (a cube for local-space or colliding emitters) and the inherited-velocity term, an event target composing from its chain root, and cover every particle the reference simulation produces over the linger for rotated, moving, distance-spawning, inheriting and event-target emitters of the linear module class.
- **Covers:** Z2-I-VFX-06, Z2-I-VFX-07
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXBounds_AttachedAndWorldSpace` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "Bounds"

### Z2-R-VFX-073 GPU bounds validation

- **Statement:** In development builds the GPU measures each emitter's bounds and live count into a validation record read back one to F frames late, and reports when a particle leaves the CPU bounds; nothing but diagnostics reads it and Shipping builds do not measure.
- **Covers:** Z2-I-VFX-06
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticleBounds_ValidationDetectsEscape` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "GPU bounds validation"

### Z2-R-VFX-074 Bake reproducible

- **Statement:** Cooking the same effect twice produces byte-identical files.
- **Covers:** Z2-I-VFX-06
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXCook_Reproducible` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Authoring.md → "Bounds baking"; Zenith2/Docs/Design/VFX/Tests.md → "Determinism tests"

### Z2-R-VFX-075 Per-view frustum culling

- **Statement:** Each particle-drawing view draws only the emitters of its world whose instance bounds intersect its frustum.
- **Covers:** Z2-I-VFX-07
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-VFX-004
- **Acceptance evidence:** `Test_ParticleDraw_PerViewCulling` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Rendering.md → "Per-view emitter culling"

### Z2-R-VFX-076 Cull behaviour

- **Statement:** A culled `kill` emitter restarts empty on readmission and a culled `pause` emitter resumes its frozen particles.
- **Covers:** Z2-I-VFX-07, Z2-I-VFX-09
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXCulling_KillAndPause` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "Culling behaviour"

### Z2-R-VFX-077 View feature gating

- **Statement:** Shadow views never draw particles, and a preview view draws them only when its request sets the particles feature bit.
- **Covers:** Z2-I-VFX-07, Z2-I-VFX-12
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-059
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticleDraw_PerViewCulling`, `Test_ParticleDraw_PreviewViewFeature` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Rendering.md → "Per-view emitter culling"

### Z2-R-VFX-078 Per-view draw limit

- **Statement:** A view draws at most 4,096 particle draw commands, dropping the least significant emitters and counting them.
- **Covers:** Z2-I-VFX-07, Z2-I-VFX-11
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticleDraw_ViewDrawLimit` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Rendering.md → "Per-view emitter culling"

### Z2-R-VFX-079 Dead slots draw nothing

- **Statement:** A slot without a live particle produces a degenerate primitive and no pixel.
- **Covers:** Z2-I-VFX-07
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticleDraw_DegenerateDeadSlots` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Rendering.md → "Sprite rendering"

### Z2-R-VFX-080 Stable radix sort

- **Statement:** The particle sort is a stable four-pass LSD radix sort whose output equals a stable sort of the same pairs.
- **Covers:** Z2-I-VFX-08
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-VFX-003
- **Acceptance evidence:** `Test_ParticleSort_StableAndCorrect` (`Test`, WARP and lavapipe)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "ParticleSort algorithm"; Zenith2/Docs/Design/VFX/Rendering.md → "ParticleSort table"

### Z2-R-VFX-081 Sort partition edge cases

- **Statement:** Key counts at and around the partition size sort correctly.
- **Covers:** Z2-I-VFX-08
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticleSort_PartitionBoundaries` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "ParticleSort algorithm"

### Z2-R-VFX-082 Per-emitter sort segments

- **Statement:** Each depth-sorted emitter's particles stay in their own segment, dead keys first and then live particles back to front.
- **Covers:** Z2-I-VFX-08
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticleSort_SegmentsPerEmitter` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "ParticleCompactAndSortKeys dispatch"

### Z2-R-VFX-083 Sort view and emitter ranks

- **Statement:** The sort view is chosen by the documented precedence, and each view ranks its blended emitters back to front by bounds depth plus sort bias.
- **Covers:** Z2-I-VFX-08
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticleSortView_Selection`, `Test_ParticleDraw_SortedBackToFront` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Rendering.md → "Emitter ranks and the sort view"

### Z2-R-VFX-084 Two draw partitions

- **Statement:** Blended emitters draw first in rank order with premultiplied blending and additive emitters after them in key order.
- **Covers:** Z2-I-VFX-08
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-VFX-004
- **Acceptance evidence:** `Test_ParticleDraw_PartitionOrder` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Rendering.md → "Partitions"; Zenith2/Docs/Design/VFX/Rendering.md → "Order within an emitter"

### Z2-R-VFX-085 Sorted-particle budget

- **Statement:** Depth-sorted emitters beyond `vfx.sortedParticlesPerFrame` or `vfx.sortedEmittersPerFrame` draw in their unsorted order and are counted.
- **Covers:** Z2-I-VFX-08, Z2-I-VFX-11
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-VFX-003
- **Acceptance evidence:** `Test_VFXAdmission_SortedBudget` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "Sorted-particle admission"

### Z2-R-VFX-086 LOD viewers

- **Statement:** LOD and significance use the world's main camera plus at most eight enabled viewer components, the nearest viewer deciding.
- **Covers:** Z2-I-VFX-09
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXLOD_ViewerSet` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "Viewers"

### Z2-R-VFX-087 Screen size and hysteresis

- **Statement:** The LOD level changes at each threshold scaled by `1 ± hysteresis`, and `cullScreenSize` and `maxDistance` cull.
- **Covers:** Z2-I-VFX-09, Z2-I-VFX-07
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXLOD_ScreenSizeAndHysteresis` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "Screen size and LOD selection"

### Z2-R-VFX-088 LOD levels disable modules

- **Statement:** A LOD level skips its listed update modules and scales spawning by its spawn scale times `vfx.spawnScale`.
- **Covers:** Z2-I-VFX-09
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXLOD_LevelDisablesModules` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/EffectFormat.md → "LOD keys"; Zenith2/Docs/Design/VFX/Simulation.md → "Screen size and LOD selection"

### Z2-R-VFX-089 Tier defaults

- **Statement:** The S24 default tier resolves the S24 LOD bias, spawn scale, lighting mode and budgets, and desktop tiers the desktop values; a tier change reaches the module's settings and, through `Flux_Renderer::ApplyConfigChanges`, the particle system's budgets and lighting clamp from the next built frame without changing any restart member.
- **Covers:** Z2-I-VFX-09, Z2-I-VFX-11
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-013
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXConfig_TierDefaults`, `Test_ParticleSystem_TierChangeAppliesBudgets` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX.md → "Configuration keys"; Zenith2/Docs/Design/VFX/Rendering.md → "Capability and quality tiers"

### Z2-R-VFX-090 Event ring capacity

- **Statement:** A source emitter's event ring holds exactly `C_src × E` records indexed `(ordinal mod C_src) × E + e`, `e` the compact event index (collisions `0 … m − 1`, death `m`), so it is a slot array that is never full and no record is ever dropped, and every raised event spawns its children in the source's next executed step; cleared records spawn none.
- **Covers:** Z2-I-VFX-10
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticleEvents_RingCapacity` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "Event spawning"

### Z2-R-VFX-091 Effect lights

- **Statement:** An emitter's light module produces LIGHT's light deltas with the named key, the intensity curve and linger fade, never shadow-casting, within `vfx.maxEffectLights` per world.
- **Covers:** Z2-I-VFX-10
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXExtract_EffectLightsAndDecals` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Rendering.md → "Effect lights"

### Z2-R-VFX-092 Effect decals

- **Statement:** An emitter's decal module writes GEOM decal records with a deterministic stable key and fade, within `vfx.maxEffectDecals` per world.
- **Covers:** Z2-I-VFX-10
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXExtract_EffectLightsAndDecals` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Rendering.md → "Effect decals"

### Z2-R-VFX-093 Weather effects for ENV

- **Statement:** ENV's precipitation is authorable as a world-space GPU emitter whose spawn volume follows the anchor while its particles fall in world space, killed behind the camera by an inverted kill volume, splashing through depth-collision events; the wind and precipitation occlusion modules read the frame environment on the GPU, so ENV's weather prefabs need no VFX code of their own.
- **Covers:** Z2-I-VFX-02, Z2-I-VFX-10
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticleDraw_WeatherRainFollowsCamera`, `Test_ParticleWind_FrameEnvironment`, `Test_ParticlePrecipitation_OcclusionFallback` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Authoring.md → "Weather effects for ENV"

### Z2-R-VFX-094 Stage 1 budget values

- **Statement:** The Stage 1 per-system budget ticket for VFX, ordered after the CORE, JOBS, RPROD and SCRIPT Stage 1 budget tickets, writes in one diff the values of `Z2-B-VFX-001` … `Z2-B-VFX-004` and of the two pending rows "VFX lifecycle CPU" and "VFX trigger queue capacity per world" (cross-doc item X263), the trigger-queue script reserve default (`Z2-B-SCRIPT-024` × `Z2-B-JOBS-002`) and capacity, VFX's allocation and the DecisionLog entry, from reference-engine or published data, and confirms that the CORE ruling's `Z2-B-CORE-005` allocation covers VFX's per-world "CPU particle output" demand line, an input of that earlier ruling.
- **Covers:** Z2-I-VFX-11
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-012, Z2-D-097
- **Budgets:** Z2-B-VFX-001, Z2-B-VFX-002, Z2-B-VFX-003, Z2-B-VFX-004, Z2-B-SCRIPT-024, Z2-B-JOBS-002, Z2-B-CORE-005
- **Acceptance evidence:** the `needs-human` ruling ticket "Stage 1 per-system budget ticket for VFX" with its `Check-Allocation.ps1` run
- **Design:** Zenith2/Docs/Design/VFX.md → "Owned rows"; Zenith2/Docs/Design/VFX.md → "Proposed values"; Zenith2/Docs/Design/VFX.md → "Pending row needs"; Zenith2/Docs/Design/VFX.md → "Stage 1"; Zenith2/Docs/Design/VFX/Simulation.md → "CPU particle output"

### Z2-R-VFX-095 Configuration keys

- **Statement:** Every `vfx.*` key is registered with its type, domain, desktop and S24 defaults, and an out-of-domain value takes its default with one warning.
- **Covers:** Z2-I-VFX-11
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXConfig_KeysRegistered` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX.md → "Configuration keys"

### Z2-R-VFX-096 Particles draw GPU time

- **Statement:** The particle and ribbon draws of the `W-Cinematic` and `W-OpenWorld` workloads stay within `Z2-B-VFX-004` per view on Windows and the S24.
- **Covers:** Z2-I-VFX-03, Z2-I-VFX-04, Z2-I-VFX-11
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-VFX-004
- **Acceptance evidence:** the Stage 5 acceptance budget run (`needs-human`, `gpu-reason:visual-measure`)
- **Design:** Zenith2/Docs/Design/VFX.md → "Owned rows"

### Z2-R-VFX-097 Counters published

- **Statement:** Every counter of "Counters" is registered once and moves when its condition occurs, and the `vfx` tab's rows and the panel provider show them.
- **Covers:** Z2-I-VFX-11
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXDiagnostics_CountersPublished`, `Test_VFXPanelProvider_BuildsRows` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX.md → "Counters"; Zenith2/Docs/Design/VFX.md → "On-screen profiler rows"

### Z2-R-VFX-098 Memory pressure shedding

- **Statement:** At WARNING and CRITICAL the particle budgets scale by 0.75 and 0.5 within one frame and revert one frame after the level falls, never changing simulation.
- **Covers:** Z2-I-VFX-11
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-VFX-001
- **Acceptance evidence:** `Test_ParticleSystem_PressureShedsAndReverts` (`Test`); the VFX row in the memory-shedding case table `Zenith2/Tests/Memory/Internal/Zenith_MemorySheddingCases.h`, run by the memory integration suite (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "Memory pressure response"

### Z2-R-VFX-099 Pool full behaviour

- **Statement:** An emitter whose pool region is at its maximum stays unallocated and undrawn, is counted, and allocates once space frees.
- **Covers:** Z2-I-VFX-11, Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-VFX-001
- **Acceptance evidence:** `Test_ParticleSystem_PoolFullRetries` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Rendering.md → "Render-side failure behaviour"; Zenith2/Docs/Design/VFX/Simulation.md → "Emitter pool ranges"

### Z2-R-VFX-100 Maximum-capacity stress

- **Statement:** One GPU emitter at 262,144 particles runs 600 frames with no validation error and no non-finite particle.
- **Covers:** Z2-I-VFX-02, Z2-I-VFX-11
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXStress_MaxCapacityFountain` (`z2-stress`)
- **Design:** Zenith2/Docs/Design/VFX/Tests.md → "Stress tests"

### Z2-R-VFX-101 Churn stress

- **Statement:** Ten thousand detached effects and a thousand effect load cycles return the pool, instance table, references and allocations to empty.
- **Covers:** Z2-I-VFX-10, Z2-I-VFX-11
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXStress_EmitterChurn`, `Test_VFXStress_AssetChurn` (`z2-stress`)
- **Design:** Zenith2/Docs/Design/VFX/Tests.md → "Stress tests"

### Z2-R-VFX-102 Debug views

- **Statement:** Each particle debug flag draws its debug view in development builds and is ignored in Shipping.
- **Covers:** Z2-I-VFX-11, Z2-I-VFX-12
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticleDebug_ViewsRender` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX.md → "Debug views"; Zenith2/Docs/Design/VFX/PublicAPI.md → "Flux_ParticleSystem"

### Z2-R-VFX-103 Sprite facing modes

- **Statement:** Sprites face by the five facing modes with the documented bases, pivot and velocity stretch.
- **Covers:** Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-VFX-004
- **Acceptance evidence:** `Test_ParticleDraw_SpriteFacing` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Rendering.md → "Facing modes"; Zenith2/Docs/Design/VFX/Rendering.md → "Sprite rendering"

### Z2-R-VFX-104 Sub-UV flipbooks

- **Statement:** Flipbook frames and blends follow the over-life and frames-per-second formulas with explicit-level sampling, and a particle's sub-UV start frame (0 … 255) fits its state's eight flag bits 24–31 for every `subUV` module of at most 256 frames.
- **Covers:** Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticleDraw_SubUVBlend` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Rendering.md → "Sub-UV flipbooks"

### Z2-R-VFX-105 Soft particles

- **Statement:** Soft particles fade linearly over their soft distance of linear view depth.
- **Covers:** Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticleDraw_SoftParticleFade` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Rendering.md → "Soft particles"

### Z2-R-VFX-106 Fog and pre-exposure

- **Statement:** Particle radiance is pre-exposed; a blended particle's fogged output is `c_p × T + L × o` (in-scattered light scaled by its opacity) and an additive particle's `c_p × T`, both through LIGHT's `Flux_ApplyFroxelFog`.
- **Covers:** Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticleDraw_FogAdditiveTransmittanceOnly`, `Test_ParticleDraw_PreExposure` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Rendering.md → "Fog and pre-exposure"

### Z2-R-VFX-107 Lit particles

- **Statement:** Vertex- and pixel-lit particles evaluate LIGHT's lighting with the documented surface and the instance's lighting channels.
- **Covers:** Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticleDraw_LitModes` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Rendering.md → "Lighting modes"

### Z2-R-VFX-108 Mesh particles

- **Statement:** Mesh particles draw the mesh's LOD 0 section per particle without depth writes, and nothing while the mesh is not resident.
- **Covers:** Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticleDraw_MeshParticles` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Rendering.md → "Mesh particle rendering"

### Z2-R-VFX-109 Indirect draw runs

- **Statement:** Each view's particle draws are CPU-written count-less indirect commands issued as one call per PSO run.
- **Covers:** Z2-I-VFX-02, Z2-I-VFX-03, Z2-I-VFX-04
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-054
- **Budgets:** Z2-B-VFX-004
- **Acceptance evidence:** `Test_ParticleDraw_IndirectRunsPerPSO` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Rendering.md → "Draw records and indirect commands"

### Z2-R-VFX-110 Draw pass declarations

- **Statement:** The `Particles` pass declares the fields, targets, load and store operations and blend states of its table.
- **Covers:** Z2-I-VFX-02, Z2-I-VFX-03, Z2-I-VFX-04
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-055, Z2-D-056
- **Budgets:** Z2-B-VFX-004
- **Acceptance evidence:** `Test_ParticlePasses_DeclareTableFields` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Rendering.md → "Particles table"

### Z2-R-VFX-111 Backend and path parity

- **Statement:** The particle parity scenes agree between Vulkan and D3D12 and between Forward and Forward+ within their tolerances in one run.
- **Covers:** Z2-I-VFX-02, Z2-I-VFX-03, Z2-I-VFX-04
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-035
- **Budgets:** none
- **Acceptance evidence:** `ParticlesBasic`, `ParticlesLit`, `ParticlesDebris` under `BackendParity` and `PathParity`
- **Design:** Zenith2/Docs/Design/VFX/Tests.md → "Render tests"; Zenith2/Docs/Design/VFX/Rendering.md → "Capability and quality tiers"

### Z2-R-VFX-112 PSO precache

- **Statement:** The precache set holds every particle PSO the cooked effects can bind, so a precached frame records no particle PSO miss.
- **Covers:** Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-025
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticlePSO_PrecacheCoversCookedEffects` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Rendering.md → "PSO keys and permutations"

### Z2-R-VFX-113 Shader rules

- **Statement:** Particle programs never discard and never write depth, and pass the shader lints.
- **Covers:** Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-014
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticleShaders_NoDiscardNoDepthWrite` (`Test`), `ShaderChecks` gate step
- **Design:** Zenith2/Docs/Design/VFX/Rendering.md → "Blend modes"; Zenith2/Docs/Design/VFX/Tests.md → "Gate steps"

### Z2-R-VFX-114 Texture fallback

- **Statement:** A sprite texture not yet resident draws the white fallback and is counted.
- **Covers:** Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticleDraw_TextureFallback` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Rendering.md → "Render-side failure behaviour"

### Z2-R-VFX-115 Effect document schema

- **Statement:** Effect documents follow the schema of "Effect document", and the cook reports every error of a document in one pass in check order with its JSON pointer.
- **Covers:** Z2-I-VFX-01, Z2-I-VFX-02, Z2-I-VFX-12
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-079, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXCook_DocumentValidationOrder` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/EffectFormat.md → "Top-level keys"; Zenith2/Docs/Design/VFX/EffectFormat.md → "Document validation"

### Z2-R-VFX-116 Effect identity

- **Statement:** A document's `id` equals its cook item's GUID, and the step mints no GUID.
- **Covers:** Z2-I-VFX-01, Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXCook_IdMustMatchGuid` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Authoring.md → "Importer and intermediate"

### Z2-R-VFX-117 Curve baking

- **Statement:** Curves bake to 32-sample tables evaluated identically on both simulators, deduplicated bit-exactly, with a warning beyond 1% error.
- **Covers:** Z2-I-VFX-01, Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXCook_CurveBakeAndDedup` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/EffectFormat.md → "Curve baking"

### Z2-R-VFX-118 Capacity sizing

- **Statement:** Automatic capacities are multiples of 64 following the estimate formulas, which cover the maximum rate times the maximum lifetime because a death frees no slot; explicit capacities are kept, and both warn when below the estimate or the measured peak.
- **Covers:** Z2-I-VFX-01, Z2-I-VFX-02, Z2-I-VFX-11
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXCook_CapacitySizing` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Authoring.md → "Capacity sizing"

### Z2-R-VFX-119 Cooked effect format

- **Statement:** The `.zvfx` file has ASSET's header and the ten sections with the documented layouts, little-endian, written and read field by field.
- **Covers:** Z2-I-VFX-01, Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_TestEffectBuilder_MatchesCook`, `Test_VFXCook_Reproducible` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/EffectFormat.md → "Cooked effect format"; Zenith2/Docs/Design/VFX/EffectFormat.md → "Section kinds"

### Z2-R-VFX-120 Reader rejects corrupt files

- **Statement:** The effect reader refuses every corrupt, mismatched or over-limit file with its error and never asserts or reads out of bounds.
- **Covers:** Z2-I-VFX-01, Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticleEffectReader_RejectsCorrupt` (`Test`); `Zenith_Fuzz_ParticleEffect` (`Sanitizers -Preset win-asan`)
- **Design:** Zenith2/Docs/Design/VFX/EffectFormat.md → "Reader validation"

### Z2-R-VFX-121 Format limits

- **Statement:** Each format limit loads at the limit and is refused at the limit plus one.
- **Covers:** Z2-I-VFX-01, Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticleEffect_LimitsAndLimitPlusOne` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/EffectFormat.md → "Format limits"; Zenith2/Docs/Design/VFX/EffectFormat.md → "Document limits"

### Z2-R-VFX-122 Document migration

- **Statement:** The effect document type migrates through one step and fixture pair per version with unknown fields preserved, and cooked files re-cook on a format change.
- **Covers:** Z2-I-VFX-01, Z2-I-VFX-02, Z2-I-VFX-12
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXDocumentMigration_Fixtures` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/EffectFormat.md → "Effect document migration"

### Z2-R-VFX-123 Cook key inputs

- **Statement:** The effect cook key covers the document, each type's reflection and each mesh's published bounds, and nothing a cooked effect does not depend on.
- **Covers:** Z2-I-VFX-01, Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXCook_KeyInputs` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Authoring.md → "Effect cook step"

### Z2-R-VFX-124 Particle type set

- **Statement:** The `vfx.particleTypes` step writes the used particle types and (program, key, variant) triples, a lit emitter's keys for its lighting mode and every lower one, and a missing compiled triple fails the cook.
- **Covers:** Z2-I-VFX-02, Z2-I-VFX-12
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-025, Z2-D-015
- **Budgets:** Z2-B-RPROD-024
- **Acceptance evidence:** `Test_VFXCook_ParticleTypeSet` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Authoring.md → "Particle type set step"; Zenith2/Docs/Design/VFX/Rendering.md → "PSO keys and permutations"

### Z2-R-VFX-125 Cook report members

- **Statement:** The VFX cook steps add their report members with every listed field.
- **Covers:** Z2-I-VFX-01, Z2-I-VFX-02, Z2-I-VFX-12
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXCook_ReportMembers` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Authoring.md → "Cook report members"

### Z2-R-VFX-126 Effect kind loader

- **Statement:** The effect kind loader activates a CPU-resident effect, headless or not, with its resident bytes reported and no GPU resource created.
- **Covers:** Z2-I-VFX-01, Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-VFX-001
- **Acceptance evidence:** `Test_ParticleEffectLoader_CpuResidentHeadless` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/PublicAPI.md → "Effect kind loader"

### Z2-R-VFX-127 Missing effect plays nothing

- **Statement:** A component naming a missing effect plays nothing, is counted, and starts once a mount provides the effect.
- **Covers:** Z2-I-VFX-01, Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXEffect_MissingAssetPlaysNothing` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX.md → "Failure table"

### Z2-R-VFX-128 Engine effect content

- **Statement:** The engine effects cook with no error or warning and render in the particle parity scenes.
- **Covers:** Z2-I-VFX-01, Z2-I-VFX-02, Z2-I-VFX-03, Z2-I-VFX-04, Z2-I-VFX-12
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXEngineContent_CooksClean` (`Test`); the `Particles*` parity scenes
- **Design:** Zenith2/Docs/Design/VFX/Authoring.md → "Engine effect content"

### Z2-R-VFX-129 Preview session

- **Statement:** An effect preview world (purpose `WORLD_PURPOSE_PREVIEW`) draws its effect through the runtime path into a preview view that requests the particles feature; depth collision and fog history are absent there, as stated.
- **Covers:** Z2-I-VFX-12
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-079, Z2-D-059
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXPreview_SessionDrawsEffect` (`Test`, WARP and lavapipe)
- **Design:** Zenith2/Docs/Design/VFX/Authoring.md → "Preview session"

### Z2-R-VFX-130 Timeline scrubbing

- **Statement:** A preview seek re-simulates from effect time 0 in fixed steps of the preview world's tick duration, executed in arrival order within each frame and drawn after the frame's last step, and the live particle state equals playback from the start at one tick per frame (the same live set, every live slot field for field with the generation byte masked), event children included.
- **Covers:** Z2-I-VFX-12
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-079, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXPreview_SeekMatchesPlayback` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Authoring.md → "Timeline scrubbing"; Zenith2/Docs/Design/VFX/Simulation.md → "Catch-up steps"

### Z2-R-VFX-131 Catch-up only in previews

- **Statement:** Catch-up steps and freeze run at most `vfx.preview.catchUpStepsPerFrame` steps per frame and only in worlds whose purpose is `WORLD_PURPOSE_PREVIEW`; play and edit worlds ignore the preview control and count it.
- **Covers:** Z2-I-VFX-12
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXSchedule_CatchUpOnlyInPreview`, `Test_ParticleCatchUp_StepsInOneFrame` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "Catch-up steps"; Zenith2/Docs/Design/VFX/PublicAPI.md → "Zenith_Module_VFX header"

### Z2-R-VFX-132 Preview statistics

- **Statement:** Per-instance statistics, copied on the main thread from the snapshot of frame k − 2, report capacities, spawns, LOD levels, admission, the CPU-known live upper bound and, with validation, the measured bounds and live count for the preview and inspector.
- **Covers:** Z2-I-VFX-12, Z2-I-VFX-13
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticleSystem_InstanceStats` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Authoring.md → "Preview statistics"; Zenith2/Docs/Design/VFX/PublicAPI.md → "Flux_ParticleStats"

### Z2-R-VFX-133 Effect hot reload

- **Statement:** A reloaded effect restarts its attached instances, destroys its detached ones and swaps its constants buffer, and a failed reload keeps the old effect.
- **Covers:** Z2-I-VFX-12
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXReload_EffectSwapRestartsInstances` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Authoring.md → "Hot reload"

### Z2-R-VFX-134 Inspector parameter data

- **Statement:** The effect asset exposes its declared parameters' names, slots, types and defaults, which the inspector shows and writes on assignment.
- **Covers:** Z2-I-VFX-13
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticleEffect_ParameterTable` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Authoring.md → "Effect inspector"; Zenith2/Docs/Design/VFX/PublicAPI.md → "Zenith_ParticleEffectAsset"

### Z2-R-VFX-135 Edit-world play controls

- **Statement:** In an edit world, play and stop requests by command behave as in a play world.
- **Covers:** Z2-I-VFX-13
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXEditWorld_PlayControls` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Authoring.md → "Effect inspector"

### Z2-R-VFX-136 Runtime-spawned attached effects distinct

- **Statement:** Two entities spawned at run time (null persistent IDs) carrying effects in one world get distinct render-side instances and distinct default seeds, and their seeds survive a save and load into a world with another tag.
- **Covers:** Z2-I-VFX-10
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXInstanceKey_RuntimeSpawnedEntitiesDistinct` (`Test`, WARP)
- **Design:** Zenith2/Docs/Design/VFX/PublicAPI.md → "Instance keys"; Zenith2/Docs/Design/VFX/PublicAPI.md → "Zenith_VFXEffectComponent"

### Z2-R-VFX-137 Event children slots never collide

- **Statement:** An auto-sized event target's capacity covers its children's ordinal span in the chain form: for a target `t` at depth `d` on the path `a_0 → … → a_d = t` from its chain root, `(Π_{i=1…d} k_i × E_edge,i) × (ceil(r_root × (W + Δ_max)) + B_root(W) + n_prewarm,root)` with `W = L_max,t + Σ_{i=0…d−1} (L_max,a_i − d_min,a_i + Δ_max)` (at depth 1 `k × E_edge × (ceil(r_src × (W + Δ_max)) + B_src(W) + n_prewarm,src)` with `W = L_max + L_src,max − d_min + Δ_max`), where the earliest event age `d_min` is 0 for a collision edge and for an ancestor with any killing module (`collision` kill or `killAfter`, `killVolume`, `attractor` kill radius, `precipitationOcclusion`, a `custom` type with `bKILLS`) or a parameter-bound lifetime, else `L_min − Δ_max`; a target at depth 2 or more has a power-of-two capacity and slot `o & (C − 1)`, so its wrapping 32-bit child ordinals keep their slots continuous; so no live child is overwritten however and whenever its ancestors die, at any chain depth (distance-spawning roots up to the 20 m/s reference speed).
- **Covers:** Z2-I-VFX-10
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXCook_EventTargetCapacityVariedLifetimes` (`Test`); `Test_ParticleParity_EventsVariedLifetimes` (`Test`, WARP and lavapipe)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "Event spawning"; Zenith2/Docs/Design/VFX/Authoring.md → "Capacity sizing"

### Z2-R-VFX-138 Event freshness by simulate step

- **Statement:** Event records are tagged with the source's simulate-step counter, which every executed step (catch-up steps included) advances and a deferred or failed frame leaves unchanged, and a target consumes each tag once, so seeks and deferred frames keep every event child.
- **Covers:** Z2-I-VFX-10, Z2-I-VFX-12
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticleEvents_DeferredFrameKeepsChildren`, `Test_ParticleCatchUp_StepsInOneFrame`, `Test_VFXPreview_SeekMatchesPlayback` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "Event spawning"; Zenith2/Docs/Design/VFX/Simulation.md → "Deferred and failed frames"; Zenith2/Docs/Design/VFX/PublicAPI.md → "Flux_EmitterCommand"

### Z2-R-VFX-139 Event graphs admitted as one unit

- **Statement:** CPU and GPU admission admit, refuse or pause the emitters of a connected event graph together, so a source never advances a step its target misses.
- **Covers:** Z2-I-VFX-10, Z2-I-VFX-11
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-VFX-003
- **Acceptance evidence:** `Test_VFXAdmission_GpuPerFrame`, `Test_VFXCpuSim_WorkerCountIndependent` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "GPU admission per frame"; Zenith2/Docs/Design/VFX/Simulation.md → "CPU admission per world"; Zenith2/Docs/Design/VFX/Simulation.md → "CPU update algorithm"

### Z2-R-VFX-140 Shutdown releases render side first

- **Statement:** At shutdown the module destroys its callback tokens, then `DisableParticles` releases every render-side effect handle, then the kind loader is unregistered with no referenced effect and destroyed.
- **Covers:** Z2-I-VFX-01, Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXModule_ShutdownOrder` (`Test`, WARP; slice 7), `Test_VFXModule_ShutdownReleasesRenderSideFirst` (`Test`, WARP; slice 11)
- **Design:** Zenith2/Docs/Design/VFX/PublicAPI.md → "Module shutdown order"; Zenith2/Docs/Design/VFX.md → "Lifetime"

### Z2-R-VFX-141 Render-side budgets follow tier changes

- **Statement:** The non-restart render-side keys reach the particle system through `Flux_Renderer::ApplyConfigChanges` and take effect at the next built frame; the simulation-side keys reach `VFX.Update` through the module's change callback.
- **Covers:** Z2-I-VFX-11
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-VFX-003, Z2-B-VFX-004
- **Acceptance evidence:** `Test_ParticleSystem_TierChangeAppliesBudgets` (`Test`, WARP)
- **Design:** Zenith2/Docs/Design/VFX.md → "Runtime configuration changes"; Zenith2/Docs/Design/VFX/PublicAPI.md → "Flux_ParticleSystem"

### Z2-R-VFX-142 On-screen profiler rows and panel

- **Statement:** VFX's `vfx` tab rows compile in DEVT's catalogue, each BAR row carrying its full-scale `m_szLimit` (the GPU-work bar against the admission budget, the memory bar against the pool capacity), and its panel provider, registered at Start through the start context, shows the particle system's statistics.
- **Covers:** Z2-I-VFX-11, Z2-I-VFX-12
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-VFX-001, Z2-B-VFX-002, Z2-B-VFX-003, Z2-B-VFX-004
- **Acceptance evidence:** `Test_VFXPanelProvider_BuildsRows` (`Test`); DEVT's `Test_ProfilerPanels_CatalogueCompiles` over `Zenith_PanelsVFX.cpp`
- **Design:** Zenith2/Docs/Design/VFX.md → "On-screen profiler rows"

### Z2-R-VFX-143 Detached start time

- **Statement:** A detached effect starts at effect time `max(0, T_k − n_rec × t_tick + leadSeconds)` from its record's tick and lead alone, the same on every run.
- **Covers:** Z2-I-VFX-10
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXDetached_StartTimeFromRecordingTick` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "Detached effects and the trigger queue"; Zenith2/Docs/Design/VFX/PublicAPI.md → "Zenith_VFXCommands header"

### Z2-R-VFX-144 Ordinal reset before wrap

- **Statement:** An emitter resets to a new generation at ordinal 0 in the frame whose next ordinal `o'` would make `o' × P` pass `2³² − 2²⁴` (`P` = 1, or the largest `E_edge × k` of an event source's edges), an event source together with its whole event graph, so neither a spawn ordinal nor a depth-1 child ordinal ever wraps and `ordinal mod C` stays continuous; the child ordinals of a target at depth 2 or more may wrap, and its power-of-two capacity keeps `o mod C = o & (C − 1)` continuous across the wrap, so no live child is overwritten at any depth.
- **Covers:** Z2-I-VFX-01, Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXSchedule_OrdinalResetBeforeWrap` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "Spawn ordinals and capacity"

### Z2-R-VFX-145 Generation wrap clears ranges

- **Statement:** When an emitter's 8-bit generation wraps from 255 to 0 its state, trail and event ranges are cleared, so no particle or record of an old generation 0 reads as live.
- **Covers:** Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticleSystem_GenerationWrapClearsRange` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "Emitter pool ranges"

### Z2-R-VFX-146 Instance timeout

- **Statement:** A render-side instance without a command for two consecutive submitted frames is destroyed and counted; deferred frames do not count.
- **Covers:** Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticleSystem_InstanceTimeout` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "Emitter pool ranges"

### Z2-R-VFX-147 Command stream backend independent

- **Statement:** The emitter command stream of a fixed scenario is identical on the three ECS backends.
- **Covers:** Z2-I-VFX-10
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXExtract_BackendIndependent` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "Determinism rules"; Zenith2/Docs/Design/VFX/Simulation.md → "Commands per frame"

### Z2-R-VFX-148 Document validator fuzzed

- **Statement:** The effect document validator never crashes or asserts on mutated JSON and reports at least one cook error code for every document it refuses.
- **Covers:** Z2-I-VFX-12
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Zenith_Fuzz_VFXDocument` (`Sanitizers -Preset win-asan` smoke run; long run in stage acceptance)
- **Design:** Zenith2/Docs/Design/VFX/EffectFormat.md → "Document validation"; Zenith2/Docs/Design/VFX/Tests.md → "Failure and fault-injection tests"

### Z2-R-VFX-149 Pending triggers never saved

- **Statement:** The trigger queue registers no serialized property, so a saved world carries no pending trigger and a load starts with an empty queue.
- **Covers:** Z2-I-VFX-10
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXSave_TriggerQueueNotSaved` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "Detached effects and the trigger queue"

### Z2-R-VFX-150 Authority census rows

- **Statement:** VFX's five components, its trigger-queue resource and its two events appear in the authority census in its twelve columns (library `ZenithVFX`, stage S5, property IDs and names) with their owning phase, writers and readers, in the diff that registers them, and VFX's registration-shape test asserts those authorities against the registry.
- **Covers:** Z2-I-VFX-10
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXEffectComponent_Registration` (`Test`); the census rows are checked by review in the registering diff, because GAME's census table assigns no census test to a module no Lifecycle suite links
- **Design:** Zenith2/Docs/Design/VFX/PublicAPI.md → "Authority census rows"

### Z2-R-VFX-151 Sort fallback identical

- **Statement:** The reduce-then-scan form of the radix sort, selected by `vfx.sort.algorithm`, writes the same sorted pairs as the single-pass form and never waits across workgroups.
- **Covers:** Z2-I-VFX-08
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-VFX-003
- **Acceptance evidence:** `Test_ParticleSort_ReduceThenScanMatchesOnesweep` (`Test`); the S24 sort stress of the Stage 5 device run (`needs-human`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "Reduce-then-scan fallback"; Zenith2/Docs/Design/VFX/Rendering.md → "ParticleSort table"

### Z2-R-VFX-152 Depth collision within its world

- **Statement:** Depth collision applies only to world-space GPU emitters of the sort view's world with a previous HiZ; other emitters run without it and are counted.
- **Covers:** Z2-I-VFX-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticleDepthCollision_PreviousHiZ` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "Depth-buffer collision"

### Z2-R-VFX-153 ZenithFlux API slice links RPROD paths

- **Statement:** Before the particle passes exist, the API slice's definitions let RPROD's path functions and a `BuildGraph` that calls the particle functions link and run, declaring no particle pass, and let VFX's module start and shutdown enable and disable a real, empty particle system through the RPROD-declared renderer members.
- **Covers:** Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-085
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticleDraw_NoParticleFrameDeclaresNoPass`, `Test_ParticleSystem_EnableDisableLifecycle`, `Test_VFXModule_ShutdownOrder` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/PublicAPI.md → "Flux_ParticleSystem internals"; Zenith2/Docs/Design/VFX/PublicAPI.md → "Flux_ParticleSystem"; Zenith2/Docs/Design/VFX.md → "Stage 5 slices"

### Z2-R-VFX-154 Catch-up steps executed in arrival order

- **Statement:** An emitter's commands of a frame are steps in arrival order (kept steps first, the unflagged command last), executed as emit-then-simulate per step in ascending step order with one `ParticleEmit` and one `ParticleSimulate` pass per step, and only the last step is drawn, so the live particle state drawn in a seek frame equals one-tick-per-frame playback after the same number of steps (generation byte masked).
- **Covers:** Z2-I-VFX-02, Z2-I-VFX-12
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-079, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticleCatchUp_StepsInOneFrame` (`Test`, WARP and lavapipe)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "Catch-up steps"; Zenith2/Docs/Design/VFX/Simulation.md → "Frame building"; Zenith2/Docs/Design/VFX/PublicAPI.md → "Flux_ParticleFrame"

### Z2-R-VFX-155 Seek steps survive deferred frames

- **Statement:** A seeking emitter's steps that a deferred or failed frame or a GPU admission refusal did not execute are kept unmerged, in order, at most 64 per emitter, and executed first in the next frame that runs them; every command of a seek in progress carries `FLUX_EMITTER_FLAG_SEEK`, so a one-step seek frame is kept unmerged too; the live particle state after the seek equals one-tick-per-frame playback (generation byte masked); a seek whose kept steps would exceed 64 is interrupted (counted, reported in the instance stats, drawn as nothing until the next seek) and the preview session issues it again.
- **Covers:** Z2-I-VFX-12, Z2-I-VFX-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-079, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticleCatchUp_DeferredSeekKeepsSteps` (`Test`, WARP and lavapipe)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "Deferred and failed frames"; Zenith2/Docs/Design/VFX/Authoring.md → "Timeline scrubbing"; Zenith2/Docs/Design/VFX/PublicAPI.md → "Flux_ParticleSystem"

### Z2-R-VFX-156 Event consumers read their own edge

- **Statement:** Each event edge owns a contiguous range `[e_base, e_base + E_edge)` of its source's compact event slots, its target's batch runs `C_src × E_edge × k` threads, and a consumer accepts only records whose kind bit is its edge's `on` and whose index lies in its range.
- **Covers:** Z2-I-VFX-10, Z2-I-VFX-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticleEvents_ConsumerMatchesEdgeKind` (`Test`, WARP and lavapipe)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "Event spawning"; Zenith2/Docs/Design/VFX/PublicAPI.md → "Flux_ParticleEventRecord"; Zenith2/Docs/Design/VFX/PublicAPI.md → "Flux_EmitterCommand"

### Z2-R-VFX-157 Event-target bounds from the chain root

- **Statement:** An event target's runtime bounds compose its chain root's emitter-frame box and its spawn-relative box over its bounds history `H_t = L_t + Δ_max + H_source` with the chain's composed world displacement and inherited-velocity gain, so children spawned by falling, moving or inheriting sources are inside the bounds that culling reads.
- **Covers:** Z2-I-VFX-06, Z2-I-VFX-07, Z2-I-VFX-10
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXBounds_AttachedAndWorldSpace`, `Test_VFXCook_BoundsBake` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "Bounds"; Zenith2/Docs/Design/VFX/Authoring.md → "Bounds baking"; Zenith2/Docs/Design/VFX/EffectFormat.md → "Emitter record section"

### Z2-R-VFX-158 Wind term from the frame wind

- **Statement:** The cooked bounds exclude wind; the render side grows each instance's culling bounds by the largest `m_fWindGain × W_e`, `W_e` the largest `Flux_ParticleWindBound` of the instance's world over the emitter's bounds history, so particles carried by any wind ENV can produce stay inside the culled bounds.
- **Covers:** Z2-I-VFX-06, Z2-I-VFX-07
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_ParticleBounds_WindExpansion` (`Test`, WARP and lavapipe); `Test_VFXBounds_AttachedAndWorldSpace` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "Bounds"; Zenith2/Docs/Design/VFX/PublicAPI.md → "Particle wind bound"; Zenith2/Docs/Design/VFX/Rendering.md → "Per-view emitter culling"

### Z2-R-VFX-159 Bounds proven or flagged as estimates

- **Statement:** For emitters whose modules and chain belong to the linear class, the composed bounds' world, inherited and wind parts and their bucket unions are a proven bound at every step up to 1/15 s (the own displacement `½ a L (L + Δ_max)` of the semi-implicit integrator), around the emitter-frame box the cook measures at 1/60 s steps, which rests on `margin` and the development-build validation at larger run-time steps; every other emitter that combines a world term with `speedLimit`, `vortex`, `attractor` or local-space `turbulence`, or has `collision`, carries bounds flag bit 2 and the cook warning `VFX_COOK_WARNING_BOUNDS_ESTIMATE`.
- **Covers:** Z2-I-VFX-06
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_VFXCook_BoundsBake` (`Test`)
- **Design:** Zenith2/Docs/Design/VFX/Simulation.md → "Bounds"; Zenith2/Docs/Design/VFX/Authoring.md → "Bounds baking"; Zenith2/Docs/Design/VFX/EffectFormat.md → "Document validation"

## Coverage of inventory items

| Inventory item | Title | Requirements |
|---|---|---|
| Z2-I-VFX-01 | CPU particles | Z2-R-VFX-001, Z2-R-VFX-002, Z2-R-VFX-003, Z2-R-VFX-004, Z2-R-VFX-005, Z2-R-VFX-006, Z2-R-VFX-007, Z2-R-VFX-008, Z2-R-VFX-009, Z2-R-VFX-010, Z2-R-VFX-023, Z2-R-VFX-024, Z2-R-VFX-025, Z2-R-VFX-026, Z2-R-VFX-027, Z2-R-VFX-028, Z2-R-VFX-029, Z2-R-VFX-031, Z2-R-VFX-032, Z2-R-VFX-033, Z2-R-VFX-034, Z2-R-VFX-035, Z2-R-VFX-036, Z2-R-VFX-046, Z2-R-VFX-047, Z2-R-VFX-048, Z2-R-VFX-049, Z2-R-VFX-054, Z2-R-VFX-057, Z2-R-VFX-060, Z2-R-VFX-115, Z2-R-VFX-116, Z2-R-VFX-117, Z2-R-VFX-118, Z2-R-VFX-119, Z2-R-VFX-120, Z2-R-VFX-121, Z2-R-VFX-122, Z2-R-VFX-123, Z2-R-VFX-125, Z2-R-VFX-126, Z2-R-VFX-127, Z2-R-VFX-128, Z2-R-VFX-140, Z2-R-VFX-144 |
| Z2-I-VFX-02 | GPU particles | Z2-R-VFX-001, Z2-R-VFX-002, Z2-R-VFX-003, Z2-R-VFX-004, Z2-R-VFX-007, Z2-R-VFX-008, Z2-R-VFX-009, Z2-R-VFX-010, Z2-R-VFX-023, Z2-R-VFX-024, Z2-R-VFX-025, Z2-R-VFX-026, Z2-R-VFX-027, Z2-R-VFX-028, Z2-R-VFX-029, Z2-R-VFX-030, Z2-R-VFX-031, Z2-R-VFX-037, Z2-R-VFX-038, Z2-R-VFX-039, Z2-R-VFX-040, Z2-R-VFX-041, Z2-R-VFX-042, Z2-R-VFX-043, Z2-R-VFX-044, Z2-R-VFX-045, Z2-R-VFX-046, Z2-R-VFX-047, Z2-R-VFX-048, Z2-R-VFX-049, Z2-R-VFX-050, Z2-R-VFX-051, Z2-R-VFX-052, Z2-R-VFX-053, Z2-R-VFX-054, Z2-R-VFX-055, Z2-R-VFX-056, Z2-R-VFX-057, Z2-R-VFX-093, Z2-R-VFX-099, Z2-R-VFX-100, Z2-R-VFX-103, Z2-R-VFX-104, Z2-R-VFX-105, Z2-R-VFX-106, Z2-R-VFX-107, Z2-R-VFX-108, Z2-R-VFX-109, Z2-R-VFX-110, Z2-R-VFX-111, Z2-R-VFX-112, Z2-R-VFX-113, Z2-R-VFX-114, Z2-R-VFX-115, Z2-R-VFX-116, Z2-R-VFX-117, Z2-R-VFX-118, Z2-R-VFX-119, Z2-R-VFX-120, Z2-R-VFX-121, Z2-R-VFX-122, Z2-R-VFX-123, Z2-R-VFX-124, Z2-R-VFX-125, Z2-R-VFX-126, Z2-R-VFX-127, Z2-R-VFX-128, Z2-R-VFX-140, Z2-R-VFX-144, Z2-R-VFX-145, Z2-R-VFX-146, Z2-R-VFX-153, Z2-R-VFX-154, Z2-R-VFX-155 |
| Z2-I-VFX-03 | ribbons | Z2-R-VFX-026, Z2-R-VFX-058, Z2-R-VFX-059, Z2-R-VFX-060, Z2-R-VFX-096, Z2-R-VFX-109, Z2-R-VFX-110, Z2-R-VFX-111, Z2-R-VFX-128 |
| Z2-I-VFX-04 | trails | Z2-R-VFX-061, Z2-R-VFX-062, Z2-R-VFX-063, Z2-R-VFX-096, Z2-R-VFX-109, Z2-R-VFX-110, Z2-R-VFX-111, Z2-R-VFX-128 |
| Z2-I-VFX-05 | particle collision | Z2-R-VFX-006, Z2-R-VFX-064, Z2-R-VFX-065, Z2-R-VFX-066, Z2-R-VFX-067, Z2-R-VFX-068, Z2-R-VFX-069, Z2-R-VFX-152, Z2-R-VFX-156 |
| Z2-I-VFX-06 | particle bounds | Z2-R-VFX-070, Z2-R-VFX-071, Z2-R-VFX-072, Z2-R-VFX-073, Z2-R-VFX-074, Z2-R-VFX-157, Z2-R-VFX-158, Z2-R-VFX-159 |
| Z2-I-VFX-07 | particle culling | Z2-R-VFX-023, Z2-R-VFX-072, Z2-R-VFX-075, Z2-R-VFX-076, Z2-R-VFX-077, Z2-R-VFX-078, Z2-R-VFX-079, Z2-R-VFX-087, Z2-R-VFX-157, Z2-R-VFX-158 |
| Z2-I-VFX-08 | particle sorting | Z2-R-VFX-080, Z2-R-VFX-081, Z2-R-VFX-082, Z2-R-VFX-083, Z2-R-VFX-084, Z2-R-VFX-085, Z2-R-VFX-151 |
| Z2-I-VFX-09 | VFX LOD | Z2-R-VFX-006, Z2-R-VFX-076, Z2-R-VFX-086, Z2-R-VFX-087, Z2-R-VFX-088, Z2-R-VFX-089 |
| Z2-I-VFX-10 | VFX event integration | Z2-R-VFX-009, Z2-R-VFX-011, Z2-R-VFX-012, Z2-R-VFX-013, Z2-R-VFX-014, Z2-R-VFX-015, Z2-R-VFX-016, Z2-R-VFX-017, Z2-R-VFX-018, Z2-R-VFX-019, Z2-R-VFX-020, Z2-R-VFX-021, Z2-R-VFX-022, Z2-R-VFX-063, Z2-R-VFX-067, Z2-R-VFX-090, Z2-R-VFX-091, Z2-R-VFX-092, Z2-R-VFX-093, Z2-R-VFX-101, Z2-R-VFX-136, Z2-R-VFX-137, Z2-R-VFX-138, Z2-R-VFX-139, Z2-R-VFX-143, Z2-R-VFX-147, Z2-R-VFX-149, Z2-R-VFX-150, Z2-R-VFX-156, Z2-R-VFX-157 |
| Z2-I-VFX-11 | VFX budgets | Z2-R-VFX-018, Z2-R-VFX-019, Z2-R-VFX-022, Z2-R-VFX-034, Z2-R-VFX-035, Z2-R-VFX-036, Z2-R-VFX-041, Z2-R-VFX-056, Z2-R-VFX-057, Z2-R-VFX-078, Z2-R-VFX-085, Z2-R-VFX-089, Z2-R-VFX-094, Z2-R-VFX-095, Z2-R-VFX-096, Z2-R-VFX-097, Z2-R-VFX-098, Z2-R-VFX-099, Z2-R-VFX-100, Z2-R-VFX-101, Z2-R-VFX-102, Z2-R-VFX-118, Z2-R-VFX-139, Z2-R-VFX-141, Z2-R-VFX-142 |
| Z2-I-VFX-12 | previews for code-authored effects | Z2-R-VFX-006, Z2-R-VFX-055, Z2-R-VFX-071, Z2-R-VFX-077, Z2-R-VFX-102, Z2-R-VFX-115, Z2-R-VFX-122, Z2-R-VFX-124, Z2-R-VFX-125, Z2-R-VFX-128, Z2-R-VFX-129, Z2-R-VFX-130, Z2-R-VFX-131, Z2-R-VFX-132, Z2-R-VFX-133, Z2-R-VFX-138, Z2-R-VFX-142, Z2-R-VFX-148, Z2-R-VFX-154, Z2-R-VFX-155 |
| Z2-I-VFX-13 | inspectors for code-authored effects | Z2-R-VFX-132, Z2-R-VFX-134, Z2-R-VFX-135 |

## Excluded and deferred

No VFX inventory item is excluded or deferred: all thirteen are required at Stage 5 and each is covered above. Three boundaries are recorded so that their absence here is not read as a gap:

- Destruction debris that collides with and affects the world is a physics entity, not a particle; whether destruction is in scope is D-CLASS-SIM (open, Stage 2), and VFX supplies only presentation debris (mesh particles with depth collision).
- The editor panels that host the preview and the inspector are EDIT's; VFX's requirements cover the data, the preview world behaviour and the statistics those panels show (Z2-D-079).
- Tile-local merging of particle passes on the S24 waits on D-TBDR (open, Stage 5) and Z2-D-036; no VFX requirement depends on it.
