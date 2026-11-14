# LIGHT requirement catalogue

## Purpose

This catalogue breaks the LIGHT inventory items (`Zenith2/Docs/Requirements/Inventory.md → "Inventory v1"`, section "LIGHT Lighting & image quality") into testable requirements `Z2-R-LIGHT-NNN`. Each requirement names the inventory items it covers, the stage whose acceptance it serves, its classification, its governing decisions and budget rows, the artefact that proves it, and the headings of the LIGHT design family (`Zenith2/Docs/Design/LIGHT.md` and its section files) that specify it; the design entry's "Requirement index" is the reverse map. LIGHT owns every requirement here; where another area supplies part of the evidence (RPROD's frame functions, view list, pressure hook and parity tooling, RHI's GPU scene and upload ring, ADVGFX's path tracer, GAME's camera extractor, PLAT's thermal adaptation and configuration layers, ENV's atmosphere, clouds, water and per-view records, WORLD's bake), the area is named in the statement or evidence and in `Zenith2/Docs/Design/LIGHT.md → "Dependencies"`. A requirement whose realisation waits for a core-document change cites the pending cross-doc item (X80, X103 to X108, X125, X148) in its statement; a requirement whose scope waits for a human ruling cites it (Q41, X106).

## Requirements

### Z2-R-LIGHT-001 LIGHT code lives in ZenithFlux and the lighting shader modules

- **Statement:** LIGHT adds no library: its runtime code lives in `Zenith2/Engine/ZenithFlux/` (public headers in the root, passes under `Internal/Passes/<Pass>/`, subsystems under `Internal/Lighting/`), its shaders under `Zenith2/Shaders/Lighting/` and `Zenith2/Shaders/Passes/`, its cook steps in ZenithCook, and no LIGHT source sits under a platform directory.
- **Covers:** Z2-I-LIGHT-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-001, Z2-D-055
- **Budgets:** none
- **Acceptance evidence:** `SourceLayout`, `CompileCoverage` and `Libgraph` gate steps on the API-only ticket; review of the file list against "ZenithFlux LIGHT files".
- **Design:** Zenith2/Docs/Design/LIGHT.md → "ZenithFlux LIGHT files"; Zenith2/Docs/Design/LIGHT.md → "Shader modules and passes"; Zenith2/Docs/Design/LIGHT.md → "Target summary"

### Z2-R-LIGHT-002 LIGHT public headers carry contracts and are self-contained

- **Statement:** Every public type of the LIGHT headers carries its own contract block (the enumerations, `Flux_CPULight`, `Flux_LightDeltaOp`, `Flux_RenderProbeKey`, `Flux_ProbeCaptureMode`, `Flux_ShadowViewKind`, `Flux_OutputMode`, `Flux_ViewGpuTime`, `Flux_LightingSystemDesc` and the cook's baked-data structs included), every fallible function returns `Zenith_Result<…, Flux_LightingError>`, and each header includes what it names (`ZenithCore/Zenith_Types.h` for the `uint` types), compiles alone and includes no third-party, backend or `Internal/` header.
- **Covers:** Z2-I-LIGHT-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045, Z2-D-047
- **Budgets:** none
- **Acceptance evidence:** `ApiContractCheck` and `HeaderSelfContained` gate steps over the LIGHT headers.
- **Design:** Zenith2/Docs/Design/LIGHT.md → "Header map"; Zenith2/Docs/Design/LIGHT.md → "Rules across the LIGHT API"; Zenith2/Docs/Design/LIGHT.md → "Flux_LightingErrors header"; Zenith2/Docs/Design/LIGHT/Temporal.md → "Flux_Upscaler header"

### Z2-R-LIGHT-003 API-only slice lands first

- **Statement:** The first LIGHT implementation ticket lands every LIGHT header with its declarations, contract blocks, settings structs and feature functions whose bodies add no pass, so RPROD frame functions and dependent LIGHT tickets build against the API before any pass exists.
- **Covers:** Z2-I-LIGHT-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-055
- **Budgets:** none
- **Acceptance evidence:** The API-only ticket passes `Build`, `ApiContractCheck` and `HeaderSelfContained`; dependent tickets are `blocks`-linked from it.
- **Design:** Zenith2/Docs/Design/LIGHT.md → "API-only slices and ordering"; Zenith2/Docs/Design/LIGHT.md → "Header map"

### Z2-R-LIGHT-004 Feature functions keep Rendering signatures

- **Statement:** Every LIGHT feature function that Rendering names (`Flux_AddLightDecalList`, `Flux_AddLightClustering`, `Flux_AddVolumetricFog`, `Flux_AddSSAO`, `Flux_AddContactShadows`, `Flux_AddShadowView`, `Flux_AddDeferredLighting`, `Flux_AddSkyAtmosphere`, `Flux_AddSSR`, `Flux_AddTemporalAA`, `Flux_AddAutoExposure`, `Flux_AddDepthOfField`, `Flux_AddMotionBlur`, `Flux_AddBloom`, `Flux_AddTonemap`) has exactly the Rendering per-pass signature, and each adds no pass when its view feature bit, tier or capability disables it.
- **Covers:** Z2-I-LIGHT-01, Z2-I-LIGHT-07, Z2-I-LIGHT-09, Z2-I-LIGHT-12, Z2-I-LIGHT-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-055, Z2-D-035
- **Budgets:** none
- **Acceptance evidence:** `Test_Paths_DisabledFeatureAddsNoPass` extended to every LIGHT feature; compilation of RPROD frame functions against the headers.
- **Design:** Zenith2/Docs/Design/LIGHT.md → "Rules across the LIGHT API"; Zenith2/Docs/Design/LIGHT/Lighting.md → "Flux_Lights header"; Zenith2/Docs/Design/LIGHT/Shadows.md → "Flux_Shadows header"

### Z2-R-LIGHT-005 Settings reach features through the view context

- **Statement:** LIGHT feature functions read their quality settings from `Flux_ViewContext::m_pxQuality->m_xLight` (a `Flux_LightQualitySettings`, the member RPROD's `Flux_QualitySettings` carries), authored environment settings from `Flux_RenderLightingEnvironment` and camera settings from `Flux_CameraLightingSettings`, never from configuration or globals; a feature runs only when its `Flux_ViewFeature` bit is set, and `Flux_AddSSAO` takes its `Flux_SSAOSettings` argument.
- **Covers:** Z2-I-LIGHT-08, Z2-I-LIGHT-12, Z2-I-LIGHT-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** Review; `ForbiddenCheck` writable-data scan; `Test_LightSettings_ViewOverridesApplied` (an editor view override changes only that view's copy of `m_xLight`).
- **Design:** Zenith2/Docs/Design/LIGHT.md → "Flux_LightSettings header"; Zenith2/Docs/Design/LIGHT.md → "Rules across the LIGHT API"

### Z2-R-LIGHT-006 LIGHT quality keys registered under render.quality

- **Statement:** `Flux_RegisterLightConfigKeys`, called once from RPROD's `Zenith_RegisterFluxConfigKeys`, registers every LIGHT key of "Flux_LightSettings header" with its type, default, range and allowed layers (the froxel grid as the three `CONFIG_VALUE_TYPE_INT` keys `render.quality.froxelGridX`, `…Y` and `…Z`, because the store has no three-component type) (every `render.quality.*` key allows every layer including the quality tier and is not restart-required; `r.output.mode` and `r.hdr.*` exclude the quality tier; `r.exposure.fixedDeltaSeconds` and `r.debug.lighting` allow the engine default and the command line only), and `Flux_ReadLightQualitySettings` fills `Flux_QualitySettings::m_xLight`, returning `FLUX_LIGHTING_ERROR_INVALID_SETTINGS` and keeping the previous values of a sub-struct whose rule is violated.
- **Covers:** Z2-I-LIGHT-08, Z2-I-LIGHT-11
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_LightConfig_RegisterAllKeys`, `Test_LightConfig_TierFileApplied`, `Test_LightConfig_InvalidValueKeepsDefault`, `Test_LightConfig_FroxelGridKeys`, `Test_LightConfig_FroxelGridRead`.
- **Design:** Zenith2/Docs/Design/LIGHT.md → "Flux_LightSettings header"; Zenith2/Docs/Design/LIGHT.md → "Configuration keys"; Zenith2/Docs/Design/LIGHT/QualityTiers.md → "Tier values and configuration"

### Z2-R-LIGHT-007 Histories live in per-view persistent state

- **Statement:** Every LIGHT history (TAA or upscaler colour, exposure, SSR previous colour, velocity, froxel scattering, SSGI) lives in the view's `Flux_ViewPersistentState` keyed by its view ID, and no LIGHT history is shared between views or kept elsewhere.
- **Covers:** Z2-I-LIGHT-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-059
- **Budgets:** Z2-B-RHI-001
- **Acceptance evidence:** `Test_View_EditorHistoryIndependentOfMain` extended with LIGHT histories (`Test_TAA_EditorViewIndependence`).
- **Design:** Zenith2/Docs/Design/LIGHT/Temporal.md → "Histories LIGHT owns"

### Z2-R-LIGHT-008 First-write rule for histories

- **Statement:** A history resource is cleared in the frame it is created, a consumer with an invalid history leaves its previous-history field unset and takes the no-history branch, and every history write covers the whole current half, so no undefined history memory reaches an output; a history's validity advances only on a submitted frame.
- **Covers:** Z2-I-LIGHT-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-057, Z2-D-059
- **Budgets:** none
- **Acceptance evidence:** `Test_History_FirstFrameNoRead`, `Test_TAA_FirstFrameNoHistoryRead` and `Test_Fog_FirstFrameNoHistoryRead` (graph inspection shows no previous-half read on the first frame).
- **Design:** Zenith2/Docs/Design/LIGHT/Temporal.md → "The first-write rule for histories"; Zenith2/Docs/Design/LIGHT.md → "History validity state machine"; Zenith2/Docs/Design/LIGHT.md → "Deferred and failed frames"

### Z2-R-LIGHT-009 History invalidation events

- **Statement:** A view's histories are invalidated exactly on the events of "History invalidation" (first frame, camera cut, output-extent change, shading-path change, a tier change of a history's format or extent for that history, device-loss recovery, memory-pressure action 12 for capture and preview views).
- **Covers:** Z2-I-LIGHT-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-059
- **Budgets:** none
- **Acceptance evidence:** `Test_HistoryPolicy_InvalidationEvents`, `Test_HistoryPolicy_QualityAndScaleEvents`, `Test_View_CameraCutResetsHistory`.
- **Design:** Zenith2/Docs/Design/LIGHT/Temporal.md → "History invalidation"; Zenith2/Docs/Design/LIGHT.md → "History validity state machine"

### Z2-R-LIGHT-010 Dynamic-resolution steps keep output-extent histories

- **Statement:** A dynamic-resolution scale change invalidates none of the output-extent histories (TAA, exposure, fog), and render-extent histories are resampled by normalised UV in the frame after the change, the narrowed rule of cross-doc item X104 (applied by core-doc batch 4).
- **Covers:** Z2-I-LIGHT-08, Z2-I-LIGHT-11
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-059
- **Budgets:** none
- **Acceptance evidence:** `Test_TAA_DynamicResolutionKeepsHistory`, `Test_HistoryPolicy_InvalidationEvents`, `Test_HistoryPolicy_QualityAndScaleEvents`.
- **Design:** Zenith2/Docs/Design/LIGHT/Temporal.md → "Dynamic resolution and history validity"

### Z2-R-LIGHT-011 History ping-pong writes are marked outputs

- **Statement:** Every LIGHT history is written into the current half of its ping-pong pair, marked with `MarkOutput`, and read from the previous half in the next frame.
- **Covers:** Z2-I-LIGHT-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-057
- **Budgets:** none
- **Acceptance evidence:** Graph unit test `Test_History_PingPongMarkedOutput`, `Test_TAA_HistoryPingPong`, `Test_Fog_HistoryPingPong` and `Test_Exposure_HistoryPingPong` (the history write pass is never culled; the next frame reads the other half).
- **Design:** Zenith2/Docs/Design/LIGHT/Temporal.md → "Histories LIGHT owns"

### Z2-R-LIGHT-012 Teleport of the camera is a camera cut

- **Statement:** A teleport of the entity owning the active camera, or of an ancestor, produces a camera cut for that frame's view request, invalidating the view's TAA, fog and exposure histories.
- **Covers:** Z2-I-LIGHT-08, Z2-I-LIGHT-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080, Z2-D-059
- **Budgets:** none
- **Acceptance evidence:** `Test_TAA_TeleportCameraCutInvalidatesHistory` (with GAME's camera extractor).
- **Design:** Zenith2/Docs/Design/LIGHT/Temporal.md → "History invalidation"

### Z2-R-LIGHT-013 Teleported instances produce zero velocity and no smear

- **Statement:** An instance in a section's history-reset list has zero motion that frame, and TAA and motion blur show no trail from the jump.
- **Covers:** Z2-I-LIGHT-08, Z2-I-LIGHT-09, Z2-I-LIGHT-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_TAA_TeleportedInstanceNoSmear`.
- **Design:** Zenith2/Docs/Design/LIGHT/Temporal.md → "History invalidation"; Zenith2/Docs/Design/LIGHT/PostProcess.md → "Motion blur"

### Z2-R-LIGHT-014 Halton jitter sequence

- **Statement:** Every jittered view uses the Halton (2, 3) sequence with period 8 for TAA and the ratio rule for the upscaler, applied only through `m_xJitteredProjection`, with velocity from the unjittered matrices and the sequence restarting on a camera cut; the offset reaches RPROD's view-list construction only through `Flux_LightingSystem::GetJitterOffsetPixels`, computed from the view's committed submitted-frame counter, so a frame that submits nothing repeats its index.
- **Covers:** Z2-I-LIGHT-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-035
- **Budgets:** Z2-B-LIGHT-010
- **Acceptance evidence:** `Test_Jitter_Halton23`, `Test_Jitter_PeriodForRatio`, `Test_Jitter_OffsetAccessor`.
- **Design:** Zenith2/Docs/Design/LIGHT/Temporal.md → "Jitter sequence"; Zenith2/Docs/Design/LIGHT.md → "Flux_Lighting header"

### Z2-R-LIGHT-015 Variance clipping in weighted YCoCg

- **Statement:** The TAA resolve clips the history toward the 3 × 3 neighbourhood mean into the box mean ± γσ in luminance-weighted YCoCg, with the γ of its quality level.
- **Covers:** Z2-I-LIGHT-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-035
- **Budgets:** Z2-B-LIGHT-010
- **Acceptance evidence:** `Test_TAA_MovingObjectNoGhosting`; `LightingTemporal` parity scene.
- **Design:** Zenith2/Docs/Design/LIGHT/Temporal.md → "TAA resolve"

### Z2-R-LIGHT-016 Velocity dilation and bicubic reprojection

- **Statement:** The resolve reprojects with the velocity of the front-most neighbourhood sample and samples the history with a 5-tap Catmull-Rom filter.
- **Covers:** Z2-I-LIGHT-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-035
- **Budgets:** Z2-B-LIGHT-010
- **Acceptance evidence:** `Test_TAA_StaticConvergence`, `Test_TAA_MovingObjectNoGhosting`.
- **Design:** Zenith2/Docs/Design/LIGHT/Temporal.md → "TAA resolve"

### Z2-R-LIGHT-017 Disocclusion rejects history

- **Statement:** A reprojected position outside the screen, an invalid history, or a velocity-history mismatch above the threshold sets the current weight to 1 for that pixel.
- **Covers:** Z2-I-LIGHT-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-059
- **Budgets:** Z2-B-LIGHT-010
- **Acceptance evidence:** `Test_TAA_DisocclusionRejectsHistory`.
- **Design:** Zenith2/Docs/Design/LIGHT/Temporal.md → "TAA resolve"

### Z2-R-LIGHT-018 Anti-flicker weighting and NaN sanitisation

- **Statement:** The resolve weights samples by `1 / (1 + Y)` and replaces non-finite history or current values, counting them in `Flux.Temporal.HistoryNaN`.
- **Covers:** Z2-I-LIGHT-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-035
- **Budgets:** none
- **Acceptance evidence:** `Test_History_NaNSanitised`.
- **Design:** Zenith2/Docs/Design/LIGHT/Temporal.md → "TAA resolve"; Zenith2/Docs/Design/LIGHT/Temporal.md → "The first-write rule for histories"

### Z2-R-LIGHT-019 Temporal upscale to output extent

- **Statement:** With render and output extents different, `Flux_AddTemporalAA` runs the upscale variant: Lanczos-2 weighted current samples, confidence-weighted blending, the ratio-dependent jitter period, output and history at output extent.
- **Covers:** Z2-I-LIGHT-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-035
- **Budgets:** Z2-B-LIGHT-010
- **Acceptance evidence:** `Test_Upscale_ConvergenceAtFloor`.
- **Design:** Zenith2/Docs/Design/LIGHT/Temporal.md → "Temporal upscale"

### Z2-R-LIGHT-020 Upscaler is a quality-tier setting

- **Statement:** The upscaler is `Flux_UpscalerMode`, the one upscaler enumeration of Zenith2, selected per tier by `render.quality.upscaler` (`taa` = `FLUX_UPSCALER_TAA`, `taau` = `FLUX_UPSCALER_TAAU`); `external` (`FLUX_UPSCALER_EXTERNAL`) is refused with `FLUX_LIGHTING_ERROR_INVALID_SETTINGS` until D-SEL-UPSCALER (open, Stage 5) is ruled, and the value 0 is never valid.
- **Covers:** Z2-I-LIGHT-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-035, D-SEL-UPSCALER (open, Stage 5)
- **Budgets:** Z2-B-LIGHT-010
- **Acceptance evidence:** `Test_LightConfig_ExternalUpscalerRejected`, `Test_LightConfig_UpscalerValues`; the tier files.
- **Design:** Zenith2/Docs/Design/LIGHT/Temporal.md → "Upscaler selection"; Zenith2/Docs/Design/LIGHT/Temporal.md → "Flux_Upscaler header"; Zenith2/Docs/Design/LIGHT/QualityTiers.md → "Temporal and resolution tiers"

### Z2-R-LIGHT-021 Temporal convergence and stability sequences

- **Statement:** On the `LightingTemporal` scene TAA converges within FLIP 0.02 of a 16× supersampled reference after 32 frames, and a moving bright object leaves less than 2% of its luminance behind after 4 frames.
- **Covers:** Z2-I-LIGHT-09, Z2-I-LIGHT-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-063
- **Budgets:** Z2-B-LIGHT-010
- **Acceptance evidence:** `Test_TAA_StaticConvergence`, `Test_TAA_MovingObjectNoGhosting` in the `Test` step; the `LightingTemporal` backend parity scene.
- **Design:** Zenith2/Docs/Design/LIGHT/Tests.md → "Temporal sequence tests"; Zenith2/Docs/Design/LIGHT/Tests.md → "Parity scenes"

### Z2-R-LIGHT-022 Dynamic resolution follows GPU frame time

- **Statement:** The dynamic-resolution controller of each eligible view smooths the GPU frame time of its newest completed frame (EMA 0.15) and computes the desired per-axis scale `s × √(target / t̄)`.
- **Covers:** Z2-I-LIGHT-11
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-SHIP-011
- **Acceptance evidence:** `Test_DynRes_StepLimits`, `Test_DynRes_FloorAndCap`.
- **Design:** Zenith2/Docs/Design/LIGHT/Temporal.md → "Dynamic-resolution controller"

### Z2-R-LIGHT-023 Scale bounded by the 75% floor and the cap

- **Statement:** The scale never goes below `max(Z2-B-SHIP-011, render.quality.dynamicResolutionMin)` (the floor row is 0.75 per axis) nor above `min(render.quality.dynamicResolutionMax, thermal cap)`, and a run at the floor that misses its target is reported, never rescued by going lower.
- **Covers:** Z2-I-LIGHT-11
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-SHIP-011
- **Acceptance evidence:** `Test_DynRes_FloorAndCap`, `Test_DynRes_RangeFromSettings`; budget-run evidence `framesAtFloor`.
- **Design:** Zenith2/Docs/Design/LIGHT/Temporal.md → "Dynamic-resolution controller"; Zenith2/Docs/Design/LIGHT/Temporal.md → "Thermal and pressure inputs"

### Z2-R-LIGHT-024 Step limits, hysteresis and quantisation

- **Statement:** One update changes the scale by at most 5%, decreases need 2 frames over target and increases 8 frames under 0.9 × target, and every scale is a multiple of 0.01.
- **Covers:** Z2-I-LIGHT-11
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** `Test_DynRes_Hysteresis`, `Test_DynRes_Quantised`, `Test_DynRes_InvalidSamplesIgnored`.
- **Design:** Zenith2/Docs/Design/LIGHT/Temporal.md → "Dynamic-resolution controller"

### Z2-R-LIGHT-025 Thermal cap and tier offset drive resolution

- **Statement:** `Flux_DynamicResolutionController::SetThermalCap`, called by the runtime's thermal adaptation target on the main thread, caps the scale from the next update (clamped to the floor), and a thermal tier change applies its LIGHT tier values at the next frame, touching no simulation state.
- **Covers:** Z2-I-LIGHT-11, Z2-I-PLAT-29
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-SHIP-011
- **Acceptance evidence:** `Test_DynRes_FloorAndCap` (cap cases), `Test_DynRes_ThermalCapThroughRuntime`, `Test_ThermalCoupling_ResolutionNeverBelowFloor`, `Test_ThermalCoupling_TierChangeTouchesNoSimulation`; PLAT's `Test_Thermal_ResolutionNeverBelowFloor` and `Test_Thermal_TierChangeTouchesNoSimulation`; the S24 thermal device run.
- **Design:** Zenith2/Docs/Design/LIGHT/Temporal.md → "Thermal and pressure inputs"

### Z2-R-LIGHT-026 Critical memory pressure forces the floor

- **Statement:** While the memory governor reports critical pressure the controller holds the floor scale; the pressure reaches it only through RPROD's pressure hook calling `SetPressureFloor(true)` on the build node for every view with `HasDynamicResolution` (RPROD's "Quality tier selection" step 4), `UpdateDynamicResolution` never writes the flag, and the controller resumes its normal steps one frame after `SetPressureFloor(false)` (Architecture shedding action 11).
- **Covers:** Z2-I-LIGHT-11
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_DynRes_PressureForcesFloor`, `Test_Lighting_CriticalPressureShedding`.
- **Design:** Zenith2/Docs/Design/LIGHT/Temporal.md → "Thermal and pressure inputs"

### Z2-R-LIGHT-027 Dynamic-resolution telemetry

- **Statement:** The controller publishes `Flux.DynRes.ScalePercent`, `Flux.DynRes.FramesAtFloor` and `Flux.DynRes.ThermalCapPercent` every frame, from which budget runs fill `dynresHistogram` and `framesAtFloor`.
- **Covers:** Z2-I-LIGHT-11
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-SHIP-011
- **Acceptance evidence:** Counter test `Test_DynRes_Counters`; a budget-run evidence record.
- **Design:** Zenith2/Docs/Design/LIGHT/Temporal.md → "Dynamic-resolution telemetry"

### Z2-R-LIGHT-028 Workload runs fix dynamic-resolution settings

- **Statement:** LIGHT's part of a budget run's fixed graphics settings ([Z2-N-BUDGET-122]): every LIGHT quality value, the dynamic-resolution enablement, target and upscaler included, is a `render.quality.*` key read only through `Flux_ReadLightQualitySettings`, so command-line-layer values of every such key, which the Player's budget-run mode receives as `--set` arguments from the workload definition (row L41, PLAT's command-line layer, no workload layer), fully determine `Flux_QualitySettings::m_xLight` whatever the user layer holds.
- **Covers:** Z2-I-LIGHT-11
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** `Test_LightConfig_CommandLineLayerDeterminesQuality`; the budget-run mode's own test once its owner gap (row L41) is routed.
- **Design:** Zenith2/Docs/Design/LIGHT/Temporal.md → "Dynamic-resolution controller"; Zenith2/Docs/Design/LIGHT.md → "Configuration keys"

### Z2-R-LIGHT-029 Luminance histogram of absolute luminance

- **Statement:** The histogram pass bins `log2` luminance over [−12, 18] into 256 bins from the TAA output divided by the frame's pre-exposure, with integer atomics so the result is identical every run.
- **Covers:** Z2-I-LIGHT-12
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-035
- **Budgets:** Z2-B-LIGHT-011
- **Acceptance evidence:** `Test_Exposure_HistogramDeterministic`.
- **Design:** Zenith2/Docs/Design/LIGHT/PostProcess.md → "Luminance histogram"

### Z2-R-LIGHT-030 Metering and percentile average

- **Statement:** The adapt pass meters with the selected mode, discards the configured low and high percentiles, and computes the target EV100 clamped to the world's range, keeping the previous target for an empty histogram.
- **Covers:** Z2-I-LIGHT-12
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-LIGHT-011
- **Acceptance evidence:** `Test_Exposure_MeterHistogramReference` against `Flux_MeterHistogramEV100`.
- **Design:** Zenith2/Docs/Design/LIGHT/PostProcess.md → "Metering"

### Z2-R-LIGHT-031 Exponential adaptation with reproducible time step

- **Statement:** Exposure adapts exponentially with separate brightening and darkening speeds, the time step is the clamped wall-clock interval unless `r.exposure.fixedDeltaSeconds` sets it, and an invalid history snaps to the target.
- **Covers:** Z2-I-LIGHT-12
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-059
- **Budgets:** Z2-B-LIGHT-011
- **Acceptance evidence:** `Test_Exposure_AdaptCurve`, `Test_Exposure_SnapOnInvalidHistory`, `Test_Exposure_TemporalAdaptation`.
- **Design:** Zenith2/Docs/Design/LIGHT/PostProcess.md → "Adaptation"

### Z2-R-LIGHT-032 Exposure buffer and consistent pre-exposure

- **Statement:** The 16-byte exposure buffer holds the multiplier, current and target EV100 and flags; every render-extent pass uses the frame's read-back pre-exposure, the histogram divides it out, and tonemap and bloom apply the current GPU exposure over it.
- **Covers:** Z2-I-LIGHT-12
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-059
- **Budgets:** none
- **Acceptance evidence:** `Test_Exposure_PreExposureConsistency` (a scene rendered with two pre-exposures tonemaps identically within 1/255).
- **Design:** Zenith2/Docs/Design/LIGHT/PostProcess.md → "Exposure buffer"; Zenith2/Docs/Design/LIGHT/PostProcess.md → "Pre-exposure pipeline"

### Z2-R-LIGHT-033 Manual and physical camera exposure

- **Statement:** A camera in `FLUX_EXPOSURE_MODE_MANUAL_EV` or `FLUX_EXPOSURE_MODE_PHYSICAL` uses its EV100 (from aperture, shutter and ISO for physical) and adds no histogram pass.
- **Covers:** Z2-I-LIGHT-12
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Exposure_EV100FromCamera`, `Test_Exposure_ManualAddsNoPass`.
- **Design:** Zenith2/Docs/Design/LIGHT/PostProcess.md → "Camera exposure"; Zenith2/Docs/Design/LIGHT/PostProcess.md → "Exposure units"

### Z2-R-LIGHT-034 Capture and preview views use fixed exposure

- **Statement:** Capture views use their probe's fixed EV100 and preview views EV100 10 unless requested otherwise, and neither adds an exposure pass.
- **Covers:** Z2-I-LIGHT-12
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-059
- **Budgets:** none
- **Acceptance evidence:** `Test_Exposure_CaptureFixed`.
- **Design:** Zenith2/Docs/Design/LIGHT/PostProcess.md → "Camera exposure"

### Z2-R-LIGHT-035 Scene-referred Rec.709 working space

- **Statement:** Every LIGHT pass up to the tonemap works in pre-exposed scene-referred linear Rec.709, and the content gamut is Rec.709 on every output.
- **Covers:** Z2-I-LIGHT-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-014
- **Budgets:** none
- **Acceptance evidence:** Review; `Test_Tonemap_OutputModesMatchCPU`.
- **Design:** Zenith2/Docs/Design/LIGHT/ColourManagement.md → "Working colour space"

### Z2-R-LIGHT-036 ACES-style SDR operator

- **Statement:** The SDR tonemap is the ACES fitted operator with exactly the input matrix, rational curve and output matrix of "SDR tonemap operator", saturated to [0, 1].
- **Covers:** Z2-I-LIGHT-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-035
- **Budgets:** Z2-B-LIGHT-015
- **Acceptance evidence:** `Test_Tonemap_ACESFittedReference`, `Test_Tonemap_ACESFittedMonotonic`.
- **Design:** Zenith2/Docs/Design/LIGHT/ColourManagement.md → "SDR tonemap operator"

### Z2-R-LIGHT-037 3D grading LUT application

- **Statement:** Grading applies a 32³ RGBA8 LUT on the shaped display domain `w^(1/2.2)` with texel-centre remapping and trilinear sampling, a camera LUT overriding the world's, a two-LUT blend, and no lookup when grading is disabled.
- **Covers:** Z2-I-LIGHT-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-LIGHT-015
- **Acceptance evidence:** `Test_GradingLUT_IdentityWithinQuantisation`; `LightingPost` parity scene.
- **Design:** Zenith2/Docs/Design/LIGHT/ColourManagement.md → "Grading LUT"

### Z2-R-LIGHT-038 Grading LUT cook from .cube

- **Statement:** `Cook_GradingLUT` converts a `.cube` of size 17 to 65 with a [0, 1] domain to a 32³ `.zlut` by tetrahedral resampling, rejecting malformed input with `COOK_ERROR_LUT_FORMAT`, and a missing or corrupt LUT disables grading with a warning.
- **Covers:** Z2-I-LIGHT-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CubeReader_Grammar`, `Test_GradingLUTFile_RoundTrip`, `Test_GradingLUTFile_RejectsCorrupt`, `Test_Lighting_MissingLUTIdentity`, `Zenith_Fuzz_CubeReader`.
- **Design:** Zenith2/Docs/Design/LIGHT/ColourManagement.md → "Grading LUT cook"

### Z2-R-LIGHT-039 SDR output through the sRGB swapchain

- **Statement:** The SDR output writes display-linear values through the `B8G8R8A8_SRGB` view, whose hardware encode is the only sRGB transfer applied.
- **Covers:** Z2-I-LIGHT-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-014
- **Budgets:** none
- **Acceptance evidence:** `Test_Tonemap_OutputModesMatchCPU` (SDR rows).
- **Design:** Zenith2/Docs/Design/LIGHT/ColourManagement.md → "Output transforms"

### Z2-R-LIGHT-040 HDR10 output

- **Statement:** HDR10 output applies the ACES-style Lottes curve solved for the peak and paper white, converts Rec.709 to Rec.2020 and encodes SMPTE ST 2084 PQ into `A2B10G10R10`, matching the reference values within 1e-5.
- **Covers:** Z2-I-LIGHT-14
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-035
- **Budgets:** Z2-B-LIGHT-015
- **Acceptance evidence:** `Test_Tonemap_LottesProperties`, `Test_PQ_ReferenceValues`, `Test_Tonemap_HDRModesMatchCPU` (HDR10 rows); the HDR10 display device run.
- **Design:** Zenith2/Docs/Design/LIGHT/ColourManagement.md → "HDR tonemap operator"; Zenith2/Docs/Design/LIGHT/ColourManagement.md → "Output transforms"

### Z2-R-LIGHT-041 scRGB output

- **Statement:** scRGB output writes linear Rec.709 nits divided by 80 into `R16G16B16A16_SFLOAT` on Windows.
- **Covers:** Z2-I-LIGHT-14
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-LIGHT-015
- **Acceptance evidence:** `Test_Tonemap_HDRModesMatchCPU` (scRGB rows).
- **Design:** Zenith2/Docs/Design/LIGHT/ColourManagement.md → "Output transforms"

### Z2-R-LIGHT-042 Output mode selection and SDR fallback

- **Statement:** The output mode follows `r.output.mode` with automatic HDR10 when the surface and display support it, an unsupported explicit request falls back to SDR with a warning and a counter, a mode change is a swapchain recreation that invalidates no history, and every mode's tonemap permutation is in the boot PSO group.
- **Covers:** Z2-I-LIGHT-14
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-025
- **Budgets:** none
- **Acceptance evidence:** `Test_OutputMode_Selection`; `Test_Swapchain_*` recreation tests with an HDR toggle.
- **Design:** Zenith2/Docs/Design/LIGHT/ColourManagement.md → "Output mode selection"

### Z2-R-LIGHT-043 HDR calibration and metadata

- **Statement:** Peak, paper-white and UI brightness are configuration values in their ranges, and HDR10 output sets static metadata with Rec.709 primaries, D65, the peak as maximum mastering luminance and MaxCLL, and `min(peak, 400)` as MaxFALL.
- **Covers:** Z2-I-LIGHT-14
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_HDRMetadata_Values`; the HDR10 display device run.
- **Design:** Zenith2/Docs/Design/LIGHT/ColourManagement.md → "HDR calibration and metadata"

### Z2-R-LIGHT-044 Optional vignette and deterministic film grain

- **Statement:** Vignette applies scene-referred before the operator and film grain display-referred after the LUT from a frame-indexed hash, both only when their permutation is enabled, and neither in parity, capture or preview views.
- **Covers:** Z2-I-LIGHT-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-LIGHT-015
- **Acceptance evidence:** `Test_Tonemap_GrainDeterministic`, `Test_Tonemap_VignetteProfile`.
- **Design:** Zenith2/Docs/Design/LIGHT/ColourManagement.md → "Vignette and film grain"

### Z2-R-LIGHT-045 Colour-space conversions match references

- **Statement:** The CPU and GPU colour-space conversions (Rec.709 ↔ XYZ, Rec.709 ↔ Rec.2020, the ACES matrices, PQ) use exactly the constants of "Colour-space matrices" and agree with each other.
- **Covers:** Z2-I-LIGHT-13, Z2-I-LIGHT-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_ColourMatrices_RoundTrip`, `Test_PQ_ReferenceValues`, `Test_Tonemap_OutputModesMatchCPU`, `Test_Tonemap_HDRModesMatchCPU`.
- **Design:** Zenith2/Docs/Design/LIGHT/ColourManagement.md → "Colour-space matrices"

### Z2-R-LIGHT-046 Bloom chain

- **Statement:** Bloom downsamples `render.quality.bloomLevels` levels (6 desktop, 5 mobile, 4 on `QUALITY_TIER_S24_LOW`) with the 13-tap filter and a Karis average on the first level, upsamples with a 3 × 3 tent of the configured radius, and is composited energy-conservingly in the tonemap by its intensity.
- **Covers:** Z2-I-LIGHT-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-LIGHT-014
- **Acceptance evidence:** `LightingPost` parity scene; `Test_Bloom_EnergyConserving` (the composite of a uniform image is unchanged).
- **Design:** Zenith2/Docs/Design/LIGHT/PostProcess.md → "Bloom"

### Z2-R-LIGHT-047 Bloom soft threshold

- **Statement:** The optional soft threshold is continuous and monotonic in luminance around T ± K, and the default T = 0 passes all energy.
- **Covers:** Z2-I-LIGHT-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-LIGHT-014
- **Acceptance evidence:** `Test_Bloom_SoftThresholdContinuity`.
- **Design:** Zenith2/Docs/Design/LIGHT/PostProcess.md → "Bloom"

### Z2-R-LIGHT-048 Circle of confusion

- **Statement:** The circle of confusion is the thin-lens value from the camera's focal length, aperture, sensor width and focus distance, or the artistic focus-range model when no focal length is set, signed and clamped to the maximum.
- **Covers:** Z2-I-LIGHT-16
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-LIGHT-012
- **Acceptance evidence:** `Test_DoF_CircleOfConfusionThinLens`.
- **Design:** Zenith2/Docs/Design/LIGHT/PostProcess.md → "Depth of field"

### Z2-R-LIGHT-049 Depth-of-field gather and composite

- **Statement:** The gather runs at half output extent with 24 or 48 golden-angle samples, a tile-dilated radius and near-field priority, and the composite blends by the circle of confusion.
- **Covers:** Z2-I-LIGHT-16
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-LIGHT-012
- **Acceptance evidence:** `LightingPost` parity scene (Stage 5); CinematicSlice human sign-off (Z2-Q-LIGHT-016).
- **Design:** Zenith2/Docs/Design/LIGHT/PostProcess.md → "Depth of field"

### Z2-R-LIGHT-050 Depth of field per camera

- **Statement:** Depth of field runs only for views whose camera enables it and whose feature bit is set, and is disabled at critical memory pressure.
- **Covers:** Z2-I-LIGHT-16
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-059
- **Budgets:** Z2-B-LIGHT-012
- **Acceptance evidence:** `Test_Paths_DisabledFeatureAddsNoPass` (DoF), `Test_Lighting_CriticalPressureShedsPost`.
- **Design:** Zenith2/Docs/Design/LIGHT/PostProcess.md → "Depth of field"; Zenith2/Docs/Design/LIGHT/QualityTiers.md → "Fallbacks and triggers"

### Z2-R-LIGHT-051 Motion blur tile and neighbour maxima

- **Statement:** Motion blur computes the 16 × 16 tile maximum velocity and its 3 × 3 neighbourhood maximum from shutter-scaled, clamped velocities.
- **Covers:** Z2-I-LIGHT-15
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-LIGHT-013
- **Acceptance evidence:** `Test_MotionBlur_TileMaxReference`.
- **Design:** Zenith2/Docs/Design/LIGHT/PostProcess.md → "Motion blur"

### Z2-R-LIGHT-052 Motion blur reconstruction gather

- **Statement:** The gather takes 8 or 16 jittered samples along the neighbour-max velocity with soft depth and velocity comparisons, and copies tiles below 0.5 px.
- **Covers:** Z2-I-LIGHT-15
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-LIGHT-013
- **Acceptance evidence:** `LightingPost` parity scene (Stage 5); CinematicSlice human sign-off (Z2-Q-LIGHT-017).
- **Design:** Zenith2/Docs/Design/LIGHT/PostProcess.md → "Motion blur"

### Z2-R-LIGHT-053 Motion blur never smears cuts or teleports

- **Statement:** A camera cut produces zero camera velocity and a history-reset instance zero object velocity, so neither is blurred.
- **Covers:** Z2-I-LIGHT-15
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** Z2-B-LIGHT-013
- **Acceptance evidence:** `Test_MotionBlur_CameraCutNoBlur`, `Test_TAA_TeleportedInstanceNoSmear`.
- **Design:** Zenith2/Docs/Design/LIGHT/PostProcess.md → "Motion blur"

### Z2-R-LIGHT-054 Photometric light units

- **Statement:** Light intensities are stored in lux (directional), candela (point, spot) and nits (rect, disc), with the lumen and Kelvin conversions of "Light units" and their degenerate-input clamps.
- **Covers:** Z2-I-LIGHT-01, Z2-I-LIGHT-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_LightUnits_LumensToCandela`, `Test_LightUnits_LumensToNitsArea`, `Test_LightUnits_KelvinToRec709`.
- **Design:** Zenith2/Docs/Design/LIGHT/Lighting.md → "Light units"

### Z2-R-LIGHT-055 Light record encodings per type

- **Statement:** `Flux_EncodeLightRecord` is the one writer of the 64-byte light record, encoding every type of "Light record encodings" including LIGHT's flag bits and the rect tangent packing.
- **Covers:** Z2-I-LIGHT-01, Z2-I-LIGHT-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-058
- **Budgets:** none
- **Acceptance evidence:** `Test_LightRecord_EncodeEveryType`, `Test_LightRecord_EncodeAreaTypes`.
- **Design:** Zenith2/Docs/Design/LIGHT/Lighting.md → "Light record encodings"

### Z2-R-LIGHT-056 Light deltas maintain the CPU mirror and the light table

- **Statement:** Light deltas are consumed in world-tag and merge order by RHI's `Flux_GPUScene::ApplyDeltas`, which calls LIGHT's `Flux_ValidateLightDelta` and `Flux_EncodeLightRecord`, and then by `Flux_LightingSystem::ApplyRenderWorld`, which updates the CPU light mirror with the same validation verdict; unknown updates become adds, invalid deltas are refused and counted, and a full table drops the light.
- **Covers:** Z2-I-LIGHT-01, Z2-I-LIGHT-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-058, Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_LightDelta_ApplyAddUpdateRemove`, `Test_LightDelta_InvalidRefused`, `Test_LightMirror_InvalidDeltaKeepsEntry`, `Test_LightDelta_TableFull`, `Test_LightDelta_ValidationSharedVerdict`.
- **Design:** Zenith2/Docs/Design/LIGHT/Lighting.md → "Light extraction and the CPU light mirror"; Zenith2/Docs/Design/LIGHT/Lighting.md → "Division of the delta work with the GPU scene"

### Z2-R-LIGHT-057 Deterministic light and decal list

- **Statement:** The light and decal list build writes the frame-visible local lights and decals in ascending index order through a groupshared prefix, orders directional lights sun slot, moon slot, then scene directional lights by luminance with their roles in word 1, keeps at most two, and is byte-identical across runs and backends.
- **Covers:** Z2-I-LIGHT-01, Z2-I-LIGHT-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-035
- **Budgets:** Z2-B-LIGHT-001
- **Acceptance evidence:** `Test_LightList_CPUReferenceMatchesGPU`, `Test_LightList_DirectionalOverflow`, `Test_LightList_SunMoonSlotsFirst`.
- **Design:** Zenith2/Docs/Design/LIGHT/Lighting.md → "Light and decal list build"

### Z2-R-LIGHT-058 Froxel light clusters

- **Statement:** Clustering uses the grid and capacities fixed per capability tier (compile-time constants, not configuration keys), exponential slices, ascending-order assignment of lights, decals and (Stage 4) probes, depth-range-restricted decals, and deterministic overflow truncation with counters; every `Flux_ClusterIndex` caller maps a pixel and depth through the grid, tile size and depth range (n, f) its parameter struct carries (`m_xClusterGrid`, `m_xClusterTileSize`, `m_xClusterDepthRange`), equal to those the clusters were built for.
- **Covers:** Z2-I-LIGHT-01, Z2-I-LIGHT-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-035
- **Budgets:** Z2-B-LIGHT-003
- **Acceptance evidence:** `Test_Clusters_SliceMapping`, `Test_Clusters_ListsDeterministic`, `Test_Clusters_OverflowKeepsLowestIndices`, `Test_Clusters_SpotConeTest`, `Test_Clusters_LayoutPerCapabilityTier`.
- **Design:** Zenith2/Docs/Design/LIGHT/Lighting.md → "Light clustering"; Zenith2/Docs/Design/LIGHT/Lighting.md → "Cluster grid"; Zenith2/Docs/Design/LIGHT.md → "Rendering table additions pending X103"

### Z2-R-LIGHT-059 Single-source EvaluateLighting

- **Statement:** Every path lights through `EvaluateLightingEx` in the step order of "Evaluation order", with the light source the only path-specific input, no AO on direct light, fog applied by the caller, and no side effects.
- **Covers:** Z2-I-LIGHT-01, Z2-I-LIGHT-02, Z2-I-LIGHT-06, Z2-I-LIGHT-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-035, Z2-D-015
- **Budgets:** none
- **Acceptance evidence:** `Test_Lighting_EvaluationOrderReference`; `ShaderChecks`; `LightingBasic` parity.
- **Design:** Zenith2/Docs/Design/LIGHT/Lighting.md → "The EvaluateLighting contract"

### Z2-R-LIGHT-060 GGX BRDF with the precision split

- **Statement:** The BRDF uses GGX, height-correlated Smith and Schlick Fresnel with the α floor 0.002, with `n·h`, D, V and α² in float and colour terms in half.
- **Covers:** Z2-I-LIGHT-01, Z2-I-LIGHT-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-014
- **Budgets:** none
- **Acceptance evidence:** `Test_BRDF_ShaderMatchesReference`, `Test_BRDF_GGXNormalisation`; `ShaderChecks.HalfRatioReport`.
- **Design:** Zenith2/Docs/Design/LIGHT/Lighting.md → "BRDF"

### Z2-R-LIGHT-061 Energy-conserving coupled BRDF

- **Statement:** The multi-scatter specular (with `F_avg = F0 + (F90 − F0)/21`) and the coupled diffuse term, an elaboration of LIGHT's (design choice LR3), make the BRDF reciprocal and bound its directional albedo by 1 + 1e-3, with the IBL using the same energy terms; the diffuse and multi-scatter terms are guarded (denominators `max(…, 1e-4)`, `saturate` numerators, `f_d = 0` for a zero diffuse albedo), so the white metal (F0 = 1) and the α floor give finite, non-negative values in the shader and the CPU reference alike.
- **Covers:** Z2-I-LIGHT-01, Z2-I-LIGHT-02, Z2-I-LIGHT-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-063
- **Budgets:** none
- **Acceptance evidence:** `Test_BRDF_WhiteFurnace`, `Test_BRDF_Reciprocity`, `Test_BRDF_MultiScatterMetalEnergy`, `Test_BRDF_IBLFurnace`.
- **Design:** Zenith2/Docs/Design/LIGHT/Lighting.md → "BRDF"

### Z2-R-LIGHT-062 Punctual attenuation and cones

- **Statement:** Point and spot lights use the windowed inverse-square falloff and squared smooth cone, and sphere lights the form-factor diffuse and widened specular.
- **Covers:** Z2-I-LIGHT-01, Z2-I-LIGHT-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_LightAttenuation_WindowedInverseSquare`; `PT_AnalyticPoint`, `PT_SphereLight` comparisons.
- **Design:** Zenith2/Docs/Design/LIGHT/Lighting.md → "Light attenuation and shape"

### Z2-R-LIGHT-063 Lighting channels

- **Statement:** A light affects a surface only when its channel mask intersects the instance's, never changing shadow casting.
- **Covers:** Z2-I-LIGHT-01, Z2-I-LIGHT-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_LightingChannels_Mask`; a render test with two channels.
- **Design:** Zenith2/Docs/Design/LIGHT/Lighting.md → "Lighting channels"

### Z2-R-LIGHT-064 Rect and disc area lights

- **Statement:** Rect lights are evaluated with LTC over the horizon-clipped polygon and disc lights over an area-corrected 8-gon, with their luminance units and barn cones.
- **Covers:** Z2-I-LIGHT-01, Z2-I-LIGHT-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-063
- **Budgets:** Z2-B-LIGHT-003
- **Acceptance evidence:** `PT_RectLight`, `PT_DiscLight` comparisons within tolerance.
- **Design:** Zenith2/Docs/Design/LIGHT/Lighting.md → "Light attenuation and shape"; Zenith2/Docs/Design/LIGHT/Lighting.md → "Lighting lookup tables"

### Z2-R-LIGHT-065 Advanced surface lobes in EvaluateLighting

- **Statement:** Clear coat, anisotropy, sheen and subsurface integrate into `EvaluateLightingEx` as "Advanced surface lobe integration" states, without new program permutations, each passing the furnace and reciprocity bounds.
- **Covers:** Z2-I-LIGHT-01, Z2-I-LIGHT-02, Z2-I-LIGHT-06
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-063, Z2-D-035
- **Budgets:** none
- **Acceptance evidence:** `Test_BRDF_AdvancedLobesFurnace`; GEOM's surface-model tests.
- **Design:** Zenith2/Docs/Design/LIGHT/Lighting.md → "Advanced surface lobe integration"

### Z2-R-LIGHT-066 Deferred and visibility-buffer lighting

- **Statement:** Deferred lighting decodes the G-buffer (and GB4 for extended models) into `Flux_SurfaceData`, skips sky texels, evaluates `EvaluateLightingEx` with cluster lights and fog, and matches Forward+ within the capability-matrix tolerance.
- **Covers:** Z2-I-LIGHT-01, Z2-I-LIGHT-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-035
- **Budgets:** Z2-B-LIGHT-008, Z2-B-LIGHT-009
- **Acceptance evidence:** `Test_DeferredLighting_HarnessMatchesEvaluateLighting`, `Test_DeferredLighting_SampledReadsOnly`, `Test_DeferredLighting_MatchesForwardPlus`, `Test_DeferredLighting_NoTileLocalRead`; `PathParity` on the `Lighting*` scenes.
- **Design:** Zenith2/Docs/Design/LIGHT/Lighting.md → "Deferred lighting pass"

### Z2-R-LIGHT-067 Cooked lighting LUTs

- **Statement:** The DFG, sheen albedo and LTC tables are cooked by `Cook_LightingLUTs` from the CPU reference into the formats of "Lighting lookup tables", and the cook refuses a DFG table whose energy exceeds 1 + 1e-3.
- **Covers:** Z2-I-LIGHT-01, Z2-I-LIGHT-02, Z2-I-LIGHT-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_BRDF_DFGTableMatchesReference`, `Test_LightingLUT_DFGEnergyBound`.
- **Design:** Zenith2/Docs/Design/LIGHT/Lighting.md → "Lighting lookup tables"; Zenith2/Docs/Design/LIGHT.md → "Cook steps"

### Z2-R-LIGHT-068 Independent CPU reference BRDF tests

- **Statement:** The CPU reference BRDF, sharing no code with `Zenith2/Shaders/Lighting/`, checks the shader BRDF for white furnace, reciprocity and the closed-form cases on WARP and lavapipe, the Lambert case being the configuration F0 = 0, in which the full BRDF equals `ρ / π` exactly (within 1e-6).
- **Covers:** Z2-I-LIGHT-01, Z2-I-LIGHT-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-063
- **Budgets:** none
- **Acceptance evidence:** `Test_BRDF_WhiteFurnace`, `Test_BRDF_Reciprocity`, `Test_BRDF_LambertClosedForm`, `Test_BRDF_GGXNormalisation`, `Test_BRDF_ShaderMatchesReference` in the `Test` step.
- **Design:** Zenith2/Docs/Design/LIGHT/Tests.md → "Analytical BRDF tests"; Zenith2/Docs/Design/LIGHT/Lighting.md → "Closed-form reductions"

### Z2-R-LIGHT-069 Shadow atlas

- **Statement:** Every frame's shadow views render into one transient D24S8 (or D32F_S8 fallback) atlas of the tier's `render.quality.shadowAtlasWidth` × `shadowAtlasHeight`, cleared at first use and per tile, standard Z only; no shadow content survives the frame, and a frame without shadowed lights leaves the consumers' `[Optional]` atlas fields unset (item (e) of X103); a layout is valid only when the strip fits, a local block fits across, one size-aligned local block fits below one strip (`height ≥ roundUp(T_c, L) + L`) and the local maximum is at least the minimum, and an invalid layout is refused with `FLUX_LIGHTING_ERROR_INVALID_SETTINGS`.
- **Covers:** Z2-I-LIGHT-01, Z2-I-LIGHT-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-061, Z2-D-057
- **Budgets:** Z2-B-LIGHT-002, Z2-B-RHI-010
- **Acceptance evidence:** `Test_Shadows_AtlasTileIsolation`, `Test_ShadowAtlas_InvalidSettingsRejected`, `Test_LightConfig_AtlasSettingsRefused`, `Test_ShadowAtlas_FinestAndCoarsestLevelsFill`; sync validation clean with aliasing and poisoning on.
- **Design:** Zenith2/Docs/Design/LIGHT/Shadows.md → "Shadow atlas"; Zenith2/Docs/Design/LIGHT/Shadows.md → "Atlas layout"

### Z2-R-LIGHT-070 Deterministic atlas allocation

- **Statement:** Local shadow tiles are allocated per frame by the one packer, `Flux_ShadowAtlasAllocator`'s buddy quadtree, in size, priority and key order, halving on failure down to the minimum and then leaving the light unshadowed and counted in `Flux.Shadows.Dropped`, with per-light size hysteresis. The hysteresis state advances only on submitted frames.
- **Covers:** Z2-I-LIGHT-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-059
- **Budgets:** Z2-B-LIGHT-002
- **Acceptance evidence:** `Test_ShadowAtlas_PackingDeterministic`, `Test_ShadowAtlas_FallbackHalvesThenDrops`, `Test_ShadowAtlas_PointLightAllOrNothing`, `Test_ShadowAtlas_Hysteresis`.
- **Design:** Zenith2/Docs/Design/LIGHT/Shadows.md → "Atlas allocation"; Zenith2/Docs/Design/LIGHT/Shadows.md → "Atlas tile sizing"

### Z2-R-LIGHT-071 Practical cascade splits

- **Statement:** Cascade splits follow the practical split scheme with λ over the shadow distance, with the first split at the view near plane and the last at the shadow distance.
- **Covers:** Z2-I-LIGHT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-LIGHT-002
- **Acceptance evidence:** `Test_CSM_SplitsPracticalScheme`.
- **Design:** Zenith2/Docs/Design/LIGHT/Shadows.md → "Cascaded shadow maps"

### Z2-R-LIGHT-072 Stable cascades

- **Statement:** Each cascade is fitted with the slice's bounding sphere, a quantised radius, a camera-independent light basis and texel-snapped translation, so sub-texel camera motion and camera rotation do not shimmer.
- **Covers:** Z2-I-LIGHT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-LIGHT-002
- **Acceptance evidence:** `Test_CSM_BoundingSphereContainsSlice`, `Test_CSM_TexelSnapping`, `Test_CSM_StableUnderRotation`, `Test_CSM_LightBasisDegenerate`.
- **Design:** Zenith2/Docs/Design/LIGHT/Shadows.md → "Cascaded shadow maps"

### Z2-R-LIGHT-073 Cascade depth range and pancaking

- **Statement:** Cascades extend their near plane by the caster extension and render with depth clamp, in standard Z with no precision check.
- **Covers:** Z2-I-LIGHT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-061
- **Budgets:** none
- **Acceptance evidence:** `Test_View_DepthModePrecisionCheck` (cascade case); `Test_Shadows_ContactEdgeLeakBound`.
- **Design:** Zenith2/Docs/Design/LIGHT/Shadows.md → "Cascaded shadow maps"; Zenith2/Docs/Design/LIGHT/Shadows.md → "Shadow depth rendering"

### Z2-R-LIGHT-074 Cascade selection, blend and fade

- **Statement:** Shaders select the first cascade whose far split exceeds the pixel depth, blend linearly across the band, and fade the sun shadow out over the last 5% of the shadow distance.
- **Covers:** Z2-I-LIGHT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Shadows_CascadeBlendContinuity`.
- **Design:** Zenith2/Docs/Design/LIGHT/Shadows.md → "Cascaded shadow maps"; Zenith2/Docs/Design/LIGHT/Shadows.md → "Shadow sampling in shaders"

### Z2-R-LIGHT-075 Spot shadow views

- **Statement:** A shadowed spot light renders one perspective standard-Z view covering its outer cone plus the guard band, with its near and far planes from its range.
- **Covers:** Z2-I-LIGHT-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-059, Z2-D-061
- **Budgets:** Z2-B-LIGHT-002
- **Acceptance evidence:** `LightingBasic` parity; `Test_View_DepthModePrecisionCheck` (spot case).
- **Design:** Zenith2/Docs/Design/LIGHT/Shadows.md → "Spot shadow views"

### Z2-R-LIGHT-076 Point shadow views

- **Statement:** A shadowed point light renders six equal face tiles with a guard-banded field of view, and shaders select the face by the major axis with the fixed tie order.
- **Covers:** Z2-I-LIGHT-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-059
- **Budgets:** Z2-B-LIGHT-002
- **Acceptance evidence:** `Test_PointShadow_FaceSelection`; `LightingBasic` parity.
- **Design:** Zenith2/Docs/Design/LIGHT/Shadows.md → "Point shadow views"

### Z2-R-LIGHT-077 Shadow view record layout

- **Statement:** `Flux_ShadowViewRecord` is the 96-byte record of "Shadow view record", written once per shadow view per frame into the upload ring, identical on both APIs.
- **Covers:** Z2-I-LIGHT-01, Z2-I-LIGHT-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-015, Z2-D-058
- **Budgets:** none
- **Acceptance evidence:** `Test_ShadowRecord_Layout`, `Test_ShadowRecord_GPUReadback`, `Test_ShadowRecord_WorldToShadowMapsTile`.
- **Design:** Zenith2/Docs/Design/LIGHT/Shadows.md → "Shadow view record"

### Z2-R-LIGHT-078 Shadow filtering levels

- **Statement:** Shadow lookups use the filter of the tier's quality (bilinear, 3 × 3 Castaño, contact-hardening with a blocker search) with every tap clamped into the tile interior.
- **Covers:** Z2-I-LIGHT-01, Z2-I-LIGHT-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-LIGHT-002
- **Acceptance evidence:** `Test_Shadows_AtlasTileIsolation`; `PT_AreaLightShadow` (quality 2).
- **Design:** Zenith2/Docs/Design/LIGHT/Shadows.md → "Filtering"

### Z2-R-LIGHT-079 Shadow bias

- **Statement:** Shadows combine the fixed slope-scaled raster bias, a receiver normal offset scaled by texel size and angle, and the light's depth bias, leaking at most 2 texels at a contact edge and showing no acne up to 85°.
- **Covers:** Z2-I-LIGHT-01, Z2-I-LIGHT-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Shadows_ContactEdgeLeakBound`, `Test_Shadows_NoAcneOnSlopes`.
- **Design:** Zenith2/Docs/Design/LIGHT/Shadows.md → "Bias"; Zenith2/Docs/Design/LIGHT/Shadows.md → "Shadow depth rendering"

### Z2-R-LIGHT-080 Shadowed-light budget

- **Statement:** `Flux_LightingSystem::PlanShadows` selects the shadowed lights by priority descending then `Flux_RenderLightKey` ascending, directional lights always, local lights up to `render.quality.maxShadowedLights`, and an unshadowed light's record carries `m_uShadowIndex = 0xFFFFFFFF`.
- **Covers:** Z2-I-LIGHT-01, Z2-I-LIGHT-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-059
- **Budgets:** Z2-B-RPROD-022
- **Acceptance evidence:** `Test_ShadowBudget_PriorityOrder`, `Test_Shadows_DroppedLightUnshadowed`.
- **Design:** Zenith2/Docs/Design/LIGHT/Shadows.md → "Shadowed-light budget"

### Z2-R-LIGHT-081 Screen-space contact shadows

- **Statement:** Contact shadows march 8 or 16 jittered screen-space steps toward one directional entry c, the first in frame-list order whose record carries `FLUX_LIGHT_FLAG_CONTACT_SHADOWS` (the sun slot whenever it is active, never the moon slot, whose bit is cleared and counted), with the thickness test, write `mask = s / steps` for the first occlusion at step s (a near occluder darkens most) raised toward 1 by the border fade, and the mask multiplies only entry c's shadow; every other directional entry takes 1.
- **Covers:** Z2-I-LIGHT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-LIGHT-006
- **Acceptance evidence:** `Test_ContactShadows_ThinOccluder`, `Test_ContactShadows_SingleEntry`; `AE_ContactShadows`.
- **Design:** Zenith2/Docs/Design/LIGHT/Shadows.md → "Contact shadows"; Zenith2/Docs/Design/LIGHT/Lighting.md → "Evaluation order"; Zenith2/Docs/Design/LIGHT/Lighting.md → "Sun and moon slots"

### Z2-R-LIGHT-082 Area light shadows

- **Statement:** Rect and disc lights render one 120° perspective shadow view and use the area penumbra filter; surfaces outside the cone are unshadowed.
- **Covers:** Z2-I-LIGHT-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-063
- **Budgets:** Z2-B-LIGHT-002
- **Acceptance evidence:** `PT_AreaLightShadow` within tolerance.
- **Design:** Zenith2/Docs/Design/LIGHT/Shadows.md → "Area light shadow views"

### Z2-R-LIGHT-083 GTAO

- **Statement:** GTAO integrates the analytic cosine-weighted visibility over 2, 3 or 4 slices with the tier's steps, radius, falloff, thin-object heuristic and temporal noise.
- **Covers:** Z2-I-LIGHT-06, Z2-I-LIGHT-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-LIGHT-005
- **Acceptance evidence:** `Test_GTAO_AnalyticConfigurations`; `AE_GTAO`.
- **Design:** Zenith2/Docs/Design/LIGHT/GI.md → "GTAO"

### Z2-R-LIGHT-084 GTAO denoise and upsample

- **Statement:** The GTAO result is denoised with a 5 × 5 depth- and normal-aware edge-preserving filter and, at half resolution, upsampled with a joint edge-preserving filter.
- **Covers:** Z2-I-LIGHT-06, Z2-I-LIGHT-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-LIGHT-005
- **Acceptance evidence:** `LightingBasic` parity (AO channel near-exact); `AE_GTAO`.
- **Design:** Zenith2/Docs/Design/LIGHT/GI.md → "GTAO denoise and upsample"

### Z2-R-LIGHT-085 AO application

- **Statement:** Lighting combines SSAO with material AO by `min`, applies the multi-bounce correction to indirect diffuse and the specular occlusion to environment specular only.
- **Covers:** Z2-I-LIGHT-06, Z2-I-LIGHT-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Lighting_EvaluationOrderReference` (AO cases).
- **Design:** Zenith2/Docs/Design/LIGHT/Lighting.md → "BRDF"; Zenith2/Docs/Design/LIGHT/GI.md → "GTAO denoise and upsample"

### Z2-R-LIGHT-086 Froxel grid and slice distribution

- **Statement:** The froxel volume uses the tier grid with exponential slices from 0.5 m to the tier far distance, independent of the view far plane.
- **Covers:** Z2-I-LIGHT-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-LIGHT-004
- **Acceptance evidence:** `Test_Fog_SliceDistribution` against `Flux_FroxelSliceDepth`.
- **Design:** Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Froxel grid"

### Z2-R-LIGHT-087 Scattering injection

- **Statement:** Injection sums the media of "Media model" and the directional entries, volumetric local lights, sky ambient and emission per froxel with their phases and shadow quality into pre-exposed in-scatter and extinction; `FLUX_LIGHT_FLAG_VOLUMETRIC` gates every directional term, the sun-role entry carries its atmosphere transmittance and cloud shadow, the moon-role entry its transmittance without cloud shadow, and a role-less directional entry injects with neither.
- **Covers:** Z2-I-LIGHT-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-LIGHT-004
- **Acceptance evidence:** `Test_Fog_HomogeneousBeerLambert`, `Test_Fog_DirectionalVolumetricGating`; `LightingFog` parity.
- **Design:** Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Scattering injection"; Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Media model"

### Z2-R-LIGHT-088 Energy-conserving front-to-back integration

- **Statement:** Integration accumulates the analytic per-step in-scatter and transmittance front to back per column and stores in-scatter and transmittance to each slice's far edge.
- **Covers:** Z2-I-LIGHT-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-LIGHT-004
- **Acceptance evidence:** `Test_Fog_HomogeneousBeerLambert`.
- **Design:** Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Integration"

### Z2-R-LIGHT-089 Temporal froxel reprojection

- **Statement:** With temporal fog enabled, injection jitters within the slice and blends with the reprojected per-view fog history, rejecting it when invalid or out of range.
- **Covers:** Z2-I-LIGHT-07, Z2-I-LIGHT-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-059
- **Budgets:** Z2-B-LIGHT-004
- **Acceptance evidence:** `Test_Fog_TemporalStability`.
- **Design:** Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Temporal reprojection"

### Z2-R-LIGHT-090 Fog applied in-shader

- **Statement:** Opaque, deferred, transparent, particle and sky passes apply the integrated volume with `Flux_ApplyFroxelFog` at the pixel's slice coordinate; no fog composite pass exists.
- **Covers:** Z2-I-LIGHT-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-035
- **Budgets:** none
- **Acceptance evidence:** `LightingFog` parity across paths.
- **Design:** Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Fog application"

### Z2-R-LIGHT-091 Transmittance LUT

- **Statement:** The 256 × 64 transmittance LUT uses the Bruneton parameterisation and 40 steps, matches the CPU reference within 0.5%, and is persistent, recomputed only when the atmosphere block's `m_uSkyRevision` changes, whichever of the atmosphere, fog or star values raised it (item (f) of X103).
- **Covers:** Z2-I-LIGHT-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-ENV-001
- **Acceptance evidence:** `Test_Sky_TransmittanceLUTReference`, `Test_Sky_LUTRebuildOnRevisionOnly`.
- **Design:** Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Transmittance LUT"

### Z2-R-LIGHT-092 Multiple-scattering LUT

- **Statement:** The 32 × 32 multiple-scattering LUT is computed with Hillaire's second-order method over 64 directions and 20 steps including the ground albedo.
- **Covers:** Z2-I-LIGHT-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-ENV-001
- **Acceptance evidence:** `Test_Sky_MultiScatterReference` against `Flux_ComputeMultiScatterReference`.
- **Design:** Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Multiple-scattering LUT"

### Z2-R-LIGHT-093 Sky-view LUT

- **Statement:** The 192 × 108 sky-view LUT is computed once per frame from the main camera with the non-linear horizon mapping, starting at the fog far distance, with single and multiple scattering.
- **Covers:** Z2-I-LIGHT-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-ENV-001
- **Acceptance evidence:** `Test_Fog_SkyNoDoubleCount` against `Flux_ComputeSkyRadianceReference`.
- **Design:** Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Sky-view LUT"

### Z2-R-LIGHT-094 Sky draw with aerial perspective counted once

- **Statement:** The sky pass composites, back to front, the star field, the sky-view LUT, the sun disc in main and editor views, ENV's cloud layer and the froxel volume's last slice as aerial perspective, so the atmosphere is counted exactly once.
- **Covers:** Z2-I-LIGHT-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-ENV-001
- **Acceptance evidence:** `Test_Fog_SkyNoDoubleCount`, `Test_Sky_CompositeOrder`; OpenWorldSlice sign-off (Z2-Q-LIGHT-007).
- **Design:** Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Sky draw and aerial perspective"; Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Sun transmittance"

### Z2-R-LIGHT-095 Local fog volumes

- **Statement:** Up to 64 box or ellipsoid local fog volumes per world add their media to the injection with edge fades.
- **Covers:** Z2-I-LIGHT-07
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-LIGHT-004
- **Acceptance evidence:** `Test_Fog_LocalVolumeDensity` (a volume's optical depth against its analytic value).
- **Design:** Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Local fog volumes"

### Z2-R-LIGHT-096 SSR ray generation

- **Statement:** SSR traces pixels below the roughness cutoff with VNDF-sampled reflection rays, frame-varying noise and a normal-offset start, in screen UV and linear depth.
- **Covers:** Z2-I-LIGHT-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-060
- **Budgets:** Z2-B-LIGHT-007
- **Acceptance evidence:** `Test_SSR_MirrorPlaneHit`.
- **Design:** Zenith2/Docs/Design/LIGHT/Reflections.md → "SSR ray generation"

### Z2-R-LIGHT-097 Hierarchical trace on the HiZ min channel

- **Statement:** The SSR trace walks the final pyramid's min channel, ascending when the ray passes in front of a cell's nearest depth and descending otherwise, with the tier's iteration cap and the thickness test.
- **Covers:** Z2-I-LIGHT-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-060
- **Budgets:** Z2-B-LIGHT-007
- **Acceptance evidence:** `Test_SSR_MirrorPlaneHit`; `AE_SSR`.
- **Design:** Zenith2/Docs/Design/LIGHT/Reflections.md → "SSR hierarchical trace"

### Z2-R-LIGHT-098 SSR hit validation and confidence

- **Statement:** Hits are rejected on back faces and off-screen reprojection and attenuated by border, roughness and distance fades, with radiance from the reprojected previous colour rescaled by the pre-exposure ratio.
- **Covers:** Z2-I-LIGHT-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-LIGHT-007
- **Acceptance evidence:** `Test_SSR_MissFallsBackToProbe`.
- **Design:** Zenith2/Docs/Design/LIGHT/Reflections.md → "SSR hit validation"

### Z2-R-LIGHT-099 SSR denoise

- **Statement:** The SSR result is denoised by the edge-aware ratio estimator without an SSR history.
- **Covers:** Z2-I-LIGHT-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-LIGHT-007
- **Acceptance evidence:** `LightingSSR` parity; `AE_SSR`.
- **Design:** Zenith2/Docs/Design/LIGHT/Reflections.md → "SSR denoise"

### Z2-R-LIGHT-100 SSR composite replaces environment specular

- **Statement:** The composite adds `c × w × (L_ssr − L_env)` with w from the forward specular-weight target (present only with the view's SSR feature bit) or the G-buffer and L_env recomputed with the same IBL function, so a confident hit fully replaces the environment term.
- **Covers:** Z2-I-LIGHT-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-LIGHT-007
- **Acceptance evidence:** `Test_SSR_CompositeRemovesEnvironment`.
- **Design:** Zenith2/Docs/Design/LIGHT/Reflections.md → "SSR composite"

### Z2-R-LIGHT-101 IBL split-sum evaluation

- **Statement:** Environment specular samples the cube array with the dominant-direction correction, roughness mip selection, probe then sky weights, `E_tot(μv)`, specular occlusion and horizon fade.
- **Covers:** Z2-I-LIGHT-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-063
- **Budgets:** none
- **Acceptance evidence:** `Test_BRDF_IBLFurnace`; `LightingIBL` parity; `PT_IBLEnvironment`.
- **Design:** Zenith2/Docs/Design/LIGHT/Reflections.md → "IBL evaluation"; Zenith2/Docs/Design/LIGHT/Reflections.md → "Environment cube array"

### Z2-R-LIGHT-102 Static environment

- **Statement:** A `.zenv` cooked from an RGBE image or the sky model supplies slice 0 and the sky SH, and the engine default environment is always loaded, so slice 0 is never empty.
- **Covers:** Z2-I-LIGHT-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_EnvironmentFile_RoundTrip`, `Test_EnvironmentFile_RejectsCorrupt`, `Test_RGBEReader_Grammar`, `Test_Lighting_MissingEnvironmentFallsBack`, `Zenith_Fuzz_RGBEReader`.
- **Design:** Zenith2/Docs/Design/LIGHT/Reflections.md → "Static environment"

### Z2-R-LIGHT-103 Reflection probe cooked format

- **Statement:** Baked probes are `.zprb` files whose info section is the 64-byte capture record of "Probe info record", validated on load (sizes, non-finite values, non-zero reserved bytes) and refused with `FLUX_LIGHTING_ERROR_CORRUPT_DATA` on any violation.
- **Covers:** Z2-I-LIGHT-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_ProbeFile_RoundTrip`, `Test_ProbeFile_RejectsCorrupt`, `Zenith_Fuzz_ProbeFile`.
- **Design:** Zenith2/Docs/Design/LIGHT/Reflections.md → "Reflection probe cooked format"; Zenith2/Docs/Design/LIGHT/Reflections.md → "Probe info record"

### Z2-R-LIGHT-104 Probe capture through capture views

- **Statement:** Realtime and missing baked probes are captured through six capture views on the capture path with a fixed exposure, round-robined within the capture cap; a face counts as captured only when its frame submitted.
- **Covers:** Z2-I-LIGHT-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-059
- **Budgets:** Z2-B-RPROD-023
- **Acceptance evidence:** `Test_Probe_CaptureConstantEnvironment`.
- **Design:** Zenith2/Docs/Design/LIGHT/Reflections.md → "Probe capture"

### Z2-R-LIGHT-105 GGX probe prefilter

- **Statement:** Probe and sky cubes are prefiltered per mip at roughness `m / (mips − 1)` by filtered importance sampling with a fixed Hammersley set, matching the CPU cook.
- **Covers:** Z2-I-LIGHT-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_ProbeFilter_GPUMatchesCook`.
- **Design:** Zenith2/Docs/Design/LIGHT/Reflections.md → "Probe filtering"

### Z2-R-LIGHT-106 Probe blending and parallax

- **Statement:** Probes blend in priority and volume order with influence fades and box parallax, the remainder going to the sky, at most the cluster cap per pixel.
- **Covers:** Z2-I-LIGHT-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_ProbeBlend_WeightsSumToOne`, `Test_ProbeBlend_ParallaxBoxProjection`; `LightingProbes` parity.
- **Design:** Zenith2/Docs/Design/LIGHT/Reflections.md → "Probe blending"; Zenith2/Docs/Design/LIGHT/Reflections.md → "Reflection probe record"

### Z2-R-LIGHT-107 Probe residency and streaming integration

- **Statement:** Probes are made resident by the scored selection up to the slice capacity, baked probes load and unload with their streaming cell, and cancellations leave no slice assigned.
- **Covers:** Z2-I-LIGHT-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_ProbeResidency_SelectionDeterministic`, `Test_Probes_LoadCancelStress`.
- **Design:** Zenith2/Docs/Design/LIGHT/Reflections.md → "Probe residency and streaming"; Zenith2/Docs/Design/LIGHT.md → "Probe residency state machine"

### Z2-R-LIGHT-108 Probe fallback chain

- **Statement:** A pixel uses SSR, then resident probes, then the captured sky, the static environment and the engine default, and missing probes are counted.
- **Covers:** Z2-I-LIGHT-03, Z2-I-LIGHT-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_SSR_MissFallsBackToProbe`, `Test_Lighting_MissingEnvironmentFallsBack`.
- **Design:** Zenith2/Docs/Design/LIGHT/Reflections.md → "Probe fallback chain"

### Z2-R-LIGHT-109 Dynamic sky capture

- **Statement:** A sky or time-of-day change recaptures slice 0 one face per submitted frame into a ping-pong cube, filters it and projects the sky SH, visible within 7 submitted frames; a frame that submits nothing captures its face again in the next built frame.
- **Covers:** Z2-I-LIGHT-03, Z2-I-LIGHT-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_SkyCapture_UpdatesWithinSevenFrames`.
- **Design:** Zenith2/Docs/Design/LIGHT/Reflections.md → "Sky capture"

### Z2-R-LIGHT-110 Irradiance volume cooked format

- **Statement:** Irradiance volumes are `.zgiv` files of L1 SH per channel in RGBA16F 3D images plus a validity image, validated on load.
- **Covers:** Z2-I-LIGHT-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_IrradianceVolumeFile_RoundTrip`, `Test_IrradianceVolumeFile_RejectsCorrupt`, `Zenith_Fuzz_IrradianceVolumeFile`.
- **Design:** Zenith2/Docs/Design/LIGHT/GI.md → "Irradiance volume cooked format"

### Z2-R-LIGHT-111 Lightmap set cooked format

- **Statement:** Lightmap sets are `.zlmp` files with page records and instance bindings over RGBM irradiance and directional GEOM texture pages.
- **Covers:** Z2-I-LIGHT-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_LightmapSetFile_RoundTrip`, `Test_LightmapSetFile_RejectsCorrupt`, `Test_Lightmap_RGBMRoundTrip`, `Zenith_Fuzz_LightmapSetFile`.
- **Design:** Zenith2/Docs/Design/LIGHT/GI.md → "Lightmap set cooked format"

### Z2-R-LIGHT-112 Bake output contract for WORLD

- **Statement:** WORLD's bake writes LIGHT's formats only through the `Cook_Write*` functions (with `Cook_BakedProbe`, `Cook_BakedIrradianceVolume` and `Cook_BakedLightmapPage`) and `Cook_ProjectSH9`, which dilate, encode, validate and produce byte-identical files for identical input.
- **Covers:** Z2-I-LIGHT-03, Z2-I-LIGHT-04, Z2-I-WORLD-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_Bake_WritersDeterministic`, `Test_Bake_RejectsNonFinite`.
- **Design:** Zenith2/Docs/Design/LIGHT/GI.md → "Bake output contract"

### Z2-R-LIGHT-113 Irradiance volume sampling

- **Statement:** Surfaces sample up to 8 selected volumes with normal-offset trilinear weights, validity and back-face weighting, and priority blending with edge fades.
- **Covers:** Z2-I-LIGHT-04
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_IrradianceVolume_TrilinearWithValidity`; `LightingBakedGI` parity.
- **Design:** Zenith2/Docs/Design/LIGHT/GI.md → "Irradiance volume sampling"

### Z2-R-LIGHT-114 Lightmap evaluation

- **Statement:** Forward colour programs decode the RGBM irradiance and directional term of a lightmapped instance with explicit UV1 gradients.
- **Covers:** Z2-I-LIGHT-04
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Lightmap_RGBMRoundTrip`; `LightingBakedGI` parity.
- **Design:** Zenith2/Docs/Design/LIGHT/GI.md → "Lightmap evaluation"

### Z2-R-LIGHT-115 Lightmap binding and deferred paths

- **Statement:** Per-instance lightmap bindings reach shaders through the binding the X106 ruling selects (the instance lighting table if accepted, the per-material-slot indirection if refused), and on the deferred paths lightmapped irradiance is written at G-buffer time with the baked-diffuse flag.
- **Covers:** Z2-I-LIGHT-04
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-058, Z2-D-022
- **Budgets:** none
- **Acceptance evidence:** `LightingBakedGI` parity with its per-feature tolerance.
- **Design:** Zenith2/Docs/Design/LIGHT/GI.md → "Lightmap binding"; Zenith2/Docs/Design/LIGHT/GI.md → "Lightmaps on the deferred paths"

### Z2-R-LIGHT-116 Baked GI residency and streaming

- **Statement:** Volumes and lightmap sets load with their streaming cell, fade in over 30 frames, and missing data falls through to the next indirect source.
- **Covers:** Z2-I-LIGHT-04
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_BakedGI_LoadCancelStress`, `Test_BakedGI_FadeIn`.
- **Design:** Zenith2/Docs/Design/LIGHT/GI.md → "Baked GI residency and streaming"

### Z2-R-LIGHT-117 SSGI

- **Statement:** On the tiers that enable `render.quality.ssgiEnabled` (WIN_HIGH and WIN_EPIC, as ruled by Q41 (a) on 2026-10-16), SSGI traces one half-resolution cosine ray per pixel on the HiZ min channel, falls back to baked or sky irradiance on a miss, and accumulates temporally and spatially.
- **Covers:** Z2-I-LIGHT-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-060
- **Budgets:** none
- **Acceptance evidence:** `AE_SSGI`; `Test_SSGI_HistoryInvalidation`.
- **Design:** Zenith2/Docs/Design/LIGHT/GI.md → "SSGI"; Zenith2/Docs/Design/LIGHT/GI.md → "SSGI platform scope"

### Z2-R-LIGHT-118 SSGI documented error budget

- **Statement:** SSGI meets its documented energy, accuracy, stability and off-screen criteria on the approximation error scenes, validated against its own budget, not path-tracer convergence.
- **Covers:** Z2-I-LIGHT-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-063
- **Budgets:** none
- **Acceptance evidence:** `AE_SSGI` in the Stage 5 acceptance suite, a report under `Zenith2/Docs/Research/`.
- **Design:** Zenith2/Docs/Design/LIGHT/GI.md → "SSGI error budget"; Zenith2/Docs/Design/LIGHT/Tests.md → "Approximation error scenes"

### Z2-R-LIGHT-119 Indirect diffuse source order

- **Statement:** Indirect diffuse uses SSGI, then lightmaps, then irradiance volumes, then the sky SH, blending remainders in that order.
- **Covers:** Z2-I-LIGHT-04, Z2-I-LIGHT-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_IndirectDiffuse_SourceOrder`, `Test_IndirectDiffuse_LightmapSourceOrder`, `Test_IndirectDiffuse_SSGISourceOrder`; debug view 11.
- **Design:** Zenith2/Docs/Design/LIGHT/GI.md → "Indirect diffuse selection"

### Z2-R-LIGHT-120 Sky irradiance SH

- **Statement:** The sky radiance is projected to windowed L2 SH with solid-angle weights, deterministically on CPU and GPU, and evaluated as irradiance with the cosine-lobe coefficients.
- **Covers:** Z2-I-LIGHT-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_SH9_ConstantEnvironment`, `Test_SH9_CosineLobe`, `Test_SH9_GPUMatchesCPU`.
- **Design:** Zenith2/Docs/Design/LIGHT/GI.md → "Sky irradiance"

### Z2-R-LIGHT-121 LIGHT QualityMatrix rows

- **Statement:** LIGHT supplies the feature rows Z2-Q-LIGHT-001 to Z2-Q-LIGHT-021 to `Zenith2/Docs/QualityMatrix.md` in the schema of "Production-quality acceptance", with techniques per tier, fallbacks, triggers, acceptance kinds, references and thresholds.
- **Covers:** Z2-I-LIGHT-01, Z2-I-LIGHT-02, Z2-I-LIGHT-03, Z2-I-LIGHT-04, Z2-I-LIGHT-05, Z2-I-LIGHT-06, Z2-I-LIGHT-07, Z2-I-LIGHT-09, Z2-I-LIGHT-10, Z2-I-LIGHT-11, Z2-I-LIGHT-12, Z2-I-LIGHT-13, Z2-I-LIGHT-14, Z2-I-LIGHT-15, Z2-I-LIGHT-16
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-012, Z2-D-021
- **Budgets:** none
- **Acceptance evidence:** The Stage 1 QualityMatrix ticket's `QualityMatrix.md` rows, checked by the coverage gate step's schema parse.
- **Design:** Zenith2/Docs/Design/LIGHT/QualityTiers.md → "QualityMatrix rows supplied by LIGHT"

### Z2-R-LIGHT-122 Per-platform LIGHT tier values

- **Statement:** Every LIGHT tier value of "Quality tiers" is a `render.quality.*` key whose value in each tier file `<TierID>.config.json` matches the tables for each Windows and S24 tier, the Stage 3 default tiers first and the full set in Stage 5.
- **Covers:** Z2-I-LIGHT-01, Z2-I-LIGHT-02, Z2-I-LIGHT-06, Z2-I-LIGHT-07, Z2-I-LIGHT-10, Z2-I-LIGHT-11
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-RPROD-022
- **Acceptance evidence:** `Test_LightConfig_TierFileApplied` over every tier file; QualityMatrix goldens per tier.
- **Design:** Zenith2/Docs/Design/LIGHT/QualityTiers.md → "Shadow tiers"; Zenith2/Docs/Design/LIGHT/QualityTiers.md → "Volumetric tiers"; Zenith2/Docs/Design/LIGHT/QualityTiers.md → "Screen-space tiers"; Zenith2/Docs/Design/LIGHT/QualityTiers.md → "Reflection and GI tiers"; Zenith2/Docs/Design/LIGHT/QualityTiers.md → "Temporal and resolution tiers"; Zenith2/Docs/Design/LIGHT/QualityTiers.md → "Post-processing and output tiers"

### Z2-R-LIGHT-123 Fallbacks and their triggers

- **Statement:** Each LIGHT feature takes the fallback of "Fallbacks and triggers" exactly when its trigger (capability, pressure level, thermal state, missing data) occurs.
- **Covers:** Z2-I-LIGHT-01, Z2-I-LIGHT-03, Z2-I-LIGHT-04, Z2-I-LIGHT-05, Z2-I-LIGHT-06, Z2-I-LIGHT-11, Z2-I-LIGHT-14
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Lighting_CriticalPressureShedding`, `Test_Lighting_CriticalPressureShedsPost`, `Test_LightingFallbacks_TriggersApplied`, `Test_OutputMode_Selection`, `Test_SSR_MissFallsBackToProbe`, `Test_Lighting_MissingEnvironmentFallsBack`, `Test_IndirectDiffuse_SourceOrder`.
- **Design:** Zenith2/Docs/Design/LIGHT/QualityTiers.md → "Fallbacks and triggers"

### Z2-R-LIGHT-124 Async compute only for the A passes

- **Statement:** Only LightClustering, VolumetricFog, GTAO and ContactShadows carry `FLUX_PASS_FLAG_ASYNC_COMPUTE`, and results are bit-identical with async compute on or off.
- **Covers:** Z2-I-LIGHT-01, Z2-I-LIGHT-06, Z2-I-LIGHT-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-055
- **Budgets:** none
- **Acceptance evidence:** `Test_AsyncCompute_LightingIdentical`; graph unit inspection of pass flags.
- **Design:** Zenith2/Docs/Design/LIGHT.md → "Async-compute eligibility"

### Z2-R-LIGHT-125 LIGHT CPU work on Flux.BuildGraph without locks

- **Statement:** LIGHT CPU work runs on the `Flux.BuildGraph` node, reads only the sealed render world and Flux state, adds no mutex, and receives asset completions through the bounded `flux.lighting.loadQueue` with the reject policy.
- **Covers:** Z2-I-LIGHT-03, Z2-I-LIGHT-04, Z2-I-LIGHT-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-050, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `linux-tsan` stress is not applicable (GPU library); `Test_LightingSystem_LoadQueueFull` and review against the lock table.
- **Design:** Zenith2/Docs/Design/LIGHT.md → "Frame-graph placement"; Zenith2/Docs/Design/LIGHT.md → "Lock-order entries"; Zenith2/Docs/Design/LIGHT.md → "Thread affinity per type"

### Z2-R-LIGHT-126 LIGHT counters

- **Statement:** LIGHT emits every counter of "Counters" once per render frame through the profiler hooks, with GPU counters through the counter readback.
- **Covers:** Z2-I-LIGHT-01, Z2-I-LIGHT-02, Z2-I-LIGHT-03, Z2-I-LIGHT-11, Z2-I-LIGHT-12
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_LightingCounters_Emitted`.
- **Design:** Zenith2/Docs/Design/LIGHT.md → "Counters"

### Z2-R-LIGHT-127 LIGHT debug views

- **Statement:** Development builds offer every debug view of "Debug views" through `r.debug.lighting`.
- **Covers:** Z2-I-LIGHT-01, Z2-I-LIGHT-02, Z2-I-LIGHT-06, Z2-I-LIGHT-07, Z2-I-LIGHT-09, Z2-I-LIGHT-12
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_LightingDebug_EveryViewAddsPasses` (each value adds its debug pass and no validation error).
- **Design:** Zenith2/Docs/Design/LIGHT.md → "Debug views"

### Z2-R-LIGHT-128 Lighting frame-dump section

- **Statement:** The frame dump carries a `lighting` object with light counts, shadow views, splits, probe slots, exposure and dynamic-resolution scales.
- **Covers:** Z2-I-LIGHT-01, Z2-I-LIGHT-02, Z2-I-LIGHT-11, Z2-I-LIGHT-12
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameDump_LightingSection`.
- **Design:** Zenith2/Docs/Design/LIGHT.md → "Frame-dump entries"

### Z2-R-LIGHT-129 Corrupt or wrong-version lighting data

- **Statement:** Every LIGHT reader refuses truncated, malformed, non-finite, over-limit or wrong-version files with `FLUX_LIGHTING_ERROR_CORRUPT_DATA` or `_VERSION_MISMATCH`, never asserting, and the asset is treated as missing.
- **Covers:** Z2-I-LIGHT-03, Z2-I-LIGHT-04, Z2-I-LIGHT-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EnvironmentFile_RejectsCorrupt`, `Test_ProbeFile_RejectsCorrupt`, `Test_IrradianceVolumeFile_RejectsCorrupt`, `Test_LightmapSetFile_RejectsCorrupt`, `Test_GradingLUTFile_RejectsCorrupt`, and the fuzz targets under `Sanitizers -Preset win-asan`.
- **Design:** Zenith2/Docs/Design/LIGHT.md → "Failure table"; Zenith2/Docs/Design/LIGHT/Tests.md → "Format and fuzz tests"

### Z2-R-LIGHT-130 Device-loss recovery of lighting state

- **Statement:** After device loss every LIGHT history and probe slot is invalid, baked data and engine content reload, realtime probes re-capture, and the image reconverges.
- **Covers:** Z2-I-LIGHT-03, Z2-I-LIGHT-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Lighting_DeviceLossRecovery`, `Test_Lighting_DeviceLossRecoveryTemporalScene`, `Test_Probes_DeviceLossRecapture`, `Test_BakedGI_DeviceLossReload`.
- **Design:** Zenith2/Docs/Design/LIGHT.md → "Failure table"; Zenith2/Docs/Design/LIGHT/Temporal.md → "History invalidation"

### Z2-R-LIGHT-131 Memory-pressure shedding limited to the Architecture actions

- **Statement:** Under memory pressure LIGHT applies exactly Architecture's Rendering actions that touch it (SSR, DoF and motion blur off; the fog grid one step down; the floor scale; capture and preview histories dropped), reverts them one frame after the level falls, and sheds nothing else.
- **Covers:** Z2-I-LIGHT-06, Z2-I-LIGHT-07, Z2-I-LIGHT-11, Z2-I-LIGHT-15, Z2-I-LIGHT-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Lighting_CriticalPressureShedding` with Rendering's `Test_Memory_CriticalShedding`.
- **Design:** Zenith2/Docs/Design/LIGHT.md → "Failure table"; Zenith2/Docs/Design/LIGHT/QualityTiers.md → "Fallbacks and triggers"

### Z2-R-LIGHT-132 Lighting component types registered from C++

- **Statement:** Lights, probes, volumes, lightmap sets, world lighting and camera lighting are C++ component structs of `Flux_LightComponents.h` with `Zenith.Lighting.*` stable names and fixed property IDs, registered by ZenithRuntime's `Zenith_RegisterLightingComponentTypes` in startup step 6 (Q42 ruled (b); X108 applied by core-doc batch 4), and extracted with unit conversion.
- **Covers:** Z2-I-LIGHT-01, Z2-I-LIGHT-02, Z2-I-LIGHT-03, Z2-I-LIGHT-04, Z2-I-LIGHT-12, Z2-I-LIGHT-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_LightComponents_Registered` (one `SUBCASE` per later type set), `Test_LightComponents_RoundTripByPropertyID`, `Test_LightExtraction_ConvertsUnits`.
- **Design:** Zenith2/Docs/Design/LIGHT.md → "Light component schemas"; Zenith2/Docs/Design/LIGHT.md → "Flux_LightComponents header"; Zenith2/Docs/Design/LIGHT.md → "ZenithRuntime LIGHT files"

### Z2-R-LIGHT-133 LIGHT cook steps

- **Statement:** The LUT, environment, grading and bake-writer cook steps are pure, deterministic functions of their declared inputs with derived-data keys, reporting into the cook report's `lighting` member.
- **Covers:** Z2-I-LIGHT-03, Z2-I-LIGHT-04, Z2-I-LIGHT-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_Cook_DFGDeterministic`, `Test_Cook_EnvironmentDeterministic`, `Test_Cook_GradingLUTDeterministic` and `Test_Cook_LightingDeterministic` (two cooks, identical bytes); the cook report fixture.
- **Design:** Zenith2/Docs/Design/LIGHT.md → "Cook steps"

### Z2-R-LIGHT-134 Backend parity of the lighting scenes

- **Statement:** The `Lighting*` parity scenes render on Vulkan and D3D12 within their proposed per-scene tolerances, surface channels first, with aliasing on and off.
- **Covers:** Z2-I-LIGHT-01, Z2-I-LIGHT-02, Z2-I-LIGHT-06, Z2-I-LIGHT-07, Z2-I-LIGHT-09, Z2-I-LIGHT-12, Z2-I-LIGHT-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-063
- **Budgets:** Z2-B-RPROD-031
- **Acceptance evidence:** `BackendParity` gate step on the `Lighting*` scenes.
- **Design:** Zenith2/Docs/Design/LIGHT/Tests.md → "Parity scenes"

### Z2-R-LIGHT-135 Path parity of the lighting scenes

- **Statement:** Forward+, Forward, Deferred and the visibility buffer render the `Lighting*` scenes within the capability-matrix comparisons and LIGHT's path tolerances.
- **Covers:** Z2-I-LIGHT-01, Z2-I-LIGHT-02, Z2-I-LIGHT-06, Z2-I-LIGHT-07
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-063, Z2-D-035
- **Budgets:** Z2-B-RPROD-031
- **Acceptance evidence:** `PathParity` gate step on the `Lighting*` scenes.
- **Design:** Zenith2/Docs/Design/LIGHT/Tests.md → "Parity scenes"

### Z2-R-LIGHT-136 Path-tracer comparison for analytic lights

- **Statement:** The analytic point, spot, directional, sphere, rect and disc light scenes match the converged path-traced references within their tolerances.
- **Covers:** Z2-I-LIGHT-01, Z2-I-LIGHT-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-037, Z2-D-063
- **Budgets:** Z2-B-ADVGFX-006, Z2-B-ADVGFX-007, Z2-B-ADVGFX-008
- **Acceptance evidence:** The `PT_Analytic*`, `PT_SphereLight`, `PT_RectLight`, `PT_DiscLight` comparisons in the Stage 5 acceptance suite.
- **Design:** Zenith2/Docs/Design/LIGHT/Tests.md → "Path-tracer comparison scenes"

### Z2-R-LIGHT-137 IBL furnace against the path tracer

- **Statement:** The IBL furnace and environment scenes match the converged path-traced references within their tolerances.
- **Covers:** Z2-I-LIGHT-06
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-037, Z2-D-063
- **Budgets:** Z2-B-ADVGFX-008
- **Acceptance evidence:** `PT_IBLFurnace`, `PT_IBLEnvironment` in the Stage 5 acceptance suite.
- **Design:** Zenith2/Docs/Design/LIGHT/Tests.md → "Path-tracer comparison scenes"

### Z2-R-LIGHT-138 Area-light shadows against the path tracer

- **Statement:** The area-light shadow scene matches the path-traced reference within its penumbra tolerance.
- **Covers:** Z2-I-LIGHT-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-037, Z2-D-063
- **Budgets:** Z2-B-ADVGFX-004, Z2-B-ADVGFX-005, Z2-B-ADVGFX-008
- **Acceptance evidence:** `PT_AreaLightShadow` in the Stage 5 acceptance suite.
- **Design:** Zenith2/Docs/Design/LIGHT/Tests.md → "Path-tracer comparison scenes"

### Z2-R-LIGHT-139 Temporal sequence tests

- **Statement:** The temporal sequence tests of "Temporal sequence tests" pass on WARP and lavapipe with fixed jitter, seeds and exposure time step.
- **Covers:** Z2-I-LIGHT-08, Z2-I-LIGHT-09, Z2-I-LIGHT-10, Z2-I-LIGHT-12
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-059
- **Budgets:** none
- **Acceptance evidence:** The `Test_TAA_*`, `Test_Fog_TemporalStability`, `Test_Exposure_TemporalAdaptation`, `Test_Upscale_ConvergenceAtFloor` tests in the `Test` step.
- **Design:** Zenith2/Docs/Design/LIGHT/Tests.md → "Temporal sequence tests"

### Z2-R-LIGHT-140 History invalidation on teleport test

- **Statement:** A recorded teleport of the camera owner invalidates the view histories that frame and the output equals the no-history frame.
- **Covers:** Z2-I-LIGHT-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_TAA_TeleportCameraCutInvalidatesHistory`.
- **Design:** Zenith2/Docs/Design/LIGHT/Tests.md → "Temporal sequence tests"; Zenith2/Docs/Design/LIGHT/Temporal.md → "History invalidation"

### Z2-R-LIGHT-141 LIGHT budget rows allocated

- **Statement:** The Stage 1 per-system budget ticket for LIGHT allocates Z2-B-LIGHT-001 to Z2-B-LIGHT-015 and the pending LIGHT row needs, and every LIGHT ticket citing a pending row is blocked by it.
- **Covers:** Z2-I-LIGHT-01, Z2-I-LIGHT-02, Z2-I-LIGHT-06, Z2-I-LIGHT-07, Z2-I-LIGHT-09, Z2-I-LIGHT-10, Z2-I-LIGHT-12, Z2-I-LIGHT-13, Z2-I-LIGHT-15, Z2-I-LIGHT-16
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-012, Z2-D-084
- **Budgets:** Z2-B-LIGHT-001, Z2-B-LIGHT-002, Z2-B-LIGHT-003, Z2-B-LIGHT-004, Z2-B-LIGHT-005, Z2-B-LIGHT-006, Z2-B-LIGHT-007, Z2-B-LIGHT-008, Z2-B-LIGHT-009, Z2-B-LIGHT-010, Z2-B-LIGHT-011, Z2-B-LIGHT-012, Z2-B-LIGHT-013, Z2-B-LIGHT-014, Z2-B-LIGHT-015
- **Acceptance evidence:** The budget ticket's `allocation.json` and DecisionLog entry; `Check-Allocation.ps1`.
- **Design:** Zenith2/Docs/Design/LIGHT.md → "Rows owned"; Zenith2/Docs/Design/LIGHT.md → "Pending row needs"

### Z2-R-LIGHT-142 D-TBDR revalidation of Stage 5 LIGHT work

- **Statement:** Every Stage 5 LIGHT rendering ticket is blocked by the D-TBDR (open, Stage 5) revalidation, and no LIGHT pass reads an attachment tile-locally before the ruling.
- **Covers:** Z2-I-LIGHT-01, Z2-I-LIGHT-02, Z2-I-LIGHT-04, Z2-I-LIGHT-05, Z2-I-LIGHT-14, Z2-I-LIGHT-15, Z2-I-LIGHT-16
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-036, D-TBDR (open, Stage 5)
- **Budgets:** none
- **Acceptance evidence:** The revalidation ticket; `FLUX_RG_ERROR_TILE_LOCAL_UNSUPPORTED` graph test.
- **Design:** Zenith2/Docs/Design/LIGHT.md → "Stage table"; Zenith2/Docs/Design/LIGHT.md → "Open decisions"

### Z2-R-LIGHT-143 D-SEL-UPSCALER ruling

- **Statement:** D-SEL-UPSCALER (open, Stage 5), registered by cross-doc item X107 (applied by core-doc batch 4), is ruled by a harness ticket measuring the candidates against TAAU on FLIP and GPU time and a needs-human ruling ticket, adopting a candidate for a tier only when it wins on both.
- **Covers:** Z2-I-LIGHT-10
- **Stage:** S5
- **Classification:** research
- **Decisions:** Z2-D-084, Z2-D-025, D-SEL-UPSCALER (open, Stage 5)
- **Budgets:** Z2-B-LIGHT-010
- **Acceptance evidence:** The harness under `Zenith2/Benchmarks/Upscalers/` with its CTest smoke; the ruling report under `Zenith2/Docs/Research/` and the DecisionLog entry.
- **Design:** Zenith2/Docs/Design/LIGHT/Temporal.md → "Upscaler selection"; Zenith2/Docs/Design/LIGHT.md → "Selection decisions LIGHT owns"

### Z2-R-LIGHT-144 Screen-space approximations within their budgets

- **Statement:** GTAO, contact shadows and SSR meet the documented error budgets of the approximation error scenes, validated against those budgets rather than path-tracer convergence.
- **Covers:** Z2-I-LIGHT-01, Z2-I-LIGHT-06
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-063
- **Budgets:** none
- **Acceptance evidence:** `AE_GTAO`, `AE_ContactShadows`, `AE_SSR` in the Stage 5 acceptance suite.
- **Design:** Zenith2/Docs/Design/LIGHT/Tests.md → "Approximation error scenes"

### Z2-R-LIGHT-145 Light keys from two disjoint functions

- **Statement:** Every light key comes from `Flux_MakeEntityLightKey` (bit 63 clear) or `Flux_MakeNamedLightKey` (bit 63 set), both `Zenith_Hash64` with the world tag as seed, so an entity light and a named engine light (ENV's sun and moon) never share a key.
- **Covers:** Z2-I-LIGHT-01, Z2-I-LIGHT-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_LightKey_EntityAndNamedDisjoint`, `Test_LightKey_NamedMatchesHash`.
- **Design:** Zenith2/Docs/Design/LIGHT/Lighting.md → "Light keys"

### Z2-R-LIGHT-146 One validation verdict for the GPU scene and the mirror

- **Statement:** `Flux_ValidateLightDelta` is the one validation of a light delta: the GPU scene and the CPU light mirror both call it, so a delta refused for the table is refused for the mirror in the same frame, and the 112-byte `Flux_RenderLightDelta` layout is pinned by its offset table.
- **Covers:** Z2-I-LIGHT-01, Z2-I-LIGHT-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-058
- **Budgets:** none
- **Acceptance evidence:** `Test_LightDelta_ValidationSharedVerdict`, `Test_LightDelta_Layout`.
- **Design:** Zenith2/Docs/Design/LIGHT/Lighting.md → "Division of the delta work with the GPU scene"; Zenith2/Docs/Design/LIGHT/Lighting.md → "The delta record"; Zenith2/Docs/Design/LIGHT/Lighting.md → "Validation"

### Z2-R-LIGHT-147 Sun and moon slots are directional lights 0 and 1

- **Statement:** From Stage 3 (moved from Stage 4 by the Q45 ruling, Z2-D-131), the render environment's sun and moon slots occupy the two reserved light-table slots, which no delta and no full-state resync ever hands out, and are directional entries 0 and 1 whatever other directional lights the scene holds; their role is the slot position, never a flag (bits 5–7 of `m_uFlags` are reserved, and `Flux_ValidateEnvironmentLight` takes no role argument and clears and counts a set reserved bit exactly as for a delta); ENV writes them with LIGHT's slot flag contract (sun: casts-shadow, contact shadows and volumetric; moon: volumetric, and casts-shadow above 1 lux), a missing bit never makes a slot invalid, and the moon's contact-shadow bit is cleared and counted; with no sun slot (a world without ENV's celestial state) the brightest scene directional light takes the sun role when the atmosphere is enabled.
- **Covers:** Z2-I-LIGHT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-058
- **Budgets:** none
- **Acceptance evidence:** `Test_LightList_SunMoonSlotsFirst`, `Test_Lighting_SunMoonSlotsReserved`, `Test_Lighting_SunMoonSlotFlags`, `Test_Lighting_FullStateKeepsReservedSlots`, `Test_LightDelta_InvalidRefused`.
- **Design:** Zenith2/Docs/Design/LIGHT/Lighting.md → "Sun and moon slots"; Zenith2/Docs/Design/LIGHT/Lighting.md → "Light and decal list build"; Zenith2/Docs/Design/LIGHT/Lighting.md → "Light record encodings"; Zenith2/Docs/Design/LIGHT/Lighting.md → "Validation"

### Z2-R-LIGHT-148 One shadow-planning entry

- **Statement:** `Flux_LightingSystem::PlanShadows(xNonShadowViews, xFrameArena)`, called by RPROD's `Flux_BuildViewList`, is the one place shadowed lights are selected, cascades fitted, tiles sized and allocated, shadow-view records written and changed light records staged, and its output is a pure function of its inputs and the per-light hysteresis state.
- **Covers:** Z2-I-LIGHT-01, Z2-I-LIGHT-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-059, Z2-D-055
- **Budgets:** Z2-B-LIGHT-002
- **Acceptance evidence:** `Test_PlanShadows_Deterministic`, `Test_PlanShadows_StagesChangedRecords`, `Test_PlanShadows_InvalidSettingsNoViews`.
- **Design:** Zenith2/Docs/Design/LIGHT/Shadows.md → "Shadow planning in view-list construction"; Zenith2/Docs/Design/LIGHT/Shadows.md → "Shadowed-light budget"

### Z2-R-LIGHT-149 One shadow-atlas tile and plan type in a leaf header

- **Statement:** `Flux_RenderLightKey`, `Flux_AtlasTile`, `Flux_ShadowViewKind`, `Flux_ShadowViewPlan` and `Flux_ShadowAtlasAllocator` are declared once, in `Flux_ShadowAtlas.h`, which includes only ZenithCore headers and `Flux_LightingErrors.h`, so RPROD's `Flux_View.h` and `Flux_ViewContext.h` include it without an include cycle and no other header declares any of these types.
- **Covers:** Z2-I-LIGHT-01, Z2-I-LIGHT-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-047
- **Budgets:** none
- **Acceptance evidence:** `HeaderSelfContained` over `Flux_ShadowAtlas.h`; `Test_ShadowAtlas_LeafHeaderIncludes`.
- **Design:** Zenith2/Docs/Design/LIGHT/Shadows.md → "Flux_ShadowAtlas header"

### Z2-R-LIGHT-150 Cluster layout fixed per capability tier

- **Statement:** `Flux_GetClusterLayout` returns the grid, far distance and per-cluster capacities of the capability tier as compile-time constants (16 × 9 × 24, 2,000 m, 128, 32, 8 desktop; 8 × 5 × 16, 1,000 m, 48, 8, 4 mobile), and no configuration key changes them.
- **Covers:** Z2-I-LIGHT-01, Z2-I-LIGHT-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-035
- **Budgets:** Z2-B-LIGHT-003
- **Acceptance evidence:** `Test_Clusters_LayoutPerCapabilityTier`.
- **Design:** Zenith2/Docs/Design/LIGHT/Lighting.md → "Cluster grid"

### Z2-R-LIGHT-151 One upscaler enumeration

- **Statement:** `Flux_UpscalerMode` in `Flux_Upscaler.h` is the only upscaler enumeration in ZenithFlux, and RPROD's quality settings carry the upscaler only as `m_xLight.m_xTemporal.m_eUpscaler`.
- **Covers:** Z2-I-LIGHT-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-035
- **Budgets:** none
- **Acceptance evidence:** `Test_LightConfig_UpscalerValues`; review of `Flux_Quality.h` against the header.
- **Design:** Zenith2/Docs/Design/LIGHT/Temporal.md → "Flux_Upscaler header"; Zenith2/Docs/Design/LIGHT/Temporal.md → "Upscaler selection"

### Z2-R-LIGHT-152 LIGHT quality values live only in m_xLight

- **Statement:** Every quality value whose rendering meaning is LIGHT's (shadows, froxel grid, SSAO, SSR, TAA, upscaler, bloom, contact shadows, dynamic-resolution range) is a member of `Flux_QualitySettings::m_xLight` bound to exactly one `render.quality.*` key, and no key is registered twice across ZenithFlux.
- **Covers:** Z2-I-LIGHT-01, Z2-I-LIGHT-07, Z2-I-LIGHT-09, Z2-I-LIGHT-10, Z2-I-LIGHT-11, Z2-I-LIGHT-12
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_LightConfig_NoDuplicateQualityKeys`, `Test_LightConfig_RegisterAllKeys`.
- **Design:** Zenith2/Docs/Design/LIGHT.md → "Flux_LightSettings header"; Zenith2/Docs/Design/LIGHT.md → "Configuration keys"

### Z2-R-LIGHT-153 The controller is the only writer of the dynamic-resolution scale

- **Statement:** A view with a dynamic-resolution controller renders at `GetScale()` and nothing else sets its scale; a view without a controller renders at scale 1.0; `HasDynamicResolution` tells which views have one, and a controller consumes each GPU-time sample (by its frame index) once.
- **Covers:** Z2-I-LIGHT-11
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-SHIP-011
- **Acceptance evidence:** `Test_DynRes_ControllerIsSoleScaleSource`, `Test_DynRes_SampleConsumedOnce`.
- **Design:** Zenith2/Docs/Design/LIGHT/Temporal.md → "Dynamic-resolution controller"; Zenith2/Docs/Design/LIGHT/Temporal.md → "Thermal and pressure inputs"

### Z2-R-LIGHT-154 Star field behind the atmosphere

- **Statement:** The sky draw adds ENV's star cube behind the atmosphere as `T_view(ω) × L_star(R⁻¹ω) × intensity`, zero below the horizon and absent without a star cube, and the sky capture includes it.
- **Covers:** Z2-I-LIGHT-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-ENV-001
- **Acceptance evidence:** `Test_Sky_StarFieldBehindAtmosphere`.
- **Design:** Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Star field"; Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Sky draw and aerial perspective"

### Z2-R-LIGHT-155 Cloud layer composite

- **Statement:** When ENV's cloud layer ran, the sky draw and the sky capture composite `cloudRadiance + cloudTransmittance × sky` behind the aerial perspective, rescaling by the pre-exposure ratio for views other than the main view, and with the targets unset the sky is exactly the cloudless sky.
- **Covers:** Z2-I-LIGHT-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-ENV-001
- **Acceptance evidence:** `Test_Sky_CloudLayerComposite`.
- **Design:** Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Cloud layer composite"

### Z2-R-LIGHT-156 Cloud shadow on the sun only

- **Statement:** When ENV's cloud-shadow map is present, the sun term of `EvaluateLightingEx` and of the froxel injection is multiplied by `Flux_SampleCloudShadow`, the outside-square transmittance applies beyond the map, and no other light, the moon or the sun disc is affected.
- **Covers:** Z2-I-LIGHT-01, Z2-I-LIGHT-07
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Lighting_CloudShadowOnSunOnly`.
- **Design:** Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Cloud shadow on the sun"; Zenith2/Docs/Design/LIGHT/Lighting.md → "The EvaluateLighting contract"

### Z2-R-LIGHT-157 Underwater medium override

- **Statement:** LIGHT copies the `m_xMedium` of each ENV `Flux_RenderEnvironmentView` record present in the frame (ENV writes one only for a submerged view, at most 16 per world) into a `Flux_ViewMediumOverride` keyed by its view ID, refusing and counting invalid coefficients, keeps at most 16 per frame across worlds (the first in world-tag then written order, the rest dropped and counted; neither ENV's blocks resource nor `Flux_AtmosphereBlock` carries an override), and the view whose ID matches injects the homogeneous water medium in place of the height fog, attenuates the sun and moon by the water column, and applies the per-channel transmittance reconstructed from the exponent ratio, exact for the homogeneous medium.
- **Covers:** Z2-I-LIGHT-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-LIGHT-004
- **Acceptance evidence:** `Test_Fog_UnderwaterOverride`.
- **Design:** Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Underwater medium override"; Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Integration"; Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Fog application"

### Z2-R-LIGHT-158 Atmosphere LUTs rebuild on a sky revision change only

- **Statement:** The transmittance and multiple-scattering LUTs are rebuilt only in the first frame, after device loss, on an atmosphere-block validity change or on any `m_uSkyRevision` change, every other frame reads the persistent copies, and the revision a rebuild was made for is committed only when its frame submitted, so a deferred or failed rebuild frame leaves the rebuild pending for the next built frame.
- **Covers:** Z2-I-LIGHT-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-ENV-001
- **Acceptance evidence:** `Test_Sky_LUTRebuildOnRevisionOnly`.
- **Design:** Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Transmittance LUT"; Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Multiple-scattering LUT"; Zenith2/Docs/Design/LIGHT.md → "Deferred and failed frames"

### Z2-R-LIGHT-159 Lighting environment blocks and their defaults

- **Statement:** `Flux_RenderLightingEnvironment` carries the atmosphere block and the world-lighting block, both filled by the one engine environment extractor (X125); an absent atmosphere block gives LIGHT's atmosphere defaults with height fog off, no stars and no cloud shadow, and an absent world-lighting block the engine default environment. The per-view underwater medium is not part of either block.
- **Covers:** Z2-I-LIGHT-06, Z2-I-LIGHT-07, Z2-I-LIGHT-12, Z2-I-LIGHT-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_LightingEnvironment_BlockDefaults`.
- **Design:** Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Atmosphere ownership"; Zenith2/Docs/Design/LIGHT.md → "Flux_LightSettings header"

### Z2-R-LIGHT-160 Specular-weight target only with SSR

- **Statement:** The forward opaque programs write the second colour target `m_xSpecularWeight` only in the `bPERM_SSR_WEIGHT` permutation, selected when the view's `FLUX_VIEW_FEATURE_SSR` bit is set, so a tier without SSR declares, writes and stores no such target.
- **Covers:** Z2-I-LIGHT-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-014
- **Budgets:** Z2-B-LIGHT-007
- **Acceptance evidence:** `Test_SSR_SpecularWeightOnlyWithSSR` (graph inspection on `QUALITY_TIER_S24_DEFAULT` and `QUALITY_TIER_WIN_DEFAULT`).
- **Design:** Zenith2/Docs/Design/LIGHT/Reflections.md → "SSR composite"

### Z2-R-LIGHT-161 Lightmap binding follows the X106 ruling

- **Statement:** Two instances of one mesh with different lightmap rectangles sample their own lightmap texels under whichever binding the X106 ruling selects, and no Stage 5 lightmap ticket lands a binding before the ruling.
- **Covers:** Z2-I-LIGHT-04
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-058, Z2-D-022
- **Budgets:** none
- **Acceptance evidence:** `Test_Lightmap_BindingPerInstance`; the ruling recorded on X106 and FLAGGED_PROPOSALS row LIGHT-1.
- **Design:** Zenith2/Docs/Design/LIGHT/GI.md → "Lightmap binding"

### Z2-R-LIGHT-162 Dynamic GI on the tiers without SSGI

- **Statement:** On the tiers without SSGI (WIN_LOW, WIN_DEFAULT and every S24 tier, as ruled by Q41 (a) on 2026-10-16), dynamic diffuse GI is the sky-SH recapture on a time-of-day change and realtime probe recapture, visible within 7 frames of the change, and the catalogue records the platform scope in "Excluded and deferred".
- **Covers:** Z2-I-LIGHT-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_IndirectDiffuse_SourceOrder` on `QUALITY_TIER_S24_DEFAULT`, `Test_SkyCapture_UpdatesWithinSevenFrames`; the S24 Forward+ lighting device run.
- **Design:** Zenith2/Docs/Design/LIGHT/GI.md → "SSGI platform scope"; Zenith2/Docs/Design/LIGHT/Reflections.md → "Sky capture"

### Z2-R-LIGHT-163 LIGHT tickets never edit core documents

- **Statement:** Every addition LIGHT needs to a core document is listed in "Rendering table additions pending X103" (the payload of X103 and X148) or its own cross-doc item (X80, X104 to X108, X125); a LIGHT ticket that depends on one cites it, edits no core document, and is blocked until the orchestrator marks the item applied.
- **Covers:** Z2-I-LIGHT-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-085, Z2-D-022
- **Budgets:** none
- **Acceptance evidence:** The drafted LIGHT tickets' `Cites:` and blocker lines, checked in the ticket review; review of each LIGHT diff for core-document edits.
- **Design:** Zenith2/Docs/Design/LIGHT.md → "Rendering table additions pending X103"; Zenith2/Docs/Design/LIGHT.md → "Assumed types from pending design documents"

### Z2-R-LIGHT-164 LIGHT per-frame upload data follows the build-node order

- **Statement:** Every LIGHT producer of per-frame upload data (shadow-view records, the light-list view frusta, the frame probe records, the per-view local fog volumes and irradiance-volume records) declares its bytes with `DeclareFrameData` during graph setup and writes them only in `Flux_LightingSystem::WriteFrameData()`, on the ring `Create` borrowed, through `GetFrameData` after the frame's `ExtendReservation` returned `FITS` or `EXTENDED`; each consumer field binds the frame chunk LIGHT imports once per frame with `ImportFrameChunk` and locates its records by a plain-data element base filled with the parameter struct; LIGHT allocates and writes no ring memory before the extension, and plain data reaches the GPU only through the record jobs' parameter structs (RHI row R18, LIGHT rows L35 and L42, X129).
- **Covers:** Z2-I-LIGHT-01, Z2-I-LIGHT-02, Z2-I-LIGHT-03, Z2-I-LIGHT-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-055
- **Budgets:** Z2-B-RHI-006
- **Acceptance evidence:** `Test_Lighting_FrameDataDeclaredBeforeExtension`, `Test_Lighting_FrameDataOrderInForwardPlusFrame`; RHI's `Test_UploadRing_AllocateBeforeExtendAsserts` over a LIGHT frame.
- **Design:** Zenith2/Docs/Design/LIGHT.md → "Build-node upload data"; Zenith2/Docs/Design/LIGHT/Shadows.md → "Flux_Shadows header"; Zenith2/Docs/Design/LIGHT/Reflections.md → "Reflection probe record"

### Z2-R-LIGHT-165 A frame that submits nothing advances no LIGHT frame state

- **Statement:** `Flux_LightingSystem::EndFrame` commits the frame's pending LIGHT state only when the frame's outcome is `FLUX_FRAME_OUTCOME_SUBMITTED`: on a deferred or failed frame the atmosphere LUT rebuild stays pending, no history validity, jitter index, sky-capture face, probe-capture count, residency counter, shadow tile hysteresis or `m_uLastShadowIndex` advances, the exposure keeps its value through the failed readback without a reset, and `Flux.Lighting.FramesNotSubmitted` counts the frame.
- **Covers:** Z2-I-LIGHT-08, Z2-I-LIGHT-03, Z2-I-LIGHT-07, Z2-I-LIGHT-12
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-059
- **Budgets:** none
- **Acceptance evidence:** `Test_Lighting_DeferredFrameKeepsState` (deferred and failed variants), `Test_Probes_DeferredFrameKeepsState`, `Test_SSGI_DeferredFrameKeepsState`, `Test_Sky_LUTRebuildOnRevisionOnly`.
- **Design:** Zenith2/Docs/Design/LIGHT.md → "Deferred and failed frames"; Zenith2/Docs/Design/LIGHT.md → "History validity state machine"; Zenith2/Docs/Design/LIGHT.md → "Probe residency state machine"

### Z2-R-LIGHT-166 LIGHT shared GPU structs live in Common modules

- **Statement:** Every `[FluxShared]` struct LIGHT declares is in its assigned module under `Zenith2/Shaders/Common/`: `Flux_AtmosphereGPU`, `Flux_FogMediumGPU`, `Flux_MediumOverrideGPU` and `Flux_LocalFogVolumeGPU` in `Common/Media.slang`, `Flux_IrradianceVolumeRecord` and `Flux_LightmapPageRecord` in `Common/GI.slang`, `Flux_ReflectionProbeRecord` in `Common/Probes.slang`, and `Flux_InstanceLightingRecord` (if X106 is accepted) in `Common/GPUScene.slang`, with the layouts of their tables and only admitted member types (X148).
- **Covers:** Z2-I-LIGHT-04, Z2-I-LIGHT-03, Z2-I-LIGHT-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-015
- **Budgets:** none
- **Acceptance evidence:** `Test_LightingSharedTypes_Layout`, `Test_GIRecords_Layout`; the shader compiler's `Test_ShaderCompiler_SharedLocationRejected` and `Test_ShaderCompiler_SharedTypesLayoutMatchesReflection` over the LIGHT modules.
- **Design:** Zenith2/Docs/Design/LIGHT/Volumetrics.md → "GPU parameter records"; Zenith2/Docs/Design/LIGHT/GI.md → "GI GPU records"; Zenith2/Docs/Design/LIGHT/Reflections.md → "Reflection probe record"; Zenith2/Docs/Design/LIGHT/GI.md → "Binding if X106 is accepted"

### Z2-R-LIGHT-167 EvaluateLightingEx reaches the directional and atmosphere data through its light source

- **Statement:** Step 3 of `EvaluateLightingEx` reads the directional entries and their roles from the frame light list through `Flux_LightSource::GetDirectionalCount` and `GetDirectional`, and the transmittance LUT, the atmosphere geometry, media and enabled flag and the cloud-shadow map and mapping through `GetAtmosphere`, which every lighting pass and `VolumetricFog` fill from their `m_xLightList`, `m_xTransmittanceLUT`, `m_xAtmosphere`, `m_xCloudShadowMap` and `m_xCloudShadowMapping` fields (X148); the function stays pure over its three arguments and both light sources give the same directional result.
- **Covers:** Z2-I-LIGHT-01, Z2-I-LIGHT-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-035
- **Budgets:** none
- **Acceptance evidence:** `Test_Lighting_DirectionalDataPath`, `Test_Lighting_EvaluationOrderReference`.
- **Design:** Zenith2/Docs/Design/LIGHT/Lighting.md → "The EvaluateLighting contract"; Zenith2/Docs/Design/LIGHT/Lighting.md → "Evaluation order"; Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Sun transmittance"; Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Scattering injection"; Zenith2/Docs/Design/LIGHT.md → "Rendering table additions pending X103"

### Z2-R-LIGHT-168 Probe keys derived from the entity identity

- **Statement:** `Flux_MakeProbeKey(xEntity, uWorldTag)` is the one derivation of a `Flux_RenderProbeKey`: `Zenith_Hash64` of the 16 bytes of the entity's persistent ID with seed `uWorldTag`, bit 63 cleared, the derivation of `Flux_MakeEntityLightKey`; probe keys are never serialized.
- **Covers:** Z2-I-LIGHT-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_ProbeKey_MatchesEntityDerivation`.
- **Design:** Zenith2/Docs/Design/LIGHT/Reflections.md → "Flux_Reflections header"

### Z2-R-LIGHT-169 Probe filter and sky capture passes follow their pass tables

- **Statement:** The `ProbeFilter` pass and the `SkyCapture` and `SHProject` passes are implemented with exactly the programs, parameter fields (names, C++ types, access, stages), outputs and formats, queue G and permutations of their pass tables, which join Rendering's per-pass specifications through X148; the filter's destination and the capture face are plain writes of persistent-imported resources, never `[FullCoverage]` or `[WriteOnly]`, so every layer a step does not index keeps its contents.
- **Covers:** Z2-I-LIGHT-03, Z2-I-LIGHT-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-015, Z2-D-055
- **Budgets:** none
- **Acceptance evidence:** `Test_LightingPasses_DeclareTableFields`, `Test_SkyCapture_DeclareTableFields`, `Test_Probes_FramePassesDeclareTableFields`, `Test_ProbeFilter_GPUMatchesCook`, `Test_SkyCapture_UpdatesWithinSevenFrames`.
- **Design:** Zenith2/Docs/Design/LIGHT/Reflections.md → "Probe filter pass table"; Zenith2/Docs/Design/LIGHT/Reflections.md → "Sky capture pass table"

### Z2-R-LIGHT-170 SSGI follows its pass table

- **Statement:** The `SSGI` pass (`SSGITrace` then two `SSGIDenoise` iterations) is implemented with exactly the programs, parameter fields, outputs, formats, queue G and permutations of "SSGI pass table", which joins Rendering's frame anatomy through X105 and X148.
- **Covers:** Z2-I-LIGHT-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-015, Z2-D-055
- **Budgets:** none
- **Acceptance evidence:** `Test_SSGI_DeclareTableFields`, `Test_SSGI_HistoryInvalidation`.
- **Design:** Zenith2/Docs/Design/LIGHT/GI.md → "SSGI pass table"; Zenith2/Docs/Design/LIGHT/GI.md → "SSGI"

## Coverage of inventory items

| Item | Text | Requirements |
|---|---|---|
| Z2-I-LIGHT-01 | directional-light shadows | Z2-R-LIGHT-004, Z2-R-LIGHT-054, Z2-R-LIGHT-055, Z2-R-LIGHT-056, Z2-R-LIGHT-057, Z2-R-LIGHT-058, Z2-R-LIGHT-059, Z2-R-LIGHT-060, Z2-R-LIGHT-061, Z2-R-LIGHT-062, Z2-R-LIGHT-063, Z2-R-LIGHT-064, Z2-R-LIGHT-065, Z2-R-LIGHT-066, Z2-R-LIGHT-067, Z2-R-LIGHT-068, Z2-R-LIGHT-069, Z2-R-LIGHT-071, Z2-R-LIGHT-072, Z2-R-LIGHT-073, Z2-R-LIGHT-074, Z2-R-LIGHT-077, Z2-R-LIGHT-078, Z2-R-LIGHT-079, Z2-R-LIGHT-080, Z2-R-LIGHT-081, Z2-R-LIGHT-121, Z2-R-LIGHT-122, Z2-R-LIGHT-123, Z2-R-LIGHT-124, Z2-R-LIGHT-126, Z2-R-LIGHT-127, Z2-R-LIGHT-128, Z2-R-LIGHT-132, Z2-R-LIGHT-134, Z2-R-LIGHT-135, Z2-R-LIGHT-136, Z2-R-LIGHT-141, Z2-R-LIGHT-142, Z2-R-LIGHT-144, Z2-R-LIGHT-145, Z2-R-LIGHT-146, Z2-R-LIGHT-147, Z2-R-LIGHT-148, Z2-R-LIGHT-149, Z2-R-LIGHT-150, Z2-R-LIGHT-152, Z2-R-LIGHT-156, Z2-R-LIGHT-164, Z2-R-LIGHT-167 |
| Z2-I-LIGHT-02 | local-light shadows | Z2-R-LIGHT-054, Z2-R-LIGHT-055, Z2-R-LIGHT-056, Z2-R-LIGHT-057, Z2-R-LIGHT-058, Z2-R-LIGHT-059, Z2-R-LIGHT-060, Z2-R-LIGHT-061, Z2-R-LIGHT-062, Z2-R-LIGHT-063, Z2-R-LIGHT-064, Z2-R-LIGHT-065, Z2-R-LIGHT-066, Z2-R-LIGHT-067, Z2-R-LIGHT-068, Z2-R-LIGHT-069, Z2-R-LIGHT-070, Z2-R-LIGHT-075, Z2-R-LIGHT-076, Z2-R-LIGHT-077, Z2-R-LIGHT-078, Z2-R-LIGHT-079, Z2-R-LIGHT-080, Z2-R-LIGHT-082, Z2-R-LIGHT-121, Z2-R-LIGHT-122, Z2-R-LIGHT-126, Z2-R-LIGHT-127, Z2-R-LIGHT-128, Z2-R-LIGHT-132, Z2-R-LIGHT-134, Z2-R-LIGHT-135, Z2-R-LIGHT-136, Z2-R-LIGHT-138, Z2-R-LIGHT-141, Z2-R-LIGHT-142, Z2-R-LIGHT-145, Z2-R-LIGHT-146, Z2-R-LIGHT-148, Z2-R-LIGHT-149, Z2-R-LIGHT-150, Z2-R-LIGHT-164 |
| Z2-I-LIGHT-03 | probes | Z2-R-LIGHT-103, Z2-R-LIGHT-104, Z2-R-LIGHT-105, Z2-R-LIGHT-106, Z2-R-LIGHT-107, Z2-R-LIGHT-108, Z2-R-LIGHT-109, Z2-R-LIGHT-112, Z2-R-LIGHT-121, Z2-R-LIGHT-123, Z2-R-LIGHT-125, Z2-R-LIGHT-126, Z2-R-LIGHT-129, Z2-R-LIGHT-130, Z2-R-LIGHT-132, Z2-R-LIGHT-133, Z2-R-LIGHT-164, Z2-R-LIGHT-165, Z2-R-LIGHT-166, Z2-R-LIGHT-168, Z2-R-LIGHT-169 |
| Z2-I-LIGHT-04 | baked GI | Z2-R-LIGHT-110, Z2-R-LIGHT-111, Z2-R-LIGHT-112, Z2-R-LIGHT-113, Z2-R-LIGHT-114, Z2-R-LIGHT-115, Z2-R-LIGHT-116, Z2-R-LIGHT-119, Z2-R-LIGHT-121, Z2-R-LIGHT-123, Z2-R-LIGHT-125, Z2-R-LIGHT-129, Z2-R-LIGHT-132, Z2-R-LIGHT-133, Z2-R-LIGHT-142, Z2-R-LIGHT-161, Z2-R-LIGHT-166 |
| Z2-I-LIGHT-05 | dynamic GI | Z2-R-LIGHT-117, Z2-R-LIGHT-118, Z2-R-LIGHT-119, Z2-R-LIGHT-121, Z2-R-LIGHT-123, Z2-R-LIGHT-142, Z2-R-LIGHT-162, Z2-R-LIGHT-170 |
| Z2-I-LIGHT-06 | reflections | Z2-R-LIGHT-059, Z2-R-LIGHT-061, Z2-R-LIGHT-065, Z2-R-LIGHT-067, Z2-R-LIGHT-083, Z2-R-LIGHT-084, Z2-R-LIGHT-085, Z2-R-LIGHT-096, Z2-R-LIGHT-097, Z2-R-LIGHT-098, Z2-R-LIGHT-099, Z2-R-LIGHT-100, Z2-R-LIGHT-101, Z2-R-LIGHT-102, Z2-R-LIGHT-108, Z2-R-LIGHT-109, Z2-R-LIGHT-120, Z2-R-LIGHT-121, Z2-R-LIGHT-122, Z2-R-LIGHT-123, Z2-R-LIGHT-124, Z2-R-LIGHT-127, Z2-R-LIGHT-131, Z2-R-LIGHT-134, Z2-R-LIGHT-135, Z2-R-LIGHT-137, Z2-R-LIGHT-141, Z2-R-LIGHT-144, Z2-R-LIGHT-159, Z2-R-LIGHT-160, Z2-R-LIGHT-169 |
| Z2-I-LIGHT-07 | volumetrics | Z2-R-LIGHT-004, Z2-R-LIGHT-059, Z2-R-LIGHT-083, Z2-R-LIGHT-084, Z2-R-LIGHT-085, Z2-R-LIGHT-086, Z2-R-LIGHT-087, Z2-R-LIGHT-088, Z2-R-LIGHT-089, Z2-R-LIGHT-090, Z2-R-LIGHT-091, Z2-R-LIGHT-092, Z2-R-LIGHT-093, Z2-R-LIGHT-094, Z2-R-LIGHT-095, Z2-R-LIGHT-121, Z2-R-LIGHT-122, Z2-R-LIGHT-124, Z2-R-LIGHT-127, Z2-R-LIGHT-131, Z2-R-LIGHT-134, Z2-R-LIGHT-135, Z2-R-LIGHT-141, Z2-R-LIGHT-152, Z2-R-LIGHT-154, Z2-R-LIGHT-155, Z2-R-LIGHT-156, Z2-R-LIGHT-157, Z2-R-LIGHT-158, Z2-R-LIGHT-159, Z2-R-LIGHT-164, Z2-R-LIGHT-165, Z2-R-LIGHT-166, Z2-R-LIGHT-167 |
| Z2-I-LIGHT-08 | temporal-history management | Z2-R-LIGHT-001, Z2-R-LIGHT-002, Z2-R-LIGHT-003, Z2-R-LIGHT-005, Z2-R-LIGHT-006, Z2-R-LIGHT-007, Z2-R-LIGHT-008, Z2-R-LIGHT-009, Z2-R-LIGHT-010, Z2-R-LIGHT-011, Z2-R-LIGHT-012, Z2-R-LIGHT-013, Z2-R-LIGHT-089, Z2-R-LIGHT-125, Z2-R-LIGHT-130, Z2-R-LIGHT-139, Z2-R-LIGHT-140, Z2-R-LIGHT-163, Z2-R-LIGHT-165 |
| Z2-I-LIGHT-09 | antialiasing | Z2-R-LIGHT-004, Z2-R-LIGHT-012, Z2-R-LIGHT-013, Z2-R-LIGHT-014, Z2-R-LIGHT-015, Z2-R-LIGHT-016, Z2-R-LIGHT-017, Z2-R-LIGHT-018, Z2-R-LIGHT-021, Z2-R-LIGHT-121, Z2-R-LIGHT-127, Z2-R-LIGHT-134, Z2-R-LIGHT-139, Z2-R-LIGHT-141, Z2-R-LIGHT-152 |
| Z2-I-LIGHT-10 | upscaling | Z2-R-LIGHT-019, Z2-R-LIGHT-020, Z2-R-LIGHT-021, Z2-R-LIGHT-121, Z2-R-LIGHT-122, Z2-R-LIGHT-139, Z2-R-LIGHT-141, Z2-R-LIGHT-143, Z2-R-LIGHT-151, Z2-R-LIGHT-152 |
| Z2-I-LIGHT-11 | dynamic resolution | Z2-R-LIGHT-006, Z2-R-LIGHT-010, Z2-R-LIGHT-022, Z2-R-LIGHT-023, Z2-R-LIGHT-024, Z2-R-LIGHT-025, Z2-R-LIGHT-026, Z2-R-LIGHT-027, Z2-R-LIGHT-028, Z2-R-LIGHT-121, Z2-R-LIGHT-122, Z2-R-LIGHT-123, Z2-R-LIGHT-126, Z2-R-LIGHT-128, Z2-R-LIGHT-131, Z2-R-LIGHT-152, Z2-R-LIGHT-153 |
| Z2-I-LIGHT-12 | exposure | Z2-R-LIGHT-004, Z2-R-LIGHT-005, Z2-R-LIGHT-029, Z2-R-LIGHT-030, Z2-R-LIGHT-031, Z2-R-LIGHT-032, Z2-R-LIGHT-033, Z2-R-LIGHT-034, Z2-R-LIGHT-121, Z2-R-LIGHT-126, Z2-R-LIGHT-127, Z2-R-LIGHT-128, Z2-R-LIGHT-132, Z2-R-LIGHT-134, Z2-R-LIGHT-139, Z2-R-LIGHT-141, Z2-R-LIGHT-152, Z2-R-LIGHT-159, Z2-R-LIGHT-165 |
| Z2-I-LIGHT-13 | colour management | Z2-R-LIGHT-004, Z2-R-LIGHT-005, Z2-R-LIGHT-035, Z2-R-LIGHT-036, Z2-R-LIGHT-037, Z2-R-LIGHT-038, Z2-R-LIGHT-039, Z2-R-LIGHT-044, Z2-R-LIGHT-045, Z2-R-LIGHT-046, Z2-R-LIGHT-047, Z2-R-LIGHT-121, Z2-R-LIGHT-129, Z2-R-LIGHT-132, Z2-R-LIGHT-133, Z2-R-LIGHT-134, Z2-R-LIGHT-141, Z2-R-LIGHT-159 |
| Z2-I-LIGHT-14 | HDR | Z2-R-LIGHT-040, Z2-R-LIGHT-041, Z2-R-LIGHT-042, Z2-R-LIGHT-043, Z2-R-LIGHT-045, Z2-R-LIGHT-121, Z2-R-LIGHT-123, Z2-R-LIGHT-142 |
| Z2-I-LIGHT-15 | motion blur | Z2-R-LIGHT-013, Z2-R-LIGHT-051, Z2-R-LIGHT-052, Z2-R-LIGHT-053, Z2-R-LIGHT-121, Z2-R-LIGHT-131, Z2-R-LIGHT-141, Z2-R-LIGHT-142 |
| Z2-I-LIGHT-16 | depth of field | Z2-R-LIGHT-048, Z2-R-LIGHT-049, Z2-R-LIGHT-050, Z2-R-LIGHT-121, Z2-R-LIGHT-131, Z2-R-LIGHT-141, Z2-R-LIGHT-142 |

## Excluded and deferred

Every LIGHT inventory item is classified required. One requirement is research (Z2-R-LIGHT-143, the D-SEL-UPSCALER ruling); none is deferred, excluded or game-side. One item carries a platform scope, ruled by the human on 2026-10-16 (Q41 (a)) and recorded as a classification note in the first row below, and the boundaries that keep the area bounded are exclusions of capabilities from LIGHT:

| Scope note or exclusion | Rationale | Decision, ruling or owner |
|---|---|---|
| Z2-I-LIGHT-05 dynamic GI on `QUALITY_TIER_WIN_LOW`, `QUALITY_TIER_WIN_DEFAULT` and every `QUALITY_TIER_S24_*` tier: sky-SH recapture and realtime probe recapture only; no screen-space or probe-volume dynamic diffuse GI (Z2-R-LIGHT-162); SSGI on `QUALITY_TIER_WIN_HIGH` and `_EPIC` (Z2-R-LIGHT-117) | the S24 GPU budget has no room for a second GI technique, and the minimum-specification image uses one technique (`Zenith2/Docs/Design/LIGHT/GI.md → "SSGI platform scope"`) | Q41 (a), HUMAN RULED 2026-10-16: classification note — Z2-I-LIGHT-05 stays required at S5, realised by Z2-R-LIGHT-117 (SSGI on `QUALITY_TIER_WIN_HIGH` and `_EPIC`) and Z2-R-LIGHT-162 (sky-SH and probe recapture on every other tier) |
| hardware ray-traced shadows, reflections and GI | deferred to Stage 7 | Z2-D-043 (ADVGFX) |
| MSAA | antialiasing is TAA or the temporal upscaler in every path | Z2-D-035 |
| tile-local merging of lighting passes | not before the ruling | Z2-D-036, D-TBDR (open, Stage 5) |
| split-screen lighting | local multiplayer and split-screen excluded | Z2-D-076 |
| stereo lighting | deferred to Stage 7 with XR; feature signatures stay per view | Z2-D-072 |
| cached, static or virtual shadow maps | every shadow view re-renders each frame | design choice LR1 of `Zenith2/Docs/Design/LIGHT.md → "Design choices this family makes"` |
| IES photometric profiles | lights are photometric through units and cones in v1 | design choice LR5 |
| wide-gamut (beyond Rec.709) content | HDR extends luminance only | design choice LR8 |
| the lighting bake, probe placement and the lightmap tracer | world authoring | WORLD (Z2-I-WORLD-06) |
| atmosphere, fog, cloud, weather, water and time-of-day authoring, the sun and moon lights, the star cubemap and the submersion test | environment authoring; LIGHT owns the passes and the hooks, and consumes ENV's per-view records | ENV (Z2-I-ENV-06 to Z2-I-ENV-09) |
| the path tracer | validation tool | Z2-D-037 (ADVGFX) |
| views, culling, the tier mechanism and frame anatomy | rendering production | RPROD |
