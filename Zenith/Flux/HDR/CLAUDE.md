# Flux HDR Pipeline

## Overview

High Dynamic Range rendering pipeline that enables proper handling of bright light sources, bloom effects, and tone mapping for photorealistic output. The system renders the scene to a 16-bit floating-point HDR buffer, applies bloom, and then tone maps to the final LDR output.

## Architecture

```
Scene Rendering (Deferred Shading, SSAO, Fog, Particles, SDFs)
                    |
                    v
    [HDR Scene Target - RGBA16F]
                    |
        +-----------+-----------+
        |                       |
        v                       v
   Bloom Threshold        Tone Mapping
        |                       ^
        v                       |
   Downsample Chain (5 mips)    |
        |                       |
        v                       |
   Upsample Chain (additive)----+
        |
        v
    [Final Render Target - LDR]
```

## Files

| File | Purpose |
|------|---------|
| `Flux_HDR.h` | Class declaration, enums (ToneMappingOperator, HDR_DebugMode) |
| `Flux_HDR.cpp` | Implementation - bloom, tone mapping, render targets |

## Shaders

| Shader | Location | Purpose |
|--------|----------|---------|
| `Flux_ToneMapping.frag` | `Shaders/HDR/` | Final tone mapping with ACES/Reinhard/Uncharted2/Neutral operators |
| `Flux_BloomThreshold.frag` | `Shaders/HDR/` | Extracts bright areas using soft threshold |
| `Flux_BloomDownsample.frag` | `Shaders/HDR/` | 13-tap downsample filter for blur |
| `Flux_BloomUpsample.frag` | `Shaders/HDR/` | 9-tap tent filter upsample with blur |

## Render Targets

| Target | Format | Purpose |
|--------|--------|---------|
| `s_xHDRSceneTarget` | `RGBA16F` | Main HDR scene accumulation |
| `s_axBloomChain[5]` | `RGBA16F` | Bloom downsample/upsample chain |

## Target Setups

| Setup | Use Case |
|-------|----------|
| `GetHDRSceneTargetSetup()` | Color-only (deferred shading, SSAO, fog) |
| `GetHDRSceneTargetSetupWithDepth()` | With depth (particles, SDFs) |

## Render Order

The HDR system uses these render orders defined in `Flux_Enums.h`:

```cpp
RENDER_ORDER_HDR_LUMINANCE,    // Luminance histogram (future)
RENDER_ORDER_HDR_ADAPTATION,   // Auto-exposure adaptation (future)
RENDER_ORDER_HDR_BLOOM,        // Bloom passes
RENDER_ORDER_HDR_TONEMAP,      // Final tone mapping to LDR
```

## Tone Mapping Operators

```cpp
TONEMAPPING_ACES           // Academy Color Encoding System (default)
TONEMAPPING_ACES_FITTED    // Faster ACES approximation
TONEMAPPING_REINHARD       // Simple Reinhard curve
TONEMAPPING_UNCHARTED2     // Uncharted 2 filmic curve
TONEMAPPING_NEUTRAL        // Neutral/minimal curve
```

## Debug Variables (via Zenith_DebugVariables)

| Path | Type | Description |
|------|------|-------------|
| `Flux/HDR/Enable` | bool | Enable/disable HDR |
| `Flux/HDR/DebugMode` | uint | Debug visualization mode |
| `Flux/HDR/Exposure` | float | Manual exposure (0.01-10.0) |
| `Flux/HDR/AutoExposure` | bool | Enable auto-exposure |
| `Flux/HDR/BloomEnabled` | bool | Enable bloom |
| `Flux/HDR/BloomIntensity` | float | Bloom intensity (0.0-2.0) |
| `Flux/HDR/BloomThreshold` | float | Bloom threshold (0.0-5.0) |
| `Flux/HDR/ToneMappingOperator` | uint | Select tone mapping curve |

## Debug Modes

```cpp
HDR_DEBUG_NONE             // Normal rendering
HDR_DEBUG_LUMINANCE_HEAT   // False-color luminance visualization
HDR_DEBUG_HISTOGRAM_OVERLAY // Histogram graph overlay
HDR_DEBUG_EXPOSURE_METER   // Show exposure info
HDR_DEBUG_BLOOM_ONLY       // Isolate bloom contribution
HDR_DEBUG_BLOOM_MIPS       // Grid of bloom mip levels
HDR_DEBUG_PRE_TONEMAP      // Raw HDR clamped to [0,1]
HDR_DEBUG_CLIPPING         // Highlight clipped pixels
HDR_DEBUG_EV_ZONES         // Zone system overlay
```

## Integration Points

Systems that render to HDR target:
- `Flux_DeferredShading` - Main lighting pass
- `Flux_SSAO` - Ambient occlusion
- `Flux_Fog` (all variants) - Atmospheric fog
- `Flux_Particles` - Particle effects (with depth)
- `Flux_SDFs` - Signed distance fields (with depth)

Systems that render after tone mapping (to final target):
- `Flux_Text` - UI text
- `Flux_Quads` - UI elements
- ImGui - Editor interface

## Initialization Order

HDR must be initialized BEFORE systems that render to the HDR target:

```cpp
Flux_Swapchain::Initialise();
Flux_Graphics::Initialise();
Flux_HDR::Initialise();        // Creates HDR targets
// ... then DeferredShading, SSAO, Fog, etc.
```

## Common Operations

### Render to HDR (no depth):
```cpp
Flux::SubmitCommandList(&cmdList, Flux_HDR::GetHDRSceneTargetSetup(), RENDER_ORDER_XYZ);
```

### Render to HDR (with depth):
```cpp
Flux::SubmitCommandList(&cmdList, Flux_HDR::GetHDRSceneTargetSetupWithDepth(), RENDER_ORDER_XYZ);
```

### Access HDR texture for sampling:
```cpp
Flux_ShaderResourceView& srv = Flux_HDR::GetHDRSceneSRV();
```

## Performance Notes

- Bloom uses 5-level mip chain for wide blur radius
- 13-tap downsample filter provides high quality blur
- Target GPU time: ~1.5ms for bloom + ~0.5ms for tone mapping (1080p)
- VRAM usage: ~55MB for HDR target + bloom chain

## Future Work

- Luminance histogram computation (compute shader)
- Auto-exposure based on histogram
- Eye adaptation with temporal smoothing
- Volumetric light scattering integration
