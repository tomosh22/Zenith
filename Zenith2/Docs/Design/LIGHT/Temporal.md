# LIGHT design: temporal histories, antialiasing, upscaling and dynamic resolution

Section file of `Zenith2/Docs/Design/LIGHT.md`. It specifies how LIGHT manages per-view temporal histories (the Stage 2 item Z2-I-LIGHT-08), the jitter sequence, TAA with variance clipping and velocity reprojection, the temporal upscaler, the upscaler selection, and the dynamic-resolution controller with its 75% floor and thermal input. The pass is Rendering's `Zenith2/Docs/Rendering/PerPass/Common.md → "TemporalAA"` (budget `Z2-B-LIGHT-010`, queue G); histories are Rendering's per-view persistent state (`Zenith2/Docs/Rendering.md → "View identity and persistent state"`, Z2-D-059).

## Scope of this file

| Topic | Heading |
|---|---|
| which histories LIGHT uses and their rules | "Histories LIGHT owns", "The first-write rule for histories", "History invalidation", "Dynamic resolution and history validity" |
| sub-pixel jitter | "Jitter sequence" |
| the TAA resolve | "TAA resolve" |
| the temporal upscaler | "Temporal upscale" |
| which upscaler runs | "Upscaler selection" |
| the resolution controller | "Dynamic-resolution controller", "Thermal and pressure inputs", "Dynamic-resolution telemetry" |
| the headers | "Flux_Temporal header", "Flux_Upscaler header" |

## Histories LIGHT owns

Every history lives in the view's `Flux_ViewPersistentState`, owned by `Flux_ViewRegistry` and keyed by the view ID ([Z2-N-REND-598]); LIGHT reads and writes these members and never keeps a history elsewhere, so an editor viewport never touches the main view's history.

| History | Member | Format and extent | Writer | Reader |
|---|---|---|---|---|
| TAA or upscaler colour | `m_axTAAHistory[2]` (Rendering) | RGBA16F, output extent | `TemporalAA` | next frame's `TemporalAA` |
| exposure | `m_xExposure` (Rendering) | 16-byte buffer (`Zenith2/Docs/Design/LIGHT/PostProcess.md → "Exposure buffer"`) | `AutoExposure` | tonemap, bloom, next frame's adapt |
| SSR previous colour | `m_xSSRPrevColour` (Rendering) | R11G11B10F, render extent of its frame | the copy after the SSR composite | next frame's SSR and SSGI |
| velocity history | `m_axVelocityHistory[2]` (Rendering) | RG16F, render extent | the prepass or G-buffer velocity copy | TAA disocclusion test |
| froxel scattering | `m_axFogScattering[2]` (added, row L7, item (a) of X103) | RGBA16F 3D, froxel grid | `VolumetricFog` | next frame's `VolumetricFog` |
| SSGI irradiance | `m_axSSGIHistory[2]` (added, row L15, item (a) of X103, Stage 5) | RGBA16F, half render extent | `SSGI` | next frame's `SSGI` |

Every history write goes to the current half of its ping-pong pair, is marked with `MarkOutput`, and is read from the previous half in the next submitted frame ([Z2-N-REND-804]). A frame that submits nothing writes no history and swaps no ping-pong (`Zenith2/Docs/Design/RHI/Memory.md → "Frame deferral"`), so LIGHT's validity state advances only on submitted frames (`Zenith2/Docs/Design/LIGHT.md → "History validity state machine"`, `→ "Deferred and failed frames"`).

## The first-write rule for histories

A history is imported into the graph, so the transient first-write rule ([Z2-N-REND-432]) does not reach it; LIGHT applies its own rule:

| Rule | Mechanism |
|---|---|
| defined contents | a history resource is cleared once in the frame it is created (a compute clear to zero, or `AddClearPass`), before any pass could read it, so its contents are never undefined memory |
| no read of an invalid history | while `m_bHistoryValid` is false the consumer leaves its previous-history field unset (`[Optional]`, reading slot 0's null substitute) and its shader takes the no-history branch (`m_uHistoryValid = 0` in TAA's data), so no invalid history reaches an output |
| full-coverage writes | every history write is `[FullCoverage] [WriteOnly]` over the whole current half, so after one valid frame both halves hold defined data |
| NaN guard | the TAA, fog and SSGI resolves replace a non-finite history sample by the current frame's value and count it in `Flux.Temporal.HistoryNaN`; a non-finite current sample is replaced by 0 before the history is written |

<!-- assumption: pending RPROD, X103 -->
Rendering's `TemporalAA` table declares `m_xPrevHistory` without `[Optional]`; LIGHT marks every previous-history field (`TemporalAA`'s `m_xPrevHistory` and the new `m_xPrevVelocity`, `VolumetricFog`'s `m_xPrevScattering`, SSGI's previous history) `[Optional]` so the invalid-history frame declares no read: row L16 and item (g) of X103 (applied by core-doc batch 4).
<!-- /assumption -->

## History invalidation

A view's histories are invalid on the events of [Z2-N-REND-599] and on the additional events LIGHT needs:

| Event | Histories invalidated | Source |
|---|---|---|
| the first frame the view exists | all | Rendering |
| a camera cut (`Flux_ViewRequest::m_bCameraCut`) | all | Rendering; a teleport of the camera entity or of any ancestor sets it (see below) |
| an output-extent change | all | Rendering (swapchain recreation, [Z2-N-REND-983]) |
| a shading-path change | all | Rendering ([Z2-N-REND-692]) |
| a quality-tier change that changes a history's format or extent (`render.quality.upscaler`, `render.quality.froxelGridX`, `…froxelGridY`, `…froxelGridZ`, `render.quality.ssgiEnabled`) | the affected histories only | LIGHT |
| device-loss recovery | all (their resources are recreated) | LIGHT, with `Zenith2/Docs/Design/RHI/Recovery.md → "Device-loss state machine"` |
| memory-pressure action 12 | capture and preview views' | Architecture's shedding table |
| a dynamic-resolution scale change | none of the output-extent histories (see "Dynamic resolution and history validity") | LIGHT |

Teleport: a teleport of an instance resets that instance's motion-vector history (previous transform := current, [Z2-N-REND-009], `Zenith2/Docs/Architecture/LifecycleCommands.md → "Teleport"`), which leaves the view's histories valid; the TAA resolve then treats the instance's pixels as any disocclusion. A teleport of the entity that owns the active camera, or of any of its ancestors, is a camera cut: the engine camera extractor sets `m_bCameraCut` for that frame's view request.

<!-- assumption: pending GAME, RPROD -->
The camera extractor that fills `Flux_ViewRequest` is GAME's and RPROD's; LIGHT relies on it setting `m_bCameraCut` for a teleported camera (row L17 of the cross-area notes).
<!-- /assumption -->

## Dynamic resolution and history validity

[Z2-N-REND-599] invalidates histories on a render-extent change; applied literally, every dynamic-resolution step would restart TAA. LIGHT keeps the rule for histories whose own extent changes and exempts the output-extent histories:

| History | Behaviour when the render extent changes by a dynamic-resolution step |
|---|---|
| TAA or upscaler colour (output extent) | valid: its extent does not change |
| exposure | valid |
| froxel scattering | valid: the froxel grid is fixed per tier |
| SSR previous colour | valid: it is sampled by normalised UV |
| velocity history, SSGI history (render extent) | resampled by normalised UV in the frame after the change and then valid |
| the HiZ pyramid and visibility bits | Rendering's: the previous pyramid is projected with its own extent (`m_xHiZExtent`), so it stays usable for phase 1 |

<!-- assumption: pending RPROD, X104 -->
[Z2-N-REND-599]'s "render-extent change" reads "an output-extent change, or a change of a history resource's own extent" since core-doc batch 4 applied cross-doc item X104 (which also rewords [Z2-N-REND-983] and renames `Test_View_ExtentChangeResetsHistory` to `Test_View_OutputExtentChangeResetsHistory`), and `Flux_ViewPersistentState::m_uRenderWidth`/`m_uRenderHeight` record the extent of the last frame for UV-normalised resampling; row L18 of the cross-area notes. Until X104 is applied, the LIGHT dynamic-resolution ticket cites X104 and changes no Rendering text.
<!-- /assumption -->

## Jitter sequence

| Rule | Value |
|---|---|
| sequence | Halton (2, 3), indices 1 … P, repeating; `Flux_JitterSequence` precomputes the P points in [0, 1)² |
| period P | 8 for TAA; for the upscaler `min(64, max(8, ceil(8 × (output / render)²)))` (the FSR 2 rule) |
| offset | `jitterPixels = halton − 0.5` in render pixels |
| NDC jitter | `m_xJitterNDC = (2 × jitterPixels.x / renderWidth, −2 × jitterPixels.y / renderHeight)`, added to the projection's clip-space translation terms to form `m_xJitteredProjection` (`Zenith2/Docs/Rendering.md → "Render views"`) |
| consumers | every depth-writing and colour pass of the view uses the jittered projection; velocity uses the unjittered matrices; shadow, capture and preview views are not jittered |
| frame index | the view's own counter of submitted frames since its history became valid, so a camera cut restarts the sequence at index 1 and a frame that submits nothing repeats its index rather than skipping one; the counter is `Flux_LightingSystem` state, one per view in the same per-view slot as the view's dynamic-resolution controller and exposure state (at most `Flux_LightingSystemDesc::m_uMaxViews`), advanced only by `EndFrame` on a submitted frame, and a view without a committed counter (its first frame, or an invalid history) uses index 1 |
| accessor | `Flux_LightingSystem::GetJitterOffsetPixels(Flux_ViewID xView, float fOutputOverRender)` (`Zenith2/Docs/Design/LIGHT.md → "Flux_Lighting header"`) returns `jitterPixels` for the view's committed index from a `Flux_JitterSequence` of period `Flux_JitterSequence::PeriodForRatio(fOutputOverRender)`, one cached per period; RPROD's view-list construction calls it in step 4 of `Zenith2/Docs/Design/RPROD/Views.md → "View list construction"` for every view that is jittered (not a shadow, capture or preview view) and has `FLUX_VIEW_FEATURE_TEMPORAL_AA`, with the view's output-over-render ratio, and forms `m_xJitteredProjection` from the result by the NDC row above; every other view's `m_xJitteredProjection` equals its projection (row L40). `Test_Jitter_OffsetAccessor` asserts it |

## TAA resolve

`TAA.slang`, compute, 8 × 8 groups over the output extent; with `bPERM_UPSCALE` false the render and output extents are equal.

| Step | Rule |
|---|---|
| 1 current sample | for output pixel o, the render-space position `r = (o + 0.5) × render / output − 0.5 − jitterPixels`; the nearest render texel's colour, and its 3 × 3 neighbourhood |
| 2 colour space | neighbourhood values are converted to YCoCg and luminance-weighted by `1 / (1 + Y)` (Karis 2014) to suppress fireflies |
| 3 moments | mean μ and standard deviation σ of the 9 samples |
| 4 velocity | the velocity of the front-most (smallest linear depth) of the 3 × 3 render neighbourhood (velocity dilation) |
| 5 reprojection | `uvPrev = uv − velocity`; the history is read with a 5-tap Catmull-Rom (bicubic, Jimenez 2016) filter |
| 6 disocclusion | the history is rejected (α = 1) when `uvPrev` is outside [0, 1]², when the history is invalid, or when the previous velocity at `uvPrev` (the `[Optional]` field `m_xPrevVelocity`, the view's previous velocity-history half, item (g) of X103) differs from the current by more than `max(0.5 px, 0.1 × |velocity|)` in output pixels (the velocity history test); with `m_xPrevVelocity` unset (invalid velocity history) the velocity test is skipped |
| 7 variance clipping | the history (in weighted YCoCg) is clipped toward μ into the box `μ ± γσ` (Salvi 2016), `γ = 1.0` (desktop quality 2: 1.25 when `|velocity| < 0.5 px`); clipping, not clamping, moves the history along the line to μ |
| 8 blend | `α = Flux_TemporalSettings::m_fCurrentWeight` (default 1/10 desktop, 1/8 mobile), raised to `min(1, α + |velocity|_px × 0.02)` for fast motion; `result = lerp(history, current, α)` |
| 9 output | un-weight the luminance, back to RGB; the history half gets RGBA16F (A = the running sample count, saturating at 16, used as a confidence by the upscaler), the output R11G11B10F |

Quality `uPERM_QUALITY` 0 uses a 5-sample plus-shaped neighbourhood and bilinear history; 1 the full algorithm with γ = 1.0; 2 adds the motion-adaptive γ and the anti-ghosting velocity test at full precision.

## Temporal upscale

`Flux_AddTemporalUpscale` is `TAA.slang` with `bPERM_UPSCALE` true (Rendering's resolution boundary, [Z2-N-REND-786]); it differs from the resolve in three steps:

| Step | Upscale rule |
|---|---|
| 1 | the current contribution is a Lanczos-2-windowed sum over the 3 × 3 render texels around r, each weighted by its distance to the output pixel centre in render units, instead of the nearest texel |
| 8 | `α = saturate(w_centre × α_base / (confidence + 1))`: output pixels far from any render sample this frame take less of the current frame, and the history's running count raises its weight |
| jitter | the period of "Jitter sequence" for the ratio in use |

Flux has no other built-in upscaler; a third-party one is the subject of "Upscaler selection".

## Upscaler selection

`Flux_UpscalerMode` is the one upscaler enumeration of Zenith2, declared in `Zenith2/Engine/ZenithFlux/Flux_Upscaler.h` ("Flux_Upscaler header"); RPROD's `Flux_QualitySettings` carries the value through `Flux_LightQualitySettings::m_xTemporal.m_eUpscaler`, read from `render.quality.upscaler`, and declares no upscaler enumeration of its own (RPROD row RP19 withdrew its `Flux_UpscalerKind`; row L28).

| `render.quality.upscaler` value | Enumerator (`Flux_UpscalerMode`) | Technique | Availability |
|---|---|---|---|
| `taa` | `FLUX_UPSCALER_TAA` = 1 | TAA at render = output | every tier |
| `taau` | `FLUX_UPSCALER_TAAU` = 2 | Flux's temporal upscale | every tier; the default whenever dynamic resolution is enabled |
| `external` | `FLUX_UPSCALER_EXTERNAL` = 3 | a third-party temporal upscaler | refused by `Flux_ReadLightQualitySettings` with `FLUX_LIGHTING_ERROR_INVALID_SETTINGS` (the previous value stays) until D-SEL-UPSCALER (open, Stage 5) is ruled |

Value 0 is invalid, so a zero-initialised settings value never selects an upscaler silently.

D-SEL-UPSCALER (open, Stage 5), a selection LIGHT owns: candidates AMD FidelityFX Super Resolution 2 or 3 (MIT, shader source compiled offline through the pinned Slang/DXC toolchain, Vulkan and D3D12, mobile-capable), NVIDIA DLSS (proprietary SDK, desktop NVIDIA only), Intel XeSS (Windows), or none. Criteria: licence compatible with shipping; runs on both RHIs; no runtime shader compilation (Z2-D-025); FLIP against a 4× supersampled reference on the TAA temporal scenes at the same GPU time; S24 availability. Acceptance rule: adopt a candidate for a tier only when it beats `FLUX_UPSCALER_TAAU` on FLIP at equal or lower `Z2-B-LIGHT-010` GPU time on that tier's reference hardware across 10 repetitions (the research procedure of `Zenith2/Docs/Rendering.md → "Shading path research methodology"`). The decision is a harness ticket plus a `needs-human` ruling ticket (Z2-D-084); registering the D-name in `Zenith2/Docs/DecisionLog.md → "Open decisions"` and `Zenith2/Docs/Architecture.md → "Named selection decisions"` is cross-doc item X107 (applied by core-doc batch 4) and row L19 of the cross-area notes; the D-name is registered there, and this section is its record of candidates, criteria and acceptance rule.

## Dynamic-resolution controller

`Flux_DynamicResolutionController`, one per view that has `FLUX_VIEW_FEATURE_TEMPORAL_AA` and dynamic resolution enabled (`Flux_DynamicResolutionSettings::m_bEnabled`, `render.quality.dynamicResolutionEnabled`; main view and editor game view by default, other views never), owned by `Flux_LightingSystem`, updated by `Flux_LightingSystem::UpdateDynamicResolution` on `Flux.BuildGraph` before the view list is built. `Flux_LightingSystem::HasDynamicResolution(xViewID)` answers whether a view has one; `GetDynamicResolution` asserts it.

| Input | Source |
|---|---|
| GPU frame time | the difference between the first and last timestamp of the newest completed frame of this view's graph (`Zenith2/Docs/Rendering/RenderGraph.md → "GPU timestamps"`), 2 to 3 frames old, delivered as a `Flux_ViewGpuTime` with that frame's index; the controller consumes a frame index once and skips a sample whose index is not newer than the last consumed one, so an update without a new completed frame (for example after a deferred frame) changes nothing |
| target | `Flux_DynamicResolutionSettings::m_fTargetMilliseconds` (default 16.7 × 0.9 = 15.0 ms; budget runs take the workload's value, [Z2-N-BUDGET-122]) |
| floor | `max(Z2-B-SHIP-011, Flux_DynamicResolutionSettings::m_fMinScale)`: the Dynamic-resolution floor row is 0.75 per axis ([Z2-N-BUDGET-087]); the tier's `render.quality.dynamicResolutionMin` may raise it, never lower it |
| cap | `min(Flux_DynamicResolutionSettings::m_fMaxScale, thermal cap)` (`render.quality.dynamicResolutionMax`, ≤ 1; "Thermal and pressure inputs") |

| Step | Rule |
|---|---|
| 1 smoothing | `t̄ = t̄ + 0.15 × (t − t̄)` (exponential moving average) |
| 2 desired scale | `s* = s × √(target / t̄)` (the pixel count scales with the square of the per-axis scale) |
| 3 step limit | `s* ∈ [s × 0.95, s × 1.05]` |
| 4 hysteresis | decrease immediately when `t̄ > target` for 2 consecutive frames; increase only after `t̄ < 0.9 × target` for 8 consecutive frames |
| 5 quantisation | round to a multiple of 0.01, so at most 26 distinct extents exist between the floor and 1.0 and the graph caches stay bounded |
| 6 clamp | `s ∈ [floor, cap]` |
| 7 publish | `GetScale()` returns s; RPROD's `Flux_BuildViewList` step 4 reads it for the view and sets `Flux_ViewExtent::m_fDynamicResolutionScale = s`, render extent `ceil(output × s)` per axis ([Z2-N-REND-591]) |

The controller is the only writer of the scale: RPROD's view list takes `GetScale()` for a view that has a controller and applies no other factor, a view without one (capture, preview, texture) renders at scale 1.0, and RPROD's quality settings carry no resolution member (RPROD row RP19; row L28). A CPU-bound frame (GPU time below target while the frame misses 16.7 ms) raises the scale toward the cap, which is correct: resolution does not help a CPU bottleneck. A run at the floor that still misses its target fails its row ([Z2-N-BUDGET-088]); the controller never goes below the floor to rescue it.

## Thermal and pressure inputs

| Input | Rule |
|---|---|
| thermal cap | `Flux_DynamicResolutionController::SetThermalCap(float fMaxScale)` is called by ZenithRuntime's `Zenith_ThermalAdaptationTarget::SetResolutionCap` (`Zenith2/Docs/Design/PLAT/Android.md → "Applying thermal actions"`, PLAT row P16) on the main thread; the value is stored in a `std::atomic<uint32>` bit pattern and read at step 6 of the next update; it is clamped to [floor, 1.0] |
| quality-tier offset | thermal adaptation changes `quality.activeTier`; the controller reads its settings from the new tier's values at its next update, like every presentation consumer (`Zenith2/Docs/Design/PLAT/Configuration.md → "Quality tier changes"`) |
| memory pressure | at `MEMORY_PRESSURE_CRITICAL` the scale is the floor while the level lasts (Architecture's shedding action 11, [Z2-N-REND-506]). The one path is RPROD's (`Zenith2/Docs/Design/RPROD/Renderer.md → "Quality tier selection"` step 4): RPROD's pressure hook, on `Flux.BuildGraph`, reads the pressure state (RHI's `Flux_MemoryConsumer`, `Zenith2/Docs/Design/RHI/Memory.md → "Memory pressure responder"`) and calls `Flux_LightingSystem::GetDynamicResolution(xViewID).SetPressureFloor(true)` for every view for which `HasDynamicResolution(xViewID)` is true while the level lasts, and `SetPressureFloor(false)` when it falls. `UpdateDynamicResolution` takes no pressure input and never writes the flag, so RPROD's call is not overwritten; the controller applies the flag at step 6 of its next update and resumes its normal steps one frame after the flag clears. RPROD's hook is the only caller of `SetPressureFloor` (row L28) |
| simulation | none of these inputs touches simulation state, so thermal and pressure adaptation never affect determinism (Z2-D-019, [Z2-N-ARCH-919]) |

## Dynamic-resolution telemetry

| Output | Content |
|---|---|
| counter `Flux.DynRes.ScalePercent` | the main view's scale × 100 each frame |
| counter `Flux.DynRes.FramesAtFloor` | frames rendered at the floor since the run started |
| counter `Flux.DynRes.ThermalCapPercent` | the cap × 100 |
| budget-run fields | `dynresHistogram` (1% buckets) and `framesAtFloor` of the evidence record ([Z2-N-BUDGET-089]), filled by the Player's budget-run mode from these counters |

## Flux_Temporal header

```cpp
// Zenith2/Engine/ZenithFlux/Flux_Temporal.h
#pragma once
#include <atomic>
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithFlux/Flux_LightSettings.h"
#include "ZenithFlux/Flux_RenderGraph.h"
#include "ZenithFlux/Flux_Upscaler.h"

struct Flux_ViewGpuTime;   // Flux_Lighting.h

/// @contract value Flux_JitterSequence
/// Ownership: Plain value; the points live inline (at most 64).
/// Thread affinity: IMMUTABLE after construction.
///   m_uPeriod is const; m_axPoints is non-const because the constructor body fills the points; each cached sequence is
///   constructed once and never assigned.
/// Invalidation: None.
/// Units: Render pixels in [−0.5, 0.5).
/// Errors: A period outside [1, 64] is ZENITH_ASSERT.
/// Version: since Z2 Stage 3. The Halton (2, 3) points are fixed; a change is an image change and regenerates goldens.
/// @endcontract
class Flux_JitterSequence
{
public:
	explicit Flux_JitterSequence(uint32 uPeriod);
	Zenith_Vector2 GetOffsetPixels(uint64 ulFrameIndex) const;
	uint32 GetPeriod() const;
	static uint32 PeriodForRatio(float fOutputOverRender);   // "Jitter sequence"
private:
	Zenith_Vector2 m_axPoints[64];
	const uint32 m_uPeriod = 0;
};

/// @contract type Flux_DynamicResolutionController
/// Ownership: Owned by Flux_LightingSystem, one per dynamic-resolution view, keyed by Flux_ViewID.
/// Thread affinity: ANY_THREAD_EXCEPT(Update, GetScale, SetPressureFloor, Reset). SetThermalCap may be called from the main
///   thread (an atomic); the others run on the Flux.BuildGraph node, SetPressureFloor from RPROD's pressure hook
///   (Zenith2/Docs/Design/RPROD/Renderer.md "Quality tier selection" step 4) and Update from
///   Flux_LightingSystem::UpdateDynamicResolution.
/// Invalidation: None.
/// Units: Per-axis scale of the output resolution; milliseconds.
/// Errors: None; non-finite frame times are ignored and counted in Flux.DynRes.InvalidSamples.
/// Version: since Z2 Stage 3; the thermal input since Z2 Stage 5.
/// @endcontract
class Flux_DynamicResolutionController
{
public:
	explicit Flux_DynamicResolutionController(const Flux_DynamicResolutionSettings& xSettings);
	void Update(const Flux_ViewGpuTime& xSample, const Flux_DynamicResolutionSettings& xSettings);   // skips a frame index already consumed
	float GetScale() const;
	void SetThermalCap(float fMaxScale);
	void SetPressureFloor(bool bForceFloor);
	void Reset();                       // on an output-extent change: scale 1.0 capped
private:
	float m_fScale = 1.0f;
	float m_fSmoothedMs = 0.0f;
	uint32 m_uFramesOver = 0;
	uint32 m_uFramesUnder = 0;
	uint64 m_ulLastFrameIndex = 0;             // the newest consumed sample's frame index
	std::atomic<uint32> m_uThermalCapBits;
	bool m_bPressureFloor = false;
};

/// @contract functions Flux_TemporalFunctions
/// Ownership: Feature functions that add passes to the graph they receive.
/// Thread affinity: The Flux.BuildGraph node.
/// Invalidation: Returned handles are valid for the current graph only.
/// Units: Output-extent colour, pre-exposed.
/// Errors: No pass is added when the view's FLUX_VIEW_FEATURE_TEMPORAL_AA bit is clear; the caller then uses the
///   render-extent colour directly (render extent equals output extent in that case).
/// Version: since Z2 Stage 3.
/// @endcontract
Flux_RGTexture Flux_AddTemporalAA(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_ViewTargets& xTargets);
Flux_RGTexture Flux_AddTemporalUpscale(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_ViewTargets& xTargets);
```

`Flux_AddTemporalAA` is Rendering's function; it dispatches to the upscale variant when the view's render and output extents differ or the tier selects `FLUX_UPSCALER_TAAU`, so the epilogue calls one function.

## Flux_Upscaler header

```cpp
// Zenith2/Engine/ZenithFlux/Flux_Upscaler.h
#pragma once
#include "ZenithCore/Zenith_Types.h"   // uint8

/// @contract enum Flux_UpscalerMode
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; FLUX_UPSCALER_EXTERNAL is refused by Flux_ReadLightQualitySettings until D-SEL-UPSCALER (open,
///   Stage 5) is ruled; 0 is never a valid value.
/// Version: since Z2 Stage 3. The one upscaler enumeration of Zenith2 (RPROD's Flux_Quality.h includes this header);
///   enumerators keep their values once shipped ([Z2-N-CONTRACT-467]).
/// @endcontract
enum Flux_UpscalerMode : uint8 { FLUX_UPSCALER_TAA = 1, FLUX_UPSCALER_TAAU = 2, FLUX_UPSCALER_EXTERNAL = 3 };
```

The header includes only ZenithCore's `Zenith_Types.h` (for `uint8`, so it is self-contained) and nothing from ZenithFlux, so RPROD's `Flux_Quality.h` and LIGHT's `Flux_LightSettings.h` both include it without a cycle. `Flux_AddTemporalUpscale` is the explicit form used by tests. `<atomic>` is on the allowed standard-header list. Implementation: `Zenith2/Engine/ZenithFlux/Internal/Passes/TAA/`, `Internal/Lighting/DynamicResolution/`.
