# VFX design: authoring, preview and cooking

Section file of `Zenith2/Docs/Design/VFX.md`. It specifies how effects are authored in code with previews (Z2-D-079): the effect document and the optional Slang particle types, the effect preview with its timeline scrubbing and statistics, the effect inspector, the cook pipeline from the `zenith-doc` import to the `.zvfx` file (validation, curve baking, capacity sizing, bounds baking, the constants image), the global particle type set step that feeds the shader compiler, hot reload of effects and of particle types, the engine effect content, and the weather effects ENV's prefabs use. The document and the cooked file themselves are `Zenith2/Docs/Design/VFX/EffectFormat.md`.

## Scope of this file

| Topic | Heading |
|---|---|
| what an author writes, and where | "Code-authored effects" |
| seeing an effect while editing it | "Effect preview" |
| the component inspector | "Effect inspector" |
| from source to cooked file | "Cook pipeline" |
| edits reaching a running editor | "Hot reload" |
| what the engine ships | "Engine effect content" |
| ENV's weather effects | "Weather effects for ENV" |

## Code-authored effects

There is no node graph and no visual effect editor. An effect is two kinds of text, both reviewed as code:

| Artefact | Path | Owner of the format | Required |
|---|---|---|---|
| the effect document `<Name>.vfx.json` | engine: `Zenith2/Engine/ZenithVFX/Content/Effects/`; game: `Zenith2/Games/<Name>/Assets/VFX/` | VFX (`Zenith2/Docs/Design/VFX/EffectFormat.md → "Effect document"`) | yes |
| a code-authored update type (a Slang struct implementing `Flux_ParticleUpdateType`) | engine: `Zenith2/Shaders/Particles/Types/`; game: `Zenith2/Games/<Name>/Assets/VFX/` (`.slang`) | VFX (`Zenith2/Docs/Design/VFX/PublicAPI.md → "Flux_ParticleUpdateType"`) | no; `gpu` emitters only |
| a code-authored shading type (implementing `Flux_ParticleShadingType`) | as above | VFX (`Zenith2/Docs/Design/VFX/PublicAPI.md → "Flux_ParticleShadingType"`) | no; `ParticleShading_Standard` otherwise |

- A game contains no compiled-language source ([Z2-N-ARCH-205]); an effect is therefore always data plus optional Slang, and a game triggers effects from Luau (`VFX.Spawn`, `VFX.SpawnAttached`, `ctx:SpawnVfx`) or by placing `Zenith.VFX.Effect` in scenes and prefabs.
- The built-in modules of `Zenith2/Docs/Design/VFX/EffectFormat.md → "Module catalogue"` cover every Inventory feature; a code-authored type exists for behaviour no module expresses (a vortex field shaped by a gameplay curve, a stylised shading model), and costs a compiled permutation per program it is used with ("PSO keys and permutations" of `Zenith2/Docs/Design/VFX/Rendering.md`).
- A type's `Params` struct is reflected by the shader compiler; the document's `updateType.parameters` (and the render module's `shadingType.parameters`) set its fields by name, packed into the emitter's constants block at the reflected offsets, little-endian, with the reflected defaults for omitted fields, exactly as a material's `parameters` are packed (`Zenith2/Docs/Design/GEOM/Materials.md → "Material cook"`, step 6).
- An author edits text in their editor of choice; the editor's preview ("Effect preview") reloads on every save, so the loop is edit, save, see.

`ZenithShaderCompiler`, in cook mode, writes one `Reflection/Particles/<ModulePath>/<Type>.particle.json` per type found in the shader roots that implements `Flux_ParticleUpdateType` or `Flux_ParticleShadingType`, schema version 1, holding `module`, `type`, `typeId` (`Zenith_Hash64` of `"<module path without extension>/<Type>"`), `interface` (`Flux_ParticleUpdateType` or `Flux_ParticleShadingType`) and `params` (size and fields, as the material reflection's `params`, `Zenith2/Docs/Design/GEOM/Materials.md → "Material reflection input"`); the effect cook validates documents against it the way the material cook reads its reflection. `Zenith2/Docs/Rendering/ShaderSystem.md → "Reflection JSON"` lists the file and `Zenith2/Docs/Rendering/ShaderSystem.md → "Command line"` the `--particle-type-set <file>` input the step writes (cross-doc item X241 (3), (6)).

## Effect preview

### Preview session

The effect preview is an EDIT panel; VFX supplies everything it shows. The panel owns one preview world per open effect.

<!-- assumption: pending EDIT (cross-area row VX11) -->
EDIT's asset preview host creates a preview world through the same world-creation path as its edit world with the world purpose `WORLD_PURPOSE_PREVIEW` (cross-doc item X243 (b)), attaches the game's modules to it (so `Zenith_Module_VFX`'s `AttachWorld` runs as for a play world and records the purpose, which alone enables catch-up and freeze), ticks it at the game's tick rate while the panel is visible, and issues one `VIEW_KIND_PREVIEW` request per panel through its own extractor (`Flux_ViewRequest` of `Zenith2/Docs/Design/RPROD/Views.md → "Flux_View header"`), with `m_uFeatureMaskSet = FLUX_VIEW_FEATURE_PARTICLES | FLUX_VIEW_FEATURE_VOLUMETRIC_FOG` and `m_ulOwnerID` the effect's GUID hash. The panel writes the preview world's components by commands, so undo, the authority rules and the command stream are the edit world's.
<!-- /assumption -->

| Element | Content |
|---|---|
| the effect entity | at the world origin, `Zenith.VFX.Effect` with the effect, `AutoPlay` true, the parameters the panel's sliders set; `Zenith.VFX.PreviewControl` for the transport ("Timeline scrubbing") |
| the camera | an orbit camera at the document's `preview.cameraDistance` (default: 2.5 × the bounds' largest half extent, at least 1 m) looking at the bounds' centre; it is also the world's main camera, so it is the LOD viewer |
| the backdrop | `preview.backdrop`: `grid` (a ground plane with a 1 m grid and the default sky), `sky` (sky alone), `black` (a black clear, no sky); with `preview.environment`, that environment prefab is instanced instead (its sun, sky, fog and weather) |
| colliders | a `grid` backdrop places a `Zenith.VFX.Collider` plane at y = 0, so collision modules visibly bounce; the panel's colliders toggle removes it |
| looping | with `preview.loopPreview` (default true), the panel restarts a finished non-looping effect by incrementing `PlayRequest` 0.5 s after its `Zenith.VFX.EffectStopped` event or, for an effect that never stops by itself, after its `duration` + linger |
| determinism | the preview instance's seed is the effect's `seed` when set, else `Flux_MakeAttachedParticleSeed` of the preview entity's simulation ID and the play generation, so a fresh panel (whose effect entity is the preview world's first spawned entity) always previews the same particles and each loop of the preview shows the next generation's variation |
| transport | play, pause (`Paused`), stop (`StopRequest`), restart (`PlayRequest`), single-step (one frame of 1/60 s while frozen), playback speed (`TimeScale` 0.1 … 4), and the timeline scrubber |
| moving emitter | the "orbit emitter" control moves the effect entity on a circle of 2 m at 2 m/s, so distance spawning, inherited velocity, world-space trails and ribbons can be judged |
| LOD | the camera-distance control moves the orbit camera, so LOD transitions and the cull screen size are judged at the distances a game uses; the current level and screen size are shown in the statistics |

A preview runs the shipped runtime path end to end (document → cook server → `.zvfx` → loader → `VFX.Update` → render commands → Flux), so the particles' simulation, ordering and shading are a game's; the preview-only behaviours are catch-up steps and the freeze, both gated to preview worlds. Two rendering inputs differ, because `VIEW_KIND_PREVIEW` keeps no HiZ pyramid and no fog history (`Zenith2/Docs/Design/RPROD/Views.md → "Persistent state by kind"`):

| Input | In a preview | Consequence |
|---|---|---|
| the previous HiZ that depth collision reads | absent: depth collision reads the sort view's previous HiZ and applies only to emitters of the sort view's world; a preview view keeps no HiZ, and while the editor's views exist the sort view belongs to the edit world (the sort view rule of `Zenith2/Docs/Design/VFX/Rendering.md → "Emitter ranks and the sort view"`) | depth collision is inert in the preview (`Flux.Particles.DepthCollisionUnavailable` counts it), and the strip says so; the `grid` backdrop's collider plane shows analytic collision instead |
| the froxel fog's temporal history | absent | fog is the volume of the current frame alone, without temporal accumulation; particles fog by the same formula, against a noisier volume |

An author judging depth collision or fogged particles at final quality places the effect in a scene and views it in the editor's scene view (an editor view, which keeps both).

### Timeline scrubbing

The scrubber shows the effect's timeline from 0 to `max(duration, 1 s) + linger`, with burst markers, each emitter's start delay and spawn span, and event edges. Dragging writes `Zenith.VFX.PreviewControl`:

1. `SeekSeconds` = the scrubber time, clamped to [0, `fVFX_MAX_SEEK_SECONDS`]; `SeekRequest` incremented; `Freeze` set while the mouse button is held and for a paused preview.
2. `VFX.Update` resets the instance and re-simulates it from effect time 0 to `SeekSeconds` in fixed steps of the preview world's tick duration `dt_s` (event children included: event freshness is keyed by simulate step, never by frame, `Zenith2/Docs/Design/VFX/Simulation.md → "Event spawning"`), at most `vfx.preview.catchUpStepsPerFrame` steps per frame ("Catch-up steps" of `Zenith2/Docs/Design/VFX/Simulation.md`), executed in step order within each frame and drawn after the frame's last step, so in a 60 Hz preview world a seek to 10 s (600 steps) completes in 75 frames at the default of 8.
3. While catching up, the scrubber shows the reached time as a progress bar under its handle and the viewport shows the partially caught-up effect; a new seek during a catch-up abandons it and starts from 0 again, so the last seek always wins. Each frame the session reads `GetInstanceStats(key).m_bSeekInterrupted` (`Zenith2/Docs/Design/VFX/PublicAPI.md → "Flux_ParticleSystem"`): when it turns true (the render side could not keep the seek's steps, "Deferred and failed frames" of `Zenith2/Docs/Design/VFX/Simulation.md`), the session increments `SeekRequest` once with the same `SeekSeconds`; a second interruption of the same seek leaves the preview empty and the strip reads "seek interrupted: lower `vfx.preview.catchUpStepsPerFrame`", because the effect's catch-up work does not fit the tier's `vfx.gpuActiveCapacity`.
4. Once caught up, a frozen preview stays at `SeekSeconds` (presentation time stops); otherwise the effect plays on from there.

The scrubbed live particle state equals the live state of the same effect played from its start with the same seed and one tick of the preview world per frame (interpolation alpha 0, time scale 1), whose presentation Δ is `dt_s` bit for bit ("Presentation clock" of `Zenith2/Docs/Design/VFX/Simulation.md`): the same slots are live and each live slot is equal field for field, except the 8-bit generation in `m_uFlags` bits 0–7, which the seek's `RESET` advanced (`Test_VFXPreview_SeekMatchesPlayback`, `Zenith2/Docs/Design/VFX/Tests.md → "Determinism tests"`); so a scrubbed frame is a frame a game could show.

### Preview statistics

The statistics strip reads `Flux_ParticleSystem::GetStats` and `GetInstanceStats(key)` (`Zenith2/Docs/Design/VFX/PublicAPI.md → "Flux_ParticleStats"`) for the preview instance, and the simulation side's per-instance record for CPU emitters, once per frame:

| Statistic | Source |
|---|---|
| per emitter: live particles: exact for CPU emitters; for GPU emitters the CPU-known upper bound `m_auLiveUpperBound`, and, with `vfx.debug.validateBounds` (development builds), the measured count `m_auMeasuredLive` of the frame `m_ulMeasuredFrameIndex`, one to F frames behind (F the frames in flight, "GPU bounds validation" of `Zenith2/Docs/Design/VFX/Simulation.md`); capacity, spawned this frame, LOD level, admitted or refused and why | instance stats; `VFX.Update` |
| per emitter: overwritten live particles (validation builds) | `Flux.Particles.Overwrites` attributed to the emitter |
| effect: significance, screen size, the CPU bounds, the GPU-measured bounds and their ratio | instance stats ("GPU bounds validation" of `Zenith2/Docs/Design/VFX/Simulation.md`) |
| GPU time of the preview frame's particle passes | the GPU profiler's pass timings for `ParticleEmit`, `ParticleSimulate`, `ParticleCompactAndSortKeys`, `ParticleSort`, `Particles` in the preview view (DEVT's profiler surface) |
| memory: pool bytes held by the instance, constants buffer bytes | instance stats |
| warnings of the last cook of the document | the cook server's result for the item (`Zenith2/Docs/Design/ASSET/Cook.md → "Cook diagnostics"`) |

With `ShowBounds`, the viewport draws the CPU bounds in white and the GPU-measured bounds in yellow (debug-overlay lines), and the strip turns the ratio red when a particle left the CPU bounds, which is the author's cue to widen `bounds.margin` or switch to `fixed`.

## Effect inspector

Effects placed in scenes and prefabs are edited through EDIT's generic component inspector over the reflected properties of `Zenith.VFX.Effect`, `Zenith.VFX.Collider` and `Zenith.VFX.Viewer` (`Zenith2/Docs/Design/VFX/PublicAPI.md → "Zenith_VFXComponents header"`); VFX adds three customisations to it (pending EDIT, cross-area row VX11):

| Customisation | Behaviour |
|---|---|
| the effect picker | the `Effect` property picks from assets of `ASSET_KIND_PARTICLE_EFFECT`; assigning an effect writes its declared parameter defaults into the parameter properties in the same command ("Zenith_VFXEffectComponent" of `Zenith2/Docs/Design/VFX/PublicAPI.md`) |
| named parameters | properties 9 … 16 are shown under the effect's parameter names with their declared types (a colour parameter as a colour picker, a float3 as a vector); undeclared slots are hidden |
| play controls | in the edit world, "play", "stop" and "restart" buttons increment `PlayRequest` / `StopRequest` by command, so an effect can be judged in place; the edit world runs VFX like a play world |
| gizmos | a collider draws its shape (plane grid, sphere, box, capsule) in the scene view; a viewer draws its frustum; an effect draws its CPU bounds when selected |

The edit world's editor viewports reach the LOD selection through `Zenith.VFX.Viewer` components the editor places on its viewport camera entities (up to the eight viewers of "Viewers" in `Zenith2/Docs/Design/VFX/Simulation.md`), so an effect near the scene camera runs at full detail even when the game camera is far away.

## Cook pipeline

### Importer and intermediate

| Stage | Owner | Behaviour |
|---|---|---|
| import | ASSET's `zenith-doc` importer (`Zenith2/Docs/Design/ASSET/Import.md → "Importer registry"`) | `*.vfx.json` maps to one product of `ASSET_KIND_PARTICLE_EFFECT`, GUID the document's `id`; the importer writes the product's `vfx.effectDocument` intermediate, the source file's bytes unchanged (cross-area row VX5) |
| intermediate | VFX | `vfx.effectDocument`: the UTF-8 bytes of an effect document; the only writer is the `zenith-doc` importer, since no importer generates effects |
| cook | VFX's `vfx.effect` step ("Effect cook step") | reads the intermediate through `Zenith_CookContext::ReadIntermediate` |
| global | VFX's `vfx.particleTypes` step ("Particle type set step") | after every effect is cooked, before RPROD's `rprod.shaders` |

The document's `id` must equal the item's GUID (`VFX_COOK_ERROR_SCHEMA` otherwise), and the step writes the `.zvfx` under that GUID and mints none, as GEOM's material step does.

### Effect cook step

| Field of `Zenith_CookStepDesc` (`Zenith2/Docs/Design/ASSET/Cook.md → "Zenith_CookStepDesc"`) | Value |
|---|---|
| `m_szID` | `vfx.effect` |
| `m_uVersion` | 1 |
| `m_ePhase`, `m_eKind` | `COOK_PHASE_ASSET`, `ASSET_KIND_PARTICLE_EFFECT` |
| `m_aszToolVersions` | none |
| `m_axEngineFallbacks` | none: the kind's fallback policy is `NULL_OBJECT` (an attached effect with a missing effect plays nothing) |
| `m_bOutOfProcess` | true: the step reads only intermediates, published properties and reflection files |
| `m_uPlatformMask` | every platform |

`Cook(xContext)` runs these steps in order; every error is a `Zenith_VFXCookErrorCode` reported as `COOK_ERROR_STEP_FAILED` with the code in `m_uDetail` (`Zenith2/Docs/Design/VFX/EffectFormat.md → "Document validation"`), and every check of a document is made before the step returns, so one cook reports every error the document has:

| # | Action | Failure |
|---|---|---|
| 1 | `ReadIntermediate("vfx.effectDocument")`; parse with `Zenith_JsonDocument` under `Zenith_JsonLimits`; migrate in memory to schema version 1 when older (never writing the source) | `VFX_COOK_ERROR_SCHEMA` with line and column; a missing blob is `VFX_COOK_ERROR_SCHEMA` naming the product |
| 2 | validate checks 1 … 8 of "Document validation" | the codes listed there |
| 3 | resolve every texture, mesh and decal-material GUID through the context; `DeclareReference(guid, ASSET_DEPENDENCY_FLAG_HARD)` for each, in the order textures, meshes, decal materials, each list in first-use order | `VFX_COOK_ERROR_REFERENCE` |
| 4 | read each mesh's published `geom.meshBounds` property (`ReadDependencyOutput`) for mesh particles' size bounds | `VFX_COOK_ERROR_REFERENCE` when the property is absent |
| 5 | load each code-authored type's reflection ("Code-authored effects"); check the interface and pack the type's `Params` | `VFX_COOK_ERROR_CUSTOM_TYPE` |
| 6 | bake every curve, gradient and random-curve bound into tables (`Zenith2/Docs/Design/VFX/EffectFormat.md → "Curve baking"`), deduplicating bit-equal tables | warning `CURVE_RESOLUTION` |
| 7 | pack the module parameter blocks and the constants image (`Zenith2/Docs/Design/VFX/EffectFormat.md → "Effect constants image"`) | `VFX_COOK_ERROR_LIMIT` |
| 8 | size capacities ("Capacity sizing") | warnings `CAPACITY_OVERFLOW`, `SORT_COST` |
| 9 | bake bounds ("Bounds baking") for `baked` mode; compute the analytic terms, the composed event-target terms and the estimate flag for both modes | `VFX_COOK_ERROR_BOUNDS_BAKE`; warning `BOUNDS_ESTIMATE` |
| 10 | write the ten sections through `MakeWriterDesc(0x5846565A, uFLUX_PARTICLE_EFFECT_FORMAT_VERSION)` and `WriteAssetFile(bytes, residentEstimate)`, the estimate the reader's `GetResidentBytes()` computed from the same tables | the context's write errors |
| 11 | add the item's cook-report entry ("Cook report members") | — |

Warnings `UNUSED_PARAMETER` (a declared parameter no module binds) and `BURST_SPACING` are reported by step 2.

The step's derived-data key inputs, after the driver's rows and in this order: the `vfx.effectDocument` blob's bytes (row `vfx.document`); each code-authored type's reflection file hash, in first-use order (row `vfx.typeReflection`); each mesh's `geom.meshBounds` published bytes, through `ReadDependencyOutput`, which joins the mesh's key hash; `uZENITH_ENGINE_COOK_VERSION`. Textures and decal materials contribute only their GUIDs (already in the document's bytes), because the cooked effect stores references to them and nothing derived from their content.

<!-- assumption: pending GEOM (cross-area row VX8) -->
GEOM's `geom.mesh` step publishes `geom.meshBounds` (`PublishProperty`): 28 bytes, LOD 0's local AABB (6 × f32) and its bounding-sphere radius about the origin (f32), little-endian.
<!-- /assumption -->

### Capacity sizing

<!-- elaboration -->
The cook gives every emitter a capacity `C` that is a multiple of 64 (a power of two for an event target at depth 2 or more), an event capacity and a trail sample count, so the runtime never allocates per particle and never overwrites a live particle at the authored maximum rate. Because a ring slot is freed only when its ordinal comes round again, never by a death, every estimate below covers the authored maximum rate times the maximum lifetime, whatever fraction of particles dies early (cost V2 of `Zenith2/Docs/Design/VFX.md → "VFX rulings"`).

| Emitter | Analytic estimate `A` |
|---|---|
| rate | `ceil(r_max × L_max)`: `r_max` the largest value of the rate over the timeline (the curve table's maximum × scale + offset, or the bound parameter's declared default × scale + offset), `L_max` the largest lifetime of the `lifetime` module (10 s default when absent: the module's default) |
| bursts | `+ B_max`: the largest total count (the upper bound of `count`) of the bursts that fire within any window of `L_max` seconds over two consecutive timeline cycles |
| distance | `+ ceil(L_max × fVFX_COOK_DISTANCE_REFERENCE_SPEED / spacing)`, the reference speed 20 m/s, and at most `maxPerFrame × ceil(L_max × 60)` |
| event target | for a target `t` at depth `d` ≥ 1, on the path `a_0 → a_1 → … → a_d = t` from its chain root `a_0` (the first ancestor that is no event target) through the edges `e_1 … e_d` (`e_i` from `a_{i−1}` to `a_i`): `A = (Π_{i=1…d} k_i × E_edge,i) × (ceil(r_root × (W + Δ_max)) + B_root(W) + n_prewarm,root)` with `W = L_max,t + Σ_{i=0…d−1} (L_max,a_i − d_min,a_i + Δ_max)` (derivation below): `k_i` edge `e_i`'s `count`, `E_edge,i` its event slots (1 for a death edge, `maxCollisions` for a collision edge, "Event spawning" of `Zenith2/Docs/Design/VFX/Simulation.md`), `r_root` the root's rate estimate (its `r_max`, plus its distance term's rate `20 / spacing` when it distance-spawns), `L_max,t` the target's largest lifetime, `L_max,a_i` ancestor `a_i`'s largest lifetime, `d_min,a_i` the earliest age at which `a_i` raises edge `e_{i+1}`'s event (step 2 below, from `a_i`'s own lifetimes and killing modules), `Δ_max` = `fFLUX_PARTICLE_MAX_STEP_SECONDS` (1/15 s), `B_root(W)` the root's largest total burst count within any window of `W` seconds over two consecutive timeline cycles, `n_prewarm,root` its prewarm count (0 without prewarm). At depth 1 (the source is the root) this is the one-hop form `k × E_edge × (ceil(r_src × (W + Δ_max)) + B_src(W) + n_prewarm,src)` with `W = L_max + L_src,max − d_min + Δ_max`, which steps 1–4 below derive |
| prewarm | `max(A, n_prewarm)`, `n_prewarm` of "Prewarm" in `Zenith2/Docs/Design/VFX/Simulation.md` |

The event-target derivation, which `Zenith2/Docs/Design/VFX/Simulation.md → "Event spawning"` relies on. A child has ordinal `(o_s × E_edge + e_local) × k + j` and slot `ordinal mod C_target`, so no live child is overwritten exactly when the live children's ordinals span at most `C_target` (modulo 2³² for a target at depth 2 or more, whose power-of-two capacity divides 2³²). Take any effect time `τ` and a target at depth 1:

1. A child alive at `τ` was spawned in `(τ − L_max, τ]`; its source event was consumed by the emission of the step after the event, at most `Δ_max` after it, so the event happened in `[τ − L_max − Δ_max, τ]`.
2. A source particle's ordinal is assigned in the frame of its spawn, at effect time `t_o`; it was pre-aged by at most `Δ_max` in that frame, so its event happens at an age, measured from `t_o`, in `[d_min, L_src,max]`, where `d_min = max(0, L_src,min − Δ_max)` for a death edge whose source has no killing module, and `d_min = 0` for a collision edge (its events fall anywhere in the source's life) and for a death edge whose source has any killing module, because such a source dies at any age: `collision` with `kill` true or `killAfter` > 0, `killVolume`, `attractor` with a `killRadius` above 0, `precipitationOcclusion`, or a `custom` update type whose `bKILLS` is true. A lifetime bound to a parameter (`P` form) also gives `d_min = 0`, its declared default only sizing `L_src,max` (with the `CAPACITY_OVERFLOW` warning naming the assumption).
3. So the live children's sources had `t_o` in `[τ − L_max − Δ_max − L_src,max, τ − d_min]`, a window of length `W = L_max + L_src,max − d_min + Δ_max`. Ordinals are monotonic in `t_o`, and the ordinals assigned within a window of length `W` number at most `ceil(r_src × (W + Δ_max))` rate and distance spawns (the window's frames can straddle both ends by one frame), plus the bursts firing in it, plus the prewarm block (spawned at once in the first frame, dying at any age up to `L_src,max`).
4. Those source ordinals are contiguous, so the children's ordinals span at most `E_edge × k` times their count: the formula above.

At depth `d` ≥ 2, steps 1–2 apply once per hop. A live child's source `a_{d−1}` raised its event at an age in `[d_min,a_{d−1}, L_max,a_{d−1}]` after its own spawn (an event child is spawned at age 0, so step 2's pre-ageing term only makes `d_min,a_{d−1}` conservative), and that spawn consumed an event of `a_{d−2}` at most `Δ_max` after it was raised; so each hop back to the root widens the window by `L_max,a_i − d_min,a_i + Δ_max`, and the root's ordinal frames of every live child's ancestry lie in a window of the chain form's `W`, which step 3 counts. Ordinals of `a_i` are monotonic in `a_{i−1}`'s as blocks (an ordinal `o` of `a_{i−1}` owns `a_i`'s ordinals `[o × E_edge,i × k_i, (o + 1) × E_edge,i × k_i)`), hence in the root's, so contiguous root ordinals have contiguous descendants at every depth and step 4 multiplies their count by `Π k_i × E_edge,i`. A child's own ordinals need not be monotonic in its source's spawn time (a source particle with a smaller ordinal can die after one with a larger ordinal), which is why the window is taken at the root.

For example a death edge with 4 children from a source of rate 100/s, lifetimes in [0.2, 2] s and no killing module, into a target of 0.5 s: `d_min` = 0.2 − 1/15 = 0.1333, `W` = 0.5 + 2 − 0.1333 + 0.0667 = 2.4333, `ceil(100 × 2.5)` = 250 source ordinals, `A` = 4 × 250 = 1,000, `C` = `roundUp(1,100, 64)` = 1,152. With a `killVolume` on the source, `d_min` = 0, `W` = 2.5667, `ceil(100 × 2.6333)` = 264, `A` = 1,056, `C` = 1,216. The bound holds for rate, burst and prewarm sources at any lifetime spread; for a distance-spawning source it holds while the emitter moves at most `fVFX_COOK_DISTANCE_REFERENCE_SPEED` (20 m/s), the sizing's stated assumption, beyond which the validation build counts `Flux.Particles.Overwrites`. `Test_VFXCook_EventTargetCapacityVariedLifetimes` and `Test_ParticleParity_EventsVariedLifetimes` hold it for spread lifetimes, a collision edge, a `killVolume` source, a `collision` `kill: true` source and a three-emitter chain.

- `auto` capacity is `C = clamp(roundUp(ceil(A × 1.1), 64), 64, maxForTarget)` (262,144 GPU, 4,096 CPU), except for an event target at depth 2 or more (its source is itself an event target), whose capacity is the power of two `C = clamp(nextPow2(max(64, ceil(A × 1.1))), 64, maxForTarget)` (both maximums are powers of two): its 32-bit child ordinals can wrap, and only a capacity dividing 2³² keeps its slot `o mod C = o & (C − 1)` continuous across the wrap ("Event spawning" of `Zenith2/Docs/Design/VFX/Simulation.md`). A clamped value, or an estimate that used a parameter default, reports `VFX_COOK_WARNING_CAPACITY_OVERFLOW` naming the emitter, the estimate and the assumption, and the runtime then overwrites the oldest slots, which is the defined behaviour of an over-full ring.
- An explicit `capacity` below `A` reports the same warning; the cook never changes an explicit value. An explicit capacity of an event target at depth 2 or more that is not a power of two is `VFX_COOK_ERROR_EVENT_GRAPH` (check 5 of `Zenith2/Docs/Design/VFX/EffectFormat.md → "Document validation"`).
- The bounds bake ("Bounds baking") measures each emitter's peak live count over its seeds; a peak above `C` reports `CAPACITY_OVERFLOW` with the measured peak, so an estimate wrong in either direction is visible in the cook report.
- The event capacity of a source emitter is exactly `C_src × E` records (`E` its event slots per particle: 1 for its death edge plus `maxCollisions` for its collision edge), the ring of "Event spawning" in `Zenith2/Docs/Design/VFX/Simulation.md`; every record has its own slot, so none is ever dropped. A product above `uFLUX_PARTICLE_MAX_EVENT_RECORDS` (262,144) is `VFX_COOK_ERROR_LIMIT` naming the emitter; the bytes are `48 × C_src × E` in the pool's event region.
- Trail samples are the `trail` module's `samples` (2 … 8); its bytes are `C × samples × 16` in the trail region.
- A depth-sorted emitter whose `C` exceeds 32,768 reports `VFX_COOK_WARNING_SORT_COST`: one such emitter takes the S24's whole sorted-particle budget ("Sorted-particle admission" of `Zenith2/Docs/Design/VFX/Simulation.md`).

### Bounds baking

<!-- elaboration -->
An emitter's runtime bounds are composed from these cooked quantities ("Bounds" of `Zenith2/Docs/Design/VFX/Simulation.md`): the emitter-frame box `B_e` (record "local bounds"; for an event target its spawn-relative box `S_t` instead, bounds flag bit 1), the world displacement box `D` (record "world displacement") with its cube flag, the inherited-velocity gain `J` (seconds), the wind gain `G` (metres per m/s), the bounds history `H` (seconds) and, for an event target, its chain root's emitter index. The cook measures `B_e` and `S_t` by simulation and computes the rest analytically, because only the measured boxes move and rotate with the emitter.

For `bounds.mode: baked`, the cook measures `B_e` by running the CPU reference simulation of the effect (`Zenith_VFXRunReference`, `Zenith2/Docs/Design/VFX/PublicAPI.md → "Zenith_VFXReference header"`) over the cooked tables of steps 1 … 8, before the file is written:

| Rule | Value |
|---|---|
| runs | `bakeSeeds` runs, run i with the instance seed `Flux_ParticleHash(i + 1)` |
| duration | `bakeSeconds`, at fixed steps of 1/60 s; looping effects run that long, non-looping ones until the timeline and the longest lifetime have elapsed if that is shorter. Run-time steps reach `fFLUX_PARTICLE_MAX_STEP_SECONDS`, at which an emitter-frame velocity that changes during a particle's life (drag, curves, an emitter-space force of a local-space emitter) integrates differently, so the measured boxes rest on `margin` and the development-build validation at those steps ("Bounds" of `Zenith2/Docs/Design/VFX/Simulation.md`); the analytic terms below hold at every step up to it |
| emitter | stationary at the origin with identity rotation and scale (an empty track); parameters at their declared defaults; LOD level 0; spawn scale 1 |
| world terms | excluded (`m_bExcludeWorldTerms`): gravity, a `force` with `space: world` and, for a world-space emitter, a `force` with `space: emitter` (it rotates with the emitter during a particle's life) and `turbulence` contribute zero, because they act in world axes or along a rotating axis; they are the box `D` below. The `wind` module runs against zero wind, so its relaxation still scales the velocity (the part of the module that is linear in the emitter-frame velocity) and the wind itself is the render side's term |
| distance spawning | a stationary emitter spawns nothing by distance, so the bake runs a `distance` module as a rate of `fVFX_COOK_DISTANCE_REFERENCE_SPEED / spacing` (20 m/s), capped at `maxPerFrame × 60` per second, at the emitter origin; the path itself is covered at run time by the bucket union of "Bounds" |
| inherited velocity | zero in the bake; covered at run time by the inherited-velocity term from the emitter's measured speed. An event child inherits `inheritVelocity ×` its source record's velocity, whose emitter-frame part the bake includes (the source's world-term-free velocity), and whose world and inherited parts the composed `D` and `J` below carry |
| environment | no colliders, no depth collision, no precipitation occlusion (those modules run with empty inputs) |
| simulated targets | every emitter, `gpu` ones included: the reference simulator executes the GPU modules' CPU twins, which is the same arithmetic the parity tests hold the GPU to |
| measured | per emitter, the AABB of every live particle position after every step and of every event record's position (a particle's death position lies one integration past its last live position), expanded per particle by its draw extent: `0.5 × max(size.x, size.y) × √2` for sprites (× the stretch for velocity facing: `+ |v| × stretch`), `0.5 × width` for ribbons and trails, `radius × size.x` for mesh particles (the mesh's bounding-sphere radius); for an event target, also the AABB of every child's position minus its own spawn position, with the same expansion (`Zenith_VFXReferenceEmitterResult::m_xSpawnRelativeBounds`); and the peak live count and the overwrites (event children included) |
| margin | each half extent grows by `margin × half extent` on every side, then the box is rounded outward to a 1/256 m grid (`floor(min × 256) / 256`, `ceil(max × 256) / 256`) |
| output | each emitter record's local bounds: `B_e` for an emitter that is no event target, `S_t = R_t ⊕ S_source` for an event target (`R_t` its own spawn-relative box after the margin; `S_source` the source's `S`, `{0}` when the source is no event target), so a chain's relative displacements add; the effect header's bounds the union of the roots' `B_e` |
| cost | at most 10⁸ particle-steps per effect, counted by the simulator; beyond it the bake stops with `VFX_COOK_ERROR_BOUNDS_BAKE` naming the effect and suggesting `fixed` bounds |
| code-authored update types | an emitter with an `updateType` cannot be baked (its update has no C++ form): `baked` mode on such an emitter's effect is `VFX_COOK_ERROR_BOUNDS_BAKE` naming the emitter, and the author gives `fixed` bounds |

The analytic terms, for both `baked` and `fixed` modes (a `fixed` box is `B_e`, the emitter-frame part, as the author measures it under identity rotation with no world term; for an event target in `fixed` mode the cook takes `S_t = [min − max, max − min]` per axis of the fixed box, which contains every displacement between two points of it), computed per emitter in table order (sources before targets), every bound of a value taken at the extreme of its form (a constant, the random bounds, a curve table's extremes × scale + offset, or a bound parameter's declared default × scale + offset with the `CAPACITY_OVERFLOW` warning naming the assumption):

| Term | Value |
|---|---|
| world acceleration range `[a_min, a_max]` | per axis, summed over the emitter's world-axis accelerations: gravity `(0, −9.80665 × scale, 0)`, a world-space `force`, and, for a world-space emitter, an emitter-space `force` as `±|acceleration|_max` on every axis (it rotates with the emitter) and turbulence `±strength × Σ_{i<octaves} 0.5^i` on every axis (the noise's range) |
| own displacement `D_own` | per axis `[min(0, ½ a_min L_max (L_max + Δ_max)), max(0, ½ a_max L_max (L_max + Δ_max))]`, `L_max` the emitter's largest lifetime, `Δ_max` = `fFLUX_PARTICLE_MAX_STEP_SECONDS`. The simulators integrate semi-implicitly (accelerations into `v`, then `p += v × dt`, "Particle update order" of `Zenith2/Docs/Design/VFX/Simulation.md`), so after steps `dt_1 … dt_n` with `t_k = dt_1 + … + dt_k` a constant acceleration `a` displaces a particle by `a × Σ_k t_k dt_k = a × (½ t_n² + ½ Σ_k dt_k²) ≤ ½ a L (L + Δ_max)` for `t_n ≤ L` and every `dt_k ≤ Δ_max`; an acceleration varying inside `[a_min, a_max]` and `drag` (which scales every earlier increment by a factor in [0, 1]) only move each increment's weight `dt_i (t_n − t_{i−1})` within `[0, dt_i (t_n − t_{i−1})]`, so the box holds with both |
| world velocity `V` (cook-time only) | own `V_own` per axis `[min(0, a_min L_max), max(0, a_max L_max)]`; an emitter that is no event target has `V = V_own`; a target has `V_t = V_own,t ⊕ iv × V_source`, the world velocity a child can carry at its death or collision; `iv` is its edge's `inheritVelocity` rounded to half precision, the value both simulators read from `m_uEventEdge` bits 16–31 ("Event spawning" of `Zenith2/Docs/Design/VFX/Simulation.md`), and every composed term below uses that value |
| `D` (record "world displacement") | no event target: `D_own`; an event target: `D_t = D_source ⊕ D_own,t ⊕ iv × L_max,t × V_source`, the source chain's world displacement at the spawn point, the child's own, and the displacement of the world velocity it inherited. Every box contains 0, which is what lets a velocity scaled into [0, 1] stay inside (`drag`, the wind relaxation) |
| cube flag (`m_uBoundsFlags` bit 0) | set for a local-space emitter (its emitter-space gravity rotates with the emitter), for an emitter with a `collision` module (a bounce reflects a displacement), and for an event target whose source carries it: the runtime then uses the cube of half extent `max|corner of D|` |
| inherited-velocity gain `J` (seconds, record "inherited-velocity gain") | `F × L_max`, the inherited-velocity factor `F` the `inheritVelocity` module's `factor` (its upper bound, 0 when absent) for an emitter that is no event target; an event target `J_t = J_source + F_t × L_max,t` with `F_t = f_t + iv × F_source`: the source particle's displacement by its inherited emitter velocity, plus the child's by its own and by the share it inherited |
| wind gain `G` (metres per m/s, record "wind gain") | `F_w × L_max` with `F_w` the `wind` module's `factor` (its upper bound; 0 without the module) for an emitter that is no event target; an event target `G_t = G_source + F_w,t × L_max,t` with `F_w,t = f_w,t + iv × F_w,source`: the wind part of a velocity is at most `F_w ×` the wind speed bound, because the relaxation `v ← v + (f × W − v) × s`, `s ∈ [0, 1]`, keeps a wind-driven velocity between its previous value and `f × W` |
| bounds history `H` (seconds, record "bounds history") | `L_max` for an emitter that is no event target; `L_max,t + fFLUX_PARTICLE_MAX_STEP_SECONDS + H_source` for an event target |
| estimate flag (`m_uBoundsFlags` bit 2) | set, with `VFX_COOK_WARNING_BOUNDS_ESTIMATE` naming the emitter and the module, for the estimate class of "Bounds" in `Zenith2/Docs/Design/VFX/Simulation.md` (a world term combined with `speedLimit`, `vortex`, `attractor` or local-space `turbulence`; any `collision`) |

The wind displacement is not in `D`: ENV's wind ranges up to 60 m/s with gusts up to twice the speed (`Zenith2/Docs/Design/ENV/WaterWeather.md`, weather `windSpeed` 0 … 60, `gustStrength` 0 … 2), so a cook-time reference wide enough to be conservative (`60 × (1 + 2 + 0.5)` = 210 m/s) would make every windy emitter's box hundreds of metres wide and its culling and LOD useless, while a smaller reference is not conservative. The render side, which receives each world's wind in the render environment every frame, adds `G × W` instead (`Zenith2/Docs/Design/VFX/Rendering.md → "Per-view emitter culling"`).

`fixed` bounds are copied from the document. The bake is deterministic: the reference simulator is single-threaded per run, uses only `float` arithmetic in a fixed order, and the cook host is Windows x64 alone, so a re-cook of the same inputs writes the same bytes (ASSET's reproducibility check). `Test_VFXCook_BoundsBake` holds the composition against the reference simulation of the same fixtures on moving and rotating tracks, run without colliders: a sparks emitter tilted 90°, a distance-spawned ribbon moving at 15 m/s, an `inheritVelocity` emitter moving at 10 m/s, a turbulent world-space emitter, `Debug_Sparks` with its smoke target on a track moving at 10 m/s, and a `wind` emitter under a uniform 40 m/s wind (`Zenith_VFXReferenceDesc::m_xWind`) with the wind cube `G × 40` added; every particle of 64 held-out seeds lies inside the runtime bounds the cooked quantities compose, the gravity fixtures run at fixed steps of 1/15 s as well as 1/60 s, and `D` equals the `D_own` above (`½ a L (L + Δ_max)` per axis) composed as the table states. A colliding local-space fixture carries the estimate flag and `VFX_COOK_WARNING_BOUNDS_ESTIMATE`, and every linear-class fixture carries neither.

### Particle type set step

The step feeds the cook-time compilation of particle programs of `Zenith2/Docs/Rendering/ShaderSystem.md → "Enumeration rule"` (step 6, [Z2-N-REND-123], [Z2-N-REND-124]; cross-doc item X241); the step lands with slice 14.

| Field | Value |
|---|---|
| `m_szID` | `vfx.particleTypes` |
| `m_uVersion` | 1 |
| `m_ePhase`, `m_eKind` | `COOK_PHASE_GLOBAL_POST`, `ASSET_KIND_NONE` |
| `m_aszRunsAfter` | none of its own; RPROD's `rprod.shaders` lists `vfx.particleTypes` in its runs-after (cross-area row VX15) |
| `m_aszOutputPrefixes` | `VFX/` |
| `m_bOutOfProcess` | false |

1. Read every cooked effect's report entry for the game and platform (each lists its emitters' (target, render kind, update type ID, shading type ID, lighting, soft, fog, sub-UV blend, trail, mesh stream-set) tuples).
2. Write `VFX/particle_types.json`, the shader compiler's `--particle-type-set` input: the distinct code-authored update types and shading types (`typeId`, `module`, `type`), sorted by `typeId`, and the used (program, program key, type variant) triples, sorted by program path, then key, then variant. A program key lists the permutation values of "PSO keys and permutations" (`Zenith2/Docs/Design/VFX/Rendering.md`) the emitter's settings select, for each shading path the platform enables; because `vfx.maxLightingMode` may clamp any emitter at run time ("PSO keys and permutations" of `Zenith2/Docs/Design/VFX/Rendering.md`), a lit emitter's keys are listed for its authored lighting mode and every lower mode (at most three).
3. After `rprod.shaders` compiles, `rprod.pso`'s check of used triples against the compiled set covers the particle triples, failing with `COOK_ERROR_PERMUTATION_MISSING` naming the effect, the program and the key ([Z2-N-REND-125]); the particle programs' counts join the per-game total checked against `Z2-B-RPROD-024` Shader permutation limit per game.

```json
{ "schemaVersion": 1, "game": "OpenWorldSlice", "platform": "windows",
  "types": [ { "typeId": "0x3c95…", "interface": "Flux_ParticleUpdateType", "module": "Zenith2/Games/OpenWorldSlice/Assets/VFX/Vortex.slang", "type": "ParticleUpdate_Vortex" } ],
  "uses": [ { "program": "Zenith2/Shaders/Passes/Particles/Simulate.slang", "permutation": { "bPERM_DEPTH_COLLISION": true }, "variant": "0x3c95…", "effects": [ "8d0f3c1e-5a52-4f0e-9e4b-6b1d2f0a7c11" ] } ] }
```

The step's key inputs are every effect's report tuple list and the platform; editing an effect that changes no tuple re-cooks nothing here.

### Cook report members

The `vfx.effect` step adds one `vfx` report member per item (`Zenith_CookContext::AddReportMember`), serialized in this order:

| Member | Content |
|---|---|
| `emitters` | per emitter: `name`, `target`, `capacity`, `capacityEstimate`, `capacitySource` (`auto` or `explicit`), `peakLive` (from the bake, or null for `fixed` bounds), `eventCapacity`, `trailSamples`, `constantsBytes`, the tuple of "Particle type set step" step 1 |
| `bounds` | `mode`, the effect bounds, the particle-steps the bake used |
| `curves` | table count, deduplicated count, the worst resolution error |
| `residentBytes` | the reader's estimate |
| `warnings` | the codes reported, with the JSON pointer of each |

The `vfx.particleTypes` step adds a `particleTypes` member: the type count, the triple count, and per program the variant count.

## Hot reload

| Edit | Path to the running editor |
|---|---|
| an effect document saved | the cook server re-imports and re-cooks the item and publishes it; EDIT calls `Zenith_AssetRegistry::RequestReload` (`Zenith2/Docs/Design/ASSET/Loading.md → "Development reload"`); the reload swaps the asset object, keeping the old one `asset.reload.retireFrames` frames |
| a code-authored type's `.slang` saved | the shader compiler's watch mode recompiles the type's variants and the renderer swaps the PSOs through the watch-mode reload protocol (`Zenith2/Docs/Rendering.md → "Shader system and offline compilation"`); a `Params` layout change re-cooks every effect that uses the type (their key holds the reflection hash) and reloads them as above |
| an engine effect or a referenced texture or mesh | as an effect document, or through the dependency's own reload |

On a reload swap, `VFX.Update` sees, for every instance whose `Resolve` returns a different object than the one it recorded, an effect change: an attached instance restarts under a new instance key (the old one is destroyed, so its particles vanish rather than outliving a layout they no longer match), a detached instance is destroyed, and a preview loops back to its current scrub time; the render side resolves the new object, creates its constants buffer, and releases the old one through deferred deletion once no instance references it. A failed reload keeps the old object, so the effect keeps playing and the panel shows the cook errors. Reload is development-only, as `RequestReload` is.

## Engine effect content

The engine ships these effects under `Zenith2/Engine/ZenithVFX/Content/Effects/`, cooked like any content ([Z2-N-ARCH-028]) and used by the engine's tests, benchmarks and the slices:

| Effect | Content | Exercises |
|---|---|---|
| `Debug_Sparks.vfx.json` | a GPU burst of 200 velocity-stretched additive sprites with gravity, drag and analytic collision, and a death event spawning smoke | bursts, events, collision, additive partition |
| `Debug_Smoke.vfx.json` | a GPU rate emitter of lit, soft, depth-sorted alpha sprites with a sub-UV flipbook, `turbulence` (value noise, "Particle update order" of `Zenith2/Docs/Design/VFX/Simulation.md`) and wind | sorting, lighting, soft particles, fog, flipbooks, turbulence, wind |
| `Debug_Ribbon.vfx.json` | a CPU distance-spawned emitter ribbon with tail fade | CPU target, distance spawning, ribbons |
| `Debug_Trails.vfx.json` | a GPU emitter of particles with trail ribbons and orbit velocity | trails |
| `Debug_Debris.vfx.json` | GPU mesh particles spinning about their +Y axis (the `rotation` module's `rate`; mesh particles rotate about +Y alone, "Mesh particle rendering" of `Zenith2/Docs/Design/VFX/Rendering.md`) with depth collision | mesh particles, depth collision |
| `Debug_Light.vfx.json` | a flash with a light module and a scorch decal | effect lights and decals |
| `Stress_Fountain.vfx.json` | one GPU emitter at the maximum capacity 262,144 | capacity limits, the stress suite |

Their textures (`Zenith2/Engine/ZenithVFX/Content/Textures/`: a soft-dot sprite, a 4×4 smoke flipbook, a spark streak) are committed source images imported by CONTENT's image importer like every engine texture.

## Weather effects for ENV

ENV's weather prefabs (rain, snow, splashes) carry `Zenith.VFX.Effect` components that name effects ENV authors under its own content directory; ENV spawns and destroys the prefabs and never depends on VFX, VFX sees its own components on those entities, ENV owns the documents and their look, and VFX owns the modules they use. Nothing of VFX lands before Stage 5 (`Zenith2/Docs/Design/VFX.md → "Stage plan"`), so ENV's Stage 4 weather prefabs carry no VFX component; the components join the prefabs in ENV's Stage 5 content diff, after VFX's slice 15 ships the modules below.

| ENV need | VFX surface |
|---|---|
| rain streaks and snowflakes falling past a moving camera | a GPU emitter with `space: world` on the prefab's entity, which ENV parents to the world's environment anchor (typically the camera rig, `Zenith2/Docs/Design/ENV/WaterWeather.md → "Effect prefabs"`): the spawn shape (`shape: box`, extents covering the visible volume, above the camera) is in emitter space, so the spawn volume follows the camera, while the particles simulate in world space and fall past it; a `killVolume` box with `invert: true` in emitter space (the same volume grown by the fall distance) kills particles the camera has left behind, and the ring's capacity is `rate × L_max` whatever the kill volume does (a death frees no slot), so ENV authors the lifetime as the spawn volume's fall time; each weather state's prefab names its own effect, so light and heavy rain are two documents and ENV writes no VFX component |
| no rain under roofs | the `precipitationOcclusion` update module, reading ENV's occlusion capture ("Precipitation occlusion" of `Zenith2/Docs/Design/VFX/Simulation.md`) |
| splashes where rain lands | the rain emitter's `collision` module with `depth: true` and `kill: true` (depth collision is GPU and world space only, which the rain emitter is), and a `collision` event edge to a splash emitter (`gpu`, `space: world`) spawning a few short-lived sprites at each record's position |
| wind | the `wind` update module, reading the frame environment's wind on the GPU |
| ground wetness and puddles | not particles: ENV's surface wetness, never a VFX decal |

Until ENV's occlusion capture exists, an effect with `precipitationOcclusion` runs with the module reading "not occluded" and counts `Flux.Particles.OcclusionUnavailable` (cross-area row VX6). Rain killed by occlusion, collision or the kill volume long before its lifetime still holds its ring slot until its ordinal comes round (cost V2 of `Zenith2/Docs/Design/VFX.md → "VFX rulings"`): capacity is `rate × L_max`, set by the authored lifetime alone, so the lifetime is authored as the volume's fall time, and the kill volume only removes particles the camera left behind. `Test_ParticleDraw_WeatherRainFollowsCamera` holds the surface.
