# Zenith2 rendering: per-pass specifications, shared passes

Section file of `Zenith2/Docs/Rendering.md`. It details the entry-file section "Per-pass specifications" for every pass that appears in more than one shading path, plus the prologue, shadow, culling and epilogue passes. Normative statements share the `REND` tag family of the entry file.

## Conventions

Each pass has one `###` heading, its pass name. Under it:

| Table | Columns |
|---|---|
| Summary | program (shader file and generated params type), feature function, kind, queue, dispatch or draws, capability requirements, budget row, permutations |
| Parameters | every resource field of the parameter struct (field, C++ type, access, stages) and the plain-data fields in one row |
| Targets | raster and fullscreen passes: every attachment with its format and load/store ops; compute passes: the principal UAV outputs with their formats |

Queue abbreviations are those of `Zenith2/Docs/Rendering/FrameAnatomy.md` "Reading the frame tables". "Render extent" and "output extent" are defined in "Render views" of the entry file. Budget rows are cited by `Z2-B-` ID and name from `Zenith2/Docs/Budgets.md`; each GPU-time row is pending until the Stage 1 per-system budget ticket for its area allocates it. Every feature function returns the handles its consumers need and adds no pass when its feature is disabled.

- [Z2-N-REND-800] Every pass below is implemented with exactly the program, parameter fields (names, C++ types, access and stages), targets and formats, queue, capability requirements and permutations its tables give; a change to any of them edits this file in the same diff (Z2-D-085).
- [Z2-N-REND-801] Every render target and storage texture listed in this file and the other per-pass files uses at most 16 bits per channel, except the visibility ID (R32_UINT) and justified D32F depth, which is the enforcement surface of `FLUX_RG_ERROR_32BIT_TARGET_UNJUSTIFIED`.

### Stage marks frame data and the frame environment

A field, target or permutation marked "(Stage N)" joins its pass in the stage named and is absent before it; every unmarked entry exists from the stage that lands the pass.

<!-- elaboration -->
| Convention | Rule |
|---|---|
| frame-data ranges | a field that reads build-node frame data (shadow-view records, view frusta, probe records, local fog volumes, irradiance-volume records, transparent entries, scatter records) binds the frame chunk of `Flux_UploadRing::ImportFrameChunk`, the same `Flux_RGBuffer` on every call of a frame, and a plain-data `uint32 m_u<Name>Base` element base locates the range its producer declared (`Zenith2/Docs/Rendering/GpuResources.md` "Build-node upload order") |
| frame environment | every mesh pass that draws a material type whose vertex modifier or `Evaluate` reads the frame environment declares `m_xFrameEnvironment` (`Zenith2/Docs/Rendering.md → "Materials"`, subsection "Material vertex modifier"): `Zenith_RHISRV<Zenith_RHIStructBuffer<Env_FrameEnvironment>>` `[Optional]`, the frame's 128-byte `Env_FrameEnvironment` records (ENV's `[FluxShared]` struct of `Zenith2/Shaders/Common/Environment.slang`, one per world, written by `Flux_EnvironmentRenderer::WriteFrameEnvironment` as frame data) with the plain-data `uint32 m_uFrameEnvironmentBase` (the record of the pass's world); a world without the Environment module leaves the field unset, so the material reads slot 0's zeroed null substitute (no terrains, no water, zero wind) |
| cluster mapping | every caller of `Flux_ClusterIndex` carries the plain data `Zenith_RHIUInt3 m_xClusterGrid`, `Zenith_RHIUInt2 m_xClusterTileSize` and `Zenith_RHIFloat2 m_xClusterDepthRange` (n, f), written by the caller of `Flux_AddLightClustering` from the same cluster layout as the build step (`Zenith2/Docs/Design/LIGHT/Lighting.md → "Cluster grid"`) |
| lighting passes | "every lighting pass" is ForwardPlusOpaque, ForwardOpaquePerObject, DeferredLighting and TransparentForward: the passes that call `EvaluateLightingEx`; their shared LIGHT fields are listed once, in "Lighting pass fields" below |

- [Z2-N-REND-806] A pass that reads build-node frame data, the frame environment or the cluster mapping declares exactly the fields of the conventions table above, and a field marked with a stage is absent from the pass before that stage.

### Lighting pass fields

Every lighting pass, and `VolumetricFog` where marked, carries these fields beside those of its own table (`Zenith2/Docs/Design/LIGHT.md → "Rendering table additions pending X103"`):

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_xLightList` (also `VolumetricFog`) | `Zenith_RHISRV<Zenith_RHIByteBuffer>` (the frame light list: word 0 local count, word 1 directional count and roles, words 2–3 directional light-table indices) | read | pixel or compute |
| `m_xTransmittanceLUT` (also `VolumetricFog`) | `Zenith_RHISRV<Zenith_RHITex2D>` (`Flux_FrameShared::m_xTransmittanceLUT`) | read | pixel or compute |
| `m_xShadowAtlas`, `m_xShadowViews` (also `VolumetricFog`) | `Zenith_RHISRV<Zenith_RHITex2D>`, `Zenith_RHISRV<Zenith_RHIStructBuffer<Flux_ShadowViewRecord>>` `[Optional]` both: a frame with no shadowed light has no atlas and sets neither | read | pixel or compute |
| `m_xCloudShadowMap` (also `VolumetricFog`; Stage 5) | `Zenith_RHISRV<Zenith_RHITex2D>` `[Optional]` (ENV's cloud shadow map) | read | pixel or compute |
| `m_xIBLSpecular`, `m_xIBLIrradiance`, `m_xProbeRecords` (Stage 4) | `Zenith_RHISRV<Zenith_RHITexCubeArray>` (the environment cube array), `Zenith_RHISRV<Zenith_RHIByteBuffer>` (the sky irradiance SH), `Zenith_RHISRV<Zenith_RHIStructBuffer<Flux_ReflectionProbeRecord>>` `[Optional]`; before Stage 4 `m_xIBLSpecular` and `m_xIBLIrradiance` are persistent `Zenith_RHIDescriptorHandle` values | read | pixel or compute |
| `m_xGIVolumes`, `m_xSSGI` (Stage 5) | `Zenith_RHISRV<Zenith_RHIStructBuffer<Flux_IrradianceVolumeRecord>>` (frame data), `Zenith_RHISRV<Zenith_RHITex2D>` (the SSGI result, half render extent) `[Optional]` both | read | pixel or compute |
| `m_xLightmapPages` (ForwardPlusOpaque, ForwardOpaquePerObject, TransparentForward, the G-buffer pass and the material resolve; Stage 5) | `Zenith_RHISRV<Zenith_RHIStructBuffer<Flux_LightmapPageRecord>>` `[Optional]` | read | pixel or compute |
| data | `Flux_AtmosphereGPU m_xAtmosphere` (planet and atmosphere geometry, Rayleigh, Mie and ozone media, enabled flag); `Zenith_RHIFloat4 m_xCloudShadowMapping` (origin x, origin z, 1 / extent or 0 without a map, outside transmittance); `uint32 m_uFrameIndex`; `uint32 m_uShadowViewBase`, `m_uProbeRecordBase`, `m_uGIVolumeBase` (frame-data element bases); `uint32 m_uGIVolumeCount` (Stage 5); the cluster mapping where the pass is clustered (also `VolumetricFog`) | — | — |

- [Z2-N-REND-807] Every lighting pass, and `VolumetricFog` for the rows marked so, declares the fields of the table above in addition to its own table's, with the access, attributes and stages given, so that `EvaluateLightingEx` and the froxel injection receive the directional list, the atmosphere and the shadow data through the same fields on every path.

## Prologue passes

### GPUSceneGrow

| Property | Value |
|---|---|
| Program | none (a graph buffer copy) |
| Feature function | part of `void Flux_AddGPUSceneUpdate(Flux_RenderGraph& xGraph, Flux_GPUScene& xScene, Flux_FrameShared& xShared)` |
| Kind | copy (`AddCopyBuffer`), one per table that grows this frame |
| Queue | G |
| Capability | all tiers |
| Budget row | `Z2-B-RPROD-004` GPU-scene scatter upload GPU |
| Permutations | none |

| Resource | Access |
|---|---|
| old table buffer | `COPY_SOURCE` |
| new table buffer | `COPY_DEST`, full coverage of the old contents' range, the remainder written by the scatter or never read |

### GPUSceneScatter

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/GPUScene/Scatter.slang` → `Flux_GPUSceneScatterParams` |
| Feature function | `Flux_AddGPUSceneUpdate` (above) |
| Kind | compute, 256 threads per group, `ceil(totalWords / 256)` groups |
| Queue | G (first pass of the frame) |
| Capability | all tiers |
| Budget row | `Z2-B-RPROD-004` GPU-scene scatter upload GPU |
| Permutations | none |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_xRecords` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` (`SRV(chunk, range)` of the imported upload-ring chunk) | read | compute |
| `m_xInstances` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` | write | compute |
| `m_xPrimitives` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` | write | compute |
| `m_xMaterialSlots` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` | write | compute |
| `m_xLights` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` | write | compute |
| data | `uint32 m_uRecordsOffset`, `m_uRecordCount`, `m_uTotalWords`, `m_uTableMask` | — | — |

A table field whose bit in `m_uTableMask` is clear is `[Optional]` and left unset, so the pass declares no write on that table.

### StreamingUpload

| Property | Value |
|---|---|
| Program | none (copies) |
| Feature function | `void Flux_AddStreamingUploads(Flux_RenderGraph& xGraph, Flux_StreamingUploader& xUploader)` |
| Kind | copy passes (`FLUX_PASS_FLAG_COPY_QUEUE`): buffer-to-texture and buffer-to-buffer from upload-ring staging, and texture-to-texture copies of a streaming swap's retained mips from the current persistent read-only texture (a read of the source, consistent with [Z2-N-REND-547]) |
| Queue | C (G when no copy queue exists) |
| Capability | all tiers |
| Budget row | `Z2-B-GEOM-003` Streaming upload bytes per frame |
| Permutations | none |

| Resource | Access |
|---|---|
| staging (upload ring) | host-written, read by the copy |
| the current texture of a streaming swap (retained mips) | `COPY_SOURCE`, persistent read-only |
| new persistent textures, mesh-pool regions, material-constants blocks, VT physical tiles and page-table regions | `COPY_DEST`; unreferenced by any table until acquired (see `Zenith2/Docs/Rendering/GpuResources.md` "Persistent resources") |

### GeometryPoolGrow

| Property | Value |
|---|---|
| Program | none (a graph buffer copy) |
| Feature function | part of `Flux_AddStreamingUploads`, declared before the frame's `StreamingUpload` pass (`Zenith2/Docs/Design/GEOM/Meshes.md → "Pool growth"`) |
| Kind | copy (`AddCopyBuffer`), one per geometry pool buffer that grows this frame (raster, skin-source, index, meshlet and morph pools; the procedural pool by its two-region variant) |
| Queue | G |
| Capability | all tiers |
| Budget row | `Z2-B-GEOM-003` Streaming upload bytes per frame |
| Permutations | none |

| Resource | Access |
|---|---|
| old pool buffer | `COPY_SOURCE` |
| new pool buffer | `COPY_DEST`, the old contents' range; the rest is written by later uploads into unreferenced regions |

Primitive and skinning dispatch records keep their element offsets across growth, so no record changes; every subsequent import and vertex-stream binding uses the new buffer, and the old one retires through deferred deletion.

### ComputeSkinning

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/Skinning/Skinning.slang` → `Flux_SkinningParams` |
| Feature function | `void Flux_AddSkinning(Flux_RenderGraph& xGraph, const Flux_SkinningBatch& xBatch, Flux_FrameShared& xShared)` |
| Kind | compute, 64 threads per group, one thread per vertex, one dispatch per skinned-mesh layout (stream-set) |
| Queue | A |
| Capability | all tiers |
| Budget row | `Z2-B-GEOM-004` Compute skinning and morph targets GPU (the W-Characters workload sizes it) |
| Permutations | `uPERM_WEIGHTS_PER_VERTEX` {4}, `bPERM_JOINTS16` {false, true}, `bPERM_WEIGHTS16` {false, true}, `bPERM_POSITION32` {false, true} |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_xBindPositions`, `m_xBindSurface`, `m_xSkinStream` | `Zenith_RHIDescriptorHandle` (unified stream pools, persistent read-only) | read | compute |
| `m_xJointPalettes` | `Zenith_RHISRV<Zenith_RHIStructBuffer<Zenith_RHIFloat3x4>>` (upload ring, per frame) | read | compute |
| `m_xDispatchRecords` | `Zenith_RHISRV<Zenith_RHIStructBuffer<Flux_SkinDispatchRecord>>` | read | compute |
| `m_xSkinnedCurrent` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` `[FullCoverage]` over the slots it writes | write | compute |
| data | `uint32 m_uVertexCount`, `m_uRecordCount` | — | — |

| Output | Format |
|---|---|
| skinned-vertex pool, current half | per vertex: position FLOAT16x4 (FLOAT32x4 fallback layout), QTangent SNORM16x4, re-encoded with the bias rule (see `Zenith2/Docs/Rendering/VertexData.md` "Skinned vertex pool") |

### MorphTargets

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/Skinning/Morph.slang` → `Flux_MorphParams` |
| Feature function | part of `Flux_AddSkinning`; runs before `ComputeSkinning` for meshes with active morph weights, into a morph scratch transient that skinning reads |
| Kind | compute, 64 threads per group |
| Queue | A |
| Capability | all tiers |
| Budget row | `Z2-B-GEOM-004` Compute skinning and morph targets GPU |
| Permutations | `bPERM_POSITION32` {false, true} |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_xMorphDeltas` | `Zenith_RHIDescriptorHandle` (persistent read-only sparse delta buffer) | read | compute |
| `m_xWeights` | `Zenith_RHISRV<Zenith_RHIStructBuffer<float>>` | read | compute |
| `m_xMorphedScratch` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` `[FullCoverage]` | write | compute |

<!-- informative -->
GEOM fixes the skinning permutation mapping (`Zenith2/Docs/Design/GEOM/VertexFormat.md → "Program stream-set specialisation"`): `bPERM_JOINTS16` is set for the cooked skin formats J16W8 and J16W16, `bPERM_WEIGHTS16` for J8W16 and J16W16, and `bPERM_POSITION32` for a cooked FLOAT32X3 position, selecting both the bind-position input and the FLOAT32 pool output; one dispatch runs per cooked code, reading the compute-only skin-source pool. In the raster passes `bPERM_SKINNED` is a `[Permutation]` link-time constant coupled to the stream-set: a pool code (`POOL16`, `POOL32`) compiles with `true` alone and every other code with `false` alone, and the pool code's `POSITION` attribute is bound to GEOM's null stream (stream 7, stride 0), never omitted, because every permutation of a mesh vertex program declares it.
<!-- /informative -->

<!-- elaboration -->
The particle passes simulate ring slots addressed by spawn ordinal, with direct dispatches the CPU sizes per batch, no alive or dead list, a global digit histogram feeding a stable radix sort, and CPU-written count-less indirect draws in two blend partitions (`Zenith2/Docs/Design/VFX/Rendering.md` → "Pass tables"; the ring-slot costs are `Zenith2/Docs/Design/VFX.md` → "Ring-slot costs"). In a frame carrying preview catch-up steps, or steps of a seeking emitter kept from a deferred or failed frame, `ParticleEmit` and `ParticleSimulate` repeat once per step, emit then simulate, in step order, at most `uFLUX_PARTICLE_MAX_STEPS_PER_FRAME` = 128 steps, before `ParticleCompactAndSortKeys` and the four `ParticleSort` passes; step 0's passes keep the names `ParticleEmit` and `ParticleSimulate`, and step s ≥ 1's are `ParticleEmitStep<s>` and `ParticleSimulateStep<s>`.

### ParticleEmit

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/Particles/Emit.slang` → `Flux_ParticleEmitParams` |
| Feature function | part of `void Flux_AddParticleSimulation(Flux_RenderGraph& xGraph, const Flux_ParticleFrame& xFrame, Flux_FrameShared& xShared)` |
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

The effect constants are read through each command's `m_uEffectConstants` (a persistent buffer the passes read, [Z2-N-REND-068]).

### ParticleSimulate

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/Particles/Simulate.slang` → `Flux_ParticleSimulateParams` |
| Feature function | part of `Flux_AddParticleSimulation` (see "ParticleEmit") |
| Kind | compute, 64 threads per group, `Dispatch(m_uGroupCount)` per simulate batch, one thread per slot of every admitted emitter (direct dispatch: the CPU knows every count); declared once per step (`ParticleSimulate`, `ParticleSimulateStep<s>`), each after its step's `ParticleEmit`, so steps run in step order |
| Queue | A |
| Capability | all tiers |
| Budget row | `Z2-B-VFX-003` Particle simulation GPU |
| Permutations | `bPERM_DEPTH_COLLISION` {false, true}; the particle type variant as above |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_xEmitterCommands` | as in ParticleEmit | read | compute |
| `m_xParticleState` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` | read-write | compute |
| `m_xTrailSamples` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` `[Optional]` | read-write | compute |
| `m_xEventRecords` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` `[Optional]` | write | compute |
| `m_xColliders` | `Zenith_RHISRV<Zenith_RHIStructBuffer<Flux_ParticleCollider>>` `[Optional]` (frame chunk) | read | compute |
| `m_xPrevDepth` | `Zenith_RHISRV<Zenith_RHITex2D>` `[Optional]` (the sort view's previous HiZ pyramid, max channel) | read | compute |
| `m_xFrameEnvironment` | as "Conventions" (the frame environment: wind) `[Optional]` | read | compute |
| `m_xPrecipitationOcclusion` | `Zenith_RHISRV<Zenith_RHITex2D>` `[Optional]` (ENV's capture, Stage 5) | read | compute |
| `m_xBoundsValidation` | `Zenith_RHIUAV<Zenith_RHIStructBuffer<Flux_ParticleValidationRecord>>` `[Optional]` (development builds; bounds and live counts) | read-write | compute |
| data | `uint32 m_uFirstCommand`, `m_uCommandCount`, the three region offsets, `m_uFrameEnvironmentBase`; `Zenith_RHIFloat4x4 m_xPrevViewProjection`, `m_xPrevView`, `m_xPrevInvProjection`; `Zenith_RHIUInt2 m_xPrevDepthExtent`; `float m_fTimeSeconds` (wind time) | — | — |

### ParticleCompactAndSortKeys

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/Particles/SortKeys.slang` → `Flux_ParticleSortKeysParams` |
| Feature function | part of `Flux_AddParticleSimulation` (see "ParticleEmit") |
| Kind | compute, 64 threads per group, `Dispatch(m_uGroupCount)` per sort-key batch, one thread per slot of every depth-sorted emitter |
| Queue | A |
| Capability | all tiers |
| Budget row | `Z2-B-VFX-003` Particle simulation GPU |
| Permutations | none |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_xEmitterCommands` | as above | read | compute |
| `m_xParticleState` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` | read | compute |
| `m_xCpuParticles` | `Zenith_RHISRV<Zenith_RHIStructBuffer<Flux_ParticleState>>` `[Optional]` (frame chunk) | read | compute |
| `m_xSortKeys` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` `[FullCoverage]` over `[0, 8 × N)` (transient) | write | compute |
| `m_xGlobalHistogram` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` (4 × 256 words, cleared by `AddClearBuffer`) | read-write | compute |
| data | `Zenith_RHIFloat4x4 m_xSortView`, `uint32 m_uFirstCommand`, `m_uCommandCount`, `m_uStateRegionOffset`, `m_uKeyCount` | — | — |

### ParticleSort

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/Particles/RadixSort.slang` → `Flux_RadixSortParams` |
| Feature function | part of `Flux_AddParticleSimulation` (see "ParticleEmit") |
| Kind | compute, 256 threads per group, four digit passes (`Zenith2/Docs/Design/VFX/Simulation.md` → "ParticleSort algorithm"): `onesweep` one `Dispatch(ceil(N / 2048))` per pass; `reduceThenScan` (`Zenith2/Docs/Design/VFX/Simulation.md → "Reduce-then-scan fallback"`) `Dispatch(ceil(N / 2048))`, `Dispatch(1)`, `Dispatch(ceil(N / 2048))` per pass |
| Queue | A |
| Capability | all tiers; subgroup arithmetic (prefix sums), which the baseline profile carries on every supported device |
| Budget row | `Z2-B-VFX-003` Particle simulation GPU |
| Permutations | `uPERM_PASS` {0, 1, 2, 3} × `uPERM_STAGE` {0 single pass, 1 reduce, 2 scan, 3 scatter}; `vfx.sort.algorithm` selects `onesweep` (stage 0) or `reduceThenScan` (stages 1 … 3) |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_xKeysIn` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` (ping-pong transient) | read | compute |
| `m_xKeysOut` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` `[FullCoverage]` over `[0, 8 × N)` | write | compute |
| `m_xGlobalHistogram` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` | read | compute |
| `m_xPartitionStatus` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` (`ceil(N / 2048) × 256` words, cleared by `AddClearBuffer` before each pass) | read-write | compute |
| `m_xTicket` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` (one word, cleared before each pass; stage 0) | read-write | compute |
| `m_xPartitionCounts` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` `[Optional]` (`ceil(N / 2048) × 256` words; stages 1 … 3, transient) | read-write | compute |
| data | `uint32 m_uDigitShift`, `m_uKeyCount`, `m_uPartitionCount` | — | — |

### ProceduralGeometry

| Property | Value |
|---|---|
| Program | per generator, under `Zenith2/Shaders/Passes/Procedural/` → `Flux_<Generator>Params` |
| Feature function | `void Flux_AddProceduralGeometry(Flux_RenderGraph& xGraph, const Flux_ProceduralFrame& xFrame, Flux_FrameShared& xShared)` |
| Kind | compute, one dispatch per generator with dirty slots |
| Queue | A |
| Capability | all tiers |
| Budget row | `Z2-B-GEOM-005` Procedural geometry regeneration GPU |
| Permutations | per generator |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_xGeneratorInputs` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` | read | compute |
| `m_xProceduralPool` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` `[FullCoverage]` over the regenerated slots | write | compute |

### LightDecalListBuild

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/Lights/LightList.slang` → `Flux_LightListParams` |
| Feature function | `void Flux_AddLightDecalList(Flux_RenderGraph& xGraph, const Flux_ViewList& xViews, Flux_FrameShared& xShared)` |
| Kind | compute, one thread per light and per decal |
| Queue | G |
| Capability | all tiers |
| Budget row | `Z2-B-LIGHT-001` Light and decal list build GPU |
| Permutations | none |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_xLightTable` | `Zenith_RHISRV<Zenith_RHIStructBuffer<Flux_LightRecord>>` | read | compute |
| `m_xDecals` | `Zenith_RHISRV<Zenith_RHIStructBuffer<Flux_DecalRecord>>` (upload ring) | read | compute |
| `m_xViewFrusta` | `Zenith_RHISRV<Zenith_RHIStructBuffer<Flux_FrustumPlanes>>` | read | compute |
| `m_xLightList`, `m_xDecalList` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` `[FullCoverage]` (the layouts of `Zenith2/Docs/Design/LIGHT/Lighting.md → "Light and decal list build"`: the light list's word 1 carries the directional count and the sun and moon roles) | write | compute |
| `m_xProbeRecords`, `m_xProbeList` (Stage 4, a third group) | `Zenith_RHISRV<Zenith_RHIStructBuffer<Flux_ReflectionProbeRecord>>`, `Zenith_RHIUAV<Zenith_RHIByteBuffer>` `[FullCoverage]`, `[Optional]` both | read / write | compute |
| data | `uint32 m_uLightCount`, `m_uDecalCount`, `m_uFrustumCount`, `m_uViewFrustumBase` (the frustum planes' frame-data element base), `m_uAtmosphereEnabled` (the sun-role rule), `m_uProbeCount` (Stage 4) | — | — |

### VTFeedbackClear

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/VirtualTexture/VTFeedbackClear.slang` → `Flux_VTFeedbackClearParams` |
| Feature function | `Flux_RGBuffer Flux_AddVTFeedbackClear(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_VirtualTextureSystem& xVT)` |
| Kind | compute, 256 threads per group, one thread per 4 feedback texels (a `uint4` store) |
| Queue | G (A when the graph finds a gap; `FLUX_PASS_FLAG_ASYNC_COMPUTE` is not set, because the pass is not A-marked in Rendering's frame tables) |
| Capability | all tiers |
| Budget row | `Z2-B-ADVGFX-003` Virtual texturing feedback and page-table update GPU |
| Permutations | none |

| Field | Type | Access | Attributes |
|---|---|---|---|
| `m_xFeedback` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` | UAV, C | `[FullCoverage]` |
| `m_uTexelCount` | `uint` | — | |

### VTPageTableUpdate

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/VirtualTexture/VTPageTableUpdate.slang` → `Flux_VTPageTableUpdateParams` |
| Feature function | `void Flux_AddVTPageTableUpdate(Flux_RenderGraph& xGraph, Flux_FrameShared& xShared, Flux_VirtualTextureSystem& xVT)` |
| Kind | compute, 64 threads per group, one thread per written entry, one dispatch per page mip with records |
| Queue | G |
| Capability | all tiers |
| Budget row | `Z2-B-ADVGFX-003` |
| Permutations | none |

| Field | Type | Access | Attributes |
|---|---|---|---|
| `m_xPageTableMip` | `Zenith_RHIUAV<Zenith_RHITex2D>` | UAV, C (one mip) | |
| `m_xRuns` | `Zenith_RHISRV<Zenith_RHIStructBuffer<Flux_VTPageTableRun>>` | SRV, C (the frame chunk) | |
| `m_uRunBase`, `m_uRunCount`, `m_uEntryCount` | `uint` | — | |

### VTFeedbackReadback

| Property | Value |
|---|---|
| Program | none |
| Feature function | `void Flux_AddVTFeedbackReadback(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_RGBuffer xFeedback, Flux_VirtualTextureSystem& xVT)` |
| Kind | readback (`AddReadback`) of the whole feedback buffer |
| Queue | G or C, as the graph assigns readbacks |
| Capability | all tiers |
| Budget row | `Z2-B-RHI-011` GPU readback pool size (the pool), `Z2-B-ADVGFX-003` (the copy) |
| Permutations | none |

### Virtual texture fields of material passes

| Pass | Added fields | Access |
|---|---|---|
| every pass with an alpha-test permutation or material variants, which today is `Prepass` (alpha-tested bucket), `DeferredDepthPrepass` (alpha-tested bucket), `ShadowDepth` (alpha-tested bucket), `VisibilityRaster` (`bPERM_ALPHA_TEST` and its material variants), `ForwardPlusOpaque`, `ForwardOpaquePerObject`, `GBuffer` and `MaterialResolve` | `[Optional] Zenith_RHISRV<Zenith_RHITex2DArray> m_xVTPhysicalSRGB`, `[Optional] Zenith_RHISRV<Zenith_RHITex2DArray> m_xVTPhysicalLinear`, `[Optional] Zenith_RHISRV<Zenith_RHITex2D> m_xVTPageTable` | SRV, P (C for `MaterialResolve`) |
| `ForwardPlusOpaque`, `ForwardOpaquePerObject`, `GBuffer`, `MaterialResolve` | `[Optional] Zenith_RHIUAV<Zenith_RHIByteBuffer> m_xVTFeedback`, `uint m_uVTFeedbackStride`, `uint m_uVTFeedbackJitter` | UAV, P (C) |

## Shadow passes

### ShadowInstanceCull

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/Culling/InstanceCull.slang` → `Flux_InstanceCullParams` (shared with instance culling) |
| Feature function | part of `void Flux_AddShadowView(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_FrameShared& xShared)` |
| Kind | compute (cull, prefix, emit dispatches of `Zenith2/Docs/Rendering/Culling.md` "Indirect draw compaction") |
| Queue | G |
| Capability | all tiers; the HiZ test where the tier enables shadow HiZ |
| Budget row | `Z2-B-LIGHT-002` Shadow views total GPU |
| Permutations | the `Flux_InstanceCullParams` permutation set declared in "InstanceCullPhase1", with `uPERM_STEP` ∈ {cull, prefix, emit} |

Parameters and outputs are those of "InstanceCullPhase1" with the shadow bucket (`m_uShadowBucket`) selected and distance culling against the light's range or the cascade's far split.

### ShadowDepth

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/Shadows/ShadowDepth.slang` → `Flux_ShadowDepthParams` |
| Feature function | part of `Flux_AddShadowView` |
| Kind | raster, one `DrawIndexedIndirectCount` per shadow bucket; opaque buckets first (position-only index buffer, position stream only), then alpha-tested buckets (`[AlphaTest]` variant, positions and UV0) |
| Queue | G |
| Capability | all tiers |
| Budget row | `Z2-B-LIGHT-002` Shadow views total GPU |
| Permutations | `bPERM_ALPHA_TEST` {false, true}, stream-sets, material variants (alpha-tested only) |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_xInstances`, `m_xPrimitives`, `m_xMaterialSlots` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` | read | vertex, pixel |
| `m_xDrawRecords`, `m_xVisibleInstances` | `Zenith_RHISRV<Zenith_RHIStructBuffer<Flux_DrawRecord>>`, `Zenith_RHISRV<Zenith_RHIByteBuffer>` | read | vertex |
| `m_xSkinnedCurrent`, `m_xProceduralPool` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` `[Optional]` | read | vertex |
| data | `Zenith_RHIFloat4x4 m_xViewProjection`, `uint32 m_uBucketRecordBase` | — | — |

| Target | Format | Load / store |
|---|---|---|
| shadow atlas tile (depth) | D24S8, or D32F_S8 under the capability fallback | the tile is cleared (`CLEAR` over the tile's render area within an atlas cleared at its first use each frame), `STORE` |

Raster state: depth test `LESS_OR_EQUAL`, write on, depth clamp on for directional cascades (pancaking), depth bias from the fixed depth-bias entry `FLUX_DEPTH_BIAS_SHADOW` (constant 0, slope 2.0, clamp 0; each light's own bias travels in its shadow-view record), cull back unless two-sided. A material with a vertex modifier is displaced with the main view's time ([Z2-N-REND-258]).

### ShadowHiZBuild

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/HiZ/HiZ.slang` → `Flux_HiZParams` (see "HiZBuild") |
| Feature function | part of `Flux_AddShadowView`, calling `Flux_AddHiZBuild` over the shadow view's tile |
| Kind | compute, single-pass downsampler over the shadow view's atlas tile |
| Queue | G |
| Capability | quality tiers that enable shadow HiZ |
| Budget row | `Z2-B-LIGHT-002` Shadow views total GPU |
| Permutations | `bPERM_ORTHOGRAPHIC` {false, true}; `uPERM_DEPTH_MODE` fixed to standard (shadow views are always standard Z) |

Parameters and outputs are those of "HiZBuild", with `m_xDepth` the shadow atlas viewed over the tile's rectangle (`m_xTileOffset`, `Zenith_RHIUInt2`, added as plain data) and the pyramid the shadow view's persistent pyramid.

## Culling and HiZ passes

### InstanceCullPhase1

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/Culling/InstanceCull.slang` → `Flux_InstanceCullParams` |
| Feature function | `Flux_CullOutput Flux_AddInstanceCullPhase1(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_ViewTargets& xTargets)` |
| Kind | compute: cull (64 threads per candidate group), prefix (one group of 256 per bucket), emit (one thread per survivor), after the phase's "CullClear" and before its "CullSort" |
| Queue | G |
| Capability | all tiers |
| Budget row | `Z2-B-RPROD-005` Instance culling phase 1 GPU |
| Permutations | the one declaration of the `InstanceCull` program's constants: `uPERM_PHASE` {1, 2}, `bPERM_HIZ` {false, true}, `bPERM_ORTHOGRAPHIC` {false, true}, `uPERM_STEP` {cull, prefix, emit, transparent} with `[PermutationValueRequires(transparent, "uPERM_PHASE", 2)]` and `[PermutationValueRequires(transparent, "bPERM_ORTHOGRAPHIC", false)]`; the enumeration compiles 26 keys (24 for cull, prefix and emit, every one of which a main, editor, capture, texture or shadow view can select, and 2 for transparent), and `transparent` is the step of "TransparentCull" alone |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_xInstances`, `m_xPrimitives`, `m_xMaterialSlots` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` | read | compute |
| `m_xPrevVisibilityBits` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` | read | compute |
| `m_xPrevHiZ` | `Zenith_RHISRV<Zenith_RHITex2D>` | read | compute |
| `m_xCommandCounters` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` (cleared transient, one `u32` per command slot of the view's bucket kind: the draw-key counters of `Zenith2/Docs/Rendering/Culling.md` "Indirect draw compaction") | read-write | compute |
| `m_xSlotCommands` | `Zenith_RHISRV<Zenith_RHIStructBuffer<Flux_SlotCommandRecord>>` (persistent import: each material slot's command base per bucket kind and its LOD count) | read | compute |
| `m_xBucketCommandBase` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` (persistent import: `u32` first command per bucket) | read | compute |
| `m_xSortDispatchArgs` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` (12 bytes, `(n, 1, 1)`, n the view's non-zero command count of the kind, written by the prefix step, read indirectly by `CullSort`) | read-write | compute |
| `m_xDebugBounds` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` `[Optional]` (the culled-rectangle list of the HiZ debug overlay, development builds) | write | compute |
| `m_xSurvivors` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` (`Flux_CullSurvivor`: instance, command) | read-write | compute |
| `m_xDrawArgs` | `Zenith_RHIUAV<Zenith_RHIStructBuffer<Flux_IndirectDrawIndexed>>` | write | compute |
| `m_xDrawCounts` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` (cleared transient, one u32 per bucket) | read-write | compute |
| `m_xDrawRecords` | `Zenith_RHIUAV<Zenith_RHIStructBuffer<Flux_DrawRecord>>` | write | compute |
| `m_xVisibleInstances` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` | write | compute |
| `m_xDrawnBits` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` (cleared transient) | write | compute |
| `m_xCounters` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` (culling diagnostics) | read-write | compute |
| `m_xTransparentEntries` | `Zenith_RHISRV<Zenith_RHIStructBuffer<Flux_TransparentEntry>>` `[Optional]` (step transparent) | read | compute |
| `m_xFinalHiZ` | `Zenith_RHISRV<Zenith_RHITex2D>` `[Optional]` (step transparent: the view's final pyramid) | read | compute |
| data | `Zenith_RHIFloat4x4 m_xViewProjection`, `m_xPrevViewProjection`, `Zenith_RHIFloat4 m_axFrustumPlanes[6]`, `float m_fNear`, `uint32 m_uInstanceCount`, `m_uBucketCount`, `m_uTransparentCount`, `uint32 m_uHiZMipCount`, `Zenith_RHIFloat2 m_xHiZExtent` (both describe the pyramid read, which keeps its own extent across a dynamic-resolution step), `Flux_LODView m_xLODView` (the LOD reference view, GEOM's `[FluxShared]` struct of `Common/LOD.slang`; also the origin of the per-instance cull distance), `uint32 m_uBucketKind` (0 depth, 1 colour, 2 shadow), `uint32 m_uDebugFlags` (bit 0 writes `m_xDebugBounds`) | — | — |

### InstanceCullPhase2

The same program with `uPERM_PHASE` = 2: the candidates are every instance whose `m_xDrawnBits` bit is clear after phase 1, `m_xPrevHiZ` is replaced by this frame's pyramid and `m_xPrevViewProjection` by the current view-projection, the draw arguments go to the phase-2 regions, and the emit step sets, in the next frame's visibility bits (`m_xVisibilityBits`, `Zenith_RHIUAV<Zenith_RHIByteBuffer>`, read-write, cleared to zero by an `AddClearBuffer` pass before phase 1), the bit of every instance it draws, so the buffer holds the union of both phases. Feature function `Flux_CullOutput Flux_AddInstanceCullPhase2(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, const Flux_CullOutput& xPhase1, Flux_ViewTargets& xTargets)`. Budget row: `Z2-B-RPROD-008` Instance culling phase 2 GPU.

### HiZBuild

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/HiZ/HiZ.slang` → `Flux_HiZParams` |
| Feature function | `Flux_RGTexture Flux_AddHiZBuild(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_RGTexture xDepth)` |
| Kind | compute, single-pass downsampler, 256 threads per group, one group per 64×64 tile |
| Queue | G |
| Capability | all tiers; wave intrinsics |
| Budget row | `Z2-B-RPROD-007` HiZ pyramid build GPU |
| Permutations | `uPERM_DEPTH_MODE` {standard, reversed, reversed-infinite} with `[PermutationRequires("bPERM_ORTHOGRAPHIC", false)]`, `bPERM_ORTHOGRAPHIC` {false, true}; the enumeration rule therefore compiles 4 keys (three perspective depth modes, one orthographic standard) |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_xDepth` | `Zenith_RHISRV<Zenith_RHITex2D>` (depth aspect) | read | compute |
| `m_axMips[13]` | `Zenith_RHIUAV<Zenith_RHITex2D>` `[FullCoverage]`, `[Optional]` beyond the mip count | write | compute |
| `m_xCounter` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` (cleared transient, 4 bytes) | read-write | compute |
| data | `float m_fNear`, `m_fFar`, `uint32 m_uMipCount`, `m_uGroupCount`, `Zenith_RHIUInt2 m_xExtent` | — | — |

| Output | Format | Extent |
|---|---|---|
| HiZ pyramid | RG16F (R max, G min, linear metres) | render extent, full mip chain |

### CullClear

| Property | Value |
|---|---|
| Program | none: four graph clear-buffer passes (`AddClearBuffer`) |
| Feature function | `Flux_AddInstanceCullPhase1`, `Flux_AddInstanceCullPhase2`, `Flux_AddShadowViewCulling` and, Stage 5, `Flux_AddMeshletCullPhase1`/`2`, before the phase's first `InstanceCull` or `MeshletCull` dispatch |
| Kind | clear buffer (to zero unless the table below names a pattern) |
| Queue | G |
| Capability | all tiers |
| Budget row | within the phase's culling row (`Z2-B-RPROD-005`, `Z2-B-RPROD-008`, `Z2-B-RPROD-015`, or `Z2-B-LIGHT-002` for shadow views) |
| Permutations | none |

| Cleared buffer | Size |
|---|---|
| `m_xCommandCounters` | 4 bytes × the view's command count of the bucket kind |
| `m_xDrawCounts` | 4 bytes × the bucket count |
| the survivor cursor | 4 bytes |
| `m_xSortDispatchArgs` | 12 bytes, cleared to `(0, 1, 1)` (a clear-buffer pass with a 12-byte pattern) |

### CullSort

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/Culling/CullSort.slang` → `Flux_CullSortParams` |
| Feature function | the culling feature functions, after `InstanceCull[emit]` and, Stage 5, after meshlet survivor emission |
| Kind | compute indirect (`AddComputePassIndirect`): one group of 256 threads per visible command (per meshlet range in the meshlet form), over the dispatch arguments the prefix step wrote; a groupshared bitonic sort for ranges of at most 2,048 entries, otherwise a global merge-path sort in 2,048-entry tiles |
| Queue | G |
| Capability | all tiers |
| Budget row | within the phase's culling row (`Z2-B-RPROD-005`, `Z2-B-RPROD-008`, `Z2-B-RPROD-015`) |
| Permutations | `uPERM_KEY` {instance, instanceMeshlet}: 2 keys (the 32-bit instance key; the 64-bit (instance, meshlet) key of meshlet culling, Stage 5) |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_xDrawArgs` | `Zenith_RHISRV<Zenith_RHIStructBuffer<Flux_IndirectDrawIndexed>>` | read | compute |
| `m_xDrawCounts` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` | read | compute |
| `m_xVisibleInstances` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` | read-write | compute |
| `m_xScratch` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` (transient merge-path tiles for ranges above 2,048 entries, 8 bytes × the view's visible-instance capacity) | read-write | compute |
| the indirect-argument buffer | not a field: `m_xSortDispatchArgs` of `InstanceCull`, passed to `AddComputePassIndirect`, which declares it as an `INDIRECT_ARGS` read | indirect read | — |
| data | `uint32 m_uBucketCount`, `uint32 m_uCommandCount`, `uint32 m_uRegionBase` | — | — |

### BucketScatter

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/Culling/BucketScatter.slang` → `Flux_BucketScatterParams` |
| Feature function | `Flux_DrawKeyTable::AddScatterPass(Flux_RenderGraph& xGraph, Flux_FrameShared& xShared)`, called by `Flux_Renderer::BuildGraph` right after `Flux_AddGPUSceneUpdate` in frames where the draw-key bases were rebuilt; no pass otherwise |
| Kind | compute: one thread per scatter record, 64 threads per group, `(ceil(max(records, buckets) / 64), 1, 1)` groups |
| Queue | G |
| Capability | all tiers |
| Budget row | `Z2-B-RPROD-004` GPU-scene scatter upload GPU (the same kind of work, measured together) |
| Permutations | none: 1 key |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_xRecords` | `Zenith_RHISRV<Zenith_RHIStructBuffer<Flux_SlotCommandScatter>>` (frame data: `u32` material slot, then its 16-byte `Flux_SlotCommandRecord`, 20 bytes) | read | compute |
| `m_xBaseUpload` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` (frame data, `u32` per bucket) | read | compute |
| `m_xSlotCommands` | `Zenith_RHIUAV<Zenith_RHIStructBuffer<Flux_SlotCommandRecord>>` (persistent import) | write | compute |
| `m_xBucketCommandBase` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` `[FullCoverage]` of the bucket range (persistent import) | write | compute |
| data | `uint32 m_uRecordCount`, `uint32 m_uBucketCount`, `m_uRecordsBase`, `m_uBaseUploadBase` (frame-data element bases) | — | — |

### TransparentCull

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/Culling/InstanceCull.slang` → `Flux_InstanceCullParams`, `uPERM_STEP` = transparent |
| Feature function | part of `void Flux_AddTransparentForward(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_ViewTargets& xTargets)`, declared after the final HiZ build and before `TransparentForward` |
| Kind | compute, 64 threads per group, one thread per `Flux_TransparentEntry` (see `Zenith2/Docs/Rendering/Culling.md` "Transparent commands") |
| Queue | G |
| Capability | all tiers |
| Budget row | `Z2-B-RPROD-010` Transparent forward GPU |
| Permutations | `uPERM_STEP` = transparent, `uPERM_PHASE` = 2, `bPERM_HIZ` {false, true}, `bPERM_ORTHOGRAPHIC` = false |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_xInstances`, `m_xPrimitives`, `m_xMaterialSlots` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` | read | compute |
| `m_xTransparentEntries` | `Zenith_RHISRV<Zenith_RHIStructBuffer<Flux_TransparentEntry>>` (the view's sorted entries in the upload ring) | read | compute |
| `m_xFinalHiZ` | `Zenith_RHISRV<Zenith_RHITex2D>` (the view's final pyramid) | read | compute |
| `m_xDrawArgs` | `Zenith_RHIUAV<Zenith_RHIStructBuffer<Flux_IndirectDrawIndexed>>` `[FullCoverage]` (one slot per entry) | write | compute |
| `m_xDrawRecords` | `Zenith_RHIUAV<Zenith_RHIStructBuffer<Flux_DrawRecord>>` `[FullCoverage]` (one record per entry) | write | compute |
| `m_xVisibleInstances` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` `[FullCoverage]` (one entry per transparent entry) | write | compute |
| data | `Zenith_RHIFloat4x4 m_xViewProjection`, `Zenith_RHIFloat4 m_axFrustumPlanes[6]`, `float m_fNear`, `uint32 m_uTransparentCount`, `m_uHiZMipCount`, `Zenith_RHIFloat2 m_xHiZExtent` | — | — |

| Output | Layout |
|---|---|
| transparent command slots | `Flux_IndirectDrawIndexed` × entry count, declared count-less (`IndirectArgs(xArgs)`) in `TransparentForward`'s targets |
| draw records, visible-instance entries | one per entry, indexed by entry |

- [Z2-N-REND-802] The prologue, shadow, culling and HiZ passes above are the ones every path shares; each path's frame uses them unchanged, selecting only permutations.

## Shared lighting passes

### LightClustering

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/Lights/Clustering.slang` → `Flux_ClusteringParams` |
| Feature function | `Flux_RGBuffer Flux_AddLightClustering(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, const Flux_ViewTargets& xTargets)` |
| Kind | compute, two dispatches: froxel cluster bounds build (one thread per cluster), then light and decal assignment (one group per cluster tile) |
| Queue | A |
| Capability | all tiers; not used by the Forward path |
| Budget row | `Z2-B-LIGHT-003` Light clustering GPU |
| Permutations | `uPERM_STEP` {build, assign} |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_xLightList`, `m_xDecalList` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` | read | compute |
| `m_xLightTable` | `Zenith_RHISRV<Zenith_RHIStructBuffer<Flux_LightRecord>>` | read | compute |
| `m_xDepthMinMax` | `Zenith_RHISRV<Zenith_RHITex2D>` (the final HiZ pyramid at the cluster-tile mip) | read | compute |
| `m_xClusterBounds` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` `[FullCoverage]` | write | compute |
| `m_xClusterLists` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` `[FullCoverage]` (per cluster: offset, light count, decal count, probe count; then indices, `Zenith2/Docs/Design/LIGHT/Lighting.md → "Cluster list layout"`) | write | compute |
| `m_xProbeList` (Stage 4) | `Zenith_RHISRV<Zenith_RHIByteBuffer>` `[Optional]` (the frame probe list) | read | compute |
| data | `Zenith_RHIUInt3 m_xGrid` (desktop 16 × 9 × 24, mobile 8 × 5 × 16 clusters, exponential depth slices), `Zenith_RHIFloat4x4 m_xInvProjection`, `float m_fNear`, `m_fFar`, `uint32 m_uMaxLights`, `m_uMaxDecals`, `m_uMaxProbes` (the per-cluster capacities of the tier) | — | — |

### VolumetricFog

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/Fog/Froxels.slang` → `Flux_FroxelFogParams` |
| Feature function | `Flux_RGTexture Flux_AddVolumetricFog(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, const Flux_ViewTargets& xTargets)` |
| Kind | compute, two dispatches: scattering injection (one thread per froxel), then front-to-back integration (one thread per froxel column) |
| Queue | A |
| Capability | all tiers; froxel resolution per quality tier (desktop high 160 × 90 × 64, mobile 80 × 45 × 32) |
| Budget row | `Z2-B-LIGHT-004` Volumetric fog GPU |
| Permutations | `uPERM_STEP` {inject, integrate}, `bPERM_TEMPORAL` {false, true}, `uPERM_SHADOW_QUALITY` {0, 1, 2} |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_xClusterLists`, `m_xLightTable` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` | read | compute |
| `m_xShadowAtlas` | `Zenith_RHISRV<Zenith_RHITex2D>` `[Optional]` | read | compute |
| the marked rows of "Lighting pass fields" | as listed there (light list, transmittance LUT, shadow views, cloud shadow map, atmosphere, cloud-shadow mapping, frame index, cluster mapping) | read | compute |
| `m_xPrevScattering` | `Zenith_RHISRV<Zenith_RHITex3D>` `[Optional]` (the previous half of `m_axFogScattering`; unset while the history is invalid) | read | compute |
| `m_xIBLIrradiance` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` (the sky irradiance SH) | read | compute |
| `m_xLocalFogVolumes` (Stage 5) | `Zenith_RHISRV<Zenith_RHIStructBuffer<Flux_LocalFogVolumeGPU>>` `[Optional]` (frame data) | read | compute |
| `m_xScattering` | `Zenith_RHIUAV<Zenith_RHITex3D>` `[FullCoverage]` (the current half of `m_axFogScattering`) | write | compute |
| `m_xIntegrated` | `Zenith_RHIUAV<Zenith_RHITex3D>` `[FullCoverage]` | write | compute |
| data | `Zenith_RHIUInt3 m_xGrid`, `Zenith_RHIUInt2 m_xRenderExtent` (the render extent the clusters were built at), `float m_fNear`, `m_fFar`, `m_fHistoryWeight`, `m_fPreExposure`, `uint32 m_uLocalVolumeCount`, `m_uLocalFogVolumeBase`, `Zenith_RHIFloat4x4 m_xInvViewProjection`, `m_xPrevViewProjection`, `Flux_FogMediumGPU m_xMedium`, `Flux_MediumOverrideGPU m_xOverride` (Stage 4, the view's underwater medium), `Zenith_RHIFloat3 m_xMediumExtinctionRatio` | — | — |

| Output | Format |
|---|---|
| scattering and integrated volumes | RGBA16F (in-scattered radiance RGB, extinction or transmittance A) |

### GTAO

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/GTAO/GTAO.slang` → `Flux_GTAOParams`; denoise `Zenith2/Shaders/Passes/GTAO/GTAODenoise.slang` → `Flux_GTAODenoiseParams` |
| Feature function | `Flux_RGTexture Flux_AddSSAO(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, const Flux_SSAOSettings& xSettings)` |
| Kind | compute, 8 × 8 groups; GTAO then a bilateral denoise |
| Queue | A |
| Capability | all tiers |
| Budget row | `Z2-B-LIGHT-005` SSAO GTAO and denoise GPU |
| Permutations | `uPERM_QUALITY` {0, 1, 2}, `bPERM_HALF_RESOLUTION` {false, true} (denoise: `bPERM_UPSAMPLE` {false, true}) |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_xDepth` | `Zenith_RHISRV<Zenith_RHITex2D>` | read | compute |
| `m_xNormals` | `Zenith_RHISRV<Zenith_RHITex2D>` (prepass normals, or GB1, or `SurfaceData` normal) | read | compute |
| `m_xAO` | `Zenith_RHIUAV<Zenith_RHITex2D>` `[FullCoverage]` `[WriteOnly]` | write | compute |
| data | `Zenith_RHIFloat4x4 m_xInvProjection`, `Zenith_RHIFloat2 m_xInvExtent`, `float m_fRadius`, `uint32 m_uSliceCount`, `float m_fThickness`, `uint32 m_uStepsPerSide`, `m_uFrameIndex` | — | — |

| Output | Format |
|---|---|
| AO (raw and denoised) | R8_UNORM, render extent (half extent with `bPERM_HALF_RESOLUTION`) |

### ContactShadows

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/Shadows/ContactShadows.slang` → `Flux_ContactShadowsParams` |
| Feature function | `Flux_RGTexture Flux_AddContactShadows(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, const Flux_ViewTargets& xTargets)` |
| Kind | compute, 8 × 8 groups, a screen-space ray march towards the sun |
| Queue | A |
| Capability | all tiers |
| Budget row | `Z2-B-LIGHT-006` Screen-space shadows GPU |
| Permutations | `uPERM_STEPS` {8, 16} |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_xDepth` | `Zenith_RHISRV<Zenith_RHITex2D>` | read | compute |
| `m_xMask` | `Zenith_RHIUAV<Zenith_RHITex2D>` `[FullCoverage]` `[WriteOnly]` | write | compute |
| data | `Zenith_RHIFloat3 m_xSunDirectionVS`, `float m_fMaxDistance`, `Zenith_RHIFloat4x4 m_xProjection`, `float m_fThickness`, `uint32 m_uFrameIndex`, `Zenith_RHIFloat4x4 m_xInvProjection` | — | — |

| Output | Format |
|---|---|
| contact-shadow mask | R8_UNORM, render extent |

### SSGI

Stage 5, LIGHT's screen-space diffuse GI (`Zenith2/Docs/Design/LIGHT/GI.md → "SSGI"`). Its platform scope (the desktop tiers that set `render.quality.ssgiEnabled`) is LIGHT's design and awaits the human's confirmation of the dynamic-GI scope (`Zenith2/Docs/Rendering.md → "Shading paths and capability matrix"`, the SSGI row).

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/SSGI/SSGITrace.slang` → `Flux_SSGITraceParams`; `Zenith2/Shaders/Passes/SSGI/SSGIDenoise.slang` → `Flux_SSGIDenoiseParams` |
| Feature function | `Flux_RGTexture Flux_AddSSGI(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, const Flux_ViewTargets& xTargets)` |
| Kind | compute, 8 × 8 groups over the half render extent: the trace with temporal accumulation (one dispatch), then two à-trous denoise dispatches (step widths 1 and 2) |
| Queue | G |
| Capability | `FLUX_CAPABILITY_TIER_DESKTOP`; runs when the view's tier sets `render.quality.ssgiEnabled` |
| Budget row | the pending LIGHT row "SSGI GPU" (producer: the Stage 1 per-system budget ticket for LIGHT) |
| Permutations | `SSGITrace`: none; `SSGIDenoise`: `uPERM_ITERATION` {0, 1} |

| Field (`Flux_SSGITraceParams`) | C++ type | Access | Stages |
|---|---|---|---|
| `m_xDepth`, `m_xNormals`, `m_xVelocity` | `Zenith_RHISRV<Zenith_RHITex2D>` (prepass normals on Forward+ and Forward, GB1 on the deferred paths) | read | compute |
| `m_xHiZ` | `Zenith_RHISRV<Zenith_RHITex2D>` (final pyramid, min channel) | read | compute |
| `m_xPrevColour` | `Zenith_RHISRV<Zenith_RHITex2D>` `[Optional]` (the view's `m_xSSRPrevColour`; unset: every ray misses) | read | compute |
| `m_xPrevSSGI` | `Zenith_RHISRV<Zenith_RHITex2D>` `[Optional]` (the previous half of `m_axSSGIHistory`) | read | compute |
| `m_xIBLIrradiance` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` (the sky SH) | read | compute |
| `m_xGIVolumes` | `Zenith_RHISRV<Zenith_RHIStructBuffer<Flux_IrradianceVolumeRecord>>` `[Optional]` | read | compute |
| `m_xHistory` | `Zenith_RHIUAV<Zenith_RHITex2D>` `[FullCoverage]` `[WriteOnly]` (the current half of `m_axSSGIHistory`) | write | compute |
| data | `Zenith_RHIFloat4x4 m_xInvViewProjection`, `m_xPrevViewProjection`, `Zenith_RHIFloat2 m_xInvHalfExtent`, `float m_fPrevPreExposure`, `uint32 m_uFrameIndex`, `m_uGIVolumeCount`, `m_uGIVolumeBase`, `m_uHistoryValid`, `m_uMaxIterations` (32) | — | — |

| Field (`Flux_SSGIDenoiseParams`) | C++ type | Access | Stages |
|---|---|---|---|
| `m_xInput` | `Zenith_RHISRV<Zenith_RHITex2D>` (the history half at iteration 0, the iteration-0 output at 1) | read | compute |
| `m_xDepth`, `m_xNormals` | `Zenith_RHISRV<Zenith_RHITex2D>` | read | compute |
| `m_xOutput` | `Zenith_RHIUAV<Zenith_RHITex2D>` `[FullCoverage]` `[WriteOnly]` (a transient; iteration 1's output is the view's SSGI result) | write | compute |
| data | `Zenith_RHIFloat2 m_xInvHalfExtent`, `uint32 m_uStepWidth` | — | — |

| Output | Format |
|---|---|
| SSGI history (both halves) and the SSGI result | RGBA16F, half render extent: RGB irradiance in lux (not pre-exposed), A confidence in [0, 1] |

The result is bound as `m_xSSGI` in every lighting pass of the view ("Lighting pass fields").

### DeferredLighting

Shared by the Deferred and visibility-buffer paths.

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/DeferredLighting/DeferredLighting.slang` → `Flux_DeferredLightingParams` |
| Feature function | `Flux_RGTexture Flux_AddDeferredLighting(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, const Flux_ViewTargets& xTargets)` |
| Kind | compute, 8 × 8 groups, one thread per pixel, lights from the pixel's cluster (`Flux_ClusterLightSource`) |
| Queue | G |
| Capability | `FLUX_CAPABILITY_TIER_DESKTOP` |
| Budget row | `Z2-B-LIGHT-008` Deferred lighting GPU (Deferred path); `Z2-B-LIGHT-009` Visibility buffer lighting GPU (visibility-buffer path) |
| Permutations | `uPERM_SHADOW_QUALITY` {0, 1, 2}, `bPERM_FOG` {false, true}, `bPERM_SSAO` {false, true}, `bPERM_CONTACT_SHADOWS` {false, true}, `bPERM_ADVANCED_SURFACES` {false, true} (Stage 5: reads GB4) |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_axGBuffer[5]`, `m_xDepth` | `Zenith_RHISRV<Zenith_RHITex2D>`; `m_axGBuffer[4]` (GB4, the advanced-surface parameters, Stage 5) is `[Optional]` and set only when the frame's G-buffer pass ran its advanced-surfaces permutation | read | compute |
| `m_xClusterLists`, `m_xLightTable` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` | read | compute |
| `m_xAO`, `m_xContactShadows` | `Zenith_RHISRV<Zenith_RHITex2D>` `[Optional]` | read | compute |
| `m_xFogVolume` | `Zenith_RHISRV<Zenith_RHITex3D>` `[Optional]` | read | compute |
| `m_xBRDFLUT` | `Zenith_RHIDescriptorHandle` (persistent read-only) | read | compute |
| the fields of "Lighting pass fields" | as listed there (the shadow atlas and shadow views `[Optional]`, IBL, probes, GI) | read | compute |
| `m_xSceneColour` | `Zenith_RHIUAV<Zenith_RHITex2D>` `[FullCoverage]` `[WriteOnly]` | write | compute |
| data | `Zenith_RHIFloat4x4 m_xInvViewProjection`, `Zenith_RHIFloat3 m_xCameraPositionWS`, `float m_fExposure`, the cluster mapping; the deferred pass branches per pixel on the shading-model bits of GB2 | — | — |

| Output | Format |
|---|---|
| HDR colour | R11G11B10F, render extent |

## Shared late passes

### SkyAtmosphere

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/Sky/SkyViewLUT.slang` → `Flux_SkyViewLUTParams` (compute, once per frame, shared through `Flux_AddSkyLUTs`, also producing the transmittance and multiple-scattering LUTs); `Zenith2/Shaders/Passes/Sky/SkyDraw.slang` → `Flux_SkyDrawParams` (per view, fullscreen) |
| Feature function | `void Flux_AddSkyAtmosphere(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_ViewTargets& xTargets)`; the shared LUT steps are added once per frame by `Flux_AddSkyLUTs` |
| Kind | compute (LUTs) plus fullscreen raster over the sky texels (depth test `EQUAL` against the far clear value, depth read-only) |
| Queue | G |
| Capability | all tiers |
| Budget row | `Z2-B-ENV-001` Sky and atmosphere GPU |
| Permutations | `SkyDraw`: `bPERM_AERIAL_PERSPECTIVE` {false, true}; `SkyViewLUT`: `uPERM_STEP` {transmittance, multiScatter, skyView} |

The transmittance and multiple-scattering LUTs are persistent-imported textures recomputed in exactly the frames whose atmosphere block's `m_uSkyRevision` changed; the sky-view LUT is recomputed every frame.

| Field (`Flux_SkyViewLUTParams`) | C++ type | Access | Stages |
|---|---|---|---|
| `m_xTransmittanceLUT` | `Zenith_RHIUAV<Zenith_RHITex2D>` `[FullCoverage]` (step transmittance), `Zenith_RHISRV<Zenith_RHITex2D>` (steps multiScatter and skyView) | write / read | compute |
| `m_xMultiScatterLUT` | `Zenith_RHIUAV<Zenith_RHITex2D>` `[FullCoverage]` (step multiScatter), `Zenith_RHISRV<Zenith_RHITex2D>` (step skyView) | write / read | compute |
| `m_xSkyViewLUT` | `Zenith_RHIUAV<Zenith_RHITex2D>` `[FullCoverage]` | write | compute |
| data | `Zenith_RHIFloat3 m_xSunDirectionWS`, `Zenith_RHIFloat3 m_xSunIlluminance`, `float m_fPlanetRadius`, `float m_fAtmosphereHeight`, `Zenith_RHIFloat4 m_axScattering[3]` (Rayleigh, Mie, ozone), `Zenith_RHIFloat4 m_xScaleHeights` (H_R, H_M, ozone centre, ozone half width), `Zenith_RHIFloat4 m_xGroundAlbedoMieG`, `Zenith_RHIFloat3 m_xMoonDirectionWS`, `m_xMoonIlluminance`, `float m_fStartDistance`, `float m_fPreExposure` | — | — |

| Field (`Flux_SkyDrawParams`) | C++ type | Access | Stages |
|---|---|---|---|
| `m_xSkyViewLUT`, `m_xTransmittanceLUT` | `Zenith_RHISRV<Zenith_RHITex2D>` | read | pixel |
| `m_xFogVolume` | `Zenith_RHISRV<Zenith_RHITex3D>` `[Optional]` (aerial perspective) | read | pixel |
| `m_xStarField` (Stage 4) | `Zenith_RHIDescriptorHandle` (persistent read-only cube; the invalid handle when the atmosphere has no star field) | read | pixel |
| `m_xCloudLayer`, `m_xCloudTransmittance` (Stage 4) | `Zenith_RHISRV<Zenith_RHITex2D>` `[Optional]` (ENV's cloud layer) | read | pixel |
| data | `Zenith_RHIFloat4x4 m_xInvViewProjection`, `Zenith_RHIFloat3 m_xCameraPositionWS`, `float m_fExposure`, `Zenith_RHIFloat3 m_xSunDirectionWS`, `m_xSunIlluminance`, `float m_fSunAngularRadius`, `uint32 m_uDrawSunDisc`, `uint32 m_uAtmosphereEnabled`, `Zenith_RHIFloat3 m_xFallbackSkyRadiance`, `Zenith_RHIFloat4 m_xStarRotation`, `float m_fStarIntensity`, `float m_fCloudPreExposureRatio` (the last three Stage 4) | — | — |

| Target / output | Format | Load / store |
|---|---|---|
| transmittance LUT (256 × 64), multiple-scattering LUT (32 × 32), sky-view LUT (192 × 108) | RGBA16F, RGBA16F, R11G11B10F | compute outputs |
| HDR colour | R11G11B10F | `LOAD`, `STORE` |
| depth | D24S8 / D32F_S8 | read-only, `LOAD` |

### SkyCapture

Stage 4: LIGHT recaptures the sky into slice 0 of the environment cube array, one face per frame, when the atmosphere, the time of day or the cloud cover changes (`Zenith2/Docs/Design/LIGHT/Reflections.md → "Sky capture"`).

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/Sky/SkyCapture.slang` → `Flux_SkyCaptureParams` (one face); `Zenith2/Shaders/Passes/Probes/SHProject.slang` → `Flux_SHProjectParams` (the sky SH, after the filter) |
| Feature function | `void Flux_AddSkyCapture(Flux_RenderGraph& xGraph, const Flux_RenderLightingEnvironment& xEnvironment, Flux_FrameShared& xShared, uint32 uFace, Flux_RGTexture& xCaptureCube)`; the SH projection is added by `Flux_ReflectionProbeManager::AddPasses` in the frame after the sixth face, after `Flux_AddProbeFilter` for slice 0 |
| Kind | compute: `SkyCapture` 8 × 8 groups over one face; `SHProject` one group of 256 threads with a fixed-order reduction tree |
| Queue | G |
| Capability | all tiers (face size 128 desktop, 64 mobile) |
| Budget row | `Z2-B-ENV-001` Sky and atmosphere GPU, and the pending LIGHT row "Probe capture and filter GPU" for the filter |
| Permutations | `SkyCapture`: `bPERM_STARS` {false, true}, `bPERM_CLOUDS` {false, true}; `SHProject`: none |

| Field (`Flux_SkyCaptureParams`) | C++ type | Access | Stages |
|---|---|---|---|
| `m_xSkyViewLUT`, `m_xTransmittanceLUT`, `m_xMultiScatterLUT` | `Zenith_RHISRV<Zenith_RHITex2D>` | read | compute |
| `m_xStarField` | `Zenith_RHIDescriptorHandle` (persistent read-only cube; invalid without stars) | read | compute |
| `m_xCloudLayer`, `m_xCloudTransmittance` | `Zenith_RHISRV<Zenith_RHITex2D>` `[Optional]` | read | compute |
| `m_xPrevSkySH` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` (`m_xIBLIrradiance`: the previous capture's SH, for the virtual ground) | read | compute |
| `m_xCaptureFace` | `Zenith_RHIUAV<Zenith_RHITex2DArray>` (mip 0 of the persistent-imported ping-pong capture pair, all 12 layers of that mip as [Z2-N-REND-051] requires; a plain write, neither `[FullCoverage]` nor `[WriteOnly]`, so the other layers keep their contents) | write | compute |
| data | `uint32 m_uFace`, `m_uCaptureHalf`, `m_uFaceSize`, `Flux_AtmosphereGPU m_xAtmosphere`, `Zenith_RHIFloat3 m_xSunDirectionWS`, `m_xSunIlluminance`, `m_xMoonDirectionWS`, `m_xMoonIlluminance`, `m_xGroundAlbedo`, `m_xFallbackSkyRadiance`, `Zenith_RHIFloat4 m_xStarRotation`, `float m_fStarIntensity`, `m_fPreExposure` (the capture's fixed pre-exposure), `m_fCloudPreExposureRatio` | — | — |

| Field (`Flux_SHProjectParams`) | C++ type | Access | Stages |
|---|---|---|---|
| `m_xSource` | `Zenith_RHISRV<Zenith_RHITexCube>` (the captured cube, mip 0) | read | compute |
| `m_xSH` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` `[FullCoverage]` (`m_xIBLIrradiance`, 144 bytes of `Flux_SkyIrradianceSH`) | write | compute |
| data | `uint32 m_uFaceSize`, `float m_fInvPreExposure` | — | — |

| Output | Format |
|---|---|
| a capture face | R11G11B10F, the face size, pre-exposed by `m_fPreExposure`; `SkyCapture` writes layer `6 × m_uCaptureHalf + m_uFace` by its shader indexing |
| the sky SH | 9 × `float4` (RGB, pad), radiance in nits, not pre-exposed |

### ProbeFilter

Stage 4: the prefilter of a captured probe or sky cube into one slice of the environment cube array (`Zenith2/Docs/Design/LIGHT/Reflections.md → "Probe filtering"`).

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/Probes/ProbeFilter.slang` → `Flux_ProbeFilterParams` |
| Feature function | `void Flux_AddProbeFilter(Flux_RenderGraph& xGraph, Flux_RGTexture xSourceCube, Flux_RGTexture& xCubeArray, uint32 uSlice, uint32 uSampleCount)` |
| Kind | compute, 8 × 8 × 1 groups with one z per cube face: `downsample` once per mip of the temporary chain, `copy` once, `prefilter` once per output mip m ≥ 1 |
| Queue | G |
| Capability | all tiers (face size 128 desktop, 64 mobile) |
| Budget row | the pending LIGHT row "Probe capture and filter GPU" (producer: the Stage 1 per-system budget ticket for LIGHT) |
| Permutations | `uPERM_STEP` {downsample, copy, prefilter}, `uPERM_SAMPLES` {16, 32} (`render.quality.probesFilterSamples`) |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_xSource` | `Zenith_RHISRV<Zenith_RHITexCube>` (the captured cube: mip n − 1 of its temporary chain for `downsample`, the whole chain for `copy` and `prefilter`) | read | compute |
| `m_xChainMip` | `Zenith_RHIUAV<Zenith_RHITex2DArray>` `[FullCoverage]` (mip n of the temporary chain, 6 layers; step `downsample`) | write | compute |
| `m_xDestination` | `Zenith_RHIUAV<Zenith_RHITex2DArray>` (mip m of the persistent-imported environment cube array, all its layers; a plain write, so every other resident slice keeps its contents; steps `copy` and `prefilter`) | write | compute |
| data | `uint32 m_uSlice`, `m_uMip`, `m_uMipCount`, `m_uFaceSize`, `m_uSampleCount`, `float m_fRoughness` (`m / (mipCount − 1)`), `float m_fSourceTexelSolidAngle` | — | — |

| Output | Format |
|---|---|
| the filtered probe or sky slice, every mip | R11G11B10F (the environment cube array's format); thread group z (the face) writes layer `6 × m_uSlice + z` of `m_xDestination`, and the cube array is marked output in the frame |

### SSR

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/SSR/SSRTrace.slang`, `SSRDenoise.slang`, `SSRComposite.slang` → `Flux_SSRTraceParams`, `Flux_SSRDenoiseParams`, `Flux_SSRCompositeParams` |
| Feature function | `Flux_RGTexture Flux_AddSSR(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_ViewTargets& xTargets)` |
| Kind | compute, three dispatches, 8 × 8 groups |
| Queue | G |
| Capability | all tiers; half-resolution trace on mobile |
| Budget row | `Z2-B-LIGHT-007` SSR trace denoise and composite GPU |
| Permutations | `uPERM_QUALITY` {0, 1, 2}, `bPERM_HALF_RESOLUTION` {false, true} |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_xHiZ` | `Zenith_RHISRV<Zenith_RHITex2D>` (final pyramid, min channel) | read | compute |
| `m_xNormals`, `m_xRoughness` | `Zenith_RHISRV<Zenith_RHITex2D>` (prepass targets, or G-buffer / `SurfaceData`) | read | compute |
| `m_xPrevColour` | `Zenith_RHISRV<Zenith_RHITex2D>` (the view's `m_xSSRPrevColour`) | read | compute |
| `m_xVelocity` | `Zenith_RHISRV<Zenith_RHITex2D>` | read | compute |
| `m_xHits` | `Zenith_RHIUAV<Zenith_RHITex2D>` `[FullCoverage]` | write | compute |
| `m_xSceneColour` | `Zenith_RHIUAV<Zenith_RHITex2D>` (composite) | read-write | compute |
| `m_xIBLSpecular`, `m_xProbeRecords`, `m_xClusterLists`, `m_xBRDFLUT` | as in `Zenith2/Docs/Rendering/PerPass/ForwardPlus.md` "ForwardPlusOpaque" and "Lighting pass fields" (the environment term recomputed where SSR replaces it), `m_xProbeRecords` `[Optional]` | read | compute |
| `m_xSpecularWeight` (Forward+ and Forward), `m_xSurfaceParams[2]` (GB0 and GB2 on Deferred and the visibility buffer, Stage 5) | `Zenith_RHISRV<Zenith_RHITex2D>` `[Optional]` | read | compute |
| data | `float m_fPreExposure`, `m_fPrevPreExposure`, `m_fMaxRoughness`, `m_fMaxDistance`, `uint32 m_uMaxIterations`, `m_uMaxMip`, `m_uFrameIndex`, `m_uProbeRecordBase`, `Zenith_RHIUInt3 m_xClusterGrid` and the cluster mapping, `Zenith_RHIFloat4x4 m_xInvViewProjection`, `m_xPrevViewProjection` | — | — |

| Output | Format |
|---|---|
| hit buffer | RGBA16F (reflected radiance, confidence) |
| composited HDR colour; next frame's SSR previous colour (a copy) | R11G11B10F |

### OpaqueColourCopy

| Property | Value |
|---|---|
| Program | none (`AddCopyTexture`), or a fullscreen downsample `Zenith2/Shaders/Passes/Transparent/ColourCopy.slang` when the tier uses a half-resolution refraction source |
| Feature function | part of `Flux_AddTransparentForward` |
| Kind | copy or fullscreen |
| Queue | G |
| Capability | all tiers; added only when a visible transparent material refracts |
| Budget row | `Z2-B-RPROD-010` Transparent forward GPU |
| Permutations | none |

| Output | Format |
|---|---|
| opaque colour copy | R11G11B10F |

### TransparentForward

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/Transparent/Transparent.slang` → `Flux_TransparentParams` |
| Feature function | `void Flux_AddTransparentForward(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_ViewTargets& xTargets)` |
| Kind | raster; the non-compacted indirect commands written by "TransparentCull", one per CPU-sorted transparent instance-section, back to front, declared count-less with `IndirectArgs(xArgs)` and drawn run by run with `DrawIndexedIndirect` (see `Zenith2/Docs/Rendering/Culling.md` "Transparent commands") |
| Queue | G |
| Capability | all tiers |
| Budget row | `Z2-B-RPROD-010` Transparent forward GPU |
| Permutations | material variants, stream-sets, `bPERM_CLUSTERED` {false (Forward path), true}, `bPERM_REFRACTION` {false, true}, `bPERM_FOG` {false, true} |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_xInstances`, `m_xPrimitives`, `m_xMaterialSlots`, `m_xDrawRecords`, `m_xVisibleInstances` | `Zenith_RHISRV<…>` as in "ShadowDepth" | read | vertex, pixel |
| `m_xClusterLists` or `m_xPerObjectLights`, `m_xLightTable` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` | read | pixel |
| `m_xOpaqueColourCopy`, `m_xDepth` | `Zenith_RHISRV<Zenith_RHITex2D>` `[Optional]` for the copy | read | pixel |
| `m_xFogVolume` | `Zenith_RHISRV<Zenith_RHITex3D>` `[Optional]` | read | pixel |
| `m_xIBLSpecular`, `m_xIBLIrradiance`, `m_xBRDFLUT` | as in `Zenith2/Docs/Rendering/PerPass/ForwardPlus.md` "ForwardPlusOpaque" | read | pixel |
| the fields of "Lighting pass fields" | as listed there (the shadow atlas and shadow views `[Optional]`, the light list, the atmosphere) | read | pixel |
| `m_xFrameEnvironment` | as "Conventions" (the frame environment) | read | vertex, pixel |

| Target | Format | Load / store |
|---|---|---|
| HDR colour | R11G11B10F | `LOAD`, `STORE`; the material's blend state |
| depth | D24S8 / D32F_S8 | read-only, test `LESS_OR_EQUAL` (standard) or `GREATER_OR_EQUAL` (reversed), no write |

### UIWorldSpace

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/UI/UIWorldSpace.slang` → `Flux_UIWorldSpaceParams` (world-space UI panels; `Zenith2/Docs/Design/UI/Rendering.md` → "UIWorldSpace pass") |
| Feature function | `void Flux_AddUIWorldSpace(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_ViewTargets& xTargets)`, called through UI's `Flux_ViewExtension` at `FLUX_EXTENSION_POINT_TRANSPARENT` |
| Kind | raster, panels back to front by the view-space distance of their centre, batches in paint order within a panel, premultiplied-alpha blending, unlit emissive, no motion vectors |
| Queue | G |
| Capability | all tiers; main and editor views |
| Budget row | `Z2-B-UI-003` UI composite GPU |
| Permutations | `bPERM_SDF_TEXT` {false, true} |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_xVertices` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` (the frame's UI quad chunk, read by vertex pulling) | read | vertex |
| `m_xAtlas` | `Zenith_RHIDescriptorHandle` (persistent read-only glyph and image atlases) | read | pixel |
| data | the data of `Flux_UIDrawParams` plus the panel's canvas-to-world matrix, the view's jittered view-projection, the world luminance in nits (`ui.world.luminanceNits`) and the view's pre-exposure | — | — |

| Target | Format | Load / store |
|---|---|---|
| HDR scene colour | R11G11B10F | `LOAD`, `STORE`; premultiplied-alpha blending |
| depth | D24S8 / D32F_S8 | read-only, test `Flux_DepthModeOps::GetOpaqueCompare` of the view's `Flux_DepthMode` (`LESS_OR_EQUAL` standard, `GREATER_OR_EQUAL` reversed), no write |

### Particles

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/Particles/ParticleDraw.slang`, `RibbonDraw.slang`, `MeshParticleDraw.slang` → `Flux_ParticleDrawParams`, `Flux_RibbonDrawParams`, `Flux_MeshParticleDrawParams` |
| Feature function | `void Flux_AddParticleDraw(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_ViewTargets& xTargets)` |
| Kind | raster; per run one `DrawIndexedIndirect` over the CPU-written commands of the view in the frame chunk (count-less), partition 0 then partition 1 |
| Queue | G |
| Capability | all tiers |
| Budget row | `Z2-B-VFX-004` Particles and ribbons draw GPU |
| Permutations | `Zenith2/Docs/Design/VFX/Rendering.md` → "PSO keys and permutations" |

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
| the fields of "Lighting pass fields" | as listed there, all `[Optional]` (lit permutations) | read | vertex, pixel |
| indirect arguments | `Flux_IndirectArgsRef` in the targets (the frame chunk range of the view's commands) | `INDIRECT_ARGS` | — |
| vertex and index buffers | `Flux_IndexBufferRef` (the quad index buffer, or GEOM's index pool for mesh runs); `Flux_VertexStreamRef` of GEOM's raster pool streams (mesh runs) | `VERTEX`, `INDEX` | — |
| data | `uint32 m_uRecordBase`, the three region offsets, `m_uCpuParticleBase`, `m_uDebugFlags` (`Flux_ParticleDebugFlags`, 0 in Shipping); `float m_fPreExposure`; the cluster mapping (lit permutations, "Conventions"); `Zenith_RHIFloat4x4 m_xViewProjection` (jittered), `m_xView`; `Zenith_RHIFloat3 m_xCameraPositionWS` | — | — |

| Target | Format | Load / store |
|---|---|---|
| HDR colour | R11G11B10F | `LOAD`, `STORE`; partition blend state |
| depth | D24S8 / D32F_S8 | read-only, test `Flux_DepthModeOps::GetOpaqueCompare` (`LESS_OR_EQUAL` standard, `GREATER_OR_EQUAL` reversed), no write |

## Epilogue passes

### TemporalAA

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/TAA/TAA.slang` → `Flux_TAAParams`; an alternative temporal upscaler is selected by the quality tier and specified in `Zenith2/Docs/Design/LIGHT.md`, and a third-party external upscaler is the open selection D-SEL-UPSCALER (open, Stage 5) |
| Feature function | `Flux_RGTexture Flux_AddTemporalAA(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_ViewTargets& xTargets)` |
| Kind | compute, 8 × 8 groups over the output extent: jittered history, velocity reprojection, variance clipping, dynamic-resolution upscale |
| Queue | G |
| Capability | all tiers |
| Budget row | `Z2-B-LIGHT-010` TAA or temporal upscaler GPU |
| Permutations | `bPERM_UPSCALE` {false, true}, `uPERM_QUALITY` {0, 1, 2} |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_xSceneColour`, `m_xVelocity`, `m_xDepth` | `Zenith_RHISRV<Zenith_RHITex2D>` | read | compute |
| `m_xPrevHistory`, `m_xPrevVelocity` | `Zenith_RHISRV<Zenith_RHITex2D>` `[Optional]` both (the view's previous TAA history and previous velocity half; unset while the history is invalid, so the invalid-history frame declares no read) | read | compute |
| `m_xHistory` | `Zenith_RHIUAV<Zenith_RHITex2D>` `[FullCoverage]` `[WriteOnly]` (current half, marked output) | write | compute |
| `m_xOutput` | `Zenith_RHIUAV<Zenith_RHITex2D>` `[FullCoverage]` `[WriteOnly]` | write | compute |
| data | `Zenith_RHIFloat2 m_xJitter`, `m_xPrevJitter`, `Zenith_RHIUInt2 m_xRenderExtent`, `m_xOutputExtent`, `uint32 m_uHistoryValid`, `float m_fCurrentWeight`, `m_fGamma`, `m_fPreExposureRatio` | — | — |

| Output | Format |
|---|---|
| TAA history | RGBA16F, output extent |
| output colour | R11G11B10F, output extent |

### AutoExposure

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/Exposure/Histogram.slang`, `Adapt.slang` → `Flux_HistogramParams`, `Flux_ExposureAdaptParams` |
| Feature function | `Flux_RGBuffer Flux_AddAutoExposure(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, const Flux_ViewTargets& xTargets)` |
| Kind | compute: a 256-bin log-luminance histogram (16 × 16 groups, groupshared bins), then one group adapting exposure |
| Queue | G |
| Capability | all tiers; capture views use a fixed exposure and add neither pass |
| Budget row | `Z2-B-LIGHT-011` Auto-exposure GPU |
| Permutations | none |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_xColour` | `Zenith_RHISRV<Zenith_RHITex2D>` | read | compute |
| `m_xHistogram` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` (cleared transient, 256 × u32) | read-write | compute |
| `m_xExposure` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` (the view's persistent exposure, 16 bytes) | read-write | compute |
| data | `float m_fPreExposure`, `uint32 m_eMetering` (`Flux_MeteringMode`), `float m_fHistogramMinLog2`, `m_fHistogramMaxLog2`, `m_fLowPercent`, `m_fHighPercent`, `m_fSpeedUp`, `m_fSpeedDown`, `m_fDeltaSeconds`, `m_fCompensation`, `m_fMinEV100`, `m_fMaxEV100`, `m_fConfiguredEV100`, `uint32 m_uReadStride`, `m_uHistoryValid` | — | — |

### DepthOfField

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/DoF/CoC.slang`, `Gather.slang`, `Composite.slang` → `Flux_DoFCoCParams`, `Flux_DoFGatherParams`, `Flux_DoFCompositeParams` |
| Feature function | `Flux_RGTexture Flux_AddDepthOfField(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_RGTexture xColour, Flux_RGTexture xDepth)` |
| Kind | compute, 8 × 8 groups: circle of confusion (output extent), gather (half output extent), composite (output extent) |
| Queue | G |
| Capability | all tiers; per camera, only when the view's `FLUX_VIEW_FEATURE_DEPTH_OF_FIELD` bit is set |
| Budget row | `Z2-B-LIGHT-012` Depth of field GPU |
| Permutations | `uPERM_QUALITY` {0, 1} (gather) |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_xDepth` (CoC) | `Zenith_RHISRV<Zenith_RHITex2D>` | read | compute |
| `m_xCoC` | `Zenith_RHIUAV<Zenith_RHITex2D>` `[FullCoverage]` `[WriteOnly]` (CoC); `Zenith_RHISRV<Zenith_RHITex2D>` (gather, composite) | write / read | compute |
| `m_xColour` | `Zenith_RHISRV<Zenith_RHITex2D>` (gather, composite) | read | compute |
| `m_xGather` | `Zenith_RHIUAV<Zenith_RHITex2D>` `[FullCoverage]` `[WriteOnly]` (gather); `Zenith_RHISRV<Zenith_RHITex2D>` (composite) | write / read | compute |
| `m_xOutput` | `Zenith_RHIUAV<Zenith_RHITex2D>` `[FullCoverage]` `[WriteOnly]` (composite) | write | compute |
| data | `float m_fFocusDistance`, `m_fFocusRange`, `m_fMaxCoCPixels`, `Zenith_RHIFloat4x4 m_xInvProjection`; Stage 5: `float m_fFocalLengthMm`, `m_fAperture`, `m_fSensorWidthMm`, `uint32 m_uOutputWidth`, `uint32 m_uPhysical` | — | — |

| Output | Format |
|---|---|
| circle of confusion | R16F |
| half-resolution gather, composited colour | R11G11B10F |

### MotionBlur

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/MotionBlur/TileMax.slang`, `Gather.slang` → `Flux_MotionBlurTileParams`, `Flux_MotionBlurGatherParams` |
| Feature function | `Flux_RGTexture Flux_AddMotionBlur(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_RGTexture xColour, Flux_RGTexture xVelocity, Flux_RGTexture xDepth)` |
| Kind | compute: tile max velocity (16 × 16-pixel tiles), neighbourhood max (3 × 3 tiles), then gather (8 × 8 groups) |
| Queue | G |
| Capability | all tiers; optional per view |
| Budget row | `Z2-B-LIGHT-013` Motion blur GPU |
| Permutations | `uPERM_STEP` {tileMax, neighbourMax} (tile program); `uPERM_SAMPLES` {8, 16} (gather) |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_xVelocity` | `Zenith_RHISRV<Zenith_RHITex2D>` | read | compute |
| `m_xTileMax`, `m_xNeighbourMax` | `Zenith_RHIUAV<Zenith_RHITex2D>` `[FullCoverage]` `[WriteOnly]` in the step that writes each, `Zenith_RHISRV<Zenith_RHITex2D>` after | write / read | compute |
| `m_xColour`, `m_xDepth` | `Zenith_RHISRV<Zenith_RHITex2D>` (gather) | read | compute |
| `m_xOutput` | `Zenith_RHIUAV<Zenith_RHITex2D>` `[FullCoverage]` `[WriteOnly]` (gather) | write | compute |
| data | `float m_fShutterFraction`, `float m_fMaxBlurPixels`, `Zenith_RHIUInt2 m_xExtent` | — | — |

| Output | Format |
|---|---|
| tile max and neighbour max velocity | RG16F |
| blurred colour | R11G11B10F |

### Bloom

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/Bloom/Downsample.slang`, `Upsample.slang` → `Flux_BloomDownsampleParams`, `Flux_BloomUpsampleParams` |
| Feature function | `Flux_RGTexture Flux_AddBloom(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_RGTexture xColour, Flux_RGBuffer xExposure)` |
| Kind | compute, 8 × 8 groups: a downsample chain of the tier's `render.quality.bloomLevels` levels (4 to 6) then the upsample chain, one pass per mip, each reading mip n and writing mip n ± 1 of the same chain texture |
| Queue | G |
| Capability | all tiers |
| Budget row | `Z2-B-LIGHT-014` Bloom GPU |
| Permutations | `bPERM_FIRST` {false, true} (the first downsample reads the scene colour and applies the exposure-scaled threshold) |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_xSource` | `Zenith_RHISRV<Zenith_RHITex2D>` (scene colour, or chain mip n) | read | compute |
| `m_xExposure` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` `[Optional]` (first downsample) | read | compute |
| `m_xDestination` | `Zenith_RHIUAV<Zenith_RHITex2D>` `[FullCoverage]` (chain mip n ± 1; the upsample reads and accumulates it, so it is `READ_WRITE` there) | write / read-write | compute |
| data | `Zenith_RHIFloat2 m_xInvSourceExtent`, `float m_fThreshold`, `float m_fRadius`, `float m_fKnee`, `float m_fPreExposure` | — | — |

| Output | Format |
|---|---|
| bloom chain | R11G11B10F, half output extent at mip 0 |

### TonemapAndGrade

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/Tonemap/Tonemap.slang` → `Flux_TonemapParams` |
| Feature function | `Flux_RGTexture Flux_AddTonemap(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, const Flux_TonemapInputs& xInputs, Flux_RGTexture& xOutput)` |
| Kind | fullscreen, writing the view's output (the swapchain image or the view's texture) |
| Queue | G |
| Capability | all tiers; HDR10 and scRGB output where the swapchain reports the colour space |
| Budget row | `Z2-B-LIGHT-015` Tonemap and colour grading GPU |
| Permutations | `uPERM_OUTPUT` {SDR, HDR10, scRGB}, `bPERM_VIGNETTE` {false, true}, `bPERM_GRAIN` {false, true} |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_xColour`, `m_xBloom` | `Zenith_RHISRV<Zenith_RHITex2D>` | read | pixel |
| `m_xExposure` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` | read | pixel |
| `m_xGradingLUT` | `Zenith_RHIDescriptorHandle` (persistent read-only 32³ RGBA8 3D LUT, or a per-view graph texture while grading is edited) | read | pixel |
| `m_xGradingLUTB` | `Zenith_RHIDescriptorHandle` `[Optional]` (the second LUT of a blend) | read | pixel |
| data | `float m_fPreExposure`, `m_fBloomIntensity`, `m_fPeakNits`, `m_fPaperWhiteNits`, `m_fCurveB`, `m_fCurveC`, `uint32 m_uLUTEnabled`, `float m_fLUTBlend`, `m_fVignetteIntensity`, `m_fGrainIntensity`, `uint32 m_uFrameIndex` (`Zenith2/Docs/Design/LIGHT/ColourManagement.md → "Tonemap parameters"`; the HDR calibration fields Stage 5) | — | — |

| Target | Format | Load / store |
|---|---|---|
| view output | `B8G8R8A8_SRGB` (SDR), `A2B10G10R10_UNORM` (HDR10), `R16G16B16A16_SFLOAT` (scRGB), or the texture view's format | `DONT_CARE` with `FullCoverage()`, `STORE` |

### UIComposite

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/UI/UIDraw.slang` → `Flux_UIDrawParams` (batched quads and glyphs from the UI draw lists; the UI system itself is specified in `Zenith2/Docs/Design/UI.md`) |
| Feature function | `void Flux_AddUIComposite(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_RGTexture& xOutput)` |
| Kind | raster, direct draws per batch with `SetScissor` per clip rectangle, premultiplied-alpha blending |
| Queue | G |
| Capability | all tiers; main view and the editor game view |
| Budget row | `Z2-B-UI-003` UI composite GPU |
| Permutations | `bPERM_SDF_TEXT` {false, true} |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_xVertices` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` (the frame's UI vertex data in the upload ring, read by vertex pulling) | read | vertex |
| `m_xAtlas` | `Zenith_RHIDescriptorHandle` (persistent read-only glyph and image atlases) | read | pixel |
| data | `Zenith_RHIFloat4x4 m_xProjection`, `uint32 m_uBatchFirstVertex`, `float m_fSDFSharpness`, `uint32 m_uOutputMode`, `float m_fNitsForOne` | — | — |

| Target | Format | Load / store |
|---|---|---|
| view output | as "TonemapAndGrade" | `LOAD`, `STORE` |

### DebugOverlay

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/Debug/DebugLines.slang`, `DebugText.slang` → `Flux_DebugLinesParams`, `Flux_DebugTextParams` (the on-screen profiler draws through the same programs) |
| Feature function | `void Flux_AddDebugOverlay(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_RGTexture& xOutput)` |
| Kind | raster, direct draws |
| Queue | G |
| Capability | all tiers; development builds, and shipping builds with the on-screen profiler enabled |
| Budget row | `Z2-B-DEVT-003` Debug overlays and on-screen profiler GPU |
| Permutations | `bPERM_DEPTH_TESTED` {false, true} (lines) |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_xVertices` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` (the frame's debug vertex data in the upload ring) | read | vertex |
| `m_xDepth` | `Zenith_RHISRV<Zenith_RHITex2D>` `[Optional]` (depth-tested lines, resolved against the view's depth at output extent) | read | pixel |
| `m_xFontAtlas` | `Zenith_RHIDescriptorHandle` (persistent read-only) | read | pixel |
| data | `Zenith_RHIFloat4x4 m_xViewProjection`, `Zenith_RHIFloat2 m_xInvOutputExtent`, `uint32 m_uQuadBase` (the first quad's byte offset in `m_xVertices`), `uint32 m_uReversedZ` (1 when the view's depth mode is reversed Z per Z2-D-061, 0 for standard Z) | — | — |

| Target | Format | Load / store |
|---|---|---|
| view output | as "TonemapAndGrade" | `LOAD`, `STORE` |

### HiZDebugOverlay

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/Debug/HiZOverlay.slang` → `Flux_HiZOverlayParams` |
| Feature function | `void Flux_AddHiZDebugOverlay(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_RGTexture& xOutput)`, called by the common epilogue after `Flux_AddDebugOverlay` for the view `r.debug.hiz` selects ([Z2-N-REND-658]); no pass when the debug view is off |
| Kind | raster, output extent: a fullscreen triangle (step `pyramid`), then one indirect instanced quad draw over the culled-rectangle list (step `rectangles`, instance count = the list's count) |
| Queue | G |
| Capability | all tiers; development builds |
| Budget row | `Z2-B-DEVT-003` Debug overlays and on-screen profiler GPU |
| Permutations | `uPERM_STEP` {pyramid, rectangles}: 2 keys |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_xHiZ` | `Zenith_RHISRV<Zenith_RHITex2D>` (the view's final pyramid) | read | pixel |
| `m_xDebugBounds` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` (the cull passes' culled-rectangle list: a count, then per rectangle four `u16` corners in output pixels and a `u32` reason) | read | vertex |
| data | `uint32 m_uMip` (`0xFFFFFFFF` = auto), `uint32 m_uChannel` (0 max, 1 min), `float m_fNear`, `float m_fFar`, `Zenith_RHIFloat2 m_xOutputExtent` | — | — |

| Target | Format | Load / store |
|---|---|---|
| view output | as "TonemapAndGrade" | `LOAD`, `STORE`, alpha blend |

### Present

| Property | Value |
|---|---|
| Program | none |
| Feature function | `void Flux_AddPresent(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_RGTexture xOutput)` |
| Kind | a side-effect pass (`Flux_SideEffect`) that transitions the swapchain image to `PRESENT`; for a view that is not presented, the pass instead marks the output texture with `MarkOutput` |
| Queue | G |
| Capability | all tiers |
| Budget row | `Z2-B-RPROD-020` Frame pacing present-interval deviation; `Z2-B-RPROD-027` Input-to-photon latency |
| Permutations | none |

<!-- elaboration -->
| Present contract | Windows, Vulkan | Windows, D3D12 | Android (S24) |
|---|---|---|---|
| Present mode | `FIFO` (`r.vsync = 1`, default); `MAILBOX`, else `IMMEDIATE`, with `r.vsync = 0` | flip model `DXGI_SWAP_EFFECT_FLIP_DISCARD`; sync interval 1 (`r.vsync = 1`) or 0 with `DXGI_PRESENT_ALLOW_TEARING` | `FIFO` |
| Swapchain images | framesInFlight + 1 | framesInFlight + 1 buffers | framesInFlight + 1 |
| Latency control | `VK_KHR_present_wait` (with `VK_KHR_present_id`) where present: frame N's build waits for present N − framesInFlight | `DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT`, `SetMaximumFrameLatency(framesInFlight)`; frame begin waits on the waitable object | `VK_GOOGLE_display_timing` where present, for the pacer's target present times |
| Who presents | the render-submit thread, after submitting the frame's final graphics batch, waiting on the frame's render-finished semaphore | the render-submit thread, after the final direct-queue submission | the render-submit thread |


- [Z2-N-REND-803] The lighting, late and epilogue passes above use the formats of their tables on every path that runs them; SSR, GTAO and the lighting passes select their normal and roughness inputs per path (prepass targets in Forward+ and Forward, G-buffer targets in Deferred, `SurfaceData` targets in the visibility buffer) and change nothing else.
- [Z2-N-REND-804] The TAA history and every other per-view history above is written into the current half of the view's ping-pong resource, marked with `MarkOutput`, and read from the previous half the next frame.
- [Z2-N-REND-805] The render-submit thread issues the present of each presented view after that frame's final graphics batch, with the present mode, image count and latency control of the table above; the pacing algorithm that chooses when the next frame begins is the frame pacer of `Zenith2/Docs/Design/RPROD.md`, measured by `Z2-B-RPROD-020` and `Z2-B-RPROD-027`.
