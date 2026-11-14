# LIGHT design: ambient occlusion and global illumination

Section file of `Zenith2/Docs/Design/LIGHT.md`. It specifies GTAO, the sky irradiance, the baked global illumination (irradiance volumes and lightmaps: their cooked formats, the bake output contract WORLD produces them against, and their runtime evaluation), screen-space GI as an approximation with a documented error budget, and the selection of the indirect diffuse source per pixel. GTAO is Rendering's pass `Zenith2/Docs/Rendering/PerPass/Common.md → "GTAO"` (budget `Z2-B-LIGHT-005`, queue A). The bake itself (placement, scheduling, the lightmap tracer) is WORLD's (Z2-I-WORLD-06); baked GI's runtime evaluation is Z2-I-LIGHT-04 and dynamic GI is Z2-I-LIGHT-05, both Stage 5.

## Scope of this file

| Topic | Heading |
|---|---|
| ambient occlusion | "GTAO", "GTAO denoise and upsample" |
| the sky's diffuse term | "Sky irradiance" |
| baked GI data | "Irradiance volume cooked format", "Lightmap set cooked format", "Bake output contract" |
| baked GI at runtime | "Irradiance volume sampling", "Lightmap evaluation", "Lightmap binding", "Lightmaps on the deferred paths", "Baked GI residency and streaming", "GI GPU records" |
| dynamic GI | "SSGI", "SSGI pass table", "SSGI error budget" |
| which source a pixel uses | "Indirect diffuse selection" |
| the header | "Flux_GlobalIllumination header" |

## GTAO

Ground-truth ambient occlusion (Jimenez et al. 2016) at render extent (or half with `bPERM_HALF_RESOLUTION`), one thread per pixel in 8 × 8 groups, inputs depth and the path's normals ([Z2-N-REND-803]).

| Parameter | Value |
|---|---|
| slices per pixel | `m_uSliceCount` = 2, 3, 4 for `uPERM_QUALITY` 0, 1, 2 |
| steps per side | 4, 6, 8 for quality 0, 1, 2 |
| world radius | `Flux_SSAOSettings::m_fRadiusMetres` (default 1.0 m), projected to screen at the pixel's depth, clamped to 256 pixels |
| falloff | the horizon weight falls linearly from 1 at `0.6 × radius` to 0 at radius |
| noise | slice rotation `IGN(pixel) + 0.618034 × (frameIndex mod 6)`, step offset `IGN(pixel + (5, 3))`; TAA accumulates the pattern |
| thin-object heuristic | a horizon sample more than `m_fThicknessMetres` (default 0.3 m) in front of the running horizon lerps the horizon back toward it by 0.5 |
| integral | per slice, the two horizon angles clamped to the hemisphere around the projected normal, then the analytic cosine-weighted inner integral `¼(−cos(2h − n) + cos n + 2h sin n)` summed over both sides and weighted by the projected normal length |
| output | visibility in [0, 1] (1 unoccluded), R8_UNORM |

## GTAO denoise and upsample

`GTAODenoise.slang`: a 5 × 5 cross edge-preserving filter on depth (`exp(−|Δz| / (0.05 × z))`) and normal (`max(0, n·n')^16`); with `bPERM_UPSAMPLE` it also upsamples the half-resolution result with a 2 × 2 joint edge-preserving upsample on depth. The denoised visibility is `m_xAO`; `EvaluateLightingEx` combines it with the material AO by `min` and applies the multi-bounce and specular-occlusion terms of `Zenith2/Docs/Design/LIGHT/Lighting.md → "BRDF"`.

## Sky irradiance

The sky's diffuse contribution is its radiance projected onto real spherical harmonics up to band 2 (9 coefficients per channel).

| Rule | Value |
|---|---|
| basis | the real SH basis `Y_lm` of Sloan 2008 (`Y00 = 0.282095`, `Y1m = 0.488603 (y, z, x)`, `Y2m` 1.092548 xy, 1.092548 yz, 0.315392 (3z² − 1), 1.092548 xz, 0.546274 (x² − y²)), in the world frame |
| projection | `c_lm = ∫ L(ω) Y_lm(ω) dω`, computed on the GPU from the captured sky cube (Stage 4) or by the cook for a static environment (`LIGHT_SECTION_ENV_SH`), with texel solid-angle weights `Ω = 4 / (size² (1 + u² + v²)^1.5)` in `double` on the CPU and `float` with a fixed-order reduction tree on the GPU |
| irradiance | `E(n) = Σ Â_l c_lm Y_lm(n)`, `Â_0 = π`, `Â_1 = 2π/3`, `Â_2 = π/4` (Ramamoorthi and Hanrahan 2001); negative results clamp to 0 |
| windowing | the band-2 coefficients are multiplied by a Hanning window `0.5 (1 + cos(π l / 3))` before use, to suppress ringing on high-contrast skies |
| storage | `Flux_SkyIrradianceSH`: 9 `float4` (RGB, pad) = 144 bytes in a buffer bound as `m_xIBLIrradiance`: a `Flux_PersistentReadOnly` buffer in Stage 3, a persistent-imported graph buffer SRV from Stage 4, when the sky capture writes it on the GPU (item (i) of X103) |
| consumers | `Flux_EvaluateIndirectDiffuse` as the last fallback, and the froxel ambient term |

## Irradiance volume cooked format

Stage 4 format, Stage 5 evaluation. A `.zgiv` file (magic `'ZGIV'`, the GEOM cooked file header, `Zenith2/Docs/Design/GEOM.md → "GEOM cooked file header"`) holds one axis-aligned probe grid.

| Section (kind) | Value | Content |
|---|---|---|
| `LIGHT_SECTION_GIV_INFO` | 130 | 64 bytes: volume GUID (16); grid dimensions 3 × u32 (each 2 … 256, product ≤ 1,048,576); world-space minimum corner 3 × f32; probe spacing 3 × f32 (metres, 0.25 … 16); stored irradiance scale f32; priority u32; blend distance f32 |
| `LIGHT_SECTION_GIV_SH_R`, `_SH_G`, `_SH_B` | 131, 132, 133 | per probe, the L1 irradiance SH of one colour channel as 4 half floats (`c00, c1−1, c10, c11` pre-multiplied by `Â_l`, divided by the stored scale), in x-fastest order; each section is a RGBA16F 3D texture image |
| `LIGHT_SECTION_GIV_VALIDITY` | 134 | per probe, one byte: 255 valid, 0 inside geometry (the bake's back-face ratio above 0.1); an R8_UNORM 3D texture image |

- Loaded as four `Flux_PersistentReadOnly` 3D textures (RGBA16F ×3, R8_UNORM ×1), all formats of `Zenith2/Docs/Rendering.md → "Required format features"`.
- Reader validation as the probe file (`FLUX_LIGHTING_ERROR_CORRUPT_DATA`), fuzz `Zenith_Fuzz_IrradianceVolumeFile`.

## Lightmap set cooked format

Stage 4 format, Stage 5 evaluation. A `.zlmp` file (magic `'ZLMP'`) describes one lightmap set (one per streaming cell or scene) whose pages are GEOM textures.

| Section (kind) | Value | Content |
|---|---|---|
| `LIGHT_SECTION_LMP_INFO` | 140 | 32 bytes: set GUID (16), page count u32 (≤ 256), instance binding count u32, RGBM range f32 (default 16), reserved |
| `LIGHT_SECTION_LMP_PAGES` | 141 | per page 48 bytes: irradiance page texture GUID (16), direction page texture GUID (16), page size u32, per-page irradiance scale f32, reserved 8 |
| `LIGHT_SECTION_LMP_BINDINGS` | 142 | per bound instance 32 bytes: the instance's authoring persistent ID (16), page index u16, flags u16, UV1 scale 2 × f16, UV1 bias 2 × f16 (8 bytes), reserved 4 |

| Page texture | GEOM usage class | Encoding |
|---|---|---|
| irradiance | `TEXTURE_USAGE_LINEAR` (BC7 / ASTC 4×4) | RGBM: RGB = `sqrt(E / (scale × range)) / M`, A = M = `ceil(255 × max(sqrt(E_rgb / (scale × range)))) / 255` (the RGBM of Karis 2009 with a square-root shaper), E the irradiance in lux |
| direction | `TEXTURE_USAGE_LINEAR` | RG = the dominant incident direction, octahedral in [0, 1]; B = directionality `|Σ w_i d_i| / Σ w_i` in [0, 1] |

Pages are ordinary `.ztex` files (`Zenith2/Docs/Design/GEOM/Textures.md → "The cooked texture format"`), streamed by GEOM's texture streamer.

## Bake output contract

WORLD's bake (Z2-I-WORLD-06) writes LIGHT's formats through the `Cook_Write*` functions of ZenithCook (`Zenith2/Tools/ZenithCook/Internal/Lighting/`), never by producing bytes itself.

```cpp
// Zenith2/Tools/ZenithCook/Cook_LightingBake.h  (tool side; never in a Player)
/// @contract value Cook_BakedIrradianceVolume
/// Ownership: Plain value borrowing its spans from WORLD's bake, which owns the arrays until the writer returns.
/// Thread affinity: IMMUTABLE while a writer reads it; the cook worker process's thread.
///   Members non-const: the bake fills it field by field before the writer reads it.
/// Invalidation: The spans are invalid after the writer returns.
/// Units: Metres; lux (irradiance SH already multiplied by the band factors).
/// Errors: None; the writer validates (COOK_ERROR_LIGHTING_BAKE).
/// Version: since Z2 Stage 4. Cook_BakedLightmapPage and Cook_BakedProbe carry this block with their own names
///   (units: lux and unit directions per texel; nits of the prefiltered cube at the stored pre-exposure).
/// @endcontract
struct Cook_BakedIrradianceVolume
{
	Zenith_PersistentID m_xGUID;
	uint32 m_auDims[3];
	Zenith_Vector3 m_xMinCorner;
	Zenith_Vector3 m_xSpacing;
	Zenith_Span<const float> m_afIrradianceSHL1;   // 12 floats per probe: RGB × (c00, c1−1, c10, c11), already × Â_l
	Zenith_Span<const uint8> m_auValidity;        // 1 byte per probe
	uint32 m_uPriority;
	float m_fBlendDistanceMetres;
};

struct Cook_BakedLightmapPage
{
	uint32 m_uSize;                               // a power of two, 64 … 4,096
	Zenith_Span<const float> m_afIrradiance;       // RGB lux per texel
	Zenith_Span<const float> m_afDirection;        // xyz per texel, unit or zero
	Zenith_Span<const float> m_afDirectionality;   // per texel, [0, 1]
	Zenith_Span<const uint8> m_auCoverage;        // 1 inside a chart, 0 outside
};

/// @contract value Cook_LightmapBinding
/// Ownership: Plain value; WORLD's bake owns the array of bindings until Cook_WriteLightmapSet returns.
/// Thread affinity: IMMUTABLE while the writer reads it; the cook worker process's thread.
///   Members non-const: the bake fills each binding field by field before the writer reads it.
/// Invalidation: None (no pointers).
/// Units: UV1 scale and bias in lightmap-page UV per mesh UV1 unit; the writer converts both pairs to the f16 of
///   LIGHT_SECTION_LMP_BINDINGS (round to nearest even) and refuses a value that is not finite as an f16.
/// Errors: None; the writer validates (COOK_ERROR_LIGHTING_BAKE naming the binding index for a page index >= the
///   page count, a non-zero m_uFlags bit outside the defined set, or a non-finite scale or bias).
/// Version: since Z2 Stage 4.
/// @endcontract
struct Cook_LightmapBinding
{
	Zenith_PersistentID m_xInstance;              // the instance's authoring persistent ID (section 142 bytes 0-15)
	uint16 m_uPageIndex;                          // index into the axPages span (bytes 16-17)
	uint16 m_uFlags;                              // bytes 18-19; no flag is defined at Stage 4, so it is 0
	float m_afUV1Scale[2];                         // written as 2 x f16 at bytes 20-23
	float m_afUV1Bias[2];                          // written as 2 x f16 at bytes 24-27; bytes 28-31 reserved, zero
};

struct Cook_BakedProbe
{
	Zenith_PersistentID m_xGUID;                   // the probe asset GUID
	uint32 m_uFaceSize;                           // the platform's cube face size (128 desktop, 64 mobile)
	uint32 m_uMipCount;                           // log2(m_uFaceSize) + 1
	float m_fStoredPreExposure;                    // the capture's fPreExposure (Reflections.md "Probe capture")
	float m_fCaptureEV100;
	Zenith_Vector3 m_xCaptureCentreWS;
	bool m_bInterior;
	Zenith_Span<const float> m_afPrefilteredRGB;   // 6 faces × every mip, layout of LIGHT_SECTION_ENV_CUBE, 3 floats per texel
};

/// @contract functions Cook_LightingBakeWriters
/// Ownership: Pure functions of their inputs writing into the caller's Cook_OutputSink.
/// Thread affinity: ANY_THREAD; one cook worker per invocation.
/// Invalidation: None.
/// Units: As the structs above.
/// Errors: COOK_ERROR_LIGHTING_BAKE for a non-finite input or a grid or page limit beyond the format's (naming the
///   element); the sink's error otherwise. Cook_ProjectSH9 asserts a face size >= 1 and span sizes of 6 x size^2 x 3 and 27.
/// Version: since Z2 Stage 4.
/// @endcontract
Zenith_Result<void, Cook_Error> Cook_WriteIrradianceVolume(const Cook_BakedIrradianceVolume& xVolume, Cook_OutputSink& xSink);
Zenith_Result<void, Cook_Error> Cook_WriteLightmapSet(Zenith_Span<const Cook_BakedLightmapPage> axPages,
	Zenith_Span<const Cook_LightmapBinding> axBindings, Cook_OutputSink& xSink);
Zenith_Result<void, Cook_Error> Cook_WriteReflectionProbe(const Cook_BakedProbe& xProbe, Cook_OutputSink& xSink);
void Cook_ProjectSH9(Zenith_Span<const float> afCubeRGB, uint32 uFaceSize, Zenith_Span<float> afOutSH27);
```

<!-- assumption: pending WORLD, ASSET -->
`Cook_Error`, `Cook_OutputSink` and the cook step interface are ZenithCook's (ASSET); `Cook_LightmapBinding` (declared above) carries the instance persistent ID, page index, flags and UV1 scale and bias, which the writer packs into one 32-byte record of `LIGHT_SECTION_LMP_BINDINGS`; the lightmap tracer and the probe-placement tools are WORLD's. Row L13 of the cross-area notes.
<!-- /assumption -->

| Writer step | Rule |
|---|---|
| dilation | lightmap texels outside every chart take the average of covered neighbours in 8 passes, so bilinear filtering and BC7 blocks never read uncovered texels |
| encoding | RGBM and octahedral direction of "Lightmap set cooked format", then GEOM's texture cook |
| validation | non-finite input values are `COOK_ERROR_LIGHTING_BAKE` naming the element; grid or page limits likewise |
| determinism | the writers are pure functions of their input spans; identical bakes give byte-identical files |

## Irradiance volume sampling

`Flux_SampleIrradianceVolumes(float3 xPositionWS, float3 xNormalWS, float3 xGeometricNormalWS)` in `IndirectDiffuse.slang`, over at most `uFLUX_MAX_GI_VOLUMES_PER_VIEW` = 8 volumes selected per view on the CPU (volumes intersecting the view frustum, priority descending, then nearest), each a `Flux_IrradianceVolumeRecord` of "GI GPU records".

| Step | Rule |
|---|---|
| sample point | `p' = p + 0.3 × spacing × geometric normal` (moves the sample off the surface) |
| cell | the 8 probes of the grid cell containing p' |
| weights | trilinear weights × validity (0 or 1) × a back-face weight `max(0.05, (dot(normalize(probe − p'), n) + 1) / 2)²` |
| normalisation | the weighted SH are divided by the weight sum; a sum below 1e-4 is "no data" |
| irradiance | `E(n) = Σ c_lm Y_lm(n)` for bands 0–1 (the coefficients already include `Â_l`), scaled by the stored scale |
| multiple volumes | in priority order, each weighted by its edge fade `saturate(distanceInside / blendDistance)`, accumulated as in probe blending; the remainder falls through to the next source of "Indirect diffuse selection" |

## Lightmap evaluation

In the colour programs of the forward paths (which have UV1), `Flux_SampleLightmap(Flux_LightmapBinding, float2 xUV1, float3 xNormalWS)`:

| Step | Rule |
|---|---|
| UV | `uv = xUV1 × bindingScale + bindingBias` |
| decode | `E_rgb = (rgbm.rgb × rgbm.a)² × range × pageScale` |
| directional term | `E(n) = E_rgb × (1 − b + 2 b × max(0, dot(n, d)))`, clamped to at least 0, with d the decoded direction and b the directionality (a fully directional texel doubles toward d and is dark away from it; b = 0 is the plain irradiance) |
| sampling | `uFLUX_SAMPLER_LINEAR_CLAMP` (2), explicit gradients from UV1's (`Flux_MaterialInput::m_xUV1Grad`) |

## Lightmap binding

<!-- assumption: pending RPROD, X106 (ruling requested) -->
A lightmap binding is per instance (instances of one mesh have different atlas rectangles), and none of the four GPU-scene tables can hold it: the instance record is fixed at 128 bytes with no free field ([Z2-N-REND-535]) and material slots are allocated per (mesh, material assignment) and shared by instances (`Zenith2/Docs/Design/RHI/Memory.md → "Slot allocators"`). LIGHT proposes a fifth persistent table, the instance lighting table: one 16-byte `[FluxShared] Flux_InstanceLightingRecord` per instance slot (declared in `Zenith2/Shaders/Common/GPUScene.slang` beside the other GPU-scene records, [Z2-N-REND-136]), indexed by the instance index, written through the same scatter upload and imported like the other tables, with initial capacity equal to the instance table's. That extends the table set Z2-D-058 settles, so it is flagged proposal LIGHT-1 and cross-doc item X106 with a ruling requested of the human (Z2-D-022); row L14 of the cross-area notes. Until the ruling, no ticket lands either binding below; the Stage 5 lightmap ticket is blocked by the ruling and implements whichever binding the ruling selects.
<!-- /assumption -->

### Binding if X106 is accepted

<!-- elaboration -->
| Offset | Size | Field | Meaning |
|---|---|---|---|
| 0 | 4 | `u32 m_uLightmapPage` | index into the page-record buffer; `0xFFFFFFFF` none |
| 4 | 4 | `u32 m_uFlags` | bit 0 static-lit (skip irradiance volumes for this instance); bits 1–31 zero |
| 8 | 4 | `uint m_uUV1ScalePacked` | two IEEE half floats (x in bits 0–15, y in bits 16–31), unpacked with `f16tof32`; lightmap UV = UV1 × scale + bias. A `[FluxShared]` struct admits no 16-bit member type (`Zenith2/Docs/Rendering/ShaderSystem.md → "Shared GPU structs"`), hence the packing |
| 12 | 4 | `uint m_uUV1BiasPacked` | the bias, packed the same way |

`static_assert(sizeof(Flux_InstanceLightingRecord) == 16)`; at 262,144 desktop instances the table is 4 MiB, budgeted inside `Z2-B-RPROD-001`. The colour programs read `m_xInstanceLighting[instanceIndex]` beside the instance record. The Stage 5 lightmap ticket's diff then carries the amendments of X106 ([Z2-N-REND-530], [Z2-N-REND-535] to [Z2-N-REND-537] read "five"; Z2-D-058 receives a dated amendment line) once the orchestrator marks X106 applied.

### Binding if X106 is refused

<!-- elaboration -->
Lightmaps then bind through a per-material-slot indirection. RPROD's engine instance extractor (the owner of instance deltas and their material-assignment keys, Q48) gives every lightmapped instance a distinct material assignment key `(mesh, material assignment, lightmap binding index)`, so RHI's slot allocator gives it its own material slot, and a 16-byte LIGHT extension appended to that slot's material constants block carries the same four fields as the table above (page, flags with the static-lit bit, UV1 scale and bias). Costs: one material slot per lightmapped instance, so the material-slot table (65,536 records desktop, 16,384 mobile, [Z2-N-REND-537]) bounds the lightmapped instance count, and instances beyond it fall back to irradiance volumes, counted in `Flux.GI.LightmapSlotOverflow`; instances of one mesh no longer share a material slot when lightmapped. No settled decision changes. Row L32 of the cross-area notes records the GEOM and RHI half of this alternative (the key extension and the constants extension).

### Page records

The page index selects a `[FluxShared] Flux_LightmapPageRecord` (16 bytes, declared in `Zenith2/Shaders/Common/GI.slang`, layout in "GI GPU records") in a persistent buffer of at most 4,096 pages per renderer, written through upload requests as its pages become resident; both bindings use it unchanged.

## Lightmaps on the deferred paths

The G-buffer has no UV1 channel, so on Deferred the G-buffer pass evaluates `Flux_SampleLightmap × diffuseAlbedo × (1 − E_tot(μv)) / π × multiBounceAO` for lightmapped instances and adds it, pre-exposed like every G-buffer emissive value, to GB3's emissive; deferred lighting then skips indirect diffuse for pixels whose static-lit bit it cannot see, which is why the static-lit instances write a GB2 shading-model flag bit (bit 4 of the shading-model byte, `FLUX_SURFACE_MODEL_FLAG_BAKED_DIFFUSE`; GEOM's byte leaves bit 4 free, bits 0–3 being taken per GM9 and X80; assumption pending GEOM and RPROD, row L14, part of X106). The flag bit and its meaning are the same under either binding of "Lightmap binding"; only where the G-buffer pass reads the binding from differs. The visibility-buffer resolve does the same from the reconstructed UV1. The difference from the forward paths (SSAO applied at G-buffer time to baked light is the material AO only) is a per-feature tolerance in the capability matrix.

## Baked GI residency and streaming

| Rule | Value |
|---|---|
| unit | irradiance volumes and lightmap sets load with their streaming cell (STREAM), through the asset loader kinds `Flux.IrradianceVolume` and `Flux.LightmapSet` (assumption pending ASSET, row L11) |
| memory | the pending LIGHT row "Baked GI resident memory" |
| missing data | an instance whose lightmap page is not resident uses the irradiance volumes, then the sky; a volume not resident is skipped |
| transitions | a cell streaming in fades its volumes' weight from 0 to 1 over 30 frames to avoid a visible pop |

## GI GPU records

<!-- elaboration -->
The two baked-GI records the lighting passes read are `[FluxShared]` structs declared in the new module `Zenith2/Shaders/Common/GI.slang` (its row of the shared-struct module table of `Zenith2/Docs/Rendering/ShaderSystem.md → "Shared GPU structs"`, "`GI.slang` | `Flux_IrradianceVolumeRecord`, `Flux_LightmapPageRecord`", is part of X148's payload, `Zenith2/Docs/Design/LIGHT.md → "Rendering table additions pending X103"`); `IndirectDiffuse.slang` imports it. `Flux_InstanceLightingRecord` is declared in `Common/GPUScene.slang` beside the other GPU-scene records ("Binding if X106 is accepted").

| Struct | Offset | Size | Field |
|---|---|---|---|
| `Flux_IrradianceVolumeRecord` (64 B) | 0 | 12 | `float3 m_xMinCorner` (metres) |
| | 12 | 4 | `float m_fBlendDistance` (metres) |
| | 16 | 12 | `float3 m_xSpacing` (metres per probe step) |
| | 28 | 4 | `float m_fIrradianceScale` (the file's stored scale) |
| | 32 | 12 | `uint3 m_xDims` (probes per axis) |
| | 44 | 4 | `uint m_uFadeUnorm` (the streaming fade of "Baked GI residency and streaming" × 65,535 in bits 0–15; bits 16–31 zero) |
| | 48 | 4 | `uint m_uSHRTexture` (bindless index of the red-channel SH 3D texture) |
| | 52 | 4 | `uint m_uSHGTexture` |
| | 56 | 4 | `uint m_uSHBTexture` |
| | 60 | 4 | `uint m_uValidityTexture` |
| `Flux_LightmapPageRecord` (16 B) | 0 | 4 | `uint m_uIrradianceTexture` (bindless index of the page's irradiance texture) |
| | 4 | 4 | `uint m_uDirectionTexture` |
| | 8 | 4 | `float m_fPageScale` |
| | 12 | 4 | `float m_fRGBMRange` |

| Record | Where it lives | How it is written |
|---|---|---|
| `Flux_IrradianceVolumeRecord` | per view, at most `uFLUX_MAX_GI_VOLUMES_PER_VIEW` = 8, in the priority order of "Irradiance volume sampling"; bound as `m_xGIVolumes` | declared at setup and written at order 5 (`Zenith2/Docs/Design/LIGHT.md → "Build-node upload data"`) |
| `Flux_LightmapPageRecord` | one persistent buffer of 4,096 records per renderer, indexed by the page index; bound as `m_xLightmapPages` | a page's record is written through an upload request when the page becomes resident and zeroed when it is evicted (`Zenith2/Docs/Design/RHI/Memory.md → "Upload request state machine"`), never as frame data |

`Test_LightingSharedTypes_Layout` asserts both tables and the 16-byte `Flux_InstanceLightingRecord`.

## SSGI

Screen-space diffuse GI (Stage 5), a new pass `SSGI` (`Zenith2/Shaders/Passes/SSGI/SSGITrace.slang`, `SSGIDenoise.slang`), queue G, after GTAO and contact shadows in every desktop path, enabled per tier by `render.quality.ssgiEnabled`.

<!-- assumption: pending RPROD, X105 -->
SSGI is not in Rendering's frame anatomy; adding the pass, its per-view history `m_axSSGIHistory[2]` (RGBA16F, half render extent) in `Flux_ViewPersistentState` (item (a) of X103) and its GPU row changes `Zenith2/Docs/Rendering/FrameAnatomy.md` and the per-pass tables. That change is cross-doc item X105, made by the Stage 5 RPROD frame-anatomy completion ticket together with the D-TBDR (open, Stage 5) measurement; LIGHT's Stage 5 SSGI ticket is blocked by it and cites X105. Row L15 of the cross-area notes.
<!-- /assumption -->

### SSGI platform scope

Q41 is ruled (a) by the human on 2026-10-16: dynamic GI is SSGI on `QUALITY_TIER_WIN_HIGH` and `QUALITY_TIER_WIN_EPIC`, with the sky-SH recapture and realtime probe recapture on every other tier, so the table below stands and the catalogue records the platform scope as a classification note.

Z2-I-LIGHT-05 "dynamic GI" is required at Stage 5 with no platform qualifier. This design realises it as follows (ruling Q41 (a)):

| Tiers | Dynamic diffuse GI | Reason |
|---|---|---|
| `QUALITY_TIER_WIN_HIGH`, `QUALITY_TIER_WIN_EPIC` | one-bounce SSGI (this section) over the baked or sky source, plus the Stage 4 sky-SH recapture on a time-of-day change and realtime probe recapture | the desktop GPU rows have room for the pass |
| `QUALITY_TIER_WIN_LOW`, `QUALITY_TIER_WIN_DEFAULT`, every `QUALITY_TIER_S24_*` tier | sky-SH recapture and realtime probe recapture only; no screen-space or probe-volume dynamic diffuse GI | the S24 GPU budget has no room for a second GI technique; WIN_LOW and WIN_DEFAULT match the S24 technique so the minimum-specification image uses one technique |

The catalogue records the platform scope as a classification note in `Zenith2/Docs/Requirements/LIGHT.md → "Excluded and deferred"` (Z2-R-LIGHT-117 for SSGI on WIN_HIGH and WIN_EPIC, Z2-R-LIGHT-162 for the other tiers), and Stage 5 SSGI tickets implement this table.

### SSGI algorithm

| Step | Rule |
|---|---|
| resolution | half render extent |
| rays | one cosine-weighted ray per pixel per frame (IGN and R2 noise), traced with the HiZ min-channel walk of `Zenith2/Docs/Design/LIGHT/Reflections.md → "SSR hierarchical trace"` with at most 32 iterations and a thickness of `0.1 m + 0.02 z` |
| hit radiance | last frame's composited colour at the reprojected hit, divided by the previous pre-exposure (so SSGI adds one bounce of on-screen light) |
| miss | the pixel's baked or sky irradiance from "Indirect diffuse selection" in the ray's direction (the volumes' SH evaluated along the ray, or the sky SH) |
| temporal | exponential accumulation in the view's SSGI history (current weight 1/16, reset on disocclusion by the depth and normal test of TAA's reprojection) |
| spatial | a 5 × 5 à-trous depth- and normal-aware filter, 2 iterations |
| output | irradiance E (lux, RGB), replacing the baked source for the pixel where the history's confidence is at least 0.5, blended below that |

### SSGI pass table

<!-- assumption: pending RPROD, X105, X148 -->
The pass table in Rendering's per-pass format (`Zenith2/Docs/Rendering/PerPass/Common.md → "Conventions"`), for the Stage 5 frame-anatomy change of X105; X148 carries it with the rest of `Zenith2/Docs/Design/LIGHT.md → "Rendering table additions pending X103"`.
<!-- /assumption -->

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/SSGI/SSGITrace.slang` → `Flux_SSGITraceParams`; `Zenith2/Shaders/Passes/SSGI/SSGIDenoise.slang` → `Flux_SSGIDenoiseParams` |
| Feature function | `Flux_RGTexture Flux_AddSSGI(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, const Flux_ViewTargets& xTargets)` |
| Kind | compute, 8 × 8 groups over the half render extent: the trace with temporal accumulation (one dispatch), then two à-trous denoise dispatches (step widths 1 and 2) |
| Queue | G |
| Capability | `FLUX_CAPABILITY_TIER_DESKTOP`; runs when the view's tier sets `render.quality.ssgiEnabled` ("SSGI platform scope") |
| Budget row | the pending LIGHT row "SSGI GPU" (producer: the Stage 1 per-system budget ticket for LIGHT) |
| Permutations | `SSGITrace`: none; `SSGIDenoise`: `uPERM_ITERATION` {0, 1} |

| Field (`Flux_SSGITraceParams`) | C++ type | Access | Stages |
|---|---|---|---|
| `m_xDepth`, `m_xNormals`, `m_xVelocity` | `Zenith_RHISRV<Zenith_RHITex2D>` (prepass normals on Forward+, GB1 on the deferred paths) | read | compute |
| `m_xHiZ` | `Zenith_RHISRV<Zenith_RHITex2D>` (final pyramid, min channel) | read | compute |
| `m_xPrevColour` | `Zenith_RHISRV<Zenith_RHITex2D>` `[Optional]` (the view's `m_xSSRPrevColour`; unset: every ray misses) | read | compute |
| `m_xPrevSSGI` | `Zenith_RHISRV<Zenith_RHITex2D>` `[Optional]` (the previous half of `m_axSSGIHistory`) | read | compute |
| `m_xIBLIrradiance` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` (the sky SH) | read | compute |
| `m_xGIVolumes` | `Zenith_RHISRV<Zenith_RHIStructBuffer<Flux_IrradianceVolumeRecord>>` `[Optional]` | read | compute |
| `m_xHistory` | `Zenith_RHIUAV<Zenith_RHITex2D>` `[FullCoverage]` `[WriteOnly]` (the current half of `m_axSSGIHistory`) | write | compute |
| data | `Zenith_RHIFloat4x4 m_xInvViewProjection`, `m_xPrevViewProjection`, `Zenith_RHIFloat2 m_xInvHalfExtent`, `float m_fPrevPreExposure`, `uint32 m_uFrameIndex`, `m_uGIVolumeCount`, `m_uHistoryValid`, `m_uMaxIterations` (32) | — | — |

| Field (`Flux_SSGIDenoiseParams`) | C++ type | Access | Stages |
|---|---|---|---|
| `m_xInput` | `Zenith_RHISRV<Zenith_RHITex2D>` (the history half at iteration 0, the iteration-0 output at 1) | read | compute |
| `m_xDepth`, `m_xNormals` | `Zenith_RHISRV<Zenith_RHITex2D>` | read | compute |
| `m_xOutput` | `Zenith_RHIUAV<Zenith_RHITex2D>` `[FullCoverage]` `[WriteOnly]` (a transient; iteration 1's output is the view's SSGI result) | write | compute |
| data | `Zenith_RHIFloat2 m_xInvHalfExtent`, `uint32 m_uStepWidth` | — | — |

| Output | Format |
|---|---|
| SSGI history (both halves) and the SSGI result | RGBA16F, half render extent: RGB irradiance in lux (not pre-exposed), A confidence in [0, 1] |

The result is bound as `m_xSSGI` in every lighting pass of the view (the Stage 5 rows of the X103 table) and sampled bilinearly at full render extent by `Flux_EvaluateIndirectDiffuse`. With `m_uHistoryValid` 0 the trace takes the no-history branch of the first-write rule (`Zenith2/Docs/Design/LIGHT/Temporal.md → "The first-write rule for histories"`).

## SSGI error budget

Screen-space GI is validated against its own documented error budget, not against path-tracer convergence ([Z2-N-REND-947]).

| Criterion | Budget, measured on the SSGI scenes of `Zenith2/Docs/Design/LIGHT/Tests.md → "Approximation error scenes"` |
|---|---|
| energy | SSGI never adds light that the reference lacks: the mean indirect diffuse over pixels whose indirect light is on screen is at most the path-traced reference mean × 1.05 |
| accuracy | the mean absolute error of indirect diffuse over those pixels is at most 20% of the reference mean |
| stability | the frame-to-frame luminance change of a static scene after 64 frames is at most 2% per pixel at the 99th percentile |
| off-screen | pixels whose indirect light comes from off-screen geometry are excluded from the accuracy criterion and must equal the baked fallback within 5% |

The thresholds are the proposed values of the pending LIGHT row Z2-B-LIGHT-022 "SSGI error budget" (`Zenith2/Docs/Design/LIGHT.md → "Pending row needs"`).

## Indirect diffuse selection

`Flux_EvaluateIndirectDiffuse` fills `Flux_LightingInput::m_xIndirectIrradiance` before `EvaluateLightingEx`:

| Order | Source | Used when |
|---|---|---|
| 1 | SSGI | Stage 5 tiers with `render.quality.ssgiEnabled` ("SSGI platform scope"), the view has an SSGI result, and its confidence ≥ 0.5 (blended below) |
| 2 | lightmap | the instance has a resident lightmap page (forward paths; deferred paths through GB3) |
| 3 | irradiance volumes | the point lies in a resident volume with weight |
| 4 | sky irradiance | always; the remainder of every blend |

Stage 3 has only source 4; each further source is added by its stage's ticket without changing the order.

## Flux_GlobalIllumination header

```cpp
// Zenith2/Engine/ZenithFlux/Flux_GlobalIllumination.h
#pragma once
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithFlux/Flux_LightingErrors.h"
#include "ZenithFlux/Flux_LightSettings.h"
#include "ZenithFlux/Flux_RenderGraph.h"

/// @contract value Flux_SH9
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: projection accumulates the coefficients in place.
/// Invalidation: None.
/// Units: Radiance coefficients (nits) or irradiance (lux), as the producer states.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Flux_SH9 { Zenith_Vector3 m_axCoefficients[9]; };

/// @contract value Flux_SkyIrradianceSH
/// Ownership: Plain value; the GPU copy is a 144-byte buffer bound as m_xIBLIrradiance.
/// Thread affinity: IMMUTABLE.
///   Members non-const: projection accumulates the coefficients in place.
/// Invalidation: None.
/// Units: Radiance SH coefficients of the sky (nits), windowed as "Sky irradiance" states.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Flux_SkyIrradianceSH { Zenith_Vector4 m_axCoefficients[9]; };   // RGB, pad

/// @contract functions Flux_GIFunctions
/// Ownership: Pure CPU functions and feature functions that add passes to the graph they receive.
/// Thread affinity: ANY_THREAD for the SH functions; the Flux.BuildGraph node for the feature functions.
/// Invalidation: None.
/// Units: As above; metres.
/// Errors: Flux_ProjectCubeToSH9 asserts a face size ≥ 1 and a span of 6 × size² × 3 floats; feature functions add no
///   pass when the feature is disabled.
/// Version: since Z2 Stage 3 (SSAO, SH); Z2 Stage 5 (SSGI).
/// @endcontract
Flux_SH9 Flux_ProjectCubeToSH9(Zenith_Span<const float> afCubeRGB, uint32 uFaceSize);
Zenith_Vector3 Flux_EvaluateSH9Irradiance(const Flux_SH9& xRadianceSH, Zenith_Vector3 xNormal);
Flux_RGTexture Flux_AddSSAO(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, const Flux_SSAOSettings& xSettings);
Flux_RGTexture Flux_AddSSGI(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, const Flux_ViewTargets& xTargets);

/// @contract type Flux_BakedGIManager
/// Ownership: Owned by Flux_LightingSystem; owns the resident irradiance volumes, lightmap sets and their page records.
/// Thread affinity: OWNER_THREAD on Flux.BuildGraph; load completions arrive through flux.lighting.loadQueue.
/// Invalidation: Volume and page indices are valid for the frame that selected them.
/// Units: Metres, lux.
/// Errors: Load failures are FLUX_LIGHTING_ERROR_CORRUPT_DATA; a full page table is FLUX_LIGHTING_ERROR_CAPACITY.
/// Version: since Z2 Stage 5.
/// @endcontract
class Flux_BakedGIManager
{
public:
	static Zenith_Result<Zenith_Owned<Flux_BakedGIManager>, Flux_LightingError> Create(const Flux_GISettings& xSettings,
		Flux_PersistentResources& xResources);
	void SelectVolumes(const Flux_ViewList& xViews);
	uint32 GetResidentVolumeCount() const;
	uint32 GetResidentPageCount() const;
};
```

`Flux_AddSSAO` keeps Rendering's signature (`Zenith2/Docs/Rendering/PerPass/Common.md → "GTAO"`) and is implemented in `Zenith2/Engine/ZenithFlux/Internal/Passes/GTAO/`; the baked GI code in `Internal/Lighting/BakedGI/`, SSGI in `Internal/Passes/SSGI/`.
