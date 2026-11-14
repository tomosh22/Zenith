# ENV requirement catalogue: Stages 4 to 6

## Purpose

The second file of the ENV catalogue: the requirements whose `Stage` is S4, S5 or S6, in the same format and the same `Z2-R-ENV-NNN` numbering as `Zenith2/Docs/Requirements/ENV.md`, which holds the purpose of the whole catalogue, the Stage 1 and Stage 3 requirements, the coverage of inventory items (over both files) and the exclusions.

## Requirements

### Z2-R-ENV-017 Terrain documents migrate

- **Statement:** The terrain, tile and terrain layer documents carry a schema version and a tool-side migration chain with one step per version and a migration fixture per version, preserve unknown members at their canonical anchors with one warning each, and are written by `Zenith_TerrainDocument::Save` in the canonical form of their registered schemas through the journaled atomic write.
- **Covers:** Z2-I-ENV-01, Z2-I-ENV-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EnvironmentDocumentTypes_MigrationFixtures` and `Test_EnvironmentDocumentTypes_TerrainSaveIsCanonical` in `ZenithCook_Tests`, `Test_TerrainDocument_UnknownFieldPreserved`; the journaled-write tests of the framework over ENV documents.
- **Design:** Zenith2/Docs/Design/ENV.md → "Migration and re-cook"; Zenith2/Docs/Design/ENV/Bake.md → "Authoring document schemas"; Zenith2/Docs/Design/ENV/Bake.md → "Environment document types"

### Z2-R-ENV-025 Thermal erosion brush

- **Statement:** ERODE_THERMAL runs the stated Jacobi iterations, reduces slopes above the talus angle and conserves total height within 1e-3 m.
- **Covers:** Z2-I-ENV-01
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_TerrainBrush_ErodeThermal_ReducesSlopes`.
- **Design:** Zenith2/Docs/Design/ENV/Terrain.md → "Brush kinds"

### Z2-R-ENV-028 Paint brushes

- **Statement:** PAINT_LAYER, ERASE_LAYER, the hole, density and exclusion brushes compute the rules of "Brush kinds", and an erase that leaves a sparse map all zero drops the map.
- **Covers:** Z2-I-ENV-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_TerrainBrush_Hole_PaintErase`, `Test_TerrainBrush_EraseToZero_DropsMap`.
- **Design:** Zenith2/Docs/Design/ENV/Terrain.md → "Brush kinds"

### Z2-R-ENV-029 Weight normalisation

- **Statement:** After every paint, erase or layer removal each texel's weights sum to exactly 255 by the integer algorithm of "Weight normalisation".
- **Covers:** Z2-I-ENV-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_TerrainBrush_PaintLayer_WeightsSumTo255`, `Test_TerrainDocument_RemoveLayer_WeightRedistributed`.
- **Design:** Zenith2/Docs/Design/ENV/Terrain.md → "Weight normalisation"

### Z2-R-ENV-036 Sculpt preview latency

- **Statement:** The time from the end of a dab's `ApplyStroke` to the first presented frame showing it stays within the pending editor row "Terrain sculpt preview latency".
- **Covers:** Z2-I-ENV-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** pending row "Terrain sculpt preview latency"
- **Acceptance evidence:** The W-Editor measurement of the row on the reference desktop, recorded by EDIT's W-Editor row ticket (needs-human device step).
- **Design:** Zenith2/Docs/Design/ENV/Terrain.md → "Live preview"; Zenith2/Docs/Design/ENV.md → "Pending row needs"

### Z2-R-ENV-039 Height-based blend

- **Statement:** Layer weights are adjusted by the height-based blend of "Height-based blend", and a blend depth of 1 reproduces the linear blend.
- **Covers:** Z2-I-ENV-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_TerrainMaterial_HeightBlend`.
- **Design:** Zenith2/Docs/Design/ENV/Terrain.md → "Height-based blend"

### Z2-R-ENV-042 Terrain splat render golden

- **Statement:** The fixture terrain with four layers renders on WARP and lavapipe in one run with the two captures within `Z2-B-ENV-013` Environment render test tolerances of each other and each capture's albedo channel equal to the CPU top-four blend of the layers at the depth-reconstructed positions; no committed golden is compared (ruling Q55).
- **Covers:** Z2-I-ENV-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-063
- **Budgets:** pending row "Environment render test tolerances"
- **Acceptance evidence:** `Test_EnvRender_TerrainSplatCrossBackend` on WARP and lavapipe.
- **Design:** Zenith2/Docs/Design/ENV/Tests.md → "Render and parity tests"

### Z2-R-ENV-048 Physical material and hole queries

- **Statement:** A query returns the quad's physical material from its dominant authored layer and flags points over holes with the triangle height.
- **Covers:** Z2-I-ENV-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_TerrainRuntime_PhysicalMaterialDominantLayer`, `Test_TerrainRuntime_HoleFlag`.
- **Design:** Zenith2/Docs/Design/ENV/Terrain.md → "Physical material queries"; Zenith2/Docs/Design/ENV/Terrain.md → "Holes in queries"

### Z2-R-ENV-049 Terrain raycast

- **Statement:** `Zenith_TerrainWorldResource::Raycast` walks the crossed quads with a 2D DDA, intersects each quad's two triangles of the fixed diagonal, passes through holes and absent tiles, returns `ENV_ERROR_NOT_RESIDENT` when the ray reaches an inactive tile first and `ENV_ERROR_CAPACITY` when `uMaxSteps` quads pass without a hit.
- **Covers:** Z2-I-ENV-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_TerrainRuntime_Raycast`.
- **Design:** Zenith2/Docs/Design/ENV/Terrain.md → "Terrain raycast"; Zenith2/Docs/Design/ENV/Terrain.md → "Zenith_TerrainRuntime header"

### Z2-R-ENV-063 Detail foliage generation

- **Statement:** Detail foliage is procedural geometry of GEOM's: `Flux_DetailFoliageSource` implements `Zenith_ProceduralGeometrySource` (`GetDesc`, `ComputeSizing`, `WriteIndices`), registers through `RegisterSource` when ENV's GPU side is created, and the foliage extractor emits one `Flux_RenderProceduralInstance` and instance delta per (detail cell, detail type, block), nearest the anchor first and at most `environment.foliage.detailCellsPerFrame` per frame; ENV never allocates or writes the pool itself.
- **Covers:** Z2-I-ENV-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-058
- **Budgets:** Z2-B-GEOM-005
- **Acceptance evidence:** `Test_DetailFoliage_SourceSizingAndIndices`, `Test_DetailFoliage_ExtractorEmitsProceduralRecords` and `Test_EnvRender_DetailFoliageCrossBackend`.
- **Design:** Zenith2/Docs/Design/ENV/Foliage.md → "Detail source"; Zenith2/Docs/Design/ENV/Foliage.md → "Detail cells and lattice"; Zenith2/Docs/Design/ENV/Foliage.md → "Blade and card geometry"

### Z2-R-ENV-064 Detail foliage hash is stable

- **Statement:** The generator and the detail material evaluate the same hash of (terrain seed, detail type, detail cell, lattice point, draw), so a regenerated cell is identical, placement never depends on frame or dispatch order, and a lower density keeps a subset of the higher density's blades.
- **Covers:** Z2-I-ENV-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-014
- **Budgets:** none
- **Acceptance evidence:** `Test_DetailFoliage_HashStableAcrossFrames`, `Test_DetailFoliage_LowerDensityIsSubset`.
- **Design:** Zenith2/Docs/Design/ENV/Foliage.md → "Generator hash"; Zenith2/Docs/Design/ENV/Foliage.md → "Draw-time placement"

### Z2-R-ENV-065 Detail foliage tiers

- **Statement:** The detail outer and near radii, lattice caps, density scale and vertex cap follow the tier table of "Detail foliage tiers" through the `environment.foliage.*` keys, blades in the near ring and cards beyond, with a ring or cap change applied as a topology change.
- **Covers:** Z2-I-ENV-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-014, Z2-D-035
- **Budgets:** none
- **Acceptance evidence:** `Test_DetailFoliage_TierSettings` (keys to generator parameters) and the W-Outdoor S24 run.
- **Design:** Zenith2/Docs/Design/ENV/Foliage.md → "Detail foliage tiers"; Zenith2/Docs/Design/ENV/Foliage.md → "Tier thinning rule"

### Z2-R-ENV-066 Deterministic wind state

- **Statement:** The wind state is advanced each tick in phase 1 from the blended weather and the wind stream with explicitly set counters, so the state at tick N is a pure function of the seed, the weather history and N.
- **Covers:** Z2-I-ENV-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Wind_StateDeterministicPerTick`, `Test_Wind_WanderBounded`.
- **Design:** Zenith2/Docs/Design/ENV/Foliage.md → "Global wind state"; Zenith2/Docs/Design/ENV.md → "Zenith_Wind header"

### Z2-R-ENV-067 One shared wind function

- **Statement:** `Env_WindSample` and `Env_FoliageWindOffset` in `Zenith2/Shaders/Environment/Wind.slang` are the single source of wind displacement for every pass that positions a foliage vertex, including shadow views, and the trunk bend preserves distance from the pivot.
- **Covers:** Z2-I-ENV-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-015, Z2-D-014
- **Budgets:** none
- **Acceptance evidence:** `Test_FoliageWind_ShadowMatchesMainDepth`, `Test_FoliageWind_LengthPreservingBend`.
- **Design:** Zenith2/Docs/Design/ENV/Foliage.md → "Env_WindSample"; Zenith2/Docs/Design/ENV/Foliage.md → "Env_FoliageWindOffset"; Zenith2/Docs/Design/ENV/Foliage.md → "Shadow and depth pass agreement"

### Z2-R-ENV-068 Wind CPU mirror

- **Statement:** `Zenith_EvaluateWindCPU` matches `Env_WindSample` within 1e-3 × max(1, |v|) m/s on the test grid, and no simulation state depends on the GPU value.
- **Covers:** Z2-I-ENV-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Wind_GpuMatchesCpuMirror`.
- **Design:** Zenith2/Docs/Design/ENV/Foliage.md → "CPU mirror and tolerance"

### Z2-R-ENV-069 Wind time base

- **Statement:** The wind time is the frame's interpolated simulation time reduced modulo 3,600 s in double precision before it is written as float, and the previous frame's time is supplied for velocity.
- **Covers:** Z2-I-ENV-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_Wind_TimeReductionContinuous`.
- **Design:** Zenith2/Docs/Design/ENV/Foliage.md → "Env_WindSample"

### Z2-R-ENV-070 Foliage vertex modifier and velocity

- **Statement:** `Material_Foliage` and `Material_DetailFoliage` apply the wind through the material vertex-modifier hook, whose input carries UV0 and the frame's `Env_FrameEnvironment`, for the current and previous frame times, so displaced leaves and blades write correct motion vectors.
- **Covers:** Z2-I-ENV-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-014, Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_FoliageWind_VelocityFromPreviousTime`. Pending core-doc batch 4 (X127 (1)): the vertex-modifier hook with UV0 and the frame environment in its input.
- **Design:** Zenith2/Docs/Design/ENV/Foliage.md → "Material_Foliage"; Zenith2/Docs/Design/ENV/Foliage.md → "Previous-frame evaluation for velocity"

### Z2-R-ENV-071 Foliage material GPU rules

- **Statement:** Leaf sections are alpha-tested (drawn after opaque in depth passes) and two-sided by their material documents' flags, trunks and rocks are opaque with back-face culling, and every sample uses explicit gradients.
- **Covers:** Z2-I-ENV-03, Z2-I-ENV-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-014
- **Budgets:** none
- **Acceptance evidence:** `Test_EnvRender_ShaderLintsClean` and the `PSO.OpaqueCullBack` lint over the foliage PSOs.
- **Design:** Zenith2/Docs/Design/ENV/Foliage.md → "Material_Foliage"

### Z2-R-ENV-072 Local wind sources

- **Statement:** Up to 16 `Zenith_WindSourceComponent` sources nearest the main view add bounded radial and directional wind contributions to the render wind block and the CPU mirror.
- **Covers:** Z2-I-ENV-04
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Wind_LocalSourcesBounded` (Stage 5) in `ZenithEnvironment_Tests`.
- **Design:** Zenith2/Docs/Design/ENV/Foliage.md → "Local wind sources"

### Z2-R-ENV-073 Water bodies

- **Statement:** Water bodies are entities with `Zenith_WaterBodyComponent` of kind ocean (at most one per world), lake (a simple counter-clockwise polygon of at most 256 vertices) or river (Stage 5), each naming a water document.
- **Covers:** Z2-I-ENV-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-020
- **Budgets:** none
- **Acceptance evidence:** `Test_Water_LakePolygonContainment`, `Test_EnvironmentComponents_Registration`.
- **Design:** Zenith2/Docs/Design/ENV/WaterWeather.md → "Water body kinds"

### Z2-R-ENV-074 Water document

- **Statement:** The `*.water.json` document has the members and ranges of "Water document", migrates like every ENV document, and cooks to `.zwater`.
- **Covers:** Z2-I-ENV-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EnvironmentCook_ParameterDocumentsRoundTrip`, the water migration fixtures, `Fuzz_EnvironmentDocuments`.
- **Design:** Zenith2/Docs/Design/ENV/WaterWeather.md → "Water document"; Zenith2/Docs/Design/ENV/Bake.md → "Water file"

### Z2-R-ENV-075 Gerstner synthesis

- **Statement:** Water displacement, normal and velocity are the Gerstner sum of "Gerstner sum" with deep-water dispersion, evaluated in wave-index order with phases reduced in double precision.
- **Covers:** Z2-I-ENV-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Water_Gerstner_KnownValues`.
- **Design:** Zenith2/Docs/Design/ENV/WaterWeather.md → "Gerstner sum"; Zenith2/Docs/Design/ENV/WaterWeather.md → "Time base"

### Z2-R-ENV-076 Wave set validation

- **Statement:** A wave set with more than 8 waves, a non-finite value, a wavelength outside [0.5, 1,000] m, a non-unit direction or a folding sum is refused with `ENV_ERROR_WAVESET_INVALID` and the wave index.
- **Covers:** Z2-I-ENV-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_Water_ValidateWaveSet_Rejections`.
- **Design:** Zenith2/Docs/Design/ENV/WaterWeather.md → "Wave set validation"

### Z2-R-ENV-077 Deterministic water height query

- **Statement:** The CPU water height query uses four fixed-point iterations at simulation time and returns bit-identical results on every worker of one build, with a horizontal residual below 1 mm for validated sets.
- **Covers:** Z2-I-ENV-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Water_HeightQuery_ResidualBelow1mm`, `Test_Water_Sample_DeterministicAcrossWorkers`.
- **Design:** Zenith2/Docs/Design/ENV/WaterWeather.md → "Height query by fixed-point iteration"; Zenith2/Docs/Design/ENV/WaterWeather.md → "Zenith_WaterSystem header"

### Z2-R-ENV-078 Wind-driven wave amplitude

- **Statement:** A wind-driven water body scales its amplitudes by min(2, (speed / 8 m/s)²) of the tick's wind, applied in phase 1.
- **Covers:** Z2-I-ENV-05, Z2-I-ENV-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Water_WindDrivenAmplitude`.
- **Design:** Zenith2/Docs/Design/ENV/WaterWeather.md → "Water document"

### Z2-R-ENV-079 Water queries out of bounds

- **Statement:** A water query where no body covers the point returns `ENV_ERROR_OUT_OF_BOUNDS`, and `IsSubmerged` classifies points against the displaced surface.
- **Covers:** Z2-I-ENV-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_Water_OutOfBounds`, `Test_Water_Submersion`.
- **Design:** Zenith2/Docs/Design/ENV/WaterWeather.md → "Zenith_WaterSystem header"

### Z2-R-ENV-080 Water surface rendering

- **Statement:** Water surfaces are GPU-scene instances of the `WaterGrid` mesh selected by GEOM's quadtree rule with LOD distance ratio 8 and morphing, so adjacent nodes meet without cracks, drawn with the body's transparent-domain two-sided water material in the transparent forward pass with refraction, clustered lighting and fog.
- **Covers:** Z2-I-ENV-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-058, Z2-D-014
- **Budgets:** Z2-B-RPROD-010
- **Acceptance evidence:** `Test_EnvRender_WaterSurfaceCrossBackend`, `Test_Water_NodeSelectionCrackFree`.
- **Design:** Zenith2/Docs/Design/ENV/WaterWeather.md → "Water node selection"; Zenith2/Docs/Design/ENV/WaterWeather.md → "Material_Water"

### Z2-R-ENV-081 Water optical model

- **Statement:** Water shading uses Beer–Lambert transmittance, single scattering, Schlick reflectance from the index of refraction, and depth-faded shorelines with foam as "Optical model" and "Shoreline and depth fade" define.
- **Covers:** Z2-I-ENV-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-063
- **Budgets:** none
- **Acceptance evidence:** `Test_Water_SchlickWithinBudget`, `Test_Water_TransmittanceHalfPrecision`, `Test_EnvRender_WaterSurfaceCrossBackend`.
- **Design:** Zenith2/Docs/Design/ENV/WaterWeather.md → "Optical model"; Zenith2/Docs/Design/ENV/WaterWeather.md → "Shoreline and depth fade"

### Z2-R-ENV-082 Water reflections by tier

- **Statement:** Water reflects the sky-view LUT and IBL on S24 and desktop-low tiers, and from Stage 5 a round-robined half-resolution planar reflection view on desktop high for the ocean or the largest visible lake.
- **Covers:** Z2-I-ENV-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-059
- **Budgets:** none
- **Acceptance evidence:** `Test_EnvRender_PlanarReflectionView` (Stage 5): the view is requested within the view budget and sampled.
- **Design:** Zenith2/Docs/Design/ENV/WaterWeather.md → "Reflections by tier"

### Z2-R-ENV-083 Underwater rendering

- **Statement:** A submerged view gets one `Flux_RenderEnvironmentView` record whose payload is LIGHT's `Flux_ViewMediumOverride`, keyed by the view ID RPROD's camera extractor computes for the request and evaluated at the water resource's current tick, at most 16 per world nearest the anchor (each further submerged view counted and drawn without the medium); a view above water gets none; the underwater composite pass runs after transparents and particles with a per-pixel waterline split near the surface, and no pass is added when no view is submerged.
- **Covers:** Z2-I-ENV-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-014, Z2-D-063
- **Budgets:** Z2-B-LIGHT-004; pending row "Water surface and underwater GPU"; pending row "Underwater composite error"
- **Acceptance evidence:** `Test_Water_SubmergedViewRecordsCapped`, `Test_EnvRender_UnderwaterComposite`, `Test_Underwater_CompositeMatchesReference`. The view records travel through core-doc item X156 (pending core-doc batch 4); LIGHT copies their payloads into `Flux_LightingFrame::m_axViewMediumOverrides` (rows EN17, EN23).
- **Design:** Zenith2/Docs/Design/ENV/WaterWeather.md → "Submersion test"; Zenith2/Docs/Design/ENV/WaterWeather.md → "Underwater composite pass"; Zenith2/Docs/Design/ENV/WaterWeather.md → "Underwater medium"

### Z2-R-ENV-084 Water error budget

- **Statement:** Water is validated against the documented budget of "Water error budget" on the water validation scenes, never against the path tracer.
- **Covers:** Z2-I-ENV-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-063
- **Budgets:** pending row "Underwater composite error"
- **Acceptance evidence:** `Test_Water_GpuDisplacementMatchesCpu`, `Test_Water_NormalMatchesFiniteDifference`, `Test_Water_SchlickWithinBudget`, `Test_Water_TransmittanceHalfPrecision`, `Test_Underwater_CompositeMatchesReference`.
- **Design:** Zenith2/Docs/Design/ENV/WaterWeather.md → "Water error budget"; Zenith2/Docs/Design/ENV/Tests.md → "Error-budget tests"

### Z2-R-ENV-085 FFT ocean tier

- **Statement:** On desktop high from Stage 5 the ocean is displaced by a JONSWAP inverse-FFT tier while gameplay queries its fitted Gerstner set, within the pending row "Ocean FFT to query-set height error".
- **Covers:** Z2-I-ENV-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-063
- **Budgets:** pending row "Ocean FFT to query-set height error"
- **Acceptance evidence:** `Test_Water_FftFitWithinBudget`.
- **Design:** Zenith2/Docs/Design/ENV/WaterWeather.md → "FFT ocean tier"

### Z2-R-ENV-086 Rivers

- **Statement:** From Stage 5 river splines cook to a strip mesh and flow map, render with two-phase flow-mapped normals, and answer the CPU query with the strip height and flow velocity.
- **Covers:** Z2-I-ENV-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_Water_RiverQuery` and `Test_EnvRender_RiverCrossBackend` (Stage 5).
- **Design:** Zenith2/Docs/Design/ENV/WaterWeather.md → "River flow"

### Z2-R-ENV-087 Caustics

- **Statement:** From Stage 5 on desktop tiers, caustics are projected onto surfaces under water within 20 m of depth, attenuated by the column transmittance.
- **Covers:** Z2-I-ENV-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-063
- **Budgets:** none
- **Acceptance evidence:** `Test_EnvRender_CausticsCrossBackend` (Stage 5).
- **Design:** Zenith2/Docs/Design/ENV/WaterWeather.md → "Caustics"

### Z2-R-ENV-091 Weather modulates the atmosphere

- **Statement:** `Zenith_ApplyWeatherToAtmosphere` scales haze and fog, whitens fog under cloud cover and raises ground albedo with snow exactly as "Weather modulation of the atmosphere" defines.
- **Covers:** Z2-I-ENV-06, Z2-I-ENV-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-063
- **Budgets:** none
- **Acceptance evidence:** `Test_Atmosphere_WeatherModulation`.
- **Design:** Zenith2/Docs/Design/ENV/Atmosphere.md → "Weather modulation of the atmosphere"

### Z2-R-ENV-092 Sky and atmosphere GPU row in the flagship workloads

- **Statement:** In W-OpenWorld and W-Cinematic, whose first playable milestones are Stage 4 deliverables, LIGHT's sky passes and ENV's cloud layer pass of each view stay within `Z2-B-ENV-001` per platform and path, measured with MP-GPUPASS; W-Outdoor's measurement of the row is Z2-R-ENV-150's at Stage 3 and Z2-R-ENV-151's at Stage 4.
- **Covers:** Z2-I-ENV-06, Z2-I-ENV-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-ENV-001
- **Acceptance evidence:** The W-OpenWorld and W-Cinematic measurements of the Stage 4 acceptance (evidence records per `Zenith2/Docs/Budgets.md`), after the Stage 1 per-system budget ticket for ENV allocates the row.
- **Design:** Zenith2/Docs/Design/ENV.md → "Rows owned"

### Z2-R-ENV-100 Day and night review

- **Statement:** OpenWorldSlice's lighting and atmosphere over a day/night transition pass the flagship sign-off of [Z2-N-BUDGET-433].
- **Covers:** Z2-I-ENV-09, Z2-I-ENV-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-021
- **Budgets:** none
- **Acceptance evidence:** The OpenWorldSlice flagship acceptance sign-off record (needs-human acceptance item).
- **Design:** Zenith2/Docs/Design/ENV/Tests.md → "Workload and device evidence"

### Z2-R-ENV-101 Cloud model

- **Statement:** Cloud density and lighting follow "Cloud model", with coverage and density from the blended weather and shape from ENV's cooked cloud-noise volumes scrolled by the wind.
- **Covers:** Z2-I-ENV-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-063
- **Budgets:** pending row "Cloud layer approximation error"
- **Acceptance evidence:** `Test_Clouds_LayerMatchesReference`.
- **Design:** Zenith2/Docs/Design/ENV/Atmosphere.md → "Cloud model"

### Z2-R-ENV-102 Cloud layer on every tier

- **Statement:** The cloud layer pass runs once per frame per world with 8 steps on S24 tiers and 16 on desktop tiers, writes sky-parameterised radiance and transmittance into `Flux_FrameShared::m_xCloudLayer` and `m_xCloudTransmittance`, is composited by LIGHT's sky draw, and adds no pass at zero coverage.
- **Covers:** Z2-I-ENV-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-014, Z2-D-035
- **Budgets:** Z2-B-ENV-001
- **Acceptance evidence:** `Test_EnvRender_CloudLayerCrossBackend`, `Test_Clouds_ZeroCoverageAddsNothing`.
- **Design:** Zenith2/Docs/Design/ENV/Atmosphere.md → "Cloud layer pass"; Zenith2/Docs/Design/ENV/Atmosphere.md → "Cloud quality tiers"

### Z2-R-ENV-103 Cloud tier selection and S24 fallback

- **Statement:** `environment.clouds.tier` selects the technique of "Cloud quality tiers", the S24 uses the 8-step layer, and a disabled or unsupported tier falls back as the table states.
- **Covers:** Z2-I-ENV-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-035, Z2-D-014
- **Budgets:** none
- **Acceptance evidence:** `Test_Clouds_TierFallback` in `ZenithFlux_Tests` and the W-Outdoor S24 run.
- **Design:** Zenith2/Docs/Design/ENV/Atmosphere.md → "Cloud quality tiers"

### Z2-R-ENV-104 Cloud error budget

- **Statement:** The cloud layer and the Stage 5 volumetric clouds are validated against the documented budget of "Cloud error budget", including temporal stability.
- **Covers:** Z2-I-ENV-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-063
- **Budgets:** pending row "Cloud layer approximation error"; pending row "Volumetric cloud approximation error"
- **Acceptance evidence:** `Test_Clouds_LayerMatchesReference`, `Test_Clouds_TemporallyStable`, `Test_Clouds_ZeroCoverageAddsNothing`.
- **Design:** Zenith2/Docs/Design/ENV/Atmosphere.md → "Cloud error budget"

### Z2-R-ENV-105 Volumetric clouds

- **Statement:** From Stage 5 on desktop high, volumetric clouds raymarch at quarter resolution with temporal reprojection in a per-view history and occlude the sun disc, within the pending row "Volumetric cloud approximation error".
- **Covers:** Z2-I-ENV-07
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-059, Z2-D-063
- **Budgets:** Z2-B-ENV-001; pending row "Volumetric cloud approximation error"
- **Acceptance evidence:** `Test_Clouds_VolumetricMatchesHighStepReference`.
- **Design:** Zenith2/Docs/Design/ENV/Atmosphere.md → "Volumetric clouds"

### Z2-R-ENV-106 Cloud shadows

- **Statement:** From Stage 5 a cloud-shadow map over 4 km around the camera, moved in 64 m steps, attenuates the sun on desktop and S24 high tiers.
- **Covers:** Z2-I-ENV-07
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-063
- **Budgets:** none
- **Acceptance evidence:** `Test_EnvRender_CloudShadowsCrossBackend` (Stage 5).
- **Design:** Zenith2/Docs/Design/ENV/Atmosphere.md → "Cloud shadows"

### Z2-R-ENV-107 Weather document

- **Statement:** The `*.weather.json` document has the states, parameters, effects and transitions of "Weather document" and "Weather state parameters", and the cook refuses an invalid table.
- **Covers:** Z2-I-ENV-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_Weather_TableValidation`, the weather migration fixtures, `Fuzz_EnvironmentDocuments`.
- **Design:** Zenith2/Docs/Design/ENV/WaterWeather.md → "Weather document"; Zenith2/Docs/Design/ENV/WaterWeather.md → "Weather state parameters"

### Z2-R-ENV-108 Weather state machine table

- **Statement:** The weather machine realises exactly the transition table of "Transition table" for every (state, event) cell, handling events in the stated order within a tick.
- **Covers:** Z2-I-ENV-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Weather_TransitionTable`.
- **Design:** Zenith2/Docs/Design/ENV/WaterWeather.md → "States and events"; Zenith2/Docs/Design/ENV/WaterWeather.md → "Transition table"

### Z2-R-ENV-109 Deterministic weather draws

- **Statement:** Dwell and successor draws come from the weather stream with a saved counter, only on dwell end, transition end and release, so the weather sequence is a pure function of the table, seed and commands.
- **Covers:** Z2-I-ENV-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Weather_DwellAndChoiceDeterministic`, `Test_Weather_ChoiceFollowsWeights`.
- **Design:** Zenith2/Docs/Design/ENV/WaterWeather.md → "Dwell and choice draws"

### Z2-R-ENV-110 Weather commands

- **Statement:** Request, force (with hold) and release commands from scripts, the editor and cinematics apply at the next phase 1 in merge order, a request while held is refused and counted, and an unknown state is refused.
- **Covers:** Z2-I-ENV-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Weather_ForceHoldRefusesRequests`, `Test_Weather_UnknownStateCommand`.
- **Design:** Zenith2/Docs/Design/ENV/WaterWeather.md → "Transition table"; Zenith2/Docs/Design/ENV/Atmosphere.md → "Commands"

### Z2-R-ENV-111 Weather blending

- **Statement:** Parameters blend with a smoothstep over the transition, wind direction along the shorter arc, precipitation switching at the midpoint, restarts from the current blend, and wetness and snow converge at the stated rates.
- **Covers:** Z2-I-ENV-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Weather_BlendSmoothstepAndDirectionArc`, `Test_Weather_RestartMidTransitionBlendsFromCurrent`, `Test_Weather_WetnessConvergenceRates`.
- **Design:** Zenith2/Docs/Design/ENV/WaterWeather.md → "Blending parameters during a transition"

### Z2-R-ENV-112 Weather effect prefabs

- **Statement:** A state's effect prefabs are spawned through commands parented to the environment anchor while the state holds and destroyed when it ends, with no ENV dependency on VFX or AUDIO.
- **Covers:** Z2-I-ENV-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080, Z2-D-024
- **Budgets:** none
- **Acceptance evidence:** `Test_Weather_EffectPrefabsSpawnedAtSyncPoint`; `Libgraph` (no ENV edge to ZenithVFX or ZenithAudio).
- **Design:** Zenith2/Docs/Design/ENV/WaterWeather.md → "Effect prefabs"

### Z2-R-ENV-113 Weather presentation parameters

- **Statement:** The blended cloud, fog, haze, wetness and snow values reach the render environment every frame, and `Env_ApplySurfaceWeather` applies wetness and snow to terrain, foliage and standard materials.
- **Covers:** Z2-I-ENV-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-014
- **Budgets:** none
- **Acceptance evidence:** `Test_Weather_SurfaceFunctionMatchesFormula` (compute harness) in `ZenithFlux_Tests`.
- **Design:** Zenith2/Docs/Design/ENV/WaterWeather.md → "Presentation parameters"; Zenith2/Docs/Design/ENV/WaterWeather.md → "Shared wetness function"

### Z2-R-ENV-114 Weather events

- **Statement:** `Zenith_WeatherChangedEvent` is emitted at every transition start and arrival in phase 1 and delivered in phase 9 of the same tick to Luau and GAME systems.
- **Covers:** Z2-I-ENV-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Weather_ChangedEventDeliveredPhase9`.
- **Design:** Zenith2/Docs/Design/ENV/WaterWeather.md → "Weather events for gameplay"

### Z2-R-ENV-115 Weather tiers

- **Statement:** From Stage 5 puddles, snow accumulation, precipitation occlusion and lightning flashes run on the tiers of "Weather tiers", presentation only.
- **Covers:** Z2-I-ENV-08
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-014, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_EnvRender_WeatherTiersCrossBackend` (Stage 5) and `Test_EnvironmentDeterminism_TierIndependent`.
- **Design:** Zenith2/Docs/Design/ENV/WaterWeather.md → "Weather tiers"

### Z2-R-ENV-125 Collision heightfield extraction

- **Statement:** `Zenith_ExtractTerrainCollision` returns per tile the quantised heights, per-quad physical materials, hole bits and the fixed diagonal, consumed by PHYS's heightfield cook into one collision shape per tile.
- **Covers:** Z2-I-ENV-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-003
- **Budgets:** none
- **Acceptance evidence:** `Test_EnvironmentCook_CollisionAndNavExtraction`, `Test_TerrainRuntime_HeightMatchesCollisionTriangles`.
- **Design:** Zenith2/Docs/Design/ENV/Bake.md → "Heightfield extraction"; Zenith2/Docs/Design/ENV/Bake.md → "Holes and materials in collision"

### Z2-R-ENV-126 Collidable foliage shapes

- **Statement:** Collidable foliage instances of a cooked cell become one `Zenith_FoliageCollisionSet` per cell in record order, cooked by PHYS into one static compound per cell referenced from the cell content.
- **Covers:** Z2-I-ENV-11, Z2-I-ENV-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-003
- **Budgets:** none
- **Acceptance evidence:** `Test_EnvironmentCook_CollisionAndNavExtraction` (the foliage set matches the collidable records).
- **Design:** Zenith2/Docs/Design/ENV/Bake.md → "Collidable foliage"

### Z2-R-ENV-127 Navigation geometry extraction

- **Statement:** `Zenith_ExtractNavigationGeometry` returns per tile the decimated walkable triangles without holes, with area codes from layers, slope and water, for the navigation bake.
- **Covers:** Z2-I-ENV-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, D-SEL-NAV (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** `Test_EnvironmentCook_CollisionAndNavExtraction`.
- **Design:** Zenith2/Docs/Design/ENV/Bake.md → "Navigation geometry extraction"; Zenith2/Docs/Design/ENV/Bake.md → "Area flags"

### Z2-R-ENV-128 Navigation hook waits for D-SEL-NAV

- **Statement:** The navigation hook's consumer is implemented after D-SEL-NAV (open, Stage 1/2) is ruled; until then the extraction exists as the API-only slice with its extraction tests.
- **Covers:** Z2-I-ENV-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-NAV (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** The navigation consumer ticket is `blocks`-linked from the D-SEL-NAV ruling ticket.
- **Design:** Zenith2/Docs/Design/ENV/Bake.md → "Navigation geometry extraction"; Zenith2/Docs/Design/ENV.md → "Open decisions"

### Z2-R-ENV-129 Regeneration after edits

- **Statement:** `Zenith_RegenerateTerrainDerivedData` regenerates, for the dirty tiles only, the tile file, collision tile and navigation geometry, and re-places the dirty tiles' cells dilated by the placement run's largest ring, with the same writers the cook uses, so the incremental result equals a clean cook.
- **Covers:** Z2-I-ENV-11, Z2-I-ENV-12, Z2-I-ENV-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079, D-SEL-NAV (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** `Test_EnvironmentCook_IncrementalEqualsClean`.
- **Design:** Zenith2/Docs/Design/ENV/Bake.md → "Regeneration after edits"

### Z2-R-ENV-130 Cell alignment

- **Statement:** STREAM cells used with a terrain are whole multiples of 128 m aligned to its origin, and the cook refuses a violating world (`COOK_ERROR_IMPORT_FAILED` with an `ENV_ERROR_INVALID_DESC` diagnostic).
- **Covers:** Z2-I-ENV-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_EnvironmentCook_CellAlignmentChecked`.
- **Design:** Zenith2/Docs/Design/ENV/Bake.md → "Environment cells and STREAM cells"

### Z2-R-ENV-131 Cell content records

- **Statement:** ENV's contribution to a streaming cell is scene content of terrain-tile, foliage-cell, collider and navigation entities referencing ENV's, PHYS's and AI's cooked assets, with the terrain root, large water bodies and settings in the always-resident layer.
- **Covers:** Z2-I-ENV-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-020
- **Budgets:** none
- **Acceptance evidence:** `Test_EnvironmentCells_ContentRecords` in `Zenith_Integration_Environment` (a cooked cell holds exactly the listed entities).
- **Design:** Zenith2/Docs/Design/ENV/Bake.md → "Cell content records"

### Z2-R-ENV-132 Cell load and unload leave nothing behind

- **Statement:** 1,000 randomised cell load, cancel and unload cycles leave no ENV asset handle, render instance, world-resource entry or allocation.
- **Covers:** Z2-I-ENV-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_EnvironmentCells_LoadUnload1000Cycles_NoLeak` (`z2-stress`).
- **Design:** Zenith2/Docs/Design/ENV/Bake.md → "Activation and cancellation"

### Z2-R-ENV-133 Large coordinates

- **Statement:** ENV positions stay in their authored frame in cooked data (resolving 1 mm within ±8,192 m and 2 mm within ±16,384 m of its origin, which ENV accepts), and STREAM's rebase offset is subtracted in double precision at extraction and added back to CPU query inputs, with a rebase re-extracting ENV's render records as a full-state extraction.
- **Covers:** Z2-I-ENV-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_EnvironmentCells_RebaseReextracts` in `Zenith_Integration_Environment` (with STREAM's rebase, pending STREAM).
- **Design:** Zenith2/Docs/Design/ENV/Bake.md → "Large coordinates"

### Z2-R-ENV-134 Resident data under pressure

- **Statement:** ENV's tile and foliage cell data is reported as `MEMORY_SUBSYSTEM_STREAMING` usage and released through STREAM's evictions, and memory pressure never changes a simulation result.
- **Covers:** Z2-I-ENV-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-STREAM-001; pending row "Environment resident CPU data"
- **Acceptance evidence:** `Test_EnvironmentDeterminism_PressureIndependent`; the memory-consumer counters in `Test_MemoryGovernor_ForcedCriticalShedsEachConsumer` (CORE) with the module.
- **Design:** Zenith2/Docs/Design/ENV/Foliage.md → "Instance cap and shedding"; Zenith2/Docs/Design/ENV.md → "Failure table"

### Z2-R-ENV-141 Environment documents migrate

- **Statement:** The foliage type, placement, atmosphere, weather, water, terrain layer and cloud-noise documents are read into the value types of "Document value types" from current-version bytes, each type carries a schema version and a tool-side migration chain with a fixture per version, is edited as a JSON tree through SCENE's document framework so its unknown members are preserved (ENV declares no writer for them), and joins the authoring-document list.
- **Covers:** Z2-I-ENV-03, Z2-I-ENV-06, Z2-I-ENV-08, Z2-I-ENV-10, Z2-I-ENV-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EnvironmentDocumentTypes_MigrationFixtures` (every type's fixtures) in `ZenithCook_Tests`; `Fuzz_EnvironmentDocuments`.
- **Design:** Zenith2/Docs/Design/ENV.md → "Migration and re-cook"; Zenith2/Docs/Design/ENV.md → "Zenith_EnvironmentDocuments header"; Zenith2/Docs/Design/ENV/Bake.md → "Document value types"; Zenith2/Docs/Design/ENV/Bake.md → "Environment document types"

### Z2-R-ENV-142 Environment replay determinism

- **Statement:** A 3,600-tick replay with weather transitions, time commands and streamed tiles gives identical per-tick world hashes with 1, 2, 8 and all workers, forced compaction and two chunk capacities, on each ECS backend.
- **Covers:** Z2-I-ENV-08, Z2-I-ENV-09, Z2-I-ENV-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_EnvironmentDeterminism_ReplayWorkerInvariant` in `Tests/Determinism/`.
- **Design:** Zenith2/Docs/Design/ENV/Tests.md → "Determinism tests"

### Z2-R-ENV-145 Environment debug views

- **Statement:** Each debug variable of "Debug views" draws its view in development builds and the frame dump carries the environment object.
- **Covers:** Z2-I-ENV-01, Z2-I-ENV-02, Z2-I-ENV-03, Z2-I-ENV-04, Z2-I-ENV-05, Z2-I-ENV-06, Z2-I-ENV-07, Z2-I-ENV-08, Z2-I-ENV-09, Z2-I-ENV-10, Z2-I-ENV-11, Z2-I-ENV-12, Z2-I-ENV-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_EnvironmentDebugViews_Registered` and `Test_EnvironmentFrameDump_HasEnvironmentObject` in `ZenithFlux_Tests`.
- **Design:** Zenith2/Docs/Design/ENV.md → "Debug views"; Zenith2/Docs/Design/ENV.md → "Frame-dump entries"

### Z2-R-ENV-148 Environment Luau bindings

- **Statement:** The `Environment` binding table exposes the queries and commands of "Luau errors and asserts", setters record commands visible at the next tick, and every misuse raises a controlled Luau error naming the argument.
- **Covers:** Z2-I-ENV-08, Z2-I-ENV-09, Z2-I-ENV-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-016, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_EnvironmentBindings_MisuseRaisesLuauErrors` and a Luau conformance script in `Zenith_Integration_Environment`.
- **Design:** Zenith2/Docs/Design/ENV.md → "Luau errors and asserts"

### Z2-R-ENV-151 ENV rows hold in W-Outdoor at Stage 4

- **Statement:** In the Stage 4 W-Outdoor and W-OpenWorld runs, water surface and underwater passes, detail-foliage generation and ENV's CPU rows stay within the pending rows "Water surface and underwater GPU", "Detail foliage generation GPU", "Environment update CPU" and "Environment extraction CPU", and ENV's tile and foliage cell data stays within `Z2-B-STREAM-001` (the streaming rows' first confirmation, ruling Q73: W-Outdoor at Stage 3 runs without the Streaming module).
- **Covers:** Z2-I-ENV-05, Z2-I-ENV-03, Z2-I-ENV-08, Z2-I-ENV-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-GEOM-005; Z2-B-STREAM-001; pending row "Water surface and underwater GPU"; pending row "Detail foliage generation GPU"; pending row "Environment update CPU"; pending row "Environment extraction CPU"
- **Acceptance evidence:** The W-Outdoor stage acceptance measurement with MP-GPUPASS.
- **Design:** Zenith2/Docs/Design/ENV.md → "Pending row needs"; Zenith2/Docs/Design/ENV.md → "Rows measured against"

### Z2-R-ENV-154 Foliage activation catch-up

- **Statement:** After a teleport or rapid traversal in W-Outdoor, the foliage activation backlog drains within the pending row "Foliage activation catch-up frames" without a hitch beyond the hitch rows.
- **Covers:** Z2-I-ENV-03, Z2-I-ENV-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** pending row "Foliage activation catch-up frames"
- **Acceptance evidence:** The MP-STREAM teleport runs of the Stage 4 acceptance.
- **Design:** Zenith2/Docs/Design/ENV/Tests.md → "Workload and device evidence"; Zenith2/Docs/Design/ENV/Foliage.md → "Activation and deactivation"

### Z2-R-ENV-155 S24 sustained W-Outdoor

- **Statement:** In the S24 sustained run of W-Outdoor, thermal step-downs reduce foliage density and cloud tier only through the quality tier, and simulation hashes match an unthrottled run.
- **Covers:** Z2-I-ENV-03, Z2-I-ENV-07
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-010, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** The Stage 6 WS-SUSTAIN device run record (needs-human device step).
- **Design:** Zenith2/Docs/Design/ENV/Tests.md → "Workload and device evidence"

### Z2-R-ENV-156 GPU performance rules compliance

- **Statement:** ENV's passes and materials follow the universal GPU rules: at most 16 bits per channel except justified targets, half precision for colour, discard only in alpha-tested variants, back-face culling unless two-sided, and no tile-local reads.
- **Covers:** Z2-I-ENV-02, Z2-I-ENV-04, Z2-I-ENV-05, Z2-I-ENV-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-014, Z2-D-036
- **Budgets:** none
- **Acceptance evidence:** `Test_EnvRender_ShaderLintsClean` (`ShaderChecks`) and the render graph's compile checks in the render tests.
- **Design:** Zenith2/Docs/Design/ENV.md → "Shader modules and engine content"; Zenith2/Docs/Design/ENV/WaterWeather.md → "Underwater composite pass"; Zenith2/Docs/Design/ENV/Atmosphere.md → "Cloud layer pass"

### Z2-R-ENV-157 Environment backend parity

- **Statement:** The environment parity scene (terrain, foliage, water and clouds at a fixed time and weather) passes BackendParity from Stage 4 and, with the Deferred and visibility-buffer paths, PathParity in Stage 5.
- **Covers:** Z2-I-ENV-02, Z2-I-ENV-05, Z2-I-ENV-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-063
- **Budgets:** none
- **Acceptance evidence:** `Test_EnvRender_BackendParity` in `BackendParity`; the Stage 5 PathParity run.
- **Design:** Zenith2/Docs/Design/ENV/Tests.md → "Render and parity tests"

### Z2-R-ENV-158 Terrain and foliage in every path

- **Statement:** From Stage 5 terrain and foliage draw in the Deferred and visibility-buffer paths through the same vertex modifier and terrain material, with the capability-matrix rows ENV requests.
- **Covers:** Z2-I-ENV-01, Z2-I-ENV-03, Z2-I-ENV-04
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-035
- **Budgets:** none
- **Acceptance evidence:** The Stage 5 PathParity run over the environment parity scene.
- **Design:** Zenith2/Docs/Design/ENV.md → "Stage table"; Zenith2/Docs/Design/ENV.md → "Assumed types from pending design documents"

### Z2-R-ENV-159 Local fog volumes

- **Statement:** From Stage 5 `Zenith_LocalFogVolumeComponent` entities reach LIGHT's local fog volumes, at most 64 per world nearest the main camera.
- **Covers:** Z2-I-ENV-06
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_Atmosphere_LocalFogVolumesExtracted` (Stage 5).
- **Design:** Zenith2/Docs/Design/ENV.md → "Zenith_EnvironmentComponents header"

### Z2-R-ENV-167 Detail foliage within the GEOM limits

- **Statement:** Each detail procedural instance holds at most 8,191 lattice points (a (cell, type) lattice beyond it splits into row blocks), its sizing box contains every position the generator writes and every drawn position after the draw-time lift and the 0.5 m wind clamp (so GEOM's writer never clamps and culling never drops a drawn blade), the extractor keeps the resident detail vertices within `environment.foliage.detailMaxVertices` nearest the anchor first, and the pool sizing inputs of "Detail pool sizing" feed GEOM's Stage 1 budget ticket.
- **Covers:** Z2-I-ENV-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-058, Z2-D-014
- **Budgets:** Z2-B-GEOM-001; Z2-B-GEOM-005
- **Acceptance evidence:** `Test_DetailFoliage_BlocksWithinGeomLimits`, `Test_DetailFoliage_SourceSizingAndIndices` (extreme-parameter positions inside the box before the clamp), `Test_DetailFoliage_VertexCapNearestFirst`; the Stage 1 per-system budget ticket for GEOM citing the sizing table.
- **Design:** Zenith2/Docs/Design/ENV/Foliage.md → "Blade and card geometry"; Zenith2/Docs/Design/ENV/Foliage.md → "Detail pool sizing"; Zenith2/Docs/Design/ENV/Foliage.md → "Detail cells and lattice"

### Z2-R-ENV-168 Detail placement at draw time

- **Statement:** `Material_DetailFoliage` places each generated blade in its vertex modifier: the pivot from the lattice index in UV0, acceptance from the type's density and splat layers times the density scale, the base on the terrain's triangle height, and wind for the current and previous time, so density and splat edits and density-scale changes apply without regeneration.
- **Covers:** Z2-I-ENV-03, Z2-I-ENV-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-014, Z2-D-058
- **Budgets:** none
- **Acceptance evidence:** `Test_DetailFoliage_DrawTimePlacementMatchesCpu` (compute harness against a CPU reference), `Test_DetailFoliage_DensityEditWithoutRegeneration`. Pending core-doc batch 4 (X127 (1)).
- **Design:** Zenith2/Docs/Design/ENV/Foliage.md → "Draw-time placement"

### Z2-R-ENV-169 Water material generated per document

- **Statement:** The water cook generates each water document's `material:water` sub-asset (`Material_Water`, transparent, two-sided, premultiplied, refraction) with the document's optical parameters and textures, the `.zwater` records its GUID, and a body's render record carries its material and mesh asset handles.
- **Covers:** Z2-I-ENV-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-014
- **Budgets:** none
- **Acceptance evidence:** `Test_EnvironmentCook_WaterMaterialGenerated`, `Test_Water_BodyRecordCarriesHandles`.
- **Design:** Zenith2/Docs/Design/ENV/WaterWeather.md → "Water GPU side"; Zenith2/Docs/Design/ENV/Bake.md → "Water file"

### Z2-R-ENV-170 Water node instances

- **Statement:** Water nodes are GPU-scene instances inserted by ENV's GPU side with the custom-data layout of "Water GPU side" (level, node x, node z, body index); an ocean's root square is centred within 2,048 m of the reference position with its displacement faded to zero at its boundary, and its one skirt instance begins exactly at that boundary, so every point within 129,024 m of the camera is drawn by exactly one node or skirt quad; the skirt's T-junctions with the boundary nodes are accepted within the raster crack budget of "Water node selection", a lake's polygon bounds are at most 4,096 m per axis (larger is `ENV_ERROR_INVALID_DESC` at activation), at most 256 bodies are drawn per frame, and a body REMOVE removes every node instance and releases its handles.
- **Covers:** Z2-I-ENV-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-058
- **Budgets:** none
- **Acceptance evidence:** `Test_Water_OceanCoverageNoGap`, `Test_Water_NodeCustomDataLayout`, `Test_Water_BodyRemoveReleasesEverything`, `Test_EnvRender_WaterSurfaceCrossBackend` (with its boundary crack case). The water-body records travel through core-doc item X156 (pending core-doc batch 4).
- **Design:** Zenith2/Docs/Design/ENV/WaterWeather.md → "Water GPU side"; Zenith2/Docs/Design/ENV/WaterWeather.md → "Water node selection"

### Z2-R-ENV-171 VT-backed terrain reference scene

- **Statement:** ENV owns the TerrainVT reference scene of the physical-accuracy suite: four VT-backed layers on a 256 m fixture terrain rendered by the raster path agree with the converged path-traced reference within `Z2-B-ADVGFX-008`.
- **Covers:** Z2-I-ENV-02, Z2-I-ENV-01
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-043, Z2-D-037
- **Budgets:** Z2-B-ADVGFX-008
- **Acceptance evidence:** `Test_EnvRender_TerrainVTReference` in the Stage 5 physical-accuracy suite (pending ADVGFX, row EN19).
- **Design:** Zenith2/Docs/Design/ENV/Terrain.md → "VT-backed terrain materials"

### Z2-R-ENV-173 Terrain documents merge per tile file

- **Statement:** Terrain tile documents merge by file, a tile changed on both sides being a conflict resolved by taking one side whole, and the terrain document merges structurally keyed by tile coordinates, layer IDs and GUIDs, with order changes on both sides a conflict.
- **Covers:** Z2-I-ENV-01, Z2-I-ENV-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** CONTENT's merge driver tests over ENV's fixture documents (pending CONTENT, row EN20), `Test_TerrainDocument_MergeFixtures`.
- **Design:** Zenith2/Docs/Design/ENV/Bake.md → "Merging terrain documents"

### Z2-R-ENV-176 Weather and wind state saved

- **Statement:** The weather runtime state and the wind state with their stream counters are reflected properties of the environment state, saved and restored with the world, so a restored run continues the same weather and wind sequence.
- **Covers:** Z2-I-ENV-08, Z2-I-ENV-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_Weather_SaveRestoreContinuesSequence`, `Test_Wind_StateDeterministicPerTick`.
- **Design:** Zenith2/Docs/Design/ENV.md → "Zenith_EnvironmentState header"

### Z2-R-ENV-178 Cloud noise volumes cooked by ENV

- **Statement:** ENV cooks each `*.cloudnoise.json` document into a `.zcloudnoise` file of asset kind `Environment.CloudNoise` (code 34, added by ASSET's growth rule) with the generator of "Cloud noise generator" exactly (the `pack3` bit order, the draw keys, seeds and indices, the zero-gradient fallback, the quintic fade, the octave sum normalised by 1.875, `remap`, the Euclidean Worley distance) evaluated in double with the pinned hash (D-SEL-HASH), quantised per channel and chained by the 2 × 2 × 2 integer mean, tiling across every face, and ENV's kind loader creates the 3D `R8G8B8A8_UNORM` texture the cloud passes sample.
- **Covers:** Z2-I-ENV-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-004, D-SEL-HASH (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_CloudNoise_GeneratorGoldenTexels` (16 golden texels under the constant draw), `Test_CloudNoise_GeneratorDrawArguments`, `Test_CloudNoise_CookDeterministicAndTileable`, `Test_EnvironmentReaders_RejectOverLimitCounts`, `Fuzz_CloudNoiseFile`; the kind row is added in the ticket's diff with ASSET as reviewer (row EN22).
- **Design:** Zenith2/Docs/Design/ENV/Atmosphere.md → "Cloud noise volumes"; Zenith2/Docs/Design/ENV/Atmosphere.md → "Cloud noise generator"; Zenith2/Docs/Design/ENV/Bake.md → "Cloud noise file"; Zenith2/Docs/Design/ENV/Bake.md → "Cloud noise document schema"

### Z2-R-ENV-181 Runtime objects of the Stage 4 kinds

- **Statement:** The runtime objects of kinds 26, 27 and 34 are `Zenith_WeatherAsset` (owning its `Zenith_WeatherTable`, converted with `sim.tickHz`), `Zenith_WaterAsset` and `Flux_CloudNoiseResource` with exactly the accessors of "Runtime asset objects", immutable after load, and their names match ASSET's kind table.
- **Covers:** Z2-I-ENV-05, Z2-I-ENV-07, Z2-I-ENV-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_EnvironmentAssets_AccessorsMatchSections` (the weather and water cases), `Test_CloudNoiseKindLoader_ExtentAndChain`.
- **Design:** Zenith2/Docs/Design/ENV/Bake.md → "Runtime asset objects"; Zenith2/Docs/Design/ENV/WaterWeather.md → "Zenith_WeatherSystem header"

### Z2-R-ENV-182 Detail material generated per terrain

- **Statement:** From Stage 4 the terrain cook declares and generates the `material:detail` sub-asset (`Material_DetailFoliage`, with the detail types' constants) only for a terrain whose foliage-type list holds at least one detail type; otherwise no `material:detail` is declared and the second GUID of section 0x0206 is zero.
- **Covers:** Z2-I-ENV-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_EnvironmentCook_TerrainMaterialsGenerated` (the detail and no-detail fixtures). The material needs the vertex-modifier hook of core-doc item X127 (1).
- **Design:** Zenith2/Docs/Design/ENV/Bake.md → "Step graph"; Zenith2/Docs/Design/ENV/Foliage.md → "Draw-time placement"
