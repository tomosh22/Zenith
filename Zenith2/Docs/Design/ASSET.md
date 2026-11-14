# ASSET design: the asset database, import, cook, derived-data cache and runtime loading

This is the design document of the ASSET area (`Z2 ASSET — Asset pipeline`). Workers implement the runtime library `ZenithAssets` (with the `.zpak` archive reader), the asset-pipeline half of the host tool `ZenithCook`, and the `.meta` document type on SCENE's authoring-document framework against it. It elaborates `Zenith2/Docs/Architecture.md → "Library dependency graph"`, `Zenith2/Docs/Contracts.md → "Handle kinds"` and `Zenith2/Docs/Contracts.md → "Compatibility policy per kind"`, and never contradicts them; where it needs something another area has not yet specified it says so in an `<!-- assumption: pending <AREA> -->` block, and every such need is a row of `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md`. The requirement catalogue is `Zenith2/Docs/Requirements/ASSET.md`; every obligation below traces to a `Z2-R-ASSET-NNN` listed in "Requirement index".

<!-- informative -->
The reference engine at `C:/dev/Zenith/Zenith` has no asset database, no stable identity and no derived-data cache. Its assets are addressed by raw file paths (`Zenith/AssetHandling/Zenith_AssetHandle.h` wraps a path string and a loaded pointer), its import runs inside the editor process with the importer's settings hard-coded per file type, its cooked files (`.zmesh`, `.ztex`, `.zanim`) carry no engine version and no codec record, and a stale export is found by deleting `.manifest` files by hand. Loads are synchronous on the calling thread. Zenith2 keeps none of that shape: identity is a persistent GUID in a tracked sidecar, import and cook are pure keyed functions whose outputs are cached, every cooked file carries a versioned header with its codec record, and every load is an asynchronous, cancellable, back-pressured operation.
<!-- /informative -->

## Purpose and scope

### What ASSET owns

| Capability | Where it lives |
|---|---|
| the runtime asset registry: shared reference-counted asset handles, slots, states, the asset-kind table, reflection value hooks and the reference-default resolver for handle fields | `Zenith2/Engine/ZenithAssets/` (`Zenith_AssetRegistry`, `Zenith_AssetHandle<T>`, `Zenith_AnyAssetHandle`) |
| the cooked-asset catalogue the Player loads (GUID to cooked file, logical path, dependencies, hashes) | `Zenith2/Engine/ZenithAssets/` (`Zenith_AssetCatalogue`) and the cook |
| the common cooked-asset file container: header, section table with COMPRESS's 24-byte section descriptors, checks, limits, reader and writer | `Zenith2/Engine/ZenithAssets/` (`Zenith_CookedFile.h`) |
| asynchronous loading: requests as operation handles, priorities, hard-dependency loading, the I/O → decompress → GPU upload → activation pipeline, cancellation at every step, completion reporting | `Zenith2/Engine/ZenithAssets/` (`Zenith_AssetLoader`) |
| the upward loader interface each asset kind's owner implements (Flux for textures, meshes and materials; ZenithScene for scenes and prefabs; modules for their kinds) | `Zenith2/Engine/ZenithAssets/Zenith_AssetKindLoader.h` |
| byte-range and section reads of cooked assets for streaming consumers | `Zenith2/Engine/ZenithAssets/` (`Zenith_AssetReader`) |
| decompression back-pressure: the bounded decode queue, I/O throttled by decode capacity, the staging budget, priority reserves, streamed decode | `Zenith2/Engine/ZenithAssets/Internal/Loading/` |
| residency budgets: resident-byte accounting, the keep-alive cache, the in-flight budget, the memory consumer of `MEMORY_SUBSYSTEM_ASSETS` and its shedding | `Zenith2/Engine/ZenithAssets/Internal/Residency/` |
| missing and corrupt assets: classification, the per-kind fallback mechanism with catalogue-listed engine fallbacks, retry after mount changes | `Zenith2/Engine/ZenithAssets/` |
| the built-in loaders of `ASSET_KIND_DATA` and `ASSET_KIND_COMPRESSION_DICTIONARY` | `Zenith2/Engine/ZenithAssets/Internal/BuiltIn/` (`Zenith_BuiltInAssets.h`) |
| the `.zpak` archive reader: the `Zenith_MountSource` of the package and patch mounts (PACK writes the archives) | `Zenith2/Engine/ZenithAssets/Internal/Archive/` (`Zenith_AssetArchive.h`) |
| the asset database: GUIDs, `.meta` importer-settings sidecars, sub-asset identity, scan and reconcile, the dependency graph, provenance records | `Zenith2/Tools/ZenithCook/Internal/Database/` |
| import and reimport: the importer interface and registry, settings resolution, intermediates, identity preservation | `Zenith2/Tools/ZenithCook/Internal/Import/` |
| the cook driver `Zenith_Cook`: incremental, dependency-driven, deterministic; cook steps, validation, the catalogue, the mount manifest, the configuration-fragment merge, the dependency file, the cook report, resume after interruption, the cook server | `Zenith2/Tools/ZenithCook/Internal/Cook/`, `Internal/Server/` |
| the derived-data cache client `Zenith_DerivedDataCache`, its key, the local store and the store interface a shared store implements | `Zenith2/Tools/ZenithCook/Internal/DDC/` |
| the `.meta` document type on SCENE's authoring-document framework (`ZenithDocument`): its registration and canonical schema, the importer-settings migration inside `.meta` documents, the removed-setting rule, the cook's recovery and refusal, the `.meta` fixtures | `Zenith2/Tools/ZenithCook/Internal/Database/` (`Zenith2/Docs/Design/ASSET/Migration.md`) |

### Explicit exclusions

| Not ASSET | Owner | Why |
|---|---|---|
| the supported-DCC matrix, the glTF, image, audio and FBX importers themselves, coordinate and unit conversion, import presets, reimport that preserves authored overrides, recoverable batch operations, rename/move/delete with reference repair, the out-of-process importer requirement and its crash-isolation tests, the shared DDC's operation and failure-mode requirements, semantic merges, distributed cooking, content provenance of source origin | CONTENT (Z2-I-CONTENT-01 … -31) | ASSET provides the mechanisms these build on; see "Boundary with CONTENT" |
| the `.zpak` packer and its packing policy (which files go into which archive, order, alignment beyond the minimum, chunking into several archives), patch and DLC manifests, Play Asset Delivery, dictionary training and policy per asset class, installation layouts | PACK | packaging (Z2-I-PACK-01 … -16); ASSET's cooked files are PACK's inputs; ASSET owns the archive reader and its version-1 layout contract (`Zenith2/Docs/Design/ASSET/Formats.md → "Archive file"`), PACK's packer writes that layout |
| the formats and cook steps of meshes, materials and textures, texture streaming, the loaders Flux implements | GEOM | `Zenith2/Docs/Design/GEOM.md`; GEOM adopts ASSET's common header ("Cooked asset file" in `Zenith2/Docs/Design/ASSET/Formats.md`) |
| scene and prefab documents, their loaders and cooked formats, transactional scene loading, reference repair inside scenes; the authoring-document migration framework and canonical serialization (`ZenithDocument`, `ZenithDocumentHost`) | SCENE | Z2-I-SCENE-01 … -10; the framework is SCENE's library by ruling Q43 (a), and ASSET's share of [Z2-N-CONTRACT-550] is the `.meta` type on it |
| world streaming: cells, streaming priorities per cell, HLOD, coordinated subsystem streaming | STREAM | STREAM requests asset loads through `Zenith_AssetLoader` |
| the virtual filesystem, mounts, `Zenith_AsyncIO`, low-disk detection | PLAT | `Zenith2/Docs/Design/PLAT/Filesystem.md → "Asynchronous I/O"` |
| codecs, the section descriptor, section compression and decompression | COMPRESS | `Zenith2/Docs/Design/COMPRESS.md → "How cooked formats embed the descriptor"` |
| Luau bytecode cooking, the module manifest, `.d.luau` generation | SCRIPT | `Zenith2/Docs/Scripting.md → "Shipping bytecode compatibility"`; the cook driver runs SCRIPT's steps |
| schema validation and `schemas.zreg` | ECS | `Zenith2/Docs/Architecture/DataDefinedComponents.md → "Registration at game load"`; the cook driver runs ECS's step |
| shader compilation, material-variant enumeration, PSO precache sets | RPROD, GEOM | `Zenith2/Docs/Rendering/ShaderSystem.md`; the cook driver invokes `ZenithShaderCompiler` in cook mode as their step |
| the editor's asset browser, import dialogs, reimport commands, the migration UI | EDIT | EDIT reaches ASSET through the cook server ("The cook server" in `Zenith2/Docs/Design/ASSET/Cook.md`) |
| the on-screen profiler's drawing, asset profiling views, content-validation dashboards | DEVT | ASSET publishes counters, zones and reports ("Diagnostics and profiling") |
| third-party mod content | none | modding is excluded (Z2-D-075); no mount or catalogue path loads content from outside the package, patch, loose and user mounts |

### Boundary with CONTENT

ASSET owns the database, the DDC mechanism, cooking and loading; CONTENT owns the production workflows built on them. Where a CONTENT item needs an ASSET mechanism, the mechanism is specified here and the item's behaviour, policy and acceptance are CONTENT's:

| CONTENT item | ASSET provides | CONTENT decides and tests |
|---|---|---|
| Z2-I-CONTENT-01 … -13 (DCC matrix, conventions) | the importer interface, the intermediate outputs, the settings schema mechanism ("Importer interface" in `Zenith2/Docs/Design/ASSET/Import.md`) | each importer, its fidelity rules and per-DCC tests |
| Z2-I-CONTENT-14 import presets | preset references in `.meta`, settings resolution order ("Settings resolution" in Import.md) | the preset document format, preset authoring, shipped presets |
| Z2-I-CONTENT-15 reimport preserving authored overrides | sub-asset identity by importer key, identity preservation on reimport ("Identity preservation on reimport" in Import.md) | which authored data each importer treats as an override and how it merges |
| Z2-I-CONTENT-16 … -18 recoverable batch operations | the framework's journaled write and recovery for `.meta` batches (`Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Journaled write"`, used as `Zenith2/Docs/Design/ASSET/Migration.md → "Reading and writing meta documents through the framework"` states), the cook journal ("Interrupted cook and resume" in Cook.md) | batch operation definitions, reports, resumption UX |
| Z2-I-CONTENT-19 … -21 rename, move, delete with repair | rename and move detection keeping GUIDs, tombstones, the reference edges of the dependency graph ("Rename and move detection", "Dependency graph" in `Zenith2/Docs/Design/ASSET/Database.md`) | the repair operations and their UI flow |
| Z2-I-CONTENT-22 importer crash isolation | the cook worker-process pool and its protocol ("Worker processes" in Cook.md) | the requirement that importers run out of process, and the crash tests |
| Z2-I-CONTENT-23 … -26 shared DDC and its failure modes | the store interface, the client's fallback, verification and never-corrupt rules ("Failure modes", "Shared store contract" in `Zenith2/Docs/Design/ASSET/DDC.md`) | the shared store backend, its deployment and its failure-mode acceptance runs |
| Z2-I-CONTENT-30 distributed cooking | pure, keyed cook steps whose outputs any host can produce ("Keys and invalidation" in Cook.md) | the distribution scheduler |
| Z2-I-CONTENT-31 content provenance | pipeline provenance records ("Provenance records" in Database.md) and a `.meta` member `provenance` that ASSET carries unchanged | the origin metadata's schema (DCC, author, licence) |

### Inventory items covered

| Item | Text (Inventory v1) | Covered by |
|---|---|---|
| Z2-I-ASSET-01 | asset database | Database.md "The asset database", "Scan and reconcile"; Formats.md "Asset database file" |
| Z2-I-ASSET-02 | asset dependency graph | Database.md "Dependency graph"; Loading.md "Dependency loading"; Cook.md "Dependency-driven invalidation" |
| Z2-I-ASSET-03 | import | Import.md "Importer interface", "Import algorithm" |
| Z2-I-ASSET-04 | reimport | Import.md "Reimport" |
| Z2-I-ASSET-05 | stable GUIDs | Database.md "Asset identity" |
| Z2-I-ASSET-06 | importer settings | Import.md "Import settings"; Formats.md "Meta sidecar schema"; Migration.md "The meta document type", "Importer settings inside meta documents" |
| Z2-I-ASSET-07 | derived-data cache | DDC.md "Key derivation", "Local store", "Lookup populate and verify" |
| Z2-I-ASSET-08 | derived-data cache invalidation | DDC.md "Key inputs", "Failure modes" |
| Z2-I-ASSET-09 | incremental cooking | Cook.md "Incremental cook", "Interrupted cook and resume" |
| Z2-I-ASSET-10 | asset validation | Cook.md "Validation"; Loading.md "Header read and validation" |
| Z2-I-ASSET-11 | asset provenance | Database.md "Provenance records" |
| Z2-I-ASSET-12 | missing assets | Loading.md "Missing and corrupt assets" |
| Z2-I-ASSET-13 | corrupt assets | Loading.md "Missing and corrupt assets"; Formats.md "Checks" |
| Z2-I-ASSET-14 | async loading | RuntimeAPI.md "Zenith_AssetLoader header", "Zenith_AssetArchive header"; Loading.md "Asset state machine", "Pipeline stages", "Cancellation" |
| Z2-I-ASSET-15 | residency budgets | Loading.md "Residency budgets and eviction" |
| Z2-I-ASSET-16 | decompression back-pressure: bounded decode queues | Loading.md "Decompression back-pressure" |
| Z2-I-ASSET-17 | decompression back-pressure: I/O throttled by decode capacity | Loading.md "Issue rule" |

### Document family

| File | Content | Its `##` headings |
|---|---|---|
| `Zenith2/Docs/Design/ASSET.md` (this file) | purpose, dependencies, layout, the API map and the top-level contracts, every skeleton section | the headings of this file |
| `Zenith2/Docs/Design/ASSET/RuntimeAPI.md` | every public declaration of `ZenithAssets` with its contract block, the archive reader and the built-in kinds | Scope of this file · Header list · Zenith_AssetTypes header · Zenith_AssetHandle header · Zenith_AssetRegistry header · Zenith_AssetKindLoader header · Zenith_AssetLoader header · Zenith_AssetReader header · Zenith_AssetCatalogue header · Zenith_CookedFile header · Zenith_AssetArchive header · Zenith_BuiltInAssets header · Registry internals · Internal types and files · Examples |
| `Zenith2/Docs/Design/ASSET/Loading.md` | the state machine, the loading pipeline, back-pressure, cancellation, residency, fallback, reload, device-loss reload, boot, shutdown | Scope of this file · Load request lifecycle · The service · Pipeline stages · Decompression back-pressure · Cancellation · Publication to simulation · Residency budgets and eviction · Unload and reload · Missing and corrupt assets · Device-loss reload · Headless loading · Shutdown sequence · Complexity |
| `Zenith2/Docs/Design/ASSET/Database.md` | GUIDs, sub-asset identity, `.meta` handling, the database, scan and reconcile, the dependency graph, provenance | Scope of this file · Asset identity · The meta sidecar · The asset database · Scan and reconcile · Dependency graph · Provenance records · Reference defaults in the cook · Database errors |
| `Zenith2/Docs/Design/ASSET/Import.md` | the importer interface, settings, import, reimport | Scope of this file · Importer interface · Importer registry · Import settings · Import algorithm · Intermediates · Reimport · Import errors · Boundary with CONTENT importers |
| `Zenith2/Docs/Design/ASSET/Cook.md` | the cook driver, steps, planning, incremental cooking, execution, validation, global outputs, resume, the cook server | Scope of this file · Cook command line · Cook services and composition · Cook steps · Planning · Incremental cook · Execution · Validation · Global outputs · Interrupted cook and resume · Low disk during a cook · Stale output removal · The cook server · Cook errors |
| `Zenith2/Docs/Design/ASSET/DDC.md` | the derived-data cache | Scope of this file · Key derivation · Zenith_DerivedDataCache API · Local store · Lookup populate and verify · Failure modes · Shared store contract · Concurrency across processes · DDC errors · Relationship to evidence fingerprints |
| `Zenith2/Docs/Design/ASSET/Formats.md` | every ASSET file format, byte for byte, and the cook server protocol | Scope of this file · Common rules · Cooked asset file · Asset catalogue file · Archive file · Meta sidecar schema · Tombstone list · Asset database file · DDC entry file · Content rule table · Cook journal · Cook lock file · Cook report · Provenance record lines · Dependency file · Cook server messages · Worker messages · Load result payload |
| `Zenith2/Docs/Design/ASSET/Migration.md` | the `.meta` document type on SCENE's authoring-document framework | Scope of this file · Ownership after the framework ruling · The meta document type · Importer settings inside meta documents · Reading and writing meta documents through the framework · Removed settings and unknown fields · Recovery before a cook · Meta migration fixtures · Registered document types · Migration errors at the ASSET boundary |
| `Zenith2/Docs/Design/ASSET/Tests.md` | every named test with the case it asserts | Scope of this file · Test targets · Handle and registry tests · Catalogue and cooked file tests · Loader tests · Back-pressure tests · Residency tests · Cancellation and stress tests · Missing and corrupt content tests · Fuzz targets · Archive and built-in kind tests · Database tests · Import tests · Cook tests · DDC tests · Meta document tests · Integration suite · Fixtures · Gate steps per test group |

### Design choices at a glance

<!-- elaboration -->
| # | Choice | Section |
|---|---|---|
| A1 | Two identities, never mixed: the asset GUID (a `Zenith_PersistentID` written in the tracked `.meta` sidecar) names an asset forever; a cooked object's file name is derived from the GUID (`content:/Objects/<xx>/<guid>.<ext>`), so renaming a source re-cooks nothing | Database.md "Asset identity"; "Cooked output tree" |
| A2 | Import (source → platform-independent intermediate) and cook (intermediate → platform cooked file) are separate pure steps, each keyed by a 128-bit hash of its inputs and cached in the DDC | Import.md "Import algorithm"; DDC.md "Key derivation" |
| A3 | Every cooked asset file begins with one 64-byte header and a 40-byte-per-section table holding COMPRESS's 24-byte descriptor, a per-section check and the section offset; GEOM's header is this header | Formats.md "Cooked asset file" |
| A4 | Asset kinds are a closed enumeration in `ZenithAssets` (games contain no C++, so no game adds a kind): dense codes below a cap of 256, the next code taken by the area that adds a kind in an ASSET-reviewed API change that also bumps the engine cook version; each kind's owner registers a loader through an upward interface ZenithAssets declares | RuntimeAPI.md "Asset kinds and the kind table" |
| A5 | A load is one pipeline per asset shared by every request for it; each request is its own operation handle; the pipeline stops when its last request is cancelled; a pipeline waits for its dependencies and its kind's loader before admission, holding no load record, and reloads and device recoveries pass the same admission | Loading.md "Request deduplication and priorities", "Load record stages", "In-flight load budget" |
| A6 | Decompression back-pressure is a reservation: a section read is issued only when a decode-queue unit and its staging bytes are reserved, so reads in flight plus undecoded sections never exceed the decode queue depth row | Loading.md "Issue rule" |
| A7 | Loads complete on background threads; simulation sees a load only through its requester's tick-boundary publication, never through the live registry state | Loading.md "Publication to simulation" |
| A8 | The pipeline runs in 2 ms background slices kicked by events, plus one cheap engine node per frame (`Assets.Service`) that polls I/O completions; nothing in ASSET blocks a job | Loading.md "The service" |
| A9 | Zero-reference assets enter a bounded keep-alive cache; memory pressure flushes it (warning) and drops CPU copies of GPU-resident data (critical), exactly the Assets row of the governor's shedding table | Loading.md "Memory pressure responses" |
| A10 | The build's cook is read-only on the source tree: a source without a `.meta` fails the cook; `.meta` files are created only by the explicit `--assign-guids` mode, the editor through the cook server, or a ticket's diff | Database.md "Build cooks never write sources" in "The meta sidecar" |
| A11 | The editor never links cook code: it drives a persistent `ZenithCook --serve` process over a line-delimited JSON protocol, which also isolates importer crashes | Cook.md "The cook server" |
| A12 | The DDC client never trusts a hit it cannot verify: every entry carries its key, producer versions and a hash per payload, and a corrupt, stale or unreachable entry is a miss followed by a local cook | DDC.md "Failure modes" |
| A13 | A GPU-kind asset activates when its object is complete on the CPU side and its uploads are requested into its owner's pending list; the owner allocates and queues them at its own render-graph node, and GPU residency is a separate sub-state the loader reports as RESIDENT, or FAILED after a post-activation allocation or upload failure (the kind's fallback then applies), so nothing in ASSET waits for rendering and startup closes before the first frame | Loading.md "GPU residency sub-state", "Tool and boot servicing" |
| A14 | Fallbacks are engine content the cook lists in every base catalogue's fallback table from the steps' declarations; the runtime pins them at loader registration and compiles in no fallback GUID | Cook.md "Engine fallbacks"; Loading.md "Fallback policy per kind" |
| A15 | The `.zpak` reader is ZenithAssets' implementation of PLAT's mount-source interface, reached through a factory PLAT calls, so ZenithPlatform never depends upward and PACK owns only the writer | RuntimeAPI.md "Zenith_AssetArchive header"; Formats.md "Archive file" |

## Dependencies

### Libraries ASSET depends on

| ASSET code | Depends on | Why |
|---|---|---|
| `ZenithAssets` | ZenithCore, ZenithReflect, ZenithPlatform, ZenithCompression, ZenithJobs, ZenithProfiling — exactly the row of `Zenith2/Docs/Architecture.md → "Library dependency graph"` (layer 3) | handles, containers, results, JSON, hashing; the type registry and asset value hooks; the virtual filesystem and async I/O; section decoding; background jobs; zones and counters |
| `ZenithCook` (ASSET's files) | the tool's declared set of `Zenith2/Docs/Architecture.md → "Library dependency graph"` (tools table), plus SCENE's `ZenithDocument` and `ZenithDocumentHost` (core-doc item X109 (a), pending), `ZenithRuntime`'s engine-type registration and every module library (core-doc item X114 (a), pending) | the database, import, cook, DDC, the `.meta` document type; module types for schema validation and `.d.luau` |

ASSET adds no dependency edge to `ZenithAssets`' Architecture row: the archive reader implements ZenithPlatform's `Zenith_MountSource`, which ZenithAssets already depends on. The tool-side edges are core-doc items (X109, X114) recorded as assumptions below.

### Headings ASSET relies on

| Document heading | What ASSET takes from it |
|---|---|
| `Zenith2/Docs/Contracts.md → "Handle kinds"` | the 16-byte `Zenith_AssetHandle<TAsset>` layout, reference counting, cross-world sharing, serialization as persistent ID, the handle tests |
| `Zenith2/Docs/Contracts.md → "Compatibility policy per kind"` | cooked re-cook and refusal; authoring-document migration with preview, backup, journaled write, override and unknown-field preservation; the migration step interface; fixtures |
| `Zenith2/Docs/Contracts.md → "Threading and concurrency contract"` | the lock levels 200 and 250 ZenithAssets owns; cancellation completion; bounded queues; shutdown order steps 4, 5 and 10 |
| `Zenith2/Docs/Contracts.md → "Errors and results"` | error records ≤ 32 bytes with `m_eCode`; untrusted input never asserts |
| `Zenith2/Docs/Architecture.md → "Memory governor"` | the Assets row of "Shedding per subsystem", `MEMORY_SUBSYSTEM_ASSETS`, transition peaks |
| `Zenith2/Docs/Architecture.md → "Security architecture"` | parser limits, trust boundaries of shipped content and patches |
| `Zenith2/Docs/Architecture.md → "Reflection and runtime type registration"` | `TYPE_KIND_ASSET`, `PROPERTY_TYPE_ASSET`, lifecycle hooks for fields that carry lifetime |
| `Zenith2/Docs/Design/ECS/Reflect.md → "Zenith_TypeRegistry"`, "Value-kind hooks", "Schema hooks"; `Zenith2/Docs/Design/ECS/Schemas.md → "Default values"` | `Zenith_ValueKindHooks`, `Zenith_PfnConstructFromPersistentID`, `InstallValueKindHooks`, `ResolveReferenceDefaults` (answering cross-area row E14) |
| `Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Zenith_MigrationRegistry header"`, "Zenith_DocumentMigrator header", "Zenith_DocumentStorage header", "Zenith_CanonicalJson header", "Opening a document", "Journaled write", "Storage-root writer lock", "Backups", "Migration fixtures", "ZenithDocumentHost"; `Zenith2/Docs/Design/SCENE.md → "ZenithDocument target"` | the authoring-document framework the `.meta` type registers into (ruling Q43 (a)); the storage-root writer lock ZenithCook's writers take through `GetWriterLock()` over ASSET's lock file (cross-area rows AS31, AS32); the call-local migrators of `OpenSourceDocument` |
| `Zenith2/Docs/Design/SCENE/Prefabs.md → "Scene and prefab cook steps"` | `Zenith_RegisterSceneCookSteps` at cook services item 6 and the two steps that read documents through `OpenSourceDocument` (cross-area row S24) |
| `Zenith2/Docs/Architecture/ECSModel.md → "Script lifetimes on the ECS side"` | asset-handle properties are read-only in chunk views and change through commands |
| `Zenith2/Docs/Architecture/CPUFrameGraph.md → "Job system"` | background jobs in 2 ms slices, `JOB_PRIORITY_BACKGROUND_HIGH` and `_LOW` |
| `Zenith2/Docs/Architecture/CompositionRoot.md → "Startup sequence"`, "The frame loop" | step 5 mounts (the archive factory, pending X154), step 6 engine types and configuration keys, step 10 value-kind hooks, step 11 the registry after the codec set, the reference defaults and SCENE's loader registration, steps 13 and 15 the other loader registrations, step 17 the startup scene; frame 0 runs no render subgraph |
| `Zenith2/Docs/Architecture/BuildSystem.md → "Games in CMake"` | the `<Name>_Content` target, `ZenithCook --game … --platform … --out …`, the depfile |
| `Zenith2/Docs/Architecture/LifecycleCommands.md → "World transition"` | scene loads for a transition; discarding results of a swapped-out world |
| `Zenith2/Docs/Scripting.md → "Merge-time capacity and asynchronous results"` | tick-boundary publication of `ctx:RequestLoad` results in completion-ID order |
| `Zenith2/Docs/Rendering/GpuResources.md → "Persistent resources"`, "Texture streaming swap", "Device loss" | what Flux's loaders do, reference counting by the asset system, device-loss reload through the residency set |
| `Zenith2/Docs/Design/CORE/Threading.md → "Zenith_OperationHandle implementation"` | `Zenith_OperationTable`, `Zenith_OperationTicket` |
| `Zenith2/Docs/Design/CORE/Containers.md → "Zenith_BoundedQueue"`, "Zenith_Array", "Zenith_HashMap and Zenith_HashSet" | queues and containers |
| `Zenith2/Docs/Design/CORE/Text.md → "JSON reader"`, "JSON writer" | `.meta` reading and canonical writing |
| `Zenith2/Docs/Design/CORE/Maths.md → "Zenith_Hash64"`, "Zenith_PersistentID functions" | `Zenith_Hash64`, `Zenith_Hash128Of`, `Zenith_Hash64Stream`; GUID text and binary forms |
| `Zenith2/Docs/Design/CORE/Diagnostics.md → "Structured logging"`, "Configuration store" | log categories; configuration keys |
| `Zenith2/Docs/Design/COMPRESS/PublicAPI.md → "Zenith_CompressedSection.h"`, "Zenith_CompressionCodecSet.h", "Zenith_CompressionContext.h" | section encode and decode, codec selection from the data, streamed decode |
| `Zenith2/Docs/Design/COMPRESS.md → "Compression setting fragment"`, "Cook integration" | the `.meta` compression fragment; DDC key members |
| `Zenith2/Docs/Design/PLAT/Filesystem.md → "Zenith_AsyncIO API"`, "Cancellation", "Zenith_VirtualFileSystem API", "Mount sources", "Mount manifest schema", "Validation rules and order", "Synchronous file access" | reads with priorities and completion queues; mounts; the `Zenith_MountSource` interface the archive reader implements |
| `Zenith2/Docs/Design/GEOM/Textures.md → "Texture residency model"`, "Fallback textures" | a texture activated before its tail is resident binds its usage's fallback; the per-usage fallbacks are engine content |
| `Zenith2/Docs/Design/PLAT/Configuration.md → "Registration API"` | `Zenith_RegisterAssetsConfigKeys` |
| `Zenith2/Docs/Design/JOBS/PublicAPI.md → "Zenith_JobSystem"`, `Zenith2/Docs/Design/JOBS/Algorithms.md → "Background jobs and operations"` | `SubmitBackground`, slices, `Continue`, cancellation tokens |
| `Zenith2/Docs/Design/MOD.md → "The composition.json record"`, "Startup scene and loose content root" | the selected modules; content-root mapping; the startup scene's virtual path |
| `Zenith2/Docs/Design/GEOM.md → "Assumed types from pending design documents"` | what GEOM expects of ASSET, reconciled in "Areas that depend on ASSET" |

### Areas ASSET depends on

| Area | For |
|---|---|
| CORE | containers, results, operation tables, JSON, hashing, persistent IDs, logging, configuration, the governor |
| PLAT | the virtual filesystem, async I/O, low-disk state, the mount manifest parser `Zenith_ParseMountManifest`, the configuration validators the cook runs |
| JOBS | background operations |
| COMPRESS | codecs, section descriptor, section and streamed decode |
| ECS (Reflect) | `TYPE_KIND_ASSET`, `Zenith_ValueKindHooks` and the reference-default resolution, `Zenith_MigrationContext`, `Zenith_PfnMigrationStep` |
| MOD | `composition.json`, the composition root's calls (steps 5, 6, 10, 11, 13, 15 and 17; shutdown), the `Assets.Service` engine node, `ZenithRuntime`'s engine-type registration for tools |
| SCENE | the authoring-document framework (`ZenithDocument`, `ZenithDocumentHost`); scene and prefab kinds |
| RHI, GEOM | Flux's kind loaders; device-loss recovery calls |

### Areas that depend on ASSET

| Area | Uses |
|---|---|
| GEOM | the common cooked header, `Zenith_AssetKindLoader` (kinds `ASSET_KIND_MESH`, `_MATERIAL`, `_TEXTURE`, `_TERRAIN`), `Zenith_AssetReader::ReadRange` and `ReadSection`, the cook step interface, `uZENITH_ENGINE_COOK_VERSION`, the cook report |
| RHI | the loader calls into `Flux_PersistentResources`; `Zenith_AssetRegistry::ReloadGpuResidencySet` in device-loss step 4 |
| SCENE | kinds `ASSET_KIND_SCENE`, `ASSET_KIND_PREFAB`; `Acquire` during deserialization; the `.meta` type registered into its framework; the cook step interface, the step registration slot of cook services item 6 and `Zenith_CookContext::OpenSourceDocument` for `scene.scene` and `scene.prefab`; the lock file its `Zenith_DocumentWriterLock` implementation opens |
| STREAM | `Zenith_AssetLoader` requests, priorities, cancellation, `Zenith_AssetLoadCompletionQueue` |
| SCRIPT | `Zenith.Asset<TKind>` bindings over `Zenith_AssetHandle<T>`; `ctx:RequestLoad` through `Zenith_AssetLoader`; verified reads of `.zlbc` files |
| ECS | the asset value hooks for `asset<K>` fields |
| MOD | registry creation at step 11, kind-loader registration window, shutdown step 10, the `<Name>_Content` command |
| PACK | cooked files and the catalogue as packaging inputs; catalogue fragments per asset pack with external references; the archive layout ASSET's reader accepts; the built-in dictionary kind |
| PLAT | the archive factory it calls when mounting package and patch archives |
| CONTENT | every mechanism of "Boundary with CONTENT" |
| EDIT | the cook server protocol |
| DEVT | counters, zones, dumps, provenance queries, the cook and validation reports |
| AUDIO, ANIM, UI, CINE, AI, PHYS, ADVGFX, VFX | their kinds' loaders and cook steps |

### Upward interfaces ASSET declares

ZenithAssets sits at layer 3, below every library that implements an asset kind. It declares the interfaces those libraries implement and the composition root passes down ([Z2-N-ARCH-078]):

| Interface | Implemented by | Purpose |
|---|---|---|
| `Zenith_AssetKindLoader` | Flux (texture, mesh, material, terrain), ZenithScene (scene, prefab), each module for its kinds | turn validated, decoded sections into a runtime object; cancel; unload; drop CPU copies |
| `Zenith_MemoryConsumer` (CORE's) | `Zenith_AssetRegistry` itself | the Assets row of the shedding table |

ZenithAssets stores no pointer into a higher library except those registered loader references, and calls them only through the interface. In the other direction it implements two interfaces lower libraries declare: PLAT's `Zenith_MountSource` (the archive reader, handed to ZenithPlatform through the factory the composition root passes at startup step 5) and ECS's `Zenith_ValueKindHooks` with its `Zenith_PfnConstructFromPersistentID` resolver (installed and bound by the composition root at steps 10 and 11).

### Assumed interfaces pending other areas

<!-- assumption: pending ARCH (X109, X110, X114, X147), SCENE, MOD -->
| Need | Owner | ASSET's assumption |
|---|---|---|
| the authoring-document framework's libraries | ARCH (layout and tools tables), SCENE | ruled Q43 (a): SCENE's engine library `ZenithDocument` (layer 2) and tool library `ZenithDocumentHost`; `ZenithCook` and `ZenithEditor` depend on both (core-doc item X109 (a), pending); ASSET registers the `.meta` type into it (cross-area rows S4, AS1) |
| module types in the cook | ARCH (tools table), MOD, SCRIPT | ZenithCook declares `ZenithRuntime` and every module library, and calls ZenithRuntime's engine-type registration and each selected module's `Zenith_Module_<NAME>_RegisterTypes` in canonical order, so its type registry equals the game's ([Z2-N-ARCH-847]) for schema validation, scene cooking and `.d.luau` (core-doc item X114 (a), pending) |
| the `Assets.Service` engine node | MOD, GRAPH | ZenithRuntime registers one engine-wide node `Assets.Service` in `FRAME_SUBGRAPH_EPILOGUE`, stage 12, group 0, `FRAME_NODE_KIND_STAGE`, `JOB_AFFINITY_ANY`, declaring a write of the world-independent resource `Zenith_AssetRegistry` and nothing else, in windowed and headless configurations alike; its body is `Zenith_AssetLoader::ServiceFrame` (core-doc item X110, pending) |
| tool test linking | ARCH (BuildSystem) | `zenith_add_tool` for an executable compiles every `Internal/**` source except `Internal/Zenith_<Name>Main.cpp` into an object library linked by both `<Name>` and `<Name>_Tests` (core-doc item X114 (b), pending) |
| the ASSET calls of the startup and shutdown tables | ARCH (CompositionRoot), MOD | step 6 `Zenith_RegisterAssetsConfigKeys` and `Zenith_RegisterAssetTypes`; step 10 `InstallValueKindHooks`; step 11 `Create` (core-doc item X110, pending) and `ResolveReferenceDefaults(PROPERTY_TYPE_ASSET, …)` with exit code 3 on an unresolvable default (core-doc item X147 (4), pending); after step 15 `CloseRegistration` and `ServiceUntilComplete(GetPinnedLoads())` (exit code 3 on failure); step 17 bounded by `runtime.startupLoadTimeoutMs`; shutdown step 4 `BeginShutdown`, step 10 the registry's destruction (X110, pending; cross-area rows AS5, AS12) |
| the `.zpak` reader's owner | PLAT, PACK | ruled: ASSET's reader in ZenithAssets, PACK's writer (core-doc item X114 (d), pending, the ownership wording only; cross-area row AS24) |
<!-- /assumption -->

<!-- assumption: pending PLAT (X154) -->
| Need | Owner | ASSET's assumption |
|---|---|---|
| the step-5 archive factory | ARCH (CompositionRoot), PLAT | a new ask, not an existing interface: `Zenith2/Docs/Design/PLAT/Filesystem.md` has no factory member in `Zenith_VirtualFileSystemDesc` and a two-word `Zenith_SourceFile` today. X154 (open) asks for (a) the startup table's step-5 row passing `&Zenith_CreateArchiveMountSource` and a `Zenith_ArchiveReaderDesc` in the `Zenith_VirtualFileSystemDesc`, and (b) PLAT's `m_pfnCreateArchiveSource` and `m_pArchiveUserData` members and `Zenith_SourceFile` widened to `uint64 m_aulOpaque[4]` (cross-area row AS22; RuntimeAPI.md "Zenith_AssetArchive header") |
<!-- /assumption -->

<!-- assumption: pending PLAT -->
| Need | Owner | ASSET's assumption |
|---|---|---|
| a public mount-manifest validator the cook can call | PLAT | ZenithPlatform exports `Zenith_Result<void, Zenith_FileError> Zenith_ParseMountManifest(Zenith_Span<const uint8> xBytes, Zenith_Arena& xArena)`, applying every rule of `Zenith2/Docs/Design/PLAT/Filesystem.md → "Mount manifest schema"` and returning `FILE_ERROR_MANIFEST_INVALID` with the key path; the `asset.manifest` step validates its output with it before publishing |
<!-- /assumption -->

## Library and file layout

### Target summary

| Target | Kind | Directory | Public headers | `ZENITH_SUPPORTED` |
|---|---|---|---|---|
| `ZenithAssets` | engine library, `zenith_add_library(ZenithAssets DEPENDS ZenithCore ZenithReflect ZenithPlatform ZenithCompression ZenithJobs ZenithProfiling)`; `ZenithProfiling` joins the call in the diff of the first ZenithAssets source that publishes a zone or a counter, so the target configures before the profiling library exists | `Zenith2/Engine/ZenithAssets/` | the nine headers of "ZenithAssets public root headers" | `ZENITH_ALL` (the runtime loader runs on every preset, including `linux-tsan` headless) |
| `ZenithAssets_Tests` | test executable (created by the helper) | `Zenith2/Engine/ZenithAssets/Tests/` | — | host presets |
| `ZenithCook` (ASSET's files) | host tool executable, `zenith_add_tool(ZenithCook DEPENDS … ZenithAuthoring ZenithContentCore THIRD_PARTY fastgltf meshoptimizer ktx-software)` (manifest entry names, lower-case kebab, ruling MANIFEST-NAMES; `ZenithAuthoring` is appended by ASSET's first ZenithAuthoring move and `ZenithContentCore` by CONTENT's first ZenithCook importer, cross-area row CT4) | `Zenith2/Tools/ZenithCook/` | none: tool internals | `ZENITH_TOOLS_HOST` |
| `ZenithCook_Tests` | test executable | `Zenith2/Tools/ZenithCook/Tests/` | — | `win` |
| `Zenith_Integration_AssetPipeline` | integration suite | `Zenith2/Tests/AssetPipeline/` | — | `win`, `win-asan` |
| `Zenith_Bench_AssetLoading` | benchmark harness | `Zenith2/Benchmarks/AssetLoading/` | — | `win`, `android-arm64` (built, run on device by the harness ticket) |

### ZenithAssets public root headers

| Header | Declares | Specified in |
|---|---|---|
| `Zenith_AssetTypes.h` | `Zenith_AssetGUID`, `Zenith_AssetKind`, `Zenith_AssetKindInfo`, `Zenith_AssetKindSet`, `Zenith_GetAssetKindInfo`, `Zenith_AssetPriority`, `Zenith_AssetState`, `Zenith_AssetGpuResidency`, `Zenith_AssetResidencyTicket`, `Zenith_AssetFallbackPolicy`, `Zenith_AssetPlatform`, `Zenith_AssetErrorCode`, `Zenith_AssetCorruption`, `Zenith_AssetError`, `uZENITH_ENGINE_COOK_VERSION`, limits, fault points, `Zenith_ToString` overloads | RuntimeAPI.md "Zenith_AssetTypes header" |
| `Zenith_AssetHandle.h` (+ `.inl`) | `Zenith_AnyAssetHandle`, `Zenith_AssetHandle<TAsset>`, `Zenith_AssetType`, `Zenith_GetAssetHandleValueKindHooks`, `Zenith_ResolveAssetReferenceDefault` | RuntimeAPI.md "Zenith_AssetHandle header" |
| `Zenith_AssetRegistry.h` (+ `.inl`) | `Zenith_AssetRegistryDesc`, `Zenith_AssetRegistry`, `Zenith_AssetRegistryStats`, `Zenith_RegisterAssetTypes`, `Zenith_RegisterAssetsConfigKeys` | RuntimeAPI.md "Zenith_AssetRegistry header" |
| `Zenith_AssetKindLoader.h` | `Zenith_AssetKindLoaderDesc`, `Zenith_AssetLoadInfo`, `Zenith_AssetSectionRequest`, `Zenith_AssetLoadedSection`, `Zenith_AssetLoadContext`, `Zenith_AssetUnloadContext`, `Zenith_AssetKindLoader` | RuntimeAPI.md "Zenith_AssetKindLoader header" |
| `Zenith_AssetLoader.h` | `Zenith_AssetLoadFlags`, `Zenith_AssetLoadRequest`, `Zenith_AssetLoadCompletion`, `Zenith_AssetLoadCompletionQueue`, `Zenith_AssetLoaderDesc`, `Zenith_AssetLoaderStats`, `Zenith_AssetLoader` | RuntimeAPI.md "Zenith_AssetLoader header" |
| `Zenith_AssetReader.h` | `Zenith_AssetReader` | RuntimeAPI.md "Zenith_AssetReader header" |
| `Zenith_AssetCatalogue.h` | `Zenith_AssetDependencyFlags`, `Zenith_AssetDependency`, `Zenith_AssetFallbackRow`, `Zenith_AssetCatalogueEntry`, `Zenith_AssetCatalogueFile`, `Zenith_AssetCatalogue` | RuntimeAPI.md "Zenith_AssetCatalogue header" |
| `Zenith_CookedFile.h` | `Zenith_CookedHeader`, `Zenith_CookedSectionEntry`, `Zenith_CookedFileExpectation`, `Zenith_CookedFileView`, `Zenith_ParseCookedHeader`, `Zenith_ComputeSectionCheck`, `Zenith_CookedFileWriter` | RuntimeAPI.md "Zenith_CookedFile header" |
| `Zenith_AssetLoadResult.h` | `Zenith_AssetLoadResultRecord` and its encode and decode functions (the replay payload) | Formats.md "Load result payload" |
| `Zenith_AssetArchive.h` | `Zenith_ArchiveReaderDesc`, `Zenith_CreateArchiveMountSource`, `Zenith_VerifyArchiveData` | RuntimeAPI.md "Zenith_AssetArchive header" |
| `Zenith_BuiltInAssets.h` | `Zenith_DataAsset`, `Zenith_CompressionDictionaryAsset` | RuntimeAPI.md "Zenith_BuiltInAssets header" |

### ZenithAssets internal files

| Path (under `Zenith2/Engine/ZenithAssets/`) | Content |
|---|---|
| `Internal/Zenith_AssetTypes.cpp` | the kind table, `Zenith_ToString` bodies |
| `Internal/Registry/Zenith_AssetRegistry.cpp`, `.h` | creation, acquisition, resolution, kind-loader registration, catalogue snapshots, stats, dump |
| `Internal/Registry/Zenith_AssetSlots.cpp`, `.h` | the slot-header array, the free list, generations, the retire queue ("Registry internals") |
| `Internal/Registry/Zenith_AssetHandle.cpp` | out-of-line copy and destroy paths, the value-kind hooks, the reference-default resolver |
| `Internal/Registry/Zenith_AssetLocks.h` | `uLOCK_LEVEL_ASSET_REGISTRY` (200), `uLOCK_LEVEL_STREAMING_REQUESTS` (250) |
| `Internal/Catalogue/Zenith_AssetCatalogue.cpp`, `Zenith_CatalogueMerge.cpp` | parsing, validation, fragment merge, indexes |
| `Internal/CookedFile/Zenith_CookedHeader.cpp`, `Zenith_CookedFileWriter.cpp` | header parse and validate; the writer |
| `Internal/Loading/Zenith_AssetLoader.cpp`, `.h` | request intake, operations, completion queues |
| `Internal/Loading/Zenith_AssetService.cpp`, `.h` | the service slice, stage advance, scheduling |
| `Internal/Loading/Zenith_LoadRecord.h` | load records and section records |
| `Internal/Loading/Zenith_DecodeQueue.cpp`, `.h` | decode units, reservations, priority reserves, decode jobs, streamed decode |
| `Internal/Loading/Zenith_StagingPool.cpp`, `.h` | I/O staging and decode buffers |
| `Internal/Loading/Zenith_HeadlessKindLoader.cpp`, `.h` | the validate-only loader of GPU kinds in headless runs |
| `Internal/Loading/Zenith_AssetReader.cpp` | range and section reads, verified file reads |
| `Internal/BuiltIn/Zenith_BuiltInLoaders.cpp`, `.h` | the `ASSET_KIND_DATA` and `ASSET_KIND_COMPRESSION_DICTIONARY` loaders |
| `Internal/Archive/Zenith_ArchiveMountSource.cpp`, `.h`, `Zenith_ArchiveFormat.cpp`, `.h` | the archive factory, checks A1–A7, lookup, listing, reads, `Zenith_VerifyArchiveData` |
| `Internal/Residency/Zenith_AssetResidency.cpp`, `.h` | resident-byte accounting, the keep-alive cache, the memory consumer |
| `Internal/Config/Platform/windows.config.json`, `android.config.json`, `linux.config.json` | ZenithAssets' configuration fragments: the values of "Configuration keys", written by the Stage 1 per-system budget ticket for ASSET |

`ZenithAssets` has no `Internal/Windows|Android|Linux` sources: every platform difference is below it, in `Zenith_AsyncIO` and the virtual filesystem.

### ZenithAssets test files

| Path (under `Zenith2/Engine/ZenithAssets/Tests/`) | Content |
|---|---|
| `Zenith_AssetHandle_Tests.cpp`, `Zenith_AssetRegistry_Tests.cpp`, `Zenith_AssetCatalogue_Tests.cpp`, `Zenith_CookedFile_Tests.cpp` | handles, registry, catalogue, container |
| `Zenith_AssetLoader_Tests.cpp`, `Zenith_AssetStateMachine_Tests.cpp`, `Zenith_AssetBackPressure_Tests.cpp`, `Zenith_AssetResidency_Tests.cpp`, `Zenith_AssetCancel_Tests.cpp`, `Zenith_AssetFailure_Tests.cpp`, `Zenith_AssetReader_Tests.cpp` | loading |
| `Zenith_AssetArchive_Tests.cpp`, `Zenith_AssetBuiltIn_Tests.cpp` | the archive reader; the built-in kinds |
| `Zenith_AssetLoader_DefectTests.cpp` | death tests (`z2-death-check`) |
| `Support/Zenith_FakeKindLoader.h` | a scripted kind loader (test kinds `0x7F00`–`0x7F0F`) with per-stage delays, failures and live-object counters |
| `Support/Zenith_AssetFixtureWriter.h` | builds cooked files and catalogues in memory or in a temporary user directory through `Zenith_CookedFileWriter` |
| `Support/Zenith_AssetRegistryTestAccess.h` | a friend of `Zenith_AssetRegistry` for ZenithAssets' own tests only, declared in RuntimeAPI.md "Test access" |
| `Support/Zenith_AssetRegistryTestRig.h`, `Support/Zenith_AssetGatedMountSource.h`, `Support/Zenith_MemoryMountSource.h`, `Support/Zenith_AssetFixtureRecorder.h` | the registry test rig, a mount source that gates reads, an in-memory mount source with error injection, and the corrupt-content fixture recorder |
| `Support/Zenith_AssetStageHold.h` | header-only stage holds for other areas' cancellation stress: parks one asset at the start of DECODING, DELIVER or ACTIVATING (RuntimeAPI.md "Test access", cross-area row ST20) |
| `Fuzz/CookedHeader.cpp`, `Fuzz/Catalogue.cpp`, `Fuzz/SectionDecode.cpp`, `Fuzz/LoadResult.cpp`, `Fuzz/Archive.cpp` | libFuzzer targets (`Zenith_Fuzz_CookedHeader`, `_Catalogue`, `_SectionDecode`, `_LoadResult`, `_Archive`) seeded at start-up from code-built inputs (no committed corpus) |
| `Fixtures/KindTable.json` | the one committed fixture file (Tests.md "Fixtures"); no binary cooked file is committed: the archives and the corrupt-content set are generated at test time |
| `Examples/Zenith_AssetLoader_Example.cpp`, `Zenith_CookedFile_Example.cpp` | the examples the contract blocks cite |

Every file under `Tests/Support/` is header-only (`inline` functions, class members defined in the class, and templates; the STL is allowed in tests), because outside ZenithCore that directory is compiled by no target and a `.cpp` there is a configure error (`Zenith2/Docs/Architecture/BuildSystem.md` [Z2-N-ARCH-255]); its headers are included by ZenithAssets' tests, its fuzz targets and the tests of libraries that depend on ZenithAssets.

### ZenithCook files

ZenithCook is ASSET's tool (`Zenith2/Docs/Architecture.md → "Repository layout"`, table "Ownership per directory": ASSET, importers CONTENT). Other areas own subdirectories of its `Internal/` (GEOM's `Geometry/`, `Skinning/`, `Materials/`, `Textures/`, `Terrain/`; CONTENT's `Importers/`; SCRIPT's `Script/`; ECS's `Schemas/`; RPROD's `Shaders/`; PACK's `Package/`); ASSET owns the rest:

| Path (under `Zenith2/Tools/ZenithCook/`) | Content |
|---|---|
| `CMakeLists.txt` | the `zenith_add_tool(ZenithCook …)` call |
| `Internal/Zenith_ZenithCookMain.cpp` | `main`: root heap, hooks, command line, the cook services, dispatch to cook, assign-guids, serve, worker, provenance and fix-duplicates modes |
| `Internal/Zenith_CookServices.cpp`, `.h` | the tool's composition root: heap, job system, VFS over host roots, codecs, type registry, step and importer registries, DDC |
| `Internal/Database/Zenith_AssetDatabase.cpp`, `.h` | `Zenith_AssetDatabase` |
| `Internal/Database/Zenith_AssetMeta.cpp`, `.h` | `Zenith_AssetMeta`, reading from the framework's opened document, canonical writing |
| `Internal/Database/Zenith_AssetMetaDocument.cpp`, `.h` | the `Zenith_AssetMetaSettingsSource` adapter over the frozen importer registry; until ASSET's ZenithAuthoring move lands, also the `Zenith.AssetMeta` registration, canonical schema and visitor, which then live in `Zenith2/Tools/Authoring/Internal/Asset/` behind `Zenith_AuthoringAsset.h` (Migration.md "The meta document type", cross-area rows ED42, CT8) together with the DDC store interface (DDC.md "Zenith_DerivedDataCache API") |
| `Internal/Database/Zenith_Tombstones.cpp`, `.h` | reading and rewriting the content roots' `.zenith-tombstones` lists |
| `Internal/Database/Zenith_AssetScan.cpp`, `.h` | scan and reconcile |
| `Internal/Database/Zenith_AssetDependencyGraph.cpp`, `.h` | the graph |
| `Internal/Database/Zenith_Provenance.cpp`, `.h` | provenance records and queries |
| `Internal/Database/Zenith_ImportDiagnosticsBlob.cpp`, `.h` | the `ZIDG` blob of stored import diagnostics (Formats.md "Import diagnostics blob"), shared by the database file and the DDC's import record entries |
| `Internal/Import/Zenith_Importer.h`, `Zenith_ImporterRegistry.cpp`, `.h`, `Zenith_ImportDriver.cpp`, `.h`, `Zenith_ImportSettings.cpp`, `.h` | the importer interface, registry, driver, settings |
| `Internal/Import/Zenith_PassthroughImporters.cpp` | ASSET's own importers: `raw` (verbatim bytes as a test and data kind) and the Zenith-native document passthrough |
| `Internal/Import/Zenith_ImportDocuments.cpp`, `.h` | the call-local document store of `OpenSourceDocument`, `OpenInputDocument`, `OpenInputDocumentByGUID` and `OpenInputDocumentRelativeTo` (Import.md "Input documents", "Documents relative to another asset") |
| `Internal/Import/Zenith_ImportProducts.cpp`, `.h` | previous products, owner members, fingerprints, derived products and importer report members (Import.md "Product members and fingerprints", "Derived products", "Import report members") |
| `Internal/Cook/Zenith_CookInputFiles.cpp`, `.h` | the `thirdparty:` root, `ReadRepositoryFile`, `ReadThirdPartyFile` and `GetPlanOrigins` (Cook.md "Repository and third-party files") |
| `Internal/Importers/`, `Internal/Content/` (CONTENT's) | CONTENT's importers and cook steps, registered at services items 4 and 6 (Cook.md "Cook services and composition", cross-area row CT4) |
| `Internal/Import/Zenith_ImportPropertyResolver.cpp`, `.h` | the run's resolved-property table, its waves and its sub-plans (Import.md "Published-property inputs") |
| `Internal/Import/Zenith_ImportReplay.cpp`, `.h` | replay of stored import diagnostics into the cook's diagnostics (Import.md "Replay into the cook report") |
| `Internal/Cook/Zenith_Cook.cpp`, `.h`, `Zenith_CookPlan.cpp`, `.h`, `Zenith_CookStep.h`, `Zenith_CookStepRegistry.cpp`, `.h`, `Zenith_CookContext.cpp`, `.h` | the driver, planning, steps |
| `Internal/Cook/Zenith_CookSourceDocuments.cpp`, `.h` | the reader-set pool and the call-local document stores of `OpenSourceDocument` (Cook.md "Source documents in cook steps") |
| `Internal/Cook/Zenith_CookLocks.cpp`, `.h` | the output-tree and database lock files, the storage-root holds through `Zenith_DocumentStorage::GetWriterLock()`, and the build cook's read-only probe `Zenith_ProbeStorageRootWriterLock` (Cook.md "Output tree and database locks") |
| `Internal/Cook/Zenith_CookValidation.cpp`, `.h` | structural, reference and rule validation |
| `Internal/Cook/Zenith_ContentRules.cpp`, `.h` | reading `Zenith2/Docs/ContentSpecs.rules.json` |
| `Internal/Cook/Zenith_EngineFallbacks.cpp`, `.h` | resolving the steps' declared engine fallbacks into the plan and the fallback table |
| `Internal/Cook/Zenith_CookOutputs.cpp`, `.h` | the output tree, atomic publication, stale removal |
| `Internal/Cook/Zenith_CatalogueWriter.cpp`, `Zenith_MountManifestWriter.cpp`, `Zenith_ConfigMerge.cpp`, `Zenith_DepFile.cpp`, `Zenith_CookReport.cpp`, `Zenith_CookJournal.cpp` | global outputs, resume journal |
| `Internal/Cook/Zenith_CookWorkers.cpp`, `.h` | the worker-process pool |
| `Internal/DDC/Zenith_DerivedDataCache.cpp`, `.h`, `Zenith_DerivedDataKey.cpp`, `.h`, `Zenith_LocalDerivedDataStore.cpp`, `.h` | the DDC |
| `Internal/DDC/Zenith_ImportEntries.cpp`, `.h` | the import record and intermediate entries, the import manifest (DDC.md "Import entries") |
| `Internal/Server/Zenith_CookServer.cpp`, `.h`, `Zenith_LineStream.cpp`, `.h`, `Zenith_CookWatch.cpp`, `.h` | the `--serve` protocol, line framing, file watching |
| `Tests/Database/`, `Tests/Import/`, `Tests/Cook/`, `Tests/DDC/`, `Tests/Server/`, `Tests/Fixtures/Games/` | ASSET's tool tests and their fixture games (Tests.md) |
| `Tests/Fuzz/MetaDocument.cpp`, `Tests/Fuzz/DdcEntry.cpp`, `Tests/Fuzz/AssetDatabase.cpp` | fuzz targets over the tool's parsers |

### Integration suite and benchmark files

| Path | Content |
|---|---|
| `Zenith2/Tests/AssetPipeline/CMakeLists.txt` | `zenith_add_integration_tests(AssetPipeline DEPENDS ZenithAssets ZenithRuntime …)` |
| `Zenith2/Tests/AssetPipeline/Zenith_AssetPipeline_Tests.cpp` | cook-then-load end to end over the fixture game `Tests/AssetPipeline/Fixtures/PipelineGame/` (the cook runs as a CTest fixture setup test `AssetPipeline_Cook`) |
| `Zenith2/Benchmarks/AssetLoading/` | `Zenith_Bench_AssetLoading`: load throughput, back-pressure behaviour and critical-load latency over a generated content set; the Stage 1 per-system budget ticket for ASSET runs it |

### ZENITH_SUPPORTED matrix

`ZenithAssets` is a CPU-only engine library and declares `ZENITH_ALL`, so its tests run on `win`, `win-asan` and `linux-tsan`, and it compiles for both Android presets (`Zenith2/Docs/Architecture/BuildSystem.md → "The zenith_add_library helper"`). ZenithCook is `ZENITH_TOOLS_HOST`. Nothing in `ZenithAssets` depends on a GPU library: GPU kinds reach it only through registered loaders.

## Public API

### API map

| Question a caller has | Answer |
|---|---|
| how do I hold an asset? | a `Zenith_AssetHandle<T>` from `Zenith_AssetRegistry::Acquire<T>(guid)` or `AcquireByLogicalPath<T>(path)`; copying adds a reference; the handle never loads anything by itself |
| how do I make it resident? | `Zenith_AssetLoader::RequestLoad` → `Zenith_OperationHandle`; poll it, or name a `Zenith_AssetLoadCompletionQueue` |
| how do I read it? | `Zenith_AssetRegistry::Resolve<T>(handle)` once ACTIVE, or `ResolveOrFallback<T>(handle)` |
| how do I implement a kind? | implement `Zenith_AssetKindLoader`, register it with `RegisterKindLoader` before `CloseRegistration` |
| how do I stream parts of a file? | `Zenith_AssetReader::ReadSection` (decoded, back-pressured) or `ReadRange` (raw bytes) |
| how do I cook a kind? | implement a `Zenith_CookStep` (Cook.md "Cook steps") and write the file through `Zenith_CookedFileWriter` |
| how do I import a source format? | implement a `Zenith_Importer` (Import.md "Importer interface") |
| how do I migrate `.meta` documents? | through SCENE's framework, into which ZenithCook registers `Zenith.AssetMeta` (Migration.md "The meta document type") |
| how does an archive get mounted? | PLAT calls `Zenith_CreateArchiveMountSource`, the factory the composition root passes at startup step 5 (RuntimeAPI.md "Zenith_AssetArchive header") |

### Rules across the ASSET API

- Every fallible function of `ZenithAssets`' public headers returns `Zenith_Result<T, Zenith_AssetError>`; every public type carries one contract block ([Z2-N-CONTRACT-111]).
- Replaceable pieces are interfaces with a virtual destructor (`Zenith_AssetKindLoader`); the registry, loader, reader and catalogue are concrete, because they have one implementation ([Z2-N-CONTRACT-035]).
- Objects with fallible construction use a static `Create` returning `Zenith_Result<Zenith_Owned<T>, Zenith_AssetError>`, with one fault point or heap countdown step per fallible step ([Z2-N-CONTRACT-222], [Z2-N-CONTRACT-341]).
- No public header includes a third-party, platform or STL header.
- Every byte read from a cooked file, a catalogue or a `.meta` document is untrusted: every size, count and offset is checked with overflow-checked arithmetic before use, and failure is a `Zenith_Result` error, never an assert ([Z2-N-ARCH-951]).
- Tool-side types (database, import, cook, DDC, the `.meta` type) follow the same naming and contract-block conventions in their `Internal/` headers, because other areas' code (CONTENT importers, GEOM cook steps) compiles against them; review confirms the blocks.
- Class layout follows `public:` → `protected:` → `private:` with one section of each; a constructor only the factory may call takes an internal tag type that is declared, not defined, in the public header (`Zenith_AssetRegistryConstructTag`).

### Asset kinds

The closed kind table (`Zenith_AssetKind`, `Zenith_AssetTypes.h`). The kind's owner specifies its cooked format and implements its loader and cook step; ASSET reserves the code, the names, the extension and the magic:

| Code | Enumerator | Stable name | Script name | Runtime object type | Owner | Cooked extension | Magic | Memory subsystem | Default fallback policy | Cacheable | GPU resident |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | `ASSET_KIND_TEXTURE` | `Flux.Texture` | `Texture` | `Flux_TextureResource` | GEOM | `.ztex` | `ZTEX` | RENDERING | SUBSTITUTE | yes | yes |
| 2 | `ASSET_KIND_MESH` | `Flux.Mesh` | `Mesh` | `Flux_MeshResource` | GEOM | `.zmesh` | `ZMSH` | RENDERING | NULL_OBJECT | yes | yes |
| 3 | `ASSET_KIND_MATERIAL` | `Flux.Material` | `Material` | `Flux_MaterialResource` | GEOM | `.zmat` | `ZMAT` | RENDERING | SUBSTITUTE | yes | yes |
| 4 | `ASSET_KIND_TERRAIN` | `Flux.Terrain` | `Terrain` | `Flux_TerrainResource` | ENV (geometry sections GEOM) | `.zterrain` | `ZTRN` | RENDERING | FAIL | no | yes |
| 5 | `ASSET_KIND_SCENE` | `Zenith.Scene` | `Scene` | `Zenith_Scene` | SCENE | `.zscene` | `ZSCN` | SIMULATION | FAIL | no | no |
| 6 | `ASSET_KIND_PREFAB` | `Zenith.Prefab` | `Prefab` | `Zenith_Prefab` | SCENE | `.zprefab` | `ZPFB` | SIMULATION | FAIL | yes | no |
| 7 | `ASSET_KIND_SKELETON` | `Animation.Skeleton` | `Skeleton` | `Zenith_SkeletonAsset` | ANIM | `.zskel` | `ZSKL` | SIMULATION | FAIL | yes | no |
| 8 | `ASSET_KIND_ANIMATION_CLIP` | `Animation.Clip` | `AnimationClip` | `Zenith_AnimationClip` | ANIM | `.zanim` | `ZANM` | SIMULATION | NULL_OBJECT | yes | no |
| 9 | `ASSET_KIND_ANIMATION_GRAPH` | `Animation.Graph` | `AnimationGraph` | `Zenith_AnimationGraphAsset` | ANIM | `.zagraph` | `ZAGR` | SIMULATION | FAIL | yes | no |
| 10 | `ASSET_KIND_SOUND` | `Audio.Sound` | `Sound` | `Zenith_SoundAsset` | AUDIO | `.zsnd` | `ZSND` | AUDIO | NULL_OBJECT | yes | no |
| 11 | `ASSET_KIND_SOUND_BANK` | `Audio.Bank` | `SoundBank` | `Zenith_SoundBankAsset` | AUDIO | `.zbank` | `ZBNK` | AUDIO | FAIL | yes | no |
| 12 | `ASSET_KIND_FONT` | `UI.Font` | `Font` | `Zenith_FontAsset` | UI | `.zfont` | `ZFNT` | ASSETS | SUBSTITUTE | yes | no |
| 13 | `ASSET_KIND_UI_DOCUMENT` | `UI.Document` | `UIDocument` | `Zenith_UIDocumentAsset` | UI | `.zui` | `ZUID` | ASSETS | FAIL | yes | no |
| 14 | `ASSET_KIND_CINEMATIC` | `Cinematics.Sequence` | `Cinematic` | `Zenith_Cinematic` | CINE | `.zcine` | `ZCIN` | ASSETS | FAIL | no | no |
| 15 | `ASSET_KIND_NAV_MESH` | `AI.NavMesh` | `NavMesh` | `Zenith_NavMeshAsset` | AI | `.znav` | `ZNAV` | SIMULATION | FAIL | no | no |
| 16 | `ASSET_KIND_COLLISION_SHAPE` | `Physics.CollisionShape` | `CollisionShape` | `Zenith_CollisionShapeAsset` | PHYS | `.zcol` | `ZCOL` | SIMULATION | FAIL | yes | no |
| 17 | `ASSET_KIND_COMPRESSION_DICTIONARY` | `Zenith.CompressionDictionary` | `CompressionDictionary` | `Zenith_CompressionDictionaryAsset` | PACK (format, training); ASSET (built-in loader) | `.zdict` | `ZDCT` | ASSETS | FAIL | yes | no |
| 18 | `ASSET_KIND_VIRTUAL_TEXTURE` | `Flux.VirtualTexture` | `VirtualTexture` | `Flux_VirtualTextureResource` | ADVGFX | `.zvt` | `ZVTX` | RENDERING | SUBSTITUTE | no | yes |
| 19 | `ASSET_KIND_PARTICLE_EFFECT` | `VFX.Effect` | `ParticleEffect` | `Zenith_ParticleEffectAsset` | VFX | `.zvfx` | `ZVFX` | RENDERING | NULL_OBJECT | yes | no |
| 20 | `ASSET_KIND_LOCALIZATION_TABLE` | `UI.LocalizationTable` | `LocalizationTable` | `Zenith_LocalizationTableAsset` | UI | `.zloc` | `ZLOC` | ASSETS | FAIL | yes | no |
| 21 | `ASSET_KIND_VIDEO` | `Cinematics.Video` | `Video` | `Zenith_VideoAsset` | CINE | `.zvid` | `ZVID` | ASSETS | NULL_OBJECT | no | no |
| 22 | `ASSET_KIND_DATA` | `Zenith.Data` | `Data` | `Zenith_DataAsset` | ASSET (built-in loader) | `.zdata` | `ZDAT` | ASSETS | FAIL | yes | no |
| 23 | `ASSET_KIND_TERRAIN_TILE` | `Environment.TerrainTile` | `TerrainTile` | `Zenith_TerrainTileAsset` | ENV | `.zenvtile` | `ZETL` | SIMULATION | FAIL | yes | no |
| 24 | `ASSET_KIND_FOLIAGE_CELL` | `Environment.FoliageCell` | `FoliageCell` | `Zenith_FoliageCellAsset` | ENV | `.zfoliage` | `ZFOL` | STREAMING | NULL_OBJECT | yes | yes |
| 25 | `ASSET_KIND_ATMOSPHERE` | `Environment.Atmosphere` | `Atmosphere` | `Zenith_AtmosphereAsset` | ENV | `.zatmos` | `ZATM` | ASSETS | SUBSTITUTE | yes | no |
| 26 | `ASSET_KIND_WEATHER` | `Environment.Weather` | `Weather` | `Zenith_WeatherAsset` | ENV | `.zweather` | `ZWTR` | ASSETS | FAIL | yes | no |
| 27 | `ASSET_KIND_WATER` | `Environment.Water` | `Water` | `Zenith_WaterAsset` | ENV | `.zwater` | `ZWAT` | ASSETS | FAIL | yes | no |
| 28 | `ASSET_KIND_ENVIRONMENT_MAP` | `Flux.Environment` | `EnvironmentMap` | `Flux_EnvironmentMapResource` | LIGHT | `.zenv` | `ZENV` | RENDERING | SUBSTITUTE | yes | yes |
| 29 | `ASSET_KIND_REFLECTION_PROBE` | `Flux.Probe` | `ReflectionProbe` | `Flux_ReflectionProbeResource` | LIGHT | `.zprb` | `ZPRB` | RENDERING | NULL_OBJECT | yes | yes |
| 30 | `ASSET_KIND_IRRADIANCE_VOLUME` | `Flux.IrradianceVolume` | `IrradianceVolume` | `Flux_IrradianceVolumeResource` | LIGHT | `.zgiv` | `ZGIV` | RENDERING | NULL_OBJECT | no | yes |
| 31 | `ASSET_KIND_LIGHTMAP_SET` | `Flux.LightmapSet` | `LightmapSet` | `Flux_LightmapSetResource` | LIGHT | `.zlmp` | `ZLMP` | RENDERING | NULL_OBJECT | no | yes |
| 32 | `ASSET_KIND_GRADING_LUT` | `Flux.GradingLUT` | `GradingLUT` | `Flux_GradingLUTResource` | LIGHT | `.zlut` | `ZLUT` | RENDERING | NULL_OBJECT | yes | yes |
| 33 | `ASSET_KIND_LIGHTING_LUT` | `Flux.LightingLUT` | `LightingLUT` | `Flux_LightingLUTResource` | LIGHT | `.zllt` | `ZLLT` | RENDERING | FAIL | yes | yes |
| 0x7F00–0x7F0F | none (test kinds) | `Test.Kind<n>` | `TestKind<n>` | `Zenith_FakeAsset<n>` (test support) | ASSET tests | `.ztest` | `ZTST` | declared | declared | declared | declared |

Requested kind rows (code reserved by ASSET; the enumerator, the `KindTable.json` row and the count increments land in the owner's API change, after which ASSET moves the row into the table above):

| Code | Enumerator | Stable name | Script name | Runtime object type | Owner | Cooked extension | Magic | Memory subsystem | Default fallback policy | Cacheable | GPU resident |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 34 | `ASSET_KIND_CLOUD_NOISE` | `Environment.CloudNoise` | `CloudNoise` | `Flux_CloudNoiseResource` | ENV | `.zcloudnoise` | `ZCLN` | RENDERING | NULL_OBJECT | yes | yes |
| 35 | `ASSET_KIND_STREAMING_WORLD` | `Streaming.World` | `StreamingWorld` | `Zenith_StreamingWorldAsset` | STREAM | `.zstream` | `ZSTW` | STREAMING | FAIL | no | no |
| 36 | `ASSET_KIND_PHYSICS_MATERIAL` | `Physics.Material` | `PhysicsMaterial` | `Zenith_PhysicsMaterialAsset` | PHYS | `.zpmat` | `ZPMT` | SIMULATION | SUBSTITUTE | yes | no |
| 37 | `ASSET_KIND_PHYSICS_RAGDOLL` | `Physics.Ragdoll` | `Ragdoll` | `Zenith_RagdollAsset` | PHYS | `.zragdoll` | `ZRGD` | SIMULATION | FAIL | yes | no |

The reserved codes are taken in the landing order the kind-adding changes fix between them, each blocked by the previous one, so the codes stay dense whichever sprint each change sits in: 34 by ENV's code-side slice, then 35 by STREAM's kind change, then 36 and 37 together by PHYS's kind change, in that order inside its one diff. A further kind takes code 38 and blocks on PHYS's change. Code 35's row is STREAM's (cross-area row ST6) and codes 36 and 37 are PHYS's (row PH4); kind 36 is SUBSTITUTE, so PHYS's material cook step declares its role-0 engine fallback, the default physical material (Cook.md "Engine fallbacks").

- Code 0 (`ASSET_KIND_NONE`) never names an asset. Defined codes are dense: the next kind takes code `uASSET_KIND_DEFINED_COUNT` (34 now), and no code at or above `uASSET_KIND_CODE_CAP` (256) is ever defined; codes 256 … 0x7EFF are never defined, 0x7F00 … 0x7F0F are test kinds and 0x7F10 … 0xFFFF invalid. A code, once listed, keeps its meaning forever.
- This table, like every part of `Zenith2/Docs/Design/ASSET.md`, is edited only by ASSET. An owner asks for a new kind or a changed row (a runtime type name, a residency column) through a cross-area note in `findings/A3_cross_area_notes.md`, and ASSET applies it in its own design diff: a changed row here, a new kind in "Requested kind rows" below with its code reserved. The owner's API change of ZenithAssets (ASSET reviewing) then adds the enumerator and the `Tests/Fixtures/KindTable.json` row exactly as the design row states, increments `uASSET_KIND_DEFINED_COUNT`, and increments `uZENITH_ENGINE_COOK_VERSION`, because a catalogue's kind count must equal the build's and every catalogue re-cooks (RuntimeAPI.md "Asset kinds and the kind table"); ASSET moves the row into the table above in its next design diff. Applied requests: kind 8's runtime type is `Zenith_AnimationClip` (ANIM AN3, matching `Zenith2/Docs/Architecture.md`'s assumed-type table); kind 19 is CPU-resident (VFX VX5: the particle system creates its constants buffer on first use, so headless runs load effects through the real loader); kind 34 is reserved for ENV's cloud noise (ENV EnB3 (d)); kind 35 for STREAM's streaming world index (STREAM ST6); kinds 36 and 37 for PHYS's physical material and ragdoll definition (PHYS PH4, with kind 16's runtime type `Zenith_CollisionShapeAsset`); kind 14's runtime type is `Zenith_Cinematic` (CINE CN1 (a): `Zenith_AssetHandle<Zenith_Cinematic>` as `Zenith2/Docs/Architecture.md`'s assumed types use it; CINE's header keeps the name `Zenith_CinematicAsset.h`), and kind 21 stays `Zenith_VideoAsset`.
- "Runtime object type" is the `TAsset` of `Zenith_AssetHandle<TAsset>`: the owner's class, which declares `static constexpr Zenith_AssetKind eASSET_KIND` with this row's enumerator (cross-area rows AS25, AS26: each owner adopts the name in this column, and a different name is requested from ASSET, which edits this table). "GPU resident" is `Zenith_AssetKindInfo::m_bGpuResident`: yes means the kind's loader creates GPU resources, the kind is served headless by the validate-only loader, reloaded by `ReloadGpuResidencySet`, asked to `DropCpuCopies` at CRITICAL, and tracked through the GPU residency sub-state (Loading.md "GPU residency sub-state").
- Codes 23 … 27 are ENV's kinds as `Zenith2/Docs/Design/ENV/Bake.md → "Cooked formats"` proposes them, and codes 28 … 33 LIGHT's (`Zenith2/Docs/Design/LIGHT.md → "Format index"`); the owners' designs specify the formats and loaders. The lighting LUT kind's extension `.zllt` is ASSET's assignment for the `ZLLT` magic LIGHT names.
- `ASSET_KIND_DATA` is ASSET's generic kind: verbatim bytes in one section, loaded into a CPU buffer by ASSET's own built-in loader (the `raw` importer's output); it backs test content and any game data file an owner declares as opaque. `ASSET_KIND_COMPRESSION_DICTIONARY` is also loaded by a built-in loader, because PACK, which trains and writes dictionaries, has no engine library (RuntimeAPI.md "Zenith_BuiltInAssets header").
- The owner of a kind may change its default fallback policy only in its own design document, by a diff that edits this table; the column is the policy (no loader overrides it). A SUBSTITUTE kind's cook step declares its role-0 engine fallback (Cook.md "Engine fallbacks").
- Test kinds are accepted by a registry only when `Zenith_AssetRegistryDesc::m_bAllowTestKinds` is set, which no Player or cook path sets and which `Create` refuses with `NOT_ALLOWED` in a Shipping build (`Zenith_IsShippingBuild()`) or with `m_bDevelopmentFeatures` false (RuntimeAPI.md "Zenith_AssetRegistry"); the cook refuses a test kind in any game's content (`COOK_ERROR_TEST_KIND_IN_CONTENT`). The "declared" cells of the test row are the columns of the `Zenith_AssetKindInfo` a test declares for each test kind it uses in `Zenith_AssetRegistryDesc::m_axTestKinds`; the registry reads every column through `GetKindInfo`, `Zenith_AssetKindSet` carries test codes in its own 16-bit member, and a test catalogue carries their format versions in a test-kind table behind a header flag (RuntimeAPI.md "Asset kinds and the kind table"; Formats.md "Kind table"). The standard declarations every runtime test starts from are `Zenith_FakeKindLoader::GetStandardTestKinds()` (Tests.md "Fixtures").

### Zenith_AssetTypes header

The full declarations are in `Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetTypes header"`. The top-level contracts:

| Type | Contract in one line |
|---|---|
| `Zenith_AssetGUID` | an alias of `Zenith_PersistentID` ([Z2-N-CONTRACT-066]): assets are named by persistent IDs, and the alias only names the role |
| `Zenith_AssetPriority` | `ASSET_PRIORITY_CRITICAL` … `ASSET_PRIORITY_BACKGROUND`, mapping one-to-one onto `IO_PRIORITY_CRITICAL` … `IO_PRIORITY_BACKGROUND` |
| `Zenith_AssetState` | `UNLOADED`, `QUEUED`, `LOADING`, `ACTIVE`, `FAILED`, `UNLOADING`, `CACHED`; the live state, for presentation and tools, never for simulation |
| `Zenith_AssetError` | a 24-byte error record: `m_eCode`, a 16-bit detail (the I/O, compression, corruption or loader code), the section index, and the asset GUID |
| `uZENITH_ENGINE_COOK_VERSION` | the engine cook version every cooked header records; bumped by any change that alters any cooked byte for unchanged inputs |

### Handle kinds used

| Handle | Kind (`Zenith2/Docs/Contracts.md` → "Handle kinds") | ASSET's role |
|---|---|---|
| `Zenith_AssetHandle<T>`, `Zenith_AnyAssetHandle` | shared asset handle | declares both, owns the registry that counts them |
| `Zenith_AssetGUID` (`Zenith_PersistentID`) | persistent ID | assigns asset and sub-asset GUIDs; serializes handles as GUIDs |
| `Zenith_OperationHandle` | operation handle | issues one per load request and per reader request from its own operation tables |
| `Flux_TextureHandle` and the other device handles | device handle | never held by ZenithAssets; kind loaders hold them inside their runtime objects |

### Public API in the split files

| Header | Heading in `Zenith2/Docs/Design/ASSET/RuntimeAPI.md` |
|---|---|
| `Zenith_AssetTypes.h` | "Zenith_AssetTypes header" |
| `Zenith_AssetHandle.h` | "Zenith_AssetHandle header" |
| `Zenith_AssetRegistry.h` | "Zenith_AssetRegistry header" |
| `Zenith_AssetKindLoader.h` | "Zenith_AssetKindLoader header" |
| `Zenith_AssetLoader.h` | "Zenith_AssetLoader header" |
| `Zenith_AssetReader.h` | "Zenith_AssetReader header" |
| `Zenith_AssetCatalogue.h` | "Zenith_AssetCatalogue header" |
| `Zenith_CookedFile.h` | "Zenith_CookedFile header" |
| `Zenith_AssetLoadResult.h` | `Zenith2/Docs/Design/ASSET/Formats.md → "Load result payload"` |
| `Zenith_AssetArchive.h` | "Zenith_AssetArchive header" |
| `Zenith_BuiltInAssets.h` | "Zenith_BuiltInAssets header" |
| tool-side: database | `Zenith2/Docs/Design/ASSET/Database.md → "The asset database"` |
| tool-side: import | `Zenith2/Docs/Design/ASSET/Import.md → "Importer interface"` |
| tool-side: cook | `Zenith2/Docs/Design/ASSET/Cook.md → "Cook steps"` |
| tool-side: DDC | `Zenith2/Docs/Design/ASSET/DDC.md → "Zenith_DerivedDataCache API"` |
| tool-side: the `.meta` document type | `Zenith2/Docs/Design/ASSET/Migration.md → "The meta document type"` |

## Data structures and formats

### Format index

| Format | Kind | Specified in |
|---|---|---|
| cooked asset file (the common header and section table) | cooked binary, versioned | Formats.md "Cooked asset file" |
| asset catalogue `assets.zcat` and fragments | cooked binary, versioned | Formats.md "Asset catalogue file" |
| `.meta` importer-settings sidecar | authoring document (JSON), migrated | Formats.md "Meta sidecar schema" |
| asset database `assetdb.zdb` | tool cache (binary, versioned, rebuildable) | Formats.md "Asset database file" |
| DDC entry `.zddc` | tool cache (binary, versioned) | Formats.md "DDC entry file" |
| cook journal `.cook/journal.jsonl` | tool state (JSON lines) | Formats.md "Cook journal" |
| cook report `.cook/cook-report.json` | tool output (JSON) | Formats.md "Cook report" |
| provenance records `.cook/provenance.jsonl` | tool output (JSON lines) | Formats.md "Provenance record lines" |
| dependency file `cook.d` | Make depfile | Formats.md "Dependency file" |
| `.zpak` archive (read by ZenithAssets, written by PACK) | cooked binary container, versioned | Formats.md "Archive file" |
| tombstone list `.zenith-tombstones` | tracked tool state (text lines) | Formats.md "Tombstone list" |
| content-rule table `Zenith2/Docs/ContentSpecs.rules.json` | authoring data read by the cook | Formats.md "Content rule table" |
| cook server and worker messages | line-delimited JSON, protocol version 1 | Formats.md "Cook server messages", "Worker messages" |
| load result payload | 24-byte binary record in replay publications | Formats.md "Load result payload" |
| framework journal `<batch>.z2journal` of `.meta` batches | SCENE's journal format | `Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Journal format"` |

### In-memory layouts that matter

| Datum | Size | Rule |
|---|---|---|
| `Zenith_AssetHandle<T>`, `Zenith_AnyAssetHandle` | 16 bytes | `{ Zenith_AssetRegistry*; uint32 slot; uint32 generation }`, Contracts' layout; `static_assert` |
| registry slot header | 64 bytes, 64-byte aligned | one cache line per asset: reference count, generation, state, kind, GUID, runtime object pointer, load record index (RuntimeAPI.md "Slot header layout") |
| `Zenith_AssetError` | 24 bytes | trivially copyable ([Z2-N-CONTRACT-480]) |
| load record | 256 bytes, pooled (`asset.load.maxInFlight` of them) | per admitted pipeline; queued pipelines hold none (RuntimeAPI.md "Internal types and files") |
| slot side record | 64 bytes per slot | the queued pipeline, its requests and dependency blocks, the cache link (RuntimeAPI.md "Slot side records") |
| section record | 96 bytes, pooled | per section in flight |
| cooked header | 64 bytes + 40 per section | Formats.md "Header layout" |
| catalogue entry | 120 bytes | Formats.md "Entry table" |
| `Zenith_AssetKindSet` | 40 bytes | one bit per code below `uASSET_KIND_CODE_CAP`, one per test code, padding |
| archive entry | 40 bytes, plus its path in the string table | Formats.md "Archive entry table" |

### Cooked output tree

The cook writes one tree per game and platform, `${ZENITH_OUT}/Cooked/<Name>/<platform>/`, which is the loose mount of development builds and PACK's packaging input. ASSET owns the layout; each row's owner owns the files' formats:

| Path inside the tree (as `content:/…`) | Content | Format owner |
|---|---|---|
| `mount-manifest.json` | the development mount manifest | PLAT (schema), ASSET (writes it) |
| `assets.zcat` | the base asset catalogue | ASSET |
| `Catalogues/<name>.zcat` | catalogue fragments (asset packs, patches) | ASSET (format), PACK (partition) |
| `Objects/<xx>/<guid>.<ext>` | every cooked asset; `<xx>` is the first two hexadecimal digits of the GUID's text form, `<guid>` its 36-character text form, `<ext>` the kind's extension | ASSET (container), the kind owner (sections) |
| `Config/Platform/<platform>.config.json`, `Config/game.config.json`, `Config/Quality/<tier>.config.json`, `Engine/Config/Quality/<tier>.config.json`, `Engine/Config/Quality/tiers.<platform>.json` | merged and copied configuration | PLAT |
| `Engine/Input/Engine.bindings.json` | the engine input bindings | PLAT |
| `Scripts/…` | `.zlbc` modules and the module manifest | SCRIPT |
| `schemas.zreg` | the schema registry blob | ECS |
| `Shaders/…`, `pso_precache.zbin` | shader blob containers and the precache set | RPROD |
| `.cook/` | the report, provenance records, the journal, the validation report; never mounted content (PACK excludes `.`-prefixed directories) | ASSET |
| `cook.d`, `.stamp` | the depfile and the stamp of `<Name>_Content` | ASSET (MOD names them, `Zenith2/Docs/Design/MOD/GameBuild.md → "Content and type targets"`) |

- No path in the tree is derived from wall-clock time, host name, user name or worker count, and no two cook steps may claim the same path (a collision is `COOK_ERROR_OUTPUT_COLLISION`).
- A cooked asset's file name depends on its GUID and kind alone, so a renamed source changes only the catalogue's logical path.

### Content root mapping

Each file under a content root of the game (`content.roots` of `game.json`) or under an engine library's content root gets one logical path, the `content:/` text the catalogue indexes and `content.startupScene` names:

| Source location | Logical path |
|---|---|
| `Games/<Name>/Assets/<p>` | `content:/<p>` |
| `Games/<Name>/<R>/<p>` for any other content root `R` | `content:/<R>/<p>` |
| `Zenith2/Engine/<Lib>/Internal/Content/<p>` of a library in the game's link set | `content:/Engine/<p>` |
| a sub-asset `<key>` of a source with logical path `L` | `L#<key>` |
| `Zenith2/Engine/ZenithAssets/Internal/Content/ImportPresets/<p>` (CONTENT's subdirectory of ZenithAssets' engine content root, cross-area row CT3 (d)) | `content:/Engine/ImportPresets/<p>`, like any engine content |

- `thirdparty:<entry>/<p>` is a read-only **input root**, not a content root (cross-area rows UI9, ShB7, UiB6): `<entry>` is a `ThirdParty/manifest.json` entry of integration mode `data` (SHIP's, `Zenith2/Docs/Design/SHIP/Manifest.md → "Integration modes in detail"`), a lower-case kebab name, and `<p>` a relative path obeying the virtual-path rules R1–R10. It is never scanned, holds no asset source and no `.meta`, and has no logical path; ZenithCook resolves it to `ZENITH_TP_<ID>_DIR/<p>` of every `data` entry ZenithCook names in its `THIRD_PARTY` list, and only importers (`ReadInput`) and steps (`ReadThirdPartyFile`) read it, each read keyed by the path text and the bytes' hash, never the host directory (Cook.md "Repository and third-party files"). A test outside ZenithCook reads the same files through SHIP's `ZENITH_TP_<ID>_DIR` definition of its own `TEST_THIRD_PARTY` entry, `ZENITH_TP_<ID>_DIR "/" <p>` being the meaning of `thirdparty:<entry>/<p>` everywhere; no runtime target reads it (runtime reads cooked output).
- Two sources mapping to one logical path (for example `Assets/Scenes/A.scene.json` and a content root `Scenes` holding `A.scene.json`, or two libraries' `Internal/Content/Textures/White.png`) are `COOK_ERROR_LOGICAL_PATH_COLLISION`, naming both sources.
- A logical path is a catalogue key, not a virtual path: sub-asset keys may contain `:`, which virtual paths forbid. A logical path's source part obeys the virtual-path rules R1–R10 of `Zenith2/Docs/Design/PLAT/Filesystem.md → "Validation rules and order"`, and a source whose mapped path fails them is `COOK_ERROR_BAD_SOURCE_NAME`.
- Files under `Tests`, `Workload`, `Acceptance` and `.migration` are never content ([Z2-N-ARCH-246]): a directory with one of these names is skipped at every depth of every content root, so `Scripts/Tests/` (SCRIPT's behaviour-state migration fixtures under `Scripts/Tests/Migration/`, core-doc item X194) is never enumerated by ZenithCook while `Scripts/Testing/` is (Database.md "Scan algorithm" step 1); neither is any `.`-prefixed name inside a content root, such as the root's tracked tombstone list `.zenith-tombstones` (Formats.md "Tombstone list").

<!-- assumption: pending MOD, GEOM -->
MOD's mapping of `Assets/<p>` and of other roots is adopted verbatim (`Zenith2/Docs/Design/MOD.md → "Startup scene and loose content root"`). Engine content lives under `Engine/<Lib>/Internal/Content/`, as PLAT's engine bindings do, because a library root holds only headers and `CMakeLists.txt` ([Z2-N-CONTRACT-021]); GEOM's `Zenith2/Engine/ZenithFlux/Content/` moves to `ZenithFlux/Internal/Content/`.
<!-- /assumption -->

## Algorithms and state machines

### Algorithm index

| Algorithm | Specified in |
|---|---|
| handle copy, destroy, resolution; zero-reference retirement | RuntimeAPI.md "Reference counting and retirement" |
| catalogue load, fragment merge with external references, fallback table, mount-change refresh | RuntimeAPI.md "Zenith_AssetCatalogue header", "GUID index and catalogue snapshots"; Formats.md "Catalogue fragments"; Loading.md "Retry after a mount change" |
| archive open, checks, lookup, listing, reads | RuntimeAPI.md "Zenith_AssetArchive header"; Formats.md "Archive checks" |
| boot order and GPU activation before upload | Loading.md "Tool and boot servicing", "GPU residency sub-state" |
| reference-default resolution | RuntimeAPI.md "Reflection value hooks"; Database.md "Reference defaults in the cook" |
| load pipeline: catalogue → dependencies → header → plan → reads → decode → delivery → activation | Loading.md "Pipeline stages" |
| decompression back-pressure | Loading.md "Decompression back-pressure" |
| cancellation at every stage, completion reporting | Loading.md "Cancellation" |
| residency accounting, keep-alive cache, pressure responses | Loading.md "Residency budgets and eviction" |
| missing and corrupt classification, fallback | Loading.md "Missing and corrupt assets" |
| device-loss reload | Loading.md "Device-loss reload" |
| scan and reconcile, rename detection, duplicate GUIDs | Database.md "Scan and reconcile" |
| dependency graph maintenance and dirty propagation | Database.md "Dependency graph"; Cook.md "Dependency-driven invalidation" |
| settings resolution, import, reimport | Import.md "Settings resolution", "Import algorithm", "Reimport" |
| cook planning, keying, execution, validation, publication | Cook.md "Planning", "Incremental cook", "Execution", "Validation" |
| DDC key derivation, lookup, populate, verify, eviction | DDC.md |
| `.meta` migration on SCENE's framework: open, importer-settings visitor, plan, apply, save, recovery | Migration.md "Importer settings inside meta documents", "Reading and writing meta documents through the framework", "Recovery before a cook" |
| engine fallbacks in the plan and the catalogue | Cook.md "Engine fallbacks" |
| cook server session and requests | Formats.md "Session rules", "Requests"; Cook.md "The cook server" |

### State machine index

| State machine | Specified in |
|---|---|
| asset slot states (`Zenith_AssetState`), every state × event | Loading.md "Asset state machine" |
| reload and device-recovery pipelines | Loading.md "Reload and recovery pipelines" |
| GPU residency (`Zenith_AssetGpuResidency`) | Loading.md "GPU residency sub-state" |
| cook server session | Formats.md "Session rules" |
| load record stages | Loading.md "Load record stages" |
| section record stages | Loading.md "Section read", "Section decode" |
| database record states | Database.md "Records" |
| cook item states and the journal | Cook.md "Interrupted cook and resume" |
| DDC entry verification | DDC.md "Lookup populate and verify" |
| `.meta` framework journal | `Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Journaled write"` |

### Frame-level flow

<!-- elaboration -->
```text
any thread:   RequestLoad ──► request queue (lock-free) ──► kick bg/Asset.Service
bg/Asset.Service slice (≤ 2 ms): drain requests, I/O completions, decode completions, loader acknowledgements
                                  → advance queued pipelines and load records → admit, issue reads within back-pressure
                                  → submit bg/Asset.Decode slices → call kind loaders
bg/Asset.Decode slice (≤ 2 ms):  verify section check → decompress (one-shot or streamed) → mark done → kick service
kind loader (any thread):        CPU object built, GPU uploads requested into the owner's pending list → Activate() → kick service
owner's render-graph node:        allocate, RequestUpload (GEOM: Flux.BuildGraph); the copy timeline passes the upload
                                  → ReportGpuResident(ticket); a full pool or a failed upload → ReportGpuFailed(ticket, code)
epilogue node Assets.Service:    AdvanceFrame(k); poll I/O completions; kick the service if work is pending (≤ asset.service.frameBudgetUs)
requester at its tick boundary:  drain its completion queue → publish in completion-ID order (Z2-D-053)
```

## Threading lifetime and error contract

### Thread affinity per type

| Type | Affinity |
|---|---|
| `Zenith_AnyAssetHandle`, `Zenith_AssetHandle<T>` | `ANY_THREAD` for copy, move, destroy, `IsNull`, `GetPersistentID`, `GetKind`; a handle object itself is used by one thread at a time like any value |
| `Zenith_AssetRegistry` | `ANY_THREAD_EXCEPT(Create, RegisterKindLoader, CloseRegistration, GetPinnedLoads, UnregisterKindLoader, OnMountsChanged, BeginShutdown, destructor)`: those are `MAIN_THREAD`; `Acquire*`, `Resolve*`, `GetState`, `GetFailure`, `GetGpuResidency`, `GetKindInfo`, `GetGpuResidentKinds`, `GetPinnedAsset`, `ReportGpuResident`, `ReportGpuFailed` are `ANY_THREAD` |
| the archive mount source | `ANY_THREAD` as PLAT's `Zenith_MountSource`; the factory `MAIN_THREAD` (startup step 5 or the frame prologue) |
| `Zenith_ResolveAssetReferenceDefault` | `MAIN_THREAD`, called by the type registry at startup step 11 or in `EndHotReload` |
| `Zenith_AssetLoader` | `ANY_THREAD` for `RequestLoad`, `SetPriority`, `GetError`, `GetStats`; `ServiceFrame` from the `Assets.Service` node only; `ServiceUntilComplete` `MAIN_THREAD` outside any frame iteration, or a tool or test thread that is not a job worker |
| `Zenith_AssetLoadCompletionQueue` | pushes `ANY_THREAD`; `Drain` `OWNER_THREAD` |
| `Zenith_AssetReader` | `ANY_THREAD` |
| `Zenith_AssetCatalogue` | `IMMUTABLE` after `Load` |
| `Zenith_AssetKindLoader` (implementations) | called by ZenithAssets on background job threads, one call at a time per asset; the implementation's other callers are its own business |
| `Zenith_AssetLoadContext` | `ANY_THREAD` for its one-shot terminal calls (`Activate`, `Fail`, `AcknowledgeCancel`); `RequestSections` only inside `BeginLoad` or `OnSectionsLoaded` |
| `Zenith_CookedFileWriter` | `OWNER_THREAD` |
| tool-side types | `Zenith_AssetDatabase` `OWNER_THREAD` (the cook's main thread); `Zenith_DerivedDataCache` `ANY_THREAD`; `Zenith_CookContext` `OWNER_THREAD` per call (the thread of the item's `AddKeyInputs`, then of its `Cook`; `OpenSourceDocument` needs no other thread, Cook.md "Source documents in cook steps") |

### Frame-graph placement

| Work | Where | Declared access |
|---|---|---|
| `Assets.Service` | `FRAME_SUBGRAPH_EPILOGUE`, engine-wide group 0 (assumption in "Assumed interfaces pending other areas") | writes the resource `Zenith_AssetRegistry`; reads nothing of any world |
| the service slices `bg/Asset.Service` | background operations, `JOB_PRIORITY_BACKGROUND_HIGH` while any CRITICAL or HIGH load is pending, else `JOB_PRIORITY_BACKGROUND_LOW` | no world access ([Z2-N-ARCH-393]) |
| decode slices `bg/Asset.Decode` | background operations at the priority of their load | none |
| kind-loader calls | inside the service or decode slice that makes them; heavy loader work is the loader's own background or render-graph work | none of ZenithAssets' |
| load publication to simulation | the requester's tick-boundary step (SCRIPT, STREAM, SCENE) | the requester's |

No ZenithAssets code runs in a simulation phase node, and no simulation node reads the live registry state (Loading.md "Publication to simulation").

### Lock-order entries

ZenithAssets uses the two rows `Zenith2/Docs/Contracts.md → "Threading and concurrency contract"` (table "Lock ordering table") already assigns it, and adds none:

| Level | Constant | Guards in ASSET | Held while |
|---|---|---|---|
| 200 | `uLOCK_LEVEL_ASSET_REGISTRY` | the free list, the GUID-to-slot index, slot allocation and freeing, the retire decision, the catalogue snapshot pointer, kind-loader registration, the missing-reference log set | never across an I/O call, a kind-loader call, a job submission or a governor call |
| 250 | `uLOCK_LEVEL_STREAMING_REQUESTS` | the request table (operation slot → load record and its priority), used by `SetPriority` and cancellation lookups | never across anything but table reads and writes |

- A thread may take 250 while holding 200 (ascending levels) and never the reverse; the service takes them one at a time.
- Neither lock is held while calling a `Zenith_AssetKindLoader` member, `Zenith_AsyncIO`, `Zenith_JobSystem` or `Zenith_MemoryGovernor`; both are short-hold rows, so a job may take them ([Z2-N-CONTRACT-316]).
- Handle copy and destroy take no lock (atomic reference counts on the slot header); `ReportGpuResident` and `ReportGpuFailed` take no lock (one compare-exchange of the slot's 64-bit residency word, which holds the installed and current attempt values, the open flag, the residency and the open attempt's early outcome together, RuntimeAPI.md "Slot header layout").
- Deviation from the Contracts row, stated explicitly: `Zenith2/Docs/Contracts.md → "Threading and concurrency contract"` describes level 200's guards as "slot table growth, persistent-ID map, zero-reference retire list"; ZenithAssets' slot array never grows (it is reserved once at `Create`), so level 200 guards the items in the table above instead. The level number and the short-hold flag are unchanged; the Guards wording is core-doc item X114 (c) (pending).
- The archive reader takes no lock: its entry table is immutable after creation, and each per-request file is used by one thread at a time as PLAT's contract requires.

### Threads and background operations

ZenithAssets starts no thread. Its work runs on the job system's workers as background operations and on `Zenith_AsyncIO`'s I/O threads (PLAT's). At most one `bg/Asset.Service` operation is in flight at a time (an atomic "service scheduled" flag), so the service's state needs no lock; decode slices run concurrently, at most `asset.decode.concurrency` at once.

### Construction and shutdown

| Step | Owner | ASSET action |
|---|---|---|
| startup step 5 | ZenithRuntime, PLAT | the VFS desc carries `&Zenith_CreateArchiveMountSource` and a `Zenith_ArchiveReaderDesc`; PLAT creates every package and patch archive source through it — pending PLAT (X154): a new ask of PLAT's filesystem design and of the startup table (cross-area row AS22) |
| startup step 6 | ZenithRuntime | `Zenith_RegisterAssetsConfigKeys(xSchema, ePlatform)` with the engine libraries' keys in library-layer order, and `Zenith_RegisterAssetTypes(xTypes)` (one `TYPE_KIND_ASSET` type per kind) among the engine types before any module's types ([Z2-N-ARCH-847]) |
| startup step 10 | ZenithRuntime | `InstallValueKindHooks(PROPERTY_TYPE_ASSET, Zenith_GetAssetHandleValueKindHooks())`, before `RegisterSchemaBlob` |
| startup step 11 | ZenithRuntime | `Zenith_AssetRegistry::Create` after the codec set: allocate the slot array, the operation tables and queues, load and merge the catalogue, the consumer and the missing-reference set, register the built-in loaders (headless: the validate-only loader for every GPU-resident kind); then `ResolveReferenceDefaults(PROPERTY_TYPE_ASSET, &Zenith_ResolveAssetReferenceDefault, registry)` before any world; either failure is exit code 3 |
| startup step 11, after `Create` | ZenithScene | `Zenith_RegisterSceneAssetKinds` registers the scene and prefab loaders (`Zenith2/Docs/Design/SCENE.md → "Construction and shutdown"`) |
| steps 13 and 15 | Flux (13), modules (15) | `RegisterKindLoader` for their kinds; each registration pins the kind's fallback-table assets, requests them at CRITICAL and resumes the pipelines of its kind waiting for a loader (Loading.md "Tool and boot servicing") |
| after step 15, before step 17 | ZenithRuntime | `CloseRegistration`, which fails every pipeline still waiting for a loader with `ASSET_ERROR_NO_LOADER`; then `ServiceUntilComplete(GetPinnedLoads(), runtime.startupLoadTimeoutMs)`, a failure or timeout being exit code 3; a load of a kind without a loader from then on fails `NO_LOADER` |
| startup step 17 | ZenithRuntime, SCENE | the startup scene and its hard dependencies load at CRITICAL, GPU kinds activating once their uploads are requested (Loading.md "Tool and boot servicing") |
| shutdown step 4 | ZenithRuntime | `BeginShutdown`: refuse new requests (`ASSET_ERROR_SHUTTING_DOWN`), request cancellation of every load and read; the job system's drain runs the service until every operation has completed |
| shutdown step 6 | ZenithRuntime, modules, Flux, ZenithScene | before destroying a kind loader, its owner calls `UnregisterKindLoader(kind)` explicitly, which unloads every ACTIVE and CACHED asset of the kind and services until each is acknowledged (Loading.md "Shutdown sequence") |
| shutdown step 10 | ZenithRuntime | destroy the registry: report every slot still referenced ([Z2-N-CONTRACT-085]) and every pinned fallback released |

<!-- assumption: pending ARCH (X110, X147), MOD -->
The startup and shutdown rows above are core-doc items for `Zenith2/Docs/Architecture/CompositionRoot.md → "Startup sequence"` and `Zenith2/Docs/Contracts.md → "Threading and concurrency contract"` (table "Shutdown ownership order"): X110 (pending) for steps 6, 10, 11 (`Create`), 15, 17 and shutdown steps 4 and 10, X147 (4) (pending) for step 11's `ResolveReferenceDefaults`; cross-area rows AS5 and AS12 carry them for MOD's composition root. The step-5 row is X154 (open), fenced above.
<!-- /assumption -->

### Cancellation summary

| Cancelled while | Behaviour (Loading.md "Cancellation points by stage") |
|---|---|
| queued (catalogue lookup, waiting for dependencies, for its loader or for admission) | removed; dependency requests it made are cancelled and its dependency block released; nothing was read and no load record was held |
| reading (I/O) | every in-flight read is cancelled through its operation handle; the section's staging and decode unit are released after PLAT reports the read complete |
| decoding | the decode slice observes the flag at its next slice boundary (≤ 2 ms), releases its streamed-decode context, staging and unit |
| in the kind loader (building the object, requesting uploads) | `Zenith_AssetKindLoader::Cancel` is called; the loader withdraws its upload requests, releases every resource it created for the load and calls `AcknowledgeCancel`; uploads requested before `Activate` and running after it belong to the object and end with its `Unload` |
| activating | activation is atomic: a load whose loader has called `Activate` completes `SUCCEEDED` ([Z2-N-CONTRACT-337]) |

A request's operation reports `OPERATION_STATUS_CANCELLED` only after everything held for it has been released ([Z2-N-CONTRACT-335]).

### Error enumerations

| Enumeration | Declared in | Specified in |
|---|---|---|
| `Zenith_AssetErrorCode`, `Zenith_AssetCorruption`, record `Zenith_AssetError` | `ZenithAssets/Zenith_AssetTypes.h` | RuntimeAPI.md "Errors" |
| `Zenith_AssetDatabaseError` | `ZenithCook/Internal/Database/Zenith_AssetDatabase.h` | Database.md "Database errors" |
| `Zenith_ImportError` | `ZenithCook/Internal/Import/Zenith_Importer.h` | Import.md "Import errors" |
| `Zenith_CookErrorCode`, record `Zenith_CookError` | `ZenithCook/Internal/Cook/Zenith_Cook.h` | Cook.md "Cook errors" |
| `Zenith_DerivedDataError` | `ZenithCook/Internal/DDC/Zenith_DerivedDataCache.h` | DDC.md "DDC errors" |
| none for migration: `.meta` documents report SCENE's `Zenith_DocumentError`, mapped at the database boundary | — | Migration.md "Migration errors at the ASSET boundary" |

The operation failure code (`Zenith_OperationTicket::CompleteFailed`) of a load or read is its `Zenith_AssetErrorCode` value; the full record is read with `Zenith_AssetLoader::GetError` or `Zenith_AssetReader::GetError` ([Z2-N-CONTRACT-092]).

### Luau errors versus asserts

- ZenithAssets raises no Luau error itself. The Luau binding of `Zenith.Asset<TKind>` (SCRIPT's, `Zenith2/Docs/Scripting/Bindings.md`) turns a kind mismatch into its misuse error ZS1507; nothing script-supplied reaches an ASSET assert.
- Asserts guard programmer errors only: calling `RequestSections` outside the two loader callbacks, a second terminal call on a load context, `RegisterKindLoader` after `CloseRegistration` from the main thread (`ZENITH_CHECK`), destroying the registry with a load in flight (`ZENITH_CHECK`).
- File, catalogue, `.meta` and DDC bytes never reach an assert ([Z2-N-CONTRACT-472]).

## Diagnostics and profiling

### Counters

<!-- elaboration -->
Published through the profiler hooks every frame by `Assets.Service` ([Z2-N-CONTRACT-345] for the queue names). G is a gauge, C cumulative since the registry was created; no ASSET counter is a per-frame delta.

| Counter | Kind | Meaning |
|---|---|---|
| `asset.slots.live`, `asset.slots.high_water` | G | slots in use; high-water mark |
| `asset.slots.retired_exhausted` | C | slots retired at generation exhaustion |
| `asset.state.<state>` for each `Zenith_AssetState` | G | assets per state |
| `asset.loads.requested`, `.succeeded`, `.failed`, `.cancelled`, `.deduplicated` | C | request outcomes |
| `asset.loads.cancelled.queued`, `.header_read`, `.reading`, `.decoding`, `.deliver`, `.activating` | C | cancellations by the stage the pipeline was in when its cancellation took effect (Loading.md "Cancellation points by stage"); their sum equals `asset.loads.cancelled` (cross-area row ST20) |
| `asset.loads.in_flight`, `asset.loads.in_flight_bytes` | G | pipelines running; their declared file bytes |
| `asset.requests.queue.depth`, `.high_water`, `.full`, `.dropped` | G, G, C, C | the request intake queue (REJECT) |
| `asset.retire.queue.*`, `asset.ack.queue.*`, `asset.decode_done.queue.*`, `asset.io_done.queue.*` (`.depth`, `.high_water`, `.full`, `.dropped`) | G, G, C, C | the internal queues of Loading.md "Internal queues" |
| `<name>.depth`, `.high_water`, `.full`, `.dropped` of every requester completion queue | G, G, C, C | each `Zenith_AssetLoadCompletionQueue`, under the name it was created with |
| `asset.decode.queue.depth`, `.high_water`, `.full`, `.dropped` | G, G, C, C | decode units reserved (the row Z2-B-ASSET-002); issue attempts refused for want of a unit; `.dropped` always 0 |
| `asset.decode.slices`, `asset.decode.slice_overruns`, `asset.decode.bytes_in`, `asset.decode.bytes_out` | C | decode work |
| `asset.io.staging_bytes`, `asset.io.staging_high_water` | G | staging reserved |
| `asset.io.throttled` | C | service passes that left at least one ready section unissued for back-pressure |
| `asset.io.queue_full` | C | reads refused by `Zenith_AsyncIO` with `IO_ERROR_QUEUE_FULL` and retried |
| `asset.io.bytes_read`, `asset.io.reads` | C | reads issued by ZenithAssets |
| `asset.cache.bytes`, `asset.cache.entries` | G | the keep-alive cache |
| `asset.cache.hits`, `.evictions`, `.flushes` | C | cache behaviour; flushes by memory pressure |
| `asset.resident.<subsystem>.host_bytes`, `.device_bytes` | G | resident bytes reported by kind loaders, per memory subsystem |
| `asset.cpu_copies_dropped_bytes` | C | bytes released at CRITICAL |
| `asset.failures.<error>` for each `Zenith_AssetErrorCode` | C | failures by class |
| `asset.fallbacks.substituted` | C | resolutions that returned a fallback |
| `asset.missing` | C | unresolvable GUIDs acquired (the Contracts diagnostic), every one counted |
| `asset.missing.log_suppressed` | C | `asset.missing` records not logged because the 1,024-GUID log set was full |
| `asset.gpu.pending`, `asset.gpu.lost` | G | slots whose residency field is PENDING; LOST. Both follow every change of the field, whichever writer makes it: −1 for the old value, +1 for the new (RuntimeAPI.md "Slot header layout"), so a PENDING slot unloaded, swapped, recovered or lost moves each exactly once |
| `asset.gpu.failed` | C | accepted `ReportGpuFailed` calls, at most one per attempt, counted by the thread whose exchange accepted it (installed or early) |
| `asset.availability.epoch` | G | `GetAvailabilityEpoch()`: publications that can make a resolution newly available (RuntimeAPI.md "Zenith_AssetRegistry") |
| `asset.pipelines.queued`, `asset.loads.waiting_loader` | G | pipelines in the QUEUED sub-stages (loads, reloads, recoveries); those waiting in WAIT_LOADER |
| `asset.pinned.assets` | G | pinned fallback-table assets |
| `asset.reloads.deferred` | C | development reloads deferred by a refused double-residency reservation |
| `asset.archive.sources`, `asset.archive.entries` | G | mounted archive sources created by the reader and their entries |
| `asset.service.slice_us`, `asset.service.frame_us` | G | the last service slice; the epilogue node's time |
| `asset.critical_latency_us` | G | request-to-activation time of the latest CRITICAL load |
| `asset.reloads`, `asset.device_reloads` | C | development and device-loss reloads |

### On-screen profiler rows

The on-screen profiler (DEVT, Z2-D-005) shows these ASSET rows:

| Panel | Rows |
|---|---|
| frame | `Assets.Service` node time; background time spent in `bg/Asset.Service` and `bg/Asset.Decode` slices this frame |
| streaming | loads in flight and their bytes; decode queue depth against its row with the high-water mark; staging bytes against the staging budget; throttle and I/O-queue-full rates; critical-load latency |
| memory | `MEMORY_SUBSYSTEM_ASSETS` resident bytes against Z2-B-ASSET-001; keep-alive cache bytes and entries; per-subsystem resident bytes reported by kind loaders |
| assets | counts per state; failures by class in the last 600 frames with the last 16 failures (GUID, logical path, error) from the registry dump |

### Log categories

| Category | Constant | Content |
|---|---|---|
| `asset.registry` | `xLOG_CATEGORY_ASSET_REGISTRY` | creation, catalogue loads and merges, registration, shutdown reports |
| `asset.load` | `xLOG_CATEGORY_ASSET_LOAD` | failures (one `LOG_LEVEL_ERROR` record per asset per failure class, with GUID, logical path, stage, error, detail), fallbacks, reloads |
| `asset.missing` | `xLOG_CATEGORY_ASSET_MISSING` | the unresolvable-reference diagnostic of [Z2-N-CONTRACT-086]: once per GUID while the 1,024-GUID set has room, then at most once per frame |
| `asset.archive` | `xLOG_CATEGORY_ASSET_ARCHIVE` | archive creation, the failing check number, the verification result |
| `asset.residency` | `xLOG_CATEGORY_ASSET_RESIDENCY` | pressure responses, cache flushes, peak refusals |
| `cook.database`, `cook.import`, `cook.plan`, `cook.step`, `cook.ddc`, `cook.validate`, `cook.server` | tool categories | the tool's own records |

### Profiler zones

| Zone | Emitted by |
|---|---|
| `Assets.Service` | the epilogue node (the node's own zone) |
| `bg/Asset.Service` | each service slice |
| `bg/Asset.Decode` | each decode slice, with the asset GUID and section index as zone arguments |
| `Asset.KindLoader.<Kind>.Begin`, `.Sections` | kind-loader calls made by ZenithAssets |

### Debug views and dumps

- `Zenith_AssetRegistry::WriteDump(Zenith_JsonWriter&)` writes schema `zenith2.asset-dump` version 1: every live slot (GUID, logical path, kind, state, reference count, resident bytes, failure), the loads in flight with their stage and priority, the decode queue, the cache in LRU order. The development console command `asset.dump` (DEVT's console) writes it to `user:/logs/asset-dump-<frame>.json`; frame dumps include it.
- A frame dump taken by DEVT includes the counters above for the frame.

### Provenance queries

`ZenithCook --provenance <guid | logical path | cooked path> --game <Name> --platform <p>` prints the chain of Database.md "Provenance queries API": the cooked file, its header content hash, the asset GUID and logical path, the source files with their byte hashes, the importer and its version, the resolved settings hash, the cook step and version, the toolchain versions and the DDC key; the cook server answers the same query for the editor (`provenance` message).

### Telemetry

The counters above and the per-run cook summary (items cooked, DDC hits and misses, wall-clock per phase) are the telemetry ASSET emits; DEVT's regression dashboards read the cook report.

## Authoring and cooking

### What ASSET authors

ASSET's one authoring document is the `.meta` importer-settings sidecar (Formats.md "Meta sidecar schema"), an authoring-document type of [Z2-N-CONTRACT-552] that migrates, never re-cooks (Z2-D-048). ASSET has no visual editor of its own.

### Editor surfaces ASSET exposes

EDIT owns the asset browser, the import dialogs, reimport commands and the migration UI. ASSET exposes to them, through the cook server protocol (Cook.md "The cook server"):

| Surface | Messages |
|---|---|
| the session handshake | `hello` (protocol and engine cook versions) |
| asset browser listing and search | `list`, `find`, `record` |
| live reload after edits on disk | `watch`, then `changed` and `published` notifications |
| import of new files, `.meta` creation | `assignGuids`, `import` |
| reimport and settings edits | `setSettings`, `reimport` |
| dependency views (references, referenced-by) | `dependencies`, `dependents` |
| cook for play-in-editor, progress, errors | `cook`, progress and diagnostic notifications |
| provenance and validation panels | `provenance`, `validate` |
| migration preview and application for `.meta` documents | `migratePreview`, `migrateApply` |

The runtime side (asset states, failures, residency) is visible to the editor through the registry dump and counters of its play worlds.

### What CONTENT builds on ASSET

The mechanisms of "Boundary with CONTENT"; CONTENT's importers and presets are compiled into ZenithCook under `Internal/Importers/` against Import.md "Importer interface".

### Cook command lines

The exact command lines, options and exit codes are Cook.md "Cook command line"; the build's command is `ZenithCook --game <Name> --platform <windows|android> --out ${ZENITH_OUT}/Cooked/<Name>/<platform>` ([Z2-N-ARCH-207]).

### Migration hooks

| Document type | Chain registered by | Specified in |
|---|---|---|
| `.meta` envelope (`Zenith.AssetMeta`), registered into SCENE's framework | ZenithCook and ZenithEditor, from `Zenith_RegisterAssetMetaMigrations` | Migration.md "The meta document type"; Formats.md "Meta migration chain" |
| importer settings objects (`Zenith.ImporterSettings.<id>`), migrated inside the `.meta` type | each importer's `m_apfnSettingsSteps` | Migration.md "Importer settings inside meta documents"; Import.md "Settings schema" |
| PLAT's bindings and configuration documents | ZenithCook and ZenithEditor, from PLAT's step table (version 1 has none) | `Zenith2/Docs/Design/PLAT.md → "Migration hooks"` |

## Failure handling

### Failure table

| Failure | Behaviour | Evidence |
|---|---|---|
| a GUID absent from the catalogue | `Acquire` returns `ASSET_ERROR_NOT_FOUND`; deserialization maps it to a null handle and one `asset.missing` diagnostic per GUID within the bounded log set ([Z2-N-CONTRACT-086]) | `Test_AssetRegistry_UnknownGuidGivesNullAndDiagnostic`, `Test_AssetRegistry_MissingLogBounded` |
| an archive that fails a check A1–A7 | the archive factory returns `FILE_ERROR_ARCHIVE_CORRUPT`; PLAT does not mount it (a Shipping package archive stops startup with exit code 3; a patch archive leaves the previous patch mounted) | `Test_AssetArchive_EachCheckRejects` |
| a pinned engine fallback fails to load, or a SUBSTITUTE kind has none | startup stops with exit code 3 naming the kind, role and error (`NO_FALLBACK` at registration) | `Test_AssetRegistry_PinnedFallbacksFromCatalogue` |
| a catalogue entry whose file is absent (an undelivered asset pack) | the load fails `ASSET_ERROR_NOT_INSTALLED`; it is retried when the mounts change | `Test_AssetLoader_NotInstalledRetriesAfterMount` |
| a cooked file with a bad magic, header check, size, table, section bounds, section check, descriptor, GUID, kind or content hash | the load fails `ASSET_ERROR_CORRUPT` with the `Zenith_AssetCorruption` detail; the kind's fallback policy applies; never an assert | `Test_AssetFailure_EachCorruptionClassFails` |
| a format version or engine cook version mismatch | `ASSET_ERROR_VERSION_MISMATCH` per asset and at `RegisterKindLoader`; a base catalogue whose engine cook version or kind count differs from the build stops startup with exit code 3 (`COOK_IDENTITY_MISMATCH`) | `Test_CookedFile_VersionMismatchRefused`, `Test_AssetRegistry_CatalogueCookIdentityMismatchFailsCreate`, `Test_AssetRegistry_RegisterKindLoaderRules` |
| a decode failure (`COMPRESSION_ERROR_CORRUPT_INPUT`, `_TRUNCATED_INPUT`, `_SIZE_MISMATCH`) | `ASSET_ERROR_CORRUPT`, detail `ASSET_CORRUPTION_DECODE_FAILED` | `Test_AssetFailure_DecodeCorruptionFails` |
| an I/O failure | `ASSET_ERROR_IO` with the `Zenith_IOErrorCode` in the detail | `Test_AssetFailure_IOErrorFails` |
| a kind loader failure | `ASSET_ERROR_LOADER_FAILED` with the loader's code | `Test_AssetLoader_LoaderFailureReported` |
| a GPU allocation or upload that fails after activation (a full pool, an upload reporting `FAILED`) | the loader reports `ReportGpuFailed`: residency FAILED, the kind's fallback substituted, counted and logged once; cleared by a reload or a device recovery | `Test_AssetLoader_GpuKindActivatesBeforeUpload` |
| a load of a kind whose loader has not registered | it waits in WAIT_LOADER until the loader registers; `CloseRegistration` fails it `NO_LOADER` (a pinned one stops startup with exit code 3) | `Test_AssetLoader_BootWithoutRendering` |
| a hard dependency failed | the dependent fails `ASSET_ERROR_DEPENDENCY_FAILED` unless the dependency's kind substitutes a fallback, in which case the dependent loads against the fallback | `Test_AssetLoader_DependencyFallbackAllowsParent` |
| the registry is full | `ASSET_ERROR_REGISTRY_FULL` from `Acquire`; logged with the row name | `Test_AssetRegistry_CapacityExhaustionRefuses` |
| allocation failure | `ASSET_ERROR_OUT_OF_MEMORY` from the failing call; factories unwind | the fault sweeps of Tests.md |
| the decode queue or staging is full | the service stops issuing reads (BR-STALL of Z2-B-ASSET-002), counted | `Test_AssetBackPressure_BurstNeverExceedsDepth` |
| memory pressure | the Assets row of the governor's shedding table | `Test_AssetResidency_WarningFlushesCache`, `_CriticalDropsCpuCopies` |
| device loss | ACTIVE GPU-kind assets recover through their loaders with residency LOST, then PENDING, then RESIDENT, admitted within `asset.load.maxInFlight` however many there are; CACHED ones are evicted, and an object whose recovery is still queued when its last reference drops unloads instead of caching; a second loss keeps queued recoveries, restarts running ones and supersedes the earlier operation | `Test_AssetLoader_DeviceRecoveryReloadsGpuKinds` |
| an unresolvable asset default in a schema | startup exit code 3 at step 11; the cook fails `ecs.schemas` | `Test_AssetHandle_ResolveReferenceDefault`, `Test_AssetDatabase_CookResolverForDefaults` |
| cancellation at any stage | everything released before `CANCELLED` | `Test_AssetCancel_Stress1000Cycles` |
| a cook step fails | the item fails with its error, dependents fail `COOK_ERROR_DEPENDENCY_FAILED`, the cook exits 1 after every independent item has been attempted, and no partial output is published | `Test_Cook_StepFailurePublishesNothing` |
| the DDC is unreachable, corrupt or stale | a miss: the step cooks locally; the store is never corrupted by the client | `Test_DerivedDataCache_UnreachableFallsBackLocal`, `_CorruptEntryIsMiss`, `_StaleEntryIsMiss` |
| an interrupted cook | the next cook resumes from the journal and converges on the clean cook's bytes | `Test_Cook_InterruptedResumesToCleanBytes` |
| low disk during a cook | the cook stops with `COOK_ERROR_LOW_DISK` before publishing, with nothing half-written | `Test_Cook_LowDiskStopsCleanly` |
| a cook server and a build cook over one output tree, or two writers of one source storage root | the output tree's lock admits one writer: the second exits with code 6 (`COOK_ERROR_OUTPUT_LOCKED`) having read and written nothing, and no process deletes another's temporaries; a storage root's writer lock admits one writer per batch, a refused writer exits 6 (`COOK_ERROR_SOURCE_LOCKED`) after `cook.sourceLockWaitMs`, and a build cook that finds a live writer's journal waits, then exits 6 rather than 3 (Cook.md "Output tree and database locks") | `Test_Cook_ConcurrentServerAndBuildExclusive` |
| a source document changes after the cook's scan | the item that opens it fails with `COOK_ERROR_SOURCE_CHANGED` and publishes nothing; independent items publish; no item reads a mix of scanned and newer bytes (Cook.md "Source documents in cook steps") | `Test_CookContext_OpenSourceDocument` |

### Missing and corrupt assets

Loading.md "Missing and corrupt assets" classifies every failure and applies the kind's fallback policy: `ASSET_FALLBACK_POLICY_SUBSTITUTE` resolves to the kind's registered fallback asset (for example GEOM's per-usage fallback texture and the default material); `ASSET_FALLBACK_POLICY_NULL_OBJECT` resolves to null and the consumer draws, plays or animates nothing; `ASSET_FALLBACK_POLICY_FAIL` resolves to the error and the consumer reports it (a scene load failure fails the world transition).

### DDC failures

DDC.md "Failure modes": every DDC failure degrades to a local cook; the local store deletes an entry it proved corrupt; the shared store is never written by a client except with complete, verified entries through write-then-rename, and never deleted by a client.

### Low disk

The runtime writes nothing (content is read-only; the registry dump goes through `Zenith_AsyncIO::WriteAtomic` to `user:/logs/` and is refused like any write, PLAT's rules). The cook checks free space before publishing each output batch (Cook.md "Low disk during a cook"); the local DDC refuses to populate below its free-space margin and evicts.

### Interrupted cook

Cook.md "Interrupted cook and resume": outputs are published by write-then-rename, the journal records every published output, and the next run verifies and reuses them.

### Allocation failure

Every allocating call returns `ASSET_ERROR_OUT_OF_MEMORY` and leaves the registry unchanged; a load that fails to allocate a section record or staging fails the load (never the process); factories unwind every step (Tests.md fault sweeps).

### Device loss

`Zenith2/Docs/Rendering/GpuResources.md → "Device loss"` step 4 calls `Zenith_AssetRegistry::ReloadGpuResidencySet`; Loading.md "Device-loss reload" specifies it.

### Budget breach

| Row | Breach code | ASSET behaviour |
|---|---|---|
| Z2-B-ASSET-001 | BR-GOVERNOR | the governor raises the level; ASSET sheds its row |
| Z2-B-ASSET-002 | BR-STALL | the service waits for decode capacity; `asset.decode.queue.full` increments and the on-screen profiler shows it |

## Test strategy

### Test targets and gate steps

| Target | Runs in | Contents |
|---|---|---|
| `ZenithAssets_Tests` | `Test` step on `win-debug`, `win-release`; `Sanitizers -Preset linux-tsan` and `win-asan` | handles, registry, catalogue, container, loader, back-pressure, residency, cancellation, failure, reader |
| `ZenithCook_Tests` | `Test` step on `win-debug` | database, import, cook, DDC, server, the `.meta` document type on SCENE's framework |
| `Zenith_Fuzz_*` of ZenithAssets and ZenithCook | `win-asan` 60-second smoke; full duration in stage acceptance | parsers |
| `Zenith_Integration_AssetPipeline` | `Test` step on `win-debug` | cook-then-load end to end |
| `Zenith_Bench_AssetLoading` | `--smoke` in the `Test` step; full on reference hardware by the Stage 1 harness ticket | measurement |

### Test catalogue index

Every named test with the case it asserts is in `Zenith2/Docs/Design/ASSET/Tests.md`; each heading there is one test group.

### Fixtures

Tests.md "Fixtures": the fixture games `Tests/Fixtures/Games/Tiny`, `…/Dependencies`, `…/Corrupt`, the corrupt-content fixture set generated at test time (one file per `Zenith_AssetCorruption` value, never committed), the `.meta` migration fixtures `v1.json`/`v1.expected.json`, and the fuzz seeds built in code at start-up.

### Sanitizer, fuzz and stress runs

- `Test_AssetCancel_Stress1000Cycles`, `Test_AssetLoader_StressUnderTsan` and `Test_AssetBackPressure_BurstNeverExceedsDepth` run under `linux-tsan` through the `Sanitizers` step; that needs `Zenith2/Engine/ZenithAssets/Internal/Loading/**` and `Internal/Registry/**` in the `Zenith2Memory` category's paths, a protected `zagent.project.json` edit made by the Stage 2 PROC `needs-human` ticket that adds `Sanitizers -Preset linux-tsan` to the `Zenith2Memory` category (`Zenith2/Docs/Roadmap.md → "Stage 0 Tooling bring-up"`, the category table of what PROC tickets add; "Pending new rows"); until it lands they run in every stage acceptance's full sanitizer set.
- Accepted risk until that PROC ticket lands: a Stage 2 diff that touches the loader or the registry merges without a `linux-tsan` run of these tests, so a data race it introduces is found only by the next stage acceptance's sanitizer set. Every Stage 2 loader ticket's Definition of Done therefore lists the three tests and their `linux-tsan` run as a manual acceptance item for the reviewer to confirm from the ticket's evidence, and the requirement catalogue's Z2-R-ASSET-145 names the PROC ticket as its dependency.
- The fuzz targets run as `win-asan` smoke runs in the `Test` step and for their full duration in stage acceptance ([Z2-N-ARCH-952]).

### Acceptance evidence

| Evidence (`Zenith2/Docs/Roadmap.md → "Acceptance evidence"`) | ASSET's artefact | First required at |
|---|---|---|
| reimport | `Test_Import_ReimportKeepsSubAssetGuids`, `Test_Integration_ReimportThenLoad` | Stage 3 |
| clean versus incremental cook equivalence | `Test_Cook_CleanEqualsIncremental` over the fixture games and every flagship slice's content | Stage 3 |
| missing or corrupt content | `Test_Integration_MissingAndCorruptContent` over the corrupt-content fixture set | Stage 3 |
| streaming cancellation (1,000 cycles) | `Test_AssetCancel_StreamingStress1000Cycles` (with dependencies and a GPU-stage loader) | Stage 4 |
| unload/reload stress | `Test_AssetResidency_UnloadReloadStress` | Stage 4 |
| fuzzing of cooked formats | the fuzz targets | Stage 2 |

## Budget rows

### Rows ASSET owns

| ID | Name | Status | Use in ASSET |
|---|---|---|---|
| Z2-B-ASSET-001 | Asset system resident data and decode buffers | pending (Stage 1 per-system budget ticket for ASSET) | the budget of `MEMORY_SUBSYSTEM_ASSETS` together with Z2-B-UI-001 and Z2-B-CINE-001 ([Z2-N-ARCH-823]); ASSET's own share covers the registry, catalogue, staging, decode buffers and scratch, the keep-alive cache's host bytes and the CPU data of `ASSETS`-subsystem kinds |
| Z2-B-ASSET-002 | Decode queue depth | pending (same ticket) | the value of `asset.decode.queueDepth`; breach BR-STALL ([Z2-N-BUDGET-238]) |

### Rows ASSET is measured against

| Row | How ASSET contributes |
|---|---|
| Z2-B-SHIP-013 Cold boot Windows, Z2-B-SHIP-014 Cold boot S24 (W-Load) | startup-scene load time and critical-load latency |
| Z2-B-CORE-001, Z2-B-CORE-002, Z2-B-CORE-003 | process totals that ASSET's resident bytes are part of |
| Z2-B-CORE-006 Streaming double-residency peak | reserved by development reloads (Loading.md "Development reload") |
| Z2-B-COMPRESS-001, Z2-B-COMPRESS-002 decompression throughput floors | the decode throughput the decode concurrency is sized against |
| Z2-B-STREAM-003 … Z2-B-STREAM-005 | streaming requests that ASSET's pipeline serves |
| Z2-B-EDIT-004 Asset reimport, Z2-B-EDIT-010 Walkthrough reimport an asset | the cook server's reimport and watch paths |

### Pending new rows

Recorded as pending, producer the Stage 1 per-system budget ticket for ASSET (cross-area row AS7; core-doc item X112, applied in batch 4, added them to `Budgets.md` as Z2-B-ASSET-003 … Z2-B-ASSET-011 in this order):

| Proposed row | Unit | Member of | Why |
|---|---|---|---|
| Asset registry slot capacity | assets | — | `asset.registry.maxAssets`; the registry is bounded ([Z2-N-CONTRACT-342]) |
| Asset loads in flight | loads | — | `asset.load.maxInFlight` |
| Asset in-flight load bytes | MB | Z2-B-ASSET-001 | `asset.load.maxInFlightBytes` |
| Asset I/O staging bytes | MB | Z2-B-ASSET-001 | `asset.io.stagingBytes` |
| Asset keep-alive cache bytes | MB | Z2-B-ASSET-001 | `asset.cache.maxBytes` |
| Asset service CPU per frame | ms | — | `asset.service.frameBudgetUs` and the background share |
| Critical asset load latency | ms | — | request-to-activation of a CRITICAL load of the W-Load start scene's largest asset |
| Local DDC size | GB | — | `cook.ddc.localMaxBytes` |
| Incremental no-op cook time per slice | s | — | the editor iteration contribution of Z2-B-EDIT-004 |

A PROC ticket adds `Zenith2/Engine/ZenithAssets/Internal/Loading/**` and `Internal/Registry/**` to the `Zenith2Memory` category's paths (a protected `zagent.project.json` edit).

### Configuration keys

Registered by `Zenith_RegisterAssetsConfigKeys`; every value comes from ZenithAssets' platform fragments, which the Stage 1 per-system budget ticket for ASSET writes, so no budget number is compiled into ZenithAssets:

| Key | Type | Meaning |
|---|---|---|
| `asset.registry.maxAssets` | int | slot capacity (row "Asset registry slot capacity") |
| `asset.decode.queueDepth` | int | decode units (Z2-B-ASSET-002) |
| `asset.decode.concurrency` | int | decode slices running at once; 0 = `max(1, workers / 2)` |
| `asset.decode.sliceBytes` | int | decoded bytes per decode slice (1 MiB default in the fragment) |
| `asset.io.stagingBytes` | int | staging budget |
| `asset.io.chunkBytes` | int | streamed-decode chunk size |
| `asset.io.streamThresholdBytes` | int | sections above this stored size decode streamed |
| `asset.load.maxInFlight`, `asset.load.maxInFlightBytes` | int | the in-flight budget |
| `asset.cache.maxBytes` | int | keep-alive cache |
| `asset.service.frameBudgetUs` | int | the `Assets.Service` node's budget |
| `asset.reload.retireFrames` | int | frames an object replaced by a development reload stays alive |

Each key is restart-required, not simulation-affecting (shedding and residency change no simulation state, [Z2-N-ARCH-812]), and allowed in the engine-default, platform and command-line layers.

## Stage plan

### Stage 1

| Slice | Content | Governing |
|---|---|---|
| per-system budget ticket for ASSET (`needs-human` ruling) | values of Z2-B-ASSET-001 and -002 and of the pending rows above, with `allocation.json`, run on `Zenith_Bench_AssetLoading` | Z2-D-012, Z2-D-084 |
| harness ticket | `Zenith2/Benchmarks/AssetLoading/` with a `--smoke` CTest run | Z2-D-084 |
| pull-forward: API-only slices | `Zenith_AssetTypes.h`, `Zenith_AssetHandle.h`, `Zenith_CookedFile.h` declarations with contract blocks (fix version `Z2 Stage 2`) | — |

### Stage 2

The "serialization and basic assets" deliverable (`Zenith2/Docs/Roadmap.md → "Stage 2 Foundation"`), in dependency order:

1. `ZenithAssets` types, handles and value hooks; registry slots and side records, acquisition, retirement; test kinds declared per registry.
2. The cooked container: header, table, checks, writer; fuzz targets.
3. The catalogue: format (the test-kind table included), loading, indexes.
4. The loader: requests, one pipeline per asset, the QUEUED sub-stages (catalogue lookup, hard dependencies, waiting for a loader, admission) with no load record held before admission, header and section reads through `Zenith_AsyncIO`, decode through the codec set in a bounded decode queue, delivery, activation, cancellation at every stage, the 1,000-cycle stress, completion queues, the service and `Assets.Service`, the headless loader, the reader.
5. The `.meta` document type on SCENE's `ZenithDocument` framework (after SCENE's framework core lands, itself after D-SEL-JSON (open, Stage 1)): registration, canonical schema, importer-settings visitor, recovery and refusal, fixtures.
6. ZenithCook: the database (GUIDs, `.meta` v1, the tombstone lists and the deletion rule, the cook-side reference-default resolver), the importer interface and ASSET's passthrough importers, the full (non-incremental) cook with steps, content selection from `composition.json` with the declared engine fallbacks cooked into every catalogue (Z2-R-ASSET-152), the configuration merge, the catalogue with its fallback table, the mount manifest, structural and reference validation, the depfile, stale removal, the report.
7. The runtime pieces the boot needs: the value-kind hooks and the reference-default resolver, the built-in data and dictionary loaders, the pinned fallback table read from the catalogue and pinned at registration (Z2-R-ASSET-067), GPU activation before upload with the residency sub-state and its failure edge, loads waiting for late-registering loaders, the verified reads of catalogue file entries (SCRIPT's bytecode, schema blob and configuration from Stage 2 on), the queue policies and counters.
8. Integration: `Games/Minimal` cooks and loads on Windows and Android with its engine fallbacks pinned (a SUBSTITUTE kind's registration refuses `NO_FALLBACK` otherwise); the integration suite.

The DDC key's hash is D-SEL-HASH (open, Stage 1); until the ruling no cooked header is shipped with a persisted hash.

### Stage 3

Pull-forwards of Stage 4 items that the Stage 3 acceptance evidence needs: reimport keeping identity for the slice content, clean versus incremental equivalence (incremental by full key comparison, before the DDC exists), the missing and corrupt content set with the fallbacks it substitutes (the fallback table itself is Stage 2's), device-loss residency reload with recovery pipelines admitted within the in-flight limits, the Shipping exclusions. The inventory stages of Z2-I-ASSET-04, -12 and -13 move to S3 by core-doc item X113 (pending).

### Stage 4

The full asset pipeline and streaming (`Zenith2/Docs/Roadmap.md → "Stage 4 Production content and worlds"`): the DDC (key, local store, verification, failure modes, the shared-store interface CONTENT implements), incremental and dependency-driven invalidation, content-rule validation from the rule table and its report, provenance, the cook journal and resume, low disk, worker processes, the output-tree and database locks, the cook server for EDIT (protocol version 1 with its handshake, paging, watching and notifications); the `.zpak` archive reader and its PLAT factory integration (PACK's archives are Stage 4, Z2-I-PACK-02); catalogue fragments with external references; runtime back-pressure in full (throttled I/O, staging budget, priority reserves, streamed decode), residency budgets (accounting, keep-alive cache, in-flight budget, shedding), development reload, catalogue fragments, the streaming cancellation and unload/reload stress, the budget-row measurements.

### Stage 5 to Stage 7

Stage 6: cooked-version refusal and patch-line compatibility demonstrated with fixtures (Z2-D-048), security hardening evidence (full-duration fuzz). Stages 5 and 7 add no ASSET capability.

### API-only slices

Interfaces land first: `Zenith_AssetTypes.h`; `Zenith_AssetHandle.h`; `Zenith_AssetKindLoader.h` (so GEOM, SCENE and modules implement loaders in parallel); `Zenith_CookedFile.h` (so GEOM writes files); ZenithCook's `Zenith_CookStep.h` and `Zenith_Importer.h` (so GEOM and CONTENT write steps and importers); in Stage 4, `Zenith_AssetArchive.h` (so PACK's packer tests read back its archives).

## Decisions

### Settled decisions governing ASSET

| ID | Subject in ASSET |
|---|---|
| Z2-D-038 | JSON authoring (`.meta`), versioned binary cooked data, serialization never through C++ layout |
| Z2-D-048 | cooked re-cook and refusal; `.meta` migration with preview, backup, journaled write, unknown fields |
| Z2-D-065 | the cooked-content hash is an evidence-fingerprint input; the DDC key is its per-asset part |
| Z2-D-004 | codecs recorded per section, selected from the data |
| Z2-D-040 | shared asset handles are generational and reference-counted |
| Z2-D-041 | containers from CORE; no new container |
| Z2-D-045 | `Zenith_Result`, asserts for programmer errors only |
| Z2-D-046 | the registry is an object the composition root owns and passes; no global |
| Z2-D-053 | loads become simulation-visible only at tick boundaries in completion-ID order |
| Z2-D-019 | shedding, residency and load timing never change simulation results |
| Z2-D-024, Z2-D-081 | the cook registers exactly the game's selected modules |
| Z2-D-025 | the cook invokes the offline shader compiler; nothing compiles at run time |
| Z2-D-075 | no mod content |
| Z2-D-082 | importer libraries (manifest entries `fastgltf`, `meshoptimizer`, `ktx-software`) pinned and confined to ZenithCook's `Internal/` |
| Z2-D-085, Z2-D-097 | this document is edited in the same diff as any ASSET contract change; rulings are appended to DecisionLog.md in the ticket's diff |

### Open decisions ASSET depends on

| Decision | Stage | What waits on it |
|---|---|---|
| D-SEL-JSON | open, Stage 1 | `.meta` reading and writing, the cook report, the migration framework (through CORE's `Zenith_JsonDocument`, which is backend-neutral) |
| D-SEL-HASH | open, Stage 1 | the DDC key, header content hashes, section checks, the cooked-content hash |
| D-SEL-FBX | open, Stage 4 | CONTENT's FBX importer; ASSET's interface is unchanged either way |
| D-SEL-HEAP | open, Stage 1 | nothing specific: ASSET allocates through `Zenith_Heap` |

### Selection decisions ASSET owns

None. The candidates ASSET might have selected are either CORE's (the hash, the JSON library) or CONTENT's (the shared DDC backend).

### Design rulings ASSET records

These six rulings are appended to `Zenith2/Docs/DecisionLog.md` together, in one human-run decision-log diff with consecutive free `Z2-D-NNN` IDs (Z2-D-097), before any implementation that relies on them; each implementing change cites its entry: the GUID-derived cooked object naming (A1); the closed kind table and its growth rule (A4); the reservation form of back-pressure (A6); the out-of-process cook server as EDIT's only path to the pipeline (A11); GPU activation before upload (A13); catalogue-listed engine fallbacks (A14). Two orchestrator rulings already govern this design and need no further record here: the migration framework is SCENE's `ZenithDocument` (Q43 (a)), and the `.zpak` reader is ASSET's in ZenithAssets with PACK owning the writer (A15).

## Requirement index

| Requirement | Title | Stage | Design headings |
|---|---|---|---|
| Z2-R-ASSET-001 | ZenithAssets library target | S2 | ASSET.md "Target summary", "ZenithAssets internal files", "ZENITH_SUPPORTED matrix" |
| Z2-R-ASSET-002 | Public header rules and contract blocks | S2 | ASSET.md "Rules across the ASSET API"; RuntimeAPI.md "Header list" |
| Z2-R-ASSET-003 | Closed asset kind table | S2 | ASSET.md "Asset kinds"; RuntimeAPI.md "Asset kinds and the kind table" |
| Z2-R-ASSET-004 | Asset kinds registered as reflected asset types | S2 | RuntimeAPI.md "Registration functions"; ASSET.md "Construction and shutdown" |
| Z2-R-ASSET-005 | Asset GUIDs are persistent IDs | S2 | RuntimeAPI.md "Asset kinds and the kind table"; Database.md "GUIDs" |
| Z2-R-ASSET-006 | Asset handle layout and reference counting | S2 | RuntimeAPI.md "Zenith_AssetHandle", "Reference counting and retirement" |
| Z2-R-ASSET-007 | Untyped handles and reflection value hooks | S2 | RuntimeAPI.md "Zenith_AnyAssetHandle", "Reflection value hooks" |
| Z2-R-ASSET-008 | Handle resolution never asserts | S2 | RuntimeAPI.md "Zenith_AssetRegistry"; Tests.md "Handle and registry tests" |
| Z2-R-ASSET-009 | Registry creation unwinds | S2 | RuntimeAPI.md "Zenith_AssetRegistryDesc", "Zenith_AssetRegistry" |
| Z2-R-ASSET-010 | Registry slot capacity | S2 | RuntimeAPI.md "Slot header layout"; ASSET.md "Configuration keys" |
| Z2-R-ASSET-011 | Zero-reference retirement | S2 | RuntimeAPI.md "Reference counting and retirement"; Loading.md "Unload" |
| Z2-R-ASSET-012 | Registry outlives handles and reports leaks | S2 | ASSET.md "Construction and shutdown"; Loading.md "Shutdown sequence" |
| Z2-R-ASSET-013 | Catalogue load and validation | S2 | RuntimeAPI.md "Zenith_AssetCatalogue header"; Formats.md "Asset catalogue file", "Catalogue limits" |
| Z2-R-ASSET-014 | Catalogue fragments and mount changes | S4 | RuntimeAPI.md "GUID index and catalogue snapshots"; Formats.md "Catalogue fragments"; Loading.md "Retry after a mount change" |
| Z2-R-ASSET-015 | Acquisition by GUID and logical path | S2 | RuntimeAPI.md "Zenith_AssetRegistry"; ASSET.md "Content root mapping" |
| Z2-R-ASSET-016 | Unresolvable references give null handles | S2 | RuntimeAPI.md "Zenith_AssetRegistry"; Loading.md "Failure classification" |
| Z2-R-ASSET-017 | Kind loader registration | S2 | RuntimeAPI.md "Zenith_AssetKindLoader header", "Zenith_AssetRegistry"; ASSET.md "Construction and shutdown" |
| Z2-R-ASSET-018 | Common cooked file header | S2 | Formats.md "Header layout", "Section table", "Section placement and alignment" |
| Z2-R-ASSET-019 | Cooked header and table validation | S2 | Formats.md "Checks", "Limits"; RuntimeAPI.md "Parsing and validation functions" |
| Z2-R-ASSET-020 | Codec selected from the data | S2 | Loading.md "Section decode"; Formats.md "Section table" |
| Z2-R-ASSET-021 | Deterministic cooked file writer | S2 | RuntimeAPI.md "Zenith_CookedFileWriter" |
| Z2-R-ASSET-022 | Cooked version refusal | S2 | Formats.md "Checks", "Catalogue header"; RuntimeAPI.md "Parsing and validation functions", "Zenith_AssetCatalogue header"; Loading.md "Failure classification" |
| Z2-R-ASSET-023 | Load requests return operation handles | S2 | RuntimeAPI.md "Zenith_AssetLoader" |
| Z2-R-ASSET-024 | One pipeline per asset | S2 | Loading.md "Request deduplication and priorities" |
| Z2-R-ASSET-025 | Pipeline stage order | S2 | Loading.md "Load record stages", "Pipeline stages", "Catalogue lookup" |
| Z2-R-ASSET-026 | Hard dependencies load first | S2 | Loading.md "Dependency loading" |
| Z2-R-ASSET-027 | Load priorities | S2 | Loading.md "Request deduplication and priorities" |
| Z2-R-ASSET-028 | Cancellation during I/O | S2 | Loading.md "Cancellation points by stage" |
| Z2-R-ASSET-029 | Cancellation during decompression | S2 | Loading.md "Cancellation points by stage" |
| Z2-R-ASSET-030 | Cancellation while the loader builds the object | S2 | Loading.md "Cancellation points by stage"; RuntimeAPI.md "Zenith_AssetKindLoader" |
| Z2-R-ASSET-031 | Cancellation during activation | S2 | Loading.md "Cancellation points by stage" |
| Z2-R-ASSET-032 | Completion reported after release | S2 | Loading.md "Completion reporting" |
| Z2-R-ASSET-033 | Ownership of partial results | S2 | Loading.md "Ownership of partial results" |
| Z2-R-ASSET-034 | Load cancellation stress | S2 | Tests.md "Cancellation and stress tests" |
| Z2-R-ASSET-035 | Streaming cancellation stress with dependencies | S4 | Tests.md "Cancellation and stress tests" |
| Z2-R-ASSET-036 | Completion queues for requesters | S2 | RuntimeAPI.md "Zenith_AssetLoadCompletionQueue" |
| Z2-R-ASSET-037 | Simulation sees loads only through tick-boundary publication | S2 | Loading.md "Publication to simulation" |
| Z2-R-ASSET-038 | Service in background slices | S2 | Loading.md "Service scheduling" |
| Z2-R-ASSET-039 | Assets.Service frame node | S2 | Loading.md "The Assets.Service node"; ASSET.md "Frame-graph placement" |
| Z2-R-ASSET-040 | Tool and boot servicing | S2 | Loading.md "Tool and boot servicing"; RuntimeAPI.md "Zenith_AssetLoader" |
| Z2-R-ASSET-041 | Headless loading | S2 | Loading.md "Headless loading" |
| Z2-R-ASSET-042 | Range and section reads | S2 | RuntimeAPI.md "Zenith_AssetReader header" |
| Z2-R-ASSET-043 | Verified reads of cooked files | S2 | RuntimeAPI.md "Zenith_AssetReader header"; Formats.md "File entries" |
| Z2-R-ASSET-044 | Bounded decode queue | S2 | Loading.md "Definitions", "Decode concurrency and slices", "Internal queues" |
| Z2-R-ASSET-045 | I/O throttled by decode capacity | S4 | Loading.md "Issue rule" |
| Z2-R-ASSET-046 | Staging byte budget | S4 | Loading.md "Issue rule" |
| Z2-R-ASSET-047 | Decode priority reserves | S4 | Loading.md "Priority reserves" |
| Z2-R-ASSET-048 | Streamed decode of large sections | S4 | Loading.md "Streamed decode of large sections" |
| Z2-R-ASSET-049 | Decode slices and concurrency | S4 | Loading.md "Decode concurrency and slices" |
| Z2-R-ASSET-050 | Back-pressure counters and stall breach | S4 | Loading.md "Back-pressure counters"; ASSET.md "Counters" |
| Z2-R-ASSET-051 | Back-pressure burst test | S4 | Tests.md "Back-pressure tests" |
| Z2-R-ASSET-052 | Resident byte accounting | S4 | Loading.md "Resident byte accounting" |
| Z2-R-ASSET-053 | Asset memory consumer | S4 | Loading.md "Memory pressure responses"; RuntimeAPI.md "Zenith_AssetRegistry" |
| Z2-R-ASSET-054 | Keep-alive cache | S4 | Loading.md "The keep-alive cache" |
| Z2-R-ASSET-055 | Shedding at warning | S4 | Loading.md "Memory pressure responses" |
| Z2-R-ASSET-056 | Shedding at critical | S4 | Loading.md "Memory pressure responses" |
| Z2-R-ASSET-057 | Shedding timing and revert | S4 | Loading.md "Memory pressure responses" |
| Z2-R-ASSET-058 | In-flight load budget | S4 | Loading.md "In-flight load budget", "Reload and recovery pipelines"; RuntimeAPI.md "Zenith_AssetKindLoaderDesc" |
| Z2-R-ASSET-059 | Reload double-residency reservation | S4 | Loading.md "Development reload" |
| Z2-R-ASSET-060 | Unload and reload stress | S4 | Loading.md "Unload and reload"; Tests.md "Residency tests" |
| Z2-R-ASSET-061 | Development reload | S4 | Loading.md "Development reload" |
| Z2-R-ASSET-062 | Device-loss residency reload | S3 | Loading.md "Device-loss reload", "Reload and recovery pipelines", "GPU residency sub-state", "In-flight load budget" |
| Z2-R-ASSET-063 | Missing assets | S3 | Loading.md "Failure classification"; ASSET.md "Missing and corrupt assets" |
| Z2-R-ASSET-064 | Undelivered content retried after mounts change | S4 | Loading.md "Retry after a mount change" |
| Z2-R-ASSET-065 | Corrupt assets fail without asserting | S3 | Loading.md "Failure classification"; Formats.md "Checks" |
| Z2-R-ASSET-066 | Fallback per kind | S3 | Loading.md "Fallback policy per kind", "GPU residency sub-state", "The keep-alive cache"; ASSET.md "Asset kinds" |
| Z2-R-ASSET-067 | Engine fallbacks pinned at startup | S2 | Loading.md "Fallback policy per kind", "Tool and boot servicing"; RuntimeAPI.md "Zenith_AssetRegistry" |
| Z2-R-ASSET-068 | Parser fuzz targets | S2 | Tests.md "Fuzz targets" |
| Z2-R-ASSET-069 | Missing and corrupt content fixture set | S3 | Tests.md "Missing and corrupt content tests", "Fixtures" |
| Z2-R-ASSET-070 | Meta sidecar per asset source | S2 | Database.md "What is an asset source", "The meta sidecar" |
| Z2-R-ASSET-071 | GUID assignment | S2 | Database.md "GUIDs" |
| Z2-R-ASSET-072 | GUIDs never reused | S2 | Database.md "GUIDs", "Orphaned meta files and tombstones"; Formats.md "Tombstone list" |
| Z2-R-ASSET-073 | Duplicate GUIDs rejected | S2 | Database.md "Duplicate GUIDs" |
| Z2-R-ASSET-074 | Rename and move keep GUIDs | S4 | Database.md "Rename and move detection" |
| Z2-R-ASSET-075 | Sub-asset identity | S2 | Database.md "Sub-asset identity" |
| Z2-R-ASSET-076 | Asset database file | S2 | Database.md "Database file location and rebuild"; Formats.md "Asset database file" |
| Z2-R-ASSET-077 | Scan and reconcile | S2 | Database.md "Scan algorithm" |
| Z2-R-ASSET-078 | Dependency graph | S2 | Database.md "Dependency graph" |
| Z2-R-ASSET-079 | Build cooks never write sources | S2 | Database.md "Reading and writing meta documents"; Cook.md "Options" |
| Z2-R-ASSET-080 | Content names and root mapping | S2 | ASSET.md "Content root mapping"; Cook.md "Content root mapping rules" |
| Z2-R-ASSET-081 | Meta documents migrate | S2 | Migration.md "The meta document type", "Registered document types"; Formats.md "Meta migration chain" |
| Z2-R-ASSET-082 | Meta canonical form and unknown fields | S2 | Database.md "Canonical meta form"; Migration.md "The meta document type"; Formats.md "Keys" |
| Z2-R-ASSET-083 | Importer interface and registry | S2 | Import.md "Importer interface", "Importer registry" |
| Z2-R-ASSET-084 | Import settings resolution | S2 | Import.md "Settings schema", "Settings resolution", "Compression setting" |
| Z2-R-ASSET-085 | Deterministic import | S2 | Import.md "Import algorithm", "Intermediates" |
| Z2-R-ASSET-086 | Reimport triggers | S4 | Import.md "Reimport triggers" |
| Z2-R-ASSET-087 | Reimport keeps identity | S3 | Import.md "Identity preservation on reimport" |
| Z2-R-ASSET-088 | Editor reimport through the cook server | S4 | Import.md "Editor reimport"; Cook.md "Requests"; Formats.md "Notifications" |
| Z2-R-ASSET-089 | Importer isolation mechanism | S4 | Cook.md "Worker processes"; Import.md "Boundary with CONTENT importers" |
| Z2-R-ASSET-090 | Cook command line and exit codes | S2 | Cook.md "Options", "Exit codes" |
| Z2-R-ASSET-091 | Cook step registry, phases and source documents | S2 | Cook.md "Cook steps", "Step phases", "Step registration functions", "Zenith_CookContext", "Source documents in cook steps" |
| Z2-R-ASSET-092 | Content selection from the composition | S2 | Cook.md "Content selection" |
| Z2-R-ASSET-093 | Configuration fragment merge | S2 | Cook.md "Configuration merge" |
| Z2-R-ASSET-094 | Catalogue and mount manifest generation | S2 | Cook.md "Catalogue generation", "Mount manifest" |
| Z2-R-ASSET-095 | Cooked output tree and atomic publication | S2 | ASSET.md "Cooked output tree"; Cook.md "Atomic output publication" |
| Z2-R-ASSET-096 | Deterministic cook output | S2 | Cook.md "Deterministic ordering" |
| Z2-R-ASSET-097 | Dependency file | S2 | Cook.md "Dependency file"; Formats.md "Dependency file" |
| Z2-R-ASSET-098 | Incremental cook by key | S4 | Cook.md "Keys and invalidation", "Output reuse" |
| Z2-R-ASSET-099 | Dependency-driven invalidation | S4 | Cook.md "Dependency-driven invalidation" |
| Z2-R-ASSET-100 | Clean and incremental cook equivalence | S3 | Cook.md "Output reuse"; Tests.md "Cook tests" |
| Z2-R-ASSET-101 | Cook report | S2 | Cook.md "Cook report"; Formats.md "Cook report" |
| Z2-R-ASSET-102 | Structural validation of outputs | S2 | Cook.md "Structural validation" |
| Z2-R-ASSET-103 | Reference validation | S2 | Cook.md "Reference validation" |
| Z2-R-ASSET-104 | Content rule validation | S4 | Cook.md "Content rules"; Formats.md "Content rule table" |
| Z2-R-ASSET-105 | Validation report | S4 | Cook.md "Validation report" |
| Z2-R-ASSET-106 | Provenance records | S4 | Database.md "Provenance record content"; Formats.md "Provenance record lines" |
| Z2-R-ASSET-107 | Provenance queries | S4 | Database.md "Provenance queries API"; ASSET.md "Provenance queries" |
| Z2-R-ASSET-108 | Interrupted cook resumes | S4 | Cook.md "Interrupted cook and resume", "Output tree and database locks"; Formats.md "Cook journal" |
| Z2-R-ASSET-109 | Low disk during a cook | S4 | Cook.md "Low disk during a cook" |
| Z2-R-ASSET-110 | Stale output removal | S2 | Cook.md "Stale output removal" |
| Z2-R-ASSET-111 | Cook worker processes | S4 | Cook.md "Worker processes" |
| Z2-R-ASSET-112 | Cook server protocol | S4 | Cook.md "The cook server"; Formats.md "Cook server messages", "Requests" |
| Z2-R-ASSET-113 | DDC key inputs | S4 | DDC.md "Key inputs" |
| Z2-R-ASSET-114 | DDC key encoding | S4 | DDC.md "Key encoding" |
| Z2-R-ASSET-115 | Compression settings in DDC keys | S4 | DDC.md "Key inputs"; Import.md "Compression setting" |
| Z2-R-ASSET-116 | Local DDC store | S4 | DDC.md "Directory layout", "Atomic population"; Formats.md "DDC entry file" |
| Z2-R-ASSET-117 | DDC lookup populate verify | S4 | DDC.md "Lookup populate and verify" |
| Z2-R-ASSET-118 | Corrupt DDC entries | S4 | DDC.md "Corrupt entry" |
| Z2-R-ASSET-119 | Stale DDC entries | S4 | DDC.md "Stale entry" |
| Z2-R-ASSET-120 | Unreachable DDC store | S4 | DDC.md "Unreachable store" |
| Z2-R-ASSET-121 | Shared DDC store contract | S4 | DDC.md "Shared store contract", "Zenith_DerivedDataStore" |
| Z2-R-ASSET-122 | DDC verification sampling | S4 | DDC.md "Verification sampling" |
| Z2-R-ASSET-123 | Local DDC size cap | S4 | DDC.md "Size cap and eviction" |
| Z2-R-ASSET-124 | DDC invalidation per key input | S4 | DDC.md "Key inputs"; Tests.md "DDC tests" |
| Z2-R-ASSET-125 | Cooked content hash | S2 | Formats.md "Catalogue header"; DDC.md "Relationship to evidence fingerprints" |
| Z2-R-ASSET-126 | Meta document type registration | S2 | Migration.md "The meta document type", "Registered document types" |
| Z2-R-ASSET-127 | Meta migration preview | S2 | Migration.md "Reading and writing meta documents through the framework" |
| Z2-R-ASSET-128 | Meta migration backup | S2 | Migration.md "Reading and writing meta documents through the framework" |
| Z2-R-ASSET-129 | Journaled meta writes and recovery | S2 | Migration.md "Reading and writing meta documents through the framework", "Recovery before a cook" |
| Z2-R-ASSET-130 | Unknown fields and removed settings in meta documents | S2 | Migration.md "Removed settings and unknown fields" |
| Z2-R-ASSET-131 | Newer meta documents open read-only | S2 | Migration.md "Importer settings inside meta documents"; Database.md "Reading and writing meta documents" |
| Z2-R-ASSET-132 | Migration fixtures for meta documents | S2 | Migration.md "Meta migration fixtures" |
| Z2-R-ASSET-133 | Lock usage | S2 | ASSET.md "Lock-order entries" |
| Z2-R-ASSET-134 | Asset counters | S2 | ASSET.md "Counters"; Loading.md "Internal queues" |
| Z2-R-ASSET-135 | On-screen profiler rows | S2 | ASSET.md "On-screen profiler rows" |
| Z2-R-ASSET-136 | Log categories and messages | S2 | ASSET.md "Log categories" |
| Z2-R-ASSET-137 | Registry dump | S2 | ASSET.md "Debug views and dumps" |
| Z2-R-ASSET-138 | Shutdown sequence | S2 | Loading.md "Shutdown sequence"; ASSET.md "Construction and shutdown" |
| Z2-R-ASSET-139 | Development services absent from Shipping | S3 | Loading.md "Development reload"; Cook.md "The cook server" |
| Z2-R-ASSET-140 | Stage 1 ASSET budget ticket | S1 | ASSET.md "Rows ASSET owns", "Pending new rows", "Stage 1" |
| Z2-R-ASSET-141 | Asset memory within its row | S4 | ASSET.md "Rows ASSET owns" |
| Z2-R-ASSET-142 | Decode queue depth within its row | S4 | ASSET.md "Rows ASSET owns"; Loading.md "Back-pressure counters" |
| Z2-R-ASSET-143 | Boot loading within the cold-boot rows | S4 | ASSET.md "Rows ASSET is measured against"; Loading.md "Tool and boot servicing" |
| Z2-R-ASSET-144 | Minimal game content cooks and loads on both platforms | S2 | Tests.md "Integration suite" |
| Z2-R-ASSET-145 | Loader stress under linux-tsan | S2 | Tests.md "Cancellation and stress tests"; ASSET.md "Sanitizer, fuzz and stress runs" |
| Z2-R-ASSET-146 | Examples compiled as tests | S2 | RuntimeAPI.md "Examples" |
| Z2-R-ASSET-147 | Cooked compatibility demonstrated | S6 | Tests.md "Missing and corrupt content tests"; ASSET.md "Stage 5 to Stage 7" |
| Z2-R-ASSET-148 | Kind code allocation and kind sets | S2 | ASSET.md "Asset kinds"; RuntimeAPI.md "Asset kinds and the kind table" |
| Z2-R-ASSET-149 | Format versions of unused kinds | S2 | Formats.md "Kind table"; RuntimeAPI.md "Zenith_AssetRegistry", "Zenith_AssetCatalogue header" |
| Z2-R-ASSET-150 | Asset reference-default resolvers | S2 | RuntimeAPI.md "Reflection value hooks"; Database.md "Reference defaults in the cook" |
| Z2-R-ASSET-151 | GPU kinds activate before their uploads complete | S2 | Loading.md "GPU residency sub-state", "Tool and boot servicing", "Delivery and activation"; RuntimeAPI.md "Zenith_AssetLoadContext", "Zenith_AssetRegistry" |
| Z2-R-ASSET-152 | Engine fallbacks cooked into every catalogue | S2 | Cook.md "Engine fallbacks"; Formats.md "Fallback table" |
| Z2-R-ASSET-153 | Every state and event of the asset state machine | S2 | Loading.md "Asset state machine", "Reload and recovery pipelines" |
| Z2-R-ASSET-154 | Catalogue fragment external references | S4 | Formats.md "Catalogue fragments", "Dependency and string tables"; RuntimeAPI.md "Zenith_AssetCatalogue header" |
| Z2-R-ASSET-155 | Archive format validation | S4 | Formats.md "Archive file", "Archive checks", "Archive limits" |
| Z2-R-ASSET-156 | Archive lookup listing and reads | S4 | RuntimeAPI.md "Zenith_AssetArchive header" |
| Z2-R-ASSET-157 | Archive sources mounted through the PLAT factory | S4 | RuntimeAPI.md "Zenith_AssetArchive header"; ASSET.md "Construction and shutdown" |
| Z2-R-ASSET-158 | Cook server handshake and protocol version | S4 | Formats.md "Framing", "Session rules", "Requests" |
| Z2-R-ASSET-159 | Cook server result paging and line limit | S4 | Formats.md "Framing", "Requests" |
| Z2-R-ASSET-160 | Cook server change notifications | S4 | Formats.md "Notifications"; Cook.md "The cook server" |
| Z2-R-ASSET-161 | Cook server unknown requests and cancellation | S4 | Formats.md "Session rules" |
| Z2-R-ASSET-162 | Queue full policies and queue counters | S2 | Loading.md "Internal queues"; ASSET.md "Counters" |
| Z2-R-ASSET-163 | Content rules read from a data table | S4 | Cook.md "Content rules"; Formats.md "Content rule table" |
| Z2-R-ASSET-164 | Tracked tombstone lists | S2 | Formats.md "Tombstone list"; Database.md "Database file location and rebuild" |
| Z2-R-ASSET-165 | Built-in data and dictionary loaders | S2 | RuntimeAPI.md "Zenith_BuiltInAssets header" |
| Z2-R-ASSET-166 | Cancelled loads are never published | S2 | Loading.md "Publication to simulation"; Formats.md "Load result payload" |
| Z2-R-ASSET-167 | Importer settings migrate inside meta documents | S2 | Migration.md "Importer settings inside meta documents"; Import.md "Settings schema" |
| Z2-R-ASSET-168 | Test kinds declared per registry | S2 | ASSET.md "Asset kinds"; RuntimeAPI.md "Asset kinds and the kind table", "Zenith_AssetRegistryDesc", "Zenith_AssetRegistry"; Formats.md "Kind table"; Tests.md "Fixtures" |
| Z2-R-ASSET-169 | GPU residency failure after activation | S2 | Loading.md "GPU residency sub-state", "Fallback policy per kind"; RuntimeAPI.md "Zenith_AssetRegistry", "Zenith_AssetKindLoader" |
| Z2-R-ASSET-170 | Residency reports race-free across reloads | S2 | RuntimeAPI.md "Slot header layout", "Zenith_AssetRegistry", "Priorities and states", "Zenith_AssetLoadContext"; Loading.md "GPU residency sub-state" |
| Z2-R-ASSET-171 | Loads wait for late-registering loaders | S2 | Loading.md "Load record stages", "Asset state machine", "Tool and boot servicing"; RuntimeAPI.md "Zenith_AssetRegistry" |
| Z2-R-ASSET-172 | Queued pipelines hold no load record | S2 | RuntimeAPI.md "Slot side records", "Internal types and files"; Loading.md "Dependency loading", "In-flight load budget" |
| Z2-R-ASSET-173 | Reload and recovery pipelines pass admission | S3 | Loading.md "Reload and recovery pipelines", "In-flight load budget", "Device-loss reload", "Internal queues" |
| Z2-R-ASSET-174 | Cook output tree and database locks | S4 | Cook.md "Output tree and database locks", "Exit codes"; Formats.md "Cook lock file"; Database.md "Database file location and rebuild"; Migration.md "Recovery before a cook" |
| Z2-R-ASSET-175 | Deleted asset GUIDs reach the tracked tombstone lists | S2 | Database.md "Orphaned meta files and tombstones"; Formats.md "Tombstone list" |
