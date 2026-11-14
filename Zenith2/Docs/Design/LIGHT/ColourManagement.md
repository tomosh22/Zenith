# LIGHT design: colour management, tonemapping and HDR output

Section file of `Zenith2/Docs/Design/LIGHT.md`. It specifies the working colour space, the ACES-style tonemap operators for SDR and HDR, the 3D grading LUT and its cook, the three output transforms (SDR, HDR10, scRGB), output-mode selection and fallback, HDR calibration and metadata, vignette and film grain, and the colour-space matrices every LIGHT CPU and GPU function uses. The pass is Rendering's `Zenith2/Docs/Rendering/PerPass/Common.md → "TonemapAndGrade"` (budget `Z2-B-LIGHT-015`, queue G), and the swapchain formats are Rendering's ([Z2-N-REND-985]).

## Scope of this file

| Topic | Heading |
|---|---|
| the scene-referred space | "Working colour space" |
| the operators | "SDR tonemap operator", "HDR tonemap operator" |
| grading | "Grading LUT", "Grading LUT cook" |
| the display encodings | "Output transforms", "Output mode selection", "HDR calibration and metadata" |
| optional effects | "Vignette and film grain" |
| UI on HDR outputs | "UI and HDR" |
| the matrices | "Colour-space matrices" |
| the pass inputs | "Tonemap parameters" |
| the header | "Flux_ColourManagement header" |

## Working colour space

| Stage of the frame | Space |
|---|---|
| material textures, light colours, GPU tables | linear Rec.709 primaries, D65 white ([Z2-N-REND-014]) |
| HDR colour from the opaque pass to bloom | scene-referred linear Rec.709 radiance, pre-exposed (`Zenith2/Docs/Design/LIGHT/PostProcess.md → "Pre-exposure pipeline"`) |
| after the operator | display-referred, normalised to the output's peak, Rec.709 primaries |
| the swapchain | sRGB-encoded Rec.709 (SDR), PQ-encoded Rec.2020 (HDR10), linear Rec.709 scaled 1.0 = 80 nits (scRGB) |

Zenith2 content is Rec.709-gamut throughout; the HDR outputs extend luminance, not gamut (design choice LR8 of `Zenith2/Docs/Design/LIGHT.md → "Design choices this family makes"`).

## SDR tonemap operator

The ACES 1.x RRT and sRGB ODT approximation of Hill (the "ACES fitted" operator), evaluated per pixel in `float`:

| Step | Formula |
|---|---|
| 1 exposure | `x = colour × exposure_current / fPreExposure`, plus bloom and vignette ("Tonemap parameters") |
| 2 input matrix | `v = M_in × x`, `M_in` = [[0.59719, 0.35458, 0.04823], [0.07600, 0.90834, 0.01566], [0.02840, 0.13383, 0.83777]] (sRGB to AP1 with the RRT saturation) |
| 3 fitted curve | `v' = (v (v + 0.0245786) − 0.000090537) / (v (0.983729 v + 0.4329510) + 0.238081)` per channel |
| 4 output matrix | `w = M_out × v'`, `M_out` = [[1.60475, −0.53108, −0.07367], [−0.10208, 1.10813, −0.00605], [−0.00327, −0.07276, 1.07602]] |
| 5 clamp | `w = saturate(w)`: display-referred linear, 1.0 = the display's white |
| 6 grading | the LUT of "Grading LUT" |
| 7 encode | the sRGB swapchain view encodes in hardware |

`Flux_TonemapACESFitted` implements steps 2–5 on the CPU for the unit tests; the reference values of `Zenith2/Docs/Design/LIGHT/Tests.md → "Colour unit tests"` are computed from these constants in `double`.

## HDR tonemap operator

For HDR10 and scRGB the fitted curve of step 3 is replaced by the parameterised curve of Lottes 2016, solved for the display's peak, keeping the same input and output matrices ("ACES-style"):

| Parameter | Value |
|---|---|
| contrast a | 1.6 |
| shoulder d | 0.977 |
| scene maximum `hdrMax` | 64.0 (scene-linear value mapped to the peak) |
| mid-grey in `midIn` | 0.18 |
| mid-grey out `midOut` | `0.18 × m_fPaperWhiteNits / m_fPeakNits` (paper white default 203 nits, ITU-R BT.2408) |
| b | `(−midIn^a + hdrMax^a × midOut) / ((hdrMax^(a d) − midIn^(a d)) × midOut)` |
| c | `(hdrMax^(a d) × midIn^a − hdrMax^a × midIn^(a d) × midOut) / ((hdrMax^(a d) − midIn^(a d)) × midOut)` |
| curve | `f(v) = v^a / (v^(a d) × b + c)`, so `f(midIn) = midOut` and `f(hdrMax) = 1` |
| output | `w = saturate(M_out × f(M_in × x))`, then `nits = w × m_fPeakNits` |

b and c are computed once per frame on the CPU (`Flux_ComputeLottesCurve`) in `double` and passed as plain data. With `m_fPeakNits = 100` and paper white 100, the HDR operator is the SDR mapping's HDR counterpart; the SDR output never uses it.

## Grading LUT

| Property | Value |
|---|---|
| format | 32³ `R8G8B8A8_UNORM` 3D texture ([Z2-N-REND-038] table), alpha unused |
| domain | the display-referred, normalised output `w` of either operator, shaped by `s = w^(1/2.2)` for perceptually even sampling; the LUT stores the graded `s'`, and `w' = s'^2.2` |
| sampling | `uvw = s × (31/32) + 0.5/32` with `uFLUX_SAMPLER_LINEAR_CLAMP_NO_MIP` (9), trilinear |
| identity | `m_uLUTEnabled = 0` skips the lookup; the engine identity LUT exists for editing |
| source | the world's lighting component names a `.zlut` asset; a camera may override it (`Flux_CameraLightingSettings::m_xGradingLUT`); the camera's wins |
| persistent use | the cooked LUT is `Flux_PersistentReadOnly`, bound as `m_xGradingLUT` (`Zenith2/Docs/Rendering/PerPass/Common.md → "TonemapAndGrade"`) |
| editing | while EDIT's grading editor changes a LUT, the edited LUT is a per-view graph texture uploaded each frame ("or a per-view graph texture while grading is edited", Rendering's table); the edit ends with a re-cook of the `.zlut` |
| blending | a transition between two LUTs (a cinematic cut) blends the two lookups linearly by `m_fLUTBlend` in the shader (two taps) |

## Grading LUT cook

`Cook_GradingLUT` converts an Adobe/Resolve `.cube` file into a `.zlut` (magic `'ZLUT'`, the GEOM cooked file header):

| Rule | Value |
|---|---|
| accepted input | `LUT_3D_SIZE` 17 … 65; `DOMAIN_MIN 0 0 0` and `DOMAIN_MAX 1 1 1` (others are `COOK_ERROR_LUT_FORMAT`); `TITLE` ignored; red varying fastest; values finite in [0, 1] (clamped with a warning otherwise) |
| input encoding | the `.cube` is assumed authored on the shaped domain `s` (import preset key `lutInputEncoding`: `shaped` default, `linear` applies `s = w^(1/2.2)` to the domain before resampling) |
| resampling | to 32³ by tetrahedral interpolation of the source, in `double`, then rounded to 8 bits (round half up) |
| sections | `LIGHT_SECTION_LUT_INFO` (150): size u32 = 32, format u32; `LIGHT_SECTION_LUT_TEXELS` (151): 32³ × 4 bytes, x fastest, zstd |
| fuzz | `Zenith_Fuzz_CubeReader` (`Zenith2/Tools/ZenithCook/Tests/Fuzz/CubeReader.cpp`) |

## Output transforms

| Output (`Flux_OutputMode`) | Swapchain format and colour space | Encoding in the tonemap shader (`uPERM_OUTPUT`) |
|---|---|---|
| `FLUX_OUTPUT_MODE_SDR` | `B8G8R8A8_SRGB` / `B8G8R8A8_UNORM_SRGB`, `VK_COLOR_SPACE_SRGB_NONLINEAR_KHR` / `DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709` | none: linear `w` written through the sRGB view (hardware IEC 61966-2-1 encode) |
| `FLUX_OUTPUT_MODE_HDR10` | `A2B10G10R10_UNORM_PACK32` / `R10G10B10A2_UNORM`, `VK_COLOR_SPACE_HDR10_ST2084_EXT` / `DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020` | `rgb2020 = M_709→2020 × nits`, then PQ per channel |
| `FLUX_OUTPUT_MODE_SCRGB` | `R16G16B16A16_SFLOAT`, `VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT` / `DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709` | `nits / 80` (linear Rec.709; 1.0 = 80 nits) |
| a texture view's output | the view texture's format (R11G11B10F or RGBA8) | SDR operator, no encode for float textures; the sRGB rule for an sRGB texture |

PQ (SMPTE ST 2084): `Y = nits / 10000`, `E = ((c1 + c2 Y^m1) / (1 + c3 Y^m1))^m2`, `m1 = 2610/16384`, `m2 = 2523/4096 × 128`, `c1 = 3424/4096`, `c2 = 2413/4096 × 32`, `c3 = 2392/4096 × 32`. Reference values: 0 nits → 7.3e-7, 100 → 0.50808, 203 → 0.58069, 1,000 → 0.75183, 10,000 → 1.0.

## Output mode selection

| Rule | Value |
|---|---|
| request | `r.output.mode`: `auto` (default), `sdr`, `hdr10`, `scrgb` (a restart-free user setting) |
| auto | HDR10 when the RHI reports the HDR10 colour space for the main window's surface and the OS reports HDR enabled on that display; otherwise SDR |
| unsupported request | an explicit `hdr10` or `scrgb` the surface lacks falls back to SDR with the `flux.colour` warning `output mode <m> unsupported by the display; using sdr` and the counter `Flux.Colour.OutputFallback` |
| change | a mode change is a swapchain recreation (Rendering's trigger "HDR output toggle", `Zenith2/Docs/Rendering/GpuResources.md → "Swapchain recreation"`), which invalidates no history because the output extent is unchanged |
| platforms | Windows: all three; S24: SDR and HDR10 (scRGB is not offered on Android) |
| PSOs | every mode's tonemap permutation is in the boot PSO group ([Z2-N-REND-985]) |

## HDR calibration and metadata

| Setting | Key | Default | Range |
|---|---|---|---|
| peak luminance | `r.hdr.peakNits` | the display's reported maximum luminance, else 1,000 | 400 … 10,000 |
| paper white | `r.hdr.paperWhiteNits` | 203 | 80 … 500 |
| UI brightness | `r.hdr.uiNits` | the paper white | 80 … 500 |

<!-- assumption: pending RHI -->
HDR10 output sets static metadata through the RHI's swapchain (`vkSetHdrMetadataEXT` with `VK_EXT_hdr_metadata`, `IDXGISwapChain4::SetHDRMetaData`): Rec.709 primaries and D65 white (the content gamut), maximum mastering luminance = the peak, minimum 0.0001 nits, MaxCLL = the peak, MaxFALL = `min(peak, 400)`. LIGHT passes a `Flux_HDRMetadata` value to the swapchain manager (`Zenith2/Docs/Design/RHI.md → "Division of Flux code with RPROD"`, `Flux_SwapchainManager`); row L21 of the cross-area notes.
<!-- /assumption -->

## Vignette and film grain

Both optional per camera (`bPERM_VIGNETTE`, `bPERM_GRAIN`), off in parity, capture and preview views.

| Effect | Rule |
|---|---|
| vignette | scene-referred, before the operator: `r = length((uv − 0.5) × (aspect, 1)) / length((0.5 × aspect, 0.5))`, `x *= 1 − m_fVignetteIntensity × r^2.2` |
| film grain | display-referred, after the LUT and before encoding: `n = Flux_Hash2D(pixel, frameIndex)` in [0, 1) (`Zenith2/Shaders/Common/Random.slang`), `w += (n − 0.5) × m_fGrainIntensity × (0.5 + 0.5 × (1 − Y_w))`, Y_w the Rec.709 luminance of w; deterministic for a frame index |

## UI and HDR

The UI composite (`Zenith2/Docs/Rendering/PerPass/Common.md → "UIComposite"`, UI's pass) draws SDR-authored UI onto the encoded output. LIGHT provides `Zenith2/Shaders/Common/ColourSpace.slang` with `Flux_EncodeOutput(half3 xLinearRec709, float fNitsForOne, uint uOutputMode)`, which the UI programs call with `fNitsForOne = r.hdr.uiNits` (SDR: the plain value); UI alpha blending then happens in the output encoding, which on HDR10 is not linear, and is a human sign-off item of the Game2D slice (design choice LR9; row L22 of the cross-area notes).

## Colour-space matrices

`Flux_ColourMatrices` (CPU, `double` constants) and `ColourSpace.slang` (GPU) use exactly these values:

| Conversion | Matrix (rows) |
|---|---|
| Rec.709 → XYZ (D65) | [0.4124564, 0.3575761, 0.1804375], [0.2126729, 0.7151522, 0.0721750], [0.0193339, 0.1191920, 0.9503041] |
| XYZ → Rec.709 | [3.2404542, −1.5371385, −0.4985314], [−0.9692660, 1.8760108, 0.0415560], [0.0556434, −0.2040259, 1.0572252] |
| Rec.709 → Rec.2020 | [0.627404, 0.329283, 0.043313], [0.069097, 0.919540, 0.011362], [0.016391, 0.088013, 0.895595] |
| Rec.2020 → Rec.709 | [1.660491, −0.587641, −0.072850], [−0.124551, 1.132900, −0.008349], [−0.018151, −0.100579, 1.118730] |
| sRGB → AP1 with RRT saturation (`M_in`) | as "SDR tonemap operator" |
| AP1 with ODT saturation → sRGB (`M_out`) | as "SDR tonemap operator" |
| Rec.709 luminance | (0.2126, 0.7152, 0.0722) |

The sRGB transfer functions are CORE's `Zenith_SrgbToLinear` and `Zenith_LinearToSrgb` (`Zenith2/Docs/Design/CORE/Maths.md → "Zenith_Colour"`); the GPU uses the hardware sRGB views and `Flux_EncodeSRGB8` for storage writes ([Z2-N-REND-844]).

## Tonemap parameters

<!-- assumption: pending RPROD, X103 -->
Rendering's `TonemapAndGrade` table lists the resource fields; LIGHT needs the plain-data fields below in `Flux_TonemapParams` and the `[Optional]` second LUT `m_xGradingLUTB`: row L20 and item (j) of X103, which core-doc batch 4 applied.
<!-- /assumption -->

| Field | Meaning |
|---|---|
| `float m_fPreExposure` | the frame's pre-exposure (divides out before the operator) |
| `float m_fBloomIntensity` | `Flux_BloomSettings::m_fIntensity` |
| `float m_fPeakNits`, `m_fPaperWhiteNits` | calibration |
| `float m_fCurveB`, `m_fCurveC` | the Lottes coefficients |
| `uint32 m_uLUTEnabled`, `float m_fLUTBlend` | grading; a second LUT handle `m_xGradingLUTB` (`[Optional]`) for blends |
| `float m_fVignetteIntensity`, `m_fGrainIntensity`, `uint32 m_uFrameIndex` | optional effects |

`Flux_TonemapInputs` (the argument of `Flux_AddTonemap`) is `{ Flux_RGTexture m_xColour; Flux_RGTexture m_xBloom; Flux_RGBuffer m_xExposure; }`.

## Flux_ColourManagement header

```cpp
// Zenith2/Engine/ZenithFlux/Flux_ColourManagement.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithFlux/Flux_LightSettings.h"
#include "ZenithFlux/Flux_RenderGraph.h"

/// @contract enum Flux_OutputMode
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; Flux_SelectOutputMode never returns FLUX_OUTPUT_MODE_COUNT.
/// Version: since Z2 Stage 3 (SDR); HDR10 and scRGB since Z2 Stage 5. Selects the uPERM_OUTPUT value of the tonemap
///   program, so the values are never renumbered.
/// @endcontract
enum Flux_OutputMode : uint8 { FLUX_OUTPUT_MODE_SDR = 0, FLUX_OUTPUT_MODE_HDR10, FLUX_OUTPUT_MODE_SCRGB, FLUX_OUTPUT_MODE_COUNT };

/// @contract value Flux_TonemapInputs
/// Ownership: Plain value of graph handles.
/// Thread affinity: IMMUTABLE.
///   Members non-const: callers default-construct it and fill its fields.
/// Invalidation: Valid for the current graph only.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Flux_TonemapInputs
{
	Flux_RGTexture m_xColour;
	Flux_RGTexture m_xBloom;
	Flux_RGBuffer m_xExposure;
};

/// @contract value Flux_LottesCurve
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: the producer default-constructs it and fills its fields before returning it.
/// Invalidation: None.
/// Units: Dimensionless curve coefficients.
/// Errors: None; peak and paper white are clamped to their configuration ranges before solving.
/// Version: since Z2 Stage 5.
/// @endcontract
struct Flux_LottesCurve { double m_dA = 1.6; double m_dD = 0.977; double m_dB = 0.0; double m_dC = 0.0; double m_dHdrMax = 64.0; };

/// @contract value Flux_HDRMetadata
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: the producer default-constructs it and fills its fields before returning it.
/// Invalidation: None.
/// Units: Nits; CIE 1931 xy chromaticities.
/// Errors: None.
/// Version: since Z2 Stage 5.
/// @endcontract
struct Flux_HDRMetadata
{
	float m_afPrimariesXY[6];   // Rec.709 red, green, blue
	float m_afWhiteXY[2];       // D65
	float m_fMaxMasteringNits;
	float m_fMinMasteringNits;
	float m_fMaxCLL;
	float m_fMaxFALL;
};

/// @contract functions Flux_ColourFunctions
/// Ownership: Pure CPU reference functions over values, and the tonemap feature function.
/// Thread affinity: ANY_THREAD for the pure functions; the Flux.BuildGraph node for Flux_AddTonemap.
/// Invalidation: None.
/// Units: Linear Rec.709 values; nits for the HDR functions.
/// Errors: None; non-finite input returns 0.
/// Version: since Z2 Stage 3 (SDR); Z2 Stage 5 (HDR).
/// @endcontract
Zenith_Vector3 Flux_TonemapACESFitted(Zenith_Vector3 xLinear);
Flux_LottesCurve Flux_ComputeLottesCurve(float fPeakNits, float fPaperWhiteNits);
Zenith_Vector3 Flux_TonemapHDR(Zenith_Vector3 xLinear, const Flux_LottesCurve& xCurve, float fPeakNits);   // returns nits
float Flux_EncodePQ(float fNits);
float Flux_DecodePQ(float fEncoded);
Zenith_Vector3 Flux_Rec709ToRec2020(Zenith_Vector3 xRGB);
Zenith_Vector3 Flux_Rec2020ToRec709(Zenith_Vector3 xRGB);
Zenith_Vector3 Flux_Rec709ToXYZ(Zenith_Vector3 xRGB);
Zenith_Vector3 Flux_XYZToRec709(Zenith_Vector3 xXYZ);
Flux_OutputMode Flux_SelectOutputMode(Flux_OutputModeRequest eRequest, bool bSurfaceHDR10, bool bSurfaceScRGB, bool bDisplayHDR);
Flux_HDRMetadata Flux_MakeHDRMetadata(float fPeakNits);

Flux_RGTexture Flux_AddTonemap(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, const Flux_TonemapInputs& xInputs,
	Flux_RGTexture& xOutput);
```

`Flux_OutputModeRequest` is declared in `Flux_LightSettings.h` (`FLUX_OUTPUT_MODE_REQUEST_AUTO`, `_SDR`, `_HDR10`, `_SCRGB`). `Flux_AddTonemap` keeps Rendering's signature; implementation `Zenith2/Engine/ZenithFlux/Internal/Passes/Tonemap/`, CPU functions `Internal/Lighting/Colour/`.
