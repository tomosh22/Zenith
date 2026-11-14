# LIGHT design: test strategy and catalogue

Section file of `Zenith2/Docs/Design/LIGHT.md`. It lists every LIGHT test by ID (`Test_<Type>_<Behaviour>`, `Zenith2/Docs/Contracts.md → "Tests and code hygiene gates"`), the case each asserts, its file and the gate step that runs it, plus the parity scenes, the analytical BRDF tests, the path-tracer comparison scenes, the approximation error scenes and the device runs. Rendering's own test tables (views, culling, binding, graph) are cited, not repeated. Every test a requirement of `Zenith2/Docs/Requirements/LIGHT.md` names as evidence is in one of the tables below, or in "Tests of other areas the evidence names".

## Scope of this file

| Topic | Heading |
|---|---|
| where tests live and who runs them | "Test targets and gate steps" |
| CPU unit tests | "Light model unit tests", "Shadow unit tests", "Temporal unit tests", "Exposure and post unit tests", "Colour unit tests", "GI and probe unit tests", "Configuration and component unit tests", "Format and fuzz tests", "Cook and bake tests" |
| GPU render tests | "Render tests", "ENV hook render tests" |
| the three validation roles | "Parity scenes", "Analytical BRDF tests", "Path-tracer comparison scenes", "Approximation error scenes" |
| temporal behaviour | "Temporal sequence tests" |
| counters, debug views and dumps | "Diagnostics tests" |
| robustness | "Failure and stress tests" |
| hardware | "Device runs" |

## Test targets and gate steps

| Target | Path | Contents | Gate step |
|---|---|---|---|
| `ZenithFlux_Tests` | `Zenith2/Engine/ZenithFlux/Tests/Lighting/`, `Tests/Shadows/`, `Tests/Temporal/`, `Tests/Post/`, `Tests/Colour/`, `Tests/GI/`, `Tests/Probes/`, `Tests/Config/` | CPU unit tests (no device) and render tests (WARP and lavapipe, [Z2-N-REND-034]) | `Test` (every ticket), with sync validation clean ([Z2-N-REND-382]) |
| `ZenithFlux_Tests` (reference BRDF) | `Zenith2/Engine/ZenithFlux/Tests/ReferenceBRDF/` | the independent CPU BRDF, sharing no code with `Zenith2/Shaders/Lighting/` ([Z2-N-REND-949]) | `Test` |
| `ZenithFlux_Tests` with `TEST_DEPENDS ZenithPathTracer` | `Zenith2/Engine/ZenithFlux/Tests/PhysicalAccuracy/` | path-tracer comparisons ([Z2-N-REND-948]) | `Test` smoke at capped resolution; the full convergence run in the Stage 5 acceptance suite |
| `ZenithRuntime_Tests` | `Zenith2/Engine/ZenithRuntime/Tests/Lighting/` | the component registration tests | `Test` |
| parity scenes | `Zenith2/Engine/ZenithFlux/Tests/Parity/Scenes/Lighting*.scene.json` with entries in `Tests/Parity/Tolerances.json` ([Z2-N-REND-942]) | captures compared across backends and paths | `BackendParity` (from Stage 3), `PathParity` (from Stage 5) |
| cook tests | `Zenith2/Tools/ZenithCook/Tests/Lighting/` | LUT, environment, probe, volume and lightmap writers and readers | `Test` |
| fuzz | `Zenith2/Engine/ZenithFlux/Tests/Fuzz/`, `Zenith2/Tools/ZenithCook/Tests/Fuzz/` | the readers | `Sanitizers -Preset win-asan` (libFuzzer smoke), stage acceptance (long runs) |
| shader lints | the lighting and pass programs | `ShaderChecks` (`MaterialExplicitGradients` for material modules, `DiscardOnlyInAlphaTest`, `NoMin16Float`, `HalfRatioReport`) | `ShaderChecks` |

### Tests of other areas the evidence names

| Test | Owner | Used for |
|---|---|---|
| `Test_Paths_DisabledFeatureAddsNoPass`, `Test_View_CameraCutResetsHistory`, `Test_View_DepthModePrecisionCheck`, `Test_View_EditorHistoryIndependentOfMain`, `Test_View_OutputExtentChangeResetsHistory` (today `Test_View_ExtentChangeResetsHistory`, renamed by X104), `Test_Memory_CriticalShedding`, `Test_Swapchain_*` | Rendering and RPROD (`Zenith2/Docs/Design/RPROD/Tests.md`) | extended with LIGHT's features, histories and output modes |
| `Test_Thermal_ResolutionNeverBelowFloor`, `Test_Thermal_TierChangeTouchesNoSimulation` | PLAT | the thermal coupling |
| `Test_Clouds_ZeroCoverageAddsNothing`, `Test_Atmosphere_SunMoonSlots` (ENV's slot form under X125; it asserts the slot flag contract of `Zenith2/Docs/Design/LIGHT/Lighting.md → "Sun and moon slots"` once ENV conforms, row L39) | ENV | the cloud and sun hooks |
| `Test_UploadRing_AllocateBeforeExtendAsserts` | RHI (`Zenith2/Docs/Design/RHI/Tests.md`) | the build-node upload order LIGHT's producers follow |
| `Test_ShaderCompiler_SharedLocationRejected`, `Test_ShaderCompiler_SharedTypesLayoutMatchesReflection` | Rendering (`Zenith2/Tools/ZenithShaderCompiler/Tests/`, [Z2-N-REND-137]) | the `Common/` placement and layouts of LIGHT's `[FluxShared]` structs |

## Light model unit tests

| Test | Asserts |
|---|---|
| `Test_LightUnits_LumensToCandela` | point `Φ/4π` and spot `Φ/(2π(1 − cos θ))` at θ = 10°, 45°, 89° against `double` references within 1e-6 relative; the 1e-4 angle clamp |
| `Test_LightUnits_LumensToNitsArea` | 1,000 lm through `Flux_LumensToNitsRect` (2 × 0.5 m, one-sided `1,000 / (π × 1)` = 318.3099 nits, two-sided half of it) and `Flux_LumensToNitsDisc` (r = 0.3 m, one-sided `1,000 / (π × π × 0.09)` = 1,125.7909 nits, two-sided half) against `double` references within 1e-6 relative; a width, height or radius of 0 is clamped to 1e-4 m and gives the `double` value at 1e-4 m within 1e-6 relative |
| `Test_LightUnits_KelvinToRec709` | `Flux_KelvinToLinearRec709` at 1,667, 2,222, 4,000, 6,500, 10,000 and 25,000 K equals a `double` evaluation of the same Kang et al. 2002 fit and XYZ → Rec.709 matrix (`Zenith2/Docs/Design/LIGHT/Lighting.md → "Light units"`) within 1e-3 per component: 6,500 K gives (1, 0.9436, 0.9928), not white, 1,667 K gives (1, 0.1758, 0) with the negative blue clamped, 25,000 K gives (0.3774, 0.5124, 1); 1,000 K returns the 1,667 K value and 30,000 K the 25,000 K value bit for bit (clamping); the ratio B / R strictly increases over 64 evenly spaced temperatures from 2,000 to 25,000 K (monotonic warmth); every component is in [0, 1] and the maximum is 1 |
| `Test_LightRecord_EncodeEveryType` | every field of `Flux_EncodeLightRecord` for each Stage 3 `Flux_LightType` (directional, point, spot) at its offset; a type value the encoder does not know is refused by validation before encoding |
| `Test_LightRecord_EncodeAreaTypes` | (Stage 5) every field of types 3 (rect) and 4 (disc) at its offset, including the rect tangent octahedral packing round trip within 1/32767 and the decoded tangent within 1e-3 of the re-orthogonalised input |
| `Test_LightKey_EntityAndNamedDisjoint` | 10,000 random entity IDs and world tags give keys with bit 63 clear; `Flux_MakeNamedLightKey` of `"Zenith.Environment.Sun"` and `"…Moon"` for world tags 0 … 15 has bit 63 set; no entity key equals a named key |
| `Test_LightKey_NamedMatchesHash` | `Flux_MakeNamedLightKey(s, t).m_ulValue == (Zenith_Hash64(s, t) | 1ull << 63)` for 8 names and 4 tags; `Flux_MakeEntityLightKey(id, t)` equals `Zenith_Hash64` of the 16 ID bytes with seed t, bit 63 cleared |
| `Test_LightDelta_Layout` | `sizeof(Flux_RenderLightDelta) == 112`, `alignof == 8`, and `offsetof` of each of the 22 members equals the offset table of `Zenith2/Docs/Design/LIGHT/Lighting.md → "The delta record"` |
| `Test_LightDelta_ApplyAddUpdateRemove` | the mirror and staged records after ADD, UPDATE, REMOVE; unknown-key UPDATE becomes ADD; unknown REMOVE ignored; the counters |
| `Test_LightDelta_InvalidRefused` | the pure `Flux_ValidateLightDelta`: each refusal rule of "Validation" refuses the delta alone, and each flag-clearing rule clears exactly its flag (including each of the reserved bits 5, 6 and 7 and one of 24–31 set on a delta of `m_axLightDeltas`) and returns the delta otherwise unchanged |
| `Test_LightMirror_InvalidDeltaKeepsEntry` | the mirror's half: a refused delta keeps the key's previous mirror entry (or no entry) and counts `Flux.Lights.Invalid` once; each cleared flag counts `Flux.Lights.FlagIgnored` |
| `Test_LightDelta_ValidationSharedVerdict` | a sequence of 64 deltas (valid, refused, flag-cleared) applied to the real `Flux_GPUScene` on WARP and lavapipe and to the mirror: every key the GPU scene refused is absent or unchanged in the mirror, every accepted key is present in both with the same slot, and the staged record equals `Flux_EncodeLightRecord` of the accepted delta |
| `Test_LightDelta_TableFull` | a full light table drops the light with `FLUX_GPU_SCENE_ERROR_TABLE_FULL`, the mirror marks it `m_bDropped` and `Flux.Lights.Dropped` counts it |
| `Test_Lighting_SunMoonSlotsReserved` | (Stage 3, Q45) after `Create`, slots 0 and 1 are never handed to a delta ADD (4,096 ADDs allocate slots ≥ 2); an environment with `m_bHasSun` stages slot 0; one with `m_bHasSun` false stages the inactive record once; an invalid slot (type point) is inactive and counted |
| `Test_Lighting_SunMoonSlotFlags` | (Stage 3, Q45) a sun slot with casts-shadow, contact and volumetric and a moon slot with volumetric and casts-shadow (the slot flag contract) are active and encode exactly those bits in `m_uFlags`; a sun slot with reserved bit 5, 6 or 7 set is accepted by `Flux_ValidateEnvironmentLight` with the bit cleared, stays active and counts `Flux.Lights.FlagIgnored` once per bit; a moon slot carrying contact shadows is active with the bit cleared and counted; a sun slot with none of the three bits is active (not refused), unshadowed and injects no fog; a moon slot without casts-shadow is active and unshadowed; the five enumerators of `Flux_LightFlag` OR to `0x1F`, so no role flag exists |
| `Test_Lighting_FullStateKeepsReservedSlots` | (Stage 3, Q45) a full-state render world with the sun, the moon and 100 lights: the real `Flux_GPUScene` re-allocates the 100 lights into slots ≥ 2, slots 0 and 1 hold the restaged sun and moon records, and the mirror holds 102 entries |
| `Test_ProbeKey_MatchesEntityDerivation` | (Stage 4) `Flux_MakeProbeKey(id, t).m_ulValue` equals `Flux_MakeEntityLightKey(id, t).m_ulValue` for 1,000 random IDs and tags 0 … 15, and bit 63 is clear |
| `Test_LightList_CPUReferenceMatchesGPU` | the CPU reference of the list build equals the GPU buffer byte for byte on a 4,096-light fixture (render test on WARP and lavapipe) |
| `Test_LightList_DirectionalOverflow` | three scene directional lights and no slots keep the two brightest, tie by index; the role bits give entry 0 the sun role with the atmosphere enabled and no role with it disabled |
| `Test_LightList_SunMoonSlotsFirst` | an active sun slot, an active moon slot and three brighter scene directional lights: words 2–3 are slots 0 and 1, word 1 bits 8 and 9 are set, the three scene lights count `Flux.Lights.DirectionalDropped`; with the moon slot inactive, entry 1 is the brightest scene light with bit 9 clear |
| `Test_Clusters_SliceMapping` | `Flux_ClusterIndex` against the analytic slice boundaries at every boundary ± 1e-4 relative, beyond far, at near; for both capability tiers and render extents 1920 × 1080 and 1333 × 750, a `Flux_ClusterMapping` built from the `m_xClusterGrid`, `m_xClusterTileSize` and `m_xClusterDepthRange` the CPU fills gives, for 1,024 random pixels and depths, the cluster whose `build`-step AABB contains the point |
| `Test_Clusters_LayoutPerCapabilityTier` | `Flux_GetClusterLayout(DESKTOP)` is (16, 9, 24, 2,000 m, 128, 32, 8) and `(MOBILE)` (8, 5, 16, 1,000 m, 48, 8, 4); the configuration-side check is `Test_LightConfig_NoClusterKeys` |
| `Test_Clusters_ListsDeterministic` | cluster lists byte-identical over 10 runs and across Vulkan and D3D12 (render test) |
| `Test_Clusters_OverflowKeepsLowestIndices` | 200 lights in one cluster keep the first 128 in ascending order and count the overflow |
| `Test_Clusters_SpotConeTest` | the cone–sphere test against a brute-force point-sampled reference over 10,000 random cones and boxes: no false negative |
| `Test_LightAttenuation_WindowedInverseSquare` | `W(d)` is 1 at d = 0, 0 at the range, monotonic; spot `S` at the inner and outer angles |
| `Test_LightingChannels_Mask` | the channel predicate for every pair of 8-bit masks, including 0 |

## Shadow unit tests

| Test | Asserts |
|---|---|
| `Test_CSM_SplitsPracticalScheme` | `Flux_ComputeCascadeSplits` against the formula in `double` for N = 1 … 4, λ = 0, 0.5, 0.8, 1; `c_0 = n`, `c_N = D` exactly |
| `Test_CSM_BoundingSphereContainsSlice` | for N = 4 slices at fov 60° and 90°, aspects 16:9 and 1:1: every corner of the slice satisfies `|corner − centre| ≤ r × (1 + 1e-5)`; minimality: when the centre is not clamped, both corner rings lie on the sphere (`|corner − centre|` within 1e-4 × r of r), and when it is clamped to `z = c_{i+1}` the far ring does; distances in `double` |
| `Test_CSM_TexelSnapping` | a camera translation of 0.3 texels changes the projection's translation by a whole number of texels and nothing else |
| `Test_CSM_StableUnderRotation` | a 360° camera yaw sweep leaves every cascade's radius and texel size unchanged |
| `Test_CSM_LightBasisDegenerate` | a light direction within 0.99 of +Y uses the +X up vector and yields an orthonormal basis |
| `Test_ShadowAtlas_PackingDeterministic` | the same request list gives the same tiles over 100 runs; tiles never overlap; each tile is aligned to its size |
| `Test_ShadowAtlas_AllocateFailsWhenFull` | the allocator alone: a request that does not fit returns `FLUX_LIGHTING_ERROR_ATLAS_FULL` with no tile left allocated, and the same request at half size succeeds while space remains |
| `Test_ShadowAtlas_FinestAndCoarsestLevelsFill` | on the widest layout (8,192 × 8,192, one shadowed directional light with two cascades of T_c = 4,096, local maximum 4,096, minimum 32) exactly 32,768 one-face 32-texel requests succeed, distinct, 32-aligned and inside the bottom 4,096 rows, and the next returns `FLUX_LIGHTING_ERROR_ATLAS_FULL`; after a new `BeginFrame` exactly 2 one-face 4,096-texel requests succeed and the third fails likewise; `GetUsedTexelArea` equals 8,192² in both full states; `ShadowAtlasAllocator.cpp` also holds `static_assert(uNODE_COUNT == Flux_AtlasNodeCount(8192, 4096, 32))` (87,380) |
| `Test_ShadowAtlas_FallbackHalvesThenDrops` | `PlanShadows`' retry loop: a request that cannot fit is retried at half size down to the minimum, then dropped with `m_uShadowIndex = 0xFFFFFFFF`, `FLUX_LIGHTING_ERROR_ATLAS_FULL` from the last attempt and one `Flux.Shadows.Dropped` |
| `Test_ShadowAtlas_PointLightAllOrNothing` | a point light gets six tiles of one size or none |
| `Test_ShadowAtlas_Hysteresis` | a desired size oscillating by less than 2× never changes the tile; one outside for 8 frames does |
| `Test_ShadowAtlas_InvalidSettingsRejected` | each rule of "Atlas layout" refused alone by `BeginFrame`: a width below cascades × T_c; a width below the local maximum; T_c = 512, local maximum 1,024, height 1,536 (below `roundUp(512, 1024) + 1024` = 2,048, the layout with no aligned block); a local maximum below the minimum; each returns `FLUX_LIGHTING_ERROR_INVALID_SETTINGS` and leaves the previous layout in force; every tier column of QualityTiers.md is accepted |
| `Test_ShadowAtlas_LeafHeaderIncludes` | a test translation unit including only `ZenithFlux/Flux_ShadowAtlas.h` uses all six of its types, `Flux_RenderLightKey`, `Flux_AtlasTile`, `Flux_ShadowViewKind`, `Flux_ShadowViewPlan`, `Flux_ShadowAtlasLayout` and `Flux_ShadowAtlasAllocator`, and compiles; the header's include list is exactly `Zenith_Matrix.h`, `Zenith_Result.h`, `Zenith_Span.h`, `Zenith_Types.h`, `Flux_LightingErrors.h`, and `Flux_LightingErrors.h` includes only `Zenith_Types.h` (source-text assertions over both headers), so RPROD's `Flux_View.h` and `Flux_ViewContext.h` include it without a cycle |
| `Test_ShadowBudget_PriorityOrder` | priority descending then key ascending; the local cap; directional lights exempt |
| `Test_PlanShadows_Deterministic` | the same view list, mirror and hysteresis state give byte-identical plans and records over 100 runs; the plans are cascades first, then local faces in allocation order |
| `Test_PlanShadows_StagesChangedRecords` | over a 3-frame sequence (a light added, a light unchanged, a light whose tile is dropped) exactly the added light and the dropped light's records are staged through the real `Flux_GPUScene`, the dropped one with `m_uShadowIndex = 0xFFFFFFFF`, and `Flux.Shadows.Dropped` counts 1 |
| `Test_PlanShadows_InvalidSettingsNoViews` | a quality whose atlas violates "Atlas layout" makes `PlanShadows` return `FLUX_LIGHTING_ERROR_INVALID_SETTINGS` with an empty plan and every light record staged unshadowed |
| `Test_ShadowRecord_Layout` | `sizeof(Flux_ShadowViewRecord) == 96` and the six `offsetof` values of the layout table (0, 64, 80, 84, 88, 92) through the generated C++ struct |
| `Test_ShadowRecord_GPUReadback` | the records written at order 5 read back on both APIs with every field at its offset |
| `Test_ShadowRecord_WorldToShadowMapsTile` | a world point projected by `m_xWorldToShadow` lands inside the tile's UV rectangle for points inside the view frustum |
| `Test_PointShadow_FaceSelection` | the face order and tie rule for the 26 axis-aligned and diagonal directions |

## Temporal unit tests

| Test | Asserts |
|---|---|
| `Test_Jitter_Halton23` | the first 16 points against the Halton (2, 3) definition; offsets in [−0.5, 0.5) |
| `Test_Jitter_PeriodForRatio` | 8 at ratio 1, 8 at 1.0 to 1.15, the FSR rule above, 64 cap |
| `Test_Jitter_OffsetAccessor` | `GetJitterOffsetPixels(view, 1.0)` over 10 submitted frames returns `Flux_JitterSequence(8).GetOffsetPixels(k)` for the committed indices 1 … 10; a deferred frame 5 returns the same offset in frames 5 and 6; a camera cut restarts at index 1; a view with no committed counter returns index 1's offset; ratio 2.0 uses the period `PeriodForRatio(2.0)` |
| `Test_DynRes_StepLimits` | one update never changes the scale by more than 5% |
| `Test_DynRes_FloorAndCap` | 1,000 frames at 3× the target settle at 0.75 exactly; a thermal cap of 0.8 is never exceeded; a cap below the floor is clamped to the floor |
| `Test_DynRes_RangeFromSettings` | with `m_fMinScale` 0.85 and `m_fMaxScale` 0.95, 1,000 frames at 3× the target settle at 0.85 and 1,000 at 0.3× at 0.95; a `m_fMinScale` of 0.5 is raised to the 0.75 floor |
| `Test_DynRes_Hysteresis` | a frame time oscillating between 0.95 and 1.02 × target does not raise the scale; 8 frames below 0.9 × target do |
| `Test_DynRes_Quantised` | every scale is a multiple of 0.01 |
| `Test_DynRes_PressureForcesFloor` | `SetPressureFloor(true)` called for every view with `HasDynamicResolution` (as RPROD's pressure hook calls it; the test makes the calls) gives the floor at the next `UpdateDynamicResolution` and holds it across 10 updates at 0.3× the target; `UpdateDynamicResolution` never clears the flag; the normal steps resume one frame after `SetPressureFloor(false)`; `GetDynamicResolution` on a view without a controller is a `z2-death-assert` case |
| `Test_DynRes_InvalidSamplesIgnored` | NaN and negative frame times change nothing and are counted |
| `Test_DynRes_SampleConsumedOnce` | the same `Flux_ViewGpuTime` (same frame index) passed to 5 consecutive updates changes the smoothed time and the hysteresis counters once; a sample with an older index changes nothing |
| `Test_DynRes_ControllerIsSoleScaleSource` | (the Stage 3 temporal integration, `Zenith2/Engine/ZenithFlux/Tests/Temporal/TemporalSequenceTests.cpp`, over RPROD's `Flux_BuildViewList` step 4) a frame whose view list holds a main view with a controller and a texture view without one: the main view's `m_fDynamicResolutionScale` equals `GetScale()` bit for bit and the texture view's is 1.0; after `SetPressureFloor(true)` the main view carries the floor and the texture view still 1.0 |
| `Test_DynRes_ThermalCapThroughRuntime` | (integration suite `Zenith2/Tests/Lighting/`) PLAT's `Zenith_ThermalMonitor` over a scripted test provider (`THERMAL_STATUS_NONE`, `_MODERATE`, `_SEVERE`, `_CRITICAL`, then `_NONE`) calls `Zenith_ThermalAdaptationTarget::SetResolutionCap`, which calls `SetThermalCap`: after each step the next update never exceeds the cap, `Flux.DynRes.ThermalCapPercent` equals it × 100, and the cap is 1.0 one update after `_NONE` |
| `Test_ThermalCoupling_ResolutionNeverBelowFloor` | (integration suite) 100 hot samples, 200 updates at 3× the target and a requested cap of 0.5 leave the main view's scale at 0.75 (Z2-B-SHIP-011) and never below; PLAT's own `Test_Thermal_ResolutionNeverBelowFloor` stays PLAT's |
| `Test_ThermalCoupling_TierChangeTouchesNoSimulation` | (integration suite) a thermal tier offset of −1 changes the view's `Flux_LightQualitySettings` copy to the lower tier and back, and the `Zenith_ReplayLog` simulation hashes of 300 ticks with and without the sequence are identical; PLAT's own `Test_Thermal_TierChangeTouchesNoSimulation` stays PLAT's |
| `Test_DynRes_Counters` | after 100 updates with 30 at the floor and a thermal cap of 0.9, `Flux.DynRes.ScalePercent` equals the main view's scale × 100, `.FramesAtFloor` is 30 and `.ThermalCapPercent` is 90 |
| `Test_HistoryPolicy_InvalidationEvents` | (Stage 2, the policy table) each event of "History invalidation" invalidates exactly its listed histories; a dynamic-resolution step invalidates none of the output-extent histories |
| `Test_HistoryPolicy_QualityAndScaleEvents` | (Stage 3) each tier-key change invalidates exactly its histories (the upscaler key the TAA history, the froxel keys the fog history, the SSGI key the SSGI history), and a scale step from 1.0 to 0.9 leaves TAA, exposure, fog and SSR previous colour valid while the velocity history declares one resample pass in the next frame |
| `Test_History_PingPongMarkedOutput` | (Stage 2, the `LightingHistoryProbe` of the history resources) the probe's write is declared on the current half with `MarkOutput`, survives graph culling with no reader in the frame, and the next frame's probe reads the other half (graph inspection over 3 frames); with frame 2 deferred, frame 3 writes the same half as frame 2 and reads frame 1's |
| `Test_TAA_HistoryPingPong` | the TAA history write is declared on the current half with `MarkOutput`, survives culling with no reader, and the next frame's resolve reads the other half as `m_xPrevHistory` (3 frames); with frame 2 deferred, frame 3 writes the same half as frame 2 |
| `Test_Fog_HistoryPingPong` | the fog history write is declared on the current half with `MarkOutput` and the next frame's temporal step reads the other half |
| `Test_Exposure_HistoryPingPong` | the exposure buffer write is marked output and read by the next frame's adapt |
| `Test_SSGI_HistoryInvalidation` | a camera cut, an output-extent change and a toggle of `render.quality.ssgiEnabled` each leave the SSGI previous-history field unset in that frame; a dynamic-resolution step resamples it (Stage 5) |

## Exposure and post unit tests

| Test | Asserts |
|---|---|
| `Test_Exposure_EV100FromCamera` | f/16, 1/100 s, ISO 100 gives 14.64 (log2(25,600)) within 1e-5 |
| `Test_Exposure_FromEV100` | `Flux_ExposureFromEV100` at EV −4, 0, 10, 16 equals 13.333333, 0.83333333, 8.1380208e-4, 1.2715658e-5 (`1 / (1.2 × 2^EV)` in `double`) within 1e-6 relative |
| `Test_Exposure_MeterHistogramReference` | `Flux_MeterHistogramEV100` with min −12, max 18, percents 50 and 95, compensation 0, limits −4 and 16, bin centres `−12 + (k + 0.5) × 30 / 255` ("Metering"): all weight in bin 128 gives 6.1176471; equal weight in every bin (kept weight from bin 128 to 243.2, bin 243 contributing 0.2 of its weight, weighted mean of k + 0.5 = 185.6) gives 12.835294; half the weight in bin 50 and half in bin 200 (the low cut removes bin 50, the high cut 5% of bin 200) gives 14.588235; each within 1e-4 EV; an empty histogram returns `fPreviousTarget` exactly |
| `Test_Exposure_AdaptCurve` | 120 steps of `Flux_AdaptEV100` at Δt = 1/60 against the closed form `target + (EV0 − target) e^(−speed t)` within 1e-4; up and down speeds |
| `Test_Exposure_SnapOnInvalidHistory` | the first frame snaps to the target |
| `Test_Exposure_ManualAddsNoPass` | a view in `FLUX_EXPOSURE_MODE_MANUAL_EV` and one in `_PHYSICAL` declare neither the histogram nor the adapt pass, and their `fPreExposure` is `1 / (1.2 × 2^EV100)` of the manual or camera EV100 |
| `Test_Exposure_CaptureFixed` | a capture view with `captureEV100` 6 and a preview view without an EV100 declare no exposure pass and use EV100 6 and 10 |
| `Test_DoF_CircleOfConfusionThinLens` | a 50 mm f/2 lens (A = 25 mm) focused at 2 m, sensor 36 mm, 2,560 px wide, `m_fMaxCoCPixels` 64: `Flux_CircleOfConfusionPixels` is −45.584045 px at 1 m (0.64102564 mm, in front of focus), +22.792023 px at 4 m (0.32051282 mm) and +45.584045 px at 1e9 m (the limit `A f / (z_f − f)`), each within 1e-4 relative; with `m_fMaxCoCPixels` 32 the 1 m value clamps to −32 exactly; artistic mode (`m_fFocalLengthMm` 0, focus range 5 m, max 32) gives +32 at 20 m and −12.8 at 0 m within 1e-5 |
| `Test_MotionBlur_TileMaxReference` | a 256 × 144 velocity field `v(x, y) = ((x mod 17) − 8, (y mod 13) − 6)` px: the GPU tile maximum and neighbour maximum equal, bit for bit, the CPU reference `Zenith2/Engine/ZenithFlux/Tests/Post/MotionBlurReference.cpp` (the largest magnitude, ties to the first in row-major order); both select an input value, so no tolerance applies |
| `Test_MotionBlur_CameraCutNoBlur` | a frame with `m_bCameraCut` after a 90° camera turn: the camera velocity is zero, every tile's neighbour maximum is below 0.5 px and the output equals the input bit for bit |
| `Test_Bloom_SoftThresholdContinuity` | with T = 1 and K = 0.5, the contribution factor at Y = T − K ± 1e-4 and at Y = T + K ± 1e-4 differs across each point by at most 1e-3 (continuity), and is non-decreasing in Y over 10,000 samples in [0, 4] (monotonicity, at most 1e-6 decrease tolerated for rounding); T = 0 returns the input colour within 1e-6 relative |
| `Test_Bloom_EnergyConserving` | for a uniform input of radiance 1 with T = 0, the sum of the chain's mip-0 energy equals the input's within 1% (the composite's half is `Test_Tonemap_BloomCompositeEnergy`) |

## Colour unit tests

| Test | Asserts |
|---|---|
| `Test_Tonemap_ACESFittedReference` | `Flux_TonemapACESFitted` at inputs 0, 0.18, 1, 4, 16, 64 (grey) and three saturated primaries against `double` evaluations of the constants within 1e-5 |
| `Test_Tonemap_ACESFittedMonotonic` | monotonic per channel on grey over [0, 1,000]; output in [0, 1] |
| `Test_Tonemap_LottesProperties` | for peaks 400, 1,000, 4,000 and paper white 203: `f(midIn) = midOut` and `f(hdrMax) = 1` within 1e-9 in `double`; monotonic |
| `Test_Tonemap_GrainDeterministic` | the grain term for 16 pixels at frame indices 0 and 1 against the CPU hash reference within 1e-6; the same frame index gives bit-identical output on both APIs; intensity 0 changes no value |
| `Test_Tonemap_VignetteProfile` | at `m_fVignetteIntensity` 0.5 the factor is 1 at the centre, `1 − 0.5` at the corners and `1 − 0.5 × r^2.2` at r = 0.5 within 1e-5 (aspect 16:9); intensity 0 changes no value |
| `Test_PQ_ReferenceValues` | the ST 2084 values of "Output transforms" within 1e-5; encode/decode round trip within 1e-4 relative over [0.01, 10,000] nits |
| `Test_ColourMatrices_RoundTrip` | 709 → 2020 → 709 and 709 → XYZ → 709 within 1e-5; D65 white maps to Y = 1 |
| `Test_OutputMode_Selection` | every row of "Output mode selection", including the SDR fallback result (the warning is the caller's) |
| `Test_HDRMetadata_Values` | Rec.709 primaries, D65, MaxFALL = min(peak, 400) |
| `Test_GradingLUT_IdentityWithinQuantisation` | the identity LUT applied on the CPU reference path changes no value by more than 1/255 |

## GI and probe unit tests

| Test | Asserts |
|---|---|
| `Test_SH9_ConstantEnvironment` | a constant radiance L cube projects to `c00 = L √(4π)` and other coefficients below 1e-6; irradiance `π L` in every direction |
| `Test_SH9_CosineLobe` | a clamped-cosine environment's irradiance against the analytic Ramamoorthi–Hanrahan result within 1% |
| `Test_SH9_GPUMatchesCPU` | the GPU projection of a cube synthesised in the test from fixed SH coefficients equals `Flux_ProjectCubeToSH9` within 1e-4 relative (render test) |
| `Test_ProbeBlend_WeightsSumToOne` | probe and sky weights sum to 1 for 1,000 random configurations; priority and volume ordering |
| `Test_ProbeBlend_ParallaxBoxProjection` | 1,000 random positions inside a 10 × 4 × 6 m parallax box with random unit reflection vectors: the box-projected direction is within 1e-5 rad of the `double` ray–box intersection reference, and positions within 1e-3 m of a face still return a unit vector |
| `Test_ProbeResidency_SelectionDeterministic` | the resident set for a fixed view list is independent of delta arrival order |
| `Test_IrradianceVolume_TrilinearWithValidity` | a 2 × 2 × 2 grid of constant SH (`c00` = 1) with one invalid probe holding `c00` = 100: at 64 points inside the cell the irradiance equals the constant grid's within 1e-5 relative (the invalid probe gets zero weight); the weights equal a `double` evaluation of "Irradiance volume sampling" (trilinear × validity × back-face) within 1e-5 relative; a cell of eight invalid probes returns "no data" (weight sum below 1e-4) |
| `Test_Lightmap_RGBMRoundTrip` | encode then decode within 1% relative over [1e-3, range] lux |
| `Test_IndirectDiffuse_SourceOrder` | (the irradiance-volume rows) a resident volume replaces the sky source where its weight is 1 and blends at its edge fade, following "Indirect diffuse selection"; on `QUALITY_TIER_S24_DEFAULT` the order is lightmap, volumes, sky and the SSGI source is never selected |
| `Test_IndirectDiffuse_LightmapSourceOrder` | an instance with a resident lightmap page takes the lightmap and skips the volumes; a non-resident page falls through to the volumes |
| `Test_IndirectDiffuse_SSGISourceOrder` | with `render.quality.ssgiEnabled` 1 in a test store, SSGI at confidence 1 replaces the baked source and at 0.25 blends with it; with 0 no SSGI pass is declared |
| `Test_BakedGI_FadeIn` | a streamed-in volume's weight is `k / 30` at frame k after residency, reaching 1 at frame 30, and a streamed-out volume's contribution is 0 in the frame it is removed |

## Configuration and component unit tests

| Test | Asserts |
|---|---|
| `Test_LightConfig_RegisterAllKeys` | `Flux_RegisterLightConfigKeys(xSchema, ePlatform)` called directly on a fresh test schema, once with `BUILD_PLATFORM_WINDOWS` and once with `BUILD_PLATFORM_ANDROID` (RPROD's `Zenith_RegisterFluxConfigKeys` is not called): every key named in "Flux_LightSettings header" exists with its type, default (the `QUALITY_TIER_WIN_DEFAULT` column for Windows, `QUALITY_TIER_S24_DEFAULT` for Android), range and allowed-layer mask of the registration table, none restart-required, none simulation-affecting |
| `Test_LightConfig_NoDuplicateQualityKeys` | every `render.quality.*` key is registered exactly once across `Zenith_RegisterFluxConfigKeys` (RPROD's and LIGHT's entries), and no RPROD member reads a key LIGHT registers |
| `Test_LightConfig_TierFileApplied` | each committed tier fragment `Internal/Config/Quality/<TierID>.config.json` (two at Stage 3, seven from Stage 5; no fixture copy), loaded as layer 4, gives an `m_xLight` equal member by member to its column of `Zenith2/Docs/Design/LIGHT/QualityTiers.md`, and each satisfies the atlas rules |
| `Test_LightingFallbacks_TriggersApplied` | (Stage 5) for every row of `Zenith2/Docs/Design/LIGHT/QualityTiers.md → "Fallbacks and triggers"`, a test state raising exactly that row's trigger makes the settings reader or the view's copy take exactly that row's fallback and change no other member, and clearing the trigger restores the previous values one frame later |
| `Test_LightConfig_InvalidValueKeepsDefault` | an out-of-range `render.quality.shadowCascades` (5), a `render.quality.exposureHighPercent` below the low percent and an atlas violating "Atlas layout" each return `FLUX_LIGHTING_ERROR_INVALID_SETTINGS`, keep that sub-struct's previous values and log `flux.quality` once |
| `Test_LightConfig_ExternalUpscalerRejected` | `render.quality.upscaler = external` is refused with `FLUX_LIGHTING_ERROR_INVALID_SETTINGS` and the upscaler stays `FLUX_UPSCALER_TAAU` |
| `Test_LightConfig_UpscalerValues` | `taa` and `taau` map to 1 and 2; a zero-initialised `Flux_TemporalSettings` value is rejected by validation; `Flux_Upscaler.h` declares the only enumeration whose name contains `Upscaler` in ZenithFlux's public headers (a source-text assertion) |
| `Test_LightConfig_CommandLineLayerDeterminesQuality` | LIGHT's part of a budget run (row L41): a store holding a command-line-layer value (`--set`) for every `render.quality.*` key `Flux_RegisterLightConfigKeys` registered (the key list is read back from the registration, never typed into the test) is read by `Flux_ReadLightQualitySettings` 64 times, each time after the user layer is rewritten with a different random valid value of every one of those keys and of every other registered key; every read returns a byte-identical `Flux_LightQualitySettings` equal to the command-line values, and `GetProvenance` of each key names `CONFIG_LAYER_COMMAND_LINE`. The test fails if any LIGHT quality value is read from a key outside that list, from a lower layer, or from anything but the store |
| `Test_LightConfig_FroxelGridKeys` | `render.quality.froxelGridX`, `…Y` and `…Z` are registered as `CONFIG_VALUE_TYPE_INT` with range 16 … 256 and no `render.quality.froxelGrid` key exists; a tier layer setting X to 300 is refused by the store's range check |
| `Test_LightConfig_FroxelGridRead` | `Flux_ReadLightQualitySettings` over a test store whose schema widens Y's range and holds Y = 8 returns `FLUX_LIGHTING_ERROR_INVALID_SETTINGS` and keeps all three previous components |
| `Test_LightConfig_AtlasSettingsRefused` | through `Flux_ReadLightQualitySettings`, each of the four "Atlas layout" rules violated alone in the store is refused with `FLUX_LIGHTING_ERROR_INVALID_SETTINGS` and keeps the previous shadow settings |
| `Test_LightSettings_ViewOverridesApplied` | an editor view with `Flux_QualityOverrides::m_iShadowQualityDelta = −1` gets `m_xLight.m_xShadows.m_uFilterQuality` one lower in its copy while the main view's copy is unchanged |
| `Test_LightingEnvironment_BlockDefaults` | a world with no ENV resource yields `Flux_AtmosphereBlock::m_bValid` false and the sky and fog passes use the `Flux_AtmosphereParams` defaults with height fog off, no stars and a cloud-shadow factor of 1; a world without `Zenith.Lighting.WorldLighting` uses the engine default environment |
| `Test_LightComponents_Registered` (`ZenithRuntime_Tests`) | (Stage 3) `Zenith_RegisterLightingComponentTypes` registers the five Stage 3 types of "Light component schemas" with their stable names, version 1, every property ID and name of the table, `PHASE_LATE_SCRIPTS` authority and lifecycle hooks on `Zenith.Lighting.WorldLighting` (asset handles); `Freeze` succeeds; the same `TEST_CASE` grows by one `SUBCASE` per later type set with the same assertions: the reflection-probe type (Stage 4), the rect and disc types (Stage 5) and the irradiance-volume and lightmap-set types (Stage 5), so the ten types are covered with no name defined twice |
| `Test_LightComponents_RoundTripByPropertyID` (`ZenithRuntime_Tests`) | each type written through the registry's serializer and read back gives equal values; reordering the members in a test struct registered with the same IDs reads the same values |
| `Test_LightExtraction_ConvertsUnits` | a point light of 800 lm becomes `800 / 4π` cd, a spot of 30° outer angle its cone conversion, 6,500 K the tint (1, 0.9436, 0.9928) within 1e-3 per component (the value `Flux_KelvinToLinearRec709` returns, not white), degrees become radians, the direction is the entity's −Z, and the key is `Flux_MakeEntityLightKey` |

## Format and fuzz tests

| Test | Asserts |
|---|---|
| `Test_EnvironmentFile_RoundTrip`, `Test_IrradianceVolumeFile_RoundTrip`, `Test_LightmapSetFile_RoundTrip`, `Test_GradingLUTFile_RoundTrip` | writer then reader gives identical data; byte-identical output for identical input (determinism) |
| `Test_ProbeFile_RoundTrip` | writer then reader gives identical data; byte-identical output for identical input; the info section is exactly 64 bytes with the offsets of `Zenith2/Docs/Design/LIGHT/Reflections.md → "Probe info record"` |
| `Test_EnvironmentFile_RejectsCorrupt`, `Test_ProbeFile_RejectsCorrupt`, `Test_IrradianceVolumeFile_RejectsCorrupt`, `Test_LightmapSetFile_RejectsCorrupt`, `Test_GradingLUTFile_RejectsCorrupt` | for its format, each case a mutation the test applies to a copy of a valid file its writer produced in memory (no corrupt file is committed): truncation at every section boundary, a wrong magic, a wrong format version (`FLUX_LIGHTING_ERROR_VERSION_MISMATCH`), a section-size mismatch, a non-finite float, a non-zero reserved byte and out-of-limit dimensions each return `FLUX_LIGHTING_ERROR_CORRUPT_DATA` (or the version error) and never assert |
| `Test_CubeReader_Grammar` | the `.cube` grammar rules of "Grading LUT cook" |
| `Test_RGBEReader_Grammar` | the RGBE rules of "Static environment" |
| `Zenith_Fuzz_ProbeFile`, `Zenith_Fuzz_IrradianceVolumeFile`, `Zenith_Fuzz_LightmapSetFile`, `Zenith_Fuzz_EnvironmentFile`, `Zenith_Fuzz_GradingLUTFile`, `Zenith_Fuzz_CubeReader`, `Zenith_Fuzz_RGBEReader` | libFuzzer targets; no crash, no sanitizer report, no allocation above the parser limits |
| `Test_LightingLUT_DFGEnergyBound` | the cooked DFG table satisfies the cook-time `E_tot ≤ 1 + 1e-3` check; a corrupted table fixture is refused |

The non-finite-float case of the `…_RejectsCorrupt` row applies only to formats with a float field: the `.zlut` grading LUT stores UNORM texels and an integer header, so `Test_GradingLUTFile_RejectsCorrupt` has no such case.

## Cook and bake tests

| Test | Asserts |
|---|---|
| `Test_Cook_LightingDeterministic` | (the Stage 4 cooked-formats integration) two cooks of every LIGHT source kind that exists at Stage 4 (`DFG.lutsrc.json`, `DefaultEnvironment.env.json`, an `.hdr` environment, `Identity.gradinglut.json`, a 17³ `.cube`) and of the three bake writers in separate processes give byte-identical outputs and identical derived-data keys, and the cook report's `lighting` member lists the sizes, the LUT energy result and the clamped-value count |
| `Test_Cook_DFGDeterministic`, `Test_Cook_EnvironmentDeterministic`, `Test_Cook_GradingLUTDeterministic`, `Test_Cook_SheenLTCDeterministic` | the per-step forms, one `TEST_CASE` each in its cook step's test file: `DFG.lutsrc.json` (Stage 3); `DefaultEnvironment.env.json` and an `.hdr` from `Cook_TestMakeRGBE` (Stage 3); `Identity.gradinglut.json` and a generated 17³ `.cube` (Stage 3); the `SheenAlbedo` and `LTC` tables over 10 runs (Stage 5) — two cooks in separate processes give byte-identical outputs and identical keys |
| `Test_Bake_LightmapBindingsPacked` | 3 `Cook_LightmapBinding` values written by `Cook_WriteLightmapSet` over 2 pages give 3 × 32-byte `LIGHT_SECTION_LMP_BINDINGS` records with every field at its offset (0, 16, 18, 20, 24, 28), the UV1 scale and bias as round-to-nearest-even f16 (a bias of 0.3 becomes 0.300048828125) and bytes 28–31 zero; a page index past the page count, a non-zero flags value and a scale not finite as an f16 are refused with `COOK_ERROR_LIGHTING_BAKE` naming the binding index |
| `Test_Bake_WritersDeterministic` | `Cook_WriteReflectionProbe`, `Cook_WriteIrradianceVolume` and `Cook_WriteLightmapSet` on fixed inputs produce byte-identical files over 10 runs and across two job-count settings; the lightmap dilation fills every uncovered texel within 8 texels of a chart |
| `Test_Bake_RejectsNonFinite` | a NaN or infinity in any input span of each writer returns `COOK_ERROR_LIGHTING_BAKE` naming the element index, and grid or page limits beyond the format's are refused likewise |

## Render tests

GPU tests on WARP and lavapipe, sync validation clean, aliasing on and off with poisoning ([Z2-N-REND-433]).

| Test | Asserts |
|---|---|
| `Test_Lighting_EvaluationOrderReference` | a single-pixel compute harness evaluating `EvaluateLightingEx` for 64 fixed surface and light configurations matches the CPU reference BRDF within 2 half ULPs of the output |
| `Test_CloudShadow_UnboundMapFactorOne` | (Stage 5) with the cloud-shadow map field unset, `m_xCloudShadowMapping.z` = 0 makes `Flux_SampleCloudShadow` return 1 with no map bound, and the sun's contribution equals the uncapped `Test_Lighting_DirectionalDataPath` value bit for bit |
| `Test_Lighting_DirectionalDataPath` | a harness pass with the "every lighting pass" fields of the X103 table: a light list with the sun slot active (word 1 = 0x101, word 2 = 0) makes `GetDirectionalCount` 1 and `GetDirectional(0).m_uRoles` 1; with `m_xAtmosphere.m_uEnabled` 1 the sun's contribution to a lit plane equals the unattenuated value × `Flux_ComputeAtmosphereTransmittance` at the plane's altitude and the sun's zenith cosine within 1% (LUT sampling); with `m_uEnabled` 0 it is unattenuated within 2 half ULPs; `m_xCloudShadowMapping.z` = 0 leaves the cloud factor 1 with no map bound; the same harness through `Flux_PerObjectLightSource`, whose per-object list also holds the sun (Rendering's selection keeps it), gives the same result bit for bit, so step 4's skip of directional records counts the sun once |
| `Test_LightingSharedTypes_Layout` | (Stage 2, the shading-module stub slice) the generated `Flux_SharedTypes.generated.h` declares `Flux_AtmosphereGPU` (64 B), `Flux_FogMediumGPU` (48 B), `Flux_MediumOverrideGPU` (48 B), `Flux_LocalFogVolumeGPU` (112 B) from `Common/Media.slang` and `Flux_ReflectionProbeRecord` (64 B) from `Common/Probes.slang`, each with every offset of its layout table; no LIGHT module outside `Zenith2/Shaders/Common/` carries `[FluxShared]` (a source-text assertion over `Zenith2/Shaders/Lighting/` and `Zenith2/Shaders/Passes/`) |
| `Test_GIRecords_Layout` | (Stage 5) `Flux_IrradianceVolumeRecord` (64 B) and `Flux_LightmapPageRecord` (16 B) from `Common/GI.slang` with every offset of their layout tables |
| `Test_InstanceLightingRecord_Layout` | (Stage 5, the X106-accepted configuration) `Flux_InstanceLightingRecord` (16 B) from `Common/GPUScene.slang` with every offset; a refusal replaces this case with the material-constants extension's 16-byte layout |
| `Test_LightingStubs_DocumentedValues` | (Stage 2) a stub harness on WARP and lavapipe: `EvaluateLightingEx` through either light source returns `half3(emissive × fPreExposure)` bit for bit with zero weights, `EvaluateLighting` equals its `m_xRadiance`, both sources report zero lights, directional entries and probes, every `Flux_Sample*Shadow` returns 1 and both IBL terms return 0 |
| `Test_LightingPasses_DeclareTableFields` | (Stage 4, the probe filter) the `ProbeFilter` pass declares exactly the fields, access, stages, formats and permutations of "Probe filter pass table" (Reflections.md) on queue G, `m_xDestination` carries neither `[FullCoverage]` nor `[WriteOnly]`, and after a filter into slice 3 every other slice of the environment cube array reads back bit-identical to its contents before the pass |
| `Test_SkyCapture_DeclareTableFields` | (Stage 4) the `SkyCapture` and `SHProject` passes declare exactly the fields of "Sky capture pass table" on queue G; `m_xCaptureFace` carries neither `[FullCoverage]` nor `[WriteOnly]` and after capturing face 2 every other capture layer reads back bit-identical |
| `Test_Probes_FramePassesDeclareTableFields` | (Stage 4 integration) graph inspection of one frame with a probe capture and a sky capture: the three probe passes declare their tables' fields exactly as above inside the real frame |
| `Test_SSGI_DeclareTableFields` | (Stage 5) the `SSGITrace` and `SSGIDenoise` passes declare exactly the fields, access, stages, formats and permutations of "SSGI pass table" (GI.md) on queue G |
| `Test_Lighting_PathsAgreeOnSurfaceData` | the surface-data channels of the lighting parity scene agree between Forward+, Forward, Deferred and the visibility buffer within the near-exact thresholds (Stage 5) |
| `Test_Shadows_ContactEdgeLeakBound` | the plane-on-plane scene leaks at most 2 texels at the contact edge |
| `Test_Shadows_NoAcneOnSlopes` | a plane swept from 0° to 85° to the sun shows no self-shadowed texel at filter quality 1 |
| `Test_Shadows_CascadeBlendContinuity` | the shadow term is continuous (at most 1/255 step) across each cascade boundary |
| `Test_Shadows_AtlasTileIsolation` | a filter kernel at a tile edge never reads the neighbouring tile (two adjacent tiles, one all-occluded) |
| `Test_Shadows_DroppedLightUnshadowed` | an overfull atlas leaves the lowest-priority light unshadowed with `m_uShadowIndex = 0xFFFFFFFF` |
| `Test_ContactShadows_SingleEntry` | a frame without environment slots with two scene directional lights both carrying contact shadows and a thin occluder: only entry 0 (the first flagged entry) is multiplied by the mask, entry 1 takes 1, and the pass traced along entry 0's direction; with entry 0's bit cleared entry 1 becomes c; a frame whose moon slot carries the bit has it cleared and counted, and the moon is never masked |
| `Test_ContactShadows_ThinOccluder` | a 2 cm post casts a contact shadow whose mask at the step of first occlusion s is `s / steps` within 1/255; a floating occluder above the thickness does not |
| `Test_GTAO_AnalyticConfigurations` | a flat plane gives AO 1; the inside of a 90° corner at the edge gives the analytic GTAO value within 0.03 |
| `Test_Fog_DirectionalVolumetricGating` | three frames of each `Flux_BuildFogTestView` variant (in code; not the `LightingFog` parity scene): a sun-role entry without `FLUX_LIGHT_FLAG_VOLUMETRIC` injects zero in-scatter; a role-less scene directional light (atmosphere disabled) with the bit injects `E × visibility × phase_mix` with `T_atm = 1` and no cloud shadow, matching a CPU evaluation of that term within 1%; a moon-role entry with the bit injects with its transmittance and no cloud shadow |
| `Test_Fog_SliceDistribution` | the injection's slice depths equal `Flux_FroxelSliceDepth` for every slice of the 80 × 45 × 32 and 160 × 90 × 64 grids within 1e-5 relative, and `Flux_FroxelSliceCoordinate` inverts it within 1e-4 |
| `Test_Fog_HomogeneousBeerLambert` | a homogeneous medium of σ_t = 0.01 gives the transmittance `exp(−σ_t d)` at 10, 100, 1,000 m within 1% |
| `Test_Fog_LocalVolumeDensity` | a box and an ellipsoid volume of density 0.05 without fade in an empty world give, along a ray through their centres, an optical depth equal to density × chord length within 2% at the 128 × 72 × 48 grid |
| `Test_Fog_SkyNoDoubleCount` | with fog density 0, sky pixels equal the sky-view LUT with the atmosphere from F plus the froxel atmosphere to F, matching `Flux_ComputeSkyRadianceReference` (one integration from the camera to the top) within 2% at 16 directions |
| `Test_Sky_TransmittanceLUTReference` | the transmittance LUT against `Flux_ComputeAtmosphereTransmittance` in `double` at 32 sample points within 0.5% |
| `Test_Sky_MultiScatterReference` | the 32 × 32 multiple-scattering LUT against `Flux_ComputeMultiScatterReference` (64 directions, 20 steps, `double`) at 16 sample points within 1% |
| `Test_Sky_LUTRebuildOnRevisionOnly` | over 10 frames with the sky revision changing at frame 5 only, the transmittance and MS steps are declared in frames 0 and 5 alone (`Flux.Sky.LUTRebuilds` = 2) and the persistent copies are read in the others; a fog-only revision change also rebuilds; a device-loss recovery forces a rebuild; with frame 5 deferred through the real `Flux_UploadRing` (a declared need above its reservation), the steps are declared in frames 5 and 6 and counted once (in frame 6) |
| `Test_Sky_CompositeOrder` | with a star cube of radiance 1, a constant cloud layer (radiance 0.2, transmittance 0.5) and a fog volume of known last slice, a sky pixel equals `L_fog + T_fog × (0.2 + 0.5 × (stars + skyView + sunDisc))` within 1/255 after the SDR tonemap |
| `Test_SSR_MirrorPlaneHit` | a mirror floor below a coloured cube reflects the cube at the geometric reflection position within 1 pixel |
| `Test_SSR_MissFallsBackToProbe` | an off-screen reflection composites the probe specular unchanged |
| `Test_SSR_CompositeRemovesEnvironment` | with SSR confidence 1 the environment specular contribution is fully replaced: the scene rendered with a black probe and with a white probe composites to scene colours that differ by at most one R11G11B10F quantum per channel (2⁻⁶ relative for R and G, whose mantissas have 6 bits, 2⁻⁵ for B) at every SSR-hit pixel |
| `Test_SSR_SpecularWeightOnlyWithSSR` | on `QUALITY_TIER_S24_DEFAULT` and `QUALITY_TIER_WIN_LOW` the forward opaque pass declares one colour target and no `m_xSpecularWeight` resource exists in the graph; on `QUALITY_TIER_WIN_DEFAULT` it declares two, the second R11G11B10F `CLEAR`/`STORE` |
| `Test_ProbeFilter_GPUMatchesCook` | the GPU prefilter of a 16²-per-face cube synthesised in the test from a fixed band-2 SH radiance equals the CPU cook within 2 half ULPs per channel |
| `Test_Probe_CaptureConstantEnvironment` | capturing a constant-radiance environment of 1 nit at a probe pre-exposure of 1 gives, in every face and mip, a value within one R11G11B10F quantum of 1 per channel (2⁻⁶ relative for R and G, 2⁻⁵ for B) |
| `Test_SkyCapture_UpdatesWithinSevenFrames` | a sun direction change of 10° at frame 0 changes slice 0 and the sky SH by frame 7 and not before frame 6; with no change no capture pass is declared (`Flux.Sky.Captures` unchanged) |
| `Test_Lightmap_BindingPerInstance` | two instances of one mesh bound to different lightmap rectangles of one page sample different texels (their decoded irradiances differ by the fixture's 2× ratio within 1%) under the one binding the X106 ruling selects (only that binding is built and tested; the other binding's text is replaced when the ruling is applied) |
| `Test_AsyncCompute_LightingIdentical` | the lighting parity scene with `m_bAsyncCompute` on and off is bit-identical ([Z2-N-REND-791]) |
| `Test_Exposure_HistogramDeterministic` | the histogram buffer is identical over 10 runs and across backends |
| `Test_Exposure_PreExposureConsistency` | a fixed HDR test image (a 64-step luminance ramp over 16 EV and a saturated primary strip, built in the test) carried through the pre-exposed chain with the pre-exposure forced to two values 4 EV apart tonemaps to images within 1/255 per channel of each other; the `LightingPost` integration frame asserts the same property inside a whole frame |
| `Test_Tonemap_OutputModesMatchCPU` | (Stage 3, SDR) a 64-step grey and primary ramp through the SDR output read back from the view output equals the CPU reference within 1 code at 8 bits |
| `Test_Tonemap_HDRModesMatchCPU` | (Stage 5) the same ramp through each HDR mode equals `Flux_TonemapHDR` then the encode (HDR10 10-bit within 1 code, scRGB within 1e-3) |
| `Test_DeferredLighting_HarnessMatchesEvaluateLighting` | (Stage 5, no path frame) a harness graph writes synthetic G-buffer transients GB0–GB3 and depth from 64 fixed surfaces plus 4 far-depth pixels, then `Flux_AddDeferredLighting` lights them: every lit pixel equals `LightingHarness.slang`'s `EvaluateLightingEx` of the same surface within 2 half ULPs per channel and far-depth pixels are exactly zero |
| `Test_DeferredLighting_SampledReadsOnly` | (Stage 5, the same harness graph) the pass declares GB0–GB3 and depth as sampled reads with no tile-local input, and a harness pass declaring a tile-local read of GB1 is refused with `FLUX_RG_ERROR_TILE_LOCAL_UNSUPPORTED` |
| `Test_DeferredLighting_MatchesForwardPlus` | (Stage 5 path-parity integration) the Deferred path's final image of `LightingBasic` within the matrix tolerance of Forward+ |
| `Test_DeferredLighting_NoTileLocalRead` | (Stage 5 path-parity integration) graph inspection of a real Deferred frame and a real visibility-buffer frame of `LightingBasic`: `DeferredLighting` declares every G-buffer attachment as a sampled read with no tile-local input |

## ENV hook render tests

| Test | Asserts |
|---|---|
| `Test_Sky_StarFieldBehindAtmosphere` | the 8²-per-face star cube the test builds in memory (one lit texel per face) at rotation identity and at 90° about the celestial axis appears at the rotated direction; its sky contribution equals `T_view(ω) × L × intensity` within 1% at zenith, is 0 below the horizon, and is absent (bit-identical sky) with a null cube |
| `Test_Sky_CloudLayerComposite` | the constant cloud fixture composites to `0.2 + 0.5 × sky` within 1/255; with the targets unset the sky is bit-identical to the cloudless sky; an editor view at a pre-exposure 2 EV different from the main view rescales the cloud radiance by the ratio |
| `Test_Lighting_CloudShadowOnSunOnly` | with a cloud-shadow map of transmittance 0.25 inside the square, the sun's contribution to a lit plane is 0.25 of its unshadowed value within 1/255, the moon's and a point light's are unchanged, and a point outside the square takes `m_fOutsideTransmittance` beyond the 128 m blend band (Stage 5) |
| `Test_Fog_UnderwaterOverride` | a render world carrying one ENV `Flux_RenderEnvironmentView` record for the main view (`m_xMedium.m_ulViewID` the value of `Flux_MakeViewID(VIEW_KIND_MAIN, world tag, 0)`, `m_xMedium.m_xScattering = (0.01, 0.02, 0.03)`, `m_xMedium.m_xAbsorption = (0.3, 0.05, 0.02)`, `m_ulWaterBodyKey` and `m_bWaterline` set to values LIGHT must ignore): `ApplyRenderWorld` publishes one `Flux_ViewMediumOverride` with that view's ID, the main view's fog gives the per-channel transmittance `exp(−σ_t d)` at 5, 20, 50 m within 2% per channel through `m_xMediumExtinctionRatio`, and an editor view of the same world (no record) uses the height fog; `Flux_AtmosphereBlock` has no override member; two worlds carrying 10 records each publish 16 overrides (the first world's 10, then the second's first 6 in written order) and count `Flux.Fog.OverridesDropped` 4 times; a record with a negative absorption is refused and counted in `Flux.Lights.Invalid` |

## Parity scenes

Scenes under `Zenith2/Engine/ZenithFlux/Tests/Parity/Scenes/`, fixed camera path, jitter, noise seeds and exposure ([Z2-N-REND-942]); tolerances are `Z2-B-RPROD-031` (pending) with LIGHT's proposals below.

| Scene | Exercises | Backend parity (proposed) | Path parity (proposed) |
|---|---|---|---|
| `LightingBasic` | directional + 16 point + 16 spot lights, CSM, local shadows, GTAO, contact shadows | FLIP ≤ 0.01 | FLIP ≤ 0.02 (Forward: 8 per-object slots, one held by the kept directional light, so at most 7 local lights per object) |
| `LightingIBL` | static environment, IBL on a roughness × metallic sphere grid | FLIP ≤ 0.01 | FLIP ≤ 0.02 |
| `LightingFog` | height fog, volumetric sun and spot, sky | FLIP ≤ 0.015 | FLIP ≤ 0.02 |
| `LightingSSR` | mirror and rough floors, SSR on | FLIP ≤ 0.02 | per-feature tolerance FLIP ≤ 0.04 (G-buffer quantisation of normals) |
| `LightingTemporal` | the TAA sequence scene of "Temporal sequence tests" | FLIP ≤ 0.02 after 32 frames | FLIP ≤ 0.03 |
| `LightingPost` | exposure, bloom, tonemap, grading LUT (DoF and motion blur on from Stage 5) | FLIP ≤ 0.01 | FLIP ≤ 0.02 |
| `LightingProbes` (Stage 4) | three blended box probes, parallax | FLIP ≤ 0.015 | FLIP ≤ 0.02 |
| `LightingBakedGI` (Stage 5) | an irradiance volume and a lightmapped room | FLIP ≤ 0.015 | per-feature tolerance FLIP ≤ 0.03 (lightmap on the deferred paths) |

Each scene also compares its surface-data channels near-exactly first ([Z2-N-REND-940]).

## Analytical BRDF tests

Rendering's tests (`Test_BRDF_WhiteFurnace`, `Test_BRDF_Reciprocity`, `Test_BRDF_LambertClosedForm`, `Test_BRDF_GGXNormalisation`, `Test_BRDF_ShaderMatchesReference`, [Z2-N-REND-950]) run against LIGHT's BRDF of `Zenith2/Docs/Design/LIGHT/Lighting.md → "BRDF"`, each closed-form case through the reduction of that file's "Closed-form reductions", with LIGHT's added cases:

| Test | Asserts |
|---|---|
| `Test_BRDF_WhiteFurnace` | directional albedo ≤ 1 + 1e-3 over roughness {0.045 (α = 0.002), 0.05, 0.25, 0.5, 0.75, 1} × F0 {0, 0.04, 0.5, 1} × 16 view angles, and ≥ 0.99 for a white dielectric (equality up to quantisation); every evaluated value is finite and non-negative, including F0 = 1 with albedo 1 (the white metal, where the unguarded diffuse term is 0/0) |
| `Test_BRDF_Reciprocity` | `|f(l, v) − f(v, l)| ≤ 1e-5` at 1,000 random pairs over roughness [0.045, 1] and F0 [0, 1] including F0 = 1 and α = 0.002; every value finite |
| `Test_BRDF_LambertClosedForm` | with `specular = 0` and `metallic = 0` (F0 = 0, hence F90 = 0, `F_avg = 0`, `F_ms = 0`, `E_tot ≡ 0`), the full BRDF equals `ρ / π` within 1e-6 absolute for ρ ∈ {0.18, 0.5, 1}, roughness ∈ {0.05, 0.5, 1}, at the 16 × 16 fixed (μl, μv) points and 3 azimuths, in the CPU reference and in `BRDFHarness.slang` |
| `Test_BRDF_GGXNormalisation` | `∫ D(h)(n·h) dω_h = 1 ± 1e-3` at α ∈ {0.002, 0.05, 0.25, 0.5, 1} by 2²⁰-sample stratified integration in `double` |
| `Test_BRDF_ShaderMatchesReference` | `BRDFHarness.slang` against the CPU reference at 4,096 fixed configurations within 2 half ULPs |
| `Test_BRDF_MultiScatterMetalEnergy` | a white metal (F0 = 1, albedo 1, metallic 1) conserves energy within 1e-3 at roughness {0.045, 0.1, 0.25, 0.5, 0.75, 1}, its diffuse term is exactly 0, and no evaluation returns NaN or infinity (the guards of "BRDF") |
| `Test_BRDF_IBLFurnace` | the split-sum IBL of a uniform environment of radiance 1 on a white dielectric returns 1 within 2e-3 |
| `Test_BRDF_AdvancedLobesFurnace` (Stage 5) | clear coat, anisotropy, sheen and subsurface each pass the furnace and reciprocity bounds (subsurface: albedo ≤ 1 + 1e-3 including the transmission) |
| `Test_BRDF_DFGTableMatchesReference` | the cooked DFG table equals the reference integration within 1e-3 absolute |

## Path-tracer comparison scenes

Stage 5, the physical-accuracy role ([Z2-N-REND-944] to [Z2-N-REND-946]): the path tracer of `ZenithPathTracer` renders each scene to its convergence criterion (`Z2-B-ADVGFX-006`, `Z2-B-ADVGFX-007`) under the caps `Z2-B-ADVGFX-004`, `Z2-B-ADVGFX-005`, with every screen-space approximation (SSAO, SSR, contact shadows, SSGI, fog) disabled in the raster path; the tolerance is `Z2-B-ADVGFX-008`.

| Scene | Content | Raster features compared | Proposed tolerance (FLIP mean, relative luminance error) |
|---|---|---|---|
| `PT_AnalyticPoint` | a diffuse and a glossy sphere on a plane, one point light, no shadows | punctual BRDF, attenuation; the diffuse sphere is the Lambert reduction | 0.02, 2% |
| `PT_AnalyticSpotDirectional` | the same with a spot and a directional light | cone, directional | 0.02, 2% |
| `PT_SphereLight` | a 0.5 m sphere light near a glossy plane | sphere-light diffuse form factor and specular widening | 0.04, 5% |
| `PT_RectLight` (Stage 5) | a 1 × 0.5 m rect light over rough and smooth planes | LTC rect diffuse and specular | 0.04, 5% |
| `PT_DiscLight` (Stage 5) | a disc light | the 8-gon approximation | 0.04, 5% |
| `PT_IBLFurnace` | a white and a grey sphere inside a uniform white environment | IBL diffuse and specular energy | 0.01, 1% |
| `PT_IBLEnvironment` | the sphere grid under the static environment | IBL split sum and SH irradiance | 0.04, 5% |
| `PT_AreaLightShadow` | a rect light and a box occluder inside the 120° cone | area-light shadow penumbra (quality 2) | 0.06, 8% on the penumbra region |
| `PT_TerrainVT` | VT-backed terrain materials under the sun (ADVGFX's scene; LIGHT supplies the lighting) | the sun on terrain | 0.03, 3% |

Each raster image uses the parity scene conventions and is compared after the same tonemap; the report records the tracer's spp, time and convergence state, and a "not converged" run fails rather than passes ([Z2-N-REND-946]). `PT_Analytic*` in the catalogue's evidence names `PT_AnalyticPoint` and `PT_AnalyticSpotDirectional`.

## Approximation error scenes

Screen-space techniques are validated against their documented budgets ([Z2-N-REND-947]) on scenes whose reference is the path tracer with the same content:

| Scene | Technique | Budget |
|---|---|---|
| `AE_GTAO` | GTAO against the path-traced ambient visibility (white sky, no direct light) | mean absolute AO error ≤ 0.08 over non-sky pixels |
| `AE_ContactShadows` | contact shadows against path-traced sun visibility for thin occluders | ≤ 10% of pixels in the contact region misclassified |
| `AE_SSR` | SSR against path-traced mirror and glossy reflections of on-screen objects | FLIP ≤ 0.08 over pixels whose reflected object is on screen |
| `AE_SSGI` (Stage 5, ruling Q41 (a)) | SSGI | the four criteria of `Zenith2/Docs/Design/LIGHT/GI.md → "SSGI error budget"` |

## Temporal sequence tests

| Test | Asserts |
|---|---|
| `Test_TAA_StaticConvergence` | a static scene with the jitter sequence converges after 32 frames to within FLIP 0.02 of a 16× supersampled reference |
| `Test_TAA_MovingObjectNoGhosting` | a bright sphere moving 8 px/frame across a dark background leaves less than 2% of its luminance behind after 4 frames |
| `Test_TAA_DisocclusionRejectsHistory` | pixels revealed by a moving occluder take the current frame (history weight 0) that frame |
| `Test_TAA_CameraCutResetsHistory` | the frame after a cut equals the no-history frame bit for bit |
| `Test_TAA_TeleportCameraCutInvalidatesHistory` | a `Zenith_RecordTeleport` of the camera's owner entity: the extracted view request has `m_bCameraCut`, the TAA, fog and exposure histories are invalid that frame, and the output equals the no-history frame |
| `Test_TAA_TeleportedInstanceNoSmear` | a teleported instance has zero velocity that frame, and its pixels show no trail in the next 4 frames |
| `Test_TAA_DynamicResolutionKeepsHistory` | a scale step from 1.0 to 0.9 keeps the history valid and the output changes by at most FLIP 0.03 |
| `Test_TAA_EditorViewIndependence` | Rendering's `Test_View_EditorHistoryIndependentOfMain` with LIGHT's fog and exposure histories added |
| `Test_Fog_TemporalStability` | a static fog scene's per-froxel change after 16 frames is below 1% |
| `Test_Exposure_TemporalAdaptation` | a view entering a dark room adapts along the closed-form curve with `r.exposure.fixedDeltaSeconds` = 1/60, within 0.05 EV at every frame |
| `Test_Upscale_ConvergenceAtFloor` | at scale 0.75 the upscaled static scene converges within FLIP 0.04 of the native-resolution TAA result after 64 frames |

## Diagnostics tests

| Test | Asserts |
|---|---|
| `Test_LightingCounters_Emitted` | (Stage 3, in-code) one frame of `Flux_BuildCounterTestWorld` emits every counter of `Zenith2/Docs/Design/LIGHT.md → "Counters"` exactly once through a recording profiler hook (those of Stage 4 and 5 features at 0), the GPU counters within 3 frames, with the fixture's known `Flux.Lights.Count` (11), `.Directional` (1) and `Flux.Shadows.Views` |
| `Test_LightingCounters_EmittedInBasicScene` | (Stage 3 Forward+ integration) the same assertions over one frame of the `LightingBasic` parity scene, with that scene's known values |
| `Test_LightingDebug_EveryViewAddsPasses` | `Flux_LightingDebugExtension` reports `FLUX_EXTENSION_POINT_OVERLAY` and the main and editor view kinds; each value 1 … 17 makes its `AddPasses` add LIGHT's `LightingDebug` pass once on a harness graph, the graph compiles with no diagnostic and sync validation stays clean; value 0 adds none, and 18 leaves the previous view |
| `Test_FrameDump_LightingSection` | (Stage 3, in-code) a frame dump of a lit test scene contains the `lighting` object with the light counts, the sun and moon slot states, one entry per shadow view (key, kind, tile, matrices), the cascade splits, the probe slots (an empty array at Stage 3), the exposure values, the dynamic-resolution scale and the sky revision, and it parses with `Zenith_JsonDocument` |
| `Test_FrameDump_LightingSectionInBasicScene` | (Stage 3 Forward+ integration) the same object for a frame of `LightingBasic` |

## Failure and stress tests

| Test | Asserts |
|---|---|
| `Test_History_FirstFrameNoRead` | (Stage 2) a new view's `LightingHistoryProbe` declares no read of its previous half (graph inspection), the creation frame carries the clear, and its outputs are finite |
| `Test_TAA_FirstFrameNoHistoryRead` | a new view's TAA resolve declares no read of `m_xPrevHistory` or `m_xPrevVelocity` and takes the no-history branch |
| `Test_Fog_FirstFrameNoHistoryRead` | a new view's fog temporal step declares no read of the previous fog half and takes α = 1 |
| `Test_History_NaNSanitised` | a NaN injected into the TAA history is replaced and counted, and the output stays finite |
| `Test_Lighting_FrameDataOrderInForwardPlusFrame`, `Test_Probes_FrameDataDeclaredBeforeExtension`, `Test_LocalFog_FrameDataDeclaredBeforeExtension`, `Test_IrradianceVolume_FrameDataDeclaredBeforeExtension` | the row below, each for one producer set in its own `TEST_CASE`: the Stage 3 producers inside the real Forward+ frame; the probe records (64 bytes each, Stage 4); the local fog volumes (112 bytes each, Stage 5); the GI volumes (64 bytes each, Stage 5) — declared through `DeclareFrameData` at setup, written through `GetFrameData` at order 5 after `ExtendReservation`, and located by their element bases |
| `Test_Lighting_FrameDataDeclaredBeforeExtension` | (Stage 3, the shadow-view and view-frustum producers outside a frame) with the real `Flux_UploadRing` and the upload list's per-frame trace, a frame with shadows, probes, local fog volumes and GI volumes: every LIGHT producer calls `DeclareFrameData` during setup (owner strings the pass names), the declared bytes of each range equal `n × sizeof(T) + sizeof(T) − 16` for its n records (96 per shadow view, `sizeof(Flux_FrustumPlanes)` = 96 per frustum, 64 per probe, 112 per local fog volume, 64 per GI volume), and every write goes through `GetFrameData` inside `WriteFrameData()` after `ExtendReservation`, on the ring `Create` borrowed; LIGHT calls neither `Allocate` nor `GetFrameData` before it; each consumer field is bound to `Flux_LightingFrame::m_xFrameChunk`, and for ticket offsets of 16, 48 and 80 bytes (the double's choice) each consumer's plain-data element base is `ceil(offset / sizeof(T))` and every record lands at byte `base × sizeof(T)` inside its declared range (padded by `sizeof(T) − 16`) |
| `Test_Probes_DeferredFrameKeepsState`, `Test_SSGI_DeferredFrameKeepsState` | the Stage 4 rows (the sky-capture face and the probe capture progress) and the Stage 5 rows (the SSGI history state) of the row below, each in its own `TEST_CASE` |
| `Test_Lighting_DeferredFrameKeepsState` | (Stage 3 rows; the sky-capture face, probe capture and SSGI clauses are the two tests above) a 12-frame sequence with frame 5 deferred (and a second run with frame 5 failed) through the real `Flux_UploadRing` (a declared need above its reservation defers the frame, one above its cap fails it), the sky revision changing at frame 5, a sky capture on face 3 at frame 5, a light whose tile hysteresis counter is at 7 and an exposure readback in flight: frame 5 writes no ring byte from LIGHT (the double's cursor stays 0 and a poisoned range is unchanged), `Flux.Lighting.FramesNotSubmitted` counts 1; frame 6 declares the LUT steps and `Flux.Sky.LUTRebuilds` counts that rebuild once; the TAA, fog and SSGI history states, the jitter index, the sky-capture face (3 again in frame 6), the probe capture counts, the light's hysteresis counter and `m_uLastShadowIndex` after frame 6 equal those of the undeferred sequence after frame 5; the exposure keeps its value, `Flux.Exposure.Reset` is unchanged and frame 6 declares a new readback |
| `Test_LightingSystem_Create_FailsAt<Step>_Unwinds` (one `TEST_CASE` per step of `Zenith2/Docs/Design/LIGHT.md → "Construction and shutdown"`, each named with its step and owned by the ticket that implements the step: `Allocate`, `DFGLUT`, `SheenLUT`, `LTCLUT`, `DefaultEnvironment`, `LightSlots`, `AtlasAllocator`, `ProbeManager`, `BakedGIManager`, `LoadQueue`, `ControllerPool`, `ExposurePool`, `CounterReadback`; the pattern itself is never a test name) | failure injection at that step returns its error, undoes exactly the earlier steps in reverse order (the two light slots released when `LightSlots` or a subsequent step failed), and leaves no allocation, resource or registration behind ([Z2-N-CONTRACT-341]) |
| `Test_LightingSystem_LoadQueueFull` | 1,025 completions in one frame: the 1,025th is refused with `FLUX_LIGHTING_ERROR_QUEUE_FULL`, `flux.lighting.loadQueue.full` counts it, the asset system's retry lands it the next frame, and `.high_water` reports 1,024 |
| `Test_Probes_LoadCancelStress` | (Stage 4) 1,000 load and cancel cycles of baked probe assets leave no slice, resource or allocation behind |
| `Test_BakedGI_LoadCancelStress` | (Stage 5) 1,000 load and cancel cycles of irradiance-volume and lightmap-set assets leave no page, resource or allocation behind |
| `Test_Lighting_DeviceLossRecovery` | (Stage 3, in-code) after a simulated device loss the histories are invalid, the default content reloads and the atmosphere LUTs rebuild; 64 frames after recovery `Flux_BuildDeviceLossTestView` is within FLIP 0.02 of its pre-loss image at the same frame of its sequence |
| `Test_Lighting_DeviceLossRecoveryTemporalScene`, `Test_Probes_DeviceLossRecapture`, `Test_BakedGI_DeviceLossReload` | the `LightingTemporal` scene form (Stage 3 failure integration: within FLIP 0.02 of the pre-loss image 64 frames after recovery), the probe re-capture (Stage 4) and the baked-data reload (Stage 5), each its own `TEST_CASE` |
| `Test_Lighting_CriticalPressureShedding` | (Stage 3) at `MEMORY_PRESSURE_CRITICAL`, SSR is off, the fog grid drops a step, the dynamic-resolution scale is the floor, and nothing else changes; all revert one frame after the level falls (with Rendering's `Test_Memory_CriticalShedding`) |
| `Test_Lighting_CriticalPressureShedsPost` | (Stage 5) at CRITICAL, DoF and motion blur are off as well, and both are back one frame after the level falls |
| `Test_Lighting_MissingEnvironmentFallsBack` | a corrupt `.zenv` logs, counts and uses the default environment |
| `Test_Lighting_MissingLUTIdentity` | a missing or corrupt `.zlut` disables grading with a warning |

## Device runs

| Run | Content | Label |
|---|---|---|
| S24 Forward+ lighting | the `Lighting*` parity scenes on the S24 against the Windows goldens within the S24 tolerance; `Z2-B-LIGHT-*` GPU times on W-Indoor and W-Outdoor; on `QUALITY_TIER_S24_DEFAULT`, the sky-SH and probe recapture as the dynamic GI of that tier | `needs-human` device step, `gpu-reason:visual-measure` |
| RTX 3070 budget run | every `Z2-B-LIGHT-*` row on the workloads that name it | `needs-human`, `gpu-reason:visual-measure` |
| HDR10 display check | CinematicSlice's HDR segments on an HDR10 monitor and the S24 | `needs-human`, `gpu-reason:render-capture` |
| thermal run | WS-SUSTAIN on the S24: the scale and tier follow the thermal steps (PLAT's thermal device run) | `needs-human` |
