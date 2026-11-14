# Zenith2 rendering: frame anatomy for each shading path

Section file of `Zenith2/Docs/Rendering.md`. It details the entry-file sections "Frame anatomy common prologue", "Frame anatomy Forward+", "Frame anatomy Forward", "Frame anatomy Deferred", "Frame anatomy Visibility buffer" and "Frame anatomy common epilogue". Normative statements share the `REND` tag family of the entry file. Every pass named here is specified in the per-pass section files (see "Per-pass specifications" in the entry file).

## Reading the frame tables

| Column | Meaning |
|---|---|
| # | position in the path's declaration order; the compiled order is the topological order of "Graph compile" and may interleave independent passes |
| Pass | the pass name, which is also its heading in the per-pass section files; the view suffix (`[Main]`, `[Editor.2]`) is added at runtime |
| Feature function | the free function that declares the pass (or passes) over graph handles |
| Queue | G graphics, A may run on the async-compute queue where the graph finds a gap, C copy |
| Reads, Writes | the principal resources; the complete lists are the parameter struct fields and targets in the per-pass specification |

Stage-7 items are marked "(Stage 7)". Passes shared by every path appear once, in the common prologue or epilogue. The common prologue is the first group of GPU passes of a render frame, declared by the render subgraph's graph build; it is distinct from the frame prologue of `Zenith2/Docs/Architecture.md → "CPU frame graph and scheduler"`, the eight main-thread steps before `RunFrame`.

- [Z2-N-REND-723] Each shading path's frame function (`Flux_BuildForwardPlusView`, `Flux_BuildForwardView`, `Flux_BuildDeferredView`, `Flux_BuildVisibilityBufferView`, dispatched by `Flux_BuildView`) calls its feature functions in the order of its table below, and a feature whose view feature bit is clear, or which the capability matrix excludes, is not called.

## Common prologue

Run once per frame, not per view.

| # | Pass | Feature function | Queue | Reads | Writes |
|---|---|---|---|---|---|
| 1 | `GPUSceneGrow`, `GPUSceneScatter` | `Flux_AddGPUSceneUpdate` | G | scatter records in the upload ring | instance, primitive, material-slot and light tables |
| 1b | `BucketScatter` (frames whose draw-key bases were rebuilt) | `Flux_DrawKeyTable::AddScatterPass` | G | slot-command scatter records and the bucket base array in the upload ring | the slot-command and bucket-command-base buffers |
| 2a | `GeometryPoolGrow` (frames in which a geometry pool grows) | `Flux_AddStreamingUploads` | G | the old pool buffer | the new pool buffer |
| 2 | `StreamingUpload` | `Flux_AddStreamingUploads` | C | staging in the upload ring; retained mips of the current texture of a streaming swap | new persistent textures, mesh-pool regions, VT physical tiles and page table |
| 2b | `VideoConvert` (frames with an active video player; CINE's video system, recorded only when `Flux_FrameShared::m_pxVideo` is non-null) | CINE's internal video-system function | G | the video plane textures | the video surfaces |
| 2c | `VTPageTableUpdate` (Stage 4, frames with page-table records) | `Flux_AddVTPageTableUpdate` | G | the frame's page-table runs | the VT page table |
| 3 | `ComputeSkinning`, `MorphTargets` | `Flux_AddSkinning` | A | joint palettes, bind-pose streams, morph deltas | skinned-vertex pool (current half) |
| 4 | `ParticleEmit`, `ParticleSimulate`, `ParticleCompactAndSortKeys`, `ParticleSort` (in a frame carrying preview catch-up steps or steps kept from a deferred or failed frame, `ParticleEmit` and `ParticleSimulate` repeat once per step, emit then simulate in step order, at most `uFLUX_PARTICLE_MAX_STEPS_PER_FRAME` = 128 steps, as `ParticleEmitStep<s>` and `ParticleSimulateStep<s>` for s ≥ 1, before `ParticleCompactAndSortKeys` and the four `ParticleSort` passes) | `Flux_AddParticleSimulation` | A | emitter commands, particle pool, colliders, the sort view's previous HiZ | particle pool, sorted pairs |
| 5 | `ProceduralGeometry` | `Flux_AddProceduralGeometry` | A | procedural parameters | procedural-geometry pool |
| 5b | ENV's Stage 5 prologue passes: volumetric clouds, cloud shadow map, ocean FFT (with the Environment module) | `Env_AddVolumetricClouds`, `Env_AddCloudShadowMap`, `Env_AddOceanFFT` | A | ENV's state, cloud noise, the water spectrum | cloud history, cloud shadow map, ocean displacement |
| 6 | `LightDecalListBuild` | `Flux_AddLightDecalList` | G | light table, decal records, the union of view frusta, the resident probe records (Stage 4) | frame light list, frame decal list, frame probe list (Stage 4) |
| 6b | `SkyViewLUT` (steps transmittance and multiScatter on an `m_uSkyRevision` change, skyView every frame) | `Flux_AddSkyLUTs` | G | the atmosphere block | transmittance LUT, multiple-scattering LUT, sky-view LUT |
| 6c | ENV's cloud layer (with the Environment module, Stage 4) | `Env_AddCloudLayer` | G | the sky-view and transmittance LUTs, cloud noise | cloud layer, cloud transmittance |
| 6d | `SkyCapture`, `ProbeFilter` (Stage 4, frames that recapture the sky or a probe) | `Flux_AddSkyCapture`, `Flux_AddProbeFilter` | G | the sky LUTs, the star field, the cloud layer | the environment cube array, the sky irradiance SH |
| 7 | per shadow view: `ShadowInstanceCull`, `ShadowDepth`, optionally `ShadowHiZBuild` | `Flux_AddShadowView` | G | tables, pools, shadow-view records, previous shadow pyramid | shadow atlas tile, shadow visibility bits and pyramid |

- [Z2-N-REND-724] The common prologue runs once per frame in the order of the table above, before the first per-view pass, and its outputs reach every view through `Flux_FrameShared`.
- [Z2-N-REND-725] The shadow views of step 7 are exactly the shadow views of the frame's view list (see "Render views" in the entry file); each runs GPU culling (see "Hierarchical-Z occlusion culling") and then depth-only rendering into its shadow-atlas tile, with no lighting and no post-processing.
- [Z2-N-REND-726] Step 2c runs immediately after the virtual-texture tile uploads of step 2, so every material pass of every view reads the frame's page table; the per-view `VTFeedbackClear` runs before a feedback view's first writer and `VTFeedbackReadback` after its last, before `TemporalAA` (`Zenith2/Docs/Design/ADVGFX.md` → "Common prologue placement"). Step 2's copies complete on the copy queue and become visible to subsequent frames through the one-time acquire list of `Zenith2/Docs/Rendering/GpuResources.md` "Persistent resources"; no pass of the same frame reads a region that step 2 writes.

## Per-view prologue

| # | Pass | Feature function | Queue | Reads | Writes |
|---|---|---|---|---|---|
| 8 | `InstanceCullPhase1` | `Flux_AddInstanceCullPhase1` | G | tables, previous visibility bits, previous HiZ, previous view-projection | phase-1 indirect args, counts, draw records, visible-instance list |

- [Z2-N-REND-727] Instance culling phase 1 runs once per main, editor, capture and texture view, before that view's first depth-writing pass: frustum culling against the current camera, then HiZ against last frame's pyramid with bounds projected by last frame's view-projection.

## Early-Z ordering

- [Z2-N-REND-728] In every path, every depth-writing pass draws the opaque bucket first and then the alpha-tested bucket; the alpha-tested pipelines are the only pipelines allowed to `discard`.
- [Z2-N-REND-729] Every colour pass after the depth-writing passes of a view uses an `EQUAL` depth test with depth writes off and no discard, so early-Z stays enabled for all shading; the visibility-buffer path's resolve reads the visibility ID and has no depth test at all.

## Forward+ frame

Clustered; the default path and the one path on Android.

| # | Pass | Feature function | Queue | Reads | Writes |
|---|---|---|---|---|---|
| 1 | `ForwardPlusPrepass` (phase 1) | `Flux_AddForwardPlusPrepass` | G | phase-1 draws, tables, pools | depth, velocity RG16F, normals RG16_UNORM (octahedral), roughness R8_UNORM |
| 2a | `HiZBuild` | `Flux_AddHiZBuild` | G | depth | this frame's pyramid |
| 2b | `InstanceCullPhase2` | `Flux_AddInstanceCullPhase2` | G | tables, phase-1 draw bits, new pyramid | phase-2 draws, visibility bits |
| 2c | `ForwardPlusPrepass` (phase 2) | `Flux_AddForwardPlusPrepass` | G | phase-2 draws | depth, velocity, normals, roughness (`LOAD`) |
| 2d | `HiZBuild` (final) | `Flux_AddHiZBuild` | G | complete depth | final pyramid (persistent) |
| 3 | `LightClustering` | `Flux_AddLightClustering` | A | frame light and decal lists, depth range | froxel cluster lists |
| 4 | `VolumetricFog` | `Flux_AddVolumetricFog` | A | clusters, shadow atlas, fog settings, previous fog volume | integrated froxel volume (3D) |
| 5 | `GTAO` | `Flux_AddSSAO` | A | depth, normals | AO R8_UNORM (after bilateral denoise) |
| 6 | `ContactShadows` | `Flux_AddContactShadows` | A | depth, sun direction | contact-shadow mask R8_UNORM |
| 6b | `SSGI` (Stage 5, desktop tiers) | `Flux_AddSSGI` | G | depth, normals, velocity, final pyramid (min), previous SSR colour, previous SSGI history | SSGI history and result RGBA16F, half render extent |
| 7 | `ForwardPlusOpaque` | `Flux_AddForwardPlusOpaque` | G | draws, clusters, shadow atlas, IBL, AO, contact shadows, fog volume, decals, SSGI (Stage 5) | HDR colour R11G11B10F (depth `EQUAL`, read-only); specular weight R11G11B10F with SSR |
| 8 | `SkyAtmosphere` | `Flux_AddSkyAtmosphere` | G | sky-view LUT, transmittance LUT, fog volume, depth, cloud layer (Stage 4) | HDR colour (sky texels) |
| 9 | `SSR` | `Flux_AddSSR` | G | final pyramid (min), normals, roughness, previous SSR colour, IBL, specular weight | HDR colour (composited with IBL specular) |
| 10 | `OpaqueColourCopy`, `TransparentCull`, `TransparentForward` | `Flux_AddTransparentForward` | G | CPU-sorted transparent draws, clusters, fog volume, opaque colour copy | HDR colour (blended) |
| 11 | `Particles` | `Flux_AddParticleDraw` | G | particle pool, depth (soft particles), fog volume | HDR colour (blended) |
| 11b | ENV's underwater composite (a submerged view, with the Environment module) | `Env_AddUnderwater` | G | HDR colour, depth, the view's underwater medium | HDR colour |
| 12 | → common epilogue | | | | |

- [Z2-N-REND-737] The Forward+ frame declares its passes in the order of the table above.
- [Z2-N-REND-738] The Forward+ prepass writes depth, velocity, and thin normals plus roughness (RG16_UNORM octahedral normal and R8_UNORM roughness) for the screen-space passes, drawing the opaque bucket and then the alpha-tested bucket in each phase.
- [Z2-N-REND-739] Volumetric fog integrates into its froxel volume before any shading, and the opaque, transparent and particle passes all sample the integrated volume in-shader; no separate fog composite pass exists.
- [Z2-N-REND-740] Forward opaque uses the equal-depth test (no overdraw), runs `EvaluateMaterial` with clustered decals applied to `Flux_SurfaceData`, runs `EvaluateLighting` over the cluster lights, shadows, IBL and AO, applies froxel fog in-shader, and writes HDR colour.
- [Z2-N-REND-741] Sky and atmosphere render from the sky-view LUT with aerial perspective sampled from the froxel volume; SSR traces hierarchically against the final pyramid's min channel, samples the previous frame's colour, denoises, and composites with IBL specular.
- [Z2-N-REND-742] Transparent forward draws the CPU-sorted transparent instances back to front with clustered lighting and per-pixel froxel fog, with refraction reading a copy of the opaque colour made by `OpaqueColourCopy`; particles and ribbons follow, as soft particles against depth, with froxel fog.
- [Z2-N-REND-743] No pass of any path uses MSAA: antialiasing is TAA or the temporal upscaler, consistent with the non-MSAA prepass, HiZ and history.

## Forward frame

The non-clustered reference path, used for the parity reference, previews and captures.

| # | Pass | Feature function | Queue | Reads | Writes |
|---|---|---|---|---|---|
| 1 | `ForwardPlusPrepass` (phases 1 and 2), `HiZBuild`, `InstanceCullPhase2`, `HiZBuild` (final) | as Forward+ | G | as Forward+ | as Forward+ |
| 2 | `PerObjectLightLists` | `Flux_AddPerObjectLightLists` | G | frame light list, visible-instance list | per-instance light lists (at most the per-object light cap `Z2-B-RPROD-013`) and decal lists |
| 3 | `VolumetricFog`, `GTAO`, `ContactShadows` | as Forward+ | A | as Forward+ | as Forward+ |
| 3b | `SSGI` (Stage 5) | as Forward+ | G | as Forward+ | as Forward+ |
| 4 | `ForwardOpaquePerObject` | `Flux_AddForwardOpaquePerObject` | G | draws, per-object light and decal lists, shadow atlas, IBL, AO, fog volume, SSGI (Stage 5) | HDR colour; specular weight with SSR |
| 5 | `SkyAtmosphere`, `SSR`, `OpaqueColourCopy`, `TransparentCull`, `TransparentForward`, `Particles`, ENV's underwater composite | as Forward+ | G | as Forward+ | as Forward+ |
| 6 | → common epilogue | | | | |

- [Z2-N-REND-752] The Forward frame declares its passes in the order of the table above, reusing the Forward+ prepass, HiZ and culling passes unchanged.
- [Z2-N-REND-753] Per-object light lists hold at most the per-object light cap `Z2-B-RPROD-013` Per-object light cap (pending; proposed 8) per object, chosen on the CPU and the GPU from the light list: the CPU part is the frame light list itself (the lights the CPU keeps after its view-frustum test and shadowed-light selection, which `LightDecalListBuild` compacts), and the GPU part selects each visible object's lights from it by descending contribution at the object's bounds, with the directional light always included; transparent forward in the Forward path uses the same per-object lists.
- [Z2-N-REND-754] Volumetric fog integration runs before the opaque pass, then GTAO and contact shadows, as in Forward+; the opaque pass applies per-object lights, per-object decals and in-shader fog.

## Deferred frame

Desktop platforms.

| # | Pass | Feature function | Queue | Reads | Writes |
|---|---|---|---|---|---|
| 1 | `DeferredDepthPrepass` (phase 1), `HiZBuild`, `InstanceCullPhase2`, `DeferredDepthPrepass` (phase 2), `HiZBuild` (final) | `Flux_AddDeferredDepthPrepass` + shared | G | phase-1 and phase-2 draws | depth; pyramids |
| 2 | `GBuffer` | `Flux_AddGBuffer` | G | all draws (`EQUAL` depth, no discard) | GB0 albedo+AO, GB1 normal, GB2 roughness+metallic+specular+shading model, GB3 emissive, velocity; GB4 advanced-surface parameters in the advanced-surfaces permutation (Stage 5) |
| 3 | `DeferredDecals` | `Flux_AddDeferredDecals` | G | frame decal list, depth | GB0, GB1, GB2 (blended) |
| 4 | `GTAO`, `ContactShadows` | shared | A | depth, GB1 | AO, contact-shadow mask |
| 4b | `SSGI` (Stage 5) | shared | G | depth, GB1, velocity, final pyramid | SSGI result |
| 5 | `LightClustering`, `VolumetricFog` | shared | A | light and decal lists, shadow atlas | clusters, fog volume |
| 6 | `DeferredLighting` | `Flux_AddDeferredLighting` | G | G-buffer, depth, clusters, shadow atlas, IBL, AO, contact shadows, fog volume | HDR colour |
| 7 | `SkyAtmosphere`, `SSR` (G-buffer normals and roughness), `OpaqueColourCopy`, `TransparentCull`, `TransparentForward` (clustered, with fog), `Particles`, ENV's underwater composite | shared | G | as Forward+ | HDR colour |
| 8 | → common epilogue | | | | |

- [Z2-N-REND-762] The Deferred frame declares its passes in the order of the table above; the G-buffer pass uses an equal depth test and no discard, and `EvaluateMaterial` writes `Flux_SurfaceData` into the G-buffer.
- [Z2-N-REND-763] Deferred decals modify the G-buffer after the G-buffer pass and before any pass that reads it; deferred lighting is one compute pass over tiles and clusters that runs `EvaluateLighting` into HDR colour with froxel fog applied.
- [Z2-N-REND-764] Every G-buffer target is at most 16 bits per channel, with the formats fixed in `Zenith2/Docs/Rendering/PerPass/Deferred.md` "GBuffer".

## Visibility buffer frame

Desktop platforms.

| # | Pass | Feature function | Queue | Reads | Writes |
|---|---|---|---|---|---|
| 1 | `MeshletCullPhase1` | `Flux_AddMeshletCullPhase1` | G | tables, meshlets, previous visibility bits, previous pyramid | phase-1 visible-meshlet list, mesh-task or fallback args |
| 2 | `VisibilityRaster` (phase 1) | `Flux_AddVisibilityRaster` | G | phase-1 visible meshlets, pools | visibility ID R32_UINT (cleared to `0xFFFFFFFF`), depth |
| 3 | `HiZBuild`, `MeshletCullPhase2`, `VisibilityRaster` (phase 2), `HiZBuild` (final) | shared + VB | G | as above | as above |
| 4 | `MaterialClassify` | `Flux_AddMaterialClassify` | G | visibility ID, visible-meshlet list, material-slot table | per-tile material bins, per-material indirect dispatch args |
| 5 | `LightClustering` | shared | A | light and decal lists | clusters |
| 6 | `MaterialResolve` | `Flux_AddMaterialResolve` | G | visibility ID, bins, tables, pools, clusters (decals) | `SurfaceData` targets (same formats as the G-buffer, GB4 included in the advanced-surfaces permutation) |
| 7 | `VelocityReconstruct` | `Flux_AddVelocityReconstruct` | G | visibility ID, current and previous transforms, previous skinned positions | velocity RG16F |
| 8 | `GTAO`, `VolumetricFog` | shared | A | depth, `SurfaceData` normal | AO, fog volume |
| 8b | `SSGI` (Stage 5) | shared | G | depth, `SurfaceData` normal, velocity, final pyramid | SSGI result |
| 9 | `DeferredLighting` | shared | G | `SurfaceData`, depth, clusters, shadow atlas, IBL, AO, fog volume | HDR colour |
| 10 | `SkyAtmosphere`, `SSR`, `OpaqueColourCopy`, `TransparentCull`, `TransparentForward`, `Particles`, ENV's underwater composite | shared | G | as Forward+ | HDR colour |
| 11 | → common epilogue | | | | |

- [Z2-N-REND-773] The visibility-buffer frame declares its passes in the order of the table above.
- [Z2-N-REND-774] Meshlet culling phase 1 tests instances, then meshlets, for frustum, cone and HiZ against last frame's pyramid with the previous view-projection; the visibility raster draws opaque meshlets first and alpha-tested meshlets second, through mesh shaders or the compute/indirect fallback.
- [Z2-N-REND-775] The visibility ID is `(visibleMeshletIndex << 7) | triangleInMeshlet` in an R32_UINT target cleared to `0xFFFFFFFF` ("no geometry"), which allows up to 2²⁵ (33,554,432) visible meshlets and 128 triangles per meshlet; per-instance data is reached through the visible-meshlet list, because (instance, triangle) cannot fit 32 bits at 100k instances.
- [Z2-N-REND-776] Material classification bins every 8×8 pixel tile's material variants and emits one indirect dispatch argument set per material variant; the material resolve runs one compute dispatch per material bin, reconstructs attributes from barycentrics with analytic gradients, runs `EvaluateMaterial`, applies clustered decals to `Flux_SurfaceData`, and writes `Flux_SurfaceData` like a G-buffer.
- [Z2-N-REND-777] Velocity is reconstructed from the visibility ID and the previous transforms, including the skinned pool's previous positions; lighting is the deferred lighting pass, with froxel fog.
- [Z2-N-REND-778] Transparent geometry never enters the visibility buffer; it draws in the shared transparent forward pass with fog.

## Common epilogue

Run per view, after the path's last pass.

| # | Pass | Feature function | Queue | Reads | Writes |
|---|---|---|---|---|---|
| 1 | `TemporalAA` | `Flux_AddTemporalAA` | G | HDR colour, velocity, depth, previous TAA history | output-extent HDR colour, current TAA history RGBA16F |
| 2 | `AutoExposure` | `Flux_AddAutoExposure` | G | output-extent HDR colour, exposure history | luminance histogram, exposure buffer |
| 3 | `DepthOfField` (optional, per camera) | `Flux_AddDepthOfField` | G | colour, depth | colour |
| 4 | `MotionBlur` (optional) | `Flux_AddMotionBlur` | G | colour, velocity, depth | colour |
| 5 | `Bloom` | `Flux_AddBloom` | G | colour, exposure | bloom chain R11G11B10F |
| 6 | `TonemapAndGrade` | `Flux_AddTonemap` | G | colour, bloom, exposure, grading LUT | display-referred output (swapchain format or view texture) |
| 6b | `VideoComposite` (main views and the editor's game view, when `Flux_FrameShared::m_pxVideo` has an active player or an overlay record) | `Flux_AddVideoComposite` (CINE's) | G | output, video surfaces, screen overlay records | output |
| 6c | the view extensions at `FLUX_EXTENSION_POINT_BEFORE_UI` (output extent; where tonemapping would run when its bit is clear) | `Flux_CallViewExtensions` | G | output | output |
| 7 | `UIComposite` | `Flux_AddUIComposite` | G | UI draw lists, output | output |
| 8 | `DebugOverlay` | `Flux_AddDebugOverlay` | G | debug draws, profiler data | output |
| 8b | `HiZDebugOverlay` (development builds, the view `r.debug.hiz` selects) | `Flux_AddHiZDebugOverlay` | G | final pyramid, culled-rectangle list | output |
| 9 | `Present` | `Flux_AddPresent` | G | output | swapchain (main view) or the view's output texture |

- [Z2-N-REND-787] The common epilogue declares its passes per view in the order of the table above: TAA or temporal upscaler with jittered history, velocity reprojection, variance clipping and the dynamic-resolution upscale to output; auto-exposure (luminance histogram, then adaptation); optional depth of field (circle of confusion, gather, composite); optional motion blur (tile max velocity, then gather); bloom (downsample/upsample chain); tonemap and colour grading; the video composite and then the view extensions at `FLUX_EXTENSION_POINT_BEFORE_UI`, so 2D canvases draw over cinematic video and under the UI (`Zenith2/Docs/Design/RPROD/Passes.md` → "Common epilogue rows 6b and 6c"); UI composite; debug overlays and the on-screen profiler; present.
- [Z2-N-REND-788] Tonemap and grading applies an ACES-style operator, a 3D grading LUT, the output-transform selection (SDR, HDR10 or scRGB) and the colour-space conversion, with vignette and film grain optional.
- [Z2-N-REND-789] The UI composite blends the game UI over the output; world-space UI is drawn earlier, in the transparent pass, and is not drawn again.
- [Z2-N-REND-790] Present is the frame-pacing-controlled swapchain present for the main view, or a copy into the view's output texture for every other view, the editor's game view among them, which outputs a tool texture; a main view under a development capture presents a letterboxed copy of its capture output; frame pacing is specified in `Zenith2/Docs/Design/RPROD.md`.

## Async compute placement

- [Z2-N-REND-791] Exactly the passes marked A in the tables above (compute skinning and morph targets, the particle simulation passes, procedural geometry, ENV's Stage 5 prologue passes, light clustering, volumetric fog, GTAO and contact shadows, as the frame anatomy of every path fixes them) are declared with `FLUX_PASS_FLAG_ASYNC_COMPUTE`, and each runs on the async-compute queue exactly when the gap rule of `Zenith2/Docs/Rendering/RenderGraph.md` "Graph compile" (step 3) places it there; results are identical whichever queue runs it, which the parity suite checks by running every parity scene with `m_bAsyncCompute` on and off.
- [Z2-N-REND-792] Every pass marked G runs on the graphics queue and carries no `FLUX_PASS_FLAG_ASYNC_COMPUTE`, and a pass marked C runs on the copy queue when one exists.

<!-- elaboration -->
<!-- informative -->
Several compute passes marked G (the HiZ builds, SSR, deferred lighting, material classification and resolve, velocity reconstruction, and the TAA, exposure, depth-of-field, motion-blur and bloom dispatches) have no dependency that pins them to the graphics queue. Making any of them async-eligible changes the frame anatomy, so it is decided, with measurements, by the Stage 5 RPROD frame-anatomy completion ticket together with the D-TBDR measurement (open, Stage 5), not by a pass author.
<!-- /informative -->

## Environment frame slots

ENV's GPU passes are ZenithFlux functions ENV declares in `Flux_Environment.h`, so the path frame functions call them without a module edge; each adds no pass when its feature is absent from the view or the world, and a world without the Environment module adds none (`Zenith2/Docs/Design/RPROD/Passes.md → "Pass implementation rules"`; `Zenith2/Docs/Design/ENV.md → "Frame-level flow of ENV render work"`).

<!-- elaboration -->
| Pass | Slot | Paths |
|---|---|---|
| `Env_AddCloudLayer` (Stage 4) | once per frame as a shared producer, after the sky LUTs and before the first view's `Flux_AddSkyAtmosphere`; its outputs reach the sky draw and the sky capture through `Flux_FrameShared::m_xCloudLayer` and `m_xCloudTransmittance` | every path |
| `Env_AddUnderwater` (Stage 4) | per view, after `Flux_AddParticleDraw` and before the common epilogue, for a view whose `Flux_RenderEnvironmentView` record marks it submerged | every path |
| `Env_AddVolumetricClouds`, `Env_AddCloudShadowMap`, `Env_AddOceanFFT` (Stage 5, desktop high tier) | in the common prologue after `Flux_AddProceduralGeometry`, async-flagged | every desktop path |
| ENV's detail foliage generator | through `Flux_AddProceduralGeometry` (GEOM's), no slot of its own | every path |

- [Z2-N-REND-793] ENV's passes run exactly in the slots of the table above, each adds no pass when its feature is absent from the view or the world, and a frame of a world without the Environment module declares none of them.
- [Z2-N-REND-794] `Flux_AddSSGI` (Stage 5, LIGHT) runs after `Flux_AddContactShadows` (after `Flux_AddSSAO` on the visibility-buffer path, which has no contact-shadow pass) in every desktop path, on the queue G, and adds no pass on `FLUX_CAPABILITY_TIER_MOBILE` or when the view's tier leaves `render.quality.ssgiEnabled` unset.
