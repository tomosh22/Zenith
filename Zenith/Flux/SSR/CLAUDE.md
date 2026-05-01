# Flux_SSR - Screen Space Reflections System

## Overview

The SSR (Screen Space Reflections) system provides real-time reflections by ray marching through the Hi-Z depth buffer. It integrates seamlessly with the deferred shading pipeline, blending with IBL (Image-Based Lighting) reflections where SSR lacks coverage.

## Architecture

### Three-pass pipeline (mirrors SSGI)

```
RENDER_ORDER_HIZ_GENERATE      <- Depth pyramid (required dependency)
RENDER_ORDER_SSR_RAYMARCH      <- Ray marching pass (HALF-res)
RENDER_ORDER_SSR_UPSAMPLE      <- Bilateral upsample to full-res
RENDER_ORDER_SSR_RESOLVE       <- Roughness-based blur (optional)
RENDER_ORDER_APPLY_LIGHTING    <- Deferred shading (consumes SSR)
```

The ray-march pass runs at **half resolution** for ~75% pixel-shader cost reduction; the upsample pass reconstructs the full-resolution output via depth-weighted bilateral 2x2 sampling. The resolve pass remains optional (gated on `m_bSSRRoughnessBlurEnabled`); when disabled the deferred shader reads the upsampled output directly — never the raw half-res raymarch.

### Files

| File | Purpose |
|------|---------|
| [Flux_SSR.h](Flux_SSR.h) | Class declaration, debug mode enum |
| [Flux_SSR.cpp](Flux_SSR.cpp) | Implementation, pipeline setup |
| [../Shaders/SSR/Flux_SSR_RayMarch.slang](../Shaders/SSR/Flux_SSR_RayMarch.slang) | Half-res HiZ ray marching |
| [../Shaders/SSR/Flux_SSR_Upsample.slang](../Shaders/SSR/Flux_SSR_Upsample.slang) | Depth-weighted bilateral 2x2 upsample (half→full) |
| [../Shaders/SSR/Flux_SSR_Resolve.slang](../Shaders/SSR/Flux_SSR_Resolve.slang) | Roughness blur (optional) |

### Constants buffer

`SSRConstants` is a CBV (`Flux_DynamicConstantBuffer`), not push constants — required because the cached HiZ mip-size array (`u_axHiZMipSizes[12]`) exceeds the 128-byte push limit, and to honour the project convention of passing texture dimensions through the constant buffer rather than calling `GetDimensions` inside shaders. Uploaded once per frame in the RayMarch pass; bound by all three SSR passes.

The CBV holds (in addition to tuning fields):
- Half-res output dimensions (`u_fHalfResWidth/Height`, `u_fRcpHalfResWidth/Height`) — read by the upsample shader to find the half-res neighbourhood without `GetDimensions(g_xSSRTex)`.
- Cached HiZ per-mip dimensions (`u_axHiZMipSizes[12].xyzw` = w, h, 1/w, 1/h) — read by the ray-march loop to pick the right texel size at each mip level without `GetDimensions(g_xHiZTex, mip, ...)` in the inner loop.

## Implementation Details

### Render Targets

| Target | Resolution | Format | Purpose |
|--------|------------|--------|---------|
| `s_xRayMarch` | Half | RGBA16F | RGB = reflected color, A = hit confidence |
| `s_xUpsampled` | Full | RGBA16F | Bilateral upsample to full-res (always-on) |
| `s_xResolved` | Full | RGBA16F | Blurred reflection for rough surfaces (optional) |

### Pass 1: Ray Marching (half-res)

**Inputs:**
- G-Buffer normals (`MRT_INDEX_NORMALSAMBIENT`)
- G-Buffer material (`MRT_INDEX_MATERIAL`) - roughness
- G-Buffer diffuse (`MRT_INDEX_DIFFUSE`) - color source (when `UseGBufferColor` enabled)
- Depth buffer
- Hi-Z buffer from `Flux_HiZ::GetHiZSRV()`

**Algorithm:**
1. Reconstruct world position from depth
2. Compute reflection vector: `R = reflect(V, N)`
3. Transform ray to screen space
4. Linear ray march with configurable step count
5. Sample color at hit position
6. Apply edge fade for screen borders
7. Early-out for roughness > `MaxRoughness`

**Output:** `s_xRayMarchResult` (RGBA16F)

### Pass 2: Bilateral Upsample (half→full)

**Purpose:** Reconstruct full-res reflections from the half-res ray-march output, edge-aware via depth weights.

**Algorithm:**
- 2x2 bilinear sample around the full-res pixel's half-res footprint
- Depth-weighted bilateral filter (depth-relative sigma — 2% of local depth)
- Sky pixels short-circuit to zero
- Half-res dimensions read from the SSR CBV (no in-shader `GetDimensions`)

**Output:** `s_xUpsampled` (full-resolution RGBA16F). This is the "no roughness blur" final output — deferred reads it directly when `m_bSSRRoughnessBlurEnabled` is off.

### Pass 3: Resolve/Blur (optional, full-res)

**Purpose:** Apply roughness-based bilateral blur for rough surfaces. Reads the upsampled output (not the raw raymarch).

**Algorithm:**
- Roughness < 0.15: pass through (mirror-like — already full-res from upsample)
- Roughness > 0.7: pass through (very rough — IBL handles it)
- 0.15 ≤ roughness ≤ 0.7: bilateral blur weighted by spatial Gaussian, normal similarity, depth, and hit confidence
- Kernel up to 7x7 base, ~21x21 with depth-scale (`SSR_RESOLVE_MAX_KERNEL_SIZE = 3`, `SSR_RESOLVE_DEPTH_SCALE_MAX = 1.5`)

**Output:** `s_xResolved` (RGBA16F)

### Deferred Shading Integration

SSR is blended with IBL specular in `ComputeIBLAmbient()`. **Fresnel is applied here, not in SSR** (per industry standard):

```glsl
// In Flux_DeferredShading.frag
if (g_bSSREnabled != 0)
{
    vec4 xSSR = texture(g_xSSRTex, xUV);
    float fSSRConfidence = xSSR.a;

    // Apply Fresnel weighting - produces stronger reflections at grazing angles
    // This is where the physics-correct Fresnel effect is applied
    vec3 xSSRFresnel = FresnelSchlickRoughness(NdotV, F0, fRoughness);
    vec3 xSSRSpecular = xSSR.rgb * xSSRFresnel;

    // Blend: high confidence = SSR, low confidence = IBL fallback
    xFinalSpecular = mix(xSpecularIBL, xSSRSpecular, fSSRConfidence);
}
```

**Why Fresnel is NOT applied in SSR:** The SSR shader outputs raw reflected color. Applying Fresnel in the deferred shader ensures:
1. Same Fresnel applied to both SSR and IBL (seamless blending)
2. Energy conservation maintained
3. Matches industry standard (FidelityFX SSSR, UE4)

## Public Interface

```cpp
class Flux_SSR
{
public:
    static void Initialise();
    static void Shutdown();
    static void Reset();

    static void Render(void*);
    static void SubmitRenderTask();
    static void WaitForRenderTask();

    // For deferred shading to sample
    static Flux_ShaderResourceView& GetReflectionSRV();
    static bool IsEnabled();
};
```

## Debug Variables

| Path | Type | Range | Default | Description |
|------|------|-------|---------|-------------|
| `Flux/SSR/Enable` | bool | - | true | Enable/disable SSR |
| `Flux/SSR/RoughnessBlur` | bool | - | true | Enable resolve pass blur (graphics options) |
| `Flux/SSR/DebugMode` | uint | 0-100 | 0 | Visualization mode (99 = solid magenta proof-of-life) |
| `Flux/SSR/Intensity` | float | 0-2 | 1.0 | Reflection intensity multiplier |
| `Flux/SSR/MaxDistance` | float | 1-100 | 50.0 | Max ray march distance (world units) |
| `Flux/SSR/MaxRoughness` | float | 0-1 | 1.0 | Skip SSR above this roughness |
| `Flux/SSR/StepCount` | uint | 8-256 | **32** | Ray march iterations (lowered from 64; HiZ acceleration converges quickly) |
| `Flux/SSR/StartMip` | uint | 0-10 | 5 | Starting HiZ mip level for hierarchical traversal |
| `Flux/SSR/Thickness` | float | 0.01-1 | 0.5 | Surface thickness for hit detection (m) |
| `Flux/SSR/ContactHardeningDist` | float | 0.5-10 | 2.0 | World-space distance over which contact-hardening confidence ramps in (m) |
| `Flux/SSR/Textures/RayMarch` | texture | - | - | Raw half-res ray-march result |
| `Flux/SSR/Textures/Upsampled` | texture | - | - | Bilateral upsample (full-res, pre-blur) |
| `Flux/SSR/Textures/Resolved` | texture | - | - | Blurred result (full-res, when RoughnessBlur is on) |

## Debug Visualization Modes

| Mode | Value | Output |
|------|-------|--------|
| `SSR_DEBUG_NONE` | 0 | Normal rendering |
| `SSR_DEBUG_RAY_DIRECTIONS` | 1 | RGB = reflection direction |
| `SSR_DEBUG_HIT_POSITIONS` | 2 | RGB = hit world position / 100 |
| `SSR_DEBUG_REFLECTION_UVS` | 3 | RG = hit screen UV |
| `SSR_DEBUG_CONFIDENCE_MASK` | 4 | Grayscale confidence |
| `SSR_DEBUG_HIZ_TRAVERSAL` | 5 | Heatmap of iterations used |
| `SSR_DEBUG_ROUGHNESS_BLUR` | 6 | Roughness heatmap |
| `SSR_DEBUG_EDGE_FADE` | 7 | Screen edge fadeout |
| `SSR_DEBUG_FINAL_RESULT` | 8 | Final resolved reflection |

## Edge Fade

SSR applies smooth fadeout at screen edges to avoid hard cutoffs:

```glsl
// Fade at screen edges (20% border)
vec2 xEdgeFade = smoothstep(0.0, 0.2, xHitUV) * smoothstep(1.0, 0.8, xHitUV);
float fEdgeFade = xEdgeFade.x * xEdgeFade.y;
```

## Confidence Calculation

The alpha channel encodes hit confidence based on **geometric factors only** (per industry standard - FidelityFX SSSR, UE4):
- Edge fade factor (screen border)
- Distance fade (ray travel distance)
- Depth confidence (hit accuracy)
- Backface confidence (surface orientation)
- Stretch confidence (ray/surface angle - prevents stretching artifacts)

**Important:** SSR confidence is NOT based on viewing angle (NdotV). The deferred shader applies `FresnelSchlickRoughness()` which naturally produces stronger reflections at grazing angles. Fading SSR confidence at grazing angles would fight against this physics.

```glsl
fConfidence = fEdgeFade * fDistanceFade * fDepthConfidence * fBackfaceConfidence * fStretchConfidence;
```

## Performance Considerations

- **Half-res ray march:** Dominant cost saving — ~75% pixel-shader reduction vs the historical full-res implementation. All quality tuning happens against the half-res baseline.
- **Step Count (default 32):** Higher = better quality, more expensive. With HiZ acceleration plus binary-search refinement, 32 is sufficient for most scenes. Match SSGI's default.
- **Binary search iterations:** Auto-scales 4 (≤1080p) / 5 (>1080p). Half-res origins make sub-1/16-pixel precision wasted.
- **MaxRoughness:** Skip very rough surfaces (use IBL only).
- **RoughnessBlur:** Adds ~10-15% cost for resolve pass at default kernel size 3 (capped at ~21x21 with depth-scale).
- **HiZ mip dimensions:** Cached in the SSR CBV (`u_axHiZMipSizes[12]`) so the ray-march inner loop never calls `GetDimensions` — eliminates per-iteration driver overhead.

## Known Limitations

1. **Off-screen reflections:** Cannot reflect objects outside screen bounds
2. **Self-occlusion:** Objects may incorrectly reflect themselves
3. **Thin objects:** May miss very thin geometry
4. **Glossy surfaces:** Limited blur kernel size affects very rough surfaces

## Dependencies

- **Flux_HiZ:** Must be enabled for SSR to function
- **G-Buffer:** Requires normals, material, depth, diffuse
- **Deferred Shading:** Consumes SSR result in lighting pass
