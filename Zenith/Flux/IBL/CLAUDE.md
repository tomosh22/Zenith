# Flux IBL (Image-Based Lighting) Pipeline

## Overview

Image-Based Lighting system for realistic ambient lighting and reflections. Uses a split-sum approximation with precomputed BRDF LUT for efficient real-time IBL. Provides diffuse irradiance from environment and roughness-based specular reflections.

## Architecture

```
[Environment Source]
    (Skybox/Atmosphere)
          |
    +-----+-----+
    |           |
    v           v
Irradiance    Prefiltered
Convolution   Environment
(32x32 cube)  (128x128 cube, 7 mips)
    |               |
    +-------+-------+
            |
            v
    [Deferred Shading]
            |
    +-------+-------+
    |               |
    v               v
Diffuse IBL    Specular IBL
(N-based)      (R-based + BRDF LUT)
```

## Files

| File | Purpose |
|------|---------|
| `Flux_IBLImpl.h` | `Flux_IBLImpl` class declaration, `IBL_RegenState` enum, `IBLConfig` constants |
| `Flux_IBL_Shaders.h` | Shader decls (`xIBL_BRDFIntegration`, `xIBL_IrradianceConvolution`, `xIBL_PrefilterEnvMap`) + `apxALL[]` array |
| `Flux_IBL.cpp` | Implementation - BRDF LUT generation, texture management |

## Shaders

| Shader | Location | Purpose |
|--------|----------|---------|
| `Flux_BRDFIntegration.slang` | `Shaders/IBL/` | Generates split-sum BRDF LUT (once at init) |
| `Flux_IrradianceConvolution.slang` | `Shaders/IBL/` | Computes cosine-weighted diffuse irradiance from environment |
| `Flux_PrefilterEnvMap.slang` | `Shaders/IBL/` | Generates GGX-prefiltered specular environment mip chain |

## IBL Textures

| Texture | Format | Size | Purpose |
|---------|--------|------|---------|
| BRDF LUT | RG16F | 512x512 | NdotV x Roughness → (scale, bias) |
| Irradiance Map | RGBA16F | 32x32 (cube) | Cosine-weighted hemisphere for diffuse |
| Prefiltered Map | RGBA16F | 128x128 (cube, 7 mips) | Roughness-based specular |

## Debug Variables (via Zenith_DebugVariables, tools-only)

Registered in `RegisterDebugVariables()` (guarded by `ZENITH_TOOLS`):

| Path | Type | Description |
|------|------|-------------|
| `Flux/IBL/ShowBRDFLUT` | bool | Display BRDF LUT overlay |
| `Flux/IBL/ForceRoughness` | bool | Override surface roughness |
| `Flux/IBL/ForcedRoughness` | float | Roughness value when forced (0-1) |
| `Flux/IBL/RegenerateBRDFLUT` | bool | Force a BRDF LUT regeneration |
| `Flux/IBL/Textures/BRDF_LUT` | texture | Live BRDF LUT preview |

Enable/diffuse/specular toggles are **not** debug variables — they live in
`Zenith_GraphicsOptions` (`m_bIBLEnabled`, `m_bIBLDiffuseEnabled`,
`m_bIBLSpecularEnabled`), read via `IsEnabled()` / `IsDiffuseEnabled()` / `IsSpecularEnabled()`.

## Debug Modes

```cpp
IBL_DEBUG_NONE             // Normal rendering
IBL_DEBUG_IRRADIANCE_MAP   // Show irradiance as sphere
IBL_DEBUG_PREFILTERED_MIPS // Show roughness mip levels
IBL_DEBUG_BRDF_LUT         // Show BRDF integration texture
IBL_DEBUG_DIFFUSE_ONLY     // Only diffuse IBL contribution
IBL_DEBUG_SPECULAR_ONLY    // Only specular IBL contribution
IBL_DEBUG_FRESNEL          // Fresnel term visualization
IBL_DEBUG_REFLECTION_VECTOR// Reflection direction arrows
IBL_DEBUG_PROBE_VOLUMES    // Probe influence wireframes
IBL_DEBUG_PROBE_CAPTURE    // Preview probe cubemap
IBL_DEBUG_ROUGHNESS_LOD    // Which mip level is sampled
```

## Configuration (`IBLConfig`, in `Flux_IBLImpl.h`)

| Constant | Value | Purpose |
|----------|-------|---------|
| `uBRDF_LUT_SIZE` | 512 | BRDF LUT dimensions |
| `uIRRADIANCE_SIZE` | 32 | Irradiance cubemap face size |
| `uPREFILTER_SIZE` | 128 | Prefiltered cubemap face size (mip 0) |
| `uPREFILTER_MIP_COUNT` | 7 | Prefiltered specular mip chain length |
| `uMAX_PROBES` | 16 | Maximum IBL probes |
| `uPASSES_PER_FRAME` | 8 | Regeneration passes executed per frame (frame amortization) |

## Render-Graph Integration & Frame-Amortized Regeneration

`SetupRenderGraph()` declares **49 render-graph passes** (1 BRDF LUT pass writing a 2D texture + 48 passes writing cubemap subresources):

- 1 BRDF LUT pass (`ExecuteBRDFLUTPass`)
- 6 irradiance face passes (`ExecuteIrradianceFacePass`, one per cube face)
- 42 prefilter mip-face passes (`ExecutePrefilterMipFacePass`, `uPREFILTER_MIP_COUNT` × 6 faces)

Regeneration is **amortized across multiple frames** rather than done in one frame. `m_eRegenState`
(`IBL_RegenState`: `IBL_REGEN_IDLE` → `IBL_REGEN_IRRADIANCE` → `IBL_REGEN_PREFILTER`) tracks progress,
and `UpdateGraphPassEnables()` enables at most `uPASSES_PER_FRAME` (8) of the convolution/prefilter
passes per frame so the cost is spread out. `IsReady()` returns true once all IBL textures have been
generated at least once (`m_bIBLReady`).

## IBL Math (Split-Sum Approximation)

The specular IBL integral is split into two parts:

```
L_o = ∫ L_i(l) * f(l,v) * (n·l) dl

    ≈ (∫ L_i(l) * D(l,h) dl) * (∫ f(l,v) * (n·l) dl)
        ↑ Prefiltered Map          ↑ BRDF LUT
```

### BRDF LUT Generation
```glsl
// Input: UV.x = NdotV, UV.y = roughness
// Output: RG = (scale, bias) for F0 * scale + bias
vec2 IntegrateBRDF(float NdotV, float roughness)
{
    // Importance sample GGX
    // Accumulate: A = (1-Fc) * G_Vis, B = Fc * G_Vis
    // Where Fc = Schlick Fresnel term
}
```

### Deferred Shading Usage
```glsl
// F0 (reflectance at normal incidence)
vec3 F0 = mix(vec3(0.04), albedo, metallic);

// Diffuse IBL
vec3 irradiance = texture(irradianceMap, N).rgb;
vec3 diffuseIBL = irradiance * albedo * (1.0 - metallic);

// Specular IBL
float mipLevel = roughness * maxMipLevel;
vec3 prefilteredColor = textureLod(prefilteredMap, R, mipLevel).rgb;
vec2 brdf = texture(brdfLUT, vec2(NdotV, roughness)).rg;
vec3 specularIBL = prefilteredColor * (F * brdf.x + brdf.y);
```

## Integration Points

**Uses:**
- `Flux_Graphics::s_xFrameConstantsBuffer` for view matrices
- `Flux_Graphics::s_xQuadMesh` for fullscreen passes

**Used by:**
- `Flux_DeferredShading` samples IBL textures for ambient lighting

## Initialization & Render-Graph Order

IBL is a registered render feature — `RegisterFeature<&Zenith_Engine::IBL>(xReg, "IBL", Flux_IBLShaders::apxALL)`
in `RegisterDefaultFeaturesInto()` (`Flux_FeatureRegistry.cpp`), placed right after `FluxGraphics`.

The registry drives three walks (see `Flux/CLAUDE.md`): `Initialise` runs in registration order
(only "FluxGraphics first" is load-bearing — every feature's init touches only foundation + its own
state), `SetupRenderGraph` is appended at the call site, and `Shutdown` runs in reverse. The
**load-bearing** constraint is render-graph declaration order: IBL declares its texture-writing passes
before `DeferredShading` declares the passes that read those textures.

The sky source (`Skybox`) is registered after IBL; IBL samples it lazily when marked dirty
(`UpdateSkyIBL()` / the regen state machine), not during `Initialise()`.

## Common Operations

### Access IBL textures in shaders:
```cpp
// In deferred shading setup
xLayout.m_axBindingGroups[0].m_axBindings[X].m_eType = BINDING_TYPE_TEXTURE;  // BRDF LUT
xLayout.m_axBindingGroups[0].m_axBindings[Y].m_eType = BINDING_TYPE_TEXTURE;  // Irradiance
xLayout.m_axBindingGroups[0].m_axBindings[Z].m_eType = BINDING_TYPE_TEXTURE;  // Prefiltered

// Binding
xBinder.BindSRV(xBRDFBinding, &g_xEngine.IBL().GetBRDFLUTSRV());
xBinder.BindSRV(xIrradianceBinding, &g_xEngine.IBL().GetIrradianceMapSRV());
xBinder.BindSRV(xPrefilteredBinding, &g_xEngine.IBL().GetPrefilteredMapSRV());
```

### Trigger IBL update when lighting changes:
```cpp
// When sun direction changes significantly
g_xEngine.IBL().MarkAllProbesDirty();
g_xEngine.IBL().UpdateSkyIBL();
```

## Performance Notes

- BRDF LUT: Generated once at startup (~5ms)
- Irradiance convolution: ~2ms when updated
- Prefilter generation: ~10ms when updated (7 mip levels, amortized across frames)
- Runtime IBL sampling: ~0.2ms added to deferred shading
- VRAM: BRDF LUT ~1MB, Irradiance ~0.5MB, Prefiltered ~10MB

## Future Work

- Scene probe capture for local reflections
- Probe interpolation and blending
- Screen-space reflections fallback
- Realtime convolution with temporal spreading
