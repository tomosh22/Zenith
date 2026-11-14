# WORLD design — World authoring

The design of the WORLD area: how large worlds are authored and baked. A world is a world document plus region documents (SCENE scenes, one per layer and authoring region), generator documents and baked lighting documents; the importer `world.world` partitions that authoring content into STREAM's runtime contract — cell scenes, the streaming world document, HLOD recipes and lighting intermediates — whenever ASSET's keys say an input changed; generators reproduce procedural content from the world seed, either at every cook or baked into generated region documents; and three baking workflows (lighting, navigation, HLOD) share one orchestration of units, fingerprints, ordering, cancellation, resumption and reports. WORLD's code is tool code: `Zenith2/Tools/Authoring/Internal/WORLD/` in the `ZenithAuthoring` tool library (ruling Q84 (a)), the importer in `Zenith2/Tools/ZenithCook/Internal/World/` and the editor extension in `Zenith2/Tools/ZenithEditor/Internal/World/`; nothing of WORLD runs in a Player. It elaborates `Zenith2/Docs/Contracts.md` → "Compatibility policy per kind" and `Zenith2/Docs/Architecture.md` → "Determinism guarantee and mechanisms" for authoring data and never restates their bullets. A detail designed here beyond the plan is marked `<!-- elaboration -->`; an interface assumed from an area that has not declared it is fenced `<!-- assumption: pending <AREA> -->` and recorded in `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md` (rows WD1 to WD34). The requirement catalogue is `Zenith2/Docs/Requirements/WORLD.md`.

This entry document holds the skeleton and the top-level contracts; seven section files hold the rest ("Document family").

## Purpose and scope

### What WORLD owns

| Piece | Where | Specified in |
|---|---|---|
| editable world partitions: the world document, authoring regions, region documents and their placement rules | ZenithAuthoring, ZenithEditor | `Zenith2/Docs/Design/WORLD/Documents.md` → "World document", "Region documents" |
| gameplay layers and data layers: layer kinds, states, realisations, per-layer bake participation, the editor's layer view | ZenithAuthoring, ZenithEditor | `Zenith2/Docs/Design/WORLD/Documents.md` → "Layer objects"; `Zenith2/Docs/Design/WORLD/Partition.md` → "Layer realisation" |
| the partition cook: the importer `world.world`, grid assignment, the static collider split, cross-cell reference nulling, persistent markers, cell and layer scene documents, the streaming world document, products and their derived identities | ZenithCook, ZenithAuthoring | `Zenith2/Docs/Design/WORLD/Partition.md` |
| concurrent editing granularity: the file and keyed child each edit touches, merge through CONTENT's structural driver, region moves, terrain tiles per region, advisory locks, merge policies, re-derivation after merges | ZenithAuthoring, ZenithEditor | `Zenith2/Docs/Design/WORLD/Editing.md` → "Concurrent editing granularity" |
| reproducible procedural generation: the generator framework, versions, execution modes, seeds, output identity, four built-in generators, staleness, goldens | ZenithAuthoring | `Zenith2/Docs/Design/WORLD/Generation.md` |
| baking workflows: units, fingerprints, the planner, execution, the report; the lighting bake (atlas, BVH, tracer, irradiance volumes, baked probes, documents), the navigation workflow (navigation input scenes and derived navigation documents inside the import), HLOD generation (nodes, eligibility, material modes, merge recipes) | ZenithAuthoring, ZenithEditor, ZenithCook | `Zenith2/Docs/Design/WORLD/Bakes.md` |
| the World and World Bakes panels, region loading and projection, WORLD's commands and automation steps | ZenithEditor | `Zenith2/Docs/Design/WORLD/Editing.md` |
| every WORLD test, fixture and golden | test targets | `Zenith2/Docs/Design/WORLD/Tests.md` |

### What WORLD does not own

| Not owned | Owner | Why |
|---|---|---|
| runtime streaming: grids at run time, cells, the cell state machine, loading, admission, cancellation, delta holders, `Zenith.Streaming.CellStatus`, region flags, origin rebasing, HLOD selection and extraction | STREAM | "Boundary with STREAM" below |
| scene and prefab documents, edits, resolution, authoring-ID derivation, cooking scenes, additive loads | SCENE | WORLD writes scene documents through SCENE's API (`Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_SceneDocument header", "Zenith_SceneEdit header") |
| the editor shell, documents, commands, undo, the cook client, source control, the migration UI | EDIT | WORLD is an area extension (`Zenith2/Docs/Design/EDIT/Panels.md` → "Area-owned panels") |
| importers, cook steps, keys, the DDC, the cook server, asset kinds | ASSET | WORLD implements one importer against ASSET's interface (`Zenith2/Docs/Design/ASSET/Import.md` → "Zenith_Importer") |
| terrain, foliage, placement algorithm, collision and navigation extraction, ENV's own bake | ENV | WORLD orchestrates (`Zenith2/Docs/Design/ENV.md` → "Boundary with WORLD"; `Zenith2/Docs/Design/ENV/Procedural.md` → "Boundary with WORLD procedural generation"); ENV writes its cell content through WORLD's writer |
| lighting formats, their loaders and cook steps, run-time GI evaluation, realtime probe capture | LIGHT | WORLD's bake feeds LIGHT's writers (`Zenith2/Docs/Design/LIGHT/GI.md` → "Bake output contract") |
| navmesh tiles: their bake stages, format, product declaration, step and runtime | AI | WORLD derives AI's navigation inputs and documents inside its import and calls AI's declaration function and cell contribution (`Zenith2/Docs/Design/AI/Navigation.md` → "Streamed tiles"; row WD23) |
| mesh processing: simplification, merging, quantisation, the mesh format; engine materials | GEOM | HLOD meshes are GEOM meshes built from WORLD's merge recipes (row WD8) |
| the semantic merge driver | CONTENT | `Zenith2/Docs/Design/CONTENT/Merge.md` → "Merge API"; WORLD supplies canonical schemas (rows WD16, WD27) and merges its documents only through it |
| release refusal of stale bakes | PACK | row WD14 (b); region documents are input-only and never packaged (row WD28 withdraws WD14 (a)) |
| any run-time WORLD library, Luau binding, misuse code or asynchronous source kind | none | WORLD has no Player-side code: every run-time effect of a world is STREAM's, SCENE's or the content owners' |

Excluded outright: dynamic plugins registering generators (Z2-D-024); mod-supplied worlds, regions or generators (Z2-D-075); C++ in a game's world content (Z2-D-002).

### Boundary with STREAM

WORLD authors and cooks; STREAM runs. The contract between them is STREAM's runtime contract, which WORLD writes to and never re-specifies:

| STREAM heading WORLD writes to | What WORLD produces for it |
|---|---|
| `Zenith2/Docs/Design/STREAM/Formats.md` → "Streaming world document" | the primary product's intermediate `stream.worldDocument`, a document in exactly that schema, generated from the world document's grids and the partition (`Zenith2/Docs/Design/WORLD/Partition.md` → "Streaming world document generation") |
| `Zenith2/Docs/Design/STREAM/Formats.md` → "Cell scene rules" | one cell scene per non-empty cell: the anchor root, entities within the overlap margin, persistent markers, cross-cell references nulled and listed (`Zenith2/Docs/Design/WORLD/Partition.md` → "Cell scene documents"); answers STREAM's row ST13 |
| `Zenith2/Docs/Design/STREAM/Formats.md` → "Content of each grid role" | grid assignment by role and the static collider split (`Zenith2/Docs/Design/WORLD/Partition.md` → "Grid assignment", "Static collider split") |
| `Zenith2/Docs/Design/STREAM/Formats.md` → "HLOD node records" | the HLOD node hierarchy and its meshes (`Zenith2/Docs/Design/WORLD/Bakes.md` → "HLOD workflow") |
| `Zenith2/Docs/Design/STREAM/Runtime.md` → "Cell identity across re-cooks" | cell scene GUIDs derived from (world, grid, coordinate), stable across every re-cook (`Zenith2/Docs/Design/WORLD/Partition.md` → "Product identity") |
| `Zenith2/Docs/Design/STREAM/PublicAPI.md` → "Zenith_StreamingComponents header" | the authored values of `Zenith.Streaming.CellAnchor` and `Zenith.Streaming.Persistent`; WORLD never writes `Zenith.Streaming.CellDelta`, `Zenith.Streaming.CellStatus` or any property of a source; a layer's initial state reaches sources through the grid member `defaultActive` STREAM applies (row WD19) |

STREAM's cells, its streaming world document and its runtime grids are therefore outputs of WORLD's cook, while WORLD's authoring partition — the region grid of `regionSize`, the layers, the generators — never reaches the runtime: a region size, a layer boundary or an editor load radius changes no runtime behaviour except through the cells the partition derives (Z2-I-WORLD-09). STREAM's row "world partition authoring, data layers, the partition cook that writes cell scenes, the HLOD and navigation bakes → WORLD" (`Zenith2/Docs/Design/STREAM.md` → "What STREAM does not own") is this section.

### Inventory items covered

WORLD covers every item of `Zenith2/Docs/Requirements/Inventory.md` → "Inventory v1", table "WORLD World authoring", all required at S4: `Z2-I-WORLD-01` (editable world partitions), `-02` (gameplay layers), `-03` (data layers), `-04` (concurrent editing granularity), `-05` (reproducible procedural generation), `-06` (baking workflow: lighting; baked GI evaluation itself is LIGHT's at S5), `-07` (baking workflow: navigation; D-SEL-NAV, open, Stage 1/2), `-08` (baking workflow: HLOD), `-09` (authoring contracts separate from runtime streaming). The mapping is `Zenith2/Docs/Requirements/WORLD.md` → "Coverage of inventory items". WORLD owns no item of another area.

### HLOD ownership

WORLD owns HLOD **generation**: choosing nodes, eligible instances, material modes, triangle targets and error bounds, and writing one merge recipe per node mesh (`Zenith2/Docs/Design/WORLD/Bakes.md` → "HLOD workflow"). GEOM owns the mesh **processing** a recipe asks for (merging, simplification, quantisation, the `.zmesh` format) inside its `geom.mesh` step, because GEOM's design scopes itself to LOD within a mesh and states that world-scale LOD is STREAM's HLOD, not GEOM's mesh LOD (`Zenith2/Docs/Design/GEOM.md` → "Design rulings owned by GEOM"), STREAM owns HLOD **selection and display** at run time (`Zenith2/Docs/Design/STREAM/Runtime.md` → "HLOD selection and extraction") and names the HLOD bake as WORLD's, and the inventory places "baking workflow: HLOD" in WORLD (Z2-I-WORLD-08).

### Document family

| File | Holds | Its `##` headings |
|---|---|---|
| `Zenith2/Docs/Design/WORLD/Documents.md` | every authoring document | Scope of this file · Document set · Common document rules · World document · Layer objects · Region documents · Generator document · Baked lighting documents · Derived navigation documents · World validation messages · Document limits · Document type registration |
| `Zenith2/Docs/Design/WORLD/Partition.md` | the partition cook | Scope of this file · The world importer · Importer inputs · Partition algorithm · Grid assignment · Static collider split · Layer realisation · Cross-cell references · Persistent markers · Contributed cell content · Cell scene documents · Streaming world document generation · Unstreamed worlds · Products and keys · Partition determinism · Partition limits |
| `Zenith2/Docs/Design/WORLD/Generation.md` | procedural generation | Scope of this file · Generator terms · Generator interface and registry · Execution modes · Seed derivation · Output identity · Running a generator · Built-in generators · Obstacles and dependencies · Generator outputs and staleness · Generator determinism · Golden outputs |
| `Zenith2/Docs/Design/WORLD/Bakes.md` | the baking workflows | Scope of this file · Workflow overview · Bake units · Input fingerprints · Bake planner · Bake execution · Bake report format · Lighting workflow · Navigation workflow · HLOD workflow |
| `Zenith2/Docs/Design/WORLD/Editing.md` | the editor and concurrency | Scope of this file · World editor extension · World panel · Region projection · Region loading · World commands · Partition preview · Bake panel and commands · Play in the editor · Concurrent editing granularity · Merge conflicts in WORLD documents · After a merge |
| `Zenith2/Docs/Design/WORLD/PublicAPI.md` | every declaration | Header list · Limits and constants · Error and diagnostics types · Identity and keys · World description types · World document functions · Document type registration functions · Source access interface · Cell content writer · Role rules · Partition functions · Generator types · Fingerprint builder · Bake planning functions · Lighting bake functions · HLOD functions · Navigation document functions · Importer declarations · Editor declarations |
| `Zenith2/Docs/Design/WORLD/Tests.md` | every test | Test targets · Fixtures and generated content · Document tests · Identity tests · Partition tests · Generator tests · Bake tests · Importer tests · Editor tests · Integration tests · Gate steps per test group |

### Design choices at a glance

<!-- elaboration -->
Each choice below is a design decision beyond the plan's Appendix-A row; the section that specifies it carries an `<!-- elaboration -->` marker.

<!-- informative -->
| Choice | Taken | Rejected alternative and why |
|---|---|---|
| region documents | SCENE scene documents, one per (layer, authoring region), transforms relative to the region anchor, read only as inputs of the world's importer (no `.meta`, no standalone cook) | a WORLD-specific region type: it would need its own edits, undo, prefab handling and merge policy, all of which SCENE and EDIT already provide for scenes; one file per entity (UE5's one-file-per-actor): finer locking, but tens of thousands of files per world and no prefab-instance grouping |
| when the partition runs | at import, as ASSET's importer `world.world`, with every output an intermediate in the DDC and no generated source file | an editor command writing generated cell scenes into the source tree: every concurrent edit would conflict on the shared streaming document and on generated cells, and a forgotten regeneration would ship stale cells |
| product identity | GUIDs derived from (world, product key) by SHA-256, never written into the `.meta` | `.meta` sub-asset rows: thousands of rows per world, a merge hotspot on every new cell, and a build cook that fails on a missing row |
| layers | one model with two kinds: data layers (authoring organisation, editor-only or baked) and gameplay layers (run-time switchable, realised as a STREAM grid or an additive scene) | a WORLD run-time layer system: it would duplicate STREAM's grids and SCENE's additive set and add run-time state to save |
| lighting bake | explicit, from the editor or an automation script, writing tracked bake documents with a recorded input fingerprint; a CPU path tracer, deterministic per texel | baking inside every cook: a lamp move would re-trace minutes of lightmaps before play-in-editor; a GPU bake: non-deterministic across drivers and impossible headless on the gate hosts |
| navigation and HLOD | inside the cook, keyed per tile and per node | explicit bakes: stale navmesh and stale proxies are visible defects that a cook can always avoid cheaply |
| procedural generation | pure, versioned generators with counter-based seeds, cook or baked mode | rate-style operations (the old engine's Flatten brush): results depending on how often they ran |
<!-- /informative -->

### Reconciliations with other documents

Additions WORLD needs from documents it cannot edit; each is fenced in this family by an assumption block and carried by its cross-area row; a core-document need is also a `CROSS_DOC_ITEMS` row.

| # | Need | Owner | Row | Core-doc item |
|---|---|---|---|---|
| R1 | `Zenith_ImportContext::OpenSourceDocument`, `OpenInputDocument`, `OpenInputDocumentByGUID` for importers | ASSET | WD1 | — |
| R2 | `ReadSourceProducts` as an import input (its `ReadAssetImportKey` is withdrawn: no cook or import key is a bake input, R31) | ASSET | WD2 | — |
| R3 | input-only documents (generator, light bake and light page documents): classified sources with no `.meta` and no product, read by their world's importer | ASSET | WD3 | — |
| R4 | `DeclareDerivedProduct`: importer-derived product GUIDs outside the `.meta` | ASSET | WD4 | — |
| R5 | `Zenith_RegisterWorldImporters` in cook services item 4 | ASSET | WD5 | — |
| R6 | SCENE's `scene.scene` cooks a product scene from the intermediate `scene.document`; moved IDs are accepted across documents | SCENE | WD6 | — |
| R7 | STREAM's `stream.world` reads the intermediate `stream.worldDocument`; `Zenith_ValidateStreamingGridObject`; `Streaming.GetGridIndex` | STREAM | WD7 | — |
| R8 | GEOM's mesh step cooks merge recipes; the published `geom.sourceMesh`; the HLOD proxy material | GEOM | WD8 | — |
| R9 | GEOM's CPU cooked-mesh reader `Zenith_ReadCookedMeshGeometry`; the texture source-image writer (the LOD 0 digest extends it, R30) | GEOM | WD9 | — |
| R10 | LIGHT's ASSET-phase steps of kinds 29 to 31 over WORLD's intermediates; `Cook_EncodeLightmapPage` (answers LIGHT row L13) | LIGHT | WD10 | — |
| R11 | AI adopts `Zenith_WorldCellBounds` and `Zenith_WorldCellContentWriter`; `Zenith_AIMakeNavigationManifestView` (answers AI11; its tracked per-region documents are replaced by R23) | AI | WD11 | — |
| R12 | WORLD's parser-limit rows; WORLD's document types in [Z2-N-CONTRACT-552]'s list (applied by core-doc patch h; X303 (1) withdrawn: the per-owner growing `DEPENDS` of X301 covers WORLD's edges) | ARCH, CONTRACT | WD12 | X303 (2), (3); X341 (applied) |
| R13 | ENV's published cell content (`environment.cellContent.<x>_<z>`, Z2T-ENV-102; `Zenith_EnvironmentWriteCellContent` withdrawn by ruling WORLD-WDB9-WDB6) with the stable names and property IDs of `Zenith_TerrainTileComponent` and `Zenith_FoliageCellComponent`; the stable names of AUDIO's and AI's spatial components WORLD's role rules name | ENV, AUDIO, AI | WD13 | — |
| R14 | releases refused while bakes are stale or missing, read from the cook report's replayed diagnostic codes (WD14 (b) as rewritten; answered by PACK's PK26; WD14 (a) withdrawn by R28) | PACK | WD14 | — |
| R15 | the document-type registration order; the bake panel's orchestration (answers ED18, ED15) | EDIT | WD15 | X257 (2) as amended by X301 |
| R16 | per-type merge policies for WORLD's documents | CONTENT | WD16 | — |
| R17 | WORLD's pending budget rows Z2-B-WORLD-001 to -005 and their producer (applied by core-doc patch h) | Budgets | WD17 | X304 (applied) |
| R18 | proxies of static colliders collide as the originals | PHYS | WD18 | — |
| R19 | the grid member `defaultActive`; a grid with no cell; the role table's trigger, layer-collider and explicit-static-body rows (the source half of WD19 (b) is superseded by R32) | STREAM | WD19 | — |
| R20 | `Zenith.Physics.ColliderProxy`: events and hits reported as `Source`, the proxy's body removed when `Source` is destroyed; the "Region freeze" sentence on triggers | PHYS | WD20 | — |
| R21 | region and generated region documents as input-only documents; the importer's kind 15 (WD21 (b) `ReadCookItemKey` and the `cookKey` request, and WD21 (c) the derived-data blobs, are withdrawn by R31) | ASSET | WD21 | — |
| R22 | input-only scene documents whose `id` names no asset | SCENE | WD22 | — |
| R23 | `Zenith_AIDeclareNavigationProducts`; scene entries with `offset`; volumes with an explicit `index`; `ai.navMesh` reading cooked scene products | AI | WD23 | — |
| R24 | WORLD's ZenithAuthoring owner row (edges, fixture paths); input-only document create and delete; the projection anchor and edit-world origin shift (refines WD15 (b)); LFS lock warnings for terrain tiles; WD15 (f) withdrawn | EDIT | WD24 | — |
| R25 | probe face size and pre-exposure; volume and probe placement relative to their entities | LIGHT | WD25 | — |
| R26 | `uENV_NAVIGATION_GEOMETRY_VERSION` | ENV | WD26 | — |
| R27 | the merge-policy row of WORLD's derived navigation documents withdrawn (none exist as files) | CONTENT | WD27 | — |
| R28 | WD14 (a) withdrawn: no region scene is an asset | PACK | WD28 | — |
| R29 | the deterministic transcendentals WORLD calls (ruling X314-FMA3; answered by CORE: `Zenith2/Docs/Design/CORE/Maths.md` → "Deterministic transcendental functions") | CORE | WD29 | X314 |
| R30 | the published mesh property `geom.lod0Summary` (LOD 0 digest and bounding sphere) and the digest function `Zenith_ComputeMeshGeometryDigest` (extends WD9) | GEOM | WD30 | — |
| R31 | `ReadPublishedProperty` and `OpenInputDocumentRelativeTo` for importers; the cook-server request `property`; WD21 (b), (c) withdrawn | ASSET | WD31 | — |
| R32 | a source's override pair `GridForcedOn`, `GridForcedOff` over the world's default mask, superseding the source half of WD19 (b) (extends WD19) | STREAM | WD32 | — |
| R33 | `Zenith_Place` computes rotations with CORE's deterministic sine and cosine; terrain loading through a WORLD tile source; an empty water span accepted by `Zenith_ExtractNavigationGeometry` | ENV | WD33 | X314 |
| R34 | the home of `Zenith_AIDeclareNavigationProducts` in ZenithAuthoring with two callers; `terrains` entries by GUID; terrain-only documents (extends WD23) | AI | WD34 | — |
| R35 | the tools-host job priority exempt from the 2 ms slice: the ARCH half landed as [Z2-N-ARCH-443]; JOBS declared it (`Zenith2/Docs/Design/JOBS/Algorithms.md` → "Tools-host background priority", answer row J25: accepted by a job system created with `m_bAcceptToolBackground`) | JOBS | J25 (answer) | X350 (ruled (a)) |
| R36 | importer diagnostics carry a code, and every import's diagnostics are persisted and replayed in every cook report | ASSET | WD35 | — |

## Dependencies

### Libraries WORLD code links

| Target | WORLD's files | Edges WORLD needs | Notes |
|---|---|---|---|
| `ZenithAuthoring` (static tool library, `ZENITH_TOOLS_HOST`) | `Zenith_AuthoringWorld.h`, `Internal/WORLD/**`, `Tests/WORLD/**`, `Tests/Fixtures/WORLD/**`, `Tests/Fixtures/Migration/WORLD/**` | ZenithReflect, ZenithDocument, ZenithScene, ZenithAssets, ZenithStreaming, ZenithEnvironment, ZenithCompression (the root header's includes) and ZenithJobs (WORLD's tests), each appended by WORLD's move ticket to the library's `DEPENDS` when the list on the `master` it branches from does not hold it (`Zenith2/Docs/Design/EDIT.md` → "ZenithAuthoring contract rules" rule 4; no edge is claimed as already declared; row WD24 completes EDIT's owner row) | no third-party library: zstd is reached through `Zenith_Compression` |
| `ZenithCook` (tool) | `Internal/World/**`, `Tests/World/**` | ZenithAuthoring (declared by the first owner's move, `Zenith2/Docs/Design/EDIT.md` → "Tool-side code the editor needs") | the importer reaches LIGHT's writers in ZenithCook's own `Internal/Lighting/` |
| `ZenithEditor` (tool) | `Internal/World/**`, `Tests/World/**` | ZenithAuthoring, ZenithEditorCore, ZenithStreaming (already listed) | ImGui only inside ZenithEditor |
| `Zenith_Integration_WorldAuthoring` | `Zenith2/Tests/WorldAuthoring/` | ZenithRuntime, ZenithStreaming, ZenithScene, ZenithECS_Custom, ZenithPhysics, ZenithPhysics_Jolt, ZenithAuthoring, ZenithContentCore (CONTENT's merge API) | `ZENITH_TOOLS_HOST` |

### Headings this design relies on

| Heading | For |
|---|---|
| `Zenith2/Docs/Design/STREAM/Formats.md` → "Streaming world document", "Cell scene rules", "Grid object", "HLOD node records", "Worst-case active cells", "Validation messages", "Limits"; `Zenith2/Docs/Design/STREAM/Runtime.md` → "Cell identity across re-cooks", "HLOD selection and extraction", "Region states and coordinated subsystems"; `Zenith2/Docs/Design/STREAM/PublicAPI.md` → "Zenith_StreamingComponents header", "Script bindings" | the runtime contract WORLD writes |
| `Zenith2/Docs/Design/SCENE/Format.md` → "Scene document schema", "Resolved ID space"; `Zenith2/Docs/Design/SCENE/Prefabs.md` → "Derivation", "Resolution algorithm", "Scene and prefab cook steps", "Document edits for the editor", "Sort keys for edits"; `Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_SceneDocument header", "Zenith_SceneEdit header", "Zenith_PrefabResolver header", "Zenith_AuthoringID header"; `Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "Canonical rules", "Zenith_MigrationRegistry header", "Journaled write", "Storage-root writer lock" | documents, identities, writes |
| `Zenith2/Docs/Design/ASSET/Import.md` → "Zenith_Importer", "Zenith_ImportContext", "Import algorithm", "Intermediates"; `Zenith2/Docs/Design/ASSET/Cook.md` → "Zenith_CookContext", "The cook server", "Output tree and database locks"; `Zenith2/Docs/Design/ASSET/Database.md` → "Sub-asset identity", "What is an asset source"; `Zenith2/Docs/Design/ASSET.md` → "Asset kinds" | the importer and its products |
| `Zenith2/Docs/Design/EDIT.md` → "ZenithAuthoring contract rules", "Library creation ticket"; `Zenith2/Docs/Design/EDIT/Foundation.md` → "Documents", "Commands and history", "Editor frame", "Editor-originated writes", "Session object"; `Zenith2/Docs/Design/EDIT/Panels.md` → "Extension interface", "Area-owned panels"; `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Source control", "Restoring a backup", "Merge-friendly files"; `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Bake panel" | the editor side |
| `Zenith2/Docs/Design/JOBS/PublicAPI.md` → "Zenith_JobSystem header", "Zenith_JobContext header"; `Zenith2/Docs/Design/CONTENT/Merge.md` → "Merge API", "Merge policies"; `Zenith2/Docs/Design/ECS/Reflect.md` → "Zenith_TypeInfo header"; `Zenith2/Docs/Design/PHYS/Simulation.md` → "Region freeze", "Cell bodies" | scheduling, merging, the split's exclusions, collision streaming |
| `Zenith2/Docs/Design/ENV/Procedural.md` → "Zenith_ProceduralPlacement header", "Seeded streams", "Boundary with WORLD procedural generation"; `Zenith2/Docs/Design/ENV/Bake.md` → "Cell content records", "Navigation geometry extraction" | placement and terrain content |
| `Zenith2/Docs/Design/LIGHT/GI.md` → "Bake output contract", "Lightmap set cooked format", "Irradiance volume cooked format", "Indirect diffuse selection"; `Zenith2/Docs/Design/LIGHT/Reflections.md` → "Reflection probe cooked format"; `Zenith2/Docs/Design/LIGHT/Lighting.md` → "Light units"; `Zenith2/Docs/Design/LIGHT.md` → "Light component schemas" | the lighting bake |
| `Zenith2/Docs/Design/AI/Navigation.md` → "Bake inputs", "Streamed tiles"; `Zenith2/Docs/Design/AI/Formats.md` → "Navigation document schema" | the navigation workflow |
| `Zenith2/Docs/Design/GEOM/Meshes.md` → "Mesh cook pipeline", "LOD generation"; `Zenith2/Docs/Design/GEOM/Textures.md` → "Texture source image intermediate"; `Zenith2/Docs/Design/PHYS/ComponentsAPI.md` → "Zenith_PhysicsComponents header"; `Zenith2/Docs/Design/RPROD/Extractors.md` → "Render component registration" | HLOD, the bake's geometry, the collider split |
| `Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_Sha256", "Zenith_RandomSource", "Zenith_Hash64", "Deterministic transcendental functions" | fingerprints, identities, seeds, tool-output determinism |
| `Zenith2/Docs/Contracts.md` → "Compatibility policy per kind", "Contract blocks per type", "Errors and results", "Tests and code hygiene gates"; `Zenith2/Docs/Architecture/BuildSystem.md` → "Tools, benchmarks and integration suites", "Test, conformance and fuzz targets" | the contracts every WORLD type obeys |

### Upward-callback interfaces WORLD declares

WORLD's code is tool code with no dependant below it, so the interfaces it declares are implemented beside it, never by a lower library: `Zenith_WorldSourceAccess` (implemented by the importer, the editor and tests), `Zenith_WorldCellContentWriter` (implemented by WORLD, called by ENV's and AI's ZenithAuthoring functions in the same library), `Zenith_WorldCellContributor` (implemented by the importer's two production contributors over ENV's and AI's functions, and by tests), `Zenith_WorldMeshGeometrySource` (implemented by the editor and tests) and `Zenith_WorldGenerator` (implemented by the built-ins). WORLD implements ASSET's `Zenith_Importer`, SCENE's `Zenith_PrefabProvider` (`Zenith_WorldImportPrefabProvider`), ENV's `Zenith_PlacementSurface` (`Zenith_WorldPlanePlacementSurface`) and EDIT's `Zenith_EditorExtension`, `Zenith_EditorPanel` and `Zenith_EditorCommand`.

### Areas that consume WORLD

| Consumer | Uses |
|---|---|
| STREAM | the streaming world documents and cell scenes WORLD's importer produces (row ST13) |
| AI | `Zenith_WorldCellContentWriter` and `Zenith_WorldCellBounds` for its cell contribution (row WD11); ENV's cell content is placed by WORLD from ENV-102's published properties (ruling WORLD-WDB9-WDB6) |
| LIGHT | the baked lighting intermediates its steps cook (row WD10) |
| GEOM | the HLOD merge recipes its mesh step cooks (row WD8) |
| EDIT | WORLD's extension; the bake panel's orchestration (row WD15) |
| PACK | the stale-bake diagnostics (row WD14 (b)) |
| GAME | the layer grid names and indices (the import report, `Streaming.GetGridIndex`) and their `defaultActive` in the streaming world; OpenWorldSlice's world |

## Library and file layout

### ZenithAuthoring files

| Path (under `Zenith2/Tools/Authoring/`) | Contents |
|---|---|
| `Zenith_AuthoringWorld.h` | every declaration of `Zenith2/Docs/Design/WORLD/PublicAPI.md` except the importer's and the editor's |
| `Internal/WORLD/Zenith_WorldDocumentTypes.cpp` | `Zenith_RegisterWorldDocumentTypes`, the four canonical schemas |
| `Internal/WORLD/Zenith_WorldDocumentReader.cpp` | `Zenith_ReadWorldDocument`, `Zenith_ValidateWorldDocument`, the reader rules (W1 to W4, W6 to W12, W15, W16, W18, W19, W25, W26, W28, W29) |
| `Internal/WORLD/Zenith_WorldTerrain.cpp` | `Zenith_WorldTerrainTileSource`, `Zenith_WorldLoadTerrain` |
| `Internal/WORLD/Zenith_WorldIdentity.cpp` | key and ID functions |
| `Internal/WORLD/Zenith_WorldDiagnostics.cpp` | the collector, `Zenith_ToString` tables |
| `Internal/WORLD/Partition/Zenith_WorldPartitionPlan.h`, `.cpp` | steps 1 to 13 of "Partition algorithm", the importer rules |
| `Internal/WORLD/Partition/Zenith_WorldRoleRules.cpp` | `Zenith_GetWorldRoleRules` |
| `Internal/WORLD/Partition/Zenith_WorldColliderSplit.cpp` | the static collider split |
| `Internal/WORLD/Partition/Zenith_WorldReferences.cpp` | reference nulling and records |
| `Internal/WORLD/Partition/Zenith_WorldCellWriter.cpp` | cell, layer and content scene documents; the cell content writer |
| `Internal/WORLD/Partition/Zenith_WorldStreamDocument.cpp` | `Zenith_WriteWorldStreamDocument` |
| `Internal/WORLD/Generation/Zenith_WorldGenerators.cpp` | the registry, `Zenith_RunWorldGenerator`, obstacles, fingerprints |
| `Internal/WORLD/Generation/Zenith_WorldGeneratorLattice.cpp`, `…Spline.cpp`, `…Scatter.cpp`, `…ProbeGrid.cpp` | the built-ins |
| `Internal/WORLD/Bakes/Zenith_WorldBakePlanner.cpp` | units, the planner, the report writer |
| `Internal/WORLD/Bakes/Zenith_WorldFingerprint.cpp` | the builder, the lighting fingerprint |
| `Internal/WORLD/Bakes/Zenith_WorldLightAtlas.cpp`, `Zenith_WorldBvh.h`, `.cpp`, `Zenith_WorldTracer.h`, `.cpp`, `Zenith_WorldLightBake.cpp`, `Zenith_WorldLightBakeDocuments.cpp` | the lighting bake |
| `Internal/WORLD/Bakes/Zenith_WorldHlod.cpp` | HLOD nodes and recipes |
| `Internal/WORLD/Bakes/Zenith_WorldNavigationDocuments.cpp` | navigation input scenes and derived navigation documents |
| `Tests/WORLD/*.cpp`, `Tests/WORLD/Support/*.h` (header-only), `Tests/Fixtures/WORLD/Golden/*.json`, `Tests/Fixtures/Migration/WORLD/<Type>/` | `Zenith2/Docs/Design/WORLD/Tests.md` |

Every source is platform-independent: WORLD has no `Internal/Windows/` file (it calls no platform API; file writes go through SCENE's framework).

### ZenithCook files

| Path (under `Zenith2/Tools/ZenithCook/`) | Contents |
|---|---|
| `Internal/World/Zenith_WorldImporter.h`, `.cpp` | the importer, its source access, the two production contributors, `Zenith_RegisterWorldImporters` |
| `Internal/World/Zenith_WorldImportPrefabProvider.cpp` | the prefab provider over `OpenInputDocumentByGUID` |
| `Internal/World/Zenith_WorldLightingImport.cpp` | the lighting intermediates of `Zenith2/Docs/Design/WORLD/Bakes.md` → "Lighting import" |
| `Tests/World/*.cpp` | importer tests and the fixture setup case |

### ZenithEditor files

`Zenith2/Tools/ZenithEditor/Internal/World/`: the headers of `Zenith2/Docs/Design/WORLD/PublicAPI.md` → "Editor declarations" and their sources; `Zenith2/Tools/ZenithEditor/Tests/World/` (EDIT's extension table row for `Internal/World/`, `Zenith2/Docs/Design/EDIT/Panels.md` → "Extension table").

### Supported matrix

| Target | `ZENITH_SUPPORTED` |
|---|---|
| `ZenithAuthoring`, `ZenithCook`, `ZenithEditor` and their tests | `ZENITH_TOOLS_HOST` (`win`) |
| `Zenith_Integration_WorldAuthoring` | `ZENITH_TOOLS_HOST` |

No WORLD source builds for Android or in a sanitizer preset; no Player's Shipping link closure contains it ([Z2-N-ARCH-241]).

## Public API

### API map

The full declarations are `Zenith2/Docs/Design/WORLD/PublicAPI.md`. The top-level contracts:

| Type or function | Contract in one line |
|---|---|
| `Zenith_ReadWorldDocument` | parses and validates a world document into `Zenith_WorldDesc`, reporting every finding of the reader rules (the importer rules are the partition's) |
| `Zenith_RegisterWorldDocumentTypes` | registers WORLD's four document types and canonical schemas with a migration registry |
| `Zenith_WorldSourceAccess` | how WORLD's functions read documents, product lists and mesh geometry digests: the importer's, the editor's and tests' implementations |
| `Zenith_WorldLoadTerrain`, `Zenith_WorldTerrainTileSource` | ENV's terrain document loaded through the source access, every tile a recorded input |
| `Zenith_PlanWorldPartition` | the partition: a pure, single-threaded function of the opened documents, returning a plan whose writers produce every intermediate |
| `Zenith_WorldCellContentWriter` | how ENV and AI add entities to a cell |
| `Zenith_WorldGenerator`, `Zenith_PrepareWorldGenerator`, `Zenith_RunWorldGeneratorRegion` | the generator interface, one generator document's plan and its region jobs (work items) |
| `Zenith_WorldFingerprintBuilder` | SHA-256 fingerprints of bake inputs |
| `Zenith_PlanWorldBakes` | the bake units, their states, order and selection |
| `Zenith_PrepareWorldLightingUnit`, `Zenith_TraceWorldLightingItem`, `Zenith_FinishWorldLightingUnit` | trace one lighting unit deterministically as independent work items |
| `Zenith_BuildWorldHlodNodes` | the HLOD node hierarchy and recipes |
| `Zenith_DeriveWorldNavigationDocument` | one navigation region's AI navigation document, in memory |
| `Zenith_WorldImporter` | ASSET's importer `world.world` over the functions above |
| `Zenith_WorldEditorExtension` | the editor extension |

### Rules across the WORLD API

- Every fallible function returns `Zenith_Result<T, Zenith_WorldError>`; document content never reaches an assert ([Z2-N-CONTRACT-472]); every finding carries its document path and JSON pointer.
- Every result that reaches a document or an intermediate is a pure function of its inputs: no clock, thread count, address, hash-map iteration order or host path ([Z2-N-ARCH-884] in spirit, for tool output).
- Every persisted identity and fingerprint uses SHA-256; `Zenith_Hash64` appears only in random streams, whose outputs are generated content.
- WORLD types hold no lock and no global state; the editor's objects are `MAIN_THREAD`; the partition, generator and bake functions are `OWNER_THREAD`, single-threaded, and never start, join or wait on a job. Parallelism is the caller's: WORLD exposes independent work items (generator region jobs, lighting work items) that write disjoint slots; the importer runs them inline on its import thread, the editor submits each as a background job at the tools-host priority `JOB_PRIORITY_BACKGROUND_TOOL` of [Z2-N-ARCH-443] (ruling X350 (a); `Zenith_JobSystem::SubmitBackground`; declared by JOBS in `Zenith2/Docs/Design/JOBS/Algorithms.md` → "Tools-host background priority" and accepted by a job system created with `Zenith_JobSystemDesc::m_bAcceptToolBackground`), at most `2 × GetMaxConcurrentToolBackground()` in flight per operation, and observes completion from `Poll`, and tests drive them through `RunRoot` with a `ParallelFor` (`Zenith2/Docs/Design/JOBS/PublicAPI.md` → "Zenith_JobSystem header"). The editor's main thread runs no WORLD computation except the partition preview's per-frame assignment, bounded by `uWORLD_EDITOR_PREVIEW_ROOTS_PER_FRAME`: the partition, generator prepare, region jobs with their generated documents' bytes, light-bake job building, prepare, tracing and encoding all run in those jobs (`Zenith2/Docs/Design/WORLD/Editing.md` → "World editor extension", "Partition preview").

### Handle kinds used

| Handle | Where |
|---|---|
| `Zenith_PersistentID` | world, layer, region, entity, generator, product and bake identities ([Z2-N-CONTRACT-066]) |
| `Zenith_AssetGUID` | products, meshes, materials, prefabs, terrains |
| `Zenith_EditorDocumentID` | the editor's references to open WORLD and region documents |
| `Zenith_OperationHandle` | the editor's background operations hold one per submitted work item (`Zenith2/Docs/Design/WORLD/Editing.md` → "World editor extension") |

### Configuration keys

WORLD registers no runtime configuration key (it has no runtime). Its editor preferences are `world.editor.loadRadiusRegions`, `world.editor.unloadRadiusRegions` and `world.editor.previewGrid` (`Zenith2/Docs/Design/WORLD/Editing.md` → "World editor extension"); the importer has no settings.

### Script bindings

None. Gameplay switches layers through STREAM's source override masks `GridForcedOn` and `GridForcedOff` (row WD32) and, in unstreamed worlds, SCENE's `Scene.LoadAdditive`; the bindings WORLD needs are STREAM's (`Streaming.GetGridIndex`, row WD7; the effective-mask read of rows WD19 and WD32).

<!-- assumption: pending STREAM (rows WD19, WD32) -->
STREAM's `Zenith.Streaming.Source` gains `GridForcedOn` and `GridForcedOff`, its grid objects `defaultActive`, and its bindings `Streaming.GetGridIndex` and `Streaming.GetEffectiveGridMask`; none of these names is in STREAM's text today. Until STREAM's patch lands, the integration tests that toggle layers are blocked by it.
<!-- /assumption --> WORLD therefore defines no Luau misuse code and no asynchronous source kind; the misuse block ZS3000 to ZS3099 the orchestrator assigned to WORLD stays unused, and a WORLD binding added at any stage would take its codes from that block through its own cross-document reservation.

## Data structures and formats

### Format index

| Format | Kind | Section |
|---|---|---|
| `*.world.json` (`Zenith.World.WorldDocument` v1) | authoring document | `Zenith2/Docs/Design/WORLD/Documents.md` → "World document" |
| region documents | SCENE scene documents under WORLD's rules | `Zenith2/Docs/Design/WORLD/Documents.md` → "Region documents" |
| `*.generator.json` (`Zenith.World.GeneratorDocument` v1) | authoring document (input-only) | `Zenith2/Docs/Design/WORLD/Documents.md` → "Generator document" |
| `*.lightbake.json`, `*.lightpage.json` (v1) | baked documents (input-only) | `Zenith2/Docs/Design/WORLD/Documents.md` → "Baked lighting documents" |
| derived navigation documents | AI's schema, in memory inside the import | `Zenith2/Docs/Design/WORLD/Documents.md` → "Derived navigation documents" |
| intermediate `scene.document` of `navInput:` products | a canonical SCENE scene document | `Zenith2/Docs/Design/WORLD/Partition.md` → "Navigation input scenes" |
| intermediate `scene.document` | a canonical SCENE scene document | `Zenith2/Docs/Design/WORLD/Partition.md` → "Cell scene documents" |
| intermediate `stream.worldDocument` | STREAM's world document | `Zenith2/Docs/Design/WORLD/Partition.md` → "Streaming world document generation" |
| intermediate `geom.meshMergeRecipe` | GEOM's recipe (row WD8) | `Zenith2/Docs/Design/WORLD/Bakes.md` → "Merge recipes" |
| intermediates `light.bakedLightmapSet`, `light.bakedIrradianceVolume`, `light.bakedProbe` | little-endian binary, versioned by magic and version | `Zenith2/Docs/Design/WORLD/Bakes.md` → "Lighting import" |
| `zenith2.world-bake-report` v1 | untracked JSON report | `Zenith2/Docs/Design/WORLD/Bakes.md` → "Bake report format" |

### In-memory sizes that matter

| Structure | Size | Bound |
|---|---|---|
| the partition's resolved content | about 400 bytes per resolved entity plus its field sets | `uWORLD_MAX_WORLD_ENTITIES`; the pending row "World partition import memory" |
| the world ID index | 32 bytes per entity | the same |
| a BVH | 32 bytes per node (≤ 2 per triangle) and 48 per triangle | `uWORLD_MAX_BAKE_TRIANGLES`: 16,777,216 × (2 × 32 + 48) bytes = 1.75 GiB at the limit (the 2-nodes-per-triangle bound; about 1 GiB with full 4-triangle leaves, 16,777,216 × (0.5 × 32 + 48) bytes); the pending row "Lighting bake tracer memory" |
| a lightmap page in tracing | 4,096² × 32 bytes = 512 MiB at the largest page | one page in flight per unit |

## Algorithms and state machines

### Algorithm index

| Algorithm | Complexity | Section |
|---|---|---|
| world document reading | O(n log n) in members | `Zenith2/Docs/Design/WORLD/Documents.md` → "World validation messages" |
| the partition | O(E log E) | `Zenith2/Docs/Design/WORLD/Partition.md` → "Partition algorithm" |
| grid assignment and the collider split | O(E) | "Grid assignment", "Static collider split" |
| cross-cell references | O(Σ ENTITY properties × log E) | "Cross-cell references" |
| a generator run | per built-in, O(outputs log outputs) plus ENV's placement | `Zenith2/Docs/Design/WORLD/Generation.md` → "Running a generator" |
| bake planning | O(U log U + E) | `Zenith2/Docs/Design/WORLD/Bakes.md` → "Bake planner" |
| atlas packing | O(n log n + n × shelves) | "Atlas packing" |
| BVH construction and tracing | O(T log T) build; O(rays × log T) trace | "BVH construction", "Tracing" |
| HLOD nodes and recipes | O(instances log instances) | "HLOD workflow" |

### State machine index

| Machine | States | Section |
|---|---|---|
| bake unit | FRESH, STALE, MISSING, EDITED, ORPHANED, IN_COOK | `Zenith2/Docs/Design/WORLD/Bakes.md` → "Bake units" |
| bake operation | IDLE, RUNNING, WRITING, CANCELLING, CANCELLED, FAILED, DONE | "Bake execution" |
| baked generator region | FRESH, STALE, MISSING, EDITED, ORPHANED | `Zenith2/Docs/Design/WORLD/Generation.md` → "Generator outputs and staleness" |

### Authoring to runtime flow

```text
author edits ──► region documents, world document, generator documents      (EDIT commands; one file per edit unit)
explicit bakes ─► light bake documents, generated region documents           (World Bakes panel, automation)
cook or cook server:
  import world.world ─► resolve regions + prefabs ─► cook-mode generators ─► classify ─► assign grids/cells
                     ─► collider split ─► references ─► persistence ─► ENV / AI / LIGHT contributions
                     ─► HLOD nodes ─► intermediates: cell scenes, layer scenes, streaming document, recipes, lighting
  steps: scene.scene (cells) · stream.world (.zstream) · geom.mesh (HLOD) · LIGHT/GEOM (lighting) · ai.navMesh (tiles)
runtime (STREAM): bind .zstream ─► stream cells ─► HLOD proxies ─► region flags
```

## Threading lifetime and error contract

### Thread affinity per type

| Type | Affinity |
|---|---|
| `Zenith_WorldDesc`, `Zenith_WorldPartitionPlan`, `Zenith_WorldGeneratorResult`, `Zenith_WorldLightBakeResult`, `Zenith_WorldHlodResult` | `IMMUTABLE` after creation |
| `Zenith_WorldDiagnostics`, `Zenith_WorldFingerprintBuilder`, `Zenith_WorldGeneratorContext` | `OWNER_THREAD` |
| `Zenith_WorldSourceAccess` implementations | `OWNER_THREAD`: the one thread running the current call (the import thread; one job at a time of the editor operation that created it; the test's thread), never two threads at once, never a work item |
| `Zenith_WorldGeneratorPlan`, `Zenith_WorldLightBakeState` | work items for distinct indices may run on any threads concurrently; the caller orders their completion before the fold |
| `Zenith_WorldMeshGeometrySource` | `OWNER_THREAD`: read only by the lighting prepare phase, which copies every triangle |
| `Zenith_WorldImporter` | `ANY_THREAD` (ASSET's importer contract) |
| the editor extension, panels, commands, region loader, background operations | `MAIN_THREAD`; their jobs run at `JOB_PRIORITY_BACKGROUND_TOOL` on the editor's job system, observed from `Poll`, never joined |

### Frame-graph placement

None: WORLD runs no node of any world's frame graph. The editor's WORLD work runs in EDIT's frame steps (region loading in step 2, panels in the shell step) and in background operations; the importer runs in the cook's import phase.

### Lock-order entries

None: WORLD declares no `uLOCK_LEVEL_` constant. Its writes take SCENE's storage-root writer lock through `SaveDocuments`, inside the framework's own ordering (`Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "Storage-root writer lock").

### Cancellation and shutdown

| Situation | Behaviour |
|---|---|
| the editor cancels a bake | no further work item is submitted; running items stop at their next row; nothing of the current unit is written; written units stay FRESH |
| the editor closes during a bake | the session's shutdown cancels the operation, and the job system's `CancelAndDrainBackground` completes its items (EDIT's shutdown sequence); a batch in flight completes or rolls back through its journal |
| the cook server cancels an import | ASSET's item-boundary cancellation; an import is one item, so a cancelled import declares nothing |
| a crash mid-batch | the framework's journal is recovered at the next session or cook start |

### Error enumerations

`Zenith_WorldErrorCode` and `Zenith_WorldWarningCode` (`Zenith2/Docs/Design/WORLD/PublicAPI.md` → "Error and diagnostics types"). At ASSET's boundary every import failure is `IMPORT_ERROR_SOURCE_INVALID` (or `IMPORT_ERROR_OUT_OF_MEMORY`) after one diagnostic per finding, reported through the code-taking `Zenith_ImportContext::Report` (row WD35) so that the cook report entry's `code` is the WORLD code's name and its text starts with it too, which the cook reports as `COOK_ERROR_IMPORT_FAILED`; WORLD adds no `Zenith_CookErrorCode` (ASSET's table is closed to areas, the ENV-F7 shape, `Zenith2/Docs/Design/ENV/Bake.md` → "Cook determinism"). In the editor a WORLD error becomes `EDITOR_ERROR_DOCUMENT` with the WORLD code logged.

### Luau errors versus asserts

WORLD has no Luau surface, so it raises no Luau error. Asserts guard WORLD's own invariants (a span length mismatch from WORLD's own code, a writer called with a cell index out of range); document content, generator parameters and bake files never assert.

### Determinism obligations

1. A world's cooked output is a pure function of its documents, the product lists it reads and the build ("Partition determinism" in `Zenith2/Docs/Design/WORLD/Partition.md`).
2. A generator's output is a pure function of its document, the world seed, its inputs and the build's hash library, independent of worker count, region order and region subset (`Zenith2/Docs/Design/WORLD/Generation.md` → "Generator determinism").
3. A lighting unit's documents are a pure function of its fingerprinted inputs (items 1 to 7 and the LOD 0 geometry digests of the meshes it traced) and the tracer version, independent of worker count; the tracer's draws are keyed by a constant seed, never by a document value (`Zenith2/Docs/Design/WORLD/Bakes.md` → "Lighting fingerprint", "Tracing").
4. Every identity WORLD derives is SHA-256-based and independent of D-SEL-HASH (open, Stage 1).
5. The bytes of every tracked WORLD output (generated region documents, light bake header and page documents) and of every intermediate are identical on every machine that runs one build: WORLD's tool code evaluates no CRT transcendental, only CORE's deterministic sine, cosine and arctangent and IEEE `sqrt` ([Z2-N-ARCH-888]; ruling X314-FMA3; row WD29), its light-unit conversions included (WORLD converts with LIGHT's formulas and `Zenith_Cos` itself, never through ZenithFlux), and so does ENV's `Zenith_Place` for the rotations of placed instances (row WD33, assumption block below), and every tool main calls CORE's `Zenith_ApplyCrtMathMode()` as its first statement (`_set_FMA3_enable(0)` on Windows x64) as the ruling's defence in depth ([Z2-N-ARCH-889]), so a colleague's re-bake of unchanged inputs is a no-op, not a whole-file diff of an LFS document.

The functions are CORE's (`Zenith2/Docs/Design/CORE/Maths.md` → "Deterministic transcendental functions"; `Zenith2/Engine/ZenithCore/Zenith_Transcendental.h`, included by `Zenith_Maths.h`): CORE's follow-up answered row WD29 with WORLD's three names unchanged (`Zenith_Sin`, `Zenith_Cos`, `Zenith_Atan2`, each with `double` and `float` overloads), and `Zenith_RandomSource::UnitVector` uses `Zenith_SinCos(double)`. WORLD's generator and tracer tickets are blocked by CORE's trigonometric ticket Z2T-CORE-100.

<!-- assumption: pending ENV (row WD33) -->
ENV's `Zenith_Place` computes each placed instance's rotation from its random yaw with CORE's deterministic functions and says so in `Zenith2/Docs/Design/ENV/Procedural.md` → "Determinism argument"; ENV's text does not state it today. Baked `world.scatter` output is byte-identical across machines only once it does; the scatter ticket is blocked by ENV's patch.
<!-- /assumption -->

## Diagnostics and profiling

### Log categories

| Category | Logged |
|---|---|
| `world.import` | the import's summary per grid, every finding (also in the cook report through the import context) |
| `world.generate` | generator runs: regions, outputs, staleness |
| `world.bake` | bake runs: units, results, cancellations, write failures |
| `world.editor` | region loading limits, projection-origin moves, merge-resolution actions |

### Counters

Published from the editor through the profiler hooks' counter hook ([Z2-N-CONTRACT-155]); the cook has no overlay, so the importer's numbers are its report diagnostics.

| Counter | Kind | Meaning |
|---|---|---|
| `world.editor.regions_open` | GAUGE | open region documents of the active world |
| `world.editor.regions_wanted` | GAUGE | regions within the load radius |
| `world.bake.units_done`, `.units_failed`, `.units_cancelled` | CUMULATIVE | bake unit results |
| `world.bake.texels`, `world.bake.rays` | CUMULATIVE | traced texels and rays |
| `world.bake.stale_units` | GAUGE | STALE plus MISSING explicit units of the active world |

### Profiler zones

`World.Partition` with `.Resolve`, `.Generate`, `.Assign`, `.References`, `.Contribute`, `.Hlod`, `.Write`; `World.Generate.<GeneratorID>`; `World.Bake.Plan`, `World.Bake.Unit`, `World.Bake.Bvh`, `World.Bake.Trace`, `World.Bake.Write` (Tracy and the editor's overlay, `Zenith2/Docs/Design/DEVT/Overlay.md` → "Editor overlay").

### Debug views and reports

The partition preview (`Zenith2/Docs/Design/WORLD/Editing.md` → "Partition preview"), `world.partition.check`, the World Bakes panel, the bake report (`Zenith2/Docs/Design/WORLD/Bakes.md` → "Bake report format") and the import's INFO diagnostics (cells, entities, promotions, splits, references, HLOD nodes and lighting freshness per grid) in the cook report.

## Authoring and cooking

### What WORLD authors

The world document, region documents (with SCENE's content), layers, generator documents, baked generator outputs and light bake documents; the editor surfaces of `Zenith2/Docs/Design/WORLD/Editing.md`. Lights, probes and volumes themselves are LIGHT's components authored in EDIT's inspector; terrain is ENV's.

### Importer and cook steps

One importer, `world.world` (`Zenith2/Docs/Design/WORLD/Partition.md` → "The world importer"); WORLD registers no cook step: every product is cooked by its kind's owner.

### Migration hooks

The four WORLD document types start at version 1 with empty chains and fixtures per step (`Zenith2/Docs/Design/WORLD/Documents.md` → "Migration chains"); region documents migrate with SCENE's chain; derived navigation documents exist only inside the import and are re-derived, never migrated; baked lighting is re-baked when `uWORLD_LIGHT_TRACER_VERSION` changes; cooked outputs re-cook on `uWORLD_IMPORTER_VERSION` or any owner's version change (Z2-D-048).

## Failure handling

| Failure | Detected | Behaviour |
|---|---|---|
| a malformed or invalid world document | the importer, the editor validator | every finding reported; the import declares nothing, so the previous cooked world stays in the tree until a successful import replaces it |
| a missing region or prefab | resolution | `WORLD_ERROR_REGION_MISSING`, `WORLD_ERROR_PREFAB_RESOLUTION`; nothing declared |
| an entity outside its cell's bound | step 7 | promoted to the large grid, or `WORLD_ERROR_ENTITY_BOUNDS` |
| a limit exceeded | before declaring | `WORLD_ERROR_LIMIT` naming the cell or layer |
| a generator failure or capacity | the run | the import fails naming the generator and region |
| a stale or missing bake | the importer | warnings W34, W35, W40, W41, reported with their codes and persisted with the import, so every cook report lists them, replayed when the world's import did not run in that cook (row WD35); the old bake's products stay attached; a Shipping package is refused while the cook report holds them (rows WD14 (b), PK26) |
| a corrupt bake document | the importer | W42 to W44 errors: the import fails; re-baking the unit repairs it |
| allocation failure | any function | `WORLD_ERROR_OUT_OF_MEMORY` (`IMPORT_ERROR_OUT_OF_MEMORY` at the importer), nothing partial returned |
| a locked storage root, low disk, an external edit during a save | `SaveDocuments` | the bake or command fails with the framework's error; earlier bake units stay written; the document becomes CONFLICTED on a stale plan |
| a cook-server restart during a bake's mesh cook | the cook client | the operation's COOKING state waits for the client's restart policy and fails with the client's error when it gives up, writing nothing |
| a merged world that is invalid | the next import | the findings name both sides' paths (`Zenith2/Docs/Design/WORLD/Editing.md` → "After a merge") |

WORLD holds no device resource, so device loss does not affect it; the bake traces on the CPU.

## Test strategy

### Test targets and gate steps

`ZenithAuthoring_Tests`, `ZenithCook_Tests` and `ZenithEditor_Tests` (their WORLD files) and `Zenith_Integration_WorldAuthoring`, all in the `Test` step of the `win` presets (`Zenith2/Docs/Design/WORLD/Tests.md` → "Test targets", "Gate steps per test group").

### Test catalogue index

`Zenith2/Docs/Design/WORLD/Tests.md` lists every named test with the case it asserts: "Document tests", "Identity tests", "Partition tests", "Generator tests", "Bake tests", "Importer tests", "Editor tests", "Integration tests".

### Fixtures

Every fixture is generated at test time or committed as text: in-memory sources through `Zenith_WorldTestSources`, the cooked fixture game through `WORLD_CookedFixtures`, hand-written goldens of spec-derivable outputs, and the recorded generator goldens of one `needs-human` ticket blocked by D-SEL-HASH (open, Stage 1).

### Acceptance evidence

| Stage 4 acceptance item | Evidence |
|---|---|
| OpenWorldSlice's world authored as regions and layers, cooked into streamed cells | the slice's cook in the acceptance run (clean versus incremental equal, ASSET's suite) and its playthrough so far with streaming active |
| reproducible procedural generation | `Test_WorldGenerator_WorkerCountInvariant`, `Test_WorldGenerator_RegionSubsetMatchesFullRun` and the recorded goldens |
| baking workflows | the OpenWorldSlice lighting bake run headless by a human through the `world.bake` automation step, its report attached, and the cook reporting no stale unit; the same bake run on a second host of the same build gives byte-identical bake documents (determinism obligation 5) |
| concurrent editing granularity | `Test_WorldConcurrency_DisjointBranchesMergeToUnion` |

## Budget rows

### Rows WORLD owns

None adopted. WORLD's five rows are pending in `Zenith2/Docs/Budgets.md` → "Editor iteration rows": Z2-B-WORLD-001 to Z2-B-WORLD-005, in the order of the table below (core-doc item X304, applied):

### Pending row needs

| Name | Unit | Procedure | Platform | Workloads | Breach | Producer |
|---|---|---|---|---|---|---|
| World partition import time | s | MP-EDIT | Win | W-Editor (OpenWorldSlice's world: a clean import and the re-import after a one-region edit, `Zenith2/Docs/Design/WORLD/Partition.md` → "Partition cost and scaling") | BR-ACCEPT | Stage 1 per-system budget ticket for WORLD |
| World partition import memory | MB | MP-SUBMEM | Win | W-Editor (tool process, outside every Player bound) | BR-ACCEPT | same |
| Lighting bake time per cell | s | MP-EDIT | Win | W-Editor | BR-ACCEPT | same |
| Lighting bake tracer memory | MB | MP-SUBMEM | Win | W-Editor (tool process) | BR-ACCEPT | same |
| Edit-world region open latency | ms | MP-EDIT | Win | W-Editor | BR-ACCEPT | same |

The producer of Z2-B-WORLD-001 to Z2-B-WORLD-005 is "Stage 1 per-system budget ticket for WORLD" (no harness half: the ruling cites reference-engine and published data, `Zenith2/Docs/Budgets.md` → "Per-system producer shape"); every WORLD ticket that cites one of these rows is blocked by it. Z2-B-WORLD-002 and Z2-B-WORLD-004 are editor-or-tool rows excluded from every Player bound.

### Rows measured against

| Row | Why |
|---|---|
| Z2-B-ASSET-011 | an incremental no-op cook of a slice includes the world import's key checks |
| Z2-B-EDIT-005, Z2-B-EDIT-006 | scene open and play-in-editor start of a world |
| Z2-B-STREAM-001 … Z2-B-STREAM-005 | the cooked world's cells must let STREAM meet its rows; WORLD's partition limits and cell sizes are the levers |
| Z2-B-LIGHT-019 | the baked GI of a cell (page count × page size) is resident with it |
| Z2-B-CONTENT-001, Z2-B-CONTENT-002 | bake documents, always LFS-tracked by suffix; their total counts toward OpenWorldSlice's source quota |
| Z2-B-PACK-002 | lighting assets and HLOD meshes count toward the install size |

## Stage plan

### Stage table

| Stage | WORLD work | Gated by |
|---|---|---|
| S1 | "Stage 1 per-system budget ticket for WORLD" (the producer of the five pending rows) | Budgets' producer ordering |
| S3 sprint (fix version S4) | the API-only slice of `Zenith_AuthoringWorld.h` (types, errors, identity, descriptors, interfaces) and its identity and document-reading implementations, pulled forward once ZenithAuthoring exists (EDIT's Stage 2 library-creation ticket `Z2T-EDIT-007`, `Zenith2/Docs/Design/EDIT.md` → "Library creation ticket") | blocked by `Z2T-EDIT-007` |
| S4 | document types and validation; the partition and its writers; the importer (after ASSET's rows WD1 to WD5 and SCENE's WD6, STREAM's WD7); generators; the bake planner; the lighting bake (after GEOM's WD9, LIGHT's WD10); HLOD (after GEOM's WD8); navigation input scenes and AI's products (after D-SEL-NAV's ruling and AI's WD11, WD23); the editor extension (after EDIT's WD15); the integration suite; the OpenWorldSlice world | D-SEL-NAV (open, Stage 1/2) for the navigation tickets; D-SEL-HASH (open, Stage 1) for the recorded goldens |
| S5 | the LIGHT revalidation adds rect and disc lights and may replace the sky model with the static environment's SH9 | LIGHT's Stage 5 evaluation |

### API-only slices

1. `Zenith_AuthoringWorld.h` declarations of "Limits and constants" to "Cell content writer" plus "Generator types", "Fingerprint builder" and the function declarations of every other section with stub bodies returning `WORLD_ERROR_INVALID_DESC`, so ENV's and AI's contribution functions, the importer and the editor compile against it.
2. `Zenith_WorldImporter.h` with a stub `Import` returning `IMPORT_ERROR_SOURCE_INVALID`, landed with ASSET's WD1 to WD5.

### Ordering inside Stage 4

Documents and identity → partition (in-memory sources) → importer → cook integration and integration suite → generators → bake planner → HLOD → lighting bake → editor extension → OpenWorldSlice world authoring → the acceptance bake.

## Decisions

### Settled decisions

| Decision | How it binds WORLD |
|---|---|
| Z2-D-002 | games contain no C++: generators are built in, configured by data |
| Z2-D-004 | payload compression is deterministic per codec |
| Z2-D-019 | the partition, generators and bakes are reproducible; the run-time guarantee is STREAM's, the tool-side mirror is WORLD's |
| Z2-D-021 | OpenWorldSlice's world authoring is a flagship deliverable |
| Z2-D-024 | no dynamic generator plugins |
| Z2-D-038 | JSON authoring documents, versioned binary cooked data; intermediates never depend on C++ layout |
| Z2-D-040, Z2-D-045, Z2-D-047 | ownership, results and contract blocks |
| Z2-D-048 | migration of WORLD's documents with preview, backup, journaled write and unknown-field preservation; re-cook of outputs |
| Z2-D-075 | no modding |
| Z2-D-084 | the WORLD budget rows are a ruling ticket |

### Open decisions

- D-SEL-NAV (open, Stage 1/2): the navigation tickets wait on its ruling (its "Consumers blocked" list names WORLD's navigation baking).
- D-SEL-HASH (open, Stage 1): cook-mode generator outputs and the recorded goldens depend on it; identities and fingerprints do not.

### Design rulings owned by WORLD

| Ruling | Section |
|---|---|
| region documents are SCENE scenes | `Zenith2/Docs/Design/WORLD/Documents.md` → "Region documents" |
| the partition is an import with derived product identities | `Zenith2/Docs/Design/WORLD/Partition.md` → "The world importer", "Product identity" |
| two layer kinds realised through STREAM grids and SCENE additive scenes | `Zenith2/Docs/Design/WORLD/Partition.md` → "Layer realisation" |
| static colliders split into collision cells | `Zenith2/Docs/Design/WORLD/Partition.md` → "Static collider split" |
| lighting is an explicit, fingerprinted, CPU-traced bake of indirect light | `Zenith2/Docs/Design/WORLD/Bakes.md` → "Lighting workflow" |
| WORLD owns HLOD generation, GEOM the mesh processing | "HLOD ownership" |
| generators are versioned assignments with derived output identities | `Zenith2/Docs/Design/WORLD/Generation.md` → "Generator interface and registry", "Output identity" |
| region documents are input-only documents of their world's importer, never assets | `Zenith2/Docs/Design/WORLD/Documents.md` → "Region documents" |
| every persisted layer identity derives from the layer `id`; layer grid indices are frozen by `layerGridOrder` | `Zenith2/Docs/Design/WORLD/Documents.md` → "Layer objects", "Streaming object" |
| navigation inputs are derived inside the import, as cells are | `Zenith2/Docs/Design/WORLD/Partition.md` → "Navigation input scenes" |
| bakes are background operations outside the editor history | `Zenith2/Docs/Design/WORLD/Bakes.md` → "Bake execution" |
| editor WORLD work runs in tools-host-priority jobs over plans of the saved documents | `Zenith2/Docs/Design/WORLD/Editing.md` → "World editor extension" |
| the lighting fingerprint names the decoded LOD 0 geometry, never a cook key | `Zenith2/Docs/Design/WORLD/Bakes.md` → "Lighting fingerprint" |
| every reference to a layer or a generator holds its `id` | `Zenith2/Docs/Design/WORLD/Documents.md` → "Generator references" |

### Selection decisions WORLD owns

None.

## Requirement index

| Requirement | Title | Design headings |
|---|---|---|
| Z2-R-WORLD-001 | WORLD code in ZenithAuthoring | Zenith2/Docs/Design/WORLD.md → "ZenithAuthoring files"; Zenith2/Docs/Design/WORLD.md → "Supported matrix"; Zenith2/Docs/Design/WORLD.md → "Libraries WORLD code links" |
| Z2-R-WORLD-002 | Public header contracts | Zenith2/Docs/Design/WORLD/PublicAPI.md → "Header list"; Zenith2/Docs/Design/WORLD/PublicAPI.md → "Error and diagnostics types"; Zenith2/Docs/Design/WORLD.md → "Rules across the WORLD API" |
| Z2-R-WORLD-003 | World importer in ZenithCook | Zenith2/Docs/Design/WORLD/Partition.md → "The world importer"; Zenith2/Docs/Design/WORLD/PublicAPI.md → "Importer declarations" |
| Z2-R-WORLD-004 | World editor extension | Zenith2/Docs/Design/WORLD/Editing.md → "World editor extension"; Zenith2/Docs/Design/WORLD/PublicAPI.md → "Editor declarations" |
| Z2-R-WORLD-005 | API-only slices land first | Zenith2/Docs/Design/WORLD.md → "API-only slices"; Zenith2/Docs/Design/WORLD.md → "Stage table" |
| Z2-R-WORLD-006 | No runtime library or script surface | Zenith2/Docs/Design/WORLD.md → "Script bindings"; Zenith2/Docs/Design/WORLD.md → "Boundary with STREAM"; Zenith2/Docs/Design/WORLD.md → "What WORLD does not own" |
| Z2-R-WORLD-007 | World document schema | Zenith2/Docs/Design/WORLD/Documents.md → "World document"; Zenith2/Docs/Design/WORLD/PublicAPI.md → "World description types"; Zenith2/Docs/Design/WORLD/PublicAPI.md → "World document functions" |
| Z2-R-WORLD-008 | World validation messages | Zenith2/Docs/Design/WORLD/Documents.md → "World validation messages" |
| Z2-R-WORLD-009 | Region size and origin | Zenith2/Docs/Design/WORLD/Documents.md → "World root members"; Zenith2/Docs/Design/WORLD/Documents.md → "Region objects" |
| Z2-R-WORLD-010 | Regions as a keyed map | Zenith2/Docs/Design/WORLD/Documents.md → "Region objects"; Zenith2/Docs/Design/WORLD/PublicAPI.md → "Identity and keys" |
| Z2-R-WORLD-011 | Region documents are input-only scenes | Zenith2/Docs/Design/WORLD/Documents.md → "Region documents" |
| Z2-R-WORLD-012 | Anchor-relative region transforms | Zenith2/Docs/Design/WORLD/Documents.md → "Region documents"; Zenith2/Docs/Design/WORLD/Partition.md → "Partition algorithm" |
| Z2-R-WORLD-013 | Document limits | Zenith2/Docs/Design/WORLD/Documents.md → "Document limits" |
| Z2-R-WORLD-014 | Document type registration and migration | Zenith2/Docs/Design/WORLD/Documents.md → "Document type registration"; Zenith2/Docs/Design/WORLD/Documents.md → "Migration chains" |
| Z2-R-WORLD-015 | Malformed input never asserts | Zenith2/Docs/Design/WORLD/Documents.md → "Document limits"; Zenith2/Docs/Design/WORLD/Documents.md → "Payload objects" |
| Z2-R-WORLD-016 | Layer object schema | Zenith2/Docs/Design/WORLD/Documents.md → "Layer objects" |
| Z2-R-WORLD-017 | Editor-only data layers never cooked | Zenith2/Docs/Design/WORLD/Documents.md → "Layer kinds and states"; Zenith2/Docs/Design/WORLD/Partition.md → "Layer realisation"; Zenith2/Docs/Design/WORLD/Partition.md → "Cross-cell references" |
| Z2-R-WORLD-018 | Baked data layers merge into base grids | Zenith2/Docs/Design/WORLD/Partition.md → "Grid assignment"; Zenith2/Docs/Design/WORLD/Partition.md → "Layer realisation" |
| Z2-R-WORLD-019 | Gameplay layers are runtime only | Zenith2/Docs/Design/WORLD/Documents.md → "Layer kinds and states" |
| Z2-R-WORLD-020 | Layer grids | Zenith2/Docs/Design/WORLD/Partition.md → "Layer realisation"; Zenith2/Docs/Design/WORLD/Partition.md → "Partition algorithm" |
| Z2-R-WORLD-021 | Grid limit counts layer grids | Zenith2/Docs/Design/WORLD/Documents.md → "World validation messages"; Zenith2/Docs/Design/WORLD/Partition.md → "Partition limits" |
| Z2-R-WORLD-022 | Additive gameplay layer scenes | Zenith2/Docs/Design/WORLD/Partition.md → "Layer realisation" |
| Z2-R-WORLD-023 | Layers switch through STREAM and SCENE | Zenith2/Docs/Design/WORLD/Partition.md → "Layer realisation" |
| Z2-R-WORLD-024 | Runtime layers excluded from bakes | Zenith2/Docs/Design/WORLD/Partition.md → "Layer realisation"; Zenith2/Docs/Design/WORLD/Bakes.md → "Instance eligibility" |
| Z2-R-WORLD-025 | Layer view defaults shared and overrides per user | Zenith2/Docs/Design/WORLD/Editing.md → "World panel"; Zenith2/Docs/Design/WORLD/Documents.md → "Layer objects" |
| Z2-R-WORLD-026 | Locked layers refuse edits | Zenith2/Docs/Design/WORLD/Editing.md → "World panel" |
| Z2-R-WORLD-027 | Importer inputs recorded | Zenith2/Docs/Design/WORLD/Partition.md → "Importer inputs" |
| Z2-R-WORLD-028 | Importer document access | Zenith2/Docs/Design/WORLD/Partition.md → "The world importer"; Zenith2/Docs/Design/WORLD/Partition.md → "Importer inputs" |
| Z2-R-WORLD-029 | Region resolution | Zenith2/Docs/Design/WORLD/Partition.md → "Partition algorithm" |
| Z2-R-WORLD-030 | Grid assignment by role | Zenith2/Docs/Design/WORLD/Partition.md → "Grid assignment"; Zenith2/Docs/Design/WORLD/PublicAPI.md → "Role rules" |
| Z2-R-WORLD-031 | Cell coordinate by floor | Zenith2/Docs/Design/WORLD/Partition.md → "Partition algorithm" |
| Z2-R-WORLD-032 | Overlap bound and large promotion | Zenith2/Docs/Design/WORLD/Partition.md → "Partition algorithm"; Zenith2/Docs/Design/WORLD/Partition.md → "Grid assignment" |
| Z2-R-WORLD-033 | Static collider split | Zenith2/Docs/Design/WORLD/Partition.md → "Static collider split"; Zenith2/Docs/Design/WORLD/Partition.md → "Grid assignment" |
| Z2-R-WORLD-034 | Unsplit collider radius rule | Zenith2/Docs/Design/WORLD/Partition.md → "Static collider split"; Zenith2/Docs/Design/WORLD/Documents.md → "World validation messages" |
| Z2-R-WORLD-035 | Cross-cell references | Zenith2/Docs/Design/WORLD/Partition.md → "Cross-cell references" |
| Z2-R-WORLD-036 | Persistent markers | Zenith2/Docs/Design/WORLD/Partition.md → "Persistent markers" |
| Z2-R-WORLD-037 | Cell anchor root | Zenith2/Docs/Design/WORLD/Partition.md → "Cell scene documents" |
| Z2-R-WORLD-038 | Cells pass STREAM's checks | Zenith2/Docs/Design/WORLD/Partition.md → "Cell scene documents"; Zenith2/Docs/Design/WORLD/Partition.md → "Streaming world document generation" |
| Z2-R-WORLD-039 | Streaming world document generation | Zenith2/Docs/Design/WORLD/Partition.md → "Streaming world document generation" |
| Z2-R-WORLD-040 | Unstreamed worlds | Zenith2/Docs/Design/WORLD/Partition.md → "Unstreamed worlds" |
| Z2-R-WORLD-041 | Products and intermediate blobs | Zenith2/Docs/Design/WORLD/Partition.md → "Products and keys" |
| Z2-R-WORLD-042 | Derived product identity | Zenith2/Docs/Design/WORLD/Partition.md → "Product identity"; Zenith2/Docs/Design/WORLD/PublicAPI.md → "Identity and keys" |
| Z2-R-WORLD-043 | Partition byte determinism | Zenith2/Docs/Design/WORLD/Partition.md → "Partition determinism" |
| Z2-R-WORLD-044 | Partition independent of input order | Zenith2/Docs/Design/WORLD/Partition.md → "Partition determinism" |
| Z2-R-WORLD-045 | WORLD work independent of workers | Zenith2/Docs/Design/WORLD.md → "Rules across the WORLD API"; Zenith2/Docs/Design/WORLD/Partition.md → "Partition determinism" |
| Z2-R-WORLD-046 | Partition limits | Zenith2/Docs/Design/WORLD/Partition.md → "Partition limits" |
| Z2-R-WORLD-047 | Failed import declares nothing | Zenith2/Docs/Design/WORLD/Partition.md → "The world importer"; Zenith2/Docs/Design/WORLD.md → "Error enumerations" |
| Z2-R-WORLD-048 | ENV cell contribution | Zenith2/Docs/Design/WORLD/Partition.md → "Contributed cell content"; Zenith2/Docs/Design/WORLD/PublicAPI.md → "Cell content writer" |
| Z2-R-WORLD-049 | AI cell contribution | Zenith2/Docs/Design/WORLD/Partition.md → "Contributed cell content" |
| Z2-R-WORLD-050 | Lighting attachment | Zenith2/Docs/Design/WORLD/Partition.md → "Contributed cell content" |
| Z2-R-WORLD-051 | Incremental re-import | Zenith2/Docs/Design/WORLD/Partition.md → "The world importer"; Zenith2/Docs/Design/WORLD/Partition.md → "Partition cost and scaling" |
| Z2-R-WORLD-052 | Clean equals incremental | Zenith2/Docs/Design/WORLD/Partition.md → "Partition determinism" |
| Z2-R-WORLD-053 | Re-partition keeps identity | Zenith2/Docs/Design/WORLD/Partition.md → "Product identity" |
| Z2-R-WORLD-054 | Content edits touch one region | Zenith2/Docs/Design/WORLD/Editing.md → "Concurrent editing granularity"; Zenith2/Docs/Design/WORLD/Editing.md → "Terrain editing granularity" |
| Z2-R-WORLD-055 | Independent additions are disjoint keyed children | Zenith2/Docs/Design/WORLD/Editing.md → "Concurrent editing granularity"; Zenith2/Docs/Design/WORLD/Documents.md → "World canonical schema" |
| Z2-R-WORLD-056 | No derived runtime data in sources | Zenith2/Docs/Design/WORLD/Editing.md → "Concurrent editing granularity"; Zenith2/Docs/Design/WORLD/Partition.md → "Product identity" |
| Z2-R-WORLD-057 | Disjoint branches merge to the union | Zenith2/Docs/Design/WORLD/Editing.md → "Concurrent editing granularity" |
| Z2-R-WORLD-058 | Moving content between regions | Zenith2/Docs/Design/WORLD/Editing.md → "Moving content between regions" |
| Z2-R-WORLD-059 | Placing creates the region | Zenith2/Docs/Design/WORLD/Editing.md → "World commands" |
| Z2-R-WORLD-060 | Writes through the document framework | Zenith2/Docs/Design/WORLD/Documents.md → "Common document rules"; Zenith2/Docs/Design/WORLD/Bakes.md → "Bake execution" |
| Z2-R-WORLD-061 | Merge conflict policies | Zenith2/Docs/Design/WORLD/Editing.md → "Merge conflicts in WORLD documents" |
| Z2-R-WORLD-062 | Re-derivation after a merge | Zenith2/Docs/Design/WORLD/Editing.md → "After a merge" |
| Z2-R-WORLD-063 | Region projection anchor | Zenith2/Docs/Design/WORLD/Editing.md → "Region projection" |
| Z2-R-WORLD-064 | Region loading around the camera | Zenith2/Docs/Design/WORLD/Editing.md → "Region loading" |
| Z2-R-WORLD-065 | Advisory claims | Zenith2/Docs/Design/WORLD/Editing.md → "Concurrent editing granularity"; Zenith2/Docs/Design/WORLD/Editing.md → "Terrain editing granularity" |
| Z2-R-WORLD-066 | Static generator registry | Zenith2/Docs/Design/WORLD/Generation.md → "Generator interface and registry" |
| Z2-R-WORLD-067 | Generator document validation | Zenith2/Docs/Design/WORLD/Documents.md → "Generator document"; Zenith2/Docs/Design/WORLD/Generation.md → "Running a generator" |
| Z2-R-WORLD-068 | Version pinning and upgrade preview | Zenith2/Docs/Design/WORLD/Generation.md → "Upgrading a generator version" |
| Z2-R-WORLD-069 | Generator version retirement | Zenith2/Docs/Design/WORLD/Generation.md → "Generator interface and registry" |
| Z2-R-WORLD-070 | Cook-mode generation | Zenith2/Docs/Design/WORLD/Generation.md → "Execution modes"; Zenith2/Docs/Design/WORLD/Partition.md → "Partition algorithm" |
| Z2-R-WORLD-071 | Baked-mode generation | Zenith2/Docs/Design/WORLD/Generation.md → "Execution modes"; Zenith2/Docs/Design/WORLD/Generation.md → "Generator outputs and staleness" |
| Z2-R-WORLD-072 | Seed derivation | Zenith2/Docs/Design/WORLD/Generation.md → "Seed derivation" |
| Z2-R-WORLD-073 | Generated output identity | Zenith2/Docs/Design/WORLD/Generation.md → "Output identity" |
| Z2-R-WORLD-074 | Generator determinism | Zenith2/Docs/Design/WORLD/Generation.md → "Generator determinism" |
| Z2-R-WORLD-075 | Generators are assignments | Zenith2/Docs/Design/WORLD/Generation.md → "Running a generator" |
| Z2-R-WORLD-076 | Obstacles and dependencies | Zenith2/Docs/Design/WORLD/Generation.md → "Obstacles and dependencies" |
| Z2-R-WORLD-077 | Generator capacity | Zenith2/Docs/Design/WORLD/Generation.md → "Running a generator" |
| Z2-R-WORLD-078 | Each output owned by one region | Zenith2/Docs/Design/WORLD/Generation.md → "Running a generator" |
| Z2-R-WORLD-079 | world.lattice | Zenith2/Docs/Design/WORLD/Generation.md → "world.lattice" |
| Z2-R-WORLD-080 | world.spline | Zenith2/Docs/Design/WORLD/Generation.md → "world.spline" |
| Z2-R-WORLD-081 | world.scatter | Zenith2/Docs/Design/WORLD/Generation.md → "world.scatter" |
| Z2-R-WORLD-082 | world.probeGrid | Zenith2/Docs/Design/WORLD/Generation.md → "world.probeGrid" |
| Z2-R-WORLD-083 | Baked generator staleness | Zenith2/Docs/Design/WORLD/Generation.md → "Generator outputs and staleness" |
| Z2-R-WORLD-084 | Edited outputs are protected | Zenith2/Docs/Design/WORLD/Generation.md → "Generator outputs and staleness" |
| Z2-R-WORLD-085 | Detaching an output | Zenith2/Docs/Design/WORLD/Generation.md → "Detaching an output" |
| Z2-R-WORLD-086 | Hand-written generator goldens | Zenith2/Docs/Design/WORLD/Generation.md → "Golden outputs" |
| Z2-R-WORLD-087 | Recorded generator goldens | Zenith2/Docs/Design/WORLD/Generation.md → "Golden outputs" |
| Z2-R-WORLD-088 | Bake units | Zenith2/Docs/Design/WORLD/Bakes.md → "Bake units" |
| Z2-R-WORLD-089 | Fingerprints are SHA-256 | Zenith2/Docs/Design/WORLD/Bakes.md → "Input fingerprints"; Zenith2/Docs/Design/WORLD/PublicAPI.md → "Fingerprint builder" |
| Z2-R-WORLD-090 | Bake ordering | Zenith2/Docs/Design/WORLD/Bakes.md → "Bake planner" |
| Z2-R-WORLD-091 | Atomic bake units | Zenith2/Docs/Design/WORLD/Bakes.md → "Bake execution" |
| Z2-R-WORLD-092 | Bake cancellation | Zenith2/Docs/Design/WORLD/Bakes.md → "Bake execution" |
| Z2-R-WORLD-093 | Bake resumption | Zenith2/Docs/Design/WORLD/Bakes.md → "Bake execution" |
| Z2-R-WORLD-094 | Bake report | Zenith2/Docs/Design/WORLD/Bakes.md → "Bake report format" |
| Z2-R-WORLD-095 | Headless bakes | Zenith2/Docs/Design/WORLD/Bakes.md → "Bake execution"; Zenith2/Docs/Design/WORLD/Editing.md → "World editor extension" |
| Z2-R-WORLD-096 | Bake panel orchestration | Zenith2/Docs/Design/WORLD/Editing.md → "Bake panel and commands" |
| Z2-R-WORLD-097 | Lighting fingerprint content | Zenith2/Docs/Design/WORLD/Bakes.md → "Lighting fingerprint" |
| Z2-R-WORLD-098 | Lighting staleness at cook | Zenith2/Docs/Design/WORLD/Documents.md → "World validation messages"; Zenith2/Docs/Design/WORLD/Bakes.md → "Lighting fingerprint" |
| Z2-R-WORLD-099 | Light bake documents | Zenith2/Docs/Design/WORLD/Documents.md → "Baked lighting documents"; Zenith2/Docs/Design/WORLD/Documents.md → "Payload objects" |
| Z2-R-WORLD-100 | Lightmap atlas packing | Zenith2/Docs/Design/WORLD/Bakes.md → "Atlas packing" |
| Z2-R-WORLD-101 | Deterministic BVH | Zenith2/Docs/Design/WORLD/Bakes.md → "BVH construction" |
| Z2-R-WORLD-102 | Lightmaps hold indirect light | Zenith2/Docs/Design/WORLD/Bakes.md → "Lighting workflow"; Zenith2/Docs/Design/WORLD/Bakes.md → "Tracing" |
| Z2-R-WORLD-103 | Direct light at path vertices | Zenith2/Docs/Design/WORLD/Bakes.md → "Tracing"; Zenith2/Docs/Design/WORLD/Bakes.md → "Bake geometry" |
| Z2-R-WORLD-104 | Declared sky | Zenith2/Docs/Design/WORLD/Bakes.md → "Escaped rays" |
| Z2-R-WORLD-105 | Irradiance volumes | Zenith2/Docs/Design/WORLD/Bakes.md → "Tracing" |
| Z2-R-WORLD-106 | Baked reflection probes | Zenith2/Docs/Design/WORLD/Bakes.md → "Tracing" |
| Z2-R-WORLD-107 | Lighting bake determinism | Zenith2/Docs/Design/WORLD/Bakes.md → "Tracing" |
| Z2-R-WORLD-108 | Lighting import | Zenith2/Docs/Design/WORLD/Bakes.md → "Lighting import" |
| Z2-R-WORLD-109 | Lighting bake restorable outside the history | Zenith2/Docs/Design/WORLD/Bakes.md → "Bake execution"; Zenith2/Docs/Design/WORLD/Editing.md → "Bake panel and commands" |
| Z2-R-WORLD-110 | Bake geometry from cooked meshes | Zenith2/Docs/Design/WORLD/Bakes.md → "Bake geometry" |
| Z2-R-WORLD-111 | Derived navigation documents | Zenith2/Docs/Design/WORLD/Documents.md → "Derived navigation documents"; Zenith2/Docs/Design/WORLD/Partition.md → "Navigation input scenes" |
| Z2-R-WORLD-112 | Navigation inputs derived in the import | Zenith2/Docs/Design/WORLD/Partition.md → "Navigation input scenes"; Zenith2/Docs/Design/WORLD/Bakes.md → "Navigation workflow" |
| Z2-R-WORLD-113 | Disjoint navigation tiles | Zenith2/Docs/Design/WORLD/Documents.md → "Derived navigation documents" |
| Z2-R-WORLD-114 | Navigation follows every import | Zenith2/Docs/Design/WORLD/Bakes.md → "Navigation workflow"; Zenith2/Docs/Design/WORLD/Partition.md → "Navigation input scenes" |
| Z2-R-WORLD-115 | Navigation baked by the cook | Zenith2/Docs/Design/WORLD/Bakes.md → "Navigation workflow" |
| Z2-R-WORLD-116 | WORLD generates HLOD | Zenith2/Docs/Design/WORLD.md → "HLOD ownership"; Zenith2/Docs/Design/WORLD/Bakes.md → "HLOD workflow" |
| Z2-R-WORLD-117 | HLOD node hierarchy | Zenith2/Docs/Design/WORLD/Bakes.md → "HLOD workflow" |
| Z2-R-WORLD-118 | HLOD instance eligibility | Zenith2/Docs/Design/WORLD/Bakes.md → "Instance eligibility" |
| Z2-R-WORLD-119 | HLOD material modes | Zenith2/Docs/Design/WORLD/Bakes.md → "Material modes" |
| Z2-R-WORLD-120 | HLOD recipes deterministic | Zenith2/Docs/Design/WORLD/Bakes.md → "Merge recipes" |
| Z2-R-WORLD-121 | GEOM cooks merge recipes | Zenith2/Docs/Design/WORLD/Bakes.md → "Merge recipes" |
| Z2-R-WORLD-122 | Play in the editor | Zenith2/Docs/Design/WORLD/Editing.md → "Play in the editor" |
| Z2-R-WORLD-123 | WORLD diagnostics | Zenith2/Docs/Design/WORLD.md → "Log categories"; Zenith2/Docs/Design/WORLD.md → "Counters"; Zenith2/Docs/Design/WORLD.md → "Profiler zones" |
| Z2-R-WORLD-124 | Partition preview | Zenith2/Docs/Design/WORLD/Editing.md → "Partition preview" |
| Z2-R-WORLD-125 | Partition check command | Zenith2/Docs/Design/WORLD/Editing.md → "World commands" |
| Z2-R-WORLD-126 | WORLD budget producer | Zenith2/Docs/Design/WORLD.md → "Pending row needs" |
| Z2-R-WORLD-127 | OpenWorldSlice world | Zenith2/Docs/Design/WORLD.md → "Acceptance evidence" |
| Z2-R-WORLD-128 | Stale bakes block releases | Zenith2/Docs/Design/WORLD.md → "Reconciliations with other documents" |
| Z2-R-WORLD-129 | Region documents are never packaged | Zenith2/Docs/Design/WORLD/Documents.md → "Region documents"; Zenith2/Docs/Design/WORLD/Documents.md → "Document set" |
| Z2-R-WORLD-130 | Layer identities derive from the layer id | Zenith2/Docs/Design/WORLD/Documents.md → "Layer objects"; Zenith2/Docs/Design/WORLD/Partition.md → "Product identity" |
| Z2-R-WORLD-131 | Stable layer grid indices | Zenith2/Docs/Design/WORLD/Documents.md → "Streaming object"; Zenith2/Docs/Design/WORLD/Partition.md → "Layer realisation" |
| Z2-R-WORLD-132 | Initial layer state is cooked | Zenith2/Docs/Design/WORLD/Partition.md → "Layer realisation"; Zenith2/Docs/Design/WORLD/Partition.md → "Streaming world document generation" |
| Z2-R-WORLD-133 | Runtime layer collision follows its layer | Zenith2/Docs/Design/WORLD/Partition.md → "Static collider split"; Zenith2/Docs/Design/WORLD/Partition.md → "Layer realisation" |
| Z2-R-WORLD-134 | Collider proxies carry their source | Zenith2/Docs/Design/WORLD/Partition.md → "Static collider split" |
| Z2-R-WORLD-135 | Grid roles conform to STREAM | Zenith2/Docs/Design/WORLD/Partition.md → "Grid assignment" |
| Z2-R-WORLD-136 | Persistent scene references reported | Zenith2/Docs/Design/WORLD/Partition.md → "Cross-cell references" |
| Z2-R-WORLD-137 | Partition pre-checks STREAM limits | Zenith2/Docs/Design/WORLD/Partition.md → "Partition limits" |
| Z2-R-WORLD-138 | Tool outputs identical across machines | Zenith2/Docs/Design/WORLD.md → "Determinism obligations"; Zenith2/Docs/Design/WORLD/Bakes.md → "Tracing" |
| Z2-R-WORLD-139 | WORLD work items never block | Zenith2/Docs/Design/WORLD.md → "Rules across the WORLD API"; Zenith2/Docs/Design/WORLD/Editing.md → "World editor extension" |
| Z2-R-WORLD-140 | Terrain editing granularity | Zenith2/Docs/Design/WORLD/Editing.md → "Terrain editing granularity" |
| Z2-R-WORLD-141 | Generated edits survive schema migration | Zenith2/Docs/Design/WORLD/Generation.md → "Generator outputs and staleness" |
| Z2-R-WORLD-142 | Editor WORLD work in tools-host jobs | Zenith2/Docs/Design/WORLD/Editing.md → "World editor extension"; Zenith2/Docs/Design/WORLD/Bakes.md → "Bake execution" |
| Z2-R-WORLD-143 | Editor plans read saved documents | Zenith2/Docs/Design/WORLD/Editing.md → "World editor extension"; Zenith2/Docs/Design/WORLD/Editing.md → "Bake panel and commands" |
| Z2-R-WORLD-144 | Lighting fingerprint names the traced geometry | Zenith2/Docs/Design/WORLD/Bakes.md → "Lighting fingerprint"; Zenith2/Docs/Design/WORLD/Bakes.md → "Bake execution" |
| Z2-R-WORLD-145 | Terrain tiles are recorded inputs | Zenith2/Docs/Design/WORLD/Partition.md → "Importer inputs"; Zenith2/Docs/Design/WORLD/Partition.md → "Terrain loading"; Zenith2/Docs/Design/WORLD/Generation.md → "Generator outputs and staleness" |
| Z2-R-WORLD-146 | Layers and generators referenced by identity | Zenith2/Docs/Design/WORLD/Documents.md → "Generator references"; Zenith2/Docs/Design/WORLD/Documents.md → "Generator document"; Zenith2/Docs/Design/WORLD/Editing.md → "World commands" |
| Z2-R-WORLD-147 | Obstacles gathered by distance | Zenith2/Docs/Design/WORLD/Generation.md → "Obstacles and dependencies"; Zenith2/Docs/Design/WORLD/Generation.md → "Running a generator" |
| Z2-R-WORLD-148 | Explicit static bodies are static | Zenith2/Docs/Design/WORLD/Partition.md → "Grid assignment"; Zenith2/Docs/Design/WORLD/Partition.md → "Static collider split"; Zenith2/Docs/Design/WORLD/Bakes.md → "Instance eligibility" |
| Z2-R-WORLD-149 | Terrain-only navigation regions | Zenith2/Docs/Design/WORLD/Documents.md → "Derived navigation documents"; Zenith2/Docs/Design/WORLD/Partition.md → "Navigation input scenes" |
| Z2-R-WORLD-150 | Unstreamed worlds reference their navigation | Zenith2/Docs/Design/WORLD/Partition.md → "Unstreamed worlds"; Zenith2/Docs/Design/WORLD/Partition.md → "Contributed cell content" |
| Z2-R-WORLD-151 | Pinned plans are the one editor snapshot | Zenith2/Docs/Design/WORLD/Editing.md → "World editor extension"; Zenith2/Docs/Design/WORLD/Bakes.md → "Bake execution" |
| Z2-R-WORLD-152 | Baked scatter covers its surface reach and exclusions | Zenith2/Docs/Design/WORLD/Generation.md → "Obstacles and dependencies"; Zenith2/Docs/Design/WORLD/Generation.md → "Generator outputs and staleness" |
| Z2-R-WORLD-153 | Layer grids fit STREAM's live-instance bound | Zenith2/Docs/Design/WORLD/Partition.md → "Partition algorithm"; Zenith2/Docs/Design/WORLD/Partition.md → "Partition limits" |
