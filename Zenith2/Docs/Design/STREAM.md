# STREAM design — World streaming

The design of the STREAM area: runtime world streaming. A streamed world is a persistent layer (the world's main scene) plus a cooked spatial partition of up to eight uniform runtime grids whose cells are cooked scenes; STREAM decides, every tick and from simulation state alone, which cells are required, loads them as SCENE additive instances with priorities, admits their activation under dependency and pacing rules, cancels them at every stage, persists the state of persistent entities across unload and reload, repairs cross-cell references, rebases the world origin for large coordinates, shows hierarchical level-of-detail (HLOD) proxies for cells that are not loaded, and coordinates the physics, navigation and audio content of cells through grid roles, cross-grid dependencies and region flags. It elaborates `Zenith2/Docs/Architecture.md` → "Memory governor", `Zenith2/Docs/Architecture.md` → "Determinism guarantee and mechanisms", `Zenith2/Docs/Scripting.md` → "Merge-time capacity and asynchronous results" and `Zenith2/Docs/Contracts.md` → "Threading and concurrency contract", and never restates their bullets. A detail designed here beyond the specification is marked `<!-- elaboration -->`; an interface this design assumes from an area that has not yet declared it is fenced `<!-- assumption: pending <AREA> -->` and recorded in `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md` (rows ST1 to ST22). The requirement catalogue is `Zenith2/Docs/Requirements/STREAM.md`.

This entry document holds the skeleton and the top-level contracts; four split files hold the rest (see "Document family").

## Purpose and scope

### What STREAM owns

| Piece | Where | Specified in |
|---|---|---|
| the module `Streaming`: its two entry points, its module instance, its per-world attachment | `ZenithStreaming` | `Zenith2/Docs/Design/STREAM/PublicAPI.md` → "Zenith_Module_Streaming header" |
| spatial partitioning: runtime grids, cells, grid roles, radii, cross-grid dependencies, the cell-key space | `ZenithStreaming` | `Zenith2/Docs/Design/STREAM/Formats.md` → "Streaming world cooked file"; `Zenith2/Docs/Design/STREAM/Runtime.md` → "Distance and band computation" |
| the asset kind `Streaming.World` (the cooked `.zstream` partition index), its reader, its writer and its kind loader | `ZenithStreaming` | `Zenith2/Docs/Design/STREAM/Formats.md` → "Streaming world cooked file"; `Zenith2/Docs/Design/STREAM/PublicAPI.md` → "Zenith_StreamingWorldAsset header" |
| the streaming world authoring document (`*.streamworld.json`), its schema, validation and migration chain; the cook step `stream.world` | `ZenithStreaming`, `ZenithCook` | `Zenith2/Docs/Design/STREAM/Formats.md` → "Streaming world document"; § "Authoring and cooking" |
| asynchronous cell loading over SCENE additive instances, priorities, admission, pacing, unloading, the cell state machine | `ZenithStreaming` | `Zenith2/Docs/Design/STREAM/Runtime.md` → "Cell state machine", "Load ordering and admission" |
| cancellation of a cell at every stage (I/O, decompression, GPU upload, preparation, activation), completion reporting, ownership of partial results, the 1,000-cycle stress | `ZenithStreaming` | `Zenith2/Docs/Design/STREAM/Runtime.md` → "Cancellation at every stage" |
| large-coordinate handling: absolute double-precision cell anchors, the root translation of activated cells, the origin-rebase policy | `ZenithStreaming` (policy); the shift mechanism is SCENE's (row ST3) | `Zenith2/Docs/Design/STREAM/Runtime.md` → "Large coordinates and origin rebasing" |
| cross-cell references: the cooked reference table, binding at activation, unbinding at unload | `ZenithStreaming` | `Zenith2/Docs/Design/STREAM/Runtime.md` → "Cross-cell references" |
| persistent entities: persistence modes, the cell delta store, tombstones, capture at unload, restore at activation | `ZenithStreaming` | `Zenith2/Docs/Design/STREAM/Runtime.md` → "Persistent entities" |
| HLOD: the node hierarchy, the display cut, residency of proxy meshes, the HLOD extractor | `ZenithStreaming` | `Zenith2/Docs/Design/STREAM/Runtime.md` → "HLOD selection and extraction" |
| coordinated physics, navigation and audio streaming: grid roles, dependencies, guard bands, the region flag every region-bound entity carries | `ZenithStreaming` | `Zenith2/Docs/Design/STREAM/Runtime.md` → "Region states and coordinated subsystems" |
| the streaming memory consumer and the Streaming row of the governor's shedding table; the double-residency reservation of HLOD-covered prefetch | `ZenithStreaming` | `Zenith2/Docs/Design/STREAM/Runtime.md` → "Memory pressure and transition peaks" |
| STREAM's components, configuration keys, script bindings, counters, zones, logs, dump and debug view | `ZenithStreaming` | `Zenith2/Docs/Design/STREAM/PublicAPI.md`; § "Diagnostics and profiling" |
| every STREAM test, fixture, fuzz target and integration suite | test targets | `Zenith2/Docs/Design/STREAM/Tests.md` |

### What STREAM does not own

| Not owned | Owner | Why |
|---|---|---|
| the asset loader, residency, decode back-pressure, the request priorities, the keep-alive cache, development reload | ASSET | `Zenith2/Docs/Design/ASSET/Loading.md` → "Publication to simulation"; STREAM requests loads and never re-specifies them |
| scene preparation, additive instances, their activation command buffer, transform propagation, the shift of transforms at an origin rebase | SCENE | `Zenith2/Docs/Design/SCENE/Loading.md` → "Additive scenes"; STREAM drives SCENE through the activation hook of row ST2 and the shift command of row ST3 |
| texture streaming (reallocate-and-swap, mip residency), the swap mechanism and its peak reservation | GEOM (policy), RHI (`Flux_StreamingSwap`, ruling GB32) | `Zenith2/Docs/Design/GEOM/Textures.md` → "Reallocate-and-swap realisation"; STREAM never touches a texture |
| the generic pool allocator and every GPU upload | RHI (RB12), GEOM | uploads of cell assets are their kind owners'; STREAM's cancellation reaches them through ASSET |
| mesh LOD within a mesh | GEOM | `Zenith2/Docs/Design/GEOM/Meshes.md` → "LOD selection"; HLOD is world-scale and STREAM's |
| world partition authoring, data layers, the partition cook that writes cell scenes, the HLOD and navigation bakes | WORLD | Z2-I-WORLD-01 to -09; STREAM owns the runtime contract those tools write to (§ "Authoring and cooking") |
| static colliders, bodies and their freezing; navmesh tiles, agents and paths; audio banks, emitters and voices | PHYS, AI, AUDIO | STREAM coordinates through data (cell content, grid roles, the region flag), never through their APIs ([Z2-N-ARCH-850]) |
| the environment content of a cell (terrain tiles, foliage cells) and its own activation | ENV | `Zenith2/Docs/Design/ENV/Bake.md` → "Environment cells and STREAM cells" |
| probe, irradiance-volume and lightmap residency once their components exist | LIGHT | `Zenith2/Docs/Design/LIGHT/Reflections.md` → "Probe residency and streaming"; `Zenith2/Docs/Design/LIGHT/GI.md` → "Baked GI residency and streaming" |
| saves, the world snapshot at phase 11, replay records | SAVE | STREAM keeps its save-relevant state in components (`Zenith2/Docs/Design/STREAM/Runtime.md` → "Save and restore"; row ST14) |
| interest management and network relevancy | NET (Stage 7, Z2-D-073) | a networked relevancy grid is NET's; STREAM serves one simulation |
| virtualized geometry and virtual texturing | ADVGFX (Z2-D-043) | HLOD proxies are ordinary GEOM meshes |

Excluded outright: mod-supplied cells or worlds (Z2-D-075); dynamic plugins registering streaming kinds (Z2-D-024); streaming decisions taken from wall-clock time, I/O timing, memory pressure or the render camera (Z2-D-019, [Z2-N-ARCH-812]).

### Inventory items covered

STREAM covers every item of `Zenith2/Docs/Requirements/Inventory.md` → "Inventory v1", table "STREAM World streaming", all required at S4: `Z2-I-STREAM-01` (spatial partitioning), `-02` (async cell loading), `-03` (priorities of cell loads), `-04` (cancellation of cell loads, decomposed per the streaming-cancellation example), `-05` (large-coordinate handling), `-06` (cross-cell references), `-07` (persistent entities), `-08` (HLOD), `-09` (coordinated physics streaming), `-10` (coordinated navigation streaming; library D-SEL-NAV, open, Stage 1/2), `-11` (coordinated audio streaming). The mapping is `Zenith2/Docs/Requirements/STREAM.md` → "Coverage of inventory items". STREAM owns no item of another area; it supplies the cell residency that LIGHT's rows LB20 and ENV's row EN7 consume (§ "Areas that consume STREAM").

### Document family

| File | Holds | Its `##` headings |
|---|---|---|
| `Zenith2/Docs/Design/STREAM/PublicAPI.md` | every public header of `ZenithStreaming` and the internal declarations other files rely on | Scope of this file · Header list · Zenith_StreamingTypes header · Zenith_StreamingErrors header · Zenith_StreamingComponents header · Zenith_StreamingWorldAsset header · Zenith_StreamingWorldWriter header · Zenith_StreamingState header · Zenith_StreamingCommands header · Zenith_Module_Streaming header · Internal declarations · Script bindings · Assumed declarations of other areas |
| `Zenith2/Docs/Design/STREAM/Formats.md` | cooked, document, payload and dump formats; in-memory records | Scope of this file · Common rules · Streaming world cooked file · Streaming world document · Cell scene rules · Cell delta encoding · Module command payloads · Stream dump format · In-memory runtime records |
| `Zenith2/Docs/Design/STREAM/Runtime.md` | every algorithm and state machine | Scope of this file · World binding · Per-tick update · Distance and band computation · Desired-set algorithm · Load ordering and admission · Cell state machine · Action buffers and rejection · Cancellation at every stage · Ownership of partial results · Completion reporting · Cross-cell references · Persistent entities · Region states and coordinated subsystems · Large coordinates and origin rebasing · Teleports and readiness · The streaming service · HLOD selection and extraction · PSO groups and the preparation gate · Memory pressure and transition peaks · Save and restore · World transitions and shutdown · Replay and determinism · Headless runs · Complexity |
| `Zenith2/Docs/Design/STREAM/Tests.md` | every test | Test files · Unit tests · Cook and format tests · Integration tests · Stress and determinism tests · Fuzz target · Death and fault-injection tests · Fixtures and generated content · Gate steps per test group |

### Design choices at a glance

This table is the register of the family's elaborations: each choice it names is a design decision beyond the plan's Appendix-A row, and the section that specifies it carries an `<!-- elaboration -->` marker (grids: Formats.md "Streaming world cooked file"; the cell state machine and CANCELLING held until the discarded publication: Runtime.md "Cell state machine"; the snapshot pool: PublicAPI.md "Zenith_StreamingSnapshotPool"; the delta encoding: Formats.md "Cell delta encoding" and Runtime.md "Persistent entities"; the persisted cell status on per-cell holders and the overrides on the world reference: Runtime.md "Persisted cell status", "Overrides on the world reference"; HLOD: Runtime.md "HLOD selection and extraction"; the rebase policy: Runtime.md "Large coordinates and origin rebasing"; the PSO gate: Runtime.md "PSO groups and the preparation gate"; coordination: Runtime.md "Physics coordination", "Navigation coordination", "Audio coordination").

<!-- informative -->
| Choice | Taken | Rejected alternative and why |
|---|---|---|
| partition | up to eight uniform 2D or 3D runtime grids per world, each with a role and its own radii (the UE5 World Partition "runtime grids" model) | an octree or loose quadtree: variable cell sizes make loading ranges content-dependent and dependency closure harder to bound; grids give O(1) cell lookup and fixed worst-case active counts the cook can check |
| cell content | a cell is a cooked SCENE scene loaded as an additive instance | a STREAM-private instantiation path would duplicate SCENE's transactional, all-or-nothing activation and its logged publication |
| determinism | every decision that changes the world (which cells load, unload, activate, when the origin moves) is a pure function of simulation state and configuration; I/O timing reaches simulation only through SCENE's logged publication ([Z2-N-SCRIPT-336]) | deciding from the render camera, memory pressure or load completion order would make two runs with the same inputs diverge |
| HLOD | presentation only: an extractor writes proxy-mesh instances under module keys; no entity, no save, no simulation cost | HLOD proxies as entities would consume SCENE's 256 additive instances, enter saves and the world hash, and could not be shed under memory pressure ([Z2-N-ARCH-812]) |
| coordination | data-coupled: grid roles, cross-grid activation dependencies and the region flag component other modules read by stable name | a STREAM API PHYS, AI and AUDIO call would be a module-to-module edge the library graph forbids ([Z2-N-ARCH-074]) |
| large coordinates | absolute double cell anchors, cell scenes authored relative to their anchor, a root translation at activation, and quantised origin rebases executed by SCENE | per-entity double positions or cell-relative transforms in every component: a layout change across every engine library |
| persistence | per-cell delta records in `Zenith.Streaming.CellDelta` components and every subsequently read status of a cell in `Zenith.Streaming.CellStatus` on the same holder, so saves, replays and the world hash carry them with no SAVE-specific code and a restore resumes them exactly (SAVE row SV10 adopts this) | a STREAM-owned side store would need a `Zenith_SaveParticipant` (ZenithSave declares one, row SV17) and its own section format, migration chain and world-hash contribution, all of which the component path already has; rebuilding the status from SCENE's set alone would lose retry ticks, delays, backoffs and tombstones, which no instance state records |

Zenith (the reference engine at `C:/dev/Zenith/Zenith`) streamed only terrain LODs (`Flux/Terrain/Flux_TerrainStreamingManagerImpl.h`): a synchronous per-frame loop over a 16-chunk radius around the camera, at most 8 chunk loads per frame, no cancellation, no world partition and no persistence. Zenith2 keeps the bounded-per-frame idea and the "active set rebuilt around the viewer" shape and replaces everything else.
<!-- /informative -->

### Reconciliations with other documents

Additions STREAM needs from documents it cannot edit; each is fenced in this family by an `assumption: pending <AREA>` (or `pending <X item>`) block and carried by the cross-area row named in `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md`; a need that edits a core document is also a queued item of `C:/dev/Zenith2-drafts/reviews/CROSS_DOC_ITEMS.md`, applied in the core-doc batch.

| # | Need | Owner | Row | Core-doc item |
|---|---|---|---|---|
| R1 | `ZenithStreaming`'s declared dependencies gain `ZenithReflect` (its component and resource types), `ZenithFlux` and `ZenithShadersGenerated` (the HLOD extractor and the PSO precacher); the module's layer stays 7 | `Zenith2/Docs/Architecture.md` → "Library dependency graph" | ST1 | X225 (applied) |
| R2 | `Zenith_SceneActivationHook` (eight members, with `AmendComponents` and `Zenith_SceneComponentAmender`), `Zenith_SceneWorldState::SetActivationHook`, `Zenith_AdditiveLoadDesc::m_ulHookTag`, `Zenith_AdditiveSceneSet::GetLiveInstances` and `GetErrorDetail`, `Zenith_ScenePublicationSink::SetPublicationResolver`, the hook's call points and obligations (a) to (j): the discarded publication for every outcome, its kind decided at publication by a resolver, `OnPreparationEnded` for a removed pending preparation, the hook captured per load, the amendment, the enumeration, save fidelity (three COHERENT cases), test support with three named SCENE tests, the ASSET cause of `ASSET_UNAVAILABLE`, the report of a refused restore re-request | SCENE | ST2 | — (a SCENE post-signoff patch) |
| R3 | the world-origin resource `Zenith.WorldOrigin` and the command `Scene.ShiftWorldOrigin`, played last in step 7 of its sync point | SCENE | ST3 | — |
| R4 | the reflection property flag `PROPERTY_FLAG_WORLD_POSITION` and the schema attribute `"worldPosition": true`, which the shift playback honours; the C++ declaration form of a `container<u8>` member | ECS; `Zenith2/Docs/Architecture/DataDefinedComponents.md` → "The schema file" | ST4 | X227 (a) (applied: the schema half); X364 (the ECS design half: `PROPERTY_FLAG_WORLD_POSITION = 1u << 7` and `ElementType(PROPERTY_TYPE_U8).MaxCount(65535)`) |
| R5 | `Zenith_SceneWorldState* Zenith_WorldAttachContext::GetSceneState()` | MOD; `Zenith2/Docs/Architecture/CompositionRoot.md` → "Startup sequence" | ST5 | X227 (b) (applied) |
| R6 | kind code 35 `ASSET_KIND_STREAMING_WORLD` and the cook error codes `COOK_ERROR_STREAM_*` | ASSET | ST6 | — |
| R7 | streaming-cell PSO groups keyed by STREAM's group ID; at an origin change a full-state extraction that translates every previous-frame transform and previous view transform by −S instead of resetting histories | RPROD | ST7 | — |
| R8 | the CRITICAL shedding cell of the Streaming row and [Z2-N-ARCH-815]'s deferral read as presentation-only; a simulation-required load reserves its double residency for the whole load without deferring, and releases the covering proxy on refusal | `Zenith2/Docs/Architecture.md` → "Memory governor" | ST17 | X226 (applied) |
| R9 | the phase-1 row of the twelve phases names `Streaming.Update`, ordered before `Scene.ActivateAdditive` | `Zenith2/Docs/Architecture/FramePhases.md` → "The twelve phases" | ST5 | X227 (c) (applied) |
| R10 | the pending row "Cell activation entities per tick", the `.zstream` and cell-delta parser limits, and V16's tie to Z2-B-ECS-003 | `Zenith2/Docs/Budgets.md`; `Zenith2/Docs/Architecture.md` → "Security architecture" | ST16 | X228 (applied) |
| R11 | Stage 3's focused-workload confirmation excludes STREAM's rows, first confirmed at Stage 4 | `Zenith2/Docs/Roadmap.md` → "Stage 3 Playable authoring slice" | — | X229 (applied) |
| R12 | ENV's float-precision acceptance reconciled with STREAM's 4,096 m precision radius | ENV | ST19 | — |
| R13 | a test-only stage hold and per-stage cancellation counters for ASSET's pipeline stages | ASSET | ST20 | — |
| R14 | a per-source publication resolver on SCRIPT's `Zenith_AsyncResultTable` (`SetPublicationResolver`, live mode, before logging), ZenithRuntime's sink forwarding SCENE's resolver to it, and the replay rule that drops a local completion of an ID published from the log | SCRIPT, MOD | ST21 | X271 (`Scripting/PublicTypes.md`, applied), with a SCRIPT post-signoff patch |
| R15 | `ASSET_ERROR_DEPENDENCY_FAILED` carries the failed dependency's root `Zenith_AssetErrorCode` in its detail | ASSET | ST22 | — |
| R16 | `Zenith_WorldPlaybackAccess::GetWorldTag`, so STREAM's playbacks find their world's state | ECS; `Zenith2/Docs/Architecture/ECSModel.md` | — | X238 |

## Dependencies

### Libraries STREAM depends on

| Library | Layer | Kind | `DEPENDS` |
|---|---|---|---|
| `ZenithStreaming` | 7 | module (`KIND MODULE`, `MODULE_NAME Streaming`) | ZenithRuntime, ZenithAssets, ZenithScene, ZenithECS, ZenithJobs, ZenithPlatform, ZenithCore, ZenithProfiling (the row of `Zenith2/Docs/Architecture.md` → "Library dependency graph"), plus ZenithReflect, ZenithFlux and ZenithShadersGenerated (reconciliation R1) |

No third-party library. What each edge is for: ZenithCore — results, owned pointers, arenas, pools, `Zenith_Array`, `Zenith_HashMap`, `Zenith_BoundedQueue`, `Zenith_PersistentID`, `Zenith_Hash64`, maths and bounds, the memory governor, logging, configuration values and fault injection; ZenithReflect — the type builder for STREAM's components and resources, field sets for the delta store, the script binding ABI; ZenithECS — command buffers, command kinds, systems, chunk views, queries, the world simulation; ZenithScene — additive instances, the activation hook, the lifecycle listener, transform components, the world origin; ZenithAssets — the asset kind loader, the registry, load requests, the cooked-file reader and writer; ZenithJobs — job contexts of the nodes; ZenithPlatform — the `Zenith_Config` facade the start context returns; ZenithRuntime — the module interfaces; ZenithFlux and ZenithShadersGenerated — `Flux_RenderWorldWriter`, `Flux_RenderInstanceDelta`, `Flux_MeshResource` and `Flux_PSOPrecacher` (generated headers reach STREAM through `Flux_RenderWorld.h`, [Z2-N-ARCH-083]); ZenithProfiling — zones and counters.

`Zenith2/Docs/Architecture.md` → "Declared direct dependencies" lists ZenithStreaming's edges as ZenithRuntime, ZenithAssets, ZenithScene, ZenithECS, ZenithJobs, ZenithPlatform, ZenithCore and ZenithProfiling, plus the three this design needs, since core-doc item X225 (row ST1): a module's `Zenith_Module_<M>_RegisterTypes` takes `Zenith_TypeRegistry&` (ZenithReflect), and STREAM's HLOD extractor and PSO-group requests need ZenithFlux, as ZenithEnvironment's row already declares, so the row carries ZenithReflect, ZenithFlux and ZenithShadersGenerated, and the STREAM target's `DEPENDS` written in "ZenithStreaming target" matches it.

### Headings this design relies on

| Heading | For |
|---|---|
| `Zenith2/Docs/Design/SCENE/Loading.md` → "Additive scenes", "Additive load sequence", "Preparation state machine", "Publication service", "Cancellation", "Tick-boundary handler"; `Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_AdditiveScenes header", "Zenith_SceneLoader header", "Zenith_SceneWorld header", "Zenith_LifecycleListener header", "Zenith_TransformHierarchy header" | loading cells as additive instances, their states, the listener |
| `Zenith2/Docs/Design/SCENE/Transforms.md` → "Transform components", "Teleport" | source positions, the shifted transforms, teleports |
| `Zenith2/Docs/Design/SCENE/Format.md` → "Cooked scene payload"; `Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "Zenith_MigrationRegistry header" | cell scenes in the cook; the document type registration |
| `Zenith2/Docs/Design/ASSET.md` → "Asset kinds"; `Zenith2/Docs/Design/ASSET/RuntimeAPI.md` → "Zenith_AssetKindLoader", "Zenith_AssetLoader", "Zenith_AssetLoadRequest", "Zenith_AssetRegistry", "Zenith_AssetHandle", "Priorities and states", "Zenith_CookedFileWriter", "Parsing and validation functions" | the world kind, its loader, the booster and prefetch requests |
| `Zenith2/Docs/Design/ASSET/Formats.md` → "Header layout", "Section table", "Checks", "Limits" | the `.zstream` container |
| `Zenith2/Docs/Design/ASSET/Loading.md` → "Publication to simulation", "Cancellation points by stage", "Request deduplication and priorities", "Dependency loading", "Priority reserves", "Memory pressure responses" | what a cell load costs and how it cancels below STREAM |
| `Zenith2/Docs/Design/ASSET/Cook.md` → "Zenith_CookStepDesc", "Zenith_CookStep", "Zenith_CookContext", "Source documents in cook steps", "Step registration functions" | the `stream.world` cook step |
| `Zenith2/Docs/Design/ECS/WorldAPI.md` → "Zenith_CommandBuffer header", "Zenith_CommandKinds header", "Zenith_WorldSimulation header", "Zenith_ECSObservers header", "Zenith_EntityTable header", "Zenith_ChunkView header", "Zenith_Query header"; `Zenith2/Docs/Design/ECS/SystemAPI.md` → "Zenith_System header", "Zenith_SystemContext header", "Zenith_MergeContext header" | systems, commands, playbacks, observation |
| `Zenith2/Docs/Design/ECS/Scheduling.md` → "Sync-point playback", "Epilogue and extractors", "Capacity acceptance" | when STREAM's records play and how buffers are accepted |
| `Zenith2/Docs/Design/ECS/Reflect.md` → "Zenith_TypeBuilder", "Zenith_FieldSet header", "Binary field-set record"; `Zenith2/Docs/Design/ECS/Schemas.md` → "Schema migration"; `Zenith2/Docs/Design/ECS/Queries.md` → "Change-filtered queries" | component registration, the delta store, the reset rule |
| `Zenith2/Docs/Design/RPROD/Extractors.md` → "Instance keys", "Flux_RenderInstanceDelta layout", "Full-state extraction", "Extractor registration"; `Zenith2/Docs/Design/RPROD/PSO.md` → "Precache API", "PSO creation schedule realisation"; `Zenith2/Docs/Design/RPROD/Renderer.md` → "Flux_Renderer header" | the HLOD extractor and the cell PSO groups |
| `Zenith2/Docs/Design/GEOM/Meshes.md` → "Flux_MeshResource header" | HLOD proxy meshes |
| `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_Array", "Zenith_HashMap and Zenith_HashSet", "Zenith_BoundedQueue"; `Zenith2/Docs/Design/CORE/Memory.md` → "Pool internals", "Fault-injection support"; `Zenith2/Docs/Design/CORE/Maths.md` → "Coordinate systems", "Numerical conventions", "Bounds types", "Zenith_Hash64"; `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Consumer registration"; `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Configuration store", "Structured logging" | containers, pools, maths, the governor, configuration and logs |
| `Zenith2/Docs/Design/JOBS/PublicAPI.md` → "Zenith_JobContext", "Zenith_JobSystem" | node bodies |
| `Zenith2/Docs/Design/MOD.md` → "Module lifecycle state machine"; `Zenith2/Docs/Design/MOD/GameBuild.md` → "Module to target mapping" | the module's lifecycle and its row |
| `Zenith2/Docs/Architecture/CompositionRoot.md` → "Startup sequence", "The frame loop"; `Zenith2/Docs/Architecture/FramePhases.md` → "The twelve phases", "Sync points and the tick boundary", "Per-field authority"; `Zenith2/Docs/Architecture/LifecycleCommands.md` → "Teleport", "World transition", "Shutdown"; `Zenith2/Docs/Architecture/CPUFrameGraph.md` → "Render extraction and pipelining", "Cancellation and shutdown"; `Zenith2/Docs/Architecture/BuildSystem.md` → "The zenith_add_library helper", "Test, conformance and fuzz targets", "Tools, benchmarks and integration suites" | the frame, its phases and the build |
| `Zenith2/Docs/Contracts.md` → "Handle kinds", "Contract blocks per type", "Threading and concurrency contract", "Errors and results", "Compatibility policy per kind", "Layering DAG" | the contracts every STREAM type obeys |
| `Zenith2/Docs/Scripting.md` → "Merge-time capacity and asynchronous results"; `Zenith2/Docs/Scripting/Bindings.md` → "Binding registration and the thunk ABI" | publication rules and the bindings |
| `Zenith2/Docs/Budgets.md` → "Teleport and rapid traversal", "Breach consequences"; `Zenith2/Docs/Rendering.md` → "GPU lifetime and synchronisation", "Recovery" | the measured rows and device loss |

### Upward-callback interfaces STREAM declares

None. STREAM is a module at layer 7; no library depends on it, so it declares no interface for a lower layer to call. Every upward call it receives goes through an interface a lower library declares, which STREAM implements:

| Interface | Declared by | STREAM's implementation | Called |
|---|---|---|---|
| `Zenith_ModuleInstance` | ZenithRuntime | `Zenith_StreamingModuleInstance` | attach and detach per world, main thread |
| `Zenith_AssetKindLoader` | ZenithAssets | `Zenith_StreamingWorldKindLoader` (kind `Streaming.World`) | ZenithAssets' background slices |
| `Zenith_MemoryConsumer` | ZenithCore | `Zenith_StreamingMemoryConsumer` | the governor, frame prologue |
| `Zenith_LifecycleListener` | ZenithScene | `Zenith_StreamingLifecycleListener` (mask: TELEPORTED, DESTROY_PENDING) | the world's sync-point node |
| `Zenith_SceneActivationHook` | ZenithScene (row ST2) | `Zenith_StreamingActivationHook` | sync-point node, tick-boundary handler, preparation threads, the `Scene.ActivateAdditive` node |
| `Zenith_PfnCommandPlayback` | ZenithECS | the four STREAM command kinds | the world's sync-point node, step 7 |
| `Zenith_PfnSystemRun`, `Zenith_PfnExtract` | ZenithECS, ZenithJobs | `Streaming.Update`, `Streaming.Service`, `Streaming.ExtractHLOD` | the frame graph |

### Assumed interfaces pending other areas

The assumed declarations are written out in `Zenith2/Docs/Design/STREAM/PublicAPI.md` → "Assumed declarations of other areas", each in its own assumption block: SCENE's activation hook and origin (rows ST2, ST3), SCRIPT's publication resolver (ST21), ASSET's dependency root cause (ST22), ECS's world-position flag (ST4), MOD's scene-state accessor (ST5), ASSET's kind code and cook errors (ST6), RPROD's origin-change rule (ST7); ECS's `Zenith_WorldPlaybackAccess::GetWorldTag` is core-doc item X238. PHYS, AI and AUDIO consume STREAM's region flag, grid roles and origin by stable name in their own designs, which answer rows ST8 to ST10: PHYS freezes a region-inactive body and builds a cell's bodies at its activation (`Zenith2/Docs/Design/PHYS/Simulation.md` → "Region freeze", "Cell bodies") and shifts its backend world at the shift's own sync point under STREAM's threshold, quantum and interval (`Zenith2/Docs/Design/PHYS/Simulation.md` → "Origin shift"); AI suspends a region-inactive agent and loads and removes navigation tiles with their cells (`Zenith2/Docs/Design/AI/Agents.md` → "Agent state machine"; `Zenith2/Docs/Design/AI/Navigation.md` → "Streamed tiles", "Tile remove procedure", "Origin shift"; the library stays D-SEL-NAV, open, Stage 1/2); AUDIO virtualises a region-inactive emitter and follows the origin (`Zenith2/Docs/Design/AUDIO/Streaming.md` → "Region-bound emitters", "Cells unloading under playing voices", "Origin shifts"). STREAM's own behaviour depends on them only through the data contract of "Region states and coordinated subsystems".

### Areas that consume STREAM

| Consumer | Uses |
|---|---|
| WORLD | writes the streaming world document, the cell scenes and the HLOD proxy meshes to STREAM's rules (row ST13) |
| ENV | its cell content is cell scene content; it reads `Zenith.WorldOrigin` for its render records; its terrain importer's `Environment.TerrainValidate` stage reads the grid cell sizes and origins of the streaming world document and refuses a misaligned grid with `COOK_ERROR_IMPORT_FAILED` and one ERROR diagnostic `ENV_ERROR_INVALID_DESC` (ruling ENV-F7: no cook error code of ENV's own; `Zenith2/Docs/Design/ENV/Bake.md` → "Environment cells and STREAM cells"; row ST11, answering EN7) |
| LIGHT | probes, irradiance volumes and lightmap sets are components of cell content, loaded with the cell as hard dependencies of its scene and released at unload (row ST12, answering LB20) |
| PHYS, AI, AUDIO | read `Zenith.Streaming.RegionBound` and `Zenith.WorldOrigin` by stable name; their cell content lives in grids of their role (rows ST8 to ST10, answered by PHYS's `Zenith2/Docs/Design/PHYS/Simulation.md` → "Region freeze", AI's `Zenith2/Docs/Design/AI/Navigation.md` → "Streamed tiles" and AUDIO's `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Region-bound emitters") |
| SAVE | saves `Zenith.Streaming.CellDelta`, `Zenith.Streaming.CellStatus`, `Zenith.Streaming.Persistent`, `Zenith.Streaming.CellAnchor` and STREAM's source, region and world-reference properties like every component; restores ACTIVE cells from ENTITIES and re-requests LOADING and QUEUED cells with their saved hook tags; reports a capture COHERENT through SCENE's participant in the three cases of Runtime.md "What is saved" (rows ST14, SV10) |
| GAME | puts `Zenith.Streaming.Source` on the possessed pawn and the camera in content, reads `Ready` for loading screens and teleports (row ST18) |
| RPROD | serves the cell PSO groups STREAM requests; reads `Zenith.WorldOrigin` for the full-state extraction (row ST7) |
| DEVT | displays STREAM's counters and profiler rows, draws the debug view, combines STREAM's teleport counters with GEOM's for MP-STREAM (row ST15) |
| EDIT | shows the streaming panel and the cell overrides in play worlds |
| scripts | the components (generated `.d.luau`) and four bindings: three `READ_WORLD` and the tools-only override (`Zenith2/Docs/Design/STREAM/PublicAPI.md` → "Script bindings") |

## Library and file layout

### ZenithStreaming target

`Zenith2/Engine/ZenithStreaming/CMakeLists.txt` is exactly:

```cmake
zenith_add_library(ZenithStreaming KIND MODULE MODULE_NAME Streaming SUPPORTED ZENITH_GPU
	DEPENDS ZenithRuntime ZenithFlux ZenithShadersGenerated ZenithAssets ZenithScene ZenithECS ZenithReflect
		ZenithJobs ZenithPlatform ZenithCore ZenithProfiling
	TEST_DEPENDS ZenithECS_Custom)
```

`SUPPORTED ZENITH_GPU` is every module's matrix (`Zenith2/Docs/Architecture/BuildSystem.md` → "The zenith_add_library helper"; the row of `Zenith2/Docs/Design/MOD/GameBuild.md` → "Module to target mapping": first stage S4); `ZenithStreaming_Tests` therefore builds in `win` and `win-asan`. The fault-point prefix of the library is `0x5357` ("SW").

### Public root headers

| Header | Declares | Section |
|---|---|---|
| `Zenith2/Engine/ZenithStreaming/Zenith_Module_Streaming.h` | the two module entry points | `Zenith2/Docs/Design/STREAM/PublicAPI.md` → "Zenith_Module_Streaming header" |
| `Zenith2/Engine/ZenithStreaming/Zenith_StreamingTypes.h` | limits, enums (grid role, distance metric, cell state, band, persistence mode, region policy, cell override), `Zenith_StreamingCellCoord`, `Zenith_StreamingGroupID` | "Zenith_StreamingTypes header" |
| `Zenith2/Engine/ZenithStreaming/Zenith_StreamingErrors.h` | `Zenith_StreamingErrorCode`, `Zenith_StreamingError`, `Zenith_ToString` | "Zenith_StreamingErrors header" |
| `Zenith2/Engine/ZenithStreaming/Zenith_StreamingComponents.h` | the seven component structs and `Zenith_StreamingCellOverrideSlot`, `Zenith_RegisterStreamingTypes` (which also registers the resource type and the payload structs) | "Zenith_StreamingComponents header" |
| `Zenith2/Engine/ZenithStreaming/Zenith_StreamingWorldAsset.h` | `Zenith_StreamingWorldAsset` (the runtime object of kind `Streaming.World`) and its record views | "Zenith_StreamingWorldAsset header" |
| `Zenith2/Engine/ZenithStreaming/Zenith_StreamingWorldWriter.h` | `Zenith_StreamingWorldDesc`, `Zenith_ValidateStreamingWorld`, `Zenith_EncodeStreamingWorld` and `Zenith_WriteStreamingWorldFile` (the cook and the tests) | "Zenith_StreamingWorldWriter header" |
| `Zenith2/Engine/ZenithStreaming/Zenith_StreamingState.h` | `Zenith_StreamingStateView`, the read-only view tools and the debug panel use | "Zenith_StreamingState header" |
| `Zenith2/Engine/ZenithStreaming/Zenith_StreamingCommands.h` | `Zenith_RecordStreamingCellOverride` | "Zenith_StreamingCommands header" |

### Internal files

| File | Contents |
|---|---|
| `Internal/Zenith_StreamingModule.h`, `.cpp` | `Zenith_StreamingModuleInstance`, `Zenith_Module_Streaming_RegisterTypes`, `Zenith_Module_Streaming_Start`, attach and detach |
| `Internal/Zenith_StreamingConfig.h`, `.cpp` | the configuration keys of `Zenith2/Docs/Design/STREAM/PublicAPI.md` → "Internal declarations" and their reading into `Zenith_StreamingSettings` |
| `Internal/Zenith_StreamingWorldState.h`, `.cpp` | `Zenith_StreamingWorldState` (the world resource `Zenith.Streaming.State`): cell runtime records, sources, admission table, inboxes |
| `Internal/Zenith_StreamingUpdate.cpp` | the `Streaming.Update` system body (Runtime.md "Per-tick update") |
| `Internal/Zenith_StreamingBands.h`, `.cpp` | distances and bands (Runtime.md "Distance and band computation", "Desired-set algorithm") |
| `Internal/Zenith_StreamingCellMachine.h`, `.cpp` | the cell state machine's transition function (Runtime.md "Cell state machine") |
| `Internal/Zenith_StreamingAdmission.cpp` | load ordering, admission, pacing (Runtime.md "Load ordering and admission") |
| `Internal/Zenith_StreamingActions.h`, `.cpp` | action buffers and their observation (Runtime.md "Action buffers and rejection") |
| `Internal/Zenith_StreamingPersistence.h`, `.cpp` | capture, tombstones, the delta encoding and its decoder (Runtime.md "Persistent entities"; Formats.md "Cell delta encoding") |
| `Internal/Zenith_StreamingReferences.cpp` | binding and unbinding cross-cell references (Runtime.md "Cross-cell references") |
| `Internal/Zenith_StreamingRegions.cpp` | region flags and region policies (Runtime.md "Region states and coordinated subsystems") |
| `Internal/Zenith_StreamingRebase.cpp` | the rebase decision (Runtime.md "Large coordinates and origin rebasing") |
| `Internal/Zenith_StreamingHook.h`, `.cpp` | `Zenith_StreamingActivationHook` |
| `Internal/Zenith_StreamingSnapshotPool.h`, `.cpp` | the per-world pool of preparation snapshots and its retiring rule |
| `Internal/Zenith_StreamingCommandKinds.cpp` | the four command kinds and their playback functions |
| `Internal/Zenith_StreamingListener.cpp` | `Zenith_StreamingLifecycleListener` (tombstones and teleports written at the playback, PublicAPI.md "Zenith_StreamingLifecycleListener") |
| `Internal/Zenith_StreamingService.h`, `.cpp` | the `Streaming.Service` epilogue stage: boosters, prefetch, PSO groups, HLOD residency, shedding, counters |
| `Internal/Zenith_StreamingHLOD.h`, `.cpp` | the HLOD cut and the `Streaming.ExtractHLOD` extractor |
| `Internal/Zenith_StreamingMemory.cpp` | `Zenith_StreamingMemoryConsumer` |
| `Internal/Zenith_StreamingWorldKindLoader.h`, `.cpp` | `Zenith_StreamingWorldKindLoader` |
| `Internal/Zenith_StreamingWorldReader.cpp` | parsing and validating `.zstream` sections into `Zenith_StreamingWorldAsset` |
| `Internal/Zenith_StreamingWorldWriter.cpp` | `Zenith_WriteStreamingWorldFile` |
| `Internal/Zenith_StreamingScriptBindings.cpp` | the four bindings |
| `Internal/Zenith_StreamingDiagnostics.cpp` | counters, zones, the `stream.dump` writer, the debug-view records |

Every `Internal/` file is shared across platforms; STREAM has no `Internal/Windows/`, `Internal/Android/` or `Internal/Linux/` file, because it calls no platform API.

### Tests files

| File | Target | Holds |
|---|---|---|
| `Zenith2/Engine/ZenithStreaming/Tests/Zenith_StreamingTypes_Tests.cpp` | `ZenithStreaming_Tests` | limits, key packing, `Zenith_ToString` |
| `Tests/Zenith_StreamingWorldFormat_Tests.cpp` | `ZenithStreaming_Tests` | writer and reader, every reader check |
| `Tests/Zenith_StreamingBands_Tests.cpp` | `ZenithStreaming_Tests` | distances, bands, the desired set, dependency closure |
| `Tests/Zenith_StreamingCellMachine_Tests.cpp` | `ZenithStreaming_Tests` | every cell of the state table |
| `Tests/Zenith_StreamingAdmission_Tests.cpp` | `ZenithStreaming_Tests` | ordering, admission, pacing, caps |
| `Tests/Zenith_StreamingPersistence_Tests.cpp` | `ZenithStreaming_Tests` | capture, encoding, tombstones, restore |
| `Tests/Zenith_StreamingReferences_Tests.cpp` | `ZenithStreaming_Tests` | bind and unbind |
| `Tests/Zenith_StreamingRegions_Tests.cpp` | `ZenithStreaming_Tests` | region flags and policies |
| `Tests/Zenith_StreamingRebase_Tests.cpp` | `ZenithStreaming_Tests` | the rebase rule, root translations |
| `Tests/Zenith_StreamingHLOD_Tests.cpp` | `ZenithStreaming_Tests` | the cut, residency bands, extraction records |
| `Tests/Zenith_StreamingService_Tests.cpp` | `ZenithStreaming_Tests` | boosters, prefetch, PSO groups, shedding |
| `Tests/Zenith_StreamingModule_Tests.cpp` | `ZenithStreaming_Tests` | entry points, fault injection, attach and detach |
| `Tests/Golden/stream-dump-unbound.json`, `Tests/Golden/stream-dump-bound.json` | committed JSON text, hand-written from Formats.md "Stream dump format" | the golden files of `Test_StreamingState_DumpShape` |
| `Tests/Support/Zenith_StreamingTestWorld.h` | header-only ([Z2-N-ARCH-255]) | the generated test world builder of Tests.md "Fixtures and generated content" |
| `Tests/Fuzz/StreamingWorld.cpp` | `Zenith_Fuzz_StreamingWorld` (`win-asan`) | the `.zstream` reader fuzz target |

### ZenithCook files

| File | Holds |
|---|---|
| `Zenith2/Tools/ZenithCook/Internal/Streaming/Zenith_StreamingCookSteps.h`, `.cpp` | `Zenith_RegisterStreamingCookSteps(Zenith_CookStepRegistry&, Zenith_CookServices&)` and the `stream.world` step |
| `Zenith2/Tools/ZenithCook/Internal/Streaming/Zenith_StreamingDocument.h`, `.cpp` | the document reader and validator, `Zenith_RegisterStreamingDocumentTypes(Zenith_MigrationRegistry&)` |
| `Zenith2/Tools/ZenithCook/Tests/Streaming/` | `ZenithCook_Tests` cases of Tests.md "Cook and format tests" |

### Integration suite and fuzz target

| Target | Declaration | Holds |
|---|---|---|
| `Zenith_Integration_Streaming` | `zenith_add_integration_tests(Streaming DEPENDS ZenithRuntime ZenithStreaming ZenithSave ZenithCompression_Zlib ZenithCompression_Zstd ZenithECS_Custom ZenithECS_Flecs ZenithECS_EnTT)` in `Zenith2/Tests/Streaming/` | the headless runtime suites of Tests.md "Integration tests" and "Stress and determinism tests" |
| `Zenith_Integration_StreamingRender` | `zenith_add_integration_tests(StreamingRender DEPENDS ZenithRuntime ZenithStreaming ZenithFlux ZenithSave ZenithCompression_Zlib ZenithCompression_Zstd ZenithECS_Custom SUPPORTED ZENITH_WINDOWS)` in `Zenith2/Tests/StreamingRender/`, the DEVT `DevToolsRender` shape | the GPU tests of Tests.md "Render tests", on WARP and lavapipe (`Zenith2/Docs/Design/RHI.md` → "Headless CI devices") |
| `Zenith_Fuzz_StreamingWorld` | created by the test registry in `win-asan` from `Tests/Fuzz/StreamingWorld.cpp` (`Zenith2/Docs/Architecture/BuildSystem.md` → "Test, conformance and fuzz targets") | the reader fuzz target, 60-second smoke |

### Supported matrix

| Target | `ZENITH_SUPPORTED` |
|---|---|
| `ZenithStreaming` | `ZENITH_GPU` (`win`, `win-asan`, `android-arm64`, `android-x86_64`) |
| `ZenithStreaming_Tests` | `ZENITH_HOST` narrowed by its dependencies to `win`, `win-asan` |
| `Zenith_Integration_Streaming` | `win`, `win-asan` (narrowed by `ZenithRuntime`) |
| `Zenith_Integration_StreamingRender` | `win` (`ZENITH_WINDOWS` narrowed by `ZenithRuntime` to the presets with a test preset and a GPU: WARP and lavapipe) |
| `Zenith_Fuzz_StreamingWorld` | `win-asan` |
| ZenithCook's streaming files | ZenithCook's matrix (`ZENITH_TOOLS_HOST`) |

## Public API

### API map

The full declarations, with contract blocks, are `Zenith2/Docs/Design/STREAM/PublicAPI.md`. The top-level contracts:

| Type or function | Contract in one line |
|---|---|
| `Zenith_Module_Streaming_RegisterTypes` | registers STREAM's seven components, the override slot struct, its resource type, its payload structs and its configuration keys, and nothing else ([Z2-N-ARCH-848]) |
| `Zenith_Module_Streaming_Start` | registers the kind loader, the four command kinds, the bindings and the memory consumer; returns the module instance |
| `Zenith_StreamingModuleInstance` (Internal) | per world: adds `Zenith.Streaming.State`, the systems, the listener and the activation hook; detach releases the world's presentation state |
| `Zenith_StreamingWorldAsset` | the immutable partition of one streamed world: grids, cells, dependencies, HLOD nodes, references, persistent entries |
| `Zenith_WriteStreamingWorldFile` | byte-deterministic writer of the `.zstream` sections |
| `Zenith_StreamingStateView` | a read-only, out-of-frame view of one world's streaming state for tools, the dump and the editor |
| `Zenith_RecordStreamingCellOverride` | records a forced cell state (development builds and tests) |
| `Zenith_StreamingError` | the 16-byte error record; codes `STREAMING_ERROR_*` numbered from 1 |

### Rules across the STREAM API

- Every fallible public function returns `Zenith_Result<T, Zenith_StreamingError>` ([Z2-N-CONTRACT-461]); untrusted bytes (a `.zstream`, a document, a delta record read back from a save) never reach an assert ([Z2-N-CONTRACT-472]).
- No STREAM type holds a `Zenith_Mutex`; STREAM adds no lock-order row. Cross-thread state is the atomics of the snapshot slots (holder mask, gate flag) and of the memory consumer.
- Every simulation-visible STREAM decision runs on `Streaming.Update` (phase 1) or a STREAM playback, and reads only simulation state, the cooked world and simulation-affecting configuration.
- Every STREAM component is C++-registered, as LIGHT's and RPROD's engine components are: stable names `Zenith.Streaming.*`, property IDs permanent.

### Handle kinds used

| Handle | Where |
|---|---|
| `Zenith_PersistentID` | cell scene GUIDs, the world asset GUID, entity identities in references, persistent entries and delta records ([Z2-N-CONTRACT-066]) |
| `Zenith_EntityHandle` | sources, region-bound entities, anchors, delta holders, persistent entities while active; never serialized ([Z2-N-CONTRACT-064]) |
| `Zenith_AssetHandle<Zenith_StreamingWorldAsset>`, `Zenith_AssetHandle<Zenith_Scene>`, `Zenith_AssetHandle<Flux_MeshResource>` | the world reference, cell scenes, HLOD meshes; counted references ([Z2-N-CONTRACT-082]) |
| `Zenith_OperationHandle` | booster, prefetch, HLOD and PSO-group requests, owned by the service ([Z2-N-CONTRACT-091]) |
| `Zenith_SceneInstanceID` | the SCENE instance of a cell, saved in its anchor |

### Component types

| Stable name | Struct | Properties (ID, name, type, authority) | Section |
|---|---|---|---|
| `Zenith.Streaming.WorldRef` | `Zenith_StreamingWorldRefComponent` | 1 World asset `Streaming.World` (COMMANDS_ONLY); 2 Enabled bool (LATE_SCRIPTS); 3 Overrides fixed array of 64 `Zenith.Streaming.CellOverrideSlot` (COMMANDS_ONLY); 4 BindingState u8 (INPUT) | PublicAPI.md "Zenith_StreamingComponents header" |
| `Zenith.Streaming.Source` | `Zenith_StreamingSourceComponent` | 1 Enabled bool; 2 RadiusScale f32; 3 GridMask u32; 4 Primary bool; 5 DrivesHLOD bool (default false); 6 Priority u8 (all LATE_SCRIPTS); 7 Ready bool; 8 MissingRequiredCells u32; 9 TeleportSequence u32; 10–12 PreviousX/Y/Z f64; 13 PreviousTick u64; 14 TeleportPending bool (INPUT) | same |
| `Zenith.Streaming.RegionBound` | `Zenith_StreamingRegionBoundComponent` | 1 Policy u8 (COMMANDS_ONLY); 2 InActiveRegion bool; 3 CellIndex u32; 4 EvaluatedTick u64 (INPUT, all serialized) | same |
| `Zenith.Streaming.Persistent` | `Zenith_StreamingPersistentComponent` | 1 Mode u8; 2 CellScene persistent_id; 3 EntityID persistent_id (COMMANDS_ONLY) | same |
| `Zenith.Streaming.CellAnchor` | `Zenith_StreamingCellAnchorComponent` | 1 CellScene persistent_id; 2 Instance u32; 3 WorldID persistent_id (COMMANDS_ONLY) | same |
| `Zenith.Streaming.CellDelta` | `Zenith_StreamingCellDeltaComponent` | 1 CellScene persistent_id; 2 WorldID persistent_id; 3 FormatVersion u32; 4 Bytes container<u8> (COMMANDS_ONLY) | same |
| `Zenith.Streaming.CellStatus` | `Zenith_StreamingCellStatusComponent` | 1 State u8; 2 Instance u32; 3 RetriesUsed u8; 4 CapacityFailures u8; 5 TransientFailures u8; 6 Flags u16; 7 RetryTick u64; 8 UnloadDueTick u64; 9 TombstoneBits and 10 PendingTombstoneBits fixed arrays of 16 u64; 11 DeltaMergePending bool (INPUT; on the cell's holder beside `CellDelta`) | same; Runtime.md "Persisted cell status" |

World resources: `Zenith.Streaming.State` (`Zenith_StreamingWorldState`, owning phase `PHASE_INPUT`), whose one system writer is `Streaming.Update` (STREAM's playbacks, listener and hook also write it on the sync-point node and the tick-boundary handler, "Thread affinity per type"), read by `Streaming.Service`, `Streaming.ExtractHLOD` and the `READ_WORLD` bindings.

### Configuration keys

Registered by `Zenith_Module_Streaming_RegisterTypes` through the type registry's configuration schema; the full table with types, ranges and layers is `Zenith2/Docs/Design/STREAM/PublicAPI.md` → "Internal declarations" (table "Configuration keys"). Keys marked simulation-affecting are written into every replay header (`Zenith2/Docs/Design/CORE/Diagnostics.md` → "Simulation-affecting list").

### Script bindings

`Streaming.IsPositionActive(position: vector): boolean`, `Streaming.GetCellState(position: vector, grid: string?): string` and `Streaming.GetWorldOrigin(): (number, number, number)`, all `SCRIPT_FUNCTION_EFFECT_READ_WORLD` on `Zenith.Streaming.State` and `Zenith.WorldOrigin` (read directly, so a rebase tick's phases 2 to 10 see the shifted origin) and callable from systems and behaviours; plus `Streaming.SetCellOverride` in `SCRIPT_FUNCTION_CONTEXT_TOOLS` only. Declarations: `Zenith2/Docs/Design/STREAM/PublicAPI.md` → "Script bindings".

## Data structures and formats

### Format index

| Format | Kind | Section |
|---|---|---|
| `.zstream` (kind `Streaming.World`, magic `ZSTW`) | cooked, ASSET container, versioned | `Zenith2/Docs/Design/STREAM/Formats.md` → "Streaming world cooked file" |
| `*.streamworld.json` (document type `Zenith.Streaming.WorldDocument`, schema version 1) | authoring document | `Zenith2/Docs/Design/STREAM/Formats.md` → "Streaming world document" |
| cell scene rules (anchor root, bounds, persistent and reference markers) | constraints on SCENE documents | `Zenith2/Docs/Design/STREAM/Formats.md` → "Cell scene rules" |
| cell delta bytes (`Zenith.Streaming.CellDelta` property 4) | runtime and saved, versioned | `Zenith2/Docs/Design/STREAM/Formats.md` → "Cell delta encoding" |
| module command payloads | sync-point records | `Zenith2/Docs/Design/STREAM/Formats.md` → "Module command payloads" |
| `zenith2.stream-dump` version 1 | diagnostic JSON | `Zenith2/Docs/Design/STREAM/Formats.md` → "Stream dump format" |

### In-memory layouts that matter

| Record | Bytes | Count bound | Section |
|---|---|---|---|
| `Zenith_StreamingCellRuntime` | 56 | one per cell, at most 65,536 per world (3.5 MiB) | Formats.md "In-memory runtime records" |
| `Zenith_StreamingCellStatusComponent` | 288 | one per holder: a cell whose status is not the default or whose delta holds a record (Runtime.md "Persisted cell status", holder pruning) | PublicAPI.md "Zenith_StreamingComponents header" |
| `Zenith_StreamingCellDeltaMirror` | 32 | one per cell (2 MiB at 65,536 cells) | Formats.md "In-memory runtime records" |
| `Zenith_StreamingSourceRuntime` | 96 | 16 per world | same |
| `Zenith_StreamingAdmissionEntry` | 24 | `stream.activation.maxCellsPerTick` (32 at most) | same |
| `Zenith_StreamingSnapshotSlot` | 64 plus its 8,320-byte entity table | `uSTREAMING_SNAPSHOT_SLOTS` (112) per attached world: 0.9 MiB | same |
| `Zenith_StreamingHLODRuntime` | 32 | one per HLOD node, at most 65,536 per world | same |

### Cell key and coordinates

A cell is named by its **cell index**: its position in the world's global cell table (0 … cell count − 1, at most 65,535), sorted by (grid, z, x). Its **cell coordinate** is `Zenith_StreamingCellCoord { uint8 m_uGrid; int32 m_iX; int32 m_iZ; }`, |x|, |z| ≤ 2^20. The cell's absolute XZ bounds are `[gridOrigin + coord × cellSize, gridOrigin + (coord + 1) × cellSize)` in `double` metres, and its **anchor** is the minimum corner at the grid's base height. Absolute positions are world-frame metres in `double`; a component position is a `float` relative to the current world origin `O` of `Zenith.WorldOrigin`, and the absolute position of a local position `p` is `O + (double)p` (`Zenith2/Docs/Design/STREAM/Runtime.md` → "Large coordinates and origin rebasing").

## Algorithms and state machines

### Algorithm index

| Algorithm | Complexity | Section |
|---|---|---|
| the per-tick update (15 steps) | O(sources × cells in range + cells in transition + region-bound entities) | `Zenith2/Docs/Design/STREAM/Runtime.md` → "Per-tick update" |
| distances and bands | O(1) per (source, cell) | "Distance and band computation" |
| the desired set and the dependency closure | O(Σ cells in each source's prefetch disc + dependency edges) | "Desired-set algorithm" |
| load ordering, admission and pacing | O(n log n) for n candidates | "Load ordering and admission" |
| action buffers and their observation | O(actions) | "Action buffers and rejection" |
| cross-cell binding and unbinding | O(references of the cell) | "Cross-cell references" |
| persistence capture, tombstones and restore | O(persistent entities of the cell × configured types) | "Persistent entities" |
| region flags | O(region-bound entities) | "Region states and coordinated subsystems" |
| the rebase rule | O(1) | "Large coordinates and origin rebasing" |
| the service (boosters, prefetch, PSO groups, HLOD residency) | O(cells in transition + prefetch disc + HLOD nodes in range), capped by `stream.service.frameBudgetUs` | "The streaming service" |
| the HLOD cut and extraction | O(HLOD nodes in range) | "HLOD selection and extraction" |
| reset and rebuild after a restore | O(cells + delta holders + anchors) | "Save and restore" |

### Per-tick flow

```text
tick boundary (T-1 → T): SCENE publishes cell preparations (QUEUED / FAILED), logged
phase 1  Streaming.Update            observe → sources → bands → closure → unloads/cancels → loads → admission → regions → readiness → rebase
phase 1  Scene.ActivateAdditive      asks the hook: AdmitActivation, GetRootTranslation (STREAM's table, built just before)
sync 1   step 6: spawns of admitted cells, destroys of unloaded cells
         step 7: Scene.LoadAdditive commits (hook OnLoadCommitted), Scene.ForgetInstance, Streaming.ApplyCellState,
                 Streaming.BindReferences, Streaming.UnbindReferences, then Scene.ShiftWorldOrigin last
phases 2–10  gameplay sees active cells, flags and readiness; physics freezes region-inactive bodies
frame epilogue  Streaming.Service    boosters, prefetch, PSO groups, HLOD residency, shedding, counters
                Streaming.ExtractHLOD  HLOD instance deltas
frame prologue (next)  SCENE StepActivations starts the committed preparations; the preparation polls the hook gate before BUILDING
```

### State machine index

| Machine | States | Section |
|---|---|---|
| cell (simulation side) | UNLOADED, REQUESTED, LOADING, QUEUED, ACTIVATING, ACTIVE, UNLOADING, CANCELLING, FAILED, FAILED_PERMANENT; 34 rows, total over every observed instance state | `Zenith2/Docs/Design/STREAM/Runtime.md` → "Cell state machine" |
| prefetch and booster request (presentation side) | NONE, REQUESTED, RESIDENT, RELEASING | "The streaming service" |
| HLOD node (presentation side) | ABSENT, REQUESTED, READY, DISPLAYED | "HLOD selection and extraction" |
| snapshot slot | FREE (mask 0), HELD BY BOTH (mask 3), SIMULATION ONLY (mask 1: the preparation ended, the cell not yet observed), PREPARATION ONLY (mask 2: only after `DetachWorldState`, in a retiring pool, or after the rebuild neutralised an unadopted restored load, Runtime.md "Rebuild" R4) | "PSO groups and the preparation gate" |
| world binding | NONE, BOUND, REFUSED, FAILED, UNBINDING | "World binding" |

### Frame-graph nodes and access

| Node | Kind and phase | Affinity | Reads | Writes |
|---|---|---|---|---|
| `Streaming.Update` | system, `PHASE_INPUT`, `Before("Scene.ActivateAdditive")`, `EmitsCommands` | `JOB_AFFINITY_ANY` | `Zenith.Streaming.WorldRef` 1–3, `Zenith.Streaming.Source` 1–6, `Zenith.WorldTransform` 1, `Zenith.Streaming.RegionBound` 1, `Zenith.Streaming.Persistent`, `Zenith.Streaming.CellAnchor`, `Zenith.Streaming.CellDelta`, every type of `stream.persistence.componentTypes`; resources `Zenith.SceneActivationQueue`, `Zenith.WorldOrigin` | resource `Zenith.Streaming.State`; `Zenith.Streaming.WorldRef` 4; `Zenith.Streaming.Source` 7–14; `Zenith.Streaming.RegionBound` 2–4; `Zenith.Streaming.CellStatus` (holders); commands |
| `Streaming.Service` | epilogue stage, stage 12 | `JOB_AFFINITY_ANY` | `Zenith.Streaming.State`, `Zenith.SceneActivationQueue`, `Zenith.WorldOrigin`, `Zenith.Streaming.Source`, `Zenith.WorldTransform` 1 | STREAM's per-world presentation state (private to this node and the extractor) |
| `Streaming.ExtractHLOD` | extractor, stage 12, `After("Streaming.Service")` | `JOB_AFFINITY_ANY` | as `Streaming.Service` | its `Flux_RenderWorldWriter` partition |

The full declarations and the ordering argument are `Zenith2/Docs/Design/STREAM/Runtime.md` → "Per-tick update" and "The streaming service".

## Threading lifetime and error contract

### Thread affinity per type

| Type | Affinity |
|---|---|
| `Zenith_StreamingModuleInstance` | `MAIN_THREAD` (Architecture's module contract) |
| `Zenith_StreamingWorldState` | `EXTERNALLY_SYNCHRONISED`: written by `Streaming.Update` and by STREAM's playbacks, listener and hook on the sync-point node and the tick-boundary handler; read by the hook on the `Scene.ActivateAdditive` node (ordered after `Streaming.Update`), by the service and extractor in the epilogue, and by `READ_WORLD` bindings in phases 2 to 10 |
| `Zenith_StreamingWorldAsset` | `IMMUTABLE` once activated |
| `Zenith_StreamingSnapshotPool` | `ANY_THREAD_EXCEPT(Acquire, ReleaseSimulation, SetCell, FillEntityTable, DetachWorldState)`: the first four on the world's `Streaming.Update` and sync-point nodes (or the main thread during a restore), the fifth, `DetachWorldState`, on the main thread at detach; each slot's holder mask and gate flag are atomics read and written from preparation threads and the service |
| `Zenith_StreamingLifecycleListener` | the world's sync-point node, inside the playback that dispatches its event; writes only through the playback access |
| `Zenith_StreamingActivationHook` | one per world, owned by the world's snapshot pool; as the hook interface (row ST2): each member on its stated thread |
| `Zenith_StreamingMemoryConsumer` | `ANY_THREAD` for the byte getters (relaxed atomic sums); `OnPressureLevelChanged` on the main thread |
| `Zenith_StreamingWorldKindLoader` | `ANY_THREAD` (ASSET's loader contract) |
| `Zenith_StreamingStateView` | `MAIN_THREAD` with no frame of the world running |

### Frame-graph placement

`Streaming.Update` is the only STREAM node of the TICK subgraph; `Streaming.Service` and `Streaming.ExtractHLOD` are EPILOGUE nodes of the world's group. The service runs once per frame, so presentation work never multiplies with catch-up ticks. STREAM registers no main-thread node and no prologue work.

### Lock-order entries

None: STREAM declares no `uLOCK_LEVEL_` constant and holds no lock. It calls ZenithAssets (`uLOCK_LEVEL_ASSET_REGISTRY` 200 for `Acquire` from `Streaming.Update` and the service; `uLOCK_LEVEL_STREAMING_REQUESTS` 250 for the service's requests) and the PSO precacher (from the service) with no lock of its own held, so it adds no ordering edge.

### Construction and shutdown

| Moment | STREAM's action |
|---|---|
| startup step 6 | `Zenith_Module_Streaming_RegisterTypes`: types and configuration keys |
| startup step 15 | `Zenith_Module_Streaming_Start`, in this order (the order of its fault points 0x53570001 to 0x53570006): reads the settings; creates the module instance; reserves the retiring-pool list (16 entries, never grown); registers the memory consumer; registers the kind loader for `Streaming.World`; the four command kinds; the four bindings |
| startup step 16 (and every world creation) | `AttachWorld`, in this order (fault points 0x53570010 to 0x53570015): obtains the SCENE state; creates the world's state and its presentation state; adds the resource, the three nodes and the listener; creates the world's snapshot pool (112 slots); sets the hook |
| shutdown step 4 | nothing of its own: ASSET's `BeginShutdown` cancels STREAM's requests, SCENE's preparations end and call `OnPreparationEnded` |
| shutdown step 6 | `DetachWorld` per world (presentation state released, handles destroyed; it allocates nothing and cannot fail); then the module instance: `UnregisterKindLoader`, then every retiring pool is destroyed after `ZENITH_CHECK` that every slot is FREE |

The fault points and their unwinding are `Zenith2/Docs/Design/STREAM/PublicAPI.md` → "Zenith_Module_Streaming header".

### Cancellation summary

| Stage | Who cancels | What STREAM releases, and when it reports |
|---|---|---|
| I/O and decompression of a prefetch or booster | the service destroys its operation handle | nothing else; ASSET releases its records before the operation completes ([Z2-N-CONTRACT-335]) |
| GPU upload of a cell asset | the asset's last reference drops (ASSET calls the kind loader's `Cancel` or `Unload`) | the handles STREAM held |
| SCENE preparation (cell LOADING) | `Streaming.Update` records the instance's unload; SCENE cancels the preparation | the cell is CANCELLING until SCENE's discarded publication reaches the hook (`OnDiscardedPublication`); the slot is free once `OnPreparationEnded` has also run |
| published, not active (QUEUED) | `Streaming.Update` records the unload and refuses admission | the queue entry (SCENE); the slot |
| activation | cannot be interrupted: one sync point; a cell that stops being wanted while ACTIVATING is unloaded once ACTIVE ([Z2-N-CONTRACT-337]) | everything at the subsequent unload |
| the cell reports UNLOADED | only when the instance is UNLOADED, its publication (if it was LOADING) has been discarded, and every simulation-side handle of the cell destroyed | `stream.cells.released` + 1 |

The full specification is `Zenith2/Docs/Design/STREAM/Runtime.md` → "Cancellation at every stage".

### Error enumerations

`Zenith_StreamingErrorCode` (`Zenith2/Docs/Design/STREAM/PublicAPI.md` → "Zenith_StreamingErrors header"), numbered from 1:

| Code | Meaning |
|---|---|
| `STREAMING_ERROR_INVALID_DESC` | a descriptor or argument outside its documented range |
| `STREAMING_ERROR_OUT_OF_MEMORY` | an allocation failed (factory, world state, pool) |
| `STREAMING_ERROR_WORLD_CORRUPT` | a `.zstream` failed a reader check |
| `STREAMING_ERROR_WORLD_VERSION` | the format version is not `uSTREAMING_WORLD_FORMAT_VERSION` |
| `STREAMING_ERROR_WORLD_LIMIT` | a count above a limit of "Limits" |
| `STREAMING_ERROR_WORLD_NOT_RESIDENT` | the world asset was not active when its reference was bound |
| `STREAMING_ERROR_WORLD_DUPLICATE` | a second enabled `Zenith.Streaming.WorldRef` in one world |
| `STREAMING_ERROR_WORLDREF_RUNTIME` | a `Zenith.Streaming.WorldRef` on an entity without a persistent ID (not scene content) |
| `STREAMING_ERROR_CELL_LOAD_FAILED` | SCENE published a failure for a cell's instance |
| `STREAMING_ERROR_CELL_COMMIT_REFUSED` | SCENE refused a cell's load at commit (`SCENE_ERROR_LIMIT`) |
| `STREAMING_ERROR_CELL_DUPLICATE_INSTANCE` | another instance of a cell's scene is live (`SCENE_ERROR_DUPLICATE_INSTANCE`) |
| `STREAMING_ERROR_SNAPSHOT_POOL_FULL` | no free snapshot slot (a defect of the cap arithmetic; logged and counted) |
| `STREAMING_ERROR_OVERRIDE_NOT_ALLOWED` | a cell override in a Shipping build or with `stream.debug.allowOverrides` false |
| `STREAMING_ERROR_DELTA_CAPACITY` | a cell's delta would exceed `uSTREAMING_MAX_DELTA_BYTES_PER_CELL` |
| `STREAMING_ERROR_DELTA_CORRUPT` | a delta record read back failed its decode |
| `STREAMING_ERROR_CONFIG_INVALID` | a configured persistent type is unknown or not a component |
| `STREAMING_ERROR_SCENE_STATE_UNAVAILABLE` | the attach context gave no SCENE world state |
| `STREAMING_ERROR_REGISTRATION` | a kind loader, command kind or binding registration was refused |

Cook errors are ASSET's `Zenith_CookError` codes `COOK_ERROR_STREAM_*` (§ "Authoring and cooking"; row ST6).

### Luau errors versus asserts

| Situation | Behaviour |
|---|---|
| a binding called with a non-vector or an unknown grid name | a controlled Luau error (`SCRIPT_THUNK_ERROR_MISUSE`, ZS1511-style text), never an assert ([Z2-N-CONTRACT-473]) |
| `Streaming.SetCellOverride` outside a tools invocation | the dispatcher's context misuse error |
| a script writing `Zenith.Streaming.Source` 7–9 or a COMMANDS_ONLY property directly | the authority misuse error of SCRIPT (schedule-compile error for a declared write) |
| corrupt cooked or saved bytes | `Zenith_Result` errors and counters |
| STREAM's own invariants (a slot released twice, a cell index out of range from engine code) | `ZENITH_ASSERT`; corruption-prone ones (pool accounting at destruction) `ZENITH_CHECK` |

### Determinism obligations

1. `Streaming.Update` reads only: components, `Zenith.WorldOrigin`, SCENE's additive set, the cooked world asset, the tick index and simulation-affecting configuration. It reads no clock, no asset state, no memory level, no I/O status and no render state ([Z2-N-ARCH-885], [Z2-N-ARCH-812]).
2. Every ordering STREAM makes (load candidates, unloads, admissions, region updates, references, captures) is by keys built from cell indices, quantised distances and simulation IDs, never by addresses or completion order ([Z2-N-ARCH-884]).
3. I/O and preparation timing reach simulation only as SCENE's publications (source kind 8, `ASYNC_SOURCE_SCENE_LOAD`), which are logged; a replay holds the tick until a recomputed preparation is ready ([Z2-N-SCRIPT-337]), and the discarded publication of a cancelled load is RECORDED, its kind and payload decided at publication by SCENE's resolver rather than by the thread that parked it, so the log never depends on thread timing and a replay never waits for a preparation it cancels itself (rows ST2 (b), ST21).
4. Presentation (prefetch, boosters, PSO groups, HLOD) may read the memory level, asset residency and the PSO state, and writes no simulation state.
5. Failures caused by capacity or memory pressure never consume a retry, at SCENE's level or at ASSET's (SCENE carries the ASSET cause of `ASSET_UNAVAILABLE`, row ST2 (i)), and a transient I/O failure consumes one only after a bounded number of free retries (`Zenith2/Docs/Design/STREAM/Runtime.md` → "Failure classification"), so pressure delays content but never removes it.
6. Every field of a cell's state that a subsequent tick reads, and every STREAM-written source, region and world-reference property, is in a saved component, written directly in the step that changes it (`Zenith2/Docs/Design/STREAM/Runtime.md` → "Persisted cell status"), and every per-tick rule that acts on saved state (the re-homes and holder prunes of step 14) reads only that state, so a replay resumed from an EXACT capture (`Zenith_RuntimeReplay_ResumeFromSave`) reproduces the recording's per-tick hashes. A live continuation equals an uninterrupted live run only while no asynchronous request is in flight at the capture or issued in the compared window ([Z2-N-SCRIPT-338]): cell loads publish at ticks their preparations' timing decides, so a streamed run is compared against its recording.

## Diagnostics and profiling

### Counters

Published through the profiler hooks' counter hook with the kind each counter states ([Z2-N-CONTRACT-155], `Zenith2/Docs/Contracts.md` → "No mutable global state"); per world the name is suffixed `.<worldTag>`.

| Counter | Kind | Meaning |
|---|---|---|
| `stream.cells.state.<state>` (ten names, one per state) | GAUGE | cells in each state |
| `stream.cells.loads_issued` | CUMULATIVE | load actions recorded |
| `stream.cells.loads_deferred` | CUMULATIVE | wanted cells not issued because a cap was reached (BR-DEFER of Z2-B-STREAM-003) |
| `stream.cells.in_flight` | GAUGE | REQUESTED + LOADING cells (MP-COUNTER of Z2-B-STREAM-003) |
| `stream.cells.activated` | CUMULATIVE | cells that reached ACTIVE |
| `stream.cells.activation_deferred` | CUMULATIVE | admission refusals for pacing |
| `stream.cells.activation_rejected` | CUMULATIVE | admitted cells SCENE did not activate (capacity) |
| `stream.cells.unloaded` | CUMULATIVE | unload actions recorded |
| `stream.cells.cancelled` | CUMULATIVE | cancellations of LOADING or QUEUED cells |
| `stream.cells.released` | CUMULATIVE | cells whose release completed (Runtime.md "Completion reporting") |
| `stream.cells.failed` | CUMULATIVE | published failures |
| `stream.cells.commit_refused` | CUMULATIVE | SCENE commit refusals |
| `stream.cells.late_state_apply` | CUMULATIVE | activations whose state records were rejected and re-applied |
| `stream.cells.unload_deferred` | CUMULATIVE | unload candidates left waiting by `stream.unload.maxCellsPerTick` |
| `stream.cells.blocked_by_dependency` | CUMULATIVE | load candidates skipped because a dependency is FAILED_PERMANENT |
| `stream.cells.preempted` | CUMULATIVE | kept cells preempted by a REQUIRED candidate at the live cap (Runtime.md "Loads (step 9)") |
| `stream.cells.capacity_failures` | CUMULATIVE | failures of a capacity or memory-pressure class (no retry consumed, Runtime.md "Failure classification") |
| `stream.cells.transient_failures` | CUMULATIVE | failures of the transient class (I/O, an undelivered pack, an unknown dependency cause) |
| `stream.cells.unknown_failure_detail` | CUMULATIVE | `ASSET_UNAVAILABLE` failures whose ASSET detail was 0 or outside the enumerators |
| `stream.cells.external_unload` | CUMULATIVE | cell instances unloaded by code other than STREAM (T24 to T27) |
| `stream.cells.anchor_destroyed` | CUMULATIVE | ACTIVE cells whose anchor root gameplay destroyed (T28) |
| `stream.cells.unexpected_state` | CUMULATIVE | observations no other row allows (T34), and discarded publications of cells not CANCELLING |
| `stream.cells.state_apply_skipped` | CUMULATIVE | ApplyCellState or BindReferences playbacks that met a QUEUED instance (SCENE's activation rejected) |
| `stream.actions.rejected` | CUMULATIVE | action buffers the merge rejected |
| `stream.commands.malformed` | CUMULATIVE | STREAM command payloads ignored for their size, a field out of range or an instance mismatch |
| `stream.hook.unknown_tags` | CUMULATIVE | hook calls whose tag has no slot or whose pool has no world state, and discarded publications of a slot with no cell (an unadopted restored load) |
| `stream.snapshots.pool_full` | CUMULATIVE | `Acquire` refusals (unreachable for a save written within the configuration ranges) |
| `stream.restore.unadopted` | CUMULATIVE | restored in-flight cell loads the rebuild released instead of adopting, and restore re-requests SCENE refused (row ST2 (j)) |
| `stream.restore.unclaimed_instances` | CUMULATIVE | restored ACTIVE instances of a cell scene that no holder or anchor of the bound world claims (the game's own), left alone |
| `stream.overrides.unknown_cell`, `.refused` | CUMULATIVE | cell overrides naming no cell; overrides refused (Shipping or `stream.debug.allowOverrides` false) |
| `stream.world.bound_exceeds_cap` | GAUGE | 1 when the bound world's worst-case live instances exceed `stream.maxLiveInstances` |
| `stream.rebase.precision_warning` | GAUGE | 1 when rebasing is off and a cell lies beyond the 4,096 m precision radius |
| `stream.precision.cells_beyond_radius` | GAUGE | ACTIVE cells with a corner beyond 4,096 m of O on an axis (Runtime.md "Precision target") |
| `stream.references.bound`, `.unbound`, `.unresolved` | CUMULATIVE | reference writes |
| `stream.persistence.captured_entities`, `.captured_bytes`, `.tombstones`, `.restored_entities`, `.delta_overflow` | CUMULATIVE | the delta store |
| `stream.persistence.tombstones_dropped`, `.records_dropped`, `.unknown_types`, `.unconfigured_types`, `.unknown_modes`, `.ineligible_properties` | CUMULATIVE | pending tombstone bits discarded at an external unload (T26, T27); delta records dropped by D9 (their entity belongs to no cell); record types unknown to the registry; record types not configured in this build; persistence modes outside the enumerators (once per entity); ineligible properties skipped (once per type) |
| `stream.persistence.records_rehomed`, `.rehome_deferred`, `.holders_respawned`, `.holders_pruned` | CUMULATIVE | records moved to their entity's new cell by step 14; records left in place at a visit because the new cell was live or in flight; holders re-spawned after gameplay destroyed them; holders destroyed because their cell returned to the default status with an empty delta |
| `stream.persistence.orphan_deltas` | GAUGE | delta holders naming a cell scene the bound world does not have |
| `stream.regions.inactive` | GAUGE | region-bound entities whose region is inactive |
| `stream.regions.destroyed` | CUMULATIVE | entities destroyed by `STREAMING_REGION_POLICY_DESTROY` |
| `stream.rebase.count` | CUMULATIVE | origin shifts recorded |
| `stream.sources.not_ready` | GAUGE | enabled sources with missing required cells |
| `stream.residency_miss_ticks` | CUMULATIVE | ticks with a primary source not ready (the cells part of Z2-B-STREAM-005) |
| `stream.teleport.count`, `.pending` | CUMULATIVE, GAUGE | teleports observed; teleports not yet ready |
| `stream.teleport.last_to_ready_us` | GAUGE | wall-clock microseconds from the last teleport's frame to the frame its source became ready (presentation measurement, Runtime.md "Teleports and readiness") |
| `stream.prefetch.requests`, `.cancelled`, `.resident` | CUMULATIVE, CUMULATIVE, GAUGE | prefetch requests |
| `stream.boosters.active` | GAUGE | booster requests held |
| `stream.pso.groups_pending` | GAUGE | PSO group requests not complete |
| `stream.hlod.displayed`, `.resident`, `.gaps` | GAUGE, GAUGE, GAUGE | HLOD nodes displayed, meshes resident, regions with neither content nor proxy |
| `stream.shed.level`, `stream.shed.prefetch_cancelled`, `stream.shed.hlod_evicted`, `stream.shed.cells_evicted` | GAUGE, CUMULATIVE, CUMULATIVE, CUMULATIVE | shedding (Architecture's row "Streaming"); the last counts cells whose presentation residency CRITICAL released |
| `stream.peak.deferred` | CUMULATIVE | prefetches deferred by a refused double-residency reservation |
| `stream.peak.proxy_released` | CUMULATIVE | covering proxies released because a required load's reservation was refused |
| `stream.memory.bytes` | GAUGE | STREAM's resident bytes (MP-SUBMEM share of Z2-B-STREAM-001) |
| `stream.cells.activation_entities` | GAUGE | the largest entity sum admitted in one tick of the frame (MP-COUNTER of Z2-B-STREAM-006); read by the service from the world state's per-tick ring |
| `stream.update_us_delta`, `stream.service_us_delta` | DELTA | the two nodes' spans per tick and per frame (MP-CPUZONE of Z2-B-STREAM-002) |

### On-screen profiler rows

The streaming panel (DEVT's overlay, row ST15) shows per world: cells per state; in-flight cells against `stream.maxConcurrentCellLoads` and Z2-B-STREAM-003; STREAM memory against Z2-B-STREAM-001; `Streaming.Update` and `Streaming.Service` time against Z2-B-STREAM-002; sources with their readiness; HLOD displayed and gaps; the shedding level; the last teleport-to-ready time against Z2-B-STREAM-004. ASSET's decode-queue row sits beside it (`Zenith2/Docs/Design/ASSET/Loading.md` → "Priority reserves").

### Log categories

| Category | Logged |
|---|---|
| `stream.world` | binding, refusal, failure of a world |
| `stream.cell` | failures, permanent failures, commit refusals, late state applies (one line per event, at most 1 per cell per 600 ticks after the first) |
| `stream.persist` | capture overflow, delta decode failures, configuration errors |
| `stream.ref` | unresolved references (first per (cell, reference) per session) |
| `stream.rebase` | every shift: tick, vector, new origin |
| `stream.hlod` | HLOD mesh failures |
| `stream.cook` | cook validation messages (ZenithCook) |

### Profiler zones

`Streaming.Update` (the node), with sub-zones `Streaming.Update.Observe`, `.Bands`, `.Unload`, `.Load`, `.Admit`, `.Regions`, `.Persist`; `Streaming.Service` with `.Boosters`, `.Prefetch`, `.HLOD`; `Streaming.ExtractHLOD`; `Streaming.Playback.<Kind>` for each command kind.

### Debug views and dumps

- `stream.debug.view` (configuration key, development builds): the service writes debug-draw records through its writer partition (`Flux_RenderWorldWriter::AddDebugDraw`): each cell of the selected grid within 2 km as an XZ rectangle coloured by state (UNLOADED grey, REQUESTED and LOADING yellow, QUEUED and ACTIVATING orange, ACTIVE green, UNLOADING and CANCELLING purple, FAILED red), HLOD nodes displayed as cyan outlines, sources as circles of their radii.
- `stream.dump` (a development-console command DEVT registers, row ST15): writes `zenith2.stream-dump` version 1 (`Zenith2/Docs/Design/STREAM/Formats.md` → "Stream dump format") through `Zenith_StreamingStateView::WriteDump`.

### Telemetry

Per frame, the frame telemetry record carries the GAUGE counters above and the per-frame deltas of the CUMULATIVE ones; MP-STREAM's runner reads `stream.teleport.*` and `stream.residency_miss_ticks` with GEOM's texture-residency counters (row ST15).

## Authoring and cooking

### What STREAM authors

STREAM has no visual editor: world partitioning is WORLD's (Z2-I-WORLD-01). STREAM owns the runtime contract that authoring output must meet — the streaming world document, the cell scene rules, the cooked index — and the tools that validate and cook it.

### The streaming world document

`Games/<Name>/Scenes/<World>/<World>.streamworld.json`, document type `Zenith.Streaming.WorldDocument`, schema version 1, registered with SCENE's migration registry by `Zenith_RegisterStreamingDocumentTypes(Zenith_MigrationRegistry&)` from ZenithCook and ZenithEditor (tool-side registration, as ENV's). Its schema, validation messages and an example are `Zenith2/Docs/Design/STREAM/Formats.md` → "Streaming world document". WORLD's partition cook writes it (row ST13); it can also be written by hand for tests and small games.

### Cook step

| Field | Value |
|---|---|
| ID | `stream.world` |
| version | 1 |
| phase | `COOK_PHASE_ASSET`, kind `ASSET_KIND_STREAMING_WORLD` |
| inputs | the document (`OpenSourceDocument`), every cell scene's cooked output (`ReadDependencyOutput`), every HLOD mesh's catalogue entry |
| key inputs | the document's digest; each cell scene's output hash; each HLOD mesh's output hash; `uSTREAMING_WORLD_FORMAT_VERSION` |
| output | one `.zstream` through `Zenith_CookedFileWriter`, sections of Formats.md "Streaming world cooked file" |
| dependencies declared | cell scenes and HLOD meshes as soft dependencies (packaged, never loaded with the index) |
| report member | `streaming`: per grid its cell count, worst-case active cells, entity totals; references and persistent entries |
| errors | `COOK_ERROR_STREAM_DOCUMENT`, `_GRID`, `_CELL_RANGE`, `_CELL_DUPLICATE`, `_CELL_SCENE_KIND`, `_CELL_ANCHOR`, `_CELL_BOUNDS`, `_DEPENDENCY_CYCLE`, `_RADII`, `_REFERENCE`, `_PERSISTENT_ID`, `_HLOD`, `_LIMIT` (row ST6) |

The step's algorithm, every check and the worst-case active-cell bound are `Zenith2/Docs/Design/STREAM/Formats.md` → "Streaming world document" (validation) and "Cell scene rules".

### Editor surfaces

In editor play worlds the streaming panel lists every cell with its state, band, instance and delta size; `Streaming.SetCellOverride` (tools context) forces a cell; the debug view draws the grid. The edit world contains no `Zenith.Streaming.WorldRef` unless the author places one, so the edit world is never streamed by STREAM (WORLD edits partitions through documents).

### Migration hooks

`Zenith.Streaming.WorldDocument` starts at schema version 1 with an empty chain; every subsequent schema change adds one step with a fixture pair under `Zenith2/Tools/ZenithCook/Tests/Streaming/Migration/` (JSON text, [Z2-N-CONTRACT-549]). The cooked `.zstream` re-cooks on any format change (Z2-D-048). The cell delta encoding carries its own format version (Formats.md "Cell delta encoding"); a saved delta of an older version is read by the decoder of that version and rewritten at the next capture; each delta record carries its component's schema version and is migrated through `Zenith_MigrateFieldSet` before it is applied.

## Failure handling

### Failure table

| Failure | Detected | Behaviour |
|---|---|---|
| the world asset is corrupt or of another version | the kind loader | the load fails (`ASSET_ERROR_LOADER_FAILED` with the STREAM code); the asset's fallback policy is FAIL, so the persistent scene fails to load; the world runs without streaming and `stream.world` logs |
| the world asset is not active when its reference is bound | `Streaming.Update` | `STREAMING_ERROR_WORLD_NOT_RESIDENT`; binding REFUSED for that reference |
| two enabled world references | `Streaming.Update` | the lowest simulation ID wins; the others are REFUSED with `STREAMING_ERROR_WORLD_DUPLICATE` |
| a cell's scene is missing or corrupt (a content code of Runtime.md "Failure classification": `COOKED_CORRUPT`, `COOKED_VERSION`, `UNKNOWN_TYPE`, `DUPLICATE_INSTANCE`, `ASSET_UNAVAILABLE` with a content ASSET detail such as `ASSET_ERROR_NOT_FOUND` or `ASSET_ERROR_CORRUPT`, …) | SCENE's preparation fails; published | cell FAILED; retried after `stream.cell.retryTicks` up to `stream.cell.maxRetries`, then FAILED_PERMANENT until the world is rebound (its reference disabled and re-enabled, which the editor does to a play world after a cook), a simulation event, so the reset is deterministic |
| a capacity or memory-pressure failure: `SCENE_ERROR_LIMIT` at commit or start (SCENE's set, completion IDs or 64-operation table full), `OUT_OF_MEMORY`, `MEMORY_REFUSED`, `ACTIVATION_CAPACITY`, an unrequested `CANCELLED`, or `ASSET_UNAVAILABLE` with an ASSET capacity detail (`OUT_OF_MEMORY`, `QUEUE_FULL`, `REGISTRY_FULL`, `SHUTTING_DOWN`, `TIMEOUT`) | `OnLoadCommitted` or a published failure | the cell becomes FAILED without consuming a retry and is re-issued after a backoff of `stream.cell.capacityRetryTicks` doubling per consecutive capacity failure up to `stream.cell.retryTicks` (saturating arithmetic); `stream.cells.commit_refused` (at commit), `stream.cells.capacity_failures` |
| a transient failure: `ASSET_UNAVAILABLE` with `ASSET_ERROR_IO`, `NOT_INSTALLED`, `DEPENDENCY_FAILED` (until row ST22 lands) or no detail | a published failure | up to `stream.cell.maxTransientRetries` consecutive ones consume no retry and back off as capacity failures; further ones are content failures; `stream.cells.transient_failures` |
| SCENE refuses the commit as a duplicate | `OnLoadCommitted` | a content failure (another live instance of the cell scene is the game's own): one retry consumed |
| `SCENE_ERROR_ACTIVATION_TOO_LARGE` | a published failure | FAILED_PERMANENT at once (unreachable for a cooked cell, V16) |
| another caller unloads a cell's instance, or gameplay destroys a cell's anchor | observation next tick | the cell is released through the external-unload or destroyed-anchor rows (T24 to T28); its delta keeps every earlier record and the activation's tombstones; `stream.cells.external_unload`, `stream.cells.anchor_destroyed` |
| an instance state no row allows | observation | the instance is released and the cell UNLOADED (T34); `stream.cells.unexpected_state` |
| a save holds more in-flight cell loads than the pool can adopt (a corrupt save), or a restored in-flight load the rebuild cannot adopt for another reason | the rebuild (R4) | the unadopted instances are released, a slot one of them holds neutralised first (empty table, gate open, simulation bit released), and their cells load afresh; `stream.restore.unadopted` |
| SCENE refuses a restore's re-request | `OnLoadCommitted` with instance 0 at restore | nothing is appended; `stream.restore.unadopted` (row ST2 (j)) |
| a delta holder names a cell scene the re-cooked world no longer has | the rebuild (R5) or the binding marks it | records of entities that now belong to another cell are re-homed there by step 14 once that cell is unloaded; the holder is kept with the rest and never applied; `stream.persistence.orphan_deltas`, `stream.persistence.records_rehomed` |
| gameplay destroys a holder (STREAM's runtime entity) | holder discovery, next update | a holder is re-spawned with the cell's current status and an empty delta (the old delta's records are lost); `stream.persistence.holders_respawned` |
| an action buffer is rejected at merge | observation next tick | the action's transition is reverted (Runtime.md "Action buffers and rejection") |
| a cell's activation is rejected by capacity | observation next tick | ACTIVATING → QUEUED; re-admitted at its next turn |
| a delta exceeds its cap | capture | STATE records are dropped in descending persistent ID, DESTROYED records always kept; `stream.persistence.delta_overflow`; they reload in their authored state |
| a delta fails to decode (a corrupt save) | `Streaming.ApplyCellState` | that cell's delta is discarded with `STREAMING_ERROR_DELTA_CORRUPT` logged; the cell activates authored |
| a reference target is not found at bind | playback | the property keeps null; `stream.references.unresolved` |
| a source count above 16 | `Streaming.Update` | sources beyond the 16 lowest simulation IDs are ignored; logged once per world |
| memory pressure | the consumer | the shedding of Runtime.md "Memory pressure and transition peaks" |

### Missing and corrupt content

Every byte STREAM reads from content or a save is validated before use: the `.zstream` by the reader checks of `Zenith2/Docs/Design/STREAM/Formats.md` → "Reader checks", the delta bytes by the decoder's checks of "Cell delta encoding", payloads of STREAM's own command kinds by their size and range checks (a malformed payload is counted and ignored; it can only come from engine code). Cell scenes are SCENE's to validate.

### Allocation failure

`Zenith_Module_Streaming_Start` and `Zenith_StreamingModuleInstance::AttachWorld` unwind on every failing step and return `MODULE_ERROR_OUT_OF_MEMORY` (each step has a fault point, PublicAPI.md "Zenith_Module_Streaming header"). After attach STREAM allocates per tick only from the node's arena (`Zenith_SystemContext::GetScratch`) and from the world state's fixed arrays, so a tick never fails an allocation except through `FATAL_CODE_ARENA_EXHAUSTED` at the arena cap; a snapshot slot is a fixed pool entry; the delta store grows inside the container heap through commands (capacity accepted at merge).

### Device loss

STREAM holds no device resource. During a loss the service's booster and prefetch requests continue (ASSET's recovery pipelines run beside them); PSO group handles complete FAILED with `FLUX_PSO_ERROR_DEVICE_LOST`, which opens the preparation gate (Runtime.md "PSO groups and the preparation gate"); the extractor writes nothing while the renderer's device-loss state is not normal, because the render subgraph does not run, and the next sealed render world after recovery is a full extraction, which re-emits every displayed HLOD node.

### Budget breach

| Row | Breach | STREAM's consequence |
|---|---|---|
| Z2-B-STREAM-001 | BR-GOVERNOR | the governor raises the level; STREAM sheds |
| Z2-B-STREAM-002 | BR-ACCEPT | a BUG on the STREAM epic; nothing at run time |
| Z2-B-STREAM-003 | BR-DEFER | wanted loads wait for a free slot (`stream.cells.loads_deferred`) |
| Z2-B-STREAM-004, -005 | BR-ACCEPT | BUG tickets from the MP-STREAM evidence |

### Cancellation failures

A cancellation cannot fail: every stage has a defined release (Runtime.md "Ownership of partial results"); a cell stuck in CANCELLING longer than 600 ticks (a SCENE preparation that never ends) is logged once under `stream.cell` and keeps its slot; the 1,000-cycle stress asserts the count is zero.

## Test strategy

### Test targets and gate steps

| Target | Gate step | Configurations |
|---|---|---|
| `ZenithStreaming_Tests` | `Test` | `win-debug`, `win-release`; `win-asan` in `Sanitizers` |
| `ZenithCook_Tests` (streaming cases) | `Test` | `win-debug` |
| `Zenith_Integration_Streaming` | `Test` | `win-debug`, `win-release` (the 1,000-cycle stress and determinism suites run in `win-release` for time) |
| `Zenith_Integration_StreamingRender` | `Test` | `win-release` on WARP and lavapipe |
| `Zenith_Fuzz_StreamingWorld` | `Sanitizers -Preset win-asan` (60-second smoke) | `win-asan` |

### Test catalogue index

`Zenith2/Docs/Design/STREAM/Tests.md` lists every named test with the case it asserts, grouped as "Unit tests", "Cook and format tests", "Integration tests", "Stress and determinism tests", "Fuzz target" and "Death and fault-injection tests".

### Fixtures

Every binary fixture is generated at test time; outputs derivable from this specification are compared with committed golden files edited by hand (the two stream dumps; the hand-computed delta bytes of `Test_StreamingPersistence_DeltaGoldenBytes`, written in the test source from Formats.md "Cell delta encoding"). `Zenith_StreamingTestWorld` (header-only, `Tests/Support/`) builds a streaming world, its cell scenes and its HLOD meshes in memory through `Zenith_WriteStreamingWorldFile` and SCENE's and GEOM's writers, and installs them in an in-memory catalogue through ASSET's test fixtures; no binary file is committed. JSON documents for the cook tests are committed as text.

### Stress, fuzz and sanitizer runs

- `Test_Streaming_CancelReload1000Cycles_NoLeak` ([Z2-N-CONTRACT-339]).
- `Test_StreamingDeterminism_ReplayAcrossWorkers` (1, 2, 8 and all workers; forced compaction; two chunk capacities; each ECS backend; [Z2-N-CONTRACT-356]).
- `Zenith_Fuzz_StreamingWorld` over the reader.
- `ZenithStreaming_Tests` under `win-asan`. STREAM is a GPU-matrix module, so `linux-tsan` does not build it (`Zenith2/Docs/Architecture/BuildSystem.md` → "The ZENITH_SUPPORTED matrix" sends module CPU-side suites to `win-asan`), and no ThreadSanitizer run covers it. That is acceptable for one reason stated in full in PublicAPI.md "Zenith_StreamingSnapshotPool" ("Concurrency coverage"): the pool is STREAM's only concurrent code, its protocol is six operations on one slot's atomics with named memory orders, and `Test_StreamingSnapshotPool_EveryInterleavingOfOneSlot` enumerates every interleaving of them deterministically, which a race detector would only sample; `Test_StreamingSnapshotPool_ConcurrentGateAndEnd` adds 8-thread use-after-free and double-release coverage under `win-asan`.

### Acceptance evidence

| Stage 4 acceptance item | Evidence |
|---|---|
| streaming cancellation (1,000 cancel/reload cycles, nothing left behind) | `Test_Streaming_CancelReload1000Cycles_NoLeak` (every stage: I/O, decompression, dependency activation, SCENE's decode, gate and build, queue, activation) and `Test_Streaming_CancelReload1000Cycles_GPUUploads` (GPU upload, on WARP and lavapipe), both registered CTests the acceptance run executes |
| teleport and rapid-traversal rows | MP-STREAM runs of W-Outdoor and W-OpenWorld on the RTX 3070 desktop and the S24 (`needs-human` device steps) for Z2-B-STREAM-004 and -005 |
| streaming CPU and memory rows | MP-CPUZONE and MP-SUBMEM of the same runs for Z2-B-STREAM-002 and -001 |
| OpenWorldSlice playthrough so far | the slice's playthrough with streaming active |

## Budget rows

### Rows STREAM owns

| ID | Name | Status | Unit | Procedure | Breach | Measured by STREAM as |
|---|---|---|---|---|---|---|
| Z2-B-STREAM-001 | Streaming CPU-side resident and in-flight cell data | pending | MB | MP-SUBMEM | BR-GOVERNOR | `MEMORY_SUBSYSTEM_STREAMING`: STREAM's consumer (world assets, world states, snapshot pool, presentation states) plus the shares other consumers report there (ENV's Z2-B-ENV-006, kinds whose memory subsystem is STREAMING) |
| Z2-B-STREAM-002 | Streaming CPU per frame | pending | ms | MP-CPUZONE | BR-ACCEPT | the `Streaming.Update` spans of the frame's ticks plus `Streaming.Service` and `Streaming.ExtractHLOD`; `stream.service.frameBudgetUs` is allocated from it |
| Z2-B-STREAM-003 | Concurrent cell loads in flight | pending | cells | MP-COUNTER | BR-DEFER | `stream.cells.in_flight`; the value is `stream.maxConcurrentCellLoads` |
| Z2-B-STREAM-004 | Teleport-to-resident time | pending | s | MP-STREAM | BR-ACCEPT | the cells part through `stream.teleport.last_to_ready_us`; textures are GEOM's (Z2-B-GEOM-009) |
| Z2-B-STREAM-005 | Rapid-traversal residency miss frames | pending | frames | MP-STREAM | BR-ACCEPT | the cells part through `stream.residency_miss_ticks` mapped to frames by the runner |

**First confirmation at Stage 4.** The Stage 3 W-Outdoor workload runs without the `Streaming` module (the Stage 3 WorkloadOutdoor game does not select it; W-Outdoor's 4 km² fits the no-rebase precision radius, Runtime.md "Precision target"), so no STREAM code exists to produce these numbers at Stage 3. Z2-B-STREAM-001 to -005 and Z2-B-CORE-006 are first confirmed by measurement at Stage 4, in W-Outdoor (re-run with `Streaming` selected) and W-OpenWorld; core-doc item X229 carries the matching exception into `Zenith2/Docs/Roadmap.md` → "Stage 3 Playable authoring slice" ("Stage 3 budget rows"), and row ST11 asks ENV to read its Z2-R-ENV-152 streaming rows the same way. The snapshot pool's fixed 0.9 MiB per attached world (Runtime.md "Resident bytes") is part of Z2-B-STREAM-001's measured share.

All five are allocated by the "Stage 1 per-system budget ticket for STREAM", a single `needs-human` ruling from reference-engine or published data (STREAM has no harness half, `Zenith2/Docs/Budgets.md` → "Per-system producer shape"), blocked by the CORE, JOBS and RPROD rulings.

### Rows STREAM is measured against

| Row | Why |
|---|---|
| Z2-B-CORE-006 Streaming double-residency peak | the reservation for HLOD-covered prefetch (Runtime.md "Memory pressure and transition peaks") |
| Z2-B-ASSET-002, -004, -005, -006 | STREAM's boosters and prefetch run inside ASSET's in-flight and decode budgets |
| Z2-B-ASSET-008 Asset service CPU per frame | ASSET's share of streaming work |
| Z2-B-ECS-003, Z2-B-ECS-004 | activation spawns and STREAM's commands fit the per-tick capacities; `stream.activation.maxEntitiesPerTick` stays below Z2-B-ECS-003 |
| Z2-B-SCENE-001, Z2-B-SCENE-002 | cell scene data and the propagation of activated cells |
| Z2-B-GEOM-003, Z2-B-GEOM-009 | uploads and texture residency of streamed cells |
| Z2-B-PLAT-003 | critical reads of required cells |
| Z2-B-ENV-006, Z2-B-ENV-008 | ENV's share of STREAMING memory and its activation catch-up |
| Z2-B-SHIP-015, Z2-B-SHIP-016 | hitches of activation and rebase ticks |

### Pending row needs

| Row need | Unit | Procedure | Breach | Producer | Why |
|---|---|---|---|---|---|
| Z2-B-STREAM-006 Cell activation entities per tick | entities | MP-COUNTER | BR-DEFER | Stage 1 per-system budget ticket for STREAM | the value of `stream.activation.maxEntitiesPerTick`; the design default 4,096 stands until the row is allocated (row ST16; Budgets' row since X228 (a), [Z2-N-BUDGET-473]); it stays below Z2-B-ECS-003, whose allocated value also caps a cooked cell's entity count (Formats.md V16, X228 (c)) |

## Stage plan

### Stage 1

The "Stage 1 per-system budget ticket for STREAM" allocates Z2-B-STREAM-001 to -006 (-006 is the row of § "Pending row needs"); no STREAM code lands in Stage 1.

### Stage 3 W-Outdoor

The Stage 3 WorkloadOutdoor game does not select the `Streaming` module, so W-Outdoor's Stage 3 runs measure no STREAM row; STREAM's rows are first confirmed at Stage 4 (§ "Budget rows"; `Zenith2/Docs/Roadmap.md` → "Stage 3 budget rows", X229). Nothing of the runtime is pulled into Stage 3 for W-Outdoor.

### Stage 3 sprint pull-forward

Per `Zenith2/Docs/Roadmap.md` → "Stage 3 pull-forward" ("asset-pipeline and streaming foundations"), these parts depend on no unresolved Stage 3 decision and are scheduled in the Stage 3 sprint with fix version `Z2 Stage 4`: the public headers as API-only slices; the `.zstream` format, reader, writer and fuzz target; the document schema, validator and cook step; the kind loader; the band and desired-set algorithms as pure functions with their tests.

### Stage 4

Everything else: the per-tick update and the cell state machine over SCENE's hook (after rows ST2 and ST5 land), action buffers, cancellation and the 1,000-cycle stress, persistence, references, regions and coordinated grids, the rebase policy (after row ST3), HLOD, the service, memory shedding, save and restore, the integration suites (headless and `StreamingRender`), W-Outdoor's and OpenWorldSlice's streaming and the first measurement of STREAM's rows in both workloads, and the Stage 4 acceptance evidence. Navigation coordination's tests use AI's navmesh tiles once D-SEL-NAV (open, Stage 1/2) is ruled.

### API-only slices

| Slice | Headers | Lets these start |
|---|---|---|
| types and errors | `Zenith_StreamingTypes.h`, `Zenith_StreamingErrors.h` | everything |
| components | `Zenith_StreamingComponents.h` | WORLD's partition cook, ENV, LIGHT, PHYS, AI, AUDIO conformance work |
| world asset and writer | `Zenith_StreamingWorldAsset.h`, `Zenith_StreamingWorldWriter.h` | the cook step, test fixtures, RPROD's PSO grouping |
| module entry points | `Zenith_Module_Streaming.h` | MOD's composition tables |

### Stages 5 to 7

No STREAM requirement is staged beyond Stage 4. Stage 5's advanced multi-view adds no streaming source of its own (capture views are presentation; their cameras carry `Zenith.Streaming.Source` only when a game places one). Networked relevancy (Stage 7) is NET's (Z2-D-073).

## Decisions

### Settled decisions

| Decision | How STREAM applies it |
|---|---|
| Z2-D-019 | every streaming decision is a function of simulation state; I/O timing enters through logged publications |
| Z2-D-053 | cell preparations become visible only at a tick boundary, through SCENE's publication |
| Z2-D-080 | `Streaming.Update` runs in phase 1; STREAM's outputs carry per-field authority |
| Z2-D-024 | `Streaming` is a static module selected in `game.json` |
| Z2-D-038 | the document is JSON, the index a versioned binary; the delta store is field sets, never layouts |
| Z2-D-048 | `.zstream` re-cooks; the document migrates; saved deltas migrate per version |
| Z2-D-040, Z2-D-045, Z2-D-046, Z2-D-047 | ownership, errors, no globals, one contract block per type |
| Z2-D-041 | no custom container: `Zenith_Array`, `Zenith_HashMap`, `Zenith_BoundedQueue` and `Zenith_Pool` serve STREAM |
| Z2-D-006, Z2-D-050 | STREAM's work is frame-graph nodes; the extractor writes the pipelined render world |
| Z2-D-002 | games stream through components and bindings, with no C++ |
| Z2-D-043 | HLOD proxies are ordinary meshes; virtualized geometry stays Stage 7 |
| Z2-D-073, Z2-D-075 | no network relevancy and no mod cells in STREAM |
| Z2-D-012 | STREAM's five rows are Budgets v1 rows |
| Z2-D-085, Z2-D-097 | a STREAM contract change edits this family in the same diff; rulings are appended to DecisionLog.md in the ticket's diff |

### Open decisions

| Decision | Effect on STREAM |
|---|---|
| D-SEL-NAV (open, Stage 1/2) | the navigation tile components in NAVIGATION cells and their tests |
| D-SEL-HASH (open, Stage 1) | `Zenith_Hash64` keys of HLOD instances and PSO groups (never serialized, so a change moves them all at once) |
| D-SEL-JSON (open, Stage 1) | the document reader's backing |
| D-PHASES refinement (open, Stage 1; pre-decided Z2-D-080) | the phase-1 placement of `Streaming.Update` |

### Selection and classification decisions STREAM owns

None. The partition model, the HLOD model and the coordination model are design choices recorded in § "Design choices at a glance", not selections among third-party candidates.

### Justification records

None: STREAM adds no custom container, allocator, heap wrapper or threading primitive (Z2-D-041).

### Cross-area rows

The rows STREAM appended to `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md`, in short:

| Row | To | Need |
|---|---|---|
| ST1 | ARCH | ZenithStreaming declares ZenithReflect, ZenithFlux, ZenithShadersGenerated |
| ST2 | SCENE | `Zenith_SceneActivationHook` (eight members), `Zenith_SceneComponentAmender`, `SetActivationHook`, `m_ulHookTag`, `GetLiveInstances`, `GetErrorDetail`, `SetPublicationResolver`, the call points and obligations (a) to (j) |
| ST3 | SCENE | `Zenith.WorldOrigin` and `Scene.ShiftWorldOrigin`, played last in step 7 |
| ST4 | ECS | `PROPERTY_FLAG_WORLD_POSITION`, the schema attribute, the C++ `container<u8>` member form |
| ST5 | MOD, ARCH | `Zenith_WorldAttachContext::GetSceneState()` |
| ST6 | ASSET | kind 35 `ASSET_KIND_STREAMING_WORLD`; `COOK_ERROR_STREAM_*` |
| ST7 | RPROD | cell PSO groups; full-state extraction translating histories on an origin change |
| ST8, ST9, ST10 | PHYS, AI, AUDIO | region flag, grid roles, origin shifts; answered by PHYS (rows PH2, PH20), AI (row AI10; the library stays D-SEL-NAV, open, Stage 1/2) and AUDIO (row AU11) |
| ST11 | ENV | answers EN7; reads `Zenith.WorldOrigin`; ENV's importer reads grid geometry and reports misalignment per ENV-F7 (`COOK_ERROR_IMPORT_FAILED` with an `ENV_ERROR_INVALID_DESC` diagnostic); corrects "STREAM evicts cells" (presentation residency only) |
| ST12 | LIGHT | answers LB20 |
| ST13 | WORLD | partition cook output rules |
| ST14 | SAVE | reconciled with SV10: save-stable hook tags, no STREAM participant; every status a subsequent tick reads saved in components (`CellStatus` on per-cell holders), so captures are EXACT except the three COHERENT cases SCENE reports |
| ST15 | DEVT | panel, debug draw, dump command, MP-STREAM combination |
| ST16 | BUDGET | pending row need "Cell activation entities per tick" |
| ST17 | ARCH | [Z2-N-ARCH-815] deferral and the CRITICAL "evict cells" wording read per [Z2-N-ARCH-812]; the required-load reservation (X226) |
| ST18 | GAME | sources on pawns and cameras, readiness for loading screens and teleports; save fidelity of a capture with cells in flight |
| ST19 | ENV | float-precision acceptance aligned with the 4,096 m precision radius; ENV-152's streaming rows first confirmed at Stage 4 |
| ST20 | ASSET | a test-only hold of one request at a pipeline stage and per-stage cancellation counters |
| ST21 | SCRIPT, MOD | a per-source publication resolver on `Zenith_AsyncResultTable`, the sink's forwarding of it, and the replay rule for a late local completion |
| ST22 | ASSET | the root cause of `ASSET_ERROR_DEPENDENCY_FAILED` in its detail |

## Requirement index

| Requirement | Specified in |
|---|---|
| Z2-R-STREAM-001 Module library target | STREAM.md → "ZenithStreaming target"; STREAM.md → "Supported matrix"; STREAM.md → "Libraries STREAM depends on" |
| Z2-R-STREAM-002 Public headers carry contracts | STREAM/PublicAPI.md → "Header list"; STREAM.md → "Rules across the STREAM API"; STREAM.md → "Public root headers" |
| Z2-R-STREAM-003 API-only slices land first | STREAM.md → "API-only slices"; STREAM.md → "Stage 3 sprint pull-forward" |
| Z2-R-STREAM-004 Module entry points | STREAM/PublicAPI.md → "Zenith_Module_Streaming header" |
| Z2-R-STREAM-005 Construction steps unwind | STREAM/PublicAPI.md → "Zenith_Module_Streaming header"; STREAM/Tests.md → "Death and fault-injection tests"; STREAM.md → "Construction and shutdown" |
| Z2-R-STREAM-006 Attach and detach per world | STREAM/PublicAPI.md → "Zenith_Module_Streaming header"; STREAM.md → "Construction and shutdown"; STREAM/PublicAPI.md → "Zenith_StreamingSnapshotPool" |
| Z2-R-STREAM-007 Components registered with permanent IDs | STREAM/PublicAPI.md → "Zenith_StreamingComponents header"; STREAM.md → "Component types" |
| Z2-R-STREAM-008 Configuration keys and settings | STREAM/PublicAPI.md → "Configuration keys"; STREAM/PublicAPI.md → "Zenith_StreamingSettings"; STREAM/Runtime.md → "Eligible property kinds" |
| Z2-R-STREAM-009 Error record and codes | STREAM/PublicAPI.md → "Zenith_StreamingErrors header"; STREAM.md → "Error enumerations" |
| Z2-R-STREAM-010 Up to eight runtime grids | STREAM/Formats.md → "Grid records"; STREAM/Formats.md → "Grid object"; STREAM/PublicAPI.md → "Zenith_StreamingTypes header" |
| Z2-R-STREAM-011 Cell key space | STREAM.md → "Cell key and coordinates"; STREAM/PublicAPI.md → "Zenith_StreamingWorldAsset header"; STREAM/Formats.md → "Reader checks" |
| Z2-R-STREAM-012 Cell absolute bounds and anchors | STREAM.md → "Cell key and coordinates"; STREAM/Formats.md → "Cell records"; STREAM/Runtime.md → "Large coordinates and origin rebasing" |
| Z2-R-STREAM-013 Grid roles order loading | STREAM/PublicAPI.md → "Zenith_StreamingTypes header"; STREAM/Runtime.md → "Load ordering and admission" |
| Z2-R-STREAM-014 Grid-derived dependencies | STREAM/Formats.md → "Dependency section"; STREAM/Formats.md → "Cook step algorithm" |
| Z2-R-STREAM-015 Dependencies activate first | STREAM/Runtime.md → "Desired-set algorithm"; STREAM/Runtime.md → "Load ordering and admission" |
| Z2-R-STREAM-016 Worst-case active-cell bound | STREAM/Formats.md → "Worst-case active cells"; STREAM/Formats.md → "World record" |
| Z2-R-STREAM-017 Entity bounds within the overlap margin | STREAM/Formats.md → "Entity bounds and the overlap margin"; STREAM/Formats.md → "Validation messages" |
| Z2-R-STREAM-018 Streaming world document schema | STREAM/Formats.md → "Root members"; STREAM/Formats.md → "Grid object"; STREAM/Formats.md → "Cell object"; STREAM/Formats.md → "HLOD object"; STREAM/Formats.md → "Reference object" |
| Z2-R-STREAM-019 Document type registration and migration | STREAM/Formats.md → "Migration chain"; STREAM.md → "Migration hooks" |
| Z2-R-STREAM-020 Cook step stream.world | STREAM/Formats.md → "Cook step algorithm"; STREAM.md → "Cook step" |
| Z2-R-STREAM-021 Every validation message | STREAM/Formats.md → "Validation messages"; STREAM/Tests.md → "Validation message tests" |
| Z2-R-STREAM-022 Derived cell facts | STREAM/Formats.md → "Cell object"; STREAM/Formats.md → "Cook step algorithm" |
| Z2-R-STREAM-023 Cell scene anchor rule | STREAM/Formats.md → "Anchor root"; STREAM/Formats.md → "Cell scene checks" |
| Z2-R-STREAM-024 Cook determinism | STREAM/Formats.md → "Common rules"; STREAM/PublicAPI.md → "Zenith_StreamingWorldWriter header" |
| Z2-R-STREAM-025 .zstream container use | STREAM/Formats.md → "Header use"; STREAM/Formats.md → "Sections" |
| Z2-R-STREAM-026 Record layouts of the index | STREAM/Formats.md → "World record"; STREAM/Formats.md → "Grid records"; STREAM/Formats.md → "Cell records"; STREAM/Formats.md → "Persistent section"; STREAM/Formats.md → "Reference sections"; STREAM/Formats.md → "HLOD node records" |
| Z2-R-STREAM-027 Reader checks S1 to S20 | STREAM/Formats.md → "Reader checks"; STREAM/Tests.md → "Reader check tests" |
| Z2-R-STREAM-028 Parser limits at and above | STREAM/Formats.md → "Limits"; STREAM/PublicAPI.md → "Zenith_StreamingTypes header" |
| Z2-R-STREAM-029 Reader fuzz target | STREAM/Tests.md → "Fuzz target"; STREAM.md → "Integration suite and fuzz target" |
| Z2-R-STREAM-030 World kind loader | STREAM/PublicAPI.md → "Zenith_StreamingWorldKindLoader"; STREAM/Formats.md → "Reader checks" |
| Z2-R-STREAM-031 Asset kind row | STREAM/PublicAPI.md → "ASSET kind row"; STREAM.md → "Reconciliations with other documents" |
| Z2-R-STREAM-032 World binding by component | STREAM/Runtime.md → "World binding" |
| Z2-R-STREAM-033 Binding reads no asynchronous state | STREAM/Runtime.md → "World binding"; STREAM/Runtime.md → "Replay and determinism" |
| Z2-R-STREAM-034 Unbinding unloads paced | STREAM/Runtime.md → "World binding"; STREAM/Runtime.md → "Cell state machine" |
| Z2-R-STREAM-035 Binding allocation failure | STREAM/Runtime.md → "World binding"; STREAM.md → "Allocation failure" |
| Z2-R-STREAM-036 Update node declaration | STREAM/Runtime.md → "Update node declaration"; STREAM.md → "Frame-graph nodes and access"; STREAM.md → "Thread affinity per type" |
| Z2-R-STREAM-037 Fifteen update steps in order | STREAM/Runtime.md → "Steps"; STREAM.md → "Profiler zones" |
| Z2-R-STREAM-038 Distance to a cell | STREAM/Runtime.md → "Distance and band computation" |
| Z2-R-STREAM-039 Five bands with inclusive thresholds | STREAM/Runtime.md → "Distance and band computation" |
| Z2-R-STREAM-040 Forward bias reorders only | STREAM/Runtime.md → "Distance and band computation" |
| Z2-R-STREAM-041 Bounded enumeration | STREAM/Runtime.md → "Distance and band computation"; STREAM/Runtime.md → "Complexity" |
| Z2-R-STREAM-042 Sources capped at sixteen | STREAM/Runtime.md → "Steps"; STREAM.md → "Failure table" |
| Z2-R-STREAM-043 KEEP hysteresis | STREAM/Runtime.md → "Desired-set algorithm" |
| Z2-R-STREAM-044 Dependency closure in one pass | STREAM/Runtime.md → "Desired-set algorithm" |
| Z2-R-STREAM-045 Unload delay | STREAM/Runtime.md → "Desired-set algorithm" |
| Z2-R-STREAM-046 Cell overrides | STREAM/PublicAPI.md → "Zenith_StreamingCommands header"; STREAM/Runtime.md → "Distance and band computation"; STREAM/Runtime.md → "Overrides on the world reference" |
| Z2-R-STREAM-047 Total ordering key | STREAM/Runtime.md → "Ordering key" |
| Z2-R-STREAM-048 Concurrent loads capped | STREAM/Runtime.md → "Caps"; STREAM/Runtime.md → "Loads (step 9)"; STREAM.md → "Counters" |
| Z2-R-STREAM-049 Load actions per tick capped | STREAM/Runtime.md → "Caps"; STREAM/Runtime.md → "Loads (step 9)" |
| Z2-R-STREAM-050 Live instances capped with preemption | STREAM/Runtime.md → "Caps"; STREAM/Runtime.md → "Loads (step 9)"; STREAM/Runtime.md → "World binding" |
| Z2-R-STREAM-051 Activation paced by cells and entities | STREAM/Runtime.md → "Admission (step 10)"; STREAM.md → "Pending row needs"; STREAM/Formats.md → "Validation messages" |
| Z2-R-STREAM-052 Unloads paced farthest first | STREAM/Runtime.md → "Unloads (step 8)" |
| Z2-R-STREAM-053 Boosters raise loading priority | STREAM/Runtime.md → "Sub-steps, in order"; STREAM/Runtime.md → "Teleports" |
| Z2-R-STREAM-054 Prefetch at background priority | STREAM/Runtime.md → "Sub-steps, in order"; STREAM/Runtime.md → "Prefetch disc"; STREAM/Runtime.md → "Request state machine" |
| Z2-R-STREAM-055 Cell state set | STREAM/Runtime.md → "States"; STREAM/PublicAPI.md → "Zenith_StreamingTypes header"; STREAM/Runtime.md → "Totality" |
| Z2-R-STREAM-056 Every transition row | STREAM/Runtime.md → "Transitions" |
| Z2-R-STREAM-057 Capacity failures are not content failures | STREAM/Runtime.md → "Transitions"; STREAM.md → "Failure table"; STREAM/Runtime.md → "Failure classification" |
| Z2-R-STREAM-058 Retries and permanent failure | STREAM/Runtime.md → "Transitions"; STREAM.md → "Failure table"; STREAM/Runtime.md → "Failure classification" |
| Z2-R-STREAM-059 Failed instances released | STREAM/Runtime.md → "Transitions"; STREAM/Runtime.md → "Action buffers and rejection" |
| Z2-R-STREAM-060 One buffer per action | STREAM/Runtime.md → "Action buffers and rejection" |
| Z2-R-STREAM-061 Rejected actions detected and reverted | STREAM/Runtime.md → "Action buffers and rejection"; STREAM/Runtime.md → "Transitions" |
| Z2-R-STREAM-062 Activation is never interrupted | STREAM/Runtime.md → "Transitions"; STREAM/Runtime.md → "Cancellation at every stage" |
| Z2-R-STREAM-063 Admission through the activation hook | STREAM/PublicAPI.md → "SCENE activation hook"; STREAM/Runtime.md → "Admission (step 10)" |
| Z2-R-STREAM-064 Cancel before preparation start | STREAM/Runtime.md → "Cancellation at every stage" |
| Z2-R-STREAM-065 Cancel during I/O | STREAM/Runtime.md → "Cancellation at every stage"; STREAM/Runtime.md → "Ownership of partial results" |
| Z2-R-STREAM-066 Cancel during decompression | STREAM/Runtime.md → "Cancellation at every stage"; STREAM/Tests.md → "Assumed test support of other areas" |
| Z2-R-STREAM-067 Cancel during GPU upload | STREAM/Runtime.md → "Cancellation at every stage"; STREAM/Tests.md → "Render tests" |
| Z2-R-STREAM-068 Cancel during preparation building and gate | STREAM/Runtime.md → "Cancellation at every stage"; STREAM/PublicAPI.md → "Zenith_StreamingActivationHook" |
| Z2-R-STREAM-069 Cancel while queued | STREAM/Runtime.md → "Cancellation at every stage"; STREAM/Runtime.md → "Transitions" |
| Z2-R-STREAM-070 Cancel of presentation requests | STREAM/Runtime.md → "Cancellation at every stage"; STREAM/Runtime.md → "Request state machine" |
| Z2-R-STREAM-071 Ownership of partial results | STREAM/Runtime.md → "Ownership of partial results" |
| Z2-R-STREAM-072 Completion reporting | STREAM/Runtime.md → "Completion reporting"; STREAM.md → "Cancellation summary" |
| Z2-R-STREAM-073 1,000-cycle cancellation stress | STREAM/Tests.md → "Stress and determinism tests"; STREAM.md → "Acceptance evidence" |
| Z2-R-STREAM-074 Stuck cancellation is diagnosed | STREAM/Runtime.md → "Transitions"; STREAM.md → "Cancellation failures" |
| Z2-R-STREAM-075 Snapshot pool sizing | STREAM/PublicAPI.md → "Zenith_StreamingSnapshotPool"; STREAM/Runtime.md → "Restored loads beyond the pool"; STREAM/Runtime.md → "Caps" |
| Z2-R-STREAM-076 Concurrent slot safety | STREAM/PublicAPI.md → "Zenith_StreamingSnapshotPool"; STREAM.md → "Stress, fuzz and sanitizer runs" |
| Z2-R-STREAM-077 Stable hook tags | STREAM/PublicAPI.md → "Zenith_StreamingSnapshotPool" |
| Z2-R-STREAM-078 Hook outlives its preparations | STREAM/PublicAPI.md → "Zenith_StreamingActivationHook"; STREAM/Runtime.md → "World transitions and shutdown" |
| Z2-R-STREAM-079 SCENE hook contract | STREAM/PublicAPI.md → "SCENE activation hook" |
| Z2-R-STREAM-080 Cooked reference table | STREAM/Formats.md → "Reference sections"; STREAM/Formats.md → "Cross-cell reference nulling" |
| Z2-R-STREAM-081 Bind both directions at activation | STREAM/Runtime.md → "Binding" |
| Z2-R-STREAM-082 Unbind at unload | STREAM/Runtime.md → "Unbinding" |
| Z2-R-STREAM-083 Fixed-array elements only | STREAM/Runtime.md → "Rules"; STREAM/Formats.md → "Reference object" |
| Z2-R-STREAM-084 Unresolved references counted | STREAM/Runtime.md → "Binding"; STREAM.md → "Log categories" |
| Z2-R-STREAM-085 Persistence modes | STREAM/Runtime.md → "Modes"; STREAM/Formats.md → "Persistent markers" |
| Z2-R-STREAM-086 Eligible property kinds | STREAM/Runtime.md → "Eligible property kinds" |
| Z2-R-STREAM-087 Tombstones from destruction | STREAM/Runtime.md → "Tombstones"; STREAM/PublicAPI.md → "Zenith_StreamingLifecycleListener" |
| Z2-R-STREAM-088 Tombstoned entities skipped at preparation | STREAM/Runtime.md → "Tombstones"; STREAM/PublicAPI.md → "SCENE activation hook" |
| Z2-R-STREAM-089 Capture at unload | STREAM/Runtime.md → "Capture" |
| Z2-R-STREAM-090 Delta encoding | STREAM/Formats.md → "Delta header"; STREAM/Formats.md → "Entity records" |
| Z2-R-STREAM-091 Delta decoder checks | STREAM/Formats.md → "Decoder checks"; STREAM/Runtime.md → "Restore" |
| Z2-R-STREAM-092 Delta capacity | STREAM/Runtime.md → "Capture"; STREAM/Formats.md → "Delta header" |
| Z2-R-STREAM-093 Restore at activation | STREAM/Runtime.md → "Restore" |
| Z2-R-STREAM-094 World positions anchor-relative | STREAM/Runtime.md → "Eligible property kinds"; STREAM/PublicAPI.md → "ECS world-position flag" |
| Z2-R-STREAM-095 Delta versions and schema migration | STREAM/Formats.md → "Versioning"; STREAM.md → "Migration hooks" |
| Z2-R-STREAM-096 Persistence across reload stress | STREAM/Tests.md → "Integration tests" |
| Z2-R-STREAM-097 Region flag | STREAM/Runtime.md → "The region" |
| Z2-R-STREAM-098 Region destroy policy | STREAM/Runtime.md → "The region" |
| Z2-R-STREAM-099 Physics coordination | STREAM/Runtime.md → "Physics coordination"; STREAM/Formats.md → "Content of each grid role" |
| Z2-R-STREAM-100 Navigation coordination | STREAM/Runtime.md → "Navigation coordination"; STREAM/Formats.md → "Content of each grid role" |
| Z2-R-STREAM-101 Audio coordination | STREAM/Runtime.md → "Audio coordination"; STREAM/Formats.md → "Content of each grid role" |
| Z2-R-STREAM-102 Data coupling only | STREAM/Runtime.md → "What other modules may and may not do"; STREAM.md → "Design choices at a glance" |
| Z2-R-STREAM-103 Light content loads with its cell | STREAM/Formats.md → "Content of each grid role"; STREAM.md → "Areas that consume STREAM" |
| Z2-R-STREAM-104 Two coordinate frames | STREAM/Runtime.md → "The two frames"; STREAM.md → "Cell key and coordinates" |
| Z2-R-STREAM-105 Root translation at activation | STREAM/Runtime.md → "Root translation"; STREAM/PublicAPI.md → "SCENE activation hook" |
| Z2-R-STREAM-106 Rebase rule | STREAM/Runtime.md → "Rebase rule" |
| Z2-R-STREAM-107 Rebasing off by default | STREAM/Runtime.md → "Rebase rule"; STREAM/PublicAPI.md → "Configuration keys"; STREAM/Runtime.md → "World binding" |
| Z2-R-STREAM-108 Long journey precision | STREAM/Runtime.md → "Rebase rule"; STREAM/Tests.md → "Integration tests" |
| Z2-R-STREAM-109 Rebase consumers | STREAM/Runtime.md → "Who reacts to a rebase"; STREAM/PublicAPI.md → "RPROD origin-change rule" |
| Z2-R-STREAM-110 Teleport detection | STREAM/Runtime.md → "Teleports" |
| Z2-R-STREAM-111 Readiness per source | STREAM/Runtime.md → "Readiness" |
| Z2-R-STREAM-112 Teleport-to-ready measurement | STREAM/Runtime.md → "Teleport-to-ready measurement" |
| Z2-R-STREAM-113 Service node | STREAM/Runtime.md → "Service node declaration"; STREAM/Runtime.md → "Sub-steps, in order" |
| Z2-R-STREAM-114 HLOD hierarchy | STREAM/Formats.md → "HLOD node records"; STREAM/Formats.md → "HLOD object" |
| Z2-R-STREAM-115 HLOD display cut | STREAM/Runtime.md → "The cut" |
| Z2-R-STREAM-116 HLOD residency hysteresis | STREAM/Runtime.md → "Residency" |
| Z2-R-STREAM-117 HLOD extraction | STREAM/Runtime.md → "Extraction" |
| Z2-R-STREAM-118 HLOD is presentation only | STREAM/Runtime.md → "Extraction"; STREAM.md → "Design choices at a glance" |
| Z2-R-STREAM-119 PSO groups per cell | STREAM/Runtime.md → "PSO groups and the preparation gate"; STREAM/PublicAPI.md → "RPROD origin-change rule" |
| Z2-R-STREAM-120 Memory consumer | STREAM/Runtime.md → "Resident bytes"; STREAM/PublicAPI.md → "Zenith_StreamingMemoryConsumer" |
| Z2-R-STREAM-121 Shedding per level | STREAM/Runtime.md → "Shedding" |
| Z2-R-STREAM-122 Shedding never changes simulation | STREAM/Runtime.md → "Shedding"; STREAM/Runtime.md → "Replay and determinism" |
| Z2-R-STREAM-123 Double-residency reservation | STREAM/Runtime.md → "Double-residency reservation" |
| Z2-R-STREAM-124 Simulation reads only simulation state | STREAM.md → "Determinism obligations"; STREAM/Runtime.md → "Replay and determinism" |
| Z2-R-STREAM-125 Replay across workers and backends | STREAM/Tests.md → "Stress and determinism tests"; STREAM/Runtime.md → "Replay and determinism" |
| Z2-R-STREAM-126 I/O timing enters only through publications | STREAM/Runtime.md → "Replay and determinism" |
| Z2-R-STREAM-127 State saved as components | STREAM/Runtime.md → "What is saved"; STREAM/Runtime.md → "Persisted cell status" |
| Z2-R-STREAM-128 Rebuild after restore | STREAM/Runtime.md → "Rebuild" |
| Z2-R-STREAM-129 Restored in-flight loads | STREAM/Runtime.md → "Rebuild"; STREAM/PublicAPI.md → "Zenith_StreamingActivationHook"; STREAM/Runtime.md → "Restored loads beyond the pool" |
| Z2-R-STREAM-130 World transition discards in-flight cells | STREAM/Runtime.md → "World transitions and shutdown" |
| Z2-R-STREAM-131 Shutdown mid-load | STREAM/Runtime.md → "World transitions and shutdown"; STREAM.md → "Construction and shutdown" |
| Z2-R-STREAM-132 Headless behaviour | STREAM/Runtime.md → "Headless runs" |
| Z2-R-STREAM-133 Counters | STREAM.md → "Counters" |
| Z2-R-STREAM-134 Log categories and rate limits | STREAM.md → "Log categories" |
| Z2-R-STREAM-135 Profiler zones | STREAM.md → "Profiler zones" |
| Z2-R-STREAM-136 Stream dump | STREAM/Formats.md → "Stream dump format"; STREAM/PublicAPI.md → "Zenith_StreamingState header" |
| Z2-R-STREAM-137 Debug view | STREAM.md → "Debug views and dumps" |
| Z2-R-STREAM-138 Read-world bindings | STREAM/PublicAPI.md → "Script bindings" |
| Z2-R-STREAM-139 Tools-only override binding | STREAM/PublicAPI.md → "Script bindings" |
| Z2-R-STREAM-140 Malformed payloads ignored | STREAM/Formats.md → "Module command payloads"; STREAM/PublicAPI.md → "Command kinds" |
| Z2-R-STREAM-141 Budget rows allocated | STREAM.md → "Rows STREAM owns"; STREAM.md → "Pending row needs"; STREAM.md → "Stage 1" |
| Z2-R-STREAM-142 Streaming memory within its row | STREAM.md → "Rows STREAM owns"; STREAM.md → "Acceptance evidence"; STREAM.md → "Stage 3 W-Outdoor" |
| Z2-R-STREAM-143 Streaming CPU within its row | STREAM.md → "Rows STREAM owns"; STREAM.md → "Acceptance evidence" |
| Z2-R-STREAM-144 Teleport and traversal rows | STREAM.md → "Acceptance evidence"; STREAM/Runtime.md → "Teleport-to-ready measurement" |
| Z2-R-STREAM-145 Rapid traversal stress | STREAM/Tests.md → "Stress and determinism tests" |
| Z2-R-STREAM-146 Traversal integration | STREAM/Tests.md → "Integration tests" |
| Z2-R-STREAM-147 Stage 4 acceptance evidence | STREAM.md → "Acceptance evidence"; STREAM.md → "Stage 3 W-Outdoor" |
| Z2-R-STREAM-148 Failure handling table | STREAM.md → "Failure table"; STREAM.md → "Missing and corrupt content" |
| Z2-R-STREAM-149 Device loss | STREAM.md → "Device loss" |
| Z2-R-STREAM-150 No lock and no custom primitive | STREAM.md → "Lock-order entries"; STREAM.md → "Justification records" |
| Z2-R-STREAM-151 Cancel during SCENE decoding | STREAM/Runtime.md → "Cancellation at every stage"; STREAM/Tests.md → "Assumed test support of other areas" |
| Z2-R-STREAM-152 Cancel during dependency activation | STREAM/Runtime.md → "Cancellation at every stage" |
| Z2-R-STREAM-153 Cancellation with a live booster | STREAM/Runtime.md → "Cancellation at every stage"; STREAM/Runtime.md → "Sub-steps, in order" |
| Z2-R-STREAM-154 External unload and destroyed anchor | STREAM/Runtime.md → "Transitions"; STREAM/Runtime.md → "Totality" |
| Z2-R-STREAM-155 Unexpected instance states released | STREAM/Runtime.md → "Transitions"; STREAM/Runtime.md → "Totality" |
| Z2-R-STREAM-156 No load in a release tick | STREAM/Runtime.md → "Loads (step 9)"; STREAM/Runtime.md → "Failure classification" |
| Z2-R-STREAM-157 Discarded publication recorded | STREAM/PublicAPI.md → "SCENE activation hook"; STREAM/Runtime.md → "Replay and determinism" |
| Z2-R-STREAM-158 Cancelled loads replay exactly | STREAM/Runtime.md → "Replay and determinism"; STREAM/Tests.md → "Stress and determinism tests" |
| Z2-R-STREAM-159 Hook captured per load | STREAM/PublicAPI.md → "SCENE activation hook"; STREAM/PublicAPI.md → "Zenith_StreamingActivationHook" |
| Z2-R-STREAM-160 Content-stable cell identity | STREAM/Runtime.md → "Cell identity across re-cooks"; STREAM/PublicAPI.md → "Zenith_StreamingComponents header" |
| Z2-R-STREAM-161 Re-cooked cells removed or added | STREAM/Runtime.md → "Cell identity across re-cooks"; STREAM/Formats.md → "Decoder checks" |
| Z2-R-STREAM-162 Component presence persisted | STREAM/Runtime.md → "Component presence"; STREAM/Formats.md → "Entity records" |
| Z2-R-STREAM-163 HLOD scales with its driving source | STREAM/Runtime.md → "Driving source and scaling" |
| Z2-R-STREAM-164 Precision target | STREAM/Runtime.md → "Precision target"; STREAM/Runtime.md → "Rebase rule" |
| Z2-R-STREAM-165 Bindings read the shifted origin | STREAM/PublicAPI.md → "Script bindings"; STREAM/Runtime.md → "Who reacts to a rebase" |
| Z2-R-STREAM-166 Required-load double residency | STREAM/Runtime.md → "Double-residency reservation" |
| Z2-R-STREAM-167 Save fidelity with cells in flight | STREAM/Runtime.md → "What is saved" |
| Z2-R-STREAM-168 Discard kind resolved at publication | STREAM/PublicAPI.md → "SCENE activation hook"; STREAM/PublicAPI.md → "SCRIPT publication resolver"; STREAM/Runtime.md → "Replay and determinism" |
| Z2-R-STREAM-169 Replay drops a late local discard | STREAM/PublicAPI.md → "SCRIPT publication resolver"; STREAM/Runtime.md → "Replay and determinism"; STREAM/Runtime.md → "Completion reporting" |
| Z2-R-STREAM-170 ASSET causes of unavailable content classified | STREAM/Runtime.md → "Failure classification"; STREAM/PublicAPI.md → "SCENE activation hook"; STREAM/PublicAPI.md → "ASSET dependency root cause" |
| Z2-R-STREAM-171 Cell status persisted on its holder | STREAM/Runtime.md → "Persisted cell status"; STREAM/PublicAPI.md → "Zenith_StreamingComponents header"; STREAM/Runtime.md → "Action buffers and rejection" |
| Z2-R-STREAM-172 Exact restore of streaming state | STREAM/Runtime.md → "What is saved"; STREAM/Runtime.md → "Rebuild"; STREAM/Runtime.md → "Persisted cell status" |
| Z2-R-STREAM-173 Source, region and reference state persisted | STREAM/Runtime.md → "Teleports"; STREAM/Runtime.md → "The region"; STREAM/Runtime.md → "Overrides on the world reference"; STREAM/Runtime.md → "World binding" |
| Z2-R-STREAM-174 Unadopted restored loads release their slot | STREAM/Runtime.md → "Rebuild"; STREAM/Runtime.md → "Restored loads beyond the pool"; STREAM/PublicAPI.md → "Zenith_StreamingActivationHook" |
| Z2-R-STREAM-175 Deferred re-homes run every tick | STREAM/Runtime.md → "Cell identity across re-cooks"; STREAM/Runtime.md → "Capture"; STREAM/Formats.md → "Decoder checks" |
| Z2-R-STREAM-176 Holders of default status are pruned | STREAM/Runtime.md → "Persisted cell status"; STREAM/Runtime.md → "Complexity" |
