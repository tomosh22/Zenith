# CONTENT design: content production

This is the design document of the CONTENT area (`Z2 CONTENT — Content production`). Workers implement against it: the source importers (glTF 2.0, images, audio and, per D-SEL-FBX (open, Stage 4), FBX), the supported-DCC feature matrix and the coordinate and unit conventions those importers enforce, import presets, reimport that preserves authored overrides, recoverable batch operations with reference repair, the out-of-process importer requirement and its crash tests, the shared derived-data cache backend and its failure modes, the structure-aware three-way merge driver, distributed cooking, content provenance and the repository storage policy. It elaborates `Zenith2/Docs/Architecture.md → "Repository layout"`, `Zenith2/Docs/Architecture.md → "Security architecture"` and `Zenith2/Docs/Contracts.md → "Compatibility policy per kind"` and never contradicts them. Where it needs something another area has not specified, it says so in an `<!-- assumption: pending <AREA> -->` block, and every such need is a row `CT<n>` of `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md`. The requirement catalogue is `Zenith2/Docs/Requirements/CONTENT.md`; every obligation below traces to a `Z2-R-CONTENT-NNN` listed in "Requirement index".

<!-- informative -->
The reference engine at `C:/dev/Zenith` imported artist content twice: through Assimp (`Tools/Zenith_Tools_MeshExport.cpp`, `Tools/Zenith_Tools_AssimpConvert.cpp`) and, for binary glTF that Assimp rejects because gltfpack writes `EXT_meshopt_compression`, through a hand-written `.glb` reader (`Tools/Zenith_Tools_GlbImport.cpp`, 1,612 lines, with its own meshopt decoder in `Tools/Zenith_Tools_MeshoptDecode.cpp`). Four lessons from that path shape this design. (1) Every `.glb` import came out inside-out: glTF winds front faces counter-clockwise and the old engine wound them `cross(C − A, B − A)`, so the importer reversed every triangle and then had to special-case reflected node transforms; Zenith2 winds counter-clockwise everywhere ([Z2-N-REND-885]), so the importer reverses only a negatively-determinant instance and a test proves outward winding. (2) Tangent frames came out NaN on import, from degenerate UVs and source tangents passed through unchecked; the importer validates every source tangent and hands GEOM's MikkTSpace pass a clean mesh. (3) Imported characters faced backwards, because the exporter's forward axis was never checked against the engine's model front; Zenith2 fixes the model front at +Z (`Zenith2/Docs/Design/CORE/Maths.md → "Coordinate systems"`), converts other conventions explicitly and tests every DCC route on a probe asset with a known front. (4) A hand-made asset's real size and facing were unknowable until it was read, and several were scaled by 100 or 0.01; every import records its bounds in metres in the cook report and a preset may bound them.
<!-- /informative -->

## Purpose and scope

### What CONTENT owns

| Capability | Where it lives |
|---|---|
| the supported-DCC feature matrix: the DCC routes, the glTF 2.0 core and extension support table, per-feature support for skins, morphs, animation, materials, cameras and lights, the image and audio source-format tables, the fidelity thresholds, and the per-DCC fixture tests | `Zenith2/Docs/Design/CONTENT/DccMatrix.md`; `Zenith2/Tools/ZenithCook/Tests/Fixtures/Content/Dcc/` |
| coordinate and unit conventions on import: axis, handedness and unit conversion, the negative-determinant rule, winding, tangent and normal validation, bounds and facing reports | `Zenith2/Docs/Design/CONTENT/DccMatrix.md`; `Zenith2/Tools/ContentCore/` |
| the source parsers: glTF 2.0 (through fastgltf and meshoptimizer), the image decoders and the audio decoders, with their parser limits and fuzz targets | `Zenith2/Tools/ContentCore/` (`ZenithContentCore`) |
| the importers that implement ASSET's `Zenith_Importer`: `gltf`, `image`, `audio` and `fbx` (`Zenith_FbxImporter`, per D-SEL-FBX (open, Stage 4)), their settings descriptions, product keys, rename recognition, generated materials, packed textures and the generated model prefab | `Zenith2/Tools/ZenithCook/Internal/Importers/` |
| import presets: the preset document type `Zenith.Content.ImportPresetDocument`, its schema and migration chain, preset matching at `.meta` creation, and the engine's shipped presets | `Zenith2/Tools/Authoring/Internal/CONTENT/`; `Zenith2/Engine/ZenithAssets/Internal/Content/ImportPresets/` |
| reimport that preserves authored overrides: CONTENT's members of the `.meta` sub-asset entries (`exclude`, `materialRemap`), product fingerprints and rename recognition, stable product keys, node keys and entity IDs | `Zenith2/Docs/Design/CONTENT/PresetsAndReimport.md` |
| recoverable batch operations: the batch specification, the operation journal, chunked execution, resume, abandon, rollback, the report, and the operations that use them (move, rename, delete, reimport, preset and settings changes, sub-asset remaps, orphan repair) | `Zenith2/Tools/Authoring/Internal/CONTENT/Batch/`; `Zenith2/Tools/ZenithContent/` |
| asset rename, move and delete with reference repair across every authoring document | `Zenith2/Tools/Authoring/Internal/CONTENT/Repair/` |
| importer crash isolation: every third-party parser runs out of process, the fault-injection cook key, the crash and timeout tests | `Zenith2/Tools/ZenithCook/Internal/Importers/`; `Zenith2/Docs/Design/CONTENT/Tests.md` |
| the shared derived-data cache store backend (a file-share store), its deployment rules, its administration commands and its failure-mode acceptance runs | `Zenith2/Tools/Authoring/Internal/CONTENT/SharedDDC/`; `Zenith2/Tools/ZenithContent/` |
| the structure-aware three-way merge driver for scenes, prefabs, schemas and every other canonical authoring document, its conflict-marker format and its git integration | `Zenith2/Tools/ContentCore/` (the engine), `Zenith2/Tools/Authoring/Internal/CONTENT/Merge/` (the per-type policies), `Zenith2/Tools/ZenithContent/` (the driver) |
| distributed cooking: the partitioned pre-cook through the shared store, the coordinator and the helper agent | `Zenith2/Tools/ZenithContent/` |
| content provenance: the origin metadata schema of the `.meta` member `provenance`, its validation, the licence table and the cooked licence list | `Zenith2/Tools/ContentCore/`; `Zenith2/Tools/ZenithCook/Internal/Content/` |
| the repository storage policy: the LFS tracking threshold and per-game source quotas (Z2-B-CONTENT-001 … -009), the `.gitattributes` rules and the storage check | `Zenith2/Tools/ZenithContent/`; `Zenith2/.gitattributes` |

### Boundary with ASSET

ASSET (signed off) owns the asset database, `.meta` sidecars, the kind table, the importer interface and registry, settings resolution, sub-asset identity, the cook driver, cook steps, the worker-process pool, the cook server, the derived-data cache mechanism and the local store. CONTENT re-specifies none of these; it implements the interfaces ASSET declares and specifies the policies ASSET leaves to it, item by item (`Zenith2/Docs/Design/ASSET.md → "Boundary with CONTENT"`):

| CONTENT item | ASSET mechanism CONTENT uses (cited, not restated) | What CONTENT adds |
|---|---|---|
| Z2-I-CONTENT-01 … -13 | `Zenith2/Docs/Design/ASSET/Import.md → "Importer interface"`, "Intermediates" | the four importers, their settings descriptions, the DCC matrix, the conventions and the per-DCC tests |
| Z2-I-CONTENT-14 | `Zenith2/Docs/Design/ASSET/Import.md → "Settings resolution"` (layer 2 reads the preset's `settings`) | the preset document type, preset matching at `.meta` creation, the shipped presets |
| Z2-I-CONTENT-15 | `Zenith2/Docs/Design/ASSET/Import.md → "Identity preservation on reimport"` | stable product keys, fingerprinted rename recognition, CONTENT's sub-asset entry members and their application, stable generated entity IDs |
| Z2-I-CONTENT-16 … -18 | `Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Journaled write"` (each chunk's document writes), `Zenith2/Docs/Design/ASSET/Formats.md → "Cook server messages"` (the cook requests) | the batch specification, the operation journal above the framework's per-batch journal, chunking, resume, abandon, rollback and the report |
| Z2-I-CONTENT-19 … -21 | `Zenith2/Docs/Design/ASSET/Database.md → "Rename and move detection"`, "Orphaned meta files and tombstones", "Dependency graph"; SCENE's `Zenith_PlanReferenceRepair` (`Zenith2/Docs/Design/SCENE/SceneAPI.md → "Zenith_ReferenceRepair header"`) | the move, rename and delete operations, the generic repairer for non-scene documents, orphaned sub-asset repair |
| Z2-I-CONTENT-22 | `Zenith2/Docs/Design/ASSET/Cook.md → "Worker processes"` | `m_bOutOfProcess` true for every third-party parser, the fault-source cook key, the crash, hang and partial-output tests |
| Z2-I-CONTENT-23 … -26 | `Zenith2/Docs/Design/ASSET/DDC.md → "Shared store contract"`, "Failure modes" | the file-share store, its layout, its timeout watchdog, its administration, its deployment rules and the failure-mode acceptance runs |
| Z2-I-CONTENT-27 … -29 | `Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Canonical serialization"`; `Zenith2/Docs/Design/SCENE.md → "Merge support for CONTENT"` | the merge engine, the per-type merge policies, the conflict-marker format, the git driver |
| Z2-I-CONTENT-30 | `Zenith2/Docs/Design/ASSET/Cook.md → "Keys and invalidation"` | the partitioned pre-cook, its coordinator and helpers |
| Z2-I-CONTENT-31 | `Zenith2/Docs/Design/ASSET/Database.md → "Provenance records"` (ASSET carries the `.meta` `provenance` member verbatim) | the origin schema, its validation, the licence table and the `content.licences` cook step |

Where CONTENT needs more than ASSET provides, the need is fenced as an assumption and filed as a cross-area row to ASSET ("Assumed interfaces pending other areas"); CONTENT never edits an ASSET heading, the kind table or ZenithCook's ASSET-owned files.

### Explicit exclusions

| Not CONTENT | Owner | Why |
|---|---|---|
| the asset database, `.meta` schema and migration, settings resolution, sub-asset identity, the cook driver, worker pool, cook server, DDC client and local store | ASSET | `Zenith2/Docs/Design/ASSET.md → "Boundary with CONTENT"` |
| the mesh, texture and material cook steps and formats, the intermediate mesh's byte layout, MikkTSpace tangent generation, LOD generation | GEOM | `Zenith2/Docs/Design/GEOM.md → "Import preset keys GEOM reads"`; CONTENT fills GEOM's structures |
| skeleton extraction, rest-pose derivation, clip resampling and compression, retargeting | ANIM | `Zenith2/Docs/Design/ANIM/Formats.md → "Import conversion"`; CONTENT calls ANIM's helpers with parsed, converted data |
| the decoded-audio blob's layout and the sound cook | AUDIO | `Zenith2/Docs/Design/AUDIO/Authoring.md → "Audio importer boundary"`; CONTENT writes the blob AUDIO specifies |
| scene and prefab documents, canonical serialization, the reference-repair planner for scenes and prefabs | SCENE | `Zenith2/Docs/Design/SCENE/Prefabs.md → "Reference repair"`; inventory item Z2-I-CONTENT-28 is delivered by SCENE's Stage 2 requirements |
| the asset browser, import dialogs, reimport commands, the source-control panel, the batch-report and merge panels | EDIT | `Zenith2/Docs/Design/EDIT/Pipeline.md → "Asset operations"`; CONTENT supplies the models EDIT displays |
| packaging, archives, asset packs and dictionaries | PACK | cooked files are PACK's inputs |
| the licence inventory and release notices | SHIP | `Zenith2/Docs/Design/SHIP/Release.md → "Content licences"` reads CONTENT's cooked licence list |
| world partitions, gameplay layers, bake orchestration | WORLD | Appendix A row WORLD |
| the character workflow (rig, retarget, graph, preview, timed walkthrough) | CHAR | CHAR uses CONTENT's importers as its first step |
| any Luau surface | none | CONTENT is tool-side only: no runtime library, no binding, no misuse code, no asynchronous source kind |
| third-party mod content | none | modding is excluded (Z2-D-075) |

### Inventory items covered

| Item | Text (Inventory v1) | Covered by |
|---|---|---|
| Z2-I-CONTENT-01 | a supported-DCC feature matrix | DccMatrix.md "DCC routes", "Feature support matrix", "Source format tables" |
| Z2-I-CONTENT-02 | DCC matrix: glTF 2.0 extensions | DccMatrix.md "glTF core support", "glTF extension support"; Importers.md "glTF importer" |
| Z2-I-CONTENT-03 | DCC matrix: FBX via a conversion path if selected | DccMatrix.md "FBX route"; Importers.md "FBX importer" |
| Z2-I-CONTENT-04 | DCC matrix per-feature support: skins | Importers.md "Skins" |
| Z2-I-CONTENT-05 | DCC matrix per-feature support: morphs | Importers.md "Morph targets" |
| Z2-I-CONTENT-06 | DCC matrix per-feature support: animation | Importers.md "Animations" |
| Z2-I-CONTENT-07 | DCC matrix per-feature support: materials | Importers.md "Materials", "Textures referenced by materials" |
| Z2-I-CONTENT-08 | DCC matrix per-feature support: cameras | Importers.md "Cameras" |
| Z2-I-CONTENT-09 | DCC matrix per-feature support: lights | Importers.md "Lights" |
| Z2-I-CONTENT-10 | coordinate conventions: axis conversion on import | DccMatrix.md "Conversion settings", "Basis change algorithm" |
| Z2-I-CONTENT-11 | coordinate conventions: handedness conversion on import | DccMatrix.md "Basis change algorithm", "Winding and the negative-determinant rule" |
| Z2-I-CONTENT-12 | unit conventions: unit conversion on import | DccMatrix.md "Conversion settings", "Unit conversion" |
| Z2-I-CONTENT-13 | coordinate and unit conventions tested per DCC | DccMatrix.md "Convention probe asset", "Per-DCC fixtures" |
| Z2-I-CONTENT-14 | import presets | PresetsAndReimport.md "Preset document schema", "Preset matching", "Shipped presets" |
| Z2-I-CONTENT-15 | reimport that preserves authored overrides | PresetsAndReimport.md "Override-preserving reimport", "Product overrides schema" |
| Z2-I-CONTENT-16 | recoverable batch operations: journaled | BatchOperations.md "Operation journal" |
| Z2-I-CONTENT-17 | recoverable batch operations: resumable | BatchOperations.md "Resume algorithm" |
| Z2-I-CONTENT-18 | recoverable batch operations: with a report | BatchOperations.md "Batch report" |
| Z2-I-CONTENT-19 | asset rename with reference repair | BatchOperations.md "Move and rename" |
| Z2-I-CONTENT-20 | asset move with reference repair | BatchOperations.md "Move and rename" |
| Z2-I-CONTENT-21 | asset delete with reference repair | BatchOperations.md "Delete" |
| Z2-I-CONTENT-22 | importer crash isolation (out-of-process importers) | Importers.md "Crash isolation" |
| Z2-I-CONTENT-23 | shared DDC operation | SharedCache.md "File-share store", "Deployment" |
| Z2-I-CONTENT-24 | shared DDC failure mode: unreachable | SharedCache.md "Unreachable share" |
| Z2-I-CONTENT-25 | shared DDC failure mode: corrupt | SharedCache.md "Corrupt entries" |
| Z2-I-CONTENT-26 | shared DDC failure mode: stale | SharedCache.md "Stale entries" |
| Z2-I-CONTENT-27 | semantic merges: a structure-aware 3-way merge driver for scenes, prefabs and schemas | Merge.md "Structural merge algorithm", "Merge policies" |
| Z2-I-CONTENT-28 | semantic merges depend on canonical serialization | Merge.md "Canonical inputs and outputs" (the serialization itself is SCENE's Stage 2 delivery) |
| Z2-I-CONTENT-29 | semantic merges: nested-prefab overrides and array-ordering conflicts fall back to an explicit textual-conflict marker | Merge.md "Conflict markers", "Scene and prefab rules" |
| Z2-I-CONTENT-30 | distributed cooking | SharedCache.md "Distributed cooking" |
| Z2-I-CONTENT-31 | content provenance | SharedCache.md "Content provenance" |

### Document family

| File | Content | Its `##` headings |
|---|---|---|
| `Zenith2/Docs/Design/CONTENT.md` (this file) | purpose, dependencies, layout, the API map and the top-level contracts, every skeleton section | the headings of this file |
| `Zenith2/Docs/Design/CONTENT/DccMatrix.md` | the DCC routes, the glTF support tables, the feature matrix, the source-format tables, the conventions, the probe asset, fidelity thresholds, the FBX route | Scope of this file · DCC routes · glTF core support · glTF extension support · Feature support matrix · Source format tables · Conversion settings · Basis change algorithm · Unit conversion · Winding and the negative-determinant rule · Normals and tangents on import · Bounds and facing report · Convention probe asset · Per-DCC fixtures · Fidelity thresholds · FBX route |
| `Zenith2/Docs/Design/CONTENT/Importers.md` | `ZenithContentCore`'s parser API, the four importers, products, keys, materials, skins, morphs, animations, cameras, lights, the generated prefab, images, audio, crash isolation, parser limits, errors | Scope of this file · ZenithContentCore API · Importer registration · glTF importer · Product keys · Meshes · Skins · Morph targets · Animations · Materials · Textures referenced by materials · Cameras · Lights · Generated model prefab · Physics collision products · Image importer · Audio importer · FBX importer · Import report member · Crash isolation · Parser limits · Import detail codes |
| `Zenith2/Docs/Design/CONTENT/PresetsAndReimport.md` | presets and override-preserving reimport | Scope of this file · Preset document schema · Preset validation · Preset matching · Shipped presets · Preset document type registration · Override-preserving reimport · Product overrides schema · Rename recognition · Applying product overrides · Overrides across settings migration |
| `Zenith2/Docs/Design/CONTENT/BatchOperations.md` | the batch engine and the operations built on it | Scope of this file · Batch API · Batch specification · Operation kinds · Planning · Chunked execution · Operation journal · Resume algorithm · Abandon and rollback · Batch report · Move and rename · Delete · Reimport and settings batches · Sub-asset remap and orphan repair · Generic reference repairer · Cook channel · Batch state machine · Batch command line |
| `Zenith2/Docs/Design/CONTENT/Merge.md` | the merge driver | Scope of this file · Merge API · Driver command line · Canonical inputs and outputs · Structural merge algorithm · Keyed array merge · Set merge · Merge policies · Scene and prefab rules · Schema rules · Environment document rules · Conflict markers · Text fallback · Git integration · Merge determinism and limits |
| `Zenith2/Docs/Design/CONTENT/SharedCache.md` | the shared DDC store, its failure modes, distributed cooking, provenance and the storage policy | Scope of this file · File-share store · Store layout · Store operations · Timeout watchdog · Unreachable share · Corrupt entries · Stale entries · Store administration · Deployment · Distributed cooking · Partition protocol · Content provenance · Licence table · Content licences step · Storage policy check |
| `Zenith2/Docs/Design/CONTENT/Tests.md` | every named test with the case it asserts | Scope of this file · Test targets · Fixture generation · ContentCore tests · Importer tests · Convention tests · Preset and reimport tests · Batch tests · Merge tests · Shared cache tests · Provenance and storage tests · Integration suite · Fuzz targets · Needs-human recordings · Gate steps per test group |

### Design choices at a glance

<!-- elaboration -->
| # | Choice | Section |
|---|---|---|
| CT-R1 | The shared DDC backend is a file-share store (an SMB share or a local directory) with the local store's create-only rename discipline; no cache server is deployed | SharedCache.md "File-share store" |
| CT-R2 | Source decoding uses stb_image for PNG, JPEG, TGA and BMP, dr_wav, dr_flac and dr_mp3 for WAV, AIFF, FLAC and MP3, and stb_vorbis for Ogg Vorbis, all compiled into the host-only `ZenithContentCore` and run only in cook worker processes | Importers.md "ZenithContentCore API" |
| CT-R3 | Every glTF scene becomes one generated prefab product whose entity IDs are derived from the product GUID and a stable node key, so scene overrides keyed by those IDs survive reimport | Importers.md "Generated model prefab" |
| CT-R4 | Authored per-product overrides are members of the product's own `.meta` sub-asset entry (CONTENT's `exclude` and `materialRemap` beside ANIM's `annotation` and `rig`), so one per-product mechanism serves both areas, renames carry the members with the GUID, and no per-product settings layer exists | PresetsAndReimport.md "Product overrides schema" |
| CT-R5 | The merge driver is a separate tool executable (`ZenithContent merge`) invoked by git through `.gitattributes`, so a merge needs no cook server and no editor process. It links `ZenithEditorCore` (for `Zenith_RegisterEditorDocumentTypes`) and with it the GPU-module closure, so it exists only after a `win` tools build; until then git merges those paths with its own line merge, and `Setup-Zenith2.ps1` installs the driver only after building the tools | Merge.md "Git integration" |
| CT-R6 | Distributed cooking is a partitioned pre-cook: helper hosts with checkouts at the same commit cook disjoint item partitions into the shared store, and the coordinator's ordinary cook then hits it; correctness rests on the derived-data keys alone | SharedCache.md "Distributed cooking" |
| CT-R7 | A batch operation keeps its own journal under the storage root's `.migration/content-ops/`, above the framework's per-batch journal, and every chunk is re-checkable by SHA-256 digests | BatchOperations.md "Operation journal" |
| CT-R8 | Source parsers live in a host-only library supported on `win` and `win-asan`, so the libFuzzer targets [Z2-N-ARCH-952] requires exist in the preset that builds fuzz targets | "Library and file layout" |

## Dependencies

### Libraries CONTENT depends on

| CONTENT target | Depends on | Why |
|---|---|---|
| `ZenithContentCore` (`Zenith2/Tools/ContentCore/`) | ZenithCore, ZenithDocument; third party (manifest names) `fastgltf` (with its `dependsOn` `simdjson`), `meshoptimizer`, `stb-image`, `dr-wav`, `dr-flac`, `dr-mp3`, `stb-vorbis` | containers, results, JSON, SHA-256, hashing, clocks; canonical serialization and the document tree for the merge engine; the parsers |
| CONTENT's files in `ZenithCook` (`Internal/Importers/`, `Internal/Content/`) | ZenithCook's own declared set (`Zenith2/Docs/Design/ASSET.md → "Libraries ASSET depends on"`), plus `ZenithContentCore` and `ZenithAuthoring` | the importer interface, the import context, ANIM's import helpers, GEOM's intermediate writers, KTX-Software for `KHR_texture_basisu` |
| CONTENT's files in `ZenithAuthoring` (`Internal/CONTENT/`, `Zenith_AuthoringContent.h`) | the edges CONTENT's move appends to ZenithAuthoring's `DEPENDS` (rule 4 of `Zenith2/Docs/Design/EDIT.md → "ZenithAuthoring contract rules"`, row ED50 (b)): ZenithDocument, ZenithScene, ZenithAssets, ZenithContentCore | ZenithDocument (migrators, canonical writing), ZenithScene (repair planner, canonical schemas), ZenithAssets (GUIDs, kinds), ZenithContentCore (the journal codec, validators and merge engine) |
| `ZenithContent` (`Zenith2/Tools/ZenithContent/`) | ZenithContentCore, ZenithAuthoring, ZenithEditorCore, ZenithDocument, ZenithDocumentHost, ZenithScene, ZenithAssets, ZenithCore | the merge, batch, storage, cache-administration and distribution commands; `Zenith_RegisterEditorDocumentTypes` and the editor cook client (EDIT's) |
| `Zenith_Integration_ContentPipeline` (`Zenith2/Tests/ContentPipeline/`) | ZenithContentCore, ZenithAuthoring, ZenithAssets, ZenithScene, ZenithRuntime | cook-then-load and tool end-to-end runs |

### Headings CONTENT relies on

| Document heading | What CONTENT takes from it |
|---|---|
| `Zenith2/Docs/Design/ASSET/Import.md → "Importer interface"`, "Zenith_ImporterDesc", "Zenith_ImportContext", "Settings resolution", "Identity preservation on reimport", "Boundary with CONTENT importers" | the interface the four importers implement, the context they read and declare through, the settings layers presets plug into, the identity rules overrides rely on |
| `Zenith2/Docs/Design/ASSET/Cook.md → "Worker processes"`, "Step registration functions", "Cook services and composition", "Output tree and database locks", "Keys and invalidation" | where importers run, how CONTENT's step registers, the lock rules batch operations respect, the keys distributed cooking relies on |
| `Zenith2/Docs/Design/ASSET/DDC.md → "Shared store contract"`, "Failure modes", "Key inputs" | the contract the file-share store implements and the client rules it never relaxes |
| `Zenith2/Docs/Design/ASSET/Database.md → "Sub-asset identity"`, "Rename and move detection", "Orphaned meta files and tombstones", "Dependency graph", "Provenance records" | GUID stability, move detection, tombstones, referrer edges, the provenance record carrying the origin object |
| `Zenith2/Docs/Design/ASSET/Formats.md → "Meta sidecar schema"`, "Cook server messages", "DDC entry file" | the `.meta` members CONTENT's schemas fill, the requests batch operations send, the entry bytes the store holds |
| `Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Canonical serialization"`, "Zenith_DocumentMigrator header", "Journaled write", "Storage-root writer lock", "Zenith_DocumentDiff header" | canonical writing for merges and generated documents, `SaveDocuments` for chunk writes, the writer lock held per chunk |
| `Zenith2/Docs/Design/SCENE/Prefabs.md → "Reference repair"`, "Override semantics per property kind", "Derivation"; `Zenith2/Docs/Design/SCENE/SceneAPI.md → "Zenith_ReferenceRepair header"`; `Zenith2/Docs/Design/SCENE/Format.md → "Prefab document schema"`, "Scene canonical schema" | the scene and prefab repair planner, the merge granularity per property kind, the derivation generated entity IDs use, the prefab document the importer generates |
| `Zenith2/Docs/Design/GEOM.md → "Import preset keys GEOM reads"`; `Zenith2/Docs/Design/GEOM/Textures.md → "Texture source image intermediate"`, "Texture usage classes"; `Zenith2/Docs/Design/GEOM/Materials.md → "Material document intermediate"`, "Material document schema"; `Zenith2/Docs/Design/GEOM/Meshes.md → "Mesh cook pipeline"`, "Position-only indices" | the keys importers declare (their names, `cull.customDistance`, and the `lod.positionOnlyThreshold` ratio), the blobs they write, the generated material documents |
| `Zenith2/Docs/Design/ANIM/Formats.md → "Import conversion"`, "Skeleton extraction from a glTF skin", "Document type table", "Clip annotation document schema" | the helper calls and the neutral structures the glTF importer fills; the per-clip and per-rig documents attached to sub-asset entries, which carry every per-clip value |
| `Zenith2/Docs/Design/SHIP/Manifest.md → "Licence fields"`; `Zenith2/Docs/Design/SHIP/Release.md → "Content licences"` | `licence_policy.json`'s `allowedContent`, the shippable-licence list CONTENT reads and never edits; the inventory that reads `provenance-licences.json` |
| `Zenith2/Docs/Design/AUDIO/Authoring.md → "Audio importer boundary"`; `Zenith2/Docs/Design/AUDIO/Formats.md → "Sound import settings"` | the decoded-audio blob and the eleven settings keys the audio importer declares |
| `Zenith2/Docs/Design/RPROD/Extractors.md → "Render component registration"`; `Zenith2/Docs/Design/LIGHT.md → "Flux_LightComponents header"` | the stable names and property IDs of the camera, mesh-instance and light components the generated prefab writes |
| `Zenith2/Docs/Design/CORE/Maths.md → "Coordinate systems"`, "Units", "Zenith_Sha256"; `Zenith2/Docs/Design/CORE/Text.md → "JSON reader"`, "JSON writer" | the conventions every importer converts into; digests; JSON |
| `Zenith2/Docs/Design/EDIT.md → "Tool-side code the editor needs"`; `Zenith2/Docs/Design/EDIT/Pipeline.md → "Asset operations"`, "Merge conflicts in documents", "Diff revert and resolve" | ZenithAuthoring's per-owner layout; where EDIT calls batch operations and the merge driver |
| `Zenith2/Docs/Rendering/VertexData.md → "Index buffers and mesh splitting"` | [Z2-N-REND-885]: counter-clockwise winding and the negative-determinant reversal |
| `Zenith2/Docs/Architecture.md → "Security architecture"`; `Zenith2/Docs/Architecture.md → "Third-party dependencies"` | [Z2-N-ARCH-951], [Z2-N-ARCH-952], [Z2-N-ARCH-953]; the dependency table the decoders join |
| `Zenith2/Docs/Architecture/BuildSystem.md → "Tools, benchmarks and integration suites"`, "The ZENITH_SUPPORTED matrix", "Test, conformance and fuzz targets" | `zenith_add_tool`, the matrix macros, fuzz-target creation in `win-asan` only |
| `Zenith2/Docs/Contracts.md → "Compatibility policy per kind"`, "Errors and results", "Contract blocks per type", "Tests and code hygiene gates", "Lock ordering table" | migration of the preset type, error enumerations, contract blocks, test naming, the watchdog lock's row |
| `Zenith2/Docs/Design/CORE/Diagnostics.md → "Clocks"`; `Zenith2/Docs/Design/CORE/Threading.md → "Threads"` | the system clock behind `Zenith_GetSystemContentClock`; the fault hook's blocking wait |
| `Zenith2/Docs/Budgets.md → "Package and storage budgets"`, "Editor iteration rows" | Z2-B-CONTENT-001 … -009, Z2-B-EDIT-004 |

### Areas CONTENT depends on

| Area | For |
|---|---|
| ASSET | the importer interface, the worker pool, the database, the cook server, the DDC client; the extensions of "Assumed interfaces pending other areas" |
| SCENE | canonical serialization, the document framework, the repair planner, prefab documents, derived authoring IDs |
| GEOM | the mesh source structure, the texture source blob, the material document, the preset keys |
| ANIM | the skeleton and clip import helpers |
| AUDIO | the decoded-audio layout and settings keys |
| RPROD, LIGHT | component stable names and property IDs for cameras, mesh instances and lights |
| PHYS | the collision importer settings and the collision source blob |
| EDIT | `ZenithAuthoring`'s layout, `ZenithEditorCore`'s document-type registration and cook client, the panels that display CONTENT's models |
| ECS | the schema document's structure for schema merges |
| ENV | the merge policy of terrain documents (row EN20) |
| CORE | containers, JSON, SHA-256, hashing, persistent IDs, logging |
| SHIP | the manifest and licence rows of the new decoders; the setup script's merge-driver installation |

### Areas that depend on CONTENT

| Area | Uses |
|---|---|
| ASSET | the importers registered at cook services item 4; the shared store at item 7; the `content.licences` step |
| GEOM | intermediate meshes, texture sources and generated material documents from the importers; preset values of its keys |
| ANIM | parsed, converted glTF nodes, skins and animations passed to its helpers |
| AUDIO | the `decoded-audio` blob and the declared settings keys (row AU10) |
| UI | the preset document format for `ui.sprite` (row UI11) |
| ENV | the merge driver's terrain-document policy (row EN20); the foliage mesh preset |
| EDIT | batch operations above `editor.assetOps.batchThreshold`, the merge driver behind "Open merge tool", preset documents in the generic document editor |
| SHIP | `provenance-licences.json` (row SH9) |
| CHAR | the importers as the first step of the character workflow |
| PACK | nothing directly: cooked files only |

### Upward interfaces CONTENT declares

| Interface | Declared in | Implemented by | Purpose |
|---|---|---|---|
| `Zenith_ContentCookChannel` | `Zenith2/Tools/Authoring/Zenith_AuthoringContent.h` | EDIT's `ZenithEditorCore` over `Zenith_EditorCookClient` (row CT18) | the cook-server requests a batch operation issues, so `ZenithAuthoring` never links cook code |
| `Zenith_ContentReferenceRepairer` | `Zenith2/Tools/Authoring/Zenith_AuthoringContent.h` | CONTENT's generic repairer and the SCENE adapter, both inside `Internal/CONTENT/Repair/` | per-document-type repair of references |

### Assumed interfaces pending other areas

<!-- assumption: pending ASSET -->
| Need | ASSET's assumed answer | Row |
|---|---|---|
| a top-level `.meta` member `physics` (PHYS's) carried verbatim like `provenance`; the cook server's `setSettings` writing the `.meta` changes batch operations need | ASSET's schema admits `physics`; `setSettings` gains optional `args` `preset` (GUID text or null), `provenance` (object or null), `subAssetMembers` (`[{key, members}]`), `remapSubAssets` (`[{from, to}]`), `removeOrphanedSubAssets` (`[key]`) and `restoreSubAssets` (recorded entries), with `settings` made optional, each journaled like `settings` | CT1, CT22 |
| one per-product mechanism: owner members on `.meta` sub-asset entries | a `subAssets` entry admits, beside `key`, `guid`, `kind`, `orphaned`, the owner members `exclude` and `materialRemap` (CONTENT), `annotation` and `rig` (ANIM) and the ASSET-written `fingerprint`; reconciliation keeps every member of an entry on reimport, moves them with the GUID on `RenameProduct` and on `remapSubAssets`, and keeps them when the entry is orphaned; the owner members' canonical text, in GUID order, joins the settings hash; `fingerprint` joins no key; a tombstoned GUID found on a live entry is accepted and leaves the tombstones; `record` returns `preset`, `provenance` and every entry with its members | CT22 |
| the import context exposes the previous run's sub-asset entries and records fingerprints | `Zenith_ImportContext::GetPreviousProducts()` returning `Zenith_Span<const Zenith_ImportPreviousProduct>` (`m_strKey`, `m_xGUID`, `m_eKind`, `m_bOrphaned`, `m_bHasFingerprint`, `m_ulFingerprint`, and `m_strMembersJson`, the owner members as canonical compact JSON text; key byte order); `SetProductFingerprint(const Zenith_AssetGUID&, uint64)` after `DeclareProduct` | CT2, CT22 |
| an importer contributes a cook-report member | `Zenith_ImportContext::AddReportMember(Zenith_StringView strMember, Zenith_ByteSpan xJsonValue)`, merged into the report's `members` by name like a step's (a name contributed more than once becomes an array), stored with the intermediates and replayed on a cached import | CT2 |
| the preset resolution refuses a preset of another importer, and the `zenith-doc` importer treats `*.importpreset.json` as a document with no product | settings resolution step 2 reads the preset's `importer` member and fails `IMPORT_ERROR_SETTING_INVALID` on a mismatch | CT3 |
| `.meta` creation asks CONTENT for the preset and lists the proposal | `Zenith_AssetDatabase::AssignMissingGuids` and the server's `assignGuids` call `Zenith_ImportPresetSet::Select`, write its GUID into the new `.meta`'s `preset`, and list `presets [{source, preset, glob}]` in their result | CT3, CT24 |
| CONTENT's importer and step registration in the cook services | item 4 registers `Zenith_RegisterContentImporters` directly after ASSET's passthrough importers and before every module's importers; item 6's fixed order gains CONTENT after SCENE and before the modules; item 5 registers `Zenith_RegisterContentDocumentTypes`; item 7 creates the shared store through `Zenith_CreateSharedDerivedDataStore` when `cook.ddc.sharedMode` is not `off` | CT4 |
| worker processes see the parent's step- and importer-owned cook keys | the pool starts `ZenithCook --worker <pipe>` with every `--set` argument of the parent's command line | CT5 |
| a global step reads the plan's origin metadata and a repository file | `Zenith_CookContext::GetPlanOrigins()` (global steps only) returning `Zenith_Span<const Zenith_CookPlanOrigin>` = `{GUID, kind, logical path, origin JSON text}` in GUID order; `Zenith_CookContext::ReadRepositoryFile(Zenith_StringView strZenith2RelativePath, uint64 ulMaxBytes)` for GLOBAL_POST steps in `AddKeyInputs` and `Cook`, whose file hash the driver adds to the step's key | CT6, CT24 |
| a cook restricted to some sources | the run description's existing `m_axOnly` exposed as `--only @<file>` (GUID lines; a source GUID selects all of its products), superseding the partition key of CT7 | CT23 |
| the DDC store interface, key types and entry verifier are reachable from `ZenithAuthoring` | ASSET moves `Zenith_DerivedDataStore`, `Zenith_DerivedDataStoreResult`, `Zenith_DerivedDataKey` and a verifier `Zenith_VerifyDerivedDataEntry(Zenith_ByteSpan, const Zenith_DerivedDataKey&) → Zenith_Result<Zenith_DerivedDataEntryInfo, Zenith_DerivedDataError>` (the success value carries the producer step ID and version) into `Zenith_AuthoringAsset.h`, the per-owner root header of `Zenith2/Docs/Design/EDIT.md → "Tool-side code the editor needs"` | CT8, CT24 |
| member-schema migration of `provenance`; a `.bin` buffer is not an unrecognised file; ASSET's example provenance object conforms | ASSET's constexpr member-migration table `Zenith_GetAssetMetaMemberMigrations()` in ZenithAuthoring's ASSET files (`Zenith2/Docs/Design/ASSET/Migration.md → "Member migrations"`), run inside the `.meta` type's visitor; a run-time registration function is not used, because a registry filled at start-up is mutable global state; the scan classifies a file recorded as another source's import input without `COOK_WARNING_UNRECOGNISED_FILE`; the `Formats.md` example carries `schema` and `origin` | CT24 |
| ZenithCook links CONTENT's libraries and hosts its subdirectories | ZenithCook's `DEPENDS` gains `ZenithContentCore` and `ZenithAuthoring`; `Internal/Importers/` and `Internal/Content/` are CONTENT's subdirectories | CT4 |
| engine presets ship in ZenithAssets' content root | `Zenith2/Engine/ZenithAssets/Internal/Content/ImportPresets/` is a CONTENT-owned subdirectory | CT3 |
<!-- /assumption -->

<!-- assumption: pending GEOM, ANIM, PHYS, SCENE, EDIT, ECS, ARCH, SHIP, LIGHT, WORLD, CORE -->
| Need | Owner | Assumption | Row |
|---|---|---|---|
| the intermediate mesh structure and writer | GEOM | `Zenith_CookMeshSource` and `Zenith_WriteCookMeshSource(const Zenith_CookMeshSource&, Zenith_Array<uint8>&) → Zenith_Result<void, Zenith_CookError>` in `Tools/ZenithCook/Internal/Geometry/Zenith_CookMeshSource.h`, blob name `mesh`, with the members Importers.md "Meshes" fills | CT9 |
| `Material_Standard`'s parameter and texture names beyond the four Rendering names, and the `texture.quality` value set | GEOM | `m_xEmissive`, `m_xEmissiveFactor`, `m_fNormalScale`, `m_fOcclusionStrength`; `texture.quality` is `low`, `medium`, `high` (default `high`) | CT10 |
| the types and defaults of `texture.quality`, `texture.uastcRdoLambda`, `texture.alphaCoverageCutoff`, `texture.virtual` and the range of `lod.positionOnlyThreshold`; GEOM's writer row for the image importer records the resolved, non-zero `texture.usage` | GEOM | Importers.md "Settings" assumption; `cull.customDistance` and the 0.9 ratio are GEOM's text already | CT10, CT21 |
| the complete clip import settings list, as source-wide defaults; ANIM's `annotation` and `rig` members on sub-asset entries as the one per-product mechanism; ANIM's helpers compute fingerprints, escape the reserved bytes of the names in their keys and cut two-name keys at 100 bytes | ANIM | the keys of Importers.md "Animations" settings table; the escaping of Importers.md "Product keys" rule 3 | CT11, CT25, CT33 |
| the collision source blob | PHYS | `Zenith_WritePhysicsCollisionSource` in `Tools/ZenithCook/Internal/Physics/Zenith_PhysicsIntermediates.h`, blob `physics.collisionSource`; product key `collision:shape`; the `physics.material` member holds a GUID | CT12, CT26 |
| generated prefab products | SCENE | `scene.prefab` cooks a product whose source is not a `.prefab.json` from its `scene.prefabDocument` intermediate, and the cook's and editor's prefab providers resolve such a prefab through it, so variants and instances of a model prefab resolve | CT13 |
| the `Zenith_ContentCookChannel` implementation (asynchronous: `Begin*`, `Poll`, `Cancel`) and the panels; the thread that runs a batch | EDIT | `ZenithEditorCore` implements the channel, whose FORWARD mode queues each request for the main thread and polls its completion without waiting; EDIT's panels show batch progress and reports, preset documents and their proposals, sub-asset entry members and merge results; the editor runs `Zenith_ContentBatchOperation::Plan` and `Run` (to PLANNED, then to the end) on its `EditorContentBatch` thread, which is therefore the operation's owner thread | CT18, CT27 (b), (c), CT30 |
| a canonical schema for component-schema documents | ECS | none required: Merge.md "Schema rules" merges them by their own keys | CT14 |
| parser-limit rows, the decoders in the dependency table, ZenithContentCore in the tools tables, the new tool directory in the layout | ARCH | X rows of `reviews/CROSS_DOC_ITEMS.md` | CT15 |
| manifest entries and licence rows of the decoders under lower-case kebab names, fastgltf's `dependsOn` `simdjson`; merge-driver installation in `Setup-Zenith2.ps1`; `allowedContent` of `licence_policy.json` as the one shippable-licence list; git-lfs installed and pulled on every gate host and lane | SHIP | rows added by the integrating ticket; the setup script calls `ZenithContent merge --install` and installs git-lfs (`git lfs install`, `git lfs pull`), a Stage 3 prerequisite of the recorded-fixture decoder tests; CONTENT reads `allowedContent` and never writes it | CT16, CT28, CT32 |
| merge policies for WORLD's documents; whether the bake documents are always LFS | WORLD | the WORLD rows of Merge.md "Merge policies" (answers WD16, WD27); the bake documents stay plain text until WORLD chooses (SharedCache.md "Storage policy check") | CT29, CT31 |
| spot-light cone angles are half-angles | LIGHT | `m_fInnerAngleDegrees` and `m_fOuterAngleDegrees` are measured from the cone axis | CT17 |
| deterministic transcendental functions for every result that reaches an intermediate | CORE | CORE's deterministic transcendental functions (X314), named `Zenith_Sin`, `Zenith_Cos`, `Zenith_Atan2` (WORLD's row WD29) and assumed `Zenith_Exp`, `Zenith_Log`, `Zenith_Pow` until CORE's follow-up fixes the names (Importers.md "Zenith_MediaDecoders header") | X314 (ruling X314-FMA3) |
<!-- /assumption -->

## Library and file layout

### Target summary

| Target | Kind | Directory | Public headers | `ZENITH_SUPPORTED` |
|---|---|---|---|---|
| `ZenithContentCore` | static tool library, `zenith_add_tool(ZenithContentCore DEPENDS ZenithCore ZenithDocument THIRD_PARTY fastgltf simdjson meshoptimizer stb-image dr-wav dr-flac dr-mp3 stb-vorbis SUPPORTED ZENITH_WINDOWS)` (manifest entry names, lower-case kebab; no `Internal/Zenith_ZenithContentCoreMain.cpp`, so a static library) | `Zenith2/Tools/ContentCore/` | the five headers of "ZenithContentCore public headers" | `ZENITH_WINDOWS` (`win`, `win-asan`): host-only, CPU-only, and its fuzz targets need `win-asan` |
| `ZenithContentCore_Tests` | test executable | `Zenith2/Tools/ContentCore/Tests/` | — | `win`, `win-asan` |
| CONTENT's ZenithCook files | inside ASSET's tool `ZenithCook` | `Zenith2/Tools/ZenithCook/Internal/Importers/`, `Internal/Content/`; tests `Tests/Importers/`, `Tests/Content/` | none (tool internals) | `ZENITH_TOOLS_HOST` |
| CONTENT's ZenithAuthoring files | inside the tool library `ZenithAuthoring` (`Zenith2/Docs/Design/EDIT.md → "Tool-side code the editor needs"`: one owner per `Internal/<Area>/` subdirectory and one root header per owner) | `Zenith2/Tools/Authoring/Zenith_AuthoringContent.h`, `Zenith2/Tools/Authoring/Internal/CONTENT/`; tests `Zenith2/Tools/Authoring/Tests/CONTENT/` | `Zenith_AuthoringContent.h` | `ZENITH_TOOLS_HOST` |
| `ZenithContent` | tool executable, `zenith_add_tool(ZenithContent DEPENDS ZenithContentCore ZenithAuthoring ZenithEditorCore ZenithDocument ZenithDocumentHost ZenithScene ZenithAssets ZenithCore)` | `Zenith2/Tools/ZenithContent/` | none | `ZENITH_TOOLS_HOST` |
| `ZenithContent_Tests` | test executable over `ZenithContent_Objects` | `Zenith2/Tools/ZenithContent/Tests/` | — | `win` |
| `Zenith_Integration_ContentPipeline` | integration suite | `Zenith2/Tests/ContentPipeline/` | — | `win` |
| `Zenith_Bench_FbxFidelity` | the D-SEL-FBX harness (Z2-D-084) | `Zenith2/Benchmarks/FbxFidelity/` | — | `win` |

<!-- informative -->
`ZenithContentCore` exists because [Z2-N-ARCH-952] requires a libFuzzer target for every parser of external bytes, image and audio decoders included, and fuzz targets are created in `win-asan` alone (`Zenith2/Docs/Architecture/BuildSystem.md → "Test, conformance and fuzz targets"`), while ZenithCook and ZenithAuthoring are `ZENITH_TOOLS_HOST` (`win` alone). The parsers therefore live in a library that depends on nothing with a GPU stack, so it configures in `win-asan`; the importers that implement ASSET's interface stay in ZenithCook and call it. `linux-tsan` is excluded because fastgltf's manifest platforms are `windows`.
<!-- /informative -->

### ZenithContentCore public headers

| Header (under `Zenith2/Tools/ContentCore/`, included as `"ContentCore/<name>"`) | Declares | Specified in |
|---|---|---|
| `Zenith_ContentTypes.h` | `Zenith_ContentErrorCode`, `Zenith_ContentError`, `Zenith_ContentImportDetail`, the version constants and the 39 parser-limit constants, `Zenith_ContentLimitKind`, `Zenith_ContentLimitRow`, `Zenith_GetContentLimits`, `Zenith_GltfCounts`, `Zenith_ImageCounts`, `Zenith_AudioCounts`, `Zenith_CheckGltfCounts`, `Zenith_CheckImageCounts`, `Zenith_CheckAudioCounts`, `Zenith_ContentClock`, `Zenith_GetSystemContentClock`, `Zenith_PfnContentFreeBytes`, `Zenith_ToString` overloads | Importers.md "ZenithContentCore API", "Import detail codes"; BatchOperations.md "Batch API" |
| `Zenith_ContentConversion.h` | `Zenith_ContentAxis`, `Zenith_ContentMirror`, `Zenith_ContentConversionDesc`, `Zenith_ContentConversion`, `Zenith_MakeContentConversion` | DccMatrix.md "Basis change algorithm" |
| `Zenith_GltfSource.h` | `Zenith_GltfSourceDesc`, `Zenith_GltfSourceScene` and its element structs, `Zenith_ParseGltfSource`, `Zenith_GltfReadCallback` | Importers.md "ZenithContentCore API" |
| `Zenith_MediaDecoders.h` | `Zenith_DecodedImage`, `Zenith_DecodeImageSource`, `Zenith_DecodedAudio`, `Zenith_AudioDecodeStats`, `Zenith_AudioDecodeOptions`, `Zenith_DecodeAudioSource` (two overloads), `Zenith_WriteDecodedAudioBlob` | Importers.md "Image importer", "Audio importer" |
| `Zenith_ContentMerge.h` | `Zenith_ContentMergeMode`, `Zenith_ContentArrayPolicy`, `Zenith_ContentArrayRule`, `Zenith_ContentMergePolicy`, `Zenith_ContentMergeInput`, `Zenith_ContentMergeDecision`, `Zenith_ContentMergeResult`, `ulCONTENT_MERGE_MAX_INPUT_BYTES`, `uCONTENT_MERGE_MAX_EDIT_DISTANCE`, `Zenith_MergeDocuments3`, `Zenith_MergeText3` | Merge.md "Merge API" |
| `Zenith_ContentValidation.h` | `Zenith_ContentSettingView`, `Zenith_ImporterSettingSchemaView`, `Zenith_ContentValidationMessage`, `Zenith_ValidateImportPresetJson`, `Zenith_ContentSubAssetView`, `Zenith_ValidateProductOverridesJson`, `Zenith_ValidateContentProvenanceJson`, `Zenith_ContentLicenceInfo`, `Zenith_FindContentLicence`, `Zenith_IsContentLicenceShippable`, the journal views `Zenith_ContentBatchPlanView`, `Zenith_ContentBatchStateView`, `Zenith_ContentJournalPrepared`, `Zenith_ContentJournalChunk`, `Zenith_ContentJournalMeta` and their element structs, `Zenith_ReadContentBatchPlan`, `Zenith_WriteContentBatchPlan`, `Zenith_ReadContentBatchState`, `Zenith_WriteContentBatchState`, `Zenith_ReadContentBatchPrepared`, `Zenith_WriteContentBatchPrepared`, `Zenith_ReadContentBatchChunk`, `Zenith_WriteContentBatchChunk`, `Zenith_ReadContentBatchMeta`, `Zenith_WriteContentBatchMeta`, `Zenith_ComputeContentBatchPlanSha256` | PresetsAndReimport.md "Preset validation", "Product overrides schema"; SharedCache.md "Content provenance", "Licence table"; BatchOperations.md "Operation journal" |

### ZenithContentCore internal files

| Path (under `Zenith2/Tools/ContentCore/`) | Content |
|---|---|
| `CMakeLists.txt` | the `zenith_add_tool(ZenithContentCore …)` call |
| `Internal/Zenith_ContentTypes.cpp` | `Zenith_ToString` bodies |
| `Internal/Conversion/Zenith_ContentConversion.cpp` | the basis matrix, transform, direction and quaternion conversion |
| `Internal/Gltf/Zenith_GltfParse.cpp`, `.h` | fastgltf parsing over memory, the read callback for external buffers and images, extension gating |
| `Internal/Gltf/Zenith_GltfAccessors.cpp`, `.h` | accessor decoding (sparse, normalized, quantized, meshopt-compressed buffer views) into `float`/`uint32` arrays |
| `Internal/Gltf/Zenith_GltfScene.cpp`, `.h` | nodes, world transforms, negative-determinant flags, meshes, skins, morphs, animations, materials, cameras, lights into `Zenith_GltfSourceScene`, applying the conversion |
| `Internal/Gltf/Zenith_GltfLimits.cpp` | the limit checks of Importers.md "Parser limits" |
| `Internal/Media/Zenith_ImageDecode.cpp`, `Zenith_AudioDecode.cpp`, `Zenith_AudioChunks.cpp`, `Zenith_DecodedAudioBlob.cpp` | stb_image, dr_wav, dr_flac, dr_mp3 and stb_vorbis wrappers, WAV and AIFF loop and cue chunk parsing, Vorbis-comment loop tags, the `ZAPC` writer |
| `Internal/Merge/Zenith_ContentMerge3.cpp`, `Zenith_ContentMergeKeyed.cpp`, `Zenith_ContentMergeWriter.cpp`, `Zenith_ContentTextMerge3.cpp` | the structural merge, keyed and set arrays, the conflict-marker writer, the diff3 text fallback |
| `Internal/Validation/Zenith_ContentValidation.cpp`, `Zenith_ContentLicences.cpp`, `Zenith_ContentBatchJournal.cpp` | the three JSON validators, the licence table, the journal codec |
| `Internal/ThirdParty/Zenith_StbImageUnit.cpp`, `Zenith_DrLibsUnit.cpp`, `Zenith_StbVorbisUnit.cpp` | the single translation units that define the single-file libraries' implementations (dr-wav, dr-flac and dr-mp3 in `Zenith_DrLibsUnit.cpp`), with their allocation routes to `Zenith_Heap` (Importers.md "Zenith_MediaDecoders header") and `#pragma warning` suppressions under a decision ID (the CT-R2 ruling) |
| `Internal/Zenith_ContentClock.cpp` | `Zenith_GetSystemContentClock` over CORE's `Zenith_GetMonotonicTimeUs` and `Zenith_GetUtcTime` |

### ZenithContentCore test files

| Path (under `Zenith2/Tools/ContentCore/Tests/`) | Content |
|---|---|
| `Zenith_ContentConversion_Tests.cpp`, `Zenith_GltfSource_Tests.cpp`, `Zenith_GltfAccessors_Tests.cpp`, `Zenith_GltfLimits_Tests.cpp`, `Zenith_MediaDecoders_Tests.cpp`, `Zenith_ContentMerge_Tests.cpp`, `Zenith_ContentTextMerge_Tests.cpp`, `Zenith_ContentValidation_Tests.cpp`, `Zenith_ContentBatchJournal_Tests.cpp` | Tests.md "ContentCore tests" |
| `Support/Zenith_TestGltfBuilder.h`, `Support/Zenith_TestPngWriter.h`, `Support/Zenith_TestWavWriter.h`, `Support/Zenith_TestFlacWriter.h`, `Support/Zenith_TestMergeSchemas.h`, `Support/Zenith_ScriptedContentClock.h`, `Support/Zenith_RecordedFixtures.h` | header-only builders that write `.gltf` text with data URIs, uncompressed-deflate PNG, PCM WAV and verbatim-subframe FLAC in memory, the merge test schemas, the scripted clock and the recorded-fixture loader that refuses git-lfs pointer text ([Z2-N-ARCH-255]; non-Core `Tests/Support/` is header-only) |
| `Fuzz/GltfSource.cpp`, `Fuzz/ImageSource.cpp`, `Fuzz/AudioSource.cpp`, `Fuzz/MergeInput.cpp`, `Fuzz/BatchJournal.cpp`, `Fuzz/ContentValidation.cpp` | libFuzzer targets `Zenith_Fuzz_GltfSource`, `_ImageSource`, `_AudioSource`, `_MergeInput`, `_BatchJournal`, `_ContentValidation`, seeded from the builders at start-up |
| `Fixtures/Recorded/` | the needs-human recorded Ogg Vorbis and MP3 fixtures and the baseline and progressive JPEGs, with their `RECORDING.md` (Tests.md "Needs-human recordings"); no other committed binary |

### CONTENT's ZenithCook files

| Path (under `Zenith2/Tools/ZenithCook/`) | Content |
|---|---|
| `Internal/Importers/Zenith_ContentImporters.h`, `.cpp` | `Zenith_RegisterContentImporters` |
| `Internal/Importers/Zenith_GltfImporter.cpp`, `.h` | the `gltf` importer |
| `Internal/Importers/Zenith_GltfProducts.cpp`, `.h` | product keys, rename recognition, override application |
| `Internal/Importers/Zenith_GltfMeshes.cpp`, `Zenith_GltfMaterials.cpp`, `Zenith_GltfTextures.cpp`, `Zenith_GltfPrefab.cpp` | meshes into GEOM's mesh source, generated material documents, embedded and packed texture sources, the generated model prefab |
| `Internal/Importers/Zenith_ImageImporter.cpp`, `.h`, `Zenith_AudioImporter.cpp`, `.h`, `Zenith_FbxImporter.cpp`, `.h` | the other three importers |
| `Internal/Importers/Zenith_ContentFaultSource.cpp`, `.h` | the `cook.content.faultSource` hook |
| `Internal/Importers/Windows/Zenith_FbxConverterProcess_Windows.cpp` | the converter child process of the FBX route |
| `Internal/Content/Zenith_ContentCookSteps.h`, `.cpp` | `Zenith_RegisterContentCookSteps`, the `content.licences` step |
| `Tests/Importers/`, `Tests/Content/`, `Tests/Fixtures/Content/` | Tests.md "Importer tests", "Convention tests", "Provenance and storage tests"; the text fixtures and the recorded DCC fixtures |

### CONTENT's ZenithAuthoring files

| Path (under `Zenith2/Tools/Authoring/`) | Content |
|---|---|
| `Zenith_AuthoringContent.h` | the one public root header of CONTENT in ZenithAuthoring (`#include "Authoring/Zenith_AuthoringContent.h"`), "Public API" |
| `Internal/CONTENT/Zenith_ContentDocumentTypes.cpp` | `Zenith_RegisterContentDocumentTypes`, the preset canonical schema |
| `Internal/CONTENT/Presets/Zenith_ImportPresetSet.cpp` | preset matching |
| `Internal/CONTENT/Batch/Zenith_ContentBatch.cpp`, `Zenith_ContentBatchPlan.cpp`, `Zenith_ContentBatchChunking.cpp`, `.h` (the pure packing function), `Zenith_ContentMetaClassify.cpp` (BatchOperations.md "Meta element classification"), `Zenith_ContentBatchRun.cpp`, `Zenith_ContentBatchResume.cpp`, `Zenith_ContentBatchReport.cpp`, `Zenith_ContentBatchLock.cpp` | the batch engine |
| `Internal/CONTENT/Batch/Windows/Zenith_ContentFileMoves_Windows.cpp` | `MoveFileExW` moves with `MOVEFILE_WRITE_THROUGH` |
| `Internal/CONTENT/Repair/Zenith_ContentRepair.cpp`, `Zenith_ContentGenericRepairer.cpp`, `Zenith_ContentSceneRepairer.cpp` | the repairer registry, the generic repairer, the SCENE adapter |
| `Internal/CONTENT/Merge/Zenith_ContentMergePolicies.cpp` | the per-type merge policies built from the migration registry |
| `Internal/CONTENT/SharedDDC/Zenith_FileShareDerivedDataStore.cpp`, `.h`, `Zenith_FileShareStoreAdmin.cpp`, `Windows/Zenith_FileShareStore_Windows.cpp` | the file-share store, its watchdog (`uLOCK_LEVEL_CONTENT_DDC_WATCHDOG`) and its administration |
| `Tests/CONTENT/` | Tests.md "Preset and reimport tests", "Batch tests", "Shared cache tests" |

### ZenithContent files

| Path (under `Zenith2/Tools/ZenithContent/`) | Content |
|---|---|
| `CMakeLists.txt` | the `zenith_add_tool(ZenithContent …)` call |
| `Internal/Zenith_ZenithContentMain.cpp` | `main`: root heap, hooks, the subcommand dispatch of "Batch command line", "Driver command line", "Store administration", "Partition protocol", "Storage policy check" |
| `Internal/Merge/Zenith_ContentMergeCommand.cpp` | the git merge driver and `--install`, `--print-gitattributes` |
| `Internal/Batch/Zenith_ContentBatchCommand.cpp` | `batch plan|run|resume|status|abandon|rollback|discard` |
| `Internal/Storage/Zenith_ContentStorageCheck.h`, `.cpp` | `storage-check`, `Zenith_RunContentStorageCheck` over an injectable git runner (SharedCache.md "Storage policy check") |
| `Internal/SharedDDC/Zenith_ContentDdcCommand.cpp` | `ddc init|stats|verify|prune` |
| `Internal/Distribute/Zenith_ContentDistribute.h`, `Zenith_ContentCoordinator.cpp`, `Zenith_ContentHelper.cpp` | `distribute`, `helper`, `Zenith_DerivePartitionState`, `Zenith_RunContentCoordinator` (SharedCache.md "Partition protocol") |
| `Internal/Windows/Zenith_ContentProcess_Windows.cpp` | child processes (`git`, `cmake`, `ZenithCook`) with argument vectors, `Zenith_RunGitProcess` |
| `Internal/Config/storage.config.json` | the storage policy values written by the Stage 1 per-system budget ticket for CONTENT |
| `Tests/` | Tests.md "Merge tests", "Provenance and storage tests" (the command-level cases) |

### Integration suite and harness files

| Path | Content |
|---|---|
| `Zenith2/Tests/ContentPipeline/CMakeLists.txt` | `zenith_add_integration_tests(ContentPipeline DEPENDS ZenithContentCore ZenithAuthoring ZenithAssets ZenithScene ZenithRuntime SUPPORTED win)` |
| `Zenith2/Tests/ContentPipeline/Zenith_ContentPipeline_Tests.cpp` | Tests.md "Integration suite" |
| `Zenith2/Tests/ContentPipeline/Support/Zenith_ContentGameFixture.h` | header-only `Zenith_WriteContentGameFixture`, called by the fixture-setup case and by each `z2-stress` case for its own copy |
| `${ZENITH_OUT}/test-fixtures/CONTENT/ContentGame/` (out of tree; no committed file) | the fixture game the fixture-setup case writes (Tests.md "Fixture generation"); `Zenith2/Tests/ContentPipeline/` holds no fixture directory, so no generated file can be committed there (Q55) |
| `Zenith2/Benchmarks/FbxFidelity/` | `Zenith_Bench_FbxFidelity`: the D-SEL-FBX converter prototypes and the fidelity comparison (DccMatrix.md "FBX route"), with a `--smoke` CTest run |

### ZENITH_SUPPORTED matrix

`ZenithContentCore` declares `ZENITH_WINDOWS`; every other CONTENT target is `ZENITH_TOOLS_HOST` because it depends on ZenithCook's, ZenithAuthoring's or ZenithEditorCore's GPU-module closure ([Z2-N-ARCH-172]). Nothing of CONTENT is in any Player, Shipping link closure or Android preset ([Z2-N-ARCH-241]).

## Public API

### API map

| Question a caller has | Answer |
|---|---|
| how is a glTF file parsed into engine conventions? | `Zenith_ParseGltfSource` (`ContentCore/Zenith_GltfSource.h`) with a `Zenith_ContentConversion` |
| how is an image or an audio file decoded? | `Zenith_DecodeImageSource`, `Zenith_DecodeAudioSource` (`ContentCore/Zenith_MediaDecoders.h`) |
| how are the importers registered? | `Zenith_RegisterContentImporters` (ZenithCook `Internal/Importers/Zenith_ContentImporters.h`) at cook services item 4 |
| how is a preset chosen for a new source? | `Zenith_ImportPresetSet::Select` (`Authoring/Zenith_AuthoringContent.h`) |
| how does the editor move 500 assets? | on EDIT's `EditorContentBatch` thread: build a `Zenith_ContentBatchSpec`, `Zenith_ContentBatchOperation::Plan`, `Run` (planning, to PLANNED), check the planned documents, `Run` again (execution), all over the asynchronous `Zenith_ContentCookChannel` in FORWARD mode; `ZenithContent` does the same on its main thread; a tool that pumps its own main thread calls `Step` with a budget instead of `Run` |
| how is an interrupted batch continued? | `Zenith_ContentBatchOperation::Open` then `Run` (or `Step`) on the PAUSED operation, or `ZenithContent batch resume` |
| how are three versions of a document merged? | `Zenith_MergeDocuments3` with the policy from `Zenith_BuildContentMergePolicies`, or `ZenithContent merge` from git |
| how does a cook reach the shared cache? | the cook services create `Zenith_FileShareDerivedDataStore` through `Zenith_CreateSharedDerivedDataStore` and hand it to ASSET's client |

### Rules across the CONTENT API

- Every CONTENT type follows the tool-side conventions ASSET states for ZenithCook (`Zenith2/Docs/Design/ASSET.md → "Rules across the ASSET API"`): Zenith Hungarian, one contract block per type with Ownership, Thread affinity, Invalidation, Units, Errors and Version (plus Reentrancy for interfaces), every fallible function returning `Zenith_Result<T, E>` ([Z2-N-CONTRACT-111]).
- The public headers of `ZenithContentCore` and `Zenith_AuthoringContent.h` include no STL, third-party or platform header; the tools' `Internal/` files and tests may use the STL ([Z2-N-CONTRACT-275]) but never smart pointers.
- Every byte read from a source file, a JSON document, a journal, a cache entry or a git object is untrusted: sizes, counts and offsets are checked with overflow-checked arithmetic, and failure is a `Zenith_Result` error, never an assert ([Z2-N-ARCH-951]).
- Objects with fallible construction use a static `Create` returning `Zenith_Result<Zenith_Owned<T>, Zenith_ContentError>`, unwinding what they built on failure, with one fault point per fallible step (prefix `0x4354`, "CT").
- Importers return ASSET's `Zenith_ImportError`; the detail of a source error is a `Zenith_ContentImportDetail` reported through the context.

### Errors

```cpp
// Zenith2/Tools/ContentCore/Zenith_ContentTypes.h
/// @contract enum Zenith_ContentErrorCode
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; each enumerator's meaning is the table "Content error codes".
/// Version: since Z2 Stage 3. Tool-side; values are appended, never renumbered ([Z2-N-CONTRACT-467]).
/// @endcontract
enum Zenith_ContentErrorCode : uint8
{
	CONTENT_ERROR_INVALID_ARGUMENT = 1,
	CONTENT_ERROR_OUT_OF_MEMORY,
	CONTENT_ERROR_IO,
	CONTENT_ERROR_LIMIT_EXCEEDED,
	CONTENT_ERROR_SOURCE_INVALID,
	CONTENT_ERROR_UNSUPPORTED,
	CONTENT_ERROR_DOCUMENT,
	CONTENT_ERROR_PRESET_INVALID,
	CONTENT_ERROR_OVERRIDES_INVALID,
	CONTENT_ERROR_PROVENANCE_INVALID,
	CONTENT_ERROR_LICENCE_FORBIDDEN,
	CONTENT_ERROR_BATCH_BUSY,
	CONTENT_ERROR_BATCH_NOT_FOUND,
	CONTENT_ERROR_BATCH_JOURNAL_CORRUPT,
	CONTENT_ERROR_BATCH_STATE,
	CONTENT_ERROR_BATCH_CONFLICT,
	CONTENT_ERROR_BATCH_CANCELLED,
	CONTENT_ERROR_SPEC_INVALID,
	CONTENT_ERROR_REPAIR_FAILED,
	CONTENT_ERROR_COOK_CHANNEL,
	CONTENT_ERROR_SOURCE_LOCKED,
	CONTENT_ERROR_LOW_DISK,
	CONTENT_ERROR_MERGE_INPUT_INVALID,
	CONTENT_ERROR_STORE_UNUSABLE,
	CONTENT_ERROR_GIT,
	CONTENT_ERROR_DISTRIBUTE,
};

/// @contract value Zenith_ContentError
/// Ownership: Plain value, 8 bytes, trivially copyable ([Z2-N-CONTRACT-480]).
/// Thread affinity: IMMUTABLE. Members non-const: an error record is copy-assigned into the first-error and last-error
///   members of mutable state.
/// Invalidation: None.
/// Units: m_uDetail is the code of the layer below (a Zenith_DocumentErrorCode, Zenith_SceneErrorCode, Zenith_CookErrorCode
///   text index, Zenith_ImportError or Zenith_ContentImportDetail) as the code's row states; m_uIndex a chunk, target or line.
/// Errors: None.
/// Version: since Z2 Stage 3. Tool-side.
/// @endcontract
struct Zenith_ContentError
{
	Zenith_ContentErrorCode m_eCode;
	uint8 m_uReserved = 0;
	uint16 m_uDetail = 0;
	uint32 m_uIndex = 0;
};
static_assert(sizeof(Zenith_ContentError) == 8);
constexpr const char* Zenith_ToString(Zenith_ContentErrorCode eCode);
```

| Code | Returned when | `m_uDetail` |
|---|---|---|
| `INVALID_ARGUMENT` | a desc or argument violates its contract (a null heap, an unknown game) | 0 |
| `OUT_OF_MEMORY` | an allocation failed; the object is unchanged | 0 |
| `IO` | a file, pipe or child-process operation failed | the OS error's low 16 bits |
| `LIMIT_EXCEEDED` | a parser limit of Importers.md "Parser limits" | the `Zenith_ContentImportDetail` |
| `SOURCE_INVALID` | malformed source bytes | the `Zenith_ContentImportDetail` |
| `UNSUPPORTED` | a required glTF extension or a source feature outside the matrix | the `Zenith_ContentImportDetail` |
| `DOCUMENT` | a document-framework error | the `Zenith_DocumentErrorCode` |
| `PRESET_INVALID` | a preset document fails "Preset validation" | the failing rule's index |
| `OVERRIDES_INVALID` | a CONTENT member of a `.meta` sub-asset entry fails rules O1 … O7 | the failing rule's index |
| `PROVENANCE_INVALID` | an origin object fails its schema | the failing rule's index |
| `LICENCE_FORBIDDEN` | an origin names a licence the licence table forbids | the licence table row |
| `BATCH_BUSY` | another batch holds the storage root's content-ops lock | 0 |
| `BATCH_NOT_FOUND` | no journal of that operation ID | 0 |
| `BATCH_JOURNAL_CORRUPT` | the operation journal fails its schema or digest checks | the failing rule's index |
| `BATCH_STATE` | a request not allowed in the operation's state (resume of a COMPLETED operation) | the `Zenith_ContentBatchState` |
| `BATCH_CONFLICT` | a chunk's files changed outside the operation | the chunk index in `m_uIndex` |
| `BATCH_CANCELLED` | the caller cancelled at a chunk boundary | 0 |
| `SPEC_INVALID` | a batch specification fails its schema | the failing rule's index |
| `REPAIR_FAILED` | SCENE's planner or the generic repairer failed | the `Zenith_SceneErrorCode` or 0 |
| `COOK_CHANNEL` | a cook request failed or the server is unreachable | the request's `COOK_ERROR_*` index |
| `SOURCE_LOCKED` | a storage root's writer lock stayed held | 0 |
| `LOW_DISK` | the framework or the batch's own free-space check refused | 0 |
| `MERGE_INPUT_INVALID` | the driver found an input file above 2 GiB, which it does not read (it writes the marker-only block of Merge.md "Driver command line" and exits 1); an input over 64 MiB or not UTF-8 is a whole-file conflict block, never this error | 0 |
| `STORE_UNUSABLE` | a shared store root is absent, unwritable for a writer, or lacks its marker file | 0 |
| `GIT` | a `git` command failed or is absent | the exit code |
| `DISTRIBUTE` | the partition protocol's files are malformed | 0 |

`Zenith_ContentImportDetail` is Importers.md "Import detail codes".

### Zenith_AuthoringContent header

The full declarations are in the split files; the header's members are:

| Declaration | Specified in |
|---|---|
| `Zenith_RegisterContentDocumentTypes`, `Zenith_GetImportPresetCanonicalSchema` | PresetsAndReimport.md "Preset document type registration" |
| `Zenith_ImportPresetEntry`, `Zenith_ImportPresetSet` | PresetsAndReimport.md "Preset matching" |
| `Zenith_ContentCookChannel` and its value types (`Zenith_ContentCookRecord`, `Zenith_ContentCookFailure`, `Zenith_ContentCookResult`, `Zenith_ContentSubAssetMembers`, `Zenith_ContentSetSettingsRequest`, `Zenith_ContentCookListRequest`, `Zenith_ContentCookRequestKind`, `Zenith_ContentCookTicket`, `Zenith_ContentCookPoll`, `Zenith_ContentCookResponse`) | BatchOperations.md "Cook channel" |
| `Zenith_ContentBatchKind`, `Zenith_ContentBatchState`, `Zenith_ContentChunkState`, `Zenith_ContentBatchSpec` and its elements, `Zenith_ContentBatchDesc`, `Zenith_ContentBatchProgress`, `Zenith_ContentBatchStepResult`, `Zenith_ContentBatchOperation`, the fault-point constants | BatchOperations.md "Batch API" |
| `Zenith_ContentReferenceRepairer`, `Zenith_ContentRepairRequest`, `Zenith_ContentRepairEdit` | BatchOperations.md "Generic reference repairer" |
| `Zenith_BuildContentMergePolicies` | Merge.md "Merge policies" |
| `Zenith_FileShareStoreDesc`, `Zenith_FileShareStoreStats`, `Zenith_CreateSharedDerivedDataStore`, `Zenith_QueryFileShareStoreStats` | SharedCache.md "File-share store" |
| `Zenith_FileShareAdminDesc`, `Zenith_FileShareVersionStats`, `Zenith_FileShareAdminStats`, `Zenith_FileShareVerifyOptions`, `Zenith_FileShareFindingKind`, `Zenith_FileShareVerifyFinding`, `Zenith_FileShareVerifyResult`, `Zenith_FileSharePruneOptions`, `Zenith_FileSharePruneResult`, `Zenith_FileShareStoreAdmin` | SharedCache.md "Store administration" |

### Handle kinds used

| Handle | Kind (`Zenith2/Docs/Contracts.md → "Handle kinds"`) | CONTENT's role |
|---|---|---|
| `Zenith_AssetGUID` (`Zenith_PersistentID`) | persistent ID | sub-asset GUIDs by ASSET's rules; `materialRemap` keys and values; replacement GUIDs in repairs |
| `Zenith_PersistentID` of generated entities | persistent ID (authoring ID) | derived per Importers.md "Generated model prefab" |
| `Zenith_OperationHandle` | operation handle | not used: batch operations are tool objects run on their owner thread (`Run` or `Step`), not engine operations |

CONTENT holds no world, device or shared asset handle: it is tool-side only.

## Data structures and formats

### Format index

| Format | Kind | Specified in |
|---|---|---|
| import preset document `*.importpreset.json` (`zenith2.import-preset` v1) | authoring document, migrated | PresetsAndReimport.md "Preset document schema" |
| `.meta` sub-asset entry members `exclude`, `materialRemap` (CONTENT's) and `fingerprint` (ASSET-written from the importer) | part of an authoring document | PresetsAndReimport.md "Product overrides schema", "Product fingerprints" |
| `.meta` member `provenance` (CONTENT's origin schema v1) | part of an authoring document | SharedCache.md "Content provenance" |
| batch specification (`zenith2.content-batch-spec` v1) | tool input JSON | BatchOperations.md "Batch specification" |
| operation journal: `<opId>/plan.json` (`zenith2.content-batch-plan` v1, immutable), `<opId>/state.json` (`zenith2.content-batch-state` v1), `<opId>/chunk-<k>.prepared.json` (`zenith2.content-batch-chunk-prepared` v1, immutable), `<opId>/chunk-<k>.json` (`zenith2.content-batch-chunk` v1), `<opId>/meta/<k>/<n>.json` (`zenith2.content-batch-meta` v1, immutable), with the staged `before/<k>/` and `after/<k>/` copies | tool state, canonical JSON | BatchOperations.md "Operation journal" |
| batch report `report.json` (`zenith2.content-batch-report` v1) | tool output JSON | BatchOperations.md "Batch report" |
| merge conflict-marker text | text inside a document | Merge.md "Conflict markers" |
| shared store layout and marker `store.json` (`zenith2.shared-ddc` v1) | directory layout plus a JSON marker; entries are ASSET's `.zddc` | SharedCache.md "Store layout" |
| partition job files `job.json`, `partition-<k>.txt`, `claim-<k>.json`, `heartbeat-<k>.json`, `done-<k>.json`, `cancel.json` (`zenith2.content-distribute` v1); the helper's `tool-stamp.json` | coordination JSON on a share; the helper's tool pin | SharedCache.md "Partition protocol" |
| `provenance-licences.json` | cooked output JSON | SharedCache.md "Content licences step" |
| `storage.config.json` (`zenith2.content-storage` v1) and the storage report | tool configuration and output | SharedCache.md "Storage policy check" |
| `dcc_matrix.json` (`zenith2.dcc-matrix` v1) | test data agreeing with DccMatrix.md | DccMatrix.md "Per-DCC fixtures" |
| the `contentImports` cook-report member | cook report JSON | Importers.md "Import report member" |
| intermediates CONTENT writes: `mesh` (GEOM's), `geom.textureSource` (GEOM's), `geom.materialDocument` (GEOM's), `anim.skeleton` and `anim.clip` (ANIM's, through its helpers), `decoded-audio` (AUDIO's), `scene.prefabDocument` (a canonical prefab document), `physics.collisionSource` (PHYS's) | DDC intermediates | Importers.md "glTF importer" and each product section |

### Common format rules

- JSON written by CONTENT uses `Zenith_JsonWriter` with LF line endings and one trailing LF; authoring documents and journals use the canonical form (`Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Canonical serialization"`); every JSON input is parsed with `Zenith_JsonDocument` under `Zenith_JsonLimits` with duplicate keys refused.
- Digests are CORE's SHA-256 written as 64 lowercase hex digits (`Zenith_FormatSha256Hex`); GUIDs are the 36-character persistent-ID text; times in tool outputs are UTC `YYYYMMDDTHHMMSSZ`, and no time, host name or user name reaches a cooked output or an intermediate.
- Binary data CONTENT itself defines is little-endian and written field by field (Z2-D-038); CONTENT defines no cooked runtime format.
- No CONTENT code whose result reaches an intermediate, a cooked output, a shared-store entry or a tracked file calls a CRT transcendental (`sin`, `cos`, `tan`, `atan`, `atan2`, `exp`, `log`, `log10`, `pow`, or their `f` forms), directly or inside a single-file library: it calls CORE's deterministic transcendental functions (X314; ruling X314-FMA3), so one input gives the same bytes on every machine, which distributed cooking and the shared store rely on. `sqrt` and the basic operations are correctly rounded by IEEE 754 and allowed. The sites are the colour downscale (Importers.md "Colour downscale"), stb_vorbis's tables (Importers.md "Zenith_MediaDecoders header") and the front-marker yaw (`Zenith_Atan2`, DccMatrix.md "Bounds and facing report").

### In-memory layouts that matter

| Datum | Size | Rule |
|---|---|---|
| `Zenith_ContentError` | 8 bytes | `static_assert` |
| `Zenith_GltfSourceScene` | arena-resident; bounded by Importers.md "Parser limits" | one per import, in the importer's scratch arena; never on a stack |
| merge trees | three parsed documents plus the merged tree | bounded by 3 × 64 MiB of input (Merge.md "Merge determinism and limits") |
| batch plan | per target 96 bytes plus its paths; 8 bytes per (target, document) pair while chunking; one chunk's image at a time while running (at most 65,536 documents) | at most 1,048,576 targets per operation; a unit holds at most one document's bytes and one 64 MiB save sub-batch |

## Algorithms and state machines

### Algorithm index

| Algorithm | Specified in |
|---|---|
| basis change for axis, handedness and units | DccMatrix.md "Basis change algorithm" |
| negative-determinant mirrored variants and winding | DccMatrix.md "Winding and the negative-determinant rule" |
| normal and tangent validation | DccMatrix.md "Normals and tangents on import" |
| bounds and facing measurement | DccMatrix.md "Bounds and facing report" |
| glTF import: parse, products, meshes, skins, morphs, animations, materials, ORM packing, cameras, lights, the generated prefab | Importers.md "glTF importer" and its sections |
| product keys and rename recognition | Importers.md "Product keys"; PresetsAndReimport.md "Rename recognition" |
| product overrides application | PresetsAndReimport.md "Applying product overrides" |
| preset matching | PresetsAndReimport.md "Preset matching" |
| batch planning, chunked execution, resume, abandon, rollback | BatchOperations.md "Planning", "Chunked execution", "Resume algorithm", "Abandon and rollback" |
| generic reference repair | BatchOperations.md "Generic reference repairer" |
| structural three-way merge, keyed arrays, sets, conflict markers, diff3 | Merge.md |
| shared store get, put, timeout, prune | SharedCache.md "Store operations", "Timeout watchdog", "Store administration" |
| partitioned distributed cook | SharedCache.md "Partition protocol" |
| licence list and storage check | SharedCache.md "Content licences step", "Storage policy check" |

### State machine index

| State machine | Specified in |
|---|---|
| batch operation states × events | BatchOperations.md "Batch state machine" |
| chunk states | BatchOperations.md "Chunked execution" |
| partition job states | SharedCache.md "Partition protocol" |

## Threading lifetime and error contract

### Thread affinity per type

| Type | Affinity |
|---|---|
| `Zenith_ParseGltfSource`, `Zenith_DecodeImageSource`, `Zenith_DecodeAudioSource`, `Zenith_MergeDocuments3`, `Zenith_MergeText3`, the validators | `ANY_THREAD`; pure functions of their inputs over caller-owned arenas |
| the four importers | `ANY_THREAD` as ASSET's `Zenith_Importer`; one import per call, single-threaded inside the call, in a cook worker process in every configuration except `--in-process` |
| `Zenith_ImportPresetSet` | `OWNER_THREAD` while built; `IMMUTABLE` once built |
| `Zenith_ContentBatchOperation` | `OWNER_THREAD`: the one thread that called `Plan` or `Open`, never a job worker: EDIT's `EditorContentBatch` thread in the editor (`Zenith2/Docs/Design/EDIT.md → "Threads EDIT owns"`), the main thread in `ZenithContent` |
| `Zenith_ContentCookChannel` | called on the batch's owner thread only; no member blocks, and an operation has at most 32 requests outstanding |
| `Zenith_FileShareStoreAdmin` | `OWNER_THREAD`: `ZenithContent`'s main thread |
| `Zenith_FileShareDerivedDataStore` | `ANY_THREAD` as ASSET's store interface; its watchdog thread is its own |
| the `content.licences` step | `ANY_THREAD` per ASSET's step contract |
| the `ZenithContent` commands | the process's main thread, plus the helper's heartbeat thread and the store's watchdog thread |

### Frame-graph placement

None. CONTENT has no runtime code, registers no frame-graph node and reads no world: every CONTENT function runs in ZenithCook, its worker processes, ZenithEditor between frames, or ZenithContent.

### Lock-order entries

CONTENT adds one row to `Zenith2/Docs/Contracts.md → "Lock ordering table"` ([Z2-N-CONTRACT-318], [Z2-N-CONTRACT-320]; the core-doc batch lands it from cross-doc row X306):

| Level | Name | Owner | Guards | Short hold | Same level by address | Acquired on |
|---|---|---|---|---|---|---|
| 950 | `uLOCK_LEVEL_CONTENT_DDC_WATCHDOG` (declared in `Zenith2/Tools/Authoring/Internal/CONTENT/SharedDDC/Zenith_FileShareDerivedDataStore.h`) | ZenithAuthoring (CONTENT) | the file-share store's 256-slot table of in-flight file calls (thread handle, deadline, cancelled flag) | yes | no | any thread calling the store's `Get` or `Put`, and the store's watchdog thread |

It is a leaf: its holder calls `CancelSynchronousIo` (the watchdog) or reads and writes one slot (a caller) and nothing else; it never logs, allocates or performs file I/O while holding it, so it never needs a lock at a lower level such as the log sinks (920) (SharedCache.md "Timeout watchdog"). Until the row lands, the ticket introducing the mutex stays `draft` ([Z2-N-CONTRACT-320]). CONTENT's cross-process locks are files, taken in this order and never in another:

1. the operation lock `<storage root>/.migration/content-ops/active.lock` (BatchOperations.md "Chunked execution"), held for the whole run;
2. each storage root's writer lock (`Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Storage-root writer lock"`), taken per chunk in byte order of root path and released before any cook request, because the cook server takes the same locks for its sidecar writes.

### Threads

`ZenithContentCore` starts no thread. The file-share store starts one watchdog thread at `Create` and joins it in its destructor. `ZenithContent helper` starts one heartbeat thread per claimed partition and joins it before writing the partition's `done` file. Every child process (`git`, `ZenithCook`, the FBX converter) is waited for by its starter, with a timeout, and terminated on timeout.

### Cancellation and shutdown

- A batch operation checks its progress callback's cancel flag after every unit of work and acts on it at the next chunk boundary; a cancel returns `CONTENT_ERROR_BATCH_CANCELLED` with the journal in state PAUSED and every completed chunk kept; nothing in a chunk is cancelled half-way, so the tree is consistent at every return. A `Step` call never blocks: a cook request in flight is polled by the next call; `Run` sleeps 1 ms between polls; planning is units like execution, so a cancel during planning returns at the end of the current unit with nothing written.
- An import is cancelled only by its worker process being terminated (ASSET's pool); it holds nothing that outlives the process.
- `ZenithContent` handles Ctrl+C by setting the same cancel flag; a second Ctrl+C terminates the process, after which `batch resume` continues.

### Error enumerations

| Enumeration | Declared in | Specified in |
|---|---|---|
| `Zenith_ContentErrorCode`, record `Zenith_ContentError` | `Zenith2/Tools/ContentCore/Zenith_ContentTypes.h` | "Errors" |
| `Zenith_ContentImportDetail` | the same | Importers.md "Import detail codes" |
| importer results | ASSET's `Zenith_ImportError` | `Zenith2/Docs/Design/ASSET/Import.md → "Import errors"` |
| the `content.licences` step's results | ASSET's `Zenith_CookError` | `Zenith2/Docs/Design/ASSET/Cook.md → "Cook errors"` |

### Luau errors versus asserts

CONTENT raises no Luau error and has no Luau surface. Asserts guard programmer errors only (a batch call from a thread other than its owner, a second `Run` on a running operation); file, JSON, journal, git and cache bytes never reach an assert ([Z2-N-CONTRACT-472]).

## Diagnostics and profiling

### Log categories

| Category | Name | Compiled minimum |
|---|---|---|
| `xLOG_CATEGORY_CONTENT_IMPORT` | `content.import` | `LOG_LEVEL_INFO` |
| `xLOG_CATEGORY_CONTENT_BATCH` | `content.batch` | `LOG_LEVEL_INFO` |
| `xLOG_CATEGORY_CONTENT_MERGE` | `content.merge` | `LOG_LEVEL_INFO` |
| `xLOG_CATEGORY_CONTENT_DDC` | `content.ddc` | `LOG_LEVEL_WARNING` |
| `xLOG_CATEGORY_CONTENT_DISTRIBUTE` | `content.distribute` | `LOG_LEVEL_INFO` |
| `xLOG_CATEGORY_CONTENT_STORAGE` | `content.storage` | `LOG_LEVEL_INFO` |

Declared with `ZENITH_LOG_CATEGORY` (`Zenith2/Docs/Design/CORE/Diagnostics.md → "Levels and categories"`); repeated warnings of one kind log once per source per run and are counted thereafter.

### Cook report members

| Member | Writer | Content |
|---|---|---|
| `contentImports` | each CONTENT importer through `AddReportMember` (pending ASSET, row CT2) | per source: GUID, importer, products, bounds in metres, counts, warnings (Importers.md "Import report member") |
| `contentLicences` | the `content.licences` step | the count of licensed assets per licence identifier |

### Counters

CONTENT publishes no runtime counter and no on-screen profiler row: it has no runtime code. Its measurable quantities are the report members above, the batch report's counts (BatchOperations.md "Batch report"), the store statistics (`ZenithContent ddc stats`, SharedCache.md "Store administration") and the coordinator's summary (SharedCache.md "Partition protocol"); DEVT's dashboards read them from those files.

### Debug views and dumps

`ZenithContent batch status --json` prints a journal; `ZenithContent ddc stats --json` the store statistics; `ZenithContent merge --explain` prints the per-member merge decisions of a merge (Merge.md "Driver command line").

### Telemetry

None at runtime. The cook report members and the batch report are the telemetry of record.

## Authoring and cooking

### What CONTENT authors

| Document | Type | Edited with |
|---|---|---|
| import preset `*.importpreset.json` | `Zenith.Content.ImportPresetDocument`, an authoring document joining [Z2-N-CONTRACT-552]'s list (row CT15) | EDIT's generic document editor, validated by `Zenith_ValidateImportPresetJson` |
| CONTENT's sub-asset entry members and the `.meta` member `provenance` | parts of ASSET's `Zenith.AssetMeta` | EDIT's asset inspector through the cook server's `setSettings` (pending ASSET, rows CT1, CT22) |
| the batch specification | not an authoring document (tool input) | EDIT's asset browser builds it; `ZenithContent batch` reads it from a file |

### Editor surfaces CONTENT supplies

CONTENT writes no ImGui code. It supplies the models EDIT's panels display (row CT18):

| Surface (EDIT) | CONTENT model |
|---|---|
| import settings and the preset picker in the asset inspector | `Zenith_ImportPresetSet::GetPresetsFor(importer)`; the importer descs of `Zenith_RegisterContentImporters` |
| the per-product override editor (material remap, exclude) and the clip and rig document attachments ANIM defines | `Zenith_ValidateProductOverridesJson` and the sub-asset entries of the `record` request |
| the preset proposals of new `.meta` files | the `presets [{source, preset, glob}]` list of `assignGuids` (row CT24) |
| batch progress and the batch report | the `Zenith_ContentBatchProgress` the progress callback receives after every unit on the batch thread, which EDIT hands to the main thread; the report JSON |
| "Open merge tool" | `ZenithContent merge` run on the three index stages, and its `--explain` output |
| the import report (bounds, warnings) | the `contentImports` report member |

### Cook steps

| Step | Phase | Kind | Version | Specified in |
|---|---|---|---|---|
| `content.licences` | `COOK_PHASE_GLOBAL_POST`, runs after every ASSET-phase item and before `asset.catalogue` | `ASSET_KIND_NONE`; its one owned output prefix is the file name `provenance-licences.json` at the tree root | 1 | SharedCache.md "Content licences step" |

### Importers

| ID | Extensions | Version | Settings version | Out of process | Specified in |
|---|---|---|---|---|---|
| `gltf` | `.gltf`, `.glb` | 1 (+ `uANIM_IMPORT_VERSION`, + `uCONTENT_GLTF_IMPORT_VERSION`) | 1 | yes | Importers.md "glTF importer" |
| `image` | `.png`, `.jpg`, `.jpeg`, `.tga`, `.bmp` | 1 | 1 | yes | Importers.md "Image importer" |
| `audio` | `.wav`, `.flac`, `.ogg`, `.mp3`, `.aif`, `.aiff` | 1 | 1 | yes | Importers.md "Audio importer" |
| `fbx` | `.fbx` | 1 | 1 | yes | Importers.md "FBX importer" |

### Migration hooks

| Document type | Chain | Fixtures |
|---|---|---|
| `Zenith.Content.ImportPresetDocument` | version 1, no step; registered by `Zenith_RegisterContentDocumentTypes` | `Zenith2/Tools/Authoring/Tests/Fixtures/Migration/CONTENT/ImportPresetDocument/v1.json`, `v1.expected.json`, under ZenithAuthoring's one fixture root (rule 2 of `Zenith2/Docs/Design/EDIT.md → "ZenithAuthoring contract rules"`, row ED48; [Z2-N-CONTRACT-548]) |
| `Zenith.ImporterSettings.gltf`, `.image`, `.audio`, `.fbx` (the settings objects inside `.meta`) | version 1, no step; each importer's `m_apfnSettingsSteps` | `Zenith2/Tools/ZenithCook/Tests/Fixtures/Migration/ImporterSettings/<id>/v1.json`, `v1.expected.json` (`Zenith2/Docs/Design/ASSET/Migration.md → "Meta migration fixtures"`) |
| CONTENT's sub-asset entry members (`exclude`, `materialRemap`) | no version of their own: they are part of the `Zenith.AssetMeta` schema, and a change to them is a `.meta` version step ASSET registers at CONTENT's request (row CT22) | `Zenith2/Tools/Authoring/Tests/Fixtures/Migration/Asset/AssetMeta/` (ASSET's fixture root since the `.meta` registration moved to ZenithAuthoring) |
| `provenance` member | schema version 1, carried in the member (`"schema": 1`); its chain is the rows for `provenance` in ASSET's constexpr table `Zenith_GetAssetMetaMemberMigrations()`, run inside the `.meta` type's visitor (row CT24); version 1 has no step and so no row, and a later schema is a CONTENT diff appending its `{member, fromVersion, step}` row with ASSET reviewing | ASSET's `Zenith2/Tools/Authoring/Tests/Fixtures/Migration/Asset/AssetMeta/provenance/v1.json`, `v1.expected.json` |

## Failure handling

### Failure table

| Failure | Behaviour | Evidence |
|---|---|---|
| a malformed or over-limit source | the import fails `IMPORT_ERROR_SOURCE_INVALID` or `IMPORT_ERROR_LIMIT_EXCEEDED` with the detail and the source path; no intermediate is stored; other sources import | `Test_GltfImporter_MalformedSourcesFailCleanly`, the fuzz targets |
| a required glTF extension outside the matrix | `IMPORT_ERROR_SOURCE_INVALID`, detail `CONTENT_IMPORT_DETAIL_REQUIRED_EXTENSION`, naming the extension | `Test_GltfImporter_RequiredUnsupportedExtensionFails` |
| an importer crash or hang | ASSET's pool fails the item `IMPORT_ERROR_WORKER_CRASHED` or `_WORKER_TIMEOUT`; the cook continues; no partial intermediate exists | `Test_ContentCrash_WorkerCrashContained`, `Test_ContentCrash_WorkerHangTimesOut` |
| an image without a declared usage | the image import fails `IMPORT_ERROR_SETTING_INVALID`, detail `CONTENT_IMPORT_DETAIL_USAGE_UNSET`, naming the file and the command that sets a preset | `Test_ImageImporter_UnsetUsageFailsLoudly` |
| a preset of another importer, or an invalid preset | `IMPORT_ERROR_SETTING_INVALID` (ASSET's resolution, row CT3); the editor's preset validator reports the rule | `Test_ImportPreset_ValidationRules` |
| a material remap on an orphaned entry, or naming a material that is no longer live | kept in the `.meta`, not applied, reported `CONTENT_WARNING_OVERRIDE_DANGLING`; an orphaned entry carrying `exclude: true` is excluded, never dangling | `Test_ProductOverrides_DanglingKeptAndReported`, `Test_ProductOverrides_ExcludeStableAcrossThreeReimports` |
| a renamed object whose fingerprint changed, or a deleted object and an unrelated new one with a different fingerprint | no rename: the old key orphaned, the new one a new GUID, `CONTENT_WARNING_RENAME_NO_EVIDENCE` naming both; a GUID is re-bound only when the fingerprints are equal, and then always warned `CONTENT_WARNING_RENAME_ASSUMED` with both keys, so two different objects with equal fingerprints are re-bound visibly and a remap batch undoes it | `Test_RenameRecognition_DeletedAndAddedNeverRebound`, `Test_RenameRecognition_OneToOneRenames` |
| a batch interrupted at any step | `Resume` converges on the tree an uninterrupted run gives; every chunk is fully old or fully new | `Test_ContentBatch_InterruptedAtEachStepResumes` |
| files changed under a paused batch | `CONTENT_ERROR_BATCH_CONFLICT` naming the chunk and paths; nothing written | `Test_ContentBatch_ConflictOnExternalEdit` |
| low disk during a batch | the batch's own free-space check before a chunk's PREPARED step, or the framework's refusal (`DOCUMENT_ERROR_LOW_DISK`), gives `CONTENT_ERROR_LOW_DISK`; the operation pauses at the chunk boundary | `Test_ContentBatch_LowDiskPauses` |
| an interruption after the cook server applied a `.meta` request and before the journal recorded it | `Resume` classifies the request from the server's record: applied, never re-issued | `Test_ContentBatch_RemapResumeAfterServerWrite` |
| a `.meta` or a document a completed batch changed, edited again before `Rollback` | the element or document classifies OTHER: conflict, the operation stays ROLLING_BACK, nothing is sent or written for it, the newer edit is kept and listed | `Test_ContentBatch_RollbackRefusesLaterMetaEdit` |
| a before-copy or a staged after-copy damaged on disk | `CONTENT_ERROR_BATCH_JOURNAL_CORRUPT` for that chunk before anything of it is written | `Test_ContentBatch_DamagedBeforeCopyRefused` |
| a plan whose document repairs exceed 1,000,000, or a target referenced by more than 65,536 documents | `CONTENT_ERROR_SPEC_INVALID` (rule B11) during planning, naming the chunk size that would fit; nothing written | `Test_ContentBatch_ChunkPackingGroupsSharedDocuments` |
| a storage root locked by the editor or a cook server | `CONTENT_ERROR_SOURCE_LOCKED` after `content.batch.lockWaitMs`; the operation pauses | `Test_ContentBatch_SourceLockedPauses` |
| the cook server unreachable during a batch | `CONTENT_ERROR_COOK_CHANNEL`; the chunk's document writes stand, its cook step is pending; `Resume` re-issues it | `Test_ContentBatch_CookChannelFailureResumes` |
| a shared store unreachable, timing out, corrupt or stale | ASSET's client falls back to a local cook; the store is never written partially or deleted from by a client | `Test_SharedDDC_UnreachableFallsBackLocal`, `_TimeoutCancelsAndFallsBack`, `_CorruptEntryLeftAndReported`, `_StaleEntryIsMiss` |
| a merge input that does not parse or exceeds the JSON node limits | the driver falls back to the text merge; markers on every conflicting hunk; exit 1 when any conflict remains | `Test_ContentMerge_UnparsableFallsBackToText` |
| a merge input over 64 MiB or not valid UTF-8 | one whole-file conflict block with ours and theirs, exit 1; `<A>` never holds ours alone | `Test_ContentMerge3_OversizeAndInvalidUtf8WholeFileBlock`, `Test_ContentMergeCommand_ExitCodesAndAtomicWrite` |
| a helper host lost during a distributed cook | its partition's heartbeat goes stale; the coordinator stops waiting; its own cook produces the missing outputs | `Test_ContentDistribute_LostHelperDoesNotBlock` |
| a helper whose ZenithCook cannot be built from the job's commit, or a done file from another commit's tool | `toolMismatch`: nothing is cooked into the store by that helper; the coordinator counts the partition as not `ok` | `Test_ContentDistribute_ToolMismatchRefused` |
| a binary file at or above the LFS threshold outside LFS, or a file whose attributes say LFS committed as a plain blob | the storage check reports `STORAGE_NOT_LFS`, exit 1 | `Test_ContentStorage_DetectsUntrackedBinary`, `Test_ContentStorage_DetectsLfsAttributeWithoutPointer` |
| a recorded fixture read on a host without git-lfs | the reading test fails at once naming the path and the setup step, never decoding pointer text | `Test_MediaDecoders_RecordedFixturePointerFailsClearly` |
| allocation failure | every call returns `CONTENT_ERROR_OUT_OF_MEMORY` or `IMPORT_ERROR_OUT_OF_MEMORY` and leaves its outputs unchanged | the fault sweeps of Tests.md |

### Missing and corrupt input

Every source, preset, journal, specification, cache entry and job file is untrusted ([Z2-N-ARCH-951]). A missing referenced file (a glTF's external buffer) fails the import with `IMPORT_ERROR_INPUT_NOT_FOUND`; a corrupt operation journal stops `Resume` with `CONTENT_ERROR_BATCH_JOURNAL_CORRUPT`, writes nothing, and names the journal; the person resolves it by `batch abandon --force-journal` (BatchOperations.md "Abandon and rollback").

### Allocation failure

Every allocation goes through `Zenith_Heap`, the single-file libraries by three routes (Importers.md "Zenith_MediaDecoders header"): stb_image's macros over the root heap's `TryAllocate`, never its OOM-handler path; dr_wav, dr_flac and dr_mp3 through their allocation-callback arguments over `Zenith_AudioDecodeOptions::m_pxHeap`; stb_vorbis through an alloc buffer from that heap. A failed allocation returns the out-of-memory error of the layer and releases what the call built. The OOM fault sweeps of Tests.md reach every route except stb_image's, which is exempt by name because its macros can reach only the process's root heap (Importers.md "Zenith_MediaDecoders header").

### Device loss

Not applicable: CONTENT holds no device resource.

### Budget breach

| Row | Breach | CONTENT behaviour |
|---|---|---|
| Z2-B-CONTENT-001 | BR-ACCEPT | the storage check fails (exit 1) and the stage acceptance blocks |
| Z2-B-CONTENT-002 … -009 | BR-ACCEPT | the storage check reports `STORAGE_QUOTA_EXCEEDED` for the game |

## Test strategy

### Test targets and gate steps

| Target | Runs in | Contents |
|---|---|---|
| `ZenithContentCore_Tests` | `Test` step on `win-debug`; `Sanitizers -Preset win-asan` | conversion, glTF parsing, decoders, merge, validators, journal codec |
| `Zenith_Fuzz_GltfSource`, `_ImageSource`, `_AudioSource`, `_MergeInput`, `_BatchJournal`, `_ContentValidation` | `win-asan` 60-second smoke; full duration in stage acceptance ([Z2-N-ARCH-952]) | the parsers |
| `ZenithCook_Tests` (CONTENT's cases) | `Test` step on `win-debug` | importers in `Zenith_MakeImportTestContext` contexts and steps in `Zenith_MakeCookTestContext` contexts, conventions, crash isolation, the licences step |
| `ZenithAuthoring_Tests` (CONTENT's cases) | `Test` step on `win-debug` | presets, overrides, batch engine, repair, the store, merge policies |
| `ZenithContent_Tests` | `Test` step on `win-debug` | the commands, the git driver over a temporary repository, the storage check, distribution |
| `Zenith_Integration_ContentPipeline` | `Test` step on `win-debug` | cook-then-load, reimport with overrides, batch resume through a killed child process, shared-store failure modes, distributed equivalence |
| `Zenith_Bench_FbxFidelity` | `--smoke` in the `Test` step; full by the D-SEL-FBX ruling ticket | the converter comparison |

### Test catalogue index

Every named test with the case it asserts is in `Zenith2/Docs/Design/CONTENT/Tests.md`.

### Fixtures

No binary fixture is committed by a worker (the generated-fixture rule of `Zenith2/Docs/Architecture/BuildSystem.md` [Z2-N-ARCH-257]): glTF, PNG, WAV and FLAC fixtures are written in code by the header-only builders at test time; the fixture game's sources are written by the fixture-setup case `Fixture_ContentGame_Generate` into `${ZENITH_OUT}/test-fixtures/CONTENT/` ([Z2-N-ARCH-257], entry `CONTENT_GameFixtures`, fixture `ContentGameFixtures`, variable `ZENITH2_CONTENT_FIXTURES`, consuming suite `z2-content-cooked`); the per-DCC exports and the Ogg Vorbis, MP3 and JPEG fixtures are committed by needs-human recording tickets (Tests.md "Needs-human recordings"), each blocked by the Stage 3 LFS-attributes ticket (Z2-R-CONTENT-151).

### Acceptance evidence

| Evidence (`Zenith2/Docs/Roadmap.md → "Acceptance evidence"`) | CONTENT's artefact | First required at |
|---|---|---|
| reimport (preserving authored overrides), CONTENT's Stage 3 share | stable product keys and GUID survival across reimport: `Test_GltfImporter_ProductKeysAndPrimary` and `Test_GltfImporter_SameInputsSameIntermediates`, with ASSET's `Test_Import_ReimportKeepsSubAssetGuids` | Stage 3 |
| reimport (preserving authored overrides), the override layer | `Test_ContentPipeline_ReimportPreservesOverrides`: sub-asset entry members, fingerprinted renames, node-key entity identity, ANIM clip documents | Stage 4 |
| missing or corrupt content | `Test_ContentPipeline_CorruptSourcesFailCleanly` | Stage 3 |
| the DCC matrix | `Test_DccMatrix_EverySupportedRouteRecorded` (suite `z2-acceptance-content`), with `Test_DccMatrix_EveryRowHasPassingFixture` over the recorded exports | Stage 4 |
| shared DDC failure modes | `Test_ContentPipeline_SharedDDCFailureModes`, and the `needs-human` step "share unplugged mid-cook" (Tests.md "Needs-human recordings") | Stage 4 |
| batch recovery | `Test_ContentPipeline_BatchKilledAndResumed` | Stage 4 |
| semantic merge | `Test_ContentPipeline_GitMergeDriverEndToEnd` | Stage 4 |
| distributed cooking | `Test_ContentPipeline_DistributedCookEqualsLocal` | Stage 4 |
| security hardening | every CONTENT fuzz target run for its full duration without a finding ([Z2-N-ARCH-960]) | Stage 6 |

## Budget rows

### Rows CONTENT owns

| ID | Name | Status | Use in CONTENT |
|---|---|---|---|
| Z2-B-CONTENT-001 | LFS tracking threshold | pending (Stage 1 per-system budget ticket for CONTENT) | `lfsThresholdBytes` of `storage.config.json`; the storage check's binary rule |
| Z2-B-CONTENT-002 | Source-asset quota OpenWorldSlice | pending (same ticket) | `quotasGB.OpenWorldSlice` |
| Z2-B-CONTENT-003 | Source-asset quota CinematicSlice | pending (same ticket) | `quotasGB.CinematicSlice` |
| Z2-B-CONTENT-004 | Source-asset quota Game2D | pending (same ticket) | `quotasGB.Game2D` |
| Z2-B-CONTENT-005 | Source-asset quota WorkloadIndoor | pending (same ticket) | `quotasGB.WorkloadIndoor` |
| Z2-B-CONTENT-006 | Source-asset quota WorkloadOutdoor | pending (same ticket) | `quotasGB.WorkloadOutdoor` |
| Z2-B-CONTENT-007 | Source-asset quota WorkloadCharacters | pending (same ticket) | `quotasGB.WorkloadCharacters` |
| Z2-B-CONTENT-008 | Source-asset quota WorkloadDense | pending (same ticket) | `quotasGB.WorkloadDense` |
| Z2-B-CONTENT-009 | Source-asset quota WorkloadUI2D | pending (same ticket) | `quotasGB.WorkloadUI2D` |

### Rows CONTENT is measured against

| Row | How CONTENT contributes |
|---|---|
| Z2-B-EDIT-004 Asset reimport | the importer's share of the reimport time (parse, convert, intermediates) |
| Z2-B-EDIT-010 Walkthrough reimport an asset | the import settings and preset flow |
| Z2-B-ASSET-010 Local DDC size | shared hits copied into the local store |
| Z2-B-ASSET-011 Incremental no-op cook time per slice | no CONTENT work when nothing changed: the importers are keyed |
| Z2-B-PROC-001 Lane checkout storage per Z2 lane | the LFS policy keeps binary content out of the duplicated working tree |

### Pending new rows

Recorded as pending, producer the Stage 1 per-system budget ticket for CONTENT; the Budgets.md addition is cross-doc row X of CT15:

| Proposed row | Unit | Procedure | Why |
|---|---|---|---|
| Batch reference repair time per 1,000 documents | s | MP-EDIT: a move of 1,000 assets referenced from 1,000 scene documents of the W-Editor workload, each scene referencing 10 of the assets spread across the folder, from `Plan` to COMPLETED (planning, chunking and repetition included), reporting W beside the time | the editor's folder-move experience; the chunk size is tuned against it (cross-doc row X286 as amended by X354) |
| Semantic merge time of a 16 MiB scene | ms | MP-EDIT: `ZenithContent merge` over the W-Editor workload's largest scene with 1% of entities changed on each side | git runs the driver synchronously during a merge |
| Shared DDC hit ratio on a warm incremental cook | percent | the cook report's `ddcSharedHits / items` on a second host after one host cooked the slice | the value of the shared store; a reported row (BR-REPORT) |

### Configuration and cook keys

| Key | Kind | Default | Meaning |
|---|---|---|---|
| `cook.ddc.sharedRoot` | cook key (CONTENT's, read by the shared-store factory) | empty | the share's root path (`\\server\share\ZenithDDC` or a local directory); empty with a shared mode other than `off` is `COOK_ERROR_BAD_COMMAND_LINE` |
| `cook.content.faultSource` | cook key (CONTENT's importers) | empty | `crash:<Zenith2-relative source path>` or `hang:<path>`: the fault hook of Importers.md "Crash isolation" |
| `cook.content.fbxConverterTimeoutSeconds` | cook key (CONTENT's FBX importer) | 300 | the converter child's wall-clock limit |
| `content.batch.chunkTargets` | `ZenithContent` and editor option | 64 | targets per chunk for small components; a component of shared references is packed up to 16 × this (BatchOperations.md "Planning" step 4) |
| `content.batch.lockWaitMs` | the same | 10,000 | the writer-lock wait before `CONTENT_ERROR_SOURCE_LOCKED` |
| `content.batch.maxDocumentsPerSave` | the same | 256 | documents per `SaveDocuments` call |

## Stage plan

### Stage 1

| Slice | Content | Governing |
|---|---|---|
| the per-system budget ticket for CONTENT (`needs-human` ruling) | the values of Z2-B-CONTENT-001 … -009 and of the pending rows, written to `Zenith2/Tools/ZenithContent/Internal/Config/storage.config.json`, `Budgets.md` and the DecisionLog entry in one diff | Z2-D-012, Z2-D-084 |

### Stage 3

Stage 3's acceptance measures the focused workloads (W-Characters and W-Outdoor import real glTF characters, props and textures) and requires reimport and corrupt-content evidence (`Zenith2/Docs/Roadmap.md → "Content"`), so the importers' core is Stage 3 work by the stage it serves (requirement stage = the stage whose acceptance it serves). The inventory items it serves, Z2-I-CONTENT-01, -02, -04, -06, -07, -15 and -22, are moved to S3 by the inventory change of cross-doc row X334 (the X305 option (a), made like Z2-D-131's stage corrections), so no CONTENT requirement sits earlier than an item it covers; each moved item keeps its Stage 4 parts in Stage 4 requirements.

| Slice | Content | Blocked by |
|---|---|---|
| LFS attributes block | the `# BEGIN zenith-content lfs` block of `Zenith2/.gitattributes`: one `*<ext> filter=lfs diff=lfs merge=lfs -text` line per v1 `binaryExtensions` suffix, a hand-written golden (SharedCache.md "Storage policy check", Z2-R-CONTENT-151); it `blocks` every needs-human recording ticket, the compressed-media one included | nothing |
| ContentCore parsers | `ZenithContentCore` with the glTF core subset (meshes, skins, animations, PBR materials, embedded and external images), `EXT_meshopt_compression` and `KHR_mesh_quantization`, the image and audio decoders, their parser limits and fuzz targets, `Zenith_LoadRecordedFixture` and its pointer refusal | the declaration slice of "API-only slices"; SHIP's manifest entries (row CT16); for the media-decoder ticket's recorded-fixture cases, the compressed-media recording ticket (Tests.md "Needs-human recordings"), itself blocked by the LFS attributes block, and git-lfs installed and pulled on every gate host by SHIP's setup script (row CT32), a Stage 3 prerequisite |
| the `gltf` importer core | out of process, stable product keys, the identity-conversion path, winding, negative-determinant variants with the decomposition rule, normal and tangent validation, the bounds report, the `contentImports` member | ASSET's previous products, fingerprints and report member (rows CT2, CT22); GEOM's mesh source (row CT9); ANIM's import helpers (row AN14, answered by CT20) |
| the `image` and `audio` importers | the decoders behind ASSET's interface, declared usage, AUDIO's `ZAPC` blob | GEOM's texture source writer (Textures.md); AUDIO's layout (row AU10) |
| Stage 3 evidence | the corrupt-source run and the Stage 3 share of the reimport row of "Acceptance evidence" | the slices above; ASSET's `Test_Import_ReimportKeepsSubAssetGuids` |

### Stage 4

Everything else: the full DCC matrix with per-DCC recorded fixtures, every extension row, morph targets, cameras, lights, the generated model prefab and node keys, axis, handedness and unit conversion settings with the probe and tangent-frame tests, presets and preset matching, product overrides and fingerprinted rename recognition, batch operations with repair, crash-isolation tests through the fault-source key, the shared store and its failure-mode acceptance, the merge driver and its git integration, distributed cooking, provenance and the licences step, the storage check, the FBX route per D-SEL-FBX (open, Stage 4) and its harness.

### Stage 5 to Stage 7

Stage 5: `KHR_materials_clearcoat` and `KHR_materials_sheen` map to GEOM's Stage 5 surface models (Importers.md "Materials"). Stage 6: the fuzz targets' full-duration runs in the security evidence ([Z2-N-ARCH-960]). Stage 7 adds nothing.

### API-only slices

Declarations land before their consumers: `ContentCore/Zenith_ContentTypes.h`, `Zenith_GltfSource.h` and `Zenith_MediaDecoders.h` land as declarations with contract blocks before the importers; `Zenith_AuthoringContent.h`'s `Zenith_ContentCookChannel` lands before EDIT's implementation and before the batch engine.

## Decisions

### Settled decisions governing CONTENT

| ID | Subject in CONTENT |
|---|---|
| Z2-D-038 | JSON authoring documents (presets, overrides, provenance), versioned binary only where another area owns it |
| Z2-D-048 | the preset type migrates with preview, backup, journaled write, unknown-field preservation; cooked data re-cooks |
| Z2-D-079 | materials stay code plus a document: the importer generates a document for an engine material type, never a node graph |
| Z2-D-082 | the decoders and fastgltf are pinned and confined to `Internal/` |
| Z2-D-084 | D-SEL-FBX is a harness ticket plus a ruling ticket |
| Z2-D-085, Z2-D-097 | this document is edited in the same diff as any CONTENT contract change; rulings are appended to DecisionLog.md in the ticket's diff |
| Z2-D-065 | the cooked-content hash, which the shared store and distributed cooking never change, is the evidence-fingerprint input |
| Z2-D-045 | `Zenith_Result` for every recoverable failure; asserts for programmer errors only |
| Z2-D-075 | no mod content |
| Z2-D-040, Z2-D-041 | ownership through `Zenith_Owned`; CORE containers only |

### Open decisions CONTENT depends on

| Decision | Stage | What waits on it |
|---|---|---|
| D-SEL-FBX | open, Stage 4 | the `fbx` importer's conversion branch, the Maya route of the DCC matrix, the FBX fidelity fixtures |
| D-SEL-HASH | open, Stage 1 | nothing CONTENT persists: every digest and every persisted key component CONTENT writes (product-key suffixes, node-key local IDs, fingerprints, journal digests) is SHA-256, never `Zenith_Hash64` |
| D-SEL-JSON | open, Stage 1 | every JSON CONTENT reads or writes, through CORE's backend-neutral document |

### Selection decisions CONTENT owns

CONTENT owns the harness and ruling tickets of D-SEL-FBX (open, Stage 4): the harness `Zenith_Bench_FbxFidelity` builds an out-of-process converter for each candidate and compares its output with the glTF reference exports under the thresholds of DccMatrix.md "Fidelity thresholds"; the ruling ticket runs it, reviews the licences and appends the ruling (DccMatrix.md "FBX route").

### Design rulings CONTENT records

The eight rulings CT-R1 … CT-R8 of "Design choices at a glance" are appended to `Zenith2/Docs/DecisionLog.md` together, in one human-run decision-log diff with consecutive free `Z2-D-NNN` IDs (Z2-D-097), before any implementation that relies on them; each implementing change cites its entry.

## Requirement index

| Requirement | Title | Stage | Design headings |
|---|---|---|---|
| Z2-R-CONTENT-001 | ZenithContentCore library target | S3 | CONTENT.md "Target summary", "ZenithContentCore internal files", "ZENITH_SUPPORTED matrix" |
| Z2-R-CONTENT-002 | CONTENT files inside ZenithCook | S3 | CONTENT.md "CONTENT's ZenithCook files"; Importers.md "Importer registration" |
| Z2-R-CONTENT-003 | CONTENT files inside ZenithAuthoring | S4 | CONTENT.md "CONTENT's ZenithAuthoring files", "Zenith_AuthoringContent header" |
| Z2-R-CONTENT-004 | ZenithContent tool target | S4 | CONTENT.md "ZenithContent files"; BatchOperations.md "Batch command line"; Merge.md "Driver command line" |
| Z2-R-CONTENT-005 | Tool-side API rules | S3 | CONTENT.md "Rules across the CONTENT API" |
| Z2-R-CONTENT-006 | Content error record | S3 | CONTENT.md "Errors" |
| Z2-R-CONTENT-007 | Import detail codes | S3 | Importers.md "Import detail codes" |
| Z2-R-CONTENT-008 | Supported-DCC matrix is normative and machine-checked | S4 | DccMatrix.md "DCC routes", "Feature support matrix", "Per-DCC fixtures" |
| Z2-R-CONTENT-009 | glTF core support | S3 | DccMatrix.md "glTF core support"; Importers.md "ZenithContentCore API" |
| Z2-R-CONTENT-010 | glTF extension support table | S4 | DccMatrix.md "glTF extension support" |
| Z2-R-CONTENT-011 | Unsupported glTF extensions | S3 | DccMatrix.md "glTF extension support"; Importers.md "glTF importer" |
| Z2-R-CONTENT-012 | Meshopt and quantized geometry | S3 | DccMatrix.md "glTF extension support"; Importers.md "ZenithContentCore API" |
| Z2-R-CONTENT-013 | Texture transforms | S4 | Importers.md "Textures referenced by materials" |
| Z2-R-CONTENT-014 | Basis Universal textures | S4 | Importers.md "Textures referenced by materials" |
| Z2-R-CONTENT-015 | FBX through the selected conversion path | S4 | DccMatrix.md "FBX route"; Importers.md "FBX importer" |
| Z2-R-CONTENT-016 | FBX refused when no path is selected | S4 | Importers.md "FBX importer" |
| Z2-R-CONTENT-017 | D-SEL-FBX fidelity harness | S4 | DccMatrix.md "FBX route", "Fidelity thresholds" |
| Z2-R-CONTENT-018 | Skin import through ANIM's helpers | S3 | Importers.md "Skins", "Meshes" |
| Z2-R-CONTENT-019 | Skin weight validation and normalisation | S3 | Importers.md "Skins" |
| Z2-R-CONTENT-020 | Morph target import | S4 | Importers.md "Morph targets" |
| Z2-R-CONTENT-021 | Animation import through ANIM's helpers | S3 | Importers.md "Animations" |
| Z2-R-CONTENT-022 | Clip settings declared as source-wide defaults | S4 | Importers.md "Animations"; PresetsAndReimport.md "What an authored override is" |
| Z2-R-CONTENT-023 | Generated standard material documents | S3 | Importers.md "Materials" |
| Z2-R-CONTENT-024 | Occlusion roughness metallic packing | S3 | Importers.md "Textures referenced by materials" |
| Z2-R-CONTENT-025 | Alpha mode and double-sided mapping | S3 | Importers.md "Materials" |
| Z2-R-CONTENT-026 | Unlit and emissive-strength extensions | S4 | Importers.md "Materials" |
| Z2-R-CONTENT-027 | Advanced material extensions | S5 | Importers.md "Materials" |
| Z2-R-CONTENT-028 | Embedded and external images | S3 | Importers.md "Textures referenced by materials" |
| Z2-R-CONTENT-029 | Camera import | S4 | Importers.md "Cameras"; DccMatrix.md "Basis change algorithm" |
| Z2-R-CONTENT-030 | Punctual light import | S4 | Importers.md "Lights" |
| Z2-R-CONTENT-031 | Generated model prefab | S4 | Importers.md "Generated model prefab" |
| Z2-R-CONTENT-032 | Stable generated entity IDs | S4 | Importers.md "Node keys", "Generated model prefab"; PresetsAndReimport.md "Reimport guarantees" |
| Z2-R-CONTENT-033 | Physics collision products | S4 | Importers.md "Physics collision products" |
| Z2-R-CONTENT-034 | Primitive modes and default material | S3 | Importers.md "Meshes" |
| Z2-R-CONTENT-035 | Mesh intermediate content | S3 | Importers.md "Meshes" |
| Z2-R-CONTENT-036 | Conversion settings | S4 | DccMatrix.md "Conversion settings" |
| Z2-R-CONTENT-037 | Axis conversion | S4 | DccMatrix.md "Basis change algorithm" |
| Z2-R-CONTENT-038 | Handedness conversion | S4 | DccMatrix.md "Basis change algorithm", "Winding and the negative-determinant rule" |
| Z2-R-CONTENT-039 | Unit conversion | S4 | DccMatrix.md "Unit conversion" |
| Z2-R-CONTENT-040 | Counter-clockwise winding preserved | S3 | DccMatrix.md "Winding and the negative-determinant rule" |
| Z2-R-CONTENT-041 | Mirrored instances | S3 | DccMatrix.md "Winding and the negative-determinant rule", "Matrix nodes and the negative-determinant decomposition" |
| Z2-R-CONTENT-042 | Normals and tangents validated | S3 | DccMatrix.md "Normals and tangents on import" |
| Z2-R-CONTENT-043 | Normal map green convention | S4 | DccMatrix.md "Conversion settings"; Importers.md "Image importer" |
| Z2-R-CONTENT-044 | Bounds and facing report | S3 | DccMatrix.md "Bounds and facing report"; Importers.md "Import report member" |
| Z2-R-CONTENT-045 | Preset size and facing expectations | S4 | DccMatrix.md "Bounds and facing report"; PresetsAndReimport.md "Preset document schema" |
| Z2-R-CONTENT-046 | Convention probe asset | S4 | DccMatrix.md "Convention probe asset" |
| Z2-R-CONTENT-047 | Per-DCC convention fixtures | S4 | DccMatrix.md "Per-DCC fixtures"; Tests.md "Needs-human recordings" |
| Z2-R-CONTENT-048 | Image importer | S3 | Importers.md "Image importer"; DccMatrix.md "Source format tables" |
| Z2-R-CONTENT-049 | Image usage is declared never inferred | S3 | Importers.md "Image importer" |
| Z2-R-CONTENT-050 | Audio importer decoding | S3 | Importers.md "Audio importer"; DccMatrix.md "Source format tables" |
| Z2-R-CONTENT-051 | Audio loop points and markers | S3 | Importers.md "Audio importer" |
| Z2-R-CONTENT-052 | Audio settings keys declared | S3 | Importers.md "Audio importer" |
| Z2-R-CONTENT-053 | Source format tables | S4 | DccMatrix.md "Source format tables" |
| Z2-R-CONTENT-054 | Importer determinism | S3 | Importers.md "glTF importer" |
| Z2-R-CONTENT-055 | Parser limits | S3 | Importers.md "Parser limits" |
| Z2-R-CONTENT-056 | Parser fuzz targets | S3 | Tests.md "Fuzz targets" |
| Z2-R-CONTENT-057 | Third-party parsers run out of process | S3 | Importers.md "Crash isolation" |
| Z2-R-CONTENT-058 | Fault-source cook key | S4 | Importers.md "Crash isolation" |
| Z2-R-CONTENT-059 | Crash containment tests | S4 | Importers.md "Crash isolation"; Tests.md "Importer tests" |
| Z2-R-CONTENT-060 | Hang containment tests | S4 | Importers.md "Crash isolation" |
| Z2-R-CONTENT-061 | Import report member | S3 | Importers.md "Import report member" |
| Z2-R-CONTENT-062 | Preset document type | S4 | PresetsAndReimport.md "Preset document schema", "Preset document type registration" |
| Z2-R-CONTENT-063 | Preset validation | S4 | PresetsAndReimport.md "Preset validation" |
| Z2-R-CONTENT-064 | Preset migration fixtures | S4 | PresetsAndReimport.md "Preset document type registration" |
| Z2-R-CONTENT-065 | Preset matching at meta creation | S4 | PresetsAndReimport.md "Preset matching" |
| Z2-R-CONTENT-066 | Shipped engine presets | S4 | PresetsAndReimport.md "Shipped presets" |
| Z2-R-CONTENT-067 | Preset change reimports its users | S4 | PresetsAndReimport.md "Preset matching" |
| Z2-R-CONTENT-068 | Stable product keys | S3 | Importers.md "Product keys" |
| Z2-R-CONTENT-069 | Rename recognition | S4 | PresetsAndReimport.md "Rename recognition", "Product fingerprints", "Recognition steps" |
| Z2-R-CONTENT-070 | Product overrides schema | S4 | PresetsAndReimport.md "Product overrides schema" |
| Z2-R-CONTENT-071 | Material remap override | S4 | PresetsAndReimport.md "Applying product overrides" |
| Z2-R-CONTENT-072 | Exclude override | S4 | PresetsAndReimport.md "Applying product overrides" |
| Z2-R-CONTENT-073 | No per-product settings layer | S4 | PresetsAndReimport.md "Product overrides schema", "Applying product overrides" |
| Z2-R-CONTENT-074 | Dangling overrides kept | S4 | PresetsAndReimport.md "Applying product overrides" |
| Z2-R-CONTENT-075 | Scene overrides survive reimport | S4 | PresetsAndReimport.md "Override-preserving reimport" |
| Z2-R-CONTENT-076 | Product members across meta migration | S4 | PresetsAndReimport.md "Overrides across settings migration" |
| Z2-R-CONTENT-077 | Batch specification | S4 | BatchOperations.md "Batch specification", "Operation kinds" |
| Z2-R-CONTENT-078 | Batch planning | S4 | BatchOperations.md "Planning" |
| Z2-R-CONTENT-079 | Chunked execution | S4 | BatchOperations.md "Chunked execution" |
| Z2-R-CONTENT-080 | Operation journal | S4 | BatchOperations.md "Operation journal" |
| Z2-R-CONTENT-081 | Resume after interruption | S4 | BatchOperations.md "Resume algorithm" |
| Z2-R-CONTENT-082 | Conflict detection on resume | S4 | BatchOperations.md "Resume algorithm", "Chunked execution" |
| Z2-R-CONTENT-083 | Abandon and rollback | S4 | BatchOperations.md "Abandon and rollback" |
| Z2-R-CONTENT-084 | Batch report | S4 | BatchOperations.md "Batch report" |
| Z2-R-CONTENT-085 | One batch per storage root | S4 | BatchOperations.md "Chunked execution" |
| Z2-R-CONTENT-086 | Batch cancellation | S4 | BatchOperations.md "Chunked execution" |
| Z2-R-CONTENT-087 | Batch state machine | S4 | BatchOperations.md "Batch state machine" |
| Z2-R-CONTENT-088 | Batch command line | S4 | BatchOperations.md "Batch command line" |
| Z2-R-CONTENT-089 | Cook channel interface | S4 | BatchOperations.md "Cook channel" |
| Z2-R-CONTENT-090 | Batch low disk and locks | S4 | BatchOperations.md "Chunked execution" |
| Z2-R-CONTENT-091 | Batch stress | S4 | BatchOperations.md "Stress case", "Fault-point consultation order"; Tests.md "Batch tests" |
| Z2-R-CONTENT-092 | Move keeps identity | S4 | BatchOperations.md "Move and rename" |
| Z2-R-CONTENT-093 | Move repairs path references | S4 | BatchOperations.md "Move and rename", "Generic reference repairer" |
| Z2-R-CONTENT-094 | Rename of a folder | S4 | BatchOperations.md "Move and rename" |
| Z2-R-CONTENT-095 | Delete with replacement | S4 | BatchOperations.md "Delete" |
| Z2-R-CONTENT-096 | Delete without replacement | S4 | BatchOperations.md "Delete" |
| Z2-R-CONTENT-097 | Deleted files recoverable and tombstoned | S4 | BatchOperations.md "Delete" |
| Z2-R-CONTENT-098 | Orphaned sub-asset repair | S4 | BatchOperations.md "Sub-asset remap and orphan repair" |
| Z2-R-CONTENT-099 | Generic reference repairer | S4 | BatchOperations.md "Generic reference repairer" |
| Z2-R-CONTENT-100 | Scene repair through SCENE's planner | S4 | BatchOperations.md "Generic reference repairer" |
| Z2-R-CONTENT-101 | Reimport and settings batches | S4 | BatchOperations.md "Reimport and settings batches" |
| Z2-R-CONTENT-102 | File-share store | S4 | SharedCache.md "File-share store", "Store layout", "Store operations" |
| Z2-R-CONTENT-103 | Store put is create-only | S4 | SharedCache.md "Store operations" |
| Z2-R-CONTENT-104 | Store get is never partial | S4 | SharedCache.md "Store operations" |
| Z2-R-CONTENT-105 | Store timeout watchdog | S4 | SharedCache.md "Timeout watchdog" |
| Z2-R-CONTENT-106 | Unreachable share falls back | S4 | SharedCache.md "Unreachable share" |
| Z2-R-CONTENT-107 | Corrupt shared entry falls back | S4 | SharedCache.md "Corrupt entries" |
| Z2-R-CONTENT-108 | Stale shared entry falls back | S4 | SharedCache.md "Stale entries" |
| Z2-R-CONTENT-109 | Store administration | S4 | SharedCache.md "Store administration" |
| Z2-R-CONTENT-110 | Store deployment | S4 | SharedCache.md "Deployment" |
| Z2-R-CONTENT-111 | Shared DDC failure-mode acceptance | S4 | SharedCache.md "Unreachable share", "Corrupt entries", "Stale entries"; Tests.md "Integration suite" |
| Z2-R-CONTENT-112 | Merge driver command | S4 | Merge.md "Driver command line", "Canonical inputs and outputs" |
| Z2-R-CONTENT-113 | Structural three-way merge | S4 | Merge.md "Structural merge algorithm" |
| Z2-R-CONTENT-114 | Keyed and set array merges | S4 | Merge.md "Keyed array merge", "Set merge" |
| Z2-R-CONTENT-115 | Merge consumes canonical serialization | S4 | Merge.md "Canonical inputs and outputs" |
| Z2-R-CONTENT-116 | Conflict marker format | S4 | Merge.md "Conflict markers" |
| Z2-R-CONTENT-117 | Nested-prefab override conflicts | S4 | Merge.md "Scene and prefab rules" |
| Z2-R-CONTENT-118 | Array-ordering conflicts | S4 | Merge.md "Keyed array merge", "Conflict markers" |
| Z2-R-CONTENT-119 | Scene semantic checks | S4 | Merge.md "Scene and prefab rules" |
| Z2-R-CONTENT-120 | Schema merge rules | S4 | Merge.md "Schema rules" |
| Z2-R-CONTENT-121 | Environment and world document merge rules | S4 | Merge.md "Environment document rules", "Merge policies" |
| Z2-R-CONTENT-122 | Text fallback merge | S4 | Merge.md "Text fallback" |
| Z2-R-CONTENT-123 | Git integration | S4 | Merge.md "Git integration" |
| Z2-R-CONTENT-124 | Merge determinism | S4 | Merge.md "Merge determinism and limits" |
| Z2-R-CONTENT-125 | Distributed partitioned cook | S4 | SharedCache.md "Distributed cooking", "Partition protocol" |
| Z2-R-CONTENT-126 | Helper checkout and tool verification | S4 | SharedCache.md "Partition protocol" |
| Z2-R-CONTENT-127 | Lost helpers never block | S4 | SharedCache.md "Partition protocol" |
| Z2-R-CONTENT-128 | Distributed output equivalence | S4 | SharedCache.md "Distributed cooking" |
| Z2-R-CONTENT-129 | Origin metadata schema | S4 | SharedCache.md "Content provenance" |
| Z2-R-CONTENT-130 | Licence table | S4 | SharedCache.md "Licence table" |
| Z2-R-CONTENT-131 | Content licences step | S4 | SharedCache.md "Content licences step", "Licence table" |
| Z2-R-CONTENT-132 | Storage policy check | S4 | SharedCache.md "Storage policy check" |
| Z2-R-CONTENT-133 | Storage budget rows | S1 | CONTENT.md "Rows CONTENT owns"; SharedCache.md "Storage policy check" |
| Z2-R-CONTENT-134 | LFS attributes | S4 | SharedCache.md "Storage policy check" |
| Z2-R-CONTENT-135 | Log categories | S3 | CONTENT.md "Log categories" |
| Z2-R-CONTENT-136 | Editor models | S4 | CONTENT.md "Editor surfaces CONTENT supplies" |
| Z2-R-CONTENT-137 | Design rulings recorded | S3 | CONTENT.md "Design rulings CONTENT records" |
| Z2-R-CONTENT-138 | Full-duration fuzz evidence | S6 | CONTENT.md "Acceptance evidence"; Tests.md "Fuzz targets" |
| Z2-R-CONTENT-139 | Stage 3 corrupt-source evidence | S3 | CONTENT.md "Acceptance evidence"; Tests.md "Integration suite" |
| Z2-R-CONTENT-140 | Glb container and buffers | S3 | Importers.md "ZenithContentCore API" |
| Z2-R-CONTENT-141 | Tangent frames under conversion | S4 | DccMatrix.md "Convention probe asset", "Basis change algorithm" |
| Z2-R-CONTENT-142 | Product fingerprints | S4 | PresetsAndReimport.md "Product fingerprints" |
| Z2-R-CONTENT-143 | Batch time-budgeted stepping | S4 | BatchOperations.md "Batch API", "Chunked execution" |
| Z2-R-CONTENT-144 | Recoverable meta changes | S4 | BatchOperations.md "Chunked execution", "Operation journal", "Resume algorithm" |
| Z2-R-CONTENT-145 | Every supported route recorded | S4 | DccMatrix.md "Per-DCC fixtures" |
| Z2-R-CONTENT-146 | Watchdog lock row | S4 | CONTENT.md "Lock-order entries"; SharedCache.md "Timeout watchdog" |
| Z2-R-CONTENT-147 | Declared tool-side APIs and clock seam | S4 | SharedCache.md "Store administration", "Partition protocol"; BatchOperations.md "Batch API" |
| Z2-R-CONTENT-148 | Escaped product key names | S3 | Importers.md "Product keys" |
| Z2-R-CONTENT-149 | Batch owner thread and stepped planning | S4 | BatchOperations.md "Batch API", "Planning"; CONTENT.md "Thread affinity per type" |
| Z2-R-CONTENT-150 | Bounded batch work | S4 | BatchOperations.md "Planning", "Operation journal", "Chunked execution" |
| Z2-R-CONTENT-151 | LFS attributes before recorded binaries | S3 | SharedCache.md "Storage policy check"; CONTENT.md "Stage 3"; Tests.md "Needs-human recordings" |
