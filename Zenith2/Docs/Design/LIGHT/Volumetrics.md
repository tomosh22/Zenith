# LIGHT design: volumetric fog, sky and atmosphere

Section file of `Zenith2/Docs/Design/LIGHT.md`. It specifies the froxel fog volume (grid, media, injection, temporal reprojection, integration, application), the sky and atmosphere passes (the transmittance, multiple-scattering and sky-view LUTs, the sky draw and aerial perspective), and the hooks through which ENV's star field, cloud layer, cloud shadows and underwater medium reach those passes. The passes are Rendering's: `Zenith2/Docs/Rendering/PerPass/Common.md → "VolumetricFog"` (budget `Z2-B-LIGHT-004`, queue A) and `Zenith2/Docs/Rendering/PerPass/Common.md → "SkyAtmosphere"` (budget `Z2-B-ENV-001`, queue G).

## Scope of this file

| Topic | Heading |
|---|---|
| ownership of the atmosphere between LIGHT and ENV | "Atmosphere ownership" |
| the participating media | "Media model" |
| the froxel grid | "Froxel grid" |
| per-froxel lighting | "Scattering injection" |
| temporal stability | "Temporal reprojection" |
| front-to-back integration | "Integration" |
| sampling the volume in shaders | "Fog application" |
| the three atmosphere LUTs | "Transmittance LUT", "Multiple-scattering LUT", "Sky-view LUT" |
| the sky texels | "Sky draw and aerial perspective", "Star field", "Cloud layer composite" |
| the sun at a shading point | "Sun transmittance", "Cloud shadow on the sun" |
| a submerged view | "Underwater medium override" |
| local fog volumes | "Local fog volumes" |
| the header | "Flux_Atmosphere header" |

## Atmosphere ownership

| Concern | Owner |
|---|---|
| the fog and sky passes, their algorithms, LUT formats and parameterisations, the physical defaults below, the star-field, cloud-layer, cloud-shadow and underwater hooks of this file | LIGHT (Stage 3 for the Forward+ frame of the playable slice; the ENV hooks at the stage of the ENV feature that feeds them) |
| the atmosphere, fog and local-fog-volume components and documents, time of day, the sun and moon lights, the star cubemap, clouds and their passes, weather, water and the underwater composite (Z2-I-ENV-06 to Z2-I-ENV-09) | ENV, whose `Flux_EnvironmentBlocks` resource supplies the atmosphere block of `Flux_RenderLightingEnvironment` (`Zenith2/Docs/Design/LIGHT.md → "Flux_LightSettings header"`) and the sun and moon slot values that the engine environment extractor copies, and which adds its cloud passes to the frame; a view's submersion reaches LIGHT through ENV's per-view `Flux_RenderEnvironmentView` records ("Underwater medium override") |
| the sky GPU budget row `Z2-B-ENV-001` | ENV (LIGHT's passes and ENV's cloud layer are measured against it together) |

<!-- assumption: pending RPROD, ENV, X125 -->
`Flux_RenderEnvironment` has one `SetEnvironment` per world per frame ([Z2-N-REND-008] unchanged) and one writer, the engine environment extractor `Render.ExtractEnvironment` (`Zenith2/Docs/Rendering.md → "Render environment"`, RPROD row RP17, the orchestrator's ruling on X125 (applied by core-doc batch 4)). LIGHT's `Flux_RenderLightingEnvironment m_xLighting` is two groups the extractor fills: the atmosphere block (`Flux_AtmosphereBlock`), copied from ENV's `Flux_EnvironmentBlocks` world resource, which carries LIGHT's types (`Flux_AtmosphereParams`, `Flux_FogMediumParams`, `Flux_LocalFogVolume`, `Flux_StarField`, `Flux_CloudShadowMapping`, `m_uSkyRevision`; the per-view underwater medium is not in it: it is ENV's `Flux_RenderEnvironmentView` record, which LIGHT turns into a `Flux_ViewMediumOverride`, "Underwater medium override"); and the world-lighting block (`Flux_WorldLightingBlock`), read by reflection from the `Zenith.Lighting.WorldLighting` component. The sun and moon are the environment's explicit slots (`Zenith2/Docs/Design/LIGHT/Lighting.md → "Sun and moon slots"`). ENV's block setters (row EN5) are withdrawn by that ruling. A world with no ENV resource (a game without the ZenithEnvironment module, an interior-only scene) gets `Flux_AtmosphereBlock::m_bValid` false, and LIGHT then uses the defaults of `Flux_AtmosphereParams` with height fog disabled, no local volumes, no stars and no cloud shadow. Row L31 of the cross-area notes answers EN5 and EN6.
<!-- /assumption -->

## Media model

<!-- elaboration -->
All coefficients are per metre; the world is Y-up with the ground plane at `y = m_fGroundHeightMetres` and the planet centre at `(0, m_fGroundHeightMetres − R_g, 0)`, so the altitude of a point p is `|p − centre| − R_g`.

| Medium | Scattering σ_s(h) | Absorption σ_a(h) | Phase | Default |
|---|---|---|---|---|
| Rayleigh | `β_R · exp(−h / H_R)` | 0 | `3 / (16π) · (1 + cos²θ)` | `β_R = (5.802, 13.558, 33.1) × 10⁻⁶`, `H_R = 8,000 m` |
| Mie | `β_M · exp(−h / H_M)` | `α_M · exp(−h / H_M)` | Cornette–Shanks with `g_M` | `β_M = 3.996 × 10⁻⁶`, `α_M = 4.40 × 10⁻⁶`, `H_M = 1,200 m`, `g_M = 0.8` |
| ozone | 0 | `α_O · max(0, 1 − |h − 25,000| / 15,000)` | — | `α_O = (0.650, 1.881, 0.085) × 10⁻⁶` |
| height fog | `ρ₀ · a · exp(−k (h − h₀))` | `ρ₀ · (1 − a) · exp(−k (h − h₀))` | Henyey–Greenstein with `g_F` | density `ρ₀ = 0.002`, albedo `a = 0.9`, falloff `k = 0.05`, base height `h₀ = 0`, `g_F = 0.3` |
| local fog volumes (Stage 5) | per volume, "Local fog volumes" | per volume | Henyey–Greenstein, per volume | — |
| an underwater view's medium (Stage 4) | `σ_s,w` of "Underwater medium override", homogeneous | `σ_a,w`, homogeneous, per channel | isotropic | — |
| fog emission | — | — | — | `L_e` (nits, default 0), added to the in-scatter |

Planet and atmosphere: `R_g = 6,360,000 m`, `R_t = R_g + 100,000 m`; ground albedo `0.3` (Hillaire 2020 defaults). `Flux_AtmosphereParams` and `Flux_FogMediumParams` carry every value of this table.

## Froxel grid

| Parameter | Rule |
|---|---|
| resolution | the three `CONFIG_VALUE_TYPE_INT` keys `render.quality.froxelGridX`, `…froxelGridY` and `…froxelGridZ` per quality tier (`Zenith2/Docs/Design/LIGHT/QualityTiers.md → "Volumetric tiers"`), read into `m_xLight` by `Flux_ReadLightQualitySettings`: desktop high 160 × 90 × 64, mobile 80 × 45 × 32, as Rendering's capability row states |
| slice distribution | exponential: slice k spans `[n_f · (F / n_f)^(k / K), n_f · (F / n_f)^((k + 1) / K)]`, `n_f = Flux_FogSettings::m_fNearMetres` (0.5 m), `F = Flux_FogSettings::m_fFarMetres` (`render.quality.fogFarMetres`, desktop 2,000 m, mobile 800 m); F is a tier value independent of the view's far plane, which is what lets the sky-view LUT start at F for every view (design choice LR12) |
| froxel position | the centre of the screen tile at the jittered slice depth ("Temporal reprojection"), unprojected with the view's unjittered inverse projection |
| formats | scattering and integrated volumes RGBA16F (Rendering's output table); the 3D view is one mip, `[FullCoverage]` |
| memory | 160 × 90 × 64 × 8 B = 7.03 MiB per volume desktop; the view holds 3 volumes (the two history halves and the integrated volume), the scattering volume being a transient |

## Scattering injection

Step `uPERM_STEP = inject`, one thread per froxel, 4 × 4 × 4 groups.

| Term | Per-froxel value |
|---|---|
| media | σ_s and σ_t = σ_s + σ_a of every medium of "Media model" at the froxel's altitude, summed: Rayleigh, Mie and ozone from the pass's plain-data `Flux_AtmosphereGPU m_xAtmosphere` (0 when its `m_uEnabled` is 0), the height fog from `Flux_FogMediumGPU m_xMedium`; local fog volumes (Stage 5) added from `m_xLocalFogVolumes`; for a view whose override is enabled, the override's homogeneous medium (`Flux_MediumOverrideGPU m_xOverride`) replaces the height fog ("Underwater medium override") |
| directional lights | the directional entries are read from the pass's `m_xLightList` (words 1–3) and `m_xLightTable` exactly as `GetDirectional` does (`Zenith2/Docs/Design/LIGHT/Lighting.md → "The EvaluateLighting contract"`), with a `Flux_LightingAtmosphere` built from the pass's `m_xTransmittanceLUT`, `m_xCloudShadowMap`, `m_xAtmosphere` and `m_xCloudShadowMapping`. `FLUX_LIGHT_FLAG_VOLUMETRIC` gates every directional term: an entry whose record lacks the bit injects nothing, whatever its role, and every entry carrying it injects, with or without a role. For the sun (the directional entry with the sun role, `Zenith2/Docs/Design/LIGHT/Lighting.md → "Light and decal list build"`): `E_sun × T_atm(h, μ_sun) × visibility × cloudShadow × phase_mix(θ)`, where `phase_mix = (σ_s,R · P_R + σ_s,M · P_M + σ_s,F · P_HG) / σ_s`, `T_atm` from "Sun transmittance", `cloudShadow` from "Cloud shadow on the sun", and the visibility from the cascades at the froxel centre with `uPERM_SHADOW_QUALITY` (`render.quality.fogShadowQuality`; 0: unshadowed, 1: one bilinear comparison tap, 2: the 3 × 3 kernel of `Zenith2/Docs/Design/LIGHT/Shadows.md → "Filtering"` quality 1); the moon (the entry with the moon role) adds the same term with its own illuminance and transmittance and no cloud shadow; an entry with no role (a scene directional light of a world without environment slots while the atmosphere is disabled, or a second scene directional light) adds `E × visibility × phase_mix(θ)` with `T_atm = 1` and no cloud shadow, as step 3 of `Zenith2/Docs/Design/LIGHT/Lighting.md → "Evaluation order"` lights it; `T_atm` is 1 for every entry while `m_xAtmosphere.m_uEnabled` is 0, and each entry's visibility comes from its own cascades (1 when its `m_uShadowIndex` is `0xFFFFFFFF`). `Test_Fog_DirectionalVolumetricGating` asserts the three cases |
| local lights | every light of the froxel's light cluster, `Flux_ClusterIndex(xPixel, fDepth, xMap)` with `xPixel = floor((froxel.xy + 0.5) / m_xGrid.xy × m_xRenderExtent)` (the froxel tile centre in render pixels of the extent the clusters were built at), `fDepth` the froxel's jittered slice depth and `xMap` from the pass's `m_xClusterGrid`, `m_xClusterTileSize` and `m_xClusterDepthRange` (`Zenith2/Docs/Design/LIGHT/Lighting.md → "Cluster grid"`), carrying `FLUX_LIGHT_FLAG_VOLUMETRIC`, with the attenuation of `Zenith2/Docs/Design/LIGHT/Lighting.md → "Light attenuation and shape"` (n·l omitted), the Henyey–Greenstein phase of the fog medium, the volumetric scale of the light's flags, and its shadow at quality ≥ 1 |
| ambient | `σ_s × c₀₀ / √(4π)`: the sphere average of the sky radiance SH (`Zenith2/Docs/Design/LIGHT/GI.md → "Sky irradiance"`), an isotropic approximation of sky in-scatter |
| emission | `L_e × ρ(h) / ρ₀` for the height fog |
| output | `m_xScattering`: RGB = pre-exposed in-scattered radiance per metre (`(Σ E_i phase_i) × fPreExposure`), A = σ_t |

## Temporal reprojection

With `bPERM_TEMPORAL` (`render.quality.fogTemporal`), each frame jitters the froxel sample depth inside its slice by the Halton(2) value of `frameIndex mod 8` and blends with the reprojected history:

| Step | Rule |
|---|---|
| 1 | reproject the froxel's world position with the previous frame's unjittered view-projection and the previous frame's slice distribution into history coordinates `(u, v, w)` |
| 2 | history valid when the view's fog history is valid (`Zenith2/Docs/Design/LIGHT/Temporal.md → "History invalidation"`) and `(u, v, w)` lies in [0, 1]³ |
| 3 | `S = lerp(S_history, S_current, α)`, `α = 1 − Flux_FogSettings::m_fHistoryWeight` (default weight 0.9), trilinear history sample; α = 1 when the history is invalid |
| 4 | the blended value is written to the current half of the view's fog history (RGBA16F 3D, marked output) and is the input of "Integration" |

<!-- assumption: pending RPROD, X103 -->
`Flux_ViewPersistentState` (`Zenith2/Docs/Rendering.md → "View identity and persistent state"`) has no fog history; `m_xPrevScattering` of Rendering's `VolumetricFog` table is "view history". LIGHT needs `Flux_TextureHandle m_axFogScattering[2]` in `Flux_ViewPersistentState`: row L7 and item (a) of X103 (applied by core-doc batch 4).
<!-- /assumption -->

## Integration

Step `uPERM_STEP = integrate`, one thread per froxel column (8 × 8 groups), front to back from slice 0 (the energy-conserving integration of Hillaire 2015):

| Quantity | Update per slice k |
|---|---|
| step length | `Δ_k = (z_{k+1} − z_k) × |ray| / |ray · forward|` for the column's ray |
| transmittance of the step | `T_k = exp(−σ_t Δ_k)` |
| in-scatter of the step | `I_k = (S_rgb − S_rgb · T_k) / max(σ_t, 1e-7)` (constant in-scatter over the step, integrated analytically) |
| accumulation | `L += T_acc × I_k`; `T_acc *= T_k` |
| write | `m_xIntegrated[k] = (L, T_acc)`: the in-scatter and transmittance from the camera to the far edge `z_{k+1}` |

The integration is sequential per column and therefore deterministic; it runs in `float` and stores half. For a view with an underwater override the extinction is per channel: the integration uses the luminance extinction `σ_lum = dot(σ_t,rgb, (0.2126, 0.7152, 0.0722))` for A, and `Flux_ApplyFroxelFog` reconstructs the per-channel transmittance as `T_rgb = T^(σ_t,rgb / σ_lum)` from the plain-data ratio `m_xMediumExtinctionRatio` (1, 1, 1 without an override), which is exact for the homogeneous water medium (`T = exp(−σ d)` per channel) and approximate only where a local fog volume overlaps the water.

## Fog application

`Zenith2/Shaders/Lighting/Fog.slang` exports `Flux_ApplyFroxelFog(half3 xColour, float2 xScreenUV, float fLinearDepth)`, used by the forward opaque passes, deferred lighting, transparent forward, particles and the sky draw.

| Step | Rule |
|---|---|
| slice coordinate | `s(z) = K × log(z / n_f) / log(F / n_f)`, clamped to [0, K] |
| lookup | the integrated volume at `(u, v, w)` = `(screenUV, max(s − 0.5, 0) / K)`, trilinear with `uFLUX_SAMPLER_LINEAR_CLAMP_NO_MIP`, so texel centre k + 0.5 in w-space corresponds to its stored depth `z_{k+1}` |
| before slice 0 | for `s < 1` the first texel's in-scatter and optical depth scale linearly by s |
| beyond F | the last slice's value (`z ≥ F`); the atmosphere beyond F is part of the sky-view LUT, never of the fog volume |
| result | `xColour × T_rgb + L` (both pre-exposed), `T_rgb = T^m_xMediumExtinctionRatio` per channel ("Integration"; `T_rgb = T` without an underwater override) |

## Transmittance LUT

The `transmittance` step of `SkyViewLUT.slang` writes a 256 × 64 RGBA16F table. The table is a persistent-imported texture recomputed only in a frame whose `Flux_AtmosphereBlock::m_uSkyRevision` differs from the revision it was last computed for (and in the first frame, a frame whose atmosphere block validity changed, and after device-loss recovery); otherwise the persistent copy is read. This is the "changed flag" ENV's extraction step 2 supplies (EN6): ENV increments the revision whenever the atmosphere, fog or star values change, and LIGHT rebuilds on any change of the revision whatever block changed (the LUTs depend on the atmosphere only, so a fog-only or star-only change costs one unneeded rebuild and never a stale LUT). "The revision it was last computed for" is committed by `EndFrame` only when the frame that declared the rebuild submitted (`Zenith2/Docs/Design/LIGHT.md → "Deferred and failed frames"`): a deferred or failed rebuild frame leaves the rebuild pending, and the next built frame declares it again. Persistence of both atmosphere LUTs is row L9 and item (f) of X103.

| Axis | Mapping (Bruneton 2017) |
|---|---|
| y (altitude r) | `ρ = √(max(r² − R_g², 0))`, `H = √(R_t² − R_g²)`, `y = ρ / H` |
| x (view zenith cosine μ) | `d = −rμ + √(r²(μ² − 1) + R_t²)` (distance to the top boundary), `d_min = R_t − r`, `d_max = ρ + H`, `x = (d − d_min) / (d_max − d_min)` |
| integration | 40 midpoint steps of the extinction of "Media model" (Rayleigh, Mie, ozone; height fog excluded) along the ray to the top boundary |
| value | RGB transmittance, A = 1 |

## Multiple-scattering LUT

<!-- assumption: pending RPROD, X103 -->
Rendering's `SkyViewLUT` program declares `uPERM_STEP` {transmittance, skyView}; LIGHT adds a `multiScatter` step writing a 32 × 32 RGBA16F table (Hillaire 2020, section 5.5) between them, the field `m_xMultiScatterLUT` in `Flux_SkyViewLUTParams` and `Flux_FrameShared`, and the persistence rule of "Transmittance LUT" for both tables. This is row L9 and item (f) of X103 (applied by core-doc batch 4).
<!-- /assumption -->

| Element | Rule |
|---|---|
| axes | x = `0.5 + 0.5 × μ_sun` (sun zenith cosine), y = altitude / `(R_t − R_g)` |
| integration | 64 directions on a uniform 8 × 8 sphere grid, 20 steps each, second-order scattering with the transmittance LUT; the result `Ψ_ms = L_2nd / (1 − f_ms)` of Hillaire's equation 10, with the ground albedo |
| value | RGB `Ψ_ms`, A = 1 |
| recomputation | with the transmittance LUT, on a sky revision change, committed only by a submitted frame as "Transmittance LUT" states (the table is independent of the sun direction, which is an axis) |

## Sky-view LUT

The `skyView` step writes the 192 × 108 R11G11B10F table once per frame from the main view's camera position (or, with no main view, the first editor view's), shared by every view through `Flux_FrameShared::m_xSkyViewLUT`.

| Element | Rule |
|---|---|
| x (azimuth) | the azimuth relative to the sun's azimuth, `x = φ / (2π)` |
| y (elevation) | non-linear around the horizon: `l` the elevation relative to the horizon angle at the camera's altitude, `y = 0.5 + 0.5 × sign(l) × √(|l| / (π/2))` |
| ray | from the camera position plus `F` metres along the direction (the fog volume's far distance), to the atmosphere's top or the ground |
| integration | 30 steps: single scattering with the transmittance LUT and the Rayleigh and Mie phases, plus multiple scattering from the MS LUT; the top-of-atmosphere illuminance `E_sun` of the entry with the sun role, and the moon's (the entry with the moon role) added with the same phases |
| value | pre-exposed radiance (×`fPreExposure` of the main view) |

Views other than the main view reuse the main view's LUT; a view far from the main camera in altitude sees the main camera's sky, which is the accepted cost of one LUT per frame ([Z2-N-REND-605]). ENV's cloud layer pass writes its targets in this LUT's parameterisation ("Cloud layer composite").

## Sky draw and aerial perspective

`SkyDraw.slang`, per view, a fullscreen pass over the sky texels (depth `EQUAL` against the far clear value, depth read-only). For a pixel of direction ω the composite is evaluated back to front:

| Order | Term | Rule |
|---|---|---|
| 1 | star field | `stars = T_view(ω) × L_star(R⁻¹ ω) × m_fIntensityNits × fPreExposure` ("Star field"); 0 when the block carries no star field |
| 2 | sky radiance | `sky = stars + skyView(ω)`, the sky-view LUT at the pixel's direction |
| 3 | sun disc | main and editor views only: inside the sun's angular radius, `sky += E_sun / (π × radius²) × T_atm(camera, ω) × limbDarkening(t) × fPreExposure`, with `limbDarkening(t) = 1 − 0.6 (1 − √(1 − t²))`, t the normalised distance from the disc centre; the disc is excluded from capture views, whose sky enters IBL (`Zenith2/Docs/Design/LIGHT/Reflections.md → "Sky capture"`) where the sun is a separate analytic light |
| 4 | cloud layer | `sky = cloudRadiance(ω) + cloudTransmittance(ω) × sky` when ENV's cloud layer ran this frame ("Cloud layer composite"); clouds therefore occlude the stars and the sun disc |
| 5 | aerial perspective (`bPERM_AERIAL_PERSPECTIVE`) | `sky = L_fog(F) + T_fog(F) × sky`, with the fog volume's last slice: the froxel volume carries the atmosphere and fog up to F, the LUT carries the atmosphere beyond F, so nothing is counted twice (design choice LR12) |
| — | no atmosphere | when `Flux_AtmosphereParams::m_bEnabled` is false, orders 1 to 3 are replaced by the constant `m_xFallbackSkyRadiance` (nits, default 0) × `fPreExposure`; orders 4 and 5 still apply |
| — | output | pre-exposed radiance into the HDR colour target |

<!-- assumption: pending RPROD, X103 -->
The sun-disc data (`m_xSunDirectionWS`, `m_xSunIlluminance`, `m_fSunAngularRadius`) and the hook fields `m_xStarField`, `m_xStarRotation`, `m_fStarIntensity`, `m_xCloudLayer`, `m_xCloudTransmittance` of `Flux_SkyDrawParams` are items (f) of X103 and the additions of `Zenith2/Docs/Design/LIGHT.md → "Rendering table additions pending X103"`; the cloud targets reach the pass through `Flux_FrameShared::m_xCloudLayer` and `m_xCloudTransmittance`, set by ENV's `Env_AddCloudLayer` and left unset when it adds no pass.
<!-- /assumption -->

`Flux_ComputeSkyRadianceReference` (the CPU reference of orders 2 and 5 with a single integration from the camera to the top of the atmosphere, "Flux_Atmosphere header") is the reference of `Test_Fog_SkyNoDoubleCount`.

## Star field

ENV's night sky (`Zenith2/Docs/Design/ENV/Atmosphere.md → "Night sky"`) reaches the sky draw through `Flux_AtmosphereBlock::m_xStars`, copied by the environment extractor from ENV's resource:

| Field | Meaning |
|---|---|
| `m_xCubeTexture` | the `Zenith_PersistentID` of ENV's HDR star cubemap (a GEOM cube texture, `TEXTURE_USAGE_LINEAR`); null: no stars |
| `m_xRotation` | the celestial rotation `R` ENV computes from the hour (`2π × h / 23.9344696` about the celestial axis) as a `Zenith_Quaternion`; the sky draw samples the cube in direction `R⁻¹ ω` |
| `m_fIntensityNits` | ENV's `m_fStarIntensity`, the radiance the cube's value 1.0 represents |

`T_view(ω)` is the transmittance-LUT tap from the camera's altitude along ω to the top of the atmosphere (0 below the horizon, where the ray meets the ground), so the stars fade into the horizon haze and vanish behind a daylight sky by magnitude alone; the sky draw adds no day/night switch of its own. The cube is sampled with `uFLUX_SAMPLER_LINEAR_CLAMP` at mip 0. The sky capture includes the stars ("Sky capture" of Reflections.md), the aerial perspective does not attenuate them further than order 5 already does.

## Cloud layer composite

ENV's cloud layer pass (`Zenith2/Docs/Design/ENV/Atmosphere.md → "Cloud layer pass"`, `Env_AddCloudLayer`, once per frame before `SkyAtmosphere`, row EN11) writes two 192 × 108 targets in the sky-view LUT's parameterisation: in-scattered radiance (R11G11B10F, pre-exposed by the main view's `fPreExposure`, the same convention as the sky-view LUT) and transmittance (R8_UNORM). The sky draw reads them `[Optional]` with the sky-view LUT's (x, y) mapping at ω and applies order 4 of "Sky draw and aerial perspective". A view whose pre-exposure differs from the main view's rescales `cloudRadiance` by `fPreExposure(view) / fPreExposure(main)`, as it does the sky-view LUT. The sky capture applies the same composite. With either target unset the order is skipped, so a world without clouds draws exactly the cloudless sky (`Test_Sky_CloudLayerComposite`, and ENV's `Test_Clouds_ZeroCoverageAddsNothing`).

## Sun transmittance

When the atmosphere is enabled, the `m_xColourIntensity` of the directional entry with the sun role is its top-of-atmosphere illuminance, and every consumer multiplies it by `Flux_SampleSunTransmittance(Flux_LightingAtmosphere xAtm, float3 xPositionWS, float3 xToLightWS)`, one transmittance-LUT tap of `xAtm.m_xTransmittanceLUT` at the point's altitude `|p − centre| − R_g` (centre and radii from `xAtm.m_xAtmosphere`, "Media model") and the light's zenith cosine, mapped as "Transmittance LUT" states; when the atmosphere is disabled, the factor is 1 and the lux value is the ground illuminance. The entry with the moon role takes the same factor with its own direction; a directional entry with neither role is not attenuated by the atmosphere. The roles are the bits of word 1 of the light list (`Zenith2/Docs/Design/LIGHT/Lighting.md → "Light and decal list build"`): ENV's sun and moon slots are directional entries 0 and 1 whatever other directional lights the scene holds (X125's rule, replacing "the first two directional lights").

## Cloud shadow on the sun

Stage 5, on the tiers where ENV writes it (desktop and S24 high, `Zenith2/Docs/Design/ENV/Atmosphere.md → "Cloud shadows"`): ENV's `Env_AddCloudShadowMap` writes a 512 × 512 R8_UNORM transmittance map once per frame into `Flux_FrameShared::m_xCloudShadowMap`, covering a square of `m_fExtentMetres` (4,096 m) centred at `(m_fOriginX, m_fOriginZ)` and moved in 64 m steps (`Flux_AtmosphereBlock::m_xCloudShadow`).

| Rule | Value |
|---|---|
| lookup | `Flux_SampleCloudShadow(Flux_LightingAtmosphere xAtm, float3 xPositionWS)`: with `(originX, originZ, invExtent, outside) = xAtm.m_xCloudShadowMapping`, the factor is 1 when `invExtent` is 0 (no map this frame); else `uv = ((x − originX), (z − originZ)) × invExtent + 0.5`, bilinear with `uFLUX_SAMPLER_LINEAR_CLAMP` on `xAtm.m_xCloudShadowMap`; the map stores the transmittance of the cloud layer along the sun direction, so no projection along the sun is applied on LIGHT's side |
| outside the square | `m_fOutsideTransmittance` (ENV's mean coverage transmittance; 1 when unset), blended in over the outer 128 m of the square so the edge does not show |
| applied to | the directional entry with the sun role only, in step 3 of `Zenith2/Docs/Design/LIGHT/Lighting.md → "Evaluation order"` and in the froxel sun term; never to the moon or another light; never to the sun disc (the cloud layer composite occludes it instead) |
| absent | the map unset (a tier without it, a world without clouds, Stage 3 and 4): the factor is 1 |

The consumers' `m_xCloudShadowMap` fields are `[Optional]` additions to ForwardPlusOpaque, ForwardOpaquePerObject, DeferredLighting, TransparentForward and VolumetricFog (`Zenith2/Docs/Design/LIGHT.md → "Rendering table additions pending X103"`).

## Underwater medium override

ENV's underwater medium (`Zenith2/Docs/Design/ENV/WaterWeather.md → "Underwater medium"`) replaces a submerged view's height fog. Which views exist is known only at extraction, so the medium is a per-view extraction record, never a member of ENV's `Flux_EnvironmentBlocks` resource or of `Flux_AtmosphereBlock`.

<!-- assumption: pending ENV, RPROD, row L36 -->
The orchestrator's ruling on LIGHT's round-2 finding 4, as refined (RPROD row RP25 (b) and (d)): the per-view record is ENV's `Flux_RenderEnvironmentView` (`Zenith2/Docs/Design/ENV.md → "Render-world records"`), which ENV's `Environment.ExtractEnvironment` writes once per view request with the submersion of `Zenith2/Docs/Design/ENV/WaterWeather.md → "Submersion test"`, keyed by the `Flux_ViewID` that RPROD's camera extractor gives the same request (`Flux_MakeViewID(kind, owner, sub-index)`, `Zenith2/Docs/Design/RPROD/Extractors.md → "Camera extractor"`). RPROD's camera extractor reads no water state, because ZenithRuntime has no dependency on ZenithEnvironment. LIGHT consumes the records on the render side: `Flux_ViewMediumOverride` (declared in `Flux_LightSettings.h`) is LIGHT's per-view evaluation input, built from them.
<!-- /assumption -->

| Step (`Flux_LightingSystem::ApplyRenderWorld`, `Flux.BuildGraph`) | Rule |
|---|---|
| source | every `Flux_RenderEnvironmentView` present in the frame's render world (`Flux_RenderWorldSection::m_axEnvironmentViews`), sections in ascending world-tag order, records in written order; ENV writes a record only for a submerged view (`struct Flux_RenderEnvironmentView { Flux_ViewMediumOverride m_xMedium; uint64 m_ulWaterBodyKey; bool m_bWaterline; }`, at most `uFLUX_ENV_MAX_SUBMERGED_VIEWS` = 16 per world), so presence is submersion and no field is tested for it |
| conversion | the record's `m_xMedium` copied whole into one `Flux_ViewMediumOverride` (its `m_ulViewID`, `m_xScattering`, `m_xAbsorption` and `m_fSurfaceHeightMetres`; ENV has already multiplied the coefficients by its `underwaterFogDensityScale`); `m_ulWaterBodyKey` and `m_bWaterline` serve ENV's composite and are not read; a record whose coefficients are negative or non-finite, or whose surface height is non-finite, is refused, logged once per view (`flux.lighting`, `view <id>: invalid underwater medium`) and counted in `Flux.Lights.Invalid` |
| capacity | at most `uFLUX_MAX_VIEW_MEDIUM_OVERRIDES` = 16 per frame across every world, in the frame arena, published as `Flux_LightingFrame::m_axViewMediumOverrides`; ENV's cap is per world, so with several worlds the frame keeps the first 16 accepted records in world-tag then written order, and each subsequent one gets no override, is logged once per view and counted in `Flux.Fog.OverridesDropped` |
| no ENV | a world without the Environment module writes no record, so no view has an override |

| Field | Meaning |
|---|---|
| `m_ulViewID` | the `m_ulValue` of the submerged view's `Flux_ViewID` |
| `m_xScattering`, `m_xAbsorption` | the water body's per-channel coefficients per metre |
| `m_fSurfaceHeightMetres` | the water surface's world height at the camera |

| Rule | Value |
|---|---|
| selection | when view v's fog parameters are filled on the build node, the override whose `m_ulViewID` equals `v.m_xViewID.m_ulValue` (a linear scan of at most 16); none: the height fog |
| medium | while an override for the view exists, the froxel injection of that view uses the homogeneous medium `σ_s = m_xScattering`, `σ_a = m_xAbsorption` with an isotropic phase instead of the height fog; Rayleigh, Mie and ozone are excluded for froxels below `m_fSurfaceHeightMetres`, and local fog volumes still add |
| sun and moon | the sun and moon terms are attenuated by the water column above the froxel, `exp(−(σ_s + σ_a) × max(0, surfaceHeight − y))` per channel |
| GPU record | `Flux_MediumOverrideGPU` ("GPU parameter records") with `m_uEnabled` 1 when an override was selected, and the plain-data ratio `m_xMediumExtinctionRatio` of "Integration" |
| beyond F | the fog volume ends at F; ENV's `Env_AddUnderwater` composite adds only the part beyond F, as ENV's section states |
| transitions | an override appearing or disappearing is not a history invalidation; the fog history's disocclusion by range (step 2 of "Temporal reprojection") and the 0.9 history weight settle it within 20 frames |

## Local fog volumes

Stage 5, from ENV's local fog volume components (`Flux_AtmosphereBlock::m_axLocalFogVolumes`, at most `uFLUX_MAX_LOCAL_FOG_VOLUMES` = 64 per world, the nearest 64 to the main camera kept by the extractor).

| Field | Meaning |
|---|---|
| shape | box or ellipsoid, a world-from-local `Zenith_Matrix4` (unit cube or unit sphere in local space) |
| density, height falloff, albedo, anisotropy, emission | as the height fog, in local space (falloff along local +Y) |
| edge fade | a fraction of the local extent over which density falls linearly to 0 |

The injection loops over the volumes whose world bounds intersect the froxel tile column's bounds (a per-view list of at most `render.quality.fogLocalVolumes`, at most 64, built on the CPU; `Flux_AddVolumetricFog` declares its `112 × n` bytes with `DeclareFrameData` at setup and `WriteFrameData` writes them at order 5, `Zenith2/Docs/Design/LIGHT.md → "Build-node upload data"`) and adds their media; with no local volume the loop is skipped.

## Flux_Atmosphere header

```cpp
// Zenith2/Engine/ZenithFlux/Flux_Atmosphere.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithFlux/Flux_LightSettings.h"
#include "ZenithFlux/Flux_RenderGraph.h"

inline constexpr uint32 uFLUX_MAX_LOCAL_FOG_VOLUMES = 64;
inline constexpr uint32 uFLUX_MAX_VIEW_MEDIUM_OVERRIDES = 16;   // per frame, "Underwater medium override"

/// @contract functions Flux_AtmosphereFunctions
/// Ownership: Feature functions that add passes to the graph they receive.
/// Thread affinity: The Flux.BuildGraph node.
/// Invalidation: The returned handles are valid for the current graph only.
/// Units: Metres; pre-exposed radiance.
/// Errors: No pass is added when the feature bit of the view is clear or the tier disables the feature; parameter
///   validation happens at extraction (FLUX_LIGHTING_ERROR_INVALID_ENVIRONMENT, values replaced by the defaults).
/// Version: since Z2 Stage 3; the star, cloud and underwater hooks since Z2 Stage 4; local fog volumes and the cloud
///   shadow since Z2 Stage 5.
/// @endcontract
Flux_RGTexture Flux_AddVolumetricFog(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, const Flux_ViewTargets& xTargets);
void Flux_AddSkyAtmosphere(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_ViewTargets& xTargets);
void Flux_AddSkyLUTs(Flux_RenderGraph& xGraph, const Flux_RenderLightingEnvironment& xEnvironment, const Flux_ViewList& xViews,
	Flux_FrameShared& xShared);   // once per frame; writes m_xTransmittanceLUT and the MS LUT while a rebuild is pending, and m_xSkyViewLUT

/// @contract functions Flux_AtmosphereReference
/// Ownership: Pure CPU functions, used by the unit tests, the environment cook and ENV's gameplay ground illuminance.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: Metres; transmittance dimensionless; radiance in nits (not pre-exposed).
/// Errors: None; altitudes are clamped to [0, R_t − R_g].
/// Version: since Z2 Stage 3.
/// @endcontract
Zenith_Vector3 Flux_ComputeAtmosphereTransmittance(const Flux_AtmosphereParams& xParams, float fAltitudeMetres,
	float fCosZenith, uint32 uSteps);
Zenith_Vector3 Flux_ComputeMultiScatterReference(const Flux_AtmosphereParams& xParams, float fAltitudeMetres,
	float fCosSunZenith, uint32 uDirections, uint32 uSteps);   // Ψ_ms in double internally
Zenith_Vector3 Flux_ComputeSkyRadianceReference(const Flux_AtmosphereParams& xParams, const Flux_FogMediumParams& xFog,
	Zenith_Vector3 xCameraPositionWS, Zenith_Vector3 xDirectionWS, Zenith_Vector3 xSunDirectionWS, Zenith_Vector3 xSunIlluminance,
	uint32 uSteps);   // single integration from the camera to the top of the atmosphere, atmosphere and height fog together
float Flux_FroxelSliceDepth(uint32 uSlice, uint32 uSliceCount, float fNearMetres, float fFarMetres);
float Flux_FroxelSliceCoordinate(float fLinearDepth, uint32 uSliceCount, float fNearMetres, float fFarMetres);
```

### GPU parameter records

<!-- elaboration -->
The media parameters of the fog pass and of every lighting pass travel as `[FluxShared]` structs. [Z2-N-REND-136] admits a `[FluxShared]` declaration only in its assigned module under `Zenith2/Shaders/Common/`, so the four structs are declared in the new module `Zenith2/Shaders/Common/Media.slang`, whose row of the shared-struct module table (`Zenith2/Docs/Rendering/ShaderSystem.md → "Shared GPU structs"`: "`Media.slang` | `Flux_AtmosphereGPU`, `Flux_FogMediumGPU`, `Flux_MediumOverrideGPU`, `Flux_LocalFogVolumeGPU`") is part of X148's payload (`Zenith2/Docs/Design/LIGHT.md → "Rendering table additions pending X103"`). `Zenith2/Shaders/Lighting/Fog.slang` and `Atmosphere.slang` import the module and declare no shared struct of their own. The build node fills them from `Flux_AtmosphereParams`, `Flux_FogMediumParams`, the view's selected `Flux_ViewMediumOverride` and `Flux_LocalFogVolume`:

| Struct | Offset | Size | Field |
|---|---|---|---|
| `Flux_AtmosphereGPU` (64 B) | 0 | 12 | `float3 m_xRayleighScattering` (β_R, per metre) |
| | 12 | 4 | `float m_fRayleighScaleHeight` (H_R) |
| | 16 | 12 | `float3 m_xOzoneAbsorption` (α_O) |
| | 28 | 4 | `float m_fMieScattering` (β_M) |
| | 32 | 4 | `float m_fMieAbsorption` (α_M) |
| | 36 | 4 | `float m_fMieScaleHeight` (H_M) |
| | 40 | 4 | `float m_fMieAnisotropy` (g_M) |
| | 44 | 4 | `float m_fGroundHeight` (metres, the ground plane's y) |
| | 48 | 4 | `float m_fPlanetRadius` (R_g) |
| | 52 | 4 | `float m_fTopRadius` (R_t = R_g + atmosphere height) |
| | 56 | 4 | `uint m_uEnabled` (`Flux_AtmosphereParams::m_bEnabled`: 0 or 1) |
| | 60 | 4 | reserved, zero |
| `Flux_FogMediumGPU` (48 B) | 0 | 12 | `float3 m_xAlbedo` |
| | 12 | 4 | `float m_fDensity` (0 when the height fog is disabled) |
| | 16 | 12 | `float3 m_xEmission` (nits) |
| | 28 | 4 | `float m_fHeightFalloff` |
| | 32 | 4 | `float m_fBaseHeight` |
| | 36 | 4 | `float m_fAnisotropy` |
| | 40 | 8 | reserved, zero |
| `Flux_MediumOverrideGPU` (48 B) | 0 | 12 | `float3 m_xScattering` |
| | 12 | 4 | `uint m_uEnabled` (0 or 1) |
| | 16 | 12 | `float3 m_xAbsorption` |
| | 28 | 4 | `float m_fSurfaceHeight` |
| | 32 | 16 | reserved, zero |
| `Flux_LocalFogVolumeGPU` (112 B) | 0 | 64 | `float4x4 m_xLocalFromWorld` (the inverse of the volume's world-from-local) |
| | 64 | 12 | `float3 m_xAlbedo` |
| | 76 | 4 | `float m_fDensity` |
| | 80 | 12 | `float3 m_xEmission` |
| | 92 | 4 | `float m_fHeightFalloff` |
| | 96 | 4 | `float m_fEdgeFade` |
| | 100 | 4 | `float m_fAnisotropy` |
| | 104 | 4 | `uint m_uShape` (0 box, 1 ellipsoid) |
| | 108 | 4 | reserved, zero |

Each struct gets the generated size and offset asserts of `Flux_SharedTypes.generated.h` (`Zenith2/Docs/Rendering/ShaderSystem.md → "Shared GPU structs"`); `Test_LightingSharedTypes_Layout` asserts the tables above. The ozone layer's centre (25,000 m) and half width (15,000 m) are constants of "Media model", not fields. `Flux_AtmosphereGPU` is the plain-data field `m_xAtmosphere` of every lighting pass and of `VolumetricFog`, from which `Flux_LightingAtmosphere` is built (`Zenith2/Docs/Design/LIGHT/Lighting.md → "The EvaluateLighting contract"`).

`Flux_AddVolumetricFog` and `Flux_AddSkyAtmosphere` keep Rendering's signatures; `Flux_AddSkyLUTs` is the shared producer that Rendering's frame puts inside `Flux_AddSkyAtmosphere`'s first call of the frame (assumption pending RPROD, row L9, item (f) of X103). The implementation lives in `Zenith2/Engine/ZenithFlux/Internal/Passes/Fog/` and `Internal/Passes/Sky/`; the shader functions `Flux_SampleSunTransmittance` and `Flux_SampleCloudShadow` live in `Zenith2/Shaders/Lighting/Atmosphere.slang` and take the `Flux_LightingAtmosphere` of `Zenith2/Docs/Design/LIGHT/Lighting.md → "The EvaluateLighting contract"`.
