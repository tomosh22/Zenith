# Flux_SSR - Screen Space Reflections System

## Overview

The SSR (Screen Space Reflections) system provides real-time reflections by ray marching through the Hi-Z depth buffer. It integrates seamlessly with the deferred shading pipeline, blending with IBL (Image-Based Lighting) reflections where SSR lacks coverage.

## Architecture

### Render Order

```
RENDER_ORDER_HIZ_GENERATE      <- Depth pyramid (required dependency)
RENDER_ORDER_SSR_RAYMARCH      <- Ray marching pass
RENDER_ORDER_SSR_RESOLVE       <- Roughness-based blur
RENDER_ORDER_APPLY_LIGHTING    <- Deferred shading (consumes SSR)
```

SSR runs after Hi-Z generation but before deferred lighting, allowing reflections to be incorporated into BRDF evaluation.

### Files

| File | Purpose |
|------|---------|
| [Flux_SSR.h](Flux_SSR.h) | Class declaration, debug mode enum |
| [Flux_SSR.cpp](Flux_SSR.cpp) | Implementation, pipeline setup |
| [../Shaders/SSR/Flux_SSR_RayMarch.frag](../Shaders/SSR/Flux_SSR_RayMarch.frag) | Ray marching fragment shader |
| [../Shaders/SSR/Flux_SSR_Resolve.frag](../Shaders/SSR/Flux_SSR_Resolve.frag) | Roughness blur fragment shader |

## Implementation Details

### Render Targets

| Target | Format | Purpose |
|--------|--------|---------|
| `s_xRayMarchResult` | RGBA16F | RGB = reflected color, A = hit confidence |
| `s_xResolvedReflection` | RGBA16F | Blurred reflection for rough surfaces |

### Pass 1: Ray Marching

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

### Pass 2: Resolve/Blur

**Purpose:** Apply roughness-based bilateral blur for rough surfaces

**Algorithm:**
- Roughness < 0.1: pass through (mirror-like)
- Roughness >= 0.1: 9-tap bilateral blur weighted by:
  - Gaussian spatial falloff
  - Normal similarity (edge preservation)
  - Hit confidence

**Output:** `s_xResolvedReflection` (RGBA16F)

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

| Path | Type | Range | Description |
|------|------|-------|-------------|
| `Flux/SSR/Enable` | bool | - | Enable/disable SSR |
| `Flux/SSR/UseGBufferColor` | bool | - | Sample G-Buffer diffuse vs last frame HDR |
| `Flux/SSR/RoughnessBlur` | bool | - | Enable resolve pass blur |
| `Flux/SSR/DebugMode` | uint | 0-8 | Visualization mode |
| `Flux/SSR/Intensity` | float | 0-2 | Reflection intensity multiplier |
| `Flux/SSR/MaxDistance` | float | 1-100 | Max ray march distance (world units) |
| `Flux/SSR/MaxRoughness` | float | 0-1 | Skip SSR above this roughness |
| `Flux/SSR/StepCount` | uint | 8-128 | Ray march iterations |
| `Flux/SSR/Thickness` | float | 0.01-1 | Surface thickness for hit detection |
| `Flux/SSR/Textures/RayMarch` | texture | - | Raw ray march result |
| `Flux/SSR/Textures/Resolved` | texture | - | Blurred result |

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

- **Step Count:** Higher = better quality, more expensive
- **MaxRoughness:** Skip very rough surfaces (use IBL only)
- **RoughnessBlur:** Adds ~25% cost for resolve pass
- **Resolution:** Can render at half-res for significant savings

## Known Limitations

1. **Off-screen reflections:** Cannot reflect objects outside screen bounds
2. **Self-occlusion:** Objects may incorrectly reflect themselves
3. **Thin objects:** May miss very thin geometry
4. **Glossy surfaces:** Limited blur kernel size affects very rough surfaces

## Dependencies

- **Flux_HiZ:** Must be enabled for SSR to function
- **G-Buffer:** Requires normals, material, depth, diffuse
- **Deferred Shading:** Consumes SSR result in lighting pass
