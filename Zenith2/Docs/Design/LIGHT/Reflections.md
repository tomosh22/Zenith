# LIGHT design: reflections, image-based lighting and probes

Section file of `Zenith2/Docs/Design/LIGHT.md`. It specifies image-based specular lighting, the static environment and the dynamic sky capture, reflection probes (cooked format, capture through capture views, filtering, blending, residency and streaming, the fallback chain) and screen-space reflections (the hierarchical trace on the HiZ min channel, denoise and composite). The SSR pass is Rendering's (`Zenith2/Docs/Rendering/PerPass/Common.md → "SSR"`, budget `Z2-B-LIGHT-007`, queue G); capture views are Rendering's `VIEW_KIND_CAPTURE` (`Zenith2/Docs/Rendering.md → "Render views"`).

## Scope of this file

| Topic | Heading |
|---|---|
| the specular environment term | "IBL evaluation" |
| the cube array that holds every environment | "Environment cube array" |
| the cooked static environment | "Static environment" |
| the dynamic sky environment | "Sky capture", "Sky capture pass table" |
| reflection probes | "Reflection probe record", "Reflection probe cooked format", "Probe capture", "Probe filtering", "Probe filter pass table", "Probe blending", "Probe residency and streaming", "Probe fallback chain" |
| screen-space reflections | "SSR ray generation", "SSR hierarchical trace", "SSR hit validation", "SSR denoise", "SSR composite" |
| the header | "Flux_Reflections header" |

## Environment cube array

Every specular environment the shaders sample lives in one `TextureCubeArray` per renderer, `R11G11B10_FLOAT`, with a full GGX-prefiltered mip chain.

| Property | `FLUX_CAPABILITY_TIER_DESKTOP` | `FLUX_CAPABILITY_TIER_MOBILE` |
|---|---|---|
| face size (mip 0) | 128 | 64 |
| mips | 8 (128 … 1) | 7 (64 … 1) |
| slices | 65: slice 0 plus `uFLUX_MAX_RESIDENT_PROBES_DESKTOP` = 64 probe slices | 17: slice 0 plus `uFLUX_MAX_RESIDENT_PROBES_MOBILE` = 16 |
| slice 0 | the sky environment ("Sky capture") or the static environment ("Static environment") | the same |
| slices 1 … N | resident reflection probes, assigned by "Probe residency and streaming"; `render.quality.probesMaxResident` (at most the capacity above) limits how many are used | |
| memory | 65 × 6 × 128² × 4 B × 4/3 = 34.1 MB | 17 × 6 × 64² × 4 B × 4/3 = 2.2 MB |

| Stage | How shaders reach it |
|---|---|
| Stage 3 | slice 0 alone, cooked: a `Flux_PersistentReadOnly` cube texture, bound through the specified `m_xIBLSpecular` `Zenith_RHIDescriptorHandle` field of the opaque and deferred-lighting passes (`Zenith2/Docs/Rendering/PerPass/ForwardPlus.md → "ForwardPlusOpaque"`) |
| Stage 4 | the whole array is GPU-written (captures and filtering), so it becomes a persistent-imported graph resource (imported every frame, `MarkOutput` on frames that write it) and the consumers' `m_xIBLSpecular` field becomes `Zenith_RHISRV<Zenith_RHITexCubeArray>` |

<!-- assumption: pending RPROD, X103 -->
A GPU-written resource can never be `Flux_PersistentReadOnly` ([Z2-N-REND-547], [Z2-N-REND-550]), so the Stage 4 probe and sky capture changes the type of `m_xIBLSpecular` and `m_xIBLIrradiance` in the parameter tables of `Zenith2/Docs/Rendering/PerPass/ForwardPlus.md → "ForwardPlusOpaque"`, `Zenith2/Docs/Rendering/PerPass/Forward.md → "ForwardOpaquePerObject"`, `Zenith2/Docs/Rendering/PerPass/Common.md → "DeferredLighting"` and `→ "TransparentForward"`, and adds `m_xProbeRecords` (`Zenith_RHISRV<Zenith_RHIStructBuffer<Flux_ReflectionProbeRecord>>`). This is item (i) of cross-doc item X103 (applied by core-doc batch 4); the Stage 4 ticket cites X103 and its field rows in `Zenith2/Docs/Design/LIGHT.md → "Rendering table additions pending X103"`. Row L10 of the cross-area notes.
<!-- /assumption -->

## IBL evaluation

`Zenith2/Shaders/Lighting/IBL.slang`, called by step 5 of `Zenith2/Docs/Design/LIGHT/Lighting.md → "Evaluation order"`.

| Step | Rule |
|---|---|
| reflection vector | `R = reflect(−v, n)`, then the dominant-direction correction of Lagarde and de Rousiers 2014: `R' = normalize(lerp(n, R, (1 − r²)(√(1 − r²) + r²)))`, r = perceptual roughness |
| mip selection | `lod = r × (mipCount − 1)`, matching "Probe filtering" where mip m is prefiltered at roughness `m / (mipCount − 1)` |
| probes | for each probe of the pixel's cluster (or per-object list) in list order: parallax-corrected direction ("Probe blending"), weight `w_i = fade_i × remaining`, `remaining −= w_i`; stop when `remaining ≤ 1/255` |
| sky | the rest: `remaining × slice0(R', lod)` |
| specular | `L_env = Σ w_i L_i × intensity_i` (pre-exposed by the view's `fPreExposure` ÷ the slice's stored pre-exposure), times `E_tot(μv)` and specular occlusion |
| horizon | `L_env *= saturate(1 + 1.3 × dot(R', n_geometric))²` (Jimenez 2016 horizon fade) |
| diffuse | the diffuse environment term is indirect diffuse (`Zenith2/Docs/Design/LIGHT/GI.md → "Indirect diffuse selection"`), never the cube array |

## Static environment

The Stage 3 environment: a cooked `.zenv` file (magic `'ZENV'`), produced by the cook step `Cook_Environment` from either a Radiance RGBE `.hdr` equirectangular image or the sky model of `Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Media model"` evaluated at a given sun direction.

| Section (kind) | Value | Content |
|---|---|---|
| `LIGHT_SECTION_ENV_INFO` | 110 | info record, 32 bytes: face size u32, mip count u32, format u32 (`ZENITH_RHI_FORMAT_R11G11B10_FLOAT`), stored pre-exposure f32, source kind u32 (0 image, 1 sky model), sun direction 3 × f32 when the sky model is baked without the sun disc |
| `LIGHT_SECTION_ENV_CUBE` | 111 | 6 faces × every mip, faces in the order +X, −X, +Y, −Y, +Z, −Z within a mip, mips largest first, raw R11G11B10F texels |
| `LIGHT_SECTION_ENV_SH` | 112 | the sky radiance SH (9 × RGB f32, 108 bytes, `Zenith2/Docs/Design/LIGHT/GI.md → "Sky irradiance"`) |

The file uses the GEOM cooked file header (`Zenith2/Docs/Design/GEOM.md → "GEOM cooked file header"`) with every section described by COMPRESS's descriptor (`Zenith2/Docs/Design/COMPRESS.md → "Compressed section descriptor on disk"`), zstd level 19 for the cube, NONE for the info and SH sections. The engine's default environment `Zenith2/Engine/ZenithFlux/Internal/Content/Lighting/DefaultEnvironment.env.json` (the sky model at a 45° sun) is loaded at renderer startup, so slice 0 is never empty.

| RGBE reader rule | Value |
|---|---|
| header | `#?RADIANCE` or `#?RGBE`, `FORMAT=32-bit_rle_rgbe`, a `-Y h +X w` resolution line |
| limits | width ≤ 16,384, height ≤ 8,192, `w = 2h` (equirectangular), otherwise `COOK_ERROR_ENVIRONMENT_FORMAT` |
| decoding | new-style RLE scanlines only; a malformed run is `COOK_ERROR_ENVIRONMENT_FORMAT` naming the scanline |
| fuzz | `Zenith_Fuzz_RGBEReader` (`Zenith2/Tools/ZenithCook/Tests/Fuzz/RGBEReader.cpp`) |

## Sky capture

Stage 4; the pass table is "Sky capture pass table". When ENV's atmosphere or time of day changes (the sun direction differs by more than 0.5° from the last captured one, `m_uSkyRevision` of the atmosphere block changed, or ENV's cloud coverage × density changed by more than 0.05 since the last capture), LIGHT recaptures slice 0:

| Step | Rule |
|---|---|
| 1 | `Flux_AddSkyCapture` renders one face per frame (round robin +X … −Z) of a 128² (mobile 64²) cube with the composite of `Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Sky draw and aerial perspective"` (the sky-view LUT, the star field, ENV's cloud layer when present) without the sun disc (the sun is the analytic directional light) and without aerial perspective, and with the ENV ground albedo below the horizon as a Lambertian virtual ground lit by the sun and the sky SH of the previous capture |
| 2 | faces render into the capture half of a two-cube ping-pong (a persistent-imported cube array of 2 slices); the shaders keep sampling the other half |
| 3 | after the sixth face, "Probe filtering" prefilters the captured cube into slice 0 of the environment cube array and projects the sky SH ("Sky irradiance") |
| 4 | the change becomes visible in the frame after the filter: at most 7 submitted frames after the sky changed |
| 5 | the face round robin, the captured-sky key (the sun direction, `m_uSkyRevision` and the cloud term the capture was made for) and the ping-pong half advance only when the frame submitted: a deferred or failed frame's face is captured again in the next built frame (`Zenith2/Docs/Design/LIGHT.md → "Deferred and failed frames"`) |

### Sky capture pass table

<!-- assumption: pending RPROD, X148 -->
The pass table in Rendering's per-pass format, part of X148's payload; it joins Rendering's shared late passes as `SkyCapture`.
<!-- /assumption -->

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/Sky/SkyCapture.slang` → `Flux_SkyCaptureParams` (one face); `Zenith2/Shaders/Passes/Probes/SHProject.slang` → `Flux_SHProjectParams` (the sky SH, after the filter) |
| Feature function | `void Flux_AddSkyCapture(Flux_RenderGraph& xGraph, const Flux_RenderLightingEnvironment& xEnvironment, Flux_FrameShared& xShared, uint32 uFace, Flux_RGTexture& xCaptureCube)`; the SH projection is added by `Flux_ReflectionProbeManager::AddPasses` in the frame after the sixth face, after `Flux_AddProbeFilter` for slice 0 |
| Kind | compute: `SkyCapture` 8 × 8 groups over one face; `SHProject` one group of 256 threads with the fixed-order reduction tree of `Zenith2/Docs/Design/LIGHT/GI.md → "Sky irradiance"` |
| Queue | G |
| Capability | all tiers (face size 128 desktop, 64 mobile) |
| Budget row | `Z2-B-ENV-001` Sky and atmosphere GPU (the sky passes' row) and the pending LIGHT row "Probe capture and filter GPU" for the filter |
| Permutations | `SkyCapture`: `bPERM_STARS` {false, true}, `bPERM_CLOUDS` {false, true}; `SHProject`: none |

| Field (`Flux_SkyCaptureParams`) | C++ type | Access | Stages |
|---|---|---|---|
| `m_xSkyViewLUT`, `m_xTransmittanceLUT`, `m_xMultiScatterLUT` | `Zenith_RHISRV<Zenith_RHITex2D>` | read | compute |
| `m_xStarField` | `Zenith_RHIDescriptorHandle` (persistent read-only cube; invalid without stars) | read | compute |
| `m_xCloudLayer`, `m_xCloudTransmittance` | `Zenith_RHISRV<Zenith_RHITex2D>` `[Optional]` | read | compute |
| `m_xPrevSkySH` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` (`m_xIBLIrradiance`: the previous capture's SH for the virtual ground) | read | compute |
| `m_xCaptureFace` | `Zenith_RHIUAV<Zenith_RHITex2DArray>` (mip 0 of the persistent-imported ping-pong pair, all 12 layers of that mip as [Z2-N-REND-051] requires; a plain write with neither `[FullCoverage]` nor `[WriteOnly]`, so the other 11 layers, including the faces already captured, are preserved) | write | compute |
| data | `uint32 m_uFace`, `m_uCaptureHalf` (the ping-pong half being captured, 0 or 1), `m_uFaceSize`, `Flux_AtmosphereGPU m_xAtmosphere`, `Zenith_RHIFloat3 m_xSunDirectionWS`, `m_xSunIlluminance`, `m_xMoonDirectionWS`, `m_xMoonIlluminance`, `m_xGroundAlbedo`, `m_xFallbackSkyRadiance`, `Zenith_RHIFloat4 m_xStarRotation`, `float m_fStarIntensity`, `m_fPreExposure` (the capture's fixed pre-exposure), `m_fCloudPreExposureRatio` | — | — |

| Field (`Flux_SHProjectParams`) | C++ type | Access | Stages |
|---|---|---|---|
| `m_xSource` | `Zenith_RHISRV<Zenith_RHITexCube>` (the captured cube, mip 0) | read | compute |
| `m_xSH` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` `[FullCoverage]` (`m_xIBLIrradiance`, 144 bytes of `Flux_SkyIrradianceSH`) | write | compute |
| data | `uint32 m_uFaceSize`, `float m_fInvPreExposure` | — | — |

| Output | Format |
|---|---|
| a capture face | R11G11B10F, the face size, pre-exposed by `m_fPreExposure` |
| the sky SH | 9 × `float4` (RGB, pad), radiance in nits (not pre-exposed), windowed by the consumer |

## Reflection probe record

`Flux_ReflectionProbeRecord`, 64 bytes, a `[FluxShared]` struct in the module `Zenith2/Shaders/Common/Probes.slang` (the module-table row is row L10, item (i) of X103). The struct is declared from Stage 2 by the shading-module stub slice, because `Flux_LightSource::GetProbe` returns it; its producer and every reader of its values are Stage 4.

| Offset | Size | Field | Meaning |
|---|---|---|---|
| 0 | 12 | `m_xCentreWS` | capture position, metres |
| 12 | 4 | `m_uCubeSlice` | slice of the environment cube array |
| 16 | 12 | `m_xInfluenceHalfExtent` | axis-aligned influence box half extent about the influence centre, metres |
| 28 | 4 | `m_fFadeDistance` | metres over which the weight falls from 1 to 0 inside the influence box |
| 32 | 12 | `m_xParallaxHalfExtent` | axis-aligned parallax box half extent about the capture centre, metres; zero disables parallax correction |
| 44 | 4 | `m_fIntensity` | `artistIntensity / storedPreExposure` |
| 48 | 12 | `m_xInfluenceCentreWS` | the influence box centre |
| 60 | 4 | `m_uFlags` | bits 0–7 priority (higher first), bit 8 box parallax, bit 9 exterior-only (skipped when the camera is inside an interior probe), bits 16–31 reserved |

Records are rebuilt every frame from the resident probes, sorted by the blending key of "Probe blending" (so ascending record index is blending order), and uploaded as frame data: `Flux_ReflectionProbeManager::AddPasses` declares `64 × n` bytes with `DeclareFrameData` at setup and `WriteFrameData` writes them at order 5 (`Zenith2/Docs/Design/LIGHT.md → "Build-node upload data"`), bound as `Flux_FrameShared::m_xProbeRecords` (row L10). At most 64 records (desktop, 16 mobile) make 4 KiB, which is cheaper than tracking record slots across frames. The frame probe list is built by the light and decal list pass's third group (probes behave like decals in `Zenith2/Docs/Design/LIGHT/Lighting.md → "Light and decal list build"`), and probes enter the clusters as `Zenith2/Docs/Design/LIGHT/Lighting.md → "Light clustering"` states.

## Reflection probe cooked format

A baked probe (WORLD's bake output, `Zenith2/Docs/Design/LIGHT/GI.md → "Bake output contract"`) is a `.zprb` file (magic `'ZPRB'`, the GEOM cooked file header):

| Section (kind) | Value | Content |
|---|---|---|
| `LIGHT_SECTION_PROBE_INFO` | 120 | the 64-byte capture record of "Probe info record" |
| `LIGHT_SECTION_PROBE_CUBE` | 121 | the prefiltered cube, layout as `LIGHT_SECTION_ENV_CUBE` |

### Probe info record

<!-- elaboration -->
The file carries only what the bake produced; the influence box, fade, parallax box, priority and intensity are the component's and arrive through `Flux_RenderProbeDelta` every time they change, so moving an influence box never needs a re-bake.

| Offset | Size | Field | Meaning |
|---|---|---|---|
| 0 | 16 | probe GUID | the asset's `Zenith_PersistentID` |
| 16 | 4 | `u32` face size | must equal the platform's cube face size (128 desktop, 64 mobile); the cook resamples otherwise |
| 20 | 4 | `u32` mip count | `log2(face size) + 1` |
| 24 | 4 | `f32` stored pre-exposure | the capture's `fPreExposure` |
| 28 | 4 | `f32` capture EV100 | the capture exposure the bake used |
| 32 | 12 | `3 × f32` capture centre | the world position the cube was captured at, metres |
| 44 | 4 | `u32` flags | bit 0 interior; bits 1–31 zero |
| 48 | 16 | reserved | zero; a reader refuses non-zero reserved bytes |
| 64 | — | end | `Test_ProbeFile_RoundTrip` asserts the section's size is exactly 64 bytes and every offset |

For a baked probe, the record's `m_xCentreWS` is the file's capture centre (the cube was captured there, so parallax correction must use it); when it differs from the delta's `m_xCaptureCentreWS` by more than 0.01 m the manager logs the `flux.probes` warning `probe <guid>: moved since bake` once and keeps the file's value.

- The face size of a cooked probe must equal the platform's cube face size; the cook resamples otherwise.
- The reader validates the header, section sizes (exactly `6 × Σ mip area × 4` bytes), non-finite floats (refused), and every size against the parser limits ([Z2-N-ARCH-953]); a failure is `FLUX_LIGHTING_ERROR_CORRUPT_DATA`.
- Fuzz: `Zenith_Fuzz_ProbeFile` (`Zenith2/Engine/ZenithFlux/Tests/Fuzz/ProbeFile.cpp`).

## Probe capture

A probe marked "realtime" (its component's `captureMode`, see `Zenith2/Docs/Design/LIGHT.md → "Light component schemas"`) or a baked probe whose `.zprb` is missing is captured at runtime through six `VIEW_KIND_CAPTURE` views (`Flux_MakeViewID(VIEW_KIND_CAPTURE, probeID, face)`).

| Rule | Value |
|---|---|
| path | Forward+ on every platform until the Forward path lands in Stage 5 (the interim rule of `Flux_SelectShadingPath`, orchestrator ruling Q83 (a), now the stage note of [Z2-N-REND-691] through cross-doc item X266, applied by core-doc patch g), then Forward on desktop and Forward+ on mobile ([Z2-N-REND-691]) |
| feature mask | Rendering's capture default: HiZ, shadow receive, SSAO, fog, sky, transparent, particles, colour; no TAA, no post ([Z2-N-REND-601]) |
| extent | the platform face size, 90° field of view, near `render.quality.probesCaptureNearMetres` (0.1 m), far the probe's `captureFarMetres` (default 500 m); the depth mode follows the precision check |
| exposure | fixed: `fPreExposure = 1 / (1.2 × 2^EV)` with EV the probe's `captureEV100` (default 12 outdoors, 6 for probes flagged interior); the value is stored as the slice's pre-exposure |
| scheduling | Rendering's round-robin ([Z2-N-REND-609]); each probe's six faces are six requests with the probe's update interval (`updateIntervalFrames`, default 0 = once after load, then only on request); a face counts as captured only when its frame submitted, so a deferred or failed frame's face is requested again |
| order | a probe becomes usable after its six faces and its filter pass completed; until then the fallback chain applies |
| cost cap | the per-frame capture count is `Z2-B-RPROD-023`'s tier value; the GPU time is the pending LIGHT row Z2-B-LIGHT-017 "Probe capture and filter GPU" (`Zenith2/Docs/Design/LIGHT.md → "Pending row needs"`) |

The captured face texture is a transient; the filter pass reads it.

## Probe filtering

`Flux_AddProbeFilter` (Stage 4; also used by "Sky capture"; the cook runs the same algorithm on the CPU), program `Zenith2/Shaders/Passes/Probes/ProbeFilter.slang`, queue G; the pass table is "Probe filter pass table".

| Step | Rule |
|---|---|
| 1 | downsample the captured cube into a temporary mip chain by 2 × 2 box filtering |
| 2 | mip 0 of the output copies the source mip 0 (roughness 0) |
| 3 | each mip m ≥ 1 is prefiltered at roughness `r_m = m / (mipCount − 1)` by GGX importance sampling with filtered importance sampling (Křivánek and Colbert 2008): 32 samples (desktop) or 16 (mobile) of the VNDF around N = V = R, each read from the source mip `0.5 × log2(Ω_s / Ω_p) + 1`, with `Ω_s = 1 / (N × pdf)` and `Ω_p = 4π / (6 × size²)` |
| 4 | the sample set is a fixed Hammersley sequence, identical on every backend |
| 5 | the result is written into the probe's slice of the environment cube array |

The CPU cook implements the same filter in `double` (`Cook_ProbeFilter`), and `Test_ProbeFilter_GPUMatchesCook` compares the two on a fixture cube within 2 half ULPs per channel after conversion.

### Probe filter pass table

<!-- assumption: pending RPROD, X148 -->
The pass table in Rendering's per-pass format (`Zenith2/Docs/Rendering/PerPass/Common.md → "Conventions"`), part of X148's payload (`Zenith2/Docs/Design/LIGHT.md → "Rendering table additions pending X103"`); it joins Rendering's shared late passes as `ProbeFilter`.
<!-- /assumption -->

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/Probes/ProbeFilter.slang` → `Flux_ProbeFilterParams` |
| Feature function | `void Flux_AddProbeFilter(Flux_RenderGraph& xGraph, Flux_RGTexture xSourceCube, Flux_RGTexture& xCubeArray, uint32 uSlice, uint32 uSampleCount)` |
| Kind | compute, 8 × 8 × 1 groups with one z per cube face: `downsample` once per mip of the temporary chain (steps 1), `copy` once (step 2), `prefilter` once per output mip m ≥ 1 (step 3) |
| Queue | G |
| Capability | all tiers (face size 128 desktop, 64 mobile) |
| Budget row | the pending LIGHT row "Probe capture and filter GPU" (producer: the Stage 1 per-system budget ticket for LIGHT) |
| Permutations | `uPERM_STEP` {downsample, copy, prefilter}, `uPERM_SAMPLES` {16, 32} (`render.quality.probesFilterSamples`) |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_xSource` | `Zenith_RHISRV<Zenith_RHITexCube>` (the captured cube: mip n − 1 of its temporary chain for `downsample`, the whole chain for `copy` and `prefilter`) | read | compute |
| `m_xChainMip` | `Zenith_RHIUAV<Zenith_RHITex2DArray>` `[FullCoverage]` (mip n of the temporary chain, 6 layers; step `downsample` only) | write | compute |
| `m_xDestination` | `Zenith_RHIUAV<Zenith_RHITex2DArray>` (mip m of the persistent-imported environment cube array, all its layers as [Z2-N-REND-051] requires; a plain write with neither `[FullCoverage]` nor `[WriteOnly]`, so every other resident slice keeps its contents; steps `copy` and `prefilter`) | write | compute |
| data | `uint32 m_uSlice`, `m_uMip`, `m_uMipCount`, `m_uFaceSize`, `m_uSampleCount`, `float m_fRoughness` (`m / (mipCount − 1)`), `float m_fSourceTexelSolidAngle` (Ω_p) | — | — |

| Output | Format |
|---|---|
| the filtered probe or sky slice, every mip | R11G11B10F (the environment cube array's format) |

The temporary chain is a transient cube of the source's face size with a full mip chain; the destination is the persistent-imported environment cube array, marked output in the frame ([Z2-N-REND-804]). Which layers a step writes is the shader's indexing, never the view: thread group z (the cube face, 0 to 5) writes layer `6 × m_uSlice + z` of `m_xDestination`, and `SkyCapture` writes layer `6 × m_uCaptureHalf + m_uFace` of `m_xCaptureFace`; `m_xChainMip` is a whole 6-layer transient mip and keeps `[FullCoverage]`.

## Probe blending

| Rule | Value |
|---|---|
| influence weight | `fade_i = saturate(min over axes of (halfExtent − |p − centre|) / fadeDistance)` |
| order | cluster or per-object order: priority descending, then influence volume ascending (smaller first), then record index; the frame probe list is sorted on the CPU by that key before upload, and the clusters keep ascending list order |
| parallax | box projection (Lagarde and Zanuttini 2012): intersect the ray `p + t R'` with the parallax box, sample the direction from the capture centre to the hit |
| blending | step "probes" of "IBL evaluation"; at most the cluster layout's `m_uMaxProbes` (8 desktop, 4 mobile, `Zenith2/Docs/Design/LIGHT/Lighting.md → "Cluster grid"`) probes per pixel |
| exterior-only | probes with bit 9 are skipped while the main camera is inside any interior probe's influence box |

## Probe residency and streaming

| Rule | Value |
|---|---|
| residency | at most N slices (the cube array's probe capacity); a probe is resident when a slice holds its filtered cube |
| selection | every frame, the probes whose influence boxes intersect the view frusta, scored by `priority × 1000 + projected screen coverage`, highest first; the top N keep or get slices, ties by persistent ID |
| baked probes | loaded through the asset loader (kind `Flux.Probe`, assumption pending ASSET, row L11) when streamed in with their cell; the cube section is copied into the array slice by a `StreamingUpload` copy |
| eviction | a probe outside the selection for 120 frames frees its slice (deferred deletion is not needed: the slice stays allocated, only its assignment changes) |
| streaming cancellation | a load cancelled before its upload leaves no slice assigned; one cancelled after the upload releases the slice at the next selection |

## Probe fallback chain

For a pixel, the first available source wins:

| Order | Source |
|---|---|
| 1 | SSR where it hits with confidence ("SSR composite") |
| 2 | the blended resident probes of the pixel |
| 3 | slice 0: the captured sky (Stage 4) |
| 4 | slice 0: the cooked static environment of the world's lighting component |
| 5 | slice 0: the engine default environment, which always exists |

A probe that is not yet captured, failed to load, or was evicted contributes nothing and its weight falls to the next source; `Flux.Probes.Missing` counts probes that were selected but not resident.

## SSR ray generation

The SSR program's first dispatch (`SSRTrace.slang`), one thread per trace pixel (full render extent, or half with `bPERM_HALF_RESOLUTION`, which mobile tiers always use).

| Step | Rule |
|---|---|
| gate | pixels with roughness above `Flux_SSRSettings::m_fMaxRoughness` (default 0.6) or depth at the far clear value write a miss (confidence 0) |
| ray | the reflection of the view ray about a GGX half vector sampled with the VNDF method (Heitz 2018) at the pixel's roughness; the random numbers are `IGN(pixel + frameIndex × 5.588238)` and its R2 partner, so TAA integrates the noise |
| start | the position reconstructed from depth, offset by `0.01 × linearDepth` along the geometric normal (the prepass normal) |
| screen-space ray | the ray is transformed to (screen UV, linear depth) at its start and at its clip-space end (the intersection with the near plane or a length of `m_fMaxDistanceMetres`, default 200 m), so linear depth varies hyperbolically along the screen segment and is interpolated in 1/z |

## SSR hierarchical trace

The trace walks the final HiZ pyramid's min channel (G channel, `Zenith2/Docs/Rendering/Culling.md → "Pyramid format"`, [Z2-N-REND-631]).

| Step | Rule |
|---|---|
| start | mip 0; the ray's current cell at the current mip |
| iteration | advance the ray to the boundary of its current cell at the current mip; read the cell's minimum linear depth |
| in front | if the ray's linear depth at the far end of the step is less than the cell's minimum (the ray passes in front of every sample in the cell): accept the step and ascend one mip (at most `m_uMaxMip`, default 7) |
| behind | otherwise do not advance; descend one mip; at mip 0 the ray has reached the surface: a candidate hit |
| termination | a candidate hit, the ray leaving the screen or the view's depth range, or `m_uMaxIterations` (tier: 32, 64 or 96) |
| thickness | a candidate hit is accepted when `rayDepth − cellMinDepth < thickness`, `thickness = 0.25 m + 0.02 × cellMinDepth`; otherwise the ray continues at mip 0 from behind the surface for at most 4 iterations, then misses |

Stepping on the min channel never passes a ray through an occluder, because a ray may skip a footprint only when it is in front of that footprint's nearest depth; this is why Rendering keeps the min channel.

## SSR hit validation

| Check | Result |
|---|---|
| back face | the prepass (or G-buffer) normal at the hit faces away from the ray (`dot(n_hit, R) > 0`): miss |
| screen border | confidence × `saturate(min(u, 1 − u, v, 1 − v) / 0.1)` |
| roughness fade | confidence × `saturate((m_fMaxRoughness − r) / 0.1)` |
| distance | confidence × `1 − saturate((t − 0.8 × maxDistance) / (0.2 × maxDistance))` |
| radiance | the view's `m_xSSRPrevColour` (last frame's composited colour) at the hit's UV reprojected by the velocity at the hit: `uvPrev = uvHit − velocity(uvHit)`; a reprojected UV off screen is a miss |
| pre-exposure | the sampled radiance × `fPreExposure(current) / fPreExposure(previous)` |
| output | `m_xHits`: RGB radiance, A confidence |

## SSR denoise

`SSRDenoise.slang`, one dispatch: a 3 × 3 (half resolution: 4 × 4, including the upsample) edge-aware ratio estimator (Stachowiak 2015): each neighbour's radiance is weighted by its confidence, the BRDF ratio `pdf_neighbour / pdf_centre` of its ray under the centre pixel's roughness and normal, a depth weight `exp(−|Δz| / (0.1 × z))` and a normal weight `max(0, n·n')^32`; the output confidence is the weighted mean confidence. Temporal stability comes from TAA; SSR keeps no history of its own.

## SSR composite

`SSRComposite.slang` blends the denoised reflection into `m_xSceneColour` (read-write, `Zenith2/Docs/Rendering/PerPass/Common.md → "SSR"`):

`colour += c × w × (L_ssr − L_env)`

| Symbol | Source |
|---|---|
| c | the denoised confidence |
| w | the environment specular weight `E_tot(μv) × specularOcclusion`: on Forward+ and Forward the opaque pass's second colour target `m_xSpecularWeight` (R11G11B10F, present only when the view's `FLUX_VIEW_FEATURE_SSR` bit is set), on Deferred and the visibility buffer recomputed from the G-buffer's F0 and roughness |
| L_env | the probe and sky specular the opaque pass already added: recomputed in the composite from the normal, roughness and position with the same `Flux_EvaluateIBLSpecular` (so the subtraction removes exactly what was added), reading the composite's `m_xIBLSpecular`, `m_xProbeRecords` and `m_xClusterLists` fields (item (h) of X103) |
| L_ssr | the denoised radiance |

After the composite, a copy of the scene colour becomes the view's next `m_xSSRPrevColour` ([Z2-N-REND-804]).

<!-- assumption: pending RPROD, X103 -->
Rendering's forward opaque passes write one colour target; the composite needs the per-pixel specular reflectance, which the Forward+ prepass (normal and roughness only) cannot provide. LIGHT adds a second colour target `m_xSpecularWeight`, `R11G11B10_FLOAT` at render extent, `CLEAR` to zero then `STORE`, written from `Flux_LightingOutput::m_xEnvSpecularWeight` and read by the composite. It is attached only when the view's `FLUX_VIEW_FEATURE_SSR` bit is set: the forward opaque programs of ForwardPlusOpaque and ForwardOpaquePerObject gain the permutation `bPERM_SSR_WEIGHT` {false, true}; with `false` the program declares one colour output and the target is not declared in the graph, so no tier without SSR (`QUALITY_TIER_WIN_LOW`, `QUALITY_TIER_S24_LOW`, `QUALITY_TIER_S24_DEFAULT`) writes or stores it ([Z2-N-REND-919], [Z2-N-REND-921]). `Flux_ViewTargets` gains `m_xSpecularWeight` (unset without SSR). This is item (h) of cross-doc item X103 (applied by core-doc batch 4); row L12 of the cross-area notes.

On the S24 the one SSR-on tier is `QUALITY_TIER_S24_HIGH`: the target costs 4 bytes per render pixel written and stored per frame (at a 2,340 × 1,080 render extent, 10.1 MB of tile store per frame), a cost the Stage 1 per-system budget ticket for LIGHT allocates inside the `QUALITY_TIER_S24_HIGH` value of `Z2-B-LIGHT-007` (the SSR row carries its target's write as well as the trace, denoise and composite; pending row need of `Zenith2/Docs/Design/LIGHT.md → "Pending row needs"`).
<!-- /assumption -->

## Flux_Reflections header

```cpp
// Zenith2/Engine/ZenithFlux/Flux_Reflections.h
#pragma once
#include "ZenithCore/Zenith_PersistentID.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithFlux/Flux_LightingErrors.h"
#include "ZenithFlux/Flux_LightSettings.h"
#include "ZenithFlux/Flux_Lights.h"                 // Flux_LightDeltaOp
#include "ZenithFlux/Flux_PersistentResources.h"
#include "ZenithFlux/Flux_RenderGraph.h"

/// @contract value Flux_RenderProbeKey
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: it is copy-assigned into descriptors, records and tables.
/// Invalidation: None.
/// Units: None; the 64-bit key Flux_MakeProbeKey derives.
/// Errors: None.
/// Version: since Z2 Stage 4. Never serialized; recomputed by the extractor every frame.
/// @endcontract
struct Flux_RenderProbeKey { uint64 m_ulValue = 0; };

/// @contract enum Flux_ProbeCaptureMode
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; a value outside the enumeration in a probe delta is FLUX_LIGHTING_ERROR_INVALID_PROBE.
/// Version: since Z2 Stage 4. The values equal the component's CaptureMode property (1 baked, 2 realtime).
/// @endcontract
enum Flux_ProbeCaptureMode : uint8 { FLUX_PROBE_CAPTURE_BAKED = 1, FLUX_PROBE_CAPTURE_REALTIME };

/// @contract functions Flux_ProbeKeys
/// Ownership: Pure function.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
/// The engine probe extractor's key for a Zenith.Lighting.ReflectionProbe component: Zenith_Hash64 of the 16 bytes of
/// the entity's Zenith_PersistentID with seed uWorldTag, bit 63 cleared: the derivation of Flux_MakeEntityLightKey.
/// Probe keys and light keys never meet (different deltas, different tables), so equal values are harmless.
Flux_RenderProbeKey Flux_MakeProbeKey(const Zenith_PersistentID& xEntity, uint32 uWorldTag);

/// @contract value Flux_RenderProbeDelta
/// Ownership: Written by the engine probe extractor into render-world storage.
/// Thread affinity: IMMUTABLE after Render.Seal.
///   Members non-const: the extractor fills each record field by field in the render world before Render.Seal.
/// Invalidation: Invalid after the render-world slot resets.
/// Units: Metres; EV100; frames.
/// Errors: Validated by Flux_ReflectionProbeManager::ApplyDeltas (FLUX_LIGHTING_ERROR_INVALID_PROBE); never asserted.
/// Version: since Z2 Stage 4. A render-world value; never serialized.
/// @endcontract
struct Flux_RenderProbeDelta
{
	Flux_RenderProbeKey m_xKey;
	Flux_LightDeltaOp m_eOp;                      // FLUX_LIGHT_DELTA_OP_ADD, _UPDATE, _REMOVE
	Flux_ProbeCaptureMode m_eCaptureMode;
	uint8 m_uPriority;
	uint8 m_uFlags;                              // bit 0 box parallax, bit 1 exterior-only, bit 2 interior
	Zenith_PersistentID m_xBakedAsset;            // the .zprb asset, null for realtime
	Zenith_Vector3 m_xCaptureCentreWS;
	float m_fCaptureFarMetres;
	Zenith_Vector3 m_xInfluenceCentreWS;
	float m_fFadeDistanceMetres;
	Zenith_Vector3 m_xInfluenceHalfExtent;
	float m_fIntensity;
	Zenith_Vector3 m_xParallaxHalfExtent;
	float m_fCaptureEV100;
	uint32 m_uUpdateIntervalFrames;
	uint32 m_uCaptureRequestSerial;              // incremented by the component to request one recapture
};

/// @contract type Flux_ReflectionProbeManager
/// Ownership: Owned by Flux_LightingSystem; owns the environment cube array, the probe record buffer and the slot map.
/// Thread affinity: OWNER_THREAD on Flux.BuildGraph; asset-load completions arrive through the bounded queue
///   flux.lighting.loadQueue.
/// Invalidation: Slice indices are valid for the frame that assigned them.
/// Units: Metres, frames.
/// Errors: ApplyDeltas reports FLUX_LIGHTING_ERROR_INVALID_PROBE per refused delta; load failures are
///   FLUX_LIGHTING_ERROR_CORRUPT_DATA and fall back ("Probe fallback chain").
/// Version: since Z2 Stage 4.
/// @endcontract
class Flux_ReflectionProbeManager
{
public:
	static Zenith_Result<Zenith_Owned<Flux_ReflectionProbeManager>, Flux_LightingError> Create(
		const Flux_ProbeSettings& xSettings, Flux_PersistentResources& xResources);
	void ApplyDeltas(Zenith_Span<const Flux_RenderProbeDelta> axDeltas);
	void SelectResident(const Flux_ViewList& xViews);
	void AddPasses(Flux_RenderGraph& xGraph, const Flux_ViewList& xViews, Flux_FrameShared& xShared);   // captures, filters, uploads
	uint32 GetResidentCount() const;
	uint32 GetMissingCount() const;
};

/// @contract functions Flux_ReflectionFunctions
/// Ownership: Feature functions that add passes to the graph they receive.
/// Thread affinity: The Flux.BuildGraph node.
/// Invalidation: Returned handles are valid for the current graph only.
/// Units: Pre-exposed radiance.
/// Errors: No pass is added when the view's feature bit is clear.
/// Version: since Z2 Stage 3 (SSR); Z2 Stage 4 (captures and filter).
/// @endcontract
Flux_RGTexture Flux_AddSSR(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_ViewTargets& xTargets);
Flux_RGTexture Flux_AddProbeCaptureFace(Flux_RenderGraph& xGraph, const Flux_ViewContext& xCaptureView,
	Flux_ViewTargets& xTargets);                               // builds one capture face through Flux_BuildView
void Flux_AddProbeFilter(Flux_RenderGraph& xGraph, Flux_RGTexture xSourceCube, Flux_RGTexture& xCubeArray, uint32 uSlice,
	uint32 uSampleCount);
void Flux_AddSkyCapture(Flux_RenderGraph& xGraph, const Flux_RenderLightingEnvironment& xEnvironment, Flux_FrameShared& xShared,
	uint32 uFace, Flux_RGTexture& xCaptureCube);

inline constexpr uint32 uFLUX_MAX_RESIDENT_PROBES_DESKTOP = 64;
inline constexpr uint32 uFLUX_MAX_RESIDENT_PROBES_MOBILE = 16;
```

`Flux_AddSSR` keeps Rendering's signature and is implemented in `Zenith2/Engine/ZenithFlux/Internal/Passes/SSR/`; the probe code lives in `Internal/Lighting/Probes/` and `Internal/Passes/Probes/`.
