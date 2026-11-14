# STREAM requirement catalogue

## Purpose

This catalogue breaks the STREAM inventory items (`Zenith2/Docs/Requirements/Inventory.md` → "Inventory v1", table "STREAM World streaming") into testable requirements `Z2-R-STREAM-NNN`. Each requirement names the inventory items it covers, the stage whose acceptance it serves, its classification, its governing decisions and budget rows, the artefact that proves it and the gate step or acceptance suite that runs it, and the headings of the STREAM design family (`Zenith2/Docs/Design/STREAM.md` and its four section files) that specify it; the design entry's "Requirement index" is the reverse map. STREAM owns every requirement here; where another area supplies part of the evidence (SCENE's hook and origin, SAVE's cross-system tests, PHYS's freeze, AI's suspension, AUDIO's virtualisation, LIGHT's residency, RPROD's full-state extraction, DEVT's panel), the evidence names it and the cross-area row is listed in `Zenith2/Docs/Design/STREAM.md` → "Reconciliations with other documents". Requirements whose evidence depends on another area's pending declaration say "once row ST<n> lands".

## Requirements

### Z2-R-STREAM-001 Module library target

- **Statement:** `ZenithStreaming` is a `KIND MODULE` static library with `MODULE_NAME Streaming` and `SUPPORTED ZENITH_GPU`, whose `CMakeLists.txt` is exactly the call of "ZenithStreaming target", contains no platform-specific source, and is linked into a Player only when the game's `game.json` selects `Streaming`.
- **Covers:** Z2-I-STREAM-01, Z2-I-STREAM-02, Z2-I-STREAM-03, Z2-I-STREAM-04, Z2-I-STREAM-05, Z2-I-STREAM-06, Z2-I-STREAM-07, Z2-I-STREAM-08, Z2-I-STREAM-09, Z2-I-STREAM-10, Z2-I-STREAM-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-024, Z2-D-002
- **Budgets:** none
- **Acceptance evidence:** `Libgraph` (declared edges, with the three edges of row ST1 reported as the pending X row until the core-doc batch applies it), `CompileCoverage` and `SourceLayout` on the scaffold ticket.
- **Design:** Zenith2/Docs/Design/STREAM.md → "ZenithStreaming target"; Zenith2/Docs/Design/STREAM.md → "Supported matrix"; Zenith2/Docs/Design/STREAM.md → "Libraries STREAM depends on"

### Z2-R-STREAM-002 Public headers carry contracts

- **Statement:** Every public type of the eight ZenithStreaming root headers carries one contract block with the six fields (plus Reentrancy for interfaces), every fallible public function returns `Zenith_Result<…, Zenith_StreamingError>`, and each header compiles alone with no STL type and no third-party header.
- **Covers:** Z2-I-STREAM-01, Z2-I-STREAM-02, Z2-I-STREAM-03, Z2-I-STREAM-04, Z2-I-STREAM-05, Z2-I-STREAM-06, Z2-I-STREAM-07, Z2-I-STREAM-08, Z2-I-STREAM-09, Z2-I-STREAM-10, Z2-I-STREAM-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-047, Z2-D-045, Z2-D-041
- **Budgets:** none
- **Acceptance evidence:** `ApiContractCheck`, `HeaderSelfContained` and `ForbiddenCheck` over `Zenith2/Engine/ZenithStreaming/*.h`.
- **Design:** Zenith2/Docs/Design/STREAM/PublicAPI.md → "Header list"; Zenith2/Docs/Design/STREAM.md → "Rules across the STREAM API"; Zenith2/Docs/Design/STREAM.md → "Public root headers"

### Z2-R-STREAM-003 API-only slices land first

- **Statement:** The four API-only slices (types and errors; components; world asset and writer; module entry points) land as declarations with contract blocks before their implementations, so WORLD, ENV, LIGHT, PHYS, AI, AUDIO and RPROD tickets build against them.
- **Covers:** Z2-I-STREAM-01, Z2-I-STREAM-02, Z2-I-STREAM-03, Z2-I-STREAM-04, Z2-I-STREAM-05, Z2-I-STREAM-06, Z2-I-STREAM-07, Z2-I-STREAM-08, Z2-I-STREAM-09, Z2-I-STREAM-10, Z2-I-STREAM-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-070
- **Budgets:** none
- **Acceptance evidence:** The slice tickets' `Build` and `ApiContractCheck` steps; the dependent tickets' `blocks` links name the slices.
- **Design:** Zenith2/Docs/Design/STREAM.md → "API-only slices"; Zenith2/Docs/Design/STREAM.md → "Stage 3 sprint pull-forward"

### Z2-R-STREAM-004 Module entry points

- **Statement:** `Zenith_Module_Streaming_RegisterTypes` registers only STREAM's types and configuration keys, and `Zenith_Module_Streaming_Start` performs exactly the seven steps of "Zenith_Module_Streaming header" in order, returning the module instance.
- **Covers:** Z2-I-STREAM-01, Z2-I-STREAM-02, Z2-I-STREAM-03, Z2-I-STREAM-04, Z2-I-STREAM-05, Z2-I-STREAM-06, Z2-I-STREAM-07, Z2-I-STREAM-08, Z2-I-STREAM-09, Z2-I-STREAM-10, Z2-I-STREAM-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-024, Z2-D-081
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingModule_RegisterTypesOnlyTypesAndKeys`, `Test_StreamingModule_StartRegistersEverything` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/PublicAPI.md → "Zenith_Module_Streaming header"

### Z2-R-STREAM-005 Construction steps unwind

- **Statement:** Start's steps 2 to 7 (fault points 0x53570001 to 0x53570006, in the one step order shared by the prose, the fault-point list and "Construction and shutdown") and AttachWorld's steps 2, 3, 5, 6, 7 and 8 (0x53570010 to 0x53570015) each fail alone under fault injection with the stated error, unwinding every earlier step and leaving no registration, allocation or retiring pool; the retiring-pool list is reserved at Start for `uSTREAMING_MAX_RETIRING_POOLS` and DetachWorld allocates nothing.
- **Covers:** Z2-I-STREAM-01, Z2-I-STREAM-02, Z2-I-STREAM-03, Z2-I-STREAM-04, Z2-I-STREAM-05, Z2-I-STREAM-06, Z2-I-STREAM-07, Z2-I-STREAM-08, Z2-I-STREAM-09, Z2-I-STREAM-10, Z2-I-STREAM-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045, Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingModule_StartFaultPoints`, `Test_StreamingModule_AttachFaultPoints`, `Test_StreamingModule_RetiringListFullChecks` (`Test`, `Sanitizers -Preset win-asan`).
- **Design:** Zenith2/Docs/Design/STREAM/PublicAPI.md → "Zenith_Module_Streaming header"; Zenith2/Docs/Design/STREAM/Tests.md → "Death and fault-injection tests"; Zenith2/Docs/Design/STREAM.md → "Construction and shutdown"

### Z2-R-STREAM-006 Attach and detach per world

- **Statement:** AttachWorld adds the world state resource, the three nodes, the listener, the 112-slot snapshot pool and the hook; DetachWorld reverses them without allocating, retires a pool with held slots and destroys it once every slot is FREE; 100 attach/detach cycles leave nothing behind.
- **Covers:** Z2-I-STREAM-01, Z2-I-STREAM-02, Z2-I-STREAM-03, Z2-I-STREAM-04, Z2-I-STREAM-05, Z2-I-STREAM-06, Z2-I-STREAM-07, Z2-I-STREAM-08, Z2-I-STREAM-09, Z2-I-STREAM-10, Z2-I-STREAM-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingModule_AttachDetachRepeat`, `Test_StreamingSnapshotPool_RetiredUntilFree`, `Test_StreamingSnapshotPool_DestroyWithHeldSlotChecks` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/PublicAPI.md → "Zenith_Module_Streaming header"; Zenith2/Docs/Design/STREAM.md → "Construction and shutdown"; Zenith2/Docs/Design/STREAM/PublicAPI.md → "Zenith_StreamingSnapshotPool"

### Z2-R-STREAM-007 Components registered with permanent IDs

- **Statement:** The seven components (`Zenith.Streaming.WorldRef`, `.Source`, `.RegionBound`, `.Persistent`, `.CellAnchor`, `.CellDelta`, `.CellStatus`), the struct `Zenith.Streaming.CellOverrideSlot` and the resource `Zenith.Streaming.State` register with the stable names, property IDs, types, defaults, ranges, authorities and serialized and editor-hidden flags of the component table, every STREAM-written property serialized, and `Freeze` accepts them.
- **Covers:** Z2-I-STREAM-01, Z2-I-STREAM-02, Z2-I-STREAM-03, Z2-I-STREAM-04, Z2-I-STREAM-05, Z2-I-STREAM-06, Z2-I-STREAM-07, Z2-I-STREAM-08, Z2-I-STREAM-09, Z2-I-STREAM-10, Z2-I-STREAM-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-002, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingComponents_RegisteredShape` (`Test`); GAME's `Test_Gameplay_AuthorityCensusMatchesRegistry` with STREAM's census rows added in the registering diff.
- **Design:** Zenith2/Docs/Design/STREAM/PublicAPI.md → "Zenith_StreamingComponents header"; Zenith2/Docs/Design/STREAM.md → "Component types"

### Z2-R-STREAM-008 Configuration keys and settings

- **Statement:** Every key of the table "Configuration keys" is registered with its type, default, range, layers, restart flag and simulation-affecting flag; `Zenith_ReadStreamingSettings` refuses, with `STREAMING_ERROR_CONFIG_INVALID` naming the key, an unknown or non-component persistent type, the derived transform types `Zenith.WorldTransform` and `Zenith.PreviousWorldTransform`, and a rebase quantum above twice the threshold.
- **Covers:** Z2-I-STREAM-01, Z2-I-STREAM-02, Z2-I-STREAM-03, Z2-I-STREAM-04, Z2-I-STREAM-05, Z2-I-STREAM-06, Z2-I-STREAM-07, Z2-I-STREAM-08, Z2-I-STREAM-09, Z2-I-STREAM-10, Z2-I-STREAM-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-STREAM-003
- **Acceptance evidence:** `Test_StreamingModule_SettingsRangesAndTypes`, `Test_StreamingSettings_RefusesDerivedTransforms` (`Test`); simulation-affecting keys appear in the replay header (`Test_StreamingDeterminism_ReplayAcrossWorkers`).
- **Design:** Zenith2/Docs/Design/STREAM/PublicAPI.md → "Configuration keys"; Zenith2/Docs/Design/STREAM/PublicAPI.md → "Zenith_StreamingSettings"; Zenith2/Docs/Design/STREAM/Runtime.md → "Eligible property kinds"

### Z2-R-STREAM-009 Error record and codes

- **Statement:** `Zenith_StreamingError` is 16 bytes with the fields of "Zenith_StreamingErrors header", codes are numbered from 1 in the order listed and never change meaning, and every code has a distinct `Zenith_ToString`.
- **Covers:** Z2-I-STREAM-01, Z2-I-STREAM-02, Z2-I-STREAM-03, Z2-I-STREAM-04, Z2-I-STREAM-05, Z2-I-STREAM-06, Z2-I-STREAM-07, Z2-I-STREAM-08, Z2-I-STREAM-09, Z2-I-STREAM-10, Z2-I-STREAM-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045, Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingTypes_RecordLayouts`, `Test_StreamingTypes_ToStringCoversEveryEnumerator` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/PublicAPI.md → "Zenith_StreamingErrors header"; Zenith2/Docs/Design/STREAM.md → "Error enumerations"

### Z2-R-STREAM-010 Up to eight runtime grids

- **Statement:** A streaming world has 1 to 8 uniform grids, each with a unique name, a role, a distance metric, a cell size of 32 to 8,192 m in steps of 32, an origin, a vertical extent and its own radii.
- **Covers:** Z2-I-STREAM-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingWorldFormat_ReaderCheckS5`, `Test_StreamingWorldFormat_ReaderCheckS6`, `Test_StreamingCook_MessageV3` to `Test_StreamingCook_MessageV7` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Formats.md → "Grid records"; Zenith2/Docs/Design/STREAM/Formats.md → "Grid object"; Zenith2/Docs/Design/STREAM/PublicAPI.md → "Zenith_StreamingTypes header"

### Z2-R-STREAM-011 Cell key space

- **Statement:** A cell is named by its global index (sorted by grid, z, x) and its coordinate `(grid, x, z)` with |x|, |z| ≤ 2^20; `FindCell` finds any coordinate and `FindCellByScene` any cell scene GUID in O(1) expected time, and `FindCellContaining` maps an absolute point to its cell.
- **Covers:** Z2-I-STREAM-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingWorldFormat_FindCellHashCoversRange` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM.md → "Cell key and coordinates"; Zenith2/Docs/Design/STREAM/PublicAPI.md → "Zenith_StreamingWorldAsset header"; Zenith2/Docs/Design/STREAM/Formats.md → "Reader checks"

### Z2-R-STREAM-012 Cell absolute bounds and anchors

- **Statement:** A cell's absolute bounds are `origin + coord × size` to `origin + (coord + 1) × size` in binary64, its vertical extent the grid's, and its anchor the minimum corner at the grid's base height.
- **Covers:** Z2-I-STREAM-01, Z2-I-STREAM-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingBands_DistanceToCellXZ`, `Test_StreamingRebase_RootTranslationExact` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM.md → "Cell key and coordinates"; Zenith2/Docs/Design/STREAM/Formats.md → "Cell records"; Zenith2/Docs/Design/STREAM/Runtime.md → "Large coordinates and origin rebasing"

### Z2-R-STREAM-013 Grid roles order loading

- **Statement:** Grid roles COLLISION, NAVIGATION, GAMEPLAY, AUDIO and LARGE are values 1 to 5, and at equal band and distance cells load and activate in role order.
- **Covers:** Z2-I-STREAM-01, Z2-I-STREAM-03, Z2-I-STREAM-09, Z2-I-STREAM-10, Z2-I-STREAM-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingAdmission_RoleOrderCollisionFirst` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/PublicAPI.md → "Zenith_StreamingTypes header"; Zenith2/Docs/Design/STREAM/Runtime.md → "Load ordering and admission"

### Z2-R-STREAM-014 Grid-derived dependencies

- **Statement:** The cook derives, for every grid h in a cell's grid dependency mask, a dependency on every cell of h whose rectangle intersects the cell's rectangle grown by h's overlap margin, merged with the document's explicit dependencies, at most 32 per cell, acyclic.
- **Covers:** Z2-I-STREAM-01, Z2-I-STREAM-09, Z2-I-STREAM-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingCook_MessageV11`, `Test_StreamingCook_MessageV19`, `Test_StreamingWorldFormat_ReaderCheckS13`, `Test_StreamingWorldFormat_ReaderCheckS14` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Formats.md → "Dependency section"; Zenith2/Docs/Design/STREAM/Formats.md → "Cook step algorithm"

### Z2-R-STREAM-015 Dependencies activate first

- **Statement:** No cell is admitted for activation while any of its dependencies is not ACTIVE, and no ACTIVE cell is unloaded while any of its dependents is not UNLOADED.
- **Covers:** Z2-I-STREAM-01, Z2-I-STREAM-09, Z2-I-STREAM-10, Z2-I-STREAM-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingAdmission_WaitsForDependenciesActive`, `Test_StreamingAdmission_BlockedByPermanentDependency`, `Test_StreamingDesired_DependencyOutlivesDependents`, `Test_StreamingIntegration_CollisionBeforeGameplay` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Desired-set algorithm"; Zenith2/Docs/Design/STREAM/Runtime.md → "Load ordering and admission"

### Z2-R-STREAM-016 Worst-case active-cell bound

- **Statement:** The cook computes per grid, in dependency order, `reach(h) = max(r_h, max over dependents g of reach(g) + c_g + m_h·c_h)` and `B'(h) = min(cellCount(h), (2·ceil(reach(h)/c_h) + 1)²)`, so cells pinned through a chain of dependencies are bounded, records B'(h) in the grid record and the sum in the world record, and refuses a world whose sum exceeds 192.
- **Covers:** Z2-I-STREAM-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingCook_WorstCaseBound` (including a dependent grid whose reach exceeds its dependency's and a three-grid chain), `Test_StreamingCook_MessageV23` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Formats.md → "Worst-case active cells"; Zenith2/Docs/Design/STREAM/Formats.md → "World record"

### Z2-R-STREAM-017 Entity bounds within the overlap margin

- **Statement:** Every entity of a cell scene lies inside the cell rectangle grown by the overlap margin and inside the grid's vertical extent, else the cook fails with `COOK_ERROR_STREAM_CELL_BOUNDS` naming the entity and the excess distance.
- **Covers:** Z2-I-STREAM-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingCook_MessageV15` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Formats.md → "Entity bounds and the overlap margin"; Zenith2/Docs/Design/STREAM/Formats.md → "Validation messages"

### Z2-R-STREAM-018 Streaming world document schema

- **Statement:** `*.streamworld.json` of type `Zenith.Streaming.WorldDocument`, schema version 1, has exactly the root, grid, cell, HLOD and reference members of the schema tables with their defaults, is written canonically and preserves unknown members.
- **Covers:** Z2-I-STREAM-01, Z2-I-STREAM-06, Z2-I-STREAM-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingCook_ValidDocumentCooks`, `Test_StreamingCook_MessageV1`, `Test_StreamingCook_MessageV2` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Formats.md → "Root members"; Zenith2/Docs/Design/STREAM/Formats.md → "Grid object"; Zenith2/Docs/Design/STREAM/Formats.md → "Cell object"; Zenith2/Docs/Design/STREAM/Formats.md → "HLOD object"; Zenith2/Docs/Design/STREAM/Formats.md → "Reference object"

### Z2-R-STREAM-019 Document type registration and migration

- **Statement:** `Zenith_RegisterStreamingDocumentTypes` registers the document type with an empty version-1 chain from ZenithCook and ZenithEditor, and every subsequent schema change adds one step with a JSON fixture pair.
- **Covers:** Z2-I-STREAM-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingCook_Migration_V1Identity` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Formats.md → "Migration chain"; Zenith2/Docs/Design/STREAM.md → "Migration hooks"

### Z2-R-STREAM-020 Cook step stream.world

- **Statement:** The cook step `stream.world` version 1 runs the nine steps of "Cook step algorithm", keys on the document digest, the cell scenes' and HLOD meshes' output hashes and the format version, declares cell scenes and meshes as soft dependencies and writes the report member `streaming`.
- **Covers:** Z2-I-STREAM-01, Z2-I-STREAM-06, Z2-I-STREAM-07, Z2-I-STREAM-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingCook_ValidDocumentCooks`, `Test_StreamingCook_DeterministicOnRepeat` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Formats.md → "Cook step algorithm"; Zenith2/Docs/Design/STREAM.md → "Cook step"

### Z2-R-STREAM-021 Every validation message

- **Statement:** Each of V1 to V24 is produced by its condition with its cook error and message text naming the document and a JSON pointer, every message is reported before the step fails, in V order then document order; V16's entity limit is the smaller of 16,384 and the configured `ecs.maxSpawnsPerTick`.
- **Covers:** Z2-I-STREAM-01, Z2-I-STREAM-06, Z2-I-STREAM-07, Z2-I-STREAM-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** The 24 tests `Test_StreamingCook_MessageV1` to `Test_StreamingCook_MessageV24` enumerated in Tests.md "Validation message tests", each over its committed document `Documents/V<n>.streamworld.json`, and `Test_StreamingCook_CellEntityLimitFollowsSpawnCap` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Formats.md → "Validation messages"; Zenith2/Docs/Design/STREAM/Tests.md → "Validation message tests"

### Z2-R-STREAM-022 Derived cell facts

- **Statement:** The anchor entity, entity count, persistent entries and estimated bytes of each cell are derived by the cook from the cooked cell scene and the catalogue, never authored in the document.
- **Covers:** Z2-I-STREAM-01, Z2-I-STREAM-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingCook_DerivesAnchorAndPersistent` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Formats.md → "Cell object"; Zenith2/Docs/Design/STREAM/Formats.md → "Cook step algorithm"

### Z2-R-STREAM-023 Cell scene anchor rule

- **Statement:** A cell scene has exactly one root, carrying `Zenith.Streaming.CellAnchor` with its own cell scene's persistent ID and the world ID and an identity local transform, and every other entity is its descendant.
- **Covers:** Z2-I-STREAM-01, Z2-I-STREAM-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingCook_MessageV12`, `Test_StreamingCook_MessageV13`, `Test_StreamingCook_MessageV14` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Formats.md → "Anchor root"; Zenith2/Docs/Design/STREAM/Formats.md → "Cell scene checks"

### Z2-R-STREAM-024 Cook determinism

- **Statement:** Two cooks of one document produce byte-identical `.zstream` files and DDC keys, and the encoder output is independent of the order of the descriptor's spans.
- **Covers:** Z2-I-STREAM-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingCook_DeterministicOnRepeat`, `Test_StreamingWorldFormat_ByteIdenticalAnyInputOrder` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Formats.md → "Common rules"; Zenith2/Docs/Design/STREAM/PublicAPI.md → "Zenith_StreamingWorldWriter header"

### Z2-R-STREAM-025 .zstream container use

- **Statement:** `.zstream` uses ASSET's 64-byte header with magic `ZSTW`, format version 1, kind `ASSET_KIND_STREAMING_WORLD` and exactly eight sections of kinds 1 to 8, each index 0, zstd level 3 above 4 KiB.
- **Covers:** Z2-I-STREAM-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingWorldFormat_RoundTrip`, `Test_StreamingWorldFormat_ReaderCheckS1` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Formats.md → "Header use"; Zenith2/Docs/Design/STREAM/Formats.md → "Sections"

### Z2-R-STREAM-026 Record layouts of the index

- **Statement:** The world (64 B), grid (144 B), cell (96 B), dependency (4 B), persistent (24 B), reference (64 B), outgoing (4 B) and HLOD node (96 B) records have exactly the fields, offsets and rules of their tables.
- **Covers:** Z2-I-STREAM-01, Z2-I-STREAM-06, Z2-I-STREAM-07, Z2-I-STREAM-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingWorldFormat_RoundTrip` and the 20 reader-check tests of Tests.md "Reader check tests" (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Formats.md → "World record"; Zenith2/Docs/Design/STREAM/Formats.md → "Grid records"; Zenith2/Docs/Design/STREAM/Formats.md → "Cell records"; Zenith2/Docs/Design/STREAM/Formats.md → "Persistent section"; Zenith2/Docs/Design/STREAM/Formats.md → "Reference sections"; Zenith2/Docs/Design/STREAM/Formats.md → "HLOD node records"

### Z2-R-STREAM-027 Reader checks S1 to S20

- **Statement:** The kind loader applies S1 to S20 in order after ASSET's container checks and fails with `STREAMING_ERROR_WORLD_CORRUPT` (or `_WORLD_LIMIT`) and the check number, never asserting on file data.
- **Covers:** Z2-I-STREAM-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** The 20 tests `Test_StreamingWorldFormat_ReaderCheckS1` to `Test_StreamingWorldFormat_ReaderCheckS20` enumerated in Tests.md "Reader check tests" (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Formats.md → "Reader checks"; Zenith2/Docs/Design/STREAM/Tests.md → "Reader check tests"

### Z2-R-STREAM-028 Parser limits at and above

- **Statement:** Every limit of the Formats "Limits" table loads at the limit and fails at limit + 1, in the reader and in the cook.
- **Covers:** Z2-I-STREAM-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingWorldFormat_LimitsAtAndAbove` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Formats.md → "Limits"; Zenith2/Docs/Design/STREAM/PublicAPI.md → "Zenith_StreamingTypes header"

### Z2-R-STREAM-029 Reader fuzz target

- **Statement:** `Zenith_Fuzz_StreamingWorld` feeds arbitrary section bytes and delta bytes to the reader and the delta decoder with no crash, sanitizer report or allocation beyond the limits, from a corpus generated at test time.
- **Covers:** Z2-I-STREAM-01, Z2-I-STREAM-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** 60-second smoke of `Zenith_Fuzz_StreamingWorld` under `Sanitizers -Preset win-asan`.
- **Design:** Zenith2/Docs/Design/STREAM/Tests.md → "Fuzz target"; Zenith2/Docs/Design/STREAM.md → "Integration suite and fuzz target"

### Z2-R-STREAM-030 World kind loader

- **Statement:** `Zenith_StreamingWorldKindLoader` requests the eight sections in one round, builds the asset in one allocation through steps L1 to L7, reports its resident bytes under `MEMORY_SUBSYSTEM_STREAMING`, and on `Cancel` frees a partial asset and acknowledges.
- **Covers:** Z2-I-STREAM-01, Z2-I-STREAM-02, Z2-I-STREAM-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-040, Z2-D-045
- **Budgets:** Z2-B-STREAM-001
- **Acceptance evidence:** `Test_StreamingWorldFormat_LoaderCancelFreesPartial`, `Test_StreamingWorldFormat_LoaderOutOfMemory` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/PublicAPI.md → "Zenith_StreamingWorldKindLoader"; Zenith2/Docs/Design/STREAM/Formats.md → "Reader checks"

### Z2-R-STREAM-031 Asset kind row

- **Statement:** The kind `Streaming.World` is registered with the code `uASSET_KIND_DEFINED_COUNT` gives it (35 after ENV's 34), script name `StreamingWorld`, runtime type `Zenith_StreamingWorldAsset`, memory subsystem STREAMING, fallback FAIL, not cacheable, not GPU-resident.
- **Covers:** Z2-I-STREAM-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** ASSET's kind-table test over the new row once row ST6 is applied; `Test_StreamingModule_StartRegistersEverything` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/PublicAPI.md → "ASSET kind row"; Zenith2/Docs/Design/STREAM.md → "Reconciliations with other documents"

### Z2-R-STREAM-032 World binding by component

- **Statement:** A world is streamed when exactly one enabled `Zenith.Streaming.WorldRef` on scene content resolves; further references are refused with `STREAMING_ERROR_WORLD_DUPLICATE`, runtime-created ones with `STREAMING_ERROR_WORLDREF_RUNTIME`, each logged once; the binding moves through NONE, BOUND, REFUSED, FAILED and UNBINDING (value 4 of `m_eBinding`, "unbinding" in the dump).
- **Covers:** Z2-I-STREAM-01, Z2-I-STREAM-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingBinding_RefusalsAndRebinding`, `Test_StreamingCellMachine_RowT22` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "World binding"

### Z2-R-STREAM-033 Binding reads no asynchronous state

- **Statement:** Binding resolves the world asset only through the hard-dependency guarantee of scene content; a resolve failure is a defect reported as `STREAMING_ERROR_WORLD_NOT_RESIDENT`, and no other asset state is read by simulation.
- **Covers:** Z2-I-STREAM-01, Z2-I-STREAM-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingDeterminism_IODelayDoesNotChangeTicks` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "World binding"; Zenith2/Docs/Design/STREAM/Runtime.md → "Replay and determinism"

### Z2-R-STREAM-034 Unbinding unloads paced

- **Statement:** Disabling or changing the world reference unloads every cell through the ordinary paced unloads with their captures, then releases the world handle; rebinding the same world resets FAILED_PERMANENT cells.
- **Covers:** Z2-I-STREAM-01, Z2-I-STREAM-02, Z2-I-STREAM-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingCellMachine_RowT23`, `Test_StreamingBinding_RefusalsAndRebinding`, `Test_StreamingCellMachine_RetriesExhaustToPermanent` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "World binding"; Zenith2/Docs/Design/STREAM/Runtime.md → "Cell state machine"

### Z2-R-STREAM-035 Binding allocation failure

- **Statement:** A failure to size the per-cell arrays at binding leaves the binding FAILED with `STREAMING_ERROR_OUT_OF_MEMORY` and no partial arrays.
- **Covers:** Z2-I-STREAM-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045, Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingBinding_ArraysOutOfMemory` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "World binding"; Zenith2/Docs/Design/STREAM.md → "Allocation failure"

### Z2-R-STREAM-036 Update node declaration

- **Statement:** `Streaming.Update` is a phase-1 system, `JOB_AFFINITY_ANY`, `Before("Scene.ActivateAdditive")`, `EmitsCommands`, with exactly the reads and writes of "Update node declaration"; it is the only system that writes `Zenith.Streaming.State`, which STREAM's playbacks, listener and hook also write on the sync-point node and the tick-boundary handler, and which the service only reads.
- **Covers:** Z2-I-STREAM-01, Z2-I-STREAM-02, Z2-I-STREAM-03, Z2-I-STREAM-04, Z2-I-STREAM-05, Z2-I-STREAM-06, Z2-I-STREAM-07, Z2-I-STREAM-08, Z2-I-STREAM-09, Z2-I-STREAM-10, Z2-I-STREAM-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080, Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingModule_ScheduleFreezes`, `Test_StreamingService_CountersDifferencedPerFrame`, `Test_StreamingBindings_ReadWorld` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Update node declaration"; Zenith2/Docs/Design/STREAM.md → "Frame-graph nodes and access"; Zenith2/Docs/Design/STREAM.md → "Thread affinity per type"

### Z2-R-STREAM-037 Fifteen update steps in order

- **Statement:** Each tick, `Streaming.Update` runs steps 1 to 15 of "Steps" in order, with the profiler sub-zones named there.
- **Covers:** Z2-I-STREAM-01, Z2-I-STREAM-02, Z2-I-STREAM-03, Z2-I-STREAM-04, Z2-I-STREAM-05, Z2-I-STREAM-06, Z2-I-STREAM-07, Z2-I-STREAM-08, Z2-I-STREAM-09, Z2-I-STREAM-10, Z2-I-STREAM-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** Z2-B-STREAM-002
- **Acceptance evidence:** `Test_StreamingIntegration_TraversalLoadsAndUnloads`, `Test_StreamingDiagnostics_ZoneNames` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Steps"; Zenith2/Docs/Design/STREAM.md → "Profiler zones"

### Z2-R-STREAM-038 Distance to a cell

- **Statement:** The distance from a source to a cell is computed in binary64 from the source's absolute position to the cell's box, horizontally for XZ grids and with the vertical term for XYZ grids.
- **Covers:** Z2-I-STREAM-01, Z2-I-STREAM-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingBands_DistanceToCellXZ`, `Test_StreamingBands_DistanceToCellXYZ` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Distance and band computation"

### Z2-R-STREAM-039 Five bands with inclusive thresholds

- **Statement:** A cell's band is REQUIRED, ACTIVATION, KEEP, PREFETCH or NONE by the thresholds `Rreq·s`, `Ract·s`, `(Ract + H)·s`, `Rpre·s`, each inclusive, with s clamped to [0.25, 4], and the most urgent band of any source wins.
- **Covers:** Z2-I-STREAM-01, Z2-I-STREAM-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingBands_BandThresholdsInclusive`, `Test_StreamingBands_RadiusScaleClamped`, `Test_StreamingBands_MostUrgentSourceWins` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Distance and band computation"

### Z2-R-STREAM-040 Forward bias reorders only

- **Statement:** The forward bias `d_eff = d × (1 − β·max(0, cos θ))` with β = `stream.priority.forwardBias` changes the ordering distance only, never a band.
- **Covers:** Z2-I-STREAM-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingBands_ForwardBiasOrdersAhead` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Distance and band computation"

### Z2-R-STREAM-041 Bounded enumeration

- **Statement:** Per (source, grid) at most 4,096 coordinates are examined; a larger prefetch square is clipped, never below the KEEP square, and logged once per world and grid.
- **Covers:** Z2-I-STREAM-01, Z2-I-STREAM-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** Z2-B-STREAM-002
- **Acceptance evidence:** `Test_StreamingBands_EnumerationClipAt4096` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Distance and band computation"; Zenith2/Docs/Design/STREAM/Runtime.md → "Complexity"

### Z2-R-STREAM-042 Sources capped at sixteen

- **Statement:** At most 16 enabled sources per world are used, the 16 lowest simulation IDs; more are ignored and logged once per world.
- **Covers:** Z2-I-STREAM-02, Z2-I-STREAM-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingBands_SourcesCappedAtSixteen` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Steps"; Zenith2/Docs/Design/STREAM.md → "Failure table"

### Z2-R-STREAM-043 KEEP hysteresis

- **Statement:** A cell in the KEEP band is retained when loading or active and is never newly issued.
- **Covers:** Z2-I-STREAM-02, Z2-I-STREAM-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingDesired_KeepRetainsNeverStarts` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Desired-set algorithm"

### Z2-R-STREAM-044 Dependency closure in one pass

- **Statement:** Wanted cells pin their dependencies at their own urgent band (KEEP pins at KEEP) in one pass over the grids in dependency order.
- **Covers:** Z2-I-STREAM-01, Z2-I-STREAM-09, Z2-I-STREAM-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingDesired_ClosurePinsDependencies` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Desired-set algorithm"

### Z2-R-STREAM-045 Unload delay

- **Statement:** An ACTIVE cell that becomes unwanted is unloaded only after its grid's `unloadDelayTicks` consecutive unwanted ticks.
- **Covers:** Z2-I-STREAM-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingDesired_UnloadDelayCountsDown` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Desired-set algorithm"

### Z2-R-STREAM-046 Cell overrides

- **Statement:** `Streaming.SetCellOverride` writes the bound world reference's override slots (property 3: 64 slots, replaced per coordinate, freed by NONE), from which every update reads the overrides, forcing a cell REQUIRED or NONE when overrides are allowed, FORCE_UNLOADED also forcing every transitive dependent to NONE; an override is refused with `STREAMING_ERROR_OVERRIDE_NOT_ALLOWED` in Shipping, with `stream.debug.allowOverrides` false, while unbound or with every slot full, and an override of a coordinate with no cell is counted and ignored.
- **Covers:** Z2-I-STREAM-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingBands_OverridesApplyOnlyWhenAllowed`, `Test_StreamingBands_ForceUnloadedTakesDependents`, `Test_StreamingBands_OverridesPersistOnWorldReference`, `Test_StreamingBindings_OverrideToolsOnly` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/PublicAPI.md → "Zenith_StreamingCommands header"; Zenith2/Docs/Design/STREAM/Runtime.md → "Distance and band computation"; Zenith2/Docs/Design/STREAM/Runtime.md → "Overrides on the world reference"

### Z2-R-STREAM-047 Total ordering key

- **Statement:** Loads, unloads and admissions are ordered by the 64-bit key (band, 3 − source priority, role, quantised effective distance, grid, cell index), a total order identical on every machine, sorted with `Zenith_Sort`.
- **Covers:** Z2-I-STREAM-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingAdmission_KeyIsTotalOrder` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Ordering key"

### Z2-R-STREAM-048 Concurrent loads capped

- **Statement:** Issuing never makes cells in REQUESTED, LOADING and CANCELLING exceed `stream.maxConcurrentCellLoads` (default 16, at most 48), nor the pending count (those, plus QUEUED cells, plus UNLOADING cells whose recorded unload was of a QUEUED instance) exceed `stream.maxConcurrentCellLoads` + `stream.maxQueuedCells` (default 32, at most 64), so LOADING + CANCELLING + QUEUED ≤ that sum at every phase 11, rejected unloads included; after a restore up to `uSTREAMING_SNAPSHOT_SLOTS` adopted in-flight loads may exceed both caps, and no load is issued until the counts fall below them; wanted cells beyond a cap are counted in `stream.cells.loads_deferred`.
- **Covers:** Z2-I-STREAM-02, Z2-I-STREAM-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** Z2-B-STREAM-003
- **Acceptance evidence:** `Test_StreamingAdmission_ConcurrentCap`, `Test_StreamingAdmission_PendingCapBoundsQueued` (including its rejected-unload case), `Test_StreamingAdmission_AdoptedLoadsAboveCapsIssueNothing` (`Test`); `stream.cells.in_flight` in MP-COUNTER.
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Caps"; Zenith2/Docs/Design/STREAM/Runtime.md → "Loads (step 9)"; Zenith2/Docs/Design/STREAM.md → "Counters"

### Z2-R-STREAM-049 Load actions per tick capped

- **Statement:** At most `stream.maxNewRequestsPerTick` load actions are recorded per tick.
- **Covers:** Z2-I-STREAM-02, Z2-I-STREAM-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingAdmission_NewRequestsPerTickCap` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Caps"; Zenith2/Docs/Design/STREAM/Runtime.md → "Loads (step 9)"

### Z2-R-STREAM-050 Live instances capped with preemption

- **Statement:** Live cells never exceed `stream.maxLiveInstances` (at most 192 of SCENE's 256); a REQUIRED candidate blocked by the cap cuts the delay of the least urgent unwanted ACTIVE cell or, when there is none, preempts the least urgent KEEP-only ACTIVE cell; binding a world whose cooked bound exceeds the cap logs and counts it.
- **Covers:** Z2-I-STREAM-02, Z2-I-STREAM-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingAdmission_LiveCapAndPreemption`, `Test_StreamingBinding_BoundAboveCapWarns`, `Test_StreamingIntegration_LiveInstanceCapWithGameAdditives` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Caps"; Zenith2/Docs/Design/STREAM/Runtime.md → "Loads (step 9)"; Zenith2/Docs/Design/STREAM/Runtime.md → "World binding"

### Z2-R-STREAM-051 Activation paced by cells and entities

- **Statement:** At most `stream.activation.maxCellsPerTick` cells and `stream.activation.maxEntitiesPerTick` entities are admitted per tick, except that the first admission of a tick is never refused for its entity count.
- **Covers:** Z2-I-STREAM-02, Z2-I-STREAM-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** Z2-B-SHIP-015
- **Acceptance evidence:** `Test_StreamingAdmission_ActivationCellsCap`, `Test_StreamingAdmission_ActivationEntitiesCap` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Admission (step 10)"; Zenith2/Docs/Design/STREAM.md → "Pending row needs"; Zenith2/Docs/Design/STREAM/Formats.md → "Validation messages"

### Z2-R-STREAM-052 Unloads paced farthest first

- **Statement:** At most `stream.unload.maxCellsPerTick` unloads and cancellations are issued per tick, least urgent first.
- **Covers:** Z2-I-STREAM-02, Z2-I-STREAM-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingAdmission_UnloadsFarthestFirst` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Unloads (step 8)"

### Z2-R-STREAM-053 Boosters raise loading priority

- **Statement:** The service holds one HIGH-priority load request (CRITICAL within `stream.teleport.criticalTicks` of a teleport, for required cells) for each REQUESTED, LOADING or CANCELLING cell's scene and destroys it in the frame the cell leaves those states.
- **Covers:** Z2-I-STREAM-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-ASSET-004, Z2-B-STREAM-004
- **Acceptance evidence:** `Test_StreamingService_BoosterRaisesPriority` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Sub-steps, in order"; Zenith2/Docs/Design/STREAM/Runtime.md → "Teleports"

### Z2-R-STREAM-054 Prefetch at background priority

- **Statement:** The service prefetches unloaded PREFETCH-band cells and its lookahead disc at `ASSET_PRIORITY_BACKGROUND`, at most 8 new per frame, and cancels each when the cell becomes wanted or leaves the band.
- **Covers:** Z2-I-STREAM-02, Z2-I-STREAM-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-ASSET-002, Z2-B-STREAM-001
- **Acceptance evidence:** `Test_StreamingService_PrefetchCappedAndCancelled` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Sub-steps, in order"; Zenith2/Docs/Design/STREAM/Runtime.md → "Prefetch disc"; Zenith2/Docs/Design/STREAM/Runtime.md → "Request state machine"

### Z2-R-STREAM-055 Cell state set

- **Statement:** A cell is in exactly one of the ten states of "States" with the holdings listed, and every (state, observed instance state) pair resolves to exactly one row of the transition table.
- **Covers:** Z2-I-STREAM-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingCellMachine_TableIsTotal` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "States"; Zenith2/Docs/Design/STREAM/PublicAPI.md → "Zenith_StreamingTypes header"; Zenith2/Docs/Design/STREAM/Runtime.md → "Totality"

### Z2-R-STREAM-056 Every transition row

- **Statement:** Each transition T1 to T34 happens exactly on its event with its actions, counters and slot-bit changes.
- **Covers:** Z2-I-STREAM-02, Z2-I-STREAM-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** The 34 tests `Test_StreamingCellMachine_RowT1` to `Test_StreamingCellMachine_RowT34` enumerated in Tests.md "Cell state machine" (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Transitions"

### Z2-R-STREAM-057 Capacity failures are not content failures

- **Statement:** A failure of class capacity (`SCENE_ERROR_LIMIT` at commit or start, `OUT_OF_MEMORY`, `MEMORY_REFUSED`, `ACTIVATION_CAPACITY`, an unrequested `CANCELLED`, or `ASSET_UNAVAILABLE` whose ASSET detail is `OUT_OF_MEMORY`, `QUEUE_FULL`, `REGISTRY_FULL`, `SHUTTING_DOWN` or `TIMEOUT`) makes the cell FAILED without consuming a retry, re-issued after a backoff of `stream.cell.capacityRetryTicks` doubling per consecutive capacity failure up to `stream.cell.retryTicks` in saturating `uint64` arithmetic, so capacity and memory pressure, at SCENE's or ASSET's level, never make content permanently absent.
- **Covers:** Z2-I-STREAM-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingCellMachine_CommitRefusedDoesNotConsumeRetry`, `Test_StreamingCellMachine_CapacityFailureDoesNotConsumeRetry`, `Test_StreamingCellMachine_AssetCapacityFailureDoesNotConsumeRetry`, `Test_StreamingIntegration_LiveInstanceCapWithGameAdditives` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Transitions"; Zenith2/Docs/Design/STREAM.md → "Failure table"; Zenith2/Docs/Design/STREAM/Runtime.md → "Failure classification"

### Z2-R-STREAM-058 Retries and permanent failure

- **Statement:** A content failure (every SCENE code other than the capacity codes, `ASSET_UNAVAILABLE` and `ACTIVATION_TOO_LARGE`, including `DUPLICATE_INSTANCE`; `ASSET_UNAVAILABLE` with a content ASSET detail; a transient failure beyond `stream.cell.maxTransientRetries`) retries after `stream.cell.retryTicks` up to `stream.cell.maxRetries`, then the cell is FAILED_PERMANENT until a rebinding; `ACTIVATION_TOO_LARGE` and a catalogue miss are FAILED_PERMANENT at once.
- **Covers:** Z2-I-STREAM-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingCellMachine_RetriesExhaustToPermanent`, `Test_StreamingCellMachine_ActivationTooLargeIsPermanent`, `Test_StreamingCellMachine_CatalogueMissIsPermanent`, `Test_StreamingCellMachine_AssetTransientFailureIsBounded` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Transitions"; Zenith2/Docs/Design/STREAM.md → "Failure table"; Zenith2/Docs/Design/STREAM/Runtime.md → "Failure classification"

### Z2-R-STREAM-059 Failed instances released

- **Statement:** A cell whose instance becomes FAILED records the unload that releases SCENE's record in the same tick and keeps "release pending" until it observes the record gone, re-recording a rejected release; it retries only afterwards.
- **Covers:** Z2-I-STREAM-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingCellMachine_RowT6`, `Test_StreamingCellMachine_RowT32`, `Test_StreamingCellMachine_RowT33` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Transitions"; Zenith2/Docs/Design/STREAM/Runtime.md → "Action buffers and rejection"

### Z2-R-STREAM-060 One buffer per action

- **Statement:** Every action is its own command buffer with merge key `(0, ordinal)`, ordinals assigned in the order rebuild releases, unloads, loads, holder repairs and status-holder actions, admissions, region destroys, rebase, step 14's re-homes, step 14's holder prunes.
- **Covers:** Z2-I-STREAM-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** Z2-B-ECS-004
- **Acceptance evidence:** `Test_StreamingActions_OneBufferPerAction` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Action buffers and rejection"

### Z2-R-STREAM-061 Rejected actions detected and reverted

- **Statement:** A rejected load, unload, cancellation, release, delta merge or state application is detected at the next update and its transition reverted or re-recorded (T4, T12, T13, T17, T19, T29 to T32), with no capture lost.
- **Covers:** Z2-I-STREAM-02, Z2-I-STREAM-04, Z2-I-STREAM-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** Z2-B-ECS-004
- **Acceptance evidence:** `Test_StreamingActions_RejectedLoadReverts`, `Test_StreamingActions_RejectedUnloadKeepsCapture`, `Test_StreamingActions_RejectedActivationLateApply`, `Test_StreamingActions_ApplyMeetsQueuedInstance` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Action buffers and rejection"; Zenith2/Docs/Design/STREAM/Runtime.md → "Transitions"

### Z2-R-STREAM-062 Activation is never interrupted

- **Statement:** A cell that stops being wanted while ACTIVATING completes its activation and is unloaded from ACTIVE at a subsequent tick.
- **Covers:** Z2-I-STREAM-02, Z2-I-STREAM-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingCellMachine_ActivatingNotInterruptible` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Transitions"; Zenith2/Docs/Design/STREAM/Runtime.md → "Cancellation at every stage"

### Z2-R-STREAM-063 Admission through the activation hook

- **Statement:** SCENE activates a STREAM-tagged queued instance only when `AdmitActivation` admits it, and a refusal does not count against `scene.activationRetryTicks`.
- **Covers:** Z2-I-STREAM-02, Z2-I-STREAM-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingAdmission_RefusedAdmissionNotCountedBySCENE` (`Test`), once row ST2 lands.
- **Design:** Zenith2/Docs/Design/STREAM/PublicAPI.md → "SCENE activation hook"; Zenith2/Docs/Design/STREAM/Runtime.md → "Admission (step 10)"

### Z2-R-STREAM-064 Cancel before preparation start

- **Statement:** Unloading a LOADING cell whose preparation has not started removes the pending preparation, receives `OnPreparationEnded` and releases the cell at the discarded publication SCENE parks at once.
- **Covers:** Z2-I-STREAM-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Streaming_CancelReload1000Cycles_NoLeak` (stage "before start", hit count above 0) (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Cancellation at every stage"

### Z2-R-STREAM-065 Cancel during I/O

- **Statement:** Unloading a cell whose scene or a hard dependency is in ASSET's `HEADER_READ` or `SECTIONS` `READING` cancels SCENE's operation and drops the scene handle, and ASSET cancels the reads when that was the pipeline's last live request, with or without the service's booster alive.
- **Covers:** Z2-I-STREAM-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-ASSET-005
- **Acceptance evidence:** `Test_Streaming_CancelReload1000Cycles_NoLeak` (stage I/O, with and without a booster, hit count above 0) (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Cancellation at every stage"; Zenith2/Docs/Design/STREAM/Runtime.md → "Ownership of partial results"

### Z2-R-STREAM-066 Cancel during decompression

- **Statement:** Unloading a cell while a zstd section of its scene or a hard dependency is in ASSET's `SECTIONS` `DECODING` stage stops the decode slice at its next boundary, destroys its compression context and releases its staging, leaving no decode slice running for the cell.
- **Covers:** Z2-I-STREAM-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-ASSET-002
- **Acceptance evidence:** `Test_Streaming_CancelReload1000Cycles_NoLeak` (stage ASSET decompression, held by ASSET's stage hold of row ST20, hit count above 0) (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Cancellation at every stage"; Zenith2/Docs/Design/STREAM/Tests.md → "Assumed test support of other areas"

### Z2-R-STREAM-067 Cancel during GPU upload

- **Statement:** Unloading a cell while a dependency's GPU upload is in flight (ASSET `DELIVER`/`ACTIVATING`) drops the last reference, and the kind loader's `Cancel` or `Unload` retires staging and pool ranges through deferred deletion, so the RHI and GEOM resource counters return to their pre-test values.
- **Covers:** Z2-I-STREAM-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-GEOM-003
- **Acceptance evidence:** `Test_Streaming_CancelReload1000Cycles_GPUUploads` in `Zenith_Integration_StreamingRender` on WARP and lavapipe (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Cancellation at every stage"; Zenith2/Docs/Design/STREAM/Tests.md → "Render tests"

### Z2-R-STREAM-068 Cancel during preparation building and gate

- **Statement:** Unloading a cell in SCENE's DEPENDENCIES, GATED or BUILDING releases every acquired handle, interned string and the init storage, and a closed gate of a detached world opens so the operation reaches its cancellation point.
- **Covers:** Z2-I-STREAM-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Streaming_CancelReload1000Cycles_NoLeak` (stages DEPENDENCIES, GATED, BUILDING, hit counts above 0) (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Cancellation at every stage"; Zenith2/Docs/Design/STREAM/PublicAPI.md → "Zenith_StreamingActivationHook"

### Z2-R-STREAM-069 Cancel while queued

- **Statement:** Unloading a QUEUED cell drops SCENE's queue entry and prepared scene, and no entity is created.
- **Covers:** Z2-I-STREAM-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingCellMachine_RowT9`, the stress's QUEUED stage (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Cancellation at every stage"; Zenith2/Docs/Design/STREAM/Runtime.md → "Transitions"

### Z2-R-STREAM-070 Cancel of presentation requests

- **Statement:** Destroying a prefetch, booster, HLOD or PSO-group operation handle cancels that request and releases its presentation handle-pool entry within one frame.
- **Covers:** Z2-I-STREAM-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingService_PrefetchCappedAndCancelled`, `Test_StreamingService_GateRules` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Cancellation at every stage"; Zenith2/Docs/Design/STREAM/Runtime.md → "Request state machine"

### Z2-R-STREAM-071 Ownership of partial results

- **Statement:** Every partial result of a cell load has exactly the owner and last releaser of "Ownership of partial results".
- **Covers:** Z2-I-STREAM-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_Streaming_CancelReload1000Cycles_NoLeak` residue assertions (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Ownership of partial results"

### Z2-R-STREAM-072 Completion reporting

- **Statement:** A cell reports released (`stream.cells.released`) only when it is UNLOADED, its slot's simulation bit is clear and every simulation-side handle is dropped; presentation handles are destroyed within one frame.
- **Covers:** Z2-I-STREAM-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingCellMachine_RowT16`, `Test_StreamingCellMachine_RowT18` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Completion reporting"; Zenith2/Docs/Design/STREAM.md → "Cancellation summary"

### Z2-R-STREAM-073 1,000-cycle cancellation stress

- **Statement:** 1,000 seeded load-then-cancel cycles at randomised stages (ASSET I/O, decompression and dependency activation; SCENE decode, dependencies, gate and build; queue; activation), each run with and without a live booster, leave no entity, asset reference, interned string, presentation handle, reservation or allocation; `stream.cells.released` equals the loads issued and every stage's hit count is above 0.
- **Covers:** Z2-I-STREAM-04, Z2-I-STREAM-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_Streaming_CancelReload1000Cycles_NoLeak` in `Zenith_Integration_Streaming` (`Test`, `win-release`) and in the Stage 4 acceptance run.
- **Design:** Zenith2/Docs/Design/STREAM/Tests.md → "Stress and determinism tests"; Zenith2/Docs/Design/STREAM.md → "Acceptance evidence"

### Z2-R-STREAM-074 Stuck cancellation is diagnosed

- **Statement:** A cell in CANCELLING longer than 600 ticks is logged once under `stream.cell`; the stress asserts none occurs.
- **Covers:** Z2-I-STREAM-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingCellMachine_RowT20`, `Test_StreamingStress_RapidTraversal` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Transitions"; Zenith2/Docs/Design/STREAM.md → "Cancellation failures"

### Z2-R-STREAM-075 Snapshot pool sizing

- **Statement:** A world's snapshot pool has `uSTREAMING_SNAPSHOT_SLOTS` = `uSTREAMING_MAX_CONCURRENT_LOADS` + `uSTREAMING_MAX_QUEUED_CELLS` = 112 slots (0.9 MiB with their entity tables), at least the in-flight loads (LOADING, CANCELLING and QUEUED, bounded by the pending count with rejected unloads included) any save written within the configuration ranges can hold, so a restore re-requesting every in-flight load never exhausts it; any acquisition refusal is counted and its load released, never silently left.
- **Covers:** Z2-I-STREAM-02, Z2-I-STREAM-04, Z2-I-STREAM-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** Z2-B-STREAM-001
- **Acceptance evidence:** `Test_StreamingIntegration_SaveWithManyQueuedCells`, `Test_StreamingIntegration_RestoreBeyondPoolReleases`, `Test_StreamingAdmission_PendingCapBoundsQueued`, `Test_StreamingTypes_LimitsMatchFormats` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/PublicAPI.md → "Zenith_StreamingSnapshotPool"; Zenith2/Docs/Design/STREAM/Runtime.md → "Restored loads beyond the pool"; Zenith2/Docs/Design/STREAM/Runtime.md → "Caps"

### Z2-R-STREAM-076 Concurrent slot safety

- **Statement:** Slots are released by whichever of simulation and preparation clears the last holder bit, every tag resolves to its slot while held, and every interleaving of one slot's six concurrent operations keeps the protocol; the coverage stands in for ThreadSanitizer, which does not build a module.
- **Covers:** Z2-I-STREAM-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-041
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingSnapshotPool_EveryInterleavingOfOneSlot` (`Test`), `Test_StreamingSnapshotPool_ConcurrentGateAndEnd` under `Sanitizers -Preset win-asan`.
- **Design:** Zenith2/Docs/Design/STREAM/PublicAPI.md → "Zenith_StreamingSnapshotPool"; Zenith2/Docs/Design/STREAM.md → "Stress, fuzz and sanitizer runs"

### Z2-R-STREAM-077 Stable hook tags

- **Statement:** A load's hook tag is `Zenith_MakeStreamingHookTag(cell, tick)`, a pure function of simulation state, unique among a world's live loads and across a restore; its cell bits are informational and never read to identify a cell.
- **Covers:** Z2-I-STREAM-02, Z2-I-STREAM-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingTypes_HookTagStable` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/PublicAPI.md → "Zenith_StreamingSnapshotPool"

### Z2-R-STREAM-078 Hook outlives its preparations

- **Statement:** A detached world's pool and hook stay alive until every preparation holding a slot has ended, and answer safely without a world state.
- **Covers:** Z2-I-STREAM-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingSnapshotPool_RetiredUntilFree` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/PublicAPI.md → "Zenith_StreamingActivationHook"; Zenith2/Docs/Design/STREAM/Runtime.md → "World transitions and shutdown"

### Z2-R-STREAM-079 SCENE hook contract

- **Statement:** SCENE calls the hook's eight members at exactly the points and threads of the assumed declaration, polling the gate before BUILDING and calling `ShouldSkipEntity` and `AmendComponents` only after it opens, and meets obligations (a) to (j) of "SCENE activation hook".
- **Covers:** Z2-I-STREAM-02, Z2-I-STREAM-04, Z2-I-STREAM-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** SCENE's `Test_AdditiveScenes_HookCallPoints`, `Test_AdditiveScenes_DiscardResolverRecordsCancelled` and `Test_AdditiveScenes_AssetErrorDetailPublished`, which the row ST2 patch registers; STREAM's `Test_StreamingPersistence_TombstoneSkipsAtPreparation`, `Test_StreamingPersistence_ComponentAddedAndRemovedSurvive`, `Test_StreamingCellMachine_RowT18`, `Test_StreamingDeterminism_DiscardResolvedAtPublication` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/PublicAPI.md → "SCENE activation hook"

### Z2-R-STREAM-080 Cooked reference table

- **Statement:** Cross-cell references are recorded in the document and the index with source and target cells and entities, the component, the property and the element, at most 4,096 per cell per direction, with the source slot nulled in the cell scene.
- **Covers:** Z2-I-STREAM-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingCook_MessageV22`, `Test_StreamingWorldFormat_ReaderCheckS16`, `Test_StreamingWorldFormat_ReaderCheckS17` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Formats.md → "Reference sections"; Zenith2/Docs/Design/STREAM/Formats.md → "Cross-cell reference nulling"

### Z2-R-STREAM-081 Bind both directions at activation

- **Statement:** When a cell activates, every reference between it and an ACTIVE cell is written in both directions in the activation sync point, whichever cell activated first.
- **Covers:** Z2-I-STREAM-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingReferences_BindBothDirectionsSecondActivationWins`, `Test_StreamingIntegration_ReferencesAcrossTraversal` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Binding"

### Z2-R-STREAM-082 Unbind at unload

- **Statement:** When a cell unloads, every slot of an ACTIVE cell that still names an entity of it is nulled at that sync point; a slot gameplay re-pointed is untouched.
- **Covers:** Z2-I-STREAM-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingReferences_UnbindNullsOnlyStaleHandle` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Unbinding"

### Z2-R-STREAM-083 Fixed-array elements only

- **Statement:** A reference slot is a scalar entity property or one fixed-array element; containers of entities are refused at cook, so binding never allocates.
- **Covers:** Z2-I-STREAM-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingReferences_FixedArrayElement`, `Test_StreamingCook_MessageV22` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Rules"; Zenith2/Docs/Design/STREAM/Formats.md → "Reference object"

### Z2-R-STREAM-084 Unresolved references counted

- **Statement:** A reference whose target is missing stays null and counts `stream.references.unresolved`, logged once per cell and reference per session.
- **Covers:** Z2-I-STREAM-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingReferences_MissingTargetCountsUnresolved` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Binding"; Zenith2/Docs/Design/STREAM.md → "Log categories"

### Z2-R-STREAM-085 Persistence modes

- **Statement:** An entity with `Zenith.Streaming.Persistent` mode DESTRUCTION keeps its destruction and mode STATE also the presence and eligible state of its configured components across unload and reload; others reload authored; an unknown mode is treated as NONE and counted.
- **Covers:** Z2-I-STREAM-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingPersistence_StateRestoredAtActivation`, `Test_StreamingPersistence_TombstoneSkipsAtPreparation` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Modes"; Zenith2/Docs/Design/STREAM/Formats.md → "Persistent markers"

### Z2-R-STREAM-086 Eligible property kinds

- **Statement:** Capture includes only the eligible kinds of the table and skips the rest, counted once per type.
- **Covers:** Z2-I-STREAM-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingPersistence_CaptureEligibleKindsOnly` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Eligible property kinds"

### Z2-R-STREAM-087 Tombstones from destruction

- **Statement:** A persistent entity destroyed while its cell is ACTIVE or ACTIVATING, other than by its cell's accepted unload (the "unload in progress" window, which only the unload of an ACTIVE cell opens and which never outlives it), gets a pending tombstone bit written by STREAM's listener into its cell's holder at the destroy's own playback, committed at the next update and discarded only when the cell's instance was externally unloaded; no tombstone is queued or dropped; a death that records no bit is written DESTROYED by the capture's absence rule; a destroy in phase 1 after `Streaming.Update` in the unload's own tick keeps the captured state, a window only an engine phase-1 node could open and any such node closes by declaring `Before("Streaming.Update")`.
- **Covers:** Z2-I-STREAM-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingPersistence_UnloadDestroysAreNotTombstones`, `Test_StreamingPersistence_DeathInLaterPhaseAfterRejectedUnload`, `Test_StreamingPersistence_PhaseOneDestroyInUnloadTick`, `Test_StreamingPersistence_ManyDeathsInOneTickAllTombstoned`, `Test_StreamingPersistence_TombstonesWrittenAtDestroyPlayback` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Tombstones"; Zenith2/Docs/Design/STREAM/PublicAPI.md → "Zenith_StreamingLifecycleListener"

### Z2-R-STREAM-088 Tombstoned entities skipped at preparation

- **Statement:** A tombstoned entity and its descendants are removed from the prepared cell scene, and references naming it become null.
- **Covers:** Z2-I-STREAM-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingPersistence_TombstoneSkipsAtPreparation`, `Test_StreamingSnapshotPool_EntityTableAnswers` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Tombstones"; Zenith2/Docs/Design/STREAM/PublicAPI.md → "SCENE activation hook"

### Z2-R-STREAM-089 Capture at unload

- **Statement:** An unloading ACTIVE cell's persistent entities are captured in phase 1 of the unload tick, each STATE record stating every configured type as present (with its field set) or absent, and written to its delta holder in the unload's own buffer, with tombstoned, vanished and reparented-out entities recorded as destroyed.
- **Covers:** Z2-I-STREAM-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** Z2-B-STREAM-002
- **Acceptance evidence:** `Test_StreamingPersistence_StateRestoredAtActivation`, `Test_StreamingPersistence_ReparentedOutSurvivesWithoutDuplicate` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Capture"

### Z2-R-STREAM-090 Delta encoding

- **Statement:** A cell delta is the header and sorted entity records of "Cell delta encoding" (DESTROYED records; STATE records with an ascending absent-type list and ascending present field sets), byte-identical for equal content.
- **Covers:** Z2-I-STREAM-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingPersistence_DeltaRoundTrip`, `Test_StreamingPersistence_DeltaGoldenBytes` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Formats.md → "Delta header"; Zenith2/Docs/Design/STREAM/Formats.md → "Entity records"

### Z2-R-STREAM-091 Delta decoder checks

- **Statement:** The decoder applies D1 to D9 and never asserts; a failing delta activates its cell authored with `STREAMING_ERROR_DELTA_CORRUPT` logged.
- **Covers:** Z2-I-STREAM-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingPersistence_DecoderChecks`, `Test_StreamingPersistence_CorruptDeltaActivatesAuthored` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Formats.md → "Decoder checks"; Zenith2/Docs/Design/STREAM/Runtime.md → "Restore"

### Z2-R-STREAM-092 Delta capacity

- **Statement:** A delta never exceeds 65,535 bytes or 1,024 records; on overflow STATE records are dropped in descending ID and DESTROYED records kept, logged once and counted.
- **Covers:** Z2-I-STREAM-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** Z2-B-STREAM-001
- **Acceptance evidence:** `Test_StreamingPersistence_OverflowDropsStateKeepsTombstones` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Capture"; Zenith2/Docs/Design/STREAM/Formats.md → "Delta header"

### Z2-R-STREAM-093 Restore at activation

- **Statement:** `Streaming.ApplyCellState` applies the cell's delta values in the activation sync point before reference binding, so restored values are the first any system sees, except in the T12 case (STREAM's admission buffer rejected), where they appear one tick late, overwrite what gameplay wrote to the restored properties in the activation tick, leave that tick's tombstones standing (admission, not the playback, clears the bits) and are counted; a QUEUED instance (SCENE's activation rejected) is skipped and counted, not treated as malformed.
- **Covers:** Z2-I-STREAM-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingPersistence_StateRestoredAtActivation`, `Test_StreamingActions_RejectedActivationLateApply`, `Test_StreamingActions_ApplyMeetsQueuedInstance` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Restore"

### Z2-R-STREAM-094 World positions anchor-relative

- **Statement:** A property flagged `PROPERTY_FLAG_WORLD_POSITION` is stored anchor-relative in a delta and converted back at restore, so an origin shift while the cell is unloaded never moves it.
- **Covers:** Z2-I-STREAM-07, Z2-I-STREAM-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingPersistence_WorldPositionAnchorRelative` (`Test`), once row ST4 lands.
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Eligible property kinds"; Zenith2/Docs/Design/STREAM/PublicAPI.md → "ECS world-position flag"

### Z2-R-STREAM-095 Delta versions and schema migration

- **Statement:** A delta of an older format version is decoded by its version's decoder and rewritten at the next capture, and each component record is migrated to its type's current schema before it is applied.
- **Covers:** Z2-I-STREAM-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingPersistence_SchemaMigratedAtRestore` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Formats.md → "Versioning"; Zenith2/Docs/Design/STREAM.md → "Migration hooks"

### Z2-R-STREAM-096 Persistence across reload stress

- **Statement:** 50 random modifications and destructions over 20 cells survive 10 traversal round trips.
- **Covers:** Z2-I-STREAM-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingIntegration_PersistAcrossReload` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Tests.md → "Integration tests"

### Z2-R-STREAM-097 Region flag

- **Statement:** Every `Zenith.Streaming.RegionBound` entity's `InActiveRegion` and `CellIndex` are written each tick from the region grid (first COLLISION grid, else first GAMEPLAY), true everywhere in an unbound world.
- **Covers:** Z2-I-STREAM-09, Z2-I-STREAM-10, Z2-I-STREAM-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingRegions_FlagFollowsCollisionCell`, `Test_StreamingRegions_UnboundWorldAllTrue`, `Test_StreamingRegions_GameplayGridFallback` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "The region"

### Z2-R-STREAM-098 Region destroy policy

- **Statement:** An entity with policy DESTROY is destroyed exactly in the tick its region flag is computed false while its saved flag is true and its saved evaluation tick is the previous tick, so a fresh spawn, a freshly bound world or a long-unevaluated entity is never destroyed by its first evaluation, and a restored run destroys exactly what the uninterrupted run destroys.
- **Covers:** Z2-I-STREAM-09, Z2-I-STREAM-10, Z2-I-STREAM-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingRegions_DestroyPolicyOnTransition`, `Test_StreamingRegions_DestroyNeedsBoundPreviousEvaluation` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "The region"

### Z2-R-STREAM-099 Physics coordination

- **Statement:** Static collision lives in COLLISION cells that GAMEPLAY cells depend on, and PHYS freezes a body whose region flag is false, so no body simulates over absent collision.
- **Covers:** Z2-I-STREAM-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingIntegration_CollisionBeforeGameplay`, `Test_StreamingIntegration_RegionFreezesBodies` (`Test`), with PHYS's freeze test once row ST8 lands.
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Physics coordination"; Zenith2/Docs/Design/STREAM/Formats.md → "Content of each grid role"

### Z2-R-STREAM-100 Navigation coordination

- **Statement:** Navigation tiles live in NAVIGATION cells that GAMEPLAY cells depend on, and AI suspends an agent whose region flag is false; tile components follow D-SEL-NAV (open, Stage 1/2).
- **Covers:** Z2-I-STREAM-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** AI's navigation streaming test once row ST9 and D-SEL-NAV land; `Test_StreamingDesired_ClosurePinsDependencies` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Navigation coordination"; Zenith2/Docs/Design/STREAM/Formats.md → "Content of each grid role"

### Z2-R-STREAM-101 Audio coordination

- **Statement:** Ambience, emitters and acoustic volumes live in AUDIO cells with their banks as the cells' hard dependencies, and AUDIO virtualises an emitter whose region flag is false.
- **Covers:** Z2-I-STREAM-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** AUDIO's cell-streaming test once row ST10 lands; `Test_StreamingIntegration_TraversalLoadsAndUnloads` with an AUDIO grid (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Audio coordination"; Zenith2/Docs/Design/STREAM/Formats.md → "Content of each grid role"

### Z2-R-STREAM-102 Data coupling only

- **Statement:** PHYS, AI, AUDIO, ENV and LIGHT read STREAM's components and `Zenith.WorldOrigin` by stable name and never call STREAM or write its properties.
- **Covers:** Z2-I-STREAM-09, Z2-I-STREAM-10, Z2-I-STREAM-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Libgraph` (no module-to-module edge); SCRIPT's authority checks over STREAM's properties (`Freeze`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "What other modules may and may not do"; Zenith2/Docs/Design/STREAM.md → "Design choices at a glance"

### Z2-R-STREAM-103 Light content loads with its cell

- **Statement:** Reflection probes, irradiance volumes and lightmap sets are components of GAMEPLAY-cell entities (the lightmap set on the anchor root) whose baked assets are hard dependencies of the cell scene, entering at the activation sync point and leaving at the unload.
- **Covers:** Z2-I-STREAM-01, Z2-I-STREAM-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** LIGHT's residency tests (row ST12) over `Zenith_StreamingTestWorld` cells; `Test_StreamingIntegration_TraversalLoadsAndUnloads` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Formats.md → "Content of each grid role"; Zenith2/Docs/Design/STREAM.md → "Areas that consume STREAM"

### Z2-R-STREAM-104 Two coordinate frames

- **Statement:** Absolute positions are binary64 and component positions binary32 relative to `Zenith.WorldOrigin`; STREAM computes every distance and anchor in binary64.
- **Covers:** Z2-I-STREAM-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingRebase_RootTranslationExact`, `Test_StreamingIntegration_RebaseLongJourney` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "The two frames"; Zenith2/Docs/Design/STREAM.md → "Cell key and coordinates"

### Z2-R-STREAM-105 Root translation at activation

- **Statement:** An admitted cell is placed with the root translation `(float)(anchor − O)` applied by SCENE to its roots and recorded world positions.
- **Covers:** Z2-I-STREAM-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingRebase_RootTranslationExact` (`Test`), once row ST2 lands.
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Root translation"; Zenith2/Docs/Design/STREAM/PublicAPI.md → "SCENE activation hook"

### Z2-R-STREAM-106 Rebase rule

- **Statement:** With rebasing enabled, a shift of `round(L/q)·q` is recorded when the primary source exceeds `stream.rebase.thresholdMetres` (2,048) on a rebased axis, at least `stream.rebase.minIntervalTicks` (30) after the last, including in a tick with an activating cell (SCENE plays the shift last in step 7), so `|L| ≤ T + v·N/60` holds.
- **Covers:** Z2-I-STREAM-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingRebase_ThresholdIntervalQuantum`, `Test_StreamingRebase_ShiftInActivationTick` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Rebase rule"

### Z2-R-STREAM-107 Rebasing off by default

- **Statement:** The default configuration records no origin shift, and binding a world with rebasing off that has a cell beyond the 4,096 m precision radius logs once and sets `stream.rebase.precision_warning`.
- **Covers:** Z2-I-STREAM-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingRebase_DisabledByDefault`, `Test_StreamingBinding_PrecisionWarningWithoutRebase` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Rebase rule"; Zenith2/Docs/Design/STREAM/PublicAPI.md → "Configuration keys"; Zenith2/Docs/Design/STREAM/Runtime.md → "World binding"

### Z2-R-STREAM-108 Long journey precision

- **Statement:** Over a 60 km journey at 300 m/s with continuous admissions and rebasing at the defaults, exactly 29 shifts are recorded, the primary's local position never exceeds 2,198 m on an axis, and every active entity's absolute position stays within 1 mm of its authored absolute position.
- **Covers:** Z2-I-STREAM-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** Z2-B-SHIP-016
- **Acceptance evidence:** `Test_StreamingIntegration_RebaseLongJourney` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Rebase rule"; Zenith2/Docs/Design/STREAM/Tests.md → "Integration tests"

### Z2-R-STREAM-109 Rebase consumers

- **Statement:** After a shift, RPROD performs a full-state extraction that translates previous-frame and previous-view transforms by −S instead of resetting histories, STREAM's HLOD records are updated with the shifted translation, and scripts read the shifted origin.
- **Covers:** Z2-I-STREAM-05, Z2-I-STREAM-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingHLOD_RebaseUpdatesTransforms` (`Test`); ENV's `Test_EnvironmentCells_RebaseReextracts` once row ST7 lands.
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Who reacts to a rebase"; Zenith2/Docs/Design/STREAM/PublicAPI.md → "RPROD origin-change rule"

### Z2-R-STREAM-110 Teleport detection

- **Statement:** A source is teleported when its saved `TeleportPending`, set by STREAM's listener at the teleport's playback, is true, or when it moved more than `stream.teleport.distanceMetres` since its saved previous position of the previous tick; its `TeleportSequence` increments.
- **Covers:** Z2-I-STREAM-02, Z2-I-STREAM-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingIntegration_TeleportReadiness`, `Test_StreamingSources_PreviousPositionPersisted` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Teleports"

### Z2-R-STREAM-111 Readiness per source

- **Statement:** Each tick every source's `MissingRequiredCells` and `Ready` are written from its REQUIRED band, and `stream.residency_miss_ticks` counts ticks with a primary source not ready.
- **Covers:** Z2-I-STREAM-02, Z2-I-STREAM-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** Z2-B-STREAM-005
- **Acceptance evidence:** `Test_StreamingIntegration_TeleportReadiness`, `Test_StreamingIntegration_TwoSourcesShareCells` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Readiness"

### Z2-R-STREAM-112 Teleport-to-ready measurement

- **Statement:** The service publishes `stream.teleport.last_to_ready_us` from the frames of a teleport and of the source becoming ready, reading only committed state.
- **Covers:** Z2-I-STREAM-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-STREAM-004
- **Acceptance evidence:** `Test_StreamingService_TeleportToReadyMeasured` (`Test`); MP-STREAM runs.
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Teleport-to-ready measurement"

### Z2-R-STREAM-113 Service node

- **Statement:** `Streaming.Service` is an epilogue stage of the world that reads committed state, writes only STREAM's presentation state (never a component or resource; it differences cumulative counters against its own copy), records no command, and stops at `stream.service.frameBudgetUs`, resuming in order.
- **Covers:** Z2-I-STREAM-02, Z2-I-STREAM-03, Z2-I-STREAM-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-050
- **Budgets:** Z2-B-STREAM-002
- **Acceptance evidence:** `Test_StreamingService_FrameBudgetResumes`, `Test_StreamingService_CountersDifferencedPerFrame` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Service node declaration"; Zenith2/Docs/Design/STREAM/Runtime.md → "Sub-steps, in order"

### Z2-R-STREAM-114 HLOD hierarchy

- **Statement:** HLOD nodes form per-grid hierarchies of up to three levels, each level-k node covering 4^(k−1) × 4^(k−1) base cells with 1 to 4 proxy meshes, stored breadth-first.
- **Covers:** Z2-I-STREAM-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingWorldFormat_ReaderCheckS18`, `Test_StreamingWorldFormat_ReaderCheckS19`, `Test_StreamingCook_MessageV20`, `Test_StreamingCook_MessageV21` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Formats.md → "HLOD node records"; Zenith2/Docs/Design/STREAM/Formats.md → "HLOD object"

### Z2-R-STREAM-115 HLOD display cut

- **Statement:** Each frame, within every top-level node's maximum distance, each base cell holding a cell is covered by exactly one of its content (ACTIVE or ACTIVATING), one READY proxy chosen by refine distances, or a counted gap; a level-1 proxy is displayed whenever its cell holds no content, so a loading cell keeps its proxy until it is active, and an UNLOADING cell holds none.
- **Covers:** Z2-I-STREAM-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingHLOD_CutNestsByDistance`, `Test_StreamingHLOD_ActiveContentSuppressesProxy`, `Test_StreamingHLOD_ProxyHeldUntilCellActive`, `Test_StreamingHLOD_NotReadyFallsBackAndCountsGaps` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "The cut"

### Z2-R-STREAM-116 HLOD residency hysteresis

- **Statement:** Proxy meshes are requested within max distance + `stream.hlod.hysteresisMetres` and released beyond max + 2 × hysteresis; a failed mesh disables its node for the session.
- **Covers:** Z2-I-STREAM-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-STREAM-001
- **Acceptance evidence:** `Test_StreamingHLOD_ResidencyHysteresis` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Residency"

### Z2-R-STREAM-117 HLOD extraction

- **Statement:** `Streaming.ExtractHLOD` writes module-key instance deltas (REMOVEs then ADDs in ascending key), UPDATEs every displayed node at a rebase, re-emits everything on a full-state extraction, and keeps refused writes for the next frame.
- **Covers:** Z2-I-STREAM-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-050, Z2-D-043
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingHLOD_ExtractorKeysAndOrder`, `Test_StreamingHLOD_FullExtractionReemits`, `Test_StreamingHLOD_RebaseUpdatesTransforms` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Extraction"

### Z2-R-STREAM-118 HLOD is presentation only

- **Statement:** HLOD creates no entity, enters no save or world hash, and is shed under memory pressure.
- **Covers:** Z2-I-STREAM-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingDeterminism_MemoryPressureDoesNotChangeTicks` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Extraction"; Zenith2/Docs/Design/STREAM.md → "Design choices at a glance"

### Z2-R-STREAM-119 PSO groups per cell

- **Statement:** In each epilogue the service requests the `FLUX_PSO_GROUP_STREAMING_CELL` group (`Zenith_MakeStreamingGroupID(cell scene)`) of every slot with the simulation bit set, its entity table filled and no request yet, live or restored, and opens the slot's gate on completion, failure, 300 frames after that request, 30 frames of a full queue, at `Acquire` headless for a live load, and in R4 headless for a restored one.
- **Covers:** Z2-I-STREAM-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingService_GateRules`, `Test_StreamingService_GateRulesRestoredLoadRenderer`, `Test_StreamingService_GateRulesRestoredLoadHeadless`, `Test_StreamingService_GroupRequestedForLoadCommittedInSameTick`, `Test_StreamingRender_PSOGateOpensOnGroupCompletion`, `Test_StreamingTypes_GroupIDMatchesHashOfGuid` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "PSO groups and the preparation gate"; Zenith2/Docs/Design/STREAM/PublicAPI.md → "RPROD origin-change rule"

### Z2-R-STREAM-120 Memory consumer

- **Statement:** STREAM's consumer reports `MEMORY_SUBSYSTEM_STREAMING` resident bytes of world assets, world states, snapshot pools and presentation states, without counting cell content twice.
- **Covers:** Z2-I-STREAM-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** Z2-B-STREAM-001
- **Acceptance evidence:** MP-SUBMEM capture of W-Outdoor and W-OpenWorld; `Test_StreamingModule_StartRegistersEverything` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Resident bytes"; Zenith2/Docs/Design/STREAM/PublicAPI.md → "Zenith_StreamingMemoryConsumer"

### Z2-R-STREAM-121 Shedding per level

- **Statement:** At WARNING STREAM halves the prefetch radius and cancels background requests; at CRITICAL it cancels every prefetch, releases presentation residency outside the REQUIRED band except the top-level HLOD nodes of the cut, and issues boosters only for required cells; no simulation cell is unloaded; each within one frame, reverted one frame after the level falls.
- **Covers:** Z2-I-STREAM-02, Z2-I-STREAM-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-STREAM-001
- **Acceptance evidence:** STREAM's row of `axMEMORY_SHEDDING_CASES` in the memory suite (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Shedding"

### Z2-R-STREAM-122 Shedding never changes simulation

- **Statement:** No world hash changes when memory pressure is forced to WARNING or CRITICAL mid-run.
- **Covers:** Z2-I-STREAM-02, Z2-I-STREAM-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingDeterminism_MemoryPressureDoesNotChangeTicks` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Shedding"; Zenith2/Docs/Design/STREAM/Runtime.md → "Replay and determinism"

### Z2-R-STREAM-123 Double-residency reservation

- **Statement:** A prefetch under a displayed proxy reserves `TRANSITION_PEAK_STREAMING_DOUBLE_RESIDENCY` first and is deferred on refusal; a required load is never deferred: it reserves at once and holds the reservation for the whole load, and on refusal its covering proxy is released and its region counted as gaps until it activates.
- **Covers:** Z2-I-STREAM-02, Z2-I-STREAM-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-CORE-006
- **Acceptance evidence:** `Test_StreamingService_PrefetchPeakDeferred`, `Test_StreamingService_RequiredLoadReservesDoubleResidency`, `Test_StreamingService_RequiredLoadRefusedReleasesProxy` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Double-residency reservation"

### Z2-R-STREAM-124 Simulation reads only simulation state

- **Statement:** `Streaming.Update` and STREAM's playbacks read only components, `Zenith.WorldOrigin`, SCENE's additive set, the cooked world, the catalogue, the tick and simulation-affecting configuration.
- **Covers:** Z2-I-STREAM-01, Z2-I-STREAM-02, Z2-I-STREAM-03, Z2-I-STREAM-04, Z2-I-STREAM-05, Z2-I-STREAM-06, Z2-I-STREAM-07, Z2-I-STREAM-08, Z2-I-STREAM-09, Z2-I-STREAM-10, Z2-I-STREAM-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingDeterminism_IODelayDoesNotChangeTicks` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM.md → "Determinism obligations"; Zenith2/Docs/Design/STREAM/Runtime.md → "Replay and determinism"

### Z2-R-STREAM-125 Replay across workers and backends

- **Statement:** A recorded 3,000-tick traversal replays with equal per-tick world hashes at 1, 2, 8 and all workers, with forced compaction and two chunk capacities, on each ECS backend.
- **Covers:** Z2-I-STREAM-01, Z2-I-STREAM-02, Z2-I-STREAM-03, Z2-I-STREAM-04, Z2-I-STREAM-05, Z2-I-STREAM-06, Z2-I-STREAM-07, Z2-I-STREAM-08, Z2-I-STREAM-09, Z2-I-STREAM-10, Z2-I-STREAM-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingDeterminism_ReplayAcrossWorkers` (`Test`, `win-release`).
- **Design:** Zenith2/Docs/Design/STREAM/Tests.md → "Stress and determinism tests"; Zenith2/Docs/Design/STREAM/Runtime.md → "Replay and determinism"

### Z2-R-STREAM-126 I/O timing enters only through publications

- **Statement:** Randomised I/O delays change only the publication ticks, and each run replays exactly.
- **Covers:** Z2-I-STREAM-01, Z2-I-STREAM-02, Z2-I-STREAM-03, Z2-I-STREAM-04, Z2-I-STREAM-05, Z2-I-STREAM-06, Z2-I-STREAM-07, Z2-I-STREAM-08, Z2-I-STREAM-09, Z2-I-STREAM-10, Z2-I-STREAM-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingDeterminism_IODelayDoesNotChangeTicks` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Replay and determinism"

### Z2-R-STREAM-127 State saved as components

- **Statement:** STREAM's saved state is its components (cell deltas and cell status on per-cell holders keyed by cell scene GUID, anchors, persistent markers, sources with their previous positions and pending teleports, region policies and flags with their evaluation ticks, the world reference with its binding state and override slots) and SCENE's additive set; STREAM registers no save participant and excludes no entity; a capture is COHERENT only with a cell LOADING, QUEUED or CANCELLING or with a tagged commit refused at a sync point of its tick, each reported by SCENE's participant.
- **Covers:** Z2-I-STREAM-07, Z2-I-STREAM-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** SAVE's `Test_SaveCrossSystem_UnloadSaveReloadEquivalent`, `Test_SaveCrossSystem_DestroyedPersistentStaysDestroyed`; STREAM's `Test_StreamingIntegration_RefusedCommitCaptureIsCoherent` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "What is saved"; Zenith2/Docs/Design/STREAM/Runtime.md → "Persisted cell status"

### Z2-R-STREAM-128 Rebuild after restore

- **Statement:** After a restore (reset signal) the update rebuilds every cell's record from its holder's saved status, cross-checked with SCENE's live instances by scene GUID and anchor, makes UNLOADED and unclaimed every cell whose persisted instance SCENE now reports LOADING or QUEUED (a rejected activation or a rejected unload of a QUEUED instance included), adopts restored in-flight loads, marks the holders whose records must be re-homed (moving none) and writes back what it changed, so the next update applies the rows the uninterrupted run applies.
- **Covers:** Z2-I-STREAM-07, Z2-I-STREAM-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingIntegration_SaveDuringStreaming`, `Test_StreamingIntegration_RestoreActiveWithPendingStateApply`, `Test_StreamingIntegration_RestoreRejectedActivationAdopts`, `Test_StreamingIntegration_ExactRestoreWithFailedAndDelayedCells` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Rebuild"

### Z2-R-STREAM-129 Restored in-flight loads

- **Statement:** A LOADING or QUEUED cell instance re-requested by SAVE with its saved hook tag is adopted by STREAM when the pool gave it a slot, its cell identified by its scene GUID, its entity table filled from the restored delta before its gate opens; every other in-flight instance of a cell scene is released, never ignored, a slot it holds neutralised first (Z2-R-STREAM-174); a re-request SCENE refuses is counted and needs no release.
- **Covers:** Z2-I-STREAM-07, Z2-I-STREAM-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** SAVE's `Test_SaveCrossSystem_LoadingInstanceReRequestedWithHookTag`, `Test_StreamingIntegration_SaveDuringStreaming`, `Test_StreamingIntegration_SaveWithManyQueuedCells`, `Test_StreamingIntegration_RestoreBeyondPoolReleases`, `Test_StreamingIntegration_RestoreRejectedActivationAdopts` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Rebuild"; Zenith2/Docs/Design/STREAM/PublicAPI.md → "Zenith_StreamingActivationHook"; Zenith2/Docs/Design/STREAM/Runtime.md → "Restored loads beyond the pool"

### Z2-R-STREAM-130 World transition discards in-flight cells

- **Statement:** A world transition with cells loading discards them, leaves no slot held after their preparations end, and the new world binds fresh.
- **Covers:** Z2-I-STREAM-04, Z2-I-STREAM-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingIntegration_WorldTransitionDiscardsInFlight` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "World transitions and shutdown"

### Z2-R-STREAM-131 Shutdown mid-load

- **Statement:** Shutdown with 16 cells loading completes with every slot FREE and no reservation reported by the governor.
- **Covers:** Z2-I-STREAM-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingIntegration_ShutdownMidLoad` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "World transitions and shutdown"; Zenith2/Docs/Design/STREAM.md → "Construction and shutdown"

### Z2-R-STREAM-132 Headless behaviour

- **Statement:** Without a device, every simulation algorithm runs unchanged, a live load's gate opens at acquisition and a restored load's in R4, and no PSO, HLOD or extraction work is issued.
- **Covers:** Z2-I-STREAM-01, Z2-I-STREAM-02, Z2-I-STREAM-03, Z2-I-STREAM-04, Z2-I-STREAM-05, Z2-I-STREAM-06, Z2-I-STREAM-07, Z2-I-STREAM-08, Z2-I-STREAM-09, Z2-I-STREAM-10, Z2-I-STREAM-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingIntegration_HeadlessGatesAndNoPresentation`, `Test_StreamingService_GateRulesRestoredLoadHeadless` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Headless runs"

### Z2-R-STREAM-133 Counters

- **Statement:** STREAM publishes every counter of "Counters" with its kind, per world suffixed by the world tag.
- **Covers:** Z2-I-STREAM-01, Z2-I-STREAM-02, Z2-I-STREAM-03, Z2-I-STREAM-04, Z2-I-STREAM-05, Z2-I-STREAM-06, Z2-I-STREAM-07, Z2-I-STREAM-08, Z2-I-STREAM-09, Z2-I-STREAM-10, Z2-I-STREAM-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingDiagnostics_EveryCounterPublished` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM.md → "Counters"

### Z2-R-STREAM-134 Log categories and rate limits

- **Statement:** STREAM logs only under its seven categories with the per-event limits stated.
- **Covers:** Z2-I-STREAM-01, Z2-I-STREAM-02, Z2-I-STREAM-03, Z2-I-STREAM-04, Z2-I-STREAM-05, Z2-I-STREAM-06, Z2-I-STREAM-07, Z2-I-STREAM-08, Z2-I-STREAM-09, Z2-I-STREAM-10, Z2-I-STREAM-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingDiagnostics_LogCategoriesAndLimits` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM.md → "Log categories"

### Z2-R-STREAM-135 Profiler zones

- **Statement:** `Streaming.Update`, `Streaming.Service`, `Streaming.ExtractHLOD` and each playback have the zones and sub-zones listed.
- **Covers:** Z2-I-STREAM-01, Z2-I-STREAM-02, Z2-I-STREAM-03, Z2-I-STREAM-04, Z2-I-STREAM-05, Z2-I-STREAM-06, Z2-I-STREAM-07, Z2-I-STREAM-08, Z2-I-STREAM-09, Z2-I-STREAM-10, Z2-I-STREAM-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** Z2-B-STREAM-002
- **Acceptance evidence:** `Test_StreamingDiagnostics_ZoneNames` (`Test`); the MP-CPUZONE capture of the Stage 4 acceptance runs names the same zones.
- **Design:** Zenith2/Docs/Design/STREAM.md → "Profiler zones"

### Z2-R-STREAM-136 Stream dump

- **Statement:** `Zenith_StreamingStateView::WriteDump` writes `zenith2.stream-dump` version 1 with every member in order, equal byte for byte to the committed hand-written golden files.
- **Covers:** Z2-I-STREAM-01, Z2-I-STREAM-02, Z2-I-STREAM-03, Z2-I-STREAM-04, Z2-I-STREAM-05, Z2-I-STREAM-06, Z2-I-STREAM-07, Z2-I-STREAM-08, Z2-I-STREAM-09, Z2-I-STREAM-10, Z2-I-STREAM-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingState_DumpShape` against `Tests/Golden/stream-dump-unbound.json` and `-bound.json` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Formats.md → "Stream dump format"; Zenith2/Docs/Design/STREAM/PublicAPI.md → "Zenith_StreamingState header"

### Z2-R-STREAM-137 Debug view

- **Statement:** With `stream.debug.view` naming a grid in a non-Shipping build, the service draws cells within 2 km coloured by state, displayed HLOD nodes and source radii.
- **Covers:** Z2-I-STREAM-01, Z2-I-STREAM-02, Z2-I-STREAM-03, Z2-I-STREAM-04, Z2-I-STREAM-05, Z2-I-STREAM-06, Z2-I-STREAM-07, Z2-I-STREAM-08, Z2-I-STREAM-09, Z2-I-STREAM-10, Z2-I-STREAM-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingService_DebugViewRecords` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM.md → "Debug views and dumps"

### Z2-R-STREAM-138 Read-world bindings

- **Statement:** `Streaming.IsPositionActive`, `Streaming.GetCellState` and `Streaming.GetWorldOrigin` return this tick's update's cell states and the current `Zenith.WorldOrigin` (read from the resource, so a rebase tick's phases 2 to 10 get the shifted origin) in systems and behaviours, and misuse raises a controlled Luau error.
- **Covers:** Z2-I-STREAM-02, Z2-I-STREAM-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-002
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingBindings_ReadWorld` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/PublicAPI.md → "Script bindings"

### Z2-R-STREAM-139 Tools-only override binding

- **Statement:** `Streaming.SetCellOverride` is callable only in a tools invocation.
- **Covers:** Z2-I-STREAM-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-002
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingBindings_OverrideToolsOnly` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/PublicAPI.md → "Script bindings"

### Z2-R-STREAM-140 Malformed payloads ignored

- **Statement:** Each STREAM command kind ignores and counts a payload of the wrong size or with an out-of-range field.
- **Covers:** Z2-I-STREAM-01, Z2-I-STREAM-02, Z2-I-STREAM-03, Z2-I-STREAM-04, Z2-I-STREAM-05, Z2-I-STREAM-06, Z2-I-STREAM-07, Z2-I-STREAM-08, Z2-I-STREAM-09, Z2-I-STREAM-10, Z2-I-STREAM-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingCommands_MalformedPayloadsIgnored` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Formats.md → "Module command payloads"; Zenith2/Docs/Design/STREAM/PublicAPI.md → "Command kinds"

### Z2-R-STREAM-141 Budget rows allocated

- **Statement:** The Stage 1 per-system budget ticket for STREAM allocates Z2-B-STREAM-001 to -005 and the pending row "Cell activation entities per tick" from reference or published data.
- **Covers:** Z2-I-STREAM-01, Z2-I-STREAM-02, Z2-I-STREAM-03, Z2-I-STREAM-04, Z2-I-STREAM-05, Z2-I-STREAM-06, Z2-I-STREAM-07, Z2-I-STREAM-08, Z2-I-STREAM-09, Z2-I-STREAM-10, Z2-I-STREAM-11
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-STREAM-001, Z2-B-STREAM-002, Z2-B-STREAM-003, Z2-B-STREAM-004, Z2-B-STREAM-005
- **Acceptance evidence:** The ruling recorded in `Zenith2/Docs/DecisionLog.md` and the rows' values in `Zenith2/Docs/Budgets.md` (the `needs-human` budget ticket).
- **Design:** Zenith2/Docs/Design/STREAM.md → "Rows STREAM owns"; Zenith2/Docs/Design/STREAM.md → "Pending row needs"; Zenith2/Docs/Design/STREAM.md → "Stage 1"

### Z2-R-STREAM-142 Streaming memory within its row

- **Statement:** STREAMING resident and in-flight bytes stay within Z2-B-STREAM-001 in the MP-SUBMEM runs of W-Outdoor and W-OpenWorld, first measured at Stage 4 (the Stage 3 W-Outdoor run does not select `Streaming`, X229).
- **Covers:** Z2-I-STREAM-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-STREAM-001
- **Acceptance evidence:** MP-SUBMEM reports of the Stage 4 acceptance runs, recorded by the `needs-human` acceptance ticket.
- **Design:** Zenith2/Docs/Design/STREAM.md → "Rows STREAM owns"; Zenith2/Docs/Design/STREAM.md → "Acceptance evidence"; Zenith2/Docs/Design/STREAM.md → "Stage 3 W-Outdoor"

### Z2-R-STREAM-143 Streaming CPU within its row

- **Statement:** The frame's `Streaming.Update` spans plus `Streaming.Service` and `Streaming.ExtractHLOD` stay within Z2-B-STREAM-002, first measured at Stage 4.
- **Covers:** Z2-I-STREAM-02, Z2-I-STREAM-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-STREAM-002
- **Acceptance evidence:** MP-CPUZONE reports of the Stage 4 acceptance runs, recorded by the `needs-human` acceptance ticket.
- **Design:** Zenith2/Docs/Design/STREAM.md → "Rows STREAM owns"; Zenith2/Docs/Design/STREAM.md → "Acceptance evidence"

### Z2-R-STREAM-144 Teleport and traversal rows

- **Statement:** Teleport-to-resident time and rapid-traversal residency misses stay within Z2-B-STREAM-004 and -005 on the RTX 3070 desktop and the S24, first measured at Stage 4.
- **Covers:** Z2-I-STREAM-02, Z2-I-STREAM-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-STREAM-004, Z2-B-STREAM-005
- **Acceptance evidence:** MP-STREAM runs of W-Outdoor and W-OpenWorld (the device steps are `needs-human`).
- **Design:** Zenith2/Docs/Design/STREAM.md → "Acceptance evidence"; Zenith2/Docs/Design/STREAM/Runtime.md → "Teleport-to-ready measurement"

### Z2-R-STREAM-145 Rapid traversal stress

- **Statement:** A source at 300 m/s for 10,000 ticks never exceeds a cap and leaves no cell in a transitional state beyond 600 ticks.
- **Covers:** Z2-I-STREAM-02, Z2-I-STREAM-03, Z2-I-STREAM-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** Z2-B-STREAM-003
- **Acceptance evidence:** `Test_StreamingStress_RapidTraversal` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Tests.md → "Stress and determinism tests"

### Z2-R-STREAM-146 Traversal integration

- **Statement:** A 2 km walk activates every cell within the activation radius within 30 ticks of entering it and unloads every cell beyond KEEP after its delay, holding every cap.
- **Covers:** Z2-I-STREAM-01, Z2-I-STREAM-02, Z2-I-STREAM-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingIntegration_TraversalLoadsAndUnloads` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Tests.md → "Integration tests"

### Z2-R-STREAM-147 Stage 4 acceptance evidence

- **Statement:** The Stage 4 acceptance run includes the two 1,000-cycle stresses, the first MP-STREAM, MP-SUBMEM and MP-CPUZONE measurements of STREAM's rows and Z2-B-CORE-006 in W-Outdoor and W-OpenWorld, and the OpenWorldSlice playthrough with streaming active.
- **Covers:** Z2-I-STREAM-01, Z2-I-STREAM-02, Z2-I-STREAM-03, Z2-I-STREAM-04, Z2-I-STREAM-05, Z2-I-STREAM-06, Z2-I-STREAM-07, Z2-I-STREAM-08, Z2-I-STREAM-09, Z2-I-STREAM-10, Z2-I-STREAM-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-STREAM-004, Z2-B-STREAM-005
- **Acceptance evidence:** The `Zenith2Acceptance` Stage 4 ticket's run record.
- **Design:** Zenith2/Docs/Design/STREAM.md → "Acceptance evidence"; Zenith2/Docs/Design/STREAM.md → "Stage 3 W-Outdoor"

### Z2-R-STREAM-148 Failure handling table

- **Statement:** Every failure of the failure table is detected where stated and behaves as stated, with no assert on untrusted input.
- **Covers:** Z2-I-STREAM-01, Z2-I-STREAM-02, Z2-I-STREAM-03, Z2-I-STREAM-04, Z2-I-STREAM-05, Z2-I-STREAM-06, Z2-I-STREAM-07, Z2-I-STREAM-08, Z2-I-STREAM-09, Z2-I-STREAM-10, Z2-I-STREAM-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** The failure-path tests of `Zenith_StreamingCellMachine_Tests.cpp`, `Zenith_StreamingPersistence_Tests.cpp` and `Zenith_StreamingWorldFormat_Tests.cpp`, and `Test_StreamingIntegration_ExternalUnloadAndDestroyedAnchor` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM.md → "Failure table"; Zenith2/Docs/Design/STREAM.md → "Missing and corrupt content"

### Z2-R-STREAM-149 Device loss

- **Statement:** During device loss the extractor writes nothing, PSO group failures open their gates, and the first sealed render world after recovery re-emits every displayed HLOD node.
- **Covers:** Z2-I-STREAM-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingService_GateRules` (DEVICE_LOST), `Test_StreamingHLOD_FullExtractionReemits` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM.md → "Device loss"

### Z2-R-STREAM-150 No lock and no custom primitive

- **Statement:** STREAM declares no lock level, holds no lock, and adds no custom container or threading primitive.
- **Covers:** Z2-I-STREAM-01, Z2-I-STREAM-02, Z2-I-STREAM-03, Z2-I-STREAM-04, Z2-I-STREAM-05, Z2-I-STREAM-06, Z2-I-STREAM-07, Z2-I-STREAM-08, Z2-I-STREAM-09, Z2-I-STREAM-10, Z2-I-STREAM-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-041
- **Budgets:** none
- **Acceptance evidence:** `ForbiddenCheck` and the lock-order checker over `Zenith2/Engine/ZenithStreaming/`.
- **Design:** Zenith2/Docs/Design/STREAM.md → "Lock-order entries"; Zenith2/Docs/Design/STREAM.md → "Justification records"

### Z2-R-STREAM-151 Cancel during SCENE decoding

- **Statement:** Unloading a cell while SCENE decodes its already-decompressed sections (`DECODING`) releases the partial `Zenith_SceneData` arena and the scene handle at the next chunk boundary.
- **Covers:** Z2-I-STREAM-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Streaming_CancelReload1000Cycles_NoLeak` (stage SCENE `DECODING`, held by SCENE's preparation hold of row ST2 (h), hit count above 0) (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Cancellation at every stage"; Zenith2/Docs/Design/STREAM/Tests.md → "Assumed test support of other areas"

### Z2-R-STREAM-152 Cancel during dependency activation

- **Statement:** Unloading a cell while a CPU-kind hard dependency is in ASSET's `DELIVER` or `ACTIVATING` makes ASSET call the kind loader's `Cancel`, which releases its partial object and acknowledges, leaving no object of the dependency behind.
- **Covers:** Z2-I-STREAM-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_Streaming_CancelReload1000Cycles_NoLeak` (stage dependency activation, held by ASSET's stage hold of row ST20, hit count above 0) (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Cancellation at every stage"

### Z2-R-STREAM-153 Cancellation with a live booster

- **Statement:** A cancellation while the service's booster or a prefetch of the cell scene is alive leaves the same residue as one without: the booster is destroyed in the frame the cell leaves LOADING or CANCELLING and the scene asset unloads at zero references.
- **Covers:** Z2-I-STREAM-04, Z2-I-STREAM-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Streaming_CancelReload1000Cycles_NoLeak` (every stage run with and without a booster) (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Cancellation at every stage"; Zenith2/Docs/Design/STREAM/Runtime.md → "Sub-steps, in order"

### Z2-R-STREAM-154 External unload and destroyed anchor

- **Statement:** A cell instance unloaded by other code, or an ACTIVE cell whose anchor root gameplay destroyed, is released through T24 to T28: the cell becomes UNLOADED, its delta keeps every earlier record and the activation's tombstones, the event is counted, and the cell reloads when wanted.
- **Covers:** Z2-I-STREAM-02, Z2-I-STREAM-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingCellMachine_RowT24` to `Test_StreamingCellMachine_RowT28`, `Test_StreamingIntegration_ExternalUnloadAndDestroyedAnchor` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Transitions"; Zenith2/Docs/Design/STREAM/Runtime.md → "Totality"

### Z2-R-STREAM-155 Unexpected instance states released

- **Statement:** An observed instance state that no other row allows for the cell's state releases the instance, returns the cell to UNLOADED and is counted and logged once per cell per session.
- **Covers:** Z2-I-STREAM-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingCellMachine_RowT34`, `Test_StreamingCellMachine_TableIsTotal` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Transitions"; Zenith2/Docs/Design/STREAM/Runtime.md → "Totality"

### Z2-R-STREAM-156 No load in a release tick

- **Statement:** A cell whose unload or release is recorded in a tick is not a load candidate in that tick, so STREAM never meets its own previous instance at commit.
- **Covers:** Z2-I-STREAM-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingCellMachine_NoLoadInReleaseTick` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Loads (step 9)"; Zenith2/Docs/Design/STREAM/Runtime.md → "Failure classification"

### Z2-R-STREAM-157 Discarded publication recorded

- **Statement:** Every tagged load unloaded while LOADING, whatever its operation's outcome and including a pending preparation removed before it started, yields exactly one discarded publication logged as `PUBLICATION_KIND_GENERIC_RECORDED` with `SCENE_ERROR_CANCELLED`, delivered to `OnDiscardedPublication` before SCENE releases the record.
- **Covers:** Z2-I-STREAM-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** SCENE's `Test_AdditiveScenes_DiscardResolverRecordsCancelled` (registered by the row ST2 patch); STREAM's `Test_StreamingCellMachine_RowT18`, `Test_StreamingDeterminism_CancelledLoadsReplay` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/PublicAPI.md → "SCENE activation hook"; Zenith2/Docs/Design/STREAM/Runtime.md → "Replay and determinism"

### Z2-R-STREAM-158 Cancelled loads replay exactly

- **Statement:** A recorded run of the 1,000-cycle cancellation stress replays at 1 and all workers with equal per-tick world hashes, every discarded publication at its recorded tick, and no replay tick held by a cancelled preparation.
- **Covers:** Z2-I-STREAM-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingDeterminism_CancelledLoadsReplay` (`Test`, `win-release`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Replay and determinism"; Zenith2/Docs/Design/STREAM/Tests.md → "Stress and determinism tests"

### Z2-R-STREAM-159 Hook captured per load

- **Statement:** Each committed tagged load calls the hook pointer captured at its commit for all its subsequent calls, so clearing the world's hook at detach or destroying the world never races a running preparation, and STREAM keeps every captured hook alive until its slots are FREE.
- **Covers:** Z2-I-STREAM-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingSnapshotPool_RetiredUntilFree`, `Test_StreamingIntegration_WorldTransitionDiscardsInFlight` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/PublicAPI.md → "SCENE activation hook"; Zenith2/Docs/Design/STREAM/PublicAPI.md → "Zenith_StreamingActivationHook"

### Z2-R-STREAM-160 Content-stable cell identity

- **Statement:** Delta holders, anchors and persistent markers name their cell by the cell scene's persistent ID; a re-cook that inserts or removes cells loses no saved destruction or state of any remaining cell.
- **Covers:** Z2-I-STREAM-07, Z2-I-STREAM-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingPersistence_DeltaSurvivesRecookWithInsertedCell` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Cell identity across re-cooks"; Zenith2/Docs/Design/STREAM/PublicAPI.md → "Zenith_StreamingComponents header"

### Z2-R-STREAM-161 Re-cooked cells removed or added

- **Statement:** A delta of a cell a re-cook removed is kept, never applied and counted, with every record whose entity now belongs to another cell re-homed there by the update's step 14; an added cell reloads authored; a record of an entity moved to another cell is re-homed to it by step 14 (deferred while that cell is live or in flight, surviving captures; Z2-R-STREAM-175), and only a record whose entity belongs to no cell is dropped and counted.
- **Covers:** Z2-I-STREAM-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingPersistence_RecookRemovedCellKeepsOrphanDelta`, `Test_StreamingPersistence_RecookMovedEntityRehomed` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Cell identity across re-cooks"; Zenith2/Docs/Design/STREAM/Formats.md → "Decoder checks"

### Z2-R-STREAM-162 Component presence persisted

- **Statement:** A STATE record states every configured type as present or absent, and at reload SCENE's amendment adds runtime-added components (with their captured values) and removes runtime-removed ones before the entity exists, so no tick sees the authored component set.
- **Covers:** Z2-I-STREAM-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingPersistence_ComponentAddedAndRemovedSurvive`, `Test_StreamingPersistence_UnconfiguredTypeKeepsAuthoredPresence` (`Test`), once row ST2 (e) lands.
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Component presence"; Zenith2/Docs/Design/STREAM/Formats.md → "Entity records"

### Z2-R-STREAM-163 HLOD scales with its driving source

- **Statement:** The cut is driven by the first `DrivesHLOD` source, else the primary, else the lowest-ID source, with every HLOD distance multiplied by that source's radius scale, so a scaled source leaves no uncovered ring.
- **Covers:** Z2-I-STREAM-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingHLOD_DrivingSourceSelection`, `Test_StreamingHLOD_ScaledSourceLeavesNoUncoveredRing` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Driving source and scaling"

### Z2-R-STREAM-164 Precision target

- **Statement:** STREAM's large-coordinate numbers derive from CORE's 0.25 mm convention: the precision radius is 4,096 m, the rebase defaults keep the primary within 2,198 m at STREAM's design maximum traversal speed of 300 m/s, and ACTIVE cells beyond the radius are reported by `stream.precision.cells_beyond_radius`.
- **Covers:** Z2-I-STREAM-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** Z2-B-SHIP-016
- **Acceptance evidence:** `Test_StreamingRebase_PrecisionGauge`, `Test_StreamingIntegration_RebaseLongJourney` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Precision target"; Zenith2/Docs/Design/STREAM/Runtime.md → "Rebase rule"

### Z2-R-STREAM-165 Bindings read the shifted origin

- **Statement:** In a rebase tick, the read-world bindings called in phases 2 to 10 convert local positions with the shifted origin and return it.
- **Covers:** Z2-I-STREAM-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingBindings_ReadWorld` (case "rebase tick, phase 5") (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/PublicAPI.md → "Script bindings"; Zenith2/Docs/Design/STREAM/Runtime.md → "Who reacts to a rebase"

### Z2-R-STREAM-166 Required-load double residency

- **Statement:** A simulation-required load under a displayed proxy reserves its double residency without deferring and holds it for the whole load; a refusal releases the covering proxy at once, so Z2-B-CORE-006 is never exceeded and the load never waits.
- **Covers:** Z2-I-STREAM-02, Z2-I-STREAM-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-CORE-006
- **Acceptance evidence:** `Test_StreamingService_RequiredLoadReservesDoubleResidency`, `Test_StreamingService_RequiredLoadRefusedReleasesProxy` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Double-residency reservation"

### Z2-R-STREAM-167 Save fidelity with cells in flight

- **Statement:** A capture taken with any cell LOADING, QUEUED or CANCELLING, or with a tagged commit refused at a sync point of its tick, is COHERENT and its continuation converges to the uninterrupted run's cell set and persistent values; every other capture is EXACT for STREAM, whatever its failed, permanent, delayed, preempted, overridden, tombstoned or region-bound state.
- **Covers:** Z2-I-STREAM-07, Z2-I-STREAM-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingIntegration_SaveDuringStreaming`, `Test_StreamingIntegration_RefusedCommitCaptureIsCoherent`, `Test_StreamingIntegration_RestoreRejectedActivationAdopts`, `Test_StreamingIntegration_ExactRestoreWithFailedAndDelayedCells` (`Test`); SAVE's `Test_SaveCrossSystem_LoadingInstanceReRequestedWithHookTag`.
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "What is saved"

### Z2-R-STREAM-168 Discard kind resolved at publication

- **Statement:** The kind and payload of a cancelled tagged load's publication are decided at the tick boundary by SCENE's publication resolver from the record's "discard requested" flag, before SCRIPT logs it, so a result parked before the unload's playback, after it or never is logged identically as `PUBLICATION_KIND_GENERIC_RECORDED` `{instance, SCENE_ERROR_CANCELLED, 0}`, and no member asserts on a parked entry.
- **Covers:** Z2-I-STREAM-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingDeterminism_DiscardResolvedAtPublication` (`Test`, `win-release`); SCENE's `Test_AdditiveScenes_DiscardResolverRecordsCancelled` (registered by the row ST2 patch).
- **Design:** Zenith2/Docs/Design/STREAM/PublicAPI.md → "SCENE activation hook"; Zenith2/Docs/Design/STREAM/PublicAPI.md → "SCRIPT publication resolver"; Zenith2/Docs/Design/STREAM/Runtime.md → "Replay and determinism"

### Z2-R-STREAM-169 Replay drops a late local discard

- **Statement:** Under a publication source a recorded discard publishes at its logged tick without holding the tick, and the replay's own local completion of that ID, arriving before or after it, is dropped without an assertion, its prepared scene released and its slot freed, with every per-tick hash equal to the recording's.
- **Covers:** Z2-I-STREAM-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingDeterminism_ReplayReleasesLateLocalDiscard` (`Test`, `win-release`).
- **Design:** Zenith2/Docs/Design/STREAM/PublicAPI.md → "SCRIPT publication resolver"; Zenith2/Docs/Design/STREAM/Runtime.md → "Replay and determinism"; Zenith2/Docs/Design/STREAM/Runtime.md → "Completion reporting"

### Z2-R-STREAM-170 ASSET causes of unavailable content classified

- **Statement:** A `SCENE_ERROR_ASSET_UNAVAILABLE` failure is classified by the ASSET error code SCENE carries as its detail: capacity codes never consume a retry, `ASSET_ERROR_IO`, `NOT_INSTALLED`, an unknown dependency cause and a missing detail consume none for up to `stream.cell.maxTransientRetries` consecutive failures, and every other ASSET code is a content failure; the classification is total over every SCENE and ASSET code.
- **Covers:** Z2-I-STREAM-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingCellMachine_AssetCapacityFailureDoesNotConsumeRetry`, `Test_StreamingCellMachine_AssetTransientFailureIsBounded`, `Test_StreamingCellMachine_AssetDetailClassifiesEveryCode` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Failure classification"; Zenith2/Docs/Design/STREAM/PublicAPI.md → "SCENE activation hook"; Zenith2/Docs/Design/STREAM/PublicAPI.md → "ASSET dependency root cause"

### Z2-R-STREAM-171 Cell status persisted on its holder

- **Statement:** A cell's holder is spawned in the buffer of the first load the cell is issued without one (or by a status-holder action for a catalogue miss), and is mapped in step 3 of the next update before the commit-refused and discard inboxes are drained, so a refusal of that first load's commit is written into it; every persisted field of the cell's record (state, instance, failure counters, retry and unload due ticks, flags, committed and pending tombstones, delta merge pending) equals its `Zenith.Streaming.CellStatus` at the end of every step, written directly, never by a rejectable command; a holder gameplay destroys is re-spawned.
- **Covers:** Z2-I-STREAM-07, Z2-I-STREAM-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingPersistence_StatusPersistedOnHolder`, `Test_StreamingPersistence_HolderSpawnedWithFirstLoad` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Persisted cell status"; Zenith2/Docs/Design/STREAM/PublicAPI.md → "Zenith_StreamingComponents header"; Zenith2/Docs/Design/STREAM/Runtime.md → "Action buffers and rejection"

### Z2-R-STREAM-172 Exact restore of streaming state

- **Statement:** A replay resumed with `Zenith_RuntimeReplay_ResumeFromSave` from a save of a recorded run taken with no cell LOADING, QUEUED or CANCELLING and no tagged commit refused in its tick reproduces the recording's per-tick world hashes (`m_bHashesCompared` and `m_bVerified` true) through 600 ticks in which loads are issued and published, with cells FAILED mid-backoff, FAILED_PERMANENT, in their unload delay, preempted or overridden, with pending and committed tombstones, region-bound DESTROY entities at region edges, moving sources and a pending teleport, on each ECS backend; a live continuation is claimed equal to an uninterrupted live run only while no asynchronous request is in flight at the capture or issued in the compared window.
- **Covers:** Z2-I-STREAM-07, Z2-I-STREAM-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingIntegration_ExactRestoreWithFailedAndDelayedCells` (`Test`, `win-release`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "What is saved"; Zenith2/Docs/Design/STREAM/Runtime.md → "Rebuild"; Zenith2/Docs/Design/STREAM/Runtime.md → "Persisted cell status"

### Z2-R-STREAM-173 Source, region and reference state persisted

- **Statement:** A source's previous absolute position, its tick and a pending teleport, a region-bound entity's flag, containing cell and evaluation tick, and the world reference's binding state and override slots are serialized `PHASE_INPUT` or command-written properties of their own components, written in the step or playback that changes them, so a restore infers no teleport, destroys no entity and drops no override or binding the uninterrupted run would not.
- **Covers:** Z2-I-STREAM-07, Z2-I-STREAM-05, Z2-I-STREAM-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingSources_PreviousPositionPersisted`, `Test_StreamingRegions_DestroyNeedsBoundPreviousEvaluation`, `Test_StreamingBands_OverridesPersistOnWorldReference` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Teleports"; Zenith2/Docs/Design/STREAM/Runtime.md → "The region"; Zenith2/Docs/Design/STREAM/Runtime.md → "Overrides on the world reference"; Zenith2/Docs/Design/STREAM/Runtime.md → "World binding"

### Z2-R-STREAM-174 Unadopted restored loads release their slot

- **Statement:** Every restored in-flight cell load the rebuild does not adopt that holds a snapshot slot has the slot neutralised before its release (a zero-filled entity table, the gate open, the simulation bit released, the slot's cell `uSTREAMING_INVALID_CELL`) and the restored-load inbox cleared at the end of R4, so the cancelled preparation reaches its end, the slot becomes FREE, and its discarded publication is counted and ignored, never appended to the discard inbox; a cell whose persisted ACTIVATING or UNLOADING state meets a LOADING or QUEUED instance is made UNLOADED so its re-requested load is adopted.
- **Covers:** Z2-I-STREAM-04, Z2-I-STREAM-07, Z2-I-STREAM-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingIntegration_RestoreRejectedActivationAdopts`, `Test_StreamingIntegration_RestoreBeyondPoolReleases` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Rebuild"; Zenith2/Docs/Design/STREAM/Runtime.md → "Restored loads beyond the pool"; Zenith2/Docs/Design/STREAM/PublicAPI.md → "Zenith_StreamingActivationHook"

### Z2-R-STREAM-175 Deferred re-homes run every tick

- **Statement:** A holder carrying a delta record to re-home carries `CellStatus` flag bit 6, set by the rebuild, the binding's marking pass and a capture that carries such a record, and the update's step 14 re-homes its records, at most `stream.unload.maxCellsPerTick` holders per tick in a visiting order that is a function of the tick and the persisted bits, into each target cell once it is UNLOADED, FAILED or FAILED_PERMANENT, after every other action of the tick; neither the rebuild nor the binding moves a record, so an uninterrupted run and a replay resumed from an EXACT capture record the same re-homes at the same ticks.
- **Covers:** Z2-I-STREAM-07, Z2-I-STREAM-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamingPersistence_RecookMovedEntityRehomed` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Cell identity across re-cooks"; Zenith2/Docs/Design/STREAM/Runtime.md → "Capture"; Zenith2/Docs/Design/STREAM/Formats.md → "Decoder checks"

### Z2-R-STREAM-176 Holders of default status are pruned

- **Statement:** In step 14, a cell that entered UNLOADED in the tick and still is, whose holder holds the default status apart from its two ticks and an empty delta and is written by no other action of the tick, gets a holder-prune action; the next update resolves it from the holder's validity, a rejected prune is not retried until the cell next enters UNLOADED, and holders therefore number at most the cells with non-default status or a non-empty delta, plus the cells whose last prune was rejected, plus orphans.
- **Covers:** Z2-I-STREAM-07, Z2-I-STREAM-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-038
- **Budgets:** Z2-B-STREAM-001
- **Acceptance evidence:** `Test_StreamingPersistence_DefaultHolderPruned` (`Test`).
- **Design:** Zenith2/Docs/Design/STREAM/Runtime.md → "Persisted cell status"; Zenith2/Docs/Design/STREAM/Runtime.md → "Complexity"

## Coverage of inventory items

| Item | Text | Requirements |
|---|---|---|
| Z2-I-STREAM-01 | spatial partitioning | Z2-R-STREAM-001, Z2-R-STREAM-002, Z2-R-STREAM-003, Z2-R-STREAM-004, Z2-R-STREAM-005, Z2-R-STREAM-006, Z2-R-STREAM-007, Z2-R-STREAM-008, Z2-R-STREAM-009, Z2-R-STREAM-010, Z2-R-STREAM-011, Z2-R-STREAM-012, Z2-R-STREAM-013, Z2-R-STREAM-014, Z2-R-STREAM-015, Z2-R-STREAM-016, Z2-R-STREAM-017, Z2-R-STREAM-018, Z2-R-STREAM-019, Z2-R-STREAM-020, Z2-R-STREAM-021, Z2-R-STREAM-022, Z2-R-STREAM-023, Z2-R-STREAM-024, Z2-R-STREAM-025, Z2-R-STREAM-026, Z2-R-STREAM-027, Z2-R-STREAM-028, Z2-R-STREAM-029, Z2-R-STREAM-030, Z2-R-STREAM-031, Z2-R-STREAM-032, Z2-R-STREAM-033, Z2-R-STREAM-034, Z2-R-STREAM-035, Z2-R-STREAM-036, Z2-R-STREAM-037, Z2-R-STREAM-038, Z2-R-STREAM-039, Z2-R-STREAM-041, Z2-R-STREAM-044, Z2-R-STREAM-103, Z2-R-STREAM-124, Z2-R-STREAM-125, Z2-R-STREAM-126, Z2-R-STREAM-132, Z2-R-STREAM-133, Z2-R-STREAM-134, Z2-R-STREAM-135, Z2-R-STREAM-136, Z2-R-STREAM-137, Z2-R-STREAM-140, Z2-R-STREAM-141, Z2-R-STREAM-146, Z2-R-STREAM-147, Z2-R-STREAM-148, Z2-R-STREAM-150, Z2-R-STREAM-160, Z2-R-STREAM-175 |
| Z2-I-STREAM-02 | async cell loading | Z2-R-STREAM-001, Z2-R-STREAM-002, Z2-R-STREAM-003, Z2-R-STREAM-004, Z2-R-STREAM-005, Z2-R-STREAM-006, Z2-R-STREAM-007, Z2-R-STREAM-008, Z2-R-STREAM-009, Z2-R-STREAM-030, Z2-R-STREAM-032, Z2-R-STREAM-033, Z2-R-STREAM-034, Z2-R-STREAM-036, Z2-R-STREAM-037, Z2-R-STREAM-042, Z2-R-STREAM-043, Z2-R-STREAM-045, Z2-R-STREAM-046, Z2-R-STREAM-048, Z2-R-STREAM-049, Z2-R-STREAM-050, Z2-R-STREAM-051, Z2-R-STREAM-052, Z2-R-STREAM-054, Z2-R-STREAM-055, Z2-R-STREAM-056, Z2-R-STREAM-057, Z2-R-STREAM-058, Z2-R-STREAM-059, Z2-R-STREAM-060, Z2-R-STREAM-061, Z2-R-STREAM-062, Z2-R-STREAM-063, Z2-R-STREAM-073, Z2-R-STREAM-075, Z2-R-STREAM-077, Z2-R-STREAM-079, Z2-R-STREAM-103, Z2-R-STREAM-110, Z2-R-STREAM-111, Z2-R-STREAM-112, Z2-R-STREAM-113, Z2-R-STREAM-119, Z2-R-STREAM-120, Z2-R-STREAM-121, Z2-R-STREAM-122, Z2-R-STREAM-123, Z2-R-STREAM-124, Z2-R-STREAM-125, Z2-R-STREAM-126, Z2-R-STREAM-127, Z2-R-STREAM-128, Z2-R-STREAM-130, Z2-R-STREAM-132, Z2-R-STREAM-133, Z2-R-STREAM-134, Z2-R-STREAM-135, Z2-R-STREAM-136, Z2-R-STREAM-137, Z2-R-STREAM-138, Z2-R-STREAM-139, Z2-R-STREAM-140, Z2-R-STREAM-141, Z2-R-STREAM-142, Z2-R-STREAM-143, Z2-R-STREAM-144, Z2-R-STREAM-145, Z2-R-STREAM-146, Z2-R-STREAM-147, Z2-R-STREAM-148, Z2-R-STREAM-150, Z2-R-STREAM-154, Z2-R-STREAM-155, Z2-R-STREAM-156, Z2-R-STREAM-166, Z2-R-STREAM-167, Z2-R-STREAM-170, Z2-R-STREAM-171, Z2-R-STREAM-172, Z2-R-STREAM-174, Z2-R-STREAM-176 |
| Z2-I-STREAM-03 | priorities (of cell loads) | Z2-R-STREAM-001, Z2-R-STREAM-002, Z2-R-STREAM-003, Z2-R-STREAM-004, Z2-R-STREAM-005, Z2-R-STREAM-006, Z2-R-STREAM-007, Z2-R-STREAM-008, Z2-R-STREAM-009, Z2-R-STREAM-013, Z2-R-STREAM-036, Z2-R-STREAM-037, Z2-R-STREAM-038, Z2-R-STREAM-039, Z2-R-STREAM-040, Z2-R-STREAM-041, Z2-R-STREAM-042, Z2-R-STREAM-043, Z2-R-STREAM-047, Z2-R-STREAM-048, Z2-R-STREAM-049, Z2-R-STREAM-050, Z2-R-STREAM-051, Z2-R-STREAM-052, Z2-R-STREAM-053, Z2-R-STREAM-054, Z2-R-STREAM-063, Z2-R-STREAM-110, Z2-R-STREAM-111, Z2-R-STREAM-113, Z2-R-STREAM-124, Z2-R-STREAM-125, Z2-R-STREAM-126, Z2-R-STREAM-132, Z2-R-STREAM-133, Z2-R-STREAM-134, Z2-R-STREAM-135, Z2-R-STREAM-136, Z2-R-STREAM-137, Z2-R-STREAM-140, Z2-R-STREAM-141, Z2-R-STREAM-143, Z2-R-STREAM-144, Z2-R-STREAM-145, Z2-R-STREAM-146, Z2-R-STREAM-147, Z2-R-STREAM-148, Z2-R-STREAM-150, Z2-R-STREAM-153 |
| Z2-I-STREAM-04 | cancellation (of cell loads) | Z2-R-STREAM-001, Z2-R-STREAM-002, Z2-R-STREAM-003, Z2-R-STREAM-004, Z2-R-STREAM-005, Z2-R-STREAM-006, Z2-R-STREAM-007, Z2-R-STREAM-008, Z2-R-STREAM-009, Z2-R-STREAM-030, Z2-R-STREAM-036, Z2-R-STREAM-037, Z2-R-STREAM-056, Z2-R-STREAM-061, Z2-R-STREAM-062, Z2-R-STREAM-064, Z2-R-STREAM-065, Z2-R-STREAM-066, Z2-R-STREAM-067, Z2-R-STREAM-068, Z2-R-STREAM-069, Z2-R-STREAM-070, Z2-R-STREAM-071, Z2-R-STREAM-072, Z2-R-STREAM-073, Z2-R-STREAM-074, Z2-R-STREAM-075, Z2-R-STREAM-076, Z2-R-STREAM-078, Z2-R-STREAM-079, Z2-R-STREAM-124, Z2-R-STREAM-125, Z2-R-STREAM-126, Z2-R-STREAM-129, Z2-R-STREAM-130, Z2-R-STREAM-131, Z2-R-STREAM-132, Z2-R-STREAM-133, Z2-R-STREAM-134, Z2-R-STREAM-135, Z2-R-STREAM-136, Z2-R-STREAM-137, Z2-R-STREAM-140, Z2-R-STREAM-141, Z2-R-STREAM-145, Z2-R-STREAM-147, Z2-R-STREAM-148, Z2-R-STREAM-150, Z2-R-STREAM-151, Z2-R-STREAM-152, Z2-R-STREAM-153, Z2-R-STREAM-157, Z2-R-STREAM-158, Z2-R-STREAM-159, Z2-R-STREAM-168, Z2-R-STREAM-169, Z2-R-STREAM-174 |
| Z2-I-STREAM-05 | large-coordinate handling | Z2-R-STREAM-001, Z2-R-STREAM-002, Z2-R-STREAM-003, Z2-R-STREAM-004, Z2-R-STREAM-005, Z2-R-STREAM-006, Z2-R-STREAM-007, Z2-R-STREAM-008, Z2-R-STREAM-009, Z2-R-STREAM-012, Z2-R-STREAM-023, Z2-R-STREAM-036, Z2-R-STREAM-037, Z2-R-STREAM-094, Z2-R-STREAM-104, Z2-R-STREAM-105, Z2-R-STREAM-106, Z2-R-STREAM-107, Z2-R-STREAM-108, Z2-R-STREAM-109, Z2-R-STREAM-124, Z2-R-STREAM-125, Z2-R-STREAM-126, Z2-R-STREAM-132, Z2-R-STREAM-133, Z2-R-STREAM-134, Z2-R-STREAM-135, Z2-R-STREAM-136, Z2-R-STREAM-137, Z2-R-STREAM-138, Z2-R-STREAM-140, Z2-R-STREAM-141, Z2-R-STREAM-147, Z2-R-STREAM-148, Z2-R-STREAM-150, Z2-R-STREAM-164, Z2-R-STREAM-165, Z2-R-STREAM-173 |
| Z2-I-STREAM-06 | cross-cell references | Z2-R-STREAM-001, Z2-R-STREAM-002, Z2-R-STREAM-003, Z2-R-STREAM-004, Z2-R-STREAM-005, Z2-R-STREAM-006, Z2-R-STREAM-007, Z2-R-STREAM-008, Z2-R-STREAM-009, Z2-R-STREAM-018, Z2-R-STREAM-020, Z2-R-STREAM-021, Z2-R-STREAM-026, Z2-R-STREAM-036, Z2-R-STREAM-037, Z2-R-STREAM-080, Z2-R-STREAM-081, Z2-R-STREAM-082, Z2-R-STREAM-083, Z2-R-STREAM-084, Z2-R-STREAM-124, Z2-R-STREAM-125, Z2-R-STREAM-126, Z2-R-STREAM-132, Z2-R-STREAM-133, Z2-R-STREAM-134, Z2-R-STREAM-135, Z2-R-STREAM-136, Z2-R-STREAM-137, Z2-R-STREAM-140, Z2-R-STREAM-141, Z2-R-STREAM-147, Z2-R-STREAM-148, Z2-R-STREAM-150 |
| Z2-I-STREAM-07 | persistent entities | Z2-R-STREAM-001, Z2-R-STREAM-002, Z2-R-STREAM-003, Z2-R-STREAM-004, Z2-R-STREAM-005, Z2-R-STREAM-006, Z2-R-STREAM-007, Z2-R-STREAM-008, Z2-R-STREAM-009, Z2-R-STREAM-020, Z2-R-STREAM-021, Z2-R-STREAM-022, Z2-R-STREAM-026, Z2-R-STREAM-029, Z2-R-STREAM-034, Z2-R-STREAM-036, Z2-R-STREAM-037, Z2-R-STREAM-061, Z2-R-STREAM-075, Z2-R-STREAM-077, Z2-R-STREAM-079, Z2-R-STREAM-085, Z2-R-STREAM-086, Z2-R-STREAM-087, Z2-R-STREAM-088, Z2-R-STREAM-089, Z2-R-STREAM-090, Z2-R-STREAM-091, Z2-R-STREAM-092, Z2-R-STREAM-093, Z2-R-STREAM-094, Z2-R-STREAM-095, Z2-R-STREAM-096, Z2-R-STREAM-124, Z2-R-STREAM-125, Z2-R-STREAM-126, Z2-R-STREAM-127, Z2-R-STREAM-128, Z2-R-STREAM-129, Z2-R-STREAM-132, Z2-R-STREAM-133, Z2-R-STREAM-134, Z2-R-STREAM-135, Z2-R-STREAM-136, Z2-R-STREAM-137, Z2-R-STREAM-140, Z2-R-STREAM-141, Z2-R-STREAM-147, Z2-R-STREAM-148, Z2-R-STREAM-150, Z2-R-STREAM-154, Z2-R-STREAM-160, Z2-R-STREAM-161, Z2-R-STREAM-162, Z2-R-STREAM-167, Z2-R-STREAM-171, Z2-R-STREAM-172, Z2-R-STREAM-173, Z2-R-STREAM-174, Z2-R-STREAM-175, Z2-R-STREAM-176 |
| Z2-I-STREAM-08 | HLOD | Z2-R-STREAM-001, Z2-R-STREAM-002, Z2-R-STREAM-003, Z2-R-STREAM-004, Z2-R-STREAM-005, Z2-R-STREAM-006, Z2-R-STREAM-007, Z2-R-STREAM-008, Z2-R-STREAM-009, Z2-R-STREAM-018, Z2-R-STREAM-020, Z2-R-STREAM-021, Z2-R-STREAM-026, Z2-R-STREAM-036, Z2-R-STREAM-037, Z2-R-STREAM-109, Z2-R-STREAM-113, Z2-R-STREAM-114, Z2-R-STREAM-115, Z2-R-STREAM-116, Z2-R-STREAM-117, Z2-R-STREAM-118, Z2-R-STREAM-121, Z2-R-STREAM-122, Z2-R-STREAM-123, Z2-R-STREAM-124, Z2-R-STREAM-125, Z2-R-STREAM-126, Z2-R-STREAM-132, Z2-R-STREAM-133, Z2-R-STREAM-134, Z2-R-STREAM-135, Z2-R-STREAM-136, Z2-R-STREAM-137, Z2-R-STREAM-140, Z2-R-STREAM-141, Z2-R-STREAM-147, Z2-R-STREAM-148, Z2-R-STREAM-149, Z2-R-STREAM-150, Z2-R-STREAM-163, Z2-R-STREAM-166 |
| Z2-I-STREAM-09 | coordinated physics streaming | Z2-R-STREAM-001, Z2-R-STREAM-002, Z2-R-STREAM-003, Z2-R-STREAM-004, Z2-R-STREAM-005, Z2-R-STREAM-006, Z2-R-STREAM-007, Z2-R-STREAM-008, Z2-R-STREAM-009, Z2-R-STREAM-013, Z2-R-STREAM-014, Z2-R-STREAM-015, Z2-R-STREAM-036, Z2-R-STREAM-037, Z2-R-STREAM-044, Z2-R-STREAM-097, Z2-R-STREAM-098, Z2-R-STREAM-099, Z2-R-STREAM-102, Z2-R-STREAM-124, Z2-R-STREAM-125, Z2-R-STREAM-126, Z2-R-STREAM-132, Z2-R-STREAM-133, Z2-R-STREAM-134, Z2-R-STREAM-135, Z2-R-STREAM-136, Z2-R-STREAM-137, Z2-R-STREAM-140, Z2-R-STREAM-141, Z2-R-STREAM-147, Z2-R-STREAM-148, Z2-R-STREAM-150, Z2-R-STREAM-173 |
| Z2-I-STREAM-10 | coordinated navigation streaming | Z2-R-STREAM-001, Z2-R-STREAM-002, Z2-R-STREAM-003, Z2-R-STREAM-004, Z2-R-STREAM-005, Z2-R-STREAM-006, Z2-R-STREAM-007, Z2-R-STREAM-008, Z2-R-STREAM-009, Z2-R-STREAM-013, Z2-R-STREAM-014, Z2-R-STREAM-015, Z2-R-STREAM-036, Z2-R-STREAM-037, Z2-R-STREAM-044, Z2-R-STREAM-097, Z2-R-STREAM-098, Z2-R-STREAM-100, Z2-R-STREAM-102, Z2-R-STREAM-124, Z2-R-STREAM-125, Z2-R-STREAM-126, Z2-R-STREAM-132, Z2-R-STREAM-133, Z2-R-STREAM-134, Z2-R-STREAM-135, Z2-R-STREAM-136, Z2-R-STREAM-137, Z2-R-STREAM-140, Z2-R-STREAM-141, Z2-R-STREAM-147, Z2-R-STREAM-148, Z2-R-STREAM-150 |
| Z2-I-STREAM-11 | coordinated audio streaming | Z2-R-STREAM-001, Z2-R-STREAM-002, Z2-R-STREAM-003, Z2-R-STREAM-004, Z2-R-STREAM-005, Z2-R-STREAM-006, Z2-R-STREAM-007, Z2-R-STREAM-008, Z2-R-STREAM-009, Z2-R-STREAM-013, Z2-R-STREAM-015, Z2-R-STREAM-036, Z2-R-STREAM-037, Z2-R-STREAM-097, Z2-R-STREAM-098, Z2-R-STREAM-101, Z2-R-STREAM-102, Z2-R-STREAM-124, Z2-R-STREAM-125, Z2-R-STREAM-126, Z2-R-STREAM-132, Z2-R-STREAM-133, Z2-R-STREAM-134, Z2-R-STREAM-135, Z2-R-STREAM-136, Z2-R-STREAM-137, Z2-R-STREAM-140, Z2-R-STREAM-141, Z2-R-STREAM-147, Z2-R-STREAM-148, Z2-R-STREAM-150 |

## Excluded and deferred

Every STREAM inventory item is classified required at Stage 4; no STREAM requirement is deferred, excluded or game-side. STREAM's budget rows are first confirmed by measurement at Stage 4: the Stage 3 W-Outdoor run does not select the Streaming module (core-doc item X229). Z2-R-STREAM-141 is the Stage 1 budget producer. The parts of "Stage 3 sprint pull-forward" (the API slices, the format, reader, writer, fuzz target, document, cook step, kind loader and the pure band functions) are scheduled in the Stage 3 sprint but serve the Stage 4 acceptance, so their requirements are staged S4. Navigation coordination's evidence waits on D-SEL-NAV (open, Stage 1/2) without changing its stage. The boundaries that keep the area bounded are exclusions of other capabilities from STREAM:

| Not in STREAM | Rationale | Decision or owner |
|---|---|---|
| mod-supplied cells or streaming worlds | mods are excluded from Zenith2's scope | Z2-D-075 |
| dynamic plugins registering streaming kinds | modules are static and selected in `game.json` | Z2-D-024 |
| networked interest management and relevancy | NET's, Stage 7 | Z2-D-073 |
| virtualized geometry and virtual texturing for distant content | ADVGFX's; HLOD proxies are ordinary meshes | Z2-D-043 |
| texture streaming and the reallocate-and-swap mechanism | GEOM's policy and RHI's `Flux_StreamingSwap` | GEOM, RHI (`Zenith2/Docs/Design/GEOM/Textures.md` → "Reallocate-and-swap realisation") |
| the asset loader, residency, deduplication and development reload | ASSET's | ASSET (`Zenith2/Docs/Design/ASSET/Loading.md` → "Publication to simulation") |
| world partition authoring, data layers, the partition and HLOD bakes | WORLD's; STREAM owns the runtime contract they write to | WORLD (Z2-I-WORLD-01) |
| streaming decisions from wall-clock time, I/O timing, memory pressure or the render camera | they would break determinism | Z2-D-019 |
