# VFX design: simulation

Section file of `Zenith2/Docs/Design/VFX.md`. It specifies how effects run: the presentation-only model and its clock, the attached-effect lifecycle that simulation owns, detached effects and the trigger queue, the per-emitter presentation state machine, spawn scheduling, the deterministic random numbers, particle initialisation and update with every module's arithmetic, collision, trails, LOD and significance, the CPU and GPU budgets and admission, the CPU simulator, the GPU pipeline pass by pass with dispatch shapes and complexity, the determinism rules, the render-world command stream (including deferred frames and device loss) and the memory-pressure response. The C++ simulator (`Zenith2/Engine/ZenithVFX/Internal/Simulation/`) and the Slang simulator (`Zenith2/Shaders/Passes/Particles/`, `Zenith2/Shaders/Particles/`) implement the same steps in the same order; where this file gives a formula, both evaluate exactly that expression in `float`.

## Scope of this file

| Topic | Heading |
|---|---|
| what particles are and which clock drives them | "Simulation model" |
| play, stop, auto-destroy and events of entity-attached effects | "Attached effect lifecycle" |
| fire-and-forget effects | "Detached effects and the trigger queue" |
| each emitter's presentation states | "Emitter instance state machine" |
| how many particles spawn each frame, with which ages | "Spawn scheduling" |
| random numbers | "Particle random numbers" |
| what a new particle looks like | "Particle initialisation" |
| one simulation step | "Particle update order" |
| colliders and the depth buffer | "Collision" |
| per-particle history | "Particle trails" |
| distance-based quality | "LOD and significance" |
| what may run this frame | "Budgets and admission" |
| the CPU target | "CPU simulation" |
| the GPU target | "GPU simulation" |
| what is reproducible | "Determinism rules" |
| how commands reach the renderer | "Render-world command stream" |
| memory pressure | "Memory pressure response" |

## Simulation model

### Presentation-only particles

Particles are presentation state. No simulation node reads a particle, an emitter's presentation state, a spawn count or a GPU result: the simulation side of VFX is the attached-effect lifecycle ("Attached effect lifecycle"), which depends only on component values and the tick index. So the determinism guarantee (Z2-D-019) holds for every world with the VFX module whatever particles do, memory-pressure shedding of particles never changes a simulation result ([Z2-N-ARCH-812]), and a replay never needs a particle. Particle appearance is itself reproducible under the narrower rule of "Determinism rules": the same render-world command sequence gives the same particle state, bit for bit, on one backend, device and build.

The consequences that shape the design:

- Particle collision raises events only inside the effect (sub-emitters, "Event spawning"); gameplay never observes a particle collision. Gameplay effects of impacts (sounds, damage) come from simulation (physics contacts, scripts), which may also start an effect.
- An entity's destruction never cuts its particles off: the effect is released and its particles live out on the render side ("Emitter instance state machine").
- The GPU never reports particle state back to the CPU except the development-build validation readback (bounds and live counts), which feeds diagnostics alone ("GPU bounds validation"); no admission, scheduling or draw decision ever reads a GPU-written count.

### Presentation clock

<!-- elaboration -->
Each world's presentation time for frame k is `T_k = (n_k + α_k) × t_tick` in `double`, where `n_k` is the world's tick index after the frame's last tick and `α_k` the frame's interpolation alpha (`Zenith2/Docs/Architecture/CPUFrameGraph.md → "Execution"`, the simulation clock), `t_tick` the tick duration in seconds. `VFX.Update` reads `n_k` and `t_tick` from its `Zenith_SystemContext` and `α_k` from the world's simulation clock, which `Zenith_WorldAttachContext` hands the module at attach time.

`Zenith_WorldAttachContext` exposes `const Zenith_SimulationClock& GetSimulationClock() const` (`Zenith2/Docs/Architecture/CompositionRoot.md → "Module interfaces"`, cross-doc item X243 (a)) (MAIN_THREAD during `AttachWorld`; the reference stays valid for the world's lifetime), the same clock RPROD's engine extractors receive through `Zenith_EngineExtractorDeps::m_pxClock` (`Zenith2/Docs/Design/RPROD/Extractors.md → "Extractor registration"`).

Per effect instance: `Δ_k = clamp(float(((double(n_k − n_{k−1}) + (double(α_k) − double(α_{k−1}))) × t_tick) × double(timeScale)), 0, fFLUX_PARTICLE_MAX_STEP_SECONDS)` with `t_tick = double(GetTickDurationUs()) × 10⁻⁶`, which is `(T_k − T_{k−1}) × timeScale` algebraically, evaluated in tick units so that a frame of exactly one tick at alpha 0 and time scale 1 yields `float(t_tick)`, the catch-up step `dt_s` of "Catch-up steps", bit for bit; `Δ_k` is 0 while the component's `Paused` is set, the world is paused (its tick index and alpha do not move, [Z2-N-ARCH-507]) or a preview freezes it. Effect time advances by `Δ_k`, so a hitch longer than 1/15 s slows the effect instead of teleporting it; simulation time is unaffected. Every simulator step of the frame uses `dt = Δ_k`.

### Where each part runs

| Work | Node or call | Thread | Per |
|---|---|---|---|
| attached-effect lifecycle: play, stop, generations, destroy-after, started and stopped events | `VFX.Lifecycle`, a C++ system of phase 10 (`PHASE_LATE_SCRIPTS`) | any worker | tick |
| spawn commands into the trigger queue | `VFX.Spawn` playback at sync points; the VFX trigger consumer at script merges | the sync-point and merge nodes | tick |
| presentation: trigger drain, effect resolution, viewers and LOD, CPU admission, spawn scheduling, CPU particle simulation, effect lights and decals, the frame's command staging | `VFX.Update`, an epilogue stage of stage 12 (registered with `AddEpilogueStage`), CPU emitters fanned out with `ParallelFor` | any worker | frame |
| the frame's emitter commands, effect lights and effect decals into the render world | `VFX.Extract`, the module's extractor (phase 12, `After("VFX.Update")`) | any worker | frame |
| render side: commands into the instance table, GPU admission, the frame's dispatch and draw lists, frame data | `Flux_ParticleApplyRenderWorld`, `Flux_ParticleBuildFrame`, `Flux_ParticleWriteFrameData` on `Flux.BuildGraph` | `Flux.BuildGraph` node | frame (k − 1) |
| GPU emission, simulation, sort keys, sort | the passes of `Flux_AddParticleSimulation`, common prologue row 4 | GPU, async-compute queue where the graph finds a gap | frame |
| draws | `Flux_AddParticleDraw` per view | GPU graphics queue | view |

## Attached effect lifecycle

An attached effect is an entity with `Zenith.VFX.Effect`. Its lifecycle is simulation state held in `Zenith.VFX.EffectState`, so saves and replays restore it exactly.

### Lifecycle state machine

States are `Zenith_VFXEffectState` values, plus "no state component" (NEW). `VFX.Lifecycle` evaluates, once per tick per entity: row 8 first, and when it matches nothing else happens for that entity this tick; otherwise the first matching row of 1 … 6 for the entity's state, then row 7.

| # | State | Event | Next state | Action |
|---|---|---|---|---|
| 1 | NEW | the entity has `Zenith.VFX.Effect` and no `Zenith.VFX.EffectState`, `AutoPlay` true | PLAYING (when the add plays back) | record `AddComponent(Zenith.VFX.EffectState{ StartTick = tick, StopTick = 0, ObservedPlay = PlayRequest, ObservedStop = StopRequest, Generation = 1, State = PLAYING })`; emit `Zenith.VFX.EffectStarted{ entity, effect, 1 }` |
| 2 | NEW | as 1, `AutoPlay` false | IDLE | record the add with `State = IDLE`, `Generation = 0`, the observed values the current requests |
| 3 | any of IDLE, PLAYING, STOPPED | `PlayRequest ≠ ObservedPlay` | PLAYING | when PLAYING: emit `EffectStopped{ …, Generation, reason 2 }`; then `Generation += 1`, `StartTick = tick`, `StopTick = 0`, `ObservedPlay = PlayRequest`, `ObservedStop = StopRequest` (a stop requested in the same tick is absorbed by the restart); emit `EffectStarted` |
| 4 | PLAYING | `StopRequest ≠ ObservedStop` | STOPPED | `ObservedStop = StopRequest`, `StopTick = tick`; emit `EffectStopped{ …, reason 1 }` |
| 5 | IDLE, STOPPED | `StopRequest ≠ ObservedStop` | unchanged | `ObservedStop = StopRequest` |
| 6 | any | none of the above | unchanged | none |
| 7 | PLAYING, STOPPED | `DestroyAfterSeconds > 0` and `tick − StartTick ≥ D`, `D = ceil(n − e)` converted to `uint64`, with `n = double(x) × double(tickHz)` (`x` the `float` value of `DestroyAfterSeconds`, `tickHz` the configured integer rate `sim.tickHz` of `Zenith2/Docs/Architecture/CPUFrameGraph.md → "Simulation clock"` read at Start) and `e = min(0.5 × ulp(x) × tickHz, 0.4990234375)`, `ulp(x)` the spacing of `float` values at `x` (`2^(E − 150)` for the biased exponent field `E = (bit_cast<uint32>(x) >> 23) & 0xFF` when `E ≥ 1`, `2⁻¹⁴⁹` when `E = 0`) | IDLE | record `Destroy(entity)`; emit `EffectStopped{ …, reason 3 }` (once: the state leaves PLAYING and STOPPED) |
| 8 | any | a component value outside its domain (time scale outside [0, 8], a non-finite parameter or time) | unchanged | nothing starts or stops for the entity this tick; `vfx.lifecycle.invalid_components` + 1; one `vfx` warning per entity per run |

The rounding rule of row 7 counts ticks of the configured rate, not of the rounded tick duration (33,333 µs at 30 Hz), so 0.5 s is 15 ticks at 30 Hz, 30 at 60 Hz and 60 at 120 Hz. The subtraction of `e` absorbs the `float` representation of the decimal value the author wrote, and only that:

- `n` is exact: `x` has 24 significant bits and `tickHz` is below 2²⁹ (every configured rate), so their product needs at most 53 bits and is exact in `double`.
- Exactness: let `v` be the decimal value the author wrote, `x` its nearest `float`, so `|v − x| ≤ ulp(x) / 2`. When `v × tickHz` is an integer `m` and `0.5 × ulp(x) × tickHz < 0.5`, then `|n − m| ≤ e`, so `n − e` lies in `[m − 2e, m] ⊂ (m − 1, m]` and `D = m`, the decimal-exact count. The condition holds for every value of the property's domain [0, 86,400] at every `tickHz` ≤ 127, because `ulp(x) ≤ 2⁻⁷` below 131,072.
- Otherwise `D` is `ceil(n)` or one tick fewer, because `n` is exact and `0 ≤ e < 0.5`; against the decimal value it is within one tick of `ceil(v × tickHz)` on either side whenever `ulp(x) × tickHz ≤ 1` (every value of the domain at every rate up to 128 Hz): one tick fewer when `v × tickHz` lies within `2e` above an integer, and one tick more only where the cap binds (128 Hz in the top binade), when `n − e` can exceed `m`. At rates where `ulp(x) × tickHz > 1` the `float` itself cannot hold the decimal value to within a tick, and `D` follows `x`.
- Worked values: 0.1 s at 30 Hz (`x` = 0.100000001490116, `n` = 3.0000000447, `e` = 1.1 × 10⁻⁷) is 3 ticks, not 4; 0.51 s at 30 Hz (`n` = 15.2999997) is 16; 86,400 s at 120 Hz (`ulp` = 2⁻⁷, `e` = 0.46875, `n` = 10,368,000) is exactly 10,368,000.

`Generation` is a 32-bit counter; the instance key mixes it in ("Instance keys" of `Zenith2/Docs/Design/VFX/PublicAPI.md`), so a restart is a new presentation instance and the old instance's particles live out.

### Lifecycle system

`VFX.Lifecycle` (`Zenith_SystemDesc{ "VFX.Lifecycle", PHASE_LATE_SCRIPTS, &Zenith_VFXLifecycleRun, pxWorldState, JOB_AFFINITY_ANY, WRITER_SOURCE_NONE, 20 }`) declares:

| Declaration | Content |
|---|---|
| `Read(Zenith.VFX.Effect)` | every property |
| `Write(Zenith.VFX.EffectState)` | every property (VFX.Lifecycle is the one direct writer, [Z2-N-ARCH-484]) |
| `Query` Q1 | `Zenith.VFX.Effect` and not `Zenith.VFX.EffectState` |
| `Query` Q2 | `Zenith.VFX.Effect` and `Zenith.VFX.EffectState` |
| `EmitsCommands()` | the state additions and the destroys |
| `EmitsEvent` | `Zenith.VFX.EffectStarted`, `Zenith.VFX.EffectStopped` |

Its body runs `ParallelFor` over the chunks of Q1 and Q2 (grain 1 chunk), each leaf iterating rows in row order and applying the state machine above with the entity's own command buffer and event writer (`Zenith_SystemContext::Commands(ulSimulationID)`, `Events(ulSimulationID)`), so commands and events merge in (system order, logical invocation ID, sequence) order whatever the worker count ([Z2-N-CONTRACT-354]). Cost is O(entities with an effect) per tick with no allocation beyond the command records; the system has no per-world state of its own. Late behaviours that write `PlayRequest` or `StopRequest` directly are ordered before it by the same-stage reader-after-writer rule; writes from other phases arrive by command at earlier sync points. Its CPU cost is inside the `CPU-SIM` bound through the pending row "VFX lifecycle CPU" ("Pending row needs" of `Zenith2/Docs/Design/VFX.md`).

## Detached effects and the trigger queue

A detached effect is a fire-and-forget instance with no entity of its own: an impact spark, a muzzle flash. It is presentation only, never saved, and never visible to simulation.

<!-- elaboration -->
| Element | Rule |
|---|---|
| queue | the world resource `Zenith.VFX.TriggerQueue` (`Zenith_VFXTriggerQueue`, internal to ZenithVFX), owning phase `PHASE_LATE_SCRIPTS`, one per world, a fixed array of `vfx.triggerQueueCapacity` records of 112 bytes (the 96-byte payload, the 8-byte sequence, the 8-byte recording tick index), `QUEUE_FULL_POLICY_DROP_NEWEST` ([Z2-N-CONTRACT-343]) |
| writers | the `VFX.Spawn` playback (sync-point step 6, merge order) and the VFX trigger consumer (script commit, merge order); both run one at a time per world ([Z2-N-CONTRACT-346] does not apply: the queue's capacity is presentation, so a full queue drops the newest record and simulation is unchanged) |
| shares | the first `vfx.triggerQueueScriptReserve` records of a frame's capacity are reserved for the script consumer: a `VFX.Spawn` playback (C++ systems) appends only while the frame's C++ records number fewer than `vfx.triggerQueueCapacity − vfx.triggerQueueScriptReserve`, and drops the record otherwise (`vfx.triggers.dropped` + 1); the script consumer appends while the queue holds fewer than `vfx.triggerQueueCapacity` records. The reserve is at least `Z2-B-SCRIPT-024` VFX trigger capacity per tick × `sim.maxCatchUpTicks` (the triggers one frame's script merges can accept), so a script trigger the merge accepted is never dropped by the queue; Start refuses a reserve above `vfx.triggerQueueCapacity − 64` by taking both keys' defaults with one `vfx` warning |
| saving | the resource registers no serialized property, so a saved world carries no pending trigger and a load starts with an empty queue (a trigger recorded in the tick of a save plays in the saving process only) |
| sequence | each appended record takes the world's next trigger sequence (a `uint64` the queue holds and never resets during the world's life), so sequences follow (phase order, merge order) and are identical on every run |
| reader | `VFX.Update`, its sole consumer (`WriteResource`, [Z2-N-ARCH-727]), drains every record each frame in sequence order |
| start time | a record's effect time at frame k is `max(0, T_k − n_rec × t_tick + m_fLeadSeconds)`, where `n_rec` is the tick index the record carries and `m_fLeadSeconds` the payload's lead (how long before the end of tick `n_rec` the trigger happened, 0 for most triggers; an animation notify's sub-tick time); computed in `double` from the record and the presentation clock alone, so it is the same on every run |
| key and seed | `Flux_MakeDetachedParticleKey(worldTag, sequence)` (`uint16` world tag); the payload's seed, or `Flux_DeriveParticleSeed(key)` when it is 0 |
| per-world cap | at most `vfx.detachedEffectsPerWorld` live detached instances: a record beyond the cap is discarded in sequence order, `vfx.detached.dropped` + 1 |
| attach | with `VFX_SPAWN_FLAG_ATTACH`, the instance follows its target's interpolated world transform each frame (position always; rotation and scale with their flags), the payload transform applied as a local offset; a target that stops resolving freezes the instance at its last transform and releases it |
| endless and looping effects | a detached instance plays one timeline cycle: `loop` is ignored, and an endless effect (`duration` 0) spawns for `fVFX_DETACHED_ENDLESS_SECONDS` = 10 s; each case counts `vfx.detached.truncated` |
| end | after the timeline (and every emitter's `startDelay` + `duration`) the instance releases every emitter and forgets its state once its linger time has passed |
| capacity sizing | `vfx.triggerQueueCapacity` defaults to the pending row "VFX trigger queue capacity per world" proposal of 1,024 (desktop) and 512 (S24), and `vfx.triggerQueueScriptReserve` to 512 and 256; the Stage 1 VFX budget ticket sets the reserve to `Z2-B-SCRIPT-024` × `sim.maxCatchUpTicks` and the capacity to the reserve plus the C++ share it rules (`Zenith2/Docs/Design/VFX.md → "Pending row needs"`); `Test_VFXTriggerQueue_ScriptReserve` and `Test_VFXStress_TriggerFlood` hold the rule |

## Emitter instance state machine

`VFX.Update` keeps, for every live instance (attached or detached), one presentation record per emitter. Events are evaluated in the order listed each frame; `—` means the event cannot change that state.

| State | effect resolved | effect failed | start delay reached | timeline end | stop | restart or seek | LOD culled | LOD admitted | linger elapsed |
|---|---|---|---|---|---|---|---|---|---|
| WAITING_ASSET | DELAYED, or SPAWNING when the delay is 0 | DONE (`vfx.effects.failed` + 1) | — | — | DONE | WAITING_ASSET (new generation) | — | — | — |
| DELAYED | — | — | SPAWNING | — | RELEASED | DELAYED (reset) | DELAYED (no command) | DELAYED | — |
| SPAWNING | — | — | — | RELEASED (non-looping) | RELEASED | SPAWNING (RESET, PREWARM) | CULLED | — | — |
| CULLED | — | — | — | RELEASED | RELEASED | SPAWNING on admission | — | SPAWNING (RESET when its cull behaviour is kill) | — |
| RELEASED | — | — | — | — | — | SPAWNING (new generation) | RELEASED | RELEASED | DONE |
| DONE | — | — | — | — | — | — | — | — | — |

| Transition | Command `VFX.Extract` writes for the emitter |
|---|---|
| any frame in DELAYED, SPAWNING, CULLED with kill (inactive) or pause | one `UPDATE`: `ACTIVE` when spawning and admitted; `VISIBLE` when the instance is not LOD-culled; `PAUSED` while paused |
| entering RELEASED | one `RELEASE`, then nothing (the render side destroys the emitter after its last particle; a CPU emitter keeps simulating its live particles on the CPU, sending `UPDATE` with zero spawns until they are gone, then `DESTROY`) |
| entering DONE from WAITING_ASSET; a world detach; an effect asset change on the component | one `DESTROY` |

The linger time of a released emitter is the effect's `GetLingerSeconds()`. An attached effect whose entity is destroyed, or whose component is removed, releases every emitter of its instance in the frame `VFX.Update` no longer finds it; a change of the component's `Effect` destroys the old instance (its particles vanish) and starts a new one.

## Spawn scheduling

Spawn counts are computed on the CPU, by `VFX.Update`, for both targets: the CPU therefore knows every emitter's exact workload, which is what makes admission and budgets exact ("Budgets and admission"). Per emitter it keeps a `double` rate accumulator `a` (the spawn scheduler runs on the CPU alone, so the "`float` on both sides" rule of this file does not apply to it), a `float` distance accumulator `d`, the `uint32` next spawn ordinal `o`, the timeline position and loop index.

### Rate spawning

With the emitter's timeline position `x` in [0, 1] (`(τ − startDelay) / duration` within the current cycle, at the middle of the frame's interval `τ − Δ/2`), the rate `r = Evaluate(rate, x)` (C, K or P forms; an endless emitter admits C and P alone, `VFX_COOK_ERROR_VALUE_FORM` otherwise), and the spawn scale `S = spawnScale(LOD level) × vfx.spawnScale`:

1. `a += double(r) × double(Δ) × double(S)`; `n = floor(a)`; `a −= n` (exact in `double`).
2. Distance spawning adds its count ("Distance spawning") to `n`; the two share the frame's even spacing.
3. `n > capacity`: `n = capacity`, `a = frac(a)`, `vfx.spawns.clamped` + the excess.
4. With `n > 0`: `m_fSpawnAgeStep = Δ / n`, `m_fSpawnAgeFirst = Δ × (n − 0.5) / n`; particle j (0 ≤ j < n) has age `m_fSpawnAgeFirst − j × m_fSpawnAgeStep` at the end of the frame, so spawns are evenly spread through the frame rather than bunched at its end, and its spawn transform is the interpolation of the previous and current emitter transforms at fraction `f_j = 1 − age_j / Δ`.

### Burst spawning

For each burst b of the emitter and each cycle c (`0 … cycles − 1`, unbounded when `cycles` is 0 and the effect loops), the burst time in the timeline is `t_bc = time + c × interval` (+ `loop × duration` for loop index `loop`). When `t_bc` lies in the frame's timeline interval `(τ − Δ, τ]`:

- count = `countMin + min(floor(U × (countMax − countMin + 1)), countMax − countMin)`, `U = Flux_ParticleRandomUnit(seed, 0xFFFFFFFF − loop, 0x2000 + (b << 8) + min(c, 255))`, then `× S` rounded down;
- the frame's burst count is the sum over every burst that fires; its age offset `m_fBurstAgeOffset = τ − t_last`, `t_last` the latest firing burst time of the frame (several bursts in one frame share the latest's age: a deterministic approximation, which the cook reports as `VFX_COOK_WARNING_BURST_SPACING` when two burst times of one emitter are closer than 1/30 s; a burst larger than the capacity is `VFX_COOK_WARNING_CAPACITY_OVERFLOW`);
- burst particles take the ordinals after the frame's rate spawns, and their spawn transform is the current one.

### Distance spawning

`d += |p_k − p_{k−1}|`, the world-space emitter origins of this frame's and the previous frame's presentation transforms; with `TELEPORTED`, `d = 0` and no distance spawn happens. `n_d = min(floor(d / spacing), maxPerFrame)`, `d −= n_d × spacing`. The `n_d` particles join the rate spawns' even spacing (rule 4 of "Rate spawning"), so a trail emitter moving at constant speed places particles `spacing` metres apart along the interpolated path.

### Event spawning

An event-spawned emitter (a target) has no CPU-scheduled spawns: its children come from its source emitter's event records of the source's previous executed simulate step.

<!-- elaboration -->
| Step | Rule |
|---|---|
| event slots | the source has `E` event slots per particle: `m` (its collision edge's `maxCollisions`, 1 … 3, or 0 without a collision edge) plus 1 when it has a death edge, so 1 ≤ `E` ≤ 4. The compact event index `e` is the one rule the record, the constant and both simulators use: the particle's c-th raised collision (c = 1 … m) is `e = c − 1`, its death is `e = m` (so `e = E − 1`, and `e = 0` for a death-only source) |
| edge range | each outgoing edge owns a contiguous range of the source's slots: a collision edge `[0, m)`, a death edge `[m, m + 1)`; `e_base` is the range start and `E_edge` its length (`m` or 1), carried in the target's command (`m_uEventEdge`, `Zenith2/Docs/Design/VFX/PublicAPI.md → "Flux_EmitterCommand"`) |
| ring | one ring per source of exactly `m_uEventCapacity = C_src × E` records (`Flux_ParticleEventRecord`, 48 bytes), the single rule the cook, the reader, the pool and the tests use (`Zenith2/Docs/Design/VFX/Authoring.md → "Capacity sizing"`); `C_src × E` above `uFLUX_PARTICLE_MAX_EVENT_RECORDS` (262,144) is `VFX_COOK_ERROR_LIMIT` at cook time and `FLUX_PARTICLE_ERROR_CORRUPT` in the reader. The ring is a slot array, never a queue: every record has its own index, so it is never full and nothing is dropped |
| raising (source, in its update) | a particle whose death or collision matches an edge writes its record at ring index `i = (ordinal mod C_src) × E + e`, with the kind bit of its event (bit 2 death, bit 3 collision) and `e` in bits 0–1 of `m_uKindAndIndex`. Slot `ordinal mod C_src` belongs to one particle at a time, a particle raises event `e` at most once per step (one collision per step; death once), so no two writers of a step meet and no record is ever dropped. The record carries the source command's `m_uStepIndex`, so the ring never needs clearing |
| step tags | the render side keeps a 32-bit step counter per emitter. `Flux_ParticleBuildFrame` writes, into each of the emitter's commands of the frame in step order, the counter's value and advances it by one ("Frame building"); a frame in which the emitter is not simulated (budget-refused, paused or culled) leaves it unchanged, and `Flux_ParticleEndFrame(false)` restores the value it had before a deferred or failed frame ("Deferred and failed frames") |
| consuming (target, in the emission of each of its steps) | the render side gives each target command the tag it consumes (`m_uSpawnOrdinalBase` of an event target, `Zenith2/Docs/Design/VFX/PublicAPI.md → "Flux_EmitterCommand"`): the tag of the source's latest step that precedes this step in execution order, which is either the source's step s − 1 built earlier in this frame (target step s ≥ 1) or the source's last step of the last submitted frame (target step 0), when that tag is newer than the target's last consumed tag; else `0xFFFFFFFF` (nothing to consume). One thread per (slot, edge event, child) reads the record at `slot × E + e_base + e_local` and proceeds only when its tag equals the named tag, its generation is the source's current one, its kind bit is the edge's own `on` (a death edge's consumer ignores collision records and the reverse), and its index bits equal `e_base + e_local`; a cleared record has no kind bit, so a freshly cleared or recovered ring never spawns a child. So a seek's catch-up steps (several per frame, executed in step order) keep every child, a deferred frame consumes nothing and leaves the records for the next submitted frame, and no record is consumed twice |
| admission coupling | a target is admitted, refused or paused together with its source in every step (GPU and CPU admission treat an edge's emitters as one unit, "Budgets and admission"), and the emitters of one instance always carry the same number of steps in a frame, so a source never advances a step its target misses |
| dispatch size | a target's event batch has `C_src × E_edge × k` threads whether or not any event fired (`k` the edge's children per event); thread `i` decodes `slot = i / (E_edge × k)`, `e_local = (i / k) mod E_edge`, `j = i mod k`. GPU admission counts those threads ("GPU admission per frame"); over a source's two edges with equal `k` they total `C_src × E × k` |
| child ordinal | `o = (srcOrdinal × E_edge + e_local) × k + j` for child `j`, computed in 32-bit wrapping arithmetic on both simulators; its slot is `o mod C_target`. For a target at depth 1 (its source is no event target) the source resets with its whole event graph before `o` could pass `2³² − 2²⁴` ("Spawn ordinals and capacity"), so `o` never wraps and `o mod C_target` is continuous whatever `C_target`'s factors. A target at depth 2 or more (its source is itself an event target, whose ordinals reach `2³² − 2²⁴ − 1`) has child ordinals that wrap; its capacity is a power of two (`Zenith2/Docs/Design/VFX/Authoring.md → "Capacity sizing"`, refused otherwise by the reader), so its slot `o mod C_target = o & (C_target − 1)` is continuous across the wrap, `C_target` dividing `2³²`, and the wrap is harmless at any depth. The span of live children's ordinals, its chain-form derivation and the earliest event age `d_min` (0 for a collision edge and for a source with any killing module) are the event-target derivation of `Zenith2/Docs/Design/VFX/Authoring.md → "Capacity sizing"`; a target capacity at least that span never overwrites a live child |
| child state | position the record's, velocity the target's initial velocity plus `inheritVelocity ×` the record's velocity (the edge's `inheritVelocity` carried as a half-precision value in `m_uEventEdge` bits 16–31, which the C++ simulator rounds identically), colour the target's initialisation (the record's colour is available to a code-authored type), age 0 and not born-this-step, so the child is integrated in the same step's update |
| CPU | the same steps in C++: the ring is double-buffered by step parity (the source writes the half of its current step, the target reads the other half), and an instance's CPU emitters run in one leaf, emitter by emitter in table order (sources precede targets) and step by step ("CPU update algorithm") |

### Spawn ordinals and capacity

<!-- elaboration -->
- The rate and distance spawns of a frame take ordinals `[o, o + n)` and the bursts `[o + n, o + n + n_b)`; then `o += n + n_b`. The particle in slot `s` of a capacity-`C` emitter is the latest spawn whose `ordinal mod C = s`.
- When `n + n_b > C`, the emit threads whose ordinal is overwritten afterwards in the same frame (`ordinal + C < o + n + n_b`) do nothing, so the result is the last `C` spawns, deterministically.
- A new particle overwrites the slot's previous particle whether or not it is still alive; the cook sizes `auto` capacities so that overwriting a live particle never happens at the authored maximum rate (`Zenith2/Docs/Design/VFX/Authoring.md → "Capacity sizing"`), and the GPU validation build counts overwritten live particles in `Flux.Particles.Overwrites`.
- An emitter is reset (new generation, `o = 0`) in the frame whose next ordinal `o' = o + n + n_b` would satisfy `o' × P > 2³² − 2²⁴` in `uint64` arithmetic, where `P = 1` for an emitter that is no event source and `P = max(E_edge × k)` over its outgoing edges for an event source ("Event spawning"; at most 3 × 16 = 48), because `ordinal mod C` is continuous only while a 32-bit ordinal does not wrap (for a capacity that is no power of two), and a depth-1 child's ordinal is below `o' × E_edge × k`. The largest spawn ordinal and the largest depth-1 child ordinal then stay at or below `2³² − 2²⁴ − 1`; only an emitter with CPU-scheduled spawns has a next ordinal `o'` (an event target's ordinals derive from its source's), so `P` covers its own outgoing edges alone, and the children of a target at depth 2 or more may wrap, harmlessly, under their power-of-two capacity ("Event spawning"). An event source's reset resets every emitter of its connected event graph in the same frame (each takes a new generation, the targets' rings and live children included), because the targets' ordinals derive from the source's: a source restarting at 0 alone would place new children on the slots of live old ones. At 10,000 particles per second the limit is 4.9 days of continuous spawning for `P = 1` and 2.5 hours for `P = 48` (`(2³² − 2²⁴) / 48` = 89,128,960 ordinals). `Test_VFXSchedule_OrdinalResetBeforeWrap` starts a plain emitter at ordinal `2³² − 2²⁴ − 100` and an event source with a collision edge of `maxCollisions` 3 and 16 children at ordinal 89,128,860, and asserts each reset frame (the frame whose spawns would pass the limit), the new generations of the source and its target in that same frame, ordinal 0, and that no depth-1 child ordinal exceeds `2³² − 2²⁴ − 1`; it also starts the root of a three-emitter chain where the depth-2 child ordinals wrap and asserts zero `Overwrites` across the wrap.
- A ring slot never frees on death: a slot is reused only when its ordinal comes round again, so capacity must cover the authored maximum rate times the maximum lifetime (`C ≥ r_max × L_max`, "Capacity sizing" of `Zenith2/Docs/Design/VFX/Authoring.md`), whatever fraction of particles dies early; this is cost V2 of `Zenith2/Docs/Design/VFX.md → "VFX rulings"`.

### Prewarm

An emitter with `prewarm` P > 0 starts in the state it would have after P seconds of rate spawning: in its first `SPAWNING` frame (and after a restart) `VFX.Update` sets `PREWARM`, computes `n = min(floor(r_0 × P × S), C)` with `r_0` the rate at the timeline start, and sends the `n` prewarm spawns as the frame's spawns with ages evenly spaced over `[0, P)` (`m_fSpawnAgeFirst = P × (n − 0.5) / n`, step `P / n`). Each prewarm particle is initialised as usual and then integrated from its birth in steps of `min(age, fFLUX_PARTICLE_PREWARM_STEP_SECONDS)` (`ceil(age / step)` steps of `age / ceil(age / step)` each), running every update module except `collision`, `wind`, `trail`, `precipitationOcclusion` and event raising; a particle whose age reaches its lifetime is not created. The emitter's ordinal advances by `n`. The CPU and the GPU run the same loop inside the particle's emission.

### Catch-up steps

A preview seek (`Zenith.VFX.PreviewControl::SeekRequest`) resets the instance and re-simulates it from effect time 0 to `SeekSeconds` at a fixed step equal to the preview world's tick duration, `dt_s = float(double(GetTickDurationUs()) × 10⁻⁶)` of the world's simulation clock (`Zenith2/Docs/Architecture/CPUFrameGraph.md → "Simulation clock"`; 16,667 µs in a 60 Hz world), so the scrubbed live state equals the effect played from its start with one tick per frame, whose presentation Δ is `dt_s` bit for bit ("Presentation clock"). The seek has `N = floor(double(SeekSeconds) / double(dt_s) + 0.5)` steps (in a 60 Hz world 3.0 s is 180). `VFX.Update` runs `n_f = min(vfx.preview.catchUpStepsPerFrame, N − done)` of them in frame f (default 8, so a 10 s seek in a 60 Hz world takes 75 frames), every step with `dt = dt_s` and its own spawn scheduling ("Spawn scheduling"); a seek to 0 (`N` = 0) is one `RESET` command with `dt` 0 that carries neither `FLUX_EMITTER_FLAG_CATCH_UP` nor `FLUX_EMITTER_FLAG_SEEK`:

| Target | Steps of one frame |
|---|---|
| CPU emitter | run in place in the instance's leaf, step by step ("CPU update algorithm"); the frame's span is the state after the last |
| GPU emitter | `n_f` commands, appended to the render world in step order: the first `n_f − 1` flagged `FLUX_EMITTER_FLAG_CATCH_UP` (simulated, never drawn), the last without it (simulated, then drawn); all `n_f`, the last included, carry `FLUX_EMITTER_FLAG_SEEK`, so the render side never merges a seek's step whatever `n_f` is ("Deferred and failed frames"); the first carries `RESET` in the seek's first frame. The render side numbers an emitter's commands of a frame in arrival order, so command k (k = 1 … `n_f`) is step k − 1 and the drawn command is the emitter's last step; a non-seeking emitter's single command is step 0 ("Frame building") |
| every emitter of the instance | the same `n_f`, so a source and its event targets carry equal step counts in every frame |

After the frame in which `done` reaches `N`, a frozen preview stays there (Δ = 0) and an unfrozen one plays on with the ordinary presentation Δ. The drawn state of every catch-up frame is the state after that frame's last step, whose live particles equal those of normal playback after the same number of one-tick frames (the 8-bit generation aside, which the seek's `RESET` advanced). While catching up, the preview shows the seek progress (`Zenith2/Docs/Design/VFX/Authoring.md → "Timeline scrubbing"`). Catch-up and freeze exist for preview worlds alone: `VFX.Update` honours `Zenith.VFX.PreviewControl` only in a world whose purpose, recorded at `AttachWorld`, is `WORLD_PURPOSE_PREVIEW`; in a play or edit world the component is ignored, no command carries `CATCH_UP` or `SEEK`, and `vfx.catchup.refused` counts each component once per world. Steps the render side cannot execute in their frame (a deferred or failed frame, or a GPU admission refusal) are kept in order and executed first in the next frame that runs them, never merged ("Deferred and failed frames"); a seek whose kept steps would exceed `uFLUX_PARTICLE_MAX_PENDING_CATCH_UP_STEPS` (64) is interrupted and reported, and the preview session issues it again (`Zenith2/Docs/Design/VFX/Authoring.md → "Timeline scrubbing"`).

`Zenith_WorldAttachContext::GetWorldPurpose()` returns `Zenith_WorldPurpose` (`WORLD_PURPOSE_PLAY`, `WORLD_PURPOSE_EDIT`, `WORLD_PURPOSE_PREVIEW`), set by the world's creator: the runtime and the world host create play worlds, the editor its edit world, EDIT's asset-preview host its preview worlds (`Zenith2/Docs/Architecture/CompositionRoot.md → "Module interfaces"`, [Z2-N-ARCH-879]; cross-doc item X243 (b)).

## Particle random numbers

<!-- elaboration -->
Every random value comes from `Flux_ParticleRandomUnit(seed, ordinal, draw)` with the emitter seed (`Flux_DeriveEmitterSeed(instanceSeed, seedOffset)`), the particle's spawn ordinal and a draw index from this table; the cook writes each value's first draw index into `Flux_ParticleValue::m_uDraw`, consecutive channels taking consecutive indices:

| Draw indices | Consumer |
|---|---|
| `0x00`–`0x03` | shape: u0, u1, u2 (direction and position), u3 (cone volume depth) |
| `0x04`–`0x06` | velocity `randomVector` x, y, z |
| `0x07` | velocity `speed` |
| `0x08` | velocity `radial` |
| `0x09` | lifetime |
| `0x0A`–`0x0B` | size x, y |
| `0x0C`, `0x0D` | rotation angle, rotation rate |
| `0x0E`–`0x11` | colour r, g, b, a (one draw for a gradient's random t) |
| `0x12` | colour intensity |
| `0x13` | sub-UV start frame |
| `0x14` | decal random rotation (per instance: ordinal `0xFFFFFFFE`) |
| `0x15`–`0x1F` | reserved for initialisation modules |
| `0x20`–`0x2F` | over-life random curves, evaluated at draw time with the particle's ordinal |
| `0x1000`–`0x10FF` | a code-authored update type (`m_uRandomBase`) |
| `0x2000`–`0x2FFF` | burst counts (ordinal `0xFFFFFFFF − loop`) |

No draw depends on a slot index, a thread index, a worker or a frame time, so a particle's look is a pure function of (emitter seed, ordinal) and its spawn inputs.

## Particle initialisation

### Initialisation algorithms

For a new particle with ordinal `o`, in this order (`U(d)` is the draw of "Particle random numbers"):

1. Shape (emitter space position `p` and direction `n`), radius thickness `t` (0 surface, 1 volume):

| Shape | Position and direction |
|---|---|
| `point` | `p = 0`, `n = (0, 1, 0)` |
| `sphere` | `z = 1 − 2 U(0)`, `φ = arc × U(1)`, `n = (sqrt(1 − z²) cos φ, z, sqrt(1 − z²) sin φ)`, `p = n × radius × (1 − t + t × cbrt(U(2)))` |
| `hemisphere` | as `sphere` with `z = U(0)` (the +Y half) |
| `box` | `t < 0.5`: face `f = min(floor(U(0) × 6), 5)`, the two in-face coordinates `2 U(1) − 1`, `2 U(2) − 1`, the face coordinate ±1; else every axis `2 U(c) − 1`; `p = extents ×` those coordinates; `n = normalize(p)`, or `(0, 1, 0)` for `p = 0` |
| `cone` | `cos θ = 1 − U(0) × (1 − cos angle)`, `φ = arc × U(1)`, `n = (sin θ cos φ, cos θ, sin θ sin φ)`; base point `(radius sqrt(U(2)) cos φ, 0, radius sqrt(U(2)) sin φ)`; with `emitFrom: volume`, `p` = base + `n × length × U(3)` |
| `circle` | `φ = arc × U(0)`, `ρ = radius × (1 − t + t × sqrt(U(1)))`, `p = (ρ cos φ, 0, ρ sin φ)`, `n = (cos φ, 0, sin φ)` |
| `line` | `p = ((U(0) − 0.5) × length, 0, 0)`, `n = (0, 1, 0)` |

`alignToDirection` sets the initial rotation to `atan2(n.x, n.y)`.
2. Velocity: `v = (direction = shape ? n : normalize(vector)) × speed + randomVector + normalize(p) × radial` (the radial term is 0 for `p = 0`).
3. Lifetime `L`, size, rotation and rotation rate, colour × intensity (rgb), the sub-UV start frame `floor(U(0x13) × frames)`.
4. Space: a world-space emitter maps `p` through the interpolated rows `R = R_prev + (R_cur − R_prev) × f` (row-wise linear interpolation of the two 3×4 transforms, so scale and rotation interpolate with the position) and `v` through `R`'s 3×3 part, then adds `inheritVelocity × emitterVelocity`; a local-space emitter keeps both in emitter space.
5. Pre-ageing by the spawn age `a` (from the frame's spawn record): `p += v × a`, `age = a`; a particle with `a ≥ L` is not written. The born-this-frame flag is set (rate, distance and burst spawns), so this frame's update skips the particle once; prewarm particles were integrated by step 5 of "Prewarm" instead.
6. The slot `o mod C` receives the state with the emitter's current generation in its flags.

## Particle update order

One step, for every live slot (`m_fLifetime > 0`, generation match, age below lifetime), with `dt = m_fDeltaTime`; a disabled module (absent, or disabled by the LOD level) is skipped. CPU and GPU execute the identical sequence:

| # | Step | Arithmetic |
|---|---|---|
| 1 | born this frame | clear the flag and stop (the spawn already accounted for this frame) |
| 2 | age | `age += dt`; `age ≥ L`: raise the death event, set `m_fLifetime = 0`, stop |
| 3 | `gravity` | `v += g × scale × dt`, `g = (0, −9.80665, 0)` world, rotated into emitter space by the inverse of the emitter rotation for a local-space emitter |
| 4 | `force` | `v += acceleration × dt` (emitter-space acceleration rotated into world space for a world-space emitter) |
| 5 | `drag` | `v = v / (1 + coefficient × dt)` |
| 6 | `turbulence` | `v += strength × N(p × frequency + (0, T × scrollSpeed × frequency, 0)) × dt`, `N` the vector value noise below, `T` the effect time |
| 7 | `vortex` | `q = p − centre`; `r = q − dot(q, axis) axis`; `v += (cross(axis, r) × strength − normalize(r) × pull) × dt` (`r = 0`: no change) |
| 8 | `attractor` | `w = position − p`, `δ = |w|`; `δ < killRadius`: death; else, when `radius = 0` or `δ ≤ radius`, `v += w / δ × strength × dt` |
| 9 | `wind` (GPU) | `v += (factor × Env_WindSample(wind, p_world, time) − v) × saturate(factor × dt)` |
| 10 | `speedLimit` | `s = |v|`; `s > max`: `s' = dampen ≥ 1 ? max : max + (s − max) / (1 + 10 × dampen × dt)`, `v = v × s' / s` |
| 11 | integrate | `p_old = p`; `p += v × dt` |
| 12 | `collision` | colliders, then depth ("Collision"); a response may raise a collision event and, with `kill` or `killAfter` reached, a death |
| 13 | `killVolume` | inside (or, with `invert`, outside) the sphere or box in emitter space: death |
| 14 | rotation | `angle = wrap(angle + rate × dt)` into [−π, π] |
| 15 | `trail` | "Particle trails" |
| 16 | `precipitationOcclusion` (GPU) | "Collision", subsection "Precipitation occlusion" |
| 17 | `custom` (GPU) | `state = Flux_ActiveParticleUpdate::Update(…)`; with `bKILLS`, a zero lifetime is a death |
| 18 | write | the state back to its slot |

A death at any step raises the death event (when an edge exists), sets `m_fLifetime = 0` and ends the step. The vector value noise `N(x)` returns three channels, each a sum over `octaves` of `0.5^i × V_c(x × 2^i)`, where `V_c` is trilinear value noise over the integer lattice with smoothstep weights `w = f × f × (3 − 2f)` and lattice values `(Flux_ParticleHash((ix × 73856093) ^ (iy × 19349663) ^ (iz × 83492791) ^ seed_c) >> 8) × 2⁻²³ − 1`, with `seed_c` = `0x68E31DA4`, `0xB5297A4D`, `0x1B56C4E9` for x, y, z (32-bit wrapping integer arithmetic on both sides).

## Collision

### Analytic colliders

`VFX.Update` gathers each world's enabled `Zenith.VFX.Collider` entities (at most `uFLUX_PARTICLE_MAX_COLLIDERS_PER_WORLD` = 64, in ascending simulation ID; the rest counted in `vfx.colliders.dropped`) with their interpolated world transforms; the CPU simulator reads them directly and `VFX.Extract` sends them to the renderer as `Flux_RenderParticleCollider` records.

`Flux_RenderWorldSection` has `Zenith_Span<const Flux_RenderParticleCollider> m_axParticleColliders` and `Flux_RenderWorldWriter` has `Zenith_Result<void, Flux_RenderWorldError> AddParticleCollider(const Flux_RenderParticleCollider& xCollider)`, the element type declared by VFX in `Flux_Particles.h` (64 bytes: the world-to-collider rows, half extents, radius, shape, collision mask) and copied into the frame's `Flux_ParticleCollider` array by `Flux_ParticleBuildFrame` (`Zenith2/Docs/Rendering.md → "The render world hand-off"`, cross-doc item X240).

A particle collides with the first collider, in array order, whose mask intersects the module's `mask` and that it penetrates, with particle radius `ρ = 0.5 × size.x × radiusScale`, at most once per step:

| Shape | Signed distance `δ` and normal `n` (collider space; mapped back to world) |
|---|---|
| plane | `δ = q.y`, `n = (0, 1, 0)` |
| sphere | `δ = |q| − R`, `n = q / |q|` (`(0, 1, 0)` when `|q| < 10⁻⁶`) |
| box | outside: the closest point `c = clamp(q, −h, h)`, `δ = |q − c|`, `n = (q − c) / δ`; inside: `δ = −min_i(h_i − |q_i|)`, `n` the axis of that minimum with `q`'s sign |
| capsule | the closest point `c = (0, clamp(q.y, −h.y, h.y), 0)`, `δ = |q − c| − R`, `n = (q − c) / |q − c|` |

When `δ < ρ` and `dot(v, n) < 0`: `p += n × (ρ − δ)`; `v_n = dot(v, n) n`, `v_t = v − v_n`; `v = v_t × (1 − friction) − v_n × restitution`; the collision count rises (saturating at 3), the collision event fires while the count is at most the edge's `maxCollisions`, and `kill` or `killAfter` ends the particle. Local-space particles collide in world space through the emitter transform.

### Depth-buffer collision

GPU, world-space emitters, `depth` true, only for emitters of the sort view's world (another world's surfaces are not in its depth), and only while the frame has a sort view with a previous HiZ pyramid (`Flux_ViewContext::m_xPrevHiZ` of the main view, `Zenith2/Docs/Rendering.md → "Render views"`); otherwise the permutation without depth collision runs and `Flux.Particles.DepthCollisionUnavailable` counts the emitters.

1. Project `p` with the previous frame's view-projection of the sort view (`m_xPrevViewProjection`); outside the viewport or behind the camera: no collision.
2. Read the pyramid's max channel at mip 0 at the projected texel: the scene's linear depth `z_s` (rounded towards farther, [Z2-N-REND-631]).
3. The particle's linear depth `z_p` and `z_old` (of `p_old`) in the previous view. Collide when `z_p ≥ z_s`, `z_p ≤ z_s + depthThickness` and `z_old < z_s`.
4. The surface point and normal: reconstruct the view-space positions of the texel and its +x and +y neighbours from their linear depths and the inverse projection; `n = normalize(cross(Px − P, Py − P))`, oriented towards the camera, in world space.
5. Respond as against a plane through the surface point with normal `n` (the analytic rule above).

The test sees only surfaces visible in the sort view one frame ago; off-screen or occluded surfaces do not collide, which is the defined behaviour.

### Precipitation occlusion

With the Environment module's Stage 5 precipitation occlusion capture available, `precipitationOcclusion` kills a particle whose world position lies more than 0.05 m below the capture's occluder height at its (x, z); without it the module is inert and `Flux.Particles.OcclusionUnavailable` counts the emitters.

<!-- assumption: pending ENV (cross-area row VX6) -->
ENV exposes its top-down occlusion capture to particle passes as `Flux_FrameShared::m_xPrecipitationOcclusion` (an `[Optional]` graph texture holding occluder height or depth) and a mapping `float4 m_xPrecipitationOcclusionMapping` (origin x, origin z, 1 / extent, the depth-to-height scale; extent 0 when absent) in `Env_FrameEnvironment`, which the `ParticleSimulate` table reads with the frame environment field.
<!-- /assumption -->

## Particle trails

An emitter with the `trail` update module keeps `T` samples per slot (2 … 8) in the pool's trail region at `m_uTrailBase + slot × T + i`, with the head index in flag bits 13–15:

- At spawn every sample is the spawn position, the head 0.
- Each step, when `floor(age / interval)` exceeds its value before the step, the head advances `head = (head + 1) mod T` and the sample at the head becomes the particle's current position, width and alpha scale 1.
- Drawing (`FLUX_PARTICLE_RENDER_TRAIL`) connects the current position, then the samples from the head backwards, into a strip of `T` segments whose width and alpha come from the render module's `width` and `alpha` curves at `i / T`.
- A reset or a new particle in the slot rewrites every sample, so a strip never connects two particles.

Emitter trails (a weapon swing, a missile's smoke) are distance-spawning ribbon emitters ("Distance spawning", `FLUX_PARTICLE_RENDER_RIBBON` in `Zenith2/Docs/Design/VFX/Rendering.md → "Emitter ribbons"`); a world-space ribbon emitter that stops leaves its trail behind to fade.

## LOD and significance

<!-- elaboration -->
The viewer set, the screen-size measure, the hysteresis and the significance product below are VFX's LOD model; the plan names VFX LOD (Z2-I-VFX-09) without choosing one.

### Viewers

Each frame `VFX.Update` builds the world's viewer list: the world's main camera (`Flux_SelectMainCamera` over `Zenith.Render.Camera` with each camera entity's simulation ID, `Zenith2/Docs/Design/RPROD/Extractors.md → "Flux_RenderComponents header"`) at its interpolated world transform with its vertical field of view, then every enabled `Zenith.VFX.Viewer` entity in ascending simulation ID, at most 8 viewers in total (the rest counted in `vfx.viewers.dropped`). A world with no viewer (a headless test world) treats every instance as screen size 1, LOD level 0.

### Screen size and LOD selection

For an instance with world bounds `B` (the CPU-side conservative bounds, "Bounds" below), centre `c`, radius `r = |B.max − B.min| / 2`:

1. Per viewer v at position `x_v` with half-angle tangent `τ_v = tan(fovY_v / 2)`: `s_v = min(r / (max(|c − x_v|, r + 0.01) × τ_v), 10)`; `s = max_v s_v × 2^(−vfx.lodBias)`; the nearest distance `δ = min_v max(|c − x_v| − r, 0)`.
2. Culled when `s < cullScreenSize × (1 − h)`, or `maxDistance > 0` and `δ > maxDistance`; a culled instance is admitted again when `s ≥ cullScreenSize × (1 + h)` and `δ ≤ 0.95 × maxDistance` (h the effect's hysteresis).
3. Level: the first level L with `s ≥ screenSize_L`, else the last level. From the current level `L_c`: a finer level (`L < L_c`) is taken when `s ≥ screenSize_L × (1 + h)`; a coarser one (`L > L_c`) when `s < screenSize_{L_c} × (1 − h)`; otherwise `L_c` stays.
4. The level's spawn scale multiplies every spawn count; its disabled-module masks clear bits of each emitter's module mask for the frame.
5. Significance = `s × priority` (0 when culled).

### Culling behaviour

| Cull behaviour | While culled (LOD or budget) | On readmission |
|---|---|---|
| `kill` | no simulation, no spawns, no draw; the CPU emitter's ring is cleared at once, the GPU emitter's generation advances when it next simulates | `RESET`: the generation advances, accumulators restart, and a prewarmed emitter prewarms again |
| `pause` | no simulation, no spawns; drawn as it is when only the budget refused it, not drawn when LOD culled it | resumes from the frozen state; the accumulators were not advanced |

### Bounds

| Bounds source | Rule |
|---|---|
| fixed or baked local bounds | the effect's cooked bounds (`Zenith2/Docs/Design/VFX/Authoring.md → "Bounds baking"`), per emitter |
| a local-space emitter | `T(B_e) ⊕ W`: the emitter-frame bounds `B_e` transformed by the current emitter transform `T` (the 8 corners, then the AABB), Minkowski-added to `W`, the cube of half extent `s_max × max|corner of m_xWorldDisplacement|` (`s_max` the transform's largest axis scale), because the emitter-space gravity of a rotating local emitter can point anywhere; a local-space event target uses `T(B_root ⊕ S_t) ⊕ W_t` with its composed displacement (a local-space edge's source and target share their space, `Zenith2/Docs/Design/VFX/EffectFormat.md → "Event edges"`) |
| a world-space emitter that is no event target | `(∪_b T_b(B_e)) ⊕ D ⊕ I`: the union of `B_e` transformed by the emitter transforms of the buckets covering the last `H_e` seconds of effect time (`VFX.Update` keeps per emitter a ring of nine buckets of `H_e / 8` seconds of effect time, the current bucket plus the eight full buckets before it, each the union of the transformed bounds of the frames it covers, a bucket dropped once its end is older than `τ − H_e`, so the kept buckets always cover at least the last `H_e` seconds; `H_e` = `m_fBoundsHistorySeconds`, the emitter's largest lifetime for an emitter that is no event target, so particles left behind by a moving emitter, and every spawn point of a distance-spawning emitter's path, stay inside); `D` the cooked world displacement box `m_xWorldDisplacement` in world axes (gravity, world-space force, an emitter-space force on a world-space emitter as its rotation-invariant cube, and world-space turbulence never rotate with the emitter), replaced by its rotation-invariant cube when `m_uBoundsFlags` bit 0 is set; `I` the inherited-velocity box `±J_e × v_max` on every axis, `J_e` = `m_fInheritVelocityGainSeconds` (`factor × L_max`), `v_max` the largest emitter speed recorded in those buckets |
| an event target `t` (`m_uBoundsFlags` bit 1) | `(∪_b T_b(B_root)) ⊕ (∪_b R_b(S_t)) ⊕ D_t ⊕ I_t` over the buckets covering the target's history `H_t`: `B_root` the `B_e` of its chain's root emitter (`m_uBoundsRootEmitter`, the first ancestor that is no event target), transformed by each bucket's transforms (the positions every ancestor particle could have died at); `S_t` the target's spawn-relative box (its record's local bounds: the displacement of a child from its spawn point over its life, plus every intermediate ancestor's, Minkowski-summed by the cook) mapped by each bucket's linear part `R_b` (rotation and scale, no translation; the union over the bucket's frames); `D_t` the cooked composed world displacement (the chain's own world terms plus the world velocity each child inherits from its source, "Bounds baking" of `Zenith2/Docs/Design/VFX/Authoring.md`); `I_t = ±J_t × v_max`, `J_t` the composed inherited-velocity gain and `v_max` over the same buckets. `H_t = L_t + fFLUX_PARTICLE_MAX_STEP_SECONDS + H_source` covers the whole time since the oldest ancestor spawn a live child descends from, which the source's own history `H_source` does not |
| wind (GPU emitters with a `wind` module, or descending from one) | not in `m_xWorldBounds`: the render side adds it per view ("Per-view emitter culling" of `Zenith2/Docs/Design/VFX/Rendering.md`): the cube of half extent `G_e × W_e`, `G_e` = `m_fWindGain` (metres per m/s of wind), `W_e` the largest wind-speed bound of the emitter's world over the frames of its last `H_e` seconds of effect time (the render side's ring of nine buckets of `H_e / 8`, kept as above, from the world's render environment, `Flux_ParticleWindBound`) |
| the instance | the union of its emitters' bounds, sent in every command's `m_xWorldBounds`; the screen size and LOD selection of "Screen size and LOD selection" read it without the wind term (a quality heuristic), and culling reads it with the wind term (a containment claim) |

<!-- elaboration -->
The emitter-frame part `B_e` and the world terms are separated because only `B_e` rotates and translates with the emitter: a sparks emitter tilted 90° keeps its gravity fall along world −Y, a distance-spawned trail baked stationary still covers its path through the bucket union, and particles thrown ahead of a moving emitter by `inheritVelocity` stay inside `I`. An event target's children spawn where their sources actually were (displaced by the sources' world terms, and anywhere along the emitter's path over the chain's history), inherit a velocity that contains the sources' world and inherited parts, and fall and drift by their own, which is why a target composes from its chain root and the composed cooked terms rather than from its own baked spawn points.

For the linear class, containment of the world, inherited and wind parts and of the bucket unions is a proof, not a hope; the emitter-frame part `B_e` or `S_t` is the cook's measurement at 1/60 s steps ("Bounds baking" of `Zenith2/Docs/Design/VFX/Authoring.md`), and at run-time steps up to `fFLUX_PARTICLE_MAX_STEP_SECONDS` it rests on `margin` and the development-build validation ("GPU bounds validation"), because an emitter-frame velocity that changes during a particle's life (drag, curves, an emitter-space force of a local-space emitter) integrates differently at a larger step. The linear class: every update module of the emitter and of its ancestors is one of `gravity`, `force`, `drag`, `wind`, `turbulence` of a world-space emitter, `killVolume`, `trail`, `precipitationOcclusion`, or a kill. Each of these either adds a world-axis acceleration inside the cooked range (whose displacement under steps up to `fFLUX_PARTICLE_MAX_STEP_SECONDS` the cooked `D_own = ½ a L (L + Δ_max)` bounds), scales the whole velocity by a factor in [0, 1] (`drag`, and `wind`'s relaxation, which the bake runs against zero wind), or only removes particles. The velocity is therefore the sum of the bake's emitter-frame part (in `B_e` or `S_t`), the world part (in `D`), the inherited part (in `I`) and the wind part (in the render side's cube), each evolving independently (linear superposition), and every displacement box contains 0, so a [0, 1] scaling of a velocity component keeps its displacement inside its box. The estimate class is every other emitter that combines a world term (a non-empty `D`, a non-zero `G_e` or `J_e`, or a target's inherited source velocity) with `speedLimit`, `vortex`, `attractor`, or `turbulence` of a local-space emitter (all four depend on the full velocity or the displaced position, so the parts no longer superpose), and every emitter with `collision` (the bake runs without colliders and a bounce reflects the frame part). For it the box is an estimate that rests on `margin` and the development-build validation ("GPU bounds validation"); the cook sets `m_uBoundsFlags` bit 2 and reports `VFX_COOK_WARNING_BOUNDS_ESTIMATE` naming the emitter and the module (`Zenith2/Docs/Design/VFX/Authoring.md → "Bounds baking"`). `Test_VFXBounds_AttachedAndWorldSpace` holds the linear class against the reference simulation with a moving, rotating track, its gravity fixtures at fixed steps of 1/15 s as well as 1/60 s.

### GPU bounds validation

With `vfx.debug.validateBounds` (development builds), each simulated GPU emitter's update also accumulates its live particles' world AABB with 32-bit atomic min and max on order-preserving float encodings and its live count with an atomic add (integer results, order-independent) into its `Flux_ParticleValidationRecord`, which one `AddReadback` per frame copies into the readback pool (`Zenith2/Docs/Design/RHI/Memory.md → "Readback pool"`). The readback is READY when the frame's batch completes, observed at the next `PrepareNextFrame`, so the data is at least one and at most F frames old (F the frames in flight, `Zenith2/Docs/Rendering/GpuResources.md → "Readbacks"`); a full pool or a deferred frame fails it and the next frame requests again (BR-DEFER). When it resolves, an emitter whose measured bounds leave its CPU bounds of the measured frame (with that frame's wind cube) raises `Flux.Particles.BoundsExceeded`, and its instance stats record both boxes, the measured live count and the frame it describes. Nothing but diagnostics reads the result.

## Budgets and admission

### CPU admission per world

`VFX.Update` sorts the CPU emitters of non-culled instances by (significance descending, instance key ascending, emitter index ascending), groups each connected event graph into one unit as GPU admission does, and admits units while the admitted capacities sum to at most `vfx.cpuParticlesPerWorld` and their count is at most `vfx.cpuEmittersPerWorld`; the others are budget-culled with their cull behaviour (`vfx.cpu.budget_culled` + 1 each). Admission uses capacity, not live count, so the per-frame CPU cost has a known ceiling.

### GPU admission per frame

`Flux_ParticleBuildFrame` sorts every `ACTIVE` GPU emitter of every world by (significance descending, world tag ascending, instance key ascending, emitter index ascending), groups the emitters of each connected event graph of an instance (its sources and targets) into one unit (the cook places sources before targets, so the unit sorts at its first emitter's position), and admits units while the admitted work sums to at most `m_uGpuActiveCapacity` and the emitter count is at most `m_uGpuEmittersPerFrame`; a refused unit is budget-culled for the frame as a whole (`Flux.Particles.BudgetCulled` per emitter). An emitter's work per step is its capacity `C` (simulate threads; an emission batch is at most `C` threads more and is bounded by the same capacity) plus, for an event target, `C_src × E_edge × k` event-consumer threads ("Event spawning"); its work in the frame is the sum over its steps (a seeking emitter's catch-up steps and any steps kept from an earlier frame, "Deferred and failed frames"). Units that carry catch-up steps (preview worlds alone) are ranked ahead of every other unit, in (world tag, instance key) order, so a scrub is not starved by the edit world; a refused catch-up unit keeps its steps pending instead of taking its cull behaviour. So the frame's GPU simulation threads are exactly the admitted work, the quantity `Z2-B-VFX-003` Particle simulation GPU is calibrated against: a 4,096-slot source with a collision edge of three collision events feeding target A and a death edge feeding target B, 16 children per event each, costs `4,096 + 4,096 × 3 × 16 + 4,096 × 1 × 16 = 266,240` units before the targets' own capacities and is refused under the S24's 262,144 rather than silently exceeding it. Work scales with capacity, not live count: a dead slot still costs its simulate thread (a load and an early exit) and its draw instance (a degenerate primitive), the cost recorded in V2 of `Zenith2/Docs/Design/VFX.md → "VFX rulings"`.

### Sorted-particle admission

Of the admitted, visible, depth-sorted emitters, ranked back to front in the sort view (`Zenith2/Docs/Design/VFX/Rendering.md → "Emitter ranks and the sort view"`), the first `m_uSortedEmittersPerFrame` (at most 1,024) whose capacities sum to at most `m_uSortedParticlesPerFrame`, taken in descending significance, are depth-sorted; the rest draw oldest-first and count `Flux.Particles.SortRefused`.

## CPU simulation

### CPU emitter storage

Each CPU emitter of a live instance owns, from the module's general heap, one block holding: the state ring (`C × 64` bytes of `Flux_ParticleState`), the trail ring (`C × T × 16`), two event rings when it is a source (`2 × C × E × 48`, by step parity) and its live-index scratch (`C × 4`). The block is allocated when the emitter first spawns (a failure leaves the emitter inactive and counts `vfx.cpu.alloc_failed`) and freed when the emitter reaches DONE. `vfx.cpuParticlesPerWorld` bounds the sum of admitted capacities, and the module's memory consumer reports the allocated bytes under `MEMORY_SUBSYSTEM_RENDERING` within `Z2-B-VFX-001`.

### CPU update algorithm

`VFX.Update`, after scheduling, calls `ParallelFor(cpuInstanceCount, 1, &Zenith_VFXCpuInstanceLeaf, pxFrame)` over the instances with admitted CPU emitters, in admission order of their first emitter. Each leaf processes one instance entirely: for each of the frame's steps (one, or the catch-up steps of a preview seek) and, within a step, for each admitted CPU emitter in emitter-table order (sources before targets): event spawns from the source ring half of the previous step, the step's rate, distance and burst spawns in ordinal order ("Particle initialisation"), the update of every slot in ascending slot order ("Particle update order") writing the current step's ring half; then each emitter's live-index list. Leaves share no writable state, so the result is independent of the worker count and of which worker ran a leaf. Cost: O(C) per emitter per frame; the node's zone is `VFX.Update@W<tag>`, its CPU time the row `Z2-B-VFX-002` CPU particle update.

### CPU particle output

After the update, each admitted CPU emitter's live particles are copied, in spawn-ordinal order (oldest first), into the world's output array (the module's per-world arena, double-buffered by frame parity, `vfx.cpuParticlesPerWorld × 64` bytes per half); `VFX.Extract` hands the emitter's range to `AddEmitterCommand` as `m_axCpuParticles`, which the writer copies into the render world's slot arena. On the render side the CPU particles of every world are written into the frame chunk (`Flux_ParticleWriteFrameData`) and drawn by the same draw programs as GPU particles, through draw records flagged as CPU sources (`Zenith2/Docs/Design/VFX/Rendering.md → "Draw records and indirect commands"`). In a headless configuration (`Zenith_ModuleStartContext::IsHeadless()`) the CPU update and the output copy are skipped: scheduling, lifecycle and the command stream still run, and the commands carry empty spans.

Everything `VFX.Extract` writes is render-world slot-arena demand, which [Z2-N-REND-017] budgets within `Z2-B-CORE-005`; the writer copies `m_axCpuParticles` into the partition's slot arena and re-points it (the record rule of `Zenith2/Docs/Rendering.md → "Render-world record types"`, cross-doc item X240 (4)). Per world per frame, at most:

| Record | Bytes | Desktop default | S24 default |
|---|---|---|---|
| CPU particles | `vfx.cpuParticlesPerWorld × 64` | 1 MiB | 256 KiB |
| emitter commands | `256 ×` the emitters of at most `vfx.maxRenderInstances` instances (`VFX.Extract` writes the most significant instances first and stops there, counting the rest in `vfx.extract.dropped`, because the render side's table holds no more), ≤ 16 emitters each | ≤ 16 MiB worst case; ≈ 230 KiB for W-OpenWorld's ≈ 300 instances of 3 emitters | ≤ 4 MiB worst case |
| colliders | `64 × 64` | 4 KiB | 4 KiB |
| effect lights, effect decals | `vfx.maxEffectLights` LIGHT delta records, `vfx.maxEffectDecals` GEOM decal records | 64, 256 records | 16, 64 records |

This table is VFX's per-world demand line of "CPU particle output": it is an input of the CORE Stage 1 budget ticket's `Z2-B-CORE-005` allocation, which runs before VFX's ([Z2-N-BUDGET-472], cross-doc item X263, makes every render-world writer's stated demand that ticket's input), and the VFX Stage 1 budget ticket confirms it against the allocated value in its own diff (`Zenith2/Docs/Design/VFX.md → "Stage 1"`). `Test_VFXExtract_SlotArenaDemand` holds the caps.

## GPU simulation

### Particle pool layout

The particle pool is one pool of `Flux_GPUScene::CreatePool` (`Zenith2/Docs/Design/RHI/Memory.md → "Pool allocation"`), created by `EnableParticles`, persistent-imported every frame as `Flux_FrameShared::m_xParticlePool` ([Z2-N-REND-551]), not double-buffered. VFX owns its policy:

| Region | Granule | Alignment | Initial granules | Maximum granules | Holds |
|---|---|---|---|---|---|
| 0 state | 64 bytes (one `Flux_ParticleState`) | 256 | `vfx.pool.initialParticles` | `vfx.pool.maxParticles` | each GPU emitter's `C` slots |
| 1 trail | 16 bytes (one `Flux_ParticleTrailSample`) | 256 | `vfx.pool.initialParticles / 4` | `vfx.pool.maxTrailSamples` | `C × T` samples per trail emitter |
| 2 events | 48 bytes (one `Flux_ParticleEventRecord`) | 256 | 16,384 | `vfx.pool.maxEventRecords` | `C × E` records per event source |

The maximums are the configuration keys whose defaults are generated from `Z2-B-VFX-001` Particle pools and CPU particle data (`vfx.pool.maxParticles × 64 + vfx.pool.maxTrailSamples × 16 + vfx.pool.maxEventRecords × 48` plus the CPU share is the row's value); a region grows by RHI's resize-and-copy and refuses at its maximum with `FLUX_GPU_SCENE_ERROR_TABLE_FULL` ([Z2-N-REND-553]). Passes address a region through the plain-data byte offset `GetPoolRegionOffsetBytes(region)` added to element indices.

### Emitter pool ranges

| Event | Action |
|---|---|
| the first `UPDATE` of an unknown (world, instance) key | resolve the effect (`Zenith_AssetRegistry::Acquire` by persistent ID, then `Resolve`; an effect not ACTIVE yet waits, the instance drawing nothing); create the instance record; acquire the effect's constants buffer (created on its first instance through `Flux_PersistentResources::CreateBuffer` from `GetConstantsImage()`, reference counted per effect, released through deferred deletion with its last instance) |
| a GPU emitter's first `ACTIVE` command | `AllocatePoolRange` in region 0 (C), region 1 (C × T) when it has a trail, region 2 (C × E) when it is an event source; a `TABLE_FULL` leaves the emitter unallocated (not simulated, not drawn) and is retried at each subsequent `ACTIVE` command (`Flux.Particles.PoolFull`) |
| a freshly allocated range | cleared to zero by an `AddClearBuffer` over the range before the frame's emission, so stale contents of a reused range never read as live (`m_fLifetime` 0) |
| `RESET` | the emitter's 8-bit generation advances; when it wraps from 255 to 0 the state range, its trail range and its event ring are cleared as for a fresh range, so no particle or record of generation 0 from 256 resets ago reads as live (`Test_ParticleSystem_GenerationWrapClearsRange`) |
| destruction (`DESTROY`, or a released emitter after its linger time) | `FreePoolRange` of each range (retired through the pending store like table slots); the instance releases its effect reference when its last emitter goes |
| a world with no command for an instance for 2 consecutive submitted frames | the instance is destroyed (`Flux.Particles.InstancesTimedOut`), the guard against a world that vanished without its detach; a deferred frame does not count (`Test_ParticleSystem_InstanceTimeout`) |

### Frame building

`Flux_ParticleBuildFrame` (step 7c of `Flux.BuildGraph`, after the view list):

1. Adopt pending budgets ("Runtime configuration changes" of `Zenith2/Docs/Design/VFX.md`), then apply admission ("GPU admission per frame") to the instance table updated by step 4.
2. Number each admitted emitter's steps in arrival order: the emitter's kept steps of earlier frames first ("Deferred and failed frames"), then its commands of this frame in the order the render world holds them, so the k-th is step k − 1 and its last step is its drawn command; a non-seeking emitter has one step, step 0, and a seeking emitter's unflagged command is its last step. The frame's step count `S_frame` is the largest emitter step count, at most `uFLUX_PARTICLE_MAX_STEPS_PER_FRAME` (128: 64 kept plus 64 of the frame).
3. For each step s in ascending order and each emitter with a step s, build its `Flux_EmitterCommand` (render-world values, pool offsets, constants location, collider range, its step tag `m_uStepIndex` = the emitter's counter, which then advances by one, and for an event target the consumed source tag of "Event spawning": the source's step s − 1 tag when s ≥ 1, built just before); the counters' values before the frame and each target's last consumed tag are kept for `Flux_ParticleEndFrame(false)`. Every step but an emitter's last carries the catch-up meaning (simulated, never drawn), whether its command was flagged or was the unflagged last step of a kept frame. GPU bounds validation (flag bit 5) is set on an emitter's last step alone, so the validation record describes the drawn state.
4. Order commands by (step, simulate permutation, world tag, instance key, emitter index), so each step's commands form one contiguous range; group them into batches per (step, permutation), each batch carrying its `m_uStep`: emission batches over the step's emitters with spawns, event batches over its event-target emitters, simulate batches over every emitter with that step; sort-key batches over the depth-sorted emitters' last steps only. Record, per step, its command range and its first batch and batch count of each kind in `Flux_ParticleFrame::m_axSteps` (`Zenith2/Docs/Design/VFX/PublicAPI.md → "Flux_ParticleFrame"`).
5. Prefix the group counts within each batch: emission groups `ceil(min(spawns, C) / 64)`, event groups `ceil(C_src × E_edge × k / 64)` (a target step whose consumed tag is `0xFFFFFFFF` gets none), simulate and sort-key groups `C / 64`; write `m_uEmitGroupBase` and `m_uSimulateGroupBase`.
6. Choose the sort view and rank the depth-sorted emitters ("Sorted-particle admission"); assign each its sorted segment `[S_e, S_e + C_e)` in rank order; `m_uSortedKeyCount = Σ C_e`.
7. Per view drawing particles, cull and order the emitters and build the draw records, indirect commands and runs from each emitter's last step (`Zenith2/Docs/Design/VFX/Rendering.md → "Draw records and indirect commands"`).
8. Declare the frame data: commands (`256 ×` count, every step's), CPU particles (`64 ×` count), colliders (`64 ×` count), and per view the commands and records; keep the tickets in the returned `Flux_ParticleFrame`.

`Flux_AddParticleSimulation` then declares, before step 0, the range clears (fresh ranges, generation-wrap clears, the validation buffer and the global histogram), then for each step s in 0 … `S_frame` − 1 in ascending order: `ParticleEmit` over the step's emission and event batches (when it has any), then `ParticleSimulate` over its simulate batches; step 0's passes are named `ParticleEmit` and `ParticleSimulate`, and step s ≥ 1's `ParticleEmitStep<s>` and `ParticleSimulateStep<s>` (decimal `s`; unique names in the graph, summed by the profiler's `ParticleEmit*` and `ParticleSimulate*` patterns and charged to `Z2-B-VFX-003`). After the last step, `ParticleCompactAndSortKeys` and the four `ParticleSort` passes run when `m_uSortedKeyCount > 0`. The graph orders each step's passes after the previous step's through their read-write access to the pool, so step s + 1's emission reads the event records step s's simulation wrote, and the draw passes read the state after every emitter's last step. A frame with no admitted GPU emitter declares no particle pass ([Z2-N-REND-281]).

### ParticleEmit dispatch

`Dispatch(groups, 1, 1)` per batch, 64 threads per group. Thread `t` of group `g`:

1. Find the batch command with the largest `m_uEmitGroupBase ≤ g` (binary search, "Group mapping"); `i = (g − base) × 64 + lane`.
2. Rate, distance and burst batches: `i < min(n + n_b, C)` proceeds; spawn index `j = (n + n_b − min(n + n_b, C)) + i` (only the last C spawns); ordinal `o = m_uSpawnOrdinalBase + j`; age `j < n ? first − j × step : m_fBurstAgeOffset`; initialise ("Particle initialisation") and write slot `o mod C` (with `PREWARM`, run the prewarm integration first).
3. Event batches: decode (record, child) from `i`; apply the freshness and generation checks of "Event spawning"; initialise the child and write its slot.

Complexity O(new particles + event-ring records × children) per frame; no atomic operation; every slot written by at most one thread.

### ParticleSimulate dispatch

`Dispatch(groups, 1, 1)` per batch, groups never straddling emitters (capacities are multiples of 64). Thread: find the command by `m_uSimulateGroupBase`; slot `s = (g − base) × 64 + lane`; load the state; if live, run "Particle update order" with the command's module mask (uniform across the group, so module branches never diverge within a group); write events, trail samples and the state. Complexity O(Σ admitted capacity) per frame; cost per slot is fixed by the module mask, which is what makes the GPU row calibratable by capacity. The only atomics are the development-build bounds validation.

### ParticleCompactAndSortKeys dispatch

Over the depth-sorted emitters, one thread per slot (the same group mapping). For slot `s` of emitter e with rank R_e and segment base S_e:

- live: `z` = the particle's linear view depth in the sort view (`max(z, 0)`); `d22 = asuint(z) >> 9` (the 22 high bits of a non-negative IEEE float, monotonic in z); key = `(R_e << 22) | (0x3FFFFF − d22)`;
- dead: key = `(R_e << 22)`;
- write `Flux_ParticleSortPair{ key, stateIndex }` at `S_e + s`; CPU-target depth-sorted emitters write their live particles from the frame chunk with payload bit 31 set and pad their segment with dead keys;
- accumulate the four 8-bit digit histograms (4 × 256 counters) through a groupshared histogram per group and one atomic add per non-zero bin into the global histogram, cleared by `AddClearBuffer` before the pass (integer sums: order-independent).

Ascending keys therefore order emitters from farthest (rank 0) to nearest and, within an emitter, dead slots first (their key is the segment's smallest) and then live particles from farthest to nearest; every emitter's sorted entries occupy exactly its segment, so its draw needs no GPU-written count.

### ParticleSort algorithm

A stable LSD radix sort of `N = m_uSortedKeyCount` pairs by their 32-bit key, four passes of 8-bit digits (`uPERM_PASS` 0 … 3, digit `(key >> 8p) & 0xFF`), each a single-pass partitioned scatter with decoupled look-back over partitions of `uFLUX_PARTICLE_SORT_PARTITION_KEYS` = 2,048 pairs (256 threads × 8):

1. A group takes its partition index from an atomic ticket counter (cleared before the pass), so a partition waits only on partitions whose groups already started; the partition covers pairs `[P × 2048, P × 2048 + 2048)`.
2. Local ranking: eight stable one-bit splits of the partition's pairs in groupshared memory, each split a block-wide exclusive prefix sum of the bit's complement (a per-thread sequential scan of its 8 pairs, a subgroup prefix sum across lanes, and a groupshared scan across subgroups, valid for every subgroup size from 4 to 128); the result is the partition sorted stably by digit, with the partition's digit counts.
3. Look-back: publish each digit's local count with flag AGGREGATE in the partition status array (`numPartitions × 256` words, cleared before the pass); thread d walks back from partition P − 1, summing AGGREGATE counts until it meets an INCLUSIVE value, then publishes INCLUSIVE = that sum + the local count.
4. Every group computes the exclusive scan of the pass's 256-bin global histogram (from `ParticleCompactAndSortKeys`) in groupshared memory.
5. Scatter: a pair at local sorted position `i` with digit d goes to `global[d] + partitionPrefix[d] + (i − localStart[d])`.

The output is a pure function of the input: positions are exact integer sums, independent of ticket order and of the subgroup size. Each pass reads and writes 8N bytes plus the status array; four passes ping-pong between `m_xKeysIn` and `m_xKeysOut`, so the sorted pairs end in the buffer the first pass read, which `Flux_FrameShared::m_xParticleSorted` names. Complexity O(N) per pass.

### Reduce-then-scan fallback

The look-back of "ParticleSort algorithm" waits on partitions of other workgroups, and neither Vulkan nor D3D12 guarantees inter-workgroup forward progress; a GPU that schedules a waiting group ahead of the group it waits on hangs. `vfx.sort.algorithm` (`Flux_ParticleSortAlgorithm`, default `onesweep`) selects the fallback `reduceThenScan`, which never waits across groups and produces the same bytes. Per digit pass, three dispatches of `RadixSort.slang` (`uPERM_STAGE` 1 … 3; the single-pass algorithm is stage 0):

1. Reduce: one group per partition of 2,048 pairs counts the partition's 256 digits into `partitionCounts[P × 256 + d]` (groupshared histogram, plain stores).
2. Scan: one group of 256 threads, thread d computing the exclusive prefix of `partitionCounts[· × 256 + d]` over the partitions in ascending order, in place (integer sums).
3. Scatter: one group per partition re-ranks its pairs stably (step 2 of "ParticleSort algorithm") and writes each pair to `global[d] + partitionPrefix[d] + (i − localStart[d])`, as step 5 does.

The output equals the single-pass sort's bit for bit (`Test_ParticleSort_ReduceThenScanMatchesOnesweep`), costs two extra dispatches and one extra read of the pairs per pass, and keeps the `ParticleSort` pass name and budget row. Which one the S24 ships is decided by the Stage 5 device run (slice 18 of `Zenith2/Docs/Design/VFX.md → "Stage 5 slices"`), whose sort stress runs `2²⁰` keys for 1,000 frames with `onesweep`: a hang or a device loss sets the S24 tier files' `vfx.sort.algorithm` to `reduceThenScan` in that ticket's diff.

### Group mapping

`Flux_ParticleFindCommand(uGroup, uFirstCommand, uCount, uField)` in `Zenith2/Shaders/Particles/ParticleFrame.slang`: a binary search over the batch's commands for the last command whose group base (emission or simulate field) is ≤ `uGroup`, at most 12 iterations for 4,096 commands; the CPU writes bases in ascending order, so the search is total.

## Determinism rules

| Rule | Mechanism |
|---|---|
| simulation never depends on particles | "Presentation-only particles"; VFX.Lifecycle reads components and the tick alone |
| the command stream is a pure function of the world's simulation state and its presentation-time sequence | `VFX.Update` iterates instances in (instance key, emitter index) order, CPU emitters in a `ParallelFor` whose leaf partition is worker-independent ([Z2-N-ARCH-395]), trigger records in sequence order |
| a particle's initial state is a pure function of (emitter seed, ordinal, spawn record) | "Particle random numbers" |
| the GPU state after a frame is a pure function of the previous state and the frame's commands | ring slots by ordinal, at most one writer per slot, no atomics in state paths, groups never straddle emitters |
| the sort is deterministic and stable | "ParticleSort algorithm" |
| draw order is deterministic | the partitions, ranks and per-emitter orders of `Zenith2/Docs/Design/VFX/Rendering.md → "Draw ordering"`, with ties broken by (world tag, instance key, emitter index) |
| async compute on or off, aliasing on or off, any frames-in-flight count | the passes produce identical bytes on either queue and in any placement; the parity suite checks it ([Z2-N-REND-791], [Z2-N-REND-433]) |
| CPU and GPU simulators agree within tolerance, not bit for bit | GPU arithmetic may fuse multiply-adds and approximate transcendentals; `Zenith2/Docs/Design/VFX/Tests.md → "CPU reference parity tests"` states the tolerances |

## Render-world command stream

### Commands per frame

Each frame, `VFX.Extract` writes, for every instance of its world in ascending instance-key order and every emitter in index order, the commands of "Emitter instance state machine"; effect lights (`AddLightDelta`) and effect decals (`AddDecal`) follow, then the colliders. `Flux_ParticleApplyRenderWorld` consumes the sections in world-tag order: per command, validate, create or update the emitter, and queue releases and destructions. A command refused by the writer (`FLUX_RENDER_WORLD_ERROR_SLOT_FULL`) is counted in `vfx.extract.dropped`; the emitter then misses that frame's update and, after two frames, times out on the render side, which the next full-state frame repairs.

### Full-state frames

With `IsFullExtraction()` ([Z2-N-REND-015]), `VFX.Extract` writes exactly what it writes in any frame (every command is already full state), and the render side, for a world whose section has `m_bFullState`, destroys every instance of that world that the section does not mention. GPU particle state survives a resync; it does not survive a device loss.

### Deferred and failed frames

`Flux_ParticleEndFrame(false)` (a deferred or failed frame, `Zenith2/Docs/Rendering/GpuResources.md → "Build-node upload order"`) restores every emitter's step counter and every target's last consumed tag to their values before the frame (no step executed, so the source's records of its last executed step stay unconsumed and the next submitted frame's targets consume them: `Test_ParticleEvents_DeferredFrameKeepsChildren`), and moves the frame's commands into the system's pending list. An emitter none of whose commands in the frame carries `FLUX_EMITTER_FLAG_SEEK` and that holds no kept steps is merged: spawn counts add (the first command's ordinal base kept, so ordinals stay continuous), `dt` adds and is clamped to `fFLUX_PARTICLE_MAX_STEP_SECONDS`, the newer transforms win, flags OR together, a `RELEASE` or `DESTROY` is kept; CPU spans are not kept (the next frame carries newer CPU state). An emitter whose commands carry `FLUX_EMITTER_FLAG_SEEK` (a preview seek in progress, however many steps the frame had, one included) or that already holds kept steps is never merged, because a merged step of added and clamped `dt` would no longer equal the seek's fixed steps: each of its commands is kept as its own pending step, in step order, after the steps it already holds. A GPU admission refusal of a catch-up unit keeps its steps the same way ("GPU admission per frame").

The next frame executes an emitter's kept steps first, then its own commands, all numbered as steps of that frame ("Frame building"), and only the emitter's last step is drawn. An emitter holds at most `uFLUX_PARTICLE_MAX_PENDING_CATCH_UP_STEPS` = 64 kept steps, so a frame never carries more than `uFLUX_PARTICLE_MAX_STEPS_PER_FRAME` = 128 steps per emitter. When keeping a frame's steps would exceed 64 for any emitter of an instance (two consecutive unexecuted frames of a seek at the maximum `vfx.preview.catchUpStepsPerFrame`), the seek is interrupted: every kept step of every emitter of the instance is dropped, the emitters are reset (new generation, cleared) at the next executed frame, `Flux.Particles.SeekInterrupted` counts the instance, and its `Flux_ParticleInstanceStats::m_bSeekInterrupted` reads true, and the instance draws nothing, until a command with `RESET` arrives for the instance (the next seek's first command), which is the preview session's signal to issue the seek again (`Zenith2/Docs/Design/VFX/Authoring.md → "Timeline scrubbing"`); a partially re-simulated state is never shown as if it were the seek target. The list holds at most `m_uMaxInstances × 16` merged entries plus the kept steps; beyond it the oldest merged `UPDATE` is dropped (`Flux.Particles.PendingDropped`). This is RHI's rule that emitter commands of a deferred frame apply in the next submitted frame (`Zenith2/Docs/Design/RHI/Memory.md → "Frame deferral"`).

### Device loss

`Flux_ParticleOnDeviceRecovered` re-creates the pool's ranges for every live GPU emitter (cleared), the quad index buffer and every effect constants buffer from the assets' CPU copies; GPU particles restart empty while ordinals, generations, step counters and accumulators continue (the event regions are cleared with the state, and a cleared record carries no kind bit, so no stale record is ever accepted), so effects resume spawning on the first recovered frame (`Flux.Particles.DeviceResets` + 1). CPU particles are unaffected.

## Memory pressure response

Particles belong to the Rendering subsystem of `Zenith2/Docs/Architecture.md → "Memory governor"`; their shedding is presentation only.

| Level | Render side (`Flux_ParticleSystem`, through RHI's rendering consumer) | Simulation side (the module's memory consumer) |
|---|---|---|
| WARNING | admitted GPU capacity and sorted particles × 0.75; pool regions stop growing | `vfx.cpuParticlesPerWorld` × 0.75 |
| CRITICAL | GPU capacity and sorted particles × 0.5; the ranges of emitters culled for more than 120 frames are freed (they re-allocate on readmission) | CPU capacity × 0.5; `vfx.detachedEffectsPerWorld` × 0.5; new detached spawns beyond it dropped |

Each response applies within one frame of the level change and reverts one frame after the level falls ([Z2-N-ARCH-821]); `Flux.Particles.PressureScale` and `vfx.pressure.scale` publish the active factor, and `Test_ParticleSystem_PressureShedsAndReverts` forces each level.

The Rendering row of `Zenith2/Docs/Architecture.md → "Shedding per subsystem"` states the particle responses, the single source of shedding ([Z2-N-ARCH-822]; cross-doc item X242 (b)): admitted GPU particle capacity, sorted particles and CPU particle capacity × 0.75 with particle pool regions no longer growing (WARNING), and × 0.5 with the pool ranges of emitters culled for more than 120 frames freed and the detached-effect cap halved (CRITICAL); this file adds implementation detail only.
