# Bindless Texture Coverage Audit (Phase-3 work-list)

> Produced in the pre-Phase-3 hardening step of the Flux binding-model overhaul. This is the verified
> work-list for **maximal** Phase 3 (every sampled texture → the bindless `g_axTextures[]` table, set 2,
> declared in [Shaders/Common/Bindings.slang](Shaders/Common/Bindings.slang) as `g_xBindlessSet.g_axTextures`).
> Evidence is file:line against the committed tree (`f96ec370`). Update as consumers migrate.

## How to read the uniformity column
A consumer that, after migration, computes its `g_axTextures[idx]` index from a **per-draw constant** (a
material index in draw constants) is **DRAW-UNIFORM** → plain `g_axTextures[idx]`. A consumer whose index
varies **within a wave** (per-vertex attribute, per-instance, per-fragment, per-texel) is **NON-UNIFORM** →
the index MUST be wrapped in `NonUniformResourceIndex(idx)` (the `shaderSampledImageArrayNonUniformIndexing`
feature is already enabled — `Zenith_Vulkan.cpp` device create). Wrapping a draw-uniform index is harmless but
unnecessary.

## Material/texture-table candidates (move to `g_axTextures[]` in Phase 3)

| Consumer | Current sampling (file:line) | Index after migration | Uniformity | NonUniformResourceIndex? | Cube/2D | Effort |
|---|---|---|---|---|---|---|
| **Quads / UI** | `Flux_Quads.slang:77,85` — **already** `g_xBindlessSet.g_axTextures[input.texture]` | per-vertex attribute (`a_uTexture`, TEXCOORD3) | **NON-UNIFORM** | **YES — currently MISSING, add it** | 2D | already bindless; just annotate |
| StaticMeshes G-buffer | `Flux_StaticMesh_ToGBuffer.slang:52-60` — 9 named `Sampler2D` | per-draw material index in draw constants | DRAW-UNIFORM | no | 2D | HIGH (9 textures × EvaluateMaterialSurface) |
| StaticMeshes shadow | `Flux_StaticMesh_ToShadowmap.slang:48` — 1 `Sampler2D` (base, cutout) | per-draw material index | DRAW-UNIFORM | no | 2D | LOW |
| AnimatedMeshes G-buffer / shadow | `Flux_AnimatedMesh_ToGBuffer.slang:57-65` (+ `_ToShadowmap`) — 9 + 1 | per-draw material index | DRAW-UNIFORM | no | 2D | HIGH |
| InstancedMeshes G-buffer | `Flux_InstancedMesh_ToGBuffer.slang:80-92` — 9 + VAT anim tex | per-draw material index (VAT tex is per-instance vertex anim, draw-uniform index) | DRAW-UNIFORM | no | 2D | HIGH |
| Terrain G-buffer | `Flux_Terrain_ToGBuffer.slang:74-99` — 20 named `Sampler2D` (4 mat × 5) + splatmap | 4 material base indices in draw/pass constants; splatmap **weights** are per-fragment but the 4 **indices** are pass-constant | DRAW-UNIFORM (indices) | no | 2D | **Phase 4** (collapse via splatmap) |
| Translucency forward | `Flux_Translucent_Forward.slang:97-105` — 9 (set 4) | per-draw material index | DRAW-UNIFORM | no | 2D | HIGH |
| MaterialPreview forward | `Flux_MaterialPreview_Forward.slang:99-107` — 9 (set 4) | per-draw material index | DRAW-UNIFORM | no | 2D | HIGH (tools) |
| Text / font atlas | `Flux_Text.slang:65-100` — 1 `Sampler2D` (MSDF atlas, set 3) | per-pass atlas index | DRAW-UNIFORM (pass-constant) | no | 2D | LOW |
| Particles | `Flux_Particles.slang:18` — 1 `Sampler2D` (set 3) | per-pass billboard index | DRAW-UNIFORM (pass-constant) | no | 2D | LOW |
| Decals (textured-brush) | `Flux_Decals_Apply.slang:49` — 1 `Sampler2D` `g_xBrushTex` (set 3) | per-pass brush index | DRAW-UNIFORM (pass-constant) | no | 2D | LOW (procedural mode samples nothing) |

**Net:** the only **non-uniform** material/texture consumer is **Quads/UI** (per-vertex index) — and it already
samples `g_axTextures[]` but **lacks** `NonUniformResourceIndex`, a latent correctness bug to fix in Phase 3 B4.
Every other candidate is draw- or pass-uniform.

## STATUS — Phase 3 maximal-bindless COMPLETE (2026-06-26)
All sampled-2D consumers now route through `g_xBindlessSet.g_axTextures[]`:
- **Materials** (Static/Animated/Instanced mesh G-buffer+shadow, Translucency, MaterialPreview) — B2/B3, `46f39273`.
- **Quads/UI** — `NonUniformResourceIndex` added, `f6947796`.
- **Text** (MSDF atlas) — index in `TextConstants.g_uAtlasIdx` (pass-uniform).
- **Particles** (billboard) — index in a new `ParticleConstantsLayout.g_uTexIdx` PASS CB (pass-uniform).
- **Decals** (brush) — index per-decal in `m_xParams.z` (asuint), sampled with `NonUniformResourceIndex` (per-instance).

Skybox/IBL stay OUT (cubemaps, not the 2D table). A future per-material env map would need a separate `g_xCubemaps[]`.

## Out of the 2D material table (do NOT migrate)

- **Runtime cubemaps** — Skybox (`Flux_Skybox.slang:19` `SamplerCube`), IBL irradiance/prefiltered + the
  irradiance-convolution / prefilter inputs (`Flux_IrradianceConvolution.slang:29`, `Flux_PrefilterEnvMap.slang`).
  These are view/pass cube resources, NOT per-draw material cubemaps. `g_axTextures[]` is a 2D
  `Sampler2D` array. If per-material env maps are ever needed, add a **separate** `g_xCubemaps[]` bindless array
  to the BINDLESS set — out of scope for the 2D table.
- **Scene/view resources** — G-buffer MRTs, depth, CSM cascades, BRDF LUT, HiZ pyramid, SSR/SSGI/SSAO
  intermediates, HDR/bloom targets, fog. Sampled by DeferredShading / SSR / SSGI / SSAO / HiZ / Fog / HDR. These
  are view/pass frequency and belong in the VIEW set (Phase 5), never the material table.
- **IBL-source caveat for "maximal" scope:** the disk-loaded **2D equirect source** image (and any 2D skybox
  source) CAN become a bindless 2D texture on upload; the *runtime-baked cubemaps* derived from it cannot (see
  cubemaps above). "Skybox/IBL-source bindless" therefore means the 2D source inputs only.

## Current `NonUniformResourceIndex` usage: ZERO
Grep of `Flux/Shaders` found no `NonUniformResourceIndex`. The only site that needs it today is
`Flux_Quads.slang:77,85`. Add it there in Phase 3 and at every future per-texel/per-instance index site
(terrain splat in Phase 4).
