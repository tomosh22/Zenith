# Flux IBL (Image-Based Lighting) Pipeline

## Overview

Image-Based Lighting system for realistic ambient lighting and reflections. Uses a split-sum approximation with precomputed BRDF LUT for efficient real-time IBL. Provides diffuse irradiance from environment and roughness-based specular reflections.

## The energy contract (ZM-D-171 — read before touching any of this)

- **There is NO IBL intensity scale.** The old `m_fIntensity = 0.5` (and its
  `g_fIBLIntensity` shader multiply in DeferredShading + Translucency) was an
  exposure-era fudge and was DELETED. The irradiance cube stores E/π via
  cosine-weighted Monte Carlo (the π in the Lambert BRDF and the π in the PDF
  cancel), so `kD * irradiance * albedo` in the deferred shader is
  energy-correct as-is; any scale on it breaks sun↔sky consistency.
- **The environment the cubes integrate includes a Lambertian VIRTUAL GROUND**
  — now **scene-authored** as `Zenith_AtmosphereComponent::GetGroundAlbedo`
  (default 0.25, the measured mean shortwave albedo of vegetated land; ~0.7 for
  snow/desert, ~0.05 for basalt/ocean; UE5's SkyAtmosphere ships the same
  mechanism at 0.4). It used to be the `IBL_GROUND_ALBEDO` constant in
  `Shaders/Common/Atmosphere.slang`. Without it the cube's lower hemisphere is
  black and every vertical surface loses the bounce light that fills real
  shadows — the ZM-D-168 "vertical faces render near-black" defect. The
  visible-sky pass passes ground albedo 0 (real terrain supplies the ground
  there); the multiple-scattering bake **does** use it, because the ground
  bounce is a real part of the multiply-scattered field.
- **The sky integral includes orders 2..infinity** (Hillaire multiple
  scattering), for both the visible sky and the capture, gated by
  `Zenith_GraphicsOptions::m_bAtmosphereMultiScatteringEnabled`. The maths is
  shared (`Shaders/Common/MultiScatter.slang`) but the **LUTs are not**: the
  Skybox bakes from the LIVE medium, the capture from its FROZEN snapshot. One
  shared LUT would force one of them to integrate the wrong atmosphere.
- **The direct sun key derives from the same atmosphere**: sun radiance =
  `AtmosphereConfig::fSUN_INTENSITY` (the engine's ONE radiometric anchor) ×
  per-channel transmittance along the sun ray
  (`Flux/Skybox/Flux_AtmosphereTransmittance.h`, unit-tested + mutation-proven).
  The ray direction comes from the winning scene `Zenith_SunComponent` (or the
  exact historical engine fallback if no scene authors one); that component
  carries direction/time-of-day geometry only. Sun, sky and ambient therefore
  cannot drift apart, and there is no tunable sun colour or radiance.

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
There is no intensity knob (see the energy contract above); `m_bIBLEnabled=false`
falls back to the flat `dbg_fAmbientFallbackIntensity` (0.03) path in
DeferredShading — a DIAGNOSTIC state only, never a shipped look.

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
| `uBUFFER_COUNT` | 2 | Irradiance/prefiltered cubes are double buffered (coherent publication) |

## Render-Graph Integration & Frame-Amortized Regeneration

`SetupRenderGraph()` declares **97 render-graph passes** (1 BRDF LUT pass writing a 2D texture + 48
cubemap-subresource passes **per buffer**):

- 1 BRDF LUT pass (`ExecuteBRDFLUTPass`)
- 6 irradiance face passes × 2 buffers (`ExecuteIrradianceFacePass`, one per cube face)
- 42 prefilter mip-face passes × 2 buffers (`ExecutePrefilterMipFacePass`, `uPREFILTER_MIP_COUNT` × 6 faces)

Regeneration is **amortized across multiple frames** rather than done in one frame. `m_eRegenState`
(`IBL_RegenState`: `IBL_REGEN_IDLE` → `IBL_REGEN_IRRADIANCE` → `IBL_REGEN_PREFILTER`) tracks progress,
and `UpdateGraphPassEnables()` enables at most `uPASSES_PER_FRAME` (8) of the convolution/prefilter
passes per frame so the cost is spread out. `IsReady()` returns true once all IBL textures have been
generated at least once (`m_bIBLReady`).

### Runtime capture: `RequestEnvironmentUpdate` (the ONE invalidation path)

`Flux_GraphicsImpl::UploadFrameConstants` offers the whole live environment — resolved sun direction
plus the authored Rayleigh/Mie/Mie-G medium plus the captured radiometric anchor — to
`Flux_IBLImpl::RequestEnvironmentUpdate` as a `Flux_IBLEnvironmentSnapshot`, every frame. It does not
compare anything itself and it never calls `MarkAllProbesDirty` (nor `UpdateSkyIBL`, which was the same
function under a second name and is deleted). The IBL owns the schedule:

- **Displacement accumulates against the last CAPTURED target**, never the previous frame.
  Re-basing the baseline every frame is why CityBuilder's 120 s day at 60 FPS (~0.05°/frame, under the
  ~0.081° dot threshold `Flux_IBLEnvironment::fSUN_DIRECTION_DOT_EPSILON`) used to leave the IBL frozen
  for the whole day, while at 30 FPS (~0.1°/frame) it crossed every frame and thrashed.
- **An in-flight generation is never restarted.** Changes arriving mid-generation are *coalesced* into
  one latest pending target (`m_xPendingEnvironment`); the active snapshot is immutable for the whole
  generation. Restarting is what starved regeneration at low frame rates.
- **Completion promotes the pending target** and starts the next generation from it if it still differs
  — deferred to the *next* tick, because the completing frame's 8 passes have not been recorded yet
  when the state machine reaches idle.

### Snapshot coherence + coherent publication

Every pass of one generation builds its constants from the frozen `GetActiveEnvironment()` via
`Flux_IBLPassConstants::BuildIrradiance/BuildPrefilter`. The shaders read the **captured** sun direction
out of their pass CB — not the live VIEW-set sun — and the **authored** medium, not the
`Common/Atmosphere.slang` defaults (there is no defaults-only convenience overload left to fall back to;
the capture entry point is `ComputeEnvironmentCaptureScattering`, which takes the medium).

Because consumers sample the cubes on every frame of a 6-frame generation, in-place writes exposed a
prefiltered cube whose mips/faces came from two skies for 5 frames out of 6. Both cubes are therefore
**double buffered**: a generation writes the back pair, and `PublishCompletedGeneration` swaps front/back
only once a *complete* generation has finished. `GetIrradianceMapSRV()`/`GetPrefilteredMapSRV()` return
the front pair (Flux rewrites the persistent VIEW-set image each frame, so the swap needs no descriptor
bookkeeping); consumers declare graph `Read`s of **both** via `DeclareConsumerReads`, since the front
index flips between graph rebuilds. A first generation seeds both buffers in one frame, which also keeps
the compile-time validator satisfied that every cube a pass reads has an enabled writer.

`IsReady()` stays latched while the previous complete cubes remain usable. `IBLRegeneration::*` +
`IBLEnvironment::*` units (including simulated 120 s days at 60 and 30 FPS) and DP's windowed
`Test_SceneSunAuthority_Runtime` lock this behaviour, including the CSM refit that happens from the same
per-frame direction.

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

The sky source (`Skybox`) is registered after IBL; IBL samples it lazily when a capture is scheduled
(`RequestEnvironmentUpdate` / the regen state machine), not during `Initialise()`.

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
There is exactly ONE runtime entry point, and `Flux_GraphicsImpl::UploadFrameConstants` already calls it
every frame with the resolved environment. Do **not** call `MarkAllProbesDirty()` from gameplay or from
another renderer system: it bypasses the accumulate/coalesce/never-restart schedule.
```cpp
Flux_IBLEnvironmentSnapshot xEnv;
xEnv.m_xSunDirection  = xResolvedSunDir;      // normalised travel direction
xEnv.m_fRayleighScale = xAuthored.m_fRayleighScale;
xEnv.m_fMieScale      = xAuthored.m_fMieScale;
xEnv.m_fMieG          = xAuthored.m_fMieG;
xEnv.m_fSunIntensity  = g_xEngine.Skybox().GetSunIntensity();   // captured anchor, read-only
g_xEngine.IBL().RequestEnvironmentUpdate(xEnv);
```

## Performance Notes

- BRDF LUT: Generated once at startup (~5ms)
- Irradiance convolution: ~2ms when updated
- Prefilter generation: ~10ms when updated (7 mip levels, amortized across frames)
- Runtime IBL sampling: ~0.2ms added to deferred shading
- VRAM: BRDF LUT ~1MB, Irradiance ~2×0.5MB, Prefiltered ~2×10MB (double buffered)
- A first generation now seeds BOTH buffers, so the one-frame boot/resize spike is ~2× the old one
  (it only happens on a graph rebuild). Steady-state cost is unchanged: 8 passes/frame into the back pair.

## Known limits of the Sun/Atmosphere model (and why)

These are **deliberate** boundaries, not oversights. Each has a `TODO(<tag>)` at the
place a fix would start; read that comment before attempting one.

| Limit | Where the TODO lives | Why it is expensive |
|---|---|---|
| **No ozone absorption** — the medium is purely scattering, so twilight lacks the violet Chappuis band and reads flatter than Unreal's | `TODO(ozone)` in `Shaders/Common/Atmosphere.slang` (+ a pointer in `Common/MultiScatter.slang`) | Not a parameter: a third layer with a *tent* (not exponential) density profile, threaded into both LUT bakes, both solvers, **and** the CPU mirror `Flux_AtmosphereTransmittance.h` — miss the mirror and the direct sun key silently stops agreeing with the sky it is derived from. Re-derives twilight, so pixel baselines move. |
| **No second atmosphere light (no moon)** | `TODO(second-atmosphere-light)` in `EntityComponent/Components/Zenith_SunComponent.h` | A spine change, not a component: `g_xSunDir_Pad`/`g_xSunColour_Pad` are single `float4`s in the VIEW set (`Flux_PersistentSetLayouts.h`), the sky renders one disk, CSM fits one direction, the capture convolves one sun. DP's night uses ordinary authored lights instead. |
| **No artistic sun colour/intensity override** | `TODO(look-override)` in `Zenith_SunComponent.h` | ZM-D-171 *removed* exactly those knobs because they let sky and key light disagree. The sanctioned way to widen the look space is to widen what is **physically** authorable. |
| **Planet/atmosphere radii are compile-time** | `TODO(planet-radius)` in `Shaders/Common/Atmosphere.slang` | Threading them means changing the LUT parameterisation, both bakes, the CPU mirror and the IBL reference height — all of which currently assume the Earth pair. |
| **Brightness is globally metered** — no way to lift one region without moving the metering everywhere | `TODO(exposure-locality)` in `Flux/Skybox/Flux_SkyboxImpl.h` (beside the anchor) | The fix is *local exposure* in `Flux/HDR`, not a per-scene anchor; an anchor knob re-breaks the agreement the anchor exists to guarantee. |
| **Ambient lags direct light** by up to one capture | — (inherent) | Amortised capture is the design; Unreal's real-time SkyLight capture has the same property. Tune with `m_fIBLCaptureThresholdDegrees` / `m_uIBLPassesPerFrame`. |

## Future Work

- Scene probe capture for local reflections
- Probe interpolation and blending
- Screen-space reflections fallback
- Realtime convolution with temporal spreading
