# Flux_SSGI - Screen Space Global Illumination System

## Overview

The SSGI (Screen Space Global Illumination) system provides real-time indirect diffuse lighting by ray marching through the Hi-Z depth buffer. Multiple rays per pixel sample the hemisphere above each surface, accumulating bounced light from nearby geometry.

The system uses a four-pass architecture:
1. **Ray March** (quarter-res by default) - Multi-ray hemisphere sampling with HiZ acceleration
2. **Upsample** - Bilateral upsample from half to full resolution
3. **Denoise H** - Horizontal half of the separable joint bilateral filter
4. **Denoise V** - Vertical half of the separable joint bilateral filter

The denoise is **separable**: O(2r) cost per pixel instead of O(r²) by running a
1D blur along X then along Y. Strict bilateral non-separability is mathematically
present but visually negligible at this kernel size.

## Architecture

### Pass dependencies

The render graph schedules these passes via Read/Write declarations; there is no explicit ordering enum. Typical resolved order:

| Pass | Reads | Writes |
|------|-------|--------|
| HiZ generation | scene depth | HiZ chain (mip pyramid) |
| SSGI raymarch | HiZ chain, G-buffer (normals, material, diffuse) | SSGI raymarch target |
| SSGI upsample | SSGI raymarch, depth | SSGI full-res |
| SSGI denoise H | SSGI full-res, depth | SSGI denoise intermediate |
| SSGI denoise V | SSGI denoise intermediate, depth | SSGI final |
| Deferred shading | SSGI final, G-buffer, IBL | HDR scene |

### Files

| File | Purpose |
|------|---------|
| [Flux_SSGIImpl.h](Flux_SSGIImpl.h) | Class declaration (`Flux_SSGIImpl`), debug mode enum, `Flux_SSGISelection` selector struct |
| [Flux_SSGI_Shaders.h](Flux_SSGI_Shaders.h) | Shader decls (`apxALL`) owned by the feature |
| [Flux_SSGI.cpp](Flux_SSGI.cpp) | Implementation, pipeline setup, render passes, render-graph setup |
| [../Shaders/SSGI/Flux_SSGI_RayMarch.slang](../Shaders/SSGI/Flux_SSGI_RayMarch.slang) | HiZ ray marching, hemisphere sampling |
| [../Shaders/SSGI/Flux_SSGI_Upsample.slang](../Shaders/SSGI/Flux_SSGI_Upsample.slang) | Bilateral upsample shader |
| [../Shaders/SSGI/Flux_SSGI_DenoiseH.slang](../Shaders/SSGI/Flux_SSGI_DenoiseH.slang) | Separable joint bilateral — horizontal half |
| [../Shaders/SSGI/Flux_SSGI_DenoiseV.slang](../Shaders/SSGI/Flux_SSGI_DenoiseV.slang) | Separable joint bilateral — vertical half (final output) |

## Implementation Details

### Render Targets

| Target | Resolution | Format | Purpose |
|--------|------------|--------|---------|
| `s_xRawResult` | Divisor-based (quarter-res by default) | RGBA16F | RGB = indirect color, A = confidence |
| `s_xResolved` | Full | RGBA16F | Upsampled result |
| `s_xDenoiseH` | Full | RGBA16F | Horizontal-blurred intermediate (between H and V) |
| `s_xDenoised` | Full | RGBA16F | Final denoised output (V pass result) |

### Pass 1: Ray Marching

**Inputs:**
- G-Buffer normals (`MRT_INDEX_NORMALSAMBIENT`)
- G-Buffer material (`MRT_INDEX_MATERIAL`) - roughness in R channel
- G-Buffer diffuse (`MRT_INDEX_DIFFUSE`) - albedo at hit positions
- Depth buffer
- Hi-Z buffer from `Flux_HiZ::GetHiZSRV()`
- Blue noise texture (temporal variation)

**Algorithm:**
1. For each pixel, shoot N rays into cosine-weighted hemisphere
2. Use HiZ-accelerated ray marching (O(log N) traversal)
3. Sample albedo at hit positions
4. Accumulate with confidence weighting
5. Output average color and confidence

**Output:** `s_xRawResult` (divisor-based resolution; quarter-res by default; RGBA16F)

### Pass 2: Bilateral Upsample

**Purpose:** Edge-aware upsample from half to full resolution

**Algorithm:**
- 2x2 bilinear sampling of half-res result
- Depth-weighted bilateral filter
- Preserves edges at depth discontinuities

**Output:** `s_xResolved` (full-resolution RGBA16F)

### Pass 3 / 4: Separable Joint Bilateral Denoise (H then V)

**Purpose:** Reduce noise while preserving geometric and material edges. Split
into two 1D sub-passes for O(2r) total cost instead of O(r²). Each sub-pass
shares the exact same `DenoisePushConstantsLayout` constant buffer; only the
inner-loop direction (`xOffset = (i, 0)` vs `(0, i)`) differs.

**Algorithm:**
The joint bilateral filter computes weights based on four factors:

```
Weight(p,q) = G_spatial * G_depth * G_normal * G_albedo
```

Where:
- **G_spatial**: Gaussian falloff with distance (pixels)
- **G_depth**: Relative depth difference (% of local depth)
- **G_normal**: Surface orientation difference (1 - dot product)
- **G_albedo**: Material color difference (Euclidean RGB distance)

**Physical Motivation:**
- Same depth -> same surface -> should have similar GI
- Same normal -> same orientation -> similar lighting response
- Same albedo -> same material -> similar GI interaction

**Output:** `s_xDenoised` (full-resolution RGBA16F)

### Deferred Shading Integration

SSGI **replaces** (not adds to) IBL diffuse where confidence is high:

```glsl
// In Flux_DeferredShading.frag
if (g_bSSGIEnabled != 0)
{
    vec4 xSSGI = texture(g_xSSGITex, xUV);
    float fSSGIConfidence = xSSGI.a;

    // SSGI provides indirect radiance, multiply by kD and albedo
    vec3 xSSGIDiffuse = kD * xSSGI.rgb * xAlbedo;

    // Blend: high confidence = SSGI, low confidence = IBL fallback
    xFinalDiffuse = mix(xDiffuseIBL, xSSGIDiffuse, fSSGIConfidence);
}
```

## Public Interface

`Flux_SSGIImpl` derives from the CRTP base `Flux_ScreenSpaceEffectBase<Flux_SSGIImpl>` and is accessed as a singleton instance via `g_xEngine.SSGI()` (returning `Flux_SSGIImpl&`) — the methods are instance methods, not statics on a `Flux_SSGI` class.

```cpp
class Flux_SSGIImpl : public Flux_ScreenSpaceEffectBase<Flux_SSGIImpl>
{
public:
    void Initialise();
    void BuildPipelines();
    void ShutdownImpl();                                  // CRTP hook (SSGI owns no CBV → no-op)

    void SetupRenderGraph(Flux_RenderGraph& xGraph);      // declares the four SSGI passes
    void ApplyDenoiseSelectionToGraph(Flux_RenderGraph& xGraph);

    u_int ComputeEffectiveBinarySearchIterations() const; // resolution-scaled hit refinement
    void UpdateSSGIConstants();

    // For deferred shading to sample
    Flux_ShaderResourceView& GetSSGISRV();
    bool IsEnabled() const;                               // gated on Zenith_GraphicsOptions::m_bSSGIEnabled

    // Per-pass transient attachments
    Flux_RenderAttachment& GetRawResultAttachment();
    Flux_RenderAttachment& GetResolvedAttachment();
    Flux_RenderAttachment& GetDenoisedAttachment();
};
```

Enable/disable is **not** a debug variable: `IsEnabled()` returns `Zenith_GraphicsOptions::Get().m_bSSGIEnabled && m_bInitialised`. The denoise toggle is likewise owned by Graphics Options (`m_bSSGIDenoiseEnabled`).

The committed-handle selector (`Flux_CommittedHandleSelector<Flux_SSGISelection>`, where `Flux_SSGISelection = {bool m_bDenoise, u_int m_uDivisor}`) tracks which transient the deferred pass reads (Denoised when denoise is on, Resolved otherwise) and triggers a graph rebuild when either the denoise toggle or the raymarch resolution divisor diverges from the committed selection.

## Debug Variables

### Ray Marching

SSGI's enable/disable toggle is **not** a debug variable — it lives in Graphics Options (`Zenith_GraphicsOptions::m_bSSGIEnabled`); see `IsEnabled()`.

| Path | Type | Range | Default | Description |
|------|------|-------|---------|-------------|
| `Flux/SSGI/DebugMode` | uint | 0-4 | 0 | Visualization mode |
| `Flux/SSGI/Intensity` | float | 0-2 | 1.0 | GI intensity multiplier |
| `Flux/SSGI/MaxDistance` | float | 1-100 | 30.0 | Max ray distance (world units) |
| `Flux/SSGI/Thickness` | float | 0.01-2 | 0.5 | Surface thickness for hits |
| `Flux/SSGI/StepCount` | uint | 8-128 | **24** | HiZ traversal iterations (most rays hit early or exit screen) |
| `Flux/SSGI/StartMip` | uint | 0-10 | 4 | Starting HiZ mip level |
| `Flux/SSGI/RaysPerPixel` | uint | 1-8 | **2** | Hemisphere samples per pixel (lowered with separable denoise) |
| `Flux/SSGI/RoughnessThreshold` | float | 0-1 | **0.05** | Below this: skip SSGI (SSR handles smoother surfaces) |
| `Flux/SSGI/BinarySearchIterations` | uint | 1-6 | **2** | Hit-refinement iterations (1080p base; auto-bumped to 3 at 1440p, 4 at 4K, ceiling 6) |
| `Flux/SSGI/ResolutionDivisor` | uint | 2-8 | **4** | Raymarch render target = full / divisor. 2=half, 4=quarter (default), 8=eighth. Triggers a graph rebuild on change. |

A fixed upper roughness gate of `0.95` lives in the shader (named constant
`SSGI_ROUGH_GATE`) — above this, SSGI bails since IBL diffuse dominates. The
multi-ray loop also early-exits when average per-ray confidence exceeds `0.95`
(`SSGI_EARLY_EXIT_CONFIDENCE`).

### Denoising

`KernelRadius` is **per-axis**: the H pass blurs a `(2r+1)`-tap line, then V
does the same orthogonally. Total separable footprint is `(2r+1)+(2r+1)` taps
vs `(2r+1)²` for the old non-separable kernel.

The denoise enable toggle is owned by Graphics Options (`Zenith_GraphicsOptions::m_bSSGIDenoiseEnabled`), not registered as a debug variable; it is read into the denoise push constants each bind.

| Path | Type | Range | Default | Description |
|------|------|-------|---------|-------------|
| `Flux/SSGI/Denoise/KernelRadius` | uint | 1-8 | **3** | Per-axis filter radius (pixels) |
| `Flux/SSGI/Denoise/SpatialSigma` | float | 0.5-4 | **1.5** | Spatial Gaussian sigma (tighter for the smaller separable kernel) |
| `Flux/SSGI/Denoise/DepthSigma` | float | 0.01-0.1 | 0.02 | Depth threshold (% of depth) |
| `Flux/SSGI/Denoise/NormalSigma` | float | 0.1-1 | 0.5 | Normal threshold |
| `Flux/SSGI/Denoise/AlbedoSigma` | float | 0.05-0.5 | 0.1 | Albedo threshold |

### Debug Textures

| Path | Description |
|------|-------------|
| `Flux/SSGI/Textures/Raw` | Half-res ray march result |
| `Flux/SSGI/Textures/Resolved` | Upsampled (pre-denoise) |
| `Flux/SSGI/Textures/Denoised` | Final denoised output |

## Debug Visualization Modes

| Mode | Value | Output |
|------|-------|--------|
| `SSGI_DEBUG_NONE` | 0 | Normal rendering |
| `SSGI_DEBUG_RAY_DIRECTIONS` | 1 | RGB = hemisphere sample directions |
| `SSGI_DEBUG_HIT_POSITIONS` | 2 | RGB = hit world position / 100 |
| `SSGI_DEBUG_CONFIDENCE` | 3 | Grayscale confidence mask |
| `SSGI_DEBUG_FINAL_RESULT` | 4 | Final GI with confidence |

## Confidence Calculation

The alpha channel encodes hit quality based on multiple factors:

```glsl
fConfidence = fEdgeFade          // Screen border fade
            * fDistanceFade      // Ray travel distance
            * fDepthConfidence   // Hit accuracy vs thickness
            * fBackfaceConfidence // Surface orientation
            * fHorizonFade;      // Horizon occlusion
```

Minimum confidence is clamped to 0.02 to prevent black holes.

## Tuning Guide

### Noise vs Performance

| Setting | Low Quality | Balanced (default) | High Quality |
|---------|------------|--------------------|--------------|
| RaysPerPixel | 1 | 2 | 4-8 |
| KernelRadius (per-axis) | 1-2 | 3 | 5-8 |
| StepCount | 16 | 24 | 48-64 |
| BinarySearchIterations | 1 | 2 | 3-4 |

### Edge Preservation

- **DepthSigma**: Lower = sharper depth edges, more noise on flat surfaces
- **NormalSigma**: Lower = preserves creases better, may leave noise in corners
- **AlbedoSigma**: Lower = preserves material boundaries, may leave noise

**Recommended starting values:**
- DepthSigma: 0.02 (2% of local depth)
- NormalSigma: 0.5 (corresponds to ~60 degree angle difference)
- AlbedoSigma: 0.1 (10% color difference)

## Performance Characteristics

| Pass | Resolution | Approximate Cost (1080p) | Notes |
|------|------------|--------------------------|-------|
| Ray March | Quarter (default) | ~0.3ms | Scales linearly with pixel count: half-res ≈ 4× cost, full-res ≈ 16×. Early-exits at ≥0.95 average confidence. Step count default lowered 32→24. |
| Upsample | Full | ~0.2ms | Bilateral 2×2 from quarter-res — wider reconstruction footprint than from half-res, but bilateral edge weighting keeps silhouettes sharp |
| Denoise H | Full | ~0.1ms | Linear in KernelRadius (was O(r²) before separable split) |
| Denoise V | Full | ~0.1ms | Linear in KernelRadius |
| **Total (defaults)** | — | **~0.7ms** | Down from ~2.0ms baseline |

GPU cost scales roughly linearly with the number of raymarch pixels — at 4K
quarter-res = 1920×1080, at 1080p quarter-res = 480×270. If quarter-res is too
blocky on low-resolution displays, set `Flux/SSGI/ResolutionDivisor` to 2.

CPU-side per-pass timing is wrapped under `ZENITH_PROFILE_INDEX__FLUX_SSGI` —
see the ImGui profiling overlay for live numbers. GPU-side timing requires an
external tool (RenderDoc) until the engine grows GPU timestamp queries.

## Known Limitations

1. **Screen-space only**: Cannot capture GI from off-screen geometry
2. **Single bounce**: Only captures one indirect bounce
3. **Thin geometry**: May miss very thin objects
4. **Temporal coherence**: No temporal accumulation (may exhibit shimmer)
5. **Large kernels**: Denoise kernel > 8 pixels becomes expensive (linear after the separable split, but still scales). At very large radii, the loss of strict bilateral non-separability can cause subtle softening at corners.

## Dependencies

- **Flux_HiZ**: Must be enabled for SSGI to function
- **G-Buffer**: Requires normals, material, depth, diffuse
- **Deferred Shading**: Consumes SSGI result in lighting pass

**Note:** The denoise pass has no special dependencies beyond G-buffer access - it's a purely deterministic filter.
