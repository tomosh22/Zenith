# Zenith2 rendering: per-pass specifications, Forward+

Section file of `Zenith2/Docs/Rendering.md`. It details the entry-file section "Per-pass specifications" for the passes unique to the Forward+ path; the Forward path reuses its prepass. Normative statements share the `REND` tag family of the entry file.

## Conventions

The tables follow `Zenith2/Docs/Rendering/PerPass/Common.md` "Conventions". Shared passes of the Forward+ frame (culling, HiZ, clustering, fog, GTAO, contact shadows, sky, SSR, transparent, particles, epilogue) are specified there.

- [Z2-N-REND-820] The Forward+ passes below are implemented exactly as their tables specify (Z2-D-085 for any change), and Forward+ is the one path on `FLUX_CAPABILITY_TIER_MOBILE`, so every pass here also meets the mobile capability tier.

## Forward+ passes

### ForwardPlusPrepass

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/ForwardPlus/Prepass.slang` → `Flux_PrepassParams` |
| Feature function | `void Flux_AddForwardPlusPrepass(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, const Flux_CullOutput& xCull, Flux_ViewTargets& xTargets)` (called once per phase) |
| Kind | raster; per phase, one `DrawIndexedIndirectCount` per depth bucket, opaque buckets first, then alpha-tested buckets (`[AlphaTest]` variant) |
| Queue | G |
| Capability | all tiers |
| Budget row | `Z2-B-RPROD-006` Depth velocity normal prepass GPU, and on the S24 `Z2-B-RPROD-026` HiZ mid-frame depth store and reload bandwidth |
| Permutations | `bPERM_ALPHA_TEST` {false, true}, `bPERM_SKINNED` {false, true}, stream-sets, material variants (alpha-tested buckets only; opaque buckets use the material-independent program) |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_xInstances`, `m_xPrimitives`, `m_xMaterialSlots` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` | read | vertex, pixel |
| `m_xDrawRecords` | `Zenith_RHISRV<Zenith_RHIStructBuffer<Flux_DrawRecord>>` | read | vertex |
| `m_xVisibleInstances` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` | read | vertex |
| `m_xSkinnedCurrent`, `m_xSkinnedPrevious` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` `[Optional]` | read | vertex |
| `m_xProceduralPool` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` `[Optional]` | read | vertex |
| `m_xFrameEnvironment` | as `Zenith2/Docs/Rendering/PerPass/Common.md` "Conventions" (the frame environment, for material vertex modifiers) | read | vertex, pixel |
| data | `Zenith_RHIFloat4x4 m_xViewProjection` (jittered), `m_xViewProjectionUnjittered`, `m_xPrevViewProjectionUnjittered`, `uint32 m_uBucketRecordBase` | — | — |

| Target | Format | Phase 1 load / store | Phase 2 load / store |
|---|---|---|---|
| depth | D24S8, or D32F_S8 (precision or capability) | `CLEAR` (1.0 standard, 0.0 reversed), `STORE` | `LOAD`, `STORE` |
| velocity | RG16F (screen-space motion in UV units, unjittered) | `CLEAR` (0, 0), `STORE` | `LOAD`, `STORE` |
| normals | RG16_UNORM (octahedral view-space normal) | `CLEAR`, `STORE` | `LOAD`, `STORE` |
| roughness | R8_UNORM (perceptual roughness) | `CLEAR`, `STORE` | `LOAD`, `STORE` |

Raster state: depth test `LESS_OR_EQUAL` (standard) or `GREATER_OR_EQUAL` (reversed), write on, cull back unless two-sided. The vertex stage reads stream 0 (position) and stream 1 (QTangent, UV0); the shadow index buffer (`meshopt_generateShadowIndexBuffer`) is not used here, because the prepass reads stream 1 for normals and velocity. A material with a vertex modifier is evaluated twice, with the current and the previous frame's time and transform, so velocity is correct for displaced vertices ([Z2-N-REND-258]).

- [Z2-N-REND-821] The prepass normal target holds the octahedral encoding (`Zenith2/Docs/Rendering/VertexData.md` "Octahedral codec") of the interpolated vertex normal for opaque buckets drawn with the material-independent program, and of the normal-mapped shading normal for alpha-tested buckets and for opaque material variants whose `bPERM_PREPASS_NORMAL_MAP` is true.

### ForwardPlusOpaque

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/ForwardPlus/Opaque.slang` → `Flux_OpaqueParams` |
| Feature function | `void Flux_AddForwardPlusOpaque(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, const Flux_CullOutput& xCull, Flux_ViewTargets& xTargets)` |
| Kind | raster; one `DrawIndexedIndirectCount` per colour bucket over the union of both culling phases' draws |
| Queue | G |
| Capability | all tiers |
| Budget row | `Z2-B-RPROD-009` Forward opaque GPU |
| Permutations | material variants, stream-sets, `uPERM_SHADOW_QUALITY` {0, 1, 2}, `bPERM_FOG` {false, true}, `bPERM_SSAO` {false, true}, `bPERM_CONTACT_SHADOWS` {false, true}, `bPERM_DECALS` {false, true}, `bPERM_SKINNED` {false, true}, `bPERM_SSR_WEIGHT` {false, true} (the second colour target, set exactly when the view's mask carries `FLUX_VIEW_FEATURE_SSR`) |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_xInstances`, `m_xPrimitives`, `m_xMaterialSlots`, `m_xDrawRecords`, `m_xVisibleInstances` | as "ForwardPlusPrepass" | read | vertex, pixel |
| `m_xSkinnedCurrent`, `m_xProceduralPool` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` `[Optional]` | read | vertex |
| `m_xClusterLists`, `m_xLightTable` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` | read | pixel |
| `m_xDecalRecords` | `Zenith_RHISRV<Zenith_RHIStructBuffer<Flux_DecalRecord>>` `[Optional]` | read | pixel |
| `m_xShadowAtlas` | `Zenith_RHISRV<Zenith_RHITex2D>` `[Optional]` (a frame with no shadowed light has no atlas) | read | pixel |
| `m_xShadowViews` | `Zenith_RHISRV<Zenith_RHIStructBuffer<Flux_ShadowViewRecord>>` `[Optional]` (frame data, located by `m_uShadowViewBase`) | read | pixel |
| `m_xAO`, `m_xContactShadows` | `Zenith_RHISRV<Zenith_RHITex2D>` `[Optional]` | read | pixel |
| `m_xFogVolume` | `Zenith_RHISRV<Zenith_RHITex3D>` `[Optional]` | read | pixel |
| `m_xIBLSpecular`, `m_xIBLIrradiance`, `m_xBRDFLUT` | `Zenith_RHIDescriptorHandle` (persistent read-only); from Stage 4 `m_xIBLSpecular` and `m_xIBLIrradiance` take the graph types of `Zenith2/Docs/Rendering/PerPass/Common.md` "Lighting pass fields" | read | pixel |
| the remaining fields of "Lighting pass fields" | as listed there (the light list, the transmittance LUT, the cloud shadow map, probes, GI, lightmap pages) | read | pixel |
| `m_xFrameEnvironment` | as `Zenith2/Docs/Rendering/PerPass/Common.md` "Conventions" | read | vertex, pixel |
| data | `Zenith_RHIFloat4x4 m_xViewProjection`, `Zenith_RHIFloat3 m_xCameraPositionWS`, `Zenith_RHIUInt3 m_xClusterGrid`, `Zenith_RHIUInt2 m_xClusterTileSize`, `Zenith_RHIFloat2 m_xClusterDepthRange`, `float m_fExposure`, `uint32 m_uBucketRecordBase`, and the plain data of "Lighting pass fields" | — | — |

| Target | Format | Load / store |
|---|---|---|
| HDR colour | R11G11B10F, render extent | `CLEAR` (0, 0, 0), `STORE` |
| specular weight (`m_xSpecularWeight`) | R11G11B10F, render extent, attached exactly when `FLUX_VIEW_FEATURE_SSR` is set (`bPERM_SSR_WEIGHT`); without the bit it is not attached and the output's write mask is 0 ([Z2-N-REND-919], [Z2-N-REND-921]) | `CLEAR` 0, `STORE` |
| depth | as the prepass | read-only, `LOAD`; test `EQUAL`, write off |

The pixel stage fills `Flux_MaterialInput` (explicit gradients from `ddx`/`ddy` at entry), calls `EvaluateMaterial`, applies the pixel's cluster decals to `Flux_SurfaceData`, calls `EvaluateLightingEx<Flux_ClusterLightSource>`, applies froxel fog, writes pre-exposure HDR radiance and, with `bPERM_SSR_WEIGHT`, the environment specular weight SSR composites with; it contains no discard.

- [Z2-N-REND-822] Forward+ opaque binds the depth target read-only with an `EQUAL` test, contains no `discard`, and shades each visible pixel exactly once (no overdraw), for opaque and alpha-tested buckets alike.
