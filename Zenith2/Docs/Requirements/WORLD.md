# WORLD requirement catalogue

## Purpose

This catalogue breaks the WORLD inventory items (`Zenith2/Docs/Requirements/Inventory.md` → "Inventory v1", table "WORLD World authoring") into testable requirements `Z2-R-WORLD-NNN`. Each names the inventory items it covers, the stage whose acceptance it serves, its classification, its governing decisions and budget rows, the artefact that proves it and the gate step or acceptance suite that runs it, and the headings of the WORLD design family (`Zenith2/Docs/Design/WORLD.md` and its seven section files) that specify it; the entry's "Requirement index" is the reverse map. WORLD owns every requirement here. Requirements whose evidence depends on another area's pending declaration say "once row WD<n> lands"; their cross-area rows are listed in `Zenith2/Docs/Design/WORLD.md` → "Reconciliations with other documents".

## Requirements

### Z2-R-WORLD-001 WORLD code in ZenithAuthoring

- **Statement:** WORLD's shared tool code lives in `Zenith2/Tools/Authoring/Internal/WORLD/` with the single public root header `Zenith2/Tools/Authoring/Zenith_AuthoringWorld.h`, built `ZENITH_TOOLS_HOST`, and no WORLD source exists in any engine library or Player link closure.
- **Covers:** Z2-I-WORLD-01, Z2-I-WORLD-02, Z2-I-WORLD-03, Z2-I-WORLD-04, Z2-I-WORLD-05, Z2-I-WORLD-06, Z2-I-WORLD-07, Z2-I-WORLD-08, Z2-I-WORLD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-024, Z2-D-002
- **Budgets:** none
- **Acceptance evidence:** `SourceLayout`, `CompileCoverage` and `Libgraph` (no WORLD target in any Shipping closure, [Z2-N-ARCH-241]) on the slice ticket.
- **Design:** Zenith2/Docs/Design/WORLD.md → "ZenithAuthoring files"; Zenith2/Docs/Design/WORLD.md → "Supported matrix"; Zenith2/Docs/Design/WORLD.md → "Libraries WORLD code links"

### Z2-R-WORLD-002 Public header contracts

- **Statement:** Every type of `Zenith_AuthoringWorld.h` carries one contract block, every fallible function returns `Zenith_Result<…, Zenith_WorldError>`, and the header compiles alone with no STL type.
- **Covers:** Z2-I-WORLD-01, Z2-I-WORLD-02, Z2-I-WORLD-03, Z2-I-WORLD-04, Z2-I-WORLD-05, Z2-I-WORLD-06, Z2-I-WORLD-07, Z2-I-WORLD-08, Z2-I-WORLD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-047, Z2-D-045, Z2-D-041
- **Budgets:** none
- **Acceptance evidence:** `ApiContractCheck`, `HeaderSelfContained`, `ForbiddenCheck` on `Zenith2/Tools/Authoring/Zenith_AuthoringWorld.h`.
- **Design:** Zenith2/Docs/Design/WORLD/PublicAPI.md → "Header list"; Zenith2/Docs/Design/WORLD/PublicAPI.md → "Error and diagnostics types"; Zenith2/Docs/Design/WORLD.md → "Rules across the WORLD API"

### Z2-R-WORLD-003 World importer in ZenithCook

- **Statement:** The importer `world.world` lives in `Zenith2/Tools/ZenithCook/Internal/World/`, claims `.world.json`, is registered by `Zenith_RegisterWorldImporters` and has exactly the descriptor of "The world importer".
- **Covers:** Z2-I-WORLD-01, Z2-I-WORLD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldImporter_DescAndRegistration` (`ZenithCook_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Partition.md → "The world importer"; Zenith2/Docs/Design/WORLD/PublicAPI.md → "Importer declarations"

### Z2-R-WORLD-004 World editor extension

- **Statement:** WORLD's editor code is the extension `Zenith_WorldEditorExtension` in `Zenith2/Tools/ZenithEditor/Internal/World/`, registering its panels, commands, automation steps and preferences without duplicates.
- **Covers:** Z2-I-WORLD-01, Z2-I-WORLD-02, Z2-I-WORLD-03, Z2-I-WORLD-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-024
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldEditor_ExtensionRegisters` (`ZenithEditor_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Editing.md → "World editor extension"; Zenith2/Docs/Design/WORLD/PublicAPI.md → "Editor declarations"

### Z2-R-WORLD-005 API-only slices land first

- **Statement:** The API-only slice of `Zenith_AuthoringWorld.h` (declarations with stub bodies) and the importer header stub land before their implementations and before ENV's and AI's contribution functions that call WORLD's writer (the declare-first ruling Q56).
- **Covers:** Z2-I-WORLD-01, Z2-I-WORLD-02, Z2-I-WORLD-03, Z2-I-WORLD-04, Z2-I-WORLD-05, Z2-I-WORLD-06, Z2-I-WORLD-07, Z2-I-WORLD-08, Z2-I-WORLD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** The slice tickets' `Build` and `ApiContractCheck` steps; dependent tickets' `blocks` links name the slices.
- **Design:** Zenith2/Docs/Design/WORLD.md → "API-only slices"; Zenith2/Docs/Design/WORLD.md → "Stage table"

### Z2-R-WORLD-006 No runtime library or script surface

- **Statement:** WORLD ships no runtime library, Luau binding, misuse code or asynchronous source kind; every run-time effect of a world is produced by STREAM, SCENE and the content owners from WORLD's cooked outputs.
- **Covers:** Z2-I-WORLD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-024
- **Budgets:** none
- **Acceptance evidence:** `Libgraph` (no WORLD target in a Player closure) and review of the binding tables (no `World` namespace).
- **Design:** Zenith2/Docs/Design/WORLD.md → "Script bindings"; Zenith2/Docs/Design/WORLD.md → "Boundary with STREAM"; Zenith2/Docs/Design/WORLD.md → "What WORLD does not own"

### Z2-R-WORLD-007 World document schema

- **Statement:** `*.world.json` is document type `Zenith.World.WorldDocument` version 1 with exactly the members, defaults and ranges of "World root members", read into `Zenith_WorldDesc` by `Zenith_ReadWorldDocument`.
- **Covers:** Z2-I-WORLD-01, Z2-I-WORLD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldDocument_ReadsExample` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Documents.md → "World document"; Zenith2/Docs/Design/WORLD/PublicAPI.md → "World description types"; Zenith2/Docs/Design/WORLD/PublicAPI.md → "World document functions"

### Z2-R-WORLD-008 World validation messages

- **Statement:** Each validation rule is checked by exactly one pass, the reader (W1 to W4, W6 to W12, W15, W16, W18, W19, W25, W26, W28, W29, over the world document alone) or the importer pass of the partition (W5, W13, W14, W17, W20 to W47, over the documents they read); each finding carries its level, code, rule number, document path and JSON pointer, every finding of a pass is reported before failing, the collector's overflow is summarised, and an ERROR fails both the import and the editor's validation.
- **Covers:** Z2-I-WORLD-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldDocument_EveryValidationMessage`, `Test_WorldDocument_ReportsEveryFindingOfAPass`, `Test_WorldDocument_DiagnosticsCapReportsDropped`, `Test_WorldPartition_ImporterValidationMessages` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Documents.md → "World validation messages"

### Z2-R-WORLD-009 Region size and origin

- **Statement:** `regionSize` is one of 128, 256, 512, 1,024 and 2,048 m and each `origin` component a multiple of 128 m; a region covers its half-open rectangle at the world's vertical extent.
- **Covers:** Z2-I-WORLD-01, Z2-I-WORLD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldDocument_EveryValidationMessage` (W4) and `Test_WorldPartition_RegionAnchorComposition` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Documents.md → "World root members"; Zenith2/Docs/Design/WORLD/Documents.md → "Region objects"

### Z2-R-WORLD-010 Regions as a keyed map

- **Statement:** The world document's regions are a keyed map from `<LayerKey>/<x>_<z>[/<GeneratorKey>]` to `{ scene, generator }` with the key grammar of "Region objects", parsed by `Zenith_ParseWorldRegionKey`.
- **Covers:** Z2-I-WORLD-01, Z2-I-WORLD-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldDocument_RegionKeyGrammar` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Documents.md → "Region objects"; Zenith2/Docs/Design/WORLD/PublicAPI.md → "Identity and keys"

### Z2-R-WORLD-011 Region documents are input-only scenes

- **Statement:** A region document is a SCENE scene document at `Regions/<LayerKey>/<x>_<z>.scene.json` obeying rules R1 to R6, read only as an input of its world's importer with no `.meta` and no standalone cook, and the importer reports W20 to W24 for violations.
- **Covers:** Z2-I-WORLD-01, Z2-I-WORLD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldPartition_ForbiddenStreamComponents`, `Test_WorldPartition_DuplicateEntityAcrossRegions`, `Test_WorldPartition_OutsideRegionWarns` (`ZenithAuthoring_Tests`, `Test` step (`win`)). `Test_WorldImporter_RegionsAreInputOnly` (`ZenithCook_Tests`, `Test` step (`win`)), once rows WD21 and WD22 land.
- **Design:** Zenith2/Docs/Design/WORLD/Documents.md → "Region documents"

### Z2-R-WORLD-012 Anchor-relative region transforms

- **Statement:** Root transforms in a region document are relative to the region anchor `(origin.x + x·S, baseY, origin.z + z·S)`, composed in double precision by the partition.
- **Covers:** Z2-I-WORLD-01, Z2-I-WORLD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldPartition_RegionAnchorComposition` (`ZenithAuthoring_Tests`, `Test` step (`win`)); `Test_WorldEditor_ProjectionOrigin` (`ZenithEditor_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Documents.md → "Region documents"; Zenith2/Docs/Design/WORLD/Partition.md → "Partition algorithm"

### Z2-R-WORLD-013 Document limits

- **Statement:** Every limit of "Document limits" is enforced and tested at the limit and at the limit plus one.
- **Covers:** Z2-I-WORLD-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldDocument_Limits` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Documents.md → "Document limits"

### Z2-R-WORLD-014 Document type registration and migration

- **Statement:** `Zenith_RegisterWorldDocumentTypes` registers the four WORLD document types with their suffixes, schema tags, versions and canonical schemas, each with a migration chain and fixtures per step, and the editor and the cook register them in the same order.
- **Covers:** Z2-I-WORLD-01, Z2-I-WORLD-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldDocument_RegistrationAndSchemas`, `Test_WorldDocument_CanonicalRoundTrip` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Documents.md → "Document type registration"; Zenith2/Docs/Design/WORLD/Documents.md → "Migration chains"

### Z2-R-WORLD-015 Malformed input never asserts

- **Statement:** No malformed, truncated or oversized WORLD document, payload or generator parameter object reaches an assert or an out-of-bounds read; each produces findings.
- **Covers:** Z2-I-WORLD-01, Z2-I-WORLD-05, Z2-I-WORLD-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldDocument_MalformedCorpusNeverAsserts`, `Test_WorldLightBakeDocument_PayloadChecks` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Documents.md → "Document limits"; Zenith2/Docs/Design/WORLD/Documents.md → "Payload objects"

### Z2-R-WORLD-016 Layer object schema

- **Statement:** A layer is a keyed-map entry with `id`, `kind`, `state`, `grid`, `realisation`, `initiallyActive`, `bakes` and `editor` as "Layer objects" states, names 1 to 24 characters of `[A-Za-z0-9_-]` used for display only, at most 32 layers, with unique `id`s and unique layer keys.
- **Covers:** Z2-I-WORLD-02, Z2-I-WORLD-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldDocument_LayerCombinations`, `Test_WorldDocument_LayerKeyCollision`, `Test_WorldDocument_Limits` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Documents.md → "Layer objects"

### Z2-R-WORLD-017 Editor-only data layers never cooked

- **Statement:** Content of an `editorOnly` data layer reaches no cook output, and a reference to it from cooked content is nulled with `WORLD_WARNING_REFERENCE_UNRESOLVED`.
- **Covers:** Z2-I-WORLD-03, Z2-I-WORLD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldPartition_EditorOnlyDropped` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Documents.md → "Layer kinds and states"; Zenith2/Docs/Design/WORLD/Partition.md → "Layer realisation"; Zenith2/Docs/Design/WORLD/Partition.md → "Cross-cell references"

### Z2-R-WORLD-018 Baked data layers merge into base grids

- **Statement:** Content of a `baked` data layer becomes content of the base grids' cells, going to the layer's `grid` when set and otherwise to the grid of its role.
- **Covers:** Z2-I-WORLD-03, Z2-I-WORLD-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldPartition_RoleRules` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Partition.md → "Grid assignment"; Zenith2/Docs/Design/WORLD/Partition.md → "Layer realisation"

### Z2-R-WORLD-019 Gameplay layers are runtime only

- **Statement:** A `gameplay` layer has state `runtime` with realisation `grid` or `additive`, and any other combination is W8.
- **Covers:** Z2-I-WORLD-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldDocument_LayerCombinations` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Documents.md → "Layer kinds and states"

### Z2-R-WORLD-020 Layer grids

- **Statement:** Each grid-realised gameplay layer gets grid `layer.<LayerKey>` at index `layerGridBase` plus its position in `layerGridOrder`, copied from `layerGridTemplate` with `hlod.levels` 0, role gameplay, depending on the collision and navigation grids, holding only that layer's content, its static colliders included.
- **Covers:** Z2-I-WORLD-02, Z2-I-WORLD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldPartition_LayerGridRealisation`, `Test_WorldPartition_LayerGridIndicesStable` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Partition.md → "Layer realisation"; Zenith2/Docs/Design/WORLD/Partition.md → "Partition algorithm"

### Z2-R-WORLD-021 Grid limit counts layer grids

- **Statement:** Base grids plus layer grids never exceed STREAM's 8 grids; a world needing more is W10.
- **Covers:** Z2-I-WORLD-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldDocument_GridLimitCountsLayerGrids` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Documents.md → "World validation messages"; Zenith2/Docs/Design/WORLD/Partition.md → "Partition limits"

### Z2-R-WORLD-022 Additive gameplay layer scenes

- **Statement:** In an unstreamed world an additive gameplay layer becomes one product scene `layer:<layer id>` with world-origin-relative positions within `uSTREAMING_PRECISION_RADIUS_METRES` (4,096 m), loadable through SCENE's additive load; an additive layer in a streamed world is refused (W25).
- **Covers:** Z2-I-WORLD-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldPartition_AdditiveLayerScene`, `Test_WorldDocument_LayerCombinations` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Partition.md → "Layer realisation"

### Z2-R-WORLD-023 Layers switch through STREAM and SCENE

- **Statement:** A gameplay layer is switched at run time only by the sources' `GridMask` bit of its layer grid or, in an unstreamed world, by SCENE's additive load of its scene; WORLD adds no run-time state, and the switch state is saved by its owners.
- **Covers:** Z2-I-WORLD-02, Z2-I-WORLD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldCooked_GameplayLayerToggle` (`Zenith_Integration_WorldAuthoring` (suite `z2-world-cooked`), `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Partition.md → "Layer realisation"

### Z2-R-WORLD-024 Runtime layers excluded from bakes

- **Statement:** Runtime layer content is excluded from navigation input scenes, lightmaps and HLOD proxies; a `bakes` member true on a RUNTIME or EDITOR_ONLY layer is refused (W19).
- **Covers:** Z2-I-WORLD-02, Z2-I-WORLD-06, Z2-I-WORLD-07, Z2-I-WORLD-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldDocument_LayerCombinations`, `Test_WorldPartition_NavigationInputScenes`, `Test_WorldHlod_Eligibility` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Partition.md → "Layer realisation"; Zenith2/Docs/Design/WORLD/Bakes.md → "Instance eligibility"

### Z2-R-WORLD-025 Layer view defaults shared and overrides per user

- **Statement:** A layer's `editor` member holds the team defaults of colour, visibility, loading and locking, and each user's overrides are written only into the untracked session file.
- **Covers:** Z2-I-WORLD-03, Z2-I-WORLD-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldEditor_LayerOverridesStayInSession` (`ZenithEditor_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Editing.md → "World panel"; Zenith2/Docs/Design/WORLD/Documents.md → "Layer objects"

### Z2-R-WORLD-026 Locked layers refuse edits

- **Statement:** Every command touching a region document of a layer the user has locked fails with `EDITOR_ERROR_DOCUMENT_READ_ONLY` before applying a change, and nothing is recorded.
- **Covers:** Z2-I-WORLD-03, Z2-I-WORLD-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldEditor_LayerLockRefusesEdits` (`ZenithEditor_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Editing.md → "World panel"

### Z2-R-WORLD-027 Importer inputs recorded

- **Statement:** Every document and product list the importer reads is recorded as an import input, so any change to one re-imports the world.
- **Covers:** Z2-I-WORLD-01, Z2-I-WORLD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldImporter_RecordsEveryInput` (`ZenithCook_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Partition.md → "Importer inputs"

### Z2-R-WORLD-028 Importer document access

- **Statement:** The importer opens its source, region, prefab, generator and bake documents migrated in memory through the import context members of row WD1 and never writes a source file.
- **Covers:** Z2-I-WORLD-01, Z2-I-WORLD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldImporter_RecordsEveryInput` (`ZenithCook_Tests`, `Test` step (`win`)), once row WD1 lands.
- **Design:** Zenith2/Docs/Design/WORLD/Partition.md → "The world importer"; Zenith2/Docs/Design/WORLD/Partition.md → "Importer inputs"

### Z2-R-WORLD-029 Region resolution

- **Statement:** Each region document is resolved with SCENE's resolver and the import prefab provider, and a resolution failure is `WORLD_ERROR_PREFAB_RESOLUTION` with SCENE's code as detail.
- **Covers:** Z2-I-WORLD-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldPartition_SmallWorldGolden` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Partition.md → "Partition algorithm"

### Z2-R-WORLD-030 Grid assignment by role

- **Statement:** A baked hierarchy without a layer grid goes to the grid of the first matching role rule of `Zenith_GetWorldRoleRules`, chosen through `defaultFor`, then the first grid of the STREAM role, then the first gameplay grid.
- **Covers:** Z2-I-WORLD-01, Z2-I-WORLD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldPartition_RoleRules` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Partition.md → "Grid assignment"; Zenith2/Docs/Design/WORLD/PublicAPI.md → "Role rules"

### Z2-R-WORLD-031 Cell coordinate by floor

- **Statement:** A hierarchy's cell is the floor of its root's world position minus the grid origin over the cell size, computed in double.
- **Covers:** Z2-I-WORLD-01, Z2-I-WORLD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldPartition_CellCoordinateFloor` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Partition.md → "Partition algorithm"

### Z2-R-WORLD-032 Overlap bound and large promotion

- **Statement:** Every entity lies within STREAM's overlap bound of its cell, a violating hierarchy moves to the large grid when allowed and fitting, and otherwise the import fails with `WORLD_ERROR_ENTITY_BOUNDS`.
- **Covers:** Z2-I-WORLD-01, Z2-I-WORLD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldPartition_LargePromotion` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Partition.md → "Partition algorithm"; Zenith2/Docs/Design/WORLD/Partition.md → "Grid assignment"

### Z2-R-WORLD-033 Static collider split

- **Statement:** With `splitStaticColliders`, each static non-trigger collider of a BAKED hierarchy assigned to a non-collision base grid, in a hierarchy carrying no `Zenith.Streaming.Persistent`, `Zenith.Behaviour`, `Zenith.Physics.Joint` or schema-defined component, becomes a proxy entity with ID `Zenith_WorldColliderProxyID` and `Zenith.Physics.ColliderProxy` naming its source in the collision cell under it and is removed from the original; triggers of mixed hierarchies, colliders under rigid bodies and layer-grid colliders stay.
- **Covers:** Z2-I-WORLD-01, Z2-I-WORLD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldPartition_StaticColliderSplit`, `Test_WorldPartition_SplitSkipsPersistentAndScripted` (`ZenithAuthoring_Tests`, `Test` step (`win`)). `Test_WorldCooked_StreamsHeadless`, `Test_WorldCooked_DestroyedSourceRemovesProxyCollision` (`Zenith_Integration_WorldAuthoring` (suite `z2-world-cooked`), `Test` step (`win`)), once row WD20 lands.
- **Design:** Zenith2/Docs/Design/WORLD/Partition.md → "Static collider split"; Zenith2/Docs/Design/WORLD/Partition.md → "Grid assignment"

### Z2-R-WORLD-034 Unsplit collider radius rule

- **Statement:** Every grid receiving a static collider that is not split (splitting disabled, a layer grid, or a hierarchy the split never splits) has an activation radius at least the collision grid's activation radius plus hysteresis, else W27 names the grid, the entity and its layer.
- **Covers:** Z2-I-WORLD-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldPartition_SplitDisabledRadiusRule`, `Test_WorldPartition_SplitSkipsPersistentAndScripted`, `Test_WorldPartition_LayerGridRealisation` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Partition.md → "Static collider split"; Zenith2/Docs/Design/WORLD/Documents.md → "World validation messages"

### Z2-R-WORLD-035 Cross-cell references

- **Statement:** Every ENTITY property naming another cell is written null with a reference object in the streaming document, references into the persistent scene are kept, and unbindable or unresolved references are nulled with their warnings.
- **Covers:** Z2-I-WORLD-01, Z2-I-WORLD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldPartition_CrossCellReferences` (`ZenithAuthoring_Tests`, `Test` step (`win`)); `Test_WorldCooked_ReferenceBinds` (`Zenith_Integration_WorldAuthoring` (suite `z2-world-cooked`), `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Partition.md → "Cross-cell references"

### Z2-R-WORLD-036 Persistent markers

- **Statement:** The partition writes `Zenith.Streaming.Persistent` properties 2 and 3 as the cell GUID and the entity ID, keeps the authored mode, and fails above 1,024 persistent entities per cell.
- **Covers:** Z2-I-WORLD-01, Z2-I-WORLD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldPartition_PersistentMarkers` (`ZenithAuthoring_Tests`, `Test` step (`win`)); `Test_WorldCooked_PersistentEntitySurvivesUnload` (`Zenith_Integration_WorldAuthoring` (suite `z2-world-cooked`), `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Partition.md → "Persistent markers"

### Z2-R-WORLD-037 Cell anchor root

- **Statement:** Each cell scene has exactly one root, the anchor with ID `Zenith_WorldAnchorEntityID`, an identity transform and `Zenith.Streaming.CellAnchor { cell, 0, world }`, and every hierarchy is its child with an anchor-relative root position.
- **Covers:** Z2-I-WORLD-01, Z2-I-WORLD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldPartition_AnchorRoot` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Partition.md → "Cell scene documents"

### Z2-R-WORLD-038 Cells pass STREAM's checks

- **Statement:** Every cell scene and streaming document the partition writes passes STREAM's validation and cell scene checks V12 to V18.
- **Covers:** Z2-I-WORLD-01, Z2-I-WORLD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldPartition_CellsPassStreamChecks` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Partition.md → "Cell scene documents"; Zenith2/Docs/Design/WORLD/Partition.md → "Streaming world document generation"

### Z2-R-WORLD-039 Streaming world document generation

- **Statement:** The primary product's intermediate `stream.worldDocument` is STREAM's document with the grid table, every non-empty cell sorted by (grid, z, x), the HLOD objects and the references in STREAM's orders.
- **Covers:** Z2-I-WORLD-01, Z2-I-WORLD-08, Z2-I-WORLD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldPartition_StreamDocumentShape` (`ZenithAuthoring_Tests`, `Test` step (`win`)), once row WD7 lands for the cook.
- **Design:** Zenith2/Docs/Design/WORLD/Partition.md → "Streaming world document generation"

### Z2-R-WORLD-040 Unstreamed worlds

- **Statement:** In unstreamed mode the importer declares one primary content scene with every baked hierarchy within 4,096 m of the origin and no streaming world, cell or HLOD node, and refuses grid-realised layers (W26) and lighting (W29).
- **Covers:** Z2-I-WORLD-01, Z2-I-WORLD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldPartition_UnstreamedContentScene` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Partition.md → "Unstreamed worlds"

### Z2-R-WORLD-041 Products and intermediate blobs

- **Statement:** The importer declares exactly the products of "Products and keys" in key byte order, each with exactly its intermediate blobs.
- **Covers:** Z2-I-WORLD-01, Z2-I-WORLD-06, Z2-I-WORLD-08, Z2-I-WORLD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldPartition_ProductsDerivedAndSorted` (`ZenithAuthoring_Tests`, `Test` step (`win`)); `Test_WorldImporter_IntermediateBlobNames` (`ZenithCook_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Partition.md → "Products and keys"

### Z2-R-WORLD-042 Derived product identity

- **Statement:** Every non-primary product's GUID is `Zenith_WorldDeriveProductGUID(world, key)` over SHA-256 and is never written into the world's `.meta`.
- **Covers:** Z2-I-WORLD-01, Z2-I-WORLD-04, Z2-I-WORLD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldIdentity_KeyIDVectors`, `Test_WorldIdentity_ProductGUIDVectors`, `Test_WorldIdentity_ScopeConstantsDistinct`, `Test_WorldPartition_ProductsDerivedAndSorted` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Partition.md → "Product identity"; Zenith2/Docs/Design/WORLD/PublicAPI.md → "Identity and keys"

### Z2-R-WORLD-043 Partition byte determinism

- **Statement:** Two partitions of the same inputs produce byte-identical intermediates for every product, in one process or two.
- **Covers:** Z2-I-WORLD-01, Z2-I-WORLD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldPartition_SameInputsSameBytes` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Partition.md → "Partition determinism"

### Z2-R-WORLD-044 Partition independent of input order

- **Statement:** Permuting the order regions are opened or the order of members on disk changes no output byte.
- **Covers:** Z2-I-WORLD-01, Z2-I-WORLD-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldPartition_RegionOrderIrrelevant` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Partition.md → "Partition determinism"

### Z2-R-WORLD-045 WORLD work independent of workers

- **Statement:** The partition runs single-threaded, and every WORLD work item (a generator region job, a lighting work item) gives byte-identical results for any worker count, any order and inline execution.
- **Covers:** Z2-I-WORLD-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldGenerator_WorkerCountInvariant`, `Test_WorldLightBake_WorkerCountInvariant` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD.md → "Rules across the WORLD API"; Zenith2/Docs/Design/WORLD/Partition.md → "Partition determinism"

### Z2-R-WORLD-046 Partition limits

- **Statement:** Every limit of "Partition limits" is checked before any product is declared and tested at the limit and at the limit plus one.
- **Covers:** Z2-I-WORLD-01, Z2-I-WORLD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldPartition_Limits` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Partition.md → "Partition limits"

### Z2-R-WORLD-047 Failed import declares nothing

- **Statement:** An import with any ERROR finding returns `IMPORT_ERROR_SOURCE_INVALID` after reporting every finding with its code name first and declares no product.
- **Covers:** Z2-I-WORLD-01, Z2-I-WORLD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldImporter_FailureDeclaresNothing` (`ZenithCook_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Partition.md → "The world importer"; Zenith2/Docs/Design/WORLD.md → "Error enumerations"

### Z2-R-WORLD-048 ENV cell contribution

- **Statement:** For every terrain-holding cell the importer's environment contributor reads ENV's published `environment.cellContent.<x>_<z>` property of each environment cell whose minimum corner lies in the cell and writes one entity per present product through `Zenith_WorldCellContentWriter`, appended after the authored content (ruling WORLD-WDB9-WDB6).
- **Covers:** Z2-I-WORLD-01, Z2-I-WORLD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldPartition_ContributorsCalled` (`ZenithAuthoring_Tests`, `Test` step (`win`)) and `Test_WorldImporter_DeclaresEnvironmentCellContent` (`ZenithCook_Tests`, `Test` step (`win`)), once Z2T-ENV-102 lands.
- **Design:** Zenith2/Docs/Design/WORLD/Partition.md → "Contributed cell content"; Zenith2/Docs/Design/WORLD/PublicAPI.md → "Cell content writer"

### Z2-R-WORLD-049 AI cell contribution

- **Statement:** For every navigation cell the partition calls AI's `Zenith_AIWriteCellNavigationContent` through the contributor seam with the navigation products the import itself declared and appends its tile entities.
- **Covers:** Z2-I-WORLD-07, Z2-I-WORLD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldPartition_ContributorsCalled` (`ZenithAuthoring_Tests`, `Test` step (`win`)). The production contributor, once rows WD11 and WD23 land.
- **Design:** Zenith2/Docs/Design/WORLD/Partition.md → "Contributed cell content"

### Z2-R-WORLD-050 Lighting attachment

- **Statement:** A lighting cell with a light bake header gets the lightmap set on its anchor, each baked probe's `BakedAsset` and an irradiance-volume entity; stale headers attach with W41 and missing ones attach nothing with W40.
- **Covers:** Z2-I-WORLD-06, Z2-I-WORLD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldPartition_LightingAttached` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Partition.md → "Contributed cell content"

### Z2-R-WORLD-051 Incremental re-import

- **Statement:** Editing one region re-imports the world and leaves the intermediate bytes of every unaffected cell, layer scene, navigation input scene and HLOD recipe unchanged, so their cook items hit the DDC; the re-import's own time is the pending row "World partition import time", measured after a one-region edit.
- **Covers:** Z2-I-WORLD-01, Z2-I-WORLD-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-ASSET-011
- **Acceptance evidence:** `Test_WorldImporter_ReimportOnRegionEdit` (`ZenithCook_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Partition.md → "The world importer"; Zenith2/Docs/Design/WORLD/Partition.md → "Partition cost and scaling"

### Z2-R-WORLD-052 Clean equals incremental

- **Statement:** A clean cook and an incremental cook of the fixture game after a one-region edit produce identical trees.
- **Covers:** Z2-I-WORLD-01, Z2-I-WORLD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldCookFixture_WriteAndCook`, `Test_WorldImporter_CleanEqualsIncremental` (`ZenithCook_Tests`, `Test` step (`win`)); the Stage 4 acceptance's ASSET suite over OpenWorldSlice.
- **Design:** Zenith2/Docs/Design/WORLD/Partition.md → "Partition determinism"

### Z2-R-WORLD-053 Re-partition keeps identity

- **Statement:** Changing a grid's cell size keeps every entity's persistent ID and the GUID of every cell coordinate that still holds content, and STREAM re-homes saved deltas to the cells that hold their entities, because cell membership never enters an entity's identity.
- **Covers:** Z2-I-WORLD-01, Z2-I-WORLD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldCooked_RepartitionKeepsIdentity` (`Zenith_Integration_WorldAuthoring` (suite `z2-world-cooked`), `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Partition.md → "Product identity"

### Z2-R-WORLD-054 Content edits touch one region

- **Statement:** A content edit inside a region changes only that region's document, a terrain stroke only the tile documents under its brush, and every other edit kind only the files and keyed children of "Concurrent editing granularity".
- **Covers:** Z2-I-WORLD-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldEditor_MoveAcrossRegionsOneRecord`, `Test_WorldEditor_PlaceCreatesRegion`, `Test_WorldEditor_LayerRenameEditsOneKey` (`ZenithEditor_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Editing.md → "Concurrent editing granularity"; Zenith2/Docs/Design/WORLD/Editing.md → "Terrain editing granularity"

### Z2-R-WORLD-055 Independent additions are disjoint keyed children

- **Statement:** Every collection of identified children in a WORLD document is a keyed or ID map, so independent additions are disjoint keyed children of those maps.
- **Covers:** Z2-I-WORLD-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldDocument_ConcurrentAdditionsAreDisjointKeys` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Editing.md → "Concurrent editing granularity"; Zenith2/Docs/Design/WORLD/Documents.md → "World canonical schema"

### Z2-R-WORLD-056 No derived runtime data in sources

- **Statement:** No cell scene, streaming document, HLOD mesh or other partition product is a source file, and the world's `.meta` does not change when content moves between cells.
- **Covers:** Z2-I-WORLD-04, Z2-I-WORLD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldPartition_ProductsDerivedAndSorted` (`ZenithAuthoring_Tests`, `Test` step (`win`)) and review of the importer's writes.
- **Design:** Zenith2/Docs/Design/WORLD/Editing.md → "Concurrent editing granularity"; Zenith2/Docs/Design/WORLD/Partition.md → "Product identity"

### Z2-R-WORLD-057 Disjoint branches merge to the union

- **Statement:** Two branches editing disjoint regions and adding disjoint layers and regions merge through CONTENT's structural driver with no conflict, never by a line merge, and cook to the same tree and partition plan as the world with both edits applied.
- **Covers:** Z2-I-WORLD-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldConcurrency_DisjointBranchesMergeToUnion` (`Zenith_Integration_WorldAuthoring` (suite `z2-world-cooked`), `Test` step (`win`)). Once row WD16 lands.
- **Design:** Zenith2/Docs/Design/WORLD/Editing.md → "Concurrent editing granularity"

### Z2-R-WORLD-058 Moving content between regions

- **Statement:** A root that leaves its region is moved to the target region's document with unchanged IDs in the same record, and undo restores it at its old sort key.
- **Covers:** Z2-I-WORLD-04, Z2-I-WORLD-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldEditor_MoveAcrossRegionsOneRecord` (`ZenithEditor_Tests`, `Test` step (`win`)), once row WD6 (b) lands.
- **Design:** Zenith2/Docs/Design/WORLD/Editing.md → "Moving content between regions"

### Z2-R-WORLD-059 Placing creates the region

- **Statement:** Placing content where the current layer has no region creates the region document and object in the same record.
- **Covers:** Z2-I-WORLD-01, Z2-I-WORLD-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldEditor_PlaceCreatesRegion` (`ZenithEditor_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Editing.md → "World commands"

### Z2-R-WORLD-060 Writes through the document framework

- **Statement:** Every WORLD write of a tracked file goes through `SaveDocuments` or `ApplyPlan` with its expected digest under the storage root's writer lock.
- **Covers:** Z2-I-WORLD-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldEditor_BakeOperationStates` (storage-locked case) (`ZenithEditor_Tests`, `Test` step (`win`)) and review.
- **Design:** Zenith2/Docs/Design/WORLD/Documents.md → "Common document rules"; Zenith2/Docs/Design/WORLD/Bakes.md → "Bake execution"

### Z2-R-WORLD-061 Merge conflict policies

- **Statement:** Region documents resolve through CONTENT's scene merge, world and generator documents through keyed merges, bake documents by taking one side and re-baking, and derived navigation documents by re-derivation.
- **Covers:** Z2-I-WORLD-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** Review against CONTENT's policies once row WD16 lands; `Test_WorldConcurrency_DisjointBranchesMergeToUnion` (`Zenith_Integration_WorldAuthoring` (suite `z2-world-cooked`), `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Editing.md → "Merge conflicts in WORLD documents"

### Z2-R-WORLD-062 Re-derivation after a merge

- **Statement:** After a merge the next import recomputes every fingerprint and derived document and reports invalid combinations naming both paths.
- **Covers:** Z2-I-WORLD-04, Z2-I-WORLD-06, Z2-I-WORLD-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldPartition_DuplicateEntityAcrossRegions` (`ZenithAuthoring_Tests`, `Test` step (`win`)); `Test_WorldImporter_ReimportOnRegionEdit` (`ZenithCook_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Editing.md → "After a merge"

### Z2-R-WORLD-063 Region projection anchor

- **Statement:** The editor projects a region document's roots at its anchor relative to the edit world's `Zenith.WorldOrigin`, which moves only through SCENE's shift so every projected document moves together, and gizmo edits record anchor-relative values.
- **Covers:** Z2-I-WORLD-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldEditor_ProjectionOrigin` (`ZenithEditor_Tests`, `Test` step (`win`)). Once row WD24 lands.
- **Design:** Zenith2/Docs/Design/WORLD/Editing.md → "Region projection"

### Z2-R-WORLD-064 Region loading around the camera

- **Statement:** The region loader opens regions within the load radius nearest first at most 4 per frame, closes clean regions beyond the unload radius, keeps dirty ones and respects the document limit.
- **Covers:** Z2-I-WORLD-01, Z2-I-WORLD-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-EDIT-005
- **Acceptance evidence:** `Test_WorldEditor_RegionLoaderRadius`, `Test_WorldEditor_RegionLoaderRespectsDocumentLimit` (`ZenithEditor_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Editing.md → "Region loading"

### Z2-R-WORLD-065 Advisory claims

- **Statement:** Per-user layer locks and EDIT's LFS locks are WORLD's claim mechanisms; the bake panel warns before baking a cell whose documents another user has locked and the terrain tool before a stroke over a tile another user has locked.
- **Covers:** Z2-I-WORLD-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldEditor_LayerLockRefusesEdits` (`ZenithEditor_Tests`, `Test` step (`win`)) and review of the panel.
- **Design:** Zenith2/Docs/Design/WORLD/Editing.md → "Concurrent editing granularity"; Zenith2/Docs/Design/WORLD/Editing.md → "Terrain editing granularity"

### Z2-R-WORLD-066 Static generator registry

- **Statement:** Generators are a constant table of built-ins in ascending ID compiled into ZenithAuthoring, each implementing every version from its oldest to its latest; no generator is loaded dynamically.
- **Covers:** Z2-I-WORLD-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-024, Z2-D-002
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldGenerator_RegistrySorted` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Generation.md → "Generator interface and registry"

### Z2-R-WORLD-067 Generator document validation

- **Statement:** A generator document is validated against its schema and its generator's parameter validation, reporting W17, W30 to W33, W36 and W39.
- **Covers:** Z2-I-WORLD-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldGenerator_UnknownVersionRefused`, `Test_WorldGenerator_DependsOnOrderAndCycle`, `Test_WorldGenerator_ParametersRefusedOrAccepted` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Documents.md → "Generator document"; Zenith2/Docs/Design/WORLD/Generation.md → "Running a generator"

### Z2-R-WORLD-068 Version pinning and upgrade preview

- **Statement:** A generator document runs exactly its pinned version, and moving it to another version is an explicit command that previews added, removed and moved outputs.
- **Covers:** Z2-I-WORLD-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldGenerator_UnknownVersionRefused` (`ZenithAuthoring_Tests`, `Test` step (`win`)); `Test_WorldEditor_UpgradePreviewDiff` (`ZenithEditor_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Generation.md → "Upgrading a generator version"

### Z2-R-WORLD-069 Generator version retirement

- **Statement:** Each built-in generator implements every version from its oldest to its latest; while a built-in has a single version no version is retired and no migration fixture exists, and retiring a version starts only once a second version exists, then taking one fix version of deprecation (W31 as a warning) and a migration step moving pinned documents to the oldest remaining version.
- **Covers:** Z2-I-WORLD-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldGenerator_SingleVersionNoRetirement` (`ZenithAuthoring_Tests`, `Test` step (`win`)): every built-in at version 1 only, an empty generator-document migration chain, no WORLD migration fixture, and a document pinned to an unimplemented version refused with W31.
- **Design:** Zenith2/Docs/Design/WORLD/Generation.md → "Generator interface and registry"

### Z2-R-WORLD-070 Cook-mode generation

- **Statement:** A cook-mode generator runs inside every partition import, its outputs existing only in the cooked cells.
- **Covers:** Z2-I-WORLD-05, Z2-I-WORLD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldPartition_SmallWorldGolden` with a cook-mode lattice (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Generation.md → "Execution modes"; Zenith2/Docs/Design/WORLD/Partition.md → "Partition algorithm"

### Z2-R-WORLD-071 Baked-mode generation

- **Statement:** `world.generate` writes a baked generator's region documents, their region objects and its `outputs` record in one journaled batch and one history record.
- **Covers:** Z2-I-WORLD-05, Z2-I-WORLD-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldEditor_GenerateRefusesEdited` (`ZenithEditor_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Generation.md → "Execution modes"; Zenith2/Docs/Design/WORLD/Generation.md → "Generator outputs and staleness"

### Z2-R-WORLD-072 Seed derivation

- **Statement:** Every draw of a generator document comes from the world seed and the document's one stream of "Seed derivation" with its counter set to the output ordinal times 16 plus the draw index, so an output's draws never depend on the region computing it.
- **Covers:** Z2-I-WORLD-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldGenerator_RegionSizeInvariant`, `Test_WorldGenerator_RegionSubsetMatchesFullRun` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Generation.md → "Seed derivation"

### Z2-R-WORLD-073 Generated output identity

- **Statement:** Every generated root's ID is `Zenith_WorldGeneratedID(generator, output key)` with output keys that describe the output, so regeneration keeps the IDs of surviving outputs, including outputs that move across a region border.
- **Covers:** Z2-I-WORLD-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldIdentity_GeneratedIDVectors`, `Test_WorldGenerator_LatticeGolden`, `Test_WorldGenerator_InputChangeKeepsSurvivingIDs` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Generation.md → "Output identity"

### Z2-R-WORLD-074 Generator determinism

- **Statement:** A generator's output is identical for any worker count, region order, region subset and region size, and on every machine that runs one build.
- **Covers:** Z2-I-WORLD-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldGenerator_WorkerCountInvariant`, `Test_WorldGenerator_RegionSubsetMatchesFullRun`, `Test_WorldGenerator_RegionSizeInvariant` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Generation.md → "Generator determinism"

### Z2-R-WORLD-075 Generators are assignments

- **Statement:** Running a generator twice over unchanged inputs gives exactly the output of running it once, a baked generator never reads its own or another generator's generated regions as obstacles, and regeneration replaces rather than accumulates.
- **Covers:** Z2-I-WORLD-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldGenerator_RunTwiceIdentical`, `Test_WorldGenerator_BakedRerunIgnoresOwnOutput` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Generation.md → "Running a generator"

### Z2-R-WORLD-076 Obstacles and dependencies

- **Statement:** A region job reads authored footprints of its 3 × 3 neighbourhood (never generated regions) and the outputs of its `dependsOn` generators as sorted world-absolute obstacle discs of radius `obstacleRadius`; a dependency cycle is W33 and a baked generator depending on a cook-mode one is W36.
- **Covers:** Z2-I-WORLD-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldGenerator_ObstaclesSorted`, `Test_WorldGenerator_DependsOnOrderAndCycle` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Generation.md → "Obstacles and dependencies"

### Z2-R-WORLD-077 Generator capacity

- **Statement:** More than 65,536 outputs in one region is `WORLD_ERROR_GENERATOR_LIMIT` and fails the import.
- **Covers:** Z2-I-WORLD-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldGenerator_CapacityFails` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Generation.md → "Running a generator"

### Z2-R-WORLD-078 Each output owned by one region

- **Statement:** Every output is emitted by exactly the region job whose rectangle holds its owner point, so no output is duplicated or lost at a region border; a generator emitting an output it does not own fails with `WORLD_ERROR_GENERATOR_FAILED`.
- **Covers:** Z2-I-WORLD-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldGenerator_SplineBorderSampleEmittedOnce`, `Test_WorldGenerator_LatticeJitterKeepsBorderPoints`, `Test_WorldGenerator_OwnerOutsideRegionFails` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Generation.md → "Running a generator"

### Z2-R-WORLD-079 world.lattice

- **Statement:** `world.lattice` version 1 places prefab instances on the lattice of "world.lattice" with keys `j × 65,536 + i`, drawing nothing when jitter is 0.
- **Covers:** Z2-I-WORLD-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldGenerator_LatticeGolden` against `Golden/lattice-v1.json` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Generation.md → "world.lattice"

### Z2-R-WORLD-080 world.spline

- **Statement:** `world.spline` version 1 places instances at arc-length steps along its polyline with the offsets, jitter and alignment of "world.spline".
- **Covers:** Z2-I-WORLD-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldGenerator_SplineGolden` against `Golden/spline-v1.json` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Generation.md → "world.spline"

### Z2-R-WORLD-081 world.scatter

- **Statement:** `world.scatter` version 1 places prefab instances with ENV's `Zenith_Place` over the region's placement cells with WORLD's seed, obstacles and exclusions, keyed `<cx>_<cz>/<k>`.
- **Covers:** Z2-I-WORLD-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldGenerator_ScatterUsesEnvPlacement` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Generation.md → "world.scatter"

### Z2-R-WORLD-082 world.probeGrid

- **Statement:** `world.probeGrid` version 1 places baked reflection-probe entities on a lattice above its surface with the probe properties of "world.probeGrid".
- **Covers:** Z2-I-WORLD-05, Z2-I-WORLD-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldGenerator_ProbeGridGolden` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Generation.md → "world.probeGrid"

### Z2-R-WORLD-083 Baked generator staleness

- **Statement:** A baked region job is FRESH, STALE, MISSING, EDITED or ORPHANED by its SHA-256 fingerprint and output digest, and the fingerprint excludes the `outputs` member and every `Zenith_Hash64` value.
- **Covers:** Z2-I-WORLD-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldGenerator_StalenessStates`, `Test_WorldGenerator_EditedDetectionAcrossSchemaVersion` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Generation.md → "Generator outputs and staleness"

### Z2-R-WORLD-084 Edited outputs are protected

- **Statement:** `world.generate` refuses a hand-edited generated region unless `overwriteEdited` is set, and generates the other regions.
- **Covers:** Z2-I-WORLD-05, Z2-I-WORLD-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldEditor_GenerateRefusesEdited` (`ZenithEditor_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Generation.md → "Generator outputs and staleness"

### Z2-R-WORLD-085 Detaching an output

- **Statement:** Detaching a generated region moves its entities with unchanged IDs into the authored region document and removes its output record in one record.
- **Covers:** Z2-I-WORLD-05, Z2-I-WORLD-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldEditor_DetachKeepsIDs` (`ZenithEditor_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Generation.md → "Detaching an output"

### Z2-R-WORLD-086 Hand-written generator goldens

- **Statement:** The spec-derivable outputs of `world.lattice`, `world.spline` and `world.probeGrid` without random draws are committed hand-written goldens the tests compare with.
- **Covers:** Z2-I-WORLD-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldGenerator_LatticeGolden`, `Test_WorldGenerator_SplineGolden`, `Test_WorldGenerator_ProbeGridGolden` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Generation.md → "Golden outputs"

### Z2-R-WORLD-087 Recorded generator goldens

- **Statement:** The version-1 outputs that depend on random draws are recorded once by a needs-human ticket after the D-SEL-HASH ruling, which lands the files together with the test that compares every run with them bit for bit.
- **Covers:** Z2-I-WORLD-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-HASH (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** The recording ticket's files under `Tests/Fixtures/WORLD/Golden/recorded/` and `Test_WorldGenerator_RecordedGoldensUnchanged` in the same ticket (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Generation.md → "Golden outputs"

### Z2-R-WORLD-088 Bake units

- **Statement:** The planner enumerates exactly the lighting, generator, navigation and HLOD units of "Bake units" with their keys and states.
- **Covers:** Z2-I-WORLD-06, Z2-I-WORLD-07, Z2-I-WORLD-08, Z2-I-WORLD-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldBake_UnitsEnumerated` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Bakes.md → "Bake units"

### Z2-R-WORLD-089 Fingerprints are SHA-256

- **Statement:** Every persisted bake fingerprint is a SHA-256 over the fixed byte sequence its kind defines; a baked generator's fingerprint contains no `Zenith_Hash64` value directly or through a position derived from one, while a lighting fingerprint may hash the transforms of cook-mode generator outputs, which are `Zenith_Hash64`-derived, and stays correct because a change of the hash algorithm bumps `uWORLD_LIGHT_TRACER_VERSION` in the same diff.
- **Covers:** Z2-I-WORLD-06, Z2-I-WORLD-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldFingerprint_BuilderVectors`, `Test_WorldGenerator_FingerprintExcludesOutputs`, `Test_WorldBake_FingerprintChangesPerInput` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Bakes.md → "Input fingerprints"; Zenith2/Docs/Design/WORLD/PublicAPI.md → "Fingerprint builder"

### Z2-R-WORLD-090 Bake ordering

- **Statement:** The planner orders units by Kahn's algorithm over their dependencies with the stated ties, generator units before the lighting units they affect, identically across runs.
- **Covers:** Z2-I-WORLD-06, Z2-I-WORLD-07, Z2-I-WORLD-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldBake_OrderIsDeterministic`, `Test_WorldBake_GeneratorPrecedesLighting` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Bakes.md → "Bake planner"

### Z2-R-WORLD-091 Atomic bake units

- **Statement:** Each explicit unit's outputs, including the deletion of its page documents beyond the new page count, are written in one journaled `SaveDocuments` batch, so a unit is wholly written or not at all, no surplus page of an earlier bake survives (W45 reports one that does), and a failed write leaves earlier units written.
- **Covers:** Z2-I-WORLD-06, Z2-I-WORLD-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldEditor_BakeOperationStates`, `Test_WorldEditor_BakeUnitsRestorableFromBackup` (`ZenithEditor_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Bakes.md → "Bake execution"

### Z2-R-WORLD-092 Bake cancellation

- **Statement:** Cancelling a bake finishes the running tile or face, writes nothing of the current unit and reports it cancelled.
- **Covers:** Z2-I-WORLD-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldLightBake_CancelWritesNothing` (`ZenithAuthoring_Tests`, `Test` step (`win`)); `Test_WorldEditor_BakeOperationStates` (`ZenithEditor_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Bakes.md → "Bake execution"

### Z2-R-WORLD-093 Bake resumption

- **Statement:** An interrupted bake leaves completed units FRESH, and the next run bakes exactly the remaining units.
- **Covers:** Z2-I-WORLD-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldBake_InterruptedRunResumes` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Bakes.md → "Bake execution"

### Z2-R-WORLD-094 Bake report

- **Statement:** Every bake run writes `zenith2.world-bake-report` version 1 under the untracked `.editor/world/<W>/` with every unit's before and after states, outputs and errors.
- **Covers:** Z2-I-WORLD-06, Z2-I-WORLD-07, Z2-I-WORLD-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldEditor_BakeOperationStates` reads the report (`ZenithEditor_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Bakes.md → "Bake report format"

### Z2-R-WORLD-095 Headless bakes

- **Statement:** The automation step `world.bake` runs a bake plan from an automation script with kinds, scope and force and waits for its result.
- **Covers:** Z2-I-WORLD-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldEditor_ExtensionRegisters` (`ZenithEditor_Tests`, `Test` step (`win`)); the Stage 4 acceptance bake.
- **Design:** Zenith2/Docs/Design/WORLD/Bakes.md → "Bake execution"; Zenith2/Docs/Design/WORLD/Editing.md → "World editor extension"

### Z2-R-WORLD-096 Bake panel orchestration

- **Statement:** The World Bakes panel lists the active world's units with their states and runs the actions of "Bake panel and commands", answering EDIT's bake panel.
- **Covers:** Z2-I-WORLD-06, Z2-I-WORLD-07, Z2-I-WORLD-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldEditor_BakeOperationStates`, `Test_WorldEditor_GenerateAppliesAfterBackground` (`ZenithEditor_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Editing.md → "Bake panel and commands"

### Z2-R-WORLD-097 Lighting fingerprint content

- **Statement:** A lighting unit's fingerprint covers exactly the inputs of "Lighting fingerprint" in that order (the resolved settings, the cell, every instance with its transform and eligibility, every light of baked lighting layers and the persistent scene, probes, each terrain's origin, height limits, tile list and the digests of its tiles in the gather rectangle, ENV's extraction version, and the LOD 0 geometry digest of every mesh), so each changes it, and nothing outside the gather rectangle, any layer list, the world seed or a cook key does.
- **Covers:** Z2-I-WORLD-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldBake_FingerprintChangesPerInput`, `Test_WorldBake_FingerprintIndependentOfSourceOrder` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Bakes.md → "Lighting fingerprint"

### Z2-R-WORLD-098 Lighting staleness at cook

- **Statement:** The world importer reports a missing lighting unit as W40 and a stale one, including a tracer version change, as W41, and a missing or stale baked generator region as W35 or W34, each through the code-taking `Report` with its WORLD code; every cook report lists these entries by `code`, replayed from the persisted diagnostics of the import that produced the world's current intermediates when that import did not run in the cook (a CLEAN world or a DDC hit).
- **Covers:** Z2-I-WORLD-06, Z2-I-WORLD-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldPartition_LightingAttached` (`ZenithAuthoring_Tests`, `Test` step (`win`)); `Test_WorldImporter_StaleBakeDiagnosticsCarryCodes` (`ZenithCook_Tests`, `Test` step (`win`)), once row WD35 lands.
- **Design:** Zenith2/Docs/Design/WORLD/Documents.md → "World validation messages"; Zenith2/Docs/Design/WORLD/Bakes.md → "Lighting fingerprint"

### Z2-R-WORLD-099 Light bake documents

- **Statement:** A lighting unit writes one header document and one page document per page in the schemas of "Baked lighting documents" with derived IDs and zstd base64 payloads.
- **Covers:** Z2-I-WORLD-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-004, Z2-D-038
- **Budgets:** Z2-B-CONTENT-001
- **Acceptance evidence:** `Test_WorldLightBake_DocumentsRoundTrip`, `Test_WorldLightBakeDocument_PayloadChecks` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Documents.md → "Baked lighting documents"; Zenith2/Docs/Design/WORLD/Documents.md → "Payload objects"

### Z2-R-WORLD-100 Lightmap atlas packing

- **Statement:** Receivers are packed into square rectangles by sorted shelf packing with the bindings of "Atlas packing", and more than 256 pages is `WORLD_ERROR_ATLAS_FULL`.
- **Covers:** Z2-I-WORLD-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldLightBake_AtlasPacking` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Bakes.md → "Atlas packing"

### Z2-R-WORLD-101 Deterministic BVH

- **Statement:** The bake BVH is binned SAH with 12 bins, leaves of at most 4 triangles and stable partitions, identical across builds of one input.
- **Covers:** Z2-I-WORLD-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldLightBake_BvhDeterministic` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Bakes.md → "BVH construction"

### Z2-R-WORLD-102 Lightmaps hold indirect light

- **Statement:** Lightmap texels hold indirect irradiance plus sky irradiance from cosine-weighted path tracing, never the direct light of the scene's lights.
- **Covers:** Z2-I-WORLD-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldLightBake_SkyOnlyIrradiance`, `Test_WorldLightBake_DirectExcludedFromLightmap`, `Test_WorldLightBake_InfinitePlaneBounce` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Bakes.md → "Lighting workflow"; Zenith2/Docs/Design/WORLD/Bakes.md → "Tracing"

### Z2-R-WORLD-103 Direct light at path vertices

- **Statement:** Direct irradiance at a path vertex uses one shadow ray per light with LIGHT's unit, range and cone rules.
- **Covers:** Z2-I-WORLD-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldLightBake_BounceFromPointLight` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Bakes.md → "Tracing"; Zenith2/Docs/Design/WORLD/Bakes.md → "Bake geometry"

### Z2-R-WORLD-104 Declared sky

- **Statement:** An escaped ray receives the settings' two-hemisphere sky radiance, part of the fingerprint.
- **Covers:** Z2-I-WORLD-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldLightBake_SkyOnlyIrradiance` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Bakes.md → "Escaped rays"

### Z2-R-WORLD-105 Irradiance volumes

- **Statement:** Each lighting unit bakes one irradiance volume over its cell and content height with L1 SH multiplied by the band factors and validity 0 above a 10% back-face ratio.
- **Covers:** Z2-I-WORLD-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldLightBake_VolumeSHAndValidity` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Bakes.md → "Tracing"

### Z2-R-WORLD-106 Baked reflection probes

- **Statement:** Each baked reflection probe in a unit gets six mip-0 radiance faces traced from its capture centre in LIGHT's cube layout.
- **Covers:** Z2-I-WORLD-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldLightBake_ProbeFaces` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Bakes.md → "Tracing"

### Z2-R-WORLD-107 Lighting bake determinism

- **Statement:** A lighting unit's documents are byte-identical for any worker count and work-item order and on every machine that runs one build.
- **Covers:** Z2-I-WORLD-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldLightBake_WorkerCountInvariant` (`ZenithAuthoring_Tests`, `Test` step (`win`)). The Stage 4 acceptance bake repeated on a second host.
- **Design:** Zenith2/Docs/Design/WORLD/Bakes.md → "Tracing"

### Z2-R-WORLD-108 Lighting import

- **Statement:** The importer turns light bake documents into the `geom.textureSource` and LIGHT intermediates of "Lighting import" that LIGHT's and GEOM's steps cook.
- **Covers:** Z2-I-WORLD-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-LIGHT-019
- **Acceptance evidence:** `Test_WorldImporter_IntermediateBlobNames` (`ZenithCook_Tests`, `Test` step (`win`)), once row WD10 lands.
- **Design:** Zenith2/Docs/Design/WORLD/Bakes.md → "Lighting import"

### Z2-R-WORLD-109 Lighting bake restorable outside the history

- **Statement:** `world.bake.lighting` runs as a background operation outside the editor history, never locking it, and writes each unit as one journaled, backed-up batch whose backup the report names for "Restore this batch".
- **Covers:** Z2-I-WORLD-06, Z2-I-WORLD-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldEditor_BakeDoesNotLockHistory`, `Test_WorldEditor_BakeUnitsRestorableFromBackup`, `Test_WorldEditor_EditDuringBakeMarksUnitStale` (`ZenithEditor_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Bakes.md → "Bake execution"; Zenith2/Docs/Design/WORLD/Editing.md → "Bake panel and commands"

### Z2-R-WORLD-110 Bake geometry from cooked meshes

- **Statement:** The bake reads LOD 0 triangles, normals and UV1 of cooked meshes through GEOM's CPU reader and terrain triangles through ENV's extraction over the terrain WORLD loads, after asking the cook server to cook them, treats only static instances (no rigid body, or a STATIC one) as occluders and receivers, makes receivers only of hierarchies assigned to the lighting grid, and bakes only probes of such hierarchies.
- **Covers:** Z2-I-WORLD-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldLightBake_GeometrySourceTrianglesAndUV1` (`ZenithAuthoring_Tests`, `Test` step (`win`)). The editor's cooked mesh source, once row WD9 lands.
- **Design:** Zenith2/Docs/Design/WORLD/Bakes.md → "Bake geometry"

### Z2-R-WORLD-111 Derived navigation documents

- **Statement:** With navigation enabled the importer derives, in memory, one AI navigation document per navigation region with the members of "Derived navigation documents", its scene entries naming the navigation input scenes the 3 × 3 neighbourhood has, with their region anchors as offsets, and each hierarchy belongs to the input scene of the region that owns it.
- **Covers:** Z2-I-WORLD-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-NAV (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldNavigation_DerivedDocument`, `Test_WorldNavigation_InputsAreWorldAbsolute` (`ZenithAuthoring_Tests`, `Test` step (`win`)). Once D-SEL-NAV's ruling and rows WD11 and WD23 land.
- **Design:** Zenith2/Docs/Design/WORLD/Documents.md → "Derived navigation documents"; Zenith2/Docs/Design/WORLD/Partition.md → "Navigation input scenes"

### Z2-R-WORLD-112 Navigation inputs derived in the import

- **Statement:** Navigation input scenes and AI's navigation products are intermediates and derived products of the world's import, including cook-mode generator outputs, so no navigation file is tracked and no editor command maintains one.
- **Covers:** Z2-I-WORLD-07, Z2-I-WORLD-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-NAV (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldPartition_NavigationInputScenes`, `Test_WorldNavigation_CookModeOutputsAreInputs` (`ZenithAuthoring_Tests`, `Test` step (`win`)). `Test_WorldImporter_DeclaresNavigationProducts` (`ZenithCook_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Partition.md → "Navigation input scenes"; Zenith2/Docs/Design/WORLD/Bakes.md → "Navigation workflow"

### Z2-R-WORLD-113 Disjoint navigation tiles

- **Statement:** Derived navigation documents of different regions never declare the same tile, for every AI tile size.
- **Covers:** Z2-I-WORLD-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-NAV (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldNavigation_NoOverlappingTiles` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Documents.md → "Derived navigation documents"

### Z2-R-WORLD-114 Navigation follows every import

- **Statement:** Every import re-derives the navigation inputs and products from the current documents, so ASSET's keys re-bake exactly the tiles whose inputs changed and no stale navigation file can exist.
- **Covers:** Z2-I-WORLD-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-NAV (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldImporter_ReimportOnRegionEdit`, `Test_WorldImporter_DeclaresNavigationProducts` (`ZenithCook_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Bakes.md → "Navigation workflow"; Zenith2/Docs/Design/WORLD/Partition.md → "Navigation input scenes"

### Z2-R-WORLD-115 Navigation baked by the cook

- **Statement:** Navigation tiles are baked by AI's cook steps, the panel's bake being a cook request for the selected regions' tile products.
- **Covers:** Z2-I-WORLD-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-NAV (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldCooked_StreamsHeadless` with navigation cells (`Zenith_Integration_WorldAuthoring` (suite `z2-world-cooked`), `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Bakes.md → "Navigation workflow"

### Z2-R-WORLD-116 WORLD generates HLOD

- **Statement:** WORLD chooses HLOD nodes, eligible instances, material modes, triangle targets and error bounds and writes one merge recipe per node mesh, which GEOM's mesh step processes.
- **Covers:** Z2-I-WORLD-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldHlod_RecipeDeterministic` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD.md → "HLOD ownership"; Zenith2/Docs/Design/WORLD/Bakes.md → "HLOD workflow"

### Z2-R-WORLD-117 HLOD node hierarchy

- **Statement:** HLOD nodes form STREAM's quadtree: one level-1 node per eligible cell, a parent at floor-divided coordinates for every node below the top, every node above level 1 with a child.
- **Covers:** Z2-I-WORLD-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldHlod_NodeHierarchy` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Bakes.md → "HLOD workflow"

### Z2-R-WORLD-118 HLOD instance eligibility

- **Statement:** Only visible, shown static instances of HLOD layers at least the level's minimum radius and not culled before the refine distance contribute to a level.
- **Covers:** Z2-I-WORLD-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldHlod_Eligibility` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Bakes.md → "Instance eligibility"

### Z2-R-WORLD-119 HLOD material modes

- **Statement:** `keep` deals material groups into up to four slots of 255 materials, dropping the smallest beyond with a warning, and `flatten` gives one slot with GEOM's proxy material.
- **Covers:** Z2-I-WORLD-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldHlod_KeepModeSlots` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Bakes.md → "Material modes"

### Z2-R-WORLD-120 HLOD recipes deterministic

- **Statement:** A node's recipe bytes and mesh GUID are unchanged by edits outside the node's cells, so an unchanged node re-cooks from the DDC.
- **Covers:** Z2-I-WORLD-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldHlod_RecipeDeterministic` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Bakes.md → "Merge recipes"

### Z2-R-WORLD-121 GEOM cooks merge recipes

- **Statement:** HLOD mesh products are cooked by GEOM's mesh step from the merge recipe intermediate as row WD8 specifies.
- **Covers:** Z2-I-WORLD-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldCooked_StreamsHeadless` with HLOD nodes resident (`Zenith_Integration_WorldAuthoring` (suite `z2-world-cooked`), `Test` step (`win`)), once row WD8 lands.
- **Design:** Zenith2/Docs/Design/WORLD/Bakes.md → "Merge recipes"

### Z2-R-WORLD-122 Play in the editor

- **Statement:** Playing a streamed world cooks its primary product through the cook server and starts the play world from the persistent scene, warning about unsaved regions.
- **Covers:** Z2-I-WORLD-01, Z2-I-WORLD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-EDIT-006
- **Acceptance evidence:** `Test_WorldCooked_StreamsHeadless` (`Zenith_Integration_WorldAuthoring` (suite `z2-world-cooked`), `Test` step (`win`)) and EDIT's play tests over the fixture.
- **Design:** Zenith2/Docs/Design/WORLD/Editing.md → "Play in the editor"

### Z2-R-WORLD-123 WORLD diagnostics

- **Statement:** WORLD logs in its four categories, publishes its editor counters and zones and reports the partition summary as INFO diagnostics in the cook report.
- **Covers:** Z2-I-WORLD-01, Z2-I-WORLD-02, Z2-I-WORLD-03, Z2-I-WORLD-04, Z2-I-WORLD-05, Z2-I-WORLD-06, Z2-I-WORLD-07, Z2-I-WORLD-08, Z2-I-WORLD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldImporter_FailureDeclaresNothing` and the summary diagnostics of `Test_WorldImporter_RecordsEveryInput` (`ZenithCook_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD.md → "Log categories"; Zenith2/Docs/Design/WORLD.md → "Counters"; Zenith2/Docs/Design/WORLD.md → "Profiler zones"

### Z2-R-WORLD-124 Partition preview

- **Statement:** The partition preview draws the preview grid's cells with the loaded content's counts, HLOD nodes, lighting unit states, generator scopes and bound violations.
- **Covers:** Z2-I-WORLD-01, Z2-I-WORLD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** Review of the preview's debug-draw records in `Test_WorldEditor_ExtensionRegisters` (`ZenithEditor_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Editing.md → "Partition preview"

### Z2-R-WORLD-125 Partition check command

- **Statement:** `world.partition.check` runs the full partition in memory over the open and on-disk documents and shows its findings without changing a document.
- **Covers:** Z2-I-WORLD-01, Z2-I-WORLD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldEditor_ExtensionRegisters` (`ZenithEditor_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Editing.md → "World commands"

### Z2-R-WORLD-126 WORLD budget producer

- **Statement:** The Stage 1 per-system budget ticket for WORLD allocates the five pending WORLD rows before any WORLD ticket cites them.
- **Covers:** Z2-I-WORLD-01, Z2-I-WORLD-02, Z2-I-WORLD-03, Z2-I-WORLD-04, Z2-I-WORLD-05, Z2-I-WORLD-06, Z2-I-WORLD-07, Z2-I-WORLD-08, Z2-I-WORLD-09
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-012, Z2-D-084
- **Budgets:** none
- **Acceptance evidence:** The producer ticket's diff of `Zenith2/Docs/Budgets.md` once core-doc item X304 lands.
- **Design:** Zenith2/Docs/Design/WORLD.md → "Pending row needs"

### Z2-R-WORLD-127 OpenWorldSlice world

- **Statement:** OpenWorldSlice's world is authored as regions and layers, cooked into streamed cells, baked with no stale unit and played through its playthrough so far at Stage 4.
- **Covers:** Z2-I-WORLD-01, Z2-I-WORLD-02, Z2-I-WORLD-03, Z2-I-WORLD-04, Z2-I-WORLD-05, Z2-I-WORLD-06, Z2-I-WORLD-07, Z2-I-WORLD-08, Z2-I-WORLD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-021
- **Budgets:** Z2-B-STREAM-001, Z2-B-STREAM-002, Z2-B-STREAM-004, Z2-B-STREAM-005
- **Acceptance evidence:** The Stage 4 acceptance run: the slice cook, the headless `world.bake` report and the playthrough so far.
- **Design:** Zenith2/Docs/Design/WORLD.md → "Acceptance evidence"

### Z2-R-WORLD-128 Stale bakes block releases

- **Statement:** A Shipping package is refused while the cook report of its tree holds a diagnostic whose `code` is `WORLD_WARNING_BAKE_STALE` or `WORLD_WARNING_BAKE_MISSING`, including entries replayed for a world import that did not run in that cook (rows WD14 (b), WD35; PACK's PK26).
- **Covers:** Z2-I-WORLD-06, Z2-I-WORLD-05
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** PACK's `Test_PackageMode_StaleBakesRefuseShipping` (its fixture: replayed diagnostics of a world import that did not run) and WORLD's end-to-end `Test_WorldPackage_StaleBakeRefusedAndNoRegionShipped` (`ZenithCook_Tests`, `Test` step (`win`)), once rows WD14 (b) and WD35 land.
- **Design:** Zenith2/Docs/Design/WORLD.md → "Reconciliations with other documents"

### Z2-R-WORLD-129 Region documents are never packaged

- **Statement:** Region and generated region documents are input-only documents with no `.meta` and no product of their own, so no region scene is cooked standalone or reaches a package.
- **Covers:** Z2-I-WORLD-01, Z2-I-WORLD-09
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-PACK-002
- **Acceptance evidence:** `Test_WorldImporter_RegionsAreInputOnly` (`ZenithCook_Tests`, `Test` step (`win`)), once row WD21 lands.
- **Design:** Zenith2/Docs/Design/WORLD/Documents.md → "Region documents"; Zenith2/Docs/Design/WORLD/Documents.md → "Document set"

### Z2-R-WORLD-130 Layer identities derive from the layer id

- **Statement:** Every persisted identity of a layer (its region keys and paths, its layer grid name, its layer scene product key) derives from its `id`, so renaming a layer edits one keyed child of `layers` and changes no product GUID or intermediate byte.
- **Covers:** Z2-I-WORLD-02, Z2-I-WORLD-03, Z2-I-WORLD-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldPartition_LayerRenameKeepsProductGUIDs`, `Test_WorldIdentity_KeysFromIDs` (`ZenithAuthoring_Tests`, `Test` step (`win`)). `Test_WorldEditor_LayerRenameEditsOneKey` (`ZenithEditor_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Documents.md → "Layer objects"; Zenith2/Docs/Design/WORLD/Partition.md → "Product identity"

### Z2-R-WORLD-131 Stable layer grid indices

- **Statement:** A layer grid's index is `layerGridBase` plus its position in `layerGridOrder`, which only grows, so a saved `GridMask` names the same layers after any layer is added, renamed or deleted; a base-grid change is refused (W28) until `world.streaming.renumberLayerGrids` states the save consequence.
- **Covers:** Z2-I-WORLD-02, Z2-I-WORLD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldPartition_LayerGridIndicesStable`, `Test_WorldDocument_LayerGridOrderRules` (`ZenithAuthoring_Tests`, `Test` step (`win`)). `Test_WorldEditor_RenumberLayerGrids` (`ZenithEditor_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Documents.md → "Streaming object"; Zenith2/Docs/Design/WORLD/Partition.md → "Layer realisation"

### Z2-R-WORLD-132 Initial layer state is cooked

- **Statement:** Each layer grid object of the streaming world document carries `defaultActive` equal to the layer's `initiallyActive`, and a source whose override masks gameplay never wrote activates no cell of a layer whose `initiallyActive` is false.
- **Covers:** Z2-I-WORLD-02, Z2-I-WORLD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldPartition_LayerGridRealisation`, `Test_WorldPartition_StreamDocumentShape` (`ZenithAuthoring_Tests`, `Test` step (`win`)). `Test_WorldCooked_LayerInitiallyInactiveByDefault` (`Zenith_Integration_WorldAuthoring` (suite `z2-world-cooked`), `Test` step (`win`)), once row WD19 lands.
- **Design:** Zenith2/Docs/Design/WORLD/Partition.md → "Layer realisation"; Zenith2/Docs/Design/WORLD/Partition.md → "Streaming world document generation"

### Z2-R-WORLD-133 Runtime layer collision follows its layer

- **Statement:** A runtime grid layer's static colliders stay in its own layer grid and never reach the base collision grid, so with the layer's bit clear nothing of it collides.
- **Covers:** Z2-I-WORLD-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldPartition_LayerGridRealisation` (`ZenithAuthoring_Tests`, `Test` step (`win`)). `Test_WorldCooked_GameplayLayerToggle` (`Zenith_Integration_WorldAuthoring` (suite `z2-world-cooked`), `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Partition.md → "Static collider split"; Zenith2/Docs/Design/WORLD/Partition.md → "Layer realisation"

### Z2-R-WORLD-134 Collider proxies carry their source

- **Statement:** A collider proxy names its source entity in `Zenith.Physics.ColliderProxy`, its contacts, hits and trigger events report the source, and destroying the source removes the proxy's collision.
- **Covers:** Z2-I-WORLD-01, Z2-I-WORLD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldPartition_StaticColliderSplit` (`ZenithAuthoring_Tests`, `Test` step (`win`)). `Test_WorldCooked_DestroyedSourceRemovesProxyCollision` (`Zenith_Integration_WorldAuthoring` (suite `z2-world-cooked`), `Test` step (`win`)), once row WD20 lands.
- **Design:** Zenith2/Docs/Design/WORLD/Partition.md → "Static collider split"

### Z2-R-WORLD-135 Grid roles conform to STREAM

- **Statement:** Kinematic-platform hierarchies and pure trigger and static-collider hierarchies are assigned to the collision grid as STREAM's role table requires; trigger colliders of mixed gameplay hierarchies stay with their hierarchy as row WD19 records.
- **Covers:** Z2-I-WORLD-01, Z2-I-WORLD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldPartition_RoleRules` (`ZenithAuthoring_Tests`, `Test` step (`win`)). `Test_WorldCooked_KinematicPlatformInCollisionCell` (`Zenith_Integration_WorldAuthoring` (suite `z2-world-cooked`), `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Partition.md → "Grid assignment"

### Z2-R-WORLD-136 Persistent scene references reported

- **Statement:** Every `ENTITY` reference from the persistent scene to region content is reported as `WORLD_WARNING_REFERENCE_UNBINDABLE` with both paths.
- **Covers:** Z2-I-WORLD-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldPartition_PersistentSceneReferencesReported` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Partition.md → "Cross-cell references"

### Z2-R-WORLD-137 Partition pre-checks STREAM limits

- **Statement:** Before declaring any product the partition checks STREAM's cell coordinate bound (V8), dependencies per cell (V19) and worst-case live instances (V23) with STREAM's formulas, so `stream.world` never fails on a world the import accepted.
- **Covers:** Z2-I-WORLD-01, Z2-I-WORLD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-STREAM-001
- **Acceptance evidence:** `Test_WorldPartition_Limits` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Partition.md → "Partition limits"

### Z2-R-WORLD-138 Tool outputs identical across machines

- **Statement:** Every tracked WORLD output and every intermediate is byte-identical on every machine that runs one build, because WORLD's tool code calls only CORE's deterministic transcendentals and IEEE square root ([Z2-N-ARCH-888]) and every tool main calls `Zenith_ApplyCrtMathMode()` first ([Z2-N-ARCH-889]).
- **Covers:** Z2-I-WORLD-05, Z2-I-WORLD-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** The Stage 4 acceptance bake repeated on a second host (human); review of WORLD's sources for CRT transcendental calls, once CORE's Z2T-CORE-100 lands.
- **Design:** Zenith2/Docs/Design/WORLD.md → "Determinism obligations"; Zenith2/Docs/Design/WORLD/Bakes.md → "Tracing"

### Z2-R-WORLD-139 WORLD work items never block

- **Statement:** No WORLD function starts, joins or waits on a job: the importer runs WORLD work inline, the editor submits each work item as a background job at the tools-host priority of ruling X350 observed from `Poll`, and tests use `RunRoot`.
- **Covers:** Z2-I-WORLD-01, Z2-I-WORLD-02, Z2-I-WORLD-03, Z2-I-WORLD-04, Z2-I-WORLD-05, Z2-I-WORLD-06, Z2-I-WORLD-07, Z2-I-WORLD-08, Z2-I-WORLD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldGenerator_WorkerCountInvariant`, `Test_WorldLightBake_WorkerCountInvariant` (`ZenithAuthoring_Tests`, `Test` step (`win`)). `Test_WorldEditor_BakeDoesNotLockHistory` (`ZenithEditor_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD.md → "Rules across the WORLD API"; Zenith2/Docs/Design/WORLD/Editing.md → "World editor extension"

### Z2-R-WORLD-140 Terrain editing granularity

- **Statement:** Every 128 m terrain tile belongs to the region holding its minimum corner, a stroke touches only the tile documents under its brush, tile documents merge whole-file, and the terrain tool warns before a stroke over a tile another user has locked.
- **Covers:** Z2-I-WORLD-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-CONTENT-001
- **Acceptance evidence:** Review of the terrain tool's lock warning and `Test_WorldDocument_EveryValidationMessage` for W14 (`ZenithAuthoring_Tests`, `Test` step (`win`)), once row WD24 lands.
- **Design:** Zenith2/Docs/Design/WORLD/Editing.md → "Terrain editing granularity"

### Z2-R-WORLD-141 Generated edits survive schema migration

- **Statement:** A generated region document whose SCENE schema version differs from its record is compared with a fresh generation, not by digest, so a migration or re-canonicalisation alone never makes it EDITED.
- **Covers:** Z2-I-WORLD-05, Z2-I-WORLD-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldGenerator_EditedDetectionAcrossSchemaVersion` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Generation.md → "Generator outputs and staleness"

### Z2-R-WORLD-142 Editor WORLD work in tools-host jobs

- **Statement:** Every WORLD computation of the editor (the partition, generator prepare and region jobs with their generated documents' bytes, light-bake job building, prepare, tracing, finishing and encoding) runs in background jobs at the tools-host priority `JOB_PRIORITY_BACKGROUND_TOOL` of [Z2-N-ARCH-443], and the editor's main thread runs no WORLD computation except the partition preview's per-frame assignment, bounded by `uWORLD_EDITOR_PREVIEW_ROOTS_PER_FRAME`.
- **Covers:** Z2-I-WORLD-01, Z2-I-WORLD-05, Z2-I-WORLD-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldEditor_NoHeavyWorkOnMainThread`, `Test_WorldEditor_PreviewBoundedPerFrame` (`ZenithEditor_Tests`, `Test` step (`win`)), JOBS's declaration being `Zenith2/Docs/Design/JOBS/Algorithms.md` → "Tools-host background priority".
- **Design:** Zenith2/Docs/Design/WORLD/Editing.md → "World editor extension"; Zenith2/Docs/Design/WORLD/Bakes.md → "Bake execution"

### Z2-R-WORLD-143 Editor plans read saved documents

- **Statement:** The editor's plan operation computes the partition, the LOD 0 digests and the bake plan from the saved documents in the background, coalescing refresh requests; bakes and generations pin one plan and refuse to start while a document it read is unsaved or newer on disk.
- **Covers:** Z2-I-WORLD-01, Z2-I-WORLD-04, Z2-I-WORLD-05, Z2-I-WORLD-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldEditor_PlanOperationRefreshesAfterEdit`, `Test_WorldEditor_BakeRefusedWhileInputsDirty`, `Test_WorldEditor_EditDuringBakeMarksUnitStale` (`ZenithEditor_Tests`, `Test` step (`win`)), once row WD31 lands.
- **Design:** Zenith2/Docs/Design/WORLD/Editing.md → "World editor extension"; Zenith2/Docs/Design/WORLD/Editing.md → "Bake panel and commands"

### Z2-R-WORLD-144 Lighting fingerprint names the traced geometry

- **Statement:** A lighting fingerprint hashes each mesh's published LOD 0 geometry digest, never a cook key, and its membership, the bake's occluders and receivers and HLOD's size test use the bounding sphere of the same published summary; the header records the fingerprint recomputed from the digests of the geometry the tracer decoded; every list of the fingerprint is count-prefixed.
- **Covers:** Z2-I-WORLD-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldLightBake_TracedFingerprintNamesDecodedGeometry`, `Test_WorldBake_FingerprintChangesPerInput` (`ZenithAuthoring_Tests`, `Test` step (`win`)), once rows WD30 and WD31 land.
- **Design:** Zenith2/Docs/Design/WORLD/Bakes.md → "Lighting fingerprint"; Zenith2/Docs/Design/WORLD/Bakes.md → "Bake execution"

### Z2-R-WORLD-145 Terrain tiles are recorded inputs

- **Statement:** WORLD loads terrain only through ENV's document API over its source access, so every terrain and tile document the world reads is an import input and a baked generator fingerprint input, and a sculpt or paint stroke re-imports the world, re-places cook-mode scatter and recomputes lighting staleness.
- **Covers:** Z2-I-WORLD-05, Z2-I-WORLD-06, Z2-I-WORLD-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldGenerator_FingerprintCoversTerrainTiles` (`ZenithAuthoring_Tests`, `Test` step (`win`)); `Test_WorldImporter_ReimportOnTerrainStroke` (`ZenithCook_Tests`, `Test` step (`win`)), once row WD31 lands.
- **Design:** Zenith2/Docs/Design/WORLD/Partition.md → "Importer inputs"; Zenith2/Docs/Design/WORLD/Partition.md → "Terrain loading"; Zenith2/Docs/Design/WORLD/Generation.md → "Generator outputs and staleness"

### Z2-R-WORLD-146 Layers and generators referenced by identity

- **Statement:** Every reference to a layer or a generator in a WORLD document holds its `id`, generator files and map keys use the generator key, and a generator's display name lives in its document alone, so renaming a layer or a generator edits one member of one file and changes no path, key, fingerprint or intermediate byte.
- **Covers:** Z2-I-WORLD-02, Z2-I-WORLD-03, Z2-I-WORLD-04, Z2-I-WORLD-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldPartition_LayerRenameKeepsProductGUIDs` (`ZenithAuthoring_Tests`, `Test` step (`win`)); `Test_WorldEditor_GeneratorRenameEditsOneMember`, `Test_WorldEditor_LayerRenameEditsOneKey` (`ZenithEditor_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Documents.md → "Generator references"; Zenith2/Docs/Design/WORLD/Documents.md → "Generator document"; Zenith2/Docs/Design/WORLD/Editing.md → "World commands"

### Z2-R-WORLD-147 Obstacles gathered by distance

- **Statement:** A region job's obstacles are every world obstacle within its built-in's reach plus the obstacle radius of the region rectangle, so the obstacles an output sees depend on positions alone and generated output is independent of the region size.
- **Covers:** Z2-I-WORLD-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldGenerator_RegionSizeInvariant`, `Test_WorldGenerator_ObstaclesSorted` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Generation.md → "Obstacles and dependencies"; Zenith2/Docs/Design/WORLD/Generation.md → "Running a generator"

### Z2-R-WORLD-148 Explicit static bodies are static

- **Statement:** An entity whose body is none or a `PHYSICS_MOTION_TYPE_STATIC` rigid body is static for the role rules, W27, the lighting bake and HLOD; an explicit static body is never split and its grid obeys the radius rule.
- **Covers:** Z2-I-WORLD-01, Z2-I-WORLD-06, Z2-I-WORLD-08, Z2-I-WORLD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldPartition_RoleRules`, `Test_WorldPartition_StaticColliderSplit`, `Test_WorldLightBake_GeometrySourceTrianglesAndUV1`, `Test_WorldHlod_Eligibility` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Partition.md → "Grid assignment"; Zenith2/Docs/Design/WORLD/Partition.md → "Static collider split"; Zenith2/Docs/Design/WORLD/Bakes.md → "Instance eligibility"

### Z2-R-WORLD-149 Terrain-only navigation regions

- **Statement:** Every region coordinate intersecting a navigation-enabled terrain of the world is a navigation region with a derived document naming that terrain, so agents path across open ground with no authored content.
- **Covers:** Z2-I-WORLD-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-NAV (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldNavigation_TerrainOnlyRegionsGetDocuments`, `Test_WorldNavigation_MembershipByOwner`, `Test_WorldNavigation_NoOverlappingTiles` (`ZenithAuthoring_Tests`, `Test` step (`win`)), once D-SEL-NAV's ruling and row WD34 land.
- **Design:** Zenith2/Docs/Design/WORLD/Documents.md → "Derived navigation documents"; Zenith2/Docs/Design/WORLD/Partition.md → "Navigation input scenes"

### Z2-R-WORLD-150 Unstreamed worlds reference their navigation

- **Statement:** In an unstreamed world the partition writes one `Zenith.AI.NavMeshRef` entity per derived navigation manifest into the content scene and calls ENV's contributor once per role over the whole world's extent.
- **Covers:** Z2-I-WORLD-01, Z2-I-WORLD-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldPartition_UnstreamedContentScene` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Partition.md → "Unstreamed worlds"; Zenith2/Docs/Design/WORLD/Partition.md → "Contributed cell content"

### Z2-R-WORLD-151 Pinned plans are the one editor snapshot

- **Statement:** Every document an editor plan or a pinned bake or generation reads is checked against the plan's read table of file digests or recorded at its first open, a plan run that may have read across a save is discarded, and a bake unit or generated region whose read paths changed after the plan started is FAILED or refused, so no job reads bytes the plan did not record.
- **Covers:** Z2-I-WORLD-04, Z2-I-WORLD-05, Z2-I-WORLD-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldEditor_TileSavedDuringBakeFailsUnit`, `Test_WorldEditor_PlanDiscardedOnConcurrentSave`, `Test_WorldEditor_EditDuringBakeMarksUnitStale`, `Test_WorldEditor_BakeAndGenerateExclusive` (`ZenithEditor_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Editing.md → "World editor extension"; Zenith2/Docs/Design/WORLD/Bakes.md → "Bake execution"

### Z2-R-WORLD-152 Baked scatter covers its surface reach and exclusions

- **Statement:** A baked generator region's fingerprint hashes the terrain tiles within its built-in's surface reach plus 128 m (1,280 m for `world.scatter`) and, for `world.scatter`, every exclusion shape within its reach, so a stroke or an exclusion move that a regeneration could answer differently makes the region STALE.
- **Covers:** Z2-I-WORLD-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldGenerator_FingerprintCoversTerrainTiles`, `Test_WorldGenerator_ExclusionMoveStalesBakedScatter` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Generation.md → "Obstacles and dependencies"; Zenith2/Docs/Design/WORLD/Generation.md → "Generator outputs and staleness"

### Z2-R-WORLD-153 Layer grids fit STREAM's live-instance bound

- **Statement:** A layer grid depends on the collision grid and on a grid of STREAM role `navigation` only, a retired layer slot depends on nothing, and the partition checks the resulting worst-case live instances against 192 with STREAM's formula, the worked bound of "Partition limits" included.
- **Covers:** Z2-I-WORLD-02, Z2-I-WORLD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-STREAM-001
- **Acceptance evidence:** `Test_WorldPartition_Limits`, `Test_WorldPartition_LayerGridRealisation` (`ZenithAuthoring_Tests`, `Test` step (`win`)).
- **Design:** Zenith2/Docs/Design/WORLD/Partition.md → "Partition algorithm"; Zenith2/Docs/Design/WORLD/Partition.md → "Partition limits"

## Coverage of inventory items

| Item | Text | Requirements |
|---|---|---|
| Z2-I-WORLD-01 | editable world partitions | Z2-R-WORLD-001, Z2-R-WORLD-002, Z2-R-WORLD-003, Z2-R-WORLD-004, Z2-R-WORLD-005, Z2-R-WORLD-007, Z2-R-WORLD-008, Z2-R-WORLD-009, Z2-R-WORLD-010, Z2-R-WORLD-011, Z2-R-WORLD-012, Z2-R-WORLD-013, Z2-R-WORLD-014, Z2-R-WORLD-015, Z2-R-WORLD-018, Z2-R-WORLD-027, Z2-R-WORLD-028, Z2-R-WORLD-029, Z2-R-WORLD-030, Z2-R-WORLD-031, Z2-R-WORLD-032, Z2-R-WORLD-033, Z2-R-WORLD-034, Z2-R-WORLD-035, Z2-R-WORLD-036, Z2-R-WORLD-037, Z2-R-WORLD-038, Z2-R-WORLD-039, Z2-R-WORLD-040, Z2-R-WORLD-041, Z2-R-WORLD-042, Z2-R-WORLD-043, Z2-R-WORLD-044, Z2-R-WORLD-045, Z2-R-WORLD-046, Z2-R-WORLD-047, Z2-R-WORLD-048, Z2-R-WORLD-051, Z2-R-WORLD-052, Z2-R-WORLD-053, Z2-R-WORLD-058, Z2-R-WORLD-059, Z2-R-WORLD-063, Z2-R-WORLD-064, Z2-R-WORLD-122, Z2-R-WORLD-123, Z2-R-WORLD-124, Z2-R-WORLD-125, Z2-R-WORLD-126, Z2-R-WORLD-127, Z2-R-WORLD-129, Z2-R-WORLD-134, Z2-R-WORLD-135, Z2-R-WORLD-136, Z2-R-WORLD-137, Z2-R-WORLD-139, Z2-R-WORLD-142, Z2-R-WORLD-143, Z2-R-WORLD-148, Z2-R-WORLD-150 |
| Z2-I-WORLD-02 | gameplay layers | Z2-R-WORLD-001, Z2-R-WORLD-002, Z2-R-WORLD-004, Z2-R-WORLD-005, Z2-R-WORLD-016, Z2-R-WORLD-019, Z2-R-WORLD-020, Z2-R-WORLD-021, Z2-R-WORLD-022, Z2-R-WORLD-023, Z2-R-WORLD-024, Z2-R-WORLD-123, Z2-R-WORLD-126, Z2-R-WORLD-127, Z2-R-WORLD-130, Z2-R-WORLD-131, Z2-R-WORLD-132, Z2-R-WORLD-133, Z2-R-WORLD-139, Z2-R-WORLD-146, Z2-R-WORLD-153 |
| Z2-I-WORLD-03 | data layers | Z2-R-WORLD-001, Z2-R-WORLD-002, Z2-R-WORLD-004, Z2-R-WORLD-005, Z2-R-WORLD-016, Z2-R-WORLD-017, Z2-R-WORLD-018, Z2-R-WORLD-025, Z2-R-WORLD-026, Z2-R-WORLD-123, Z2-R-WORLD-126, Z2-R-WORLD-127, Z2-R-WORLD-130, Z2-R-WORLD-139, Z2-R-WORLD-146 |
| Z2-I-WORLD-04 | concurrent editing granularity | Z2-R-WORLD-001, Z2-R-WORLD-002, Z2-R-WORLD-004, Z2-R-WORLD-005, Z2-R-WORLD-010, Z2-R-WORLD-014, Z2-R-WORLD-025, Z2-R-WORLD-026, Z2-R-WORLD-042, Z2-R-WORLD-044, Z2-R-WORLD-051, Z2-R-WORLD-054, Z2-R-WORLD-055, Z2-R-WORLD-056, Z2-R-WORLD-057, Z2-R-WORLD-058, Z2-R-WORLD-059, Z2-R-WORLD-060, Z2-R-WORLD-061, Z2-R-WORLD-062, Z2-R-WORLD-064, Z2-R-WORLD-065, Z2-R-WORLD-071, Z2-R-WORLD-084, Z2-R-WORLD-085, Z2-R-WORLD-109, Z2-R-WORLD-112, Z2-R-WORLD-123, Z2-R-WORLD-126, Z2-R-WORLD-127, Z2-R-WORLD-130, Z2-R-WORLD-139, Z2-R-WORLD-140, Z2-R-WORLD-141, Z2-R-WORLD-143, Z2-R-WORLD-145, Z2-R-WORLD-146, Z2-R-WORLD-151 |
| Z2-I-WORLD-05 | reproducible procedural generation | Z2-R-WORLD-001, Z2-R-WORLD-002, Z2-R-WORLD-005, Z2-R-WORLD-015, Z2-R-WORLD-066, Z2-R-WORLD-067, Z2-R-WORLD-068, Z2-R-WORLD-069, Z2-R-WORLD-070, Z2-R-WORLD-071, Z2-R-WORLD-072, Z2-R-WORLD-073, Z2-R-WORLD-074, Z2-R-WORLD-075, Z2-R-WORLD-076, Z2-R-WORLD-077, Z2-R-WORLD-078, Z2-R-WORLD-079, Z2-R-WORLD-080, Z2-R-WORLD-081, Z2-R-WORLD-082, Z2-R-WORLD-083, Z2-R-WORLD-084, Z2-R-WORLD-085, Z2-R-WORLD-086, Z2-R-WORLD-087, Z2-R-WORLD-088, Z2-R-WORLD-089, Z2-R-WORLD-091, Z2-R-WORLD-098, Z2-R-WORLD-123, Z2-R-WORLD-126, Z2-R-WORLD-127, Z2-R-WORLD-128, Z2-R-WORLD-138, Z2-R-WORLD-139, Z2-R-WORLD-141, Z2-R-WORLD-142, Z2-R-WORLD-143, Z2-R-WORLD-145, Z2-R-WORLD-146, Z2-R-WORLD-147, Z2-R-WORLD-151, Z2-R-WORLD-152 |
| Z2-I-WORLD-06 | baking workflow: lighting | Z2-R-WORLD-001, Z2-R-WORLD-002, Z2-R-WORLD-005, Z2-R-WORLD-015, Z2-R-WORLD-024, Z2-R-WORLD-041, Z2-R-WORLD-050, Z2-R-WORLD-062, Z2-R-WORLD-082, Z2-R-WORLD-088, Z2-R-WORLD-089, Z2-R-WORLD-090, Z2-R-WORLD-091, Z2-R-WORLD-092, Z2-R-WORLD-093, Z2-R-WORLD-094, Z2-R-WORLD-095, Z2-R-WORLD-096, Z2-R-WORLD-097, Z2-R-WORLD-098, Z2-R-WORLD-099, Z2-R-WORLD-100, Z2-R-WORLD-101, Z2-R-WORLD-102, Z2-R-WORLD-103, Z2-R-WORLD-104, Z2-R-WORLD-105, Z2-R-WORLD-106, Z2-R-WORLD-107, Z2-R-WORLD-108, Z2-R-WORLD-109, Z2-R-WORLD-110, Z2-R-WORLD-123, Z2-R-WORLD-126, Z2-R-WORLD-127, Z2-R-WORLD-128, Z2-R-WORLD-138, Z2-R-WORLD-139, Z2-R-WORLD-142, Z2-R-WORLD-143, Z2-R-WORLD-144, Z2-R-WORLD-145, Z2-R-WORLD-148, Z2-R-WORLD-151 |
| Z2-I-WORLD-07 | baking workflow: navigation | Z2-R-WORLD-001, Z2-R-WORLD-002, Z2-R-WORLD-005, Z2-R-WORLD-024, Z2-R-WORLD-049, Z2-R-WORLD-062, Z2-R-WORLD-088, Z2-R-WORLD-090, Z2-R-WORLD-094, Z2-R-WORLD-096, Z2-R-WORLD-111, Z2-R-WORLD-112, Z2-R-WORLD-113, Z2-R-WORLD-114, Z2-R-WORLD-115, Z2-R-WORLD-123, Z2-R-WORLD-126, Z2-R-WORLD-127, Z2-R-WORLD-139, Z2-R-WORLD-149, Z2-R-WORLD-150 |
| Z2-I-WORLD-08 | baking workflow: HLOD | Z2-R-WORLD-001, Z2-R-WORLD-002, Z2-R-WORLD-005, Z2-R-WORLD-024, Z2-R-WORLD-039, Z2-R-WORLD-041, Z2-R-WORLD-088, Z2-R-WORLD-090, Z2-R-WORLD-094, Z2-R-WORLD-096, Z2-R-WORLD-116, Z2-R-WORLD-117, Z2-R-WORLD-118, Z2-R-WORLD-119, Z2-R-WORLD-120, Z2-R-WORLD-121, Z2-R-WORLD-123, Z2-R-WORLD-126, Z2-R-WORLD-127, Z2-R-WORLD-139, Z2-R-WORLD-148 |
| Z2-I-WORLD-09 | authoring contracts separate from runtime streaming | Z2-R-WORLD-001, Z2-R-WORLD-002, Z2-R-WORLD-003, Z2-R-WORLD-005, Z2-R-WORLD-006, Z2-R-WORLD-007, Z2-R-WORLD-009, Z2-R-WORLD-011, Z2-R-WORLD-012, Z2-R-WORLD-017, Z2-R-WORLD-020, Z2-R-WORLD-023, Z2-R-WORLD-027, Z2-R-WORLD-028, Z2-R-WORLD-030, Z2-R-WORLD-031, Z2-R-WORLD-032, Z2-R-WORLD-033, Z2-R-WORLD-035, Z2-R-WORLD-036, Z2-R-WORLD-037, Z2-R-WORLD-038, Z2-R-WORLD-039, Z2-R-WORLD-040, Z2-R-WORLD-041, Z2-R-WORLD-042, Z2-R-WORLD-043, Z2-R-WORLD-046, Z2-R-WORLD-047, Z2-R-WORLD-048, Z2-R-WORLD-049, Z2-R-WORLD-050, Z2-R-WORLD-052, Z2-R-WORLD-053, Z2-R-WORLD-056, Z2-R-WORLD-070, Z2-R-WORLD-122, Z2-R-WORLD-123, Z2-R-WORLD-124, Z2-R-WORLD-125, Z2-R-WORLD-126, Z2-R-WORLD-127, Z2-R-WORLD-129, Z2-R-WORLD-131, Z2-R-WORLD-132, Z2-R-WORLD-134, Z2-R-WORLD-135, Z2-R-WORLD-137, Z2-R-WORLD-139, Z2-R-WORLD-148, Z2-R-WORLD-153 |

## Excluded and deferred

Every WORLD inventory item is required at Stage 4; no WORLD requirement is deferred, excluded or game-side. Z2-R-WORLD-126 is the Stage 1 budget producer; Z2-R-WORLD-128 and Z2-R-WORLD-129 serve the Stage 6 packaging acceptance through PACK. The navigation requirements wait on D-SEL-NAV (open, Stage 1/2) and the recorded goldens on D-SEL-HASH (open, Stage 1) without changing their stage. The boundaries that keep WORLD bounded are exclusions of other capabilities from it:

| Not in WORLD | Rationale | Decision or owner |
|---|---|---|
| runtime streaming, cell states, deltas, HLOD display | STREAM runs what WORLD cooks | STREAM (`Zenith2/Docs/Design/WORLD.md` → "Boundary with STREAM") |
| dynamic generator plugins | static modules only | Z2-D-024 |
| mod-supplied worlds or generators | modding is excluded | Z2-D-075 |
| run-time GI evaluation, navmesh stages, mesh processing | owned by LIGHT, AI, GEOM | their designs |
| rect and disc lights in the Stage 4 bake | Stage 5 light types | LIGHT's Stage 5 revalidation |
