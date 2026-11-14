# SCENE design — Scenes & prefabs

The design of the SCENE area: stable authoring identities, scene and prefab documents with canonical serialization, nested prefabs, overrides and variants, reference repair, the authoring-document migration framework, transactional loading, additive scenes, scene transitions, transform propagation in phase 8, and missing-type handling. It elaborates `Zenith2/Docs/Architecture/LifecycleCommands.md`, `Zenith2/Docs/Architecture/FramePhases.md`, `Zenith2/Docs/Architecture/ECSModel.md`, `Zenith2/Docs/Architecture/DataDefinedComponents.md` and `Zenith2/Docs/Contracts.md` → "Compatibility policy per kind", and never restates their bullets; where it adds a detail it marks it `<!-- elaboration -->`, and where it assumes an interface another area has not yet declared it marks it `<!-- assumption: pending <AREA> -->` and records it in `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md` (rows S1 to S25); an assumption on a core document names its part of cross-doc item X109 or the core-doc item it waits on. The requirement catalogue is `Zenith2/Docs/Requirements/SCENE.md`.

This entry document holds the skeleton and the top-level contracts; eight split files hold the rest (see "Document family").

## Purpose and scope

### What SCENE owns

| Piece | Where | Specified in |
|---|---|---|
| the transform value type, the transform components, the hierarchy cache over ECS's parent relation, the phase-8 propagation system, teleport, reparenting, interpolation data | `ZenithScene` | `Zenith2/Docs/Design/SCENE/Transforms.md` |
| `Zenith_LifecycleListener` and its dispatcher, the SCENE lifecycle command recorders (teleport, prefab instantiation) | `ZenithScene` | `Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_LifecycleListener header", "Zenith_LifecycleCommands header" |
| authoring identities and their derivation, the scene and prefab document model, document edits for EDIT's command system | `ZenithScene` | `Zenith2/Docs/Design/SCENE/Prefabs.md` → "Authoring identities", "Document edits for the editor" |
| the scene and prefab document formats and their canonical schemas; the cooked scene and prefab payloads | `ZenithScene` | `Zenith2/Docs/Design/SCENE/Format.md` |
| prefab resolution, nested prefabs, override blocks and their order, variants, runtime instantiation, reference repair | `ZenithScene` | `Zenith2/Docs/Design/SCENE/Prefabs.md` |
| scene preparation and activation, the startup scene, additive scenes, the scene half of world transitions, carry-over capture, missing types at load | `ZenithScene` | `Zenith2/Docs/Design/SCENE/Loading.md` |
| the authoring-document migration framework, its one home under the Q43 ruling (registry, chain execution, preview, backup, journaled write, recovery, the unknown-member guard, low-disk refusal), canonical serialization, document paths and document pointers, and the host-filesystem storage the tools use | `ZenithDocument`, `ZenithDocumentHost` | `Zenith2/Docs/Design/SCENE/DocumentFramework.md` |
| the module seams: the activation hook STREAM implements on additive loads, the additive set's save enumeration and restore entry SAVE calls, the world-origin resource and its shift command, the fluid-query interface PHYS reads and ENV implements | `ZenithScene` | `Zenith2/Docs/Design/SCENE/ModuleSeams.md` |
| every SCENE test, fixture, fuzz target and benchmark | test and benchmark targets | `Zenith2/Docs/Design/SCENE/Tests.md` |

### What SCENE does not own

| Not owned | Owner | Why |
|---|---|---|
| the parent relation of entities, `SetParent` commands, the destroy cascade | ECS | cross-area note E9; `Zenith2/Docs/Design/ECS/Backends.md` → "Entity table" |
| component-level schema evolution (`Zenith_MigrationChain`, `Zenith_MigrateFieldSet`, schema ops, schema fixtures) | ECS | `Zenith2/Docs/Design/ECS/Schemas.md` → "Schema migration"; SCENE calls it inside documents |
| the lifecycle-command contract (publication points, the scenario tests) and D-PHASES | GAME | `Zenith2/Docs/Architecture/LifecycleCommands.md` → "Settled rules and ownership"; Z2-D-080 |
| world creation, the swap, the replay records of a transition | MOD (ZenithRuntime) | `Zenith2/Docs/Architecture/CompositionRoot.md` → "The frame loop"; SCENE supplies preparation and activation |
| cooked-container headers, the asset database and registry; the `.meta` document type with its importer-settings chains and fixtures, and the cook's integration with the framework | ASSET | [Z2-N-CONTRACT-550] delivers the framework "by SCENE and ASSET"; the split is `Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "Ownership split with ASSET" (cross-area row S15) |
| the migration UI, the editor's scene and prefab tools, undo, selection | EDIT | Z2-I-EDIT-01, -07, -15 to -17; SCENE exposes the document model, the edits and the preview data |
| the structure-aware 3-way merge driver, batch asset operations | CONTENT | Z2-I-CONTENT-27, -16 to -21; SCENE supplies canonical serialization (Z2-I-CONTENT-28) and repair plans |
| saves, their journaled writes and the replay log | SAVE | `Zenith2/Docs/Architecture.md` → "Multiplayer foundations"; SAVE may reuse `ZenithDocument`'s journaled write over a storage adapter of its own (cross-area row S17), which SCENE does not require |
| rendering; extraction of transforms into `Flux_RenderWorld` | RPROD and the modules' extractors | Flux never sees Scene ([Z2-N-ARCH-076]); extractors read SCENE's components |

Excluded outright: mod-supplied scenes and prefabs (Z2-D-075); dynamic plugins registering scene types (Z2-D-024); a second parent store or hierarchy component (cross-area note E9).

### Inventory items covered

SCENE covers every item of `Zenith2/Docs/Requirements/Inventory.md` → "Inventory v1", table "SCENE Scenes & prefabs": `Z2-I-SCENE-01` (stable authoring identities, S2), `-02` (nested prefabs, S4), `-03` (prefab overrides, S4), `-04` (prefab variants, S4), `-05` (reference repair, S4), `-06` (additive scenes, S3), `-07` (transactional loading, S3), `-08` (transform propagation, S2), `-09` (scene transitions, S3), `-10` (missing-type handling, S2). It also owns the CONTENT item `Z2-I-CONTENT-28` (canonical serialization, S2, "a Stage 2 SCENE requirement") and the document-level half of the migration framework that `Z2-I-ECS-03` (schema evolution, "migration framework, Z2-D-048") names, per [Z2-N-CONTRACT-550]. The mapping is `Zenith2/Docs/Requirements/SCENE.md` → "Coverage of inventory items".

### Document family

| File | Holds | Its `##` headings |
|---|---|---|
| `Zenith2/Docs/Design/SCENE/Transforms.md` | transforms, hierarchy, propagation | Transform value type · Transform components · Hierarchy cache · Propagation system · Teleport · Interpolation data · Reparenting helpers · Transform tests |
| `Zenith2/Docs/Design/SCENE/DocumentFramework.md` | ZenithDocument and ZenithDocumentHost | Why a separate library · Ownership split with ASSET · Document paths · Zenith_DocumentErrors header · Zenith_DocumentStorage header · Canonical serialization · Zenith_MigrationRegistry header · Zenith_DocumentMigrator header · Zenith_DocumentDiff header · Digests · Migration algorithms · Migration fixtures · ZenithDocumentHost |
| `Zenith2/Docs/Design/SCENE/Format.md` | document and cooked formats | Files and virtual paths · Scene document schema · Scene canonical schema · Unknown components · Cooked scene payload · Scene diagnostic dump |
| `Zenith2/Docs/Design/SCENE/Prefabs.md` | identities, prefabs, repair, edits | Authoring identities · Resolution algorithm · Variants · Prefab provider · Cooking resolved prefabs · Runtime instantiation · Reference repair · Document edits for the editor |
| `Zenith2/Docs/Design/SCENE/Loading.md` | loading, additive scenes, transitions | Principles · Preparation operation · Activation paths · Startup scene · Scene transitions · Additive scenes · Publication service · Cancellation · In-flight asynchronous results across a transition · Missing-type handling at load |
| `Zenith2/Docs/Design/SCENE/SceneAPI.md` | every ZenithScene header | Header list · Zenith_SceneErrors header · Zenith_TransformHierarchy header · Zenith_LifecycleListener header · Zenith_LifecycleCommands header · Zenith_AuthoringID header · Zenith_SceneDocument header · Zenith_SceneEdit header · Zenith_SceneData header · Zenith_PrefabResolver header · Zenith_SceneCooker header · Zenith_SceneAssets header · Zenith_SceneLoader header · Zenith_AdditiveScenes header · Zenith_SceneCapture header · Zenith_ReferenceRepair header · Zenith_SceneDocumentTypes header · Zenith_SceneWorld header · Script bindings |
| `Zenith2/Docs/Design/SCENE/ModuleSeams.md` | the activation hook, the additive-set save entries, the world origin, the fluid query (the 2026-10-09 post-signoff patch: rows ST2, ST3, SV4, EN26, PH8, PH20), the collision query (2026-10-17: rows GA2 (a), GA3 (a)) | Scope of the module seams · Activation hook declarations · Activation hook obligations · Additive set save and restore · Zenith_WorldOrigin header · Zenith_FluidQuery header · Zenith_CollisionQuery header · Module seam diagnostics |
| `Zenith2/Docs/Design/SCENE/Tests.md` | every test | Test files · Canonical serialization tests · Document framework tests · Host storage tests · Transform and hierarchy tests · Document and identity tests · Prefab tests · Reference repair tests · Cook and load tests · Diagnostics and binding tests · Integration and scenario tests · Module seam tests · Golden fixtures · Benchmarks and the budget harness · Gate steps per test group |

### Reconciliations with the specification

Additions SCENE makes to core documents, each carried by cross-doc item X109 (pending the core-doc batch that follows SCENE round 1; part (a) additionally follows the Q43 ruling) and each fenced in this family by an `assumption: pending X109(<part>)` block:

| # | Item | Specification touched | X109 part |
|---|---|---|---|
| S-X1 | the new libraries `ZenithDocument` (engine, layer 2) and `ZenithDocumentHost` (tool, `ZENITH_HOST`), and `ZenithScene`'s edges to `ZenithDocument` and `ZenithCompression`; the justification is `Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "Why a separate library" | `Zenith2/Docs/Architecture.md` → "Library dependency graph", "Repository layout"; `Zenith2/Docs/Architecture/BuildSystem.md` → "The ZENITH_SUPPORTED matrix" (tool table) | (a) |
| S-X2 | prefab instantiation expands into ordinary spawn records at record time, so it plays at step 6 as `Zenith2/Docs/Architecture/FramePhases.md` → "Commands" lists, rather than as a registered command kind as the settled-rules paragraph of LifecycleCommands.md says; teleport and the other SCENE commands are registered kinds | `Zenith2/Docs/Architecture/LifecycleCommands.md` → "Settled rules and ownership" | (c) |
| S-X3 | `Zenith_Transform` is the value type; the components are `Zenith_LocalTransform`, `Zenith_WorldTransform` and `Zenith_PreviousWorldTransform`; the example of `Zenith2/Docs/Architecture/CPUFrameGraph.md` → "Declarations at the ECS level" reads `Zenith_WorldTransform::xTYPE_ID` with `uPROP_POSITION`, `uPROP_ROTATION` | CPUFrameGraph.md | (c) |
| S-X4 | prologue step 7 covers the whole world-transition work in order: creating each isolated world whose preparation succeeded (the table "Schedule recompile triggers" already places world creation there), sliced activation within `scene.activationBudgetUs`, then the swap; the eight-step count of [Z2-N-ARCH-437] is unchanged | `Zenith2/Docs/Architecture/CPUFrameGraph.md` → "The frame prologue"; `Zenith2/Docs/Architecture/LifecycleCommands.md` → "World transition" | (b) |
| S-X5 | `ASYNC_SOURCE_SCENE_LOAD = 8`, now assigned in SCRIPT's registry (`Zenith2/Docs/Scripting.md` → "Asynchronous source kind registry", [Z2-N-SCRIPT-924], cross-doc item X211, applied); SCENE cites the value and declares none | `Zenith2/Docs/Scripting.md`; `Zenith2/Docs/Architecture.md` → "Multiplayer foundations" | (c), superseded by X211 |
| S-X6 | the journal of a batch is written at the storage root (`Games/<Name>/`), not beside each document, so one batch may span directories and recovery scans one location | `Zenith2/Docs/Contracts.md` → "Compatibility policy per kind" (table "Journaled atomic write", step 1) | (d) |

## Dependencies

### Libraries SCENE depends on

| Library | Layer | `DEPENDS` | Third party |
|---|---|---|---|
| `ZenithDocument` | 2 | ZenithCore, ZenithReflect, ZenithProfiling | — |
| `ZenithScene` | 4 | ZenithCore, ZenithReflect, ZenithECS, ZenithAssets, ZenithJobs, ZenithProfiling (the row of `Zenith2/Docs/Architecture.md` → "Library dependency graph"), plus ZenithDocument and ZenithCompression (reconciliation S-X1) | — |
| `ZenithDocumentHost` | tool library (`zenith_add_tool`, `SUPPORTED ZENITH_HOST`) | ZenithDocument, ZenithCore | — (the STL in its own sources, [Z2-N-CONTRACT-275]) |

What each edge is for: ZenithCore — results, owned pointers, arenas, containers, `Zenith_PersistentID`, the JSON reader and writer, hashing (`Zenith_Hash64`, and `Zenith_Sha256` for digests and authoring-ID derivation), maths, the memory governor, logging, fault injection; ZenithReflect — type IDs, type info, field sets, the reflection walk, the migration types, phases and arbitration declarations; ZenithECS — worlds, command buffers, command kinds, systems, observers, the inspector; ZenithAssets — asset handles, the registry and its loader interface; ZenithJobs — background operations, `ParallelFor`, continuations; ZenithProfiling — zones and counters; ZenithDocument — the framework; ZenithCompression — the codec IDs the cooker passes to ASSET's writer and the section decompression of `Zenith_DecodeCookedSceneFile`.

### Headings this design relies on

| Heading | For |
|---|---|
| `Zenith2/Docs/Design/ECS/WorldAPI.md` → "Zenith_World header", "Zenith_CommandBuffer header", "Zenith_CommandKinds header", "Zenith_WorldSimulation header", "Zenith_WorldInspector header", "Zenith_ECSObservers header", "Zenith_EntityTable header"; `Zenith2/Docs/Design/ECS/SystemAPI.md` → "Zenith_System header", "Zenith_SystemContext header", "Zenith_MergeContext header" | worlds, commands, systems, observers, inspection |
| `Zenith2/Docs/Design/ECS/Scheduling.md` → "Sync-point playback", "Out-of-tick playback", "Tick boundary and end-of-tick" | when SCENE's playback functions and observer run |
| `Zenith2/Docs/Design/ECS/Queries.md` → "Change versions and merge versions", "Change-filtered queries" | transform change bits |
| `Zenith2/Docs/Design/ECS/Reflect.md` → "Zenith_FieldSet header", "Zenith_ReflectionWalk header", "Zenith_Migration header", "JSON field-set form", "Binary field-set record", "Zenith_TypeBuilder" | serialization and migration |
| `Zenith2/Docs/Design/ECS/Schemas.md` → "Schema migration" | component chains inside documents |
| `Zenith2/Docs/Design/CORE/Text.md` → "JSON writer", "JSON reader", "String interning" | documents and canonical bytes |
| `Zenith2/Docs/Design/CORE/Maths.md` → "Coordinate systems", "Zenith_Quaternion", "Zenith_Hash64", "Zenith_PersistentID functions" | transforms and IDs |
| `Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_Sha256", "Zenith_Sha256 declarations" | digests and authoring-ID derivation (`Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "Digests") |
| `Zenith2/Docs/Design/CORE/Memory.md` → "Fault-injection support" | factory failure tests |
| `Zenith2/Docs/Design/JOBS/PublicAPI.md` → "Zenith_JobContext header", "Zenith_JobSystem header", "Zenith_SimulationClock header" | background preparation, continuations, alpha |
| `Zenith2/Docs/Design/COMPRESS.md` → "How cooked formats embed the descriptor" | cooked sections |
| `Zenith2/Docs/Design/MOD.md` → "Startup scene and loose content root"; `Zenith2/Docs/Design/MOD/ProjectCreation.md` → "The written scene", "game.json migration" | the startup scene, templates, `game.json` on the framework |
| `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Escape check after resolution" | the host storage's escape check |
| `Zenith2/Docs/Contracts.md` → "Compatibility policy per kind", "Handle kinds", "Errors and results", "Threading and concurrency contract" | the contracts every SCENE type obeys |
| `Zenith2/Docs/Architecture/LifecycleCommands.md` → "World transition", "Teleport", "Spawn", "Listener interface" | the lifecycle contract |
| `Zenith2/Docs/Scripting.md` → "Merge-time capacity and asynchronous results", "Behaviour state model and recovery" | publication and what documents store for behaviours |
| `Zenith2/Docs/Design/ASSET.md` → "Migration hooks" | the `.meta` type and the cook's side of the framework (the split of `Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "Ownership split with ASSET") |
| `Zenith2/Docs/Design/ASSET.md` → "Asset kinds"; `Zenith2/Docs/Design/ASSET/RuntimeAPI.md` → "Zenith_AssetHandle", "Zenith_AssetRegistry", "Zenith_AssetKindLoader", "Zenith_AssetLoader", "Zenith_CookedFile header", "Zenith_CookedFileWriter"; `Zenith2/Docs/Design/ASSET/Formats.md` → "Cooked asset file"; `Zenith2/Docs/Design/ASSET/Loading.md` → "Publication to simulation" | the two asset kinds, their loaders and handles, the cooked container (ASSET's answer AS11 to S7) |
| `Zenith2/Docs/Design/ASSET/Cook.md` → "Cook services and composition", "Zenith_CookStepDesc", "Zenith_CookStep", "Zenith_CookContext", "Zenith_CookStepRegistry", "Engine fallbacks", "Plan construction"; `Zenith2/Docs/Design/ASSET/DDC.md` → "Key inputs"; `Zenith2/Docs/Design/ASSET/Loading.md` → "Dependency loading"; `Zenith2/Docs/Design/ASSET/RuntimeAPI.md` → "Zenith_AssetLoadContext" | SCENE's two cook steps, their keys and hard references, and the dependency handles the kind loaders receive |
| `Zenith2/Docs/Design/SCRIPT/Runtime.md` → "Asynchronous result table"; `Zenith2/Docs/Design/SCRIPT/PublicAPI.md` → "Zenith_ScriptHost header" | the per-world publication state `Zenith_ScenePublicationSink` maps onto, and the handler access the publish callback receives |
| `Zenith2/Docs/Design/ECS/Scheduling.md` → "Authority and schedule checks" | the authority violation the local-transform test asserts |
| `Zenith2/Docs/Architecture/CPUFrameGraph.md` → "The frame prologue", "Schedule compilation and caching" | prologue step 7 and the world-creation precedent (X109(b)) |
| `Zenith2/Docs/Architecture/BuildSystem.md` → "The zenith_add_library helper", "Tools, benchmarks and integration suites", "The ZENITH_SUPPORTED matrix" | the three targets' declarations, test and fuzz targets |

### Upward-callback interfaces SCENE declares

| Interface | Declared in | Implemented by | Called |
|---|---|---|---|
| `Zenith_LifecycleListener` | `ZenithScene/Zenith_LifecycleListener.h` | Physics, Animation, Cinematics, Streaming modules; the runtime's extraction state | sync-point step 7 and structural events |
| `Zenith_ScenePublicationSink` | `ZenithScene/Zenith_AdditiveScenes.h` | ZenithRuntime (per world) | additive load commit and completion; from Stage 4 also the discard resolver it forwards to SCRIPT (`Zenith2/Docs/Design/SCENE/ModuleSeams.md` → "Discarded publications of tagged loads") |
| `Zenith_SceneActivationHook` | `ZenithScene/Zenith_AdditiveScenes.h` | the Streaming module (per world, through `SetActivationHook`) | commit, preparation (gate, skip, amend, end), activation (admit, root translation), discarded publication, restore re-request (`Zenith2/Docs/Design/SCENE/ModuleSeams.md` → "Hook call points") |
| `Zenith_FluidQuery` | `ZenithScene/Zenith_FluidQuery.h` | the Environment module (per world, added as the resource `Zenith.FluidQuery`) | read by the Physics module's buoyancy from phases 2 to 12 (`Zenith2/Docs/Design/SCENE/ModuleSeams.md` → "Zenith_FluidQuery header") |
| `Zenith_CollisionQuery` | `ZenithScene/Zenith_CollisionQuery.h` | the Physics module (per world, added as the resource `Zenith.CollisionQuery`) | read by GAME's `Gameplay.Cameras` in phase 7 (`Zenith2/Docs/Design/SCENE/ModuleSeams.md` → "Zenith_CollisionQuery header") |
| `Zenith_SceneTransitionSink` | `ZenithScene/Zenith_SceneWorld.h` | ZenithRuntime | `Scene.RequestTransition` playback |
| `Zenith_PrefabProvider` | `ZenithScene/Zenith_PrefabResolver.h` | ZenithEditor, ZenithCook, tests | resolution |
| `Zenith_DocumentStorage` | `ZenithDocument/Zenith_DocumentStorage.h` | `ZenithDocumentHost`, `Zenith_MemoryDocumentStorage` | every framework read and write |
| `Zenith_DocumentWriterLock` | `ZenithDocument/Zenith_DocumentStorage.h` | `ZenithDocumentHost` (the lock file of `Zenith2/Docs/Design/ASSET/Formats.md` → "Cook lock file") | around every recovery, plan application and save batch over a writable host storage; ZenithCook's sidecar-writing modes for a run or a request (cross-area row AS31) |

SCENE implements ECS's `Zenith_StructuralObserver` (`Zenith2/Docs/Design/SCENE/Transforms.md` → "Hierarchy maintenance at playback") and `Zenith_TickBoundaryHandler` (`Zenith2/Docs/Design/SCENE/Loading.md` → "Tick-boundary handler"), ZenithAssets' `Zenith_AssetKindLoader` twice (`Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_SceneAssets header"), and `Zenith_ReferenceResolver` of ZenithReflect for capture and activation.

### Areas that consume SCENE

| Consumer | Calls |
|---|---|
| MOD (the composition root) | `Zenith_RegisterSceneTypes` (step 6), `Zenith_SceneServices::Create`, `Zenith_RegisterSceneAssetKinds`, `Zenith_SceneLoader::Create` (step 11), `Zenith_RegisterSceneCommandKinds` (step 15), `Zenith_SceneWorldState::Create`/`Attach` (step 16), `LoadStartupScene` (step 17), `StepActivations` (prologue step 7), `Zenith_CaptureEntities` (swaps), `Zenith_SceneWorldState::AcceptScriptLoadRequest` (the asynchronous-request multiplexer, source kind 8) |
| EDIT | `Zenith_SceneDocument`, `Zenith_SceneEdit`, `Zenith_ApplySceneEdit`, `Zenith_MakeSortKeyBetween`, `Zenith_MakeEvenSortKeys` (cross-area row S20), `Zenith_ResolveDocument`, `Zenith_ActivateSceneData`, `Zenith_CaptureWorld`, `Zenith_DocumentMigrator` (preview, apply, save, recover), `Zenith_ReferenceIndex` |
| ASSET (cook) | `Zenith_RegisterSceneCookSteps` (cook services item 6), `Zenith_CookSceneDocument` through those steps, `Zenith_RegisterSceneAssetKinds`, `Zenith_RegisterSceneDocumentTypes` |
| CONTENT | `Zenith_PlanReferenceRepair`, the canonical schemas for the merge driver |
| ASSET (`.meta`), MOD (`game.json`), PLAT, GEOM, ANIM, UI, CINE, ECS (schema docs) | `ZenithDocument` for their document types: `RegisterDocumentType`, `Zenith_RegisterMigrationStep`, fixtures (cross-area rows S15, S16) |
| SAVE | `Zenith_CaptureEntities`, `Zenith_ActivateSceneData`, `Zenith_AdditiveSceneSet::ForEachInstance` and `RestoreInstances` and `IsCaptureCoherentOnly` for its `Zenith.Scene.AdditiveSet` participant (row SV4); the world-origin resource through its resources section; the journaled write is offered, not required (cross-area row S17) |
| STREAM | `Zenith_SceneActivationHook` and the hook tag, `GetLiveInstances`, `GetErrorDetail`, `Zenith_RecordShiftWorldOrigin`, `Zenith.WorldOrigin`, the preparation hold of `Tests/Support/` (rows ST2, ST3) |
| PHYS, ENV, AI, AUDIO, RPROD | `Zenith_FluidQuery` (ENV implements, PHYS reads; rows EN26, PH8); `LIFECYCLE_EVENT_ORIGIN_SHIFTED` and `Zenith.WorldOrigin` (PHYS row PH20; STREAM's "Who reacts to a rebase") |
| PHYS, GAME | `Zenith_CollisionQuery` and `Zenith.CollisionQuery` (PHYS implements, GAME's cameras read; rows GA2 (a), GA3 (a)) |
| SCRIPT | SCENE's bindings; the behaviour attachment is a component like any other |
| every module and extractor | the transform components, `Zenith_InterpolateWorldTransform`, the lifecycle listener |

### Assumed interfaces from other areas

| Assumption | Owner | Cross-area row |
|---|---|---|
| `STRUCTURAL_EVENT_PARENT_CHANGED` and cycle rejection of `SET_PARENT` | ECS | S3 |
| ticket handles inside ENTITY-typed property values of records of the same buffer | ECS | S8 |
| `Zenith_MigrationDiagnostics` members, including `m_pfnDeclareRemoved` | ECS | S6, S18 |
| `Zenith_DocumentTree` in ZenithCore (settled: declared in `Zenith2/Docs/Design/CORE/Text.md` → "JSON document tree") | CORE | S5 |
| the document-pointer grammar of `Zenith_DocumentTree::FindPath` (settled: rules G1 to G4 of the same section; a malformed pointer is `JSON_ACCESS_ERROR_INVALID_POINTER`, a token on a scalar `JSON_ACCESS_ERROR_WRONG_KIND`) | CORE | S5 |
| ZenithRuntime's implementation of `Zenith_ScenePublicationSink` over SCRIPT's asynchronous result table; a script load's result kept for its ticket and decoded; the multiplexer's routing of source kind 8 | MOD, SCRIPT | S11, S21, S22 |
| prologue step 7 scope; `ASYNC_SOURCE_SCENE_LOAD`; the library rows; the journal at the storage root | ARCH, MOD, CONTRACT | X109(a) to (d); S9, S10, S19 |
| the local-transform authority | GAME, PHYS, CINE | S1, S2 |
| `Zenith_CookContext::OpenSourceDocument`, the cook steps' document access | ASSET | S24 |
| the runtime sink's forwarding of `SetPublicationResolver` to SCRIPT's table, and SCRIPT's live-mode resolver call and replay drop rule | MOD, SCRIPT | ST21 (core-doc item X271) |
| a play-last command-kind flag and `Zenith_WorldPlaybackAccess::GetPlaybackVersion` for the origin shift; `PROPERTY_FLAG_WORLD_POSITION` | ECS | S25; ST4 (core-doc item X227 (a)) |
| `LIFECYCLE_EVENT_ORIGIN_SHIFTED` in Architecture's enumeration | ARCH | PH20 (core-doc item X264 (j)) |
| the root `Zenith_AssetErrorCode` of a failed hard dependency | ASSET | ST22 |

<!-- informative -->
What a scene document stores for behaviours: a behaviour is attached by the component that names its module (`Zenith2/Docs/Scripting.md` → "Behaviours and chunk systems": "one per entity carrying a Behaviour component that names the module"), stored like every component in the entity's `components` map. A behaviour's declared state is not authored in scenes: it is created from the declaration's defaults when the instance is created, and only checkpoints and saves serialize it (`Zenith2/Docs/Scripting.md` → "Behaviour state model and recovery"). Zenith stored scene entities in a binary `.zscen` with a per-component `WriteToDataStream`; Zenith2 stores reflected field sets in canonical JSON, so a struct change never breaks a document (Z2-D-038).
<!-- /informative -->

## Library and file layout

### ZenithDocument target

```cmake
# Zenith2/Engine/ZenithDocument/CMakeLists.txt — the whole file
zenith_add_library(ZenithDocument DEPENDS ZenithCore ZenithReflect ZenithProfiling)
```

<!-- assumption: pending X109(a) -->
`KIND ENGINE`, `SUPPORTED ZENITH_ALL`, `EXCEPTIONS OFF` (the defaults of `Zenith2/Docs/Architecture/BuildSystem.md` → "The zenith_add_library helper"). The Architecture row this library needs is cross-doc item X109(a).
<!-- /assumption -->

Public headers: `Zenith_DocumentErrors.h`, `Zenith_DocumentPath.h` (`Zenith_ValidateStoragePath`, `Zenith_ValidateDocumentPath`, the document-pointer functions), `Zenith_DocumentStorage.h`, `Zenith_CanonicalJson.h`, `Zenith_MigrationRegistry.h`, `Zenith_DocumentMigrator.h`, `Zenith_DocumentDiff.h`; digests come from CORE (`Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "Digests"). Internal files: `Internal/Zenith_DocumentPath.cpp`, `Internal/Zenith_MemoryDocumentStorage.cpp`, `Internal/Zenith_CanonicalJson.cpp`, `Internal/Zenith_UnknownMemberGuard.cpp`, `Internal/Zenith_MigrationRegistry.cpp`, `Internal/Zenith_DocumentMigrator.cpp`, `Internal/Zenith_JournaledWrite.cpp`, `Internal/Zenith_JournalFormat.cpp`, `Internal/Zenith_DocumentDiff.cpp`, `Internal/Zenith_DocumentNames.cpp` (`Zenith_ToString`, log category `document`). No platform files. Tests: `Zenith2/Engine/ZenithDocument/Tests/` into `ZenithDocument_Tests`, and `Tests/Fuzz/DocumentJournal.cpp` into the fuzz target `Zenith_Fuzz_DocumentJournal` (`Zenith2/Docs/Design/SCENE/Tests.md` → "Test files").

### ZenithScene target

```cmake
# Zenith2/Engine/ZenithScene/CMakeLists.txt — the whole file
zenith_add_library(ZenithScene
	DEPENDS ZenithCore ZenithReflect ZenithECS ZenithAssets ZenithJobs ZenithProfiling ZenithDocument ZenithCompression
	TEST_DEPENDS ZenithECS_Custom ZenithECS_Flecs ZenithECS_EnTT)
```

`KIND ENGINE`, `SUPPORTED ZENITH_ALL`, `EXCEPTIONS OFF` (the defaults of `Zenith2/Docs/Architecture/BuildSystem.md` → "The zenith_add_library helper"); the two added edges are X109(a). `TEST_DEPENDS` links the three ECS backends into `ZenithScene_Tests` and nowhere else, because ZenithScene declares the ECS API only and a test that creates a world needs a backend factory: a test creates its world with `Zenith_CreateWorld_Custom` unless its row in `Zenith2/Docs/Design/SCENE/Tests.md` says "every backend", in which case it runs once per factory (`Zenith_CreateWorld_Custom`, `_Flecs`, `_EnTT`) in doctest subcases. `ZenithDocument_Tests` and `ZenithDocumentHost_Tests` create no world and link no backend. Public headers: `Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Header list". Internal files:

| File | Contents |
|---|---|
| `Internal/Zenith_Transform.cpp`, `Internal/Zenith_TransformComponents.cpp` | transform maths, registration |
| `Internal/Zenith_TransformHierarchy.cpp`, `Internal/Zenith_SceneStructuralObserver.cpp` | the cache and its maintenance |
| `Internal/Zenith_ScenePropagate.cpp`, `Internal/Zenith_ScenePropagate.h` | `Scene.PropagateTransforms` (run and merge functions, `Zenith_ScenePropagationState`, the test-only `Zenith_ScenePropagationTestAccess` reader) and out-of-tick propagation |
| `Internal/Zenith_SceneCommands.cpp` | the command-kind registration and every playback function |
| `Internal/Zenith_LifecycleDispatcher.cpp` | listener registry and dispatch |
| `Internal/Zenith_AuthoringID.cpp` | derivation (over CORE's `Zenith_Sha256Digest`) and remapping |
| `Internal/Zenith_SceneDocument.cpp`, `Internal/Zenith_SceneDocumentValidate.cpp`, `Internal/Zenith_SceneEdit.cpp` | the document model, validation, edits |
| `Internal/Zenith_SceneDocumentTypes.cpp` | canonical schemas, the document-type registration, the field-set visitor |
| `Internal/Zenith_PrefabResolve.cpp`, `Internal/Zenith_PrefabInstantiate.cpp` | resolution and runtime instantiation |
| `Internal/Zenith_SceneCooker.cpp`, `Internal/Zenith_SceneCookedDecode.cpp` | cook and decode |
| `Internal/Zenith_SceneAssets.cpp` | the two asset kinds and their two kind loaders |
| `Internal/Zenith_SceneLoader.cpp`, `Internal/Zenith_ScenePrepare.cpp`, `Internal/Zenith_SceneActivate.cpp` | preparation, activation |
| `Internal/Zenith_AdditiveScenes.cpp` | the additive set, `Scene.ActivateAdditive`, the publication handler, the tick-boundary handler, the script-load inbox, `Zenith_SceneLoadResult` coding |
| `Internal/Zenith_AdditiveSceneSave.cpp` | `ForEachInstance`, `RestoreInstances` (Stage 3) |
| `Internal/Zenith_AdditiveSceneHook.cpp` | the hook call points, `Zenith_SceneComponentAmender`, the discard flag and resolver, the coherence input (Stage 4) |
| `Internal/Zenith_WorldOrigin.cpp` | `Zenith_RecordShiftWorldOrigin`, the `Scene.ShiftWorldOrigin` playback and its scratch (Stage 4) |
| `Internal/Zenith_SceneCapture.cpp` | capture |
| `Internal/Zenith_ReferenceRepair.cpp`, `Internal/Zenith_ReferenceIndex.cpp` | repair |
| `Internal/Zenith_SceneWorld.cpp`, `Internal/Zenith_SceneServices.cpp` | per-world and process state |
| `Internal/ScriptBindings/Zenith_SceneScriptBindings.cpp` | `Zenith_RegisterSceneScriptBindings` and the bound functions (`ZenithReflect/Zenith_ScriptBinding.h` only) |
| `Internal/Zenith_SortKey.cpp` | `Zenith_MakeSortKeyBetween`, `Zenith_MakeEvenSortKeys`, `Zenith_IsValidSortKey` |
| `Internal/Zenith_SceneNames.cpp` | `Zenith_ToString`, counter names, log categories |

No platform files. Tests: `Zenith2/Engine/ZenithScene/Tests/` into `ZenithScene_Tests`; `Tests/Fuzz/SceneCooked.cpp` and `Tests/Fuzz/SceneDocument.cpp` into `Zenith_Fuzz_SceneCooked` and `Zenith_Fuzz_SceneDocument`.

### ZenithDocumentHost target

```cmake
# Zenith2/Tools/DocumentHost/CMakeLists.txt — the whole file
zenith_add_tool(ZenithDocumentHost DEPENDS ZenithDocument ZenithCore SUPPORTED ZENITH_HOST)
```

A static tool library: the directory has no `Internal/Zenith_ZenithDocumentHostMain.cpp` (`Zenith2/Docs/Architecture/BuildSystem.md` → "Tools, benchmarks and integration suites"). Include root `Zenith2/Tools` ([Z2-N-ARCH-164]), so its header is `DocumentHost/Zenith_HostDocumentStorage.h`. `SUPPORTED ZENITH_HOST` (`win`, `win-asan`, `linux-tsan`) replaces the tool default `ZENITH_TOOLS_HOST`, so its tests run under ASan and TSan. `Tests/**` becomes `ZenithDocumentHost_Tests` by the glob partition the sibling helpers share with `zenith_add_library`. Files and the rationale: `Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "ZenithDocumentHost". Linked by ZenithEditor, ZenithCook and ZenithNewGameScaffold; a tool target, so outside every Shipping link closure ([Z2-N-ARCH-241]). The Architecture row is X109(a).

### Test and benchmark targets

| Target | Kind | Directory | Links |
|---|---|---|---|
| `ZenithDocument_Tests`, `ZenithScene_Tests`, `ZenithDocumentHost_Tests` | test | each library's `Tests/` | the library, `ZenithTestSupport`, doctest; `ZenithScene_Tests` also the three ECS backends through the `TEST_DEPENDS` of its call above |
| `Zenith_Fuzz_SceneCooked`, `Zenith_Fuzz_SceneDocument` (ZenithScene), `Zenith_Fuzz_DocumentJournal` (ZenithDocument) | fuzz (`win-asan`), one per `Tests/Fuzz/<Stem>.cpp` | `Tests/Fuzz/SceneCooked.cpp`, `Tests/Fuzz/SceneDocument.cpp`, `Tests/Fuzz/DocumentJournal.cpp` | the library, `ZenithTestSupport`, libFuzzer |
| `Zenith_Integration_Scene` | integration | `Zenith2/Tests/Scene/` | ZenithRuntime and the three ECS backends; `SUPPORTED` `win`, `win-asan` |
| `Zenith_Bench_Scene` | benchmark | `Zenith2/Benchmarks/Scene/` | ZenithScene, the custom backend, nanobench |

### Supported matrix

ZenithDocument and ZenithScene are `ZENITH_ALL` (`win`, `win-asan`, `linux-tsan`, `android-arm64`, `android-x86_64`), so `linux-tsan` runs every SCENE unit test under TSan and UBSan; ZenithDocumentHost is `ZENITH_HOST` (`win`, `win-asan`, `linux-tsan`; tools never run on Android). On Android presets the test executables build and are not registered with CTest; the S24 run is part of the Stage 3 acceptance, on MOD's test-APK route with the fixture trees pushed to each test APK's device directory ([Z2-N-ARCH-250]).

## Public API

### API rules

- Every fallible public function returns `Zenith_Result<T, E>` with `E` one of `Zenith_SceneError`, `Zenith_DocumentError`, or (for Contracts' step registration) ECS's `Zenith_MigrationError`, each an error record of [Z2-N-CONTRACT-480].
- Every public type has one contract block; the declarations are in the split files with their blocks.
- Every SCENE entry point is type-erased through reflection IDs; the transform components' C++ structs are the only C++ component types SCENE names, and data-defined components pass through the same paths ([Z2-N-CONTRACT-376]).
- No public header includes a platform header, a third-party header or an STL header outside the allowed list.
- Serialization goes through reflected field sets, never a layout copy (Z2-D-038, [Z2-N-CONTRACT-377]).

### ZenithDocument API summary

| Type or function | Header | Thread affinity | Purpose |
|---|---|---|---|
| `Zenith_DocumentError`, `Zenith_DocumentWarningCode` | `Zenith_DocumentErrors.h` | IMMUTABLE | errors and warnings |
| `Zenith_ValidateStoragePath`, `Zenith_ValidateDocumentPath`, `Zenith_ValidateDocumentPointer`, `Zenith_AppendDocumentPointerToken`, `Zenith_AppendDocumentPointerIndex` | `Zenith_DocumentPath.h` | ANY_THREAD | path rules (storage: P1 to P5; documents: P1 to P6) and the document-pointer grammar (D1 to D4) |
| `Zenith_DocumentStorage`, `Zenith_MemoryDocumentStorage` | `Zenith_DocumentStorage.h` | EXTERNALLY_SYNCHRONISED | byte storage |
| `Zenith_CanonicalSchema`, `Zenith_WriteCanonicalJson`, `Zenith_IsCanonicalJson`, `Zenith_RenameMemberInPlace` | `Zenith_CanonicalJson.h` | ANY_THREAD | canonical serialization (Z2-I-CONTENT-28) |
| `Zenith_MigrationRegistry`, `Zenith_DocumentTypeDesc`, `Zenith_RegisterMigrationStep` | `Zenith_MigrationRegistry.h` | MAIN_THREAD then IMMUTABLE | types and chains |
| `Zenith_DocumentMigrator`, `Zenith_OpenedDocument`, `Zenith_MigrationPlan`, `Zenith_DocumentWrite` | `Zenith_DocumentMigrator.h` | OWNER_THREAD | open, preview, apply, save, recover |
| `Zenith_DocumentDiff` | `Zenith_DocumentDiff.h` | IMMUTABLE | preview diffs |
| `Zenith_CreateHostDocumentStorage` | `DocumentHost/Zenith_HostDocumentStorage.h` (tool include root `Zenith2/Tools`) | MAIN_THREAD | host storage (tools) |

### ZenithScene API summary

| Type or function | Header | Thread affinity | Purpose |
|---|---|---|---|
| `Zenith_Transform`, the transform components | `Zenith_Transform.h`, `Zenith_TransformComponents.h` | IMMUTABLE | Z2-I-SCENE-08 |
| `Zenith_TransformHierarchy`, `Zenith_InterpolateWorldTransform` | `Zenith_TransformHierarchy.h` | EXTERNALLY_SYNCHRONISED / ANY_THREAD | hierarchy, interpolation |
| `Zenith_LifecycleListener`, `Zenith_LifecycleDispatcher` | `Zenith_LifecycleListener.h` | sync-point node | lifecycle notifications |
| `Zenith_RecordTeleport`, `Zenith_RecordInstantiatePrefab`, `Zenith_RecordInstantiatePrefabEx`, `Zenith_RecordSetParentKeepWorld`, `Zenith_RecordRequestSceneTransition` | `Zenith_LifecycleCommands.h` | the buffer's | lifecycle commands |
| `Zenith_DeriveAuthoringID`, `Zenith_RemapAuthoringIDs` | `Zenith_AuthoringID.h` | ANY_THREAD | Z2-I-SCENE-01 |
| `Zenith_SceneDocument`, `Zenith_SceneEdit`, `Zenith_ApplySceneEdit`, `Zenith_MakeSortKeyBetween` | `Zenith_SceneDocument.h`, `Zenith_SceneEdit.h` | OWNER_THREAD | the document model, edits and sort keys |
| `Zenith_SceneData`, `Zenith_ResolveDocument`, `Zenith_PrefabProvider` | `Zenith_SceneData.h`, `Zenith_PrefabResolver.h` | OWNER_THREAD | Z2-I-SCENE-02 to -04 |
| `Zenith_CookSceneDocument`, `Zenith_DecodeCookedScene`, `Zenith_DecodeCookedSceneFile`, `Zenith_Scene`, `Zenith_Prefab`, `Zenith_RegisterSceneAssetKinds` | `Zenith_SceneCooker.h`, `Zenith_SceneAssets.h` | calling thread | cooked scenes and their asset kinds |
| `Zenith_SceneLoader`, `Zenith_PreparedScene`, activation functions | `Zenith_SceneLoader.h` | MAIN_THREAD | Z2-I-SCENE-07, -09 |
| `Zenith_AdditiveSceneSet`, `Zenith_RecordLoadAdditiveScene`, `Zenith_RecordUnloadAdditiveScene`, `Zenith_SceneLoadResult` | `Zenith_AdditiveScenes.h` | per member | Z2-I-SCENE-06 |
| `Zenith_CaptureEntities`, `Zenith_CaptureWorld` | `Zenith_SceneCapture.h` | MAIN_THREAD out of frame | carry-over and play copy |
| `Zenith_PlanReferenceRepair`, `Zenith_ReferenceIndex` | `Zenith_ReferenceRepair.h` | OWNER_THREAD | Z2-I-SCENE-05 |
| `Zenith_RegisterSceneDocumentTypes` | `Zenith_SceneDocumentTypes.h` | MAIN_THREAD | the two document types |
| `Zenith_SceneServices`, `Zenith_SceneWorldState` | `Zenith_SceneWorld.h` | MAIN_THREAD | process and world state |
| `Zenith_SavedSceneInstance`, `ForEachInstance`, `RestoreInstances` | `Zenith_AdditiveScenes.h` | as `Zenith_AdditiveSceneSet`; restore MAIN_THREAD out of tick | SAVE's additive-set participant (Stage 3) |
| `Zenith_SceneActivationHook`, `Zenith_SceneComponentAmender`, `SetPublicationResolver`, `SetPreparationProbe` | `Zenith_AdditiveScenes.h`, `Zenith_SceneLoader.h` | per member | STREAM's seam on additive loads (Stage 4) |
| `Zenith_WorldOrigin`, `Zenith_RecordShiftWorldOrigin` | `Zenith_WorldOrigin.h` | resource readers; the buffer's | large coordinates (Stage 4) |
| `Zenith_FluidSample`, `Zenith_FluidQuery` | `Zenith_FluidQuery.h` | ANY_THREAD for declared readers | the fluid query (Stage 4) |
| `Zenith_CollisionSweepDesc`, `Zenith_CollisionHit`, `Zenith_CollisionQuery` | `Zenith_CollisionQuery.h` | ANY_THREAD for declared readers | the camera collision query (Stage 3) |

### Handle kinds used

| Handle | Kind (`Zenith2/Docs/Contracts.md` → "Handle kinds") | SCENE's role |
|---|---|---|
| `Zenith_PersistentID` | persistent ID | authoring IDs of documents, entities and instances; asset references in documents and cooked data |
| `Zenith_EntityHandle` | world handle | resolved at activation; never serialized |
| `Zenith_AssetHandle<Zenith_Scene>`, `<Zenith_Prefab>` and component asset handles | shared asset handle | held by prepared scenes and resident prefabs |
| `Zenith_OperationHandle` | operation handle | a preparation; destroying it cancels |
| `Zenith_SceneInstanceID`, `Zenith_SceneLoadTicket` | world-local values, not handle kinds | additive instances |

### Configuration keys

| Key | Type, range | Default | Restart-required; allowed layers | Read by |
|---|---|---|---|---|
| `scene.activationBudgetUs` | u32, 100 … 50,000 | per platform: the `Win` value of Z2-B-SCENE-003 in µs on Windows and Linux hosts, its `S24` value on Android; 2,000 on every platform until the row is allocated | no; every layer | `StepActivations` |
| `scene.activationRetryTicks` | u32, 1 … 600 | 8 | yes; every layer except `CONFIG_LAYER_QUALITY_TIER` | `Scene.ActivateAdditive` |
| `scene.maxAdditiveInstances` | u32, 1 … 4,096 | 256 | yes; every layer except `CONFIG_LAYER_QUALITY_TIER` | `Zenith_SceneWorldDesc` |
| `scene.prepareReserveThresholdBytes` | u64, 0 … 2^40 | 1,048,576 | no; every layer | preparation |

`scene.activationRetryTicks` and `scene.maxAdditiveInstances` are simulation-affecting (they decide activation ticks and acceptance) and are written into the replay header like ECS's capacity keys. PLAT's schema rules (`Zenith2/Docs/Design/PLAT/Configuration.md` → "Registration API") therefore make both restart-required and keep them out of the quality-tier layer, or `Zenith_Config::Create` fails `CONFIG_SOURCE_ERROR_SCHEMA_RULE` at startup step 6. The keys are registered by `Zenith_RegisterSceneConfigKeys(Zenith_ConfigSchema&, Zenith_BuildPlatform)`, the form of every engine library's registration (`Zenith2/Docs/Design/MOD.md` → "Configuration-key registration"); the platform parameter selects the per-platform default of `scene.activationBudgetUs`.

## Data structures and formats

### In-memory structures

| Structure | Size | Specified in |
|---|---|---|
| `Zenith_Transform` / `Zenith_LocalTransform` / `Zenith_PreviousWorldTransform` | 40 bytes each | `Zenith2/Docs/Design/SCENE/Transforms.md` → "Transform components" |
| `Zenith_WorldTransform` | 92 bytes | same |
| `Zenith_HierarchyNode` | 24 bytes per entity with a parent or children | `Zenith2/Docs/Design/SCENE/Transforms.md` → "Hierarchy cache" |
| `Zenith_SceneError` / `Zenith_DocumentError` | 24 / 16 bytes | the error headers |
| `Zenith_SceneEntityRecord` | 72 bytes (`static_assert`) plus field sets in the arena | `Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_SceneData header" |
| `Zenith_TeleportPayload` | 56 bytes | `Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_LifecycleCommands header" |
| cooked entity record | 48 bytes | `Zenith2/Docs/Design/SCENE/Format.md` → "Sections" |
| cooked asset-reference record | 24 bytes | `Zenith2/Docs/Design/SCENE/Format.md` → "Sections" |
| `Zenith_SceneLoadResult` | 8 bytes (the publication payload of `ASYNC_SOURCE_SCENE_LOAD`) | `Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_AdditiveScenes header" |
| `Zenith_SavedSceneInstance` | plain value with a borrowed root span | `Zenith2/Docs/Design/SCENE/ModuleSeams.md` → "Activation hook declarations" |
| `Zenith_WorldOrigin` / `Zenith_WorldOriginShift` | 40 / 24 bytes | `Zenith2/Docs/Design/SCENE/ModuleSeams.md` → "Zenith_WorldOrigin header" |

### Formats summary

| Format | Kind | Specified in |
|---|---|---|
| `*.scene.json`, `*.prefab.json` | authoring documents (canonical JSON), migrated never merely re-cooked | `Zenith2/Docs/Design/SCENE/Format.md` → "Scene document schema" |
| canonical serialization | byte rules over every framework document type | `Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "Canonical serialization" |
| cooked scene and prefab (`ZSCN`, `ZPFB`, format version 1) | SCENE's seven section kinds in ASSET's cooked asset file | `Zenith2/Docs/Design/SCENE/Format.md` → "Cooked scene payload" |
| `<batch>.z2journal` | canonical JSON journal | `Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "Journal format" |
| `.migration/backups/` | backup tree | `Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "Backups" |
| `zenith2.migration-preview`, `zenith2.reference-repair`, `zenith2.scene-dump` | reports (canonical JSON, version 1) | the owning sections |
| migration fixtures `Tests/Fixtures/Migration/{SceneDocument,PrefabDocument}/v<N>.json` | test data | `Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "Migration fixtures" |

## Algorithms and state machines

### Algorithm index

| Algorithm | Heading | Complexity |
|---|---|---|
| propagation | `Zenith2/Docs/Design/SCENE/Transforms.md` → "Propagation algorithm" | O(chunks + changed roots + dirty children) per tick |
| hierarchy maintenance | `Zenith2/Docs/Design/SCENE/Transforms.md` → "Hierarchy maintenance at playback" | O(subtree) per reparent |
| teleport | `Zenith2/Docs/Design/SCENE/Transforms.md` → "Teleport" | O(subtree) |
| canonical writing | `Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "Canonical writing algorithm" | O(n log n) members |
| open, chain, plan, apply, journal, recovery | `Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "Migration algorithms" | O(bytes × steps); diff O((N + M) D) |
| ID derivation and remapping | `Zenith2/Docs/Design/SCENE/Prefabs.md` → "Derivation", "Remapping" | O(1) per ID; O(set) |
| prefab resolution and override blocks | `Zenith2/Docs/Design/SCENE/Prefabs.md` → "Resolution algorithm" | O(entities × nesting) |
| runtime instantiation | `Zenith2/Docs/Design/SCENE/Prefabs.md` → "Runtime instantiation" | O(prefab entities) |
| reference repair | `Zenith2/Docs/Design/SCENE/Prefabs.md` → "Reference repair" | O(references) after an O(documents) scan |
| preparation and activation | `Zenith2/Docs/Design/SCENE/Loading.md` → "Preparation operation", "Activation paths" | O(entities + field bytes) |
| additive load | `Zenith2/Docs/Design/SCENE/Loading.md` → "Additive load sequence" | one buffer per instance |

### State machines

| Machine | States | Table |
|---|---|---|
| preparation | REQUESTED → READING → DECODING → DEPENDENCIES → (GATED, tagged loads only) → BUILDING → READY; FAILED; CANCELLED | `Zenith2/Docs/Design/SCENE/Loading.md` → "Preparation state machine"; `Zenith2/Docs/Design/SCENE/ModuleSeams.md` → "Gated preparation state" |
| additive instance | LOADING → QUEUED → ACTIVE → UNLOADED; FAILED | `Zenith2/Docs/Design/SCENE/Loading.md` → "Additive scene set" |
| journal | PREPARED → BACKED_UP → WRITTEN → COMMITTING → deleted; recovery per state | `Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "Journaled write" |
| opened document | CURRENT, MIGRATED, READ_ONLY_NEWER | `Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "Opening a document" |
| migration registry | OPEN → FROZEN | `Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "Zenith_MigrationRegistry header" |

### Tick walkthrough of SCENE work

1. Frame prologue step 7 (widened by X109(b)): the runtime creates isolated worlds whose preparation succeeded, `StepActivations` starts every pending additive preparation and advances the isolated activations, and the runtime swaps a transition whose completion was published.
2. Tick boundary: at step 1 SCENE records the tick, clears the refused-commit flag, resolves tagged discards (the resolver, live mode) and its publication handler moves published additive preparations into the activation queue, marks them failed or completes a discard; at step 3 SCENE's tick-boundary handler commits the script-load inbox and releases `UNLOADED` records (a tagged one only after its discard was published).
3. Phase 1: `Scene.ActivateAdditive` records one buffer per queued instance.
4. Each sync point: SCENE's playback functions (teleport, reparent, bind references, additive commits and forgets, transition requests) at step 7, and last in step 7 the origin shift, which dispatches `LIFECYCLE_EVENT_ORIGIN_SHIFTED`; the structural observer maintains the hierarchy at step 6 and forwards lifecycle events.
5. Phase 6: the physics sync and other arbitrated writers of local transforms.
6. Phase 8: `Scene.PropagateTransforms`; its merge commits the transform change bits and clears the hierarchy's dirty set; ECS records the invocation's read version.
7. Phase 12: extractors read the world and previous transforms with the interpolation factor.

## Threading lifetime and error contract

### Thread affinity per type

| Type | Keyword | Notes |
|---|---|---|
| value types, enums, components | `IMMUTABLE` | |
| `Zenith_TransformHierarchy`, `Zenith_AdditiveSceneSet` | `EXTERNALLY_SYNCHRONISED` | mutated on sync-point, tick-boundary and phase-1 nodes of their world only |
| `Zenith_LifecycleDispatcher` | `ANY_THREAD_EXCEPT(Dispatch)` | Dispatch on the sync-point node |
| `Zenith_SceneLoader`, `Zenith_SceneServices`, `Zenith_SceneWorldState` | `MAIN_THREAD` | preparation bodies run as background jobs |
| `Zenith_SceneDocument`, `Zenith_SceneEdit`, `Zenith_DocumentMigrator`, `Zenith_ReferenceIndex` | `OWNER_THREAD` | tool threads |
| `Zenith_MigrationRegistry` | `MAIN_THREAD` until Freeze, then `IMMUTABLE` | |
| `Zenith_DocumentStorage` | `EXTERNALLY_SYNCHRONISED` | |

### Frame-graph nodes SCENE runs

| Node | Subgraph, stage | Declared access | Body |
|---|---|---|---|
| `Scene.ActivateAdditive@W<tag>` | TICK, 1 | writes resource `Zenith.SceneActivationQueue`; emits commands | `Zenith2/Docs/Design/SCENE/Loading.md` → "Additive load sequence" |
| `Scene.PropagateTransforms@W<tag>` | TICK, 8 | reads `Zenith.LocalTransform`; writes `Zenith.WorldTransform`, `Zenith.PreviousWorldTransform` and resource `Zenith.TransformHierarchy` (the resource write on its merge node); emits commands | `Zenith2/Docs/Design/SCENE/Transforms.md` → "Propagation algorithm" |
| SCENE playback functions | the sync-point nodes (ECS) | the playback access | per command kind |
| SCENE's structural observer and tick-boundary handler | the sync-point and tick-boundary nodes (ECS) | the playback access; the tick-boundary access (unused) | Transforms.md → "Hierarchy maintenance at playback"; Loading.md → "Tick-boundary handler" |

Both systems are `JOB_AFFINITY_ANY` C++ systems registered through `Zenith_SystemRegistry` (`Zenith2/Docs/Design/ECS/SystemAPI.md` → "Zenith_System header"), so ECS's `EmitInto` places them in their world's group ([Z2-N-ARCH-423]).

### Lock-order entries

SCENE acquires no `Zenith_Mutex` of its own and adds no row to `Zenith2/Docs/Contracts.md` → "Threading and concurrency contract" (table "Lock ordering table"): its per-world state is mutated single-threaded on its world's nodes; the loader's operation table is main-thread only; preparation reaches CORE's string-table lock (850, short-hold) when interning and ZenithAssets' registry lock (200, short-hold) when acquiring handles, never both at once.

### Construction and shutdown

| Step (`Zenith2/Docs/Architecture/CompositionRoot.md` → "Startup sequence") | SCENE action |
|---|---|
| 6 | `Zenith_RegisterSceneTypes` in the declaration phase |
| 11 | `Zenith_SceneServices::Create`, `Zenith_RegisterSceneAssetKinds(xAssets, xServices)` before the registry's `CloseRegistration`, `Zenith_SceneLoader::Create`; the loader's memory consumer registered with the governor |
| 15 | `Zenith_RegisterSceneCommandKinds` before the table is sealed; `Zenith_RegisterSceneScriptBindings` into the engine binding table before it is sealed |
| 16 | per world: `Zenith_SceneWorldState::Create`, `Attach` (observer, handler, systems, resources, listeners registered by modules afterwards) |
| 17 | `LoadStartupScene` |
| prologue step 7 | `StepActivations` |
| shutdown step 4 | the loader's outstanding preparations are cancelled with every other operation and complete |
| shutdown step 6 | each `Zenith_SceneWorldState` destroyed with its world; then the loader (after every world); `UnregisterKindLoader(ASSET_KIND_SCENE)` and `(ASSET_KIND_PREFAB)`; then the services, which own the two kind loaders |

The tools construct `Zenith_MigrationRegistry` (registering every document type they handle, then `Freeze`), a `Zenith_DocumentStorage` per storage root and a `Zenith_DocumentMigrator`, and call `RecoverPendingJournals` before any other document operation; the cook, over a read-only storage, refuses to cook when it reports a pending journal (`Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "Ownership split with ASSET").

### Cancellation

Preparation cancellation is `Zenith2/Docs/Design/SCENE/Loading.md` → "Cancellation"; frame cancellation completes SCENE's nodes without running their bodies ([Z2-N-ARCH-412]). A propagation whose node and merge were both skipped leaves ECS's previous read version, the hierarchy's dirty set and the revisit marks unchanged (only a completed merge advances or clears them), so the next pass sees every local-transform commit and structural change since the last completed pass and a skipped phase 8 leaves no stale world transform after the next tick. A cancellation that lands between the node and its merge lets the started node finish: it has swapped the marks and written world rows in place, uncommitted, so the next pass finds `m_bMergePending` still set, takes the union of both revisit sets and runs the one forced pass that writes and marks changed every row it visits; no change bit is lost, and the `Previous` of a row that moved only in the cancelled tick equals that tick's world on the tick after the forced pass (`Zenith2/Docs/Design/SCENE/Transforms.md` → "Propagation state", and step 0 of "Propagation algorithm"); after `RestoreSimulationState` the previous read version is 0 and the pass compares every row (`Zenith2/Docs/Design/SCENE/Transforms.md` → "Propagation algorithm").

### Error enumerations

| Enumeration | Header | Codes |
|---|---|---|
| `Zenith_SceneErrorCode` in `Zenith_SceneError` | `Zenith_SceneErrors.h` | 24 codes |
| `Zenith_DocumentErrorCode` in `Zenith_DocumentError` | `Zenith_DocumentErrors.h` | 23 codes |
| `Zenith_DocumentWarningCode` | `Zenith_DocumentErrors.h` | 7 warnings |

Caller responses: a startup-scene error is exit code 3; an additive load's error is its instance's state and a published failure; a transition's preparation error fails the transition operation and the old world continues; a document error in a tool is shown with its diagnostics and nothing is written.

### Luau errors versus asserts

SCENE raises no Luau error of its own: its bindings validate their arguments and raise SCRIPT's catalogued misuse error for a nil or wrong-kind value, a non-finite transform or an out-of-range flag ([Z2-N-CONTRACT-473]); everything a script causes at playback (a stale entity, a prefab not resident, a capacity rejection) is counted and logged. SCENE asserts only programmer errors in engine code (a registration after the first tick, a dispatch from a listener, a take of an incomplete operation).

### Determinism obligations

| Obligation | Mechanism |
|---|---|
| world transforms independent of workers and layout | pure per-row computation; levels in fixed order; bitwise change tests |
| change versions independent of scheduling | ECS's precomputed merge versions |
| instance IDs and completion IDs in merge order | a C++ load's at sync-point playback; a script load's completion ID at SCRIPT's commit in merge order and its instance ID at the next tick boundary in inbox order |
| content appears at logged ticks | publication at tick boundaries in completion-ID order; replay holds the tick ([Z2-N-SCRIPT-337]) |
| derived IDs identical everywhere | CORE's SHA-256 (FIPS 180-4, outside D-SEL-HASH (open, Stage 1), `Zenith2/Docs/Design/CORE/Maths.md` → "SHA-256 outside D-SEL-HASH") of fixed byte strings; no open decision reaches it |
| nothing observes storage order | every SCENE iteration is by simulation ID, persistent ID or activation order |
| a tagged load discarded while loading publishes identically however its result raced the unload | the discard flag written on the sync-point node and read by the resolver on the tick-boundary node; the publication is RECORDED, so a replay never holds a tick for it (`Zenith2/Docs/Design/SCENE/ModuleSeams.md` → "Discarded publications of tagged loads") |
| a restored loading instance completes under the recorded request | `RestoreInstances` re-requests under the recorded completion ID and allocates none (`Zenith2/Docs/Design/SCENE/ModuleSeams.md` → "Restore algorithm") |
| an origin shift is applied identically by a run and its replay | the shift plays last in step 7, at most once per sync point, as a pure function of each old value (`Zenith2/Docs/Design/SCENE/ModuleSeams.md` → "Origin shift playback") |

## Diagnostics and profiling

### Counters

Published per tick per world through the counter hook, each suffixed `.<worldTag>` where per world: `scene.transform.changed`, `scene.transform.auto_added`, `scene.transform.late_world_added`, `scene.transform.orphaned`, `scene.transform.derived_removed`, `scene.transform.non_finite`, `scene.hierarchy.nodes`, `scene.hierarchy.max_depth`, `scene.hierarchy.depth_rejections`, `scene.hierarchy_bytes`, `scene.teleport.count`, `scene.teleport.stale`, `scene.reparent.singular`, `scene.instantiate.count`, `scene.instantiate.not_resident`, `scene.instantiate.too_large`, `scene.references.unresolved`, `scene.additive.active`, `scene.additive.retries`, `scene.additive.failed`, `scene.carryover.missing`, `scene.load.unknown_components`, `scene.transition.refused`; process-wide: `scene.prepare.live`, `scene.prepare.bytes`, `scene.prepare.cancelled`, `scene.prepare.derived_ids` (authoring IDs derived in `BUILDING` slices), `scene.activation.slice_us`, `scene.resident_bytes`. The tools publish `document.opened`, `document.migrated`, `document.written`, `document.backups`, `document.journals_recovered`, `document.warnings`. The module seams add the process-wide `scene.prepare.cancelled.<state>`, `scene.prepare.orphaned`, `scene.amend.unknown_types`, `scene.references.skipped` and the per-world `scene.restore.refused`, `scene.origin.shifts`, `scene.origin.shifts_ignored`, `scene.origin.invalid_shifts` (`Zenith2/Docs/Design/SCENE/ModuleSeams.md` → "Module seam diagnostics").

### Profiling zones

`Scene.PropagateTransforms` (the node; Z2-B-SCENE-002's span), `Scene.ActivateAdditive`, `Scene.Playback.<Kind>`, `Scene.Observer`, `bg/Scene.Prepare.<reason>` (background slices), `Scene.ActivateSlice` (prologue), `Document.Open`, `Document.Plan`, `Document.Apply`, `Document.Recover`, `Document.Canonical`.

### Log categories

| Category | Messages |
|---|---|
| `scene.document` | every validation diagnostic as `<path>:<line>:<column>: <keyPath>: <message>` |
| `scene.prefab` | resolution errors with the prefab chain; dangling overrides as `scene.prefab: <document>: override of <target> <stableName> <path> is dangling (target removed from <prefab>)` |
| `scene.load` | preparation failures with the scene path and the first missing asset; unknown components under SKIP |
| `scene.hierarchy` | depth-limit corrections |
| `scene.transition` | carry-over skips, refusals |
| `document` | framework errors and every warning, with both spellings for the unknown-field warning |

Per-tick lines are rate-limited to 64 per category with a count of the rest.

### On-screen profiler rows

DEVT draws from the counters alone: propagation time against Z2-B-SCENE-002; changed transforms per tick; hierarchy nodes and depth; additive instances by state; preparations in flight and their bytes; SCENE resident bytes against Z2-B-SCENE-001 on the memory panel.

### Debug views and dumps

`scene.dump <worldTag> <path>` and `scene.hierarchy.validate <worldTag>` (`Zenith2/Docs/Design/SCENE/Format.md` → "Scene diagnostic dump"); a debug view draws each entity's world axes and parent links for a selected subtree (development builds, drawn through DEVT's debug primitives).

## Authoring and cooking

### What SCENE exposes to EDIT

- The document model (`Zenith_SceneDocument`) with O(1) lookup by authoring ID, the edits with inverses (undo), `Zenith_SceneEditSet` with remapping (copy and paste), the resolved form and `Zenith_ActivateSceneData` for the edit world, `Zenith_CaptureWorld` for play copies ([Z2-N-ARCH-766]).
- The migration preview data (`Zenith_MigrationPlan` and its `zenith2.migration-preview` report: per document versions, diff hunks, changed paths, warnings, blocked entries), `ApplyPlan` (backup and journaled write) and `RecoverPendingJournals` (resume or roll back), which EDIT's migration UI presents (Z2-I-EDIT-15 to -17).
- Prefab editing primitives: override writes, reverts, apply-to-prefab, unpack (Z2-I-EDIT-07).
- The reference index and repair plans for the asset browser's rename, move and delete.

### Cook step

SCENE implements the cook steps `scene.scene` and `scene.prefab` in `Zenith2/Tools/ZenithCook/Internal/Scene/`, registered by `Zenith_RegisterSceneCookSteps` at cook services item 6. Each opens its document through the cook's read-only migrator, resolves it with the cook's provider, cooks with `Zenith_CookSceneDocument`, declares every asset reference (and in a scene every instanced prefab) as a hard dependency, and puts the document and every prefab its resolution read into its derived-data key (`Zenith2/Docs/Design/SCENE/Prefabs.md` → "Cooking resolved prefabs", "Scene and prefab cook steps"). An unknown type, a missing prefab or a newer document is a cook error naming the document.

### Migration hooks

SCENE's document types register through `Zenith_RegisterSceneDocumentTypes`; the other owners register theirs (MOD `game.json`, ASSET `.meta`, ECS `*.schema.json`, PLAT bindings and configuration, GEOM materials, ANIM, UI and CINE documents) into the same registry in the editor and the cook; each owner ships its fixtures ([Z2-N-CONTRACT-548]).

### Merge support for CONTENT

CONTENT's structure-aware 3-way merge driver (Z2-I-CONTENT-27) parses scene and prefab documents with the canonical schemas of `Zenith2/Docs/Design/SCENE/Format.md` → "Scene canonical schema": ID maps merge member-wise by ID, keyed maps by stable name, field sets by property ID; override blocks merge per (target, stable name, path); fixed-array and container overrides and ordered arrays that conflict fall back to an explicit textual-conflict marker (Z2-I-CONTENT-29). The merged result is written canonically, so a merge introduces no formatting churn.

## Failure handling

| Failure | Where | Defined behaviour |
|---|---|---|
| missing or corrupt document | open | `DOCUMENT_ERROR_NOT_FOUND`, `_PARSE` or `_SHAPE` with position; nothing written; the editor shows the diagnostics |
| newer document or component version | open | read-only with a diagnostic; every write refused ([Z2-N-CONTRACT-537]) |
| unknown component type | document; edit world; cook; runtime | preserved opaque and round-tripped; not created; cook error; FAIL or SKIP policy (`Zenith2/Docs/Design/SCENE/Loading.md` → "Missing-type handling at load") |
| broken reference (entity, prefab, asset) | resolution, activation | dangling overrides kept and reported; unresolved entity references null and counted; a missing prefab fails resolution; repair plans fix them |
| prefab cycle or excessive nesting | resolution | `SCENE_ERROR_PREFAB_CYCLE` / `_TOO_DEEP` naming the chain |
| failed migration step | open, plan | `DOCUMENT_ERROR_STEP_FAILED`; storage untouched; the plan entry blocked |
| interrupted migration or save | next tool start | `RecoverPendingJournals` rolls back or forward per the journal state; every write refused until it has run |
| corrupt journal or recovery conflict | recovery | reported with the journal or document named; nothing changed; writes stay refused |
| low disk | journaled write, step 0b | `DOCUMENT_ERROR_LOW_DISK` naming the storage when its free space is below the threshold of `Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "Journaled write" (step 0b); nothing written |
| a step deletes, moves or changes an unknown member | open, plan | `DOCUMENT_ERROR_STEP_FAILED` naming the step and the member; storage untouched ([Z2-N-CONTRACT-545]) |
| corrupt or mismatched cooked scene | preparation | `SCENE_ERROR_COOKED_CORRUPT` / `_VERSION`; nothing activated |
| asset unavailable | preparation | `SCENE_ERROR_ASSET_UNAVAILABLE` naming the first missing asset; nothing activated |
| allocation failure | factories | unwind and `*_OUT_OF_MEMORY` ([Z2-N-CONTRACT-340]); arenas route through the OOM handler |
| memory pressure | preparation | a refused governor reservation fails `SCENE_ERROR_MEMORY_REFUSED`; SCENE sheds nothing at a level change (simulation memory is budgeted, not shed, `Zenith2/Docs/Architecture.md` → "Memory governor"), except that `Zenith_Prefab` resolved data of zero-reference prefabs is released at WARNING |
| merge-time capacity | additive activation, instantiation | retried then failed; instantiation records nothing |
| cancellation | preparation, transition | released on the executor thread before completion is reported |
| non-finite transform | propagation | flagged and counted, never asserted |
| a saved additive set that is malformed, or a re-request that cannot be honoured | `RestoreInstances` | `SCENE_ERROR_INVALID_DESC` or `SCENE_ERROR_LIMIT` with nothing restored; a refused re-request parks its failure under the recorded ID, is counted and reported to the hook (`Zenith2/Docs/Design/SCENE/ModuleSeams.md` → "Restore algorithm") |
| a second or invalid origin shift in one sync point | `Scene.ShiftWorldOrigin` playback | counted, logged and ignored; nothing dispatched |
| the amender cannot allocate | `BUILDING` of a tagged load | the preparation fails `SCENE_ERROR_OUT_OF_MEMORY`; `OnPreparationEnded` still follows |
| hierarchy depth over 64 | playback | corrected to root, logged, counted |
| device loss | — | not a SCENE event |
| budget breach | Z2-B-SCENE-002 | BR-ACCEPT: blocks the stage acceptance until fixed or re-budgeted |
| budget breach | Z2-B-SCENE-001 | BR-GOVERNOR: the simulation subsystem raises `memory.simulation_over_budget` |

## Test strategy

### Test kinds

| Kind | What | Where |
|---|---|---|
| unit | every public type and every failure scenario | `Zenith2/Docs/Design/SCENE/Tests.md` → "Canonical serialization tests" to "Cook and load tests" |
| round-trip and byte stability | golden documents and payloads | `Zenith2/Docs/Design/SCENE/Tests.md` → "Golden fixtures" |
| prefab override matrix and variants | every property kind at every level | `Test_PrefabResolve_OverrideMatrix`, the variant tests |
| migration fixtures | per version per type | `Test_SceneDocumentMigration_Fixtures`, `Test_DocumentFramework_TestDocFixtures` |
| interruption | the journal at every step and rename | `Test_JournaledWrite_InterruptedAtEachStep_Recovers` |
| determinism | 1, 2, 8 and all workers × compaction × capacities × backends | `Test_TransformPropagation_IndependentOfWorkersAndLayout`, `Test_PrefabInstantiate_DeterministicAcrossWorkers` |
| cancellation stress | 1,000 cycles | `Test_SceneLoader_CancelStress` |
| scenario | transactional load with an in-flight async result | `Test_SceneTransition_InFlightAdditiveDiscarded` and GAME's `Test_Scenario_WorldTransitionWithInFlightAsync` |
| fuzz | cooked decoder, document validator, journal | the three fuzz targets |
| failure injection | every factory step | the `_Create_FailsAt<Step>_Unwinds` tests |
| benchmarks | propagation, decode, activation, resolve | `Zenith_Bench_Scene` |

### Required named tests

Every test named in `Zenith2/Docs/Design/SCENE/Tests.md` is required and lands in the ticket that implements its subject ([Z2-N-CONTRACT-410]); the gate step per group is `Zenith2/Docs/Design/SCENE/Tests.md` → "Gate steps per test group".

## Budget rows

### Rows SCENE owns

| Row | Name | Status | Procedure | SCENE measurement |
|---|---|---|---|---|
| Z2-B-SCENE-001 | Scene and prefab runtime data | pending, allocated by the Stage 1 per-system budget ticket for SCENE | MP-SUBMEM, BR-GOVERNOR | `scene.resident_bytes`: hierarchy caches, additive sets, prepared scenes, resident prefab data, reported through the loader's memory consumer (`MEMORY_SUBSYSTEM_SIMULATION`) |
| Z2-B-SCENE-002 | Transform propagation CPU | pending, same producer | MP-CPUZONE, BR-ACCEPT | the p95 span of `Scene.PropagateTransforms` per world per tick |
| Z2-B-SCENE-003 | Scene activation slice per frame prologue | pending, same producer ([Z2-N-BUDGET-464]) | MP-CPUZONE, BR-DEFER | the span of `Scene.ActivateSlice` per prologue step 7 in the transition workloads (W-OpenWorld, W-Cinematic, W-Game2D); `scene.activationBudgetUs` takes the value in µs as its engine-default layer |

The producer is the needs-human ruling "Stage 1 per-system budget ticket for SCENE", blocked by the CORE, JOBS and RPROD rulings; SCENE has no Stage 1 harness half (`Zenith2/Docs/Budgets.md` → "Per-system producer shape").

### Rows SCENE is measured against

- Z2-B-CORE-007 (world-transition peak): the runtime reserves it before a transition's preparation.
- Z2-B-ECS-003, Z2-B-ECS-004 (spawn and command capacity per tick): additive activation and prefab instantiation are merge-accepted against them.
- Z2-B-EDIT-005 (scene open): dominated by SCENE's open and resolve.
- Z2-B-SHIP-013, Z2-B-SHIP-014 (cold boot): include the startup-scene load.
- Z2-B-SHIP-016 (maximum frame time, the rebase-tick hitch STREAM names): includes the origin-shift playback of one sync point.
- the CPU-SIM and MEM-WIN-RAM / MEM-S24 allocation sums of which Z2-B-SCENE-002, Z2-B-SCENE-003 and Z2-B-SCENE-001 are members.

### Pending row needs

| Need | Unit | Procedure | Breach | Producer |
|---|---|---|---|---|
| S-R1 "Scene activation slice per frame prologue" (settled: added to `Zenith2/Docs/Budgets.md` as Z2-B-SCENE-003, [Z2-N-BUDGET-464]) | ms | MP-CPUZONE on `Scene.ActivateSlice` | BR-DEFER (the remaining chunks continue next prologue) | Stage 1 per-system budget ticket for SCENE |

Until Z2-B-SCENE-003 is allocated, `scene.activationBudgetUs` takes its design default as the engine-default layer; the row was cross-area note S12.

## Stage plan

### Stage 1

| Ticket | Kind | Content | Decisions |
|---|---|---|---|
| Stage 1 per-system budget ticket for SCENE | `needs-human` ruling | allocates Z2-B-SCENE-001, Z2-B-SCENE-002 and Z2-B-SCENE-003 | Z2-D-012, Z2-D-084 |
| D-PHASES refinement (open, Stage 1), SCENE as input | input | confirms the local-transform authority and phase 8's content | Z2-D-080 |
| D-SEL-JSON (open, Stage 1), D-SEL-HASH (open, Stage 1) | consumed | the JSON backend under documents; the hash under type IDs and session cache keys | — |

### Stage 2

In dependency order; each ticket lands its tests in the same diff:

| # | Slice | Kind | Blocked by |
|---|---|---|---|
| 1 | API-only: every ZenithDocument public header with contract blocks | api | CORE JSON reader and writer; CORE `Zenith_Sha256`; ECS Reflect API-only slice |
| 2 | document and storage paths, document pointers, memory storage, canonical serialization with the unknown-member rules | implementation | 1 |
| 3 | migration registry, chain execution, open, plan, diff | implementation | 2; ECS migration types |
| 4 | journaled write, backups, low-disk refusal, recovery, save retention; `Test.Doc` fixtures | implementation | 3 |
| 5 | ZenithDocumentHost | implementation | 4 |
| 6 | API-only: every ZenithScene public header | api | 1; ECS WorldAPI API-only slice; ASSET API-only |
| 7 | transform types and components, registration | implementation | 6 |
| 8 | hierarchy cache and structural observer, lifecycle dispatcher | implementation | 7; ECS world simulation (structural events) |
| 9 | `Scene.PropagateTransforms`, out-of-tick propagation, teleport, reparent-keep-world, interpolation | implementation | 8 |
| 10 | scene document model, validation (including flat prefab documents: kind PREFAB, no instances, `variantOf` null), document types, canonical schemas, sort keys, unknown components, SCENE fixtures v1 | implementation | 3, 7 |
| 11 | cooked payload, cooker with a null provider (scenes without instances and flat prefabs), decoder, fuzz targets, asset kinds | implementation | 10; ASSET loader interface; COMPRESS sections |
| 12 | startup scene load and out-of-tick activation | implementation | 11, 9; MOD startup step 17 |
| 13 | diagnostics: counters, zones, logs, dumps | implementation | 9, 12 |
| 14 | Stage 2 acceptance contribution: the minimal game's startup scene on Windows and the S24, the determinism matrix for propagation | acceptance input | 12, 13 |

### Stage 3

| # | Slice | Kind |
|---|---|---|
| 15 | preparation operation (background slices, cancellation, governor reservation) | implementation |
| 16 | additive scenes: commands, pending preparations started in `StepActivations`, publication handler, tick-boundary handler, `Scene.ActivateAdditive`, unload, set, replay | implementation (blocked by MOD's publication service) |
| 17 | scene transitions: isolated activation, `StepActivations`, carry-over capture, transition requests | implementation (with MOD's swap) |
| 18 | runtime prefab instantiation of cooked prefabs and `Scene.Instantiate` for `ctx:Spawn`, over the Stage 3 subset of flat prefab documents cooked with a null provider (`Zenith2/Docs/Design/SCENE/Prefabs.md` → "Flat prefabs at Stage 3") | implementation |
| 19 | editor foundation support: edits with inverses, edit sets, remapping, lookup — pulled forward into the Stage 2 sprint when SCENE's slices 1–10 are done (`Zenith2/Docs/Roadmap.md` → "Pull-forward rule") | implementation |
| 20 | SCENE script bindings, the script-load inbox and `Zenith_SceneLoadResult` | implementation (blocked by SCRIPT's asynchronous-request commit and cross-area row S22) |
| 21 | `Zenith_Integration_Scene` suite and the Stage 3 acceptance contribution | integration |
| 21a | the additive set's save enumeration and restore entry for SAVE (API-only slice first, then the implementation with the recorded-ID re-request; row SV4) | api, implementation |
| 21b | API-only: the collision query and its resource type (rows GA2 (a), GA3 (a)), after slice 27's fluid query, so PHYS and GAME can claim against it | api |

### Stage 4

| # | Slice | Kind |
|---|---|---|
| 22 | prefab documents, resolution with nesting, override blocks, variants, resolved cache, provider | implementation (a pull-forward candidate into the Stage 3 sprint, as Roadmap names prefabs) |
| 23 | prefab editing edits, unpack | implementation |
| 24 | reference index and repair plans | implementation (with CONTENT's batch operations) |
| 25 | complete migration coverage: every document type registered in the editor and cook with fixtures; EDIT's migration UI over the preview, apply and recovery API | integration (with EDIT) |
| 26 | W-Editor scene-open measurement contribution | acceptance input |
| 27 | API-only: the fluid query and its resource type (row EN26), pulled into the Stage 3 sprint so ENV and PHYS can claim against it | api |
| 28 | API-only: the activation hook, amender, resolver, probe and world-origin declarations (rows ST2, ST3), pulled into the Stage 3 sprint | api |
| 29 | the activation hook's call points, the gated state, amendment, the probe and hold; then the discard resolver; then the detail, live-instance, coherence and restore-reporting obligations | implementation |
| 30 | the world origin, its shift playback and the origin-shifted event (rows ST3, PH20) | implementation (waits on ECS row S25 and core-doc items X227 (a), X264 (j)) |
| 31 | the module seams through ZenithRuntime: tagged discards with replay, the origin shift, the fluid-query resource, a restored loading instance | integration |

### Stages 5 to 7

- Stage 5: nothing new; propagation measured on the completed workloads.
- Stage 6: SCENE's migration fixtures in the compatibility-policy demonstration ([Z2-N-CONTRACT-551]).
- Stage 7: networked replication reads the transform change bits (NET); no SCENE change is planned.

### API-only slices

Slices 27 and 28 (Stage 4, pulled forward), slice 21b and the API half of slice 21a (Stage 3) add the declarations of `Zenith2/Docs/Design/SCENE/ModuleSeams.md` to existing and new headers in the same way, after the slices that created those headers; slice 28 also gives ZenithRuntime's existing sink a forwarding-free override of the new pure member, so no implementer stops compiling. Slices 1 and 6 land every public declaration with its contract block (a block's `Version: since Z2 Stage N` names the stage whose acceptance first exercises the type; the declaration itself lands in Stage 2), compile under `HeaderSelfContained` and `ApiContractCheck`, and carry only header-only functions (`Zenith_ToString`, limits) with their tests; they unblock EDIT, MOD, SAVE, SCRIPT and the modules that name SCENE types. Any subsequent change to a declaration is an API change with review ([Z2-N-CONTRACT-491]).

## Decisions

### Settled decisions

| ID | Governs |
|---|---|
| Z2-D-038 | JSON for authoring, versioned binary for cooked data, reflection-driven serialization, canonical authoring JSON |
| Z2-D-048 | authoring-document migration with preview, backup, journaled write, override and unknown-field preservation, fixtures |
| Z2-D-080 | phase 8 transform propagation; per-field authority; lifecycle commands at sync points |
| Z2-D-019 | determinism of propagation, instantiation and loading |
| Z2-D-053 | publication of loads at tick boundaries |
| Z2-D-020 | data-defined components pass through SCENE unchanged |
| Z2-D-040, Z2-D-045, Z2-D-046, Z2-D-047 | ownership and handles; results and asserts; no global state; contract blocks |
| Z2-D-001 | the build system (CMake presets, Ninja Multi-Config, globbed sources, one library per backend): the three targets are declared through its helpers, and `ZenithScene_Tests` links the three ECS backend libraries through `TEST_DEPENDS` |
| Z2-D-012, Z2-D-084 | budget rows and their harness-plus-ruling producer |
| Z2-D-097 | rulings appended to the decision log in the ticket's diff |

### Open decisions

- D-SEL-JSON (open, Stage 1): the parser backend under `Zenith_JsonDocument`; no SCENE code depends on it, because canonical bytes are written by Zenith code on both paths.
- D-SEL-HASH (open, Stage 1): `Zenith_Hash64` reaches SCENE only through type IDs ([Z2-N-CONTRACT-373]; every stable name's ID changes together, so registration, cooked type IDs and saves follow the ruling's own re-cook and SAVE handling) and through session cache keys that are never persisted. Derived authoring IDs do not use it: they are derived with CORE's `Zenith_Sha256`, which CORE places outside D-SEL-HASH (`Zenith2/Docs/Design/CORE/Maths.md` → "SHA-256 outside D-SEL-HASH"; `Zenith2/Docs/Design/SCENE/Prefabs.md` → "Derivation"), because they are stored in documents (override targets and ENTITY values naming entities of nested instances), cooked payloads and saves, and a per-document migration step cannot re-derive them.
- D-PHASES refinement (open, Stage 1): may move the local-transform authority; one registration line changes.
- D-SCRIPT-TXN refinement (open, Stage 1): may change merge-time acceptance, which additive activation and instantiation rely on through ECS's `TryAccept` alone.

### Selection decisions SCENE owns

SCENE owns no named selection decision. Choices made in this design and measured by `Zenith_Bench_Scene`: world transforms as matrices plus decomposition; the activation chunk of 4,096 entities; the level-by-level child propagation with a grain of 256; the depth limit 64 and the nesting limit 16.

### Justification records

SCENE adds no custom container, allocator, heap wrapper or threading primitive: the hierarchy cache uses CORE's `Zenith_HashMap`, `Zenith_HashSet` and `Zenith_Array`; SHA-256 is CORE's `Zenith_Sha256`, not SCENE's; a new library is not custom infrastructure under [Z2-N-CONTRACT-278] (`Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "Why a separate library").

## Requirement index

Every requirement of `Zenith2/Docs/Requirements/SCENE.md` and `Zenith2/Docs/Requirements/SCENE/Stage4Plus.md` and the design headings that specify it (paths relative to `Zenith2/Docs/Design/`):

| Requirement | Title | Design headings |
|---|---|---|
| Z2-R-SCENE-001 | ZenithScene library target | SCENE.md → "ZenithScene target"; SCENE.md → "Libraries SCENE depends on" |
| Z2-R-SCENE-002 | ZenithDocument library target | SCENE.md → "ZenithDocument target"; SCENE/DocumentFramework.md → "Why a separate library" |
| Z2-R-SCENE-003 | ZenithDocumentHost tool library | SCENE.md → "ZenithDocumentHost target"; SCENE/DocumentFramework.md → "ZenithDocumentHost" |
| Z2-R-SCENE-004 | Contract blocks on every SCENE header | SCENE.md → "API rules"; SCENE/SceneAPI.md → "Header list" |
| Z2-R-SCENE-005 | Upward notifications through SCENE-declared interfaces | SCENE.md → "Upward-callback interfaces SCENE declares" |
| Z2-R-SCENE-006 | Scene errors are records numbered from 1 | SCENE/SceneAPI.md → "Zenith_SceneErrors header"; SCENE/DocumentFramework.md → "Zenith_DocumentErrors header"; SCENE.md → "Error enumerations" |
| Z2-R-SCENE-007 | Authoring IDs are persistent IDs | SCENE/Prefabs.md → "Authoring identities"; SCENE/Format.md → "Scene document schema" |
| Z2-R-SCENE-008 | Authoring IDs are never reused within a document | SCENE/Prefabs.md → "Identity rules the editor foundation builds on"; SCENE/Format.md → "Document validation" |
| Z2-R-SCENE-009 | Deterministic ID derivation for instanced entities | SCENE/Prefabs.md → "Derivation"; SCENE/SceneAPI.md → "Zenith_AuthoringID header" |
| Z2-R-SCENE-010 | Derived ID collisions are reported | SCENE/Prefabs.md → "Derivation" |
| Z2-R-SCENE-011 | Copy and paste remaps IDs | SCENE/Prefabs.md → "Remapping" |
| Z2-R-SCENE-012 | Constant-time lookup by authoring ID | SCENE/Prefabs.md → "Identity rules the editor foundation builds on"; SCENE/SceneAPI.md → "Zenith_SceneDocument header" |
| Z2-R-SCENE-013 | Document edits return their inverse | SCENE/Prefabs.md → "Document edits for the editor"; SCENE/SceneAPI.md → "Zenith_SceneEdit header" |
| Z2-R-SCENE-014 | Play-world entities map back to document objects | SCENE/Prefabs.md → "Identity rules the editor foundation builds on"; SCENE/Loading.md → "Carry-over" |
| Z2-R-SCENE-015 | Canonical byte format | SCENE/DocumentFramework.md → "Canonical rules"; SCENE/Format.md → "Scene canonical schema" |
| Z2-R-SCENE-016 | Stable key order | SCENE/DocumentFramework.md → "Canonical rules"; SCENE/DocumentFramework.md → "Canonical writing algorithm" |
| Z2-R-SCENE-017 | One ID-keyed child per line | SCENE/DocumentFramework.md → "Canonical rules"; SCENE/Format.md → "Scene canonical schema" |
| Z2-R-SCENE-018 | Field sets in numeric property-ID order | SCENE/DocumentFramework.md → "Canonical rules" |
| Z2-R-SCENE-019 | Canonical numbers | SCENE/DocumentFramework.md → "Canonical rules" |
| Z2-R-SCENE-020 | Canonical writing is idempotent | SCENE/DocumentFramework.md → "Canonical rules" |
| Z2-R-SCENE-021 | Sibling order through sort keys | SCENE/Format.md → "Sort keys" |
| Z2-R-SCENE-022 | Canonical writer failure leaves output unchanged | SCENE/DocumentFramework.md → "Canonical writing algorithm" |
| Z2-R-SCENE-023 | Scene document schema version 1 | SCENE/Format.md → "Root object"; SCENE/Format.md → "Entity object"; SCENE/Format.md → "Component object" |
| Z2-R-SCENE-024 | Prefab document schema version 1 | SCENE/Format.md → "Prefab document schema" |
| Z2-R-SCENE-025 | Derived components are never authored | SCENE/Format.md → "Document validation"; SCENE/Transforms.md → "Transform components" |
| Z2-R-SCENE-026 | Document validation reports every violation of a pass | SCENE/Format.md → "Document validation"; SCENE/SceneAPI.md → "Zenith_SceneData header" |
| Z2-R-SCENE-027 | Document limits | SCENE/Format.md → "Document limits" |
| Z2-R-SCENE-028 | Document references by ID never by path | SCENE/Format.md → "Files and virtual paths"; SCENE/Prefabs.md → "Reference repair" |
| Z2-R-SCENE-029 | Unknown components are preserved opaque | SCENE/Format.md → "Unknown components" |
| Z2-R-SCENE-030 | Unknown components warn once per document and name | SCENE/Format.md → "Unknown components" |
| Z2-R-SCENE-031 | Unknown fields of known components round-trip | SCENE/Loading.md → "Missing-type handling at load"; SCENE/DocumentFramework.md → "Canonical rules" |
| Z2-R-SCENE-032 | Unknown components are a cook error | SCENE/Prefabs.md → "Cooking resolved prefabs"; SCENE/Loading.md → "Missing-type handling at load" |
| Z2-R-SCENE-033 | Runtime unknown-type policy | SCENE/Loading.md → "Missing-type handling at load"; SCENE/SceneAPI.md → "Zenith_SceneLoader header" |
| Z2-R-SCENE-034 | Unknown components survive prefab instancing | SCENE/Format.md → "Unknown components"; SCENE/Prefabs.md → "Re-identification" |
| Z2-R-SCENE-035 | Document types registered with version and chain | SCENE/DocumentFramework.md → "Zenith_MigrationRegistry header" |
| Z2-R-SCENE-036 | Contracts step registration signature | SCENE/DocumentFramework.md → "Zenith_MigrationRegistry header" |
| Z2-R-SCENE-037 | Chain gaps fail at tool start | SCENE/DocumentFramework.md → "Zenith_MigrationRegistry header"; SCENE/DocumentFramework.md → "Running a chain" |
| Z2-R-SCENE-038 | Older documents migrate through the chain in memory | SCENE/DocumentFramework.md → "Opening a document"; SCENE/DocumentFramework.md → "Running a chain" |
| Z2-R-SCENE-039 | Newer documents open read-only | SCENE/DocumentFramework.md → "Opening a document" |
| Z2-R-SCENE-040 | Failed migration leaves storage unchanged | SCENE/DocumentFramework.md → "Running a chain" |
| Z2-R-SCENE-041 | Upgrade preview is a dry run | SCENE/DocumentFramework.md → "Planning a migration"; SCENE/DocumentFramework.md → "Zenith_DocumentDiff header" |
| Z2-R-SCENE-042 | Non-deterministic steps are blocked | SCENE/DocumentFramework.md → "Planning a migration" |
| Z2-R-SCENE-043 | Preview report for the migration UI | SCENE/DocumentFramework.md → "Planning a migration"; SCENE.md → "What SCENE exposes to EDIT" |
| Z2-R-SCENE-044 | Automatic backup before any write | SCENE/DocumentFramework.md → "Applying a plan"; SCENE/DocumentFramework.md → "Backups" |
| Z2-R-SCENE-045 | Stale plans are refused | SCENE/DocumentFramework.md → "Applying a plan" |
| Z2-R-SCENE-046 | Journaled atomic write | SCENE/DocumentFramework.md → "Journaled write"; SCENE/DocumentFramework.md → "Journal format" |
| Z2-R-SCENE-047 | Interrupted writes recover all-or-nothing | SCENE/DocumentFramework.md → "Journaled write" |
| Z2-R-SCENE-048 | Pending journals block writes | SCENE/DocumentFramework.md → "Journaled write" |
| Z2-R-SCENE-049 | Save backups are retained within a limit | SCENE/DocumentFramework.md → "Applying a plan"; SCENE/DocumentFramework.md → "Backups" |
| Z2-R-SCENE-050 | Unknown fields preserved through migration | SCENE/DocumentFramework.md → "Opening a document"; SCENE/DocumentFramework.md → "Canonical rules" |
| Z2-R-SCENE-051 | Overrides preserved through migration | SCENE/DocumentFramework.md → "Override preservation in migration steps" |
| Z2-R-SCENE-052 | Component chains run inside documents | SCENE/SceneAPI.md → "Zenith_SceneDocumentTypes header"; SCENE/DocumentFramework.md → "Opening a document" |
| Z2-R-SCENE-053 | Migration fixtures per version per SCENE type | SCENE/DocumentFramework.md → "Migration fixtures" |
| Z2-R-SCENE-054 | Framework fixtures for a multi-version type | SCENE/DocumentFramework.md → "Migration fixtures" |
| Z2-R-SCENE-055 | Document paths are validated | SCENE/DocumentFramework.md → "Document paths"; SCENE/DocumentFramework.md → "ZenithDocumentHost" |
| Z2-R-SCENE-056 | Framework digests use CORE SHA-256 | SCENE/DocumentFramework.md → "Digests"; SCENE/DocumentFramework.md → "Journal format" |
| Z2-R-SCENE-057 | Framework factories unwind on failure | SCENE/DocumentFramework.md → "Zenith_DocumentMigrator header"; SCENE/DocumentFramework.md → "Zenith_DocumentStorage header" |
| Z2-R-SCENE-058 | Complete migration coverage with the editor | SCENE.md → "Stage 4"; SCENE.md → "Migration hooks" |
| Z2-R-SCENE-059 | Compatibility demonstrated with migration fixtures | SCENE.md → "Stages 5 to 7" |
| Z2-R-SCENE-060 | Transform components registered | SCENE/Transforms.md → "Transform components" |
| Z2-R-SCENE-061 | Local transform authority and arbitration | SCENE/Transforms.md → "Transform components" |
| Z2-R-SCENE-062 | World transforms derived and not serialized | SCENE/Transforms.md → "Transform components"; SCENE/Transforms.md → "Out-of-tick propagation" |
| Z2-R-SCENE-063 | World equals parent times local | SCENE/Transforms.md → "Propagation algorithm"; SCENE/Transforms.md → "Transform value type" |
| Z2-R-SCENE-064 | Decomposed world with shear flag | SCENE/Transforms.md → "Transform value type" |
| Z2-R-SCENE-065 | Propagation runs in phase 8 | SCENE/Transforms.md → "Propagation system"; SCENE.md → "Frame-graph nodes SCENE runs" |
| Z2-R-SCENE-066 | Transform change bits | SCENE/Transforms.md → "Propagation algorithm" |
| Z2-R-SCENE-067 | Propagation is deterministic across workers and layout | SCENE/Transforms.md → "Propagation algorithm"; SCENE.md → "Determinism obligations" |
| Z2-R-SCENE-068 | Previous world transform is the preceding tick | SCENE/Transforms.md → "Propagation algorithm"; SCENE/Transforms.md → "Interpolation data" |
| Z2-R-SCENE-069 | One interpolation formula | SCENE/Transforms.md → "Interpolation data" |
| Z2-R-SCENE-070 | Hierarchy cache follows ECS parent relation | SCENE/Transforms.md → "Hierarchy cache"; SCENE/Transforms.md → "Hierarchy maintenance at playback" |
| Z2-R-SCENE-071 | Hierarchy depth limit | SCENE/Transforms.md → "Hierarchy maintenance at playback" |
| Z2-R-SCENE-072 | Missing derived components repaired | SCENE/Transforms.md → "Hierarchy maintenance at playback"; SCENE/Transforms.md → "Propagation algorithm" |
| Z2-R-SCENE-073 | Orphans computed as roots | SCENE/Transforms.md → "Propagation algorithm" |
| Z2-R-SCENE-074 | Non-finite transforms flagged and never asserted | SCENE/Transforms.md → "Propagation algorithm"; SCENE.md → "Failure handling" |
| Z2-R-SCENE-075 | Out-of-tick propagation after loads | SCENE/Transforms.md → "Out-of-tick propagation" |
| Z2-R-SCENE-076 | Teleport resets the subtree immediately | SCENE/Transforms.md → "Teleport" |
| Z2-R-SCENE-077 | Teleport listener order and stale entities | SCENE/Transforms.md → "Teleport" |
| Z2-R-SCENE-078 | Reparent keeping the world transform | SCENE/Transforms.md → "Reparenting helpers" |
| Z2-R-SCENE-079 | Lifecycle listener dispatch | SCENE/SceneAPI.md → "Zenith_LifecycleListener header" |
| Z2-R-SCENE-080 | Propagation within its budget row | SCENE.md → "Rows SCENE owns"; SCENE/Tests.md → "Benchmarks and the budget harness" |
| Z2-R-SCENE-081 | SCENE budget rows allocated | SCENE.md → "Stage 1"; SCENE.md → "Rows SCENE owns" |
| Z2-R-SCENE-082 | World state per world | SCENE/SceneAPI.md → "Zenith_SceneWorld header" |
| Z2-R-SCENE-083 | Resolution algorithm | SCENE/Prefabs.md → "Resolution algorithm" |
| Z2-R-SCENE-084 | Nested instance IDs and references re-derived | SCENE/Prefabs.md → "Re-identification" |
| Z2-R-SCENE-085 | Prefab cycles detected | SCENE/Prefabs.md → "Resolution algorithm" |
| Z2-R-SCENE-086 | Prefab nesting depth limit | SCENE/Prefabs.md → "Resolution algorithm" |
| Z2-R-SCENE-087 | Resolved prefab cache | SCENE/Prefabs.md → "Resolution algorithm"; SCENE/Prefabs.md → "Prefab provider" |
| Z2-R-SCENE-088 | Override application order | SCENE/Prefabs.md → "Applying an override block" |
| Z2-R-SCENE-089 | Outermost override wins | SCENE/Prefabs.md → "Applying an override block" |
| Z2-R-SCENE-090 | Override keys are target ID plus property path | SCENE/Format.md → "Override block"; SCENE/Prefabs.md → "Override semantics per property kind" |
| Z2-R-SCENE-091 | Override granularity per property kind | SCENE/Prefabs.md → "Override semantics per property kind" |
| Z2-R-SCENE-092 | Dangling overrides are kept and reported | SCENE/Prefabs.md → "Applying an override block" |
| Z2-R-SCENE-093 | Variants keep base IDs | SCENE/Prefabs.md → "Variants" |
| Z2-R-SCENE-094 | Variant chains resolve in order | SCENE/Prefabs.md → "Variants" |
| Z2-R-SCENE-095 | Cook resolves prefabs completely | SCENE/Prefabs.md → "Cooking resolved prefabs"; SCENE/Prefabs.md → "Flat prefabs at Stage 3" |
| Z2-R-SCENE-096 | Cooking is deterministic | SCENE/Prefabs.md → "Cooking resolved prefabs"; SCENE/Tests.md → "Golden fixtures" |
| Z2-R-SCENE-097 | Runtime prefab instantiation is one atomic buffer | SCENE/Prefabs.md → "Runtime instantiation" |
| Z2-R-SCENE-098 | Instantiated prefabs visible to the next phase | SCENE/Prefabs.md → "Runtime instantiation"; SCENE/Transforms.md → "Transform value type" |
| Z2-R-SCENE-099 | Instantiation deterministic across workers | SCENE/Prefabs.md → "Runtime instantiation" |
| Z2-R-SCENE-100 | Unpacking an instance | SCENE/Prefabs.md → "Reference repair" |
| Z2-R-SCENE-101 | Prefab editing edits for EDIT | SCENE/Prefabs.md → "Document edits for the editor" |
| Z2-R-SCENE-102 | Reference repair on move | SCENE/Prefabs.md → "Reference repair" |
| Z2-R-SCENE-103 | Reference repair on delete | SCENE/Prefabs.md → "Reference repair" |
| Z2-R-SCENE-104 | Reference repair on prefab entity deletion | SCENE/Prefabs.md → "Reference repair" |
| Z2-R-SCENE-105 | Duplicate documents re-identified | SCENE/Prefabs.md → "Reference repair" |
| Z2-R-SCENE-106 | Repair plans write one journaled batch | SCENE/Prefabs.md → "Reference repair"; SCENE/SceneAPI.md → "Zenith_ReferenceRepair header" |
| Z2-R-SCENE-107 | Reference index incremental update | SCENE/SceneAPI.md → "Zenith_ReferenceRepair header" |
| Z2-R-SCENE-108 | Cooked scene container and sections | SCENE/Format.md → "Cooked scene payload"; SCENE/Format.md → "Scene header section"; SCENE/Format.md → "Sections" |
| Z2-R-SCENE-109 | Cooked decoder treats input as untrusted | SCENE/Format.md → "Sections" |
| Z2-R-SCENE-110 | Mismatched cooked versions refused | SCENE/Format.md → "Sections" |
| Z2-R-SCENE-111 | Startup scene loaded before the first tick | SCENE/Loading.md → "Startup scene" |
| Z2-R-SCENE-112 | Preparation touches no world | SCENE/Loading.md → "Preparation operation"; SCENE/Loading.md → "Preparation state machine" |
| Z2-R-SCENE-113 | Preparation is an operation handle | SCENE/Loading.md → "Preparation state machine"; SCENE/SceneAPI.md → "Zenith_SceneLoader header" |
| Z2-R-SCENE-114 | Cancellation at every preparation stage | SCENE/Loading.md → "Cancellation" |
| Z2-R-SCENE-115 | Cancellation stress | SCENE/Loading.md → "Cancellation" |
| Z2-R-SCENE-116 | Completion before cancel reports success | SCENE/Loading.md → "Cancellation" |
| Z2-R-SCENE-117 | Preparation reserves memory with the governor | SCENE/Loading.md → "Preparation state machine" |
| Z2-R-SCENE-118 | Scene memory within its row | SCENE.md → "Rows SCENE owns"; SCENE/SceneAPI.md → "Zenith_SceneLoader header" |
| Z2-R-SCENE-119 | Isolated-world activation within a budget | SCENE/Loading.md → "Out-of-tick activation"; SCENE/Loading.md → "Scene transitions" |
| Z2-R-SCENE-120 | References bound by persistent ID at activation | SCENE/Loading.md → "Out-of-tick activation" |
| Z2-R-SCENE-121 | Additive loads committed in merge order | SCENE/Loading.md → "Additive load sequence"; SCENE/SceneAPI.md → "Zenith_SceneLoader header" |
| Z2-R-SCENE-122 | Additive completion published at a tick boundary | SCENE/Loading.md → "Additive load sequence"; SCENE/Loading.md → "Publication service" |
| Z2-R-SCENE-123 | Additive activation is atomic at the publication tick | SCENE/Loading.md → "Additive load sequence" |
| Z2-R-SCENE-124 | Additive activation retries after merge rejection | SCENE/Loading.md → "Additive load sequence" |
| Z2-R-SCENE-125 | Oversized additive scenes fail at once | SCENE/Loading.md → "Additive load sequence" |
| Z2-R-SCENE-126 | Duplicate additive instances derive IDs | SCENE/Loading.md → "Instance identity"; SCENE/Loading.md → "Preparation state machine"; SCENE/Prefabs.md → "Derivation" |
| Z2-R-SCENE-127 | Additive unload | SCENE/Loading.md → "Unload" |
| Z2-R-SCENE-128 | Additive scene set states | SCENE/Loading.md → "Additive scene set"; SCENE/SceneAPI.md → "Zenith_AdditiveScenes header" |
| Z2-R-SCENE-129 | Additive replay determinism | SCENE/Loading.md → "Principles"; SCENE/Loading.md → "Additive load sequence" |
| Z2-R-SCENE-130 | Transition preparation in isolation | SCENE/Loading.md → "Scene transitions" |
| Z2-R-SCENE-131 | Transition swaps after the published load | SCENE/Loading.md → "Scene transitions" |
| Z2-R-SCENE-132 | Transition carry-over keeps persistent IDs | SCENE/Loading.md → "Carry-over" |
| Z2-R-SCENE-133 | In-flight results discarded at a transition | SCENE/Loading.md → "In-flight asynchronous results across a transition" |
| Z2-R-SCENE-134 | Transition cancellation and refusal | SCENE/Loading.md → "Cancellation"; SCENE/Loading.md → "Scene transitions" |
| Z2-R-SCENE-135 | Scripts request transitions through commands | SCENE/Loading.md → "Scene transitions"; SCENE/SceneAPI.md → "Script bindings" |
| Z2-R-SCENE-136 | SCENE counters | SCENE.md → "Counters"; SCENE/Tests.md → "Diagnostics and binding tests" |
| Z2-R-SCENE-137 | SCENE log categories and dumps | SCENE.md → "Log categories"; SCENE/Format.md → "Scene diagnostic dump"; SCENE/Tests.md → "Diagnostics and binding tests" |
| Z2-R-SCENE-138 | Scene script bindings | SCENE/SceneAPI.md → "Script bindings"; SCENE.md → "Luau errors versus asserts"; SCENE/Tests.md → "Diagnostics and binding tests" |
| Z2-R-SCENE-139 | Scaffold template scenes are valid | SCENE/Format.md → "Example scene document"; SCENE/Tests.md → "Golden fixtures" |
| Z2-R-SCENE-140 | Scene open within the editor row | SCENE.md → "Rows SCENE is measured against" |
| Z2-R-SCENE-141 | Structure-aware merge support | SCENE.md → "Merge support for CONTENT"; SCENE/Prefabs.md → "Override semantics per property kind" |
| Z2-R-SCENE-142 | Scene work on Android | SCENE.md → "Supported matrix" |
| Z2-R-SCENE-143 | Sort-key insertion algorithm | SCENE/Format.md → "Sort key insertion algorithm"; SCENE/Prefabs.md → "Sort keys for edits"; SCENE/SceneAPI.md → "Zenith_SceneDocument header" |
| Z2-R-SCENE-144 | Sort-key exhaustion and rebalance | SCENE/Format.md → "Sort key insertion algorithm"; SCENE/Prefabs.md → "Sort keys for edits" |
| Z2-R-SCENE-145 | Unknown members anchored at write time | SCENE/DocumentFramework.md → "Canonical rules"; SCENE/DocumentFramework.md → "Unknown members across migration steps" |
| Z2-R-SCENE-146 | Unknown-member guard around every step | SCENE/DocumentFramework.md → "Unknown members across migration steps" |
| Z2-R-SCENE-147 | Derived IDs depend on no open decision | SCENE/Prefabs.md → "Derivation"; SCENE/SceneAPI.md → "Zenith_AuthoringID header"; SCENE/DocumentFramework.md → "Digests" |
| Z2-R-SCENE-148 | Nested override targets store derived IDs | SCENE/Format.md → "Resolved ID space"; SCENE/Prefabs.md → "Derivation" |
| Z2-R-SCENE-149 | Derived components removed with the local transform | SCENE/Transforms.md → "Propagation algorithm"; SCENE/Transforms.md → "Propagation state"; SCENE/Transforms.md → "Propagation system" |
| Z2-R-SCENE-150 | Low-disk refusal before a journaled write | SCENE/DocumentFramework.md → "Journaled write"; SCENE/DocumentFramework.md → "Zenith_DocumentStorage header" |
| Z2-R-SCENE-151 | Pending journals reported over read-only storage | SCENE/DocumentFramework.md → "Journaled write"; SCENE/DocumentFramework.md → "Ownership split with ASSET" |
| Z2-R-SCENE-152 | Flat prefabs at Stage 3 | SCENE/Prefabs.md → "Flat prefabs at Stage 3"; SCENE/SceneAPI.md → "Zenith_PrefabResolver header" |
| Z2-R-SCENE-153 | Document type IDs are stable-name type IDs | SCENE/DocumentFramework.md → "Zenith_MigrationRegistry header" |
| Z2-R-SCENE-154 | Additive loads fail when the publication source is full | SCENE/Loading.md → "Publication service"; SCENE/SceneAPI.md → "Zenith_AdditiveScenes header" |
| Z2-R-SCENE-155 | Children rejoin when a parent regains its local transform | SCENE/Transforms.md → "Propagation algorithm"; SCENE/Transforms.md → "Propagation state" |
| Z2-R-SCENE-156 | Propagation filters on the previous read version | SCENE/Transforms.md → "Propagation state"; SCENE/Transforms.md → "Propagation algorithm"; SCENE.md → "Cancellation" |
| Z2-R-SCENE-157 | Document pointers address members by exact key | SCENE/DocumentFramework.md → "Document pointers"; SCENE/DocumentFramework.md → "Document tree" |
| Z2-R-SCENE-158 | Script additive loads have one completion ID | SCENE/Loading.md → "Script requests"; SCENE/Loading.md → "Additive load sequence"; SCENE/SceneAPI.md → "Zenith_AdditiveScenes header"; SCENE/SceneAPI.md → "Script bindings" |
| Z2-R-SCENE-159 | Tick-boundary and publication handlers change only SCENE state | SCENE/Loading.md → "Tick-boundary handler"; SCENE/Loading.md → "Publication service"; SCENE/SceneAPI.md → "Zenith_AdditiveScenes header" |
| Z2-R-SCENE-160 | Scene asset kinds conform to ASSET | SCENE/SceneAPI.md → "Zenith_SceneAssets header"; SCENE/Format.md → "Files and virtual paths" |
| Z2-R-SCENE-161 | Scene and prefab cook steps | SCENE/Prefabs.md → "Scene and prefab cook steps"; SCENE/Prefabs.md → "Cooking resolved prefabs"; SCENE.md → "Cook step" |
| Z2-R-SCENE-162 | Storage-root writer lock around every framework write | SCENE/DocumentFramework.md → "Storage-root writer lock"; SCENE/DocumentFramework.md → "Zenith_DocumentStorage header"; SCENE/DocumentFramework.md → "ZenithDocumentHost" |
| Z2-R-SCENE-163 | Additive set enumeration for saves | SCENE/ModuleSeams.md → "Saved instance enumeration"; SCENE/ModuleSeams.md → "Activation hook declarations" |
| Z2-R-SCENE-164 | Additive set restore entry | SCENE/ModuleSeams.md → "Restore algorithm" |
| Z2-R-SCENE-165 | Restored loading instances re-requested under the recorded completion ID | SCENE/ModuleSeams.md → "Restore algorithm"; SCENE/ModuleSeams.md → "Additive set save and restore" |
| Z2-R-SCENE-166 | Activation hook declarations | SCENE/ModuleSeams.md → "Activation hook declarations" |
| Z2-R-SCENE-167 | Activation hook call points | SCENE/ModuleSeams.md → "Hook call points" |
| Z2-R-SCENE-168 | Gated preparation state for tagged loads | SCENE/ModuleSeams.md → "Gated preparation state"; SCENE/Loading.md → "Preparation state machine" |
| Z2-R-SCENE-169 | One discarded publication per tagged load unloaded while loading | SCENE/ModuleSeams.md → "Discarded publications of tagged loads"; SCENE/Loading.md → "Tick-boundary handler" |
| Z2-R-SCENE-170 | Discarded publications are recorded and decided at publication | SCENE/ModuleSeams.md → "Discarded publications of tagged loads"; SCENE/Loading.md → "Publication service" |
| Z2-R-SCENE-171 | Preparation end reported once per tagged load | SCENE/ModuleSeams.md → "Hook captured per load" |
| Z2-R-SCENE-172 | The hook is captured per load | SCENE/ModuleSeams.md → "Hook captured per load" |
| Z2-R-SCENE-173 | Component amendment before the init record is final | SCENE/ModuleSeams.md → "Component amendment" |
| Z2-R-SCENE-174 | Live instances in ascending order | SCENE/ModuleSeams.md → "Live instances and capture coherence" |
| Z2-R-SCENE-175 | Capture coherence input for the additive-set participant | SCENE/ModuleSeams.md → "Live instances and capture coherence" |
| Z2-R-SCENE-176 | Cancelled-preparation counters per state and the preparation hold | SCENE/ModuleSeams.md → "Preparation probe and hold"; SCENE/Loading.md → "Cancellation" |
| Z2-R-SCENE-177 | The ASSET cause of an unavailable asset is published | SCENE/ModuleSeams.md → "Asset error detail" |
| Z2-R-SCENE-178 | Restore re-requests are reported to the hook | SCENE/ModuleSeams.md → "Restore re-requests and the hook" |
| Z2-R-SCENE-179 | World origin resource | SCENE/ModuleSeams.md → "Zenith_WorldOrigin header" |
| Z2-R-SCENE-180 | Origin shift plays last in its sync point | SCENE/ModuleSeams.md → "Origin shift playback" |
| Z2-R-SCENE-181 | Origin-shifted lifecycle event | SCENE/ModuleSeams.md → "Origin shifted lifecycle event" |
| Z2-R-SCENE-182 | Fluid query interface in ZenithScene | SCENE/ModuleSeams.md → "Zenith_FluidQuery header" |

## Cross-area notes

The rows S1 to S25 of `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md` list what SCENE needs from, or offers to, ECS, CORE, ASSET, ARCH, MOD, SCRIPT, SAVE, EDIT, GAME, PHYS, CINE and BUDGET; each assumption block in this family names its row or its part of cross-doc item X109. The post-signoff patch of 2026-10-09 answered rows ST2, ST3 (STREAM), SV4 (SAVE), EN26 (ENV), PH8 and PH20 (PHYS) in `Zenith2/Docs/Design/SCENE/ModuleSeams.md`; its own asks are rows S25 (ECS) and the open items ST21 (SCRIPT, MOD), ST22 (ASSET), X227 (a) and X264 (j).
