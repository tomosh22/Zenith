# Flux_SSR - Screen Space Reflections System

## Overview

The SSR (Screen Space Reflections) system provides real-time reflections by ray marching through the Hi-Z depth buffer. It integrates seamlessly with the deferred shading pipeline, blending with IBL (Image-Based Lighting) reflections where SSR lacks coverage.

## Architecture

### Four-pass pipeline (mirrors SSGI)

The render graph schedules these passes via Read/Write declarations; there is no explicit ordering enum. Typical resolved order:

| Pass | Reads | Writes |
|------|-------|--------|
| HiZ generation | scene depth | HiZ chain (mip pyramid) |
| SSR raymarch (half-res) | HiZ chain, G-buffer normals/material | SSR raymarch target + aux (half-res, dual-MRT) |
| SSR upsample | SSR raymarch + aux (half-res), depth | SSR full-res target + aux (dual-MRT) |
| SSR DenoiseH (horizontal, optional) | SSR upsampled + aux, depth/normals/material | DenoiseH accumulator + confidence (dual-MRT) |
| SSR DenoiseV (vertical, optional) | DenoiseH accumulators, upsampled + aux | DenoiseV (final denoised reflection) |
| Deferred shading | SSR reflection (DenoiseV or upsampled), G-buffer, IBL | HDR scene |

The raymarch pass runs at **half resolution** for ~75% pixel-shader cost reduction; the upsample pass reconstructs the full-resolution output via a depth-weighted 4x4 KNN bilateral gather (16 candidates, top-4 selected). The roughness blur is a **separable bilateral denoise** split into two passes (DenoiseH horizontal + DenoiseV vertical) gated on `m_bSSRRoughnessBlurEnabled` via `ApplyBlurSelectionToGraph`; when disabled the deferred shader reads the upsampled output directly — never the raw half-res raymarch. The committed handle the deferred pass reads (`GetReflectionHandle`) is tracked by a `Flux_CommittedHandleSelector<bool>` that triggers a graph rebuild when the live toggle diverges from the committed selection.

### Files

| File | Purpose |
|------|---------|
| [Flux_SSRImpl.h](Flux_SSRImpl.h) | Class declaration (`Flux_SSRImpl`), debug mode enum, transient/pass handles, selector |
| [Flux_SSR_Shaders.h](Flux_SSR_Shaders.h) | Shader decls + `apxALL` array (feature-owned) |
| [Flux_SSR.cpp](Flux_SSR.cpp) | Implementation, pipeline setup |
| [../Shaders/SSR/Flux_SSR_RayMarch.slang](../Shaders/SSR/Flux_SSR_RayMarch.slang) | Half-res HiZ ray marching |
| [../Shaders/SSR/Flux_SSR_Upsample.slang](../Shaders/SSR/Flux_SSR_Upsample.slang) | Depth-weighted 4x4 KNN bilateral upsample (half→full) |
| [../Shaders/SSR/Flux_SSR_DenoiseH.slang](../Shaders/SSR/Flux_SSR_DenoiseH.slang) | Roughness bilateral blur, horizontal pass (optional) |
| [../Shaders/SSR/Flux_SSR_DenoiseV.slang](../Shaders/SSR/Flux_SSR_DenoiseV.slang) | Roughness bilateral blur, vertical pass (optional) |
| [../Shaders/SSR/Flux_SSR_DenoiseCommon.slang](../Shaders/SSR/Flux_SSR_DenoiseCommon.slang) | Shared denoise utility functions |

### Constants buffer

`SSRConstants` is a CBV (`Flux_DynamicConstantBuffer`), not push constants — required because the cached HiZ mip-size array (`u_axHiZMipSizes[12]`) exceeds the 128-byte push limit, and to honour the project convention of passing texture dimensions through the constant buffer rather than calling `GetDimensions` inside shaders. Uploaded once per frame in the RayMarch pass; bound by the RayMarch and Upsample passes (the denoise passes bind only `FrameConstants`).

The CBV holds (in addition to tuning fields):
- Half-res output dimensions (`u_fHalfResWidth/Height`, `u_fRcpHalfResWidth/Height`) — read by the upsample shader to find the half-res neighbourhood without `GetDimensions(g_xSSRTex)`.
- Cached HiZ per-mip dimensions (`u_axHiZMipSizes[12].xyzw` = w, h, 1/w, 1/h) — read by the ray-march loop to pick the right texel size at each mip level without `GetDimensions(g_xHiZTex, mip, ...)` in the inner loop.

## Implementation Details

### Render Targets

All targets are graph transients held as `Flux_TransientHandle` members on `Flux_SSRImpl` (created in `SetupRenderGraph`), not static render-targets. RayMarch and Upsample are **dual-MRT**: RT0 carries colour/confidence, RT1 (the `*Aux` target) carries hit metadata (hit UV, ray travel distance, ray count) used by the denoise passes for BRDF reuse and variance estimation. Every SSR pass declares `.ClearTargets()` on `AddPass(...)` so its render targets start each frame cleared (no stale-content reuse across frames).

| Target (handle) | Resolution | Format | Purpose |
|--------|------------|--------|---------|
| `m_xRayMarchHandle` | Half | RGBA16F | RT0: RGB = reflected color, A = hit confidence |
| `m_xRayMarchAuxHandle` | Half | RGBA16F | RT1: hit metadata (UV / travel distance / ray count) |
| `m_xUpsampledHandle` | Full | RGBA16F | Bilateral upsample to full-res (always-on) — canonical SSR output when blur is off |
| `m_xUpsampledAuxHandle` | Full | RGBA16F | Upsampled aux metadata (reconstructed with the same depth weights) |
| `m_xDenoiseHHandle` | Full | RGBA16F | DenoiseH RT0: (Σw·color, Σw) accumulator |
| `m_xDenoiseHConfHandle` | Full | RGBA16F | DenoiseH RT1: (Σw·conf) confidence accumulator |
| `m_xDenoiseVHandle` | Full | RGBA16F | Final denoised reflection (deferred consumes this when blur is on) |

### Pass 1: Ray Marching (half-res)

**Inputs:**
- G-Buffer normals (`MRT_INDEX_NORMALSAMBIENT`)
- G-Buffer material (`MRT_INDEX_MATERIAL`) - roughness
- G-Buffer diffuse (`MRT_INDEX_DIFFUSE`) - color source for hit sampling
- Depth buffer
- Hi-Z buffer from `Flux_HiZ::GetHiZSRV()`

**Algorithm:**
1. Reconstruct world position from depth
2. Compute reflection vector: `R = reflect(V, N)`
3. Transform ray to screen space
4. Hierarchical Z (HiZ) ray march with an adaptive per-pixel step budget and binary-search refinement
5. Sample color at hit position
6. Apply edge fade for screen borders
7. Early-out for roughness > `MaxRoughness`

**Output:** `s_xRayMarchResult` (RGBA16F)

### Pass 2: Bilateral Upsample (half→full)

**Purpose:** Reconstruct full-res reflections from the half-res ray-march output, edge-aware via depth weights.

**Algorithm:**
- 4x4 KNN bilateral gather: scores 16 half-res candidates around the full-res target pixel by composite (depth + normal + roughness) similarity, then accumulates the top-4 weighted by inverse texel distance
- Depth-weighted bilateral filter (depth-relative sigma — 2% of local depth)
- Sky pixels short-circuit to zero
- Half-res dimensions read from the SSR CBV (no in-shader `GetDimensions`)
- The wider 4x4 KNN gather (upgraded from the legacy 2x2 bilateral) reaches across depth/normal discontinuities and drops mismatched candidates before they corrupt the accumulator

**Output:** `m_xUpsampledHandle` (full-resolution RGBA16F) + `m_xUpsampledAuxHandle`. This is the "no roughness blur" final output — deferred reads it directly when `m_bSSRRoughnessBlurEnabled` is off.

### Pass 3 + 4: Denoise (separable bilateral, optional, full-res)

**Purpose:** Apply roughness-based bilateral blur for rough surfaces, split into a **horizontal** (DenoiseH) and **vertical** (DenoiseV) pass that must run together. Reads the upsampled output (not the raw raymarch). Both passes are enabled/disabled as a unit by `ApplyBlurSelectionToGraph` (gated on `m_bSSRRoughnessBlurEnabled`).

**Algorithm:**
- Roughness gating inside the shader skips mirror-smooth and very-rough pixels (the latter handled by IBL)
- Bilateral weights driven by the `SSRDenoiseConstants` tuning knobs: `SpatialSigma`, `DepthSigma`, `NormalSigma`, `RoughnessSigma`, `KernelRadius`
- DenoiseH is dual-MRT: RT0 = the (Σw·color, Σw) accumulator, RT1 = the parallel (Σw·conf) accumulator; aux metadata enables BRDF reuse
- DenoiseV reads H's accumulators + the upsampled colour (passthrough fallback) + aux, applies its own bilateral × BRDF kernel, and divides numerator/denominator to produce the final RGBA

**Output:** DenoiseH → `m_xDenoiseHHandle` + `m_xDenoiseHConfHandle`; DenoiseV → `m_xDenoiseVHandle` (the final denoised reflection the deferred shader consumes)

### Deferred Shading Integration

SSR is blended with IBL specular in `ComputeIBLAmbient()`. **Fresnel is applied here, not in SSR** (per industry standard):

```hlsl
// In Flux_DeferredShading.slang
if (DeferredShadingConstants.g_bSSREnabled != 0)
{
    float4 xSSR     = g_xSSRTex.Sample(xUV);
    fSSRConfidence  = xSSR.a;

    // Apply Fresnel weighting - produces stronger reflections at grazing angles.
    // SSR uses plain FresnelSchlick(NdotV, F0) (no roughness term); the
    // roughness-aware FresnelSchlickRoughness() variant is used for IBL.
    float3 xSSRFres = FresnelSchlick(NdotV, F0);
    xSSRSpecular    = xSSR.rgb * xSSRFres;
}
// Blend: high confidence = SSR, low confidence = IBL fallback
```

**Why Fresnel is NOT applied in SSR:** The SSR shader outputs raw reflected color. Applying Fresnel in the deferred shader ensures:
1. Same Fresnel applied to both SSR and IBL (seamless blending)
2. Energy conservation maintained
3. Matches industry standard (FidelityFX SSSR, UE4)

## Public Interface

`Flux_SSRImpl` is an **instance** class (one held by the engine), reached via `g_xEngine.SSR()` — its methods are **not** static. It derives from the CRTP base `Flux_ScreenSpaceEffectBase<Flux_SSRImpl>`, which supplies the shared lifecycle (`Shutdown()` calls the `ShutdownImpl()` hook below; the base owns `m_pxGraph` / `m_bInitialised`).

```cpp
class Flux_SSRImpl : public Flux_ScreenSpaceEffectBase<Flux_SSRImpl>
{
public:
    void Initialise();
    void BuildPipelines();
    void ShutdownImpl();                       // CRTP hook from the base's Shutdown()

    void SetupRenderGraph(Flux_RenderGraph&);
    void ApplyBlurSelectionToGraph(Flux_RenderGraph&);
    void UpdateSSRConstants();

    // For deferred shading to sample
    Flux_TransientHandle      GetReflectionHandle() const;
    Flux_ShaderResourceView&  GetReflectionSRV();
    bool                      IsEnabled() const;
};
```

Call sites use the instance, e.g. `g_xEngine.SSR().GetReflectionSRV()`. The non-capturing Execute*/hot-reload trampolines recover the instance via `g_xEngine.SSR()`.

## Debug Variables

Enable/disable and roughness-blur toggle are **Graphics Options** (`Zenith_GraphicsOptions::m_bSSREnabled` / `m_bSSRRoughnessBlurEnabled`), registered under the `Graphics` scope — not Flux-scoped tuning vars:

| Path | Type | Range | Default | Description |
|------|------|-------|---------|-------------|
| `Graphics/SSR/Enabled` | bool | - | true | Enable/disable SSR (Graphics Options) |
| `Graphics/SSR/RoughnessBlur` | bool | - | true | Enable the separable denoise blur (Graphics Options) |

The remaining tuning knobs are registered under the `Flux` scope (TOOLS only):

| Path | Type | Range | Default | Description |
|------|------|-------|---------|-------------|
| `Flux/SSR/DebugMode` | uint | 0-100 | 0 | Visualization mode (99 = solid magenta proof-of-life) |
| `Flux/SSR/Intensity` | float | 0-2 | 1.0 | Reflection intensity multiplier |
| `Flux/SSR/MaxDistance` | float | 1-100 | 50.0 | Max ray march distance (world units) |
| `Flux/SSR/MaxRoughness` | float | 0-1 | 1.0 | Skip SSR above this roughness |
| `Flux/SSR/StepCount` | uint | 8-256 | **32** | Ray march iterations (lowered from 64; HiZ acceleration converges quickly) |
| `Flux/SSR/StartMip` | uint | 0-10 | 5 | Starting HiZ mip level for hierarchical traversal |
| `Flux/SSR/Thickness` | float | 0.01-1 | 0.5 | Surface thickness for hit detection (m) |
| `Flux/SSR/ContactHardeningDist` | float | 0.5-10 | 2.0 | World-space distance over which contact-hardening confidence ramps in (m) |
| `Flux/SSR/RaysPerPixel` | uint | 1-4 | 1 | Reflection rays per pixel |
| `Flux/SSR/StepCountMin` | uint | 4-64 | 16 | Min march steps (high-roughness / centre-of-frame floor) |
| `Flux/SSR/Denoise/SpatialSigma` | float | 0.5-6 | 1.5 | Denoise spatial Gaussian sigma |
| `Flux/SSR/Denoise/DepthSigma` | float | 0.001-0.1 | 0.02 | Denoise depth-weight sigma |
| `Flux/SSR/Denoise/NormalSigma` | float | 0.05-1 | 0.2 | Denoise normal-similarity sigma |
| `Flux/SSR/Denoise/RoughnessSigma` | float | 0.01-0.5 | 0.1 | Denoise roughness-similarity sigma |
| `Flux/SSR/Denoise/KernelRadius` | uint | 1-8 | 4 | Denoise kernel radius |
| `Flux/SSR/Textures/RayMarch` | texture | - | - | Raw half-res ray-march result (RT0) |
| `Flux/SSR/Textures/RayMarchAux` | texture | - | - | Half-res ray-march aux metadata (RT1) |
| `Flux/SSR/Textures/Upsampled` | texture | - | - | Bilateral upsample (full-res, pre-blur) |
| `Flux/SSR/Textures/UpsampledAux` | texture | - | - | Upsampled aux metadata (full-res) |
| `Flux/SSR/Textures/DenoiseH` | texture | - | - | DenoiseH accumulator (full-res) |
| `Flux/SSR/Textures/DenoiseHConf` | texture | - | - | DenoiseH confidence accumulator (full-res) |
| `Flux/SSR/Textures/DenoiseV` | texture | - | - | Final denoised result (full-res, when RoughnessBlur is on) |

## Debug Visualization Modes

From the `SSR_DebugMode` enum in [Flux_SSRImpl.h](Flux_SSRImpl.h):

| Mode | Value | Output |
|------|-------|--------|
| `SSR_DEBUG_NONE` | 0 | Normal rendering |
| `SSR_DEBUG_RAY_DIRECTIONS` | 1 | RGB = reflection direction |
| `SSR_DEBUG_SCREEN_DIRECTIONS` | 2 | Screen-space ray directions |
| `SSR_DEBUG_HIT_POSITIONS` | 3 | RGB = hit world position |
| `SSR_DEBUG_REFLECTION_UVS` | 4 | RG = hit screen UV |
| `SSR_DEBUG_CONFIDENCE` | 5 | Grayscale confidence |
| `SSR_DEBUG_DEPTH_COMPARISON` | 6 | Depth comparison diagnostic |
| `SSR_DEBUG_EDGE_FADE` | 7 | Screen edge fadeout |
| `SSR_DEBUG_MARCH_DISTANCE` | 8 | Ray march distance |
| `SSR_DEBUG_FINAL_RESULT` | 9 | Final resolved reflection |
| `SSR_DEBUG_ROUGHNESS` | 10 | Roughness |
| `SSR_DEBUG_WORLD_NORMAL_Y` | 11 | World-normal Y component |
| `SSR_DEBUG_RAY_COUNT` | 12 | Rays cast per pixel |

## Edge Fade

SSR applies smooth fadeout at screen edges to avoid hard cutoffs. The fade band is **roughness-dependent**, not a fixed 20% border: `fEdgeMargin = SSR_EDGE_MARGIN_BASE + fRoughness * SSR_EDGE_MARGIN_ROUGHNESS` = `0.08 + roughness * 0.10`, yielding an 8%–18% margin. It is applied via `ComputeEdgeFade(xFinalUV, fEdgeMargin)` (see `Flux_SSR_RayMarch.slang`).

## Confidence Calculation

The alpha channel encodes hit confidence based on **geometric factors only** (per industry standard - FidelityFX SSSR, UE4):
- Edge fade factor (screen border)
- Distance fade (ray travel distance)
- Depth confidence (hit accuracy)
- Backface confidence (surface orientation)
- Stretch confidence (ray/surface angle - prevents stretching artifacts)

**Important:** SSR confidence is NOT based on viewing angle (NdotV). The deferred shader applies `FresnelSchlick(NdotV, F0)` which naturally produces stronger reflections at grazing angles. Fading SSR confidence at grazing angles would fight against this physics.

```glsl
fConfidence = fEdgeFade * fDistanceFade * fDepthConfidence * fBackfaceConfidence * fStretchConfidence;
```

## Performance Considerations

- **Half-res ray march:** Dominant cost saving — ~75% pixel-shader reduction vs the historical full-res implementation. All quality tuning happens against the half-res baseline.
- **Step Count (default 32):** Higher = better quality, more expensive. With HiZ acceleration plus binary-search refinement, 32 is sufficient for most scenes. Match SSGI's default.
- **Binary search iterations:** Auto-scales 4 (≤1920px width) / 5 (>1920px width) — the ~1080p / 1440p breakpoint. Half-res origins make sub-1/16-pixel precision wasted.
- **MaxRoughness:** Skip very rough surfaces (use IBL only).
- **RoughnessBlur:** Adds the two separable denoise passes (DenoiseH + DenoiseV); cost scales with `Denoise/KernelRadius` (default 4).
- **HiZ mip dimensions:** Cached in the SSR CBV (`u_axHiZMipSizes[12]`) so the ray-march inner loop never calls `GetDimensions` — eliminates per-iteration driver overhead.
- **Frame index pinned to 0:** SSR has no temporal accumulation, so the blue-noise frame index (`m_uFrameIndex`) is pinned to 0 (see `Flux_SSR.cpp`) — rotating the noise field per frame would produce visible shimmer rather than integrating across frames. The blue noise is still used for spatial dithering, multi-ray spread, and the denoise passes; only the per-frame rotation is suppressed.

## Known Limitations

1. **Off-screen reflections:** Cannot reflect objects outside screen bounds
2. **Self-occlusion:** Objects may incorrectly reflect themselves
3. **Thin objects:** May miss very thin geometry
4. **Glossy surfaces:** Limited blur kernel size affects very rough surfaces

## Dependencies

- **Flux_HiZ:** Must be enabled for SSR to function
- **G-Buffer:** Requires normals, material, depth, diffuse
- **Deferred Shading:** Consumes SSR result in lighting pass
