# Flux_SSGI - Screen Space Global Illumination System

## Overview

The SSGI (Screen Space Global Illumination) system provides real-time indirect diffuse lighting by ray marching through the Hi-Z depth buffer. Multiple rays per pixel sample the hemisphere above each surface, accumulating bounced light from nearby geometry.

The system uses a three-pass architecture:
1. **Ray March** (half-res) - Multi-ray hemisphere sampling with HiZ acceleration
2. **Upsample** - Bilateral upsample from half to full resolution
3. **Denoise** - Joint bilateral filter for edge-aware noise reduction

## Architecture

### Render Order

```
RENDER_ORDER_HIZ_GENERATE      <- Depth pyramid (required dependency)
RENDER_ORDER_SSGI_RAYMARCH     <- Multi-ray hemisphere sampling
RENDER_ORDER_SSGI_UPSAMPLE     <- Bilateral upsample to full-res
RENDER_ORDER_SSGI_DENOISE      <- Joint bilateral denoising
RENDER_ORDER_APPLY_LIGHTING    <- Deferred shading (consumes SSGI)
```

### Files

| File | Purpose |
|------|---------|
| [Flux_SSGI.h](Flux_SSGI.h) | Class declaration, debug mode enum |
| [Flux_SSGI.cpp](Flux_SSGI.cpp) | Implementation, pipeline setup, render passes |
| [../Shaders/SSGI/Flux_SSGI_RayMarch.frag](../Shaders/SSGI/Flux_SSGI_RayMarch.frag) | HiZ ray marching, hemisphere sampling |
| [../Shaders/SSGI/Flux_SSGI_Upsample.frag](../Shaders/SSGI/Flux_SSGI_Upsample.frag) | Bilateral upsample shader |
| [../Shaders/SSGI/Flux_SSGI_Denoise.frag](../Shaders/SSGI/Flux_SSGI_Denoise.frag) | Joint bilateral filter shader |

## Implementation Details

### Render Targets

| Target | Resolution | Format | Purpose |
|--------|------------|--------|---------|
| `s_xRawResult` | Half | RGBA16F | RGB = indirect color, A = confidence |
| `s_xResolved` | Full | RGBA16F | Upsampled result |
| `s_xDenoised` | Full | RGBA16F | Final denoised output |

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

**Output:** `s_xRawResult` (half-resolution RGBA16F)

### Pass 2: Bilateral Upsample

**Purpose:** Edge-aware upsample from half to full resolution

**Algorithm:**
- 2x2 bilinear sampling of half-res result
- Depth-weighted bilateral filter
- Preserves edges at depth discontinuities

**Output:** `s_xResolved` (full-resolution RGBA16F)

### Pass 3: Joint Bilateral Denoise

**Purpose:** Reduce noise while preserving geometric and material edges

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

```cpp
class Flux_SSGI
{
public:
    static void Initialise();
    static void Shutdown();
    static void Reset();

    static void Render(void*);
    static void SubmitRenderTask();
    static void WaitForRenderTask();

    // For deferred shading to sample
    static Flux_ShaderResourceView& GetSSGISRV();
    static bool IsEnabled();
    static bool IsInitialised();
};
```

## Debug Variables

### Ray Marching

| Path | Type | Range | Description |
|------|------|-------|-------------|
| `Flux/SSGI/Enable` | bool | - | Enable/disable SSGI |
| `Flux/SSGI/DebugMode` | uint | 0-4 | Visualization mode |
| `Flux/SSGI/Intensity` | float | 0-2 | GI intensity multiplier |
| `Flux/SSGI/MaxDistance` | float | 1-100 | Max ray distance (world units) |
| `Flux/SSGI/Thickness` | float | 0.01-2 | Surface thickness for hits |
| `Flux/SSGI/StepCount` | uint | 8-128 | HiZ traversal iterations |
| `Flux/SSGI/StartMip` | uint | 0-10 | Starting HiZ mip level |
| `Flux/SSGI/RaysPerPixel` | uint | 1-8 | Hemisphere samples per pixel |

### Denoising

| Path | Type | Range | Description |
|------|------|-------|-------------|
| `Flux/SSGI/Denoise/Enable` | bool | - | Enable denoise pass |
| `Flux/SSGI/Denoise/KernelRadius` | uint | 1-8 | Filter radius (pixels) |
| `Flux/SSGI/Denoise/SpatialSigma` | float | 0.5-4 | Spatial Gaussian sigma |
| `Flux/SSGI/Denoise/DepthSigma` | float | 0.01-0.1 | Depth threshold (% of depth) |
| `Flux/SSGI/Denoise/NormalSigma` | float | 0.1-1 | Normal threshold |
| `Flux/SSGI/Denoise/AlbedoSigma` | float | 0.05-0.5 | Albedo threshold |

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

| Setting | Low Quality | Balanced | High Quality |
|---------|------------|----------|--------------|
| RaysPerPixel | 1-2 | 3-4 | 6-8 |
| KernelRadius | 2 | 4 | 6 |
| StepCount | 16 | 32 | 64 |

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
| Ray March | Half | ~1.5ms | Scales with RaysPerPixel |
| Upsample | Full | ~0.2ms | Fixed cost |
| Denoise | Full | ~0.3ms | Scales with KernelRadius^2 |

## Known Limitations

1. **Screen-space only**: Cannot capture GI from off-screen geometry
2. **Single bounce**: Only captures one indirect bounce
3. **Thin geometry**: May miss very thin objects
4. **Temporal coherence**: No temporal accumulation (may exhibit shimmer)
5. **Large kernels**: Denoise kernel > 8 pixels becomes expensive

## Dependencies

- **Flux_HiZ**: Must be enabled for SSGI to function
- **G-Buffer**: Requires normals, material, depth, diffuse
- **Deferred Shading**: Consumes SSGI result in lighting pass

**Note:** The denoise pass has no special dependencies beyond G-buffer access - it's a purely deterministic filter.
