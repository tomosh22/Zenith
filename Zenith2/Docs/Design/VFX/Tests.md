# VFX design: test strategy

Section file of `Zenith2/Docs/Design/VFX.md`. It names every VFX test target, the unit tests of the simulation side and of the render side, the CPU reference parity tests and their tolerances, the GPU simulation and render tests on WARP and lavapipe, the determinism tests, the stress suite, the failure and fault-injection tests, the cook and format tests, the fixtures and the gate steps that run them. Every test is pass/fail, carries no pinned count, prints the seed of any randomness it uses and lives under a `Tests/` directory ([Z2-N-CONTRACT-409]).

## Scope of this file

| Topic | Heading |
|---|---|
| where tests live and what runs them | "Test targets" |
| CPU-only tests of each type | "Unit tests" |
| the CPU simulator against the GPU simulator | "CPU reference parity tests" |
| GPU dispatch tests | "GPU simulation tests" |
| image and graph tests | "Render tests" |
| reproducibility | "Determinism tests" |
| capacity and churn | "Stress tests" |
| every failure path | "Failure and fault-injection tests" |
| the cook and the reader | "Cook tests" |
| test data | "Fixtures" |
| which gate step runs what | "Gate steps" |

## Test targets

| Target | Path | Contents |
|---|---|---|
| `ZenithVFX_Tests` | `Zenith2/Engine/ZenithVFX/Tests/` | the module, components, lifecycle system, trigger queue, scheduling, admission, the CPU simulator, the reference simulation, the extractor, bindings, save state; CPU only, against the real ECS backends and the real `Flux_RenderWorldWriter` |
| `ZenithFlux_Tests` (particles) | `Zenith2/Engine/ZenithFlux/Tests/Particles/` | the effect reader and loader, the particle system, frame building, every particle pass, sorting, draw records, shading; CPU tests plus GPU tests on WARP (D3D12) and lavapipe (Vulkan) ([Z2-N-REND-034]) |
| parity scenes | `Zenith2/Engine/ZenithFlux/Tests/Parity/Scenes/Particles*.scene.json` ([Z2-N-REND-942]) | the scenes of "Render tests", compared across backends and paths within one run |
| integration suite `Zenith_Integration_VFX` | `Zenith2/Tests/VFX/` | whole-runtime runs: a composed runtime with the VFX module, Luau scripts triggering effects, save and load, the slices' VFX scenes headless (no renderer) and on WARP and lavapipe |
| cook tests | `Zenith2/Tools/ZenithCook/Tests/VFX/` | the `vfx.effect` and `vfx.particleTypes` steps, document validation, curve baking, capacity sizing, bounds baking, migration fixtures |
| fuzz | `Zenith2/Engine/ZenithFlux/Tests/Fuzz/` target `Zenith_Fuzz_ParticleEffect`; `Zenith2/Tools/ZenithCook/Tests/Fuzz/` target `Zenith_Fuzz_VFXDocument` | the cooked-file reader; the document validator |
| benchmarks | `Zenith_Bench_VFXBudget` under `Zenith2/Benchmarks/Budgets/VFX/` | the four rows `Z2-B-VFX-001` … `Z2-B-VFX-004` ("Budget rows" of `Zenith2/Docs/Design/VFX.md`) |

Every GPU test is an ordinary `Test` gate test: WARP and lavapipe are software devices present on every gate machine, so no VFX test carries `needs-gpu`; only the budget measurements on the reference hardware and the S24 are device runs ("Gate steps").

Tests never substitute a double for a concrete class: `Zenith_ParticleEffectAsset` is built by `Flux_ParseParticleEffect` over cooked bytes, the particle system by `EnableParticles` on a real renderer over a WARP or lavapipe device (or none, for CPU-side tests of the render world), worlds by the runtime's real world creation, and render worlds by the real writer. Cooked effect bytes for ZenithFlux tests come from `Flux_TestEffectBuilder` (`Zenith2/Engine/ZenithFlux/Tests/Support/Flux_TestEffectBuilder.h`, header-only, `inline` functions: non-Core `Tests/Support/` compiles into no target, [Z2-N-ARCH-255] and the X184 ruling), which writes valid `.zvfx` files through ASSET's real `Zenith_CookedFileWriter` (ZenithAssets) from a C++ description; it is a builder of real data, not a double, so it builds its inputs in code in every preset (`win-asan` included), and `Test_TestEffectBuilder_MatchesCook` (in the cook tests, which exist where ZenithCook builds) holds it to the cook's output for every fixture document. Cook-step tests obtain their `Zenith_CookContext` from `MakeCookContext` of ASSET's `Zenith2/Tools/ZenithCook/Internal/Cook/Zenith_CookTestContexts.h` (the GEOM-N2 ruling); the kind-loader tests drive the real `Zenith_AssetRegistry` through ASSET's header-only `Zenith2/Engine/ZenithAssets/Tests/Support/Zenith_AssetKindLoaderHarness.h` (the ASSET-F25 ruling); `Flux_Renderer`, `Flux_LightingSystem` and `Flux_GPUScene` are always the real concrete classes on the headless Null, WARP or lavapipe path (the FINAL-DOUBLES ruling).

## Unit tests

| Test | Asserts |
|---|---|
| `Test_ParticleRandom_KnownValues` | `Flux_ParticleHash` and `Flux_ParticleRandomU32` of 16 fixed inputs, evaluated at compile time (`static_assert`) by an independent straight-line transcription of the formulas of "Particle random functions" in the test, equal the library's results; the Slang twin returns the same 16 values on WARP and lavapipe. No digit is committed, so nothing needs recording |
| `Test_ParticleRandom_UnitRange` | `Flux_ParticleRandomUnit` over 2²⁰ inputs lies in [0, 1), never reaches 1, and its histogram over 64 bins is within 1% of uniform |
| `Test_ParticleInstanceKey_AttachedDetachedDisjoint` | attached keys have bit 63 clear and detached keys set; a restart (new generation) changes the key; equal inputs give equal keys; two simulation IDs give different attached keys and different `Flux_MakeAttachedParticleSeed` values; the seed is unchanged when only the world tag changes |
| `Test_VFXInstanceKey_RuntimeSpawnedEntitiesDistinct` | two entities spawned at run time (null persistent IDs) from one prefab carrying an effect, in one world on WARP: the render side holds two instances (`GetInstanceStats` finds both keys), the keys and the default seeds differ, and both draw; after a save and a load into a world with a different tag, the restored simulation IDs give the same default seeds and two distinct instances again |
| `Test_VFXEffectComponent_Registration` | the five component types register with their stable names, property IDs, names, defaults and serialization flags; `Zenith.VFX.PreviewControl` is not serialized; every registered component, the resource `Zenith.VFX.TriggerQueue` and the two events carry exactly the owning phase, writers and property IDs of their rows in "Authority census rows" (`Zenith2/Docs/Design/VFX/PublicAPI.md`), the registration-shape check GAME's census table assigns to a module no Lifecycle suite links |
| `Test_VFXLifecycle_StateMachine` | every row of "Lifecycle state machine" (`Zenith2/Docs/Design/VFX/Simulation.md`) on the three ECS backends: the state, the recorded commands and the emitted events, in order, for each event of the row; an invalid value in the same tick as a play request starts nothing (row 8 is evaluated first) |
| `Test_VFXLifecycle_DestroyAfterTicks` | with the expectation computed in the test by the rule of row 7 (its `ulp` from the `float`'s exponent bits), and checked against these decimal-exact counts the rule must reproduce: `DestroyAfterSeconds` 0.5 destroys the entity exactly 15, 30 and 60 ticks after start at 30, 60 and 120 Hz; 0.1 at 30 Hz after exactly 3 ticks; 0.51 at 30 Hz after 16; 86,400 at 120 Hz after exactly 10,368,000 |
| `Test_VFXLifecycle_InvalidComponentIgnored` | a NaN parameter or a time scale of 9 starts and stops nothing and counts once |
| `Test_VFXTriggerQueue_DropNewest` | C++ `VFX.Spawn` records beyond the C++ share (`capacity − reserve`) in one frame: the newest are dropped, `vfx.triggers.dropped` counts each, the kept sequences are contiguous |
| `Test_VFXTriggerQueue_ScriptReserve` | a frame whose C++ records fill their share and whose script merges accept `Z2-B-SCRIPT-024` triggers in each of `sim.maxCatchUpTicks` ticks keeps every script trigger; Start with a reserve above `capacity − 64` takes both defaults and logs one warning |
| `Test_VFXSave_TriggerQueueNotSaved` | a world saved in a tick that recorded triggers loads with an empty `Zenith.VFX.TriggerQueue`, and the saved bytes carry no property of the resource |
| `Test_VFXDetached_StartTimeFromRecordingTick` | a record of tick `n_rec` with lead 0 and with lead `t_tick / 2`, drained at a frame of presentation time `T_k`, starts at effect time `max(0, T_k − n_rec × t_tick + lead)`, the same on two runs |
| `Test_VFXTriggerQueue_SequenceOrder` | triggers recorded by two systems and a script in one tick drain in (phase, merge) order with increasing sequences, on 1 and 8 workers |
| `Test_VFXSpawnPayload_Validation` | each payload check of `Zenith_VFXSpawnPayload` returns `VFX_ERROR_INVALID_ARGUMENT` at record time and the command is not recorded |
| `Test_VFXScript_SpawnBindings` | `VFX.Spawn` and `VFX.SpawnAttached` from Luau append trigger records with the given payloads; each misuse code (`ZS5601`, `ZS5602`) is raised for its misuse and never asserts; `VFX.ParameterIndex` called by a behaviour every tick returns `nil` before the tick at which the world publishes the effect's load as loaded and the declared slot from that tick on, identically on two runs whose loads complete at different wall-clock times (the second run's load delayed), and an unknown name returns `nil` |
| `Test_VFXSchedule_RateSpawnCounts` | a rate of 37.5/s over 600 frames of varying Δ spawns, at every frame, exactly the count a reference loop in the test computes by rule 1 of "Rate spawning" in `double` over the same Δ sequence, and within one particle of `floor(37.5 × Σ Δ)` |
| `Test_VFXSchedule_BurstCycles` | bursts with `cycles` and `interval` fire at their times in every timeline cycle; a looping timeline wraps without losing or repeating a burst |
| `Test_VFXSchedule_DistanceSpawning` | an emitter moved 10 m with spacing 0.25 spawns 40 particles at the interpolated positions; a teleport spawns none |
| `Test_VFXSchedule_PrewarmAges` | a prewarm of 1 s yields `min(floor(r_0 × 1), C)` particles with ages evenly spaced in [0, 1) |
| `Test_VFXSchedule_CatchUpOnlyInPreview` | a `Zenith.VFX.PreviewControl` seek in a world of purpose `WORLD_PURPOSE_PREVIEW` produces catch-up commands; the same component in a `WORLD_PURPOSE_PLAY` and a `WORLD_PURPOSE_EDIT` world produces none and counts `vfx.catchup.refused` once per world |
| `Test_VFXSchedule_OrdinalResetBeforeWrap` | an emitter started at ordinal `2³² − 2²⁴ − 100` at 10,000 particles per second resets (new generation, ordinal 0) in the frame whose spawns would pass `2³² − 2²⁴`, and never wraps; an event source with a collision edge of `maxCollisions` 3 and 16 children (`E_edge × k` = 48) started at ordinal 89,128,860 resets with its target in the frame whose spawns would make `o' × 48` pass `2³² − 2²⁴` (both take a new generation in that frame), and no depth-1 child ordinal ever exceeds `2³² − 2²⁴ − 1`; a three-emitter chain (a rate root, a death edge of 16 children into a middle target, a death edge of 16 children from it into a depth-2 target) with the root started at ordinal `2²⁴ − 100`, so that the depth-2 child ordinals pass `2³²` within its first frames: the depth-2 target's capacity is a power of two, its children take slots `o & (C − 1)`, and the reference simulation run across the wrap reports zero `Overwrites` with no reset of the graph before the root's own limit |
| `Test_VFXLOD_ScreenSizeAndHysteresis` | the LOD level for screen sizes swept up and down across each threshold changes at `threshold × (1 ± hysteresis)`, and `cullScreenSize` and `maxDistance` cull |
| `Test_VFXLOD_ViewerSet` | the main camera plus up to eight enabled viewers; the ninth is ignored and counted; significance uses the nearest viewer |
| `Test_VFXAdmission_CpuPerWorld` | with more CPU emitters than `vfx.cpuEmittersPerWorld`, the admitted set is the highest significance, ties by key, and identical on every run |
| `Test_VFXAdmission_GpuPerFrame` | the GPU admission across two worlds respects `vfx.gpuActiveCapacity` and `vfx.gpuEmittersPerFrame`, ordered by significance then key; an event graph is admitted or refused as one unit and its work includes `C_src × E_edge × k` per target (the 266,240-unit example of "GPU admission per frame" is refused under 262,144); a unit carrying catch-up steps is ranked ahead of every other unit, its work is the sum over its steps, and when refused its steps are kept pending rather than culled |
| `Test_VFXAdmission_SortedBudget` | depth-sorted emitters beyond `vfx.sortedParticlesPerFrame` or `vfx.sortedEmittersPerFrame` fall back to their unsorted order and are counted |
| `Test_VFXBounds_AttachedAndWorldSpace` | runtime bounds composed by "Bounds" (`Zenith2/Docs/Design/VFX/Simulation.md`) cover every particle position the reference simulation (`Zenith_VFXRunReference` with the same moving, rotating track, no colliders) produces over its linger, every gravity fixture run at fixed steps of 1/15 s as well as 1/60 s, for: a point-spawned zero-velocity emitter under gravity (its end-of-life fall is the semi-implicit `½ g L (L + dt)`), a world-space emitter with gravity tilted 90° and rotating at 1 rad/s, a local-space emitter with gravity on the same track, a distance-spawned ribbon moving at 15 m/s, an `inheritVelocity` 2 emitter moving at 10 m/s, an attached detached effect following its target, `Debug_Sparks`' smoke target (children spawned by falling, emitter-velocity-inheriting sparks on a track moving at 10 m/s, composed from the sparks emitter as chain root), and a `wind` emitter under a uniform 40 m/s wind (`m_xWind`), composed with the wind cube `Flux_ParticleWindExpansion(m_fWindGain, 40)` |
| `Test_VFXExtract_CommandStream` | a fixed scenario of 40 frames (start, stop, restart, cull, teleport, destroy, detach) writes exactly the command sequence of "Emitter instance state machine" (`Zenith2/Docs/Design/VFX/Simulation.md`) for each emitter |
| `Test_VFXExtract_EffectLightsAndDecals` | a light module produces `ADD`, `UPDATE`… `REMOVE` deltas with the named key, the fade, and no shadow flag; a decal module writes `Flux_RenderDecal` records with the stable key of "Effect decals" (`Zenith2/Docs/Design/VFX/Rendering.md`) and its fade; the per-world caps drop by significance and age |
| `Test_VFXSave_StateRoundTrip` | a world with attached effects in every lifecycle state saves and loads to the same `Zenith.VFX.EffectState` values; the presentation after load equals a fresh start of the same generations |
| `Test_VFXModule_HeadlessRunsLogic` | a runtime without a renderer starts the module, runs lifecycles, triggers and CPU emitters, and creates no particle system |
| `Test_ParticleSystem_CommandValidation` | each rejection rule of `Flux_RenderEmitterCommand`'s block drops the command, counts it, and leaves the emitter's previous state |
| `Test_ParticleSystem_PoolRangeReuse` | emitters created and destroyed in a fixed order reuse pool ranges through `FreePoolRange` and never overlap a live range |
| `Test_ParticleSystem_TierChangeAppliesBudgets` | as "Flux_ParticleSystem" of `Zenith2/Docs/Design/VFX/PublicAPI.md` states: a tier change applies the render-side budgets and the lighting clamp from the next built frame and changes no restart member |
| `Test_ParticleSystem_GenerationWrapClearsRange` | 256 resets of one emitter: at the wrap from 255 to 0 its state, trail and event ranges are cleared, and no particle or event record of the old generation 0 reads as live or spawns a child |
| `Test_ParticleSystem_InstanceTimeout` | an instance without a command for two consecutive submitted frames is destroyed and counted; a deferred frame between them does not count |
| `Test_ParticleSystem_PressureShedsAndReverts` | at WARNING the admitted GPU capacity, the sorted particles and the CPU capacity scale by 0.75 and the pool stops growing; at CRITICAL by 0.5, the ranges of emitters culled for more than 120 frames are freed and the detached cap halves; every value reverts one frame after the level falls |
| `Test_VFXClock_DeltaClampAndPause` | the presentation clock of "Presentation clock" (`Zenith2/Docs/Design/VFX/Simulation.md`): Δ follows `(n + α) × t_tick` × time scale, a 0.5 s hitch advances effect time by 1/15 s, and `Paused`, a paused world and a frozen preview give Δ = 0 |
| `Test_VFXDetached_AttachFollowsTarget` | a detached effect spawned with `VFX_SPAWN_FLAG_ATTACH` follows its target's interpolated position (and rotation and scale with their flags) with the payload offset; a destroyed target freezes the instance at its last transform and releases it |
| `Test_VFXDetached_CapAndTruncation` | records beyond `vfx.detachedEffectsPerWorld` are discarded in sequence order and counted; a looping or endless effect played detached plays one cycle, or `fVFX_DETACHED_ENDLESS_SECONDS`, and counts `vfx.detached.truncated` |
| `Test_VFXMemory_ConsumerReportsCpuStorage` | the module's memory consumer reports the CPU particle storage and the trigger queues under `MEMORY_SUBSYSTEM_RENDERING`, equal to the bytes the module allocated |
| `Test_VFXColliders_GatherCapAndInvalid` | 70 enabled colliders: the first 64 by simulation ID are gathered and sent as `Flux_RenderParticleCollider` records, 6 counted dropped; a collider with a negative radius is disabled and counted in `vfx.colliders.invalid` |
| `Test_VFXCulling_KillAndPause` | a `kill` emitter culled and readmitted restarts empty (`RESET`); a `pause` emitter keeps its particles frozen and resumes them |
| `Test_VFXLOD_LevelDisablesModules` | at LOD level 1 of a fixture effect the listed update modules are skipped and spawns scale by the level's `spawnScale` × `vfx.spawnScale` |
| `Test_VFXConfig_KeysRegistered` | every key of "Configuration keys" (`Zenith2/Docs/Design/VFX.md`) is registered with its type, domain and default; an out-of-domain value takes the default with one `vfx` warning |
| `Test_VFXConfig_TierDefaults` | the S24 default tier resolves `vfx.lodBias` 0.5, `vfx.spawnScale` 0.75, `vfx.maxLightingMode` `vertex` and the S24 budgets; the desktop tiers resolve the desktop values |
| `Test_VFXEvents_StableNames` | `Zenith.VFX.EffectStarted` and `Zenith.VFX.EffectStopped` register under their stable names with their fields, so another module subscribes by name alone |
| `Test_VFXDiagnostics_CountersPublished` | every counter of "Counters" (`Zenith2/Docs/Design/VFX.md`) is registered once and moves when its condition is forced |
| `Test_VFXReload_EffectSwapRestartsInstances` | a reload swap of an effect asset destroys its detached instances and restarts its attached instances under new keys; the render side creates the new constants buffer and releases the old one after the retire frames; a failed reload keeps the old effect playing |
| `Test_VFXEditWorld_PlayControls` | in an edit world, incrementing `PlayRequest` and `StopRequest` by command plays and stops an effect exactly as in a play world |
| `Test_ParticleEffect_ParameterTable` | a parsed effect exposes its declared parameters' names, first slots, slot counts and defaults in declaration order; `FindParameter` of an undeclared name returns `FLUX_PARTICLE_ERROR_LIMIT` |
| `Test_ParticleEffectLoader_CpuResidentHeadless` | the effect kind loader activates an effect in a headless registry and with a renderer alike, reports `MEMORY_SUBSYSTEM_RENDERING` resident bytes equal to `GetResidentBytes()`, creates no GPU resource, and `DropCpuCopies` returns 0 |
| `Test_ParticleSystem_InstanceStats` | `GetStats` and `GetInstanceStats` return copies of the snapshot of frame `k − 2` during frame k (the three-slot ring); for a live key they report capacities, spawns, LOD levels, simulated flags and the live upper bound matching the frame's commands; an unknown key returns false |
| `Test_VFXModule_ShutdownOrder` | slice 7's module shutdown against slice 2a's empty particle system on WARP, as "Module shutdown order" of `Zenith2/Docs/Design/VFX/PublicAPI.md` states, read from the `Zenith_VFXShutdownTrace` the test installs through `Zenith_VFXModuleTestAccess.h`: the panel-provider and configuration tokens are destroyed first, `DisableParticles` is called exactly once and leaves `GetParticleSystem()` null, then `UnregisterKindLoader` runs, then the loader is destroyed |
| `Test_ParticleSystem_EnableDisableLifecycle` | slice 2a's bodies: `EnableParticles` with an invalid desc returns `FLUX_PARTICLE_ERROR_INVALID_DESC` and leaves `GetParticleSystem()` null; a valid call returns the system, and a second call `FLUX_PARTICLE_ERROR_ALREADY_ENABLED`; `DisableParticles` makes `GetParticleSystem()` null and a second `DisableParticles` does nothing; the `Allocate` fault point `0x56461001` unwinds to a null system; `GetStats` of the empty system is all zero |
| `Test_VFXModule_ShutdownReleasesRenderSideFirst` | as "Module shutdown order" of `Zenith2/Docs/Design/VFX/PublicAPI.md` states, with a renderer on WARP and effects playing in two worlds at shutdown, the order read from the installed `Zenith_VFXShutdownTrace` |
| `Test_VFXPanelProvider_BuildsRows` | the provider registered at Start reports panel ID `vfxsystem`, and its `Build` writes one row per statistic of "On-screen profiler rows" (`Zenith2/Docs/Design/VFX.md`) with the values of `GetStats`; every BAR row of `axPANEL_ROWS_VFX` carries the `m_szLimit` that table gives (the GPU-work and memory bars a counter expression, the millisecond bars empty until the budget ticket sets them); without a particle system it writes one Text row saying so; the `vfx` tab's catalogue rows compile (DEVT's `Test_ProfilerPanels_CatalogueCompiles` covers the file) |
| `Test_VFXExtract_SlotArenaDemand` | a world at the per-world caps writes at most the render-world bytes "CPU particle output" (`Zenith2/Docs/Design/VFX/Simulation.md`) states, and the writer refuses nothing at the default `Z2-B-CORE-005` proposal |
| `Test_ParticleSystem_FullStateResync` | a full-state section that omits a live instance of its world destroys it; instances it mentions keep their GPU particles |
| `Test_ParticleSortView_Selection` | the sort view is the main view, else the lowest-ID editor view, else the first texture, capture, then preview view; a frame with no particle-drawing view sorts nothing |

## CPU reference parity tests

The C++ simulator and the Slang simulator implement the same steps in the same order (`Zenith2/Docs/Design/VFX/Simulation.md → "Determinism rules"`). Parity tests run one fixture effect through `Zenith_VFXRunReference` (with `m_bKeepFinalStates`) and through the real particle system on WARP and lavapipe with the same instance seed, the same fixed step 1/60 s and the same number of steps (120 unless stated), then read the GPU pool back and compare slot by slot.

| Quantity | Tolerance |
|---|---|
| spawn ordinal, generation, slot assignment, live set, collision count, event raises | exact |
| lifetime, spawn-time random draws, sub-UV start frame | bit-exact (integer hash and one exact multiply) |
| age | within 10⁻⁶ s |
| position | `|Δp| ≤ 10⁻³ m + 10⁻⁴ × |p|` |
| velocity | `|Δv| ≤ 10⁻³ m/s + 10⁻⁴ × |v|` |
| size, rotation | relative 10⁻⁴; rotation absolute 10⁻⁴ rad |
| colour (evaluated at draw time from the same curves) | 1/1024 per channel |
| trail samples | as position |

A particle whose CPU and GPU positions lie on opposite sides of a collider surface, a kill volume face or a noise lattice plane at some step diverges legitimately; the test reports each such particle with its step and excludes it when such particles are at most 0.1% of the live set, and fails otherwise. Each fixture places its geometry so that the expected count is zero on both devices; the allowance absorbs fused multiply-add differences only.

| Test | Fixture |
|---|---|
| `Test_ParticleParity_Integration` | every update module except `collision` and `trail`, one emitter each, 4,096 particles |
| `Test_ParticleParity_Collision` | analytic colliders of every shape with restitution, friction and `killAfter` |
| `Test_ParticleParity_Events` | death and collision events chaining three emitters |
| `Test_ParticleParity_EventsVariedLifetimes` | sources with random lifetimes in [0.2, 2] s and a death edge of 4 children, auto-sized, one without a killing module, one with a `killVolume`, one with `collision` `kill: true` against a plane collider, and a three-emitter chain whose depth-2 target has a power-of-two capacity: zero `Overwrites` on both simulators over 600 steps, and the children agree within the table above |
| `Test_ParticleParity_Trails` | trail sampling at intervals of 1/240 to 1/10 s |
| `Test_ParticleParity_PrewarmAndBursts` | prewarm 2 s plus bursts larger than the capacity (ring overwrite) |
| `Test_ParticleParity_LocalSpace` | a local-space emitter on a moving, rotating transform |
| `Test_ParticleParity_CpuTargetMatchesGpu` | the same document with `target` `cpu` and `gpu` (modules both support): the CPU emitter's output and the GPU pool agree within the table above |

## GPU simulation tests

On WARP and lavapipe, sync validation clean, aliasing on and off with poisoning ([Z2-N-REND-433]):

| Test | Asserts |
|---|---|
| `Test_ParticlePasses_DeclareTableFields` | graph inspection: the five passes declare exactly the fields, access, stages, queues and permutations of "Pass tables" (`Zenith2/Docs/Design/VFX/Rendering.md`), the four simulation passes carry the async-compute flag, and a frame with no admitted emitter declares no particle pass |
| `Test_ParticleEmit_RingSlots` | spawns of ordinals `o … o + n` land in slots `o mod C …` with the right generation; a burst of `C + 10` leaves the newest `C` |
| `Test_ParticleSimulate_DeadSlotsUntouched` | a slot with zero lifetime is never written by the simulate pass (poisoned slot contents survive) |
| `Test_ParticleSort_StableAndCorrect` | 2²⁰ random keys, and keys with long runs of equal values: the output is sorted, stable, and equal to `std::stable_sort` of the same pairs |
| `Test_ParticleSort_SegmentsPerEmitter` | three depth-sorted emitters' particles stay in their segments, each with its dead keys first and then its live particles back to front |
| `Test_ParticleSort_ReduceThenScanMatchesOnesweep` | for the inputs of `Test_ParticleSort_StableAndCorrect` and `Test_ParticleSort_PartitionBoundaries`, `vfx.sort.algorithm` `reduceThenScan` writes the same pairs as `onesweep`, byte for byte |
| `Test_ParticleSort_PartitionBoundaries` | key counts of 1, 2,047, 2,048, 2,049 and 131,072 sort correctly (partition edge cases) |
| `Test_ParticleDepthCollision_PreviousHiZ` | particles falling onto a depth-only plane of the previous frame's HiZ bounce at the plane within one HiZ texel's depth; a first frame without history collides with nothing; an emitter of another world than the sort view's runs without depth collision and is counted |
| `Test_ParticleCatchUp_StepsInOneFrame` | for `Debug_Sparks` (a death event) and a collision-event fixture: a seek frame of 8 commands (7 flagged `CATCH_UP`, the last unflagged) executes 8 emit-and-simulate steps in arrival order in that frame (graph inspection: `ParticleEmit`, `ParticleSimulate`, `ParticleEmitStep1` … `ParticleSimulateStep7`, in that order, then the sort passes), the live state after the frame equals 8 frames of playback that each advance the preview world's clock by exactly `GetTickDurationUs()` (one tick, alpha 0): the same slots live under each run's current generation, every live slot equal field by field with `m_uFlags` bits 0–7 masked, and the same children spawned at each step, event children included; the drawn state is the state after the last step (a render of the frame equals a render of the eighth playback frame) |
| `Test_ParticleBounds_WindExpansion` | a GPU `wind` emitter (factor 1, lifetime 2 s, so `G` = 2) in a world whose render environment carries speed 40 m/s, gust strength 0.5 and turbulence 0.2 (`Flux_ParticleWindBound` = 40 × (1 + 0.5 + 0.1) = 64 m/s): the instance's culling bounds equal the command's `m_xWorldBounds` grown by 128 m on every side, and with `vfx.debug.validateBounds` no frame of 600 sets `m_abBoundsExceeded`; after the wind drops to 0 the growth stays for between `H` = 2 s and `H + H / 8` = 2.25 s of effect time (the render side's nine-bucket ring of `H / 8`) and then falls to 0; a world whose render environment has `m_bValid` false grows nothing |
| `Test_ParticleBounds_ValidationDetectsEscape` | an effect with deliberately undersized `fixed` bounds sets `m_abBoundsExceeded` and reports the measured bounds; correctly sized bounds never set it; the measured live count equals the reference simulation's live count of the frame `m_ulMeasuredFrameIndex` names, which lies one to F frames behind the frame that reads it |
| `Test_ParticleEvents_RingCapacity` | a source with a death edge and a collision edge of `maxCollisions` 3 gets a ring of exactly `C × 4` records; with every particle colliding three times and dying in one step, every raised event spawns its children in the next step and none is lost; a fresh range spawns no child from its cleared records |
| `Test_ParticleEvents_DeferredFrameKeepsChildren` | a frame deferred between a source's death step and its target's next step leaves the step counters unchanged, and the next submitted frame spawns exactly the children an undeferred run spawns |
| `Test_ParticleCatchUp_DeferredSeekKeepsSteps` | a seek of `Debug_Sparks` whose second catch-up frame is deferred: the next submitted frame executes the 8 kept steps, then its own 8, in order (16 steps, graph inspection), and the live state after the seek equals 180 frames of one-tick playback (live set and live slots field by field, `m_uFlags` bits 0–7 masked); with `vfx.preview.catchUpStepsPerFrame` 64 and two consecutive deferred seek frames the kept steps would exceed 64, so `Flux.Particles.SeekInterrupted` counts one, `m_bSeekInterrupted` reads true and the instance draws nothing until the next seek's `RESET` command, after which the re-issued seek again equals playback; with `vfx.preview.catchUpStepsPerFrame` 1 every seek frame's single command carries `FLUX_EMITTER_FLAG_SEEK`, a deferred seek frame's step is kept as its own step rather than merged (its `dt` is not added to the next frame's), and the seek still equals one-tick playback |
| `Test_ParticleEvents_ConsumerMatchesEdgeKind` | a source with a death edge to target A and a collision edge of `maxCollisions` 2 to target B (E = 3): its records carry compact indices 0 and 1 for collisions and 2 for death; A spawns children only from death records and B only from collision records, each exactly the count the reference simulation spawns; a death-only source (E = 1) writes index 0 and touches no record beyond `C × 1`; A's event batch has `C × 1 × k` threads and B's `C × 2 × k` |
| `Test_ParticleEffect_ConstantsImageMatchesGpu` | the effect constants buffer the particle system creates on first use is byte-identical to `GetConstantsImage()` |
| `Test_ParticleUpdateType_CustomTypeRuns` | an emitter naming the test type `TestUpdate_Spiral` runs the type's update in the simulate pass (positions follow the type's analytic spiral within 10⁻³ m) and its `Params` reach the type at the reflected offsets |
| `Test_ParticlePrecipitation_OcclusionFallback` | with an occlusion capture, a particle more than 0.05 m below the occluder height dies; without one the module is inert and the emitters are counted in `Flux.Particles.OcclusionUnavailable` |
| `Test_ParticleWind_FrameEnvironment` | the `wind` module accelerates particles by the frame environment's wind as a CPU evaluation of the same formula within 10⁻⁴ relative; a frame without the environment applies no wind |

## Render tests

GPU tests on WARP and lavapipe. Every image assertion is an in-run comparison (two renders in the same run, or a render against an analytic expectation); VFX commits no golden image, so no test needs a recording ticket.

| Test | Asserts |
|---|---|
| `Test_ParticleDraw_SpriteFacing` | a single particle per facing mode renders a quad whose corners project to the positions computed on the CPU from the same basis, within 0.5 pixel |
| `Test_ParticleDraw_DegenerateDeadSlots` | a pool with every other slot dead draws exactly the live particles (pixel count of a 1-pixel-per-particle fixture) |
| `Test_ParticleDraw_PartitionOrder` | a premultiplied red layer behind a premultiplied blue layer and an additive green particle: the composite equals the analytic blend of back-to-front partition 0 then partition 1, within 1/255 |
| `Test_ParticleDraw_SortedBackToFront` | 64 overlapping alpha particles render identically to the CPU-sorted analytic composite, and differently with sorting disabled |
| `Test_ParticleDraw_SoftParticleFade` | a particle intersecting a plane fades linearly over `softDistance` of view depth, within 1/255 at 8 sample depths |
| `Test_ParticleDraw_FogAdditiveTransmittanceOnly` | in a homogeneous fog volume an additive particle's contribution is the unfogged value × transmittance, and a blended particle of opacity 0.25 composites as `c_p × T + L × 0.25` over its background (no glow beyond its coverage), within 1/255 |
| `Test_ParticleDraw_PreExposure` | the same frame at two pre-exposures 4 EV apart tonemaps within 1/255 |
| `Test_ParticleDraw_SubUVBlend` | the flipbook frame and blend factor at 16 ages equal the CPU formula, sampled from a fixture flipbook whose cells encode their index |
| `Test_ParticleDraw_RibbonStrip` | an emitter ribbon breaks at a dead particle and never joins head to tail after the ring wraps; a trail ribbon follows its samples |
| `Test_ParticleDraw_MeshParticles` | mesh particles draw the mesh's LOD 0 section at each particle's transform; a non-resident mesh draws nothing and counts |
| `Test_ParticleDraw_LitModes` | a particle lit by one point light: `vertex` and `pixel` modes equal `EvaluateLighting` of the documented surface at the centre and per pixel, within 2/255; lighting channels outside the light's mask leave it unlit |
| `Test_ParticleDraw_IndirectRunsPerPSO` | a view with 12 emitters over 3 PSOs issues exactly 3 `DrawIndexedIndirect` calls with the CPU-written counts (command recording inspection) |
| `Test_ParticleDraw_PerViewCulling` | an emitter outside one editor view's frustum and inside another is drawn by the second alone; a shadow view draws no particle |
| `Test_ParticleDraw_PreviewViewFeature` | a preview view with `FLUX_VIEW_FEATURE_PARTICLES` set draws particles; without it, none |
| `Test_ParticleDraw_TextureFallback` | an unresident sprite texture draws the white fallback and counts |
| `Test_ParticleDraw_RibbonUVModes` | on a CPU and on a GPU emitter ribbon, with u computed in the test from the CPU-known span of "Emitter ribbons" (`Zenith2/Docs/Design/VFX/Rendering.md`): `stretch` maps u from 0 at `q_lo` to 1 at the newest particle and `tailFade` scales alpha by u; `tile` repeats every `tileLength` metres of a distance-spawned ribbon |
| `Test_ParticleDraw_WeatherRainFollowsCamera` | a rain fixture of "Weather effects for ENV" (`Zenith2/Docs/Design/VFX/Authoring.md`) on an anchor moving 50 m in 2 s over a depth-only ground plane: particles keep their world positions as the anchor moves (none translates with it), new spawns lie in the moved spawn box, particles behind the kill volume die, and splash particles appear on the plane within one HiZ texel's depth |
| `Test_ParticleDraw_NoParticleFrameDeclaresNoPass` | with the API slice's definitions and with the full implementation, a view carrying `FLUX_VIEW_FEATURE_PARTICLES` and a null `m_pxParticles` declares no pass, and `Flux_AddParticleSimulation` with an empty frame declares none |
| `Test_ParticleDraw_TrailWidthAlphaCurves` | a trail's width and alpha at each sample index follow the render module's curves over the sample index |
| `Test_ParticleDraw_ViewDrawLimit` | 4,097 drawn emitters in one view draw the 4,096 most significant and count one in `Flux.Particles.ViewDrawsDropped` |
| `Test_ParticleDebug_ViewsRender` | each `Flux_ParticleDebugFlags` bit draws its view of "Debug views" (`Zenith2/Docs/Design/VFX.md`), and a Shipping-configured system ignores every bit |
| `Test_ParticleShaders_NoDiscardNoDepthWrite` | a source-text assertion over `Zenith2/Shaders/Passes/Particles/` and `Zenith2/Shaders/Particles/`: no `discard`, no depth output; and every particle draw PSO has depth writes off |
| `Test_VFXPreview_SessionDrawsEffect` | a preview world with one effect entity and a `VIEW_KIND_PREVIEW` request carrying `FLUX_VIEW_FEATURE_PARTICLES` draws the effect through the runtime path on WARP and lavapipe (non-zero particle pixels inside the effect's projected bounds, none outside) |

| Parity scene | Exercises | Comparison |
|---|---|---|
| `ParticlesBasic` | the engine effects `Debug_Sparks`, `Debug_Smoke`, `Debug_Ribbon`, `Debug_Trails` with a fixed camera path | `BackendParity` (Vulkan against D3D12, within the run) and `PathParity` (Forward against Forward+, the matrix row "Particles and ribbons (soft)") |
| `ParticlesLit` | `Debug_Smoke` lit in vertex and pixel modes under a sun and 16 point lights, with volumetric fog | as above |
| `ParticlesDebris` | `Debug_Debris` mesh particles with depth collision | as above |

Tolerances are proposed for `Z2-B-RPROD-031`'s table as for every parity scene: backend FLIP ≤ 0.01, path FLIP ≤ 0.015; aliasing on and off must be bit-identical ([Z2-N-REND-433]).

## Determinism tests

| Test | Asserts |
|---|---|
| `Test_ParticleSim_RunTwiceIdentical` | the same 300-frame scenario run twice in one process on one device gives a bit-identical pool, sorted pairs and colour target at every 50th frame |
| `Test_ParticleSim_AsyncComputeIdentical` | the scenario with `m_bAsyncCompute` on and off is bit-identical ([Z2-N-REND-791]) |
| `Test_VFXCpuSim_WorkerCountIndependent` | CPU emitters (an event chain among them) on 1, 2 and 8 workers produce bit-identical particle spans and commands |
| `Test_ParticleSim_AliasingIdentical` | the 300-frame scenario of `Test_ParticleSim_RunTwiceIdentical` with transient aliasing on (with poisoning) and off gives a bit-identical pool, sorted pairs and colour target ([Z2-N-REND-433]) |
| `Test_VFXExtract_BackendIndependent` | the command stream of a fixed scenario is identical on the three ECS backends |
| `Test_VFXPreview_SeekMatchesPlayback` | for `Debug_Sparks` (death events spawning smoke) and `Debug_Trails` in a 60 Hz preview world: a seek to 3.0 s (180 steps of `dt_s`) equals 180 frames of playback from the start with the same seed, each advancing the preview world's clock by exactly `GetTickDurationUs()` (one tick, alpha 0): the live set is equal (every slot live under each run's current generation), every live slot is equal field by field with `m_uFlags` bits 0–7 masked, and every step spawns the same children, event children included; raw pool bytes are not compared, because the seek's `RESET` advances the generation and leaves stale bytes in slots it has not reached |
| `Test_VFXPresentation_SimulationUnaffected` | a replay of 600 ticks with VFX enabled and with the module absent gives identical simulation state hashes (particles are presentation only and never feed simulation, `Zenith2/Docs/Design/VFX/Simulation.md → "Presentation-only particles"`) |
| `Test_VFXSave_ReplayIdentical` | a save at tick 300 loaded and run to 600 gives the same simulation hash and the same `Zenith.VFX.EffectState` values as the uninterrupted run |
| `Test_VFXCook_Reproducible` | cooking each fixture document twice gives byte-identical `.zvfx` files |

## Stress tests

In the `z2-stress` suite (600 seconds, every preset):

| Test | Asserts |
|---|---|
| `Test_VFXStress_MaxCapacityFountain` | `Stress_Fountain` at 262,144 particles for 600 frames on WARP and lavapipe: no validation error, no NaN in the pool, the live count at its steady state |
| `Test_VFXStress_EmitterChurn` | 10,000 detached effects started and finished over 3,000 frames: the pool, the instance table and every effect reference return to empty |
| `Test_VFXStress_TriggerFlood` | scripts trigger at `Z2-B-SCRIPT-024`'s capacity every tick for 3,000 ticks while C++ systems record twice the C++ share every frame: nothing accepted by the script merge is dropped by the queue |
| `Test_VFXStress_AssetChurn` | 1,000 load, play, unload cycles of an effect with textures and meshes leave no allocation, resource or reference behind |
| `Test_VFXStress_ManyWorlds` | four worlds with 1,024 emitters each under the per-frame GPU admission: admission is fair by significance across worlds and the frame stays within the command and draw limits |

## Failure and fault-injection tests

| Test | Asserts |
|---|---|
| `Test_VFXModule_Start_FailsAt<Step>_Unwinds` (one per fallible step of "Zenith_Module_VFX header" in `Zenith2/Docs/Design/VFX/PublicAPI.md`: `Allocate`, `EffectLoader`, `CommandKind`, `ScriptBindings`, `EnableParticles`, `ConfigListener`, `ProfilerPanel`; the last two also undo `EnableParticles` through `DisableParticles`) | the step's fault point fails that step alone; the returned error is the step's; no leak in the test heap, no registration left behind, no thread running ([Z2-N-CONTRACT-341]) |
| `Test_ParticleSystem_Create_FailsAt<Step>_Unwinds` (`Allocate`, `InstanceTable`, `Pool`, `QuadIndexBuffer`, `BoundsReadback`, `PendingList`) | as above, and `GetParticleSystem()` stays null |
| `Test_ParticleEffectReader_RejectsCorrupt` | each refusal of "Reader validation" (`Zenith2/Docs/Design/VFX/EffectFormat.md`) on a minimally mutated valid file returns its code; nothing asserts |
| `Test_ParticleEffect_LimitsAndLimitPlusOne` | each limit of "Format limits" at the limit loads and at the limit + 1 is refused ([Z2-N-ARCH-953]) |
| `Test_VFXEffect_MissingAssetPlaysNothing` | a component naming a missing effect resolves to the null object, plays nothing, counts `vfx.effects.failed`, and a subsequent mount that provides the effect starts it |
| `Test_ParticleSystem_PoolFullRetries` | with the pool at its maximum, a new emitter stays unallocated, draws nothing, counts `Flux.Particles.PoolFull`, and allocates once space frees |
| `Test_ParticleSystem_DeferredFrameKeepsCommands` | a frame deferred by the upload ring keeps its commands in the pending list and applies them, in order, the next frame; a failed frame drops only state-free commands and counts |
| `Test_ParticleSystem_DeviceLossRecovery` | after a simulated device loss every live GPU emitter's range, the quad index buffer and every effect constants buffer are recreated, GPU particles restart empty while ordinals and generations continue, spawning resumes on the first recovered frame, CPU particles are unaffected, and `Flux.Particles.DeviceResets` counts one |
| `Test_VFXScript_MisuseNeverAsserts` | every misuse of the Luau surface (wrong types, unknown effect, unknown parameter, stale entity) raises its code and leaves the world unchanged |
| `Zenith_Fuzz_ParticleEffect` | the reader over mutated cooked files never crashes, never asserts and never reads out of bounds (`Sanitizers -Preset win-asan` smoke run; the long run in stage acceptance) |
| `Zenith_Fuzz_VFXDocument` | the document validator over mutated JSON never crashes, never asserts, and reports at least one `Zenith_VFXCookErrorCode` for every document it refuses (`Sanitizers -Preset win-asan` smoke run; ZenithCook itself is `win`-only, so the target is built in `win-asan` by ASSET's `zenith_cook_fuzz_target` from the validator's `Internal/VFX/Zenith_VFXDocumentValidator.cpp` alone; the long run in stage acceptance) |

## Cook tests

| Test | Asserts |
|---|---|
| `Test_VFXCook_DocumentValidationOrder` | a document with one error of each check of "Document validation" reports all of them in one cook, in check order, each with its JSON pointer |
| `Test_VFXCook_IdMustMatchGuid` | a document whose `id` differs from the item GUID fails with `VFX_COOK_ERROR_SCHEMA` |
| `Test_VFXCook_CurveBakeAndDedup` | curves bake to the 32-sample tables of "Curve baking"; bit-equal tables are stored once; a curve beyond 1% error warns |
| `Test_VFXCook_CapacitySizing` | `auto` capacities for rate, burst, distance and event fixtures equal the formulas of "Capacity sizing" (`Zenith2/Docs/Design/VFX/Authoring.md`); an event source's event capacity is exactly `C_src × E`, and above 262,144 fails with `VFX_COOK_ERROR_LIMIT`; an explicit capacity below the estimate warns and is kept |
| `Test_VFXCook_EventTargetCapacityVariedLifetimes` | the auto-sized target capacity equals the event-target formula of "Capacity sizing" (`Zenith2/Docs/Design/VFX/Authoring.md`, including its two worked values 1,152 and 1,216) and makes the reference simulation report zero overwrites over 600 steps of every bake seed, for: a death edge whose source's lifetimes spread over [0.2, 2] s; a collision edge; a death edge whose source has a `killVolume` killing particles at uniformly random ages; and a death edge whose source has `collision` with `kill: true`, run with a plane collider at y = 0 (`m_axColliders`); and a three-emitter chain (a rate root with lifetimes in [0.2, 2] s, a death edge of 4 children into a middle target with lifetimes in [0.2, 1] s, a death edge of 4 children from it into a depth-2 target), whose depth-2 capacity equals the chain-form formula rounded by `clamp(nextPow2(max(64, ceil(A × 1.1))), 64, maxForTarget)` and is a power of two, with zero overwrites; for the first fixture a capacity of half the formula's (forced explicitly) reports some, and an explicit non-power-of-two capacity on the depth-2 target fails with `VFX_COOK_ERROR_EVENT_GRAPH` |
| `Test_VFXCook_BoundsBake` | the runtime composition of each fixture's cooked `B_e` or `S_t`, `D`, flags, `J`, `G`, `H` and root index contains every particle of 64 further seeds of the reference simulation on the fixture's track, run without colliders (a held-out check of the margin): a sparks emitter tilted 90°, a distance-spawned ribbon at 15 m/s, an `inheritVelocity` emitter at 10 m/s, a turbulent world-space emitter, a colliding local-space emitter, `Debug_Sparks` with its smoke target on a track moving at 10 m/s, and a `wind` emitter under a uniform 40 m/s wind with the wind cube `G × 40` added; `D`, `J`, `G` and `H` equal the analytic and composed terms of "Bounds baking" computed in the test (`D_own` per axis `½ a L (L + Δ_max)` with `Δ_max` = 1/15 s, the composed terms with the half-rounded `inheritVelocity`), and the held-out check of the gravity fixtures runs at fixed steps of 1/15 s as well as 1/60 s; the fixtures with a `collision` module carry bounds flag bit 2 and `VFX_COOK_WARNING_BOUNDS_ESTIMATE`, the linear-class fixtures neither; the 10⁸-step budget fails a pathological fixture with `VFX_COOK_ERROR_BOUNDS_BAKE`; an `updateType` emitter under `baked` fails |
| `Test_VFXCook_KeyInputs` | editing the document, a type's reflection or a referenced mesh's bounds changes the key; editing a referenced texture's pixels does not |
| `Test_VFXCook_ParticleTypeSet` | `particle_types.json` lists exactly the used types and triples of the fixture game, sorted; a missing compiled triple fails the cook with `COOK_ERROR_PERMUTATION_MISSING` |
| `Test_VFXCook_ReportMembers` | the `vfx` and `particleTypes` report members carry every field of "Cook report members" |
| `Test_VFXDocumentMigration_Fixtures` | every `v<N>.json` under the VFX migration fixtures migrates to its `v<N>.expected.json`, unknown fields preserved ([Z2-N-CONTRACT-548]) |
| `Test_TestEffectBuilder_MatchesCook` | the ZenithFlux test builder's file for each fixture equals the cook's output byte for byte; the fixtures cover non-default start delays and seed offsets, per-emitter LOD disabled-module masks differing between emitters, `axis` and `world` facing axes, every render flag bit, and baked and fixed bounds with world displacement boxes and cube flags, and the parsed `Flux_ParticleEmitterDesc` of each holds those values |
| `Test_ParticlePSO_PrecacheCoversCookedEffects` | the precache set of a fixture game holds every particle PSO its cooked effects can bind for each enabled path and depth format; a frame drawing every fixture effect records no PSO miss |
| `Test_VFXEngineContent_CooksClean` | every engine effect under `Zenith2/Engine/ZenithVFX/Content/Effects/` cooks with no error and no warning |

## Fixtures

| Fixture | Path |
|---|---|
| effect documents for unit, parity, cook and render tests | `Zenith2/Tools/ZenithCook/Tests/Fixtures/VFX/*.vfx.json` (one per module and per error class) |
| migration pairs | `Zenith2/Tools/ZenithCook/Tests/Fixtures/Migration/VFX/v<N>.json`, `v<N>.expected.json` |
| test particle types | `Zenith2/Engine/ZenithFlux/Tests/Particles/Types/TestUpdate_Spiral.slang`, `TestShading_Flat.slang` |
| textures | the engine effect textures (`Zenith2/Engine/ZenithVFX/Content/Textures/`) and a cell-index flipbook generated in the test |
| scenes | the parity scenes above and the integration suite's `Zenith2/Tests/VFX/Scenes/*.scene.json` |

No fixture is a committed binary: cooked files are produced by the cook or by `Flux_TestEffectBuilder` in the test run, so a format-version bump needs no fixture regeneration. The committed text fixtures (effect documents, migration pairs) are spec-derivable and edited by hand in the diff that changes the rule they test (the GOLDEN-FILES ruling); no VFX test needs a recorded run, and no test asks a worker to record a count, a digest or an image (the WORKER-EVIDENCE ruling).

## Gate steps

| Gate step | Runs |
|---|---|
| `Test` | `ZenithVFX_Tests`, `ZenithFlux_Tests` (particles, on WARP and lavapipe), the cook tests, `Zenith_Integration_VFX` |
| `BackendParity`, `PathParity` | the `Particles*` parity scenes (Stage 5) |
| `Sanitizers -Preset win-asan` | the fuzz smoke runs and every test above under ASan; ZenithVFX is a `ZENITH_GPU` module and is never built in `linux-tsan`, so the threaded tests (`Test_VFXCpuSim_WorkerCountIndependent`, `Test_VFXTriggerQueue_SequenceOrder`) compare results on 1, 2 and 8 workers: that proves the results are independent of the worker count, not that the code is free of data races; race freedom rests on the leaves sharing no writable state (by construction and review) and on JOBS's TSan runs of the job system they run on |
| `ShaderChecks` | the particle programs and the test types (`NoMin16Float`, `HalfRatioReport`; particle programs contain no `discard`) |
| device runs (`needs-human`, `gpu-reason:visual-measure`) | the `Z2-B-VFX-001` … `Z2-B-VFX-004` budget measurements on the reference desktop GPU and the S24 at Stage 5 acceptance, the sort stress (`2²⁰` keys for 1,000 frames with `onesweep`, deciding the S24's `vfx.sort.algorithm`, `Zenith2/Docs/Design/VFX/Simulation.md → "Reduce-then-scan fallback"`), and the slices' VFX scenes viewed on the S24 |
