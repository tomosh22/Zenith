# Zenith2 design — MOD Engine modules and projects

The design of the in-repo game model: how a game directory under `Zenith2/Games/<Name>/` becomes one statically linked executable, how optional engine modules are selected in `game.json` and registered by the composition root, how `ZenithRuntime` and the Player start, run and stop a game, and how a new game is created without writing C++. Workers implement against this document family; the requirement catalogue is `Zenith2/Docs/Requirements/MOD.md`.

A line `<!-- elaboration -->` marks a detail designed here beyond the core documents. A block `<!-- assumption: pending <AREA> -->` states an interface assumed from an area whose design document is not yet signed off; each such block is also recorded in `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md`. Design documents are not normative-scanned; every obligation below traces to a `Z2-R-MOD-NNN` (see "Requirement index").

## Purpose and scope

### What MOD owns

| Piece | Where | Specified in |
|---|---|---|
| the in-repo game model: `Zenith2/Games/<Name>/`, its allowed entries, one executable per game | `Zenith2/Games/`, `Zenith2/CMake/ZenithAddGame.cmake` | this file, `Zenith2/Docs/Design/MOD/GameBuild.md` |
| `zenith_add_game` and the `game.json` schema, validator and messages | `Zenith2/CMake/ZenithAddGame.cmake` | `Zenith2/Docs/Design/MOD/GameBuild.md` |
| the module and backend mapping tables that `zenith_add_game` resolves | `Zenith2/CMake/ZenithAddGame.cmake` | `Zenith2/Docs/Design/MOD/GameBuild.md` → "Module to target mapping", "Backend to target mapping" |
| the generated composition TU and the `composition.json` record | `${ZENITH_OUT}/Games/<Name>/` | `Zenith2/Docs/Design/MOD/GameBuild.md` → "Generated composition TU template"; this file |
| the composition root: `ZenithRuntime` (startup, module hosting, shutdown, the frame-loop driver, the engine stage nodes, exit codes, the command line) and `ZenithPlayer` (the entry points) | `Zenith2/Engine/ZenithRuntime/`, `Zenith2/Player/` | this file, citing Zenith2/Docs/Architecture.md → "Composition root and Player" |
| the Android shared object and Gradle wiring | `Zenith2/Player/GradleTemplate/`, `Zenith2/CMake/ZenithAddGame.cmake` | `Zenith2/Docs/Design/MOD/GameBuild.md` → "Android Gradle wiring" |
| project creation: `ZenithNewGame`, `ZenithNewGameScaffold`, templates, `game.json` migration | `Zenith2/Tools/NewGame/`, `Zenith2/Tools/NewGameScaffold/` | `Zenith2/Docs/Design/MOD/ProjectCreation.md` |
| the proving game `Games/Minimal` and the module and backend selections of every repository game | `Zenith2/Games/*/game.json` | this file, "Repository games" |
| the Stage 0 scaffold ticket's contents (it is parented to the MOD epic) | the tree root | this file, "Stage 0 scaffold" |

MOD owns the composition root's wiring and lifecycle. It does not own what the wired subsystems do: the frame graph and scheduler are GRAPH and JOBS (Zenith2/Docs/Architecture.md → "CPU frame graph and scheduler"), frame phases and lifecycle commands (including world transitions) are GAME (Zenith2/Docs/Architecture.md → "Frame phases and authority", "Lifecycle commands"), render extraction's content is RPROD (Zenith2/Docs/Rendering.md → "Frame anatomy common prologue"), script bindings of the low libraries are SCRIPT (Zenith2/Docs/Scripting.md → "Modules, bindings and generated types"), the virtual filesystem and configuration are PLAT (Zenith2/Docs/Architecture.md → "Platform layer"), and each module's content is its area's. Files of `ZenithRuntime` whose content another area specifies are marked in "ZenithRuntime files".

### Exclusions

| Excluded | Decision | Why it stays excluded | What enforces it |
|---|---|---|---|
| dynamic plugins (DLL or shared-object modules loaded at run time) | Z2-D-024 | a second link model, a binary-compatibility surface and an unload problem, for games that all live in one tree | `ForbiddenCheck` rejects `LoadLibrary*` and `dlopen` outside `ZenithPlatform/Internal/` and `ZenithRHI_*/Internal/` ([Z2-N-ARCH-274]); a module is only ever `KIND MODULE`, a static library ([Z2-N-ARCH-166]) |
| an installed SDK | Z2-D-002 | games outside the tree escape the gates | there is no install target for engine libraries and no exported CMake package; `zenith_add_game` exists only inside the tree |
| out-of-repo projects | Z2-D-002 | the same | `zenith_add_game` requires its directory to be `Zenith2/Games/<Name>/` (step 1 of Zenith2/Docs/Design/MOD/GameBuild.md → "Step by step") |
| third-party modding | Z2-D-075 | needs run-time code loading, a stable script API across releases and an untrusted-content boundary, each contradicting settled choices | no runtime path mounts, loads or executes content outside the package, patch, loose and user mounts, and the user mount never provides content ([Z2-N-ARCH-264]) |

### Inventory items covered

| Item | Summary | Classification | Stage |
|---|---|---|---|
| Z2-I-MOD-01 | in-repo games under `Zenith2/Games/` statically linking the engine | required | S2 (model specified S1) |
| Z2-I-MOD-02 | `zenith_add_game` | required | S2 |
| Z2-I-MOD-03 | `game.json`: module selection | required | S2 |
| Z2-I-MOD-04 | `game.json`: backend selection | required | S2 |
| Z2-I-MOD-05 | `game.json`: platform selection | required | S2 |
| Z2-I-MOD-06 | static optional modules | required | S2 |
| Z2-I-MOD-07 | composition-root registration of modules | required | S2 |
| Z2-I-MOD-08 | project creation CLI with no C++ | required | S2 |
| Z2-I-MOD-09 | project creation wizard with no C++ | required | S4 |
| Z2-I-MOD-10 | dynamic plugins | excluded | — |
| Z2-I-MOD-11 | installed SDK | excluded | — |
| Z2-I-MOD-12 | out-of-repo projects | excluded | — |
| Z2-I-MOD-13 | third-party modding | excluded | — |

### Document family

| File | Holds | Its `##` headings |
|---|---|---|
| `Zenith2/Docs/Design/MOD.md` (this file) | the entry: scope, dependencies, layout, the composition-root API, formats, algorithms, the threading and error contract, diagnostics, failure handling, tests, stages, decisions, the requirement index, the repository games | the headings of this file |
| `Zenith2/Docs/Design/MOD/GameBuild.md` | `zenith_add_game`'s CMake implementation, `game.json` schema v1 and its messages, the mapping tables, the generated TU, Android Gradle wiring, the CMake fixture tests | zenith_add_game implementation · Module to target mapping · Backend to target mapping · game.json schema v1 · Generated composition TU template · Android Gradle wiring · CMake fixture tests |
| `Zenith2/Docs/Design/MOD/RuntimeSeams.md` | the single module publication sink (orchestrator ruling Q76 (a)), the world attach context lookups, the world-host seams GAME's world host calls, the any-thread test-failure entry, the start context's overlay access, and their tests | Scope of this file · Module publication sink · World attach context lookups · World host seams · Test assertion failures and world shutdown requests · Start context registrations · Start context overlay access · Runtime test seams for modules · Seam tests |
| `Zenith2/Docs/Design/MOD/ProjectCreation.md` | `ZenithNewGame`, `ZenithNewGameScaffold`, templates, the creation algorithm, the C++ validator, `game.json` migration, the wizard boundary, the project-creation tests | ZenithNewGame command line · ZenithNewGameScaffold library API · Templates · Scaffold algorithm · Validation in C++ · game.json migration · Editor wizard boundary · Project creation tests |

### Reconciliations with the core documents

<!-- elaboration -->
Implementing Zenith2/Docs/Architecture/CompositionRoot.md exactly as written runs into five conflicts with other core documents (R1–R5) and needs one addition (R6). This design resolves each as below and records them in `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md` (X48 in the cross-document queue), so the Architecture owner can adopt the resolution or rule otherwise. The composition hash is not a reconciliation: MOD implements [Z2-N-ARCH-864] exactly (§ "Composition hash") and carries the extra inputs it needs in a separate build-identity value.

| # | Conflict | Resolution here |
|---|---|---|
| R1 | `Zenith_GameComposition` is declared in `Player/Zenith_GameComposition.h`, but `ZenithRuntime` (layer 6) consumes it and `ZenithPlayer` (layer 7) sits above the runtime, so the runtime would include a higher library's header (Zenith2/Docs/Architecture.md → "Library dependency graph"); `Zenith_CommandLine::Parse` takes the Player's `Zenith_PlayerArgs` for the same reason | the composition types move to `Engine/ZenithRuntime/Zenith_GameComposition.h`; `Player/Zenith_GameComposition.h` declares only `Zenith_GetGameComposition()` and includes the runtime header; `Parse` takes `(iArgc, ppszArgv)`. In the same patch `RunFrameLoop` and `Zenith_RuntimeMain` return `Zenith_ExitCode` (a new public header `Engine/ZenithRuntime/Zenith_ExitCode.h`, whose values are Architecture's exit-code table) instead of `int` |
| R2 | `Zenith_ModuleError`, `Zenith_RuntimeError` and `Zenith_CommandLineError` carry `const char*` and `Zenith_StringView` members; [Z2-N-CONTRACT-480] admits an error enumeration or a trivially copyable struct of at most 32 bytes whose public `m_eCode` is such an enumeration, carrying fixed-size context but "never a pointer to heap data or a string", so the three structs are illegal as written | each stays a struct under its Architecture name, in the string-free record form: first member `m_eCode` (the `…ErrorCode` enumeration, numbered from 1), then fixed-size context only (a step, a module index, an argument index), each at most 4 bytes; every `Zenith_Result<…, Zenith_ModuleError>` signature is textually Architecture's; the context that needs text (paths, key names, the failing module's name, the cause) is logged at the point of failure ([Z2-N-CONTRACT-464]) and kept in `Zenith_StartupReport` (see "Zenith_Runtime.h"). The string-free form is chosen over plain enumerations because a caller of `Create` or `Parse` needs the step, module or argument without parsing a log, and it keeps Architecture's type names |
| R3 | the generated TU's single path `${ZENITH_OUT}/Games/<Name>/` is written by every configure preset, and the Windows and Android trees resolve different RHI and physics backends | one subdirectory per configure preset (Zenith2/Docs/Design/MOD/GameBuild.md → "Generated file writing"); the Gradle project likewise |
| R4 | the composition example names factories `Zenith_CreateDevice_*` and `Zenith_CreateCodec_*`, while Zenith2/Docs/Rendering.md names `Zenith_CreateRHI_*` and Zenith2/Docs/Contracts.md names `Zenith_CreateCompression_*` | the generator copies every factory symbol from the backend's `FACTORY` declaration and never spells one, so the owning designs decide the names |
| R5 | the Online module takes a backend chosen by configuration from every linked backend, which the one-factory `Start` form cannot express | a backend-list `Start` form (Zenith2/Docs/Design/MOD/GameBuild.md → "Module to target mapping") |
| R6 | an addition, not a conflict: the rule that every construction step has a failure-injection test (Zenith2/Docs/Contracts.md → "Threading and concurrency contract") needs a seam `Zenith_Runtime::Create` does not declare | `Zenith_RuntimeTestAccess::Create` with step hooks, compiled in every configuration and reachable only through the test-access type ([Z2-N-CONTRACT-415]; the ECS precedent of `Zenith_WorldTestAccess`) |

## Dependencies

### Areas MOD depends on

| Area | What MOD relies on | Cited heading |
|---|---|---|
| (build contract) | `zenith_add_library` arguments, kinds, the `ZENITH_SUPPORTED` matrix, `declared_deps.json`, presets, flags, the Android toolchain rules | `Zenith2/Docs/Architecture/BuildSystem.md → "The zenith_add_library helper"`, `"Presets"`, `"Compile and link flags"`, `"Games in CMake"` |
| (composition contract) | the composition table, the startup sequence, the module interfaces, exit codes, the command line, the Gradle template | `Zenith2/Docs/Architecture/CompositionRoot.md → "The generated composition table"`, `"Startup sequence"`, `"Android packaging"`, `"Required tests"` |
| CORE | `Zenith_Heap`, `Zenith_Owned`, `Zenith_Result`, `Zenith_Span`, `Zenith_StringView`, `Zenith_StringTable` (the process interned-string table, `Zenith2/Docs/Design/CORE/Text.md` → "String interning"), the structured log and the log router, the JSON reader and writer, the root-heap and profiler-hook installers, `Zenith_BuildInfo.h` | `Zenith2/Docs/Contracts.md → "Ownership and allocation"`, `"No mutable global state"`, `"Errors and results"`; `Zenith2/Docs/Design/CORE.md` (pending) |
| PLAT | `Zenith_Platform`, the virtual filesystem mounts, `Zenith_Config` and its layers, the executable directory | `Zenith2/Docs/Architecture/PlatformLayer.md → "Virtual filesystem"`, `"Configuration layering"`; `Zenith2/Docs/Design/PLAT.md` (pending) |
| JOBS, GRAPH | `Zenith_JobSystem`, `Zenith_FrameExecutor`, the simulation clock, schedule compilation, the frame prologue | `Zenith2/Docs/Architecture/CPUFrameGraph.md → "Frame structure"`, `"Schedule compilation and caching"`, `"Cancellation and shutdown"` |
| ECS | `Zenith_TypeRegistry`, `Zenith_WorldRegistry`, `Zenith_World`, `Zenith_WorldSimulation`, `Zenith_PfnCreateWorld`, `RegisterSchemaBlob`, `Freeze` | `Zenith2/Docs/Architecture.md → "Reflection and runtime type registration"`; `Zenith2/Docs/Architecture/ECSModel.md → "World registry"`; `Zenith2/Docs/Architecture/DataDefinedComponents.md → "Registration at game load"` |
| SCENE, ASSET | the startup scene load, the asset registry, the cook's reading of `composition.json` | `Zenith2/Docs/Design/SCENE.md`, `Zenith2/Docs/Design/ASSET.md` (pending) |
| RHI, RPROD | device creation from `rhi.backend`, the Flux renderer, the engine stage nodes' content, the D3D12 Agility exports | `Zenith2/Docs/Rendering.md → "GPU baseline and profiles"`, `"GPU lifetime and synchronisation"`; `Zenith2/Docs/Design/RHI.md` (pending) |
| COMPRESS | the two codec factories | `Zenith2/Docs/Design/COMPRESS.md` (pending) |
| SCRIPT | the script runtime, `Zenith_ScriptRecoveryPolicy`, the binding ABI, `.d.luau` generation | `Zenith2/Docs/Scripting.md → "Modules, bindings and generated types"`, `"Quarantine and hot reload"` |
| GAME | world creation and transition, the lifecycle shutdown | `Zenith2/Docs/Architecture.md → "Lifecycle commands"`, `"Worlds and isolation"` |
| EDIT | the new-project wizard dialog | `Zenith2/Docs/Design/EDIT.md` (pending) |
| SHIP | the manifest pins (JDK, Android SDK, Gradle, AGP, NDK), the offline Gradle cache | `Zenith2/Docs/Architecture/BuildSystem.md → "Toolchain pins and the setup script"`; `Zenith2/Docs/Design/SHIP.md` (pending) |
| PROC | the gate steps that check MOD's output: `SourceLayout`, `CompileCoverage`, `Libgraph`, `ForbiddenCheck`, `TestSeparationCheck`, `ApkCheck`, `Test` | `Zenith2/Docs/Contracts.md → "Enforceability matrix"` |

### Libraries and third-party dependencies

| Target | Declared direct dependencies | Third party |
|---|---|---|
| `ZenithRuntime` | exactly the row of Zenith2/Docs/Architecture.md → "Library dependency graph": ZenithCore, ZenithGraph, ZenithJobs, ZenithProfiling, ZenithPlatform, ZenithReflect, ZenithCompression, ZenithAssets, ZenithECS, ZenithScene, ZenithSave, ZenithScript, ZenithRHI, ZenithFlux | none |
| `ZenithPlayer` | ZenithRuntime, ZenithCore, ZenithProfiling, ZenithPlatform | none (no SDL3 header: [Z2-N-ARCH-858] and the Player layout of Zenith2/Docs/Architecture/CompositionRoot.md) |
| `ZenithNewGameScaffold` | ZenithCore, ZenithPlatform, ZenithReflect (Zenith2/Docs/Design/MOD/ProjectCreation.md → "Validation in C++") | none |
| `ZenithNewGame` | ZenithNewGameScaffold, ZenithCore, ZenithPlatform, ZenithReflect | none |
| every game executable | ZenithPlayer, ZenithRuntime, its modules and backends (links only; a game declares nothing) | none directly |

### Interfaces MOD declares for lower layers

- `Zenith_ModuleInstance` (`ZenithRuntime/Zenith_Module.h`) is the upward interface through which the runtime reaches module code: modules sit above `ZenithRuntime`, so the runtime never names a module's type, only this interface and the two entry-point pointers in the composition ([Z2-N-ARCH-078]).
- The composition table itself is the other downward-pointing seam: the generated TU (attributed to the game executable, the top of the graph) hands the runtime function pointers into modules and backends, so no library below the executable references a module or backend symbol.
- MOD declares no callback for libraries below `ZenithRuntime`; the runtime is the caller of every lower library.

### Areas that depend on MOD

Every module area (TWOD, PHYS, ANIM, AUDIO, AI, UI, ONLINE, NET, STREAM, GAME, CINE, VFX, ENV) implements the two entry points and `Zenith_ModuleInstance`; ASSET's cook reads `composition.json`; EDIT links `ZenithNewGameScaffold`; the areas owning repository games (RPROD, ECS, UI, GAME, CINE, TWOD) author their game's content inside the `game.json` selections of "Repository games"; DEVT's on-screen profiler displays the startup timeline and composition; SHIP packages the executables and APKs.

## Library and file layout

### Targets owned by MOD

| Target | Directory | Kind | `SUPPORTED` | Public root headers |
|---|---|---|---|---|
| `ZenithRuntime` | `Zenith2/Engine/ZenithRuntime/` | `KIND ENGINE` | `ZENITH_GPU` | `Zenith_Runtime.h`, `Zenith_RuntimeTestAccess.h`, `Zenith_CommandLine.h`, `Zenith_Module.h`, `Zenith_GameComposition.h`, `Zenith_ExitCode.h`, `Zenith_Extractor.h`, `Zenith_ModulePublication.h` (from Stage 3), `Zenith_RuntimeToolAccess.h` (from Stage 3, tool sources only), and GAME's `Zenith_WorldControl.h`, `Zenith_WorldState.h` |
| `ZenithPlayer` | `Zenith2/Player/` | `KIND PLAYER` | `ZENITH_GPU` | `Player/Zenith_GameComposition.h`, `Player/Zenith_PlayerMain.h` |
| `ZenithNewGameScaffold` | `Zenith2/Tools/NewGameScaffold/` | tool library | `ZENITH_TOOLS_HOST` | `Zenith_NewGameScaffold.h` |
| `ZenithNewGame` | `Zenith2/Tools/NewGame/` | tool executable | `ZENITH_TOOLS_HOST` | none |
| `Zenith_Integration_NewGame` | `Zenith2/Tests/NewGame/` | integration suite | `ZENITH_TOOLS_HOST` | none |
| each game `<Name>` | `Zenith2/Games/<Name>/` | game executable (Windows) or shared object (Android) | the presets its `platforms` allow within `ZENITH_GPU` | none |

### ZenithRuntime files

| Path under `Zenith2/Engine/ZenithRuntime/` | Contents | Content specified by |
|---|---|---|
| `CMakeLists.txt` | `zenith_add_library(ZenithRuntime DEPENDS ZenithCore ZenithGraph ZenithJobs ZenithProfiling ZenithPlatform ZenithReflect ZenithCompression ZenithAssets ZenithECS ZenithScene ZenithSave ZenithScript ZenithRHI ZenithFlux SUPPORTED ZENITH_GPU)` | MOD |
| `Zenith_GameComposition.h` | the composition types (§ "Zenith_GameComposition.h") | MOD |
| `Zenith_Module.h` | the module interfaces (§ "Zenith_Module.h") | MOD |
| `Zenith_Runtime.h` | `Zenith_Runtime`, `Zenith_RuntimeMain`, `Zenith_RuntimeError`, `Zenith_StartupStep`, `Zenith_StartupReport` (§ "Zenith_Runtime.h") | MOD |
| `Zenith_RuntimeTestAccess.h` | § "Zenith_RuntimeTestAccess.h" | MOD |
| `Zenith_CommandLine.h` | § "Zenith_CommandLine.h" | MOD |
| `Zenith_ExitCode.h` | § "Zenith_ExitCode.h" | MOD |
| `Zenith_Extractor.h` | the extractor descriptor | GRAPH (Zenith2/Docs/Architecture/CPUFrameGraph.md → "Render extraction and pipelining") |
| `Zenith_ModulePublication.h` | `Zenith_ModulePublication`, `Zenith_ModuleSourceDesc`, `Zenith_PublicationSinkError`, `Zenith_ModulePublicationHandler`, `Zenith_ModulePublicationSink` (from Stage 3; Zenith2/Docs/Design/MOD/RuntimeSeams.md → "Zenith_ModulePublication header") | MOD |
| `Zenith_RuntimeToolAccess.h` | `Zenith_RuntimeToolAccess`, `Zenith_ToolFrameHooks`, `Zenith_ToolServices`, `Zenith_ToolRuntimeDesc`, `Zenith_ToolWorld`, `Zenith_ToolWorldDesc`, `Zenith_SchemaWindowResult` (from Stage 3; Zenith2/Docs/Design/MOD/RuntimeSeams.md → "Runtime tool access"; included only by tool sources, cross-doc item X258) | MOD |
| `Zenith_WorldControl.h`, `Zenith_WorldState.h` | the world's pause control and world state resource, public so modules name them (A3 cross-area row GA5 (b)) | GAME (`Zenith2/Docs/Design/GAME/WorldHost.md` → "World host files and split with MOD") |
| `Zenith_RuntimeScriptBindings.h` | `Zenith_Result<void, Zenith_ScriptBindingError> Zenith_RegisterLowLibraryScriptBindings(Zenith_ScriptBindingTable& xTable)`, the engine's low-library script bindings in the runtime's order, public so ZenithCook's services register the same set the runtime does at step 14 (A5 cross-area rows SB29, AB8) | SCRIPT (a SCRIPT-owned public header inside ZenithRuntime, landed by SCRIPT's low-library bindings ticket); MOD only lists the file |
| `Internal/Zenith_RuntimeCreate.cpp` | `Zenith_Runtime::Create`, the startup sequence steps 4–18 | MOD |
| `Internal/Zenith_RuntimeDestroy.cpp` | `~Zenith_Runtime`, the shutdown order | MOD (order: Zenith2/Docs/Contracts.md → "Threading and concurrency contract") |
| `Internal/Zenith_RuntimeMain.cpp` | `Zenith_RuntimeMain` | MOD |
| `Internal/Zenith_RuntimeToolAccess.cpp` | the tool host's `Create` mode, `RunOneFrame` with its hooks and call positions, the tool worlds, extractors and schema queue (from Stage 3; Zenith2/Docs/Design/MOD/RuntimeSeams.md → "Runtime tool access") | MOD |
| `Internal/Zenith_StartupSteps.h`, `Internal/Zenith_StartupSteps.cpp` | the step table, the unwind stack, the exit-code map | MOD |
| `Internal/Zenith_CompositionValidation.h`, `.cpp` | `Zenith_ValidateComposition` (§ "Composition validation at startup") | MOD |
| `Internal/Zenith_CompositionDump.cpp` | the composition dump (§ "Startup composition dump") | MOD |
| `Internal/Zenith_ModuleHost.h`, `.cpp` | module registration, start, attach, detach, destroy (§ "Module lifecycle state machine") | MOD |
| `Internal/Zenith_ModuleRegistrationJournal.h`, `.cpp` | the per-module journal of context registrations (§ "Registration journal and undo") | MOD |
| `Internal/Zenith_ModuleStartContext.cpp`, `Internal/Zenith_WorldAttachContext.cpp` | the context implementations and their rejection rules | MOD |
| `Internal/Zenith_CommandLine.cpp` | `Zenith_CommandLine::Parse` | MOD |
| `Internal/Zenith_FrameLoop.h`, `Internal/Zenith_FrameLoop.cpp` | `Zenith_Runtime::RunFrameLoop`, the prologue driver (§ "Frame loop driver") and, from Stage 3, `Zenith_FrameLoop_PrecompileSwap` (Zenith2/Docs/Design/MOD/RuntimeSeams.md → "Swap precompile and test hooks") | MOD (prologue steps: GRAPH) |
| `Internal/Zenith_EngineStages.cpp` | registration of the engine stage nodes (§ "Frame-graph nodes registered by ZenithRuntime") | MOD registers; RPROD and GRAPH specify the node bodies |
| `Internal/Zenith_EngineExtractors.cpp` | the engine's own extractors | RPROD |
| `Internal/Zenith_LightingTypes.h`, `.cpp` | `Zenith_RegisterLightingComponentTypes`, called in startup step 6's engine-types part: the C++-registered lighting component types (`Zenith.Lighting.DirectionalLight` among them) | LIGHT (`Zenith2/Docs/Design/LIGHT.md` → "ZenithRuntime LIGHT files") |
| `Internal/Zenith_RenderTypes.h`, `.cpp` | `Zenith_RegisterRenderComponentTypes`, called in startup step 6's engine-types part: the C++-registered render component types (`Zenith.Render.Camera`, `Zenith.Render.MeshInstance`) | RPROD (`Zenith2/Docs/Design/RPROD/Extractors.md` → "Render component registration") |
| `Internal/Zenith_WorldHost.cpp` | world creation for the composition, the world-transition entry `RequestWorldTransition` | GAME (Zenith2/Docs/Architecture/LifecycleCommands.md → "World transition") |
| `Internal/Zenith_HotReloadWindow.cpp` | prologue step 6 ([Z2-N-ARCH-863]) | SCRIPT and ECS |
| `Internal/Zenith_SceneSeams.h`, `.cpp` | `Zenith_RuntimeScenePublicationSink`, `Zenith_RuntimeScenePublishForwarder`, `Zenith_AsyncRequestMultiplexer` (§ "SCENE and SCRIPT seams of the runtime") | MOD, against SCENE's and SCRIPT's interfaces |
| `Internal/Zenith_ModulePublicationSinkImpl.h`, `.cpp` | the module publication sink's implementation and `Zenith_ModulePublishForwarder` (from Stage 3; Zenith2/Docs/Design/MOD/RuntimeSeams.md → "Module publication sink") | MOD, against SCRIPT's interfaces |
| `Internal/Zenith_ScriptHostImpl.h`, `.cpp` | `Zenith_RuntimeScriptListener`, the runtime's `Zenith_ScriptWorldListener` (from Stage 3; § "Step construction notes", the SCRIPT table) | MOD, against SCRIPT's interface |
| `Internal/Zenith_ScriptMigrationFixtures.h`, `.cpp` | the `--script-migration-fixtures` run (from Stage 3; § "The gametest format") | MOD, calling SCRIPT's `Zenith_ScriptRestoreMigrationFixtures` |
| `Internal/PlatformBridge/Zenith_InputPhaseSystem.cpp`, `Zenith_InputResources.cpp`, `Zenith_InputCommandKinds.cpp`, `Zenith_HapticsStage.cpp`, `Zenith_PlatformEventPublisher.cpp`, `Zenith_PlatformEventTypes.cpp` | the runtime bridge of ZenithPlatform: the `Input.Actions` phase-1 system, the reflected input resources, the input command kinds, the `Input.Haptics` epilogue stage, platform event publication and the `Platform.*` event types | PLAT (`Zenith2/Docs/Design/PLAT.md` → "Runtime bridge files in ZenithRuntime") |
| `Internal/ScriptBindings/Zenith_InputScriptBindings.cpp` | the `Input` Luau namespace (the seventh bridge file) | PLAT |
| `Internal/Package/`, `Internal/ScriptBindings/Zenith_PackageScriptBindings.cpp`, `Tests/Zenith_RuntimePackage_Tests.cpp` | the package runtime bridge, called from MOD's files at the places of Zenith2/Docs/Design/MOD/RuntimeSeams.md → "Call sites PACK's tickets add" (from Stage 6) | PACK (`Zenith2/Docs/Design/PACK/PublicAPI.md` → "Runtime bridge declarations") |
| `Internal/Zenith_RuntimePlatformListeners.cpp` | the runtime's implementations of PLAT's `Zenith_AppLifecycleListener` and `Zenith_ThermalAdaptationTarget` | MOD, against PLAT's interfaces (PLAT P6 g) |
| `Internal/ScriptBindings/*.cpp` | bindings of the libraries below ZenithReflect | SCRIPT (Zenith2/Docs/Scripting.md → "Modules, bindings and generated types") |
| `Internal/Zenith_RuntimeLog.h` | the five log categories of § "Log categories" | MOD |
| `Tests/Zenith_Composition_Tests.cpp`, `Tests/Zenith_Runtime_Tests.cpp`, `Tests/Zenith_RuntimeCreate_Tests.cpp`, `Tests/Zenith_ModuleHost_Tests.cpp`, `Tests/Zenith_CommandLine_Tests.cpp` | § "Test files" | MOD |
| `Tests/FakeModules/` | fake module entry points and `Zenith_ModuleInstance` implementations compiled into `ZenithRuntime_Tests` (a recording module, a module failing on demand, a module making a rejected registration) | MOD |
| `Tests/Fixtures/Content/` | the cooked minimal scene the runtime tests boot headless | MOD |

MOD implements no string interner: the process interned-string table ([Z2-N-ARCH-609]) is CORE's `Zenith_StringTable` (`Zenith2/Docs/Design/CORE/Text.md` → "String interning"); ZenithRuntime constructs the one process-lifetime instance at the start of startup step 10, before any type registration, with `Zenith_StringTableDesc::m_uMaxStrings` and `m_ulMaxTextBytes` read from CORE's configuration keys `strings.maxStrings` and `strings.maxTextBytes` (required keys, `m_bRequired`, supplied by ZenithCore's platform configuration fragment until CORE's pending row "String intern table capacity" is adopted; `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Core configuration keys") in the configuration built at step 6, passes it by reference to the type registry, the script runtime and every world, and destroys it with the registries.

`ZenithRuntime` has no `Internal/Windows|Android|Linux/` files of its own: every platform difference it needs (the executable directory, the window) comes through `Zenith_Platform`.

### ZenithPlayer files

The Player layout is Zenith2/Docs/Architecture/CompositionRoot.md → "Player layout", with reconciliation R1:

| Path under `Zenith2/Player/` | Contents |
|---|---|
| `CMakeLists.txt` | `zenith_add_library(ZenithPlayer KIND PLAYER DEPENDS ZenithRuntime ZenithCore ZenithProfiling ZenithPlatform SUPPORTED ZENITH_GPU)` |
| `Zenith_GameComposition.h` | `#include "ZenithRuntime/Zenith_GameComposition.h"` and the declaration of `Zenith_GetGameComposition()`, which each game's generated TU defines |
| `Zenith_PlayerMain.h` | `Zenith_PlayerArgs`, `Zenith_PlayerMain` (§ "Player headers") |
| `Internal/Zenith_PlayerMain.cpp` | steps 1–3 of the startup sequence, then `Zenith_RuntimeMain` |
| `Internal/Windows/Zenith_PlayerEntry_Windows.cpp` | `int main(int, char**)`; its first statement is `Zenith_ApplyCrtMathMode();` (`ZenithCore/Zenith_Transcendental.h`, [Z2-N-ARCH-889]; A3 cross-area row C20); then re-reads the arguments as UTF-8 from `GetCommandLineW`/`CommandLineToArgvW` into arena storage on `main`'s stack; calls `Zenith_PlayerMain` |
| `Internal/Android/Zenith_PlayerEntry_Android.cpp` | `extern "C" __attribute__((visibility("default"))) int SDL_main(int iArgc, char** ppszArgv)`; its first statement is `Zenith_ApplyCrtMathMode();` (an empty body on Android, kept so every entry has the same shape); calls `Zenith_PlayerMain` |
| `Tests/Zenith_PlayerMain_Tests.cpp`, `Tests/Zenith_PlayerArguments_Tests.cpp` | § "Test files" |
| `GradleTemplate/` | the Android Gradle template (Zenith2/Docs/Architecture/CompositionRoot.md → "Android packaging"; no compiled source); from Stage 4 it also holds `app/src/main/java/org/zenith2/player/Zenith2Accessibility.java`, the class of `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Java bridge contract", with `Zenith2Activity.getAccessibilityHostView()`, its `-keep` rule in `app/proguard-rules.pro` and `fontScale` in the manifest's `android:configChanges` (A3 cross-area row P20; the core file list is cross-document item X269 (3)) |

### Tools NewGame and NewGameScaffold files

The complete file list is `Zenith2/Docs/Design/MOD/ProjectCreation.md` → "ZenithNewGameScaffold library API" ("Targets and files").

### CMake helper files owned by MOD

| File | Contents |
|---|---|
| `Zenith2/CMake/ZenithAddGame.cmake` | `zenith_add_game`, `zenith_validate_game_json`, module and backend resolution, the generators, the Android wiring (Zenith2/Docs/Design/MOD/GameBuild.md) |
| `Zenith2/CMake/Tests/ZenithCMakeTests.cmake` and the runners and fixtures under `Zenith2/CMake/Tests/` | Zenith2/Docs/Design/MOD/GameBuild.md → "CMake fixture tests" |

`Zenith2/CMake/` is SHIP's directory (Zenith2/Docs/Architecture.md → "Repository layout", "Ownership per directory"); the two entries above are MOD's content inside it, and `CMake/ZenithAddLibrary.cmake` and the other helpers remain SHIP's, landed first by the Stage 0 scaffold ticket (§ "Stage 0 scaffold").

### Game directory layout

```text
Zenith2/Games/<Name>/
	game.json                     the descriptor (Zenith2/Docs/Design/MOD/GameBuild.md → "game.json schema v1")
	CMakeLists.txt                zenith_add_game(<Name>)
	Assets/                       content root; Assets/Config/game.config.json is the game configuration layer
	Scenes/                       content root; scene documents
	Schemas/                      content root; *.schema.json data-defined components
	Scripts/                      content root; Luau modules
	Tests/                        optional; <Stem>.gametest.json and the input scripts they name
	Workload/                     optional; <W>.workload.json (workload games and flagship slices)
	Acceptance/                   optional; the flagship acceptance package
	.migration/                   optional, gitignored; authoring-document migration backups ([Z2-N-CONTRACT-541])
	.editor/                      optional, gitignored, from Stage 4; the editor's per-user state (core text since X312; Zenith2/Docs/Design/MOD/GameBuild.md → "Game directory entries")
	package.recipe.json           optional file, from Stage 4; PACK's packaging recipe (pending ARCH, X453; Zenith2/Docs/Design/MOD/GameBuild.md → "Game directory entries")
```

The four content-root directories are the defaults every template writes; `content.roots` may name other top-level directories, and only the entries of Zenith2/Docs/Design/MOD/GameBuild.md → "Game directory entries" are allowed. No compiled-language source exists anywhere under `Games/` ([Z2-N-ARCH-205]).

### Supported matrices

| Target | Presets it exists in | Why |
|---|---|---|
| `ZenithRuntime`, `ZenithPlayer` | `win`, `win-asan`, `android-arm64`, `android-x86_64` | GPU libraries exclude `linux-tsan` ([Z2-N-ARCH-173]) |
| a game with `platforms: ["windows","android"]` | the same four | its platforms within `ZENITH_GPU` |
| a game with `platforms: ["windows"]` | `win`, `win-asan` | step 5 of `zenith_add_game` returns on Android presets |
| `ZenithNewGameScaffold`, `ZenithNewGame`, `Zenith_Integration_NewGame` | `win` | host tools (`ZENITH_TOOLS_HOST`) |
| the CMake fixture tests | registered in `win` | they need no build, only `cmake -P` |

## Public API

Every declaration below is Zenith Hungarian (Z2-D-034). Contract blocks follow Zenith2/Docs/Contracts.md → "Contract blocks per type"; the grammar's `Example` field is omitted where no compiled example exists yet.

### Zenith_GameComposition.h

```cpp
// Zenith2/Engine/ZenithRuntime/Zenith_GameComposition.h
#pragma once
#include "ZenithCore/Zenith_Span.h"
#include "ZenithECS/Zenith_World.h"                 // Zenith_PfnCreateWorld
#include "ZenithRHI/Zenith_RHIDevice.h"             // Zenith_PfnCreateDevice
#include "ZenithCompression/Zenith_Compression.h"   // Zenith_PfnCreateCompression
#include "ZenithScript/Zenith_ScriptRecovery.h"     // Zenith_ScriptRecoveryPolicy
#include "ZenithRuntime/Zenith_Module.h"

/// @contract functions ModuleEntryPoints
/// Ownership: Pointers to functions defined by module libraries; never owned.
/// Thread affinity: MAIN_THREAD. The runtime calls both entry points on the main thread during startup.
/// Invalidation: None; the functions are statically linked.
/// Units: None.
/// Errors: Zenith_ModuleError (see Zenith_Module.h).
/// Version: since Z2 Stage 2; the signatures change only with uZENITH_MODULE_INTERFACE_VERSION.
/// @endcontract
using Zenith_PfnModuleRegisterTypes = Zenith_Result<void, Zenith_ModuleError> (*)(Zenith_TypeRegistry& xTypes);
using Zenith_PfnModuleStart = Zenith_Result<Zenith_Owned<Zenith_ModuleInstance>, Zenith_ModuleError> (*)(
	Zenith_ModuleStartContext& xContext);

/// @contract value Zenith_ModuleDescriptor
/// Ownership: constexpr data in the game's generated TU.
/// Thread affinity: IMMUTABLE. Constant data.
/// Invalidation: None.
/// Units: None.
/// Errors: Validated at startup by the runtime; a malformed descriptor stops startup with exit code 3.
/// Version: since Z2 Stage 2; a new member is appended and the generator fills it.
/// @endcontract
struct Zenith_ModuleDescriptor
{
	const char* const m_szName = nullptr;                              // the MODULE_NAME
	const uint32 m_uInterfaceVersion = 0;                             // uZENITH_MODULE_INTERFACE_VERSION when the TU was compiled
	const Zenith_PfnModuleRegisterTypes m_pfnRegisterTypes = nullptr;
	const Zenith_PfnModuleStart m_pfnStart = nullptr;                  // the entry point, or its backend-binding thunk
	const char* const m_szBackendName = nullptr;                       // "Jolt"; "Steam,Offline" for a backend list; nullptr without a family
	const char* const m_szModuleDependencies = nullptr;                // the selected module dependencies, comma-joined ("Physics"); "" when none
	const Zenith_PfnCreateDeviceInterop m_pfnCreateDeviceInterop = nullptr;   // Stage 7: the XR module's device-interop factory; nullptr for
	                                                   // every other module (a composition-time hook, not a module entry point)
};

/// @contract value Zenith_RHIBackendEntry
/// Ownership: constexpr data in the game's generated TU.
/// Thread affinity: IMMUTABLE. Constant data.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_RHIBackendEntry
{
	const char* const m_szName = nullptr;              // BACKEND_NAME: "Vulkan", "D3D12"
	const Zenith_PfnCreateDevice m_pfnCreate = nullptr;
};

/// @contract value Zenith_CodecEntry
/// Ownership: constexpr data in the game's generated TU.
/// Thread affinity: IMMUTABLE. Constant data.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_CodecEntry
{
	const char* const m_szName = nullptr;              // "zlib", "zstd"
	const Zenith_PfnCreateCompression m_pfnCreate = nullptr;
};

/// @contract value Zenith_GameSettings
/// Ownership: constexpr data in the game's generated TU, derived from game.json.
/// Thread affinity: IMMUTABLE. Constant data.
/// Invalidation: None.
/// Units: Ticks for m_uScriptRecoveryDelayTicks.
/// Errors: None; the values were validated by zenith_add_game.
/// Version: since Z2 Stage 2; the script-recovery members are read from Z2 Stage 3.
/// @endcontract
struct Zenith_GameSettings
{
	const char* const m_szStartupScene = nullptr;               // a content:/ virtual path; the default of content.startupScene
	const bool m_bScriptRecoveryDeclared = false;               // game.json carries scriptRecovery
	const Zenith_ScriptRecoveryPolicy m_eScriptRecoveryPolicy = SCRIPT_RECOVERY_POLICY_MANUAL;
	const uint32 m_uScriptRecoveryDelayTicks = 0;
	const uint32 m_uScriptRecoveryMaxPerModule = 0;
};

/// @contract value Zenith_GameComposition
/// Ownership: constexpr data defined by the game's generated TU; returned by reference from Zenith_GetGameComposition.
/// Thread affinity: IMMUTABLE. Constant data.
/// Invalidation: None; it lives for the process.
/// Units: None.
/// Errors: Zenith_ValidateComposition checks it at the start of startup step 6 (exit code 3 on a defect).
/// Version: since Z2 Stage 2; members are appended, and the generator and this header change in one diff.
/// @endcontract
struct Zenith_GameComposition
{
	const char* const m_szGameName = nullptr;
	const char* const m_szGameVersion = nullptr;
	const Zenith_Span<const Zenith_ModuleDescriptor> m_axModules = {};          // canonical registration order
	const Zenith_PfnCreateWorld m_pfnCreateWorld = nullptr;                     // the selected ECS backend
	const char* const m_szECSBackendName = nullptr;                             // "Custom", "Flecs" or "EnTT"
	const Zenith_Span<const Zenith_RHIBackendEntry> m_axRHIBackends = {};       // Windows: Vulkan, D3D12; Android: Vulkan
	const Zenith_Span<const Zenith_CodecEntry> m_axCodecs = {};                 // zlib and zstd, always both
	const Zenith_GameSettings m_xSettings = {};
	const uint64 m_ulCompositionHash = 0;                                      // [Z2-N-ARCH-864], § "Composition hash"
	const uint64 m_ulBuildIdentity = 0;                                        // per preset, § "Build identity"
};
```

<!-- assumption: pending ARCH -->
`m_szModuleDependencies` is not yet in Architecture: Zenith2/Docs/Architecture/CompositionRoot.md → "The generated composition table" currently ends `Zenith_ModuleDescriptor` with "`const char* m_szBackendName;                       // "Jolt"; a backend list comma-joined ("Offline"); nullptr without a family`". The field follows it in the batched Architecture patch (cross-area note M26).
<!-- /assumption -->

<!-- assumption: pending RHI -->
`Zenith_PfnCreateDevice` is the pointer-type name Zenith2/Docs/Architecture/CompositionRoot.md declares beside the RHI factories; Zenith2/Docs/Rendering.md names those factories `Zenith_CreateRHI_*`, so the RHI design may rename the type (for example `Zenith_PfnCreateRHI`), and this header follows its ruling.
<!-- /assumption -->

`Zenith_PfnCreateCompression`, the `Zenith_Compression` interface and `Zenith_CompressionDesc` are COMPRESS's (`Zenith2/Docs/Design/COMPRESS/PublicAPI.md` → "Zenith_Compression.h"), and Zenith2/Docs/Architecture/CompositionRoot.md → "The generated composition table" declares `Zenith_CodecEntry` with the same field type.

<!-- assumption: pending CORE -->
`Zenith_Span<T>` (declared in `ZenithCore/Zenith_Span.h`, the X7/X26 ruling) has a `constexpr` constructor from a C array and a `constexpr` default constructor giving an empty span, so the composition is a constant expression.
<!-- /assumption -->

<!-- assumption: pending SCRIPT -->
`Zenith_ScriptRecoveryPolicy` (Zenith2/Docs/Scripting.md → "Quarantine and hot reload") is declared in the public ZenithScript header `ZenithScript/Zenith_ScriptRecovery.h`; the SCRIPT design fixes the header name.
<!-- /assumption -->

### Zenith_Module.h

The module interfaces are Zenith2/Docs/Architecture/CompositionRoot.md → "Startup sequence" ("Module interfaces") verbatim, with reconciliation R2 applied to the error type (string members removed) and the rejection rules below added.

```cpp
// Zenith2/Engine/ZenithRuntime/Zenith_Module.h (the parts this design changes or adds)
inline constexpr uint32 uZENITH_MODULE_INTERFACE_VERSION = 1;

/// @contract enum Zenith_ModuleErrorCode
/// Ownership: A plain value.
/// Thread affinity: IMMUTABLE. A value type.
/// Invalidation: None.
/// Units: None.
/// Errors: MODULE_ERROR_REGISTRATION: a type, system, command-kind, binding, resource, listener, extractor or
///   epilogue-stage registration was rejected (§ "Registration rejection rules"). MODULE_ERROR_BACKEND_CREATE: the
///   selected backend's factory failed. MODULE_ERROR_DEVICE: a required device could not be opened.
///   MODULE_ERROR_OUT_OF_MEMORY: an allocation failed. The module logs the detail before returning.
/// Version: since Z2 Stage 2; enumerators keep their meaning once shipped.
/// @endcontract
enum Zenith_ModuleErrorCode : uint8
{
	MODULE_ERROR_REGISTRATION = 1,
	MODULE_ERROR_BACKEND_CREATE,
	MODULE_ERROR_DEVICE,
	MODULE_ERROR_OUT_OF_MEMORY,
};

constexpr const char* Zenith_ToString(Zenith_ModuleErrorCode eCode);

/// @contract value Zenith_ModuleError
/// Ownership: A plain value returned inside a Zenith_Result.
/// Thread affinity: IMMUTABLE. A value type.
///   Members non-const: it is the E of a Zenith_Result, whose move assignment assigns it.
/// Invalidation: None.
/// Units: None.
/// Errors: m_eCode names the failure; a module returns m_uModuleIndex as 0xFFFF and the module host fills it with the
///   module's index in m_axModules before the error leaves the host. The error form of [Z2-N-CONTRACT-480]: 4 bytes, no string.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_ModuleError
{
	Zenith_ModuleErrorCode m_eCode;
	uint8 m_uReserved = 0;
	uint16 m_uModuleIndex = 0xFFFFu;
};

/// @contract interface Zenith_ModuleInstance
/// Ownership: Created by the module's Start function as a Zenith_Owned; owned by ZenithRuntime's module host until shutdown.
/// Thread affinity: MAIN_THREAD. Every member is called on the main thread, outside frame work.
/// Invalidation: None.
/// Units: None.
/// Errors: AttachWorld returns Zenith_ModuleError; DetachWorld cannot fail.
/// Reentrancy: NONE. A module never calls the runtime's module host from inside these members.
/// Version: since Z2 Stage 2; a change is a uZENITH_MODULE_INTERFACE_VERSION bump.
/// @endcontract
class Zenith_ModuleInstance
{
public:
	virtual ~Zenith_ModuleInstance() = default;
	virtual Zenith_Result<void, Zenith_ModuleError> AttachWorld(Zenith_WorldAttachContext& xContext) = 0;
	virtual void DetachWorld(uint16 uWorldTag) = 0;
};
```

`Zenith_ModuleStartContext`, `Zenith_WorldAttachContext`, `Zenith_EpilogueStageDesc`, `Zenith_CommandKindDesc` and `Zenith_PfnCommandPlayback` are exactly as Zenith2/Docs/Architecture/CompositionRoot.md declares them. From Stage 2 `Zenith_ModuleStartContext` has `GetProfilerOverlay` (Zenith2/Docs/Design/MOD/RuntimeSeams.md → "Start context overlay access"; core text since cross-doc item X245 (c); Z2T-MOD-086, integration Z2T-MOD-076), from Stage 3 `Zenith_WorldAttachContext` also has `GetPublicationSink`, `FindCommandKind` and `GetWorldControl` (→ "World attach context lookups"; core text since cross-doc item X344), from Stage 4 `Zenith_ModuleStartContext` has `IsRecordingReplay` and `RequestExitAfterStart` (core text since cross-doc item X369), and from Stage 6 `ProvideEntitlementQuery` (→ "Start context members for ONLINE and PACK"; pending cross-doc item X325 (1)). Their `Register*` and `Add*` members return `void`; a rejected registration is recorded in the context, and the runtime treats the call that made it as failed with `MODULE_ERROR_REGISTRATION` once it returns (§ "Registration rejection rules"). A module constructs its error as `Zenith_Err(Zenith_ModuleError{ MODULE_ERROR_DEVICE })` and logs the detail first.

Each module's root header, `Engine/<Library>/Zenith_Module_<MODULE_NAME>.h`, declares its two entry points in the `Start` form of Zenith2/Docs/Design/MOD/GameBuild.md → "Module to target mapping" and nothing else ([Z2-N-ARCH-168]).

### Zenith_Runtime.h

```cpp
// Zenith2/Engine/ZenithRuntime/Zenith_Runtime.h
#pragma once
#include "ZenithRuntime/Zenith_CommandLine.h"
#include "ZenithRuntime/Zenith_ExitCode.h"
#include "ZenithRuntime/Zenith_GameComposition.h"

/// @contract enum Zenith_RuntimeErrorCode
/// Ownership: A plain value.
/// Thread affinity: IMMUTABLE. A value type.
/// Invalidation: None.
/// Units: None.
/// Errors: RUNTIME_ERROR_STARTUP_STEP: a startup step failed (Zenith_StartupReport names it).
///   RUNTIME_ERROR_CONFIGURATION: Create returns it (with m_eStep) for the exit-code-2 rows of steps 6 and 9.
///   RUNTIME_ERROR_TRANSITION_PEAK_REFUSED, RUNTIME_ERROR_TRANSITION_IN_PROGRESS, RUNTIME_ERROR_UNKNOWN_WORLD:
///   RequestWorldTransition refusals (Zenith2/Docs/Architecture/LifecycleCommands.md → "World transition").
///   RUNTIME_ERROR_WORLD_CREATE … RUNTIME_ERROR_FRAME_FAILED: Zenith_RuntimeToolAccess refusals (since Z2 Stage 3;
///   Zenith2/Docs/Design/MOD/RuntimeSeams.md → "Runtime tool access"; core text Zenith2/Docs/Architecture/CompositionRoot.md → "Runtime and entry types", X370).
/// Version: since Z2 Stage 2; enumerators keep their meaning once shipped.
/// @endcontract
enum Zenith_RuntimeErrorCode : uint8
{
	RUNTIME_ERROR_STARTUP_STEP = 1,
	RUNTIME_ERROR_CONFIGURATION,
	RUNTIME_ERROR_TRANSITION_PEAK_REFUSED,
	RUNTIME_ERROR_TRANSITION_IN_PROGRESS,
	RUNTIME_ERROR_UNKNOWN_WORLD,
	RUNTIME_ERROR_WORLD_CREATE,          // tool access: CreateWorld failed (m_uModuleIndex: the failing module or 0xFFFF)
	RUNTIME_ERROR_WORLD_SCHEDULED,       // tool access: AddToolExtractor after the world joined a compiled schedule
	RUNTIME_ERROR_REGISTRATION,          // tool access: the world's system registry refused the extractor
	RUNTIME_ERROR_QUEUE_FULL,            // tool access: QueueSchemaJson over 64 files or 8 MiB
	RUNTIME_ERROR_FRAME_FAILED,          // tool access: RunOneFrame ended with an unrecoverable runtime error
};

constexpr const char* Zenith_ToString(Zenith_RuntimeErrorCode eCode);

/// @contract enum Zenith_StartupStep
/// Ownership: A plain value.
/// Thread affinity: IMMUTABLE. A value type.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2; the values are the step numbers of Zenith2/Docs/Architecture/CompositionRoot.md → "Startup sequence"
///   and change only with that table.
/// @endcontract
enum Zenith_StartupStep : uint8
{
	STARTUP_STEP_ROOT_HEAP = 1,        // Player
	STARTUP_STEP_PROFILER_HOOKS,       // Player
	STARTUP_STEP_COMMAND_LINE,         // Player
	STARTUP_STEP_PLATFORM,
	STARTUP_STEP_FILE_SYSTEM,
	STARTUP_STEP_CONFIGURATION,
	STARTUP_STEP_LOGGING,
	STARTUP_STEP_MEMORY_GOVERNOR,
	STARTUP_STEP_JOB_SYSTEM,
	STARTUP_STEP_REGISTRATION,
	STARTUP_STEP_ASSETS,
	STARTUP_STEP_RHI_DEVICE,
	STARTUP_STEP_RENDERER,
	STARTUP_STEP_SCRIPT_RUNTIME,
	STARTUP_STEP_MODULE_START,
	STARTUP_STEP_INITIAL_WORLD,
	STARTUP_STEP_STARTUP_SCENE,
	STARTUP_STEP_SCHEDULE,
	STARTUP_STEP_FRAME_LOOP,
};

inline constexpr uint32 uZENITH_STARTUP_STEP_COUNT = 19;

/// @contract value Zenith_RuntimeError
/// Ownership: A plain value returned inside a Zenith_Result.
/// Thread affinity: IMMUTABLE. A value type.
///   Members non-const: it is the E of a Zenith_Result, whose move assignment assigns it.
/// Invalidation: None.
/// Units: None.
/// Errors: m_eCode names the failure; for RUNTIME_ERROR_STARTUP_STEP, m_eStep is the failing step and m_uModuleIndex the
///   failing module (0xFFFF when no module failed). The error form of [Z2-N-CONTRACT-480]: 4 bytes, no string.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_RuntimeError
{
	Zenith_RuntimeErrorCode m_eCode;
	Zenith_StartupStep m_eStep = static_cast<Zenith_StartupStep>(0);   // 0 unless RUNTIME_ERROR_STARTUP_STEP or RUNTIME_ERROR_CONFIGURATION
	uint16 m_uModuleIndex = 0xFFFFu;
};

/// @contract value Zenith_StartupReport
/// Ownership: Owned by the Player's Zenith_PlayerMain, which times steps 1–3 into it; Zenith_RuntimeMain passes it to
///   Zenith_Runtime::Create, which fills the rest.
/// Thread affinity: MAIN_THREAD. Written during Create, read afterwards.
/// Invalidation: None.
/// Units: Microseconds for m_aulStepMicroseconds, from the platform's monotonic clock.
/// Errors: None; it describes an error, it has none of its own.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_StartupReport
{
	Zenith_StartupStep m_eLastCompletedStep = static_cast<Zenith_StartupStep>(0);   // 0: none
	Zenith_StartupStep m_eFailedStep = static_cast<Zenith_StartupStep>(0);          // 0: no failure
	Zenith_ExitCode m_eExitCode = EXIT_CODE_NORMAL;
	uint16 m_uFailedModuleIndex = 0xFFFFu;             // index into the composition's m_axModules, or 0xFFFF
	Zenith_ModuleErrorCode m_eModuleError = static_cast<Zenith_ModuleErrorCode>(0);
	uint16 m_uStepsUnwound = 0;
	uint64 m_aulStepMicroseconds[uZENITH_STARTUP_STEP_COUNT] = {};                 // [step − 1]; steps 1–3 are the Player's
	char m_acFailedModuleName[32] = {};             // the failing module's MODULE_NAME, NUL-terminated
	char m_acCause[160] = {};                       // the failing step's one-line cause, NUL-terminated, truncated at 159 bytes
};

/// @contract type Zenith_Runtime
/// Ownership: Created by Zenith_Runtime::Create as a Zenith_Owned; one instance per process, owned by Zenith_RuntimeMain.
///   Owns every engine subsystem, module instance and world it creates.
/// Thread affinity: MAIN_THREAD. Every member runs on the main thread.
/// Invalidation: None while it lives; destroying it runs the shutdown order and invalidates every handle into the worlds.
/// Units: None.
/// Errors: Create returns Zenith_RuntimeError and fills Zenith_StartupReport; RequestWorldTransition returns the
///   transition refusals; RunFrameLoop returns an exit code.
/// Reentrancy: NONE. No member is called from inside frame work or a module callback.
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_Runtime
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_Runtime>, Zenith_RuntimeError> Create(
		const Zenith_CommandLine& xCommandLine, const Zenith_GameComposition& xComposition, Zenith_Heap& xRootHeap,
		Zenith_LogRouter& xLogRouter, Zenith_OOMPressureFlag& xOOMPressureFlag, Zenith_Profiler& xProfiler,
		Zenith_StartupReport& xReportOut);                                    // startup steps 4–18
	~Zenith_Runtime();                                                        // the shutdown order
	Zenith_ExitCode RunFrameLoop();                             // step 19
	Zenith_Result<Zenith_OperationHandle, Zenith_RuntimeError> RequestWorldTransition(
		uint16 uFromWorldTag, const Zenith_WorldTransitionDesc& xDesc);      // GAME's semantics
	void RequestShutdown();                                                   // effective when the current frame-loop iteration returns
	void RecordTestAssertionFailure();   // ANY_THREAD (core text since X344): the normal exit becomes EXIT_CODE_TEST_FAILED
	const Zenith_GameComposition& GetComposition() const;
	const Zenith_StartupReport& GetStartupReport() const;
};

/// @contract functions RuntimeMain
/// Ownership: Creates, runs and destroys one Zenith_Runtime on the caller's stack frame.
/// Thread affinity: MAIN_THREAD. Called once, from Zenith_PlayerMain.
/// Invalidation: None.
/// Units: None.
/// Errors: Returns an exit code of Zenith_ExitCode; never returns an error value.
/// Version: since Z2 Stage 2.
/// @endcontract
Zenith_ExitCode Zenith_RuntimeMain(const Zenith_CommandLine& xCommandLine,
	const Zenith_GameComposition& xComposition, Zenith_Heap& xRootHeap, Zenith_LogRouter& xLogRouter,
	Zenith_OOMPressureFlag& xOOMPressureFlag, Zenith_Profiler& xProfiler,
	Zenith_StartupReport& xReportInOut);   // the Player's report, with steps 1–3 already timed
```

<!-- elaboration -->
Architecture declares `RunFrameLoop` and `Zenith_RuntimeMain` as returning `int`; they return `Zenith_ExitCode`, whose values are that `int` table, and `Zenith_PlayerMain` converts with `static_cast<int>` (part of reconciliation R1's patch). `Create` gains the `Zenith_StartupReport&` out-parameter of reconciliation R2: the error record carries the step and module index, and the report carries everything textual and every timing. The report is a fixed-size value on `Zenith_RuntimeMain`'s stack (192 bytes of text plus the timings), so reporting a failure allocates nothing.

Every `Create` form and `Zenith_RuntimeMain` take the Player's `Zenith_LogRouter&`, `Zenith_OOMPressureFlag&` and `Zenith_Profiler&` directly after `xRootHeap`, in that order ([Z2-N-ARCH-876]); the runtime state references all three and destroys none, and the profiler reaches `Zenith_JobSystemDesc::m_pxProfiler` at step 9 and DEVT's `Zenith_DevTools` at step 7 (A3 cross-area rows DV3, DV12). The shared create implementation's first statement, before step 4 and in every host mode, is `ZENITH_CHECK(Zenith_IsCrtMathModeApplied())` (`ZenithCore/Zenith_Transcendental.h`), so a `main` that forgot `Zenith_ApplyCrtMathMode()` fails every Windows run, Release included (A3 cross-area row C20; [Z2-N-ARCH-889]).

### Zenith_RuntimeTestAccess.h

```cpp
// Zenith2/Engine/ZenithRuntime/Zenith_RuntimeTestAccess.h
using Zenith_PfnStartupStepHook = bool (*)(Zenith_StartupStep eStep, void* pUserData);   // false: inject a failure
using Zenith_PfnStartupUnwindHook = void (*)(Zenith_StartupStep eStep, void* pUserData);

/// @contract value Zenith_RuntimeTestHooks
/// Ownership: Owned by the test; the runtime copies it during Create.
/// Thread affinity: MAIN_THREAD. Hooks are called on the main thread during Create and destruction.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Reentrancy: NONE. A hook calls no Zenith_Runtime member and installs no hook.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_RuntimeTestHooks
{
	Zenith_PfnStartupStepHook m_pfnBeforeStep = nullptr;       // called before each of steps 4–18
	Zenith_PfnStartupStepHook m_pfnAfterStep = nullptr;        // called after each succeeds; false fails it after construction
	Zenith_PfnStartupUnwindHook m_pfnOnUnwind = nullptr;       // called as each constructed step is destroyed
	void* m_pUserData = nullptr;
};

/// @contract type Zenith_RuntimeTestAccess
/// Ownership: A static-only access type; holds no state.
/// Thread affinity: MAIN_THREAD. As Zenith_Runtime.
/// Invalidation: None.
/// Units: None.
/// Errors: As Zenith_Runtime::Create.
/// Reentrancy: NONE. A hook calls no Zenith_Runtime member and installs no hook.
/// Version: since Z2 Stage 2; compiled in every configuration and used only by test targets (Zenith2/Docs/Contracts.md
///   → "Tests and code hygiene gates": no test-mode switch in shipping code).
/// @endcontract
class Zenith_RuntimeTestAccess
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_Runtime>, Zenith_RuntimeError> Create(
		const Zenith_CommandLine& xCommandLine, const Zenith_GameComposition& xComposition, Zenith_Heap& xRootHeap,
		Zenith_LogRouter& xLogRouter, Zenith_OOMPressureFlag& xOOMPressureFlag, Zenith_Profiler& xProfiler,
		const Zenith_RuntimeTestHooks& xHooks, Zenith_StartupReport& xReportOut);
};
```

`Zenith_Runtime::Create` is `Zenith_RuntimeTestAccess::Create` with an all-null hook table; the two share one implementation in `Internal/Zenith_RuntimeCreate.cpp`, so the tested path is the shipped path. The world-host hooks `m_pfnOnSwapStep` and `m_pfnOnTeardownStep`, `GetGameFlowState` and `CreatePurposedWorld` are Zenith2/Docs/Design/MOD/RuntimeSeams.md → "Swap precompile and test hooks" and → "World attach context lookups"; `m_pxFaults`, `m_pfnOnShutdownStep`, the test mount sources, `m_strBasePathOverride` and `GetPlatformBasePath` are → "Runtime test seams for modules"; `GetEntitlementProviderModule` is → "Start context members for ONLINE and PACK". The tool host's access header, `Zenith_RuntimeToolAccess.h`, is → "Runtime tool access".

### Zenith_CommandLine.h

The option table is Zenith2/Docs/Architecture/CompositionRoot.md → "Startup sequence" ("Command-line options"); the accessors are Architecture's. Reconciliations R1 and R2 change `Parse`'s parameters and make the error record string-free:

```cpp
// Zenith2/Engine/ZenithRuntime/Zenith_CommandLine.h (the parts this design changes)
/// @contract enum Zenith_CommandLineErrorCode
/// Ownership: A plain value.
/// Thread affinity: IMMUTABLE. A value type.
/// Invalidation: None.
/// Units: None.
/// Errors: COMMAND_LINE_ERROR_UNKNOWN_OPTION, COMMAND_LINE_ERROR_MISSING_ARGUMENT, COMMAND_LINE_ERROR_BAD_VALUE;
///   every one maps to exit code 2.
/// Version: since Z2 Stage 2.
/// @endcontract
enum Zenith_CommandLineErrorCode : uint8
{
	COMMAND_LINE_ERROR_UNKNOWN_OPTION = 1,
	COMMAND_LINE_ERROR_MISSING_ARGUMENT,
	COMMAND_LINE_ERROR_BAD_VALUE,
};

/// @contract value Zenith_CommandLineError
/// Ownership: A plain value returned inside a Zenith_Result.
/// Thread affinity: IMMUTABLE. A value type.
///   Members non-const: it is the E of a Zenith_Result, whose move assignment assigns it.
/// Invalidation: None.
/// Units: None.
/// Errors: m_eCode names the failure and m_uArgIndex the offending argument (an index into ppszArgv, never 0), so the
///   caller prints ppszArgv[m_uArgIndex]. The error form of [Z2-N-CONTRACT-480]: 4 bytes, no string.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_CommandLineError
{
	Zenith_CommandLineErrorCode m_eCode;
	uint8 m_uReserved = 0;
	uint16 m_uArgIndex = 0;
};

enum Zenith_CommandLineFileOption : uint8
{
	COMMAND_LINE_FILE_CONFIG_DUMP = 1,
	COMMAND_LINE_FILE_RECORD,
	COMMAND_LINE_FILE_REPLAY,
	COMMAND_LINE_FILE_INPUT_SCRIPT,
	COMMAND_LINE_FILE_EVIDENCE_DIR,
	COMMAND_LINE_FILE_SCRIPT_MIGRATION_FIXTURES,   // since Z2 Stage 3: the second directory grant, read-only
};

// Architecture's budget-run enums, unchanged (Zenith2/Docs/Budgets.md → "Frame-rate definitions and measurement procedure").
enum Zenith_BudgetRunCacheState : uint8 { BUDGET_RUN_CACHE_NONE = 0, BUDGET_RUN_CACHE_COLD, BUDGET_RUN_CACHE_WARM };
enum Zenith_BudgetRunConditions : uint8 { BUDGET_RUN_CONDITIONS_NONE = 0, BUDGET_RUN_CONDITIONS_CONTROLLED, BUDGET_RUN_CONDITIONS_ORDINARY };

// class Zenith_CommandLine: Architecture's accessors unchanged, among them GetFileGrant and the budget-run accessors
//   const Zenith_CommandLineFileGrant* GetFileGrant(Zenith_CommandLineFileOption eOption) const;  // nullptr when absent
//   bool IsBudgetRun() const;                                     // --budget-run given
//   Zenith_StringView GetBudgetRunWorkload() const;               // the --budget-run argument; empty outside a budget run
//   uint32 GetWorkloadVersion() const;                           // --workload-version; 0 outside a budget run
//   Zenith_BudgetRunCacheState GetCacheState() const;             // --cache-state; BUDGET_RUN_CACHE_NONE outside
//   Zenith_BudgetRunConditions GetConditions() const;             // --conditions; BUDGET_RUN_CONDITIONS_NONE outside
//   uint32 GetRepetition() const;                                // --repetition; 0 outside a budget run
// and Parse becomes
static Zenith_Result<Zenith_CommandLine, Zenith_CommandLineError> Parse(int iArgc, char** ppszArgv);
```

Parsing rules:

- `ppszArgv[0]` is the executable path and is skipped; every other argument is UTF-8; at most 65,535 arguments are read, a longer command line being `COMMAND_LINE_ERROR_BAD_VALUE` at index 65,535.
- An option is exactly one of the spellings of the table; `--set` takes `<key>=<value>` in the next argument (the first `=` splits it; an argument without `=` is `COMMAND_LINE_ERROR_BAD_VALUE`); every other option with an argument takes the next argument; a missing next argument is `COMMAND_LINE_ERROR_MISSING_ARGUMENT`.
- `--exit-after-ticks` and `--timeout-seconds` take decimal integers 1 … 2⁶³−1 and 1 … 86,400; anything else is `COMMAND_LINE_ERROR_BAD_VALUE`.
- A repeated option other than `--set` is `COMMAND_LINE_ERROR_BAD_VALUE`; repeated `--set` keeps the command-line order, and the configuration layer applies them in that order (the last one wins).
- `--assets-root` in a Shipping build is `COMMAND_LINE_ERROR_UNKNOWN_OPTION` (the loose mount does not exist there, [Z2-N-ARCH-922]).
- The budget-run switches follow Zenith2/Docs/Architecture/CompositionRoot.md → "Command-line options" and [Z2-N-ARCH-878]: `--workload-version` takes a decimal integer 1 … 2³²−1, `--repetition` 1 … 1,000, `--cache-state` exactly `cold` or `warm`, `--conditions` exactly `controlled` or `ordinary`, anything else being `COMMAND_LINE_ERROR_BAD_VALUE`; `--workload-version`, `--cache-state`, `--conditions` or `--repetition` without `--budget-run`, or a repeated budget-run switch, is `COMMAND_LINE_ERROR_BAD_VALUE`; a budget run without `--evidence-dir` is `COMMAND_LINE_ERROR_MISSING_ARGUMENT`; in a Shipping build `--budget-run`, its four companion switches and `--evidence-dir` are `COMMAND_LINE_ERROR_UNKNOWN_OPTION`. Each is exit code 2.
- The command line constructs one `Zenith_CommandLineFileGrant` (declared by PLAT in `ZenithPlatform/Zenith_HostFile.h`, constructible only by `Zenith_CommandLine`, `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Command-line host files") per host-path option given, and hands it out through `GetFileGrant`; in Shipping builds those options are rejected by the parser.
- The command line holds views into `main`'s argument strings and allocates nothing beyond a fixed-capacity array of 64 `--set` views; a 65th `--set` is `COMMAND_LINE_ERROR_BAD_VALUE`.

- From Stage 3, `--script-migration-fixtures <dir>` (non-Shipping; an empty or repeated argument is `COMMAND_LINE_ERROR_BAD_VALUE`) constructs the read-only directory grant `COMMAND_LINE_FILE_SCRIPT_MIGRATION_FIXTURES`, through which the runtime reads a game's migration fixtures (§ "The gametest format"; A5 cross-area rows SB30, MB27). `Zenith2/Docs/Architecture/CompositionRoot.md` → "Command-line options" lists the option and the enumerator, [Z2-N-ARCH-878] makes it `COMMAND_LINE_ERROR_UNKNOWN_OPTION` in Shipping, and [Z2-N-ARCH-923] (`Zenith2/Docs/Architecture/PlatformLayer.md` → "Path validation") admits the read-only directory grant (cross-doc item X311, row MB27 (1)); Z2T-MOD-068 stays `draft` on rows MB27 (2) and (3), PLAT's directory read and SCRIPT's declaration.

### Zenith_ExitCode.h

```cpp
// Zenith2/Engine/ZenithRuntime/Zenith_ExitCode.h
/// @contract enum Zenith_ExitCode
/// Ownership: A plain value.
/// Thread affinity: IMMUTABLE. A value type.
/// Invalidation: None.
/// Units: None.
/// Errors: None; the values are the process exit codes of Zenith2/Docs/Architecture/CompositionRoot.md → "Startup sequence"
///   ("Exit codes"), which gate steps and CI branch on.
/// Version: since Z2 Stage 2; a value never changes meaning.
/// @endcontract
enum Zenith_ExitCode : uint8
{
	EXIT_CODE_NORMAL = 0,
	EXIT_CODE_RUNTIME_ERROR = 1,
	EXIT_CODE_USAGE = 2,
	EXIT_CODE_STARTUP_VALIDATION = 3,
	EXIT_CODE_DEVICE = 4,
	EXIT_CODE_TEST_FAILED = 5,
	EXIT_CODE_DEVICE_LOST = 6,       // the graphics device was lost and could not be recovered; autosave requested
	EXIT_CODE_WATCHDOG = 124,
};
```

### Player headers

```cpp
// Zenith2/Player/Zenith_GameComposition.h
#pragma once
#include "ZenithRuntime/Zenith_GameComposition.h"

/// @contract functions GameComposition
/// Ownership: Defined by each game's generated TU; returns a reference to its constexpr composition.
/// Thread affinity: ANY_THREAD. It returns constant data.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 0 (the stub composition), full form since Z2 Stage 2.
/// @endcontract
const Zenith_GameComposition& Zenith_GetGameComposition();

// Zenith2/Player/Zenith_PlayerMain.h
/// @contract value Zenith_PlayerArgs
/// Ownership: Owned by the platform entry point; the arguments outlive every engine object.
/// Thread affinity: MAIN_THREAD. Created and read on the main thread.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 0.
/// @endcontract
struct Zenith_PlayerArgs
{
	int m_iArgc = 0;
	char** m_ppszArgv = nullptr;        // UTF-8 (converted from GetCommandLineW on Windows)
};

/// @contract functions PlayerMain
/// Ownership: Owns the root heap and the profiler hooks for the process lifetime.
/// Thread affinity: MAIN_THREAD. Called once by the platform entry point.
/// Invalidation: None.
/// Units: None.
/// Errors: Returns a process exit code of the table "Exit codes".
/// Version: since Z2 Stage 0.
/// @endcontract
int Zenith_PlayerMain(const Zenith_PlayerArgs& xArgs);
```

The platform entry (`main` on Windows, `SDL_main` on Android) calls `Zenith_ApplyCrtMathMode()` as its first statement, before any other code and before the root heap ([Z2-N-ARCH-889]; [Z2-N-CONTRACT-142] as reconciled by core-doc patch f; A3 cross-area row C20), then converts its arguments and calls `Zenith_PlayerMain`, which does, in order:

1. declare the `Zenith_OOMPressureFlag` on its stack, construct the root `Zenith_Heap` there with the flag as `Zenith_HeapDesc::m_pOOMUserData`, and `Zenith_InstallRootHeap(&xHeap)` (step 1); the statement directly after it is DEVT's crash-helper dispatch, `int iExit = 0; if (Zenith_RunCrashHelperIfRequested(xArgs.m_iArgc, xArgs.m_ppszArgv, Zenith_CrashHelperIdentity{ Zenith_GetGameComposition().m_szGameName, Zenith_GetGameComposition().m_ulBuildIdentity }, iExit)) { Zenith_InstallRootHeap(nullptr); return iExit; }`, added by DEVT's crash-wiring ticket ([Z2-N-ARCH-860]; `Zenith2/Docs/Design/DEVT/Crash.md` → "Crash handler lifecycle"; row DV11), so a relaunched crash helper never reaches step 1b;
2. `Zenith_LogRouter::Create` (allocations only: no thread, no lock, no hook called), the one engine object created between steps 1 and 2 (CORE's step 1b); on failure write `Zenith2: log router allocation failed` to standard error, uninstall the root heap and return 4 without installing any hook;
3. `Zenith_Profiler::Create(Zenith_GetPlayerProfilerDesc(Zenith_GetBuildPlatform(), Zenith_GetBuildConfiguration(), Zenith_IsSanitizerBuild()))`, the one profiler (Tracy and the on-screen capture inside it in development builds, counters only in Shipping); on failure write `Zenith2: profiler creation failed: <error>` to standard error, destroy the router, uninstall the heap and return 4; then `pxProfiler->FillHooks(xHooks)`, the router's `FillHooks(xHooks)` and `Zenith_InstallProfilerHooks(&xHooks)` (step 2; `Zenith2/Docs/Design/DEVT/Profiler.md` → "Creation and shutdown", row DV12);
4. `Zenith_CommandLine::Parse(xArgs.m_iArgc, xArgs.m_ppszArgv)` (step 3, timed into the report of item 5), on failure writing `Zenith2: unknown or malformed option '<xArgs.m_ppszArgv[m_uArgIndex]>'` to standard error and returning 2 without creating any runtime object;
5. `Zenith_RuntimeMain(xCommandLine, Zenith_GetGameComposition(), xHeap, xRouter, xOOMPressureFlag, *pxProfiler, xReport)` ([Z2-N-ARCH-876]), where `xReport` is the Player's `Zenith_StartupReport` with the durations of steps 1–3 already in `m_aulStepMicroseconds[0 … 2]`; the runtime starts the router's drain thread at step 7;
6. destroy the log router, which joins its drain thread (Contracts' shutdown step 11, whose "Threads joined" cell gains the log drain thread: cross-area note M36);
7. `Zenith_InstallProfilerHooks(nullptr)`, then destroy the profiler (which joins `Zenith.ProfFlush`), then `Zenith_InstallRootHeap(nullptr)`, then destroy the heap (the Debug leak report);
8. return the exit code.

The Player is the only place the root heap, the hooks, the log router and the profiler are installed or created ([Z2-N-ARCH-846]). The CMake check `ZenithPlayer.EntryCallsCrtMathModeFirst` (a `cmake -P` source scan registered in `Zenith2/CMake/Tests/ZenithCMakeTests.cmake`, CORE's `ZenithCore.TranscendentalNoCrtCall` precedent) reads both entry files and fails unless the first statement of the `main` and `SDL_main` bodies is `Zenith_ApplyCrtMathMode();`.

<!-- assumption: pending CORE -->
The order is CORE's landed contract (`Zenith2/Docs/Design/CORE/Diagnostics.md` → "Log router", the `Zenith_LogRouter` contract block): the router is "Created by Create … in main after the root heap (startup step 1b; Create starts no thread); the composition root registers the sinks, resizes the ring and starts the drain thread at startup step 7". The Player creates the router and fills and installs the hooks; ZenithRuntime's step 7 performs, in order, `SetGlobalLevel` and `SetCategoryLevel` from `log.level` and `log.categoryLevels`, `AddSink` for every sink, `ResizeRing` from `log.ringBytes`, and `StartDrainThread()` (§ "Step construction notes"). Step 1b is an addition to Architecture's startup table, carried with cross-area note M36 (which supersedes M33).
<!-- /assumption -->

### Module root headers

A module root header has exactly this shape (Physics shown; a plain module omits the second `Start` parameter; Online takes the backend list):

```cpp
// Zenith2/Engine/ZenithPhysics/Zenith_Module_Physics.h
#pragma once
#include "ZenithPhysics/Zenith_Physics.h"          // Zenith_PfnCreatePhysics
#include "ZenithRuntime/Zenith_Module.h"

/// @contract functions Module_Physics
/// Ownership: RegisterTypes registers into the caller's registry; Start returns the module instance as a Zenith_Owned.
/// Thread affinity: MAIN_THREAD. Called by ZenithRuntime during startup.
/// Invalidation: None.
/// Units: None.
/// Errors: Zenith_ModuleError.
/// Version: since Z2 Stage 3.
/// @endcontract
Zenith_Result<void, Zenith_ModuleError> Zenith_Module_Physics_RegisterTypes(Zenith_TypeRegistry& xTypes);
Zenith_Result<Zenith_Owned<Zenith_ModuleInstance>, Zenith_ModuleError> Zenith_Module_Physics_Start(
	Zenith_ModuleStartContext& xContext, Zenith_PfnCreatePhysics pfnCreateBackend);
```

### Handle kinds used

| Handle kind (Zenith2/Docs/Contracts.md → "Handle kinds") | Where MOD uses it |
|---|---|
| `Zenith_OperationHandle` | `RequestWorldTransition` returns one (GAME's semantics); shutdown cancels every outstanding operation (Zenith2/Docs/Contracts.md → "Threading and concurrency contract", "Shutdown ownership order" step 4) |
| world tags (`uint16`, 1 … 4095) | `AttachWorld`/`DetachWorld` and `Zenith_WorldAttachContext::GetWorldTag` identify a world by its tag from `Zenith_WorldRegistry` |
| `Zenith_Owned<T>` | every subsystem, module instance and world the runtime owns (not a handle kind; the ownership type of Zenith2/Docs/Contracts.md → "Ownership and allocation") |

MOD introduces no handle kind.

## Data structures and formats

### The composition.json record

`${ZENITH_OUT}/Games/<Name>/composition.json`, written by `zenith_add_game` in every preset with identical bytes, is the resolved, platform-independent record of one game's composition. Keys appear in the order shown, arrays are byte-ordered unless marked, indentation is four spaces, line endings LF, and the file is rewritten only when its bytes change.

```json
{
    "schema": "zenith2.game-composition",
    "schemaVersion": 1,
    "name": "OpenWorldSlice",
    "version": "0.1.0",
    "platforms": ["android", "windows"],
    "modules": ["Animation", "Audio", "Cinematics", "Environment", "Gameplay", "Online", "Physics", "AI", "Streaming", "UI", "VFX"],
    "ecs": "Flecs",
    "physics": { "android": "Jolt", "windows": "PhysX" },
    "online": { "android": ["Offline"], "windows": ["Offline"] },
    "contentRoots": ["Assets", "Scenes", "Schemas", "Scripts"],
    "startupScene": "content:/Scenes/Main.scene.json",
    "scriptRecovery": null,
    "compositionHash": { "android": "8a0c2e6f1d3b5a79", "windows": "5d1f0a3c9e7b2248" }
}
```

| Key | Meaning |
|---|---|
| `modules` | canonical order (§ "Canonical module order"), not byte order |
| `physics` | an object keyed by platform whenever `Physics` is selected (the string form of `game.json` is expanded); `null` otherwise |
| `online` | an object keyed by platform whenever `Online` is selected, each value the Online backend names linked on that platform in composition order, derived like `compositionHash` from the declaration store with the platform's representative preset; `null` otherwise. It is here so an evidence fingerprint over this file distinguishes, for example, a Steam-linked Windows build from an Offline-only one, which the composition hash does not |
| `scriptRecovery` | `null`, or `{ "policy", "delayTicks", "maxRecoveriesPerModule" }` as declared |
| `compositionHash` | the per-platform value of § "Composition hash" in its printed spelling; each preset computes its own platform's value from its resolution and every other platform's value from the declaration store, taking that platform's RHI and physics backend sets from the `ZENITH_DECL_<Lib>_SUPPORTED` lists evaluated for its representative preset (`win` for `windows`, `android-arm64` for `android`), so every preset writes the same object; the per-preset build identity is not recorded here |

Consumers: the cook (`ZenithCook` reads `modules` to register exactly the selected set), the editor (which game to open with which modules), and the evidence fingerprint (DEVT), which hashes this file into a game's evidence input.

### Canonical module order

`zenith_add_game` orders the selected modules topologically by their module dependencies, breaking every tie by byte order of `MODULE_NAME` ([Z2-N-ARCH-844]):

1. `ready` := the selected modules with no selected module dependency, sorted by byte order.
2. While `ready` is non-empty: remove its first element `m`, append it to the result; for each selected module `d` that depends on `m`, remove the edge; when `d` has no remaining edge, insert it into `ready` keeping byte order.
3. The dependency graph is acyclic by Zenith2/Docs/Architecture.md → "Library dependency graph" ([Z2-N-ARCH-072]); a result shorter than the selection is an engine defect and a `FATAL_ERROR` naming the modules left over.

The module count is at most 13, so the quadratic implementation (`list(SORT)` after each insertion) is exact and cheap. For the selection `UI, Physics, AI, Animation, Cinematics` the order is `Animation, Cinematics, Physics, AI, UI`: `Animation`, `Physics` and `UI` are ready; `Animation` goes first and releases `Cinematics`, which sorts before `Physics`; `Physics` releases `AI`, which sorts before `UI`.

### Composition hash

`m_ulCompositionHash` identifies the composition a binary was built from, for logs, replays and evidence records. `zenith_add_game` computes it exactly as [Z2-N-ARCH-864] states:

1. Build the text of LF-joined lines, UTF-8, with no trailing LF: the game name; the game version; each module name in canonical order, one per line; the ECS backend name; the physics backend name for the platform, or `none`; the RHI backend names in table order, one per line; the codec names in table order (`zlib`, `zstd`), one per line.
2. `string(SHA256 strDigest "${strText}")`, where `strText` joins the lines with `\n` (a game with no module contributes no module line).
3. Take the first 16 hexadecimal digits of `strDigest` as eight bytes b0 … b7 (b0 the first digit pair) and read them as a little-endian `uint64`: the value is b0 + b1·2⁸ + … + b7·2⁵⁶.
4. The one printed spelling of the hash is the digest's first 16 lower-case hexadecimal digits exactly as `string(SHA256)` prints them (b0 b1 … b7). The configure report, `composition.json`, the TU's header comment, the startup dump and the build-identity input all use that spelling; the runtime prints `m_ulCompositionHash` by emitting its bytes from the least significant (b0) to the most significant (b7), which reproduces it. The TU's `0x…ull` literal is the numeric value, so its digits are the byte pairs in reverse order (b7 … b0).

The inputs name only what the executable links, so two presets of one platform (`win` and `win-asan`) share a composition hash, and the Windows and Android values of one game differ exactly when their RHI or physics sets differ.

### Build identity

<!-- elaboration -->
`m_ulBuildIdentity` identifies the preset-specific build of a composition. It carries the inputs [Z2-N-ARCH-864] leaves out, so the composition hash stays exactly Architecture's:

1. The LF-joined lines, UTF-8, no trailing LF and no trailing space on any line: the composition hash in its printed spelling (§ "Composition hash" step 4); the configure preset name (`ZENITH_PRESET`); the Online backend names in composition order, one per line, or the literal ASCII line `none` when `Online` is not selected; `uZENITH_MODULE_INTERFACE_VERSION` in decimal; the startup scene's virtual path; the script-recovery settings as `<policy>,<delayTicks>,<maxRecoveriesPerModule>` (for example `automatic,60,3`), or the literal ASCII line `default` when `scriptRecovery` is absent.
2. SHA-256, the little-endian value and the one printed spelling exactly as the composition hash's steps 2–4.

### Hash golden

The values for `Games/Minimal` (no module, Custom ECS, no physics, no Online, `scriptRecovery` absent, interface version 1) on the `win` preset, which `CMake_Composition_NoModules` pins:

| Value | Input lines (LF-joined, no trailing LF) | SHA-256 | Printed spelling | TU literal |
|---|---|---|---|---|
| composition hash | `Minimal`, `0.1.0`, `Custom`, `none`, `Vulkan`, `D3D12`, `zlib`, `zstd` | `704cef4b6c1da71a90243020859a923a7b90d6ccb6183f8a1972d3d3f5698e6a` | `704cef4b6c1da71a` | `0x1aa71d6c4bef4c70ull` |
| build identity | `704cef4b6c1da71a`, `win`, `none`, `1`, `content:/Scenes/Main.scene.json`, `default` | `fde0475d91f2eebbb28d99cbd90bb98ba2a1be91ce181c3a835bd15ddf85c4c2` | `fde0475d91f2eebb` | `0xbbeef2915d47e0fdull` |

<!-- assumption: pending ARCH -->
`m_ulBuildIdentity` is not yet in Architecture: Zenith2/Docs/Architecture/CompositionRoot.md → "The generated composition table" currently ends `Zenith_GameComposition` with "`uint64 m_ulCompositionHash;                                     // recipe below; for logs, replays and evidence`" and its example `xCOMPOSITION` carries one hash literal. The field follows it in the batched Architecture patch (cross-area note M25).
<!-- /assumption -->

`uZENITH_MODULE_INTERFACE_VERSION` is read by `zenith_add_game` from `Engine/ZenithRuntime/Zenith_Module.h` with `file(STRINGS … REGEX "uZENITH_MODULE_INTERFACE_VERSION = [0-9]+")`, and that header is added to `CMAKE_CONFIGURE_DEPENDS`, so a version bump regenerates every composition. The build identity appears in the configure report, the startup dump and the header comment of the generated TU; it is not written to `composition.json`, which is platform-independent and identical from every preset.

<!-- informative -->
SHA-256 is used, not the engine's `Zenith_Hash64`, because both values are computed in CMake, which has SHA-256 built in and no xxHash; their consumers compare them for equality.
<!-- /informative -->

### Startup scene and loose content root

- The startup scene's virtual path is `content:/` followed by the game-relative path of `content.startupScene`, with a first component of `Assets/` removed: `Scenes/Main.scene.json` becomes `content:/Scenes/Main.scene.json`, and `Assets/Levels/Intro.scene.json` becomes `content:/Levels/Intro.scene.json`.
- ZenithRuntime registers the configuration key `content.startupScene` (type string, allowed in the game and command-line layers) with the composition's `m_xSettings.m_szStartupScene` as its engine default, so `--set content.startupScene=content:/Scenes/Other.scene.json` boots a different scene without rebuilding, which the workload runs and scene tests use.
- In non-Shipping Windows builds without `--assets-root`, the runtime mounts `<executable directory>/../../../Cooked/<m_szGameName>/windows/` as the loose mount; that is `${ZENITH_OUT}/Cooked/<Name>/windows/` because the executable is at `${ZENITH_OUT}/<preset>/<Config>/bin/`. On Android, the loose mount is PLAT's `adb push` directory (Zenith2/Docs/Architecture/PlatformLayer.md → "Virtual filesystem").

<!-- assumption: pending ASSET -->
The cook maps each content root to the virtual root as above: files under `Assets/` at `content:/<path below Assets>`, files under any other root `R` at `content:/R/<path below R>`, and reports two files mapping to one virtual path as a cook error. This matches the game configuration layer's location `content:/Config/game.config.json` from `Games/<Name>/Assets/Config/` (Zenith2/Docs/Architecture/PlatformLayer.md → "Configuration layering").
<!-- /assumption -->

<!-- assumption: pending PLAT -->
`Zenith_Platform` exposes the executable's directory as an OS path for the loose-root default; the PLAT draft (`Zenith2/Docs/Design/PLAT/Platform.md`, not yet signed off) names it `Zenith_Platform::GetBasePath()` (`SDL_GetBasePath`, UTF-8, trailing `/`), and the runtime uses that accessor.
<!-- /assumption -->

### The gametest format

<!-- elaboration -->
`Games/<Name>/Tests/<Stem>.gametest.json` declares one headless boot test of the game (registration: Zenith2/Docs/Design/MOD/GameBuild.md → "Game test registration").

```json
{
	"schemaVersion": 1,
	"ticks": 300,
	"timeoutSeconds": 60,
	"inputScript": "Tests/WalkForward.input.json",
	"set": { "content.startupScene": "content:/Scenes/TestArena.scene.json" }
}
```

| Key | Type | Presence | Rule |
|---|---|---|---|
| `schemaVersion` | integer | always | `1` |
| `ticks` | integer | always | 1–100,000; passed as `--exit-after-ticks` |
| `timeoutSeconds` | integer | always | 1–600; passed as `--timeout-seconds` |
| `inputScript` | string | optional | a game-relative path beginning `Tests/` to an existing file; passed as `--input-script` with the absolute path |
| `set` | object of strings | optional | each key a dotted configuration key, each value a string; passed as `--set <key>=<value>` in byte order of key |
| `requiresPackage` | boolean | optional (default `false`) | `true`: the test runs only in the Debug configuration, after the game's Debug package (PACK's `<Name>_Package`) has been built (Zenith2/Docs/Design/MOD/GameBuild.md → "Game test registration", step 4) |
| `x-<anything>` | any | optional | ignored |

`requiresPackage` is an optional key added to version 1. Every existing file is unchanged and keeps its meaning, so `schemaVersion` stays 1 (Zenith2/Docs/Contracts.md → "Compatibility policy per kind"). A gametest names files of the package it depends on through its own `set` values, relative to its assets root `${ZENITH_OUT}/Cooked/<Name>/windows`, for example PACK's `package.simulatedDelivery.root` (`Zenith2/Docs/Design/PACK/Algorithms.md` → "Simulated deliverable units"). MOD passes those values through unchanged.

Every gametest runs headless; a test that needs a device is a `needs-gpu` ticket's render test, not a gametest. The test passes when the game exits with code 0 (`EXIT_CODE_NORMAL`): startup succeeded, `ticks` ticks ran, and no automated assertion (exit code 5) or watchdog (124) fired.

<!-- elaboration -->
The migration-fixture run (from Stage 3; A5 cross-area row SB30) is not a `gametest.json` key: SCRIPT's registry block registers `Game_<Name>_ScriptMigrations` as the game's development Player run `--headless --exit-after-ticks 1 --script-migration-fixtures <Zenith2>/Games/<Name>/Scripts/Tests/Migration` ([Z2-N-SCRIPT-400]). After startup step 17 the runtime reads every `<Behaviour>/v<n>.zbst` below the granted directory (at most 1,024 files of at most 1 MiB, in byte order of the relative path), fills one `Zenith_ScriptSavedState` per file (`m_ulSourceID` the little-endian `uint64` at byte offset 8 of the ZBST header, `m_xCheckpoint` the file's bytes, every other member at its default) and calls SCRIPT's `Zenith_ScriptRestoreMigrationFixtures` on the initial world; when the loop stops it checks `Zenith_ScriptWorld::GetDisabledModuleCount()` of that world. Every failure — no fixture, an oversized or truncated file, a read error, a restore error, a disabled module — is an automated test assertion (`RecordTestAssertionFailure`), so the run exits with code 5 after the requested ticks with one `runtime.diagnostics` line `script migration fixtures: <cause>`; startup never fails because of a fixture, and no new exit code exists.

### Startup report

`Zenith_StartupReport` (§ "Zenith_Runtime.h") records, per startup step, its wall-clock duration in microseconds, the last completed step, and on failure the failing step, the module index and error for module steps, the number of steps unwound and the exit code. `Zenith_RuntimeMain` logs it (§ "Startup timeline") and returns its exit code; `Zenith_Runtime::GetStartupReport` keeps it for the profiler's startup panel.

### In-memory module host layout

<!-- elaboration -->
```cpp
// Zenith2/Engine/ZenithRuntime/Internal/Zenith_ModuleHost.h
enum Zenith_ModuleState : uint8
{
	MODULE_STATE_DECLARED = 1,        // in the composition
	MODULE_STATE_TYPES_REGISTERED,    // RegisterTypes returned success
	MODULE_STATE_STARTED,             // Start returned an instance and no registration was rejected
	MODULE_STATE_FAILED,              // RegisterTypes, Start or a registration failed; the startup unwinds
	MODULE_STATE_DESTROYED,           // the instance was destroyed and its journal undone
};

struct Zenith_HostedModule
{
	const Zenith_ModuleDescriptor* m_pxDescriptor = nullptr;   // into the composition
	Zenith_Owned<Zenith_ModuleInstance> m_xInstance;           // null until STARTED
	Zenith_ModuleRegistrationJournal m_xStartJournal;          // registrations made through the start context
	Zenith_ModuleState m_eState = MODULE_STATE_DECLARED;
	uint64 m_aulAttachedWorldMask[64] = {};                   // bit per world tag 0–4095
};

class Zenith_ModuleHost
{
	// One Zenith_HostedModule per composition entry, in canonical order, in a Zenith_FixedArray of capacity 32;
	// per world, a Zenith_ModuleRegistrationJournal per module for the attach registrations.
};
```

The world mask is 512 bytes per module (4,096 tags), 6.5 KiB for thirteen modules, allocated once from the root heap at step 15. The attach journals live in each world's host record and are freed with the world.

## Algorithms and state machines

### Composition validation at startup

`Zenith_ValidateComposition(const Zenith_GameComposition&)` runs at the start of step 6, before any registration (§ "Configuration-key registration"), and fails startup with exit code 3 and the message shown on the first defect:

| Check | Message (log category `runtime.composition`) |
|---|---|
| `m_szGameName`, `m_szGameVersion`, `m_szECSBackendName`, `m_xSettings.m_szStartupScene` non-null and non-empty | `composition: <field> is empty` |
| `m_pfnCreateWorld` non-null | `composition: no ECS backend factory` |
| `m_axRHIBackends` non-empty with non-null factories and unique names | `composition: RHI backend list is empty or malformed` |
| `m_axCodecs` holds exactly `zlib` and `zstd` with non-null factories | `composition: codec list must be exactly zlib and zstd` |
| every module descriptor has a non-empty unique name and non-null entry points | `composition: module <index> is malformed` |
| every module's `m_uInterfaceVersion` equals `uZENITH_MODULE_INTERFACE_VERSION` | `module '<name>' interface version <v> does not match runtime <r>` ([Z2-N-ARCH-845]) |
| at most 32 modules | `composition: <n> modules exceed the host capacity of 32` |
| every name in a descriptor's `m_szModuleDependencies` is a module of the composition | `composition: module '<name>' depends on unknown module '<dep>'` |
| `m_axModules` is in the canonical order of § "Canonical module order" recomputed from those dependency lists | `composition: modules are not in canonical order; expected <names comma-joined>` |

The generator produces a valid composition by construction; the check exists for stale build trees and hand-built test compositions, and the canonical-order row makes every game's startup re-verify the order its generated table carries ([Z2-N-ARCH-844]).

### Startup sequence with unwinding

The sequence is Zenith2/Docs/Architecture/CompositionRoot.md → "Startup sequence". The runtime implements it as a table of steps, each a (construct, destroy) pair, and an unwind stack:

1. For step s = 4 … 18: call the test hook `m_pfnBeforeStep(s)` (a `false` return injects a failure), start the step's timer, run `construct(s)`, stop the timer into `m_aulStepMicroseconds[s−1]`.
2. On success, push s onto the unwind stack, set `m_eLastCompletedStep = s`, call `m_pfnAfterStep(s)` (a `false` return fails s after construction: s is on the stack and is unwound too).
3. On failure of s: record `m_eFailedStep = s`, the cause text and the exit code from § "Step failure to exit code mapping"; log `Runtime: startup failed at step <s> (<step name>): <cause>`; pop and `destroy` every step on the stack in reverse (calling `m_pfnOnUnwind` for each), counting `m_uStepsUnwound`; return `Zenith_RuntimeError{ RUNTIME_ERROR_STARTUP_STEP, s, <module index or 0xFFFF> }`, except for the exit-code-2 rows of steps 6 and 9, which return `Zenith_RuntimeError{ RUNTIME_ERROR_CONFIGURATION, s, 0xFFFF }`. A failure injected by a test hook has no cause of its own: it takes the exit code of the step's first row in the mapping table and the cause `injected by test hook`.
4. After step 18, the runtime object owns every constructed step; its destructor runs the shutdown sequence, which destroys them in the order of § "Shutdown sequence".

Each `construct(s)` is itself a factory that unwinds its own partial work before returning an error (Zenith2/Docs/Contracts/Memory.md → "Fallible construction"); the stack unwinds whole steps. Every allocation a step makes uses `Zenith_TryNew` (never `Zenith_New`), so an allocation failure during startup is that step's failure, not the OOM handler's. Steps with inner loops (10, 15, 16) unwind their partially completed iterations as § "Module lifecycle state machine" specifies before reporting.

Complexity: O(steps + modules × worlds) with no allocation during unwinding (the stack is a fixed array of 19 entries on the runtime object).

### Step construction notes

<!-- assumption: pending JOBS -->
The steps that drive another area's subsystem follow that area's design for what they pass; the CORE, PLAT, JOBS and GRAPH drafts (`Zenith2/Docs/Design/CORE/Diagnostics.md`, `Zenith2/Docs/Design/PLAT.md`, `Zenith2/Docs/Design/JOBS.md`, `Zenith2/Docs/Design/GRAPH.md`) ask for the following, which the step table implements:

| Step | Action beyond Architecture's table |
|---|---|
| before 4 | fill `Zenith_PlatformDesc`'s early keys (the queue capacities; from Stage 4 also `platform.accessibility.backend` into `m_eAccessibilityBackend`, A3 cross-area row P20 (b)) from the parsed `--set` values through `ApplyValueText` on a scratch configuration store (PLAT P6 a) |
| after 4 | `Zenith_Platform::StartWatchdog(uSeconds, EXIT_CODE_WATCHDOG)` when `--timeout-seconds` is given (PLAT P5, `Zenith2/Docs/Design/PLAT/Platform.md` → "Process watchdog") |
| 5 | the four command-line host files (`--config-dump`, `--record`, `--replay`, `--input-script`, and `--evidence-dir` where the option table carries it) are opened only through the `Zenith_CommandLineFileGrant`s `Zenith_CommandLine` hands out (§ "Zenith_CommandLine.h"; PLAT P6 f) |
| 6 | after `Zenith_Config::Create`, `Zenith_InputSystem::LoadBindings` (PLAT P6 b) |
| 7 | `SetGlobalLevel` and `SetCategoryLevel` from `log.level` and `log.categoryLevels`, `AddSink` for every sink, `ResizeRing` from `log.ringBytes`, then the router's `StartDrainThread()` (CORE's landed contract; a `StartDrainThread` failure is `FATAL_CODE_LOGGING_UNAVAILABLE`) |
| 9 | call `Zenith_ApplyFloatEnvironment()` on the main thread first (Zenith2/Docs/Contracts.md → "Threading and concurrency contract", "Floating-point environment"); fill the job-system description from the `jobs.*` configuration keys and `runtime.headless`; validate the simulation-clock description, an invalid one being exit code 2; under `--replay`, drive the clock with the log's tick counts |
| 16 | mark exactly one input world: the startup world, moved to the new world at a world-transition swap, and to the play world while the editor runs one (PLAT P6 c) |
| 18 | acquire the compiled schedule from GRAPH's schedule cache and hand it to the frame executor |
| 19 | call `Zenith_InputDevices::CarryFrameTransitions()` after a `RunFrame` in which the input world ran no tick (PLAT P6 d); while `Zenith_AppLifecycle::ShouldThrottle()` holds, call `Zenith_Platform::WaitForEvents(100)` in place of a frame (PLAT P6 e); in the prologue's recompile step, acquire a recompiled schedule from the cache inside the zone `Frame.ScheduleRecompile`; after every `RunFrame`, release the cache's unused schedules |
| shutdown | the executor's current iteration completes and no further `RunFrame` is called; the executor is destroyed before the job system; the job system's background work is cancelled and drained at Contracts' shutdown step 4 |
<!-- /assumption -->

The steps that drive SCENE call exactly the functions of `Zenith2/Docs/Design/SCENE.md` → "Construction and shutdown", declared in `Zenith2/Docs/Design/SCENE/SceneAPI.md` and `Zenith2/Docs/Design/SCENE/Transforms.md`; the runtime state holds the process objects (`Zenith_Owned<Zenith_SceneServices>`, `Zenith_Owned<Zenith_SceneLoader>`) and each world's host record holds its `Zenith_Owned<Zenith_SceneWorldState>`. Step 16 also gives each world its simulation limits, with the tick duration ECS reports to systems ([Z2-N-ARCH-413]):

| Step | Calls, in order | Failure |
|---|---|---|
| 6 | `Zenith_RegisterSceneConfigKeys(xSchema, ePlatform)` (CORE's `(Zenith_ConfigSchema&, Zenith_BuildPlatform)` form, `ePlatform` = `Zenith_GetBuildPlatform()`) among the engine key registrations in library-layer order (ZenithScene's after ZenithECS's `Zenith_RegisterECSConfigKeys` and ZenithAssets' `Zenith_RegisterAssetsConfigKeys`, which ZenithScene depends on); `Zenith_RegisterSceneTypes(xTypes)` in the Scene slot of the engine types ([Z2-N-ARCH-847]: Core, Reflect, ECS, Scene, Assets, Save, Script, Runtime, Flux) | a key refusal is exit code 2 (`RUNTIME_ERROR_CONFIGURATION`, the key named in the cause); a type refusal is exit code 3 |
| 11 | after `Zenith_AssetRegistry::Create`: `Zenith_SceneServices::Create(Zenith_SceneServicesDesc{ m_pxTypes = the registry frozen at step 10, m_pxStrings = the process string table, m_pxAssets = the asset registry, m_pxCodecs = the codec set, m_pxHeap = the root heap, m_pxFaults = the fault injector or null })`; `Zenith_RegisterSceneAssetKinds(xAssets, *pxServices)`, before the `CloseRegistration` of step 15; `Zenith_SceneLoader::Create(*pxServices, xJobs, xAssets, xGovernor, xRootHeap, pxFaults)`; from Stage 3, `xGovernor.RegisterConsumer(pxLoader->GetMemoryConsumer())` (added to the runtime's step-11 file by SCENE's memory-consumer slice, together with the consumer it registers). Destroy, in a startup unwind, runs in reverse: `UnregisterConsumer` (from Stage 3), the loader, `UnregisterKindLoader(ASSET_KIND_PREFAB)` and `(ASSET_KIND_SCENE)`, the services | `SCENE_ERROR_OUT_OF_MEMORY` is exit code 4; a kind-loader registration the asset registry refuses (`SCENE_ERROR_INVALID_DESC` from `Zenith_RegisterSceneAssetKinds`) is exit code 3 |
| 15 | before `Zenith_ModuleHost::StartAll`: `Zenith_RegisterSceneCommandKinds(xCommandKinds, *pxServices)`, so SCENE's command kinds take their IDs before any module's; from Stage 3, `Zenith_RegisterSceneScriptBindings(xBindings, *pxServices)` and the runtime's asynchronous-request consumer (§ "SCENE and SCRIPT seams of the runtime"), both before `Zenith_ScriptBindingTable::Seal` | a SCENE registration error is exit code 3 |
| 16 | per world, after `Zenith_WorldSimulation::Create`: `SetLimits(xLimits)`, where `xLimits = Zenith_ReadECSSimulationLimits(xSchema, xStore)` and `xLimits.m_ulTickDurationUs = xClock.GetTickDurationUs()` of the step-9 `Zenith_SimulationClock`, so `Zenith_SystemContext::GetTickDurationUs` reports the configured tick (`Zenith2/Docs/Design/ECS/SystemAPI.md`); then, before any module attaches, `Zenith_SceneWorldState::Create(Zenith_SceneWorldDesc{ m_pxServices, m_pxWorld = the world simulation, m_pxLoader = the scene loader, m_pxPublication = null at Stage 2 and the world's `Zenith_RuntimeScenePublicationSink` from Stage 3, m_pxTransitions = the world host's `Zenith_SceneTransitionSink` (GAME's, `Zenith2/Docs/Design/GAME/WorldHost.md` → "Transition request intake"; null until it lands), m_uMaxAdditiveInstances = scene.maxAdditiveInstances, m_pxFaults })` and `Attach()`. `Zenith_WorldHost_DestroyWorld` destroys the world's scene state before its world simulation | world creation failed: exit code 4 |
| 17 | `pxLoader->LoadStartupScene(<content.startupScene>, xWorldSimulation, xSceneWorldState)` on the initial world | any `Zenith_SceneError` is exit code 3 with the cause `startup scene '<path>': <Zenith_ToString of the code>` |
| prologue step 7 (from Stage 3) | `pxLoader->StepActivations(<scene.activationBudgetUs>)` once per prologue, inside the zone `Runtime.Prologue.SceneActivations` (§ "Frame loop driver") | none: activation failures are SCENE's per-instance states |
| shutdown | each world's scene state with its world, before its script world (§ "Shutdown sequence" step 2); then, after the module instances (step 3): `UnregisterConsumer` of the loader's consumer (from Stage 3), the loader, `UnregisterKindLoader(ASSET_KIND_SCENE)` and `(ASSET_KIND_PREFAB)` (the registry unloads every resident scene and prefab through the loaders), then `Zenith_SceneServices`, which owns both loaders | none |

The steps that drive SCRIPT's per-world objects call exactly `Zenith2/Docs/Design/SCRIPT.md` → "Construction and shutdown" rows 16 and 17 and its shutdown row (from Stage 3; A5 cross-area row MB24); each world's host record holds its `Zenith_ScriptWorld*`, which the script runtime owns:

| Step | Calls, in order | Failure |
|---|---|---|
| 6 | from Stage 3, in the Script slot of the engine types: `Zenith_ScriptRegisterEngineTypes(xTypes)` (`Zenith_BehaviourComponent`); a Stage 2 build leaves the slot empty, because SCRIPT's Stage 2 build has only its refusing stub | exit code 3, cause `engine types: ZenithScript: <code>` |
| 10 | from Stage 3, after `RegisterSchemaBlob` and before `Freeze`: read `content:/Scripts/scripts.zlmf` (the bytes are kept for the runtime's life), `Zenith_ScriptModuleManifest::Load`, then `Zenith_ScriptRegisterBehaviourStateTypes(xTypes, *pxManifest)` | exit code 3, causes `script manifest: missing`, `script manifest: <code>`, `script state types: <code>` |
| 14 | the process `Zenith_ScriptBindingTable` (every stage); from Stage 3 then `Zenith_ScriptWatchdog::Create` (`m_uLimitMs` from `script.watchdogMs`, `m_uMaxWorlds` 16, `m_uFrameThreadCount` from the job system) and `Zenith_ScriptRuntime::Create` over the table, the manifest, the watchdog and the frozen registry (debugger at its defaults). A Stage 2 build creates neither: `Zenith_ScriptRuntime::Create` is a refusing stub there, so the runtime pointer stays null and `Zenith_ModuleStartContext::GetScripts` asserts (A5 cross-area row SB16) | exit code 4, causes `script bindings: out of memory`, `script watchdog: <code>`, `script runtime: <code>` |
| 15 | after `Zenith_CommandKindTable::Seal`: `Zenith_ScriptBindingTable::Seal` | exit code 3, cause `script bindings: <code>` |
| 16 | per world, after `SetLimits` and before `Zenith_SceneWorldState::Create`: `Zenith_ScriptReadWorldLimits(xSchema, xStore, xDesc)` (SCRIPT's public reader of its allocated rows, `Zenith2/Docs/Design/SCRIPT/PublicAPI.md` → "Zenith_ScriptWorld header"), then the runtime-held members of `xDesc` (ECS world and simulation, frozen type registry, tag, job system, watchdog, sealed binding table, module manifest, the world seed of `Zenith_WorldDesc`, `GetFrameThreadCount()`, the `scriptRecovery` settings, listener, root heap, fault injector), then `Zenith_ScriptRuntime::CreateWorld(xDesc)` | a limits or creation error is exit code 4 (the step-16 world-creation row), causes `script world limits: <code>` and `script world: <code>` |
| 17 | on the initial world, before `LoadStartupScene`: `LoadModules()`, then `RegisterScriptNodes(xSystems)` with the world's `Zenith_SystemRegistry` | either error is exit code 3, causes `script modules: <code>` and `script nodes: <code>`; a module disabled for the session is no failure |
| shutdown | in `Zenith_WorldHost_DestroyWorld`, after the scene state and before the world simulation: `Zenith_ScriptRuntime::DestroyWorld(*pxScriptWorld)`; at shutdown step 4 the runtime, then the watchdog, then the binding table | none |

The listener every world's desc borrows is the runtime's `Zenith_RuntimeScriptListener` (`Internal/Zenith_ScriptHostImpl.cpp`, from Stage 3): `OnWorldHalted` records the halt in the world's host record (an atomic, written on the frame thread that observed the fatal event) and logs it under `runtime.diagnostics`; `OnModuleDisabled` and `OnModuleRecovered` log under `runtime.diagnostics`. After each `RunFrame` the frame loop reads the records and a recorded halt is an unrecoverable runtime error, so the Player returns exit code 1 after the current frame (`Zenith2/Docs/Design/SCRIPT/Runtime.md` → "World halt procedure"). The halt's `REPLAY_TICK_FLAG_NON_REPLAYABLE` joins with the SAVE replay recorder (A5 cross-area rows SB12, MB15).

From Stage 4, step 11 also gives the asset registry the process memory governor and registers its consumer (A3 cross-area row AB11; Z2T-MOD-067): `Zenith_AssetRegistryDesc::m_pxGovernor` is the step-8 governor, `RegisterConsumer(&xAssets.GetMemoryConsumer())` (`MEMORY_SUBSYSTEM_ASSETS`) follows `Zenith_AssetRegistry::Create` at once, before SCENE's loader consumer and every module's, and `UnregisterConsumer` runs immediately before the registry is destroyed, in a startup unwind and at shutdown step 6. Before Stage 4 `m_pxGovernor` is null, which ASSET's `Create` accepts, because `GetMemoryConsumer` is defined by ASSET's Stage 4 residency change.

Step 9 passes the Player's profiler as `Zenith_JobSystemDesc::m_pxProfiler` (the runtime state's `m_pxProfiler`, set from the `Create` parameter; the thread lanes of `Zenith2/Docs/Design/JOBS/PublicAPI.md`). Step 4's destroy entry and steps 11, 15 and 16 gain PACK's bridge calls at Stage 6, and step 4 passes the test hooks' base-path override to `Zenith_PlatformDesc`; both are Zenith2/Docs/Design/MOD/RuntimeSeams.md → "Call sites PACK's tickets add" and → "Runtime test seams for modules". In tool mode (`Zenith_RuntimeToolAccess::Create`) steps 7, 12, 14, 16, 17 and 18 differ as → "Runtime tool access" states. `Zenith_RuntimeMain` runs no frame when a module requested an exit after start (→ "Start context members for ONLINE and PACK").

Under `--replay` the runtime makes no resume decision (A3 cross-area row SV24; `Zenith2/Docs/Design/SAVE/Algorithms.md` → "Replay mode"): step 17 calls none of PLAT's `GetLaunchResumeCandidate`, SAVE's resume-header reader, `DecideResume` or `ReportResumeOutcome`, and loads the startup scene, so a replay neither reads nor deletes the user's `saves/resume.zsave`; and step 11 does not read the last save profile (`Zenith_ReadLastSaveProfile`; the initial profile is `local`). PLAT's relaunch decision is simply not invoked.

### Configuration-key registration

No module has a third entry point (Z2-D-024; [Z2-N-ARCH-168]): a module registers its configuration keys from `Zenith_Module_<M>_RegisterTypes`, because reflection types and configuration keys are both static declarations, and both are made before the configuration store is built at step 6. At the start of step 6, before `Zenith_Config::Create`, the runtime:

1. validates the composition (§ "Composition validation at startup");
2. creates the configuration schema and the type registry, the registry in its declaration phase;
3. calls every engine library's `Zenith_Register<Library>ConfigKeys(Zenith_ConfigSchema&, Zenith_BuildPlatform)` in layer order (ZenithPlatform's is `Zenith_RegisterPlatformConfigKeys`), then registers the runtime's own keys (`content.startupScene` with the composition's default, `runtime.headless`), then registers the engine's reflection types;
4. calls each module's `m_pfnRegisterTypes` in canonical order; a module registers its types into the registry and its keys into the schema the registry exposes during this phase;
5. freezes the schema, and builds the store with `Zenith_Config::Create`.

Step 10 then binds the registry to the process `Zenith_StringTable`, registers the game's `schemas.zreg` and freezes the registry, so [Z2-N-ARCH-847]'s order (engine types, modules in canonical order, data-defined schemas, then `Freeze`) is unchanged; only the declaration part of it runs earlier. An engine or runtime key-registration failure fails step 6 with `RUNTIME_ERROR_CONFIGURATION` (exit code 2); a module's `RegisterTypes` failure fails step 6 with `MODULE_ERROR_REGISTRATION` (exit code 3). The generated composition row format and every golden are unchanged. PLAT's note P3, which asked for a per-module registration pointer, conforms to this ruling (X61; `Zenith2/Docs/Design/PLAT/Configuration.md`).

Budget-run graphics settings. A budget run fixes every graphics setting from its workload definition, never from the user's settings file or command line ([Z2-N-BUDGET-122]), and it does so through the existing command-line layer, not a workload layer (`Zenith2/Docs/Design/PLAT/Configuration.md` → "Command-line layer"): the workload runner `Zenith2/Benchmarks/Run-Workload.ps1` passes the workload definition's value of every registered `render.quality.*` key as one `--set`, and the command-line layer sits above the user layer, so no user value of such a key survives resolution. The runtime guarantees that shape on its side. In a budget run (the command line carries `--budget-run <workload>`, [Z2-N-BUDGET-105]; `Zenith_CommandLine::IsBudgetRun()`), between freezing the schema (item 5) and `Zenith_Config::Create`, step 6 checks that every key the frozen schema registers under the prefix `render.quality.` is named by exactly one `--set`, and that no `--set` names a `render.quality.` key the schema does not register. A key named by no `--set` (the runner omitted it, so the user layer would decide it), a key named by two or more (any `--set` of that key beyond the workload's own), or an unregistered `render.quality.` key fails step 6 with `RUNTIME_ERROR_CONFIGURATION` (exit code 2) and one `runtime.startup` line, `budget run: render.quality key '<key>' set <n> times; the workload sets each exactly once` or `budget run: render.quality key '<key>' is not registered`. Outside a budget run the check does not run and repeated `--set` keeps its ordinary last-wins rule. `Test_Runtime_BudgetRun_QualityKeysExactlyOnce` covers the accepted shape and the three refusals, and `Test_Runtime_NotBudgetRun_SkipsQualityCheck` the ordinary run (§ "Runtime and composition tests"); LIGHT's `Test_LightConfig_CommandLineLayerDeterminesQuality` covers the store side.

<!-- elaboration -->
The two halves of A3 cross-area row L41 map onto this check exactly. "The runner passes every `render.quality.*` key via `--set`" is DEVT's runner (`Zenith2/Benchmarks/Run-Workload.ps1` learns every registered key from a `--config-dump` pre-launch and passes each once, `Zenith2/Docs/Design/DEVT.md`, row L41), and a runner that omits one fails the run here ("set 0 times"). "The budget-run mode admits no other `--set` of such a key" is the "two or more" refusal: any `--set` of a `render.quality.*` key beyond the runner's own (a user's, an appended CI option) makes that key's count 2 and fails step 6, so no value of such a key except the workload's can reach the store, from the user layer or from the command line.

Zenith2/Docs/Architecture/CompositionRoot.md → "Command-line options" lists `--budget-run`, its four companion switches and `--evidence-dir` ([Z2-N-ARCH-860] admits exactly the options of that table), so `Zenith_CommandLine` accepts them under the rules of § "Zenith_CommandLine.h" and [Z2-N-ARCH-878], and `IsBudgetRun()` is true exactly when `--budget-run` was parsed.

ZenithReflect's `Zenith_TypeRegistry` accepts registrations before the process `Zenith_StringTable` exists, recording declarations until step 10's `BindStringTable`, and exposes the configuration schema being built through `GetConfigSchema()`, valid until the schema is frozen (Zenith2/Docs/Architecture.md → "Registration API"; `Zenith2/Docs/Design/ECS/Reflect.md` → "Zenith_TypeRegistry").

Zenith2/Docs/Architecture/CompositionRoot.md → "Startup sequence" creates the type registry and registers the engine and module types at step 6, as above, and step 10 creates `Zenith_WorldRegistry`, binds the string table, registers the game's `schemas.zreg` and freezes the registry.

### Step failure to exit code mapping

| Step | Failure | Exit code |
|---|---|---|
| 1b (Player) | the log router could not be allocated | 4 |
| 3 (Player) | command-line error | 2 |
| 4 platform | SDL initialisation or window creation failed | 4 |
| 5 file system | a mount root is missing or unreadable (package, patch, `--assets-root`) | 4 |
| 5 file system | a package or patch manifest fails verification or its cooked-content version mismatches | 3 |
| 6 configuration | a required key has no value after resolution or a schema rule is broken (`CONFIG_SOURCE_ERROR_REQUIRED_KEY_MISSING`, `CONFIG_SOURCE_ERROR_SCHEMA_RULE`), or an engine key registration failed | 2 |
| 6 configuration | composition validation failed, an interface version mismatched, a module's `RegisterTypes` failed, or an engine library's reflection-type registration was refused (`Zenith_RegisterSceneTypes` among them) | 3 |
| 7 logging | a log sink or the crash reporter could not be created | 4 |
| 7 logging | the log drain thread could not start | fatal: `ZENITH_FATAL(FATAL_CODE_LOGGING_UNAVAILABLE)` (CORE's failure procedure ends the process) |
| 8 memory governor | allocation failed | 4 |
| 9 job system | a worker or the render-submit thread could not be started | 4 |
| 9 job system | the job-system or simulation-clock description built from configuration is invalid | 2 |
| 10 registration | `schemas.zreg` is invalid; from Stage 3, `scripts.zlmf` is missing or invalid, or a behaviour-state type was refused | 3 |
| 11 assets | a codec factory, the asset registry, the scene services or the scene loader failed to allocate | 4 |
| 11 assets | the cooked-content index version mismatches the engine, or the asset registry refused SCENE's scene or prefab kind loader (`Zenith_RegisterSceneAssetKinds`) | 3 |
| 11 assets | from Stage 6, `Zenith_RuntimePackage_Create` returned `PACKAGE_ERROR_REPLAY_CONTENT_MISSING` (value 37; under `--replay`, a unit of the recorded pack set or a DLC revision is not installed, [Z2-N-ARCH-784]) | 5 |
| 11 assets | from Stage 6, `Zenith_RuntimePackage_Create` returned any other `Zenith_PackageError`, in every build type (among them a Shipping build's absent or invalid `package.zpkm`) | 3 |
| 12 RHI device | device or swapchain creation failed | 4 |
| 13 renderer | the Flux renderer failed to create | 4 |
| 14 script runtime | the script binding table could not be allocated; from Stage 3, the script watchdog or the script runtime failed to create | 4 |
| 15 module start | `MODULE_ERROR_REGISTRATION`, or an engine registration was refused (`Zenith_RegisterSceneCommandKinds`; from Stage 3 `Zenith_RegisterSceneScriptBindings`), or `Zenith_ScriptBindingTable::Seal` returned a `Zenith_ScriptBindingError` | 3 |
| 15 module start | `MODULE_ERROR_BACKEND_CREATE`, `MODULE_ERROR_DEVICE`, `MODULE_ERROR_OUT_OF_MEMORY` | 4 |
| 16 initial world | world creation failed | 4 |
| 16 initial world | a module's `AttachWorld` returned `MODULE_ERROR_REGISTRATION` | 3 |
| 16 initial world | a module's `AttachWorld` returned another error | 4 |
| 17 startup scene | the scene is missing, corrupt, of a mismatched version, or references an unregistered type; from Stage 3, the world's script module manifest is unusable (`LoadModules`) or its script systems could not be registered (`RegisterScriptNodes`) | 3 |
| 18 schedule | schedule compilation failed | 3 |
| after startup | an unrecoverable runtime error | 1 |
| after startup | an automated test or replay assertion failed | 5 |
| any time | the `--timeout-seconds` watchdog fired | 124 |

Headless runs skip steps 12 and 13 ([Z2-N-ARCH-849]); their rows never apply there. An unknown backend name has no row: `rhi.backend` is RHI's per-platform enum key (lower-case names, `vulkan` alone on Android), so per [Z2-N-ARCH-929] a value outside it (`metal`, the wrong-case `D3D12`, an Android `d3d12`) is one configuration warning and the next-lower layer stays in effect, down to the platform default `vulkan`; step 12 only maps the resolved value onto the composition's `m_axRHIBackends` entry, and a missing entry is an engine defect (`ZENITH_ASSERT`), never an exit code (orchestrator ruling Q67 (a)). A configuration file that fails to parse is not a failure: per [Z2-N-ARCH-929] it contributes nothing, every key falls back to the lower layers with a warning, and a corrupt user file is quarantined (Zenith2/Docs/Design/PLAT/Configuration.md → "Corrupt configuration handling"); `Zenith_Config::Create` fails only on a schema rule, a missing required key or allocation.

### Module lifecycle state machine

Per module m (in canonical order index i):

| State | Event | Next state | Action |
|---|---|---|---|
| `DECLARED` | step 6: `RegisterTypes` succeeds | `TYPES_REGISTERED` | — |
| `DECLARED` | step 6: `RegisterTypes` fails | `FAILED` | step 6 fails; the schema and the type registry (holding every key and type registered so far) are destroyed whole by step 6's own unwind, so no per-module undo exists |
| `TYPES_REGISTERED` | step 15: `Start` returns an instance and the start context recorded no rejection | `STARTED` | store the instance and the start journal |
| `TYPES_REGISTERED` | step 15: `Start` fails | `FAILED` | undo m's start journal in reverse; step 15 then destroys modules i−1 … 0 in reverse (each: undo journal, destroy instance → `DESTROYED`) and fails |
| `TYPES_REGISTERED` | step 15: `Start` returns an instance but a registration was rejected | `FAILED` | destroy the instance, undo m's start journal in reverse, then as the row above |
| `STARTED` | a world is created (step 16 or a world transition) | `STARTED` | `AttachWorld` for modules 0 … n−1 in canonical order; on m's failure: undo m's attach journal for that world, `DetachWorld(tag)` on modules i−1 … 0 in reverse, destroy the world, report the failure |
| `STARTED` | a world is destroyed | `STARTED` | `DetachWorld(tag)` on modules n−1 … 0 in reverse, each followed by undoing its attach journal for that world |
| `STARTED` | shutdown step 6 | `DESTROYED` | after every world is destroyed: destroy instances n−1 … 0 in reverse, each followed by undoing its start journal |
| `FAILED`, `DESTROYED` | any | unchanged | nothing further is called on the module |

A module is never started without its types registered, never attached before it is started, and never called after `DESTROYED`. `DetachWorld` is called only for worlds whose `AttachWorld` succeeded (the world mask), so a module never receives a detach for a world it never joined.

### Registration rejection rules

The start and attach contexts reject a registration, record the rejection, and log `module '<name>': <what> rejected: <reason>`:

| Registration | Rejected when |
|---|---|
| `RegisterCommandKind` | `m_szName` does not start with `<MODULE_NAME>.`, is not unique in the process, `m_xPayloadType` is not a registered `TYPE_KIND_STRUCT`, `m_uMaxPayloadBytes` is 0 or smaller than the payload type, `m_uAllowedPhaseMask` is 0 or has a bit outside 1–10, or `m_pfnPlayback` is null |
| `RegisterScriptBindings` | the binding table fails its own validation (Zenith2/Docs/Scripting.md → "Modules, bindings and generated types") or names a binding another registrant owns |
| `RegisterMemoryConsumer` | the consumer is already registered |
| `AddWorldResource` | the resource type is not registered, is already a resource of this world, or `eOwningPhase` is not a phase |
| `AddLifecycleListener` | the listener is already registered for this world |
| `AddExtractor`, `AddEpilogueStage` | `m_szName` does not start with `<MODULE_NAME>.`, is not unique in the world, or the function pointer is null |
| system registrations through `GetSystems()` | the system registry rejects them (Zenith2/Docs/Architecture/ECSModel.md → "Scheduling contract") |

The first rejection makes the enclosing `Start` or `AttachWorld` fail with `MODULE_ERROR_REGISTRATION` whatever it returns; the remaining registrations of the same call are still validated and logged, so one run reports every defect of a module.

### Registration journal and undo

Every registration a module makes through a context is appended to that module's journal (for the start context) or to its per-world journal (for the attach context) as a record `{ kind, token }`, where `token` is the value the registered registry returned for its removal. Undo walks the journal from the last record to the first and calls the owning registry's removal for each ([Z2-N-ARCH-851]); a module never unregisters another module's registration, because it never sees another journal. The journal is a bounded array of 256 records per journal; a 257th registration is itself rejected with reason `journal full (256 registrations)`.

### Shutdown sequence

`~Zenith_Runtime` follows Zenith2/Docs/Contracts.md → "Threading and concurrency contract" ("Shutdown ownership order") steps 2–10; this design fixes step 6 and the order of every other object the runtime constructed:

1. Drain the in-flight pipelined frame (Contracts step 2, [Z2-N-ARCH-426]): the shutdown request takes effect when the current iteration `RunFrame(N+1)` returns (simulation N+1 finished, RENDER(N) submitted), no further `RunFrame` is called, and the render world N+1 that EPILOGUE(N+1) extracted is discarded, never rendered (§ "Frame loop driver"); shut down each world by its lifecycle (step 3), cancel every outstanding operation and run jobs until each reports completion (step 4), wait for GPU idle on every queue and retire every deferred deletion (step 5).
2. Destroy the worlds in descending world-tag order, each first detaching the modules in reverse canonical order (§ "Module lifecycle state machine"); each world's `Zenith_SceneWorldState` is destroyed with its world, then (from Stage 3) its `Zenith_ScriptWorld` through `Zenith_ScriptRuntime::DestroyWorld`, both before its `Zenith_WorldSimulation`.
3. Destroy the module instances in reverse canonical order, each followed by undoing its start journal; module-owned threads (the audio device thread among them) are joined by their instances here (Contracts step 6). Then the SCENE process objects of step 11, after every world: unregister the scene loader's memory consumer (from Stage 3, when SCENE's memory-consumer slice registers it), destroy the loader, `UnregisterKindLoader(ASSET_KIND_SCENE)` and `UnregisterKindLoader(ASSET_KIND_PREFAB)`, then destroy `Zenith_SceneServices`, which owns both kind loaders (`Zenith2/Docs/Design/SCENE.md` → "Construction and shutdown"; its "shutdown step 6" is Contracts step 6).
4. Destroy the frame executor, then the frame schedule cache (both built by step 18; the cache outlives the executor because the executor may hold pointers to cached schedules until it is destroyed, `Zenith2/Docs/Design/GRAPH.md` → "Shutdown" and Contracts step 6), both before the job system (step 5 below); then the script runtime (14, from Stage 3), the script watchdog (14, from Stage 3) and the script binding table (14), then the Flux renderer (13).
5. Destroy the job system (9), which joins the render-submit thread and then the workers (Contracts step 7).
6. Flush the user layers while `Zenith_AsyncIO` is still alive: `Zenith_Config::FlushUserLayer()` (`Zenith2/Docs/Design/PLAT/Configuration.md` → "Persisting the user layer"), then `Zenith_InputSystem::FlushUserBindings()` (`Zenith2/Docs/Design/PLAT/Input.md` → "Persisting user bindings"), each on the main thread, each submitting its pending `WriteAtomic` if any and waiting for it (`Zenith2/Docs/Design/PLAT.md` → "Threads PLAT owns and shutdown"); a flush error is logged under `runtime.shutdown` and shutdown continues. Then destroy the owners of the platform's I/O threads (`Zenith_AsyncIO`, Contracts step 8), then the RHI device and swapchain (12, Contracts step 9, which reports live device handles), then the asset registry and the two codecs (11, Contracts step 10, which reports referenced slots).
7. Destroy the type and world registries (10) and the memory governor (8), log `Runtime: shutdown complete in <ms> ms` under `runtime.shutdown`, then destroy the log sinks and crash reporter (7), the configuration (6), the virtual filesystem (5) and the platform (4).
8. Return to `Zenith_RuntimeMain`, which returns the exit code to the Player; Contracts steps 11 and 12 follow there.

Step 1 calls GAME's `Zenith_WorldHost_ShutdownWorlds`, and right after step 2 the runtime calls `Zenith_WorldHost_ReleaseRetiredResults(xState, true)`; both calls are added by the GAME tickets that define them, and step 2's per-world destruction is `Zenith_WorldHost_DestroyWorld` walking GAME's teardown table (Zenith2/Docs/Design/MOD/RuntimeSeams.md → "World creation and destruction"). From Stage 6 PACK's tickets add `Zenith_RuntimePackage_BeginShutdown` in step 1 (Contracts step 4), `Zenith_RuntimePackage_Destroy` in step 3 after the module instances and `Zenith_RuntimePackage_ReleaseInstanceGuard` in step 7 between the virtual filesystem and the platform (→ "Call sites PACK's tickets add").

<!-- elaboration -->
Contracts step 8 names the platform services it destroys as the owners of the platform's I/O threads (`Zenith_AsyncIO`, step 6 above), and Contracts step 10b destroys `Zenith_Config`, `Zenith_VirtualFileSystem` and then the SDL platform object itself (step 7 above), after the RHI device and the asset registry, because the swapchain surface belongs to the platform window and must be destroyed before it. The watchdog timer belongs to `Zenith_Platform`, so the `Zenith.Watchdog` thread is joined, unfired, by the platform's destructor in step 7, not in step 6. Audio is a module and is destroyed with the modules (step 3).

### Frame loop driver

`Zenith_Runtime::RunFrameLoop` is Zenith2/Docs/Architecture/CompositionRoot.md → "The frame loop"; the prologue steps are Zenith2/Docs/Architecture/CPUFrameGraph.md → "Frame structure" ("The frame prologue"). This design adds the four places the loop ends:

| Condition | Checked at | Exit code returned |
|---|---|---|
| `RequestShutdown` was called (by a platform quit event, a script's quit request, `--exit-after-ticks` reached) | after the frame in which it was requested | 0, or 5 when an automated test assertion was recorded during the run |
| an unrecoverable runtime error was recorded | after the current frame | 1 |
| the device-loss handler or the frame-failure exit requested shutdown (Zenith2/Docs/Rendering.md → "Recovery") | after the frame whose prologue step 4 (`Flux_Renderer::RunPrologue()`) made the request | 6 (`EXIT_CODE_DEVICE_LOST`), or the `FATAL_CODE_DEVICE_LOST` fallback when the exit has not finished within 10 s (§ "Device loss and headless") |
| the `--timeout-seconds` watchdog | the watchdog `Zenith_Platform::StartWatchdog(uSeconds, EXIT_CODE_WATCHDOG)` started after step 4, which logs `Runtime: watchdog fired after <n> s` and ends the process with the OS immediate exit; the platform's destructor joins it unfired | 124 (never returns to the loop) |

`--exit-after-ticks <n>` counts ticks of the primary world, summed across world swaps (the option table's wording); the request is made at the tick boundary after the n-th tick, so exactly n ticks run.

The driver follows `Zenith2/Docs/Design/GRAPH.md` → "Shutdown" and `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Hand-off to Flux_RenderWorld", and does exactly this:

- **Prologue step 4, renderer housekeeping.** After `Zenith_MemoryGovernor::Update`, the driver calls `Flux_Renderer::RunPrologue()` on the main thread (swapchain recreations, the frames-in-flight change and the device-loss handler, Zenith2/Docs/Architecture/CPUFrameGraph.md → "The frame prologue").
- **The render condition.** The driver calls `RunFrame` with `bRunRender = !headless && frameIndex > 0 && renderer.GetDeviceLossState() == FLUX_DEVICE_LOSS_STATE_NORMAL`, reading the polled device-loss state after step 4's `RunPrologue`, so the render subgraph is empty from `LOST` through `RESUMING` and while `EXITING` (Zenith2/Docs/Architecture/CompositionRoot.md → "The frame loop", [Z2-N-ARCH-880]).

- **Prologue step 8, recompile.** The driver opens the zone `Frame.ScheduleRecompile` around each rebuild of the frame graph and its `Zenith_FrameScheduleCache::Acquire` call, and passes the result to `Zenith_FrameExecutor::SetSchedule`. It rebuilds and acquires in two cases:
  1. a structural trigger of `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Cache invalidation triggers" (a world created, destroyed or swapped; a script system hot-reloaded with a changed access set, phase or system set; component schemas re-registered; a render view set whose shape changed): `Acquire` with the default `Zenith_FrameCompileOptions`;
  2. a `jobs.recompile` request: when `Zenith_FrameExecutor::ConsumeRecompileRequest()` returns true (the development console's `jobs.recompile` records the request through `Zenith_FrameExecutor::RequestRecompile()`), the driver rebuilds and calls `Acquire` with `Zenith_FrameCompileOptions::m_bForceMiss` set, so the command costs exactly one extra cache miss and no hit. The request is consumed once per frame, whether or not a structural change also rebuilt in that frame; a frame with both acquires once, with `m_bForceMiss` set.
- **Prologue step 8, reprioritisation.** When `Zenith_FrameExecutor::IsReprioritiseDue()` holds and no rebuild ran in this frame, the driver calls `Zenith_FrameScheduleCache::Reprioritise(xExecutor.GetMeasuredCosts())` and passes the result to `SetSchedule`; this is not a rebuild and opens no `Frame.ScheduleRecompile` zone.
- **Prologue step 7, scene activations (from Stage 3).** The driver calls `Zenith_SceneLoader::StepActivations(<scene.activationBudgetUs>)` once per prologue inside the zone `Runtime.Prologue.SceneActivations`; it first starts the pending additive preparations of every attached world, then advances the isolated-world activations within the budget (`Zenith2/Docs/Design/SCENE/Loading.md` → "Scene transitions", "Additive load sequence"). GAME's world-host procedure (`Zenith2/Docs/Design/GAME/WorldHost.md` → "Prologue step 7 procedure") makes this same call its step 4; the ticket that lands that procedure moves the call inside `Zenith_WorldHost_StepTransitions` and removes the driver's direct call in the same diff, so the loader is stepped exactly once per prologue.
- **Before `RunFrame(k)`.** The driver tells Flux which frame index (k − 1) the render subgraph renders, and hands `Zenith_FrameExecutor::IsRenderWorldResyncPending()` to Rendering's render-world writer, so the extract nodes of EPILOGUE(k) perform a full-state extraction after a cancelled epilogue.
- **After every `RunFrame`.** The driver calls `Zenith_FrameScheduleCache::ReleaseUnused` with the schedules `Zenith_FrameExecutor::GetSchedulesInUse` reports (current, pending and the render schedule of the next iteration).
- **`Render.Seal` ordering.** The engine stage `Render.Seal` (EPILOGUE stage 13, group 0) declares no `Before` or `After` of its own. The ordering belongs to the extractors: `Zenith_WorldSimulation::EmitInto` declares `Before("Render.Seal@W0", true)` on every extractor of every world ([Z2-N-ARCH-439]; `Zenith2/Docs/Design/ECS/Scheduling.md` → "Epilogue and extractors").
- **Prologue flag and world requests (from Stage 2).** The driver sets the runtime state's `m_bFramePrologueActive` for frame-loop step 1 and clears it before `RunFrame`, so every prologue allocation of a frame precedes the frame's first tick and the `REPLAY_RECORD_TICK` next completion ID ([Z2-N-ARCH-794]); after each `RunFrame` it treats a non-zero `m_uShutdownRequestedByWorld` as `RequestShutdown` and reads the any-thread test-assertion count (Zenith2/Docs/Design/MOD/RuntimeSeams.md → "Test assertion failures and world shutdown requests"). GAME's prologue step-7 calls are inserted by GAME's tickets (→ "Call sites GAME's tickets add").
- **Shutdown.** A shutdown request takes effect when the current iteration returns: `RunFrame(N+1)` completes, the composition root calls no further `RunFrame`, and the in-flight render world N+1 is discarded and never rendered ([Z2-N-ARCH-426]); the request does not call `RequestCancel`. The executor is then destroyed before the job system, and the schedule cache after the executor (§ "Shutdown sequence" step 4).

### SCENE and SCRIPT seams of the runtime

From Stage 3 ZenithRuntime implements the two runtime-side seams that additive scene loads cross ([Z2-N-ARCH-078]: ZenithScene cannot name SCRIPT's per-world publication state or the runtime). Both live in `Internal/Zenith_SceneSeams.h` and `.cpp`:

| Type | Implements | Instances | Behaviour |
|---|---|---|---|
| `Zenith_RuntimeScenePublicationSink` | SCENE's `Zenith_ScenePublicationSink` over the world's `Zenith_AsyncResultTable`, whose pointer it binds once at the world's creation (`Zenith_ScriptWorld::GetAsyncResults()` read then and never re-resolved) | one per world, held as `Zenith_Owned<Zenith_ScenePublicationSink> m_pxScenePublication` in the world's host record, created before the world's `Zenith_SceneWorldState`; the world host moves it into the world's `Zenith_RetiredResults` entry (swap step 6 for a swapped-out world, teardown step `ASYNC_RESULTS` for any other) and destroys it with the table at its release, never earlier (`Zenith2/Docs/Design/GAME/WorldHost.md` → "Retired world asynchronous results") | `SetPublishHandler(pfn, user)`: `DeclareSource(ASYNC_SOURCE_SCENE_LOAD, scene.maxAdditiveInstances, uSCENE_LOAD_RESULT_BYTES)`, store the pair, `SetHandler(ASYNC_SOURCE_SCENE_LOAD, &m_xForwarder)`; `SetPublicationResolver(pfn, user)`: `SetPublicationResolver(ASYNC_SOURCE_SCENE_LOAD, pfn, user)` on the bound table, which keeps it when retired (A3 cross-area row ST21 (b), [Z2-N-SCRIPT-341]); `AllocateCompletionID()`: `AllocateCompletionID(ASYNC_SOURCE_SCENE_LOAD)`, `ASYNC_TABLE_ERROR_FULL` returned as 0; `Park(id, kind, payload)`: `Complete(id, kind, payload)`; `ASYNC_TABLE_ERROR_UNKNOWN_COMPLETION` under a publication source is a late local completion, a no-op counted in `runtime.async.late_local_completions` (CUMULATIVE; row ST21 (c)), and any other error `ZENITH_ASSERT` (an ID the table did not allocate for kind 8 is an engine defect, the interface's contract) |
| `Zenith_RuntimeScenePublishForwarder` | SCRIPT's `Zenith_AsyncPublicationHandler` | the sink's member | `OnPublished(xPublication, xWorld)` calls the stored handler with `m_ulCompletionID`, `m_uPublicationKind`, `m_xPayload`, `xWorld` and the stored user data, on the world's tick-boundary node; SCRIPT then keeps a script request's publication for its ticket and releases a C++ request's slot (`Zenith2/Docs/Design/SCRIPT/Runtime.md` → "Asynchronous result table") |
| `Zenith_AsyncRequestMultiplexer` | SCRIPT's `Zenith_ScriptSideEffectConsumer` for `SIDE_EFFECT_KIND_ASYNC_REQUEST` | one per process, in the runtime state, registered with `SetSideEffectConsumer(SIDE_EFFECT_KIND_ASYNC_REQUEST, …)` at step 15 before `Seal`; destroyed after every world | `Consume(uWorldTag, eKind, ulCompletionID, xPayload)` reads the source kind from payload byte 0: kind 8 (`ASYNC_SOURCE_SCENE_LOAD`) goes to the world's `Zenith_SceneWorldState::AcceptScriptLoadRequest(ulCompletionID, xPayload)`; any other kind goes to the consumer the owning module named through `Zenith_ModuleStartContext::SetScriptSideEffectConsumer(SIDE_EFFECT_KIND_ASYNC_REQUEST, …)` for the source kinds that module declared with `DeclareScriptAsyncSource`; an empty payload or a kind with no route is `ZENITH_ASSERT` and counted in `runtime.async.unrouted` (a binding that records a source kind nobody serves is an engine or module defect) |

Sink binding and lifetime (cross-doc item X274 (3)). Every runtime sink over a world's `Zenith_AsyncResultTable` — the scene publication sink above, the single module publication sink of the Q76 (a) ruling (`Zenith_ModulePublicationSink`, which `Zenith_WorldAttachContext::GetPublicationSink()` returns) and SAVE's sink — binds the `Zenith_AsyncResultTable*` once, at the world's creation (`Zenith2/Docs/Design/GAME/WorldHost.md` → "World creation" step 4, before any module attaches), and never re-resolves `Zenith_ScriptWorld::GetAsyncResults()`; moving the table's `Zenith_Owned` into a retired entry does not move its pointee, so a sink bound at creation keeps reaching the same table after the detach. The world host record holds the scene publication sink and the module publication sink as `Zenith_Owned` and destroys neither itself: the world host takes both into the world's `Zenith_RetiredResults` entry and destroys them with the table at its release (the table, then the scene sink, then the module sink), so a background job that completes after its world was destroyed (a SCENE or STREAM preparation continuation through the scene sink; an AI path job or PHYS's `DetachWorld` park through the module sink) reaches the retired table, which discards and counts the result. SAVE's sink is not taken, because SAVE unbinds the world in its own `DetachWorld` and no SAVE job completes into a destroyed world's table. The module publication sink's members, rules and tests are Zenith2/Docs/Design/MOD/RuntimeSeams.md → "Module publication sink".

<!-- elaboration -->
`Zenith_ModuleStartContext::SetScriptSideEffectConsumer` forwards to the binding table's `SetSideEffectConsumer` for every kind except `SIDE_EFFECT_KIND_ASYNC_REQUEST`, which has exactly one consumer per binding table ([Z2-N-SCRIPT-226]); for that kind the context records the module's consumer in the multiplexer's route table under each source kind the same module declared, and journals the entry like every other registration (§ "Registration journal and undo"). A second consumer for one source kind is a rejected registration. The multiplexer's route table is a fixed array of 16 entries indexed by source kind, written at step 15 only and read on merge nodes afterwards, so it needs no lock; its world lookup reads the world host records, which change only between frames.

The world-transition half of SCENE's runtime seams (`Zenith_SceneTransitionSink`, the transition record, isolated-world creation in prologue step 7 and the swap) is GAME's content of `Internal/Zenith_WorldHost.cpp` (`Zenith2/Docs/Design/GAME/WorldHost.md` → "World host files and split with MOD", "World transition"); MOD's step-16 code passes its sink through `Zenith_SceneWorldDesc::m_pxTransitions` once it exists.

## Threading lifetime and error contract

### Thread affinity per type

| Type | Affinity |
|---|---|
| `Zenith_GameComposition`, `Zenith_ModuleDescriptor`, `Zenith_RHIBackendEntry`, `Zenith_CodecEntry`, `Zenith_GameSettings` | `IMMUTABLE` |
| `Zenith_Runtime`, `Zenith_RuntimeTestAccess`, `Zenith_RuntimeMain` | `MAIN_THREAD` |
| `Zenith_ModuleInstance`, `Zenith_ModuleStartContext`, `Zenith_WorldAttachContext` | `MAIN_THREAD` (Architecture's blocks) |
| `Zenith_CommandLine` | `IMMUTABLE` after `Parse` |
| `Zenith_StartupReport` | `MAIN_THREAD` |
| `ZenithNewGameScaffold` functions | `ANY_THREAD` (no shared state) |

No runtime code in MOD's files runs on a job worker except the engine stage node bodies, whose affinity RPROD and GRAPH specify.

### Frame-graph nodes registered by ZenithRuntime

`Internal/Zenith_EngineStages.cpp` registers the engine stage nodes of Zenith2/Docs/Architecture/CPUFrameGraph.md → "Frame structure" once per frame schedule compile:

| Node | Subgraph | Affinity | Body specified in |
|---|---|---|---|
| `Render.Seal` | `FRAME_SUBGRAPH_EPILOGUE` | any worker | Zenith2/Docs/Architecture/CPUFrameGraph.md → "Render extraction and pipelining" |
| `Flux.BuildGraph` | `FRAME_SUBGRAPH_RENDER` | any worker | Zenith2/Docs/Rendering.md → "Render graph" |
| `Flux.Record` (parallel sub-jobs) | `FRAME_SUBGRAPH_RENDER` | any worker | Zenith2/Docs/Rendering.md → "Render graph" |
| `Flux.Submit`, `Flux.Present` | `FRAME_SUBGRAPH_RENDER` | the render-submit thread | Zenith2/Docs/Rendering.md → "GPU lifetime and synchronisation" |

In the headless configuration none of these is registered and the render subgraph is empty ([Z2-N-ARCH-849]); the per-world engine extractors (`Internal/Zenith_EngineExtractors.cpp`) are registered by the runtime's own attach step for every world, after the modules' `AttachWorld` calls. Modules add their nodes only through the attach context.

### Lock-order entries

MOD adds no lock and no row to Zenith2/Docs/Contracts.md → "Threading and concurrency contract" ("Lock ordering table"): the runtime, the module host, the journals and the contexts are main-thread objects touched only outside frame work. `Zenith_NewGameScaffold` holds no lock.

### Cancellation and shutdown

- The runtime owns no operation of its own except the world transition, whose cancellation is GAME's (Zenith2/Docs/Architecture/LifecycleCommands.md → "World transition"). Shutdown cancels every outstanding operation before destroying any subsystem (§ "Shutdown sequence").
- Threads: the runtime starts none directly. The job system's workers and the render-submit thread are joined by the job system's destructor; module-owned threads are joined by the owning module instance's destructor (Contracts.md's shutdown table step 6); the watchdog timer thread belongs to `Zenith_Platform`.
- A module instance's destructor runs with the job system still alive, so a module may drain its own background work there; it must not submit new frame work.

### Error enumerations

| Enumeration | Declared in | Enumerators |
|---|---|---|
| `Zenith_ModuleError` (record: `m_eCode`, `m_uModuleIndex`) | `ZenithRuntime/Zenith_Module.h` | `Zenith_ModuleErrorCode`: `MODULE_ERROR_REGISTRATION = 1`, `MODULE_ERROR_BACKEND_CREATE`, `MODULE_ERROR_DEVICE`, `MODULE_ERROR_OUT_OF_MEMORY` |
| `Zenith_RuntimeError` (record: `m_eCode`, `m_eStep`, `m_uModuleIndex`) | `ZenithRuntime/Zenith_Runtime.h` | `Zenith_RuntimeErrorCode`: `RUNTIME_ERROR_STARTUP_STEP = 1`, `RUNTIME_ERROR_CONFIGURATION`, `RUNTIME_ERROR_TRANSITION_PEAK_REFUSED`, `RUNTIME_ERROR_TRANSITION_IN_PROGRESS`, `RUNTIME_ERROR_UNKNOWN_WORLD` |
| `Zenith_CommandLineError` (record: `m_eCode`, `m_uArgIndex`) | `ZenithRuntime/Zenith_CommandLine.h` | `Zenith_CommandLineErrorCode`: `COMMAND_LINE_ERROR_UNKNOWN_OPTION = 1`, `COMMAND_LINE_ERROR_MISSING_ARGUMENT`, `COMMAND_LINE_ERROR_BAD_VALUE` |
| `Zenith_NewGameError` | `NewGameScaffold/Zenith_NewGameScaffold.h` | Zenith2/Docs/Design/MOD/ProjectCreation.md → "ZenithNewGameScaffold library API" |

Each code enumeration has a `Zenith_ToString` overload; each record is at most 4 bytes, trivially copyable, and carries no string or pointer ([Z2-N-CONTRACT-480]). Configure-time errors are CMake messages, not enumerations (Zenith2/Docs/Design/MOD/GameBuild.md → "Validation messages").

### Luau errors versus asserts

MOD exposes nothing to Luau: no binding is declared in MOD's files except the low-library bindings of `Internal/ScriptBindings/`, whose misuse errors are SCRIPT's. Every input MOD reads at run time — the command line, the composition (constant but validated), configuration values — is treated as untrusted and reported through `Zenith_Result` and exit codes, never an assert ([Z2-N-CONTRACT-472]). Asserts in MOD guard only programmer errors in the runtime itself: a module host called off the main thread (`ZENITH_ASSERT`), an unwind stack overflow (`ZENITH_CHECK`), and a state-machine transition not in the table (`ZENITH_UNREACHABLE`).

## Diagnostics and profiling

### Configure report

Every configure prints one line per game and preset (Zenith2/Docs/Design/MOD/GameBuild.md → "Configure report"), and a game skipped on a platform prints its skip line, so a CI log shows every game's resolution.

### Startup composition dump

At the end of step 7 (the first moment log sinks exist) the runtime logs the composition under the category `runtime.composition`, one fact per line:

```text
[runtime.composition] game=OpenWorldSlice version=0.1.0 hash=5d1f0a3c9e7b2248 build=91c4e07a2b6d38f5 headless=false
[runtime.composition] ecs=Flecs
[runtime.composition] rhi=Vulkan,D3D12 selected=vulkan (rhi.backend from content:/Config/Platform/windows.config.json:3)
[runtime.composition] codecs=zlib,zstd
[runtime.composition] module[0]=Animation iface=1
[runtime.composition] module[5]=Online iface=1 backend=Offline
[runtime.composition] module[6]=Physics iface=1 backend=PhysX
[runtime.composition] module[7]=AI iface=1
[runtime.composition] startupScene=content:/Scenes/Main.scene.json (engine default)
[runtime.composition] scriptRecovery=default
```

The `selected=` field and the startup-scene provenance come from `Zenith_Config::GetProvenance` (Zenith2/Docs/Architecture/PlatformLayer.md → "Configuration layering"). After step 15 each module logs `[runtime.module] <name> started in <µs> µs (<registrations> registrations)`.

### Startup timeline

After step 18 (or after a failure's unwind) `Zenith_RuntimeMain` logs the startup report under `runtime.startup`: one line per step, `step <nn> <name> <µs> µs`, then `total <ms> ms` and, on failure, `failed at step <nn> <name>; unwound <k> steps; exit <code>`. The total from process entry to the first frame is the startup's share of the W-Load cold-boot rows (see "Budget rows"); DEVT's on-screen profiler shows the same data in its startup panel from `Zenith_Runtime::GetStartupReport`.

### Profiler zones and counters

| Zone or counter | Emitted by |
|---|---|
| zone `Runtime.Startup.<nn>.<StepName>` | each startup step |
| zone `Runtime.Module.<Name>.RegisterTypes`, `.Start`, `.AttachWorld`, `.DetachWorld`, `.Destroy` | the module host around each entry point |
| zone `Runtime.Shutdown.<Phase>` | each shutdown phase of § "Shutdown sequence" |
| counter `runtime.modules` | the number of started modules |
| counter `runtime.worlds` | the number of live worlds |
| counter `runtime.registrations.rejected` | total rejected registrations since start (non-zero only in a failed start that the log explains) |

### Log categories

| Category | Content |
|---|---|
| `runtime.composition` | the composition dump and composition-validation failures |
| `runtime.startup` | step failures, the timeline |
| `runtime.module` | module start, attach, detach, destroy, rejected registrations |
| `runtime.shutdown` | shutdown phases and duration |
| `runtime.commandline` | parse errors (written to standard error as well, because they precede the log sinks) |

<!-- assumption: pending CORE -->
Categories are dotted lower-case names of the structured log that the CORE design specifies (`Zenith2/Docs/Design/CORE.md`, not yet signed off; its draft declares each category with `ZENITH_LOG_CATEGORY(Identifier, NameLiteral, CompiledMinimum)` and logs with `ZENITH_LOG_INFO` and its siblings). ZenithRuntime declares the five categories above in `Internal/Zenith_RuntimeLog.h`; the bracketed prefix in the examples is the text sink's rendering.
<!-- /assumption -->

### Tool diagnostics

`ZenithNewGame` reports through standard error in the fixed messages of Zenith2/Docs/Design/MOD/ProjectCreation.md → "ZenithNewGame command line"; `--dry-run` prints the plan with each file's size and SHA-256, so the wizard's preview and a reviewer see exactly what would be written.

## Authoring and cooking

### game.json authoring

`game.json` is authored by hand or written by `ZenithNewGame` and the wizard; every rule is checked at configure time with the exact messages of Zenith2/Docs/Design/MOD/GameBuild.md → "Validation messages", and `ZenithNewGame validate <Name>` checks the same rules without configuring. The editor's project-settings panel (EDIT) edits it through `ZenithNewGameScaffold`'s validator and canonical writer, never through its own serializer.

### Project creation

`ZenithNewGame create` and the editor wizard create a game through one library with one set of templates; the specification is `Zenith2/Docs/Design/MOD/ProjectCreation.md`.

### The editor wizard

EDIT owns the dialog; MOD owns what a creation does. The boundary is `Zenith2/Docs/Design/MOD/ProjectCreation.md → "Editor wizard boundary"`.

### Cook integration

MOD's part of cooking is limited to creating the `<Name>_Content` and `<Name>_Types` targets with the exact command lines of [Z2-N-ARCH-207] and providing `composition.json`. What the cook does with a game's content roots (importers, schema validation into `schemas.zreg`, `.d.luau` generation, packaging) is ASSET's and SCRIPT's.

### Migration of game.json

`game.json` is an authoring document (project settings) and migrates with preview, backup, journaled write and `x-` preservation through `ZenithNewGame --migrate` (Zenith2/Docs/Design/MOD/ProjectCreation.md → "game.json migration"). `zenith_add_game` accepts only the current `schemaVersion` and names the migration command when it meets another.

## Failure handling

### Configure-time failures

| Failure | Behaviour |
|---|---|
| invalid, missing, oversized or BOM-prefixed `game.json` | the configure fails with every collected message and the summary line; no target of the game exists |
| a missing module dependency | the configure fails with the dependency message; nothing is added implicitly |
| a module or backend not supported in the preset | the configure fails naming the preset and the library's matrix |
| PhysX selected for Android after a ruling against it | the configure fails with the PhysX message |
| a game directory with an unknown top-level entry | the configure fails naming the entry |
| an engine defect (no compression or RHI backend in a preset) | `FATAL_ERROR` naming the missing library |
| a game not built for the preset's OS | a `STATUS` skip line; not an error |

### Startup failures

Every startup failure unwinds exactly the steps constructed so far, in reverse, logs the failing step and cause, and exits with the code of § "Step failure to exit code mapping". No partially started module is left running, no registration is left behind, and the Debug leak report at root-heap destruction is empty.

### Module failures after startup

A module's `AttachWorld` failing for a world created after startup (a world transition) detaches the modules already attached to that world in reverse, destroys the new world, and reports the failure to the transition's operation handle; the running world is untouched (GAME's transition semantics). A module never fails in `DetachWorld` or its destructor; a defect there is a programmer error caught by its own asserts.

### Allocation failure

Every runtime allocation during startup uses `Zenith_TryNew` and maps failure to the step's failure (exit code 4, or the module's `MODULE_ERROR_OUT_OF_MEMORY`); after startup, allocation failure follows the OOM handler of Zenith2/Docs/Contracts/Memory.md → "OOM handler". The command line allocates nothing.

### Device loss and headless

Device loss is RHI's (Zenith2/Docs/Rendering.md → "Recovery"); when its policy chooses exit, the exit is clean: the device-loss handler requests an autosave and shutdown, the runtime runs the shutdown sequence and returns `EXIT_CODE_DEVICE_LOST` (6), and `ZENITH_FATAL(FATAL_CODE_DEVICE_LOST)` is raised only as the fallback when that clean exit has not finished within 10 s (Zenith2/Docs/Contracts.md → "Errors and results", [Z2-N-CONTRACT-484]; Zenith2/Docs/Architecture/CompositionRoot.md → "Exit codes"). `EXIT_CODE_DEVICE` (4) keeps the meaning "a required device could not be created". A headless run creates no device, so modules that need one (audio output, UI rendering) start in their headless mode and never report `MODULE_ERROR_DEVICE` for it.

### Project creation failures

`ZenithNewGame` leaves nothing behind on failure: a refused name or existing directory writes nothing; an I/O failure removes the partially written directory; a crash mid-write leaves a directory without `CMakeLists.txt` (so no build sees it) and with the `.newgame-incomplete` marker (so `create` refuses it with a message naming the fix). `--migrate` never leaves `game.json` half-written: the journal completes or rolls back on the next run.

## Test strategy

### Test files

| File | Target | Tests |
|---|---|---|
| `Zenith2/Engine/ZenithRuntime/Tests/Zenith_Composition_Tests.cpp` | `ZenithRuntime_Tests` | the composition-validation tests |
| `Zenith2/Engine/ZenithRuntime/Tests/Zenith_Runtime_Tests.cpp` | `ZenithRuntime_Tests` | Architecture's composition tests that run in-process, the exit-code and report tests |
| `Zenith2/Engine/ZenithRuntime/Tests/Zenith_RuntimeCreate_Tests.cpp` | `ZenithRuntime_Tests` | one failure-injection test per construction step |
| `Zenith2/Engine/ZenithRuntime/Tests/Zenith_ModuleHost_Tests.cpp` | `ZenithRuntime_Tests` | the module lifecycle tests |
| `Zenith2/Engine/ZenithRuntime/Tests/Zenith_CommandLine_Tests.cpp` | `ZenithRuntime_Tests` | the parser's edge cases |
| `Zenith2/Player/Tests/Zenith_PlayerArguments_Tests.cpp`, `Zenith_PlayerMain_Tests.cpp` | `ZenithPlayer_Tests` | `Test_Player_ArgumentParsing`, the Player main tests |
| `Zenith2/CMake/Tests/**` | CTest tests `CMake_*` | Zenith2/Docs/Design/MOD/GameBuild.md → "CMake fixture tests" |
| `Zenith2/Tools/NewGameScaffold/Tests/*.cpp`, `Zenith2/Tests/NewGame/*.cpp` | `ZenithNewGameScaffold_Tests`, `Zenith_Integration_NewGame` | Zenith2/Docs/Design/MOD/ProjectCreation.md → "Project creation tests" |
| `Zenith2/Games/<Name>/Tests/*.gametest.json` | CTest tests `Game_<Name>_<Stem>` | every repository game's boot tests |

### Generated test compositions

<!-- elaboration -->
In the `win` preset, `zenith_end_build` (in `CMake/ZenithAddGame.cmake`) runs the composition generator of Zenith2/Docs/Design/MOD/GameBuild.md in table-only mode over the fixtures `Zenith2/CMake/Tests/Composition/ModuleOrderShuffledA/` and `ModuleOrderShuffledB/` (the modules `UI, Cinematics, 2D, Gameplay, Animation` and `Animation, Gameplay, 2D, Cinematics, UI`), resolving the declared module set of each fixture's `game.json` against the fixed declaration table `Zenith2/CMake/Tests/GameJson/declarations.cmake` (as the script-mode runner does), never against the live declaration store, so the tables exist from Stage 2 although none of those modules lands before Stage 3; it writes `${ZENITH_OUT}/generated/ZenithRuntime/TestCompositions/Zenith_TestComposition_OrderA.generated.h` and `_OrderB.generated.h`, adds that directory to `ZenithRuntime_Tests`' private include directories, and records both files in `declared_deps.json`'s `generated` array with `attributedTo` `ZenithRuntime` (whose test target consumes them) and `producer` `zenith_end_build`. Table-only mode emits only `constexpr` module-descriptor arrays in a namespace named for the fixture, forward-declares each plain module's two entry points instead of including module headers (the fixtures select plain modules only, so no module or backend header is needed below the Player), and emits no `Zenith_GetGameComposition`. The recording fake modules of `Tests/FakeModules/` define the entry points of the five fixture modules (`2D`, `Animation`, `Cinematics`, `Gameplay`, `UI`), and the generated `Cinematics` descriptor carries `m_szModuleDependencies` `"Animation"` from the fixed table. The doctest therefore runs over tables the generator produced, not over hand-copied ones.

The runtime tests use fake module libraries under `Zenith2/Engine/ZenithRuntime/Tests/FakeModules/` (a recording module that logs every entry-point and context call into a test-owned buffer, a module whose `RegisterTypes`, `Start` or `AttachWorld` fails on demand, and a module that makes a rejected registration) and hand-built `constexpr` compositions in the test files, run headless against the fixture content of `Zenith2/Engine/ZenithRuntime/Tests/Fixtures/Content/` (a cooked minimal scene).

### Runtime and composition tests

| Test | Asserts |
|---|---|
| `Test_Composition_InterfaceVersionMismatch_Stops` | a descriptor with interface version 2 stops startup at the start of step 6 with exit code 3 and the exact message |
| `Test_Composition_ModuleOrderCanonical` | over the two generated test tables `Zenith_TestComposition_OrderA` and `_OrderB` (§ "Generated test compositions"), produced from one module set listed in two orders: the tables are identical entry for entry, both pass the canonical-order check, and a headless start with the recording fake modules registers them in that order (`2D`, `Animation`, `Cinematics`, `Gameplay`, `UI`); a hand-built table with `Cinematics` before `Animation` stops at step 10 with exit code 3 and the canonical-order message |
| `Test_Composition_Validate_RejectsEmptyRHIList` | an empty RHI span fails validation with its message |
| `Test_Composition_Validate_RejectsMissingCodec` | a codec list without `zstd` fails validation |
| `Test_Composition_Validate_RejectsDuplicateModuleName` | two descriptors named `UI` fail validation |
| `Test_Composition_Validate_RejectsNullEntryPoint` | a null `m_pfnStart` fails validation |
| `Test_Composition_Dump_ListsEveryEntry` | a captured log holds one `runtime.composition` line per module, the ECS and RHI lines and the hash, in the order of § "Startup composition dump" |
| `Test_Composition_StartupScene_IsConfigDefault` | `content.startupScene` reads the composition's path with provenance "engine default", and `--set` overrides it with command-line provenance |
| `Test_Runtime_StartupOrder` | the step hooks observe steps 4–18 in order, and destruction observes them in the shutdown order |
| `Test_Runtime_StartupFailureUnwinds` | for every step s, a `false` from `m_pfnBeforeStep(s)` unwinds exactly steps s−1 … 4 in reverse and returns the exit code of the step's first row in the mapping table, with `m_eStep` = s in the error record |
| `Test_Runtime_AfterStepFailure_UnwindsTheStepToo` | a `false` from `m_pfnAfterStep(s)` unwinds s and every earlier step |
| `Test_Runtime_Create_FailsAt<Step>_Unwinds` (15 tests, `<Step>` ∈ Platform, FileSystem, Configuration, Logging, MemoryGovernor, JobSystem, Registration, Assets, RHIDevice, Renderer, ScriptRuntime, ModuleStart, InitialWorld, StartupScene, Schedule) | the step's own construction fails through the fault-injecting heap of `ZenithTestSupport` (not a hook), and the test asserts the error, zero leaks from the test heap, no thread left running and no registration left behind ([Z2-N-CONTRACT-341]); the RHIDevice and Renderer cases run non-headless on a WARP device, the rest headless |
| `Test_Runtime_RegistryFrozenBeforeWorld` | Architecture's case |
| `Test_Runtime_HotReloadWindowOnlyInPrologue` | Architecture's case |
| `Test_Runtime_HeadlessSkipsDevice` | Architecture's case |
| `Test_Runtime_HeadlessMatchesWindowedSimulation` | Architecture's case, windowed on WARP |
| `Test_Runtime_ExitCodeMapping_MatchesTable` | each row of the mapping table, caused by its real cause (a fault-injecting heap, a missing mount, the required key `strings.maxStrings` removed from the fixture's platform configuration fragment, a failing fake module or backend factory, a corrupt fixture scene), yields its exit code |
| `Test_Runtime_FrameLoop_ExecutorProtocol` | a headless run of three frames with a forced schedule recompile, under capturing profiler hooks: the zone `Frame.ScheduleRecompile` appears in prologue step 8 of the forced frame only, the schedule cache's statistics show the superseded schedule released after the following `RunFrame`, and after the shutdown request the zone timeline shows no further `RunFrame` and no present of the render world extracted by the last iteration, followed by the executor's destruction before the job system's and the cache's after the executor's |
| `Test_Runtime_StartupPlatformDuties` | with `--timeout-seconds 30`, `--set platform.eventQueueCapacity=512` and `--record <tmp>` a headless start observes the early key in the platform description before step 4, the watchdog started after step 4, `LoadBindings` after `Zenith_Config::Create` in step 6, the record file opened through its grant, the log drain thread started after the last `AddSink` in step 7, and exactly one input world marked at step 16 |
| `Test_Runtime_FrameLoop_ThrottleAndCarry` | a forced `ShouldThrottle()` makes the loop call `WaitForEvents(100)` instead of `RunFrame`, and a paused frame with no tick is followed by one `CarryFrameTransitions()` call |
| `Test_Runtime_StepNine_AppliesJobConfiguration` | the float environment is applied on the main thread before the job system exists, `jobs.workerCount=3` yields three workers, and an invalid clock description stops step 9 with exit code 2 |
| `Test_Runtime_RhiBackend_InvalidValueFallsBackToDefault` | `--set rhi.backend=d3d12` selects the `D3D12` entry on a WARP device; `--set rhi.backend=metal` and `--set rhi.backend=D3D12` (wrong case) each log one `config` warning and leave `vulkan` selected, so the `Vulkan` entry's factory is the one called and no backend-name failure exists ([Z2-N-ARCH-929], ruling Q67 (a)) |
| `Test_Runtime_LogCategories_Declared` | across a start and a forced failure, every record whose category starts `runtime.` is in one of the five categories of § "Log categories", and each of the five appears at least once |
| `Test_Runtime_StartupReport_RecordsEveryStep` | a successful start records a non-zero duration for every executed step and `m_eLastCompletedStep = 18` |
| `Test_Runtime_ExitAfterTicks_RunsExactlyN` | `--exit-after-ticks 7` runs exactly 7 ticks of the primary world and exits 0, and a world swap after tick 4 still stops after 7 ticks in total |
| `Test_Runtime_LooseRootDefault_RelativeToExecutable` | without `--assets-root`, the loose mount resolves to the executable's `../../../Cooked/<Game>/windows/` |
| `Test_Runtime_ScriptRecoverySettings_Passed` | a composition declaring `automatic`, 30 and 2 makes the script runtime report exactly that policy; an undeclared one yields the configuration-dependent default |
| `Test_Runtime_StartupZones_Emitted` | capturing profiler hooks installed by the test records one `Runtime.Startup.<nn>.<StepName>` zone per executed step and one `Runtime.Module.<Name>.<EntryPoint>` zone per module call |

### Module lifecycle tests

| Test | Asserts |
|---|---|
| `Test_ModuleHost_RegisterTypesInCanonicalOrder` | three recording modules see `RegisterTypes` in composition order, all before `Zenith_Config::Create` and before any `Start` |
| `Test_Runtime_ModuleConfigKey_ResolvesFromGameLayer` | a recording module that registers the key `fake.value` in `RegisterTypes` reads back the game-layer value set in the fixture content's `game.config.json` through `GetConfig()` in `Start` |
| `Test_Runtime_EngineKeyRegistrationFailure_Exit2` | a duplicated engine key (injected through the fault-injecting heap's registration hook) fails step 6 with `RUNTIME_ERROR_CONFIGURATION` and exit code 2 |
| `Test_ModuleHost_RegisterTypesFailure_UnwindsRegistry` | the second module failing `RegisterTypes` fails step 6 with exit code 3, no module is started and the test heap reports no leak |
| `Test_ModuleHost_StartFailure_DestroysStartedInReverse` | the third of four modules failing `Start` destroys modules 2 and 1 in that order, never starts the fourth, and undoes every journal |
| `Test_ModuleHost_RejectedRegistration_FailsStart` | a module registering a duplicate command kind fails with `MODULE_ERROR_REGISTRATION` although its `Start` returned success, and its instance is destroyed |
| `Test_ModuleHost_CommandKindName_MustCarryModulePrefix` | `Audio.Play` registered by `Physics` is rejected with its reason |
| `Test_ModuleHost_AttachFailure_DetachesInReverse` | a module failing `AttachWorld` makes earlier modules receive `DetachWorld` for that world in reverse and the modules after it receive nothing |
| `Test_ModuleHost_DetachOnlyAttachedWorlds` | a module whose attach failed never receives `DetachWorld` for that world |
| `Test_ModuleHost_Shutdown_DetachesAndDestroysInReverse` | shutdown detaches every world from modules in reverse canonical order, then destroys instances in reverse |
| `Test_Runtime_ModuleRegistrationsUndone` | Architecture's case |
| `Test_ModuleHost_JournalFull_RejectsRegistration` | a 257th registration is rejected and fails the start |
| `Test_ModuleHost_StartThunk_PassesSelectedBackend` | a composition whose thunk binds a recording physics factory makes the fake physics module receive exactly that factory pointer |

### Player tests

| Test | Asserts |
|---|---|
| `Test_Player_ArgumentParsing` | Architecture's case, plus a `--set` without `=`, a missing argument, a repeated `--headless`, and 65 `--set` options each exit 2, with `m_uArgIndex` naming the offending argument |
| `Test_PlayerMain_ReturnsRuntimeExitCode` | `Zenith_PlayerMain` over a fake composition whose startup scene is missing returns 3 |
| `Test_PlayerMain_UnknownOption_CreatesNoRuntime` | an unknown option returns 2 and the root heap's allocation count is unchanged |
| `ZenithPlayer.EntryCallsCrtMathModeFirst` | (a `cmake -P` CTest in `Zenith2/CMake/Tests/ZenithCMakeTests.cmake`) the first statement of `main` in `Player/Internal/Windows/Zenith_PlayerEntry_Windows.cpp` and of `SDL_main` in `Player/Internal/Android/Zenith_PlayerEntry_Android.cpp` is `Zenith_ApplyCrtMathMode();`; fails naming the file otherwise |
| `Test_PlayerMain_ProfilerCreatedAndPassed` | `Zenith_PlayerMain` creates one `Zenith_Profiler` before parsing the command line, the runtime receives it, and it is destroyed after the hooks are uninstalled and before the heap; a profiler-creation failure injected through the heap's fault countdown returns 4 with no hook installed |
| `Test_CommandLine_RepeatedSet_KeepsOrder` | `--set a=1 --set a=2` yields `a=2` with command-line provenance |
| `Test_CommandLine_AssetsRootRefusedUnderShippingRules` | the internal parser `Zenith_ParseCommandLineWithRules(iArgc, ppszArgv, bShippingRules)` (`Internal/Zenith_CommandLine.cpp`, which `Parse` calls with `Zenith_IsShippingBuild()`) refuses `--assets-root` with `COMMAND_LINE_ERROR_UNKNOWN_OPTION` when `bShippingRules` is true and accepts it when false; tests never build a Shipping configuration ([Z2-N-ARCH-181]), so the rule is exercised through the parameter |

### Game boot tests

Every repository game carries `Tests/Boot.gametest.json` (Minimal and the templates: 1 tick; the workload games and slices: 300 ticks; 60 s each), registered as `Game_<Name>_Boot` (for example `Game_WorkloadOutdoor_Boot` and `Game_WorkloadCharacters_Boot`). For `Games/Minimal` the test is `Game_Minimal_Boot`, the Stage 2 proof that the whole path works: `game.json` → `zenith_add_game` → composition → Player → runtime startup → schema registration → data-only startup scene → one frame → clean shutdown with exit 0. From Stage 3, `Game_Minimal_Scripted` adds the behaviour and 300 ticks.

### Gate step interplay

| Gate step | What it proves for MOD |
|---|---|
| `SourceLayout` | no compiled-language source under `Games/` ([Z2-N-ARCH-205]); every game `CMakeLists.txt` is exactly one `zenith_add_game` call ([Z2-N-ARCH-157]); the generated TU lives under `out/`, outside the scanned tree |
| `CompileCoverage` | the generated TU is compiled in every configuration of the game target's matrix; the game's matrix is its `platforms` within `ZENITH_GPU`, recorded in `declared_deps.json`'s `games` array, so a Windows-only game is not expected to produce an Android object |
| `Libgraph` | the generated TU's includes and symbols are attributed to the game executable ([Z2-N-ARCH-081]); a module using another module without `DEPENDS` fails ([Z2-N-ARCH-266]); the runtime includes no module or backend header (reconciliation R1 keeps the composition type in the runtime); the Shipping link closure of every game contains no compiler, test or tool target ([Z2-N-ARCH-241]) |
| `ForbiddenCheck` | no `LoadLibrary*`/`dlopen` outside the two allowed `Internal/` trees ([Z2-N-ARCH-274]); the writable-data scan finds no mutable symbol in any game's composition object (`Test_Composition_NoMutableGlobals`) or in `ZenithRuntime` |
| `TestSeparationCheck` | no test target in any game's Shipping link closure |
| `HeaderSelfContained`, `ApiContractCheck` | every MOD public header compiles alone and carries its contract blocks |
| `ApkCheck` | `SDL_main` exported by every game shared object (`Test_Composition_AndroidExportsSdlMain`), 16 KB alignment of every `.so` and APK |
| `Test` | every doctest, fixture and gametest above |

### Which gate step runs each

| Tests | Gate step and preset |
|---|---|
| `ZenithRuntime_Tests`, `ZenithPlayer_Tests` | `Test` (`win-debug`, `win-release`); `Sanitizers -Preset win-asan` when the diff touches the `Zenith2Memory` category's paths |
| `CMake_*` | `Test` (`win-debug`) |
| `ZenithNewGameScaffold_Tests`, `Zenith_Integration_NewGame` | `Test` (`win-debug`; the integration suite in `win` alone) |
| `Game_<Name>_<Stem>` | `Test` (`win-debug`, `win-release`) and `Sanitizers -Preset win-asan` |
| the Android checks | `Build -Preset android-arm64` and `ApkCheck` (base category from Stage 2) |
| `Test_NewProjectWizard_MatchesCli` | `Test` (`win-debug`), from Stage 4 |
| the S24 boot of `Games/Minimal` | the Stage 2 acceptance run, a `needs-human` device step |

## Budget rows

MOD owns no budget row. It is measured against these rows, all from `C:/dev/Zenith2-drafts/areas/core/budget_rows.json`:

| Row | Name | Status | What of MOD it bounds |
|---|---|---|---|
| Z2-B-SHIP-013 | Cold boot Windows | adopted (10 s) | the startup sequence's share of the W-Load cold boot on Windows, reported by § "Startup timeline" |
| Z2-B-SHIP-014 | Cold boot S24 | adopted (15 s) | the same on the S24 |
| Z2-B-PROC-007 | Step timeout Configure | pending | `zenith_add_game`'s configure cost across every game |
| Z2-B-PROC-008, Z2-B-PROC-009, Z2-B-PROC-010 | Step timeout Build win-debug, win-release, android-arm64 | pending | every game's executable and content build within the Build step |
| Z2-B-PROC-011 | Step timeout Test | pending | the fixture, runtime and gametest suites |
| Z2-B-PROC-023 | Step timeout ApkCheck | pending | the Gradle builds `zenith_apks` runs |
| Z2-B-PACK-001 | AAB base module download size | pending | each game's shared object is the largest native part of the base module |

No new row is needed: startup time is inside the adopted cold-boot rows, and configure and build costs are inside the PROC step timeouts, whose producers are the Stage 0 and Stage 2 PROC tickets named in Zenith2/Docs/Budgets.md → "Row index".

## Stage plan

### Stage 0 scaffold

The scaffold ticket is the last Stage 0 ticket and the first `/tick` runs, parented to `Z2 MOD — Engine modules & projects` (Zenith2/Docs/Roadmap.md → "Stage 0 Tooling bring-up"). Its MOD-owned contents:

| Deliverable | Contents |
|---|---|
| root build | `Zenith2/CMakeLists.txt` and `Zenith2/CMakePresets.json` exactly as Zenith2/Docs/Architecture/BuildSystem.md → "Top-level tree and helper modules" and "Presets" give them, including `android-arm64` and `android-x86_64` |
| helpers | `CMake/ZenithBuild.cmake`, `ZenithAddLibrary.cmake`, `ZenithAddTool.cmake`, `ZenithWarnings.cmake`, `ZenithFlags.cmake`, `ZenithSupported.cmake`, `ZenithThirdParty.cmake`, the two toolchain files (Zenith2/Docs/Architecture/BuildSystem.md → "The zenith_add_library helper"), and `CMake/ZenithAddGame.cmake` in its Stage 0 subset (Zenith2/Docs/Design/MOD/GameBuild.md → "Stage 0 subset") |
| a first engine library | `Engine/ZenithCore/` with one empty-bodied public header and its `ZenithCore_Tests` holding one passing test, so every base gate step has something to check |
| the Player stub | `Player/CMakeLists.txt` (`KIND PLAYER`, no `DEPENDS` at Stage 0), `Player/Zenith_GameComposition.h` declaring the stub `Zenith_GameComposition { const char* m_szGameName; const char* m_szGameVersion; }` and `Zenith_GetGameComposition()`, `Player/Zenith_PlayerMain.h`, `Player/Internal/Zenith_PlayerMain.cpp` returning 0, and both platform entry points as in "ZenithPlayer files" |
| the stub game | `Games/Minimal/` with `CMakeLists.txt`, the Stage 0 `game.json` (keys `schemaVersion`, `name`, `displayName`, `version`, `platforms`, `content`, `android`) and empty `Assets/`, `Scenes/`, `Schemas/` and `Scripts/` directories (each holding `.keep`) |
| proof | `Build -Preset win-debug`, `win-release` and `android-arm64` produce `Minimal.exe` and `libMinimal.so`; `llvm-readelf --dyn-syms libMinimal.so` lists `SDL_main` |

The Stage 0 `ZenithPlayer` has no dependency because no runtime exists; its `DEPENDS` grows to the Architecture row in the Stage 2 game-model ticket. The D-SLANG (open, Stage 1) arm64 probe builds on this stub.

<!-- elaboration -->
Two Stage 0 forms are superseded by SHIP's test-target change (SHIP's T5; A3 cross-area row SH20): the scaffold applies a library's `TEST_LABELS` to its discovered tests through `set_tests_properties`, which T5 replaces by the discovery call's `LABELS` and doctest's `ADD_LABELS` of [Z2-N-ARCH-251]; and `zenith_end_build` creates the conformance executables before it includes the test registry. The registry `Zenith2/CMake/Tests/ZenithCMakeTests.cmake` ends, after every area block, with exactly three calls in this order: `_zenith_register_suite_content()` (Z2T-MOD-066), `_zenith_ship_registry_tail()` (SHIP's T5) and the scaffold's `_zenith_register_fuzz_targets()`, last, so every helper that creates a target runs before the generic fuzz rule.

### Stage 1

| Deliverable | Kind |
|---|---|
| the specification ticket for the static-module system and the in-repo game model (Zenith2/Docs/Roadmap.md → "Stage 1 Architecture validation", deliverable 17; Z2-D-002, Z2-D-024, Z2-D-081): confirms this document family against the Stage 1 rulings, rules on the six reconciliations of "Reconciliations with the core documents" with the Architecture owner, and appends the rulings to `Zenith2/Docs/DecisionLog.md` in its own diff | specification (`needs-human`) |
| Android packaging: `Player/GradleTemplate/`, the Gradle project generation and `<Name>_Apk` (Zenith2/Docs/Design/MOD/GameBuild.md → "Android Gradle wiring"), pulled forward because the D-SLANG arm64 probe installs an APK on the S24 | implementation (pulled forward, fix version `Z2 Stage 1`) |

### Stage 2

The in-repo game model (Zenith2/Docs/Roadmap.md → "Stage 2 Foundation", deliverable 7), as interface-first tickets:

1. **API-only:** `ZenithRuntime`'s public headers (`Zenith_GameComposition.h`, `Zenith_Module.h`, `Zenith_Runtime.h`, `Zenith_RuntimeTestAccess.h`, `Zenith_CommandLine.h`, `Zenith_ExitCode.h`) and the Player headers, with contract blocks and `HeaderSelfContained` passing, so module areas can write their entry points against them.
2. The full `zenith_add_game`: the validator, module and backend resolution, the generators, the build records, and the CMake fixture tests except the four that need `ZenithNewGame` (`CMake_GameJson_TooLarge`, `_ByteOrderMark`, `_DuplicateKey`, `_ValidateStepRefuses`), which land with item 6 together with `<Name>_Validate`.
3. The module host: registration, start, attach, journals, rejection rules and shutdown, with the fake-module tests.
4. The startup sequence, unwinding, exit-code mapping and startup report, with the failure-injection tests (blocked by the PLAT, JOBS, ECS, ASSET, RHI, RPROD and SCRIPT tickets that provide each step's subsystem; a step whose subsystem has not landed is a no-op construct/destroy pair until it does, recorded in the step table).
5. `Games/Minimal` completed: full `game.json`, a data-only scene, a schema and its one-frame boot test; built on Windows and Android and booted headless. Its behaviour is a Stage 3 ticket ("Minimal").
6. `ZenithNewGame` and `ZenithNewGameScaffold` with templates (all three select no module and attach no behaviour, so each boots at Stage 2), the C++ validator, the build-time `<Name>_Validate` step with its four fixtures, migration and the integration suite.

Gated by: D-SEL-JSON (open, Stage 1) for both validators' JSON handling in C++; D-SEL-PHYSX-ANDROID (open, Stage 1) for the PhysX matrix the validator reads (the mechanism is tested with fixture declarations before the ruling).

### Stage 3

The behaviour ticket for `Games/Minimal` and the templates: `Scripts/Main.luau` is written by the scaffold and attached to each template's `Main`/`Camera` entity, and `Games/Minimal` gains its behaviour and `Game_Minimal_Scripted` ("Minimal"; Zenith2/Docs/Design/MOD/ProjectCreation.md → "The written behaviour"). The first modules arrive (Physics, Animation, Audio, UI, 2D, Gameplay, Net: the table "Module to target mapping"), each implementing the entry points against the Stage 2 API; the workload games' `game.json` files are completed to "Repository games"; `scriptRecovery` is honoured by the script runtime.

### Stage 4

The editor wizard (Z2-I-MOD-09) over `ZenithNewGameScaffold`, with `Test_NewProjectWizard_MatchesCli`; the Online backend-list form with the Offline backend; the flagship slices' `game.json` files per "Repository games".

### Stage 5 to Stage 7

Stage 5 adds the VFX module to the games that list it. Stage 6 adds the Steam and Google Play Online backends to the backend table (each by its own `SUPPORTED` matrix, no generator change) and the Shipping packaging of every game (SHIP, PACK). Stage 7 (consoles, XR, networking) adds platforms: a new platform value in `platforms` is a `game.json` schema change with a migration step and a decision.

## Decisions

### Settled decisions governing MOD

| Decision | What it fixes for MOD |
|---|---|
| Z2-D-001 | CMake presets, Ninja Multi-Config, globbed sources, one library per backend, link-time backend selection by the consumer |
| Z2-D-002 | games in `Zenith2/Games/<Name>/`, statically linked, no C++, one `zenith_add_game` call, no installed SDK or out-of-repo project, creation by CLI and wizard |
| Z2-D-024 | optional modules are static libraries selected in `game.json` and registered by the composition root; no dynamic plugins |
| Z2-D-075 | modding excluded |
| Z2-D-081 | one executable per game: the shared Player main with the engine and the selected modules; the generated composition table is data, not game C++ |
| Z2-D-082 | third-party dependencies pinned and confined, which is why the Gradle build runs offline against pinned artefacts |
| Z2-D-046 | no mutable global state except the root heap and profiler hooks installed in `main`; the composition is constant data |
| Z2-D-045 | no exceptions outside the Luau firewall; `Zenith_Result` for recoverable errors, whose error type follows [Z2-N-CONTRACT-480] (reconciliation R2's string-free records); asserts for programmer errors |
| Z2-D-048 | `game.json` migrates as an authoring document |
| Z2-D-010 | the platforms `game.json` may name, minSdk 34, 16 KB pages |
| Z2-D-003, Z2-D-004 | one physics backend linked per game and platform; both compression backends always |
| Z2-D-097 | a ruling from MOD's Stage 1 specification ticket is appended to `Zenith2/Docs/DecisionLog.md` in its own diff |

### Open decisions MOD depends on

| Decision | Effect on MOD |
|---|---|
| D-SEL-PHYSX-ANDROID (open, Stage 1) | the `SUPPORTED` matrix of `ZenithPhysics_PhysX`, which the validator reads to refuse or accept PhysX for Android |
| D-SEL-JSON (open, Stage 1) | the JSON reader and writer under the C++ validator and the canonical writer |
| D-SEL-HASH (open, Stage 1) | none directly (the composition hash is SHA-256 in CMake), but the evidence fingerprint that hashes `composition.json` uses it |
| D-SEL-CRASH (open, Stage 2) | the crash reporter created at startup step 7 |
| D-SEL-2DPHYS (open, Stage 2) | whether the `2D` module gains a module dependency on `Physics` (a row change in "Module to target mapping") |
| D-SLANG (open, Stage 1) | consumes the Stage 0 Player stub and the Stage 1 Android packaging |
| D-SEL-EnTT-SHIPPING (flagged proposal P1, Stage 5; not an accepted decision) | if approved, `EnTT` could become research-only, which would change `WorkloadCharacters`' `backends.ecs` and the validator's allowed values |

### Selections MOD owns

MOD owns no selection or classification decision. Its choices are elaborations recorded in this document family and confirmed by the Stage 1 specification ticket: the reconciliations R1–R6, the per-preset generated-file directory, the gametest format, the allowed game-directory entries, `Games/Minimal`, and the backend-list `Start` form.

## Requirement index

Every requirement of `Zenith2/Docs/Requirements/MOD.md` and the headings that specify it (paths relative to `Zenith2/Docs/Design/`).

| Requirement | Title | Stage | Design headings |
|---|---|---|---|
| Z2-R-MOD-001 | Games live only under Zenith2/Games | S2 | MOD.md → "Game directory layout"; MOD/GameBuild.md → "Step by step" |
| Z2-R-MOD-002 | No compiled-language source in a game | S2 | MOD.md → "Game directory layout"; MOD.md → "Gate step interplay"; MOD/GameBuild.md → "Target creation" |
| Z2-R-MOD-003 | Game directory entries are restricted | S2 | MOD/GameBuild.md → "Game directory entries"; MOD/GameBuild.md → "Validation messages" |
| Z2-R-MOD-004 | A game CMakeLists.txt is one zenith_add_game call | S2 | MOD.md → "Game directory layout"; MOD/ProjectCreation.md → "Files every template writes" |
| Z2-R-MOD-005 | Every game builds on every change | S2 | MOD.md → "Supported matrices"; MOD/GameBuild.md → "Target creation" |
| Z2-R-MOD-006 | Games/Minimal builds on Windows and Android | S2 | MOD.md → "Minimal"; MOD.md → "Stage 2" |
| Z2-R-MOD-007 | Games/Minimal boots headless | S2 | MOD.md → "Minimal"; MOD.md → "Game boot tests"; MOD.md → "The gametest format" |
| Z2-R-MOD-008 | Games/Minimal runs on the S24 | S2 | MOD.md → "Minimal"; MOD.md → "Which gate step runs each"; MOD/GameBuild.md → "Android Gradle wiring" |
| Z2-R-MOD-009 | Workload games select the tabled modules and backends | S3 | MOD.md → "Workload games"; MOD.md → "Canonical module order" |
| Z2-R-MOD-010 | Flagship slices select the tabled modules and backends | S4 | MOD.md → "Flagship slices" |
| Z2-R-MOD-011 | The game model is specified in Stage 1 | S1 | MOD.md → "Reconciliations with the core documents"; MOD.md → "Stage 1" |
| Z2-R-MOD-012 | zenith_add_game checks the game name | S2 | MOD/GameBuild.md → "Step by step"; MOD/GameBuild.md → "Validation messages" |
| Z2-R-MOD-013 | game.json is read through the CMake JSON reader | S2 | MOD/GameBuild.md → "Reading game.json through the CMake JSON reader"; MOD/GameBuild.md → "Validation messages" |
| Z2-R-MOD-014 | Validation reports every error in one configure | S2 | MOD/GameBuild.md → "Step by step"; MOD/GameBuild.md → "Validation messages" |
| Z2-R-MOD-015 | Validation messages use the fixed formats | S2 | MOD/GameBuild.md → "Validation messages" |
| Z2-R-MOD-016 | A game not built for the OS is skipped | S2 | MOD/GameBuild.md → "Step by step" |
| Z2-R-MOD-017 | Modules resolve to KIND MODULE libraries | S2 | MOD/GameBuild.md → "Module resolution"; MOD/GameBuild.md → "Module to target mapping" |
| Z2-R-MOD-018 | Module dependencies are never added implicitly | S2 | MOD/GameBuild.md → "Module resolution"; MOD/GameBuild.md → "Validation messages" |
| Z2-R-MOD-019 | Unsupported modules and backends fail per preset | S2 | MOD/GameBuild.md → "Module resolution"; MOD/GameBuild.md → "Backend resolution" |
| Z2-R-MOD-020 | Modules register in canonical order | S2 | MOD.md → "Canonical module order"; MOD/GameBuild.md → "Module resolution"; MOD.md → "Generated test compositions"; MOD.md → "Composition validation at startup" |
| Z2-R-MOD-021 | The ECS backend is selected at link time | S2 | MOD/GameBuild.md → "Backend resolution"; MOD/GameBuild.md → "Backend to target mapping" |
| Z2-R-MOD-022 | The physics backend is selected per platform | S2 | MOD/GameBuild.md → "Backend resolution"; MOD/GameBuild.md → "Keys" |
| Z2-R-MOD-023 | PhysX for Android follows the capability matrix | S2 | MOD/GameBuild.md → "Backend resolution"; MOD/GameBuild.md → "Validation messages" |
| Z2-R-MOD-024 | Both compression backends are always linked | S2 | MOD/GameBuild.md → "Backend resolution"; MOD/GameBuild.md → "Backend to target mapping" |
| Z2-R-MOD-025 | RHI backends follow the platform | S2 | MOD/GameBuild.md → "Backend resolution"; MOD/GameBuild.md → "Backend to target mapping" |
| Z2-R-MOD-026 | Online backends are all linked and chosen by configuration | S4 | MOD/GameBuild.md → "Backend resolution"; MOD/GameBuild.md → "Module to target mapping"; MOD/GameBuild.md → "Thunk rules" |
| Z2-R-MOD-027 | Generated files are per preset and rewritten only on change | S2 | MOD/GameBuild.md → "Generated file writing" |
| Z2-R-MOD-028 | The composition TU holds only constant data and thunks | S2 | MOD/GameBuild.md → "Generated composition TU template"; MOD/GameBuild.md → "Thunk rules"; MOD/GameBuild.md → "Include rules" |
| Z2-R-MOD-029 | Factory symbols come from backend declarations | S2 | MOD/GameBuild.md → "Backend resolution"; MOD/GameBuild.md → "Backend to target mapping" |
| Z2-R-MOD-030 | The composition hash follows ARCH-864 | S2 | MOD.md → "Composition hash"; MOD.md → "Hash golden"; MOD/GameBuild.md → "Template text" |
| Z2-R-MOD-031 | composition.json records the resolved composition | S2 | MOD.md → "The composition.json record"; MOD/GameBuild.md → "Build records" |
| Z2-R-MOD-032 | The Windows game is an executable whose one source is generated | S2 | MOD/GameBuild.md → "Target creation" |
| Z2-R-MOD-033 | The Android game is one shared object exporting SDL_main | S2 | MOD/GameBuild.md → "Android shared object specifics"; MOD/GameBuild.md → "Target creation" |
| Z2-R-MOD-034 | Windows games carry the D3D12 Agility exports | S2 | MOD/GameBuild.md → "Windows executable specifics"; MOD/GameBuild.md → "Include rules" |
| Z2-R-MOD-035 | Content and type targets use the fixed cook commands | S2 | MOD/GameBuild.md → "Content and type targets" |
| Z2-R-MOD-036 | Game tests are registered from gametest files | S2 | MOD/GameBuild.md → "Game test registration"; MOD.md → "The gametest format" |
| Z2-R-MOD-037 | Aggregate targets build every game | S2 | MOD/GameBuild.md → "The Apk target"; MOD/GameBuild.md → "Target creation" |
| Z2-R-MOD-038 | Games are recorded in declared_deps.json | S2 | MOD/GameBuild.md → "Build records"; MOD.md → "Gate step interplay" |
| Z2-R-MOD-039 | The configure report names every game resolution | S2 | MOD/GameBuild.md → "Configure report"; MOD.md → "Configure report" |
| Z2-R-MOD-040 | CMake fixture suite covers the validator | S2 | MOD/GameBuild.md → "CMake fixture tests"; MOD/GameBuild.md → "Fixture list" |
| Z2-R-MOD-041 | Gradle project is generated per game and preset | S1 | MOD/GameBuild.md → "Gradle project generation"; MOD/GameBuild.md → "Placeholders" |
| Z2-R-MOD-042 | JNI staging and build types match configurations | S1 | MOD/GameBuild.md → "Build types and JNI staging" |
| Z2-R-MOD-043 | The Apk target runs pinned Gradle offline | S1 | MOD/GameBuild.md → "The Apk target" |
| Z2-R-MOD-044 | Cooked content ships in the APK assets | S2 | MOD/GameBuild.md → "Build types and JNI staging" |
| Z2-R-MOD-045 | schemaVersion 1 is the only accepted version | S2 | MOD/GameBuild.md → "Keys"; MOD/GameBuild.md → "Validation messages" |
| Z2-R-MOD-046 | Identity keys are validated | S2 | MOD/GameBuild.md → "Keys" |
| Z2-R-MOD-047 | platforms selects windows and android | S2 | MOD/GameBuild.md → "Keys" |
| Z2-R-MOD-048 | modules is a unique list of module names | S2 | MOD/GameBuild.md → "Keys" |
| Z2-R-MOD-049 | content roots and startup scene are validated | S2 | MOD/GameBuild.md → "Keys"; MOD/GameBuild.md → "Validation messages" |
| Z2-R-MOD-050 | The android block exists exactly for Android games | S2 | MOD/GameBuild.md → "Keys" |
| Z2-R-MOD-051 | scriptRecovery is validated and carried to the runtime | S3 | MOD/GameBuild.md → "Keys"; MOD.md → "Zenith_GameComposition.h" |
| Z2-R-MOD-052 | Unknown keys fail and x- keys are ignored | S2 | MOD/GameBuild.md → "Keys"; MOD/ProjectCreation.md → "game.json migration" |
| Z2-R-MOD-053 | A module is a static library with two entry points | S2 | MOD/GameBuild.md → "Module to target mapping"; MOD.md → "Module root headers" |
| Z2-R-MOD-054 | The module table is the contract for module areas | S2 | MOD/GameBuild.md → "Module to target mapping" |
| Z2-R-MOD-055 | Module dependencies are declared and checked | S2 | MOD.md → "Gate step interplay"; MOD/GameBuild.md → "Module resolution" |
| Z2-R-MOD-056 | An unselected module is absent from the game | S2 | MOD/GameBuild.md → "Target creation"; MOD/GameBuild.md → "Content and type targets" |
| Z2-R-MOD-057 | No code is loaded at run time | S2 | MOD.md → "Exclusions"; MOD.md → "Gate step interplay" |
| Z2-R-MOD-058 | Backends reach modules only through Start | S2 | MOD/GameBuild.md → "Thunk rules"; MOD/GameBuild.md → "Module to target mapping" |
| Z2-R-MOD-059 | Composition types live in ZenithRuntime | S2 | MOD.md → "Zenith_GameComposition.h"; MOD.md → "Player headers"; MOD.md → "Reconciliations with the core documents" |
| Z2-R-MOD-060 | The composition is validated at startup | S2 | MOD.md → "Composition validation at startup" |
| Z2-R-MOD-061 | An interface-version mismatch stops startup | S2 | MOD.md → "Composition validation at startup" |
| Z2-R-MOD-062 | Registration follows the fixed order | S2 | MOD.md → "Configuration-key registration"; MOD.md → "Module lifecycle state machine"; MOD.md → "Startup sequence with unwinding" |
| Z2-R-MOD-063 | A RegisterTypes failure unwinds the registry | S2 | MOD.md → "Module lifecycle state machine" |
| Z2-R-MOD-064 | A Start failure destroys started modules in reverse | S2 | MOD.md → "Module lifecycle state machine"; MOD.md → "Step failure to exit code mapping" |
| Z2-R-MOD-065 | A rejected registration fails the call that made it | S2 | MOD.md → "Registration rejection rules" |
| Z2-R-MOD-066 | Modules attach to every world in canonical order | S2 | MOD.md → "Module lifecycle state machine" |
| Z2-R-MOD-067 | Registrations are undone in reverse | S2 | MOD.md → "Registration journal and undo" |
| Z2-R-MOD-068 | Shutdown detaches and destroys modules in reverse | S2 | MOD.md → "Shutdown sequence" |
| Z2-R-MOD-069 | No registration through static initialisation | S2 | MOD.md → "Gate step interplay"; MOD/GameBuild.md → "Include rules" |
| Z2-R-MOD-070 | Startup runs the steps in order and unwinds in reverse | S2 | MOD.md → "Startup sequence with unwinding"; MOD.md → "Zenith_RuntimeTestAccess.h" |
| Z2-R-MOD-071 | Every startup step has a failure-injection test | S2 | MOD.md → "Runtime and composition tests"; MOD.md → "Startup sequence with unwinding" |
| Z2-R-MOD-072 | Startup failures map to the exit-code table | S2 | MOD.md → "Step failure to exit code mapping"; MOD.md → "Zenith_ExitCode.h" |
| Z2-R-MOD-073 | Headless is the same executable without a device | S2 | MOD.md → "Device loss and headless"; MOD.md → "Frame-graph nodes registered by ZenithRuntime" |
| Z2-R-MOD-074 | Headless simulation matches windowed simulation | S2 | MOD.md → "Runtime and composition tests" |
| Z2-R-MOD-075 | The command line accepts exactly the option table | S2 | MOD.md → "Zenith_CommandLine.h"; MOD.md → "Player tests" |
| Z2-R-MOD-076 | The Player installs the root heap and hooks first | S2 | MOD.md → "Player headers" |
| Z2-R-MOD-077 | The startup report records every step | S2 | MOD.md → "Startup report"; MOD.md → "Zenith_Runtime.h" |
| Z2-R-MOD-078 | The composition is dumped at startup | S2 | MOD.md → "Startup composition dump"; MOD.md → "Log categories" |
| Z2-R-MOD-079 | Startup emits profiler zones and the timeline | S2 | MOD.md → "Profiler zones and counters"; MOD.md → "Startup timeline" |
| Z2-R-MOD-080 | The loose content root defaults beside the executable | S2 | MOD.md → "Startup scene and loose content root" |
| Z2-R-MOD-081 | The startup scene is a configuration default | S2 | MOD.md → "Startup scene and loose content root" |
| Z2-R-MOD-082 | Exit after ticks runs exactly the requested ticks | S2 | MOD.md → "Frame loop driver" |
| Z2-R-MOD-083 | Startup fits the cold-boot rows | S3 | MOD.md → "Startup timeline"; MOD.md → "Budget rows" |
| Z2-R-MOD-084 | ZenithNewGame create takes the fixed options and defaults | S2 | MOD/ProjectCreation.md → "Synopsis"; MOD/ProjectCreation.md → "ZenithNewGame command line" |
| Z2-R-MOD-085 | Invalid names are refused with exit code 2 | S2 | MOD/ProjectCreation.md → "Output"; MOD/ProjectCreation.md → "Scaffold algorithm" |
| Z2-R-MOD-086 | Creation writes the template files and no C++ | S2 | MOD/ProjectCreation.md → "Templates"; MOD/ProjectCreation.md → "Files every template writes" |
| Z2-R-MOD-087 | Creation is atomic | S2 | MOD/ProjectCreation.md → "Scaffold algorithm" |
| Z2-R-MOD-088 | A created game configures, builds and boots | S2 | MOD/ProjectCreation.md → "Project creation tests" |
| Z2-R-MOD-089 | Creation output is byte-stable | S2 | MOD/ProjectCreation.md → "Canonical JSON"; MOD/ProjectCreation.md → "The written game.json" |
| Z2-R-MOD-090 | validate checks a game without configuring | S2 | MOD/ProjectCreation.md → "Validation in C++" |
| Z2-R-MOD-091 | The C++ and CMake validators agree | S2 | MOD/ProjectCreation.md → "Validation in C++"; MOD/GameBuild.md → "Fixture list" |
| Z2-R-MOD-092 | game.json migrates as an authoring document | S2 | MOD/ProjectCreation.md → "game.json migration" |
| Z2-R-MOD-093 | ZenithNewGame exit codes are fixed | S2 | MOD/ProjectCreation.md → "Exit codes" |
| Z2-R-MOD-094 | The Zenith2 root is found or given | S2 | MOD/ProjectCreation.md → "Synopsis"; MOD/ProjectCreation.md → "Scaffold algorithm" |
| Z2-R-MOD-095 | The wizard creates games through the scaffold library | S4 | MOD/ProjectCreation.md → "Editor wizard boundary" |
| Z2-R-MOD-096 | The wizard shows the plan and the validator messages | S4 | MOD/ProjectCreation.md → "Editor wizard boundary" |
| Z2-R-MOD-097 | The wizard tells the author how the new game builds | S4 | MOD/ProjectCreation.md → "Editor wizard boundary" |
| Z2-R-MOD-098 | The Stage 0 scaffold lands the presets and a Player stub | S0 | MOD.md → "Stage 0 scaffold" |
| Z2-R-MOD-099 | The Stage 0 stub game builds on Windows and Android | S0 | MOD.md → "Stage 0 scaffold"; MOD/GameBuild.md → "Stage 0 subset" |
| Z2-R-MOD-100 | Dynamic plugins are excluded | S1 | MOD.md → "Exclusions" |
| Z2-R-MOD-101 | An installed SDK is excluded | S1 | MOD.md → "Exclusions" |
| Z2-R-MOD-102 | Out-of-repo projects are excluded | S1 | MOD.md → "Exclusions" |
| Z2-R-MOD-103 | Third-party modding is excluded | S1 | MOD.md → "Exclusions" |
| Z2-R-MOD-104 | The build runs the C++ validator | S2 | MOD/GameBuild.md → "Checks only the C++ validator makes"; MOD/GameBuild.md → "Content and type targets" |
| Z2-R-MOD-105 | Workload games gain their Stage 4 modules | S4 | MOD.md → "Workload games" |
| Z2-R-MOD-106 | The build identity carries the per-preset inputs | S2 | MOD.md → "Build identity"; MOD/GameBuild.md → "Template text" |
| Z2-R-MOD-107 | Runtime error records are string-free | S2 | MOD.md → "Error enumerations"; MOD.md → "Reconciliations with the core documents"; MOD.md → "Zenith_Runtime.h" |
| Z2-R-MOD-108 | Startup steps drive CORE, PLAT, JOBS and GRAPH as their designs specify | S2 | MOD.md → "Step construction notes"; MOD.md → "Step failure to exit code mapping" |
| Z2-R-MOD-109 | The frame-loop driver follows the executor protocol | S2 | MOD.md → "Frame loop driver"; MOD.md → "Shutdown sequence" |
| Z2-R-MOD-110 | Games/Minimal gains its behaviour at Stage 3 | S3 | MOD.md → "Minimal"; MOD.md → "Stage 3" |
| Z2-R-MOD-111 | Templates write and attach the Main behaviour from Stage 3 | S3 | MOD/ProjectCreation.md → "The written behaviour"; MOD/ProjectCreation.md → "Files every template writes" |
| Z2-R-MOD-112 | One module publication sink per world, bound at creation and retired with the table | S3 | MOD/RuntimeSeams.md → "Sink binding at world creation"; MOD/RuntimeSeams.md → "Sink lifetime and retirement" |
| Z2-R-MOD-113 | Sink sources and handlers are registrations with rejection rules | S3 | MOD/RuntimeSeams.md → "Source declaration and handler registration" |
| Z2-R-MOD-114 | Sink allocation is first-fit at declared sites and refused under replay in the prologue | S3 | MOD/RuntimeSeams.md → "Allocation sites and acceptance"; MOD/RuntimeSeams.md → "Replay behaviour of the sink" |
| Z2-R-MOD-115 | Parked results reach the table live or retired and are forwarded to module handlers | S3 | MOD/RuntimeSeams.md → "Parking a result"; MOD/RuntimeSeams.md → "The module publication forwarder"; MOD/RuntimeSeams.md → "Sink diagnostics" |
| Z2-R-MOD-116 | The scene sink forwards SCENE's resolver and drops late local completions | S3 | MOD.md → "SCENE and SCRIPT seams of the runtime" |
| Z2-R-MOD-117 | The world attach context answers the sink, command-kind, control, clock and purpose lookups | S3 | MOD/RuntimeSeams.md → "World attach context lookups" |
| Z2-R-MOD-118 | Test assertions are recorded from any thread and worlds can request shutdown | S2 | MOD/RuntimeSeams.md → "Test assertion failures and world shutdown requests"; MOD.md → "Frame loop driver" |
| Z2-R-MOD-119 | World creation is factored for the world host and destruction walks GAME's teardown order | S2 | MOD/RuntimeSeams.md → "Runtime state the world host shares"; MOD/RuntimeSeams.md → "World creation and destruction"; MOD/RuntimeSeams.md → "Call sites GAME's tickets add" |
| Z2-R-MOD-120 | A swap's schedule is precompiled and the swap is observable through test hooks | S3 | MOD/RuntimeSeams.md → "Swap precompile and test hooks" |
| Z2-R-MOD-121 | Modules reach the profiler overlay from their start context | S2 | MOD/RuntimeSeams.md → "Start context overlay access" |
| Z2-R-MOD-122 | The start context implements frame listeners, engine epilogue stages, script consumers and script sources | S3 | MOD/RuntimeSeams.md → "Start context registrations" |
| Z2-R-MOD-123 | Modules' fault points and test mount sources are reachable in a booted runtime | S3 | MOD/RuntimeSeams.md → "Runtime test seams for modules" |
| Z2-R-MOD-124 | A source kind has one declaration point and FULL is reported at allocation | S3 | MOD/RuntimeSeams.md → "Source declaration and handler registration"; MOD/RuntimeSeams.md → "Parking a result" |
| Z2-R-MOD-125 | A mixed kind's owner allocates its notifications in the prologue through the sink | S3 | MOD/RuntimeSeams.md → "Allocation sites and acceptance"; MOD/RuntimeSeams.md → "Parking a result" |
| Z2-R-MOD-126 | Modules learn a recording run and can request an exit after start | S4 | MOD/RuntimeSeams.md → "Start context members for ONLINE and PACK" |
| Z2-R-MOD-127 | One module provides the package entitlement query from its start context | S6 | MOD/RuntimeSeams.md → "Start context members for ONLINE and PACK" |
| Z2-R-MOD-128 | Test hooks override the platform base path | S6 | MOD/RuntimeSeams.md → "Runtime test seams for modules" |
| Z2-R-MOD-129 | A tool host drives the game's runtime through the tool access header | S3 | MOD/RuntimeSeams.md → "Runtime tool access" |
| Z2-R-MOD-130 | The Player's entries apply the CRT maths mode first and the runtime checks it | S2 | MOD.md → "Player headers"; MOD.md → "Zenith_Runtime.h" |
| Z2-R-MOD-131 | Android backup and transfer rules keep the device record and transient save files out | S4 | MOD/GameBuild.md → "Backup and transfer rules" |

## Repository games

### Minimal

`Zenith2/Games/Minimal` is the Stage 2 proving game (Zenith2/Docs/Roadmap.md → "Stage 2 Foundation": "a minimal game under `Zenith2/Games/` that builds on Windows and Android"). It is kept after Stage 2 as the smallest game that exercises every step of the game model, and it is the reference output of `ZenithNewGame create Minimal --template Blank` plus one schema.

<!-- elaboration -->
`Games/Minimal` is a ninth repository game beside the eight that Z2-D-002 and Zenith2/Docs/Architecture.md → "Repository layout" ("Games in the repository") list; the Roadmap's Stage 2 deliverable requires it, and adding its row to that table is recorded in `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md`.

```json
{
	"schemaVersion": 1,
	"name": "Minimal",
	"displayName": "Minimal",
	"version": "0.1.0",
	"platforms": ["android", "windows"],
	"modules": [],
	"backends": {
		"ecs": "Custom"
	},
	"content": {
		"roots": ["Assets", "Scenes", "Schemas", "Scripts"],
		"startupScene": "Scenes/Main.scene.json"
	},
	"android": {
		"applicationId": "com.zenith2.minimal",
		"versionCode": 1,
		"orientation": "landscape"
	}
}
```

| File | Content |
|---|---|
| `Scenes/Main.scene.json` | a data-only scene: one entity `Main` with a transform and the `MinimalCounter` component; no behaviour |
| `Scripts/.keep` | empty; the `Scripts` content root exists with no module in it until Stage 3 |
| `Schemas/MinimalCounter.schema.json` | one component `MinimalCounter` (`stableName` `Minimal.MinimalCounter`, `version` 1, `authority` `EarlyScripts`, one field `{ "id": 1, "name": "Ticks", "type": "u32" }`), so startup step 10 registers a data-defined type from `schemas.zreg` |
| `Assets/Config/game.config.json` | the empty game configuration layer |
| `Tests/Boot.gametest.json` | 1 tick, 60 s: the smoke boots headless, runs one frame through the frame loop and exits 0 |

<!-- elaboration -->
`Games/Minimal` ships at Stage 2 with no behaviour script, because the Luau gameplay runtime is a Stage 3 deliverable (Zenith2/Docs/Roadmap.md → "Stage 3 Playable authoring slice", deliverable 7) and a scene attaching a behaviour nothing can load stops at step 17. The Stage 3 ticket that lands behaviours adds `Scripts/Main.luau` (the Blank template's behaviour, extended to write `MinimalCounter.Ticks` each tick), attaches it to the `Main` entity, and adds `Tests/Scripted.gametest.json` (300 ticks, 60 s), registered as `Game_Minimal_Scripted`.

### Workload games

The focused workload games (Zenith2/Docs/Budgets.md → "Workload definitions") and their selections. Each workload game's content is authored by the owning area's Stage 3 workload-game ticket inside these selections; changing a selection edits this table in the same diff.

A module that has not yet landed (its "First stage" in Zenith2/Docs/Design/MOD/GameBuild.md → "Module to target mapping") is added to a game's `game.json` by the ticket that lands the module or the game content needing it, never earlier, because `zenith_add_game` refuses an unknown module. So each game has a Stage 3 selection (the modules first available in Stage 2 or 3) and a full selection (from Stage 4); this rule applies to the flagship slices below too.

| Game | Owner | `platforms` | Stage 3 `modules` (canonical order) | Full `modules` from Stage 4 (canonical order) | `backends.ecs` | `backends.physics` |
|---|---|---|---|---|---|---|
| `WorkloadIndoor` | RPROD | `android`, `windows` | `Gameplay` | `Gameplay` | `Custom` | — |
| `WorkloadOutdoor` | RPROD | `android`, `windows` | `Gameplay` | `Environment`, `Gameplay`, `Streaming` | `Flecs` | — |
| `WorkloadCharacters` | ANIM | `android`, `windows` | `Animation`, `Gameplay`, `Net`, `Physics` | `Animation`, `Gameplay`, `Net`, `Physics`, `AI` | `EnTT` | `Jolt` |
| `WorkloadDense` | ECS | `android`, `windows` | `Gameplay`, `Physics` | `Gameplay`, `Physics` | `Custom` | `Jolt` |
| `WorkloadUI2D` | UI | `android`, `windows` | `2D`, `Gameplay`, `UI` | `2D`, `Gameplay`, `UI` | `Custom` | — |

`WorkloadCharacters` selects `Net` because Zenith2/Docs/Architecture.md → "Multiplayer foundations" names it as the game the Stage 3 loopback proof drives with `--set net.role=…`; `AI` joins at Stage 4, when `ZenithAI` lands.

<!-- assumption: pending ARCH -->
Architecture's sentence currently reads "The proof is the integration suite `Zenith2/Tests/NetLoopback/` driving two processes of the game `Games/WorkloadCharacters` (Physics, Animation, AI and Net modules)." At Stage 3 the game links `Animation`, `Gameplay`, `Net` and `Physics` (the loopback needs no AI); the batched Architecture patch rewords the parenthesis to that set (cross-area note M32).
<!-- /assumption -->

<!-- elaboration -->
The ECS backends are spread so that each of the three is the linked backend of at least one game built on every change: a backend regression then breaks a real game's build and boot test, not only its conformance suite. W-Dense's rule that every selectable backend meets its rows ([Z2-N-BUDGET-204]) cannot be met by one `game.json`, because a game links one ECS backend (Z2-D-081); the per-backend W-Dense runs therefore belong to the ECS research harness under `Zenith2/Benchmarks/`, which links `ZenithRuntime` and each backend with its own composition, recorded as a cross-area note for ECS and RESEARCH.

All workload games use `content.roots` `["Assets", "Scenes", "Schemas", "Scripts"]`, `content.startupScene` `Scenes/Main.scene.json` (the workload runner selects its start scene through `--set content.startupScene=…` from the workload definition), `android.applicationId` `com.zenith2.<name lower-cased>`, `versionCode` 1 and `orientation` `landscape`.

### Flagship slices

| Game | Owner | `platforms` | `modules` (canonical order) | `backends.ecs` | `backends.physics` |
|---|---|---|---|---|---|
| `OpenWorldSlice` | GAME | `android`, `windows` | `Animation`, `Audio`, `Cinematics`, `Environment`, `Gameplay`, `Online`, `Physics`, `AI`, `Streaming`, `UI`, `VFX` | `Flecs` | `{ "android": "Jolt", "windows": "PhysX" }` |
| `CinematicSlice` | CINE | `android`, `windows` | `Animation`, `Audio`, `Cinematics`, `Environment`, `Gameplay`, `Physics`, `UI`, `VFX` | `Custom` | `Jolt` |
| `Game2D` | TWOD | `android`, `windows` | `2D`, `Audio`, `Gameplay`, `Online`, `UI` | `Custom` | — |

- `OpenWorldSlice` keeps Architecture's example selections (Flecs; PhysX on Windows and Jolt on Android; AI, Animation, Audio, Gameplay, Physics, Streaming, UI) and adds `Environment` (terrain, weather), `Cinematics` (quest scenes), `Online` (cloud saves and achievements through the offline adapter from Stage 4) and `VFX`. Its Luau quest, dialogue and inventory modules are game-side (Z2-D-077) and add no engine module.
- The canonical order in this table follows § "Canonical module order": `Physics` precedes `AI`, `Animation` precedes `Cinematics`, and the rest are byte-ordered as they become ready.
- Every slice uses the same `content`, `android` conventions as the workload games, and carries `Workload/<W>.workload.json` and `Acceptance/` from the owning area's tickets.
- The table gives the full selection; the modules each slice lists before Stage 4 follow the rule stated under "Workload games" (only landed modules), and the full selection is the Stage 4 acceptance state.
