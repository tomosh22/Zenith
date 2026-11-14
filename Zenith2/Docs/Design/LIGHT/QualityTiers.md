# LIGHT design: quality tiers and QualityMatrix rows

Section file of `Zenith2/Docs/Design/LIGHT.md`. It gives, per LIGHT feature, the technique and parameters of every quality tier on Windows and on the S24, the fallback and its trigger, and the QualityMatrix rows LIGHT supplies to the Stage 1 QualityMatrix ticket in the schema of `Zenith2/Docs/Budgets.md → "Production-quality acceptance"` ([Z2-N-BUDGET-403] to [Z2-N-BUDGET-409]). The tier mechanism, the tier list and the tier files are RPROD's (`Zenith2/Docs/Design/RPROD/Renderer.md → "Quality settings"`, → "Quality tier selection"); this file owns the meaning and the values of LIGHT's keys.

## Scope of this file

| Topic | Heading |
|---|---|
| the tier IDs LIGHT uses | "Tier list" |
| how a tier reaches the passes | "Tier values and configuration" |
| per-feature tables | "Shadow tiers", "Volumetric tiers", "Screen-space tiers", "Reflection and GI tiers", "Temporal and resolution tiers", "Post-processing and output tiers" |
| fallbacks | "Fallbacks and triggers" |
| QualityMatrix rows | "QualityMatrix rows supplied by LIGHT" |

## Tier list

<!-- assumption: pending RPROD -->
The tier list is RPROD's and the Stage 1 QualityMatrix ticket's (`Zenith2/Docs/Design/PLAT/Configuration.md → "Quality tier resolution"` names illustrative IDs). LIGHT writes its tables against: Windows `QUALITY_TIER_WIN_LOW`, `QUALITY_TIER_WIN_DEFAULT`, `QUALITY_TIER_WIN_HIGH`, `QUALITY_TIER_WIN_EPIC`; S24 `QUALITY_TIER_S24_LOW`, `QUALITY_TIER_S24_DEFAULT`, `QUALITY_TIER_S24_HIGH`. Row L23 of the cross-area notes; RPROD adopted these IDs for Stage 5 (RPROD row RP19).
<!-- /assumption -->

Stage 3 delivers `QUALITY_TIER_WIN_DEFAULT` and `QUALITY_TIER_S24_DEFAULT` (the slice's tiers); Stage 5 delivers the full list with the per-platform quality tiers (Z2-I-RPROD-20).

## Tier values and configuration

Every tier value below is a `render.quality.*` key LIGHT registers (`Zenith2/Docs/Design/LIGHT.md → "Configuration keys"`), set per tier in RPROD's tier fragment `Zenith2/Engine/ZenithFlux/Internal/Config/Quality/<TierID>.config.json` (one file per tier carries RPROD's and LIGHT's keys, cooked into `content:/Engine/Config/Quality/<TierID>.config.json`, PLAT's layer 4). RPROD's step 2 of "Quality tier selection" calls `Flux_ReadLightQualitySettings` into `Flux_QualitySettings::m_xLight` whenever a quality-tier-layer key or the active tier changed (`Zenith2/Docs/Design/PLAT/Configuration.md → "Quality tier changes"`). On/off rows are bits of RPROD's `render.quality.features` mask (the `Flux_ViewFeature` bit named in the row), not LIGHT keys. A key that changes a history's format or extent invalidates that history ("History invalidation" of `Zenith2/Docs/Design/LIGHT/Temporal.md`). The cluster grid and capacities are fixed per capability tier and appear in no tier file (`Zenith2/Docs/Design/LIGHT/Lighting.md → "Cluster grid"`). `Test_LightConfig_TierFileApplied` loads each tier file and compares the resulting `m_xLight` with the tables below.

## Shadow tiers

| Key | WIN_LOW | WIN_DEFAULT | WIN_HIGH | WIN_EPIC | S24_LOW | S24_DEFAULT | S24_HIGH |
|---|---|---|---|---|---|---|---|
| `FLUX_VIEW_FEATURE_SHADOW_RECEIVE` | on | on | on | on | on | on | on |
| `render.quality.shadowAtlasWidth` × `shadowAtlasHeight` | 4096 × 2048 | 4096 × 4096 | 8192 × 4096 | 8192 × 4096 | 2048 × 2048 | 4096 × 2048 | 4096 × 2048 |
| `render.quality.shadowCascades` | 3 | 4 | 4 | 4 | 2 | 3 | 3 |
| `render.quality.shadowCascadeTileTexels` | 1024 | 1024 | 2048 | 2048 | 1024 | 1024 | 1024 |
| `render.quality.shadowDistanceMetres` | 150 | 250 | 400 | 600 | 80 | 120 | 160 |
| `render.quality.maxShadowedLights` (local) | 8 | 16 | 32 | 32 | 4 | 8 | 12 |
| `render.quality.shadowLocalTileMaxTexels` | 512 | 1024 | 1024 | 2048 | 256 | 512 | 512 |
| `render.quality.shadowLocalTileMinTexels` | 64 | 128 | 128 | 128 | 64 | 64 | 64 |
| `render.quality.shadowQuality` (`uPERM_SHADOW_QUALITY`) | 0 | 1 | 1 | 2 | 0 | 1 | 1 |
| `render.quality.shadowHiZ` | off | on | on | on | off | off | on |
| `FLUX_VIEW_FEATURE_CONTACT_SHADOWS`, `render.quality.contactShadowSteps` | off | on, 8 | on, 16 | on, 16 | off | on, 8 | on, 8 |
| `render.quality.contactShadowMaxDistanceMetres` | — | 0.3 | 0.3 | 0.3 | — | 0.2 | 0.2 |

Every column satisfies the four atlas rules of `Zenith2/Docs/Design/LIGHT/Shadows.md → "Atlas layout"` (width ≥ cascades × cascade tile; width ≥ local tile maximum L; height ≥ roundUp(cascade tile, L) + L; L ≥ the local minimum; the cascade tile is a multiple of L in every column, so roundUp changes nothing: for example WIN_LOW 3 × 1024 ≤ 4096, 512 ≤ 4096 and 1024 + 512 ≤ 2048; WIN_EPIC 4 × 2048 ≤ 8192 and 2048 + 2048 ≤ 4096 exactly; S24_LOW 2 × 1024 ≤ 2048, 256 ≤ 2048 and 1024 + 256 ≤ 2048), which `Test_LightConfig_TierFileApplied` checks for every tier file. Memory of the atlas (D24S8): 32, 64, 128, 128 MiB (Windows), 16, 32, 32 MiB (S24), a transient within `Z2-B-RHI-010`.

## Volumetric tiers

| Key | WIN_LOW | WIN_DEFAULT | WIN_HIGH | WIN_EPIC | S24_LOW | S24_DEFAULT | S24_HIGH |
|---|---|---|---|---|---|---|---|
| `FLUX_VIEW_FEATURE_VOLUMETRIC_FOG` | on | on | on | on | on | on | on |
| `render.quality.froxelGridX` × `froxelGridY` × `froxelGridZ` (three INT keys) | 80 × 45 × 32 | 128 × 72 × 48 | 160 × 90 × 64 | 160 × 90 × 64 | 64 × 36 × 24 | 80 × 45 × 32 | 80 × 45 × 32 |
| `render.quality.fogFarMetres` | 1,000 | 2,000 | 2,000 | 2,000 | 500 | 800 | 800 |
| `render.quality.fogTemporal` | off | on | on | on | off | on | on |
| `render.quality.fogShadowQuality` | 0 | 1 | 1 | 2 | 0 | 1 | 1 |
| `render.quality.fogLocalVolumes` (Stage 5) | 0 | 16 | 64 | 64 | 0 | 8 | 16 |
| sky LUTs | every tier (fixed sizes) | | | | | | |

## Screen-space tiers

| Key | WIN_LOW | WIN_DEFAULT | WIN_HIGH | WIN_EPIC | S24_LOW | S24_DEFAULT | S24_HIGH |
|---|---|---|---|---|---|---|---|
| `FLUX_VIEW_FEATURE_SSAO`, `render.quality.ssaoQuality`, `ssaoHalfResolution` | on, 0, half | on, 1, full | on, 2, full | on, 2, full | on, 0, half | on, 0, half | on, 1, half |
| `FLUX_VIEW_FEATURE_SSR`, `render.quality.ssrQuality` | off | on, 0 | on, 1 | on, 2 | off | off | on, 0 (half-resolution trace) |
| `render.quality.ssrMaxIterations` | — | 32 | 64 | 96 | — | — | 32 |
| `render.quality.ssgiEnabled` (Stage 5, ruling Q41 (a)) | off | off | on | on | off | off | off |

The SSR rows decide the specular-weight target: it is declared only on the tiers with `FLUX_VIEW_FEATURE_SSR` on (`Zenith2/Docs/Design/LIGHT/Reflections.md → "SSR composite"`). The SSGI row is the platform scope of `Zenith2/Docs/Design/LIGHT/GI.md → "SSGI platform scope"`, as ruling Q41 (a) fixes it.

## Reflection and GI tiers

| Key | WIN_LOW | WIN_DEFAULT | WIN_HIGH | WIN_EPIC | S24_LOW | S24_DEFAULT | S24_HIGH |
|---|---|---|---|---|---|---|---|
| `render.quality.probesMaxResident` (Stage 4) | 16 | 32 | 64 | 64 | 8 | 12 | 16 |
| `render.quality.probesRealtimeCapture` | off | on | on | on | off | off | on |
| `render.quality.probesFilterSamples` | 16 | 32 | 32 | 32 | 16 | 16 | 16 |
| `render.quality.giVolumes` (Stage 5) | on | on | on | on | on | on | on |
| `render.quality.giLightmaps` (Stage 5) | on | on | on | on | on | on | on |

## Temporal and resolution tiers

| Key | WIN_LOW | WIN_DEFAULT | WIN_HIGH | WIN_EPIC | S24_LOW | S24_DEFAULT | S24_HIGH |
|---|---|---|---|---|---|---|---|
| `FLUX_VIEW_FEATURE_TEMPORAL_AA` | on | on | on | on | on | on | on |
| `render.quality.upscaler` | taau | taau | taau | taa | taau | taau | taau |
| `render.quality.taaQuality` (`uPERM_QUALITY`) | 0 | 1 | 2 | 2 | 0 | 1 | 1 |
| `render.quality.dynamicResolutionEnabled` | on | on | on | off | on | on | on |
| `render.quality.dynamicResolutionMin` … `dynamicResolutionMax` | 0.75 … 1.0 | 0.75 … 1.0 | 0.75 … 1.0 | 1.0 … 1.0 | 0.75 … 1.0 | 0.75 … 1.0 | 0.75 … 1.0 |
| `render.quality.dynamicResolutionTargetMs` | 15.0 | 15.0 | 15.0 | — | 15.0 | 15.0 | 15.0 |
| `render.quality.taaCurrentWeight` | 0.1 | 0.1 | 0.1 | 0.1 | 0.125 | 0.125 | 0.125 |

The range's lower bound is `Z2-B-SHIP-011` on every tier ([Z2-N-BUDGET-087]); the QualityMatrix "Dynamic resolution range" column takes these values.

## Post-processing and output tiers

| Key | WIN_LOW | WIN_DEFAULT | WIN_HIGH | WIN_EPIC | S24_LOW | S24_DEFAULT | S24_HIGH |
|---|---|---|---|---|---|---|---|
| `render.quality.exposureMetering` | centre | centre | centre | centre | centre | centre | centre |
| `FLUX_VIEW_FEATURE_BLOOM`, `render.quality.bloomLevels` | on, 5 | on, 6 | on, 6 | on, 6 | on, 4 | on, 5 | on, 5 |
| `FLUX_VIEW_FEATURE_DEPTH_OF_FIELD` allowed, `render.quality.dofQuality` (Stage 5) | on, 0 | on, 0 | on, 1 | on, 1 | off | on, 0 | on, 0 |
| `FLUX_VIEW_FEATURE_MOTION_BLUR` allowed, `render.quality.motionBlurSamples` (Stage 5) | off | on, 8 | on, 16 | on, 16 | off | off | on, 8 |
| `r.output.mode` default (a user key, not a tier value) | auto | auto | auto | auto | auto | auto | auto |

Depth of field and motion blur run only for a camera that enables them (`Flux_CameraLightingSettings`) on a tier whose feature mask allows them.

## Fallbacks and triggers

| Feature | Fallback | Trigger |
|---|---|---|
| cascaded shadows | fewer cascades (the next lower tier's count), then the sun unshadowed beyond the cascades | quality-tier offset (thermal); `MEMORY_PRESSURE_CRITICAL` does not change shadows (Architecture's table has no shadow action) |
| local shadows | the light unshadowed for the frame, counted in `Flux.Shadows.Dropped` | atlas full |
| shadow HiZ | frustum and distance culling only | a tier with `render.quality.shadowHiZ` off |
| SSR | probe and sky specular | `MEMORY_PRESSURE_CRITICAL` (shedding action 9), a tier without SSR |
| volumetric fog resolution | the next lower tier's grid | `MEMORY_PRESSURE_CRITICAL` (shedding action 10, RPROD's `m_bFroxelGridDown`) |
| reflection probes | the sky, then the static environment | not resident, capture pending, load failure |
| baked GI | the sky SH | no resident data |
| SSGI | baked GI, then the sky | a tier without SSGI, confidence below 0.5 |
| TAAU / dynamic resolution | the floor scale | `MEMORY_PRESSURE_CRITICAL` (shedding action 11: RPROD's pressure hook calls `SetPressureFloor`) |
| DoF, motion blur | disabled | `MEMORY_PRESSURE_CRITICAL` (shedding action 9) |
| HDR10 / scRGB output | SDR | the surface lacks the colour space |
| stars, cloud composite, cloud shadow | the sky without them, a cloud-shadow factor of 1 | ENV's resource absent or the targets unset |
| area lights | none: they are evaluated on every tier | — |

## QualityMatrix rows supplied by LIGHT

One row per (feature, tier) in `## Rendering features` of `Zenith2/Docs/QualityMatrix.md`; the table below gives each feature's row template (the per-tier techniques are the tables above). Golden captures follow [Z2-N-BUDGET-408] (`Zenith2/Games/<Game>/Acceptance/Goldens/<FeatureID>_<TierID>_<Win|S24>.png`); the threshold values are the proposals the Stage 1 QualityMatrix ticket adopts or replaces.

| Feature ID | Feature | Requirements | Game for the golden | Acceptance kind | Threshold (proposed) | Budget rows |
|---|---|---|---|---|---|---|
| Z2-Q-LIGHT-001 | cascaded sun shadows | Z2-R-LIGHT-069, Z2-R-LIGHT-070, Z2-R-LIGHT-071, Z2-R-LIGHT-072, Z2-R-LIGHT-073, Z2-R-LIGHT-074, Z2-R-LIGHT-075, Z2-R-LIGHT-076, Z2-R-LIGHT-077, Z2-R-LIGHT-078, Z2-R-LIGHT-079, Z2-R-LIGHT-148 | OpenWorldSlice | golden | `FLIP<=0.05; R<=0.08 G<=0.08 B<=0.08` | Z2-B-LIGHT-002 |
| Z2-Q-LIGHT-002 | local-light shadows | Z2-R-LIGHT-069, Z2-R-LIGHT-070, Z2-R-LIGHT-075, Z2-R-LIGHT-076, Z2-R-LIGHT-148 | CinematicSlice | golden | `FLIP<=0.05; R<=0.08 G<=0.08 B<=0.08` | Z2-B-LIGHT-002 |
| Z2-Q-LIGHT-003 | contact shadows | Z2-R-LIGHT-081 | CinematicSlice | golden | `FLIP<=0.04; R<=0.06 G<=0.06 B<=0.06` | Z2-B-LIGHT-006 |
| Z2-Q-LIGHT-004 | GTAO | Z2-R-LIGHT-083, Z2-R-LIGHT-084, Z2-R-LIGHT-085 | CinematicSlice | golden | `FLIP<=0.04; R<=0.06 G<=0.06 B<=0.06` | Z2-B-LIGHT-005 |
| Z2-Q-LIGHT-005 | clustered lighting | Z2-R-LIGHT-057, Z2-R-LIGHT-058, Z2-R-LIGHT-059, Z2-R-LIGHT-060, Z2-R-LIGHT-061, Z2-R-LIGHT-062, Z2-R-LIGHT-063, Z2-R-LIGHT-150 | CinematicSlice | golden | `FLIP<=0.03; R<=0.05 G<=0.05 B<=0.05` | Z2-B-LIGHT-001, Z2-B-LIGHT-003 |
| Z2-Q-LIGHT-006 | volumetric fog | Z2-R-LIGHT-086, Z2-R-LIGHT-087, Z2-R-LIGHT-088, Z2-R-LIGHT-089, Z2-R-LIGHT-090, Z2-R-LIGHT-157 | OpenWorldSlice | golden | `FLIP<=0.05; R<=0.08 G<=0.08 B<=0.08` | Z2-B-LIGHT-004 |
| Z2-Q-LIGHT-007 | sky and atmosphere | Z2-R-LIGHT-091, Z2-R-LIGHT-092, Z2-R-LIGHT-093, Z2-R-LIGHT-094, Z2-R-LIGHT-154, Z2-R-LIGHT-155, Z2-R-LIGHT-158 | OpenWorldSlice | human | the human-row text of [Z2-N-BUDGET-407]; segment: day/night transition segment | Z2-B-ENV-001 |
| Z2-Q-LIGHT-008 | IBL and reflection probes | Z2-R-LIGHT-101, Z2-R-LIGHT-102, Z2-R-LIGHT-103, Z2-R-LIGHT-104, Z2-R-LIGHT-105, Z2-R-LIGHT-106, Z2-R-LIGHT-107, Z2-R-LIGHT-108, Z2-R-LIGHT-109 | CinematicSlice | golden | `FLIP<=0.05; R<=0.08 G<=0.08 B<=0.08` | Z2-B-LIGHT-017 |
| Z2-Q-LIGHT-009 | SSR | Z2-R-LIGHT-096, Z2-R-LIGHT-097, Z2-R-LIGHT-098, Z2-R-LIGHT-099, Z2-R-LIGHT-100, Z2-R-LIGHT-160 | CinematicSlice | golden | `FLIP<=0.06; R<=0.10 G<=0.10 B<=0.10` | Z2-B-LIGHT-007 |
| Z2-Q-LIGHT-010 | baked GI (volumes and lightmaps) | Z2-R-LIGHT-110, Z2-R-LIGHT-111, Z2-R-LIGHT-112, Z2-R-LIGHT-113, Z2-R-LIGHT-114, Z2-R-LIGHT-115, Z2-R-LIGHT-116, Z2-R-LIGHT-161 | OpenWorldSlice | golden | `FLIP<=0.04; R<=0.06 G<=0.06 B<=0.06` | Z2-B-LIGHT-019 |
| Z2-Q-LIGHT-011 | SSGI (ruling Q41 (a)) | Z2-R-LIGHT-117, Z2-R-LIGHT-118 | CinematicSlice | golden | `FLIP<=0.08; R<=0.12 G<=0.12 B<=0.12` | Z2-B-LIGHT-016, Z2-B-LIGHT-022 |
| Z2-Q-LIGHT-012 | TAA and temporal upscale | Z2-R-LIGHT-014, Z2-R-LIGHT-015, Z2-R-LIGHT-016, Z2-R-LIGHT-017, Z2-R-LIGHT-018, Z2-R-LIGHT-019, Z2-R-LIGHT-020, Z2-R-LIGHT-021 | OpenWorldSlice | golden | `FLIP<=0.04; R<=0.06 G<=0.06 B<=0.06` (after the fixed 32-frame sequence) | Z2-B-LIGHT-010 |
| Z2-Q-LIGHT-013 | dynamic resolution | Z2-R-LIGHT-022, Z2-R-LIGHT-023, Z2-R-LIGHT-024, Z2-R-LIGHT-025, Z2-R-LIGHT-026, Z2-R-LIGHT-027, Z2-R-LIGHT-028, Z2-R-LIGHT-153 | OpenWorldSlice | golden | `FLIP<=0.06; R<=0.10 G<=0.10 B<=0.10` at the floor scale against scale 1.0 | Z2-B-SHIP-011 |
| Z2-Q-LIGHT-014 | auto exposure | Z2-R-LIGHT-029, Z2-R-LIGHT-030, Z2-R-LIGHT-031, Z2-R-LIGHT-032, Z2-R-LIGHT-033, Z2-R-LIGHT-034 | OpenWorldSlice | human | the human-row text of [Z2-N-BUDGET-407]; segment: interior-to-exterior segment | Z2-B-LIGHT-011 |
| Z2-Q-LIGHT-015 | bloom | Z2-R-LIGHT-046, Z2-R-LIGHT-047 | CinematicSlice | human | the human-row text of [Z2-N-BUDGET-407]; segment: the whole playthrough | Z2-B-LIGHT-014 |
| Z2-Q-LIGHT-016 | depth of field | Z2-R-LIGHT-048, Z2-R-LIGHT-049, Z2-R-LIGHT-050 | CinematicSlice | human | the human-row text of [Z2-N-BUDGET-407]; segment: the close-up segments | Z2-B-LIGHT-012 |
| Z2-Q-LIGHT-017 | motion blur | Z2-R-LIGHT-051, Z2-R-LIGHT-052, Z2-R-LIGHT-053 | CinematicSlice | human | the human-row text of [Z2-N-BUDGET-407]; segment: the whole playthrough | Z2-B-LIGHT-013 |
| Z2-Q-LIGHT-018 | tonemap and grading (SDR) | Z2-R-LIGHT-035, Z2-R-LIGHT-036, Z2-R-LIGHT-037, Z2-R-LIGHT-038, Z2-R-LIGHT-039 | CinematicSlice | golden | `FLIP<=0.02; R<=0.03 G<=0.03 B<=0.03` | Z2-B-LIGHT-015 |
| Z2-Q-LIGHT-019 | HDR output | Z2-R-LIGHT-040, Z2-R-LIGHT-041, Z2-R-LIGHT-042, Z2-R-LIGHT-043 | CinematicSlice | human | the human-row text of [Z2-N-BUDGET-407]; segment: the HDR segments, on an HDR10 display | Z2-B-LIGHT-015 |
| Z2-Q-LIGHT-020 | area lights | Z2-R-LIGHT-064 | CinematicSlice | golden | `FLIP<=0.05; R<=0.08 G<=0.08 B<=0.08` | Z2-B-LIGHT-003 |
| Z2-Q-LIGHT-021 | deferred and visibility-buffer lighting | Z2-R-LIGHT-066 | CinematicSlice | golden | `FLIP<=0.03; R<=0.05 G<=0.05 B<=0.05` against Forward+ | Z2-B-LIGHT-008, Z2-B-LIGHT-009 |

Every `Requirements` cell is an explicit list of `Z2-R-LIGHT-NNN` IDs separated by `, ` ([Z2-N-BUDGET-406]: the IDs the row realises), never a range. Every row's `Windows technique` and `S24 technique` cells name the tier's values from the tables above, and its `Fallback` and `Fallback trigger` cells the row of "Fallbacks and triggers".
