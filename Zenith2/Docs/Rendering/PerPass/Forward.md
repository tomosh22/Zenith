# Zenith2 rendering: per-pass specifications, Forward

Section file of `Zenith2/Docs/Rendering.md`. It details the entry-file section "Per-pass specifications" for the passes unique to the Forward (non-clustered reference) path. Normative statements share the `REND` tag family of the entry file.

## Conventions

The tables follow `Zenith2/Docs/Rendering/PerPass/Common.md` "Conventions". The Forward frame reuses `Zenith2/Docs/Rendering/PerPass/ForwardPlus.md` "ForwardPlusPrepass" and every shared pass of `Zenith2/Docs/Rendering/PerPass/Common.md`; transparent forward runs with `bPERM_CLUSTERED` false.

- [Z2-N-REND-835] The Forward passes below are implemented exactly as their tables specify, run on `FLUX_CAPABILITY_TIER_DESKTOP` alone, and produce, on parity scenes with no more lights affecting any object than the per-object light cap `Z2-B-RPROD-013`, the same surface-data channels as Forward+ within the matrix tolerances.

## Forward passes

### PerObjectLightLists

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/Forward/PerObjectLights.slang` → `Flux_PerObjectLightsParams` |
| Feature function | `Flux_RGBuffer Flux_AddPerObjectLightLists(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, const Flux_CullOutput& xCull)` |
| Kind | compute, one thread per visible instance |
| Queue | G |
| Capability | `FLUX_CAPABILITY_TIER_DESKTOP` |
| Budget row | `Z2-B-RPROD-011` Per-object light list build GPU |
| Permutations | none |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_xInstances`, `m_xPrimitives` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` | read | compute |
| `m_xVisibleInstances` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` | read | compute |
| `m_xLightList`, `m_xDecalList`, `m_xLightTable` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` | read | compute |
| `m_xPerObjectLights` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` `[FullCoverage]`: per visible instance, `Z2-B-RPROD-013` Per-object light cap (pending; proposed 8) u16 light indices, 4 u16 decal indices and the counts (32 bytes at the proposed cap) | write | compute |

Selection (lights in the per-object list are the frame list's local lights plus the directional light, which `EvaluateLightingEx` evaluates once, from the frame light list, and skips in the per-object loop): for each instance, score every light intersecting its world bounds by `luminous intensity / max(d², r²)` at the bounds centre, keep the directional light, then the highest-scoring lights up to the per-object light cap in descending score with ties broken by light index; decals up to 4 by projected overlap.

- [Z2-N-REND-836] Per-object light selection follows the scoring and tie-break rule above, so the Forward path's light set per object is deterministic.

### ForwardOpaquePerObject

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/Forward/ForwardOpaquePerObject.slang` → `Flux_ForwardOpaquePerObjectParams` |
| Feature function | `void Flux_AddForwardOpaquePerObject(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, const Flux_CullOutput& xCull, Flux_RGBuffer xPerObjectLights, Flux_ViewTargets& xTargets)` |
| Kind | raster; as "ForwardPlusOpaque" with `EvaluateLighting<Flux_PerObjectLightSource>` and per-object decals |
| Queue | G |
| Capability | `FLUX_CAPABILITY_TIER_DESKTOP` |
| Budget row | `Z2-B-RPROD-012` Forward per-object opaque GPU |
| Permutations | as "ForwardPlusOpaque", `bPERM_SSR_WEIGHT` included |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| every field of `Flux_OpaqueParams` (the Forward+ opaque program's) except `m_xClusterLists` and `m_xDecalRecords` | as `Zenith2/Docs/Rendering/PerPass/ForwardPlus.md` "ForwardPlusOpaque" | read | vertex, pixel |
| `m_xPerObjectLights` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` | read | pixel |
| `m_xDecalTable` | `Zenith_RHISRV<Zenith_RHIStructBuffer<Flux_DecalRecord>>` | read | pixel |

| Target | Format | Load / store |
|---|---|---|
| HDR colour | R11G11B10F | `CLEAR`, `STORE` |
| specular weight (`m_xSpecularWeight`) | as `Zenith2/Docs/Rendering/PerPass/ForwardPlus.md` "ForwardPlusOpaque": attached exactly when `FLUX_VIEW_FEATURE_SSR` is set | `CLEAR` 0, `STORE` |
| depth | as the prepass | read-only, `EQUAL`, write off |

- [Z2-N-REND-837] Forward opaque with per-object lights uses the equal-depth test with no discard, applies per-object decals and in-shader froxel fog, and differs from Forward+ opaque only in its light and decal source.
