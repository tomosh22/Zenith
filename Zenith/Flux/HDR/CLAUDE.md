# Flux HDR Pipeline

## Overview

High Dynamic Range rendering pipeline that enables proper handling of bright light sources, bloom effects, and tone mapping for photorealistic output. The system renders the scene to a 16-bit floating-point HDR buffer, applies bloom, and then tone maps to the final LDR output. The default tone-mapping operator is `TONEMAPPING_AGX` (modern filmic); all operators are implemented in `Flux_ToneMapping.slang`.

> **Bloom is AFTER TAA.** The bloom threshold and the tonemap read `Flux_GraphicsImpl::GetSceneColourForPostFX()`,
> which returns the **TAA-resolved** scene colour when TAA is on (the default), else the raw HDR scene target.
> So the whole bloom + tonemap chain operates on the temporally-resolved (and, under upscaling, output-res)
> image. The **auto-exposure histogram** still reads the raw `GetHDRSceneTarget` (render res under upscaling —
> its CB/dispatch dims come from `GetRenderDims()`), because exposure should meter the scene, not the AA'd result.
> The inline **FXAA stopgap** that used to live in `Flux_ToneMapping.slang` was **removed** once TAA became the
> shipping AA (see `Flux/TAA/CLAUDE.md`).

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
| `Flux_HDRImpl.h` | Class declaration (`Flux_HDRImpl`), enums (ToneMappingOperator, HDR_DebugMode) |
| `Flux_HDR.cpp` | Implementation - bloom, tone mapping, render targets |
| `Flux_HDR_Shaders.h` | Shader-program decls owned by the HDR feature (`Flux_HDRShaders::apxALL`) |

## Shaders

| Shader | Location | Purpose |
|--------|----------|---------|
| `Flux_ToneMapping.slang` | `Shaders/HDR/` | Final tone mapping with AGX/ACES/ACES_FITTED/Reinhard/Uncharted2/Neutral operators |
| `Flux_BloomThreshold.slang` | `Shaders/HDR/` | Extracts bright areas using soft threshold |
| `Flux_BloomDownsample.slang` | `Shaders/HDR/` | 13-tap downsample filter for blur |
| `Flux_BloomUpsample.slang` | `Shaders/HDR/` | 9-tap tent filter upsample with blur |
| `Flux_Luminance.slang` | `Shaders/HDR/` | Compute pass building the luminance histogram |
| `Flux_Adaptation.slang` | `Shaders/HDR/` | Compute pass deriving the adapted exposure from the histogram |

## Render Targets

HDR owns only its **private bloom chain** (a graph transient created in
`Flux_HDR::SetupRenderGraph`). The **HDR scene target** is a *shared* render target
owned by `Flux_Graphics` — created up front in `Flux_GraphicsImpl::SetupRenderGraph`
(the first feature), before any feature writes it. HDR reads / tonemaps it via
`g_xEngine.FluxGraphics().GetHDRSceneTarget()`.

| Target | Owner | Format | Purpose |
|--------|-------|--------|---------|
| HDR scene target (`m_xHDRSceneTargetHandle`) | `Flux_Graphics` | `RGBA16F` | Main HDR scene accumulation (shared; many features write it) |
| Bloom chain (`m_axBloomChainHandles[5]`) | `Flux_HDR` | `RGBA16F` | Bloom downsample/upsample chain (HDR-private) |

## Target Setups

The HDR scene-target setup helpers live on `Flux_Graphics` (it owns the target):

| Setup | Use Case |
|-------|----------|
| `g_xEngine.FluxGraphics().GetHDRSceneTargetSetup()` | Color-only (deferred shading, fog) |
| `g_xEngine.FluxGraphics().GetHDRSceneTargetSetupWithDepth()` | With depth (particles, SDFs) |

## Pass placement

HDR registers these passes with the render graph; ordering is derived from Read/Write declarations, not from any enum.

| Pass | Status | Reads | Writes |
|------|--------|-------|--------|
| Luminance histogram (`HDR_LuminanceHistogram`, compute) | active | HDR scene | histogram buffer |
| Exposure adaptation (`HDR_Adaptation`, compute) | active | histogram buffer | exposure buffer |
| Bloom (downsample + blur + composite) | active | HDR scene | bloom mip chain |
| Tonemap (HDR → LDR) | active | HDR scene, bloom output, exposure | swapchain LDR target |

The tonemap pass naturally runs after anything that writes the HDR scene (deferred shading, SSAO, fog, particles) and before anything that reads the LDR target (UI text, UI quads, ImGui).

## Tone Mapping Operators

```cpp
TONEMAPPING_ACES           // Academy Color Encoding System
TONEMAPPING_ACES_FITTED    // Faster ACES approximation
TONEMAPPING_REINHARD       // Simple Reinhard curve
TONEMAPPING_UNCHARTED2     // Uncharted 2 filmic curve
TONEMAPPING_NEUTRAL        // Neutral/minimal curve
TONEMAPPING_AGX            // Modern filmic curve (default)
```

## Debug Variables (via Zenith_DebugVariables)

HDR/auto-exposure enable (and bloom enable) live on `Zenith_GraphicsOptions`
(`Graphics/...` path), **not** these debug variables. The variables below are only
what `RegisterDebugVariables()` actually registers (tools builds).

| Path | Type | Description |
|------|------|-------------|
| `Flux/HDR/DebugMode` | uint | Debug visualization mode |
| `Flux/HDR/Exposure` | float | Manual exposure (0.01-10.0) |
| `Flux/HDR/BloomIntensity` | float | Bloom intensity (0.0-2.0) |
| `Flux/HDR/BloomThreshold` | float | Bloom threshold (0.0-5.0) |
| `Flux/HDR/ToneMappingOperator` | uint | Select tone mapping curve |
| `Flux/HDR/ShowHistogram` | bool | Overlay the luminance histogram |
| `Flux/HDR/FreezeExposure` | bool | Freeze auto-exposure adaptation |
| `Flux/HDR/AdaptationSpeed` | float | Eye-adaptation speed (0.1-10.0) |
| `Flux/HDR/TargetLuminance` | float | Auto-exposure target luminance (0.01-1.0) |
| `Flux/HDR/MinExposure` | float | Auto-exposure lower clamp (0.01-1.0) |
| `Flux/HDR/MaxExposure` | float | Auto-exposure upper clamp (1.0-20.0) |

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
HDR_DEBUG_TONEMAP_PASS_TEST // Tone-map pass validation
HDR_DEBUG_RAW_HDR_TEXTURE  // Raw HDR texture display
```

## Integration Points

Systems that render to HDR target:
- `Flux_DeferredShading` - Main lighting pass (SSAO's blurred output is read here and folded into the ambient term; SSAO does not write the HDR target directly)
- `Flux_Fog` (all variants) - Atmospheric fog
- `Flux_Particles` - Particle effects (with depth)
- `Flux_SDFs` - Signed distance fields (with depth)

Systems that render after tone mapping (to final target):
- `Flux_Text` - UI text
- `Flux_Quads` - UI elements
- ImGui - Editor interface

## Initialization Order

The shared HDR scene target is created by `Flux_Graphics` (the first-registered
feature) in its `SetupRenderGraph` — before any feature declares a pass on it. The
ordering is feature-registry-driven (see `Flux_FeatureRegistry.cpp`), not a manual
call sequence. `Flux_HDR::Initialise` creates only HDR's histogram/exposure buffers;
its private bloom chain is created in `Flux_HDR::SetupRenderGraph`.

## Common Operations

### Render to HDR (no depth):
HDR targets are declared as render-graph pass writes; the pass's record callback
records directly into the supplied `Flux_CommandBuffer*`. Target setup comes from
`g_xEngine.FluxGraphics().GetHDRSceneTargetSetup()` (or `...WithDepth()`), wired via
the pass's `Writes(...)` declarations in `SetupRenderGraph` — there is no manual
command-list submission.

### Access HDR texture for sampling:
```cpp
Flux_ShaderResourceView& srv = g_xEngine.FluxGraphics().GetHDRSceneSRV();
```

## Performance Notes

- Bloom uses 5-level mip chain for wide blur radius
- 13-tap downsample filter provides high quality blur
- Target GPU time: ~1.5ms for bloom + ~0.5ms for tone mapping (1080p)
- VRAM usage: ~55MB for HDR target + bloom chain

## Auto-Exposure / Luminance

Auto-exposure runs as two active compute passes (see Pass placement). `Flux_Luminance.slang`
builds a 256-bin luminance histogram from the HDR scene into `m_xHistogramBuffer`;
`Flux_Adaptation.slang` derives the adapted exposure (target luminance, min/max clamp,
adaptation speed) into `m_xExposureBuffer`, which the tonemap pass consumes. Whether
auto-exposure is engaged is owned by `Zenith_GraphicsOptions`; the per-frame tuning lives
in the `Flux/HDR/*` debug variables (TargetLuminance, AdaptationSpeed, Min/MaxExposure,
FreezeExposure, ShowHistogram).

## Future Work

- Volumetric light scattering integration
