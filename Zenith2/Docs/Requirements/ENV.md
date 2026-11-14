# ENV requirement catalogue

## Purpose

This catalogue breaks the ENV inventory items (`Zenith2/Docs/Requirements/Inventory.md → "Inventory v1"`, section "ENV Environment") into testable requirements `Z2-R-ENV-NNN`. Each requirement names the inventory items it covers, the stage whose acceptance it serves, its classification, its governing decisions and budget rows, the artefact that proves it, and the headings of the ENV design family (`Zenith2/Docs/Design/ENV.md` and its section files) that specify it; the design entry's "Requirement index" is the reverse map. The catalogue is two files with one numbering: this file holds the purpose, the requirements of Stages 1 and 3, the coverage of inventory items and the exclusions; `Zenith2/Docs/Requirements/ENV/Stage4Plus.md` holds the requirements of Stages 4, 5 and 6. ENV owns every requirement here; where another area supplies part of the evidence (GEOM's terrain geometry, LIGHT's sky and fog passes, RPROD's instancing and frame, PHYS's and AI's cooks, EDIT's tools, STREAM's cells, WORLD's orchestration), that area is named in the evidence and in `Zenith2/Docs/Design/ENV.md → "Dependencies"`. A budget named "pending row" is a row proposed in `Zenith2/Docs/Design/ENV.md → "Pending row needs"` and allocated by its producer before the requirement's tickets are claimable.

## Requirements

### Z2-R-ENV-001 Module library target

- **Statement:** `ZenithEnvironment` is a `KIND MODULE` static library with `MODULE_NAME Environment` in `Zenith2/Engine/ZenithEnvironment/`, declares `ZENITH_GPU` and exactly the `DEPENDS` list of the table "Target summary", contains no platform-specific source, and no Player links it unless the game's `game.json` selects `Environment`.
- **Covers:** Z2-I-ENV-01, Z2-I-ENV-02, Z2-I-ENV-03, Z2-I-ENV-04, Z2-I-ENV-05, Z2-I-ENV-06, Z2-I-ENV-07, Z2-I-ENV-08, Z2-I-ENV-09, Z2-I-ENV-10, Z2-I-ENV-11, Z2-I-ENV-12, Z2-I-ENV-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-024, Z2-D-001, Z2-D-042
- **Budgets:** none
- **Acceptance evidence:** `Libgraph` (declared edges against `declared_deps.json`), `CompileCoverage` and `SourceLayout` on the ticket that lands the library; a game without the module links no ZenithEnvironment symbol (Libgraph link surface). Pending core-doc batch 4 (X124): the `ZenithPlatform`, `ZenithCompression` and `ZenithProfiling` edges and ZenithCook's edge land in Architecture.md first, and the scaffold ticket is `blocks`-linked from that batch.
- **Design:** Zenith2/Docs/Design/ENV.md → "Target summary"; Zenith2/Docs/Design/ENV.md → "ZenithEnvironment files"; Zenith2/Docs/Design/ENV.md → "ZENITH_SUPPORTED matrix"

### Z2-R-ENV-002 Module entry points

- **Statement:** `Zenith_Module_Environment.h` declares exactly `Zenith_Module_Environment_RegisterTypes` and `Zenith_Module_Environment_Start` with the signatures of [Z2-N-ARCH-168], and their steps register ENV's types, configuration keys, kind loaders, command kinds, bindings, memory consumer, world resources, systems and extractors in the order of the entry-point table, and no document type or migration chain (those are tool-side).
- **Covers:** Z2-I-ENV-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-024, Z2-D-081
- **Budgets:** none
- **Acceptance evidence:** `Test_EnvironmentModule_Headless_RunsSimulationSystems` and `Test_Runtime_ModuleRegistrationsUndone` (ZenithRuntime) with the Environment module; `ApiContractCheck` over the header.
- **Design:** Zenith2/Docs/Design/ENV.md → "Zenith_Module_Environment header"

### Z2-R-ENV-003 Construction steps unwind

- **Statement:** Every construction step of module start and world attach has a failure-injection test that fails that step alone and asserts the module error, zero leaks, and no registration or configuration callback left behind.
- **Covers:** Z2-I-ENV-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045, Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** The nine tests `Test_EnvironmentModule_Create_FailsAtRegisterTypes1_Unwinds` … `Test_EnvironmentModule_Create_FailsAtAttachWorld3_Unwinds` (one per step of the entry-point table) in `ZenithEnvironment_Tests` (`Test` step).
- **Design:** Zenith2/Docs/Design/ENV.md → "Zenith_Module_Environment header"; Zenith2/Docs/Design/ENV.md → "Construction and shutdown"

### Z2-R-ENV-004 Public headers carry contracts

- **Statement:** Every public type of the ZenithEnvironment headers and of `ZenithFlux/Flux_Environment.h` carries one contract block, every fallible function returns `Zenith_Result<…, Zenith_EnvironmentError>`, each header compiles alone with no STL type and no third-party header, and every `[FluxShared]` struct of ENV is declared in `Zenith2/Shaders/Common/Environment.slang`.
- **Covers:** Z2-I-ENV-01, Z2-I-ENV-02, Z2-I-ENV-03, Z2-I-ENV-04, Z2-I-ENV-05, Z2-I-ENV-06, Z2-I-ENV-07, Z2-I-ENV-08, Z2-I-ENV-09, Z2-I-ENV-10, Z2-I-ENV-11, Z2-I-ENV-12, Z2-I-ENV-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-047, Z2-D-045, Z2-D-041
- **Budgets:** none
- **Acceptance evidence:** `ApiContractCheck`, `HeaderSelfContained` and `ForbiddenCheck` over the headers; the shader compiler's `ZSC_ERROR_SHARED_LOCATION` check over ENV's modules (`ShaderChecks`).
- **Design:** Zenith2/Docs/Design/ENV.md → "Rules across the ENV API"; Zenith2/Docs/Design/ENV.md → "Header map"

### Z2-R-ENV-005 API-only slices first

- **Statement:** The first ENV tickets land the API-only slices of "API-only slices" (declarations, contract blocks, stub definitions) so EDIT, WORLD, PHYS and AI tickets build against them before the implementations.
- **Covers:** Z2-I-ENV-01, Z2-I-ENV-02, Z2-I-ENV-03, Z2-I-ENV-04, Z2-I-ENV-05, Z2-I-ENV-06, Z2-I-ENV-07, Z2-I-ENV-08, Z2-I-ENV-09, Z2-I-ENV-10, Z2-I-ENV-11, Z2-I-ENV-12, Z2-I-ENV-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-070
- **Budgets:** none
- **Acceptance evidence:** Each API-only ticket passes `Build`, `ApiContractCheck` and `HeaderSelfContained`; dependent tickets are `blocks`-linked from it.
- **Design:** Zenith2/Docs/Design/ENV.md → "API-only slices"; Zenith2/Docs/Design/ENV.md → "Ticket slices and ordering"

### Z2-R-ENV-006 Error record and codes

- **Statement:** `Zenith_EnvironmentError` is a trivially copyable record of at most 32 bytes holding a `Zenith_EnvironmentErrorCode` numbered from 1 and fixed-size context, with a `Zenith_ToString` overload, and ENV maps it explicitly to the module error and to ASSET's asset error detail at those boundaries.
- **Covers:** Z2-I-ENV-01, Z2-I-ENV-02, Z2-I-ENV-03, Z2-I-ENV-04, Z2-I-ENV-05, Z2-I-ENV-06, Z2-I-ENV-07, Z2-I-ENV-08, Z2-I-ENV-09, Z2-I-ENV-10, Z2-I-ENV-11, Z2-I-ENV-12, Z2-I-ENV-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_EnvironmentErrors_ValuesStable` (static_asserts of every enumerator and the record size) in `ZenithEnvironment_Tests`; review of the boundary mappings.
- **Design:** Zenith2/Docs/Design/ENV.md → "Zenith_EnvironmentErrors header"; Zenith2/Docs/Design/ENV.md → "Error enumerations"

### Z2-R-ENV-007 Configuration keys are presentation only

- **Statement:** Every `environment.*` configuration key of the table "Configuration keys" is registered from Stage 3 with its type, range, default and allowed layers (the detail keys `detailNearRadius` and `detailMaxVertices` too, inert until the Stage 4 detail foliage reads them), and none is simulation-affecting, so a quality-tier change or thermal adaptation never changes a simulation result.
- **Covers:** Z2-I-ENV-03, Z2-I-ENV-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_EnvironmentConfig_KeysRegistered` (a config dump lists every key with its schema) and `Test_EnvironmentDeterminism_TierIndependent`.
- **Design:** Zenith2/Docs/Design/ENV.md → "Configuration keys"

### Z2-R-ENV-008 Headless configuration

- **Statement:** In a headless run the module registers its phase-1 systems and world resources and no extractor, so time of day, weather, wind, terrain height and water queries run while no render record is produced.
- **Covers:** Z2-I-ENV-09, Z2-I-ENV-11
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-081
- **Budgets:** none
- **Acceptance evidence:** `Test_EnvironmentModule_Headless_RunsSimulationSystems` in `Zenith_Integration_Environment`.
- **Design:** Zenith2/Docs/Design/ENV.md → "Zenith_Module_Environment header"; Zenith2/Docs/Design/ENV.md → "Frame-graph placement"

### Z2-R-ENV-009 Components registered

- **Statement:** The nine components of "Zenith_EnvironmentComponents header" are the C++ structs declared there, each with one contract block, the property IDs and stable property names of its member comments and the stated authorities, registered from C++ with `NETWORK_AUTHORITY_SERVER`, serialized properties and lifecycle hooks for asset-handle properties, and indistinguishable from other registered types to every consumer.
- **Covers:** Z2-I-ENV-01, Z2-I-ENV-03, Z2-I-ENV-05, Z2-I-ENV-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-020
- **Budgets:** none
- **Acceptance evidence:** `Test_EnvironmentComponents_Registration` (registry lookups, property IDs and names equal the header comments, authorities, hooks balance asset references, serialization by property ID) in `ZenithEnvironment_Tests`; `ApiContractCheck` over the header.
- **Design:** Zenith2/Docs/Design/ENV.md → "Zenith_EnvironmentComponents header"

### Z2-R-ENV-010 World resources owned by phase 1

- **Statement:** The environment state, render-blocks, terrain, foliage and water world resources are registered with owning phase `PHASE_INPUT` and are written only by ENV's four phase-1 systems (`Environment.Advance` writes the state and the render blocks, each other system one resource); every other change arrives as a command.
- **Covers:** Z2-I-ENV-06, Z2-I-ENV-08, Z2-I-ENV-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** Schedule compile of the fixture world (a second writer is `SCHEDULE_ERROR_RESOURCE_AUTHORITY`, `Test_EnvironmentSchedule_SecondWriterRejected`).
- **Design:** Zenith2/Docs/Design/ENV.md → "Frame-graph placement"; Zenith2/Docs/Design/ENV.md → "Tick-level flow of ENV work"

### Z2-R-ENV-011 Terrain tile grid

- **Statement:** A terrain is a grid of 128 m tiles of 257 × 257 height samples at 0.5 m, at most 256 tiles per axis, with sample and texel coordinates as "Coordinates and tile grid" defines and absent tiles allowed.
- **Covers:** Z2-I-ENV-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_TerrainDocument_Create_TileGeometry`.
- **Design:** Zenith2/Docs/Design/ENV/Terrain.md → "Coordinates and tile grid"

### Z2-R-ENV-012 Float authoring heights

- **Statement:** Authored heights are IEEE binary32 metres within the document's height limits, and the cook quantises them to `uint16` over the terrain's tight range with the formula of "Height representation", identical in GEOM's tile and ENV's tile CPU data.
- **Covers:** Z2-I-ENV-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_EnvironmentCook_HeightsMatchGeomTiles`; `Test_TerrainDocument_Load_OutOfRangeValues`.
- **Design:** Zenith2/Docs/Design/ENV/Terrain.md → "Height representation"

### Z2-R-ENV-013 Terrain document schema

- **Statement:** The `*.terrain.json` document has exactly the members and rules of "Terrain document schema" and `Load` rejects any violation with `ENV_ERROR_DOCUMENT_SCHEMA` naming the field.
- **Covers:** Z2-I-ENV-01, Z2-I-ENV-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_TerrainDocument_SaveLoad_RoundTripsBytes`, `Test_TerrainDocument_Create_RejectsInvalidDesc`, `Fuzz_TerrainDocument`.
- **Design:** Zenith2/Docs/Design/ENV/Bake.md → "Terrain document schema"; Zenith2/Docs/Design/ENV/Terrain.md → "Zenith_TerrainDocument header"

### Z2-R-ENV-014 Terrain tile document schema

- **Statement:** Each present tile is one `*.terraintile.json` whose payloads are base64 of COMPRESS sections decoded with the codec named by the data, with sparse layer, density and exclusion maps, an optional hole mask and explicit instances sorted by ID.
- **Covers:** Z2-I-ENV-01, Z2-I-ENV-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_TerrainDocument_SaveLoad_RoundTripsBytes`, `Test_TerrainDocument_Load_CorruptTilePayload`, `Fuzz_TerrainDocument`.
- **Design:** Zenith2/Docs/Design/ENV/Bake.md → "Terrain tile document schema"

### Z2-R-ENV-015 Load validation order

- **Statement:** `Zenith_TerrainDocument::Load` validates in the order of "Zenith_TerrainDocument header" and returns the first failure as a recoverable error naming the document, tile or field, never an assert.
- **Covers:** Z2-I-ENV-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_TerrainDocument_Load_MissingTile`, `Test_TerrainDocument_Load_CorruptTilePayload`, `Test_TerrainDocument_Load_OutOfRangeValues`, `Fuzz_TerrainDocument`.
- **Design:** Zenith2/Docs/Design/ENV/Terrain.md → "Zenith_TerrainDocument header"

### Z2-R-ENV-016 Shared borders stay equal

- **Statement:** Shared border samples of adjacent tiles are equal after every edit, a mismatch at load fails with `ENV_ERROR_TILE_BORDER_MISMATCH` in the cook, and the editor's repair takes the smaller-index tile's value with a warning.
- **Covers:** Z2-I-ENV-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_TerrainBrush_BorderStaysEqual`, `Test_TerrainDocument_Load_BorderMismatchFailsInCook`, `Test_TerrainDocument_Load_BorderRepairInEditor`, `Test_EnvironmentCook_BorderMismatchFails`.
- **Design:** Zenith2/Docs/Design/ENV/Terrain.md → "Height clamping and border consistency"; Zenith2/Docs/Design/ENV/Terrain.md → "Brush application order"

### Z2-R-ENV-018 Newer documents read-only

- **Statement:** An ENV document whose schema version is newer than the running tool's is opened read-only with `ENV_WARNING_NEWER_SCHEMA`, and `Save` refuses it with `ENV_ERROR_DOCUMENT_READ_ONLY`.
- **Covers:** Z2-I-ENV-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_TerrainDocument_NewerVersionReadOnly`.
- **Design:** Zenith2/Docs/Design/ENV/Terrain.md → "Zenith_TerrainDocument header"

### Z2-R-ENV-019 Byte-stable saves

- **Statement:** `Save` writes the terrain document and only the dirty tile documents through one journaled batch, byte-stable, so saving an unchanged document writes nothing new.
- **Covers:** Z2-I-ENV-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_TerrainDocument_SaveLoad_RoundTripsBytes`.
- **Design:** Zenith2/Docs/Design/ENV/Terrain.md → "Zenith_TerrainDocument header"

### Z2-R-ENV-020 Structural terrain edits

- **Statement:** `AddTile`, `RemoveTile`, `AddLayer`, `RemoveLayer`, `MoveLayer`, `SetFoliageTypes` and `SetPlacementRuleSets` each produce a reversible `Zenith_TerrainEdit`, an added tile takes its borders from present neighbours, and the palette never exceeds 16 layers.
- **Covers:** Z2-I-ENV-01, Z2-I-ENV-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_TerrainDocument_AddRemoveTile_BordersFromNeighbours`, `Test_TerrainDocument_LayerLimit`, `Test_TerrainDocument_RemoveLayer_WeightRedistributed`.
- **Design:** Zenith2/Docs/Design/ENV/Terrain.md → "Zenith_TerrainDocument header"; Zenith2/Docs/Design/ENV/Terrain.md → "Layers"

### Z2-R-ENV-021 Brush falloff

- **Statement:** `Zenith_TerrainBrushFalloff` computes the weight of "Brush falloff" with the stated operation order, and the brushes and the editor cursor both use it.
- **Covers:** Z2-I-ENV-01, Z2-I-ENV-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_TerrainBrush_Falloff_Table`.
- **Design:** Zenith2/Docs/Design/ENV/Terrain.md → "Brush falloff"

### Z2-R-ENV-022 Brush application order

- **Statement:** Dabs apply in stroke order and every sample of a dab reads the pre-dab snapshot, so a dab's result does not depend on sample visit order.
- **Covers:** Z2-I-ENV-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_TerrainBrush_Smooth_ReadsSnapshot`.
- **Design:** Zenith2/Docs/Design/ENV/Terrain.md → "Brush application order"

### Z2-R-ENV-023 Brushes are worker-count invariant

- **Statement:** A stroke applied with no job system and with 1, 2, 8 and all workers gives bit-identical tiles.
- **Covers:** Z2-I-ENV-01, Z2-I-ENV-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_TerrainBrush_ParallelEqualsSerial`.
- **Design:** Zenith2/Docs/Design/ENV/Terrain.md → "Parallel application"

### Z2-R-ENV-024 Height brushes

- **Statement:** RAISE, LOWER, FLATTEN (a rate), SET_HEIGHT (an assignment), SMOOTH, NOISE (seeded) and RAMP compute exactly the rules of "Brush kinds".
- **Covers:** Z2-I-ENV-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_TerrainBrush_Raise_ExactValues`, `Test_TerrainBrush_Flatten_IsARate`, `Test_TerrainBrush_SetHeight_IsAnAssignment`, `Test_TerrainBrush_Smooth_ReadsSnapshot`, `Test_TerrainBrush_Noise_SeedReproducible`, `Test_TerrainBrush_Ramp_Profile`.
- **Design:** Zenith2/Docs/Design/ENV/Terrain.md → "Brush kinds"; Zenith2/Docs/Design/ENV/Terrain.md → "Stroke and dab model"

### Z2-R-ENV-026 Heights clamp to limits

- **Statement:** Every brush clamps results to the document's height limits and counts clamped strokes in `environment.terrain.brush_clamped`, never failing the stroke.
- **Covers:** Z2-I-ENV-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_TerrainBrush_ClampsToLimits`.
- **Design:** Zenith2/Docs/Design/ENV/Terrain.md → "Height clamping and border consistency"

### Z2-R-ENV-027 Invalid strokes change nothing

- **Statement:** A stroke with a non-finite value, a radius outside [0.5, 512] m, a fraction outside [0, 1] or an unknown layer or foliage type returns its error with the dab index and leaves the document's bytes and revision unchanged.
- **Covers:** Z2-I-ENV-01, Z2-I-ENV-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_TerrainBrush_InvalidDab_ChangesNothing`.
- **Design:** Zenith2/Docs/Design/ENV/Terrain.md → "Stroke and dab model"

### Z2-R-ENV-030 Foliage scatter and erase brushes

- **Statement:** SCATTER_FOLIAGE runs one `Zenith_Place` per dab with the seed `Zenith_Hash64U64(dab index, m_ulNoiseSeed)`, the implicit rule "Environment.Scatter", the falloff weight as the density factor through the scatter surface adapter and the existing explicit instances as obstacles, and turns the kept instances into explicit instances with fresh never-reused IDs; ERASE_FOLIAGE removes the instances whose weight is at least 0.5.
- **Covers:** Z2-I-ENV-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_TerrainBrush_ScatterFoliage_Deterministic`, `Test_TerrainBrush_ScatterFoliage_KeepsFootprints`.
- **Design:** Zenith2/Docs/Design/ENV/Terrain.md → "Scatter brush"; Zenith2/Docs/Design/ENV/Terrain.md → "Brush kinds"; Zenith2/Docs/Design/ENV/Terrain.md → "Explicit foliage instances"

### Z2-R-ENV-031 Undo and redo are bit-exact

- **Statement:** Reverting an edit restores the document's exact pre-edit bytes and reapplying restores the post-edit bytes, for every brush kind and structural edit.
- **Covers:** Z2-I-ENV-01, Z2-I-ENV-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_TerrainEdit_UndoRedo_RoundTripsBitExact`.
- **Design:** Zenith2/Docs/Design/ENV/Terrain.md → "Undo integration"; Zenith2/Docs/Design/ENV/Terrain.md → "Zenith_TerrainEdit contents"

### Z2-R-ENV-032 Stale edits refused

- **Statement:** `ApplyEdit` refuses an edit whose document or revision does not match with `ENV_ERROR_EDIT_STALE` and changes nothing.
- **Covers:** Z2-I-ENV-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_TerrainEdit_StaleRevision_Refused`.
- **Design:** Zenith2/Docs/Design/ENV/Terrain.md → "Undo integration"

### Z2-R-ENV-033 Edit memory proportional to area

- **Statement:** An edit records one union rectangle per touched (tile, channel), compressed at the stroke's end, and an edit above the pending row "Terrain edit delta memory per undo step" is still returned and counted.
- **Covers:** Z2-I-ENV-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** pending row "Terrain edit delta memory per undo step"
- **Acceptance evidence:** `Test_TerrainEdit_MemoryProportionalToArea`, `Test_TerrainBrush_LongStroke_Stress`.
- **Design:** Zenith2/Docs/Design/ENV/Terrain.md → "Delta encoding"

### Z2-R-ENV-034 EDIT owns undo

- **Statement:** EDIT's terrain command owns each `Zenith_TerrainEdit`; ENV declares no undo stack and stores no pointer into the editor.
- **Covers:** Z2-I-ENV-01, Z2-I-ENV-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-079, Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** Review of ENV's headers (`Libgraph`: no ENV edge to ZenithEditor); EDIT's terrain undo integration test (pending EDIT).
- **Design:** Zenith2/Docs/Design/ENV/Terrain.md → "Undo integration"; Zenith2/Docs/Design/ENV.md → "What ENV authors"

### Z2-R-ENV-035 Live preview

- **Statement:** A stroke's dirty tiles reach the rendered terrain within one frame through replacements of the whole 9-mip height and hole chains and whole splat and detail-density chains, computed by the preview with the cold cook's formulas (the quantisation, the hole conversion with absent neighbours, GEOM's chain rules), never a single-mip swap, with `Flux_TerrainRenderer::UpdateNodeBounds` per node run called only in the frame the replaced height chain publishes, a tile whose replacement is refused (`PEAK_REFUSED` or any error) kept dirty and re-sent, and at most `environment.terrain.previewTilesPerFrame` tiles per frame, so every previewed mip equals a cold cook of the edited document.
- **Covers:** Z2-I-ENV-01, Z2-I-ENV-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_TerrainPreview_ChainEqualsColdCook` in `ZenithCook_Tests` (which links the preview and GEOM's cook function); `Test_EnvRender_TerrainPreviewSwap` (GPU readback of every mip) on WARP and lavapipe. The tile-update record travels through core-doc item X156 (pending core-doc batch 4). The GEOM dependency (row EN21, Q52 ruled (a)) is satisfied by GEOM's landed text (row GM31): `Zenith2/Docs/Design/GEOM/Textures.md → "Texture replace from memory"`; the ticket is `blocks`-linked only behind the GEOM ticket that implements it.
- **Design:** Zenith2/Docs/Design/ENV/Terrain.md → "Live preview"; Zenith2/Docs/Design/ENV/Terrain.md → "Zenith_TerrainRuntime header"

### Z2-R-ENV-037 Cooked top-four splat

- **Statement:** The cook reduces each texel to at most four (layer, weight) pairs by the ordering and redistribution of "Cooked top-four reduction", losslessly for texels with at most four layers, reports the others, and writes the index and weight images for GEOM's classes `FLUX_TEXTURE_USAGE_SPLAT_INDEX` and `FLUX_TEXTURE_USAGE_SPLAT_WEIGHT`.
- **Covers:** Z2-I-ENV-01, Z2-I-ENV-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_TerrainSplat_TopFourLosslessUpToFour`, `Test_TerrainSplat_TopFourTieBreak`, `Test_TerrainSplat_OverFourReported`.
- **Design:** Zenith2/Docs/Design/ENV/Terrain.md → "Cooked top-four reduction"

### Z2-R-ENV-038 Layer blend in the material

- **Statement:** `Material_TerrainLayered` accumulates the 2 × 2 texel footprint per layer from the splat textures reached through the terrain's tile table, decoding each index texel read with `Load` as `uint4(round(x × 255))`, and selects the strongest layers as "Layer blend in the terrain material" defines, never filtering indices.
- **Covers:** Z2-I-ENV-01, Z2-I-ENV-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-079, Z2-D-014
- **Budgets:** none
- **Acceptance evidence:** `Test_TerrainMaterial_BlendMatchesCpuReference`, `Test_TerrainMaterial_SplatIndexDecodeExact`. Pending core-doc batch 4 (X146): the frame environment constants through which the material reaches the tile table.
- **Design:** Zenith2/Docs/Design/ENV/Terrain.md → "Layer blend in the terrain material"; Zenith2/Docs/Design/ENV/Terrain.md → "Terrain material module"

### Z2-R-ENV-040 Layers per pixel by tier

- **Statement:** The terrain material samples four layers on desktop tiers and two on S24 tiers through `uPERM_LAYERS`, selected by `environment.terrain.layersPerPixel`, and the two-layer path renormalises.
- **Covers:** Z2-I-ENV-01, Z2-I-ENV-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-014, Z2-D-035
- **Budgets:** none
- **Acceptance evidence:** `Test_TerrainMaterial_TwoLayerTierRenormalises`; the permutation count report of the cook.
- **Design:** Zenith2/Docs/Design/ENV/Terrain.md → "Layers per pixel by quality tier"

### Z2-R-ENV-041 Terrain material follows the material rules

- **Statement:** `Material_TerrainLayered` samples only through `Flux_SampleMaterial` with explicit gradients from the adapter's world-metre UVs, uses `half` for colour and weights, holds its layer textures as material texture fields of the per-terrain generated material, and reads the per-tile splat textures through the terrain's tile table of `Env_FrameEnvironment`, which ENV's GPU side replaces by reallocate-and-swap, and the terrain tile table binds a null detail-density handle while the density texture's `Flux_TextureResource::IsFallbackBound()` is true.
- **Covers:** Z2-I-ENV-01, Z2-I-ENV-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-014, Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_TerrainMaterial_ExplicitGradientsOnly` (ShaderChecks lint fixture), `Test_EnvRender_ShaderLintsClean` and `Test_EnvRender_TerrainTileTableFollowsSwap`. Pending core-doc batch 4 (X146).
- **Design:** Zenith2/Docs/Design/ENV/Terrain.md → "Terrain material module"

### Z2-R-ENV-043 Terrain activation through the render world

- **Statement:** A `Zenith_TerrainComponent` whose asset becomes active produces a terrain ADD record carrying the lowest free extractor slot, which ENV's GPU side turns into `Flux_TerrainRenderer::AddTerrain`, reading GEOM's record index with `GetRecordIndex` right after `AddTerrain` returns to place the terrain's GPU record and slot-table entry, and the component's removal produces REMOVE, which frees both; the extractor derives ADD and REMOVE by comparing the live terrains with its own bookkeeping, never by draining a phase-1 queue, and keeps a refused record pending for the next frame.
- **Covers:** Z2-I-ENV-01, Z2-I-ENV-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-050, Z2-D-058
- **Budgets:** none
- **Acceptance evidence:** `Test_TerrainRuntime_TerrainAddRemoveRecords`, `Test_EnvironmentExtract_RecordsIndependentOfTickCount` and `Test_EnvironmentExtract_RefusedRecordRetried` in `ZenithEnvironment_Tests`; `Test_EnvRender_TwoTerrainsRecordsEitherOrder` and `Test_EnvRender_TerrainSplatCrossBackend`. The records travel through core-doc item X156 (pending core-doc batch 4). The GEOM dependency (row EN21, Q52 ruled (a)) is satisfied by GEOM's landed text (row GM31): `Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain record constant"`; the ticket is `blocks`-linked only behind the GEOM ticket that implements it.
- **Design:** Zenith2/Docs/Design/ENV/Terrain.md → "Zenith_TerrainRuntime header"; Zenith2/Docs/Design/ENV/Terrain.md → "Terrain record slots"; Zenith2/Docs/Design/ENV.md → "Render-world records"; Zenith2/Docs/Design/ENV.md → "Record writer"; Zenith2/Docs/Design/ENV/Atmosphere.md → "Extraction steps"

### Z2-R-ENV-044 Terrain kind loader

- **Statement:** The `Flux.Terrain` kind loader in ENV's GPU side builds `Flux_TerrainDesc` from GEOM's sections and the material records from ENV's sections and exposes the ENV sections' CPU copies to the module.
- **Covers:** Z2-I-ENV-01, Z2-I-ENV-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-058
- **Budgets:** none
- **Acceptance evidence:** `Test_TerrainKindLoader_BuildsDescAndRecords` in `ZenithFlux_Tests`.
- **Design:** Zenith2/Docs/Design/ENV/Bake.md → "Terrain file"; Zenith2/Docs/Design/ENV.md → "ZenithFlux ENV files"

### Z2-R-ENV-045 Height query definition

- **Statement:** CPU height and normal queries interpolate the triangle of the quad split along its (s, t)–(s + 1, t + 1) diagonal with the fixed float operation order of "Triangle interpolation", bit-identical to the collision tile.
- **Covers:** Z2-I-ENV-01, Z2-I-ENV-11, Z2-I-ENV-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_TerrainRuntime_HeightMatchesCollisionTriangles`.
- **Design:** Zenith2/Docs/Design/ENV/Terrain.md → "Triangle interpolation"

### Z2-R-ENV-046 Height query residency errors

- **Statement:** A query outside every terrain returns `ENV_ERROR_OUT_OF_BOUNDS` and a query in a present but inactive tile returns `ENV_ERROR_NOT_RESIDENT` with its counter, never asserting.
- **Covers:** Z2-I-ENV-01, Z2-I-ENV-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_TerrainRuntime_NotResidentAndOutOfBounds`.
- **Design:** Zenith2/Docs/Design/ENV/Terrain.md → "Residency and tick-boundary activation"

### Z2-R-ENV-047 Tile activation at tick boundaries

- **Statement:** Tile CPU data becomes answerable only at the phase 1 following the tick boundary that publishes its asset load, so every worker of a tick sees the same tile set and a replay sees it at the same tick.
- **Covers:** Z2-I-ENV-01, Z2-I-ENV-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-053, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_TerrainRuntime_TileActivationAtTickBoundary`.
- **Design:** Zenith2/Docs/Design/ENV/Terrain.md → "Residency and tick-boundary activation"; Zenith2/Docs/Design/ENV/Terrain.md → "Zenith_TerrainRuntime header"

### Z2-R-ENV-050 Foliage type document

- **Statement:** The `*.foliage.json` document has the members and rules of "Foliage type document", and the cook refuses inconsistent render-only, collision and gameplay-visible combinations (`COOK_ERROR_IMPORT_FAILED` with an `ENV_ERROR_DOCUMENT_SCHEMA` diagnostic).
- **Covers:** Z2-I-ENV-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_FoliageType_DocumentValidation`, the foliage-type migration fixtures, `Fuzz_EnvironmentDocuments`.
- **Design:** Zenith2/Docs/Design/ENV/Foliage.md → "Foliage type document"

### Z2-R-ENV-051 Placed instances are cooked records

- **Statement:** Placed foliage instances are 32-byte `Zenith_FoliageInstance` records in cooked cells, not ECS entities, with rotations quantised through GEOM's exact kernel and records sorted by (type, key).
- **Covers:** Z2-I-ENV-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-058, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_FoliageInstance_LayoutIs32Bytes`, `Test_EnvironmentCook_ByteIdenticalAcrossWorkers`.
- **Design:** Zenith2/Docs/Design/ENV/Foliage.md → "Instance record layout"; Zenith2/Docs/Design/ENV/Foliage.md → "Foliage classes"

### Z2-R-ENV-052 Stable foliage render keys

- **Statement:** Each placed instance's render-instance key is `Zenith_FoliageRenderKey` of (world tag, cell, instance key), where the instance key is unique within the cell across every rule set and explicit instance, so render keys are identical across reloads of the cell and distinct between instances and cells.
- **Covers:** Z2-I-ENV-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-058, D-SEL-HASH (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_Foliage_RenderKeyStable`, `Test_Placement_MultipleRuleSets_KeysUniqueAndExcluded`.
- **Design:** Zenith2/Docs/Design/ENV/Foliage.md → "Render instance keys"; Zenith2/Docs/Design/ENV/Procedural.md → "Output ordering and capacity"

### Z2-R-ENV-053 Foliage activation into the GPU scene

- **Statement:** An active cell's drawn instances enter the GPU scene as ADD instance deltas, nearest to the environment anchor first and at most `environment.foliage.maxActivationsPerFrame` per frame, with translations rebased at extraction, and are culled, LOD-selected and drawn by the GPU-driven instance path.
- **Covers:** Z2-I-ENV-03, Z2-I-ENV-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-058, Z2-D-060, Z2-D-050
- **Budgets:** Z2-B-RPROD-001
- **Acceptance evidence:** `Test_Foliage_ActivationEmitsAddDeltas`, `Test_EnvRender_FoliageInstancesCulled`.
- **Design:** Zenith2/Docs/Design/ENV/Foliage.md → "Activation and deactivation"

### Z2-R-ENV-054 Foliage deactivation

- **Statement:** A cell that leaves the active set emits REMOVE for exactly the keys it emitted and releases its asset handle.
- **Covers:** Z2-I-ENV-03, Z2-I-ENV-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-058
- **Budgets:** none
- **Acceptance evidence:** `Test_Foliage_DeactivationRemovesEveryKey`.
- **Design:** Zenith2/Docs/Design/ENV/Foliage.md → "Activation and deactivation"

### Z2-R-ENV-055 Foliage full-state extraction

- **Statement:** A full-state extraction re-emits every drawn instance of every active cell as ADD without the per-frame limit, so a resync rebuilds the GPU scene's foliage exactly.
- **Covers:** Z2-I-ENV-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_Foliage_FullExtractionReemits`.
- **Design:** Zenith2/Docs/Design/ENV/Foliage.md → "Full-state extraction"

### Z2-R-ENV-056 Foliage extraction bookkeeping is presentation

- **Statement:** The foliage extractor's bookkeeping is owned by the module instance, written by the one extractor node, and excluded from world state hashes and saves.
- **Covers:** Z2-I-ENV-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_EnvironmentDeterminism_ReplayWorkerInvariant` (world hashes independent of extraction) and review.
- **Design:** Zenith2/Docs/Design/ENV/Foliage.md → "Activation and deactivation"

### Z2-R-ENV-057 Tier thinning of render-only foliage

- **Statement:** Render-only placed instances are drawn when their thinning rank is below the density scale's threshold, so lowering and raising the scale removes and restores the same instances in a fixed order.
- **Covers:** Z2-I-ENV-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-014
- **Budgets:** none
- **Acceptance evidence:** `Test_Foliage_ThinningMonotonic`.
- **Design:** Zenith2/Docs/Design/ENV/Foliage.md → "Tier thinning rule"

### Z2-R-ENV-058 Simulation-visible foliage ignores tiers

- **Statement:** Instances of collidable or gameplay-visible types are drawn whenever their cell is active, at every density scale and pressure level.
- **Covers:** Z2-I-ENV-03, Z2-I-ENV-11
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Foliage_SimulationVisibleIgnoresTier`, `Test_EnvironmentDeterminism_TierIndependent`.
- **Design:** Zenith2/Docs/Design/ENV/Foliage.md → "Render-only and simulation-visible instances"

### Z2-R-ENV-059 Instance cap and shedding

- **Statement:** When the placed-instance count would exceed `environment.foliage.instanceCap` or the GPU scene refuses an instance, only render-only instances are shed: the render-only instances kept are those of the lowest thinning ranks, so the highest ranks are shed first, and the shed count is reported.
- **Covers:** Z2-I-ENV-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** `Test_Foliage_InstanceCapShedsRenderOnlyFirst`, `Test_Foliage_TableFullLowersCap`.
- **Design:** Zenith2/Docs/Design/ENV/Foliage.md → "Instance cap and shedding"

### Z2-R-ENV-060 Foliage instance cap row

- **Statement:** The resident placed-instance cap is the pending row "Placed foliage instances resident cap", sized by W-Outdoor's 100k instances, and the configuration key's engine default is generated from it.
- **Covers:** Z2-I-ENV-03
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** pending row "Placed foliage instances resident cap"
- **Acceptance evidence:** The Stage 1 per-system budget ticket for ENV allocating the row (needs-human ruling) and the generated budget table. The configuration dump of `Test_EnvironmentConfig_KeysRegistered` shows the engine default of `environment.foliage.instanceCap` equal to the allocated row's value.
- **Design:** Zenith2/Docs/Design/ENV.md → "Pending row needs"; Zenith2/Docs/Design/ENV/Foliage.md → "Instance cap and shedding"

### Z2-R-ENV-061 Gameplay foliage queries

- **Statement:** `QueryInstances` returns gameplay-visible and collidable instances within a radius in (distance, cell, key) order, refuses render-only types, and reports a short output span with `ENV_ERROR_CAPACITY`.
- **Covers:** Z2-I-ENV-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Foliage_QueryInstancesSortedAndBounded`, `Test_Foliage_QueryRenderOnlyRefused`.
- **Design:** Zenith2/Docs/Design/ENV/Foliage.md → "Zenith_FoliageSystem header"

### Z2-R-ENV-062 Failed foliage cells stay empty

- **Statement:** A foliage cell whose asset fails to load stays empty, is counted in `environment.foliage.cells_failed`, and blocks no other cell.
- **Covers:** Z2-I-ENV-03, Z2-I-ENV-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_Foliage_FailedCellStaysEmpty`.
- **Design:** Zenith2/Docs/Design/ENV/Foliage.md → "Activation and deactivation"; Zenith2/Docs/Design/ENV.md → "Failure table"

### Z2-R-ENV-088 Atmosphere document

- **Statement:** The `*.atmosphere.json` document carries every value of LIGHT's media model plus the celestial, time-of-day and cloud settings, within the ranges of "Validation limits", and cooks to `.zatmos` in SI units.
- **Covers:** Z2-I-ENV-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_Atmosphere_ValidateLimits`, `Test_Atmosphere_ToFluxUnits`, `Test_EnvironmentCook_ParameterDocumentsRoundTrip`.
- **Design:** Zenith2/Docs/Design/ENV/Atmosphere.md → "Atmosphere document"; Zenith2/Docs/Design/ENV/Atmosphere.md → "Validation limits"; Zenith2/Docs/Design/ENV/Bake.md → "Atmosphere file"

### Z2-R-ENV-089 Atmosphere fallback

- **Statement:** A cooked atmosphere outside the validation limits is refused at load and the world uses LIGHT's defaults with `ENV_WARNING_ATMOSPHERE_FALLBACK`.
- **Covers:** Z2-I-ENV-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_Atmosphere_CorruptCookedFallsBack` in `ZenithEnvironment_Tests`.
- **Design:** Zenith2/Docs/Design/ENV/Atmosphere.md → "Validation limits"; Zenith2/Docs/Design/ENV.md → "Failure table"

### Z2-R-ENV-090 Atmosphere parameters feed LIGHT

- **Statement:** `Environment.Advance` fills the `Flux_EnvironmentBlocks` world resource each tick with the weather-modulated `Flux_AtmosphereParams` and `Flux_FogMediumParams` and a `m_uSkyRevision` raised on every change, and the engine environment extractor, the one writer of the render environment, copies them into LIGHT's atmosphere block; ENV calls no environment setter.
- **Covers:** Z2-I-ENV-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-050, Z2-D-080
- **Budgets:** Z2-B-ENV-001
- **Acceptance evidence:** `Test_Atmosphere_RenderBlocksWrittenInPhase1` in `ZenithEnvironment_Tests`; RPROD's `Test_EngineExtractor_EnvironmentSetOnce`; `Test_EnvRender_BackendParity`. Pending core-doc batch 4 (X125, ruled): Rendering's [Z2-N-REND-008] text and RPROD's engine extractor.
- **Design:** Zenith2/Docs/Design/ENV/Atmosphere.md → "Blocks ENV writes"; Zenith2/Docs/Design/ENV/Atmosphere.md → "Extraction steps"; Zenith2/Docs/Design/ENV.md → "Render-world records"

### Z2-R-ENV-093 Integer time-of-day clock

- **Statement:** The clock is integer microseconds of the day advanced every tick by the Bresenham rule of "Advance per tick", so exactly one day passes in `m_ulTicksPerGameDay` ticks with no drift.
- **Covers:** Z2-I-ENV-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_TimeOfDay_ExactDayAfterDTicks`, `Test_TimeOfDay_NoDriftOverLongRun`, `Test_TimeOfDay_FrozenWhenZeroDayLength`.
- **Design:** Zenith2/Docs/Design/ENV/Atmosphere.md → "Clock representation"; Zenith2/Docs/Design/ENV/Atmosphere.md → "Advance per tick"

### Z2-R-ENV-094 Time-of-day commands

- **Statement:** The set-time and set-scale commands, recorded in phases 2 to 10, take effect at the next tick's phase 1 in merge order and zero the remainder; an invalid value is dropped and counted.
- **Covers:** Z2-I-ENV-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_TimeOfDay_SetTimeAndScaleCommands`.
- **Design:** Zenith2/Docs/Design/ENV/Atmosphere.md → "Commands"

### Z2-R-ENV-095 Solar position

- **Statement:** The sun direction and elevation come from the declination, hour-angle and elevation formulas of "Solar position" in double precision, narrowed last, with markers read at the tick's clock.
- **Covers:** Z2-I-ENV-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Celestial_SolarPositionKnownValues`.
- **Design:** Zenith2/Docs/Design/ENV/Atmosphere.md → "Solar position"

### Z2-R-ENV-096 Moon and night sky

- **Statement:** The moon's direction, phase and illuminated fraction follow "Lunar position and phase", and the star field rotates about the celestial axis at the sidereal rate.
- **Covers:** Z2-I-ENV-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Celestial_MoonPhaseCycle`.
- **Design:** Zenith2/Docs/Design/ENV/Atmosphere.md → "Lunar position and phase"; Zenith2/Docs/Design/ENV/Atmosphere.md → "Night sky"

### Z2-R-ENV-097 Sun and moon lights

- **Statement:** When the settings drive them, ENV fills the render environment's explicit sun and moon slots with every member of the table "Sun and moon illuminance": LIGHT's named keys, `FLUX_LIGHT_DELTA_OP_UPDATE`, the directional type, lighting-channel mask 0x01, volumetric scale 128, the slot flag contract's bits (the sun `FLUX_LIGHT_FLAG_CASTS_SHADOW`, `FLUX_LIGHT_FLAG_CONTACT_SHADOWS` and `FLUX_LIGHT_FLAG_VOLUMETRIC`; the moon `FLUX_LIGHT_FLAG_VOLUMETRIC` plus `FLUX_LIGHT_FLAG_CASTS_SHADOW` above 1 lux) with every other flag bit zero, the bodies' angular radii as source radii, the sun's normalised colour and the moon's white as tints, LIGHT's shadow defaults, top-of-atmosphere intensities and directions at the tick's clock and zero intensity below the horizon; no slot when the settings disable them; the role of each body is its slot; with both slots filled a scene directional light is dropped by LIGHT's overflow and ENV warns once per world.
- **Covers:** Z2-I-ENV-09, Z2-I-ENV-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_Atmosphere_SunMoonSlots` (every member, exact flag words 0x7, 0x4 and 0x5, both slots accepted by `Flux_ValidateEnvironmentLight` with no flag cleared), `Test_Atmosphere_SceneDirectionalLightYields` (WARP, with LIGHT's light list).
- **Design:** Zenith2/Docs/Design/ENV/Atmosphere.md → "Sun and moon illuminance"

### Z2-R-ENV-098 Time-of-day events

- **Statement:** Crossing an hour, sunrise, sunset, dawn or dusk emits one `Zenith_TimeOfDayEvent` per marker in time order, delivered in phase 9.
- **Covers:** Z2-I-ENV-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_TimeOfDay_MarkersInOrder`.
- **Design:** Zenith2/Docs/Design/ENV/Atmosphere.md → "Markers and events"

### Z2-R-ENV-099 Clock state saved

- **Statement:** The clock (time of day, day, ticks per day, remainder) is a reflected property of the environment state saved and restored with the world, so a restored run continues the same sequence.
- **Covers:** Z2-I-ENV-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_TimeOfDay_SaveRestore`.
- **Design:** Zenith2/Docs/Design/ENV.md → "Zenith_EnvironmentState header"

### Z2-R-ENV-116 Placement rule document

- **Statement:** The `*.placement.json` document has the members and ranges of "Placement rule document", each rule's `layers` naming terrain palette layer IDs that `Zenith_Place` resolves against the desc's layer palette, and invalid rules or unknown layers are refused with `ENV_ERROR_PLACEMENT_RULE_INVALID` and the rule or stage index.
- **Covers:** Z2-I-ENV-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048, D-SEL-HASH (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_Placement_RuleValidation`, `Test_Placement_RuleLayerResolved`, the placement migration fixtures, `Fuzz_EnvironmentDocuments`.
- **Design:** Zenith2/Docs/Design/ENV/Procedural.md → "Placement rule document"; Zenith2/Docs/Design/ENV/Procedural.md → "Surface filters"; Zenith2/Docs/Design/ENV/Procedural.md → "Placement value types"

### Z2-R-ENV-117 Seeded placement streams

- **Statement:** Every placement draw is a counter-based draw of `Zenith_RandomSource` keyed by (seed, rule stable ID, cell, candidate, k) with the counter set explicitly, per "Seeded streams".
- **Covers:** Z2-I-ENV-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, D-SEL-HASH (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_Placement_CellSubsetMatchesFullRun`.
- **Design:** Zenith2/Docs/Design/ENV/Procedural.md → "Seeded streams"

### Z2-R-ENV-118 Candidate generation

- **Statement:** Candidates are jittered points of a lattice of spacing 1/sqrt(density) anchored at the grid origin, indexed row-major per cell.
- **Covers:** Z2-I-ENV-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, D-SEL-HASH (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_Placement_CanonicalOrder`.
- **Design:** Zenith2/Docs/Design/ENV/Procedural.md → "Candidate generation"

### Z2-R-ENV-119 Rule filters and acceptance

- **Statement:** Candidates pass the surface filters in the stated order, then the density-map and noise acceptance draw, with each rejection counted under its first failing filter.
- **Covers:** Z2-I-ENV-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, D-SEL-HASH (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_Placement_FiltersApplyInOrder`, `Test_Placement_DensityMapAcceptance`.
- **Design:** Zenith2/Docs/Design/ENV/Procedural.md → "Surface filters"; Zenith2/Docs/Design/ENV/Procedural.md → "Density acceptance"; Zenith2/Docs/Design/ENV/Procedural.md → "Noise modulation"

### Z2-R-ENV-120 Spacing and exclusion

- **Statement:** Survivors of one rule are spaced at least `minDistance` apart by the local-priority rule with the symmetric float distance test, decided among the survivors before any cross-rule exclusion; a stage with `excludeEarlierRules` then removes the spaced instances near the kept instances of every earlier stage of every rule set and near the obstacles; and authored shapes and the masks of the candidate's rule set or ALL reject candidates.
- **Covers:** Z2-I-ENV-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, D-SEL-HASH (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_Placement_MinDistanceHeld`, `Test_Placement_CrossRuleExclusion`, `Test_Placement_MultipleRuleSets_KeysUniqueAndExcluded`, `Test_Placement_CellSubsetMatchesFullRun` (a removed survivor still suppresses a neighbour across a cell border).
- **Design:** Zenith2/Docs/Design/ENV/Procedural.md → "Same-rule spacing"; Zenith2/Docs/Design/ENV/Procedural.md → "Cross-rule exclusion"; Zenith2/Docs/Design/ENV/Procedural.md → "Authored exclusion shapes"

### Z2-R-ENV-121 Placement is worker and order invariant

- **Statement:** `Zenith_Place` produces bit-identical output for 1, 2, 8 and all workers and for any order and subset of requested cells, by the ring rule and the stage order of "Cross-cell determinism".
- **Covers:** Z2-I-ENV-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, D-SEL-HASH (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_Placement_WorkerCountInvariant`, `Test_Placement_CellSubsetMatchesFullRun`.
- **Design:** Zenith2/Docs/Design/ENV/Procedural.md → "Cross-cell determinism"; Zenith2/Docs/Design/ENV/Procedural.md → "Determinism argument"

### Z2-R-ENV-122 Placement output order and capacity

- **Statement:** Each cell's instances are sorted by the instance key `(rule set << 27) | (rule << 21) | candidate`, explicit instances carrying bit 31, and a cell over its cap returns `ENV_ERROR_PLACEMENT_CAPACITY` with its first instances, failing the cook.
- **Covers:** Z2-I-ENV-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, D-SEL-HASH (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_Placement_CanonicalOrder`, `Test_Placement_CapacityError`, `Test_Placement_MaxDensity_Stress`.
- **Design:** Zenith2/Docs/Design/ENV/Procedural.md → "Output ordering and capacity"

### Z2-R-ENV-123 One placement implementation

- **Statement:** The cook, the editor preview, the scatter brush and runtime callers all use `Zenith_Place`, so the preview equals the cooked result.
- **Covers:** Z2-I-ENV-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-079, D-SEL-HASH (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_Placement_EditorPreviewEqualsCook`.
- **Design:** Zenith2/Docs/Design/ENV/Procedural.md → "Runtime and editor use"

### Z2-R-ENV-124 Runtime reproducible placement for WORLD

- **Statement:** A runtime caller placing cells from the world seed over the runtime terrain resource gets the same instances on every run of one build, which WORLD's reproducible generation orchestrates.
- **Covers:** Z2-I-ENV-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, D-SEL-HASH (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_Placement_RuntimeSurfaceReproducible` in `Zenith_Integration_Environment` (two runs, identical output).
- **Design:** Zenith2/Docs/Design/ENV/Procedural.md → "Runtime and editor use"; Zenith2/Docs/Design/ENV/Procedural.md → "Boundary with WORLD procedural generation"

### Z2-R-ENV-135 Cooked container and sections

- **Statement:** Every ENV cooked file (`.zterrain`, `.zenvtile`, `.zfoliage`, `.zatmos`, `.zweather`, `.zwater`, `.zcloudnoise`) uses ASSET's container with the magics, format versions and section kinds of the registry, little-endian and written field by field.
- **Covers:** Z2-I-ENV-01, Z2-I-ENV-02, Z2-I-ENV-03, Z2-I-ENV-04, Z2-I-ENV-05, Z2-I-ENV-06, Z2-I-ENV-07, Z2-I-ENV-08, Z2-I-ENV-09, Z2-I-ENV-10, Z2-I-ENV-11, Z2-I-ENV-12, Z2-I-ENV-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_EnvironmentCook_TerrainSectionsComplete`, `Test_EnvironmentCook_ParameterDocumentsRoundTrip`.
- **Design:** Zenith2/Docs/Design/ENV/Bake.md → "Section kind registry"; Zenith2/Docs/Design/ENV.md → "ENV cooked file header"; Zenith2/Docs/Design/ENV.md → "Byte order and serialization rules"

### Z2-R-ENV-136 Cook determinism

- **Statement:** Every ENV cook output is byte-identical for identical inputs regardless of the cook's worker count and step order.
- **Covers:** Z2-I-ENV-01, Z2-I-ENV-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_EnvironmentCook_ByteIdenticalAcrossWorkers`.
- **Design:** Zenith2/Docs/Design/ENV/Bake.md → "Cook determinism"

### Z2-R-ENV-137 Derived-data keys

- **Statement:** The terrain importer's key covers the terrain document, every document it reads through `ReadInput` (tiles, layers, foliage types, placement rule sets, exclusion sources) and `uENV_TERRAIN_IMPORTER_VERSION`, which folds GEOM's terrain cook version, the placement algorithm version and ENV's stages version; each ENV step's key covers its product's intermediates; so an unchanged product of a re-imported terrain re-cooks from the DDC.
- **Covers:** Z2-I-ENV-01, Z2-I-ENV-02, Z2-I-ENV-03, Z2-I-ENV-04, Z2-I-ENV-05, Z2-I-ENV-06, Z2-I-ENV-07, Z2-I-ENV-08, Z2-I-ENV-09, Z2-I-ENV-10, Z2-I-ENV-11, Z2-I-ENV-12, Z2-I-ENV-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-065
- **Budgets:** none
- **Acceptance evidence:** `Test_EnvironmentCook_DerivedDataKeyCoversInputs` (each declared input, a halo tile, a rule, a foliage type, the platform and each folded version changes the key; an undeclared change does not), `Test_EnvironmentCook_IncrementalEqualsClean`.
- **Design:** Zenith2/Docs/Design/ENV.md → "Cook steps and derived-data keys"; Zenith2/Docs/Design/ENV/Bake.md → "Step graph"

### Z2-R-ENV-138 Readers validate untrusted bytes

- **Statement:** Every ENV reader validates the container, sizes, counts, floats, ranges and indices before use and fails with a recoverable error naming the section, never an assert, and each has a fuzz target.
- **Covers:** Z2-I-ENV-01, Z2-I-ENV-02, Z2-I-ENV-03, Z2-I-ENV-04, Z2-I-ENV-05, Z2-I-ENV-06, Z2-I-ENV-07, Z2-I-ENV-08, Z2-I-ENV-09, Z2-I-ENV-10, Z2-I-ENV-11, Z2-I-ENV-12, Z2-I-ENV-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_EnvironmentReaders_RejectOverLimitCounts`, the ENV fuzz targets (smoke in `win-asan`, full duration at acceptance).
- **Design:** Zenith2/Docs/Design/ENV/Bake.md → "Reader validation"; Zenith2/Docs/Design/ENV/Tests.md → "Fuzz targets"

### Z2-R-ENV-139 Mismatched cooked versions refused

- **Statement:** An ENV reader refuses a file of another format version or engine cook version with a recoverable asset error, and any engine version change re-cooks ENV content.
- **Covers:** Z2-I-ENV-01, Z2-I-ENV-02, Z2-I-ENV-03, Z2-I-ENV-04, Z2-I-ENV-05, Z2-I-ENV-06, Z2-I-ENV-07, Z2-I-ENV-08, Z2-I-ENV-09, Z2-I-ENV-10, Z2-I-ENV-11, Z2-I-ENV-12, Z2-I-ENV-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EnvironmentReaders_RejectMismatchedVersion`.
- **Design:** Zenith2/Docs/Design/ENV.md → "Migration and re-cook"; Zenith2/Docs/Design/ENV.md → "ENV cooked file header"

### Z2-R-ENV-140 Terrain geometry hand-off

- **Statement:** The terrain importer's `Environment.TerrainGeometry` stage calls GEOM's `Zenith_CookTerrainGeometry` with exactly one tile per grid cell (an absent cell synthesised from the present tiles' shared samples and `m_fHeightMin`, every one of its quads counted as a hole), each tile's 257 × 257 hole samples converted from the authored hole quads and absent cells, and the height and hole product GUIDs the importer declared, and writes the returned sections unchanged into the terrain file and the returned samples as the source images of the height and hole textures, written with `Zenith_WriteTextureSource` as `geom.textureSource` blobs, so ENV's tile heights equal GEOM's texels.
- **Covers:** Z2-I-ENV-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_EnvironmentCook_HeightsMatchGeomTiles`, `Test_EnvironmentCook_HoleMaskConversion` (including the absent-neighbour case), `Test_EnvironmentCook_AbsentCellsPassedDense`, GEOM's `Test_CookTerrain_TilesAndPyramid` (`Zenith2/Docs/Design/GEOM.md`). The GEOM dependency (row EN21, Q52 ruled (a)) is satisfied by GEOM's landed text (row GM31): `Zenith2/Docs/Design/GEOM/Textures.md → "Texture source image intermediate"`; the ticket is `blocks`-linked only behind the GEOM ticket that implements it.
- **Design:** Zenith2/Docs/Design/ENV/Bake.md → "Geometry hand-off to GEOM"; Zenith2/Docs/Design/ENV/Bake.md → "Terrain file"

### Z2-R-ENV-143 Environment counters

- **Statement:** ENV publishes every counter of "Counters" with its stated kind and naming.
- **Covers:** Z2-I-ENV-01, Z2-I-ENV-02, Z2-I-ENV-03, Z2-I-ENV-04, Z2-I-ENV-05, Z2-I-ENV-06, Z2-I-ENV-07, Z2-I-ENV-08, Z2-I-ENV-09, Z2-I-ENV-10, Z2-I-ENV-11, Z2-I-ENV-12, Z2-I-ENV-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_EnvironmentCounters_Emitted` (a recording hook table sees every counter in a scripted scene).
- **Design:** Zenith2/Docs/Design/ENV.md → "Counters"

### Z2-R-ENV-144 On-screen environment panel

- **Statement:** The on-screen profiler shows the Environment panel of "On-screen profiler rows" in game and editor on Windows and the S24, labelled with the rows it feeds.
- **Covers:** Z2-I-ENV-01, Z2-I-ENV-02, Z2-I-ENV-03, Z2-I-ENV-04, Z2-I-ENV-05, Z2-I-ENV-06, Z2-I-ENV-07, Z2-I-ENV-08, Z2-I-ENV-09, Z2-I-ENV-10, Z2-I-ENV-11, Z2-I-ENV-12, Z2-I-ENV-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_EnvironmentProfilerPanel_Rows` (panel data model) and the S24 device check of the profiler (needs-human device step).
- **Design:** Zenith2/Docs/Design/ENV.md → "On-screen profiler rows"; Zenith2/Docs/Design/ENV.md → "Profiler zones"

### Z2-R-ENV-146 Environment log categories

- **Statement:** ENV logs through its categories, each warning once per asset or document (per world for the anchor and scene-sun warnings) per run with the GUID, named by `Zenith_ToString` of its `Zenith_EnvironmentWarning` value.
- **Covers:** Z2-I-ENV-01, Z2-I-ENV-02, Z2-I-ENV-03, Z2-I-ENV-04, Z2-I-ENV-05, Z2-I-ENV-06, Z2-I-ENV-07, Z2-I-ENV-08, Z2-I-ENV-09, Z2-I-ENV-10, Z2-I-ENV-11, Z2-I-ENV-12, Z2-I-ENV-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_EnvironmentLog_WarningsOncePerAsset`.
- **Design:** Zenith2/Docs/Design/ENV.md → "Log categories"

### Z2-R-ENV-147 Cook report members

- **Statement:** The cook report carries the `environment` members of "Cook report members".
- **Covers:** Z2-I-ENV-01, Z2-I-ENV-02, Z2-I-ENV-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-065
- **Budgets:** none
- **Acceptance evidence:** `Test_EnvironmentCook_ReportMembers`.
- **Design:** Zenith2/Docs/Design/ENV.md → "Cook report members"

### Z2-R-ENV-149 Pending ENV rows allocated

- **Statement:** The Stage 1 per-system budget ticket for ENV allocates `Z2-B-ENV-001` and the pending rows of "Pending row needs" that it produces, and every ENV ticket citing a pending row is blocked until then.
- **Covers:** Z2-I-ENV-01, Z2-I-ENV-02, Z2-I-ENV-03, Z2-I-ENV-04, Z2-I-ENV-05, Z2-I-ENV-06, Z2-I-ENV-07, Z2-I-ENV-08, Z2-I-ENV-09, Z2-I-ENV-10, Z2-I-ENV-11, Z2-I-ENV-12, Z2-I-ENV-13
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-012, Z2-D-084
- **Budgets:** Z2-B-ENV-001
- **Acceptance evidence:** The ruling ticket's diff: the Budgets.md rows, `allocation.json` and the DecisionLog entry, with `Check-Allocation.ps1` exit 0.
- **Design:** Zenith2/Docs/Design/ENV.md → "Pending row needs"; Zenith2/Docs/Design/ENV.md → "Rows owned"; Zenith2/Docs/Design/ENV.md → "Stage table"

### Z2-R-ENV-150 ENV rows hold in W-Outdoor at Stage 3

- **Statement:** In the Stage 3 W-Outdoor run, ENV's phase-1 nodes and extractors stay within the pending rows "Environment update CPU" and "Environment extraction CPU", and the sky passes within `Z2-B-ENV-001`, with terrain, placed foliage, atmosphere and time of day active.
- **Covers:** Z2-I-ENV-01, Z2-I-ENV-03, Z2-I-ENV-06, Z2-I-ENV-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-ENV-001; Z2-B-RPROD-003; pending row "Environment update CPU"; pending row "Environment extraction CPU"
- **Acceptance evidence:** The W-Outdoor measurement of the Stage 3 acceptance with MP-CPUZONE and MP-GPUPASS.
- **Design:** Zenith2/Docs/Design/ENV.md → "Pending row needs"; Zenith2/Docs/Design/ENV.md → "Profiler zones"

### Z2-R-ENV-152 W-Outdoor evidence

- **Statement:** W-Outdoor v1 (4 km² of terrain, 100k foliage instances, a 2 km view, 10 m/s traversal) meets its frame-time and hitch rows on the reference desktop (both backends) and the S24 at the Stage 3 acceptance, where the WorkloadOutdoor game does not select the Streaming module (ruling Q73), and again with the Stage 4 features at the Stage 4 acceptance; the streaming rows (`Z2-B-STREAM-001` for ENV's tile and foliage cell data) are first confirmed at Stage 4 by Z2-R-ENV-151.
- **Covers:** Z2-I-ENV-01, Z2-I-ENV-03, Z2-I-ENV-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012, Z2-D-021
- **Budgets:** Z2-B-ENV-001; Z2-B-GEOM-002
- **Acceptance evidence:** The W-Outdoor evidence records of the Stage 3 acceptance and the Stage 4 acceptance (needs-human device steps).
- **Design:** Zenith2/Docs/Design/ENV/Tests.md → "Workload and device evidence"

### Z2-R-ENV-153 W-Outdoor authored in Stage 3

- **Statement:** The requirements of Z2-I-ENV-01, -03, -06, -09 and -10 carry fix version Z2 Stage 3 and land in the Stage 3 sprint, and the WorkloadOutdoor game ticket of Stage 3 deliverable 9 (fix version Z2 Stage 3) is `blocks`-linked from their tickets, so the workload is authored and measured in Stage 3.
- **Covers:** Z2-I-ENV-01, Z2-I-ENV-03, Z2-I-ENV-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-070, Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** The Stage 3 WorkloadOutdoor ticket (fix version `Z2 Stage 3`, sprint `Stage 3 — Playable authoring slice`) is `blocks`-linked from the ENV tickets of those items; pending core-doc batch 4 (Q45) for the Inventory rows.
- **Design:** Zenith2/Docs/Design/ENV.md → "Stage table"

### Z2-R-ENV-160 Terrain LOD distance ratio at least 8

- **Statement:** A terrain's `lodDistanceRatio` defaults to 8 and lies in [8, 64]: `Create` refuses a lower value with `ENV_ERROR_INVALID_DESC`, `Load` with `ENV_ERROR_DOCUMENT_SCHEMA` naming the field, and the cook writes it into GEOM's section 0x0101, so no ENV terrain is refused by `AddTerrain`.
- **Covers:** Z2-I-ENV-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_TerrainDocument_Create_RejectsInvalidDesc` (ratio 7.9), `Test_TerrainDocument_Load_LODRatioBelowEightRejected`, GEOM's `Test_Terrain_LODRatioBelowEightRejected`.
- **Design:** Zenith2/Docs/Design/ENV/Terrain.md → "Zenith_TerrainDocument header"; Zenith2/Docs/Design/ENV/Bake.md → "Terrain document schema"

### Z2-R-ENV-161 Placement keys include the rule set

- **Statement:** A placed instance's key has disjoint bit fields for the rule set (27–30), the rule (21–26) and the candidate (0–20) and an explicit instance's key sets bit 31 over an ID below 2^31, so keys never collide within a cell across the terrain's rule sets.
- **Covers:** Z2-I-ENV-10, Z2-I-ENV-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, D-SEL-HASH (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_Placement_MultipleRuleSets_KeysUniqueAndExcluded` (two rule sets with equal rule names in one cell).
- **Design:** Zenith2/Docs/Design/ENV/Procedural.md → "Output ordering and capacity"; Zenith2/Docs/Design/ENV/Procedural.md → "Zenith_ProceduralPlacement header"

### Z2-R-ENV-162 Cross-rule-set exclusion and per-set masks

- **Statement:** One `Zenith_Place` run evaluates every rule set of the terrain as stages ordered by (set, rule), an excluding stage excludes the kept instances of all earlier stages of every set, and `Zenith_PlacementSurface::Sample` takes the rule-set index and returns the larger of that set's mask and the ALL mask, rejecting at 0.5.
- **Covers:** Z2-I-ENV-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, D-SEL-HASH (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_Placement_MultipleRuleSets_KeysUniqueAndExcluded`, `Test_Placement_RuleSetExclusionMask`.
- **Design:** Zenith2/Docs/Design/ENV/Procedural.md → "Cross-rule exclusion"; Zenith2/Docs/Design/ENV/Procedural.md → "Surface filters"; Zenith2/Docs/Design/ENV/Procedural.md → "Placement value types"

### Z2-R-ENV-163 Exclusion chain bounded

- **Statement:** At most 8 stages of one placement run set `excludeEarlierRules` (a ninth is refused with `ENV_ERROR_PLACEMENT_RULE_INVALID` naming its global stage), and each stage computes survivors and kept decisions only on the rings of "Cross-cell determinism", so a single-cell run touches at most 361 cells per stage.
- **Covers:** Z2-I-ENV-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, D-SEL-HASH (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_Placement_ExcludingStageLimit`, `Test_Placement_SingleCellRingBound` (a counting surface records at most 361 cells per stage).
- **Design:** Zenith2/Docs/Design/ENV/Procedural.md → "Cross-rule exclusion"; Zenith2/Docs/Design/ENV/Procedural.md → "Cross-cell determinism"

### Z2-R-ENV-164 Scatter brush keeps footprints

- **Statement:** Every instance a scatter stroke adds lies at least `footprint(new) + footprint(existing)` from every pre-existing explicit instance and at least `min(2 × footprint, 64)` from every other new instance of the stroke, gets the next ID of its tile, and a dab that would exceed the per-tile limit or the 2^31 ID range fails the stroke with `ENV_ERROR_CAPACITY` and restores the pre-stroke document.
- **Covers:** Z2-I-ENV-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_TerrainBrush_ScatterFoliage_KeepsFootprints`, `Test_TerrainBrush_ScatterFoliage_CapacityRestores`.
- **Design:** Zenith2/Docs/Design/ENV/Terrain.md → "Scatter brush"

### Z2-R-ENV-165 Environment blocks resource

- **Statement:** `Flux_EnvironmentBlocks`, declared in `ZenithFlux/Flux_Environment.h`, is the `PHASE_INPUT` world resource "Zenith.Environment.RenderBlocks" with no serialized property, written by `Environment.Advance` alone every tick and read by the engine environment extractor after the phase boundary, holding LIGHT's types for the atmosphere, fog, stars, cloud shadow and local fog volumes and ENV's clouds, wind, weather and tick rate, with its defaults when `m_bValid` is false.
- **Covers:** Z2-I-ENV-06, Z2-I-ENV-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-050, Z2-D-080, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Atmosphere_RenderBlocksWrittenInPhase1` (the resource changes only in phase 1; a phase-12 write is a schedule error), `Test_EnvironmentDeterminism_ReplayWorkerInvariant` (the resource is identical per tick across workers), RPROD's `Test_EngineExtractor_EnvironmentSetOnce`. Pending core-doc batch 4 (X125, ruled).
- **Design:** Zenith2/Docs/Design/ENV.md → "Render-world records"; Zenith2/Docs/Design/ENV/Atmosphere.md → "Blocks ENV writes"

### Z2-R-ENV-166 Sun and moon slots and scene lights

- **Statement:** The sun and moon occupy the explicit slots of the render environment whenever the settings drive them, keep their slots at zero intensity below the horizon, and never yield a slot to a scene directional light.
- **Covers:** Z2-I-ENV-09, Z2-I-ENV-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_Atmosphere_SunMoonSlots`, `Test_Atmosphere_SceneDirectionalLightYields`. Pending core-doc batch 4 (X125, ruled).
- **Design:** Zenith2/Docs/Design/ENV/Atmosphere.md → "Sun and moon illuminance"

### Z2-R-ENV-172 Terrain materials generated per terrain

- **Statement:** The terrain cook generates the terrain's `material:terrain` sub-asset (`Material_TerrainLayered`, its 48 layer textures as texture fields and a zero `m_uTerrainRecord` placeholder, never a runtime index) from the layer documents, and `Flux_TerrainDesc::m_xMaterial` names it; the detail material is Z2-R-ENV-182's.
- **Covers:** Z2-I-ENV-01, Z2-I-ENV-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-079, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_EnvironmentCook_TerrainMaterialsGenerated`, `Test_TerrainKindLoader_BuildsDescAndRecords`. Pending core-doc batch 4 (X146). The GEOM dependency (row EN21, Q52 ruled (a)) is satisfied by GEOM's landed text (row GM31): `Zenith2/Docs/Design/GEOM/Materials.md → "Material document intermediate"`, `Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain record constant"`; the ticket is `blocks`-linked only behind the GEOM ticket that implements it.
- **Design:** Zenith2/Docs/Design/ENV/Terrain.md → "Terrain material module"; Zenith2/Docs/Design/ENV/Bake.md → "Step graph"

### Z2-R-ENV-174 No ENV GPU cost without the module

- **Statement:** A game whose `game.json` does not select `Environment` creates no `Flux_EnvironmentRenderer`, registers no detail source, packages no ENV shader blob and precaches no ENV pipeline, and a game with the module reports its ENV variant count in the cook report.
- **Covers:** Z2-I-ENV-01, Z2-I-ENV-02, Z2-I-ENV-03, Z2-I-ENV-04, Z2-I-ENV-05, Z2-I-ENV-06, Z2-I-ENV-07, Z2-I-ENV-08, Z2-I-ENV-09, Z2-I-ENV-10, Z2-I-ENV-11, Z2-I-ENV-12, Z2-I-ENV-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-024, Z2-D-014
- **Budgets:** none
- **Acceptance evidence:** `Test_EnvironmentModule_NotSelectedNoGpuSide` (a fixture game without the module: no renderer object, no ENV content in its package listing), `Test_EnvironmentCook_ReportMembers`.
- **Design:** Zenith2/Docs/Design/ENV.md → "ZenithFlux ENV files"; Zenith2/Docs/Design/ENV.md → "Cook report members"

### Z2-R-ENV-175 Presentation keys follow tier changes

- **Statement:** The module re-reads its `environment.*` keys in its configuration change callback at every quality-tier change and user commit and publishes them for the next frame, and nodes and extractors read only the published copy.
- **Covers:** Z2-I-ENV-03, Z2-I-ENV-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-014
- **Budgets:** none
- **Acceptance evidence:** `Test_EnvironmentConfig_TierChangeRepublishes`.
- **Design:** Zenith2/Docs/Design/ENV.md → "Configuration keys"

### Z2-R-ENV-177 Environment anchor

- **Statement:** The entity carrying `Zenith_EnvironmentAnchorComponent` is ENV's presentation focus for foliage activation order, detail cells, effect prefabs (whose transforms follow it by design), local fog volumes, local wind sources, submerged-view priority and the cloud-shadow square; without one the world origin is used with one warning per world; the environment state and every terrain, water and foliage query answer are identical with and without it.
- **Covers:** Z2-I-ENV-03, Z2-I-ENV-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-020
- **Budgets:** none
- **Acceptance evidence:** `Test_EnvironmentAnchor_FocusAndFallback`, `Test_EnvironmentDeterminism_ReplayWorkerInvariant`.
- **Design:** Zenith2/Docs/Design/ENV.md → "Zenith_EnvironmentComponents header"; Zenith2/Docs/Design/ENV/Foliage.md → "Activation and deactivation"

### Z2-R-ENV-179 Environment document types registered tool-side

- **Statement:** ZenithCook and ZenithEditor register ENV's document types into SCENE's `Zenith_MigrationRegistry` before `Freeze` through `Zenith_RegisterEnvironmentDocumentTypes` (from Stage 3 the terrain, tile, layer, foliage type, placement and atmosphere types; the weather, water and cloud-noise types with their Stage 4 introduction), with the stable names, suffixes, ID keys, version and canonical schemas of "Environment document types"; ZenithEnvironment declares no dependency on `ZenithDocument` and registers no document type, and `Zenith_TerrainDocument::Save` writes bytes that are canonical under the terrain and tile schemas.
- **Covers:** Z2-I-ENV-01, Z2-I-ENV-03, Z2-I-ENV-06, Z2-I-ENV-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_EnvironmentDocumentTypes_Registered` and `Test_EnvironmentDocumentTypes_TerrainSaveIsCanonical` in `ZenithCook_Tests`; `Libgraph` (no `ZenithDocument` edge of ZenithEnvironment). ASSET's registered-types table lists the row (row EN25).
- **Design:** Zenith2/Docs/Design/ENV/Bake.md → "Environment document types"; Zenith2/Docs/Design/ENV.md → "Zenith_EnvironmentDocuments header"; Zenith2/Docs/Design/ENV/Terrain.md → "Zenith_TerrainDocument header"

### Z2-R-ENV-180 Runtime objects of the Stage 3 kinds

- **Statement:** The runtime objects of kinds 4, 23, 24 and 25 are `Flux_TerrainResource`, `Zenith_TerrainTileAsset`, `Zenith_FoliageCellAsset` and `Zenith_AtmosphereAsset` with exactly the accessors of "Runtime asset objects", built by their kind loaders from the validated sections decoded once field by field, immutable after load and read in place while a handle is held, and their names match ASSET's kind table.
- **Covers:** Z2-I-ENV-01, Z2-I-ENV-03, Z2-I-ENV-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_EnvironmentAssets_AccessorsMatchSections` (the tile, foliage-cell and atmosphere cases) in `ZenithEnvironment_Tests`, `Test_TerrainKindLoader_BuildsDescAndRecords`; `ApiContractCheck` over `Zenith_EnvironmentAssets.h` and `Flux_Environment.h`.
- **Design:** Zenith2/Docs/Design/ENV/Bake.md → "Runtime asset objects"; Zenith2/Docs/Design/ENV/Bake.md → "Terrain file"

## Coverage of inventory items

Requirements of either catalogue file; a requirement of Stage 4, 5 or 6 is in `Zenith2/Docs/Requirements/ENV/Stage4Plus.md`.

| Item | Text | Requirements |
|---|---|---|
| Z2-I-ENV-01 | terrain sculpting | Z2-R-ENV-001, Z2-R-ENV-004, Z2-R-ENV-005, Z2-R-ENV-006, Z2-R-ENV-009, Z2-R-ENV-011, Z2-R-ENV-012, Z2-R-ENV-013, Z2-R-ENV-014, Z2-R-ENV-015, Z2-R-ENV-016, Z2-R-ENV-017, Z2-R-ENV-018, Z2-R-ENV-019, Z2-R-ENV-020, Z2-R-ENV-021, Z2-R-ENV-022, Z2-R-ENV-023, Z2-R-ENV-024, Z2-R-ENV-025, Z2-R-ENV-026, Z2-R-ENV-027, Z2-R-ENV-031, Z2-R-ENV-032, Z2-R-ENV-033, Z2-R-ENV-034, Z2-R-ENV-035, Z2-R-ENV-036, Z2-R-ENV-037, Z2-R-ENV-038, Z2-R-ENV-040, Z2-R-ENV-041, Z2-R-ENV-043, Z2-R-ENV-044, Z2-R-ENV-045, Z2-R-ENV-046, Z2-R-ENV-047, Z2-R-ENV-135, Z2-R-ENV-136, Z2-R-ENV-137, Z2-R-ENV-138, Z2-R-ENV-139, Z2-R-ENV-140, Z2-R-ENV-143, Z2-R-ENV-144, Z2-R-ENV-145, Z2-R-ENV-146, Z2-R-ENV-147, Z2-R-ENV-149, Z2-R-ENV-150, Z2-R-ENV-152, Z2-R-ENV-153, Z2-R-ENV-158, Z2-R-ENV-160, Z2-R-ENV-171, Z2-R-ENV-172, Z2-R-ENV-173, Z2-R-ENV-174, Z2-R-ENV-179, Z2-R-ENV-180 |
| Z2-I-ENV-02 | terrain painting | Z2-R-ENV-001, Z2-R-ENV-004, Z2-R-ENV-005, Z2-R-ENV-006, Z2-R-ENV-013, Z2-R-ENV-014, Z2-R-ENV-017, Z2-R-ENV-020, Z2-R-ENV-021, Z2-R-ENV-023, Z2-R-ENV-027, Z2-R-ENV-028, Z2-R-ENV-029, Z2-R-ENV-031, Z2-R-ENV-034, Z2-R-ENV-035, Z2-R-ENV-037, Z2-R-ENV-038, Z2-R-ENV-039, Z2-R-ENV-040, Z2-R-ENV-041, Z2-R-ENV-042, Z2-R-ENV-044, Z2-R-ENV-135, Z2-R-ENV-137, Z2-R-ENV-138, Z2-R-ENV-139, Z2-R-ENV-143, Z2-R-ENV-144, Z2-R-ENV-145, Z2-R-ENV-146, Z2-R-ENV-147, Z2-R-ENV-149, Z2-R-ENV-156, Z2-R-ENV-157, Z2-R-ENV-171, Z2-R-ENV-172, Z2-R-ENV-173, Z2-R-ENV-174 |
| Z2-I-ENV-03 | foliage placement | Z2-R-ENV-001, Z2-R-ENV-004, Z2-R-ENV-005, Z2-R-ENV-006, Z2-R-ENV-007, Z2-R-ENV-009, Z2-R-ENV-030, Z2-R-ENV-050, Z2-R-ENV-051, Z2-R-ENV-052, Z2-R-ENV-053, Z2-R-ENV-054, Z2-R-ENV-055, Z2-R-ENV-056, Z2-R-ENV-057, Z2-R-ENV-058, Z2-R-ENV-059, Z2-R-ENV-060, Z2-R-ENV-061, Z2-R-ENV-062, Z2-R-ENV-063, Z2-R-ENV-064, Z2-R-ENV-065, Z2-R-ENV-071, Z2-R-ENV-126, Z2-R-ENV-135, Z2-R-ENV-137, Z2-R-ENV-138, Z2-R-ENV-139, Z2-R-ENV-141, Z2-R-ENV-143, Z2-R-ENV-144, Z2-R-ENV-145, Z2-R-ENV-146, Z2-R-ENV-149, Z2-R-ENV-150, Z2-R-ENV-151, Z2-R-ENV-152, Z2-R-ENV-153, Z2-R-ENV-154, Z2-R-ENV-155, Z2-R-ENV-158, Z2-R-ENV-161, Z2-R-ENV-164, Z2-R-ENV-167, Z2-R-ENV-168, Z2-R-ENV-174, Z2-R-ENV-175, Z2-R-ENV-177, Z2-R-ENV-179, Z2-R-ENV-180, Z2-R-ENV-182 |
| Z2-I-ENV-04 | foliage wind | Z2-R-ENV-001, Z2-R-ENV-004, Z2-R-ENV-005, Z2-R-ENV-006, Z2-R-ENV-066, Z2-R-ENV-067, Z2-R-ENV-068, Z2-R-ENV-069, Z2-R-ENV-070, Z2-R-ENV-071, Z2-R-ENV-072, Z2-R-ENV-135, Z2-R-ENV-137, Z2-R-ENV-138, Z2-R-ENV-139, Z2-R-ENV-143, Z2-R-ENV-144, Z2-R-ENV-145, Z2-R-ENV-146, Z2-R-ENV-149, Z2-R-ENV-156, Z2-R-ENV-158, Z2-R-ENV-168, Z2-R-ENV-174, Z2-R-ENV-176 |
| Z2-I-ENV-05 | water | Z2-R-ENV-001, Z2-R-ENV-004, Z2-R-ENV-005, Z2-R-ENV-006, Z2-R-ENV-009, Z2-R-ENV-073, Z2-R-ENV-074, Z2-R-ENV-075, Z2-R-ENV-076, Z2-R-ENV-077, Z2-R-ENV-078, Z2-R-ENV-079, Z2-R-ENV-080, Z2-R-ENV-081, Z2-R-ENV-082, Z2-R-ENV-083, Z2-R-ENV-084, Z2-R-ENV-085, Z2-R-ENV-086, Z2-R-ENV-087, Z2-R-ENV-135, Z2-R-ENV-137, Z2-R-ENV-138, Z2-R-ENV-139, Z2-R-ENV-141, Z2-R-ENV-143, Z2-R-ENV-144, Z2-R-ENV-145, Z2-R-ENV-146, Z2-R-ENV-148, Z2-R-ENV-149, Z2-R-ENV-151, Z2-R-ENV-156, Z2-R-ENV-157, Z2-R-ENV-169, Z2-R-ENV-170, Z2-R-ENV-174, Z2-R-ENV-181 |
| Z2-I-ENV-06 | atmosphere | Z2-R-ENV-001, Z2-R-ENV-004, Z2-R-ENV-005, Z2-R-ENV-006, Z2-R-ENV-010, Z2-R-ENV-088, Z2-R-ENV-089, Z2-R-ENV-090, Z2-R-ENV-091, Z2-R-ENV-092, Z2-R-ENV-097, Z2-R-ENV-100, Z2-R-ENV-135, Z2-R-ENV-137, Z2-R-ENV-138, Z2-R-ENV-139, Z2-R-ENV-141, Z2-R-ENV-143, Z2-R-ENV-144, Z2-R-ENV-145, Z2-R-ENV-146, Z2-R-ENV-149, Z2-R-ENV-150, Z2-R-ENV-159, Z2-R-ENV-165, Z2-R-ENV-166, Z2-R-ENV-174, Z2-R-ENV-179, Z2-R-ENV-180 |
| Z2-I-ENV-07 | clouds | Z2-R-ENV-001, Z2-R-ENV-004, Z2-R-ENV-005, Z2-R-ENV-006, Z2-R-ENV-007, Z2-R-ENV-092, Z2-R-ENV-101, Z2-R-ENV-102, Z2-R-ENV-103, Z2-R-ENV-104, Z2-R-ENV-105, Z2-R-ENV-106, Z2-R-ENV-135, Z2-R-ENV-137, Z2-R-ENV-138, Z2-R-ENV-139, Z2-R-ENV-143, Z2-R-ENV-144, Z2-R-ENV-145, Z2-R-ENV-146, Z2-R-ENV-149, Z2-R-ENV-155, Z2-R-ENV-156, Z2-R-ENV-157, Z2-R-ENV-174, Z2-R-ENV-175, Z2-R-ENV-178, Z2-R-ENV-181 |
| Z2-I-ENV-08 | weather | Z2-R-ENV-001, Z2-R-ENV-004, Z2-R-ENV-005, Z2-R-ENV-006, Z2-R-ENV-010, Z2-R-ENV-078, Z2-R-ENV-091, Z2-R-ENV-107, Z2-R-ENV-108, Z2-R-ENV-109, Z2-R-ENV-110, Z2-R-ENV-111, Z2-R-ENV-112, Z2-R-ENV-113, Z2-R-ENV-114, Z2-R-ENV-115, Z2-R-ENV-135, Z2-R-ENV-137, Z2-R-ENV-138, Z2-R-ENV-139, Z2-R-ENV-141, Z2-R-ENV-142, Z2-R-ENV-143, Z2-R-ENV-144, Z2-R-ENV-145, Z2-R-ENV-146, Z2-R-ENV-148, Z2-R-ENV-149, Z2-R-ENV-151, Z2-R-ENV-174, Z2-R-ENV-176, Z2-R-ENV-177, Z2-R-ENV-181 |
| Z2-I-ENV-09 | day/night | Z2-R-ENV-001, Z2-R-ENV-004, Z2-R-ENV-005, Z2-R-ENV-006, Z2-R-ENV-008, Z2-R-ENV-010, Z2-R-ENV-093, Z2-R-ENV-094, Z2-R-ENV-095, Z2-R-ENV-096, Z2-R-ENV-097, Z2-R-ENV-098, Z2-R-ENV-099, Z2-R-ENV-100, Z2-R-ENV-135, Z2-R-ENV-137, Z2-R-ENV-138, Z2-R-ENV-139, Z2-R-ENV-142, Z2-R-ENV-143, Z2-R-ENV-144, Z2-R-ENV-145, Z2-R-ENV-146, Z2-R-ENV-148, Z2-R-ENV-149, Z2-R-ENV-150, Z2-R-ENV-165, Z2-R-ENV-166, Z2-R-ENV-174 |
| Z2-I-ENV-10 | procedural placement | Z2-R-ENV-001, Z2-R-ENV-004, Z2-R-ENV-005, Z2-R-ENV-006, Z2-R-ENV-116, Z2-R-ENV-117, Z2-R-ENV-118, Z2-R-ENV-119, Z2-R-ENV-120, Z2-R-ENV-121, Z2-R-ENV-122, Z2-R-ENV-123, Z2-R-ENV-124, Z2-R-ENV-129, Z2-R-ENV-135, Z2-R-ENV-136, Z2-R-ENV-137, Z2-R-ENV-138, Z2-R-ENV-139, Z2-R-ENV-141, Z2-R-ENV-143, Z2-R-ENV-144, Z2-R-ENV-145, Z2-R-ENV-146, Z2-R-ENV-147, Z2-R-ENV-149, Z2-R-ENV-153, Z2-R-ENV-161, Z2-R-ENV-162, Z2-R-ENV-163, Z2-R-ENV-174, Z2-R-ENV-179 |
| Z2-I-ENV-11 | collision generation | Z2-R-ENV-001, Z2-R-ENV-004, Z2-R-ENV-005, Z2-R-ENV-006, Z2-R-ENV-008, Z2-R-ENV-045, Z2-R-ENV-048, Z2-R-ENV-058, Z2-R-ENV-125, Z2-R-ENV-126, Z2-R-ENV-129, Z2-R-ENV-135, Z2-R-ENV-137, Z2-R-ENV-138, Z2-R-ENV-139, Z2-R-ENV-143, Z2-R-ENV-144, Z2-R-ENV-145, Z2-R-ENV-146, Z2-R-ENV-149, Z2-R-ENV-174 |
| Z2-I-ENV-12 | navigation generation | Z2-R-ENV-001, Z2-R-ENV-004, Z2-R-ENV-005, Z2-R-ENV-006, Z2-R-ENV-127, Z2-R-ENV-128, Z2-R-ENV-129, Z2-R-ENV-135, Z2-R-ENV-137, Z2-R-ENV-138, Z2-R-ENV-139, Z2-R-ENV-143, Z2-R-ENV-144, Z2-R-ENV-145, Z2-R-ENV-146, Z2-R-ENV-149, Z2-R-ENV-174 |
| Z2-I-ENV-13 | environment streaming integration | Z2-R-ENV-001, Z2-R-ENV-002, Z2-R-ENV-003, Z2-R-ENV-004, Z2-R-ENV-005, Z2-R-ENV-006, Z2-R-ENV-009, Z2-R-ENV-043, Z2-R-ENV-045, Z2-R-ENV-046, Z2-R-ENV-047, Z2-R-ENV-049, Z2-R-ENV-053, Z2-R-ENV-054, Z2-R-ENV-062, Z2-R-ENV-130, Z2-R-ENV-131, Z2-R-ENV-132, Z2-R-ENV-133, Z2-R-ENV-134, Z2-R-ENV-135, Z2-R-ENV-137, Z2-R-ENV-138, Z2-R-ENV-139, Z2-R-ENV-142, Z2-R-ENV-143, Z2-R-ENV-144, Z2-R-ENV-145, Z2-R-ENV-146, Z2-R-ENV-149, Z2-R-ENV-151, Z2-R-ENV-152, Z2-R-ENV-154, Z2-R-ENV-174 |

## Excluded and deferred

Every ENV inventory item is classified required: Z2-I-ENV-01, -03, -06, -09 and -10 at Stage 3 by the orchestrator's ruling on Q45 (pending core-doc batch 4 (Q45) for the Inventory rows), the other eight at Stage 4; no ENV requirement is deferred, excluded or game-side. Each requirement's `Stage` is the stage whose acceptance its evidence belongs to, which is also its fix version: S3 for what the Stage 3 acceptance (W-Outdoor, save/load, the cook equivalence) exercises, S4 for what needs the Stage 4 features (painting, water, weather, clouds, detail foliage, collision, navigation, streaming, the editor iteration rows, migration coverage, the flagship workloads' sky row). Requirements staged S5 serve the same items at higher quality tiers (volumetric clouds, cloud shadows, the FFT ocean, rivers, caustics, planar reflections, weather tiers, thermal erosion, local wind and fog, the Deferred and visibility-buffer paths, the VT-backed terrain reference scene); the S1 requirements are the budget producer; the S6 requirement is shipping qualification. The boundaries that keep the area bounded are exclusions of other capabilities from ENV:

| Not in ENV | Rationale | Decision or owner |
|---|---|---|
| terrain geometry (the cook function, CDLOD, height-tile textures, the min/max pyramid) | geometry is GEOM's; ENV builds the function's input | GEOM; `Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain cook contract"` |
| the sky, fog and LUT passes | lighting passes are LIGHT's; ENV supplies parameters | LIGHT; `Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Atmosphere ownership"` |
| the terrain and foliage editor tools and undo transactions | editor tools are EDIT's over ENV's documents | EDIT; Z2-D-079 |
| world partitions, layers, concurrent editing, procedural-generation orchestration, baking workflows | world authoring is WORLD's | WORLD |
| cell streaming and cancellation, large-coordinate rebasing | streaming is STREAM's | STREAM |
| collision shapes, bodies, buoyancy; navmesh generation and queries | ENV supplies geometry and hooks | PHYS; AI with D-SEL-NAV (open, Stage 1/2) |
| rain and snow particles, ambience, gameplay reactions to weather | ENV spawns prefabs and emits events | VFX, AUDIO, GAME |
| a material node graph | materials are Slang code with previews | Z2-D-079 |
| runtime terrain deformation | not an inventory item; the terrain is static outside the editor | none (scope) |
