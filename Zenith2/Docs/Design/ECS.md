# ECS design — Reflection & ECS

The design of the ECS area: `ZenithReflect`, the one reflection system with explicit and runtime type registration, stable type and property IDs, schema versions and data-defined component schemas; and `ZenithECS`, the type-erased ECS API with its world simulation (command buffers, sync points, structural epochs, invocation-scoped views, per-column change versions, per-field authority checks, arbitration, the frame-graph translation), implemented by three backends, `ZenithECS_Custom`, `ZenithECS_Flecs` and `ZenithECS_EnTT` (Z2-D-039). It elaborates `Zenith2/Docs/Architecture/ECSModel.md`, `Zenith2/Docs/Architecture/DataDefinedComponents.md`, `Zenith2/Docs/Architecture/FramePhases.md`, `Zenith2/Docs/Architecture/LifecycleCommands.md` and `Zenith2/Docs/Architecture.md` → "Reflection and runtime type registration", "Frame phases and authority" and "Worlds and isolation", and never restates their bullets; where it adds a detail a worker needs it marks it `<!-- elaboration -->` and lists it under § "Reconciliation items". The requirement catalogue is `Zenith2/Docs/Requirements/ECS.md`.

This entry document holds the skeleton and the top-level contracts. Eight split files hold the rest:

| File | Holds | Its `##` headings |
|---|---|---|
| `Zenith2/Docs/Design/ECS/Reflect.md` | the ZenithReflect API and its internals | Header list · Zenith_ReflectIDs header · Zenith_ReflectErrors header · Zenith_TypeInfo header · Zenith_ReflectValueTraits header · Zenith_TypeLayout header · Zenith_TypeRegistry header · Zenith_Arbitration header · Zenith_FieldSet header · Zenith_ReflectionWalk header · Zenith_Migration header · Registry internals · Layout algorithm · Lifecycle hooks · Field-set codecs · Reflection walk algorithm · Reflect internal files |
| `Zenith2/Docs/Design/ECS/Schemas.md` | the data-defined schema front end | Scope of this file · Zenith_Schema header · Schema validation pipeline · Validation rules catalogue · Default values · Authority and arbitration in schemas · Event schemas · The schema lock file · The schema registry blob format · Schema migration · Luau type generation from schemas · Cook integration · Editor re-registration |
| `Zenith2/Docs/Design/ECS/WorldAPI.md` | the ZenithECS public headers with contract blocks, except the four of SystemAPI.md; its header list names every header | Header list · Zenith_ECSTypes header · Zenith_EntityHandle header · Zenith_ComponentHandle header · Zenith_EntityTable header · Zenith_ContainerHeap header · Zenith_ChunkView header · Zenith_Query header · Zenith_World header · Zenith_CommandBuffer header · Zenith_CommandKinds header · Zenith_WorldSimulation header · Zenith_WorldRegistry header · Zenith_WorldInspector header · Zenith_ECSObservers header · Zenith_ECSScriptTraits header · Backend factory headers · Typed convenience wrappers |
| `Zenith2/Docs/Design/ECS/SystemAPI.md` | the system and scheduling headers with contract blocks | Scope of this file · Zenith_System header · Zenith_SystemContext header · Zenith_MergeContext header · Zenith_ScheduleErrors header |
| `Zenith2/Docs/Design/ECS/Queries.md` | queries, views and change tracking | Query compilation · Matched-chunk cache and invalidation · Chunk iteration contract · Change-filtered queries · Views and validation · Change versions and merge versions · Stage order of merge points · Version wrap sweep · Row change bits and CommitRowChanges · Own-row and any-row reads · Resolve and random access |
| `Zenith2/Docs/Design/ECS/Scheduling.md` | systems, commands and playback | System registry internals · Authority and schedule checks · EmitInto translation · Command buffers · Sync-point playback · Tick boundary and end-of-tick · Arbitration merge · Events and reductions · Out-of-tick playback · World registry algorithm · World state hash |
| `Zenith2/Docs/Design/ECS/Backends.md` | the three backends | Common backend contract · Entity table · Change tables · Playback writes · Container heap · Arbitration staging · Custom backend · flecs backend · EnTT backend and the dynamic-storage adapter · Schema revision in the edit world · Factory construction steps · Backend internal files |
| `Zenith2/Docs/Design/ECS/Tests.md` | every test and benchmark | Test files · Reflect tests · Schema tests · ECS unit tests · Conformance suite · Determinism suite · Fuzz targets · Death tests · Backend unit tests · Benchmarks and research scenarios · Stage 1 budget harness · Gate steps per test group |

`<!-- assumption: pending <AREA> -->` marks an interface ECS assumes from a design being written concurrently, and `<!-- assumption: pending core-doc batch 4 (X<n>) -->` or `<!-- assumption: pending core-doc batch (X<n>) -->` (an item queued after batch 4, for the next batch under the freeze rule) a rule this design states before the owning core document does (§ "Reconciliation items"); the first kind is in `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md` (rows E1 onward) for reconciliation.

## Purpose and scope

### What ECS owns

| Piece | Where | Specified in |
|---|---|---|
| the one reflection system: type registry, C++ and runtime registration, stable IDs, schema versions, freeze, the hot-reload window, revisions | `ZenithReflect` | `Zenith2/Docs/Design/ECS/Reflect.md` → "Zenith_TypeRegistry header", "Registry internals" |
| computed layouts, lifecycle hooks, value encodings, field sets and their codecs, the reflection walk, migration types | `ZenithReflect` | `Zenith2/Docs/Design/ECS/Reflect.md` |
| the phase enum, network authority and arbitration declarations (declared in ZenithReflect because authority is reflected metadata) | `ZenithReflect` | `Zenith2/Docs/Architecture/FramePhases.md` → "The twelve phases" |
| data-defined component schemas: validation, error catalogue, lock file, `schemas.zreg`, schema migration, component Luau types | `ZenithReflect` | `Zenith2/Docs/Design/ECS/Schemas.md` |
| the type-erased ECS API: worlds, handles, command buffers, queries, chunk views, systems, contexts, merges | `ZenithECS` | `Zenith2/Docs/Design/ECS/WorldAPI.md`, `Zenith2/Docs/Design/ECS/SystemAPI.md` |
| the world simulation: sync-point playback, the tick boundary, end-of-tick playback, structural epochs, merge versions and the version wrap sweep, change tracking, events, reductions, arbitration merges, merge capacity | `ZenithECS` | `Zenith2/Docs/Design/ECS/Scheduling.md`, `Zenith2/Docs/Design/ECS/Queries.md` |
| the per-field authority checks and schedule-compile errors, and the translation of a world into the CPU frame graph (`EmitInto`) | `ZenithECS` | `Zenith2/Docs/Design/ECS/Scheduling.md` → "Authority and schedule checks", "EmitInto translation" |
| world tags and generation offsets (`Zenith_WorldRegistry`) and world isolation | `ZenithECS` | `Zenith2/Docs/Design/ECS/Scheduling.md` → "World registry algorithm" |
| three backends, the EnTT dynamic-storage adapter, the shared entity table, container heap and change tables | `ZenithECS_Custom`, `ZenithECS_Flecs`, `ZenithECS_EnTT`, `ZenithECS` | `Zenith2/Docs/Design/ECS/Backends.md` |
| the conformance suite every backend passes, the determinism suite, the ECS integration suite `Zenith2/Tests/Determinism/`, the ECS benchmarks and research scenarios | test and benchmark targets | `Zenith2/Docs/Design/ECS/Tests.md` |
| the workload game `Games/WorkloadDense` and its Stage 1 workload scale ticket | ECS epic | § "Stage plan" |

### What ECS does not own

| Not owned | Owner | Why |
|---|---|---|
| the frame-phase contract itself (what each phase owns, lifecycle publication points, the D-PHASES refinement) | GAME (Z2-D-080, D-PHASES refinement, open, Stage 1) | `Zenith2/Docs/Architecture/FramePhases.md` → "Settled rules and ownership": GAME owns the contract, ECS the checks |
| the frame-graph builder, schedule compilation, caching, the executor, `Zenith_FixedOrderReduce` | GRAPH | `Zenith2/Docs/Design/GRAPH.md` → "What this area owns" |
| the job system and `ParallelFor` | JOBS | `Zenith2/Docs/Design/JOBS.md` → "What JOBS owns" |
| Luau transactions, snapshots, shadows, side-effect sub-buffers, script views and their misuse errors, the binding ABI (`Zenith_ScriptBinding.h`, which lives in ZenithReflect), `Zenith_ScriptEmitDefinitions` | SCRIPT | `Zenith2/Docs/Scripting.md` → "Transaction scope and the entity boundary", `Zenith2/Docs/Scripting/Bindings.md` |
| transforms, the transform hierarchy's propagation, scenes, prefabs, `Zenith_LifecycleListener`, the authoring-document migration framework | SCENE (the framework with ASSET, [Z2-N-CONTRACT-550]) | `Zenith2/Docs/Architecture/LifecycleCommands.md` → "Settled rules and ownership" |
| saves and the replay log | SAVE | `Zenith2/Docs/Architecture.md` → "Multiplayer foundations" |
| the inspector UI and the schema editor surfaces | EDIT (ECS exposes the reflection walk and the world inspector) | § "Authoring and cooking" |
| the cook and its `luau-types` step | ASSET (tool), SCRIPT (bindings part) | § "Authoring and cooking" |
| the research methodology and the Stage 5 research report | RESEARCH (Z2-D-064) | ECS supplies the scenarios and harnesses, `Zenith2/Docs/Design/ECS/Tests.md` → "Benchmarks and research scenarios" |
| network IDs and replication | NET | ECS supplies per-row change versions and the replicated flag |

Excluded outright: dynamic plugins and mod-defined components (Z2-D-075, Z2-D-024); a second schema front end or reflection system (Z2-D-020, [Z2-N-CONTRACT-371]); structural changes applied outside playback ([Z2-N-CONTRACT-077]).

### Inventory items covered

ECS covers every item of `Zenith2/Docs/Requirements/Inventory.md` → "Inventory v1", table "ECS Reflection & ECS", all required in Stage 2: `Z2-I-ECS-01` (stable type IDs), `-02` (stable property IDs), `-03` (schema evolution), `-04` (component lifecycle), `-05` (deferred structural changes), `-06` (query invalidation), `-07` (change tracking), `-08` (world isolation), `-09` (scheduling contracts), `-10` (all-backend conformance), `-11` (representative benchmarks), `-12` (runtime type registration), `-13` (data-defined component schemas, `*.schema.json` only, format specified in Stage 1), `-14` (`.d.luau` generated from data-defined schemas), `-15` (data-defined components stored natively in all three backends), `-16` (identical to C++-registered types for every consumer) and `-17` (per-field phase-authority declarations). The mapping is `Zenith2/Docs/Requirements/ECS.md` → "Coverage of inventory items".

### Relationship to the specification

| Specification | This design adds |
|---|---|
| `Zenith2/Docs/Architecture/ECSModel.md` | the complete headers with contract blocks, backend-support types, the merge context and capacity, merge versions, change tables, the playback algorithms, the backends' byte layouts |
| `Zenith2/Docs/Architecture/DataDefinedComponents.md` | the full validation catalogue with messages, the lock file, the `schemas.zreg` byte layout, migration algorithms, component Luau emission |
| `Zenith2/Docs/Architecture.md` → "Reflection and runtime type registration" | runtime builders, value encodings, value-kind hooks, field sets, the reflection walk, revision states |
| `Zenith2/Docs/Architecture/FramePhases.md` | the check order, three appended schedule codes, the translation, merge nodes, arbitration node bodies |

## Dependencies

### Library dependencies

| Library | Layer | `DEPENDS` (exactly `Zenith2/Docs/Architecture.md` → "Library dependency graph") | Third party |
|---|---|---|---|
| `ZenithReflect` | 1 | ZenithCore | — |
| `ZenithECS` | 3 | ZenithCore, ZenithReflect, ZenithGraph, ZenithJobs, ZenithProfiling | — |
| `ZenithECS_Custom` | 4 | ZenithECS, ZenithReflect, ZenithCore | — |
| `ZenithECS_Flecs` | 4 | ZenithECS, ZenithReflect, ZenithCore | flecs |
| `ZenithECS_EnTT` | 4 | ZenithECS, ZenithReflect, ZenithCore | EnTT |

What each edge is for: ZenithCore — types, results, asserts, heap, owned pointers, arenas, containers (`Zenith_Array`, `Zenith_HashMap`, `Zenith_BitArray`, `Zenith_Sort`), `Zenith_Span`, `Zenith_StringView`, `Zenith_StringID` and `Zenith_StringTable`, `Zenith_Hash64` and `Zenith_Hash64Stream`, `Zenith_PersistentID`, the JSON reader and writer, logging, fault injection; ZenithReflect — IDs, type info, phases, arbitration, field sets; ZenithGraph — `uGRAPH_INVALID_INDEX` in diagnostics; ZenithJobs — `Zenith_JobContext`, `Zenith_FrameGraphBuilder`, `Zenith_FixedOrderReduce`, `Zenith_JobAffinity`; ZenithProfiling — zones and counters through the profiler hooks.

### Headings this design relies on

| Heading | For |
|---|---|
| `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_Span", "Zenith_Array", "Zenith_HashMap and Zenith_HashSet", "Zenith_BitArray", "Sorting and searching" | storage of every internal table |
| `Zenith2/Docs/Design/CORE/Text.md` → "Zenith_StringView", "String interning", "JSON reader", "JSON writer" | names, string fields, schema parsing, canonical writing |
| `Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_Hash64", "Vector types", "Zenith_Quaternion", "Zenith_Colour", "Zenith_PersistentID functions" | type IDs, resource keys, value encodings |
| `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Results and the error taxonomy", "Structured logging" | error records ([Z2-N-CONTRACT-480]) and log categories |
| `Zenith2/Docs/Design/CORE/Memory.md` → "Fault-injection support", "Arena internals" | factory fault points; invocation arenas |
| `Zenith2/Docs/Design/GRAPH.md` → "Zenith_FrameGraph.h", "Zenith_FixedOrderReduce.h", "Front-end error mapping" | `EmitInto`'s declarations, reductions, graph error mapping |
| `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Step 0 validation", "Step 1 node names and qualified references", "Step 3 stage fences", "Step 5 version threading", "Invocation IDs", "Hand-off to Flux_RenderWorld" | the translation's rules (cross-area note G13) |
| `Zenith2/Docs/Design/JOBS/PublicAPI.md` → "Zenith_JobContext"; `Zenith2/Docs/Design/JOBS.md` → "Per-worker Luau VM affinity contract" | sub-jobs, arenas, invocation IDs |
| `Zenith2/Docs/Contracts.md` → "Handle kinds", "Reflection", "Errors and results", "Contract blocks per type", "Compatibility policy per kind", "Threading and concurrency contract" | the contracts every type obeys |
| `Zenith2/Docs/Scripting.md` → "Transaction scope and the entity boundary", "Commit and failure semantics", "Merge-time capacity and asynchronous results", "Views epochs and misuse errors", "Determinism rules and randomness" | what chunk systems need from views, merges and capacity |
| `Zenith2/Docs/Budgets.md` → "Per-system rows" | the ECS rows |

### Types used from other areas

`Zenith_Span<T>`, `Zenith_ByteSpan`, `Zenith_MutableByteSpan`, `Zenith_StringView`, `Zenith_StringID`, `Zenith_StringTable`, `Zenith_Hash64`, `Zenith_Hash64Stream`, `Zenith_JsonDocument`, `Zenith_JsonWriter`, `Zenith_Array`, `Zenith_HashMap`, `Zenith_BitArray`, `Zenith_Sort`, `Zenith_Result`, `Zenith_Owned`, `Zenith_Arena`, `Zenith_Heap`, `Zenith_FaultInjector`, `Zenith_PersistentID` (CORE, as `Zenith2/Docs/Design/CORE.md` → "Public API" names them, cross-area item X7 adopted); `Zenith_JobContext`, `Zenith_JobAffinity`, `Zenith_FrameGraphBuilder`, `Zenith_FrameGraphDiagnostic`, `Zenith_FixedOrderReduce` (JOBS and GRAPH); `Zenith_DocumentTree` (CORE, cross-area note C13); `Zenith_ScriptValueTraits`, `Zenith_ScriptOpaqueHandle` (SCRIPT's binding ABI).

### Upward-callback interfaces ECS declares

Every upward notification goes through an interface ECS declares, implemented above it and passed down by the composition root ([Z2-N-ARCH-078], [Z2-N-CONTRACT-175]):

| Interface | Declared in | Implemented by | Called |
|---|---|---|---|
| `Zenith_StructuralObserver` | `ZenithECS/Zenith_ECSObservers.h` | ZenithScene (forwards to `Zenith_LifecycleListener`) | playback step 6 and end-of-tick removal |
| `Zenith_TickBoundaryHandler` (receiving `Zenith_TickBoundaryAccess`: property reads and writes, no structure) | `ZenithECS/Zenith_ECSObservers.h` | ZenithScript, ZenithRuntime, the Cinematics module | tick-boundary steps 1–3 |
| `Zenith_PfnCommandPlayback` (with `Zenith_CommandKindDesc`) | `ZenithECS/Zenith_CommandKinds.h` | modules and ZenithScene (lifecycle commands) | playback step 7 |
| `Zenith_PfnSystemRun`, `Zenith_PfnSystemMerge` | `ZenithECS/Zenith_System.h` | module systems, SCRIPT's chunk and behaviour nodes | the node trampolines |
| `Zenith_ValueKindHooks` (with `Zenith_PfnConstructFromPersistentID`) | `ZenithReflect/Zenith_TypeRegistry.h` | ZenithAssets (asset references) | lifecycle hooks of schema types; asset-default resolution in `ResolveReferenceDefaults` |
| `Zenith_ReferenceResolver` | `ZenithReflect/Zenith_FieldSet.h` | SCENE, SAVE, the world inspector's users | field-set capture and apply |
| `Zenith_MigrationScriptHost` | `ZenithReflect/Zenith_SchemaMigrator.h` | ZenithScript | `script` migration ops |

### Consumers and what they call

| Consumer | Calls |
|---|---|
| MOD (the composition root) | `Zenith_TypeRegistry::Create`, `AttachConfigSchema`, `GetConfigSchema` (for the modules it calls), `BindStringTable`, `InstallValueKindHooks`, `RegisterSchemaBlob`, SCRIPT's behaviour-state entry at step 10 (which calls `BeginRuntimeType`), `Freeze`, `ResolveReferenceDefaults`, `BeginHotReload`/`EndHotReload`/`AbandonHotReload`, `RetireFrame`; `Zenith_WorldRegistry`; `Zenith_CommandKindTable`; `Zenith_PfnCreateWorld`; `Zenith_SystemRegistry::Create`, `GetRejectedRegistrations`; `Zenith_WorldSimulation::Create`, `EmitInto`, `AddStructuralObserver`, `AddTickBoundaryHandler`, `RecordGraphFailure` |
| every module | `Zenith_TypeBuilder`, `Zenith_SystemBuilder`, `Zenith_SystemContext`, `Zenith_CommandBuffer`, `Zenith_ColumnView<T>` |
| SCRIPT | `Zenith_RuntimeTypeBuilder` (behaviour state), `Zenith_SystemContext::GetChunkInScope`, `GetChunkOfEntity`, `Zenith_MergeContext`, `Zenith_ChunkView::CommitRowChanges`, `Zenith_RawColumn::MarkRowChanged`, `Zenith_EmitComponentLuauTypes`, `GetSpawnResults` |
| SCENE, SAVE | `Zenith_WorldInspector`, `ApplyOutOfTick`, field sets, `Zenith_MigrateFieldSet`, `HashTickState` |
| EDIT | the reflection walk, the world inspector, `ApplyOutOfTick`, `RegisterSchemaJson` in the window, `Zenith_World::IsEditWorld` (it reports the flag and never sets it) |
| MOD (world creation) | `Zenith_WorldDesc::m_bEditWorld`, set from the world's purpose when the runtime creates a world (`Zenith2/Docs/Design/ECS/WorldAPI.md` → "The edit-world flag") |
| SCENE (origin shift) | `COMMAND_KIND_FLAG_PLAY_LAST`, `Zenith_WorldPlaybackAccess::GetPlaybackVersion` |
| CINE | `Zenith_TickBoundaryAccess::SetContainer`, `Zenith_SystemContext::Events(sim, ordinal)`, the current logical invocation for random-access writes |
| GAME | the current logical invocation for random-access writes of own-row-only columns |
| AI | `Zenith_WorldPlaybackAccess::RewriteContainer` from a lifecycle listener at a sync point |
| ASSET (cook) | the schema validator, `Zenith_WriteSchemaBlob`, `Zenith_EmitComponentLuauTypes` |
| RPROD, NET | `Zenith_ChunkView::GetRowChangeVersions` |

### Needs recorded for other areas

Rows E1 onward of `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md`; in short: MOD — `Zenith_CommandKindDesc`, `Zenith_PfnCommandPlayback` and `Zenith_EpilogueStageDesc` are declared in ZenithECS, `m_bRenderExtractor`, `Render.Seal` in every configuration, `RecordGraphFailure` after a failed compile, observers and handlers registered before the first tick, the W-Dense per-backend harness (M13), behaviour-state registration at startup step 10 from the cooked script manifest, `ResolveReferenceDefaults` at step 11, the `GetRejectedRegistrations` check after each module's attach, `AddWorldResource`'s phase equal to the type's (E5, E13); SCRIPT — the playback VM index 0xFFFF, script systems register through `Zenith_SystemRegistry` rather than directly on the builder, the merge function type, the Stage 2 component types part, the cooked manifest's behaviour-state schemas, the traits member shape, no publication into a paused world, the SystemAPI.md citation (E6, E12); ARCH — the reconciliation items below; CORE — `Zenith_JsonLimits::ForSchemas`, the document tree; SHIP — a second manifest owner of flecs and EnTT for the native benchmark and the budget harness (E8, E16); SCENE — the parent relation owned by ECS and the SystemAPI.md citations (E9, E15); ASSET — value-kind hooks for asset handles and the persistent-ID resolver (E10, E14); SAVE — the snapshot buffer is a phase-11-owned world resource (E17). Revision 2 adds: SCRIPT — the registry refuses a step-6 behaviour-state registration and the step-10 entry is named, the change-filter rule every script chunk system inherits (E18); MOD — the step-10 call and the once-only `ResolveReferenceDefaults` (E19); SCENE — `GetReadVersion` for the transform propagation's last-seen version (E20); SAVE — `RestoreSimulationState` resets change tracking (E21); DEVT — three counters renamed `_delta` (E22). The consumer-seam additions answer rows EdB26 (the edit-world flag, for MOD and EDIT), S25 (play-last kinds and the playback version, for SCENE), AI24 (`RewriteContainer`, for AI), CN22 and CN23 (the tick-boundary container write and per-ordinal event writers, for CINE) and GA16 with CN24 (the current logical invocation, for GAME and CINE), recorded as row E27.

## Library and file layout

### ZenithReflect target

```cmake
# Zenith2/Engine/ZenithReflect/CMakeLists.txt — the whole file
zenith_add_library(ZenithReflect DEPENDS ZenithCore)
```

`KIND ENGINE`, `SUPPORTED ZENITH_ALL` (the default: `win`, `win-asan`, `linux-tsan`, `android-arm64`, `android-x86_64`; `Zenith2/Docs/Architecture/BuildSystem.md` → "The ZENITH_SUPPORTED matrix"), `EXCEPTIONS OFF`. Public headers (root): `Zenith_ReflectIDs.h`, `Zenith_Phase.h`, `Zenith_NetworkAuthority.h`, `Zenith_Arbitration.h`, `Zenith_ReflectErrors.h`, `Zenith_TypeInfo.h`, `Zenith_ReflectValueTraits.h`, `Zenith_TypeLayout.h`, `Zenith_TypeRegistry.h`, `Zenith_FieldSet.h`, `Zenith_ReflectionWalk.h`, `Zenith_Migration.h`, `Zenith_Schema.h`, `Zenith_SchemaMigrator.h`, `Zenith_SchemaBlob.h`, `Zenith_LuauTypes.h`, and SCRIPT's `Zenith_ScriptBinding.h`. Internal files: `Zenith2/Docs/Design/ECS/Reflect.md` → "Reflect internal files". No platform files. Tests: `Zenith2/Engine/ZenithReflect/Tests/` (`Zenith2/Docs/Design/ECS/Tests.md` → "Test files"), built into `ZenithReflect_Tests`; fuzz targets under `Tests/Fuzz/`.

### ZenithECS target

```cmake
# Zenith2/Engine/ZenithECS/CMakeLists.txt — the whole file
zenith_add_library(ZenithECS DEPENDS ZenithCore ZenithReflect ZenithGraph ZenithJobs ZenithProfiling)
```

`KIND ENGINE`, `SUPPORTED ZENITH_ALL`. It exports no factory ([Z2-N-CONTRACT-038]). Public headers: `Zenith2/Docs/Design/ECS/WorldAPI.md` → "Header list". Internal files:

| File | Contents |
|---|---|
| `Internal/Zenith_WorldSimulation.cpp`, `Internal/Zenith_WorldSimulationState.h` | the simulation object, node records, invocation slots, spawn-result log |
| `Internal/Zenith_EmitInto.cpp` | § "EmitInto translation" of Scheduling.md |
| `Internal/Zenith_SyncPoint.cpp`, `Internal/Zenith_TickBoundary.cpp`, `Internal/Zenith_EndOfTick.cpp` | playback bodies |
| `Internal/Zenith_NodeTrampolines.cpp` | `Zenith_ECSRunSystemNode`, `Zenith_ECSRunMergeNode`, the arbitration node body |
| `Internal/Zenith_SystemRegistry.cpp`, `Internal/Zenith_ScheduleChecks.cpp`, `Internal/Zenith_ScheduleMessages.cpp` | registry, checks, message formatting |
| `Internal/Zenith_SystemContext.cpp`, `Internal/Zenith_MergeContext.cpp`, `Internal/Zenith_Invocation.h` | contexts and the invocation record |
| `Internal/Zenith_CommandBuffer.cpp`, `Internal/Zenith_CommandRecords.h`, `Internal/Zenith_CommandKindTable.cpp` | records, recording, kinds |
| `Internal/Zenith_Events.cpp`, `Internal/Zenith_Reductions.cpp`, `Internal/Zenith_Arbitrate.cpp`, `Internal/Zenith_MergeCapacity.cpp` | events, reductions, arbitration, capacity |
| `Internal/Zenith_ChunkView.cpp`, `Internal/Zenith_MergeVersions.cpp` | views, commit, merge-version table |
| `Internal/Zenith_EntityTable.cpp`, `Internal/Zenith_ContainerHeap.cpp` | backend-support implementations |
| `Internal/Zenith_WorldRegistry.cpp`, `Internal/Zenith_WorldInspector.cpp`, `Internal/Zenith_WorldHash.cpp` | registry, inspector, tick hash |
| `Internal/Zenith_ECSNames.{h,cpp}` | `Zenith_ToString`, counter names, log categories |

No platform files. Tests: `Zenith2/Engine/ZenithECS/Tests/` into `ZenithECS_Tests`; `Tests/Conformance/` into `ZenithECS_Conformance`.

### ZenithECS_Custom target

```cmake
# Zenith2/Engine/ZenithECS_Custom/CMakeLists.txt — the whole file
zenith_add_library(ZenithECS_Custom KIND BACKEND BACKEND_OF ZenithECS BACKEND_NAME Custom
	FACTORY Zenith_CreateWorld_Custom
	DEPENDS ZenithECS ZenithReflect ZenithCore)
```

Public header `Zenith_ECS_Custom.h`; internal files `Zenith2/Docs/Design/ECS/Backends.md` → "Backend internal files"; `Tests/ConformanceAdapter/Zenith_CustomConformanceAdapter.cpp`; `Tests/` (backend unit tests) and `Tests/Native/` (native research scenarios) into `ZenithECS_Custom_Tests`.

### ZenithECS_Flecs target

```cmake
# Zenith2/Engine/ZenithECS_Flecs/CMakeLists.txt — the whole file
zenith_add_library(ZenithECS_Flecs KIND BACKEND BACKEND_OF ZenithECS BACKEND_NAME Flecs
	FACTORY Zenith_CreateWorld_Flecs
	DEPENDS ZenithECS ZenithReflect ZenithCore
	THIRD_PARTY flecs)
```

This is the file `Zenith2/Docs/Architecture/BuildSystem.md` → "The zenith_add_library helper" shows. flecs (manifest owner `ZenithECS_Flecs`, `cmake-subproject`, MIT) is included only from its `Internal/` files; it is built from source in the sanitizer presets (Z2-D-011).

### ZenithECS_EnTT target

```cmake
# Zenith2/Engine/ZenithECS_EnTT/CMakeLists.txt — the whole file
zenith_add_library(ZenithECS_EnTT KIND BACKEND BACKEND_OF ZenithECS BACKEND_NAME EnTT
	FACTORY Zenith_CreateWorld_EnTT
	DEPENDS ZenithECS ZenithReflect ZenithCore
	THIRD_PARTY EnTT)
```

EnTT (manifest owner `ZenithECS_EnTT`, `header-only`, MIT) is included only from its `Internal/` files.

### Test and benchmark targets

| Target | Kind | Directory | Links |
|---|---|---|---|
| `ZenithReflect_Tests`, `ZenithECS_Tests`, `ZenithECS_Custom_Tests`, `ZenithECS_Flecs_Tests`, `ZenithECS_EnTT_Tests` | test | each library's `Tests/` | the library, `ZenithTestSupport`, doctest |
| `ZenithECS_Conformance` | conformance | `ZenithECS/Tests/Conformance/` + each backend's `Tests/ConformanceAdapter/` | ZenithECS and every backend of the preset ([Z2-N-ARCH-682], [Z2-N-ARCH-269]) |
| `Zenith_Fuzz_SchemaBlob`, `Zenith_Fuzz_SchemaJson`, `Zenith_Fuzz_FieldSetDecode` | fuzz (`win-asan`) | `ZenithReflect/Tests/Fuzz/` | ZenithReflect, libFuzzer |
| `Zenith_Integration_Determinism` | integration (`zenith_add_integration_tests(Determinism DEPENDS ZenithRuntime ZenithECS_Custom ZenithECS_Flecs ZenithECS_EnTT)`) | `Zenith2/Tests/Determinism/` | `ZenithRuntime`, the three backends; `SUPPORTED` narrows to `win`, `win-asan` (ZenithRuntime is a GPU library) |
| `Zenith_Bench_ECS`, `Zenith_Bench_ECSNative`, `Zenith_Bench_ECSWorkload`, `Zenith_Bench_BudgetsECS` | benchmark | `Zenith2/Benchmarks/ECS/`, `ECSNative/`, `ECSWorkload/`, `Budgets/ECS/` | `Zenith2/Docs/Design/ECS/Tests.md` → "Benchmarks and research scenarios" (`Zenith_Bench_ECSWorkload` links `ZenithRuntime` and runs the Luau and W-Dense scenarios; `Zenith_Bench_BudgetsECS` runs the native W-Dense half) |

### Supported matrix

Every ECS library is `ZENITH_ALL`: it builds for Windows, both Android ABIs and the `linux-tsan` host, so `linux-tsan` runs the whole reflection, ECS and conformance surface under TSan and UBSan; the determinism integration suite is Windows-only because it links `ZenithRuntime` ([Z2-N-ARCH-173]). On Android presets the test executables are built and not registered with CTest ([Z2-N-ARCH-180]); the S24 run is part of the Stage 2 acceptance.

## Public API

### API rules

- Every fallible public function returns `Zenith_Result<T, E>` with `E` one of `Zenith_ReflectError`, `Zenith_SchemaError`, `Zenith_FieldSetError`, `Zenith_MigrationError`, `Zenith_ECSError` or `Zenith_ScheduleError`, each an error record of [Z2-N-CONTRACT-480] with a `Zenith_ToString`.
- Every public type has one contract block; the declarations are in the split files with their blocks.
- Every ECS entry point is type-erased by reflection ID; templates are inline wrappers that add no storage behaviour ([Z2-N-ARCH-705]), so data-defined components use exactly the code paths of C++ components.
- Backends export exactly their factory ([Z2-N-ARCH-169]); no public header names a flecs or EnTT type ([Z2-N-ARCH-681]).
- `Zenith_SystemRegistry::AddSystem` accepts phases 1 to 11 only; phase 12 (`PHASE_RENDER_EXTRACTION`), `PHASE_COMMANDS_ONLY` and any larger value are rejected at the call with `ECS_ERROR_PHASE_NOT_SCHEDULABLE` and never reach the schedule, because phase-12 work is EPILOGUE work registered through `AddEpilogueStage` or the extractor path (`Zenith2/Docs/Design/ECS/SystemAPI.md` → "Zenith_System header"). <!-- assumption: pending core-doc batch 4 (X84) -->
- No public header includes an STL header outside the allowed list; `Zenith_ReflectIDs.h` includes `<compare>` and `Zenith_ReflectValueTraits.h` `<type_traits>`, both allowed.

### ZenithReflect API summary

| Type or function | Header | Thread affinity | Purpose |
|---|---|---|---|
| `Zenith_TypeID`, `Zenith_PropertyID`, `Zenith_MakeTypeID` | `Zenith_ReflectIDs.h` | IMMUTABLE | stable identity (Z2-I-ECS-01, -02) |
| `Zenith_TypeRegistry`, `Zenith_TypeBuilder`, `Zenith_PropertyBuilder`, `Zenith_RuntimeTypeBuilder`, `Zenith_ValueKindHooks` | `Zenith_TypeRegistry.h` | MAIN_THREAD until Freeze, then ANY_THREAD except the window | registration (Z2-I-ECS-12) |
| `Zenith_TypeInfo`, `Zenith_PropertyInfo`, `Zenith_TypeLifecycleHooks`, the enums | `Zenith_TypeInfo.h` | IMMUTABLE | metadata every consumer reads (Z2-I-ECS-16) |
| `Zenith_TypeLayout`, `Zenith_ComputeTypeLayout` | `Zenith_TypeLayout.h` | IMMUTABLE / ANY_THREAD | computed layouts |
| `Zenith_SerializedFieldSet` and its functions | `Zenith_FieldSet.h` | the calling thread | layout-independent values (Z2-D-038) |
| `Zenith_WalkProperties`, `Zenith_ReadPropertyAt`, `Zenith_WritePropertyAt` | `Zenith_ReflectionWalk.h` | the calling thread | the inspector's and serializers' walk |
| `Zenith_MigrationChain`, `Zenith_MigrationError`, `Zenith_MigrateFieldSet` | `Zenith_Migration.h`, `Zenith_SchemaMigrator.h` | the calling thread | schema evolution (Z2-I-ECS-03) |
| `Zenith_SchemaError`, `Zenith_SchemaDiagnostic`, `Zenith_WriteSchemaBlob`, `Zenith_EmitComponentLuauTypes` | `Zenith_Schema.h`, `Zenith_SchemaBlob.h`, `Zenith_LuauTypes.h` | MAIN_THREAD (cook) | schemas (Z2-I-ECS-13, -14) |
| `Zenith_Phase`, `Zenith_WriterSource`, `Zenith_ArbitrationDecl` and the rest | `Zenith_Phase.h`, `Zenith_Arbitration.h` | IMMUTABLE | per-field authority (Z2-I-ECS-17) |

### ZenithECS API summary

| Type or function | Header | Thread affinity | Purpose |
|---|---|---|---|
| `Zenith_EntityHandle`, `Zenith_ComponentHandle` | `Zenith_EntityHandle.h`, `Zenith_ComponentHandle.h` | IMMUTABLE | world handles |
| `Zenith_World`, `Zenith_WorldPlaybackAccess`, `Zenith_WorldDesc`, `Zenith_PfnCreateWorld` | `Zenith_World.h` | phase-dependent / sync point | the backend interface |
| `Zenith_WorldSimulation` | `Zenith_WorldSimulation.h` | MAIN_THREAD; its node bodies on the job system | the per-world driver |
| `Zenith_CommandBuffer`, `Zenith_MergeKey`, `Zenith_SpawnDesc` | `Zenith_CommandBuffer.h` | OWNER_THREAD | deferred changes (Z2-I-ECS-05) |
| `Zenith_QueryDesc`, `Zenith_ChunkView`, `Zenith_RawColumn` | `Zenith_Query.h`, `Zenith_ChunkView.h` | OWNER_THREAD | chunk queries (Z2-I-ECS-06, -07) |
| `Zenith_SystemRegistry`, `Zenith_SystemBuilder`, `Zenith_SystemDesc`, `Zenith_SystemContext`, `Zenith_MergeContext` | `Zenith_System.h`, `Zenith_SystemContext.h`, `Zenith_MergeContext.h` | MAIN_THREAD / OWNER_THREAD | scheduling (Z2-I-ECS-09) |
| `Zenith_WorldRegistry` | `Zenith_WorldRegistry.h` | MAIN_THREAD | world tags (Z2-I-ECS-08) |
| `Zenith_WorldInspector` | `Zenith_WorldInspector.h` | MAIN_THREAD out of frame | the inspector's data |

### Backend factory headers summary

`Zenith_CreateWorld_Custom` (`ZenithECS_Custom/Zenith_ECS_Custom.h`), `Zenith_CreateWorld_Flecs` (`ZenithECS_Flecs/Zenith_ECS_Flecs.h`), `Zenith_CreateWorld_EnTT` (`ZenithECS_EnTT/Zenith_ECS_EnTT.h`), each `Zenith_Result<Zenith_Owned<Zenith_World>, Zenith_ECSError> (const Zenith_WorldDesc&)` and selected at link time by `backends.ecs` in `game.json` ([Z2-N-ARCH-682]; `Zenith2/Docs/Design/MOD/GameBuild.md` → "Backend to target mapping").

### Chunk queries

The worker-facing contract of reading and writing components from a system; the algorithms are `Zenith2/Docs/Design/ECS/Queries.md`.

1. **Declare.** At registration, `AddSystem` and then `Read`/`Write` per property (or per component, expanded per property), and `Query(desc, slot)` with terms naming the same properties — a query's `m_bWrite` must match a declared write ([Z2-N-ARCH-701]). The system's phase must own every property it writes directly ([Z2-N-ARCH-483]).
2. **Count.** In the system's run function, `xContext.GetChunkCount(slot)` gives this invocation's chunks, after `QUERY_TERM_CHANGED` filtering against what the system's previous invocation read (its read point; its own in-place writes are not re-seen, `Zenith2/Docs/Design/ECS/Queries.md` → "Change-filtered queries"). The filter is conservative and chunk-granular: it never omits a chunk holding a row changed since the read point, and it may hand the system unchanged rows (always, since chunk membership depends on capacity and compaction) and unchanged chunks (after a restore or a sweep reset), so the system's committed result must not depend on whether an unchanged row was handed to it (Z2-D-019). A row-level decision compares `GetRowChangeVersions` against `xContext.GetPreviousReadVersion()`; a system keeps no last-seen version of its own.
3. **Fan out.** Iterate `0 … count − 1` inline, or `xContext.Jobs().ParallelFor(count, grain, pfnBody, pData)` with a grain that never depends on the worker count; each leaf calls `pxNode->ForLeaf(xLeafJobs)` to get its own context.
4. **View.** `GetChunk(slot, i)` returns a `Zenith_ChunkView` for the invocation: `GetRowCount`, `GetEntities`, `GetSimulationIDs`, and `GetColumn(type, property)` → `Zenith_RawColumn` (pointer, stride, optional row-index table). Row r of a column is at `m_pData + (m_puRowIndex ? m_puRowIndex[r] : r) * m_uStride` — every backend, every storage model. `Zenith_ColumnView<T>` wraps it for C++ types.
5. **Write.** Write the declared-writable column in place and call `MarkRowChanged(r)` per written row (or `MarkAllRowsChanged` when writing every row); strings, entity, asset and persistent-ID columns are read-only in views and containers are absent — change them with commands ([Z2-N-ARCH-699]).
6. **Change structure** only by recording into `xContext.Commands(simulationID)`: spawn, destroy, add or remove a component, set a property of another entity or outside the phase's authority; they apply at the sync point after the phase ([Z2-N-ARCH-683]).
7. **Never keep a view.** It is valid for the invocation (until its merge) and until its chunk's epoch changes; C++ misuse asserts, script misuse raises a Luau error in every build ([Z2-N-ARCH-697]).
8. **Never observe order.** Chunk and row order are storage order; order results by stable simulation ID, fold through contributions, or rely on the merge order the ECS applies ([Z2-N-ARCH-702], [Z2-N-ARCH-703]).

The compiled example `Zenith2/Engine/ZenithECS/Tests/Examples/Zenith_ChunkQuery_Example.cpp` registers a two-component system, iterates with `ParallelFor`, writes one column, spawns through a command and is run on every backend by `ZenithECS_Tests`.

### Behaviour-state type registration

Every behaviour-state type (`Behaviour.<Name>.State`, `TYPE_KIND_BEHAVIOUR_STATE`, [Z2-N-SCRIPT-382]) of a game registers in **startup step 10**, after `RegisterSchemaBlob` and before `Freeze`, from the game's **cooked script manifest** (`content:/Scripts/scripts.zlmf`, whose state-schema records are SCRIPT's format: `Zenith2/Docs/Design/SCRIPT/Compiler.md` → "Module manifest format"). The code is SCRIPT's: at step 10 the composition root loads the manifest and calls SCRIPT's behaviour-state entry, `Zenith_ScriptRegisterBehaviourStateTypes(Zenith_TypeRegistry& xTypes, const Zenith_ScriptModuleManifest& xManifest)` (`Zenith2/Docs/Design/SCRIPT/HotReload.md` → "State registration at startup", `Zenith2/Docs/Design/SCRIPT/PublicAPI.md` → "Zenith_ScriptRuntime header"), which registers each state-schema record, in module-ID order (the manifest's order), through `Zenith_TypeRegistry::BeginRuntimeType(TYPE_KIND_BEHAVIOUR_STATE, "Behaviour.<Name>.State", <script name>, <StateVersion>)` … `End` with `Origin(TYPE_ORIGIN_SCHEMA)`; `Zenith_BehaviourComponent`, a C++ engine type, stays in step 6's engine-type declarations. ECS's part is the registry's rules: the registry admits the kind only once `BindStringTable` has run, and `BeginRuntimeType(TYPE_KIND_BEHAVIOUR_STATE, …)` in the DECLARING state (step 6) is `ZENITH_CHECK` (`Zenith2/Docs/Design/ECS/Reflect.md` → "Registration algorithm" item 5), so a step-6 registration fails loudly at the call instead of being accepted.

SCRIPT's revised design declares exactly this entry and this step (`Zenith2/Docs/Design/SCRIPT/HotReload.md` → "State registration at startup", `Zenith2/Docs/Design/SCRIPT/PublicAPI.md` → "Zenith_ScriptRuntime header"), with `Zenith_ScriptRegisterEngineTypes(xTypes)` keeping `Zenith_BehaviourComponent` at step 6, which settles cross-area rows E12a and E18a.

This is the one path before `Freeze`: worlds load at step 17, after the registry froze, so no behaviour-state type registers "at world load". A behaviour-state type added or changed afterwards (script hot reload, an editor session) registers only inside the hot-reload window that ZenithRuntime opens in frame-prologue step 6 (`Zenith2/Docs/Design/ECS/Reflect.md` → "Revisions and the hot-reload window"). A step-10 registration failure is a startup validation failure (exit code 3), with the registry's diagnostics logged. `Test_TypeRegistry_BehaviourStateAtStepTen` registers two behaviour-state types between a schema blob and `Freeze` and asserts they are published in revision 1, and that a third registered after `Freeze` outside a window returns `REFLECT_ERROR_FROZEN`; `Test_TypeRegistry_BehaviourStateBeforeStringTable_Checks` (suite `z2-death-check`) calls `BeginRuntimeType(TYPE_KIND_BEHAVIOUR_STATE, …)` in the DECLARING state and asserts the `ZENITH_CHECK` report.

<!-- assumption: pending core-doc batch 4 (X86) -->
[Z2-N-SCRIPT-382]'s "at world load before the registry freezes" takes this path in core-doc batch 4, and `Zenith2/Docs/Architecture/CompositionRoot.md` → "Startup sequence" step 10 names the SCRIPT entry in the batch that applies X122; the manifest's behaviour-state schema section and the SCRIPT entry that reads it at step 10 (the script runtime itself is created at step 14) are recorded for SCRIPT and MOD in the cross-area notes (rows E12, E13, E18, E19).
<!-- /assumption -->

### Handle kinds used

| Handle | Kind (`Zenith2/Docs/Contracts.md` → "Handle kinds") | ECS's role |
|---|---|---|
| `Zenith_EntityHandle` | world handle (entity), 8 bytes: slot 24 bits, generation 28 bits, world tag 12 bits | declares and resolves it; generation offsets per world |
| `Zenith_ComponentHandle` | world handle (component), 16 bytes | declares it |
| `Zenith_PersistentID` | persistent ID | stores it per entity; serializes references through it |
| `Zenith_AssetHandle<K>` | shared asset handle | stores it in columns as 16 opaque bytes through value-kind hooks |
| `Zenith_QueryID`, `Zenith_QuerySlot`, `Zenith_SpawnTicket` | world-local values, not handle kinds | declares them |

### Configuration keys

| Key | Type, range | Default | Read by | Maps to |
|---|---|---|---|---|
| `ecs.maxEntitiesPerWorld` | u32, 1 … 2^24 | 1,048,576 | composition root, world creation | `Zenith_WorldDesc::m_uMaxEntities` |
| `ecs.containerHeapBytes` | u32, 65,536 … 2^31 | 67,108,864 | world creation | `m_uContainerHeapBytes` |
| `ecs.chunkCapacityRows` | u32, 0 … 1,024 | 0 (backend default) | world creation (tests and research set it) | `m_uChunkCapacityRows` |
| `ecs.maxSpawnsPerTick` | u32 | the Z2-B-ECS-003 value (design default 65,536 while pending) | world simulation | entity-slot capacity class |
| `ecs.maxCommandsPerTick` | u32 | the Z2-B-ECS-004 value (design default 262,144 while pending) | world simulation | command-record capacity class |
| `ecs.maxEventsPerTick` | u32 | the Z2-B-GAME-003 value (design default 65,536 while pending) | world simulation | event-record capacity class |
| `ecs.maxContainerBytesPerTick` | u32 | the Z2-B-SCRIPT-016 value (design default 4,194,304 while pending) | world simulation | container-byte capacity class |
| `ecs.forceCompaction` | bool | false | world simulation (tests, research) | compaction at every sync point |
| `ecs.debugDestroyEvents` | bool | false | world destruction | `STRUCTURAL_EVENT_DESTROYED` per entity on bulk destruction (`Zenith2/Docs/Architecture/LifecycleCommands.md` → "World transition", step 3) |

The four capacity keys are simulation-affecting configuration: they are written into the replay header (`Zenith2/Docs/Architecture.md` → "Multiplayer foundations"), because they decide merge-time acceptance. The whole key set is registered by ZenithECS in the startup step-6 declaration phase through `GetConfigSchema()` with the types, ranges and defaults of the table; the composition root copies `ecs.maxEntitiesPerWorld`, `ecs.containerHeapBytes` and `ecs.chunkCapacityRows` into every `Zenith_WorldDesc` it builds, the world simulation reads the four capacity keys and `ecs.forceCompaction` at creation, and world destruction reads `ecs.debugDestroyEvents`: when true, bulk destruction emits one `STRUCTURAL_EVENT_DESTROYED` per live entity in ascending simulation ID to the structural observers before the hooks run (`Zenith2/Docs/Architecture/LifecycleCommands.md` → "World transition", step 3: per-entity destroyed events in debug diagnostics mode alone, bulk destruction otherwise), when false it emits none. An out-of-range value is rejected by the configuration store at load with the key named (`Zenith2/Docs/Design/CORE/Diagnostics.md` → "Configuration store"). `Test_ECS_ConfigKeysMapToWorldDesc` asserts all of it.

## Data structures and formats

### In-memory structures summary

| Structure | Size | Specified in |
|---|---|---|
| `Zenith_EntityHandle` / `Zenith_ComponentHandle` | 8 / 16 bytes | `Zenith2/Docs/Design/ECS/WorldAPI.md` → "Zenith_EntityHandle header" |
| entity slot record | 56 bytes per slot, reserved for the world's maximum | `Zenith2/Docs/Design/ECS/Backends.md` → "Entity table" |
| custom chunk | 16 KiB (64-byte header, SoA columns aligned 16), capacity from row bytes, at most 1,024 rows | `Zenith2/Docs/Design/ECS/Backends.md` → "Chunk layout" |
| flecs slice / EnTT synthetic chunk | up to 256 rows | `Zenith2/Docs/Design/ECS/Backends.md` → "flecs backend", "EnTT backend and the dynamic-storage adapter" |
| change block | 4 × columns × (capacity + 1) bytes per chunk | `Zenith2/Docs/Design/ECS/Backends.md` → "Change tables" |
| command record header | 32 bytes, payload 8-byte aligned | `Zenith2/Docs/Design/ECS/Scheduling.md` → "Record format" |
| `Zenith_ECSError` / `Zenith_ScheduleError` / `Zenith_ReflectError` | 16 bytes (`static_assert`ed) / ≤ 32 bytes / ≤ 32 bytes | the headers |
| `Zenith_TypeInfo`, `Zenith_PropertyInfo` | registry-owned, immutable per revision | `Zenith2/Docs/Design/ECS/Reflect.md` → "Zenith_TypeInfo header" |

### Formats summary

| Format | Kind | Specified in |
|---|---|---|
| `*.schema.json` | authoring document (JSON), migrated never merely re-cooked | `Zenith2/Docs/Architecture/DataDefinedComponents.md` → "The schema file"; validation `Zenith2/Docs/Design/ECS/Schemas.md` → "Validation rules catalogue" |
| `schemas.lock.json` | tracked JSON lock | `Zenith2/Docs/Design/ECS/Schemas.md` → "The schema lock file" |
| `schemas.zreg` | cooked binary, versioned header, little-endian | `Zenith2/Docs/Design/ECS/Schemas.md` → "The schema registry blob format" |
| binary field-set record | embedded in saves, checkpoints, cooked scenes, replication | `Zenith2/Docs/Design/ECS/Reflect.md` → "Binary field-set record" |
| JSON field-set form | embedded in scene documents and migration fixtures | `Zenith2/Docs/Design/ECS/Reflect.md` → "JSON field-set form" |
| `components.d.luau` (component part) | generated, untracked | `Zenith2/Docs/Design/ECS/Schemas.md` → "Luau type generation from schemas" |
| schema migration fixtures `Schemas/Fixtures/<Component>/v<N>.json` | test data | `Zenith2/Docs/Design/ECS/Schemas.md` → "Migration fixtures" |

## Algorithms and state machines

### Algorithm index

| Algorithm | Heading | Complexity |
|---|---|---|
| registration, Freeze validation, revisions | `Zenith2/Docs/Design/ECS/Reflect.md` → "Registry internals" | O(types × properties) at Freeze |
| layout | `Zenith2/Docs/Design/ECS/Reflect.md` → "Layout algorithm" | O(p log p) |
| schema validation and blob reading | `Zenith2/Docs/Design/ECS/Schemas.md` → "Schema validation pipeline", "The schema registry blob format" | O(bytes) |
| schema migration | `Zenith2/Docs/Design/ECS/Schemas.md` → "Schema migration" | O(fields × ops) |
| query compilation and refresh | `Zenith2/Docs/Design/ECS/Queries.md` → "Query compilation", "Matched-chunk cache and invalidation" | O(new archetypes × terms + chunks) |
| merge versions and the stage order | `Zenith2/Docs/Design/ECS/Queries.md` → "Change versions and merge versions", "Stage order of merge points" | O((n + e) log n) per stage at Freeze for n nodes and e edges, O(1) per merge |
| row change commit | `Zenith2/Docs/Design/ECS/Queries.md` → "Row change bits and CommitRowChanges" | O(rows / 64 + set bits) |
| authority checks | `Zenith2/Docs/Design/ECS/Scheduling.md` → "Authority and schedule checks" | O(accesses log accesses) |
| EmitInto | `Zenith2/Docs/Design/ECS/Scheduling.md` → "EmitInto translation" | O(systems + accesses) |
| merges and capacity | `Zenith2/Docs/Design/ECS/Scheduling.md` → "Commit and collection", "Capacity acceptance" | O(buffers log buffers) per merge |
| sync-point playback | `Zenith2/Docs/Design/ECS/Scheduling.md` → "Steps" | O(records × row bytes) |
| arbitration merge | `Zenith2/Docs/Design/ECS/Scheduling.md` → "Arbitration merge" | O(rows × sources) |
| world registry | `Zenith2/Docs/Design/ECS/Scheduling.md` → "World registry algorithm" | O(4,095) worst case per acquire |
| tick hash | `Zenith2/Docs/Design/ECS/Scheduling.md` → "World state hash" | O(entities log entities + bytes) |
| backend structural operations and compaction | `Zenith2/Docs/Design/ECS/Backends.md` | O(row bytes) per operation |

### Tick walkthrough

One tick of world w, as the executor runs it (`Zenith2/Docs/Architecture/CPUFrameGraph.md` → "Frame structure"):

1. `ECS.TickBoundary`: a pending pause applied (a paused world stops here); the tick base advanced, so V(t, 0) and every other version of the tick are defined; handlers publish asynchronous results, restore cinematics, apply disables, writing properties only through `Zenith_TickBoundaryAccess`; those writes committed at V(t, 0); tick index + 1; capacities reset.
2. Phase p = 1 … 10: system nodes run (in parallel as their accesses allow), each writing declared columns in place with change bits and recording commands; merge nodes run one at a time along the merge chain, which follows the stage's cost-independent topological order, accepting capacity and committing row changes at precomputed merge versions ordered by that same order (`Zenith2/Docs/Design/ECS/Queries.md` → "Stage order of merge points"); arbitration nodes merge staged writes; then the stage-p fence GRAPH generates (`Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Step 3 stage fences") runs the sync-point body: properties (committed once per written chunk column), structure, module and lifecycle commands, events, epochs, query refresh.
3. Phase 11: snapshot nodes read components and write phase-11-owned snapshot buffers; stage 11's barrier runs the end-of-tick playback: pending destroys removed, compaction, deferred frees, delivered events destroyed, every stale query refreshed, and the version wrap sweep when the tick crossed a sweep boundary.
4. After the frame's last tick: stage 12 extractors and epilogue stages in the EPILOGUE subgraph, the extractors ordered before `Render.Seal`, on query caches the last end-of-tick playback refreshed.

### State machines summary

| Machine | States | Table |
|---|---|---|
| type registry | DECLARING, OPEN, FROZEN, WINDOW | `Zenith2/Docs/Design/ECS/Reflect.md` → "Revisions and the hot-reload window" |
| entity slot | FREE → LIVE (Allocate at spawn playback) → DESTROY_PENDING (destroy playback) → FREE (end-of-tick, generation + 1) or RETIRED (generation at the maximum) | `Zenith2/Docs/Design/ECS/Backends.md` → "Entity table" |
| system registry | OPEN (registrations) → FROZEN (Freeze) → RELOADING (BeginReload) → FROZEN (Commit or Abandon) | `Zenith2/Docs/Design/ECS/Scheduling.md` → "System registry internals" |
| command buffer | RECORDING → ACCEPTED or REJECTED (merge) → PLAYED (sync point) → released with the arena | `Zenith2/Docs/Design/ECS/Scheduling.md` → "Commit and collection" |
| chunk view | VALID (created in its invocation) → INVALID (invocation merged, or epoch changed) | `Zenith2/Docs/Design/ECS/Queries.md` → "Views and validation" |
| query cache | FRESH → STALE (structure version changed) → FRESH (refresh at the preceding sync point) | `Zenith2/Docs/Design/ECS/Queries.md` → "Matched-chunk cache and invalidation" |

## Threading lifetime and error contract

### Thread affinity per type

| Type | Keyword | Notes |
|---|---|---|
| `Zenith_TypeRegistry` | `ANY_THREAD_EXCEPT(Create, BeginComponent, BeginType, BeginRuntimeType, RegisterSchemaJson, RegisterSchemaBlob, Freeze, BeginHotReload, EndHotReload, AbandonHotReload, RetireFrame, BindStringTable, InstallValueKindHooks, ResolveReferenceDefaults)` | the listed members on the main thread before Freeze, in the prologue window, or (`ResolveReferenceDefaults`) at startup step 11 before any world exists |
| `Zenith_TypeInfo`, `Zenith_PropertyInfo`, value types, enums | `IMMUTABLE` | |
| `Zenith_World` | `PHASE` | chunk reads and declared writes from nodes; structure at sync points |
| `Zenith_WorldPlaybackAccess` | `OWNER_THREAD` | the sync-point node or out of tick |
| `Zenith_TickBoundaryAccess` | `OWNER_THREAD` | the tick-boundary node, property members only |
| `Zenith_WorldSimulation`, `Zenith_SystemRegistry`, `Zenith_WorldRegistry`, `Zenith_CommandKindTable` (until Seal), `Zenith_WorldInspector` | `MAIN_THREAD` | |
| `Zenith_SystemContext`, `Zenith_MergeContext`, `Zenith_CommandBuffer`, `Zenith_ChunkView`, `Zenith_RawColumn` | `OWNER_THREAD` | one invocation or leaf |
| `Zenith_EntityTable`, `Zenith_ContainerHeap` | `EXTERNALLY_SYNCHRONISED` | mutation at sync points, reads in phases |

### Frame-graph nodes ECS runs

| Node | Subgraph, stage | Declared access | Body |
|---|---|---|---|
| `ECS.TickBoundary` | TICK, 0 | none (it runs before every phase node of its world by the stage order) | `Zenith2/Docs/Design/ECS/Scheduling.md` → "Tick boundary and end-of-tick" |
| every system (phases 1 … 11; phase 12 is rejected by `AddSystem`) | TICK, its phase | its declared property, resource and event reads and writes (writes on its merge node when it has one) | the trampoline and the system's run function |
| `<system>.Merge` | TICK, the system's phase | the system's written properties and emitted events | the merge function |
| `Arbitrate.<Type>.<prop>` | TICK, the writers' phase | reads every staging resource, read-writes the property | the arbitration merge |
| sync points (generated fences with ECS bodies) | TICK, after stages 1 … 10 | the fence's orderings | sync-point playback |
| end of tick (stage 11 barrier body) | TICK, after stage 11 | the fence's orderings | end-of-tick playback |
| epilogue stages and extractors | EPILOGUE, 12 | their reads and sole-consumer drains | the stage's run function |

The rules GRAPH states for this translation (cross-area note G13) hold by construction: no node name contains `@`; one tick-boundary node per world alone at stage 0; arbitration staging resources are non-persistent; disabled systems complete inside ECS's trampoline without a recompile ([Z2-N-ARCH-410]).

### EmitInto rules for the graph core

- `EmitInto` never declares a resource shared between two worlds' groups; every key includes the world tag, so worlds share no edge ([Z2-N-ARCH-423]).
- `EmitInto` is deterministic: identical registrations produce identical builder declarations in identical order (`Test_EmitInto_DeclarationHashStable`), so the schedule's structure hash is stable and caching works.
- A world loaded for a transition contributes nothing until its swap ([Z2-N-ARCH-424]): the runtime calls `EmitInto` for it only in the prologue that performs the swap.

### Lock-order entries

ECS acquires no `Zenith_Mutex` of its own and adds no row to `Zenith2/Docs/Contracts.md` → "Threading and concurrency contract" (table "Lock ordering table"): playback is single-threaded per world on its sync-point node; merge nodes are serialised by the merge chain; invocation lists are lock-free atomic pushes; the registry is immutable outside the prologue window. The locks reached are CORE's: `uLOCK_LEVEL_STRING_TABLE` (850, short-hold; CORE's row in `Zenith2/Docs/Design/CORE/Threading.md` → "Lock-order rows added by CORE", pending in Contracts' table until the CORE tickets land it) when interning string defaults and schema migration values, `uLOCK_LEVEL_HEAP_OOM` (900) on the OOM path, `uLOCK_LEVEL_LOG_SINKS` (920) through logging.

### Construction and shutdown

| Step (`Zenith2/Docs/Architecture/CompositionRoot.md` → "Startup sequence") | ECS action |
|---|---|
| 6 (start) | `Zenith_TypeRegistry::Create` (declaration phase), `AttachConfigSchema` with the configuration schema MOD creates, engine C++ type declarations, then every module's `RegisterTypes` in canonical order registering types and, through `GetConfigSchema()`, configuration keys; MOD then freezes the configuration schema (cross-area note M40; `Zenith2/Docs/Design/MOD.md` → "Configuration-key registration") |
| 10 | `Zenith_WorldRegistry::Create`, `BindStringTable` (interns the declared string defaults), `InstallValueKindHooks(PROPERTY_TYPE_ASSET, …)`, `RegisterSchemaBlob`, the behaviour-state types of the cooked script manifest through SCRIPT's step-10 entry `Zenith_ScriptRegisterBehaviourStateTypes` over the manifest's state-schema records (§ "Behaviour-state type registration"; the registry refuses the kind in DECLARING), `Freeze` — the order of [Z2-N-ARCH-847] (engine types, modules in canonical order, schemas, then `Freeze`) with behaviour-state types after the schemas (X122) |
| 11 | after the asset registry is created: `ResolveReferenceDefaults(PROPERTY_TYPE_ASSET, <ZenithAssets' resolver>, <the asset registry>)`; an unresolvable asset default is exit code 3 (`Zenith2/Docs/Design/ECS/Schemas.md` → "Default values") |
| 15 | `Zenith_CommandKindTable` filled by module starts, then `Seal` |
| 16 | `AcquireTag`, `m_pfnCreateWorld`, `Zenith_SystemRegistry::Create`, `Zenith_WorldSimulation::Create`, module attach (systems, resources, observers, handlers) |
| 17 | scene load through `ApplyOutOfTick`; script systems registered in the system registry |
| 18 | `EmitInto`, then the builder's `Compile` (a failure calls `RecordGraphFailure`, exit code 3) |
| shutdown step 3 | the tick drains; the last sync points flush ([Z2-N-ARCH-560]); commands recorded after the final sync point are counted in `world.commands_after_shutdown` ([Z2-N-ARCH-561]) |
| shutdown step 6 | `Zenith_WorldSimulation` destroyed (bulk destruction through hooks), then `ReleaseTag` |
| after step 10 | the type registry and world registry destroyed by the runtime, after every world |

### Cancellation

ECS has no asynchronous operation of its own. A cancelled frame instance ([Z2-N-ARCH-412]) completes every node that has not started without running its body, the generated fences (sync-point and end-of-tick bodies) and the tick-boundary node included, and lets every node that has started run to completion. Every ECS body is therefore atomic with respect to cancellation — a tick boundary, a merge, an arbitration node, a sync-point playback and an end-of-tick playback each ran entirely or not at all — and the world is left consistent at a sync-point boundary:

1. Storage, structure, epochs, the entity table and every query cache are those of the last sync point (or end-of-tick playback) whose body completed; no playback is half applied, so every binding and cache agrees with storage.
2. Systems of the following phase that ran before the cancellation leave their in-place column writes in storage, uncommitted: no change version advanced for them ([Z2-N-ARCH-694]), and their buffers — accepted by a merge that ran, or never merged — are not played back.
3. At the next tick-boundary node of the world (step 0), the world simulation discards every leftover phase commit list, invocation slot, playback write set and arbitration staging bit of the cancelled instance, logs the discarded buffer count under `ecs.playback` and adds it to `ecs.buffers_discarded_on_cancel`; pending-destroy marks the completed sync points made stay, and those entities are removed by the next end-of-tick playback.
4. A tick instance whose tick boundary ran keeps its advanced tick index; the next frame starts the next tick. Cancellation is not a replayable event, so no determinism guarantee covers a cancelled frame; a replay never cancels.
5. Shutdown cancels only at a frame boundary.
6. Out of tick after a cancelled instance, `Zenith_WorldSimulation::GetChangeVersion()` is V(t, M) of the cancelled tick t, not the version of its latest completed sync point s_k: systems of stage k + 1 whose merge node or final continuation had started completed and took read versions above V(t, s_k), so an editor edit or a load applied before the next tick is stamped above them and seen in the next tick (`Zenith2/Docs/Design/ECS/Queries.md` → "Change versions and merge versions" item 5).

`Test_ECS_CancelledFrameConsistent` asserts it on every backend: a phase-4 system requests cancellation from its body; every subsequent ECS node of the instance completes without its body; the entity table, epochs and every query cache agree with storage (`Zenith_WorldTestAccess::RefreshAllQueries` changes no binding); the next frame's tick boundary discards the stale commit list and logs its count; and the tick hash after the next completed tick equals a reference run in which the cancelled tick's phase-4 merge accepted nothing. A second case covers item 6: a phase-4 `QUERY_TERM_CHANGED(P)` system N with no merge node reads a property a phase-4 merge node commits (so its read point lies after sync point 3) and completes before the cancelling system runs (which declares `After('<N>')`); a `SetProperty` of P applied with `ApplyOutOfTick` between the cancelled frame and the next is stamped V(t, M) of the cancelled tick t, and N matches its chunk in the next tick.

### Error enumerations

| Enumeration | Header | Codes |
|---|---|---|
| `Zenith_ECSErrorCode` in `Zenith_ECSError` | `Zenith_ECSTypes.h` | the specification's nine plus `ECS_ERROR_NULL_HANDLE`, `ECS_ERROR_INVALID_DESC`, `ECS_ERROR_VALUE_SIZE`, `ECS_ERROR_PHASE_NOT_SCHEDULABLE` |
| `Zenith_ScheduleErrorCode` in `Zenith_ScheduleError` | `Zenith_ScheduleErrors.h` | the specification's sixteen plus `SCHEDULE_ERROR_CROSS_ENTITY_READ`, `SCHEDULE_ERROR_SCRIPT_PHASE`, `SCHEDULE_ERROR_INVALID_NAME` |
| `Zenith_ReflectErrorCode` in `Zenith_ReflectError` | `Zenith_ReflectErrors.h` | the specification's eleven plus `REFLECT_ERROR_LAYOUT`, `REFLECT_ERROR_BAD_NAME`, `REFLECT_ERROR_OUT_OF_MEMORY`, `REFLECT_ERROR_BAD_DEFAULT` |
| `Zenith_SchemaErrorCode` in `Zenith_SchemaError` | `Zenith_Schema.h` | the specification's seventeen plus `SCHEMA_ERROR_REGISTRY_FROZEN` |
| `Zenith_FieldSetError` | `Zenith_FieldSet.h` | six codes |
| `Zenith_MigrationErrorCode` in `Zenith_MigrationError` | `Zenith_Migration.h` | six codes |

Every enumeration numbers from 1 ([Z2-N-CONTRACT-465]) and has `Zenith_ToString`. Caller responses: a schema or schedule error at startup is exit code 3 ([Z2-N-ARCH-599], [Z2-N-ARCH-498]); `ECS_ERROR_WORLD_TAGS_EXHAUSTED` fails world creation recoverably ([Z2-N-CONTRACT-098]); a playback error drops the one command and counts it; a factory error during startup is exit code 4 (a required service could not be created); an `ECS_ERROR_PHASE_NOT_SCHEDULABLE` rejection fails the registering module's attach (`MODULE_ERROR_REGISTRATION`, the composition root reads `GetRejectedRegistrations`).

### Luau errors versus asserts

ECS raises no Luau error itself and holds no Luau type. Script misuse is detected by SCRIPT's view layer before it reaches an ECS call, from values ECS exposes in every build (`Zenith_ChunkView::IsValid`, `m_bWritable`, `m_bOwnRowOnly`) ([Z2-N-CONTRACT-473]). ECS asserts only programmer errors in engine code (undeclared access from C++, a stale view in C++, bit-range violations); every failure caused by content, saves, schemas or scripts is a recoverable result or a counted, logged drop ([Z2-N-CONTRACT-472]).

### Determinism obligations

| Obligation | Mechanism |
|---|---|
| merge order, never completion order ([Z2-N-ARCH-689]) | buffers sorted by key at every merge and sync point |
| simulation IDs and slots at playback in merge order ([Z2-N-ARCH-687], [Z2-N-ARCH-688]) | the entity table's lowest-free allocation inside playback |
| change versions independent of scheduling | precomputed merge versions in each stage's cost-independent topological order (`Zenith2/Docs/Design/ECS/Queries.md` → "Change versions and merge versions", "Stage order of merge points") |
| reductions independent of worker count ([Z2-N-ARCH-704]) | `Zenith_FixedOrderReduce` over merge-ordered contributions |
| storage order unobservable ([Z2-N-ARCH-702]) | tick hash and every ordering keyed by simulation ID |
| capacity acceptance in a declaration-determined order ([Z2-N-ARCH-481]) | the merge chain in each stage's cost-independent order (`Zenith2/Docs/Design/ECS/Scheduling.md` → "Merge nodes and the merge chain") and first-fit `TryAccept` |
| `QUERY_TERM_CHANGED` results independent of chunk layout | the relocation rule for destination column versions (`Zenith2/Docs/Design/ECS/Backends.md` → "Change tables"); systems whose committed results never depend on whether an unchanged row or chunk was handed to them, row-level decisions through `GetPreviousReadVersion` (`Zenith2/Docs/Design/ECS/Queries.md` → "Change-filtered queries"); the determinism fixture's row-filtered `CHANGED` system |
| a resumed replay reproduces the recorded hashes ([Z2-N-ARCH-783], [Z2-N-ARCH-789]) | `RestoreSimulationState` resets change tracking, and over-matching changes no committed result (`Zenith2/Docs/Design/ECS/Queries.md` → "Change versions and merge versions" item 2) |
| arbitration independent of worker order ([Z2-N-ARCH-495]) | per-row merge in declared source order |
| no address, slot or worker keys ([Z2-N-ARCH-884]) | every map keyed by value; archetype keys by type-ID hash |

## Diagnostics and profiling

### Counters

Published through `Zenith_DispatchCounter` with the kind in its `eKind` parameter ([Z2-N-CONTRACT-155]), each name suffixed `.<worldTag>`. Per-tick counters are published once per tick per world, after the end-of-tick playback; event counters at their event (the "Published" column names each row's point). Names follow `Zenith2/Docs/Contracts.md` → "Threading and concurrency contract" ([Z2-N-CONTRACT-156]): every delta counter's name ends in `_delta` until the DEVT hook implementation consumes `eKind`, and no other name does. Kinds follow Contracts' definitions: an amount accrued during the tick — a count of records, drops or refreshes, and equally a sum of zone durations in microseconds — is the change since the previous call, so it is DELTA, exactly as JOBS publishes its per-tick amounts and SCRIPT its per-tick totals; a sampled level (entities, chunks, bytes in use, the tick's peak snapshot bytes) is GAUGE, the latest value the hook keeps; a running total since world creation is CUMULATIVE. So a DEVT consumer treats ECS, JOBS and SCRIPT rows alike.

<!-- assumption: pending core-doc batch 4 (X83, X87) -->
`COUNTER_KIND_GAUGE` (an instantaneous value; the hook keeps the latest) is the enumerator X83 adds beside `COUNTER_KIND_CUMULATIVE` and `COUNTER_KIND_DELTA`; the eleven names of [Z2-N-ARCH-713] (`ecs.commands_delta`, `ecs.rollbacks_delta`, …) are reconciled to Contracts' `_delta` rule by X87. This design publishes the Contracts names below; a DEVT consumer keys on them.
<!-- /assumption -->

| Counter | Kind | Published | Value |
|---|---|---|---|
| `ecs.entities` | GAUGE | per tick | live entities after the end-of-tick playback ([Z2-N-ARCH-713]) |
| `ecs.chunks` | GAUGE | per tick | chunks (custom), slices (flecs) or synthetic chunks of the widest query (EnTT) |
| `ecs.archetypes` | GAUGE | per tick | archetypes (custom), tables (flecs) or distinct pool sets in use (EnTT) |
| `ecs.commands_delta` | DELTA | per tick | command records played back this tick ([Z2-N-ARCH-713]) |
| `ecs.command_overwrites_delta` | DELTA | per tick | property commands that overwrote one set earlier in the same playback |
| `ecs.commands_stale_delta` | DELTA | per tick | commands dropped for a stale or foreign handle ([Z2-N-ARCH-490]) |
| `ecs.commands_on_destroyed_delta` | DELTA | per tick | additions dropped on pending-destroy entities ([Z2-N-ARCH-491]) |
| `ecs.playback_us_delta` | DELTA | per tick | this tick's Σ of the `ECS.SyncPoint.P<p>` and `ECS.EndOfTick` zones, microseconds (Z2-B-ECS-002; [Z2-N-ARCH-713]'s `ecs.playback_us`) |
| `ecs.snapshot_bytes` | GAUGE | per tick | this tick's peak snapshot bytes, reported by SCRIPT through the merge context (a sampled peak, not an accrued amount) |
| `ecs.rollbacks_delta` | DELTA | per tick | entity-level rollbacks this tick, reported by SCRIPT through the merge context |
| `ecs.capacity_rejections_delta` | DELTA | per tick | buffers rejected at merge, all classes |
| `ecs.spawns_delta` | DELTA | per tick | spawns played back (never above Z2-B-ECS-003) |
| `ecs.destroys_delta` | DELTA | per tick | entities removed by the end-of-tick playback |
| `ecs.commands_rejected_delta` | DELTA | per tick | records dropped at record time or for a wrong value size |
| `ecs.commands_redundant_delta` | DELTA | per tick | additions of a component the entity already had |
| `ecs.commands_over_cap_delta` | DELTA | per tick | records dropped beyond a buffer's byte cap |
| `ecs.events_delta` | DELTA | per tick | event records accepted |
| `ecs.query_refreshes_delta` | DELTA | per tick | query refreshes performed |
| `ecs.query_refresh_us_delta` | DELTA | per tick | this tick's Σ of the `ECS.QueryRefresh` zones, microseconds |
| `ecs.merge_us_delta` | DELTA | per tick | this tick's Σ of the `ECS.Merge` zones, microseconds |
| `ecs.component_bytes` | GAUGE | per tick | component storage bytes (Z2-B-ECS-001) |
| `ecs.change_bytes` | GAUGE | per tick | change-table bytes (Z2-B-ECS-001) |
| `ecs.container_heap_bytes` | GAUGE | per tick | container heap bytes in use (Z2-B-ECS-001) |
| `ecs.adapter_waste_bytes` | GAUGE | per tick, EnTT only | size-class waste of the dynamic-storage adapter |
| `ecs.reduction_target_invalid_delta` | DELTA | per tick | reduction results dropped for a missing or ambiguous singleton |
| `ecs.version_sweeps` | CUMULATIVE | at each sweep | version wrap sweeps run since world creation |
| `ecs.buffers_discarded_on_cancel` | CUMULATIVE | at a tick boundary after a cancelled frame | buffers discarded since world creation (§ "Cancellation") |
| `world.commands_after_shutdown` | CUMULATIVE | at world destruction | commands recorded after the final sync point ([Z2-N-ARCH-561]) |
| `world.async_discarded` | CUMULATIVE | at world destruction | asynchronous results discarded with the world |

`Test_ECS_CountersPublished` asserts every row: its name with the world-tag suffix, its kind, and its publication point — each per-tick row exactly once per tick per world after the end-of-tick playback; `ecs.version_sweeps` once per sweep; `ecs.buffers_discarded_on_cancel` at the tick boundary after a cancelled frame and at no other; the two `world.` rows once, at world destruction.

### Profiling zones

`ECS.TickBoundary`, `ECS.SyncPoint.P<p>`, `ECS.EndOfTick`, `ECS.QueryRefresh`, `ECS.VersionSweep`, `ECS.Merge` (inside each merge node), `ECS.Arbitrate`, `ECS.EmitInto` (main thread), `Reflect.Freeze`, `Reflect.RegisterSchemaBlob`, `Reflect.HotReload`; node zones are GRAPH's trampoline's.

### Log categories and messages

| Category | Messages |
|---|---|
| `ecs.reflect` | every registry error with both parties or the missing piece; a revision published (info) |
| `ecs.schema` | every schema diagnostic in `<file>: <keyPath>: <message>` form |
| `ecs.schedule` | every schedule diagnostic in its message format |
| `ecs.playback` | a dropped command: `ecs: dropped <kind> from <system> (entity <simID>): <reason>` (rate-limited to 64 lines per tick with a count of the rest) |
| `ecs.merge` | a capacity rejection with its class |
| `ecs.world` | world creation, destruction and tag issue (info) |

### On-screen profiler rows

DEVT draws them from the counters alone: entities, chunks and archetypes per world; commands, overwrites and drops per tick; playback time against Z2-B-ECS-002; spawns against Z2-B-ECS-003; command records against Z2-B-ECS-004; component and change-tracking bytes against Z2-B-ECS-001; capacity rejections by class.

### Inspector data

EDIT's inspector shows every component of an entity through `Zenith_WorldInspector` (entities by simulation ID, components by stable name, values gathered into the reflected layout) and the reflection walk (property names, value types, ranges, tooltips and categories, hidden flags), with no branch on origin, so a data-defined component and a C++ one look identical ([Z2-N-ARCH-592], [Z2-N-ARCH-643]); the inspector's provenance line reads `m_szSourcePath`.

### Debug views dumps and telemetry

The development console command `ecs.dump <worldTag> <path>` writes a JSON dump (schema `zenith2.ecs-dump` version 1): per archetype its type set, chunk count and rows; per query its matched chunk count and staleness; per system its phase, merge node and last merge version; written with the canonical JSON writer. Telemetry per frame carries `ecs.entities`, `ecs.playback_us_delta` and `ecs.capacity_rejections_delta` of the primary world.

## Authoring and cooking

### Schema authoring

Game authors write `Games/<Name>/Schemas/*.schema.json` by hand or through EDIT's schema panel, commit `schemas.lock.json` and the migration fixtures with every version change, and see errors as `<file>: <keyPath>: <message>` from the cook, from `ZenithNewGame validate` and in the editor (`Zenith2/Docs/Design/ECS/Schemas.md`).

### Cook steps

ZenithCook (ASSET's tool) validates every schema, writes `schemas.zreg`, runs the `luau-types` step (component part from `Zenith_EmitComponentLuauTypes`) and, on the explicit verb, `schema-lock` (`Zenith2/Docs/Design/ECS/Schemas.md` → "Cook integration").

### Editor surfaces

EDIT owns the UI: the inspector (fed by § "Inspector data"), the schema panel and the re-registration flow between play sessions (`Zenith2/Docs/Design/ECS/Schemas.md` → "Editor re-registration"). Inspector edits become `SetProperty`, `AddComponent` and `RemoveComponent` commands applied with `ApplyOutOfTick`, so an edit is undoable by EDIT's command system and never writes storage directly.

### Migration hooks

Schema migration ops and C++ `Migration` steps are applied by SCENE's authoring-document framework (documents) and by SAVE (saves) through `Zenith_MigrateFieldSet`; cooked data re-cooks ([Z2-N-ARCH-605]).

## Failure handling

| Failure | Where | Defined behaviour |
|---|---|---|
| unknown type on load | a document or save names a type ID the registry lacks | SCENE and SAVE keep the component's field set as unknown data, warn, and do not create the component; playback of a spawn or add naming an unregistered type drops it with `ECS_ERROR_UNKNOWN_TYPE` logged ([Z2-N-CONTRACT-535]) |
| unknown property | a field set or command names a property the type lacks | field sets keep it as an unknown field with a warning; a command is dropped with `ECS_ERROR_UNKNOWN_PROPERTY` logged |
| schema version mismatch | data older than the type | migrated forward through the chain; newer than the type: `MIGRATION_ERROR_NEWER_VERSION`, the document opens read-only ([Z2-N-CONTRACT-537]) |
| migration failure | a retype or script op fails | the document or save migration fails with `Zenith_MigrationError`, input unchanged ([Z2-N-ARCH-607]) |
| invalid schema | cook | the cook fails with every diagnostic ([Z2-N-ARCH-599]) |
| unresolvable asset default | cook (after `Freeze`), startup step 11 | `ResolveReferenceDefaults` returns `SCHEMA_ERROR_BAD_DEFAULT` with every unresolvable field in its diagnostics; the cook fails, the runtime stops with exit code 3 before any world exists (`Zenith2/Docs/Design/ECS/Schemas.md` → "Default values") |
| unschedulable system phase | `AddSystem` | the registration is rejected at the call with `ECS_ERROR_PHASE_NOT_SCHEDULABLE`, logged and kept in `GetRejectedRegistrations`; the registering module's attach fails (`MODULE_ERROR_REGISTRATION`), so startup stops before the first tick |
| behaviour-state registration failure | startup step 10 | exit code 3 with the registry's diagnostics (§ "Behaviour-state type registration") |
| corrupt `schemas.zreg` | startup step 10 | `SCHEMA_ERROR_BLOB_CORRUPT`; the game stops before its first tick with exit code 3 |
| schedule error | startup step 18 | exit code 3 with every diagnostic; on recompile the previous schedule is kept and the change rejected ([Z2-N-ARCH-409]) |
| merge-point limit | `Freeze` | a schedule whose tick has 2^20 or more merge points fails `Freeze` with `ECS_ERROR_CAPACITY` logged and one `SCHEDULE_ERROR_GRAPH` diagnostic; exit code 3 at startup, the previous schedule kept on a recompile (`Zenith2/Docs/Design/ECS/Queries.md` → "Change versions and merge versions" item 1); `m_uMaxSystems` outside 1 … 65,536 is `ECS_ERROR_INVALID_DESC` at `Zenith_SystemRegistry::Create` |
| entity capacity exhausted | merge | the spawn is rejected (entity-level rollback for scripts, a dropped buffer for C++), `ecs.capacity_rejections_delta`; never an assert |
| command, event or container capacity exhausted | merge | as above, per class |
| container heap exhausted | merge | rejected with a diagnostic ([Z2-N-ARCH-716], `Test_ECS_ContainerHeapExhaustion`) |
| archetype or query capacity | playback, `CompileQuery` | `ECS_ERROR_CAPACITY` logged; the causing command is dropped; a query that cannot compile fails `Freeze` |
| world-tag exhaustion | world creation | `ECS_ERROR_WORLD_TAGS_EXHAUSTED` returned before any backend factory ([Z2-N-CONTRACT-098]) |
| allocation failure | factory | the factory unwinds and returns `ECS_ERROR_OUT_OF_MEMORY` ([Z2-N-ARCH-684]) |
| allocation failure | playback, merge, arenas | world-heap and arena allocation route through the OOM handler; exhaustion is `FATAL_CODE_OUT_OF_MEMORY` or `FATAL_CODE_ARENA_EXHAUSTED` (non-replayable, [Z2-N-ARCH-790]); per-tick demand is bounded by the capacity rows, so a correctly budgeted world never reaches it |
| stale or foreign handle in a command | playback | dropped with `ecs.commands_stale_delta` and a log entry, never an assert ([Z2-N-ARCH-490]) |
| cancellation | frame | the world is left at the last completed sync point; leftovers discarded at the next tick boundary (§ "Cancellation") |
| change-version wrap | long-running world | the version wrap sweep keeps every comparison below 2^31 merge points, so a `QUERY_TERM_CHANGED` query never misses a change; a query slot whose system completed no invocation for more than 2^30 merge points (a system disabled that long, then re-enabled) is reset and matches every chunk in its next invocation, a false positive confined to that one invocation (`Zenith2/Docs/Design/ECS/Queries.md` → "Version wrap sweep" item 4) |
| device loss | — | not an ECS event: headless and device-lost runs keep simulating ([Z2-N-ARCH-868]) |
| budget breach | Z2-B-ECS-002 | BR-ACCEPT: blocks the stage acceptance until fixed or re-budgeted |
| budget breach | Z2-B-ECS-001 | BR-GOVERNOR: the simulation subsystem raises `memory.simulation_over_budget` and sheds nothing (`Zenith2/Docs/Architecture.md` → "Memory governor") |
| budget breach | Z2-B-ECS-003, Z2-B-ECS-004 | BR-MERGE: deterministic merge-time rejection |

## Test strategy

### Test kinds and locations

| Kind | What | Where |
|---|---|---|
| unit | every public type's behaviour and every failure scenario | `Zenith2/Docs/Design/ECS/Tests.md` → "Reflect tests", "Schema tests", "ECS unit tests", "Backend unit tests" |
| conformance | every scenario on every backend | `Zenith2/Docs/Design/ECS/Tests.md` → "Conformance suite" |
| determinism | 1, 2, 8 and all workers × compaction × two capacities × backends | `Zenith2/Docs/Design/ECS/Tests.md` → "Determinism suite" |
| integration | world isolation, runtime replays | `Zenith2/Tests/Determinism/` |
| failure injection | every factory step | `Test_ECS_FactoryFailureUnwinds`, `Test_*_Create_FailsAt<Step>_Unwinds` |
| fuzz | schema blob, schema JSON, field-set decoding | `Zenith2/Docs/Design/ECS/Tests.md` → "Fuzz targets" |
| death | programmer-error checks | `Zenith2/Docs/Design/ECS/Tests.md` → "Death tests" |
| sanitizer and stress | TSan on the concurrent registry reads, the merge chain and parallel system iteration | `linux-tsan` runs every ECS test |
| examples | compiled usage examples | `Tests/Examples/` of ZenithReflect and ZenithECS |
| benchmarks | native and through the API, per §5.5 | `Zenith2/Docs/Design/ECS/Tests.md` → "Benchmarks and research scenarios" |

### Gate steps per test group summary

The `Test` step runs every unit, conformance, determinism, death and example test and every benchmark's `--smoke`; the `Zenith2ECS` category's `EcsConformance` line builds and runs `ZenithECS_Conformance` against the three backend targets on every diff touching `ZenithECS*` ([Z2-D-039]; `Zenith2/Docs/Contracts.md` → "Enforceability matrix"); `Sanitizers` runs when a diff also touches a `Zenith2Memory` path and at acceptance; fuzz smokes run in `win-asan`. The table is `Zenith2/Docs/Design/ECS/Tests.md` → "Gate steps per test group".

## Budget rows

### Rows ECS owns

| Row | Name | Status | Procedure | ECS measurement |
|---|---|---|---|---|
| Z2-B-ECS-001 | Component storage per world | pending, allocated by the Stage 1 per-system budget ticket for ECS | MP-SUBMEM, BR-GOVERNOR | `ecs.component_bytes` + `ecs.change_bytes` + `ecs.container_heap_bytes` + entity-table bytes per world, reported through the simulation memory consumer |
| Z2-B-ECS-002 | Sync-point command playback CPU per tick | pending, same producer | MP-CPUZONE, BR-ACCEPT | Σ of the `ECS.SyncPoint.P<p>` and `ECS.EndOfTick` zones per tick (`ecs.playback_us_delta`) |
| Z2-B-ECS-003 | Spawn capacity per tick | pending, same producer | MP-COUNTER, BR-MERGE | the value of `ecs.maxSpawnsPerTick`; `ecs.spawns_delta` never exceeds it |
| Z2-B-ECS-004 | Structural command capacity per tick | pending, same producer | MP-COUNTER, BR-MERGE | the value of `ecs.maxCommandsPerTick`; `ecs.commands_delta` never exceeds it |

The producer is split per Z2-D-084: the harness ticket "Stage 1 per-system budget harness for ECS" (`Zenith2/Benchmarks/Budgets/ECS/`, blocked by the Stage 1 workload scale ticket for W-Dense, [Z2-N-BUDGET-022]) and the `needs-human` ruling "Stage 1 per-system budget ticket for ECS", blocked by the CORE, JOBS and RPROD rulings ([Z2-N-BUDGET-023]).

### Rows ECS is measured against

- Z2-B-GAME-003 (event queue capacity per tick) and Z2-B-SCRIPT-016 (container storage capacity per tick): capacity classes the merge enforces; SCRIPT-012 and SCRIPT-013 (snapshot memory) through the counters SCRIPT reports into `ecs.snapshot_bytes`.
- The CPU-SIM and MEM-WIN-RAM / MEM-S24 allocation sums of which Z2-B-ECS-002 and Z2-B-ECS-001 are members (`Zenith2/Docs/Budgets.md` → "Purpose versioning and change rule", table "Allocation sums").
- The W-Dense rows, which every selectable backend meets ([Z2-N-BUDGET-204]), through `Zenith_Bench_ECSWorkload` (the `Zenith_ECS` path, `Bench_ECS_WDense`).

### Native-path measurements of the ECS rows

The research methodology measures every scenario natively and through `Zenith_ECS` (Z2-D-064); for the ECS rows the two paths are:

| Row | Through `Zenith_ECS` | Natively |
|---|---|---|
| Z2-B-ECS-001 Component storage per world | `ecs.component_bytes` + `ecs.change_bytes` + `ecs.container_heap_bytes` + entity-table bytes in `Bench_ECS_WDense` (`Zenith_Bench_ECSWorkload`) | component storage bytes per entity of the W-Dense mix in `Bench_ECSNative_<Backend>_WDense` (`Zenith_Bench_BudgetsECS`), with no change tables, container heap or entity table |
| Z2-B-ECS-002 Sync-point command playback CPU per tick | `ecs.playback_us_delta` in `Bench_ECS_WDense` | the mix's per-tick structural changes applied through each library's own API in `Bench_ECSNative_<Backend>_WDense` |
| Z2-B-ECS-003 Spawn capacity per tick | `ecs.spawns_delta` in `Bench_ECS_WDense`, never above the row | the mix's per-tick spawn count, as the harness's input, recorded beside the native result |
| Z2-B-ECS-004 Structural command capacity per tick | `ecs.commands_delta` in `Bench_ECS_WDense`, never above the row | the mix's per-tick command count, as the harness's input |

The budget ruling reads the `Zenith_ECS` column (what a game pays); the Stage 5 research report reads both columns, attributing their difference to the adapter and the runtime (`Zenith2/Docs/Design/ECS/Tests.md` → "Benchmarks and research scenarios", scenario `WDense`).

### Row needs recorded as pending

None beyond the four ECS rows; the design defaults of § "Configuration keys" apply only while those rows are pending, as the configuration's engine-default layer, and move to the generated budget table when the producer allocates them.

## Stage plan

### Stage 1

| Ticket | Kind | Content | Decisions |
|---|---|---|---|
| the data-defined component schema format (Roadmap Stage 1 item 13) | decision | confirms `Zenith2/Docs/Architecture/DataDefinedComponents.md` and this design's catalogue, lock file and blob format; appends the confirmation to `Zenith2/Docs/DecisionLog.md` with its own explicit decision ID | Z2-D-020 |
| the ECS, job and Luau lifetime specifications (item 6), ECS part | decision | confirms command buffers, epochs, views, change versions and merge versions with JOBS and SCRIPT | Z2-D-051, Z2-D-019 |
| the determinism rules (item 9), ECS part | decision | confirms merge order, spawn IDs at playback, event order, fixed-order trees with SCRIPT | Z2-D-019 |
| D-PHASES refinement (open, Stage 1), ECS as dependency | input | the check order and messages of `Zenith2/Docs/Design/ECS/Scheduling.md` → "Authority and schedule checks" | Z2-D-080 |
| Stage 1 workload scale ticket for W-Dense | `needs-human` ruling, ECS epic | commits `Zenith2/Docs/Research/Workloads/W-Dense.md` and the workload fields ([Z2-N-BUDGET-195]) | Z2-D-012 |
| Stage 1 per-system budget harness for ECS / budget ticket for ECS | harness + `needs-human` ruling | § "Budget rows"; the harness's W-Dense mix runs are, from Stage 2, the native half of the `WDense` research scenario | Z2-D-084 |

### Stage 2

In dependency order; each ticket lands its tests in the same diff ([Z2-N-CONTRACT-410]):

| # | Slice | Kind | Blocked by |
|---|---|---|---|
| 1 | API-only: every ZenithReflect public header with contract blocks, `Zenith_ToString`s, limits; out-of-line functions declared only | api | CORE containers, text, results |
| 2 | registry core: C++ builders, runtime builder, IDs, Freeze validation, indexes, lookup, value-kind hooks, `BindStringTable` | implementation | 1; CORE string interning |
| 3 | layout, hooks, field sets and both codecs, the reflection walk | implementation | 2 |
| 4 | schema front end: validation and catalogue, lock file, `schemas.zreg` reader and writer, fuzz targets | implementation | 3; CORE JSON reader and writer |
| 5 | schema migration, fixtures, C++ migration steps | implementation | 4 |
| 6 | hot-reload window and revisions, `RetireFrame`; behaviour-state type registration at startup step 10 from the cooked script manifest (§ "Behaviour-state type registration") and in the window afterwards; `ResolveReferenceDefaults` | implementation | 2; SCRIPT cooked manifest section (cross-area note E12) |
| 7 | component Luau types (`Zenith_EmitComponentLuauTypes`) and the cook's `luau-types` component part | implementation | 3; ASSET cook skeleton |
| 8 | API-only: every ZenithECS public header and the three factory headers | api | 1; JOBS and GRAPH API-only slices |
| 9 | entity table, container heap, change-table helpers, world registry | implementation | 8 |
| 10 | system registry, authority and schedule checks with messages, reload scopes | implementation | 8, 2 |
| 11 | command buffers, command kinds, merge capacity, merge context, the default merge | implementation | 9, 10 |
| 12 | the custom backend | implementation | 9 |
| 13 | world simulation: `EmitInto`, trampolines, the stage order of merge points with read points, merge versions and the version wrap sweep, playback write commit and structural-playback stamping, sync-point, tick-boundary (with `Zenith_TickBoundaryAccess`) and end-of-tick playback, events, reductions, arbitration, out-of-tick playback, cancellation leftovers, inspector, tick hash | implementation | 11, 12; GRAPH executor and builder |
| 14 | the conformance suite and determinism fixture, run on the custom backend | implementation | 13 |
| 15 | the flecs backend, passing the suite | implementation | 14; flecs manifest entry |
| 16 | the EnTT backend and the dynamic-storage adapter, passing the suite | implementation | 14; EnTT manifest entry |
| 17 | `Zenith2/Tests/Determinism/` integration suite and the world-isolation tests | integration | 15, 16; MOD composition root |
| 18 | benchmarks: `Zenith_Bench_ECS`, `Zenith_Bench_ECSNative`, `Zenith_Bench_ECSWorkload` (including `LuauBehavioursVsChunkSystems`) with smoke runs, and the Stage 2 native W-Dense path of `Zenith_Bench_BudgetsECS` (`Bench_ECSNative_<Backend>_WDense`, the custom half through `ZenithECS_Custom_Tests --bench-native WDense`) | implementation | 15, 16; SHIP manifest second owner (E8, E16); MOD composition root |
| 19 | diagnostics: counters, zones, log categories, `ecs.dump` | implementation | 13 |
| 20 | Stage 2 acceptance contribution: the three conformance suites green with the determinism matrix, sanitizer set, S24 device run | acceptance input | 17, 19 |

Slices 1 and 8 unblock SCRIPT, SCENE, MOD and every module that names Reflect or ECS types; slice 13 unblocks the first playable world.

Three consumer-seam slices complete Stage 2: the entity-handle reflection traits (after slice 3 and the handle header, before the conformance rows that register a `Zenith_EntityHandle` member); and, after slices 13 to 16, each landing on all three backends at once, the in-place container rewrite with the tick-boundary container write, and the play-last command kinds with the playback version. The per-ordinal event writers ride with the events part of slice 13 and the current logical invocation with its system contexts. The requirement index lists them as Z2-R-ECS-177 to Z2-R-ECS-181 (with Z2-R-ECS-121 for the current logical invocation); the Stage 3 edit-world flag is Z2-R-ECS-176.

### API-only slices

Slices 1 and 8 are the API-only tickets: they land every public declaration with its contract block, compile under `HeaderSelfContained` and `ApiContractCheck`, and carry the header-only functions (`Zenith_IsVersionNewer`, the enums' `Zenith_ToString`) with their tests; every subsequent slice implements declarations they fixed. A change to a declaration afterwards is an API change with review ([Z2-N-CONTRACT-491]). The members `Zenith2/Docs/Design/ECS/WorldAPI.md` and `Zenith2/Docs/Design/ECS/SystemAPI.md` mark `consumer seam` are not part of slices 1 and 8: each is declared, with its contract text, by the slice that implements it on every backend (the entity-handle reflection traits, the in-place container rewrite with the tick-boundary container write, the play-last kinds with the playback version, the per-ordinal event writers, and the Stage 3 edit-world flag), so an API-only slice never declares a pure virtual member no backend yet implements.

### Stages 3 to 7

- Stage 3: the focused workload-game ticket for `Games/WorkloadDense` (content per the W-Dense scale ruling; ECS epic); SCRIPT's emitter takes over the full `.d.luau` generation, calling `Zenith_EmitComponentLuauTypes`; the edit-world flag slice (`Zenith_WorldDesc::m_bEditWorld` and `Zenith_World::IsEditWorld` on all three backends, `Zenith2/Docs/Design/ECS/WorldAPI.md` → "The edit-world flag"), an API slice with its one-line implementations, landing before the runtime's tool access sets the flag and before Stage 4's `ApplySchemaRevision` reads it.
- Stage 4: EDIT's schema panel and re-registration flow on the Stage 2 window API; migration coverage for every authoring-document type ([Z2-N-CONTRACT-550]).
- Stage 5: the ECS research report (RESEARCH) from the Stage 2 harnesses; D-SEL-EnTT-SHIPPING's ruling ticket only if the human approves P1.
- Stage 6: migration fixtures in shipping qualification ([Z2-N-CONTRACT-551]); W-Dense qualification on every backend.
- Stage 7: nothing new; replication (NET) consumes per-row change versions.

## Decisions

### Settled decisions

| ID | Governs |
|---|---|
| Z2-D-019 | determinism regardless of workers, chunk layout and scheduling, per backend |
| Z2-D-020 | data-defined component schemas in `*.schema.json`, registered at load, identical to C++ types |
| Z2-D-039 | three backends in Stage 2, the API exports no factory, conformance on all three |
| Z2-D-047 | one contract block per type |
| Z2-D-080 | twelve phases, per-field authority, commands at sync points, default arbitration precedence |
| Z2-D-038 | serialization never depends on C++ layout |
| Z2-D-040, Z2-D-041 | ownership and handles; containers need justification records |
| Z2-D-045, Z2-D-046 | results and asserts; no mutable global state |
| Z2-D-048 | schema changes migrate as authoring documents |
| Z2-D-051 | the transaction boundary is the entity |
| Z2-D-064 | research methodology: native and through the adapter |
| Z2-D-082 | flecs and EnTT pinned and confined to `Internal/` |
| Z2-D-084 | harness plus ruling for the ECS budget rows |
| Z2-D-130 | flagged proposals P1 and P2 recorded, not applied |

### Open decisions

- D-PHASES refinement (open, Stage 1): may amend phase contents or admit chunk systems to more phases; ECS's checks read the phase of each declaration, so an amendment changes a table, not an algorithm.
- D-SCRIPT-TXN refinement (open, Stage 1): may amend first-fit capacity acceptance; `Zenith_MergeCapacity::TryAccept` is the one place that changes.
- D-SEL-HASH (open, Stage 1): the hash behind type IDs, resource keys and the tick hash; a change of hash re-cooks every `schemas.zreg`.
- D-SEL-JSON (open, Stage 1): the JSON backend under the schema reader; no ECS code depends on it.
- D-SEL-HEAP (open, Stage 1): the world heap's backing; no ECS code depends on it.

### Flagged proposal P1

P1 (Z2-D-130; `Zenith2/Docs/Roadmap.md` → "Flagged proposals") proposes scoping EnTT's research claim to C++-registered components and, after the Stage 5 report, ruling whether EnTT stays a shipping backend (D-SEL-EnTT-SHIPPING, a flagged proposal only). This design implements EnTT as a full shipping backend, as the settled Z2-D-039 requires, and assumes nothing about P1's outcome. It adds one piece of evidence, recorded in `C:/dev/Zenith2-drafts/findings/FLAGGED_PROPOSALS.md`: through `Zenith_ECS`, every component — C++-registered or data-defined — reaches the EnTT backend through the dynamic-storage adapter, because the backend library cannot see C++ component types; typed EnTT exists only in the native research harness (`Zenith2/Docs/Design/ECS/Backends.md` → "Adapter cost model").

### Selection decisions ECS owns

ECS owns no named selection decision. Choices made in this design and measured by the benchmarks: the custom chunk size (16 KiB, at most 1,024 rows), the flecs and EnTT synthetic chunk size (256 rows), the EnTT size classes and the adapter owning-group policy.

### Justification records ECS adds

Each is appended to `Zenith2/Docs/DecisionLog.md` with the next free explicit decision ID in the diff of the slice that adds the type (Z2-D-097), in the template of `Zenith2/Docs/Contracts.md` → "Containers and standard library policy":

| Record title | Standard alternative | Improvement claimed | Measured evidence | Acceptance criteria |
|---|---|---|---|---|
| `Custom allocator: Zenith_ContainerHeap` | per-block `Zenith_Heap` allocations | reservation at merge without allocation, bounded per-world memory, deterministic reuse | `Bench_ECS_ContainerChurn` (Zenith_Bench_ECS) at 100,000 container fields | allocation-free acceptance; no slower than the heap baseline at the median |
| `Custom container: Zenith_EntityTable` | `Zenith_SlotTable` from CORE | 28-bit generations with per-world offsets, hierarchy links and persistent-ID index in one record | `Bench_ECS_Churn` resolve and allocate rates | resolve ≤ the slot table's time; identical staleness guarantees |
| `Custom heap wrapper: Zenith_FlecsOsApi` (the flecs exception of Z2-R-ECS-137: flecs's process-wide OS API routed to the root heap, a flecs world accepting no other `m_pxHeap`) | flecs's default OS API (`malloc`/`free`) | every flecs allocation through `Zenith_Heap` (stats, leak report, sanitizer side tables), flecs's abort through `ZENITH_FATAL`, no mutable global state | `Bench_ECSNative_Flecs_ChurnRootHeapOsApi` against `Bench_ECSNative_Flecs_ChurnDefaultOsApi` (`Zenith_Bench_ECSNative`: the churn of `Bench_ECS_Churn` through native flecs under an OS API over `Zenith_GetRootHeap()` laid out as `Zenith_FlecsOsApi`'s, and under flecs's default OS API, each reporting flecs's default-allocator call counts), and `Test_FlecsOsApi_AllocationsFromRootHeap` for a flecs world behind `Zenith_ECS` | zero default-allocator calls in the root-heap run and in the test; the root-heap run no slower than the default run at the median |

Each record is appended with `Status: Provisional` and `Measured evidence: pending — <harness> run recorded by the ruling` ([Z2-N-CONTRACT-278]); the Stage 2 `needs-human` records ruling settles all three in place on the reference hardware. If a record's criteria are not met, the established alternative wins ([Z2-N-CONTRACT-279]).

## Requirement index

Every requirement of `Zenith2/Docs/Requirements/ECS.md` and of its second file `Zenith2/Docs/Requirements/ECS/ConsumerSeams.md` (Z2-R-ECS-176 onward) and the design headings that specify it (paths relative to `Zenith2/Docs/Design/`):

| Requirement | Title | Design headings |
|---|---|---|
| Z2-R-ECS-001 | ZenithReflect library target | ECS.md → "ZenithReflect target"; ECS.md → "Library dependencies" |
| Z2-R-ECS-002 | ZenithECS library target | ECS.md → "ZenithECS target"; ECS.md → "Library dependencies" |
| Z2-R-ECS-003 | Three backend library targets | ECS.md → "ZenithECS_Custom target"; ECS.md → "ZenithECS_Flecs target"; ECS.md → "ZenithECS_EnTT target"; ECS/WorldAPI.md → "Backend factory headers" |
| Z2-R-ECS-004 | Third-party headers confined to backend internals | ECS/Backends.md → "Backend internal files"; ECS.md → "ZenithECS_Flecs target"; ECS.md → "ZenithECS_EnTT target" |
| Z2-R-ECS-005 | Type-erased ECS API | ECS.md → "API rules"; ECS/WorldAPI.md → "Typed convenience wrappers" |
| Z2-R-ECS-006 | Contract blocks on every Reflect and ECS header | ECS.md → "API rules"; ECS/Reflect.md → "Header list"; ECS/WorldAPI.md → "Header list" |
| Z2-R-ECS-007 | Upward notifications through ECS-declared interfaces | ECS.md → "Upward-callback interfaces ECS declares"; ECS/WorldAPI.md → "Zenith_ECSObservers header"; ECS/WorldAPI.md → "Zenith_CommandKinds header" |
| Z2-R-ECS-008 | Type ID from the stable name | ECS/Reflect.md → "Zenith_ReflectIDs header"; ECS/Reflect.md → "Registration algorithm" |
| Z2-R-ECS-009 | Type ID collisions and duplicate names rejected | ECS/Reflect.md → "Freeze validation"; ECS/Schemas.md → "Validation rules catalogue" |
| Z2-R-ECS-010 | Explicit property IDs never reused | ECS/Reflect.md → "Freeze validation"; ECS/Schemas.md → "The schema lock file" |
| Z2-R-ECS-011 | Schema version and one migration step per increment | ECS/Reflect.md → "Freeze validation"; ECS/Reflect.md → "Zenith_Migration header" |
| Z2-R-ECS-012 | Registration is explicit and never static | ECS/Reflect.md → "Registration algorithm"; ECS/Reflect.md → "Lookup structures" |
| Z2-R-ECS-013 | Declarations accepted before the string table is bound | ECS/Reflect.md → "Registration algorithm"; ECS/Reflect.md → "Zenith_TypeRegistry"; ECS.md → "Construction and shutdown" |
| Z2-R-ECS-014 | Configuration schema exposed during declarations | ECS/Reflect.md → "Registration algorithm"; ECS/Reflect.md → "Zenith_TypeRegistry" |
| Z2-R-ECS-015 | Freeze validates the whole registry in one pass | ECS/Reflect.md → "Freeze validation" |
| Z2-R-ECS-016 | Frozen registry is immutable and safe to read concurrently | ECS/Reflect.md → "Revisions and the hot-reload window"; ECS/Reflect.md → "Zenith_TypeRegistry" |
| Z2-R-ECS-017 | Hot-reload window admits only its kinds | ECS/Reflect.md → "Revisions and the hot-reload window" |
| Z2-R-ECS-018 | Revision publication is atomic and failure keeps the old revision | ECS/Reflect.md → "Revisions and the hot-reload window" |
| Z2-R-ECS-019 | Runtime type builder is equivalent to C++ registration | ECS/Reflect.md → "Zenith_RuntimeTypeBuilder"; ECS/Reflect.md → "Registration algorithm" |
| Z2-R-ECS-020 | No consumer branches on type origin | ECS/Reflect.md → "Zenith_TypeInfo header"; ECS.md → "Inspector data" |
| Z2-R-ECS-021 | Computed layout of runtime types | ECS/Reflect.md → "Layout algorithm"; ECS/Reflect.md → "Zenith_TypeLayout header" |
| Z2-R-ECS-022 | C++ lifecycle hooks | ECS/Reflect.md → "C++ hooks"; ECS/Reflect.md → "Zenith_ReflectValueTraits header" |
| Z2-R-ECS-023 | Schema lifecycle hooks and value-kind hooks | ECS/Reflect.md → "Schema hooks"; ECS/Reflect.md → "Value-kind hooks" |
| Z2-R-ECS-024 | Value encodings independent of storage | ECS/Reflect.md → "Zenith_TypeInfo header"; ECS/Reflect.md → "Binary field-set record" |
| Z2-R-ECS-025 | Field-set decoder treats input as untrusted | ECS/Reflect.md → "Binary field-set record"; ECS/Tests.md → "Fuzz targets" |
| Z2-R-ECS-026 | JSON field-set form keyed by property ID | ECS/Reflect.md → "JSON field-set form" |
| Z2-R-ECS-027 | Serialization never copies a layout | ECS/Reflect.md → "Zenith_FieldSet header" |
| Z2-R-ECS-028 | Reflection walk for every consumer | ECS/Reflect.md → "Reflection walk algorithm"; ECS/Reflect.md → "Zenith_ReflectionWalk header" |
| Z2-R-ECS-029 | Path writes clamp declared ranges | ECS/Reflect.md → "Reflection walk algorithm" |
| Z2-R-ECS-030 | Schema format confirmed in Stage 1 | ECS.md → "Stage 1" |
| Z2-R-ECS-031 | One schema front end | ECS/Schemas.md → "Scope of this file"; ECS.md → "Schema authoring" |
| Z2-R-ECS-032 | Validation catalogue with exact messages | ECS/Schemas.md → "Validation rules catalogue"; ECS/Schemas.md → "Zenith_Schema header" |
| Z2-R-ECS-033 | Every schema error reported in one run | ECS/Schemas.md → "Schema validation pipeline"; ECS/Schemas.md → "Cook integration" |
| Z2-R-ECS-034 | Schema limits enforced | ECS/Schemas.md → "Validation rules catalogue" |
| Z2-R-ECS-035 | Unknown keys rejected and extension keys preserved | ECS/Schemas.md → "Validation rules catalogue"; ECS/Schemas.md → "Editor re-registration" |
| Z2-R-ECS-036 | Event schema restrictions | ECS/Schemas.md → "Event schemas" |
| Z2-R-ECS-037 | Entity defaults are null and asset defaults resolve | ECS/Schemas.md → "Default values"; ECS/Reflect.md → "Value-kind hooks"; ECS/Reflect.md → "Schema hooks" |
| Z2-R-ECS-038 | Schema lock file enforced | ECS/Schemas.md → "The schema lock file" |
| Z2-R-ECS-039 | Schema blob writer is deterministic | ECS/Schemas.md → "The schema registry blob format" |
| Z2-R-ECS-040 | Schema blob reader treats input as untrusted | ECS/Schemas.md → "Reading and validating a blob"; ECS/Tests.md → "Fuzz targets" |
| Z2-R-ECS-041 | Schema JSON reader is fuzzed | ECS/Tests.md → "Fuzz targets"; ECS/Schemas.md → "Schema validation pipeline" |
| Z2-R-ECS-042 | Invalid schemas stop the cook and the game | ECS/Schemas.md → "Reading and validating a blob"; ECS.md → "Failure handling" |
| Z2-R-ECS-043 | Migration chain validation | ECS/Schemas.md → "Chain validation" |
| Z2-R-ECS-044 | Migration ops applied in order | ECS/Schemas.md → "Op application" |
| Z2-R-ECS-045 | Failed migration leaves input unchanged | ECS/Schemas.md → "Op application"; ECS/Schemas.md → "Numeric retype conversion" |
| Z2-R-ECS-046 | Numeric retype conversions | ECS/Schemas.md → "Numeric retype conversion" |
| Z2-R-ECS-047 | Newer data opens read-only | ECS/Schemas.md → "Op application" |
| Z2-R-ECS-048 | Unknown fields survive migration | ECS/Schemas.md → "Op application" |
| Z2-R-ECS-049 | Script migration ops through the host | ECS/Schemas.md → "Script ops" |
| Z2-R-ECS-050 | Migration fixtures per version | ECS/Schemas.md → "Migration fixtures" |
| Z2-R-ECS-051 | Component Luau types are deterministic | ECS/Schemas.md → "Luau type generation from schemas" |
| Z2-R-ECS-052 | Component Luau types identical for C++ and schema twins | ECS/Schemas.md → "Luau type generation from schemas" |
| Z2-R-ECS-053 | Cook validates schemas and writes the blob and component types | ECS/Schemas.md → "Cook integration" |
| Z2-R-ECS-054 | Schema types change only between play sessions | ECS/Schemas.md → "Editor re-registration"; ECS/Reflect.md → "Revisions and the hot-reload window" |
| Z2-R-ECS-055 | Edit-world rows follow a schema revision | ECS/Backends.md → "Schema revision in the edit world" |
| Z2-R-ECS-056 | Every property declares an authority | ECS/Reflect.md → "Freeze validation"; ECS/Schemas.md → "Authority and arbitration in schemas" |
| Z2-R-ECS-057 | No authority in the read-only phases | ECS/Scheduling.md → "Check order"; ECS/Reflect.md → "Freeze validation" |
| Z2-R-ECS-058 | Direct writes only in the owning phase | ECS/Scheduling.md → "Check order"; ECS/Scheduling.md → "Messages" |
| Z2-R-ECS-059 | CommandsOnly properties have no direct writer | ECS/Scheduling.md → "Check order" |
| Z2-R-ECS-060 | Two direct writers need an arbitration rule | ECS/Scheduling.md → "Check order"; ECS/Scheduling.md → "Arbitration nodes" |
| Z2-R-ECS-061 | Access and authority checked per property | ECS/Scheduling.md → "Check order" |
| Z2-R-ECS-062 | Schema authority behaves as C++ authority | ECS/Schemas.md → "Authority and arbitration in schemas" |
| Z2-R-ECS-063 | World resources have an owning phase | ECS/Scheduling.md → "Check order"; ECS/Reflect.md → "Freeze validation"; ECS/SystemAPI.md → "Zenith_System header" |
| Z2-R-ECS-064 | Writer sources of arbitrated properties | ECS/Scheduling.md → "Check order" |
| Z2-R-ECS-065 | WeightedBlend only on blendable types | ECS/Reflect.md → "Freeze validation"; ECS/Schemas.md → "Validation rules catalogue" |
| Z2-R-ECS-066 | Every schedule error in one sorted pass | ECS/Scheduling.md → "Check order"; ECS/Scheduling.md → "Messages" |
| Z2-R-ECS-067 | Schedule checks run in Shipping | ECS/Scheduling.md → "Check order" |
| Z2-R-ECS-068 | Appended schedule errors | ECS/Scheduling.md → "Messages"; ECS/Scheduling.md → "Check order" |
| Z2-R-ECS-069 | Queries agree with declared access | ECS/Scheduling.md → "Check order" |
| Z2-R-ECS-070 | Behaviours run in the script phases | ECS/Scheduling.md → "Check order" |
| Z2-R-ECS-071 | Ordering declarations validated | ECS/Scheduling.md → "Check order" |
| Z2-R-ECS-072 | Phases run in fixed order | ECS/Scheduling.md → "Stages and barriers"; ECS.md → "Tick walkthrough" |
| Z2-R-ECS-073 | EmitInto obeys the graph core rules | ECS/Scheduling.md → "Node and resource naming"; ECS.md → "EmitInto rules for the graph core" |
| Z2-R-ECS-074 | EmitInto is deterministic | ECS/Scheduling.md → "EmitInto translation"; ECS.md → "EmitInto rules for the graph core" |
| Z2-R-ECS-075 | Merge nodes form a chain in stage order | ECS/Scheduling.md → "Merge nodes and the merge chain"; ECS/Queries.md → "Stage order of merge points" |
| Z2-R-ECS-076 | Extractors precede the render seal | ECS/Scheduling.md → "Epilogue and extractors" |
| Z2-R-ECS-077 | Disabling a system never recompiles | ECS/Scheduling.md → "Disabled systems" |
| Z2-R-ECS-078 | Registration index is the system order | ECS/Scheduling.md → "System registry internals" |
| Z2-R-ECS-079 | Reload scope failure keeps declarations | ECS/Scheduling.md → "System registry internals" |
| Z2-R-ECS-080 | A failed graph compile is recorded as schedule diagnostics | ECS/Scheduling.md → "System registry internals" |
| Z2-R-ECS-081 | System context grants exactly the declaration | ECS/SystemAPI.md → "Zenith_SystemContext header" |
| Z2-R-ECS-082 | Sub-jobs use their own leaf context | ECS/SystemAPI.md → "Zenith_SystemContext header"; ECS/Scheduling.md → "Commit and collection" |
| Z2-R-ECS-083 | Structural changes only at sync points | ECS/Scheduling.md → "Steps"; ECS.md → "Chunk queries" |
| Z2-R-ECS-084 | Command record format | ECS/Scheduling.md → "Record format" |
| Z2-R-ECS-085 | Playback in merge order | ECS/Scheduling.md → "Steps" |
| Z2-R-ECS-086 | Last property command in merge order wins | ECS/Scheduling.md → "Steps" |
| Z2-R-ECS-087 | Stale and foreign commands dropped | ECS/Scheduling.md → "Steps" |
| Z2-R-ECS-088 | Commands on pending-destroy entities | ECS/Scheduling.md → "Steps" |
| Z2-R-ECS-089 | Add then remove in one playback | ECS/Scheduling.md → "Steps" |
| Z2-R-ECS-090 | Spawn identity assigned at playback in merge order | ECS/Scheduling.md → "Steps"; ECS/Backends.md → "Entity table" |
| Z2-R-ECS-091 | Ticket handles resolve at playback | ECS/WorldAPI.md → "Zenith_CommandBuffer header"; ECS/Scheduling.md → "Steps"; ECS/Scheduling.md → "Property writes during structural playback" |
| Z2-R-ECS-092 | Destroy deferred to the end of the tick with cascade | ECS/Scheduling.md → "Tick boundary and end-of-tick"; ECS/Scheduling.md → "Steps" |
| Z2-R-ECS-093 | Owning phase may overwrite a command | ECS/Scheduling.md → "Steps" |
| Z2-R-ECS-094 | Command buffer caps | ECS/Scheduling.md → "Recording" |
| Z2-R-ECS-095 | Command kind table | ECS/WorldAPI.md → "Zenith_CommandKinds header" |
| Z2-R-ECS-096 | Module commands in merge order at step 7 | ECS/Scheduling.md → "Steps" |
| Z2-R-ECS-097 | Merge-time capacity is first-fit in merge order | ECS/Scheduling.md → "Capacity acceptance"; ECS/SystemAPI.md → "Zenith_MergeContext header" |
| Z2-R-ECS-098 | Commit never allocates or fails | ECS/Backends.md → "Container heap"; ECS/Scheduling.md → "Commit and collection" |
| Z2-R-ECS-099 | Out-of-tick playback | ECS/Scheduling.md → "Out-of-tick playback"; ECS/Queries.md → "Change versions and merge versions" |
| Z2-R-ECS-100 | Structural observers notified in merge order | ECS/WorldAPI.md → "Zenith_ECSObservers header"; ECS/Scheduling.md → "Steps" |
| Z2-R-ECS-101 | Tick boundary step order | ECS/Scheduling.md → "Tick boundary and end-of-tick" |
| Z2-R-ECS-102 | Events delivered in phase nine in merge order | ECS/Scheduling.md → "Events and reductions" |
| Z2-R-ECS-103 | Reductions folded by the fixed-order tree | ECS/Scheduling.md → "Events and reductions" |
| Z2-R-ECS-104 | Arbitration precedence | ECS/Scheduling.md → "Arbitration merge" |
| Z2-R-ECS-105 | Unwritten arbitrated rows keep their value | ECS/Scheduling.md → "Arbitration merge" |
| Z2-R-ECS-106 | Weighted blend arithmetic | ECS/Scheduling.md → "Arbitration merge" |
| Z2-R-ECS-107 | Arbitration independent of worker order | ECS/Scheduling.md → "Arbitration merge" |
| Z2-R-ECS-108 | Writers stage per source | ECS/Backends.md → "Arbitration staging"; ECS/Queries.md → "Views and validation" |
| Z2-R-ECS-109 | Query compilation validates terms | ECS/Queries.md → "Query compilation" |
| Z2-R-ECS-110 | Query caches refresh only at sync points | ECS/Queries.md → "Matched-chunk cache and invalidation" |
| Z2-R-ECS-111 | Storage order is unobservable | ECS/Queries.md → "Chunk iteration contract" |
| Z2-R-ECS-112 | Structural epochs change only with structure | ECS/Scheduling.md → "Steps"; ECS/Backends.md → "Common backend contract" |
| Z2-R-ECS-113 | Views are scoped to their invocation and epoch | ECS/Queries.md → "Views and validation"; ECS/WorldAPI.md → "Zenith_ChunkView header" |
| Z2-R-ECS-114 | Read-only and absent columns in views | ECS/Queries.md → "Query compilation"; ECS/Queries.md → "Views and validation" |
| Z2-R-ECS-115 | Commits never invalidate other views | ECS/Queries.md → "Row change bits and CommitRowChanges" |
| Z2-R-ECS-116 | Change versions advance at merge only | ECS/Queries.md → "Row change bits and CommitRowChanges"; ECS/Queries.md → "Change versions and merge versions"; ECS/Scheduling.md → "Property writes during structural playback"; ECS/Backends.md → "Change tables" |
| Z2-R-ECS-117 | CommitRowChanges commits once per chunk column | ECS/Queries.md → "Row change bits and CommitRowChanges"; ECS/Scheduling.md → "Playback write commit"; ECS/Scheduling.md → "Property writes during structural playback"; ECS/Backends.md → "Playback writes" |
| Z2-R-ECS-118 | Merge versions are precomputed | ECS/Queries.md → "Change versions and merge versions"; ECS/Queries.md → "Stage order of merge points" |
| Z2-R-ECS-119 | Changed-query filtering | ECS/Queries.md → "Change-filtered queries"; ECS/Queries.md → "Stage order of merge points" |
| Z2-R-ECS-120 | Per-row change versions | ECS/Backends.md → "Change tables"; ECS/Queries.md → "Change versions and merge versions" |
| Z2-R-ECS-121 | Own-row marking of written columns | ECS/SystemAPI.md → "The current logical invocation"; ECS/Queries.md → "Own-row and any-row reads" |
| Z2-R-ECS-122 | Per-row restore support | ECS/Backends.md → "Common backend contract"; ECS/Queries.md → "Row change bits and CommitRowChanges" |
| Z2-R-ECS-123 | Entity handle behaviour | ECS/WorldAPI.md → "Zenith_EntityHandle header"; ECS/Backends.md → "Entity table" |
| Z2-R-ECS-124 | Component handle behaviour | ECS/WorldAPI.md → "Zenith_ComponentHandle header" |
| Z2-R-ECS-125 | Entity table allocates the lowest free slot | ECS/Backends.md → "Entity table" |
| Z2-R-ECS-126 | World tags issued round-robin | ECS/Scheduling.md → "World registry algorithm" |
| Z2-R-ECS-127 | Initial generations for reissued tags | ECS/Scheduling.md → "World registry algorithm" |
| Z2-R-ECS-128 | World handles never cross worlds | ECS/Queries.md → "Resolve and random access"; ECS/Tests.md → "Determinism suite" |
| Z2-R-ECS-129 | Worlds tick independently | ECS/Tests.md → "Determinism suite"; ECS.md → "EmitInto rules for the graph core" |
| Z2-R-ECS-130 | Failures stay in their world | ECS/Tests.md → "Determinism suite" |
| Z2-R-ECS-131 | Per-world pause | ECS/WorldAPI.md → "Zenith_WorldSimulation header"; ECS/Scheduling.md → "Tick boundary and end-of-tick" |
| Z2-R-ECS-132 | Custom chunk layout | ECS/Backends.md → "Chunk layout"; ECS/Backends.md → "Archetypes" |
| Z2-R-ECS-133 | Custom structural operations move versions with rows | ECS/Backends.md → "Custom structural operations"; ECS/Backends.md → "Compaction"; ECS/Backends.md → "Change tables" |
| Z2-R-ECS-134 | Compaction at end of tick and on request | ECS/Backends.md → "Compaction" |
| Z2-R-ECS-135 | flecs runtime components with forwarded hooks | ECS/Backends.md → "flecs backend" |
| Z2-R-ECS-136 | flecs slices mirror table moves | ECS/Backends.md → "flecs backend" |
| Z2-R-ECS-137 | flecs allocates from the world heap | ECS/Backends.md → "flecs backend" |
| Z2-R-ECS-138 | EnTT dynamic-storage adapter | ECS/Backends.md → "EnTT backend and the dynamic-storage adapter" |
| Z2-R-ECS-139 | EnTT adapter owning groups | ECS/Backends.md → "Owning groups and views" |
| Z2-R-ECS-140 | EnTT view chunks with row-index tables | ECS/Backends.md → "Owning groups and views" |
| Z2-R-ECS-141 | Data-defined components stored natively and identically | ECS/Backends.md → "Common backend contract"; ECS/Tests.md → "Schema tests" |
| Z2-R-ECS-142 | Asset fields keep reference counts balanced | ECS/Reflect.md → "Schema hooks"; ECS/Backends.md → "Common backend contract" |
| Z2-R-ECS-143 | Container fields own their blocks | ECS/Backends.md → "Container heap" |
| Z2-R-ECS-144 | Factories unwind every failed step | ECS/Backends.md → "Factory construction steps" |
| Z2-R-ECS-145 | Conformance suite on every backend | ECS/Tests.md → "Conformance suite" |
| Z2-R-ECS-146 | Determinism matrix per backend | ECS/Tests.md → "Determinism suite" |
| Z2-R-ECS-147 | Determinism with failures | ECS/Tests.md → "Determinism suite" |
| Z2-R-ECS-148 | World-state hash is layout-independent | ECS/Scheduling.md → "World state hash" |
| Z2-R-ECS-149 | Runtime-level determinism suite | ECS/Tests.md → "Determinism suite" |
| Z2-R-ECS-150 | Stage 2 acceptance of the ECS | ECS.md → "Stage 2"; ECS/Tests.md → "Gate steps per test group" |
| Z2-R-ECS-151 | ECS counters with their kinds | ECS.md → "Counters" |
| Z2-R-ECS-152 | ECS log categories | ECS.md → "Log categories and messages" |
| Z2-R-ECS-153 | ECS dump | ECS.md → "Debug views dumps and telemetry" |
| Z2-R-ECS-154 | Inspector data identical for every origin | ECS.md → "Inspector data"; ECS/WorldAPI.md → "Zenith_WorldInspector header" |
| Z2-R-ECS-155 | Capacity keys are simulation-affecting | ECS.md → "Configuration keys" |
| Z2-R-ECS-156 | Research scenarios through Zenith_ECS | ECS/Tests.md → "Benchmarks and research scenarios" |
| Z2-R-ECS-157 | Native research scenarios | ECS/Tests.md → "Benchmarks and research scenarios"; ECS/Tests.md → "Stage 1 budget harness"; ECS.md → "Native-path measurements of the ECS rows" |
| Z2-R-ECS-158 | EnTT adapter costs reported separately | ECS/Backends.md → "Adapter cost model"; ECS/Tests.md → "Benchmarks and research scenarios" |
| Z2-R-ECS-159 | W-Dense once per backend | ECS/Tests.md → "Benchmarks and research scenarios"; ECS.md → "Native-path measurements of the ECS rows" |
| Z2-R-ECS-160 | W-Dense workload scale ruled in Stage 1 | ECS.md → "Stage 1" |
| Z2-R-ECS-161 | Stage 1 ECS budget harness | ECS/Tests.md → "Stage 1 budget harness"; ECS.md → "Rows ECS owns" |
| Z2-R-ECS-162 | ECS budget rows allocated | ECS.md → "Rows ECS owns" |
| Z2-R-ECS-163 | Playback and storage within their rows | ECS.md → "Rows ECS owns" |
| Z2-R-ECS-164 | WorkloadDense content | ECS.md → "Stages 3 to 7" |
| Z2-R-ECS-165 | Justification records for custom infrastructure | ECS.md → "Justification records ECS adds" |
| Z2-R-ECS-166 | ECS configuration key set | ECS.md → "Configuration keys" |
| Z2-R-ECS-167 | Cancellation leaves every world at a sync-point boundary | ECS.md → "Cancellation"; ECS/Scheduling.md → "Tick boundary and end-of-tick" |
| Z2-R-ECS-168 | Version wrap sweep | ECS/Queries.md → "Version wrap sweep"; ECS/Backends.md → "Change tables" |
| Z2-R-ECS-169 | Tick-boundary handlers change properties only | ECS/WorldAPI.md → "Zenith_ECSObservers header"; ECS/Scheduling.md → "Tick boundary and end-of-tick" |
| Z2-R-ECS-170 | Unschedulable system phases are rejected | ECS/SystemAPI.md → "Zenith_System header"; ECS/Scheduling.md → "Check order" |
| Z2-R-ECS-171 | Views carry only stamped merge versions | ECS/WorldAPI.md → "Zenith_ChunkView header"; ECS/Queries.md → "Views and validation" |
| Z2-R-ECS-172 | Behaviour-state types register at startup step ten | ECS.md → "Behaviour-state type registration"; ECS/Reflect.md → "Registration algorithm"; ECS/Reflect.md → "Revisions and the hot-reload window" |
| Z2-R-ECS-173 | Simulation-state restore resets change tracking | ECS/Queries.md → "Change versions and merge versions"; ECS/Scheduling.md → "Out-of-tick playback"; ECS/WorldAPI.md → "Zenith_WorldSimulation header" |
| Z2-R-ECS-174 | Previous read version for row-level change filters | ECS/SystemAPI.md → "Zenith_SystemContext header"; ECS/Queries.md → "Change-filtered queries"; ECS/Scheduling.md → "Disabled systems" |
| Z2-R-ECS-175 | Merge points per tick bounded | ECS/Queries.md → "Change versions and merge versions"; ECS/SystemAPI.md → "Zenith_System header" |
| Z2-R-ECS-176 | Edit-world flag owned by ECS and set from the world's purpose | ECS/WorldAPI.md → "The edit-world flag"; ECS/WorldAPI.md → "Zenith_World header"; ECS/Backends.md → "Factory construction steps" |
| Z2-R-ECS-177 | In-place container rewrite without a reservation | ECS/WorldAPI.md → "In-place container rewrite"; ECS/WorldAPI.md → "Container writes without a merge reservation"; ECS/Backends.md → "Container heap" |
| Z2-R-ECS-178 | Tick-boundary container writes reserve their own block | ECS/WorldAPI.md → "Tick-boundary container writes"; ECS/WorldAPI.md → "Zenith_ECSObservers header"; ECS/Scheduling.md → "Tick boundary and end-of-tick" |
| Z2-R-ECS-179 | Event writers per logical invocation ordinal | ECS/SystemAPI.md → "Zenith_SystemContext header"; ECS/SystemAPI.md → "The current logical invocation"; ECS/Scheduling.md → "Recording" |
| Z2-R-ECS-180 | Play-last command kinds and the playback version | ECS/WorldAPI.md → "Play-last command kinds"; ECS/WorldAPI.md → "Playback version"; ECS/Scheduling.md → "Steps" |
| Z2-R-ECS-181 | Entity handles as reflected C++ members | ECS/WorldAPI.md → "Zenith_EntityHandle header"; ECS/Reflect.md → "Zenith_ReflectValueTraits header" |

## Reconciliation items

Additions and resolutions this design makes to the specification, each for the owning core document to adopt; until the owner's text lands a worker follows this design, and each is also a row of the cross-area notes. The status column says where each stands: "queued" items are confirmed and wait for the core-doc batch named (the orchestrator's cross-doc queue, items X83 to X87, X122, X123, X147, X155 and X381, applied under the core-doc freeze rule); "conforms" items need no core change.

| # | Item | Specification touched | Status |
|---|---|---|---|
| R1 | Change versions are precomputed merge versions per merge point (`V(t, m) = 1 + (U mod (2^32 − 1))` over the unwrapped counter U, 0 never issued), not a run-time counter incremented at each merge node; a stage's merge points are enumerated in a cost-independent topological order of its declared property hazards and orderings (Kahn's algorithm, ties broken by registration index), and a system's last-seen version is the version of the merge point preceding its node in that order, its own merge's stamp excluded (`Zenith2/Docs/Design/ECS/Queries.md` → "Stage order of merge points", "Change-filtered queries"); `GetChangeVersion` is the latest completed sync point's in a frame and V(t, M) of the current tick out of tick; the tick boundary and out-of-tick playback stamp the versions of `Zenith2/Docs/Design/ECS/Queries.md` → "Change versions and merge versions" item 4; the version wrap sweep bounds every comparison, never causing a missed change. [Z2-N-ARCH-728]'s "no call other than `CommitRowChanges` advances a column's change version" and `Architecture/FramePhases.md` → "Sync points and the tick boundary" step 9's "through `Zenith_ChunkView::CommitRowChanges` alone" admit the playback-version stamps (rows a playback creates, step-6 writes outside the write set, columns a move adds; `Zenith2/Docs/Design/ECS/Scheduling.md` → "Property writes during structural playback") and the relocation rule that keeps a destination column version at least as new as the rows moved in (`Zenith2/Docs/Design/ECS/Backends.md` → "Change tables"), all through `Zenith_WorldPlaybackAccess::SetPlaybackVersion`. The wording X84 needs for [Z2-N-ARCH-723]: "the column's version becomes the precomputed merge version of the writing invocation's merge point; the merge points of a stage are ordered by a deterministic topological order of the stage's declared hazards and orderings that ignores cost hints (ties by registration index), and a `QUERY_TERM_CHANGED` query compares against the version of the merge point immediately preceding its system's node in that order" | `Architecture/ECSModel.md` [Z2-N-ARCH-723], [Z2-N-ARCH-728], table "Sync points and the structural epoch"; `Architecture/FramePhases.md` → "Sync points and the tick boundary" step 9; `Scripting.md` commit steps 5 and 6 | queued (X84, X86, X147) |
| R2 | Every system that records commands, emits events or reduces, or is a script system, has a merge node (the default merge for C++), so all shared capacity is consumed on the merge chain; sync-point step 3 is a checked no-op | [Z2-N-ARCH-692]; `Architecture/FramePhases.md` playback step 3 | queued (X84) |
| R3 | Appended enumerators: `ECS_ERROR_NULL_HANDLE`, `ECS_ERROR_INVALID_DESC`, `ECS_ERROR_VALUE_SIZE`, `ECS_ERROR_PHASE_NOT_SCHEDULABLE`; `SCHEDULE_ERROR_CROSS_ENTITY_READ`, `SCHEDULE_ERROR_SCRIPT_PHASE`, `SCHEDULE_ERROR_INVALID_NAME`; `REFLECT_ERROR_LAYOUT`, `REFLECT_ERROR_BAD_NAME`, `REFLECT_ERROR_OUT_OF_MEMORY`, `REFLECT_ERROR_BAD_DEFAULT`; `SCHEMA_ERROR_REGISTRY_FROZEN`; `PROPERTY_TYPE_ENUM` | ECSModel, FramePhases, Architecture.md, DataDefinedComponents; Contracts' example block already names `ECS_ERROR_NULL_HANDLE` | queued (X84; X147 for `ECS_ERROR_PHASE_NOT_SCHEDULABLE` and `REFLECT_ERROR_BAD_DEFAULT`) |
| R4 | `Zenith_WorldDesc::m_pxFaults` (Contracts' [Z2-N-CONTRACT-225] requires a fault injector in every factory desc) | ECSModel "The backend interface" | queued (X84) |
| R5 | `Zenith_CommandKindDesc`, `Zenith_PfnCommandPlayback` and `Zenith_EpilogueStageDesc` are declared in ZenithECS (layer 3), which calls them; ZenithRuntime includes them; `Zenith_EpilogueStageDesc` gains `m_bRenderExtractor` | `Architecture/CompositionRoot.md` "Module interfaces" | queued (X84) |
| R6 | Initial generation: one above the tag's released highest generation when the tag was issued before ([Z2-N-ARCH-725]), else the creation-counter offset ([Z2-N-CONTRACT-099]) | ECSModel "World registry"; Contracts "Handle kinds" | queued (X84) |
| R7 | Reference defaults: `entity` defaults are null only (an entity persistent ID is world-relative); `asset<K>` defaults take the persistent-ID form of the specification's "Default JSON form" column and are resolved once per process by `Zenith_TypeRegistry::ResolveReferenceDefaults` through `Zenith_ValueKindHooks::m_pfnConstructFromPersistentID` | DataDefinedComponents "The schema file" (default JSON form column), which this design now follows | conforms |
| R8 | The schema lock file `Schemas/schemas.lock.json` is how "the committed previous version" of [Z2-N-ARCH-594] is known | DataDefinedComponents | queued (X85) |
| R9 | Component Luau declarations are emitted by ZenithReflect in Stage 2 in stable-name and field-ID order ([Z2-N-ARCH-611]), against [Z2-N-SCRIPT-621]'s Luau-name order; Architecture is the owner | DataDefinedComponents, Scripting/Bindings.md | queued (X86, settling X70) |
| R10 | Script systems and behaviours register through `Zenith_SystemRegistry` (as `Architecture/CompositionRoot.md` startup step 17 says), not through `Zenith_ScriptWorld::RegisterScriptNodes` on the builder, so one check covers every system | Scripting/LibraryLayout.md | queued (X86) |
| R11 | Behaviour-state types register at startup step 10, after the schema blob and before `Freeze`, from the cooked script manifest, and afterwards only inside the hot-reload window (§ "Behaviour-state type registration"); [Z2-N-SCRIPT-382]'s "at world load before the registry freezes" cannot hold, because worlds load at step 17 | Scripting.md [Z2-N-SCRIPT-382], CompositionRoot | queued (X86) |
| R12 | The parent relation of entities is ECS's (the destroy cascade of [Z2-N-ARCH-549] is ECS playback); SCENE's hierarchy reads it | LifecycleCommands "Destroy" | queued (X84) |
| R13 | `Zenith_WorldPlaybackAccess` (including the playback-write members and `SetPlaybackVersion`), `Zenith_MergeContext` (including `GetReadVersion`), `Zenith_World` (including `SweepChangeVersions`), `Zenith_SystemContext` (including `GetPreviousReadVersion`), `Zenith_SystemRegistry` (including `GetRejectedRegistrations`), `Zenith_CommandBuffer` and `Zenith_WorldSimulation` gain the members marked elaboration in `Zenith2/Docs/Design/ECS/WorldAPI.md` and `Zenith2/Docs/Design/ECS/SystemAPI.md`; ECSModel adopts them by reference | ECSModel | queued (X84, X147) |
| R14 | World resources own a phase 1 … 11: a resource owning phase 11 is a snapshot buffer, the one write phase 11 admits ([Z2-N-ARCH-503]); no resource owns 12; `AddWorldResource`'s phase equals the type's | FramePhases [Z2-N-ARCH-476], [Z2-N-ARCH-488] | queued (X84) |
| R15 | `AddSystem` rejects phase 12, `PHASE_COMMANDS_ONLY` and larger values with `ECS_ERROR_PHASE_NOT_SCHEDULABLE`; phase-12 work is EPILOGUE work only | FramePhases, CPUFrameGraph "Frame structure", [Z2-N-ARCH-502] | queued (X84) |
| R16 | Counter names and kinds follow Contracts [Z2-N-CONTRACT-155] and [Z2-N-CONTRACT-156] (`_delta` names, `eKind`), with gauges published as `COUNTER_KIND_GAUGE`, against [Z2-N-ARCH-713]'s suffix-free names; per-tick amounts, zone-duration sums included, are deltas (`ecs.playback_us_delta`) | Architecture.md [Z2-N-ARCH-713]; Contracts `Zenith_CounterKind` | queued (X83, X87, X147) |
| R17 | A paused world's tick-boundary node performs none of steps 1 to 4 and publishes nothing, results queued while it is paused publishing in completion-ID order at the first boundary after it resumes; tick-boundary handlers write properties through the property-only `Zenith_TickBoundaryAccess`, committed after step 3 at V(t, 0) with no structural change (`Zenith2/Docs/Design/ECS/Scheduling.md` → "Tick boundary and end-of-tick") | `Architecture/FramePhases.md` [Z2-N-ARCH-505]; `Scripting.md` [Z2-N-SCRIPT-335]; `Architecture/LifecycleCommands.md` "World transition" | queued (X123) |
| R18 | The merge nodes of a stage are chained in the stage's cost-independent topological order (Kahn over the stage's hazard and ordering edges, the chain itself excluded, ties by registration index), the order ECS already uses for read versions, instead of registration order; a merge-node reader registered before the merge-node writer of a property it reads therefore schedules, capacity is consumed in chain order and playback stays in merge-key order (`Zenith2/Docs/Design/ECS/Scheduling.md` → "Merge nodes and the merge chain"; the orchestrator's ruling (b) on Q49) | [Z2-N-ARCH-692]; [Z2-N-SCRIPT-249]; [Z2-N-ARCH-481]; `Architecture/CPUFrameGraph.md` merge-chain wording | queued (X155) |
| R19 | Consumer seams: `Zenith_WorldDesc::m_bEditWorld` with `Zenith_World::IsEditWorld`, set by the world's creator from its purpose (`Zenith2/Docs/Design/ECS/WorldAPI.md` → "The edit-world flag"); `Zenith_CommandKindDesc::m_uFlags` with `COMMAND_KIND_FLAG_PLAY_LAST`, whose records play after every other step-7 record (→ "Play-last command kinds"), the one exception to step 7's merge order; `Zenith_TickBoundaryAccess::SetContainer`, a property write that reserves its own block (→ "Tick-boundary container writes"); `Zenith_WorldPlaybackAccess::RewriteContainer` and `GetPlaybackVersion`, and `Zenith_SystemContext::Events(sim, ordinal)`, which [Z2-N-ARCH-732] already adopts by reference as members marked elaboration | `Architecture/ECSModel.md` → "The backend interface" (`Zenith_WorldDesc`) and [Z2-N-ARCH-729] (the member list of `Zenith_TickBoundaryAccess`); `Architecture/CompositionRoot.md` (`Zenith_CommandKindDesc`); `Architecture/FramePhases.md` [Z2-N-ARCH-492] and `Architecture/LifecycleCommands.md` [Z2-N-ARCH-542] ("in merge order" at step 7) | queued (X381) |
