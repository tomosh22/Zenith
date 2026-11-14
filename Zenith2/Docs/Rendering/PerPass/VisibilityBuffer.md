# Zenith2 rendering: per-pass specifications, visibility buffer

Section file of `Zenith2/Docs/Rendering.md`. It details the entry-file section "Per-pass specifications" for the passes unique to the visibility-buffer path. Normative statements share the `REND` tag family of the entry file.

## Conventions

The tables follow `Zenith2/Docs/Rendering/PerPass/Common.md` "Conventions". HiZ, clustering, fog, GTAO, deferred lighting and the late and epilogue passes are specified in `Zenith2/Docs/Rendering/PerPass/Common.md`.

- [Z2-N-REND-850] The visibility-buffer passes below are implemented exactly as their tables specify and run on `FLUX_CAPABILITY_TIER_DESKTOP` alone, with mesh shaders when `m_bMeshShaders` is set and the compute/indirect fallback otherwise.

## Visibility buffer passes

### MeshletCullPhase1

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/VisibilityBuffer/MeshletCull.slang` → `Flux_MeshletCullParams` |
| Feature function | `Flux_MeshletCullOutput Flux_AddMeshletCullPhase1(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_ViewTargets& xTargets)` |
| Kind | compute: the instance test of `Zenith2/Docs/Rendering/PerPass/Common.md` "InstanceCullPhase1", then one thread per candidate meshlet (frustum, normal cone, HiZ with the previous view-projection) |
| Queue | G |
| Capability | `FLUX_CAPABILITY_TIER_DESKTOP` |
| Budget row | `Z2-B-RPROD-015` Meshlet culling GPU |
| Permutations | `uPERM_PHASE` {1, 2}, `bPERM_MESH_SHADER_ARGS` {false, true} |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_xInstances`, `m_xPrimitives`, `m_xMaterialSlots` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` | read | compute |
| `m_xMeshlets` | `Zenith_RHIDescriptorHandle` (persistent read-only meshlet table: bounds sphere, cone axis and cutoff, vertex and triangle offsets, counts) | read | compute |
| `m_xPrevVisibilityBits`, `m_xPrevHiZ` | as "InstanceCullPhase1" | read | compute |
| `m_xVisibleMeshlets` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>`: per entry (instance index u32, meshlet index u32), 8 bytes | read-write | compute |
| `m_xMeshletCount` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` (cleared transient) | read-write | compute |
| `m_xRasterArgs` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>`: mesh-task dispatch or fallback indirect arguments, opaque and alpha-tested ranges | write | compute |
| `m_xDrawnBits` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` | write | compute |

### MeshletCullPhase2

The same program with `uPERM_PHASE` = 2: candidates are the instances and meshlets not drawn in phase 1, tested with the current view-projection against this frame's pyramid, appended after phase 1's entries in `m_xVisibleMeshlets`; the emit writes next frame's visibility bits from the union. Feature function `Flux_MeshletCullOutput Flux_AddMeshletCullPhase2(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, const Flux_MeshletCullOutput& xPhase1, Flux_ViewTargets& xTargets)`. Budget row: `Z2-B-RPROD-015` Meshlet culling GPU.

### VisibilityRaster

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/VisibilityBuffer/VisibilityRaster.slang` → `Flux_VisibilityRasterParams` |
| Feature function | `void Flux_AddVisibilityRaster(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, const Flux_MeshletCullOutput& xCull, Flux_ViewTargets& xTargets)` (once per phase) |
| Kind | raster: `DrawMeshTasksIndirectCount` (task/amplification + mesh stages) when `m_bMeshShaders`, else an indirect indexed draw over a compute-expanded per-meshlet index list; opaque meshlets first, then alpha-tested (`[AlphaTest]`) |
| Queue | G |
| Capability | `FLUX_CAPABILITY_TIER_DESKTOP`; mesh shaders optional |
| Budget row | `Z2-B-RPROD-016` Visibility raster GPU |
| Permutations | `bPERM_MESH_SHADER` {false, true}, `bPERM_ALPHA_TEST` {false, true}, stream-sets, material variants (alpha-tested only) |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_xVisibleMeshlets` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` | read | task, mesh, vertex |
| `m_xMeshlets`, `m_xMeshletTriangles`, `m_xMeshletVertices` | `Zenith_RHIDescriptorHandle` (persistent read-only) | read | mesh, vertex |
| `m_xPositions` | `Zenith_RHIDescriptorHandle` (unified position pool, read as SNORM16x4 through `VertexFormats.slang`) | read | mesh |
| `m_xInstances`, `m_xPrimitives` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` | read | task, mesh, vertex |
| `m_xSkinnedCurrent` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` `[Optional]` | read | mesh, vertex |
| `m_xFrameEnvironment` | as `Zenith2/Docs/Rendering/PerPass/Common.md` "Conventions" (material vertex modifiers) | read | mesh, vertex |

| Target | Format | Phase 1 | Phase 2 |
|---|---|---|---|
| visibility ID | R32_UINT: visibleMeshletIndex × 128 + triangleInMeshlet | `CLEAR` to `0xFFFFFFFF`, `STORE` | `LOAD`, `STORE` |
| depth | D24S8, or D32F_S8 (precision or capability) | `CLEAR`, `STORE` | `LOAD`, `STORE` |

- [Z2-N-REND-851] The visibility ID target is R32_UINT, one of the justified 32-bit targets, cleared to the reserved value `0xFFFFFFFF` meaning "no geometry", and meshlets hold at most 128 triangles so the 7-bit triangle field suffices.

### MaterialClassify

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/VisibilityBuffer/Classify.slang` → `Flux_ClassifyParams` |
| Feature function | `Flux_MaterialBins Flux_AddMaterialClassify(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, const Flux_ViewTargets& xTargets)` |
| Kind | compute: per 8 × 8 tile, collect the distinct material variants (visibility ID → visible meshlet → instance → material slot), append the tile to each variant's bin, then a prefix pass writes one indirect dispatch argument set per variant |
| Queue | G |
| Capability | `FLUX_CAPABILITY_TIER_DESKTOP` |
| Budget row | `Z2-B-RPROD-017` Material classification GPU |
| Permutations | `uPERM_STEP` {bin, args} |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_xVisibilityID` | `Zenith_RHISRV<Zenith_RHITex2D>` | read | compute |
| `m_xVisibleMeshlets`, `m_xInstances`, `m_xMaterialSlots` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` | read | compute |
| `m_xBinCounts` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` (cleared transient, one u32 per material variant) | read-write | compute |
| `m_xBinTiles` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` | write | compute |
| `m_xDispatchArgs` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` `[FullCoverage]` (12 bytes per variant) | write | compute |

### MaterialResolve

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/VisibilityBuffer/Resolve.slang` → `Flux_ResolveParams`, one permutation per material variant |
| Feature function | `void Flux_AddMaterialResolve(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, const Flux_MaterialBins& xBins, Flux_ViewTargets& xTargets)` |
| Kind | compute, one `DispatchIndirect` per material bin, 8 × 8 threads per tile |
| Queue | G |
| Capability | `FLUX_CAPABILITY_TIER_DESKTOP` |
| Budget row | `Z2-B-RPROD-018` Material resolve GPU |
| Permutations | material variants, stream-sets, `bPERM_DECALS` {false, true}, `bPERM_SKINNED` {false, true}, `bPERM_ADVANCED_SURFACES` {false, true} (Stage 5: writes GB4) |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_xVisibilityID` | `Zenith_RHISRV<Zenith_RHITex2D>` | read | compute |
| `m_xVisibleMeshlets`, `m_xInstances`, `m_xPrimitives`, `m_xMaterialSlots` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` | read | compute |
| `m_xMeshlets`, `m_xMeshletTriangles`, `m_xMeshletVertices`, `m_xPositions`, `m_xSurface`, `m_xOptional` | `Zenith_RHIDescriptorHandle` (persistent read-only; indices read as u32 words with a halfword select) | read | compute |
| `m_xSkinnedCurrent` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` `[Optional]` | read | compute |
| `m_xBinTiles`, `m_xClusterLists`, `m_xDecalRecords` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` | read | compute |
| `m_axSurfaceData[5]` | `Zenith_RHIUAV<Zenith_RHITex2D>`, formats of the G-buffer (GB0–GB3, and GB4 `[Optional]` set in the advanced-surfaces permutation alone) | write | compute |
| `m_xLightmapPages` (Stage 5) | `Zenith_RHISRV<Zenith_RHIStructBuffer<Flux_LightmapPageRecord>>` `[Optional]` | read | compute |
| `m_xFrameEnvironment` | as `Zenith2/Docs/Rendering/PerPass/Common.md` "Conventions" | read | compute |
| data | `Zenith_RHIFloat4x4 m_xViewProjection`, `m_xInvViewProjection`, `Zenith_RHIUInt2 m_xExtent`, `uint32 m_uMaterialVariant` | — | — |

| Step per pixel | Action |
|---|---|
| 1 | decode the visibility ID; skip `0xFFFFFFFF` |
| 2 | fetch the triangle's three vertices through `VertexFormats.slang` (skinned pool positions where the instance is skinned) |
| 3 | compute perspective-correct barycentrics and their screen-space derivatives analytically from the pixel ray and the triangle |
| 4 | interpolate attributes and fill `Flux_MaterialInput` with analytic gradients |
| 5 | `EvaluateMaterial`, then the pixel's cluster decals applied to `Flux_SurfaceData` |
| 6 | write `Flux_SurfaceData` into the G-buffer-format targets |

- [Z2-N-REND-852] The material resolve reconstructs attributes from barycentrics with analytic gradients (never hardware derivatives, which a compute shader does not have across triangles), and writes `Flux_SurfaceData` in the G-buffer formats of `Zenith2/Docs/Rendering/PerPass/Deferred.md` "GBuffer".
- [Z2-N-REND-853] The surface-data targets are cleared to zero at their first use each frame (the first-write rule), because resolve dispatches cover only tiles with geometry; consumers read them only where the visibility ID is not `0xFFFFFFFF`.

### VelocityReconstruct

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/VisibilityBuffer/Velocity.slang` → `Flux_VelocityParams` |
| Feature function | `Flux_RGTexture Flux_AddVelocityReconstruct(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_ViewTargets& xTargets)` |
| Kind | compute, 8 × 8 groups over the render extent |
| Queue | G |
| Capability | `FLUX_CAPABILITY_TIER_DESKTOP` |
| Budget row | `Z2-B-RPROD-019` Velocity reconstruction GPU |
| Permutations | none |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_xVisibilityID` | `Zenith_RHISRV<Zenith_RHITex2D>` | read | compute |
| `m_xVisibleMeshlets`, `m_xInstances`, `m_xPrimitives` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` | read | compute |
| `m_xSkinnedCurrent`, `m_xSkinnedPrevious` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` `[Optional]` | read | compute |
| `m_xPositions` | `Zenith_RHIDescriptorHandle` | read | compute |
| `m_xVelocity` | `Zenith_RHIUAV<Zenith_RHITex2D>` `[FullCoverage]` `[WriteOnly]` | write | compute |
| data | `Zenith_RHIFloat4x4 m_xViewProjectionUnjittered`, `m_xPrevViewProjectionUnjittered` | — | — |

| Output | Format |
|---|---|
| velocity | RG16F (zero where the visibility ID is `0xFFFFFFFF`, camera motion only) |

- [Z2-N-REND-854] Velocity reconstruction uses the instance's previous transform and, for skinned instances, the skinned pool's previous half, so skinned and morphed geometry gets correct motion vectors, and a history-reset instance produces zero object motion.
