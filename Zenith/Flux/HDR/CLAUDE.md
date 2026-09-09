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

HDR owns only its **private bloom chains** — one 5-mip chain PER ACTIVE
FULL-PIPELINE VIEW, all graph transients created in `Flux_HDR::SetupRenderGraph`.
The **HDR scene targets** are *shared* render targets owned by `Flux_Graphics` —
created up front in `Flux_GraphicsImpl::SetupRenderGraph` (the first feature),
before any feature writes them, one per view slot. HDR reads / tonemaps a view's
via `GetHDRSceneTarget(uViewSlot)`.

| Target | Owner | Format | Purpose |
|--------|-------|--------|---------|
| HDR scene targets (`m_axHDRSceneTargetHandles[view]`) | `Flux_Graphics` | `RGBA16F` | Per-view HDR scene accumulation (shared; many features write them) |
| Bloom chains (`m_aaxBloomChainHandles[view][mip]`) | `Flux_HDR` | `RGBA16F` | Per-view bloom downsample/upsample chain, 5 mips (HDR-private) |
| Preview LDRs (`m_axPreviewLDR[view]`) | `Flux_Graphics` | `FINAL_RT_FORMAT` | **Persistent** (not a transient) 512² output of a preview view's tonemap, sampled by ImGui across graph rebuilds |

## Target Setups

The HDR scene-target setup helpers live on `Flux_Graphics` (it owns the target):

| Setup | Use Case |
|-------|----------|
| `g_xEngine.FluxGraphics().GetHDRSceneTargetSetup()` | Color-only (deferred shading, fog) |
| `g_xEngine.FluxGraphics().GetHDRSceneTargetSetupWithDepth()` | With depth (particles, SDFs) |

## Pass placement

HDR registers these passes with the render graph; ordering is derived from Read/Write declarations, not from any enum.

| Pass | Scope | Reads | Writes |
|------|-------|-------|--------|
| Luminance histogram (`HDR_LuminanceHistogram`, compute) | **main only** | slot-0 HDR scene | histogram buffer |
| Exposure adaptation (`HDR_Adaptation`, compute) | **main only** | histogram buffer | exposure buffer |
| Bloom (threshold + 4 downsamples + 4 upsamples) | **per view** | that view's post-FX scene colour | that view's bloom mip chain |
| Tonemap (`HDR_ToneMapping`) | **main only** | slot-0 post-FX scene colour, main bloom mip 0, exposure | the **Final RT** (`GetFinalRenderTarget()`) |
| Preview tonemap (`HDR_ToneMapping (Preview)`) | **per preview view** | that view's HDR scene + bloom mip 0 | that view's **persistent preview LDR** |
| Preview LDR transition (`Preview LDR Transition`) | **per preview view** | that view's preview LDR | nothing (layout-only no-op) |

The main tonemap naturally runs after anything that writes the HDR scene (deferred shading, SSAO, fog, particles) and before anything that reads the Final RT (UI text, UI quads, ImGui).

**Auto-exposure is global, not per-view, and that is why the first two rows stay
out of the per-view walk.** There is exactly ONE histogram buffer and ONE exposure
buffer (both created in `Initialise`); the histogram meters the slot-0 HDR scene at
`GetRenderWidth/Height`. Instantiating either per view would put two writers on one
buffer. The preview tonemap binds both buffers purely for descriptor validity — it
forces `m_bAutoExposure = 0`, so it never reads them.

## Per-view instantiation

`Flux_HDRImpl::SetupRenderGraph` declares the two global auto-exposure passes,
then walks
`Flux_GraphicsImpl::RenderViews().ForEachActiveFullPipelineView(...)` — a
captureless trampoline plus a `SetupCtx` carrying `this`, the graph and the
graphics reference — and finally declares the main tonemap. For each visited
view it calls:

| | |
|---|---|
| `SetupBloomViewPasses(xGraph, uSlot, w, h)` | that view's 5 transients + its threshold / 4 downsample / 4 upsample passes, every one with `.View(uSlot)` |
| `SetupPreviewViewPasses(xGraph, xGraphics, uSlot)` | **only when `xView.m_eType == FLUX_RENDER_VIEW_PREVIEW`** — that view's tonemap into its persistent preview LDR, plus the LDR layout-transition no-op |

Membership is decided by view PROPERTIES, never by slot number: slot 0 always
qualifies, a preview slot joins while its owner has it up (so its transients
exist exactly when its passes do, which the graph's unused-transient validation
demands), and depth-only shadow cascades are never full-pipeline. Today that set
is `{main} ∪ {material preview if active}`. Adding a second preview-class view
needs no edit in this feature.

**Pass names come from `Flux_ViewPassName(base, uSlot)`**
(`Flux/RenderViews/Flux_ViewPassNames.h`), not from a per-view literal table.
Slot 0 gets the base pointer back by identity — so the main view keeps the exact
historical spellings that profiling labels, `FindPass` and
`SetPassForceDisabled` key off — and every other slot gets an interned
`"<base> (<suffix>)"`. The one exception is the LDR transition, whose historical
name is a PREFIX (`"Preview LDR Transition"`); that spelling lives as the pool's
single legacy row, so this file spells only the base `"LDR Transition"`.

**★ The main bloom chain is sized from `GetOutputDims()`, NOT
`GetViewSetupDims(0)`.** Every other per-view feature (HiZ / SSAO / SSR / SSGI /
Decals) sizes from `GetViewSetupDims`, which for slot 0 resolves to
`GetRenderDims()` — the DOWNSCALED render resolution under temporal upscaling.
Bloom is output-res by design because it samples the TAA-resolved image (the
"Bloom is AFTER TAA" note at the top of this file). Feeding it
`GetViewSetupDims(0)` would silently halve main bloom the moment upscaling
engaged, and **every gate would stay green**, because upscaling defaults off and
`GetRenderDims()` then returns `GetOutputDims()` verbatim. Preview-class slots
carry no upscaling latch, so `GetViewSetupDims` is exactly right for them — and
it is the same derivation `SetupTransients` sized their HDR scene target with.

**The execute callbacks are view-agnostic.** `ExecuteBloomThreshold`,
`ExecuteBloomDownsample`, `ExecuteBloomUpsample` and `ExecutePreviewTonemap` all
recover the view from `Flux_RenderGraph::GetCurrentRecordingPassViewSlot()` (the
slot the pass declared via `.View`), so the per-mip `UserData` stays mip-only and
one callback serves every view.

The LDR transition's `.View(uSlot)` is **structural only** — it classifies the
pass into its view's chain and selects its name. It binds nothing: a pass's
`m_uViewSlot` reaches exactly one consumer (the VIEW persistent spine set in
`Zenith_Vulkan_CommandBuffer::BindPersistentSpineSets`), and that runs from
`UpdateDescriptorSets` on a draw or dispatch, of which the no-op issues none.

`Games/RenderTest/Tests/Test_RenderGraphViewStructure.cpp`
(`RT_RenderGraphViewStructure`) is the oracle for all of the above: it reads the
live compiled graph and asserts the per-view naming law, the slot-0 inventory
and the slot each pass records on.

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
`Flux_Adaptation.slang` derives the adapted exposure (5%/95% percentile-trimmed
weighted average → target = key / average, min/max clamp, adaptation speed) into
`m_xExposureBuffer`, which the tonemap pass consumes. Whether auto-exposure is
engaged is owned by `Zenith_GraphicsOptions`; the per-frame tuning lives in the
`Flux/HDR/*` debug variables (TargetLuminance, AdaptationSpeed, Min/MaxExposure,
FreezeExposure, ShowHistogram).

**Derived, never tuned (ZM-D-171 — both unit-tested in
`Flux_HDRExposure.Tests.inl`, mutation-proven):**

- **The key (`dbg_fHDRTargetLuminance`) defaults to `fHDR_EXPOSURE_KEY_ISO =
  12.5 / (100 * 1.2) ≈ 0.104`** — the ISO 2720 reflected-light-meter target
  (K = 12.5, ISO 100) with Lagarde & de Rousiers' 1.2 highlight headroom
  ("Moving Frostbite to PBR", SIGGRAPH 2014). The old 0.14 was tuned by eye.
- **The histogram domain (`fHDR_HISTOGRAM_MIN_LOG_LUMINANCE = -10`,
  `fHDR_HISTOGRAM_LOG_LUMINANCE_RANGE = 26`)** derives its top bin
  (2^16 = 65536) from the brightest radiance in the scene, which is **no longer
  the sky**: the sun disc is a physically scaled radiance (the anchor's
  irradiance over the solar solid angle, ~2.5e4) against a sky of 1-7. A top bin
  of 8 put the sun, the sky and every sunlit white surface in the SAME saturated
  bin, which does not merely skew the average — it makes every high percentile
  meaningless, so the highlight protection in `Flux_Adaptation.slang` could not
  tell a clipping surface from the sun. -10 + 26 covers the disc with room to
  spare at a still-fine 0.10 stops per bin. (See the derivation comment on
  `Flux_HDRImpl.h`; this bullet said "range 13, top bin 8" long after the
  constants moved.)
- **Scene sun authoring does not add an exposure or energy knob.** A
  `Zenith_SunComponent` authors direction/time-of-day only. Low-sun warmth,
  sunset dimming, and a zero key below the horizon derive from atmospheric
  transmittance; the histogram maximum continues to derive from the same
  engine anchor, independent of scene direction.
- **There is no exposure seed value.** The exposure buffer seeds current
  exposure with a `<= 0` "unmetered" sentinel; `Flux_Adaptation.slang` SNAPS to
  its first computed target (or identity while nothing is metered), so the
  first metered frame is correctly exposed with no authored guess. (The old
  0.4 daylight seed at three sites is gone.)

## Future Work

- Volumetric light scattering integration
