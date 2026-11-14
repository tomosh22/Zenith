# ENV design: test catalogue

Section file of `Zenith2/Docs/Design/ENV.md`. Every ENV test by its ID (`Test_<Type>_<Behaviour>`, doctest name `"<Library>/<Type>/<Behaviour>"`, `Zenith2/Docs/Contracts.md → "Tests and code hygiene gates"`), the file it lives in, the case it asserts and the gate step that runs it. Tests are pass or fail only; no count is pinned ([Z2-N-CONTRACT-408]). GPU tests run on WARP and lavapipe with validation clean ([Z2-N-REND-034]).

## Scope of this file

| Group | Heading |
|---|---|
| where tests live and which step runs them | "Test targets and locations" |
| unit tests per subsystem | "Unit tests" |
| worker-count and replay invariance | "Determinism tests" |
| cook tests | "Cook tests" |
| GPU render and parity tests | "Render and parity tests" |
| documented approximations | "Error-budget tests" |
| parsers | "Fuzz targets" |
| stress, sanitizer and failure injection | "Stress and sanitizer tests" |
| workloads and devices | "Workload and device evidence" |
| the step per group | "Gate steps per test group" |

## Test targets and locations

| Target | Sources | Links | Runs in |
|---|---|---|---|
| `ZenithEnvironment_Tests` | `Zenith2/Engine/ZenithEnvironment/Tests/**` | ZenithEnvironment and its declared dependencies, `ZenithTestSupport` | `Test` on `win-debug`, `win-release`, `android-arm64` builds (the module is `ZENITH_GPU`, so not `linux-tsan`) |
| `ZenithFlux_Tests` (ENV's files) | `Zenith2/Engine/ZenithFlux/Tests/Environment/**` | as GEOM's | `Test` on WARP and lavapipe |
| `ZenithCook_Tests` (ENV's files) | `Zenith2/Tools/ZenithCook/Tests/Environment/**` | ZenithCook's object library | `Test` on `win-debug` |
| `ZenithAuthoring_Tests` (ENV's files) | `Zenith2/Tools/Authoring/Tests/Environment/**` | `ZenithAuthoring`, `ZenithTestSupport`, doctest (`Zenith2/Docs/Design/EDIT.md → "ZenithAuthoring contract rules"`, rule 2) | `Test` in the `win` preset (`ZENITH_TOOLS_HOST`) |
| `Zenith_Integration_Environment` | `Zenith2/Tests/Environment/**` | ZenithRuntime, the Environment, Physics and Streaming modules | `Test` (headless Null world plus WARP where marked) |
| fuzz executables | `Zenith2/Engine/ZenithEnvironment/Tests/Fuzz/*.cpp` | libFuzzer | `win-asan` smoke; full duration at stage acceptance ([Z2-N-ARCH-952]) |
| fixtures | committed hand-written text only: `Zenith2/Engine/ZenithEnvironment/Tests/Fixtures/` (the 512 m fixture terrain's layer, foliage-type, placement and atmosphere documents and its constant layer textures (`*.consttex.json`) under `Terrain512/`, weather tables) and `Zenith2/Tools/ZenithCook/Tests/Fixtures/Environment/`; the terrain and its 16 tile documents (base64 COMPRESS payloads) are never committed: the header-only `Zenith_BuildTerrain512Fixture` (`Zenith2/Engine/ZenithEnvironment/Tests/Support/Zenith_EnvironmentFixtureBuilder.h`, [Z2-N-ARCH-255]) builds them in memory for the unit and cook tests | — | — |
| generated fixture set | the CTest setup entry `ENV_TerrainFixtures` (`ZenithCook_Tests --test-suite=z2-fixture-setup --test-case=Fixture_EnvTerrain_Generate`, `FIXTURES_SETUP EnvTerrainFixtures`, case file `Zenith2/Tools/ZenithCook/Tests/Environment/Zenith_EnvFixtures_Generate.cpp`) writes the fixture game `EnvFixture` under `Source/` (the built terrain and tiles, copies of the hand-written fixture documents, of every `Zenith2/Engine/ZenithFlux/Tests/Environment/Scenes/<Scene>/Content/` directory and of `Zenith2/Tests/Environment/Fixtures/Content/`, and `.meta` documents carrying the builder's fixed source GUIDs and, as `subAssets`, the fixed product GUIDs of `Zenith_GetTerrain512FixtureProducts` for every product key the importers declare, because the build cook fails a product without a GUID with `COOK_ERROR_META_SUBASSET_MISSING`; and the binary `Zenith.Data` shapes, a lake's polygon or a river's spline, which no one writes by hand: each is a `Data` entry of its scene's `scene.json` (`path` `<Name>.shape.bytes`, `guid`, `components` 2 or 4, `points` the numbers as text), written as the `u32` count and the little-endian `f32` values with a `.meta` naming ASSET's `raw` importer) and cooks it in process into `Cooked/`, both in `${ZENITH_OUT}/test-fixtures/ENV/`, named by `ZENITH2_ENV_FIXTURES` ([Z2-N-ARCH-257]); each target's doctest suite `z2-env-cooked` (`ZenithCook_Tests`, `ZenithFlux_Tests`, `Zenith_Integration_Environment`) is one entry with `FIXTURES_REQUIRED EnvTerrainFixtures`; a consumer that mutates the set copies it into a subdirectory of its own first; the entries exist only in presets that build `ZenithCook_Tests` (cross-document item X191); nothing generated is committed (ruling Q55) | `ZenithCook_Tests` (setup) | `Test` |
| migration fixtures | `Zenith2/Tools/Authoring/Tests/Fixtures/Migration/Environment/<Type>/v<N>.json` and `v<N>.expected.json` for each of the nine document types ([Z2-N-CONTRACT-548]; `<Type>` the stable name's last part, `TerrainDocument` … `CloudNoiseDocument`; version 1's pair is the identity, the canonical form of the input) | — | `Test` (in `ZenithAuthoring_Tests`, which links SCENE's framework through `ZenithAuthoring`'s `ZenithDocument` edge) |

## Unit tests

### Terrain document tests

File `Zenith2/Engine/ZenithEnvironment/Tests/Zenith_TerrainDocument_Tests.cpp`.

| Test ID | Asserts |
|---|---|
| `Test_TerrainDocument_Create_TileGeometry` | a 3 × 2 created document has 6 tiles of 257 × 257 samples at the initial height; sample (256, t) of tile (0, 0) and sample (0, t) of tile (1, 0) are one world point |
| `Test_TerrainDocument_Create_RejectsInvalidDesc` | each invalid field (0 tiles, 257 tiles, inverted limits, width over 8,192 m, non-finite origin, `m_fLODDistanceRatio` 7.9) returns `ENV_ERROR_INVALID_DESC` with that field's index; 8.0 and 64.0 are accepted |
| `Test_TerrainDocument_Load_LODRatioBelowEightRejected` | a document with `"lodDistanceRatio": 2.0` (and one with 65) fails with `ENV_ERROR_DOCUMENT_SCHEMA` naming the field; a document without the member loads with 8 |
| `Test_TerrainDocument_MergeFixtures` | the fixture pairs of "Merging terrain documents" (one tile changed on both sides, layer lists reordered on both sides, a tile added on each side) produce the conflicts and clean merges of that table through the merge policy's classification function |
| `Test_TerrainDocument_SaveLoad_RoundTripsBytes` | save, load, save again: identical bytes for every document; an unchanged save writes no tile |
| `Test_TerrainDocument_Load_MissingTile` | a listed tile the source does not have returns `ENV_ERROR_TILE_MISSING` naming the path |
| `Test_TerrainDocument_Load_CorruptTilePayload` | truncated base64, a bad section descriptor, a wrong decoded size each return `ENV_ERROR_TILE_CORRUPT` |
| `Test_TerrainDocument_Load_OutOfRangeValues` | a NaN height, a height beyond the limits, a hole value of 7, weights not summing to 255 each fail with the tile and field |
| `Test_TerrainDocument_Load_BorderMismatchFailsInCook` | a one-sample border difference fails with `ENV_ERROR_TILE_BORDER_MISMATCH` when repair is off |
| `Test_TerrainDocument_Load_BorderRepairInEditor` | with repair on, the smaller-index tile's value wins, the other tile is dirty, the counter equals the mismatched sample count |
| `Test_TerrainDocument_UnknownFieldPreserved` | an unknown member in each document kind survives load and save at its anchor (rule C3 of SCENE's "Canonical rules") and logs one warning |
| `Test_TerrainDocument_NewerVersionReadOnly` | a document with a newer schema version loads read-only and `Save` returns `ENV_ERROR_DOCUMENT_READ_ONLY` |
| `Test_TerrainDocument_AddRemoveTile_BordersFromNeighbours` | an added tile takes its border samples from present neighbours; removing and re-adding by undo restores the bytes |
| `Test_TerrainDocument_LayerLimit` | a 17th layer returns `ENV_ERROR_LAYER_LIMIT` |
| `Test_TerrainDocument_RemoveLayer_WeightRedistributed` | removing a painted layer leaves every texel summing to 255 per "Weight normalisation" |

### Brush tests

File `Zenith2/Engine/ZenithEnvironment/Tests/Zenith_TerrainBrush_Tests.cpp`.

| Test ID | Asserts |
|---|---|
| `Test_TerrainBrush_Falloff_Table` | `Zenith_TerrainBrushFalloff` at d = 0, H, (1+H)/2 and 1 for H in {0, 0.5, 1} equals the formula |
| `Test_TerrainBrush_Raise_ExactValues` | a single RAISE dab on a flat tile writes `h + S w A` at 16 sampled points and nothing outside the radius |
| `Test_TerrainBrush_Flatten_IsARate` | two FLATTEN dabs at `S w = 0.5` close 75% of the gap; one dab at `S w = 1` assigns |
| `Test_TerrainBrush_SetHeight_IsAnAssignment` | SET_HEIGHT assigns the target inside the hard region for every strength |
| `Test_TerrainBrush_Smooth_ReadsSnapshot` | a SMOOTH dab's result equals a reference that blurs a copy of the pre-dab tile (visit order irrelevant) |
| `Test_TerrainBrush_Noise_SeedReproducible` | the same stroke and seed produce identical bytes; a different seed differs |
| `Test_TerrainBrush_Ramp_Profile` | a RAMP dab gives the linear profile along the segment within 1 ULP of the formula |
| `Test_TerrainBrush_ErodeThermal_ReducesSlopes` (Stage 5) | the maximum slope in the rectangle decreases and total height is conserved within 1e-3 m |
| `Test_TerrainBrush_ClampsToLimits` | a stroke beyond the limits clamps and increments `environment.terrain.brush_clamped` |
| `Test_TerrainBrush_BorderStaysEqual` | a dab across a tile corner leaves every shared border sample equal in all four tiles |
| `Test_TerrainBrush_ParallelEqualsSerial` | a 2,000-dab stroke applied with no job system and with 1, 2, 8 and all workers gives bit-identical tiles |
| `Test_TerrainBrush_InvalidDab_ChangesNothing` | a NaN centre, a radius of 0.1 m, a strength of 2 each return `ENV_ERROR_BRUSH_INVALID` with the dab index and leave the document's bytes and revision unchanged |
| `Test_TerrainBrush_PaintLayer_WeightsSumTo255` | after a random 500-dab paint stroke over 5 layers every texel sums to 255 and matches the integer reference of "Weight normalisation" |
| `Test_TerrainBrush_EraseToZero_DropsMap` | erasing a density map to zero drops it; undo restores it |
| `Test_TerrainBrush_Hole_PaintErase` | holes flip at `S w ≥ 0.5` only |
| `Test_TerrainBrush_ScatterFoliage_Deterministic` | the same scatter stroke on two copies of a document adds identical explicit instances with the same increasing IDs; a different `m_ulNoiseSeed` differs |
| `Test_TerrainBrush_ScatterFoliage_KeepsFootprints` | on a tile holding 50 explicit instances of two types, a 40-dab scatter stroke adds instances each at least `footprint(new) + footprint(existing)` from every pre-existing instance and at least `min(2 × footprint, 64)` from every other new instance of the stroke, with IDs continuing `m_uNextExplicitID`; a type of footprint 40 m spaces its new instances 64 m apart |
| `Test_TerrainBrush_ScatterFoliage_CapacityRestores` | a stroke whose dab would push a tile past 16,384 explicit instances returns `ENV_ERROR_CAPACITY` with the dab index and leaves the document's bytes and revision equal to the pre-stroke ones |
| `Test_TerrainEdit_UndoRedo_RoundTripsBitExact` | for every brush kind: stroke, revert, reapply; the document bytes equal the post-stroke and pre-stroke bytes exactly |
| `Test_TerrainEdit_StaleRevision_Refused` | reverting an edit after another edit returns `ENV_ERROR_EDIT_STALE` and changes nothing |
| `Test_TerrainEdit_MemoryProportionalToArea` | a 10,000-dab stroke inside one 16 m disc records one union rectangle, not one per dab |

### Splat tests

File `Zenith2/Engine/ZenithEnvironment/Tests/Zenith_TerrainSplat_Tests.cpp` and `Zenith2/Engine/ZenithFlux/Tests/Environment/Flux_TerrainMaterial_Tests.cpp`.

| Test ID | Asserts |
|---|---|
| `Test_TerrainSplat_TopFourLosslessUpToFour` | texels with at most four layers keep their weights exactly |
| `Test_TerrainSplat_TopFourTieBreak` | equal weights keep the lower palette indices; the redistributed sum is 255 |
| `Test_TerrainSplat_OverFourReported` | the reduction counts texels with more than four layers |
| `Test_TerrainMaterial_BlendMatchesCpuReference` | a compute harness evaluating steps 1–3 of "Layer blend in the terrain material" at 4,096 points equals a CPU reference in test code (indices exactly, weights within 2^−10) |
| `Test_TerrainMaterial_SplatIndexDecodeExact` | a 16 × 16 `R8G8B8A8_UNORM` texture holding every value 0 … 255 in each lane, read with `Load` and decoded `uint4(round(x × 255))` on both backends, returns every value exactly |
| `Test_TerrainMaterial_TwoLayerTierRenormalises` | with `uPERM_LAYERS = 2` the two kept weights sum to 1 within 2^−10 |
| `Test_TerrainMaterial_HeightBlend` | the height-based blend of two layers at known heights matches the formula; δ = 1 equals the linear blend |
| `Test_TerrainMaterial_ExplicitGradientsOnly` | the ShaderChecks lint `ShaderChecks.MaterialExplicitGradients` passes on `TerrainLayered.slang` (a lint fixture) |

### Foliage tests

File `Zenith2/Engine/ZenithEnvironment/Tests/Zenith_FoliageSystem_Tests.cpp`.

| Test ID | Asserts |
|---|---|
| `Test_FoliageInstance_LayoutIs32Bytes` | `sizeof` and field-by-field serialization of the record |
| `Test_FoliageType_DocumentValidation` | each invalid combination (render-only with collision, scale min above max, cull distance 0) fails the import with `COOK_ERROR_IMPORT_FAILED` and a diagnostic beginning `ENV_ERROR_DOCUMENT_SCHEMA` and naming the foliage type and field |
| `Test_Foliage_RenderKeyStable` | the render key of an instance is identical across two loads of the same cell and differs between cells |
| `Test_Foliage_ActivationEmitsAddDeltas` | activating a cell of 1,000 instances emits exactly its drawn instances as ADD over `ceil(1000 / limit)` frames |
| `Test_Foliage_DeactivationRemovesEveryKey` | unloading the cell emits REMOVE for exactly the keys emitted |
| `Test_Foliage_FullExtractionReemits` | a full-state frame re-emits every drawn instance of every active cell and nothing else |
| `Test_Foliage_ThinningMonotonic` | lowering the density scale from 1 to 0.3 and back removes and restores the same instances; the kept set at 0.3 is a subset of the kept set at 0.6 |
| `Test_Foliage_SimulationVisibleIgnoresTier` | collidable and gameplay-visible types are drawn in full at density scale 0 |
| `Test_Foliage_InstanceCapShedsRenderOnlyFirst` | with a cap below the resident count, only render-only instances are shed: the kept render-only instances are those of the lowest thinning ranks, so the highest ranks are shed first |
| `Test_Foliage_TableFullLowersCap` | a refused GPU-scene add lowers the cap to the accepted count next frame and logs once |
| `Test_Foliage_QueryInstancesSortedAndBounded` | `QueryInstances` returns (distance, cell, key) order and `ENV_ERROR_CAPACITY` when the span is short |
| `Test_Foliage_QueryRenderOnlyRefused` | querying a render-only type returns `ENV_ERROR_FOLIAGE_NOT_QUERYABLE` |
| `Test_Foliage_FailedCellStaysEmpty` | a failed cell asset leaves the cell empty, counts `environment.foliage.cells_failed`, other cells activate |
| `Test_DetailFoliage_HashStableAcrossFrames` | generating a detail cell twice (and after moving the camera away and back) produces identical pool contents |
| `Test_DetailFoliage_LowerDensityIsSubset` | the blades at density 0.5 are a subset of the blades at density 1.0 |
| `Test_DetailFoliage_SourceSizingAndIndices` | `Flux_DetailFoliageSource::GetDesc` returns the descriptor table of "Detail source"; `ComputeSizing` returns `rows × n × 8` vertices, 18 or 12 indices per point, the bounds of "Blade and card geometry" and the UV range for n = 1, 12, 48 and 64 in both rings; `WriteIndices` writes the triangles of that heading, every index below the vertex count; a CPU evaluation of the generator with `cardSize` 4, `bladeHeightMax` 4, `bendMax` 1, `bladeWidth` 0.5 and every draw at 0 and at 1 writes every position inside the returned box before the writer's clamp, and the box contains every position after the draw-time lift and a wind offset of 0.5 m |
| `Test_DetailFoliage_ExtractorEmitsProceduralRecords` | with the anchor at a fixture position, the extractor emits one `Flux_RenderProceduralInstance` with `REGENERATE` and one ADD delta per selected (cell, type, block), none for cells beyond R + 8 m, at most `detailCellsPerFrame` new ones per frame, and a record with a raised topology version when a cell crosses the near radius |
| `Test_DetailFoliage_BlocksWithinGeomLimits` | for every n from 1 to 64 and a forced lattice of 128, every block is at most 8,191 points and 65,535 vertices, and the blocks cover every lattice row exactly once |
| `Test_DetailFoliage_VertexCapNearestFirst` | with `detailMaxVertices` below the demand, the admitted instances are the nearest to the anchor, their vertex total is within the cap, and the refused cells count `environment.foliage.detail_cells_capped` |

### Wind tests

File `Zenith2/Engine/ZenithEnvironment/Tests/Zenith_Wind_Tests.cpp` and `Zenith2/Engine/ZenithFlux/Tests/Environment/Flux_Wind_Tests.cpp`.

| Test ID | Asserts |
|---|---|
| `Test_Wind_StateDeterministicPerTick` | the wind state after N ticks is identical in two runs and in a run restored from a save at tick N/2 |
| `Test_Wind_WanderBounded` | over 1,000,000 ticks the wander stays within ±0.5 rad |
| `Test_Wind_GpuMatchesCpuMirror` | `Env_WindSample` at 4,096 points and 8 times matches `Zenith_EvaluateWindCPU` within the tolerance of "CPU mirror and tolerance" |
| `Test_Wind_TimeReductionContinuous` | the wind time written across the 3,600 s reduction boundary changes by one frame's step |
| `Test_FoliageWind_LengthPreservingBend` | the bend keeps every vertex's distance from the pivot within 1e-5 relative |
| `Test_FoliageWind_ShadowMatchesMainDepth` | the shadow view of a swaying tree matches the reference built from the main view's displaced positions (depth bit-exact at the silhouette samples) |
| `Test_FoliageWind_VelocityFromPreviousTime` | the prepass velocity of a displaced leaf vertex equals the difference of the current and previous-time positions projected |

### Water tests

File `Zenith2/Engine/ZenithEnvironment/Tests/Zenith_WaterSystem_Tests.cpp`.

| Test ID | Asserts |
|---|---|
| `Test_Water_ValidateWaveSet_Rejections` | 9 waves, a 0.1 m wavelength, a non-unit direction and a folding set (Σ Q k A > 1) are each rejected with the wave index |
| `Test_Water_Gerstner_KnownValues` | one wave at t = 0 and t = T/4 gives the closed-form displacement |
| `Test_Water_HeightQuery_ResidualBelow1mm` | at 10,000 points the fixed-point query's horizontal residual is below 1 mm against 64-step bisection |
| `Test_Water_Sample_DeterministicAcrossWorkers` | the same queries issued from 1, 2, 8 and all workers in one tick return identical bits |
| `Test_Water_WindDrivenAmplitude` | the amplitude scale follows `min(2, (speed/8)²)` at the tick's wind speed |
| `Test_Water_OutOfBounds` | a point outside every body returns `ENV_ERROR_OUT_OF_BOUNDS` |
| `Test_Water_LakePolygonContainment` | points inside and outside a concave lake polygon are classified correctly; a self-intersecting polygon is rejected at cook |
| `Test_Water_Submersion` | `IsSubmerged` is true below the displaced surface and false above, including at the waterline within 1 mm |
| `Test_Water_BodyRecordCarriesHandles` | a body's ADD record carries its material, grid and (ocean only) skirt handles; REMOVE is written when the component disappears and the module then releases its handles |
| `Test_Water_NodeCustomDataLayout` | node instances' custom data round-trips level, node x, node z and body index at their bit limits; the skirt carries level 15 |
| `Test_Water_BodyRemoveReleasesEverything` | after a body REMOVE, `Flux_WaterRenderer` holds no instance, table index or handle of the body |
| `Test_Water_OceanCoverageNoGap` | for 1,000 reference positions, including positions on and 1 mm either side of the 4,096 m grid's half-lines, every XZ sample of a 16 m grid within 20 km of the camera lies in exactly one selected ocean node or one skirt quad (undisplaced footprints), the skirt's inner edge equals the root square's boundary bit for bit, the root square's edge vertices have a fade factor of exactly 0, and for an ocean bounded by a rectangle no drawn quad reaches outside it |
| `Test_Water_SubmergedViewRecordsCapped` | 20 submerged texture views and one above water give exactly 16 records, those nearest the anchor, each whose `m_xMedium.m_ulViewID` equals the view ID RPROD's camera extractor computes for its request, none for the dry view, and 4 counts of `environment.water.submerged_views_dropped` |
| `Test_Water_NodeSelectionCrackFree` | for a sweep of reference positions, every pair of adjacent selected nodes differs by at most one level and their shared edge vertices displace to bit-identical points with the fixture wave set (r = 8) |

### Weather tests

File `Zenith2/Engine/ZenithEnvironment/Tests/Zenith_WeatherSystem_Tests.cpp`.

| Test ID | Asserts |
|---|---|
| `Test_Weather_TransitionTable` | a table-driven test: for every cell of "Transition table" (4 machine states × 5 events), the next state, the action and the events emitted equal the table |
| `Test_Weather_DwellAndChoiceDeterministic` | two runs of 1,000,000 ticks with the same seed produce the same state sequence; a different seed differs |
| `Test_Weather_ChoiceFollowsWeights` | over 100,000 successor draws the frequencies are within 1% of the weights |
| `Test_Weather_ForceHoldRefusesRequests` | a request while held is refused and counted; release resumes automatic transitions with a new dwell |
| `Test_Weather_RestartMidTransitionBlendsFromCurrent` | a request during a transition starts from the blended parameters, with no parameter jump |
| `Test_Weather_BlendSmoothstepAndDirectionArc` | blended parameters at a = 0, 0.5, 1 match the formulas; wind direction blends along the shorter arc across 0/2π |
| `Test_Weather_WetnessConvergenceRates` | wetness rises in 600 s and dries in 1,800 s of simulation time |
| `Test_Weather_UnknownStateCommand` | a command naming no state returns `ENV_ERROR_WEATHER_STATE_UNKNOWN` and changes nothing |
| `Test_Weather_TableValidation` | an unknown transition target, a duplicate state name, zero weight and a missing initial state each fail the cook |
| `Test_Weather_SaveRestoreContinuesSequence` | saving at tick N and restoring reproduces the uninterrupted run's sequence from N |
| `Test_Weather_EffectPrefabsSpawnedAtSyncPoint` | entering Rain spawns its prefabs through commands visible to phase 2 of the same tick; leaving it destroys them |
| `Test_Weather_ChangedEventDeliveredPhase9` | the change event emitted in phase 1 is delivered in phase 9 of the same tick |

### Atmosphere and time-of-day tests

File `Zenith2/Engine/ZenithEnvironment/Tests/Zenith_TimeOfDay_Tests.cpp` and `Zenith_AtmosphereParameters_Tests.cpp`.

| Test ID | Asserts |
|---|---|
| `Test_TimeOfDay_ExactDayAfterDTicks` | after exactly D ticks the clock is at the start time and the day counter is +1, for D values including primes and 1 |
| `Test_TimeOfDay_NoDriftOverLongRun` | after 10 × D ticks the clock is exact (no accumulated error) |
| `Test_TimeOfDay_FrozenWhenZeroDayLength` | D = 0 leaves the clock unchanged |
| `Test_TimeOfDay_SetTimeAndScaleCommands` | the commands take effect at the next tick's phase 1 and zero the remainder |
| `Test_TimeOfDay_MarkersInOrder` | a scale jump across sunrise and two hour marks emits the three events in time order |
| `Test_TimeOfDay_SaveRestore` | the clock, day and remainder round-trip through a save |
| `Test_Celestial_SolarPositionKnownValues` | at latitude 48°, day 172: solar noon elevation 65.44° ± 0.05°, sunrise hour within 2 minutes of the formula's closed form |
| `Test_Celestial_MoonPhaseCycle` | the phase advances by 1/29.530589 per day and the illuminated fraction is 1 at phase 0.5 |
| `Test_Atmosphere_ValidateLimits` | every field of "Validation limits" at its bounds passes and one step beyond fails with its index |
| `Test_Atmosphere_WeatherModulation` | haze and fog scales multiply the Mie and fog terms exactly; snow raises ground albedo |
| `Test_Atmosphere_ToFluxUnits` | every LIGHT field of `Flux_AtmosphereParams` and `Flux_FogMediumParams` equals its source field of `Zenith_AtmosphereParameters` (no unit change: the parameters are already SI; the reader's per-kilometre conversion is asserted by `Test_EnvironmentDocuments_S3ReadersRoundTrip`) |
| `Test_Atmosphere_SunMoonSlots` | every member of the blocks' `m_xSun` and `m_xMoon` equals the table of "Sun and moon illuminance" in Atmosphere.md at noon, at dusk, below the horizon cut and at a full and a new moon: the named keys, `FLUX_LIGHT_DELTA_OP_UPDATE`, `FLUX_LIGHT_TYPE_DIRECTIONAL`, `m_uLightingChannels` 0x01 and `m_uVolumetricScale` 128 on both slots, `m_uFlags` exactly 0x7 on the sun and exactly 0x4 on the moon at or below 1 lux and 0x5 above it (so bits 1, 3–31 of the moon and 3–31 of the sun are zero), the source radii equal to the angular radii, the sun's tint its colour over its largest component and the moon's white, the directions, the intensities (0 below the cut with the flags unchanged), the zero spot and rect members and LIGHT's shadow defaults; each slot passes `Flux_ValidateEnvironmentLight` with no flag cleared; `m_bHasSun`/`m_bHasMoon` are false when the settings disable them |

### Procedural placement tests

File `Zenith2/Engine/ZenithEnvironment/Tests/Zenith_ProceduralPlacement_Tests.cpp`.

| Test ID | Asserts |
|---|---|
| `Test_Placement_WorkerCountInvariant` | a 16-cell run with three rules gives bit-identical results with 1, 2, 8 and all workers and with three shuffled cell orders |
| `Test_Placement_CellSubsetMatchesFullRun` | placing any single cell alone equals that cell of the full run, including a fixture in which a survivor that cross-rule exclusion removes suppresses a lower-priority neighbour by spacing across a cell border |
| `Test_Placement_RuleLayerResolved` | a rule naming a layer by its ID filters on that layer's weights whatever palette index it has; reordering the palette changes no instance; an unknown layer ID fails with `ENV_ERROR_PLACEMENT_RULE_INVALID` naming the stage and the layer entry |
| `Test_Placement_MinDistanceHeld` | no two kept instances of one rule are closer than `minDistance`, including across cell borders |
| `Test_Placement_CrossRuleExclusion` | no subsequent-rule instance lies within the exclusion distance of an earlier rule's instance |
| `Test_Placement_FiltersApplyInOrder` | candidates rejected for slope, height, layer, water and exclusion are counted under the first failing filter |
| `Test_Placement_DensityMapAcceptance` | the kept fraction over a uniform density map of 0.25 is 0.25 ± 0.01 before spacing |
| `Test_Placement_CanonicalOrder` | each cell's output is sorted by instance key and every key equals `Zenith_PlacementInstanceKey` of its instance |
| `Test_Placement_MultipleRuleSets_KeysUniqueAndExcluded` | two rule sets that both contain a rule named "Pines" over one cell: every key of the cell is unique, the two rules draw different streams, and every excluding instance of the second set lies outside the exclusion distance of the first set's kept instances |
| `Test_Placement_RuleSetExclusionMask` | a mask painted for set 1 rejects set 1's candidates and not set 0's; the ALL mask rejects both; texels 127 and 128 fall on either side of the 0.5 threshold |
| `Test_Placement_ExcludingStageLimit` | a run of 9 excluding stages returns `ENV_ERROR_PLACEMENT_RULE_INVALID` naming the ninth stage's global index; 8 are accepted |
| `Test_Placement_SingleCellRingBound` | a counting surface records the cells each stage samples for a single requested cell with 8 excluding stages: never more than 361 per stage |
| `Test_Placement_CapacityError` | exceeding the per-cell cap returns `ENV_ERROR_PLACEMENT_CAPACITY` with the first instances in order |
| `Test_Placement_RuleValidation` | a density of 0, a minimum distance of 65 m and an unknown foliage type are each rejected with the rule index |
| `Test_Placement_EditorPreviewEqualsCook` | the editor preview path and the cook step produce identical instances for the fixture terrain |

### Cell and runtime tests

Files `Zenith2/Engine/ZenithEnvironment/Tests/Zenith_TerrainRuntime_Tests.cpp` and `Zenith2/Tests/Environment/`.

| Test ID | Asserts |
|---|---|
| `Test_TerrainRuntime_HeightMatchesCollisionTriangles` | at 10,000 points the CPU height equals the triangle interpolation of the extracted collision tile, bit for bit |
| `Test_TerrainRuntime_NotResidentAndOutOfBounds` | queries in an inactive tile and outside the terrain return their errors and counters |
| `Test_TerrainRuntime_TileActivationAtTickBoundary` | a tile whose load completes mid-tick is answered from the next tick, identically on replay |
| `Test_TerrainRuntime_PhysicalMaterialDominantLayer` | the quad's physical material is its dominant layer's, ties to the lower index |
| `Test_TerrainRuntime_HoleFlag` | a query over a hole returns `m_bHole` with the triangle height |
| `Test_TerrainRuntime_Raycast` | rays against a sloped fixture hit within 1 mm of the analytic intersection; a ray over holes and absent tiles passes through; a ray reaching an inactive tile first returns `ENV_ERROR_NOT_RESIDENT`; a `uMaxSteps` smaller than the quads to the hit returns `ENV_ERROR_CAPACITY` |
| `Test_EnvironmentModule_Headless_RunsSimulationSystems` | a headless world with the module runs 300 ticks: time of day, weather and wind advance, height and water queries answer, no extractor runs |
| `Test_EnvironmentCells_LoadUnload1000Cycles_NoLeak` | 1,000 randomised cell load, cancel and unload cycles leave no ENV asset handle, render instance, world-resource entry or allocation |
| `Test_EnvironmentBindings_MisuseRaisesLuauErrors` | each binding misuse of "Luau errors and asserts" in the entry raises a controlled Luau error naming the argument |

### Module integration and diagnostics tests

Files `Zenith2/Engine/ZenithEnvironment/Tests/Zenith_EnvironmentModule_Tests.cpp`, `Zenith2/Engine/ZenithFlux/Tests/Environment/Flux_EnvironmentRenderer_Tests.cpp` and `Zenith2/Tests/Environment/`.

| Test ID | Asserts |
|---|---|
| `Test_EnvironmentErrors_ValuesStable` | `static_assert`s of every `Zenith_EnvironmentErrorCode` value (1 … 28) and of the record's size (12 bytes, trivially copyable); `Zenith_ToString` names every code |
| `Test_EnvironmentConfig_KeysRegistered` | a configuration dump after start lists every key of "Configuration keys" with its type, range, default, layers and restart flag, and none is simulation-affecting |
| `Test_EnvironmentComponents_Registration` | every component resolves by stable name, its property authorities match the table, asset-handle properties balance references through the hooks, and serialization by property ID round-trips; the two event types resolve by stable name with the property IDs and names of the header comments (`WeatherChanged` 1 `FromNameHash` … 8 `WindDirection`, `TimeOfDay` 1 `Marker` … 4 `Day`), and an expected list with a renamed property is reported naming the ID and both names |
| `Test_EnvironmentSchedule_SecondWriterRejected` | a fixture system declaring a direct write of an ENV world resource fails the schedule compile with `SCHEDULE_ERROR_RESOURCE_AUTHORITY` |
| `Test_TerrainRuntime_TerrainAddRemoveRecords` | a terrain component whose asset activates yields one ADD record with the lowest free slot; removing the component yields one REMOVE and frees the slot; a full-state frame yields ADD again with the same slot |
| `Test_EnvironmentExtract_RecordsIndependentOfTickCount` | the same simulation run extracted with one tick per frame and with 0, 1 and 3 ticks per frame gives, at every frame boundary it shares, the same set of live terrain and water-body keys in the render side's bookkeeping, and no phase-1 resource is written by the extractor (the schedule rejects a declared write) |
| `Test_EnvironmentExtract_RefusedRecordRetried` | with the slot arena filled, a terrain ADD refused with `FLUX_RENDER_WORLD_ERROR_SLOT_FULL` stays pending and is emitted in the next frame, and a refused view record costs that view its override for one frame |
| `Test_EnvRender_TwoTerrainsRecordsEitherOrder` | two terrains activated in either order, and one removed and re-added, each draw with their own heights and splat (readback per terrain), the slot table maps each extractor slot to its record index, each detail instance reads its own terrain's density, and a terrain asset live in two worlds is refused once with `Flux.Environment.TerrainMaterialShared` |
| `Test_EnvironmentAssets_AccessorsMatchSections` | loading the fixture's `.zenvtile`, `.zfoliage`, `.zatmos`, `.zweather` and `.zwater` files through ENV's kind loaders gives runtime objects whose every accessor of Bake.md "Runtime asset objects" equals the decoded section values (spans in place: their sizes and first and last elements), and a mutated section of each fails the load with the section index |
| `Test_CloudNoiseKindLoader_ExtentAndChain` | loading each engine `.zcloudnoise` gives a `Flux_CloudNoiseResource` whose `GetExtent()` is the file's N, whose texture has log2 N + 1 mips, and whose `GetSRV()` is non-null after the upload completes |
| `Test_TerrainKindLoader_BuildsDescAndRecords` | loading the fixture `.zterrain` builds a `Flux_TerrainResource` whose `GetDesc()` equals the GEOM sections' values (ratio 8) naming the generated `material:terrain`, whose `GetInfo()`, `GetLayers()`, `GetTileAssetGUIDs()` (zero for an absent cell) and `GetCookedRange()` equal the ENV sections, and a tile table whose entries name the tiles' height, hole, splat and density textures |
| `Test_EnvRender_TerrainTileTableFollowsSwap` | a forced streaming swap of a splat texture rewrites that tile's table entry in the same frame the swap publishes, and the terrain renders identically before and after; a density texture still loading and one FAILED (`IsFallbackBound()` true) are bound as null and draw no detail blade, and the frame its load completes binds it |
| `Test_Atmosphere_RenderBlocksWrittenInPhase1` | the `Zenith.Environment.RenderBlocks` resource changes only in phase 1; `m_uSkyRevision` rises exactly on ticks whose atmosphere, fog or stars changed; `m_bValid` is false and every member at its default without a settings component; a fixture phase-12 system declaring a write of it fails the schedule compile with `SCHEDULE_ERROR_RESOURCE_AUTHORITY` |
| `Test_Atmosphere_SceneDirectionalLightYields` | on WARP, a world whose settings drive the bodies and which also holds a directional light component draws the sun and moon as directional lights 0 and 1 (LIGHT's light list), counts one `Flux.Lights.DirectionalDropped`, and logs `ENV_WARNING_SCENE_SUN_IGNORED` once |
| `Test_EnvironmentAnchor_FocusAndFallback` | detail cells, foliage activation order and the effect prefabs' transforms follow the anchor entity; without one they use the world origin and `ENV_WARNING_ANCHOR_MISSING` logs once; per tick, `Zenith_EnvironmentState`'s serialized properties and the answers of a fixed query set (terrain heights, water samples and foliage queries at 64 fixture points) are identical with and without the anchor and for two anchor paths |
| `Test_EnvironmentConfig_TierChangeRepublishes` | a quality-tier change committed in the prologue republishes the foliage settings and cloud tier before the frame's nodes run; a node reading the settings mid-frame sees one value |
| `Test_EnvironmentModule_NotSelectedNoGpuSide` | a fixture game without the module creates no `Flux_EnvironmentRenderer`, registers no `env.detailFoliage` generator, and its cooked package listing holds no ENV shader blob or material |
| `Test_Atmosphere_CorruptCookedFallsBack` | a cooked atmosphere with an out-of-range coefficient is refused and the world uses LIGHT's defaults with the warning |
| `Test_DetailFoliage_TierSettings` | each tier file's keys produce the generator radius, density and geometry of "Detail foliage tiers" |
| `Test_Clouds_TierFallback` | each `environment.clouds.tier` value selects its technique; `enabled = false` adds no cloud pass |
| `Test_Weather_SurfaceFunctionMatchesFormula` | a compute harness of `Env_ApplySurfaceWeather` matches the formulas of "Shared wetness function" within 2^−10 |
| `Test_Placement_RuntimeSurfaceReproducible` | two runs placing the same cells from the world seed over the runtime terrain resource produce identical instances |
| `Test_EnvironmentCells_ContentRecords` | a cooked cell holds exactly the ENV, PHYS and AI entities of "Cell content records" with the right asset references |
| `Test_EnvironmentCells_RebaseReextracts` | an origin rebase (STREAM's fixture) triggers a full-state extraction of ENV's records at the rebased positions |
| `Test_EnvironmentCounters_Emitted` | a recording hook table sees every counter of "Counters" with its expected value in a scripted scene |
| `Test_EnvironmentProfilerPanel_Rows` | the Environment panel's data model carries every row of "On-screen profiler rows" with its budget labels |
| `Test_EnvironmentDebugViews_Registered` | every debug variable of "Debug views" is registered and adds its pass or overlay in a development build |
| `Test_EnvironmentFrameDump_HasEnvironmentObject` | the frame dump holds the `environment` object with its members |
| `Test_EnvironmentLog_WarningsOncePerAsset` | each warning logs once per asset or document per run with the GUID |
| `Test_EnvironmentCook_ReportMembers` | the cook report carries every `environment` member of "Cook report members" for the fixture |

### Stage 5 tests

| Test ID | Asserts |
|---|---|
| `Test_Wind_LocalSourcesBounded` | at most 16 local sources reach the wind block, nearest first; their contributions are bounded by their radius and duration |
| `Test_EnvRender_PlanarReflectionView` | on desktop high, one planar reflection view is requested for the largest visible water plane within the view budget and sampled by `Material_Water` |
| `Test_Water_RiverQuery` | the river CPU query returns the strip height and the flow velocity at points along the spline |
| `Test_EnvRender_RiverCrossBackend` | the `River` scene agrees between the backends (as in "Render and parity tests"); on each, the depth-reconstructed strip surface at 1,024 sampled pixels equals the river CPU query's height within 1 cm |
| `Test_EnvRender_CausticsCrossBackend` | the `Caustics` scene agrees between the backends; on each, seabed pixels deeper than 20 m equal, bit for bit, the same frame rendered with caustics disabled, and shallower ones differ |
| `Test_EnvRender_CloudShadowsCrossBackend` | the `CloudShadows` scene agrees between the backends; on each, the cloud-shadow map at 256 texels equals the CPU layer transmittance along the sun of `Test_Clouds_ShadowMapMatchesLayerTransmittance` within `Z2-B-ENV-009` |
| `Test_EnvRender_WeatherTiersCrossBackend` | the `WeatherTiers` scene agrees between the backends on each tier; on each, the roughness channel changes exactly at the texels the CPU puddle threshold of "Shared wetness function" selects, and pixels under the roof occluder carry no precipitation term |
| `Test_Atmosphere_LocalFogVolumesExtracted` | at most 64 local fog volumes nearest the environment anchor reach the render-blocks resource and LIGHT's atmosphere block with their fields |
| `Test_EnvRender_TerrainVTReference` | the TerrainVT scene (`Zenith2/Docs/Design/ENV/Terrain.md → "VT-backed terrain materials"`) rendered by the raster path agrees with the converged path-traced reference within `Z2-B-ADVGFX-008`, in the physical-accuracy suite |

## Determinism tests

| Test ID | File | Asserts |
|---|---|---|
| `Test_EnvironmentDeterminism_ReplayWorkerInvariant` | `Zenith2/Tests/Determinism/Zenith_EnvironmentDeterminism_Tests.cpp` | a 3,600-tick replay of the fixture world with weather transitions, time commands from a script and streamed tiles produces identical per-tick world hashes (including `Zenith_EnvironmentState`) with 1, 2, 8 and all workers, forced compaction and two chunk capacities, on each ECS backend ([Z2-N-ARCH-886]) |
| `Test_EnvironmentDeterminism_TierIndependent` | same file | the same replay under the lowest and highest quality tiers gives identical world hashes (tier changes presentation only) |
| `Test_EnvironmentDeterminism_PressureIndependent` | same file | the same replay with forced memory pressure CRITICAL gives identical world hashes ([Z2-N-ARCH-812]) |

## Cook tests

File `Zenith2/Tools/ZenithCook/Tests/Environment/Zenith_EnvironmentCook_Tests.cpp`, except the `Test_EnvironmentDocumentTypes_*` rows, which live in `Zenith2/Tools/Authoring/Tests/Environment/Zenith_EnvironmentDocumentTypes_Tests.cpp` and run in `ZenithAuthoring_Tests`.

| Test ID | Asserts |
|---|---|
| `Test_EnvironmentCook_ByteIdenticalAcrossWorkers` | cooking the fixture terrain twice with 1 worker and twice with 8 produces identical bytes for every output |
| `Test_EnvironmentCook_IncrementalEqualsClean` | editing one tile and re-cooking incrementally gives the same bytes as a clean cook |
| `Test_EnvironmentCook_DerivedDataKeyCoversInputs` | changing each declared input (a tile, a halo tile, a rule, a foliage type, the platform, the algorithm version) changes the key; an undeclared change does not |
| `Test_EnvironmentCook_TerrainSectionsComplete` | the `.zterrain` holds every section of the registry in table order and each size equals its layout |
| `Test_EnvironmentCook_HeightsMatchGeomTiles` | every `.zenvtile` height equals the corresponding texel of GEOM's R16 tile |
| `Test_EnvironmentCook_HoleMaskConversion` | the 257 × 257 sample masks handed to GEOM mark exactly the samples of the authored hole quads (a single hole quad marks its four corners; a hole at a tile edge marks the shared border samples in both tiles) and the samples of every absent cell's quads (a present tile beside an absent one has 0 on the shared border and a non-empty span); a tile without a 0 sample passes an empty span and a null hole GUID |
| `Test_EnvironmentCook_AbsentCellsPassedDense` | a 3 × 3 fixture with the centre and a corner cell absent passes nine tiles in (z, x) order to `Zenith_CookTerrainGeometry`, which accepts them; the absent cells' samples equal the present neighbours' shared borders and `m_fHeightMin` elsewhere; section 0x0103 holds no null GUID |
| `Test_TerrainPreview_ChainEqualsColdCook` | in `ZenithCook_Tests` (which links the preview and the cook): after a RAISE stroke, a hole stroke beside an absent cell, a paint stroke and a density stroke, `Prepare` publishes height, hole, splat and detail-density chains whose every mip equals, byte for byte, the output of `Zenith_CookTerrainGeometry` and GEOM's texture-step chain rules for the edited document at the same height range; a clean channel publishes no chain |
| `Test_EnvironmentDocumentTypes_Registered` | `Zenith_RegisterEnvironmentDocumentTypes` into a fresh `Zenith_MigrationRegistry` registers the types of Bake.md "Environment document types" with exactly their descs; `FindTypeByPath` resolves one path per suffix (`x.terraintile.json` to the tile type, not the terrain type) and `FindType` each stable name; every canonical schema passes `Zenith_ValidateCanonicalSchema`; `Freeze` succeeds; a second call returns `DOCUMENT_ERROR_DUPLICATE_REGISTRATION`, and a call after `Freeze` `DOCUMENT_ERROR_REGISTRY_FROZEN` |
| `Test_EnvironmentDocumentTypes_MigrationFixtures` | every `v<N>.json` of the nine types migrates through a `Zenith_MemoryDocumentStorage` to the bytes of its `v<N>.expected.json`; a version without both files fails ([Z2-N-CONTRACT-549]) |
| `Test_EnvironmentDocumentTypes_TerrainSaveIsCanonical` | `Zenith_TerrainDocument::Save` of the fixture terrain, of a document with one unknown member per object kind and of a freshly created document writes bytes for which `Zenith_IsCanonicalJson` is true under the terrain and tile schemas; opening a saved document through the migrator and writing it with `Zenith_WriteCanonicalJson` reproduces the bytes |
| `Test_CloudNoise_GeneratorGoldenTexels`, `Test_CloudNoise_GeneratorDrawArguments` | the cases of Atmosphere.md "Cloud noise generator": the 16 golden texels under the draw `D ≡ 0.5`, and the recorded keys, seeds and draw indices |
| `Test_CloudNoise_CookDeterministicAndTileable` | each engine cloud-noise document cooks to identical bytes twice; the file's sizes match "Cloud noise file"; in every mip, the texels of opposite faces continue the noise across the wrap within the quantisation step; the reader refuses an extent, mip count or section size that disagrees |
| `Test_EnvironmentCook_TerrainMaterialsGenerated` | the fixture terrain cooks a `material:terrain` sub-asset whose 48 texture fields name the layer documents' textures and, from Stage 4 and only because its foliage-type list holds detail types, a `material:detail` sub-asset with the two detail types' constants; a fixture without a detail type declares no `material:detail` and section 0x0206's second GUID is zero; the sub-asset GUIDs are stable across two cooks |
| `Test_EnvironmentCook_WaterMaterialGenerated` | each water document cooks its `material:water` sub-asset with the document's optical parameters and textures, and the `.zwater` section 0x0601 names it |
| `Test_EnvironmentCook_BorderMismatchFails` | a mismatched border fails the import with `COOK_ERROR_IMPORT_FAILED` (detail `IMPORT_ERROR_SOURCE_INVALID`) and one ERROR diagnostic beginning `ENV_ERROR_TILE_BORDER_MISMATCH` and naming the tile (Bake.md "Cook pipeline") |
| `Test_EnvironmentCook_CellAlignmentChecked` | a STREAM cell size of 200 m fails the import with `COOK_ERROR_IMPORT_FAILED` and one ERROR diagnostic beginning `ENV_ERROR_INVALID_DESC` and naming the cell size and origin |
| `Test_EnvironmentCook_ParameterDocumentsRoundTrip` | each of atmosphere, weather and water cooks and reads back to the same runtime values |
| `Test_EnvironmentReaders_RejectMismatchedVersion` | each ENV reader refuses a file of another format or engine cook version with the recoverable asset error ([Z2-N-CONTRACT-528]) |
| `Test_EnvironmentReaders_RejectOverLimitCounts` | counts one above each maximum of "Reader validation" fail with the section index |
| `Test_EnvironmentCook_CollisionAndNavExtraction` | the fixture's collision tile and navigation geometry exclude holes, carry the layer materials and area codes, and mark water |

## Render and parity tests

Files under `Zenith2/Engine/ZenithFlux/Tests/Environment/`, run on WARP and lavapipe with validation clean. No ENV render test compares against a committed golden (ruling Q55): each `…CrossBackend` test renders its scene (`Tests/Environment/Scenes/<Scene>/`, whose source documents the generated fixture set cooks) on both devices in one run through RPROD's parity runner (`Zenith2/Docs/Design/RPROD/Tests.md` → "Parity suites"), requires the surface-data channels near-exact and the final images within the FLIP mean of `Z2-B-ENV-013` Environment render test tolerances between the two backends, and checks each capture against the scene's own oracle evaluated on the captured pixels, so a defect shared by both backends still fails.

| Test ID | Asserts |
|---|---|
| `Test_EnvRender_TerrainSplatCrossBackend` | the `TerrainSplat` scene (the fixture terrain, whose four layers use constant textures, a fixed camera on a four-layer boundary and a hole) agrees between the backends as above; on each, the albedo channel at every terrain pixel equals, within 2 UNORM8 quanta, the CPU top-four blend (`Zenith_ComputeTerrainSplatImages` and steps 1–3 of "Layer blend in the terrain material") of the layers' constant albedos at the depth-reconstructed world position, and every pixel inside the hole's projected footprint has background depth |
| `Test_EnvRender_TerrainPreviewSwap` | a preview tile update replaces the tile's whole height chain: a GPU readback of every mip equals the published chain, the next frame renders the new heights with no crack against the neighbouring tiles at any level, and `UpdateNodeBounds` receives every node run in the frame the height replacement publishes and in no earlier frame; with a real `Zenith_MemoryGovernor` whose transition-peak budget is below the height chain's bytes (`ReserveTransitionPeak` refuses) the refused chain stays pending per (terrain, tile, chain) and publishes once the budget allows it; a height chain refused and then a splat-only update of the same tile both publish, no edit lost; `Flux.Environment.PreviewPendingBytes` counts the retained bytes and returns to 0 |
| `Test_EnvRender_FoliageInstancesCulled` | 10,000 foliage instances draw through indirect commands; a fully occluded cluster records zero draws |
| `Test_EnvRender_DetailFoliageCrossBackend` | the `DetailFoliage` scene (one detail cell, wind frozen) agrees between the backends as above; on each, the depth channel is covered at the projection of every blade the CPU reference of "Draw-time placement" keeps and at no pixel more than 1 pixel from one |
| `Test_DetailFoliage_DrawTimePlacementMatchesCpu` | a compute harness evaluating the detail material's vertex modifier at 4,096 blades equals a CPU reference of "Draw-time placement" (kept set exactly, base heights within 1e-4 m, wind offsets from the type's wind block with the weights (t, 0, 0) and the phase of draw 7 within 1e-4 m, none longer than 0.5 m) |
| `Test_DetailFoliage_DensityEditWithoutRegeneration` | painting density on a tile with resident detail cells changes the drawn blades in the next frame while no `ProceduralGeometry` dispatch runs |
| `Test_EnvRender_WaterSurfaceCrossBackend` | the `WaterLake` and `WaterOcean` scenes agree between the backends as above; on each, at 1,024 sampled water pixels the depth-reconstructed position lies on the CPU `Zenith_SampleGerstnerSurface` surface within the displacement row of "Water error budget"; the water nodes' instance count follows node selection; the T-junction case of WaterWeather.md "Water node selection": a camera 2 m above the ocean looking at the root square's boundary 2,048 m away, with everything below the surface cleared to a key colour and the water drawn opaque, shows the key colour in at most 0.01% of the pixels within 2 pixels of the projected boundary line on both backends |
| `Test_EnvRender_UnderwaterComposite` | a submerged camera applies the medium; an above-water camera adds no pass |
| `Test_EnvRender_CloudLayerCrossBackend` | the `CloudLayer` scene (fixed weather and time) agrees between the backends as above; on each, the cloud transmittance target at 256 sky-view texels equals the 256-step CPU integrator of `Test_Clouds_LayerMatchesReference` within `Z2-B-ENV-009` Cloud layer approximation error, and the sky pixels behind zero coverage equal the cloudless sky |
| `Test_EnvRender_BackendParity` | the environment parity scene (terrain, foliage, water, clouds, fixed time and weather) passes BackendParity's comparison |
| `Test_EnvRender_ShaderLintsClean` | the ENV Slang modules pass `ShaderChecks` (explicit gradients, discard only in `[AlphaTest]` variants, half ratio reported) |

## Error-budget tests

The rows of `Zenith2/Docs/Design/ENV/WaterWeather.md → "Water error budget"` and `Zenith2/Docs/Design/ENV/Atmosphere.md → "Cloud error budget"`: `Test_Water_GpuDisplacementMatchesCpu`, `Test_Water_HeightQuery_ResidualBelow1mm`, `Test_Water_SchlickWithinBudget`, `Test_Water_TransmittanceHalfPrecision`, `Test_Water_NormalMatchesFiniteDifference`, `Test_Water_FftFitWithinBudget` (Stage 5), `Test_Underwater_CompositeMatchesReference`, `Test_Clouds_LayerMatchesReference`, `Test_Clouds_VolumetricMatchesHighStepReference` (Stage 5), `Test_Clouds_TemporallyStable`, `Test_Clouds_ZeroCoverageAddsNothing`. They live in `Zenith2/Engine/ZenithFlux/Tests/Environment/Env_ErrorBudget_Tests.cpp` (GPU cases) and `Zenith2/Engine/ZenithEnvironment/Tests/Zenith_WaterSystem_Tests.cpp` (CPU cases).

## Fuzz targets

`Zenith2/Engine/ZenithEnvironment/Tests/Fuzz/`: `Fuzz_TerrainDocument.cpp` (the terrain and tile JSON through `Load` with a memory tile source), `Fuzz_EnvironmentDocuments.cpp` (foliage, placement, atmosphere, weather, water, terrain layer and cloud-noise JSON readers), `Fuzz_TerrainTileFile.cpp`, `Fuzz_FoliageCellFile.cpp`, `Fuzz_EnvironmentParameterFiles.cpp` (the three parameter files), `Fuzz_CloudNoiseFile.cpp`. Each asserts no crash, no sanitizer report and a `Zenith_Result` error for every rejected input.

## Stress and sanitizer tests

| Test ID | Suite | Asserts |
|---|---|---|
| `Test_EnvironmentCells_LoadUnload1000Cycles_NoLeak` | `z2-stress` | as in "Cell and runtime tests" ([Z2-N-CONTRACT-339]) |
| `Test_TerrainBrush_LongStroke_Stress` | `z2-stress` | a 65,536-dab stroke over 16 tiles completes with bounded edit memory and a correct undo |
| `Test_Placement_MaxDensity_Stress` | `z2-stress` | one cell at 64 per m² completes within the CTest timeout with the capacity error path exercised |
| `Test_EnvironmentModule_Create_FailsAtRegisterTypes1_Unwinds`, `…RegisterTypes2_Unwinds`, `…Start1_Unwinds`, `…Start2_Unwinds`, `…Start3_Unwinds`, `…Start4_Unwinds`, `…AttachWorld1_Unwinds`, `…AttachWorld2_Unwinds`, `Test_EnvironmentModule_Create_FailsAtAttachWorld3_Unwinds` | default | one per step of the entry-point table (`Zenith2/Docs/Design/ENV.md → "Zenith_Module_Environment header"`): failing that step alone returns its module error and leaves no leak, no registration, no configuration callback and no thread |

The ZenithEnvironment tests run under `win-asan` through the `Sanitizers` step when the diff touches the module's paths (`Zenith2Memory` does not list modules; the ASan run comes from the stage acceptance's full sanitizer set, [Z2-N-ARCH-173]).

## Workload and device evidence

| Evidence | Run by | Asserts |
|---|---|---|
| W-Outdoor v1 measured per `Zenith2/Docs/Budgets.md → "W-Outdoor"` on the reference desktop (both backends) and the S24 | the Stage 3 and Stage 4 acceptance (needs-human device steps) | `Z2-B-ENV-001` and the ENV rows allocated by the Stage 1 per-system budget ticket for ENV hold; the frame-time rows hold with 100k foliage instances resident and the 10 m/s traversal streaming tiles and cells |
| the teleport and rapid-traversal runs of MP-STREAM in W-Outdoor | Stage 4 acceptance | no hitch beyond the hitch rows when foliage cells activate; `environment.foliage.activation_backlog` drains within the frames the pending row "Foliage activation catch-up frames" allows |
| the S24 sustained run of W-Outdoor (WS-SUSTAIN) | Stage 6 acceptance | thermal step-downs reduce foliage density and cloud tier only through the quality tier, and simulation hashes match an unthrottled run |
| the OpenWorldSlice day/night review | the flagship sign-off ([Z2-N-BUDGET-433]) | lighting and atmosphere over a day/night transition, reviewed by a person (a `needs-human` acceptance item) |

## Gate steps per test group

| Group | Step |
|---|---|
| unit, integration, cook, determinism, error-budget, render tests | `Test` (render tests on WARP and lavapipe) |
| render parity scene | `BackendParity` (from Stage 3; ENV's scene from Stage 4) and `PathParity` (Stage 5, terrain and foliage in the Deferred and visibility-buffer paths) |
| shader lints | `ShaderChecks` |
| fuzz smoke | `Test` in `win-asan` (fuzz executables' smoke runs) |
| public headers | `ApiContractCheck`, `HeaderSelfContained` |
| layering | `Libgraph` |
| workload and device evidence | stage acceptance through `Run-Committed.ps1 -Acceptance` (Z2-D-083) |
