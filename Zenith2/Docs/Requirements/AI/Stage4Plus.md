# AI requirement catalogue: Stage 4 module, navigation and agents

## Purpose

The second file of the AI catalogue: the requirements Z2-R-AI-007 … Z2-R-AI-096 (all Stage 4: the module, formats, bake and cook, runtime tiles and streaming, obstacles, links, the search and the path service, agents, avoidance and locomotion), in the same format and the same `Z2-R-AI-NNN` numbering as `Zenith2/Docs/Requirements/AI.md`, which holds the purpose of the whole catalogue, the Stage 1 and Stage 2 requirements, the coverage of inventory items (over all three files) and the exclusions. The requirements from Z2-R-AI-097 onward are `Zenith2/Docs/Requirements/AI/Stage4Systems.md`.

## Requirements

### Z2-R-AI-007 ZenithAI library target

- **Statement:** ZenithAI must be one KIND MODULE static library with MODULE_NAME AI and module dependency Physics, declaring exactly the DEPENDS list of "Target summary", SUPPORTED ZENITH_GPU, with every public header in its root and every source under Internal/.
- **Covers:** Z2-I-AI-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-024, Z2-D-001
- **Budgets:** none
- **Acceptance evidence:** The Configure, Build, SourceLayout, CompileCoverage and Libgraph steps for the ZenithAI target.
- **Design:** Zenith2/Docs/Design/AI.md → "Target summary"; Zenith2/Docs/Design/AI.md → "ZenithAI files"; Zenith2/Docs/Design/AI.md → "ZENITH_SUPPORTED matrix"

### Z2-R-AI-008 Module entry points

- **Statement:** Zenith_Module_AI.h must declare exactly Zenith_Module_AI_RegisterTypes and Zenith_Module_AI_Start, and RegisterTypes must register every component, event, struct, resource and configuration key of the module and no document type, which registers only through the tool-side Zenith_RegisterAIDocumentTypes.
- **Covers:** Z2-I-AI-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-024, Z2-D-047
- **Budgets:** none
- **Acceptance evidence:** Test_AIModule_Start_RejectsBadConfiguration and the HeaderSelfContained, ApiContractCheck and Libgraph steps (no ZenithDocument edge of ZenithAI).
- **Design:** Zenith2/Docs/Design/AI/PublicAPI.md → "Zenith_Module_AI header"; Zenith2/Docs/Design/AI.md → "Module start and world attach"

### Z2-R-AI-009 Module start unwinds on failure

- **Statement:** Every step of Zenith_Module_AI_Start and of AttachWorld must be a fault point whose failure unwinds exactly the earlier steps and leaves no registration, resource, node, listener or participant behind.
- **Covers:** Z2-I-AI-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045, Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** Test_AIModule_Start_FailsAtConfig_Unwinds to Test_AIModule_Attach_FailsAtParticipant_Unwinds in the Test step.
- **Design:** Zenith2/Docs/Design/AI.md → "Module start and world attach"; Zenith2/Docs/Design/AI/Tests.md → "Failure injection tests"

### Z2-R-AI-010 Configuration keys validated at start

- **Statement:** Every ai.* configuration key of "Configuration keys" must be registered with its type, default and range, and a malformed value must fail module start with MODULE_ERROR_REGISTRATION and a log line naming the key.
- **Covers:** Z2-I-AI-05, Z2-I-AI-14
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-024
- **Budgets:** Z2-B-AI-003
- **Acceptance evidence:** Test_AIModule_Start_RejectsBadConfiguration in the Test step.
- **Design:** Zenith2/Docs/Design/AI/PublicAPI.md → "Configuration keys"

### Z2-R-AI-011 Edit and preview worlds run no agents

- **Statement:** In a world whose purpose, read through Zenith_WorldAttachContext::GetWorldPurpose, is WORLD_PURPOSE_EDIT or WORLD_PURPOSE_PREVIEW, AttachWorld must register AI.NavIngest alone and no save participant, so no agent moves and no path request is processed there; the world origin must never be read for this.
- **Covers:** Z2-I-AI-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-024
- **Budgets:** none
- **Acceptance evidence:** Test_NavRuntime_EditWorldHasNavMeshNoAgents in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Agents.md → "Frame-graph placement"; Zenith2/Docs/Design/AI.md → "Module start and world attach"

### Z2-R-AI-012 AI error codes

- **Statement:** Every recoverable AI failure must be reported as a Zenith_AIError record of the codes of Zenith_AIErrorCode, 8 bytes with the code first, and untrusted input must never assert.
- **Covers:** Z2-I-AI-01, Z2-I-AI-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** Test_AIError_Strings and the error-path cases of every test group in the Test step.
- **Design:** Zenith2/Docs/Design/AI/PublicAPI.md → "Zenith_AIErrors header"; Zenith2/Docs/Design/AI.md → "Error enumerations"

### Z2-R-AI-013 Public API rules

- **Statement:** Every fallible AI public function must return Zenith_Result, every public type must carry one contract block, and no third-party or STL type may appear in any AI public header.
- **Covers:** Z2-I-AI-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045, Z2-D-047, Z2-D-041
- **Budgets:** none
- **Acceptance evidence:** The ApiContractCheck, HeaderSelfContained, ForbiddenCheck and Libgraph steps.
- **Design:** Zenith2/Docs/Design/AI/PublicAPI.md → "Scope of this file"; Zenith2/Docs/Design/AI.md → "Rules across the AI API"

### Z2-R-AI-014 API-only slice

- **Statement:** The first Stage 4 AI ticket must land Zenith_AITypes.h, Zenith_AIErrors.h, Zenith_NavMesh.h, Zenith_NavQuery.h, Zenith_AIComponents.h, Zenith_AIEvents.h, Zenith_AICommands.h, Zenith_AIWorldResource.h and Zenith_Module_AI.h with contract blocks and stub definitions, so ENV, STREAM, WORLD and EDIT tickets compile against them before the implementation.
- **Covers:** Z2-I-AI-01, Z2-I-AI-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-047, D-SEL-NAV (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** The HeaderSelfContained and Build steps on the slice; Test_NavPolyRef_PackUnpack and Test_NavTileKey_PackOrder (header-only tests) in the Test step.
- **Design:** Zenith2/Docs/Design/AI.md → "API-only slices"; Zenith2/Docs/Design/AI/PublicAPI.md → "Header list"

### Z2-R-AI-015 Authority census rows

- **Statement:** Every AI component, resource and event must have its row in the authority census with Library ZenithAI and Stage S4 in the diff that registers it, and AI writes of other areas' properties must be commands only.
- **Covers:** Z2-I-AI-01, Z2-I-AI-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080, D-PHASES refinement (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** Test_AuthorityCensus_OpenWorldModules (GAME's Stage 4 suite) in the Test step.
- **Design:** Zenith2/Docs/Design/AI/PublicAPI.md → "Authority census rows"; Zenith2/Docs/Design/AI/Tests.md → "Integration suite"

### Z2-R-AI-016 Memory consumer and shedding

- **Statement:** The AI module must register one memory consumer under MEMORY_SUBSYSTEM_SIMULATION reporting every byte of its tiles, volumes, runtime tables and arenas, and must shed no simulation state at WARNING or CRITICAL, releasing only the development trace ring at CRITICAL.
- **Covers:** Z2-I-AI-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-AI-001
- **Acceptance evidence:** Test_AIModule_MemoryPressureShedsNothingSimulated in the Test step; AI's row in the CORE shedding case table.
- **Design:** Zenith2/Docs/Design/AI.md → "Failure table"; Zenith2/Docs/Design/AI.md → "In-memory sizes that matter"

### Z2-R-AI-017 Lifecycle listener

- **Statement:** The AI lifecycle listener must drop an agent's corridor at a teleport, remove an entity's agent record, blackboard entries and requests at destroy, and change no other state.
- **Covers:** Z2-I-AI-05, Z2-I-AI-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** Test_AIAgent_TeleportDropsCorridor and Test_AIBlackboard_DestroyedOwnerPruned in the Test step.
- **Design:** Zenith2/Docs/Design/AI.md → "Lifecycle listener"; Zenith2/Docs/Design/AI/Agents.md → "Agent state machine"

### Z2-R-AI-018 Neutral navigation file format

- **Statement:** Every cooked navigation file must use ASSET's container with magic ZNAV, format version uAI_NAV_FORMAT_VERSION and the section kinds of its role, written field by field with no C++ layout, pointer, slot number or interned ID on disk.
- **Covers:** Z2-I-AI-01, Z2-I-AI-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** Test_NavFormats_InfoAndRoleRules and Test_NavFormats_TileSectionChecks in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Formats.md → "Navigation file"; Zenith2/Docs/Design/AI/Formats.md → "Section kinds"; Zenith2/Docs/Design/AI/Formats.md → "Info section"

### Z2-R-AI-019 Tile section checks

- **Statement:** The tile section reader must apply checks T1 to T9 in order and refuse a failing tile with AI_ERROR_TILE_CORRUPT naming the check.
- **Covers:** Z2-I-AI-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** Test_NavFormats_TileSectionChecks in the Test step; Fuzz_NavTileSection under the win-asan Sanitizers step.
- **Design:** Zenith2/Docs/Design/AI/Formats.md → "Tile section"

### Z2-R-AI-020 Navigation parser limits

- **Statement:** Every limit of the AI parser-limits table must be a constant of its reader, tested at the limit and at limit + 1, and the rows must join Architecture.md's parser-limit table through the core-doc batch.
- **Covers:** Z2-I-AI-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** Test_NavFormats_ParserLimits in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Formats.md → "Parser limits"

### Z2-R-AI-021 Navigation kind loader

- **Statement:** The kind-15 loader must load every role through ZenithAssets on loader jobs, report resident bytes under MEMORY_SUBSYSTEM_SIMULATION, honour cancellation, and fail a corrupt file without creating an object.
- **Covers:** Z2-I-AI-01, Z2-I-AI-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** Z2-B-AI-001
- **Acceptance evidence:** Test_NavAsset_LoaderContract (through Zenith_AssetKindLoaderHarness) in the Test step.
- **Design:** Zenith2/Docs/Design/AI.md → "Asset kind"; Zenith2/Docs/Design/AI/PublicAPI.md → "Zenith_NavMeshAsset header"

### Z2-R-AI-022 Generational polygon references

- **Statement:** Zenith_NavPolyRef must encode polygon, tile slot and a 16-bit salt that changes when a slot is freed or a tile is carved, so every query on a stale reference fails with AI_ERROR_STALE_POLY_REF.
- **Covers:** Z2-I-AI-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** Test_NavPolyRef_PackUnpack and Test_NavTileTable_SlotReuseBumpsSalt in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Polygon references"; Zenith2/Docs/Design/AI/PublicAPI.md → "Zenith_AITypes header"

### Z2-R-AI-023 Stable tile keys

- **Statement:** Every navigation ordering, tie-break and saved reference must use Zenith_NavTileKey and polygon indices, never a tile slot, pointer or insertion order.
- **Covers:** Z2-I-AI-05, Z2-I-AI-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** Test_NavTileKey_PackOrder and Test_NavPath_IndependentOfSlotAssignment in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Tile grid and tile keys"; Zenith2/Docs/Design/AI/Navigation.md → "Tie-breaking"

### Z2-R-AI-024 Navigation settings and classes

- **Statement:** A world must accept the classes, area table and cost profiles of exactly one settings asset, at most 4 ground and 2 volume classes, and refuse any navigation file naming another settings asset or hash with AI_ERROR_SETTINGS_MISMATCH.
- **Covers:** Z2-I-AI-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** Test_NavTileTable_SettingsMismatch and Test_NavMesh_ClassResolution in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Agent classes"; Zenith2/Docs/Design/AI/Formats.md → "Agent class object"

### Z2-R-AI-025 Area codes and cost profiles

- **Statement:** Area codes 0 to 4 must equal ENV's Zenith_NavigationArea values, codes 5 to 62 must be the settings document's game areas, every cost must be at least 1.0 or infinite, and an agent's cost profile must select its query filter.
- **Covers:** Z2-I-AI-01, Z2-I-AI-17
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** Test_NavQueryFilter_Validate and Test_NavPath_CostProfiles in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Area codes and costs"; Zenith2/Docs/Design/AI/Formats.md → "Area table"; Zenith2/Docs/Design/AI/Formats.md → "Cost profiles"

### Z2-R-AI-026 Navigation settings document type

- **Statement:** The navigation settings document Zenith.NavigationSettingsDocument must be imported by the importer ai.navSettings and cooked by the kind-15 step ai.navMesh into a SETTINGS file, with every schema rule of its classes, areas and profiles reported as an AI_ERROR_DOCUMENT_SCHEMA diagnostic naming the member path.
- **Covers:** Z2-I-AI-01, Z2-I-AI-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** Test_NavCook_SettingsDocumentSchema in the Test step (win-tools).
- **Design:** Zenith2/Docs/Design/AI/Formats.md → "Navigation settings document"; Zenith2/Docs/Design/AI.md → "Cook steps and derived-data keys"

### Z2-R-AI-027 Navigation document and products

- **Statement:** The importer ai.navigation must read a Zenith.NavigationDocument and declare, through Zenith_AIDeclareNavigationProducts with the manifest key navigation, the MANIFEST product and exactly the non-empty tile and volume products in key order, keeping product GUIDs across reimports, a volume keyed by its index member or array position.
- **Covers:** Z2-I-AI-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** Test_NavCook_NavigationDocumentProducts in the Test step (win-tools).
- **Design:** Zenith2/Docs/Design/AI/Formats.md → "Navigation document schema"; Zenith2/Docs/Design/AI.md → "Cook steps and derived-data keys"

### Z2-R-AI-028 Document migration

- **Statement:** Both AI document types must join the authoring-document migration framework with a per-version migration chain, preview, backup, journaled write and unknown-member preservation.
- **Covers:** Z2-I-AI-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** Test_NavCook_DocumentMigrationChain in the Test step (win-tools).
- **Design:** Zenith2/Docs/Design/AI.md → "Document types and migration"

### Z2-R-AI-029 Bake input gathering

- **Statement:** A tile bake must gather terrain source geometry, static collider geometry, blockers and area volumes inside the tile bounds grown by the border from source scene documents (absolute positions) and cooked scene products (positions relative to their binary64 offset), excluding dynamic bodies, triggers, controllers and ignored layers, sorted by (source kind, source persistent ID, triangle index).
- **Covers:** Z2-I-AI-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038, D-SEL-NAV (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** Test_NavCook_InputsFromScenesAndTerrain and Test_NavCook_InputsFromCookedSceneWithOffset in the Test step (win-tools).
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Bake inputs"

### Z2-R-AI-030 Terrain navigation source intermediate

- **Statement:** AI must provide the tool-side writer Zenith_AIWriteNavSourceIntermediate that ENV's importer calls for every navigation product, and the ai.navMesh step must turn its ZNSG blob into a SOURCE_GEOMETRY file that publishes ai.navSource.
- **Covers:** Z2-I-AI-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038, D-SEL-NAV (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** Test_NavCook_SourceGeometryFromEnv in the Test step (win-tools).
- **Design:** Zenith2/Docs/Design/AI/Formats.md → "Navigation source intermediate"; Zenith2/Docs/Design/AI/Formats.md → "Source geometry section"

### Z2-R-AI-031 Rasterization and filtering stages

- **Statement:** Bake stages 1 to 4 must rasterize walkable triangles by slope, merge spans within climb, remove low-hanging obstacles, ledges and low-clearance spans, build the compact heightfield and erode it by the agent radius as their table specifies.
- **Covers:** Z2-I-AI-01, Z2-I-AI-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-NAV (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** Test_NavBuild_SlopeLimit, Test_NavBuild_ClimbAndStairs, Test_NavBuild_LowClearance and Test_NavBuild_ErosionByRadius in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Bake stages"

### Z2-R-AI-032 Area volume marking

- **Statement:** Bake stage 5 must apply area volumes in ascending (priority, ordinal) order with the volume applied last winning and a null volume removing spans.
- **Covers:** Z2-I-AI-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-NAV (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** Test_NavBuild_AreaVolumesInOrder in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Bake stages"; Zenith2/Docs/Design/AI/PublicAPI.md → "Zenith_NavBuild header"

### Z2-R-AI-033 Region partition

- **Statement:** Bake stage 6 must offer the watershed, monotone and layers partitions with the region removal and merge rules of its table and fixed tie-breaks.
- **Covers:** Z2-I-AI-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-NAV (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** Test_NavBuild_PartitionsAgree and Test_NavBuild_TwoStoreyLayers in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Bake stages"

### Z2-R-AI-034 Contours and polygons

- **Statement:** Bake stages 7 and 8 must produce convex polygons of 3 to 6 vertices with welded vertices, neighbour links and portal edges flagged by tile side.
- **Covers:** Z2-I-AI-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-NAV (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** Test_NavBuild_PolygonsConvexAndBounded and Test_NavBuild_FlatPlaneOneRegion in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Bake stages"

### Z2-R-AI-035 Detail heights

- **Statement:** Bake stage 9 must produce detail triangles whose heights are within detailSampleMaxError cell heights of the input surface, or flat polygons when the sample distance is 0.
- **Covers:** Z2-I-AI-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-NAV (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** Test_NavBuild_DetailHeights in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Bake stages"

### Z2-R-AI-036 Tile assembly

- **Statement:** Bake stage 10 must remove the border, quantise vertices to tile-local u16 units, set polygon flags from the area table and build the bounding-volume tree, producing a tile section that passes checks T1 to T9.
- **Covers:** Z2-I-AI-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-NAV (open, Stage 1/2), Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** Test_NavBuild_PolygonsConvexAndBounded in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Bake stages"; Zenith2/Docs/Design/AI/Formats.md → "Tile section"

### Z2-R-AI-037 Bake limits

- **Statement:** Each bake limit, including 127 detail vertices and 255 detail triangles per polygon, must succeed at the limit and fail at limit + 1 with AI_ERROR_BUILD_LIMIT naming the stage, and the cook must report the tile and the class parameter to change.
- **Covers:** Z2-I-AI-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-NAV (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** Test_NavBuild_Limits in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Bake limits"

### Z2-R-AI-038 Bake determinism

- **Statement:** A tile's cooked bytes must be identical across runs, input orders and cook worker counts on one build.
- **Covers:** Z2-I-AI-01, Z2-I-AI-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** Test_NavBake_TileBytesIdenticalAcrossRuns and Test_NavCook_ByteIdenticalAcrossWorkers in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Bake determinism"

### Z2-R-AI-039 Incremental navigation cook

- **Statement:** A tile item's key must contain every opened document digest and dependency output key, so that a change to one collider re-cooks exactly the tiles its bounds touch.
- **Covers:** Z2-I-AI-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** Test_NavCook_IncrementalOnlyTouchedTiles in the Test step (win-tools).
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Bake determinism"; Zenith2/Docs/Design/AI.md → "Cook steps and derived-data keys"

### Z2-R-AI-040 Empty tiles

- **Statement:** A tile with no walkable span must yield zero polygons and success, and the cook must neither declare nor write a product for it.
- **Covers:** Z2-I-AI-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-NAV (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** Test_NavBuild_EmptyInputEmptyTile and Test_NavCook_NavigationDocumentProducts in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Bake stages"; Zenith2/Docs/Design/AI/Formats.md → "Navigation document schema"

### Z2-R-AI-041 Carve layers kept

- **Statement:** Tiles intersecting a navigation document carve region must carry a LAYERS section with the stage-4 compact heightfield and the tile's area volumes, from which a rebuild with no obstacle reproduces the baked polygons for the monotone partition.
- **Covers:** Z2-I-AI-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-NAV (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** Test_NavBuild_KeepLayersRoundTrip in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Formats.md → "Layers section"; Zenith2/Docs/Design/AI/Navigation.md → "Bake stages"

### Z2-R-AI-042 Editor regeneration

- **Statement:** After a terrain stroke or a scene edit the editor must rebake the affected tiles in process with the same build functions the cook calls and reload them into its world.
- **Covers:** Z2-I-AI-01, Z2-I-AI-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** Test_AIEditor_RegenerateAfterTerrainStroke in the Test step (win-tools).
- **Design:** Zenith2/Docs/Design/AI.md → "Editor surfaces"

### Z2-R-AI-043 Cell navigation content

- **Statement:** AI must provide Zenith_AIMakeNavigationManifestView over WORLD's Zenith_WorldProduct list and Zenith_AIWriteCellNavigationContent over WORLD's Zenith_WorldCellBounds and Zenith_WorldCellContentWriter, writing one Zenith.AI.NavMeshTiles entity per class and group of 16 tiles in key order and one Zenith.AI.NavVolumeRef entity per volume region whose minimum corner lies in the cell, placed from product keys alone.
- **Covers:** Z2-I-AI-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-NAV (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** Test_NavCook_CellContentRecords in ZenithAuthoring_Tests (win-tools).
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Streamed tiles"; Zenith2/Docs/Design/AI/Internals.md → "Zenith_AuthoringAI header"; Zenith2/Docs/Design/AI/Formats.md → "Navigation document schema"

### Z2-R-AI-044 Tile add procedure

- **Statement:** AI.NavIngest must add the tiles published at the previous tick boundary, in their join-list order, through the validation, slot, connection, obstacle and link steps of the tile add procedure, and must never poll or resolve an asset from a phase node.
- **Covers:** Z2-I-AI-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-NAV (open, Stage 1/2)
- **Budgets:** Z2-B-AI-001
- **Acceptance evidence:** Test_NavRuntime_TileJoinsAtPublication and Test_NavTileTable_DuplicateKeyRefused in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Tile add procedure"; Zenith2/Docs/Design/AI/Navigation.md → "Navigation asset requests"

### Z2-R-AI-045 Tile capacity

- **Statement:** A tile beyond ai.nav.maxTiles must be refused with AI_ERROR_TILE_CAPACITY and counted, changing nothing else.
- **Covers:** Z2-I-AI-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-NAV (open, Stage 1/2)
- **Budgets:** Z2-B-AI-001
- **Acceptance evidence:** Test_NavTileTable_Capacity in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Tile add procedure"; Zenith2/Docs/Design/AI.md → "Failure table"

### Z2-R-AI-046 Tile connections in both directions

- **Statement:** Adjacent tiles of one class must connect their portal edges in both tiles whatever the add order, the full-list overflow rule of "Tile connection procedure" must decide which candidates exist in both tiles, and a removed tile must leave no external link naming it.
- **Covers:** Z2-I-AI-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-NAV (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** Test_NavRuntime_TileConnectionBothDirections and Test_NavRuntime_TileRemoveDisconnects in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Tile connection procedure"; Zenith2/Docs/Design/AI/Navigation.md → "Tile remove procedure"

### Z2-R-AI-047 Streamed tile activation

- **Statement:** A tile on an entity outside an active streaming region must not be requested or added; its load must be requested in the merge node of the tick its InActiveRegion becomes true, and it must join in the phase 7 after that request's publication.
- **Covers:** Z2-I-AI-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-NAV (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** Test_NavRuntime_InactiveRegionTileAbsent in the Test step; Test_AIIntegration_StreamedWorldNavigation in Zenith_Integration_AI.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Streamed tiles"; Zenith2/Docs/Design/AI/Navigation.md → "Navigation asset requests"; Zenith2/Docs/Design/AI/Navigation.md → "Supplier records"

### Z2-R-AI-048 Navigation streaming stress

- **Statement:** 1,000 randomised tile add and remove cycles with moving agents must leave no live tile slot, asset reference, link record or allocation.
- **Covers:** Z2-I-AI-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-NAV (open, Stage 1/2)
- **Budgets:** Z2-B-AI-001
- **Acceptance evidence:** Test_NavRuntime_StreamTileLoadUnload1000Cycles in the Test step (with the test main's leak check).
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Tile remove procedure"

### Z2-R-AI-049 Origin shift preserves navigation

- **Statement:** An origin shift must be applied by AI at the sync point SCENE plays it, recomputing every tile and volume's world-local origin and translating module-state positions by SCENE's formula, without rewriting vertices or changing references, so phases 2 to 12 of the rebase tick see shifted navigation.
- **Covers:** Z2-I-AI-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-NAV (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** Test_NavRuntime_OriginShiftKeepsCorridors in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Origin shift"

### Z2-R-AI-050 Non-streamed navmesh reference

- **Statement:** A world without streaming must load its whole navmesh through one Zenith.AI.NavMeshRef component naming a MANIFEST whose hard references make every tile and volume resident with it.
- **Covers:** Z2-I-AI-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** Test_AIIntegration_CrowdOfHundred (a manifest-loaded world) in Zenith_Integration_AI.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Streamed tiles"; Zenith2/Docs/Design/AI/Formats.md → "Manifest section"

### Z2-R-AI-051 Obstacle component

- **Statement:** Zenith.AI.NavObstacle must describe a cylinder or box obstacle in MARK or CARVE mode at its entity's transform, re-applied when it moves farther than its MoveThreshold or changes.
- **Covers:** Z2-I-AI-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-NAV (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** Test_NavObstacle_MarkBlocksPolygons in the Test step.
- **Design:** Zenith2/Docs/Design/AI/PublicAPI.md → "Zenith_AIComponents header"; Zenith2/Docs/Design/AI/Navigation.md → "Obstacle modes"

### Z2-R-AI-052 Mark obstacles

- **Statement:** A MARK obstacle must flag the polygons its radius-inflated footprint overlaps as blocked, or re-area them with the highest simulation ID winning, keeping the first 256 in (tile key, polygon) order, without rebuilding any tile.
- **Covers:** Z2-I-AI-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-NAV (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** Test_NavObstacle_MarkBlocksPolygons, Test_NavObstacle_MarkAreaOverride and Test_NavObstacle_MarkTruncationOrder in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Mark procedure"

### Z2-R-AI-053 Carve obstacles

- **Statement:** A CARVE obstacle must rebuild every carve-capable tile under it from its layers with the footprint cut, so no polygon remains inside the inflated footprint, and removing it must restore the baked tile bytes.
- **Covers:** Z2-I-AI-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-NAV (open, Stage 1/2)
- **Budgets:** Z2-B-AI-002
- **Acceptance evidence:** Test_NavCarve_CutsFootprint and Test_NavCarve_RemovalRestoresBakedTile in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Carve procedure"

### Z2-R-AI-054 Carve fallback to mark

- **Statement:** A CARVE obstacle over a tile without a layers section must be applied as MARK and counted in ai.nav.carve_fallback_delta.
- **Covers:** Z2-I-AI-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-NAV (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** Test_NavCarve_FallbackToMark in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Carve procedure"; Zenith2/Docs/Design/AI/PublicAPI.md → "Zenith_AIComponents header"

### Z2-R-AI-055 Carve budget and order

- **Statement:** At most ai.nav.maxTileRebuildsPerTick tiles must be rebuilt per tick, in (first-queued tick, tile key) order, as sub-jobs whose results are swapped in tile-key order after the join.
- **Covers:** Z2-I-AI-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-NAV (open, Stage 1/2), Z2-D-006
- **Budgets:** Z2-B-AI-002
- **Acceptance evidence:** Test_NavCarve_BudgetAndOrder in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Carve procedure"

### Z2-R-AI-056 Carve determinism

- **Statement:** The tick at which each carved tile becomes visible and its bytes must be identical on every worker count, and a runtime rebuild must equal the cook-side rebuild of the same layers.
- **Covers:** Z2-I-AI-03, Z2-I-AI-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** Test_NavCarve_DeterministicAcrossWorkers and Test_NavCarve_MatchesCookRebuild in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Carve procedure"; Zenith2/Docs/Design/AI/Navigation.md → "Bake determinism"

### Z2-R-AI-057 Carving invalidates paths

- **Statement:** A carve swap must increment the tile's salt and version so every corridor through it replans in the tick it is observed.
- **Covers:** Z2-I-AI-03, Z2-I-AI-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-NAV (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** Test_NavCarve_InvalidatesCorridors in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Polygon references"; Zenith2/Docs/Design/AI/Agents.md → "Path corridor"

### Z2-R-AI-058 Off-mesh link component and registration

- **Statement:** Zenith.AI.OffMeshLink must register a link for each class in its ClassMask whose both ends snap to the navmesh within SnapDistance, keep unresolved links for retry when tiles are added, and refuse a ninth link per polygon.
- **Covers:** Z2-I-AI-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-NAV (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** Test_NavLink_RegisterBothEnds and Test_NavLink_CapacityPerPoly in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Link registration"; Zenith2/Docs/Design/AI/PublicAPI.md → "Zenith_AIComponents header"

### Z2-R-AI-059 Cross-cell links

- **Statement:** A link joining tiles of two streamed cells must be registered only while both tiles are loaded, in both directions at once.
- **Covers:** Z2-I-AI-04, Z2-I-AI-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-NAV (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** Test_NavLink_CrossCellBothTilesRequired in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Link registration"

### Z2-R-AI-060 Automatic link traversal

- **Statement:** WALK, JUMP and TELEPORT links must be traversed by AI over their duration along a line, a parabola or a teleport, with the controller suspended during WALK and JUMP.
- **Covers:** Z2-I-AI-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-NAV (open, Stage 1/2), Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** Test_NavLink_JumpTraversal in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Link traversal state machine"; Zenith2/Docs/Design/AI/Navigation.md → "Link kinds"

### Z2-R-AI-061 Scripted link traversal

- **Statement:** A SCRIPTED link must stop the agent in WAITING_LINK and emit Zenith.AI.OffMeshLinkReached until AI.CompleteOffMeshLink plays back, and a disabled link under a waiting agent must make it replan.
- **Covers:** Z2-I-AI-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-NAV (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** Test_NavLink_ScriptedWaitsForCompletion and Test_NavLink_DisabledLinkReplans in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Link traversal state machine"; Zenith2/Docs/Design/AI/Scripting.md → "Binding functions"

### Z2-R-AI-062 Optimal polygon search

- **Statement:** Zenith_NavPathSearch must run A* with node reopening, the cost and admissible heuristic of "Cost and heuristic" and entry positions replaced on relaxation, returning the least-cost corridor of the portal-midpoint graph with node reopening.
- **Covers:** Z2-I-AI-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-NAV (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** Test_NavPath_RelaxationRules, Test_NavPath_PlaneDiagonalOptimal and Test_NavPath_LCorridorThreeCorners in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Search graph"; Zenith2/Docs/Design/AI/Navigation.md → "Cost and heuristic"; Zenith2/Docs/Design/AI/Navigation.md → "Search state and slicing"

### Z2-R-AI-063 Deterministic tie-breaking

- **Statement:** The search open list must order by (f, h, tie-break key), a polygon's key being (tile key, polygon index) and a link node's (its start polygon's tile key and index, then the link entity simulation ID), so corridors are identical whatever tile slots the tiles occupy.
- **Covers:** Z2-I-AI-05, Z2-I-AI-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** Test_NavPath_IndependentOfSlotAssignment and Test_NavPath_RelaxationRules in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Tie-breaking"

### Z2-R-AI-064 Sliced search equivalence

- **Statement:** A search stepped in slices of any size must reach the same states and results as one stepped without limit, its state after k expansions being a pure function of the navmesh history it saw, the descriptor and k.
- **Covers:** Z2-I-AI-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** Test_NavPath_SlicedEqualsUnsliced in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Search state and slicing"; Zenith2/Docs/Design/AI/PublicAPI.md → "Zenith_NavQuery header"

### Z2-R-AI-065 Partial and failed paths

- **Statement:** A search must return PARTIAL toward the best polygon node when the end is unreachable (limit NONE), the node pool is exhausted (NODE_POOL) or the tick cap is reached (TICK_CAP), NO_START and NO_END when the extents find no polygon, and TRUNCATED (CORRIDOR) beyond 256 corridor polygons, a PARTIAL or TRUNCATED result's end point being the point of its last corridor polygon closest to the requested end.
- **Covers:** Z2-I-AI-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-NAV (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** Test_NavPath_PartialTowardBest, Test_NavPath_NoStartNoEnd, Test_NavPath_NodePoolExhaustion and Test_NavPath_TruncatedCorridor in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Partial results"; Zenith2/Docs/Design/AI/Navigation.md → "Search state and slicing"

### Z2-R-AI-066 Stale searches restart

- **Statement:** A search must report AI_ERROR_STALE_POLY_REF only when a tile it expanded into was removed, its slot re-used or its version changed, never for an overlay, neighbour-tile or link change, and must be restarted by the path service at most once per tick.
- **Covers:** Z2-I-AI-05, Z2-I-AI-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-NAV (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** Test_NavPath_StaleDuringSearchRestarts and Test_NavPath_OverlayChangeDoesNotStale in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Per-tick processing"; Zenith2/Docs/Design/AI/Navigation.md → "Search state and slicing"

### Z2-R-AI-067 Straight path extraction

- **Statement:** Zenith_NavFindStraightPath must string-pull the corridor with the funnel algorithm, flagging start, end, off-mesh starts and optional area crossings, with heights from the detail mesh.
- **Covers:** Z2-I-AI-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-NAV (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** Test_NavPath_StraightPathFlags in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Straight path extraction"

### Z2-R-AI-068 Path service in the frame graph

- **Statement:** Every path request must run inside AI.NavRequests in phase 7 as sliced searches stepped by sub-jobs with a fixed per-request expansion budget, so no job waits and no search runs outside the frame graph.
- **Covers:** Z2-I-AI-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-053
- **Budgets:** Z2-B-AI-002, Z2-B-AI-003
- **Acceptance evidence:** Test_PathService_DeterministicAcrossWorkers in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Per-tick processing"; Zenith2/Docs/Design/AI/Agents.md → "Frame-graph placement"

### Z2-R-AI-069 Active request cap and deferral

- **Statement:** At most ai.path.maxActiveRequests requests, the value of Z2-B-AI-003, must be active per tick, the rest deferred in queue order and counted in ai.path.deferred_delta.
- **Covers:** Z2-I-AI-05, Z2-I-AI-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** Z2-B-AI-003
- **Acceptance evidence:** Test_PathService_DeferralCounted in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Per-tick processing"; Zenith2/Docs/Design/AI/Navigation.md → "Request queue"

### Z2-R-AI-070 Request tick cap

- **Statement:** A request still active after ai.path.maxTicksPerRequest ticks must finish as PARTIAL at the cap.
- **Covers:** Z2-I-AI-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-NAV (open, Stage 1/2)
- **Budgets:** Z2-B-AI-002
- **Acceptance evidence:** Test_PathService_TickCapPartial in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Per-tick processing"

### Z2-R-AI-071 Path service determinism

- **Statement:** The tick on which each request finishes and its result must be the same for every worker count and thread interleaving in live runs whose navigation loads are released by AI's completion gate with a fixed delay, and in replays.
- **Covers:** Z2-I-AI-05, Z2-I-AI-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** Test_PathService_DeterministicAcrossWorkers and Test_AIDeterminism_LiveRunsAgree in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Per-tick processing"; Zenith2/Docs/Design/AI/Agents.md → "Determinism rules"

### Z2-R-AI-072 Agent plans superseded in place

- **Statement:** An agent must have at most one queued or active plan, a newer plan replacing the older in place and counted in ai.path.superseded_delta.
- **Covers:** Z2-I-AI-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-NAV (open, Stage 1/2)
- **Budgets:** Z2-B-AI-003
- **Acceptance evidence:** Test_PathService_SupersededAgentPlan in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Request queue"

### Z2-R-AI-073 Script path requests

- **Statement:** AI.RequestPath must be a behaviour-only buffered binding appending a 64-byte ASYNC_SOURCE_PATHFINDING request record that the multiplexer delivers to AI's consumer at commit, with its capacity accepted under Z2-B-SCRIPT-026 at the merge.
- **Covers:** Z2-I-AI-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-053, D-SCRIPT-TXN refinement (open, Stage 1)
- **Budgets:** Z2-B-SCRIPT-026
- **Acceptance evidence:** Test_AIScript_RequestPathRoundTrip and Test_AIScript_RequestPathFromSystemRejected in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Scripting.md → "Path request payload and result"; Zenith2/Docs/Design/AI/Formats.md → "Script request payload"

### Z2-R-AI-074 Script path results published at a tick boundary

- **Statement:** A script path result must be parked through the module publication sink as PUBLICATION_KIND_GENERIC_RECOMPUTED in the tick its search finished and be visible from the next tick boundary in completion-ID order.
- **Covers:** Z2-I-AI-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** Z2-B-SCRIPT-026
- **Acceptance evidence:** Test_PathService_ScriptResultPublishedNextTick and Test_PathService_ReplayRecomputesSameTick in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Publication of script results"; Zenith2/Docs/Design/AI/Formats.md → "Script result payloads"

### Z2-R-AI-075 Path result decoder

- **Statement:** AI's result decoder for ASYNC_SOURCE_PATHFINDING must decode the 416-byte payload into the PathResult table and return false for a payload AI could not have written.
- **Covers:** Z2-I-AI-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** Test_AIScript_RequestPathRoundTrip and Test_AIScript_DecoderRejectsBadPayload in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Scripting.md → "Path request payload and result"; Zenith2/Docs/Design/AI/Formats.md → "Script result payloads"

### Z2-R-AI-076 Path request cancellation

- **Statement:** A request whose owner is destroyed, quarantined or stopped must be cancelled with its node pool released in the same step, a script request completing with status CANCELLED, and every request of a world that transitions away must be discarded unpublished.
- **Covers:** Z2-I-AI-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** Z2-B-AI-001
- **Acceptance evidence:** Test_PathService_CancelReleasesNodePool, Test_PathService_ScriptRequestCancelledPayload and Test_AIIntegration_TransitionDiscardsPathRequests.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Cancellation"; Zenith2/Docs/Design/AI/Navigation.md → "Path request state machine"

### Z2-R-AI-077 Navmesh spatial queries

- **Statement:** Nearest-point, raycast, surface-movement and random-reachable-point queries must follow their algorithms with stable tie-breaks and the canonical neighbour order, and random points must come from the caller's counter-based stream.
- **Covers:** Z2-I-AI-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** Test_NavQuery_NearestPointTieBreak, Test_NavQuery_RaycastWallNormal, Test_NavQuery_RandomPointDeterministic, Test_NavQuery_MoveAlongSurfaceSlides and Test_NavQuery_MoveAlongSurfaceIndependentOfAddOrder in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Nearest point"; Zenith2/Docs/Design/AI/Navigation.md → "Navmesh raycast"; Zenith2/Docs/Design/AI/Navigation.md → "Random reachable point"; Zenith2/Docs/Design/AI/Navigation.md → "Surface movement"

### Z2-R-AI-078 Agent component

- **Statement:** Zenith.AI.Agent must carry the configuration, request, applied-velocity and state properties of its table with their authorities, and an invalid configuration must leave the agent IDLE and counted.
- **Covers:** Z2-I-AI-06, Z2-I-AI-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** Test_AIAgent_StateMachineTable in the Test step.
- **Design:** Zenith2/Docs/Design/AI/PublicAPI.md → "Agent properties"; Zenith2/Docs/Design/AI/PublicAPI.md → "Zenith_AIComponents header"

### Z2-R-AI-079 Agent runtime table

- **Statement:** Agent runtime records must be held in one simulation-ID-sorted table written only by AI.Locomotion, each sub-job writing only its agents' records, with a capacity refusal counted.
- **Covers:** Z2-I-AI-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-AI-001
- **Acceptance evidence:** Test_AIAgent_CapacityRefused in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Agents.md → "Agent runtime table"

### Z2-R-AI-080 Move commands

- **Statement:** The AI.Agent command kind must apply MOVE_TO, STOP, PAUSE, RESUME, SET_SPEED_SCALE and SET_MEDIUM at sync points in merge order, the last MOVE_TO of a sync point winning.
- **Covers:** Z2-I-AI-06, Z2-I-AI-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** Z2-B-ECS-004
- **Acceptance evidence:** Test_AIAgent_SupersededMoveAnswered in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Agents.md → "Move commands"; Zenith2/Docs/Design/AI/Formats.md → "Command payloads"

### Z2-R-AI-081 Agent state machine

- **Statement:** Every agent must follow the transitions and actions of the agent state machine table, including suspension outside active regions and after teleports.
- **Covers:** Z2-I-AI-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** Test_AIAgent_StateMachineTable and Test_AIAgent_SuspendedOutsideActiveRegion in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Agents.md → "Agent state machine"

### Z2-R-AI-082 One completion per move

- **Statement:** Every answered MoveSerial must receive exactly one Zenith.AI.MoveCompleted, re-emitted from the pending completions after a rejected buffer, with CompletedSerial monotonic.
- **Covers:** Z2-I-AI-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** Z2-B-GAME-003
- **Acceptance evidence:** Test_AIAgent_OneCompletionPerSerial and Test_AIAgent_RejectedCompletionReemitted in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Agents.md → "Agent state machine"; Zenith2/Docs/Design/AI/PublicAPI.md → "Zenith_AIEvents header"

### Z2-R-AI-083 Stuck detection

- **Statement:** A moving agent displaced less than 0.25 m over a stuck window must replan, and a second consecutive strike must fail its move.
- **Covers:** Z2-I-AI-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** Test_AIAgent_StuckFailsAfterTwoStrikes in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Agents.md → "Stuck detection"

### Z2-R-AI-084 Path corridor maintenance

- **Statement:** Each tick a moving agent must move its corridor start along the surface, validate the first 8 polygons, follow a moving target and optimise the corridor at its interval as "Path corridor" specifies.
- **Covers:** Z2-I-AI-05, Z2-I-AI-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-NAV (open, Stage 1/2)
- **Budgets:** Z2-B-AI-002
- **Acceptance evidence:** Test_NavCarve_InvalidatesCorridors and Test_AIAgent_ArrivesWithinAcceptRadius in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Agents.md → "Path corridor"

### Z2-R-AI-085 Steering and arrival

- **Statement:** Steering must aim at the next corner with arrival deceleration at the end and at link starts, and an agent must stop within its AcceptRadius of the target.
- **Covers:** Z2-I-AI-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** Test_AIAgent_ArrivesWithinAcceptRadius in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Agents.md → "Steering"

### Z2-R-AI-086 Reciprocal avoidance

- **Statement:** Local avoidance must compute ORCA half-planes for at most the tier's neighbour count and 16 boundary segments and solve them with the ordered linear program and its fallback.
- **Covers:** Z2-I-AI-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-AI-002
- **Acceptance evidence:** Test_AIOrca_HeadOnPassSafely, Test_AIOrca_CircleSwap and Test_AIOrca_InfeasibleFallback in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Agents.md → "ORCA constraints"; Zenith2/Docs/Design/AI/Agents.md → "Linear program"; Zenith2/Docs/Design/AI/Agents.md → "Neighbour selection"

### Z2-R-AI-087 Avoidance responsibility

- **Statement:** Avoidance responsibility must be split by AvoidancePriority, with full responsibility toward a neighbour that does not avoid.
- **Covers:** Z2-I-AI-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** Test_AIOrca_PriorityShare and Test_AIOrca_NonAvoidingNeighbour in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Agents.md → "ORCA constraints"

### Z2-R-AI-088 Avoidance order independence

- **Statement:** Each agent's chosen velocity must depend only on its own row, its record and the previous tick's neighbour state, so velocities are bit-identical for any chunk order and worker count.
- **Covers:** Z2-I-AI-06, Z2-I-AI-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** Test_AIOrca_OrderIndependent in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Agents.md → "Determinism rules"; Zenith2/Docs/Design/AI/Agents.md → "Crowd grid"

### Z2-R-AI-089 Crowd grid

- **Statement:** AI.CrowdGrid must build a grid of every agent sorted by (cell, simulation ID) from columns no phase-7 system writes, and neighbour queries must return exactly the agents within the radius.
- **Covers:** Z2-I-AI-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-AI-002
- **Acceptance evidence:** Test_AICrowdGrid_SortedAndComplete in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Agents.md → "Crowd grid"; Zenith2/Docs/Design/AI/Agents.md → "Frame-graph placement"

### Z2-R-AI-090 Crowd at the W-Characters scale

- **Statement:** One hundred agents with the W-Characters scripted goals must all arrive or report unreachable within 120 seconds with no sustained overlap.
- **Covers:** Z2-I-AI-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-021
- **Budgets:** Z2-B-AI-002
- **Acceptance evidence:** Test_AIIntegration_CrowdOfHundred in Zenith_Integration_AI.
- **Design:** Zenith2/Docs/Design/AI/Tests.md → "Integration suite"; Zenith2/Docs/Design/AI/Agents.md → "Avoidance"

### Z2-R-AI-091 Locomotion output through the character controller

- **Statement:** A controller agent must reach the controller only through SetProperty commands on DesiredVelocity, DesiredYawRadians and InputFlags at the sync point after phase 7, recorded only when changed, with one zero-velocity command when it stops.
- **Covers:** Z2-I-AI-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080, D-PHASES refinement (open, Stage 1)
- **Budgets:** Z2-B-ECS-004
- **Acceptance evidence:** Test_AIAgent_ControllerOutputCommands in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Agents.md → "Locomotion output"

### Z2-R-AI-092 Kinematic locomotion output

- **Statement:** An agent without a controller, a KINEMATIC agent or a volume agent must be moved by SetProperty commands on Zenith.LocalTransform position and rotation, ground agents following the surface height.
- **Covers:** Z2-I-AI-07, Z2-I-AI-16
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** Z2-B-ECS-004
- **Acceptance evidence:** Test_AIAgent_KinematicOutputCommands in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Agents.md → "Locomotion output"

### Z2-R-AI-093 Turn rate limit

- **Statement:** An agent's yaw must change by at most MaxTurnRate times the tick duration per tick.
- **Covers:** Z2-I-AI-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** Test_AIAgent_TurnRateLimit in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Agents.md → "Locomotion output"

### Z2-R-AI-094 Root agents only

- **Statement:** An agent that is not a hierarchy root must never be moved and must be counted once in ai.agent.not_root.
- **Covers:** Z2-I-AI-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** Test_AIAgent_NotRootNeverMoved in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Agents.md → "Locomotion output"

### Z2-R-AI-095 Rejected locomotion output resent

- **Statement:** An agent whose command buffer is rejected at the merge must keep its last accepted values, written only by its merge function for accepted buffers, so the next tick records them again.
- **Covers:** Z2-I-AI-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-051
- **Budgets:** Z2-B-ECS-004
- **Acceptance evidence:** Test_AIAgent_RejectedBufferResends in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Agents.md → "Locomotion output"

### Z2-R-AI-096 Agent sizes as classes

- **Statement:** Each ground class must be baked with its own radius, height, climb and slope, so an agent can use only corridors its class admits.
- **Covers:** Z2-I-AI-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-NAV (open, Stage 1/2)
- **Budgets:** Z2-B-AI-001
- **Acceptance evidence:** Test_NavConformance_N11_AgentSizes in ZenithAI_Conformance; Test_NavBuild_ErosionByRadius in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Agent classes"; Zenith2/Docs/Design/AI/Navigation.md → "Conformance scenarios"
