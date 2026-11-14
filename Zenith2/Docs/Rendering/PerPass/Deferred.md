# Zenith2 rendering: per-pass specifications, Deferred

Section file of `Zenith2/Docs/Rendering.md`. It details the entry-file section "Per-pass specifications" for the passes unique to the Deferred path. Normative statements share the `REND` tag family of the entry file.

## Conventions

The tables follow `Zenith2/Docs/Rendering/PerPass/Common.md` "Conventions". Deferred lighting and every shared pass are specified in `Zenith2/Docs/Rendering/PerPass/Common.md`.

- [Z2-N-REND-840] The Deferred passes below are implemented exactly as their tables specify and run on `FLUX_CAPABILITY_TIER_DESKTOP` alone, with every G-buffer target at most 16 bits per channel and no MSAA.

## Deferred passes

### DeferredDepthPrepass

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/Deferred/DepthPrepass.slang` → `Flux_DepthPrepassParams` |
| Feature function | `void Flux_AddDeferredDepthPrepass(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, const Flux_CullOutput& xCull, Flux_ViewTargets& xTargets)` (once per phase) |
| Kind | raster, depth only; opaque buckets through the position-only index buffer and position stream, then alpha-tested buckets (`[AlphaTest]`, positions and UV0) |
| Queue | G |
| Capability | `FLUX_CAPABILITY_TIER_DESKTOP` |
| Budget row | `Z2-B-RPROD-006` Depth velocity normal prepass GPU |
| Permutations | `bPERM_ALPHA_TEST` {false, true}, `bPERM_SKINNED` {false, true}, stream-sets, material variants (alpha-tested only) |

Parameters are those of `Zenith2/Docs/Rendering/PerPass/Common.md` "ShadowDepth" with the view's jittered view-projection.

| Target | Format | Phase 1 | Phase 2 |
|---|---|---|---|
| depth | D24S8, or D32F_S8 (precision or capability) | `CLEAR`, `STORE` | `LOAD`, `STORE` |

### GBuffer

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/Deferred/GBuffer.slang` → `Flux_GBufferParams` |
| Feature function | `void Flux_AddGBuffer(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, const Flux_CullOutput& xCull, Flux_ViewTargets& xTargets)` |
| Kind | raster; one `DrawIndexedIndirectCount` per colour bucket, both phases' draws |
| Queue | G |
| Capability | `FLUX_CAPABILITY_TIER_DESKTOP` |
| Budget row | `Z2-B-RPROD-014` G-buffer pass GPU |
| Permutations | material variants, stream-sets, `bPERM_SKINNED` {false, true}, `bPERM_ADVANCED_SURFACES` {false, true} (Stage 5: the material variant declares an advanced surface model, and the pass attaches and writes GB4) |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_xInstances`, `m_xPrimitives`, `m_xMaterialSlots`, `m_xDrawRecords`, `m_xVisibleInstances` | as `Zenith2/Docs/Rendering/PerPass/ForwardPlus.md` "ForwardPlusPrepass" | read | vertex, pixel |
| `m_xSkinnedCurrent`, `m_xSkinnedPrevious`, `m_xProceduralPool` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` `[Optional]` | read | vertex |
| `m_xFrameEnvironment` | as `Zenith2/Docs/Rendering/PerPass/Common.md` "Conventions" | read | vertex, pixel |
| `m_xLightmapPages` (Stage 5) | `Zenith_RHISRV<Zenith_RHIStructBuffer<Flux_LightmapPageRecord>>` `[Optional]` | read | pixel |
| data | `Zenith_RHIFloat4x4 m_xViewProjection`, `m_xViewProjectionUnjittered`, `m_xPrevViewProjectionUnjittered`, `uint32 m_uBucketRecordBase` | — | — |

| Target | Format | Content | Load / store |
|---|---|---|---|
| GB0 | mutable-format RGBA8: attached and sampled as `R8G8B8A8_SRGB` (hardware sRGB encode; decals blend in linear space), written as storage through its `R8G8B8A8_UNORM` view by the visibility-buffer resolve | albedo RGB, material AO | `CLEAR` (zero), `STORE` |
| GB1 | RG16_UNORM | octahedral world-space normal | as GB0 |
| GB2 | RGBA8_UNORM | roughness, metallic, specular, shading model (8-bit ID) | as GB0 |
| GB3 | R11G11B10F | emissive (linear, pre-exposure) | as GB0 |
| GB4 (Stage 5, `bPERM_ADVANCED_SURFACES` alone) | RGBA8_UNORM | the advanced-surface parameters by model: clear coat and coat roughness, or anisotropy and direction angle, or sheen colour, or subsurface colour and thickness (`Zenith2/Docs/Design/GEOM/Materials.md → "Advanced surface models"`) | as GB0; not attached in the base permutation |
| velocity | RG16F | screen-space motion, unjittered | as GB0 |
| depth | as the prepass | read-only, `LOAD`, test `EQUAL`, write off | — |

- [Z2-N-REND-841] The G-buffer pass uses an equal depth test with depth writes off and no discard, writes all five base colour targets (GB0–GB3 and velocity) in every permutation and GB4 in exactly the advanced-surfaces permutation, which is the one that attaches it, and clears them to zero at their first use each frame (pixels without geometry are not written, so `DONT_CARE` would break the first-write rule); consumers read the G-buffer only where depth is not the clear value.
- [Z2-N-REND-842] The G-buffer formats are exactly GB0 mutable-format RGBA8 (`R8G8B8A8_SRGB` attachment and sampled view, `R8G8B8A8_UNORM` storage view), GB1 RG16_UNORM, GB2 RGBA8_UNORM, GB3 R11G11B10F, velocity RG16F and, in the advanced-surfaces permutation of the desktop-only Deferred and visibility-buffer paths, GB4 RGBA8_UNORM, and the visibility-buffer material resolve writes the same formats; a frame with no resident material of an advanced surface model allocates no GB4.

### DeferredDecals

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/Deferred/Decals.slang` → `Flux_DeferredDecalParams` |
| Feature function | `void Flux_AddDeferredDecals(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_ViewTargets& xTargets)` |
| Kind | raster; one instanced unit-box draw of the frame decal list, back faces with the depth compare `Flux_DepthModeOps::GetInvertedCompare` of the view's depth mode, so a camera inside a decal box is handled |
| Queue | G |
| Capability | `FLUX_CAPABILITY_TIER_DESKTOP` |
| Budget row | `Z2-B-GEOM-006` Deferred decals GPU |
| Permutations | decal material variants, `uPERM_CHANNELS` {albedo, albedo+normal, albedo+normal+ORM} |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_xDecalList`, `m_xDecalRecords` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` | read | vertex, pixel |
| `m_xDepth` | `Zenith_RHISRV<Zenith_RHITex2D>` (sampled while bound read-only, `DEPTH_READ_ONLY` state) | read | pixel |

| Target | Format | Load / store |
|---|---|---|
| GB0, GB1, GB2 | as "GBuffer" | `LOAD`, `STORE`; blend per the decal's channel mask, write mask 0 on channels the permutation does not write |
| depth | as the prepass | read-only |

- [Z2-N-REND-843] Deferred decals modify the G-buffer after the G-buffer pass and before any reader of it, blend only the channels their permutation writes (write mask 0 elsewhere), and read depth through a read-only attachment.
<!-- elaboration -->
- [Z2-N-REND-844] GB0 is a mutable-format image (see "Required format features" in the entry file): the G-buffer pass, deferred decals and every reader use its `R8G8B8A8_SRGB` view, so hardware encodes and decodes sRGB and decal blending happens in linear space; the visibility-buffer material resolve, which cannot store through an sRGB view, writes the `R8G8B8A8_UNORM` storage view with the exact IEC 61966-2-1 encode `Flux_EncodeSRGB8` from `Zenith2/Shaders/Common/Sampling.slang`, and the graph selects the view format from the use (`Flux_RGTextureDesc::m_eStorageViewFormat` for storage views).
