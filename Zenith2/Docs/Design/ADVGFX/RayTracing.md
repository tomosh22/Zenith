# ADVGFX design: hardware ray tracing (Stage 7)

Section file of `Zenith2/Docs/Design/ADVGFX.md`. Hardware ray tracing (RT) is deferred to Stage 7 (Z2-D-043, Z2-I-ADVGFX-01), and deferred capabilities get full requirement trees and detailed leaf tickets in their stage's sprint, so this file specifies the RT tree to the depth possible before Stage 7: the RHI extension, acceleration-structure management, ray queries in Flux passes, hit shading, the four features, their capability-matrix rows and fallbacks, validation, budgets and the open decision that scopes them. Nothing here is implemented before Stage 7; no Stage 0 to 6 ticket depends on it, and no Stage 0 to 6 code reserves a field for it beyond the ones Rendering already reserves (the RT row of "Scope boundaries" in `Zenith2/Docs/Rendering.md → "Purpose and scope"`).

## Scope of this file

| Topic | Heading |
|---|---|
| when and how much | "Classification and gating", "Decision D-RT-SCOPE" |
| devices | "Device capability" |
| the RHI | "RHI extension" |
| geometry for rays | "Bottom-level structures", "Top-level structure" |
| tracing in passes | "Ray queries in passes", "Hit shading" |
| the features | "RT features" |
| when RT is absent | "RT capability matrix rows and fallbacks" |
| proof | "RT validation", "RT budgets" |

## Classification and gating

| Element | Rule |
|---|---|
| classification | deferred, Stage 7 (Z2-D-043); every RT requirement carries stage S7 and fix version `Z2 Stage 7` |
| scope decision | D-RT-SCOPE (open, Stage 7) rules which of the four features ship on which tiers and devices, and whether ray-tracing pipelines are used at all; the requirements below are written for the candidate set and the ruling's revalidation ticket narrows them |
| mechanism | inline ray queries (`VK_KHR_ray_query`, DXR 1.1 `RayQuery`) from compute and pixel shaders; ray-tracing pipelines and shader binding tables are not used unless D-RT-SCOPE (open, Stage 7) selects them, because ray queries keep the one global binding model and the render graph's pass kinds unchanged ([Z2-N-REND-075]) |
| binding model | acceleration structures are bindless resources of a fifth descriptor index space (`FLUX_DESCRIPTOR_SPACE_ACCEL_STRUCTURE`), reached through the same 8-byte parameter fields as every other resource |
| barriers | acceleration-structure builds and reads are graph accesses; synthesis stays completely automatic (Z2-D-056) |
| path tracer | the tracer's Stage 7 hardware traversal reuses the top-level structure of this file (`Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Compute and hardware traversal"`, conditional on flagged proposal ADVGFX-1) |

## Device capability

| Backend | Requirement for `m_bRayQuery` | Notes |
|---|---|---|
| Vulkan | `VK_KHR_acceleration_structure`, `VK_KHR_ray_query`, `VK_KHR_deferred_host_operations` (dependency), features `accelerationStructure` and `rayQuery`, `bufferDeviceAddress` | optional extensions recorded in `Zenith_RHICapabilities`; never added to `VP_ZENITH2_baseline` (the baseline stays RT-free) |
| D3D12 | `D3D12_FEATURE_D3D12_OPTIONS5.RaytracingTier ≥ D3D12_RAYTRACING_TIER_1_1` | WARP reports tier 1.1 in recent pinned versions; whether the pinned version does is recorded per run |
| S24 (Adreno 750) | the Vulkan row | whether RT runs on the S24 at all is part of D-RT-SCOPE (open, Stage 7) |
| lavapipe | the Vulkan row, when the pinned Mesa version exposes it | a test on a device without the capability records the skip reason ([Z2-N-REND-034]) |

Vertex formats a bottom-level build reads: `R16G16B16A16_SNORM` (the default position stream), `R16G16B16A16_SFLOAT` (the skinned pool) and `R32G32B32_SFLOAT` (the `FLOAT32x3` positions of a mesh whose cook precision check failed); all three are build formats of `VK_KHR_acceleration_structure` and of DXR 1.1, which the Stage 7 capability check confirms per device (`Test_RTCapabilities_VertexFormatsReported`).

## RHI extension

<!-- assumption: pending RHI -->
The declarations are `Zenith2/Docs/Design/ADVGFX/PublicAPI.md → "Zenith_RHIRayTracing header"`. RHI owns the backends; ADVGFX's Stage 7 tickets write the extension under RHI's review, in RHI's files: `Zenith2/Engine/ZenithRHI/Zenith_RHIRayTracing.h`, `ZenithRHI_Vulkan/Internal/Zenith_VkRayTracing.cpp`, `ZenithRHI_D3D12/Internal/Zenith_D3D12RayTracing.cpp` (row AV4 (d)).
<!-- /assumption -->

### Acceleration-structure resources

| Element | Vulkan | D3D12 |
|---|---|---|
| storage | a buffer of `ACCELERATION_STRUCTURE_STORAGE` usage holding `VkAccelerationStructureKHR` | a buffer in `D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE` |
| handle | `Zenith_RHIAccelerationStructureHandle` (generational device handle, `Zenith2/Docs/Contracts.md → "Handle kinds"`) | same |
| sizes | `vkGetAccelerationStructureBuildSizesKHR` | `GetRaytracingAccelerationStructurePrebuildInfo` |
| scratch | a graph transient buffer per build batch, aligned to `m_uScratchAlignment` | same |
| compaction | query compacted size after a static build; copy into a compacted structure in the next frame; retire the original through deferred deletion | same |
| retirement | through `Flux_DeferredDeletion` with per-queue last-use values ([Z2-N-REND-488]) | same |

### Descriptor index space for acceleration structures

| Element | Rule |
|---|---|
| space | `FLUX_DESCRIPTOR_SPACE_ACCEL_STRUCTURE`, capacity 64 desktop (one TLAS per view kind that traces, plus the tracer's) |
| Vulkan | one more binding of `VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR` in the global set, which D-SLANG (open, Stage 1)'s ruled lowering extends by one kind |
| D3D12 | SRVs of `D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE` in a fifth contiguous range of the one shader-visible heap |
| Slang | `DescriptorHandle<RaytracingAccelerationStructure>` fields; the graph tag `Zenith_RHIAccelStructTag` |

### Builds and barrier states

| Access | Graph meaning | Synthesised barrier |
|---|---|---|
| `ACCEL_BUILD_WRITE` | a build or update pass writes the structure | `VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR` at `ACCELERATION_STRUCTURE_BUILD`; D3D12 a UAV barrier on the structure buffer |
| `ACCEL_BUILD_READ` | a top-level build reads its bottom-level structures | `ACCELERATION_STRUCTURE_READ` at `ACCELERATION_STRUCTURE_BUILD` |
| `ACCEL_TRACE_READ` | a pass traces against it | `ACCELERATION_STRUCTURE_READ` at the reading stage (compute or fragment) |

The access kinds join Rendering's field-table vocabulary and barrier state machine (`Zenith2/Docs/Rendering/RenderGraph.md → "Barrier state machine"`) through the Stage 7 RT core-doc item, filed by the Stage 7 API ticket.

## Bottom-level structures

### Static meshes

| Element | Rule |
|---|---|
| unit | one bottom-level structure per (mesh, LOD) with one geometry per section, built from GEOM's unified pools in place: the position stream's region and the 16-bit index region, `vertexOffset` folded into the build's vertex offset |
| quantisation | positions are SNORM16 in the mesh's quantisation box; the box's scale and bias are folded into every top-level instance transform, so no dequantised copy exists |
| full-precision positions | a mesh whose cook precision check failed carries `FLOAT32x3` positions (`Zenith2/Docs/Rendering/VertexData.md`); its structure is built with the `R32G32B32_SFLOAT` vertex format and the identity box (scale 1, bias 0), so nothing is folded into its instance transforms (`Test_RTBLAS_Float32PositionsIdentityBox`) |
| flags | `FAST_TRACE | ALLOW_COMPACTION`; alpha-tested sections are built non-opaque, opaque sections opaque |
| when | at mesh residency (GEOM's `RESIDENT` edge, row AV3 (e)), up to `render.rt.blasBuildsPerFrame` (32) per frame in mesh-load order; a mesh whose structure is not built yet is absent from rays (its instances are traced as missing, counted `Flux.RT.MissingBLAS`) |
| LOD | an instance is traced with the LOD its main-view raster selection chose this frame, clamped to at least `render.rt.minLOD` (1 desktop) beyond `render.rt.lodDistanceMetres` (50 m), so distant geometry costs less |
| memory | counted in the pending row "Ray-tracing acceleration-structure memory"; at its cap new builds wait and the least recently traced unpinned structures are released |

### Skinned and procedural geometry

| Geometry | Rule |
|---|---|
| skinned | one structure per skinned instance built from the skinned-vertex pool's current half (`FLOAT16x4` positions, [Z2-N-REND-552]) with `ALLOW_UPDATE`; refit (update) every frame the pose changes, rebuilt every 64 frames to bound quality loss; the build runs after `ComputeSkinning` in the common prologue |
| procedural | rebuilt in the frame its region is regenerated, after `ProceduralGeometry` |
| particles and ribbons | not traced |
| terrain | ENV's terrain is traced through per-node structures of GEOM's terrain geometry at the main view's selected node set; the node set changes rebuild at most `render.rt.terrainNodesPerFrame` (16) per frame (detail at Stage 7 with ENV) |

## Top-level structure

| Step | Action |
|---|---|
| 1 | `RTInstanceBuild` (compute, common prologue after the scatter upload and the bottom-level builds): one thread per GPU-scene instance; an instance that is hidden in main, transparent, or outside `render.rt.instanceRangeMetres` (2,000 m) of every tracing view writes nothing; otherwise one 64-byte instance record (`VkAccelerationStructureInstanceKHR` / `D3D12_RAYTRACING_INSTANCE_DESC`, identical layout) with the 3 × 4 transform (the instance matrix times the mesh's quantisation box), `instanceCustomIndex` = the GPU-scene instance slot, the mask from the render layers (bits 0–7 of `m_uLayerMask`), the bottom-level address of its (mesh, LOD), and the culling flags (`FORCE_OPAQUE` for opaque-only meshes) |
| 2 | compaction of the instance records into a dense array with a count (prefix sum), so the build reads only live instances |
| 3 | `TLASBuild`: a top-level build with `FAST_TRACE`, rebuilt every frame (never updated), on graphics, or async compute where the graph finds a gap |
| 4 | the TLAS is imported into the graph as `Flux_RGAccelerationStructure` and read by every tracing pass of the frame |

## Ray queries in passes

| Element | Rule |
|---|---|
| query | `RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES>` with `CULL_BACK_FACING_TRIANGLES` off (shadow rays must see back faces of closed meshes) |
| opaque geometry | committed by the hardware |
| alpha-tested geometry | candidate hits loop: for each candidate, the material slot's alpha record (`Flux_RTAlphaRecord`, 16 bytes: base-colour texture SRV, UV0 scale and bias as half4, alpha cutoff) is read, the hit's UV0 interpolated from the index and UV streams, and the alpha sampled at the ray cone's mip; below the cutoff the candidate is skipped. A material whose opacity is not one texture's alpha times a factor is traced opaque and listed in the cook report (`rtAlphaApproximated`) |
| alpha records | one per material slot, in a persistent read-only buffer written by GEOM's material manager beside the material-slot record (row AV3 (e)) |
| ray budget | each feature declares its rays per pixel; a pass never loops over more than `uFLUX_RT_MAX_CANDIDATES` (64) candidates per ray, after which it commits the nearest (counted `Flux.RT.CandidateOverflow`) |

## Hit shading

Reflections and GI need the radiance leaving the hit point. They use the tracer's wavefront structure (`Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Wavefront pipeline"`) at real-time budgets:

| Stage | Pass | Rule |
|---|---|---|
| trace | `RTReflectionTrace`, `RTGITrace` | write a 16-byte hit record per ray (instance slot, triangle, barycentrics, t) |
| classify | `RTHitClassify` | per material variant counts and compacted ray lists (as `PTClassify`) |
| resolve | `RTHitResolve` × variants | `EvaluateMaterial` at the hit with ray-cone gradients, then `EvaluateLightingEx` with `Flux_WorldLightSource`, a `Flux_LightSource` over the frame light list (no screen clusters exist at an off-screen hit), shadows from the shadow maps, no SSR, IBL from the probes |
| result | per-ray radiance into the feature's trace target |

`Flux_WorldLightSource` and the RT shadow mask's consumption in `EvaluateLightingEx` are LIGHT's integration (row AV9 (c)).

## RT features

### RT shadows

| Element | Rule |
|---|---|
| lights | the sun and up to `render.quality.rt.shadowedLocalLights` (8) local shadowed lights by LIGHT's shadow priority |
| rays | one per pixel per light at render extent, toward a point sampled on the light's disc or sphere (blue-noise rotated per frame) |
| output | an R8 visibility mask per light, temporally accumulated and spatially denoised (variance-guided, 2 iterations), read by the lighting passes instead of the shadow-map sample for that light |
| fallback | the light keeps its shadow map when RT shadows are off for it |

### RT reflections

| Element | Rule |
|---|---|
| rays | one per pixel at half render extent for surfaces with roughness ≤ `m_fReflectionRoughnessCutoff` (0.4); GGX VNDF-sampled directions |
| shading | "Hit shading" |
| output | reflection radiance, denoised, composited where SSR would composite, replacing SSR and probe specular below the cutoff |
| fallback | SSR and probes |

### RT ambient occlusion

| Element | Rule |
|---|---|
| rays | one cosine-weighted ray per pixel within LIGHT's GTAO radius, any-hit |
| output | an R8 AO target in place of GTAO's, denoised |
| fallback | GTAO |

### RT diffuse GI

| Element | Rule |
|---|---|
| rays | one per pixel at half render extent, cosine-weighted, one bounce |
| shading | "Hit shading", with the hit's indirect term taken from the irradiance volumes or the sky SH |
| output | indirect irradiance replacing `m_xIndirectIrradiance`'s SSGI source, denoised with history |
| fallback | SSGI on the tiers that run it, sky and probe recapture elsewhere (the dynamic GI classification of `Zenith2/Docs/Design/LIGHT.md`) |

## RT capability matrix rows and fallbacks

<!-- elaboration -->
| Feature | Forward+ Win | Forward+ S24 | Forward Win | Deferred Win | Visibility buffer Win | Parity comparison |
|---|---|---|---|---|---|---|
| RT shadows (Stage 7) | pending D-RT-SCOPE | pending D-RT-SCOPE | pending D-RT-SCOPE | pending D-RT-SCOPE | pending D-RT-SCOPE | per-feature tolerance against shadow maps; physical accuracy against the tracer |
| RT reflections (Stage 7) | pending D-RT-SCOPE | no (no candidate admits it) | pending D-RT-SCOPE | pending D-RT-SCOPE | pending D-RT-SCOPE | per-feature tolerance |
| RT ambient occlusion (Stage 7) | pending D-RT-SCOPE | pending D-RT-SCOPE | pending D-RT-SCOPE | pending D-RT-SCOPE | pending D-RT-SCOPE | AO channel, per-feature tolerance |
| RT diffuse GI (Stage 7) | pending D-RT-SCOPE | no (no candidate admits it) | pending D-RT-SCOPE | pending D-RT-SCOPE | pending D-RT-SCOPE | per-feature tolerance |

The rows join `Zenith2/Engine/ZenithFlux/Internal/Paths/CapabilityMatrix.json` and Rendering's table ([Z2-N-REND-695]) in the Stage 7 revalidation diff after D-RT-SCOPE (open, Stage 7) rules. A feature runs only when the device reports `m_bRayQuery`, the tier enables it and the view's feature mask carries its bit (four new `Flux_ViewFeature` bits, Stage 7); otherwise its fallback runs, so every RT feature has a tested non-RT path ([Z2-N-REND-035]).

## RT validation

| Evidence | Rule |
|---|---|
| sync validation | every RT pass and build runs with Vulkan synchronization validation and D3D12 GPU-based validation clean ([Z2-N-REND-382]) |
| unit tests | `Test_RTInstanceBuild_MatchesGPUScene` (instance records against a CPU recomputation from the tables), `Test_RTBLAS_QuantisationFoldedIntoTransform` (a traced hit equals the dequantised position within one SNORM16 quantum), `Test_RTBLAS_Float32PositionsIdentityBox` (a `FLOAT32x3` mesh's traced hit equals its stored position within one float ulp of the build), `Test_RTAlpha_CandidateSkipsBelowCutoff`, `Test_RTSkinned_RefitFollowsPose` |
| physical accuracy | the tracer's reference scenes with each RT feature on, compared within the scene's tolerance (the RT feature is expected to be closer to the reference than its raster fallback; the report records both) |
| parity | RT on against RT off within the per-feature tolerances of the matrix rows |
| device runs | RTX 3070 frame-time and memory runs of W-Indoor and W-OpenWorld with each feature (`needs-human`, `gpu-reason:visual-measure`); the S24 run if D-RT-SCOPE admits it |

## RT budgets

Pending rows this tree needs, named descriptively until the Budgets writer assigns IDs (cross-doc item X422), each produced by the Stage 7 budget ticket for ADVGFX:

| Row | Unit | Procedure | Platform |
|---|---|---|---|
| Ray-tracing acceleration-structure memory | MB | MP-SUBMEM | Win, S24; at most 512 MiB desktop and 128 MiB S24, the bound D-RT-SCOPE's acceptance rule uses before the row is allocated |
| Ray-tracing BLAS builds per frame | builds | MP-COUNTER | Win, S24 |
| RT shadows GPU | ms | MP-GPUPASS | Win, S24 |
| RT reflections GPU | ms | MP-GPUPASS | Win |
| RT ambient occlusion GPU | ms | MP-GPUPASS | Win |
| RT diffuse GI GPU | ms | MP-GPUPASS | Win |

## Decision D-RT-SCOPE

| Element | Content |
|---|---|
| question | which RT features ship, on which quality tiers and devices (desktop alone, or the S24 too), and whether ray-tracing pipelines join ray queries |
| candidates | (1) shadows and AO on desktop high tiers only, ray queries only; (2) all four features on desktop high and epic tiers, ray queries only; (3) candidate 2 plus RT shadows on the S24; (4) candidate 2 plus ray-tracing pipelines for reflections and GI |
| measurements | an ordinary harness ticket (`Zenith2/Benchmarks/RayTracing/`, CTest smoke) measures each feature's GPU time and memory on W-Indoor and W-OpenWorld on the RTX 3070 and the S24, and each feature's physical-accuracy gain over its fallback on the reference scenes |
| acceptance rule | a feature is admitted on a tier when its GPU time fits the tier's frame budget (`Z2-B-SHIP-001`) with the rest of the frame on W-OpenWorld; the acceleration structures of every admitted feature together stay within 512 MiB on desktop and 128 MiB on the S24 (an absolute bound fixed now, which the ruling then allocates as the pending acceleration-structure row, at or below it); and its physical-accuracy error is at most its fallback's on every reference scene that exercises it, and strictly below on at least one; pipelines are admitted only if a measured feature is at least 15% faster with them, non-overlapping 95% intervals |
| consumers blocked | Z2-R-ADVGFX-122 to Z2-R-ADVGFX-140 and Z2-R-ADVGFX-171 (the RT tree) |
| nouns | `Flux_RayTracingScene`, `Flux_RayTracingFeature`, `Flux_RayTracingSettings`, `Zenith_RHIRayTracingCapabilities`, `Zenith_RHIAccelerationStructureHandle` |
| ruling | a `needs-human` ruling ticket blocked by the harness ticket appends the entry to `Zenith2/Docs/DecisionLog.md` (Z2-D-084, Z2-D-097); the name is registered as an open decision through cross-doc item X423 |
