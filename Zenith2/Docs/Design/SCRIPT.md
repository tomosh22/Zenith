# SCRIPT design — Scripting

The design of the SCRIPT area: the Luau runtime `ZenithScript` and its host-only compiler `ZenithScriptCompiler` — compilation and the cooked `.zlbc` bytecode, module loading, generated bindings and `.d.luau`, the DAP debugger, sandboxing, per-invocation transactions, stateless-system enforcement, the explicit behaviour-state model with `OnRecover`, quarantine and last-good restore, hot reload and migration, per-worker VMs, deterministic ordering, GC/CPU/memory budgets, script profiling, the exception firewall and shipping bytecode compatibility. The specification is `Zenith2/Docs/Scripting.md` with its companions `Zenith2/Docs/Scripting/{Bindings,PublicTypes,MisuseCatalogue,LibraryLayout,StatelessCorpus,TransactionStructures,BudgetRows}.md`; this design cites them heavily and adds only what a worker still lacks — file layout, the complete API index, internal data structures, step-by-step procedures, frame-graph placement, error codes, tests, budgets, stages and decisions. It never restates a specification bullet; where it adds or reconciles a detail it says so and lists it under § "Reconciliation items". The requirement catalogue is `Zenith2/Docs/Requirements/SCRIPT.md`.

This entry document holds the skeleton and the top-level contracts. Eight split files hold the rest:

| File | Holds | Its `##` headings |
|---|---|---|
| `Zenith2/Docs/Design/SCRIPT/PublicAPI.md` | every public header with declarations and contract blocks | Header list · Zenith_ScriptTypes header · Zenith_ScriptError header · Zenith_ScriptRecovery header · Zenith_ScriptWorld header · Zenith_ScriptRuntime header · Zenith_ScriptHost header · Zenith_ScriptAsync header · Zenith_ScriptStateSnapshot header · Zenith_ScriptBehaviourComponent header · Zenith_ScriptCompileOptions header · Zenith_ScriptTypesEmitter header · Zenith_ScriptBinding header · Zenith_ScriptCompile header · Zenith_ScriptStateless header · Zenith_ScriptCook header |
| `Zenith2/Docs/Design/SCRIPT/Runtime.md` | VMs, protected call, hooks, watchdog, GC, script nodes, instances, tick boundary, async results, random, halt, profiling | Scope of this file · Script runtime internals · Script world internals · VM pool per world · VM creation steps · Protected call implementation · Allocator hook implementation · Interrupt callback implementation · Watchdog internals · Garbage collection scheduling · Node registration and frame-graph placement · Chunk-system node body · Behaviour node body · Behaviour instance directory · Tick-boundary handler · Asynchronous result table · Random draws · World halt procedure · Profiling implementation · Runtime internal files |
| `Zenith2/Docs/Design/SCRIPT/Transactions.md` | the transaction end to end | Scope of this file · Transaction arena partition · Retention ceiling derivation · Invocation lifecycle · Invocation layout in the arenas · Write path implementation · Snapshot capture and restore · Copy-on-write shadow implementation · Side-effect routing · Per-entity accounting implementation · Merge node body · Capacity acceptance across ECS and SCRIPT classes · Commit implementation · Failure handling by kind · Quarantine store · Diagnostic store implementation · Worked lifecycle traces |
| `Zenith2/Docs/Design/SCRIPT/Bindings.md` | bindings, views, environments, the loader, generated types | Scope of this file · Binding table implementation · Per-VM thunk tables · Dispatcher implementation · Userdata tags and view objects · Marshalling implementation · Environment construction · Module loader implementation · The @zenith prelude and @types modules · Definition emitter procedure · Side-effect consumer registration · Low-library binding files |
| `Zenith2/Docs/Design/SCRIPT/Compiler.md` | the compiler, the cook, the formats, the stateless prototype, the lint | Scope of this file · Compiler library internals · Compile procedure · Compiler guard implementation · Fast-call listing · Cook pipeline · Cooked bytecode container writer · Module manifest format · Declaration extraction · Stateless check prototype · Stateless alternatives prototypes · Address-keyed iteration lint · Compile diagnostics report |
| `Zenith2/Docs/Design/SCRIPT/Debugger.md` | the DAP server and the sampling profiler | Scope of this file · DAP service ownership and lifetime · Transport and framing · Session state machine · Request handling · Breakpoint storage and fan-out · Stop protocol · Stop loop · Stepping · Variables and scopes · Evaluate restrictions · Debugger and simulation dispatch · Attaching on Windows and the S24 · Sampling profiler implementation · Debugger internal files |
| `Zenith2/Docs/Design/SCRIPT/HotReload.md` | state, checkpoints, recovery, hot reload, migration, saves | Scope of this file · State schema internals · State registration at startup · State proxy implementation · Checkpoint serializer · Checkpoint slot pools · Restore implementation · Quarantine and recovery scheduling · Last-good restore · Hot-reload staging area · ApplyPendingReloads procedure · Schedule commit in the prologue · Go-live at the tick boundary · Rollback procedure · Chunk-system generations · Migration execution · Schema migration script host · Behaviour state in saves · Development reload sources |
| `Zenith2/Docs/Design/SCRIPT/Tests.md` | every test, fuzz target and benchmark | Test files and targets · Firewall and VM tests · Transaction and merge tests · Determinism and fault-matrix tests · State recovery and reload tests · Module binding and sandbox tests · Compiler and cook tests · Debugger and profiling tests · Fuzz targets · Benchmarks and harnesses · Gate steps and labels |

`<!-- assumption: pending <AREA> -->` marks an interface SCRIPT assumes from another area's design; each is a row of `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md` (rows SC1 onward; SCENE's rows carry the prefix S, of which SCRIPT answers S10, S21 and S22). `<!-- assumption: pending core-doc (Xn) -->` marks a declaration or rule of the core documents (Scripting.md and its companions, Contracts.md, Architecture, Budgets.md) that this design changes and that core-doc row Xn of `C:/dev/Zenith2-drafts/reviews/CROSS_DOC_ITEMS.md` applies; until the row lands the design's form is the proposal and the core form is what is signed off, so a worker implementing a fenced item cites both.

## Purpose and scope

### What SCRIPT owns

| Piece | Where | Specified in |
|---|---|---|
| the exception firewall: the two `_EH` compile groups, the protected entry, the compiler guard, the ForbiddenCheck rules' subject matter | `ZenithScript/Internal/Zenith_ScriptProtectedCall.cpp`, `ZenithScriptCompiler/Internal/Zenith_ScriptCompilerGuard.cpp` | Scripting.md → "Luau integration and the exception firewall"; Design/SCRIPT/Runtime.md → "Protected call implementation" |
| VMs: one main VM per world, one worker VM per (frame thread, world), memory caps, GC, the watchdog | `Zenith_ScriptVM`, `Zenith_ScriptWatchdog` | Scripting.md → "VM model and threading"; Runtime.md |
| compilation, the `.zlbc` container, the module manifest, the cook pipeline, declaration extraction, the address-keyed lint | `ZenithScriptCompiler` | Scripting.md → "Shipping bytecode compatibility"; Design/SCRIPT/Compiler.md |
| module loading, environments and sandboxing | `Internal/Zenith_ScriptModuleLoader.cpp`, `Internal/Zenith_ScriptSandbox.cpp` | Scripting/Bindings.md → "Module loader", "Environments and library subsets"; Design/SCRIPT/Bindings.md |
| the binding ABI (a SCRIPT-owned file of ZenithReflect), the dispatcher, marshalling, views | `ZenithReflect/Zenith_ScriptBinding.h`, `Internal/Zenith_ScriptBindingDispatch.cpp`, `Internal/Zenith_ScriptViews.cpp` | Scripting/Bindings.md; Design/SCRIPT/Bindings.md |
| `.d.luau` and the generated `@zenith`/`@types` modules (the component and event sections are ZenithReflect's emitter, called by SCRIPT) | `Zenith_ScriptEmitDefinitions` | Scripting/Bindings.md → "Luau type generation"; Design/SCRIPT/Bindings.md → "Definition emitter procedure" |
| behaviours and chunk systems, their registration as ECS systems, the instance directory | `Internal/Zenith_ScriptChunkRunner.cpp`, `Internal/Zenith_ScriptBehaviourRunner.cpp` | Scripting.md → "Behaviours and chunk systems"; Runtime.md |
| per-invocation transactions, merge, commit, capacity, quarantine | `Internal/Zenith_ScriptTransaction.*`, `Internal/Zenith_ScriptMerge.cpp` | Scripting.md → "Transaction scope and the entity boundary" … "Merge-time capacity and asynchronous results"; Design/SCRIPT/Transactions.md |
| deterministic ordering, counter-based randomness | the merge, `Internal/Zenith_ScriptRandom.*` | Scripting.md → "Determinism rules and randomness" |
| stateless enforcement (D-LUAU-STATELESS, open, Stage 1) with its corpus | `Zenith_ScriptCheckStateless`, `ZenithScript/Tests/Corpus/Stateless/` | Scripting.md → "Stateless system enforcement"; Scripting/StatelessCorpus.md; Compiler.md |
| behaviour state, checkpoints, `OnRecover`, recovery, hot reload, migration, the schema migration script host, behaviour state in saves | `Internal/Zenith_ScriptState.cpp`, `Internal/Zenith_ScriptRecovery.cpp` | Scripting.md → "Behaviour state model and recovery", "Quarantine and hot reload"; Design/SCRIPT/HotReload.md |
| asynchronous results: completion IDs, the per-world result table, tick-boundary publication, replay publication | `Zenith_AsyncResultTable` | Scripting.md → "Merge-time capacity and asynchronous results"; Runtime.md → "Asynchronous result table" |
| the DAP server and the sampling profiler (development builds) | `ZenithScript/Internal/Debug/` | Scripting.md → "Debugging and profiling"; Design/SCRIPT/Debugger.md |
| profiling zones and counters for scripts | `Internal/Zenith_ScriptProfiling.*` | Scripting.md → "Profiling zones", "On-screen profiler rows" |
| the `Zenith.Behaviour` component | `Zenith_BehaviourComponent` | Design/SCRIPT/PublicAPI.md → "Zenith_ScriptBehaviourComponent header" |

### What SCRIPT does not own

| Not owned | Owner | Why |
|---|---|---|
| chunks, columns, views, change versions, command buffers, playback, merge capacity of the ECS classes, the system registry, reductions' fold | ECS (`Zenith2/Docs/Design/ECS.md`) | SCRIPT uses them through `Zenith_SystemContext` and `Zenith_MergeContext` |
| the frame graph, schedule compilation, the frame executor, `Zenith_FixedOrderReduce` | GRAPH, JOBS | SCRIPT registers systems; ECS translates them (`Zenith2/Docs/Design/GRAPH.md` → "The JOBS boundary") |
| frame phases, per-field authority, lifecycle commands, arbitration | GAME (Z2-D-080; D-PHASES refinement, open, Stage 1) | Architecture.md → "Frame phases and authority" |
| the replay log format (`ZRPL`) and save files | SAVE (`Engine/ZenithSave/Zenith_ReplayLog.h`) | SCRIPT hands publications and checkpoint blobs to the runtime's recorder and SAVE's snapshot |
| the composition root, `game.json`, startup and shutdown order, the frame loop, the hot-reload window's opening | MOD (`Zenith2/Docs/Design/MOD.md`) | SCRIPT is created at startup step 14 and called in prologue step 6 |
| the script editor, script panels, the play-in-editor workflow | EDIT | SCRIPT exposes compile, reload, console and diagnostics APIs (§ "Authoring and cooking") |
| the on-screen profiler overlay rendering | DEVT (Z2-D-005) | SCRIPT publishes zones and counters |
| the development listener and file watcher | PLAT (`Zenith2/Docs/Design/PLAT/Platform.md` → "Development services") | the DAP server and reload sources use them |
| the `Input` binding functions, platform event payloads | PLAT (`Zenith2/Docs/Design/PLAT/SimulationInput.md` → "Script-facing input API") | SCRIPT owns the ABI and the misuse text |
| module bindings (physics, audio, AI, UI, …) and their side-effect consumers | each module | they register through the ABI ([Z2-N-SCRIPT-109]) |
| quest, dialogue, inventory and interaction | OpenWorldSlice, game-side Luau (Z2-D-077) | never engine features |
| third-party scripts, mods | excluded (Z2-D-075) | no path loads untrusted code |

### Inventory items covered

SCRIPT covers every item of `Zenith2/Docs/Requirements/Inventory.md` → "Inventory v1", section "SCRIPT Scripting": `Z2-I-SCRIPT-01` (Luau compilation) … `Z2-I-SCRIPT-22` (the exception firewall), all required in Stage 3 (the lifetime specifications, the firewall's linkage check, the D-LUAU-STATELESS prototype and corpus, D-LUAUDBG and the native parity harness in Stage 1), and `Z2-I-SCRIPT-23` (shipping bytecode version compatibility), required in Stage 6. The data-defined part of `Z2-I-SCRIPT-05` (component `.d.luau`) lands in Stage 2 in ECS (cross-area note E6(f)); SCRIPT's requirement for it is the Stage 3 composition. The mapping to requirements is `Zenith2/Docs/Requirements/SCRIPT.md` → "Coverage of inventory items".

### Relationship to the specification

Scripting.md is signed off and unusually detailed: it declares the protected entry, the VM, the transaction structures, the misuse catalogue, the corpus, the formats and every required test. This design therefore adds only: the file map at file granularity; the full public API index and missing contract blocks; the data structures Scripting.md names without laying out (the VM pool, thunk tables, the transaction arena partition, the quarantine store, the staging area, the async result table, the diagnostic store); procedures for rules Scripting.md states without steps (world and VM creation, node registration, the behaviour node, the merge node over ECS's merge context, go-live, cook, the stateless pass, DAP request handling); and reconciliations where Scripting.md and a subsequent ruling or another area's design disagree (§ "Reconciliation items", S1 to S16).

### Document family

This file and the eight split files above. The API declarations of Scripting's companions are authoritative; `Design/SCRIPT/PublicAPI.md` adds only contract blocks and the members marked `// elaboration`, and a change to a declaration is edited in both in one diff ([Z2-N-SCRIPT-901]).

## Dependencies

### Library dependencies

| Library | Layer | `DEPENDS` (exactly `Zenith2/Docs/Architecture.md` → "Library dependency graph") | Third party |
|---|---|---|---|
| `ZenithScript` | 5 | ZenithCore, ZenithReflect, ZenithECS, ZenithScene, ZenithAssets, ZenithJobs, ZenithPlatform, ZenithProfiling | `Luau.VM`, `Luau.CodeGen` |
| `ZenithScriptCompiler` | 6 | ZenithScript, ZenithReflect, ZenithCore | `Luau.Ast`, `Luau.Compiler` |

What each edge is for: ZenithCore — results, owned pointers, the heap, arenas, pools, containers, `Zenith_Span`, `Zenith_StringView`, `Zenith_StringID`, `Zenith_Hash64`, the random derivations, logging, the JSON reader and writer, threads and locks, fault injection, `Zenith_IsShippingBuild`; ZenithReflect — the binding ABI (SCRIPT's file), the type registry, runtime type registration, `Zenith_EmitComponentLuauTypes`, `Zenith_MigrationScriptHost`; ZenithECS — systems, contexts, chunk views, command buffers, merge contexts, tick-boundary handlers, entity-handle traits; ZenithScene — prefab references for `ctx:Spawn`; ZenithAssets — asset handles and the cooked bytecode's raw reads; ZenithJobs — `Zenith_JobContext`, `ParallelFor`, the frame-thread count; ZenithPlatform — the development listener, the virtual filesystem; ZenithProfiling — zones and counters. Nothing ZenithScript depends on, and no library owning bindings, depends on ZenithScript ([Z2-N-SCRIPT-881]).

### Headings this design relies on

| Heading | For |
|---|---|
| `Zenith2/Docs/Scripting.md` (every `##`), `Zenith2/Docs/Scripting/Bindings.md`, `PublicTypes.md`, `TransactionStructures.md`, `MisuseCatalogue.md`, `StatelessCorpus.md`, `LibraryLayout.md`, `BudgetRows.md` | the specification |
| `Zenith2/Docs/Design/ECS/WorldAPI.md` → "Zenith_ChunkView header", "Zenith_ECSObservers header", "Zenith_WorldSimulation header", "Zenith_ECSScriptTraits header"; `Zenith2/Docs/Design/ECS/SystemAPI.md` → "Zenith_System header", "Zenith_SystemContext header", "Zenith_MergeContext header" | the ECS surface scripts run on |
| `Zenith2/Docs/Design/ECS/Scheduling.md` → "Merge nodes and the merge chain", "Capacity acceptance", "Tick boundary and end-of-tick", "Events and reductions" | merge placement, capacity, reductions |
| `Zenith2/Docs/Design/ECS/Reflect.md` → "Zenith_RuntimeTypeBuilder", "Revisions and the hot-reload window" | behaviour-state types, the window |
| `Zenith2/Docs/Design/ECS/Schemas.md` → "Luau type generation from schemas", "Script ops" | component `.d.luau`, the migration host |
| `Zenith2/Docs/Design/JOBS.md` → "Per-worker Luau VM affinity contract"; `Zenith2/Docs/Design/JOBS/PublicAPI.md` → "Zenith_JobContext header", "Zenith_FloatEnvironment header" | VM slots, `ParallelFor`, the FP environment |
| `Zenith2/Docs/Design/GRAPH.md` → "Zenith_FixedOrderReduce.h", "Front-end error mapping"; `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Cache invalidation triggers" | reductions, schedule errors, recompiles |
| `Zenith2/Docs/Design/MOD.md` → "Startup sequence with unwinding", "Step construction notes", "Zenith_GameComposition.h", "Frame loop driver"; `Zenith2/Docs/Design/MOD/GameBuild.md` → "Keys" | creation, the window, `scriptRecovery` |
| `Zenith2/Docs/Design/PLAT/Platform.md` → "Loopback socket", "File watcher"; `Zenith2/Docs/Design/PLAT/SimulationInput.md` → "Script-facing input API"; `Zenith2/Docs/Design/PLAT.md` → "Frame-graph placement" | DAP transport, reload sources, input bindings, platform publications |
| `Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_RandomSource", "Zenith_Hash64"; `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_BoundedQueue", "Zenith_BitArray"; `Zenith2/Docs/Design/CORE/Text.md` → "String interning", "JSON reader", "JSON writer"; `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Structured logging", "Results and the error taxonomy"; `Zenith2/Docs/Design/CORE/Memory.md` → "Fault-injection support" | the Core types |
| `Zenith2/Docs/Contracts.md` → "Errors and results", "Contract blocks per type", "Threading and concurrency contract", "No mutable global state"; `Zenith2/Docs/Contracts/Memory.md` → "Fallible construction", "Zenith_Pool" | contracts every type obeys |
| `Zenith2/Docs/Architecture/BuildSystem.md` → "The zenith_add_library helper"; `Zenith2/Docs/Architecture/CPUFrameGraph.md` → "Frame structure"; `Zenith2/Docs/Architecture/FramePhases.md` → "Sync points and the tick boundary"; `Zenith2/Docs/Architecture.md` → "Multiplayer foundations", "Memory governor", "Security architecture" | build, prologue, tick boundary, ZRPL enumerations, governor, trust |
| `Zenith2/Docs/Budgets.md` → "Scripting rows" | the rows of § "Budget rows" |

### Types used from other areas

`Zenith_Result`, `Zenith_Owned`, `Zenith_Heap`, `Zenith_Arena`, `Zenith_ArenaDesc`, `Zenith_ArenaMarker`, `Zenith_Pool`, `Zenith_Array`, `Zenith_FixedArray`, `Zenith_HashMap`, `Zenith_BitArray`, `Zenith_BoundedQueue`, `Zenith_Span`, `Zenith_ByteSpan`, `Zenith_MutableByteSpan`, `Zenith_StringView`, `Zenith_StringID`, `Zenith_StringTable`, `Zenith_Hash64`, `Zenith_PersistentID`, `Zenith_ReentrancyGuard`, `Zenith_FaultInjector`, `Zenith_Thread`, `Zenith_Mutex`, `Zenith_ConditionVariable`, `Zenith_MarkThreadAsJobWorker`, `Zenith_LogCategory`, `Zenith_ConfigSchema`, `Zenith_BuildPlatform` (CORE); `Zenith_TypeID`, `Zenith_PropertyID`, `Zenith_TypeRegistry`, `Zenith_RuntimeTypeBuilder`, `Zenith_MigrationScriptHost` (ECS/Reflect); `Zenith_World`, `Zenith_WorldSimulation`, `Zenith_SystemRegistry`, `Zenith_SystemDesc`, `Zenith_SystemContext`, `Zenith_MergeContext`, `Zenith_MergeCapacity`, `Zenith_ChunkView`, `Zenith_RawColumn`, `Zenith_InvocationScope`, `Zenith_CommandBuffer`, `Zenith_EntityHandle`, `Zenith_SpawnTicket`, `Zenith_ContainerRef`, `Zenith_TickBoundaryHandler`, `Zenith_TickBoundaryAccess` (ECS). SCRIPT uses no concurrent container: its cross-thread hand-offs are graph edges, the per-entry atomic flag of Design/SCRIPT/Transactions.md → "Transaction arena partition" and the per-slot atomics of Design/SCRIPT/Transactions.md → "Diagnostic store implementation", each with its memory orders stated ([Z2-N-CONTRACT-312]); `Zenith_JobContext`, `Zenith_JobSystem` (JOBS); `Zenith_DevListener`, `Zenith_FileWatcher` (PLAT); `Zenith_AssetHandle<T>` (ASSET).

### Upward-callback interfaces SCRIPT declares

Every upward notification from ZenithScript goes through an interface ZenithScript (or its ABI file in ZenithReflect) declares and a higher library implements ([Z2-N-CONTRACT-175]):

| Interface | Declared in | Implemented by | Called |
|---|---|---|---|
| `Zenith_ScriptWorldListener` | `ZenithScript/Zenith_ScriptHost.h` | ZenithRuntime | world halt, module disabled, module recovered |
| `Zenith_ScriptPublicationObserver` | same | the runtime's replay recorder (over ZenithSave) | tick-boundary publication |
| `Zenith_ScriptPublicationSource` | same | the runtime's replay reader | replay publication, `CanRunTick` |
| `Zenith_AsyncPublicationHandler` | same | the PLAT runtime bridge (ZenithRuntime) | publication of `ASYNC_SOURCE_PLATFORM` results |
| `Zenith_ScriptSideEffectConsumer` | `ZenithReflect/Zenith_ScriptBinding.h` | the audio and VFX modules, ZenithRuntime (saves, async multiplexer) | commit of SCRIPT-held side effects |
| `Zenith_ScriptInvocationContext` | same | ZenithScript itself (downward use by thunks) | from binding thunks |

### Interfaces SCRIPT implements for other areas

| Interface | Declared by | SCRIPT's implementation |
|---|---|---|
| `Zenith_TickBoundaryHandler` | ECS (`Zenith_ECSObservers.h`) | `Zenith_ScriptTickBoundaryAdapter` (Runtime.md → "Tick-boundary handler") |
| `Zenith_PfnSystemRun`, `Zenith_PfnSystemMerge` | ECS (`Zenith_System.h`) | the chunk, behaviour and merge node bodies |
| `Zenith_MigrationScriptHost` | ZenithReflect (`Zenith_SchemaMigrator.h`) | `Zenith_ScriptMigrationHost` (HotReload.md → "Schema migration script host") |
| `Zenith_MemoryConsumer` | CORE's governor | the script runtime's resident total under `MEMORY_SUBSYSTEM_SCRIPTS` |

### Consumers and what they call

| Consumer | Calls |
|---|---|
| MOD (composition root) | `Zenith_RegisterScriptConfigKeys` and `Zenith_ScriptRegisterEngineTypes` (step 6), `Zenith_ScriptModuleManifest::Load` and `Zenith_ScriptRegisterBehaviourStateTypes` (step 10), `Zenith_ScriptBindingTable` construction and `Seal` (steps 14–15), `Zenith_ScriptWatchdog::Create`, `Zenith_ScriptRuntime::Create` (step 14), `CreateWorld`, `LoadModules`, `RegisterScriptNodes` (steps 16–17), `HasPendingReloads`/`ApplyPendingReloads` and, in every frame, `CommitPendingReloads` (prologue step 6; cross-area row SC24), `PollDebugger`, `CanRunTick`, `DestroyWorld`, the host interfaces |
| every module | `Zenith_ScriptBindingTable::AddFunction`, `SetSideEffectConsumer`, `DeclareAsyncSource` (through its start context) |
| SCENE | `SetAsyncResultDecoder(8, …)` with its `Scene` bindings, before `Seal`; through ZenithRuntime's `Zenith_ScenePublicationSink`, `DeclareSource(8, scene.maxAdditiveInstances, 8)`, `SetHandler`, `AllocateCompletionID` and `Complete` (cross-area row S22) |
| ECS | nothing directly: script systems are ordinary registrations; ECS calls the node bodies and the tick-boundary adapter |
| SAVE | `GetStateSnapshot` (phase 11), `RestoreSavedStates`, `GetAsyncResults().RestoreNextCompletionID`, `ListUnpublished`, `ListInFlight`, `RestoreUnpublished`, `RestoreInFlight`, the phase-11 `AllocateCompletionID(ASYNC_SOURCE_SAVE)`, `AppendCommittedStateHash` and the publication source's `GetRecordedNextCompletionID` (cross-area rows SV3, SaB7, SaB16, SaB22) |
| GAME (world host) | `DetachAsyncResults`, `Retire`, `GetInFlightCount`, `GetDiscardedCount` (row GA10 (a)) |
| MOD (module publication sink) | `DeclareSource`, `SetHandler`, `SetPublicationResolver`, `AllocateCompletionID`, `Complete`, `IsRetired`, `GetSourceInFlightCount` (`Zenith2/Docs/Design/MOD/RuntimeSeams.md` → "Module publication sink") |
| PLAT (runtime bridge) | `GetAsyncResults().AllocateCompletionID`, `Complete`, `SetHandler` |
| EDIT | `Zenith_ScriptCompileModule`, `Zenith_ScriptCheckStateless`, `RequestHotReload`, `RequestModuleReload`, `ConsoleEvaluate`, `GetDiagnostic`, `Zenith_ScriptEmitDefinitions` |
| ASSET (the cook) | `Zenith_ScriptCookGame`, `Zenith_ScriptEmitDefinitions` |
| GAME, OpenWorldSlice | Luau alone |

## Library and file layout

### ZenithScript target

```cmake
# Zenith2/Engine/ZenithScript/CMakeLists.txt — the whole file
zenith_add_library(ZenithScript
	DEPENDS ZenithCore ZenithReflect ZenithECS ZenithScene ZenithAssets ZenithJobs ZenithPlatform ZenithProfiling
	THIRD_PARTY Luau
	EXCEPTIONS INTERNAL
	TEST_DEPENDS ZenithECS_Custom ZenithECS_Flecs ZenithECS_EnTT ZenithSave
	TEST_LABELS script-conformance
	CONFORMANCE_LABELS script-conformance)
```

`KIND ENGINE`, `SUPPORTED ZENITH_ALL` (the default: `win`, `win-asan`, `linux-tsan`, `android-arm64`, `android-x86_64`). The manifest's Luau entry gives `Luau.VM` and `Luau.CodeGen` to `ZenithScript` through its `targets` array ([Z2-N-ARCH-128]). `EXCEPTIONS INTERNAL` compiles `Internal/**` as `ZenithScript_EH` with `/EHs` or `-fexceptions` and the root without ([Z2-N-SCRIPT-024], [Z2-N-ARCH-175]). `ZenithSave` in `TEST_DEPENDS` lets `ZenithScript_Tests` compare `uSCRIPT_ASYNC_SOURCE_KIND_CAPACITY` with ZenithSave's `uASYNC_SOURCE_KIND_CAPACITY` (`Test_Script_Async_SourceKindCapacityMatchesRegistry`); the library itself never depends on ZenithSave. `TEST_LABELS` and `CONFORMANCE_LABELS` give every case of `ZenithScript_Tests` and `ZenithScript_Conformance` the label `script-conformance` of [Z2-N-SCRIPT-523] ([Z2-N-ARCH-251]); SCRIPT's cases compiled into other targets (`ZenithReflect_Tests`, `ZenithCook_Tests`), the integration suites and the fault tests' second label `script-fault-matrix` wait for the per-suite and integration-suite label rules of cross-doc row X215 (Design/SCRIPT/Tests.md → "Gate steps and labels").

### ZenithScript root headers

`Zenith_ScriptTypes.h`, `Zenith_ScriptError.h`, `Zenith_ScriptRecovery.h`, `Zenith_ScriptWorld.h`, `Zenith_ScriptRuntime.h`, `Zenith_ScriptHost.h`, `Zenith_ScriptAsync.h`, `Zenith_ScriptStateSnapshot.h`, `Zenith_ScriptBehaviourComponent.h`, `Zenith_ScriptCompileOptions.h`, `Zenith_ScriptTypesEmitter.h` — each declared in Design/SCRIPT/PublicAPI.md. The library root holds these headers and `CMakeLists.txt` only; `Scripts/Core.luau` (`@zenith/Core`) is a non-source directory the glob ignores ([Z2-N-ARCH-161]).

### ZenithScript internal files

Runtime.md → "Runtime internal files" (VMs, protected call, hooks, watchdog, runners, instances, tick boundary, async, random, GC, profiling, diagnostics, halt), plus:

| File | Contents | Specified in |
|---|---|---|
| `Internal/Zenith_ScriptTransaction.{h,cpp}` | TransactionStructures.md's structures, the arena set, the retention ceilings, invocation layout and the scratch-to-retention copy | Transactions.md |
| `Internal/Zenith_ScriptViews.{h,cpp}` | userdata tags, row proxies, component and fixed-array views, container objects, view validation, the write path | Design/SCRIPT/Bindings.md → "Userdata tags and view objects"; Transactions.md → "Write path implementation" |
| `Internal/Zenith_ScriptMerge.cpp` | the merge node, capacity, commit | Transactions.md |
| `Internal/Zenith_ScriptSideEffects.cpp` | `AppendSideEffect` routing, per-entity accounting | Transactions.md → "Side-effect routing" |
| `Internal/Zenith_ScriptQuarantine.cpp` | the quarantine store | Transactions.md → "Quarantine store" |
| `Internal/Zenith_ScriptBindingDispatch.cpp`, `Internal/Zenith_ScriptInvocationContextImpl.{h,cpp}`, `Internal/Zenith_ScriptMarshal.{h,cpp}` | dispatcher, invocation context, marshalling | Bindings.md |
| `Internal/Zenith_ScriptSandbox.cpp` | environments, the Zenith replacements, stubs | Design/SCRIPT/Bindings.md → "Environment construction" |
| `Internal/Zenith_ScriptModuleLoader.{h,cpp}`, `Internal/Zenith_ScriptManifest.cpp`, `Internal/Zenith_ScriptBytecodeVerify.cpp` | the loader, `Zenith_ScriptModuleManifest`, `.zlbc` verification | Design/SCRIPT/Bindings.md → "Module loader implementation"; Compiler.md → "Load-time verification procedure" |
| `Internal/Zenith_ScriptTypesEmitter.cpp` | `Zenith_ScriptEmitDefinitions` | Design/SCRIPT/Bindings.md → "Definition emitter procedure" |
| `Internal/Zenith_ScriptState.{h,cpp}`, `Internal/Zenith_ScriptCheckpoint.cpp`, `Internal/Zenith_ScriptCheckpointPools.cpp` | schemas, registration, proxies, serializer, restore, pools | HotReload.md |
| `Internal/Zenith_ScriptRecovery.cpp`, `Internal/Zenith_ScriptHotReload.cpp`, `Internal/Zenith_ScriptMigrationHost.cpp`, `Internal/Zenith_ScriptSaveState.cpp` | recovery scheduling, staging and go-live, the schema migration host, saves | HotReload.md |
| `Internal/Zenith_ScriptCompileOptions.cpp`, `Internal/Zenith_ScriptBindingSetHash.cpp` | production options, the mutable-globals list, the binding-set hash | Scripting.md → "Development compilation" |
| `Internal/Debug/*` | the DAP server and the sampler | Debugger.md → "Debugger internal files" |

No `Internal/Windows|Android|Linux/` files: ZenithScript has no platform-specific code (the FP environment is JOBS's, the listener PLAT's).

### ZenithScript test files

`Zenith2/Engine/ZenithScript/Tests/` → `ZenithScript_Tests`; `Tests/Conformance/` → `ZenithScript_Conformance`; `Tests/Fuzz/` → the fuzz targets; the file list and data directories are Design/SCRIPT/Tests.md → "Test files and targets".

### ZenithScriptCompiler target

```cmake
# Zenith2/Engine/ZenithScriptCompiler/CMakeLists.txt — the whole file
zenith_add_library(ZenithScriptCompiler
	DEPENDS ZenithScript ZenithReflect ZenithCore
	THIRD_PARTY Luau
	EXCEPTIONS INTERNAL
	SUPPORTED ZENITH_HOST
	TEST_LABELS script-conformance)
```

`Luau.Ast` and `Luau.Compiler` are owned by this target through the manifest's `targets` array. `SUPPORTED ZENITH_HOST` (`win`, `win-asan`, `linux-tsan`) keeps it out of every Android preset; its consumers are ZenithCook, ZenithEditor and test targets ([Z2-N-ARCH-242]). It is an `Engine/` library, so its `Internal/` sources are held to the runtime-library header rules of `ForbiddenCheck` rule F4 (`Zenith2/Docs/Contracts.md` → "Target classes", [Z2-N-CONTRACT-577]: a host-only engine library outside every Shipping closure is runtime-class for F4; cross-doc row X214 applied): the cook's file, directory and process access goes through the caller's `Zenith_ScriptCookHost` (Design/SCRIPT/PublicAPI.md → "Zenith_ScriptCook header") and its worker threads are CORE's `Zenith_Thread`.

### ZenithScriptCompiler files

Public headers `Zenith_ScriptCompile.h`, `Zenith_ScriptStateless.h`, `Zenith_ScriptCook.h`; internal files Compiler.md → "Compiler library internals"; tests `Zenith2/Engine/ZenithScriptCompiler/Tests/` → `ZenithScriptCompiler_Tests` (Tests.md → "Compiler and cook tests"); fuzz `Tests/Fuzz/Script_Compile.cpp`.

### SCRIPT files in other libraries

| File | Library | Contents |
|---|---|---|
| `Zenith2/Engine/ZenithReflect/Zenith_ScriptBinding.h` | ZenithReflect | the binding ABI (PublicAPI.md → "Zenith_ScriptBinding header") |
| `Zenith2/Engine/ZenithReflect/Internal/Zenith_ScriptBindingTable.cpp` | ZenithReflect | the table's non-template members, `Zenith_ScriptFailThunk` |
| `Zenith2/Engine/ZenithReflect/Tests/Zenith_ScriptBinding_Tests.cpp` | ZenithReflect | ABI tests |
| `Zenith2/Engine/ZenithECS/Zenith_ECSScriptTraits.h` | ZenithECS (ECS writes it; SCRIPT specifies the trait shape) | entity-handle traits |
| `Zenith2/Engine/ZenithRuntime/Internal/ScriptBindings/*.cpp` | ZenithRuntime | low-library bindings (Design/SCRIPT/Bindings.md → "Low-library binding files") |
| `Zenith2/Engine/ZenithRuntime/Internal/Zenith_ScriptHostImpl.cpp` | ZenithRuntime (MOD writes it) | the host interfaces' implementations |

### Script source trees and cooked outputs

| Path | Contents |
|---|---|
| `Zenith2/Games/<Name>/Scripts/**.luau` | the game's modules (`@game/…`) |
| `Zenith2/Games/<Name>/Scripts/Tests/Migration/<Behaviour>/v<n>.zbst` | migration fixtures ([Z2-N-SCRIPT-400]) |
| `Zenith2/Engine/ZenithScript/Scripts/Core.luau` | `@zenith/Core` |
| `Zenith2/Engine/<Module>/Scripts/**.luau` | a module's Luau library (`@<module>/…`) |
| `${ZENITH_OUT}/Generated/Luau/<Name>/` | `zenith.d.luau`, `components.d.luau`, `Zenith.luau`, `Types.luau`, `.luaurc`, `bindings.json` (the binding listing, Design/SCRIPT/Bindings.md → "Binding listing"), `.stamp` (never tracked, [Z2-N-ARCH-208]) |
| `${ZENITH_OUT}/Cooked/<Name>/<platform>/Scripts/` | `scripts.zlmf`, `<alias>/<path>.zlbc`, `state_ids.json` |

### Benchmarks and fuzz targets

Tests.md → "Benchmarks and harnesses" (`Zenith_Bench_ScriptStateless`, `Zenith_Bench_ScriptNativeParity`, `Zenith_Bench_BudgetsScript`, `Zenith_Bench_ScriptVM`, `Zenith_Bench_ScriptHotReload`) and → "Fuzz targets" (five `Zenith_Fuzz_Script_*` targets).

### Supported matrix and compile groups

| Target | `SUPPORTED` | Compile groups |
|---|---|---|
| `ZenithScript` | `ZENITH_ALL` | root sources exceptions off; `ZenithScript_EH` = `Internal/**` with `/EHs` / `-fexceptions` |
| `ZenithScriptCompiler` | `ZENITH_HOST` | root exceptions off; `ZenithScriptCompiler_EH` = `Internal/**` |
| `Luau.VM`, `Luau.CodeGen` | with ZenithScript | `/EHs` / `-fexceptions` (owned by ZenithScript) |
| `Luau.Ast`, `Luau.Compiler` | with ZenithScriptCompiler | `/EHs` / `-fexceptions` |
| `Luau.Analysis`, `Luau.Config` and their dependencies, `Luau.Analyze.CLI` | `ZENITH_TOOLS_HOST` | exceptions on; linked only into the host executable `luau-analyze` |
| `ZenithScript_Tests`, `ZenithScript_Conformance`, `ZenithScriptCompiler_Tests` | as their library | exceptions off ([Z2-N-SCRIPT-026]) |

Every Shipping link closure excludes `ZenithScriptCompiler`, `Luau.Compiler`, `Luau.Ast` and every `Zenith_ScriptDap` symbol ([Z2-N-SCRIPT-054], [Z2-N-SCRIPT-488]).

## Public API

### API rules

- Every fallible public function returns `Zenith_Result<T, Zenith_ScriptError>` (or the ABI's `Zenith_ScriptThunkError`, or `Zenith_AsyncTableError`); no public header names a Luau or STL type ([Z2-N-SCRIPT-882], [Z2-N-CONTRACT-272]).
- Every public type carries one contract block (PublicAPI.md); factories are `static Create(const Desc&)` returning `Zenith_Result<Zenith_Owned<T>, Zenith_ScriptError>`, every desc carries `Zenith_Heap*` and `Zenith_FaultInjector*` ([Z2-N-CONTRACT-225]), and fault points use the SCRIPT high 16 bits `0x5343`.
- Script misuse never reaches an assert: it is a Luau error of the catalogue; C++ misuse of the API by engine code is `ZENITH_ASSERT`/`ZENITH_CHECK` (§ "Luau errors versus asserts").
- `Zenith_ScriptRuntime`, `Zenith_ScriptWorld` and the binding table (until sealed) are `MAIN_THREAD`; `Zenith_AsyncResultTable::Complete`, `Zenith_ScriptWorld::IsHalted` and the watchdog's registration are the only cross-thread entry points.

### Header index

| Type or function | Header | Declared in |
|---|---|---|
| `Zenith_ScriptModuleID`, `Zenith_ScriptInvocationID`, `Zenith_ScriptLogicalID`, `Zenith_ScriptModuleKind`, `Zenith_ScriptInstanceState`, `Zenith_ScriptViewKind`, `Zenith_ScriptModuleManifestEntry`, `Zenith_ScriptModuleManifest`, `Zenith_ScriptBytecodeHeader`, `Zenith_ScriptBytecode`, `Zenith_ScriptGCTuning`, `Zenith_ScriptWatchdog`, the `uSCRIPT_*` limits | `Zenith_ScriptTypes.h` | Scripting/PublicTypes.md → "ZenithScript public types"; contract blocks and additions PublicAPI.md → "Zenith_ScriptTypes header" |
| `Zenith_ScriptNativePolicy` | `Zenith_ScriptTypes.h` | Scripting.md → "Native codegen conformance"; PublicAPI.md |
| `Zenith_ScriptWatchdogDesc`, `Zenith_ScriptStateSchemaRecord` | `Zenith_ScriptTypes.h` | PublicAPI.md → "Zenith_ScriptTypes header" |
| `Zenith_ScriptErrorCode`, `Zenith_ScriptError`, `Zenith_ScriptDiagnostic`, `Zenith_ScriptErrorCodeFromKind`, `Zenith_ToString(Zenith_ScriptErrorCode)` | `Zenith_ScriptError.h` | PublicAPI.md → "Zenith_ScriptError header" (reconciled, S1; pending X115) |
| `Zenith_ScriptRecoveryPolicy`, `Zenith_ScriptRecoverySettings`, `Zenith_ScriptRecoveryResolve` | `Zenith_ScriptRecovery.h` | Scripting.md → "Recovery policy"; PublicAPI.md → "Zenith_ScriptRecovery header" |
| `Zenith_ScriptWorldState`, `Zenith_ScriptWorldDesc`, `Zenith_ScriptWorld` | `Zenith_ScriptWorld.h` | Scripting/LibraryLayout.md → "Public API of Zenith_ScriptWorld"; additions PublicAPI.md → "Zenith_ScriptWorld header" |
| `Zenith_ScriptSafepointLimits`, `Zenith_ScriptCapacityLimits` | `Zenith_ScriptWorld.h` | PublicAPI.md → "Zenith_ScriptWorld header" |
| `Zenith_ScriptRuntimeDesc`, `Zenith_ScriptRuntime` | `Zenith_ScriptRuntime.h` | Scripting/PublicTypes.md → "Zenith_ScriptRuntime"; additions PublicAPI.md → "Zenith_ScriptRuntime header" |
| `Zenith_ScriptDebuggerDesc`, `Zenith_ScriptRegisterEngineTypes`, `Zenith_ScriptRegisterBehaviourStateTypes`, `Zenith_RegisterScriptConfigKeys` | `Zenith_ScriptRuntime.h` | PublicAPI.md → "Zenith_ScriptRuntime header" |
| `Zenith_ScriptWorldListener`, `Zenith_ScriptPublicationObserver`, `Zenith_ScriptPublicationSource`, `Zenith_AsyncPublicationHandler` | `Zenith_ScriptHost.h` | PublicAPI.md → "Zenith_ScriptHost header" |
| `Zenith_ScriptPublication`, `Zenith_AsyncTableError`, `Zenith_AsyncResultTable` | `Zenith_ScriptAsync.h` | PublicAPI.md → "Zenith_ScriptAsync header" |
| `Zenith_ScriptSavedState`, `Zenith_ScriptStateSnapshot` | `Zenith_ScriptStateSnapshot.h` | PublicAPI.md → "Zenith_ScriptStateSnapshot header" |
| `Zenith_BehaviourComponent` | `Zenith_ScriptBehaviourComponent.h` | PublicAPI.md → "Zenith_ScriptBehaviourComponent header" |
| `Zenith_ScriptCompileOptions`, `Zenith_ScriptSandboxMutableGlobals`, `Zenith_ScriptProductionCompileOptions`, `Zenith_ScriptBindingSetHash` | `Zenith_ScriptCompileOptions.h` | Scripting.md → "Development compilation"; blocks PublicAPI.md |
| `Zenith_ScriptDefinitionFile`, `Zenith_PfnDefinitionSink`, `Zenith_ScriptEmitDefinitions` | `Zenith_ScriptTypesEmitter.h` | PublicAPI.md → "Zenith_ScriptTypesEmitter header" |
| the binding ABI (`Zenith_ScriptValue` … `Zenith_ScriptBindingTable`, `Zenith_ScriptInvocationContext`, the capacity and side-effect enums) | `ZenithReflect/Zenith_ScriptBinding.h` | Scripting/Bindings.md → "Public binding types"; Scripting.md → "Capacity classes" |
| `Zenith_ScriptThunkErrorCode`, reshaped `Zenith_ScriptThunkError`, `Zenith_ScriptBindingError`, `Zenith_ScriptFailThunk`, `Zenith_ScriptSideEffectConsumer`, table additions | `ZenithReflect/Zenith_ScriptBinding.h` | PublicAPI.md → "Zenith_ScriptBinding header" (pending X115, X117) |
| `Zenith_ScriptModuleSource`, `Zenith_ScriptCompileModule`, `Zenith_ScriptFastcallSite`, `Zenith_ScriptListFastcallSites`, `Zenith_ScriptCompileReport` | `ZenithScriptCompiler/Zenith_ScriptCompile.h` | Scripting.md → "Development compilation"; PublicTypes.md; PublicAPI.md → "Zenith_ScriptCompile header" |
| `Zenith_ScriptStatelessDiagnostic`, `Zenith_ScriptStatelessVerdict`, `Zenith_ScriptAcceptedModuleSet`, `Zenith_ScriptCheckStateless` | `ZenithScriptCompiler/Zenith_ScriptStateless.h` | Scripting.md → "Load-time interface"; PublicAPI.md |
| `Zenith_ScriptCookDesc`, `Zenith_ScriptCookResult`, `Zenith_ScriptCookGame`, `Zenith_ScriptExtractDeclaration`, `Zenith_ScriptLintModule` | `ZenithScriptCompiler/Zenith_ScriptCook.h` | PublicAPI.md → "Zenith_ScriptCook header" |

Internal types a worker implements but no other library sees: `Zenith_ScriptVM`, `Zenith_ScriptVMDesc`, `Zenith_ScriptSystemRef`, `Zenith_ScriptCounters`, `Zenith_ScriptDiagnostics` (Scripting/PublicTypes.md → "ZenithScript internal types"); the protected-entry types (Scripting.md → "The protected entry"); the transaction structures (TransactionStructures.md); `Zenith_ScriptRandomKey` (Scripting.md → "Counter-based random numbers"); and this design's `Zenith_ScriptTxnArenaSet`, `Zenith_ScriptTxnRetentionEntry`, `Zenith_ScriptDebugGate`, `Zenith_ScriptWatchdogEntry`, `Zenith_ScriptInstanceDirectory`, `Zenith_ScriptInstance`, `Zenith_ScriptDiagnosticStore`, `Zenith_ScriptQuarantineEntry`, `Zenith_ScriptPendingReload`, `Zenith_ScriptStagedGeneration`, `Zenith_ScriptStateSchema`, `Zenith_ScriptCheckpointPools`, `Zenith_ScriptBreakpointSet`, `Zenith_ScriptDap`, `Zenith_ScriptMigrationHost`, `Zenith_ScriptTickBoundaryAdapter`, `Zenith_ScriptInvocationContextImpl`.

### Handle kinds used

| Handle | Kind | Owner |
|---|---|---|
| `Zenith_EntityHandle` (as `Zenith.Entity`, opaque in the ABI) | generational, world-tagged | ECS |
| `Zenith_AssetHandle<T>` (as `Zenith.Asset<T>`) | shared asset handle | ASSET |
| `Zenith_ScriptCallbackRef` | a registry reference plus owner VM and invocation (validated on every use) | SCRIPT |
| `Zenith_ScriptTicket` | provisional index, then completion ID | SCRIPT |
| provisional spawn handle | `Zenith_SpawnTicket` with the provisional bit, remapped for one tick | ECS/SCRIPT |
| `Zenith_ScriptModuleID` | an index valid for one manifest | SCRIPT |

Views are not handles: they are invocation-scoped values validated on every access ([Z2-N-SCRIPT-441]).

### Configuration keys

SCRIPT registers its keys with `Zenith_RegisterScriptConfigKeys(Zenith_ConfigSchema&, Zenith_BuildPlatform)` (Design/SCRIPT/PublicAPI.md → "Zenith_ScriptRuntime header"), which the composition root calls at startup step 6 in library-layer order (`Zenith2/Docs/Design/MOD.md` → "Configuration-key registration"). Every key is registered in every configuration, so a configuration file is valid everywhere; the two development keys carry their disabled default in Shipping too and the runtime never reads them there (the DAP service and the sampler are not linked, `if constexpr (Zenith_IsShippingBuild())`). Budget-derived defaults come from the generated budget table; while a row is pending the design default below applies.

| Key | Type | Default | Simulation-affecting | Restart-required / layers | Source |
|---|---|---|---|---|---|
| `script.watchdogMs` | int | from Z2-B-SCRIPT-003 (design default 250) | no | no / every layer | the watchdog |
| `script.gc.stepKBPerTick` | int | Z2-B-SCRIPT-007 (64) | no | no / every layer | GC |
| `script.nativePolicy` | enum `disabled`, `toolsOnly`, `all` | `toolsOnly` on every platform: no platform runs `all` before the Stage 3 native parity run ([Z2-N-SCRIPT-308]); the Stage 6 pin check switches it to the manifest's recorded `nativePolicy.<platform>` through the generated pin header (`eSCRIPT_LUAU_NATIVE_POLICY_WINDOWS`, `_ANDROID`) | yes | yes / every layer but `CONFIG_LAYER_QUALITY_TIER` (PLAT's schema rules 1 and 2, `Zenith2/Docs/Design/PLAT/Configuration.md` → "Registration API") | VM creation |
| `script.dapPort` | int | 0 (disabled); the Player's `--script-dap-port` sets it | no | no / every layer | development builds; ignored in Shipping |
| `script.profiler.sample` | bool | false | no | no / every layer | development builds; ignored in Shipping |
| `script.print.maxLinesPerModulePerTick` | int | 64 | no | no / every layer | the Zenith `print` |

Every other Z2-B-SCRIPT value is read from the budget table into `Zenith_ScriptWorldDesc` by the composition root, not exposed as a key, because changing it changes results (budgets are simulation inputs recorded by the replay's simulation-affecting list only through the build identity).

## Data structures and formats

### In-memory structures summary

| Structure | Scope | Size or bound | Specified in |
|---|---|---|---|
| `Zenith_ScriptVM` | per VM | object + Luau state; heap capped by Z2-B-SCRIPT-009/010 | Scripting.md → "VM model and threading"; Runtime.md → "VM creation steps" |
| worker VM slot table | per world | `m_uFrameThreadCount` ≤ 254 slots | Runtime.md → "VM pool per world" |
| per-VM thunk tables | per VM | one closure per bound function | Design/SCRIPT/Bindings.md → "Per-VM thunk tables" |
| transaction arenas | per world: one scratch `Zenith_Arena` per slot, one retention `Zenith_Arena` per (script system, slot) in use | scratch (F + 1) × (round_up(2W, 64 KiB) + 64 KiB), fixed; retention: the tick's retained transactions (their sum is what Z2-B-SCRIPT-013 reports), one retained block per entry between merges, each arena capped by its derived retention ceiling, which bounds reserved bytes (twice the used bound plus one block, and at least the entry-0 pre-reservation) (Transactions.md → "Retention ceiling derivation") | Transactions.md → "Transaction arena partition" |
| `Zenith_ScriptTransaction` and friends | per invocation | ≤ W | TransactionStructures.md; Transactions.md → "Invocation layout in the arenas" |
| instance directory | per world | one record per behaviour instance | Runtime.md → "Behaviour instance directory" |
| checkpoint slot pools | per world, per module generation | 2 slots + live state per instance, ≤ Z2-B-SCRIPT-015 each | HotReload.md → "Checkpoint slot pools" |
| quarantine store | per world | one entry per quarantined instance | Transactions.md → "Quarantine store" |
| hot-reload staging area | per runtime | one pending entry per (world, module); staged generations in the window | HotReload.md → "Hot-reload staging area" |
| async result table | per world | Σ in-flight × max result bytes | Runtime.md → "Asynchronous result table" |
| diagnostic store | per world and one per runtime | 64 × ~2.6 KB | Transactions.md → "Diagnostic store implementation" |
| watchdog table | per process | `m_uMaxWorlds × (1 + frame threads)` entries of 16 B (at most 4,096) | Runtime.md → "Watchdog internals" |
| breakpoint sets | per world, development builds | copy-on-write arrays | Debugger.md → "Breakpoint storage and fan-out" |
| sampling rings | per VM, development builds | 65,536 samples | Debugger.md → "Sampling profiler implementation" |

### Formats summary

| Format | Owner | Versioned header | Specified in |
|---|---|---|---|
| `.zlbc` cooked bytecode | SCRIPT | magic `ZLBC`, container version 1 | Scripting.md → "Cooked bytecode format"; Compiler.md → "Cooked bytecode container writer" |
| `scripts.zlmf` module manifest | SCRIPT | magic `ZLMF`, version 1, trailing hash | Compiler.md → "Module manifest format" |
| `ZBST` behaviour checkpoint | SCRIPT | magic `ZBST`, format version 1 | Scripting.md → "Checkpoint format"; HotReload.md → "Checkpoint serializer" |
| `state_ids.json` field-ID lock | SCRIPT (cook) | `{ "schema": "zenith2.script-state-ids", "schemaVersion": 1, "behaviours": { "<Name>": { "<field path>": <id> } } }` | Compiler.md → "Declaration extraction" |
| `.d.luau`, `Zenith.luau`, `Types.luau`, `.luaurc` | SCRIPT (and ZenithReflect for components) | a header comment with the binding-set hash | Scripting/Bindings.md → "Luau type generation"; Design/SCRIPT/Bindings.md → "Definition emitter procedure" |
| stateless corpus `manifest.json` | SCRIPT (tests) | Scripting/StatelessCorpus.md → "Corpus layout" | the corpus |
| sample dump JSON | SCRIPT (development) | `schema` `zenith2.script-samples`, `schemaVersion` 1 | Debugger.md → "Sampling profiler implementation" |
| ZRPL publications | SAVE | ZRPL format version 1 | Scripting.md → "Replay-log recording" |

Every cooked and saved format is little-endian and written field by field, never a C++ layout (Z2-D-038).

### Fault points

| Factory | Fault points (`0x5343xxxx`) |
|---|---|
| `Zenith_ScriptWorld::Create` | `…_WORLD_MAIN_VM` 0001, `_SLOT_TABLE` 0002, `_TXN_ARENAS` 0003, `_ASYNC_TABLE` 0004, `_HANDLER` 0005, `_WATCHDOG_REGISTER` 0006, `_DIAGNOSTICS` 0007 |
| `Zenith_ScriptRuntime::Create` | `…_RUNTIME_WORLD_TABLE` 0010, `_RELOAD_QUEUE` 0011, `_DEBUGGER` 0012, `_DIAGNOSTICS` 0013 |
| `Zenith_ScriptRuntime::CreateWorld` (with a debugger) | `…_RUNTIME_DAP_SLOTS` 0014 |
| `Zenith_ScriptWatchdog::Create` | `…_WATCHDOG_THREAD` 0020, `_TABLE` 0021 |

## Algorithms and state machines

### Algorithm index

| Algorithm | Specified in |
|---|---|
| protected entry | Scripting.md → "Protected entry algorithm"; Runtime.md → "Protected call implementation" |
| VM creation, world creation, runtime creation | Runtime.md |
| lazy worker VM creation | Scripting.md → "Lazy worker VM creation"; Runtime.md → "Chunk-system node body" |
| allocator hook, interrupt, watchdog | Scripting.md; Runtime.md |
| GC scheduling | Runtime.md → "Garbage collection scheduling" |
| script-system registration and system order | Runtime.md → "Node registration and frame-graph placement" |
| chunk-system node, behaviour node, wait conditions, instance directory | Runtime.md |
| invocation lifecycle, write path, snapshots, shadows, side effects, accounting | Transactions.md |
| merge, capacity, commit, failure handling, quarantine | Transactions.md |
| tick-boundary handling, async publication, random draws, world halt | Runtime.md |
| dispatcher, marshalling, environment construction, module loading, deep freeze | Bindings.md |
| definition emission | Design/SCRIPT/Bindings.md → "Definition emitter procedure" |
| compile, guard, fast-call listing, cook, declaration extraction, lint | Compiler.md |
| stateless check (the prototype and its alternatives) | Compiler.md → "Stateless check prototype", "Stateless alternatives prototypes" |
| checkpoint serialization, restore, migration, recovery, hot reload staging, go-live, rollback, saves | HotReload.md |
| DAP framing, requests, fan-out, stop loop, stepping, sampling | Debugger.md |

### Tick walkthrough

One tick of world w with behaviours and chunk systems in both script phases:

| # | Where | SCRIPT work |
|---|---|---|
| 0 | frame prologue steps 6 and 8 (first tick of the frame only) | queued hot reloads staged inside the registry window, then release lists drained and changed declarations committed through the system reload scope (HotReload.md → "ApplyPendingReloads procedure", "Schedule commit in the prologue"); step 8 recompiles |
| 1 | tick boundary step 1 | completed async results published in completion-ID order, recorded, handlers run (platform events) |
| 2 | tick boundary step 3 | modules that failed last tick disabled (SCRIPT's own flag); committed generations go live (first tick of the frame); automatic recoveries; per-tick resets |
| 3 | phase 2 | Early chunk systems fan out per chunk on worker VMs; Early behaviour nodes run on the main VM in module order; each node's merge on the chain in system order; GC steps |
| 4 | sync point after phase 2 | ECS plays back the accepted commands; spawn results feed provisional handles and checkpoint fixups |
| 5 | phases 3–9 | no script; events delivered at phase 9 |
| 6 | phase 10 | Late behaviour nodes and Early modules' handler nodes (handlers, `OnUpdate`/`Run`, checkpoint-only transactions), Late chunk systems, merges |
| 7 | sync point after phase 10 | playback; checkpoint fixups rewritten |
| 8 | phase 11 | SAVE reads the state snapshot when a save is requested |
| 9 | end of tick | ECS end-of-tick; instance destruction observed at the next directory refresh |

### Behaviour instance state machine

| State | Event | Next state and action |
|---|---|---|
| NEW | first node invocation of the instance | ACTIVE; `OnCreate` (or the first `Run` segment) runs as an entity transaction |
| ACTIVE | an invocation fails | QUARANTINED; Luau objects released, active checkpoint kept, module flagged |
| ACTIVE | the component's `Enabled` becomes false | DORMANT; no invocation; state kept |
| DORMANT | `Enabled` becomes true | ACTIVE; `OnRecover` runs first when `Run` is declared (the coroutine was never kept) |
| QUARANTINED | last-good restore or a successful hot reload of the module | PENDING_RECOVER; restored from the active slot |
| PENDING_RECOVER | its declared phase's first invocation | ACTIVE after `OnRecover` succeeds; QUARANTINED when it fails |
| ACTIVE | hot reload goes live | PENDING_RECOVER with the migrated state |

PENDING_RECOVER is entered only by an instance whose behaviour declares `OnRecover` (that is, `Run`); an instance of any other behaviour goes from QUARANTINED, from a go-live or from a save restore straight to ACTIVE (DORMANT while disabled), and `RestoreSavedStates` restores each entry to the lifecycle its flags record (Design/SCRIPT/HotReload.md → "Recovery invocations and exact restore"). `OnRecover` runs under the reserved `uSCRIPT_RECOVERY_ORDINAL` and consumes no ordinal ([Z2-N-SCRIPT-928] to [Z2-N-SCRIPT-930]).
| any | the entity disappears from the query | DESTROYED; references, slots, results and quarantine entry released |

### Entity transaction state machine

Scripting/TransactionStructures.md's `Zenith_EntityTxnState`: OPEN → SUCCEEDED (return or yield) → ACCEPTED (fits at the merge) → COMMITTED; OPEN → FAILED (error; row restored at failure); SUCCEEDED → REJECTED (does not fit, or an earlier invocation of the same instance failed or was rejected; row restored at the merge). FAILED and REJECTED are terminal; COMMITTED is terminal. No transition skips the merge.

### Module state machine

| State | Event | Next state and action |
|---|---|---|
| LOADED | a failure reported in a tick | LOADED with `m_bDisableAtNextTickBoundary` |
| LOADED (flagged) | tick boundary step 3 | DISABLED; `m_bDisabled` set, so the module's node and merge bodies return at entry (and its systems' ECS disable is requested through `Zenith_TickBoundaryAccess::RequestSystemEnabled`); listener told |
| DISABLED | reload goes live or last-good restore at a tick boundary | LOADED; `m_bDisabled` cleared (and the ECS re-enable requested the same way) |
| DISABLED | automatic policy, recovery limit reached | DISABLED_FOR_SESSION |
| any | load fails at world load | DISABLED_FOR_SESSION until a successful hot reload |

### Script world state machine

| State | Event | Next state and action |
|---|---|---|
| CREATED | `LoadModules`, `RegisterScriptNodes` | RUNNING (`SCRIPT_WORLD_STATE_RUNNING`) |
| RUNNING | a fatal event on any thread | HALTED (`SCRIPT_WORLD_STATE_HALTED`); tick abandoned; listener told; tick flagged non-replayable |
| HALTED | any request | refused with `SCRIPT_ERROR_WORLD_HALTED` |
| RUNNING or HALTED | the runtime destroys the world after draining | destroyed |

### Hot-reload request state machine

| State | Event | Next state and action |
|---|---|---|
| — | `RequestHotReload` passes its checks | QUEUED (replaces an earlier queued entry for the same world and module) |
| QUEUED | prologue step 6, `ApplyPendingReloads` | STAGED, or ROLLED_BACK on any failure (window abandoned); stays QUEUED while the (world, module) holds a COMMITTED generation not yet live |
| STAGED | prologue step 6, `CommitPendingReloads`: the window published and the world's schedule commit succeeded (or the declaration's access is unchanged) | COMMITTED; the recompile runs in step 8 of the same frame |
| STAGED | the window did not publish, or the schedule commit failed | ROLLED_BACK: staged objects released once, the previous generation and schedule unchanged (HotReload.md → "Rollback procedure") |
| COMMITTED | the world's next tick boundary step 3 | LIVE (go-live; allocates nothing and cannot fail); old objects queued for release, old checkpoint elements and buffers retired |
| COMMITTED | `RestoreSavedStates` of the world, or `RequestModuleReload` of the module, out of tick | LIVE through the same go-live run on the main thread before the call acts (HotReload.md → "Go-live at the tick boundary") |

## Threading lifetime and error contract

### Thread affinity per type

| Type | Keyword | Notes |
|---|---|---|
| `Zenith_ScriptRuntime`, `Zenith_ScriptWorld` | `MAIN_THREAD` | except `IsHalted` (any) and the node bodies and tick-boundary adapter it registers, which reach only the world tables Runtime.md → "Script world internals" orders |
| `Zenith_ScriptVM` (Internal) | `OWNER_THREAD` | the main thread for the main VM; the owning frame thread for a worker VM |
| `Zenith_ScriptWatchdog` | `ANY_THREAD` | registration under its lock |
| `Zenith_AsyncResultTable` | `ANY_THREAD_EXCEPT(AllocateCompletionID, SetHandler, SetPublicationResolver, DeclareSource, RestoreNextCompletionID)` | Complete and the count reads from any thread; `Retire`, the lists and the restores out of tick (the lists also on the phase-11 node) |
| `Zenith_ScriptStateSnapshot` | `PHASE` | phase 11 and out of tick |
| `Zenith_ScriptBindingTable` | `MAIN_THREAD` then `IMMUTABLE` | sealed at startup |
| `Zenith_ScriptInvocationContext` | `OWNER_THREAD` | the invocation's thread |
| `Zenith_ScriptSideEffectConsumer` implementations | merge nodes of one world serialised, worlds concurrent | per-world state in the consumer |
| compiler functions | `ANY_THREAD` | one arena and report per thread |
| value types, enums | `IMMUTABLE` | |

### Frame-graph placement

Runtime.md → "Node registration and frame-graph placement" (the table of where SCRIPT work runs and the system descriptors). Summary: chunk systems are `SYSTEM_KIND_SCRIPT_CHUNK` nodes (`JOB_AFFINITY_ANY`) in phases 2 and 10 with a merge node each; behaviours are `SYSTEM_KIND_SCRIPT_BEHAVIOURS` nodes (`JOB_AFFINITY_MAIN`), one per module and phase, with a merge node each; every merge node has ECS's `ANY` affinity, which SCRIPT needs no exception from because a merge body touches no Luau object (Transactions.md → "Merge node body"); every merge node is on its phase's merge chain in registration order, and dependents of a script-written column hang off the merge node ([Z2-N-SCRIPT-159]); SCRIPT's tick-boundary handler runs in `ECS.TickBoundary` steps 1 and 3 and calls no `MAIN_THREAD` member of ECS (the module disable is SCRIPT's own flag, with ECS's matching `Zenith_TickBoundaryAccess::RequestSystemEnabled`); hot reload stages and commits in prologue step 6, and the schedule recompiles in step 8 of the same frame.

### Lock-order entries

| Level | Name | Library | Protects | Short-hold |
|---|---|---|---|---|
| 700 | `uLOCK_LEVEL_SCRIPT_WATCHDOG` | ZenithScript | the watchdog's VM table and its tag → world-index map (a debugger stop never takes it) | yes |
| 710 | `uLOCK_LEVEL_SCRIPT_DAP` (`Zenith_MutexNoProfiling`) | ZenithScript (development builds) | the DAP session, the stop mailbox, each world's debugger gate and stopped count, each VM's `m_bCounted`; a holder allocates nothing and takes no other lock (Debugger.md → "Stop protocol", the DAP mutex rule) | no: a frame thread acquires it only inside the debugger-stop bracket (`Zenith_MarkThreadAsJobWorker(false)` … restore of the flag `Zenith_IsJobWorkerThread()` reported on entry, Debugger.md → "Stop protocol" rule 4), the development-build exemption of Q44 ruling (a), pending cross-doc row X120 |

Each row is added to `Zenith2/Docs/Contracts.md` → "Threading and concurrency contract" by the ticket that lands the lock ([Z2-N-CONTRACT-320]). The watchdog lock is never taken by a debugger stop (a VM's stop flag is an atomic), and no thread holding 710 calls PLAT's listener (600 … 630), the watchdog (700) or a heap. No other SCRIPT structure takes a lock: per-world merges are serialised by the merge chain, slots by the job affinity contract, retention entries by a per-entry atomic flag (Transactions.md → "Transaction arena partition") and the diagnostic store by per-slot atomics (Transactions.md → "Diagnostic store implementation").

### Construction and shutdown

| Step (`Zenith2/Docs/Architecture/CompositionRoot.md` → "Startup sequence") | SCRIPT action |
|---|---|
| 6 | `Zenith_RegisterScriptConfigKeys` in library-layer order; `Zenith_ScriptRegisterEngineTypes(xTypes)` in the Script slot of the engine types (`Zenith_BehaviourComponent`) |
| 10 | after `RegisterSchemaBlob` and before `Freeze`: `Zenith_ScriptModuleManifest::Load` of `content:/Scripts/scripts.zlmf` and `Zenith_ScriptRegisterBehaviourStateTypes(xTypes, xManifest)` (`Zenith2/Docs/Design/ECS.md` → "Behaviour-state type registration"; cross-area notes E12(a), E18(a); pending cross-doc row X122) |
| 14 | create the binding table (engine and runtime low-library bindings), the watchdog (`m_uMaxWorlds` 16, `m_uFrameThreadCount` from the job system), the runtime (a debugger in development builds when requested; its absence is no error unless `m_bRequired`) |
| 15 | modules register their bindings and consumers; the composition root seals the table (exit code 3 on a collision) |
| 16 | `Zenith_ScriptReadWorldLimits(xSchema, xStore, xDesc)` fills the desc's row-valued members (Scripting/LibraryLayout.md → "Public API of Zenith_ScriptWorld", cross-doc row X219 applied; Design/SCRIPT/PublicAPI.md → "Zenith_ScriptWorld header"), then `CreateWorld` for the initial world after the ECS world simulation exists; both calls are MOD's composition root (A5 cross-area rows SB16, MB24) |
| 17 | `LoadModules`, `RegisterScriptNodes` |
| 18 | the schedule compile includes the script systems |
| shutdown, after the tick drains | `DestroyWorld` (worker VMs, then the main VM), then the runtime (debugger thread joined), then the watchdog (thread joined), then the binding table |

### Cancellation

SCRIPT has no asynchronous operation of its own: script requests are other subsystems' operations. A cancelled frame instance completes script nodes without their bodies (ECS); a world transition discards the old world's unpublished results at the transition (Scripting.md → "Tick-boundary publication" step 4) by destroying its result table with the world.

### Error enumerations

<!-- assumption: pending core-doc (X115, X121) -->
The error enumerations below and the struct shapes of Design/SCRIPT/PublicAPI.md → "Zenith_ScriptError header" and "Zenith_ScriptBinding header" replace Scripting.md's declarations (X115); the codes `SCRIPT_ERROR_WATCHDOG_TABLE_FULL` and `SCRIPT_ERROR_DEBUGGER_UNAVAILABLE` and the diagnostics this design adds are mirrored by X121.
<!-- /assumption -->

`Zenith_ScriptErrorCode` (PublicAPI.md → "Zenith_ScriptError header"; numbered from 1), the code of `Zenith_ScriptError`:

| Code | Returned when |
|---|---|
| `SCRIPT_ERROR_RUNTIME` … `SCRIPT_ERROR_LOAD` (1–12) | a Luau error of the kind with the same name (Scripting.md → "The protected entry"), code = kind + 1 |
| `SCRIPT_ERROR_INVALID_DESC` (13) | a factory or call argument outside its documented range, including Z2-B-SCRIPT-011's refusal; `ConsoleEvaluate` in Shipping |
| `SCRIPT_ERROR_OUT_OF_MEMORY` (14) | a C++-side allocation outside any transaction failed (world creation, the arena pre-reservations, staging) |
| `SCRIPT_ERROR_WORLD_HALTED` (15) | a request to a halted world |
| `SCRIPT_ERROR_UNKNOWN_MODULE` (16) | a module ID not in the manifest |
| `SCRIPT_ERROR_NOT_STATELESS_VERIFIED` (17) | a hot reload of simulation bytecode without the stateless flag |
| `SCRIPT_ERROR_RELOAD_QUEUE_FULL` (18) | the reload queue cannot grow |
| `SCRIPT_ERROR_WATCHDOG_TABLE_FULL` (19) | a world whose main VM would take a watchdog world index when all `m_uMaxWorlds` are in use (Runtime.md → "Watchdog internals") |
| `SCRIPT_ERROR_DEBUGGER_UNAVAILABLE` (20) | `Zenith_ScriptRuntime::Create` with `m_xDebugger.m_bRequired` whose DAP listener could not be created; without `m_bRequired` the runtime is created without a debugger and nothing is returned (Runtime.md → "Script runtime internals") |
| `SCRIPT_ERROR_BYTECODE_MISMATCH` (21) | a `.zlbc` or manifest field differs from the build (the Luau-side ZS1805 is `LOAD`) |

`Zenith_ScriptThunkErrorCode` (PublicAPI.md → "Zenith_ScriptBinding header"), the code of the binding ABI's `Zenith_ScriptThunkError`: `SCRIPT_THUNK_ERROR_RUNTIME` … `SCRIPT_THUNK_ERROR_LOAD` (1–12), one per `Zenith_ScriptErrorKind` in kind order, returned by a thunk (`Zenith_ScriptFailThunk`) and by `AppendSideEffect`, `InvokeCallback` and `RetainCallback`.

`Zenith_ScriptBindingError` (the same header), returned by `Zenith_ScriptBindingTable::Seal`: `SCRIPT_BINDING_ERROR_DUPLICATE_ID`, `_DUPLICATE_NAME`, `_INVALID_NAME`, `_INVALID_DESCRIPTOR`, `_MISSING_CONSUMER`; the composition root maps any of them to startup exit code 3.

`Zenith_AsyncTableError` (PublicAPI.md → "Zenith_ScriptAsync header"): `FULL`, `PAYLOAD_TOO_LARGE`, `UNKNOWN_COMPLETION`.

Diagnostics that are not errors are Scripting/MisuseCatalogue.md → "Diagnostics that are not Luau errors", plus three this design adds (X121): `SCRIPT_STATE_RESTORE_FAILED` (HotReload.md → "Restore implementation"), `SCRIPT_DEBUGGER_UNAVAILABLE` (Runtime.md → "Script runtime internals") and `SCRIPT_ASYNC_CAPACITY_HELD` (Runtime.md → "Asynchronous result table"), and one this design adds with cross-doc row X451: `SCRIPT_BINDING_DEPRECATED_CALL`, code ZS1808 of SCRIPT's own block, a warning recorded when a world loads a module that calls a deprecated binding (Design/SCRIPT/Bindings.md → "Deprecated call sites"). Every enumeration has `Zenith_ToString` ([Z2-N-CONTRACT-466]).

### Luau errors versus asserts

| Condition | Response |
|---|---|
| any script misuse (every row of MisuseCatalogue.md) | a controlled Luau error with the exact catalogue text, in every build ([Z2-N-SCRIPT-445]) |
| budget, snapshot cap, yield in a system, checkpoint failure | abort-class Luau errors, re-raised through `pcall` ([Z2-N-SCRIPT-306]) |
| VM cap, arena growth failure, watchdog, internal exception, counter wrap | fatal: world halt, never an assert |
| cooked data, saves, replays, DAP bytes, source text | untrusted input: `Zenith_Result` or a counted rejection ([Z2-N-CONTRACT-472]) |
| engine code misusing the API (a binding added after `Seal`, a wrong-thread call, reading a `Zenith_ScriptValue` of another kind, `LUA_MULTRET`) | `ZENITH_ASSERT`/`ZENITH_CHECK` ([Z2-N-CONTRACT-471]) |

### Determinism obligations

| Obligation | Mechanism |
|---|---|
| merge order (system order, logical ID, sequence) | registration order = system order; radix sort at the merge; ECS merge keys |
| outcome independent of worker count and layout | entity boundary, per-row restore, per-entity budgets and caps, merge-time capacity in merge order |
| randomness | the 40-byte key, CORE derivations |
| async visibility | tick-boundary publication in completion-ID order; completion IDs at commit in merge order |
| GC and allocation invisible | no GC observation; VM-cap trips fatal |
| debugger and sampler invisible | stops and samples between safepoints |
| arena growth invisible | growth failure fatal; success changes no value |

## Diagnostics and profiling

### Counters

Published once per tick at the tick boundary through the profiler hooks (`COUNTER_KIND_DELTA` per-tick, `COUNTER_KIND_CUMULATIVE` totals):

| Counter | Kind | Meaning |
|---|---|---|
| `script.invocations`, `script.entity_txns` | delta | invocations and entity transactions of the tick |
| `script.failures.<kind>` | delta | failures by `Zenith_ScriptErrorKind` |
| `script.merge.rejections.<class>` | delta | merge rejections by capacity class |
| `script.instances.quarantined`, `script.modules.disabled` | cumulative | current counts |
| `script.checkpoints`, `script.checkpoint_bytes` | delta | Z2-B-SCRIPT-014/021 |
| `script.snapshot_bytes` | delta | the per-tick total against Z2-B-SCRIPT-013 |
| `script.safepoints` | delta | against Z2-B-SCRIPT-020 |
| `script.gc.kb`, `script.gc.ns`, `script.vm.bytes.<index>` | delta, delta, cumulative | GC work and heap per VM |
| `script.txn.arena_growths` | cumulative | retention-arena block allocations beyond the retained block (Transactions.md → "Transaction arena partition" rule 4) |
| `script.async.published_unconsumed.<kind>` | cumulative | published script results of each source kind not yet released (the S8 retention trace) |
| `script.diagnostics_dropped`, `script.wait.unknown` | cumulative | diagnostics dropped by a busy store slot; unknown yielded wait values |
| `script.bindings.deprecated_calls` | cumulative | `SCRIPT_BINDING_DEPRECATED_CALL` warnings recorded at world loads (Design/SCRIPT/Bindings.md → "Deprecated call sites") |

`Zenith_ScriptDiagnostics::GetTickCounters` returns the same values to tests (PublicTypes.md; the fields this design adds are Runtime.md → "Profiling implementation", pending X121). No counter counts dropped events: every delivered event runs its handlers (Runtime.md → "Behaviour node body").

### Profiling zones summary

`Script.Node`, `Script.Invocation` (module name, entity count or simulation ID), `Script.Merge`, `Script.Checkpoint`, `Script.Restore`, `Script.HotReload`, `Script.GC` (Runtime.md → "Profiling implementation"; [Z2-N-SCRIPT-464], [Z2-N-SCRIPT-465]).

### On-screen profiler rows summary

Scripting.md → "On-screen profiler rows" (Luau time, Luau GC, Script counters) are built by DEVT's overlay from the zones and counters above, in the game and the editor on Windows and the S24, behind the runtime toggle ([Z2-N-SCRIPT-466]).

### Log categories and messages

| Category | Content |
|---|---|
| `script.runtime` | every script error at its point of failure (module, simulation ID, tick, code, message), world halts |
| `script.load` | module load failures, manifest and bytecode refusals |
| `script.bindings` | binding table seal failures; the ZS1808 deprecated-call warnings at `LOG_LEVEL_WARNING` |
| `script.reload` | hot reload and recovery outcomes |
| `script.print` | the Zenith `print` (development builds) |
| `script.dap` | the DAP token line, connections, protocol errors |
| `script.cook` | cook diagnostics (tools) |
| `script.watchdog` | the hang report at `LOG_LEVEL_FATAL`, before `ZENITH_FATAL(FATAL_CODE_SCRIPT_HANG)` (Runtime.md → "Watchdog internals") |

### Diagnostic store

Transactions.md → "Diagnostic store implementation": the last 64 diagnostics per world with their full text, read by the editor's script panel, the console and tests through `GetDiagnostic`.

### Debug views and dumps

The sampling profiler's flame-graph dump (Debugger.md); the console command `script.dump.instances` (development builds) writing the instance directory (module, simulation ID, state, dirty flag, active slot) as JSON; frame dumps include each world's script counters of the dumped frame.

## Authoring and cooking

### What SCRIPT authors and what EDIT owns

Scripts are authored as text (`Games/<Name>/Scripts/**.luau`) in any editor; EDIT owns the in-editor script workflows (panels, the diagnostics list, the console window, the reload command, play-in-editor). SCRIPT provides EDIT: in-process compilation and the stateless check (ZenithScriptCompiler), `RequestHotReload` and `RequestModuleReload`, `ConsoleEvaluate`, the diagnostic store, definition regeneration on registry changes, and the DAP server the chosen debugger client attaches to (D-LUAUDBG, open, Stage 1).

### Cook steps summary

Compiler.md → "Cook pipeline" (ten steps: walk, generate definitions, resolve requires, classify, type-check, lint, stateless check, extract declarations, compile, write), invoked by ZenithCook for `<Name>_Content` and `<Name>_Types` ([Z2-N-ARCH-207]); the watch mode feeds development Players.

### IDE support

The generated `.luaurc`, `zenith.d.luau`, `components.d.luau` and prelude modules under `${ZENITH_OUT}/Generated/Luau/<Name>/` give luau-lsp strict types for every engine and game type; cook diagnostics print `<path>:<line>:<column>:` so IDEs link them.

### Migration hooks

Behaviour state migrates through `Migrate` chains with per-version fixtures (HotReload.md → "Migration execution"); data-defined schema `script` ops run through `Zenith_ScriptMigrationHost` (HotReload.md → "Schema migration script host"); cooked bytecode re-cooks and never migrates ([Z2-N-SCRIPT-505]).

## Failure handling

### Failure kinds and code paths

Every failure class of Scripting.md → "Commit and failure semantics" (table "Failure classes") and where this design handles it:

| Failure | Detected in | Code path |
|---|---|---|
| script error, uncaught misuse | protected call | Transactions.md → "Failure handling by kind" (entity rollback, quarantine for behaviours, module flagged) |
| budget exhausted | interrupt callback | the same path, kind `BUDGET` |
| yield in a system | the worker environment's coroutine stub | the same path, kind `YIELD_IN_SYSTEM` |
| snapshot cap | per-entity accounting | the same path, kind `SNAPSHOT_CAP` |
| checkpoint failure | the checkpoint serializer | the same path, kind `CHECKPOINT` |
| merge-time capacity rejection | the merge node | Transactions.md → "Merge node body" step 4 (no disable) |
| transaction-arena growth failure | a retention allocation the root heap refuses | Runtime.md → "World halt procedure" |
| retention ceiling reached | a retention allocation beyond the derived ceiling | `ZENITH_CHECK` (an engine defect: Transactions.md → "Retention ceiling derivation") |
| VM memory cap | the allocator hook | the halt procedure |
| watchdog | the watchdog thread, then the interrupt | the halt procedure; a hang past 2× the limit is a crash report and `ZENITH_FATAL` |
| internal exception | the protected call's `catch` | the halt procedure |
| `OnRecover` failure | protected call | re-quarantine, counted as a module failure |
| hot-reload failure | `ApplyPendingReloads`, `EndHotReload`, schedule commit | HotReload.md → "Rollback procedure" |

### Missing and corrupt input

| Input | Behaviour |
|---|---|
| missing or corrupt `scripts.zlmf` | `Zenith_ScriptModuleManifest::Load` fails; startup exit code 3 |
| a `.zlbc` that fails verification | that module fails to load (ZS1805), is disabled for the session, recorded in the diagnostic store and reported through `OnModuleDisabled`; `LoadModules` still succeeds and the world runs without it; `LoadModules` fails only when the manifest is unusable or none of its modules loaded (Runtime.md → "Script world internals") |
| a behaviour component naming an unknown module | no instance; one diagnostic |
| a corrupt checkpoint in a save | that instance's restore fails; it stays quarantined; the save's other data loads |
| a replay log with fewer publications than a tick needs | the replay source reports empty; the replay stops at the first mismatch (SAVE's check) |
| DAP bytes, source text | closed connection; compile error |

### Allocation failure

Inside a transaction, memory comes from the slot's pre-reserved scratch arena and the system's retention arena, and the per-entity cap raises a Luau error first; a retention-arena, pool or VM growth failure is fatal (world halt), never a partial commit, and a retention arena reaching its derived ceiling is `ZENITH_CHECK`; outside a transaction (world creation, staging, the reload queue), allocation failure is `SCRIPT_ERROR_OUT_OF_MEMORY` and the operation has no effect.

### Device loss and platform events

SCRIPT holds no device resource. Platform events reach scripts only as published results and `Platform.*` events at tick boundaries (Design/PLAT.md → "Frame-graph placement"); an Android suspend is a frame-loop pause, which SCRIPT observes as no ticks.

### Cancellation and shutdown failures

Shutdown drains the tick before any VM is destroyed; a world destroyed with requests in flight discards their results; the DAP thread and the watchdog thread are joined by their owners' destructors ([Z2-N-SCRIPT-088]).

### Budget breach behaviour

| Row | Breach code | SCRIPT behaviour |
|---|---|---|
| Z2-B-SCRIPT-001, -002, -018 | BR-LUAU | `BUDGET` Luau error of the entity transaction (recovery rows fail the recovery or migration) |
| Z2-B-SCRIPT-019 | BR-LOAD | the module fails to load |
| Z2-B-SCRIPT-003 | BR-FATAL | world halt |
| Z2-B-SCRIPT-009, -010 | BR-FATAL | world halt |
| Z2-B-SCRIPT-011 | BR-REFUSE | world creation refused |
| Z2-B-SCRIPT-012, -015 | BR-LUAU | `SNAPSHOT_CAP` / `CHECKPOINT` Luau error (and a load error for a schema above -015) |
| Z2-B-SCRIPT-016, -022 … -026, Z2-B-ECS-004, Z2-B-GAME-003 | BR-MERGE | merge-time rejection |
| Z2-B-SCRIPT-007, -013, -017, -020, -021 | BR-REPORT | reported, never a rollback |
| Z2-B-SCRIPT-004, -005, -006, -008, -014 | BR-ACCEPT | workload acceptance |

## Test strategy

### Test kinds and locations

| Kind | Where | Examples |
|---|---|---|
| unit (doctest) | `ZenithScript/Tests/`, `ZenithScriptCompiler/Tests/`, `ZenithReflect/Tests/Zenith_ScriptBinding_Tests.cpp` | firewall, VM, transactions, merge, state, modules, sandbox, bindings, compiler, cook, DAP |
| conformance | `ZenithScript/Tests/Conformance/` (all three ECS backends) | the determinism replay matrix, the five fault-matrix tests |
| corpus | `ZenithScript/Tests/Corpus/{Stateless,Budget,Representative}/` | the D-LUAU-STATELESS corpus, the native parity corpus |
| misuse | `ZenithScript/Tests/Misuse/<Code>.luau` | one trigger per catalogue row |
| golden | `ZenithScript/Tests/Golden/fixture.d.luau` | the definition emitter |
| fuzz | `ZenithScript/Tests/Fuzz/`, `ZenithScriptCompiler/Tests/Fuzz/` | checkpoint decode, manifest, bytecode header, DAP messages, compile |
| death | `z2-death-assert` suite | the unprotected-allocation assert |
| benchmark and harness | `Zenith2/Benchmarks/Script*/` | stateless evaluation, native parity, budget rows, VM cost, hot-reload latency |
| game fixtures | `Games/<Name>/Scripts/Tests/Migration/` | migration fixtures per behaviour version |
| device | the stage acceptance runs on the S24 | RAII tests under HWASan, native parity on arm64, DAP attach |

The full catalogue — every named test with its file and case — is Design/SCRIPT/Tests.md.

### Gate steps per test group

Tests.md → "Gate steps and labels": `Test` (label `script-conformance`, fault tests also `script-fault-matrix`), the `Zenith2Memory` lines for the VM glue, `Sanitizers -Preset win-asan` for fuzz smoke runs, `ForbiddenCheck`, `Libgraph`, `ApiContractCheck` and `HeaderSelfContained` for the firewall and API rules, and the S24 acceptance runs.

## Budget rows

### Rows SCRIPT owns

Every row of `Zenith2/Docs/Budgets.md` → "Scripting rows", all pending, producer "Stage 1 per-system budget ticket for SCRIPT"; the descriptive names are Scripting/BudgetRows.md → "Budget rows used":

| ID | Name | Unit | Breach | Enforced or measured by |
|---|---|---|---|---|
| Z2-B-SCRIPT-001 | Behaviour callback safepoint budget | safepoints | BR-LUAU | interrupt callback |
| Z2-B-SCRIPT-002 | Chunk-system safepoint budget per entity | safepoints | BR-LUAU | interrupt callback |
| Z2-B-SCRIPT-003 | Script wall-clock watchdog per invocation | ms | BR-FATAL | watchdog |
| Z2-B-SCRIPT-004 | Early scripts phase CPU | ms | BR-ACCEPT | profiler, workload runs |
| Z2-B-SCRIPT-005 | Late scripts phase CPU | ms | BR-ACCEPT | profiler, workload runs |
| Z2-B-SCRIPT-006 | Chunk-system CPU per tick | ms | BR-ACCEPT | profiler, workload runs |
| Z2-B-SCRIPT-007 | Per-world GC step budget | KB of GC work (per VM per tick in this design; reconciliation S9, pending X119) | BR-REPORT | GC scheduling |
| Z2-B-SCRIPT-008 | Luau GC time per frame | ms | BR-ACCEPT | profiler |
| Z2-B-SCRIPT-009 | World main VM memory cap | MB | BR-FATAL | allocator hook |
| Z2-B-SCRIPT-010 | Per-worker chunk-system VM memory cap | MB | BR-FATAL | allocator hook |
| Z2-B-SCRIPT-011 | VM memory times workers per world | MB | BR-REFUSE | world creation |
| Z2-B-SCRIPT-012 | Snapshot memory per entity | KB | BR-LUAU | per-entity accounting |
| Z2-B-SCRIPT-013 | Snapshot memory per tick | MB | BR-REPORT | tick counters |
| Z2-B-SCRIPT-014 | Checkpoint serialization CPU per tick | ms | BR-ACCEPT | checkpoint zone |
| Z2-B-SCRIPT-015 | Checkpoint reserved memory per behaviour instance | KB | BR-LUAU | cook (schema maximum), serializer |
| Z2-B-SCRIPT-016 | Container storage capacity per tick | KB | BR-MERGE | ECS `CONTAINER_BYTES` class |
| Z2-B-SCRIPT-017 | Script GC tuning parameters | %, ×, KB | BR-REPORT | VM creation |
| Z2-B-SCRIPT-018 | Recovery safepoint budget per OnRecover or Migrate step | safepoints | BR-LUAU | interrupt callback |
| Z2-B-SCRIPT-019 | Module-load safepoint budget per module scope | safepoints | BR-LOAD | interrupt callback |
| Z2-B-SCRIPT-020 | Script safepoints per world per tick | safepoints | BR-REPORT | tick counters |
| Z2-B-SCRIPT-021 | Checkpoint bytes per tick | KB | BR-REPORT | tick counters |
| Z2-B-SCRIPT-022 | String-intern capacity per world session | entries, KB | BR-MERGE | SCRIPT capacity |
| Z2-B-SCRIPT-023 | Audio trigger capacity per tick | triggers | BR-MERGE | SCRIPT capacity |
| Z2-B-SCRIPT-024 | VFX trigger capacity per tick | triggers | BR-MERGE | SCRIPT capacity |
| Z2-B-SCRIPT-025 | Save-request capacity per tick | requests | BR-MERGE | SCRIPT capacity |
| Z2-B-SCRIPT-026 | Per-source asynchronous request capacity in flight | requests | BR-MERGE | SCRIPT capacity, async table |

### Rows SCRIPT reads from other areas

| Row | Use |
|---|---|
| Z2-B-ECS-003 Spawn capacity per tick | entity-slot class through ECS |
| Z2-B-ECS-004 Structural command capacity per tick | command class through ECS |
| Z2-B-GAME-003 Event queue capacity per tick | event class through ECS |
| Z2-B-AI-003 Asynchronous path requests in flight | the path source's in-flight capacity |
| Z2-B-EDIT-002 Script hot reload | hot-reload latency (`Zenith_Bench_ScriptHotReload`) |
| Z2-B-EDIT-009 Walkthrough iterate a script | the editor walkthrough that includes a script iteration (EDIT measures) |

### Row needs recorded as pending

SCRIPT needs no new budget row. Every retention arena has a cap, as every arena must ([Z2-N-CONTRACT-227]), but the cap is derived from existing rows and capacities — Z2-B-SCRIPT-012, the world's entity capacity and Z2-B-GAME-003 — by Transactions.md → "Retention ceiling derivation", as the most one script system can retain on one slot in a tick of a world within those rows; and it bounds the RESERVED bytes `Zenith_Arena`'s cap checks — twice the used bound plus one block, covering every abandoned block tail, and never below the entry-0 pre-reservation — so no content legal under [Z2-N-BUDGET-303] can reach it, reaching it is `ZENITH_CHECK` (an engine defect) and never a script error, and it adds nothing to the MEM-* sums (transaction memory is already counted as drawn from Z2-B-SCRIPT-013). The unit of Z2-B-SCRIPT-007 (per frame in Budgets.md, per VM per tick in Scripting.md) is reconciliation item S9, pending cross-doc row X119 and settled by the Stage 1 per-system budget ticket for SCRIPT, which allocates every row of § "Rows SCRIPT owns" with `Zenith_Bench_BudgetsScript` and whose ruling is `blocks`-linked from the D-SCRIPT-TXN refinement ruling ([Z2-N-BUDGET-292]).

## Stage plan

### Stage 1

Decision and harness work, and the Stage 3 foundations it needs, pulled forward with fix version `Z2 Stage 3` where the code is production code:

1. **The ECS, job and Luau lifetime specifications** (with ECS and JOBS; Roadmap deliverable 6): confirm the exception-firewall and VM-model design of this document; the firewall ticket builds the two library targets with `EXCEPTIONS INTERNAL`, the Luau integration, `Zenith_ScriptProtectedCall`, `Zenith_ScriptVM` with the allocator and interrupt hooks, the compiler guard and `Zenith_ScriptCompileModule` (pulled forward, fix version `Z2 Stage 3`), and measures and records the Luau API linkage (`Test_Script_Firewall_LuauApiLinkageRecorded`) and the fast-call check (`Test_Script_Sandbox_NoFastcallForReplacedBuiltins`).
2. **D-LUAU-STATELESS (open, Stage 1)**: the harness ticket (the prototypes of Compiler.md, the corpus of StatelessCorpus.md, `Zenith_Bench_ScriptStateless`) and the `needs-human` ruling ticket appending the selected mechanism to `DecisionLog.md` with its measurements (Z2-D-084, Z2-D-097); plus the behaviour-state model's confirmation.
3. **D-LUAUDBG (open, Stage 1)**: the DAP server skeleton with the scripted client (`Test_Script_Dap_TokenRequired` and the skeleton form of `Test_Script_Dap_SessionRequests`), pulled forward into the Stage 1 sprint with its transport and JSON-reader producers and keeping fix version `Z2 Stage 3`, as `Zenith2/Docs/DecisionLog.md` → "D-LUAUDBG" states; and the ruling ticket choosing the client, the request set and the attach workflow, whose runtime criteria (breakpoints on every VM, stepping across yields, declared-state inspection, S24 attach) are recorded as conditions the Stage 3 DAP tests and the S24 attach run verify. The stop protocol's bracketed exemption is Q44 ruling (a), stated in `Zenith2/Docs/Contracts.md` → "Lock ordering table".
4. **D-SCRIPT-TXN refinement (open, Stage 1)**: `Zenith_Bench_ScriptNativeParity` over the budget corpus at VM level on Windows x64 and the S24 (the representative corpus per entity transaction joins in Stage 3, and no platform's native policy is recorded `all` before that run, [Z2-N-SCRIPT-308]), and the ruling fixing the native policy per platform and confirming the transaction, capacity and publication rules (including this document's reconciliations S5, S6, S8).
5. **D-PHASES refinement (open, Stage 1)** (GAME-owned): SCRIPT confirms the script-phase placement of handlers and checkpoint-only nodes ([Z2-N-SCRIPT-142], [Z2-N-SCRIPT-158]).
6. **The determinism rules** (with ECS): the merge-key and randomness specification of this document.
7. **The Stage 1 per-system budget ticket for SCRIPT**: every Z2-B-SCRIPT row allocated with `Zenith_Bench_BudgetsScript`.
8. D-SEL-HASH (open, Stage 1) is CORE's; SCRIPT consumes `Zenith_Hash64` for the random key, source IDs and every format hash.

### Stage 2

The runtime's foundations beside ECS: the binding ABI in ZenithReflect (`Zenith_ScriptBinding.h`, `Zenith_ScriptBindingTable` with `Seal`, `Zenith_ScriptFailThunk`, the ABI tests) so ECS writes `Zenith_ECSScriptTraits.h` and module owners compile bindings; ECS's component `.d.luau` emitter (ECS-owned) that SCRIPT's emitter calls. Pull-forward candidates with fix version `Z2 Stage 3`, each only when its Stage 1 rulings are in: the module manifest and `.zlbc` formats with their fuzz targets; `Zenith_ScriptWorld` creation over the ECS world simulation; the chunk-system node over `Zenith_SystemContext`.

### Stage 3

The Luau gameplay deliverable (Roadmap Stage 3 deliverable 7), in dependency order: runtime and worlds; module loader and environments; dispatcher, marshalling and views; the definition emitter and the cook pipeline; chunk systems and the merge; behaviours, the instance directory and wait conditions; state, checkpoints, restore and `OnRecover`; quarantine, recovery and hot reload; the async result table and publication; the watchdog, GC scheduling and profiling; the DAP server per the D-LUAUDBG ruling; the schema migration host; behaviour state in saves (with SAVE's save/load); every test of Scripting.md → "Required conformance tests" and "Fault-matrix tests" and every added test of Tests.md; `Games/Minimal`'s first behaviour (`Scripts/Main.luau`, `Zenith2/Docs/Design/MOD.md` → "Step construction notes").

### Stage 4

No SCRIPT engine work: OpenWorldSlice's Luau quest, dialogue and inventory samples are game-side (Z2-D-077), owned by GAME and the OpenWorldSlice tickets; SCRIPT's obligation is that they load unchanged under the stateless mechanism and pass the cook ([Z2-N-SCRIPT-761]), and editor script workflows are EDIT's against Z2-B-EDIT-002.

### Stage 6

Shipping bytecode qualification (`Z2-I-SCRIPT-23`): the Luau pin record with bytecode versions and native policy, the re-cook and patch-refusal rules exercised on a packaged build, the Shipping closure checks, and the security hardening evidence for scripts (fuzz campaigns, parser limits, no debugger in Shipping).

### Stage 7

The XR additions SCOPE requests (cross-area row SP7) need nothing new of SCRIPT at Stage 7: the side-effect kind `SIDE_EFFECT_KIND_XR_RECENTRE` and its consumer slot land in Stage 3 (Design/SCRIPT/Bindings.md → "Side-effect consumer registration"), proven with a test consumer, so the enumerator's value is fixed before any module depends on it; ZenithXR registers the real consumer and declares `ASYNC_SOURCE_XR` (Runtime.md → "Source kind values") at Stage 7. The `XR` namespace raises SCRIPT's ZS1510 and ZS1702 and no code of its own.

### API-only slices

1. `Zenith_ScriptBinding.h` with contract blocks (Stage 2), so every binding owner compiles against it.
2. The ZenithScript public headers of PublicAPI.md with stub implementations returning `SCRIPT_ERROR_INVALID_DESC`, so MOD's composition root and SAVE's snapshot compile before the runtime lands (Stage 2 pull-forward).
3. `Zenith_ScriptStateless.h` and `Zenith_ScriptCompile.h` (Stage 1), so the harness and the cook compile against the interface every alternative presents.

## Decisions

### Settled decisions

| ID | Decision | Where it binds here |
|---|---|---|
| Z2-D-016 | Luau scripting; every invocation is a transaction over engine-visible state | whole design |
| Z2-D-017 | the Luau exception firewall (amended to name both firewall targets) | § "Supported matrix and compile groups"; Runtime.md → "Protected call implementation"; Compiler.md → "Compiler guard implementation" |
| Z2-D-018 | one VM per worker thread per world for chunk systems; behaviours on the main VM | Runtime.md → "VM pool per world" |
| Z2-D-019 | determinism including failing ticks | § "Determinism obligations"; Tests.md → "Determinism and fault-matrix tests" |
| Z2-D-023 | explicit serialized behaviour state and `OnRecover` | HotReload.md |
| Z2-D-051 | the entity is the transaction boundary | Transactions.md |
| Z2-D-052 | safepoint budgets, wall clock only as a fatal watchdog | Runtime.md → "Interrupt callback implementation", "Watchdog internals" |
| Z2-D-053 | asynchronous results at tick boundaries in completion-ID order | Runtime.md → "Asynchronous result table" |
| Z2-D-080 | frame phases and authority | Runtime.md → "Node registration and frame-graph placement" |
| Z2-D-005 | on-screen profiler with Luau time and GC | § "Diagnostics and profiling" |
| Z2-D-038 | JSON authoring, versioned binary cooked data | § "Formats summary" |
| Z2-D-045 | no exceptions outside the firewall; results; asserts for programmer errors | § "Luau errors versus asserts" |
| Z2-D-046 | no mutable global state | Runtime.md (every object owned by the runtime or a world) |
| Z2-D-048 | compatibility per kind | Compiler.md → "Load-time verification procedure"; HotReload.md → "Behaviour state in saves" |
| Z2-D-075 | modding excluded | § "What SCRIPT does not own" |
| Z2-D-077 | quest, dialogue, inventory, interaction are game-side Luau | § "Stage 4" |
| Z2-D-084 | measured decisions split into harness and ruling tickets | § "Stage 1" |
| Z2-D-097 | rulings appended to DecisionLog.md in the ticket's diff | § "Stage 1" |

### Open decisions

| Decision | Stage | What SCRIPT needs from it | Until it rules |
|---|---|---|---|
| D-LUAU-STATELESS | 1 | the enforcement mechanism behind `Zenith_ScriptCheckStateless` | the primary prototype (Compiler.md → "Stateless check prototype") is the interface's implementation in the harness |
| D-LUAUDBG | 1 | the DAP client, request set and attach workflow | the server implements the minimum request set of Debugger.md; its stop protocol follows Q44 ruling (a), pending X120 |
| D-SCRIPT-TXN refinement | 1 | confirmation of Z2-D-051/052/053 and the per-platform native policy; reconciliations S5, S6, S8 | `SCRIPT_NATIVE_POLICY_TOOLS_ONLY` everywhere |
| D-PHASES refinement | 1 | confirmation of handler and checkpoint-only node placement | as Scripting.md states |
| D-SEL-HASH | 1 | the `Zenith_Hash64` function (random key, source IDs, format hashes) | the candidate XXH3-64 with seed 0 |
| D-SEL-CRASH | 2 | the crash handler the watchdog's hang report uses | the hang report logs and calls `ZENITH_FATAL` |
| D-SEL-HEAP | 1 | the general heap behind every VM, transaction arena and pool | the `Zenith_Heap` interface |

### Selection and classification decisions SCRIPT owns

SCRIPT owns no named selection decision of `Zenith2/Docs/Architecture.md` → "Named selection decisions". It owns two design choices a ruling may revisit, each recorded as a reconciliation item with its acceptance rule: the static declaration form for cooked state schemas (S3; acceptance: every repository game's behaviours cook, and the representative corpus is unchanged) and retention-by-consumption of asynchronous results (S8; acceptance: the async tests of Tests.md, including `Test_Script_Async_UnconsumedCapacityHeldDiagnostic`, and a bounded result arena under W-Dense's request load).

## Requirement index

Every requirement of `Zenith2/Docs/Requirements/SCRIPT.md` and the headings that specify it (paths relative to `Zenith2/Docs/Design/`).

| Requirement | Title | Specified in |
|---|---|---|
| Z2-R-SCRIPT-001 | ZenithScript library target | SCRIPT.md → "ZenithScript target"; SCRIPT.md → "Library dependencies"; SCRIPT.md → "Supported matrix and compile groups" |
| Z2-R-SCRIPT-002 | ZenithScriptCompiler library target | SCRIPT.md → "ZenithScriptCompiler target"; SCRIPT.md → "ZenithScriptCompiler files"; SCRIPT/Compiler.md → "Compiler library internals" |
| Z2-R-SCRIPT-003 | Public headers are Luau-free and contract-blocked | SCRIPT.md → "API rules"; SCRIPT/PublicAPI.md → "Header list"; SCRIPT.md → "Header index" |
| Z2-R-SCRIPT-004 | Luau headers confined to the firewall partitions | SCRIPT.md → "ZenithScript internal files"; SCRIPT.md → "Supported matrix and compile groups" |
| Z2-R-SCRIPT-005 | Exception tokens confined to two catch sites | SCRIPT/Runtime.md → "Protected call implementation"; SCRIPT/Compiler.md → "Compiler guard implementation" |
| Z2-R-SCRIPT-006 | One protection site | SCRIPT/Runtime.md → "Protected call implementation"; SCRIPT/Bindings.md → "Environment construction" |
| Z2-R-SCRIPT-007 | Exception flags on the firewall compile groups alone | SCRIPT.md → "Supported matrix and compile groups"; SCRIPT.md → "ZenithScript target" |
| Z2-R-SCRIPT-008 | No longjmp error model | SCRIPT.md → "Supported matrix and compile groups" |
| Z2-R-SCRIPT-009 | Luau API linkage measured, recorded and re-measured | SCRIPT/Tests.md → "Firewall and VM tests"; SCRIPT.md → "Stage 1" |
| Z2-R-SCRIPT-010 | Protected entry algorithm | SCRIPT/Runtime.md → "Protected call implementation"; SCRIPT/Runtime.md → "Allocator hook implementation" |
| Z2-R-SCRIPT-011 | Protected-call nesting and depth limit | SCRIPT/Runtime.md → "Protected call implementation" |
| Z2-R-SCRIPT-012 | Compiler guard | SCRIPT/Compiler.md → "Compiler guard implementation"; SCRIPT/Compiler.md → "Compile procedure" |
| Z2-R-SCRIPT-013 | Callback re-entry through the invocation context | SCRIPT/Bindings.md → "Dispatcher implementation" |
| Z2-R-SCRIPT-014 | RAII firewall tests on every host preset and the S24 | SCRIPT/Tests.md → "Firewall and VM tests"; SCRIPT/Tests.md → "Gate steps and labels" |
| Z2-R-SCRIPT-015 | Shipping closure excludes the compiler and the debugger | SCRIPT.md → "Supported matrix and compile groups"; SCRIPT/Debugger.md → "Scope of this file" |
| Z2-R-SCRIPT-016 | Error records conform to the Contracts error form | SCRIPT/PublicAPI.md → "Zenith_ScriptError header"; SCRIPT/PublicAPI.md → "Zenith_ScriptBinding header"; SCRIPT/Transactions.md → "Diagnostic store implementation"; SCRIPT.md → "Reconciliation items" |
| Z2-R-SCRIPT-017 | One main VM per world on the main thread | SCRIPT/Runtime.md → "Script world internals"; SCRIPT/Runtime.md → "VM pool per world"; SCRIPT/Runtime.md → "Behaviour node body"; SCRIPT/Runtime.md → "Tick-boundary handler" |
| Z2-R-SCRIPT-018 | Worker VM slot table and VM indices | SCRIPT/Runtime.md → "VM pool per world"; SCRIPT/Runtime.md → "Script world internals" |
| Z2-R-SCRIPT-019 | Lazy worker VM creation | SCRIPT/Runtime.md → "Chunk-system node body"; SCRIPT/Runtime.md → "VM creation steps" |
| Z2-R-SCRIPT-020 | No process-wide script state | SCRIPT/Runtime.md → "Script runtime internals"; SCRIPT/Runtime.md → "Script world internals" |
| Z2-R-SCRIPT-021 | Allocator hook and VM memory caps | SCRIPT/Runtime.md → "Allocator hook implementation"; SCRIPT/Runtime.md → "VM pool per world" |
| Z2-R-SCRIPT-022 | VM memory-cap trip is fatal | SCRIPT/Runtime.md → "World halt procedure"; SCRIPT.md → "Budget breach behaviour" |
| Z2-R-SCRIPT-023 | Worker VM total refused at world creation | SCRIPT/Runtime.md → "Script world internals" |
| Z2-R-SCRIPT-024 | GC tuning and bounded GC steps | SCRIPT/Runtime.md → "Garbage collection scheduling"; SCRIPT/Runtime.md → "VM creation steps" |
| Z2-R-SCRIPT-025 | Collection timing unobservable | SCRIPT/Bindings.md → "Environment construction"; SCRIPT/Runtime.md → "Garbage collection scheduling" |
| Z2-R-SCRIPT-026 | Pooled invocation threads | SCRIPT/Runtime.md → "VM creation steps"; SCRIPT/Runtime.md → "Chunk-system node body"; SCRIPT/Runtime.md → "Behaviour node body" |
| Z2-R-SCRIPT-027 | Invocation IDs | SCRIPT/PublicAPI.md → "Zenith_ScriptTypes header"; SCRIPT/Runtime.md → "Chunk-system node body" |
| Z2-R-SCRIPT-028 | VM teardown order | SCRIPT/Runtime.md → "Script runtime internals"; SCRIPT.md → "Construction and shutdown" |
| Z2-R-SCRIPT-029 | Cooked module manifest | SCRIPT/Compiler.md → "Module manifest format"; SCRIPT/Compiler.md → "Cook pipeline" |
| Z2-R-SCRIPT-030 | Literal require and cycle detection | SCRIPT/Bindings.md → "Module loader implementation"; SCRIPT/Compiler.md → "Cook pipeline" |
| Z2-R-SCRIPT-031 | Module generations and deep freeze | SCRIPT/Bindings.md → "Module loader implementation"; SCRIPT/HotReload.md → "Chunk-system generations" |
| Z2-R-SCRIPT-032 | Module kinds and declaration validation | SCRIPT/Bindings.md → "Module loader implementation"; SCRIPT/Runtime.md → "Script world internals"; SCRIPT/Compiler.md → "Cook pipeline" |
| Z2-R-SCRIPT-033 | Strict mode for simulation modules | SCRIPT/Compiler.md → "Compile procedure" |
| Z2-R-SCRIPT-034 | Role environments | SCRIPT/Bindings.md → "Environment construction"; SCRIPT/Bindings.md → "Per-VM thunk tables" |
| Z2-R-SCRIPT-035 | Mutable-globals compile option | SCRIPT/Compiler.md → "Compile procedure"; SCRIPT/Bindings.md → "Environment construction" |
| Z2-R-SCRIPT-036 | No fast call reaches a replaced builtin | SCRIPT/Compiler.md → "Fast-call listing"; SCRIPT.md → "Stage 1" |
| Z2-R-SCRIPT-037 | Zenith pcall re-raises aborts | SCRIPT/Bindings.md → "Environment construction" |
| Z2-R-SCRIPT-038 | Address-free tostring | SCRIPT/Bindings.md → "Environment construction"; SCRIPT/Bindings.md → "Userdata tags and view objects" |
| Z2-R-SCRIPT-039 | Sandboxed VMs and threads | SCRIPT/Runtime.md → "VM creation steps"; SCRIPT/Bindings.md → "Environment construction" |
| Z2-R-SCRIPT-040 | No capability outside bindings | SCRIPT/Bindings.md → "Environment construction"; SCRIPT/Bindings.md → "Low-library binding files"; SCRIPT/Bindings.md → "Per-VM thunk tables" |
| Z2-R-SCRIPT-041 | Parser limits | SCRIPT/Compiler.md → "Compile procedure"; SCRIPT/Compiler.md → "Cook pipeline" |
| Z2-R-SCRIPT-042 | Production compile options | SCRIPT/Compiler.md → "Compile procedure"; SCRIPT/HotReload.md → "Development reload sources" |
| Z2-R-SCRIPT-043 | Cooked bytecode container | SCRIPT/Compiler.md → "Cooked bytecode container writer"; SCRIPT/Compiler.md → "Cook pipeline" |
| Z2-R-SCRIPT-044 | Binding-set hash | SCRIPT/Compiler.md → "Cooked bytecode container writer"; SCRIPT/Bindings.md → "Definition emitter procedure" |
| Z2-R-SCRIPT-045 | Load-time bytecode verification and trusted mounts | SCRIPT/Compiler.md → "Load-time verification procedure"; SCRIPT/Bindings.md → "Module loader implementation" |
| Z2-R-SCRIPT-046 | Development and cooked bytecode identical | SCRIPT/HotReload.md → "Development reload sources"; SCRIPT/Compiler.md → "Compile procedure" |
| Z2-R-SCRIPT-047 | Re-cook rule and patch refusal | SCRIPT/Compiler.md → "Load-time verification procedure"; SCRIPT.md → "Stage 6" |
| Z2-R-SCRIPT-048 | Luau pin record | SCRIPT.md → "Stage 6"; SCRIPT/Compiler.md → "Load-time verification procedure" |
| Z2-R-SCRIPT-049 | Pin change re-qualification | SCRIPT.md → "Stage 6"; SCRIPT/Compiler.md → "Fast-call listing" |
| Z2-R-SCRIPT-050 | Shipping bytecode qualification run | SCRIPT.md → "Stage 6" |
| Z2-R-SCRIPT-051 | Binding ABI header in ZenithReflect | SCRIPT/PublicAPI.md → "Zenith_ScriptBinding header"; SCRIPT.md → "SCRIPT files in other libraries" |
| Z2-R-SCRIPT-052 | Registration template checks | SCRIPT/Bindings.md → "Binding table implementation" |
| Z2-R-SCRIPT-053 | Binding table sealing and IDs | SCRIPT/Bindings.md → "Binding table implementation"; SCRIPT/PublicAPI.md → "Zenith_ScriptBinding header" |
| Z2-R-SCRIPT-054 | Dispatcher algorithm | SCRIPT/Bindings.md → "Dispatcher implementation" |
| Z2-R-SCRIPT-055 | Lossless value marshalling | SCRIPT/Bindings.md → "Marshalling implementation" |
| Z2-R-SCRIPT-056 | Context and resource enforcement | SCRIPT/Bindings.md → "Dispatcher implementation"; SCRIPT/Bindings.md → "Per-VM thunk tables" |
| Z2-R-SCRIPT-057 | Buffered bindings respect their side-effect mask | SCRIPT/Transactions.md → "Side-effect routing" |
| Z2-R-SCRIPT-058 | Handle traits owned by the handle's library | SCRIPT/Bindings.md → "Marshalling implementation"; SCRIPT.md → "SCRIPT files in other libraries" |
| Z2-R-SCRIPT-059 | Bindings of libraries below Reflect | SCRIPT/Bindings.md → "Low-library binding files" |
| Z2-R-SCRIPT-060 | Side-effect consumers | SCRIPT/Bindings.md → "Side-effect consumer registration"; SCRIPT/Transactions.md → "Commit implementation" |
| Z2-R-SCRIPT-061 | Generated Luau definition outputs and runtime descriptors | SCRIPT/Bindings.md → "Definition emitter procedure"; SCRIPT/Bindings.md → "The @zenith prelude and @types modules"; SCRIPT.md → "Script source trees and cooked outputs" |
| Z2-R-SCRIPT-062 | Byte-stable generated definitions | SCRIPT/Bindings.md → "Definition emitter procedure"; SCRIPT.md → "Reconciliation items" |
| Z2-R-SCRIPT-063 | Hungarian name mapping | SCRIPT/Bindings.md → "Definition emitter procedure" |
| Z2-R-SCRIPT-064 | Cook type check | SCRIPT/Compiler.md → "Cook pipeline" |
| Z2-R-SCRIPT-065 | Address-keyed iteration lint | SCRIPT/Compiler.md → "Address-keyed iteration lint" |
| Z2-R-SCRIPT-066 | Behaviour declaration validation | SCRIPT/Bindings.md → "Module loader implementation"; SCRIPT/Compiler.md → "Declaration extraction" |
| Z2-R-SCRIPT-067 | Chunk system declaration and query | SCRIPT/Runtime.md → "Node registration and frame-graph placement"; SCRIPT/Runtime.md → "Chunk-system node body" |
| Z2-R-SCRIPT-068 | Script systems through the system registry | SCRIPT/Runtime.md → "Node registration and frame-graph placement"; SCRIPT.md → "Reconciliation items" |
| Z2-R-SCRIPT-069 | Schedule-compile checks for script systems | SCRIPT/Runtime.md → "Node registration and frame-graph placement" |
| Z2-R-SCRIPT-070 | Behaviour node placement and order | SCRIPT/Runtime.md → "Node registration and frame-graph placement"; SCRIPT/Runtime.md → "Behaviour node body" |
| Z2-R-SCRIPT-071 | Dependents start after the merge | SCRIPT/Runtime.md → "Node registration and frame-graph placement"; SCRIPT/Transactions.md → "Merge node body" |
| Z2-R-SCRIPT-072 | Disabled script nodes | SCRIPT/Runtime.md → "Tick-boundary handler"; SCRIPT/Runtime.md → "Chunk-system node body"; SCRIPT/Transactions.md → "Merge node body" |
| Z2-R-SCRIPT-073 | Behaviour instance directory | SCRIPT/Runtime.md → "Behaviour instance directory"; SCRIPT/PublicAPI.md → "Zenith_ScriptBehaviourComponent header"; SCRIPT.md → "Behaviour instance state machine" |
| Z2-R-SCRIPT-074 | Entity transaction boundary | SCRIPT/Transactions.md → "Invocation lifecycle"; SCRIPT/Transactions.md → "Failure handling by kind"; SCRIPT/Transactions.md → "Worked lifecycle traces" |
| Z2-R-SCRIPT-075 | Row trampoline and per-entity budget | SCRIPT/Transactions.md → "Invocation lifecycle"; SCRIPT/Runtime.md → "Chunk-system node body" |
| Z2-R-SCRIPT-076 | Transaction arena partition | SCRIPT/Transactions.md → "Transaction arena partition"; SCRIPT/Transactions.md → "Invocation layout in the arenas"; SCRIPT.md → "Reconciliation items" |
| Z2-R-SCRIPT-077 | Transaction arena growth failure is fatal | SCRIPT/Transactions.md → "Transaction arena partition"; SCRIPT/Runtime.md → "World halt procedure" |
| Z2-R-SCRIPT-078 | Write path | SCRIPT/Transactions.md → "Write path implementation" |
| Z2-R-SCRIPT-079 | First-write snapshots | SCRIPT/Transactions.md → "Snapshot capture and restore" |
| Z2-R-SCRIPT-080 | Per-row restore on failure | SCRIPT/Transactions.md → "Snapshot capture and restore"; SCRIPT/Transactions.md → "Failure handling by kind" |
| Z2-R-SCRIPT-081 | Copy-on-write shadows | SCRIPT/Transactions.md → "Copy-on-write shadow implementation" |
| Z2-R-SCRIPT-082 | Private side-effect buffers | SCRIPT/Transactions.md → "Side-effect routing"; SCRIPT.md → "Reconciliation items" |
| Z2-R-SCRIPT-083 | Provisional spawn handles | SCRIPT/Transactions.md → "Side-effect routing" |
| Z2-R-SCRIPT-084 | Per-entity memory cap | SCRIPT/Transactions.md → "Per-entity accounting implementation" |
| Z2-R-SCRIPT-085 | Cross-entity read enforcement | SCRIPT/Transactions.md → "Write path implementation"; SCRIPT/Bindings.md → "Userdata tags and view objects" |
| Z2-R-SCRIPT-086 | Merge-time acceptance | SCRIPT/Transactions.md → "Merge node body"; SCRIPT/Transactions.md → "Capacity acceptance across ECS and SCRIPT classes" |
| Z2-R-SCRIPT-087 | Commit cannot fail | SCRIPT/Transactions.md → "Commit implementation" |
| Z2-R-SCRIPT-088 | Merge chain in system order | SCRIPT/Transactions.md → "Merge node body"; SCRIPT/Runtime.md → "Node registration and frame-graph placement" |
| Z2-R-SCRIPT-089 | Invocations of one behaviour instance | SCRIPT/Transactions.md → "Merge node body"; SCRIPT/Runtime.md → "Behaviour node body" |
| Z2-R-SCRIPT-090 | Disable from the next tick boundary | SCRIPT/Runtime.md → "Tick-boundary handler"; SCRIPT.md → "Module state machine" |
| Z2-R-SCRIPT-091 | Commits never invalidate views | SCRIPT/Transactions.md → "Commit implementation" |
| Z2-R-SCRIPT-092 | View validation on every access | SCRIPT/Bindings.md → "Userdata tags and view objects" |
| Z2-R-SCRIPT-093 | Merge keys and logical invocation IDs | SCRIPT/Transactions.md → "Side-effect routing"; SCRIPT/Transactions.md → "Merge node body"; SCRIPT.md → "Determinism obligations" |
| Z2-R-SCRIPT-094 | Invocation ordinals | SCRIPT/Runtime.md → "Behaviour node body" |
| Z2-R-SCRIPT-095 | Counter-based script randomness | SCRIPT/Runtime.md → "Random draws" |
| Z2-R-SCRIPT-096 | Random key layout and format bump | SCRIPT/Runtime.md → "Random draws" |
| Z2-R-SCRIPT-097 | Event delivery order | SCRIPT/Runtime.md → "Behaviour node body" |
| Z2-R-SCRIPT-098 | Worker floating-point environment | SCRIPT/Tests.md → "Firewall and VM tests" |
| Z2-R-SCRIPT-099 | Determinism replay matrix | SCRIPT/Tests.md → "Determinism and fault-matrix tests" |
| Z2-R-SCRIPT-100 | Fault matrix | SCRIPT/Tests.md → "Determinism and fault-matrix tests" |
| Z2-R-SCRIPT-101 | Safepoint counting | SCRIPT/Runtime.md → "Interrupt callback implementation" |
| Z2-R-SCRIPT-102 | Budget scopes | SCRIPT/Runtime.md → "Protected call implementation"; SCRIPT.md → "Budget breach behaviour" |
| Z2-R-SCRIPT-103 | Abort values | SCRIPT/Runtime.md → "Interrupt callback implementation"; SCRIPT/Bindings.md → "Environment construction" |
| Z2-R-SCRIPT-104 | Native codegen parity | SCRIPT/Tests.md → "Benchmarks and harnesses"; SCRIPT.md → "Stage 1" |
| Z2-R-SCRIPT-105 | Wall-clock watchdog | SCRIPT/Runtime.md → "Watchdog internals"; SCRIPT/Runtime.md → "World halt procedure"; SCRIPT/Debugger.md → "Stop protocol" |
| Z2-R-SCRIPT-106 | Per-tick script counters reported | SCRIPT.md → "Counters"; SCRIPT/Transactions.md → "Per-entity accounting implementation" |
| Z2-R-SCRIPT-107 | Completion IDs and tickets | SCRIPT/Runtime.md → "Asynchronous result table"; SCRIPT/Transactions.md → "Commit implementation" |
| Z2-R-SCRIPT-108 | Tick-boundary publication | SCRIPT/Runtime.md → "Asynchronous result table"; SCRIPT/PublicAPI.md → "Zenith_ScriptAsync header"; SCRIPT.md → "Reconciliation items" |
| Z2-R-SCRIPT-109 | Replay publication at the recorded tick | SCRIPT/Runtime.md → "Asynchronous result table"; SCRIPT/PublicAPI.md → "Zenith_ScriptHost header" |
| Z2-R-SCRIPT-110 | Declared state schema registration | SCRIPT/HotReload.md → "State registration at startup"; SCRIPT/Compiler.md → "Declaration extraction" |
| Z2-R-SCRIPT-111 | State schema load errors | SCRIPT/Compiler.md → "Declaration extraction"; SCRIPT/HotReload.md → "State schema internals" |
| Z2-R-SCRIPT-112 | State proxy | SCRIPT/HotReload.md → "State proxy implementation" |
| Z2-R-SCRIPT-113 | Checkpoint rule | SCRIPT/HotReload.md → "Checkpoint serializer"; SCRIPT/Runtime.md → "Behaviour node body" |
| Z2-R-SCRIPT-114 | Checkpoint format | SCRIPT/HotReload.md → "Checkpoint serializer" |
| Z2-R-SCRIPT-115 | Checkpoint failure and slot swap | SCRIPT/HotReload.md → "Checkpoint serializer"; SCRIPT/Transactions.md → "Commit implementation" |
| Z2-R-SCRIPT-116 | Checkpoint slot pools | SCRIPT/HotReload.md → "Checkpoint slot pools" |
| Z2-R-SCRIPT-117 | Restore algorithm | SCRIPT/HotReload.md → "Restore implementation" |
| Z2-R-SCRIPT-118 | Stale references on restore | SCRIPT/HotReload.md → "Restore implementation" |
| Z2-R-SCRIPT-119 | OnRecover | SCRIPT/Runtime.md → "Behaviour node body"; SCRIPT/HotReload.md → "Last-good restore" |
| Z2-R-SCRIPT-120 | Behaviour state in saves | SCRIPT/HotReload.md → "Behaviour state in saves"; SCRIPT/PublicAPI.md → "Zenith_ScriptStateSnapshot header" |
| Z2-R-SCRIPT-121 | Checkpoint decoder is fuzzed | SCRIPT/HotReload.md → "Restore implementation"; SCRIPT/Tests.md → "Fuzz targets" |
| Z2-R-SCRIPT-122 | Quarantine | SCRIPT/Transactions.md → "Quarantine store"; SCRIPT.md → "Behaviour instance state machine" |
| Z2-R-SCRIPT-123 | Recovery policy | SCRIPT/HotReload.md → "Quarantine and recovery scheduling"; SCRIPT/PublicAPI.md → "Zenith_ScriptRecovery header" |
| Z2-R-SCRIPT-124 | Last-good restore | SCRIPT/HotReload.md → "Last-good restore" |
| Z2-R-SCRIPT-125 | Hot-reload producers | SCRIPT/HotReload.md → "Hot-reload staging area"; SCRIPT/HotReload.md → "Development reload sources" |
| Z2-R-SCRIPT-126 | Staged hot reload with rollback | SCRIPT/HotReload.md → "ApplyPendingReloads procedure"; SCRIPT/HotReload.md → "Schedule commit in the prologue"; SCRIPT/HotReload.md → "Rollback procedure" |
| Z2-R-SCRIPT-127 | Hot-reload go-live | SCRIPT/HotReload.md → "Schedule commit in the prologue"; SCRIPT/HotReload.md → "Go-live at the tick boundary" |
| Z2-R-SCRIPT-128 | Chunk-system reload generations | SCRIPT/HotReload.md → "Chunk-system generations" |
| Z2-R-SCRIPT-129 | Migrate chain and fixtures | SCRIPT/HotReload.md → "Migration execution" |
| Z2-R-SCRIPT-130 | Schema script migration host | SCRIPT/HotReload.md → "Schema migration script host" |
| Z2-R-SCRIPT-131 | Script hot-reload latency | SCRIPT/HotReload.md → "Development reload sources"; SCRIPT/Tests.md → "Benchmarks and harnesses" |
| Z2-R-SCRIPT-132 | Stateless check interface | SCRIPT/Compiler.md → "Cook pipeline"; SCRIPT/PublicAPI.md → "Zenith_ScriptStateless header"; SCRIPT/Compiler.md → "Stateless check prototype" |
| Z2-R-SCRIPT-133 | Adversarial corpus rejected at load | SCRIPT/Compiler.md → "Stateless check prototype"; SCRIPT/Tests.md → "Compiler and cook tests" |
| Z2-R-SCRIPT-134 | Accepted corpus loads | SCRIPT/Compiler.md → "Stateless check prototype"; SCRIPT/Tests.md → "Compiler and cook tests" |
| Z2-R-SCRIPT-135 | D-LUAU-STATELESS evaluation and ruling | SCRIPT/Compiler.md → "Stateless alternatives prototypes"; SCRIPT.md → "Stage 1" |
| Z2-R-SCRIPT-136 | Behaviour modules share no mutable tables | SCRIPT/Compiler.md → "Stateless check prototype" |
| Z2-R-SCRIPT-137 | Stateful library functions removed | SCRIPT/Bindings.md → "Environment construction"; SCRIPT/Compiler.md → "Stateless check prototype" |
| Z2-R-SCRIPT-138 | Rejected libraries reject their importers | SCRIPT/Compiler.md → "Cook pipeline"; SCRIPT/Compiler.md → "Stateless check prototype" |
| Z2-R-SCRIPT-139 | DAP server exposure and threads | SCRIPT/Debugger.md → "Scope of this file"; SCRIPT/Debugger.md → "Transport and framing"; SCRIPT/Debugger.md → "Request handling" |
| Z2-R-SCRIPT-140 | Breakpoint fan-out | SCRIPT/Debugger.md → "Breakpoint storage and fan-out"; SCRIPT/Debugger.md → "Stop protocol" |
| Z2-R-SCRIPT-141 | Debugger stops change no result | SCRIPT/Debugger.md → "Stop protocol"; SCRIPT/Debugger.md → "Stop loop"; SCRIPT/Debugger.md → "Debugger and simulation dispatch" |
| Z2-R-SCRIPT-142 | D-LUAUDBG client and request set | SCRIPT/Debugger.md → "Request handling"; SCRIPT/Debugger.md → "Stepping"; SCRIPT/Debugger.md → "Evaluate restrictions"; SCRIPT.md → "Stage 1" |
| Z2-R-SCRIPT-143 | S24 debugger attach | SCRIPT/Debugger.md → "Attaching on Windows and the S24" |
| Z2-R-SCRIPT-144 | Profiling zones | SCRIPT/Runtime.md → "Profiling implementation"; SCRIPT.md → "Profiling zones summary" |
| Z2-R-SCRIPT-145 | On-screen profiler rows | SCRIPT.md → "On-screen profiler rows summary"; SCRIPT.md → "Counters" |
| Z2-R-SCRIPT-146 | Sampling profiler | SCRIPT/Debugger.md → "Sampling profiler implementation" |
| Z2-R-SCRIPT-147 | Misuse errors | SCRIPT.md → "Luau errors versus asserts"; SCRIPT/Runtime.md → "Protected call implementation" |
| Z2-R-SCRIPT-148 | Misuse catalogue trigger scripts | SCRIPT/Tests.md → "Module binding and sandbox tests"; SCRIPT.md → "Test kinds and locations" |
| Z2-R-SCRIPT-149 | Input action names resolved at load | SCRIPT/Bindings.md → "Load-time name references" |
| Z2-R-SCRIPT-150 | World halt reporting | SCRIPT/Runtime.md → "World halt procedure"; SCRIPT.md → "Script world state machine"; SCRIPT/PublicAPI.md → "Zenith_ScriptHost header" |
| Z2-R-SCRIPT-151 | Representative game modules load | SCRIPT.md → "Stage 4"; SCRIPT/Tests.md → "Compiler and cook tests" |
| Z2-R-SCRIPT-152 | Gameplay rules stay game-side | SCRIPT.md → "What SCRIPT does not own"; SCRIPT.md → "Stage 4" |
| Z2-R-SCRIPT-153 | Stage 1 script budget allocation | SCRIPT.md → "Rows SCRIPT owns"; SCRIPT.md → "Row needs recorded as pending"; SCRIPT/Tests.md → "Benchmarks and harnesses" |
| Z2-R-SCRIPT-154 | No third-party script loading | SCRIPT.md → "What SCRIPT does not own"; SCRIPT/Compiler.md → "Load-time verification procedure" |
| Z2-R-SCRIPT-155 | Debugger stop bracket and parking | SCRIPT/Debugger.md → "Stop protocol"; SCRIPT/Debugger.md → "Stop loop"; SCRIPT.md → "Lock-order entries" |
| Z2-R-SCRIPT-156 | Watchdog table capacity and world index | SCRIPT/Runtime.md → "Watchdog internals"; SCRIPT/Runtime.md → "Script world internals"; SCRIPT/PublicAPI.md → "Zenith_ScriptTypes header" |
| Z2-R-SCRIPT-157 | Debugger optional at runtime creation | SCRIPT/Runtime.md → "Script runtime internals"; SCRIPT/PublicAPI.md → "Zenith_ScriptRuntime header"; SCRIPT/Debugger.md → "Scope of this file" |
| Z2-R-SCRIPT-158 | Partial module load failure | SCRIPT/Runtime.md → "Script world internals"; SCRIPT.md → "Missing and corrupt input" |
| Z2-R-SCRIPT-159 | Unconsumed asynchronous results traced | SCRIPT/Runtime.md → "Asynchronous result table"; SCRIPT.md → "Counters" |
| Z2-R-SCRIPT-160 | No script console in Shipping | SCRIPT/PublicAPI.md → "Zenith_ScriptWorld header"; SCRIPT.md → "Configuration keys" |
| Z2-R-SCRIPT-161 | Merge and tick-boundary work touch no Luau object | SCRIPT/Transactions.md → "Merge node body"; SCRIPT/Runtime.md → "Tick-boundary handler"; SCRIPT/HotReload.md → "Go-live at the tick boundary" |
| Z2-R-SCRIPT-162 | Retention ceiling derived from existing rows | SCRIPT/Transactions.md → "Retention ceiling derivation"; SCRIPT/Transactions.md → "Transaction arena partition"; SCRIPT.md → "Row needs recorded as pending" |
| Z2-R-SCRIPT-163 | Schedule commit in the prologue | SCRIPT/HotReload.md → "Schedule commit in the prologue"; SCRIPT/Runtime.md → "Node registration and frame-graph placement"; SCRIPT/PublicAPI.md → "Zenith_ScriptRuntime header" |
| Z2-R-SCRIPT-164 | Main-VM release list drained and bounded | SCRIPT/Runtime.md → "Script world internals"; SCRIPT/Runtime.md → "Behaviour node body"; SCRIPT/HotReload.md → "Schedule commit in the prologue" |
| Z2-R-SCRIPT-165 | Invocations of one instance compose container writes | SCRIPT/Transactions.md → "Copy-on-write shadow implementation" |
| Z2-R-SCRIPT-166 | Script results of handled source kinds and decoded tickets | SCRIPT/Runtime.md → "Asynchronous result table"; SCRIPT/PublicAPI.md → "Zenith_ScriptBinding header"; SCRIPT/Transactions.md → "Commit implementation" |
| Z2-R-SCRIPT-167 | Recovery invocations consume no invocation ordinal | SCRIPT/HotReload.md → "Recovery invocations and exact restore"; SCRIPT/Runtime.md → "Behaviour node body"; SCRIPT/Transactions.md → "Merge node body" |
| Z2-R-SCRIPT-168 | Saved behaviour state restores its recorded lifecycle | SCRIPT/HotReload.md → "Recovery invocations and exact restore"; SCRIPT/HotReload.md → "Behaviour state in saves"; SCRIPT/PublicAPI.md → "Zenith_ScriptStateSnapshot header" |
| Z2-R-SCRIPT-169 | Retiring a result table discards and counts | SCRIPT/Runtime.md → "Asynchronous result table"; SCRIPT/PublicAPI.md → "Zenith_ScriptAsync header"; SCRIPT/PublicAPI.md → "Zenith_ScriptWorld header" |
| Z2-R-SCRIPT-170 | Asynchronous state is listed and restored for saves | SCRIPT/Runtime.md → "Asynchronous result table"; SCRIPT/PublicAPI.md → "Zenith_ScriptAsync header" |
| Z2-R-SCRIPT-171 | Publication resolvers rewrite drained live completions | SCRIPT/Runtime.md → "Asynchronous result table"; SCRIPT/PublicAPI.md → "Zenith_ScriptAsync header" |
| Z2-R-SCRIPT-172 | Replay adopts the recorded next completion ID | SCRIPT/Runtime.md → "Asynchronous result table"; SCRIPT/PublicAPI.md → "Zenith_ScriptHost header" |
| Z2-R-SCRIPT-173 | Module sources enter the table through the runtime's sink | SCRIPT/Runtime.md → "Asynchronous result table"; SCRIPT/Runtime.md → "Source kind values" |
| Z2-R-SCRIPT-174 | Committed checkpoints feed the tick hash | SCRIPT/Runtime.md → "Committed checkpoints in the tick hash"; SCRIPT/PublicAPI.md → "Zenith_ScriptWorld header" |
| Z2-R-SCRIPT-175 | Buffered bindings read a per-function user datum | SCRIPT/Bindings.md → "Dispatcher implementation"; SCRIPT/PublicAPI.md → "Zenith_ScriptBinding header" |
| Z2-R-SCRIPT-176 | Fixed-array fields of returned structs marshal as arrays | SCRIPT/Bindings.md → "Marshalling implementation" |
| Z2-R-SCRIPT-177 | Bindings carry the Luau API classification | SCRIPT/Bindings.md → "Binding stability and deprecation"; SCRIPT/PublicAPI.md → "Zenith_ScriptBinding header" |
| Z2-R-SCRIPT-178 | A call to a deprecated binding is diagnosed at load | SCRIPT/Bindings.md → "Deprecated call sites"; SCRIPT/Compiler.md → "Module manifest format" |
| Z2-R-SCRIPT-179 | The definitions record stability, the engine version and a binding listing | SCRIPT/Bindings.md → "Binding listing"; SCRIPT/Bindings.md → "Definition emitter procedure"; SCRIPT/PublicAPI.md → "Zenith_ScriptCompileOptions header" |
| Z2-R-SCRIPT-180 | Thirty-two asynchronous source kinds | SCRIPT/Runtime.md → "Asynchronous result table"; SCRIPT/Runtime.md → "Source kind values" |
| Z2-R-SCRIPT-181 | The XR recentre side effect | SCRIPT/Bindings.md → "Side-effect consumer registration"; SCRIPT/Transactions.md → "Side-effect routing" |

## Reconciliation items

Additions and resolutions this design makes against the specification or other areas' designs. Each names its cross-area row (`C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md`) and, where a core document changes, its pending cross-doc row (`C:/dev/Zenith2-drafts/reviews/CROSS_DOC_ITEMS.md`); every site that depends on a pending row is fenced `<!-- assumption: pending core-doc (Xn) -->` in the file that uses it. Until a row lands, the core document is the signed-off text and this design's form is the proposal the row applies.

| # | Item | Specification touched | Rows |
|---|---|---|---|
| S1 | `Zenith_ScriptError` becomes a 32-byte record with `m_eCode` first (`Zenith_ScriptErrorCode`, numbered from 1, declared in `Zenith_ScriptError.h`) and a diagnostic sequence; message and traceback move to `Zenith_ScriptDiagnostic` in a diagnostic store. `Zenith_ScriptThunkError` becomes an 8-byte record with its own code enumeration `Zenith_ScriptThunkErrorCode`, its message moving to `Zenith_ScriptCallFrame::m_acErrorMessage`; `Zenith_ScriptInvocationContext::GetErrorMessage` is added; `Seal` returns the enumeration-form `Zenith_ScriptBindingError`. Reason: [Z2-N-CONTRACT-480] rejects the 2.5 KB and 256-byte forms. [Z2-N-CONTRACT-482]: each error type has its own name and code enumeration, so no Contracts change is needed (PublicAPI.md → "Zenith_ScriptError header") | Scripting.md → "The protected entry"; Scripting/Bindings.md → "Public binding types"; Scripting/PublicTypes.md | SC1; X115, X151 |
| S2 | `Zenith_ScriptWorld::RegisterScriptNodes` takes `Zenith_SystemRegistry&` (ECS R10, Architecture's startup step 17) instead of `Zenith_FrameGraphBuilder&` | Scripting/LibraryLayout.md; `Zenith2/Docs/Design/GRAPH.md` mentions of `RegisterScriptNodes` | SC2; X86 |
| S3 | Behaviour-state types register at startup step 10, after `RegisterSchemaBlob` and before `Freeze`, from the cooked manifest's state-schema records through `Zenith_ScriptRegisterBehaviourStateTypes(Zenith_TypeRegistry&, const Zenith_ScriptModuleManifest&)` (ECS's ruling, `Zenith2/Docs/Design/ECS.md` → "Behaviour-state type registration"; E12(a), E18(a)); `Zenith_BehaviourComponent` stays in the step-6 engine types through `Zenith_ScriptRegisterEngineTypes`. The cook-time registration requires `State`, `Name`, `Phase` and `StateVersion` in static (literal) form; the live declaration is compared by shape hash at load | Scripting.md [Z2-N-SCRIPT-382]; Architecture/CompositionRoot.md step 10, [Z2-N-ARCH-847] | SC3, SC15; X86, X122 |
| S4 | System order is ECS's registration index; SCRIPT registers script systems in Scripting.md's order (topological with source-ID ties, then behaviour-module order), so both definitions agree | Scripting.md [Z2-N-SCRIPT-152]; ECS "Registration index is the system order" | SC2 |
| S5 | Transaction memory is `Zenith_Arena` memory partitioned per (script system, slot): a pre-reserved scratch arena per slot for the growing structures, reset to the invocation's marker, and retention arenas per (system, slot), claimed at the system's first invocation on the slot and reset by its merge, with the growth bound of Transactions.md → "Transaction arena partition"; each retention arena's cap is derived from existing rows (Transactions.md → "Retention ceiling derivation"), so no new budget row is needed and reaching the cap is `ZENITH_CHECK`; no custom allocator and no concurrent container | Scripting.md [Z2-N-SCRIPT-211], [Z2-N-SCRIPT-224], [Z2-N-SCRIPT-225], merge step 6; TransactionStructures.md `m_xArenaMarker` comment | SC5; X116, X151 (X150 withdrawn) |
| S6 | ECS-kind side effects (structural, field commands, events, contributions) are carried in the entity's ECS command buffer and accepted or rejected through `Zenith_MergeContext`; SCRIPT-held kinds (audio, VFX, save, async) are SCRIPT records delivered to `Zenith_ScriptSideEffectConsumer`s (new ABI interface, `SetSideEffectConsumer`, `DeclareAsyncSource`, `GetSideEffectConsumer`). Container and string fields keep Scripting.md's visibility, after the merge point: strings are interned and written in place at the merge; containers are swapped at the merge through `Zenith_MergeContext::ReplaceContainer`, the ECS member SC21 requested (cross-doc row X192 applied; no fallback remains); a subsequent invocation of an instance in the same node chains its shadow on the earlier one | Scripting.md → "Commit splice algorithm" steps 1–2, table "What is transactional"; Scripting/Bindings.md → "Public binding types" | SC6, SC21; X117, X151 |
| S7 | The per-world pending-result set is `Zenith_AsyncResultTable` owned by `Zenith_ScriptWorld`, with publication observer, source and handler interfaces for the replay recorder, the replay reader and the PLAT bridge; the handler receives the tick boundary's `Zenith_TickBoundaryAccess&` (E12(d)); a handler's event append is the ECS addition SC20 requests, with PLAT's interim | Architecture.md → "Multiplayer foundations"; PLAT's frame-graph placement | SC7, SC20; X118 |
| S8 | A published script result stays until consumed by a committed invocation or its owner is destroyed or quarantined, and its in-flight capacity returns then, not at publication; a result never read is traced by `script.async.published_unconsumed.<kind>` and `SCRIPT_ASYNC_CAPACITY_HELD` | Scripting.md table "Capacity classes", async row | SC8; X118, X121 |
| S9 | Z2-B-SCRIPT-007 is applied per VM per tick | Budgets.md "per frame" unit, [Z2-N-BUDGET-295] | SC9; X119 |
| S10 | `Zenith_ModuleStartContext::RegisterScriptBindings` names the table `Zenith_ScriptBindingTable` (Architecture's `Zenith_BindingTable`) and gains `SetScriptSideEffectConsumer` and `DeclareScriptAsyncSource` | Architecture/CompositionRoot.md | SC10; X117(c) |
| S11 | Luau declarations of reflected types follow [Z2-N-ARCH-611] (stable-name order, fields by ID; X70); binding functions follow `(namespace, Luau name)` byte order | Scripting/Bindings.md [Z2-N-SCRIPT-621] | SC11; X86 |
| S12 | Declaration changes to Scripting's companions: `Zenith_ScriptWatchdogDesc` and `Create(const Zenith_ScriptWatchdogDesc&)`, `Register`'s `SCRIPT_ERROR_WATCHDOG_TABLE_FULL`, the capacity rule, no `SuspendWorld` (a stopped VM's own flag and a rebase of its start time replace it, and Scripting.md → "Wall-clock watchdog" step 4 reads per VM), the DAP thread ID by world tag; the `pxReport` parameters of `Zenith_ScriptCompileModule` and `Zenith_ScriptCheckStateless`; `Zenith_ScriptWorldDesc`'s members (`m_uFrameThreadCount` replacing `m_uWorkerCount`) and `GetDisabledModuleCount`; `Zenith_ScriptRuntimeDesc`'s members and `IsDebuggerAvailable`; the `Zenith_ScriptCounters` fields; catalogue entries `ZS1807` and the diagnostics `SCRIPT_STATE_RESTORE_FAILED`, `SCRIPT_DEBUGGER_UNAVAILABLE`, `SCRIPT_ASYNC_CAPACITY_HELD`; `Zenith_ScriptRuntime::CommitPendingReloads` | Scripting/PublicTypes.md, Scripting/LibraryLayout.md, Scripting.md → "Development compilation", "Load-time interface", "DAP server design", "Wall-clock watchdog", Scripting/MisuseCatalogue.md | SC12; X121, X151, X158 |
| S13 | A development-build debugger stop blocks only inside the bracket `Zenith_MarkThreadAsJobWorker(false)` … `(true)` — in this design the bracket restores the flag `Zenith_IsJobWorkerThread()` reported on entry, which is `true` in every job body and keeps an out-of-tick main-thread stop unflagged; not-yet-started invocations of the stopped world park at the world's gate between invocations; the tick commit is held by the graph; the count has one increment site and every change of it, the notify and each parker's predicate check are under the DAP mutex; deadlock freedom rests on the DAP thread, which holds 710 only for bookkeeping (Debugger.md → "Stop protocol") | Contracts.md [Z2-N-CONTRACT-315], lock ordering table row 710; Architecture/CPUFrameGraph.md [Z2-N-ARCH-382], [Z2-N-ARCH-394]; Scripting.md → "DAP server design" step 4 | SC14, SC22; X120 (Q44 ruled (a)) |
| S14 | Behaviour merge nodes and the tick-boundary adapter run with ECS's `ANY` affinity and touch no Luau object; main-VM releases and fresh `self`/state proxies of a tick-boundary restore are deferred to the module's next behaviour node (and the prologue's drain) on the main thread, before `OnRecover`; no tagged ordering bullet changes ([Z2-N-SCRIPT-394] holds) | Scripting.md → "Last-good restore" step 2; Scripting.md → "Hot reload of behaviour modules" step 3 | SC4; X151 |
| S15 | Hot-reload go-live commits the system reload scope in prologue step 6 through the new `Zenith_ScriptRuntime::CommitPendingReloads` (ZenithRuntime calls it in every frame, after the window closed), so the recompile runs in step 8 of the same frame and the tick boundary only swaps generations; every failure path releases the staged objects once and nothing else (HotReload.md → "Schedule commit in the prologue", "Rollback procedure") | Scripting.md → "Hot reload of behaviour modules" step 3, "Hot reload of chunk-system modules" step 3; Scripting/PublicTypes.md → "Zenith_ScriptRuntime"; Architecture/CompositionRoot.md [Z2-N-ARCH-863] | SC24; X158 |
| S16 | The tick-boundary adapter calls no `MAIN_THREAD` ECS member: a module's disable is SCRIPT's own `m_bDisabled`, exact to the tick and read by every node and merge body of the module; ECS's matching disable is the `ANY`-safe request `Zenith_TickBoundaryAccess::RequestSystemEnabled` (Runtime.md → "Tick-boundary handler"; cross-doc row X192 applied) | — (ECS's `Zenith2/Docs/Design/ECS/Scheduling.md` step 3 already describes handler disables) | SC23 |
| S17 | Orchestrator ruling Q82 (b): a recovery invocation (`OnRecover`) consumes no invocation ordinal and records, draws and is merged under the reserved `uSCRIPT_RECOVERY_ORDINAL` (accepted before the instance's ordinary invocations of the node); only behaviours declaring `OnRecover` ever enter PENDING_RECOVER; `GetStateSnapshot` holds every instance past NEW with `m_bRunSuspended`, `m_bRecoveryPending` and `m_bQuarantinedOrDisabled`, and `RestoreSavedStates` restores the recorded lifecycle, so a capture with no suspended `Run` and no quarantined or disabled entry continues bit-identically (Design/SCRIPT/HotReload.md → "Recovery invocations and exact restore"; Z2T-SCRIPT-114) | Scripting.md [Z2-N-SCRIPT-274], [Z2-N-SCRIPT-394], → "Restore algorithm" step 6 | SC25 (answers SAVE's SV23); X267 (applied) |
| S18 | The single module publication sink (orchestrator ruling Q76 (a)) reaches the table through ZenithRuntime's forwarder; the table gains retirement, counts, save listing and restore, the phase-11 save allocation, replay adoption of the recorded next completion ID (Q89 (a)), the publication resolver's implementation (X271), the committed-checkpoint hash member, the per-function binding user datum and fixed-array struct marshalling; registry values 12 to 14 are assigned (CINE, AI, PACK) | Scripting/PublicTypes.md; Scripting/LibraryLayout.md; Scripting/Bindings.md; Scripting.md registry and [Z2-N-SCRIPT-341] | GA10, SV3, ST21, TD22, UI1, AN1, CN2, AI18, PK13, ON4; X345, X346, X347 (applied by core-doc patch f) |
| S19 | Post-signoff patch 2026-10-17 (ruling MAINT-R1-RULINGS R4): every binding carries MAINT's Luau classification (since stage, stability, experimental-until stage, deprecation, registrant library), checked at `Seal` and hashed except the library; a call to a deprecated binding is recorded by the cook and reported at world load as the warning `SCRIPT_BINDING_DEPRECATED_CALL` (ZS1808); the definition emitter writes the stability comment lines, the engine version in the `zenith.d.luau` header and the binding listing `bindings.json`; `Zenith_ScriptEngineVersionHash` is the one producer of the bytecode's engine version hash | Scripting/Bindings.md "Public binding types" and "Outputs"; Scripting/MisuseCatalogue.md "Diagnostics that are not Luau errors"; Scripting/PublicTypes.md (the manifest accessor) | MT2, SH14, SC27; X451 |
| S20 | Post-signoff patch 2026-10-17 (ruling ASYNC-SOURCE-15-16): registry values 15 (`ASYNC_SOURCE_NETWORK`) and 16 (`ASYNC_SOURCE_XR`) are assigned and the source-kind capacity rises from 16 to 32, mirrored as `uSCRIPT_ASYNC_SOURCE_KIND_CAPACITY` in the binding ABI; the Stage 7 side-effect kind `SIDE_EFFECT_KIND_XR_RECENTRE` is appended | Scripting.md registry and [Z2-N-SCRIPT-924]; Scripting/PublicTypes.md; Scripting/Bindings.md; Architecture.md "Multiplayer foundations" | NT2, SP7, SC28; X392, X449, X450 |
| S21 | A ticket-result decoder's struct field may point into its payload; `Result()` copies it into a frozen Luau table with 1-based fixed arrays, and a struct field outside the payload or of an unregistered type halts the world as an internal inconsistency | none (Design/SCRIPT/Bindings.md → "Struct fields of a decoded ticket result") | AI4 (e), AIB7 |
| S22 | The watchdog's hang report calls `Zenith_CrashHandler::WriteHangReport` before the fatal when the descriptor carries a handler, composed in a preallocated buffer, and logs the same text in both branches | none (Runtime.md → "Watchdog internals"; Scripting/PublicTypes.md's watchdog contract already states the branch) | DV14, SB20, DvB16 |
