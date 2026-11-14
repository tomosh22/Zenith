# LIGHT design: exposure, depth of field, motion blur and bloom

Section file of `Zenith2/Docs/Design/LIGHT.md`. It specifies exposure (units, the pre-exposure pipeline, the luminance histogram, metering, adaptation and manual camera exposure), depth of field, motion blur and bloom. The passes are Rendering's: `Zenith2/Docs/Rendering/PerPass/Common.md → "AutoExposure"` (`Z2-B-LIGHT-011`), `→ "DepthOfField"` (`Z2-B-LIGHT-012`), `→ "MotionBlur"` (`Z2-B-LIGHT-013`) and `→ "Bloom"` (`Z2-B-LIGHT-014`), all queue G, all in the common epilogue at output extent ([Z2-N-REND-785]).

## Scope of this file

| Topic | Heading |
|---|---|
| EV100 and pre-exposure | "Exposure units", "Pre-exposure pipeline" |
| the per-view exposure state | "Exposure buffer" |
| automatic exposure | "Luminance histogram", "Metering", "Adaptation" |
| manual exposure | "Camera exposure" |
| camera effects | "Depth of field", "Motion blur" |
| bloom | "Bloom" |
| the header | "Flux_PostProcess header" |

## Exposure units

| Quantity | Definition |
|---|---|
| EV100 | `log2(N² / t × 100 / S)` for aperture N (f-stops), shutter t (seconds) and sensitivity S (ISO) |
| scene key | average luminance `L_avg` (nits) maps to `EV100 = log2(L_avg × 100 / 12.5)` (reflected-light meter constant K = 12.5) |
| exposure multiplier | `exposure = 1 / (1.2 × 2^EV100)` (the saturation-based sensitivity of Lagarde and de Rousiers 2014: the luminance that maps to 1.0 is `1.2 × 2^EV100` nits) |
| compensation | `EV100_effective = EV100 − compensation` (positive compensation brightens) |

## Pre-exposure pipeline

Radiance is pre-exposed in every render-extent pass (opaque, sky, transparent, particles, fog), so HDR colour fits R11G11B10F's range.

| Step | Rule |
|---|---|
| 1 | the CPU keeps `Flux_ExposureState` per view: the exposure multiplier read back from the view's exposure buffer through a `Flux_ReadbackHandle` issued every frame (`Zenith2/Docs/Rendering/GpuResources.md → "Readbacks"`); each frame uses the newest `READY` value, 2 to 3 frames old |
| 2 | that value is the frame's `fPreExposure`, written into every pass's plain-data `m_fExposure` field (`Zenith2/Docs/Rendering/PerPass/ForwardPlus.md → "ForwardPlusOpaque"`, `Zenith2/Docs/Rendering/PerPass/Common.md → "SkyAtmosphere"`) and into `Flux_LightingInput::m_fPreExposure` (item (c) of X103) |
| 3 | the histogram divides the colour by the same `fPreExposure`, so metering sees absolute luminance whatever the lag |
| 4 | tonemap and bloom apply `exposure_current / fPreExposure`, reading the current exposure from the GPU buffer, so the image uses the adapted exposure without waiting for the readback |
| 5 | a history resampled across frames (SSR previous colour, SSGI, TAA) is rescaled by `fPreExposure_current / fPreExposure_previous` |
| 6 | a pending or failed readback keeps the previous value; with no value yet, `fPreExposure` is the view's configured exposure. A readback declared in a frame that submits nothing fails with `FLUX_READBACK_ERROR_FRAME_DEFERRED` or `_FRAME_FAILED` (`Zenith2/Docs/Design/RHI/Memory.md → "Frame deferral"`); `Consume` treats it as any failed readback, counts no `Flux.Exposure.Reset`, and the next built frame declares a new one. The GPU exposure buffer is not written by such a frame, so the adapted state on the GPU is unchanged too |

## Exposure buffer

`Flux_ViewPersistentState::m_xExposure`, 16 bytes, little-endian:

| Offset | Field | Meaning |
|---|---|---|
| 0 | `f32 m_fExposure` | the exposure multiplier applied by tonemap and bloom |
| 4 | `f32 m_fEV100` | the current adapted EV100 |
| 8 | `f32 m_fTargetEV100` | this frame's metered target |
| 12 | `u32 m_uFlags` | bit 0 valid, bit 1 manual, bit 2 clamped to the minimum, bit 3 clamped to the maximum |

## Luminance histogram

`Histogram.slang`: 256 bins of `log2` luminance over `[m_fHistogramMinLog2, m_fHistogramMaxLog2]` = [−12, +18] (30 stops, about 0.12 stop per bin). The pass reads the plain-data fields `m_fPreExposure`, `m_eMetering`, `m_fHistogramMinLog2` and `m_fHistogramMaxLog2`, and the adapt pass `m_fLowPercent`, `m_fHighPercent`, `m_fSpeedUp`, `m_fSpeedDown`, `m_fDeltaSeconds`, `m_fCompensation`, `m_fMinEV100` and `m_fMaxEV100`, none of which Rendering's `AutoExposure` table lists yet: item (j) of X103 (applied by core-doc batch 4), with the rows of `Zenith2/Docs/Design/LIGHT.md → "Rendering table additions pending X103"`.

| Step | Rule |
|---|---|
| input | the TAA output colour at output extent ([Z2-N-REND-787]), read at one texel per 2 × 2 block (desktop) or 4 × 4 block (mobile) |
| luminance | `Y = dot(rgb, (0.2126, 0.7152, 0.0722)) / fPreExposure` (Rec.709 luminance of absolute radiance) |
| bin | `floor((log2(max(Y, 2^min)) − min) / (max − min) × 255)`, clamped to [0, 255]; pixels with `Y ≤ 2^−12` fall in bin 0 |
| weight | the metering weight of "Metering", quantised to 8 bits and added to the bin |
| accumulation | groupshared bins per 16 × 16 group, then one atomic add per non-zero bin into the 256-entry buffer (integer atomics, so the sum is order-independent and identical every run) |

## Metering

| Mode (`Flux_ExposureSettings::m_eMetering`) | Weight w(uv) |
|---|---|
| `FLUX_METERING_AVERAGE` | 1 |
| `FLUX_METERING_CENTRE_WEIGHTED` (default) | `exp(−|uv − 0.5|² / (2 × 0.25²))` |
| `FLUX_METERING_SPOT` | 1 inside a circle of radius 0.1 (of the view height) around the centre, else 0 |

`Adapt.slang`, one group: sort-free percentile averaging over the histogram, discarding the darkest `m_fLowPercent` (default 50%) and the brightest `100 − m_fHighPercent` (default 5%) of the weighted pixel count; a bin that straddles a cut contributes the fraction of its weight inside the kept range; the weighted average of `log2 Y` over the kept weight, each bin at its centre `min + (k + 0.5) × (max − min) / 255` (the bin width of "Luminance histogram"), gives `log2 L_avg`, and `m_fTargetEV100 = clamp(log2(L_avg × 100 / 12.5) − compensation, m_fMinEV100, m_fMaxEV100)` (defaults −4 and 16). A histogram with no weight keeps the previous target.

## Adaptation

| Rule | Value |
|---|---|
| step | `EV_t = EV_{t−1} + (EV_target − EV_{t−1}) × (1 − exp(−Δt × speed))` |
| speeds | `m_fSpeedUp` = 3.0 per second when brightening (EV decreasing), `m_fSpeedDown` = 1.0 when darkening |
| Δt | the monotonic wall-clock interval between two builds of this view's graph (`Zenith_GetMonotonicTimeNs`, `Zenith2/Docs/Design/CORE/Diagnostics.md → "Clocks"`), clamped to [0, 0.1] s; `r.exposure.fixedDeltaSeconds` > 0 replaces it (tests, parity and budget runs set it, so a temporal sequence is reproducible) |
| invalid history | the first frame of a view or after a camera cut: `EV_t = EV_target` (snap), starting from the view's configured EV100 as the previous value ([Z2-N-REND-599]) |
| output | `m_fExposure = 1 / (1.2 × 2^EV_t)`; non-finite values reset the buffer to the configured EV100 and count `Flux.Exposure.Reset` |

## Camera exposure

`Flux_CameraLightingSettings` travels with each view request (row L2 of the cross-area notes):

| Mode (`Flux_ExposureMode`) | Rule |
|---|---|
| `FLUX_EXPOSURE_MODE_AUTO` | the histogram and adaptation above |
| `FLUX_EXPOSURE_MODE_MANUAL_EV` | `EV100 = m_fManualEV100`; no histogram pass is added |
| `FLUX_EXPOSURE_MODE_PHYSICAL` | `EV100 = log2(N² / t × 100 / S)` from the camera's aperture, shutter and ISO; no histogram pass |

Capture views always use a fixed EV100 (the probe's `captureEV100`) and add neither pass ([Z2-N-REND-601]). Preview views use `FLUX_EXPOSURE_MODE_MANUAL_EV` with EV100 = 10 unless the preview request sets one.

## Depth of field

Per camera (`FLUX_VIEW_FEATURE_DEPTH_OF_FIELD`), Stage 5.

| Step | Rule |
|---|---|
| circle of confusion | physical (default when the camera has a focal length): thin lens, `CoC_mm = |A f (z − z_f)| / (z (z_f − f))`, A = f / N, then `CoC_px = CoC_mm / sensorWidth_mm × outputWidth`, signed negative in front of the focus plane; artistic (`m_fFocalLengthMm = 0`): `CoC_px = clamp((z − z_f) / focusRange, −1, 1) × m_fMaxCoCPixels`; clamped to `±m_fMaxCoCPixels` (default 32 at 1440p, scaled with output height); R16F |
| gather | half output extent; per pixel `uPERM_QUALITY` 0: 24, 1: 48 samples on a golden-angle spiral of radius |CoC| (the pixel's own, dilated by the 8 × 8 tile maximum of |CoC|, computed in the same dispatch through groupshared memory); scatter-as-gather weight `saturate(|CoC_sample| − distance + 1)`; near-field samples (negative CoC) weight over far ones by `saturate(−CoC_sample)`, so foreground blur bleeds over focused background |
| composite | `lerp(full-resolution colour, bilinear-upsampled gather, saturate(|CoC_px| − 0.5))` |
| bokeh | circular (the spiral); aperture blade shapes are excluded (design choice LR7 of `Zenith2/Docs/Design/LIGHT.md → "Design choices this family makes"`) |

## Motion blur

Optional per view (`FLUX_VIEW_FEATURE_MOTION_BLUR`), Stage 5; the reconstruction filter of McGuire et al. 2012.

| Step | Rule |
|---|---|
| velocity scale | `v_px = velocity × outputExtent × m_fShutterFraction` (default 0.5, a 180° shutter), clamped to `m_fMaxBlurPixels` (default 32 at 1440p, scaled with output height) |
| tile max | per 16 × 16 output-pixel tile, the velocity of largest magnitude (RG16F) |
| neighbour max | per tile, the largest of the 3 × 3 tile neighbourhood |
| gather | `uPERM_SAMPLES` 8 or 16 samples along the neighbour-max velocity, jittered by IGN, with the soft depth and velocity comparisons of the reconstruction filter; tiles whose neighbour max is below 0.5 px copy the input |
| camera cut and teleport | a camera cut produces zero camera velocity; a history-reset instance has zero object velocity ([Z2-N-REND-009]), so neither smears |

## Bloom

Every tier, Stage 3; the chain of Jimenez 2014.

| Step | Rule |
|---|---|
| first downsample | from the TAA output to half output extent: 13-tap filter with the Karis average (weights `1 / (1 + luma)` per 2 × 2 group) to suppress fireflies; colour × `exposure_current / fPreExposure`; an optional soft threshold T with knee K (`m_fThreshold`, `m_fKnee`): `contribution = colour × max(min((Y − T + K)², 4K × Y) / (4K + 1e-5), Y − T) / max(Y, 1e-5)` for `Y > T − K`, else 0; disabled by default (T = 0, which passes all energy) |
| downsample chain | `render.quality.bloomLevels` levels, 4 to 6 per tier (`Zenith2/Docs/Design/LIGHT/QualityTiers.md → "Post-processing and output tiers"`), each the 13-tap filter without the Karis average; Rendering's `Bloom` table still says "6-level (desktop) or 5-level (mobile)", which X148 changes to this key (`Zenith2/Docs/Design/LIGHT.md → "Rendering table additions pending X103"`) |
| upsample chain | a 3 × 3 tent filter of radius `m_fRadius` (default 0.85) accumulated into the next larger mip (read-write) |
| composite | in tonemap: `colour = lerp(colour, bloom, m_fIntensity)` (default 0.04), which conserves energy |

## Flux_PostProcess header

```cpp
// Zenith2/Engine/ZenithFlux/Flux_PostProcess.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithFlux/Flux_LightSettings.h"
#include "ZenithFlux/Flux_Readback.h"
#include "ZenithFlux/Flux_RenderGraph.h"

/// @contract type Flux_ExposureState
/// Ownership: Owned by Flux_LightingSystem, one per view with auto exposure, keyed by Flux_ViewID.
/// Thread affinity: OWNER_THREAD on the Flux.BuildGraph node.
/// Invalidation: Released with the view's persistent state ([Z2-N-REND-600]).
/// Units: The exposure multiplier (dimensionless) and EV100.
/// Errors: A failed readback keeps the previous value.
/// Version: since Z2 Stage 3.
/// @endcontract
class Flux_ExposureState
{
public:
	void Consume(Flux_ReadbackHandle& xReadback);   // takes a READY value and releases the handle
	float GetPreExposure() const;
	void Reset(float fConfiguredEV100);
private:
	float m_fPreExposure = 1.0f;
	float m_fLastEV100 = 0.0f;
};

/// @contract functions Flux_ExposureMaths
/// Ownership: Pure functions over values.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: EV100, nits, seconds, f-stops, ISO.
/// Errors: Non-positive aperture, shutter or ISO is ZENITH_ASSERT (configuration validation rejects them first).
/// Version: since Z2 Stage 3.
/// @endcontract
float Flux_EV100FromCamera(float fAperture, float fShutterSeconds, float fISO);
float Flux_EV100FromLuminance(float fAverageNits);
float Flux_ExposureFromEV100(float fEV100);
float Flux_AdaptEV100(float fPrevious, float fTarget, float fDeltaSeconds, float fSpeedUp, float fSpeedDown);
float Flux_MeterHistogramEV100(const uint32* puBins, float fMinLog2, float fMaxLog2, float fLowPercent,
	float fHighPercent, float fCompensation, float fMinEV100, float fMaxEV100, float fPreviousTarget);   // 256 bins; the CPU reference of Adapt.slang
float Flux_CircleOfConfusionPixels(float fDepthMetres, const Flux_CameraLightingSettings& xCamera, uint32 uOutputWidth);

/// @contract functions Flux_PostProcessFeatures
/// Ownership: Feature functions that add passes to the graph they receive.
/// Thread affinity: The Flux.BuildGraph node.
/// Invalidation: Returned handles are valid for the current graph only.
/// Units: Output-extent colour, pre-exposed.
/// Errors: No pass is added when the view's feature bit is clear or the exposure mode is not automatic.
/// Version: since Z2 Stage 3 (exposure, bloom); Z2 Stage 5 (depth of field, motion blur).
/// @endcontract
Flux_RGBuffer Flux_AddAutoExposure(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, const Flux_ViewTargets& xTargets);
Flux_RGTexture Flux_AddDepthOfField(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_RGTexture xColour, Flux_RGTexture xDepth);
Flux_RGTexture Flux_AddMotionBlur(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_RGTexture xColour,
	Flux_RGTexture xVelocity, Flux_RGTexture xDepth);
Flux_RGTexture Flux_AddBloom(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_RGTexture xColour, Flux_RGBuffer xExposure);
```

The four feature signatures are Rendering's. `Flux_AddAutoExposure` also issues the frame's exposure readback. Implementation: `Zenith2/Engine/ZenithFlux/Internal/Passes/Exposure/`, `Internal/Passes/DoF/`, `Internal/Passes/MotionBlur/`, `Internal/Passes/Bloom/`.
