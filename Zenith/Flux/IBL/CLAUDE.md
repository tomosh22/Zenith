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
(32x32 cube)  (128x128 cube, 5 mips)
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
| `Flux_IBL.h` | Class declaration, debug modes enum, configuration |
| `Flux_IBL.cpp` | Implementation - BRDF LUT generation, texture management |

## Shaders

| Shader | Location | Purpose |
|--------|----------|---------|
| `Flux_BRDFIntegration.frag` | `Shaders/IBL/` | Generates split-sum BRDF LUT (once at init) |
| `Flux_IBLCommon.fxh` | `Shaders/IBL/` | IBL sampling helper functions |

## IBL Textures

| Texture | Format | Size | Purpose |
|---------|--------|------|---------|
| BRDF LUT | RGBA16F | 512x512 | NdotV x Roughness → (scale, bias) |
| Irradiance Map | RGBA16F | 32x32 (cube) | Cosine-weighted hemisphere for diffuse |
| Prefiltered Map | RGBA16F | 128x128 (cube, 5 mips) | Roughness-based specular |

## Debug Variables (via Zenith_DebugVariables)

| Path | Type | Description |
|------|------|-------------|
| `Flux/IBL/Enable` | bool | Enable/disable IBL |
| `Flux/IBL/DebugMode` | uint | Debug visualization mode (0-10) |
| `Flux/IBL/Intensity` | float | IBL brightness multiplier (0-5) |
| `Flux/IBL/DiffuseEnabled` | bool | Enable diffuse IBL |
| `Flux/IBL/SpecularEnabled` | bool | Enable specular IBL |
| `Flux/IBL/ShowBRDFLUT` | bool | Display BRDF LUT overlay |
| `Flux/IBL/ForceRoughness` | bool | Override surface roughness |
| `Flux/IBL/ForcedRoughness` | float | Roughness value when forced (0-1) |

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

## Initialization Order

IBL must be initialized AFTER:
- `Flux_Graphics` (provides quad mesh, frame constants)
- `Flux_Atmosphere` (IBL can sample atmosphere for sky-based IBL)

And BEFORE:
- `Flux_DeferredShading` (deferred shader samples IBL textures)

```cpp
Flux_Atmosphere::Initialise();
Flux_IBL::Initialise();         // After atmosphere
// ...
Flux_DeferredShading::Initialise();  // After IBL
```

## Common Operations

### Access IBL textures in shaders:
```cpp
// In deferred shading setup
xLayout.m_axDescriptorSetLayouts[0].m_axBindings[X].m_eType = DESCRIPTOR_TYPE_TEXTURE;  // BRDF LUT
xLayout.m_axDescriptorSetLayouts[0].m_axBindings[Y].m_eType = DESCRIPTOR_TYPE_TEXTURE;  // Irradiance
xLayout.m_axDescriptorSetLayouts[0].m_axBindings[Z].m_eType = DESCRIPTOR_TYPE_TEXTURE;  // Prefiltered

// Binding
xBinder.BindSRV(xBRDFBinding, &Flux_IBL::GetBRDFLUTSRV());
xBinder.BindSRV(xIrradianceBinding, &Flux_IBL::GetIrradianceMapSRV());
xBinder.BindSRV(xPrefilteredBinding, &Flux_IBL::GetPrefilteredMapSRV());
```

### Trigger IBL update when lighting changes:
```cpp
// When sun direction changes significantly
Flux_IBL::MarkAllProbesDirty();
Flux_IBL::UpdateSkyIBL();
```

## Performance Notes

- BRDF LUT: Generated once at startup (~5ms)
- Irradiance convolution: ~2ms when updated
- Prefilter generation: ~10ms when updated (5 mip levels)
- Runtime IBL sampling: ~0.2ms added to deferred shading
- VRAM: BRDF LUT ~1MB, Irradiance ~0.5MB, Prefiltered ~10MB

## Future Work

- Scene probe capture for local reflections
- Probe interpolation and blending
- Screen-space reflections fallback
- Realtime convolution with temporal spreading
