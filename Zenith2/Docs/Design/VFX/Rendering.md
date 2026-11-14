# VFX design: rendering

Section file of `Zenith2/Docs/Design/VFX.md`. It specifies how particles reach the screen: per-view emitter culling, the two draw partitions and every ordering rule, the draw records and indirect commands, the sprite, ribbon, trail and mesh draw programs, particle shading and lighting, soft particles, fog and pre-exposure, effect lights and effect decals, the target per-pass tables with their differences from the core tables, the PSO keys and permutations, the capability and tier rules, and the render-side failure behaviour. The draw pass is `Particles` of `Zenith2/Docs/Rendering/PerPass/Common.md → "Particles"`, declared by `Flux_AddParticleDraw` after `TransparentForward` in every path (`Zenith2/Docs/Rendering/FrameAnatomy.md → "Forward+ frame"`, [Z2-N-REND-742]).

## Scope of this file

| Topic | Heading |
|---|---|
| what happens per frame and per view | "Draw pipeline overview" |
| which emitters a view draws | "Per-view emitter culling" |
| in which order | "Draw ordering" |
| the GPU-visible draw data | "Draw records and indirect commands" |
| the four render kinds | "Sprite rendering", "Ribbon rendering", "Mesh particle rendering" |
| colour, lighting, softness, fog, blending | "Particle shading" |
| lights and decals effects place | "Effect lights", "Effect decals" |
| the passes field by field | "Pass tables" |
| programs and PSOs | "PSO keys and permutations" |
| paths, tiers and platforms | "Capability and quality tiers" |
| what goes wrong and what happens | "Render-side failure behaviour" |

## Draw pipeline overview

| When | Step | Owner |
|---|---|---|
| `Flux.BuildGraph` step 4 | `Flux_ParticleApplyRenderWorld`: commands into the instance table | VFX |
| step 7c | `Flux_ParticleBuildFrame`: admission, simulation batches, the sort view and ranks, then per view the culled emitter list, the draw records, the indirect commands and the PSO runs | VFX |
| step 8 | `Flux_AddParticleSimulation` (common prologue row 4) | VFX |
| step 9, per view | `Flux_AddParticleDraw` declares one raster pass `Particles` when the view's mask carries `FLUX_VIEW_FEATURE_PARTICLES` and the view has at least one draw command, and none otherwise ([Z2-N-REND-281]) | VFX |
| step 11 | `Flux_ParticleWriteFrameData`: commands, CPU particles, colliders, draw records and indirect commands written through `GetFrameData` | VFX |
| record | the `Particles` lambda binds each run's PSO and issues one `DrawIndexedIndirect` per run (count-less multi-draw, stride 24, [Z2-N-REND-069]) | VFX |

## Per-view emitter culling

A view draws particles when its feature mask carries `FLUX_VIEW_FEATURE_PARTICLES`: by default the main, editor, capture and texture views, never a shadow view (particles cast no shadow) and a preview view only when its request sets the bit, which the effect preview does (`Zenith2/Docs/Design/VFX/Authoring.md → "Preview session"`). For each such view, an emitter is drawn when all hold:

1. it is admitted, allocated, and its command carried `VISIBLE` this frame (or it is a paused emitter refused by the budget, which draws frozen);
2. its render kind is not `NONE`;
3. its instance's world bounds intersect the view frustum (`Zenith_Intersects(Zenith_Frustum, Zenith_AABB)`, `Zenith2/Docs/Design/CORE/Maths.md → "Bounds types"`), the frustum built from the view's unjittered view-projection; the instance's bounds are the command's `m_xWorldBounds` grown on every side by the largest wind cube of its emitters, `max_e Flux_ParticleWindExpansion(m_fWindGain_e, W_e)` with `W_e` the largest `Flux_ParticleWindBound` of the instance's world over the emitter's last `m_fBoundsHistorySeconds` of effect time, kept as a ring of nine buckets of `m_fBoundsHistorySeconds / 8` (the current bucket plus the eight full buckets before it, a bucket dropped once its end is older than `τ − m_fBoundsHistorySeconds`, so at least that whole history is covered) (`Zenith2/Docs/Design/VFX/PublicAPI.md → "Particle wind bound"`, "Bounds" of `Zenith2/Docs/Design/VFX/Simulation.md`), so particles carried by a strong wind are never culled while visible;
4. its world is the view's world (a world's emitters are drawn only by that world's views; the editor's views of the edit world draw the edit world's particles).

At most `uFLUX_PARTICLE_MAX_DRAWS_PER_VIEW` = 4,096 draw commands per view; beyond, the lowest-significance emitters are not drawn in that view (`Flux.Particles.ViewDrawsDropped`). Culling is CPU-side per emitter; the rasteriser clips individual particles, and a dead slot emits a degenerate primitive ("Sprite rendering" step 2).

## Draw ordering

### Partitions

| Partition | Emitters | Blend state | Order |
|---|---|---|---|
| 0, blended | every drawn emitter whose blend is `alpha` or `premultiplied` | one state for the whole partition: colour `ONE, ONE_MINUS_SRC_ALPHA`, write mask RGB; the `alpha` mode multiplies its colour by its opacity in the shader, so both modes share it and interleave freely in depth order | the view's emitter ranks, back to front ("Emitter ranks and the sort view"), each emitter's particles in its own order ("Order within an emitter") |
| 1, additive | every drawn emitter whose blend is `additive` | colour `ONE, ONE`, write mask RGB | ascending (world tag, instance key, emitter index), each emitter's particles oldest first unless `newestFirst` |

Partition 0 is drawn before partition 1, both after `TransparentForward` in the same pass, so particles draw over transparent geometry and additive particles over blended ones: the accepted cost of having no per-pixel order-independent transparency. Both blend states are entries of the fixed blend-state table of `Zenith2/Docs/Design/RPROD/PSO.md → "Fixed state tables"` (premultiplied and additive), which the target `R11G11B10F` HDR colour supports.

### Emitter ranks and the sort view

- The sort view is the frame's main view; without one, the editor view with the lowest viewport ID; without one, the first texture view, then the first capture view, then the first preview view in view-list order; a frame without a particle-drawing view has none and sorts nothing.
- Per drawing view, the partition-0 emitters are ranked by the view depth of their instance bounds' centre plus the emitter's `sortBias`, farthest first, ties by (world tag, instance key, emitter index); rank 0 is drawn first. Each view ranks its own emitters, so an editor view's emitter order is right for that view.
- The depth-sorted emitters' particle keys use the sort view's ranks and depths (one sort per frame, "ParticleCompactAndSortKeys dispatch" of `Zenith2/Docs/Design/VFX/Simulation.md`); another view draws each emitter's particles in the sort view's order. Views close to the sort view (editor game view, a mirror) look right; a view looking back at the sort view sees each depth-sorted emitter's particles in the reverse of its own depth order, which is the defined approximation.

### Order within an emitter

| Sort mode | Instance i of the emitter's draw reads |
|---|---|
| `depth` (sorted this frame) | the payload of sorted pair `m_uSortedBase + i` |
| `depth` refused by the sorted budget, and `oldestFirst` | slot `(m_uOrderStart + i) mod C`: from the oldest possible particle to the newest |
| `newestFirst` | slot `(m_uOrderStart − i) mod C`, `m_uOrderStart` the newest ordinal's slot |
| CPU emitters | record `m_uStateBase + i` of the frame's CPU-particle range (already oldest first), reversed for `newestFirst`; depth-sorted CPU emitters through their sorted segment like GPU emitters |

## Draw records and indirect commands

Per drawing view, `Flux_ParticleBuildFrame` writes, in draw order, one `Flux_ParticleDrawRecord` and one `Flux_IndirectDrawIndexed` per drawn emitter, built from the emitter's last step of the frame (`m_uCommandIndex` names that step's command; every earlier step is a catch-up step, simulated and never drawn):

| Render kind | `m_uIndexCount` | `m_uInstanceCount` | `m_uFirstIndex`, `m_iVertexOffset` | Index buffer |
|---|---|---|---|---|
| sprite | 6 | `C` (GPU) or the live count (CPU) | 0, 0 | the quad index buffer: one `Flux_PersistentReadOnlyBuffer` of six 16-bit indices `0, 1, 2, 2, 1, 3`, created by `EnableParticles` |
| ribbon | 6 | `C − 1` segments (GPU) or `live − 1` (CPU) | 0, 0 | the quad index buffer |
| trail | 6 | `C × T` segments | 0, 0 | the quad index buffer |
| mesh | the section's index count | `C` or the live count | the mesh part's first index and vertex offset in GEOM's pools | GEOM's index pool |

`m_uDrawIndex` is the command's index within the view's command array, so `Flux_DrawRecordIndex(m_uRecordBase)` finds its record ([Z2-N-REND-070], [Z2-N-REND-071]); `m_uFirstInstance` is 0. Consecutive commands sharing one PSO form a run (`Flux_ParticleDrawRun`); the lambda issues `DrawIndexedIndirect(commands of the run)` per run, binding the run's PSO, the quad index buffer or GEOM's index pool, and for a mesh run GEOM's raster pool streams. Instance counts are CPU-known, so no GPU-written count exists and a frame's draw cost is known before submission. `m_uTexture` is the sprite texture's current SRV index (`Flux_TextureResource::GetSRV()`, or the white fallback while it is not resident), read every frame, so a texture streaming swap never needs a rewrite of particle constants.

<!-- assumption: pending GEOM (cross-area row VX8) -->
A mesh run needs, from GEOM, `Flux_MeshDrawRange Flux_MeshResource::GetDrawRange(uint32 uLOD, uint32 uSection) const` (first index, index count, vertex offset in the raster pool, valid when `GetResidency()` is resident) and `Flux_VertexStreamBinding Flux_MeshManager::GetRasterPoolStreams(Zenith_VertexStreamSetCode xCode) const` (the `Flux_VertexStreamRef` of each fixed-function stream of the raster pool, `Zenith2/Docs/Design/GEOM/Meshes.md → "Geometry pools"`), so the particle pass declares the pool's streams in its `Flux_RasterTargets` like any raster pass ([Z2-N-REND-296]).
<!-- /assumption -->

## Sprite rendering

`Zenith2/Shaders/Passes/Particles/ParticleDraw.slang`, vertex shader per (instance i, corner c in 0 … 3):

1. Read the draw record (`Flux_DrawRecordIndex`), the emitter command and the state (GPU slot or CPU record, "Order within an emitter").
2. A slot that is not live (zero lifetime, generation mismatch, age at or past lifetime) writes `SV_Position = (0, 0, 0, 0)` for every corner, a degenerate primitive the rasteriser discards.
3. `a = age / lifetime`; colour = spawn colour × `colourOverLife(a)` × `colourBySpeed(|v|)`; size = spawn size × `sizeOverLife(a)` × `sizeBySpeed(|v|)`; the over-life values are `Flux_EvaluateValue` of the emitter's over-life blocks with x = a.
4. A local-space particle maps position and velocity to world through the current emitter rows.
5. The facing basis (right `R`, up `U`, normal `N`) of "Facing modes", rotated about `N` by the particle's rotation; the corner `(±1, ±1)` offset by `pivot`: `x = p + (cx − pivot.x) × size.x / 2 × R + (cy − pivot.y) × size.y / 2 × U`.
6. `SV_Position` = the view's jittered view-projection × x; the vertex outputs the colour, the sub-UV coordinates, the view-space linear depth, `N`, and for vertex lighting the lit colour ("Lighting modes").

### Facing modes

With `toCam` = normalize(camera position − p) and the view's right `Rv` and up `Uv`:

| Facing | `N`, `R`, `U` |
|---|---|
| `cameraPlane` | `N` = −view forward, `R = Rv`, `U = Uv` |
| `cameraPosition` | `N = toCam`, `R = normalize(cross(Uv, N))`, `U = cross(N, R)` |
| `velocity` | `U = normalize(v)` (`Uv` when `|v| < 10⁻⁴`), `R = normalize(cross(U, toCam))`, `N = cross(R, U)`; size.y += `|v| × stretch` |
| `axis` | `U` = the axis in world space, `R = normalize(cross(U, toCam))`, `N = cross(R, U)` |
| `world` | the axis is `N` in emitter space mapped to world; `R` = the emitter's X mapped and orthogonalised against `N`, `U = cross(N, R)` |

### Sub-UV flipbooks

With `columns × rows` cells and `frames` used: `overLife`: `φ = start + a × frames × loops`; `fps`: `φ = start + age × fps`; frame `F = floor(φ) mod frames`, next `(F + 1) mod frames`, blend `frac(φ)` when `blend` is set. A cell's UV is `((F mod columns) + u, (F / columns) + v) / (columns, rows)`. The texture is sampled at the mip level `log2(texels per pixel)` the vertex stage computes from the particle's projected size and the cell size (`Flux_SampleParticle`, an explicit-level sample), so no implicit derivative exists in any particle program.

## Ribbon rendering

### Emitter ribbons

`Zenith2/Shaders/Passes/Particles/RibbonDraw.slang`, segment instance i connects the particles of ordinals `q = q_oldest + i` and `q + 1`, `q_oldest = ordinalEnd − C` (GPU) or the CPU range's first record. A segment is degenerate unless both slots hold those exact ordinals (`m_uSpawnOrdinal`) and are live, so a dead particle breaks the strip and a wrapped ring never joins its head to its tail. Each endpoint's side vector is `normalize(cross(tangent, toCam)) × width / 2` (`facing: axis` uses the axis instead of `toCam`), the tangent a central difference over the neighbouring live particles (one-sided at the ends), width the particle's size x × `sizeOverLife`.

UV is defined over a span the CPU knows, so no GPU-written quantity is needed:

| Target | Span `[q_lo, q_hi]` |
|---|---|
| CPU emitter | exact: `q_lo` the ordinal of the first live record of the frame's CPU range, `q_hi` the last |
| GPU emitter | the possibly-live span: `q_hi = ordinalEnd − 1`; `q_lo = max(ordinalEnd − C, q_L)`, where `q_L` is the smallest ordinal spawned at or after effect time `τ − L_max − L_max / 32`. The particle system keeps, per GPU ribbon emitter in its instance record, 32 buckets of `L_max / 32` seconds of effect time, each holding the first ordinal spawned in it (from the commands' `m_uSpawnOrdinalBase` and `m_fEffectTime`; a `RESET` or generation change empties them); `q_L` is the first ordinal of the oldest bucket younger than `τ − L_max − L_max / 32`, conservative by one bucket. `Flux_ParticleBuildFrame` writes `ordinalEnd − q_lo` into the draw record's `m_uSortedBase` (ribbons are never depth-sorted) |

`stretch` gives `u = (q − q_lo) / max(q_hi − q_lo, 1)` along the strip, 0 at the oldest possibly-live particle and 1 at the newest; for a GPU emitter whose particles die before `L_max` the visible oldest particle has `u` slightly above 0 (at most the fraction of the span its early deaths cover), the defined approximation. `tile` gives `u = q × spacing / tileLength` for a distance-spawned ribbon (stretch otherwise); v is 0 and 1 across. `tailFade` multiplies the alpha by `u` under `stretch` and by `(q − q_lo) / max(q_hi − q_lo, 1)` under `tile`. `Test_ParticleDraw_RibbonUVModes` runs both modes on a CPU and a GPU emitter.

### Particle trail ribbons

The same program with `bPERM_TRAIL`: instance i is segment `i mod T` of slot `i / T`; endpoints are the particle's current position and its trail samples from the head backwards ("Particle trails" of `Zenith2/Docs/Design/VFX/Simulation.md`); width and alpha are the render module's curves at the sample index over T; a dead slot is degenerate.

## Mesh particle rendering

`Zenith2/Shaders/Passes/Particles/MeshParticleDraw.slang` draws the mesh's LOD 0 section 0 once per particle: the vertex position and normal decode through GEOM's mesh vertex function (`Zenith2/Shaders/Common/MeshVertex.slang`), scaled by size x, rotated by the particle's rotation about +Y or, with `alignToVelocity`, by the rotation taking +Z to the velocity, translated to the particle position. The depth test is the view's opaque compare with depth writes off, like every particle program, so mesh particles never occlude geometry drawn after them; opaque debris that must occlude and cast shadows is a physics entity, not a particle. A mesh not yet resident draws nothing (its emitter's draws are skipped, `Flux.Particles.MeshNotResident`).

## Particle shading

### Shade input and output

The pixel shader fills `Flux_ParticleShadeInput` (`Zenith2/Docs/Design/VFX/PublicAPI.md → "Flux_ParticleShadingType"`) and calls `Flux_ActiveParticleShading::Shade`; the engine type `ParticleShading_Standard` is the default. The result:

`colour_out = lit(colour × litFraction) + colour × (1 − litFraction) + emissive`, then pre-exposure, fog, softness and the blend mode below.

### Lighting modes

| Mode | Evaluation |
|---|---|
| `unlit` | `lit(x) = x` |
| `vertex` | once per vertex at the particle's centre (corners of one particle agree): `EvaluateLighting` (LIGHT's single-source function, pure and callable in the vertex stage, [Z2-N-REND-698]) with the cluster light source of the centre's projected pixel and linear depth, a `Flux_SurfaceData` of albedo 1, roughness 1, metallic 0, AO 1, normal `N`, `FLUX_SHADING_MODEL_DEFAULT_LIT`; the radiance multiplies the colour in the pixel shader |
| `pixel` | the same per pixel with the interpolated position and `N`, sampling shadows like a lighting pass |

Lit particles use their instance's lighting-channel mask (`m_uLayerMask` bits 24–31, `Zenith2/Docs/Design/LIGHT/Lighting.md → "Lighting channels"`) and read the light, cluster and shadow fields of the lit permutations ("Particles table"); direct light is never multiplied by AO, and particles never write `Flux_SurfaceData` anywhere: lighting is evaluated in the particle program and blended.

### Soft particles

With `softDistance` s > 0 (`bPERM_SOFT`): the scene's linear depth `z_s` from the view's depth SRV (linearised by `Flux_DepthModeOps`' shader twin, `Zenith2/Shaders/Common/DepthMode.slang`, [Z2-N-REND-595]), the fragment's `z_p`; opacity × `saturate((z_s − z_p) / s)`.

### Fog and pre-exposure

Radiance is pre-exposed in every render-extent pass (`Zenith2/Docs/Design/LIGHT/PostProcess.md`, the rule that covers particles): the shaded colour and emissive are multiplied by the frame's `m_fPreExposure`. With `bPERM_FOG`, using only LIGHT's `Flux_ApplyFroxelFog(colour, screenUV, z_p)` (`Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Fog application"`, result `colour × T + L`): `L = Flux_ApplyFroxelFog(0, screenUV, z_p)` and `c × T = Flux_ApplyFroxelFog(c, screenUV, z_p) − L`.

| Partition | Fogged output from the premultiplied colour `c_p` and opacity `o` |
|---|---|
| 0 (`alpha`, `premultiplied`) | `(c_p × T + L × o, o)`: the in-scattered light is scaled by the particle's coverage, so a transparent edge adds no fog of its own (for `alpha`, `c_p = c × o` is formed first, which is the same as fogging `c` before premultiplication) |
| 1 (`additive`) | `(c_p × T, 0)`: transmittance alone, because adding in-scattered light per additive layer would brighten fog wherever particles overlap |

### Blend modes

| Mode | Output `(rgb, a)` | Partition |
|---|---|---|
| `alpha` | `(c × o, o)` | 0 |
| `premultiplied` | `(c, o)`, the shading type returning premultiplied colour | 0 |
| `additive` | `(c × o, 0)` | 1 |

Particle programs never `discard` and never write depth (the depth target is bound read-only), so the early-Z rules of [Z2-N-REND-722] are untouched.

## Effect lights

For every emitter with a `light` module, `VFX.Update` evaluates, per frame, while the emitter spawns or has particles alive (spawning, then the release linger): intensity `I = Evaluate(intensity, x)` (x the emitter's normalised timeline), faded linearly to 0 over the linger after release; position = the emitter's world origin + rows × offset. `VFX.Extract` writes LIGHT's `Flux_RenderLightDelta` (`Zenith2/Docs/Design/LIGHT/Lighting.md → "The delta record"`): `ADD` the first frame, `UPDATE` each following frame, `REMOVE` the frame after the light ends; key `Flux_MakeNamedLightKey("Zenith.VFX.Light.<16 hex digits of the instance key>.<2 hex digits of the emitter index>", worldTag)`; type point, `m_uFlags` 0 (never shadow-casting), `m_uLightingChannels` the module's channels, range, source radius, the tint normalised to a largest component of 1 with the intensity scaled by that component (LIGHT's conversion, as RPROD's light extractor applies it). At most `vfx.maxEffectLights` lights per world, chosen by significance with ties by (instance key, emitter index); a light pushed out of the set is removed (`vfx.lights.dropped`).

## Effect decals

For every emitter with a `decal` module, each trigger (`start`, `stop`, or the timeline time) places one decal: `Flux_RenderDecal` (`Zenith2/Docs/Design/GEOM/Materials.md → "Decal records"`) written every frame of its lifetime by `VFX.Extract`, with `m_ulStableKey = Zenith_Hash64(instance key, emitter index, trigger ordinal)` (so decal order is deterministic, `Zenith2/Docs/Design/GEOM/Materials.md → "Decal order and application"`), `m_xDecalToWorld` the box of `size` at the emitter origin projected along world −Y (`down`) or the emitter's forward, rotated by a random angle when `random` is set, `m_xMaterial` the decal material dependency's `Flux_MaterialID`, `m_fOpacity` 1 until the last `fadeOut` seconds and then linear to 0, `m_uLayerMask` the instance's. Decals outlive their instance until their lifetime ends; at most `vfx.maxEffectDecals` per world, the oldest removed first (`vfx.decals.dropped`).

## Pass tables

<!-- elaboration -->
These are VFX's target tables for the five particle passes; they replace the rows of `Zenith2/Docs/Rendering/PerPass/Common.md` (headings "ParticleEmit", "ParticleSimulate", "ParticleCompactAndSortKeys", "ParticleSort", "Particles"), which [Z2-N-REND-800] binds, through cross-doc item X240, applied under the orchestrator's 2026-10-03 ruling accepting ring slots (V2 of `Zenith2/Docs/Design/VFX.md → "VFX rulings"`, with its costs); "Differences from the core pass tables" records every difference from the rows they replaced. X240 has replaced the core rows with these tables (core-doc patch e), so V2 is no longer an assumption on REND.


### ParticleEmit table

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/Particles/Emit.slang` → `Flux_ParticleEmitParams` |
| Feature function | part of `Flux_AddParticleSimulation` |
| Kind | compute, 64 threads per group, `Dispatch(m_uGroupCount)` per emission and event batch, one thread per new particle or per (slot, edge event, child); declared once per step of the frame (`ParticleEmit` for step 0, `ParticleEmitStep<s>` for step s ≥ 1, "Frame building" of `Zenith2/Docs/Design/VFX/Simulation.md`), over that step's batches |
| Queue | A |
| Capability | all tiers |
| Budget row | `Z2-B-VFX-003` Particle simulation GPU |
| Permutations | none at program level; the particle type variant is the key's variant ID: 0 (no update type) or the type ID of a code-authored update type the game's effects use (the prewarm integration runs the type) |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_xEmitterCommands` | `Zenith_RHISRV<Zenith_RHIStructBuffer<Flux_EmitterCommand>>` (frame chunk) | read | compute |
| `m_xParticleState` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` (the pool, state region) | read-write | compute |
| `m_xTrailSamples` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` `[Optional]` (trail region) | write | compute |
| `m_xEventRecords` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` `[Optional]` (event region) | read | compute |
| data | `uint32 m_uFirstCommand`, `m_uCommandCount`, `m_uStateRegionOffset`, `m_uTrailRegionOffset`, `m_uEventRegionOffset`, `m_uBatchKind` (0 spawns, 1 events); event freshness comes from each command's step tags, never from a frame index | — | — |

The effect constants are read through each command's `m_uEffectConstants` (a persistent read-only buffer, [Z2-N-REND-068]).

### ParticleSimulate table

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/Particles/Simulate.slang` → `Flux_ParticleSimulateParams` |
| Kind | compute, 64 threads per group, `Dispatch(m_uGroupCount)` per simulate batch, one thread per slot of every admitted emitter (direct dispatch: the CPU knows every count); declared once per step (`ParticleSimulate`, `ParticleSimulateStep<s>`), each after its step's `ParticleEmit`, so steps run in step order |
| Queue | A |
| Capability | all tiers |
| Budget row | `Z2-B-VFX-003` |
| Permutations | `bPERM_DEPTH_COLLISION` {false, true}; the particle type variant as above |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_xEmitterCommands` | as in ParticleEmit | read | compute |
| `m_xParticleState` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` | read-write | compute |
| `m_xTrailSamples` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` `[Optional]` | read-write | compute |
| `m_xEventRecords` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` `[Optional]` | write | compute |
| `m_xColliders` | `Zenith_RHISRV<Zenith_RHIStructBuffer<Flux_ParticleCollider>>` `[Optional]` (frame chunk) | read | compute |
| `m_xPrevDepth` | `Zenith_RHISRV<Zenith_RHITex2D>` `[Optional]` (the sort view's previous HiZ pyramid, max channel) | read | compute |
| `m_xFrameEnvironment` | as `Zenith2/Docs/Rendering/PerPass/Common.md → "Conventions"` (the frame environment: wind) `[Optional]` | read | compute |
| `m_xPrecipitationOcclusion` | `Zenith_RHISRV<Zenith_RHITex2D>` `[Optional]` (ENV's capture, Stage 5) | read | compute |
| `m_xBoundsValidation` | `Zenith_RHIUAV<Zenith_RHIStructBuffer<Flux_ParticleValidationRecord>>` `[Optional]` (development builds; bounds and live counts) | read-write | compute |
| data | `uint32 m_uFirstCommand`, `m_uCommandCount`, the three region offsets, `m_uFrameEnvironmentBase`; `Zenith_RHIFloat4x4 m_xPrevViewProjection`, `m_xPrevView`, `m_xPrevInvProjection`; `Zenith_RHIUInt2 m_xPrevDepthExtent`; `float m_fTimeSeconds` (wind time) | — | — |

### ParticleCompactAndSortKeys table

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/Particles/SortKeys.slang` → `Flux_ParticleSortKeysParams` |
| Kind | compute, 64 threads per group, `Dispatch(m_uGroupCount)` per sort-key batch, one thread per slot of every depth-sorted emitter |
| Queue | A |
| Budget row | `Z2-B-VFX-003` |
| Permutations | none |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_xEmitterCommands` | as above | read | compute |
| `m_xParticleState` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` | read | compute |
| `m_xCpuParticles` | `Zenith_RHISRV<Zenith_RHIStructBuffer<Flux_ParticleState>>` `[Optional]` (frame chunk) | read | compute |
| `m_xSortKeys` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` `[FullCoverage]` over `[0, 8 × N)` (transient) | write | compute |
| `m_xGlobalHistogram` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` (4 × 256 words, cleared by `AddClearBuffer`) | read-write | compute |
| data | `Zenith_RHIFloat4x4 m_xSortView`, `uint32 m_uFirstCommand`, `m_uCommandCount`, `m_uStateRegionOffset`, `m_uKeyCount` | — | — |

### ParticleSort table

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/Particles/RadixSort.slang` → `Flux_RadixSortParams` |
| Kind | compute, 256 threads per group, four digit passes ("ParticleSort algorithm" of `Zenith2/Docs/Design/VFX/Simulation.md`): `onesweep` one `Dispatch(ceil(N / 2048))` per pass; `reduceThenScan` (`Zenith2/Docs/Design/VFX/Simulation.md → "Reduce-then-scan fallback"`) `Dispatch(ceil(N / 2048))`, `Dispatch(1)`, `Dispatch(ceil(N / 2048))` per pass |
| Queue | A |
| Capability | all tiers; subgroup arithmetic (prefix sums), which the baseline profile carries on every supported device |
| Budget row | `Z2-B-VFX-003` |
| Permutations | `uPERM_PASS` {0, 1, 2, 3} × `uPERM_STAGE` {0 single pass, 1 reduce, 2 scan, 3 scatter} |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_xKeysIn` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` (ping-pong transient) | read | compute |
| `m_xKeysOut` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` `[FullCoverage]` over `[0, 8 × N)` | write | compute |
| `m_xGlobalHistogram` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` | read | compute |
| `m_xPartitionStatus` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` (`ceil(N / 2048) × 256` words, cleared by `AddClearBuffer` before each pass) | read-write | compute |
| `m_xTicket` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` (one word, cleared before each pass; stage 0) | read-write | compute |
| `m_xPartitionCounts` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` `[Optional]` (`ceil(N / 2048) × 256` words; stages 1 … 3, transient) | read-write | compute |
| data | `uint32 m_uDigitShift`, `m_uKeyCount`, `m_uPartitionCount` | — | — |

### Particles table

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/Particles/ParticleDraw.slang`, `RibbonDraw.slang`, `MeshParticleDraw.slang` → `Flux_ParticleDrawParams`, `Flux_RibbonDrawParams`, `Flux_MeshParticleDrawParams` |
| Feature function | `void Flux_AddParticleDraw(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_ViewTargets& xTargets)` |
| Kind | raster; per run one `DrawIndexedIndirect` over the CPU-written commands of the view in the frame chunk (count-less), partition 0 then partition 1 |
| Queue | G |
| Capability | all tiers |
| Budget row | `Z2-B-VFX-004` Particles and ribbons draw GPU |
| Permutations | "PSO keys and permutations" |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_xParticleState` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` (the pool) | read | vertex |
| `m_xTrailSamples` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` `[Optional]` | read | vertex |
| `m_xCpuParticles` | `Zenith_RHISRV<Zenith_RHIStructBuffer<Flux_ParticleState>>` `[Optional]` (frame chunk) | read | vertex |
| `m_xSortedIndices` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` `[Optional]` (`Flux_FrameShared::m_xParticleSorted`) | read | vertex |
| `m_xEmitterCommands`, `m_xDrawRecords` | `Zenith_RHISRV<Zenith_RHIStructBuffer<…>>` (frame chunk) | read | vertex, pixel |
| `m_xDepth` | `Zenith_RHISRV<Zenith_RHITex2D>` `[Optional]` (soft permutations) | read | pixel |
| `m_xFogVolume` | `Zenith_RHISRV<Zenith_RHITex3D>` `[Optional]` | read | pixel |
| `m_xClusterLists`, `m_xLightTable` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` `[Optional]` (lit permutations) | read | vertex, pixel |
| the fields of `Zenith2/Docs/Rendering/PerPass/Common.md → "Lighting pass fields"` | as listed there, all `[Optional]` (lit permutations) | read | vertex, pixel |
| indirect arguments | `Flux_IndirectArgsRef` in the targets (the frame chunk range of the view's commands) | `INDIRECT_ARGS` | — |
| vertex and index buffers | `Flux_IndexBufferRef` (the quad index buffer, or GEOM's index pool for mesh runs); `Flux_VertexStreamRef` of GEOM's raster pool streams (mesh runs) | `VERTEX`, `INDEX` | — |
| data | `uint32 m_uRecordBase`, the three region offsets, `m_uCpuParticleBase`, `m_uDebugFlags` (`Flux_ParticleDebugFlags`, 0 in Shipping); `float m_fPreExposure`; the cluster mapping (lit permutations, `Zenith2/Docs/Rendering/PerPass/Common.md → "Conventions"`); `Zenith_RHIFloat4x4 m_xViewProjection` (jittered), `m_xView`; `Zenith_RHIFloat3 m_xCameraPositionWS` | — | — |

| Target | Format | Load / store |
|---|---|---|
| HDR colour | R11G11B10F | `LOAD`, `STORE`; partition blend state |
| depth | D24S8 / D32F_S8 | read-only, test `Flux_DepthModeOps::GetOpaqueCompare` (`LESS_OR_EQUAL` standard, `GREATER_OR_EQUAL` reversed), no write |

### Differences from the core pass tables

| Pass | Core text | VFX table | Reason |
|---|---|---|---|
| all four simulation passes | an alive and dead list (`m_xAliveIn`, `m_xAliveOut`, `m_xDeadList`) with `DispatchIndirect` over GPU-written alive counts | ring slots by spawn ordinal, direct dispatches sized on the CPU, no alive or dead list | determinism: atomic list appends order particles by completion, which makes draw order, blending and the aliasing and async parity of [Z2-N-REND-433] and [Z2-N-REND-791] non-reproducible; exact CPU-known work is also what the budget rows are calibrated against |
| `ParticleEmit` | `m_xDeadList` | trail and event fields; the update-type permutation | as above; prewarm and events |
| `ParticleSimulate` | `m_xPrevDepth` = the main view's previous HiZ | the sort view's (the main view when present); colliders, frame environment, occlusion, validation fields | collision against colliders, wind, rain occlusion |
| `ParticleCompactAndSortKeys` | writes `m_xDrawArgs`, reads `m_xAliveOut` | writes the global digit histogram; no draw arguments | draw counts are CPU-known; the histogram feeds the single-pass sort |
| `ParticleSort` | `m_xHistogram` cleared, read-write | the global histogram read, a partition-status array and a ticket counter cleared per pass | the decoupled look-back of the stable sort |
| `Particles` | `DrawIndexedIndirectCount` from the pool's GPU-written arguments; blend permutation {alpha, premultiplied, additive}; no lighting fields | CPU-written count-less `DrawIndexedIndirect` runs; two blend partitions; the mesh program; lit permutations with LIGHT's lighting fields; `m_fPreExposure` | lit particles (LIGHT's row naming "VFX's lit-particle passes"), one blend state per partition, mesh particles |
| `Zenith2/Docs/Rendering/GpuResources.md → "Pools"` (particle row layout) | "particle state, alive lists, sort keys, per-emitter indirect args" | "particle state, trail samples, event records" (sort keys are graph transients) | as above |
| `Zenith2/Docs/Rendering/FrameAnatomy.md → "Common prologue"` row 4 | reads "emitter commands, particle pool, depth of last frame for collision"; writes "particle pool, alive lists, sort keys, particle indirect args"; one emit and one simulate pass | reads emitter commands, the pool, colliders, the sort view's previous HiZ; writes the pool and the sorted pairs; in a frame carrying preview catch-up steps or steps kept from a deferred frame, `ParticleEmit` and `ParticleSimulate` repeat once per step (at most `uFLUX_PARTICLE_MAX_STEPS_PER_FRAME` = 128) before `ParticleCompactAndSortKeys` and the sort (cross-doc item X240 (7)) | as above; exact seeks and deferred seeks |

## PSO keys and permutations

| Program | Permutation constants |
|---|---|
| `Emit.slang` | the update type variant |
| `Simulate.slang` | `bPERM_DEPTH_COLLISION` × the update type variant |
| `SortKeys.slang` | none |
| `RadixSort.slang` | `uPERM_PASS` {0 … 3} × `uPERM_STAGE` {0 … 3} |
| `ParticleDraw.slang` | `uPERM_LIGHTING` {0 unlit, 1 vertex, 2 pixel} × `bPERM_SOFT` × `bPERM_FOG` × `uPERM_PARTITION` {0, 1} × `bPERM_SUBUV_BLEND` × the shading type variant |
| `RibbonDraw.slang` | `uPERM_LIGHTING` × `bPERM_SOFT` × `bPERM_FOG` × `uPERM_PARTITION` × `bPERM_TRAIL` × the shading type variant |
| `MeshParticleDraw.slang` | `uPERM_LIGHTING` × `bPERM_FOG` × `uPERM_PARTITION` × the mesh stream-set × the shading type variant |

- The update type and the shading type are link-time specialisations ([Z2-N-REND-243]'s model applied to particle types), carried in `Zenith_RHIPermutationKey::m_ulMaterialVariantID` exactly as a material variant is (`Zenith2/Docs/Rendering/ShaderSystem.md`, the permutation key): variant 0 is the engine default (no update type; `ParticleShading_Standard`), compiled at build time with the engine programs; every other variant is a type ID (`Zenith_Hash64` of `"<module path without extension>/<Type>"`), compiled at cook time against the game's particle type set ([Z2-N-REND-124]). The cook's particle type set step (`Zenith2/Docs/Design/VFX/Authoring.md → "Particle type set step"`) enumerates exactly the (program, program key, type variant) triples the game's effects use, and a used triple missing from the compiled set is a cook error ([Z2-N-REND-125]; cross-doc item X241).
- The facing mode, sort mode, sub-UV mode and space are per-draw data (group- and draw-uniform branches), never permutations.
- The lighting-mode clamp is applied when `Flux_ParticleBuildFrame` forms the draw runs: an emitter's `uPERM_LIGHTING` is `min(authored lighting, m_eMaxLighting)`, so a tier change that lowers `vfx.maxLightingMode` moves the emitter to the run of the lower permutation from the next built frame; the particle type set step lists every lower mode of each emitter (`Zenith2/Docs/Design/VFX/Authoring.md → "Particle type set step"`), so the clamped PSO is always precached.
- A draw PSO is keyed by (program, permutation, raster state, attachment formats) ([Z2-N-REND-190]): cull none (sprites and ribbons are two-sided by construction; the back-face lint applies to opaque PSOs alone), depth test on with writes off, the partition's blend state, R11G11B10F colour and the view's depth format. The precache set of every game includes the particle PSOs its cooked effects can bind for each enabled path and depth format (cross-area row VX15), so a particle draw never meets a missing PSO in a precached run; a miss follows [Z2-N-REND-192] (skip the run in shipping, magenta in development).
- The permutation count joins the game's total under `Z2-B-RPROD-024` Shader permutation limit per game.

## Capability and quality tiers

| Rule | Value |
|---|---|
| paths | every path draws particles identically (`Zenith2/Docs/Rendering.md → "Shading paths and capability matrix"`, row "Particles and ribbons (soft)", parity by final image) |
| platforms | Windows (Vulkan, D3D12) and the S24; every program uses half precision for colour and shading, `float` for positions, depths and accumulation ([Z2-N-REND-922]) |
| tier keys | `vfx.maxLightingMode` (`pixel` desktop, `vertex` S24 default tier: a pixel-lit emitter draws vertex-lit), `vfx.lodBias` (0 desktop, 0.5 S24), `vfx.spawnScale` (1 desktop, 0.75 S24), the budgets of "Configuration keys" in `Zenith2/Docs/Design/VFX.md`; all admit the quality-tier layer and need no restart: the simulation-side keys reach `VFX.Update` through the module's change callback and the render-side keys reach the particle system through `Flux_Renderer::ApplyConfigChanges` ("Runtime configuration changes" of `Zenith2/Docs/Design/VFX.md`) |
| async compute | the four simulation passes carry `FLUX_PASS_FLAG_ASYNC_COMPUTE` ([Z2-N-REND-791]) |
| headless | no particle system exists; the module's simulation side and the command stream still run |

## Render-side failure behaviour

| Failure | Behaviour | Counter |
|---|---|---|
| a command that fails validation | dropped; the emitter keeps its previous state | `Flux.Particles.CommandsRejected` |
| the effect asset is not ACTIVE yet on the render side | the instance waits, drawing nothing | `Flux.Particles.WaitingAsset` |
| the effect asset FAILED | the instance is destroyed; its world's next commands for it are rejected | `Flux.Particles.EffectFailed` |
| the effect constants buffer cannot be created | the instance waits and retries next frame | `Flux.Particles.ConstantsFailed` |
| a pool region at its maximum | the emitter stays unallocated (not simulated, not drawn) and retries at each active command | `Flux.Particles.PoolFull` |
| the instance table full (`m_uMaxInstances`) | the new instance is refused | `Flux.Particles.InstancesFull` |
| a texture not resident or failed | the white fallback texture | `Flux.Particles.TextureFallback` |
| a mesh not resident | the emitter's mesh draws are skipped | `Flux.Particles.MeshNotResident` |
| a missing PSO | [Z2-N-REND-192] | RPROD's PSO counters |
| a deferred or failed frame | the pending list ("Deferred and failed frames" of `Zenith2/Docs/Design/VFX/Simulation.md`) | `Flux.Particles.PendingDropped` |
| device loss | "Device loss" of the same file | `Flux.Particles.DeviceResets` |
